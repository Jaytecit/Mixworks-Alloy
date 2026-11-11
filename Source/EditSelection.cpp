#include "Workstation.h"
#include "PresetNodes.h"
#include <cmath>
#include <cstdlib>
#include <limits>

namespace mixworks
{
bool removeMissingAutomaticTempo(Project&, const Project&);
namespace
{
struct RepeatedSelection
{
    Project project;
    std::vector<juce::String> added;
    juce::String error;
    double end = 0;
    int unlinkedTransitions = 0;
};
juce::Result prepareLinkedRangeEdit(Project& project,const EditSelection& selection,double begin,double end,bool cropping,int& unlinked)
{
    std::vector<juce::String> affected;
    const auto changes=[&](const Clip& clip)
    { return selection.contains(clip.id) && (cropping ? begin>clip.start || end<clip.end() : begin<clip.end() && end>clip.start); };
    const auto survives=[&](const Clip& clip)
    {
        if(!selection.contains(clip.id))return true;
        return cropping ? std::max(begin,clip.start)<std::min(end,clip.end())
            : clip.start<begin || clip.end()>end;
    };
    for(const auto& link:project.transitionLinks)
    {
        const auto* out=project.clipFor(link.outgoing); const auto* in=project.clipFor(link.incoming);
        if(!out || !in)return juce::Result::fail("A connected transition source is unavailable. Select current audio and try again.");
        if(!(changes(*out) || changes(*in)) || !(survives(*out) || survives(*in)))continue;
        for(const auto* clip:{out,in})if(changes(*clip) && std::find(affected.begin(),affected.end(),clip->id)==affected.end())affected.push_back(clip->id);
    }
    const auto count=project.transitionLinks.size();
    if(!affected.empty())
    {
        // Reuse the audible unlink operation before slicing. It materialises
        // treatments and exterior hard cuts, including the unedited partner.
        // The surviving pieces then use ordinary source clocks and shared FX.
        const auto result=project.unlinkClips(affected); if(result.failed())return result;
    }
    unlinked=(int)(count-project.transitionLinks.size()); return juce::Result::ok();
}
bool sameSourceTiming(const Clip& original, const Clip& moved)
{
    if (!std::isfinite(moved.end()) || std::abs(moved.length - original.length) > 1.e-8) return false;
    std::set<double> boundaries {0, original.length};
    for (const auto* clip : std::initializer_list<const Clip*> {&original, &moved}) if (clip->tempoMap)
        for (double slip : {0., clip->audioSlip})
            for (const auto& p : clip->tempoMap->localPoints(clip->start - slip, clip->end() - slip)) boundaries.insert(p.time);
    auto agrees = [&](double t)
    {
        return std::abs(original.sourceElapsed(t) - moved.sourceElapsed(t)) < 1.e-7
            && std::abs(original.audioSourceElapsed(t) - moved.audioSourceElapsed(t)) < 1.e-7;
    };
    double previous = 0;
    for (double t : boundaries)
    { if (!agrees(t) || !agrees((previous + t) / 2)) return false; previous = t; }
    return true;
}
RepeatedSelection planRepeats(const Project& source, const EditFragment& fragment, double end, int copies, Engine* engine)
{
    RepeatedSelection result; result.project = source;
    auto fail = [&](const juce::String& message) { result.error = message; return result; };
    if (copies < 1 || copies > 32 || fragment.clips.empty() || fragment.clips.size() * copies > 128)
        return fail("Choose 1 to 32 additional copies, with at most 128 new segments per edit.");
    const double span = end - fragment.origin;
    if (!std::isfinite(span) || span <= 0 || end + copies * span > 1.e9)
        return fail("The repeated selection must fit inside the timeline.");
    if (source.hasLockedTransitions()) return fail("Unlock transitions before repeating selected audio.");
    const auto map = source.followTrackTempo ? std::make_shared<const TempoMap>(source.tempo) : std::shared_ptr<const TempoMap>();
    for (int repeat = 1; repeat <= copies; ++repeat)
    {
        std::map<juce::String, juce::String> ids;
        for (const auto& original : fragment.clips)
        {
            auto c = original;
            if (!c.media || !c.media->hasPlayback() || c.media->sourceDetailsPending)
                return fail("A selected source is unavailable.");
            if (!source.followTrackTempo && c.tempoMap) return fail("Finish Beatmatch before repeating a captured source clock in a fixed-clock mix.");
            c.id = juce::Uuid().toString(); ids[original.id] = c.id;
            c.retime(original.start + repeat * span, map);
            // Repeats preserve the chosen interval and its internal timing.
            // Reject tempo-shaped stretching instead of changing copy spacing.
            if (!sameSourceTiming(original, c)) return fail("Repeat needs the same source timing at each destination. Choose a constant-tempo region.");
            for (const auto& other : result.project.clips)
                if (other.track == c.track && c.start < other.end() - 1.e-9 && c.end() > other.start + 1.e-9)
                    return fail("Repeat overlaps existing audio on lane " + juce::String(c.track + 1) + " at " + juce::String(c.start - source.tempo.origin, 3) + " s. Crop first or choose a selection with empty space after it.");
            if (engine) if (auto error = engine->cloneClipEffects(c); error.isNotEmpty()) return fail(error);
            c.pendingPlacement = PendingPlacement::none; c.pendingPredecessor.clear();
            result.added.push_back(c.id); result.project.clips.push_back(std::move(c));
        }
        for (auto link : fragment.links)
        { link.outgoing = ids.at(link.outgoing); link.incoming = ids.at(link.incoming); link.locked = false; result.project.transitionLinks.push_back(std::move(link)); }
    }
    const auto valid = result.project.validateTransitionLinks(); if (valid.failed()) return fail(valid.getErrorMessage());
    const auto locks = source.validateTransitionLocks(result.project); if (locks.failed()) return fail(locks.getErrorMessage());
    result.end = end + copies * span; return result;
}
RepeatedSelection planReplacement(const Project& source, const EditSelection& selection, const EditFragment& fragment, Engine* engine)
{
    RepeatedSelection result; result.project = source;
    auto fail = [&](const juce::String& message) { result.error = message; return result; };
    if (selection.clips.empty() || fragment.clips.empty() || fragment.clips.size() > 128)
        return fail("Select destination audio and copy 1 to 128 source segments first.");
    if (source.hasLockedTransitions()) return fail("Unlock transitions before replacing selected audio.");
    double begin = std::numeric_limits<double>::max(), end = 0;
    int lane = trackCount; std::set<int> lanes;
    for (const auto& id : selection.clips)
    {
        const auto* c = source.clipFor(id);
        if (!c || !c->media || !c->media->hasPlayback() || c->media->sourceDetailsPending || c->pendingPlacement != PendingPlacement::none)
            return fail("A selected destination is unavailable or changed.");
        begin = std::min(begin, c->start); end = std::max(end, c->end());
        lane = std::min(lane, c->track); lanes.insert(c->track);
    }
    if (selection.range) { begin = selection.range->getStart(); end = selection.range->getEnd(); }
    if (!std::isfinite(begin) || !std::isfinite(end) || begin < 0 || end <= begin || end > 1.e9)
        return fail("Choose a valid destination interval.");
    auto touched = [&](const Clip& c) { return selection.contains(c.id) && c.start < end && c.end() > begin; };
    std::vector<juce::String> deleted;
    for (const auto& clip : result.project.clips)
        if (touched(clip) && begin <= clip.start && end >= clip.end()) deleted.push_back(clip.id);
    result.project.clearDeletedTrackTransitions(deleted);
    const auto prepared=prepareLinkedRangeEdit(result.project,selection,begin,end,false,result.unlinkedTransitions);
    if(prepared.failed())return fail(prepared.getErrorMessage());
    std::vector<Clip> kept; bool changed = false;
    for (const auto& c : result.project.clips)
    {
        if (!touched(c)) { kept.push_back(c); continue; }
        changed = true;
        const double a = std::max(begin, c.start), b = std::min(end, c.end());
        if (a > c.start) kept.push_back(c.slice(0, a - c.start));
        if (b < c.end())
        {
            auto right = c.slice(b - c.start, c.end() - b);
            if (a > c.start)
            {
                right.id = juce::Uuid().toString();
            }
            kept.push_back(std::move(right));
        }
    }
    if (!changed) return fail("The destination range contains no selected audio.");
    result.project.clips = std::move(kept);
    auto& links = result.project.transitionLinks;
    links.erase(std::remove_if(links.begin(), links.end(), [&](const auto& link)
        { return !result.project.clipFor(link.incoming) || !result.project.clipFor(link.outgoing); }), links.end());
    const auto map = source.followTrackTempo ? std::make_shared<const TempoMap>(source.tempo) : std::shared_ptr<const TempoMap>();
    std::map<juce::String, juce::String> ids;
    for (const auto& original : fragment.clips)
    {
        auto c = original; c.track += lane - fragment.lane;
        if (c.track < 0 || c.track >= trackCount || !lanes.count(c.track))
            return fail("Every copied lane must map to a selected destination lane.");
        if (!c.media || !c.media->hasPlayback() || c.media->sourceDetailsPending || c.pendingPlacement != PendingPlacement::none)
            return fail("A copied source is unavailable or still preparing.");
        if (!source.followTrackTempo && c.tempoMap) return fail("Finish Beatmatch before replacing a captured source clock in a fixed-clock mix.");
        c.retime(begin + original.start - fragment.origin, map);
        if (!sameSourceTiming(original, c)) return fail("Replace needs matching source timing at the destination. Choose a constant-tempo region.");
        if (c.start < begin || c.end() > end + 1.e-9 || c.length <= 0)
            return fail("Clipboard audio is longer than the selected destination. Copy a shorter range or select a longer destination.");
        for (const auto& other : result.project.clips)
            if (other.track == c.track && c.start < other.end() - 1.e-9 && c.end() > other.start + 1.e-9)
                return fail("Replace overlaps unselected or retained audio on lane " + juce::String(c.track + 1) + ". Select that destination audio explicitly.");
        c.id = juce::Uuid().toString(); ids[original.id] = c.id;
        if (engine) if (auto error = engine->cloneClipEffects(c); error.isNotEmpty()) return fail(error);
        c.pendingPlacement = PendingPlacement::none; c.pendingPredecessor.clear();
        result.added.push_back(c.id); result.project.clips.push_back(std::move(c));
    }
    for (auto link : fragment.links)
    {
        if (!ids.count(link.outgoing) || !ids.count(link.incoming)) return fail("The clipboard contains an incomplete transition pair.");
        link.outgoing = ids.at(link.outgoing); link.incoming = ids.at(link.incoming); link.locked = false;
        result.project.transitionLinks.push_back(std::move(link));
    }
    // Orphaned marker ownership is detached; tempo points and their clocks stay in place.
    result.project.tempo.detachMissingTracks(result.project.clips);
    const auto valid = result.project.validateTransitionLinks(); if (valid.failed()) return fail(valid.getErrorMessage());
    const auto locks = source.validateTransitionLocks(result.project); if (locks.failed()) return fail(locks.getErrorMessage());
    result.end = end; return result;
}
}
void Workstation::replaceSelectedAudio(bool commit)
{
    auto refuse = [&](const juce::String& why) { setStatus(why + " Selection, clipboard and mix kept."); };
    if (busy || engine.exporting || pendingTempo > 0 || tempoPreparationPending || engine.project.hasPendingPlacement())
    { refuse("Wait for preparation/export before replacing audio."); return; }
    if (engine.playing || engine.rehearsalActive() || engine.recordingAutomation() || engine.automationRecordMode != AutomationRecordMode::read)
    { refuse("Stop playback and finish automation in Read before replacing audio."); return; }
    if (!editClipboard && !clipClipboard) { refuse("Copy source audio first."); return; }
    if (const auto why=structuralEditUnavailable();why.isNotEmpty()) { refuse(why); return; }
    auto selection = editSelection;
    if (selection.clips.empty() || selection.primary != selectedClip)
    { selection.clear(); selection.primary = selectedClip; selection.clips.push_back(selectedClip); }
    EditFragment fragment;
    if (editClipboard) fragment = *editClipboard;
    else { fragment.clips.push_back(*clipClipboard); fragment.origin = clipClipboard->start; fragment.lane = clipClipboard->track; }
    auto source = projectEditSnapshot(); const auto document = structuralEditDocument(source); source.ensureTransitionLinks();
    auto plan = planReplacement(source, selection, fragment, commit ? &engine : nullptr);
    if (!commit)
    {
        const auto before = juce::JSON::toString(projectToJson(engine.project));
        const auto scope = editSelection; const auto primary = selectedClip; const auto revision = editClipboardRevision;
        double begin = selection.range ? selection.range->getStart() : plan.end;
        if (!selection.range) for (const auto& id : selection.clips) if (auto* c = source.clipFor(id)) begin = std::min(begin, c->start);
        auto* dialog = new juce::AlertWindow("Replace selected audio", plan.error.isNotEmpty() ? plan.error
            : "Replace selected audio from " + juce::String(begin - source.tempo.origin, 3) + " to " + juce::String(plan.end - source.tempo.origin, 3)
                + " s with " + juce::String((int) plan.added.size()) + " copied segments. Shorter copies leave a gap; other audio stays in place."
                + (plan.unlinkedTransitions>0 ? " Unlink "+juce::String(plan.unlinkedTransitions)+" affected transitions, keeping their audible curves and outside-bounds cuts on surviving audio." : juce::String())
                + " Undo restores the mix.",
            juce::MessageBoxIconType::NoIcon, this);
        dialog->addButton("Replace", 1, juce::KeyPress(juce::KeyPress::returnKey));
        dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        dialog->getButton(0)->setEnabled(plan.error.isEmpty());
        dialog->enterModalState(true, juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<Workstation>(this), before, scope, primary, revision](int result)
        {
            if (!safe || result != 1) return;
            if (safe->selectedClip != primary || safe->editSelection.primary != scope.primary || safe->editSelection.clips != scope.clips
                || safe->editSelection.range != scope.range || safe->editClipboardRevision != revision
                || before != juce::JSON::toString(projectToJson(safe->engine.project)))
            { safe->setStatus("Document, selection or clipboard changed. Open Replace again; mix kept."); return; }
            safe->replaceSelectedAudio(true);
        }), true);
        return;
    }
    if (plan.error.isNotEmpty()) { refuse(plan.error); return; }
    if (!publishStructuralEdit(std::move(plan.project),document)) return;
    editSelection.clips = plan.added; selectedClip = plan.added.front(); editSelection.primary = selectedClip;
    engine.selectedTrack = engine.project.clipFor(selectedClip)->track;
    selectedTransition.clear(); timeline.selectAutomationPoint({}, -1); refresh();
    setStatus((plan.unlinkedTransitions>0 ? juce::String("Affected transition frames unlinked; surviving treatments kept as nodes. ") : juce::String())
        + "Selected audio replaced with independent copies. Clipboard, playhead and other positions kept; one Undo restores the mix.");
}
void Workstation::clearEditSelection()
{
    editSelection.clear(); timeline.rangeDragAnchor.reset(); timeline.groupMove.reset();
}
void Workstation::refreshEditSelection()
{
    if (editSelection.primary != selectedClip) clearEditSelection();
    auto& ids = editSelection.clips;
    ids.erase(std::remove_if(ids.begin(), ids.end(), [&](const auto& id) { return !engine.project.clipFor(id); }), ids.end());
    if (ids.empty()) clearEditSelection();
    juce::String text = editSelection.range ? "Audio range / " : "Segments / ";
    text += juce::String((int) ids.size()) + " selected";
    if (const auto r = editSelection.range)
        text += " / " + juce::String(r->getStart() - engine.project.tempo.origin, 3) + " to "
            + juce::String(r->getEnd() - engine.project.tempo.origin, 3) + " s / " + juce::String(r->getLength(), 3) + " s";
    if (const auto* clip = currentClip())
        text = clip->media->libraryTitle() + " / Lane " + juce::String(clip->track + 1) + " / "
            + (ids.empty() ? "Whole track" : text);
    else text = "Select a track or drag across its waveform to highlight audio.";
    editingSelectionInfo.setText(text, juce::dontSendNotification);
    editingSelectionInfo.setTooltip(text);
    editingMove.setToggleState(!editingRange.getToggleState(), juce::dontSendNotification);
    const auto hint=editingPage==2 ? juce::String("Curve on controls playback of this curve. Touch/Latch write moved controls to the playing track on an armed lane.")
        : editingPage==1 ? juce::String("Exact settings affect the selected placement. Outer edge handles trim audio; transition markers stay independent.")
        : juce::String("Drag a waveform to highlight audio, then right-click for tools. Drag its title to move; edge bars trim. Alt ignores snapping.");
    editingHint.setText(hint, juce::dontSendNotification); editingHint.setTooltip(hint);
    editingRangeTime.setEnabled(!busy && !engine.exporting && currentClip());
    editingAll.setEnabled(!busy && !engine.exporting && !engine.project.clips.empty());
    editingClear.setEnabled(!ids.empty());
    editingUnlink.setEnabled(!busy && !engine.exporting && !engine.playing && !engine.rehearsalActive()
        && !editSelection.range && pendingTempo <= 0 && !tempoPreparationPending && !engine.project.hasPendingPlacement()
        && !engine.recordingAutomation() && engine.automationRecordMode == AutomationRecordMode::read
        && std::any_of(engine.project.transitionLinks.begin(), engine.project.transitionLinks.end(), [&](const auto& link)
        { return ids.empty() ? link.incoming == selectedClip || link.outgoing == selectedClip
            : editSelection.contains(link.incoming) || editSelection.contains(link.outgoing); }));
}
void Workstation::selectAllSegments()
{
    if (busy || engine.exporting || engine.project.clips.empty()) return;
    editSelection.clear();
    for (const auto& c : engine.project.clips) editSelection.clips.push_back(c.id);
    if (!engine.project.clipFor(selectedClip)) selectedClip = editSelection.clips.front();
    editSelection.primary = selectedClip;
    selectedTransition.clear();
    selectedAutomationTarget = -1; timeline.editingAutomationPoint.reset();
    timeline.selectedCutAmount.reset(); timeline.selectedLabel.clear(); timeline.selectedSection.reset(); timeline.selectedHotcue.reset();
    refresh(); setStatus("Selected " + juce::String((int) editSelection.clips.size()) + " segments. Copy/Cut/Delete affect this set; Escape clears it.");
}
void Workstation::setEditRange(double begin, double end)
{
    if (busy || engine.exporting || !std::isfinite(begin) || !std::isfinite(end)) return;
    const auto* clip = currentClip(); if (!clip) return;
    if (end < begin) std::swap(begin, end);
    begin = std::max(0.0, begin); end = std::min(1.e9, end);
    if (end <= begin) return;
    if (editSelection.primary != selectedClip || editSelection.clips.empty())
    { editSelection.clear(); editSelection.clips.push_back(selectedClip); }
    editSelection.primary = selectedClip;
    editSelection.range = juce::Range<double>(begin, end);
    selectedTransition.clear();
    selectedAutomationTarget = -1; timeline.editingAutomationPoint.reset();
    timeline.selectedCutAmount.reset(); timeline.selectedLabel.clear(); timeline.selectedSection.reset(); timeline.selectedHotcue.reset();
    refresh();
}
void Workstation::editRangeTime()
{
    if (busy || engine.exporting || !currentClip()) return;
    const auto id = selectedClip;
    const auto before = juce::JSON::toString(projectToJson(engine.project));
    const auto selection = editSelection;
    const auto range = editSelection.range.value_or(juce::Range<double>(currentClip()->start, currentClip()->end()));
    const double origin = engine.project.tempo.origin;
    auto* dialog = new juce::AlertWindow("Select audio range", "", juce::MessageBoxIconType::NoIcon, this);
    dialog->addTextEditor("begin", juce::String(range.getStart() - origin, 9), "Start (s)");
    dialog->addTextEditor("end", juce::String(range.getEnd() - origin, 9), "End (s)");
    dialog->addButton("Select", 1, juce::KeyPress(juce::KeyPress::returnKey));
    dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    dialog->enterModalState(true, juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<Workstation>(this), dialog, id, before, selection, origin](int result)
    {
        if (!safe || result != 1 || safe->busy || safe->engine.exporting) return;
        if (safe->selectedClip != id || before != juce::JSON::toString(projectToJson(safe->engine.project))
            || safe->editSelection.clips != selection.clips || safe->editSelection.range != selection.range)
        { safe->setStatus("Selection or document changed. Open the range editor again."); return; }
        auto number = [](const juce::String& input, double& value)
        {
            const auto s = input.trim(); if (s.isEmpty()) return false;
            const auto utf = s.toStdString(); char* end = nullptr; value = std::strtod(utf.c_str(), &end);
            return end == utf.c_str() + utf.size() && std::isfinite(value);
        };
        double a = 0, b = 0;
        if (!number(dialog->getTextEditorContents("begin"), a) || !number(dialog->getTextEditorContents("end"), b)
            || a + origin < 0 || b <= a || b + origin > 1.e9)
        { safe->setStatus("Range kept. Enter finite start/end seconds inside the timeline, with end after start."); return; }
        safe->setEditRange(a + origin, b + origin);
    }), true);
}

