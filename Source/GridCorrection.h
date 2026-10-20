#pragma once
#include "Model.h"

namespace mixworks
{
struct SourceCorrection
{
    double bpm = 0, downbeat = -1;
    int key = -1;
    static SourceCorrection from(const Media& m) { return {m.manualBpm, m.manualBeatOrigin, m.manualKey}; }
    static juce::Result parse(const juce::String& bpm, const juce::String& downbeat, int key, double duration, SourceCorrection&);
    juce::Result validate(double duration) const;
    SourceBeatGrid grid(const Media& m) const { return resolveSourceGrid(m.analysis, bpm, downbeat); }
    void apply(Media& m) const { m.manualBpm = bpm; m.manualBeatOrigin = downbeat; m.manualKey = key; }
};

class TapTempo
{
public:
    std::optional<double> tap(double monotonicSeconds);
    void reset() { times.clear(); }
    size_t count() const { return times.size(); }
private:
    std::vector<double> times;
};

// Source clock and PCM for monitoring only. Engine owns synchronization;
// callback rendering neither allocates nor releases media ownership.
class GridAudition
{
public:
    juce::Result start(std::shared_ptr<const Media>, SourceBeatGrid, double cursor, bool source, bool click, float gainDb);
    void configure(SourceBeatGrid grid, bool source, bool click, float gainDb);
    void stop();
    void render(juce::AudioBuffer<float>&, int frames, double outputRate);
    bool active() const { return running; }
    double position() const { return cursor; }
private:
    std::shared_ptr<const Media> media;
    SourceBeatGrid grid;
    double cursor = 0;
    float gain = 0.25f;
    bool running = false, sourceOn = true, clickOn = true;
};
}
