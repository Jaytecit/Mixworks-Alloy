#include "Workstation.h"
#include "StudioWorkspace.h"
#include "UiMessages.h"
#include "RetryReview.h"
#include "DownbeatReferences.h"
#include "EnergySections.h"
#include "GridCorrectionUI.h"
#include "AppWindow.h"
#include "Beatmatch.h"
#include "AutoArrangement.h"
#include <juce_cryptography/juce_cryptography.h>
#include <cmath>
#include <limits>
#include <set>
#include <cstdlib>

namespace mixworks
{
namespace
{
Project editHistorySnapshot(Engine& engine)
{
    const juce::ScopedLock guard(engine.lock);
    // Take chronology can be large. Never duplicate it for live structural Undo.
    std::vector<PerformanceTake> takes; takes.swap(engine.project.performanceTakes);
    struct Restore { Project& project; std::vector<PerformanceTake>& takes; ~Restore(){project.performanceTakes.swap(takes);} } restore{engine.project,takes};
    return engine.project;
}
int gridReviewRank(const Media& media)
{
    const auto grid=media.grid();
    if (!std::isfinite(grid.bpm) || grid.bpm<=0) return 0;
    if (media.manualBpm<=0 && (media.analysis.variableTempo || !std::isfinite(media.analysis.tempoConfidence) || media.analysis.tempoConfidence<.2)) return 1;
    if (!grid.hasPulse() || (media.manualBeatOrigin<0 && media.analysis.signalGridNeedsReview)) return 2;
    if (media.manualBeatOrigin<0 && (media.analysis.downbeatOrigin<0 || !std::isfinite(media.analysis.downbeatConfidence) || media.analysis.downbeatConfidence<.2)) return 3;
    return media.manualBeatOrigin>=0 ? (media.manualBpm>0 ? 6 : 5) : 4;
}
juce::String gridReviewText(const Media& media)
{
    static const char* labels[]={"Unknown BPM","Review BPM","Review pulse","Review bar","Detected","Manual bar","Manual grid"};
    return labels[gridReviewRank(media)];
}
juce::String gridReviewHint(const Media& media)
{
    return "Grid check: "+gridReviewText(media)+". BPM sets speed; pulse sets beat timing; bar sets beat one. "
        +juce::String(media.manualBpm>0 ? "BPM manually set. " : "BPM is automatic. ")
        +(media.manualBeatOrigin>=0 ? "Beat one manually set. " : "Beat one is automatic or unknown. ")
        +"Detected is not human confirmation. Review beginning, middle and end in Source corrections.";
}
void label(juce::Label& l, const juce::String& text, float size = 12)
{
    l.setText(text, juce::dontSendNotification); l.setFont(juce::Font(juce::FontOptions(size))); l.setColour(juce::Label::textColourId, colour::muted);
}
void slider(juce::Slider& s, double low, double high, double step, const juce::String& suffix = {})
{
    s.setRange(low, high, step); s.setSliderStyle(juce::Slider::LinearHorizontal); s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 22); s.setTextValueSuffix(suffix);
}
juce::String trackLabel(int i) { return juce::String(i + 1).paddedLeft('0', 2); }
class LibrarySeek final : public juce::Slider
{
public:
    LibrarySeek()
    {
        setSliderStyle(juce::Slider::LinearBar); setTextBoxStyle(juce::Slider::NoTextBox,false,0,0); setRange(0,1,0);
        setWantsKeyboardFocus(true); setSliderSnapsToMousePosition(true);
        onValueChange=[this] {if(!cancelled)if(const auto action=audition)if(const auto target=gesture ? gesture:media)action(target,getValue());};
    }
    void source(std::shared_ptr<Media> next)
    {
        if(media && next && media->id==next->id) {media=std::move(next);return;}
        if(gesture) {cancelled=true;gesture.reset();}
        media=std::move(next);setValue(0,juce::dontSendNotification);
    }
    void updatePosition()
    {if(!gesture && progress)if(const auto fraction=progress();fraction>=0)setValue(fraction,juce::dontSendNotification);repaint();}
    juce::String getTextFromValue(double fraction) override
    {const double seconds=length ? length():0;return seconds>0 ? timeText(seconds*fraction)+" of "+timeText(seconds) : juce::String(fraction*100,0)+"% of track";}
    void mouseDown(const juce::MouseEvent& e) override
    {
        if(e.mods.isPopupMenu()) {if(context)context(e);return;}
        cancelled=false;gesture=media;grabKeyboardFocus();juce::Slider::mouseDown(e);
    }
    void mouseUp(const juce::MouseEvent& e) override {juce::Slider::mouseUp(e);gesture.reset();cancelled=false;}
    void focusLost(FocusChangeType cause) override
    {if(gesture) {gesture.reset();cancelled=true;}juce::Slider::focusLost(cause);}
    void mouseDoubleClick(const juce::MouseEvent&) override {} // Seeking cannot double-click a song onto the timeline.
    bool keyPressed(const juce::KeyPress& key) override
    {
        cancelled=false;gesture.reset();if(!media)return false;
        if(key.getKeyCode()==juce::KeyPress::escapeKey)
        {if(progress && progress()>=0 && audition)audition(media,{});return true;}
        if(key.getKeyCode()==juce::KeyPress::spaceKey || key.getKeyCode()==juce::KeyPress::returnKey)
        {if(audition)audition(media,{});return true;}
        const double duration=length ? length():0,step=duration>0 ? (key.getModifiers().isShiftDown()?30.:5.)/duration : .05;
        double value=progress && progress()>=0 ? progress():getValue();
        if(key.getKeyCode()==juce::KeyPress::leftKey)value-=step;
        else if(key.getKeyCode()==juce::KeyPress::rightKey)value+=step;
        else if(key.getKeyCode()==juce::KeyPress::homeKey)value=0;
        else if(key.getKeyCode()==juce::KeyPress::endKey)value=1;
        else return juce::Slider::keyPressed(key);
        value=juce::jlimit(0.,1.,value);
        if(value==getValue()) {if(audition)audition(media,value);} else setValue(value,juce::sendNotificationSync);
        return true;
    }
    void paint(juce::Graphics& g) override
    {
        const auto area=getLocalBounds().toFloat().reduced(1);g.setColour(colour::background);g.fillRoundedRectangle(area,2);
        g.setColour(colour::muted.withAlpha(.65f));
        if(media && !media->peaks.empty())
        {
            float maximum=.001f;for(float peak:media->peaks)maximum=std::max(maximum,peak);
            for(int x=1;x<getWidth()-1;++x)
            {const auto at=std::min(media->peaks.size()-1,(size_t)((double)(x-1)*media->peaks.size()/std::max(1,getWidth()-2)));
                const float height=std::max(1.f,media->peaks[at]/maximum*(area.getHeight()-2));g.drawVerticalLine(x,area.getCentreY()-height*.5f,area.getCentreY()+height*.5f);}
        }
        else g.drawHorizontalLine((int)area.getCentreY(),area.getX(),area.getRight());
        const double fraction=gesture ? getValue():progress && progress()>=0 ? progress():getValue();
        const float x=area.getX()+(float)fraction*area.getWidth();g.setColour(colour::orange);g.drawVerticalLine((int)x,area.getY(),area.getBottom());
        if(hasKeyboardFocus(true)) {g.setColour(colour::focus);g.drawRoundedRectangle(area,2,1);}
    }
    std::shared_ptr<Media> media,gesture;
    std::function<void(const std::shared_ptr<Media>&,std::optional<double>)> audition;
    std::function<double()> progress,length;
    std::function<void(const juce::MouseEvent&)> context;
private:
    bool cancelled=false;
};
class LibraryCell final : public juce::Component, public juce::TooltipClient
{
public:
    explicit LibraryCell(LibraryTable& t) : model(t)
    { setInterceptsMouseClicks(false,true);addChildComponent(seek);seek.context=[this](const juce::MouseEvent& e) {model.cellClicked(row,column,e);}; }
    void resized() override {seek.setBounds(4,25,std::max(0,getWidth()-8),std::max(0,getHeight()-28));}
    void paint(juce::Graphics& g) override { model.paintCell(g, row, column, getWidth(), getHeight(), selected); }
    juce::String getTooltip() override { return model.getCellTooltip(row, column); }
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        class CellInterface final : public juce::AccessibilityCellInterface
        {
        public:
            explicit CellInterface(LibraryCell& c) : cell(c) {}
            int getDisclosureLevel() const override { return 0; }
            const juce::AccessibilityHandler* getTableHandler() const override { return cell.model.table.getAccessibilityHandler(); }
        private:
            LibraryCell& cell;
        };
        class Handler final : public juce::AccessibilityHandler
        {
        public:
            explicit Handler(LibraryCell& c) : AccessibilityHandler(c, juce::AccessibilityRole::cell, {}, {std::make_unique<CellInterface>(c)}), cell(c) {}
            juce::String getHelp() const override { return cell.getTooltip(); }
        private:
            LibraryCell& cell;
        };
        return std::make_unique<Handler>(*this);
    }
    LibraryTable& model;
    int row = 0, column = 0;
    bool selected = false;
    LibrarySeek seek;
};
class MidiSettings final : public juce::Component, private juce::Timer
{
public:
    explicit MidiSettings(Engine& e) : engine(e), audio(e.devices, 0, 0, 0, 2, true, false, true, false)
    {
        addAndMakeVisible(audio); addAndMakeVisible(help); addAndMakeVisible(action); addAndMakeVisible(learn); addAndMakeVisible(cancel); addAndMakeVisible(mappings); addAndMakeVisible(quickSetup); addAndMakeVisible(reload); addAndMakeVisible(clear);
        for (auto* child : std::initializer_list<juce::Component*> { &profiles, &profileName, &openProfile, &saveProfile, &saveAsProfile, &nameProfile, &importProfile, &exportProfile, &deleteProfile }) addAndMakeVisible(child);
        audio.getProperties().set("mixworksHint", "Choose your audio output and MIDI inputs. Smaller buffers respond faster but need more processing power.");
        label(help, "FLkey 49: enable its MIDI input above; use Custom Mode for faders / knobs. Keys play the selected lane's instrument.");
        help.setJustificationType(juce::Justification::topLeft);
        addAndMakeVisible(audioHealth); label(audioHealth, ""); audioHealth.setComponentID("audio-callback-health");
        audioHealth.setTooltip("Audio performance since the app opened. High counts can help diagnose dropouts; they are separate from driver reports.");
        addAndMakeVisible(audioOptions); addAndMakeVisible(audioChoices);
        audioOptions.setButtonText("Buffer / rate options..."); audioOptions.setComponentID("audio-buffer-rate-options");
        audioOptions.setTooltip("Choose a driver mode, then select its supported sample rates and buffer sizes above. Changing mode briefly interrupts audio.");
        audioOptions.onClick = [this] { showAudioOptions(); };
        label(audioChoices, ""); audioChoices.setComponentID("audio-format-choices");
        timerCallback(); startTimerHz(1);
        int id = 1;
        for (auto a : midiActions()) { auto name = midiActionId(a); actions.add(name); action.addItem(name, id++); }
        action.setSelectedId(1); learn.setButtonText("Learn"); cancel.setButtonText("Cancel learn");
        action.setComponentID("midi-learn-action"); learn.setComponentID("learn-midi-control"); cancel.setComponentID("cancel-midi-learn");
        learn.setTooltip("Choose an action, click Learn, then move a controller control. Only that assignment is replaced.");
        learn.onClick = [this] { engine.beginLearn(actions[action.getSelectedId() - 1]); lastOperation.clear(); updateText(); };
        cancel.onClick = [this] { engine.beginLearn({}); lastOperation.clear(); updateText(); };
        quickSetup.setButtonText("FLkey guided setup"); quickSetup.setComponentID("guided-midi-setup");
        quickSetup.setTooltip("Follow the steps to connect your controller. Existing assignments are kept and skipped.");
        quickSetup.onClick = [this] { engine.beginControllerSetup(); lastOperation.clear(); updateText(); };
        reload.setButtonText("Reload saved mappings"); reload.setComponentID("reload-midi-mappings");
        reload.setTooltip("Reload the saved controller assignments. Use Open profile to choose another saved setup.");
        reload.onClick = [this] { const auto result = engine.reloadMidiMappings(); lastOperation = result.wasOk() ? "Reload complete." : "Reload failed: " + result.getErrorMessage(); updateText(); };
        clear.setButtonText("Clear mappings"); clear.setComponentID("clear-midi-mappings");
        clear.setTooltip("Back up and clear the current controller assignments. Saved profiles and music are kept.");
        clear.onClick = [this] { const auto result = engine.clearMidiMappings(); lastOperation = result.wasOk() ? "Cleared active and startup mappings. Named copies kept." : "Clear failed: " + result.getErrorMessage(); updateText(); };
        action.setTooltip("Choose the app action to map before pressing Learn. This choice alone does not change an assignment.");
        cancel.setTooltip("Cancel the current MIDI Learn or guided setup without changing completed assignments.");
        mappings.setTooltip("Active assignments, saved profile identity, last MIDI input and last profile read. Reload replaces active assignments from disk.");
        profiles.setComponentID("midi-profiles"); profiles.setTextWhenNothingSelected("Choose a saved profile");
        profiles.setTooltip("Choose a saved controller setup, then click Open. Finished Learn steps are saved automatically.");
        profileName.setComponentID("midi-profile-name"); profileName.setTextToShowWhenEmpty("Profile name", colour::muted);
        profileName.setTooltip("Type a name, then Name active to apply it or Save as to create a distinct named profile. Typing alone changes nothing.");
        auto button = [](juce::TextButton& b, const char* caption, const char* id, const char* hint) { b.setButtonText(caption); b.setComponentID(id); b.setTooltip(hint); };
        button(openProfile, "Open profile", "open-midi-profile", "Use the chosen saved controller setup. This replaces current assignments and stops Learn.");
        button(saveProfile, "Save profile", "save-midi-profile", "Replace the active named profile with the current assignments and name, keeping a backup. Use Save as first when no named profile is active.");
        button(saveAsProfile, "Save as", "save-as-midi-profile", "Create a distinct named profile using the name above and make it active. An existing name is never overwritten.");
        button(nameProfile, "Name active", "name-midi-profile", "Apply the typed name to the active assignments and startup snapshot. Save profile updates a named copy separately.");
        button(importProfile, "Import JSON", "import-midi-profile", "Validate a JSON profile and copy its assignments into the startup snapshot. The original stays unchanged. Use Save as to keep a named copy.");
        button(exportProfile, "Export JSON", "export-midi-profile", "Write the active assignments and name to a separate JSON file without changing the active profile. Existing destinations are backed up.");
        button(deleteProfile, "Delete active", "delete-midi-profile", "Archive the active named profile beside its original file. Keep active assignments, name and startup snapshot. Save as creates a new named copy; Clear removes active mappings separately.");
        openProfile.onClick = [this] { const int i = profiles.getSelectedId() - 1; if (i >= 0 && i < profileFiles.size()) finish(engine.openMidiProfile(profileFiles[i]), "Profile opened."); };
        saveProfile.onClick = [this] { finish(engine.saveMidiProfile(), "Named profile saved."); };
        saveAsProfile.onClick = [this] { finish(engine.saveMidiProfile(profileName.getText(), true), "Named profile created."); };
        nameProfile.onClick = [this] { finish(engine.renameMidiMapping(profileName.getText()), "Active mapping named. Save profile updates its named copy."); };
        deleteProfile.onClick = [this] { finish(engine.deleteMidiProfile(), "Named profile archived. Active assignments retained."); };
        importProfile.onClick = [this] { chooseProfile(false); }; exportProfile.onClick = [this] { chooseProfile(true); };
        mappings.setComponentID("midi-profile-status"); mappings.setMultiLine(true); mappings.setReadOnly(true); updateText(); setSize(800, 784);
    }
    void finish(const juce::Result& result, const juce::String& success)
    { lastOperation = result.wasOk() ? success : "Profile operation failed: " + result.getErrorMessage(); if (result.wasOk()) profiles.setSelectedId(0, juce::dontSendNotification); updateText(); }
    void chooseProfile(bool exporting)
    {
        chooser = std::make_unique<juce::FileChooser>(exporting ? "Export MIDI profile" : "Import MIDI profile",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("MIDI profile.json"), "*.json", false);
        chooser->launchAsync(juce::FileBrowserComponent::canSelectFiles | (exporting
            ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting : juce::FileBrowserComponent::openMode),
            [safe = juce::Component::SafePointer<MidiSettings>(this), exporting](const auto& selected)
            {
                if (!safe) return;
                const auto file = selected.getResult();
                if (file == juce::File()) { safe->lastOperation = "Profile file selection cancelled."; safe->updateText(); return; }
                safe->finish(exporting ? safe->engine.exportMidiProfile(file) : safe->engine.importMidiProfile(file),
                    exporting ? "Profile exported. Active identity retained." : "Profile imported. Use Save as for a named copy.");
            });
    }
    void updateText()
    {
        mappings.setText((lastOperation.isEmpty() ? juce::String() : lastOperation + "\n\n") + engine.mappingSummary());
        const auto name = engine.midiMappingName();
        if (name != displayedName) { profileName.setText(name, false); displayedName = name; }
        const auto selected = engine.selectedMidiProfile(); const auto chosen = profiles.getSelectedId() > 0 && profiles.getSelectedId() <= profileFiles.size() ? profileFiles[profiles.getSelectedId() - 1] : juce::File();
        profileFiles = engine.midiProfileFiles(); profiles.clear(juce::dontSendNotification);
        for (int i = 0; i < profileFiles.size(); ++i)
        {
            auto title = juce::JSON::parse(profileFiles[i].loadFileAsString())["title"].toString();
            profiles.addItem(title.isEmpty() ? profileFiles[i].getFileNameWithoutExtension() : title, i + 1);
            if (profileFiles[i] == (chosen == juce::File() ? selected : chosen)) profiles.setSelectedId(i + 1, juce::dontSendNotification);
        }
        saveProfile.setEnabled(selected != juce::File()); deleteProfile.setEnabled(selected != juce::File());
    }
    void resized() override
    {
        auto area = getLocalBounds().reduced(18); audio.setBounds(area.removeFromTop(310));
        auto options = area.removeFromTop(50); audioOptions.setBounds(options.removeFromRight(190).reduced(2, 9)); audioChoices.setBounds(options);
        audioHealth.setBounds(area.removeFromTop(44)); help.setBounds(area.removeFromTop(38));
        auto row = area.removeFromTop(32); action.setBounds(row.removeFromLeft(370).reduced(2)); learn.setBounds(row.removeFromLeft(100).reduced(2)); cancel.setBounds(row.reduced(2));
        auto setup = area.removeFromTop(32); const int third = setup.getWidth() / 3;
        quickSetup.setBounds(setup.removeFromLeft(third).reduced(2)); reload.setBounds(setup.removeFromLeft(third).reduced(2)); clear.setBounds(setup.reduced(2));
        auto saved = area.removeFromTop(32); profiles.setBounds(saved.removeFromLeft(330).reduced(2)); openProfile.setBounds(saved.removeFromLeft(140).reduced(2));
        saveProfile.setBounds(saved.removeFromLeft(140).reduced(2)); deleteProfile.setBounds(saved.reduced(2));
        auto naming = area.removeFromTop(32); nameProfile.setBounds(naming.removeFromRight(160).reduced(2)); profileName.setBounds(naming.reduced(2));
        auto files = area.removeFromTop(32); const int part = files.getWidth() / 3;
        saveAsProfile.setBounds(files.removeFromLeft(part).reduced(2)); importProfile.setBounds(files.removeFromLeft(part).reduced(2)); exportProfile.setBounds(files.reduced(2));
        mappings.setBounds(area.reduced(2));
    }
    Engine& engine;
private:
    void showAudioOptions()
    {
        juce::PopupMenu menu; juce::StringArray types;
        for (auto* type : engine.devices.getAvailableDeviceTypes())
        {
            const auto name = type->getTypeName(); types.add(name);
            auto description = name;
            if (name == "Windows Audio") description += " / system sample rate and buffer";
            else if (name.contains("Low Latency")) description += " / adjustable shared buffers";
            else if (name.contains("Exclusive")) description += " / supported rates and buffers; takes exclusive output";
            else if (name == "ASIO") description += " / choose a driver, then its Control Panel";
            menu.addItem(types.size(), description, !engine.exporting,
                         name == engine.devices.getCurrentAudioDeviceType());
        }
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(audioOptions),
            [safe = juce::Component::SafePointer<MidiSettings>(this), types](int choice)
            {
                if (!safe || choice <= 0 || choice > types.size() || safe->engine.exporting) return;
                safe->engine.devices.setCurrentAudioDeviceType(types[choice - 1], true);
                safe->timerCallback();
            });
    }
    void timerCallback() override
    {
        const auto d = engine.callbackDiagnostics.snapshot();
        auto* device = engine.devices.getCurrentAudioDevice();
        const int xruns = device ? device->getXRunCount() : -1;
        const auto mode = engine.devices.getCurrentAudioDeviceType();
        juce::String choices = !device ? "No open audio output. Choose a device above." :
            juce::String(device->getAvailableSampleRates().size()) + " supported sample rate(s), "
            + juce::String(device->getAvailableBufferSizes().size()) + " buffer size(s).";
        if (mode == "Windows Audio") choices += "\nShared mode uses the Windows format. More options: choose another mode.";
        else if (mode == "ASIO") choices += "\nSelect the ASIO driver above; its Control Panel may offer more choices.";
        else choices += "\nChoose supported values above. Larger buffers give playback more headroom.";
        audioChoices.setText(choices, juce::dontSendNotification); audioChoices.setTooltip(choices);
        audioOptions.setEnabled(!engine.exporting);
        audioHealth.setText("Audio callbacks: " + juce::String((juce::int64) d.callbacks) + " / over budget: " + juce::String((juce::int64) d.overBudget)
            + " / worst: " + juce::String(1000.0 * (double) d.peakTicks / (double) juce::Time::getHighResolutionTicksPerSecond(), 2) + " ms\n"
            + "Lock skips: " + juce::String((juce::int64) d.lockMisses) + " / size skips: " + juce::String((juce::int64) d.oversized)
            + " / export silence: " + juce::String((juce::int64) d.exportSilence) + " / driver xruns: "
            + (xruns < 0 ? juce::String("unavailable") : juce::String(xruns)), juce::dontSendNotification);
    }
    juce::AudioDeviceSelectorComponent audio;
    juce::Label help, audioHealth;
    juce::Label audioChoices;
    juce::TextButton audioOptions;
    juce::ComboBox action;
    juce::StringArray actions;
    juce::TextButton learn, cancel, quickSetup, reload, clear;
    juce::ComboBox profiles; juce::TextEditor profileName;
    juce::TextButton openProfile, saveProfile, saveAsProfile, nameProfile, importProfile, exportProfile, deleteProfile;
    juce::Array<juce::File> profileFiles;
    juce::String displayedName;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String lastOperation;
    juce::TextEditor mappings;
};
void findPlugins(const juce::File& folder, juce::Array<juce::File>& out)
{
    for (const auto& entry : folder.findChildFiles(juce::File::findFilesAndDirectories, false))
        if (entry.hasFileExtension("vst3")) out.add(entry);
        else if (entry.isDirectory() && !entry.isSymbolicLink()) findPlugins(entry, out);
}
}
// Focused UI fixture: enumerates and changes formats without opening hardware.
void runAudioSettingsUiTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    class Device final : public juce::AudioIODevice
    {
    public:
        Device() : AudioIODevice("Fixture output", "Fixture audio") {}
        juce::StringArray getOutputChannelNames() override { return {"Left", "Right"}; }
        juce::StringArray getInputChannelNames() override { return {}; }
        juce::Array<double> getAvailableSampleRates() override { return {44100, 48000, 96000}; }
        juce::Array<int> getAvailableBufferSizes() override { return {256, 512, 1024}; }
        int getDefaultBufferSize() override { return 512; }
        juce::String open(const juce::BigInteger&, const juce::BigInteger&, double r, int b) override { rate = r; buffer = b; opened = true; return {}; }
        void close() override { opened = false; }
        bool isOpen() override { return opened; }
        void start(juce::AudioIODeviceCallback* c) override { callback = c; if (c) c->audioDeviceAboutToStart(this); }
        void stop() override { if (callback) callback->audioDeviceStopped(); callback = nullptr; }
        bool isPlaying() override { return callback != nullptr; }
        juce::String getLastError() override { return {}; }
        int getCurrentBufferSizeSamples() override { return buffer; }
        double getCurrentSampleRate() override { return rate; }
        int getCurrentBitDepth() override { return 32; }
        juce::BigInteger getActiveOutputChannels() const override { return juce::BigInteger(3); }
        juce::BigInteger getActiveInputChannels() const override { return {}; }
        int getOutputLatencyInSamples() override { return buffer; }
        int getInputLatencyInSamples() override { return 0; }
        bool opened = false; double rate = 48000; int buffer = 512; juce::AudioIODeviceCallback* callback = nullptr;
    };
    class Type final : public juce::AudioIODeviceType
    {
    public:
        Type() : AudioIODeviceType("Fixture audio") {}
        void scanForDevices() override {}
        juce::StringArray getDeviceNames(bool = false) const override { return {"Fixture output"}; }
        int getDefaultDeviceIndex(bool) const override { return 0; }
        int getIndexOfDevice(juce::AudioIODevice*, bool) const override { return 0; }
        bool hasSeparateInputsAndOutputs() const override { return false; }
        juce::AudioIODevice* createDevice(const juce::String&, const juce::String&) override { return new Device; }
    };
    auto engine = std::make_unique<Engine>(juce::File(), false);
    engine->devices.addAudioDeviceType(std::make_unique<Type>());
    check(engine->devices.initialise(0, 2, nullptr, false, "Fixture output").isEmpty(), "format selector opens an isolated fixture driver");
    AlloyLookAndFeel look; MidiSettings settings(*engine); settings.setLookAndFeel(&look);
    juce::ComboBox* rates = nullptr; juce::ComboBox* buffers = nullptr;
    std::function<void(juce::Component&)> find = [&](juce::Component& c)
    {
        if (auto* box = dynamic_cast<juce::ComboBox*>(&c))
            for (int i = 0; i < box->getNumItems(); ++i)
            {
                if (box->getItemText(i).contains("96000")) rates = box;
                if (box->getItemText(i).contains("1024")) buffers = box;
            }
        for (auto* child : c.getChildren()) find(*child);
    };
    find(settings);
    check(rates && buffers && rates->getNumItems() == 3 && buffers->getNumItems() == 3, "format menus expose every supported sample rate and buffer size");
    if (rates && buffers)
    {
        rates->setSelectedItemIndex(2, juce::sendNotificationSync);
        buffers->setSelectedItemIndex(2, juce::sendNotificationSync);
        const auto setup = engine->devices.getAudioDeviceSetup();
        check(setup.sampleRate == 96000 && setup.bufferSize == 1024, "choosing a different rate and buffer applies the requested device setup");
    }
    auto* options = settings.findChildWithID("audio-buffer-rate-options");
    auto* choices = dynamic_cast<juce::Label*>(settings.findChildWithID("audio-format-choices"));
    check(options && choices && choices->getText().contains("3 supported sample rate(s), 3 buffer size(s)"), "audio settings expose supported choice counts and a driver-mode shortcut");
    auto capture = report.getSiblingFile("audio-settings.png").createOutputStream();
    check(capture && juce::PNGImageFormat().writeImageToStream(settings.createComponentSnapshot(settings.getLocalBounds()), *capture), "capture the complete audio settings layout");
    settings.setLookAndFeel(nullptr);
}
ImportProgressView::ImportProgressView()
{
    for (auto* child : std::initializer_list<juce::Component*> { &heading, &file, &stage, &detail, &latestResult, &bar, &cancel }) addAndMakeVisible(child);
    label(heading, "Importing audio", 20); heading.setColour(juce::Label::textColourId, colour::text);
    label(file, {}, 14); file.setColour(juce::Label::textColourId, colour::text);
    label(stage, {}, 14); label(detail, {}, 13);
    label(latestResult, {}, 12); latestResult.setName("Last analysed track");
    bar.setColour(juce::ProgressBar::foregroundColourId, colour::orange);
    cancel.setTooltip("Request cancellation. The current file or operation may finish before stopping; completed imports are kept.");
    bar.setTextToDisplay({}); setAlwaysOnTop(true);
}
void ImportProgressView::paint(juce::Graphics& g)
{
    g.setColour(colour::raised); g.fillRoundedRectangle(getLocalBounds().toFloat(), 9);
    g.setColour(colour::orange); g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1), 9, 2);
}
void ImportProgressView::resized()
{
    auto area = getLocalBounds().reduced(22);
    heading.setBounds(area.removeFromTop(30)); detail.setBounds(area.removeFromTop(26));
    file.setBounds(area.removeFromTop(46)); stage.setBounds(area.removeFromTop(26));
    area.removeFromTop(8); bar.setBounds(area.removeFromTop(20)); area.removeFromTop(12);
    cancel.setBounds(area.removeFromTop(30).removeFromRight(140));
    latestResult.setBounds(getLocalBounds().reduced(22).removeFromBottom(35).withTrimmedRight(150));
}
void ImportProgressView::begin(int totalFiles)
{
    getProperties().remove("focusedLastResult");
    latestResult.setText({}, juce::dontSendNotification);
    heading.setText("Importing audio", juce::dontSendNotification); stopMessage = "Stopping after this track. Finished tracks will stay in your library.";
    total = totalFiles; completed = 0; fraction = 0; stopping = false; exporting = false; startedAt = juce::Time::getMillisecondCounterHiRes();
    file.setText("Starting import...", juce::dontSendNotification); stage.setText({}, juce::dontSendNotification);
    cancel.setButtonText("Stop import"); cancel.setEnabled(true); setEnabled(true); setVisible(true); toFront(false); tick();
}
void ImportProgressView::showFile(const juce::String& name, MediaReadStage phase)
{
    file.setText(name, juce::dontSendNotification);
    if (stopping) return;
    const char* description = "";
    switch (phase)
    {
        case MediaReadStage::decode: description = "Decoding audio"; break;
        case MediaReadStage::waveform: description = "Building waveform"; break;
        case MediaReadStage::analyse: description = "Detecting beats and musical key"; break;
        case MediaReadStage::cache: description = "Preparing playback audio"; break;
        case MediaReadStage::lookupAnalysis: description = "Checking saved track analysis"; break;
        case MediaReadStage::savedAnalysis: description = "Using saved beat, key and waveform analysis"; break;
        case MediaReadStage::playlist: description = "Reading local playlist paths"; break;
        case MediaReadStage::savedAudio: description = "Reusing decoded playback audio"; break;
        case MediaReadStage::tempo: description = "Reusing or preparing tempo audio"; break;
        case MediaReadStage::energy: description = "Analysing rhythm, bass and dynamics"; break;
    }
    stage.setText(description, juce::dontSendNotification);
}
void ImportProgressView::finishFile() { ++completed; fraction = total > 0 ? (double) completed / total : 0; tick(); }
void ImportProgressView::setCounts(int done, int count) { completed = done; total = count; fraction = total > 0 ? (double) completed / total : 0; tick(); }
void ImportProgressView::tick()
{
    const auto elapsed = (int) ((juce::Time::getMillisecondCounterHiRes() - startedAt) / 1000);
    const auto progress = exporting ? juce::String(renderedFrames / exportPlan.sampleRate, 1) + " / " + juce::String(exportPlan.seconds(), 1) + " seconds rendered"
                                    : juce::String(completed) + " of " + juce::String(total) + " files processed";
    detail.setText(progress + "  |  " + juce::String(elapsed / 60) + ":" + juce::String(elapsed % 60).paddedLeft('0', 2) + " elapsed", juce::dontSendNotification);
}
void ImportProgressView::setExporting(const juce::File& destination, const WavExportPlan& plan)
{
    begin(0); exporting = true; exportPlan = plan; renderedFrames = 0;
    heading.setText("Exporting mix", juce::dontSendNotification); file.setText(destination.getFileName(), juce::dontSendNotification);
    stage.setText(plan.tailSeconds > 0 ? "Stereo 24-bit WAV / including " + juce::String(plan.tailSeconds, 2) + " s effect tail" : "Stereo 24-bit WAV / last-segment cutoff", juce::dontSendNotification);
    cancel.setButtonText("Cancel export"); stopMessage = "Cancelling export. The destination will be kept."; tick();
}
void ImportProgressView::showExportProgress(juce::int64 frames)
{
    renderedFrames = frames; fraction = exportPlan.frames > 0 ? juce::jlimit(0., 1., (double) frames / exportPlan.frames) : 0; tick();
}
void ImportProgressView::requestStop()
{
    stopping = true; cancel.setEnabled(false);
    stage.setText(stopMessage, juce::dontSendNotification);
}
Timeline::Timeline(Workstation& w) : owner(w)
{
    setWantsKeyboardFocus(true);
    setFocusContainerType(FocusContainerType::focusContainer);
    setTitle("Timeline segments, transitions and automation");
   #if JUCE_WINDOWS
    // JUCE reports the Windows Menu key as a key-state change.
    struct ContextKeyListener final : juce::KeyListener
    {
        bool keyPressed(const juce::KeyPress&, juce::Component*) override { return false; }
        bool keyStateChanged(bool down, juce::Component* canvas) override
        {
            constexpr int menuKey = 0x10000 | 0x5d; // Extended VK_APPS.
            if (!down || !juce::KeyPress::isKeyCurrentlyDown(menuKey)) return false;
            const juce::KeyPress key(menuKey, juce::ModifierKeys::getCurrentModifiers(), 0);
            if (auto* item = dynamic_cast<TimelineAccessItem*>(juce::Component::getCurrentlyFocusedComponent()))
                if (item->keyPressed(key)) return true;
            return canvas->keyPressed(key);
        }
    };
    static ContextKeyListener contextKey;
    addKeyListener(&contextKey);
   #endif
    initialiseCurveTools();
}
int Timeline::laneTop(int track) const { int top = 0; for (int i = 0; i < track; ++i) top += laneSize(i); return top; }
int Timeline::laneSize(int track) const
{
    std::array<bool, trackCount> expanded {};
    for (const auto& c : owner.engine.project.clips) if (c.stems && c.stemsExpanded) expanded[(size_t) c.track] = true;
    std::array<int, trackCount> sizes {}; int needed = 0, spare = 0;
    for (int i = 0; i < trackCount; ++i)
    {
        sizes[(size_t) i] = owner.layout.laneSize(i);
        if (expanded[(size_t) i]) { needed += std::max(0, 224 - sizes[(size_t) i]); sizes[(size_t) i] = std::max(224, sizes[(size_t) i]); }
        else spare += std::max(0, sizes[(size_t) i] - WorkspaceLayout::minimumLaneHeight);
    }
    int remaining = std::min(needed, spare);
    for (int i = 0; i < trackCount; ++i) if (!expanded[(size_t) i])
    {
        const int available = std::max(0, sizes[(size_t) i] - WorkspaceLayout::minimumLaneHeight);
        const int reduction = spare > 0 ? remaining * available / spare : 0;
        sizes[(size_t) i] -= reduction; remaining -= reduction; spare -= available;
    }
    return sizes[(size_t) track];
}
int Timeline::contentLaneCount() const
{
    return trackCount;
}
int Timeline::trackAt(float y) const
{
    for (int i = 0; i < trackCount; ++i) if (y < laneTop(i + 1)) return i;
    return trackCount - 1;
}
float Timeline::laneY(int track, float y) const { return laneTop(track) + y * (laneSize(track) - curveToolsReserve(track)) / laneHeight; }
int Timeline::dividerAt(float y) const
{
    for (int i = 0; i < trackCount; ++i) if (std::abs(y - (laneTop(i + 1) - 1)) <= 3) return i;
    return -1;
}
bool Timeline::automationUsesGain(AutomationTarget target) const
{
    return target == AutomationTarget::volume || target == AutomationTarget::bass || target == AutomationTarget::mid
        || target == AutomationTarget::treble || target == AutomationTarget::master;
}
float Timeline::automationNodeMaximum(AutomationTarget target) const
{
    return automationUsesGain(target) ? juce::Decibels::gainToDecibels(1.3f) : automationRange(target).getEnd();
}
float Timeline::automationY(int track, float value, int target) const
{
    target = std::max(0, target < 0 ? owner.automationLane() : target);
    const auto type = (AutomationTarget) target;
    const float bottom = laneTop(track) + laneSize(track) - curveToolsReserve(track) - std::min(7.f, laneSize(track) * .1f) - 5;
    const float top = std::min(laneY(track, 26) + 26, bottom - 12);
    if (automationUsesGain(type))
    {
        const float unity = top + std::min(14.f, (bottom - top) * .25f);
        const float gain = value <= -60 ? 0 : juce::jlimit(0.f, 1.3f, juce::Decibels::decibelsToGain(value));
        return gain <= 1 ? bottom - (bottom - unity) * gain : unity - (unity - top) * (gain - 1) / .3f;
    }
    const auto range = automationRange(type);
    return top + (range.getEnd() - value) / range.getLength() * (bottom - top);
}
float Timeline::automationDb(int track, float y, AutomationTarget target) const
{
    const float bottom = laneTop(track) + laneSize(track) - curveToolsReserve(track) - std::min(7.f, laneSize(track) * .1f) - 5;
    const float top = std::min(laneY(track, 26) + 26, bottom - 12);
    if (automationUsesGain(target))
    {
        const float unity = top + std::min(14.f, (bottom - top) * .25f);
        const float gain = y >= unity ? (bottom - y) / (bottom - unity) : 1 + (unity - y) / (unity - top) * .3f;
        return juce::Decibels::gainToDecibels(juce::jlimit(0.f, 1.3f, gain), -60.f);
    }
    const auto range = automationRange(target);
    return range.clipValue(range.getEnd() - (y - top) / (bottom - top) * range.getLength());
}
bool Timeline::hasGeneratedAutomation(const AutomationAddress& address) const
{
    if (address.stem >= 0) return false;
    const int band = (int) address.target;
    const auto* clip = owner.engine.project.clipFor(address.clipId);
    if (!clip || band < 0) return false;
    const auto playback = owner.engine.project.transitionPlaybackFor(*clip);
    if (address.target == AutomationTarget::cutoff)
        return playback.filters[0].setting.active() || playback.filters[1].setting.active();
    if (band >= 4) return false;
    return (band == 0 && (clip->fadeIn > 0 || clip->fadeOut > 0))
        || playback.pairs[0][(size_t) band].strength > 0 || playback.pairs[1][(size_t) band].strength > 0;
}
float Timeline::automationDisplayValue(const AutomationAddress& address, double time, float manual, const TransitionPlayback* compiled) const
{
    const int band = (int) address.target;
    const auto* clip = owner.engine.project.clipFor(address.clipId);
    if (address.stem >= 0 || !clip || band < 0 || band >= 4 || !hasGeneratedAutomation(address)) return manual;
    // An end handle represents the last audible value, before the clip's
    // exclusive playback boundary. Reading the silent exterior corrupts edits.
    const double sampleTime = time == clip->end() ? std::nextafter(time, clip->start) : time;
    float gain = compiled ? compiled->gainAt(band, sampleTime) : owner.engine.project.transitionPlaybackFor(*clip).gainAt(band, sampleTime);
    if (band == 0) gain *= clip->envelope().gainAt(sampleTime - clip->start);
    return automationRange(address.target).clipValue(manual + juce::Decibels::gainToDecibels(gain, -60.f));
}
float Timeline::automationManualValue(const AutomationAddress& address, double time, float displayed) const
{
    return automationRange(address.target).clipValue(displayed - automationDisplayValue(address, time, 0));
}
void Timeline::selectAutomationPoint(const AutomationAddress& address, int point)
{
    if (point >= 0)
    {
        owner.showEditingPage(2);
        owner.clearEditSelection(); owner.automationClip = address.clipId;
        if (address.clipId.isNotEmpty()) owner.selectedClip = address.clipId;
        owner.selectedStem = address.stem; owner.selectedAutomationTarget = (int) address.target;
        if (address.target == AutomationTarget::plugin) owner.pluginLane = address;
    }
    selectedSection.reset(); selectedHotcue.reset(); hotcueGesture.reset();
    selectedCutAmount.reset();
    const auto curve = automationViewCurve(address);
    editingAutomationPoint = point >= 0 && point < (int) curve.points.size()
        ? std::optional<std::pair<AutomationAddress, int>>({address, point}) : std::nullopt;
    refreshAccessibleItems();
    owner.refreshAutomationEditor(false); repaint();
}
juce::String Timeline::automationPointLabel(const AutomationAddress& address, int point) const
{
    if (!editingAutomationPoint || !(editingAutomationPoint->first == address) || editingAutomationPoint->second != point) return {};
    const auto curve = automationViewCurve(address);
    const int track = address.target == AutomationTarget::master ? 0 : address.track;
    if (point < 0 || point >= (int) curve.points.size() || owner.automationLane() != (int) address.target
        || !(owner.displayedAddress(track) == address)) return {};
    const auto& node = curve.points[(size_t) point];
    return owner.automationText(address.target, node.value);
}
std::optional<std::pair<AutomationAddress, int>> Timeline::automationPointAt(juce::Point<float> position) const
{
    if (position.y < 0 || position.y >= laneTop(contentLaneCount())) return {};
    const int track = trackAt(position.y);
    std::optional<std::pair<AutomationAddress, int>> found;
    float nearest = 12;
    for (const auto& address : visibleAutomationAddresses(track))
    {
        if (!automationNodesVisible(address)) continue;
        const auto curve = automationViewCurve(address); if (laneSize(track) < 55) continue;
        for (int index = 0; index < (int) curve.points.size(); ++index)
        {
            const auto& point = curve.points[(size_t) index];
            const double time = owner.engine.project.automationSessionTime(address, point.time);
            const float rawDistance = position.getDistanceFrom({(float) (time * pixelsPerSecond), automationY(track, point.value, (int) address.target)});
            if (rawDistance > 12) continue;
            const float distance = rawDistance - (owner.automationLane() == (int) address.target && owner.displayedAddress(track) == address ? 3.f : 0.f);
            if (distance < nearest) { nearest = distance; found = {{address, index}}; }
        }
    }
    return found;
}
juce::Rectangle<float> Timeline::offscreenAnchorBounds(const Clip& clip, bool intro) const
{
    const auto view = getLocalArea(&owner.viewport,owner.viewport.getLocalBounds()).toFloat()
        .withWidth((float)owner.viewport.getMaximumVisibleWidth());
    const auto bounds = clipBounds(clip);
    const double local = clip.transitionTime(intro);
    if (local < 0 || local > clip.length || !bounds.intersects(view) || view.getWidth() < 100 || bounds.getHeight() < 65) return {};
    const float x = (float)((clip.start + local)*pixelsPerSecond);
    if (x >= view.getX()+7 && x <= view.getRight()-7) return {};
    const bool left = x < view.getX()+7;
    const float other = (float)((clip.start+clip.transitionTime(!intro))*pixelsPerSecond);
    const bool sameEdge = left ? other < view.getX()+7 : other > view.getRight()-7;
    const float separate = !intro && sameEdge ? 41.f : 0.f;
    return {left ? view.getX()+6+separate : view.getRight()-44-separate, bounds.getY()+43,38,17};
}
std::optional<std::pair<juce::String, bool>> Timeline::anchorAt(juce::Point<float> position) const
{
    if (owner.busy || owner.tempoPreparationPending || owner.engine.project.hasPendingPlacement()) return {};
    for (auto it = owner.engine.project.clips.rbegin(); it != owner.engine.project.clips.rend(); ++it)
    {
        const auto bounds = clipBounds(*it);
        if (position.y < bounds.getY() || position.y > bounds.getBottom() || !it->media->hasPlayback()) continue;
        for (const bool intro : {true, false})
        {
            if (offscreenAnchorBounds(*it,intro).contains(position)) return {{it->id,intro}};
            const double local = it->transitionTime(intro);
            if (local < 0 || local > it->length) continue;
            if (std::abs(position.x - (it->start + local) * pixelsPerSecond) <= 7) return {{it->id, intro}};
        }
    }
    return {};
}
void Timeline::mouseMove(const juce::MouseEvent& event)
{
    if (const auto cut = cutAmountAt(event.position))
    { setTooltip(cutAmountText(*cut) + ". Drag up/down for amount only; timing stays fixed. Paired cuts change both songs. Up/Down: 1%; Enter or double-click: exact amount."); setMouseCursor(juce::MouseCursor::UpDownResizeCursor); return; }
    if (const auto cue = hotcueAt(event.position))
    { setTooltip("HC" + juce::String(cue->slot + 1) + ". Drag to edit this segment's source cue; Alt bypasses Snap. Enter/right-click: exact source position. Delete clears only the slot. Controller pad locates while stopped."); setMouseCursor(juce::MouseCursor::LeftRightResizeCursor); return; }
    if (const auto flag = sectionFlagAt(event.position))
    { setTooltip("S" + juce::String(flag->index + 2) + (flag->manual ? " / manual section boundary" : " / energy-change suggestion") + ". Drag horizontally to adjust this segment's source-time flag. Alt bypasses Snap; Enter or double-click opens exact entry. Analysis and current transitions stay intact."); setMouseCursor(juce::MouseCursor::LeftRightResizeCursor); return; }
    if (event.y >= 0 && dividerAt(event.position.y) >= 0)
    { setTooltip("Drag to resize this lane. Double-click restores its default height."); setMouseCursor(juce::MouseCursor::UpDownResizeCursor); return; }
    if (audioSlipAt(event.position))
    { setTooltip("Move this track and its locked partners. Snap uses the timeline grid; Alt moves freely. Shift-drag slips audio only; double-click resets that slip."); setMouseCursor(juce::MouseCursor::LeftRightResizeCursor); return; }
    if (event.y < 0) { setMouseCursor(juce::MouseCursor::NormalCursor); return; }
    if (const auto swap = swapHandleAt(event.position))
    { setTooltip("Drag to move both sides of this swap together. Hold Alt to move without snapping."); setMouseCursor(juce::MouseCursor::LeftRightResizeCursor); return; }
    const auto transition = transitionAt(event.position);
    if (transition && (transition->second == Project::TransitionEdit::moveIncoming || transition->second == Project::TransitionEdit::moveOutgoing))
    {
        setTooltip(juce::String(transition->second == Project::TransitionEdit::moveIncoming ? "I / Incoming song" : "O / Outgoing song")
            + ": drag left/right to move this song inside its fixed transition window. Alt bypasses snap. Unlock protected tracks first.");
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor); return;
    }
    setTooltip(transition ? "Drag the header to move both sides of the preset. Drag its right edge to change duration. Right-click to apply or remove a preset." : juce::String("Drag the waveform to highlight audio; right-click for tools. Drag its title to move it, or an outer edge to trim. Wheel zooms; Alt bypasses beat snap. Drop audio or library songs here."));
    auto pointer = juce::MouseCursor::NormalCursor;
    if (!owner.busy)
    {
        if (edgeTrimAt(event.position)) { pointer=juce::MouseCursor::LeftRightResizeCursor; setTooltip("Drag this outer edge to trim audible audio and waveform. Extend it to restore the untouched source. Alt ignores snap; transition markers stay fixed."); }
        else if (anchorAt(event.position)) { pointer = juce::MouseCursor::LeftRightResizeCursor; setTooltip("OUT adjusts this stationary track's join and moves the next IN into alignment. IN shifts incoming audio beneath the fixed previous OUT. Snap uses the timeline beat grid; Alt moves freely."); }
        else if (labelAt(event.position) >= 0) { pointer = juce::MouseCursor::LeftRightResizeCursor; setTooltip("Drag this label to move it. Double-click to edit its text; right-click for marker actions."); }
        else if (automationPointAt(event.position)) { pointer = juce::MouseCursor::DraggingHandCursor; setTooltip("Drag this node to change time and value. Left/Right selects adjacent nodes; Enter opens precise entry; Delete removes the node. Unlock protected tracks first."); }
        else if (transition) pointer = transition->second == Project::TransitionEdit::moveWindow
            ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::LeftRightResizeCursor;
        else if (automationCurveAt(event.position))
        { pointer = juce::MouseCursor::UpDownResizeCursor; setTooltip("Drag up or down to change the curve shape. Click a node to edit its time or value."); }
        else for (const auto& clip : owner.engine.project.clips)
            if (clipBounds(clip).contains(event.position))
            {
                pointer = juce::MouseCursor::DraggingHandCursor;
                const bool linked = std::any_of(owner.engine.project.transitionLinks.begin(), owner.engine.project.transitionLinks.end(),
                    [&](const auto& link) { return link.incoming == clip.id || link.outgoing == clip.id; });
                setTooltip(linked ? "Drag the title to move this track and its locked partners. Each pair's Locked/Unlocked button controls movement; IN/OUT adjusts alignment."
                    : "Drag the waveform to highlight audio, then right-click for tools. Drag the title to move this piece. Ctrl-click selects several pieces.");
                break;
            }
    }
    setMouseCursor(pointer);
}
juce::Rectangle<float> Timeline::clipBounds(const Clip& c) const
{
    return { (float) (c.start * pixelsPerSecond), (float) laneTop(c.track) + std::min(7.f, laneSize(c.track) * .1f), std::max(3.0f, (float) (c.length * pixelsPerSecond)), (float) laneSize(c.track) - curveToolsReserve(c.track) - 2 * std::min(7.f, laneSize(c.track) * .1f) };
}
void Timeline::updateSize()
{
    const juce::ScopedValueSetter<bool> viewGuard(owner.programmaticTimelineView, true);
    double end = owner.engine.project.audibleEnd();
    if (owner.layout.followPlayhead) end = std::max(end, owner.engine.position.load());
    if (!owner.engine.project.tempo.points.empty()) end = std::max(end, owner.engine.project.tempo.points.back().time);
    if (!owner.engine.project.labels.empty()) end = std::max(end, owner.engine.project.labels.back().time);
    if (!owner.engine.project.masterAutomation.points.empty()) end = std::max(end, owner.engine.project.masterAutomation.points.back().time);
    for (const auto& track : owner.engine.project.tracks) for (const auto& lane : track.pluginAutomation) if (!lane.curve.points.empty()) end = std::max(end, lane.curve.points.back().time);
    for (const auto& scene : owner.engine.project.visualScenes) end = std::max(end, scene.end());
    const double width = std::min((double) std::numeric_limits<int>::max() / 4, std::max(60.0, end + 10) * pixelsPerSecond);
    const int contentHeight = laneTop(contentLaneCount());
    owner.viewport.setScrollBarsShown(contentHeight > owner.viewport.getHeight(), false, true, true);
    setSize(std::max(owner.viewport.getWidth(), (int) width), contentHeight);
    owner.layoutTrackHeaders();
    refreshCurveTools();
    refreshMovementLocks();
    refreshAccessibleItems();
    owner.timelineRuler.refreshAccessibleMarkers();
    owner.tempoStrip.refreshAccessibleMarkers();
    owner.timelineRuler.repaint(); owner.timelineOverview.repaint();
    repaint();
}
void Timeline::zoomAt(double factor, double timelineX)
{
    const juce::ScopedValueSetter<bool> viewGuard(owner.programmaticTimelineView, true);
    const double time = timelineX / pixelsPerSecond;
    const double screenX = timelineX - owner.viewport.getViewPositionX();
    pixelsPerSecond = juce::jlimit(0.1, WorkspaceLayout::maximumTimelineZoom, pixelsPerSecond * factor);
    const int y = owner.viewport.getViewPositionY();
    updateSize();
    owner.viewport.setViewPosition((int) std::round(time * pixelsPerSecond - screenX), y);
    owner.followPlayheadIntoView();
}
void Timeline::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    // Shift retains viewport scrolling for lanes below the visible region.
    if (e.mods.isShiftDown())
    {
        owner.viewport.setViewPosition(owner.viewport.getViewPositionX(), (int) std::round(owner.viewport.getViewPositionY() - wheel.deltaY * 280));
        return;
    }
    if (std::abs(wheel.deltaX) > std::abs(wheel.deltaY))
    { owner.setFollowPlayhead(false); owner.viewport.setViewPosition((int) std::round(owner.viewport.getViewPositionX() - wheel.deltaX * 280), owner.viewport.getViewPositionY()); return; }
    if (wheel.deltaY != 0 && !e.mods.isAnyMouseButtonDown())
        zoomAt(std::exp2(juce::jlimit(-1.0f, 1.0f, wheel.deltaY) * 2), e.position.x);
}
void Timeline::paint(juce::Graphics& g)
{
    g.fillAll(colour::background);
    const auto& p = owner.engine.project;
    auto visible = getLocalArea(&owner.viewport, owner.viewport.getLocalBounds());
    const TempoMap grid = p.gridClock();
    int stride = 1; double fastest = p.bpm;
    for (const auto& point : grid.points) fastest = std::max(fastest, point.bpm);
    while (60 / fastest * stride * pixelsPerSecond < 8) stride *= 4;
    // Four bars (16 beats in 4/4), on the same integrated map as snapping.
    // This is a counting guide, not a claim to have detected musical phrases.
    const double firstGroup = std::floor(grid.beatsAt(visible.getX() / pixelsPerSecond) / 16) * 16;
    if (60 / fastest * 16 * pixelsPerSecond >= 4)
        for (double beat = firstGroup;; beat += 16)
        {
            const float x = (float) (grid.timeAtBeat(beat) * pixelsPerSecond);
            if (x >= visible.getRight()) break;
            if ((int64_t) std::llround(beat / 16) % 2 == 0)
            { g.setColour(colour::raised.withAlpha((float) (0.24 * owner.layout.gridBrightness))); g.fillRect(x, 0.0f,
                (float) (grid.timeAtBeat(beat + 16) * pixelsPerSecond) - x, (float) getHeight()); }
        }
    const auto first = (int64_t) std::floor(grid.beatsAt(visible.getX() / pixelsPerSecond) / stride);
    if (p.tempo.origin > 0)
    {
        const int zeroX = Clip::gridPixel(p.tempo.origin, pixelsPerSecond);
        if (zeroX > visible.getX())
        {
            g.setColour(colour::panel.withAlpha(.18f)); g.fillRect(visible.getX(), 0, std::min(visible.getWidth(), zeroX - visible.getX()), getHeight());
        }
        g.setColour(colour::orange.withAlpha(.65f)); g.drawVerticalLine(zeroX, 0, (float) getHeight());
    }
    struct BeatGuide { double beat; int x; }; std::vector<BeatGuide> guides;
    for (int64_t n = first;; ++n)
    {
        const auto beat = n * stride; if (beat < grid.beatsAt(0) - 1e-8) continue;
        const double time = grid.timeAtBeat((double) beat);
        const int x = Clip::gridPixel(time, pixelsPerSecond); if (x >= visible.getRight()) break;
        guides.push_back({(double) beat, x});
        g.setColour(beat % 16 == 0 ? colour::muted.withAlpha((float) (0.28 * owner.layout.gridBrightness)) : colour::border.withAlpha((float) ((beat % 4 == 0 ? 0.65 : 0.3) * owner.layout.gridBrightness)));
        g.drawVerticalLine(x, 0, (float) getHeight());

    }
    const double subdivision = p.snapStep();
    if (subdivision < 1 && 60 / fastest * subdivision * pixelsPerSecond >= 8)
    {
        const double firstBeat = std::floor(grid.beatsAt(visible.getX() / pixelsPerSecond) / subdivision) * subdivision;
        for (double beat = firstBeat;; beat += subdivision)
        {
            if (beat < grid.beatsAt(0) - 1e-8) continue;
            const int x = Clip::gridPixel(grid.timeAtBeat(beat), pixelsPerSecond); if (x >= visible.getRight()) break;
            if (std::abs(beat - std::round(beat)) < 1e-8) continue;
            guides.push_back({beat, x});
            g.setColour(colour::border.withAlpha((float) (0.18 * owner.layout.gridBrightness)));
            g.drawVerticalLine(x, 0, (float) getHeight());
        }
    }
    for (int i = 0; i < trackCount; ++i) { g.setColour(colour::border); g.drawHorizontalLine(laneTop(i + 1) - 1, (float) visible.getX(), (float) visible.getRight()); }
    const double forgeTime = owner.engine.position.load();
    const float forgeX = (float) Clip::gridPixel(forgeTime, pixelsPerSecond);
    const bool forgeMotion = owner.forgeMotionAllowed() && owner.engine.playing && !owner.engine.gridAudition.active()
        && !owner.engine.rehearsalActive();
    const bool anySolo = std::any_of(p.tracks.begin(), p.tracks.end(), [](const auto& t) { return t.solo; });
    auto forgeEligible = [&](const Clip& c)
    {
        if (c.start > forgeTime || c.end() <= forgeTime || c.pendingPlacement != PendingPlacement::none || (!c.prepared && !c.media->hasPlayback())) return false;
        const auto& lane = p.tracks[(size_t) c.track];
        if (lane.mute || (anySolo && !lane.solo)) return false;
        const auto playback = p.transitionPlaybackFor(c);
        return (!playback.startCut || forgeTime >= *playback.startCut) && (!playback.endCut || forgeTime < *playback.endCut);
    };
    const bool forging = forgeMotion && forgeX >= visible.getX() - 100 && forgeX < visible.getRight() + 100
        && std::count_if(p.clips.begin(), p.clips.end(), forgeEligible) >= 2;
    for (const auto& c : p.clips)
    {
        const auto r = clipBounds(c); if (!r.intersects(visible.toFloat())) continue;
        const auto accent = colour::tracks[(size_t) c.track];
        if (laneSize(c.track) < 30) { g.setColour(accent.withAlpha(.7f)); g.fillRect(r.withHeight(std::min(2.f, r.getHeight()))); continue; }
        const bool selected = c.id == owner.selectedClip || owner.editSelection.contains(c.id);
        g.setColour(accent.withAlpha(selected ? 0.13f : 0.055f)); g.fillRoundedRectangle(r, 5);
        g.setColour(accent.withAlpha(selected ? 1.0f : 0.6f)); g.drawRoundedRectangle(r.reduced(0.5f), 5, selected ? 2.0f : 1.0f);
        juce::Graphics::ScopedSaveState save(g); g.reduceClipRegion(r.toNearestInt());
        const int left = std::max(visible.getX(), (int) r.getX()), right = std::min(visible.getRight(), (int) r.getRight());
        const bool linkedPiece = std::any_of(owner.engine.project.transitionLinks.begin(), owner.engine.project.transitionLinks.end(),
            [&](const auto& link) { return link.incoming == c.id; });
        g.setFont(juce::FontOptions(12).withStyle("Bold")); g.setColour(colour::text); g.drawText(c.media->name
            + (c.pitchSemitones != 0 ? "  [pitch " + juce::String(c.pitchSemitones > 0 ? "+" : "") + juce::String(c.pitchSemitones, 2) + " steps]" : juce::String())
            + (!c.media->playbackError.isEmpty() ? " [audio unavailable]" : !c.media->hasPlayback() ? (c.media->sourceDetailsPending ? " [reading duration / preparing audio]" : " [preparing audio]") : "")
            + (c.pendingPlacement == PendingPlacement::failed ? " [tempo failed; retry Beatmatch]" : c.pendingPlacement != PendingPlacement::none ? " [matching tempo]" : "")
            + (c.reversed ? " [reverse]" : "") + (c.loopSpan > 0 ? " [loop]" : "") + "  " + owner.keyText(*c.media)
            + (c.media->bpm() > 0 ? "  / " + wholeBpmText(c.media->bpm()) + " BPM / " + (c.media->manualBpm <= 0 && (c.media->analysis.variableTempo || c.media->analysis.tempoConfidence < 0.2) ? "beat grid needs review" : c.firstDownbeatTime() >= 0 ? (c.media->manualBeatOrigin >= 0 ? "4/4" : c.media->analysis.downbeatOrigin < 0 ? "4/4 best guess" : "4/4 inferred") : "downbeat unknown") : ""), r.withLeft((float) left + (c.stems ? 120.f : 0.f) + (linkedPiece ? 76.f : 0.f)).withRight((float) right).withTrimmedRight(28).reduced(8, 3).removeFromTop(18), juce::Justification::left);
        const float baseline = r.getBottom() - 5, height = std::max(1.f, r.getHeight() - 32);
        std::vector<WaveformRange> levels; levels.reserve((size_t) std::max(0, right - left));
        bool waveformPending = false;
        float ceiling = 1;
        for (const float peak : c.media->peaks) ceiling = std::max(ceiling, peak);
        for (int x = left; x < right; ++x)
        {
            // Grid lines denote pixel centres (gridPixel rounds to nearest).
            // Sample the same centred interval, rather than drawing a [x,x+1)
            // interval at its left edge, which makes an onset lead its tick.
            const double local = (x - 0.5) / pixelsPerSecond - c.start;
            const auto level = c.waveformRange(local, local + 1 / pixelsPerSecond);
            waveformPending = waveformPending || !level.ready;
            levels.push_back(level); ceiling = std::max(ceiling, level.peak);
        }
        const int mode = owner.waveformMode.getSelectedId();
        float forgePoolY = baseline;
        juce::Path waveform, body; waveform.startNewSubPath((float) left, baseline); body.startNewSubPath((float) left, baseline);
        for (int x = left; x < right; ++x)
        {
            const auto level = levels[(size_t) (x - left)]; const float rms = level.rms / ceiling;
            // Fixed mean-square energy scale exposes structure without viewport
            // normalization: scrolling or zooming cannot inflate a quiet passage.
            const float contour = mode == 1 && pixelsPerSecond < 80 ? std::min(1.0f, 2.5f * rms * rms) : rms;
            if (x == (int) forgeX) forgePoolY = baseline - (mode == 2 ? level.peak / ceiling : contour) * height * .45f;
            waveform.lineTo(x + .5f, baseline - level.peak * height / ceiling); body.lineTo(x + .5f, baseline - contour * height);
        }
        waveform.lineTo((float) right, baseline); waveform.closeSubPath();
        body.lineTo((float) right, baseline); body.closeSubPath();
        if (waveformPending)
        {
            g.setColour(colour::muted); g.setFont(juce::FontOptions(11));
            g.drawText(c.media->original && c.media->original->failed() ? "Waveform unavailable" : "Loading waveform detail...",
                       r.withTop(r.getY() + 22).withHeight(18).reduced(8, 0), juce::Justification::left);
        }
        auto drawWaveform = [&](float opacity)
        {
            if (mode != 1 || pixelsPerSecond >= 80)
            {
                g.setColour(accent.withAlpha(opacity * (mode == 2 ? 0.6f : 0.12f))); g.fillPath(waveform);
                g.setColour(accent.withAlpha(opacity * (mode == 2 ? 0.9f : 0.35f))); g.strokePath(waveform, juce::PathStrokeType(0.8f));
            }
            if (mode != 2)
            { g.setColour(accent.withAlpha(opacity * .72f)); g.fillPath(body); g.setColour(accent.withAlpha(opacity)); g.strokePath(body, juce::PathStrokeType(0.8f)); }
        };
        const auto playback = p.transitionPlaybackFor(c);
        if (!playback.startCut && !playback.endCut) drawWaveform(1);
        else
        {
            // Keep excluded source material as a ghost; editing handles and
            // automation remain fully visible and keep their original clocks.
            const double begin = std::max(c.start, playback.startCut.value_or(c.start));
            const double end = std::min(c.end(), playback.endCut.value_or(c.end()));
            auto drawRegion = [&](double from, double to, float opacity)
            {
                if (to <= from) return;
                juce::Graphics::ScopedSaveState active(g); juce::Path audible;
                audible.addRectangle(r.getX() + (float) ((from - c.start) * pixelsPerSecond), r.getY(), (float) ((to - from) * pixelsPerSecond), r.getHeight());
                g.reduceClipRegion(audible); drawWaveform(opacity);
            };
            if (end <= begin) drawWaveform(.14f);
            else { drawRegion(c.start, begin, .14f); drawRegion(begin, end, 1); drawRegion(end, c.end(), .14f); }
        }
        if (owner.showEnergySections && visibleSectionClips.count(c.id)) paintEnergySections(g, c, r, pixelsPerSecond);
        if (forging && forgeEligible(c))
        {
            juce::Graphics::ScopedSaveState hotRegion(g);
            g.reduceClipRegion(juce::Rectangle<int>((int) forgeX - 150, (int) r.getY() + 24, 164, std::max(1, (int) r.getHeight() - 24)));
            const float begin = (float) (std::max(c.start, playback.startCut.value_or(c.start)) * pixelsPerSecond);
            const float end = (float) (std::min(c.end(), playback.endCut.value_or(c.end())) * pixelsPerSecond);
            g.reduceClipRegion(juce::Rectangle<int>((int) std::ceil(begin), (int) r.getY(), std::max(0, (int) std::floor(end) - (int) std::ceil(begin)), (int) r.getHeight()));
            juce::ColourGradient molten(colour::moltenOrange.withAlpha(0.f), forgeX - 150, baseline,
                                        colour::moltenCore, forgeX, baseline, false);
            molten.addColour(.7, colour::moltenOrange.withAlpha(.8f));
            g.setGradientFill(molten); g.fillPath(mode == 2 ? waveform : body);
            g.strokePath(mode == 2 ? waveform : body, juce::PathStrokeType(1.4f));
            const float poolY = forgePoolY;
            g.setGradientFill(juce::ColourGradient(colour::moltenCore.withAlpha(.8f), forgeX, poolY,
                                                 colour::moltenOrange.withAlpha(0.f), forgeX + 22, poolY, true));
            g.fillEllipse(forgeX - 22, poolY - 22, 44, 44);
            drawForgeSparks(g, {forgeX, poolY}, 70, owner.peakDisplay.clock, .9f, c.track);
        }
        paintStems(g, c);
        // The mix clock owns full-height guides. Detected source pulses
        // remain short ticks, including genuine offsets after free edits.
        for (const auto& guide : guides)
        {
            g.setColour(colour::text.withAlpha((float) ((std::remainder(guide.beat, 16) == 0 ? 0.24 : std::remainder(guide.beat, 4) == 0 ? 0.13 : std::remainder(guide.beat, 1) == 0 ? 0.055 : 0.04) * owner.layout.gridBrightness)));
            g.drawVerticalLine(guide.x, r.getY() + 23, baseline);
        }
        if (c.media->bpm() > 0)
        {
            const double sourceBeat = 60.0 / c.media->bpm();
            const double beatDuration = c.localTimeForSource(sourceBeat);
            const double firstBeat = c.firstBeatTime();
            double firstSourceBeat = c.audioSourceElapsed(firstBeat);
            const double downbeat = c.firstDownbeatTime();
            const double barSource = downbeat >= 0 ? c.audioSourceElapsed(downbeat) : -1;
            int clipStride = beatDuration * pixelsPerSecond < 12 ? 4 : 1;
            if (clipStride == 4 && downbeat >= 0) firstSourceBeat = barSource;
            int64_t n = std::max<int64_t>(0, (int64_t) std::floor((c.audioSourceElapsed((left - r.getX()) / pixelsPerSecond) - firstSourceBeat) / (sourceBeat * clipStride)));
            for (;; ++n) { const double t = c.audioTimelineTime(firstSourceBeat + (double) n * sourceBeat * clipStride); if (t >= c.length || r.getX() + t * pixelsPerSecond >= right) break;
                if (t >= 0) {
                    const double sourceTime = firstSourceBeat + (double) n * sourceBeat * clipStride;
                    const int inBar = downbeat < 0 ? -1 : (int) ((std::llround((sourceTime - barSource) / sourceBeat) * (c.reversed ? -1 : 1) % 4 + 4) % 4);
                    const int x = Clip::gridPixel(c.beatTimelineTime(sourceTime), pixelsPerSecond);
                    // At overview zoom the shared four-bar grid carries the
                    // structure; avoid a bright fence of individual downbeats.
                    g.setColour(inBar == 0 ? colour::text.withAlpha(0.24f) : accent.withAlpha(0.16f));
                    if (beatDuration * clipStride * pixelsPerSecond >= 6) g.drawVerticalLine(x, baseline - (inBar == 0 ? 8 : 4), baseline);
                    if (sourceBeat * pixelsPerSecond > 34) { g.setFont(juce::FontOptions(10)); g.drawText(inBar < 0 ? "|" : juce::String(inBar + 1), x + 3, (int) baseline - 16, 20, 13, juce::Justification::left); }
                } }
            const double anchor = c.media->downbeatOrigin();
            if (anchor >= 0 && !c.reversed && c.loopSpan <= 0 && anchor >= c.offset)
            {
                const double local = c.audioTimelineTime(anchor - c.offset);
                if (local >= 0 && local < c.length) { const float x = r.getX() + (float) (local * pixelsPerSecond); juce::Path flag;
                    flag.addTriangle(x - 4, baseline, x + 4, baseline, x, baseline - 7); g.setColour(colour::orange); g.fillPath(flag); }
            }
        }
        g.setColour(accent.brighter(0.2f));
        for (const bool isIntro : {true, false})
        {
            const float x = r.getX() + (float) (c.transitionTime(isIntro) * pixelsPerSecond);
            g.setColour(colour::orange); g.drawVerticalLine((int) x, r.getY(), baseline);
            juce::Path handle; handle.addTriangle(x - 5, r.getY(), x + 5, r.getY(), x, r.getY() + 7);
            handle.addTriangle(x - 4, baseline, x + 4, baseline, x, baseline - 6); g.fillPath(handle);
            if (c.id == owner.selectedClip) { g.setFont(juce::FontOptions(9)); g.drawText(isIntro ? "IN" : "OUT", (int) x + 5, (int) r.getY() + 20, 26, 12, juce::Justification::left); }
            const auto proxy = offscreenAnchorBounds(c,isIntro);
            if (!proxy.isEmpty())
            {
                g.setColour(colour::panel); g.fillRoundedRectangle(proxy,3);
                g.setColour(colour::orange); g.drawRoundedRectangle(proxy,3,1);
                const bool left = x < proxy.getCentreX();
                g.setFont(juce::FontOptions(9).withStyle("Bold"));
                g.drawText((left ? "< " : "") + juce::String(isIntro ? "IN" : "OUT") + (left ? "" : " >"),proxy,juce::Justification::centred);
            }
        }
        if (c.id == owner.loopStartClip)
        {
            const float x = r.getX() + (float) (c.localTimeForSource(owner.loopStartSource) * pixelsPerSecond);
            g.setColour(colour::focus); g.drawVerticalLine((int) x, r.getY() + 23, baseline);
            g.setFont(juce::FontOptions(10)); g.drawText("LOP", (int) x + 4, (int) baseline - 16, 80, 14, juce::Justification::left);
        }
    }
    {
        for (int track = 0; track < trackCount; ++track)
        for (const auto& address : visibleAutomationAddresses(track))
        {
            if (laneSize(track) < 30) continue;
            const auto curve = automationViewCurve(address);
            const auto bounds = p.automationBounds(address);
            const double begin = std::max(bounds.getStart(), visible.getX() / pixelsPerSecond);
            const double end = std::min(bounds.getEnd(), visible.getRight() / pixelsPerSecond);
            if (end <= begin) continue;
            const int target = (int) address.target;
            const bool combined = hasGeneratedAutomation(address);
            if (!automationVisible(address)) continue;
            const auto line = automationPath(address, begin, end);
            const std::array<juce::Colour, 4> bands {juce::Colour(0xffffad70), juce::Colour(0xffe4d15b), juce::Colour(0xff76c7ef), juce::Colour(0xffd3a3ef)};
            const auto accent = target < 4 ? bands[(size_t) target] : juce::Colour::fromHSV(0.08f + target * 0.115f, 0.65f, 0.95f, 1.0f);
            const bool selected = automationNodesVisible(address);
            g.setColour(accent.withAlpha(selected ? 1.f : curve.active() || combined ? .65f : .3f));
            g.strokePath(line, juce::PathStrokeType(selected ? 2.2f : 1.4f));

        }
    }
    if (p.clips.empty() && owner.automationLane() < 0)
    {
        g.setColour(colour::muted); g.setFont(juce::FontOptions(16));
        g.drawText("Add a song", visible.withY(160).withHeight(30), juce::Justification::centred);

    }
    for (const auto& marker : p.labels) {
        g.setColour(colour::text.withAlpha(.25f));
        g.drawVerticalLine(Clip::gridPixel(marker.time, pixelsPerSecond), 0, (float) getHeight());
    }
    paintTransitions(g); paintAudioSlipHandles(g); paintSectionFlags(g); paintHotcues(g);
    if (owner.timelineRuler.alignmentGuide)
    {
        const float x=(float)(*owner.timelineRuler.alignmentGuide*pixelsPerSecond);
        const float dash[]={6,4}; g.setColour(juce::Colour(0xff67dce5));
        g.drawDashedLine(juce::Line<float>(x,0,x,(float)getHeight()),dash,2,1.5f);
    }
    const int playhead = Clip::gridPixel(owner.engine.position.load(), pixelsPerSecond);
    if (forging)
    {
        const auto seam = juce::Rectangle<float>((float) playhead - 1, (float) visible.getY(), 2, (float) visible.getHeight());
        AlloyLookAndFeel::drawMoltenMetal(g, seam, true, 1);
    }
    g.setColour(colour::text); g.drawVerticalLine(playhead, 0, (float) getHeight());
    paintDropPreview(g);
    paintEditSelection(g);
    paintAutomationNodes(g);
    paintAutomationCaption(g);
}
juce::Rectangle<float> Timeline::transitionBounds(const Project::TransitionLink& link) const
{
    const auto& p = owner.engine.project; const auto* out = p.clipFor(link.outgoing); const auto* in = p.clipFor(link.incoming);
    if (!out || !in || laneSize(out->track) < 55 || laneSize(in->track) < 55) return {};
    const auto range = p.transitionRange(link);
    const float x = (float) (range.getStart() * pixelsPerSecond), right = (float) (range.getEnd() * pixelsPerSecond);
    const float top = laneY(std::min(out->track, in->track), 26);
    const int lastTrack = std::max(out->track, in->track);
    return {x, top, std::max(link.presetOwner.isEmpty() ? 24.f : 1.f, right - x), (float) laneTop(lastTrack + 1) - curveToolsReserve(lastTrack) - top - 8};
}
juce::Rectangle<float> Timeline::transitionSongBounds(const Project::TransitionLink& link, bool outgoing) const
{
    const auto& p = owner.engine.project; const auto* clip = p.clipFor(outgoing ? link.outgoing : link.incoming);
    if (!clip || link.presetOwner.isNotEmpty() || transitionBounds(link).isEmpty()) return {};
    const auto footer = clipFooterBounds(*clip); if (footer.isEmpty()) return {};
    return footer.toFloat().withTrimmedLeft(outgoing ? 26.f : 0.f).withWidth(22).reduced(0, 2);
}
double Timeline::transitionReference(const Project& p, const std::pair<juce::String, Project::TransitionEdit>& hit) const
{
    const auto link = std::find_if(p.transitionLinks.begin(), p.transitionLinks.end(), [&](const auto& x) { return x.incoming == hit.first; });
    if (link == p.transitionLinks.end()) return 0;
    if (hit.second == Project::TransitionEdit::moveOutgoing) return p.clipFor(link->outgoing)->start;
    if (hit.second == Project::TransitionEdit::moveIncoming) return p.clipFor(link->incoming)->start;
    const auto range = p.transitionRange(*link); return hit.second == Project::TransitionEdit::resizeEnd ? range.getEnd() : range.getStart();
}
std::optional<std::pair<juce::String, Project::TransitionEdit>> Timeline::transitionAt(juce::Point<float> point) const
{
    if (owner.engine.project.hasPendingPlacement()) return {};
    const auto& links = owner.engine.project.transitionLinks;
    for (auto it = links.rbegin(); it != links.rend(); ++it)
    {
        const auto box = transitionBounds(*it); if (box.isEmpty()) continue;
        if (transitionSongBounds(*it, true).contains(point)) return {{it->incoming, Project::TransitionEdit::moveOutgoing}};
        if (transitionSongBounds(*it, false).contains(point)) return {{it->incoming, Project::TransitionEdit::moveIncoming}};
        if (point.y < box.getY() || point.y > box.getY() + 18) continue;
        if (std::abs(point.x - box.getRight()) < 5) return {{it->incoming, Project::TransitionEdit::resizeEnd}};
        if (it->presetOwner.isEmpty() && std::abs(point.x - box.getX()) < 5) return {{it->incoming, Project::TransitionEdit::resizeStart}};
        if (box.withHeight(18).contains(point)) return {{it->incoming, Project::TransitionEdit::moveWindow}};
    }
    return {};
}
void Timeline::paintTransitions(juce::Graphics& g)
{
    const auto& p = owner.engine.project;
    const auto visible = g.getClipBounds();
    const TempoMap clock = p.gridClock();
    const auto pulse = AlloyLookAndFeel::fourCountPulse(clock.beatsAt(owner.engine.position.load()), owner.engine.playing.load(), AlloyLookAndFeel::motionAllowed());
    for (const auto& link : p.transitionLinks)
    {
        const auto box = transitionBounds(link); if (box.isEmpty() || !box.expanded(4).intersects(visible.toFloat())) continue;
        const bool selected = owner.selectedTransition == link.incoming;
        if (link.alignmentOnly)
        {
            const auto header = box.withHeight(18);
            g.setColour(colour::panel); g.fillRect(header);
            g.setColour(selected ? colour::focus : colour::muted); g.drawRect(header,1);
            g.setFont(juce::FontOptions(10));
            g.drawText("OUT/IN  |  No preset",header.getIntersection(visible.toFloat()).reduced(5,0),juce::Justification::centredLeft,true);
            continue;
        }
        const auto accent = colour::moltenOrange.interpolatedWith(colour::moltenCore, .1f + pulse.heat * .2f);
        g.setColour(colour::moltenOrange.withAlpha(.025f + pulse.heat * .035f)); g.fillRect(box);
        for (int layer = 3; layer > 0; --layer)
        {
            g.setColour(colour::moltenOrange.withAlpha((selected ? .035f : .018f) + pulse.heat * .018f));
            g.drawRect(box.expanded((float) layer), 1.5f);
        }
        g.setColour(accent.withAlpha(selected ? 1.f : .85f)); g.drawRect(box, selected ? 1.8f : 1.2f);
        const auto header = box.withHeight(18);
        g.setGradientFill(juce::ColourGradient(colour::moltenEdge.darker(.45f), header.getTopLeft(), colour::panel, header.getBottomLeft(), false));
        g.fillRect(header);
        AlloyLookAndFeel::drawMoltenMetal(g, header.withHeight(1.6f), false, pulse.heat);
        g.setColour(colour::text);
        if (link.presetOwner.isEmpty()) g.fillRect(box.getX(), box.getY() + 4, 3.f, 10.f);
        const float handleWidth = std::min(box.getWidth(),link.presetOwner.isEmpty() ? 3.f : 5.f);
        g.fillRect(box.getRight()-handleWidth,box.getY()+4,handleWidth,10.f);
        const auto range = p.transitionRange(link);
        const double beats = clock.beatsAt(range.getEnd()) - clock.beatsAt(range.getStart());
        auto title = header.getIntersection(visible.toFloat()).reduced(6, 0);
        g.setFont(juce::FontOptions(10).withStyle("Bold"));
        if (link.presetName.isEmpty() && title.getWidth() >= 110)
        {
            auto count = title.removeFromLeft(48);
            for (int beat = 0; beat < 4; ++beat)
            {
                const auto cell = count.removeFromLeft(10);
                if (beat == pulse.count)
                {
                    g.setColour(colour::moltenOrange.withAlpha(.15f + pulse.heat * .1f)); g.fillRoundedRectangle(cell.reduced(.5f, 2), 2);
                    g.setColour(colour::moltenCore);
                }
                else g.setColour(colour::orange.withAlpha(.65f));
                g.drawText(juce::String(beat + 1), cell, juce::Justification::centred);
            }
        }
        if (link.locked)
        {
            const auto badge = title.removeFromRight(8).reduced(1, 4);
            g.setColour(juce::Colours::mediumseagreen); g.fillRoundedRectangle(badge, 2);
        }
        g.setColour(colour::text);
        const auto length = juce::String(beats/4,1) + " bars";
        g.drawText(link.presetName.isEmpty() ? juce::String(juce::roundToInt(beats / 4))
            : link.presetName + (title.getWidth() >= 130 ? "  |  " + length : juce::String()),title,juce::Justification::centredLeft,true);
        for (bool outgoing : {true, false})
        {
            const auto handle = transitionSongBounds(link, outgoing);
            if (handle.isEmpty()) continue;
            g.setColour(colour::panel); g.fillRect(handle); g.setColour(accent); g.drawRect(handle);
            g.drawText(outgoing ? "O" : "I", handle, juce::Justification::centred);
        }
    }
    paintSwapHandles(g);
    paintCutAmounts(g);
}
int Timeline::labelAt(juce::Point<float> position) const
{
    if (position.y < -labelHeight || position.y >= 0) return -1;
    const auto& markers = owner.engine.project.labels;
    for (int i = (int) markers.size() - 1; i >= 0; --i)
    {
        const float x = (float) (markers[(size_t) i].time * pixelsPerSecond);
        const float next = i + 1 < (int) markers.size() ? (float) (markers[(size_t) i + 1].time * pixelsPerSecond) - 4 : x + 190;
        if (position.x >= x - 4 && position.x <= std::max(x + 4, std::min(x + 190, next))) return i;
    }
    return -1;
}
void Timeline::mouseDown(const juce::MouseEvent& e)
{
    if (!owner.busy && !owner.engine.exporting && stemMouseDown(e)) return;
    finishSectionGesture(true); // A lost mouse-up must not abandon an unsaved preview.
    owner.correctionFromLibrary = false;
    if (e.y >= 0 && !e.mods.isPopupMenu() && (resizingLane = dividerAt(e.position.y)) >= 0)
    { resizeStartHeight = laneSize(resizingLane); origin = e.getPosition(); return; }
    if (owner.busy || owner.engine.exporting) return;
    dragging.clear(); dragClip.reset(); clipDrag.reset(); anchorDrag.reset(); transitionDrag.reset(); swapDrag.reset(); audioSlipDrag.reset(); edgeTrimDrag.reset(); draggingLabel.clear(); groupMove.reset(); moved = false;
    editingAutomationPoint.reset();
    curveBendDrag.reset();
    cutAmountDrag.reset();
    selectedSection.reset(); hotcueGesture.reset(); selectedHotcue.reset();
    if (beginEdgeTrim(e)) return;
    if (beginEditSelection(e)) return;
    if (beginCutAmount(e)) return;
    if (beginHotcue(e)) return;
    if (beginSectionFlag(e)) return;
    selectedCutAmount.reset();
    if (e.y >= -labelHeight && e.y < 0)
    {
        owner.tempoStrip.selectedTempoPoint = -1; selectedLabel.clear();
        const int index = labelAt(e.position);
        if (index >= 0)
        {
            const auto& marker = owner.engine.project.labels[(size_t) index]; selectedLabel = marker.id;
            if (e.mods.isPopupMenu()) { owner.removeLabelMarker(); return; }
            draggingLabel = marker.id; dragStart = marker.time; origin = e.getPosition(); owner.engine.seek(marker.time);
            owner.setStatus(marker.text + " / " + timeText(marker.time - owner.engine.project.tempo.origin) + " / drag to move, double-click to edit, right-click to remove");
        }
        else owner.engine.seek(std::max(0.0, e.x / pixelsPerSecond));
        owner.refresh(); return;
    }
    selectedLabel.clear();
    owner.tempoStrip.selectedTempoPoint = -1;
    if (!automationPointAt(e.position))
    {
        if (beginAudioSlip(e)) return;
        if (beginSwapDrag(e)) return;
    }
    if (!e.mods.isPopupMenu() && !automationPointAt(e.position)) if (const auto hit = transitionAt(e.position))
    {
        transitionDrag = hit; dragStart = transitionReference(owner.engine.project, *hit); origin = e.getPosition();
        // Canvas edits select their owner without invoking seek/scroll navigation.
        owner.selectTransition(hit->first, false);
        grabKeyboardFocus();
        if (hit->second == Project::TransitionEdit::moveIncoming || hit->second == Project::TransitionEdit::moveOutgoing)
        {
            if (const auto* link = owner.engine.project.transitionFor(hit->first))
            {
                owner.selectedClip = hit->second == Project::TransitionEdit::moveOutgoing ? link->outgoing : link->incoming;
                if (const auto* clip = owner.currentClip()) owner.selectTrack(clip->track);
                owner.refresh();
            }
        }
        owner.setStatus("Transition selected. Right-click its header for tools. Drag the frame, corners or I/O handles to adjust."); return;
    }
    if (!e.mods.isPopupMenu() && !automationPointAt(e.position)) if (const auto anchor = anchorAt(e.position))
    {
        owner.selectedClip = anchor->first;
        const auto* clip = owner.currentClip(); if (!clip) return;
        owner.selectTrack(clip->track); dragging = clip->id; draggingIntro = anchor->second;
        anchorDrag = owner.engine.project; origin = e.getPosition();
        dragStart = clip->start + clip->transitionTime(draggingIntro); owner.refresh();
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor); return;
    }
    if (e.mods.isPopupMenu() && !automationPointAt(e.position))
    {
        const auto anchor = juce::Rectangle<int>(1, 1).withPosition(e.getScreenPosition());
        if (const auto frame = transitionAt(e.position))
        { owner.showTransitionMenu(frame->first, anchor); return; }
        if (const auto swap = swapHandleAt(e.position))
        { owner.showTransitionMenu(swap->incoming, anchor); return; }
        if (const auto curve = automationCurveAt(e.position))
        {
            selectAutomationCurve(*curve);
            owner.showAutomationNodeMenu(*curve, -1, e.x / pixelsPerSecond, anchor);
            return;
        }
    }
    if (const auto hit = automationPointAt(e.position))
    {
        owner.selectedStem = hit->first.stem;
        owner.automationClip = hit->first.clipId;
        if (hit->first.clipId.isNotEmpty()) owner.selectedClip = hit->first.clipId;
        owner.selectedAutomationTarget = (int) hit->first.target;
        if (hit->first.target == AutomationTarget::plugin) owner.pluginLane = hit->first;
        const int track = trackAt((float) e.y);
        owner.selectTrack(track); const juce::ScopedLock guard(owner.engine.lock);
        int point = hit->second;
        const auto viewCurve = automationViewCurve(hit->first);
        if (point < 0 || point >= (int) viewCurve.points.size()) return;
        const auto& node = viewCurve.points[(size_t) point];
        automationCanExceedUnity = node.value >= -1.e-5f;
        if (e.mods.isPopupMenu())
        {
            selectAutomationPoint(hit->first, point); owner.refresh();
            owner.showAutomationNodeMenu(hit->first, point, e.x / pixelsPerSecond);
            return;
        }
        selectAutomationPoint(hit->first, point); grabKeyboardFocus(); owner.refresh();
        owner.setStatus(juce::String(automationName(hit->first.target)) + " " + automationPointLabel(hit->first, point) + " / drag or Enter for precise entry"); return;
    }
    if (!e.mods.isPopupMenu()) if (const auto curve = automationCurveAt(e.position))
    {
        selectAutomationCurve(*curve); grabKeyboardFocus();
        owner.setStatus(juce::String(automationName(curve->target)) + " curve / drag the line up/down to bend; drag a node to change its time/value");
        beginCurveBend(*curve, e.position);
        repaint(); return;
    }
    for (auto it = owner.engine.project.clips.rbegin(); it != owner.engine.project.clips.rend(); ++it)
        if (clipBounds(*it).contains(e.position))
        {
            if (e.mods.isPopupMenu())
            {
                const double time = e.x / pixelsPerSecond;
                const auto& scope = owner.editSelection;
                if (scope.contains(it->id) && (!scope.range || scope.range->contains(time)))
                    owner.editSelection.primary = it->id;
                else
                    owner.clearEditSelection();
                owner.selectedClip = it->id;
                owner.selectTrack(it->track);
                owner.showTimelineMenu(time, it->track);
                return;
            }
            owner.selectedAutomationTarget = -1;
            if (owner.editSelection.clips.size() > 1 && owner.editSelection.contains(it->id))
            { beginGroupMove(e, it->id); return; }
            owner.clearEditSelection();
            owner.selectedClip = it->id; owner.selectTrack(it->track); dragging = it->id; dragStart = it->start; dragClip = *it; clipDrag = owner.engine.project; origin = e.getPosition(); owner.refresh(); return;
        }
    if (e.mods.isPopupMenu() && e.y >= 0)
    { owner.clearEditSelection(); owner.selectedClip.clear(); owner.refresh(); owner.showTimelineMenu(e.x / pixelsPerSecond, trackAt((float) e.y)); return; }
    owner.selectedAutomationTarget = -1;
    owner.engine.seek(std::max(0.0, e.x / pixelsPerSecond)); owner.refresh();
}
void Timeline::mouseDrag(const juce::MouseEvent& e)
{
    if (continueEdgeTrim(e)) return;
    if (continueGroupMove(e)) return;
    if (continueEditSelection(e)) return;
    if (resizingLane >= 0) { owner.resizeLane(resizingLane, resizeStartHeight + e.y - origin.y); return; }
    if (continueHotcue(e)) return;
    if (continueSectionFlag(e)) return;
    if (continueCutAmount(e)) return;
    if (continueCurveBend(e)) return;
    if (continueAudioSlip(e)) return;
    if (continueSwapDrag(e)) return;
    if (transitionDrag && !owner.busy && e.mouseWasDraggedSinceMouseDown())
    {
        owner.engine.finishAutomation(); const juce::ScopedLock guard(owner.engine.lock);
        auto candidate = owner.engine.project;
        const double wanted = dragStart + (e.x - origin.x) / pixelsPerSecond;
        const auto result = candidate.editTransition(transitionDrag->first, transitionDrag->second,
            wanted - transitionReference(candidate, *transitionDrag), candidate.snap && !e.mods.isAltDown());
        if (result.failed()) { owner.setStatus(result.getErrorMessage()); return; }
        if (std::abs(transitionReference(candidate, *transitionDrag) - transitionReference(owner.engine.project, *transitionDrag)) < 1e-8) return;
        if (!moved) { owner.remember(); moved = true; }
        owner.engine.project = std::move(candidate); updateSize(); owner.tempoStrip.repaint();
        owner.setStatus("Transition adjusted. Undo restores the complete edit."); return;
    }
    if (anchorDrag && !owner.busy && e.mouseWasDraggedSinceMouseDown())
    {
        owner.engine.finishAutomation();
        auto candidate = *anchorDrag;
        const auto result = candidate.moveTransitionAnchor(dragging, draggingIntro, dragStart + (e.x - origin.x) / pixelsPerSecond, candidate.snap && !e.mods.isAltDown());
        if (result.failed()) { owner.setStatus(result.getErrorMessage()); return; }
        const auto selected = std::find_if(candidate.clips.begin(), candidate.clips.end(), [&](const auto& c) { return c.id == dragging; });
        const auto* current = owner.currentClip();
        if (selected == candidate.clips.end() || !current) return;
        const auto& existing = owner.engine.project.clips;
        if (std::equal(candidate.clips.begin(), candidate.clips.end(), existing.begin(), existing.end(), [](const auto& a, const auto& b)
            { return a.start == b.start && a.introAnchor == b.introAnchor && a.outroAnchor == b.outroAnchor; })) return;
        if (!moved) { owner.remember(); moved = true; }
        { const juce::ScopedLock guard(owner.engine.lock); const auto applied = owner.engine.project.applyPlacements(candidate);
          if (applied.failed()) { owner.setStatus(applied.getErrorMessage()); return; } }
        updateSize(); owner.tempoStrip.repaint();
        owner.setStatus(draggingIntro ? "IN source adjusted beneath the fixed OUT join. Previous track kept in place."
            : "OUT adjusted in this track. The next IN and its locked followers remain aligned; this track stays put."); return;
    }
    if (draggingLabel.isNotEmpty() && !owner.busy)
    {
        if (!e.mouseWasDraggedSinceMouseDown()) return;
        const auto& labels = owner.engine.project.labels;
        const auto found = std::find_if(labels.begin(), labels.end(), [&](const auto& item) { return item.id == draggingLabel; });
        if (found == labels.end()) return;
        auto marker = *found;
        marker.time = std::min(1.0e9, owner.engine.project.snappedTime(dragStart + (e.x - origin.x) / pixelsPerSecond, owner.engine.project.snap && !e.mods.isAltDown()));
        if (marker.time == found->time) return;
        if (std::any_of(labels.begin(), labels.end(), [&](const auto& item) { return item.id != marker.id && item.time == marker.time; })) return;
        if (!moved) { owner.engine.setAutomationRecordMode(AutomationRecordMode::read); owner.remember(); moved = true; }
        { const juce::ScopedLock guard(owner.engine.lock); owner.engine.project.putLabelMarker(marker); }
        owner.setStatus(marker.text + " / " + timeText(marker.time - owner.engine.project.tempo.origin)); repaint(); return;
    }
    if (editingAutomationPoint && owner.automationLane() >= 0 && !owner.busy)
    {
        const auto address = editingAutomationPoint->first; const int draggingPoint = editingAutomationPoint->second;
        const int automationTrack = address.target == AutomationTarget::master ? 0 : address.track;
        if (!e.mouseWasDraggedSinceMouseDown()) return;
        AutomationCurve* displayed=nullptr;
        const auto viewCurve = automationViewCurve(address);
        if (draggingPoint >= (int) viewCurve.points.size() || !owner.automationEditable(address)) return;
        double time = owner.engine.project.snappedTime(e.position.x / pixelsPerSecond, owner.engine.project.snap && !e.mods.isAltDown());
        // Keep exact clip-edge handles inside their audio clock. Integer mouse
        // coordinates (or float rounding just beyond the end) otherwise sample
        // the silent exterior and turn a vertical drag into a spurious boost.
        if (!owner.engine.project.snap || e.mods.isAltDown()) if (const auto* clip = owner.engine.project.clipFor(address.clipId))
            for (const double edge : {clip->start, clip->end()})
                if (std::abs(e.position.x - edge * pixelsPerSecond) <= .5) { time = edge; break; }
        if (owner.engine.exporting || owner.engine.project.automationLocked(address)) return;
        auto candidate = owner.automationEditSnapshot();
        auto* envelope = owner.materialiseAutomation(candidate, address); if (!envelope) return;
        if (envelope->points.empty()) *envelope = viewCurve;
        float value = automationDb(automationTrack, e.position.y, address.target);
        if (automationUsesGain(address.target) && value < 0) automationCanExceedUnity = false;
        if (automationUsesGain(address.target) && !automationCanExceedUnity) value = std::min(0.f, value);
        const auto result = candidate.moveAutomationPoint(address, draggingPoint, time, value);
        if (result.failed()) { owner.setStatus(result.getErrorMessage()); return; }
        const auto& changed = candidate.curve(address)->points[(size_t) draggingPoint];
        if(owner.engine.project.snap && !e.mods.isAltDown()
            && std::abs(candidate.automationSessionTime(address,changed.time)-time)>1.e-7)
        { owner.setStatus("No timeline beat fits between these neighbouring nodes. Hold Alt for a free edit."); return; }
        const auto& existing = viewCurve.points[(size_t) draggingPoint];
        if (changed.time == existing.time && changed.value == existing.value) return;
        candidate.curve(address)->enabled = true;
        if (!owner.publishAutomationEdit(std::move(candidate),address,!moved)) return;
        moved=true;
        displayed = owner.engine.project.curve(address); if (!displayed || draggingPoint >= (int) displayed->points.size()) return;
        const auto& point = displayed->points[(size_t) draggingPoint]; owner.setStatus(timeText(point.time) + " / " + owner.automationText(address.target, automationDisplayValue(address, owner.engine.project.automationSessionTime(address, point.time), point.value))); repaint(); return;
    }
    if (dragging.isEmpty() || !dragClip || owner.busy || !e.mouseWasDraggedSinceMouseDown()) return;
    owner.engine.finishAutomation();
    const juce::ScopedLock guard(owner.engine.lock);
    if (auto* c = owner.currentClip())
    {
        // Background source loading may finish during a gesture. Keep its
        // published media/mapping instead of restoring the mouse-down placeholder.
        if (c->media != dragClip->media || c->tempoMap != dragClip->tempoMap || c->awaitingSourceLength != dragClip->awaitingSourceLength)
        { dragClip = *c; clipDrag = owner.engine.project; dragStart = c->start; origin = e.getPosition(); }
        const double start = std::max(0.0, dragStart + (e.x - origin.x) / pixelsPerSecond);
        const int track = trackAt((float) e.y);
        if (e.x == origin.x && c->start == dragStart && c->track == track) return;
        auto candidate = clipDrag.value_or(owner.engine.project);
        if (e.x != origin.x || track != c->track)
        {
            const auto result = candidate.moveClipGroup({dragging},dragging,start,track,candidate.snap && !e.mods.isAltDown() && e.x!=origin.x);
            if (result.failed()) { owner.setStatus(result.getErrorMessage()); return; }
        }
        if (!moved) { owner.remember(); moved = true; }
        const auto applied = owner.engine.project.applyPlacements(candidate);
        if (applied.failed()) { owner.setStatus(applied.getErrorMessage()); return; }
        owner.engine.selectedTrack = track;
        owner.setStatus("Tracks moved.");
    }
    repaint();
}
void Timeline::mouseUp(const juce::MouseEvent& event)
{
    if (groupMove) { finishGroupMove(); mouseMove(event); return; }
    if (rangeDragAnchor) { finishEditSelection(); return; }
    finishSectionGesture(); hotcueGesture.reset();
    if (resizingLane >= 0) { resizingLane = -1; mouseMove(event); return; }
    if (moved) owner.refresh();
    curveBendDrag.reset();
    cutAmountDrag.reset();
    dragging.clear(); dragClip.reset(); clipDrag.reset(); anchorDrag.reset(); transitionDrag.reset(); swapDrag.reset(); audioSlipDrag.reset(); edgeTrimDrag.reset(); draggingLabel.clear();
    mouseMove(event);
}
void Timeline::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (const auto cue = hotcueAt(e.position)) { selectHotcue(*cue); owner.editHotcue(cue->clipId, cue->slot); return; }
    if (const auto cut = cutAmountAt(e.position)) { editCutAmount(*cut); return; }
    if (const auto flag = sectionFlagAt(e.position)) { selectSectionFlag(*flag); editSectionFlag(*flag); return; }
    if (e.y >= 0) if (const int lane = dividerAt(e.position.y); lane >= 0) { owner.resizeLane(lane, owner.layout.defaultLaneHeight); return; }
    if (owner.busy || owner.engine.exporting) return;
    if (!automationPointAt(e.position) && resetAudioSlip(e.position)) return;
    if (e.y >= -labelHeight && e.y < 0)
    {
        const int index = labelAt(e.position); owner.editLabelMarker(index >= 0 ? owner.engine.project.labels[(size_t) index].id : juce::String(), owner.engine.project.snappedTime(e.x / pixelsPerSecond, owner.engine.project.snap && !e.mods.isAltDown())); return;
    }
    if (const auto hit = automationPointAt(e.position))
    {
        owner.selectedStem = hit->first.stem;
        owner.automationClip = hit->first.clipId;
        if (hit->first.clipId.isNotEmpty()) owner.selectedClip = hit->first.clipId;
        owner.selectedAutomationTarget = (int) hit->first.target;
        if (hit->first.target == AutomationTarget::plugin) owner.pluginLane = hit->first;
        const int track = trackAt((float) e.y);
        selectAutomationPoint(hit->first, hit->second);
        owner.editAutomationPoint(track, hit->second); return;
    }
    if (const auto curve = automationCurveAt(e.position)) { selectAutomationCurve(*curve); grabKeyboardFocus(); return; }
    owner.engine.seek(e.x / pixelsPerSecond);
}

