#include "Model.h"
#include "ElasticAudio.h"
#include "SourceSectionStore.h"
#include <cmath>
#include <cstdlib>
#include <limits>
#include <set>
#include <map>

namespace mixworks
{
bool PreparedClipGeometry::operator==(const PreparedClipGeometry& other) const
{
    if (source != other.source || offset != other.offset || length != other.length || rate != other.rate || loopSpan != other.loopSpan
        || loopPhase != other.loopPhase || sourceBpm != other.sourceBpm || reversed != other.reversed) return false;
    return tempo == other.tempo;
}
double TempoMap::bpmAt(double time) const
{
    if (points.empty()) return fallback;
    auto next = std::upper_bound(points.begin(), points.end(), time, [](double t, const auto& p) { return t < p.time; });
    if (next == points.begin()) return next->bpm;
    const auto& previous = *(next - 1);
    if (next == points.end() || !next->ramp) return previous.bpm;
    return previous.bpm + (next->bpm - previous.bpm) * (time - previous.time) / (next->time - previous.time);
}
double TempoMap::beatsAt(double time) const
{
    const double beat = beatsFromStart(time);
    return origin == 0 ? beat : beat - beatsFromStart(origin);
}
double TempoMap::beatsFromStart(double time) const
{
    time = std::max(0.0, time);
    double beats = 0, from = 0, bpm = points.empty() ? fallback : points.front().bpm;
    for (const auto& p : points)
    {
        if (p.time <= from) { bpm = p.bpm; continue; }
        const double span = std::min(time, p.time) - from;
        if (span > 0) beats += (bpm * span + (p.ramp ? (p.bpm - bpm) * span * span / (2 * (p.time - from)) : 0)) / 60;
        if (time <= p.time) return beats;
        from = p.time; bpm = p.bpm;
    }
    return beats + (time - from) * bpm / 60;
}
double TempoMap::timeAtBeat(double beat) const
{
    return timeFromStartBeat(beat + (origin == 0 ? 0 : beatsFromStart(origin)));
}
double TempoMap::timeFromStartBeat(double beat) const
{
    beat = std::max(0.0, beat);
    double from = 0, bpm = points.empty() ? fallback : points.front().bpm;
    for (const auto& p : points)
    {
        if (p.time <= from) { bpm = p.bpm; continue; }
        const double duration = p.time - from, slope = p.ramp ? (p.bpm - bpm) / duration : 0;
        const double count = (bpm + slope * duration / 2) * duration / 60;
        if (beat <= count)
        {
            // Stable quadratic inverse, including descending ramps and almost-flat segments.
            const double seconds = 120 * beat / (bpm + std::sqrt(std::max(0.0, bpm * bpm + 120 * slope * beat)));
            return from + seconds;
        }
        beat -= count; from = p.time; bpm = p.bpm;
    }
    return from + beat * 60 / bpm;
}
double TempoMap::snap(double time, bool enabled, double step) const { return enabled ? timeAtBeat(std::round(beatsAt(time) / step) * step) : std::max(0.0, time); }
double TempoMap::snapWithPreroll(double time, bool enabled, double step) const
{
    if (!enabled) return time;
    if (time >= 0) return snap(time, true, step);
    const double beat = 60 / bpmAt(0) * step;
    return std::round(time / beat) * beat;
}
std::vector<TempoPoint> TempoMap::localPoints(double begin, double end) const
{
    std::vector<TempoPoint> result {{0, bpmAt(begin), true}};
    for (const auto& point : points) if (point.time > begin && point.time < end) result.push_back({point.time - begin, point.bpm, point.ramp});
    const auto next = std::lower_bound(points.begin(), points.end(), end, [](const auto& point, double time) { return point.time < time; });
    result.push_back({end - begin, bpmAt(end), next != points.end() && next->ramp});
    return result;
}
int TempoMap::setPoint(double time, double bpm, bool ramp)
{
    time = std::max(0.0, time); bpm = juce::jlimit(20.0, 400.0, bpm);
    auto it = std::lower_bound(points.begin(), points.end(), time, [](const auto& p, double t) { return p.time < t; });
    if (it != points.end() && std::abs(it->time - time) < 0.000001) { it->bpm = bpm; it->ramp = ramp; return (int) (it - points.begin()); }
    return (int) (points.insert(it, {time, bpm, ramp}) - points.begin());
}
int TempoMap::setTrackPoint(const juce::String& id, double time, double bpm, bool ramp)
{
    for (int i = 0; i < (int) points.size(); ++i) if (points[(size_t) i].clipId == id) return i;
    time = std::max(0.0, time); bpm = juce::jlimit(20.0, 400.0, bpm);
    auto it = std::upper_bound(points.begin(), points.end(), time, [](double t, const auto& p) { return t < p.time; });
    const int index = (int) std::distance(points.begin(), it); points.insert(it, {time, bpm, ramp, id}); return index;
}
int TempoMap::addTrackPointAtBeat(const juce::String& id, double beat, double bpm, bool ramp)
{
    for (int i = 0; i < (int) points.size(); ++i) if (points[(size_t) i].clipId == id) return i;
    // Solve the area under the incoming tempo segment. Adding its target must
    // not accidentally shift the track away from the requested musical overlap.
    double time = timeAtBeat(beat);
    for (size_t n = points.size(); n-- > 0;)
    {
        const auto& previous = points[n]; const double remaining = beat - beatsAt(previous.time);
        if (remaining < -1.0e-9) continue;
        const double candidate = previous.time + (ramp ? 120 / (previous.bpm + bpm) : 60 / previous.bpm) * std::max(0.0, remaining);
        if (n + 1 == points.size() || candidate < points[n + 1].time) { time = candidate; break; }
    }
    return setTrackPoint(id, time, bpm, ramp);
}
void TempoMap::detachMissingTracks(const std::vector<Clip>& clips)
{
    for (auto& p : points) if (p.clipId.isNotEmpty() && std::none_of(clips.begin(), clips.end(), [&](const auto& c) { return c.id == p.clipId; })) p.clipId.clear();
    // Ownership is independent of timing. Coincident points can terminate a
    // ramp at one BPM and begin the following segment at another. Collapsing
    // them changes the clock of every remaining clip before that timestamp.
}
void TempoMap::movePoint(int index, double time, double bpm)
{
    if (index < 0 || index >= (int) points.size()) return;
    const double low = index == 0 ? 0 : points[(size_t) index - 1].time + (points[(size_t) index - 1].clipId.isNotEmpty() || points[(size_t) index].clipId.isNotEmpty() ? 0 : 0.000001);
    const double high = index + 1 == (int) points.size() ? 1.0e9 : points[(size_t) index + 1].time - (points[(size_t) index + 1].clipId.isNotEmpty() || points[(size_t) index].clipId.isNotEmpty() ? 0 : 0.000001);
    if (index == 0) points[(size_t) index].time = 0;
    else if (low <= high) points[(size_t) index].time = juce::jlimit(low, high, time);
    points[(size_t) index].bpm = juce::jlimit(20.0, 400.0, bpm);
}
juce::String Media::libraryTitle() const
{
    const auto title = name.trim(); int digits = 0;
    while (digits < title.length() && juce::CharacterFunctions::isDigit(title[digits])) ++digits;
    // Preserve years and number-led titles such as "2 One 2" without a delimiter.
    if (digits == 0 || digits > 3 || digits == title.length()) return title;
    auto remainder = title.substring(digits); const auto first = remainder[0]; const auto trimmed = remainder.trimStart();
    if (trimmed.startsWithChar('.') || trimmed.startsWithChar('-') || trimmed.startsWithChar('_')) remainder = trimmed.substring(1).trimStart();
    else if (digits >= 2 && juce::CharacterFunctions::isWhitespace(first)) remainder = remainder.trimStart();
    else return title;
    return remainder.isEmpty() ? title : remainder;
}
bool Media::needsAnalysis() const
{
    return analysisVersion != analysisRevision || bpm() <= 0 || key() < 0 || downbeatOrigin() < 0
        || (manualBpm <= 0 && (analysis.variableTempo || analysis.tempoConfidence < 0.2))
        || (manualKey < 0 && analysis.keyConfidence < keyReviewConfidence) || analysis.rhythmIssue.isNotEmpty();
}
namespace
{
double loopPosition(double time, double span)
{
    double wrapped = std::fmod(time, span);
    if (wrapped < 0) wrapped += span;
    // Equivalent source times reached through split arithmetic can straddle zero
    // by a few floating-point ulps. Canonicalise only that numerical boundary.
    const double tolerance = 32 * std::numeric_limits<double>::epsilon() * std::max(1.0, std::abs(time));
    if (std::abs(wrapped) <= tolerance || std::abs(span - wrapped) <= tolerance) return 0;
    return wrapped;
}
}
Project::Project()
{
    for (int i = 0; i < trackCount; ++i) tracks[(size_t) i].name = "Audio " + juce::String(i + 1);
}
double Project::duration() const
{
    double end = 0;
    for (const auto& c : clips) end = std::max(end, c.end());
    return end;
}
bool Project::hasPendingPlacement() const
{
    return std::any_of(clips.begin(), clips.end(), [](const auto& c)
    { return c.pendingPlacement == PendingPlacement::append || c.pendingPlacement == PendingPlacement::drop || c.pendingPlacement == PendingPlacement::move; });
}
double Project::snappedTime(double seconds, bool enabled) const
{
    return gridClock().snap(seconds, enabled, snapStep());
}
double Project::snappedMoveTime(double reference, double seconds, bool enabled) const
{
    if (!enabled) return std::max(0., seconds);
    const auto clock = gridClock(); const double from = clock.beatsAt(reference), step = snapStep();
    // Snap displacement, not the existing phase. Insertion establishes a grid;
    // moving an already placed song must retain any manual alignment correction.
    const double minimum = std::ceil((clock.beatsAt(0) - from) / step - 1e-9);
    const double count = std::max(minimum, std::round((clock.beatsAt(std::max(0., seconds)) - from) / step));
    return clock.timeAtBeat(from + count * step);
}
double Project::snappedClipStart(const Clip& clip, double seconds, bool enabled, double magnetSeconds) const
{
    if (!enabled) return std::max(0.0, seconds);
    seconds = std::max(0.0, seconds);
    const auto grid = gridClock();
    const double sourceBpm = clip.tempoSourceBpm > 0 ? clip.tempoSourceBpm : clip.media->bpm();
    const bool mapped = followTrackTempo && clip.beatmatched && sourceBpm > 0;
    const double phase = mapped ? clip.sourceElapsed(clip.firstBeatTime()) * sourceBpm / 60 : clip.firstBeatTime();
    const double desired = mapped ? grid.beatsAt(seconds) + phase : grid.beatsAt(seconds + phase);
    const double minimum = mapped ? grid.beatsAt(0) + phase : grid.beatsAt(phase);
    // Clamping a negative *clip edge* to zero would leave its first beat off grid.
    const double step = snapStep();
    const double beat = std::max(std::ceil(minimum / step - 1.0e-9), std::round(desired / step)) * step;
    double placed = mapped ? grid.timeAtBeat(beat - phase) : grid.timeAtBeat(beat) - phase;
    placed = std::max(0.0, placed);
    // A deliberately free-positioned neighbour may supply a closer visible pulse.
    // Normal matched neighbours coincide with the background grid already.
    for (const auto& other : clips)
    {
        if (magnetSeconds <= 0 || other.id == clip.id || other.media->bpm() <= 0) continue;
        const double anchor = mapped ? grid.timeAtBeat(desired) : seconds + phase;
        if (anchor < other.start || anchor >= other.end()) continue;
        const double period = 60 / other.media->bpm() * step, first = other.sourceElapsed(other.firstBeatTime());
        const double pulse = first + std::round((other.sourceElapsed(anchor - other.start) - first) / period) * period;
        const double target = other.start + other.localTimeForSource(pulse);
        if (pulse < 0 || target >= other.end()) continue;
        if (mapped && grid.beatsAt(target) < grid.beatsAt(0) + phase) continue;
        const double candidate = mapped ? grid.timeAtBeat(grid.beatsAt(target) - phase) : target - phase;
        if (candidate >= 0 && std::abs(candidate - seconds) < magnetSeconds && std::abs(candidate - seconds) < std::abs(placed - seconds)) placed = candidate;
    }
    return placed;
}
void Clip::adoptTimelineControls(const TrackSettings& settings)
{
    controls = settings;
    auto convert = [&](AutomationCurve& curve, float manual)
    {
        if (curve.points.empty()) return;
        curve = curve.sliced(start, end(), manual);
        for (auto& point : curve.points) point.time = sourceElapsed(point.time);
    };
    for (int n = 0; n < trackAutomationCount; ++n) convert(controls.automation[(size_t) n], settings.manualValue((AutomationTarget) n));
    for (auto& lane : controls.pluginAutomation) convert(lane.curve, lane.manualValue);
}
void Clip::moveTo(double seconds)
{
    if (!std::isfinite(seconds)) return;
    seconds = std::max(0.0, seconds);
    if (seconds == start) return;
    retime(seconds, tempoMap);
    if (pendingPlacement == PendingPlacement::append || pendingPlacement == PendingPlacement::drop) pendingPlacement = PendingPlacement::move;
}
void Clip::applyPlacement(const Clip& source)
{
    start = source.start; length = source.length; rate = source.rate;
    fadeIn = source.fadeIn; fadeOut = source.fadeOut; introAnchor = source.introAnchor; outroAnchor = source.outroAnchor;
    tempoMap = source.tempoMap; tempoSourceBpm = source.tempoSourceBpm; prepared = source.prepared; preparedOffset = source.preparedOffset;
    elasticPlayback = source.elasticPlayback; elasticSource = source.elasticSource; pendingPlacement = source.pendingPlacement;
}
juce::Result Clip::alignAnchorTo(bool intro, double seconds)
{
    if (!std::isfinite(seconds) || seconds < 0) return juce::Result::fail("The alignment time is invalid.");
    if (std::abs(seconds - start - transitionTime(intro)) < 1.0e-8) return juce::Result::ok();
    const double source = sourceElapsed(transitionTime(intro));
    const double sourceBpm = tempoSourceBpm > 0 ? tempoSourceBpm : media->bpm();
    double position;
    if (tempoMap && sourceBpm > 0)
    {
        const double beat = tempoMap->beatsAt(seconds) - source * sourceBpm / 60;
        if (beat < -1.0e-8) return juce::Result::fail("That move would put a linked track before the start of the mix.");
        position = tempoMap->timeAtBeat(std::max(tempoMap->beatsAt(0), beat));
    }
    else position = seconds - source / rate;
    if (!std::isfinite(position) || position < -1.0e-8) return juce::Result::fail("That move would put a linked track before the start of the mix.");
    moveTo(std::max(0.0, position));
    return juce::Result::ok();
}
void Clip::retime(double seconds, std::shared_ptr<const TempoMap> map)
{
    if (beatmatched && map && (tempoSourceBpm > 0 || media->bpm() > 0))
    {
        const auto fades = envelope();
        const double span = length * rate, intro = fades.intro, outro = fades.outro;
        const double inSource = sourceElapsed(intro), outSource = sourceElapsed(outro);
        const double in = sourceElapsed(intro + fadeIn) - inSource, out = sourceElapsed(outro + fadeOut) - outSource;
        const auto oldTempo = tempoMap ? tempoMap->localPoints(start, end()) : TempoMap {media->bpm() * rate, {}}.localPoints(0, length);
        start = seconds; tempoMap = std::move(map); if (tempoSourceBpm <= 0) tempoSourceBpm = media->bpm();
        length = localTimeForSource(span); rate = span / length;
        fadeIn = localTimeForSource(inSource + in) - localTimeForSource(inSource);
        fadeOut = localTimeForSource(outSource + out) - localTimeForSource(outSource);
        const auto newTempo = tempoMap->localPoints(start, end());
        bool same = oldTempo.size() == newTempo.size();
        for (size_t i = 0; same && i < oldTempo.size(); ++i)
            same = std::abs(oldTempo[i].time - newTempo[i].time) < 1.0e-8 && std::abs(oldTempo[i].bpm - newTempo[i].bpm) < 1.0e-8 && oldTempo[i].ramp == newTempo[i].ramp;
        if (!same || elasticPlayback)
        {
            elasticPlayback = true; prepared.reset(); preparedOffset = 0;
            elasticSource = std::make_shared<const ElasticSource>(*this);
        }
    }
    else start = seconds;
}

juce::Result Project::setClipAudioSlip(const juce::String& id, double seconds)
{
    auto* clip = clipFor(id);
    if (!clip || !std::isfinite(seconds) || std::abs(seconds) > 1.e9) return juce::Result::fail("Invalid track audio slip.");
    if (clipTransitionLocked(id)) return juce::Result::fail("Unlock the transition before slipping its track audio.");
    if (!clip->media || !clip->media->hasPlayback() || clip->media->sourceDetailsPending || clip->pendingPlacement != PendingPlacement::none)
        return juce::Result::fail("Wait for this track's audio before slipping it.");
    clip->audioSlip = seconds;
    return juce::Result::ok();
}

int Clip::gridPixel(double time, double scale) { return (int) std::llround(time * scale); }
double Clip::beatTimelineTime(double source) const
{
    const double time = start + audioTimelineTime(source);
    if (!beatmatched || !tempoMap || tempoSourceBpm <= 0) return time;
    const double beat = tempoMap->beatsAt(time), nearest = std::round(beat);
    return std::abs(beat - nearest) < 1.0e-7 ? tempoMap->timeAtBeat(nearest) : time;
}
double Clip::transitionTime(bool intro) const
{
    if (intro) return introAnchor ? localTimeForSource(*introAnchor) : 0;
    return outroAnchor ? localTimeForSource(*outroAnchor) : length;
}
double Clip::sourceTimeAt(double local) const
{
    double elapsed = audioSourceElapsed(local);
    if (loopSpan > 0) elapsed = loopPosition(loopPhase + elapsed, loopSpan);
    return offset + (reversed ? sourceSpan() - elapsed : elapsed);
}
double Clip::sourceElapsed(double local) const
{
    const double sourceBpm = tempoSourceBpm > 0 ? tempoSourceBpm : media->bpm();
    if (!tempoMap || sourceBpm <= 0) return local * rate;
    const double time = start + local;
    // Playback preroll can precede session zero. Extend the first held tempo;
    // the visible ruler and placement still clamp at zero.
    const double beat = time < 0 ? tempoMap->beatsAt(0) + time * tempoMap->bpmAt(0) / 60 : tempoMap->beatsAt(time);
    return (beat - tempoMap->beatsAt(start)) * 60 / sourceBpm;
}
double Clip::localTimeForSource(double source) const
{
    const double sourceBpm = tempoSourceBpm > 0 ? tempoSourceBpm : media->bpm();
    if (!tempoMap || sourceBpm <= 0) return source / rate;
    const double beat = tempoMap->beatsAt(start) + source * sourceBpm / 60;
    const double first = tempoMap->beatsAt(0);
    return (beat < first ? (beat - first) * 60 / tempoMap->bpmAt(0) : tempoMap->timeAtBeat(beat)) - start;
}
double Clip::maxLength() const
{
    const auto available = localTimeForSource(media->duration() - offset);
    return loopSpan > 0 ? std::max(length, available) : reversed ? localTimeForSource(length * rate + offset) : available;
}
double Clip::firstBeatTime() const
{
    if (media->bpm() <= 0) return 0;
    const double period = 60 / media->bpm();
    const double origin = (reversed ? offset + sourceSpan() - media->beatOrigin() : media->beatOrigin() - offset) - loopPhase;
    const double phase = origin + std::ceil((audioSourceElapsed(0) - origin) / period - 1.e-9) * period;
    return std::max(0.0, audioTimelineTime(phase));
}
double Media::downbeatOrigin() const
{
    return grid().downbeatOrigin;
}
double Clip::firstDownbeatTime() const
{
    if (media->bpm() <= 0 || media->downbeatOrigin() < 0) return -1;
    const double bar = 240 / media->bpm();
    if (loopSpan > 0 && std::abs(std::remainder(loopSpan, bar)) > 1.0e-6) return -1;
    const double origin = (reversed ? offset + sourceSpan() - media->downbeatOrigin() : media->downbeatOrigin() - offset) - loopPhase;
    const double phase = origin + std::ceil((audioSourceElapsed(0) - origin) / bar - 1.e-9) * bar;
    return std::max(0.0, audioTimelineTime(phase));
}
namespace
{
WaveformRange pcmRange(const float* const* data, int channels, int frames, double begin, double end)
{
    const int first = (int) std::floor(juce::jlimit(0.0, (double) frames, begin));
    const int last = (int) std::ceil(juce::jlimit(0.0, (double) frames, end));
    WaveformRange result; double energy = 0;
    for (int ch = 0; ch < channels; ++ch) for (int i = first; i < last; ++i)
    { const float value = data[ch][i]; result.peak = std::max(result.peak, std::abs(value)); energy += (double) value * value; }
    if (last > first && channels > 0) result.rms = (float) std::sqrt(energy / ((last - first) * (double) channels));
    return result;
}
}
void Media::prepareWaveformBody()
{
    if (!hasPlayback()) return;
    const int bucket = std::max(1, frames() / 4000);
    waveformRms.clear(); waveformRms.reserve((size_t) ((frames() + bucket - 1) / bucket));
    const float* data[2] {channelData(0), channels() > 1 ? channelData(1) : nullptr};
    for (int first = 0; first < frames(); first += bucket)
        waveformRms.push_back(pcmRange(data, channels(), frames(), first, std::min(first + bucket, frames())).rms);
}
WaveformRange Media::waveformRange(double begin, double end) const
{
    begin = juce::jlimit(0.0, duration(), begin); end = juce::jlimit(begin, duration(), end);
    if (end <= begin) return {};
    const int bucket = std::max(1, frames() / 4000);
    if (hasPlayback() && (peaks.empty() || (end - begin) * sampleRate <= bucket * 2))
    {
        const float* data[2] {channelData(0), channels() > 1 ? channelData(1) : nullptr};
        return pcmRange(data, channels(), frames(), begin * sampleRate, end * sampleRate);
    }
    if (peaks.empty()) return {};
    const int first = juce::jlimit(0, (int) peaks.size() - 1, (int) std::floor(begin * sampleRate / bucket));
    const int last = juce::jlimit(first + 1, (int) peaks.size(), (int) std::ceil(end * sampleRate / bucket));
    WaveformRange result { *std::max_element(peaks.begin() + first, peaks.begin() + last), 0 };
    double powerSum = 0, weight = 0;
    for (int i = first; i < last && i < (int) waveformRms.size(); ++i)
    {
        const double count = std::max(0.0, std::min(end * sampleRate, (double) std::min((i + 1) * bucket, frames())) - std::max(begin * sampleRate, (double) i * bucket));
        powerSum += waveformRms[(size_t) i] * (double) waveformRms[(size_t) i] * count; weight += count;
    }
    if (weight > 0) result.rms = (float) std::sqrt(powerSum / weight);
    return result;
}
WaveformRange Clip::waveformRange(double begin, double end) const
{
    begin = juce::jlimit(0.0, length, begin); end = juce::jlimit(begin, length, end);
    if (end <= begin) return {};
    // At beatmatching zoom, show the PCM that playback/export actually consumes.
    const bool detail = !elasticPlayback && prepared && (end - begin) * media->sampleRate <= std::max(1, media->frames() / 4000) * 2;
    auto preparedRange = [&](double a, double b)
    {
        const float* data[2] {prepared->channelData(0), prepared->channels() > 1 ? prepared->channelData(1) : nullptr};
        return pcmRange(data, prepared->channels(), prepared->frames(), a * media->sampleRate, b * media->sampleRate);
    };
    if (detail && prepared->clipLocal) return preparedRange(begin + preparedOffset - audioSlip, end + preparedOffset - audioSlip);
    const double from = audioSourceElapsed(begin), to = audioSourceElapsed(end);
    auto peak = [&](double a, double b)
    {
        const double low = offset + (reversed ? sourceSpan() - b : a), high = offset + (reversed ? sourceSpan() - a : b);
        return detail ? preparedRange(low / prepared->rate, high / prepared->rate) : media->waveformRange(low, high);
    };
    if (loopSpan <= 0) return peak(from, to);
    const double a = loopPosition(loopPhase + from, loopSpan), span = to - from;
    if (a + span <= loopSpan) return peak(a, a + span);
    WaveformRange result; double energy = 0;
    auto add = [&](WaveformRange value, double weight) { result.peak = std::max(result.peak, value.peak); energy += value.rms * (double) value.rms * weight; };
    const double tail = loopSpan - a, remaining = span - tail;
    add(peak(a, loopSpan), tail);
    const double cycles = std::floor(remaining / loopSpan), head = remaining - cycles * loopSpan;
    if (cycles > 0) add(peak(0, loopSpan), cycles * loopSpan);
    if (head > 0) add(peak(0, head), head);
    result.rms = (float) std::sqrt(energy / span); return result;
}
Clip Clip::slice(double begin, double duration) const
{
    Clip result = *this;
    result.awaitingSourceLength = false; result.pendingPredecessor.clear();
    const double sourceBegin = sourceElapsed(begin), sourceLength = sourceElapsed(begin + duration) - sourceBegin;
    result.effectOffset += sourceBegin;
    if (loopSpan > 0) result.loopPhase = loopPosition(loopPhase + sourceBegin, loopSpan);
    else result.offset += reversed ? length * rate - sourceBegin - sourceLength : sourceBegin;
    result.start += begin; result.length = duration;
    result.rate = sourceLength / duration;
    if (begin > 0 || duration < length)
    {
        result.introAnchor = sourceElapsed(transitionTime(true)) - sourceBegin;
        result.outroAnchor = sourceElapsed(transitionTime(false)) - sourceBegin;
        const auto fades = envelope();
        result.fadeInAnchor = sourceElapsed(fades.intro) - sourceBegin;
        result.fadeOutAnchor = sourceElapsed(fades.outro) - sourceBegin;
    }
    auto sliceCurve = [&](AutomationCurve& curve)
    {
        if (curve.points.empty()) return;
        curve = curve.sliced(sourceBegin, sourceBegin + sourceLength, 0);
    };
    for (auto& settings : result.stemControls) for (auto& curve : settings.automation) sliceCurve(curve);
    // Original-track curves keep their full clock across cuts and deleted gaps.
    // Only stems and source geometry are sliced.
    // Plugin automation follows the original track clock across every piece.
    if (prepared && prepared->clipLocal) result.preparedOffset += begin;
    return result;
}
ClipEnvelope Clip::envelope() const
{
    return {length, fadeInAnchor ? localTimeForSource(*fadeInAnchor) : 0,
            fadeOutAnchor ? localTimeForSource(*fadeOutAnchor) : length - fadeOut, fadeIn, fadeOut};
}
float ClipEnvelope::gainAt(double t) const
{
    if (t < 0 || t >= length) return 0;
    double gain = 1;
    if (t < intro) return 0;
    if (fadeIn > 0) gain = std::min(gain, (t - intro) / fadeIn);
    if (fadeOut > 0) gain = std::min(gain, (outro + fadeOut - t) / fadeOut);
    else if (t >= outro) return 0;
    return (float) juce::jlimit(0.0, 1.0, gain);
}
TrackAnalysisRecord Media::analysisRecord() const { return {analysisSource, analysis, peaks, manualBpm, manualBeatOrigin, manualKey, waveformRms, analysisVersion, energy}; }
std::shared_ptr<Media> Media::catalogueCopy() const
{
    auto result = std::make_shared<Media>(); result->id = id; result->name = name; result->file = file; result->assets = assets;
    result->sampleRate = sampleRate; result->analysis = analysis; result->analysisVersion = analysisVersion; result->analysisSource = analysisSource; result->sourceModified = sourceModified;
    result->analysisFromCache = analysisFromCache; result->analysisCacheWarning = analysisCacheWarning;
    result->manualBpm = manualBpm; result->manualBeatOrigin = manualBeatOrigin; result->manualKey = manualKey; result->peaks = peaks;
    result->waveformRms = waveformRms;
    result->energy = energy;
    result->sectionTemplate = sectionTemplate; result->sectionTemplateError = sectionTemplateError;
    result->sourceDetailsPending = sourceDetailsPending;
    result->sessionMetadataPending = sessionMetadataPending; result->playbackError = playbackError;
    result->analysisFileModified = analysisFileModified; result->analysisFileBytes = analysisFileBytes;
    return result;
}
MediaResult readMedia(juce::AudioFormatManager& formats, const juce::File& file, const MediaProgress& progress, AnalysisCacheMode mode, MediaReadPurpose purpose, const TrackAnalysisRecord* libraryRecord, const std::shared_ptr<std::atomic<bool>>& cancelled)
{
    const auto stopped = [&] { return cancelled && cancelled->load(); };
    if (stopped()) return {{}, "Audio loading cancelled."};
    if (!file.existsAsFile()) return {{}, "Missing audio file: " + file.getFullPathName()};
    if (progress) progress(MediaReadStage::lookupAnalysis);
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (!reader)
    {
        bool supported = false;
        for (int i = 0; i < formats.getNumKnownFormats(); ++i) supported |= formats.getKnownFormat(i)->canHandleFile(file);
        return {{}, juce::String(supported ? "Unreadable or corrupt audio: " : "Unsupported audio format: ")
            + file.getFullPathName() + ". Try WAV, AIFF, FLAC, Ogg or MP3."};
    }
    const int channels = (int) std::min(reader->numChannels, 2u);
    if (channels < 1 || reader->sampleRate <= 0 || !std::isfinite(reader->sampleRate) || reader->lengthInSamples < 1 || reader->lengthInSamples >= std::numeric_limits<int>::max())
        return { {}, "Unsupported audio dimensions: " + file.getFileName() };
    const auto bytes = reader->lengthInSamples * channels * (int64_t) sizeof(float);
    auto m = std::make_shared<Media>();
    m->id = juce::Uuid().toString(); m->file = file; m->name = file.getFileNameWithoutExtension(); m->sampleRate = reader->sampleRate;
    if (progress) progress(MediaReadStage::lookupAnalysis);
    m->analysisSource = {audioDigest(file), file.getSize(), m->sampleRate, (int) reader->lengthInSamples, channels};
    m->sourceModified = file.getLastModificationTime().toMilliseconds();
    m->assets = readLibraryAssets(file);
    if (m->assets.title.isNotEmpty()) m->name = m->assets.title;
    auto cached = readAnalysisRecord(file, m->analysisSource);
    if (cached.record) { m->manualBpm = cached.record->manualBpm; m->manualBeatOrigin = cached.record->manualBeatOrigin; m->manualKey = cached.record->manualKey; }
    auto saved = mode == AnalysisCacheMode::refresh || !cached.reusableAutomatic ? std::optional<TrackAnalysisRecord>() : std::move(cached.record);
    const bool libraryMatches = libraryRecord && libraryRecord->source == m->analysisSource;
    if (libraryMatches && mode != AnalysisCacheMode::refresh)
    {
        if (!saved && libraryRecord->revision == analysisRevision && libraryRecord->analysis.rhythmIssue.isEmpty()
            && libraryRecord->analysis.rhythmMethod != "unavailable") saved = *libraryRecord;
        else if (saved && saved->waveformRms.empty()) saved->waveformRms = libraryRecord->waveformRms;
        if (saved && !saved->energy.complete() && libraryRecord->energy.complete()) saved->energy = libraryRecord->energy;
    }
    auto& store = PcmStore::shared(); const auto pcmKey = PcmStore::decodedKey(m->analysisSource);
    const bool playback = purpose == MediaReadPurpose::playback;
    const bool energyUpgrade = purpose == MediaReadPurpose::energyAnalysis && saved && !saved->energy.complete();
    const bool focused = mode == AnalysisCacheMode::focused;
    const auto* baseline = saved ? &saved->analysis : nullptr;
    const double protectedBpm = libraryMatches ? libraryRecord->manualBpm : m->manualBpm;
    const double protectedOrigin = libraryMatches ? libraryRecord->manualBeatOrigin : m->manualBeatOrigin;
    const int protectedKey = libraryMatches ? libraryRecord->manualKey : m->manualKey;
    // A manually reviewed bar anchor also protects its automatic pulse/BPM.
    const bool retryTempo = focused && protectedBpm <= 0 && protectedOrigin < 0 && (!baseline || needsFocusedTempo(*baseline));
    const bool retryKey = focused && protectedKey < 0 && (!baseline || needsFocusedKey(*baseline));
    const bool needsPcm = playback || !saved || energyUpgrade || retryTempo || retryKey;
    if (needsPcm) m->cache = store.read(pcmKey, m->sampleRate, channels, (int) reader->lengthInSamples, mode != AnalysisCacheMode::readOnly);
    m->decodedFromCache = m->cache != nullptr;
    if (m->cache && progress) progress(MediaReadStage::savedAudio);
    if (stopped()) return {{}, "Audio loading cancelled."};
    if (!m->cache && needsPcm)
    {
        if (progress) progress(MediaReadStage::decode);
        if (bytes > maxDecodedBytes)
        {
            juce::String error;
            m->cache = AudioCache::decode(*reader, channels, (int) reader->lengthInSamples, error, cancelled);
            if (!m->cache) return {{}, file.getFileName() + ": " + error};
            m->playbackCacheWarning = "Large source uses temporary mapped PCM; reopening decodes it again.";
        }
        else
        {
            m->samples.setSize(channels, (int) reader->lengthInSamples);
            if (!reader->read(&m->samples, 0, m->samples.getNumSamples(), 0, true, true)) return { {}, "Audio read failed: " + file.getFullPathName() };
        }
    }
    if (stopped()) return {{}, "Audio loading cancelled."};
    if (progress) progress(MediaReadStage::waveform);
    const int frames = (int) reader->lengthInSamples, bucketSize = std::max(1, frames / 4000);
    for (int pos = 0; !saved && pos < frames; pos += bucketSize)
    {
        if (stopped()) return {{}, "Audio loading cancelled."};
        float peak = 0;
        for (int ch = 0; ch < channels; ++ch)
        {
            const auto* samples = m->channelData(ch);
            for (int i = pos; i < std::min(pos + bucketSize, frames); ++i)
            {
                if (!std::isfinite(samples[i])) return { {}, "Non-finite audio sample in " + file.getFileName() };
                if (!saved) peak = std::max(peak, std::abs(samples[i]));
            }
        }
        if (!saved) m->peaks.push_back(peak);
    }
    // Cold PCM still needs validation even when only the saved analysis was reused.
    if (saved && !m->cache && needsPcm) for (int ch = 0; ch < channels; ++ch) for (int i = 0; i < frames; ++i)
        if (!std::isfinite(m->samples.getSample(ch, i))) return { {}, "Non-finite audio sample in " + file.getFileName() };
    if (saved)
    {
        if (progress) progress(MediaReadStage::savedAnalysis);
        m->analysis = std::move(saved->analysis); m->peaks = std::move(saved->peaks);
        m->waveformRms = std::move(saved->waveformRms);
        m->energy = std::move(saved->energy);
        m->manualBpm = saved->manualBpm; m->manualBeatOrigin = saved->manualBeatOrigin; m->manualKey = saved->manualKey; m->analysisFromCache = true;
    }
    else { if (progress) progress(MediaReadStage::analyse); m->analysis = analyseMusic(m->audioView(), m->sampleRate, cancelled); }
    if (libraryMatches) { m->manualBpm = libraryRecord->manualBpm; m->manualBeatOrigin = libraryRecord->manualBeatOrigin; m->manualKey = libraryRecord->manualKey; }
    if (retryTempo || retryKey)
    {
        if (progress) progress(MediaReadStage::analyse);
        m->analysis = retryMusicAnalysis(m->audioView(), m->sampleRate, m->analysis, retryTempo, retryKey, cancelled);
        m->analysisFromCache = false;
    }
    const bool refinedTiming = playback && needsPcm && refineAttackTiming(m->audioView(), m->sampleRate, m->analysis, m->manualBpm, m->manualBeatOrigin, cancelled);
    const bool newEnergy = needsPcm && !m->energy.complete();
    if (newEnergy)
    {
        if (progress) progress(MediaReadStage::energy);
        m->energy = analyseMusicalEnergy(m->audioView(), m->sampleRate, cancelled);
    }
    if (stopped()) return {{}, "Audio loading cancelled."};
    if (m->analysisSource.sha256.isNotEmpty() && audioDigest(file) != m->analysisSource.sha256) return { {}, "Audio changed while being read: " + file.getFullPathName() + ". Import it again." };
    if (needsPcm && m->waveformRms.empty()) m->prepareWaveformBody();
    if (m->analysis.rhythmIssue.isNotEmpty() && !focused) m->analysisCacheWarning = m->analysis.rhythmIssue;
    else if (mode != AnalysisCacheMode::readOnly && (!saved || retryTempo || retryKey || refinedTiming || cached.legacyLocation || (newEnergy && m->energy.complete()))) m->analysisCacheWarning = writeAnalysisRecord(file, m->analysisRecord());
    if (progress) progress(MediaReadStage::cache);
    if (!m->cache && needsPcm && mode != AnalysisCacheMode::readOnly) m->cache = store.write(pcmKey, m->sampleRate, m->samples, m->playbackCacheWarning);
    juce::String error; if (playback && !m->cache) m->cache = AudioCache::create(m->samples, error);
    if (playback && !m->cache) return { {}, file.getFileName() + ": " + error };
    m->samples.setSize(0, 0); // The OS pages retained PCM from disk; library size no longer reserves full decoded RAM.
    if (!playback) m->cache.reset(); // Catalogue entries retain facts/waveforms, not pinned PCM or temporary playback files.
    const auto recordFile = analysisRecordInputFile(file); m->analysisFileModified = recordFile.getLastModificationTime().toMilliseconds(); m->analysisFileBytes = recordFile.getSize();
    const auto sections = readSourceSectionRecord(m->analysisSource); m->sectionTemplate = sections.markers; m->sectionTemplateError = sections.error;
    return { m, {} };
}
double snapTime(double value, double bpm, bool enabled)
{
    value = std::max(0.0, value);
    return enabled ? std::round(value * bpm / 60.0) * 60.0 / bpm : value;
}
juce::String timeText(double value)
{
    const auto ms = (int64_t) std::llround(std::abs(value) * 1000);
    return juce::String(ms > 0 && value < 0 ? "-" : "") + juce::String::formatted("%02d:%02d.%03d", (int) (ms / 60000), (int) ((ms / 1000) % 60), (int) (ms % 1000));
}
juce::String midiActionId(MidiAction a)
{
    switch (a.kind)
    {
        case MidiActionKind::cutoff: return "cutoff-" + juce::String(a.index + 1);
        case MidiActionKind::deskPot: return "desk-pot-" + juce::String(a.index + 1);
        case MidiActionKind::cyclePot: return "desk-lane-" + juce::String(a.index + 1);
        case MidiActionKind::resetCutoff: return "reset-filter-" + juce::String(a.index + 1);
        case MidiActionKind::resetPans: return "reset-pans";
        case MidiActionKind::play: return "play";
        case MidiActionKind::start: return "start";
        case MidiActionKind::stop: return "stop";
        case MidiActionKind::home: return "home";
        case MidiActionKind::master: return "master";
        case MidiActionKind::volume: return "volume-" + juce::String(a.index + 1);
        case MidiActionKind::pan: return "pan-" + juce::String(a.index + 1);
        case MidiActionKind::mute: return "mute-" + juce::String(a.index + 1);
        case MidiActionKind::select: return "select-" + juce::String(a.index + 1);
        case MidiActionKind::pluginParameter: return "plugin-parameter-" + juce::String(a.index + 1);
        case MidiActionKind::bass: return "bass-" + juce::String(a.index + 1);
        case MidiActionKind::mid: return "mid-" + juce::String(a.index + 1);
        case MidiActionKind::treble: return "treble-" + juce::String(a.index + 1);
        case MidiActionKind::solo: return "solo-" + juce::String(a.index + 1);
        case MidiActionKind::record: return "record-take";
        case MidiActionKind::read: return "automation-read";
        case MidiActionKind::arm: return "arm-selected";
        case MidiActionKind::previousTrack: return "previous-lane";
        case MidiActionKind::nextTrack: return "next-lane";
        case MidiActionKind::previousTransition: return "previous-transition";
        case MidiActionKind::nextTransition: return "next-transition";
        case MidiActionKind::undo: return "undo";
        case MidiActionKind::redo: return "redo";
        case MidiActionKind::snap: return "snap";
        case MidiActionKind::panic: return "panic";
        case MidiActionKind::armTrack: return "arm-lane-" + juce::String(a.index + 1);
        case MidiActionKind::armMaster: return "arm-master";
        case MidiActionKind::resetEq: return "reset-eq-" + juce::String(a.index + 1);
    }
    return {};
}
std::vector<MidiAction> midiActions()
{
    std::vector<MidiAction> result;
    for (auto kind : {MidiActionKind::play, MidiActionKind::start, MidiActionKind::stop, MidiActionKind::home, MidiActionKind::master,
        MidiActionKind::record, MidiActionKind::read, MidiActionKind::arm, MidiActionKind::previousTrack, MidiActionKind::nextTrack,
        MidiActionKind::previousTransition, MidiActionKind::nextTransition, MidiActionKind::undo, MidiActionKind::redo, MidiActionKind::snap, MidiActionKind::panic, MidiActionKind::armMaster, MidiActionKind::resetPans}) result.push_back({kind});
    for (int i = 0; i < trackCount; ++i)
        for (auto kind : { MidiActionKind::volume, MidiActionKind::pan, MidiActionKind::mute, MidiActionKind::select, MidiActionKind::solo, MidiActionKind::bass, MidiActionKind::mid, MidiActionKind::treble, MidiActionKind::armTrack, MidiActionKind::resetEq, MidiActionKind::cutoff, MidiActionKind::resetCutoff }) result.push_back({kind, i});
    for (int i = 0; i < 8; ++i) { result.push_back({MidiActionKind::pluginParameter, i}); result.push_back({MidiActionKind::deskPot, i}); result.push_back({MidiActionKind::cyclePot, i}); }
    return result;
}
juce::Optional<MidiAction> parseMidiAction(const juce::String& id)
{
    // Old profiles continue to address the same lane; new saves use the current title.
    for (int lane = 0; lane < trackCount; ++lane)
        if (id == "low-pass-" + juce::String(lane + 1)) return MidiAction {MidiActionKind::cutoff, lane};
    for (auto action : midiActions()) if (midiActionId(action) == id) return action;
    return {};
}
float lowPassToCutoff(float hz) { return (hz - 20000.f) / 19980.f; }
float cutoffFromPosition(double position)
{
    position = juce::jlimit(-1., 1., position);
    return position < 0 ? (float) ((20000 * std::pow(1000., position) - 20000) / 19980)
        : (float) ((20 * std::pow(1000., position) - 20) / 19980);
}
double cutoffPosition(float value)
{
    value = juce::jlimit(-1.f, 1.f, value);
    return value < 0 ? std::log((20000 + (double) value * 19980) / 20000) / std::log(1000.)
        : std::log((20 + (double) value * 19980) / 20) / std::log(1000.);
}
float cutoffFromMidi(float value)
{
    const double cc = juce::jlimit(0., 127., (double) juce::roundToInt(value * 127));
    return cutoffFromPosition(cc < 63 ? (cc - 63) / 63 : cc > 64 ? (cc - 64) / 63 : 0);
}
juce::String cutoffText(float value, bool compact)
{
    value = juce::jlimit(-1.f, 1.f, value);
    if (value == 0) return "Off";
    const double hz = value < 0 ? 20000 + (double) value * 19980 : 20 + (double) value * 19980;
    auto frequency = compact ? hz >= 1000 ? juce::String(hz / 1000, 1) + "k" : juce::String(hz, 0)
        : juce::String(hz, 3).trimCharactersAtEnd("0").trimCharactersAtEnd(".");
    return juce::String(value < 0 ? "LP " : "HP ") + frequency + (compact ? "" : " Hz");
}
bool parseCutoffText(const juce::String& text, float& value)
{
    auto raw = text.trim().toUpperCase();
    if (raw == "OFF" || raw == "C" || raw == "0") { value = 0; return true; }
    const bool low = raw.startsWith("LP"), high = raw.startsWith("HP");
    if (!low && !high) return false;
    raw = raw.substring(2).trim(); if (raw.endsWith("HZ")) raw = raw.dropLastCharacters(2).trim();
    const bool kilo = raw.endsWith("K"); if (kilo) raw = raw.dropLastCharacters(1).trim();
    const auto number = raw.toStdString(); char* end = nullptr;
    const double hz = std::strtod(number.c_str(), &end) * (kilo ? 1000 : 1);
    if (number.empty() || end != number.c_str() + number.size() || !std::isfinite(hz) || hz < 20 || hz > 20000) return false;
    value = (float) (low ? (hz - 20000) / 19980 : (hz - 20) / 19980); return true;
}
const char* automationId(AutomationTarget target)
{
    switch (target) { case AutomationTarget::volume: return "volume"; case AutomationTarget::bass: return "bass"; case AutomationTarget::mid: return "mid"; case AutomationTarget::treble: return "treble"; case AutomationTarget::cutoff: return "cutoff"; case AutomationTarget::pan: return "pan"; case AutomationTarget::master: return "master"; case AutomationTarget::plugin: return "plugin"; }
    jassertfalse; return "volume";
}
const char* automationName(AutomationTarget target)
{
    switch (target) { case AutomationTarget::volume: return "Volume"; case AutomationTarget::bass: return "Bass / 120 Hz"; case AutomationTarget::mid: return "Mid / 1 kHz"; case AutomationTarget::treble: return "Treble / 8 kHz"; case AutomationTarget::cutoff: return "Cutoff / LP - Off - HP"; case AutomationTarget::pan: return "Pan"; case AutomationTarget::master: return "Master volume"; case AutomationTarget::plugin: return "VST parameter"; }
    jassertfalse; return "Volume";
}
double automationProgress(double fraction, double bend, bool exponential)
{
    const double t = juce::jlimit(0.0, 1.0, fraction);
    if (bend == 0) return t;
    if (bend >= 1) return t >= 1 ? 1 : 0;
    if (exponential) return std::abs(bend) < 1.e-12 ? t : std::expm1(8 * bend * t) / std::expm1(8 * bend);
    const double k = std::exp(8 * bend);
    return t / (k + (1 - k) * t);
}
namespace
{
double slicedBend(double bend, double begin, double end, bool exponential)
{
    if (bend >= 1) return end >= 1 ? 1 : 0;
    if (exponential) return bend * (end - begin);
    const double k = std::exp(8 * bend);
    return std::log((k + (1 - k) * begin) / (k + (1 - k) * end)) / 8;
}
}
AutomationCurve AutomationCurve::sliced(double begin, double end, float fallback) const
{
    AutomationCurve result; result.enabled = enabled;
    if (points.empty() || end <= begin) return result;
    result.points.push_back({begin, interpolatedValue(begin, fallback)});
    for (const auto& point : points) if (point.time > begin && point.time < end) result.points.push_back(point);
    result.points.push_back({end, interpolatedValue(end, fallback)});
    for (size_t i = 0; i + 1 < result.points.size(); ++i)
    {
        auto& point = result.points[i];
        const auto next = std::upper_bound(points.begin(), points.end(), point.time, [](double t, const auto& p) { return t < p.time; });
        // Held values outside the original span still belong to its endpoint's
        // representation, including gain-space silence below the ordinary floor.
        point.gainSpace = (next == points.begin() ? *next : *std::prev(next)).gainSpace;
        if (next != points.begin() && next != points.end())
        {
            const auto& previous = *std::prev(next); const double span = next->time - previous.time;
            point.exponential = previous.exponential; point.gainSpace = previous.gainSpace;
            point.bend = slicedBend(previous.bend, (point.time - previous.time) / span, (result.points[i + 1].time - previous.time) / span, previous.exponential);
        }
    }
    // The last held endpoint also owns its below-floor gain representation.
    if (!result.points.empty())
    {
        const auto next = std::upper_bound(points.begin(),points.end(),end,[](double t,const auto& p) { return t < p.time; });
        result.points.back().gainSpace = (next == points.begin() ? *next : *std::prev(next)).gainSpace;
    }
    for (auto& point : result.points) point.time -= begin;
    return result;
}
float AutomationCurve::valueAt(double time, float manualValue) const
{
    if (!active() || time < 0) return manualValue; // Stopped transport uses manual controls for instrument audition.
    return interpolatedValue(time, manualValue);
}
float AutomationCurve::interpolatedValue(double time, float manualValue) const
{
    if (points.empty()) return manualValue;
    auto next = std::upper_bound(points.begin(), points.end(), time, [](double t, const auto& p) { return t < p.time; });
    if (next == points.begin()) return next->value;
    const auto& previous = *std::prev(next);
    if (next == points.end()) return previous.value;
    if (previous.bend >= 1) return previous.value;
    const double fraction = (time - previous.time) / (next->time - previous.time);
    const double progress = automationProgress(fraction,previous.bend,previous.exponential);
    if (previous.gainSpace)
    {
        const double a = previous.value <= -160 ? 0 : juce::Decibels::decibelsToGain((double) previous.value);
        const double b = next->value <= -160 ? 0 : juce::Decibels::decibelsToGain((double) next->value);
        return (float) juce::Decibels::gainToDecibels(a+(b-a)*progress,-160.);
    }
    return (float) (previous.value + progress * (next->value - previous.value));
}
int AutomationCurve::setPoint(double time, float value)
{
    if (!std::isfinite(time) || !std::isfinite(value)) return -1;
    time = juce::jlimit(0.0, 1.0e9, time);
    auto it = std::lower_bound(points.begin(), points.end(), time, [](const auto& p, double t) { return p.time < t; });
    const int index = (int) std::distance(points.begin(), it);
    if (it != points.end() && it->time == time) it->value = value == -60 && it->gainSpace ? -160 : value;
    else
    {
        double bend = 0; bool exponential = false, gainSpace = false;
        if (it != points.begin() && it != points.end())
        {
            auto& previous = *std::prev(it); const double fraction = (time - previous.time) / (it->time - previous.time);
            exponential = previous.exponential; gainSpace = previous.gainSpace;
            bend = slicedBend(previous.bend, fraction, 1, exponential); previous.bend = slicedBend(previous.bend, 0, fraction, exponential);
        }
        points.insert(it, {time, value == -60 && gainSpace ? -160 : value, bend, exponential, gainSpace});
    }
    return index;
}
void AutomationCurve::movePoint(int index, double time, float value)
{
    if (index < 0 || index >= (int) points.size() || !std::isfinite(time) || !std::isfinite(value)) return;
    // Precise point editing displays microseconds; keep a representable gap when
    // dragging against a neighbour instead of manufacturing adjacent doubles.
    const double low = index == 0 ? 0 : points[(size_t) index - 1].time + 0.000001;
    const double high = index + 1 == (int) points.size() ? 1.0e9 : points[(size_t) index + 1].time - 0.000001;
    auto& point = points[(size_t) index];
    point.time = low <= high ? juce::jlimit(low, high, time) : point.time; point.value = value == -60 && point.gainSpace ? -160 : value;
}
juce::Range<float> automationRange(AutomationTarget target)
{
    return target == AutomationTarget::cutoff ? juce::Range<float>(-1, 1) : target == AutomationTarget::plugin ? juce::Range<float>(0, 1) : target == AutomationTarget::pan ? juce::Range<float>(-1, 1) : juce::Range<float>(-60, 12);
}
juce::Result Project::closeTimeRange(double begin, double end, const juce::String& removeId)
{
    if (!std::isfinite(begin) || !std::isfinite(end) || begin < 0 || end > 1.e9 || end <= begin)
        return juce::Result::fail("Choose a non-empty time range inside the mix.");
    if (hasPendingPlacement()) return juce::Result::fail("Wait for segment preparation before closing time.");
    Project candidate = *this; candidate.ensureTransitionLinks();
    auto valid = candidate.validateTransitionLinks(); if (valid.failed()) return valid;
    const auto* removed = candidate.clipFor(removeId);
    if (removeId.isNotEmpty() && (!removed || removed->start != begin || removed->end() != end))
        return juce::Result::fail("The segment changed. Select it again before ripple deletion.");
    for (const auto& c : candidate.clips)
    {
        if (c.id == removeId) continue;
        if (c.start < end && c.end() > begin)
            return juce::Result::fail("Another segment overlaps that range. Split or move it before closing time across all lanes.");
        if (c.start >= end && (!c.media || !c.media->hasPlayback() || c.media->sourceDetailsPending))
            return juce::Result::fail("Wait for all later segments to load before closing time.");
        if (c.start >= end && c.elasticPlayback)
            return juce::Result::fail("Prepare the later elastic segments with Beatmatch before closing time.");
    }
    for (const auto& link : candidate.transitionLinks)
    {
        if (link.outgoing == removeId || link.incoming == removeId)
            return juce::Result::fail("This segment belongs to a linked pair. Close an empty gap outside the pair, or use ordinary Delete to leave its time in place.");
        const bool outMoves = candidate.clipFor(link.outgoing)->start >= end;
        const bool inMoves = candidate.clipFor(link.incoming)->start >= end;
        if (outMoves != inMoves)
            return juce::Result::fail("A linked pair crosses that range. Close a gap outside the complete pair.");
    }
    const auto fold = [=](double t) { return t < begin ? t : t < end ? begin : begin + (t - end); };
    const double guardTime = std::nextafter(begin, 0.0);
    const bool savedGuardValid = begin == 0 || juce::JSON::toString(guardTime, false, 17).getDoubleValue()
        < juce::JSON::toString(begin, false, 17).getDoubleValue();
    if (!savedGuardValid && (!masterAutomation.points.empty() || std::any_of(tracks.begin(), tracks.end(), [](const auto& settings)
        {
            return std::any_of(settings.automation.begin(), settings.automation.end(), [](const auto& curve) { return !curve.points.empty(); })
                || std::any_of(settings.pluginAutomation.begin(), settings.pluginAutomation.end(), [](const auto& lane) { return !lane.curve.points.empty(); });
        }))) return juce::Result::fail("That time is too small to preserve saved automation at the splice. Choose a later boundary.");
    // Keep the left limit and right value at the seam as separate points. The
    // incoming ramp flag belongs to the left endpoint, not to the new jump.
    const auto spliceTempo = [=](const TempoMap& original)
    {
        auto result = original; result.origin = fold(original.origin); result.points.clear();
        if (original.points.empty()) return result;
        for (const auto& p : original.points) if (p.time < begin) result.points.push_back(p);
        const auto left = std::lower_bound(original.points.begin(), original.points.end(), begin, [](const auto& p, double t) { return p.time < t; });
        if (begin > 0) result.points.push_back({begin, left != original.points.end() && left->time == begin ? left->bpm : original.bpmAt(begin),
                                              left != original.points.end() && left->ramp});
        const auto right = std::lower_bound(original.points.begin(), original.points.end(), end, [](const auto& p, double t) { return p.time < t; });
        if (right == original.points.end() || right->time != end)
            result.points.push_back({begin, original.bpmAt(end), false});
        for (auto p = right; p != original.points.end(); ++p)
        { auto shifted = *p; shifted.time = fold(p->time); if (p == right && p->time == end) shifted.ramp = false; result.points.push_back(shifted); }
        return result;
    };
    candidate.tempo = spliceTempo(tempo);
    const auto map = std::make_shared<const TempoMap>(candidate.tempo);
    for (auto& c : candidate.clips)
    {
        if (c.id == removeId) continue;
        const auto original = c;
        if (c.start >= end) c.start = fold(c.start);
        if (c.tempoMap)
        {
            // Saved sessions reconstruct from the project map. Refuse stale
            // captured clocks rather than quietly retiming surviving audio.
            const auto a = original.tempoMap->localPoints(original.start, original.end());
            const auto b = map->localPoints(c.start, c.end());
            auto agrees = [&](double local)
            {
                const double oldTime = local == c.length ? std::nextafter(original.end(), original.start) : original.start + local;
                const double newTime = local == c.length ? std::nextafter(c.end(), c.start) : c.start + local;
                return std::abs(original.tempoMap->bpmAt(oldTime) - map->bpmAt(newTime)) < 1.e-7;
            };
            bool same = agrees(0) && agrees(c.length);
            for (const auto& p : a) same = same && agrees(p.time);
            for (const auto& p : b) same = same && agrees(p.time);
            if (!same) return juce::Result::fail("A retained source clock differs from the project. Finish Beatmatch before closing time.");
            // Earlier clips keep their exact immutable mapping and prepared PCM.
            // Later clips receive the translated clock without rendering audio.
            if (original.start >= end) c.tempoMap = map;
            const auto beforeFade = original.envelope(), afterFade = c.envelope();
            if (std::abs(beforeFade.intro - afterFade.intro) > 1.e-7 || std::abs(beforeFade.outro - afterFade.outro) > 1.e-7
                || std::abs(original.transitionTime(true) - c.transitionTime(true)) > 1.e-7
                || std::abs(original.transitionTime(false) - c.transitionTime(false)) > 1.e-7)
                return juce::Result::fail("A source anchor crosses an incompatible captured clock. Finish Beatmatch before closing time.");
        }
    }
    candidate.clips.erase(std::remove_if(candidate.clips.begin(), candidate.clips.end(), [&](const auto& c) { return c.id == removeId; }), candidate.clips.end());
    candidate.tempo.detachMissingTracks(candidate.clips);
    candidate.labels.erase(std::remove_if(candidate.labels.begin(), candidate.labels.end(), [=](const auto& l) { return l.time >= begin && l.time < end; }), candidate.labels.end());
    for (auto& label : candidate.labels) label.time = fold(label.time);
    const auto spliceCurve = [=](AutomationCurve& curve)
    {
        if (curve.points.empty()) return;
        const auto original = curve; curve.points.clear();
        const auto left = std::lower_bound(original.points.begin(), original.points.end(), begin, [](const auto& p, double t) { return p.time < t; });
        if (begin > 0)
        {
            // Session30 requires strictly increasing automation timestamps.
            // The adjacent representable time leaves no floating-point query
            // between the left guard and seam. bend=1 holds up to the jump.
            for (auto p = original.points.begin(); p != left && p->time < guardTime; ++p) curve.points.push_back(*p);
            const auto guardNext = std::lower_bound(original.points.begin(), original.points.end(), guardTime, [](const auto& p, double t) { return p.time < t; });
            if (!curve.points.empty() && guardNext != original.points.end() && guardNext->time > guardTime)
            {
                auto& last = curve.points.back();
                last.bend = slicedBend(last.bend, 0, (guardTime - last.time) / (guardNext->time - last.time), last.exponential);
            }
            curve.points.push_back({guardTime, original.interpolatedValue(guardTime, 0), 1, false,
                (left == original.points.begin() ? *left : *std::prev(left)).gainSpace});
        }
        const auto right = std::lower_bound(original.points.begin(), original.points.end(), end, [](const auto& p, double t) { return p.time < t; });
        if (right == original.points.end() || right->time != end)
        {
            double bend = 0; bool exponential = false;
            const bool gainSpace = (right == original.points.begin() ? *right : *std::prev(right)).gainSpace;
            if (right != original.points.begin() && right != original.points.end())
            {
                const auto& previous = *std::prev(right);
                exponential = previous.exponential;
                bend = slicedBend(previous.bend, (end - previous.time) / (right->time - previous.time), 1, exponential);
            }
            curve.points.push_back({begin, original.interpolatedValue(end, 0), bend, exponential, gainSpace});
        }
        for (auto p = right; p != original.points.end(); ++p)
        { auto shifted = *p; shifted.time = fold(p->time); curve.points.push_back(shifted); }
    };
    spliceCurve(candidate.masterAutomation);
    for (auto& settings : candidate.tracks)
    {
        for (auto& curve : settings.automation) spliceCurve(curve);
        for (auto& lane : settings.pluginAutomation) spliceCurve(lane.curve);
    }
    valid = candidate.validateTransitionLinks(); if (valid.failed()) return valid;
    valid = validateTransitionLocks(candidate); if (valid.failed()) return valid;
    *this = std::move(candidate); return juce::Result::ok();
}
Clip* Project::clipFor(const juce::String& id)
{
    for (auto& c : clips) if (c.id == id) return &c;
    return nullptr;
}
const Clip* Project::clipFor(const juce::String& id) const { return const_cast<Project*>(this)->clipFor(id); }
TrackSettings& Project::effectSettings(const Clip& clip)
{
    return effectControls.try_emplace(effectsIdentity(clip.effects), effectOwner(clip)->controls).first->second;
}
const TrackSettings& Project::effectSettings(const Clip& clip) const
{
    const auto found = effectControls.find(effectsIdentity(clip.effects));
    return found == effectControls.end() ? effectOwner(clip)->controls : found->second;
}
const Clip* Project::effectOwner(const Clip& clip) const
{
    for (const auto& other : clips) if (other.effects == clip.effects) return &other;
    return &clip;
}
double Project::audibleEnd() const
{
    double end = duration() + effectTailSeconds;
    for (const auto& clip : clips) end = std::max(end, clip.end() + naturalEffectTail(clip.effects));
    return end;
}
juce::Range<double> Project::automationBounds(const AutomationAddress& address) const
{
    const auto* clip = clipFor(address.clipId);
    if (!clip) return {0, audibleEnd()};
    double begin = clip->start, end = clip->end();
    if (address.stem < 0) for (const auto& other : clips) if (other.effects == clip->effects)
    { begin = std::min(begin, other.start); end = std::max(end, other.end() + std::max(effectTailSeconds, naturalEffectTail(other.effects))); }
    return {begin, end};
}
TrackSettings* Project::settings(const AutomationAddress& address)
{
    if (address.stem >= 0)
    {
        auto* c = clipFor(address.clipId);
        return c && (c->stems || c->stemDirectory != juce::File()) && address.stem < stemCount
            && (address.target == AutomationTarget::volume || address.target == AutomationTarget::pan) ? &c->stemControls[(size_t) address.stem] : nullptr;
    }
    if (address.clipId.isNotEmpty()) { auto* c = clipFor(address.clipId); return c ? &effectSettings(*c) : nullptr; }
    return address.track >= 0 && address.track < trackCount ? &tracks[(size_t) address.track] : nullptr;
}
const TrackSettings* Project::settings(const AutomationAddress& address) const
{
    if (address.stem < 0 && address.clipId.isNotEmpty())
    { const auto* c = clipFor(address.clipId); return c ? &effectSettings(*c) : nullptr; }
    return const_cast<Project*>(this)->settings(address);
}
double Project::automationTime(const AutomationAddress& address, double time) const
{
    const auto* c = clipFor(address.clipId);
    if (c && address.stem < 0) { c = effectOwner(*c); return c->effectOffset + c->sourceElapsed(time - c->start); }
    return c ? c->sourceElapsed(time - c->start) : time;
}
double Project::automationSessionTime(const AutomationAddress& address, double time) const
{
    const auto* c = clipFor(address.clipId);
    if (c && address.stem < 0) { c = effectOwner(*c); return c->start + c->localTimeForSource(time - c->effectOffset); }
    return c ? c->start + c->localTimeForSource(time) : time;
}
void Project::automationEdited(const AutomationAddress& address, AutomationEdit edit, double removedSourceTime)
{
    if (address.stem >= 0) return;
    if (address.target == AutomationTarget::bass) if (auto* c = clipFor(address.clipId)) c->automaticBass = false;
    if (address.target == AutomationTarget::bass && edit != AutomationEdit::change) for (auto& link : transitionLinks)
        if (link.bassSwap && (link.outgoing == address.clipId || link.incoming == address.clipId))
        {
            const size_t side = link.outgoing == address.clipId ? 0 : 2;
            if (edit == AutomationEdit::clearCurve || std::abs((*link.bassSwap)[side] - removedSourceTime) < 1e-7
                || std::abs((*link.bassSwap)[side + 1] - removedSourceTime) < 1e-7) link.bassSwap.reset();
        }
}
AutomationCurve* Project::curve(const AutomationAddress& address)
{
    if (address.target == AutomationTarget::master) return &masterAutomation;
    auto* t = settings(address); if (!t) return nullptr;
    if (address.target != AutomationTarget::plugin) return &t->automation[(size_t) address.target];
    for (auto& lane : t->pluginAutomation) if (lane.slotId == address.slotId && lane.parameterId == address.parameterId) return &lane.curve;
    return nullptr;
}
const AutomationCurve* Project::curve(const AutomationAddress& address) const
{
    if (address.target == AutomationTarget::master) return &masterAutomation;
    const auto* t = settings(address); if (!t) return nullptr;
    if (address.target != AutomationTarget::plugin) return &t->automation[(size_t) address.target];
    for (const auto& lane : t->pluginAutomation) if (lane.slotId == address.slotId && lane.parameterId == address.parameterId) return &lane.curve;
    return nullptr;
}
float Project::manualValue(const AutomationAddress& address) const
{
    if (address.target == AutomationTarget::master) return masterDb;
    const auto* t = settings(address); if (!t) return 0;
    if (address.target != AutomationTarget::plugin) return t->manualValue(address.target);
    for (const auto& lane : t->pluginAutomation) if (lane.slotId == address.slotId && lane.parameterId == address.parameterId) return lane.manualValue;
    return 0;
}
void Project::setManualValue(const AutomationAddress& address, float value)
{
    if (automationLocked(address)) return;
    value = automationRange(address.target).clipValue(value);
    if (address.target == AutomationTarget::master) { masterDb = value; return; }
    auto* t = settings(address); if (!t) return;
    switch (address.target)
    {
        case AutomationTarget::volume: t->gainDb = value; break;
        case AutomationTarget::pan: t->pan = value; break;
        case AutomationTarget::cutoff: t->filterCutoff = value; break;
        case AutomationTarget::bass: case AutomationTarget::mid: case AutomationTarget::treble: t->eqDb[(size_t) address.target - 1] = value; break;
        case AutomationTarget::plugin: for (auto& lane : t->pluginAutomation) if (lane.slotId == address.slotId && lane.parameterId == address.parameterId) lane.manualValue = value; break;
        default: break;
    }
}

namespace
{
juce::var curveToJson(const AutomationCurve& curve)
{
    auto* lane = new juce::DynamicObject(); juce::Array<juce::var> points; lane->setProperty("enabled", curve.enabled);
    for (const auto& point : curve.points) { auto* node = new juce::DynamicObject(); node->setProperty("time", point.time); node->setProperty("value", point.value); if (point.bend != 0) node->setProperty("bend", point.bend); if (point.exponential) node->setProperty("exponential", true); if (point.gainSpace) node->setProperty("gainSpace", true); points.add(juce::var(node)); }
    lane->setProperty("points", points); return juce::var(lane);
}
juce::var controlsToJson(const TrackSettings& t)
{
    auto* obj = new juce::DynamicObject(); obj->setProperty("gainDb", t.gainDb); obj->setProperty("pan", t.pan);
    juce::Array<juce::var> eq, controls, lanes;
    obj->setProperty("filterCutoff", t.filterCutoff);
    for (auto value : t.eqDb) eq.add(value); obj->setProperty("eqDb", eq);
    auto* automation = new juce::DynamicObject();
    for (int n = 0; n < trackAutomationCount; ++n) automation->setProperty(automationId((AutomationTarget) n), curveToJson(t.automation[(size_t) n]));
    obj->setProperty("automation", juce::var(automation));
    for (const auto& c : t.pluginControls) { auto* item = new juce::DynamicObject(); item->setProperty("insert", c.insert); item->setProperty("parameterId", c.parameterId); controls.add(juce::var(item)); }
    obj->setProperty("pluginControls", controls);
    for (const auto& lane : t.pluginAutomation)
    {
        auto* item = new juce::DynamicObject(); item->setProperty("slotId", lane.slotId); item->setProperty("parameterId", lane.parameterId);
        item->setProperty("name", lane.name); item->setProperty("manualValue", lane.manualValue); item->setProperty("curve", curveToJson(lane.curve)); lanes.add(juce::var(item));
    }
    obj->setProperty("pluginAutomation", lanes); return juce::var(obj);
}

}
juce::Result LabelMarker::validate() const
{
    if (id.isEmpty() || !std::isfinite(time) || time < 0 || time > 1.0e9
        || text.trim().isEmpty() || text.length() > 256 || text.containsAnyOf("\r\n\t"))
        return juce::Result::fail("Labels need a name of 1-256 characters on one line and a session time from 0 to 1000000000 seconds.");
    return juce::Result::ok();
}
juce::Result Project::putLabelMarker(const LabelMarker& marker)
{
    const auto validation = marker.validate(); if (validation.failed()) return validation;
    if (std::any_of(labels.begin(), labels.end(), [&](const auto& item) { return item.id != marker.id && item.time == marker.time; }))
        return juce::Result::fail("There is already a label at that time. Move or edit that label first.");
    const auto found = std::find_if(labels.begin(), labels.end(), [&](const auto& item) { return item.id == marker.id; });
    if (found == labels.end()) labels.push_back(marker); else *found = marker;
    std::stable_sort(labels.begin(), labels.end(), [](const auto& a, const auto& b) { return a.time < b.time; });
    return juce::Result::ok();
}
juce::Result Project::setEffectTailSeconds(double seconds)
{
    if (!std::isfinite(seconds) || seconds < 0 || seconds > 1.0e9 - duration())
        return juce::Result::fail("Effect tail must be nonnegative and fit within the supported mix timeline.");
    effectTailSeconds = seconds; return juce::Result::ok();
}
juce::var projectToJson(const Project& p, const std::vector<std::shared_ptr<Media>>& library)
{
    auto* root = new juce::DynamicObject();
    root->setProperty("format", "mixworks-session"); root->setProperty("version", projectVersion);
    root->setProperty("name", p.name); root->setProperty("bpm", p.bpm); root->setProperty("masterDb", p.masterDb); root->setProperty("snap", p.snap);
    root->setProperty("snapDivision", p.snapDivision);
    root->setProperty("timelineOrigin", p.tempo.origin);
    root->setProperty("keyDisplay", p.keyDisplay);
    root->setProperty("masterAutomation", curveToJson(p.masterAutomation));
    root->setProperty("mastering", masterSettingsToJson(p.mastering));
    root->setProperty("effectTailSeconds", p.effectTailSeconds);
    root->setProperty("rehearsalContext", rehearsalContextToJson(p.rehearsalContext));
    auto* launch = new juce::DynamicObject();
    launch->setProperty("timing", (int) p.launch.timing); launch->setProperty("phraseBars", p.launch.phraseBars);
    root->setProperty("launch", juce::var(launch));
    root->setProperty("followTrackTempo", p.followTrackTempo);
    root->setProperty("transitionPresets", namedTransitionStylesToJson(p.transitionPresets));
    if (!p.performanceTakes.empty())
    {
        juce::Array<juce::var> takes; for (const auto& take:p.performanceTakes) takes.add(takeToJson(take)); root->setProperty("performanceTakes",takes);
    }
    juce::Array<juce::var> tempo;
    for (const auto& point : p.tempo.points)
    {
        auto* obj = new juce::DynamicObject(); obj->setProperty("time", point.time); obj->setProperty("bpm", point.bpm); obj->setProperty("ramp", point.ramp); obj->setProperty("clipId", point.clipId); tempo.add(juce::var(obj));
    }
    root->setProperty("tempo", tempo);
    {
        auto relationships = p; relationships.ensureTransitionLinks();
        juce::Array<juce::var> links;
        for (const auto& link : relationships.transitionLinks)
        {
            auto* item = new juce::DynamicObject(); item->setProperty("outgoing", link.outgoing); item->setProperty("incoming", link.incoming); item->setProperty("locked", link.locked);
            item->setProperty("muteOutside", link.muteOutside);
            if (link.frame) { juce::Array<juce::var> frame; for (auto time : *link.frame) frame.add(time); item->setProperty("frame", frame); }
            if (link.bassSwap) { juce::Array<juce::var> positions; for (auto position : *link.bassSwap) positions.add(position); item->setProperty("bassSwap", positions); }
            if (std::any_of(link.bands.begin(), link.bands.end(), [](const auto& band) { return band.has_value(); })) item->setProperty("bands", transitionBandsToJson(link));
            item->setProperty("filter", transitionFilterToJson(link.filter));
            juce::Array<juce::var> alternatives;
            for (const auto& style : link.alternatives) alternatives.add(style ? transitionStyleToJson(*style) : juce::var());
            item->setProperty("alternatives", alternatives);
            links.add(juce::var(item));
        }
        root->setProperty("transitionLinks", links);
    }
    juce::Array<juce::var> labels;
    for (const auto& marker : p.labels)
    {
        auto* obj = new juce::DynamicObject(); obj->setProperty("id", marker.id); obj->setProperty("time", marker.time); obj->setProperty("text", marker.text); labels.add(juce::var(obj));
    }
    root->setProperty("labels", labels);
    juce::Array<juce::var> tracks, clips;
    for (const auto& t : p.tracks)
    {
        auto data = controlsToJson(t); auto* obj = data.getDynamicObject();
        obj->setProperty("name", t.name); obj->setProperty("mute", t.mute); obj->setProperty("solo", t.solo);
        obj->setProperty("plugins", juce::Array<juce::var>()); tracks.add(data);
    }
    juce::Array<juce::var> media; std::set<juce::String> included;
    auto addMedia = [&](const std::shared_ptr<Media>& m)
    {
        if (!included.insert(m->id).second) return;
        auto* obj = new juce::DynamicObject(); obj->setProperty("id", m->id); obj->setProperty("file", m->file.getFullPathName()); obj->setProperty("name", m->name);
        obj->setProperty("manualBpm", m->manualBpm); obj->setProperty("manualBeatOrigin", m->manualBeatOrigin); obj->setProperty("manualKey", m->manualKey); media.add(juce::var(obj));
    };
    for (const auto& m : library) addMedia(m);
    for (const auto& c : p.clips)
    {
        addMedia(c.media);
        auto* obj = new juce::DynamicObject(); obj->setProperty("id", c.id); obj->setProperty("mediaId", c.media->id); obj->setProperty("track", c.track);
        obj->setProperty("reversed", c.reversed); obj->setProperty("loopSpan", c.loopSpan); obj->setProperty("loopPhase", c.loopPhase);
        obj->setProperty("rate", c.rate); obj->setProperty("beatmatched", c.beatmatched);
        obj->setProperty("tempoSourceBpm", p.followTrackTempo && c.beatmatched && c.tempoMap ? c.tempoSourceBpm : 0);
        obj->setProperty("elasticPlayback", c.elasticPlayback);
        obj->setProperty("start", c.start); obj->setProperty("offset", c.offset); obj->setProperty("length", c.length); obj->setProperty("audioSlip", c.audioSlip);
        obj->setProperty("fadeIn", c.fadeIn); obj->setProperty("fadeOut", c.fadeOut); obj->setProperty("gainDb", c.gainDb);
        obj->setProperty("introAnchor", c.introAnchor ? juce::var(*c.introAnchor) : juce::var());
        obj->setProperty("outroAnchor", c.outroAnchor ? juce::var(*c.outroAnchor) : juce::var());
        obj->setProperty("fadeInAnchor", c.fadeInAnchor ? juce::var(*c.fadeInAnchor) : juce::var());
        obj->setProperty("fadeOutAnchor", c.fadeOutAnchor ? juce::var(*c.fadeOutAnchor) : juce::var());
        auto controls = p.effectSettings(c);
        const auto* effectOwner = p.effectOwner(c);
        controls.pluginAutomation = effectOwner == &c ? p.effectSettings(c).pluginAutomation : std::vector<PluginAutomation>();
        controls.pluginControls = effectOwner == &c ? p.effectSettings(c).pluginControls : std::array<PluginControl, 8>();
        obj->setProperty("effectOwner", effectOwner == &c ? juce::String() : effectOwner->id);
        obj->setProperty("trackOwnerId",effectsIdentity(c.effects));
        obj->setProperty("effectOffset", c.effectOffset);
        obj->setProperty("automaticBass", c.automaticBass); obj->setProperty("controls", controlsToJson(controls)); obj->setProperty("plugins", juce::Array<juce::var>());
        if (c.stemDirectory != juce::File())
        {
            auto* stems = new juce::DynamicObject();
            stems->setProperty("directory", c.stemDirectory.getFullPathName()); stems->setProperty("manifestHash", c.stemManifestHash);
            stems->setProperty("enabled", c.stemsEnabled); stems->setProperty("expanded", c.stemsExpanded);
            juce::Array<juce::var> controls;
            for (const auto& setting : c.stemControls) { auto value = controlsToJson(setting); value.getDynamicObject()->setProperty("mute", setting.mute); value.getDynamicObject()->setProperty("solo", setting.solo); controls.add(value); }
            stems->setProperty("controls", controls); obj->setProperty("stems", juce::var(stems));
        }
        juce::var sectionMarkers;
        if (c.sectionMarkers)
        {
            juce::Array<juce::var> markers;
            for (const auto& marker : *c.sectionMarkers)
            { auto* entry = new juce::DynamicObject(); entry->setProperty("id", marker.id); entry->setProperty("sourceSeconds", marker.sourceSeconds); entry->setProperty("contrastDb", marker.contrastDb); entry->setProperty("manual", marker.manual); entry->setProperty("label", marker.label); markers.add(juce::var(entry)); }
            sectionMarkers = markers;
        }
        obj->setProperty("sectionMarkers", sectionMarkers);
        juce::var hotcues;
        if (c.hotcues)
        {
            juce::Array<juce::var> slots;
            for (const auto& cue : *c.hotcues)
            {
                if (!cue) { slots.add(juce::var()); continue; }
                auto* entry = new juce::DynamicObject(); entry->setProperty("id", cue->id); entry->setProperty("sourceSeconds", cue->sourceSeconds); entry->setProperty("manual", cue->manual); slots.add(juce::var(entry));
            }
            hotcues = slots;
        }
        obj->setProperty("hotcues", hotcues);
        clips.add(juce::var(obj));
    }
    root->setProperty("tracks", tracks); root->setProperty("clips", clips); root->setProperty("media", media);
    return juce::var(root);
}
namespace
{
bool number(const juce::var& v, double low, double high)
{
    if (!(v.isDouble() || v.isInt() || v.isInt64())) return false;
    const double n = (double) v; return std::isfinite(n) && n >= low && n <= high;
}
bool parseCurve(const juce::var& lane, AutomationCurve& curve, juce::Range<float> range, bool legacyDb)
{
    auto* points = lane["points"].getArray();
    if (!lane.isObject() || !lane["enabled"].isBool() || !points) return false;
    curve.enabled = (bool) lane["enabled"]; double previous = -1;
    for (const auto& point : *points)
    {
        const auto value = point[legacyDb ? "db" : "value"];
        if (!point.isObject() || !number(point["time"], 0, 1.0e9) || !number(value, (bool) point["gainSpace"] && range.getStart() == -60 ? -160 : range.getStart(), range.getEnd()) || (double) point["time"] <= previous) return false;
        if (point.hasProperty("bend") && !number(point["bend"], -1, 1)) return false;
        if (point.hasProperty("exponential") && !point["exponential"].isBool()) return false;
        if (point.hasProperty("gainSpace") && (!point["gainSpace"].isBool() || ((bool) point["gainSpace"] && range.getStart() != -60))) return false;
        previous = (double) point["time"]; curve.points.push_back({previous, (float) value, point.hasProperty("bend") ? (double) point["bend"] : 0, (bool) point["exponential"], (bool) point["gainSpace"]});
    }
    return true;
}
bool parseFilterControl(const juce::var& t, TrackSettings& target, int version)
{
    if (version < 23) return true;
    const auto value = t[version >= 27 ? "filterCutoff" : "lowPassHz"];
    if (!number(value, version >= 27 ? -1 : 20, version >= 27 ? 1 : 20000)) return false;
    target.filterCutoff = version >= 27 ? (float) value : lowPassToCutoff((float) value); return true;
}
bool parseControlCurve(const juce::var& automation, AutomationCurve& curve, AutomationTarget target, int version, bool legacyDb = false)
{
    if (target != AutomationTarget::cutoff || version >= 27)
        return parseCurve(automation[automationId(target)], curve, automationRange(target), legacyDb);
    if (!parseCurve(automation["low-pass"], curve, {20, 20000}, false)) return false;
    for (auto& point : curve.points) point.value = lowPassToCutoff(point.value);
    return true;
}
bool parseClipControls(const juce::var& c, Clip& clip, std::set<juce::String>& pluginIds, int version)
{
    const auto& t = c["controls"]; auto& target = clip.controls;
    auto* plugins = c["plugins"].getArray(); auto* eq = t["eqDb"].getArray(); auto* controls = t["pluginControls"].getArray(); auto* lanes = t["pluginAutomation"].getArray();
    if (!t.isObject() || !number(t["gainDb"], -60, 12) || !number(t["pan"], -1, 1) || !plugins || !eq || eq->size() != 3 || !controls || controls->size() != 8 || !lanes || !t["automation"].isObject()) return false;
    target.gainDb = (float) t["gainDb"]; target.pan = (float) t["pan"];
    if (!parseFilterControl(t, target, version)) return false;
    for (int n = 0; n < 3; ++n) { if (!number((*eq)[n], -60, 12)) return false; target.eqDb[(size_t) n] = (float) (*eq)[n]; }
    for (int n = 0; n < (version >= 23 ? trackAutomationCount : 5); ++n)
        if (!parseControlCurve(t["automation"], target.automation[(size_t) n], (AutomationTarget) n, version)) return false;
    std::set<juce::String> slots; std::set<std::pair<juce::String, juce::String>> targets;
    for (const auto& p : *plugins)
    {
        if (!p.isObject() || !p["id"].isString() || p["id"].toString().isEmpty() || !pluginIds.insert(p["id"].toString()).second
            || !p["description"].isString() || juce::parseXML(p["description"].toString()) == nullptr || !p["state"].isString() || !p["bypass"].isBool()) return false;
        slots.insert(p["id"].toString());
    }
    for (const auto& lane : *lanes)
    {
        PluginAutomation item;
        if (!lane.isObject() || !lane["slotId"].isString() || !lane["parameterId"].isString() || lane["parameterId"].toString().isEmpty() || !lane["name"].isString() || !number(lane["manualValue"], 0, 1)) return false;
        item.slotId = lane["slotId"].toString(); item.parameterId = lane["parameterId"].toString(); item.name = lane["name"].toString(); item.manualValue = (float) lane["manualValue"];
        if (!slots.count(item.slotId) || !targets.insert({item.slotId, item.parameterId}).second || !parseCurve(lane["curve"], item.curve, {0, 1}, false)) return false;
        target.pluginAutomation.push_back(std::move(item));
    }
    for (int n = 0; n < 8; ++n)
    {
        const auto& item = (*controls)[n];
        if (!item.isObject() || !item["insert"].isInt() || (int) item["insert"] < 0 || !item["parameterId"].isString()
            || (item["parameterId"].toString().isNotEmpty() && (int) item["insert"] >= plugins->size())) return false;
        target.pluginControls[(size_t) n] = {(int) item["insert"], item["parameterId"].toString()};
    }
    return true;
}

}
juce::String relinkProjectSource(const ProjectResult& parsed, const juce::String& mediaId, const juce::File& replacement)
{
    if (parsed.error.isNotEmpty() || replacement == juce::File()) return {};
    const auto found = std::find_if(parsed.media.begin(), parsed.media.end(), [&](const auto& media) { return media.id == mediaId; });
    if (found == parsed.media.end()) return {};
    auto document = parsed.document.clone(); bool changed = false;
    if (auto* media = document["media"].getArray())
    {
        for (auto& entry : *media) if (entry["id"].toString() == mediaId)
        { entry.getDynamicObject()->setProperty("file", replacement.getFullPathName()); changed = true; }
    }
    else if (auto* clips = document["clips"].getArray())
    {
        for (auto& clip : *clips) if (clip["file"].toString() == found->path)
        { clip.getDynamicObject()->setProperty("file", replacement.getFullPathName()); changed = true; }
    }
    return changed ? juce::JSON::toString(document, false, 17) : juce::String();
}
ProjectResult parseProject(const juce::String& text)
{
    ProjectResult r;
    auto fail = [&r](const juce::String& s) { r.error = s; return r; };
    auto parsed = juce::JSON::parse(text, r.document);
    if (parsed.failed() || !r.document.isObject()) return fail("Invalid project JSON.");
    const auto& d = r.document;
    if (d["format"].toString() != "mixworks-session" || !d["version"].isInt() || (int) d["version"] < 1 || (int) d["version"] > projectVersion)
        return fail("This is not a supported Mixworks session. Fusion .mmp files are not yet supported.");
    if ((int) d["version"] == 24)
    {
        auto* saved = d["tracks"].getArray();
        if (!saved || saved->size() != 8) return fail("Invalid eight-lane session.");
        const auto neutralDocument = projectToJson(Project());
        for (int i = trackCount; i < 8; ++i)
        {
            auto neutral = (*neutralDocument["tracks"].getArray())[0].clone();
            neutral.getDynamicObject()->setProperty("name", "Audio " + juce::String(i + 1));
            neutral.getDynamicObject()->removeProperty("filterCutoff"); neutral.getDynamicObject()->setProperty("lowPassHz", 20000);
            auto* curves = neutral["automation"].getDynamicObject();
            curves->setProperty("low-pass", neutral["automation"]["cutoff"]); curves->removeProperty("cutoff");
            if (!juce::JSONUtils::deepEqual((*saved)[i], neutral))
                return fail("This mix uses lanes 5-8. Open it in Alloy 0.54.1 and move that work to lanes 1-4 before opening here. The saved mix is unchanged.");
        }
        if (const auto* savedClips = d["clips"].getArray())
            for (const auto& clip : *savedClips)
                if (clip["track"].isInt() && (int) clip["track"] >= trackCount)
                    return fail("This mix contains audio on lanes 5-8. Move it to lanes 1-4 in Alloy 0.54.1 first. The saved mix is unchanged.");
        saved->removeRange(trackCount, 8 - trackCount);
    }
    if (!d["name"].isString() || !number(d["bpm"], 20, 400) || !number(d["masterDb"], -60, 12) || !d["snap"].isBool()) return fail("Invalid session settings.");
    r.project.name = d["name"].toString(); r.project.bpm = (double) d["bpm"]; r.project.masterDb = (float) d["masterDb"]; r.project.snap = (bool) d["snap"];
    if (d.hasProperty("performanceTakes"))
    {
        if (!d["performanceTakes"].isArray() || d["performanceTakes"].size()>32) return fail("Invalid performance takes.");
        std::set<juce::String> ids;
        for (const auto& item:*d["performanceTakes"].getArray())
        {
            PerformanceTake take; const auto result=readTake(item,take); if (result.failed() || !ids.insert(take.id).second) return fail("Invalid performance take.");
            r.project.performanceTakes.push_back(std::move(take));
        }
    }
    if ((int) d["version"] >= 33 || d.hasProperty("launch"))
    {
        const auto& launch = d["launch"];
        if (!launch.isObject() || !launch["timing"].isInt() || !launch["phraseBars"].isInt()) return fail("Invalid launch timing settings.");
        r.project.launch = {(LaunchTiming) (int) launch["timing"], (int) launch["phraseBars"]};
        const auto valid = r.project.launch.validate(); if (valid.failed()) return fail(valid.getErrorMessage());
    }
    if ((int) d["version"] >= 28 || d.hasProperty("snapDivision"))
    {
        if (!d["snapDivision"].isInt() || ((int) d["snapDivision"] != 1 && (int) d["snapDivision"] != 2 && (int) d["snapDivision"] != 4))
            return fail("Invalid snap subdivision.");
        r.project.snapDivision = (int) d["snapDivision"];
    }
    r.project.tempo.fallback = r.project.bpm;
    if ((int) d["version"] >= 30)
    {
        if (!number(d["timelineOrigin"], 0, 600)) return fail("Invalid pre-roll origin.");
        r.project.tempo.origin = (double) d["timelineOrigin"];
    }
    if ((int) d["version"] >= 18)
    {
        const auto settings = readMasterSettings(d["mastering"], r.project.mastering);
        if (settings.failed()) return fail(settings.getErrorMessage());
    }
    if ((int) d["version"] >= 7)
    {
        const auto* labels = d["labels"].getArray(); if (!labels) return fail("Invalid label markers.");
        double previous = -1; std::set<juce::String> ids;
        for (const auto& item : *labels)
        {
            if (!item.isObject() || !item["id"].isString() || !number(item["time"], 0, 1.0e9) || !item["text"].isString()) return fail("Invalid label marker.");
            LabelMarker marker {item["id"].toString(), (double) item["time"], item["text"].toString()};
            if (marker.time <= previous || !ids.insert(marker.id).second) return fail("Label marker times must be ordered and unique, with unique identities.");
            const auto result = marker.validate(); if (result.failed()) return fail(result.getErrorMessage());
            r.project.labels.push_back(std::move(marker));
            previous = marker.time;
        }
    }
    r.project.followTrackTempo = false; // Preserve the sound and placement of existing fixed-tempo sessions.
    if ((int) d["version"] >= 6)
    {
        if (!d["followTrackTempo"].isBool() || !d["tempo"].isArray()) return fail("Invalid tempo map.");
        r.project.followTrackTempo = (bool) d["followTrackTempo"];
        double previous = -1; juce::String previousOwner; std::set<juce::String> owners;
        for (const auto& point : *d["tempo"].getArray())
        {
            const bool ownedFormat = (int) d["version"] >= 12;
            if (ownedFormat && !point["clipId"].isString()) return fail("Invalid tempo marker owner.");
            const auto owner = ownedFormat ? point["clipId"].toString() : juce::String();
            if (!point.isObject() || !number(point["time"], 0, 1.0e9) || !number(point["bpm"], 20, 400) || !point["ramp"].isBool()
                || (double) point["time"] < previous || ((int) d["version"] < 13 && (double) point["time"] == previous && owner.isEmpty() && previousOwner.isEmpty())
                || (previous < 0 && (double) point["time"] != 0) || (owner.isNotEmpty() && !owners.insert(owner).second)) return fail("Invalid tempo marker ordering or ownership.");
            previous = (double) point["time"]; previousOwner = owner;
            r.project.tempo.points.push_back({previous, (double) point["bpm"], (bool) point["ramp"], owner});
        }
        if (!r.project.followTrackTempo && !r.project.tempo.points.empty()) return fail("A fixed-tempo session cannot contain a tempo map.");
    }
    const bool extendedAutomation = (int) d["version"] >= 5;
    if (extendedAutomation && !parseCurve(d["masterAutomation"], r.project.masterAutomation, automationRange(AutomationTarget::master), false)) return fail("Invalid master automation.");
    const bool legacy = (int) d["version"] == 1;
    const bool hasLibrary = (int) d["version"] >= 3;
    if (!legacy) { if (!d["keyDisplay"].isInt() || !number(d["keyDisplay"], 0, 2)) return fail("Invalid key display setting."); r.project.keyDisplay = (int) d["keyDisplay"]; }
    auto* tracks = d["tracks"].getArray(); auto* clips = d["clips"].getArray();
    const int savedTrackCount = trackCount;
    if (tracks == nullptr || tracks->size() != savedTrackCount || clips == nullptr) return fail("Invalid tracks or segments.");
    std::set<juce::String> pluginIds;
    for (int i = 0; i < savedTrackCount; ++i)
    {
        const auto& t = (*tracks)[i];
        if (!t.isObject() || !t["name"].isString() || !number(t["gainDb"], -60, 12) || !number(t["pan"], -1, 1) || !t["mute"].isBool() || !t["solo"].isBool() || !t["plugins"].isArray()) return fail("Invalid track settings.");
        auto& target = r.project.tracks[(size_t) i];
        if (!parseFilterControl(t, target, (int) d["version"])) return fail("Invalid lane cutoff filter.");
        target.name = t["name"].toString(); target.gainDb = (float) t["gainDb"]; target.pan = (float) t["pan"];
        target.mute = (bool) t["mute"]; target.solo = (bool) t["solo"];

        if (hasLibrary)
        {
            auto* eq = t["eqDb"].getArray(); if (!eq || eq->size() != 3) return fail("Invalid track EQ.");
            for (int n = 0; n < 3; ++n) { if (!number((*eq)[n], -60, 12)) return fail("Invalid EQ gain."); target.eqDb[(size_t) n] = (float) (*eq)[n]; }
        }
        if ((int) d["version"] >= 4)
        {
            if (!t["automation"].isObject()) return fail("Invalid track automation.");
            for (int n = 0; n < (extendedAutomation ? ((int) d["version"] >= 23 ? trackAutomationCount : 5) : 4); ++n)
            {
                if (!parseControlCurve(t["automation"], target.automation[(size_t) n], (AutomationTarget) n, (int) d["version"], !extendedAutomation))
                    return fail("Invalid automation points: check time ordering, value type and parameter range.");
            }
        }
        if (extendedAutomation)
        {
            std::set<juce::String> trackSlots; std::set<std::pair<juce::String, juce::String>> targets;
            for (const auto& p : *t["plugins"].getArray())
            {
                if (!p["id"].isString() || p["id"].toString().isEmpty() || !pluginIds.insert(p["id"].toString()).second) return fail("Invalid or duplicate plugin identity.");
                trackSlots.insert(p["id"].toString());
            }
            auto* lanes = t["pluginAutomation"].getArray(); if (!lanes) return fail("Invalid VST automation lanes.");
            for (const auto& lane : *lanes)
            {
                PluginAutomation item;
                if (!lane.isObject() || !lane["slotId"].isString() || !lane["parameterId"].isString() || lane["parameterId"].toString().isEmpty() || !lane["name"].isString() || !number(lane["manualValue"], 0, 1)) return fail("Invalid VST automation target.");
                item.slotId = lane["slotId"].toString(); item.parameterId = lane["parameterId"].toString(); item.name = lane["name"].toString(); item.manualValue = (float) lane["manualValue"];
                if (!trackSlots.count(item.slotId) || !targets.insert({item.slotId, item.parameterId}).second || !parseCurve(lane["curve"], item.curve, {0, 1}, false)) return fail("Invalid, missing or duplicate VST automation target.");
                target.pluginAutomation.push_back(std::move(item));
            }
        }
        if (t.hasProperty("pluginControls"))
        {
            auto* controls = t["pluginControls"].getArray();
            if (!controls || controls->size() != 8) return fail("Invalid plugin MIDI control assignments.");
            for (int n = 0; n < 8; ++n)
            {
                const auto& control = (*controls)[n];
                if (!control.isObject() || !control["insert"].isInt() || (int) control["insert"] < 0 || !control["parameterId"].isString()
                    || (control["parameterId"].toString().isNotEmpty() && (int) control["insert"] >= t["plugins"].getArray()->size())) return fail("Invalid plugin MIDI control target.");
                target.pluginControls[(size_t) n] = {(int) control["insert"], control["parameterId"].toString()};
            }
        }
        for (const auto& plugin : *t["plugins"].getArray())
            if (!plugin.isObject() || !plugin["description"].isString() || !plugin["state"].isString() || !plugin["bypass"].isBool() || juce::parseXML(plugin["description"].toString()) == nullptr)
                return fail("Invalid plugin state.");
    }
    std::set<juce::String> mediaIds;
    auto parseMedia = [&](const juce::var& m, const juce::String& id) -> bool
    {
        if (!m["file"].isString() || !juce::File::isAbsolutePath(m["file"].toString()) || !m["name"].isString()) return false;
        MediaReference source; source.id = id; source.path = m["file"].toString(); source.name = m["name"].toString();
        if (legacy) { if (!number(m["bpm"], 0, 400)) return false; source.bpm = (double) m["bpm"]; }
        else
        {
            if (!number(m["manualBpm"], 0, 400) || ((double) m["manualBpm"] != 0 && (double) m["manualBpm"] < 20)
                || !number(m["manualBeatOrigin"], -1, 1.0e9) || ((double) m["manualBeatOrigin"] < 0 && (double) m["manualBeatOrigin"] != -1)
                || !m["manualKey"].isInt() || !number(m["manualKey"], -1, 23)) return false;
            source.bpm = (double) m["manualBpm"]; source.beatOrigin = (double) m["manualBeatOrigin"]; source.key = (int) m["manualKey"];
        }
        r.media.push_back(source); return true;
    };
    if (hasLibrary)
    {
        auto* media = d["media"].getArray(); if (!media) return fail("Invalid media library.");
        for (const auto& m : *media)
            if (!m.isObject() || !m["id"].isString() || m["id"].toString().isEmpty() || !mediaIds.insert(m["id"].toString()).second || !parseMedia(m, m["id"].toString())) return fail("Invalid media library entry.");
    }
    std::set<juce::String> ids; std::map<juce::String, juce::String> legacySources;
    for (const auto& c : *clips)
    {
        if (!c.isObject() || !c["id"].isString() || c["id"].toString().isEmpty() || !ids.insert(c["id"].toString()).second
            || !c["track"].isInt() || !number(c["track"], 0, savedTrackCount - 1) || !number(c["start"], 0, 1.0e9) || !number(c["offset"], 0, 1.0e9) || !number(c["length"], 0.000001, 1.0e9)
            || !number(c["fadeIn"], 0, (int) d["version"] >= 9 && !c[(int) d["version"] >= 10 ? "fadeInAnchor" : "introAnchor"].isVoid() ? 1.0e9 : (double) c["length"]) || !number(c["fadeOut"], 0, (int) d["version"] >= 9 && !c[(int) d["version"] >= 10 ? "fadeOutAnchor" : "outroAnchor"].isVoid() ? 1.0e9 : (double) c["length"]) || !number(c["gainDb"], -60, 12)) return fail("Invalid segment settings.");
        ClipReference reference; auto& clip = reference.clip;
        clip.id = c["id"].toString(); clip.track = (int) c["track"]; clip.start = (double) c["start"]; clip.offset = (double) c["offset"]; clip.length = (double) c["length"];
        if ((int) d["version"] >= 32)
        {
            if (!c.hasProperty("hotcues") || (!c["hotcues"].isVoid() && !c["hotcues"].isArray())) return fail("Invalid hotcue slots.");
            if (const auto* slots = c["hotcues"].getArray())
            {
                if (slots->size() != 9) return fail("Hotcues require exactly nine slots.");
                clip.hotcues.emplace(); std::set<juce::String> cueIds;
                for (int slot = 0; slot < 9; ++slot)
                {
                    const auto& cue = (*slots)[slot]; if (cue.isVoid()) continue;
                    const auto identity = cue["id"].toString();
                    if (!cue.isObject() || !cue["id"].isString() || identity.isEmpty() || identity.length() > 64 || !cueIds.insert(identity).second
                        || !number(cue["sourceSeconds"], 0, 1.e9) || !cue["manual"].isBool()) return fail("Invalid hotcue identity or source position.");
                    (*clip.hotcues)[(size_t) slot] = HotcueMarker{identity, (double) cue["sourceSeconds"], (bool) cue["manual"]};
                }
            }
        }
        if ((int) d["version"] >= 31)
        {
            if (!c.hasProperty("sectionMarkers") || (!c["sectionMarkers"].isVoid() && !c["sectionMarkers"].isArray())) return fail("Invalid section annotations.");
            if (const auto* markers = c["sectionMarkers"].getArray())
            {
                if (markers->size() > 4096) return fail("Too many section annotations.");
                clip.sectionMarkers.emplace(); std::set<juce::String> markerIds; double previous = -1;
                for (const auto& marker : *markers)
                {
                    const auto id = marker["id"].toString();
                    if (!marker.isObject() || !marker["id"].isString() || id.isEmpty() || id.length() > 64 || !markerIds.insert(id).second
                        || !number(marker["sourceSeconds"], 0, 1.e9) || (double) marker["sourceSeconds"] <= previous
                        || !number(marker["contrastDb"], -200, 200) || !marker["manual"].isBool()
                        || (marker.hasProperty("label") && (!marker["label"].isString() || marker["label"].toString().length() > 128 || marker["label"].toString().containsAnyOf("\r\n\t")))) return fail("Invalid section annotation ordering, identity or label.");
                    previous = (double) marker["sourceSeconds"]; clip.sectionMarkers->push_back({id, previous, (double) marker["contrastDb"], (bool) marker["manual"], marker["label"].toString()});
                }
            }
        }
        if ((int) d["version"] >= 22)
        {
            if (!number(c["audioSlip"], -1.0e9, 1.0e9)) return fail("Invalid audio slip.");
            clip.audioSlip = (double) c["audioSlip"];
        }
        if ((int) d["version"] >= 11 && (!c["automaticBass"].isBool() || !parseClipControls(c, clip, pluginIds, (int) d["version"]))) return fail("Invalid audio-track automation or effects.");
        if ((int)d["version"] >= 40)
        {
            if (!c["effectOwner"].isString() || !number(c["effectOffset"], 0, 1.e9)) return fail("Invalid shared track effects.");
            clip.effectOffset = (double)c["effectOffset"];
            const auto owner = c["effectOwner"].toString();
            if (owner.isNotEmpty())
            {
                const auto found = std::find_if(r.clips.begin(), r.clips.end(), [&](const auto& item) { return item.clip.id == owner; });
                if (found == r.clips.end() || c["plugins"].size() != 0 || !clip.controls.pluginAutomation.empty()) return fail("Invalid shared effect owner.");
                clip.effects = found->clip.effects;
            }
            if ((int)d["version"]>=41)
            {
                const auto identity=c["trackOwnerId"].toString();
                if (!c["trackOwnerId"].isString() || identity.isEmpty() || identity.length()>512) return fail("Invalid original-track identity.");
                if (owner.isNotEmpty() && effectsIdentity(clip.effects)!=identity) return fail("Conflicting original-track identity.");
                if (owner.isEmpty() && std::any_of(r.clips.begin(),r.clips.end(),[&](const auto& reference) { return effectsIdentity(reference.clip.effects)==identity; })) return fail("Duplicate original-track identity.");
                restoreEffectsIdentity(clip.effects,identity);
            }
        }
        if (c.hasProperty("stems"))
        {
            const auto s = c["stems"]; const auto* settings = s["controls"].getArray();
            const auto path = s["directory"].toString(), hash = s["manifestHash"].toString();
            if (!s.isObject() || !s["directory"].isString() || !juce::File::isAbsolutePath(path)
                || !s["manifestHash"].isString() || hash.length() != 64 || !hash.containsOnly("0123456789abcdef")
                || !s["enabled"].isBool() || !s["expanded"].isBool() || !settings || settings->size() != stemCount)
                return fail("Invalid stem asset reference.");
            clip.stemDirectory = juce::File(path); clip.stemManifestHash = hash;
            clip.stemsEnabled = (bool) s["enabled"]; clip.stemsExpanded = (bool) s["expanded"];
            for (int i = 0; i < stemCount; ++i)
            {
                Clip parsed; auto* wrapper = new juce::DynamicObject(); wrapper->setProperty("controls", (*settings)[i]);
                wrapper->setProperty("plugins", juce::Array<juce::var>());
                if (!parseClipControls(juce::var(wrapper), parsed, pluginIds, (int) d["version"])) return fail("Invalid stem controls.");
                if (!parsed.controls.pluginAutomation.empty() || parsed.controls.filterCutoff != 0
                    || parsed.controls.eqDb != std::array<float, 3>{}) return fail("Unsupported stem effects.");
                for (int j : {1, 2, 3, 5}) if (!parsed.controls.automation[(size_t) j].points.empty()) return fail("Unsupported stem automation.");
                if (!(*settings)[i]["mute"].isBool() || !(*settings)[i]["solo"].isBool()) return fail("Invalid stem mute/solo.");
                parsed.controls.mute = (bool) (*settings)[i]["mute"]; parsed.controls.solo = (bool) (*settings)[i]["solo"];
                clip.stemControls[(size_t) i] = std::move(parsed.controls);
            }
        }
        clip.automaticBass = (bool) c.getProperty("automaticBass", false);
        clip.fadeIn = (double) c["fadeIn"]; clip.fadeOut = (double) c["fadeOut"]; clip.gainDb = (float) c["gainDb"];
        if ((int) d["version"] >= 9)
        {
            auto anchor = [&](const juce::var& v) { return v.isVoid() || number(v, -1.0e9, 1.0e9); };
            if (!c.hasProperty("introAnchor") || !c.hasProperty("outroAnchor") || !anchor(c["introAnchor"]) || !anchor(c["outroAnchor"])) return fail("Invalid alignment markers.");
            if (!c["introAnchor"].isVoid()) clip.introAnchor = (double) c["introAnchor"];
            if (!c["outroAnchor"].isVoid()) clip.outroAnchor = (double) c["outroAnchor"];
        }
        if ((int) d["version"] >= 10)
        {
            auto anchor = [&](const juce::var& v) { return v.isVoid() || number(v, -1.0e9, 1.0e9); };
            if (!c.hasProperty("fadeInAnchor") || !c.hasProperty("fadeOutAnchor") || !anchor(c["fadeInAnchor"]) || !anchor(c["fadeOutAnchor"])) return fail("Invalid fade anchors.");
            if (!c["fadeInAnchor"].isVoid()) clip.fadeInAnchor = (double) c["fadeInAnchor"];
            if (!c["fadeOutAnchor"].isVoid()) clip.fadeOutAnchor = (double) c["fadeOutAnchor"];
        }
        if (!legacy)
        {
            if (!number(c["rate"], 0.05, 20) || !c["beatmatched"].isBool()) return fail("Invalid beatmatching settings.");
            clip.rate = (double) c["rate"]; clip.beatmatched = (bool) c["beatmatched"];
        }
        if ((int) d["version"] >= 13)
        {
            // The bound follows the session's 400 BPM map and minimum 0.05
            // saved playback rate, with rounding allowance for migrated maps.
            if (!number(c["tempoSourceBpm"], 0, 8000.0001)) return fail("Invalid captured source clock.");
            clip.tempoSourceBpm = (double) c["tempoSourceBpm"];
            if (clip.tempoSourceBpm > 0)
            {
                if (!clip.beatmatched || !r.project.followTrackTempo || r.project.tempo.points.empty()) return fail("A captured source clock requires a matched tempo map.");
                const double beats = r.project.tempo.beatsAt(clip.end()) - r.project.tempo.beatsAt(clip.start);
                if (std::abs(beats - clip.length * clip.rate * clip.tempoSourceBpm / 60) > std::max(1.0e-8, std::abs(beats) * 1.0e-8))
                    return fail("The captured source clock does not match saved segment geometry.");
            }
        }
        if ((int) d["version"] >= 8)
        {
            if (!c["elasticPlayback"].isBool()) return fail("Invalid segment playback mode.");
            clip.elasticPlayback = (bool) c["elasticPlayback"];
            if (clip.elasticPlayback && (!clip.beatmatched || !r.project.followTrackTempo || r.project.tempo.points.empty())) return fail("Elastic playback requires a matched segment and a shared tempo map.");
        }
        if (hasLibrary)
        {
            if (!c["mediaId"].isString() || mediaIds.count(c["mediaId"].toString()) == 0 || !c["reversed"].isBool() || !number(c["loopSpan"], 0, 1.0e9)
                || !number(c["loopPhase"], 0, (double) c["loopSpan"]) || ((double) c["loopSpan"] > 0 && (double) c["loopPhase"] >= (double) c["loopSpan"])) return fail("Invalid source, loop or reverse settings.");
            reference.mediaId = c["mediaId"].toString(); clip.reversed = (bool) c["reversed"]; clip.loopSpan = (double) c["loopSpan"]; clip.loopPhase = (double) c["loopPhase"];
        }
        else
        {
            auto path = c["file"].toString(); auto it = legacySources.find(path);
            if (it == legacySources.end())
            {
                auto id = juce::Uuid().toString(); if (!parseMedia(c, id)) return fail("Invalid legacy media reference.");
                legacySources[path] = id; reference.mediaId = id;
            }
            else
            {
                const auto existing = *std::find_if(r.media.begin(), r.media.end(), [&](const auto& m) { return m.id == it->second; });
                if (!parseMedia(c, it->second)) return fail("Invalid legacy media reference.");
                const auto candidate = r.media.back(); r.media.pop_back();
                if (candidate.name != existing.name || candidate.bpm != existing.bpm || candidate.beatOrigin != existing.beatOrigin || candidate.key != existing.key) return fail("Conflicting metadata for the same legacy source.");
                reference.mediaId = it->second;
            }
        }
        r.clips.push_back(reference);
    }
    for (const auto& p : r.project.tempo.points) if (p.clipId.isNotEmpty() && !ids.count(p.clipId)) return fail("A tempo marker refers to a missing track.");
    if ((int) d["version"] >= 13 && d.hasProperty("transitionLinks"))
    {
        if (!d["transitionLinks"].isArray()) return fail("Invalid transition links.");
        r.project.transitionLinksKnown = true;
        for (const auto& item : *d["transitionLinks"].getArray())
        {
            if (!item.isObject() || !item["outgoing"].isString() || !item["incoming"].isString()) return fail("Invalid transition link.");
            r.project.transitionLinks.push_back({item["outgoing"].toString(), item["incoming"].toString()});
            if ((int)d["version"] >= 39)
            {
                if (!item["muteOutside"].isBool()) return fail("Invalid transition mute setting.");
                r.project.transitionLinks.back().muteOutside = (bool)item["muteOutside"];
                if (item.hasProperty("frame"))
                {
                    const auto frame = item["frame"];
                    if (!frame.isArray() || frame.size() != 2 || !number(frame[0],0,1.e9) || !number(frame[1],(double)frame[0],1.e9)) return fail("Invalid transition frame.");
                    r.project.transitionLinks.back().frame = std::array<double,2>{(double)frame[0],(double)frame[1]};
                }
            }
            if ((int) d["version"] >= 15)
            {
                if (!item["locked"].isBool()) return fail("Invalid transition lock.");
                r.project.transitionLinks.back().locked = (bool) item["locked"];
            }
            if ((int) d["version"] >= 14 && item.hasProperty("bassSwap"))
            {
                const auto value = item["bassSwap"]; if (!value.isArray() || value.size() != 4) return fail("Invalid paired bass-swap positions.");
                std::array<double, 4> positions;
                for (int i = 0; i < 4; ++i) { if (!number(value[i], 0, 1.0e9)) return fail("Invalid paired bass-swap positions."); positions[(size_t) i] = (double) value[i]; }
                r.project.transitionLinks.back().bassSwap = positions;
            }
            if ((int) d["version"] >= 16 && item.hasProperty("bands"))
            {
                const auto bands = readTransitionBands(item["bands"], r.project.transitionLinks.back());
                if (bands.failed()) return fail(bands.getErrorMessage());
            }
            if ((int) d["version"] >= 20)
            {
                const auto filter = readTransitionFilter(item["filter"], r.project.transitionLinks.back().filter);
                if (filter.failed()) return fail(filter.getErrorMessage());
            }
            if ((int) d["version"] >= 17)
            {
                const auto alternatives = item["alternatives"];
                if (!alternatives.isArray() || alternatives.size() != 3) return fail("Invalid transition alternatives.");
                for (int i = 0; i < 3; ++i) if (!alternatives[i].isVoid())
                {
                    TransitionStyle style; const auto valid = readTransitionStyle(alternatives[i], style);
                    if (valid.failed()) return fail(valid.getErrorMessage());
                    r.project.transitionLinks.back().alternatives[(size_t) i] = style;
                }
            }
        }
        auto shape = r.project; for (const auto& reference : r.clips) shape.clips.push_back(reference.clip);
        const auto result = shape.validateTransitionLinks(); if (result.failed()) return fail(result.getErrorMessage());
    }
    if ((int) d["version"] >= 17)
    {
        const auto valid = readNamedTransitionStyles(d["transitionPresets"], r.project.transitionPresets);
        if (valid.failed()) return fail(valid.getErrorMessage());
    }
    if ((int) d["version"] >= 21)
    {
        const auto result = readRehearsalContext(d["rehearsalContext"], r.project.rehearsalContext);
        if (result.failed()) return fail(result.getErrorMessage());
    }
    if ((int) d["version"] >= 19)
    {
        if (!number(d["effectTailSeconds"], 0, 1.0e9)) return fail("Invalid effect-tail window.");
        auto shape = r.project; for (const auto& reference : r.clips) shape.clips.push_back(reference.clip);
        const auto result = shape.setEffectTailSeconds((double) d["effectTailSeconds"]);
        if (result.failed()) return fail(result.getErrorMessage());
        r.project.effectTailSeconds = shape.effectTailSeconds;
    }
    return r;
}
}
