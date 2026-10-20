#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <memory>

namespace mixworks
{
struct WavExportOptions { bool includeTail = false; };
struct WavExportPlan
{
    bool available = false; // Timing/size facts can be available even if resource admission fails.
    double sampleRate = 0, arrangementEnd = 0, tailSeconds = 0;
    juce::int64 frames = 0, estimatedBytes = 0;
    double seconds() const { return sampleRate > 0 ? frames / sampleRate : 0; }
};
enum class WavExportOutcome { success, cancelled, failed };
struct WavExportResult
{
    WavExportOutcome outcome = WavExportOutcome::failed;
    juce::String error;
    WavExportPlan plan;
    juce::int64 writtenFrames = 0;
    float peak = 0;
    bool wasOk() const { return outcome == WavExportOutcome::success; }
};
using WavExportProgress = std::function<void(juce::int64 writtenFrames, const WavExportPlan&)>;
}
