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
    performance->cue = [this](int slot) { locateHotcue(slot); };
    performance->editCue = [this](const juce::String& id, int slot) { editHotcue(id, slot); };
    performance->cueSelected = [this](int slot) { return timeline.selectedHotcue && timeline.selectedHotcue->clipId == selectedClip && timeline.selectedHotcue->slot == slot; };
    performance->status = [this](const juce::String& text) { setStatus(text); };
    performance->scheduledPause = [this]
    {
        if (busy || pendingTempo > 0 || tempoPreparationPending || engine.isMidiLearnEnabled()) return;
        const auto result = engine.toggleScheduledPause();
        setStatus(result.failed() ? result.getErrorMessage() : engine.scheduledPauseText());
    };
    performance->launchSettingsChanged = [this](const LaunchSettings& settings)
    {
        if (busy || engine.exporting || !engine.controlInputEnabled || pendingTempo > 0 || tempoPreparationPending || engine.rehearsalActive() || engine.isMidiLearnEnabled()) return;
        if (engine.automationRecordMode != AutomationRecordMode::read || engine.recordingAutomation())
        { setStatus("Finish the automation take and choose Read before changing saved launch timing."); return; }
        const auto valid = settings.validate(); if (valid.failed()) { setStatus(valid.getErrorMessage()); return; }
        const juce::ScopedLock guard(engine.lock); if (engine.project.launch == settings) return;
        remember(); const auto result = engine.setLaunchSettings(settings);
        setStatus(result.failed() ? result.getErrorMessage() : "Programme launch timing: " + juce::String(launchTimingName(settings.timing))
            + " / phrase " + juce::String(settings.phraseBars) + " bars. Pending hotcues and pause cancelled; trigger again."); refresh();
    };
    addAndMakeVisible(controllerTab); controllerTab.getProperties().set("alloyTab", true);
    addAndMakeVisible(editingTab); editingTab.getProperties().set("alloyTab", true);
    editingTab.onClick = [this] { toggleConsole(2); };
    editingPanel.dock.onClick = [this] { detachToolPanel(2, !editingWindow); };
    controllerTab.onClick = [this] { toggleConsole(0); };
    performancePanel.dock.onClick = [this] { detachToolPanel(0, !performanceWindow); };
    performancePanel.onKey = transitionPanel.onKey = [this](const juce::KeyPress& key) { return keyPressed(key); };
    editingPanel.onKey = [this](const juce::KeyPress& key)
    {
        if (!dynamic_cast<juce::TextEditor*>(juce::Component::getCurrentlyFocusedComponent()) && automationLane() >= 0 && !key.getModifiers().isAnyModifierKeyDown())
        {
            // An automation Delete must never fall through to deleting its segment,
            // even when its node was cleared or invalidated by another view.
            if (key.getKeyCode() == juce::KeyPress::deleteKey)
            {
                if (!timeline.keyPressed(key)) setStatus("Select a saved manual node to delete. The segment was kept.");
                return true;
            }
            if (timeline.keyPressed(key)) return true;
        }
        return keyPressed(key);
    };
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
    if (a.kind == MidiActionKind::record && engine.hasLiveHotcueVoices())
    { setStatus("Stop before recording an arrangement take. Live hotcue event recording is not available yet; the live source cursor was kept."); return; }
    // Engine captures project switches before mutation for both MIDI and mouse input.
    if ((a.kind == MidiActionKind::resetEq || a.kind == MidiActionKind::resetCutoff) && !engine.canWriteAutomation(a.index)) remember();
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
void Workstation::initialiseEditing()
{
    addAndMakeVisible(editingPanel);
    editingViewport.setViewedComponent(&editingBody, false);
    editingViewport.setScrollBarsShown(true, true);
    editingViewport.onResize = [this] { layoutEditing(); };
    for (auto* control : std::initializer_list<juce::Component*> {&inspectorLabel, &clipDetail, &reverseButton, &loopButton, &editingSelect, &editingCut, &editingCopy, &editingPaste, &editingRipple, &editingCloseGap}) editingBody.addAndMakeVisible(control);
    for (auto& control : clipSliders) editingBody.addAndMakeVisible(control);
    for (auto& control : clipLabels) editingBody.addAndMakeVisible(control);
    inspectorLabel.setText("SEGMENT", juce::dontSendNotification);
    editingSelect.setTooltip("Select a placed segment by source and lane. Selection keeps the playhead, sound and Undo unchanged.");
    editingCut.setTooltip("Cut the selected segment into the internal clipboard. Other segments keep their positions; Undo restores it. Original source audio is kept.");
    editingCopy.setTooltip("Copy the selected segment, its automation and effects into the internal clipboard. Paste at the playhead on the selected lane.");
    editingPaste.setTooltip("Paste at the playhead on this lane. Existing segments stay in place, and the new copy has separate effects.");
    editingRipple.setTooltip("Delete this segment and close its time across all lanes. Later audio, automation and markers move left; Undo restores the edit.");
    editingCloseGap.setTooltip("Remove the empty gap before this segment across all lanes. Later audio, automation and markers move left; Undo restores the edit.");
    editingSelect.onClick = [this]
    {
        if (busy || engine.exporting) return;
        juce::PopupMenu menu; std::vector<juce::String> ids;
        for (const auto& clip : engine.project.clips)
        { ids.push_back(clip.id); menu.addItem((int) ids.size(), "Segment " + juce::String((int) ids.size()) + " / " + clip.media->libraryTitle() + " / lane " + juce::String(clip.track + 1) + " / " + timeText(clip.start - engine.project.tempo.origin), true, clip.id == selectedClip); }
        if (ids.empty()) { setStatus("Add a song to create a segment first."); return; }
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&editingSelect), [safe = juce::Component::SafePointer<Workstation>(this), ids](int result)
        {
            if (!safe || safe->busy || safe->engine.exporting || result <= 0 || result > (int) ids.size()) return;
            const auto* clip = safe->engine.project.clipFor(ids[(size_t) result - 1]); if (!clip) return;
            safe->selectedClip = clip->id; safe->selectedAutomationTarget = -1; safe->timeline.selectAutomationPoint({}, -1);
            safe->selectTrack(clip->track); safe->refresh(); safe->setStatus("Selected segment / " + clip->media->libraryTitle() + " / lane " + juce::String(clip->track + 1));
        });
    };
    editingCut.onClick = [this] { performTimelineAction(TimelineAction::cut, engine.position, engine.selectedTrack); };
    editingCopy.onClick = [this] { performTimelineAction(TimelineAction::copy, engine.position, engine.selectedTrack); refresh(); };
    editingPaste.onClick = [this] { performTimelineAction(TimelineAction::paste, engine.position, engine.selectedTrack); };
    editingRipple.onClick = [this] { performTimelineAction(TimelineAction::rippleDelete, engine.position, engine.selectedTrack); };
    editingCloseGap.onClick = [this] { performTimelineAction(TimelineAction::closeGap, engine.position, engine.selectedTrack); };
    editingSelect.setButtonText("Select..."); editingCut.setButtonText("Cut"); editingCopy.setButtonText("Copy");
    editingPaste.setButtonText("Paste"); editingRipple.setButtonText("Ripple delete"); editingCloseGap.setButtonText("Close gap");
    editingRange.setButtonText("Select audio"); editingRangeTime.setButtonText("Exact range...");
    editingAll.setButtonText("All"); editingClear.setButtonText("Clear");
    initialiseAutomationEditor();
    for (auto* control : std::initializer_list<juce::Component*> {&editingRange, &editingRangeTime, &editingAll, &editingClear, &editingRangeSize, &editingSelectionInfo}) editingBody.addAndMakeVisible(control);
    editingRange.setTooltip("Drag across a waveform to highlight audio. Free drag ignores global Snap. Drag a track header to move it. Shift-drag also selects audio.");
    editingRange.onClick = [this] { timeline.rangeDragAnchor.reset(); refreshEditSelection(); };
    for (auto* control : std::initializer_list<juce::Component*> {&editingAudioPage, &editingSettingsPage, &editingAutomationPage, &editingMove, &editingSplit, &editingDelete, &editingMore, &editingHint}) editingBody.addAndMakeVisible(control);
    editingAudioPage.onClick = [this] { showEditingPage(0); };
    editingSettingsPage.onClick = [this] { showEditingPage(1); refresh(); };
    editingAutomationPage.onClick = [this] { showEditingPage(2); };
    editingMove.onClick = [this] { editingRange.setToggleState(false, juce::dontSendNotification); timeline.rangeDragAnchor.reset(); refreshEditSelection(); };
    editingMove.setTooltip("Drag tracks to move them. Ctrl-click selects several tracks; Shift-drag highlights audio. Global Snap controls track movement.");
    editingSplit.onClick = [this] { splitClip(); };
    editingDelete.onClick = [this] { performTimelineAction(TimelineAction::remove, engine.position, engine.selectedTrack); };
    editingDelete.setTooltip("Delete the highlighted audio, leaving a gap. Other audio stays in place. Undo restores the edit.");
    editingMore.onClick = [this] { showEditingMore(); };
    editingHint.setFont(juce::FontOptions(12));
    editingHint.setColour(juce::Label::textColourId, colour::text);
    clipSliders[5].setSliderStyle(juce::Slider::LinearHorizontal);
    clipSliders[5].setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 22);
    editingRangeTime.onClick = [this] { editRangeTime(); };
    for (auto* control : {&editingRepeat, &editingCrop, &editingReplace}) editingBody.addAndMakeVisible(control);
    editingReplace.onClick = [this] { performTimelineAction(TimelineAction::replaceSelection, engine.position, engine.selectedTrack); };
    editingReplace.setTooltip("Preview replacing selected destination audio with the clipboard. Copies must fit the selected time span and lanes; shorter copies leave a gap. One Undo restores the mix.");
    editingRepeat.onClick = [this] { performTimelineAction(TimelineAction::repeatSelection, engine.position, engine.selectedTrack); };
    editingCrop.onClick = [this] { performTimelineAction(TimelineAction::cropSelection, engine.position, engine.selectedTrack); };
    editingRepeat.setTooltip("Preview and add copies after the selected range or segments. Empty destination required; clipboard stays unchanged. One Undo restores the mix.");
    editingCrop.setTooltip("Keep only the selected audio range. Other audio stays in place; Undo restores the mix.");
    editingRangeTime.setTooltip("Select an exact audio interval using project start/end seconds. Selection applies only to the selected segments, without seeking, editing audio or adding Undo.");
    editingAll.onClick = [this] { selectAllSegments(); };
    editingAll.setTooltip("Select every placed segment as a group without changing the mix or playhead. Copy/Cut/Delete affect the selected scope. Ctrl+A does the same on the timeline.");
    editingClear.onClick = [this] { clearEditSelection(); refresh(); };
    editingClear.setTooltip("Clear the transient audio range or segment group without changing audio, transport or Undo. Escape also clears selection; during a range drag it restores the preceding selection.");
    editingBody.addAndMakeVisible(editingUnlink);
    editingUnlink.setTooltip("Let selected segments move independently when dragged. Undo restores their links.");
    editingUnlink.onClick = [this] { performTimelineAction(TimelineAction::unlinkSelection, engine.position, engine.selectedTrack); };
    editingRangeSize.addItem("Free drag", 1);
    editingRangeSize.addItem("Snap drag to beats", 7);
    editingRangeSize.addSeparator();
    editingRangeSize.addItemList({"Select 1 beat", "Select 1 bar", "Select 8 bars", "Select 16 bars", "Select 32 bars"}, 2);
    editingRangeSize.setSelectedId(1, juce::dontSendNotification);
    editingRangeSize.setTooltip("Free drag selects any length without snapping. Beat snap affects only selection. Musical presets select a fixed length with one click.");
    editingRangeSize.onChange = [this] { timeline.rangeDragAnchor.reset(); refreshEditSelection(); };
    editingSelectionInfo.setFont(juce::FontOptions(11));
    editingSelectionInfo.setAccessible(true);
}
void Workstation::showEditingPage(int page)
{
    editingPage = juce::jlimit(0, 2, page);
    timeline.rangeDragAnchor.reset();
    editingRange.setToggleState(editingPage == 0, juce::dontSendNotification);
    if (editingPage == 0) { selectedAutomationTarget = -1; timeline.selectAutomationPoint({}, -1); }
    if (editingPage == 1) clearEditSelection(); // Settings explicitly address the active track piece.
    editingViewport.setViewPosition(0, 0);
    layoutEditing(); refreshEditSelection(); timeline.repaint();
}
void Workstation::showEditingMore()
{
    juce::PopupMenu menu;
    menu.addSectionHeader("Selection");
    menu.addItem(1, "Choose track...", editingSelect.isEnabled());
    menu.addItem(2, "Select all tracks", editingAll.isEnabled());
    menu.addSectionHeader("Selected audio");
    menu.addItem(3, "Repeat...", editingRepeat.isEnabled());
    menu.addItem(4, "Keep only selected range", editingCrop.isEnabled());
    menu.addItem(5, "Replace with clipboard...", editingReplace.isEnabled());
    menu.addItem(6, "Unlink tracks for independent movement", editingUnlink.isEnabled());
    menu.addSectionHeader("Close time across ALL lanes");
    menu.addItem(7, "Delete track and close its time", editingRipple.isEnabled());
    menu.addItem(8, "Close empty gap before track", editingCloseGap.isEnabled());
    // Disabled commands and selection can change while a native menu is open.
    const auto selection = editSelection; const auto id = selectedClip;
    const auto before = juce::JSON::toString(projectToJson(engine.project));
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&editingMore),
        [safe = juce::Component::SafePointer<Workstation>(this), selection, id, before](int result)
    {
        if (!safe || !result || safe->busy || safe->engine.exporting) return;
        if (safe->selectedClip != id || safe->editSelection.clips != selection.clips || safe->editSelection.range != selection.range
            || before != juce::JSON::toString(projectToJson(safe->engine.project)))
        { safe->setStatus("Selection changed. Open More edits again."); return; }
        juce::TextButton* commands[] {&safe->editingSelect, &safe->editingAll, &safe->editingRepeat, &safe->editingCrop,
            &safe->editingReplace, &safe->editingUnlink, &safe->editingRipple, &safe->editingCloseGap};
        if (result > 0 && result <= 8 && commands[result - 1]->isEnabled()) commands[result - 1]->onClick();
    });
}
void Workstation::layoutEditing()
{
    if (editingViewport.getViewedComponent() == nullptr) return;
    const int requiredHeight = editingPage == 2 ? 168 : 142;
    const int bar = editingViewport.getScrollBarThickness();
    const int width = std::max(560, editingViewport.getWidth() - (editingViewport.getHeight() < requiredHeight ? bar : 0));
    editingBody.setSize(width, std::max(requiredHeight, editingViewport.getHeight() - (width > editingViewport.getWidth() ? bar : 0)));
    // Only controls for the current task are exposed. Hidden legacy buttons back the More edits menu.
    for (auto* child : editingBody.getChildren()) child->setVisible(false);
    auto put = [](juce::Component& c, juce::Rectangle<int> r) { c.setVisible(true); c.setBounds(r); };
    auto buttons = [&](juce::Rectangle<int> row, std::initializer_list<juce::Component*> controls)
    {
        const int cell = row.getWidth() / (int) controls.size();
        for (auto* c : controls) put(*c, row.removeFromLeft(cell).reduced(2, 1));
    };
    auto area = editingBody.getLocalBounds().reduced(8, 4);
    auto row = area.removeFromTop(28);
    put(editingAudioPage, row.removeFromLeft(110).reduced(1));
    put(editingSettingsPage, row.removeFromLeft(130).reduced(1));
    put(editingAutomationPage, row.removeFromLeft(120).reduced(1));
    editingAudioPage.setToggleState(editingPage == 0, juce::dontSendNotification);
    editingSettingsPage.setToggleState(editingPage == 1, juce::dontSendNotification);
    editingAutomationPage.setToggleState(editingPage == 2, juce::dontSendNotification);
    if (editingPage == 0)
    {
        put(editingMore, row.removeFromRight(104).reduced(1));
        put(editingSelectionInfo, area.removeFromTop(22));
        row = area.removeFromTop(30);
        put(editingMove, row.removeFromLeft(100).reduced(2, 1));
        put(editingRange, row.removeFromLeft(115).reduced(2, 1));
        put(editingRangeSize, row.removeFromLeft(std::min(170, row.getWidth() - 156)).reduced(2, 1));
        put(editingClear, row.removeFromRight(56).reduced(2, 1));
        put(editingRangeTime, row.reduced(2, 1));
        row = area.removeFromTop(32);
        put(editingSplit, row.removeFromLeft(135).reduced(2, 1));
        buttons(row, {&editingCut, &editingCopy, &editingPaste, &editingDelete});
        put(editingHint, area.removeFromTop(22));
    }
    else if (editingPage == 1)
    {
        put(reverseButton, row.removeFromLeft(90).reduced(2, 1));
        put(loopButton, row.removeFromLeft(90).reduced(2, 1));
        put(clipDetail, area.removeFromTop(22));
        auto fields = area.removeFromTop(88);
        const int cell = fields.getWidth() / 3;
        for (int i = 0; i < 6; ++i)
        {
            auto field = juce::Rectangle<int>(fields.getX() + (i % 3) * cell, fields.getY() + (i / 3) * 44, cell - 8, 44);
            put(clipLabels[(size_t) i], field.removeFromTop(16)); put(clipSliders[(size_t) i], field);
        }
    }
    else
    {
        row = area.removeFromTop(22); put(automationHeading, row.removeFromLeft(110)); put(automationState, row);
        row = area.removeFromTop(28); put(automationOwner, row.removeFromLeft(row.getWidth() / 2).withTrimmedRight(4)); put(automationParameter, row);
        row = area.removeFromTop(28); put(automationNode, row.removeFromRight(110));
        buttons(row, {&automationAdd, &automationPrevious, &automationNext, &automationEdit, &automationDelete});
        buttons(area.removeFromTop(28), {&automationRead, &automationMode, &automationArm, &automationPairedFilter});
        put(editingHint, area.removeFromTop(22));
    }
}
void Workstation::trackEditingWorkspace()
{
    const auto bounds = editingWindow ? editingWindow->getBounds() : layout.editingWindowBounds;
    if (layout.toolPage == toolPage && layout.editingDetached == (editingWindow != nullptr) && layout.editingWindowBounds == bounds) return;
    layout.toolPage = toolPage; layout.editingDetached = editingWindow != nullptr; layout.editingWindowBounds = bounds;
    workspaceDirty = true; workspaceChangedAt = juce::Time::getMillisecondCounterHiRes();
}
void Workstation::restoreEditingWorkspace()
{
    consoleCollapsed = false;
    const int savedPage = layout.toolPage;
    const bool detached = layout.editingDetached;
    toolPage = savedPage;
    if (detached) detachToolPanel(2, true);
    // A detached tab focuses its window; keep a usable docked page underneath.
    if (!detached || savedPage != 2) toolPage = savedPage;
    trackEditingWorkspace(); resized();
}
void Workstation::layoutToolPanels()
{
    auto area = layout.performance;
    auto tabs = area.removeFromTop(32);
    const std::array<juce::TextButton*, 3> buttons {&controllerTab, &transitionTab, &editingTab};
    for (const int page : layout.consoleOrder) buttons[(size_t) page]->setBounds(tabs.removeFromLeft(108).withTrimmedRight(4));
    editingTab.setToggleState(!consoleCollapsed && toolPage == 2, juce::dontSendNotification);
    controllerTab.setToggleState(!consoleCollapsed && toolPage == 0, juce::dontSendNotification);
    transitionTab.setToggleState(!consoleCollapsed && toolPage == 1, juce::dontSendNotification);
    editingPanel.setVisible(editingWindow || (!consoleCollapsed && toolPage == 2));
    if (!editingWindow) editingPanel.setBounds(area);
    editingPanel.dock.setDetached(editingWindow != nullptr);
    performancePanel.setVisible(performanceWindow || (!consoleCollapsed && toolPage == 0));
    transitionPanel.setVisible(transitionWindow || (!consoleCollapsed && toolPage == 1));
    if (!performanceWindow) performancePanel.setBounds(area);
    if (!transitionWindow) transitionPanel.setBounds(area);
    performancePanel.dock.setDetached(performanceWindow != nullptr);
    transitionPanel.dock.setDetached(transitionWindow != nullptr);
    layoutPerformance(); layoutTransitionUI(); layoutEditing();
}
void Workstation::toggleConsole(int page)
{
    if (!consoleCollapsed && toolPage == page) { consoleCollapsed = true; layout.timeline = {}; resized(); repaint(); }
    else showToolPage(page);
}
void Workstation::showToolPage(int page)
{
    page = juce::jlimit(0, 2, page);
    auto* window = page == 0 ? performanceWindow.get() : page == 1 ? transitionWindow.get() : editingWindow.get();
    if (window) { window->setMinimised(false); window->toFront(true); return; }
    if (consoleCollapsed) layout.timeline = {};
    consoleCollapsed = false; toolPage = page; if (page == 2) showEditingPage(editingPage); trackEditingWorkspace(); resized(); repaint();
}
void Workstation::detachToolPanel(int page, bool detached)
{
    if (page < 0 || page > 2) return;
    auto& ownedWindow = page == 0 ? performanceWindow : page == 1 ? transitionWindow : editingWindow;
    auto& panel = page == 0 ? performancePanel : page == 1 ? transitionPanel : editingPanel;
    if (page == 2) panel.setEnabled(!busy);
    if (detached == (ownedWindow != nullptr)) return;
    consoleCollapsed = false;
    if (detached)
    {
        auto window = std::make_unique<ToolPanelWindow>(panel, page == 0 ? "Controller" : page == 1 ? "Transitions" : "Editing");
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
        auto bounds = work.withSizeKeepingCentre(std::min(1440, work.getWidth()), std::min(520, work.getHeight()));
        if (page == 2 && !layout.editingWindowBounds.isEmpty())
        {
            const auto* display = displays.getDisplayForRect(layout.editingWindowBounds);
            const auto available = display ? display->userBounds.getLargestIntegerWithin() : work;
            bounds = layout.editingWindowBounds.withSize(std::min(layout.editingWindowBounds.getWidth(), available.getWidth()), std::min(layout.editingWindowBounds.getHeight(), available.getHeight())).constrainedWithin(available);
        }
        window->setBounds(bounds);
        ownedWindow = std::move(window); ownedWindow->setVisible(true);
        AlloyLookAndFeel::styleNativeTitleBar(*ownedWindow);
        if (toolPage == page)
        {
            toolPage = 0;
            for (int other : {0, 1, 2})
                if ((other == 0 ? !performanceWindow : other == 1 ? !transitionWindow : !editingWindow)) { toolPage = other; break; }
        }
    }
    else
    {
        trackEditingWorkspace();
        ownedWindow->clearContentComponent(); ownedWindow->setLookAndFeel(nullptr); ownedWindow.reset();
        addAndMakeVisible(panel); toolPage = page;
    }
    trackEditingWorkspace(); resized(); repaint();
}
}
