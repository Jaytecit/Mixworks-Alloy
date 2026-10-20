#pragma once
#include "AlloyLookAndFeel.h"
#include <vector>

namespace mixworks
{
enum class AttractMotion { system, animated, steady };
struct AttractState
{
    bool enabled = false;
    double startedAtMs = 0;
    AttractMotion motion = AttractMotion::system;
    bool allowsMotion() const { return motion == AttractMotion::animated || (motion == AttractMotion::system && AlloyLookAndFeel::motionAllowed()); }
};

// Presentation only: no Engine, model, MIDI, parameter or control callbacks.
class AttractLights final : public juce::Component
{
public:
    struct Light { float heat = 0, phase = 0; juce::Colour colour; int pattern = 0; };
    AttractLights(juce::Component& owner, const AttractState& shared);
    void syncMode();
    void paint(juce::Graphics&) override;
    void setPanels(std::vector<juce::Rectangle<float>> value) { panels = std::move(value); }
    static Light lightAt(double seconds, float x, float y, int ordinal, int count, bool animate);
private:
    enum class Kind { button, pad, dial, linear, bank, panel, keys };
    struct Target { juce::Rectangle<float> bounds; Kind kind; bool vertical = false; };
    juce::Component& host;
    const AttractState& state;
    std::vector<juce::Rectangle<float>> panels;
    void collect(juce::Component&, std::vector<Target>&);
    void illuminate(juce::Graphics&, const Target&, const Light&);
};
}
