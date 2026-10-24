#include "Engine.h"
#include <cmath>

namespace mixworks
{
juce::Result Engine::setLaunchSettings(const LaunchSettings& settings)
{
    const auto valid = settings.validate(); if (valid.failed()) return valid;
    const juce::ScopedLock guard(lock);
    if (exporting || !controlInputEnabled) return juce::Result::fail("Finish preparation or export before changing launch timing.");
    if (project.launch == settings) return juce::Result::ok();
    cancelScheduledPause(ScheduledPauseReason::clockChanged);
    cancelLiveHotcues(ScheduledPauseReason::clockChanged);
    project.launch = settings; return juce::Result::ok();
}
ScheduledPauseStatus Engine::scheduledPauseStatus() const
{
    const juce::ScopedLock guard(lock); return pauseStatus;
}
void Engine::cancelScheduledPause(ScheduledPauseReason reason)
{
    const juce::ScopedLock guard(lock);
    if (pauseStatus.state == ScheduledPauseState::pending)
    { pauseStatus.state = ScheduledPauseState::cancelled; pauseStatus.reason = reason; }
    // Keep captured storage until message-thread admission replaces it. A
    // callback cancellation must not release a vector or its last resources.
}
ScheduledPauseReason Engine::scheduledPauseInvalidReason() const
{
    if (!playing || position.load() != pauseExpected) return ScheduledPauseReason::transportChanged;
    if (sampleRate.load() != pauseRate) return ScheduledPauseReason::deviceChanged;
    if (!(project.launch == pauseSettings) || project.bpm != pauseBpm || project.followTrackTempo != pauseFollowTempo
        || project.tempo.origin != pauseGrid.origin || project.tempo.fallback != pauseGrid.fallback || project.tempo.points != pauseGrid.points)
        return ScheduledPauseReason::clockChanged;
    if (exporting || pauseExclusiveInterrupted.load() || !controlInputEnabled || rehearsalActive() || gridAudition.active()
        || latencyBlocked || !playbackReady() || project.audibleEnd() <= pauseStatus.boundary)
        return ScheduledPauseReason::unavailable;
    return ScheduledPauseReason::none;
}
juce::Result Engine::toggleScheduledPause()
{
    const juce::ScopedLock guard(lock); serviceScheduledPause();
    if (pauseStatus.state == ScheduledPauseState::pending)
    { cancelScheduledPause(); return juce::Result::ok(); }
    if (!playing || exporting || !controlInputEnabled || rehearsalActive() || gridAudition.active() || latencyBlocked || !playbackReady())
        return juce::Result::fail("Play the ready programme before scheduling a pause. Finish preparation, rehearsal or export first.");
    const double now = position.load(), rate = sampleRate.load();
    if (!std::isfinite(rate) || rate <= 0) return juce::Result::fail("The programme sample clock is unavailable.");
    const auto boundary = nextLaunchBoundary(project.gridClock(), project.launch, now);
    if (!boundary) return juce::Result::fail("Launch timing is unavailable on this grid. Choose Immediate explicitly or correct the project grid.");
    if (boundary->time >= project.audibleEnd()) return juce::Result::fail("That launch boundary is beyond the remaining programme. Choose an earlier timing.");
    pauseGrid = project.tempo;
    if (project.launch.timing == LaunchTiming::immediate)
    {
        setPlaying(false); pauseStatus = {ScheduledPauseState::paused, ScheduledPauseReason::none, now, boundary->beat, now};
        return juce::Result::ok();
    }
    pauseSettings = project.launch; pauseBpm = project.bpm; pauseFollowTempo = project.followTrackTempo;
    pauseRate = rate; pauseExpected = now; pauseExclusiveInterrupted = false;
    pauseStatus = {ScheduledPauseState::pending, ScheduledPauseReason::none, boundary->time, boundary->beat, 0};
    return juce::Result::ok();
}
void Engine::serviceScheduledPause()
{
    const juce::ScopedLock guard(lock);
    if (pauseFinishPending) { finishAutomation(); pauseFinishPending = false; }
    if (pauseStatus.state == ScheduledPauseState::pending)
    {
        const auto reason = scheduledPauseInvalidReason();
        if (reason != ScheduledPauseReason::none) cancelScheduledPause(reason);
    }
    // This service is message-thread-only (unlike serviceLiveHotcues, also used
    // by the callback). Retain small status records, retire cancelled snapshots
    // and private streams here so their last resources never die at a boundary.
    for (auto& voice : liveHotcueVoices) if (voice.status.state != LiveHotcueState::pending)
    {
        voice.queuedElastic.reset(); // Displaced stream after a commit, or a cancelled preparation.
        if (!voice.status.active && voice.captured.media)
        {
            voice.activeElastic.reset();
            voice.captured.media.reset(); voice.captured.prepared.reset(); voice.captured.tempoMap.reset();
            voice.captured.elasticSource.reset(); voice.captured.effects.reset();
            voice.captured.controls = {}; voice.captured.hotcues.reset(); voice.captured.sectionMarkers.reset(); voice.grid = {};
        }
    }
}
int Engine::framesBeforeScheduledPause(int frames, double at)
{
    if (pauseStatus.state != ScheduledPauseState::pending) return -1;
    const auto reason = scheduledPauseInvalidReason();
    if (reason != ScheduledPauseReason::none) { cancelScheduledPause(reason); return -1; }
    const double samples = (pauseStatus.boundary - at) * pauseRate;
    if (samples < -1.e-6) { cancelScheduledPause(ScheduledPauseReason::missedBoundary); return -1; }
    if (samples > frames + 1.e-6) return -1;
    // First sample at/after the boundary, allowing only arithmetic roundoff at
    // exact integer edges. This limits stop error to one device sample.
    return juce::jlimit(0, frames, (int) std::ceil(samples - 1.e-6));
}
void Engine::commitScheduledPause(double actualTime)
{
    if (pauseStatus.state != ScheduledPauseState::pending) return;
    playing = false; pauseStatus.state = ScheduledPauseState::paused; pauseStatus.actualTime = actualTime;
    cancelLiveHotcues(ScheduledPauseReason::transportChanged);
    pauseFinishPending = true;
}
juce::String Engine::scheduledPauseText() const
{
    const juce::ScopedLock guard(lock);
    switch (pauseStatus.state)
    {
        case ScheduledPauseState::idle: return {};
        case ScheduledPauseState::pending:
            return "Pause queued / " + juce::String(launchTimingName(pauseSettings.timing)) + " / bar "
                + juce::String(pauseStatus.beat / 4 + 1, 2) + " / " + juce::String(pauseStatus.boundary - pauseGrid.origin, 3) + " s";
        case ScheduledPauseState::paused: return "Last scheduled pause / " + juce::String(pauseStatus.actualTime - pauseGrid.origin, 3) + " s";
        case ScheduledPauseState::cancelled: break;
    }
    const char* reason = "Cancelled";
    switch (pauseStatus.reason)
    {
        case ScheduledPauseReason::requested: reason = "Cancelled by you"; break;
        case ScheduledPauseReason::transportChanged: reason = "Transport changed"; break;
        case ScheduledPauseReason::clockChanged: reason = "Grid or launch timing changed; trigger again"; break;
        case ScheduledPauseReason::unavailable: reason = "Programme unavailable or changed; trigger again when ready"; break;
        case ScheduledPauseReason::missedBoundary: reason = "Boundary missed; trigger again"; break;
        case ScheduledPauseReason::deviceChanged: reason = "Audio device clock changed; trigger again"; break;
        case ScheduledPauseReason::none: break;
    }
    return "Queued pause cancelled / " + juce::String(reason);
}
}
