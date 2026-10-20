#include "Workstation.h"

namespace mixworks
{
namespace
{
class ToolPanelWindow final : public AlloyPopupWindow<juce::DocumentWindow>
{
public:
    std::function<void()> dock;
    ToolPanelWindow(ToolPanel& panel, const juce::String& name) : AlloyPopupWindow("Alloy / " + name, colour::background, allButtons)
    {
        setUsingNativeTitleBar(true); setResizable(true, false); setResizeLimits(640, 360, 7680, 2160);
        setContentNonOwned(&panel, false);
    }
    void closeButtonPressed() override { if (dock) dock(); }
    void resized() override { DocumentWindow::resized(); if (auto* c = getContentComponent()) c->resized(); }
};
}
void Workstation::initialisePerformance()
{
    performance = std::make_unique<PerformanceDeck>(engine);
    addAndMakeVisible(performancePanel); performanceViewport.setViewedComponent(performance.get(), false);
    performanceViewport.setScrollBarsShown(true, true);
    performanceViewport.onResize = [this] { layoutPerformance(); };
    performance->remember = [this] { if (!busy) remember(); };
    performance->changed = [this] { dirty = true; };
    performance->action = [this](MidiAction a) { performAction(a); };
    addAndMakeVisible(controllerTab); controllerTab.getProperties().set("alloyTab", true);
    controllerTab.onClick = [this] { showToolPage(0); };
    performancePanel.dock.onClick = [this] { detachToolPanel(0, !performanceWindow); };
    performancePanel.onKey = transitionPanel.onKey = [this](const juce::KeyPress& key) { return keyPressed(key); };
    transitionPanel.dock.onClick = [this] { detachToolPanel(1, !transitionWindow); };
    engine.onPerformanceAction = [this](MidiAction a)
    {
        if (busy) return;
        if (a.kind == MidiActionKind::previousTransition || a.kind == MidiActionKind::nextTransition) navigateTransition(a.kind == MidiActionKind::nextTransition ? 1 : -1);
        else if (a.kind == MidiActionKind::undo || a.kind == MidiActionKind::redo) undo(a.kind == MidiActionKind::redo);
    };
}
void Workstation::performAction(MidiAction a)
{
    if (busy || engine.exporting) return;
    // Engine captures project switches before mutation for both MIDI and mouse input.
    if ((a.kind == MidiActionKind::resetEq || a.kind == MidiActionKind::resetLowPass) && !engine.canWriteAutomation(a.index)) remember();
    if (a.kind == MidiActionKind::resetPans && !engine.recordingAutomation()) remember();
    dirty = engine.applyMidiControl(a, 1) || dirty;
    refresh();
}
void Workstation::layoutPerformance()
{
    if (!performance) return;
    const int width = performanceViewport.getWidth(), height = performanceViewport.getHeight();
    const int bar = performanceViewport.getScrollBarThickness();
    bool vertical = height < 276;
    bool horizontal = width - (vertical ? bar : 0) < 1240;
    vertical = height - (horizontal ? bar : 0) < 276;
    horizontal = width - (vertical ? bar : 0) < 1240;
    performance->setSize(std::max(1240, width - (vertical ? bar : 0)), std::max(276, height - (horizontal ? bar : 0)));
}
void Workstation::layoutToolPanels()
{
    auto area = layout.performance;
    auto tabs = area.removeFromTop(32);
    controllerTab.setBounds(tabs.removeFromLeft(108).withTrimmedRight(4));
    transitionTab.setBounds(tabs.removeFromLeft(108).withTrimmedRight(4));
    controllerTab.setToggleState(toolPage == 0, juce::dontSendNotification);
    transitionTab.setToggleState(toolPage == 1, juce::dontSendNotification);
    performancePanel.setVisible(performanceWindow || toolPage == 0);
    transitionPanel.setVisible(transitionWindow || toolPage == 1);
    if (!performanceWindow) performancePanel.setBounds(area);
    if (!transitionWindow) transitionPanel.setBounds(area);
    performancePanel.dock.setDetached(performanceWindow != nullptr);
    transitionPanel.dock.setDetached(transitionWindow != nullptr);
    layoutPerformance(); layoutTransitionUI();
}
void Workstation::showToolPage(int page)
{
    page = juce::jlimit(0, 1, page);
    auto* window = page == 0 ? performanceWindow.get() : transitionWindow.get();
    if (window) { window->setMinimised(false); window->toFront(true); return; }
    toolPage = page; resized(); repaint();
}
void Workstation::detachToolPanel(int page, bool detached)
{
    auto& ownedWindow = page == 0 ? performanceWindow : transitionWindow;
    auto& panel = page == 0 ? performancePanel : transitionPanel;
    if (detached == (ownedWindow != nullptr)) return;
    if (detached)
    {
        auto window = std::make_unique<ToolPanelWindow>(panel, page == 0 ? "Controller" : "Transitions");
        window->setLookAndFeel(&look);
        // Native close callbacks return before their window is destroyed.
        window->dock = [safe = juce::Component::SafePointer<Workstation>(this), page]
        { juce::MessageManager::callAsync([safe, page] { if (safe) safe->detachToolPanel(page, false); }); };
        auto work = getScreenBounds();
        const auto& displays = juce::Desktop::getInstance().getDisplays();
        const auto* current = displays.getDisplayForRect(getScreenBounds());
        const auto target = std::find_if(displays.displays.begin(), displays.displays.end(), [&](const auto& display) { return &display != current; });
        if (target != displays.displays.end()) work = target->userBounds.getLargestIntegerWithin();
        else if (current) work = current->userBounds.getLargestIntegerWithin();
        window->setBounds(work.withSizeKeepingCentre(std::min(1440, work.getWidth()), std::min(520, work.getHeight())));
        ownedWindow = std::move(window); ownedWindow->setVisible(true);
        AlloyLookAndFeel::styleNativeTitleBar(*ownedWindow);
        if (toolPage == page) toolPage = 1 - page;
    }
    else
    {
        ownedWindow->clearContentComponent(); ownedWindow->setLookAndFeel(nullptr); ownedWindow.reset();
        addAndMakeVisible(panel); toolPage = page;
    }
    resized(); repaint();
}
}
