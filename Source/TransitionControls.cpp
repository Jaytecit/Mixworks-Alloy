#include "Model.h"
#include <cmath>

namespace mixworks
{
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
    if (address.clipId.isNotEmpty()) return clipTransitionLocked(address.clipId);
    return std::any_of(clips.begin(), clips.end(), [&](const auto& clip) { return clip.track == address.track && clipTransitionLocked(clip.id); });
}
juce::Result Project::validateTransitionLocks(const Project& candidate) const
{
    const auto fail = [] { return juce::Result::fail("Unlock the affected transition before changing its tracks or tempo."); };
    const auto sameCurve = [](const AutomationCurve& a, const AutomationCurve& b)
    {
        if (a.enabled != b.enabled || a.points.size() != b.points.size()) return false;
        for (size_t i = 0; i < a.points.size(); ++i) if (a.points[i].time != b.points[i].time || a.points[i].value != b.points[i].value) return false;
        return true;
    };
    if (!hasLockedTransitions()) return juce::Result::ok();
    if (bpm != candidate.bpm || followTrackTempo != candidate.followTrackTempo || tempo.fallback != candidate.tempo.fallback
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
            if (!a || !b || a->media != b->media || a->start != b->start || a->offset != b->offset || a->length != b->length
                || a->rate != b->rate || a->track != b->track || a->reversed != b->reversed || a->loopSpan != b->loopSpan || a->loopPhase != b->loopPhase
                || a->introAnchor != b->introAnchor || a->outroAnchor != b->outroAnchor || a->fadeInAnchor != b->fadeInAnchor
                || a->fadeOutAnchor != b->fadeOutAnchor || a->fadeIn != b->fadeIn || a->fadeOut != b->fadeOut
                || a->gainDb != b->gainDb || a->controls.gainDb != b->controls.gainDb || a->controls.pan != b->controls.pan
                || a->controls.eqDb != b->controls.eqDb || a->controls.pluginAutomation.size() != b->controls.pluginAutomation.size()) return fail();
            for (size_t i = 0; i < a->controls.automation.size(); ++i)
                if (!sameCurve(a->controls.automation[i], b->controls.automation[i])) return fail();
            for (size_t i = 0; i < a->controls.pluginAutomation.size(); ++i)
            {
                const auto& x = a->controls.pluginAutomation[i]; const auto& y = b->controls.pluginAutomation[i];
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
    const auto map = followTrackTempo ? tempo : TempoMap {bpm, {}};
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
        auto& points = clip->controls.automation[(size_t) AutomationTarget::bass].points;
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
    Project candidate = *this;
    auto* out = candidate.clipFor(current->outgoing); auto* in = candidate.clipFor(incoming);
    if (range.getStart() < std::max(out->start, in->start) - 1e-7 || range.getEnd() > std::min(out->end(), in->end()) + 1e-7)
        return juce::Result::fail("Keep the transition inside both songs before applying a preset.");
    auto& link = candidate.transitionLinks[(size_t) (current - transitionLinks.data())];
    candidate.clearTransitionBass(link);
    link.bands = {}; link.filter = {};
    if (preset == TransitionPreset::bassSwap)
        for (const auto* clip : {out, in})
            for (const auto& point : clip->controls.automation[(size_t) AutomationTarget::bass].points)
            {
                const double time = clip->start + clip->localTimeForSource(point.time);
                if (time >= range.getStart() - 1e-7 && time <= range.getEnd() + 1e-7)
                    return juce::Result::fail("This transition contains manual or neighbouring bass points. Keep them or remove them before applying Bass swap.");
            }
    // Fades are the frame's existing owner. Keep the frame and source cues fixed even with None.
    in->fadeIn = out->fadeOut = 0;
    in->fadeInAnchor = in->sourceElapsed(range.getStart() - in->start);
    out->fadeOutAnchor = out->sourceElapsed(range.getEnd() - out->start);
    const bool outAutomatic = out->automaticBass, inAutomatic = in->automaticBass;
    link.bassSwap = initialiseTransition(*out, *in, range.getLength(), preset, range);
    // Local ownership is the explicit link, not permission to rewrite an entire manual curve.
    out->automaticBass = outAutomatic; in->automaticBass = inAutomatic;
    if (preset == TransitionPreset::bassSwap)
    { candidate.clearTransitionBass(link); link.bands[1] = TransitionBandSettings {TransitionBandMode::swap, 1, 0, .5}; }
    const auto checked = candidate.validateTransitionLinks(); if (checked.failed()) return checked;
    const auto protection = validateTransitionLocks(candidate); if (protection.failed()) return protection;
    *this = std::move(candidate); return juce::Result::ok();
}
}
