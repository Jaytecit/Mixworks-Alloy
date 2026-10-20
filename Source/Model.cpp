#include "Model.h"
#include "ElasticAudio.h"
#include <cmath>
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
double TempoMap::snap(double time, bool enabled) const { return enabled ? timeAtBeat(std::round(beatsAt(time))) : std::max(0.0, time); }
double TempoMap::snapWithPreroll(double time, bool enabled) const
{
    if (!enabled) return time;
    if (time >= 0) return snap(time, true);
    const double beat = 60 / bpmAt(0);
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
        || (manualKey < 0 && analysis.keyConfidence < 0.55) || analysis.rhythmIssue.isNotEmpty();
}
namespace
{
double loopPosition(double time, double span)
{
    double wrapped = std::fmod(time, span);
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
    return followTrackTempo ? tempo.snap(seconds, enabled) : snapTime(seconds, bpm, enabled);
}
double Project::snappedClipStart(const Clip& clip, double seconds, bool enabled, double magnetSeconds) const
{
    if (!enabled) return std::max(0.0, seconds);
    seconds = std::max(0.0, seconds);
    const auto grid = followTrackTempo ? tempo : TempoMap {bpm, {}};
    const double sourceBpm = clip.tempoSourceBpm > 0 ? clip.tempoSourceBpm : clip.media->bpm();
    const bool mapped = followTrackTempo && clip.beatmatched && sourceBpm > 0;
    const double phase = mapped ? clip.sourceElapsed(clip.firstBeatTime()) * sourceBpm / 60 : clip.firstBeatTime();
    const double desired = mapped ? grid.beatsAt(seconds) + phase : grid.beatsAt(seconds + phase);
    const double minimum = mapped ? phase : grid.beatsAt(phase);
    // Clamping a negative *clip edge* to zero would leave its first beat off grid.
    const double beat = std::max(std::ceil(minimum - 1.0e-9), std::round(desired));
    double placed = mapped ? grid.timeAtBeat(beat - phase) : grid.timeAtBeat(beat) - phase;
    placed = std::max(0.0, placed);
    // A deliberately free-positioned neighbour may supply a closer visible pulse.
    // Normal matched neighbours coincide with the background grid already.
    for (const auto& other : clips)
    {
        if (magnetSeconds <= 0 || other.id == clip.id || other.media->bpm() <= 0) continue;
        const double anchor = mapped ? grid.timeAtBeat(desired) : seconds + phase;
        if (anchor < other.start || anchor >= other.end()) continue;
        const double period = 60 / other.media->bpm(), first = other.sourceElapsed(other.firstBeatTime());
        const double pulse = first + std::round((other.sourceElapsed(anchor - other.start) - first) / period) * period;
        const double target = other.start + other.localTimeForSource(pulse);
        if (pulse < 0 || target >= other.end()) continue;
        if (mapped && grid.beatsAt(target) < phase) continue;
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
        std::vector<AutomationPoint> points {{0, curve.interpolatedValue(start, manual)}};
        for (const auto& point : curve.points) if (point.time > start && point.time < end()) points.push_back({sourceElapsed(point.time - start), point.value});
        points.push_back({sourceElapsed(length), curve.interpolatedValue(end(), manual)}); curve.points = std::move(points);
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
        position = tempoMap->timeAtBeat(std::max(0.0, beat));
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
    const double beat = time < 0 ? time * tempoMap->bpmAt(0) / 60 : tempoMap->beatsAt(time);
    return (beat - tempoMap->beatsAt(start)) * 60 / sourceBpm;
}
double Clip::localTimeForSource(double source) const
{
    const double sourceBpm = tempoSourceBpm > 0 ? tempoSourceBpm : media->bpm();
    if (!tempoMap || sourceBpm <= 0) return source / rate;
    const double beat = tempoMap->beatsAt(start) + source * sourceBpm / 60;
    return (beat < 0 ? beat * 60 / tempoMap->bpmAt(0) : tempoMap->timeAtBeat(beat)) - start;
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
    double energy = 0, weight = 0;
    for (int i = first; i < last && i < (int) waveformRms.size(); ++i)
    {
        const double count = std::max(0.0, std::min(end * sampleRate, (double) std::min((i + 1) * bucket, frames())) - std::max(begin * sampleRate, (double) i * bucket));
        energy += waveformRms[(size_t) i] * (double) waveformRms[(size_t) i] * count; weight += count;
    }
    if (weight > 0) result.rms = (float) std::sqrt(energy / weight);
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
        const float first = curve.interpolatedValue(sourceBegin, 0), last = curve.interpolatedValue(sourceBegin + sourceLength, 0);
        std::vector<AutomationPoint> points {{0, first}};
        for (const auto& point : curve.points) if (point.time > sourceBegin && point.time < sourceBegin + sourceLength) points.push_back({point.time - sourceBegin, point.value});
        points.push_back({sourceLength, last}); curve.points = std::move(points);
    };
    for (auto& curve : result.controls.automation) sliceCurve(curve);
    for (auto& lane : result.controls.pluginAutomation) sliceCurve(lane.curve);
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
TrackAnalysisRecord Media::analysisRecord() const { return {analysisSource, analysis, peaks, manualBpm, manualBeatOrigin, manualKey, waveformRms, analysisVersion}; }
std::shared_ptr<Media> Media::catalogueCopy() const
{
    auto result = std::make_shared<Media>(); result->id = id; result->name = name; result->file = file; result->assets = assets;
    result->sampleRate = sampleRate; result->analysis = analysis; result->analysisVersion = analysisVersion; result->analysisSource = analysisSource; result->sourceModified = sourceModified;
    result->analysisFromCache = analysisFromCache; result->analysisCacheWarning = analysisCacheWarning;
    result->manualBpm = manualBpm; result->manualBeatOrigin = manualBeatOrigin; result->manualKey = manualKey; result->peaks = peaks;
    result->waveformRms = waveformRms;
    result->sourceDetailsPending = sourceDetailsPending;
    result->analysisFileModified = analysisFileModified; result->analysisFileBytes = analysisFileBytes;
    return result;
}
MediaResult readMedia(juce::AudioFormatManager& formats, const juce::File& file, const MediaProgress& progress, AnalysisCacheMode mode, MediaReadPurpose purpose, const TrackAnalysisRecord* libraryRecord, const std::shared_ptr<std::atomic<bool>>& cancelled)
{
    const auto stopped = [&] { return cancelled && cancelled->load(); };
    if (stopped()) return {{}, "Audio loading cancelled."};
    if (progress) progress(MediaReadStage::lookupAnalysis);
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (!reader) return { {}, "Cannot decode: " + file.getFileName() + ". Try WAV, AIFF, FLAC, Ogg or MP3." };
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
    }
    auto& store = PcmStore::shared(); const auto pcmKey = PcmStore::decodedKey(m->analysisSource);
    const bool playback = purpose == MediaReadPurpose::playback;
    if (playback || !saved) m->cache = store.read(pcmKey, m->sampleRate, channels, (int) reader->lengthInSamples, mode != AnalysisCacheMode::readOnly);
    m->decodedFromCache = m->cache != nullptr;
    if (m->cache && progress) progress(MediaReadStage::savedAudio);
    if (stopped()) return {{}, "Audio loading cancelled."};
    if (!m->cache && (playback || !saved))
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
    if (saved && !m->cache && playback) for (int ch = 0; ch < channels; ++ch) for (int i = 0; i < frames; ++i)
        if (!std::isfinite(m->samples.getSample(ch, i))) return { {}, "Non-finite audio sample in " + file.getFileName() };
    if (saved)
    {
        if (progress) progress(MediaReadStage::savedAnalysis);
        m->analysis = std::move(saved->analysis); m->peaks = std::move(saved->peaks);
        m->waveformRms = std::move(saved->waveformRms);
        m->manualBpm = saved->manualBpm; m->manualBeatOrigin = saved->manualBeatOrigin; m->manualKey = saved->manualKey; m->analysisFromCache = true;
    }
    else { if (progress) progress(MediaReadStage::analyse); m->analysis = analyseMusic(m->audioView(), m->sampleRate, cancelled); }
    if (libraryMatches) { m->manualBpm = libraryRecord->manualBpm; m->manualBeatOrigin = libraryRecord->manualBeatOrigin; m->manualKey = libraryRecord->manualKey; }
    if (stopped()) return {{}, "Audio loading cancelled."};
    if (m->analysisSource.sha256.isNotEmpty() && audioDigest(file) != m->analysisSource.sha256) return { {}, "Audio changed while being read: " + file.getFullPathName() + ". Import it again." };
    if ((playback || !saved) && m->waveformRms.empty()) m->prepareWaveformBody();
    if (m->analysis.rhythmIssue.isNotEmpty()) m->analysisCacheWarning = m->analysis.rhythmIssue;
    else if (mode != AnalysisCacheMode::readOnly && (!saved || cached.legacyLocation)) m->analysisCacheWarning = writeAnalysisRecord(file, m->analysisRecord());
    if (progress) progress(MediaReadStage::cache);
    if (!m->cache && (playback || !saved) && mode != AnalysisCacheMode::readOnly) m->cache = store.write(pcmKey, m->sampleRate, m->samples, m->playbackCacheWarning);
    juce::String error; if (playback && !m->cache) m->cache = AudioCache::create(m->samples, error);
    if (playback && !m->cache) return { {}, file.getFileName() + ": " + error };
    m->samples.setSize(0, 0); // The OS pages retained PCM from disk; library size no longer reserves full decoded RAM.
    if (!playback) m->cache.reset(); // Catalogue entries retain facts/waveforms, not pinned PCM or temporary playback files.
    const auto recordFile = analysisRecordInputFile(file); m->analysisFileModified = recordFile.getLastModificationTime().toMilliseconds(); m->analysisFileBytes = recordFile.getSize();
    return { m, {} };
}
double snapTime(double value, double bpm, bool enabled)
{
    value = std::max(0.0, value);
    return enabled ? std::round(value * bpm / 60.0) * 60.0 / bpm : value;
}
juce::String timeText(double value)
{
    const auto ms = (int64_t) std::llround(std::max(0.0, value) * 1000);
    return juce::String::formatted("%02d:%02d.%03d", (int) (ms / 60000), (int) ((ms / 1000) % 60), (int) (ms % 1000));
}
juce::String midiActionId(MidiAction a)
{
    switch (a.kind)
    {
        case MidiActionKind::lowPass: return "low-pass-" + juce::String(a.index + 1);
        case MidiActionKind::deskPot: return "desk-pot-" + juce::String(a.index + 1);
        case MidiActionKind::cyclePot: return "desk-lane-" + juce::String(a.index + 1);
        case MidiActionKind::resetLowPass: return "reset-filter-" + juce::String(a.index + 1);
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
        for (auto kind : { MidiActionKind::volume, MidiActionKind::pan, MidiActionKind::mute, MidiActionKind::select, MidiActionKind::solo, MidiActionKind::bass, MidiActionKind::mid, MidiActionKind::treble, MidiActionKind::armTrack, MidiActionKind::resetEq, MidiActionKind::lowPass, MidiActionKind::resetLowPass }) result.push_back({kind, i});
    for (int i = 0; i < 8; ++i) { result.push_back({MidiActionKind::pluginParameter, i}); result.push_back({MidiActionKind::deskPot, i}); result.push_back({MidiActionKind::cyclePot, i}); }
    return result;
}
juce::Optional<MidiAction> parseMidiAction(const juce::String& id)
{
    for (auto action : midiActions()) if (midiActionId(action) == id) return action;
    return {};
}
const char* automationId(AutomationTarget target)
{
    switch (target) { case AutomationTarget::volume: return "volume"; case AutomationTarget::bass: return "bass"; case AutomationTarget::mid: return "mid"; case AutomationTarget::treble: return "treble"; case AutomationTarget::lowPass: return "low-pass"; case AutomationTarget::pan: return "pan"; case AutomationTarget::master: return "master"; case AutomationTarget::plugin: return "plugin"; }
    jassertfalse; return "volume";
}
const char* automationName(AutomationTarget target)
{
    switch (target) { case AutomationTarget::volume: return "Volume"; case AutomationTarget::bass: return "Bass / 120 Hz"; case AutomationTarget::mid: return "Mid / 1 kHz"; case AutomationTarget::treble: return "Treble / 8 kHz"; case AutomationTarget::lowPass: return "Low-pass / Hz"; case AutomationTarget::pan: return "Pan"; case AutomationTarget::master: return "Master volume"; case AutomationTarget::plugin: return "VST parameter"; }
    jassertfalse; return "Volume";
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
    const double fraction = (time - previous.time) / (next->time - previous.time);
    return (float) (previous.value + fraction * (next->value - previous.value));
}
int AutomationCurve::setPoint(double time, float value)
{
    if (!std::isfinite(time) || !std::isfinite(value)) return -1;
    time = juce::jlimit(0.0, 1.0e9, time);
    auto it = std::lower_bound(points.begin(), points.end(), time, [](const auto& p, double t) { return p.time < t; });
    const int index = (int) std::distance(points.begin(), it);
    if (it != points.end() && it->time == time) it->value = value; else points.insert(it, {time, value});
    return index;
}
void AutomationCurve::movePoint(int index, double time, float value)
{
    if (index < 0 || index >= (int) points.size() || !std::isfinite(time) || !std::isfinite(value)) return;
    // Precise point editing displays microseconds; keep a representable gap when
    // dragging against a neighbour instead of manufacturing adjacent doubles.
    const double low = index == 0 ? 0 : points[(size_t) index - 1].time + 0.000001;
    const double high = index + 1 == (int) points.size() ? 1.0e9 : points[(size_t) index + 1].time - 0.000001;
    points[(size_t) index] = {low <= high ? juce::jlimit(low, high, time) : points[(size_t) index].time, value};
}
juce::Range<float> automationRange(AutomationTarget target)
{
    return target == AutomationTarget::lowPass ? juce::Range<float>(20, 20000) : target == AutomationTarget::plugin ? juce::Range<float>(0, 1) : target == AutomationTarget::pan ? juce::Range<float>(-1, 1) : juce::Range<float>(-60, 12);
}
Clip* Project::clipFor(const juce::String& id)
{
    for (auto& c : clips) if (c.id == id) return &c;
    return nullptr;
}
const Clip* Project::clipFor(const juce::String& id) const { return const_cast<Project*>(this)->clipFor(id); }
TrackSettings* Project::settings(const AutomationAddress& address)
{
    if (address.clipId.isNotEmpty()) { auto* c = clipFor(address.clipId); return c ? &c->controls : nullptr; }
    return address.track >= 0 && address.track < trackCount ? &tracks[(size_t) address.track] : nullptr;
}
const TrackSettings* Project::settings(const AutomationAddress& address) const { return const_cast<Project*>(this)->settings(address); }
double Project::automationTime(const AutomationAddress& address, double time) const
{
    const auto* c = clipFor(address.clipId); return c ? c->sourceElapsed(time - c->start) : time;
}
double Project::automationSessionTime(const AutomationAddress& address, double time) const
{
    const auto* c = clipFor(address.clipId); return c ? c->start + c->localTimeForSource(time) : time;
}
void Project::automationEdited(const AutomationAddress& address, AutomationEdit edit, double removedSourceTime)
{
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
const AutomationCurve* Project::curve(const AutomationAddress& address) const { return const_cast<Project*>(this)->curve(address); }
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
        case AutomationTarget::lowPass: t->lowPassHz = value; break;
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
    for (const auto& point : curve.points) { auto* node = new juce::DynamicObject(); node->setProperty("time", point.time); node->setProperty("value", point.value); points.add(juce::var(node)); }
    lane->setProperty("points", points); return juce::var(lane);
}
juce::var controlsToJson(const TrackSettings& t)
{
    auto* obj = new juce::DynamicObject(); obj->setProperty("gainDb", t.gainDb); obj->setProperty("pan", t.pan);
    juce::Array<juce::var> eq, controls, lanes;
    obj->setProperty("lowPassHz", t.lowPassHz);
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
    root->setProperty("keyDisplay", p.keyDisplay);
    root->setProperty("masterAutomation", curveToJson(p.masterAutomation));
    root->setProperty("mastering", masterSettingsToJson(p.mastering));
    root->setProperty("effectTailSeconds", p.effectTailSeconds);
    root->setProperty("rehearsalContext", rehearsalContextToJson(p.rehearsalContext));
    root->setProperty("followTrackTempo", p.followTrackTempo);
    root->setProperty("transitionPresets", namedTransitionStylesToJson(p.transitionPresets));
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
        obj->setProperty("automaticBass", c.automaticBass); obj->setProperty("controls", controlsToJson(c.controls)); obj->setProperty("plugins", juce::Array<juce::var>());
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
        if (!point.isObject() || !number(point["time"], 0, 1.0e9) || !number(value, range.getStart(), range.getEnd()) || (double) point["time"] <= previous) return false;
        previous = (double) point["time"]; curve.points.push_back({previous, (float) value});
    }
    return true;
}
bool parseClipControls(const juce::var& c, Clip& clip, std::set<juce::String>& pluginIds, bool hasFilter)
{
    const auto& t = c["controls"]; auto& target = clip.controls;
    auto* plugins = c["plugins"].getArray(); auto* eq = t["eqDb"].getArray(); auto* controls = t["pluginControls"].getArray(); auto* lanes = t["pluginAutomation"].getArray();
    if (!t.isObject() || !number(t["gainDb"], -60, 12) || !number(t["pan"], -1, 1) || !plugins || !eq || eq->size() != 3 || !controls || controls->size() != 8 || !lanes || !t["automation"].isObject()) return false;
    target.gainDb = (float) t["gainDb"]; target.pan = (float) t["pan"];
    if (hasFilter) { if (!number(t["lowPassHz"], 20, 20000)) return false; target.lowPassHz = (float) t["lowPassHz"]; }
    for (int n = 0; n < 3; ++n) { if (!number((*eq)[n], -60, 12)) return false; target.eqDb[(size_t) n] = (float) (*eq)[n]; }
    for (int n = 0; n < (hasFilter ? trackAutomationCount : 5); ++n)
        if (!parseCurve(t["automation"][automationId((AutomationTarget) n)], target.automation[(size_t) n], automationRange((AutomationTarget) n), false)) return false;
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
    r.project.tempo.fallback = r.project.bpm;
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
    if (tracks == nullptr || tracks->size() != savedTrackCount || clips == nullptr) return fail("Invalid tracks or clips.");
    std::set<juce::String> pluginIds;
    for (int i = 0; i < savedTrackCount; ++i)
    {
        const auto& t = (*tracks)[i];
        if (!t.isObject() || !t["name"].isString() || !number(t["gainDb"], -60, 12) || !number(t["pan"], -1, 1) || !t["mute"].isBool() || !t["solo"].isBool() || !t["plugins"].isArray()) return fail("Invalid track settings.");
        auto& target = r.project.tracks[(size_t) i];
        if ((int) d["version"] >= 23) { if (!number(t["lowPassHz"], 20, 20000)) return fail("Invalid lane low-pass cutoff."); target.lowPassHz = (float) t["lowPassHz"]; }
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
                if (!parseCurve(t["automation"][automationId((AutomationTarget) n)], target.automation[(size_t) n], automationRange((AutomationTarget) n), !extendedAutomation))
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
            || !number(c["fadeIn"], 0, (int) d["version"] >= 9 && !c[(int) d["version"] >= 10 ? "fadeInAnchor" : "introAnchor"].isVoid() ? 1.0e9 : (double) c["length"]) || !number(c["fadeOut"], 0, (int) d["version"] >= 9 && !c[(int) d["version"] >= 10 ? "fadeOutAnchor" : "outroAnchor"].isVoid() ? 1.0e9 : (double) c["length"]) || !number(c["gainDb"], -60, 12)) return fail("Invalid clip settings.");
        ClipReference reference; auto& clip = reference.clip;
        clip.id = c["id"].toString(); clip.track = (int) c["track"]; clip.start = (double) c["start"]; clip.offset = (double) c["offset"]; clip.length = (double) c["length"];
        if ((int) d["version"] >= 22)
        {
            if (!number(c["audioSlip"], -1.0e9, 1.0e9)) return fail("Invalid audio slip.");
            clip.audioSlip = (double) c["audioSlip"];
        }
        if ((int) d["version"] >= 11 && (!c["automaticBass"].isBool() || !parseClipControls(c, clip, pluginIds, (int) d["version"] >= 23))) return fail("Invalid audio-track automation or effects.");
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
                    return fail("The captured source clock does not match saved clip geometry.");
            }
        }
        if ((int) d["version"] >= 8)
        {
            if (!c["elasticPlayback"].isBool()) return fail("Invalid clip playback mode.");
            clip.elasticPlayback = (bool) c["elasticPlayback"];
            if (clip.elasticPlayback && (!clip.beatmatched || !r.project.followTrackTempo || r.project.tempo.points.empty())) return fail("Elastic playback requires a matched clip and a shared tempo map.");
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
