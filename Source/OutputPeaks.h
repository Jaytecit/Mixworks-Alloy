#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>

namespace mixworks
{
// One device-callback producer, one display consumer. Accumulate transient peaks
// across callbacks so a short peak between UI frames cannot disappear.
class StereoPeakFeed
{
public:
    void push(const juce::AudioBuffer<float>&, int frames);
    std::array<float, 2> take();
private:
    std::array<std::atomic<float>, 2> pending {};
};
}
