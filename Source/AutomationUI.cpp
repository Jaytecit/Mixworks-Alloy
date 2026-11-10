#include "Workstation.h"
#include "UiMessages.h"
#include "PresetNodes.h"
#include <cstdlib>
#include <cmath>

namespace mixworks
{
namespace
{
juce::String editorTargetName(AutomationTarget target)
{
    return target == AutomationTarget::treble ? "Treble" : juce::String(automationName(target));
}
juce::String automationEditReason(const Engine& engine,const AutomationAddress& address,bool busy,bool preparing,bool pluginAvailable)
{
    if(busy || preparing)return "Wait for source and tempo preparation before editing automation.";
    if(engine.exporting)return "Finish or cancel export before editing automation.";
    if(engine.rehearsalActive())return "End the transition rehearsal before editing automation.";
    if(engine.recordingAutomation())return "Finish automation recording or choose Read before editing saved nodes.";
    if(engine.project.automationLocked(address))return "Unlock the participating transition before editing this curve.";
    if(address.target==AutomationTarget::master)return {};
    if(address.clipId.isEmpty() && address.target!=AutomationTarget::plugin)return "Select a track to edit its automation.";
    if(!engine.project.settings(address))return "Select an available track, mixer lane or master curve before editing nodes.";
    if(const auto* clip=engine.project.clipFor(address.clipId))
    {
        if(!clip->media || !clip->media->hasPlayback())return "Relink or reload this track's missing audio before editing its curve.";
        if(clip->media->sourceDetailsPending || clip->pendingPlacement!=PendingPlacement::none)return "Wait for this source to finish loading before editing its curve.";
    }
    if(address.target==AutomationTarget::plugin && !pluginAvailable)return "Load the effect and select an available parameter before editing its curve.";
    return {};
}
}

juce::AudioProcessorParameter* Workstation::automationPluginParameter(const AutomationAddress& address) const
{
    if (address.target != AutomationTarget::plugin || address.track < 0 || address.track >= trackCount) return nullptr;
    const auto* clip = engine.project.clipFor(address.clipId);
    if (address.clipId.isNotEmpty() && !clip) return nullptr;
    const auto& chain = clip ? clip->effects->plugins : engine.plugins[(size_t) address.track];
    for (const auto& slot : chain) if (slot->id == address.slotId)
    {
        const auto found = slot->parameters.find(address.parameterId);
        return found == slot->parameters.end() ? nullptr : found->second;
    }
    return nullptr;
}

bool Workstation::automationEditable(const AutomationAddress& address) const
{
    return automationEditReason(engine,address,busy,pendingTempo>0 || tempoPreparationPending,automationPluginParameter(address)!=nullptr).isEmpty();
}

Project Workstation::automationEditSnapshot()
{
    const juce::ScopedLock guard(engine.lock); return engine.project;
}
bool Workstation::publishAutomationEdit(Project candidate,const AutomationAddress& address,bool addUndo)
{
    if (const auto why=automationEditReason(engine,address,busy,pendingTempo>0 || tempoPreparationPending,automationPluginParameter(address)!=nullptr);why.isNotEmpty())
    { setStatus(why); return false; }
    const auto protectedEdit=engine.project.validateTransitionLocks(candidate);
    if (protectedEdit.failed()) { setStatus(protectedEdit.getErrorMessage()); return false; }
    if (addUndo) { engine.setAutomationRecordMode(AutomationRecordMode::read); remember(); }
    // Preparation, history/UI notification and retirement stay outside the
    // callback lock. Preserve take events appended while the candidate was built.
    { const juce::ScopedLock guard(engine.lock); candidate.performanceTakes.swap(engine.project.performanceTakes); std::swap(candidate,engine.project); }
    dirty=true; return true;
}

AutomationCurve* Workstation::materialiseAutomation(Project& candidate, const AutomationAddress& address)
{
    const auto converted = materialiseEditablePresetNodes(candidate,address);
    if (converted.failed()) { setStatus(converted.getErrorMessage()); return nullptr; }
    if (auto* curve = candidate.curve(address)) return curve;
    auto* parameter = automationPluginParameter(address); auto* settings = candidate.settings(address);
    if (!parameter || !settings) return nullptr;
    settings->pluginAutomation.push_back({address.slotId, address.parameterId, parameter->getName(128), parameter->getValue(), {}});
    return candidate.curve(address);
}

bool Workstation::insertAutomationNode(const AutomationAddress& address,double time,bool snap)
{
    if (const auto why=automationEditReason(engine,address,busy,pendingTempo>0 || tempoPreparationPending,automationPluginParameter(address)!=nullptr);why.isNotEmpty())
    { setStatus(why); return false; }
    if (!std::isfinite(time)) { setStatus("Choose a valid timeline position before adding a node."); return false; }
    engine.finishAutomation(); auto candidate=automationEditSnapshot();
    time=candidate.snappedTime(time,snap && candidate.snap);
    const auto* clip=candidate.clipFor(address.clipId); const auto bounds=candidate.automationBounds(address);
    if (time<0 || time>1.e9 || (clip && (time<bounds.getStart() || time>bounds.getEnd())))
    { setStatus("Place the node inside its automation span. Effect curves include all split pieces, gaps and the effect ending."); return false; }
    auto* curve=materialiseAutomation(candidate,address); if (!curve) return false;
    const double local=candidate.automationTime(address,time); const float value=curve->interpolatedValue(local,candidate.manualValue(address));
    int index=-1; bool existing=false;
    if (curve->enabled) for (int i=0;i<(int)curve->points.size();++i) if (curve->points[(size_t)i].time==local) { index=i; existing=true; break; }
    if (!existing)
    { candidate.automationEdited(address); curve->enabled=true; index=curve->setPoint(local,value); if (!publishAutomationEdit(std::move(candidate),address,true)) return false; }
    timeline.selectAutomationPoint(address,index); refresh(); setStatus(existing ? "Existing node selected." : "Node added."); return true;
}
bool Workstation::deleteAutomationNode(const AutomationAddress& address,int index)
{
    if (const auto why=automationEditReason(engine,address,busy,pendingTempo>0 || tempoPreparationPending,automationPluginParameter(address)!=nullptr);why.isNotEmpty())
    { setStatus(why); return false; }
    auto candidate=automationEditSnapshot(); auto* curve=materialiseAutomation(candidate,address);
    if (!curve || index<0 || index>=(int)curve->points.size()) { setStatus("Select an available automation node to delete."); return false; }
    candidate.automationEdited(address,AutomationEdit::removePoint,curve->points[(size_t)index].time);
    curve->points.erase(curve->points.begin()+index);
    if (!publishAutomationEdit(std::move(candidate),address,true)) return false;
    timeline.editingAutomationPoint.reset(); refresh(); setStatus("Node deleted."); return true;
}

void Workstation::showAutomationNodeMenu(const AutomationAddress& address, int index, double time, juce::Rectangle<int> anchor)
{
    const auto view = timeline.automationViewCurve(address); const auto* saved = &view; const bool editable = automationEditable(address);
    const auto before = juce::JSON::toString(projectToJson(engine.project));
    const auto reason=automationEditReason(engine,address,busy,pendingTempo>0 || tempoPreparationPending,automationPluginParameter(address)!=nullptr);
    const auto label=[&](const juce::String& name) {return name+(reason.isEmpty() ? juce::String() : " — "+reason);};
    juce::PopupMenu menu;
    if (index >= 0 && index < (int) saved->points.size())
    {
        menu.addItem(1, label("Edit time / value..."), editable);
        menu.addItem(2, label("Delete node"), editable);
    }
    menu.addItem(3, label("Add node here"), editable);
    menu.addItem(4, "Automation details...");
    menu.showMenuAsync(timelineMenuOptions(anchor),
        [safe = juce::Component::SafePointer<Workstation>(this), address, index, time, before](int result)
        {
            if (!safe || result == 0) return;
            if (result != 4 && !safe->automationEditable(address))
            { safe->setStatus(automationEditReason(safe->engine,address,safe->busy,safe->pendingTempo>0 || safe->tempoPreparationPending,safe->automationPluginParameter(address)!=nullptr)); return; }
            if (before != juce::JSON::toString(projectToJson(safe->engine.project))) { safe->setStatus("The curve changed while the menu was open. Select it again."); return; }
            safe->timeline.selectAutomationCurve(address); safe->timeline.selectAutomationPoint(address, index);
            if (result == 1) safe->editAutomationPoint(address.target == AutomationTarget::master ? 0 : address.track, index);
            else if (result == 2) safe->deleteAutomationNode(address, index);
            else if (result == 3) safe->insertAutomationNode(address, time);
            else if (result == 4) safe->showAutomationDetails();
        });
}

void Workstation::showAutomationDetails()
{
    showEditingPage(2);
    showToolPage(2);
    refresh();
}

void Workstation::initialiseAutomationEditor()
{
    for (auto* c : std::initializer_list<juce::Component*> {&automationHeading, &automationOwner, &automationParameter, &automationState, &automationNode,
        &automationAdd, &automationPrevious, &automationNext, &automationEdit, &automationDelete, &automationRead, &automationMode, &automationPairedFilter})
        editingBody.addAndMakeVisible(c);
    editingBody.addChildComponent(automationArm); // Capture exclusions live in the advanced take menu.
    automationHeading.setText("AUTOMATION", juce::dontSendNotification);
    automationOwner.setTitle("Automation owner"); automationParameter.setTitle("Automation parameter"); automationMode.setTitle("Automation recording mode");
    automationOwner.setTextWhenNothingSelected("Select automation owner"); automationParameter.setTextWhenNothingSelected("Select parameter");
    automationOwner.setTooltip("Choose the track, stem, master or current effect. Track curves include preset and manual nodes across the track.");
    automationParameter.setTooltip("Select Volume, Bass, Mid, Treble, Pan, Filter or a named effect parameter. Selection is view-only.");
    automationMode.addItem("Read mode", 1); automationMode.addItem("Touch", 2); automationMode.addItem("Latch", 3);
    automationMode.setTooltip("Read plays saved automation. Touch writes to the playing track's curve on an armed lane, then returns to the curve after 0.5 seconds without input. Latch keeps writing until the track ends, Stop or Read.");
    automationRead.setTooltip("Enable or disable this owner's automation nodes. Preset nodes use the same Read control. Undo restores the change.");
    automationAdd.setTooltip("Add a point at the playhead for the selected control. Snap applies.");
    automationPrevious.setTooltip("Select the previous node without seeking; Left on the timeline does the same.");
    automationNext.setTooltip("Select the next node without seeking; Right on the timeline does the same.");
    automationEdit.setTooltip("Set the selected point's exact time and value. With no point selected, this adds one at the playhead.");
    automationDelete.setTooltip("Delete the selected automation node, including a point placed by a preset. One Undo restores it.");
    automationArm.setTooltip("Arm lets this mixer lane record moved controls during playback in Touch or Latch. It does not start playback or writing. Disarm to protect the lane; saved curves still play in Read.");
    automationPairedFilter.setButtonText("Filters...");
    automationPairedFilter.setTooltip("Choose a transition filter preset, then edit its regular Filter nodes in this same editor.");
    automationOwner.onChange = [this]
    {
        if (refreshing) return; const int i = automationOwner.getSelectedItemIndex();
        if (i < 0 || i >= (int) automationOwnerChoices.size()) return;
        auto address = automationOwnerChoices[(size_t) i];
        if (address.target == AutomationTarget::plugin) return; // The active mixer effect already identifies its parameter.
        selectedStem = address.stem;
        address.target = address.target == AutomationTarget::master ? AutomationTarget::master
            : automationLane() >= 0 && automationLane() < trackAutomationCount ? (AutomationTarget) automationLane() : AutomationTarget::volume;
        if (address.stem >= 0 && address.target != AutomationTarget::volume && address.target != AutomationTarget::pan) address.target = AutomationTarget::volume;
        timeline.selectAutomationCurve(address); refresh();
    };
    automationParameter.onChange = [this]
    {
        if (refreshing) return; const int i = automationParameter.getSelectedItemIndex();
        if (i < 0 || i >= (int) automationParameterChoices.size()) return;
        auto address = automationParameterChoices[(size_t) i];
        selectedStem = address.stem;
        transitionCurveOnlyBand = -1; if ((int) address.target < 4) transitionCurveShown[(size_t) address.target] = true;
        if (address.stem >= 0 && address.target != AutomationTarget::volume && address.target != AutomationTarget::pan) address.target = AutomationTarget::volume;
        timeline.selectAutomationCurve(address); refresh();
    };
    automationAdd.onClick = [this] { if (automationLane() >= 0) insertAutomationNode(displayedAddress(engine.selectedTrack), engine.position.load()); };
    automationPrevious.onClick = [this] { timeline.keyPressed(juce::KeyPress(juce::KeyPress::leftKey)); refreshAutomationEditor(); };
    automationNext.onClick = [this] { timeline.keyPressed(juce::KeyPress(juce::KeyPress::rightKey)); refreshAutomationEditor(); };
    automationEdit.onClick = [this]
    {
        const auto address = displayedAddress(engine.selectedTrack);
        const int index = timeline.editingAutomationPoint && timeline.editingAutomationPoint->first == address ? timeline.editingAutomationPoint->second : -1;
        editAutomationPoint(engine.selectedTrack, index);
    };
    automationDelete.onClick = [this] { if (timeline.editingAutomationPoint) { const auto [address, index] = *timeline.editingAutomationPoint; deleteAutomationNode(address, index); } };
    automationRead.onClick = [this] { performTimelineAction(TimelineAction::curveRead, engine.position.load(), engine.selectedTrack); };
    automationMode.onChange = [this]
    { if (!refreshing) performTimelineAction(automationMode.getSelectedId() == 2 ? TimelineAction::recordTouch : automationMode.getSelectedId() == 3 ? TimelineAction::recordLatch : TimelineAction::recordOff, engine.position.load(), engine.selectedTrack); };
    automationArm.onClick = [this] { performTimelineAction(TimelineAction::armAutomation, engine.position.load(), engine.selectedTrack); };
    automationPairedFilter.onClick = [this]
    {
        const auto address = displayedAddress(engine.selectedTrack);
        const auto* selected = engine.project.transitionFor(selectedTransition);
        if (!selected || (selected->incoming != address.clipId && selected->outgoing != address.clipId))
            for (const auto& link : engine.project.transitionLinks) if (link.incoming == address.clipId || link.outgoing == address.clipId) { selected = &link; break; }
        if (selected) { const auto id = selected->incoming; selectTransition(id); showToolPage(1); setStatus("Choose a filter preset in Transitions. Its nodes appear on each track's Filter curve."); }
    };
}

void Workstation::refreshAutomationEditor(bool rebuildChoices)
{
    const auto address = displayedAddress(engine.selectedTrack);
    if (rebuildChoices)
    {
        automationOwner.clear(juce::dontSendNotification); automationOwnerChoices.clear();
        int ownerIndex = 0;
        auto addOwner = [&](const juce::String& name, AutomationAddress choice)
        {
            automationOwnerChoices.push_back(choice); const int id = (int) automationOwnerChoices.size(); automationOwner.addItem(name, id);
            if ((address.target == AutomationTarget::master && choice.target == AutomationTarget::master)
                || (address.target != AutomationTarget::master && choice.target != AutomationTarget::master && choice.track == address.track && choice.clipId == address.clipId && choice.stem == address.stem)) ownerIndex = id;
        };
        if (const auto* clip = currentClip()) addOwner("Track / " + clip->media->libraryTitle(), {clip->track, AutomationTarget::volume, {}, {}, clip->id});
        addOwner("Master output", {-1, AutomationTarget::master});
        if (address.target == AutomationTarget::plugin && address.clipId.isEmpty())
            addOwner("Mixer lane " + juce::String(address.track + 1) + " FX", address);
        for (const auto& clip : engine.project.clips) if (clip.stems) for (int s = 0; s < stemCount; ++s)
            addOwner(clip.media->libraryTitle() + " / " + stemNames[s], {clip.track, AutomationTarget::volume, {}, {}, clip.id, s});
        automationOwner.setSelectedId(ownerIndex, juce::dontSendNotification);
        automationParameter.clear(juce::dontSendNotification); automationParameterChoices.clear();
        auto addParameter = [&](const juce::String& name, AutomationAddress choice)
        {
            automationParameterChoices.push_back(choice); const int id = (int) automationParameterChoices.size(); automationParameter.addItem(name, id);
            if (automationLane() >= 0 && choice == address) automationParameter.setSelectedId(id, juce::dontSendNotification);
        };
        if (address.target == AutomationTarget::master) addParameter("Master volume", address);
        else if (address.track >= 0 && address.track < trackCount)
        {
            for (int target = 0; address.clipId.isNotEmpty() && target < trackAutomationCount; ++target)
            { if (address.stem >= 0 && target != 0 && target != 4) continue; auto choice = address; choice.target = (AutomationTarget) target; choice.slotId.clear(); choice.parameterId.clear(); addParameter(target == (int) AutomationTarget::cutoff ? "Filter" : editorTargetName(choice.target), choice); }
            const auto* clip = engine.project.clipFor(address.clipId); const auto& chain = clip ? clip->effects->plugins : engine.plugins[(size_t) address.track];
            for (size_t i = 0; address.stem < 0 && i < chain.size(); ++i) for (const auto& [id, parameter] : chain[i]->parameters)
                addParameter("FX " + juce::String((int) i + 1) + " / " + chain[i]->description.name + " / " + parameter->getName(128), {address.track, AutomationTarget::plugin, chain[i]->id, id, address.clipId});
        }
    }
    const bool selected = automationLane() >= 0;
    const auto* saved = selected ? engine.project.curve(address) : nullptr;
    const auto view = selected ? timeline.automationViewCurve(address) : AutomationCurve {};
    const bool generated = selected && timeline.hasGeneratedAutomation(address), editable = selected && automationEditable(address);
    const bool node = selected && timeline.editingAutomationPoint && timeline.editingAutomationPoint->first == address
        && timeline.editingAutomationPoint->second >= 0 && timeline.editingAutomationPoint->second < (int) view.points.size();
    juce::String state = !selected ? "Select a curve"
        : juce::String(timeline.automationVisible(address) ? "Shown" : "Hidden") + " / "
            + (saved && !saved->points.empty() ? saved->enabled ? "On" : "Off" : "No nodes")
            + (generated ? " / Preset" : "") + " / " + juce::String((int) view.points.size()) + " nodes";
    if (selected && engine.project.automationLocked(address)) state += " / locked";
    if (engine.recordingAutomation()) state += " / Recording";
    automationState.setText(state, juce::dontSendNotification); automationState.setTooltip(state);
    juce::String detail = "No node selected";
    if (node)
    {
        const int i = timeline.editingAutomationPoint->second; const auto& point = view.points[(size_t) i];
        const double time = engine.project.automationSessionTime(address, point.time);
        const double beats = engine.project.tempo.beatsAt(time) - engine.project.tempo.beatsAt(engine.project.tempo.origin);
        detail = "Node " + juce::String(i + 1) + "/" + juce::String((int) view.points.size()) + " / project bar "
            + juce::String((int) std::floor(beats / 4) + 1) + " beat " + juce::String(beats - std::floor(beats / 4) * 4 + 1, 2)
            + " / " + juce::String(time - engine.project.tempo.origin, 6) + " s / " + automationText(address.target, point.value);
        if (address.target == AutomationTarget::plugin) if (auto* parameter = automationPluginParameter(address)) detail += " / " + parameter->getText(point.value, 64) + " " + parameter->getLabel();
        if (generated) detail += " / Preset";
    }
    detail += " / recording: " + juce::String(engine.automationRecordMode == AutomationRecordMode::read ? "Read" : engine.automationRecordMode == AutomationRecordMode::touch ? "Touch" : "Latch")
        + (engine.recordingAutomation() ? " (writing)" : " (idle)");
    automationNode.setText(node ? "Node " + juce::String(timeline.editingAutomationPoint->second + 1) + "/" + juce::String((int) view.points.size()) : "No node selected", juce::dontSendNotification); automationNode.setTooltip(detail);
    automationAdd.setEnabled(editable); automationEdit.setEnabled(editable);
    automationPrevious.setEnabled(selected && !busy && !view.points.empty()); automationNext.setEnabled(automationPrevious.isEnabled());
    automationDelete.setEnabled(editable && node);
    automationRead.setEnabled(editable && !view.points.empty()); automationRead.setToggleState(view.enabled, juce::dontSendNotification);
    const auto unavailable=!selected ? juce::String("Select a curve owner and parameter before editing automation.")
        : automationEditReason(engine,address,busy,pendingTempo>0 || tempoPreparationPending,automationPluginParameter(address)!=nullptr);
    const auto explain=[&](juce::Component& control,const juce::String& fallback)
    {
        const auto reason=control.isEnabled() ? juce::String() : unavailable.isNotEmpty() ? unavailable : fallback;
        control.getProperties().set("mixworksHintDisabled",reason); control.setDescription(reason);
    };
    explain(automationAdd,{}); explain(automationEdit,{});
    explain(automationDelete,"Select an available automation node before deleting it.");
    explain(automationRead,"Add a node before changing this curve's Read state.");
    explain(automationPrevious,"This curve has no nodes to navigate. Add one first.");
    explain(automationNext,"This curve has no nodes to navigate. Add one first.");
    automationMode.setSelectedId(engine.automationRecordMode == AutomationRecordMode::touch ? 2 : engine.automationRecordMode == AutomationRecordMode::write ? 3 : 1, juce::dontSendNotification);
    automationMode.setEnabled(!busy && !engine.exporting && address.stem < 0);
    const bool master = selected && address.target == AutomationTarget::master;
    const bool armed = master ? engine.masterAutomationArmed : engine.automationArmed[(size_t) engine.selectedTrack.load()];
    const int armedLanes = (int) std::count(engine.automationArmed.begin(), engine.automationArmed.end(), true);
    automationArm.setTooltip("Armed scope: " + juce::String(armedLanes) + " lanes; master " + (engine.masterAutomationArmed ? "armed" : "off")
        + ". This control arms this owner's lane, including its segments. Controller Record arms all four lanes and master.");
    automationArm.setButtonText(master ? "Arm master" : "Arm lane " + juce::String(engine.selectedTrack.load() + 1)); automationArm.setToggleState(armed, juce::dontSendNotification);
    automationArm.setEnabled(!busy && !engine.exporting && address.stem < 0);
    const auto mode = engine.automationRecordMode;
    const auto name = mode == AutomationRecordMode::read ? "Read" : mode == AutomationRecordMode::touch ? "Touch" : "Latch";
    recordingTools.setButtonText(juce::String(name) + " / " + juce::String(armedLanes) + " lanes armed" + (engine.masterAutomationArmed ? " + master" : ""));
    recordingTools.setTooltip("Automation recording mode and arm state. Open to change mode or arm the selected lane or master.");
    automationPairedFilter.setEnabled(selected && address.clipId.isNotEmpty() && std::any_of(engine.project.transitionLinks.begin(), engine.project.transitionLinks.end(), [&](const auto& link) { return link.incoming == address.clipId || link.outgoing == address.clipId; }));
}

juce::PopupMenu Workstation::automationMenu(int track) const
{
    juce::PopupMenu menu;
    const auto address = displayedAddress(track);
    const auto view = automationLane() >= 0 ? timeline.automationViewCurve(address) : AutomationCurve{};
    const auto* curve = &view;
    const bool available = !busy && !engine.exporting;
    const bool editable = automationLane() >= 0 && automationEditable(address);
    auto add = [&](TimelineAction action, const juce::String& label, bool enabled, bool ticked = false)
    { menu.addItem((int) action, label, enabled, ticked); };
    menu.addSectionHeader(automationLane() >= 0 ? displayedAutomationName() : "Select a point to edit");
    add(TimelineAction::curveRead, "Read curve nodes", editable && !curve->points.empty(), curve->enabled);
    add(TimelineAction::curvePoint, "Add / edit node here...", editable);
    add(TimelineAction::curveClear, "Clear curve nodes", editable && !curve->points.empty());
    add(TimelineAction::choosePlugin, "Choose effect control...", available);
    menu.addSeparator();
    menu.addSubMenu("Recording options", automationRecordingMenu(track, automationLane() == (int) AutomationTarget::master));
    return menu;
}

juce::PopupMenu Workstation::automationRecordingMenu(int track, bool master) const
{
    juce::PopupMenu recording;
    const bool available = !busy && !engine.exporting;
    recording.addItem((int) TimelineAction::recordOff, "Read (no recording)", available, engine.automationRecordMode == AutomationRecordMode::read);
    recording.addItem((int) TimelineAction::recordTouch, "Touch", available, engine.automationRecordMode == AutomationRecordMode::touch);
    recording.addItem((int) TimelineAction::recordLatch, "Latch", available, engine.automationRecordMode == AutomationRecordMode::write);
    recording.addSeparator(); recording.addItem((int) TimelineAction::armAutomation, master ? "Arm master" : "Arm lane " + juce::String(track + 1), available,
        master ? engine.masterAutomationArmed : engine.automationArmed[(size_t) track]);
    return recording;
}

void Workstation::showAutomationRecordingMenu()
{
    const int track = engine.selectedTrack.load();
    const bool master = automationLane() == (int) AutomationTarget::master;
    automationRecordingMenu(track, master).showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&recordingTools),
        [safe = juce::Component::SafePointer<Workstation>(this), track, master](int id)
        {
            if (safe && id && (id != (int) TimelineAction::armAutomation || (track == safe->engine.selectedTrack && master == (safe->automationLane() == (int) AutomationTarget::master))))
                safe->performTimelineAction((TimelineAction) id, safe->engine.position.load(), track);
        });
}
std::vector<AutomationAddress> Workstation::automationAddresses(int track) const
{
    std::vector<AutomationAddress> addresses;
    auto add = [&](const TrackSettings& settings, const juce::String& clipId)
    {
        for (int target = 0; clipId.isNotEmpty() && target < trackAutomationCount; ++target)
            if (!settings.automation[(size_t) target].points.empty()) addresses.push_back({track, (AutomationTarget) target, {}, {}, clipId});
        for (const auto& lane : settings.pluginAutomation)
            if (!lane.curve.points.empty()) addresses.push_back({track, AutomationTarget::plugin, lane.slotId, lane.parameterId, clipId});
    };
    for (const auto& clip : engine.project.clips) if (clip.track == track && clip.stems && clip.stemsExpanded)
        for (int s = 0; s < stemCount; ++s) for (int target : {0, 4})
            if (!clip.stemControls[(size_t) s].automation[(size_t) target].points.empty())
                addresses.push_back({track, (AutomationTarget) target, {}, {}, clip.id, s});
    add(engine.project.tracks[(size_t) track], {});
    for (const auto& clip : engine.project.clips) if (clip.track == track)
    {
        auto settings = engine.project.effectSettings(clip); settings.pluginAutomation.clear(); add(settings, clip.id);
        const auto* effectOwner = engine.project.effectOwner(clip);
        const bool firstOnLane = std::none_of(engine.project.clips.begin(), engine.project.clips.begin() + (&clip - engine.project.clips.data()),
            [&](const auto& other) { return other.track == track && other.effects == clip.effects; });
        if (firstOnLane) for (const auto& lane : engine.project.effectSettings(*effectOwner).pluginAutomation)
            if (!lane.curve.points.empty()) addresses.push_back({track, AutomationTarget::plugin, lane.slotId, lane.parameterId, clip.id});
    }
    if (track == 0 && !engine.project.masterAutomation.points.empty()) addresses.push_back({-1, AutomationTarget::master});
    return addresses;
}
void Workstation::editTempoPoint(int point, double time)
{
    if (busy || engine.exporting || pendingTempo > 0 || tempoPreparationPending) return;
    if (engine.project.hasLockedTransitions()) { setStatus("Unlock transitions before changing tempo."); return; }
    const auto& map = engine.project.tempo;
    if (point >= (int) map.points.size()) return;
    if (point >= 0) time = map.points[(size_t) point].time;
    const double bpm = point >= 0 ? map.points[(size_t) point].bpm : map.points.empty() ? engine.project.bpm : map.bpmAt(time);
    const auto expected = juce::JSON::toString(projectToJson(engine.project), true, 17);
    auto* dialog = new juce::AlertWindow("Tempo marker", "Set the speed here. Drag the marker to move it.", juce::MessageBoxIconType::NoIcon, this);
    dialog->addTextEditor("time", juce::String(time - engine.project.tempo.origin, 6), "Timeline seconds (negative is pre-roll)");
    const auto bpmText = preciseBpmText(bpm);
    dialog->addTextEditor("bpm", bpmText, "Speed (BPM, 20-400)");
    dialog->addButton("Apply", 1, juce::KeyPress(juce::KeyPress::returnKey)); dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    dialog->enterModalState(true, juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<Workstation>(this), dialog, point, expected, bpm, bpmText](int answer)
    {
        if (!safe || answer != 1 || safe->busy) return;
        if (safe->engine.exporting || safe->pendingTempo > 0 || safe->tempoPreparationPending || safe->engine.project.hasLockedTransitions()
            || expected != juce::JSON::toString(projectToJson(safe->engine.project), true, 17)
            || point >= (int) safe->engine.project.tempo.points.size())
        { safe->setStatus("The project changed or is protected while tempo entry was open. Select the marker again."); return; }
        auto number = [](const juce::String& text, double& value) { const auto raw = text.trim().toStdString(); char* end = nullptr; value = std::strtod(raw.c_str(), &end); return !raw.empty() && end == raw.c_str() + raw.size() && std::isfinite(value); };
        double seconds = 0, target = 0;
        const bool validTime = number(dialog->getTextEditorContents("time"), seconds); seconds += safe->engine.project.tempo.origin;
        if (!validTime || !number(dialog->getTextEditorContents("bpm"), target) || seconds < 0 || seconds > 1.0e9 || target < 20 || target > 400)
        { safe->fail("Enter a valid time and BPM between 20 and 400. The tempo map was kept."); return; }
        if (dialog->getTextEditorContents("bpm").trim() == bpmText) target = bpm;
        auto candidate = safe->engine.project; auto& tempo = candidate.tempo;
        if (tempo.points.empty()) tempo.setPoint(0, candidate.bpm);
        if (point >= 0)
        {
            if ((point == 0 && seconds != 0) || (point > 0 && (seconds < tempo.points[(size_t) point - 1].time || (seconds == tempo.points[(size_t) point - 1].time && tempo.points[(size_t) point].clipId.isEmpty() && tempo.points[(size_t) point - 1].clipId.isEmpty()))) || (point + 1 < (int) tempo.points.size() && (seconds > tempo.points[(size_t) point + 1].time || (seconds == tempo.points[(size_t) point + 1].time && tempo.points[(size_t) point].clipId.isEmpty() && tempo.points[(size_t) point + 1].clipId.isEmpty()))))
            { safe->fail("Keep the first marker at zero and other markers between their neighbours."); return; }
            tempo.movePoint(point, seconds, target); safe->tempoStrip.selectedTempoPoint = point;
        }
        else safe->tempoStrip.selectedTempoPoint = tempo.setPoint(seconds, target);
        safe->prepareTempo(std::move(candidate), false);
    }), true);
}
AutomationAddress Workstation::displayedAddress(int track) const
{
    const auto target = (AutomationTarget) std::max(0, automationLane());
    if (target == AutomationTarget::plugin) { auto address = pluginLane; if (track != address.track) address.track = -2; return address; }
    const auto* clip = engine.controlClip(track);
    const int stem = clip && clip->stems && (target == AutomationTarget::volume || target == AutomationTarget::pan) ? selectedStem : -1;
    const auto id = target != AutomationTarget::master && clip && (stem >= 0 || automationClip == selectedClip) ? selectedClip : juce::String();
    return {target == AutomationTarget::master ? -1 : track, target, {}, {}, id, stem};
}
AutomationCurve* Workstation::displayedCurve(int track)
{
    if (automationLane() < 0 || (automationLane() == (int) AutomationTarget::master && track != 0)) return nullptr;
    return engine.project.curve(displayedAddress(track));
}
juce::String Workstation::automationText(AutomationTarget target, float value) const
{
    const auto range = automationRange(target); value = range.clipValue(value);
    if (target == AutomationTarget::cutoff) return cutoffText(value, true);
    const float level = target == AutomationTarget::pan || target == AutomationTarget::plugin ? value
        : value <= range.getStart() ? 0 : juce::Decibels::decibelsToGain(value);
    return juce::String(juce::roundToInt(level * 100)) + "%";
}
juce::String Workstation::displayedAutomationName() const
{
    if (automationLane() != (int) AutomationTarget::plugin)
    {
        const auto address = displayedAddress(engine.selectedTrack);
        return (address.stem >= 0 && address.stem < stemCount ? juce::String(stemNames[address.stem]) + " / " : juce::String())
            + automationName((AutomationTarget) std::max(0, automationLane()));
    }
    if (auto* parameter = automationPluginParameter(pluginLane))
        for (const auto& slot : (engine.project.clipFor(pluginLane.clipId) ? engine.project.clipFor(pluginLane.clipId)->effects->plugins : engine.plugins[(size_t) pluginLane.track]))
            if (slot->id == pluginLane.slotId) return slot->description.name + " / " + parameter->getName(128);
    return "Choose a VST parameter";
}
void Workstation::chooseAutomationPlugin(std::optional<double> markerTime)
{
    if (busy || engine.exporting) return; const int track = engine.selectedTrack;
    juce::StringArray choices; std::vector<AutomationAddress> addresses;
    const auto& chain = engine.pluginChain(track);
    for (int insert = 0; insert < (int) chain.size(); ++insert)
        for (const auto& [id, parameter] : chain[(size_t) insert]->parameters)
        {
            choices.add(juce::String(insert + 1) + ". " + chain[(size_t) insert]->description.name + " / " + parameter->getName(128));
            addresses.push_back({track, AutomationTarget::plugin, chain[(size_t) insert]->id, id, engine.controlClip(track) ? selectedClip : juce::String()});
        }
    if (choices.isEmpty()) { setStatus("Add a VST3 with automatable parameters to the selected track first."); return; }
    auto* dialog = new juce::AlertWindow("Effect control", "Choose an effect control to change over time.", juce::MessageBoxIconType::NoIcon, this);
    dialog->addComboBox("parameter", choices, "Parameter"); dialog->addButton(markerTime ? "Add node here" : "Show curve", 1, juce::KeyPress(juce::KeyPress::returnKey)); dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    dialog->enterModalState(true, juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<Workstation>(this), dialog, addresses, markerTime](int result)
    {
        if (!safe || result != 1) return; const int choice = dialog->getComboBoxComponent("parameter")->getSelectedItemIndex();
        if (choice < 0 || choice >= (int) addresses.size()) return;
        const auto address = addresses[(size_t) choice];
        if (safe->busy || safe->engine.exporting || !safe->automationPluginParameter(address)) { safe->setStatus("This plugin parameter is no longer available."); return; }
        safe->timeline.selectAutomationCurve(address);
        if (markerTime) { safe->insertAutomationNode(address, *markerTime); return; }
        safe->refresh(); safe->setStatus("Effect selected.");
    }), true);
}
void Workstation::editAutomationPoint(int track, int point, std::optional<double> at)
{
    if(automationLane()<0) { setStatus("Select a curve owner and parameter before editing a node."); return; }
    const auto address = displayedAddress(track);
    if(const auto why=automationEditReason(engine,address,busy,pendingTempo>0 || tempoPreparationPending,automationPluginParameter(address)!=nullptr);why.isNotEmpty())
    { setStatus(why); return; }
    const auto viewCurve = timeline.automationViewCurve(address);
    if (point >= (int) viewCurve.points.size()) { setStatus("This node is no longer available. Select a current node and try again."); return; }
    const auto* curve = &viewCurve;
    const auto before = juce::JSON::toString(projectToJson(engine.project));
    const auto range = automationRange(address.target); const bool percent = address.target == AutomationTarget::pan || address.target == AutomationTarget::plugin;
    const bool gain = timeline.automationUsesGain(address.target);
    const double scale = percent ? 100 : 1;
    const double time = point >= 0 ? engine.project.automationSessionTime(address, curve->points[(size_t) point].time) : at.value_or(engine.position.load());
    const auto* parameter = automationPluginParameter(address);
    const float fallback = parameter && !engine.project.curve(address) ? parameter->getValue() : engine.project.manualValue(address);
    const float manual = point >= 0 ? curve->points[(size_t) point].value : curve->interpolatedValue(engine.project.automationTime(address, time), fallback);
    const float value = manual;
    const auto* ownerClip = engine.project.clipFor(address.clipId);
    const auto ownerName = ownerClip ? "Segment " + juce::String((int) (ownerClip - engine.project.clips.data()) + 1) + " / " + ownerClip->media->libraryTitle()
        : address.target == AutomationTarget::master ? juce::String("Master") : "Mixer lane " + juce::String(address.track + 1);
    auto* dialog = new juce::AlertWindow("Automation node", ownerName + " / " + displayedAutomationName(), juce::MessageBoxIconType::NoIcon, this);
    dialog->addTextEditor("time", juce::String(time - engine.project.tempo.origin, 6), "Timeline seconds (negative is pre-roll; bypasses snap)");
    const bool filter = address.target == AutomationTarget::cutoff;
    dialog->addTextEditor("value", filter ? cutoffText(value) : juce::String(gain ? (value <= -60 ? 0 : juce::Decibels::decibelsToGain(value) * 100) : value * scale, 3),
        filter ? "LP 20-20000 Hz / Off / HP 20-20000 Hz" : gain ? "Amount (0 to 130 %)" : "Value: " + juce::String(range.getStart() * scale) + " to " + juce::String(range.getEnd() * scale) + (percent ? " %" : " dB"));
    dialog->addButton("Apply", 1, juce::KeyPress(juce::KeyPress::returnKey)); dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    dialog->enterModalState(true, juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<Workstation>(this), dialog, address, range, scale, point, viewCurve, gain, before](int result)
    {
        if (!safe || result != 1) return;
        if(!safe->automationEditable(address))
        { safe->setStatus(automationEditReason(safe->engine,address,safe->busy,safe->pendingTempo>0 || safe->tempoPreparationPending,safe->automationPluginParameter(address)!=nullptr)); return; }
        if (before != juce::JSON::toString(projectToJson(safe->engine.project))) { safe->setStatus("The project changed while the node editor was open. Select the node again."); return; }
        auto number = [](const juce::String& text, double& value)
        {
            const auto raw = text.trim().toStdString(); char* end = nullptr; value = std::strtod(raw.c_str(), &end);
            return !raw.empty() && end == raw.c_str() + raw.size() && std::isfinite(value);
        };
        double timeValue = 0, value = 0; float filterValue = 0;
        const bool valid = address.target == AutomationTarget::cutoff ? parseCutoffText(dialog->getTextEditorContents("value"), filterValue)
            : number(dialog->getTextEditorContents("value"), value);
        if (address.target == AutomationTarget::cutoff) value = filterValue;
        const bool validTime = number(dialog->getTextEditorContents("time"), timeValue); timeValue += safe->engine.project.tempo.origin;
        if (!validTime || !valid || timeValue < 0 || timeValue > 1.0e9
            || (gain ? value < 0 || value > 130 : value / scale < range.getStart() || value / scale > range.getEnd()))
        { safe->fail("Enter a valid session time and a value inside the displayed range. The curve was kept."); return; }
        if (gain) value = juce::Decibels::gainToDecibels((float) value / 100, -60.f);
        if (safe->engine.project.clipFor(address.clipId)) if (const auto bounds = safe->engine.project.automationBounds(address); timeValue < bounds.getStart() || timeValue > bounds.getEnd())
        { safe->setStatus("Place the node inside its automation span. The curve was kept."); return; }
        value /= scale;
        timeValue = safe->engine.project.automationTime(address, timeValue);
        if (timeValue < 0) { safe->fail("Place the marker inside its audio track."); return; }
        {
        auto candidate = safe->automationEditSnapshot();
        auto* envelope = safe->materialiseAutomation(candidate, address); if (!envelope) return;
        const auto editing = envelope->points.empty() && point >= 0 ? viewCurve : *envelope;
        if (point >= 0 && (point >= (int) editing.points.size() || (point > 0 && timeValue <= editing.points[(size_t) point - 1].time)
            || (point + 1 < (int) editing.points.size() && timeValue >= editing.points[(size_t) point + 1].time)))
        { safe->fail("Keep this point between its neighbouring points. The curve was kept."); return; }
        if (point >= 0)
        {
            if (envelope->points.empty()) *envelope = editing;
            const auto moved = candidate.moveAutomationPoint(address, point, candidate.automationSessionTime(address, timeValue), (float) value);
            if (moved.failed()) { safe->fail(moved.getErrorMessage()); return; }
        }
        else
        {
            candidate.automationEdited(address);
            if (envelope->points.empty())
            {
                envelope->enabled = true;
            }
            envelope->setPoint(timeValue, (float) value);
        }
        const auto protection = safe->engine.project.validateTransitionLocks(candidate);
        if (protection.failed()) { safe->setStatus(protection.getErrorMessage()); return; }
        if (!safe->publishAutomationEdit(std::move(candidate),address,true)) return;
        }
        const auto* updated = safe->engine.project.curve(address);
        if (updated)
            for (int i = 0; i < (int) updated->points.size(); ++i)
                if (updated->points[(size_t) i].time == timeValue) { safe->timeline.selectAutomationPoint(address, i); break; }
        safe->refresh(); safe->setStatus("Node saved at project " + timeText(safe->engine.project.automationSessionTime(address, timeValue) - safe->engine.project.tempo.origin));
    }), true);
}
}
