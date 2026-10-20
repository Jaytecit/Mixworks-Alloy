#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

namespace mixworks
{
// Non-owning, read-only PCM. The buffer/mapping owner must outlive this view.
// An explicit planar stride preserves channel boundaries for mapped sources.
class PcmView
{
public:
    PcmView() = default;
    PcmView(const juce::AudioBuffer<float>& buffer)
        : external(buffer.getArrayOfReadPointers()), channelCount(buffer.getNumChannels()), frameCount(buffer.getNumSamples()) {}
    PcmView(const float* planar, int channels, int frames, int channelStride)
        : first(planar), channelCount(channels), frameCount(frames), stride(channelStride) {}
    int getNumChannels() const { return channelCount; }
    int getNumSamples() const { return frameCount; }
    const float* getReadPointer(int channel) const
    { jassert(channel >= 0 && channel < channelCount); return external ? external[channel] : first + (size_t) channel * stride; }
    float getSample(int channel, int frame) const { jassert(frame >= 0 && frame < frameCount); return getReadPointer(channel)[frame]; }
private:
    const float* const* external = nullptr;
    const float* first = nullptr;
    int channelCount = 0, frameCount = 0, stride = 0;
};
}
