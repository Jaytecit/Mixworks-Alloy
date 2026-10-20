#include "Workstation.h"
#include "AppWindow.h"

namespace mixworks
{
juce::StringArray Workstation::getMenuBarNames() { return {"File", "Edit", "Mix", "Beatmatch", "View", "Tools"}; }
juce::PopupMenu Workstation::getMenuForIndex(int index, const juce::String&)
{
    juce::PopupMenu menu;
    switch (index)
    {
        case 0: menu.addItem(1, "New mix"); menu.addItem(2, "Open mix..."); menu.addItem(3, "Save mix..."); menu.addItem(71, "Recover unsaved mix..."); menu.addSeparator(); menu.addItem(4, "Add files to library..."); menu.addItem(29, "Import playlist..."); menu.addSeparator(); menu.addItem(5, "Export mix as WAV..."); menu.addItem(30, "Export source playlist..."); menu.addItem(31, "Export track list..."); menu.addSeparator(); menu.addItem(45, "Exit Mixworks (Alt+F4)"); break;
        case 1: menu.addItem(6, "Undo", !undoHistory.empty()); menu.addItem(7, "Redo", !redoHistory.empty()); menu.addSeparator(); menu.addItem(8, "Split at playhead", currentClip() != nullptr); menu.addItem(9, "Duplicate clip", currentClip() != nullptr); menu.addItem(10, "Remove clip", currentClip() != nullptr); menu.addItem(11, "Reverse", currentClip() != nullptr, currentClip() && currentClip()->reversed); menu.addItem(12, "Loop section...", currentClip() != nullptr); break;
        case 2: menu.addSubMenu("Automation", automationMenu(engine.selectedTrack.load())); menu.addSeparator(); menu.addItem(73, "Effect ending...", !busy); menu.addSeparator(); menu.addItem(14, "Tempo marker at playhead... (P)"); menu.addItem(15, "Snap to beat", true, engine.project.snap); menu.addSeparator(); menu.addItem(32, "Label marker at playhead... (L)"); menu.addItem(33, "Edit selected label...", timeline.selectedLabel.isNotEmpty()); menu.addItem(34, "Remove selected label", timeline.selectedLabel.isNotEmpty()); menu.addItem(35, "Previous label ([)", !engine.project.labels.empty()); menu.addItem(36, "Next label (])", !engine.project.labels.empty()); menu.addSeparator(); menu.addItem(16, "Play / pause"); menu.addItem(17, "Stop"); menu.addItem(18, "Return to start"); break;
        case 3:
        {
            menu.addItem(13, "Beatmatch / rebuild transitions", !engine.project.clips.empty());
            menu.addItem(25, "Track BPM / key..."); menu.addSeparator();
            juce::PopupMenu analysis; const bool available = !busy && !engine.playing && timelineLoads.empty() && pendingTempo <= 0 && !tempoPreparationPending && !engine.exporting;
            analysis.addItem(60, "Selected tracks", available && !analysisTargets(0).empty());
            analysis.addItem(61, "Tracks needing analysis", available && !analysisTargets(1).empty());
            analysis.addItem(62, "All library tracks", available && !engine.library.empty());
            analysis.addSeparator(); analysis.addItem(63, "Select tracks needing analysis", !engine.library.empty());
            menu.addSubMenu("Re-analyse", analysis); menu.addSeparator(); menu.addSectionHeader("Transition length");
            for (int i = 0; i < transitionLength.getNumItems(); ++i) menu.addItem(40 + i, transitionLength.getItemText(i), true, transitionIndex == i);
            break;
        }
        case 4:
            menu.addItem(85, "Suggested energy sections", true, showEnergySections);
            menu.addItem(72, "Master output / Spectrum...", !busy); menu.addSeparator();
            menu.addItem(81, "Forge sparks", true, layout.forgeEffects);
            {
                juce::PopupMenu motion;
                motion.addItem(82, "Follow Windows", true, layout.forgeMotion == 0);
                motion.addItem(83, "Animated", true, layout.forgeMotion == 1);
                motion.addItem(84, "Steady", true, layout.forgeMotion == 2);
                menu.addSubMenu("Forge motion", motion);
            }
            menu.addItem(70, performanceWindow ? "Dock live desk" : "Live desk on second monitor", true, performanceWindow != nullptr);
            menu.addItem(74, "Transitions panel", true, toolPage == 1);
            menu.addItem(75, transitionWindow ? "Attach transitions" : "Detach transitions", true, transitionWindow != nullptr);
            menu.addItem(76, "Controller panel", true, toolPage == 0);
            menu.addSeparator(); menu.addItem(77, "Attract lighting (Ctrl+Shift+A)", true, attractState.enabled);
            {
                juce::PopupMenu motion;
                motion.addItem(78, "Follow Windows", true, attractState.motion == AttractMotion::system);
                motion.addItem(79, "Animated", true, attractState.motion == AttractMotion::animated);
                motion.addItem(80, "Steady", true, attractState.motion == AttractMotion::steady);
                menu.addSubMenu("Attract motion", motion);
            }
            menu.addSeparator();
            menu.addItem(19, "Clip properties", true, inspectorPage == 0); menu.addItem(20, "Effects / VST3", true, inspectorPage == 1); menu.addItem(21, "MIDI keyboard", true, inspectorPage == 2);
            menu.addSeparator(); menu.addItem(22, "Fit whole mix"); menu.addItem(23, "Zoom in"); menu.addItem(24, "Zoom out");
            if (const auto* window = findParentComponentOfClass<AppWindow>()) { menu.addSeparator(); menu.addSubMenu("Display", window->displayMenu()); }
            break;
        case 5: menu.addItem(26, "Audio / MIDI settings..."); menu.addSeparator(); menu.addItem(27, "Add VST3 effect / instrument..."); menu.addItem(28, "Assign MIDI plugin controls..."); menu.addSeparator(); menu.addItem(37, "Scan Music folder now", !engine.playing); menu.addItem(38, "Automatically scan Music folder", true, musicScanEnabled); menu.addItem(39, "Stop current Music scan", musicScan != nullptr || musicScanJobPending || musicScanManual); break;
        default: break;
    }
    if (index == 1 || index == 2) addBarMenus(menu, index == 1);
    return menu;
}
void Workstation::menuItemSelected(int id, int)
{
    if (id == 85) { showEnergySections = !showEnergySections; timeline.repaint(); return; }
    if (id >= (int) TimelineAction::curveRead && id <= (int) TimelineAction::armAutomation)
    { performTimelineAction((TimelineAction) id, engine.position.load(), engine.selectedTrack.load()); return; }
    if (id == 45) { if (auto* app = juce::JUCEApplication::getInstance()) app->systemRequestedQuit(); return; }
    if (auto* window = findParentComponentOfClass<AppWindow>()) if (window->performDisplayAction(id)) return;
    if (id == 74) { showToolPage(1); return; }
    if (id == 75) { detachToolPanel(1, !transitionWindow); return; }
    if (id == 76) { showToolPage(0); return; }
    if (id == 70) { detachToolPanel(0, !performanceWindow); return; }
    if (id == 77) { setAttractMode(!attractState.enabled); return; }
    if (id == 81)
    {
        layout.forgeEffects = !layout.forgeEffects;
        workspaceDirty = true; workspaceChangedAt = juce::Time::getMillisecondCounterHiRes();
        if (!layout.forgeEffects) peakDisplay.sparkSeconds = {};
        if (performance) performance->setForgeEffects(forgeMotionAllowed());
        timeline.repaint(); repaint(); return;
    }
    if (id >= 82 && id <= 84)
    {
        layout.forgeMotion = id - 82;
        workspaceDirty = true; workspaceChangedAt = juce::Time::getMillisecondCounterHiRes();
        if (!forgeMotionAllowed()) peakDisplay.sparkSeconds = {};
        if (performance) performance->setForgeEffects(forgeMotionAllowed());
        timeline.repaint(); repaint(); return;
    }
    if (id >= 78 && id <= 80)
    {
        attractState.motion = static_cast<AttractMotion>(id - 78);
        attractLights.repaint(); performancePanel.refreshAttract(); transitionPanel.refreshAttract(); return;
    }
    if (busy) return;
    if (performBarMenuAction(id)) return;
    if (id >= 40 && id < 40 + transitionLength.getNumItems()) { transitionIndex = id - 40; transitionLength.setSelectedId(transitionIndex + 1, juce::dontSendNotification); return; }
    switch (id)
    {
        case 1: newButton.triggerClick(); break; case 2: openButton.triggerClick(); break; case 3: chooseSave(); break; case 4: chooseImport(); break; case 5: chooseExport(); break;
        case 6: undo(false); break; case 7: undo(true); break; case 8: splitClip(); break; case 9: duplicateClip(); break; case 10: removeClip(); break;
        case 11: reverseButton.triggerClick(); break; case 12: configureLoop(); break; case 13: syncMix(true); break; case 14: editTempoPoint(-1, engine.position); break;
        case 15: snapButton.triggerClick(); break; case 16: playButton.triggerClick(); break; case 17: stopButton.triggerClick(); break; case 18: homeButton.triggerClick(); break;
        case 19: showInspectorPage(0); break; case 20: showInspectorPage(1); break; case 21: showInspectorPage(2); break; case 22: { const juce::ScopedValueSetter<bool> viewGuard(programmaticTimelineView, true); timeline.zoomAt(std::max(0.1, (viewport.getWidth() - 24.0) / std::max(30.0, engine.project.audibleEnd() + 2)) / timeline.pixelsPerSecond, viewport.getViewPositionX() + viewport.getViewWidth() * 0.5); viewport.setViewPosition(0, viewport.getViewPositionY()); followPlayheadIntoView(); break; }
        case 23: timeline.zoomAt(1.5, viewport.getViewPositionX() + viewport.getViewWidth() * 0.5); break; case 24: timeline.zoomAt(1 / 1.5, viewport.getViewPositionX() + viewport.getViewWidth() * 0.5); break; case 25: editAnalysis(); break; case 26: settings(); break;
        case 27: showInspectorPage(1); choosePlugin(); break; case 28: showInspectorPage(1); configurePluginControls(); break;
        case 29: choosePlaylistImport(); break; case 30: choosePlaylistExport(false); break; case 31: choosePlaylistExport(true); break;
        case 32: editLabelMarker({}, engine.position); break; case 33: editLabelMarker(timeline.selectedLabel, engine.position); break;
        case 34: removeLabelMarker(); break; case 35: navigateLabel(false); break; case 36: navigateLabel(true); break;
        case 37: stopMusicScan(); musicAttempts.clear(); musicScanManual = true; nextMusicScan = 0; setStatus("Music scan requested: " + musicFolder.getFullPathName()); break;
        case 38: toggleMusicScan(); break; case 39: stopMusicScan(); setStatus("Music scan stopped; it can resume on the next automatic check."); break;
        case 60: reanalyseTracks(0); break; case 61: reanalyseTracks(1); break; case 62: reanalyseTracks(2); break;
        case 63: selectTracksNeedingAnalysis(); break;
        case 71: offerRecovery(); break;
        case 72: showMastering(); break;
        case 73: configureEffectTail(); break;
        default: break;
    }
}
void Workstation::toggleWindowMode()
{
    if (auto* window = findParentComponentOfClass<AppWindow>()) window->toggleFullscreen();
}
void Workstation::showInspectorPage(int page)
{
    inspectorPage = juce::jlimit(0, 2, page);
    clipTab.setToggleState(page == 0, juce::dontSendNotification); effectsTab.setToggleState(page == 1, juce::dontSendNotification); midiTab.setToggleState(page == 2, juce::dontSendNotification);
    for (auto* c : std::initializer_list<juce::Component*> {&inspectorLabel, &clipDetail, &reverseButton, &loopButton}) c->setVisible(page == 0);
    for (auto& c : clipSliders) c.setVisible(page == 0); for (auto& c : clipLabels) c.setVisible(page == 0);
    for (auto* c : std::initializer_list<juce::Component*> {&pluginLabel, &pluginList.list, &addPluginButton, &editPluginButton, &bypassPluginButton, &removePluginButton, &pluginControlsButton}) c->setVisible(page == 1);
    piano.setVisible(page == 2); midiLabel.setVisible(page == 2); sidebarHelp.setVisible(true);
    sidebarHelp.setText(page == 0 ? "Select a playlist row or waveform to edit that clip. Volume, pan and LPF stay beside each lane; EQ is on the live desk."
        : page == 1 ? "Effects follow the selected audio track when it moves. With no audio track selected, inserts process the mixer lane for live instruments. Double-click an insert to open its editor."
        : "Play a VST3 instrument on the selected lane with this keyboard or the FLkey. Use Audio / MIDI for device and controller setup.", juce::dontSendNotification);
    resized(); repaint();
}
void Workstation::resizeLane(int lane, int height)
{
    layout.resizeLane(lane, height);
    workspaceDirty = true; workspaceChangedAt = juce::Time::getMillisecondCounterHiRes();
    timeline.updateSize(); layoutTrackHeaders(); timelineRuler.setSize(viewport.getViewWidth(), Timeline::rulerHeight); repaint();
}
void Workstation::layoutTrackHeaders()
{
    for (int i = 0; i < trackCount; ++i) strips[(size_t) i]->setBounds(0, timeline.laneTop(i) - viewport.getViewPositionY(), trackHeaders.getWidth(), timeline.laneSize(i));
    tempoStrip.setBounds(layout.tempo.withWidth(viewport.getViewWidth())); tempoStrip.repaint();
    timelineRuler.setBounds(layout.ruler.withWidth(viewport.getViewWidth()));
}
void Workstation::showBrowserPage(int page)
{
    browserPage = juce::jlimit(0, 1, page);
    libraryTab.setToggleState(browserPage == 0, juce::dontSendNotification); playlistTab.setToggleState(browserPage == 1, juce::dontSendNotification);
    for (auto* c : std::initializer_list<juce::Component*> {&libraryTable.table, &search, &keyDisplay, &trackInfoButton}) c->setVisible(browserPage == 0);
    playlistTable.table.setVisible(browserPage == 1);
    resized(); repaint();
}
void Workstation::resizeSidePanel(bool left, int width)
{
    layout.resizePanel(left, width); workspaceDirty = true; workspaceChangedAt = juce::Time::getMillisecondCounterHiRes(); resized(); repaint();
}
void Workstation::resizeTimeline(int height)
{
    layout.resizeTimeline(height);
    workspaceDirty = true; workspaceChangedAt = juce::Time::getMillisecondCounterHiRes(); resized(); repaint();
}
double Workstation::displayedTempo() const
{
    const auto& points = engine.project.tempo.points;
    const int selected = tempoStrip.selectedTempoPoint;
    if (!engine.playing && selected >= 0 && selected < (int) points.size()) return points[(size_t) selected].bpm;
    return engine.project.followTrackTempo ? engine.project.tempo.bpmAt(engine.position) : engine.project.bpm;
}
void Workstation::saveWorkspacePreferences()
{
    if (!workspaceDirty || workspacePreference == juce::File()) return;
    const auto result = writeWorkspacePreferences(workspacePreference, layout); workspaceDirty = false;
    if (result.failed()) { status = result.getErrorMessage(); juce::Logger::writeToLog(status); }
}
void Workstation::paint(juce::Graphics& g)
{
    g.fillAll(colour::background);
    g.setColour(colour::panel); g.fillRect(layout.library); g.fillRect(layout.master); g.fillRect(layout.detail);
    const auto tempoCaption = layout.tempo.withX(layout.headers.getX()).withWidth(layout.headers.getWidth());
    g.fillRect(tempoCaption); g.setColour(colour::muted); g.setFont(juce::FontOptions(10));
    g.drawText("TEMPO", tempoCaption.reduced(8, 0), juce::Justification::centredLeft);

    g.setColour(colour::border); g.drawHorizontalLine(69, 0, (float) getWidth()); g.drawHorizontalLine(layout.status.getY() - 3, 0, (float) getWidth());
    auto brand = layout.brand; AlloyLookAndFeel::drawEmblem(g, brand.removeFromLeft(27).toFloat().reduced(2)); brand.removeFromLeft(6);
    g.setColour(colour::text); g.setFont(juce::FontOptions(15).withStyle("Bold")); g.drawText("MIXWORKS", brand.removeFromLeft(91), juce::Justification::centredLeft);
    g.setColour(colour::orange); g.setFont(juce::FontOptions(15)); g.drawText("ALLOY", brand, juce::Justification::centredLeft);
    g.setColour(colour::muted); g.setFont(juce::FontOptions(11));
    g.drawText(juce::String(attractState.enabled ? "ATTRACT LIGHTS / Esc exits | " : "") + (busy ? "WORKING / " : "") + status, layout.status.withTrimmedRight(350), juce::Justification::centredLeft);
    g.setColour(dirty ? colour::orange : colour::muted); g.drawText(dirty ? "UNSAVED" : "ALLOY", layout.status.withLeft(layout.status.getRight() - 100), juce::Justification::centredRight);
    auto meter = layout.master.withTrimmedTop(70).withHeight(5).reduced(12, 0);
    g.setColour(colour::background); g.fillRect(meter);
    const float peak = engine.masterPeak.load();
    AlloyLookAndFeel::drawMoltenMetal(g, meter.withWidth((int) (meter.getWidth() * juce::jlimit(0.0f, 1.0f, peak))).toFloat(), false, juce::jlimit(0.f, 1.f, peak));
    g.setColour(colour::muted); g.setFont(juce::FontOptions(10));
    g.drawText(peak >= 1 ? "OUTPUT CLIPPING / lower master" : "MASTER OUTPUT / stereo", layout.master.withTrimmedTop(80).reduced(12, 0), juce::Justification::left);
    peakDisplay.paint(g, layout.peakPanel, forgeMotionAllowed());
}
void Workstation::resized()
{
    const juce::ScopedValueSetter<bool> viewGuard(programmaticTimelineView, true);
    const auto laneShares = layout.laneScale;
    layout.update(getWidth(), getHeight(), !performanceWindow || !transitionWindow); menuBar.setBounds(layout.menu);
    if (laneShares != layout.laneScale) { workspaceDirty = true; workspaceChangedAt = juce::Time::getMillisecondCounterHiRes(); }
    controllerConnection.setBounds(layout.status.withRight(layout.status.getRight() - 110).withLeft(layout.status.getRight() - 340));
    layoutToolPanels();
    auto controls = layout.windowControls;
    displayButton.setBounds(controls.removeFromLeft(70)); controls.removeFromLeft(4);
    windowModeButton.setBounds(controls.removeFromLeft(100)); controls.removeFromLeft(4);
    minimiseButton.setBounds(controls.removeFromLeft(28)); controls.removeFromLeft(4); closeAppButton.setBounds(controls);
    const auto* window = findParentComponentOfClass<AppWindow>();
    windowModeButton.setButtonText(window && (window->windowMode() == WindowMode::fullscreen || window->windowMode() == WindowMode::spanning) ? "Windowed" : "Fullscreen");
    importProgress.setBounds((getWidth() - 620) / 2, std::max(100, (getHeight() - 245) / 2), 620, 245);
    auto toolbar = layout.toolbar;
    auto button = [](juce::Component& c, juce::Rectangle<int>& row, int width) { c.setBounds(row.removeFromLeft(width)); row.removeFromLeft(4); };
    // Transition defaults live with their panel; overflow file/edit actions remain in the menus.
    const bool narrowToolbar = getWidth() < 1500;
    const int waveformWidth = narrowToolbar ? 112 : 174;
    const int actionBudget = toolbar.getWidth() - 100 - waveformWidth - 4 - 256 - 8 - (narrowToolbar ? 546 : 640);
    struct Action { juce::Component* control; int width; int priority; };
    const Action actions[] {{&newButton, 52, 1}, {&openButton, 58, 2}, {&saveButton, 58, 0}, {&importButton, 118, 0},
        {&exportButton, 102, 5}, {&undoButton, 54, 3}, {&redoButton, 54, 4}, {&splitButton, 52, 6}, {&duplicateButton, 82, 7}, {&removeButton, 70, 8}};
    int used = 184, priority = 0;
    for (int next = 1; next <= 8; ++next)
    {
        const auto action = std::find_if(std::begin(actions), std::end(actions), [next](const auto& item) { return item.priority == next; });
        if (used + action->width + 4 > actionBudget) break;
        used += action->width + 4; priority = next;
    }
    for (const auto& action : actions)
    {
        action.control->setVisible(action.priority <= priority);
        if (action.control->isVisible()) button(*action.control, toolbar, action.width);
    }
    toolbar.removeFromLeft(8);
    button(beatmatchButton, toolbar, 96); button(waveformMode, toolbar, waveformWidth);
    settingsButton.setBounds(layout.toolbar.withLeft(layout.toolbar.getRight() - 120));
    autoArrangeButton.setBounds(settingsButton.getX() - 128, settingsButton.getY(), 120, settingsButton.getHeight());
    layout.transport = toolbar.withRight(autoArrangeButton.getX() - 8);
    auto transport = layout.transport;
    const bool compact = transport.getWidth() < 700;
    button(homeButton, transport, compact ? 26 : 30); button(playButton, transport, compact ? 40 : 52); button(stopButton, transport, compact ? 38 : 48);
    button(timeLabel, transport, compact ? 80 : 116); button(tempoSlider, transport, compact ? 80 : 106); button(snapButton, transport, compact ? 86 : 106);
    button(zoomSlider, transport, compact ? 68 : 112);
    bottomSplitter.setVisible(true);
    bottomSplitter.setEnabled(true); bottomSplitter.setBounds(layout.timelineDivider);
    bottomSplitter.setRange(layout.minimumTimelineHeight, layout.maximumTimelineHeight, 1);
    bottomSplitter.setValue(layout.timeline.getHeight(), juce::dontSendNotification);
    leftSplitter.setBounds(layout.leftDivider); rightSplitter.setBounds(layout.rightDivider);
    const int panelBudget = getWidth() - 44 - WorkspaceLayout::minimumCentreWidth;
    leftSplitter.setRange(240, std::max(240, panelBudget - layout.master.getWidth()), 1);
    rightSplitter.setRange(240, std::max(240, panelBudget - layout.library.getWidth()), 1);
    leftSplitter.setValue(layout.library.getWidth(), juce::dontSendNotification); rightSplitter.setValue(layout.master.getWidth(), juce::dontSendNotification);
    auto library = layout.library.reduced(8, 6); auto row = library.removeFromTop(30);
    button(libraryTab, row, 84); button(playlistTab, row, 84);
    library.removeFromTop(8); search.setBounds(library.removeFromTop(26)); library.removeFromTop(6);
    row = library.removeFromTop(26); button(trackInfoButton, row, 92); keyDisplay.setBounds(row); library.removeFromTop(8); libraryTable.table.setBounds(library);
    auto playlist = layout.playlist.reduced(8, 6); playlist.removeFromTop(38); playlistTable.table.setBounds(playlist);
    trackHeaders.setBounds(layout.headers); viewport.setBounds(layout.timeline); timeline.updateSize(); layoutTrackHeaders();
    timelineRuler.setBounds(layout.ruler.withWidth(viewport.getViewWidth())); timelineOverview.setBounds(layout.overview);
    const auto laneControls = layout.headers.withY(layout.ruler.getY()).withHeight(Timeline::rulerHeight);
    if (masterPanelButton) masterPanelButton->setBounds(laneControls.withSize(34, 34).translated(4, 5));
    auto laneTools = laneControls.withTrimmedLeft(42).reduced(2, 0);
    laneAmount.setBounds(laneTools.removeFromTop(18));
    followButton.setBounds(laneTools.reduced(0, 1));
    auto master = layout.master.reduced(12, 8); masterLabel.setText("MASTER OUTPUT", juce::dontSendNotification); masterLabel.setBounds(master.removeFromTop(22)); master.removeFromTop(4); auto masterRow = master.removeFromTop(28); monitorMute.setBounds(masterRow.removeFromRight(66)); masterSlider.setBounds(masterRow);
    peakReset.setBounds(layout.peakPanel.reduced(10, 0).withY(layout.peakPanel.getY() + 9).withHeight(23));
    const bool tallMeter = layout.peakPanel.getHeight() >= 240;
    masterSlider.setSliderStyle(tallMeter ? juce::Slider::RotaryHorizontalVerticalDrag : juce::Slider::LinearHorizontal);
    masterSlider.setTextBoxStyle(tallMeter ? juce::Slider::TextBoxBelow : juce::Slider::TextBoxRight, false, 72, 20);
    if (tallMeter) masterSlider.setBounds(layout.peakPanel.withTop(layout.peakPanel.getBottom() - 94).reduced(16, 3));
    auto detail = layout.detail.reduced(12, 10); row = detail.removeFromTop(28); const int tabWidth = row.getWidth() / 3;
    clipTab.setBounds(row.removeFromLeft(tabWidth)); effectsTab.setBounds(row.removeFromLeft(tabWidth)); midiTab.setBounds(row);
    detail.removeFromTop(10);
    const bool shortInspector = detail.getHeight() < 610;
    auto help = detail.removeFromBottom(shortInspector ? 0 : 66); sidebarHelp.setBounds(help);
    auto clip = detail; inspectorLabel.setText("CLIP PROPERTIES", juce::dontSendNotification); inspectorLabel.setBounds(clip.removeFromTop(20)); clip.removeFromTop(4); clipDetail.setBounds(clip.removeFromTop(shortInspector ? 48 : 64)); clip.removeFromTop(8);
    for (int i = 0; i < 5; ++i) { clipLabels[(size_t) i].setBounds(clip.removeFromTop(15)); clipSliders[(size_t) i].setBounds(clip.removeFromTop(24)); clip.removeFromTop(3); }
    clip.removeFromTop(4); row = clip.removeFromTop(27); button(reverseButton, row, 94); loopButton.setBounds(row);
    clip.removeFromTop(8); clipLabels[5].setJustificationType(juce::Justification::centred); clipLabels[5].setBounds(clip.removeFromTop(18));
    const int dialHeight = std::max(60, std::min(126, clip.getHeight())); clipSliders[5].setBounds(clip.withHeight(dialHeight).withSizeKeepingCentre(110, dialHeight));
    auto effects = detail; pluginLabel.setBounds(effects.removeFromTop(22)); effects.removeFromTop(8); addPluginButton.setBounds(effects.removeFromTop(28)); effects.removeFromTop(10);
    pluginList.list.setBounds(effects.removeFromTop(std::max(110, effects.getHeight() - 84))); effects.removeFromTop(8); row = effects.removeFromTop(28);
    button(editPluginButton, row, 62); button(bypassPluginButton, row, 74); removePluginButton.setBounds(row); effects.removeFromTop(6); pluginControlsButton.setBounds(effects.removeFromTop(28));
    auto midi = detail; midiLabel.setBounds(midi.removeFromTop(90)); midi.removeFromTop(16); piano.setBounds(midi.removeFromTop(105));
    attractLights.setBounds(getLocalBounds());
    attractLights.setPanels({layout.brand.toFloat(), layout.library.toFloat(), layout.master.toFloat(), layout.detail.toFloat(), layout.timeline.toFloat(), layout.tempo.toFloat(), layout.headers.toFloat()});
    attractLights.toFront(false);
    followPlayheadIntoView();
}
void Workstation::setFollowPlayhead(bool enabled)
{
    if (layout.followPlayhead != enabled)
    {
        layout.followPlayhead = enabled;
        workspaceDirty = true; workspaceChangedAt = juce::Time::getMillisecondCounterHiRes();
    }
    followButton.setToggleState(enabled, juce::dontSendNotification);
    if (enabled) followPlayheadIntoView();
}
void Workstation::followPlayheadIntoView()
{
    if (!layout.followPlayhead || engine.exporting) return;
    const int width = viewport.getViewWidth();
    if (width <= 0) return;
    const int x = Clip::gridPixel(engine.position.load(), timeline.pixelsPerSecond);
    if (x >= timeline.getWidth()) timeline.updateSize();
    const int left = viewport.getViewPositionX();
    if (x >= left && x < left + width) return;
    const juce::ScopedValueSetter<bool> viewGuard(programmaticTimelineView, true);
    viewport.setViewPosition(std::max(0, x - width / 5), viewport.getViewPositionY());
}
void Workstation::initialiseAttract()
{
    addChildComponent(attractLights);
    piano.getProperties().set("alloyAttractSurface", "keys");
    menuBar.getProperties().set("alloyAttractSurface", "bank");
    performancePanel.setAttractState(attractState); transitionPanel.setAttractState(attractState);
}
void Workstation::setAttractMode(bool active)
{
    if (attractState.enabled == active) return;
    attractState.enabled = active;
    if (active) attractState.startedAtMs = juce::Time::getMillisecondCounterHiRes();
    attractLights.syncMode(); performancePanel.syncAttract(); transitionPanel.syncAttract(); repaint();
}
void Workstation::refreshAttractLights()
{
    if (!attractState.enabled || !attractState.allowsMotion()) return;
    if (isShowing()) attractLights.repaint();
    performancePanel.refreshAttract(); transitionPanel.refreshAttract();
}
}
