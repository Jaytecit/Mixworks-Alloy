#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <vector>

namespace mixworks
{
struct MidiPortConnection
{
    juce::String id, name;
    bool open = false, received = false, recent = false;
};
// Message-thread policy; observations come from AudioDeviceManager, not device names alone.
class MidiReconnectPolicy
{
public:
    struct Request { juce::String id; bool enabled; };
    std::vector<Request> update(const std::vector<MidiPortConnection>&);
private:
    struct Port { juce::String id, name; bool wanted = false, present = false; };
    std::vector<Port> known;
};
struct MidiConnectionStatus
{
    enum class State { absent, detected, connected, receiving };
    std::vector<MidiPortConnection> ports;
    bool listening = false;
    int mappings = 0;
    juce::File profile;
    juce::String profileError, mappingTitle;
    State state() const;
    juce::String caption() const;
    juce::String description() const;
};
}