LibraryTable::LibraryTable(Workstation& w, bool playlist) : owner(w), isPlaylist(playlist)
{
    table.setTitle(playlist ? "Playlist segments" : "Song library");
    table.rowName = [this](int row)
    {
        if (row < 0 || row >= getNumRows()) return juce::String();
        const auto* clip = isPlaylist ? clipAtRow(row) : nullptr;
        if (isPlaylist && !clip) return juce::String();
        const auto& media = isPlaylist ? clip->media : owner.engine.library[(size_t) owner.visibleMedia[(size_t) row]];
        auto title = media->libraryTitle();
        if (media->assets.artist.isNotEmpty()) title += ", artist " + media->assets.artist;
        title += ", BPM " + (media->bpm() > 0 ? wholeBpmText(media->bpm()) : juce::String("unknown"));
        title += ", key " + owner.keyText(*media) + " / " + keyNoteName(media->key());
        if (isPlaylist)
            title = "Segment " + juce::String((int) (clip - owner.engine.project.clips.data()) + 1) + ", " + title
                + ", mixer lane " + juce::String(clip->track + 1) + ", start " + timeText(clip->start - owner.engine.project.tempo.origin)
                + ", length " + (clip->awaitingSourceLength ? juce::String("pending") : timeText(clip->length));
        else
        {
            title += ", duration " + timeText(media->duration()) + ". " + Workstation::libraryReviewHint(*media);
            if (media->assets.genre.isNotEmpty()) title += ", genre " + media->assets.genre;
        }
        if (media->playbackError.isNotEmpty()) title += ", unavailable: " + media->playbackError;
        else if (media->sessionMetadataPending) title += ", checking source";
        return title;
    };
    table.setModel(this); table.setMultipleSelectionEnabled(true); table.setRowHeight(playlist ? 28 : 44); table.setHeaderHeight(26); table.setColour(juce::ListBox::backgroundColourId, colour::panel);
    auto& h = table.getHeader(); h.addColumn(playlist ? "TRACK" : "TITLE", 1, playlist ? 80 : 90, 80, 1000, juce::TableHeaderComponent::defaultFlags);
    h.addColumn(playlist ? "LANE" : "TIME", 2, playlist ? 50 : 62, 50, 160, playlist ? juce::TableHeaderComponent::defaultFlags : juce::TableHeaderComponent::notSortable);
    h.addColumn(playlist ? "START" : "KEY", 3, playlist ? 82 : 60, 60, 220, juce::TableHeaderComponent::defaultFlags);
    h.addColumn(playlist ? "LENGTH" : "BPM", 4, playlist ? 84 : 62, 60, 180, juce::TableHeaderComponent::defaultFlags);
    if (!playlist) h.addColumn("GENRE", 8, 100, 80, 240, juce::TableHeaderComponent::defaultFlags);
    h.addColumn("ART", 5, playlist ? 36 : 32, 32, 120);
    if (playlist) h.addColumn("TRANSITION IN", 6, 130, 130, 300, juce::TableHeaderComponent::notSortable);
    if (!playlist) { h.moveColumn(5, 0); h.moveColumn(2, 4); }
    else { h.moveColumn(6, 1); h.moveColumn(5, 5); }
    table.setHeaderHeight(26); h.setStretchToFitActive(false);
}
int LibraryTable::getNumRows() { return isPlaylist ? (int) playlistRows.size() : (int) owner.visibleMedia.size(); }
juce::Component* LibraryTable::accessibleCell(int row, int column, bool selected, juce::Component* previous)
{
    if (row < 0 || row >= getNumRows()) { delete previous; return nullptr; }
    const auto* clip = isPlaylist ? clipAtRow(row) : nullptr;
    if (isPlaylist && !clip) { delete previous; return nullptr; }
    const auto& media = isPlaylist ? clip->media : owner.engine.library[(size_t) owner.visibleMedia[(size_t) row]];
    auto* cell = dynamic_cast<LibraryCell*>(previous);
    if (!cell) { delete previous; cell = new LibraryCell(*this); }
    cell->row = row; cell->column = column; cell->selected = selected;
    juce::String text;
    if (column == 1) text = (isPlaylist ? media->name : media->libraryTitle())
        + (media->playbackError.isNotEmpty() ? ", unavailable" : media->sessionMetadataPending ? ", checking source" : "");
    else if (column == 5) text = media->assets.cover.isValid() ? "Artwork present" : "No local cover artwork";
    else if (isPlaylist)
    {
        if (column == 2) text = "Mixer lane " + juce::String(clip->track + 1);
        else if (column == 3) text = "Start " + timeText(clip->start - owner.engine.project.tempo.origin);
        else if (column == 4) text = "Length " + (clip->awaitingSourceLength ? juce::String("pending") : timeText(clip->length));
    }
    else
    {
        if (column == 2) text = "Duration " + timeText(media->duration());
        else if (column == 3) text = "Key " + owner.keyText(*media) + " / " + keyNoteName(media->key());
        else if (column == 4) text = "BPM " + (media->bpm() > 0 ? wholeBpmText(media->bpm()) : juce::String("unknown"))
            + (media->analysis.variableTempo && media->manualBpm == 0 ? ", variable tempo" : media->manualBpm == 0 && media->analysis.tempoConfidence < .2 ? ", uncertain" : "");
        else if (column == 8) text = "Genre " + (media->assets.genre.isEmpty() ? juce::String("not tagged") : media->assets.genre);
    }
    cell->setTitle(text); cell->setDescription(table.rowName(row));
    if(!isPlaylist && column==1)
    {
        cell->seek.source(media);cell->seek.setVisible(true);cell->resized();
        cell->seek.setComponentID("library-seek-"+media->id);cell->seek.setTitle("Preview position / "+media->libraryTitle());
        const auto route=owner.libraryPreview.usesPrivateOutput() ? juce::String("Uses the configured headphones output; programme playback continues.")
            : juce::String("Uses master output and pauses the arrangement without moving its cursor.");
        cell->seek.setTooltip("Click or drag to preview this part of the track. "+route);
        cell->seek.setDescription("Seek through this source without adding it to the mix. Left/Right seeks five seconds; Shift seeks thirty; Home/End seeks its edges; Space toggles preview; Escape stops. "+route);
        cell->seek.audition=[this](const std::shared_ptr<Media>& target,std::optional<double> fraction)
        {
            owner.correctionFromLibrary=true;
            for(size_t i=0;i<owner.visibleMedia.size();++i)if(owner.engine.library[(size_t)owner.visibleMedia[i]]->id==target->id)
            {if(!table.isRowSelected((int)i))table.selectRow((int)i);break;}
            owner.previewLibraryTrack(target->id,fraction);
        };
        cell->seek.progress=[this,cell] {const auto m=cell->seek.media;return m && owner.libraryPreview.active(m->id) ? owner.libraryPreview.positionFraction():-1.;};
        cell->seek.length=[this,cell] {const auto m=cell->seek.media;return m ? owner.libraryPreview.active(m->id) && owner.libraryPreview.lengthSeconds()>0 ? owner.libraryPreview.lengthSeconds():m->duration():0.;};
        cell->seek.updatePosition();
    }
    else
    {cell->seek.setVisible(false);cell->seek.source({});cell->seek.audition={};cell->seek.progress={};cell->seek.length={};}
    return cell;
}
Clip* LibraryTable::clipAtRow(int row) const
{
    return row >= 0 && row < (int) playlistRows.size() ? owner.engine.project.clipFor(playlistRows[(size_t) row]) : nullptr;
}
void LibraryTable::rebuildPlaylist()
{
    juce::StringArray selectedIds; const auto oldRows = table.getSelectedRows();
    for (int i = 0; i < oldRows.size(); ++i) if (oldRows[i] >= 0 && oldRows[i] < (int) playlistRows.size()) selectedIds.add(playlistRows[(size_t) oldRows[i]]);
    if (selectedIds.size() <= 1 || !selectedIds.contains(owner.selectedClip)) { selectedIds.clear(); selectedIds.add(owner.selectedClip); }
    playlistRows.clear(); for (const auto& clip : owner.engine.project.clips) playlistRows.push_back(clip.id);
    if (playlistSortColumn != 0) std::stable_sort(playlistRows.begin(), playlistRows.end(), [&](const auto& a, const auto& b)
    {
        const auto& first = *owner.engine.project.clipFor(a); const auto& second = *owner.engine.project.clipFor(b);
        int order = 0;
        if (playlistSortColumn == 5)
        {
            const auto& x = first.media->assets.artworkGroup; const auto& y = second.media->assets.artworkGroup;
            if (x.isEmpty() != y.isEmpty()) return !x.isEmpty();
            order = x.compare(y);
            if (order == 0) return first.media->libraryTitle().compareNatural(second.media->libraryTitle()) < 0;
        }
        else if (playlistSortColumn == 1) order = first.media->name.compareNatural(second.media->name);
        else
        {
            const double x = playlistSortColumn == 2 ? first.track : playlistSortColumn == 3 ? first.start : first.length;
            const double y = playlistSortColumn == 2 ? second.track : playlistSortColumn == 3 ? second.start : second.length;
            order = x < y ? -1 : x > y ? 1 : 0;
        }
        return playlistSortForward ? order < 0 : order > 0;
    });
    table.updateContent();
    juce::SparseSet<int> selection;
    for (int row = 0; row < (int) playlistRows.size(); ++row) if (selectedIds.contains(playlistRows[(size_t) row])) selection.addRange({row, row + 1});
    table.setSelectedRows(selection, juce::dontSendNotification);
}
void LibraryTable::paintRowBackground(juce::Graphics& g, int row, int w, int h, bool selected)
{
    if (isPlaylist) if (const auto* clip = clipAtRow(row)) selected = selected || clip->id == owner.selectedClip;
    g.fillAll(selected ? colour::orange.withAlpha(0.13f) : (row % 2 ? colour::panel : colour::panel.brighter(0.025f)));
    if (selected) { g.setColour(colour::orange); g.fillRect(0, 0, 3, h); }
    juce::ignoreUnused(w);
}
void LibraryTable::paintCell(juce::Graphics& g, int row, int col, int w, int h, bool)
{
    if (row < 0 || row >= getNumRows()) return;
    const juce::Graphics::ScopedSaveState paintState(g);
    const bool review = !isPlaylist && Workstation::libraryNeedsReview(*owner.engine.library[(size_t) owner.visibleMedia[(size_t) row]]);
    // Keep selection, preview and hit targets fully active; only subdue the ink.
    const auto reviewInk = [review](juce::Colour value) { return review ? value.interpolatedWith(colour::muted, .6f) : value; };
    juce::String text;
    if (col == 5)
    {
        const auto* clip = isPlaylist ? clipAtRow(row) : nullptr; if (isPlaylist && !clip) return;
        const auto& m = isPlaylist ? clip->media : owner.engine.library[(size_t) owner.visibleMedia[(size_t) row]];
        const int size = std::min(h - 6, w - 6); const juce::Rectangle<int> cover((w - size) / 2, (h - size) / 2, size, size);
        g.setColour(colour::raised); g.fillRoundedRectangle(cover.toFloat(), 3);
        if (m->assets.cover.isValid()) g.drawImageWithin(m->assets.cover, cover.getX(), cover.getY(), cover.getWidth(), cover.getHeight(), juce::RectanglePlacement::centred);
        else { g.setColour(colour::muted); g.setFont(juce::FontOptions(9)); g.drawText("MW", cover, juce::Justification::centred); }
        if (!isPlaylist)
        {
            const auto centre = cover.getCentre().toFloat();
            g.setColour(juce::Colours::black.withAlpha(.7f)); g.fillEllipse(centre.x-11,centre.y-11,22,22);
            g.setColour(owner.libraryPreview.active(m->id) ? colour::orange : juce::Colours::white);
            if (owner.libraryPreview.active(m->id)) g.fillRect(centre.x-4,centre.y-4,8.f,8.f);
            else { juce::Path play; play.addTriangle(centre.x-3,centre.y-6,centre.x-3,centre.y+6,centre.x+6,centre.y); g.fillPath(play); }
        }
        return;
    }
    if (isPlaylist)
    {
        const auto* clip = clipAtRow(row); if (!clip) return; const auto& c = *clip;
        if (col == 1) text = c.media->name + (!c.media->playbackError.isEmpty() ? " [unavailable]" : !c.media->hasPlayback() ? " [preparing]" : c.beatmatched ? "  [sync]" : ""); else if (col == 2) text = trackLabel(c.track); else if (col == 3) text = timeText(c.start - owner.engine.project.tempo.origin); else text = c.awaitingSourceLength ? "Pending" : timeText(c.length);
    }
    else
    {
        const auto& m = owner.engine.library[(size_t) owner.visibleMedia[(size_t) row]];
        if (col == 1)
        {
            const int x = 10;
            g.setColour(reviewInk(colour::text)); g.setFont(juce::FontOptions(12));
            g.drawText(m->libraryTitle() + (m->playbackError.isNotEmpty() ? " [unavailable]" : m->sessionMetadataPending ? " [checking source]" : ""), x, 4, std::max(0, w - x - 5), 18, juce::Justification::centredLeft, true);
            // The title cell's lower line is the seekable source preview.
            // Filename, artist and review detail remain in hover/accessibility.
            return;
        }
        else if (col == 8) text = m->assets.genre;
        else if (col == 2) text = timeText(m->duration()).dropLastCharacters(4); else if (col == 3) text = owner.keyText(*m); else text = m->bpm() > 0 ? wholeBpmText(m->bpm()) + (m->analysis.variableTempo && m->manualBpm == 0 ? " ~" : m->manualBpm == 0 && m->analysis.tempoConfidence < 0.2 ? " ?" : "") : "Unknown";
        if (col == 3 && m->key() >= 0)
        {
            auto* selected = owner.currentClip();
            if (selected && harmonicallyCompatible(selected->media->key(), m->key())) { g.setColour(colour::orange.withAlpha(0.12f)); g.fillRoundedRectangle(3, 3, (float) w - 6, (float) h - 6, 3); }
        }
    }
    g.setColour(reviewInk(col == 1 ? colour::text : colour::muted)); g.setFont(juce::FontOptions(12)); g.drawText(text, 10, 0, w - 15, h, juce::Justification::centredLeft, true);
}
void LibraryTable::cellDoubleClicked(int row, int column, const juce::MouseEvent&)
{
    if (!isPlaylist && column != 5 && row >= 0 && row < getNumRows()) owner.addMedia(owner.visibleMedia[(size_t) row]);
    else if (isPlaylist) { if (const auto* clip = clipAtRow(row)) owner.engine.seek(clip->start); }
}
juce::String LibraryTable::getCellTooltip(int row, int column)
{
    if (row < 0 || row >= getNumRows()) return {};
    const auto* clip = isPlaylist ? clipAtRow(row) : nullptr; if (isPlaylist && !clip) return {};
    const auto& media = isPlaylist ? clip->media : owner.engine.library[(size_t) owner.visibleMedia[(size_t) row]];
    const auto& a = media->analysis;
    if (!isPlaylist && column == 5) return owner.libraryPreview.usesPrivateOutput()
        ? "Play / stop preview in headphones.\n" + owner.libraryPreview.description()
        : "Play / stop preview through master output. Arrangement playback pauses; its position is kept.";
    const auto retryHint = a.focusedRevision > 0 && (a.focusedTempo > 0 || a.focusedKey > 0)
        ? juce::String(" Open Source corrections to review the retry result.") : juce::String();
    if (!isPlaylist && column == 1) return media->libraryTitle()
        + (media->assets.artist.isNotEmpty() ? " / "+media->assets.artist : juce::String())
        + "\n"+media->file.getFileName()+"\n"+Workstation::libraryReviewHint(*media);
    if (!isPlaylist && column == 4) return Workstation::libraryReviewHint(*media);
    if (!isPlaylist && column == 8) return media->assets.genre.isEmpty() ? "No genre tag found in this file." : "File genre tag: " + media->assets.genre;
    if (!isPlaylist && column == 3)
        return Workstation::keyEvidenceHint(*media);
    return media->playbackError.isNotEmpty() ? "This file could not be opened. Find it or import it again."
        : media->sessionMetadataPending ? "Checking this track's file. The mix can continue playing."
        : "Select this track or drag it into the mix." + retryHint;
}
juce::PopupMenu LibraryTable::beatmatchMenu(int row)
{
    const auto* clip = isPlaylist ? clipAtRow(row) : nullptr;
    if (isPlaylist ? clip == nullptr : row < 0 || row >= getNumRows()) return {};
    const auto media = isPlaylist ? clip->media : owner.engine.library[(size_t) owner.visibleMedia[(size_t) row]];
    if (!media) return {};
    juce::PopupMenu menu;
    if (!isPlaylist)
    {
        menu.addItem(8700, owner.libraryPreview.active(media->id) ? "Stop preview" : (owner.libraryPreview.usesPrivateOutput() ? "Preview in headphones" : "Preview through master"));
        if (owner.libraryPreview.usesPrivateOutput()) menu.addItem(8701, "Choose preview output..."); menu.addSeparator();
    }
    const auto targets = owner.analysisTargets(0);
    const bool available = !owner.busy && !owner.engine.playing && owner.timelineLoads.empty()
        && owner.pendingTempo <= 0 && !owner.tempoPreparationPending && !owner.engine.exporting;
    menu.addSectionHeader(juce::String(targets.size()) + (targets.size() == 1 ? " source selected" : " sources selected"));
    menu.addItem(64, "Retry weak BPM / key", available && std::any_of(targets.begin(), targets.end(), [](const auto& m) { return Workstation::canRetryAnalysis(*m); }));
    menu.addItem(60, "Re-analyse selected tracks", available && !targets.empty());
    menu.addSeparator();
    menu.addItem(65, "Select all visible tracks (Ctrl+A)", !owner.busy && getNumRows() > 0);
    menu.addItem(63, "Select tracks needing analysis", !owner.busy && !owner.engine.library.empty());
    if (table.getNumSelectedRows() > 1) return menu;
    menu.addSeparator();
    menu.addSubMenu("Beatmatch", owner.getMenuForIndex(3, {}));
    menu.addSeparator();
    menu.addItem(8600, "Split track...");
    menu.addItem(8601, "Split track on CPU...");
    if (isPlaylist && clip)
    {
        menu.addSeparator();
        menu.addItem((int) TimelineAction::insertBefore, "Insert song before this segment...", owner.canInsertArrangement() && !owner.engine.library.empty());
        menu.addItem((int) TimelineAction::insertAfter, "Insert song after this segment...", owner.canInsertArrangement() && !owner.engine.library.empty());
    }
    return menu;
}
void LibraryTable::cellClicked(int row, int, const juce::MouseEvent& event)
{
    // Clicking an already-selected row need not emit selectedRowsChanged.
    owner.correctionFromLibrary = !isPlaylist;
    if (!event.mods.isPopupMenu()) return;
    const auto* clip = isPlaylist ? clipAtRow(row) : nullptr;
    if (isPlaylist ? clip == nullptr : row < 0 || row >= getNumRows()) return;
    const auto media = isPlaylist ? clip->media : owner.engine.library[(size_t) owner.visibleMedia[(size_t) row]];
    if (!media) return;
    if (!table.isRowSelected(row)) table.selectRow(row);
    selectedRowsChanged(row);
    const auto targets = owner.analysisTargets(0);
    auto menu = beatmatchMenu(row);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&table).withTargetScreenArea({event.getScreenX(), event.getScreenY(), 1, 1}),
        [safe = juce::Component::SafePointer<Workstation>(&owner), sourceId = media->id, clipId = clip ? clip->id : juce::String(), targets](int command)
        {
            if (!safe || command == 0) return;
            if (command == 8700) { safe->previewLibraryTrack(sourceId); return; }
            if (command == 8701) { safe->libraryPreview.chooseOutput(*safe); return; }
            if (command == 60 || command == 64) { safe->reanalyseTracks(0, command == 64, targets); return; }
            if (command == 8600 || command == 8601) { safe->splitStems(sourceId, command == 8601); return; }
            if (command == (int) TimelineAction::insertBefore || command == (int) TimelineAction::insertAfter)
                safe->chooseArrangementInsertion(clipId, command == (int) TimelineAction::insertAfter);
            else if (command == 25) safe->editAnalysis(sourceId);
            else safe->menuItemSelected(command, 3);
        });
}
void LibraryTable::selectedRowsChanged(int row)
{
    if (!owner.refreshing) owner.correctionFromLibrary = !isPlaylist;
    if (isPlaylist && clipAtRow(row)) { const auto& c = *clipAtRow(row); owner.timeline.selectedLabel.clear(); owner.selectedClip = c.id; owner.selectTrack(c.track); owner.refresh(); }
    else if (!isPlaylist) owner.selectedLibraryId = row >= 0 && row < getNumRows() ? owner.engine.library[(size_t) owner.visibleMedia[(size_t) row]]->id : juce::String();
}
void LibraryTable::sortOrderChanged(int column, bool forwards)
{
    if (isPlaylist) { playlistSortColumn = column; playlistSortForward = forwards; rebuildPlaylist(); table.repaint(); return; }
    owner.librarySortReferenceId = (column == 3 || column == 4) ? owner.selectedLibraryId : juce::String();
    owner.librarySortColumn = column; owner.librarySortForward = forwards; owner.filterLibrary(); table.updateContent();
    if (table.getSelectedRow() >= 0) table.scrollToEnsureRowIsOnscreen(table.getSelectedRow());
    table.repaint();
}
namespace
{
// Native title bars have no JUCE content inset. Keep the app-owned frame above
// an opaque VST editor without resizing or intercepting the editor's controls.
class PluginFrame final : public juce::Component, private juce::ComponentListener
{
public:
    explicit PluginFrame(juce::Component& window) : host(&window)
    {
        setInterceptsMouseClicks(false, false); setAlwaysOnTop(true);
        window.addComponentListener(this); window.addAndMakeVisible(this); setBounds(window.getLocalBounds());
    }
    ~PluginFrame() override { if (host) host->removeComponentListener(this); }
    void paint(juce::Graphics& g) override { g.setColour(colour::moltenOrange); g.drawRect(getLocalBounds(), 2); }
private:
    juce::Component* host;
    void componentMovedOrResized(juce::Component& window, bool, bool) override { setBounds(window.getLocalBounds()); }
    void componentBeingDeleted(juce::Component&) override { host = nullptr; delete this; }
};
}
PluginWindow::PluginWindow(PluginSlot& slot) : juce::DocumentWindow(slot.description.name, colour::panel, juce::DocumentWindow::closeButton), slotId(slot.id)
{
    getProperties().set("mixworksExternalEditor", true);
    setUsingNativeTitleBar(true); auto* editor = slot.processor->createEditorAndMakeActive();
    if (editor == nullptr) editor = new juce::GenericAudioProcessorEditor(*slot.processor);
    setContentOwned(editor, true); setResizable(editor->isResizable(), false); centreWithSize(editor->getWidth(), editor->getHeight()); setVisible(true);
    AlloyLookAndFeel::styleNativeTitleBar(*this);
    new PluginFrame(*this); // Lifetime ends with this window's Component notification.
}
PluginList::PluginList(Workstation& w) : owner(w) { list.setModel(this); list.setRowHeight(36); list.setColour(juce::ListBox::backgroundColourId, colour::background); }
int PluginList::getNumRows() { return (int) owner.engine.pluginChain(owner.engine.selectedTrack.load()).size(); }
void PluginList::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= getNumRows()) return;
    const auto& p = owner.engine.pluginChain(owner.engine.selectedTrack.load())[(size_t) row];
    g.fillAll(selected ? colour::raised : colour::background); g.setColour(p->bypass ? colour::muted : colour::orange); g.fillEllipse(10, 14, 7, 7);
    const int latency = p->unsupportedLatency();
    g.setColour(latency > 0 ? juce::Colours::orangered : colour::text); g.setFont(juce::FontOptions(12)); g.drawText(juce::String(row + 1) + "  " + p->description.name + (p->bypass ? " [bypassed]" : latency > 0 ? " [latency " + juce::String(latency) + " / blocked]" : ""), 27, 0, w - 32, h, juce::Justification::left);
}
void PluginList::listBoxItemDoubleClicked(int row, const juce::MouseEvent&) { owner.showEditor(row); }
juce::String PluginList::getTooltipForRow(int row)
{
    const auto& chain = owner.engine.pluginChain(owner.engine.selectedTrack.load());
    if (row < 0 || row >= (int) chain.size()) return {};
    const auto& slot = *chain[(size_t) row]; const auto issue = slot.latencyError();
    return issue.isNotEmpty() ? issue : slot.description.name + (slot.bypass ? " / bypassed" : "");
}
TrackStrip::TrackStrip(Workstation& w, int i) : owner(w), index(i)
{
    setWantsKeyboardFocus(true); setTitle("Resize lane " + juce::String(i + 1));
    select.setTooltip("Select this mixer lane and its effects. This does not move any segments.");
    mute.setTooltip("Mute this lane in playback and export. Click again to restore it.");
    solo.setTooltip("Hear only soloed lanes. Click again to remove this lane from solo.");
    pan.setTooltip("Move the sound left or right. Double-click to centre it.");
    gain.setTooltip("A direct move takes control; Record captures a performance.");
    setTooltip("Drag the bottom edge to resize this lane; double-click resets. With the edge focused, Up/Down resize, Home resets and End doubles the height.");
    for (auto* c : std::initializer_list<juce::Component*> { &select, &mute, &solo, &cutoff, &gain, &pan }) addAndMakeVisible(c);
    select.setButtonText(trackLabel(i) + "  AUDIO " + juce::String(i + 1)); select.onClick = [this] { owner.selectTrack(index); };
    mute.setClickingTogglesState(true); solo.setClickingTogglesState(true);
    slider(gain, -60, 12, 0.1, " dB"); slider(pan, -1, 1, 0.01); gain.setDoubleClickReturnValue(true, 0); pan.setDoubleClickReturnValue(true, 0);
    pan.configure("Lane " + juce::String(i + 1) + " pan");
    pan.textFromValueFunction = [](double value) { return std::abs(value) < 0.005 ? juce::String("C") : juce::String(std::abs(value) * 100, 0) + (value < 0 ? " L" : " R"); };
    pan.valueFromTextFunction = [](const juce::String& text) { const auto v = text.trim().toUpperCase(); return v == "C" ? 0.0 : juce::jlimit(-1.0, 1.0, v.getDoubleValue() / 100 * (v.contains("L") ? -1 : 1)); };
    cutoff.configure("Lane " + juce::String(i + 1) + " cutoff filter"); cutoff.configureCutoff();
    cutoff.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 65, 18);
    cutoff.setTooltip("Left removes treble; right removes bass across this lane. Double-click returns to Off.");
    cutoff.onDragStart = [this] { controlGesture = true; if (!owner.engine.canWriteAutomation(index)) owner.remember(); };
    cutoff.onDragEnd = pan.onDragEnd = gain.onDragEnd = [this] { controlGesture = false; };
    cutoff.onValueChange = [this] { if (!owner.refreshing) { if (!controlGesture && !owner.engine.canWriteAutomation(index)) owner.remember(); owner.engine.setControlValue({index, AutomationTarget::cutoff}, (float) cutoff.getValue()); owner.dirty = true; } };
    pan.updateText(); gain.setName("Lane " + juce::String(i + 1) + " volume");
    gain.onDragStart = pan.onDragStart = [this] { controlGesture = true; if (!owner.engine.canWriteAutomation(index)) owner.remember(); };
    gain.onValueChange = [this] { if (!owner.refreshing) { if (!controlGesture && !owner.engine.canWriteAutomation(index)) owner.remember(); const juce::ScopedLock guard(owner.engine.lock); owner.engine.setControlValue({index, AutomationTarget::volume}, (float) gain.getValue()); owner.dirty = true; } };
    pan.onValueChange = [this] { if (!owner.refreshing) { if (!controlGesture && !owner.engine.canWriteAutomation(index)) owner.remember(); const juce::ScopedLock guard(owner.engine.lock); owner.engine.setControlValue({index, AutomationTarget::pan}, (float) pan.getValue()); owner.dirty = true; } };
    mute.onClick = [this] { owner.performAction({MidiActionKind::mute,index}); };
    solo.onClick = [this] { owner.performAction({MidiActionKind::solo,index}); };
}
void TrackStrip::refresh()
{
    const auto& t = owner.engine.controlSettings(index);
    const auto& mixer = owner.engine.project.tracks[(size_t) index];
    const auto laneName = "Mixer lane " + juce::String(index + 1);
    const auto controlOwner = laneName;
    auto describe = [](juce::Slider& control, const juce::String& title)
    {
        auto& properties = control.getProperties();
        if (!properties.contains("alloyOwnerBaseHint")) properties.set("alloyOwnerBaseHint", control.getTooltip());
        control.setName(title); control.setTitle(title); control.setTooltip(title + ". " + properties["alloyOwnerBaseHint"].toString());
    };
    describe(gain, controlOwner + " / Volume"); describe(pan, controlOwner + " / Pan"); describe(cutoff, laneName + " / Filter cutoff");
    select.setTooltip("Select " + laneName + ". Volume, Pan, Filter, Mute and Solo affect every track on this lane. VST effects retain their selected track owner.");
    if (!gain.isMouseButtonDown() && !AlloyLookAndFeel::editingSliderText(gain)) gain.setValue(t.gainDb, juce::dontSendNotification);
    if (!pan.isMouseButtonDown() && !AlloyLookAndFeel::editingSliderText(pan)) pan.setValue(t.pan, juce::dontSendNotification);
    if (!cutoff.isMouseButtonDown() && !AlloyLookAndFeel::editingSliderText(cutoff)) cutoff.setValue(mixer.filterCutoff, juce::dontSendNotification);
    const bool unlocked = !owner.engine.project.automationLocked({index, AutomationTarget::volume});
    for (auto* control : {static_cast<juce::Slider*>(&pan), &gain}) control->setEnabled(!owner.busy && unlocked);
    cutoff.setEnabled(!owner.busy && !owner.engine.project.automationLocked({index, AutomationTarget::cutoff}));
    select.setButtonText(trackLabel(index) + "  " + (owner.engine.controlClip(index) ? owner.engine.controlClip(index)->media->name : mixer.name) + (std::any_of(t.automation.begin(), t.automation.end(), [](const auto& curve) { return curve.active(); }) || std::any_of(t.pluginAutomation.begin(), t.pluginAutomation.end(), [](const auto& lane) { return lane.curve.active(); }) ? " [A]" : ""));
    mute.setToggleState(mixer.mute, juce::dontSendNotification); solo.setToggleState(mixer.solo, juce::dontSendNotification);
    select.setColour(juce::TextButton::buttonColourId, owner.engine.selectedTrack == index ? colour::orange.withAlpha(0.23f) : colour::raised); repaint();

}
void TrackStrip::paint(juce::Graphics& g)
{
    g.fillAll(colour::panel); g.setColour(colour::border); g.drawHorizontalLine(getHeight() - 1, 0, (float) getWidth());
    if (getHeight() < 30) { g.setColour(colour::tracks[(size_t) index]); g.fillRect(7, getHeight() / 2, getWidth() - 14, 1); return; }
    g.setColour(colour::tracks[(size_t) index]); g.fillRoundedRectangle(1, 5, 2, (float) getHeight() - 13, 1);
    if (getHeight() >= 124)
    {
        g.setColour(colour::muted); g.setFont(juce::FontOptions(10));
        g.drawText("CUTOFF", 7, 50, 65, 12, juce::Justification::centred);
        g.drawText("PAN " + pan.getTextFromValue(pan.getValue()), 78, 50, getWidth() - 85, 12, juce::Justification::centred);
    }
    auto level = juce::jlimit(0.0f, 1.0f, owner.engine.meters[(size_t) index].load());
    g.setColour(colour::background); g.fillRect(6, getHeight() - 6, getWidth() - 12, 3);
    AlloyLookAndFeel::drawMoltenMetal(g, {6.f, getHeight() - 6.f, (getWidth() - 12) * level, 3}, false, level);
    if (level >= 1) { g.setColour(colour::error); g.fillRect(getWidth() - 8, getHeight() - 7, 3, 5); }
    if (owner.forgeMotionAllowed() && level >= .89f
        && std::fmod(owner.peakDisplay.clock + index * .31, .73) < .22)
        drawForgeSparks(g, {6 + (getWidth() - 12) * level, getHeight() - 6.f}, 18, owner.peakDisplay.clock, .8f, index);
}
void TrackStrip::resized()
{
    const int h = getHeight(), half = (getWidth() - 21) / 2;
    for (auto* c : std::initializer_list<juce::Component*> {&select, &mute, &solo, &cutoff, &pan, &gain}) c->setVisible(h >= 30);
    if (h < 30) return;
    select.setBounds(7, 4, getWidth() - 14, 20);
    mute.setBounds(7, 28, half, 20); solo.setBounds(14 + half, 28, half, 20);
    gain.setBounds(7, h - 28, getWidth() - 14, 20); gain.setTextBoxStyle(juce::Slider::TextBoxRight, false, 57, 18);
    const bool dials = h >= 100;
    const int dialTop = h >= 124 ? 63 : 50;
    cutoff.setBounds(7, dialTop, half, std::max(20, h - 31 - dialTop));
    pan.setBounds(14 + half, dialTop, half, std::max(20, h - 31 - dialTop));
    for (auto* dial : {&cutoff, &pan})
    {
        dial->setVisible(dials);
        dial->setTextBoxStyle(h >= 164 ? juce::Slider::TextBoxBelow : juce::Slider::NoTextBox, false, half, 18);
    }
    // Compact editing lanes still need immediate Mute/Solo/level access.
    // Keep the title, then share one control row instead of hiding the mixer.
    if (h >= 50 && h < 76)
    {
        mute.setBounds(7, 28, 26, 20); solo.setBounds(36, 28, 26, 20);
        gain.setBounds(67, 28, std::max(20, getWidth() - 74), 20);
        gain.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    }
    mute.setVisible(h >= 50); solo.setVisible(h >= 50); gain.setVisible(h >= 50);

}

