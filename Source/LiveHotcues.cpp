#include "Engine.h"
#include "Hotcues.h"
#include <cmath>

namespace mixworks
{
Engine::LiveHotcueVoice* Engine::liveVoiceFor(const juce::String& id)
{
    for (auto& voice : liveHotcueVoices) if (voice.captured.id == id) return &voice;
    return nullptr;
}
const Engine::LiveHotcueVoice* Engine::liveVoiceFor(const juce::String& id) const
{
    for (const auto& voice : liveHotcueVoices) if (voice.captured.id == id) return &voice;
    return nullptr;
}
double Engine::liveClipTime(const juce::String& id, double time) const
{
    const juce::ScopedLock guard(lock);
    const auto* voice = liveVoiceFor(id); return time + (voice && voice->status.active ? voice->shift : 0.);
}
LiveHotcueStatus Engine::liveHotcueStatus(const juce::String& id) const
{
    const juce::ScopedLock guard(lock);
    if (const auto* voice = liveVoiceFor(id)) return voice->status;
    return {};
}
bool Engine::hasLiveHotcueVoices() const
{
    const juce::ScopedLock guard(lock);
    return std::any_of(liveHotcueVoices.begin(),liveHotcueVoices.end(),[](const auto& voice) { return voice.status.active; });
}
ScheduledPauseReason Engine::liveHotcueInvalidReason(const LiveHotcueVoice& voice, bool queue) const
{
    const auto& old = voice.captured; const auto* clip = project.clipFor(old.id);
    if (!clip || clip->media != old.media || !(clip->media->analysisSource == voice.source)
        || clip->effects != old.effects || clip->start != old.start || clip->offset != old.offset || clip->length != old.length
        || clip->track != old.track || clip->audioSlip != old.audioSlip || clip->rate != old.rate || clip->reversed != old.reversed
        || clip->loopSpan != old.loopSpan || clip->loopPhase != old.loopPhase || clip->prepared != old.prepared
        || clip->preparedOffset != old.preparedOffset || clip->tempoMap != old.tempoMap || clip->tempoSourceBpm != old.tempoSourceBpm
        || clip->elasticSource != old.elasticSource || clip->elasticPlayback != old.elasticPlayback
        || clip->pendingPlacement != PendingPlacement::none || project.clipTransitionLocked(old.id)) return ScheduledPauseReason::unavailable;
    if (sampleRate.load() != voice.rate) return ScheduledPauseReason::deviceChanged;
    if (position.load() != voice.expected) return ScheduledPauseReason::transportChanged;
    if (exporting || liveExclusiveInterrupted.load() || !controlInputEnabled || rehearsalActive() || gridAudition.active()
        || latencyBlocked || !playbackReady() || ((automationRecordMode != AutomationRecordMode::read || recordingAutomation()) && !capturingPerformance())) return ScheduledPauseReason::unavailable;
    if (queue && voice.status.state == LiveHotcueState::pending)
    {
        if (!playing) return ScheduledPauseReason::transportChanged;
        if (!(project.launch == voice.settings) || project.bpm != voice.bpm || project.followTrackTempo != voice.followTempo
            || project.tempo.origin != voice.grid.origin || project.tempo.fallback != voice.grid.fallback || project.tempo.points != voice.grid.points)
            return ScheduledPauseReason::clockChanged;
        const auto* cue = hotcueFor(*clip, voice.status.slot);
        if (!cue || !(*cue == voice.cue) || voice.status.boundary>project.audibleEnd()+1.e-9) return ScheduledPauseReason::unavailable;
    }
    return ScheduledPauseReason::none;
}
void Engine::serviceLiveHotcues()
{
    const juce::ScopedLock guard(lock);
    for (auto& voice : liveHotcueVoices) if (voice.status.active || voice.status.state == LiveHotcueState::pending)
    {
        const auto reason = liveHotcueInvalidReason(voice,false);
        if (reason != ScheduledPauseReason::none)
        { voice.status.state = LiveHotcueState::cancelled; voice.status.reason = reason; voice.status.active = false; }
        else if (voice.status.state == LiveHotcueState::pending)
        {
            const auto queueReason = liveHotcueInvalidReason(voice);
            if (queueReason != ScheduledPauseReason::none)
            { voice.status.state = LiveHotcueState::cancelled; voice.status.reason = queueReason; }
        }
    }
}
void Engine::cancelLiveHotcue(const juce::String& id)
{
    const juce::ScopedLock guard(lock);
    if (auto* voice = liveVoiceFor(id); voice && voice->status.state == LiveHotcueState::pending)
    { voice->status.state = LiveHotcueState::cancelled; voice->status.reason = ScheduledPauseReason::requested; }
}
void Engine::cancelLiveHotcues(ScheduledPauseReason reason, bool clearVoices)
{
    const juce::ScopedLock guard(lock);
    for (auto& voice : liveHotcueVoices)
    {
        if (voice.status.state == LiveHotcueState::pending || (clearVoices && voice.status.active))
        { voice.status.state = LiveHotcueState::cancelled; voice.status.reason = reason; }
        if (clearVoices) voice.status.active = false;
    }
}
juce::Result Engine::triggerLiveHotcue(const juce::String& id, int slot)
{
    const juce::ScopedLock guard(lock); serviceLiveHotcues();
    if (auto* existing = liveVoiceFor(id); existing && existing->status.state == LiveHotcueState::pending && existing->status.slot == slot)
    { cancelLiveHotcue(id); return juce::Result::ok(); }
    if (!playing || exporting || !controlInputEnabled || rehearsalActive() || gridAudition.active() || latencyBlocked || !playbackReady())
        return juce::Result::fail("Play the ready programme before launching a live hotcue. Finish preparation or audition first.");
    if ((automationRecordMode != AutomationRecordMode::read || recordingAutomation()) && !capturingPerformance())
        return juce::Result::fail("Live hotcue event recording is not available yet. Finish the take and choose Read before launching.");
    if (isMidiLearnEnabled()) return juce::Result::fail("Finish MIDI Learn before launching a live hotcue.");
    const auto* clip = project.clipFor(id); const auto* cue = clip ? hotcueFor(*clip, slot) : nullptr;
    if (!clip || !clip->media || !cue || project.clipTransitionLocked(id)) return juce::Result::fail("Select an unlocked segment with a saved HC1-HC9 cue.");
    const double now = position.load(), rate = sampleRate.load();
    const auto destination = hotcueTimelineTime(*clip, slot, liveClipTime(id, now));
    if (!destination) return juce::Result::fail("This source cue is outside the segment. Edit its bounds or choose another cue.");
    const double current = liveClipTime(id,now);
    if (current < clip->start || current >= clip->end()) return juce::Result::fail("This segment is not playing. Locate a stopped segment before starting it.");
    if (!std::isfinite(rate) || rate <= 0) return juce::Result::fail("The programme audio clock is unavailable.");
    const auto boundary = nextLaunchBoundary(project.gridClock(),project.launch,now);
    if (!boundary || boundary->time>project.audibleEnd()+1.e-9) return juce::Result::fail("That launch boundary is unavailable. Choose another timing or correct the project grid.");
    // The target must still have playable content when its boundary arrives.
    if (current + boundary->time - now>clip->end()+1.e-9) return juce::Result::fail("The target ends before this boundary. Choose an earlier launch timing.");
    auto* voice = liveVoiceFor(id);
    if (!voice)
    {
        // Reclaim retired owners only here, never in a device callback.
        liveHotcueVoices.erase(std::remove_if(liveHotcueVoices.begin(),liveHotcueVoices.end(),[&](const auto& v)
        { return !v.status.active && v.status.state != LiveHotcueState::pending && !project.clipFor(v.captured.id); }),liveHotcueVoices.end());
        liveHotcueVoices.emplace_back(); voice = &liveHotcueVoices.back();
    }
    voice->captured = *clip; voice->source = clip->media->analysisSource; voice->cue = *cue;
    const bool active = voice->status.active;
    voice->status = {LiveHotcueState::pending,ScheduledPauseReason::none,slot,boundary->time,boundary->beat,0,active};
    voice->grid = project.tempo; voice->settings = project.launch; voice->bpm = project.bpm; voice->followTempo = project.followTrackTempo;
    voice->rate = rate; voice->expected = now; voice->destination = *destination; liveExclusiveInterrupted = false;
    if (clip->elasticPlayback)
    {
        auto prepared=std::make_unique<ElasticRenderer>(); prepared->prepare(*clip,*destination,rate,blockSize);
        voice->queuedElastic=std::move(prepared);
    }
    else voice->queuedElastic.reset();
    prepareClipProcessing();
    return juce::Result::ok();
}
juce::String Engine::liveHotcueText(const juce::String& id) const
{
    const juce::ScopedLock guard(lock); const auto* voice = liveVoiceFor(id); if (!voice) return {};
    const auto& state = voice->status; const auto* clip = project.clipFor(id);
    const auto owner = clip && clip->media ? clip->media->libraryTitle() : juce::String("Removed segment");
    const auto prefix = "HC" + juce::String(state.slot + 1) + " / " + owner + " / ";
    if (state.state == LiveHotcueState::pending)
        return prefix + "queued / programme bar " + juce::String(state.beat / 4 + 1,2) + " / " + juce::String(state.boundary - project.tempo.origin,3) + " s / press again to cancel";
    if (state.state == LiveHotcueState::cancelled)
    {
        const char* reason = state.reason == ScheduledPauseReason::requested ? "cancelled by you" : state.reason == ScheduledPauseReason::clockChanged ? "grid or timing changed"
            : state.reason == ScheduledPauseReason::deviceChanged ? "device clock changed" : state.reason == ScheduledPauseReason::transportChanged ? "programme transport changed"
            : state.reason == ScheduledPauseReason::missedBoundary ? "boundary missed; trigger again" : "target or programme unavailable; trigger again";
        return prefix + reason + (state.active ? " / prior live cursor continues" : "");
    }
    if (state.active && clip)
    {
        const double at = position.load() + voice->shift;
        return prefix + (at >= clip->end() ? "target ended" : "live source " + juce::String(clip->sourceTimeAt(at-clip->start),3) + " s")
            + " / programme clock unchanged";
    }
    return {};
}
}
