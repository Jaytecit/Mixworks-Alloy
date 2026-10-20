#include "Model.h"
#include <cmath>
#include <map>
#include <set>

namespace mixworks
{
namespace
{
int bassPoint(const Clip& clip, double source)
{
    const auto& points = clip.controls.automation[(size_t) AutomationTarget::bass].points;
    for (size_t i = 0; i < points.size(); ++i) if (std::abs(points[i].time - source) < 1e-7) return (int) i;
    return -1;
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
    const auto clock = followTrackTempo ? tempo : TempoMap {bpm, {}};
    std::array<double, 2> times;
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
        times[side] = clock.timeAtBeat((clock.beatsAt(begin) + clock.beatsAt(end)) / 2);
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
    const auto clock = followTrackTempo ? tempo : TempoMap {bpm, {}};
    const double delta = clock.beatsAt(time) - clock.beatsAt((*times)[incomingSide ? 1 : 0]);
    if (std::abs(delta) < 1e-9) return juce::Result::ok();
    const auto range = transitionRange(*current);
    if (range.getLength() <= 1e-6) return juce::Result::fail("Set a non-zero transition frame first.");
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
        auto& curve = clip->controls.automation[1]; std::array<int, 2> indices; std::array<float, 2> values;
        for (size_t edge = 0; edge < 2; ++edge)
        {
            const size_t i = side * 2 + edge; indices[edge] = bassPoint(*clip, previous[i]);
            if (indices[edge] < 0) return juce::Result::fail("The owned bass point is unavailable.");
            values[edge] = curve.points[(size_t) indices[edge]].value;
            const double oldTime = clip->start + clip->localTimeForSource(previous[i]);
            const double next = clock.timeAtBeat(clock.beatsAt(oldTime) + delta);
            if (next < range.getStart() - 1e-7 || next > range.getEnd() + 1e-7)
                return juce::Result::fail("Keep both swap lines inside their transition frame.");
            targets[i] = clip->sourceElapsed(next - clip->start);
        }
        if (indices[0] == indices[1]) return juce::Result::fail("The owned bass endpoints overlap.");
        const double first = std::min(previous[side * 2], targets[side * 2]);
        const double last = std::max(previous[side * 2 + 1], targets[side * 2 + 1]);
        for (size_t i = 0; i < curve.points.size(); ++i)
            if ((int) i != indices[0] && (int) i != indices[1] && curve.points[i].time >= first - 1e-7 && curve.points[i].time <= last + 1e-7)
                return juce::Result::fail("A manual or neighbouring bass point blocks this move. Its data was kept.");
        for (int index : {std::max(indices[0], indices[1]), std::min(indices[0], indices[1])}) curve.points.erase(curve.points.begin() + index);
        for (size_t edge = 0; edge < 2; ++edge) curve.setPoint(targets[side * 2 + edge], values[edge]);
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
    struct Choice { size_t link; juce::String source; double a, b, begin, end, distance; };
    std::optional<Choice> nearest; bool ambiguous = false, linked = false;
    for (size_t n = 0; n < transitionLinks.size(); ++n)
    {
        const auto& link = transitionLinks[n];
        if (link.incoming != targetId && link.outgoing != targetId) continue;
        linked = true; const auto* source = clipFor(link.incoming == targetId ? link.outgoing : link.incoming);
        if (!source || !source->media || !source->media->hasPlayback() || source->media->sourceDetailsPending)
            return juce::Result::fail("The transition partner's source is not ready. Its bass cut is unknown.");
        const auto range = transitionRange(link);
        const auto& curve = source->controls.automation[(size_t) AutomationTarget::bass];
        if (!curve.active()) continue;
        for (size_t i = 1; i < curve.points.size(); ++i)
        {
            const auto& a = curve.points[i - 1]; const auto& b = curve.points[i];
            if (a.value != 0 || b.value != -60) continue;
            const double begin = source->start + source->localTimeForSource(a.time), end = source->start + source->localTimeForSource(b.time);
            if (!std::isfinite(begin) || !std::isfinite(end) || end <= begin
                || begin < range.getStart() - 1e-7 || end > range.getEnd() + 1e-7
                || begin < std::max(target->start, source->start) || end > std::min(target->end(), source->end())) continue;
            // Distance is to the full-cut (-60 dB) node, in mix time.
            const double distance = std::abs(end - time);
            if (!nearest || distance < nearest->distance - 1e-7)
            { nearest = Choice {n, source->id, a.time, b.time, begin, end, distance}; ambiguous = false; }
            else if (std::abs(distance - nearest->distance) <= 1e-7) ambiguous = true;
        }
    }
    if (!linked) return juce::Result::fail("This track has no linked transition partner. Link the overlapping tracks first.");
    if (!nearest) return juce::Result::fail("No enabled full bass cut (0 to -60 dB) was found on a linked partner inside the shared transition.");
    if (ambiguous) return juce::Result::fail("Two bass cuts are equally close. Right-click nearer the intended cut.");
    const auto& choice = *nearest; const auto& current = transitionLinks[choice.link];
    if (clipTransitionLocked(targetId) || clipTransitionLocked(choice.source))
        return juce::Result::fail("Unlock transitions on both participating tracks before pairing bass.");
    if (current.bands[1] && current.bands[1]->generated())
        return juce::Result::fail("This transition uses generated Bass treatment. Set Bass to Manual before pairing its manual cut.");
    double a = target->sourceElapsed(choice.begin - target->start), b = target->sourceElapsed(choice.end - target->start);
    if (!std::isfinite(a) || !std::isfinite(b) || a < 0 || b <= a)
        return juce::Result::fail("The bass cut has no valid shared source interval.");
    const auto& targetCurve = target->controls.automation[(size_t) AutomationTarget::bass];
    if (!targetCurve.points.empty() && !targetCurve.enabled)
        return juce::Result::fail("The receiving track has a disabled bass curve. Enable its Read or clear it before pairing.");
    std::array<std::optional<double>, 2> reused;
    for (const auto& point : targetCurve.points) if (point.time >= a - 1e-7 && point.time <= b + 1e-7)
    {
        const int edge = std::abs(point.time - a) < 1e-7 && point.value == -60 ? 0
            : std::abs(point.time - b) < 1e-7 && point.value == 0 ? 1 : -1;
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
    auto* receiving = candidate.clipFor(targetId); auto& curve = receiving->controls.automation[(size_t) AutomationTarget::bass];
    curve.enabled = true; curve.setPoint(a, -60); curve.setPoint(b, 0);
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
                    indices[s] = bassPoint(*partners[s], positions[s * 2 + endpoint]);
                    if (indices[s] < 0) return juce::Result::fail("The paired bass point was removed. Reapply the preset.");
                    const auto& points = partners[s]->controls.automation[(size_t) AutomationTarget::bass].points;
                    if (indices[s] > 0) first = std::max(first, partners[s]->start + partners[s]->localTimeForSource(points[(size_t) indices[s] - 1].time + 1e-6));
                    if (indices[s] + 1 < (int) points.size()) last = std::min(last, partners[s]->start + partners[s]->localTimeForSource(points[(size_t) indices[s] + 1].time - 1e-6));
                }
                if (first > last) return juce::Result::fail("No shared bass-swap position fits between these points.");
                const double target = juce::jlimit(first, last, time);
                for (size_t s = 0; s < 2; ++s)
                {
                    auto& curve = partners[s]->controls.automation[(size_t) AutomationTarget::bass];
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
    const double target = candidate.snappedTime(reference + delta, snapping);
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
            auto* clip = s == 0 ? out : in; auto& curve = clip->controls.automation[1];
            for (size_t e = 0; e < 2; ++e)
            {
                const int index = bassPoint(*clip, (*bass)[s * 2 + e]);
                values[s][e] = curve.points[(size_t) index].value;
                if (s == 0) bassTimes[e] = out->start + out->localTimeForSource((*bass)[e]);
            }
            for (int e = 1; e >= 0; --e) curve.points.erase(curve.points.begin() + bassPoint(*clip, (*bass)[s * 2 + (size_t) e]));
        }
        link->bassSwap.reset();
    }
    if (edit == TransitionEdit::moveWindow || edit == TransitionEdit::resizeStart)
    {
        begin += delta; if (edit == TransitionEdit::moveWindow) end += delta;
        const auto result = candidate.moveTransitionAnchor(out->id, false, begin, false); if (result.failed()) return result;
    }
    else if (edit == TransitionEdit::resizeEnd) end += delta;
    else
    {
        const auto id = edit == TransitionEdit::moveOutgoing ? out->id : in->id;
        if (target < 0) return juce::Result::fail("The song cannot start before the mix.");
        if (edit == TransitionEdit::moveOutgoing)
        {
            const auto result = candidate.moveClip(id, target, false, 0, false); if (result.failed()) return result;
        }
        else candidate.clipFor(id)->moveTo(target);
        out = candidate.clipFor(candidate.transitionLinks[edge].outgoing); in = candidate.clipFor(incoming);
        (edit == TransitionEdit::moveOutgoing ? out->outroAnchor : in->introAnchor)
            = candidate.clipFor(id)->sourceElapsed(begin - candidate.clipFor(id)->start);
        const auto result = candidate.reconcileTransitions(); if (result.failed()) return result;
    }
    out = candidate.clipFor(candidate.transitionLinks[edge].outgoing); in = candidate.clipFor(incoming);
    if (begin < std::max(out->start, in->start) - 1e-7 || end > std::min(out->end(), in->end()) + 1e-7
        || end < begin || (bass && end - begin < 1e-6))
        return juce::Result::fail("Keep the transition inside both songs, with its end after its start.");
    const double scale = oldRange.getLength() > 1e-7 ? (end - begin) / oldRange.getLength() : 1;
    in->fadeIn = std::min(end - begin, in->fadeIn * scale);
    out->fadeOut = std::min(end - begin, out->fadeOut * scale);
    in->fadeInAnchor = in->sourceElapsed(begin - in->start);
    out->fadeOutAnchor = out->sourceElapsed(end - out->fadeOut - out->start);
    if (bass)
    {
        std::array<double, 4> positions;
        for (size_t s = 0; s < 2; ++s)
        {
            auto* clip = s == 0 ? out : in; auto& curve = clip->controls.automation[1];
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
        auto& positions = *link.bassSwap; auto& curve = in->controls.automation[(size_t) AutomationTarget::bass];
        std::array<int, 2> indices {}; std::array<double, 2> targets {};
        for (size_t endpoint = 0; endpoint < 2; ++endpoint)
        {
            indices[endpoint] = bassPoint(*in, positions[2 + endpoint]);
            if (indices[endpoint] < 0 || bassPoint(*out, positions[endpoint]) < 0) return juce::Result::fail("The paired bass point was removed. Reapply the preset.");
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
        const auto bands = validateTransitionBands(link); if (bands.failed()) return bands;
        const auto filter = validateTransitionFilter(link); if (filter.failed()) return filter;
        for (const auto& style : link.alternatives) if (style)
        { const auto valid = style->validate(); if (valid.failed()) return valid; }
        if (link.bassSwap)
        {
            const auto& p = *link.bassSwap;
            if (!std::all_of(p.begin(), p.end(), [](double x) { return std::isfinite(x) && x >= 0; }) || p[0] >= p[1] || p[2] >= p[3]
                || bassPoint(*clipFor(link.outgoing), p[0]) < 0 || bassPoint(*clipFor(link.outgoing), p[1]) < 0
                || bassPoint(*clipFor(link.incoming), p[2]) < 0 || bassPoint(*clipFor(link.incoming), p[3]) < 0)
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
    if (transitionLinksKnown) return;
    transitionLinksKnown = true;
    std::vector<const Clip*> order; for (const auto& clip : clips) order.push_back(&clip);
    std::stable_sort(order.begin(), order.end(), [](const auto* a, const auto* b) { return a->start < b->start; });
    // Adopt only relationships already present in an older arrangement. Opening
    // a session must never move tracks to manufacture a transition.
    for (size_t i = 1; i < order.size(); ++i)
        if (std::abs(order[i - 1]->start + order[i - 1]->transitionTime(false) - order[i]->start - order[i]->transitionTime(true)) < 1.0e-7)
            transitionLinks.push_back({order[i - 1]->id, order[i]->id});
}
juce::Result Project::linkClips(const juce::String& outgoing, const juce::String& incoming)
{
    ensureTransitionLinks();
    if (std::any_of(transitionLinks.begin(), transitionLinks.end(), [&](const auto& link) { return link.outgoing == outgoing && link.incoming == incoming; })) return juce::Result::ok();
    transitionLinks.push_back({outgoing, incoming});
    const auto result = validateTransitionLinks();
    if (result.failed()) transitionLinks.pop_back();
    return result;
}
void Project::detachTransitionLinks(const juce::String& id)
{
    ensureTransitionLinks();
    transitionLinks.erase(std::remove_if(transitionLinks.begin(), transitionLinks.end(), [&](const auto& link) { return link.outgoing == id || link.incoming == id; }), transitionLinks.end());
}
juce::Result Project::moveClip(const juce::String& id, double seconds, bool snapping, double magnetSeconds, bool adoptMarkedNeighbours)
{
    if (!std::isfinite(seconds) || seconds < 0 || !clipFor(id)) return juce::Result::fail("The track position is invalid.");
    Project candidate = *this; candidate.ensureTransitionLinks();
    const auto valid = candidate.validateTransitionLinks(); if (valid.failed()) return valid;
    if (adoptMarkedNeighbours)
    {
        // Old sessions have source-owned cues but no partner identities. A
        // movement explicitly joins marked neighbours; opening/preparation does
        // not move a legacy arrangement or manufacture a coincident pair.
        std::vector<const Clip*> order; for (const auto& clip : candidate.clips) order.push_back(&clip);
        std::stable_sort(order.begin(), order.end(), [](const auto* a, const auto* b) { return a->start < b->start; });
        std::set<juce::String> group {id}; bool changed = true;
        while (changed)
        {
            changed = false;
            for (const auto& link : candidate.transitionLinks) if (group.count(link.outgoing) || group.count(link.incoming))
            { changed |= group.insert(link.outgoing).second; changed |= group.insert(link.incoming).second; }
            for (size_t i = 1; i < order.size(); ++i)
            {
                const auto* out = order[i - 1]; const auto* in = order[i];
                if (!out->outroAnchor || !in->introAnchor || (!group.count(out->id) && !group.count(in->id))) continue;
                if (std::any_of(candidate.transitionLinks.begin(), candidate.transitionLinks.end(), [&](const auto& link) { return link.outgoing == out->id || link.incoming == in->id; })) continue;
                const auto result = candidate.linkClips(out->id, in->id); if (result.failed()) return result;
                group.insert(out->id); group.insert(in->id); changed = true;
            }
        }
    }
    auto* selected = candidate.clipFor(id);
    const bool paired = std::any_of(candidate.transitionLinks.begin(), candidate.transitionLinks.end(), [&](const auto& link) { return link.outgoing == id || link.incoming == id; });
    // A partner is moving too, so its old pulse positions are not stationary
    // magnetic targets. A linked group snaps against the shared mix clock.
    selected->moveTo(candidate.snappedClipStart(*selected, seconds, snapping, paired ? 0 : magnetSeconds));
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
    if (snapping && !intro)
        local = juce::jlimit(0.0, clip.length, candidate.snappedTime(time, true) - clip.start);
    else if (snapping && clip.media->bpm() > 0)
    {
        const double period = 60 / clip.media->bpm(), phase = clip.sourceElapsed(clip.transitionTime(intro));
        const double first = std::ceil(-phase / period - 1.0e-9);
        const double last = std::floor((clip.sourceElapsed(clip.length) - phase) / period + 1.0e-9);
        if (first > last) return juce::Result::fail("No beat fits inside this track.");
        local = juce::jlimit(0.0, clip.length, clip.localTimeForSource(phase
            + juce::jlimit(first, last, std::round((clip.sourceElapsed(local) - phase) / period)) * period));
    }
    if (std::abs(local - clip.transitionTime(intro)) >= 1.0e-9)
        (intro ? clip.introAnchor : clip.outroAnchor) = clip.sourceElapsed(local);
    // Once paired, identity owns adjacency even if a long pickup moves a clip's
    // file edge before its predecessor. Playlist order and screen position do
    // not reinterpret the relationship on each drag.
    auto previous = std::find_if(candidate.transitionLinks.begin(), candidate.transitionLinks.end(), [&](const auto& link) { return link.incoming == id; });
    auto next = std::find_if(candidate.transitionLinks.begin(), candidate.transitionLinks.end(), [&](const auto& link) { return link.outgoing == id; });
    if ((intro && previous == candidate.transitionLinks.end()) || (!intro && next == candidate.transitionLinks.end()))
    {
        std::vector<const Clip*> order; for (const auto& item : clips) order.push_back(&item);
        std::stable_sort(order.begin(), order.end(), [](const auto* a, const auto* b) { return a->start < b->start; });
        const auto at = std::find_if(order.begin(), order.end(), [&](const auto* item) { return item->id == id; });
        if (intro && at != order.begin()) { const auto result = candidate.linkClips((*(at - 1))->id, id); if (result.failed()) return result; }
        if (!intro && at + 1 != order.end()) { const auto result = candidate.linkClips(id, (*(at + 1))->id); if (result.failed()) return result; }
    }
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
