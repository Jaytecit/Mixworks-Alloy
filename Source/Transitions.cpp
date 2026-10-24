#include "Model.h"
#include "ElasticAudio.h"
#include "PresetNodes.h"
#include <cmath>
#include <map>
#include <set>

namespace mixworks
{
namespace
{
int bassPoint(const Project& project, const Clip& clip, double source)
{
    const auto& points = project.effectSettings(clip).automation[(size_t) AutomationTarget::bass].points;
    for (size_t i = 0; i < points.size(); ++i) if (points[i].time == source) return (int) i;
    for (size_t i = 0; i < points.size(); ++i) if (std::abs(points[i].time - source) < 1e-7) return (int) i;
    return -1;
}
struct NodeSwapSide { size_t first = 0, last = 0; double begin = 0, end = 0; bool sharp = false; };
std::optional<std::array<NodeSwapSide,2>> ordinarySwap(const Project& project, const Project::TransitionLink& link, int band)
{
    if (band < 0 || band > 3 || (link.bands[(size_t)band] && link.bands[(size_t)band]->generated()) || (band == 1 && link.bassSwap)) return {};
    const auto range = project.transitionRange(link); if (range.getLength() <= 1.e-7) return {};
    std::array<NodeSwapSide,2> result;
    std::array<float,2> high {}, low {};
    for (size_t side = 0; side < 2; ++side)
    {
        const auto* clip = project.clipFor(side == 0 ? link.outgoing : link.incoming); if (!clip) return {};
        const auto& curve = project.effectSettings(*clip).automation[(size_t)band]; if (!curve.active()) return {};
        const double begin = clip->sourceElapsed(range.getStart()-clip->start), end = clip->sourceElapsed(range.getEnd()-clip->start);
        bool found = false;
        auto& span = result[side];
        for (size_t i = 1; i < curve.points.size(); ++i)
        {
            const auto& a = curve.points[i-1]; const auto& b = curve.points[i];
            // Boundary guards belong to exterior audio, not to the handover.
            if (b.time <= begin+1.e-8 || a.time >= end-1.e-8 || a.value == b.value) continue;
            if (a.time < begin-1.e-7 || b.time > end+1.e-7 || (side == 0 ? b.value >= a.value : b.value <= a.value)) return {};
            if (found && i != span.last+1) return {}; // ambiguous, separately authored changes
            if (!found) span.first = i-1;
            span.last = i; found = true;
        }
        if (!found) return {};
        span.sharp = span.last == span.first+1 && curve.points[span.first].bend >= 1;
        span.begin = clip->start+clip->localTimeForSource(curve.points[span.first].time);
        span.end = clip->start+clip->localTimeForSource(curve.points[span.last].time);
        // A full-frame crossfade is not a movable swap. Short fade handovers
        // and sharp steps are derived from their real editable nodes.
        if (!span.sharp && span.end-span.begin > range.getLength()*.5) return {};
        high[side] = curve.points[side == 0 ? span.first : span.last].value;
        low[side] = curve.points[side == 0 ? span.last : span.first].value;
    }
    if (std::abs(high[0]-high[1]) > .001f || std::abs(low[0]-low[1]) > .001f || high[0]-low[0] < .001f) return {};
    return result;
}
}
std::optional<std::array<double, 4>> initialiseTransition(Clip& previous, Clip& incoming, double overlap, TransitionPreset preset, std::optional<juce::Range<double>> window)
{
    // Alignment guides and EQ belong to source positions, independent of volume fades.
    const double downbeat = incoming.firstDownbeatTime();
    const double cue = downbeat >= 0 ? downbeat : incoming.firstBeatTime();
    const double end = window ? window->getEnd() : std::min(previous.end(), incoming.end());
    const double cueTime = incoming.start + cue;
    // Alignment cues include the outgoing endpoint. The audio overlap remains
    // half-open; its strict fade-window test must not replace an edge IN with zero.
    const bool alignmentEndpoint = preset == TransitionPreset::alignOnly && std::abs(cueTime - end) < 1e-7;
    const double begin = window ? window->getStart() : (cueTime < end || alignmentEndpoint ? cueTime : incoming.start);
    if (!window && (preset != TransitionPreset::alignOnly || !previous.outroAnchor))
        previous.outroAnchor = overlap > 0 ? std::optional<double>(previous.sourceElapsed(begin - previous.start)) : std::nullopt;
    if (!window) incoming.introAnchor = incoming.sourceElapsed(begin - incoming.start);
    if (preset == TransitionPreset::alignOnly || overlap <= 0 || end <= begin) return {};
    if (preset == TransitionPreset::crossfade)
    {
        previous.fadeOutAnchor = previous.sourceElapsed(begin - previous.start); previous.fadeOut = end - begin;
        incoming.fadeInAnchor = incoming.sourceElapsed(begin - incoming.start); incoming.fadeIn = end - begin;
        return {};
    }
    double rampBegin = (begin + end) / 2 - std::min((end - begin) / 8, 60 / std::max(20.0, incoming.media->bpm())) / 2;
    double rampEnd = begin + end - rampBegin;
    if (incoming.tempoMap)
    {
        const auto& clock = *incoming.tempoMap;
        const double first = clock.beatsAt(begin), last = clock.beatsAt(end);
        const double width = std::min(1.0, (last - first) / 8);
        double quantum = previous.firstDownbeatTime() >= 0 && incoming.firstDownbeatTime() >= 0 ? 4 : 1;
        if (std::ceil((first + width / 2) / quantum) > std::floor((last - width / 2) / quantum)) quantum = 1;
        const double low = std::ceil((first + width / 2) / quantum), high = std::floor((last - width / 2) / quantum);
        if (low <= high)
        {
            const double middle = juce::jlimit(low, high, std::round((first + last) / (2 * quantum))) * quantum;
            rampBegin = clock.timeAtBeat(middle - width / 2); rampEnd = clock.timeAtBeat(middle + width / 2);
        }
    }
    auto& out = previous.controls.automation[(size_t) AutomationTarget::bass];
    auto& in = incoming.controls.automation[(size_t) AutomationTarget::bass];
    if (!window && ((!previous.automaticBass && !out.points.empty()) || (!incoming.automaticBass && !in.points.empty()))) return {};
    previous.automaticBass = incoming.automaticBass = true;
    out.enabled = in.enabled = true;
    out.setPoint(previous.sourceElapsed(rampBegin - previous.start), 0);
    out.setPoint(previous.sourceElapsed(rampEnd - previous.start), -60);
    in.setPoint(incoming.sourceElapsed(rampBegin - incoming.start), -60);
    in.setPoint(incoming.sourceElapsed(rampEnd - incoming.start), 0);
    return std::array<double, 4> {previous.sourceElapsed(rampBegin - previous.start), previous.sourceElapsed(rampEnd - previous.start),
        incoming.sourceElapsed(rampBegin - incoming.start), incoming.sourceElapsed(rampEnd - incoming.start)};
}

std::optional<std::array<double, 2>> Project::transitionSwapTimes(const TransitionLink& link, int band) const
{
    if (band < 0 || band >= 4) return {};
    const auto* out = clipFor(link.outgoing); const auto* in = clipFor(link.incoming);
    if (!out || !in || !out->media || !in->media) return {};
    const auto clock = gridClock();
    std::array<double, 2> times;
    if (const auto nodes = ordinarySwap(*this,link,band))
    {
        for (size_t side = 0; side < 2; ++side)
        { const auto& span = (*nodes)[side]; times[side] = span.sharp ? span.end : clock.timeAtBeat((clock.beatsAt(span.begin)+clock.beatsAt(span.end))/2); }
        return times;
    }
    for (size_t side = 0; side < 2; ++side)
    {
        const auto* clip = side == 0 ? out : in;
        double begin = 0, end = 0;
        if (link.bands[(size_t) band] && link.bands[(size_t) band]->swapping())
        {
            const auto envelope = transitionPlaybackFor(*clip).pairs[side == 0 ? 1 : 0][(size_t) band];
            begin = envelope.begin; end = envelope.end;
        }
        else if (band == 1 && link.bassSwap)
        {
            begin = clip->start + clip->localTimeForSource((*link.bassSwap)[side * 2]);
            end = clip->start + clip->localTimeForSource((*link.bassSwap)[side * 2 + 1]);
        }
        else return {};
        const auto& curve = effectSettings(*clip).automation[(size_t) band];
        const bool sharp = band == 1 && link.bassSwap && std::any_of(curve.points.begin(),curve.points.end(),
            [&](const auto& point) { return std::abs(point.time - (*link.bassSwap)[side * 2]) < 1.e-7 && point.bend >= 1; });
        times[side] = sharp ? end : clock.timeAtBeat((clock.beatsAt(begin) + clock.beatsAt(end)) / 2);
        if (!std::isfinite(times[side])) return {};
    }
    return times;
}
juce::Result Project::setTransitionSwapTime(const juce::String& incoming, int band, bool incomingSide, double time)
{
    if (!std::isfinite(time) || band < 0 || band >= 4) return juce::Result::fail("Invalid swap position.");
    if (hasPendingPlacement()) return juce::Result::fail("Wait for track placement before moving a swap.");
    auto valid = validateTransitionLinks(); if (valid.failed()) return valid;
    const auto* current = transitionFor(incoming); if (!current) return juce::Result::fail("The swap is unavailable.");
    if (clipTransitionLocked(current->outgoing) || clipTransitionLocked(current->incoming))
        return juce::Result::fail("Unlock transitions on both participating tracks before moving their swap.");
    const auto times = transitionSwapTimes(*current, band); if (!times) return juce::Result::fail("This band has no paired swap.");
    const auto clock = gridClock();
    const double delta = clock.beatsAt(time) - clock.beatsAt((*times)[incomingSide ? 1 : 0]);
    if (std::abs(delta) < 1e-9) return juce::Result::ok();
    const auto range = transitionRange(*current);
    if (range.getLength() <= 1e-6) return juce::Result::fail("Set a non-zero transition frame first.");
    if (const auto nodes = ordinarySwap(*this,*current,band))
    {
        auto candidate = *this;
        for (size_t side = 0; side < 2; ++side)
        {
            const auto& span = (*nodes)[side]; auto* clip = candidate.clipFor(side == 0 ? current->outgoing : current->incoming);
            auto& points = candidate.effectSettings(*clip).automation[(size_t)band].points;
            const size_t first = span.sharp ? span.last : span.first;
            for (size_t i = first; i <= span.last; ++i)
            {
                const double session = clip->start+clip->localTimeForSource(points[i].time);
                const double next = clock.timeAtBeat(clock.beatsAt(session)+delta);
                if (next < range.getStart()-1.e-8 || next > range.getEnd()+1.e-8)
                    return juce::Result::fail("Keep both swap handles inside their transition frame.");
                points[i].time = clip->sourceElapsed(next-clip->start);
            }
            for (size_t i = 1; i < points.size(); ++i) if (points[i].time <= points[i-1].time+1.e-10)
                return juce::Result::fail("Another node blocks this swap move. Its shape was kept.");
        }
        valid = candidate.validateTransitionLinks(); if (valid.failed()) return valid;
        valid = validateTransitionLocks(candidate); if (valid.failed()) return valid;
        *this = std::move(candidate); return juce::Result::ok();
    }
    if (current->bands[(size_t) band] && current->bands[(size_t) band]->swapping())
    {
        auto setting = *current->bands[(size_t) band];
        setting.position += delta / (clock.beatsAt(range.getEnd()) - clock.beatsAt(range.getStart()));
        return setTransitionBand(incoming, band, setting);
    }
    // Legacy bass owns four source points. Move them atomically in musical
    // time; do not sequence ordinary point edits, which clamp against each other.
    auto candidate = *this; auto& link = candidate.transitionLinks[(size_t) (current - transitionLinks.data())];
    const auto previous = *link.bassSwap; std::array<double, 4> targets;
    for (size_t side = 0; side < 2; ++side)
    {
        auto* clip = candidate.clipFor(side == 0 ? link.outgoing : link.incoming);
        auto& curve = candidate.effectSettings(*clip).automation[1]; std::array<int, 2> indices; std::array<float, 2> values; std::array<double,2> bends; std::array<bool,2> exponential, gainSpace;
        const int opening = bassPoint(candidate,*clip,previous[side * 2]);
        const bool sharp = opening >= 0 && curve.points[(size_t) opening].bend >= 1;
        for (size_t edge = 0; edge < 2; ++edge)
        {
            const size_t i = side * 2 + edge; indices[edge] = bassPoint(candidate,*clip, previous[i]);
            if (indices[edge] < 0) return juce::Result::fail("The owned bass point is unavailable.");
            values[edge] = curve.points[(size_t) indices[edge]].value;
            bends[edge] = curve.points[(size_t) indices[edge]].bend; exponential[edge] = curve.points[(size_t) indices[edge]].exponential; gainSpace[edge] = curve.points[(size_t) indices[edge]].gainSpace;
            const double oldTime = clip->start + clip->localTimeForSource(previous[i]);
            const double next = sharp && edge == 0 ? oldTime : clock.timeAtBeat(clock.beatsAt(oldTime) + delta);
            if (next < range.getStart() - 1e-7 || next > range.getEnd() + 1e-7)
                return juce::Result::fail("Keep both swap lines inside their transition frame.");
            targets[i] = clip->sourceElapsed(next - clip->start);
        }
        if (targets[side * 2] >= targets[side * 2 + 1]) return juce::Result::fail("Keep the swap after its opening node.");
        if (indices[0] == indices[1]) return juce::Result::fail("The owned bass endpoints overlap.");
        const double first = std::min(previous[side * 2], targets[side * 2]);
        const double last = std::max(previous[side * 2 + 1], targets[side * 2 + 1]);
        for (size_t i = 0; i < curve.points.size(); ++i)
            if ((int) i != indices[0] && (int) i != indices[1] && curve.points[i].time >= first && curve.points[i].time <= last)
                return juce::Result::fail("A manual or neighbouring bass point blocks this move. Its data was kept.");
        for (int index : {std::max(indices[0], indices[1]), std::min(indices[0], indices[1])}) curve.points.erase(curve.points.begin() + index);
        for (size_t edge = 0; edge < 2; ++edge) curve.points.push_back({targets[side * 2 + edge],values[edge],bends[edge],exponential[edge],gainSpace[edge]});
        std::sort(curve.points.begin(),curve.points.end(),[](const auto& a,const auto& b) { return a.time < b.time; });
        clip->automaticBass = false;
    }
    link.bassSwap = targets;
    valid = candidate.validateTransitionLinks(); if (valid.failed()) return valid;
    valid = validateTransitionLocks(candidate); if (valid.failed()) return valid;
    *this = std::move(candidate); return juce::Result::ok();
}
juce::Result Project::pairBassSwap(const juce::String& targetId, double time, BassSwapPairing& result)
{
    result = {};
    if (!std::isfinite(time) || time < 0) return juce::Result::fail("Choose a valid timeline location.");
    if (hasPendingPlacement()) return juce::Result::fail("Wait for track placement before pairing bass.");
    auto valid = validateTransitionLinks(); if (valid.failed()) return valid;
    const auto* target = clipFor(targetId);
    if (!target || !target->media || !target->media->hasPlayback() || target->media->sourceDetailsPending)
        return juce::Result::fail("Right-click an available audio track to receive the bass swap.");
    struct Choice { size_t link; juce::String source; double a, b, begin, end, distance, bend; float first, last; bool exponential, gainSpace; };
    std::optional<Choice> nearest; bool ambiguous = false, linked = false;
    for (size_t n = 0; n < transitionLinks.size(); ++n)
    {
        const auto& link = transitionLinks[n];
        if (link.incoming != targetId && link.outgoing != targetId) continue;
        linked = true; const auto* source = clipFor(link.incoming == targetId ? link.outgoing : link.incoming);
        if (!source || !source->media || !source->media->hasPlayback() || source->media->sourceDetailsPending)
            return juce::Result::fail("The transition partner's source is not ready. Its bass cut is unknown.");
        const auto range = transitionRange(link);
        const auto& curve = effectSettings(*source).automation[(size_t) AutomationTarget::bass];
        if (!curve.active()) continue;
        for (size_t i = 1; i < curve.points.size(); ++i)
        {
            const auto& a = curve.points[i - 1]; const auto& b = curve.points[i];
            if (!((a.value == 0 && b.value == -60) || (a.value == -60 && b.value == 0))) continue;
            const double begin = source->start + source->localTimeForSource(a.time), end = source->start + source->localTimeForSource(b.time);
            if (!std::isfinite(begin) || !std::isfinite(end) || end <= begin
                || begin < range.getStart() - 1e-7 || end > range.getEnd() + 1e-7
                || begin < std::max(target->start, source->start) || end > std::min(target->end(), source->end())) continue;
            // Distance is to the handoff endpoint in mix time.
            const double distance = std::abs(end - time);
            const bool falling = a.value == 0;
            // Prefer authored cuts over intervening recovery ramps. An
            // incoming-only opening is also usable from the outgoing side.
            if (!nearest || (falling && nearest->first != 0) || (falling == (nearest->first == 0) && distance < nearest->distance - 1e-7))
            { nearest = Choice {n, source->id, a.time, b.time, begin, end, distance,a.bend,a.value,b.value,a.exponential,a.gainSpace}; ambiguous = false; }
            else if (falling == (nearest->first == 0) && std::abs(distance - nearest->distance) <= 1e-7) ambiguous = true;
        }
    }
    if (!linked) return juce::Result::fail("This track has no linked transition partner. Link the overlapping tracks first.");
    if (!nearest) return juce::Result::fail("No enabled full bass cut (0 / -60 dB) was found on a linked partner inside the shared transition.");
    if (ambiguous) return juce::Result::fail("Two bass cuts are equally close. Right-click nearer the intended cut.");
    const auto& choice = *nearest; const auto& current = transitionLinks[choice.link];
    const float firstValue = choice.first == 0 ? -60.f : 0.f, lastValue = choice.last == 0 ? -60.f : 0.f;
    if (clipTransitionLocked(targetId) || clipTransitionLocked(choice.source))
        return juce::Result::fail("Unlock transitions on both participating tracks before pairing bass.");
    if (current.bands[1] && current.bands[1]->generated())
        return juce::Result::fail("This transition uses generated Bass treatment. Set Bass to Manual before pairing its manual cut.");
    double a = target->sourceElapsed(choice.begin - target->start), b = target->sourceElapsed(choice.end - target->start);
    if (!std::isfinite(a) || !std::isfinite(b) || a < 0 || b <= a)
        return juce::Result::fail("The bass cut has no valid shared source interval.");
    const auto& targetCurve = effectSettings(*target).automation[(size_t) AutomationTarget::bass];
    if (!targetCurve.points.empty() && !targetCurve.enabled)
        return juce::Result::fail("The receiving track has a disabled bass curve. Enable its Read or clear it before pairing.");
    std::array<std::optional<double>, 2> reused;
    for (const auto& point : targetCurve.points) if (point.time >= a - 1e-7 && point.time <= b + 1e-7)
    {
        const int edge = std::abs(point.time - a) < 1e-7 && point.value == firstValue ? 0
            : std::abs(point.time - b) < 1e-7 && point.value == lastValue ? 1 : -1;
        if (edge < 0 || reused[(size_t) edge]) return juce::Result::fail("Existing bass points block the receiving swap. They were kept.");
        reused[(size_t) edge] = point.time;
    }
    // Compatible existing points keep their exact identity; never insert an
    // almost-equal alias that the paired-point owner would interpret twice.
    a = reused[0].value_or(a); b = reused[1].value_or(b);
    if (b <= a) return juce::Result::fail("The receiving bass endpoints overlap.");
    std::array<double, 4> positions = current.outgoing == targetId ? std::array<double, 4>{a, b, choice.a, choice.b}
        : std::array<double, 4>{choice.a, choice.b, a, b};
    if (current.bassSwap)
    {
        bool same = true; for (size_t i = 0; i < positions.size(); ++i) same &= std::abs((*current.bassSwap)[i] - positions[i]) < 1e-7;
        if (!same) return juce::Result::fail("This transition already owns a different bass pair. Its pairing was kept.");
        result = {current.incoming, choice.source, false}; return juce::Result::ok();
    }
    // A source endpoint or matching receiver point must never acquire two owners.
    for (size_t n = 0; n < transitionLinks.size(); ++n) if (n != choice.link && transitionLinks[n].bassSwap)
    {
        const auto& other = transitionLinks[n];
        for (size_t side = 0; side < 2; ++side)
        {
            const auto& id = side == 0 ? other.outgoing : other.incoming;
            if (id != targetId && id != choice.source) continue;
            const double first = id == targetId ? a : choice.a, last = id == targetId ? b : choice.b;
            for (size_t edge = 0; edge < 2; ++edge)
            {
                const double point = (*other.bassSwap)[side * 2 + edge];
                if (std::abs(point - first) < 1e-7 || std::abs(point - last) < 1e-7)
                    return juce::Result::fail("A neighbouring bass pair already owns one of these points. Its pairing was kept.");
            }
        }
    }
    Project candidate = *this; auto& link = candidate.transitionLinks[choice.link];
    auto* receiving = candidate.clipFor(targetId); auto& curve = candidate.effectSettings(*receiving).automation[(size_t) AutomationTarget::bass];
    curve.enabled = true; curve.setPoint(a, firstValue); curve.setPoint(b, lastValue);
    const int firstNode = bassPoint(candidate,*receiving,a); if (firstNode >= 0) { curve.points[(size_t) firstNode].bend = choice.bend; curve.points[(size_t) firstNode].exponential = choice.exponential; curve.points[(size_t) firstNode].gainSpace = choice.gainSpace; }
    receiving->automaticBass = false; candidate.clipFor(choice.source)->automaticBass = false;
    link.bands[1].reset(); link.bassSwap = positions;
    valid = candidate.validateTransitionLinks(); if (valid.failed()) return valid;
    valid = validateTransitionLocks(candidate); if (valid.failed()) return valid;
    result = {link.incoming, choice.source, true}; *this = std::move(candidate); return juce::Result::ok();
}
juce::Result Project::moveAutomationPoint(const AutomationAddress& address, int point, double time, float value)
{
    if (automationLocked(address)) return juce::Result::fail("Unlock the transition before editing its automation.");
    const auto range = automationRange(address.target);
    if (!std::isfinite(time) || time < 0 || !std::isfinite(value) || value < range.getStart() || value > range.getEnd())
        return juce::Result::fail("Invalid automation position or value.");
    Project candidate = *this; auto* selected = candidate.curve(address);
    if (!selected || point < 0 || point >= (int) selected->points.size()) return juce::Result::fail("The automation point is unavailable.");
    const double old = selected->points[(size_t) point].time;
    if (address.target == AutomationTarget::bass && address.clipId.isNotEmpty())
        for (auto& link : candidate.transitionLinks) if (link.bassSwap && (link.outgoing == address.clipId || link.incoming == address.clipId))
        {
            auto& positions = *link.bassSwap; const int side = link.outgoing == address.clipId ? 0 : 2;
            for (int endpoint = 0; endpoint < 2; ++endpoint) if (std::abs(positions[(size_t) side + endpoint] - old) < 1e-7)
            {
                auto* out = candidate.clipFor(link.outgoing); auto* in = candidate.clipFor(link.incoming);
                if (!out || !in) return juce::Result::fail("The bass-swap partner is unavailable.");
                std::array<Clip*, 2> partners {out, in}; std::array<int, 2> indices {};
                double first = std::max(out->start, in->start), last = std::min(out->end(), in->end());
                for (size_t s = 0; s < 2; ++s)
                {
                    indices[s] = bassPoint(candidate,*partners[s], positions[s * 2 + endpoint]);
                    if (indices[s] < 0) return juce::Result::fail("The paired bass point was removed. Reapply the preset.");
                    const auto& points = candidate.effectSettings(*partners[s]).automation[(size_t) AutomationTarget::bass].points;
                    if (indices[s] > 0) first = std::max(first, partners[s]->start + partners[s]->localTimeForSource(points[(size_t) indices[s] - 1].time + 1e-6));
                    if (indices[s] + 1 < (int) points.size()) last = std::min(last, partners[s]->start + partners[s]->localTimeForSource(points[(size_t) indices[s] + 1].time - 1e-6));
                }
                if (first > last) return juce::Result::fail("No shared bass-swap position fits between these points.");
                const double target = juce::jlimit(first, last, time);
                for (size_t s = 0; s < 2; ++s)
                {
                    auto& curve = candidate.effectSettings(*partners[s]).automation[(size_t) AutomationTarget::bass];
                    const double source = partners[s]->sourceElapsed(target - partners[s]->start);
                    curve.movePoint(indices[s], source, partners[s]->id == address.clipId ? value : curve.points[(size_t) indices[s]].value);
                    positions[s * 2 + endpoint] = curve.points[(size_t) indices[s]].time;
                    partners[s]->automaticBass = false;
                }
                const auto protection = validateTransitionLocks(candidate); if (protection.failed()) return protection;
    *this = std::move(candidate); return juce::Result::ok();
            }
        }
    candidate.automationEdited(address);
    selected->movePoint(point, candidate.automationTime(address, time), value);
    const auto protection = validateTransitionLocks(candidate); if (protection.failed()) return protection;
    *this = std::move(candidate); return juce::Result::ok();
}
juce::Range<double> Project::transitionRange(const TransitionLink& link) const
{
    const auto* out = clipFor(link.outgoing); const auto* in = clipFor(link.incoming);
    if (!out || !in) return {};
    if (link.frame) return {out->start + out->localTimeForSource((*link.frame)[0]), out->start + out->localTimeForSource((*link.frame)[1])};
    const double begin = out->start + out->transitionTime(false);
    const auto envelope = out->envelope();
    return {begin, std::max(begin, std::min(in->end(), out->start + envelope.outro + envelope.fadeOut))};
}
juce::Result Project::editTransition(const juce::String& incoming, TransitionEdit edit, double delta, bool snapping)
{
    if (!std::isfinite(delta) || hasPendingPlacement()) return juce::Result::fail("Wait for track placement to finish.");
    Project candidate = *this; candidate.ensureTransitionLinks();
    const auto valid = candidate.validateTransitionLinks(); if (valid.failed()) return valid;
    auto link = std::find_if(candidate.transitionLinks.begin(), candidate.transitionLinks.end(), [&](const auto& x) { return x.incoming == incoming; });
    if (link == candidate.transitionLinks.end()) return juce::Result::fail("The transition is unavailable.");
    if (link->locked) return juce::Result::fail("Unlock this transition before editing it.");
    const auto edge = (size_t) std::distance(candidate.transitionLinks.begin(), link);
    auto* out = candidate.clipFor(link->outgoing); auto* in = candidate.clipFor(incoming);
    const auto oldRange = candidate.transitionRange(*link);
    double begin = oldRange.getStart(), end = oldRange.getEnd();
    const double reference = edit == TransitionEdit::resizeEnd ? end
        : edit == TransitionEdit::moveOutgoing ? out->start : edit == TransitionEdit::moveIncoming ? in->start : begin;
    const double target = candidate.snappedMoveTime(reference, reference + delta, snapping);
    delta = target - reference;
    if (std::abs(delta) < 1e-9) return juce::Result::ok();
    // Remove only the explicitly owned endpoints before changing geometry.
    // Their values and relative positions travel with the transition window;
    // unrelated source automation keeps its original coordinates.
    const auto bass = link->bassSwap;
    std::array<double, 2> bassTimes {}; std::array<std::array<float, 2>, 2> values {};
    if (bass)
    {
        for (size_t s = 0; s < 2; ++s)
        {
            auto* clip = s == 0 ? out : in; auto& curve = candidate.effectSettings(*clip).automation[1];
            for (size_t e = 0; e < 2; ++e)
            {
                const int index = bassPoint(candidate,*clip, (*bass)[s * 2 + e]);
                values[s][e] = curve.points[(size_t) index].value;
                if (s == 0) bassTimes[e] = out->start + out->localTimeForSource((*bass)[e]);
            }
            for (int e = 1; e >= 0; --e) curve.points.erase(curve.points.begin() + bassPoint(candidate,*clip, (*bass)[s * 2 + (size_t) e]));
        }
        link->bassSwap.reset();
    }
    if (edit == TransitionEdit::moveWindow || edit == TransitionEdit::resizeStart)
    {
        begin += delta; if (edit == TransitionEdit::moveWindow) end += delta;
        // The frame moves through the shared audio; it does not reflow songs.
    }
    else if (edit == TransitionEdit::resizeEnd) end += delta;
    else
    {
        const auto id = edit == TransitionEdit::moveOutgoing ? out->id : in->id;
        if (target < 0) return juce::Result::fail("The song cannot start before the mix.");
        candidate.clipFor(id)->moveTo(target);
        out = candidate.clipFor(candidate.transitionLinks[edge].outgoing); in = candidate.clipFor(incoming);
        (edit == TransitionEdit::moveOutgoing ? out->outroAnchor : in->introAnchor)
            = candidate.clipFor(id)->sourceElapsed(begin - candidate.clipFor(id)->start);
    }
    out = candidate.clipFor(candidate.transitionLinks[edge].outgoing); in = candidate.clipFor(incoming);
    if (begin < std::max(out->start, in->start) - 1e-7 || end > std::min(out->end(), in->end()) + 1e-7
        || end < begin || (bass && end - begin < 1e-6))
        return juce::Result::fail("Keep the transition inside both songs, with its end after its start.");
    const double scale = oldRange.getLength() > 1e-7 ? (end - begin) / oldRange.getLength() : 1;
    out->outroAnchor = out->sourceElapsed(begin - out->start);
    in->introAnchor = in->sourceElapsed(begin - in->start);
    candidate.transitionLinks[edge].frame = std::array<double,2>{std::max(0.,out->sourceElapsed(begin-out->start)), std::max(0.,out->sourceElapsed(end-out->start))};
    if (in->fadeIn > 0) { in->fadeIn = std::min(end - begin, in->fadeIn * scale); in->fadeInAnchor = in->sourceElapsed(begin-in->start); }
    if (out->fadeOut > 0) { out->fadeOut = std::min(end - begin, out->fadeOut * scale); out->fadeOutAnchor = out->sourceElapsed(end-out->fadeOut-out->start); }
    if (bass)
    {
        std::array<double, 4> positions;
        for (size_t s = 0; s < 2; ++s)
        {
            auto* clip = s == 0 ? out : in; auto& curve = candidate.effectSettings(*clip).automation[1];
            for (size_t e = 0; e < 2; ++e)
                positions[s * 2 + e] = clip->sourceElapsed(begin + (bassTimes[e] - oldRange.getStart()) * scale - clip->start);
            for (const auto& p : curve.points)
                if (p.time >= positions[s * 2] - 1e-7 && p.time <= positions[s * 2 + 1] + 1e-7)
                    return juce::Result::fail("The transition would cross another bass point.");
            for (size_t e = 0; e < 2; ++e) curve.setPoint(positions[s * 2 + e], values[s][e]);
        }
        candidate.transitionLinks[edge].bassSwap = positions;
    }
    const auto reconciled = candidate.reconcileBassSwaps(); if (reconciled.failed()) return reconciled;
    const auto protection = validateTransitionLocks(candidate); if (protection.failed()) return protection;
    *this = std::move(candidate); return juce::Result::ok();
}
juce::Result Project::reconcileBassSwaps()
{
    const auto valid = validateTransitionLinks(); if (valid.failed()) return valid;
    Project candidate = *this;
    for (auto& link : candidate.transitionLinks) if (link.bassSwap)
    {
        auto* out = candidate.clipFor(link.outgoing); auto* in = candidate.clipFor(link.incoming);
        if (!out || !in) return juce::Result::fail("The bass-swap partner is unavailable.");
        auto& positions = *link.bassSwap; auto& curve = effectSettings(*in).automation[(size_t) AutomationTarget::bass];
        std::array<int, 2> indices {}; std::array<double, 2> targets {};
        for (size_t endpoint = 0; endpoint < 2; ++endpoint)
        {
            indices[endpoint] = bassPoint(*this,*in, positions[2 + endpoint]);
            if (indices[endpoint] < 0 || bassPoint(*this,*out, positions[endpoint]) < 0) return juce::Result::fail("The paired bass point was removed. Reapply the preset.");
            const double at = out->start + out->localTimeForSource(positions[endpoint]);
            if (at < std::max(out->start, in->start) - 1e-7 || at > std::min(out->end(), in->end()) + 1e-7)
                return juce::Result::fail("The bass swap would move outside the shared audio. Move its paired points first.");
            targets[endpoint] = in->sourceElapsed(at - in->start);
        }
        for (size_t i = 0; i < curve.points.size(); ++i) if ((int) i != indices[0] && (int) i != indices[1])
            if (curve.points[i].time >= targets[0] - 1e-7 && curve.points[i].time <= targets[1] + 1e-7)
                return juce::Result::fail("The bass swap would cross another EQ point. Move its paired points first.");
        const float a = curve.points[(size_t) indices[0]].value, b = curve.points[(size_t) indices[1]].value;
        curve.points.erase(curve.points.begin() + std::max(indices[0], indices[1]));
        curve.points.erase(curve.points.begin() + std::min(indices[0], indices[1]));
        curve.setPoint(targets[0], a); curve.setPoint(targets[1], b);
        positions[2] = targets[0]; positions[3] = targets[1];
    }
    const auto protection = validateTransitionLocks(candidate); if (protection.failed()) return protection;
    *this = std::move(candidate); return juce::Result::ok();
}
juce::Result Project::reconcileTransitions()
{
    Project candidate = *this; candidate.ensureTransitionLinks();
    const auto valid = candidate.validateTransitionLinks(); if (valid.failed()) return valid;
    const auto links = candidate.transitionLinks;
    for (const auto& link : links)
        if (std::none_of(links.begin(), links.end(), [&](const auto& other) { return other.incoming == link.outgoing; }))
        {
            const auto result = candidate.moveClip(link.outgoing, candidate.clipFor(link.outgoing)->start, false, 0, false);
            if (result.failed()) return result;
        }
    const auto protection = validateTransitionLocks(candidate); if (protection.failed()) return protection;
    *this = std::move(candidate); return juce::Result::ok();
}
juce::Result Project::applyPlacements(const Project& candidate)
{
    if (clips.size() != candidate.clips.size()) return juce::Result::fail("The arrangement changed during this gesture. Drag again.");
    for (const auto& clip : clips)
    {
        const auto* source = candidate.clipFor(clip.id);
        if (!source || source->media != clip.media || source->offset != clip.offset || source->loopSpan != clip.loopSpan
            || source->loopPhase != clip.loopPhase || source->reversed != clip.reversed || source->tempoMap != clip.tempoMap)
            return juce::Result::fail("The source changed during this gesture. Drag again.");
    }
    const auto result = candidate.validateTransitionLinks(); if (result.failed()) return result;
    Project applied = *this;
    for (auto& clip : applied.clips)
    {
        const auto& source = *candidate.clipFor(clip.id); clip.applyPlacement(source);
    }
    applied.transitionLinks = candidate.transitionLinks; applied.transitionLinksKnown = candidate.transitionLinksKnown;
    for (auto& link : applied.transitionLinks)
    {
        const auto current = std::find_if(transitionLinks.begin(), transitionLinks.end(), [&](const auto& item)
            { return item.outgoing == link.outgoing && item.incoming == link.incoming; });
        // Geometry gestures cannot restore automation from their mouse-down
        // snapshot. Current ownership and values remain authoritative.
        link.bassSwap = current == transitionLinks.end() ? std::nullopt : current->bassSwap;
        if (current != transitionLinks.end()) { link.bands = current->bands; link.filter = current->filter; link.alternatives = current->alternatives; }
        if (current != transitionLinks.end()) link.muteOutside = current->muteOutside;
    }
    const auto bass = applied.reconcileBassSwaps(); if (bass.failed()) return bass;
    const auto protection = validateTransitionLocks(applied); if (protection.failed()) return protection;
    *this = std::move(applied); return juce::Result::ok();
}
juce::Result Project::validateTransitionLinks() const
{
    std::map<juce::String, juce::String> next; std::set<juce::String> incoming;
    for (const auto& link : transitionLinks)
    {
        if (link.outgoing == link.incoming || !clipFor(link.outgoing) || !clipFor(link.incoming)
            || !next.emplace(link.outgoing, link.incoming).second || !incoming.insert(link.incoming).second)
            return juce::Result::fail("Invalid transition ownership.");
        if (link.frame)
        {
            const auto& frame = *link.frame;
            if (!std::isfinite(frame[0]) || !std::isfinite(frame[1]) || frame[0] < -1.e-7 || frame[1] < frame[0])
                return juce::Result::fail("Invalid transition frame.");
            const auto* out = clipFor(link.outgoing); const auto* in = clipFor(link.incoming);
            if (out->media && in->media)
            {
                const auto range = transitionRange(link);
                if (range.getStart() < std::max(out->start,in->start)-1.e-7 || range.getEnd() > std::min(out->end(),in->end())+1.e-7)
                    return juce::Result::fail("Keep the transition inside both tracks' audio.");
            }
        }
        const auto bands = validateTransitionBands(link); if (bands.failed()) return bands;
        const auto filter = validateTransitionFilter(link); if (filter.failed()) return filter;
        for (const auto& style : link.alternatives) if (style)
        { const auto valid = style->validate(); if (valid.failed()) return valid; }
        if (link.bassSwap)
        {
            const auto& p = *link.bassSwap;
            if (!std::all_of(p.begin(), p.end(), [](double x) { return std::isfinite(x) && x >= 0; }) || p[0] >= p[1] || p[2] >= p[3]
                || bassPoint(*this,*clipFor(link.outgoing), p[0]) < 0 || bassPoint(*this,*clipFor(link.outgoing), p[1]) < 0
                || bassPoint(*this,*clipFor(link.incoming), p[2]) < 0 || bassPoint(*this,*clipFor(link.incoming), p[3]) < 0)
                return juce::Result::fail("Invalid paired bass-swap positions.");
        }
    }
    for (const auto& link : transitionLinks)
    {
        std::set<juce::String> seen; auto id = link.outgoing;
        while (next.count(id))
        {
            if (!seen.insert(id).second) return juce::Result::fail("A transition chain cannot contain a cycle.");
            id = next.at(id);
        }
    }
    return juce::Result::ok();
}
void Project::ensureTransitionLinks()
{
    transitionLinksKnown = true;
}
juce::Result Project::linkClips(const juce::String& outgoing, const juce::String& incoming)
{
    ensureTransitionLinks();
    if (std::any_of(transitionLinks.begin(), transitionLinks.end(), [&](const auto& link) { return link.outgoing == outgoing && link.incoming == incoming; })) return juce::Result::ok();
    transitionLinks.push_back({outgoing, incoming});
    if (const auto* out = clipFor(outgoing); out && out->media && clipFor(incoming) && clipFor(incoming)->media)
    {
        const auto range = transitionRange(transitionLinks.back());
        transitionLinks.back().frame = std::array<double,2>{std::max(0.,out->sourceElapsed(range.getStart()-out->start)),std::max(0.,out->sourceElapsed(range.getEnd()-out->start))};
    }
    const auto result = validateTransitionLinks();
    if (result.failed()) transitionLinks.pop_back();
    return result;
}
void Project::detachTransitionLinks(const juce::String& id)
{
    ensureTransitionLinks();
    transitionLinks.erase(std::remove_if(transitionLinks.begin(), transitionLinks.end(), [&](const auto& link) { return link.outgoing == id || link.incoming == id; }), transitionLinks.end());
}
juce::Result Project::unlinkClips(const std::vector<juce::String>& ids)
{
    if (ids.empty() || hasPendingPlacement()) return juce::Result::fail("Select ready segments before unlinking.");
    Project candidate = *this; candidate.ensureTransitionLinks();
    const std::set<juce::String> selected(ids.begin(), ids.end());
    for (const auto& id : ids) if (!candidate.clipFor(id)) return juce::Result::fail("A selected segment is no longer available.");
    std::vector<TransitionLink> removed;
    for (const auto& link : candidate.transitionLinks)
        if (selected.count(link.incoming) || selected.count(link.outgoing)) removed.push_back(link);
    if (removed.empty()) return juce::Result::ok();
    for (const auto& link : removed) for (const auto& id : {link.outgoing, link.incoming})
    {
        const auto* clip = candidate.clipFor(id);
        if (candidate.clipTransitionLocked(id)) return juce::Result::fail("Unlock the connected transitions before unlinking.");
        if (!clip->media || !clip->media->hasPlayback() || clip->media->sourceDetailsPending)
            return juce::Result::fail("Wait for the connected sources to finish preparing.");
    }
    // Preserve procedural treatments as ordinary source automation before
    // removing their relationship. Work privately so any refusal is atomic.
    for (const auto& link : removed)
    {
        const auto converted = materialisePresetNodes(candidate, link.incoming);
        if (converted.failed()) return converted;
    }
    for (const auto& link : removed)
    {
        const auto range = transitionRange(link);
        auto* out = candidate.clipFor(link.outgoing); auto* in = candidate.clipFor(link.incoming);
        // Zero-length fades also encode hard audible boundaries. Keep these
        // source-owned so unlinking cannot expose hidden heads/tails.
        if (link.muteOutside)
        {
            in->fadeInAnchor = in->sourceElapsed(std::max(in->envelope().intro, range.getStart() - in->start));
            out->fadeOutAnchor = out->sourceElapsed(std::min(out->envelope().outro, range.getEnd() - out->start));
        }
        in->introAnchor.reset(); out->outroAnchor.reset();
    }
    candidate.transitionLinks.erase(std::remove_if(candidate.transitionLinks.begin(), candidate.transitionLinks.end(), [&](const auto& link)
    { return selected.count(link.incoming) || selected.count(link.outgoing); }), candidate.transitionLinks.end());
    const auto valid = candidate.validateTransitionLinks(); if (valid.failed()) return valid;
    const auto locks = validateTransitionLocks(candidate); if (locks.failed()) return locks;
    *this = std::move(candidate); return juce::Result::ok();
}
juce::Result Project::moveClip(const juce::String& id, double seconds, bool snapping, double magnetSeconds, bool adoptMarkedNeighbours)
{
    if (!std::isfinite(seconds) || seconds < 0 || !clipFor(id)) return juce::Result::fail("The track position is invalid.");
    Project candidate = *this; candidate.ensureTransitionLinks();
    const auto valid = candidate.validateTransitionLinks(); if (valid.failed()) return valid;
    juce::ignoreUnused(adoptMarkedNeighbours); // Relationships are always explicit.
    auto* selected = candidate.clipFor(id);
    // Existing movement retains manual phase; source/neighbor phase acquisition
    // belongs to insertion, not to a drag of an already aligned song.
    juce::ignoreUnused(magnetSeconds);
    selected->moveTo(candidate.snappedMoveTime(selected->start, seconds, snapping));
    auto current = id;
    for (;;)
    {
        const auto link = std::find_if(candidate.transitionLinks.begin(), candidate.transitionLinks.end(), [&](const auto& item) { return item.incoming == current; });
        if (link == candidate.transitionLinks.end()) break;
        const auto* next = candidate.clipFor(current); auto* previous = candidate.clipFor(link->outgoing);
        const auto result = previous->alignAnchorTo(false, next->start + next->transitionTime(true));
        if (result.failed()) return result;
        current = previous->id;
    }
    current = id;
    for (;;)
    {
        const auto link = std::find_if(candidate.transitionLinks.begin(), candidate.transitionLinks.end(), [&](const auto& item) { return item.outgoing == current; });
        if (link == candidate.transitionLinks.end()) break;
        const auto* previous = candidate.clipFor(current); auto* next = candidate.clipFor(link->incoming);
        const auto result = next->alignIntroTo(previous->start + previous->transitionTime(false));
        if (result.failed()) return result;
        current = next->id;
    }
    const auto bass = candidate.reconcileBassSwaps(); if (bass.failed()) return bass;
    const auto protection = validateTransitionLocks(candidate); if (protection.failed()) return protection;
    *this = std::move(candidate); return juce::Result::ok();
}
juce::Result Project::moveClipGroup(const std::vector<juce::String>& ids, const juce::String& anchor, double seconds, int track, bool snapping)
{
    const std::set<juce::String> selected(ids.begin(), ids.end());
    const auto* reference = clipFor(anchor);
    if (selected.size() < 2 || selected.size() != ids.size() || !selected.count(anchor) || !reference
        || !std::isfinite(seconds) || seconds < 0 || track < 0 || track >= trackCount)
        return juce::Result::fail("Choose a valid group destination inside the timeline.");
    const double delta = snappedMoveTime(reference->start, seconds, snapping) - reference->start;
    const int lanes = track - reference->track;
    Project candidate = *this; candidate.ensureTransitionLinks();
    for (const auto& link : candidate.transitionLinks)
        if (selected.count(link.outgoing) != selected.count(link.incoming))
            return juce::Result::fail("Select both ends of every linked transition before moving the group.");
    for (const auto& id : ids)
    {
        auto* clip = candidate.clipFor(id);
        if (!clip || !clip->media || !clip->media->hasPlayback() || clip->media->sourceDetailsPending
            || clip->pendingPlacement != PendingPlacement::none)
            return juce::Result::fail("A selected source is unavailable or still preparing.");
        if (candidate.clipTransitionLocked(id)) return juce::Result::fail("Unlock the selected transitions before moving the group.");
        const auto original = *clip;
        clip->start += delta; clip->track += lanes;
        if (!std::isfinite(clip->end()) || clip->start < 0 || clip->end() > 1.e9 || clip->track < 0 || clip->track >= trackCount)
            return juce::Result::fail("The complete group must fit inside the timeline and four mixer lanes.");
        if (clip->tempoMap && delta != 0)
        {
            // Preserve the complete local source clock and prepared audio. A
            // destination with a different tempo shape needs explicit retiming,
            // not a hidden stretch of individual members during a rigid move.
            const double low = std::min(0., -clip->audioSlip), high = clip->length + std::max(0., -clip->audioSlip);
            const auto before = clip->tempoMap->localPoints(original.start + low, original.start + high);
            const auto after = clip->tempoMap->localPoints(clip->start + low, clip->start + high);
            const auto saved = tempo.localPoints(clip->start + low, clip->start + high);
            auto agrees = [&](double local)
            {
                const double a = original.start + local, b = clip->start + local;
                return std::abs(clip->tempoMap->bpmAt(a) - clip->tempoMap->bpmAt(b)) < 1.e-9
                    && (!followTrackTempo || std::abs(clip->tempoMap->bpmAt(a) - tempo.bpmAt(b)) < 1.e-9);
            };
            std::set<double> boundaries {low, high};
            for (const auto& point : before) boundaries.insert(low + point.time);
            for (const auto& point : after) boundaries.insert(low + point.time);
            if (followTrackTempo) for (const auto& point : saved) boundaries.insert(low + point.time);
            bool same = true; double previous = low;
            for (double local : boundaries)
            { same = same && agrees(local) && agrees((previous + local) / 2); previous = local; }
            if (!same) return juce::Result::fail("Group movement keeps source timing. Choose a destination with the same tempo curve.");
        }
        // Elastic streams use absolute programme time. Rebind the immutable
        // source through the same placement rule as Clip::retime.
        if (delta != 0 && clip->elasticPlayback) clip->elasticSource = std::make_shared<const ElasticSource>(*clip);
    }
    // No-op admission may inspect inferred legacy links, but must not publish
    // that metadata or require an existing overlap to be resolved first.
    if (delta == 0 && lanes == 0) return candidate.validateTransitionLinks();
    for (const auto& c : candidate.clips) if (selected.count(c.id))
        for (const auto& other : candidate.clips) if (!selected.count(other.id) && other.track == c.track
            && c.start < other.end() - 1.e-9 && c.end() > other.start + 1.e-9)
            return juce::Result::fail("The group overlaps an unselected segment. Choose empty destination lanes/times.");
    const auto valid = candidate.validateTransitionLinks(); if (valid.failed()) return valid;
    const auto locks = validateTransitionLocks(candidate); if (locks.failed()) return locks;
    *this = std::move(candidate); return juce::Result::ok();
}
juce::Result Project::moveClipFollowing(const juce::String& id, double seconds, bool snapping)
{
    if (!std::isfinite(seconds) || seconds < 0) return juce::Result::fail("The track cannot move before the mix begins.");
    if (hasPendingPlacement()) return juce::Result::fail("Wait for track preparation before moving the following tracks.");
    Project candidate = *this; candidate.ensureTransitionLinks();
    const auto valid = candidate.validateTransitionLinks(); if (valid.failed()) return valid;
    const auto selected = std::find_if(clips.begin(), clips.end(), [&](const auto& clip) { return clip.id == id; });
    if (selected == clips.end()) return juce::Result::fail("The selected track is unavailable.");
    const auto first = (size_t) std::distance(clips.begin(), selected);
    std::set<juce::String> following;
    for (size_t i = first; i < clips.size(); ++i)
    {
        const auto& clip = clips[i];
        if (!clip.media || !clip.media->hasPlayback() || clip.media->sourceDetailsPending)
            return juce::Result::fail("Wait for all following tracks to load before moving them.");
        following.insert(clip.id);
    }
    // Document/playlist order owns this edit. Sorting a table does not rewrite it.
    for (const auto& link : candidate.transitionLinks)
    {
        if (following.count(link.outgoing) && !following.count(link.incoming))
            return juce::Result::fail("This transition points back to an earlier playlist track. Move that pair separately.");
        if (!following.count(link.outgoing) && following.count(link.incoming) && link.incoming != id)
            return juce::Result::fail("A later track is paired with an earlier playlist track. Move that pair separately.");
    }
    const double target = candidate.snappedMoveTime(selected->start, seconds, snapping);
    if (std::abs(target - selected->start) < 1e-9) return juce::Result::ok();
    const auto clock = gridClock();
    const double beats = clock.beatsAt(target) - clock.beatsAt(selected->start);
    const auto* incoming = candidate.transitionFor(id);
    const auto pinned = incoming ? std::optional<juce::Range<double>>(candidate.transitionRange(*incoming)) : std::nullopt;
    // Move the entire requested suffix explicitly. Frame drags now leave songs
    // fixed, so they cannot also be the owner of following-track movement.
    for (size_t i = first; i < clips.size(); ++i)
    {
        const auto& clip = clips[i];
        const double beat = clock.beatsAt(clip.start) + beats;
        if (beat < clock.beatsAt(0) - 1e-8) return juce::Result::fail("A following track would move before the pre-roll begins.");
        candidate.clipFor(clip.id)->moveTo(clock.timeAtBeat(std::max(clock.beatsAt(0),beat)));
    }
    if (pinned)
    {
        auto* clip = candidate.clipFor(id);
        clip->introAnchor = clip->sourceElapsed(pinned->getStart()-clip->start);
    }
    if (std::abs(candidate.clipFor(id)->start - target) > 1e-7)
        return juce::Result::fail("That move conflicts with the fixed incoming transition.");
    if (pinned)
    {
        const auto after = candidate.transitionRange(*candidate.transitionFor(id));
        const auto* clip = candidate.clipFor(id);
        if (std::abs(after.getStart() - pinned->getStart()) > 1e-7 || std::abs(after.getEnd() - pinned->getEnd()) > 1e-7
            || std::abs(clip->start + clip->transitionTime(true) - pinned->getStart()) > 1e-7
            || clip->start > pinned->getStart() + 1e-7 || clip->end() < pinned->getEnd() - 1e-7)
            return juce::Result::fail("Keep the fixed incoming transition inside the selected track. Earlier tracks were kept.");
    }
    const auto links = candidate.validateTransitionLinks(); if (links.failed()) return links;
    const auto protection = validateTransitionLocks(candidate); if (protection.failed()) return protection;
    *this = std::move(candidate); return juce::Result::ok();
}

juce::Result Project::moveTransitionAnchor(const juce::String& id, bool intro, double time, bool snapping)
{
    auto found = std::find_if(clips.begin(), clips.end(), [&](const auto& clip) { return clip.id == id; });
    if (found == clips.end() || !std::isfinite(time)) return juce::Result::fail("The alignment track is unavailable.");
    if (hasPendingPlacement()) return juce::Result::fail("Wait for track placement to finish.");
    const size_t selected = (size_t) std::distance(clips.begin(), found);
    Project candidate = *this;
    candidate.ensureTransitionLinks();
    const auto valid = candidate.validateTransitionLinks(); if (valid.failed()) return valid;
    auto& clip = candidate.clips[selected];
    double local = juce::jlimit(0.0, clip.length, time - clip.start);
    if (snapping)
    {
        const auto clock = candidate.gridClock(); const double step = candidate.snapStep();
        const double phase = clock.beatsAt(clip.start + clip.transitionTime(intro));
        const double first = std::ceil((clock.beatsAt(clip.start) - phase) / step - 1e-9);
        const double last = std::floor((clock.beatsAt(clip.end()) - phase) / step + 1e-9);
        if (first > last) return juce::Result::fail("No beat fits inside this track.");
        const double count = juce::jlimit(first, last, std::round((clock.beatsAt(time) - phase) / step));
        local = juce::jlimit(0., clip.length, clock.timeAtBeat(phase + count * step) - clip.start);
    }
    if (std::abs(local - clip.transitionTime(intro)) >= 1.0e-9)
        (intro ? clip.introAnchor : clip.outroAnchor) = clip.sourceElapsed(local);
    // Once paired, identity owns adjacency even if a long pickup moves a clip's
    // file edge before its predecessor. Playlist order and screen position do
    // not reinterpret the relationship on each drag.
    auto previous = std::find_if(candidate.transitionLinks.begin(), candidate.transitionLinks.end(), [&](const auto& link) { return link.incoming == id; });
    auto next = std::find_if(candidate.transitionLinks.begin(), candidate.transitionLinks.end(), [&](const auto& link) { return link.outgoing == id; });
    if (!intro && next != candidate.transitionLinks.end() && next->frame)
        (*next->frame)[0] = clip.sourceElapsed(local);
    if (intro)
    {
        previous = std::find_if(candidate.transitionLinks.begin(), candidate.transitionLinks.end(), [&](const auto& link) { return link.incoming == id; });
        if (previous != candidate.transitionLinks.end())
        {
            const auto* out = candidate.clipFor(previous->outgoing);
            const auto result = clip.alignIntroTo(out->start + out->transitionTime(false)); if (result.failed()) return result;
        }
    }
    auto current = id;
    for (;;)
    {
        next = std::find_if(candidate.transitionLinks.begin(), candidate.transitionLinks.end(), [&](const auto& link) { return link.outgoing == current; });
        if (next == candidate.transitionLinks.end()) break;
        const auto* out = candidate.clipFor(current); auto* in = candidate.clipFor(next->incoming);
        const auto result = in->alignIntroTo(out->start + out->transitionTime(false)); if (result.failed()) return result;
        current = in->id;
    }
    const auto bass = candidate.reconcileBassSwaps(); if (bass.failed()) return bass;
    const auto protection = validateTransitionLocks(candidate); if (protection.failed()) return protection;
    *this = std::move(candidate);
    return juce::Result::ok();
}
}
