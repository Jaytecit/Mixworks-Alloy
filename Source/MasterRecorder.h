#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
namespace mixworks
{
// A bounded producer. Disk work belongs to JUCE's background writer; a failed
// write makes the partial take visibly incomplete and never stops programme PCM.
class MasterRecorder
{
public:
    MasterRecorder();
    ~MasterRecorder();
    juce::Result start(const juce::File&,double rate,int fifoFrames = 131072);
    void push(const juce::AudioBuffer<float>&,int frames);
    juce::String finish();
    bool active() const { return writer!=nullptr; }
    bool failed() const { return overflow.load(); }
    juce::File file;
private:
    juce::TimeSliceThread thread {"Master WAV writer"};
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> writer;
    std::atomic<bool> overflow {false};
    int64_t acceptedFrames = 0;
};
}
