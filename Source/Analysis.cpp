#include "Analysis.h"
#include "NeuralRhythm.h"
#include <deque>
#include <complex>
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <limits>
#include <set>
#include <map>

namespace mixworks
{
namespace
{
constexpr double analysisRate = 11025;
constexpr double pi = juce::MathConstants<double>::pi;
struct Segment { std::vector<float> mono; double sourceStart = 0, tonalWeight = 1; };
struct Transient { double time = 0, novelty = 0, attackContrast = 0, attackSharpness = 0; };
struct Pulse
{
    double bpm = 0, origin = 0, confidence = 0;
    std::vector<Transient> transients;
    double coverageSeconds = 0;
    bool independentPhysical = false;
    double sourceStart = 0;
};
std::pair<double, double> refineAttack(const std::vector<float>& signal, int centre, int radius)
{
    constexpr int step = 22; int strongest = centre; double strength = 0;
    for (int i = std::max(step, centre - radius); i < std::min((int) signal.size() - step, centre + radius); i += step)
    {
        double rise = 0;
        for (int j = 0; j < step; ++j) rise += std::abs(signal[(size_t) (i + j)]) - std::abs(signal[(size_t) (i - j - 1)]);
        if (rise > strength) { strength = rise; strongest = i; }
    }
    return {strongest / analysisRate, strength};
}
bool fitGrid(const std::vector<std::pair<double, double>>& beats, double& period, double& origin)
{
    if (beats.size() < 6) return false;
    double meanN = 0, meanT = 0;
    for (const auto& b : beats) { meanN += b.first; meanT += b.second; }
    meanN /= beats.size(); meanT /= beats.size();
    double nt = 0, nn = 0;
    for (const auto& b : beats) { nt += (b.first - meanN) * (b.second - meanT); nn += (b.first - meanN) * (b.first - meanN); }
    if (nn <= 0) return false;
    const double fitted = nt / nn;
    if (std::abs(fitted - period) > period * 0.04) return false;
    period = fitted; origin = meanT - period * meanN; return true;
}
double interpolate(const std::vector<float>& v, double pos)
{
    if (pos < 0 || pos >= (double) v.size() - 1) return 0;
    auto index = (size_t) pos; return v[index] + (pos - index) * (v[index + 1] - v[index]);
}
Segment monoSegment(const PcmView& samples, double rate, double start, double length, bool side = false, const std::shared_ptr<std::atomic<bool>>& cancelled = {})
{
    Segment part; part.sourceStart = start;
    if (cancelled && *cancelled) return part;
    part.mono.resize((size_t) std::floor(length * analysisRate));
    // A lowpass before decimation avoids folding high-frequency percussion into chroma.
    juce::dsp::IIR::Filter<float> filter;
    filter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(rate, (float) std::min(4500.0, rate * 0.4));
    const int begin = (int) (part.sourceStart * rate), end = std::min(samples.getNumSamples(), begin + (int) std::ceil(length * rate));
    double leftEnergy = 0, rightEnergy = 0, cross = 0;
    if (samples.getNumChannels() >= 2)
        for (int i = begin; i < end; i += std::max(1, (end - begin) / 4096)) { double a = samples.getSample(0, i), b = samples.getSample(1, i); leftEnergy += a * a; rightEnergy += b * b; cross += a * b; }
    const bool antiPhase = leftEnergy * rightEnergy > 0 && cross / std::sqrt(leftEnergy * rightEnergy) < -0.2;
    float previous = 0; size_t out = 0;
    for (int i = begin; i < end && out < part.mono.size(); ++i)
    {
        if ((i & 4095) == 0 && cancelled && *cancelled) return {};
        float value = 0;
        if (side && samples.getNumChannels() >= 2) value = (samples.getSample(0, i) - samples.getSample(1, i)) * 0.5f;
        else if (antiPhase) value = samples.getSample(leftEnergy >= rightEnergy ? 0 : 1, i);
        else for (int ch = 0; ch < samples.getNumChannels(); ++ch) value += samples.getSample(ch, i) / samples.getNumChannels();
        const float current = filter.processSample(value);
        while (out < part.mono.size() && (double) out * rate / analysisRate <= i - begin)
        {
            double fraction = (double) out * rate / analysisRate - (i - begin - 1);
            part.mono[out++] = previous + (float) juce::jlimit(0.0, 1.0, fraction) * (current - previous);
        }
        previous = current;
    }
    return part;
}
std::vector<Segment> segments(const PcmView& samples, double rate, const std::shared_ptr<std::atomic<bool>>& cancelled)
{
    const double duration = samples.getNumSamples() / rate;
    std::vector<Segment> result;
    const int count = duration > 180 ? maxAnalysisWindows : duration > 75 ? 3 : 1;
    const double length = count == 1 ? duration : std::min(180.0 / count, duration / count);
    // Distribute the same 180-second budget across long tracks. Three windows
    // can give one syncopated breakdown enough weight to veto the drum sections.
    for (int s = 0; s < count; ++s)
        result.push_back(monoSegment(samples, rate, count == 1 ? 0 : (duration - length) * s / (count - 1), length, false, cancelled));
    return result;
}
Pulse pulse(const Segment& segment, bool percussive = false, const std::vector<float>* timingSignal = nullptr)
{
    const int size = percussive ? 110 : 1024, hop = percussive ? 22 : 128;
    Pulse result; result.coverageSeconds = segment.mono.size() / analysisRate; result.independentPhysical = percussive; result.sourceStart = segment.sourceStart;
    const auto& timing = timingSignal ? *timingSignal : segment.mono;
    if (segment.mono.size() < (size_t) (analysisRate * 5)) return result;
    juce::dsp::FFT fft(10); juce::dsp::WindowingFunction<float> window(size, juce::dsp::WindowingFunction<float>::hann);
    std::vector<float> spectrum(size * 2), previous(size / 2), onset;
    std::vector<double> energy;
    if (percussive)
    {
        for (int pos = 0; pos + size <= (int) segment.mono.size(); pos += hop)
        {
            double sum = 0; for (int i = 0; i < size; ++i) sum += segment.mono[(size_t) (pos + i)] * segment.mono[(size_t) (pos + i)];
            energy.push_back(std::sqrt(sum / size));
            const size_t i = energy.size() - 1;
            onset.push_back((float) std::max(0.0, energy[i] - (i >= 6 ? energy[i - 6] : 0.0)));
        }
    }
    else for (int pos = 0; pos + size <= (int) segment.mono.size(); pos += hop)
    {
        std::fill(spectrum.begin(), spectrum.end(), 0.0f); std::copy_n(segment.mono.data() + pos, size, spectrum.data());
        window.multiplyWithWindowingTable(spectrum.data(), size); fft.performFrequencyOnlyForwardTransform(spectrum.data());
        double flux = 0;
        for (int bin = 3; bin < 400; ++bin)
        {
            float magnitude = std::log1p(20 * spectrum[(size_t) bin]);
            flux += std::max(0.0f, magnitude - previous[(size_t) bin]) * (bin < 24 ? 3.0 : 1.0);
            previous[(size_t) bin] = magnitude;
        }
        onset.push_back((float) flux);
    }
    double total = std::accumulate(onset.begin(), onset.end(), 0.0);
    if (total < 1.0e-5) return result;
    auto original = onset;
    for (int i = 0; i < (int) onset.size(); ++i)
    {
        double mean = 0; int count = 0;
        const int background = percussive ? 80 : 16;
        for (int j = std::max(0, i - background); j <= std::min((int) onset.size() - 1, i + background); ++j) { mean += original[(size_t) j]; ++count; }
        onset[(size_t) i] = (float) std::max(0.0, (double) original[(size_t) i] - mean / count);
    }
    const double frameRate = analysisRate / hop;
    // These independent rises belong to the measured source, even when no
    // single scalar pulse can explain it. Preserve them before fitting a grid.
    if (percussive)
    {
        const float threshold = *std::max_element(onset.begin(), onset.end()) * 0.06f;
        for (int pos = 1; pos + 1 < (int) onset.size(); ++pos)
        {
            if (onset[(size_t) pos] < threshold || onset[(size_t) pos] < onset[(size_t) pos - 1] || onset[(size_t) pos] <= onset[(size_t) pos + 1]) continue;
            int strongest = pos;
            const int end = std::min((int) onset.size() - 1, pos + (int) (0.05 * frameRate));
            for (int j = pos + 1; j <= end; ++j) if (onset[(size_t) j] > onset[(size_t) strongest]) strongest = j;
            const auto attack = refineAttack(timing, strongest * hop + size / 2, (int) (0.020 * analysisRate));
            if (attack.second > 0.001) result.transients.push_back({attack.first + segment.sourceStart, onset[(size_t) strongest],
                onset[(size_t) strongest] * onset[(size_t) strongest] / std::max(1.0e-12, energy[(size_t) strongest]),
                juce::jlimit(0., 1., (energy[(size_t) strongest] - (strongest >= 6 ? energy[(size_t) strongest - 6] : 0.))
                    / std::max(1.0e-12, energy[(size_t) strongest]))});
            pos = strongest + (int) (0.05 * frameRate);
        }
    }

    const int minLag = (int) (60 * frameRate / 200), maxLag = (int) std::ceil(60 * frameRate / 65);
    std::vector<double> ac((size_t) maxLag + 2);
    for (int lag = minLag - 1; lag <= maxLag + 1; ++lag)
    {
        double cross = 0, a = 0, b = 0;
        for (int i = lag; i < (int) onset.size(); ++i) { double x = onset[(size_t) i], y = onset[(size_t) (i - lag)]; cross += x * y; a += x * x; b += y * y; }
        ac[(size_t) lag] = a * b > 0 ? cross / std::sqrt(a * b) : 0;
    }
    int best = minLag;
    for (int lag = minLag; lag <= maxLag; ++lag)
        if (ac[(size_t) lag] * (1.0 + 0.04 * minLag / lag) > ac[(size_t) best] * (1.0 + 0.04 * minLag / best)) best = lag;
    if (ac[(size_t) best] < 0.15) return result;
    double denominator = ac[(size_t) best - 1] - 2 * ac[(size_t) best] + ac[(size_t) best + 1];
    double lag = best + (std::abs(denominator) > 1.0e-12 ? juce::jlimit(-0.5, 0.5, 0.5 * (ac[(size_t) best - 1] - ac[(size_t) best + 1]) / denominator) : 0);
    double period = lag / frameRate;
    double phase = 0, phaseScore = -1;
    for (int p = 0; p < 160; ++p)
    {
        double candidate = p * period / 160, score = 0;
        for (double t = candidate; t * frameRate < onset.size() - 1; t += period) score += interpolate(onset, t * frameRate);
        if (score > phaseScore) { phaseScore = score; phase = candidate; }
    }
    // A double-length autocorrelation peak may win simply because its lag lands
    // closer to an integer FFT hop. Prefer the shorter pulse when it explains
    // equally strong, otherwise omitted, intervening onsets.
    if (period * 0.5 >= 60.0 / 200)
    {
        auto localPeak = [&](double t)
        {
            int centre = (int) std::round(t * frameRate); double peak = 0;
            for (int j = std::max(0, centre - 2); j <= std::min((int) onset.size() - 1, centre + 2); ++j) peak = std::max(peak, (double) onset[(size_t) j]);
            return peak;
        };
        double on = 0, between = 0;
        for (double t = phase; (t + period) * frameRate < onset.size() - 1; t += period) { on += localPeak(t); between += localPeak(t + period * 0.5); }
        if (on > 0 && between / on > 0.75) period *= 0.5;
    }
    // Fit actual onset maxima to beat ordinals; long-baseline regression refines sub-frame BPM.
    double sumN = 0, sumT = 0, sumNN = 0, sumNT = 0; int count = 0;
    std::vector<std::pair<int, double>> beats;
    for (int n = 0; (phase + n * period) * frameRate < onset.size() - 1; ++n)
    {
        int predicted = (int) std::round((phase + n * period) * frameRate), peak = juce::jlimit(0, (int) onset.size() - 1, predicted);
        const int radius = std::max(1, (int) (period * frameRate * 0.18));
        for (int j = std::max(1, predicted - radius); j <= std::min((int) onset.size() - 2, predicted + radius); ++j) if (onset[(size_t) j] > onset[(size_t) peak]) peak = j;
        if (onset[(size_t) peak] < total / original.size() * 0.2) continue;
        const double t = peak / frameRate;
        sumN += n; sumT += t; sumNN += n * n; sumNT += n * t; ++count; beats.push_back({n, t});
    }
    double d = count * sumNN - sumN * sumN;
    if (count < 6 || d <= 0) return result;
    const double fitted = (count * sumNT - sumN * sumT) / d;
    if (std::abs(fitted - period) < period * 0.04) { period = fitted; phase = (sumT - period * sumN) / count; }
    // Spectral-flux windows look ahead; find the sharp time-domain rise near each grid pulse.
    std::vector<double> corrections;
    std::vector<std::pair<double, double>> timeBeats;
    for (const auto& beat : beats)
    {
        const int centre = (int) ((beat.second + size * 0.5 / analysisRate) * analysisRate);
        const auto attack = refineAttack(timing, centre, (int) ((percussive ? 0.020 : 0.055) * analysisRate));
        if (attack.second > 0.001)
        {
            const double time = attack.first;
            corrections.push_back(time - (phase + beat.first * period));
            timeBeats.push_back({beat.first, time});
        }
    }
    if (!corrections.empty()) { std::sort(corrections.begin(), corrections.end()); phase += corrections[corrections.size() / 2]; }
    // Refit at actual transient times, excluding off-grid accents. The FFT fit above
    // selects the pulse; its window-centre bias must not define the final grid.
    std::vector<std::pair<double, double>> inliers;
    for (const auto& b : timeBeats) if (std::abs(b.second - (phase + b.first * period)) < period * 0.12) inliers.push_back(b);
    if (inliers.size() < 6) return result;
    fitGrid(inliers, period, phase);
    double residual = 0; for (const auto& b : inliers) residual += std::pow(b.second - (phase + b.first * period), 2);
    double confidence = ac[(size_t) best] * std::min(1.0, count * period / (segment.mono.size() / analysisRate));
    confidence *= std::exp(-std::sqrt(residual / std::max((size_t) 1, inliers.size())) / (period * 0.15));
    result.bpm = 60.0 / period; result.origin = phase + segment.sourceStart;
    result.confidence = juce::jlimit(0.0, 1.0, confidence);
    if (!percussive) for (const auto& beat : inliers) result.transients.push_back({beat.second + segment.sourceStart, 1.0});
    return result;
}
Pulse percussionPulse(const Segment& part)
{
    // The low band proposes attacks; their timing belongs to the original
    // waveform. Share this boundary between distributed and targeted windows.
    Segment percussion; percussion.sourceStart = part.sourceStart; percussion.mono.resize(part.mono.size());
    juce::dsp::IIR::Filter<float> highpass, lowpass;
    highpass.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(analysisRate, 35.0);
    lowpass.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(analysisRate, 180.0);
    for (size_t i = 0; i < part.mono.size(); ++i) percussion.mono[i] = lowpass.processSample(highpass.processSample(part.mono[i]));
    return pulse(percussion, true, &part.mono);
}
struct BarPhase { int phase = -1; double confidence = 0; bool conflict = false; };
struct BeatEnergy
{
    std::array<double, 4> bands {}; // Whole beat: broadband, bass, mid, treble.
    double attack = 0, before = 0;
    double bassAttack = 0, bassBefore = 0;
    int frames = 0;
    double percussion() const { return std::max(0.0, bassAttack - bassBefore); }
};
std::vector<BeatEnergy> downbeatEnergy(const PcmView& samples, double rate, const MusicAnalysis& result, const std::shared_ptr<std::atomic<bool>>& cancelled)
{
    const double duration = samples.getNumSamples() / rate, period = 60 / result.bpm;
    std::vector<BeatEnergy> beats((size_t) std::ceil((duration - result.beatOrigin) / period));
    juce::dsp::IIR::Filter<float> sub, bass, midHigh, midLow, treble;
    sub.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(analysisRate, 35.0);
    bass.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(analysisRate, 180.0);
    midHigh.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(analysisRate, 180.0);
    midLow.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(analysisRate, 1800.0);
    treble.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(analysisRate, 1800.0);
    // Stream bounded chunks into beat-sized features. No whole-track FFT or PCM copy.
    for (double start = 0; start < duration; start += 60)
    {
        if (cancelled && *cancelled) return {};
        const auto part = monoSegment(samples, rate, start, std::min(60.0, duration - start), false, cancelled);
        for (size_t i = 0; i < part.mono.size(); ++i)
        {
            const float value = part.mono[i];
            const std::array<double, 4> filtered {value, bass.processSample(sub.processSample(value)),
                midLow.processSample(midHigh.processSample(value)), treble.processSample(value)};
            const double position = start + i / analysisRate - result.beatOrigin + 0.02;
            const int n = (int) std::floor(position / period);
            const double within = position - n * period;
            if (n >= 0 && n < (int) beats.size())
            {
                auto& beat = beats[(size_t) n]; ++beat.frames;
                for (size_t band = 0; band < 4; ++band) beat.bands[band] += filtered[band] * filtered[band];
                if (within < 0.14)
                {
                    beat.attack += value * value;
                    beat.bassAttack += filtered[1] * filtered[1];
                }
            }
            if (n + 1 >= 0 && n + 1 < (int) beats.size() && within >= period - 0.14)
            {
                beats[(size_t) (n + 1)].before += value * value;
                beats[(size_t) (n + 1)].bassBefore += filtered[1] * filtered[1];
            }
        }
    }
    for (auto& beat : beats)
    {
        for (auto& band : beat.bands) band /= std::max(1, beat.frames);
        beat.attack /= 0.14 * analysisRate; beat.before /= 0.14 * analysisRate;
        beat.bassAttack /= 0.14 * analysisRate; beat.bassBefore /= 0.14 * analysisRate;
    }
    return beats;
}
double quantile(std::vector<double> values, double fraction)
{
    if (values.empty()) return 0;
    auto at = values.begin() + (size_t) ((values.size() - 1) * fraction);
    std::nth_element(values.begin(), at, values.end()); return *at;
}
BarPhase accentBarPhase(const std::vector<BeatEnergy>& beats, const std::vector<Segment>& parts,
                        const MusicAnalysis& result, double attackScale)
{
    const double period = 60 / result.bpm;
    std::array<std::vector<double>, 4> accents;
    int agreed = -1; double confidence = 1;
    for (const auto& part : parts)
    {
        std::array<std::vector<double>, 4> local;
        const int first = std::max(0, (int) std::ceil((part.sourceStart - result.beatOrigin) / period));
        for (int n = first; n < (int) beats.size(); ++n)
        {
            if ((result.beatOrigin + n * period - part.sourceStart + 0.14) * analysisRate >= part.mono.size()) break;
            const auto group = (size_t) (n % 4);
            local[group].push_back(beats[(size_t) n].percussion());
            accents[group].push_back(beats[(size_t) n].percussion());
        }
        // Use the same measured bass attack as the phrase scorer. A quiet
        // tonal passage can have a large relative gap but negligible drum
        // support; it cannot establish or veto the track's bar phase.
        std::array<double, 4> energy {};
        size_t count = beats.size();
        for (size_t i = 0; i < 4; ++i) { energy[i] = quantile(local[i], 0.5); count = std::min(count, local[i].size()); }
        std::array<int, 4> order {0, 1, 2, 3};
        std::sort(order.begin(), order.end(), [&](int a, int b) { return energy[(size_t) a] > energy[(size_t) b]; });
        const double best = energy[(size_t) order[0]], gap = best > 1.0e-12 ? (best - energy[(size_t) order[1]]) / best : 0;
        if (count >= 6 && best >= attackScale * 0.1 && gap >= 0.2)
        {
            if (agreed >= 0 && agreed != order[0]) return {-1, 0, true};
            agreed = order[0]; confidence = std::min(confidence, gap);
        }
    }
    if (agreed < 0) return {};
    const double best = quantile(accents[(size_t) agreed], 0.5);
    for (int i = 0; i < 4; ++i) if (i != agreed && quantile(accents[(size_t) i], 0.5) > best * 0.8) return {};
    return {agreed, confidence};
}
BarPhase structuralBarPhase(const std::vector<BeatEnergy>& beats, double attackScale)
{
    if (beats.size() < 40) return {};
    std::array<double, 4> scale {};
    for (size_t band = 0; band < 4; ++band)
    {
        std::vector<double> energy; for (const auto& beat : beats) energy.push_back(beat.bands[band]);
        scale[band] = std::max({1.0e-12, quantile(energy, 0.5), scale[0] * 0.0001});
    }
    // REA: MixMeister combines cyclic rise counts with interval strength
    // (MmEngine 0x10004330; 0x100840b0 returns an interval maximum). Our
    // independent multiband score uses bass attack contrast: a recurring melody in
    // a drumless breakdown must not get the same vote as a strong phrase entry.
    // Normalize within the track and cap support, so gain and one loud crash
    // cannot determine bar phase. Weight before peak selection and recurrence
    // counting; weak tonal events must not inflate the supporting event count.
    std::vector<double> novelty(beats.size());
    for (size_t n = 4; n + 3 < beats.size(); ++n)
    {
        std::array<double, 4> sustainedRise {}, immediateRise {};
        for (size_t band = 0; band < 4; ++band)
        {
            immediateRise[band] = std::max(0.0, std::log1p(beats[n].bands[band] / scale[band])
                - std::log1p(std::max(beats[n - 1].bands[band], beats[n - 4].bands[band]) / scale[band]));
            novelty[n] += immediateRise[band];
            for (size_t b = 0; b < 4; ++b)
                sustainedRise[band] += (std::log1p(beats[n + b].bands[band] / scale[band])
                    - std::log1p(beats[n + b - 4].bands[band] / scale[band])) / 4;
        }
        // A recurring hat/fill on beat two can create many sharp treble rises
        // without changing the musical section. Require the entry to change the
        // following bar's bass or both broadband and midrange energy as well.
        const double bassEntry = std::min(sustainedRise[1], immediateRise[1]);
        const double midEntry = std::min({sustainedRise[0], sustainedRise[2], immediateRise[0], immediateRise[2]});
        // The IIR bands overlap. A bright hat leaks into a quiet mid band;
        // logarithmic normalization must not promote that leakage to tonal proof.
        const double tonalShare = std::min(1.0, beats[n].bands[2] / std::max(1.0e-12, beats[n].bands[3]));
        const double entrySupport = std::max(bassEntry, midEntry * tonalShare);
        novelty[n] *= juce::jlimit(0.0, 1.0, entrySupport / 0.15);
        novelty[n] *= std::min(1.0, beats[n].percussion() / attackScale);
    }
    const double threshold = std::max(0.75, quantile(novelty, 0.9));
    std::array<double, 4> votes {};
    std::array<int, 4> counts {}, first {-1, -1, -1, -1}, last {};
    for (int n = 4; n + 4 < (int) beats.size(); ++n)
    {
        if (novelty[(size_t) n] < threshold) continue;
        bool peak = true;
        for (int j = n - 4; j <= n + 4; ++j)
            if (novelty[(size_t) j] > novelty[(size_t) n]) { peak = false; break; }
        if (!peak) continue;
        const auto group = (size_t) (n % 4);
        // Each event has a bounded vote; one crash/pickup cannot supply recurrence.
        const double strength = std::min(2.0, novelty[(size_t) n]);
        votes[group] += strength * strength; ++counts[group];
        if (first[group] < 0) first[group] = n;
        last[group] = n;
    }
    std::array<int, 4> order {0, 1, 2, 3};
    std::sort(order.begin(), order.end(), [&](int a, int b) { return votes[(size_t) a] > votes[(size_t) b]; });
    const auto best = (size_t) order[0];
    const double total = std::accumulate(votes.begin(), votes.end(), 0.0);
    const double gap = votes[best] > 0 ? (votes[best] - votes[(size_t) order[1]]) / votes[best] : 0;
    if (counts[best] < 3 || last[best] - first[best] < 32 || votes[best] <= total * 0.5 || gap < 0.35) return {};
    return {order[0], gap * std::min(1.0, counts[best] / 6.0)};
}
void inferDownbeat(const std::vector<Segment>& parts, const PcmView& samples, double rate, MusicAnalysis& result, const std::shared_ptr<std::atomic<bool>>& cancelled)
{
    if (result.bpm <= 0 || result.variableTempo || result.tempoConfidence < 0.2) return;
    const auto beats = downbeatEnergy(samples, rate, result, cancelled);
    result.downbeatExaminedSeconds = samples.getNumSamples() / rate;
    std::vector<double> attacks;
    for (const auto& beat : beats) attacks.push_back(beat.percussion());
    const double attackScale = std::max(1.0e-12, quantile(attacks, 0.75));
    const auto accent = accentBarPhase(beats, parts, result, attackScale);
    const auto structure = structuralBarPhase(beats, attackScale);
    if (accent.conflict || (accent.phase >= 0 && structure.phase >= 0 && accent.phase != structure.phase))
    { result.downbeatMethod = "signal-conflict"; return; }
    const auto chosen = structure.phase >= 0 ? structure : accent;
    if (chosen.phase < 0) return;
    std::vector<double> levels;
    for (const auto& beat : beats)
        if (beat.bands[0] > 1.0e-12) levels.push_back(beat.bands[0]);
    // Energy is squared amplitude. A quiet percussion intro can be much softer
    // than the later full-band drop; use local attack contrast above a low
    // track-relative floor, not a fraction of the later kick's strength.
    const double floor = std::max(1.0e-12, quantile(levels, 0.5) * 0.0001);
    // Extend the selected bar phase back to the first audible attack, never to
    // an extrapolated beat in silence. Preserve the common fitted pulse grid.
    for (size_t n = (size_t) chosen.phase; n < beats.size(); n += 4)
        if (beats[n].bands[0] > floor && beats[n].attack - beats[n].before > std::max(floor, beats[n].attack * 0.1))
        {
            result.downbeatOrigin = result.beatOrigin + n * 60 / result.bpm;
            result.downbeatConfidence = chosen.confidence * result.tempoConfidence;
            result.downbeatMethod = structure.phase >= 0 ? "signal-structure" : "signal-accent";
            return;
        }
}
double correlation(const std::array<double, 12>& chroma, const double* profile, int root)
{
    double meanC = std::accumulate(chroma.begin(), chroma.end(), 0.0) / 12;
    double meanP = std::accumulate(profile, profile + 12, 0.0) / 12, cross = 0, cc = 0, pp = 0;
    for (int n = 0; n < 12; ++n) { double a = chroma[(size_t) ((n + root) % 12)] - meanC, b = profile[n] - meanP; cross += a * b; cc += a * a; pp += b * b; }
    return cc * pp > 1.0e-12 ? cross / std::sqrt(cc * pp) : 0;
}
struct TonalPeak { double note, weight; int frame; };
void tonality(const std::vector<Segment>& parts, MusicAnalysis& result)
{
    constexpr int size = 8192, hop = 4096, firstBin = 40, lastBin = 2970;
    juce::dsp::FFT fft(13); juce::dsp::WindowingFunction<float> window(size, juce::dsp::WindowingFunction<float>::hann);
    std::vector<float> spectrum(size * 2); std::vector<TonalPeak> peaks;
    double tuningX = 0, tuningY = 0, tuningWeight = 0; int frames = 0;
    for (const auto& part : parts)
        for (int pos = 0; pos + size <= (int) part.mono.size(); pos += hop)
        {
            std::fill(spectrum.begin(), spectrum.end(), 0.0f); std::copy_n(part.mono.data() + pos, size, spectrum.data()); window.multiplyWithWindowingTable(spectrum.data(), size); fft.performFrequencyOnlyForwardTransform(spectrum.data());
            // Normalize spectral registers separately. A loud kick/bass must
            // not gate out the quieter third/seventh carried by another voice.
            if (*std::max_element(spectrum.begin() + firstBin, spectrum.begin() + lastBin) < 0.00001f) continue;
            std::vector<float> envelope(lastBin); std::deque<int> maxima; int right = firstBin;
            for (int bin = firstBin; bin < lastBin; ++bin)
            {
                const int end = std::min(lastBin, (int) std::ceil(bin * std::sqrt(2.0)));
                while (right < end)
                {
                    while (!maxima.empty() && spectrum[(size_t) maxima.back()] <= spectrum[(size_t) right]) maxima.pop_back();
                    maxima.push_back(right++);
                }
                const int begin = std::max(firstBin, (int) std::floor(bin / std::sqrt(2.0)));
                while (!maxima.empty() && maxima.front() < begin) maxima.pop_front();
                envelope[(size_t) bin] = spectrum[(size_t) maxima.front()];
            }
            std::vector<TonalPeak> framePeaks;
            for (int bin = firstBin; bin < lastBin; ++bin)
            {
                float magnitude = spectrum[(size_t) bin];
                const float max = envelope[(size_t) bin];
                if (magnitude < max * 0.06f || magnitude <= spectrum[(size_t) bin - 1] || magnitude <= spectrum[(size_t) bin + 1]) continue;
                // Require a narrow spectral peak above its local noise floor.
                // Frame normalization alone otherwise gives noise/percussion the
                // same key vote as a sustained musical chord.
                double floor = 0;
                for (int j = bin - 8; j <= bin + 8; ++j) floor += spectrum[(size_t) j];
                if (magnitude < floor / 17 * 3) continue;
                double a = std::log(std::max(1.0e-12f, spectrum[(size_t) bin - 1])), b = std::log(magnitude), c = std::log(std::max(1.0e-12f, spectrum[(size_t) bin + 1]));
                double denominator = a - 2 * b + c;
                double delta = std::abs(denominator) > 1.0e-12 ? juce::jlimit(-0.5, 0.5, 0.5 * (a - c) / denominator) : 0;
                double frequency = (bin + delta) * analysisRate / size;
                double note = 69 + 12 * std::log2(frequency / 440.0);
                double weight = std::sqrt(magnitude / std::max(0.00001f, max)) / (1 + frequency / 1500.0);
                framePeaks.push_back({note, weight, frames});
            }
            // Reduce overtones already explained by a lower strong peak. A
            // harmonic-rich single note must remain a pitch rather than a chord.
            const auto raw = framePeaks;
            for (size_t p = 0; p < framePeaks.size(); ++p)
                for (size_t lower = 0; lower < p; ++lower)
                {
                    const double ratio = std::pow(2.0, (raw[p].note - raw[lower].note) / 12);
                    const double harmonic = std::round(ratio);
                    if (harmonic >= 2 && harmonic <= 8 && raw[lower].weight >= raw[p].weight * 0.6
                        && std::abs(12 * std::log2(ratio / harmonic)) < 0.08)
                    { framePeaks[p].weight *= 0.12; break; }
                }
            double sum = 0; for (auto& p : framePeaks) sum += p.weight;
            for (auto& p : framePeaks)
            {
                p.weight *= part.tonalWeight / std::max(1.0e-12, sum); peaks.push_back(p);
                const double cents = (p.note - std::round(p.note)) * 100;
                tuningX += p.weight * std::cos(cents * 2 * pi / 100); tuningY += p.weight * std::sin(cents * 2 * pi / 100);
                tuningWeight += p.weight;
            }
            ++frames;
        }
    if (frames < 4 || peaks.size() < 20) return;
    if (std::hypot(tuningX, tuningY) < tuningWeight * 0.2) return; // No coherent semitone tuning, e.g. broadband noise.
    result.tuningCents = std::atan2(tuningY, tuningX) * 100 / (2 * pi);
    std::vector<std::array<double, 12>> chromas((size_t) frames);
    for (const auto& p : peaks)
    {
        double note = p.note - result.tuningCents / 100;
        int nearest = (int) std::round(note); double distance = std::abs(note - nearest);
        if (distance < 0.4) chromas[(size_t) p.frame][(size_t) ((nearest % 12 + 12) % 12)] += p.weight * std::pow(std::cos(distance * pi / 0.8), 2);
    }
    const double profiles[4][12] {
        // Published Sha'ath popular/electronic and Faraldo Beatport profiles;
        // fixed external data, never fitted to this library or its labels.
        {7.239,3.504,3.584,2.845,5.819,4.559,2.448,6.995,3.391,4.556,4.074,4.459},
        {7.003,3.144,4.359,5.404,3.672,4.090,3.908,6.200,3.634,2.872,5.355,3.832},
        {1,0,.42,0,.53,.37,0,.77,0,.38,.21,.30},
        {1,0,.36,.39,0,.38,0,.74,.27,0,.42,.23}
    };
    std::array<double, 12> global {}; std::array<double, 24> votes {}, winners {};
    int passages = 0;
    for (const auto& c : chromas) for (int n = 0; n < 12; ++n) global[(size_t) n] += c[(size_t) n];
    for (int offset = 0; offset < frames; offset += 16)
    {
        std::array<double, 12> local {};
        for (int f = offset; f < std::min(frames, offset + 16); ++f) for (int n = 0; n < 12; ++n) local[(size_t) n] += chromas[(size_t) f][(size_t) n];
        for (int key = 0; key < 24; ++key) votes[(size_t) key] += std::max(0.0, (correlation(local, profiles[key / 12], key % 12) + correlation(local, profiles[2 + key / 12], key % 12)) * 0.5);
        int winner = 0; double best = -1;
        for (int key = 0; key < 24; ++key)
        {
            const double score = (correlation(local, profiles[key / 12], key % 12) + correlation(local, profiles[2 + key / 12], key % 12)) * 0.5;
            if (score > best) { best = score; winner = key; }
        }
        if (best > 0) { ++winners[(size_t) winner]; ++passages; }
    }
    std::array<double, 24> score {};
    double maxVote = *std::max_element(votes.begin(), votes.end());
    for (int key = 0; key < 24; ++key) score[(size_t) key] = 0.7 * (correlation(global, profiles[key / 12], key % 12) + correlation(global, profiles[2 + key / 12], key % 12)) * 0.5 + 0.3 * votes[(size_t) key] / std::max(1.0e-12, maxVote);
    std::array<int, 24> order; std::iota(order.begin(), order.end(), 0); std::sort(order.begin(), order.end(), [&](int a, int b) { return score[(size_t) a] > score[(size_t) b]; });
    double energy = std::accumulate(global.begin(), global.end(), 0.0), maximum = *std::max_element(global.begin(), global.end());
    if (energy < 1.0e-8 || maximum / energy > 0.85 || score[(size_t) order[0]] < 0.45) return; // A single tone is a pitch, not evidence of a key.
    result.key = order[0]; result.alternativeKey = order[1];
    const double margin = (score[(size_t) order[0]] - score[(size_t) order[1]]) / std::max(1.0e-12, 1 - score[(size_t) order[1]]);
    int agreement = 0;
    for (int model = 0; model < 2; ++model)
    {
        int winner = 0; double best = -1;
        for (int key = 0; key < 24; ++key)
        {
            const double candidate = correlation(global, profiles[model * 2 + key / 12], key % 12);
            if (candidate > best) { best = candidate; winner = key; }
        }
        if (winner == result.key) ++agreement;
    }
    // A stability/margin score, not a calibrated probability. The old additive
    // formula saturated even when sections or major/minor models disagreed.
    result.keyConfidence = juce::jlimit(0.0, 1.0, margin) * winners[(size_t) result.key] / std::max(1, passages) * agreement / 2.0;
}
bool fitNeuralGrid(const NeuralRhythm& neural, double duration, const std::vector<Segment>& parts,
                   const std::vector<const Pulse*>& pulses, const std::deque<Pulse>& measurements, const PcmView& samples, double rate, MusicAnalysis& result, const std::shared_ptr<std::atomic<bool>>& cancelled, RhythmDiagnostics* diagnostics)
{
    const auto transientGrid = result;
    double calibration = 0;
    bool signalTiming = false;
    if (neural.error.isNotEmpty() || neural.beats.size() < 16) return false;
    std::vector<double> periods;
    for (size_t i = 8; i < neural.beats.size(); ++i) periods.push_back((neural.beats[i] - neural.beats[i - 8]) / 8);
    const double low = quantile(periods, 0.25), high = quantile(periods, 0.75);
    double initial = 0; int count = 0;
    for (auto p : periods) if (p >= low && p <= high) { initial += p; ++count; }
    if (count == 0) return false;
    initial /= count;
    if (initial < 60.0 / 400 || initial > 60.0 / 20) return false;
    struct Grid { double period, origin, score, sourceStart; };
    struct Support
    {
        double agreement = 0, coverage = 0; size_t events = 0, inliers = 0;
        double combined() const { return agreement * coverage; }
    };
    std::vector<Grid> local;
    auto neuralSupport = [&](double period, double origin, double begin, double end)
    {
        double value = 0, weight = 0, first = -1, last = -1;
        int64_t previous = std::numeric_limits<int64_t>::min(); size_t occupied = 0, events = 0;
        for (size_t i = 0; i < neural.beats.size(); ++i)
        {
            const double time = neural.beats[i] + calibration; if (time < begin || time >= end) continue;
            ++events; if (first < 0) first = time;
            last = time;
            const double distance = std::remainder(time - origin, period) / std::min(0.04, period * 0.12);
            value += neural.beatConfidence[i] * std::max(0.0, 1 - distance * distance); weight += neural.beatConfidence[i];
            const auto ordinal = (int64_t) std::llround((time - origin) / period);
            if (std::abs(distance) < 1 && ordinal != previous) { ++occupied; previous = ordinal; }
        }
        const double predictions = first >= 0 ? (last - first) / period + 1 : 0;
        return Support {weight > 0 ? value / weight : 0, predictions > 0 ? std::min(1.0, occupied / predictions) : 0, events, occupied};
    };
    auto score = [&](double period, double origin, double begin, double end)
    { return neuralSupport(period, origin, begin, end).combined(); };
    // Local fits keep tempo and phase paired; short-baseline quantization must
    // not be extrapolated over an entire recording before considering its peers.
    for (double start = 0; start < duration; start += 20)
    {
        std::vector<double> seeds {initial};
        if (transientGrid.bpm > 0) seeds.push_back(60 / transientGrid.bpm);
        for (size_t lag : {size_t(1), size_t(2), size_t(8)})
        {
            std::vector<double> observed;
            for (size_t i = lag; i < neural.beats.size(); ++i)
                if (neural.beats[i - lag] >= start && neural.beats[i] < start + 20)
                    observed.push_back((neural.beats[i] - neural.beats[i - lag]) / lag);
            if (!observed.empty()) seeds.push_back(quantile(observed, 0.5));
        }
        for (const double seed : seeds)
        {
        if (seed < 60.0 / 400 || seed > 60.0 / 20) continue;
        std::complex<double> phase {}; size_t observations = 0;
        for (size_t i = 0; i < neural.beats.size(); ++i)
            if (neural.beats[i] >= start && neural.beats[i] < start + 20)
            { phase += std::polar((double) neural.beatConfidence[i], (neural.beats[i] - start) * 2 * pi / seed); ++observations; }
        if (observations < 8 || std::abs(phase) < observations * 0.2) continue;
        double period = seed, origin = start + std::arg(phase) * seed / (2 * pi);
        for (int iteration = 0; iteration < 3; ++iteration)
        {
            std::vector<std::pair<double, double>> points;
            for (auto time : neural.beats) if (time >= start && time < start + 20)
            {
                const double n = std::round((time - origin) / period);
                if (std::abs(time - (origin + n * period)) < std::min(0.05, period * 0.15)) points.push_back({n, time});
            }
            fitGrid(points, period, origin);
        }
        local.push_back({period, origin, score(period, origin, 0, duration), start});
        }
    }
    if (local.empty()) return false;
    const auto best = *std::max_element(local.begin(), local.end(), [](const Grid& a, const Grid& b) { return a.score < b.score; });
    double period = best.period, origin = best.origin;
    for (int iteration = 0; iteration < 3; ++iteration)
    {
        std::vector<std::pair<double, double>> points;
        for (auto time : neural.beats)
        {
            const double n = std::round((time - origin) / period);
            if (std::abs(time - (origin + n * period)) < std::min(0.04, period * 0.12)) points.push_back({n, time});
        }
        fitGrid(points, period, origin);
    }
    // Reconcile learned metrical level with physical evidence. A competing
    // level needs both sharper attacks and independently recurring 4/4 bars;
    // a matching steady grid can refine the model's 20 ms annotation frames.
    if (!std::isfinite(period) || 60 / period < 20 || 60 / period > 400) return false;
    result.neuralBpm = 60 / period; result.signalBpm = transientGrid.bpm;
    result.signalTempoConfidence = transientGrid.tempoConfidence; result.signalGridNeedsReview = transientGrid.variableTempo;
    const double measuredSpan = neural.beats.back() - neural.beats.front();
    // Ordinary refinement uses the model fit tolerance. Larger corrections
    // need recurring sharp physical attacks, rather than layer intensity alone.
    const double refinementTolerance = std::min(0.04, period * 0.12);
    const double tempoAgreement = 2 * refinementTolerance / std::max(period, measuredSpan);
    bool percussiveMetre = false; MusicAnalysis barProof;
    const double ratio = transientGrid.bpm > 0 ? 60 / period / transientGrid.bpm : 0;
    bool alias = false; for (double factor : {0.5, 2.0, 1.5, 2.0 / 3.0}) alias |= std::abs(ratio / factor - 1) < 0.015;
    if (alias && !transientGrid.variableTempo && transientGrid.tempoConfidence >= 0.2)
    {
        auto physicalSupport = [&](double beatPeriod, double phase)
        {
            double support = 0; int windows = 0;
            for (const auto* windowPointer : pulses)
            {
                const auto& window = *windowPointer;
                std::vector<double> contrasts;
                for (const auto& event : window.transients) if (event.attackContrast > 0) contrasts.push_back(event.attackContrast);
                if (contrasts.empty() || window.coverageSeconds <= 0) continue;
                const double cap = std::max(1.0e-12, quantile(contrasts, 0.75)); double on = 0, total = 0;
                for (const auto& event : window.transients)
                {
                    const double weight = std::min(1.0, event.attackContrast / cap);
                    const double distance = std::remainder(event.time - phase, beatPeriod) / std::min(0.035, beatPeriod * 0.12);
                    on += weight * std::max(0.0, 1 - distance * distance); total += weight;
                }
                if (total > 0) { support += on / std::sqrt(total * window.coverageSeconds / beatPeriod); ++windows; }
            }
            return windows > 0 ? support / windows : 0.0;
        };
        const double pulseSupport = physicalSupport(60 / transientGrid.bpm, transientGrid.beatOrigin);
        const double neuralAttackSupport = physicalSupport(period, origin);
        if (pulseSupport > neuralAttackSupport * 1.15)
        {
            barProof = transientGrid; inferDownbeat(parts, samples, rate, barProof, cancelled);
            if (cancelled && *cancelled) return false;
            percussiveMetre = barProof.downbeatOrigin >= 0;
        }
    }
    auto sharpAttackSupport = [&](double phase)
    {
        double support = 0, predictions = 0;
        size_t supportingBeats = 0;
        for (const auto& window : measurements) if (window.independentPhysical && window.coverageSeconds > 0)
        {
            std::vector<double> contrasts;
            for (const auto& event : window.transients) if (event.attackContrast > 0) contrasts.push_back(event.attackContrast);
            if (contrasts.empty()) continue;
            const double cap = std::max(1.0e-12, quantile(contrasts, .75));
            std::map<int64_t, double> beats;
            for (const auto& event : window.transients)
            {
                const double distance = std::remainder(event.time - phase, period) / refinementTolerance;
                // An RMS rise exceeding 80% over 12ms supplies sharp-onset
                // evidence. A slow bass swell cannot win merely by being loud.
                const double sharp = std::max(0., (event.attackSharpness - .8) / .2);
                const double value = std::min(1., event.attackContrast / cap) * sharp * std::max(0., 1 - distance * distance);
                const auto ordinal = (int64_t) std::llround((event.time - phase) / period);
                beats[ordinal] = std::max(beats[ordinal], value);
            }
            for (const auto& beat : beats) if (beat.second > 0) { support += beat.second; ++supportingBeats; }
            predictions += window.coverageSeconds / period;
        }
        return supportingBeats >= 8 && predictions > 0 ? support / predictions : 0.;
    };
    auto phaseRefinementSupported = [&]
    {
        const double difference = std::abs(std::remainder(transientGrid.beatOrigin - origin, period));
        if (difference < refinementTolerance) return true;
        if (difference >= period * (transientGrid.tempoConfidence >= .8 ? .45 : .3)) return false;
        return sharpAttackSupport(transientGrid.beatOrigin) > sharpAttackSupport(origin) * 1.15;
    };
    if (percussiveMetre || (!transientGrid.variableTempo && transientGrid.bpm > 0 && transientGrid.tempoConfidence >= 0.2
        && std::abs(transientGrid.bpm * period / 60 - 1) < tempoAgreement
        && phaseRefinementSupported()))
    {
        calibration = std::remainder(transientGrid.beatOrigin - origin, period);
        period = 60 / transientGrid.bpm; origin = transientGrid.beatOrigin;
        signalTiming = true;
    }
    result.bpm = std::round(60 / period * 10000) / 10000; period = 60 / result.bpm;
    result.beatOrigin = std::fmod(std::fmod(origin, period) + period, period);
    result.rhythmMethod = "beat-this-final0";
    result.neuralPhaseCorrectionMs = calibration * 1000;
    result.downbeatOrigin = -1; result.downbeatConfidence = 0;
    result.downbeatMethod = "unknown";
    result.beatWindows.clear(); result.examinedSeconds = 0;
    result.downbeatExaminedSeconds = duration;
    double residual = 0, confidence = 0; size_t inliers = 0;
    for (size_t i = 0; i < neural.beats.size(); ++i)
    {
        const double deviation = std::remainder(neural.beats[i] + calibration - origin, period);
        if (std::abs(deviation) < std::min(0.04, period * 0.12)) { residual += deviation * deviation; confidence += neural.beatConfidence[i]; ++inliers; }
    }
    result.tempoConfidence = confidence / neural.beats.size();
    result.gridErrorMs = inliers > 0 ? std::sqrt(residual / inliers) * 1000 : 0;
    result.variableTempo = !signalTiming && inliers < neural.beats.size() * 0.7;
    if (signalTiming)
    {
        result.tempoConfidence = std::max(result.tempoConfidence, transientGrid.tempoConfidence);
        result.gridErrorMs = transientGrid.gridErrorMs;
    }
    // A calibrated phase cannot establish all-source steadiness. Corroborate
    // competing local timing against independently measured physical rises.
    // Agreement and occupied predictions remain separate; sparse neural events
    // alone neither prove a tempo change nor veto strong physical evidence.
    for (double begin = 0; begin < duration; begin += 20)
    {
        if (cancelled && *cancelled) return false;
        const double end = std::min(duration, begin + 20);
        // Distributed windows calibrate the global estimate; they cannot own
        // coverage of the intervals between them. Examine physical timing in
        // contiguous bounded windows, independently of neural event availability.
        // Each local observation is counted once, with its own source coverage.
        const auto physicalWindow = percussionPulse(monoSegment(samples, rate, begin, end - begin, false, cancelled));
        if (cancelled && *cancelled) return false;
        LocalTimingEvidence evidence; evidence.sourceStart = begin; evidence.sourceEnd = end;
        evidence.examinedPhysicalSeconds = physicalWindow.coverageSeconds;
        std::vector<const Transient*> physical;
        std::vector<double> contrasts;
        for (const auto& event : physicalWindow.transients) if (event.time >= begin && event.time < end && event.attackContrast > 0)
        {
            physical.push_back(&event); contrasts.push_back(event.attackContrast);
            if (diagnostics) diagnostics->physicalTransients.push_back({event.time, event.novelty, event.attackContrast});
        }
        evidence.physicalEvents = (int) physical.size();
        if (physical.size() < 8 || evidence.examinedPhysicalSeconds <= 0)
        { if (diagnostics) diagnostics->localFits.push_back(evidence); continue; }
        const double cap = std::max(1.0e-12, quantile(contrasts, .75));
        auto physicalSupport = [&](double testPeriod, double phase)
        {
            double value = 0, total = 0; std::set<int64_t> occupied; size_t inliers = 0;
            for (const auto* event : physical)
            {
                const double weight = std::min(1., event->attackContrast / cap);
                const double distance = std::remainder(event->time - phase, testPeriod) / std::min(.035, testPeriod * .12);
                value += weight * std::max(0., 1 - distance * distance); total += weight;
                if (std::abs(distance) < 1) { ++inliers; occupied.insert((int64_t) std::llround((event->time - phase) / testPeriod)); }
            }
            const double predictions = evidence.examinedPhysicalSeconds / testPeriod;
            return Support {total > 0 ? value / total : 0, predictions > 0 ? std::min(1., occupied.size() / predictions) : 0, physical.size(), inliers};
        };
        const auto global = physicalSupport(period, origin); evidence.globalPhysicalSupport = global.combined();
        Support bestPhysical; const Grid* proposal = nullptr; double fittedPeriod = 0, fittedOrigin = 0;
        std::vector<Grid> proposals;
        for (const auto& candidate : local) if (candidate.sourceStart == begin)
            proposals.push_back({candidate.period, candidate.origin + calibration, candidate.score, begin});
        // A sparse or mistaken neural passage must not veto a physical tempo
        // proposal. The same independent agreement/coverage tests assess both.
        if (physicalWindow.bpm > 0) proposals.push_back({60 / physicalWindow.bpm, physicalWindow.origin, 0, begin});
        for (const auto& candidate : proposals)
        {
            double candidatePeriod = candidate.period, candidateOrigin = candidate.origin;
            bool fitted = false;
            for (int iteration = 0; iteration < 3; ++iteration)
            {
                std::vector<std::pair<double, double>> points;
                for (const auto* event : physical)
                {
                    const double ordinal = std::round((event->time - candidateOrigin) / candidatePeriod);
                    if (std::abs(event->time - candidateOrigin - ordinal * candidatePeriod) < std::min(.04, candidatePeriod * .12)) points.push_back({ordinal, event->time});
                }
                if (!fitGrid(points, candidatePeriod, candidateOrigin)) { fitted = false; break; }
                fitted = true;
            }
            if (!fitted || candidatePeriod < 60. / 400 || candidatePeriod > 60. / 20) continue;
            const auto support = physicalSupport(candidatePeriod, candidateOrigin);
            if (support.combined() > bestPhysical.combined())
            { bestPhysical = support; proposal = &candidate; fittedPeriod = candidatePeriod; fittedOrigin = candidateOrigin; }
        }
        if (proposal)
        {
            evidence.hasPhysicalFit = true; evidence.physicalBpm = 60 / fittedPeriod; evidence.physicalOrigin = fittedOrigin;
            evidence.physicalAgreement = bestPhysical.agreement; evidence.physicalCoverage = bestPhysical.coverage; evidence.physicalInliers = (int) bestPhysical.inliers;
            const auto neuralEvidence = neuralSupport(proposal->period, proposal->origin, begin, end);
            evidence.neuralAgreement = neuralEvidence.agreement; evidence.neuralCoverage = neuralEvidence.coverage;
            const double tolerance = std::min(.035, period * .12);
            const bool differs = std::abs(fittedPeriod / period - 1) * evidence.examinedPhysicalSeconds > 2 * tolerance
                || std::abs(std::remainder(fittedOrigin - origin, period)) > tolerance;
            const double localPeriodRatio = fittedPeriod / period;
            const bool simpleSubdivision = std::abs(localPeriodRatio - 2) < .03 || std::abs(localPeriodRatio - .5) < .0075;
            evidence.conflict = differs && !simpleSubdivision && evidence.examinedPhysicalSeconds >= 8 * fittedPeriod
                && bestPhysical.inliers >= 8 && bestPhysical.agreement >= .8 && bestPhysical.coverage >= .7
                && bestPhysical.agreement - global.agreement > .3 && bestPhysical.combined() - global.combined() > .3;
            result.variableTempo = result.variableTempo || evidence.conflict;
        }
        if (diagnostics) diagnostics->localFits.push_back(evidence);
    }
    for (const auto& part : parts)
    {
        const double end = part.sourceStart + part.mono.size() / analysisRate;
        const double support = score(period, origin, part.sourceStart, end);
        const Grid* section = nullptr; double localSupport = 0;
        for (const auto& candidate : local)
        {
            const double value = score(candidate.period, candidate.origin + calibration, part.sourceStart, end);
            if (value > localSupport) { localSupport = value; section = &candidate; }
        }
        if (!signalTiming && localSupport >= 0.8 && localSupport - support > 0.3) result.variableTempo = true;
        // A phase shift is also unusable for fixed-grid playback. Rhythmic
        // subdivision by itself never sets this flag; measured timing does.
        result.beatWindows.push_back(section ? BeatEstimate{part.sourceStart, 60 / section->period, section->origin + calibration, localSupport}
                                            : BeatEstimate{part.sourceStart});
        result.examinedSeconds += part.mono.size() / analysisRate;
    }
    if (result.variableTempo) return true;
    if (percussiveMetre)
    {
        result.downbeatOrigin = barProof.downbeatOrigin; result.downbeatConfidence = barProof.downbeatConfidence;
        result.downbeatMethod = barProof.downbeatMethod; return true;
    }
    std::array<double, 4> votes {}; std::array<int, 4> counts {};
    for (size_t i = 0; i < neural.downbeats.size(); ++i)
    {
        const double time = neural.downbeats[i] + calibration;
        const double n = std::round((time - origin) / period);
        if (std::abs(time - (origin + n * period)) >= std::min(0.05, period * 0.15)) continue;
        const auto phase = (size_t) (((int64_t) n % 4 + 4) % 4);
        votes[phase] += neural.downbeatConfidence[i]; ++counts[phase];
    }
    std::array<int, 4> order {0, 1, 2, 3};
    std::sort(order.begin(), order.end(), [&](int a, int b) { return votes[(size_t) a] > votes[(size_t) b]; });
    const auto winner = (size_t) order[0]; const double total = std::accumulate(votes.begin(), votes.end(), 0.0);
    const double gap = votes[winner] > 0 ? (votes[winner] - votes[(size_t) order[1]]) / votes[winner] : 0;
    if (counts[winner] < 8 || votes[winner] < total * 0.6 || gap < 0.35) return true;
    for (auto observed : neural.beats)
    {
        const double time = observed + calibration;
        const double n = std::round((time - origin) / period);
        if ((size_t) (((int64_t) n % 4 + 4) % 4) != winner || std::abs(time - (origin + n * period)) >= std::min(0.04, period * 0.12)) continue;
        result.downbeatOrigin = origin + n * period;
        if (result.downbeatOrigin < 0) continue;
        result.downbeatConfidence = result.tempoConfidence * votes[winner] / total * gap;
        result.downbeatMethod = "beat-this-final0";
        break;
    }
    return true;
}

}
MusicAnalysis analyseMusic(const PcmView& samples, double rate, const std::shared_ptr<std::atomic<bool>>& cancelled, RhythmDiagnostics* diagnostics)
{
    MusicAnalysis result; if (diagnostics) *diagnostics = {};
    const auto aborted = [&]
    {
        if (!cancelled || !*cancelled) return false;
        if (diagnostics) *diagnostics = {};
        result.rhythmIssue = "Analysis cancelled."; result.rhythmMethod = "unavailable"; result.variableTempo = true;
        result.tempoConfidence = 0; result.downbeatOrigin = -1; result.downbeatConfidence = 0; result.downbeatMethod = "unknown"; return true;
    };
    if (aborted()) return result;
    if (!std::isfinite(rate) || rate < 8000 || samples.getNumChannels() < 1 || samples.getNumSamples() < rate * 2) return result;
    auto parts = segments(samples, rate, cancelled);
    // Stable source observations own their data while selected fits borrow pointers.
    std::deque<Pulse> measurements;
    std::vector<const Pulse*> pulses; pulses.reserve(parts.size());
    if (aborted()) return result;
    const auto neural = analyseNeuralRhythm(samples, rate, nullptr, cancelled);
    if (aborted()) return result;
    for (const auto& part : parts)
    {
        if (aborted()) return result;
        result.examinedSeconds += part.mono.size() / analysisRate;
        // Broadband flux can lock to syncopated hats or bass harmonics while
        // reporting the right BPM. Measure the percussive low band separately;
        // its onset phase belongs to that band all the way through time fitting.
        measurements.push_back(percussionPulse(part));
        const Pulse* selected = &measurements.back();
        if (selected->confidence < 0.35)
        {
            measurements.push_back(pulse(part));
            if (measurements.back().confidence > selected->confidence) selected = &measurements.back();
        }
        result.beatWindows.push_back({part.sourceStart, selected->bpm, selected->origin, selected->confidence});
        if (selected->bpm > 0) pulses.push_back(selected);
    }
    if (!pulses.empty())
    {
        std::sort(pulses.begin(), pulses.end(), [](const Pulse* a, const Pulse* b) { return a->bpm < b->bpm; });
        const auto& chosen = *pulses[pulses.size() / 2];
        double period = 60 / chosen.bpm, origin = chosen.origin, confidence = 0, tempoConflict = 0;
        for (const auto* item : pulses)
        {
            const auto& p = *item;
            if (std::abs(p.bpm - chosen.bpm) / chosen.bpm > 0.015) tempoConflict += p.confidence;
            else confidence += p.confidence;
        }
        result.variableTempo = tempoConflict > confidence * 0.4;
        auto phaseSupport = [](const Pulse& p, double phase, double beatPeriod)
        {
            double support = 0, total = 0;
            for (const auto& onset : p.transients)
            {
                const double distance = std::remainder(onset.time - phase, beatPeriod) / std::min(0.035, beatPeriod * 0.12);
                support += onset.novelty * std::max(0.0, 1 - distance * distance); total += onset.novelty;
            }
            return total > 0 ? support / total : 0.0;
        };
        // Regress beat ordinals across the actual source-window separation.
        // Averaging wrapped phases loses the long-baseline tempo information and
        // a small local BPM error becomes audible drift late in a long track.
        if (!result.variableTempo)
        {
            // A median-BPM window may have locked to a snare or offbeat during a
            // breakdown; select phase by confidence-weighted cross-window support.
            // Each local fit owns its period AND phase. Combining one window's
            // period with another's origin can accumulate enough error over the
            // unsampled gap to reject a perfectly steady track. Compare only
            // measured hypotheses; do not sweep tempo to fit distant offbeats.
            double bestScore = -1;
            for (const auto* candidate : pulses)
            {
                const auto& anchor = *candidate;
                if (std::abs(anchor.bpm - chosen.bpm) / chosen.bpm > 0.015) continue;
                const double candidatePeriod = 60 / anchor.bpm;
                double score = 0;
                for (const auto* item : pulses)
                {
                    const auto& p = *item;
                    score += p.confidence * phaseSupport(p, anchor.origin, candidatePeriod);
                }
                if (score > bestScore) { bestScore = score; origin = anchor.origin; period = candidatePeriod; }
            }
            for (int iteration = 0; iteration < 3; ++iteration)
            {
                std::vector<std::pair<double, double>> grid;
                for (const auto* p : pulses) for (const auto& onset : p->transients)
                {
                    const double time = onset.time;
                    const double ordinal = std::round((time - origin) / period);
                    if (std::abs(time - (origin + ordinal * period)) < std::min(0.030, period * 0.10)) grid.push_back({ordinal, time});
                }
                fitGrid(grid, period, origin);
            }
        }
        result.bpm = std::round(60 / period * 10000) / 10000;
        period = 60 / result.bpm;
        result.beatOrigin = std::fmod(std::fmod(origin, period) + period, period);
        double error = 0, supporting = 0, conflicting = 0; size_t count = 0;
        for (const auto* item : pulses)
        {
            const auto& p = *item;
            // A breakdown's strongest local pattern may be syncopated. Retain
            // all measured attacks so weaker kicks can still support the common
            // phase; do not synthesize a beat from a predicted time in silence.
            double best = 0;
            for (int phase = 0; phase < 160; ++phase) best = std::max(best, phaseSupport(p, period * phase / 160, period));
            const bool agrees = best > 0 && phaseSupport(p, result.beatOrigin, period) >= best * 0.6
                && std::abs(p.bpm - result.bpm) / result.bpm <= 0.015;
            if (agrees) supporting += p.confidence; else conflicting += p.confidence;
            for (const auto& onset : p.transients)
            {
                const double deviation = std::remainder(onset.time - result.beatOrigin, period);
                if (std::abs(deviation) < std::min(0.030, period * 0.10)) { error += deviation * deviation; ++count; }
            }
        }
        result.gridErrorMs = count > 0 ? std::sqrt(error / count) * 1000 : 0;
        // Inconsistent phase across windows also needs manual review, even when
        // local tempo estimates agree (edits, tempo ramps or a shifted grid).
        if (supporting == 0 || conflicting > supporting * 0.4) result.variableTempo = true;
        result.tempoConfidence = std::min(confidence, supporting) / parts.size() * (result.variableTempo ? 0.5 : 1.0);
    }
    if (aborted()) return result;
    const bool neuralGrid = fitNeuralGrid(neural, samples.getNumSamples() / rate, parts, pulses, measurements, samples, rate, result, cancelled, diagnostics);
    if (aborted()) return result;
    if (diagnostics)
    {
        if (!neuralGrid) for (const auto& window : measurements) if (window.independentPhysical)
            for (const auto& event : window.transients) diagnostics->physicalTransients.push_back({event.time, event.novelty, event.attackContrast});
        std::sort(diagnostics->physicalTransients.begin(), diagnostics->physicalTransients.end(),
                  [](const auto& a, const auto& b) { return a[0] < b[0]; });
    }
    auto musicalBar = result; musicalBar.downbeatOrigin = -1; musicalBar.downbeatConfidence = 0; musicalBar.downbeatMethod = "unknown";
    // Section evidence must use the FINAL pulse, including repaired rhythmic
    // levels. An earlier half-tempo fit cannot own the corrected track's bars.
    if (neuralGrid && (result.downbeatMethod == "signal-accent" || result.downbeatMethod == "signal-structure")) musicalBar = result;
    else inferDownbeat(parts, samples, rate, musicalBar, cancelled);
    if (aborted()) return result;
    if (!neuralGrid || musicalBar.downbeatMethod == "signal-structure" || musicalBar.downbeatMethod == "signal-conflict"
        || (result.downbeatOrigin < 0 && musicalBar.downbeatOrigin >= 0))
    {
        result.downbeatOrigin = musicalBar.downbeatOrigin; result.downbeatConfidence = musicalBar.downbeatConfidence;
        result.downbeatMethod = musicalBar.downbeatMethod;
    }
    if (neural.error.isNotEmpty())
    {
        result.rhythmIssue = neural.error; result.variableTempo = true;
        result.rhythmMethod = "unavailable";
        result.tempoConfidence = 0; result.downbeatOrigin = -1; result.downbeatConfidence = 0;
        result.downbeatMethod = "unknown";
    }
    auto harmonicParts = parts;
    if (samples.getNumChannels() >= 2) for (const auto& part : parts)
    {
        auto side = monoSegment(samples, rate, part.sourceStart, part.mono.size() / analysisRate, true, cancelled);
        if (aborted()) return result;
        double monoEnergy = 0, sideEnergy = 0;
        for (size_t i = 0; i < part.mono.size(); ++i) { monoEnergy += part.mono[i] * part.mono[i]; sideEnergy += side.mono[i] * side.mono[i]; }
        if (sideEnergy > monoEnergy * 0.01 && sideEnergy > 1.0e-8)
        {
            side.tonalWeight = std::min(1.0, std::sqrt(sideEnergy / std::max(1.0e-12, monoEnergy)));
            harmonicParts.push_back(std::move(side));
        }
    }
    tonality(harmonicParts, result); aborted(); return result;
}
namespace
{
juce::String keyTonicName(int key, bool flats)
{
    if (key < 0 || key > 23) return "Unknown";
    const char* sharp[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const char* flat[] { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
    return flats ? flat[key % 12] : sharp[key % 12];
}
}
juce::String keyNoteName(int key, bool flats)
{
    return key < 0 || key > 23 ? "Unknown" : keyTonicName(key, flats) + (key < 12 ? " major" : " minor");
}
juce::String keyCompactName(int key, bool flats)
{
    return key < 0 || key > 23 ? "Unknown" : keyTonicName(key, flats) + (key < 12 ? "" : "m");
}
int keyWheelNumber(int key)
{
    if (key < 0 || key > 23) return 0;
    const int major[] {8,3,10,5,12,7,2,9,4,11,6,1}, minor[] {5,12,7,2,9,4,11,6,1,8,3,10};
    return key < 12 ? major[key] : minor[key - 12];
}
juce::String keyCamelot(int key)
{
    return keyWheelNumber(key) == 0 ? juce::String("?") : juce::String(keyWheelNumber(key)) + (key < 12 ? "B" : "A");
}
bool harmonicallyCompatible(int first, int second)
{
    if (first < 0 || first > 23 || second < 0 || second > 23) return false;
    int diff = std::abs(keyWheelNumber(first) - keyWheelNumber(second));
    return diff == 0 || ((first < 12) == (second < 12) && (diff == 1 || diff == 11));
}
}
