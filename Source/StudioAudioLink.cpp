#include "StudioAudioLink.h"
#include <algorithm>
#include <cmath>

namespace mixworks
{
StudioAudioLink::StudioAudioLink(SpectrumFeed& source, const std::atomic<juce::uint64>& clock)
    : Thread("Visual Studio audio"), feed(source), tempo(clock) {}

juce::uint64 StudioAudioLink::encodeTempo(double detectedBpm, double mixBpm, double beat)
{
    const auto bpm = [](double value) -> juce::uint64
    {
        return std::isfinite(value) && value > 0. && value <= 655.35 ? (juce::uint64) juce::roundToInt(value * 100.) : 0;
    };
    const auto phase = std::isfinite(beat) ? (juce::uint64) ((beat - std::floor(beat)) * 65536.) : 0;
    return bpm(detectedBpm) | (bpm(mixBpm) << 16) | (phase << 32);
}

StudioAudioLink::~StudioAudioLink()
{
    feed.enabled = false;
    signalThreadShouldExit();
    notify();
    stopThread(1000);
}

bool StudioAudioLink::start(int receiverPort)
{
    destinationPort = receiverPort;
    feed.discardAvailable();
    if (!socket.bindToPort(0, "127.0.0.1") || !startThread(juce::Thread::Priority::low)) return false;
    feed.enabled = true;
    return true;
}

void StudioAudioLink::run()
{
    constexpr int block = 1024;
    std::array<float, block> left {}, right {};
    std::array<double, block> rates {};
    std::array<char, 28 + block * sizeof(float)> packet {};
    juce::uint32 sequence = 0;
    auto drops = feed.dropped.load(std::memory_order_relaxed);
    while (!threadShouldExit())
    {
        const auto currentDrops = feed.dropped.load(std::memory_order_relaxed);
        if (currentDrops != drops)
        {
            feed.discardAvailable();
            drops = currentDrops;
            ++sequence;
        }
        const int frames = feed.read(left.data(), right.data(), rates.data(), block);
        if (frames == 0) { wait(8); continue; }
        const auto rate = rates[(size_t) frames - 1];
        int start = frames - 1;
        while (start > 0 && rates[(size_t) start - 1] == rate) --start;
        const int count = frames - start;
        // Like Alloy's visual worker, use the stronger side of the master to
        // keep stereo widening or inverted phase from cancelling the pulse.
        const auto peak = [start, count](const auto& channel)
        {
            const auto range = juce::FloatVectorOperations::findMinAndMax(channel.data() + start, count);
            return std::max(std::abs(range.getStart()), std::abs(range.getEnd()));
        };
        const auto& pcm = peak(left) >= peak(right) ? left : right;
        juce::MemoryOutputStream bytes(packet.data(), packet.size());
        bytes.write("MWV2", 4);
        bytes.writeInt((int) sequence++);
        bytes.writeInt(juce::roundToInt(rate));
        bytes.writeInt(count);
        const auto clock = tempo.load(std::memory_order_relaxed);
        bytes.writeFloat((float) (clock & 0xffffu) / 100.f);
        bytes.writeFloat((float) ((clock >> 16) & 0xffffu) / 100.f);
        bytes.writeFloat((float) ((clock >> 32) & 0xffffu) / 65536.f);
        for (int i = start; i < frames; ++i) bytes.writeFloat(pcm[(size_t) i]);
        if (socket.waitUntilReady(false, 0) == 1)
        {
            const int connected = connectedPort.load(std::memory_order_relaxed);
            if (destinationPort > 0)
                socket.write("127.0.0.1", destinationPort, packet.data(), (int) bytes.getDataSize());
            if (connected > 0 && connected != destinationPort)
                socket.write("127.0.0.1", connected, packet.data(), (int) bytes.getDataSize());
        }
    }
}
}
