#include "Workstation.h"
#include "Spectrum.h"
#include <cmath>

namespace mixworks
{
namespace
{
class MasterButton final : public juce::Button
{
public:
    MasterButton() : Button("Open master output and spectrum") { setTooltip("Master output / Spectrum. Lane count stays beside this button."); }
    void paintButton(juce::Graphics& g, bool over, bool down) override
    {
        AlloyLookAndFeel::drawEmblem(g, getLocalBounds().toFloat().reduced(1));
        if (over || down || hasKeyboardFocus(true)) { g.setColour(colour::orange.withAlpha(down ? 1.f : .7f)); g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1), 5, 1.5f); }
    }
};
class SpectrumView final : public juce::Component
{
public:
    SpectrumAnalysis analysis;
    SpectrumView() { setComponentID("master-spectrum"); setTitle("Post-master stereo spectrum"); }
    void tick(SpectrumFeed& feed) { analysis.update(feed); repaint(); }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(colour::background); auto r = getLocalBounds().reduced(12); auto title = r.removeFromTop(24);
        g.setColour(colour::text); g.setFont(juce::FontOptions(14)); g.drawText("POST-MASTER / STEREO", title, juce::Justification::centredLeft);
        const auto plot = r.reduced(38, 24).toFloat();
        const double upper = analysis.rate() > 0 ? std::min(20000., analysis.rate() * .5) : 20000.;
        auto x = [&](double frequency) { return plot.getX() + (float) (std::log(frequency / 20) / std::log(upper / 20)) * plot.getWidth(); };
        auto y = [&](float db) { return plot.getBottom() - juce::jlimit(0.f, 1.f, (db + 100) / 106) * plot.getHeight(); };
        g.setFont(juce::FontOptions(10));
        for (int level : {0, -12, -24, -48, -72, -96})
        {
            g.setColour(colour::border.withAlpha(.5f)); g.drawHorizontalLine((int) y((float) level), plot.getX(), plot.getRight());
            g.setColour(colour::muted); g.drawText(juce::String(level), (int) plot.getX() - 34, (int) y((float) level) - 7, 28, 14, juce::Justification::centredRight);
        }
        for (double frequency : {20., 50., 100., 200., 500., 1000., 2000., 5000., 10000., 20000.}) if (frequency <= upper)
        {
            g.setColour(colour::border.withAlpha(.4f)); g.drawVerticalLine((int) x(frequency), plot.getY(), plot.getBottom());
            g.setColour(colour::muted); g.drawText(frequency >= 1000 ? juce::String(frequency / 1000, 0) + "k" : juce::String(frequency, 0), (int) x(frequency) - 18, (int) plot.getBottom() + 5, 36, 15, juce::Justification::centred);
        }
        if (analysis.ready())
        {
            juce::Path line; bool started = false;
            for (int bin = 1; bin < SpectrumAnalysis::bins; ++bin)
            {
                const double frequency = bin * analysis.rate() / SpectrumAnalysis::size; if (frequency < 20 || frequency > upper) continue;
                const float px = x(frequency), py = y(analysis.levels()[(size_t) bin]);
                if (!started) { line.startNewSubPath(px, py); started = true; } else line.lineTo(px, py);
            }
            g.setColour(colour::moltenOrange.withAlpha(.12f)); g.strokePath(line, juce::PathStrokeType(5));
            g.setColour(colour::orange); g.strokePath(line, juce::PathStrokeType(1.5f));
        }
        else { g.setColour(colour::muted); g.drawText("Waiting for a complete audio window", plot.toNearestInt(), juce::Justification::centred); }
        g.setColour(colour::muted); g.drawText("dBFS / bin", r.withHeight(18), juce::Justification::centredRight);
        g.drawText("Frequency / Hz", (int) plot.getX(), (int) plot.getBottom() + 19, (int) plot.getWidth(), 14, juce::Justification::centred);
    }
};
class MasterPanel final : public juce::Component, private juce::Timer
{
public:
    MasterPanel(Engine& e, std::function<bool()> allowed, std::function<void(bool)> before, std::function<void()> changed,
                std::function<void(bool)> mute, std::function<bool(const juce::KeyPress&)> key)
        : engine(e), canEdit(std::move(allowed)), beforeEdit(std::move(before)), edited(std::move(changed)), setMute(std::move(mute)), forwardKey(std::move(key))
    {
        addAndMakeVisible(spectrum);
        const char* names[] {"Master gain", "Low shelf / 120 Hz", "Mid / 1 kHz", "High shelf / 8 kHz", "Stereo width", "Saturation drive"};
        const char* ids[] {"master-gain", "master-low", "master-mid", "master-high", "master-width", "master-drive"};
        for (size_t i = 0; i < controls.size(); ++i)
        {
            auto& slider = controls[i]; labels[i].setText(names[i], juce::dontSendNotification); addAndMakeVisible(labels[i]); addAndMakeVisible(slider);
            slider.setComponentID(ids[i]); slider.setTitle(names[i]); slider.setSliderStyle(juce::Slider::LinearHorizontal); slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 76, 22);
            slider.setRange(i == 0 ? -60 : i == 4 || i == 5 ? 0 : -12, i == 0 || i == 5 ? 12 : i == 4 ? 200 : 12, .1);
            slider.setTextValueSuffix(i == 4 ? " %" : " dB"); slider.getProperties().set("alloyMolten", true);
            slider.onDragStart = [this, i] { if (canEdit()) { dragging[i] = true; beforeEdit(i == 0); } };
            slider.onDragEnd = [this, i] { dragging[i] = false; };
            slider.onValueChange = [this, i]
            {
                if (refreshing || !canEdit()) return;
                if (!dragging[i]) beforeEdit(i == 0);
                { const juce::ScopedLock guard(engine.lock); const auto value = (float) controls[i].getValue();
                  if (i == 0) engine.setControlValue({-1, AutomationTarget::master}, value);
                  else if (i <= 3) engine.project.mastering.eqDb[i - 1] = value;
                  else if (i == 4) engine.project.mastering.width = value / 100;
                  else engine.project.mastering.saturationDriveDb = value; }
                edited();
            };
        }
        for (auto* component : std::initializer_list<juce::Component*> {&saturation, &muteMonitor, &reset, &clearClip, &meters, &note}) addAndMakeVisible(component);
        saturation.setComponentID("master-saturation"); saturation.onClick = [this]
        { if (!canEdit()) return; beforeEdit(false); { const juce::ScopedLock guard(engine.lock); engine.project.mastering.saturation = saturation.getToggleState(); } edited(); refresh(); };
        saturation.setTooltip("Deliberate harmonic saturation with a bounded output. This changes tone; it is not loudness matching.");
        muteMonitor.setComponentID("master-monitor-mute"); muteMonitor.onClick = [this] { setMute(muteMonitor.getToggleState()); };
        muteMonitor.setTooltip("Silence the device. Spectrum, playback, meters and exported audio remain active.");
        reset.setComponentID("master-reset-tone"); reset.onClick = [this]
        { if (!canEdit()) return; beforeEdit(false); { const juce::ScopedLock guard(engine.lock); engine.project.mastering = {}; } edited(); refresh(); };
        clearClip.onClick = [this] { engine.masterClipped = false; refresh(); };
        meters.setJustificationType(juce::Justification::topLeft); meters.setComponentID("master-meters");
        note.setText("Spectrum follows processed audio before monitor mute. EQ, width and saturation are saved manual settings. Master gain retains its automation. dBFS is signal level, not perceived loudness.", juce::dontSendNotification);
        note.setColour(juce::Label::textColourId, colour::muted); note.setJustificationType(juce::Justification::topLeft);
        setSize(980, 580); refresh(); startTimerHz(20);
    }
    ~MasterPanel() override { stopTimer(); suspendMonitoring(); }
    void beginMonitoring()
    {
        if (monitoring) return;
        engine.spectrum.discardAvailable(); spectrum.analysis.clear(); monitoring = true; engine.spectrum.enabled = true;
    }
    void suspendMonitoring()
    {
        engine.spectrum.enabled = false;
        if (monitoring) { spectrum.analysis.clear(); engine.spectrum.discardAvailable(); monitoring = false; }
    }
    bool keyPressed(const juce::KeyPress& key) override { return key == juce::KeyPress::escapeKey ? false : forwardKey(key); }
    void paint(juce::Graphics& g) override { g.fillAll(colour::panel); }
    void resized() override
    {
        auto bounds = getLocalBounds().reduced(14); auto right = bounds.removeFromRight(std::min(330, bounds.getWidth() / 2)); bounds.removeFromRight(12); spectrum.setBounds(bounds);
        meters.setBounds(right.removeFromTop(55)); auto row = right.removeFromTop(25); muteMonitor.setBounds(row.removeFromLeft(160)); clearClip.setBounds(row);
        for (size_t i = 0; i < controls.size(); ++i)
        {
            if (i == 5) { right.removeFromTop(3); saturation.setBounds(right.removeFromTop(24)); }
            labels[i].setBounds(right.removeFromTop(18)); controls[i].setBounds(right.removeFromTop(27)); right.removeFromTop(5);
        }
        reset.setBounds(right.removeFromTop(27)); right.removeFromTop(8); note.setBounds(right);
    }
