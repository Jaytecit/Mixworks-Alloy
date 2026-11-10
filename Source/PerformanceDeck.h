#pragma once
#include "Engine.h"
#include "AlloyLookAndFeel.h"

namespace mixworks
{
class HotcueButton final : public juce::TextButton
{
public:
    std::function<void()> onEdit;
    void mouseDown(const juce::MouseEvent& event) override
    { if (event.mods.isPopupMenu()) { if (onEdit) onEdit(); } else TextButton::mouseDown(event); }
};
// One surface, reparented between dock and satellite. Engine owns every value.
class PerformanceDeck final : public juce::Component
{
public:
    explicit PerformanceDeck(Engine&);
    std::function<void()> remember, changed;
    std::function<void(MidiAction)> action;
    std::function<void(int)> cue;
    std::function<void(const juce::String&, int)> editCue;
    std::function<bool(int)> cueSelected;
    std::function<void()> scheduledPause;
    std::function<void(const LaunchSettings&)> launchSettingsChanged;
    std::function<void(const juce::String&)> status;
    juce::PopupMenu launchMenu() const;
    void performLaunchMenu(int command, const LaunchSettings& expected, const juce::String& owner = {});
    void triggerReset(int slot);
    juce::PopupMenu controlsMenu() const;
    void performControlsMenu(int command, const juce::String& owner = {});
    void refresh(bool available, bool visibleOnly = false);
    void setForgeEffects(bool enabled) { if (forgeEffects != enabled) { forgeEffects = enabled; repaint(); } }
    void paint(juce::Graphics&) override;
    void resized() override;
    std::array<AlloyDial, 8> pots;
    std::array<juce::Slider, 8> faders;
    juce::Slider master;
    std::array<juce::Slider, 2> wheels;
    std::array<juce::ComboBox, 2> wheelTargets;
    std::array<juce::TextButton, 16> pads;
    std::array<juce::TextButton, 8> buttons;
    std::array<HotcueButton, 9> channelButtons;
    juce::TextButton controls {"Cues / resets..."};
    juce::ToggleButton learn {"MIDI Learn"};
    juce::ToggleButton followTimeline {"Follow timeline"};
    juce::TextButton cancelLearn {"Cancel learn"};
    juce::TextButton setup {"Map live desk"}, skip {"Skip"};
private:
    Engine& engine;
    bool updating = false;
    bool forgeEffects = false;
    std::array<juce::Label, 17> names;
    std::array<juce::Label, 2> wheelNames;
    std::array<bool, 2> wheelDragging {}, wheelMoved {};
    juce::Label title, state, hint, launchState;
    std::array<bool, 17> gestures {};
    std::array<AutomationAddress, 17> gestureAddresses;
    std::array<AutomationAddress, 17> displayedAddresses;
    AutomationAddress timelineAddress(int lane, AutomationTarget) const;
    void configure(juce::Slider&, int slot, MidiAction, AutomationAddress, const juce::String&);
    void configureWheel(int slot);
    void trigger(MidiAction);
};
}
