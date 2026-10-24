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
    editingPaste.setTooltip("Paste a new segment at the playhead on the selected lane. Snap applies. Source audio is shared; effects get independent ownership. Existing segments keep their positions.");
    editingRipple.setTooltip("Delete this unpaired segment and its time range across all lanes. All later segments, complete linked pairs, session automation, tempo and labels move left together. Overlaps, crossing pairs and protected transitions reject the whole edit. One Undo restores the mix; source audio is kept.");
    editingCloseGap.setTooltip("Close the empty time before this segment, from the latest earlier segment end across all lanes (or mix start). Move all later segments and complete linked pairs together, including session automation, tempo and labels. Overlaps and protected transitions reject the whole edit. One Undo restores it.");
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
    editingRange.setButtonText("Range"); editingRangeTime.setButtonText("Exact...");
    editingAll.setButtonText("All"); editingClear.setButtonText("Clear");
    initialiseAutomationEditor();
    for (auto* control : std::initializer_list<juce::Component*> {&editingRange, &editingRangeTime, &editingAll, &editingClear, &editingRangeSize, &editingSelectionInfo}) editingBody.addAndMakeVisible(control);
    editingRange.setTooltip("Drag on selected segment waveforms to select an audio range without seeking. Ctrl-click selects several segments with Range tool off. Shift-drag still slips audio when Range tool is off; Alt bypasses Snap.");
    editingRange.onClick = [this] { timeline.rangeDragAnchor.reset(); setStatus(editingRange.getToggleState() ? "Range tool on." : "Range tool off."); };
    editingRangeTime.onClick = [this] { editRangeTime(); };
    for (auto* control : {&editingRepeat, &editingCrop, &editingReplace}) editingBody.addAndMakeVisible(control);
    editingReplace.onClick = [this] { performTimelineAction(TimelineAction::replaceSelection, engine.position, engine.selectedTrack); };
    editingReplace.setTooltip("Preview replacing selected destination audio with the clipboard. Copies must fit the selected time span and lanes; shorter copies leave a gap. One Undo restores the mix.");
    editingRepeat.onClick = [this] { performTimelineAction(TimelineAction::repeatSelection, engine.position, engine.selectedTrack); };
    editingCrop.onClick = [this] { performTimelineAction(TimelineAction::cropSelection, engine.position, engine.selectedTrack); };
    editingRepeat.setTooltip("Preview and add copies after the selected range or segments. Empty destination required; clipboard stays unchanged. One Undo restores the mix.");
    editingCrop.setTooltip("Keep only the chosen range on selected segments, leaving other segments and timeline positions unchanged. Partial linked transitions refuse. One Undo restores the mix.");
    editingRangeTime.setTooltip("Select an exact audio interval using project start/end seconds. Selection applies only to the selected segments, without seeking, editing audio or adding Undo.");
    editingAll.onClick = [this] { selectAllSegments(); };
    editingAll.setTooltip("Select every placed segment as a group without changing the mix or playhead. Copy/Cut/Delete affect the selected scope. Ctrl+A does the same on the timeline.");
    editingClear.onClick = [this] { clearEditSelection(); refresh(); };
    editingClear.setTooltip("Clear the transient audio range or segment group without changing audio, transport or Undo. Escape also clears selection; during a range drag it restores the preceding selection.");
    editingBody.addAndMakeVisible(editingUnlink);
    editingUnlink.setTooltip("Unlink selected whole segments so waveform drags move them independently. Keeps audible boundaries, fades and automation. Ctrl-click pieces to move them together; Undo restores links.");
    editingUnlink.onClick = [this] { performTimelineAction(TimelineAction::unlinkSelection, engine.position, engine.selectedTrack); };
    editingRangeSize.addItemList({"Drag range", "1 beat", "1 bar", "8 bars", "16 bars", "32 bars"}, 1);
    editingRangeSize.setSelectedId(1, juce::dontSendNotification);
    editingRangeSize.setTooltip("Range tool: drag a range, or click a waveform for a fixed musical length in constant-tempo 4/4. Exact range uses project seconds.");
    editingSelectionInfo.setFont(juce::FontOptions(11));
    editingSelectionInfo.setAccessible(true);
}
void Workstation::layoutEditing()
{
    if (editingViewport.getViewedComponent() == nullptr) return;
    const int bar = editingViewport.getScrollBarThickness();
    const bool wide = editingViewport.getWidth() >= 1000;
    const int requiredHeight = wide ? 280 : 364;
    const bool vertical = editingViewport.getHeight() < requiredHeight;
    const int width = std::max(600, editingViewport.getWidth() - (vertical ? bar : 0));
    const bool horizontal = width > editingViewport.getWidth();
    editingBody.setSize(width, std::max(requiredHeight, editingViewport.getHeight() - (horizontal ? bar : 0)));
    auto area = editingBody.getLocalBounds().reduced(8, 4);
    auto automation = wide ? area.removeFromLeft(area.getWidth() * 40 / 100).withTrimmedRight(12) : area.removeFromTop(108);
    auto row = automation.removeFromTop(18);
    automationHeading.setBounds(row.removeFromLeft(110)); automationState.setBounds(row);
    auto targets = automation.removeFromTop(wide ? 52 : 26);
    if (wide) { automationOwner.setBounds(targets.removeFromTop(26).reduced(0, 1)); automationParameter.setBounds(targets.reduced(0, 1)); }
    else { automationOwner.setBounds(targets.removeFromLeft(targets.getWidth() / 2).withTrimmedRight(4)); automationParameter.setBounds(targets); }
    auto buttons = [](juce::Rectangle<int> row, std::initializer_list<juce::Component*> controls)
    {
        const int cell = row.getWidth() / (int) controls.size();
        for (auto* c : controls) c->setBounds(row.removeFromLeft(cell).reduced(2, 1));
    };
    row = automation.removeFromTop(26);
    if (wide)
    {
        buttons(row, {&automationAdd, &automationPrevious, &automationNext});
        row = automation.removeFromTop(26); automationNode.setBounds(row.removeFromRight(86));
        buttons(row, {&automationEdit, &automationDelete});
        buttons(automation.removeFromTop(26), {&automationRead, &automationMode});
        buttons(automation.removeFromTop(26), {&automationArm, &automationPairedFilter});
    }
    else
    {
        automationNode.setBounds(row.removeFromRight(110));
        buttons(row, {&automationAdd, &automationPrevious, &automationNext, &automationEdit, &automationDelete});
        buttons(automation.removeFromTop(26), {&automationRead, &automationMode, &automationArm, &automationPairedFilter});
    }
    row = area.removeFromTop(20);
    inspectorLabel.setBounds(row.removeFromLeft(130)); clipDetail.setBounds(row);
    row = area.removeFromTop(28);
    editingSelectionInfo.setBounds(row.removeFromRight(std::max(90, row.getWidth() - 400)));
    buttons(row, {&editingRange, &editingRangeSize, &editingRangeTime, &editingAll, &editingClear});
    buttons(area.removeFromTop(28), {&editingSelect, &editingCut, &editingCopy, &editingPaste, &editingRipple, &editingCloseGap});
    buttons(area.removeFromTop(28), {&editingRepeat, &editingCrop, &editingReplace, &editingUnlink});
    area.removeFromTop(4);
    auto fields = area.removeFromTop(96);
    auto gain = fields.removeFromRight(86); fields.removeFromRight(6);
    clipLabels[5].setBounds(gain.removeFromTop(16));
    clipLabels[5].setJustificationType(juce::Justification::centred);
    clipSliders[5].setBounds(gain);
    const int cell = fields.getWidth() / 3;
    for (int i = 0; i < 5; ++i)
    {
        auto field = juce::Rectangle<int>(fields.getX() + (i % 3) * cell, fields.getY() + (i / 3) * 48, cell - 8, 44);
        clipLabels[(size_t) i].setBounds(field.removeFromTop(16)); clipSliders[(size_t) i].setBounds(field);
    }
    auto actions = area.removeFromTop(28);
    reverseButton.setBounds(actions.removeFromLeft(90).reduced(2, 1));
    loopButton.setBounds(actions.removeFromLeft(90).reduced(2, 1));
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
    consoleCollapsed = false; toolPage = page; trackEditingWorkspace(); resized(); repaint();
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
