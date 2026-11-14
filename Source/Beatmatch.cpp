#include "Beatmatch.h"
#include "PresetNodes.h"
#include "Hotcues.h"
#include "EnergySections.h"
#include <SoundTouch.h>
#include "ElasticAudio.h"
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <tuple>
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
juce::String preparedKey(const Media& media, double rate, const PreparedClipGeometry* geometry = nullptr, double pitch = 0)
{
    const auto source = PcmStore::decodedKey(media.analysisSource); if (source.isEmpty()) return {};
    juce::MemoryOutputStream stream; stream.writeString("Mixworks SoundTouch 2.4.0 F32 / revision 2 / sequence 40 seek 15 overlap 8"); stream.writeString(source); stream.writeDouble(rate); stream.writeBool(geometry != nullptr);
    if (geometry)
    {
        stream.writeDouble(geometry->offset); stream.writeDouble(geometry->length); stream.writeDouble(geometry->loopSpan); stream.writeDouble(geometry->loopPhase); stream.writeDouble(geometry->sourceBpm); stream.writeBool(geometry->reversed);
        for (const auto& point : geometry->tempo) { stream.writeDouble(point.time); stream.writeDouble(point.bpm); stream.writeBool(point.ramp); }
    }
    // Preserve every existing zero-pitch cache identity. Pitched PCM has its own key.
    if (pitch != 0) { stream.writeString("segment pitch / v1"); stream.writeDouble(pitch); }
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
StretchResult stretchAudio(const Media& media, double rate, int64_t available, double pitch)
{
    if (!media.hasPlayback()) return {{}, "Source playback audio must be loaded before tempo preparation."};
    if (!std::isfinite(rate) || rate <= 0) return { {}, "Invalid tempo ratio." };
    if (!std::isfinite(pitch) || std::abs(pitch) > 6.5) return {{}, "Keep segment pitch between -6.5 and +6.5 semitones."};
    if (std::abs(rate - 1) < 1.0e-8 && pitch == 0) return {};
    const double frames = std::round(media.frames() / rate);
    const int channels = media.channels();
    if (!std::isfinite(frames) || frames < 1 || frames >= std::numeric_limits<int>::max() || frames * channels * sizeof(float) > maxPreparedBytes)
        return { {}, "Preparing this single track requires more than 1 GiB or has an invalid duration. Trim or split its source into shorter files." };
    auto result = std::make_shared<PreparedAudio>(); result->rate = rate; result->pitchSemitones = pitch;
    const auto key = preparedKey(media, rate, nullptr, pitch);
    result->cache = PcmStore::shared().read(key, media.sampleRate, channels, (int) frames);
    if (result->cache) { result->fromDiskCache = true; return {result, {}}; }
    if (frames * channels * sizeof(float) > available)
        return { {}, "Tempo preparation cannot allocate more PCM within the 1 GiB RAM budget. Check free cache disk space; earlier preparations could not be moved to disk." };
    result->samples.setSize(channels, (int) frames); result->samples.clear();
    soundtouch::SoundTouch processor; configureStretch(processor, media); processor.setTempo(rate); processor.setPitchSemiTones(pitch);
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
        for (int ch = 0; ch < channels; ++ch) for (int i = 0; i < count; ++i) input[(size_t) i * channels + ch] = media.sample(ch, start + i);
        processor.putSamples(input.data(), (unsigned) count); drain();
    }
    processor.flush(); drain();
    if (written < result->samples.getNumSamples() - 2) return { {}, "Time stretching returned incomplete audio. The current session was kept." };
    storePreparation(*result, key, media.sampleRate);
    return {result, {}};
}
namespace
{
using CacheKey = std::tuple<const Media*, double, double>;
juce::String prepareClipSource(Clip& clip, std::map<CacheKey, std::shared_ptr<const PreparedAudio>>& cache, int64_t& used)
{
    const CacheKey key {clip.media.get(), clip.rate, clip.pitchSemitones};
    if (auto it = cache.find(key); it != cache.end()) { clip.prepared = it->second; return {}; }
    if (clip.prepared && !clip.prepared->clipLocal && std::abs(clip.prepared->rate - clip.rate) < 1.0e-8
        && clip.prepared->pitchSemitones == clip.pitchSemitones) { cache[key] = clip.prepared; return {}; }
    auto result = stretchAudio(*clip.media, clip.rate, maxPreparedBytes - used, clip.pitchSemitones);
    if (result.error.isNotEmpty()) return result.error;
    clip.prepared = result.audio; cache[key] = result.audio;
    if (result.audio) used += result.audio->residentBytes();
    return {};
 }
juce::String prepareClip(Clip& clip, std::map<CacheKey, std::shared_ptr<const PreparedAudio>>& cache, int64_t& used)
{
    if (clip.media->original)
    {
        clip.prepared.reset(); clip.stemPrepared = {}; clip.preparedOffset = 0;
        clip.elasticPlayback = clip.tempoMap != nullptr || std::abs(clip.rate - 1.) > 1.e-8 || clip.pitchSemitones != 0;
        clip.elasticSource = clip.elasticPlayback ? std::make_shared<const ElasticSource>(clip) : nullptr;
        return {};
    }
    auto error = prepareClipSource(clip, cache, used); if (error.isNotEmpty()) return error;
    if (clip.stems) for (int s = 0; s < stemCount; ++s)
    {
        Clip stem = clip; stem.stems.reset(); stem.media = clip.stems->sources[(size_t) s]; stem.prepared = clip.stemPrepared[(size_t) s];
        error = prepareClipSource(stem, cache, used); if (error.isNotEmpty()) return error;
        clip.stemPrepared[(size_t) s] = stem.prepared;
    }
    return {};
}
}
MatchResult prepareBeatmatch(Project p, double bpm, bool align, double overlapBeats, int64_t existing)
{
    if (p.hasLockedTransitions()) return {std::move(p), "Unlock transitions before rebuilding or retiming the arrangement."};
    MatchResult result; result.project = std::move(p);
    if (!std::isfinite(bpm) || bpm < 20 || bpm > 400) { result.error = "BPM must be between 20 and 400."; return result; }
    const auto existingLinks = result.project.transitionLinks;
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
            clip.introAnchor.reset(); clip.outroAnchor.reset(); clip.fadeInAnchor.reset(); clip.fadeOutAnchor.reset(); clip.manualTransitionArea = false;
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
            if (in.start <= out.end() && std::any_of(existingLinks.begin(),existingLinks.end(),[&](const auto& l) { return l.outgoing == out.id && l.incoming == in.id; }))
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
    geometry.pitchSemitones = clip.pitchSemitones;
    if (clip.prepared && clip.prepared->clipLocal && clip.preparedOffset == 0 && clip.prepared->geometry == geometry) return {};
    const double frames = std::round(clip.length * media.sampleRate);
    if (!std::isfinite(frames) || frames < 1 || frames >= std::numeric_limits<int>::max() || frames * channels * sizeof(float) > maxPreparedBytes)
        return "Preparing this single tempo-ramp segment requires more than 1 GiB or has an invalid duration. Trim or split the segment into shorter sections.";
    auto audio = std::make_shared<PreparedAudio>(); audio->clipLocal = true; audio->rate = clip.rate;
    audio->pitchSemitones = clip.pitchSemitones;
    audio->geometry = std::move(geometry);
    const auto key = preparedKey(media, clip.rate, &audio->geometry, clip.pitchSemitones);
    audio->cache = PcmStore::shared().read(key, media.sampleRate, channels, (int) frames);
    if (audio->cache) { audio->fromDiskCache = true; clip.prepared = audio; clip.preparedOffset = 0; return {}; }
    if (frames * channels * sizeof(float) > maxPreparedBytes - used)
        return "Tempo preparation cannot allocate more PCM within the 1 GiB RAM budget. Check free cache disk space; earlier preparations could not be moved to disk.";
    audio->samples.setSize(channels, (int) frames); audio->samples.clear();
    soundtouch::SoundTouch processor; configureStretch(processor, media);
    processor.setPitchSemiTones(clip.pitchSemitones);
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
            for (int ch = 0; ch < channels; ++ch) { const float a = media.sample(ch, index); input[(size_t) i * channels + ch] = a + fraction * (media.sample(ch, std::min(index + 1, media.frames() - 1)) - a); }
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
struct PendingTransition { size_t outgoing, incoming; bool markedJoin; };
struct PendingPlacements { std::vector<PendingTransition> transitions; juce::String error; };
// Default targets may finish after IN; placement stays on its requested beat.
// Never shorten the ramp to fit a small source or squeeze it against a manual
// neighbour. Such sources follow the existing clock without adding a target.
bool addSpacedTempoTarget(TempoMap& map, const juce::String& id, double inBeat, double endBeat, double bpm, bool ramp)
{
    if (!ramp) { map.addTrackPointAtBeat(id, inBeat, bpm, false); return true; }
    const double at = map.timeAtBeat(inBeat);
    const auto next = std::upper_bound(map.points.begin(), map.points.end(), at + 1e-9,
        [](double time, const auto& point) { return time < point.time; });
    if (next == map.points.begin()) return false;
    const auto& previous = *(next - 1);
    if (std::abs(previous.bpm - bpm) < 1e-7)
    {
        // A nominally same-BPM point inside a future ramp would hold its start
        // longer and compress the remaining change. Leave that ramp intact.
        if (next != map.points.end() && next->ramp && std::abs(next->bpm - bpm) > 1e-7) return false;
        map.addTrackPointAtBeat(id, inBeat, bpm, true); return true;
    }
    constexpr double minimumBeats = 64; // Sixteen 4/4 bars, independent of overlap length.
    const double targetBeat = std::max(inBeat, map.beatsAt(previous.time) + minimumBeats);
    if (targetBeat > endBeat + 1e-7) return false;
    auto candidate = map;
    const int index = candidate.addTrackPointAtBeat(id, targetBeat, bpm, true);
    const auto& target = candidate.points[(size_t) index];
    if (next != map.points.end() && target.time >= next->time - 1e-9) return false;
    if (index > 0 && candidate.beatsAt(target.time) - candidate.beatsAt(candidate.points[(size_t) index - 1].time) < minimumBeats - 1e-7) return false;
    if (index + 1 < (int) candidate.points.size()
        && candidate.beatsAt(candidate.points[(size_t) index + 1].time) - candidate.beatsAt(target.time) < minimumBeats - 1e-7) return false;
    map = std::move(candidate);
    return true;
}
std::optional<juce::Range<double>> sectionWindow(Clip& out, Clip& in, const TempoMap& clock, bool enabled)
{
    const auto window = enabled ? sectionTransitionWindow(out, in, clock) : std::nullopt;
    if (window)
    {
        out.outroAnchor = out.sourceElapsed(window->getStart() - out.start);
        in.introAnchor = in.sourceElapsed(window->getStart() - in.start);
        out.fadeOutAnchor = out.sourceElapsed(window->getEnd() - out.start);
        in.fadeInAnchor = in.sourceElapsed(window->getStart() - in.start);
    }
    return window;
}
PendingPlacements placePendingClips(Project& project, double overlapBeats, std::optional<double> livePosition, TransitionPreset preset, bool sectionTransitions, double maxSectionBeats)
{
    const bool liveInsertion = livePosition.has_value();
    PendingPlacements result; auto& transitions = result.transitions;
    TempoMap map = project.gridClock();
    auto earliest = std::min_element(project.clips.begin(), project.clips.end(), [](const auto& a, const auto& b) { return a.start < b.start; });
    if (!liveInsertion && earliest != project.clips.end() && reliableTempo(*earliest)
        && (map.points.empty() || earliest->pendingPlacement != PendingPlacement::none))
    { map.fallback = earliest->media->bpm(); map.setPoint(0, map.fallback); if (map.origin > 0 && std::none_of(project.clips.begin(), project.clips.end(), [](const auto& c) { return c.pendingPlacement == PendingPlacement::none; })) map.setPoint(map.origin, map.fallback, false); }
    if (map.points.empty()) map.setPoint(0, project.bpm);
    if (map.origin > 0 && map.points.size() == 1) map.setPoint(map.origin, map.bpmAt(0), false);
    if (earliest != project.clips.end() && earliest->pendingPlacement != PendingPlacement::none
        && std::none_of(project.clips.begin(), project.clips.end(), [](const auto& c) { return c.pendingPlacement == PendingPlacement::none; }))
    {
        auto marker = std::find_if(map.points.begin(), map.points.end(), [&](const auto& point) { return point.time == map.origin; });
        if (marker != map.points.end()) marker->clipId = earliest->id;
    }
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
        const bool automatic = clip.pendingAutoTransition;
        const auto selectedPreset = clip.pendingTransitionPreset.value_or(preset);
        double selectedOverlap = !automatic || selectedPreset == TransitionPreset::alignOnly ? 0 : clip.pendingTransitionBeats.value_or(overlapBeats);
        const bool sections = automatic && clip.pendingSectionTransition.value_or(sectionTransitions) && selectedPreset != TransitionPreset::alignOnly && intent != PendingPlacement::move;
        const bool valid = reliableTempo(clip);
        const double bpm = valid ? clip.media->bpm() : map.bpmAt(clip.start), span = clip.length * clip.rate;
        Clip source = clip; source.rate = 1; source.length = span; source.tempoMap.reset();
        double desired = map.beatsAt(clip.start);
        size_t previous = project.clips.size();
        size_t marked = project.clips.size();
        for (size_t i = 0; i < project.clips.size(); ++i)
        {
            if (i == index || project.clips[i].pendingPlacement != PendingPlacement::none) continue;
            const auto& other = project.clips[i];
            if (intent == PendingPlacement::append)
            { if (previous == project.clips.size() || endBeat(other) > endBeat(project.clips[previous])) previous = i; }
            else if (startBeat(other) <= desired && (previous == project.clips.size() || startBeat(other) > startBeat(project.clips[previous]))) previous = i;
            if (i < index && other.outroAnchor
                && (intent == PendingPlacement::append || (!automatic && intent == PendingPlacement::drop
                    && other.track != clip.track && clip.start >= other.start && clip.start <= other.end()))
                && std::none_of(project.transitionLinks.begin(), project.transitionLinks.end(), [&](const auto& link) { return link.outgoing == other.id; })
                && std::none_of(transitions.begin(), transitions.end(), [&](const auto& pair) { return pair.outgoing == i; }))
                marked = i;
        }
        // An explicit OUT can receive an append or a drop across its track.
        // Keep the join ahead of the playhead and inside the arrangement.
        const double downbeat = source.firstDownbeatTime();
        const double cue = valid ? (downbeat >= 0 ? downbeat : source.firstBeatTime()) : 0;
        const double joinBeat = marked == project.clips.size() ? 0
            : map.beatsAt(project.clips[marked].start + project.clips[marked].transitionTime(false));
        const double joinStart = joinBeat - cue * bpm / 60;
        const bool markedJoin = !automatic && marked != project.clips.size() && (intent == PendingPlacement::append
            || (joinStart >= map.beatsAt(0) && (!livePosition || joinStart > map.beatsAt(*livePosition))));
        if (markedJoin || (intent == PendingPlacement::append && marked != project.clips.size())) previous = marked;
        // Independent additions use the source grid regardless of the transition preset.
        const bool independentInsertion = !automatic && intent != PendingPlacement::move;
        const bool alignBar = valid && downbeat >= 0 && (independentInsertion || index == 0
            || selectedOverlap >= 4 || selectedPreset == TransitionPreset::alignOnly);
        const double phase = markedJoin ? cue * bpm / 60 : (alignBar ? downbeat : source.firstBeatTime()) * bpm / 60;
        const double quantum = alignBar ? 4.0 : 1.0;
        if (automatic && intent == PendingPlacement::append)
        {
            // Append after the end of an established transition chain. A long
            // earlier recording must not steal its existing OUT partner.
            for (size_t visited = 0; previous != project.clips.size() && visited < project.clips.size(); ++visited)
            {
                const auto pending = std::find_if(transitions.begin(), transitions.end(), [&](const auto& item) { return item.outgoing == previous; });
                if (pending != transitions.end()) { previous = pending->incoming; continue; }
                const auto link = std::find_if(project.transitionLinks.begin(), project.transitionLinks.end(), [&](const auto& item) { return item.outgoing == project.clips[previous].id; });
                if (link == project.transitionLinks.end()) break;
                const auto tail = std::find_if(project.clips.begin(), project.clips.end(), [&](const auto& item) { return item.id == link->incoming; });
                if (tail == project.clips.end()) break;
                previous = (size_t) std::distance(project.clips.begin(), tail);
            }
        }
        std::optional<SectionTransitionChoice> sectionChoice;
        if (sections && previous != project.clips.size())
        {
            sectionChoice = chooseSectionTransition(project.clips[previous], clip, maxSectionBeats);
            if (!sectionChoice) return {{}, "Section transitions need at least 8 bars of usable audio in both tracks. Choose a fixed length for a shorter source."};
            selectedOverlap = sectionChoice->overlapBeats;
        }
        if (intent == PendingPlacement::append)
        {
            desired = previous == project.clips.size() ? 0 : std::max(startBeat(project.clips[previous]), endBeat(project.clips[previous]) - selectedOverlap);
            // Default insertion aligns the source's musical grid, not its file
            // edge. Keep all source audio and move forward to the nearest valid
            // bar (or pulse when bar phase is unknown).
            if (valid) desired = previous == project.clips.size() && map.origin > 0 ? -phase
                : std::ceil((desired + phase - 1.0e-8) / quantum) * quantum - phase;
        }
        else if (intent == PendingPlacement::drop) desired = std::round((desired + phase) / quantum) * quantum - phase;
        std::optional<double> matchedStart;
        if (sectionChoice && sectionChoice->boundaries)
        {
            const auto& out = project.clips[previous];
            const double outBpm = out.tempoSourceBpm > 0 ? out.tempoSourceBpm : out.media->bpm();
            const double remaining = (out.offset + out.audioSourceElapsed(out.length) - (*sectionChoice->boundaries)[0]) * outBpm / 60;
            const double offset = ((*sectionChoice->boundaries)[1] - clip.offset - clip.audioSourceElapsed(0)) * bpm / 60;
            desired = endBeat(out) - remaining - offset;
            matchedStart = desired;
        }
        if (automatic && selectedPreset == TransitionPreset::alignOnly && previous != project.clips.size() && intent != PendingPlacement::move)
        {
            const auto& out = project.clips[previous];
            desired = map.beatsAt(out.start + out.transitionTime(false)) - phase;
        }
        if (markedJoin) desired = joinStart;
        if (automatic && selectedPreset == TransitionPreset::bassSwap && previous != project.clips.size() && intent != PendingPlacement::move)
            for (const auto& link : project.transitionLinks) if (link.incoming == project.clips[previous].id && link.bassSwap)
            {
                const auto& out = project.clips[previous];
                const double after = map.beatsAt(out.start + out.localTimeForSource((*link.bassSwap)[3])) + 1;
                desired = std::max(desired, std::ceil((after + phase) / quantum) * quantum - phase);
            }
        const double earliestBeat = map.beatsAt(0);
        if (desired < earliestBeat) desired = std::ceil((earliestBeat + phase) / quantum) * quantum - phase;
        if (sections && previous != project.clips.size() && endBeat(project.clips[previous]) - desired < 32 - 1e-7)
            desired -= std::ceil((32 - (endBeat(project.clips[previous]) - desired)) / quantum) * quantum;
        if (livePosition && intent != PendingPlacement::move && !(markedJoin && intent == PendingPlacement::drop))
            desired = std::max(desired, (std::floor((map.beatsAt(*livePosition) + phase) / quantum) + 1) * quantum - phase);
        if (sections && previous != project.clips.size())
        {
            const double actual = endBeat(project.clips[previous]) - desired;
            const double limit = sectionChoice && sectionChoice->boundaries ? std::min(endBeat(project.clips[previous]) - startBeat(project.clips[previous]), span * bpm / 60) : std::min(maxSectionBeats, span * bpm / 60);
            if (desired < earliestBeat || actual < 32 - 1e-7 || actual > limit + 1e-7 || (matchedStart && std::abs(desired - *matchedStart) > 1e-7))
                return {{}, "The chosen section transition cannot fit here. Add the track earlier or choose a fixed length."};
            selectedOverlap = actual;
        }
        const double start = map.timeAtBeat(desired);
        if ((automatic || markedJoin) && previous != project.clips.size())
            transitions.push_back({previous, index, markedJoin});
        const bool hasMarker = std::any_of(map.points.begin(), map.points.end(), [&](const auto& p) { return p.clipId == clip.id; });
        if (markedJoin && valid && !hasMarker && !project.hasLockedTransitions())
        {
            // Preserve the clock through the authored OUT, including any
            // existing targets. During playback also preserve active voices.
            double hold = std::max(joinBeat, map.beatsAt(map.points.back().time));
            if (livePosition)
                for (const auto& established : project.clips)
                    if (established.pendingPlacement == PendingPlacement::none) hold = std::max(hold, endBeat(established));
            const double holdTime = map.timeAtBeat(hold), heldBpm = map.bpmAt(holdTime);
            auto candidate = map;
            if (holdTime > map.points.back().time + 1e-9) candidate.setPoint(holdTime, heldBpm, false);
            if (std::abs(bpm - heldBpm) > 1e-7 && addSpacedTempoTarget(candidate, clip.id, hold, desired + span * bpm / 60, bpm, true))
                map = std::move(candidate);
            else
            {
                // Keep a visible, editable marker even if there is too little
                // audio for a ramp. Inserting on the curve preserves its shape.
                const double at = map.timeAtBeat(joinBeat);
                const auto next = std::upper_bound(map.points.begin(), map.points.end(), at,
                    [](double time, const auto& point) { return time < point.time; });
                map.setTrackPoint(clip.id, at, map.bpmAt(at), next != map.points.end() && next->ramp);
            }
        }
        else if (!markedJoin && !hasMarker && (previous != project.clips.size() || map.points.front().clipId != clip.id))
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
                    auto candidate = map;
                    if (guardTime > candidate.points.back().time + 1e-9) candidate.points.push_back({guardTime, heldBpm, false});
                    if (addSpacedTempoTarget(candidate, clip.id, guardBeat, end, bpm, true)) map = std::move(candidate);
                }
                else
                {
                    const double at = map.timeAtBeat(desired + phase);
                    const auto next = std::upper_bound(map.points.begin(), map.points.end(), at, [](double t, const auto& p) { return t < p.time; });
                    point = map.setTrackPoint(clip.id, at, map.bpmAt(at), next == map.points.end() || next->ramp);
                }
            }
            else addSpacedTempoTarget(map, clip.id, desired + phase, desired + span * bpm / 60, bpm, selectedOverlap > 0);
            if (intent == PendingPlacement::move)
                desired = map.beatsAt(map.points[(size_t) point].time) - (intent == PendingPlacement::move ? 0 : phase);
        }
        clip.start = map.timeAtBeat(desired); clip.length = span; clip.rate = 1; clip.beatmatched = valid;
        clip.fadeIn = clip.fadeOut = 0; clip.prepared.reset(); clip.preparedOffset = 0;
        if (intent != PendingPlacement::move)
        { clip.introAnchor.reset(); clip.outroAnchor.reset(); clip.fadeInAnchor.reset(); clip.fadeOutAnchor.reset(); }
        if (markedJoin) clip.introAnchor = cue;
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
    if (clip.media->original)
    {
        clip.prepared.reset(); clip.stemPrepared = {}; clip.preparedOffset = 0;
        clip.elasticPlayback = clip.tempoMap != nullptr || std::abs(clip.rate - 1.) > 1.e-8 || clip.pitchSemitones != 0;
        clip.elasticSource = clip.elasticPlayback ? std::make_shared<const ElasticSource>(clip) : nullptr;
        return {};
    }
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
    if (variable)
    {
        auto error = prepareVariableClip(clip, used); if (error.isNotEmpty()) return error;
        if (clip.stems) for (int s = 0; s < stemCount; ++s)
        {
            Clip stem = clip; stem.stems.reset(); stem.media = clip.stems->sources[(size_t) s]; stem.prepared = clip.stemPrepared[(size_t) s];
            error = prepareVariableClip(stem, used); if (error.isNotEmpty()) return error;
            clip.stemPrepared[(size_t) s] = stem.prepared;
        }
        return {};
    }
    for (auto& stem : clip.stemPrepared) if (stem && stem->clipLocal) stem.reset();
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
        if (!partial) { project.transitionLinks.clear(); project.transitionLinksKnown = true; }
        for (auto& c : project.clips) if (c.automaticBass) { c.controls.automation[1] = {}; c.automaticBass = false; }
        for (auto& link : project.transitionLinks) { link.bassSwap.reset(); link.bands = {}; }
        TempoMap map = partial ? project.tempo : TempoMap {project.bpm, {}, project.tempo.origin};
        if (!partial) { for (const auto& c : project.clips) if (reliableTempo(c)) { map.fallback = c.media->bpm(); break; } map.setPoint(0, map.fallback); }
        if (!partial && !project.clips.empty())
        {
            const int marker = map.origin > 0 ? map.setPoint(map.origin, map.fallback, false) : 0;
            map.points[(size_t) marker].clipId = project.clips.front().id;
        }
        double previousEndBeat = 0, previousStartBeat = 0;
        if (partial) { const auto& previous = project.clips[alignFrom - 1]; previousStartBeat = map.beatsAt(previous.start); previousEndBeat = map.beatsAt(previous.end()); }
        for (size_t i = alignFrom; i < project.clips.size(); ++i)
        {
            auto& c = project.clips[i]; const double span = c.length * c.rate;
            double requestedOverlap = overlapBeats;
            std::optional<SectionTransitionChoice> sectionChoice;
            if (sectionTransitions && i > 0 && preset != TransitionPreset::alignOnly)
            {
                sectionChoice = chooseSectionTransition(project.clips[i - 1], c, maxSectionBeats);
                if (!sectionChoice) return {protectedInput, "Section transitions need at least 8 bars of usable audio in both tracks. Choose a fixed length for a shorter source."};
                requestedOverlap = sectionChoice->overlapBeats;
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
            if (sectionChoice && sectionChoice->boundaries)
            {
                const auto& out = project.clips[i - 1];
                const double outBpm = out.tempoSourceBpm > 0 ? out.tempoSourceBpm : out.media->bpm();
                startBeat = previousEndBeat - (out.offset + out.audioSourceElapsed(out.length) - (*sectionChoice->boundaries)[0]) * outBpm / 60
                    - ((*sectionChoice->boundaries)[1] - c.offset - c.audioSourceElapsed(0)) * bpm / 60;
            }
            if (i > 0)
            {
                if (sectionTransitions && preset != TransitionPreset::alignOnly && previousEndBeat - startBeat < 32 - 1e-7)
                    startBeat -= std::ceil((32 - (previousEndBeat - startBeat)) / quantum) * quantum;
                const double limit = sectionChoice && sectionChoice->boundaries ? std::min(previousEndBeat - previousStartBeat, lengthBeats) : std::min(maxSectionBeats, lengthBeats);
                if (sectionTransitions && preset != TransitionPreset::alignOnly && (startBeat < map.beatsAt(0) || previousEndBeat - startBeat > limit + 1e-7))
                    return {protectedInput, "The chosen section transition cannot fit here. Choose a fixed length."};
                const auto existingTarget = std::find_if(map.points.begin(), map.points.end(), [&](const auto& point) { return point.clipId == c.id; });
                if (existingTarget != map.points.end()) startBeat = map.beatsAt(existingTarget->time) - phaseBeats;
                else addSpacedTempoTarget(map, c.id, startBeat + phaseBeats, startBeat + lengthBeats, bpm, requestedOverlap > 0);
            }
            const double start = map.timeAtBeat(startBeat);
            c.start = start; c.length = valid ? map.timeAtBeat(startBeat + lengthBeats) - start : span / c.rate;
            c.rate = span / c.length; c.beatmatched = valid; c.tempoMap = valid ? std::make_shared<const TempoMap>(map) : nullptr; c.tempoSourceBpm = 0; c.preparedOffset = 0;
            c.fadeIn = c.fadeOut = 0;
            c.introAnchor.reset(); c.outroAnchor.reset(); c.fadeInAnchor.reset(); c.fadeOutAnchor.reset(); c.manualTransitionArea = false;
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
    // Placement and audio preparation have completed in this private plan.
    // Preset application below uses the same atomic frame replacement as the
    // inspector, so a rebuild cannot multiply an existing ordinary shape.
    for (auto& clip : project.clips) clip.pendingPlacement = PendingPlacement::none;
    if (align) for (size_t i = std::max((size_t) 1, alignFrom); i < project.clips.size(); ++i)
    {
        const auto* existingLink = protectedInput.transitionFor(project.clips[i].id);
        if (!existingLink || existingLink->outgoing != project.clips[i-1].id) continue;
        auto& previous = project.clips[i - 1]; auto& c = project.clips[i];
        const double overlap = juce::jlimit(0.0, std::min(previous.length, c.length), previous.end() - c.start);
        const auto window = sectionWindow(previous, c, project.gridClock(), sectionTransitions && preset != TransitionPreset::alignOnly);
        const auto bass = initialiseTransition(previous, c, overlap, preset, window);
        const auto linked = project.linkClips(previous.id, c.id); if (linked.failed()) { result.error = linked.getErrorMessage(); return result; }
        for (auto& link : project.transitionLinks) if (link.outgoing == previous.id && link.incoming == c.id)
        {
            link.muteOutside = existingLink->muteOutside;
            link.bassSwap = bass;
            if (preset == TransitionPreset::bassSwap && bass)
            { project.clearTransitionBass(link); link.bands[1] = TransitionBandSettings {TransitionBandMode::swap, 1, 0, sectionSwapPosition(project, link)}; previous.automaticBass = c.automaticBass = false; }
        }
        const auto incomingId = c.id; const auto* relationship = project.transitionFor(incomingId);
        const auto regular = relationship && project.transitionRange(*relationship).getLength() > 1.e-6
            ? project.applyTransitionPreset(incomingId,preset) : juce::Result::ok();
        if (regular.failed()) { result.error = regular.getErrorMessage(); return result; }
    }
    for (const auto& pair : placedTransitions)
    {
        auto& previous = project.clips[pair.outgoing]; auto& incoming = project.clips[pair.incoming];
        if (std::any_of(project.transitionLinks.begin(), project.transitionLinks.end(), [&](const auto& link)
            { return link.outgoing == previous.id && link.incoming == incoming.id; })) continue;
        const double overlap = std::max(0.0, std::min({previous.length, incoming.length, previous.end() - incoming.start}));
        const auto selectedPreset = incoming.pendingTransitionPreset.value_or(preset);
        const auto window = sectionWindow(previous, incoming, project.gridClock(),
            !pair.markedJoin && incoming.pendingSectionTransition.value_or(sectionTransitions) && selectedPreset != TransitionPreset::alignOnly);
        const auto bass = pair.markedJoin ? std::optional<std::array<double, 4>>{}
            : initialiseTransition(previous, incoming, overlap, selectedPreset, window);
        if ((pair.markedJoin || selectedPreset == TransitionPreset::alignOnly) && std::abs(previous.start + previous.transitionTime(false)
            - incoming.start - incoming.transitionTime(true)) > 1e-6)
        {
            result.error = "The existing OUT cannot meet this incoming IN. Stop playback or move OUT to an upcoming position, then add the track again.";
            return result;
        }
        if (overlap > 0 || pair.markedJoin || selectedPreset == TransitionPreset::alignOnly)
        {
            const auto linked = project.linkClips(previous.id, incoming.id);
            if (linked.failed()) { result.error = linked.getErrorMessage(); return result; }
            for (auto& link : project.transitionLinks) if (link.outgoing == previous.id && link.incoming == incoming.id)
            {
                link.bassSwap = bass;
                if (pair.markedJoin)
                {
                    const double begin = previous.transitionTime(false);
                    const auto clock = project.gridClock();
                    const double at = previous.start + begin;
                    const double preferredEnd = clock.timeAtBeat(clock.beatsAt(at) + (overlapBeats > 0 ? overlapBeats : 16.));
                    const double end = std::max(begin, std::min({previous.end(), incoming.end(), preferredEnd}) - previous.start);
                    link.frame = std::array<double, 2> {*previous.outroAnchor, std::max(*previous.outroAnchor, previous.sourceElapsed(end))};
                    link.alignmentOnly = true;
                }
                if (selectedPreset == TransitionPreset::bassSwap && bass)
                { project.clearTransitionBass(link); link.bands[1] = TransitionBandSettings {TransitionBandMode::swap, 1, 0, sectionSwapPosition(project, link)}; previous.automaticBass = incoming.automaticBass = false; }
            }
            const auto incomingId = incoming.id; const auto* relationship = project.transitionFor(incomingId);
            const auto regular = relationship && !pair.markedJoin
                && project.transitionRange(*relationship).getLength() > 1.e-6
                ? project.applyTransitionPreset(incomingId,selectedPreset) : juce::Result::ok();
            if (regular.failed()) { result.error = regular.getErrorMessage(); return result; }
        }
    }
    project.ensureTransitionLinks();
    for (auto& clip : project.clips) { clip.pendingAutoTransition = false; clip.pendingTransitionPreset.reset(); clip.pendingTransitionBeats.reset(); clip.pendingSectionTransition.reset(); }
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
    for (auto& clip : project.clips)
    {
        clip.pendingPlacement = PendingPlacement::none;
        if (clip.hotcuesPending && !clip.hotcues) initialiseHotcues(clip);
    }
    const auto protection = protectedInput.validateTransitionLocks(project);
    if (protection.failed()) { result.error = protection.getErrorMessage(); result.project = protectedInput; }
    return result;
}
MatchResult prepareTempoMap(Project p, bool align, double overlap, int64_t existing, size_t alignFrom, std::optional<double> live, TransitionPreset preset, bool sections)
{
    return buildTempoMap(std::move(p), align, overlap, existing, alignFrom, live, preset, sections, true, 256);
}
MatchResult planTempoMap(Project p, double overlap, TransitionPreset preset, bool sections, double maxSectionBeats, std::optional<double> livePosition)
{
    return buildTempoMap(std::move(p), false, overlap, 0, 0, livePosition, preset, sections, false, maxSectionBeats);
}
juce::String prepareAppendedClips(Project& project, size_t first, int64_t existingResidentBytes)
{
    std::map<CacheKey, std::shared_ptr<const PreparedAudio>> cache; int64_t used = existingResidentBytes;
    for (size_t i = first; i < project.clips.size(); ++i)
        if (auto error = prepareMappedPlayback(project.clips[i],cache,used); error.isNotEmpty()) return error;
    return project.validateTransitionLinks().getErrorMessage();
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
