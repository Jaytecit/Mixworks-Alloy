#include "GridCorrection.h"
#include <cerrno>
#include <cstdlib>

namespace mixworks
{
SourceBeatGrid resolveSourceGrid(const MusicAnalysis& analysis, double manualBpm, double manualBeatOrigin)
{
    const double bpm = manualBpm > 0 ? manualBpm : analysis.bpm;
    const double downbeat = manualBeatOrigin >= 0 ? manualBeatOrigin
        : manualBpm > 0 && std::abs(manualBpm - analysis.bpm) > 0.01 ? -1 : analysis.downbeatOrigin;
    return {bpm, manualBeatOrigin >= 0 ? manualBeatOrigin : analysis.beatOrigin, downbeat};
}
double SourceBeatGrid::beatAt(double sourceSeconds) const
{
    return hasPulse() ? (sourceSeconds - (hasBar() ? downbeatOrigin : pulseOrigin)) * bpm / 60 : 0;
}
double SourceBeatGrid::timeAtBeat(double beat) const
{
    return hasPulse() ? (hasBar() ? downbeatOrigin : pulseOrigin) + beat * 60 / bpm : 0;
}
float SourceBeatGrid::clickAt(double sourceSeconds) const
{
    if (!hasPulse() || !std::isfinite(sourceSeconds)) return 0;
    const double phase = beatAt(sourceSeconds); if (!std::isfinite(phase)) return 0;
    const double beat = std::floor(phase + 1.0e-9);
    const double elapsed = sourceSeconds - timeAtBeat(beat);
    if (elapsed < 0 || elapsed >= 0.012) return 0;
    const bool accent = hasBar() && std::abs(std::remainder(beat, 4.0)) < 0.1;
    return (float) ((accent ? 0.18 : 0.10) * std::sin(juce::MathConstants<double>::twoPi * (accent ? 1500 : 1000) * elapsed)
                    * std::exp(-elapsed * 350));
}
juce::Result SourceCorrection::validate(double duration) const
{
    if (!std::isfinite(duration) || duration <= 0 || !std::isfinite(bpm) || (bpm != 0 && (bpm < 20 || bpm > 400))
        || !std::isfinite(downbeat) || (downbeat != -1 && (downbeat < 0 || downbeat >= duration)) || key < -1 || key > 23)
        return juce::Result::fail("Use BPM 20-400 or automatic, and beat one inside the source audio. Current corrections were kept.");
    return juce::Result::ok();
}
juce::Result SourceCorrection::parse(const juce::String& bpmText, const juce::String& phaseText, int key, double duration, SourceCorrection& output)
{
    SourceCorrection candidate {0, -1, key};
    const auto number = [](const juce::String& text, double& value)
    {
        const auto trimmed = text.trim(); if (trimmed.isEmpty()) return true;
        auto utf8 = trimmed.toRawUTF8(); char* end = nullptr; errno = 0;
        const double parsed = std::strtod(utf8, &end);
        if (end == utf8 || *end != '\0' || errno == ERANGE || !std::isfinite(parsed)) return false;
        value = parsed; return true;
    };
    if (!number(bpmText, candidate.bpm) || !number(phaseText, candidate.downbeat))
        return juce::Result::fail("Enter one finite number in each field, or leave it blank for automatic detection.");
    // Explicit zero BPM and negative phase are not the empty-field sentinel.
    if ((!bpmText.trim().isEmpty() && candidate.bpm == 0) || (!phaseText.trim().isEmpty() && candidate.downbeat < 0))
        return juce::Result::fail("Leave a field blank to restore automatic detection.");
    const auto valid = candidate.validate(duration); if (valid.failed()) return valid;
    output = candidate; return juce::Result::ok();
}
std::optional<double> TapTempo::tap(double time)
{
    if (!std::isfinite(time)) return {};
    if (!times.empty())
    {
        const double interval = time - times.back();
        if (interval <= 0 || interval > 3) times.clear();
        else if (interval < 0.15) return {};
    }
    times.push_back(time); if (times.size() > 9) times.erase(times.begin());
    if (times.size() < 3) return {};
    std::vector<double> intervals;
    for (size_t i = 1; i < times.size(); ++i) intervals.push_back(times[i] - times[i - 1]);
    std::sort(intervals.begin(), intervals.end());
    const size_t mid = intervals.size() / 2;
    const double median = intervals.size() % 2 ? intervals[mid] : (intervals[mid - 1] + intervals[mid]) / 2;
    const double bpm = 60 / median;
    return bpm >= 20 && bpm <= 400 ? std::optional<double>(bpm) : std::nullopt;
}
juce::Result GridAudition::start(std::shared_ptr<const Media> input, SourceBeatGrid sourceGrid, double at, bool source, bool click, float gainDb)
{
    if (!input || !input->hasPlayback() || input->channels() < 1 || input->frames() < 1 || !std::isfinite(input->sampleRate)
        || input->sampleRate <= 0 || !std::isfinite(at) || at < 0 || at >= input->duration())
        return juce::Result::fail("Source audio is not ready, or the audition cursor is outside it.");
    media = std::move(input); cursor = at; configure(sourceGrid, source, click, gainDb); running = true;
    return juce::Result::ok();
}
void GridAudition::configure(SourceBeatGrid sourceGrid, bool source, bool click, float gainDb)
{
    grid = sourceGrid; sourceOn = source; clickOn = click;
    gain = juce::Decibels::decibelsToGain(std::isfinite(gainDb) ? juce::jlimit(-36.0f, -3.0f, gainDb) : -12.0f);
}
void GridAudition::stop() { running = false; media.reset(); }
void GridAudition::render(juce::AudioBuffer<float>& output, int frames, double rate)
{
    output.clear();
    if (!running || !media || !std::isfinite(rate) || rate <= 0) return;
    const float* channels[2] {media->channelData(0), media->channelData(std::min(1, media->channels() - 1))};
    for (int i = 0; i < std::min(frames, output.getNumSamples()); ++i)
    {
        if (cursor >= media->duration()) { running = false; break; }
        const double sample = cursor * media->sampleRate;
        const int first = juce::jlimit(0, media->frames() - 1, (int) sample), next = std::min(first + 1, media->frames() - 1);
        const float fraction = (float) (sample - first), click = clickOn ? grid.clickAt(cursor) : 0;
        for (int ch = 0; ch < std::min(2, output.getNumChannels()); ++ch)
            output.setSample(ch, i, (sourceOn ? (channels[ch][first] + fraction * (channels[ch][next] - channels[ch][first])) * gain : 0) + click);
        cursor += 1 / rate;
    }
}
}