bool Timeline::beginEditSelection(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() || e.y < 0) return false;
    const auto& clips = owner.engine.project.clips;
    const auto found = std::find_if(clips.rbegin(), clips.rend(), [&](const auto& c) { return clipBounds(c).contains(e.position); });
    if (found == clips.rend()) return false;
    // Waveform dragging selects audio; the header moves the placement. Nodes
    // and visible curve/marker handles retain their direct manipulation.
    const bool waveform = e.position.y >= clipBounds(*found).getY() + 20;
    const bool handle = automationPointAt(e.position).has_value() || automationCurveAt(e.position).has_value()
        || hotcueAt(e.position).has_value() || sectionFlagAt(e.position).has_value() || cutAmountAt(e.position).has_value()
        || anchorAt(e.position).has_value() || transitionAt(e.position).has_value() || audioSlipAt(e.position).has_value() || swapHandleAt(e.position).has_value();
    const bool rangeTool = waveform && !e.mods.isCtrlDown() && !handle;
    if (!rangeTool && (!e.mods.isCtrlDown() || automationPointAt(e.position))) return false;
    const auto id = found->id; const int track = found->track;
    auto& s = owner.editSelection;
    if (!rangeTool)
    {
        if (s.primary != owner.selectedClip) s.clear();
        if (s.clips.empty() && owner.engine.project.clipFor(owner.selectedClip)) s.clips.push_back(owner.selectedClip);
        s.range.reset();
        auto it = std::find(s.clips.begin(), s.clips.end(), id);
        if (it != s.clips.end()) s.clips.erase(it); else s.clips.push_back(id);
        owner.selectedClip = s.clips.empty() ? juce::String() : s.contains(id) ? id : s.clips.back();
        s.primary = owner.selectedClip;
        owner.selectedTransition.clear();
        owner.selectedAutomationTarget = -1; editingAutomationPoint.reset(); selectedCutAmount.reset();
        selectedLabel.clear(); selectedHotcue.reset(); selectedSection.reset();
        grabKeyboardFocus(); owner.selectTrack(track); return true;
    }
    rangeDragBefore = s;
    rangeDragBefore.primary = owner.selectedClip;
    if (!s.contains(id)) { s.clear(); s.clips.push_back(id); }
    owner.selectedClip = id; s.primary = id;
    owner.selectedTransition.clear();
    directRangeDrag = owner.editingRangeSize.getSelectedId()==8;
    const int size = directRangeDrag ? (owner.engine.project.snap ? 7 : 1) : owner.editingRangeSize.getSelectedId();
    const bool snapRange = size != 1 && !e.mods.isAltDown();
    rangeDragAnchor = owner.engine.project.snappedTime(e.position.x / pixelsPerSecond, snapRange);
    owner.selectedAutomationTarget = -1; editingAutomationPoint.reset(); selectedCutAmount.reset();
    selectedLabel.clear(); selectedHotcue.reset(); selectedSection.reset();
    if (size > 1 && size < 7)
    {
        const double beats[] {0, 0, 1, 4, 32, 64, 128};
        const auto clock = owner.engine.project.gridClock();
        owner.setEditRange(*rangeDragAnchor, clock.timeAtBeat(clock.beatsAt(*rangeDragAnchor) + beats[size]));
    }
    else s.range.reset();
    grabKeyboardFocus(); owner.selectTrack(track); return true;
}
bool Timeline::continueEditSelection(const juce::MouseEvent& e)
{
    if (!rangeDragAnchor) return false;
    moved = moved || e.mouseWasDraggedSinceMouseDown();
    const int size = directRangeDrag ? (owner.engine.project.snap ? 7 : 1) : owner.editingRangeSize.getSelectedId();
    if (size == 1 || size == 7)
    {
        const double end = owner.engine.project.snappedTime(std::max(0., e.position.x / pixelsPerSecond), size == 7 && !e.mods.isAltDown());
        if (end != *rangeDragAnchor) owner.setEditRange(*rangeDragAnchor, end);
        else { owner.editSelection.range.reset(); owner.refresh(); }
    }
    return true;
}
void Timeline::finishEditSelection()
{
    if (!rangeDragAnchor) return;
    // A click places the cut/paste cursor; an audio-selection drag never seeks.
    if (!moved && !owner.editSelection.range)
    {
        const double at=*rangeDragAnchor;
        // A click selects the hit piece; only a drag keeps a range/group scope.
        // Preserve that piece as selectedClip while clearing the old group.
        owner.clearEditSelection();owner.engine.seek(at);
    }
    rangeDragAnchor.reset();
    owner.refresh();
}
void Timeline::paintEditSelection(juce::Graphics& g)
{
    if (groupMove && groupMove->preview)
    {
        g.setColour(juce::Colours::lightskyblue);
        for (const auto& c : groupMove->preview->clips) if (groupMove->selection.contains(c.id))
        {
            auto r = clipBounds(c);
            g.drawRect(r.reduced(1), 2.f);
            g.drawText("Move group", r.removeFromTop(20), juce::Justification::centred);
        }
    }
    const auto& s = owner.editSelection;
    if (!s.range) return;
    const auto visible = g.getClipBounds().toFloat();
    for (const auto& c : owner.engine.project.clips) if (s.contains(c.id))
    {
        auto r = clipBounds(c);
        const float a = std::max(r.getX(), (float) (s.range->getStart() * pixelsPerSecond));
        const float b = std::min(r.getRight(), (float) (s.range->getEnd() * pixelsPerSecond));
        if (b <= a) continue;
        auto selected = juce::Rectangle<float>(a, r.getY() + 20, b - a, std::max(0.f, r.getHeight() - 20)).getIntersection(visible);
        g.setColour(juce::Colours::lightskyblue.withAlpha(.30f)); g.fillRect(selected);
        g.setColour(juce::Colours::lightskyblue); g.drawRect(selected, 1.5f);
        if (selected.getWidth() >= 62 && selected.getHeight() >= 24)
        {
            g.setColour(colour::text); g.setFont(juce::FontOptions(11));
            g.drawText(juce::String(s.range->getLength(), 3) + " s", selected.reduced(3).removeFromTop(16), juce::Justification::centred);
        }
    }
}

