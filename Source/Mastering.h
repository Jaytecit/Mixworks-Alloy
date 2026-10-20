#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <vector>

namespace mixworks
{
struct MasterSettings
{
    std::array<float, 3> eqDb {};
    float width = 1, saturationDriveDb = 0;
    bool saturation = false;
    juce::Result validate() const;
};
juce::var masterSettingsToJson(const MasterSettings&);
juce::Result readMasterSettings(const juce::var&, MasterSettings&);

// One audio producer and one UI consumer. All storage is allocated at creation;
// sample-rate metadata travels with samples rather than relabelling queued audio.
class SpectrumFeed
{
public:
    static constexpr int capacity = 8192;
    SpectrumFeed();
    std::atomic<bool> enabled {false};
    std::atomic<juce::uint64> dropped {0};
    int push(const juce::AudioBuffer<float>&, int frames, double sampleRate);
    int read(float* left, float* right, double* rates, int wanted);
    void discardAvailable(); // Consumer only; producer may continue writing.
private:
    juce::AudioBuffer<float> data;
    std::vector<double> sampleRates;
    juce::AbstractFifo fifo;
};
}
