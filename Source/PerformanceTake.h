#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

namespace mixworks
{
enum class WorkMode { arrange, recordPerform };
enum class TakeState { idle, recording, paused, finalising, complete };
enum class TakeEventKind { control, transport, seek, clipJump, mute, solo, tempo, returnAutomation, project, pluginState, midi };
struct TakeEvent
{
    int64_t frame = 0, sequence = 0;
    TakeEventKind kind = TakeEventKind::control;
    int lane = 0, control = 0, stem = -1;
    juce::String owner, slot, parameter;
    double value = 0, sourcePosition = 0;
    juce::var payload;
};
struct PerformanceTake
{
    static constexpr size_t maximumEvents = 65536;
    juce::String id = juce::Uuid().toString(), name, issue, masterWav;
    juce::var initial;
    std::vector<TakeEvent> events;
    double sampleRate = 44100, initialPosition = 0;
    int64_t frames = 0;
    TakeState state = TakeState::idle;
    bool complete = true;
};
juce::var takeToJson(const PerformanceTake&);
juce::Result readTake(const juce::var&, PerformanceTake&);
}
