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
        const auto bounds = audioSlipBounds(clip); if (bounds.isEmpty()) continue;
        juce::String groupLabel;
        const auto& links = owner.engine.project.transitionLinks;
        if (std::any_of(links.begin(), links.end(), [&](const auto& link) { return link.incoming == clip.id || link.outgoing == clip.id; }))
        {
            auto root = clip.id;
            for (size_t step = 0; step < links.size(); ++step)
            {
                const auto previous = std::find_if(links.begin(), links.end(), [&](const auto& link) { return link.incoming == root; });
                if (previous == links.end()) break;
                root = previous->outgoing;
            }
            const auto first = std::find_if(owner.engine.project.clips.begin(), owner.engine.project.clips.end(), [&](const auto& item) { return item.id == root; });
            groupLabel = "[LINK " + juce::String(1 + (int) std::distance(owner.engine.project.clips.begin(), first)) + "] ";
        }
        if (groupLabel.isNotEmpty())
        {
            auto header = clipBounds(clip).withHeight(22).getIntersection(owner.viewport.getViewArea().toFloat());
            const float width = std::min(56.f, header.getWidth() - 32.f - (clip.stems ? 120.f : 0.f));
            if (width >= 24)
            {
                auto badge = juce::Rectangle<float>(header.getX() + 4 + (clip.stems ? 120.f : 0.f), header.getY() + 2, width, 16);
                g.setColour(colour::panel); g.fillRoundedRectangle(badge, 2);
                g.setColour(colour::orange); g.drawRoundedRectangle(badge, 2, 1);
                g.setFont(juce::FontOptions(10).withStyle("Bold"));
                g.drawText(groupLabel.trim(), badge.reduced(2,0), juce::Justification::centred);
            }
        }
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
        const auto result = candidate.moveClipFollowing(baseline.id, target, candidate.snap && !event.mods.isAltDown());
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
