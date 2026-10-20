#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

namespace mixworks
{
namespace colour
{
inline const juce::Colour background {0xff15191d}, panel {0xff20262b}, raised {0xff2a3239}, border {0xff3b454e},
    text {0xffe7ebee}, muted {0xffabb5be}, orange {0xffd49a72}, focus {0xffd5e9f5}, error {0xffeb8280};
inline const std::array<juce::Colour, 8> tracks {juce::Colour(0xffd49a72), juce::Colour(0xff80b2cf), juce::Colour(0xff9eb8a1), juce::Colour(0xffb5a3c5), juce::Colour(0xffc8b57a), juce::Colour(0xff7fbab7), juce::Colour(0xffcc949c), juce::Colour(0xff9baad1)};
inline const juce::Colour moltenEdge {0xff753519}, moltenOrange {0xffff933d}, moltenCore {0xffffe1a0};
}

struct AlloyBeatPulse { int count = 0; float heat = .36f; };

class AlloyLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    static bool editingSliderText(const juce::Slider&);
    AlloyLookAndFeel();
    void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawLinearSlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    juce::Font getTextButtonFont(juce::TextButton&, int) override;
    void drawComboBox(juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getMenuBarFont(juce::MenuBarComponent&, int, const juce::String&) override;
    juce::Font getAlertWindowTitleFont() override;
    juce::Font getAlertWindowMessageFont() override;
    juce::Font getAlertWindowFont() override;
    int getAlertWindowButtonHeight() override { return 28; }
    void drawAlertBox(juce::Graphics&, juce::AlertWindow&, const juce::Rectangle<int>&, juce::TextLayout&) override;
    void drawPopupMenuBackground(juce::Graphics&, int, int) override;
    void drawFileBrowserRow(juce::Graphics&, int, int, const juce::File&, const juce::String&, juce::Image*,
        const juce::String&, const juce::String&, bool, bool, int, juce::DirectoryContentsDisplayComponent&) override;
    void drawScrollbar(juce::Graphics&, juce::ScrollBar&, int, int, int, int, bool, int, int, bool, bool) override;
    static void drawMoltenMetal(juce::Graphics&, juce::Rectangle<float>, bool vertical, float heat = .5f);
    static juce::Colour metalHeat(float amount);
    static AlloyBeatPulse fourCountPulse(double mixBeats, bool playing, bool animate);
    static bool motionAllowed();
    static void drawEmblem(juce::Graphics&, juce::Rectangle<float>);
    static bool styleNativeTitleBar(juce::Component&);
};

// Keeps JUCE's value/accessibility/text-entry and drag notifications. Only the
// rotary appearance and fine-drag/focus-wheel policy differ from a normal slider.
class AlloyDial final : public juce::Slider
{
public:
    void configure(const juce::String& name, double defaultValue = 0);
    void mouseDown(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;
private:
    double resetValue = 0;
    bool configured = false;
};
}
