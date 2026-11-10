#include "PerformanceDeck.h"
#include "PeakMeter.h"
#include "Hotcues.h"

namespace mixworks
{
namespace
{
const std::array<MidiAction, 6> deckActions {{{MidiActionKind::undo}, {MidiActionKind::redo},
    {MidiActionKind::play}, {MidiActionKind::stop}, {MidiActionKind::snap}, {MidiActionKind::record}}};
const char* potNames[] {"BASS", "MID", "TREBLE", "CUTOFF"};
constexpr std::array wheelEffects {AutomationTarget::cutoff, AutomationTarget::pan,
    AutomationTarget::bass, AutomationTarget::mid, AutomationTarget::treble};
AutomationAddress wheelAddress(int choice)
{
    const auto index = (size_t) juce::jlimit(0, trackCount * (int) wheelEffects.size() - 1, choice - 1);
    return {(int) (index / wheelEffects.size()), wheelEffects[index % wheelEffects.size()]};
}
MidiAction padAction(int i)
{
    return i < 8 ? MidiAction {MidiActionKind::cyclePot, i} : MidiAction {i < 12 ? MidiActionKind::mute : MidiActionKind::solo, i % 4};
}
const Clip* cueTarget(const Engine& engine)
{
    const auto* selected=engine.project.clipFor(engine.selectedClip);
    return selected && selected->track==engine.selectedTrack ? selected : engine.controlClip(engine.selectedTrack);
}
bool editingControl(const juce::Slider& slider)
{
    if (slider.isMouseButtonDown()) return true;
    for (int i = 0; i < slider.getNumChildComponents(); ++i)
        if (auto* label = dynamic_cast<juce::Label*>(slider.getChildComponent(i)))
            if (label->getCurrentTextEditor()) return true;
    return false;
}
}
PerformanceDeck::PerformanceDeck(Engine& e) : engine(e)
{
    setName("Alloy live performance controller");
    for (auto* c : std::initializer_list<juce::Component*> {&title, &state, &hint, &learn, &cancelLearn, &setup, &skip, &controls, &followTimeline}) addAndMakeVisible(c);
    title.setComponentID("active-midi-profile");
    title.setText("ALLOY / LIVE DESK", juce::dontSendNotification); title.setFont(juce::FontOptions(13).withStyle("Bold"));
    title.setColour(juce::Label::textColourId, colour::orange);
    state.setJustificationType(juce::Justification::centredRight);
    hint.setFont(juce::FontOptions(11)); hint.setColour(juce::Label::textColourId, colour::muted);
    cancelLearn.setTooltip("Cancel MIDI Learn or guided setup. Completed assignments are kept.");
    cancelLearn.onClick = [this] { engine.beginLearn({}); refresh(isEnabled()); };
    setup.setTooltip("Map one controller control at a time in Custom Mode. Play/Stop already work; skip buttons you do not have.");
    setup.onClick = [this] { engine.beginControllerSetup(true); refresh(isEnabled()); };
    skip.setTooltip("Skip this guided setup step without changing its existing mapping."); skip.onClick = [this] { engine.skipControllerStep(); };
    learn.setTooltip("Enable Learn, use an on-screen control, then move its matching controller control. Use Custom Mode on FLkey.");
    learn.onClick = [this] { engine.setMidiLearnEnabled(learn.getToggleState()); refresh(isEnabled()); };
    followTimeline.setTooltip("On-screen controls follow each lane's track at the playhead, including track and transition automation. During overlaps the selected playing track wins, otherwise the newest playing track. Empty lanes use mixer-lane controls. MIDI mappings, mute/solo and menu resets remain lane-owned.");
    followTimeline.onClick = [this] { resized(); refresh(isEnabled()); };
    for (int i = 0; i < 8; ++i)
    {
        pots[(size_t) i].configure("Pot " + juce::String(i + 1), 0);
        configure(pots[(size_t) i], i, {MidiActionKind::deskPot, i}, engine.deskPotAddress(i), potNames[i / 2]);
        configure(faders[(size_t) i], i + 8, {i < 4 ? MidiActionKind::volume : MidiActionKind::pan, i % 4},
            {i % 4, i < 4 ? AutomationTarget::volume : AutomationTarget::pan}, juce::String(i % 4 + 1) + (i < 4 ? " / LEVEL" : " / PAN"));
    }
    configure(master, 16, {MidiActionKind::master}, {-1, AutomationTarget::master}, "MASTER");
    for (int slot = 0; slot < 2; ++slot) configureWheel(slot);
    for (int i = 0; i < 9; ++i)
    {
        auto& b = channelButtons[(size_t) i]; addAndMakeVisible(b);
        b.setButtonText("HC" + juce::String(i + 1)); b.onClick = [this, i] { if (engine.isMidiLearnEnabled()) trigger({MidiActionKind::timelineCue,i}); else if (cue) cue(i); };
        b.onEdit = [this, i] { if (const auto* clip = cueTarget(engine)) if (editCue) editCue(clip->id, i); };
    }
    controls.setTooltip("Edit any HC1-HC9 slot, including trimmed-out cues, or reset EQ/cutoff/pans. MIDI Learn can map reset actions; existing assignments remain active.");
    controls.onClick = [this]
    {
        const auto* clip = cueTarget(engine); const auto id = clip ? clip->id : juce::String();
        controlsMenu().showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&controls), [safe = juce::Component::SafePointer<PerformanceDeck>(this), id](int command)
        { if (safe) safe->performControlsMenu(command, id); });
    };
    for (int i = 0; i < 16; ++i)
    {
        auto& pad = pads[(size_t) i]; addAndMakeVisible(pad);
        const auto a = padAction(i);
        pad.setButtonText(i < 8 ? "Lane" : juce::String(i < 12 ? "Mute " : "Solo ") + juce::String(i % 4 + 1));
        pad.setTooltip(i < 8 ? "Choose the mixer lane controlled by this pot. Enable MIDI Learn to map the lane selector."
            : juce::String(i < 12 ? "Mute or unmute lane " : "Solo or unsolo lane ") + juce::String(i % 4 + 1) + ". MIDI mapping is optional.");
        pad.onClick = [this, a] { trigger(a); };
        pad.getProperties().set("alloyPad", true);
        pad.setColour(juce::TextButton::buttonOnColourId, colour::tracks[(size_t) (i % 4)].withAlpha(0.35f));
    }
    const char* labels[] {"Undo", "Redo", "Play", "Stop", "Snap", "Record", "Pause @", "Timing..."};
    for (int i = 0; i < 8; ++i)
    {
        auto& b = buttons[(size_t) i]; addAndMakeVisible(b); b.setButtonText(labels[i]);
        b.getProperties().set("alloyPad", true);
        const char* tips[] {"Undo the most recent musical edit.", "Redo the most recently undone edit.", "Play or pause the current mix.", "Stop playback and return to the beginning.", "Toggle beat snapping for timeline edits.", "Record a performance.", "Pause the programme at the selected launch timing. Press again to cancel a queued pause.", "Choose scheduled-pause launch timing and project phrase length. Existing Play/Pause and Stop act immediately."};
        b.setTooltip(tips[i]);
        b.onClick = [this, i] { if (i < 6) trigger(deckActions[(size_t) i]); };
    }
    buttons[6].onClick = [this] { if (scheduledPause) scheduledPause(); refresh(isEnabled()); };
    buttons[7].onClick = [this]
    {
        const auto expected = engine.project.launch;
        const auto* clip = cueTarget(engine); const auto owner = clip ? clip->id : juce::String();
        launchMenu().showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&buttons[7]), [safe = juce::Component::SafePointer<PerformanceDeck>(this), expected, owner](int command)
        { if (safe) safe->performLaunchMenu(command, expected,owner); });
    };
    addAndMakeVisible(launchState); launchState.setJustificationType(juce::Justification::centred);
    launchState.setFont(juce::FontOptions(10)); launchState.setColour(juce::Label::textColourId,colour::orange);
    buttons[5].setTooltip("Record a performance: enables Touch recording for all lanes and master. Play, perform, then Stop. Ordinary live control needs no arming or MIDI setup.");
    refresh(true);
}
void PerformanceDeck::configureWheel(int slot)
{
    const auto i = (size_t) slot;
    auto& slider = wheels[i];
    auto& target = wheelTargets[i];
    auto& label = wheelNames[i];
    const bool pitch = slot == 0;
    const juce::String name = pitch ? "Pitch wheel" : "Mod wheel";
    addAndMakeVisible(slider); addAndMakeVisible(target); addAndMakeVisible(label);
    label.setText(name, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::FontOptions(11).withStyle("Bold"));
    label.setColour(juce::Label::textColourId, colour::orange);
    target.setName(name + " effect");
    target.setTooltip("Choose a mixer lane and live effect for this on-screen wheel.");
    for (int lane = 0; lane < trackCount; ++lane)
        for (size_t effect = 0; effect < wheelEffects.size(); ++effect)
            target.addItem(juce::String(lane + 1) + " / " + automationName(wheelEffects[effect]),
                           1 + lane * (int) wheelEffects.size() + (int) effect);
    target.setSelectedId(pitch ? 1 : 6, juce::dontSendNotification);
    target.onChange = [this] { refresh(isEnabled()); };
    slider.setSliderStyle(juce::Slider::LinearVertical);
    slider.getProperties().set("alloyWheel", true);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 66, 18);
    slider.setDoubleClickReturnValue(true, 0);
    slider.setTooltip(pitch ? "Drag to shape the effect. Release returns it to neutral."
                            : "Drag to shape the effect. The setting holds when released.");
    slider.onDragStart = [this, i]
    {
        wheelDragging[i] = true;
        wheelMoved[i] = false;
        wheelTargets[i].setEnabled(false);
    };
    slider.onValueChange = [this, i]
    {
        if (updating || !wheels[i].isEnabled() || !engine.controlInputEnabled || engine.isMidiLearnEnabled()) return;
        const auto address = wheelAddress(wheelTargets[i].getSelectedId());
        if ((!wheelDragging[i] || !wheelMoved[i]) && !engine.canWriteAutomation(address.track) && remember) remember();
        if (engine.setControlValue(address, (float) wheels[i].getValue()))
        {
            wheelMoved[i] = true;
            if (changed) changed();
        }
    };
    slider.onDragEnd = [this, i, pitch]
    {
        if (pitch && wheelMoved[i]) wheels[i].setValue(0, juce::sendNotificationSync);
        wheelDragging[i] = false;
        refresh(isEnabled());
    };
}
void PerformanceDeck::triggerReset(int slot)
{
    if (slot < 0 || slot >= 9) return;
    trigger({slot < 4 ? MidiActionKind::resetEq : slot < 8 ? MidiActionKind::resetCutoff : MidiActionKind::resetPans, slot < 8 ? slot % 4 : 0});
}
juce::PopupMenu PerformanceDeck::controlsMenu() const
{
    juce::PopupMenu menu; const auto* clip = cueTarget(engine);
    menu.addSectionHeader(clip ? "Hotcues / " + clip->media->libraryTitle() : "Select a segment for hotcues");
    for (int slot = 0; slot < 9; ++slot) menu.addItem(100 + slot, "Edit HC" + juce::String(slot + 1) + "...", clip && isEnabled());
    menu.addSeparator(); menu.addSectionHeader("Reset controls / MIDI Learn");
    for (int lane = 0; lane < 4; ++lane)
    {
        menu.addItem(lane + 1, "Reset EQ / mixer lane " + juce::String(lane + 1), isEnabled());
    }
    for (int lane = 0; lane < 4; ++lane) menu.addItem(lane + 5, "Reset cutoff / mixer lane " + juce::String(lane + 1), isEnabled());
    menu.addItem(9, "Centre pan controls", isEnabled()); return menu;
}
void PerformanceDeck::performControlsMenu(int command, const juce::String& id)
{
    if (!isEnabled() || !controls.isEnabled()) return;
    if (command >= 1 && command <= 9) triggerReset(command - 1);
    else if (command >= 100 && command < 109 && id.isNotEmpty() && editCue) editCue(id, command - 100);
}
juce::PopupMenu PerformanceDeck::launchMenu() const
{
    const juce::ScopedLock guard(engine.lock); juce::PopupMenu menu;
    menu.addSectionHeader("Programme pause timing / timeline cues jump immediately");
    for (int i = 0; i < 4; ++i)
        menu.addItem(200 + i, launchTimingName((LaunchTiming) i), buttons[7].isEnabled(), engine.project.launch.timing == (LaunchTiming) i);
    menu.addSeparator(); menu.addSectionHeader("Project phrase grid / anchored at bar 1");
    for (int bars : {8, 16, 32}) menu.addItem(300 + bars, juce::String(bars) + " bars", buttons[7].isEnabled(), engine.project.launch.phraseBars == bars);
    menu.addSeparator(); menu.addItem(400, "Cancel queued pause", engine.scheduledPauseStatus().state == ScheduledPauseState::pending);
    return menu;
}
void PerformanceDeck::performLaunchMenu(int command, const LaunchSettings& expected, const juce::String& owner)
{
    if (!buttons[7].isEnabled() || engine.isMidiLearnEnabled() || command == 0) return;
    if (command == 400) { engine.cancelScheduledPause(); if (status) status(engine.scheduledPauseText()); refresh(isEnabled()); return; }
    if (command == 401)
    {
        engine.cancelLiveHotcue(owner); if (status) status(engine.liveHotcueText(owner)); refresh(isEnabled()); return;
    }
    const juce::ScopedLock guard(engine.lock);
    if (!(engine.project.launch == expected)) { if (status) status("Launch timing changed while this menu was open. Choose again."); return; }
    auto settings = expected;
    if (command >= 200 && command < 204) settings.timing = (LaunchTiming) (command - 200);
    else if (command == 308 || command == 316 || command == 332) settings.phraseBars = command - 300;
    else return;
    if (launchSettingsChanged) launchSettingsChanged(settings); refresh(isEnabled());
}
void PerformanceDeck::configure(juce::Slider& s, int slot, MidiAction a, AutomationAddress address, const juce::String& name)
{
    displayedAddresses[(size_t) slot] = address;
    addAndMakeVisible(s); addAndMakeVisible(names[(size_t) slot]);
    names[(size_t) slot].setText(name, juce::dontSendNotification); names[(size_t) slot].setFont(juce::FontOptions(10));
    names[(size_t) slot].setJustificationType(juce::Justification::centred);
    names[(size_t) slot].setColour(juce::Label::textColourId, slot == 16 ? colour::orange : colour::tracks[(size_t) (slot % 4)]);
    s.setName("Live desk " + juce::String(slot < 8 ? "pot " : "fader ") + juce::String(slot < 8 ? slot + 1 : slot - 7) + " " + name);
    s.setTooltip(s.getName() + ". Drag or touch to control immediately; double-click resets. MIDI mapping is optional. Record captures a performance.");
    const bool level = address.target == AutomationTarget::volume || address.target == AutomationTarget::master;
    const auto range = automationRange(address.target);
    s.setRange(range.getStart(), level ? 6 : range.getEnd(), address.target == AutomationTarget::pan ? .01 : address.target == AutomationTarget::cutoff ? 0 : .1);
    s.setDoubleClickReturnValue(true, 0);
    s.setTextValueSuffix(address.target == AutomationTarget::cutoff ? " Hz" : address.target == AutomationTarget::pan ? "" : " dB");
    if (address.target == AutomationTarget::cutoff)
    {
        if (auto* dial = dynamic_cast<AlloyDial*>(&s)) dial->configureCutoff();
        s.setTooltip(s.getName() + ". Turn left to soften highs, right to remove bass. Centre or double-click turns filtering off.");
    }
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
        if (engine.isMidiLearnEnabled()) { engine.beginLearn(midiActionId(a), true); return; }
        gestures[(size_t) slot] = true;
        gestureAddresses[(size_t) slot] = displayedAddresses[(size_t) slot];
        if (!engine.canWriteAutomation(lane) && remember) remember();
    };
    s.onDragEnd = [this, slot] { gestures[(size_t) slot] = false; refresh(isEnabled()); };
    s.onValueChange = [this, &s, slot, a, address]
    {
        if (updating) return;
        if (engine.isMidiLearnEnabled()) { engine.beginLearn(midiActionId(a), true); return; }
        const auto target = gestures[(size_t) slot] ? gestureAddresses[(size_t) slot] : displayedAddresses[(size_t) slot];
        // A removed displayed/held track must never silently become a lane edit.
        if (target.clipId.isNotEmpty() && !engine.project.clipFor(target.clipId)) return;
        if (!gestures[(size_t) slot] && !engine.canWriteAutomation(target.track) && remember) remember();
        if (engine.setControlValue(target, (float) s.getValue()) && changed) changed();
    };
}
AutomationAddress PerformanceDeck::timelineAddress(int lane, AutomationTarget target) const
{
    AutomationAddress address {lane, target};
    if (!followTimeline.getToggleState() || lane < 0) return address;
    const double position = engine.position.load();
    const Clip* chosen = nullptr;
    for (const auto& clip : engine.project.clips)
    {
        const double time = engine.liveClipTime(clip.id, position);
        if (clip.track != lane || time < clip.start || time >= clip.end()) continue;
        if (clip.id == engine.selectedClip) { chosen = &clip; break; }
        if (!chosen || clip.start > chosen->start) chosen = &clip;
    }
    if (chosen) address.clipId = chosen->id;
    return address;
}
void PerformanceDeck::trigger(MidiAction a)
{
    if (engine.isMidiLearnEnabled()) engine.beginLearn(midiActionId(a), true);
    else if (action) action(a);
    refresh(isEnabled());
}
void PerformanceDeck::refresh(bool available, bool visibleOnly)
{
    if (visibleOnly && !isShowing()) return;
    struct SliderView { int lane = 0; AutomationTarget target {}; AutomationAddress address; juce::String owner, trackName; float value = 0; bool enabled = false; };
    struct CueView { juce::String title, tooltip; bool saved = false, queued = false, enabled = false; };
    struct View
    {
        std::array<SliderView, 17> sliders;
        std::array<SliderView, 2> wheels;
        std::array<CueView, 9> cues;
        std::array<int, 8> lanes {};
        std::array<bool, 8> muteSolo {};
        bool mapping = false, canSkip = false, voices = false, playing = false, armed = false, snap = false, pauseEnabled = false, pausePending = false;
        juce::String pauseName, pauseTooltip, timingName, timingTitle, state, mappingText, launchText, launchTitle, help;
    } view;
    std::array<bool, 17> editing {};
    for (int slot = 0; slot < 17; ++slot)
        editing[(size_t) slot] = gestures[(size_t) slot] || editingControl(slot < 8 ? static_cast<juce::Slider&>(pots[(size_t) slot]) : slot < 16 ? faders[(size_t) slot - 8] : master);
    {
        // Take only a display snapshot. Never wait behind the audio callback,
        // and never notify accessibility/layout/component listeners under its lock.
        const juce::ScopedTryLock guard(engine.lock);
        if (!guard.isLocked()) return;
        view.mapping = engine.isMidiLearnEnabled(); view.canSkip = engine.canSkipControllerStep();
        view.voices = engine.hasLiveHotcueVoices(); view.playing = engine.playing;
        view.armed = engine.automationRecordMode != AutomationRecordMode::read; view.snap = engine.project.snap;
        auto sample = [&](int slot, int lane, AutomationTarget target)
        {
            auto& s = view.sliders[(size_t) slot];
            s.address = editing[(size_t) slot] ? displayedAddresses[(size_t) slot] : timelineAddress(lane, target);
            s.lane = lane = s.address.track; s.target = target = s.address.target;
            s.owner = lane < 0 ? juce::String("Master") : "Mixer lane " + juce::String(lane + 1);
            double time = engine.position.load();
            const auto* clip = engine.project.clipFor(s.address.clipId);
            if (clip)
            {
                s.trackName = clip->media->libraryTitle();
                s.owner = "Track / " + s.trackName + " / lane " + juce::String(lane + 1);
                time = engine.liveClipTime(clip->id, time);
            }
            s.value = s.address.clipId.isEmpty() || clip ? engine.automationValue(s.address, time) : 0.f;
            s.enabled = available && (s.address.clipId.isEmpty() || clip) && !engine.project.automationLocked(s.address);
        };
        for (int i = 0; i < 8; ++i)
        {
            view.lanes[(size_t) i] = engine.deskLanes[(size_t) i];
            sample(i, view.lanes[(size_t) i], engine.deskPotAddress(i).target);
            sample(i + 8, i % 4, i < 4 ? AutomationTarget::volume : AutomationTarget::pan);
            view.muteSolo[(size_t) i] = i < 4 ? engine.project.tracks[(size_t) i].mute : engine.project.tracks[(size_t) (i % 4)].solo;
        }
        sample(16, -1, AutomationTarget::master);
        for (size_t i = 0; i < view.wheels.size(); ++i)
        {
            auto& wheel = view.wheels[i];
            wheel.address = wheelAddress(wheelTargets[i].getSelectedId());
            wheel.target = wheel.address.target;
            wheel.value = engine.automationValue(wheel.address, engine.position.load());
            wheel.enabled = available && !view.mapping && engine.controlInputEnabled
                && !engine.exporting && !engine.project.automationLocked(wheel.address);
        }
        const auto pause = engine.scheduledPauseStatus(); const auto pauseText = engine.scheduledPauseText();
        view.pausePending = pause.state == ScheduledPauseState::pending;
        view.pauseEnabled = available && !view.mapping && engine.controlInputEnabled && view.playing && !engine.rehearsalActive() && !engine.gridAudition.active() && engine.playbackReady();
        view.timingName = launchTimingName(engine.project.launch.timing);
        const auto timingText = view.timingName + " / phrase " + juce::String(engine.project.launch.phraseBars) + " bars";
        view.pauseName = "Programme pause / " + view.timingName;
        view.pauseTooltip = view.pauseName + ". " + pauseText + ". Press again to cancel a queued pause. The existing Play/Pause and Stop remain immediate.";
        view.timingTitle = "Launch timing / " + timingText;
        const auto* owner = cueTarget(engine);
        for (int slot = 0; slot < 9; ++slot)
        {
            auto& c=view.cues[(size_t)slot]; const auto* saved=owner ? hotcueFor(*owner,slot) : nullptr;
            const auto position=owner ? hotcueTimelineTime(*owner,slot,engine.position) : std::optional<double>{};
            const auto name="HC"+juce::String(slot+1)+" / "+(owner ? owner->media->libraryTitle() : juce::String("Select a track"));
            c.saved=saved!=nullptr; c.queued=false;
            c.title=name+(position ? " / timeline "+timeText(*position) : saved ? " / outside trimmed audio" : " / empty");
            c.tooltip=name+". Jump the whole mix immediately, stopped or playing; all lanes follow the timeline. Alt+"+juce::String(slot+1)
                +" does the same. Launch quantisation does not delay timeline cues. MIDI Learn maps this pad. Right-click edits its source position."
                +(saved && !position ? " Extend the trim edge or edit this cue first." : "");
            c.enabled=available && owner && (view.mapping || (saved ? position.has_value() : !view.playing));
        }
        view.state = juce::String(view.armed ? view.playing ? "REC / TOUCH" : "TAKE ARMED" : "LIVE") + "  /  LANE "
            + juce::String(engine.selectedTrack.load() + 1) + (owner ? " / SEGMENT" : "");
        view.mappingText = "Mapping: " + engine.activeMappingTitle();
        view.launchText=timingText+(view.pausePending ? "\nQueued pause / bar "+juce::String(pause.beat/4+1,2) : "\nProgramme pause");
        view.launchTitle="Programme pause timing / "+timingText+". Timeline cues jump immediately.";
        view.help=view.mapping ? engine.mappingSummary().upToFirstOccurrenceOf("\n",false,false)
            : "HC pads and Alt+1-9 jump the whole mix immediately. Pause timing: "+timingText+". "+pauseText;

    }
    const juce::ScopedValueSetter<bool> updateGuard(updating, true);
    learn.setToggleState(view.mapping, juce::dontSendNotification);
    const bool layoutChanged = setup.isVisible() != view.mapping;
    for (auto* c : {&setup, &skip, &cancelLearn}) c->setVisible(view.mapping);
    if (layoutChanged) resized();
    learn.setEnabled(available); setup.setEnabled(available); skip.setEnabled(available && view.canSkip);
    followTimeline.setEnabled(available && !view.mapping && std::none_of(editing.begin(), editing.end(), [](bool v) { return v; }));
    auto sync = [&](juce::Slider& slider, const SliderView& s, int slot)
    {
        displayedAddresses[(size_t) slot] = s.address;
        auto& properties = slider.getProperties();
        if (!properties.contains("alloyOwnerBaseHint")) { properties.set("alloyOwnerBaseHint", slider.getTooltip()); properties.set("alloyOwnerBaseName", slider.getName()); }
        const auto caption = properties["alloyOwnerBaseName"].toString() + " / " + s.owner + " / " + automationName(s.target);
        slider.setName(caption); slider.setTitle(caption); slider.setTooltip(s.owner + " / " + automationName(s.target) + ". " + properties["alloyOwnerBaseHint"].toString());
        if (!editing[(size_t) slot])
        {
            // Track curves can reach +12 dB. Do not clip their displayed value
            // to the lane desk's normal +6 dB fader travel.
            if (s.target == AutomationTarget::volume)
            {
                const double maximum = s.address.clipId.isEmpty() ? 6 : 12;
                if (slider.getMaximum() != maximum) slider.setRange(-60, maximum, .1);
            }
            slider.setValue(s.value, juce::dontSendNotification);
        }
        slider.setEnabled(s.enabled);
    };
    for (int i = 0; i < 8; ++i)
    {
        const int lane = view.lanes[(size_t) i]; const auto& pot = view.sliders[(size_t) i];
        sync(pots[(size_t) i], pot, i);
        names[(size_t) i].setText(juce::String(potNames[i / 2]) + (i % 2 ? " B" : " A") + " / " + juce::String(lane + 1)
            + (followTimeline.getToggleState() ? "\n" + (pot.trackName.isEmpty() ? juce::String("Mixer lane") : pot.trackName) : juce::String()), juce::dontSendNotification);
        names[(size_t) i].setTooltip(pot.owner);
        names[(size_t) i].setColour(juce::Label::textColourId, colour::tracks[(size_t) lane]);
        pads[(size_t) i].setButtonText(juce::String(potNames[i / 2]) + (i % 2 ? " B" : " A") + " / " + juce::String(lane + 1));
        pads[(size_t) i].setColour(juce::TextButton::buttonColourId, colour::tracks[(size_t) lane].withAlpha(.18f));
        pads[(size_t) i].setTooltip("Cycle this pot between three lanes; its partner keeps its own lane. Current lane " + juce::String(lane + 1) + ". MIDI Learn maps this lane selector.");
        const auto& fader = view.sliders[(size_t) i + 8]; sync(faders[(size_t) i], fader, i + 8);
        names[(size_t) i + 8].setText(juce::String(i % 4 + 1) + (i < 4 ? " / LEVEL" : " / PAN")
            + (followTimeline.getToggleState() ? "\n" + (fader.trackName.isEmpty() ? juce::String("Lane") : fader.trackName) : juce::String()), juce::dontSendNotification);
        names[(size_t) i + 8].setTooltip(fader.owner);
    }
    sync(master, view.sliders[16], 16);
    for (size_t i = 0; i < wheels.size(); ++i)
    {
        const auto& wheel = view.wheels[i];
        auto& slider = wheels[i];
        const auto name = wheelNames[i].getText() + " / " + wheelTargets[i].getText();
        slider.setName(name); slider.setTitle(name);
        if (!wheelDragging[i])
        {
            const auto range = automationRange(wheel.target);
            slider.setRange(range.getStart(), range.getEnd(), .01);
            slider.setTextValueSuffix(wheel.target == AutomationTarget::cutoff || wheel.target == AutomationTarget::pan ? "" : " dB");
            slider.textFromValueFunction = wheel.target == AutomationTarget::cutoff
                ? std::function<juce::String(double)>([](double value) { return cutoffText((float) value, true); })
                : std::function<juce::String(double)>();
            slider.setValue(wheel.value, juce::dontSendNotification);
            slider.updateText();
        }
        slider.setEnabled(wheel.enabled);
        wheelTargets[i].setEnabled(available && !view.mapping && !wheelDragging[i]);
    }
    for (int i = 0; i < 16; ++i)
    {
        pads[(size_t) i].setEnabled(available && (i >= 8 || (!gestures[(size_t) i] && !gestures[(size_t) (i ^ 1)])));
        pads[(size_t) i].setToggleState(i >= 8 && view.muteSolo[(size_t) (i % 8)], juce::dontSendNotification);
    }
    for (int i = 0; i < 8; ++i) buttons[(size_t) i].setEnabled(available && (i < 6 || !view.mapping));
    buttons[0].setEnabled(available && (view.mapping || (bool)getProperties().getWithDefault("historyUndoReady",true)));
    buttons[1].setEnabled(available && (view.mapping || (bool)getProperties().getWithDefault("historyRedoReady",true)));

    buttons[5].setEnabled(available && (!view.voices || engine.workMode==WorkMode::recordPerform));
    buttons[5].setTooltip(engine.workMode==WorkMode::recordPerform ? "Start / finalise an independent editable performance take." : "Record arrangement automation (advanced). Record/Perform records an independent take.");
    buttons[6].setEnabled(view.pauseEnabled); buttons[6].setButtonText(view.pausePending ? "Cancel @" : "Pause @");
    buttons[6].setTitle(view.pauseName); buttons[6].setName(view.pauseName); buttons[6].setTooltip(view.pauseTooltip);
    buttons[7].setTitle(view.timingTitle); buttons[7].setName(view.timingTitle); buttons[7].setButtonText(view.timingName);
    for (int slot = 0; slot < 9; ++slot)
    {
        auto& button = channelButtons[(size_t) slot]; const auto& c = view.cues[(size_t) slot];
        button.setTitle(c.title); button.setTooltip(c.tooltip);
        button.setColour(juce::TextButton::buttonColourId, hotcueColour(slot).withAlpha(c.saved ? .23f : .07f));
        button.setColour(juce::TextButton::buttonOnColourId, hotcueColour(slot).withAlpha(.4f));
        button.setColour(juce::TextButton::textColourOffId, hotcueColour(slot));
        button.setColour(juce::TextButton::textColourOnId, hotcueColour(slot));
        button.setButtonText("HC" + juce::String(slot + 1) + (c.queued ? " @" : ""));
        button.setToggleState(c.queued || (cueSelected && cueSelected(slot)), juce::dontSendNotification); button.setEnabled(c.enabled);
    }
    controls.setEnabled(available); buttons[2].setToggleState(view.playing, juce::dontSendNotification);
    buttons[4].setToggleState(view.snap, juce::dontSendNotification); buttons[5].setToggleState(view.armed, juce::dontSendNotification);
    state.setText(view.state, juce::dontSendNotification); state.setColour(juce::Label::textColourId, view.armed ? colour::error : colour::muted);
    launchState.setText(view.launchText, juce::dontSendNotification); launchState.setTitle(view.launchTitle); launchState.setTooltip(view.help);
    title.setText("ALLOY / LIVE DESK  |  " + view.mappingText, juce::dontSendNotification); title.setTooltip(view.mappingText);
    hint.setText({}, juce::dontSendNotification); hint.setTooltip(view.help); repaint();
}

