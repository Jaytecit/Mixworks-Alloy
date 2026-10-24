#include "Model.h"
#include "PresetNodes.h"
#include "EnergySections.h"
#include <cmath>

namespace mixworks
{
juce::Result Project::addTransition(const juce::String& outgoing, const juce::String& incoming, double beats, TransitionPreset preset)
{
    if (hasPendingPlacement() || !std::isfinite(beats) || beats < 0)
        return juce::Result::fail("Wait for preparation and choose a valid transition length.");
    const auto* out = clipFor(outgoing); const auto* in = clipFor(incoming);
    if (!out || !in || out == in || !out->media || !in->media || !out->media->hasPlayback() || !in->media->hasPlayback()
        || out->media->sourceDetailsPending || in->media->sourceDetailsPending)
        return juce::Result::fail("Select two ready, overlapping tracks.");
    if (clipTransitionLocked(outgoing) || clipTransitionLocked(incoming)) return juce::Result::fail("Unlock both tracks before adding a transition.");
    if (std::any_of(transitionLinks.begin(),transitionLinks.end(),[&](const auto& link) { return link.outgoing == outgoing || link.incoming == incoming; }))
        return juce::Result::fail("This outgoing or incoming track already has a transition. Unlink it first.");
    const double first = std::max(out->start,in->start), end = std::min(out->end(),in->end());
    if (end <= first) return juce::Result::fail("Overlap the two tracks before adding a transition.");
    const auto clock = gridClock();
    const double begin = beats > 0 ? std::max(first,clock.timeAtBeat(clock.beatsAt(end)-beats)) : first;
    auto candidate = *this;
    auto* a = candidate.clipFor(outgoing); auto* b = candidate.clipFor(incoming);
    a->outroAnchor = a->sourceElapsed(begin-a->start); b->introAnchor = b->sourceElapsed(begin-b->start);
    TransitionLink link{outgoing,incoming}; link.frame = std::array<double,2>{std::max(0.,a->sourceElapsed(begin-a->start)),std::max(0.,a->sourceElapsed(end-a->start))};
    candidate.transitionLinks.push_back(link);
    auto valid = candidate.validateTransitionLinks(); if (valid.failed()) return valid;
    if (preset != TransitionPreset::alignOnly) { valid = candidate.applyTransitionPreset(incoming,preset); if (valid.failed()) return valid; }
    valid = validateTransitionLocks(candidate); if (valid.failed()) return valid;
    *this = std::move(candidate); return juce::Result::ok();
}
juce::Result Project::setTransitionMuteOutside(const juce::String& incoming, bool enabled)
{
    if (hasPendingPlacement()) return juce::Result::fail("Wait for track preparation before changing transition muting.");
    const auto* link = transitionFor(incoming); if (!link) return juce::Result::fail("Select a transition first.");
    if (clipTransitionLocked(link->outgoing) || clipTransitionLocked(incoming)) return juce::Result::fail("Unlock both tracks before changing transition muting.");
    transitionLinks[(size_t)(link-transitionLinks.data())].muteOutside = enabled;
    return juce::Result::ok();
}
const Project::TransitionLink* Project::transitionFor(const juce::String& incoming) const
{
    const auto found = std::find_if(transitionLinks.begin(), transitionLinks.end(), [&](const auto& link) { return link.incoming == incoming; });
    return found == transitionLinks.end() ? nullptr : &*found;
}
bool Project::hasLockedTransitions() const
{
    return std::any_of(transitionLinks.begin(), transitionLinks.end(), [](const auto& link) { return link.locked; });
}
bool Project::clipTransitionLocked(const juce::String& id) const
{
    return id.isNotEmpty() && std::any_of(transitionLinks.begin(), transitionLinks.end(), [&](const auto& link)
    { return link.locked && (link.outgoing == id || link.incoming == id); });
}
bool Project::automationLocked(const AutomationAddress& address) const
{
    if (address.target == AutomationTarget::master) return false;
    if (address.stem < 0) if (const auto* owner = clipFor(address.clipId))
        return std::any_of(clips.begin(), clips.end(), [&](const auto& clip) { return clip.effects == owner->effects && clipTransitionLocked(clip.id); });
    if (address.clipId.isNotEmpty()) return clipTransitionLocked(address.clipId);
    return std::any_of(clips.begin(), clips.end(), [&](const auto& clip) { return clip.track == address.track && clipTransitionLocked(clip.id); });
}
juce::Result Project::validateTransitionLocks(const Project& candidate) const
{
    const auto fail = [] { return juce::Result::fail("Unlock the affected transition before changing its tracks or tempo."); };
    const auto sameCurve = [](const AutomationCurve& a, const AutomationCurve& b)
    {
        if (a.enabled != b.enabled || a.points.size() != b.points.size()) return false;
        for (size_t i = 0; i < a.points.size(); ++i) if (a.points[i].time != b.points[i].time || a.points[i].value != b.points[i].value || a.points[i].bend != b.points[i].bend) return false;
        return true;
    };
    if (!hasLockedTransitions()) return juce::Result::ok();
    if (bpm != candidate.bpm || followTrackTempo != candidate.followTrackTempo || tempo.origin != candidate.tempo.origin || tempo.fallback != candidate.tempo.fallback
        || tempo.points.size() != candidate.tempo.points.size()) return fail();
    for (size_t i = 0; i < tempo.points.size(); ++i)
    {
        const auto& a = tempo.points[i]; const auto& b = candidate.tempo.points[i];
        if (a.time != b.time || a.bpm != b.bpm || a.ramp != b.ramp || a.clipId != b.clipId) return fail();
    }
    for (const auto& link : transitionLinks) if (link.locked)
    {
        const auto* next = candidate.transitionFor(link.incoming);
        if (!next || !(*next == link)) return fail();
        for (const auto& id : {link.outgoing, link.incoming})
        {
            for (const auto& neighbour : transitionLinks) if (neighbour.outgoing == id || neighbour.incoming == id)
            {
                const auto* changed = candidate.transitionFor(neighbour.incoming);
                if (changed && (changed->bands != neighbour.bands || !(changed->filter == neighbour.filter))) return fail();
                if (!changed && (neighbour.filter.active() || std::any_of(neighbour.bands.begin(), neighbour.bands.end(), [](const auto& band) { return band.has_value(); }))) return fail();
            }
            for (const auto& added : candidate.transitionLinks) if ((added.outgoing == id || added.incoming == id) && !transitionFor(added.incoming)
                && (added.filter.active() || std::any_of(added.bands.begin(), added.bands.end(), [](const auto& band) { return band.has_value(); }))) return fail();
            const auto* a = clipFor(id); const auto* b = candidate.clipFor(id);
            if (!a || !b || a->media != b->media || a->sectionMarkers != b->sectionMarkers || a->hotcues != b->hotcues || a->start != b->start || a->offset != b->offset || a->length != b->length
                || a->rate != b->rate || a->track != b->track || a->reversed != b->reversed || a->loopSpan != b->loopSpan || a->loopPhase != b->loopPhase
                || a->introAnchor != b->introAnchor || a->outroAnchor != b->outroAnchor || a->fadeInAnchor != b->fadeInAnchor
                || a->fadeOutAnchor != b->fadeOutAnchor || a->fadeIn != b->fadeIn || a->fadeOut != b->fadeOut
                || a->gainDb != b->gainDb || effectSettings(*a).gainDb != candidate.effectSettings(*b).gainDb || effectSettings(*a).pan != candidate.effectSettings(*b).pan
                || effectSettings(*a).eqDb != candidate.effectSettings(*b).eqDb || effectSettings(*a).filterCutoff != candidate.effectSettings(*b).filterCutoff) return fail();
            if (a->stemDirectory != b->stemDirectory || a->stemManifestHash != b->stemManifestHash || a->stemsEnabled != b->stemsEnabled) return fail();
            for (int s = 0; s < stemCount; ++s)
            {
                const auto& x = a->stemControls[(size_t) s]; const auto& y = b->stemControls[(size_t) s];
                if (x.gainDb != y.gainDb || x.pan != y.pan || x.mute != y.mute || x.solo != y.solo
                    || !sameCurve(x.automation[0], y.automation[0]) || !sameCurve(x.automation[4], y.automation[4])) return fail();
            }
            for (size_t i = 0; i < effectSettings(*a).automation.size(); ++i)
                if (!sameCurve(effectSettings(*a).automation[i], candidate.effectSettings(*b).automation[i])) return fail();
            const auto& beforeEffects = effectSettings(*a).pluginAutomation;
            const auto& afterEffects = candidate.effectSettings(*b).pluginAutomation;
            if (beforeEffects.size() != afterEffects.size()) return fail();
            for (size_t i = 0; i < beforeEffects.size(); ++i)
            {
                const auto& x = beforeEffects[i]; const auto& y = afterEffects[i];
                if (x.slotId != y.slotId || x.parameterId != y.parameterId || x.manualValue != y.manualValue || !sameCurve(x.curve, y.curve)) return fail();
            }
        }
    }
    return juce::Result::ok();
}
juce::Result Project::setTransitionLocked(const juce::String& incoming, bool locked)
{
    if (hasPendingPlacement()) return juce::Result::fail("Wait for track placement to finish.");
    ensureTransitionLinks();
    const auto valid = validateTransitionLinks(); if (valid.failed()) return valid;
    for (auto& link : transitionLinks) if (link.incoming == incoming) { link.locked = locked; return juce::Result::ok(); }
    return juce::Result::fail("The transition is unavailable.");
}
juce::Result Project::setTransitionLength(const juce::String& incoming, double bars)
{
    if (!std::isfinite(bars) || bars <= 0) return juce::Result::fail("Choose a positive transition length.");
    const auto* link = transitionFor(incoming); if (!link) return juce::Result::fail("Select a transition first.");
    const auto range = transitionRange(*link);
    const auto map = gridClock();
    const double end = map.timeAtBeat(map.beatsAt(range.getStart()) + bars * 4);
    return editTransition(incoming, TransitionEdit::resizeEnd, end - range.getEnd(), false);
}
void Project::clearTransitionBass(TransitionLink& link, bool preservePoints)
{
    if (!link.bassSwap) return;
    if (preservePoints) for (const auto& id : {link.outgoing, link.incoming}) clipFor(id)->automaticBass = false;
    if (!preservePoints) for (size_t side = 0; side < 2; ++side)
    {
        auto* clip = clipFor(side == 0 ? link.outgoing : link.incoming);
        auto& points = effectSettings(*clip).automation[(size_t) AutomationTarget::bass].points;
        const double a = (*link.bassSwap)[side * 2], b = (*link.bassSwap)[side * 2 + 1];
        points.erase(std::remove_if(points.begin(), points.end(), [&](const auto& point)
        { return std::abs(point.time - a) < 1e-7 || std::abs(point.time - b) < 1e-7; }), points.end());
        // Earlier producers wrote a redundant opening hold, identified by their ownership flag.
        if (clip->automaticBass && (side == 1 || std::none_of(transitionLinks.begin(), transitionLinks.end(),
            [&](const auto& other) { return other.incoming == clip->id; })))
            points.erase(std::remove_if(points.begin(), points.end(), [&](const auto& point)
            { return point.time == 0 && point.value == (side == 1 ? -60.f : 0.f); }), points.end());
    }
    link.bassSwap.reset();
}
juce::Result Project::applyTransitionPreset(const juce::String& incoming, TransitionPreset preset)
{
    if (hasPendingPlacement()) return juce::Result::fail("Wait for track placement to finish.");
    if (preset != TransitionPreset::alignOnly && preset != TransitionPreset::bassSwap && preset != TransitionPreset::crossfade)
        return juce::Result::fail("Unknown transition preset.");
    const auto valid = validateTransitionLinks(); if (valid.failed()) return valid;
    const auto* current = transitionFor(incoming); if (!current) return juce::Result::fail("Select a transition first.");
    if (current->locked) return juce::Result::fail("Unlock this transition before applying a preset.");
    const auto range = transitionRange(*current);
    if (range.getLength() <= 1e-6) return juce::Result::fail("Set a non-zero transition length first.");
    TransitionStyle style;
    style.volume = preset == TransitionPreset::crossfade ? std::array<double,4>{0,1,0,1} : std::array<double,4>{0,0,1,1};
    if (preset == TransitionPreset::bassSwap)
        style.bands[1] = TransitionBandSettings{TransitionBandMode::swap,1,0,sectionSwapPosition(*this,*current)};
    return applyTransitionStyle(incoming,style);
}
}
