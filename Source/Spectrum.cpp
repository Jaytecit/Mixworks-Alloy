#include "Spectrum.h"
#include <cmath>

namespace mixworks
{
SpectrumAnalysis::SpectrumAnalysis() { clear(); }
void SpectrumAnalysis::clear()
{
    cursor = examined = newFrames = 0; sampleRate = 0; complete = false; db.fill(-120);
    historyL.fill(0); historyR.fill(0);
}
bool SpectrumAnalysis::update(SpectrumFeed& feed)
{
    const auto dropped = feed.dropped.load(std::memory_order_relaxed);
    if (dropped != lastDropped) { clear(); feed.discardAvailable(); lastDropped = dropped; return false; }
    std::array<float, 512> left {}, right {}; std::array<double, 512> rates {};
    int remaining = SpectrumFeed::capacity;
    for (int received; remaining > 0 && (received = feed.read(left.data(), right.data(), rates.data(), std::min(512, remaining))) > 0;)
    {
        remaining -= received;
        for (int i = 0; i < received; ++i)
        {
            if (rates[(size_t) i] != sampleRate) { clear(); sampleRate = rates[(size_t) i]; }
            historyL[(size_t) cursor] = left[(size_t) i]; historyR[(size_t) cursor] = right[(size_t) i];
            cursor = (cursor + 1) % size; examined = std::min(size, examined + 1); ++newFrames;
        }
    }
    if (feed.dropped.load(std::memory_order_relaxed) != lastDropped) { clear(); return false; }
    if (examined < size || newFrames < size / 2) return false;
    newFrames = 0; transformL.fill(0); transformR.fill(0);
    for (int i = 0; i < size; ++i) { transformL[(size_t) i] = historyL[(size_t) ((cursor + i) % size)]; transformR[(size_t) i] = historyR[(size_t) ((cursor + i) % size)]; }
    window.multiplyWithWindowingTable(transformL.data(), size); window.multiplyWithWindowingTable(transformR.data(), size);
    fft.performFrequencyOnlyForwardTransform(transformL.data()); fft.performFrequencyOnlyForwardTransform(transformR.data());
    // Non-normalized symmetric Hann has sum (N-1)/2. Each non-DC bin
    // reports coherent peak amplitude from mean stereo spectral power.
    const double scale = 4.0 / (size - 1);
    for (int i = 0; i < bins; ++i)
    {
        const double l = transformL[(size_t) i], r = transformR[(size_t) i];
        const double amplitude = std::sqrt((l * l + r * r) * .5) * scale * (i == 0 || i == size / 2 ? .5 : 1);
        db[(size_t) i] = (float) std::max(-120.0, 20 * std::log10(std::max(1.0e-12, amplitude)));
    }
    complete = true; return true;
}
}
