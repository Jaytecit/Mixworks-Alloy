#pragma once
#include "Engine.h"
#include "AlloyLookAndFeel.h"

namespace mixworks
{
// One surface, reparented between dock and satellite. Engine owns every value.
class PerformanceDeck final : public juce::Component
{
public:
    explicit PerformanceDeck(Engine&);
    std::function<void()> remember, changed;
    std::function<void(MidiAction)> action;
    void refresh(bool available);
    void setForgeEffects(bool enabled) { if (forgeEffects != enabled) { forgeEffects = enabled; repaint(); } }
    void paint(juce::Graphics&) override;
    void resized() override;
    std::array<AlloyDial, 8> pots;
    std::array<juce::Slider, 8> faders;
    juce::Slider master;
    std::array<juce::TextButton, 16> pads;
    std::array<juce::TextButton, 8> buttons;
    std::array<juce::TextButton, 9> channelButtons;
    juce::ToggleButton learn {"MIDI Learn"};
    juce::TextButton cancelLearn {"Cancel learn"};
    juce::TextButton setup {"Map live desk"}, skip {"Skip"};
private:
    Engine& engine;
    bool updating = false;
    bool forgeEffects = false;
    std::array<juce::Label, 17> names;
    juce::Label title, state, hint;
    std::array<bool, 17> gestures {};
    std::array<AutomationAddress, 17> gestureAddresses;
    void configure(juce::Slider&, int slot, MidiAction, AutomationAddress, const juce::String&);
    void trigger(MidiAction);
};
}
