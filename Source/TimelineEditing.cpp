#include "Workstation.h"
#include "PresetNodes.h"
#include <cmath>

namespace mixworks
{
juce::String Workstation::timelineActionUnavailable(TimelineAction action,double time,int track) const
{
    if(busy)return "Wait for preparation to finish, then try this edit again.";
    if(engine.exporting)return "Finish or cancel export before editing the mix.";
    if(track<0 || track>=trackCount || !std::isfinite(time))return "Choose a valid timeline position and lane, then try again.";
    const auto* clip=engine.project.clipFor(selectedClip);
    const bool copy=action==TimelineAction::copy || action==TimelineAction::cut;
    const bool paste=action==TimelineAction::paste;
    if (paste && track >= layout.visibleLaneCount) return "Choose a visible audio lane before pasting.";
    const bool closingTime=action==TimelineAction::rippleDelete || action==TimelineAction::closeGap;
    const bool range=action==TimelineAction::repeatSelection || action==TimelineAction::cropSelection
        || action==TimelineAction::replaceSelection || action==TimelineAction::unlinkSelection
        || (editSelection.multiple() && (copy || action==TimelineAction::remove)) || (paste && editClipboard.has_value());
    if (editSelection.multiple() && (action == TimelineAction::rippleDelete || action == TimelineAction::closeGap
        || action == TimelineAction::reverse || action == TimelineAction::loopStart || action == TimelineAction::loopEnd
        || action == TimelineAction::configureLoop || action == TimelineAction::settings
        || action == TimelineAction::insertBefore || action == TimelineAction::insertAfter))
        return "Clear the range or group to edit one segment.";
    const bool structural=range || paste || copy || action==TimelineAction::remove || action==TimelineAction::removePiece
        || action==TimelineAction::split || action==TimelineAction::reverse || action==TimelineAction::rippleDelete
        || action==TimelineAction::closeGap || action==TimelineAction::loopStart || action==TimelineAction::loopEnd || action==TimelineAction::configureLoop;
    if(!structural)return {};
    if (closingTime || range || paste || copy || action==TimelineAction::split || action==TimelineAction::remove || action==TimelineAction::removePiece)
        if (const auto why=structuralEditUnavailable();why.isNotEmpty()) return why;
    if (closingTime && (engine.replayingTake() || engine.automationRecordMode!=AutomationRecordMode::read))
        return "End take playback and choose Read before closing time.";
    if(pendingTempo>0 || tempoPreparationPending || !timelineLoads.empty() || engine.project.hasPendingPlacement())
        return "Wait for track and tempo preparation to finish, then try this edit again.";
    if(paste && !editClipboard && !clipClipboard)return "Copy audio first, then choose where to paste it.";
    if(action==TimelineAction::replaceSelection && !editClipboard && !clipClipboard)return "Copy replacement audio first, then select the audio to replace.";
    if(!paste && !clip)return "Select an audio segment before using this edit.";
    if(!paste && (!clip->media || !clip->media->hasPlayback()))return "This source is unavailable. Relink or reload its audio before editing.";
    if(!paste && clip->media->sourceDetailsPending)return "Wait for this source to finish loading before editing its audio.";
    if(range && (engine.playing || engine.rehearsalActive()))return "Stop playback or rehearsal before editing an audio range or group.";
    if(range && (engine.recordingAutomation() || engine.automationRecordMode!=AutomationRecordMode::read))
        return "Finish automation recording and choose Read before editing an audio range or group.";
    if(action==TimelineAction::cropSelection && !editSelection.range)return "Drag over the waveform to select an audio range before cropping.";
    if(action==TimelineAction::unlinkSelection && editSelection.range)return "Clear the audio range and select whole segments before unlinking.";
    if(action!=TimelineAction::copy && !paste && clip && engine.project.clipTransitionLocked(clip->id))
        return "Unlock this segment's transition before changing its audio.";
    if(paste && engine.project.hasLockedTransitions())return "Unlock transitions before pasting audio and its tempo markers.";
    if(action==TimelineAction::split)
    {
        if(editSelection.multiple())return "Clear the range or group before splitting one segment.";
        time=engine.project.snappedTime(time,engine.project.snap);
        if(time<=clip->start || time>=clip->end())return "Place the cursor inside the selected segment before splitting it.";
    }
    if(action==TimelineAction::loopStart && (time<clip->start || time>=clip->end()))return "Place Loop Start inside the selected segment.";
    if(action==TimelineAction::loopEnd && (loopStartClip!=clip->id || time<=clip->start+clip->localTimeForSource(loopStartSource) || time>clip->end()))
        return "Mark Loop Start first, then place Loop End later inside the same segment.";
    return {};
}
juce::PopupMenu Workstation::timelineMenu(double time, int track)
{
    juce::PopupMenu menu; const auto* clip = currentClip();
    const bool scoped = clip && editSelection.multiple() && editSelection.primary == selectedClip;
    if (scoped && editSelection.range)
    {
        const auto range = *editSelection.range;
        const double origin = engine.project.tempo.origin;
        menu.addSectionHeader("Selected audio / " + timeText(range.getStart() - origin) + " to " + timeText(range.getEnd() - origin)
            + " / " + juce::String((int) editSelection.clips.size()) + (editSelection.clips.size() == 1 ? " segment" : " segments"));
    }
    else if (scoped) menu.addSectionHeader(juce::String((int) editSelection.clips.size()) + " selected segments");
    else if (clip) menu.addSectionHeader(clip->media->libraryTitle() + " / lane " + juce::String(track + 1));
    else menu.addSectionHeader("Lane " + juce::String(track + 1) + " / " + timeText(time - engine.project.tempo.origin));
    const bool editable = clip && clip->media->hasPlayback() && !clip->media->sourceDetailsPending && clip->pendingPlacement == PendingPlacement::none;
    const bool linkedRange=editSelection.range && std::any_of(engine.project.transitionLinks.begin(),engine.project.transitionLinks.end(),[&](const auto& link)
        { return editSelection.contains(link.incoming) || editSelection.contains(link.outgoing); });
    const auto surgery=linkedRange ? juce::String(" / unlink affected transitions") : juce::String();
    auto add = [&](juce::PopupMenu& target, TimelineAction action, const juce::String& label, const juce::String& key, bool enabled)
    {
        const auto reason=timelineActionUnavailable(action,time,track);
        juce::PopupMenu::Item entry(label + (reason.isEmpty() ? juce::String() : " / " + reason));
        entry.itemID = (int) action; entry.isEnabled = enabled && reason.isEmpty(); entry.shortcutKeyDescription = key; target.addItem(std::move(entry));
    };
    auto item = [&](TimelineAction action, const juce::String& label, const juce::String& key, bool enabled)
    { add(menu, action, label, key, enabled); };
    const bool canPaste = editClipboard || (clipClipboard && (engine.project.followTrackTempo || !clipClipboard->tempoMap));
    const auto addTransitionText = "Add Transition / " + transitionPreset.getText() + " / " + transitionLength.getText();
    const bool transitionReady = editable && !engine.rehearsalActive() && !engine.exporting
        && pendingTempo <= 0 && !tempoPreparationPending && timelineLoads.empty()
        && !engine.project.hasPendingPlacement() && (engine.workMode == WorkMode::recordPerform
        || (!engine.recordingAutomation() && engine.automationRecordMode == AutomationRecordMode::read));
    const bool linked = clip && std::any_of(engine.project.transitionLinks.begin(), engine.project.transitionLinks.end(),
        [&](const auto& link) { return scoped ? editSelection.contains(link.incoming) || editSelection.contains(link.outgoing)
            : link.incoming == selectedClip || link.outgoing == selectedClip; });
    if (!clip)
    {
        item(TimelineAction::paste, "Paste here", {}, canPaste);
        item(TimelineAction::label, "Add label here...", "L", true);
        item(TimelineAction::tempo, "Add tempo marker here...", "P", true);
        menu.addSeparator();
        item(TimelineAction::selectSegment, "Choose segment...", {}, !engine.project.clips.empty());
        item(TimelineAction::selectAll, "Select all segments", "Ctrl+A", !engine.project.clips.empty());
        return menu;
    }
    if (scoped)
    {
        item(TimelineAction::cut, "Cut" + surgery, "Ctrl+X", editable);
        item(TimelineAction::copy, "Copy", "Ctrl+C", editable);
        item(TimelineAction::remove, "Delete selected audio" + surgery, "Del", editable);
        item(TimelineAction::repeatSelection, "Repeat selected audio...", {}, editable);
        item(TimelineAction::replaceSelection, "Replace with clipboard...", {}, editable && canPaste && !engine.project.hasLockedTransitions());
        if (editSelection.range) item(TimelineAction::cropSelection, "Keep only selected range" + surgery, {}, editable);
        else
        {
            if (editSelection.clips.size() == 2)
            {
                item(TimelineAction::addTransition, addTransitionText, {}, transitionReady);
                item(TimelineAction::newTransitionDefaults, "Defaults for new transitions...", {}, true);
            }
            if (linked) item(TimelineAction::unlinkSelection, "Unlink / move independently", {}, editable);
        }
        menu.addSeparator();
        item(TimelineAction::exactRange, "Exact range...", {}, true);
        menu.addSubMenu("Range selection", rangePolicyMenu());
        item(TimelineAction::clearSelection, "Clear selection", "Esc", true);
        return menu;
    }
    item(TimelineAction::split, "Split here", {}, editable);
    item(TimelineAction::cut, "Cut segment", "Ctrl+X", editable);
    item(TimelineAction::copy, "Copy segment", "Ctrl+C", editable);
    item(TimelineAction::paste, "Paste here", {}, canPaste);
    item(TimelineAction::removePiece, "Delete segment", "Del", clip != nullptr);
    item(TimelineAction::settings, "Segment settings...", "Ctrl+T", clip != nullptr);
    menu.addSeparator();
    item(TimelineAction::addTransition, addTransitionText, {}, transitionReady);
    item(TimelineAction::newTransitionDefaults, "Defaults for new transitions...", {}, true);
    if (linked) item(TimelineAction::unlinkSelection, "Unlink / move independently", {}, editable);
    item(TimelineAction::exactRange, "Exact range...", {}, true);
    menu.addSubMenu("Range selection", rangePolicyMenu());
    juce::PopupMenu arrange;
    add(arrange, TimelineAction::repeatSelection, "Repeat segment...", {}, editable);
    add(arrange, TimelineAction::replaceSelection, "Replace with clipboard...", {}, editable && canPaste && !engine.project.hasLockedTransitions());
    add(arrange, TimelineAction::insertBefore, "Insert song before...", {}, editable && canInsertArrangement() && !engine.library.empty());
    add(arrange, TimelineAction::insertAfter, "Insert song after...", {}, editable && canInsertArrangement() && !engine.library.empty());
    arrange.addSectionHeader("Across all lanes");
    add(arrange, TimelineAction::rippleDelete, "Ripple delete segment", "Ctrl+Shift+Del", editable);
    add(arrange, TimelineAction::closeGap, "Close gap before segment", "Ctrl+Shift+G", editable);
    menu.addSubMenu("Arrange", arrange);
    juce::PopupMenu curves;
    add(curves, TimelineAction::automationDetails, "Automation details...", {}, true);
    add(curves, TimelineAction::volume, "Add Volume node here", {}, editable);
    add(curves, TimelineAction::treble, "Add Treble node here", {}, editable);
    add(curves, TimelineAction::filter, "Add Filter node here", {}, editable);
    add(curves, TimelineAction::mid, "Add Mid node here", {}, editable);
    add(curves, TimelineAction::bass, "Add Bass node here", {}, editable);
    const bool linkedBass = clip && clip->track == track && std::any_of(engine.project.transitionLinks.begin(), engine.project.transitionLinks.end(),
        [&](const auto& link) { return link.incoming == clip->id || link.outgoing == clip->id; });
    const bool swapReady = !busy && editable && linkedBass && pendingTempo <= 0 && !tempoPreparationPending && !engine.exporting && !engine.project.clipTransitionLocked(clip->id);
    const bool insideFrame = swapReady && std::any_of(engine.project.transitionLinks.begin(),engine.project.transitionLinks.end(),[&](const auto& link)
    { const auto frame = engine.project.transitionRange(link); return (link.incoming == clip->id || link.outgoing == clip->id) && time > frame.getStart() && time < frame.getEnd(); });
    juce::PopupMenu swaps;
    swaps.addItem((int)TimelineAction::placeBassSwap,"Place Bass swap here",insideFrame);
    swaps.addItem((int)TimelineAction::placeMidSwap,"Place Mid swap here",insideFrame);
    swaps.addItem((int)TimelineAction::placeTrebleSwap,"Place Treble swap here",insideFrame);
    swaps.addItem((int)TimelineAction::placeVolumeSwap,"Place Volume swap here",insideFrame);
    swaps.addSeparator(); swaps.addItem((int)TimelineAction::pairBassSwap,"Pair existing full bass cut",swapReady);
    if (linkedBass) curves.addSubMenu("Swaps", swaps);
    curves.addSubMenu("Curve and recording", automationMenu(track));
    juce::PopupMenu markers;
    add(markers, TimelineAction::label, "Add label here...", "L", true);
    add(markers, TimelineAction::tempo, "Add tempo marker here...", "P", true);
    add(markers, TimelineAction::sectionBoundary, "Add section boundary here...", {}, editable && clip->track == track && !clip->reversed && clip->loopSpan <= 0
        && time >= clip->start && time < clip->end() && pendingTempo <= 0 && !tempoPreparationPending && !engine.exporting
        && !engine.rehearsalActive() && engine.automationRecordMode == AutomationRecordMode::read && !engine.recordingAutomation()
        && !engine.project.hasPendingPlacement() && !engine.project.clipTransitionLocked(clip->id));
    const auto& chain = engine.pluginChain(track);
    const bool effects = std::any_of(chain.begin(), chain.end(), [](const auto& slot) { return !slot->parameters.empty(); });
    add(curves, TimelineAction::effect, "Add FX node here", {}, editable && effects);
    menu.addSubMenu("Automation", curves);
    add(markers, TimelineAction::downbeat, "Set first downbeat here (4/4)", {}, editable && time >= clip->start && time < clip->end());
    menu.addSubMenu("Markers and grid", markers);
    juce::PopupMenu audio;
    add(audio, TimelineAction::reverse, "Reverse", {}, editable);
    add(audio, TimelineAction::configureLoop, "Loop section...", {}, editable);
    add(audio, TimelineAction::loopStart, "Mark loop start here", {}, editable);
    add(audio, TimelineAction::loopEnd, "Mark loop end here", {}, editable && loopStartClip == clip->id
        && time > clip->start + clip->localTimeForSource(loopStartSource) && time <= clip->end());
    add(audio, TimelineAction::tweaks, "BPM, key and downbeat...", "Ctrl+W", editable);
    add(audio, TimelineAction::harmonise, "Harmonise by ear...", {}, true);
    add(audio, TimelineAction::stems, clip->stems ? "Stem tools..." : "Separate stems...", {}, editable);
    menu.addSubMenu("Source and playback", audio);
    return menu;
}
juce::PopupMenu::Options Workstation::timelineMenuOptions(juce::Rectangle<int> anchor)
{
    auto options = juce::PopupMenu::Options().withTargetComponent(&timeline);
    return anchor.isEmpty() ? options.withMousePosition() : options.withTargetScreenArea(anchor);
}

juce::PopupMenu Workstation::rangePolicyMenu() const
{
    juce::PopupMenu sizes;
    for (const int id : {8, 1, 7, 2, 3, 4, 5, 6})
    {
        if (id == 2) sizes.addSeparator();
        sizes.addItem((int) TimelineAction::rangeDragFree + id - 1,
            editingRangeSize.getItemText(editingRangeSize.indexOfItemId(id)), true, editingRangeSize.getSelectedId() == id);
    }
    return sizes;
}

void Workstation::setRangePolicy(int id)
{
    editingRangeSize.setSelectedId(id, juce::dontSendNotification);
    timeline.rangeDragAnchor.reset();
    timelineRuler.refreshEditTools();
}

void Workstation::showTimelineMenu(double time, int track, juce::Rectangle<int> anchor)
{
    if (busy) { setStatus("Wait for preparation to finish, then open the editing menu again."); return; }
    const auto clipId = selectedClip;
    const auto scope = editSelection;
    const auto revision = editClipboardRevision;
    const auto before = scope.multiple() || editClipboard ? juce::JSON::toString(projectToJson(engine.project)) : juce::String();
    auto menu = timelineMenu(time, track);
    const auto screenAnchor = anchor.isEmpty() ? juce::Rectangle<int>(1, 1).withPosition(juce::Desktop::getMousePosition()) : anchor;
    menu.showMenuAsync(timelineMenuOptions(screenAnchor),
        [safe = juce::Component::SafePointer<Workstation>(this), clipId, time, track, scope, revision, before, screenAnchor](int id)
        {
            if (!safe || id == 0) return;
            if(safe->busy) { safe->setStatus("Preparation started while the menu was open. Wait for it to finish, then open the menu again."); return; }
            if (safe->selectedClip != clipId || scope.primary != safe->editSelection.primary
                || scope.clips != safe->editSelection.clips || scope.range != safe->editSelection.range
                || revision != safe->editClipboardRevision
                || (before.isNotEmpty() && before != juce::JSON::toString(projectToJson(safe->engine.project))))
            { safe->setStatus("Document, clipboard or selection changed while the menu was open. Open it again."); return; }
            if (clipId.isNotEmpty() && std::none_of(safe->engine.project.clips.begin(), safe->engine.project.clips.end(), [&](const auto& c) { return c.id == clipId; }))
            { safe->setStatus("The selected segment was removed. Select another segment and reopen the menu."); return; }
            if (id == (int) TimelineAction::addTransition) { safe->chooseAddTransition(screenAnchor); return; }
            if (id == (int) TimelineAction::newTransitionDefaults) { safe->showNewTransitionDefaults(screenAnchor); return; }
            if (id == (int) TimelineAction::selectSegment) { safe->chooseTimelineSegment(screenAnchor); return; }
            safe->performTimelineAction((TimelineAction) id, time, track);
        });
}
void Workstation::addTimelineAutomation(AutomationTarget target, double time, int track)
{
    const bool selectedCurve = automationLane() == (int) target && engine.selectedTrack == track;
    const auto* clip = currentClip();
    auto address = selectedCurve ? displayedAddress(track) : target == AutomationTarget::plugin ? pluginLane
        : AutomationAddress {target == AutomationTarget::master ? -1 : track, target};
    if ((int) target < trackAutomationCount && address.clipId.isEmpty() && clip && clip->track == track) address.clipId = clip->id;
    insertAutomationNode(address, time);
}
void Workstation::performTimelineAction(TimelineAction action, double time, int track)
{
    if(const auto reason=timelineActionUnavailable(action,time,track); reason.isNotEmpty()) { setStatus(reason); return; }
    if (action >= TimelineAction::rangeDragFree && action <= TimelineAction::rangeDragProject)
    { setRangePolicy((int) action - (int) TimelineAction::rangeDragFree + 1); return; }
    if (action == TimelineAction::exactRange) { editRangeTime(); return; }
    if (action == TimelineAction::clearSelection) { clearEditSelection(); refresh(); return; }
    if (action == TimelineAction::selectAll) { selectAllSegments(); return; }
    if (action == TimelineAction::selectSegment) { chooseTimelineSegment(); return; }
    if (action == TimelineAction::automationDetails)
    {
        if (const auto* clip = currentClip(); clip && (automationLane() < 0 || displayedAddress(track).clipId != clip->id))
            timeline.selectAutomationCurve({clip->track, AutomationTarget::volume, {}, {}, clip->id});
        showAutomationDetails(); return;
    }
    if (action == TimelineAction::configureLoop) { configureLoop(); return; }
    if (action == TimelineAction::replaceSelection) { replaceSelectedAudio(); return; }
    if (action == TimelineAction::removePiece)
    {
        // A shared effect curve may still select another piece's owner. Delete
        // the waveform actually under the menu, not that automation owner.
        for (auto it = engine.project.clips.rbegin(); it != engine.project.clips.rend(); ++it)
            if (it->track == track && time >= it->start && time < it->end())
            { selectedClip = it->id; clearEditSelection(); removeClip(); return; }
        setStatus("Choose a track piece to delete."); return;
    }
    if (performSelectionAction(action, time, track)) return;
    if (action >= TimelineAction::curveRead && engine.exporting) return;
    auto* clip = currentClip();
    if (action != TimelineAction::rippleDelete && action != TimelineAction::closeGap) selectTrack(track);
    switch (action)
    {
        case TimelineAction::unlinkSelection:
        {
            if (!clip || editSelection.range || engine.playing || engine.rehearsalActive() || pendingTempo > 0 || tempoPreparationPending
                || engine.recordingAutomation() || engine.automationRecordMode != AutomationRecordMode::read)
            { setStatus("Stop playback, finish preparation and clear the audio range before unlinking whole segments in Read mode."); return; }
            const auto ids = editSelection.clips.empty() ? std::vector<juce::String>{selectedClip} : editSelection.clips;
            auto candidate = projectEditSnapshot(); const auto document=structuralEditDocument(candidate);
            const auto previousLinks = candidate.transitionLinks;
            const auto result = candidate.unlinkClips(ids);
            if (result.failed()) { setStatus(result.getErrorMessage()); return; }
            if (candidate.transitionLinks == previousLinks) { setStatus("These segments already move independently."); return; }
            if (!publishStructuralEdit(std::move(candidate),document)) return;
            clearEditSelection(); selectedTransition.clear(); refresh();
            setStatus("Unlinked. Drag a waveform to move it independently; Ctrl-click pieces to move them together. Fades and automation kept. Undo restores links.");
            return;
        }
        case TimelineAction::stems: if (clip) { if (clip->stems) showStemMenu(clip->id, -1, time); else splitStems(clip->media->id, false); } return;
        case TimelineAction::harmonise: showHarmoniseByEar(); return;
        case TimelineAction::sectionBoundary:
            if (clip && clip->track == track) timeline.addSectionBoundary(clip->id, time);
            return;
        case TimelineAction::addTransition: chooseAddTransition(); return;
        case TimelineAction::insertBefore: case TimelineAction::insertAfter:
            if (clip) chooseArrangementInsertion(clip->id, action == TimelineAction::insertAfter);
            return;
        case TimelineAction::rippleDelete: case TimelineAction::closeGap:
        {
            if (!clip) return;
            auto candidate = projectEditSnapshot(true);
            const bool removing = action == TimelineAction::rippleDelete;
            const auto id = clip->id;
            const double end = removing ? clip->end() : clip->start;
            double begin = removing ? clip->start : 0;
            if (!removing) for (const auto& other : candidate.clips)
                if (other.id != id && other.start < end) begin = std::max(begin, other.end());
            if (end <= begin) { setStatus("There is no empty gap before this segment across all lanes."); return; }
            const auto result = candidate.closeTimeRange(begin, end, removing ? id : juce::String());
            if (result.failed()) { setStatus(result.getErrorMessage()); return; }
            auto seek = engine.prepareEditSeek(candidate, [begin, end](double position)
            { return position < begin ? position : begin + std::max(0., position - end); });
            const auto published = engine.commitSeek(std::move(seek), &candidate);
            if (published.failed()) { setStatus(published.getErrorMessage()); return; }
            recordEditHistory(std::move(candidate));
            clearEditSelection();
            timeline.selectAutomationPoint({}, -1); tempoStrip.selectedTempoPoint = -1;
            selectedTransition.clear(); loopStartClip.clear();
            if (removing) selectedClip.clear();
            selectTrack(track); setStatus((removing ? "Segment ripple-deleted. " : "Gap closed. ") + juce::String(end - begin, 3)
                + " seconds removed across all lanes; later segments, linked pairs and session markers/automation moved together. Undo restores the mix.");
            return;
        }
        case TimelineAction::placeVolumeSwap: case TimelineAction::placeBassSwap: case TimelineAction::placeMidSwap: case TimelineAction::placeTrebleSwap:
        {
            if (!clip || clip->track != track || pendingTempo > 0 || tempoPreparationPending || engine.exporting) return;
            engine.flushMidiControls(); engine.finishAutomation();
            const Project::TransitionLink* chosen = nullptr;
            for (const auto& link : engine.project.transitionLinks)
            {
                const auto frame = engine.project.transitionRange(link);
                if ((link.incoming != clip->id && link.outgoing != clip->id) || time <= frame.getStart() || time >= frame.getEnd()) continue;
                if (chosen) { setStatus("Two transition frames overlap here. Select the intended pair in Transitions to place its swap."); return; }
                chosen = &link;
            }
            if (!chosen) { setStatus("Right-click inside this song's transition frame to place a swap."); return; }
            const int band = (int)action-(int)TimelineAction::placeVolumeSwap;
            const auto range = engine.project.transitionRange(*chosen); const auto clock = engine.project.gridClock();
            TransitionBandSettings setting {TransitionBandMode::swap,1,0,
                (clock.beatsAt(time)-clock.beatsAt(range.getStart()))/(clock.beatsAt(range.getEnd())-clock.beatsAt(range.getStart()))};
            const auto incoming = chosen->incoming; auto candidate = engine.project;
            const auto placed = placeTransitionBandNodes(candidate,incoming,band,setting);
            if (placed.failed()) { setStatus(placed.getErrorMessage()); return; }
            remember();
            { const juce::ScopedLock guard(engine.lock); engine.project = std::move(candidate); }
            selectedTransition = selectedClip = incoming; selectedStem = -1; selectedAutomationTarget = band; automationClip = selectedClip;
            transitionCurveShown[(size_t)band] = true; if (transitionCurveOnlyBand >= 0) transitionCurveOnlyBand = band;
            transitionNodeOptionsFor.clear(); selectTrack(engine.project.clipFor(incoming)->track); refresh();
            setStatus("Swap placed as regular nodes. Drag either paired handle horizontally, or edit its nodes individually. Undo restores the previous shape.");
            return;
        }
        case TimelineAction::pairBassSwap:
        {
            if (!clip || clip->track != track) { setStatus("Right-click the segment that should receive the bass swap."); return; }
            if (pendingTempo > 0 || tempoPreparationPending) { setStatus("Wait for tempo preparation before pairing bass."); return; }
            engine.finishAutomation(); const juce::ScopedLock guard(engine.lock);
            const auto target = clip->id; auto candidate = engine.project; Project::BassSwapPairing pairing;
            const auto paired = candidate.pairBassSwap(target, time, pairing);
            if (paired.failed()) { setStatus(paired.getErrorMessage()); return; }
            if (pairing.changed) { remember(); engine.project = std::move(candidate); }
            if (selectedTransition != pairing.incoming) engine.endTransitionRehearsal();
            selectedTransition = pairing.incoming; selectedAutomationTarget = (int) AutomationTarget::bass; automationClip = selectedClip;
            setStatus(pairing.changed ? "Bass swap paired from the nearest full cut. Both B handles now move together; Undo restores the previous curve."
                : "This full bass cut is already paired."); refresh(); return;
        }
        case TimelineAction::curveRead: case TimelineAction::curveClear:
        {
            if (automationLane() < 0) return;
            const auto address = displayedAddress(track);
            if (!automationEditable(address)) { setStatus("Automation is unavailable, busy or locked."); return; }
            auto candidate = engine.project; auto* curve = materialiseAutomation(candidate,address); if (!curve || curve->points.empty()) return;
            if (action == TimelineAction::curveRead) curve->enabled = !curve->enabled;
            else { candidate.automationEdited(address, AutomationEdit::clearCurve); *curve = {}; }
            engine.setAutomationRecordMode(AutomationRecordMode::read); remember(); const juce::ScopedLock guard(engine.lock);
            engine.project = std::move(candidate);
            refresh(); return;
        }
        case TimelineAction::curvePoint: if (automationLane() >= 0) editAutomationPoint(track, -1, time); return;
        case TimelineAction::choosePlugin: chooseAutomationPlugin(); return;
        case TimelineAction::recordOff: case TimelineAction::recordTouch: case TimelineAction::recordLatch:
            engine.flushMidiControls();
            engine.setAutomationRecordMode(action == TimelineAction::recordOff ? AutomationRecordMode::read : action == TimelineAction::recordTouch ? AutomationRecordMode::touch : AutomationRecordMode::write);
            refresh(); return;
        case TimelineAction::armAutomation:
        {
            engine.flushMidiControls(); const int lane = automationLane() == (int) AutomationTarget::master ? -1 : track;
            engine.armAutomation(lane, lane < 0 ? !engine.masterAutomationArmed : !engine.automationArmed[(size_t) lane]); refresh(); return;
        }
        case TimelineAction::cut: case TimelineAction::copy:
            if (action == TimelineAction::cut && clip && engine.project.clipTransitionLocked(clip->id)) { setStatus("Unlock the transition before cutting its segment."); return; }
            if (!clip || !clip->media->hasPlayback() || clip->media->sourceDetailsPending || clip->pendingPlacement != PendingPlacement::none) return;
            editClipboard.reset(); ++editClipboardRevision;
            clipClipboard = *clip; pasteEnd = -1; if (action == TimelineAction::cut) removeClip();
            setStatus(action == TimelineAction::cut ? "Segment cut. Paste at the cursor to move it; Undo restores it." : "Segment copied. Paste at the cursor on the chosen lane."); break;
        case TimelineAction::paste:
        {
            if (!clipClipboard) return;
            if (engine.project.hasLockedTransitions()) { setStatus("Unlock transitions before pasting segments with new tempo markers."); return; }
            if (!engine.project.followTrackTempo && clipClipboard->tempoMap)
            { setStatus("Beatmatch this legacy fixed-tempo session before pasting a segment with a changing tempo map."); return; }
            auto candidate = projectEditSnapshot(); const auto document=structuralEditDocument(candidate);
            Clip pasted = *clipClipboard; pasted.id = juce::Uuid().toString(); pasted.track = track;
            if (auto error = engine.cloneClipEffects(pasted); error.isNotEmpty()) { fail(error); return; }
            pasted.pendingPlacement = PendingPlacement::none; pasted.pendingPredecessor.clear();
            const auto map = candidate.followTrackTempo ? std::make_shared<const TempoMap>(candidate.tempo) : std::shared_ptr<const TempoMap>();
            const bool continuing = time == pasteEnd && engine.position == pasteEnd;
            pasted.retime(candidate.snappedClipStart(pasted, time, candidate.snap && !continuing), map);
            const auto pastedId=pasted.id;
            if (candidate.followTrackTempo && !candidate.tempo.points.empty())
            { const double at = pasted.start + pasted.transitionTime(true);
              const auto next = std::upper_bound(candidate.tempo.points.begin(), candidate.tempo.points.end(), at, [](double t, const auto& p) { return t < p.time; });
              candidate.tempo.setTrackPoint(pasted.id, at, candidate.tempo.bpmAt(at), next == candidate.tempo.points.end() || next->ramp); }
            candidate.clips.push_back(std::move(pasted));
            if (!publishStructuralEdit(std::move(candidate),document)) return;
            selectedClip=pastedId;
            pasteEnd = engine.project.clipFor(selectedClip)->end(); engine.seek(pasteEnd);
            refresh(); break;
        }
        case TimelineAction::remove: removeClip(); break;
        case TimelineAction::volume: addTimelineAutomation(AutomationTarget::volume, time, track); break;
        case TimelineAction::treble: addTimelineAutomation(AutomationTarget::treble, time, track); break;
        case TimelineAction::mid: addTimelineAutomation(AutomationTarget::mid, time, track); break;
        case TimelineAction::bass: addTimelineAutomation(AutomationTarget::bass, time, track); break;
        case TimelineAction::filter: addTimelineAutomation(AutomationTarget::cutoff, time, track); break;
        case TimelineAction::tempo: editTempoPoint(-1, engine.project.snappedTime(time, engine.project.snap)); break;
        case TimelineAction::label: editLabelMarker({}, engine.project.snappedTime(time, engine.project.snap)); break;
        case TimelineAction::effect:
            if (automationLane() == (int) AutomationTarget::plugin && pluginLane.track == track && automationPluginParameter(pluginLane)) insertAutomationNode(pluginLane, time);
            else chooseAutomationPlugin(time);
            break;
        case TimelineAction::split:
        {
            // Splitting consumes the cursor immediately. A navigation request
            // would leave it at the old audible position until preparation ends.
            const auto moved = engine.commitSeek(engine.prepareSeek(engine.project.snappedTime(time, engine.project.snap)));
            if (moved.failed()) { setStatus(moved.getErrorMessage()); break; }
            splitClip(); break;
        }
        case TimelineAction::reverse: reverseButton.triggerClick(); break;
        case TimelineAction::tweaks: editAnalysis(); break;
        case TimelineAction::settings: showInspectorPage(0); showEditingPage(1); refresh(); clipSliders[0].grabKeyboardFocus(); break;
        case TimelineAction::downbeat: if (clip) setDownbeatAnchor(clip->id, time - clip->start); break;
        case TimelineAction::loopStart:
            if (!clip || clip->media->sourceDetailsPending || time < clip->start || time >= clip->end()) return;
            loopStartClip = clip->id;
            loopStartSource = clip->sourceElapsed(juce::jlimit(0.0, clip->length, engine.project.snappedTime(time, engine.project.snap) - clip->start));
            timeline.repaint(); setStatus("Loop start marked. Set Loop End later in this segment to repeat that range twice; Loop... changes the repeat count."); break;
        case TimelineAction::loopEnd:
        {
            if (!clip || clip->id != loopStartClip || clip->media->sourceDetailsPending) return;
            if (engine.project.clipTransitionLocked(clip->id)) { setStatus("Unlock the transition before looping its segment."); return; }
            const double begin = clip->localTimeForSource(loopStartSource), end = engine.project.snappedTime(time, engine.project.snap) - clip->start;
            if (end <= begin || end > clip->length) { setStatus("Place Loop End after Loop Start, inside the same segment."); return; }
            auto loop = clip->slice(begin, end - begin); const double span = loop.length * loop.rate;
            // A wrapped range has no single contiguous source interval.
            if (clip->loopSpan > 0) { setStatus("Remove the existing loop before marking a new source range."); return; }
            loop.loopSpan = span; loop.loopPhase = 0; loop.length = loop.localTimeForSource(span * 2); loop.rate = span * 2 / loop.length;
            loop.introAnchor.reset(); loop.outroAnchor.reset(); loop.fadeInAnchor.reset(); loop.fadeOutAnchor.reset(); loop.fadeIn = loop.fadeOut = 0;
            remember(); { const juce::ScopedLock guard(engine.lock); scheduleTempoPreparation(); *clip = std::move(loop); }
            loopStartClip.clear(); refresh(); setStatus("Marked source range repeats twice. Loop... adjusts repetition; Undo restores the segment."); break;
        }
    }
}
}