bool Timeline::groupMoveAllowed() const
{
    return !owner.busy && !owner.engine.exporting && owner.pendingTempo == 0 && !owner.tempoPreparationPending
        && !owner.engine.project.hasPendingPlacement() && !owner.engine.playing && !owner.engine.rehearsalActive()
        && !owner.engine.recordingAutomation() && owner.engine.automationRecordMode == AutomationRecordMode::read
        && owner.structuralEditUnavailable().isEmpty();
}
bool Timeline::beginGroupMove(const juce::MouseEvent& e, const juce::String& id)
{
    if (owner.editSelection.range)
    { owner.setStatus("An audio range is selected. Clear the range before moving whole segments."); return true; }
    if (!groupMoveAllowed())
    { owner.setStatus("Stop playback and finish preparation/automation in Read before moving a group."); return true; }
    auto baseline = owner.projectEditSnapshot();
    const auto* c = baseline.clipFor(id); if (!c) return true;
    auto candidate = baseline;
    const auto valid = candidate.moveClipGroup(owner.editSelection.clips, id, c->start, c->track, false);
    if (valid.failed()) { owner.setStatus(valid.getErrorMessage()); return true; }
    owner.selectedClip = id; owner.editSelection.primary = id;
    owner.selectedTransition.clear();
    GroupMoveGesture gesture;
    gesture.baseline = std::move(baseline); gesture.selection = owner.editSelection;
    gesture.anchor = id; gesture.document = owner.structuralEditDocument(gesture.baseline);
    gesture.origin = e.position; gesture.target = c->start; gesture.track = c->track;
    groupMove = std::move(gesture);
    grabKeyboardFocus(); owner.selectTrack(c->track);
    owner.setStatus("Drag to preview the group destination; release to move, Escape to cancel. Alt bypasses Snap.");
    return true;
}
bool Timeline::continueGroupMove(const juce::MouseEvent& e)
{
    if (!groupMove) return false;
    auto& gesture = *groupMove; gesture.preview.reset();
    if (!e.mouseWasDraggedSinceMouseDown()) return true;
    if (!groupMoveAllowed())
    { owner.setStatus("Group move unavailable while playing, preparing or recording. Mix kept."); repaint(); return true; }
    const auto* anchor = gesture.baseline.clipFor(gesture.anchor);
    gesture.target = anchor->start + (e.position.x - gesture.origin.x) / pixelsPerSecond;
    gesture.track = trackAt(e.position.y);
    gesture.snapping = gesture.baseline.snap && !e.mods.isAltDown();
    auto candidate = gesture.baseline;
    const auto result = candidate.moveClipGroup(gesture.selection.clips, gesture.anchor, gesture.target, gesture.track, gesture.snapping);
    if (result.failed()) owner.setStatus(result.getErrorMessage());
    else
    {
        const auto* movedAnchor = candidate.clipFor(gesture.anchor);
        owner.setStatus("Move " + juce::String((int) gesture.selection.clips.size()) + " segments / "
            + juce::String(movedAnchor->start - anchor->start, 3) + " s / release to apply; Escape cancels.");
        gesture.preview = std::move(candidate);
    }
    repaint(); return true;
}
void Timeline::finishGroupMove()
{
    if (!groupMove) return;
    auto gesture = std::move(*groupMove); groupMove.reset(); repaint();
    if (!gesture.preview)
    {
        // A click chooses one piece; dragging still moves an explicit group.
        owner.clearEditSelection(); owner.selectedClip = gesture.anchor;
        owner.refresh(); return;
    }
    auto currentProject = owner.projectEditSnapshot();
    if (!groupMoveAllowed() || owner.selectedClip != gesture.anchor || owner.editSelection.clips != gesture.selection.clips
        || owner.editSelection.range || gesture.document != owner.structuralEditDocument(currentProject))
    { owner.setStatus("Document or selection changed during the group preview. Drag again; mix kept."); return; }
    for (const auto& c : gesture.baseline.clips)
    {
        const auto* current = currentProject.clipFor(c.id);
        if (!current || current->media != c.media || current->tempoMap != c.tempoMap || current->prepared != c.prepared
            || current->elasticSource != c.elasticSource)
        { owner.setStatus("Source preparation changed during the group preview. Drag again; mix kept."); return; }
    }
    // Re-admit from current state; never publish an old gesture snapshot.
    auto candidate = std::move(currentProject);
    const auto result = candidate.moveClipGroup(gesture.selection.clips, gesture.anchor, gesture.target, gesture.track, gesture.snapping);
    if (result.failed()) { owner.setStatus(result.getErrorMessage()); return; }
    if (owner.structuralEditDocument(candidate) == gesture.document) return;
    if (!owner.publishStructuralEdit(std::move(candidate),gesture.document)) return;
    owner.editSelection = gesture.selection; // remember() clears transient selection.
    owner.engine.selectedTrack = owner.engine.project.clipFor(gesture.anchor)->track;
    owner.refresh(); owner.setStatus("Group moved. Source timing and internal links kept; one Undo restores it.");
}

