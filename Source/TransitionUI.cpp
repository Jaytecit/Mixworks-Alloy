#include "Workstation.h"
#include "PresetNodes.h"
#include <cmath>

namespace mixworks
{
namespace
{
// Linking already-prepared tracks changes their controls, not their placement or
// prepared audio. Keep insertion's other admission rules without stopping Play.
bool canAddPreparedTransition(const Engine& engine, bool preparationOrBusy)
{
    return !preparationOrBusy && !engine.exporting && !engine.rehearsalActive()
        && !engine.project.hasPendingPlacement()
        && (engine.workMode == WorkMode::recordPerform || (engine.automationRecordMode == AutomationRecordMode::read && !engine.recordingAutomation()));
}
juce::String transitionEditorNodes(const Project& project,const juce::String& incoming)
{
    TransitionStyle::Nodes nodes;
    return capturePresetNodes(project,incoming,nodes).wasOk()
        ? juce::JSON::toString(presetNodesToJson(nodes),true,17) : juce::String();
}
}
void Workstation::togglePairMovementLock(const juce::String& incoming)
{
    if(busy || engine.exporting || pendingTempo>0 || tempoPreparationPending || engine.project.hasPendingPlacement())
    { setStatus("Finish preparation/export before changing this pair's movement lock."); refresh(); return; }
    Project candidate; { const juce::ScopedLock guard(engine.lock); candidate=engine.project; }
    const auto* link=candidate.transitionFor(incoming); if(!link) {setStatus("Select a current transition pair before changing its movement lock.");return;}
    const bool locked=!link->movementLocked; const auto result=candidate.setTransitionMovementLocked(incoming,locked);
    if(result.failed()) {setStatus(result.getErrorMessage());refresh();return;}
    { const juce::ScopedLock guard(engine.lock); remember(); candidate.performanceTakes.swap(engine.project.performanceTakes); std::swap(candidate,engine.project); }
    selectedClip=selectedTransition=incoming; refresh();
    setStatus(locked ? "Pair movement locked. Move either track's title to move its locked group; unlock this pair before IN/OUT alignment."
        : "Pair movement unlocked. Track titles move independently; the transition relationship remains.");
}
void Workstation::removeSelectedTransition()
{
    if(!canAddPreparedTransition(engine,busy || pendingTempo>0 || tempoPreparationPending || !timelineLoads.empty()))
    { setStatus("Finish preparation/export or rehearsal. In Arrange choose Read before removing a transition."); return; }
    const auto* link=engine.project.transitionFor(selectedTransition); if(!link) {setStatus("Select a transition frame to remove.");return;}
    if(engine.project.clipTransitionLocked(link->outgoing) || engine.project.clipTransitionLocked(link->incoming))
    {setStatus("Turn off Protect edits in the advanced transition controls before removing this pair.");return;}
    const auto incoming=selectedTransition;
    if (link->presetOwner.isNotEmpty())
    {
        engine.flushMidiControls();
        engine.finishAutomation();
        Project before;
        { const juce::ScopedLock guard(engine.lock); before = engine.project; }
        auto candidate = before;
        auto result = candidate.removeOwnedTransitionPreset(incoming);
        if (result.wasOk()) result = before.validateTransitionLocks(candidate);
        if (result.failed()) { setStatus(result.getErrorMessage()); return; }
        {
            const juce::ScopedLock guard(engine.lock);
            remember();
            candidate.performanceTakes.swap(engine.project.performanceTakes);
            std::swap(candidate,engine.project);
        }
        refresh();
        setStatus("Preset removed. OUT/IN alignment and manual nodes kept. Undo restores the preset.");
        return;
    }
    auto document=[](Project project) {project.performanceTakes.clear();return juce::JSON::toString(projectToJson(project),true,17);};
    const auto before=document(engine.project); const auto range=engine.project.transitionRange(*link);
    auto* question=new juce::AlertWindow("Remove transition?",engine.project.clipFor(link->outgoing)->media->libraryTitle()+" -> "+engine.project.clipFor(incoming)->media->libraryTitle()
        +"\nRange "+timeText(range.getStart())+" to "+timeText(range.getEnd())
        +"\n\nUnlink this pair and keep its current automation and sound. Undo restores the pair.",juce::MessageBoxIconType::QuestionIcon,this);
    question->addButton("Unlink pair",1); question->addButton("Cancel",0,juce::KeyPress(juce::KeyPress::escapeKey));
    question->enterModalState(true,juce::ModalCallbackFunction::create([safe=juce::Component::SafePointer<Workstation>(this),incoming,before,document](int answer)
    {
        if(!safe || answer==0)return;
        if(safe->selectedTransition!=incoming || document(safe->engine.project)!=before)
        {safe->setStatus("The pair or its controls changed. Select the current transition and choose Remove again.");return;}
        if(!canAddPreparedTransition(safe->engine,safe->busy || safe->pendingTempo>0 || safe->tempoPreparationPending || !safe->timelineLoads.empty()))
        {safe->setStatus("The transition is unavailable now. Finish preparation/export, then try Remove again.");return;}
        auto candidate=safe->engine.project; const auto result=candidate.removeTransition(incoming,false);
        if(result.failed()) {safe->setStatus(result.getErrorMessage());return;}
        {const juce::ScopedLock guard(safe->engine.lock);safe->remember();candidate.performanceTakes.swap(safe->engine.project.performanceTakes);std::swap(candidate,safe->engine.project);}
        safe->selectedTransition.clear();safe->transitionNodeOptionsFor.clear();safe->refresh();
        safe->setStatus("Pair unlinked. Current automation, sound and track positions kept. Undo restores the pair.");
    }),true);
}
void Workstation::addSelectedTransition(const juce::String& outgoing, const juce::String& incoming)
{
    if (!canAddPreparedTransition(engine, busy || pendingTempo > 0 || tempoPreparationPending || !timelineLoads.empty()) || editSelection.range)
    { setStatus("Select whole tracks and finish preparation, export or automation recording before adding a transition. Normal playback can continue."); return; }
    engine.flushMidiControls();
    Project before;
    { const juce::ScopedLock guard(engine.lock); before = engine.project; }
    auto candidate=before;
    const auto result = candidate.addTransition(outgoing,incoming,transitionOverlapBeats(),selectedTransitionPreset(),true);
    if (result.failed()) { setStatus(result.getErrorMessage()); return; }
    { const juce::ScopedLock guard(engine.lock); remember();
        // The callback may have appended take events since the edit snapshot.
        candidate.performanceTakes.swap(engine.project.performanceTakes);
        std::swap(engine.project,candidate); }
    selectedTransition = selectedClip = incoming;
    transitionNodeOptionsFor.clear(); refresh();
    const auto* added = engine.project.transitionFor(incoming);
    const auto treatment = added->alignmentOnly ? juce::String("OUT/IN aligned") : added->presetName + " applied";
    setStatus(treatment + ": " + engine.project.clipFor(outgoing)->media->libraryTitle() + " -> "
        + engine.project.clipFor(incoming)->media->libraryTitle() + " at " + timeText(engine.project.transitionRange(*added).getStart())
        + ". Playback position kept; outside audio stays audible unless muted.");
}
void Workstation::chooseAddTransition(juce::Rectangle<int> anchor)
{
    if (!canAddPreparedTransition(engine, busy || pendingTempo > 0 || tempoPreparationPending || !timelineLoads.empty()) || editSelection.range)
    { setStatus("Select whole tracks and finish preparation, export or automation recording before adding a transition. Normal playback can continue."); return; }
    auto ids = editSelection.clips;
    if (ids.empty() && currentClip()) ids.push_back(selectedClip);
    if (ids.empty() || ids.size() > 2) { setStatus("Select one track, or Ctrl-select two overlapping tracks, then Add Transition."); return; }
    const auto ordered = [&](const juce::String& a, const juce::String& b)
    {
        const auto* first = engine.project.clipFor(a); const auto* second = engine.project.clipFor(b);
        return first && second && (first->start < second->start || (first->start == second->start && first->track < second->track));
    };
    if (ids.size() == 2)
    { if (!ordered(ids[0],ids[1])) std::swap(ids[0],ids[1]); addSelectedTransition(ids[0],ids[1]); return; }
    const auto* selected = engine.project.clipFor(ids.front()); if (!selected) return;
    std::vector<std::pair<juce::String,juce::String>> pairs; juce::PopupMenu menu;
    for (const auto& clip : engine.project.clips)
    {
        if (clip.id == selected->id || std::min(clip.end(),selected->end()) <= std::max(clip.start,selected->start)) continue;
        const bool first = ordered(selected->id,clip.id);
        const auto out = first ? selected->id : clip.id, in = first ? clip.id : selected->id;
        if (engine.project.clipTransitionLocked(out) || engine.project.clipTransitionLocked(in)
            || std::any_of(engine.project.transitionLinks.begin(),engine.project.transitionLinks.end(),[&](const auto& l)
                { return (l.outgoing == out || l.incoming == in) && !(l.alignmentOnly && l.outgoing == out && l.incoming == in); })) continue;
        pairs.emplace_back(out,in);
        menu.addItem((int)pairs.size(),(first ? "To: " : "From: ") + clip.media->libraryTitle() + " / lane " + juce::String(clip.track+1));
    }
    if (pairs.empty()) { setStatus("Overlap this track with a free partner, or unlink its existing transition first."); return; }
    if (pairs.size() == 1) { addSelectedTransition(pairs[0].first,pairs[0].second); return; }
    menu.showMenuAsync(anchor.isEmpty() ? juce::PopupMenu::Options().withTargetComponent(&addTransitionButton) : timelineMenuOptions(anchor),
        [safe=juce::Component::SafePointer<Workstation>(this),pairs](int choice)
        { if (safe && choice > 0 && choice <= (int)pairs.size()) safe->addSelectedTransition(pairs[(size_t)choice-1].first,pairs[(size_t)choice-1].second); });
}

void Workstation::showNewTransitionDefaults(juce::Rectangle<int> anchor)
{
    juce::PopupMenu menu, preset, length;
    menu.addSectionHeader("Defaults for new transitions");
    for (int i = 0; i < transitionPreset.getNumItems(); ++i)
        preset.addItem(1 + i, transitionPreset.getItemText(i), true, transitionPreset.getSelectedItemIndex() == i);
    for (int i = 0; i < transitionLength.getNumItems(); ++i)
        length.addItem(10 + i, transitionLength.getItemText(i), true, transitionLength.getSelectedItemIndex() == i);
    menu.addSubMenu("Treatment", preset); menu.addSubMenu("Length", length);
    menu.showMenuAsync(timelineMenuOptions(anchor), [safe = juce::Component::SafePointer<Workstation>(this)](int choice)
    {
        if (!safe || !choice || safe->busy || safe->engine.exporting) return;
        if (choice < 10) safe->transitionPreset.setSelectedItemIndex(choice - 1, juce::dontSendNotification);
        else
        {
            safe->transitionLength.setSelectedItemIndex(choice - 10, juce::dontSendNotification);
            safe->transitionLength.onChange();
        }
        safe->setStatus("New transitions / " + safe->transitionPreset.getText() + " / " + safe->transitionLength.getText());
    });
}

void Workstation::openTransitionEditor(const juce::String& incoming, bool rehearsal)
{
    if (!engine.project.transitionFor(incoming)) return;
    selectTransition(incoming, false);
    showToolPage(1);
    if (rehearsal) transitionAdvancedOpen = true;
    layoutTransitionUI();
    transitionViewport.setViewPosition(0, rehearsal ? transitionAdvancedBody.getY() : 0);
}

void Workstation::showTransitionMenu(const juce::String& incoming, juce::Rectangle<int> anchor)
{
    const auto* link = engine.project.transitionFor(incoming);
    if (!link || busy || engine.exporting) return;
    const auto* out = engine.project.clipFor(link->outgoing);
    const auto* in = engine.project.clipFor(link->incoming);
    if (!out || !in) return;
    const auto range = engine.project.transitionRange(*link);
    const auto pairAtOpen = *link;
    const auto styles = engine.project.transitionPresets;
    const bool ready = pendingTempo <= 0 && !tempoPreparationPending && timelineLoads.empty() && !engine.project.hasPendingPlacement();
    const bool unlocked = !engine.project.clipTransitionLocked(link->outgoing) && !engine.project.clipTransitionLocked(incoming);
    const bool editable = ready && unlocked && canAddPreparedTransition(engine, false);
    enum Choice { edit = 1, compare, movement, protect, mute, remove, presetBase = 10, lengthBase = 100, savedBase = 200 };
    juce::PopupMenu menu, presets, lengths;
    menu.addSectionHeader(out->media->libraryTitle() + " -> " + in->media->libraryTitle());
    if (link->presetName.isNotEmpty()) menu.addSectionHeader(link->presetName);
    else if (link->alignmentOnly) menu.addSectionHeader("OUT/IN aligned / No preset");
    menu.addSectionHeader(timeText(range.getStart() - engine.project.tempo.origin) + " to " + timeText(range.getEnd() - engine.project.tempo.origin));
    menu.addItem(edit, "Edit transition...");
    menu.addItem(compare, "Rehearsal and presets...");
    if (!ready) menu.addSectionHeader("Finish preparation before editing");
    else if (!unlocked) menu.addSectionHeader("Unlock this pair and its tracks to change treatment");
    else if (!editable) menu.addSectionHeader("End rehearsal or choose Read before changing treatment");
    for (int i = 0; i < transitionPresetForSelection.getNumItems(); ++i)
        presets.addItem(presetBase + i, transitionPresetForSelection.getItemText(i), editable);
    for (size_t i = 0; i < styles.size(); ++i)
        presets.addItem(savedBase + (int) i, styles[i].name, editable);
    const auto clock = engine.project.gridClock();
    const double barsInFrame = (clock.beatsAt(range.getEnd()) - clock.beatsAt(range.getStart())) / 4.0;
    for (int bars : {4, 8, 16, 32})
        lengths.addItem(lengthBase + bars, juce::String(bars) + " bars", editable, std::abs(barsInFrame - bars) < 1.e-6);
    menu.addSubMenu("Apply preset", presets, editable);
    menu.addSubMenu("Set frame length", lengths, editable);
    menu.addSeparator();
    menu.addItem(movement, "Lock pair movement", ready, link->movementLocked);
    menu.addItem(protect, "Protect edits", ready, link->locked);
    menu.addItem(mute, "Mute outside bounds", editable && canInsertArrangement(), link->muteOutside);
    menu.addItem(remove, link->presetOwner.isNotEmpty() ? "Remove preset" : "Unlink pair...", editable && canAddPreparedTransition(engine, !ready));
    menu.showMenuAsync(timelineMenuOptions(anchor), [safe = juce::Component::SafePointer<Workstation>(this), incoming, pairAtOpen, range, styles](int choice)
    {
        if (!safe || !choice || safe->busy || safe->engine.exporting) return;
        const auto* currentPair = safe->engine.project.transitionFor(incoming);
        if (!currentPair || !(*currentPair == pairAtOpen) || safe->engine.project.transitionRange(*currentPair) != range)
        { safe->setStatus("This pair changed. Open its transition menu again."); return; }
        if (choice == edit || choice == compare) { safe->openTransitionEditor(incoming, choice == compare); return; }
        safe->selectTransition(incoming, false);
        const auto* pair = safe->engine.project.transitionFor(incoming);
        if (!pair) return;
        if (choice == movement) safe->togglePairMovementLock(incoming);
        else if (choice == protect) safe->setTransitionProtection(!pair->locked);
        else if (choice == mute) safe->setTransitionMuteOutside(!pair->muteOutside);
        else if (choice == remove) safe->removeSelectedTransition();
        else if (choice >= savedBase)
        {
            const auto index = (size_t) (choice - savedBase);
            if (index >= styles.size() || index >= safe->engine.project.transitionPresets.size()
                || safe->engine.project.transitionPresets[index].name != styles[index].name
                || !(safe->engine.project.transitionPresets[index].style == styles[index].style))
            { safe->setStatus("Saved presets changed. Open the transition menu again."); return; }
            safe->applySelectedStyle(styles[index].style,styles[index].name);
        }
        else if (choice >= lengthBase) safe->changeSelectedTransition((double) (choice - lengthBase), {});
        else if (choice >= presetBase) safe->changeSelectedTransition({}, (TransitionPreset) (choice - presetBase));
    });
}

void Workstation::setTransitionMuteOutside(bool mute)
{
    if (!canInsertArrangement()) { refresh(); return; }
    const juce::ScopedLock guard(engine.lock);
    auto candidate = engine.project;
    const auto result = candidate.setTransitionMuteOutside(selectedTransition, mute);
    if (result.failed()) { setStatus(result.getErrorMessage()); refresh(); return; }
    remember(); engine.project = std::move(candidate); refresh();
}

void Workstation::setTransitionProtection(bool protect)
{
    if (busy || engine.exporting || engine.project.hasPendingPlacement() || pendingTempo > 0 || tempoPreparationPending)
    { setStatus("Finish preparation/export before changing edit protection."); refresh(); return; }
    auto candidate = engine.project;
    const auto result = candidate.setTransitionLocked(selectedTransition, protect);
    if (result.failed()) { setStatus(result.getErrorMessage()); refresh(); return; }
    { const juce::ScopedLock guard(engine.lock); remember(); candidate.performanceTakes.swap(engine.project.performanceTakes); std::swap(candidate, engine.project); }
    refresh(); setStatus("Transition edit protection updated. Pair movement locks are separate.");
}

void Workstation::initialiseTransitionUI()
{
    addAndMakeVisible(transitionPanel); transitionViewport.setViewedComponent(&transitionBody, false);
    transitionViewport.setScrollBarsShown(true, true); transitionViewport.setScrollBarThickness(12);
    transitionViewport.onResize = [this] { layoutTransitionUI(); };
    addAndMakeVisible(transitionTab); transitionTab.getProperties().set("alloyTab", true);
    transitionTab.onClick = [this]
    {
        if (!engine.project.transitionFor(selectedTransition))
        {
            engine.project.ensureTransitionLinks();
            const auto link = std::find_if(engine.project.transitionLinks.begin(), engine.project.transitionLinks.end(), [&](const auto& item)
            { return item.incoming == selectedClip || item.outgoing == selectedClip; });
            if (link != engine.project.transitionLinks.end()) selectedTransition = link->incoming;
        }
        toggleConsole(1); refresh();
    };
    for (auto* control : std::initializer_list<juce::Component*> {&transitionLengthLabel,
        &transitionPresetLabel, &selectedTransitionLength, &transitionPresetForSelection, &transitionLock, &previousTransition, &nextTransition}) transitionBody.addAndMakeVisible(control);
    transitionBody.addAndMakeVisible(transitionPairLabel);
    transitionBody.addAndMakeVisible(addTransitionButton); transitionBody.addAndMakeVisible(transitionMuteOutside); transitionBody.addAndMakeVisible(removeTransitionButton);
    removeTransitionButton.setTooltip("Remove the selected preset and keep manual automation and OUT/IN alignment.");
    removeTransitionButton.onClick=[this] {removeSelectedTransition();};
    addTransitionButton.setTooltip("Link two ready, overlapping tracks while stopped or playing, without moving them or the playhead. Ctrl-select two, or select one and choose its partner.");
    addTransitionButton.onClick = [this] { chooseAddTransition(); };
    transitionMuteOutside.setTooltip("For this pair, silence the incoming track before the frame and the outgoing track after it. Off keeps exterior audio and its automation. Saved with the mix; one Undo.");
    transitionMuteOutside.onClick = [this] { setTransitionMuteOutside(transitionMuteOutside.getToggleState()); };
    const std::array<const char*, 4> names {"Volume", "Bass", "Mid", "Treble"};
    const std::array<const char*, 7> modes {"None", "Crossfade", "Fade in", "Fade out", "Swap", "Manual", "Bass fade handover"};
    const std::array<juce::Colour, 4> colours {juce::Colour(0xffffad70), juce::Colour(0xffe4d15b), juce::Colour(0xff76c7ef), juce::Colour(0xffd3a3ef)};
    for (size_t band = 0; band < 4; ++band)
    {
        auto& label = transitionBandLabels[band]; label.setText(names[band], juce::dontSendNotification); label.setColour(juce::Label::textColourId, colours[band]); transitionBody.addAndMakeVisible(label);
        auto& show = transitionCurveShow[band]; auto& only = transitionCurveOnly[band];
        for (auto* button : {&show, &only}) { transitionBody.addAndMakeVisible(button); button->getProperties().set("alloyTab", true); }
        show.setButtonText("Show"); only.setButtonText("Only"); show.setClickingTogglesState(true);
        show.setTitle(juce::String(names[band]) + " curve visibility"); only.setTitle("Isolate " + juce::String(names[band]) + " curve");
        show.setTooltip("Show this band's automation curve. This affects the display only; audio and automation continue unchanged.");
        only.setTooltip("Display only this band's automation curve. Click again to restore the other visible curves. Audio is unchanged.");
        show.onClick = [this, band]
        {
            transitionCurveShown[band] = transitionCurveShow[band].getToggleState();
            if (!transitionCurveShown[band] && transitionCurveOnlyBand == (int) band) transitionCurveOnlyBand = -1;
            refreshTransitionUI(); timeline.repaint();
        };
        only.onClick = [this, band]
        {
            transitionCurveOnlyBand = transitionCurveOnlyBand == (int) band ? -1 : (int) band;
            if (transitionCurveOnlyBand >= 0) transitionCurveShown[band] = true;
            refreshTransitionUI(); timeline.repaint();
        };
        auto& mode = transitionBandModes[band]; auto& strength = transitionBandStrengths[band]; auto& gap = transitionBandGaps[band]; auto& position = transitionBandPositions[band];
        for (auto* combo : {&mode, &strength, &gap, &position}) transitionBody.addAndMakeVisible(combo);
        mode.setTitle(juce::String(names[band]) + " transition mode");
        for (int id = 0; id < (band == 1 ? 7 : 6); ++id) mode.addItem(modes[(size_t) id], id + 1);
        if (band == 0) mode.addItem("Constant power", (int) TransitionBandMode::constantPower + 1);
        strength.setTitle(juce::String(names[band]) + " transition strength");
        for (int amount : {25, 50, 75, 100}) strength.addItem(juce::String(amount) + "% strength", amount);
        gap.setTitle(juce::String(names[band]) + " swap gap");
        int gapId = 1; for (double beats : {0., .5, 1., 2., 4., 8.}) gap.addItem("Gap " + juce::String(beats, beats == .5 ? 1 : 0) + " beats", gapId++);
        position.setTitle(juce::String(names[band]) + " swap position");
        for (int amount : {25, 50, 75}) position.addItem("Swap at " + juce::String(amount) + "%", amount);
        mode.setTextWhenNothingSelected("Existing / custom");
        mode.setTooltip("Place this preset as ordinary nodes inside the selected frame, replacing that band's shape. Then drag/add/delete nodes normally. Manual keeps the current shape for editing.");
        strength.setTooltip("Place the chosen shape again at this strength, replacing its nodes in this frame. 100% reaches a full cut.");
        gap.setTooltip("Beat-based separation between the outgoing cut and incoming entry. Gaps must fit inside the frame.");
        position.setTooltip("Position of this band's swap inside the transition, measured through the mix beat map.");
        for (auto* combo : {&mode, &strength, &gap, &position}) combo->onChange = [this, band] { if (!refreshing) changeSelectedTransitionBand((int) band); };
    }
    const juce::String shortcuts = "Alt + Left/Right: navigate (seeks only while stopped). Ctrl + Alt + Left/Right: move frame. Shift + Alt + Left/Right: resize end. Alt + L: lock/unlock.";
    previousTransition.setTooltip(shortcuts); nextTransition.setTooltip(shortcuts);
    transitionPanel.addAndMakeVisible(transitionAdvanced);
    transitionBody.addChildComponent(transitionAdvancedBody);
    transitionBody.addAndMakeVisible(transitionProtection);
    transitionProtection.setTooltip("Protect this pair's tracks and treatment from edits. This is separate from the pair movement lock.");
    transitionProtection.onClick = [this] { setTransitionProtection(transitionProtection.getToggleState()); };
    transitionAdvanced.onClick = [this]
    {
        transitionAdvancedOpen = !transitionAdvancedOpen;
        layoutTransitionUI();
        transitionViewport.setViewPosition(0, transitionAdvancedOpen ? transitionAdvancedBody.getY() : 0);
    };
    transitionLengthLabel.setText("Length", juce::dontSendNotification);
    transitionPresetLabel.setText("Preset", juce::dontSendNotification);
    selectedTransitionLength.setTitle("Selected transition length");
    selectedTransitionLength.setTooltip("Resize only the selected frame, keeping its start and both songs fixed. One Undo restores it.");
    for (int bars : {4, 8, 16, 32}) selectedTransitionLength.addItem(juce::String(bars) + " bars", bars);
    selectedTransitionLength.setTextWhenNothingSelected("Select a transition");
    selectedTransitionLength.setEditableText(true);
    selectedTransitionLength.onChange = [this]
    { if (!refreshing) { const double bars = selectedTransitionLength.getText().getDoubleValue(); if (bars > 0) changeSelectedTransition(bars, {}); } };
    transitionPresetForSelection.setTitle("Selected transition preset");
    transitionPresetForSelection.setTextWhenNothingSelected("Apply preset...");
    transitionPresetForSelection.addItem("None", 1);
    transitionPresetForSelection.addItem("Bass swap", 2); transitionPresetForSelection.addItem("Crossfade", 3);
    transitionPresetForSelection.setTooltip("Apply only to this pair. Other segments and the tempo map stay in place. Manual bass conflicts are reported.");
    transitionPresetForSelection.onChange = [this]
    { if (!refreshing && transitionPresetForSelection.getSelectedId() > 0) changeSelectedTransition({}, (TransitionPreset) (transitionPresetForSelection.getSelectedId() - 1)); };
    transitionLock.onClick = [this] { changeSelectedTransition({}, {}, true); };
    transitionFilterLabel.setText("Filter", juce::dontSendNotification);
    for (auto* component : std::initializer_list<juce::Component*> {&transitionFilterLabel, &transitionFilterMode, &transitionFilterDepth}) transitionBody.addAndMakeVisible(component);
    transitionFilterMode.setTitle("Transition filter mode");
    transitionFilterMode.addItem("None", 1); transitionFilterMode.addItem("High to low", 2); transitionFilterMode.addItem("Low to high", 3);
    transitionFilterDepth.setTitle("Transition filter depth");
    for (int amount : {25, 50, 75, 100}) transitionFilterDepth.addItem(juce::String(amount) + "% depth", amount);
    transitionFilterMode.setTooltip("High to low closes the outgoing low-pass while opening the incoming high-pass. Low to high reverses those filters. Both follow this frame.");
    transitionFilterDepth.setTooltip("Place the chosen sweep again as ordinary Filter nodes, replacing cutoff points in this frame. Other parameters stay in place. None clears this frame's cutoff shape.");
    for (auto* combo : {&transitionFilterMode, &transitionFilterDepth}) combo->onChange = [this] { if (!refreshing) changeSelectedTransitionFilter(); };
    previousTransition.onClick = [this] { navigateTransition(-1); };
    nextTransition.onClick = [this] { navigateTransition(1); };
    transitionLock.setTooltip("Lock just this pair's track movement. Consecutive locked pairs move as a group; unlock this pair before adjusting IN/OUT alignment.");
    transitionAdvanced.setTooltip("Show A/B/C comparisons and saved transition presets.");
    initialiseTransitionRehearsalUI();
}
void Workstation::layoutTransitionUI()
{
    const int width = std::max(520, transitionViewport.getWidth() - transitionViewport.getScrollBarThickness());
    const int columns = width >= 900 ? 4 : 2;
    const int bandRows = 4 / columns;
    const int coreHeight = 36 + 28 + 12 + 28 + 16 + bandRows * 140 + 12 + 28 + 12 + 16;
    const int advancedHeight = width >= 700 ? 248 : 378;
    transitionBody.setSize(width, std::max(coreHeight + (transitionAdvancedOpen ? advancedHeight : 0), transitionViewport.getHeight()));
    auto area = transitionBody.getLocalBounds().reduced(10, 8);
    auto row = area.removeFromTop(28);
    auto navigation = row.removeFromRight(160);
    previousTransition.setBounds(navigation.removeFromLeft(80).withTrimmedRight(3)); nextTransition.setBounds(navigation);
    transitionPairLabel.setBounds(row.withTrimmedRight(8)); area.removeFromTop(8);
    row = area.removeFromTop(28);
    transitionAdvanced.setBounds(std::max(104, transitionPanel.getWidth() - 206), 3, 164, 26);
    addTransitionButton.setBounds(row.removeFromLeft(104).withTrimmedRight(4));
    transitionMuteOutside.setBounds(row.removeFromLeft(130).withTrimmedRight(4));
    removeTransitionButton.setBounds(row.removeFromLeft(148).withTrimmedRight(6));
    transitionProtection.setBounds(row.removeFromLeft(110)); area.removeFromTop(12);
    row = area.removeFromTop(28);
    transitionLengthLabel.setBounds(row.removeFromLeft(42)); selectedTransitionLength.setBounds(row.removeFromLeft(106)); row.removeFromLeft(12);
    transitionLock.setBounds(row.removeFromRight(100)); row.removeFromRight(12);
    transitionPresetLabel.setBounds(row.removeFromLeft(44)); transitionPresetForSelection.setBounds(row);
    area.removeFromTop(16);
    const int bandWidth = (area.getWidth() - (columns - 1) * 12) / columns;
    for (size_t band = 0; band < 4; ++band)
    {
        auto cell = juce::Rectangle<int>(area.getX() + (int) (band % columns) * (bandWidth + 12), area.getY() + (int) (band / columns) * 140, bandWidth, 132);
        transitionBandLabels[band].setBounds(cell.removeFromTop(18));
        transitionBandModes[band].setBounds(cell.removeFromTop(26)); cell.removeFromTop(4);
        row = cell.removeFromTop(24); transitionCurveShow[band].setBounds(row.removeFromLeft(row.getWidth() / 2).withTrimmedRight(3)); transitionCurveOnly[band].setBounds(row); cell.removeFromTop(4);
        row = cell.removeFromTop(24); transitionBandStrengths[band].setBounds(row.removeFromLeft(row.getWidth() / 2).withTrimmedRight(3)); transitionBandGaps[band].setBounds(row); cell.removeFromTop(4);
        transitionBandPositions[band].setBounds(cell.removeFromTop(26));
    }
    area.removeFromTop(bandRows * 140 + 12);
    row = area.removeFromTop(28);
    transitionFilterLabel.setBounds(row.removeFromLeft(42));
    transitionFilterDepth.setBounds(row.removeFromRight(110)); row.removeFromRight(6); transitionFilterMode.setBounds(row);
    area.removeFromTop(16);
    transitionAdvanced.setButtonText(transitionAdvancedOpen ? "Hide rehearsal & presets" : "Rehearsal & presets...");
    transitionAdvanced.setToggleState(transitionAdvancedOpen, juce::dontSendNotification);
    transitionAdvancedBody.setVisible(transitionAdvancedOpen);
    transitionAdvancedBody.setBounds(area.withHeight(advancedHeight));
    auto rehearsalArea = transitionAdvancedBody.getLocalBounds();
    layoutTransitionRehearsalUI(rehearsalArea);
}

void Workstation::changeSelectedTransitionFilter()
{
    if(!canAddPreparedTransition(engine,busy || pendingTempo>0 || tempoPreparationPending || !timelineLoads.empty()))
    { setStatus("Finish preparation, export or rehearsal. In Arrange choose Read before editing a transition filter; playback can continue."); refresh(); return; }
    engine.flushMidiControls(); engine.finishAutomation();
    Project before; { const juce::ScopedLock guard(engine.lock); before=engine.project; }
    const auto incoming=selectedTransition;
    if(!before.transitionFor(incoming)) { setStatus("Select an overlapping transition frame before choosing a filter."); return; }
    auto setting=transitionFilterNodeOptions;
    if(transitionFilterMode.getSelectedId()>0)setting.mode=(TransitionFilterMode)(transitionFilterMode.getSelectedId()-1);
    if(transitionFilterDepth.getSelectedId()>0)setting.strength=transitionFilterDepth.getSelectedId()/100.;
    auto candidate=before; const auto result=placeTransitionFilterNodes(candidate,incoming,setting);
    if(result.failed()) { setStatus(result.getErrorMessage()); refresh(); return; }
    const bool changed=juce::JSON::toString(projectToJson(candidate))!=juce::JSON::toString(projectToJson(before));
    auto apply=[safe=juce::Component::SafePointer<Workstation>(this),incoming,setting,changed,candidate]() mutable
    {
        if(!safe)return;
        if(safe->selectedTransition!=incoming || !canAddPreparedTransition(safe->engine,
            safe->busy || safe->pendingTempo>0 || safe->tempoPreparationPending || !safe->timelineLoads.empty()))
        { safe->setStatus("The transition is unavailable now. Finish preparation/export, then select its frame and try again. Curves kept."); safe->refresh(); return; }
        if(changed) { const juce::ScopedLock guard(safe->engine.lock); safe->remember();
            candidate.performanceTakes.swap(safe->engine.project.performanceTakes); std::swap(safe->engine.project,candidate); }
        safe->transitionFilterNodeOptions=setting;
        safe->getProperties().set("transitionEditorNodes",transitionEditorNodes(safe->engine.project,incoming));
        safe->getProperties().set("transitionEditorTarget",(int)AutomationTarget::cutoff);
        safe->selectedClip=incoming; safe->selectedAutomationTarget=(int)AutomationTarget::cutoff; safe->automationClip=incoming;
        safe->refresh(); safe->setStatus("Filter applied in this frame. Exterior nodes and mixer-lane curves kept; programme transport kept.");
    };
    const auto trusted=getProperties()["transitionEditorNodes"].toString();
    if(changed && ((int)getProperties().getWithDefault("transitionEditorTarget",-1)!=(int)AutomationTarget::cutoff
        || trusted.isEmpty() || trusted!=transitionEditorNodes(before,incoming)))
    {
        if(confirmTransitionReplacement(before,candidate,apply)) { transitionNodeOptionsFor.clear(); refresh(); return; }
    }
    apply();
}
void Workstation::changeSelectedTransitionBand(int band)
{
    if(band<0 || band>=4)return;
    if(!canAddPreparedTransition(engine,busy || pendingTempo>0 || tempoPreparationPending || !timelineLoads.empty()))
    { setStatus("Finish preparation, export or rehearsal. In Arrange choose Read before editing transition bands; playback can continue."); refresh(); return; }
    engine.flushMidiControls(); engine.finishAutomation();
    Project before; { const juce::ScopedLock guard(engine.lock); before=engine.project; }
    const auto incoming=selectedTransition;
    if(!before.transitionFor(incoming)) { setStatus("Select an overlapping transition frame before choosing its treatment."); return; }
    const size_t index=(size_t)band; auto setting=transitionNodeOptions[index];
    if(transitionBandModes[index].getSelectedId()>0)setting.mode=(TransitionBandMode)(transitionBandModes[index].getSelectedId()-1);
    if(transitionBandStrengths[index].getSelectedId()>0)setting.strength=transitionBandStrengths[index].getSelectedId()/100.;
    const std::array<double,6> gaps{0,.5,1,2,4,8}; const int gap=transitionBandGaps[index].getSelectedId();
    if(gap>0 && gap<=(int)gaps.size())setting.gapBeats=gaps[(size_t)gap-1];
    if(transitionBandPositions[index].getSelectedId()>0)setting.position=transitionBandPositions[index].getSelectedId()/100.;
    auto candidate=before; const auto result=placeTransitionBandNodes(candidate,incoming,band,setting);
    if(result.failed()) { setStatus(result.getErrorMessage()); refresh(); return; }
    const bool changed=juce::JSON::toString(projectToJson(candidate))!=juce::JSON::toString(projectToJson(before));
    auto apply=[safe=juce::Component::SafePointer<Workstation>(this),incoming,setting,index,band,changed,candidate]() mutable
    {
        if(!safe)return;
        if(safe->selectedTransition!=incoming || !canAddPreparedTransition(safe->engine,
            safe->busy || safe->pendingTempo>0 || safe->tempoPreparationPending || !safe->timelineLoads.empty()))
        { safe->setStatus("The transition is unavailable now. Finish preparation/export, then select its frame and try again. Curves kept."); safe->refresh(); return; }
        if(changed) { const juce::ScopedLock guard(safe->engine.lock); safe->remember();
            candidate.performanceTakes.swap(safe->engine.project.performanceTakes); std::swap(safe->engine.project,candidate); }
        safe->transitionNodeOptions[index]=setting;
        safe->getProperties().set("transitionEditorNodes",transitionEditorNodes(safe->engine.project,incoming));
        safe->getProperties().set("transitionEditorTarget",band);
        safe->selectedClip=incoming; safe->selectedAutomationTarget=band; safe->selectedStem=-1; safe->automationClip=incoming;
        safe->transitionCurveShown[index]=true; if(safe->transitionCurveOnlyBand>=0)safe->transitionCurveOnlyBand=band;
        if(const auto* clip=safe->engine.project.clipFor(incoming))safe->selectTrack(clip->track);
        safe->refresh(); safe->setStatus("Treatment applied in this frame. Exterior nodes and mixer-lane curves kept; programme transport kept.");
    };
    const auto trusted=getProperties()["transitionEditorNodes"].toString();
    if(changed && ((int)getProperties().getWithDefault("transitionEditorTarget",-1)!=band
        || trusted.isEmpty() || trusted!=transitionEditorNodes(before,incoming)))
    {
        if(confirmTransitionReplacement(before,candidate,apply)) { transitionNodeOptionsFor.clear(); refresh(); return; }
    }
    apply();
}
void Workstation::selectTransition(const juce::String& incoming, bool focus)
{
    const auto* link = engine.project.transitionFor(incoming); if (!link) return;
    if (!engine.project.clipFor(incoming)) return;
    if (selectedTransition != incoming) engine.endTransitionRehearsal();
    selectedTransition = incoming;
    if (selectedClip != incoming) clearEditSelection();
    selectedClip = incoming;
    if (focus)
    {
        const auto range = engine.project.transitionRange(*link);
        if (!engine.playing) engine.seek(range.getStart());
        viewport.setViewPosition(std::max(0, (int) ((range.getStart() - 2) * timeline.pixelsPerSecond)),
            std::max(0, timeline.laneTop(std::min(engine.project.clipFor(link->outgoing)->track, engine.project.clipFor(incoming)->track))));
    }
    refresh();
}
void Workstation::navigateTransition(int direction)
{
    engine.project.ensureTransitionLinks();
    std::vector<const Project::TransitionLink*> links;
    for (const auto& link : engine.project.transitionLinks) links.push_back(&link);
    std::stable_sort(links.begin(), links.end(), [&](const auto* a, const auto* b)
    { return engine.project.transitionRange(*a).getStart() < engine.project.transitionRange(*b).getStart(); });
    if (links.empty()) return;
    const auto current = std::find_if(links.begin(), links.end(), [&](const auto* link) { return link->incoming == selectedTransition; });
    const int index = current == links.end() ? (direction > 0 ? 0 : (int) links.size() - 1)
        : juce::jlimit(0, (int) links.size() - 1, (int) std::distance(links.begin(), current) + direction);
    selectTransition(links[(size_t) index]->incoming, true);
}
bool Workstation::confirmTransitionReplacement(const Project& before,const Project& after,std::function<void()> apply,const juce::String& explicitIncoming)
{
    const auto incoming=explicitIncoming.isEmpty() ? selectedTransition : explicitIncoming;
    const auto* previous=before.transitionFor(incoming);
    const auto* link=previous ? previous : after.transitionFor(incoming); if(!link)return false;
    const auto range=previous ? before.transitionRange(*link) : after.transitionRange(*link); bool replacing=false;
    juce::StringArray affected;
    for(const auto& id:{link->outgoing,link->incoming})
    {
        const auto* clip=before.clipFor(id); const auto* changed=after.clipFor(id); if(!clip || !changed)continue;
        const auto& old=before.effectSettings(*clip); const auto& next=after.effectSettings(*changed);
        for(int n=0;n<trackAutomationCount;++n)
        {
            const auto address=AutomationAddress{clip->track,(AutomationTarget)n,{},{},id};
            const auto& curve=old.automation[(size_t)n];
            const auto& updated=next.automation[(size_t)n];
            const bool same=curve.enabled==updated.enabled && curve.points.size()==updated.points.size()
                && std::equal(curve.points.begin(),curve.points.end(),updated.points.begin(),[](const auto& a,const auto& b)
                {return a.time==b.time && a.value==b.value && a.bend==b.bend && a.exponential==b.exponential && a.gainSpace==b.gainSpace;});
            if(same)continue;
            affected.addIfNotAlreadyThere(automationName((AutomationTarget)n));
            for(const auto& point:curve.points)
            { const double time=before.automationSessionTime(address,point.time); if(time>=range.getStart()-1.e-7 && time<=range.getEnd()+1.e-7)replacing=true; }
        }
        const bool fades=clip->fadeIn!=changed->fadeIn || clip->fadeOut!=changed->fadeOut;
        if(fades)affected.addIfNotAlreadyThere("track fades");
        replacing |= fades;
    }
    if(!replacing)return false;
    auto document=[](Project p) { p.performanceTakes.clear(); return juce::JSON::toString(projectToJson(p),true,17); };
    const auto baseline=document(before); const bool existing=previous!=nullptr;
    juce::String overlaps;
    for(const auto& other:before.transitionLinks) if(other.incoming!=incoming
        && (other.incoming==link->incoming || other.incoming==link->outgoing || other.outgoing==link->incoming || other.outgoing==link->outgoing))
    {
        const auto shared=before.transitionRange(other).getIntersectionWith(range);
        if(shared.getLength()>0) overlaps+="\nShared with the transition into "+before.clipFor(other.incoming)->media->libraryTitle()
            +" from "+timeText(shared.getStart())+" to "+timeText(shared.getEnd())+"; those shared nodes also change.";
    }
    auto* dialog=new juce::AlertWindow("Replace transition curves?",
        before.clipFor(link->outgoing)->media->libraryTitle()+" -> "+before.clipFor(incoming)->media->libraryTitle()
        +"\nRange "+timeText(range.getStart())+" to "+timeText(range.getEnd())
        +"\n\nReplace "+affected.joinIntoString(", ")+" in this pair's frame? Exterior nodes and mixer-lane curves stay. Lane curves still combine with the track treatment."
        +overlaps+"\n\nPlayback continues. Undo restores Arrange edits; Record/Perform keeps the change in its take.",juce::MessageBoxIconType::QuestionIcon,this);
    dialog->addButton("Replace in this range",1); dialog->addButton("Cancel",0,juce::KeyPress(juce::KeyPress::escapeKey));
    dialog->enterModalState(true,juce::ModalCallbackFunction::create([safe=juce::Component::SafePointer<Workstation>(this),baseline,incoming,existing,document,apply=std::move(apply)](int answer)
    {
        if(!safe || answer!=1)return;
        if((existing && safe->selectedTransition!=incoming) || document(safe->engine.project)!=baseline)
        { safe->setStatus("The transition or its controls changed. Review the current range and choose the preset again."); return; }
        apply();
    }),true);
    return true;
}
void Workstation::changeSelectedTransition(std::optional<double> bars, std::optional<TransitionPreset> preset, bool toggleLock, bool)
{
    if(toggleLock) { togglePairMovementLock(selectedTransition); return; }
    if (busy || engine.exporting) { setStatus("Finish preparation or export before changing this transition."); return; }
    if (pendingTempo > 0 || tempoPreparationPending) { setStatus("Wait for tempo preparation to finish before editing or locking a transition."); refresh(); return; }
    if(!toggleLock && !canAddPreparedTransition(engine,false))
    { setStatus("Finish rehearsal or placement. In Arrange choose Read before applying a transition; normal playback can continue."); refresh(); return; }
    engine.flushMidiControls(); engine.finishAutomation();
    Project before;
    { const juce::ScopedLock guard(engine.lock); before = engine.project; }
    const auto* link = before.transitionFor(selectedTransition); if (!link) { refresh(); return; }
    auto candidate = before;
    const auto result = toggleLock ? candidate.setTransitionLocked(selectedTransition, !link->locked)
        : bars ? candidate.setTransitionLength(selectedTransition, *bars) : preset ? candidate.applyTransitionPreset(selectedTransition, *preset,true)
        : juce::Result::fail("Choose a transition edit.");
    if (result.failed()) { setStatus(result.getErrorMessage()); refresh(); return; }
    if (juce::JSON::toString(projectToJson(candidate)) == juce::JSON::toString(projectToJson(before))) { refresh(); return; }
    // Model construction, serialization and component refresh must not silence
    // callbacks by holding the audio lock. Publish only the completed edit here.
    { const juce::ScopedLock guard(engine.lock); remember();
        // The callback may have appended take events since the edit snapshot.
        candidate.performanceTakes.swap(engine.project.performanceTakes);
        std::swap(engine.project,candidate); }
    setStatus(toggleLock ? "Transition " + juce::String(engine.project.transitionFor(selectedTransition)->locked ? "locked. Unlock it to edit its segments or tempo." : "unlocked.")
        : "Selected transition updated. Other segment positions and the tempo map were kept.");
    if (preset)
    {
        transitionNodeOptionsFor.clear();
        if (*preset != TransitionPreset::alignOnly)
        {
            const int band=*preset==TransitionPreset::bassSwap ? 1 : 0;
            selectedClip=selectedTransition; selectedAutomationTarget=band; selectedStem=-1; automationClip=selectedClip;
            transitionCurveShown[(size_t)band]=true; if(transitionCurveOnlyBand>=0)transitionCurveOnlyBand=band;
            selectTrack(engine.project.clipFor(selectedClip)->track);
        }
    }
    refresh();
}
void Workstation::refreshTransitionUI()
{
    auto* link = engine.project.transitionFor(selectedTransition);
    if (!link || (selectedClip != link->incoming && selectedClip != link->outgoing)) { selectedTransition.clear(); link = nullptr; }
    juce::String pairText = "Select a transition frame";
    if (link)
    {
        const auto* out = engine.project.clipFor(link->outgoing);
        const auto* in = engine.project.clipFor(link->incoming);
        if (out && in)
        {
            const auto range = engine.project.transitionRange(*link);
            const auto treatment = link->presetName.isNotEmpty() ? link->presetName : link->alignmentOnly ? juce::String("No preset") : juce::String();
            pairText = out->media->libraryTitle() + " -> " + in->media->libraryTitle()
                + (treatment.isNotEmpty() ? " / " + treatment : juce::String()) + " / "
                + timeText(range.getStart() - engine.project.tempo.origin) + " to " + timeText(range.getEnd() - engine.project.tempo.origin);
        }
    }
    transitionPairLabel.setText(pairText, juce::dontSendNotification);
    transitionPairLabel.setTooltip(pairText);
    const bool protectedTracks = link && (engine.project.clipTransitionLocked(link->outgoing) || engine.project.clipTransitionLocked(link->incoming));
    const bool editable = link && !protectedTracks && !busy && !engine.project.hasPendingPlacement();
    const auto unavailable=busy ? juce::String("Wait for preparation to finish.") : engine.exporting ? juce::String("Finish or cancel export first.")
        : protectedTracks ? juce::String("Unlock this transition and its participating tracks first.")
        : pendingTempo>0 || tempoPreparationPending || !timelineLoads.empty() ? juce::String("Wait for source and tempo preparation to finish.")
        : !currentClip() ? juce::String("Select a ready track or Ctrl-select two overlapping tracks first.")
        : editSelection.range ? juce::String("Select whole tracks; clear the highlighted audio range first.")
        : engine.workMode!=WorkMode::recordPerform && engine.automationRecordMode!=AutomationRecordMode::read ? juce::String("Choose Read before placing a transition in Arrange. Playback can continue.") : juce::String();
    addTransitionButton.getProperties().set("mixworksHintDisabled",unavailable);
    transitionPresetForSelection.getProperties().set("mixworksHintDisabled",unavailable.isEmpty() ? "Select an unlocked overlapping transition frame first." : unavailable);
    addTransitionButton.setEnabled(canAddPreparedTransition(engine, busy || pendingTempo > 0 || tempoPreparationPending || !timelineLoads.empty())
        && currentClip() && !editSelection.range);
    transitionMuteOutside.setEnabled(editable && canInsertArrangement());
    transitionMuteOutside.setToggleState(link && link->muteOutside,juce::dontSendNotification);
    removeTransitionButton.setEnabled(editable && canAddPreparedTransition(engine,busy || pendingTempo>0 || tempoPreparationPending || !timelineLoads.empty()));
    removeTransitionButton.setButtonText(link && link->presetOwner.isNotEmpty() ? "Remove preset" : "Unlink pair...");
    removeTransitionButton.setTooltip(link && link->presetOwner.isNotEmpty()
        ? "Remove preset nodes and keep manual automation and OUT/IN alignment."
        : "Unlink the selected pair and keep its current automation.");
    removeTransitionButton.getProperties().set("mixworksHintDisabled",unavailable.isEmpty() ? "Select an unlocked transition frame before removing it." : unavailable);
    refreshTransitionRehearsalUI(editable);
    selectedTransitionLength.setEnabled(editable); transitionPresetForSelection.setEnabled(editable);
    const auto expectedNodes=getProperties()["transitionEditorNodes"].toString();
    const bool nodesChanged=link && expectedNodes.isNotEmpty() && expectedNodes!=transitionEditorNodes(engine.project,selectedTransition);
    if (transitionNodeOptionsFor != selectedTransition || nodesChanged)
    {
        getProperties().remove("transitionEditorNodes"); getProperties().remove("transitionEditorTarget");
        transitionNodeOptionsFor = selectedTransition;
        transitionFilterNodeOptions = link ? link->filter : TransitionFilterSettings{};
        for (size_t band = 0; band < 4; ++band) transitionNodeOptions[band] = link ? link->bands[band].value_or(TransitionBandSettings{}) : TransitionBandSettings{};
        if (link) for (size_t band = 0; band < 4; ++band) if (!link->bands[band])
        {
            const auto* out = engine.project.clipFor(link->outgoing); const auto* in = engine.project.clipFor(link->incoming);
            if (!out || !in) continue;
            const auto& outCurve = engine.project.effectSettings(*out).automation[band];
            const auto& inCurve = engine.project.effectSettings(*in).automation[band];
            auto& setting = transitionNodeOptions[band];
            if (!outCurve.points.empty() || !inCurve.points.empty()) setting.mode = TransitionBandMode::manual;
            if (const auto times = engine.project.transitionSwapTimes(*link,(int)band))
            {
                const auto frame = engine.project.transitionRange(*link); const auto clock = engine.project.gridClock();
                const double first = clock.beatsAt(frame.getStart()), length = clock.beatsAt(frame.getEnd())-first;
                if (length > 0)
                {
                    setting.mode = TransitionBandMode::swap;
                    if (band == 1 && !link->bassSwap)
                    {
                        const auto& points = outCurve.points;
                        bool sharp = false;
                        for (size_t i = 1; i < points.size(); ++i)
                        {
                            const double t = out->start+out->localTimeForSource(points[i].time);
                            if (t > frame.getStart()+1.e-8 && t < frame.getEnd()-1.e-8 && points[i-1].value != points[i].value && points[i-1].bend >= 1) sharp = true;
                        }
                        if (!sharp) setting.mode = TransitionBandMode::swapFade;
                    }
                    setting.position = ((clock.beatsAt((*times)[0])+clock.beatsAt((*times)[1]))*.5-first)/length;
                    setting.gapBeats = std::max(0.,clock.beatsAt((*times)[1])-clock.beatsAt((*times)[0]));
                    float cut = 0; for (const auto& node : outCurve.points)
                    { const double t = out->start+out->localTimeForSource(node.time); if (t >= frame.getStart()-1.e-8 && t <= frame.getEnd()+1.e-8) cut = std::min(cut,node.value); }
                    setting.strength = cut <= (band == 0 ? -160.f : -60.f) ? 1. : 1.-juce::Decibels::decibelsToGain((double)cut);
                }
            }
        }
    }
    const auto filter = transitionFilterNodeOptions;
    transitionFilterMode.setSelectedId((int) filter.mode + 1, juce::dontSendNotification); transitionFilterMode.setEnabled(editable);
    if(link && filter.mode==TransitionFilterMode::none)
    {
        const auto* out=engine.project.clipFor(link->outgoing); const auto* in=engine.project.clipFor(link->incoming);
        if(out && in && (!engine.project.effectSettings(*out).automation[(size_t)AutomationTarget::cutoff].points.empty()
            || !engine.project.effectSettings(*in).automation[(size_t)AutomationTarget::cutoff].points.empty()))
        { transitionFilterMode.setSelectedId(0,juce::dontSendNotification); transitionFilterMode.setText("Existing / custom",juce::dontSendNotification); }
    }
    transitionFilterDepth.setSelectedId(0, juce::dontSendNotification);
    transitionFilterDepth.setText(juce::String(filter.strength * 100, 0) + "% depth", juce::dontSendNotification);
    for (int amount : {25, 50, 75, 100}) if (std::abs(filter.strength * 100 - amount) < 1e-7) transitionFilterDepth.setSelectedId(amount, juce::dontSendNotification);
    transitionFilterDepth.setEnabled(editable && filter.mode != TransitionFilterMode::none);
    transitionLock.setEnabled(link && !busy && !engine.exporting && !engine.project.hasPendingPlacement());
    transitionLock.setButtonText(link && link->movementLocked ? "Unlock pair" : "Lock pair");
    transitionLock.setToggleState(link && link->movementLocked, juce::dontSendNotification);
    transitionProtection.setEnabled(link && !busy && !engine.exporting && !engine.project.hasPendingPlacement());
    transitionProtection.setToggleState(link && link->locked,juce::dontSendNotification);
    previousTransition.setEnabled(!engine.project.transitionLinks.empty()); nextTransition.setEnabled(!engine.project.transitionLinks.empty());
    transitionPresetForSelection.setSelectedId(0, juce::dontSendNotification);
    selectedTransitionLength.setSelectedId(0, juce::dontSendNotification);
    for (size_t band = 0; band < 4; ++band)
    {
        const auto setting = transitionNodeOptions[band];
        transitionCurveShow[band].setToggleState(transitionCurveShown[band], juce::dontSendNotification);
        transitionCurveOnly[band].setToggleState(transitionCurveOnlyBand == (int) band, juce::dontSendNotification);
        transitionCurveShow[band].setEnabled(link && !busy); transitionCurveOnly[band].setEnabled(link && !busy);
        transitionBandModes[band].setSelectedId(link ? (int) setting.mode + 1 : 0, juce::dontSendNotification);
        transitionBandModes[band].setEnabled(editable);
        transitionBandStrengths[band].setSelectedId(0, juce::dontSendNotification);
        transitionBandStrengths[band].setText(juce::String(setting.strength * 100, 0) + "% strength", juce::dontSendNotification);
        for (int amount : {25, 50, 75, 100}) if (std::abs(setting.strength * 100 - amount) < 1e-7) transitionBandStrengths[band].setSelectedId(amount, juce::dontSendNotification);
        transitionBandStrengths[band].setEnabled(editable && setting.generated());
        transitionBandGaps[band].setSelectedId(0, juce::dontSendNotification);
        transitionBandGaps[band].setText("Gap " + juce::String(setting.gapBeats, 1) + " beats", juce::dontSendNotification);
        int id = 1; for (double gap : {0., .5, 1., 2., 4., 8.}) { if (std::abs(gap - setting.gapBeats) < 1e-7) transitionBandGaps[band].setSelectedId(id, juce::dontSendNotification); ++id; }
        transitionBandGaps[band].setEnabled(editable && setting.swapping());
        transitionBandPositions[band].setSelectedId(0, juce::dontSendNotification);
        transitionBandPositions[band].setText("Swap at " + juce::String(setting.position * 100, 0) + "%", juce::dontSendNotification);
        for (int amount : {25, 50, 75}) if (std::abs(setting.position * 100 - amount) < 1e-7) transitionBandPositions[band].setSelectedId(amount, juce::dontSendNotification);
        transitionBandPositions[band].setEnabled(editable && setting.swapping());
    }
    selectedTransitionLength.setTooltip(link ? transitionKeySummary(*link) : "Select a transition frame or use Previous / Next.");
    if (link)
    {
        const auto range = engine.project.transitionRange(*link);
        const auto map = engine.project.gridClock();
        const double bars = (map.beatsAt(range.getEnd()) - map.beatsAt(range.getStart())) / 4;
        for (int count : {4, 8, 16, 32}) if (std::abs(bars - count) < 1e-6) selectedTransitionLength.setSelectedId(count, juce::dontSendNotification);
        if (selectedTransitionLength.getSelectedId() == 0) selectedTransitionLength.setText(juce::String(bars, 2) + " bars (custom)", juce::dontSendNotification);
    }
}
}
