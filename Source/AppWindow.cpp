#include "AppWindow.h"
#include "AlloyLookAndFeel.h"
#include "Workstation.h"

namespace mixworks
{
namespace
{
const juce::Point<int> windowPresets[] {{1440, 810}, {1600, 900}, {1920, 1080}, {2560, 1440}};
juce::Rectangle<int> desktopSpan()
{
    const auto& displays = juce::Desktop::getInstance().getDisplays();
    std::vector<juce::Rectangle<int>> monitors;
    for (const auto& display : displays.displays) monitors.push_back(display.logicalBounds.getSmallestIntegerContainer());
    return AppWindow::spanningBounds(monitors);
}
}
juce::Rectangle<int> AppWindow::spanningBounds(const std::vector<juce::Rectangle<int>>& monitors)
{
    if (monitors.size() < 2) return {};
    juce::RectangleList<int> coverage;
    auto common = monitors.front();
    int top = common.getY(), bottom = common.getBottom(), left = common.getX(), right = common.getRight();
    for (const auto& monitor : monitors)
    {
        coverage.add(monitor); top = std::max(top, monitor.getY()); bottom = std::min(bottom, monitor.getBottom());
        left = std::max(left, monitor.getX()); right = std::min(right, monitor.getRight());
    }
    const auto all = coverage.getBounds();
    juce::Rectangle<int> result;
    // A small vertical offset between side-by-side monitors must not strand
    // menus above the shorter display. Use their shared, fully visible band.
    for (const auto candidate : {all, juce::Rectangle<int>(all.getX(), top, all.getWidth(), std::max(0, bottom - top)),
        juce::Rectangle<int>(left, all.getY(), std::max(0, right - left), all.getHeight())})
        if (candidate.getWidth() >= 1250 && candidate.getHeight() >= 800 && coverage.containsRectangle(candidate)
            && (juce::int64) candidate.getWidth() * candidate.getHeight() > (juce::int64) result.getWidth() * result.getHeight()) result = candidate;
    return result;
}
AppWindow::AppWindow(juce::Component* content)
    : juce::DocumentWindow("Mixworks Alloy", colour::background, juce::DocumentWindow::allButtons)
{
    setUsingNativeTitleBar(false); setTitleBarHeight(0);
    setContentOwned(content, false); setResizable(true, false);
    auto available = juce::Rectangle<int>(0, 0, 1440, 1060);
    if (const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        available = display->userBounds.getLargestIntegerWithin();
    available = available.reduced(8);
    windowedBounds = available.withSizeKeepingCentre(std::min(1440, available.getWidth()), std::min(810, available.getHeight()));
    const auto desktop = juce::Desktop::getInstance().getDisplays().getTotalBounds(false);
    setResizeLimits(std::min(1250, available.getWidth()), std::min(800, available.getHeight()), desktop.getWidth(), desktop.getHeight());
    setBounds(windowedBounds); setWindowMode(WindowMode::fullscreen);
}
WindowMode AppWindow::windowMode() const
{
    return (requestedMode == WindowMode::fullscreen || requestedMode == WindowMode::spanning) ? requestedMode : isFullScreen() ? WindowMode::maximised : WindowMode::windowed;
}
void AppWindow::setWindowMode(WindowMode mode)
{
    if (windowMode() == WindowMode::windowed) windowedBounds = getBounds();
    const auto restoreBounds = requestedMode == WindowMode::spanning && mode != WindowMode::spanning ? windowedBounds : getScreenBounds();
    const auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect(restoreBounds);
    if (!display) return;
    const auto screen = display->logicalBounds.getSmallestIntegerContainer();
    const auto work = display->userBounds.getLargestIntegerWithin().reduced(8);
    const bool visible = isVisible();
    setFullScreen(false);
    requestedMode = mode;
    const bool borderless = mode == WindowMode::fullscreen || mode == WindowMode::spanning;
    setResizable(!borderless, false);
    setDropShadowEnabled(!borderless);
    setTitleBarHeight(borderless ? 0 : 27);
    // A borderless peer at the display's logical bounds covers every monitor
    // pixel while respecting Windows DPI scaling. Maximising uses work bounds.
    juce::Component::addToDesktop(getDesktopWindowStyleFlags());
    if (mode == WindowMode::spanning) setBounds(desktopSpan());
    else if (mode == WindowMode::fullscreen) setBounds(screen);
    else
    {
        setBounds(windowedBounds.constrainedWithin(work));
        if (mode == WindowMode::maximised) setFullScreen(true);
    }
    resized(); setVisible(visible);
}
void AppWindow::toggleFullscreen() { setWindowMode((windowMode() == WindowMode::fullscreen || windowMode() == WindowMode::spanning) ? WindowMode::windowed : WindowMode::fullscreen); }
bool AppWindow::keyPressed(const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::F11Key) { toggleFullscreen(); return true; }
    return juce::DocumentWindow::keyPressed(key);
}
juce::PopupMenu AppWindow::displayMenu() const
{
    menuMonitorBounds.clear();
    juce::PopupMenu menu;
    const auto mode = windowMode();
    menu.addItem(fullscreen, "Fullscreen / fit monitor (F11)", true, mode == WindowMode::fullscreen);
    menu.addItem(maximised, "Maximised / keep taskbar", true, mode == WindowMode::maximised);
    menu.addItem(windowed, "Windowed", true, mode == WindowMode::windowed);
    menu.addItem(spanDisplays, "Span monitors / shared visible area", !desktopSpan().isEmpty(), mode == WindowMode::spanning);
    const auto& displays = juce::Desktop::getInstance().getDisplays();
    const auto* current = displays.getDisplayForRect(getScreenBounds());
    if (!current) return menu;
    juce::PopupMenu sizes;
    const auto available = current->userBounds.getLargestIntegerWithin().reduced(8);
    for (int i = 0; i < 4; ++i)
        sizes.addItem(size1440 + i, juce::String(windowPresets[i].x) + " x " + juce::String(windowPresets[i].y) + " (16:9)",
                      windowPresets[i].x <= available.getWidth() && windowPresets[i].y <= available.getHeight(),
                      mode == WindowMode::windowed && getWidth() == windowPresets[i].x && getHeight() == windowPresets[i].y);
    menu.addSeparator(); menu.addSubMenu("Window size", sizes);
    juce::PopupMenu monitors;
    for (int i = 0; i < displays.displays.size(); ++i)
    {
        const auto& display = displays.displays.getReference(i);
        menuMonitorBounds.push_back(display.logicalBounds.getSmallestIntegerContainer());
        monitors.addItem(monitorBase + i, "Monitor " + juce::String(i + 1) + (display.isMain ? " / primary / " : " / ")
                         + juce::String(display.physicalBounds.getWidth()) + " x " + juce::String(display.physicalBounds.getHeight()), true, &display == current);
    }
    menu.addSubMenu("Monitor", monitors); return menu;
}
bool AppWindow::performDisplayAction(int action)
{
    if (action == spanDisplays)
    {
        if (desktopSpan().isEmpty()) return false;
        setWindowMode(WindowMode::spanning); return true;
    }
    if (action == fullscreen) { setWindowMode(WindowMode::fullscreen); return true; }
    if (action == maximised) { setWindowMode(WindowMode::maximised); return true; }
    if (action == windowed) { setWindowMode(WindowMode::windowed); return true; }
    const auto& displays = juce::Desktop::getInstance().getDisplays();
    if (action >= monitorBase && action - monitorBase < (int) menuMonitorBounds.size())
    {
        const auto bounds = menuMonitorBounds[(size_t) (action - monitorBase)];
        const auto target = std::find_if(displays.displays.begin(), displays.displays.end(), [&](const auto& display)
            { return display.logicalBounds.getSmallestIntegerContainer() == bounds; });
        if (target == displays.displays.end()) return false;
        const auto mode = windowMode();
        const auto work = target->userBounds.getLargestIntegerWithin().reduced(8);
        setWindowMode(WindowMode::windowed);
        windowedBounds = work.withSizeKeepingCentre(std::min(windowedBounds.getWidth(), work.getWidth()), std::min(windowedBounds.getHeight(), work.getHeight()));
        setBounds(windowedBounds); setWindowMode(mode); return true;
    }
    if (action >= size1440 && action <= size2560)
    {
        const auto* display = displays.getDisplayForRect(getScreenBounds()); if (!display) return false;
        const auto work = display->userBounds.getLargestIntegerWithin().reduced(8);
        const auto size = windowPresets[action - size1440];
        if (size.x > work.getWidth() || size.y > work.getHeight()) return false;
        setWindowMode(WindowMode::windowed); windowedBounds = work.withSizeKeepingCentre(size.x, size.y); setBounds(windowedBounds); return true;
    }
    return false;
}
juce::BorderSize<int> AppWindow::getBorderThickness() const
{
    return (requestedMode == WindowMode::fullscreen || requestedMode == WindowMode::spanning) ? juce::BorderSize<int>() : juce::DocumentWindow::getBorderThickness();
}
int AppWindow::getDesktopWindowStyleFlags() const
{
    const auto style = juce::DocumentWindow::getDesktopWindowStyleFlags();
    return (requestedMode == WindowMode::fullscreen || requestedMode == WindowMode::spanning) ? style & ~(juce::ComponentPeer::windowHasTitleBar | juce::ComponentPeer::windowIsResizable
        | juce::ComponentPeer::windowHasDropShadow | juce::ComponentPeer::windowHasCloseButton | juce::ComponentPeer::windowHasMinimiseButton
        | juce::ComponentPeer::windowHasMaximiseButton) : style;
}
void AppWindow::closeButtonPressed()
{
    auto close = [] { if (auto* app = juce::JUCEApplication::getInstance()) app->quit(); };
    if (auto* workstation = dynamic_cast<Workstation*>(getContentComponent())) workstation->requestClose(close);
    else close();
}
}