void TrackStrip::mouseMove(const juce::MouseEvent& e) { setMouseCursor(e.y >= getHeight() - 5 ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor); }
void TrackStrip::mouseDown(const juce::MouseEvent& e) { resizingLane = e.y >= getHeight() - 5; resizeStartHeight = getHeight(); if (resizingLane) grabKeyboardFocus(); }
void TrackStrip::mouseDrag(const juce::MouseEvent& e) { if (resizingLane) owner.resizeLane(index, resizeStartHeight + e.getDistanceFromDragStartY()); }
void TrackStrip::mouseDoubleClick(const juce::MouseEvent&) { owner.resizeLane(index, owner.layout.defaultLaneHeight); }

bool TrackStrip::keyPressed(const juce::KeyPress& key)
{
    const int code = key.getKeyCode();
    if (code == juce::KeyPress::upKey || code == juce::KeyPress::downKey)
    { owner.resizeLane(index, getHeight() + (code == juce::KeyPress::upKey ? -1 : 1) * (key.getModifiers().isShiftDown() ? 1 : 8)); return true; }
    if (code == juce::KeyPress::homeKey || code == juce::KeyPress::endKey)
    { owner.resizeLane(index, owner.layout.defaultLaneHeight * (code == juce::KeyPress::homeKey ? 1 : 2)); return true; }
    return false;
}

