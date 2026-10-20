#include "Workstation.h"
#include <cmath>

namespace mixworks
{
juce::PopupMenu Workstation::timelineMenu(double time, int track)
{
    juce::PopupMenu menu; const auto* clip = currentClip();
    const bool editable = clip && clip->media->hasPlayback() && !clip->media->sourceDetailsPending && clip->pendingPlacement == PendingPlacement::none;
    auto item = [&](TimelineAction action, const juce::String& label, const juce::String& key, bool enabled)
    { juce::PopupMenu::Item entry(label); entry.itemID = (int) action; entry.isEnabled = !busy && enabled; entry.shortcutKeyDescription = key; menu.addItem(std::move(entry)); };
    item(TimelineAction::cut, "Cut", "Ctrl+X", editable); item(TimelineAction::copy, "Copy", "Ctrl+C", editable);
    item(TimelineAction::paste, "Paste", "Ctrl+V", clipClipboard && (engine.project.followTrackTempo || !clipClipboard->tempoMap)); menu.addSeparator();
    item(TimelineAction::remove, "Delete", "Del", clip != nullptr); menu.addSeparator();
    item(TimelineAction::volume, "Add Volume Marker", "V", editable); item(TimelineAction::treble, "Add Treble Marker", "T", editable);
    item(TimelineAction::mid, "Add Midrange Marker", "M", editable); item(TimelineAction::bass, "Add Bass Marker", "B", editable);
    const bool linkedBass = clip && clip->track == track && std::any_of(engine.project.transitionLinks.begin(), engine.project.transitionLinks.end(),
        [&](const auto& link) { return link.incoming == clip->id || link.outgoing == clip->id; });
    item(TimelineAction::pairBassSwap, "Pair bass swap", {}, editable && linkedBass && pendingTempo <= 0 && !tempoPreparationPending && !engine.exporting && !engine.project.clipTransitionLocked(clip->id));
    menu.addSubMenu("Automation", automationMenu(track));
    menu.addSeparator(); item(TimelineAction::label, "Add Label Marker", "L", true);
    item(TimelineAction::tempo, "Add Tempo Marker", "P", true);
    const auto& chain = engine.pluginChain(track);
    const bool effects = std::any_of(chain.begin(), chain.end(), [](const auto& slot) { return !slot->parameters.empty(); });
    item(TimelineAction::effect, "Add Effects Parameter Marker", "E", editable && effects); menu.addSeparator();
    item(TimelineAction::loopStart, "Add Loop Start Marker", "9", editable && time >= clip->start && time < clip->end());
    item(TimelineAction::loopEnd, "Add Loop End Marker", "0", editable && loopStartClip == clip->id && time > clip->start + clip->localTimeForSource(loopStartSource) && time <= clip->end());
    menu.addSeparator(); item(TimelineAction::split, "Split Segment at Cursor", "S", editable && time > clip->start && time < clip->end());
    item(TimelineAction::reverse, "Reverse", {}, editable); menu.addSeparator();
    item(TimelineAction::tweaks, "Tweaks / BPM, key and downbeat...", "Ctrl+W", editable);
    item(TimelineAction::settings, "Adjust Track Settings...", "Ctrl+T", clip != nullptr);
    menu.addSeparator(); item(TimelineAction::downbeat, "Set first downbeat here (4/4)", {}, editable && time >= clip->start && time < clip->end());
    return menu;
}
void Workstation::showTimelineMenu(double time, int track)
{
    if (busy) return;
    const auto clipId = selectedClip;
    selectTrack(track);
    auto menu = timelineMenu(time, track);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&timeline),
        [safe = juce::Component::SafePointer<Workstation>(this), clipId, time, track](int id)
        {
            if (!safe || id == 0 || safe->busy) return;
            if (clipId.isNotEmpty() && std::none_of(safe->engine.project.clips.begin(), safe->engine.project.clips.end(), [&](const auto& c) { return c.id == clipId; })) return;
            safe->selectedClip = clipId; safe->selectTrack(track);
            safe->performTimelineAction((TimelineAction) id, time, track);
        });
}
void Workstation::addTimelineAutomation(AutomationTarget target, double time, int track, bool addUndo)
{
    engine.finishAutomation(); selectTrack(track);
    const juce::ScopedLock guard(engine.lock);
    auto address = target == AutomationTarget::plugin ? pluginLane : AutomationAddress {target == AutomationTarget::master ? -1 : track, target};
    if (target != AutomationTarget::master && address.clipId.isEmpty() && engine.controlClip(track)) address.clipId = selectedClip;
    if (target != AutomationTarget::master && address.clipId.isEmpty()) { setStatus("Select an audio track to add automation."); return; }
    if (engine.project.automationLocked(address)) { setStatus("Unlock the transition before editing automation."); return; }
    auto* curve = engine.project.curve(address); if (!curve) return;
    time = engine.project.automationTime(address, engine.project.snappedTime(time, engine.project.snap));
    if (time < 0) return;
    const float manual = engine.project.manualValue(address), value = curve->interpolatedValue(time, manual);
    if (addUndo) remember();
    engine.project.automationEdited(address);
    curve->enabled = true; if (curve->points.empty() && time > 0) curve->setPoint(0, manual);
    const int point = curve->setPoint(time, value);
    timeline.selectAutomationPoint(address, point);
    refresh(); setStatus("Marker added at " + timeText(time) + ". Drag it to adjust, or double-click for an exact value.");
}
void Workstation::performTimelineAction(TimelineAction action, double time, int track)
{
    if (busy || track < 0 || track >= trackCount || !std::isfinite(time)) return;
    if (action >= TimelineAction::curveRead && engine.exporting) return;
    auto* clip = currentClip(); selectTrack(track);
    switch (action)
    {
        case TimelineAction::pairBassSwap:
        {
            if (!clip || clip->track != track) { setStatus("Right-click the audio track that should receive the bass swap."); return; }
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
            if (engine.project.automationLocked(address)) { setStatus("Unlock the transition before editing automation."); return; }
            auto* curve = engine.project.curve(address); if (!curve || curve->points.empty()) return;
            engine.setAutomationRecordMode(AutomationRecordMode::read); remember(); const juce::ScopedLock guard(engine.lock);
            if (action == TimelineAction::curveRead) curve->enabled = !curve->enabled;
            else { engine.project.automationEdited(address, AutomationEdit::clearCurve); *curve = {}; }
            refresh(); return;
        }
        case TimelineAction::curvePoint: if (automationLane() >= 0) editAutomationPoint(track); return;
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
            if (action == TimelineAction::cut && clip && engine.project.clipTransitionLocked(clip->id)) { setStatus("Unlock the transition before cutting its track."); return; }
            if (!clip || !clip->media->hasPlayback() || clip->media->sourceDetailsPending || clip->pendingPlacement != PendingPlacement::none) return;
            clipClipboard = *clip; if (action == TimelineAction::cut) removeClip();
            setStatus(action == TimelineAction::cut ? "Clip cut. Paste at the cursor to move it; Undo restores it." : "Clip copied. Paste at the cursor on the chosen lane."); break;
        case TimelineAction::paste:
        {
            if (!clipClipboard) return;
            if (engine.project.hasLockedTransitions()) { setStatus("Unlock transitions before pasting tracks with new tempo markers."); return; }
            if (!engine.project.followTrackTempo && clipClipboard->tempoMap)
            { setStatus("Beatmatch this legacy fixed-tempo session before pasting a clip with a changing tempo map."); return; }
            Clip pasted = *clipClipboard; pasted.id = juce::Uuid().toString(); pasted.track = track;
            if (auto error = engine.cloneClipEffects(pasted); error.isNotEmpty()) { fail(error); return; }
            pasted.pendingPlacement = PendingPlacement::none; pasted.pendingPredecessor.clear();
            const auto map = engine.project.followTrackTempo ? std::make_shared<const TempoMap>(engine.project.tempo) : std::shared_ptr<const TempoMap>();
            pasted.retime(engine.project.snappedClipStart(pasted, time, engine.project.snap), map);
            remember(); { const juce::ScopedLock guard(engine.lock); selectedClip = pasted.id;
                if (engine.project.followTrackTempo && !engine.project.tempo.points.empty())
                { const double at = pasted.start + pasted.transitionTime(true);
                  const auto next = std::upper_bound(engine.project.tempo.points.begin(), engine.project.tempo.points.end(), at, [](double t, const auto& p) { return t < p.time; });
                  engine.project.tempo.setTrackPoint(pasted.id, at, engine.project.tempo.bpmAt(at), next == engine.project.tempo.points.end() || next->ramp); }
                engine.project.clips.push_back(std::move(pasted)); }
            refresh(); break;
        }
        case TimelineAction::remove: removeClip(); break;
        case TimelineAction::volume: addTimelineAutomation(AutomationTarget::volume, time, track); break;
        case TimelineAction::treble: addTimelineAutomation(AutomationTarget::treble, time, track); break;
        case TimelineAction::mid: addTimelineAutomation(AutomationTarget::mid, time, track); break;
        case TimelineAction::bass: addTimelineAutomation(AutomationTarget::bass, time, track); break;
        case TimelineAction::tempo: editTempoPoint(-1, engine.project.snappedTime(time, engine.project.snap)); break;
        case TimelineAction::label: editLabelMarker({}, engine.project.snappedTime(time, engine.project.snap)); break;
        case TimelineAction::effect: chooseAutomationPlugin(time); break;
        case TimelineAction::split: engine.seek(engine.project.snappedTime(time, engine.project.snap)); splitClip(); break;
        case TimelineAction::reverse: reverseButton.triggerClick(); break;
        case TimelineAction::tweaks: editAnalysis(); break;
        case TimelineAction::settings: showInspectorPage(0); refresh(); clipSliders[0].grabKeyboardFocus(); break;
        case TimelineAction::downbeat: if (clip) setDownbeatAnchor(clip->id, time - clip->start); break;
        case TimelineAction::loopStart:
            if (!clip || clip->media->sourceDetailsPending || time < clip->start || time >= clip->end()) return;
            loopStartClip = clip->id;
            loopStartSource = clip->sourceElapsed(juce::jlimit(0.0, clip->length, engine.project.snappedTime(time, engine.project.snap) - clip->start));
            timeline.repaint(); setStatus("Loop start marked. Set Loop End later in this clip to repeat that range twice; Loop... changes the repeat count."); break;
        case TimelineAction::loopEnd:
        {
            if (!clip || clip->id != loopStartClip || clip->media->sourceDetailsPending) return;
            if (engine.project.clipTransitionLocked(clip->id)) { setStatus("Unlock the transition before looping its track."); return; }
            const double begin = clip->localTimeForSource(loopStartSource), end = engine.project.snappedTime(time, engine.project.snap) - clip->start;
            if (end <= begin || end > clip->length) { setStatus("Place Loop End after Loop Start, inside the same clip."); return; }
            auto loop = clip->slice(begin, end - begin); const double span = loop.length * loop.rate;
            // A wrapped range has no single contiguous source interval.
            if (clip->loopSpan > 0) { setStatus("Remove the existing loop before marking a new source range."); return; }
            loop.loopSpan = span; loop.loopPhase = 0; loop.length = loop.localTimeForSource(span * 2); loop.rate = span * 2 / loop.length;
            loop.introAnchor.reset(); loop.outroAnchor.reset(); loop.fadeInAnchor.reset(); loop.fadeOutAnchor.reset(); loop.fadeIn = loop.fadeOut = 0;
            remember(); { const juce::ScopedLock guard(engine.lock); scheduleTempoPreparation(); *clip = std::move(loop); }
            loopStartClip.clear(); refresh(); setStatus("Marked source range repeats twice. Loop... adjusts repetition; Undo restores the track."); break;
        }
    }
}
}
