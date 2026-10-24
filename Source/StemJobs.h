#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>

namespace mixworks
{
struct StemJobState
{
    juce::String message = "Ready to split", error;
    juce::File directory;
    double progress = 0;
    bool running = false, ready = false, reused = false;
};

// One isolated process per explicit job; never touches Media, Project or analysis.
class StemJob final : private juce::Thread
{
public:
    StemJob(juce::File source, juce::File root, juce::File runtime, juce::String device = "auto");
    ~StemJob() override;
    void begin();
    void cancel();
    void finish();
    StemJobState state() const;
    static juce::File defaultRoot();
    static juce::File defaultRuntime();
private:
    void run() override;
    void update(const juce::String&, double progress = 0);
    juce::File source, root, runtime;
    juce::String device;
    mutable juce::CriticalSection mutex;
    StemJobState current;
    std::atomic<bool> cancelled {false};
};
void runStemTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runStemIntegration(const std::function<void(bool, const juce::String&)>&, const juce::File&, const juce::StringArray&);
}