Workstation::Workstation(bool openAudioDevice) : status("Ready"), timeline(*this), timelineRuler(*this, false), timelineOverview(*this, true), tempoStrip(*this), libraryTable(*this, false), playlistTable(*this, true), pluginList(*this), piano(engine.keyboard, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    bool firstWorkspace=false;
    statusAccess.setInterceptsMouseClicks(false, false);
    statusAccess.read = [this]
    {
        auto text = "Status: " + juce::String(busy ? "Working. " : "") + status
            + ". Document: " + (sessionFile == juce::File() ? juce::String("Untitled mix") : sessionFile.getFullPathName())
            + (dirty ? ", unsaved changes." : sessionFile == juce::File() ? ", no unsaved changes." : ", saved.");
        if (const auto* clip = engine.project.clipFor(selectedClip))
            text += " Selected segment " + juce::String((int) (clip - engine.project.clips.data()) + 1)
                + ", " + clip->media->libraryTitle() + ", mixer lane " + juce::String(clip->track + 1) + ".";
        else text += " Mixer lane " + juce::String(engine.selectedTrack.load() + 1) + ".";
        return text;
    };
    addAndMakeVisible(statusAccess);
    setLookAndFeel(&look); setWantsKeyboardFocus(true);
    hints.setLookAndFeel(&look);
    for (auto* c : std::initializer_list<juce::Component*> {&libraryTab, &playlistTab, &leftSplitter, &rightSplitter, &bottomSplitter}) addAndMakeVisible(c);
    for (auto* b : {&libraryTab, &playlistTab}) b->getProperties().set("alloyTab", true);
    libraryTab.onClick = [this] { showBrowserPage(0); }; playlistTab.onClick = [this] { showBrowserPage(1); };
    addAndMakeVisible(musicScanNotice); label(musicScanNotice, {}, 11);
    musicScanNotice.setJustificationType(juce::Justification::centredLeft);
    musicScanNotice.setComponentID("music-scan-notice");
    leftSplitter.onValueChange = [this] { resizeSidePanel(true, (int) leftSplitter.getValue()); };
    leftSplitter.restore = [this]
    {
        if (layout.libraryCollapsed) { layout.libraryCollapsed = false; workspaceDirty = true; workspaceChangedAt = juce::Time::getMillisecondCounterHiRes(); resized(); repaint(); }
        else resizeSidePanel(true, WorkspaceLayout::defaultLibraryWidth);
    };
    bottomSplitter.reset = [this] { resizeTimeline(0); };
    bottomSplitter.onValueChange = [this] { resizeTimeline((int) bottomSplitter.getValue()); };
    rightSplitter.onValueChange = [this] { resizeSidePanel(false, (int) rightSplitter.getValue()); };
    rightSplitter.restore = [this]
    {
        if (layout.inspectorCollapsed) { layout.inspectorCollapsed = false; workspaceDirty = true; workspaceChangedAt = juce::Time::getMillisecondCounterHiRes(); resized(); repaint(); }
        else resizeSidePanel(false, WorkspaceLayout::defaultInspectorWidth);
    };
    for (auto* child : std::initializer_list<juce::Component*> {&menuBar, &displayButton, &windowModeButton, &minimiseButton, &closeAppButton, &trackHeaders, &keyboardButton, &instrumentSetupButton, &clipDetail, &sidebarHelp}) addAndMakeVisible(child);
    displayButton.setTooltip("Window mode, 16:9 window size and monitor. Also available under View / Display.");
    windowModeButton.setTooltip("Switch between full-monitor fullscreen and your previous window size (F11).");
    minimiseButton.setTitle("Minimise"); minimiseButton.setTooltip("Minimise Mixworks");
    closeAppButton.setTitle("Close Mixworks"); closeAppButton.setTooltip("Close Mixworks (Alt+F4)");
    displayButton.onClick = [this]
    {
        if (auto* window = findParentComponentOfClass<AppWindow>())
            window->displayMenu().showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&displayButton),
                [safe = juce::Component::SafePointer<AppWindow>(window)](int action) { if (safe) safe->performDisplayAction(action); });
    };
    windowModeButton.onClick = [this] { toggleWindowMode(); };
    minimiseButton.onClick = [this] { if (auto* window = findParentComponentOfClass<AppWindow>()) window->setMinimised(true); };
    closeAppButton.onClick = [this] { if (auto* window = findParentComponentOfClass<AppWindow>()) window->closeButtonPressed(); };
    label(clipDetail, {}, 13); clipDetail.setColour(juce::Label::textColourId, colour::text); clipDetail.setJustificationType(juce::Justification::topLeft);
    label(sidebarHelp, {}, 12); sidebarHelp.setJustificationType(juce::Justification::topLeft);
    newButton.setTooltip("Start an empty mix. Unsaved changes are checked before replacing the current mix.");
    openButton.setTooltip("Open a saved .mixworks arrangement. Unsaved changes are checked first.");
    saveButton.setTooltip("Choose a .mixworks file and save this arrangement, automation and effects.");
    importButton.setTooltip("Add audio files to the library for analysis. Drag a library row onto a lane to add it to the mix.");
    exportButton.setTooltip("Render the mix to a WAV file. Choose whether to include the configured effects tail.");
    playButton.setTooltip("Play or pause the mix at the current cursor. Space also toggles playback.");
    stopButton.setTooltip("Stop playback and return the cursor to the beginning of the mix.");
    homeButton.setTooltip("Move the playback cursor to the beginning of the mix.");
    undoButton.setTooltip("Undo the most recent musical edit. Workspace appearance is saved separately.");
    redoButton.setTooltip("Reapply the last undone musical edit.");
    splitButton.setTooltip("Split the selected segment at the playhead, keeping both resulting segments.");
    duplicateButton.setTooltip("Create another copy of the selected block in the arrangement.");
    removeButton.setTooltip("Remove the selected block from this arrangement.");
    libraryTab.setTooltip("Browse and search analysed audio. Drag a row to a lane, or double-click to add it to the mix.");
    playlistTab.setTooltip("View the current mix track list. Select a row to select its timeline segment.");
    editingTab.setTooltip("Close or focus the open details inspector. Use its dock button to move it to a separate window.");
    keyboardButton.setTooltip("Show or hide the on-screen piano below the VST3 chain for the selected lane instrument.");
    instrumentSetupButton.setTooltip("Open audio devices, MIDI inputs and controller mapping setup.");
    controllerTab.setTooltip("Show the live desk with faders, paired pots and optional MIDI Learn.");
    transitionTab.setTooltip("Show treatment, timing, locking and comparison controls for the selected transition.");
    search.setTooltip("Filter library songs by text. Clear the search to show the whole library.");
    trackInfoButton.setTooltip("Open source beat-grid audition and BPM, downbeat and key corrections for the selected source.");
    reverseButton.setTooltip("Play the selected segment backwards. The original audio file is kept.");
    loopButton.setTooltip("Set up repeated playback of the selected segment section.");
    addPluginButton.setTooltip("Choose a VST3 effect or instrument to insert on the selected lane.");
    editPluginButton.setTooltip("Open the selected VST3 plugin editor.");
    bypassPluginButton.setTooltip("Switch the selected plugin out of processing while keeping it in the chain.");
    removePluginButton.setTooltip("Remove the selected plugin from this lane and close its editor.");
    pluginControlsButton.setTooltip("Choose plugin parameters for MIDI control and automation.");
    timeLabel.setTooltip("Current mix cursor in minutes, seconds and milliseconds. Click the ruler to seek.");
    piano.getProperties().set("mixworksHint", "Play the selected lane instrument with this piano. Enable a hardware MIDI input in Audio / MIDI.");
    menuBar.getProperties().set("mixworksHint", "Open a menu to see its actions.");
    libraryTable.table.getProperties().set("mixworksHint", "Select a library row; double-click adds it to the mix, or drag it onto a lane. Click a column heading to sort, or drag its edge to resize. Right-click for source actions.");
    playlistTable.table.getProperties().set("mixworksHint", "Select a row to select its timeline segment; double-click seeks to its start. Click a heading to sort the list. Source audio stays in place.");
    pluginList.list.getProperties().set("mixworksHint", "Select a plugin to open, bypass or remove it with the controls below. This chain belongs to the selected lane.");
    initialiseMasteringUI();
    initialiseTransitionUI();
    initialisePerformance();
    keyboardButton.setClickingTogglesState(true);
    keyboardButton.onClick = [this] { showInspectorPage(keyboardButton.getToggleState() ? 2 : 1); };
    instrumentSetupButton.onClick = [this] { settings(); };
    for (auto* c : std::initializer_list<juce::Component*> { &newButton, &openButton, &saveButton, &importButton, &exportButton, &settingsButton, &playButton, &stopButton, &homeButton, &undoButton, &redoButton, &splitButton, &duplicateButton, &removeButton, &gridDimmer, &snapButton, &tempoSlider, &masterSlider, &monitorMute, &timeLabel, &inspectorLabel, &pluginLabel, &masterLabel, &libraryTable.table, &playlistTable.table, &viewport, &timelineRuler, &timelineOverview, &tempoStrip, &search, &pluginList.list, &addPluginButton, &editPluginButton, &bypassPluginButton, &removePluginButton, &pluginControlsButton, &piano, &midiLabel, &trackInfoButton, &keyDisplay, &beatmatchButton }) addAndMakeVisible(c);
    viewport.setViewedComponent(&timeline, false); viewport.setScrollBarsShown(false, false, true, true);
    viewport.setScrollBarThickness(WorkspaceLayout::timelineScrollbarThickness);
    viewport.getHorizontalScrollBar().getProperties().set("alloyMolten", true);
    viewport.getVerticalScrollBar().getProperties().set("alloyMolten", true);
    masterSlider.getProperties().set("alloyMolten", true); gridDimmer.getProperties().set("alloyMolten", true);
    playButton.getProperties().set("alloyMolten", true);
    addAndMakeVisible(waveformMode);
    waveformMode.addItem("Waveform: energy", 1); waveformMode.addItem("Waveform: peaks", 2); waveformMode.addItem("Waveform: detail", 3); waveformMode.setSelectedId(1, juce::dontSendNotification);
    waveformMode.setTooltip("Choose how much waveform detail to show. This changes the view, not the sound.");
    waveformMode.onChange = [this] { timeline.repaint(); };
    for (int i = 0; i < trackCount; ++i) { strips[(size_t) i] = std::make_unique<TrackStrip>(*this, i); trackHeaders.addAndMakeVisible(*strips[(size_t) i]); }
    sceneLane = std::make_unique<SceneLane>(*this);
    studioWorkspace = std::make_unique<StudioWorkspace>(*sceneLane);
    studioPanel = std::make_unique<ToolPanel>(*studioWorkspace, "Visual Studio");
    addChildComponent(*studioPanel);
    addAndMakeVisible(studioTab);
    studioTab.getProperties().set("alloyTab", true);
    studioTab.onClick = [this] { toggleConsole(3); };
    studioPanel->dock.onClick = [this] { detachToolPanel(3, !studioWindow); };
    label(inspectorLabel, "SEGMENT INSPECTOR"); label(pluginLabel, "VST3 INSERTS / AUDIO 1"); label(masterLabel, "MASTER"); label(timeLabel, "00:00.000", 21); timeLabel.setColour(juce::Label::textColourId, colour::orange);
    slider(tempoSlider, 20, 400, 0.01, " BPM"); tempoSlider.setSliderStyle(juce::Slider::LinearBarVertical); tempoSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 96, 28);
    tempoSlider.textFromValueFunction = [](double value) { return wholeBpmText(value); };
    tempoSlider.setVelocityBasedMode(false); tempoSlider.setSliderSnapsToMousePosition(false); tempoSlider.setMouseDragSensitivity(3800); tempoSlider.setValue(120, juce::dontSendNotification);
    tempoSlider.setTooltip("Live BPM at the playhead during playback. When stopped, edits the selected marker. Drag up/down or double-click to type; during playback, adds a tempo target at the playhead.");
    keyDisplay.addItem("Note", 1); keyDisplay.addItem("Camelot", 2); keyDisplay.addItem("Note + Camelot", 3); keyDisplay.setSelectedId(3, juce::dontSendNotification);
    keyDisplay.setTooltip("Choose note names, numbered Camelot keys or both. Unknown means no key was found.");
    keyDisplay.onChange = [this] { if (!refreshing) { engine.project.keyDisplay = keyDisplay.getSelectedId() - 1; dirty = true; refresh(); timeline.repaint(); } };
    trackInfoButton.onClick = [this] { editAnalysis(selectedLibraryId); };
    const juce::StringArray transitions {"Cut", "4 bars", "8 bars", "16 bars", "32 bars", "Sections 8-64+ bars"};
    for (int i = 0; i < transitions.size(); ++i) transitionLength.addItem(transitions[i], i + 1);
    transitionLength.setSelectedId(transitionIndex + 1, juce::dontSendNotification);
    transitionLength.setTitle("New transition length");
    transitionLength.setTooltip("Match transitions to song sections, or choose a fixed overlap length. Section matches may need a longer overlap.");
    transitionLength.onChange = [this] { transitionIndex = transitionLength.getSelectedId() - 1; };
    transitionPreset.addItem("None", 1); transitionPreset.addItem("Bass swap", 2); transitionPreset.addItem("Crossfade", 3);
    transitionPreset.setSelectedId(3, juce::dontSendNotification); transitionPreset.setTitle("New transition preset");
    transitionPreset.setTooltip("Preset for Add Transition and Auto Arrange. None creates an editable frame without changing audio. Bass swap and Crossfade place editable nodes.");
    beatmatchButton.setButtonText("Auto Arrange"); beatmatchButton.setTitle("Auto Arrange timeline");
    beatmatchButton.setTooltip("Auto Arrange the songs already on the timeline in their current time order. Try section blends; use a crossfade when no suitable section transition fits. Undo restores the arrangement.");
    beatmatchButton.onClick = [this] { if (canAutoArrange()) prepareTempo(engine.project, true, true, 0, true); else setStatus("Pause playback, unlock transitions and wait for preparation before mixing the timeline."); };
    addAndMakeVisible(followButton); followButton.setClickingTogglesState(true);
    followButton.setTitle("Follow playhead");
    followButton.setTooltip("Keep the playhead in view during playback and seeking. Pan the timeline manually to turn Follow off. Saved for this workspace.");
    followButton.onClick = [this] { setFollowPlayhead(followButton.getToggleState()); };
    initialiseShortcuts();
    slider(masterSlider, -60, 12, 0.1, " dB"); masterSlider.setValue(-3, juce::dontSendNotification); masterSlider.setTooltip("Master output gain; lower this if the peak meter turns red.");
    masterSlider.setName("Master output gain"); masterSlider.setDoubleClickReturnValue(true, 0);
    addAndMakeVisible(peakReset);
    peakReset.setTooltip("Stereo sample peaks after master processing, before monitor mute. Values and lines hold for 1.2 seconds. Click to reset held peaks and clipping. Sparks are optional in View.");
    peakReset.onClick = [this] { peakDisplay.resetPeaks(); engine.masterClipped = false; peakReset.setButtonText("PEAK"); repaint(); };
    search.setTextToShowWhenEmpty("Search library...", colour::muted); search.onTextChange = [this] { filterLibrary(); libraryTable.table.updateContent(); libraryTable.table.repaint(); };
    const std::array<juce::String, 6> names { "Start", "Source in", "Length", "Fade in", "Fade out", "Segment gain" };
    for (int i = 0; i < 6; ++i)
    {
        addAndMakeVisible(clipSliders[(size_t) i]); addAndMakeVisible(clipLabels[(size_t) i]); label(clipLabels[(size_t) i], names[(size_t) i]);
        slider(clipSliders[(size_t) i], i == 5 ? -60 : 0, i == 5 ? 12 : 3600, i == 5 ? 0.1 : 0.001, i == 5 ? " dB" : " s");
        clipSliders[(size_t) i].setName(names[(size_t) i]);
        const char* tips[] {"Exact timeline start; turn Snap off for free numerical placement.", "Trim the source beginning without deleting the original audio. IN/OUT handles align transitions instead.", "Exact playback duration of this segment.", "Optional manual volume fade in. Adding a song does not create one.", "Optional manual volume fade out. Adding a song does not create one.", "Input gain before this segment's effects; useful for gain staging into a VST."};
        clipSliders[(size_t) i].setTooltip(tips[i]);
        if (i == 5) clipSliders[(size_t) i].configure("Segment gain");
        clipSliders[(size_t) i].onDragStart = [this] { remember(); };
        clipSliders[(size_t) i].onValueChange = [this, i] { if (!refreshing) editClip(i, clipSliders[(size_t) i].getValue()); };
        clipSliders[(size_t) i].onDragEnd = [this, i] { if (i == 1 || i == 2) scheduleTempoPreparation(); };
    }
    newButton.onClick = [this] { guardUnsaved([this] { newSession(); }); };
    openButton.onClick = [this] { guardUnsaved([this] { chooseOpen(); }); };
    saveButton.onClick = [this] { chooseSave({}, true); }; importButton.onClick = [this] { chooseImport(); }; exportButton.onClick = [this] { chooseExport(); }; settingsButton.onClick = [this] { settings(); };
    playButton.setColour(juce::TextButton::buttonColourId, colour::orange.darker(0.6f));
    playButton.onClick = [this] { engine.flushMidiControls(); engine.setPlaying(!engine.playing); refresh(); };
    stopButton.onClick = [this] { engine.flushMidiControls(); engine.stop(); refresh(); }; homeButton.onClick = [this] { engine.flushMidiControls(); engine.seek(engine.project.playbackStart()); };
    undoButton.onClick = [this] { undo(false); }; redoButton.onClick = [this] { undo(true); }; splitButton.onClick = [this] { splitClip(); }; duplicateButton.onClick = [this] { duplicateClip(); }; removeButton.onClick = [this] { removeClip(); };
    tempoSlider.onValueChange = [this] { if (!refreshing) { pendingTempo = tempoSlider.getValue(); tempoChangedAt = juce::Time::getMillisecondCounterHiRes(); } };
    masterSlider.onValueChange = [this] { if (!refreshing) { const juce::ScopedLock guard(engine.lock); engine.setControlValue({-1, AutomationTarget::master}, (float) masterSlider.getValue()); dirty = true; } };
    monitorMute.setTooltip("Silence speakers and headphones. Playback, meters, master automation and WAV export continue unchanged.");
    monitorMute.onClick = [this]
    {
        layout.monitorMuted = monitorMute.getToggleState(); engine.monitorMuted = layout.monitorMuted;
        workspaceDirty = true; workspaceChangedAt = juce::Time::getMillisecondCounterHiRes();
    };
    masterSlider.onDragStart = [this] { if (!engine.canWriteAutomation(-1)) remember(); };
    snapButton.setButtonText("Snap"); snapButton.setTooltip("Snap timeline edits to the Beatmatch / Snap grid selection: beat, half beat or quarter beat. Alt bypasses snapping. This switch is undoable and can also be controlled by MIDI.");
    snapButton.onClick = [this] { performAction({MidiActionKind::snap}); };
    gridDimmer.setRange(0, 1, 0.01);
    gridDimmer.setSliderStyle(juce::Slider::LinearHorizontal); gridDimmer.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    gridDimmer.setValue(layout.gridBrightness, juce::dontSendNotification);
    gridDimmer.setTitle("Grid brightness"); gridDimmer.setName("Timeline grid brightness");
    gridDimmer.setTooltip("Dim or brighten the timeline background grid. Double-click restores the subtle default. Wheel over the timeline to zoom; View / Fit whole mix shows the arrangement.");
    gridDimmer.setDoubleClickReturnValue(true, 0.45);
    gridDimmer.onValueChange = [this] { layout.gridBrightness = gridDimmer.getValue(); workspaceDirty = true; workspaceChangedAt = juce::Time::getMillisecondCounterHiRes(); timeline.repaint(); };
    addAndMakeVisible(autoArrangeButton);
    autoArrangeButton.setColour(juce::TextButton::buttonColourId, colour::orange.darker(.45f));
    autoArrangeButton.getProperties().set("alloyMolten", true); autoArrangeButton.getProperties().set("alloyActive", true);
    autoArrangeButton.setTooltip("Extend the current arrangement with more songs, or create a new mix by key and similar BPM. Choose songs or minutes, up to 3 hours. Undo restores the previous arrangement.");
    autoArrangeButton.onClick = [this] { showAutoArrangement(); };
    addPluginButton.onClick = [this] { choosePlugin(); };
    editPluginButton.onClick = [this] { showEditor(pluginList.list.getSelectedRow()); };
    pluginControlsButton.onClick = [this] { configurePluginControls(); };
    bypassPluginButton.onClick = [this]
    {
        auto& chain = engine.pluginChain(engine.selectedTrack.load()); const int i = pluginList.list.getSelectedRow();
        if (i < 0 || i >= (int) chain.size()) return;
        const juce::ScopedLock guard(engine.lock); chain[(size_t) i]->bypass = !chain[(size_t) i]->bypass; dirty = true; pluginList.list.repaint();
    };
    removePluginButton.onClick = [this]
    {
        auto& chain = engine.pluginChain(engine.selectedTrack.load()); const int i = pluginList.list.getSelectedRow();
        if (i < 0 || i >= (int) chain.size()) return;
        auto id = chain[(size_t) i]->id;
        editors.erase(std::remove_if(editors.begin(), editors.end(), [&](const auto& e) { return e->slotId == id; }), editors.end());
        const juce::ScopedLock guard(engine.lock);
        engine.removePlugin(engine.selectedTrack, i); dirty = true; refresh();
    };
    engine.onControl = [this](bool changed) { dirty = dirty || changed; if (!busy) refresh(); };
    engine.onSeekStatus = [this](const juce::String& text) { setStatus(text); };
    refreshControllerConnection();
    engine.onProjectSwitchBegin = [this] { if (!engine.capturingPerformance()) remember(); };
    engine.onAutomationWriteBegin = [this] { remember(); };
    engine.onMidiChanged = [this]
    {
        midiLabel.setText("MIDI: " + engine.lastMidiText(), juce::dontSendNotification);
        if (settingsWindow) if (auto* content = dynamic_cast<MidiSettings*>(settingsWindow->getContentComponent())) content->updateText();
    };
    if (openAudioDevice) { auto deviceError = engine.startDevice(); if (deviceError.isNotEmpty()) status = "Audio device: " + deviceError; }
    addChildComponent(importProgress);
    importProgress.cancel.onClick = [this]
    {
        if (exportTask) { *exportTask->cancelled = true; importProgress.requestStop(); }
        else if (importCancellation) { *importCancellation = true; importProgress.requestStop(); }
    };
    addAndMakeVisible(reverseButton); addAndMakeVisible(loopButton);
    reverseButton.onClick = [this]
    {
        if (busy || engine.exporting || !currentClip()) return;
        if (engine.project.clipTransitionLocked(selectedClip)) { setStatus("Unlock the transition before reversing its segment."); refresh(); return; }
        remember(); const juce::ScopedLock guard(engine.lock);
        scheduleTempoPreparation(); currentClip()->reversed = reverseButton.getToggleState(); engine.resetPlugins(); refresh();
    };
    loopButton.onClick = [this] { configureLoop(); };
    libraryTable.table.getHeader().setSortColumnId(1, true);
    libraryPreview.onChange = [this](const juce::String& message) { setStatus(message); libraryTable.table.repaint(); };
    libraryPreview.onProgress=[this]
    {
        const auto* view=libraryTable.table.getViewport();const int height=std::max(1,libraryTable.table.getRowHeight());
        const int first=view ? view->getViewPositionY()/height:0;
        const int end=std::min(libraryTable.getNumRows(),first+libraryTable.table.getHeight()/height+2);
        for(int row=first;row<end;++row)if(auto* cell=dynamic_cast<LibraryCell*>(libraryTable.table.getCellComponent(1,row)))cell->seek.updatePosition();
    };
    if (openAudioDevice)
    {
        const auto preference = readScanPreference(musicPreference);
        musicScanEnabled = preference.enabled; musicFolderPreference = preference.folder;
        if (musicFolderPreference != juce::File()) musicFolder = musicFolderPreference;
        if (preference.error.isNotEmpty()) status = preference.error;
        catalogueFile = libraryCatalogueFile(); catalogueRestorePending = true;
        workspacePreference = PcmStore::defaultRoot().getParentDirectory().getChildFile("workspace.json");
        firstWorkspace=!workspacePreference.existsAsFile();
        const auto loaded = readWorkspacePreferences(workspacePreference, layout); if (loaded.failed()) status = loaded.getErrorMessage();
        recoveryRoot = workspacePreference.getParentDirectory().getChildFile("recovery");
        juce::String recoveryError; recoveryStore = RecoveryStore::create(recoveryRoot, recoveryError);
        if (!recoveryStore) status = "Recovery unavailable: " + recoveryError;
        nextRecovery = juce::Time::getMillisecondCounterHiRes() + 30000;
    }
    followButton.setToggleState(layout.followPlayhead, juce::dontSendNotification);
    hints.setHintsEnabled(layout.popupHints);
    gridDimmer.setValue(layout.gridBrightness, juce::dontSendNotification);
    engine.monitorMuted = layout.monitorMuted; monitorMute.setToggleState(layout.monitorMuted, juce::dontSendNotification);
    initialiseWorkModes();
    if (performance) performance->setForgeEffects(forgeMotionAllowed());
    viewport.onViewChanged = [this]
    {
        const int x = viewport.getViewPositionX();
        if (x != lastTimelineViewX && !programmaticTimelineView) setFollowPlayhead(false);
        lastTimelineViewX = x;
        layoutTrackHeaders(); timeline.refreshCurveTools(); timeline.refreshAccessibleItems(); timelineRuler.refreshAccessibleMarkers(); tempoStrip.refreshAccessibleMarkers(); timelineRuler.repaint(); timelineOverview.repaint();
    };
    nextMusicScan = juce::Time::getMillisecondCounterHiRes() + 2000;
    initialiseAttract();
    initialiseEditing();
    setSize(1440, 940); showBrowserPage(0); showInspectorPage(1); restoreEditingWorkspace(); consoleCollapsed = true; layout.timeline = {}; resized(); refresh(); tickLibraryCatalogue(); startTimerHz(25);
    if(firstWorkspace) juce::MessageManager::callAsync([safe=juce::Component::SafePointer<Workstation>(this)] { if(safe)safe->showTutorial(); });
}
Workstation::~Workstation()
{
    tutorialWindow.reset();
    if (studioWindow) { studioWindow->clearContentComponent(); studioWindow->setLookAndFeel(nullptr); }
    studioWindow.reset(); studioPanel.reset(); studioWorkspace.reset();
    sceneLane.reset();
    libraryPreview.onChange = {}; libraryPreview.onProgress={}; libraryPreview.stop();
    hints.setHintsEnabled(false); hints.setLookAndFeel(nullptr);
    trackEditingWorkspace(); saveWorkspacePreferences();
    if (editingWindow) editingWindow->clearContentComponent();
    editingWindow.reset(); editingViewport.setViewedComponent(nullptr, false);
    masterWindow.reset();
    if (performanceWindow) performanceWindow->clearContentComponent();
    if (transitionWindow) transitionWindow->clearContentComponent();
    transitionWindow.reset(); transitionViewport.setViewedComponent(nullptr, false);
    performanceWindow.reset(); performanceViewport.setViewedComponent(nullptr, false);
    earWindow.reset(); gridWindow.reset();
    stopTimer(); engine.onControl = {}; engine.onMidiChanged = {}; engine.onSeekStatus = {}; engine.onAutomationWriteBegin = {}; engine.onPerformanceAction = {};
    if (stemJob) stemJob->finish();
    saveWorkspacePreferences();
    if (importCancellation) *importCancellation = true;
    *sourceReadsCancelled = true;
    if (exportTask) *exportTask->cancelled = true;
    stopMusicScan();
    cancelTimelineAudio();
    audioWorkers.removeAllJobs(true, -1);
    exportTask.reset(); engine.exporting = false;
    workers.removeAllJobs(true, -1);
    recoveryWorkers.removeAllJobs(true, -1);
    if (recoveryStore) { const auto result = recoveryStore->discard(); if (result.failed()) juce::Logger::writeToLog(result.getErrorMessage()); }
    flushLibraryCatalogue();
    settingsWindow.reset();  editors.clear(); viewport.setViewedComponent(nullptr, false); setLookAndFeel(nullptr);
}
void Workstation::timerCallback()
{
    if (engine.streamFailed.exchange(false)) setStatus("Playback stopped: an original music file changed or could not be read. Reload that track.");
    engine.servicePerformance(); engine.recordProjectCommit();
    workModeButton.setButtonText(engine.workMode==WorkMode::arrange ? "Arrange" : "Record/Perform");
    takeButton.setTooltip(engine.performanceStatus());
    if (engine.performanceError.isNotEmpty() && status!=engine.performanceError) setStatus(engine.performanceError);
    takeButton.setButtonText(engine.capturingPerformance() ? juce::String(engine.playing ? "REC " : "Paused ")+engine.performanceStatus().fromFirstOccurrenceOf("/",false,false).upToFirstOccurrenceOf("/",false,false).trim() : "Takes / preview");
    refreshStatusAccess();
    refreshControllerConnection();
    trackEditingWorkspace();
    advancePeakMeter();
    tickMasterEmblem();
    if (workspaceDirty && juce::Time::getMillisecondCounterHiRes() - workspaceChangedAt > 700) saveWorkspacePreferences();
    tickLibraryCatalogue(); tickMusicScan(); refreshMusicScanStatus(); tickRecovery();
    layoutTrackHeaders();
    engine.serviceScheduledPause();
    engine.serviceLiveHotcues();
    engine.flushMidiControls();
    engine.advanceAutomation(engine.position);
    if (toolPage == 2 || editingWindow) refreshAutomationEditor(false);
    if (engine.latencyBlocked && !busy)
    {
        const auto error = engine.pluginLatencyError();
        if (error.isNotEmpty()) { setStatus(error); pluginList.list.repaint(); } else engine.latencyBlocked = false;
    }
    if (performance) { performance->setForgeEffects(forgeMotionAllowed()); performance->refresh(!busy && !engine.exporting, true); }
    if (importProgress.isVisible()) { if (exportTask) importProgress.showExportProgress(exportTask->completed.load()); else importProgress.tick(); }
    if (!busy && timelineLoads.empty() && (engine.project.hasPendingPlacement() || afterTimelineAudio)) finishTimelineAudio();
    if (!busy && pendingTempo > 0 && !tempoSlider.isMouseButtonDown() && !AlloyLookAndFeel::editingSliderText(tempoSlider) && juce::Time::getMillisecondCounterHiRes() - tempoChangedAt > 450) syncMix(false);
    if (!busy && tempoPreparationPending && !juce::ModifierKeys::getCurrentModifiersRealtime().isAnyMouseButtonDown() && juce::Time::getMillisecondCounterHiRes() - tempoChangedAt > 450)
    { tempoPreparationPending = false; prepareTempo(engine.project, false, false); }
    if (pendingTempo <= 0 && !tempoSlider.isMouseButtonDown() && !AlloyLookAndFeel::editingSliderText(tempoSlider))
    {
        tempoSlider.setValue(displayedTempo(), juce::dontSendNotification);
    }
    followPlayheadIntoView();
    timeLabel.setText(timeText(engine.position - engine.project.tempo.origin), juce::dontSendNotification); playButton.setButtonText(engine.playing ? "Pause" : "Play");
    playButton.getProperties().set("alloyActive", engine.playing.load()); playButton.repaint();
    timeline.repaint(); timelineRuler.repaint(); timelineOverview.repaint(); tempoStrip.repaint(); for (auto& s : strips) s->repaint(); refreshAttractLights(); repaint();
}
void Workstation::advancePeakMeter()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    peakDisplay.advance(engine.outputPeaks.take(), peakDisplayTime == 0 ? .04 : (now - peakDisplayTime) / 1000,
                        forgeMotionAllowed());
    peakDisplayTime = now;
    peakReset.setButtonText(peakDisplay.clipped ? "CLIP / reset" : "PEAK");
    peakReset.setColour(juce::TextButton::textColourOffId, peakDisplay.clipped ? colour::error : colour::orange);
}
bool Workstation::forgeMotionAllowed() const
{
    return layout.forgeEffects && (layout.forgeMotion == 1 || (layout.forgeMotion == 0 && AlloyLookAndFeel::motionAllowed()));
}
Clip* Workstation::currentClip() { for (auto& c : engine.project.clips) if (c.id == selectedClip) return &c; return nullptr; }
std::unique_ptr<juce::AccessibilityHandler> Workstation::FeedbackText::createAccessibilityHandler()
{
    class Handler final : public juce::AccessibilityHandler
    {
    public:
        explicit Handler(FeedbackText& c) : AccessibilityHandler(c, juce::AccessibilityRole::staticText), text(c) {}
        juce::String getTitle() const override { return text.read ? text.read() : juce::String(); }
    private:
        FeedbackText& text;
    };
    return std::make_unique<Handler>(*this);
}
void Workstation::refreshStatusAccess()
{
    const auto text = statusAccess.read ? statusAccess.read() : juce::String();
    const double now = juce::Time::getMillisecondCounterHiRes();
    // Coalesce background progress; accessibility queries still read the latest state.
    if (text == lastStatusAccessText || now - lastStatusAccessAt < 750) return;
    lastStatusAccessText = text; lastStatusAccessAt = now;
    if (auto* handler = statusAccess.getAccessibilityHandler()) handler->notifyAccessibilityEvent(juce::AccessibilityEvent::titleChanged);
}
void Workstation::setStatus(const juce::String& s) { status = s; refreshStatusAccess(); repaint(); }
void Workstation::setBusy(bool value)
{
    if (value) { stopMusicScan(); engine.endTransitionRehearsal(); }
    busy = value;
    engine.controlInputEnabled = !value;
    if (performance) performance->refresh(!busy && !engine.exporting);
    for (int i = 0; i < getNumChildComponents(); ++i) getChildComponent(i)->setEnabled(!busy);
    editingPanel.setEnabled(!busy);
    for (auto* control : {&displayButton, &windowModeButton, &minimiseButton, &closeAppButton}) control->setEnabled(true);
    if (masterWindow && masterWindow->getContentComponent()) masterWindow->getContentComponent()->setEnabled(!busy);
    if (settingsWindow && settingsWindow->getContentComponent()) settingsWindow->getContentComponent()->setEnabled(!busy);
    for (auto& editor : editors) editor->setEnabled(!busy);
}
void Workstation::fail(const juce::String& error)
{
    getProperties().set("lastMessageDetails", error);
    const bool technical = error.containsIgnoreCase("onnx") || error.contains("std::") || error.containsIgnoreCase("tensor") || error.containsIgnoreCase("sha256");
    setStatus(technical ? "Could not finish this action. Open Tools > Last message details." : shortUiMessage(error, 30, 200));
}
void Workstation::remember()
{
    if (engine.capturingPerformance())
    {
        const auto base=(size_t)std::max(0,(int)getProperties().getWithDefault("liveUndoBase",0));
        undoHistory.push_back(editHistorySnapshot(engine));
        if(undoHistory.size()>base+100)undoHistory.erase(undoHistory.begin()+(ptrdiff_t)base);
        redoHistory.clear(); engine.performanceProjectPending=true; dirty=true; return;
    }
    engine.cancelPreparedLaunch();
    clearEditSelection();
    engine.endTransitionRehearsal();
    const juce::ScopedLock guard(engine.lock); undoHistory.push_back(engine.project); if (undoHistory.size() > 100) undoHistory.erase(undoHistory.begin()); redoHistory.clear(); dirty = true;
}
void Workstation::undo(bool redo)
{
    if (busy) return;
    if(engine.capturingPerformance())
    {
        const auto base=(size_t)std::max(0,(int)getProperties().getWithDefault("liveUndoBase",0));
        auto& source=redo ? redoHistory : undoHistory; auto& destination=redo ? undoHistory : redoHistory;
        if(source.empty() || (!redo && source.size()<=base))
        { setStatus(redo ? "No live edit to redo in this take." : "No live structural edit to undo. Stop the take before undoing arrangement history."); return; }
        engine.flushMidiControls(); engine.finishAutomation();
        auto previous=editHistorySnapshot(engine); auto candidate=std::move(source.back()); source.pop_back();
        // Keep continuous controls performed since the structural edit. Their
        // heard chronology is independent of this curve/placement checkpoint.
        const auto keepManual=[](TrackSettings& target,const TrackSettings& current)
        { target.gainDb=current.gainDb; target.pan=current.pan; target.eqDb=current.eqDb; target.filterCutoff=current.filterCutoff; target.pluginControls=current.pluginControls; };
        for(int lane=0;lane<trackCount;++lane)keepManual(candidate.tracks[(size_t)lane],previous.tracks[(size_t)lane]);
        for(auto& clip:candidate.clips)
            if(const auto* current=previous.clipFor(clip.id))keepManual(candidate.effectSettings(clip),previous.effectSettings(*current));
        { const juce::ScopedLock guard(engine.lock); candidate.performanceTakes.swap(engine.project.performanceTakes); std::swap(engine.project,candidate);
          engine.performanceProjectPending=true; }
        destination.push_back(std::move(previous)); dirty=true; refresh();
        setStatus(redo ? "Live edit redone at the current time. Earlier heard events are kept in the take."
            : "Live edit undone at the current time. Earlier heard events and subsequent continuous controls are kept.");
        return;
    }

    clearEditSelection();
    timeline.finishSectionGesture(true);
    engine.endTransitionRehearsal();
    afterTimelineAudio = {};
    pendingTempo = 0; tempoPreparationPending = false; tempoEditBaseline.reset(); tempoStrip.selectedTempoPoint = -1;
    engine.setAutomationRecordMode(AutomationRecordMode::read);
    auto& source = redo ? redoHistory : undoHistory; auto& dest = redo ? undoHistory : redoHistory; if (source.empty()) return;
    if (!persistSourceSectionHistory(source.back())) return;
    timeline.selectAutomationPoint({}, -1);
    {
    const juce::ScopedLock guard(engine.lock);
    // Plugin insert edits are outside arrangement undo; keep their current control ownership too.
    auto currentTracks = engine.project.tracks;
    std::map<juce::String, std::array<PluginControl, 8>> currentClipControls;
    for (const auto& clip : engine.project.clips) currentClipControls[effectsIdentity(clip.effects)] = engine.project.effectSettings(clip).pluginControls;
    const double previousOrigin = engine.project.tempo.origin;
    dest.push_back(engine.project); engine.project = std::move(source.back()); source.pop_back();
    if (previousOrigin != engine.project.tempo.origin)
        engine.commitSeek(engine.prepareSeek(std::max(0., engine.position.load() + engine.project.tempo.origin - previousOrigin)));
    for (int i = 0; i < trackCount; ++i)
    {
        auto& settings = engine.project.tracks[(size_t) i]; settings.pluginControls = currentTracks[(size_t) i].pluginControls;
        settings.pluginAutomation.erase(std::remove_if(settings.pluginAutomation.begin(), settings.pluginAutomation.end(), [&](const auto& lane)
        { return std::none_of(engine.plugins[(size_t) i].begin(), engine.plugins[(size_t) i].end(), [&](const auto& slot) { return slot->id == lane.slotId; }); }), settings.pluginAutomation.end());
    }
    for (auto& clip : engine.project.clips)
    {
        auto& settings = engine.project.effectSettings(clip);
        if (auto current = currentClipControls.find(effectsIdentity(clip.effects)); current != currentClipControls.end()) settings.pluginControls = current->second;
        settings.pluginAutomation.erase(std::remove_if(settings.pluginAutomation.begin(), settings.pluginAutomation.end(), [&](const auto& lane)
        { return std::none_of(clip.effects->plugins.begin(), clip.effects->plugins.end(), [&](const auto& slot) { return slot->id == lane.slotId; }); }), settings.pluginAutomation.end());
    }
    engine.restoreManualPluginValues(); engine.tempo = engine.project.bpm;
    }
    dirty = true; refresh();
    if (engine.project.hasPendingPlacement()) finishTimelineAudio();
}
void Workstation::filterLibrary()
{
    const auto selectedId = selectedLibraryId;
    juce::StringArray selectedIds; const auto oldRows = libraryTable.table.getSelectedRows();
    for (int i = 0; i < oldRows.size(); ++i) if (oldRows[i] >= 0 && oldRows[i] < (int) visibleMedia.size())
    {
        const int source = visibleMedia[(size_t) oldRows[i]];
        if (source >= 0 && source < (int) engine.library.size()) selectedIds.add(engine.library[(size_t) source]->id);
    }
    if (selectedIds.size() <= 1 || !selectedIds.contains(selectedId)) { selectedIds.clear(); if (selectedId.isNotEmpty()) selectedIds.add(selectedId); }
    visibleMedia.clear();
    for (int i = 0; i < (int) engine.library.size(); ++i)
    {
        const auto& m = *engine.library[(size_t) i];
        if (m.name.containsIgnoreCase(search.getText()) || m.assets.artist.containsIgnoreCase(search.getText()) || m.assets.album.containsIgnoreCase(search.getText()) || m.assets.genre.containsIgnoreCase(search.getText()) || m.file.getFileName().containsIgnoreCase(search.getText()) || keyNoteName(m.key()).containsIgnoreCase(search.getText()) || keyCompactName(m.key()).containsIgnoreCase(search.getText()) || keyCamelot(m.key()).containsIgnoreCase(search.getText()) || (m.bpm() > 0 && (wholeBpmText(m.bpm()).containsIgnoreCase(search.getText()) || juce::String(m.bpm(), 2).containsIgnoreCase(search.getText())))) visibleMedia.push_back(i);
    }
    const auto reference = std::find_if(engine.library.begin(),engine.library.end(),[&](const auto& media) { return media->id == librarySortReferenceId; });
    const Media* baseline = reference == engine.library.end() ? nullptr : reference->get();
    const bool relative = baseline && ((librarySortColumn == 4 && baseline->bpm() > 0)
        || (librarySortColumn == 3 && baseline->key() >= 0 && baseline->key() < 24));
    if (librarySortColumn >= 1)
        std::stable_sort(visibleMedia.begin(), visibleMedia.end(), [this,baseline,relative](int a, int b)
        {
            const auto& first = *engine.library[(size_t) a]; const auto& second = *engine.library[(size_t) b];
            if (relative)
            {
                if ((first.id == baseline->id) != (second.id == baseline->id)) return first.id == baseline->id;
                const auto distance = [&](const Media& media)
                {
                    if (librarySortColumn == 4) return media.bpm() > 0 ? std::abs(media.bpm() - baseline->bpm()) : -1.;
                    if (media.key() < 0 || media.key() >= 24) return -1.;
                    const int ring = std::abs(keyWheelNumber(media.key()) - keyWheelNumber(baseline->key()));
                    return (double) (std::min(ring,12-ring) + ((media.key() < 12) != (baseline->key() < 12) ? 1 : 0));
                };
                const double x = distance(first), y = distance(second);
                if ((x < 0) != (y < 0)) return x >= 0;
                if (x != y) return librarySortForward ? x < y : x > y;
                return first.libraryTitle().compareNatural(second.libraryTitle()) < 0;
            }
            if (librarySortColumn == 5)
            {
                const auto& x = first.assets.artworkGroup; const auto& y = second.assets.artworkGroup;
                if (x.isEmpty() != y.isEmpty()) return !x.isEmpty();
                const int group = x.compare(y); if (group != 0) return librarySortForward ? group < 0 : group > 0;
                return first.libraryTitle().compareNatural(second.libraryTitle()) < 0;
            }
            if (librarySortColumn == 8)
            {
                const auto& x = first.assets.genre; const auto& y = second.assets.genre;
                if (x.isEmpty() != y.isEmpty()) return !x.isEmpty();
                const int order = x.compareNatural(y);
                return librarySortForward ? order < 0 : order > 0;
            }
            if (librarySortColumn == 1) { const int order = first.libraryTitle().compareNatural(second.libraryTitle()); return librarySortForward ? order < 0 : order > 0; }
            if (librarySortColumn == 2) return librarySortForward ? first.duration() < second.duration() : first.duration() > second.duration();
            double x = librarySortColumn == 4 ? first.bpm() : first.key() < 0 || first.key() > 23 ? -1 : engine.project.keyDisplay == 0 ? first.key() : keyWheelNumber(first.key()) * 2 + (first.key() < 12 ? 1 : 0);
            double y = librarySortColumn == 4 ? second.bpm() : second.key() < 0 || second.key() > 23 ? -1 : engine.project.keyDisplay == 0 ? second.key() : keyWheelNumber(second.key()) * 2 + (second.key() < 12 ? 1 : 0);
            bool unknownX = librarySortColumn == 4 ? x <= 0 : x < 0, unknownY = librarySortColumn == 4 ? y <= 0 : y < 0;
            if (unknownX != unknownY) return !unknownX;
            return librarySortForward ? x < y : x > y;
        });
    juce::SparseSet<int> rows;
    for (int i = 0; i < (int) visibleMedia.size(); ++i) if (selectedIds.contains(engine.library[(size_t) visibleMedia[(size_t) i]]->id)) rows.addRange({i, i + 1});
    libraryTable.table.setSelectedRows(rows, juce::dontSendNotification);
    const auto total = juce::String((int) engine.library.size());
    libraryTab.setButtonText("Library (" + (search.getText().isNotEmpty() ? juce::String((int) visibleMedia.size()) + " / " : juce::String()) + total + ")");
    libraryTab.setTitle("Library / " + total + " tracks total / " + juce::String((int) visibleMedia.size()) + " matching search");
    libraryTab.setTooltip(libraryTab.getTitle() + ". All saved library entries, including unavailable sources. Scan totals count only additions during that pass. Music and its subfolders are scanned recursively.");
}
void Workstation::refreshMusicScanStatus()
{
    juce::String text, detail;
    int checked = musicChecked, unchanged = musicUnchanged;
    const auto counts = juce::String(musicAdded) + " added / " + juce::String(musicRefreshed) + " refreshed / " + juce::String(musicSaved) + " saved";
    if (musicScan)
    {
        const auto progress = musicScan->progress();
        checked = progress.checked; unchanged = progress.unchanged;
        const char* stage = (progress.stage == MediaReadStage::analyse || progress.stage == MediaReadStage::energy) ? "Analysing"
            : progress.stage == MediaReadStage::decode ? "Reading audio" : progress.stage == MediaReadStage::savedAnalysis ? "Reusing analysis" : "Checking";
        text = "Scanning Music / " + juce::String(checked) + " checked\n" + counts;
        detail = juce::String(stage) + ": " + progress.file.getFileName();
        if (engine.playing || engine.exporting || busy) text = "Scan paused / " + juce::String(checked) + " checked\n" + counts;
    }
    else if (catalogueOpening) text = "Opening saved Library...";
    else if (musicScanJobPending) text = "Music scan / checking folders...";
    else if (musicScanManual) text = "Scan waiting / stop playback and finish preparation";
    else if (musicScanSummary.isNotEmpty())
    {
        text = musicScanSummary + " / " + juce::String(checked) + " checked";
        if (musicFailures || musicProblems) text += " / " + juce::String(musicFailures + musicProblems) + " issues";
        text += "\n" + counts;
    }
    else if (!catalogueRefreshQueue.empty()) text = "Checking saved tracks / " + juce::String(catalogueRefreshQueue.size()) + " remaining";
    else text = musicScanEnabled ? (engine.playing ? "Music scan / waiting for playback to stop" : "Music scan / watching folder") : "Music scan / automatic scanning off";
    if (catalogueSaveError.isNotEmpty()) text = "Library save pending / will retry\n" + counts;
    musicScanNotice.setText(text, juce::dontSendNotification);
    musicScanNotice.setColour(juce::Label::textColourId, musicScan || musicScanJobPending || musicScanManual || catalogueSaveError.isNotEmpty() ? colour::orange : colour::muted);
    const auto description = text + (detail.isNotEmpty() ? "\n" + detail : juce::String()) + "\nFolder: " + musicFolder.getFullPathName()
        + "\n" + juce::String(unchanged) + " unchanged / " + juce::String(musicReused) + " reused analysis / " + juce::String(musicAnalysed) + " processed audio"
        + "\n" + juce::String(musicFailures) + " failed imports / " + juce::String(musicProblems) + " cache warnings"
        + "\n" + juce::String(catalogueRefreshQueue.size()) + " saved tracks awaiting a source check."
        + "\nSaved counts committed Library rows. Tools / Scan Music folder now retries failures. Music scan reports lists each failed file and cache warning."
        + (catalogueSaveError.isNotEmpty() ? "\nSave issue: " + catalogueSaveError : juce::String())
        + (musicLastIssue.isNotEmpty() ? "\nLast issue: " + musicLastIssue : juce::String());
    musicScanNotice.setTooltip(description); musicScanNotice.setTitle(description);
}
void Workstation::refresh()
{
    if (automationClip != selectedClip) automationClip.clear();
    engine.recordProjectCommit();
    refreshEditSelection();
    refreshMusicScanStatus();
    refreshStatusAccess();
    saveButton.setTooltip((sessionFile == juce::File() ? "Save this untitled mix; choose a file first." : "Save current mix to " + sessionFile.getFullPathName()) + juce::String(" Ctrl+S saves; Ctrl+Shift+S chooses Save As."));
    engine.prepareClipProcessing();
    if (std::none_of(engine.project.labels.begin(), engine.project.labels.end(), [&](const auto& item) { return item.id == timeline.selectedLabel; })) timeline.selectedLabel.clear();
    refreshing = true; refreshTransitionUI(); timelineRuler.refreshEditTools(); refreshAutomationEditor(); filterLibrary(); playlistTable.rebuildPlaylist(); libraryTable.table.updateContent(); pluginList.list.updateContent();
    libraryTable.table.repaint(); playlistTable.table.repaint(); pluginList.list.repaint();
    tempoSlider.setValue(pendingTempo > 0 ? pendingTempo : displayedTempo(), juce::dontSendNotification); masterSlider.setValue(engine.project.masterDb, juce::dontSendNotification); snapButton.setToggleState(engine.project.snap, juce::dontSendNotification);
    snapButton.setButtonText(engine.project.snapDivision == 4 ? "Snap 1/4" : engine.project.snapDivision == 2 ? "Snap 1/2" : "Snap 1");
    keyDisplay.setSelectedId(engine.project.keyDisplay + 1, juce::dontSendNotification);
    for (auto& strip : strips) strip->refresh();
    pluginLabel.setText("VST3 / " + (engine.controlClip(engine.selectedTrack) ? "Track / " + engine.controlClip(engine.selectedTrack)->media->name : "Lane " + juce::String(engine.selectedTrack.load() + 1) + " / " + engine.project.tracks[(size_t) engine.selectedTrack.load()].name).toUpperCase(), juce::dontSendNotification);
    pluginLabel.setTooltip(engine.controlClip(engine.selectedTrack) ? "Effects and effect automation are shared by every split piece of this track." : "Effects for this entire lane.");
    const auto* c = currentClip(); for (auto& s : clipSliders) s.setEnabled(c != nullptr && (!c->media->sourceDetailsPending || &s == &clipSliders[0] || &s == &clipSliders[5]));
    midiLabel.setText("MIDI / Lane " + juce::String(engine.selectedTrack.load() + 1) + " / " + engine.project.tracks[(size_t) engine.selectedTrack.load()].name + "\n" + engine.lastMidiText(), juce::dontSendNotification);
    clipDetail.setText(c ? c->media->libraryTitle() + " / Lane " + juce::String(c->track + 1)
        + (c->pitchSemitones != 0 ? " / pitch " + juce::String(c->pitchSemitones, 2) + " steps" : juce::String()) : "Select a segment", juce::dontSendNotification);
    if (c)
    {
        const double values[] { c->start - engine.project.tempo.origin, c->offset, c->length, c->fadeIn, c->fadeOut, c->gainDb };
        for (int i = 0; i < 6; ++i)
        {
            if (i < 5 && (!c->media->sourceDetailsPending || i == 0)) clipSliders[(size_t) i].setRange(i == 0 ? -engine.project.tempo.origin : 0, i == 0 ? std::max(3600.0, c->end() + 60) : i == 1 ? c->media->duration() : c->maxLength(), 0.001);
            if (!AlloyLookAndFeel::editingSliderText(clipSliders[(size_t) i])) clipSliders[(size_t) i].setValue(values[i], juce::dontSendNotification);
        }
    }
    const bool clipLocked = c && engine.project.clipTransitionLocked(c->id);
    const bool editingReady = !busy && !engine.exporting && pendingTempo <= 0 && !tempoPreparationPending;
    const bool copyReady = editingReady && c && c->media->hasPlayback() && !c->media->sourceDetailsPending && c->pendingPlacement == PendingPlacement::none;
    const bool scopedSelection = editSelection.multiple();
    inspectorLabel.setText(scopedSelection ? "SELECTED RANGE / GROUP" : "SELECTED SEGMENT", juce::dontSendNotification);
    editingCopy.setTooltip(scopedSelection ? "Copy the selected audio and its effects. Stop playback and recording first." : "Copy the selected segment, its automation and effects. Paste at the playhead on the selected lane.");
    editingCut.setTooltip(scopedSelection ? "Copy and remove the selected audio range/group, leaving a gap. One Undo restores the complete edit; original audio is kept." : "Cut the selected segment. Other positions stay fixed; Undo restores it.");
    editingSelect.setEnabled(!busy && !engine.exporting && !engine.project.clips.empty());
    editingCopy.setEnabled(copyReady); editingCut.setEnabled(copyReady && !clipLocked);
    const bool rangeEditReady = copyReady && !engine.playing && !engine.rehearsalActive() && !engine.recordingAutomation()
        && engine.automationRecordMode == AutomationRecordMode::read && !engine.project.hasPendingPlacement();
    editingSplit.setEnabled(rangeEditReady && !clipLocked && !scopedSelection);
    editingSplit.setTooltip(scopedSelection ? "Clear the range/group, click the ruler for the cut position, then Split at playhead (S)."
        : "Click the ruler for the cut position, then split this track at the playhead (S). Both pieces keep their audio.");
    editingDelete.setEnabled(rangeEditReady && !clipLocked);
    editingRepeat.setEnabled(rangeEditReady && !engine.project.hasLockedTransitions());
    editingCrop.setEnabled(rangeEditReady && editSelection.range.has_value() && !clipLocked);
    editingReplace.setEnabled(rangeEditReady && (editClipboard || clipClipboard) && !engine.project.hasLockedTransitions());
    editingRipple.setEnabled(copyReady && !clipLocked && !scopedSelection); editingCloseGap.setEnabled(copyReady && !clipLocked && !scopedSelection);
    editingPaste.setEnabled(editingReady && (editClipboard || (clipClipboard && (engine.project.followTrackTempo || !clipClipboard->tempoMap))) && !engine.project.hasLockedTransitions());
    editingPaste.setTooltip("Paste at " + timeText(engine.position.load() - engine.project.tempo.origin) + " on lane " + juce::String(engine.selectedTrack.load() + 1)
        + (editClipboard ? ". The copied group keeps its spacing." : ". Other segments keep their positions."));
    for (auto& slider : clipSliders) if (clipLocked) slider.setEnabled(false);
    tempoSlider.setEnabled(!engine.project.hasLockedTransitions());
    beatmatchButton.setEnabled(!engine.project.clips.empty() && !engine.project.hasLockedTransitions());
    const bool visualSelected = sceneLane && sceneLane->hasSelectedScene();
    splitButton.setEnabled(c != nullptr && !clipLocked && !c->media->sourceDetailsPending); duplicateButton.setEnabled(c != nullptr || visualSelected); removeButton.setEnabled((c != nullptr && !clipLocked) || visualSelected);
    reverseButton.setEnabled(c != nullptr && !clipLocked && !c->media->sourceDetailsPending); loopButton.setEnabled(c != nullptr && !clipLocked && !c->media->sourceDetailsPending); reverseButton.setToggleState(c && c->reversed, juce::dontSendNotification);
    if (scopedSelection)
    {
        splitButton.setEnabled(false); duplicateButton.setEnabled(false); reverseButton.setEnabled(false); loopButton.setEnabled(false);
        for (auto& slider : clipSliders) slider.setEnabled(false);
    }
    const auto explainEdit=[&](juce::Component& control,TimelineAction action,const juce::String& fallback)
    {
        auto reason=timelineActionUnavailable(action,engine.position,engine.selectedTrack);
        if(reason.isNotEmpty())control.setEnabled(false);
        if(!control.isEnabled() && reason.isEmpty())reason=fallback;
        control.getProperties().set("mixworksHintDisabled",reason);
        control.setDescription(reason);
    };
    explainEdit(editingCopy,TimelineAction::copy,"Select ready audio before copying.");
    explainEdit(editingCut,TimelineAction::cut,"Select unlocked, ready audio before cutting.");
    explainEdit(editingPaste,TimelineAction::paste,"Choose a compatible destination before pasting the copied audio.");
    explainEdit(editingDelete,TimelineAction::remove,"Stop playback and finish automation in Read before deleting selected audio.");
    explainEdit(editingSplit,TimelineAction::split,"Stop playback and finish automation in Read before splitting selected audio.");
    explainEdit(editingCrop,TimelineAction::cropSelection,"Drag over the waveform to select an audio range before cropping.");
    explainEdit(editingRepeat,TimelineAction::repeatSelection,"Unlock the selected transitions before repeating audio.");
    explainEdit(editingReplace,TimelineAction::replaceSelection,"Unlock transitions before replacing selected audio.");
    explainEdit(splitButton,TimelineAction::split,"Select an unlocked segment and place the cursor inside it.");
    if(!visualSelected)explainEdit(removeButton,TimelineAction::remove,"Select unlocked audio before deleting it.");
    const bool canUndo=!undoHistory.empty() && (!engine.capturingPerformance()
        || undoHistory.size()>(size_t)std::max(0,(int)getProperties().getWithDefault("liveUndoBase",0)));
    undoButton.setEnabled(canUndo); redoButton.setEnabled(!redoHistory.empty());
    undoButton.getProperties().set("mixworksHintDisabled",engine.capturingPerformance()
        ? "No live structural edit to undo. Stop the take before undoing arrangement history." : "There is no earlier edit to undo.");
    if(performance) { performance->getProperties().set("historyUndoReady",canUndo); performance->getProperties().set("historyRedoReady",!redoHistory.empty()); }
    playButton.setButtonText(engine.playing ? "Pause" : "Play"); playButton.setEnabled(!busy && (engine.playing || engine.playbackReady()));
    playButton.getProperties().set("alloyActive", engine.playing.load()); refreshing = false;
    if (performance) performance->refresh(!busy && !engine.exporting);
    timeline.updateSize();
    followPlayheadIntoView();
    repaint();
}
void Workstation::selectTrack(int track)
{
    engine.flushMidiControls();
    if (track != engine.selectedTrack)
    {
        engine.silenceNotes(); engine.selectedTrack = track;
    }
    refresh();
}
void Workstation::chooseImport()
{
    chooser = std::make_unique<juce::FileChooser>("Import audio", juce::File::getSpecialLocation(juce::File::userMusicDirectory), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3", false);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems, [safe = juce::Component::SafePointer<Workstation>(this)](const auto& f) { if (safe) safe->importFiles(f.getResults()); });
}
void Workstation::importFiles(const juce::Array<juce::File>& files)
{
    if (busy || files.isEmpty()) return;
    if (std::any_of(files.begin(), files.end(), [](const auto& file) { return file.hasFileExtension("m3u;m3u8"); }))
    {
        setBusy(true); setStatus("Reading playlist paths before importing audio...");
        auto cancelled = std::make_shared<std::atomic<bool>>(false); importCancellation = cancelled;
        importProgress.begin(files.size());
        auto safe = juce::Component::SafePointer<Workstation>(this);
        workers.addJob([safe, files, cancelled]
        {
            juce::Array<juce::File> sources; juce::String error;
            for (const auto& file : files)
            {
                if (*cancelled) break;
                juce::MessageManager::callAsync([safe, cancelled, name = file.getFileName()] { if (safe && safe->importCancellation == cancelled) safe->importProgress.showFile(name, MediaReadStage::playlist); });
                if (file.hasFileExtension("m3u;m3u8"))
                {
                    const auto parsed = readPlaylist(file, [cancelled] { return cancelled->load(); });
                    if (parsed.error.isNotEmpty()) { error = parsed.error; break; }
                    sources.addArray(parsed.files);
                }
                else sources.add(file);
                juce::MessageManager::callAsync([safe, cancelled] { if (safe && safe->importCancellation == cancelled) safe->importProgress.finishFile(); });
            }
            juce::MessageManager::callAsync([safe, cancelled, sources, error]
            {
                if (!safe || safe->importCancellation != cancelled) return;
                safe->importProgress.setVisible(false); safe->importCancellation.reset(); safe->setBusy(false);
                if (*cancelled) { safe->setStatus("Playlist import cancelled. The current library and mix were kept."); return; }
                if (error.isNotEmpty()) { safe->fail(error + "\nNo files from this selection were imported."); return; }
                safe->importFiles(sources);
            });
        });
        return;
    }
    setBusy(true); setStatus("Decoding and analysing BPM, beat phase and musical key...");
    // Decode off the audio and message threads; commit each completed owned sample buffer on the message thread.
    auto safe = juce::Component::SafePointer<Workstation>(this);
    juce::Array<juce::File> pending;
    for (const auto& file : files)
    {
        bool duplicate = false; for (const auto& existing : engine.library) if (existing->file == file && existing->playbackError.isEmpty()) duplicate = true;
        if (!duplicate && !pending.contains(file)) pending.add(file);
    }
    if (pending.isEmpty()) { setBusy(false); setStatus("These songs are already in your library."); return; }
    auto cancelled = std::make_shared<std::atomic<bool>>(false); importCancellation = cancelled;
    importProgress.begin(pending.size());
    workers.addJob([safe, pending, cancelled]() mutable
    {
        juce::AudioFormatManager formats; formats.registerBasicFormats();
        juce::StringArray failures; int imported = 0, processed = 0, reused = 0, unsavedRecords = 0, decodedReused = 0;
        for (const auto& f : pending)
        {
            if (*cancelled) break;
            auto r = readMedia(formats, f, [safe, cancelled, name = f.getFileName()](MediaReadStage phase)
            {
                juce::MessageManager::callAsync([safe, cancelled, name, phase]
                {
                    if (safe && safe->importCancellation == cancelled) safe->importProgress.showFile(name, phase);
                });
            }, AnalysisCacheMode::readWrite, MediaReadPurpose::catalogue, nullptr, cancelled);
            if (r.media) { ++imported; if (r.media->analysisFromCache) ++reused; if (r.media->decodedFromCache) ++decodedReused; if (r.media->analysisCacheWarning.isNotEmpty() || r.media->playbackCacheWarning.isNotEmpty()) ++unsavedRecords; }
            else failures.add(r.error);
            ++processed;
            juce::MessageManager::callAsync([safe, cancelled, media = std::move(r.media)]() mutable
            {
                if (!safe || safe->importCancellation != cancelled) return;
                if (media)
                {
                    const auto existing = std::find_if(safe->engine.library.begin(), safe->engine.library.end(), [&](const auto& m) { return m->file == media->file; });
                    if (existing == safe->engine.library.end()) { safe->engine.library.push_back(media); safe->markCatalogueChanged(media); }
                    else if (!(*existing)->playbackError.isEmpty()) { *existing = media; safe->markCatalogueChanged(media); } // Existing clips retain their original source identity.
                }
                safe->filterLibrary(); safe->libraryTable.table.updateContent(); safe->libraryTable.table.repaint();
                safe->importProgress.finishFile();
            });
        }
        auto errors = failures.isEmpty() ? juce::String() : juce::String(failures.size()) + " file(s) could not be imported:\n" + juce::StringArray(failures.begin(), std::min(6, failures.size())).joinIntoString("\n");
        if (failures.size() > 6) errors += "\nAnd " + juce::String(failures.size() - 6) + " more. Import smaller selections to inspect individual failures.";
        const bool stopped = processed < pending.size();
        juce::MessageManager::callAsync([safe, cancelled, imported, reused, decodedReused, unsavedRecords, stopped, errors]() mutable
        {
            if (!safe || safe->importCancellation != cancelled) return;
            safe->importProgress.setVisible(false); safe->importCancellation.reset();
            safe->setBusy(false); safe->refresh(); safe->setStatus(juce::String(stopped ? "Import stopped. " : "Import complete. ") + "Added " + juce::String(imported) + " songs; reused " + juce::String(reused) + " analyses / " + juce::String(decodedReused) + " decoded audio caches. " + (unsavedRecords > 0 ? juce::String(unsavedRecords) + " cache warnings; see BPM / Key." : "Double-click a song to add it to the timeline."));
            if (errors.isNotEmpty()) safe->fail(errors);
        });
    });
}
void Workstation::editClip(int field, double value)
{
    const juce::ScopedLock guard(engine.lock); auto* c = currentClip(); if (!c || busy) return;
    if (engine.project.clipTransitionLocked(c->id)) { setStatus("Unlock the transition before editing its segment."); refresh(); return; }
    if (c->media->sourceDetailsPending && field != 0 && field != 5) return;
    if (field == 1 || field == 2) scheduleTempoPreparation();
    if (field == 0)
    {
        const auto result = engine.project.moveClip(c->id, value + engine.project.tempo.origin, engine.project.snap);
        if (result.failed()) { setStatus(result.getErrorMessage()); return; }
        dirty = true; refresh(); return;
    }
    if (field == 1) { c->offset = juce::jlimit(0.0, std::max(0.0, c->media->duration() - (c->loopSpan > 0 ? c->loopSpan : 0.001 * c->rate)), value); if (c->loopSpan == 0) c->length = std::min(c->length, (c->media->duration() - c->offset) / c->rate); }
    if (field == 2) *c = c->slice(0, juce::jlimit(std::min(0.001, c->maxLength()), c->maxLength(), value));
    if (field == 3) c->fadeIn = std::min(value, c->length);
    if (field == 4) c->fadeOut = std::min(value, c->length);
    if (field == 5) c->gainDb = (float) value;
    dirty = true; playlistTable.table.repaint(); timeline.repaint();
}
void Workstation::splitClip()
{
    if(const auto reason=timelineActionUnavailable(TimelineAction::split,engine.position,engine.selectedTrack);reason.isNotEmpty())
    { setStatus(reason); return; }
    if (editSelection.multiple()) { setStatus("Clear the range/group to split one segment."); return; }
    auto* c = currentClip(); const double at = engine.position; if (!c || c->media->sourceDetailsPending || at <= c->start || at >= c->end() || busy) return;
    if (engine.project.clipTransitionLocked(c->id)) { setStatus("Unlock the transition before splitting its segment."); return; }
    const juce::ScopedLock guard(engine.lock); const Clip original = *c;
    const double leftLength = at - c->start; Clip right = original.slice(leftLength, original.length - leftLength); right.id = juce::Uuid().toString();
    auto candidate = engine.project; candidate.ensureTransitionLinks();
    const auto index = (size_t) (c - engine.project.clips.data());
    candidate.clips[index] = original.slice(0, leftLength);
    candidate.clips.insert(candidate.clips.begin() + (ptrdiff_t) index + 1, right);
    for (auto& link : candidate.transitionLinks)
    {
        const bool outOwned = link.outgoing == original.id, inOwned = link.incoming == original.id;
        if (link.outgoing == original.id && original.transitionTime(false) >= leftLength) link.outgoing = right.id;
        if (outOwned && link.outgoing == right.id && link.frame) for (auto& value : *link.frame) value -= original.sourceElapsed(leftLength);
        if (link.incoming == original.id && original.transitionTime(true) >= leftLength) link.incoming = right.id;
        for (int side = 0; side < 2 && link.bassSwap; ++side) if (side == 0 ? outOwned : inOwned)
        {
            const bool shifted = (side == 0 ? link.outgoing : link.incoming) == right.id;
            const double sourceBegin = shifted ? original.sourceElapsed(leftLength) : 0;
            const double sourceEnd = shifted ? original.sourceElapsed(original.length) : original.sourceElapsed(leftLength);
            auto& p = *link.bassSwap;
            if (p[(size_t) side * 2] < sourceBegin || p[(size_t) side * 2 + 1] > sourceEnd) link.bassSwap.reset();
            else { p[(size_t) side * 2] -= sourceBegin; p[(size_t) side * 2 + 1] -= sourceBegin; }
        }
    }
    const auto valid = candidate.validateTransitionLinks(); if (valid.failed()) { setStatus(valid.getErrorMessage()); return; }
    remember(); engine.project = std::move(candidate); selectedClip = right.id;
    refresh();
}
void Workstation::duplicateClip()
{
    if (sceneLane && sceneLane->hasSelectedScene()) { sceneLane->duplicateSelected(); return; }
    if (editSelection.multiple()) { setStatus("Use Copy and Paste for a selected range/group."); return; }
    const auto* selected = currentClip(); if (!selected || busy) return;
    if (engine.project.hasLockedTransitions()) { setStatus("Unlock transitions before duplicating segments with new tempo markers."); return; }
    if (pendingTempo > 0 || tempoPreparationPending || engine.project.hasPendingPlacement() || selected->media->sourceDetailsPending)
    { setStatus("Wait for preparation before duplicating this segment."); return; }
    const juce::ScopedLock guard(engine.lock); const Clip original = *selected;
    Clip copy = original; copy.id = juce::Uuid().toString();
    if (const auto error = engine.cloneClipEffects(copy); error.isNotEmpty()) { setStatus("Duplicate kept the current mix: " + error); return; }
    auto candidate = engine.project;
    copy.moveTo(candidate.snappedClipStart(copy, original.end(), candidate.snap));
    if (candidate.followTrackTempo && !candidate.tempo.points.empty())
    {
        const double at = copy.start + copy.transitionTime(true);
        const auto next = std::upper_bound(candidate.tempo.points.begin(), candidate.tempo.points.end(), at, [](double t, const auto& point) { return t < point.time; });
        candidate.tempo.setTrackPoint(copy.id, at, candidate.tempo.bpmAt(at), next == candidate.tempo.points.end() || next->ramp);
    }
    candidate.clips.push_back(copy);
    const auto valid = candidate.validateTransitionLinks(); if (valid.failed()) { setStatus(valid.getErrorMessage()); return; }
    remember(); engine.project = std::move(candidate); selectedClip = copy.id; refresh();
}

bool removeMissingAutomaticTempo(Project& project, const Project& before)
{
    auto& points = project.tempo.points; const auto count = points.size();
    for (auto& point : points) if (point.clipId.isNotEmpty() && !project.clipFor(point.clipId))
        if (const auto* removed = before.clipFor(point.clipId))
            for (const auto& survivor : project.clips) if (survivor.effects == removed->effects)
            { point.clipId = survivor.id; break; }
    points.erase(std::remove_if(points.begin(),points.end(),[&](const auto& point)
    { return point.clipId.isNotEmpty() && !project.clipFor(point.clipId); }),points.end());
    if (project.clips.empty() && points.empty()) project.tempo.origin = 0;
    return points.size() != count;
}
void Workstation::removeClip()
{
    if (sceneLane && sceneLane->hasSelectedScene()) { sceneLane->removeSelected(); return; }
    if(const auto reason=timelineActionUnavailable(TimelineAction::remove,engine.position,engine.selectedTrack);reason.isNotEmpty())
    { setStatus(reason); return; }
    if (performSelectionAction(TimelineAction::remove, engine.position, engine.selectedTrack)) return;
    if (!currentClip() || busy || engine.exporting) return;
    if (pendingTempo > 0 || tempoPreparationPending || engine.project.hasPendingPlacement())
    { setStatus("Wait for track and tempo preparation before removing a segment."); return; }
    if (engine.project.clipTransitionLocked(selectedClip)) { setStatus("Unlock the transition before removing its segment."); return; }
    auto candidate = engine.project; candidate.clearDeletedTrackTransitions({selectedClip});
    auto& clips = candidate.clips; clips.erase(std::remove_if(clips.begin(),clips.end(),[this](const auto& c) { return c.id == selectedClip; }),clips.end());
    const auto links = candidate.validateTransitionLinks();
    if (links.failed()) { setStatus(links.getErrorMessage()); return; }
    const bool changedTempo = removeMissingAutomaticTempo(candidate, engine.project);
    const auto protectedResult = engine.project.validateTransitionLocks(candidate);
    if (protectedResult.failed()) { setStatus(protectedResult.getErrorMessage()); return; }
    if (changedTempo && !candidate.clips.empty() && candidate.followTrackTempo)
    {
        prepareTempo(std::move(candidate),false,true); return;
    }
    remember(); const juce::ScopedLock guard(engine.lock);
    engine.project = std::move(candidate); selectedClip.clear(); tempoStrip.selectedTempoPoint = -1; refresh();
}
void Workstation::guardUnsaved(std::function<void()> action)
{
    if (unsavedPromptOpen || choosingSave) return;
    engine.flushMidiControls();
    if (!dirty && pendingTempo <= 0 && !busy) { action(); return; }
    unsavedPromptOpen = true;
    juce::AlertWindow::showYesNoCancelBox(juce::MessageBoxIconType::QuestionIcon, "Unsaved mix", "Save changes to this mix before continuing?",
        "Save", "Discard", "Cancel", this, juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<Workstation>(this), action](int result)
        {
            if (!safe) return;
            safe->unsavedPromptOpen = false;
            if (result == 2) action();
            else if (result == 1) { safe->engine.setPlaying(false); safe->chooseSave(action, true); }
        }));
}
void Workstation::requestClose(std::function<void()> close) { guardUnsaved([this, close = std::move(close)] { clearRecovery(); close(); }); }
void Workstation::newSession()
{
    getProperties().set("tempoDocumentGeneration",(juce::int64)getProperties()["tempoDocumentGeneration"]+1);
    if (importCancellation) *importCancellation = true;
    importCancellation.reset(); importProgress.setVisible(false);
    pendingTempo = 0; tempoPreparationPending = false; tempoEditBaseline.reset(); afterTempoPreparation = {};
    tempoStrip.selectedTempoPoint = -1; setBusy(false);
    timeline.selectAutomationPoint({}, -1);
    selectedAutomationTarget = -1; selectedStem = -1; pluginLane = {}; automationClip.clear();
    timeline.visibleHotcueClips.clear(); timeline.visibleSectionClips.clear();
    timeline.selectedHotcue.reset(); timeline.selectedSection.reset();
    timeline.hotcueGesture.reset(); timeline.sectionGesture.reset();
    clearEditSelection();
    clearRecovery();
    cancelTimelineAudio();
    stopMusicScan(); musicAttempts.clear();
    engine.setAutomationRecordMode(AutomationRecordMode::read); engine.automationArmed.fill(false); engine.masterAutomationArmed = false;
    if (engine.workMode==WorkMode::recordPerform) { engine.automationArmed.fill(true); engine.masterAutomationArmed=true; engine.automationRecordMode=AutomationRecordMode::write; }
    engine.stop(); editors.clear(); const juce::ScopedLock guard(engine.lock);
    
    engine.masterClipped = false; engine.masterPeak = engine.masterInputPeak = 0;
    engine.project = Project(); engine.position = 0; engine.tempo = 120; for (auto& p : engine.plugins) p.clear(); undoHistory.clear(); redoHistory.clear(); selectedClip.clear(); sessionFile = {}; dirty = false; refresh();
    for (auto& media : engine.library) media = media->catalogueCopy();
    catalogueRefreshQueue = engine.library; // Cloned entries supersede any in-flight catalogue worker's old pointers.
    tempoStrip.selectedTempoPoint = -1; pendingTempo = 0; tempoPreparationPending = false; tempoEditBaseline.reset(); afterTempoPreparation = {};
}
juce::String Workstation::keyText(const Media& media) const
{
    const int key = media.key(); if (key < 0 || key > 23) return "Unknown";
    auto text = engine.project.keyDisplay == 0 ? keyCompactName(key) : engine.project.keyDisplay == 1 ? keyCamelot(key) : keyCamelot(key) + " / " + keyCompactName(key);
    return text;
}
int Workstation::keyEvidenceLevel(const Media& media)
{
    if (media.manualKey >= 0 || media.key() < 0 || media.key() > 23) return 0;
    const double score = media.analysis.keyConfidence;
    if (!std::isfinite(score) || score < 0 || score > 1) return 0;
    // Product ordinal, not detector confidence or calibrated accuracy. The raw
    // score multiplies profile margin, winning-passage fraction and agreement;
    // the old .55/.80 bands left almost every retained result in one category.
    return score >= .30 ? 4 : score >= .15 ? 3 : score >= .05 ? 2 : 1;
}
juce::String Workstation::keyEvidenceText(const Media& media)
{
    if (media.key() < 0 || media.key() > 23) return "Unknown";
    if (media.manualKey >= 0) return "Manual";
    // The detector already separates an accepted key from a tentative keyGuess.
    // Its multiplicative stability score is not a probability or a review verdict.
    return media.analysis.key >= 0 && media.analysis.key < 24 && std::isfinite(media.analysis.keyConfidence) && media.analysis.keyConfidence >= 0
        && media.analysis.keyConfidence <= 1 ? "Detected" : "Review";
}
juce::String Workstation::keyEvidenceHint(const Media& media)
{
    if (media.manualKey >= 0) return "Your chosen key: " + keyNoteName(media.manualKey) + ".";
    if (media.key() < 0 || media.key() > 23) return "Key not found. Review it in Source corrections.";
    if (keyEvidenceText(media) == "Detected")
        return "Detected key: " + keyNoteName(media.key()) + ". Found by the analyser; you can check or change it in Key by ear.";
    return "Suggested key: " + keyNoteName(media.key()) + ". The result is tentative or its evidence is unavailable; check it in Key by ear.";
}
bool Workstation::libraryNeedsReview(const Media& media)
{
    return gridReviewRank(media) < 4 || (keyEvidenceText(media) != "Manual" && keyEvidenceText(media) != "Detected");
}
juce::String Workstation::libraryReviewHint(const Media& media)
{
    return juce::String(libraryNeedsReview(media) ? "Subdued: grid or key needs review. This track is still available to select, preview and add. " : "Grid and key available. ")
        + gridReviewHint(media) + " " + keyEvidenceHint(media);
}
void Workstation::syncMix(bool align)
{
    if (busy) return;
    if (engine.project.hasLockedTransitions()) { pendingTempo = 0; setStatus("Unlock transitions before changing tempo or rebuilding the mix."); refresh(); return; }
    auto candidate = engine.project;
    if (!align && pendingTempo > 0)
    {
        const double target = pendingTempo; pendingTempo = 0;
        if (candidate.tempo.points.empty()) candidate.tempo.setPoint(0, candidate.bpm);
        const int selected = engine.playing ? -1 : tempoStrip.selectedTempoPoint;
        if (selected >= 0 && selected < (int) candidate.tempo.points.size()) candidate.tempo.movePoint(selected, candidate.tempo.points[(size_t) selected].time, target);
        else tempoStrip.selectedTempoPoint = candidate.tempo.setPoint(engine.position, target);
    }
    pendingTempo = 0; tempoPreparationPending = false;
    prepareTempo(std::move(candidate), align);
}
void Workstation::scheduleTempoPreparation()
{
    if (engine.project.followTrackTempo && !engine.project.tempo.points.empty()) { if (!tempoEditBaseline) tempoEditBaseline = engine.project; tempoPreparationPending = true; tempoChangedAt = juce::Time::getMillisecondCounterHiRes(); }
}
bool Workstation::ensureTempoReady(std::function<void()> action)
{
    if (engine.project.hasPendingPlacement())
    {
        afterTimelineAudio = std::move(action); finishTimelineAudio(); return false;
    }
    if (!engine.playbackReady())
    {
        if (!timelineLoads.empty()) { afterTimelineAudio = std::move(action); setStatus("Waiting for background audio preparation..."); }
        else setStatus("A source is unavailable. Reimport it or remove its segment before continuing.");
        return false;
    }
    if (pendingTempo <= 0 && !tempoPreparationPending) return true;
    afterTempoPreparation = std::move(action);
    if (pendingTempo > 0) syncMix(false);
    else { tempoPreparationPending = false; prepareTempo(engine.project, false, false); }
    return false;
}
void Workstation::prepareTempo(Project source, bool align, bool addUndo, size_t alignFrom, bool ordered)
{
    if (busy) return;
    if (engine.project.hasLockedTransitions()) { pendingTempo = 0; tempoPreparationPending = false; tempoEditBaseline.reset(); setStatus("Unlock transitions before changing tempo or rebuilding the mix."); refresh(); return; }
    if (!engine.sourceAudioReady())
    {
        ensureTempoReady([safe = juce::Component::SafePointer<Workstation>(this), align, addUndo, alignFrom, ordered]
        { if (safe) safe->prepareTempo(safe->engine.project, align, addUndo, alignFrom, ordered); }); return;
    }
    engine.flushMidiControls(); engine.setAutomationRecordMode(AutomationRecordMode::read);
    const bool resume = engine.playing; engine.setPlaying(false);
    TempoMap previous = engine.project.gridClock();
    const double positionBeat = previous.beatsAt(engine.position);
    auto previousMap = std::make_shared<const TempoMap>(previous);
    auto rollback = std::move(tempoEditBaseline); tempoEditBaseline.reset(); tempoPreparationPending = false;
    for (auto& clip : source.clips) if (clip.beatmatched && !clip.tempoMap) clip.tempoMap = previousMap;
    std::set<const PreparedAudio*> seen; int64_t used = 0;
    auto count = [&](const Project& p) { for (const auto& c : p.clips) { if (c.prepared && seen.insert(c.prepared.get()).second) used += c.prepared->residentBytes(); for (const auto& stem : c.stemPrepared) if (stem && seen.insert(stem.get()).second) used += stem->residentBytes(); } };
    count(engine.project); if (rollback) count(*rollback); for (const auto& p : undoHistory) count(p); for (const auto& p : redoHistory) count(p);
    const double beats = selectedTransitionPreset() == TransitionPreset::alignOnly ? 0 : transitionOverlapBeats();
    const auto preset = selectedTransitionPreset();
    const bool sectionTransitions = transitionIndex == 5;
    setBusy(true); setStatus("Preparing track tempo, transitions and beat grid...");
    const auto documentGeneration = (juce::int64)getProperties()["tempoDocumentGeneration"];
    workers.addJob([safe = juce::Component::SafePointer<Workstation>(this), documentGeneration, source = std::move(source), rollback = std::move(rollback), align, addUndo, alignFrom, beats, used, resume, positionBeat, preset, sectionTransitions, ordered]() mutable
    {
        auto result = ordered ? prepareOrderedTimeline(std::move(source), used) : prepareTempoMap(std::move(source), align, beats, used, alignFrom, {}, preset, sectionTransitions);
        juce::MessageManager::callAsync([safe, documentGeneration, result = std::move(result), rollback = std::move(rollback), align, addUndo, resume, positionBeat]() mutable
        {
            if (!safe || (juce::int64)safe->getProperties()["tempoDocumentGeneration"] != documentGeneration) return;
            safe->setBusy(false);
            if (result.error.isNotEmpty())
            {
                const juce::ScopedLock guard(safe->engine.lock);
                if (rollback) safe->engine.project = std::move(*rollback);
                for (auto& clip : safe->engine.project.clips) if (clip.pendingPlacement != PendingPlacement::none) clip.pendingPlacement = PendingPlacement::failed;
                safe->afterTempoPreparation = {}; safe->engine.setPlaying(resume); safe->refresh(); safe->fail(result.error); return;
            }
            if (addUndo) safe->remember();
            { const juce::ScopedLock guard(safe->engine.lock); safe->engine.project = std::move(result.project); safe->engine.tempo = safe->engine.project.tempo.bpmAt(0);
                safe->engine.position = align ? safe->engine.project.playbackStart() : safe->engine.project.tempo.timeAtBeat(positionBeat); safe->engine.resetPlugins(); }
            safe->engine.setPlaying(resume); safe->dirty = true; safe->refresh();
            safe->setStatus("Mix ready. " + juce::String(result.skipped) + " songs need a timing check.");
            auto action = std::move(safe->afterTempoPreparation); safe->afterTempoPreparation = {}; if (action) action();
        });
    });
}
void Workstation::setDownbeatAnchor(const juce::String& clipId, double localTime)
{
    if (engine.project.clipTransitionLocked(clipId)) { setStatus("Unlock the transition before changing its source grid."); return; }
    if (busy) return;
    std::shared_ptr<Media> media; double source = 0;
    for (const auto& clip : engine.project.clips) if (clip.id == clipId)
    { media = clip.media; source = clip.sourceTimeAt(juce::jlimit(0.0, clip.length, localTime)); break; }
    if (!media) return;
    auto correction = SourceCorrection::from(*media); correction.downbeat = source;
    const auto applied = applySourceCorrection(media, correction);
    if (applied.failed()) { fail(applied.getErrorMessage()); return; }
    setStatus("Downbeat saved at " + timeText(source) + ".");
}
juce::Result Workstation::applySourceCorrection(const std::shared_ptr<Media>& media, const SourceCorrection& correction, bool rebuildTimeline)
{
    if (busy || engine.exporting || !timelineLoads.empty() || pendingTempo > 0 || tempoPreparationPending)
        return juce::Result::fail("Wait for source or tempo preparation before applying a correction.");
    if (rebuildTimeline && engine.project.hasLockedTransitions())
        return juce::Result::fail("Unlock all transitions before saving and rebuilding the timeline. Save source only is still available.");
    if (!media) return juce::Result::fail("The selected source is no longer available.");
    const auto previous = SourceCorrection::from(*media);
    const auto result = engine.correctSource(media, correction); if (result.failed()) return result;
    if (previous.downbeat != correction.downbeat)
    {
        const auto reference = writeDownbeatReference(media->file,media->analysisRecord(),previous.downbeat);
        if (reference.failed()) { engine.correctSource(media,previous); return reference; }
    }
    // Do not dismiss the draft or rebuild from facts that failed to reach durable storage.
    saveAnalysisCorrection(media);
    if (media->analysisCacheWarning.isNotEmpty())
    {
        engine.correctSource(media, previous);
        return juce::Result::fail("Corrections were not saved. " + media->analysisCacheWarning
            + (previous.downbeat != correction.downbeat ? " The reviewed choice remains in the comparison archive." : juce::String()));
    }
    clearRetryError(*media, false, this);
    const auto stamp = musicFileStamp(media->file);
    for (const auto& row : engine.library)
        if (row->file == media->file && row->analysisSource == media->analysisSource)
        {
            row->analysisFileBytes = stamp.analysisBytes; row->analysisFileModified = stamp.analysisModified;
            row->analysisCacheWarning.clear(); markCatalogueChanged(row);
        }
    dirty = true; refresh();
    if (rebuildTimeline) syncMix(true);
    else setStatus("Track timing saved.");
    return juce::Result::ok();
}
void Workstation::saveAnalysisCorrection(const std::shared_ptr<Media>& media)
{
    media->analysisCacheWarning = writeAnalysisRecord(media->file, media->analysisRecord());
}
juce::String Workstation::analysisSelectionId()
{
    if (correctionFromLibrary) return selectedLibraryId;
    const auto* clip = currentClip(); return clip && clip->media ? clip->media->id : juce::String();
}
void Workstation::editAnalysis(std::optional<juce::String> sourceId)
{
    if (gridWindow && gridWindow->isVisible()) { gridWindow->toFront(true); return; }
    if (busy || !timelineLoads.empty() || pendingTempo > 0 || tempoPreparationPending)
    { setStatus("Wait for audio and tempo preparation before opening source grid correction."); return; }
    auto reviewSources = engine.library; for (const auto& c : engine.project.clips) reviewSources.push_back(c.media);
    const auto reviewFailures = retryIssues(reviewSources, getProperties()["focusedRetryRows"]);
    // Context actions use the selected clip; global commands pass their active source explicitly.
    if (!sourceId) { const auto* clip = currentClip(); sourceId = clip && clip->media ? clip->media->id : juce::String(); }
    std::shared_ptr<Media> media;
    if (sourceId->isNotEmpty())
    {
        for (const auto& item : engine.library) if (item->id == *sourceId) { media = item; break; }
        if (!media) for (const auto& clip : engine.project.clips)
            if (clip.media && clip.media->id == *sourceId) { media = clip.media; break; }
    }
    if ((!media || media->duration() <= 0) && !reviewFailures.empty())
    {
        class FailureWindow final : public AlloyPopupWindow<juce::DialogWindow>
        {
        public:
            FailureWindow() : AlloyPopupWindow("Source corrections / retry failures", colour::background, true) {}
            void closeButtonPressed() override { setVisible(false); exitModalState(0); }
        };
        auto* panel = new RetryFailurePanel(reviewFailures);
        panel->onOpen = [safe = juce::Component::SafePointer<Workstation>(this), target = juce::Component::SafePointer<RetryFailurePanel>(panel)](juce::String id)
        {
            juce::MessageManager::callAsync([safe, target, id]
            {
                if (!safe || !target || !safe->gridWindow || safe->gridWindow->getContentComponent() != target.getComponent()) return;
                safe->gridWindow->closeButtonPressed(); safe->editAnalysis(id);
            });
        };
        gridWindow = std::make_unique<FailureWindow>();
        gridWindow->setUsingNativeTitleBar(false); gridWindow->setContentOwned(panel, true); gridWindow->setLookAndFeel(&look);
        gridWindow->centreWithSize(panel->getWidth(), panel->getHeight()); gridWindow->setVisible(true); gridWindow->enterModalState(true, nullptr, false); return;
    }
    if (!media) { setStatus("Select a track to correct its BPM, beat one or key."); return; }
    const auto& a = media->analysis;
    juce::String details = media->name + "\nDetected " + preciseBpmText(a.bpm) + " BPM (evidence score " + juce::String(a.tempoConfidence * 100, 0) + "/100). Beat phase " + juce::String(a.beatOrigin, 3) + " s.";
    details += media->manualBeatOrigin >= 0 ? "\nManual downbeat anchor: " + juce::String(media->manualBeatOrigin, 4) + " s (4/4; may be anywhere in the source)."
        : media->downbeatOrigin() >= 0 ? "\nInferred downbeat anchor: " + juce::String(a.downbeatOrigin, 4) + " s (4/4; review its bar phase)."
        : "\nDownbeat unknown. Right-click a waveform at beat one to set it; automatic sync aligns pulses only.";
    details += "\nKey: " + keyNoteName(a.key) + " / " + keyCamelot(a.key) + " (evidence score " + juce::String(a.keyConfidence * 100, 0) + "/100; not a probability). Alternate: " + keyNoteName(a.alternativeKey) + " / " + keyCamelot(a.alternativeKey);
    details += "\nTuning " + juce::String(a.tuningCents, 1) + " cents; examined " + juce::String(a.examinedSeconds, 1) + " of " + juce::String(media->duration(), 1) + " seconds.";
    if (a.bpm > 0) details += " Grid deviation " + juce::String(a.gridErrorMs, 1) + " ms RMS.";
    details += a.variableTempo ? "\nTempo varies or windows disagree: review the grid before syncing." : "\nConstant-tempo grid estimate. Half/double BPM ambiguities can be corrected below.";
    details += "\nLeave overrides blank to use detection. Confidence is a heuristic, not a calibrated probability.";
    details += "\nAnalysis record: " + analysisRecordFile(media->file).getFullPathName();
    if (a.retryEvidence.isObject()) details+="\nTargeted retry: "+a.retryEvidence["tempoDecision"].toString()+"; "+a.retryEvidence["keyDecision"].toString()+". Before/candidate values, passages and method are retained in the analysis record.";
    details += media->analysisCacheWarning.isNotEmpty() ? "\n" + media->analysisCacheWarning : media->analysisFromCache ? "\nReused saved analysis for unchanged audio." : "\nAnalysis is saved in Mixworks' local application data folder.";
    if (media->playbackError.isNotEmpty()) details += "\nAudio loading: " + media->playbackError;
    details += "\nPlayback reads the original music file through a small shared buffer; no full-song audio copy is stored.";
    if (media->playbackCacheWarning.isNotEmpty()) details += "\n" + media->playbackCacheWarning;
    if (!std::isfinite(media->duration()) || media->duration() <= 0) { setStatus("Wait until this source's duration is known."); return; }
    Engine* auditionEngine=&engine;
    if (engine.workMode==WorkMode::recordPerform)
    {
        libraryPreview.stop(); privatePreparation.prepareSourceAudition(); auditionEngine=privatePreparation.privateEngine();
        const auto route=libraryPreview.playPrepared(privatePreparation,"private-grid-review");
        if (route.failed()) details+="\nPrivate playback unavailable: "+route.getErrorMessage();
    }
    else engine.setPlaying(false);
    auto* editor = new GridCorrectionEditor(*auditionEngine, media, details,
        [safe = juce::Component::SafePointer<Workstation>(this), media](const SourceCorrection& correction, bool rebuildTimeline)
        { return safe ? safe->applySourceCorrection(media, correction, rebuildTimeline) : juce::Result::fail("The mix was closed."); });
    editor->refreshFailures = [safe = juce::Component::SafePointer<Workstation>(this), grid = juce::Component::SafePointer<GridCorrectionEditor>(editor)]
    {
        if (!safe || !grid) return;
        auto sources = safe->engine.library; for (const auto& c : safe->engine.project.clips) sources.push_back(c.media);
        grid->setRetryFailures(retryIssues(sources, safe->getProperties()["focusedRetryRows"]), lastRetryResult(safe->getProperties()["focusedLastResult"]),
            [safe, grid](juce::String id)
            {
                juce::MessageManager::callAsync([safe, grid, id]
                {
                    if (!safe || !grid || !safe->gridWindow || safe->gridWindow->getContentComponent() != grid.getComponent()) return;
                    safe->gridWindow->closeButtonPressed(); safe->editAnalysis(id);
                });
            });
    };
    editor->refreshFailures();
    editor->setKeyByEarAction([safe = juce::Component::SafePointer<Workstation>(this), media, target = juce::Component::SafePointer<GridCorrectionEditor>(editor)](auto playback)
    { if (safe && target) safe->showKeyByEar(media, std::move(playback), target); });
    editor->beforeAudition = [safe = juce::Component::SafePointer<Workstation>(this)]
    {
        if (!safe) return juce::Result::fail("The mix was closed.");
        if (safe->engine.workMode == WorkMode::recordPerform) return safe->libraryPreview.playPrepared(safe->privatePreparation, "private-grid-review");
        safe->libraryPreview.stop(); return juce::Result::ok();
    };
    gridWindow = std::make_unique<GridCorrectionWindow>(editor); gridWindow->setLookAndFeel(&look);
    gridWindow->setVisible(true); gridWindow->enterModalState(true, nullptr, false);
    setStatus("Review track timing.");
    if (!media->hasPlayback())
    {
        const auto record = media->analysisRecord(); const auto file = media->file;
        audioWorkers.addJob([safe = juce::Component::SafePointer<GridCorrectionEditor>(editor), owner = juce::Component::SafePointer<Workstation>(this), record, file, cancelled = sourceReadsCancelled]
        {
            juce::AudioFormatManager formats; formats.registerBasicFormats();
            auto result = readMedia(formats, file, {}, AnalysisCacheMode::readOnly, MediaReadPurpose::playback, &record, cancelled);
            if (result.media && !(result.media->analysisSource == record.source))
            { result.media.reset(); result.error = "The source changed. Reimport it before editing its grid."; }
            juce::MessageManager::callAsync([safe, owner, media = std::move(result.media), error = result.error]
            {
                if (safe)
                {
                    if (media && error.isEmpty()) clearRetryError(*media, true, owner.getComponent());
                    safe->setPlaybackMedia(media, error); if (safe->refreshFailures) safe->refreshFailures();
                }
            });
        });
    }
}
bool Workstation::saveSessionFile(const juce::File& file)
{
    const auto error = engine.saveProject(file);
    if (error.isNotEmpty()) { fail(error); return false; }
    clearRecovery(); sessionFile = file; dirty = false; saveButton.setTooltip("Save current mix to " + file.getFullPathName() + ". Ctrl+S saves; Ctrl+Shift+S chooses Save As."); setStatus("Saved " + file.getFullPathName()); return true;
}
void Workstation::confirmChangedDestination(const juce::File& chosen, const juce::File& destination, std::function<void(bool)> after)
{
    // The chooser confirms only the typed path, before our extension correction.
    if (chosen == destination || !destination.existsAsFile()) { after(true); return; }
    auto* dialog = new juce::AlertWindow("Replace existing file?",
        "The completed filename already exists:\n" + destination.getFullPathName()
        + "\n\nReplace it? Its previous contents will be kept in a backup.", juce::MessageBoxIconType::WarningIcon, this);
    dialog->addButton("Replace", 1); dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    dialog->enterModalState(true, juce::ModalCallbackFunction::create(
        [safe = juce::Component::SafePointer<Workstation>(this), after](int answer) { if (safe) after(answer == 1); }), true);
}
juce::File Workstation::defaultSaveFolder() const
{
    return layout.saveFolder.isEmpty()
        ? juce::File::getSpecialLocation(juce::File::userDesktopDirectory).getChildFile("Mixworks")
        : juce::File(layout.saveFolder);
}
juce::File Workstation::defaultSaveLocation(const juce::String& filename)
{
    const auto folder = defaultSaveFolder();
    const auto result = folder.createDirectory();
    if (result.failed())
    {
        setStatus("Cannot use default save folder: " + folder.getFullPathName()
            + ". Choose another in File / Default save folder. " + result.getErrorMessage());
        return {};
    }
    return folder.getChildFile(filename);
}
bool Workstation::setDefaultSaveFolder(const juce::File& folder)
{
    if (folder != juce::File() && !folder.isDirectory())
    { setStatus("Choose an existing folder for the default save location."); return false; }
    auto updated = layout; updated.saveFolder = folder == juce::File() ? juce::String() : folder.getFullPathName();
    if (workspacePreference == juce::File())
    { setStatus("Cannot save the default folder: workspace preferences are unavailable."); return false; }
    const auto result = writeWorkspacePreferences(workspacePreference, updated);
    if (result.failed()) { setStatus("Default save folder was not changed. " + result.getErrorMessage()); return false; }
    layout.saveFolder = updated.saveFolder;
    setStatus("Default save folder: " + defaultSaveFolder().getFullPathName());
    return true;
}
void Workstation::chooseDefaultSaveFolder()
{
    if (choosingSave) { setStatus("Finish choosing the current save file before changing its default folder."); return; }
    chooser = std::make_unique<juce::FileChooser>("Choose default save folder", defaultSaveFolder(), "", false);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [safe = juce::Component::SafePointer<Workstation>(this)](const auto& f)
        { if (safe && f.getResult() != juce::File()) safe->setDefaultSaveFolder(f.getResult()); });
}
void Workstation::chooseSave(std::function<void()> onSaved, bool useSessionFile)
{
    if (choosingSave) return;
    if (busy || engine.exporting) { setStatus("Wait for the current task or export to finish, then save or close again. Your mix is still open."); return; }
    if (!ensureTempoReady([safe = juce::Component::SafePointer<Workstation>(this), onSaved, useSessionFile] { if (safe) safe->chooseSave(onSaved, useSessionFile); })) return;
    if (useSessionFile && sessionFile != juce::File()) { if (saveSessionFile(sessionFile) && onSaved) onSaved(); return; }
    const auto initial = defaultSaveLocation(sessionFile == juce::File() ? "Session.mixworks" : sessionFile.getFileName());
    if (initial == juce::File()) return;
    choosingSave = true;
    chooser = std::make_unique<juce::FileChooser>(useSessionFile ? "Save mix" : "Save mix as", initial, "*.mixworks", false);
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting, [safe = juce::Component::SafePointer<Workstation>(this), onSaved](const auto& f)
    {
        if (!safe) return;
        if (f.getResult() == juce::File()) { safe->choosingSave = false; return; }
        const auto destination = f.getResult().withFileExtension("mixworks");
        safe->confirmChangedDestination(f.getResult(), destination, [safe, destination, onSaved](bool confirmed)
        {
            if (!safe) return;
            safe->choosingSave = false;
            if (!confirmed) return;
            if (safe->busy || safe->engine.exporting) { safe->setStatus("Wait for the current task to finish, then save again. Your mix is still open."); return; }
            if (safe->saveSessionFile(destination) && onSaved)
                juce::MessageManager::callAsync([safe, onSaved] { if (safe) onSaved(); });
        });
    });
}
void Workstation::chooseOpen()
{
    chooser = std::make_unique<juce::FileChooser>("Open mix", sessionFile == juce::File() ? defaultSaveFolder() : sessionFile, "*.mixworks", false);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe = juce::Component::SafePointer<Workstation>(this)](const auto& f)
    {
        if (!safe || f.getResult() == juce::File()) return;
        safe->openProjectFile(f.getResult());
    });
}
void Workstation::openProjectFile(const juce::File& file) { openProjectDocument({}, file); }
void Workstation::openProjectDocument(juce::String document, const juce::File& file, bool recovered,
                                      std::shared_ptr<RecoveryStore> recovery, juce::String fingerprint, bool relinked)
{
    if (busy) return;
    setBusy(true); engine.playing = false; setStatus("Opening arrangement sources...");
    auto cancelled = std::make_shared<std::atomic<bool>>(false); importCancellation = cancelled;
    importProgress.begin(0); importProgress.setOpeningSession();
    auto safe = juce::Component::SafePointer<Workstation>(this);
    audioWorkers.addJob([safe, file, cancelled, document = std::move(document), recovered, recovery, fingerprint, relinked]
    {
        if (!safe) return;
        auto staged = safe->engine.prepareProject(document.isEmpty() ? file.loadFileAsString() : document, [safe, cancelled](int done, int total, const juce::String& name, MediaReadStage phase)
        {
            juce::MessageManager::callAsync([safe, cancelled, done, total, name, phase]
            {
                if (!safe || safe->importCancellation != cancelled) return;
                safe->importProgress.setCounts(done, total); safe->importProgress.showFile(name, phase);
            });
        }, cancelled);
        juce::MessageManager::callAsync([safe, file, cancelled, recovered, recovery, fingerprint, relinked, staged = std::make_shared<PreparedProject>(std::move(staged))]() mutable
        {
            if (!safe || safe->importCancellation != cancelled) return;
            safe->importProgress.setVisible(false); safe->importCancellation.reset();
            if (*cancelled) staged->error = "Session opening cancelled. The current session was kept.";
            if (staged->failedSource && !*cancelled)
            {
                const auto source = *staged->failedSource; safe->setStatus("Find the missing audio to open this mix.");
                safe->getProperties().set("lastMessageDetails", staged->error);
                juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::WarningIcon, "Audio unavailable", "Find " + source.name + " to open this mix. Your current mix stays open until then.",
                    "Locate source...", "Cancel", safe.getComponent(), juce::ModalCallbackFunction::create(
                    [safe, file, recovered, recovery, fingerprint, staged, source](int answer)
                    {
                        if (!safe) return;
                        if (answer == 0) { safe->setBusy(false); safe->setStatus("Opening cancelled. The current session was kept."); return; }
                        safe->chooser = std::make_unique<juce::FileChooser>("Locate " + source.name, juce::File(source.path), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3", false);
                        safe->chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                            [safe, file, recovered, recovery, fingerprint, staged, source](const auto& picker)
                            {
                                if (!safe) return;
                                safe->setBusy(false);
                                if (picker.getResult() == juce::File()) { safe->setStatus("Locating cancelled. The current session was kept."); return; }
                                auto retry = relinkProjectSource(staged->parsed, source.id, picker.getResult());
                                if (retry.isEmpty()) { safe->fail("The saved source could not be located. The current session was kept."); return; }
                                safe->openProjectDocument(std::move(retry), file, recovered, recovery, fingerprint, true);
                            });
                    }));
                return;
            }
            // Editors must be destroyed before their processors can be replaced.
            if (staged->error.isEmpty()) { safe->editors.clear();  }
            auto previousLibrary = safe->engine.library;
            auto error = safe->engine.commitProject(std::move(*staged));
            safe->setBusy(false);
            if (error.isNotEmpty()) { safe->refresh(); safe->fail(error); }
            else
            {
                safe->clearRecovery();
                safe->cancelTimelineAudio();
                safe->catalogueRefreshQueue.clear();
                for (auto& media : safe->engine.library) if (media->sessionMetadataPending)
                {
                    // Keep already loaded artwork/analysis while the session owns its ID and manual values.
                    const auto previous = std::find_if(previousLibrary.begin(), previousLibrary.end(), [&](const auto& source) { return source->file == media->file; });
                    if (previous != previousLibrary.end() && !(*previous)->analysisSource.sha256.isEmpty())
                    {
                        auto display = (*previous)->catalogueCopy(); display->id = media->id; display->name = media->name;
                        display->manualBpm = media->manualBpm; display->manualBeatOrigin = media->manualBeatOrigin; display->manualKey = media->manualKey;
                        display->sourceDetailsPending = display->sessionMetadataPending = true; media = std::move(display);
                    }
                    safe->catalogueRefreshQueue.push_back(media);
                }
                for (const auto& media : previousLibrary)
                    if (std::none_of(safe->engine.library.begin(), safe->engine.library.end(), [&](const auto& source) { return source->file == media->file; }))
                    {
                        auto copy = media->catalogueCopy();
                        if (std::any_of(safe->engine.library.begin(), safe->engine.library.end(), [&](const auto& source) { return source->id == copy->id; })) copy->id = juce::Uuid().toString();
                        safe->engine.library.push_back(copy); safe->catalogueRefreshQueue.push_back(copy);
                    }
                for (const auto& media : safe->engine.library)
                    if (std::none_of(previousLibrary.begin(), previousLibrary.end(), [&](const auto& old) { return old->file == media->file; }))
                        safe->markCatalogueChanged(media);
                safe->timeline.selectAutomationPoint({}, -1);
                safe->sessionFile = file; safe->musicAttempts.clear(); safe->undoHistory.clear(); safe->redoHistory.clear(); safe->selectedClip.clear(); safe->tempoStrip.selectedTempoPoint = -1; safe->pendingTempo = 0; safe->tempoPreparationPending = false; safe->dirty = recovered || relinked; safe->refresh();
                int reused = 0, decoded = 0, prepared = 0, unsaved = 0;
                for (const auto& media : safe->engine.library) { if (media->analysisFromCache) ++reused; if (media->decodedFromCache) ++decoded; if (media->analysisCacheWarning.isNotEmpty() || media->playbackCacheWarning.isNotEmpty()) ++unsaved; }
                for (const auto& clip : safe->engine.project.clips) if (clip.prepared) { if (clip.prepared->fromDiskCache) ++prepared; if (clip.prepared->cacheWarning.isNotEmpty()) ++unsaved; }
                safe->setStatus("Opened " + file.getFileName() + (unsaved > 0 ? " / Some files could not be cached" : "")
                    + (relinked ? " / Save to keep the new file location" : ""));
                if (recovered)
                {
                    safe->setStatus("Recovered unsaved mix. Save the session to keep these changes.");
                    // Keep the leased source snapshot until the recovered document has
                    // been saved or durably snapshotted under this window's ownership.
                    safe->restoredRecovery = recovery; safe->restoredRecoveryFingerprint = fingerprint;
                    safe->nextRecovery = 0;
                }

            }
        });
    });
}
void Workstation::chooseExport(int take)
{
    if (busy || exportTask || engine.exporting) return;
    if (engine.capturingPerformance()) { setStatus("Stop to finalise the editable take before exporting."); return; }
    if (take < -1 || take >= (int)engine.project.performanceTakes.size()) { setStatus("That take is no longer available. Reopen Takes / preview."); return; }
    const auto safe = juce::Component::SafePointer<Workstation>(this);
    const auto id = take >= 0 ? engine.project.performanceTakes[(size_t)take].id : juce::String();
    const auto fingerprint = take >= 0 ? juce::SHA256(juce::JSON::toString(takeToJson(engine.project.performanceTakes[(size_t)take])).toUTF8()).toHexString() : juce::String();
    // Bind both asynchronous boundaries to the chosen take, rather than its
    // position in a list that Undo, editing or discard can replace.
    const auto target = [safe, id, fingerprint]() -> int
    {
        if (!safe) return -2;
        if (id.isEmpty()) return -1;
        const auto& takes = safe->engine.project.performanceTakes;
        const auto found = std::find_if(takes.begin(), takes.end(), [&](const auto& item) { return item.id == id; });
        if (found == takes.end() || juce::SHA256(juce::JSON::toString(takeToJson(*found)).toUTF8()).toHexString() != fingerprint)
        { safe->setStatus("That take changed. Reopen its export chooser."); return -2; }
        return (int)(found - takes.begin());
    };
    if (!ensureTempoReady([safe, target]
    {
        if (!safe || safe->busy || safe->exportTask || safe->engine.exporting || safe->engine.capturingPerformance()) return;
        const int current = target(); if (current >= -1) safe->chooseExport(current);
    })) return;
    const auto initial = defaultSaveLocation("Mix.wav"); if (initial == juce::File()) return;
    chooser = std::make_unique<juce::FileChooser>("Export stereo 24-bit WAV", initial, "*.wav", false);
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting, [safe, target](const auto& f)
    {
        if (!safe || safe->busy || safe->exportTask || safe->engine.exporting || safe->engine.capturingPerformance() || f.getResult() == juce::File()) return;
        const auto destination = f.getResult().withFileExtension("wav");
        safe->confirmChangedDestination(f.getResult(), destination, [safe, target, destination](bool confirmed)
        {
            if (!safe || !confirmed || safe->busy || safe->exportTask || safe->engine.exporting || safe->engine.capturingPerformance()) return;
            const int current = target(); if (current < -1) return;
            // Resolve the stable take again after the replacement confirmation.
            safe->selectedExportTake = current;
            if (current >= 0) safe->beginWavExport(destination,{});
            else safe->chooseExportEnd(destination);
        });
    });
}
void Workstation::choosePlugin()
{
    if (busy) return;
    juce::Array<juce::File> files; findPlugins(juce::File("C:\\Program Files\\Common Files\\VST3"), files);
    files.sort(); juce::PopupMenu menu; for (int i = 0; i < files.size(); ++i) menu.addItem(i + 1, files[i].getFileNameWithoutExtension());
    menu.addSeparator(); menu.addItem(files.size() + 1, "Choose another VST3...");
    const auto effectOwner = selectedClip;
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&addPluginButton), [safe = juce::Component::SafePointer<Workstation>(this), files, effectOwner](int id)
    {
        if (!safe || id <= 0 || safe->selectedClip != effectOwner) return;
        auto load = [safe, effectOwner](const juce::File& file)
        {
            if (!safe || file == juce::File() || safe->selectedClip != effectOwner) return; safe->setStatus("Loading " + file.getFileName() + "...");
            const int track = safe->engine.selectedTrack; auto error = safe->engine.addPlugin(track, file);
            if (error.isNotEmpty()) safe->fail(error); else { safe->dirty = true; safe->refresh(); const auto latency = safe->engine.pluginLatencyError(); safe->setStatus(latency.isNotEmpty() ? latency : "Loaded " + file.getFileName()); safe->showEditor((int) safe->engine.pluginChain(track).size() - 1); }
        };
        if (id <= files.size()) load(files[id - 1]);
        else
        {
            safe->chooser = std::make_unique<juce::FileChooser>("Choose an effect", juce::File("C:\\Program Files\\Common Files\\VST3"), "*.vst3", false);
            safe->chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectDirectories, [load](const auto& f) { load(f.getResult()); });
        }
    });
}
void Workstation::configureLoop()
{
    auto* clip = currentClip(); if (busy || engine.exporting || !clip) return;
    if (engine.project.clipTransitionLocked(clip->id)) { setStatus("Unlock the transition before looping its segment."); return; }
    const auto id = clip->id;
    const auto before = juce::JSON::toString(projectToJson(engine.project));
    auto* dialog = new juce::AlertWindow("Loop section", "Repeat from the segment's start point.", juce::MessageBoxIconType::NoIcon, this);
    dialog->addComboBox("beats", {"1 beat", "2 beats", "4 beats", "8 beats", "16 beats", "32 beats"}, "Section length"); dialog->getComboBoxComponent("beats")->setSelectedId(3);
    dialog->addComboBox("repeats", {"2 times", "4 times", "8 times", "16 times", "32 times"}, "Repeat section"); dialog->getComboBoxComponent("repeats")->setSelectedId(2);
    dialog->addButton("Set loop", 1, juce::KeyPress(juce::KeyPress::returnKey)); if (clip->loopSpan > 0) dialog->addButton("Remove loop", 2);
    dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    dialog->enterModalState(true, juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<Workstation>(this), dialog, id, before](int answer)
    {
        if (!safe || answer == 0 || safe->busy || safe->engine.exporting || safe->engine.project.clipTransitionLocked(id) || !safe->currentClip() || safe->currentClip()->id != id) return;
        if (before != juce::JSON::toString(projectToJson(safe->engine.project))) { safe->setStatus("The segment changed while Loop was open. Select it again."); return; }
        auto* selected = safe->currentClip();
        const double sourceBpm = selected->media->bpm() > 0 ? selected->media->bpm() : safe->engine.project.bpm / selected->rate;
        const double span = std::pow(2.0, dialog->getComboBoxComponent("beats")->getSelectedId() - 1) * 60 / sourceBpm;
        if (answer == 1 && selected->offset + span > selected->media->duration()) { safe->fail("The selected section exceeds the source audio. Move Source in earlier or choose fewer beats."); return; }
        safe->remember(); const juce::ScopedLock guard(safe->engine.lock); safe->scheduleTempoPreparation();
        const double sourceLength = answer == 2 ? selected->loopSpan : span * std::pow(2.0, dialog->getComboBoxComponent("repeats")->getSelectedId());
        const double duration = selected->localTimeForSource(sourceLength);
        if (answer == 2) { selected->loopSpan = selected->loopPhase = 0; }
        else { selected->loopSpan = span; selected->loopPhase = 0; }
        selected->length = duration; selected->rate = sourceLength / duration;
        selected->introAnchor.reset(); selected->outroAnchor.reset();
        selected->fadeInAnchor.reset(); selected->fadeOutAnchor.reset();
        selected->fadeIn = std::min(selected->fadeIn, selected->length); selected->fadeOut = std::min(selected->fadeOut, selected->length);
        safe->engine.resetPlugins(); safe->refresh();
    }), true);
}
void Workstation::configurePluginControls()
{
    if (busy) return;
    const int track = engine.selectedTrack.load();
    const auto& chain = engine.pluginChain(track);
    int insert = pluginList.list.getSelectedRow();
    if (insert < 0 && !chain.empty()) insert = 0;
    if (insert < 0 || insert >= (int) chain.size()) { setStatus("Add a VST3 to this track first."); return; }
    const auto slotId = chain[(size_t) insert]->id;
    getProperties().set("lastMessageDetails", engine.pluginControlSummary(track));
    auto* dialog = new juce::AlertWindow("Plugin MIDI controls", "Choose a control for " + chain[(size_t) insert]->description.name + ". Save your mix to keep these choices.", juce::MessageBoxIconType::NoIcon, this);
    juce::StringArray controls; for (int i = 0; i < 8; ++i) controls.add("Control " + juce::String(i + 1));
    juce::StringArray parameters { "Unassigned" }; std::vector<int> indices { -1 };
    const auto& available = chain[(size_t) insert]->processor->getParameters();
    for (int i = 0; i < available.size(); ++i)
        if (available[i]->isAutomatable()) { parameters.add(available[i]->getName(128) + " [" + juce::String(i + 1) + "]"); indices.push_back(i); }
    dialog->addComboBox("control", controls, "MIDI control"); dialog->addComboBox("parameter", parameters, "Plugin parameter");
    dialog->getComboBoxComponent("parameter")->setSelectedItemIndex(-1, juce::dontSendNotification);
    dialog->getComboBoxComponent("parameter")->setTextWhenNothingSelected("Choose a parameter");
    dialog->addButton("Assign", 1, juce::KeyPress(juce::KeyPress::returnKey)); dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    dialog->enterModalState(true, juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<Workstation>(this), dialog, track, insert, slotId, indices](int answer)
    {
        if (!safe || answer != 1) return;
        const auto& current = safe->engine.pluginChain(track);
        if (insert >= (int) current.size() || current[(size_t) insert]->id != slotId) return;
        const int choice = dialog->getComboBoxComponent("parameter")->getSelectedItemIndex();
        if (choice < 0 || choice >= (int) indices.size()) { safe->setStatus("No parameter selected; assignment kept."); return; }
        auto error = safe->engine.assignPluginControl(track, dialog->getComboBoxComponent("control")->getSelectedItemIndex(), insert, indices[(size_t) choice]);
        if (error.isNotEmpty()) safe->fail(error); else { safe->dirty = true; safe->setStatus("Plugin MIDI control assigned. Save the session to keep this choice."); }
    }), true);
}
void Workstation::showEditor(int index)
{
    if (busy) return; auto& chain = engine.pluginChain(engine.selectedTrack.load()); if (index < 0 || index >= (int) chain.size()) return;
    auto& slot = *chain[(size_t) index]; for (auto& editor : editors) if (editor->slotId == slot.id) { editor->setVisible(true); editor->toFront(true); return; }
    auto window = std::make_unique<PluginWindow>(slot);
    if (auto* c = currentClip()) window->effectsOwner = c->effects;
    editors.push_back(std::move(window));
}
void Workstation::settings()
{
    if (busy) return;
    libraryPreview.stop();
    if (settingsWindow) { settingsWindow->setVisible(true); settingsWindow->toFront(true); return; }
    auto* window = new AlloyPopupWindow<juce::DialogWindow>("Audio, MIDI and FLkey 49", colour::panel, true);
    settingsWindow.reset(window); window->setUsingNativeTitleBar(false);
    window->setContentOwned(new MidiSettings(engine), true); window->setResizable(false, false);
    window->centreAroundComponent(this, window->getWidth(), window->getHeight());
    window->setVisible(true); window->toFront(true);
    AlloyLookAndFeel::styleNativeTitleBar(*settingsWindow);
}
void Workstation::previewLibraryTrack(const juce::String& id,std::optional<double> fraction)
{
    const auto found = std::find_if(engine.library.begin(),engine.library.end(),[&](const auto& media) { return media && media->id == id; });
    if (found == engine.library.end()) {setStatus("This source is no longer in the library. Choose a current track to preview.");return;}
    if (libraryPreview.usesPrivateOutput() && !libraryPreview.configured())
    {
        libraryPreview.chooseOutput(*this,[safe = juce::Component::SafePointer<Workstation>(this),id,fraction] { if (safe) safe->previewLibraryTrack(id,fraction); });
        return;
    }
    if(fraction)libraryPreview.seekToFraction((*found)->file,id,*fraction);
    else libraryPreview.toggle((*found)->file,id);
}
bool Workstation::keyPressed(const juce::KeyPress& k)
{
    if (k.getKeyCode() == juce::KeyPress::F11Key) { toggleWindowMode(); return true; }
    if (k.getKeyCode() == 'A' && k.getModifiers().isCtrlDown() && k.getModifiers().isShiftDown()) { setAttractMode(!attractState.enabled); return true; }
    if (k.getKeyCode() == juce::KeyPress::escapeKey && attractState.enabled && editSelection.clips.empty() && !timeline.rangeDragAnchor) { setAttractMode(false); return true; }
    if (busy || dynamic_cast<juce::TextEditor*>(juce::Component::getCurrentlyFocusedComponent())) return false;
    if (timeline.selectedHotcue && timeline.hotcueKeyPressed(k)) return true;
    if (k.getKeyCode() == 'D' && !k.getModifiers().isCtrlDown() && !k.getModifiers().isAltDown() && timeline.keyPressed(k)) return true;
    if (k.getKeyCode() == juce::KeyPress::escapeKey && (!editSelection.clips.empty() || timeline.rangeDragAnchor))
    {
        if (timeline.groupMove)
        { timeline.groupMove.reset(); setStatus("Group move cancelled. Selection and mix kept."); }
        else if (timeline.rangeDragAnchor)
        { editSelection = timeline.rangeDragBefore; selectedClip = editSelection.primary; timeline.rangeDragAnchor.reset(); }
        else clearEditSelection();
        refresh(); return true;
    }
    if(k.getModifiers().isAltDown() && !k.getModifiers().isCtrlDown() && k.getKeyCode()>='1' && k.getKeyCode()<='9')
    { locateHotcue(k.getKeyCode()-'1'); return true; }
    if (k.getKeyCode() == 'A' && k.getModifiers().isCtrlDown() && !k.getModifiers().isShiftDown())
    { if (timeline.editingAutomationPoint) return false; selectAllSegments(); return true; }
    if (isTimelineContextKey(k) && (timeline.hasKeyboardFocus(true) || timelineRuler.hasKeyboardFocus(true))
        && timeline.keyPressed(k)) return true;
    if (transitionKeyPressed(k)) return true;
    if (k.getKeyCode() == juce::KeyPress::escapeKey && loopStartClip.isNotEmpty()) { loopStartClip.clear(); timeline.repaint(); setStatus("Loop start cancelled."); return true; }
    if (k.getKeyCode() == juce::KeyPress::spaceKey) { playButton.triggerClick(); return true; }
    if (k.getModifiers().isCtrlDown() && k.getModifiers().isShiftDown() && (k.getKeyCode() == juce::KeyPress::deleteKey || k.getKeyCode() == 'G'))
    {
        performTimelineAction(k.getKeyCode() == 'G' ? TimelineAction::closeGap : TimelineAction::rippleDelete, engine.position, engine.selectedTrack); return true;
    }
    if (k.getKeyCode() == juce::KeyPress::deleteKey)
    {
        if (timeline.editingAutomationPoint && timeline.keyPressed(k)) return true;
        if (performSelectionAction(TimelineAction::remove, engine.position, engine.selectedTrack)) return true;
        if (timeline.selectedLabel.isNotEmpty()) removeLabelMarker(); else removeClip(); return true;
    }
    if (k.getModifiers().isCtrlDown())
    {
        if (k.getKeyCode() == 'W') { editAnalysis(analysisSelectionId()); return true; }
        if (k.getKeyCode() == 'X' || k.getKeyCode() == 'C' || k.getKeyCode() == 'V' || k.getKeyCode() == 'T')
        {
            const auto action = k.getKeyCode() == 'X' ? TimelineAction::cut : k.getKeyCode() == 'C' ? TimelineAction::copy : k.getKeyCode() == 'V' ? TimelineAction::paste : TimelineAction::settings;
            performTimelineAction(action, engine.position.load(), engine.selectedTrack.load()); return true;
        }
        if (k.getTextCharacter() == 's' || k.getKeyCode() == 'S') { chooseSave({}, !k.getModifiers().isShiftDown()); return true; }
        if (k.getKeyCode() == 'Z') { undo(k.getModifiers().isShiftDown()); return true; }
        if (k.getKeyCode() == 'D') { duplicateClip(); return true; }
        if (k.getKeyCode() == 'O') { guardUnsaved([this] { chooseOpen(); }); return true; }
    }
    if (k.getKeyCode() == 'S') { splitClip(); return true; }
    if (!k.getModifiers().isCtrlDown() && !k.getModifiers().isAltDown())
    {
        const juce::String keys("VTMBFE90"); const int index = keys.indexOfChar((juce::juce_wchar) k.getKeyCode());
        const TimelineAction actions[] {TimelineAction::volume, TimelineAction::treble, TimelineAction::mid, TimelineAction::bass, TimelineAction::filter, TimelineAction::effect, TimelineAction::loopStart, TimelineAction::loopEnd};
        if (index >= 0) { performTimelineAction(actions[index], engine.position.load(), engine.selectedTrack.load()); return true; }
        if (k.getKeyCode() == 'P') { performTimelineAction(TimelineAction::tempo, engine.position.load(), engine.selectedTrack.load()); return true; }
        if (k.getKeyCode() == 'L') { editLabelMarker({}, engine.position); return true; }
        if (k.getKeyCode() == '[') { navigateLabel(false); return true; }
        if (k.getKeyCode() == ']') { navigateLabel(true); return true; }
    }
    return false;
}
}
