#pragma once
#include "Model.h"
#include <SoundTouch.h>
#include <limits>

namespace mixworks
{
// Immutable placement facts. Splits share their original mapping so a cut cannot
// change the sound. Construction performs no decoding, analysis or stretching.
struct ElasticSource
{
    explicit ElasticSource(const Clip&);
    Clip clip;
    double progressAt(double sessionTime) const;
    double timeAtProgress(double sourceSeconds) const;
    float sample(int channel, double sourceProgress, int stem = -1) const;
};

// Continuous pitch-preserving streams with bounded recent-output caches.
// Cache storage and DSP history belong to the engine, never to undo or the UI.
class ElasticRenderer
{
public:
    static constexpr double windowSeconds = 0.1;
    void beginBlock() { ++clock; }
    void add(juce::AudioBuffer<float>&, int frames, double at, double outputRate, const Clip&, int stem = -1);
    void prepare(const Clip&, double at, double outputRate, int frames); // Message thread; private live-cue stream only.
    void reset();
    uint64_t windowsRendered() const { return rendered; }
private:
    struct Window
    {
        int64_t index = std::numeric_limits<int64_t>::min();
        juce::AudioBuffer<float> pcm;
    };
    struct Stream
    {
        std::shared_ptr<const ElasticSource> source;
        std::array<Window, 4> windows;
        soundtouch::SoundTouch processor;
        int stem = -1;
        uint64_t age = 0;
        int64_t nextWindow = std::numeric_limits<int64_t>::min(), inputFrames = 0;
        double sourceOrigin = 0;
        std::array<float, 2> lastSample {};
    };
    std::vector<std::unique_ptr<Stream>> streams;
    std::array<float, 256> input {}, output {};
    uint64_t clock = 1, rendered = 0;
    Window& window(const std::shared_ptr<const ElasticSource>&, int64_t, int stem);
};
}
