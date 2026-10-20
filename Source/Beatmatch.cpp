#include "Beatmatch.h"
#include "EnergySections.h"
#include <SoundTouch.h>
#include "ElasticAudio.h"
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <juce_cryptography/juce_cryptography.h>

namespace mixworks
{
namespace
{
void configureStretch(soundtouch::SoundTouch& processor, const Media& media)
{
    processor.setSampleRate((unsigned) std::llround(media.sampleRate)); processor.setChannels((unsigned) media.channels());
    processor.setPitch(1); processor.setRate(1); processor.setSetting(SETTING_USE_QUICKSEEK, 0); processor.setSetting(SETTING_USE_AA_FILTER, 1);
    // Keep rhythmic attacks local. SoundTouch's automatic sequence grows to
    // 90 ms at slower tempos, permitting audible beat displacement.
    processor.setSetting(SETTING_SEQUENCE_MS, 40);
    processor.setSetting(SETTING_SEEKWINDOW_MS, 15);
    processor.setSetting(SETTING_OVERLAP_MS, 8);
}
juce::String preparedKey(const Media& media, double rate, const PreparedClipGeometry* geometry = nullptr)
{
    const auto source = PcmStore::decodedKey(media.analysisSource); if (source.isEmpty()) return {};
    juce::MemoryOutputStream stream; stream.writeString("Mixworks SoundTouch 2.4.0 F32 / revision 2 / sequence 40 seek 15 overlap 8"); stream.writeString(source); stream.writeDouble(rate); stream.writeBool(geometry != nullptr);
    if (geometry)
    {
        stream.writeDouble(geometry->offset); stream.writeDouble(geometry->length); stream.writeDouble(geometry->loopSpan); stream.writeDouble(geometry->loopPhase); stream.writeDouble(geometry->sourceBpm); stream.writeBool(geometry->reversed);
        for (const auto& point : geometry->tempo) { stream.writeDouble(point.time); stream.writeDouble(point.bpm); stream.writeBool(point.ramp); }
    }
    return juce::SHA256(stream.getData(), stream.getDataSize()).toHexString();
}
void storePreparation(PreparedAudio& audio, const juce::String& key, double sampleRate)
{
    audio.cache = PcmStore::shared().write(key, sampleRate, audio.samples, audio.cacheWarning);
    if (!audio.cache)
    {
        juce::String temporaryError;
        audio.cache = AudioCache::create(audio.samples, temporaryError);
        if (!audio.cache) audio.cacheWarning += (audio.cacheWarning.isEmpty() ? "" : " ") + temporaryError;
    }
    // Publish a single backing store. Undo snapshots retain the mapping, not another RAM copy.
    if (audio.cache) audio.samples.setSize(0, 0);
}
}
StretchResult stretchAudio(const Media& media, double rate, int64_t available)
{
    if (!media.hasPlayback()) return {{}, "Source playback audio must be loaded before tempo preparation."};
    if (!std::isfinite(rate) || rate <= 0) return { {}, "Invalid tempo ratio." };
    if (std::abs(rate - 1) < 1.0e-8) return {};
    const double frames = std::round(media.frames() / rate);
    const int channels = media.channels();
    if (!std::isfinite(frames) || frames < 1 || frames >= std::numeric_limits<int>::max() || frames * channels * sizeof(float) > maxPreparedBytes)
        return { {}, "Preparing this single track requires more than 1 GiB or has an invalid duration. Trim or split its source into shorter files." };
    auto result = std::make_shared<PreparedAudio>(); result->rate = rate;
    const auto key = preparedKey(media, rate);
    result->cache = PcmStore::shared().read(key, media.sampleRate, channels, (int) frames);
    if (result->cache) { result->fromDiskCache = true; return {result, {}}; }
    if (frames * channels * sizeof(float) > available)
        return { {}, "Tempo preparation cannot allocate more PCM within the 1 GiB RAM budget. Check free cache disk space; earlier preparations could not be moved to disk." };
    result->samples.setSize(channels, (int) frames); result->samples.clear();
    soundtouch::SoundTouch processor; configureStretch(processor, media); processor.setTempo(rate);
    constexpr int block = 2048;
    std::vector<float> input((size_t) block * channels), output((size_t) block * channels); int written = 0;
    auto drain = [&]
    {
        for (;;)
        {
            const int count = (int) processor.receiveSamples(output.data(), block); if (count == 0) break;
            const int copy = std::min(count, result->samples.getNumSamples() - written);
            for (int ch = 0; ch < channels; ++ch) for (int i = 0; i < copy; ++i) result->samples.setSample(ch, written + i, output[(size_t) i * channels + ch]);
            written += copy;
        }
    };
    for (int start = 0; start < media.frames(); start += block)
    {
        const int count = std::min(block, media.frames() - start);
        for (int ch = 0; ch < channels; ++ch) for (int i = 0; i < count; ++i) input[(size_t) i * channels + ch] = media.channelData(ch)[start + i];
        processor.putSamples(input.data(), (unsigned) count); drain();
    }
    processor.flush(); drain();
    if (written < result->samples.getNumSamples() - 2) return { {}, "Time stretching returned incomplete audio. The current session was kept." };
    storePreparation(*result, key, media.sampleRate);
    return {result, {}};
}
namespace
{
using CacheKey = std::pair<const Media*, double>;
juce::String prepareClip(Clip& clip, std::map<CacheKey, std::shared_ptr<const PreparedAudio>>& cache, int64_t& used)
{
    const CacheKey key {clip.media.get(), clip.rate};
    if (auto it = cache.find(key); it != cache.end()) { clip.prepared = it->second; return {}; }
    if (clip.prepared && std::abs(clip.prepared->rate - clip.rate) < 1.0e-8) { cache[key] = clip.prepared; return {}; }
    auto result = stretchAudio(*clip.media, clip.rate, maxPreparedBytes - used);
    if (result.error.isNotEmpty()) return result.error;
    clip.prepared = result.audio; cache[key] = result.audio;
    if (result.audio) used += result.audio->residentBytes();
    return {};
}
}
MatchResult prepareBeatmatch(Project p, double bpm, bool align, double overlapBeats, int64_t existing)
{
    if (p.hasLockedTransitions()) return {std::move(p), "Unlock transitions before rebuilding or retiming the arrangement."};
    MatchResult result; result.project = std::move(p);
    if (!std::isfinite(bpm) || bpm < 20 || bpm > 400) { result.error = "BPM must be between 20 and 400."; return result; }
    const double oldBpm = result.project.bpm; result.project.bpm = bpm; result.project.tempo.fallback = bpm; result.project.tempo.points.clear(); result.project.followTrackTempo = false;
    std::map<CacheKey, std::shared_ptr<const PreparedAudio>> cache; int64_t used = existing;
    const double beat = 60 / bpm;
    for (size_t i = 0; i < result.project.clips.size(); ++i)
    {
        auto& clip = result.project.clips[i]; const double sourceBpm = clip.media->bpm();
        clip.tempoMap.reset(); clip.tempoSourceBpm = 0; clip.preparedOffset = 0; clip.elasticPlayback = false; clip.elasticSource.reset();
        const bool valid = sourceBpm > 0 && (clip.media->manualBpm > 0 || (!clip.media->analysis.variableTempo && clip.media->analysis.tempoConfidence >= 0.2));
        if (valid && (align || clip.beatmatched))
        {
            const double oldRate = clip.rate; clip.rate = bpm / sourceBpm;
            clip.length *= oldRate / clip.rate; clip.fadeIn *= oldRate / clip.rate; clip.fadeOut *= oldRate / clip.rate;
            clip.beatmatched = true; ++result.matched;
            if (!align) clip.start *= oldBpm / bpm;
        }
        else { if (!valid) clip.beatmatched = false; ++result.skipped; }
        auto error = prepareClip(clip, cache, used); if (error.isNotEmpty()) { result.error = error; return result; }
        if (align && valid)
        {
            clip.introAnchor.reset(); clip.outroAnchor.reset(); clip.fadeInAnchor.reset(); clip.fadeOutAnchor.reset();
            const double downbeat = clip.firstDownbeatTime();
            const double localPhase = downbeat >= 0 && (i == 0 || overlapBeats >= 4) ? downbeat : clip.firstBeatTime();
            const double quantum = downbeat >= 0 && (i == 0 || overlapBeats >= 4) ? 4 * beat : beat;
            double desired = 0;
            if (i > 0)
            {
                auto& previous = result.project.clips[i - 1];
                desired = previous.end() - std::min({overlapBeats * beat, previous.length / 2, clip.length / 2});
            }
            clip.start = std::ceil((desired + localPhase - 1.0e-8) / quantum) * quantum - localPhase;
            clip.start = std::max(0.0, clip.start);
            clip.fadeIn = clip.fadeOut = 0;
        }
    }
    if (align)
    {
        result.project.transitionLinks.clear(); result.project.transitionLinksKnown = true;
        for (size_t i = 1; i < result.project.clips.size(); ++i)
        {
            auto& out = result.project.clips[i - 1]; auto& in = result.project.clips[i];
            if (in.start <= out.end())
            {
                initialiseTransition(out, in, out.end() - in.start, TransitionPreset::alignOnly);
                const auto linked = result.project.linkClips(out.id, in.id); if (linked.failed()) { result.error = linked.getErrorMessage(); return result; }
            }
        }
    }
    const auto linked = result.project.reconcileTransitions(); if (linked.failed()) result.error = linked.getErrorMessage();
    return result;
}
namespace
{
bool reliableTempo(const Clip& c)
{
    return c.media->bpm() >= 20 && c.media->bpm() <= 400 && (c.media->manualBpm > 0 || (!c.media->analysis.variableTempo && c.media->analysis.tempoConfidence >= 0.2));
}
juce::String prepareVariableClip(Clip& clip, int64_t& used)
{
    if (!clip.media->hasPlayback()) return "Source playback audio must be loaded before tempo preparation.";
    const auto& media = *clip.media; const int channels = media.channels();
    PreparedClipGeometry geometry {clip.media, clip.offset, clip.length, clip.rate, clip.loopSpan, clip.loopPhase,
        clip.tempoSourceBpm > 0 ? clip.tempoSourceBpm : media.bpm(), clip.reversed, {}};
    geometry.tempo = clip.tempoMap->localPoints(clip.start, clip.end());
    if (clip.prepared && clip.prepared->clipLocal && clip.preparedOffset == 0 && clip.prepared->geometry == geometry) return {};
    const double frames = std::round(clip.length * media.sampleRate);
    if (!std::isfinite(frames) || frames < 1 || frames >= std::numeric_limits<int>::max() || frames * channels * sizeof(float) > maxPreparedBytes)
        return "Preparing this single tempo-ramp clip requires more than 1 GiB or has an invalid duration. Trim or split the clip into shorter sections.";
    auto audio = std::make_shared<PreparedAudio>(); audio->clipLocal = true; audio->rate = clip.rate;
    audio->geometry = std::move(geometry);
    const auto key = preparedKey(media, clip.rate, &audio->geometry);
    audio->cache = PcmStore::shared().read(key, media.sampleRate, channels, (int) frames);
    if (audio->cache) { audio->fromDiskCache = true; clip.prepared = audio; clip.preparedOffset = 0; return {}; }
    if (frames * channels * sizeof(float) > maxPreparedBytes - used)
        return "Tempo preparation cannot allocate more PCM within the 1 GiB RAM budget. Check free cache disk space; earlier preparations could not be moved to disk.";
    audio->samples.setSize(channels, (int) frames); audio->samples.clear();
    soundtouch::SoundTouch processor; configureStretch(processor, media);
    constexpr int block = 128;
    std::vector<float> input((size_t) block * channels), output((size_t) block * channels); int written = 0;
    auto drain = [&]
    {
        for (;;)
        {
            const int received = (int) processor.receiveSamples(output.data(), block); if (received == 0) break;
            const int count = std::min(received, audio->samples.getNumSamples() - written);
            for (int ch = 0; ch < channels; ++ch) for (int i = 0; i < count; ++i) audio->samples.setSample(ch, written + i, output[(size_t) i * channels + ch]);
            written += count;
        }
    };
    // Feed the actual trimmed/reversed/looped source section. Only SoundTouch changes tempo;
    // the audio callback reads the resulting local PCM directly, so seek and export agree.
    const double sourceFrames = std::round(clip.length * clip.rate * media.sampleRate);
    if (!std::isfinite(sourceFrames) || sourceFrames < 1 || sourceFrames >= std::numeric_limits<int64_t>::max()) return "Invalid tempo source span.";
    for (int64_t frame = 0; frame < (int64_t) sourceFrames; frame += block)
    {
        const int count = (int) std::min<int64_t>(block, (int64_t) sourceFrames - frame);
        const double localBegin = clip.localTimeForSource(frame / media.sampleRate);
        const double localEnd = frame + count == (int64_t) sourceFrames ? clip.length : clip.localTimeForSource((frame + count) / media.sampleRate);
        processor.setTempo(count / ((localEnd - localBegin) * media.sampleRate));
        for (int i = 0; i < count; ++i)
        {
            double source = (frame + i) / media.sampleRate;
            if (clip.loopSpan > 0) { source = std::fmod(clip.loopPhase + source, clip.loopSpan); if (source < 0) source += clip.loopSpan; }
            source = clip.offset + (clip.reversed ? clip.sourceSpan() - source : source);
            const double position = juce::jlimit(0.0, (double) (media.frames() - 1), source * media.sampleRate - (clip.reversed ? 1.0 : 0.0));
            const int index = (int) position; const float fraction = (float) (position - index);
            for (int ch = 0; ch < channels; ++ch) { const auto* data = media.channelData(ch); input[(size_t) i * channels + ch] = data[index] + fraction * (data[std::min(index + 1, media.frames() - 1)] - data[index]); }
        }
        processor.putSamples(input.data(), (unsigned) count); drain();
    }
    processor.flush(); drain();
    // SoundTouch rounds its processing windows. Reject material gaps instead of hiding them.
    if (written < audio->samples.getNumSamples() - 2) return "Tempo preparation returned incomplete audio. The current session was kept.";
    storePreparation(*audio, key, media.sampleRate);
    used += audio->residentBytes();
    clip.prepared = audio; clip.preparedOffset = 0; return {};
}
}
namespace
{
// Clip-owned pending intent survives hydration and undo. Construct the shared
// tempo map once, then use the normal preparation path for every affected clip.
struct PendingPlacements { std::vector<std::pair<size_t, size_t>> transitions; juce::String error; };
PendingPlacements placePendingClips(Project& project, double overlapBeats, std::optional<double> livePosition, TransitionPreset preset, bool sectionTransitions, double maxSectionBeats)
{
    const bool liveInsertion = livePosition.has_value();
    PendingPlacements result; auto& transitions = result.transitions;
    TempoMap map = project.followTrackTempo ? project.tempo : TempoMap {project.bpm, {}};
    auto earliest = std::min_element(project.clips.begin(), project.clips.end(), [](const auto& a, const auto& b) { return a.start < b.start; });
    if (!liveInsertion && earliest != project.clips.end() && reliableTempo(*earliest)
        && (map.points.empty() || earliest->pendingPlacement != PendingPlacement::none))
    { map.fallback = earliest->media->bpm(); map.setPoint(0, map.fallback); }
    if (map.points.empty()) map.setPoint(0, project.bpm);
    if (earliest != project.clips.end() && earliest->pendingPlacement != PendingPlacement::none
        && std::none_of(project.clips.begin(), project.clips.end(), [](const auto& c) { return c.pendingPlacement == PendingPlacement::none; })) map.points.front().clipId = earliest->id;
    auto startBeat = [&](const Clip& c) { return c.tempoMap ? c.tempoMap->beatsAt(c.start) : map.beatsAt(c.start); };
    auto endBeat = [&](const Clip& c)
    {
        const double clock = c.tempoSourceBpm > 0 ? c.tempoSourceBpm : c.media->bpm();
        return c.beatmatched && (c.tempoSourceBpm > 0 || reliableTempo(c)) ? startBeat(c) + c.length * c.rate * clock / 60 : map.beatsAt(c.end());
    };
    for (size_t index = 0; index < project.clips.size(); ++index)
    {
        auto& clip = project.clips[index]; const auto intent = clip.pendingPlacement;
        if (intent != PendingPlacement::append && intent != PendingPlacement::drop && intent != PendingPlacement::move) continue;
        const auto selectedPreset = clip.pendingTransitionPreset.value_or(preset);
        double selectedOverlap = selectedPreset == TransitionPreset::alignOnly ? 0 : clip.pendingTransitionBeats.value_or(overlapBeats);
        const bool sections = clip.pendingSectionTransition.value_or(sectionTransitions) && selectedPreset != TransitionPreset::alignOnly && intent != PendingPlacement::move;
        const bool valid = reliableTempo(clip);
        const double bpm = valid ? clip.media->bpm() : map.bpmAt(clip.start), span = clip.length * clip.rate;
        Clip source = clip; source.rate = 1; source.length = span; source.tempoMap.reset();
        const double downbeat = source.firstDownbeatTime();
        const bool alignBar = downbeat >= 0 && (index == 0 || selectedOverlap >= 4 || selectedPreset == TransitionPreset::alignOnly);
        const double phase = (alignBar ? downbeat : source.firstBeatTime()) * bpm / 60;
        const double quantum = alignBar ? 4.0 : 1.0;
        double desired = map.beatsAt(clip.start);
        size_t previous = project.clips.size();
        for (size_t i = 0; i < project.clips.size(); ++i)
        {
            if (i == index || project.clips[i].pendingPlacement != PendingPlacement::none) continue;
            const auto& other = project.clips[i];
            if (intent == PendingPlacement::append)
            { if (previous == project.clips.size() || endBeat(other) > endBeat(project.clips[previous])) previous = i; }
            else if (startBeat(other) <= desired && (previous == project.clips.size() || startBeat(other) > startBeat(project.clips[previous]))) previous = i;
        }
        if (intent == PendingPlacement::append)
        {
            // Append after the end of an established transition chain. A long
            // earlier recording must not steal its existing OUT partner.
            for (size_t visited = 0; previous != project.clips.size() && visited < project.clips.size(); ++visited)
            {
                const auto pending = std::find_if(transitions.begin(), transitions.end(), [&](const auto& item) { return item.first == previous; });
                if (pending != transitions.end()) { previous = pending->second; continue; }
                const auto link = std::find_if(project.transitionLinks.begin(), project.transitionLinks.end(), [&](const auto& item) { return item.outgoing == project.clips[previous].id; });
                if (link == project.transitionLinks.end()) break;
                const auto tail = std::find_if(project.clips.begin(), project.clips.end(), [&](const auto& item) { return item.id == link->incoming; });
                if (tail == project.clips.end()) break;
                previous = (size_t) std::distance(project.clips.begin(), tail);
            }
        }
        if (sections && previous != project.clips.size())
        {
            const auto choice = chooseSectionTransition(project.clips[previous], clip, maxSectionBeats);
            if (!choice) return {{}, "Section transitions need at least 8 bars of usable audio in both tracks. Choose a fixed length for a shorter source."};
            selectedOverlap = choice->overlapBeats;
        }
        if (intent == PendingPlacement::append)
        {
            desired = previous == project.clips.size() ? 0 : std::max(startBeat(project.clips[previous]), endBeat(project.clips[previous]) - selectedOverlap);
            // Default insertion aligns the source's musical grid, not its file
            // edge. Keep all source audio and move forward to the nearest valid
            // bar (or pulse when bar phase is unknown).
            if (valid) desired = std::ceil((desired + phase - 1.0e-8) / quantum) * quantum - phase;
        }
        else if (intent == PendingPlacement::drop) desired = std::round((desired + phase) / quantum) * quantum - phase;
        if (selectedPreset == TransitionPreset::alignOnly && previous != project.clips.size() && intent != PendingPlacement::move)
        {
            const auto& out = project.clips[previous];
            desired = map.beatsAt(out.start + out.transitionTime(false)) - phase;
        }
        if (selectedPreset == TransitionPreset::bassSwap && previous != project.clips.size() && intent != PendingPlacement::move)
            for (const auto& link : project.transitionLinks) if (link.incoming == project.clips[previous].id && link.bassSwap)
            {
                const auto& out = project.clips[previous];
                const double after = map.beatsAt(out.start + out.localTimeForSource((*link.bassSwap)[3])) + 1;
                desired = std::max(desired, std::ceil((after + phase) / quantum) * quantum - phase);
            }
        if (desired < 0) desired = intent == PendingPlacement::drop ? std::ceil(phase / quantum) * quantum - phase : 0;
        if (sections && previous != project.clips.size() && endBeat(project.clips[previous]) - desired < 32 - 1e-7)
            desired -= std::ceil((32 - (endBeat(project.clips[previous]) - desired)) / quantum) * quantum;
        if (livePosition && intent != PendingPlacement::move) desired = std::max(desired, (std::floor((map.beatsAt(*livePosition) + phase) / quantum) + 1) * quantum - phase);
        if (sections && previous != project.clips.size())
        {
            const double actual = endBeat(project.clips[previous]) - desired;
            if (desired < 0 || actual < 32 - 1e-7 || actual > maxSectionBeats + 1e-7 || actual > span * bpm / 60 + 1e-7)
                return {{}, "The chosen section transition cannot fit here. Add the track earlier or choose a fixed length."};
            selectedOverlap = actual;
        }
        const double start = map.timeAtBeat(desired);
        if (previous != project.clips.size()) transitions.emplace_back(previous, index);
        const bool hasMarker = std::any_of(map.points.begin(), map.points.end(), [&](const auto& p) { return p.clipId == clip.id; });
        if (!hasMarker && (previous != project.clips.size() || map.points.front().clipId != clip.id))
        {
            int point = 0;
            if (intent == PendingPlacement::move)
            {
                const auto next = std::upper_bound(map.points.begin(), map.points.end(), start, [](double t, const auto& p) { return t < p.time; });
                const bool ramp = next == map.points.end() || next->ramp;
                point = map.setTrackPoint(clip.id, start, liveInsertion ? map.bpmAt(start) : bpm, ramp);
            }
            else if (liveInsertion)
            {
                // Keep the complete clock of established voices and caller markers.
                // The incoming elastic voice alone follows its new future ramp.
                double guardBeat = std::max(map.beatsAt(*livePosition), map.beatsAt(map.points.back().time));
                for (const auto& established : project.clips)
                    if (established.pendingPlacement == PendingPlacement::none) guardBeat = std::max(guardBeat, endBeat(established));
                const double end = desired + span * bpm / 60;
                const double guardTime = map.timeAtBeat(guardBeat), heldBpm = map.bpmAt(guardTime);
                if (valid && end > guardBeat + 1e-7 && std::abs(bpm - heldBpm) > 1e-7)
                {
                    if (guardTime > map.points.back().time + 1e-9) map.points.push_back({guardTime, heldBpm, false});
                    point = map.addTrackPointAtBeat(clip.id, std::min(end, guardBeat + std::max(4., selectedOverlap)), bpm, true);
                }
                else
                {
                    const double at = map.timeAtBeat(desired + phase);
                    const auto next = std::upper_bound(map.points.begin(), map.points.end(), at, [](double t, const auto& p) { return t < p.time; });
                    point = map.setTrackPoint(clip.id, at, map.bpmAt(at), next == map.points.end() || next->ramp);
                }
            }
            else point = map.addTrackPointAtBeat(clip.id, desired + phase, bpm, selectedOverlap > 0);
            if (!liveInsertion || intent == PendingPlacement::move)
                desired = map.beatsAt(map.points[(size_t) point].time) - (intent == PendingPlacement::move ? 0 : phase);
        }
        clip.start = map.timeAtBeat(desired); clip.length = span; clip.rate = 1; clip.beatmatched = valid;
        clip.fadeIn = clip.fadeOut = 0; clip.prepared.reset(); clip.preparedOffset = 0;
        if (intent != PendingPlacement::move)
        { clip.introAnchor.reset(); clip.outroAnchor.reset(); clip.fadeInAnchor.reset(); clip.fadeOutAnchor.reset(); }
        clip.tempoMap = valid ? std::make_shared<const TempoMap>(map) : nullptr; clip.tempoSourceBpm = 0;
        clip.pendingPlacement = PendingPlacement::none;
    }
    project.tempo = std::move(map); project.bpm = project.tempo.bpmAt(0); project.followTrackTempo = true;
    return result;
}
}
namespace
{
juce::String prepareMappedPlayback(Clip& clip, std::map<CacheKey, std::shared_ptr<const PreparedAudio>>& cache, int64_t& used)
{
    bool variable = false;
    if (clip.tempoMap)
    {
        const auto& map = *clip.tempoMap; const double first = map.bpmAt(clip.start);
        variable = std::abs(first - map.bpmAt(clip.end())) > 1.0e-8;
        for (const auto& point : map.points) if (point.time > clip.start && point.time < clip.end() && std::abs(point.bpm - first) > 1.0e-8) variable = true;
    }
    if (clip.elasticPlayback && clip.tempoMap && clip.tempoSourceBpm > 0)
    {
        clip.prepared.reset(); clip.preparedOffset = 0; clip.elasticSource = std::make_shared<const ElasticSource>(clip); return {};
    }
    if (variable) return prepareVariableClip(clip, used);
    if (clip.prepared && clip.prepared->clipLocal) clip.prepared.reset();
    clip.preparedOffset = 0; return prepareClip(clip, cache, used);
}
}
static MatchResult buildTempoMap(Project p, bool align, double overlapBeats, int64_t existing, size_t alignFrom, std::optional<double> livePosition, TransitionPreset preset, bool sectionTransitions, bool prepareAudio, double maxSectionBeats)
{
    if (align && p.hasLockedTransitions()) return {std::move(p), "Unlock transitions before rebuilding the arrangement."};
    const Project protectedInput = p;
    const bool liveInsertion = livePosition.has_value();
    MatchResult result; result.project = std::move(p); auto& project = result.project;
    std::set<juce::String> inserted;
    std::vector<std::pair<juce::String, double>> explicitMoves;
    for (const auto& clip : project.clips) if (clip.pendingPlacement == PendingPlacement::move) explicitMoves.emplace_back(clip.id, clip.start);
    if (liveInsertion) for (const auto& clip : project.clips)
        if (clip.pendingPlacement == PendingPlacement::append || clip.pendingPlacement == PendingPlacement::drop || clip.pendingPlacement == PendingPlacement::move) inserted.insert(clip.id);
    auto placement = !align && project.hasPendingPlacement() ? placePendingClips(project, overlapBeats, livePosition, preset, sectionTransitions, maxSectionBeats) : PendingPlacements {};
    if (placement.error.isNotEmpty()) return {protectedInput, placement.error};
    auto& placedTransitions = placement.transitions;
    const bool partial = align && alignFrom > 0 && alignFrom < project.clips.size() && project.followTrackTempo && !project.tempo.points.empty();
    if (!partial) alignFrom = 0;
    project.followTrackTempo = true; project.tempo.fallback = project.bpm;
    if (align)
    {
        if (!partial) { project.transitionLinks.clear(); project.transitionLinksKnown = false; }
        for (auto& c : project.clips) if (c.automaticBass) { c.controls.automation[1] = {}; c.automaticBass = false; }
        for (auto& link : project.transitionLinks) { link.bassSwap.reset(); link.bands = {}; }
        TempoMap map = partial ? project.tempo : TempoMap {project.bpm, {}};
        if (!partial) { for (const auto& c : project.clips) if (reliableTempo(c)) { map.fallback = c.media->bpm(); break; } map.setPoint(0, map.fallback); }
        if (!partial && !project.clips.empty()) map.points.front().clipId = project.clips.front().id;
        double previousEndBeat = 0, previousStartBeat = 0;
        if (partial) { const auto& previous = project.clips[alignFrom - 1]; previousStartBeat = map.beatsAt(previous.start); previousEndBeat = map.beatsAt(previous.end()); }
        for (size_t i = alignFrom; i < project.clips.size(); ++i)
        {
            auto& c = project.clips[i]; const double span = c.length * c.rate;
            double requestedOverlap = overlapBeats;
            if (sectionTransitions && i > 0 && preset != TransitionPreset::alignOnly)
            {
                const auto choice = chooseSectionTransition(project.clips[i - 1], c, maxSectionBeats);
                if (!choice) return {protectedInput, "Section transitions need at least 8 bars of usable audio in both tracks. Choose a fixed length for a shorter source."};
                requestedOverlap = choice->overlapBeats;
            }
            const bool valid = reliableTempo(c); const double bpm = valid ? c.media->bpm() : map.bpmAt(map.timeAtBeat(previousEndBeat));
            Clip source = c; source.rate = 1; source.length = span; source.tempoMap.reset();
            const double downbeat = source.firstDownbeatTime();
            const bool alignBar = valid && downbeat >= 0 && (i == 0 || requestedOverlap >= 4);
            const double phaseBeats = valid ? (alignBar ? downbeat : source.firstBeatTime()) * bpm / 60 : 0;
            const double quantum = alignBar ? 4.0 : 1.0;
            const double lengthBeats = span * bpm / 60;
            const double overlap = i == 0 ? 0 : std::max(0.0, std::min(requestedOverlap, previousEndBeat - previousStartBeat));
            double startBeat = std::max(0.0, std::ceil((previousEndBeat - overlap + phaseBeats - 1.0e-8) / quantum) * quantum - phaseBeats);
            if (i > 0)
            {
                if (sectionTransitions && preset != TransitionPreset::alignOnly && previousEndBeat - startBeat < 32 - 1e-7)
                    startBeat -= std::ceil((32 - (previousEndBeat - startBeat)) / quantum) * quantum;
                if (sectionTransitions && preset != TransitionPreset::alignOnly && (startBeat < 0 || previousEndBeat - startBeat > std::min(maxSectionBeats, lengthBeats) + 1e-7))
                    return {protectedInput, "The chosen section transition cannot fit here. Choose a fixed length."};
                const int point = map.addTrackPointAtBeat(c.id, startBeat + phaseBeats, bpm, requestedOverlap > 0);
                startBeat = map.beatsAt(map.points[(size_t) point].time) - phaseBeats;
            }
            const double start = map.timeAtBeat(startBeat);
            c.start = start; c.length = valid ? map.timeAtBeat(startBeat + lengthBeats) - start : span / c.rate;
            c.rate = span / c.length; c.beatmatched = valid; c.tempoMap = valid ? std::make_shared<const TempoMap>(map) : nullptr; c.tempoSourceBpm = 0; c.preparedOffset = 0;
            c.fadeIn = c.fadeOut = 0;
            c.introAnchor.reset(); c.outroAnchor.reset(); c.fadeInAnchor.reset(); c.fadeOutAnchor.reset();
            previousStartBeat = startBeat; previousEndBeat = startBeat + lengthBeats;
        }
        // Reflow all ends once against the completed map, retaining each original source span.
        project.tempo = map; project.bpm = map.bpmAt(0);
    }
    if (project.tempo.points.empty()) project.tempo.setPoint(0, project.bpm);
    auto map = std::make_shared<const TempoMap>(project.tempo);
    std::map<CacheKey, std::shared_ptr<const PreparedAudio>> cache; int64_t used = existing;
    for (size_t index = 0; index < project.clips.size(); ++index)
    {
        auto& c = project.clips[index];
        // A live add preserves established voices and their DSP history.
        if (liveInsertion && !inserted.count(c.id)) continue;
        const bool rebuilding = align && index >= alignFrom;
        const bool captured = !rebuilding && c.beatmatched && c.tempoMap && c.tempoSourceBpm > 0;
        if (!reliableTempo(c) && !captured) { c.beatmatched = false; c.tempoMap.reset(); c.tempoSourceBpm = 0; c.elasticPlayback = false; c.elasticSource.reset(); ++result.skipped; }
        else if ((align && index >= alignFrom) || c.beatmatched)
        {
            const double sourceBpm = captured && c.media->manualBpm <= 0 ? c.tempoSourceBpm : c.media->bpm();
            const bool unchanged = c.tempoMap && c.tempoSourceBpm == sourceBpm && c.tempoMap->beatsAt(c.start) == map->beatsAt(c.start)
                && c.tempoMap->localPoints(c.start, c.end()) == map->localPoints(c.start, c.end());
            if (!unchanged)
            {
            const double span = c.length * c.rate;
            const double startBeat = c.tempoMap ? c.tempoMap->beatsAt(c.start) : map->beatsAt(c.start);
            const auto fades = c.envelope();
            const double intro = fades.intro, outro = fades.outro;
            const double introSource = c.sourceElapsed(intro), outroSource = c.sourceElapsed(outro);
            const double fadeInSource = c.sourceElapsed(intro + c.fadeIn) - introSource;
            const double fadeOutSource = c.sourceElapsed(outro + c.fadeOut) - outroSource;
            c.start = map->timeAtBeat(startBeat); c.length = map->timeAtBeat(startBeat + span * sourceBpm / 60) - c.start; c.rate = span / c.length;
            auto local = [&](double source) { return map->timeAtBeat(startBeat + source * sourceBpm / 60) - c.start; };
            c.fadeIn = local(introSource + fadeInSource) - local(introSource);
            c.fadeOut = local(outroSource + fadeOutSource) - local(outroSource);
            }
            c.tempoMap = map; c.tempoSourceBpm = sourceBpm; ++result.matched;
        }
        if (liveInsertion && c.tempoMap && c.tempoSourceBpm > 0) c.elasticPlayback = true;
        const auto error = prepareAudio ? prepareMappedPlayback(c, cache, used) : juce::String();
        if (error.isNotEmpty()) { result.error = error; return result; }
    }
    if (align) for (size_t i = std::max((size_t) 1, alignFrom); i < project.clips.size(); ++i)
    {
        auto& previous = project.clips[i - 1]; auto& c = project.clips[i];
        const double overlap = juce::jlimit(0.0, std::min(previous.length, c.length), previous.end() - c.start);
        const auto bass = initialiseTransition(previous, c, overlap, preset);
        const auto linked = project.linkClips(previous.id, c.id); if (linked.failed()) { result.error = linked.getErrorMessage(); return result; }
        for (auto& link : project.transitionLinks) if (link.outgoing == previous.id && link.incoming == c.id)
        {
            link.bassSwap = bass;
            if (preset == TransitionPreset::bassSwap && bass)
            { project.clearTransitionBass(link); link.bands[1] = TransitionBandSettings {TransitionBandMode::swap, 1, 0, .5}; previous.automaticBass = c.automaticBass = false; }
        }
    }
    for (const auto& pair : placedTransitions)
    {
        auto& previous = project.clips[pair.first]; auto& incoming = project.clips[pair.second];
        if (std::any_of(project.transitionLinks.begin(), project.transitionLinks.end(), [&](const auto& link)
            { return link.outgoing == previous.id && link.incoming == incoming.id; })) continue;
        const double overlap = std::max(0.0, std::min({previous.length, incoming.length, previous.end() - incoming.start}));
        const auto selectedPreset = incoming.pendingTransitionPreset.value_or(preset);
        const auto bass = initialiseTransition(previous, incoming, overlap, selectedPreset);
        if (selectedPreset == TransitionPreset::alignOnly && std::abs(previous.start + previous.transitionTime(false)
            - incoming.start - incoming.transitionTime(true)) > 1e-6)
        {
            result.error = "The existing OUT cannot meet this incoming IN. Stop playback or move OUT to an upcoming position, then add the track again.";
            return result;
        }
        if (overlap > 0 || selectedPreset == TransitionPreset::alignOnly)
        {
            const auto linked = project.linkClips(previous.id, incoming.id);
            if (linked.failed()) { result.error = linked.getErrorMessage(); return result; }
            for (auto& link : project.transitionLinks) if (link.outgoing == previous.id && link.incoming == incoming.id)
            {
                link.bassSwap = bass;
                if (selectedPreset == TransitionPreset::bassSwap && bass)
                { project.clearTransitionBass(link); link.bands[1] = TransitionBandSettings {TransitionBandMode::swap, 1, 0, .5}; previous.automaticBass = incoming.automaticBass = false; }
            }
        }
    }
    project.ensureTransitionLinks();
    for (auto& clip : project.clips) { clip.pendingTransitionPreset.reset(); clip.pendingTransitionBeats.reset(); clip.pendingSectionTransition.reset(); }
    if (!liveInsertion)
    {
        // A gesture made while source loading was pending owns its requested
        // time. Resolve partners around it rather than pulling it back to an
        // unchanged predecessor during the eventual preparation commit.
        for (const auto& move : explicitMoves)
        {
            const auto applied = project.moveClip(move.first, move.second, false, 0, false);
            if (applied.failed()) { result.error = applied.getErrorMessage(); return result; }
        }
        const auto linked = project.reconcileTransitions(); if (linked.failed()) { result.error = linked.getErrorMessage(); return result; }
    }
    for (auto& clip : project.clips) clip.pendingPlacement = PendingPlacement::none;
    const auto protection = protectedInput.validateTransitionLocks(project);
    if (protection.failed()) { result.error = protection.getErrorMessage(); result.project = protectedInput; }
    return result;
}
MatchResult prepareTempoMap(Project p, bool align, double overlap, int64_t existing, size_t alignFrom, std::optional<double> live, TransitionPreset preset, bool sections)
{
    return buildTempoMap(std::move(p), align, overlap, existing, alignFrom, live, preset, sections, true, 256);
}
MatchResult planTempoMap(Project p, double overlap, TransitionPreset preset, bool sections, double maxSectionBeats)
{
    return buildTempoMap(std::move(p), false, overlap, 0, 0, {}, preset, sections, false, maxSectionBeats);
}
juce::String prepareSavedClips(Project& project)
{
    if (project.followTrackTempo && !project.tempo.points.empty())
    {
        auto map = std::make_shared<const TempoMap>(project.tempo);
        std::map<CacheKey, std::shared_ptr<const PreparedAudio>> cache; int64_t used = 0;
        for (auto& clip : project.clips)
        {
            if (clip.beatmatched)
            {
                // Saved geometry owns playback. For old sessions without a
                // captured clock, derive it from the saved integrated map and
                // source span; fresh detector output must not retime the mix.
                if (clip.tempoSourceBpm <= 0)
                    clip.tempoSourceBpm = 60 * (map->beatsAt(clip.end()) - map->beatsAt(clip.start)) / (clip.length * clip.rate);
                if (!std::isfinite(clip.tempoSourceBpm) || clip.tempoSourceBpm <= 0) return "The saved source clock is invalid. The current session was kept.";
                clip.tempoMap = map;
            }
            const auto error = prepareMappedPlayback(clip, cache, used);
            if (error.isNotEmpty()) return error;
        }
        project.ensureTransitionLinks(); return project.validateTransitionLinks().getErrorMessage();
    }
    std::map<CacheKey, std::shared_ptr<const PreparedAudio>> cache; int64_t bytes = 0;
    for (auto& clip : project.clips) { auto error = prepareClip(clip, cache, bytes); if (error.isNotEmpty()) return error; }
    project.ensureTransitionLinks(); return project.validateTransitionLinks().getErrorMessage();
}
}