void PerformanceDeck::resized()
{
    auto area = getLocalBounds().reduced(12, 8); auto top = area.removeFromTop(26);
    title.setBounds(top.removeFromLeft(juce::jlimit(180, 380, getWidth() - 850))); controls.setBounds(top.removeFromLeft(120).reduced(3, 1));
    followTimeline.setBounds(top.removeFromLeft(144));
    if (learn.getToggleState()) { cancelLearn.setBounds(top.removeFromRight(100)); skip.setBounds(top.removeFromRight(48)); setup.setBounds(top.removeFromRight(108)); }
    learn.setBounds(top.removeFromRight(110)); state.setBounds(top);
    hint.setBounds({}); area.removeFromTop(7);
    auto transport = area.removeFromRight(224); area.removeFromRight(14);
    launchState.setBounds(transport.withHeight(42));
    const int bh = std::min(44, transport.getHeight() / 2);
    auto cluster = transport.withHeight(bh * 2).withY(transport.getCentreY() - bh);
    // Keep action identities stable while matching the requested physical pad layout.
    constexpr std::array<size_t, 8> transportOrder {6, 7, 0, 1, 2, 3, 5, 4};
    for (size_t slot = 0; slot < transportOrder.size(); ++slot)
        buttons[transportOrder[slot]].setBounds(cluster.getX() + (int) (slot % 4) * 56, cluster.getY() + (int) (slot / 4) * bh, 53, bh - 3);
    auto wheelArea = area.removeFromLeft(174); area.removeFromLeft(12);
    for (size_t i = 0; i < wheels.size(); ++i)
    {
        auto cell = wheelArea.removeFromLeft(87).reduced(3, 0);
        wheelNames[i].setBounds(cell.removeFromTop(18));
        wheelTargets[i].setBounds(cell.removeFromBottom(25));
        wheels[i].setBounds(cell.reduced(9, 5));
    }
    auto left = area.removeFromLeft((int) (area.getWidth() * .56)); area.removeFromLeft(16);
    auto potRow = left.removeFromTop(std::max(91, left.getHeight() / 2)); const int pw = potRow.getWidth() / 8;
    for (int i = 0; i < 8; ++i) { auto cell = potRow.removeFromLeft(pw); names[(size_t) i].setBounds(cell.removeFromTop(followTimeline.getToggleState() ? 31 : 17)); pots[(size_t) i].setBounds(cell); }
    left.removeFromTop(7); const int padWidth = left.getWidth() / 8, padHeight = left.getHeight() / 2;
    for (int i = 0; i < 16; ++i) pads[(size_t) i].setBounds(left.getX() + (i % 8) * padWidth + 2, left.getY() + (i / 8) * padHeight + 2, padWidth - 4, padHeight - 4);
    const int fw = area.getWidth() / 9;
    for (int i = 0; i < 9; ++i)
    {
        auto cell = area.removeFromLeft(fw); names[(size_t) (i + 8)].setBounds(cell.removeFromTop(followTimeline.getToggleState() ? 30 : 16));
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
