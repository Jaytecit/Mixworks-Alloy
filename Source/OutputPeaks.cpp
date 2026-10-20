#include "OutputPeaks.h"
#include <cmath>

namespace mixworks
{
void StereoPeakFeed::push(const juce::AudioBuffer<float>& audio, int frames)
{
    if (frames <= 0 || frames > audio.getNumSamples() || audio.getNumChannels() == 0) return;
    for (size_t ch = 0; ch < pending.size(); ++ch)
    {
        const float value = audio.getMagnitude(std::min((int) ch, audio.getNumChannels() - 1), 0, frames);
        if (!std::isfinite(value)) continue;
        auto previous = pending[ch].load(std::memory_order_relaxed);
        while (previous < value && !pending[ch].compare_exchange_weak(previous, value, std::memory_order_relaxed)) {}
    }
}
std::array<float, 2> StereoPeakFeed::take()
{ return {pending[0].exchange(0, std::memory_order_relaxed), pending[1].exchange(0, std::memory_order_relaxed)}; }
}
