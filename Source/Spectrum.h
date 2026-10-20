#pragma once
#include "Mastering.h"
#include <juce_dsp/juce_dsp.h>

namespace mixworks
{
// UI-thread owner of history, windowing, FFT and calibrated stereo bin levels.
class SpectrumAnalysis
{
public:
    static constexpr int order = 12, size = 1 << order, bins = size / 2 + 1;
    SpectrumAnalysis();
    bool update(SpectrumFeed&);
    void clear();
    const std::array<float, bins>& levels() const { return db; }
    double rate() const { return sampleRate; }
    bool ready() const { return complete; }
private:
    juce::dsp::FFT fft {order};
    juce::dsp::WindowingFunction<float> window {size, juce::dsp::WindowingFunction<float>::hann, false};
    std::array<float, size> historyL {}, historyR {};
    std::array<float, size * 2> transformL {}, transformR {};
    std::array<float, bins> db {};
    int cursor = 0, examined = 0, newFrames = 0;
    double sampleRate = 0;
    juce::uint64 lastDropped = 0;
    bool complete = false;
};
}
