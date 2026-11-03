#include "ArrangementInsertion.h"
#include "Beatmatch.h"
#include "Hotcues.h"
#include <cmath>
#include <set>

namespace mixworks
{
namespace
{
bool sameClock(const Clip& before, const Clip& after)
{
    for (int n = 0; n <= 32; ++n)
    {
        const double local = before.length * n / 32;
        if (std::abs(before.sourceElapsed(local) - after.sourceElapsed(local)) > 1.e-7
            || std::abs(before.audioSourceElapsed(local) - after.audioSourceElapsed(local)) > 1.e-7) return false;
    }
    if (before.tempoMap && after.tempoMap)
    {
        for (const auto& p : before.tempoMap->localPoints(before.start, before.end()))
            if (std::abs(before.sourceElapsed(p.time) - after.sourceElapsed(p.time)) > 1.e-7) return false;
        for (const auto& p : after.tempoMap->localPoints(after.start, after.end()))
            if (std::abs(before.sourceElapsed(p.time) - after.sourceElapsed(p.time)) > 1.e-7) return false;
        for (const auto& p : before.tempoMap->localPoints(before.start - before.audioSlip, before.end() - before.audioSlip))
            if (std::abs(before.audioSourceElapsed(p.time) - after.audioSourceElapsed(p.time)) > 1.e-7) return false;
        for (const auto& p : after.tempoMap->localPoints(after.start - after.audioSlip, after.end() - after.audioSlip))
            if (std::abs(before.audioSourceElapsed(p.time) - after.audioSourceElapsed(p.time)) > 1.e-7) return false;
    }
    return true;
}
void expandCurve(AutomationCurve& curve, double from, double prefixEnd, double displacement)
{
    if (curve.points.empty()) return;
    const auto original = curve;
    auto prefix = original.sliced(0, prefixEnd, 0);
    if (prefix.points.empty()) prefix.points.push_back({0, original.interpolatedValue(0, 0), 1});
    const double tail = std::max(from + 1, original.points.back().time);
    auto suffix = original.sliced(from, tail, 0);
    prefix.points.back().bend = 1; // Hold through new material; jump at resumed suffix.
    for (auto p : suffix.points) { p.time += from + displacement; prefix.points.push_back(p); }
    curve = std::move(prefix);
}
}
ArrangementInsertion planArrangementInsertion(const Project& input, std::shared_ptr<Media> media, const juce::String& anchor,
    bool after, double overlapBeats, TransitionPreset preset)
{
    ArrangementInsertion result; result.project = input;
    auto fail = [&](const juce::String& text) { result.error = text; result.project = input; return result; };
    if (!media || !media->hasPlayback() || media->sourceDetailsPending || input.hasPendingPlacement())
        return fail("Wait for source audio and existing segment preparation before inserting a song.");
    if (!std::isfinite(overlapBeats) || overlapBeats < 0 || overlapBeats > 256
        || (preset != TransitionPreset::alignOnly && preset != TransitionPreset::bassSwap && preset != TransitionPreset::crossfade))
        return fail("Choose an available transition and overlap length.");
    const double bpm = media->bpm(), phase = media->downbeatOrigin();
    if (!std::isfinite(bpm) || bpm < 20 || bpm > 400 || !std::isfinite(phase) || phase < 0 || phase >= media->duration()
        || (media->analysis.variableTempo && media->manualBpm <= 0))
        return fail("Set a reliable constant BPM and downbeat for the new song before arrangement insertion.");
    auto& p = result.project; p.ensureTransitionLinks();
    const auto valid = p.validateTransitionLinks(); if (valid.failed()) return fail(valid.getErrorMessage());
    std::vector<const Clip*> order; for (const auto& c : p.clips) order.push_back(&c);
    std::stable_sort(order.begin(), order.end(), [](const auto* a, const auto* b) { return a->start < b->start; });
    auto selected = std::find_if(order.begin(), order.end(), [&](const auto* c) { return c->id == anchor; });
    if (selected == order.end()) return fail("Select the destination segment again.");
    const size_t boundary = (size_t) std::distance(order.begin(), selected) + (after ? 1 : 0);
    const Clip* out = boundary ? order[boundary - 1] : nullptr;
    const Clip* in = boundary < order.size() ? order[boundary] : nullptr;
    auto clock = input.gridClock(); if (clock.points.empty()) clock.setPoint(0, clock.fallback);
    const double from = in ? in->start : input.duration();
    if (in && std::any_of(order.begin() + (ptrdiff_t) boundary + 1, order.end(), [&](const auto* c) { return c->start == from; }))
        return fail("Several segments start at this boundary. Choose an unambiguous song boundary first.");
    result.outgoing = out ? out->id : juce::String(); result.incoming = in ? in->id : juce::String(); result.suffixStart = from;
    const auto oldLink = in ? p.transitionFor(in->id) : nullptr;
    if (oldLink && (!out || oldLink->outgoing != out->id)) return fail("This boundary crosses another transition chain. Select its actual neighbouring pair.");
    for (const auto& link : p.transitionLinks)
    {
        const bool left = p.clipFor(link.outgoing)->start < from, right = p.clipFor(link.incoming)->start < from;
        if (left != right && (!oldLink || link.incoming != oldLink->incoming)) return fail("Another linked pair crosses this insertion boundary.");
    }
    for (const auto& c : p.clips)
        if (!c.media || !c.media->hasPlayback() || c.media->sourceDetailsPending || c.elasticPlayback)
            return fail("Load and finish Beatmatch for the existing arrangement before inserting a song.");
    if (input.hasLockedTransitions()) return fail("Unlock the protected transitions before changing arrangement time. The completed mix was kept.");
    juce::Range<double> first, second;
    const double overlap = preset == TransitionPreset::alignOnly ? 0 : overlapBeats;
    if (oldLink && p.transitionRange(*oldLink).getLength() > 1.e-6)
    {
        first = p.transitionRange(*oldLink);
        second = first;
    }
    else
    {
        if (out)
        {
            const double end = oldLink ? std::min(out->end(), p.transitionRange(*oldLink).getEnd()) : out->end();
            first = {clock.timeAtBeat(std::max(clock.beatsAt(out->start), clock.beatsAt(end) - overlap)), end};
        }
        if (in)
        {
            const double cue = in->firstDownbeatTime();
            const double begin = in->start + std::max({0., cue, in->transitionTime(true), in->envelope().intro});
            second = {begin, std::min(in->end(), clock.timeAtBeat(clock.beatsAt(begin) + overlap))};
        }
    }
    const double desired = out ? first.getStart() : 0;
    const double sourcePhase = phase * bpm / 60;
    const double startBeat = out ? std::floor((clock.beatsAt(desired) + sourcePhase) / 4 + 1.e-9) * 4 - sourcePhase : clock.beatsAt(0);
    const double start = out ? clock.timeAtBeat(startBeat) : 0;
    if (out && (start > first.getStart() + 1.e-7 || (overlap > 0 && first.getLength() < 1.e-6)))
        return fail("There is no usable incoming overlap at this boundary. Set a transition frame first.");
    double prefixEnd = in ? second.getEnd() : from;
    double clockFrom = from;
    for (const auto* c : order)
    {
        if (c->start < from) prefixEnd = std::max(prefixEnd, c->end() + std::max(0., -c->audioSlip));
        else clockFrom = std::min(clockFrom, c->start - std::max(0., c->audioSlip));
    }
    prefixEnd = std::max(prefixEnd, from);
    Clip inserted; inserted.media = media; inserted.start = start; inserted.beatmatched = true; inserted.tempoSourceBpm = bpm;
    const double available = media->duration() * bpm / 60;
    if (in)
    {
        const double prefixBeats = clock.beatsFromStart(prefixEnd) - clock.beatsFromStart(start);
        const double fromBeat = clockFrom < 0 ? clockFrom * clock.bpmAt(0) / 60 : clock.beatsFromStart(clockFrom);
        const double repeatedBeats = clock.beatsFromStart(prefixEnd) - fromBeat;
        const double phaseOffset = out ? 0 : sourcePhase - clock.beatsFromStart(clock.origin);
        const double extra = phaseOffset + std::floor((available - prefixBeats - phaseOffset) / 4 + 1.e-9) * 4;
        const double heldBeats = extra - repeatedBeats;
        if (heldBeats < 1.e-5) return fail("The new song is too short for both joins and the existing overlap. Choose a shorter overlap or longer source.");
        const double holdBpm = clock.bpmAt(prefixEnd);
        const double resume = prefixEnd + heldBeats * 60 / holdBpm;
        const double delta = resume - clockFrom;
        if (!std::isfinite(delta) || delta <= 0 || input.duration() + delta > 1.e9) return fail("The inserted arrangement would exceed the supported timeline.");
        result.displacement = delta;
        auto map = clock; map.points.clear();
        std::set<juce::String> movedIds;
        for (const auto& c : p.clips) if (c.start >= from) movedIds.insert(c.id);
        for (auto point : clock.points) if (point.time < prefixEnd)
        { if (movedIds.count(point.clipId)) point.clipId.clear(); map.points.push_back(point); }
        const auto next = std::lower_bound(clock.points.begin(), clock.points.end(), prefixEnd, [](const auto& point, double t) { return point.time < t; });
        if (prefixEnd > 0) map.points.push_back({prefixEnd, next != clock.points.end() && next->time == prefixEnd ? next->bpm : holdBpm, next != clock.points.end() && next->ramp});
        map.points.push_back({prefixEnd, holdBpm, false});
        map.points.push_back({resume, clock.bpmAt(clockFrom), false});
        for (auto point : clock.points) if (point.time >= clockFrom)
        { point.time += delta; if (!movedIds.count(point.clipId)) point.clipId.clear(); map.points.push_back(point); }
        p.tempo = std::move(map); p.followTrackTempo = true;
        auto translated = std::make_shared<const TempoMap>(p.tempo);
        for (auto& c : p.clips)
        {
            const auto before = c;
            if (c.start >= from) { c.start += delta; if (c.tempoMap) c.tempoMap = translated; ++result.moved; }
            // Existing clocks must match the map that Save/Open reconstructs.
            auto admitted = c; if (c.beatmatched) admitted.tempoMap = translated;
            if (!sameClock(before, admitted)) return fail("An existing source clock cannot be preserved at this boundary. Finish Beatmatch before inserting.");
        }
        for (auto& label : p.labels) if (label.time >= from) label.time += delta;
        for (auto& [key, state] : p.laneHandles) { state.generatedPoints.clear(); for (auto& seed : state.seeds) seed.second.clear(); }
        expandCurve(p.masterAutomation, clockFrom, prefixEnd, delta);
        for (auto& settings : p.tracks)
        {
            for (auto& curve : settings.automation) expandCurve(curve, clockFrom, prefixEnd, delta);
            for (auto& lane : settings.pluginAutomation) expandCurve(lane.curve, clockFrom, prefixEnd, delta);
        }
        inserted.tempoMap = translated; inserted.length = prefixEnd + delta - start;
        result.sourceEnd = (prefixBeats + extra) * 60 / bpm;
        second = second + delta;
    }
    else
    {
        p.tempo = clock; p.followTrackTempo = true;
        inserted.tempoMap = std::make_shared<const TempoMap>(clock);
        const double usable = sourcePhase + std::floor((available - sourcePhase) / 4) * 4;
        if (usable <= 0) return fail("The new song has no complete usable bar after its downbeat.");
        result.sourceEnd = usable * 60 / bpm;
        inserted.length = clock.timeAtBeat(clock.beatsAt(start) + usable) - start;
        for (const auto& c : p.clips)
        {
            auto admitted = c; if (c.beatmatched) admitted.tempoMap = inserted.tempoMap;
            if (!sameClock(c, admitted)) return fail("An existing source clock needs Beatmatch preparation before insertion.");
        }
    }
    inserted.rate = result.sourceEnd / inserted.length;
    if (!std::isfinite(inserted.length) || inserted.length <= 0 || result.sourceEnd > media->duration() + 1.e-7)
        return fail("The proposed song range is unavailable.");
    if (!out)
    {
        p.tempo.origin = inserted.start + inserted.firstDownbeatTime();
        inserted.tempoMap = std::make_shared<const TempoMap>(p.tempo);
    }
    int lane = -1;
    for (int n = 0; n < trackCount; ++n)
        if (std::none_of(p.clips.begin(), p.clips.end(), [&](const auto& c) { return c.track == n && c.start < inserted.end() - 1.e-7 && c.end() > inserted.start + 1.e-7; })) { lane = n; break; }
    if (lane < 0) return fail("All mixer lanes are occupied across the new song. Free a lane at this boundary first.");
    inserted.track = lane; initialiseHotcues(inserted); result.inserted = inserted.id;
    if (oldLink)
    {
        const auto removedIncoming = oldLink->incoming;
        auto link = std::find_if(p.transitionLinks.begin(), p.transitionLinks.end(), [&](const auto& l) { return l.incoming == removedIncoming; });
        p.clearTransitionBass(*link); p.transitionLinks.erase(link);
    }
    p.clips.push_back(inserted);
    // Insertion makes an independent segment. Explicit Add Transition owns new joins.
    const auto accepted = p.validateTransitionLinks(); if (accepted.failed()) return fail(accepted.getErrorMessage());
    const auto protectedResult = input.validateTransitionLocks(p); if (protectedResult.failed()) return fail(protectedResult.getErrorMessage());
    return result;
}
juce::String prepareArrangementInsertion(ArrangementInsertion& insertion, int64_t existingResidentBytes)
{
    if (insertion.error.isNotEmpty()) return insertion.error;
    auto* clip = insertion.project.clipFor(insertion.inserted); if (!clip) return "The proposed new song is unavailable.";
    const double estimate = clip->length * clip->media->sampleRate * clip->media->channels() * sizeof(float);
    if (!clip->media->original && (existingResidentBytes < 0 || estimate > maxPreparedBytes - existingResidentBytes)) return "The new song exceeds the remaining preparation memory budget.";
    Project single = insertion.project; single.clips = {*clip}; single.transitionLinks.clear(); single.transitionLinksKnown = true;
    const auto error = prepareSavedClips(single); if (error.isNotEmpty()) return error;
    clip->prepared = single.clips[0].prepared; clip->preparedOffset = single.clips[0].preparedOffset;
    clip->elasticPlayback = single.clips[0].elasticPlayback; clip->elasticSource = single.clips[0].elasticSource;
    return {};
}
}
