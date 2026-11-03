#include "Workstation.h"
#include "ElasticAudio.h"
#include <cmath>
#include <map>

namespace mixworks
{
double Project::playbackStart() const
{
    if (tempo.origin == 0) return 0;
    double first = tempo.origin;
    for (const auto& clip : clips) first = std::min(first, clip.start);
    return first;
}
juce::Result Project::setTimelineOrigin(double seconds)
{
    if (!std::isfinite(seconds) || seconds < 0 || seconds > 600) return juce::Result::fail("Pre-roll must fit within ten minutes.");
    if (seconds == tempo.origin) return juce::Result::ok();
    if (hasPendingPlacement()) return juce::Result::fail("Wait for track placement before changing pre-roll.");
    if (hasLockedTransitions()) return juce::Result::fail("Unlock transitions before changing pre-roll.");
    const double delta = seconds - tempo.origin;
    auto candidate = *this;
    bool valid = true;
    auto shift = [&](double& time)
    {
        const double next = time + delta;
        if (!std::isfinite(next) || next < -1.e-8 || next > 1.e9) { valid = false; return; }
        time = std::max(0., next);
    };
    auto shiftMap = [&](TempoMap& map)
    {
        const double initialBpm = map.bpmAt(0);
        // The first zero-time point of a rebased map is a held-tempo guard.
        if (tempo.origin > 0 && !map.points.empty() && map.points.front().time == 0 && map.points.front().clipId.isEmpty())
            map.points.erase(map.points.begin());
        for (auto& point : map.points) shift(point.time);
        if (!map.points.empty() && map.points.front().time > 0) map.points.insert(map.points.begin(), {0, initialBpm, false, {}});
        shift(map.origin);
    };
    shiftMap(candidate.tempo);
    if (candidate.followTrackTempo && candidate.tempo.points.empty() && seconds > 0)
    {
        candidate.tempo.setPoint(0, candidate.tempo.fallback, false);
        candidate.tempo.setPoint(seconds, candidate.tempo.fallback, false);
    }
    std::map<const TempoMap*, std::shared_ptr<const TempoMap>> clocks;
    auto shiftClip = [&](Clip& clip)
    {
        shift(clip.start);
        if (clip.end() + effectTailSeconds > 1.e9) valid = false;
        if (clip.tempoMap)
        {
            const auto* key = clip.tempoMap.get();
            auto found = clocks.find(key);
            if (found == clocks.end())
            {
                auto map = std::make_shared<TempoMap>(*clip.tempoMap); shiftMap(*map);
                found = clocks.emplace(key, std::move(map)).first;
            }
            clip.tempoMap = found->second;
        }
    };
    std::map<const ElasticSource*, std::shared_ptr<const ElasticSource>> elasticClocks;
    for (auto& clip : candidate.clips)
    {
        shiftClip(clip);
        if (clip.elasticSource)
        {
            const auto* key = clip.elasticSource.get();
            auto found = elasticClocks.find(key);
            if (found == elasticClocks.end())
            {
                // A split shares its original immutable playback mapping. Rebase
                // that original geometry, not the trimmed visible segment.
                auto geometry = clip.elasticSource->clip; shiftClip(geometry);
                found = elasticClocks.emplace(key, std::make_shared<const ElasticSource>(geometry)).first;
            }
            clip.elasticSource = found->second;
        }
    }
    auto shiftCurve = [&](AutomationCurve& curve)
    {
        for (auto& point : curve.points) shift(point.time);
    };
    for (auto& [key, state] : candidate.laneHandles)
    {
        for (auto& seed : state.seeds) for (auto& time : seed.second) shift(time);
        std::set<double> generated;
        for (double time : state.generatedPoints) { shift(time); generated.insert(time); }
        state.generatedPoints = std::move(generated);
    }
    shiftCurve(candidate.masterAutomation);
    for (auto& track : candidate.tracks)
    {
        for (auto& curve : track.automation) shiftCurve(curve);
        for (auto& lane : track.pluginAutomation) shiftCurve(lane.curve);
    }
    for (auto& label : candidate.labels) shift(label.time);
    for (auto& scene : candidate.visualScenes) shift(scene.start);
    if (validateVisualScenes(candidate.visualScenes).failed()) valid = false;
    if (!valid) return juce::Result::fail("Keep enough pre-roll for the intro and its markers. Move them to or after musical zero before removing it.");
    const auto links = candidate.validateTransitionLinks(); if (links.failed()) return links;
    *this = std::move(candidate); return juce::Result::ok();
}
void Workstation::changePreroll(int bars)
{
    if (busy || pendingTempo > 0 || tempoPreparationPending || engine.exporting || engine.playing || engine.rehearsalActive())
    { setStatus("Stop playback and wait for preparation before changing pre-roll."); return; }
    if (bars != 0 && bars != 1 && bars != 2 && bars != 4) return;
    engine.flushMidiControls(); engine.finishAutomation(); const juce::ScopedLock guard(engine.lock);
    const double previous = engine.project.tempo.origin;
    const double target = bars == 0 ? 0 : previous + bars * 240. / engine.project.gridClock().bpmAt(0);
    if (target == previous) return;
    auto candidate = engine.project; const auto changed = candidate.setTimelineOrigin(target);
    if (changed.failed()) { setStatus(changed.getErrorMessage()); return; }
    const double position = std::max(0., engine.position.load() + target - previous);
    remember(); engine.project = std::move(candidate); engine.seek(position);
    if (target > 0) timeline.pixelsPerSecond = std::max(timeline.pixelsPerSecond, 180. / target);
    refresh(); viewport.setViewPosition(0, viewport.getViewPositionY());
    setStatus(bars == 0 ? "Pre-roll removed. Musical zero and retained audio agree."
        : "Added " + juce::String(bars) + " bars before zero. Drag the song left to align its downbeat; playback/export retain the intro.");
}
}
