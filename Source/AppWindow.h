#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace mixworks
{
enum class WindowMode { windowed, maximised, fullscreen, spanning };

// Owns monitor geometry and window state; both UI entry points use this owner.
class AppWindow final : public juce::DocumentWindow
{
public:
    explicit AppWindow(juce::Component* content);
    WindowMode windowMode() const;
    void setWindowMode(WindowMode);
    void toggleFullscreen();
    bool keyPressed(const juce::KeyPress&) override;
    juce::PopupMenu displayMenu() const;
    bool performDisplayAction(int);
    static juce::Rectangle<int> spanningBounds(const std::vector<juce::Rectangle<int>>& monitors);
    void closeButtonPressed() override;
    enum Action { fullscreen = 1000, maximised, windowed, size1440, size1600, size1920, size2560, spanDisplays, monitorBase = 1100 };
private:
    WindowMode requestedMode = WindowMode::windowed;
    juce::Rectangle<int> windowedBounds;
    mutable std::vector<juce::Rectangle<int>> menuMonitorBounds;
    juce::BorderSize<int> getBorderThickness() const override;
    int getDesktopWindowStyleFlags() const override;
};
}
