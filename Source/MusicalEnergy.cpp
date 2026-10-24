#include "MusicalEnergy.h"
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <cmath>

namespace mixworks
{
namespace
{
double unit(double x) { return juce::jlimit(0., 1., x); }
bool bounded(double x, double high) { return std::isfinite(x) && x >= 0 && x <= high; }
double percentile(std::vector<double> values, double fraction)
{
    if (values.empty()) return 0;
    const auto index = (size_t) std::floor(fraction * (values.size() - 1));
    std::nth_element(values.begin(), values.begin() + (ptrdiff_t) index, values.end());
    return values[index];
}
struct Frame { double power = 0, bass = 0, high = 0, peak = 0; };
bool number(const juce::var& x, double high)
{ return (x.isDouble() || x.isInt() || x.isInt64()) && bounded((double) x, high); }
}
double MusicalEnergyWindow::rhythm() const { return unit(onsetsPerSecond / 6); }
double MusicalEnergyWindow::bass() const
{ return std::sqrt(bassRatio) * (.2 * rhythm() + .8 * unit(bassOnsetsPerSecond / 3)); }
double MusicalEnergyWindow::dynamics() const
{
    // Punch modulates existing activity; a wide-range pad is not a busy beat.
    return (.6 * unit((crestDb - 3) / 15) + .4 * unit(dynamicRangeDb / 24)) * unit(rhythm() * 2);
}
double MusicalEnergyWindow::score() const { return .6 * rhythm() + .25 * bass() + .15 * dynamics(); }
bool MusicalEnergyProfile::complete() const
{
    if (revision != musicalEnergyRevision || !std::isfinite(duration) || duration < .25 || duration > 10800
        || windows.empty() || windows.size() > 1351) return false;
    double end = 0;
    for (const auto& w : windows)
    {
        if (!std::isfinite(w.start) || !std::isfinite(w.end) || std::abs(w.start - end) > 1e-6
            || w.end <= w.start || w.end - w.start > 8.001 || w.end > duration + 1e-6
            || !bounded(w.coverage, 1) || !bounded(w.onsetsPerSecond, 50) || !bounded(w.bassOnsetsPerSecond, 50)
            || !bounded(w.bassRatio, 1) || !bounded(w.crestDb, 120) || !bounded(w.dynamicRangeDb, 120)) return false;
        end = w.end;
    }
    return std::abs(end - duration) <= 1e-6;
}
MusicalEnergyProfile analyseMusicalEnergy(const PcmView& pcm, double rate, const std::shared_ptr<std::atomic<bool>>& cancelled)
{
    const auto stopped = [&] { return cancelled && cancelled->load(); };
    const int channels = pcm.getNumChannels(), samples = pcm.getNumSamples();
    const double duration = samples / rate;
    if (!std::isfinite(rate) || rate < 2000 || rate > 384000 || channels < 1 || channels > 2
        || !std::isfinite(duration) || duration < .25 || duration > 10800 || stopped()) return {};
    const int hop = std::max(1, (int) std::round(rate * .02));
    struct Bands { juce::dsp::IIR::Filter<float> dc, bass, high; };
    Bands filters[2];
    for (auto& f : filters)
    {
        f.dc.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(rate, 35.f);
        f.bass.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(rate, 180.f);
        f.high.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(rate, (float) std::min(1000., rate * .35));
    }
    std::vector<Frame> frames; frames.reserve((size_t) ((samples + (int64_t) hop - 1) / hop));
    for (int first = 0; first < samples;)
    {
        if (stopped()) return {};
        const int last = (int) std::min((int64_t) samples, (int64_t) first + hop);
        Frame frame;
        for (int i = first; i < last; ++i)
        {
            double total = 0;
            for (int ch = 0; ch < channels; ++ch)
            {
                const float raw = pcm.getSample(ch, i); if (!std::isfinite(raw)) return {};
                auto& f = filters[ch]; const double full = f.dc.processSample(raw);
                const double bass = f.bass.processSample((float) full), high = f.high.processSample((float) full);
                total += full * full / channels;
                frame.bass += bass * bass / channels; frame.high += high * high / channels;
            }
            frame.power += total; frame.peak = std::max(frame.peak, total);
        }
        frame.power /= last - first; frame.bass /= last - first; frame.high /= last - first;
        if (!std::isfinite(frame.power) || !std::isfinite(frame.bass) || !std::isfinite(frame.high) || !std::isfinite(frame.peak)) return {};
        frames.push_back(frame);
        first = last;
    }
    std::vector<double> powers, bassPowers, highPowers;
    powers.reserve(frames.size()); bassPowers.reserve(frames.size()); highPowers.reserve(frames.size());
    for (const auto& f : frames) { powers.push_back(f.power); bassPowers.push_back(f.bass); highPowers.push_back(f.high); }
    const double reference = percentile(powers, .95);
    const double floor = std::max(1e-24, reference * 1e-6);
    const double fullScale = std::sqrt(std::max(reference, 1e-24));
    const double bassScale = std::sqrt(std::max(percentile(bassPowers, .95), reference * .01 + 1e-24));
    const double highScale = std::sqrt(std::max(percentile(highPowers, .95), reference * .01 + 1e-24));
    std::vector<double> novelty(frames.size()), bassNovelty(frames.size());
    for (size_t i = 2; i < frames.size(); ++i)
    {
        if ((i & 1023) == 0 && stopped()) return {};
        const auto& a = frames[i - 1]; const auto& b = frames[i];
        bassNovelty[i] = std::max(0., std::sqrt(b.bass) - std::sqrt(a.bass)) / bassScale;
        novelty[i] = .5 * std::max(0., std::sqrt(b.power) - std::sqrt(a.power)) / fullScale
            + .3 * std::max(0., std::sqrt(b.high) - std::sqrt(a.high)) / highScale + .2 * bassNovelty[i];
    }
    MusicalEnergyProfile profile; profile.revision = musicalEnergyRevision; profile.duration = duration;
    const double frameSeconds = hop / rate;
    int previousOnset = -1000, previousBass = -1000;
    for (double start = 0; start < duration - 1e-9; start += 8)
    {
        if (stopped()) return {};
        MusicalEnergyWindow w; w.start = start; w.end = std::min(start + 8, duration);
        const size_t first = (size_t) std::ceil(start / frameSeconds - 1e-8);
        const size_t last = std::min(frames.size(), (size_t) std::ceil(w.end / frameSeconds - 1e-8));
        double total = 0, bass = 0; int active = 0, onsets = 0, bassOnsets = 0;
        std::vector<double> levels, crests;
        for (size_t i = first; i < last; ++i)
        {
            const auto& f = frames[i];
            if (f.power <= floor || reference <= 1e-16) continue;
            ++active; total += f.power; bass += f.bass; levels.push_back(f.power);
            crests.push_back(10 * std::log10(std::max(f.peak, f.power) / f.power));
            const auto peak = [&](const std::vector<double>& values, double threshold, int previous)
            { return i > 0 && i + 1 < frames.size() && values[i] > threshold && values[i] >= values[i - 1]
                && values[i] > values[i + 1] && ((int) i - previous) * frameSeconds >= .08; };
            if (peak(novelty, .08, previousOnset)) { ++onsets; previousOnset = (int) i; }
            if (peak(bassNovelty, .12, previousBass)) { ++bassOnsets; previousBass = (int) i; }
        }
        w.coverage = last > first ? (double) active / (last - first) : 0;
        // Counts use elapsed window time, so long empty gaps cannot inflate density.
        w.onsetsPerSecond = onsets / (w.end - start); w.bassOnsetsPerSecond = bassOnsets / (w.end - start);
        w.bassRatio = total > 0 ? unit(bass / total) : 0;
        if (!levels.empty())
        {
            w.crestDb = juce::jlimit(0., 120., percentile(crests, .9));
            w.dynamicRangeDb = juce::jlimit(0., 120., 10 * std::log10(std::max(percentile(levels, .9), floor) / std::max(percentile(levels, .1), floor)));
        }
        profile.windows.push_back(w);
    }
    return !stopped() && profile.complete() ? profile : MusicalEnergyProfile{};
}
std::optional<double> musicalEnergyAt(const MusicalEnergyProfile& profile, double begin, double end)
{
    if (!profile.complete() || !std::isfinite(begin) || !std::isfinite(end) || begin < -1e-7 || end > profile.duration + 1e-7 || end <= begin) return {};
    double sum = 0, weight = 0, support = 0;
    for (const auto& w : profile.windows)
    {
        const double span = std::max(0., std::min(end, w.end) - std::max(begin, w.start));
        sum += span * w.score(); support += span * w.coverage; weight += span;
    }
    return weight > 0 && support / weight >= .1 ? std::optional<double>(sum / weight) : std::nullopt;
}
std::optional<double> musicalEnergyBody(const MusicalEnergyProfile& profile)
{ return musicalEnergyAt(profile, profile.duration * .1, profile.duration * .9); }
juce::var musicalEnergyToJson(const MusicalEnergyProfile& profile)
{
    if (!profile.complete()) return {};
    auto* object = new juce::DynamicObject(); object->setProperty("revision", profile.revision); object->setProperty("duration", profile.duration);
    juce::Array<juce::var> rows;
    for (const auto& w : profile.windows)
    {
        juce::Array<juce::var> row; for (double x : {w.start, w.end, w.coverage, w.onsetsPerSecond, w.bassOnsetsPerSecond, w.bassRatio, w.crestDb, w.dynamicRangeDb}) row.add(x);
        rows.add(row);
    }
    object->setProperty("windows", rows); return juce::var(object);
}
MusicalEnergyProfile parseMusicalEnergy(const juce::var& value, double duration)
{
    if (!value.isObject() || !value["revision"].isInt() || (int) value["revision"] != musicalEnergyRevision
        || !number(value["duration"], 10800) || std::abs((double) value["duration"] - duration) > 1e-6
        || !value["windows"].isArray() || value["windows"].size() > 1351) return {};
    MusicalEnergyProfile result; result.revision = musicalEnergyRevision; result.duration = duration;
    for (const auto& row : *value["windows"].getArray())
    {
        if (!row.isArray() || row.size() != 8) return {};
        for (int i = 0; i < 8; ++i) if (!number(row[i], i < 2 ? 10800 : i == 2 || i == 5 ? 1 : i < 5 ? 50 : 120)) return {};
        result.windows.push_back({(double) row[0], (double) row[1], (double) row[2], (double) row[3], (double) row[4], (double) row[5], (double) row[6], (double) row[7]});
    }
    return result.complete() ? result : MusicalEnergyProfile{};
}
}
