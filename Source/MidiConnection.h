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
struct MidiConnectionStatus
{
    enum class State { absent, detected, connected, receiving };
    std::vector<MidiPortConnection> ports;
    bool listening = false;
    int mappings = 0;
    juce::File profile;
    juce::String profileError;
    State state() const;
    juce::String caption() const;
    juce::String description() const;
};
}
