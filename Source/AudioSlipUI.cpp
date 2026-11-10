#include "Workstation.h"
#include <cmath>

namespace mixworks
{
juce::Rectangle<float> Timeline::audioSlipBounds(const Clip& clip) const
{
    if (laneSize(clip.track) < 30) return {};
    auto header = clipBounds(clip).withHeight(22).getIntersection(owner.viewport.getViewArea().toFloat());
    if (header.getWidth() < 8 || header.getHeight() < 18) return {};
    const float width = std::min(28.f, header.getWidth());
    return {header.getRight() - width, header.getY() + 1, width, 18};
}
std::optional<juce::String> Timeline::audioSlipAt(juce::Point<float> point) const
{
    for (auto it = owner.engine.project.clips.rbegin(); it != owner.engine.project.clips.rend(); ++it)
        if (audioSlipBounds(*it).contains(point)) return it->id;
    return {};
}
void Timeline::paintAudioSlipHandles(juce::Graphics& g)
{
    for (const auto& clip : owner.engine.project.clips)
    {
        for (bool left : {true,false})
        {
            const auto trim=edgeTrimBounds(clip,left); if (trim.isEmpty()) continue;
            g.setColour(colour::panel); g.fillRoundedRectangle(trim,2);
            g.setColour(colour::orange.withAlpha(owner.engine.project.clipTransitionLocked(clip.id) ? .35f : .9f));
            g.drawRoundedRectangle(trim.reduced(.5f),2,1);
            g.drawVerticalLine((int)trim.getCentreX(),trim.getY()+5,trim.getBottom()-5);
        }
        const auto bounds = audioSlipBounds(clip); if (bounds.isEmpty()) continue;
        const auto c = bounds.getCentre(); juce::Path hex; hex.addPolygon(c, 6, 8.5f);
        g.setColour(colour::panel); g.fillPath(hex);
        const bool locked = owner.engine.project.clipTransitionLocked(clip.id);
        g.setColour(locked ? colour::muted.withAlpha(.5f) : clip.audioSlip != 0 ? colour::orange : colour::text);
        g.strokePath(hex, juce::PathStrokeType(1));
        juce::Path arrows; arrows.startNewSubPath(c.x - 6, c.y); arrows.lineTo(c.x + 6, c.y);
        arrows.startNewSubPath(c.x - 3, c.y - 3); arrows.lineTo(c.x - 6, c.y); arrows.lineTo(c.x - 3, c.y + 3);
        arrows.startNewSubPath(c.x + 3, c.y - 3); arrows.lineTo(c.x + 6, c.y); arrows.lineTo(c.x + 3, c.y + 3);
        g.strokePath(arrows, juce::PathStrokeType(1.2f));
    }
}
void Timeline::refreshMovementLocks()
{
    std::set<juce::String> retained;
    for(const auto& link:owner.engine.project.transitionLinks)
    {
        const auto* in=owner.engine.project.clipFor(link.incoming); const auto* out=owner.engine.project.clipFor(link.outgoing);
        if(!in || !out)continue;
        retained.insert(link.incoming); auto& button=movementLockButtons[link.incoming];
        if(!button)
        {
            button=std::make_unique<juce::TextButton>(); button->setClickingTogglesState(true);
            button->setComponentID("pair-movement-"+link.incoming); addAndMakeVisible(*button);
            button->onClick=[safe=juce::Component::SafePointer<Workstation>(&owner),incoming=link.incoming]
                {if(safe)safe->togglePairMovementLock(incoming);};
        }
        const auto header=clipBounds(*in).withHeight(22); const float width=std::min(72.f,header.getWidth()-36-(in->stems ? 120.f : 0.f));
        button->setVisible(laneSize(in->track)>=30 && width>=48);
        button->setBounds(juce::Rectangle<float>(header.getX()+4+(in->stems ? 120.f : 0.f),header.getY()+2,std::max(0.f,width),18).toNearestInt());
        button->setButtonText(link.movementLocked ? "Locked" : "Unlocked"); button->setToggleState(link.movementLocked,juce::dontSendNotification);
        const auto pair=out->media->libraryTitle()+" -> "+in->media->libraryTitle();
        button->setTitle("Pair movement lock / "+pair);
        button->setTooltip(pair+": lock their track movement together. Consecutive locked pairs form a group; unlock this pair before changing its IN/OUT alignment.");
        button->setDescription(link.movementLocked ? "Locked: these two tracks move together by their headers." : "Unlocked: these two tracks can move independently by their headers.");
        const auto why=owner.busy || owner.pendingTempo>0 || owner.tempoPreparationPending || owner.engine.project.hasPendingPlacement()
            ? juce::String("Wait for preparation before changing this pair's movement lock.") : owner.engine.exporting ? juce::String("Finish or cancel export before changing a movement lock.") : juce::String();
        button->setEnabled(why.isEmpty()); button->getProperties().set("mixworksHintDisabled",why);
    }
    for(auto it=movementLockButtons.begin();it!=movementLockButtons.end();)if(!retained.count(it->first))it=movementLockButtons.erase(it);else ++it;
}
juce::Rectangle<float> Timeline::edgeTrimBounds(const Clip& clip, bool left) const
{
    const auto bounds=clipBounds(clip); if (bounds.getHeight()<32 || bounds.getWidth()<16) return {};
    return {left ? bounds.getX() : bounds.getRight()-6, bounds.getCentreY()-12, 6, 24};
}
std::optional<std::pair<juce::String,bool>> Timeline::edgeTrimAt(juce::Point<float> point) const
{
    for (auto it=owner.engine.project.clips.rbegin(); it!=owner.engine.project.clips.rend(); ++it)
        for (bool left : {true,false}) if (edgeTrimBounds(*it,left).expanded(2,2).contains(point)) return std::make_pair(it->id,left);
    return {};
}
bool Timeline::beginEdgeTrim(const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu() || event.mods.isCtrlDown() || event.mods.isShiftDown()) return false;
    const auto hit=edgeTrimAt(event.position); if (!hit) return false;
    if (owner.pendingTempo>0 || owner.tempoPreparationPending || owner.engine.exporting || owner.engine.capturingPerformance()
        || owner.engine.recordingAutomation() || owner.engine.automationRecordMode!=AutomationRecordMode::read)
    { owner.setStatus("Finish preparation, export or recording and choose Read before trimming track edges."); return true; }
    const auto* clip=owner.engine.project.clipFor(hit->first); if (!clip) return true;
    auto candidate=owner.engine.project;
    const auto ready=candidate.trimClip(clip->id,clip->start,clip->end());
    if (ready.failed()) { owner.setStatus(ready.getErrorMessage()); return true; }
    const auto baseline=*clip; owner.clearEditSelection(); owner.selectedClip=baseline.id; owner.selectedAutomationTarget=-1;
    edgeTrimDrag=EdgeTrimGesture{baseline,event.position.x,hit->second,baseline.start,baseline.length};
    owner.selectTrack(baseline.track); owner.refresh();
    owner.setStatus(hit->second ? "Trim start: drag to hide or restore source audio. Alt ignores snap; transition markers stay fixed."
        : "Trim end: drag to hide or restore source audio. Alt ignores snap; transition markers stay fixed.");
    return true;
}
bool Timeline::continueEdgeTrim(const juce::MouseEvent& event)
{
    if (!edgeTrimDrag) return false;
    if (!event.mouseWasDraggedSinceMouseDown() || owner.busy || owner.engine.exporting || owner.pendingTempo>0 || owner.tempoPreparationPending) return true;
    const auto gesture=*edgeTrimDrag; const auto* current=owner.engine.project.clipFor(gesture.baseline.id);
    if (!current || current->media!=gesture.baseline.media || current->tempoMap!=gesture.baseline.tempoMap
        || current->start!=gesture.lastStart || current->length!=gesture.lastLength)
    { owner.setStatus("The track changed. Release and grab its trim edge again."); return true; }
    const double reference=gesture.left ? gesture.baseline.start : gesture.baseline.end();
    const double target=owner.engine.project.snappedMoveTime(reference,reference+(event.position.x-gesture.x)/pixelsPerSecond,
        owner.engine.project.snap && !event.mods.isAltDown());
    const double begin=gesture.left ? std::max(0.,std::min(target,gesture.baseline.end()-.001)) : gesture.baseline.start;
    const double end=gesture.left ? gesture.baseline.end() : std::max(target,gesture.baseline.start+.001);
    if (begin==current->start && end==current->end()) return true;
    auto candidate=owner.engine.project; const auto result=candidate.trimClip(current->id,begin,end);
    if (result.failed()) { owner.setStatus(result.getErrorMessage()); return true; }
    bool conflict=false;
    for (const auto& link:candidate.transitionLinks) if (link.outgoing==current->id || link.incoming==current->id)
    { const auto range=candidate.transitionRange(link); conflict |= range.getStart()<begin || range.getEnd()>end; }
    { const juce::ScopedLock guard(owner.engine.lock); if (!moved) { owner.remember(); moved=true; } owner.engine.project=std::move(candidate); }
    edgeTrimDrag->lastStart=begin; edgeTrimDrag->lastLength=end-begin;
    updateSize(); owner.playlistTable.table.repaint(); repaint();
    owner.setStatus(conflict ? "Trimmed. Transition markers stay fixed outside this audio range; excluded audio is silent. Extend the edge to restore it."
        : "Trimmed this placement. Excluded audio is silent; extend the edge to restore it. Transition markers kept.");
    return true;
}
bool Timeline::beginAudioSlip(const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu()) return false;
    const auto hit = audioSlipAt(event.position); if (!hit) return false;
    if (owner.pendingTempo > 0 || owner.tempoPreparationPending || owner.engine.exporting)
    { owner.setStatus("Wait for preparation or export before slipping audio."); return true; }
    const juce::ScopedLock guard(owner.engine.lock);
    const auto* clip = owner.engine.project.clipFor(*hit); if (!clip) return true;
    const auto ready = owner.engine.project.setClipAudioSlip(*hit, clip->audioSlip);
    if (ready.failed()) { owner.setStatus(ready.getErrorMessage()); return true; }
    owner.selectedClip = *hit; owner.selectTrack(clip->track);
    audioSlipDrag = AudioSlipGesture {*clip, event.position.x, event.mods.isShiftDown(), clip->start, clip->length};
    owner.refresh(); owner.setStatus(event.mods.isShiftDown() ? "Moving audio." : "Moving tracks.");
    return true;
}
bool Timeline::continueAudioSlip(const juce::MouseEvent& event)
{
    if (!audioSlipDrag) return false;
    if (owner.busy || owner.engine.exporting || !event.mouseWasDraggedSinceMouseDown()) return true;
    if (owner.pendingTempo > 0 || owner.tempoPreparationPending) return true;
    const auto& baseline = audioSlipDrag->baseline;
    if (!audioSlipDrag->audioOnly)
    {
        owner.engine.flushMidiControls(); owner.engine.finishAutomation();
        const juce::ScopedLock guard(owner.engine.lock);
        const auto* current = owner.engine.project.clipFor(baseline.id);
        if (!current || current->media != baseline.media || current->tempoMap != baseline.tempoMap
            || current->start != audioSlipDrag->lastStart || current->length != audioSlipDrag->lastLength)
        { owner.setStatus("The track clock changed. Release and drag its move handle again."); return true; }
        const double target = baseline.start + (event.position.x - audioSlipDrag->x) / pixelsPerSecond;
        auto candidate = owner.engine.project;
        const auto result = candidate.moveClip(baseline.id, target, candidate.snap && !event.mods.isAltDown());
        if (result.failed()) { owner.setStatus(result.getErrorMessage()); return true; }
        const auto* movedClip = candidate.clipFor(baseline.id);
        if (std::abs(movedClip->start - current->start) < 1e-9) return true;
        if (!moved) { owner.remember(); moved = true; }
        audioSlipDrag->lastStart = movedClip->start; audioSlipDrag->lastLength = movedClip->length;
        owner.engine.project = std::move(candidate); updateSize(); owner.tempoStrip.repaint();
        owner.setStatus("Tracks moved.");
        repaint(); return true;
    }
    const juce::ScopedLock guard(owner.engine.lock);
    auto* clip = owner.engine.project.clipFor(baseline.id); if (!clip) return true;
    if (clip->media != baseline.media || clip->tempoMap != baseline.tempoMap || clip->start != baseline.start || clip->length != baseline.length)
    { owner.setStatus("The track clock changed. Release and drag its slip handle again."); return true; }
    const double reference = baseline.start + baseline.firstBeatTime();
    const auto clock = owner.engine.project.gridClock();
    const double target = clock.snapWithPreroll(reference + (event.position.x - audioSlipDrag->x) / pixelsPerSecond,
        owner.engine.project.snap && !event.mods.isAltDown(), owner.engine.project.snapStep());
    const double slip = juce::jlimit(-clip->length, clip->length, baseline.audioSlip + target - reference);
    if (std::abs(slip - clip->audioSlip) < 1.e-9) return true;
    auto candidate = owner.engine.project; const auto result = candidate.setClipAudioSlip(clip->id, slip);
    if (result.failed()) { owner.setStatus(result.getErrorMessage()); return true; }
    if (!moved) { owner.engine.flushMidiControls(); owner.engine.finishAutomation(); owner.remember(); moved = true; }
    // Apply only this property; never restore a mouse-down project over live edits.
    clip->audioSlip = slip;
    owner.setStatus("Audio slip " + juce::String(slip, 3) + " s"); repaint();
    return true;
}
bool Timeline::resetAudioSlip(juce::Point<float> point)
{
    const auto hit = audioSlipAt(point); if (!hit) return false;
    if (owner.pendingTempo > 0 || owner.tempoPreparationPending || owner.engine.exporting) return true;
    const juce::ScopedLock guard(owner.engine.lock); auto* clip = owner.engine.project.clipFor(*hit);
    if (!clip || clip->audioSlip == 0) return true;
    auto candidate = owner.engine.project; const auto result = candidate.setClipAudioSlip(*hit, 0);
    if (result.failed()) { owner.setStatus(result.getErrorMessage()); return true; }
    owner.engine.flushMidiControls(); owner.engine.finishAutomation(); owner.remember(); clip->audioSlip = 0;
    owner.refresh(); owner.setStatus("Audio position reset."); return true;
}
}
