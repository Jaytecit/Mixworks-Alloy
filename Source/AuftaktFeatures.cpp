#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "AuftaktFeatures.h"
#include <CDSPResampler.h>
#include <pocketfft_hdronly.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>

namespace mixworks
{
namespace
{
#include "AuftaktFilterbank.inc"
constexpr std::array<int, 3> sizes {1024, 2048, 4096}, counts {22, 46, 92}, lags {1, 1, 2};
}

AuftaktFeatures extractAuftaktFeatures(const juce::AudioBuffer<float>& pcm, double rate, size_t maximumBytes)
{
    AuftaktFeatures result;
    auto fail = [&](const juce::String& message) { result.values.clear(); result.error = message; return result; };
    if (!std::isfinite(rate) || rate < 8000 || rate > 192000 || pcm.getNumChannels() < 1
        || pcm.getNumChannels() > 2 || pcm.getNumSamples() < 1)
        return fail("Invalid AUFTAKT frontend PCM format (mono/stereo, 8–192 kHz required).");
    const double samples = std::ceil(pcm.getNumSamples() * (double) result.sampleRate / rate);
    const double frames = std::ceil(samples / result.hop);
    // Count expanded rows and resampled mono before allocating. FFT/filter scratch
    // is bounded independently of duration. The limit is a caller resource budget.
    constexpr size_t scratchBytes = 256 * 1024;
    const double needed = samples * sizeof(float) + frames * result.dimensions * sizeof(float) + scratchBytes;
    if (!std::isfinite(needed) || needed > (double) maximumBytes)
        return fail("AUFTAKT frontend exceeds the requested PCM/feature memory budget.");
    for (int ch = 0; ch < pcm.getNumChannels(); ++ch)
        for (int i = 0; i < pcm.getNumSamples(); ++i)
            if (!std::isfinite(pcm.getSample(ch, i))) return fail("Non-finite frontend PCM; no partial features returned.");
    std::vector<float> mono; mono.reserve((size_t) samples);
    auto mixed = [&](int index)
    {
        double sum = 0;
        for (int ch = 0; ch < pcm.getNumChannels(); ++ch) sum += pcm.getSample(ch, index);
        return sum / pcm.getNumChannels(); // Recovered arithmetic channel mean, including antiphase cancellation.
    };
    if (rate == result.sampleRate)
        for (int i = 0; i < pcm.getNumSamples(); ++i) mono.push_back((float) mixed(i));
    else
    {
        // Alloy's existing resampling library; vendor resampler equivalence is unknown.
        r8b::CDSPResampler24 resampler(rate, result.sampleRate, 4096);
        std::array<double, 4096> block {};
        int64_t position = 0;
        while (mono.size() < (size_t) samples)
        {
            const int count = position < pcm.getNumSamples() ? (int) std::min<int64_t>(4096, pcm.getNumSamples() - position) : 4096;
            std::fill(block.begin(), block.end(), 0);
            for (int i = 0; i < count && position + i < pcm.getNumSamples(); ++i) block[(size_t) i] = mixed((int) position + i);
            double* output = nullptr;
            const int produced = resampler.process(block.data(), count, output);
            for (int i = 0; i < produced && mono.size() < (size_t) samples; ++i) mono.push_back((float) output[i]);
            position += count;
        }
    }
    result.values.resize((size_t) frames * result.dimensions, 0);
    int bandBase = 0, column = 0;
    for (size_t resolution = 0; resolution < sizes.size(); ++resolution)
    {
        const int length = sizes[resolution], bands = counts[resolution], lag = lags[resolution];
        std::vector<float> window((size_t) length), input((size_t) length), magnitude((size_t) length / 2 + 1);
        std::vector<std::complex<float>> spectrum(magnitude.size());
        // Recovered nonperiodic Hann variant: endpoints are at 1/(N+1), N/(N+1).
        // FFT normalization mode is zero: retain the unscaled window and DFT.
        for (int i = 0; i < length; ++i)
            window[(size_t) i] = 0.5f * (1 - std::cos(((float) (i + 1) * 6.2831855f) / (float) (length + 1)));
        for (size_t frame = 0; frame < result.frames(); ++frame)
        {
            const int64_t begin = (int64_t) frame * result.hop - length / 2;
            for (int i = 0; i < length; ++i)
            {
                const auto source = begin + i;
                input[(size_t) i] = source >= 0 && source < (int64_t) mono.size() ? mono[(size_t) source] * window[(size_t) i] : 0;
            }
            pocketfft::r2c({(size_t) length}, {sizeof(float)}, {sizeof(std::complex<float>)}, 0, true,
                          input.data(), spectrum.data(), 1.0f);
            for (size_t bin = 0; bin < magnitude.size(); ++bin) magnitude[bin] = std::abs(spectrum[bin]);
            const size_t row = frame * result.dimensions + column;
            for (int band = 0; band < bands; ++band)
            {
                const auto& filter = recoveredBands[bandBase + band];
                float sum = 0;
                for (int k = 0; k < filter.count; ++k)
                    sum += magnitude[(size_t) filter.firstBin + k] * recoveredWeights[filter.offset + k];
                const float level = std::log(1.0f + sum) * 0.4342944920063019f;
                result.values[row + band] = level;
                result.values[row + bands + band] = frame >= (size_t) lag
                    ? std::max(0.0f, level - result.values[(frame - lag) * result.dimensions + column + band]) : 0;
            }
        }
        bandBase += bands; column += bands * 2;
    }
    if (!std::all_of(result.values.begin(), result.values.end(), [](float v) { return std::isfinite(v); }))
        return fail("Frontend arithmetic overflow; no partial features returned.");
    result.signalPresent = std::any_of(result.values.begin(), result.values.end(), [](float v) { return v != 0; });
    return result;
}
}