private:
    Engine& engine;
    std::function<bool()> canEdit;
    std::function<void(bool)> beforeEdit;
    std::function<void()> edited;
    std::function<void(bool)> setMute;
    std::function<bool(const juce::KeyPress&)> forwardKey;
    SpectrumView spectrum;
    std::array<juce::Slider, 6> controls;
    std::array<juce::Label, 6> labels;
    std::array<bool, 6> dragging {};
    juce::ToggleButton saturation {"Soft saturation"}, muteMonitor {"Mute monitor"};
    juce::TextButton reset {"Reset tone / width"}, clearClip {"Reset clip"};
    juce::Label meters, note;
    bool refreshing = false, monitoring = false;
    void timerCallback() override
    {
        if (!isShowing() || engine.exporting) { suspendMonitoring(); return; }
        beginMonitoring(); spectrum.tick(engine.spectrum); refresh();
    }
    void refresh()
    {
        const juce::ScopedValueSetter<bool> guard(refreshing, true); MasterSettings settings; float gain;
        { const juce::ScopedLock lock(engine.lock); settings = engine.project.mastering; gain = engine.project.masterDb; }
        const double values[] {gain, settings.eqDb[0], settings.eqDb[1], settings.eqDb[2], settings.width * 100., settings.saturationDriveDb};
        for (size_t i = 0; i < controls.size(); ++i) if (!dragging[i] && !AlloyLookAndFeel::editingSliderText(controls[i])) controls[i].setValue(values[i], juce::dontSendNotification);
        saturation.setToggleState(settings.saturation, juce::dontSendNotification); controls[5].setEnabled(settings.saturation && canEdit());
        muteMonitor.setToggleState(engine.monitorMuted, juce::dontSendNotification);
        auto level = [](float peak) { return peak < 0 ? juce::String("unavailable") : peak > 0 ? juce::String(juce::Decibels::gainToDecibels(peak), 1) + " dBFS" : juce::String("silence"); };
        meters.setText("IN  " + level(engine.masterInputPeak) + "\nOUT  " + level(engine.masterPeak) + (engine.masterClipped ? "   CLIP" : ""), juce::dontSendNotification);
        meters.setColour(juce::Label::textColourId, engine.masterClipped ? juce::Colours::orangered : colour::text);
    }
};
class MasterWindow final : public juce::DocumentWindow
{
public:
    MasterWindow(juce::Component& owner, std::unique_ptr<juce::Component> content) : DocumentWindow("Alloy / Master output and spectrum", colour::background, allButtons)
    { setUsingNativeTitleBar(true); setContentOwned(content.release(), true); setResizable(true, false); setResizeLimits(760, 580, 3840, 2160); centreAroundComponent(&owner, 980, 640); }
    void closeButtonPressed() override { if (auto* panel = dynamic_cast<MasterPanel*>(getContentComponent())) panel->suspendMonitoring(); setVisible(false); }
    bool keyPressed(const juce::KeyPress& key) override { if (key == juce::KeyPress::escapeKey) { closeButtonPressed(); return true; } return DocumentWindow::keyPressed(key); }
};
}
void Workstation::initialiseMasteringUI()
{
    masterPanelButton = std::make_unique<MasterButton>(); addAndMakeVisible(*masterPanelButton);
    masterPanelButton->onClick = [this] { showMastering(); };
}
void Workstation::showMastering()
{
    if (busy) return;
    if (!masterWindow)
    {
        auto safe = juce::Component::SafePointer<Workstation>(this);
        auto panel = std::make_unique<MasterPanel>(engine,
            [safe] { return safe && !safe->busy && !safe->engine.exporting; },
            [safe](bool gain) { if (safe && (!gain || !safe->engine.canWriteAutomation(-1))) safe->remember(); },
            [safe] { if (safe) { safe->dirty = true; safe->refresh(); } },
            [safe](bool muted) { if (safe) { safe->monitorMute.setToggleState(muted, juce::dontSendNotification); safe->monitorMute.onClick(); } },
            [safe](const juce::KeyPress& key) { return safe && safe->keyPressed(key); });
        masterWindow = std::make_unique<MasterWindow>(*this, std::move(panel)); masterWindow->setLookAndFeel(&look);
    }
    masterWindow->setVisible(true);
    if (auto* panel = dynamic_cast<MasterPanel*>(masterWindow->getContentComponent())) panel->beginMonitoring();
    masterWindow->toFront(true);
}
}
