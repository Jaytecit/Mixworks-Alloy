#include "PerformanceDeck.h"
#include "PeakMeter.h"

namespace mixworks
{
namespace
{
const std::array<MidiAction, 6> deckActions {{{MidiActionKind::undo}, {MidiActionKind::redo},
    {MidiActionKind::play}, {MidiActionKind::stop}, {MidiActionKind::snap}, {MidiActionKind::record}}};
const char* potNames[] {"BASS", "MID", "HIGH", "LPF"};
MidiAction padAction(int i)
{
    return i < 8 ? MidiAction {MidiActionKind::cyclePot, i} : MidiAction {i < 12 ? MidiActionKind::mute : MidiActionKind::solo, i % 4};
}
}
PerformanceDeck::PerformanceDeck(Engine& e) : engine(e)
{
    setName("Alloy live performance controller");
    for (auto* c : std::initializer_list<juce::Component*> {&title, &state, &hint, &learn, &cancelLearn, &setup, &skip}) addAndMakeVisible(c);
    title.setText("ALLOY / LIVE DESK", juce::dontSendNotification); title.setFont(juce::FontOptions(13).withStyle("Bold"));
    title.setColour(juce::Label::textColourId, colour::orange);
    state.setJustificationType(juce::Justification::centredRight);
    hint.setFont(juce::FontOptions(11)); hint.setColour(juce::Label::textColourId, colour::muted);
    cancelLearn.onClick = [this] { learn.setToggleState(false, juce::dontSendNotification); engine.beginLearn({}); };
    setup.setTooltip("Map all live desk controls in order. Use Custom Mode and enable the appropriate MIDI inputs. Each physical move replaces only that control/action assignment and saves a backup. Skip unavailable buttons.");
    setup.onClick = [this] { learn.setToggleState(true, juce::dontSendNotification); engine.beginControllerSetup(true); };
    skip.setTooltip("Skip this guided setup step without changing its existing mapping."); skip.onClick = [this] { engine.skipControllerStep(); };
    learn.setTooltip("Enable, then click a pad/button or move an on-screen dial/fader. Move the matching FLkey control next. Custom Mode; existing assignments are kept until replaced by your choice.");
    learn.onClick = [this] { if (!learn.getToggleState()) engine.beginLearn({}); };
    for (int i = 0; i < 8; ++i)
    {
        pots[(size_t) i].configure("Pot " + juce::String(i + 1), i >= 6 ? 20000 : 0);
        configure(pots[(size_t) i], i, {MidiActionKind::deskPot, i}, engine.deskPotAddress(i), potNames[i / 2]);
        configure(faders[(size_t) i], i + 8, {i < 4 ? MidiActionKind::volume : MidiActionKind::pan, i % 4},
            {i % 4, i < 4 ? AutomationTarget::volume : AutomationTarget::pan}, juce::String(i % 4 + 1) + (i < 4 ? " / LEVEL" : " / PAN"));
    }
    configure(master, 16, {MidiActionKind::master}, {-1, AutomationTarget::master}, "MASTER");
    for (int i = 0; i < 9; ++i)
    {
        auto& b = channelButtons[(size_t) i]; addAndMakeVisible(b);
        const MidiAction a {i < 4 ? MidiActionKind::resetEq : i < 8 ? MidiActionKind::resetLowPass : MidiActionKind::resetPans, i < 8 ? i % 4 : 0};
        b.setButtonText(i < 4 ? "EQ " + juce::String(i + 1) : i < 8 ? "LPF " + juce::String(i - 3) : "Pan C");
        b.setTooltip("Fader button " + juce::String(i + 1) + " / " + midiActionId(a)); b.onClick = [this, a] { trigger(a); };
    }
    for (int i = 0; i < 16; ++i)
    {
        auto& pad = pads[(size_t) i]; addAndMakeVisible(pad);
        const auto a = padAction(i);
        pad.setButtonText(i < 8 ? "Lane" : juce::String(i < 12 ? "Mute " : "Solo ") + juce::String(i % 4 + 1));
        pad.setTooltip("Pad " + juce::String(i + 1) + " / " + midiActionId(a) + ". Enable MIDI Learn to map this pad.");
        pad.onClick = [this, a] { trigger(a); };
        pad.getProperties().set("alloyPad", true);
        pad.setColour(juce::TextButton::buttonOnColourId, colour::tracks[(size_t) (i % 4)].withAlpha(0.35f));
    }
    const char* labels[] {"Undo", "Redo", "Play", "Stop", "Snap", "Record", "Spare", "Spare"};
    for (int i = 0; i < 8; ++i)
    {
        auto& b = buttons[(size_t) i]; addAndMakeVisible(b); b.setButtonText(labels[i]);
        b.getProperties().set("alloyPad", true);
        b.setTooltip(i < 6 ? midiActionId(deckActions[(size_t) i]) + " / optional MIDI Learn" : "Reserved for a future function");
        b.onClick = [this, i] { if (i < 6) trigger(deckActions[(size_t) i]); };
    }
    buttons[5].setTooltip("Record a performance: enables Touch recording for all lanes and master. Play, perform, then Stop. Ordinary live control needs no arming or MIDI setup.");
    refresh(true);
}
void PerformanceDeck::configure(juce::Slider& s, int slot, MidiAction a, AutomationAddress address, const juce::String& name)
{
    addAndMakeVisible(s); addAndMakeVisible(names[(size_t) slot]);
    names[(size_t) slot].setText(name, juce::dontSendNotification); names[(size_t) slot].setFont(juce::FontOptions(10));
    names[(size_t) slot].setJustificationType(juce::Justification::centred);
    names[(size_t) slot].setColour(juce::Label::textColourId, slot == 16 ? colour::orange : colour::tracks[(size_t) (slot % 4)]);
    s.setName("Live desk " + juce::String(slot < 8 ? "pot " : "fader ") + juce::String(slot < 8 ? slot + 1 : slot - 7) + " " + name);
    s.setTooltip(s.getName() + ". Drag or touch to control immediately; double-click resets. MIDI mapping is optional. Record captures a performance.");
    const bool level = address.target == AutomationTarget::volume || address.target == AutomationTarget::master;
    const auto range = automationRange(address.target);
    s.setRange(range.getStart(), level ? 6 : range.getEnd(), address.target == AutomationTarget::pan ? .01 : address.target == AutomationTarget::lowPass ? 1 : .1);
    s.setDoubleClickReturnValue(true, address.target == AutomationTarget::lowPass ? 20000 : 0);
    s.setTextValueSuffix(address.target == AutomationTarget::lowPass ? " Hz" : address.target == AutomationTarget::pan ? "" : " dB");
    if (address.target == AutomationTarget::lowPass) { s.setSkewFactorFromMidPoint(1000); }
    if (address.target == AutomationTarget::pan)
    {
        s.textFromValueFunction = [](double v) { return std::abs(v) < .005 ? juce::String("C") : juce::String(std::abs(v) * 100, 0) + (v < 0 ? " L" : " R"); };
        s.valueFromTextFunction = [](const juce::String& t) { return t.containsIgnoreCase("C") ? 0. : juce::jlimit(-1., 1., t.getDoubleValue() / 100 * (t.containsIgnoreCase("L") ? -1 : 1)); };
    }
    if (slot >= 8) s.setTextValueSuffix("");
    s.updateText();
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, slot >= 8 ? 44 : 65, 18);
    if (slot >= 8) s.setSliderStyle(juce::Slider::LinearVertical);
    s.onDragStart = [this, slot, a, address]
    {
        const auto lane = slot < 8 ? engine.deskLanes[(size_t) slot] : address.track;
        if (learn.getToggleState()) { engine.beginLearn(midiActionId(a)); return; }
        gestures[(size_t) slot] = true;
        gestureAddresses[(size_t) slot] = slot < 8 ? engine.deskPotAddress(slot) : address;
        if (!engine.canWriteAutomation(lane) && remember) remember();
    };
    s.onDragEnd = [this, slot] { gestures[(size_t) slot] = false; };
    s.onValueChange = [this, &s, slot, a, address]
    {
        if (updating) return;
        if (learn.getToggleState()) { engine.beginLearn(midiActionId(a)); return; }
        const auto target = gestures[(size_t) slot] ? gestureAddresses[(size_t) slot] : slot < 8 ? engine.deskPotAddress(slot) : address;
        if (!gestures[(size_t) slot] && !engine.canWriteAutomation(target.track) && remember) remember();
        if (engine.setControlValue(target, (float) s.getValue()) && changed) changed();
    };
}
void PerformanceDeck::trigger(MidiAction a)
{
    if (learn.getToggleState()) engine.beginLearn(midiActionId(a));
    else if (action) action(a);
    refresh(isEnabled());
}
void PerformanceDeck::refresh(bool available)
{
    updating = true;
    const bool mapping = learn.getToggleState();
    const bool layoutChanged = setup.isVisible() != mapping;
    for (auto* c : {&setup, &skip, &cancelLearn}) c->setVisible(mapping);
    if (layoutChanged) resized();
    learn.setEnabled(available);
    setup.setEnabled(available); skip.setEnabled(available && learn.getToggleState());
    const juce::ScopedLock guard(engine.lock);
    auto sync = [&](juce::Slider& slider, int lane, AutomationTarget target)
    {
        // Do not overwrite an exact value while the native editor owns focus.
        bool editing = slider.isMouseButtonDown();
        for (int slot = 0; slot < 8; ++slot) if (&slider == &pots[(size_t) slot]) editing |= gestures[(size_t) slot];
        for (int i = 0; i < slider.getNumChildComponents(); ++i)
            if (auto* label = dynamic_cast<juce::Label*>(slider.getChildComponent(i))) editing |= label->getCurrentTextEditor() != nullptr;
        const auto* clip = lane >= 0 ? engine.controlClip(lane) : nullptr;
        if (!editing) slider.setValue(engine.automationValue({lane, target, {}, {}, clip ? clip->id : juce::String()}, engine.position), juce::dontSendNotification);
        slider.setEnabled(available && (!clip || !engine.project.clipTransitionLocked(clip->id)));
    };
    for (int i = 0; i < 8; ++i)
    {
        const int lane = engine.deskLanes[(size_t) i];
        sync(pots[(size_t) i], lane, engine.deskPotAddress(i).target);
        names[(size_t) i].setText(juce::String(potNames[i / 2]) + (i % 2 ? " B" : " A") + " / " + juce::String(lane + 1), juce::dontSendNotification);
        names[(size_t) i].setColour(juce::Label::textColourId, colour::tracks[(size_t) lane]);
        pads[(size_t) i].setButtonText(juce::String(potNames[i / 2]) + (i % 2 ? " B" : " A") + " / " + juce::String(lane + 1));
        pads[(size_t) i].setColour(juce::TextButton::buttonColourId, colour::tracks[(size_t) lane].withAlpha(.18f));
        pads[(size_t) i].setTooltip("Cycle this pot between three lanes; its partner keeps its own lane. Current lane " + juce::String(lane + 1) + ". MIDI Learn maps this lane selector.");
        sync(faders[(size_t) i], i % 4, i < 4 ? AutomationTarget::volume : AutomationTarget::pan);
    }
    sync(master, -1, AutomationTarget::master);
    for (int i = 0; i < 16; ++i)
    {
        pads[(size_t) i].setEnabled(available && (i >= 8 || (!gestures[(size_t) i] && !gestures[(size_t) (i ^ 1)])));
        const int lane = i % 4;
        pads[(size_t) i].setToggleState(i >= 8 && (i < 12 ? engine.project.tracks[(size_t) lane].mute : engine.project.tracks[(size_t) lane].solo), juce::dontSendNotification);
    }
    for (int i = 0; i < 8; ++i) buttons[(size_t) i].setEnabled(available && i < 6);
    for (auto& b : channelButtons) b.setEnabled(available);
    buttons[2].setToggleState(engine.playing, juce::dontSendNotification);
    buttons[4].setToggleState(engine.project.snap, juce::dontSendNotification);
    buttons[5].setToggleState(engine.automationRecordMode != AutomationRecordMode::read, juce::dontSendNotification);
    const bool armed = engine.automationRecordMode != AutomationRecordMode::read;
    state.setText((armed ? engine.playing ? "REC / TOUCH" : "TAKE ARMED" : "LIVE") + juce::String("  /  LANE ") + juce::String(engine.selectedTrack.load() + 1)
        + (engine.controlClip(engine.selectedTrack) ? " / CLIP" : ""), juce::dontSendNotification);
    state.setColour(juce::Label::textColourId, armed ? colour::error : colour::muted);
    hint.setText(learn.getToggleState() ? engine.mappingSummary().upToFirstOccurrenceOf("\n", false, false)
        : "Faders / lower pads: lanes 1-4  |  Top pads: pot lane selectors  |  Record > Play > perform > Stop  |  MIDI optional", juce::dontSendNotification);
    updating = false; repaint();
}
void PerformanceDeck::resized()
{
    auto area = getLocalBounds().reduced(12, 8); auto top = area.removeFromTop(26);
    title.setBounds(top.removeFromLeft(190));
    if (learn.getToggleState()) { cancelLearn.setBounds(top.removeFromRight(100)); skip.setBounds(top.removeFromRight(48)); setup.setBounds(top.removeFromRight(108)); }
    learn.setBounds(top.removeFromRight(110)); state.setBounds(top);
    hint.setBounds(area.removeFromBottom(20)); area.removeFromTop(7);
    auto transport = area.removeFromRight(224); area.removeFromRight(14);
    const int bh = std::min(44, transport.getHeight() / 2);
    auto cluster = transport.withHeight(bh * 2).withY(transport.getCentreY() - bh);
    // Keep action identities stable while matching the requested physical pad layout.
    constexpr std::array<size_t, 8> transportOrder {6, 7, 0, 1, 2, 3, 5, 4};
    for (size_t slot = 0; slot < transportOrder.size(); ++slot)
        buttons[transportOrder[slot]].setBounds(cluster.getX() + (int) (slot % 4) * 56, cluster.getY() + (int) (slot / 4) * bh, 53, bh - 3);
    auto left = area.removeFromLeft((int) (area.getWidth() * .56)); area.removeFromLeft(16);
    auto potRow = left.removeFromTop(std::max(91, left.getHeight() / 2)); const int pw = potRow.getWidth() / 8;
    for (int i = 0; i < 8; ++i) { auto cell = potRow.removeFromLeft(pw); names[(size_t) i].setBounds(cell.removeFromTop(17)); pots[(size_t) i].setBounds(cell); }
    left.removeFromTop(7); const int padWidth = left.getWidth() / 8, padHeight = left.getHeight() / 2;
    for (int i = 0; i < 16; ++i) pads[(size_t) i].setBounds(left.getX() + (i % 8) * padWidth + 2, left.getY() + (i / 8) * padHeight + 2, padWidth - 4, padHeight - 4);
    const int fw = area.getWidth() / 9;
    for (int i = 0; i < 9; ++i)
    {
        auto cell = area.removeFromLeft(fw); names[(size_t) (i + 8)].setBounds(cell.removeFromTop(16));
        channelButtons[(size_t) i].setBounds(cell.removeFromBottom(22).reduced(2, 0)); cell.removeFromBottom(3);
        (i < 8 ? faders[(size_t) i] : master).setBounds(cell.reduced(2, 0));
    }
}
void PerformanceDeck::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced(0.5f);
    g.setGradientFill(juce::ColourGradient(colour::raised, 0, 0, colour::panel, 0, (float) getHeight(), false));
    g.fillRoundedRectangle(r, 7); g.setColour(colour::border); g.drawRoundedRectangle(r, 7, 1);
    // Engraved copper seam and four corner fasteners: a small instrument, not a picture of a keyboard.
    g.setColour(colour::orange.withAlpha(0.6f)); g.drawLine(14, 0.5f, (float) getWidth() - 14, 0.5f);
    for (auto p : {juce::Point<float>{5, 6}, {getWidth() - 5.0f, 6}, {5, getHeight() - 6.0f}, {getWidth() - 5.0f, getHeight() - 6.0f}})
    { g.setColour(colour::background); g.fillEllipse(p.x - 2, p.y - 2, 4, 4); g.setColour(colour::border); g.drawLine(p.x - 1, p.y, p.x + 1, p.y, 1); }
    for (int i = 0; i < 5; ++i)
    {
        const auto& slider = i < 4 ? faders[(size_t) i] : master;
        auto meter = slider.getBounds().toFloat().withTrimmedBottom(23).reduced(2, 4).removeFromRight(3);
        const auto peak = i < 4 ? engine.meters[(size_t) i].load() : engine.masterPeak.load();
        g.setColour(colour::background); g.fillRoundedRectangle(meter, 1);
        const auto amount = juce::jlimit(0.f, 1.f, peak);
        AlloyLookAndFeel::drawMoltenMetal(g, meter.removeFromBottom(meter.getHeight() * amount), true, amount);
        if (peak >= 1) { g.setColour(colour::error); g.fillRect(meter.removeFromTop(3)); }
        const double phase = juce::Time::getMillisecondCounterHiRes() / 1000;
        if (forgeEffects && peak >= .89f && std::fmod(phase + i * .31, .73) < .22)
            drawForgeSparks(g, meter.getTopLeft(), 18, phase, .8f, i);
    }
}
}
