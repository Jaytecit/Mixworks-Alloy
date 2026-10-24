#include "Workstation.h"
#include "PresetNodes.h"
#include <cmath>

namespace mixworks
{
juce::PopupMenu Workstation::timelineMenu(double time, int track)
{
    juce::PopupMenu menu; const auto* clip = currentClip();
    const bool editable = clip && clip->media->hasPlayback() && !clip->media->sourceDetailsPending && clip->pendingPlacement == PendingPlacement::none;
    auto item = [&](TimelineAction action, const juce::String& label, const juce::String& key, bool enabled)
    { juce::PopupMenu::Item entry(label); entry.itemID = (int) action; entry.isEnabled = !busy && enabled; entry.shortcutKeyDescription = key; menu.addItem(std::move(entry)); };
    item(TimelineAction::stems, clip && clip->stems ? "Stems / mix, automate or collapse..." : "Separate stems for this track...", {}, editable);
    item(TimelineAction::cut, "Cut", "Ctrl+X", editable); item(TimelineAction::copy, "Copy", "Ctrl+C", editable);
    item(TimelineAction::paste, "Paste", "Ctrl+V", editClipboard || (clipClipboard && (engine.project.followTrackTempo || !clipClipboard->tempoMap))); menu.addSeparator();
    item(TimelineAction::removePiece, "Delete this piece", editSelection.multiple() ? "" : "Del", clip != nullptr);
    if (editSelection.multiple()) item(TimelineAction::remove, "Delete selected audio", "Del", clip != nullptr);
    menu.addSeparator();
    const bool selectionReady = editable && !engine.playing && !engine.rehearsalActive() && !engine.exporting
        && pendingTempo <= 0 && !tempoPreparationPending && !engine.project.hasPendingPlacement()
        && !engine.recordingAutomation() && engine.automationRecordMode == AutomationRecordMode::read;
    item(TimelineAction::repeatSelection, "Repeat selected audio...", {}, selectionReady);
    item(TimelineAction::addTransition, "Add Transition", {}, selectionReady && !editSelection.range);
    item(TimelineAction::unlinkSelection, "Unlink / move independently", {}, selectionReady && !editSelection.range
        && std::any_of(engine.project.transitionLinks.begin(), engine.project.transitionLinks.end(), [&](const auto& link)
        { return editSelection.clips.empty() ? link.incoming == selectedClip || link.outgoing == selectedClip
            : editSelection.contains(link.incoming) || editSelection.contains(link.outgoing); }));
    item(TimelineAction::cropSelection, "Crop selected segments to range", {}, selectionReady && editSelection.range.has_value());
    item(TimelineAction::replaceSelection, "Replace selected audio...", {}, selectionReady && (editClipboard || clipClipboard) && !engine.project.hasLockedTransitions());
    menu.addSeparator();
    item(TimelineAction::rippleDelete, "Ripple delete segment (all lanes)", "Ctrl+Shift+Del", editable);
    item(TimelineAction::closeGap, "Close gap before segment (all lanes)", "Ctrl+Shift+G", editable); menu.addSeparator();
    item(TimelineAction::insertBefore, "Insert song before this segment...", {}, editable && canInsertArrangement() && !engine.library.empty());
    item(TimelineAction::insertAfter, "Insert song after this segment...", {}, editable && canInsertArrangement() && !engine.library.empty()); menu.addSeparator();
    item(TimelineAction::volume, "Add Volume Node Here", "V", editable); item(TimelineAction::treble, "Add Treble Node Here", "T", editable);
    item(TimelineAction::filter, "Add Filter Node Here", "F", editable);
    item(TimelineAction::mid, "Add Mid Node Here", "M", editable); item(TimelineAction::bass, "Add Bass Node Here", "B", editable);
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
    menu.addSubMenu("Swaps",swaps);
    menu.addSubMenu("Automation", automationMenu(track));
    menu.addSeparator(); item(TimelineAction::label, "Add Label Marker", "L", true);
    item(TimelineAction::tempo, "Add Tempo Marker", "P", true);
    item(TimelineAction::sectionBoundary, "Add Section Boundary Here...", {}, editable && clip->track == track && !clip->reversed && clip->loopSpan <= 0
        && time >= clip->start && time < clip->end() && pendingTempo <= 0 && !tempoPreparationPending && !engine.exporting
        && !engine.rehearsalActive() && engine.automationRecordMode == AutomationRecordMode::read && !engine.recordingAutomation()
        && !engine.project.hasPendingPlacement() && !engine.project.clipTransitionLocked(clip->id));
    const auto& chain = engine.pluginChain(track);
    const bool effects = std::any_of(chain.begin(), chain.end(), [](const auto& slot) { return !slot->parameters.empty(); });
    item(TimelineAction::effect, "Add FX Node Here", "E", editable && effects); menu.addSeparator();
    item(TimelineAction::loopStart, "Add Loop Start Marker", "9", editable && time >= clip->start && time < clip->end());
    item(TimelineAction::loopEnd, "Add Loop End Marker", "0", editable && loopStartClip == clip->id && time > clip->start + clip->localTimeForSource(loopStartSource) && time <= clip->end());
    menu.addSeparator(); item(TimelineAction::split, "Split Segment at Cursor", "S", editable && time > clip->start && time < clip->end());
    item(TimelineAction::reverse, "Reverse", {}, editable); menu.addSeparator();
    item(TimelineAction::tweaks, "Tweaks / BPM, key and downbeat...", "Ctrl+W", editable);
    item(TimelineAction::settings, "Adjust Segment Settings...", "Ctrl+T", clip != nullptr);
    menu.addSeparator(); item(TimelineAction::downbeat, "Set first downbeat here (4/4)", {}, editable && time >= clip->start && time < clip->end());
    return menu;
}
void Workstation::showTimelineMenu(double time, int track)
{
    if (busy) return;
    const auto clipId = selectedClip;
    const auto scope = editSelection;
    const auto revision = editClipboardRevision;
    const auto before = scope.multiple() || editClipboard ? juce::JSON::toString(projectToJson(engine.project)) : juce::String();
    selectTrack(track);
    auto menu = timelineMenu(time, track);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&timeline).withMousePosition(),
        [safe = juce::Component::SafePointer<Workstation>(this), clipId, time, track, scope, revision, before](int id)
        {
            if (!safe || id == 0 || safe->busy) return;
            if (before.isNotEmpty() && (before != juce::JSON::toString(projectToJson(safe->engine.project))
                || scope.primary != safe->editSelection.primary || scope.clips != safe->editSelection.clips || scope.range != safe->editSelection.range
                || revision != safe->editClipboardRevision))
            { safe->setStatus("Document, clipboard or selection changed while the menu was open. Open it again."); return; }
            if (clipId.isNotEmpty() && std::none_of(safe->engine.project.clips.begin(), safe->engine.project.clips.end(), [&](const auto& c) { return c.id == clipId; })) return;
            safe->selectedClip = clipId; safe->selectTrack(track);
            safe->performTimelineAction((TimelineAction) id, time, track);
        });
}
void Workstation::addTimelineAutomation(AutomationTarget target, double time, int track)
{
    const bool selectedCurve = automationLane() == (int) target && engine.selectedTrack == track;
    auto address = selectedCurve ? displayedAddress(track) : target == AutomationTarget::plugin ? pluginLane
        : AutomationAddress {target == AutomationTarget::master ? -1 : track, target};
    if (!selectedCurve && target != AutomationTarget::master && address.clipId.isEmpty() && engine.controlClip(track)) address.clipId = selectedClip;
    if (!selectedCurve && target != AutomationTarget::master && address.clipId.isEmpty()) { setStatus("Select a segment to add automation."); return; }
    insertAutomationNode(address, time);
}
void Workstation::performTimelineAction(TimelineAction action, double time, int track)
{
    if (busy || engine.exporting || track < 0 || track >= trackCount || !std::isfinite(time)) return;
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
    if (editSelection.multiple() && (action == TimelineAction::rippleDelete || action == TimelineAction::closeGap
        || action == TimelineAction::reverse || action == TimelineAction::loopStart || action == TimelineAction::loopEnd || action == TimelineAction::settings
        || action == TimelineAction::insertBefore || action == TimelineAction::insertAfter))
    { setStatus("Clear the range/group before editing one segment or closing global time."); return; }
    if (action >= TimelineAction::curveRead && engine.exporting) return;
    auto* clip = currentClip(); selectTrack(track);
    switch (action)
    {
        case TimelineAction::unlinkSelection:
        {
            if (!clip || editSelection.range || engine.playing || engine.rehearsalActive() || pendingTempo > 0 || tempoPreparationPending
                || engine.recordingAutomation() || engine.automationRecordMode != AutomationRecordMode::read)
            { setStatus("Stop playback, finish preparation and clear the audio range before unlinking whole segments in Read mode."); return; }
            const juce::ScopedLock guard(engine.lock);
            const auto ids = editSelection.clips.empty() ? std::vector<juce::String>{selectedClip} : editSelection.clips;
            auto candidate = engine.project;
            const auto result = candidate.unlinkClips(ids);
            if (result.failed()) { setStatus(result.getErrorMessage()); return; }
            if (candidate.transitionLinks == engine.project.transitionLinks) { setStatus("These segments already move independently."); return; }
            remember(); engine.project = std::move(candidate); clearEditSelection(); selectedTransition.clear(); refresh();
            setStatus("Unlinked. Drag a waveform to move it independently; Ctrl-click pieces to move them together. Fades and automation kept. Undo restores links.");
            return;
        }
        case TimelineAction::stems: if (clip) { if (clip->stems) showStemMenu(clip->id, -1, time); else splitStems(clip->media->id, false); } return;
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
            if (pendingTempo > 0 || tempoPreparationPending || engine.project.hasPendingPlacement())
            { setStatus("Wait for preparation before closing time."); return; }
            const juce::ScopedLock guard(engine.lock);
            if (engine.recordingAutomation() || engine.automationRecordMode != AutomationRecordMode::read)
            { setStatus("Choose Read and finish automation recording before closing time."); return; }
            const bool removing = action == TimelineAction::rippleDelete;
            const auto id = clip->id;
            const double end = removing ? clip->end() : clip->start;
            double begin = removing ? clip->start : 0;
            if (!removing) for (const auto& other : engine.project.clips)
                if (other.id != id && other.start < end) begin = std::max(begin, other.end());
            if (end <= begin) { setStatus("There is no empty gap before this segment across all lanes."); return; }
            auto candidate = engine.project;
            const auto result = candidate.closeTimeRange(begin, end, removing ? id : juce::String());
            if (result.failed()) { setStatus(result.getErrorMessage()); return; }
            remember(); // Ends rehearsal before capturing the transport position.
            const double position = engine.position.load();
            engine.project = std::move(candidate);
            engine.seek(position < begin ? position : position < end ? begin : begin + (position - end));
            timeline.selectAutomationPoint({}, -1); tempoStrip.selectedTempoPoint = -1;
            selectedTransition.clear(); loopStartClip.clear();
            if (removing) selectedClip.clear();
            refresh(); setStatus((removing ? "Segment ripple-deleted. " : "Gap closed. ") + juce::String(end - begin, 3)
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
            selectedTransition = selectedClip = incoming; selectedStem = -1; selectedAutomationTarget = band;
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
            selectedTransition = pairing.incoming; selectedAutomationTarget = (int) AutomationTarget::bass;
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
            Clip pasted = *clipClipboard; pasted.id = juce::Uuid().toString(); pasted.track = track;
            if (auto error = engine.cloneClipEffects(pasted); error.isNotEmpty()) { fail(error); return; }
            pasted.pendingPlacement = PendingPlacement::none; pasted.pendingPredecessor.clear();
            const auto map = engine.project.followTrackTempo ? std::make_shared<const TempoMap>(engine.project.tempo) : std::shared_ptr<const TempoMap>();
            const bool continuing = time == pasteEnd && engine.position == pasteEnd;
            pasted.retime(engine.project.snappedClipStart(pasted, time, engine.project.snap && !continuing), map);
            remember(); { const juce::ScopedLock guard(engine.lock); selectedClip = pasted.id;
                if (engine.project.followTrackTempo && !engine.project.tempo.points.empty())
                { const double at = pasted.start + pasted.transitionTime(true);
                  const auto next = std::upper_bound(engine.project.tempo.points.begin(), engine.project.tempo.points.end(), at, [](double t, const auto& p) { return t < p.time; });
                  engine.project.tempo.setTrackPoint(pasted.id, at, engine.project.tempo.bpmAt(at), next == engine.project.tempo.points.end() || next->ramp); }
                engine.project.clips.push_back(std::move(pasted)); }
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
        case TimelineAction::split: engine.seek(engine.project.snappedTime(time, engine.project.snap)); splitClip(); break;
        case TimelineAction::reverse: reverseButton.triggerClick(); break;
        case TimelineAction::tweaks: editAnalysis(); break;
        case TimelineAction::settings: showInspectorPage(0); refresh(); clipSliders[0].grabKeyboardFocus(); break;
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