bool Workstation::performSelectionAction(TimelineAction action, double time, int track, int repeatCopies)
{
    const bool cropping = action == TimelineAction::cropSelection;
    const bool repeating = action == TimelineAction::repeatSelection;
    const bool copying = action == TimelineAction::copy || action == TimelineAction::cut;
    const bool removing = cropping || action == TimelineAction::remove || action == TimelineAction::cut;
    const bool pasting = action == TimelineAction::paste && editClipboard.has_value();
    if (!copying && !removing && !pasting && !repeating) return false;
    if (!pasting && !cropping && !repeating && (!editSelection.multiple() || editSelection.primary != selectedClip)) return false;
    auto refuse = [&](const juce::String& why) { setStatus(why + " Selection and mix kept."); return true; };
    if (cropping && (!editSelection.range || editSelection.primary != selectedClip)) return refuse("Select an audio range before cropping.");
    if (busy || engine.exporting || pendingTempo > 0 || tempoPreparationPending || engine.project.hasPendingPlacement())
        return refuse("Wait for preparation/export before editing a range or group.");
    if (engine.playing || engine.rehearsalActive() || engine.recordingAutomation() || engine.automationRecordMode != AutomationRecordMode::read)
        return refuse("Stop playback and finish automation in Read before range/group edits.");
    if (const auto why=structuralEditUnavailable();why.isNotEmpty()) return refuse(why);
    auto candidate = projectEditSnapshot(); const auto document=structuralEditDocument(candidate); candidate.ensureTransitionLinks();
    if (pasting)
    {
        if (track < 0 || track >= trackCount || !std::isfinite(time) || time < 0) return refuse("Choose a valid paste time and lane.");
        if (candidate.hasLockedTransitions()) return refuse("Unlock transitions before pasting a range or group.");
        const auto fragment = *editClipboard;
        const bool continuing = time == pasteEnd && engine.position == pasteEnd;
        const double at = candidate.snappedTime(time, candidate.snap && !continuing);
        std::map<juce::String, juce::String> ids;
        std::vector<juce::String> added;
        double pastedEnd = at;
        const auto map = candidate.followTrackTempo ? std::make_shared<const TempoMap>(candidate.tempo) : std::shared_ptr<const TempoMap>();
        for (auto c : fragment.clips)
        {
            c.track += track - fragment.lane;
            if (c.track < 0 || c.track >= trackCount) return refuse("The selected group does not fit the destination lanes.");
            if (!candidate.followTrackTempo && c.tempoMap) return refuse("Beatmatch a changing source map before pasting into a fixed-clock session.");
            if (!c.media || !c.media->hasPlayback() || c.media->sourceDetailsPending) return refuse("A copied source is unavailable.");
            const auto old = c.id; c.id = juce::Uuid().toString(); ids[old] = c.id;
            c.retime(at + c.start - fragment.origin, map);
            if (!std::isfinite(c.end()) || c.start < 0 || c.end() > 1.e9) return refuse("The pasted range is outside timeline limits.");
            for (const auto& other : candidate.clips) if (other.track == c.track && c.start < other.end() - 1.e-9 && c.end() > other.start + 1.e-9)
                return refuse("Paste overlaps a segment on its destination lane. Choose an empty time or lane.");
            if (auto error = engine.cloneClipEffects(c); error.isNotEmpty()) return refuse(error);
            c.pendingPlacement = PendingPlacement::none; c.pendingPredecessor.clear();
            pastedEnd = std::max(pastedEnd, c.end());
            added.push_back(c.id); candidate.clips.push_back(std::move(c));
        }
        for (auto link : fragment.links)
        { link.outgoing = ids.at(link.outgoing); link.incoming = ids.at(link.incoming); link.locked = false; candidate.transitionLinks.push_back(std::move(link)); }
        const auto valid = candidate.validateTransitionLinks(); if (valid.failed()) return refuse(valid.getErrorMessage());
        if (!publishStructuralEdit(std::move(candidate),document)) return true;
        clearEditSelection();
        editSelection.clips = added; selectedClip = added.front(); editSelection.primary = selectedClip;
        selectedTransition.clear(); timeline.selectAutomationPoint({}, -1); pasteEnd = pastedEnd; engine.seek(pasteEnd); refresh();
        setStatus("Pasted " + juce::String((int) added.size()) + " range/group segments. Existing positions and project tempo kept; one Undo restores the mix.");
        return true;
    }
    auto selection = editSelection;
    if (repeating && (selection.clips.empty() || selection.primary != selectedClip))
    { selection.clear(); selection.primary = selectedClip; selection.clips.push_back(selectedClip); }
    if (selection.clips.empty()) return refuse("Select source audio first.");
    for (const auto& id : selection.clips)
    {
        const auto* c = candidate.clipFor(id);
        if (!c || !c->media || !c->media->hasPlayback() || c->media->sourceDetailsPending || c->pendingPlacement != PendingPlacement::none)
            return refuse("A selected source is unavailable or changed.");
        if (removing && candidate.clipTransitionLocked(id)) return refuse("Unlock the selected transition before deleting its material.");
    }
    int unlinked=0;
    std::vector<juce::String> deleted;
    if (removing && !cropping) for (const auto& id : selection.clips)
    {
        const auto* clip = candidate.clipFor(id);
        if (!selection.range || (selection.range->getStart() <= clip->start && selection.range->getEnd() >= clip->end()))
            deleted.push_back(id);
    }
    EditFragment fragment;
    fragment.origin = selection.range ? selection.range->getStart() : std::numeric_limits<double>::max();
    fragment.lane = trackCount;
    if (copying || repeating)
    {
        // Materialise only in a private copy; selecting/copying cannot change live curves.
        auto copied = candidate;
        for (const auto& link : candidate.transitionLinks) if (selection.contains(link.incoming) || selection.contains(link.outgoing))
        {
            const auto converted = materialisePresetNodes(copied, link.incoming);
            if (converted.failed()) return refuse(converted.getErrorMessage());
        }
        for (const auto& id : selection.clips)
        {
            const auto& c = *copied.clipFor(id);
            double a = c.start, b = c.end();
            const auto playback = candidate.transitionPlaybackFor(*candidate.clipFor(id));
            if (selection.range)
            {
                a = std::max({a, selection.range->getStart(), playback.startCut.value_or(a)});
                b = std::min({b, selection.range->getEnd(), playback.endCut.value_or(b)});
            }
            else for (const auto& link : candidate.transitionLinks)
            {
                // Bake exterior cuts when that relationship is not copied.
                if (link.incoming == id && !selection.contains(link.outgoing)) a = std::max(a, playback.startCut.value_or(a));
                if (link.outgoing == id && !selection.contains(link.incoming)) b = std::min(b, playback.endCut.value_or(b));
            }
            if (b <= a) continue;
            auto copy = c.slice(a - c.start, b - a);
            fragment.origin = std::min(fragment.origin, a); fragment.lane = std::min(fragment.lane, c.track);
            if (copying) if (auto error = engine.cloneClipEffects(copy); error.isNotEmpty()) return refuse(error);
            fragment.clips.push_back(std::move(copy));
        }
        if (fragment.clips.empty()) return refuse("The range contains no audible selected audio.");
        // Node conversion above captured paired treatments. Explicit geometry links only
        // travel with whole selected pairs, never with partial source slices.
        if (!selection.range) for (auto link : copied.transitionLinks)
            if (selection.contains(link.incoming) && selection.contains(link.outgoing))
            {
                const auto out = std::find_if(fragment.clips.begin(),fragment.clips.end(),[&](const auto& c) { return c.id == link.outgoing; });
                if (out != fragment.clips.end() && link.frame)
                {
                    const auto* original = copied.clipFor(link.outgoing);
                    const double shift = original->sourceElapsed(out->start-original->start);
                    for (auto& time : *link.frame) time -= shift;
                }
                link.alternatives = {}; fragment.links.push_back(std::move(link));
            }
    }
    if (repeating)
    {
        double end = selection.range ? selection.range->getEnd() : fragment.origin;
        if (!selection.range) for (const auto& c : fragment.clips) end = std::max(end, c.end());
        if (repeatCopies == 0)
        {
            const auto before = juce::JSON::toString(projectToJson(engine.project));
            const auto scope = editSelection; const auto primary = selectedClip;
            auto* dialog = new juce::AlertWindow("Repeat selected audio", "", juce::MessageBoxIconType::NoIcon, this);
            dialog->addTextEditor("copies", "1", "Additional copies (1 to 32)");
            dialog->addButton("Repeat", 1, juce::KeyPress(juce::KeyPress::returnKey));
            dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
            const auto source = candidate;
            auto update = [dialog, source, fragment, end]
            {
                const auto text = dialog->getTextEditorContents("copies").trim();
                const int count = text.isNotEmpty() && text.containsOnly("0123456789") && text.length() <= 2 ? text.getIntValue() : -1;
                const auto preview = planRepeats(source, fragment, end, count, nullptr);
                const auto message = preview.error.isNotEmpty() ? preview.error : "Add " + juce::String((int) preview.added.size())
                    + " segments from " + juce::String(end - source.tempo.origin, 3) + " to " + juce::String(preview.end - source.tempo.origin, 3)
                    + " s on the same lanes. Original audio and clipboard stay unchanged. One Undo restores the mix.";
                dialog->setMessage(message); dialog->getButton(0)->setEnabled(preview.error.isEmpty());
            };
            dialog->getTextEditor("copies")->onTextChange = update; update();
            dialog->enterModalState(true, juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<Workstation>(this), dialog, before, scope, primary, time, track](int result)
            {
                if (!safe || result != 1) return;
                if (safe->selectedClip != primary || safe->editSelection.clips != scope.clips || safe->editSelection.range != scope.range
                    || before != juce::JSON::toString(projectToJson(safe->engine.project)))
                { safe->setStatus("Document or selection changed. Open Repeat again; mix kept."); return; }
                const auto text = dialog->getTextEditorContents("copies").trim();
                const int count = text.isNotEmpty() && text.containsOnly("0123456789") && text.length() <= 2 ? text.getIntValue() : -1;
                if (count < 1 || count > 32) { safe->setStatus("Enter 1 to 32 additional copies. Mix kept."); return; }
                safe->performSelectionAction(TimelineAction::repeatSelection, time, track, count);
            }), true);
            return true;
        }
        auto repeated = planRepeats(candidate, fragment, end, repeatCopies, &engine);
        if (repeated.error.isNotEmpty()) return refuse(repeated.error);
        if (!publishStructuralEdit(std::move(repeated.project),document)) return true;
        editSelection.clips = repeated.added; selectedClip = repeated.added.front(); editSelection.primary = selectedClip;
        engine.selectedTrack = engine.project.clipFor(selectedClip)->track;
        selectedTransition.clear(); timeline.selectAutomationPoint({}, -1); refresh();
        setStatus("Added " + juce::String(repeatCopies) + " copies with independent effects. Clipboard and playhead kept; one Undo restores the mix.");
        return true;
    }
    if (removing)
    {
        candidate.clearDeletedTrackTransitions(deleted);
        if (selection.range)
        {
            const auto prepared = prepareLinkedRangeEdit(candidate,selection,selection.range->getStart(),selection.range->getEnd(),cropping,unlinked);
            if (prepared.failed()) return refuse(prepared.getErrorMessage());
        }
        std::vector<Clip> clips;
        std::vector<juce::String> retained;
        bool cropChanged = false;
        for (const auto& c : candidate.clips)
        {
            if (!selection.contains(c.id)) { clips.push_back(c); continue; }
            const double a = selection.range ? std::max(c.start, selection.range->getStart()) : c.start;
            const double b = selection.range ? std::min(c.end(), selection.range->getEnd()) : c.end();
            if (b <= a) { if (!cropping) clips.push_back(c); else cropChanged = true; continue; }
            if (cropping)
            {
                const bool trimmed = a != c.start || b != c.end(); cropChanged |= trimmed;
                clips.push_back(trimmed ? c.slice(a - c.start, b - a) : c); retained.push_back(c.id); continue;
            }
            if (a > c.start) clips.push_back(c.slice(0, a - c.start));
            if (b < c.end())
            {
                auto right = c.slice(b - c.start, c.end() - b);
                if (a > c.start)
                {
                    right.id = juce::Uuid().toString();
                }
                clips.push_back(std::move(right));
            }
        }
        if (cropping && retained.empty()) return refuse("The selected range contains no selected audio.");
        if (cropping && !cropChanged) return refuse("The range already contains all selected audio.");
        candidate.clips = std::move(clips);
        candidate.transitionLinks.erase(std::remove_if(candidate.transitionLinks.begin(), candidate.transitionLinks.end(), [&](const auto& link)
        { return !candidate.clipFor(link.incoming) || !candidate.clipFor(link.outgoing); }), candidate.transitionLinks.end());
        const bool changedTempo = !cropping && removeMissingAutomaticTempo(candidate, engine.project);
        if (cropping) candidate.tempo.detachMissingTracks(candidate.clips);
        if (changedTempo && !candidate.clips.empty() && candidate.followTrackTempo)
        {
            if (engine.project.hasLockedTransitions()) return refuse("Unlock transitions before removing their automatic tempo changes.");
            afterTempoPreparation = [safe = juce::Component::SafePointer<Workstation>(this),copying,fragment = std::move(fragment)]() mutable
            {
                if (!safe) return;
                safe->clearEditSelection(); if (!safe->engine.project.clipFor(safe->selectedClip)) safe->selectedClip.clear();
                safe->selectedTransition.clear(); safe->timeline.selectAutomationPoint({},-1);
                if (copying) { safe->editClipboard = std::move(fragment); safe->clipClipboard.reset(); safe->pasteEnd = -1; ++safe->editClipboardRevision; }
                safe->refresh(); safe->setStatus("Selected audio and its automatic tempo markers removed. Remaining audio follows the updated map; Undo restores the mix.");
            };
            prepareTempo(std::move(candidate),false,true); return true;
        }
        const auto valid = candidate.validateTransitionLinks(); if (valid.failed()) return refuse(valid.getErrorMessage());
        const auto locks = engine.project.validateTransitionLocks(candidate); if (locks.failed()) return refuse(locks.getErrorMessage());
        if (juce::JSON::toString(projectToJson(candidate)) == juce::JSON::toString(projectToJson(engine.project))) return refuse("No selected audio intersects the range.");
        if (!publishStructuralEdit(std::move(candidate),document)) return true;
        clearEditSelection();
        if (cropping)
        {
            editSelection = selection; editSelection.clips = retained;
            if (!editSelection.contains(selectedClip)) selectedClip = retained.front();
            editSelection.primary = selectedClip;
            engine.selectedTrack = engine.project.clipFor(selectedClip)->track;
        }
        if (!engine.project.clipFor(selectedClip)) selectedClip.clear();
        selectedTransition.clear(); timeline.selectAutomationPoint({}, -1);
    }
    if (copying) { editClipboard = std::move(fragment); clipClipboard.reset(); pasteEnd = -1; ++editClipboardRevision; }
    refresh(); setStatus((unlinked>0 ? juce::String("Affected transition frames unlinked; surviving treatments kept as nodes. ") : juce::String())
        + (cropping ? juce::String("Selected segments cropped to the range. Other audio, clipboard and playhead kept; one Undo restores the mix.")
        : removing ? "Selected audio removed, leaving a gap. Other positions kept; one Undo restores the mix."
        : "Selected audio copied with independent effects. Paste at the playhead on an empty destination lane/time."));
    return true;
}
}
