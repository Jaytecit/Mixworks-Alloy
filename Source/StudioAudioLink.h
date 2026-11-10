#pragma once
#include "Mastering.h"

namespace mixworks
{
class StudioAudioLink final : private juce::Thread
{
public:
    static constexpr int port = 49731;
    StudioAudioLink(SpectrumFeed&, const std::atomic<juce::uint64>&);
    ~StudioAudioLink() override;
    bool start(int receiverPort = port);
    static juce::uint64 encodeTempo(double detectedBpm, double mixBpm, double beat);
private:
    void run() override;
    SpectrumFeed& feed;
    const std::atomic<juce::uint64>& tempo;
    juce::DatagramSocket socket;
    int destinationPort = port;
};
}
