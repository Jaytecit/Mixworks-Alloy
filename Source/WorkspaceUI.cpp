#include "Workstation.h"
#include "AppWindow.h"

namespace mixworks
{
namespace
{
// JUCE transports integer IDs; keep each command family explicit and disjoint.
enum class MenuCommand : int
{
    newMix = 1,
    openMix = 2,
    saveMix = 3,
    addFiles = 4,
    exportWav = 5,
    undo = 6,
    redo = 7,
    split = 8,
    duplicate = 9,
    remove = 10,
    reverse = 11,
    loop = 12,
    rebuild = 13,
    tempo = 14,
    snap = 15,
    play = 16,
    stop = 17,
    home = 18,
    editing = 19,
    effects = 20,
    keyboard = 21,
    fit = 22,
    zoomIn = 23,
    zoomOut = 24,
    correctSource = 25,
    settings = 26,
    addPlugin = 27,
    assignPlugin = 28,
    importPlaylist = 29,
    exportPlaylist = 30,
    exportTrackList = 31,
    addLabel = 32,
    editLabel = 33,
    removeLabel = 34,
    previousLabel = 35,
    nextLabel = 36,
    scanMusic = 37,
    autoScan = 38,
    stopScan = 39,
    scanReports = 139,
    quit = 45,
    analyseSelected = 60,
    analyseReview = 61,
    analyseAll = 62,
    selectReview = 63,
    analyseFocused = 64,
    selectAllVisible = 65,
    detachController = 70,
    recover = 71,
    masterOutput = 72,
    effectEnding = 73,
    transitions = 74,
    detachTransitions = 75,
    controller = 76,
    attract = 77,
    attractSystem = 78,
    attractAnimated = 79,
    attractSteady = 80,
    forge = 81,
    forgeSystem = 82,
    forgeAnimated = 83,
    forgeSteady = 84,
    energySections = 85,
    popupHints = 86,
    detachEditing = 87,
    resetClipCutoff = 88,
    saveMixAs = 89,
    rippleDelete = 90, closeGap = 91,
    workspaceLayouts = 92, consoleOrder = 93,
    insertBefore = 94, insertAfter = 95,
    length4 = 3000, length8, length16, length32, length64, lengthSections,
    snapBeat = 3010, snapHalf, snapQuarter
};
constexpr int commandId(MenuCommand command) { return static_cast<int>(command); }
constexpr std::array<MenuCommand, 6> transitionCommands {{MenuCommand::length4, MenuCommand::length8, MenuCommand::length16, MenuCommand::length32, MenuCommand::length64, MenuCommand::lengthSections}};
}

juce::StringArray Workstation::getMenuBarNames() { return {"File", "Edit", "Mix", "Beat grids", "View", "Tools"}; }
juce::PopupMenu Workstation::getMenuForIndex(int index, const juce::String&)
{
    juce::PopupMenu menu;
    switch (index)
    {
        case 0: menu.addSectionHeader("Current: " + (sessionFile == juce::File() ? "Untitled mix" : sessionFile.getFileName()) + (dirty ? " *" : "")); menu.addItem(commandId(MenuCommand::newMix), "New mix"); menu.addItem(commandId(MenuCommand::openMix), "Open mix..."); menu.addItem(commandId(MenuCommand::saveMix), "Save mix (Ctrl+S)"); menu.addItem(commandId(MenuCommand::saveMixAs), "Save mix as... (Ctrl+Shift+S)"); menu.addItem(commandId(MenuCommand::recover), "Recover unsaved mix..."); menu.addSeparator(); menu.addItem(commandId(MenuCommand::addFiles), "Add files to library..."); menu.addItem(commandId(MenuCommand::importPlaylist), "Import playlist..."); menu.addSeparator(); menu.addItem(commandId(MenuCommand::exportWav), "Export mix as WAV..."); menu.addItem(commandId(MenuCommand::exportPlaylist), "Export source playlist..."); menu.addItem(commandId(MenuCommand::exportTrackList), "Export track list..."); menu.addSeparator(); menu.addItem(commandId(MenuCommand::quit), "Exit Mixworks (Alt+F4)"); break;
        case 1: menu.addItem(commandId(MenuCommand::undo), "Undo", !undoHistory.empty()); menu.addItem(commandId(MenuCommand::redo), "Redo", !redoHistory.empty()); menu.addSeparator(); menu.addItem(commandId(MenuCommand::split), "Split at playhead", currentClip() != nullptr); menu.addItem(commandId(MenuCommand::duplicate), "Duplicate segment", currentClip() != nullptr); menu.addItem(commandId(MenuCommand::remove), "Remove segment", currentClip() != nullptr); menu.addItem(commandId(MenuCommand::reverse), "Reverse", currentClip() != nullptr, currentClip() && currentClip()->reversed); menu.addItem(commandId(MenuCommand::loop), "Loop section...", currentClip() != nullptr); menu.addItem(commandId(MenuCommand::resetClipCutoff), "Reset selected segment cutoff", currentClip() && !engine.project.clipTransitionLocked(currentClip()->id)); break;
        case 2: menu.addSubMenu("Automation", automationMenu(engine.selectedTrack.load())); menu.addSeparator(); menu.addItem(commandId(MenuCommand::effectEnding), "Effect ending...", !busy); menu.addSeparator(); menu.addItem(commandId(MenuCommand::tempo), "Tempo marker at playhead... (P)"); menu.addItem(commandId(MenuCommand::snap), "Snap to grid", true, engine.project.snap); menu.addSeparator(); menu.addItem(commandId(MenuCommand::addLabel), "Label marker at playhead... (L)"); menu.addItem(commandId(MenuCommand::editLabel), "Edit selected label...", timeline.selectedLabel.isNotEmpty()); menu.addItem(commandId(MenuCommand::removeLabel), "Remove selected label", timeline.selectedLabel.isNotEmpty()); menu.addItem(commandId(MenuCommand::previousLabel), "Previous label ([)", !engine.project.labels.empty()); menu.addItem(commandId(MenuCommand::nextLabel), "Next label (])", !engine.project.labels.empty()); menu.addSeparator(); menu.addItem(commandId(MenuCommand::play), "Play / pause"); menu.addItem(commandId(MenuCommand::stop), "Stop"); menu.addItem(commandId(MenuCommand::home), "Return to start"); break;
        case 3:
        {
            menu.addItem(commandId(MenuCommand::rebuild), "Beatmatch / rebuild transitions", !engine.project.clips.empty());
            menu.addItem(commandId(MenuCommand::correctSource), "Track BPM / key..."); menu.addSeparator();
            juce::PopupMenu snapping;
            snapping.addItem(commandId(MenuCommand::snapBeat), "Beat", true, engine.project.snapDivision == 1);
            snapping.addItem(commandId(MenuCommand::snapHalf), "Half beat", true, engine.project.snapDivision == 2);
            snapping.addItem(commandId(MenuCommand::snapQuarter), "Quarter beat", true, engine.project.snapDivision == 4);
            menu.addSubMenu("Snap grid", snapping); menu.addSeparator();
            juce::PopupMenu analysis; const bool available = !busy && !engine.playing && timelineLoads.empty() && pendingTempo <= 0 && !tempoPreparationPending && !engine.exporting;
            const auto selected = analysisTargets(0);
            analysis.addItem(commandId(MenuCommand::analyseFocused), "Retry weak BPM / key", available && std::any_of(selected.begin(), selected.end(), [](const auto& m) { return canRetryAnalysis(*m); }));
            analysis.addItem(commandId(MenuCommand::analyseSelected), "Selected tracks", available && !analysisTargets(0).empty());
            analysis.addItem(commandId(MenuCommand::analyseReview), "Tracks needing analysis", available && !analysisTargets(1).empty());
            analysis.addItem(commandId(MenuCommand::analyseAll), "All library songs", available && !engine.library.empty());
            analysis.addSeparator(); analysis.addItem(commandId(MenuCommand::selectReview), "Select tracks needing analysis", !engine.library.empty());
            menu.addSubMenu("Re-analyse", analysis); menu.addSeparator(); menu.addSectionHeader("Transition length");
            for (int i = 0; i < (int) transitionCommands.size(); ++i) menu.addItem(commandId(transitionCommands[(size_t) i]), transitionLength.getItemText(i), true, transitionIndex == i);
            break;
        }
        case 4:
            menu.addItem(commandId(MenuCommand::workspaceLayouts), "Workspace layouts...", !busy && !engine.exporting);
            menu.addItem(commandId(MenuCommand::consoleOrder), "Arrange console tabs...", !busy && !engine.exporting);
            menu.addSeparator();
            {
                juce::PopupMenu preroll;
                const bool ready = !busy && pendingTempo <= 0 && !tempoPreparationPending && !engine.playing && !engine.exporting && !engine.rehearsalActive() && !engine.project.hasPendingPlacement() && !engine.project.hasLockedTransitions();
                preroll.addItem(2491, "Add 2 bars", ready);
                preroll.addItem(2490, "Add 1 bar", ready);
                preroll.addItem(2492, "Add 4 bars", ready);
                preroll.addSeparator(); preroll.addItem(2493, "Remove pre-roll", ready && engine.project.tempo.origin > 0);
                menu.addSubMenu("Pre-roll before bar 1", preroll);
            }
            menu.addItem(commandId(MenuCommand::popupHints), "Popup hints", true, layout.popupHints);
            menu.addItem(commandId(MenuCommand::energySections), "Suggested energy sections", true, showEnergySections);
            menu.addItem(commandId(MenuCommand::masterOutput), "Master output / Spectrum...", !busy); menu.addSeparator();
            menu.addItem(commandId(MenuCommand::forge), "Forge sparks", true, layout.forgeEffects);
            {
                juce::PopupMenu motion;
                motion.addItem(commandId(MenuCommand::forgeSystem), "Follow Windows", true, layout.forgeMotion == 0);
                motion.addItem(commandId(MenuCommand::forgeAnimated), "Animated", true, layout.forgeMotion == 1);
                motion.addItem(commandId(MenuCommand::forgeSteady), "Steady", true, layout.forgeMotion == 2);
                menu.addSubMenu("Forge motion", motion);
            }
            menu.addItem(commandId(MenuCommand::detachController), performanceWindow ? "Dock live desk" : "Live desk on second monitor", true, performanceWindow != nullptr);
            menu.addItem(commandId(MenuCommand::transitions), "Transitions panel", true, toolPage == 1);
            menu.addItem(commandId(MenuCommand::detachTransitions), transitionWindow ? "Attach transitions" : "Detach transitions", true, transitionWindow != nullptr);
            menu.addItem(commandId(MenuCommand::controller), "Controller panel", true, toolPage == 0);
            menu.addSeparator(); menu.addItem(commandId(MenuCommand::attract), "Attract lighting (Ctrl+Shift+A)", true, attractState.enabled);
            {
                juce::PopupMenu motion;
                motion.addItem(commandId(MenuCommand::attractSystem), "Follow Windows", true, attractState.motion == AttractMotion::system);
                motion.addItem(commandId(MenuCommand::attractAnimated), "Animated", true, attractState.motion == AttractMotion::animated);
                motion.addItem(commandId(MenuCommand::attractSteady), "Steady", true, attractState.motion == AttractMotion::steady);
                menu.addSubMenu("Attract motion", motion);
            }
            menu.addSeparator();
            menu.addItem(commandId(MenuCommand::editing), "Editing console", true, toolPage == 2); menu.addItem(commandId(MenuCommand::detachEditing), editingWindow ? "Attach Editing" : "Detach Editing", true, editingWindow != nullptr); menu.addItem(commandId(MenuCommand::effects), "Effects / VST3", true, inspectorPage == 1); menu.addItem(commandId(MenuCommand::keyboard), "MIDI keyboard", true, inspectorPage == 2);
            menu.addSeparator(); menu.addItem(commandId(MenuCommand::fit), "Fit whole mix"); menu.addItem(commandId(MenuCommand::zoomIn), "Zoom in"); menu.addItem(commandId(MenuCommand::zoomOut), "Zoom out");
            if (const auto* window = findParentComponentOfClass<AppWindow>()) { menu.addSeparator(); menu.addSubMenu("Display", window->displayMenu()); }
            break;
        case 5: menu.addItem(commandId(MenuCommand::settings), "Audio / MIDI settings..."); menu.addSeparator(); menu.addItem(commandId(MenuCommand::addPlugin), "Add VST3 effect / instrument..."); menu.addItem(commandId(MenuCommand::assignPlugin), "Assign MIDI plugin controls..."); menu.addSeparator(); menu.addItem(commandId(MenuCommand::scanMusic), "Scan Music folder now", !engine.playing); menu.addItem(commandId(MenuCommand::autoScan), "Automatically scan Music folder", true, musicScanEnabled); menu.addItem(commandId(MenuCommand::stopScan), "Stop current Music scan", musicScan != nullptr || musicScanJobPending || musicScanManual); menu.addItem(commandId(MenuCommand::scanReports), "Music scan reports..."); break;
        default: break;
    }
    if (index == 1)
    {
        menu.addSeparator();
        menu.addItem(commandId(MenuCommand::rippleDelete), "Ripple delete segment / all lanes (Ctrl+Shift+Delete)", currentClip() != nullptr && !busy && !engine.exporting);
        menu.addItem(commandId(MenuCommand::closeGap), "Close gap before segment / all lanes (Ctrl+Shift+G)", currentClip() != nullptr && !busy && !engine.exporting);
        menu.addItem(commandId(MenuCommand::insertBefore), "Insert song before selected segment...", currentClip() && canInsertArrangement() && !engine.library.empty());
        menu.addItem(commandId(MenuCommand::insertAfter), "Insert song after selected segment...", currentClip() && canInsertArrangement() && !engine.library.empty());
    }
    if (index == 1 || index == 2) addBarMenus(menu, index == 1);
    return menu;
}
void Workstation::menuItemSelected(int id, int menuIndex)
{
    if (id == commandId(MenuCommand::insertBefore) || id == commandId(MenuCommand::insertAfter))
    {
        if (menuIndex == 1 && currentClip()) chooseArrangementInsertion(selectedClip, id == commandId(MenuCommand::insertAfter));
        return;
    }
    if (id >= 2490 && id <= 2493) { changePreroll(id == 2493 ? 0 : id == 2490 ? 1 : id == 2491 ? 2 : 4); return; }
    if (id == commandId(MenuCommand::popupHints))
    {
        layout.popupHints = !layout.popupHints; hints.setHintsEnabled(layout.popupHints);
        workspaceDirty = true; workspaceChangedAt = juce::Time::getMillisecondCounterHiRes();
        return;
    }
    if (id == commandId(MenuCommand::energySections)) { showEnergySections = !showEnergySections; timeline.repaint(); return; }
    if (id >= (int) TimelineAction::curveRead && id <= (int) TimelineAction::armAutomation)
    { performTimelineAction((TimelineAction) id, engine.position.load(), engine.selectedTrack.load()); return; }
    if (id == commandId(MenuCommand::quit)) { if (auto* app = juce::JUCEApplication::getInstance()) app->systemRequestedQuit(); return; }
    if (auto* window = findParentComponentOfClass<AppWindow>()) if (window->performDisplayAction(id)) return;
    if (id == commandId(MenuCommand::detachEditing)) { detachToolPanel(2, !editingWindow); return; }
    if (id == commandId(MenuCommand::transitions)) { showToolPage(1); return; }
    if (id == commandId(MenuCommand::detachTransitions)) { detachToolPanel(1, !transitionWindow); return; }
    if (id == commandId(MenuCommand::controller)) { showToolPage(0); return; }
    if (id == commandId(MenuCommand::detachController)) { detachToolPanel(0, !performanceWindow); return; }
    if (id == commandId(MenuCommand::attract)) { setAttractMode(!attractState.enabled); return; }
    if (id == commandId(MenuCommand::forge))
    {
        layout.forgeEffects = !layout.forgeEffects;
        workspaceDirty = true; workspaceChangedAt = juce::Time::getMillisecondCounterHiRes();
        if (!layout.forgeEffects) peakDisplay.sparkSeconds = {};
        if (performance) performance->setForgeEffects(forgeMotionAllowed());
        timeline.repaint(); repaint(); return;
    }
    if (id >= commandId(MenuCommand::forgeSystem) && id <= commandId(MenuCommand::forgeSteady))
    {
        layout.forgeMotion = id - commandId(MenuCommand::forgeSystem);
        workspaceDirty = true; workspaceChangedAt = juce::Time::getMillisecondCounterHiRes();
        if (!forgeMotionAllowed()) peakDisplay.sparkSeconds = {};
        if (performance) performance->setForgeEffects(forgeMotionAllowed());
        timeline.repaint(); repaint(); return;
    }
    if (id >= commandId(MenuCommand::attractSystem) && id <= commandId(MenuCommand::attractSteady))
    {
        attractState.motion = static_cast<AttractMotion>(id - commandId(MenuCommand::attractSystem));
        attractLights.repaint(); performancePanel.refreshAttract(); transitionPanel.refreshAttract(); return;
    }
    if (busy) return;
    if (performBarMenuAction(id)) return;
    const auto length = std::find(transitionCommands.begin(), transitionCommands.end(), static_cast<MenuCommand>(id));
    if (length != transitionCommands.end())
    {
        if (menuIndex != 3) return; // Only the Beatmatch menu owns these defaults.
        transitionIndex = (int) std::distance(transitionCommands.begin(), length);
        transitionLength.setSelectedId(transitionIndex + 1, juce::dontSendNotification); return;
    }
    switch (static_cast<MenuCommand>(id))
    {
        case MenuCommand::workspaceLayouts: if (menuIndex == 4) chooseWorkspaceLayout(); break;
        case MenuCommand::consoleOrder: if (menuIndex == 4) chooseConsoleOrder(); break;
        case MenuCommand::rippleDelete: case MenuCommand::closeGap:
            if (menuIndex == 1) performTimelineAction(id == commandId(MenuCommand::rippleDelete) ? TimelineAction::rippleDelete : TimelineAction::closeGap, engine.position, engine.selectedTrack);
            break;
        case MenuCommand::snapBeat: case MenuCommand::snapHalf: case MenuCommand::snapQuarter:
        {
            if (menuIndex != 3) break;
            const int division = 1 << (id - commandId(MenuCommand::snapBeat));
            if (engine.project.snapDivision == division) break;
            remember(); { const juce::ScopedLock guard(engine.lock); engine.project.snapDivision = division; }
            refresh(); setStatus("Snap grid: " + juce::String(division == 1 ? "beat" : division == 2 ? "half beat" : "quarter beat") + ". Alt bypasses snapping."); break;
        }
        case MenuCommand::newMix: newButton.triggerClick(); break; case MenuCommand::openMix: openButton.triggerClick(); break; case MenuCommand::saveMix: chooseSave({}, true); break; case MenuCommand::saveMixAs: chooseSave({}, false); break; case MenuCommand::addFiles: chooseImport(); break; case MenuCommand::exportWav: chooseExport(); break;
        case MenuCommand::undo: undo(false); break; case MenuCommand::redo: undo(true); break; case MenuCommand::split: splitClip(); break; case MenuCommand::duplicate: duplicateClip(); break; case MenuCommand::remove: removeClip(); break;
        case MenuCommand::resetClipCutoff:
        {
            const auto* clip = currentClip(); if (!clip || engine.project.clipTransitionLocked(clip->id)) break;
            const AutomationAddress address {clip->track, AutomationTarget::cutoff, {}, {}, clip->id};
            engine.finishAutomation(); remember(); const juce::ScopedLock guard(engine.lock);
            engine.project.setManualValue(address, 0); if (auto* curve = engine.project.curve(address)) curve->enabled = false;
            refresh(); setStatus("Selected segment cutoff reset to Off; curve points retained. Lane cutoff is separate."); break;
        }
        case MenuCommand::reverse: reverseButton.triggerClick(); break; case MenuCommand::loop: configureLoop(); break; case MenuCommand::rebuild: syncMix(true); break; case MenuCommand::tempo: editTempoPoint(-1, engine.position); break;
        case MenuCommand::snap: snapButton.triggerClick(); break; case MenuCommand::play: playButton.triggerClick(); break; case MenuCommand::stop: stopButton.triggerClick(); break; case MenuCommand::home: homeButton.triggerClick(); break;
        case MenuCommand::editing: showInspectorPage(0); break; case MenuCommand::effects: showInspectorPage(1); break; case MenuCommand::keyboard: showInspectorPage(2); break; case MenuCommand::fit: { const juce::ScopedValueSetter<bool> viewGuard(programmaticTimelineView, true); timeline.zoomAt(std::max(0.1, (viewport.getWidth() - 24.0) / std::max(30.0, engine.project.audibleEnd() + 2)) / timeline.pixelsPerSecond, viewport.getViewPositionX() + viewport.getViewWidth() * 0.5); viewport.setViewPosition(0, viewport.getViewPositionY()); followPlayheadIntoView(); break; }
        case MenuCommand::zoomIn: timeline.zoomAt(1.5, viewport.getViewPositionX() + viewport.getViewWidth() * 0.5); break; case MenuCommand::zoomOut: timeline.zoomAt(1 / 1.5, viewport.getViewPositionX() + viewport.getViewWidth() * 0.5); break; case MenuCommand::correctSource: editAnalysis(analysisSelectionId()); break; case MenuCommand::settings: settings(); break;
        case MenuCommand::addPlugin: showInspectorPage(1); choosePlugin(); break; case MenuCommand::assignPlugin: showInspectorPage(1); configurePluginControls(); break;
        case MenuCommand::importPlaylist: choosePlaylistImport(); break; case MenuCommand::exportPlaylist: choosePlaylistExport(false); break; case MenuCommand::exportTrackList: choosePlaylistExport(true); break;
        case MenuCommand::addLabel: editLabelMarker({}, engine.position); break; case MenuCommand::editLabel: editLabelMarker(timeline.selectedLabel, engine.position); break;
        case MenuCommand::removeLabel: removeLabelMarker(); break; case MenuCommand::previousLabel: navigateLabel(false); break; case MenuCommand::nextLabel: navigateLabel(true); break;
        case MenuCommand::scanMusic: stopMusicScan(); musicAttempts.clear(); musicScanManual = true; nextMusicScan = 0; setStatus("Music scan requested: " + musicFolder.getFullPathName()); break;
        case MenuCommand::autoScan: toggleMusicScan(); break; case MenuCommand::stopScan: stopMusicScan(); setStatus("Music scan stopped; it can resume on the next automatic check."); break;
        case MenuCommand::scanReports:
            if (musicReport.existsAsFile()) musicReport.startAsProcess();
            else if (musicScanReportsFolder(musicPreference).isDirectory()) musicScanReportsFolder(musicPreference).revealToUser();
            else setStatus("No scan issues have been recorded by this version yet.");
            break;
        case MenuCommand::analyseSelected: reanalyseTracks(0); break; case MenuCommand::analyseReview: reanalyseTracks(1); break; case MenuCommand::analyseAll: reanalyseTracks(2); break;
        case MenuCommand::analyseFocused: reanalyseTracks(0, true); break;
        case MenuCommand::selectAllVisible:
            if (!busy) { auto& table = browserPage == 1 ? playlistTable.table : libraryTable.table; if (table.getNumRows() > 0) table.selectRangeOfRows(0, table.getNumRows() - 1); }
            break;
        case MenuCommand::selectReview: selectTracksNeedingAnalysis(); break;
        case MenuCommand::recover: offerRecovery(); break;
        case MenuCommand::masterOutput: showMastering(); break;
        case MenuCommand::effectEnding: configureEffectTail(); break;
        default: break;
    }
}
void Workstation::toggleWindowMode()
{
    if (auto* window = findParentComponentOfClass<AppWindow>()) window->toggleFullscreen();
}
void Workstation::initialiseShortcuts()
{
    for (auto* c : std::initializer_list<juce::Component*> {&effectsTab, &shortcutsTab, &shortcutSearch, &shortcutHeading, &allShortcuts, &shortcutResults}) addAndMakeVisible(c);
    effectsTab.setComponentID("sidebar-vst"); shortcutsTab.setComponentID("sidebar-shortcuts");
    effectsTab.onClick = [this] { showShortcuts(false); };
    shortcutsTab.onClick = [this] { showShortcuts(true); };
    effectsTab.setTooltip("Show the selected segment or lane's VST inserts.");
    shortcutsTab.setTooltip("Arrangement shortcuts. Search all commands by name or key.");
    shortcutSearch.setComponentID("shortcut-search"); shortcutSearch.setTitle("Find a shortcut");
    shortcutSearch.setTextToShowWhenEmpty("Search all shortcuts...", colour::muted);
    shortcutSearch.getProperties().set("mixworksHint", "Find a command by name, key or context. Search includes all shortcuts. Escape clears search and returns to the timeline.");
    shortcutSearch.onTextChange = [this] { refreshShortcuts(); };
    shortcutSearch.onEscapeKey = [this] { shortcutSearch.clear(); timeline.grabKeyboardFocus(); };
    shortcutSearch.onReturnKey = [this] { shortcutResults.grabKeyboardFocus(); };
    shortcutResults.setComponentID("shortcut-results"); shortcutResults.setTitle("Keyboard shortcut reference");
    shortcutResults.getProperties().set("mixworksHint", "Read or copy shortcut text. Commands act on the focused timeline control; this list does not run them.");
    shortcutResults.setMultiLine(true, true); shortcutResults.setReadOnly(true); shortcutResults.setCaretVisible(false);
    shortcutResults.setScrollbarsShown(true); shortcutResults.setFont(juce::FontOptions(13));
    shortcutResults.setColour(juce::TextEditor::backgroundColourId, colour::panel);
    shortcutResults.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    shortcutResults.setIndents(2, 6);
    shortcutHeading.setFont(juce::FontOptions(12, juce::Font::bold));
    shortcutHeading.setColour(juce::Label::textColourId, colour::muted);
    allShortcuts.setComponentID("shortcut-all"); allShortcuts.setTooltip("Switch between common shortcuts and the full keyboard reference.");
    allShortcuts.setColour(juce::TextButton::buttonColourId, colour::panel);
    allShortcuts.setColour(juce::TextButton::textColourOffId, colour::orange);
    allShortcuts.onClick = [this] { showAllShortcuts = !showAllShortcuts; refreshShortcuts(); resized(); };
    refreshShortcuts();
}
void Workstation::showShortcuts(bool visible)
{
    showingShortcuts = visible; resized(); repaint();
}
void Workstation::refreshShortcuts()
{
    // Keep this reference beside the UI, and check entries against the native key handlers.
    // Context is explicit: the same key may edit a marker, node or segment depending on focus.
    struct Shortcut { const char* keys; const char* command; const char* context; bool everyday = false; };
    static constexpr Shortcut entries[] {
        {"Space", "Play / pause", "Transport", true},
        {"Ctrl + S", "Save mix", "File", true},
        {"Ctrl + Z", "Undo", "Editing", true},
        {"Ctrl + Shift + Z", "Redo", "Editing", true},
        {"Ctrl + C", "Copy segment or selected range/group", "Editing", true},
        {"Ctrl + X", "Cut segment or selected range/group", "Editing", true},
        {"Ctrl + V", "Paste at playhead; cursor moves to end", "Editing", true},
        {"S", "Split selected segment at playhead", "Editing", true},
        {"Ctrl + D", "Duplicate selected segment", "Editing", true},
        {"Delete", "Remove selection", "Segment, range/group, label or selected node", true},
        {"Alt + Left / Right", "Previous / next transition", "Transitions", true},
        {"Ctrl + T", "Open Editing console", "Selected segment", true},
        {"Ctrl + O", "Open mix", "File"},
        {"Ctrl + Shift + S", "Save mix as", "File"},
        {"Ctrl + A", "Select all segments", "Timeline"},
        {"Ctrl + A", "Select all visible songs", "Library / Playlist table focused"},
        {"Ctrl + Shift + Delete", "Ripple delete segment across all lanes", "Editing"},
        {"Ctrl + Shift + G", "Close gap before segment across all lanes", "Editing"},
        {"Ctrl + W", "Review source BPM / key", "Selected source"},
        {"Escape", "Clear selection or cancel range / loop start", "Timeline"},
        {"Ctrl + Alt + Left / Right", "Move transition frame by one mix beat", "Selected transition"},
        {"Shift + Alt + Left / Right", "Resize transition end by one mix beat", "Selected transition"},
        {"Alt + L", "Lock / unlock transition", "Selected transition"},
        {"V", "Add volume automation node", "Selected segment at playhead"},
        {"B", "Add bass automation node", "Selected segment at playhead"},
        {"M", "Add mid automation node", "Selected segment at playhead"},
        {"T", "Add treble automation node", "Selected segment at playhead"},
        {"F", "Add filter automation node", "Selected segment at playhead"},
        {"E", "Add / choose VST automation parameter", "Selected segment at playhead"},
        {"P", "Add or edit tempo marker", "Playhead"},
        {"L", "Add label marker", "Playhead"},
        {"[ / ]", "Previous / next label", "Navigation"},
        {"9 / 0", "Set loop start / end", "Selected segment at playhead"},
        {"D / Shift + D", "Next / previous cut amount handle", "Timeline"},
        {"Up / Down", "Change cut amount by 1%", "Selected cut amount handle"},
        {"Shift + Up / Down", "Change cut amount by 10%", "Selected cut amount handle"},
        {"Enter", "Edit exact cut amount", "Selected cut amount handle"},
        {"Escape", "Deselect cut amount handle", "Timeline"},
        {"Left / Right", "Previous / next automation node", "Visible automation curve, timeline focused"},
        {"Enter", "Edit exact automation node value / time", "Selected automation node"},
        {"Delete", "Delete automation node", "Selected automation node"},
        {"Left / Right", "Previous / next section or hotcue", "Selected section or hotcue flag"},
        {"Up / Down", "Move section or hotcue by one mix beat", "Selected section or hotcue flag"},
        {"Shift + Up / Down", "Move section or hotcue by four mix beats", "Selected section or hotcue flag"},
        {"Enter", "Edit section label/position or hotcue position", "Selected section or hotcue flag"},
        {"Delete", "Clear hotcue slot", "Selected hotcue flag; section flags cannot be deleted"},
        {"Escape", "Deselect section or hotcue", "Selected section or hotcue flag"},
        {"Left / Right", "Previous / next tempo marker", "Tempo strip focused"},
        {"Enter / Delete", "Edit / remove tempo marker", "Tempo strip focused; first marker cannot be removed"},
        {"Escape", "Deselect tempo marker", "Tempo strip focused"},
        {"Left / Right", "Scroll timeline left / right", "Ruler or overview focused"},
        {"Home / End", "Scroll to timeline start / end", "Ruler or overview focused"},
        {"+ / -", "Zoom in / out", "Ruler or overview focused"},
        {"Shift + Left / Right", "Resize overview's left edge", "Overview focused"},
        {"Ctrl + Left / Right", "Resize overview's right edge", "Overview focused"},
        {"Up / Down", "Resize lane controls by 8 px", "Lane header focused; Shift gives 1 px"},
        {"Home / End", "Default / double lane height", "Lane header focused"},
        {"Up / Down", "Resize timeline by 10 px", "Bottom divider focused; Shift gives 1 px"},
        {"Left / Right", "Resize side panel by 10 px", "Side divider focused; Shift gives 1 px"},
        {"Home / End", "Reset / minimise timeline height", "Bottom divider focused"},
        {"Enter / Escape", "Read results / clear shortcut search", "Shortcut search focused"},
        {"Home / Enter", "Restore side panel", "Side divider focused"},
        {"End", "Collapse side panel", "Side divider focused"},
        {"Arrow keys", "Adjust focused dial", "Dial focused"},
        {"Home", "Reset focused dial", "Dial focused"},
        {"Space", "Play / pause source preview", "BPM / key review; outside numeric text fields"},
        {"Enter", "Select focused timeline object", "Keyboard accessibility"},
        {"Tab / Shift + Tab", "Next / previous control", "Keyboard focus"},
        {"Enter / Escape", "Apply / cancel dialog", "Where the dialog provides these actions"},
        {"F11", "Toggle fullscreen", "Window"},
        {"Alt + F4", "Close window", "Window; unsaved work is guarded"},
        {"Ctrl + Shift + A", "Toggle attract lighting", "Appearance"},
        {"Escape", "Exit attract lighting", "When no timeline selection needs clearing"},
        {"Alt + drag", "Bypass grid snapping", "Timeline edits"},
        {"Shift + drag", "Slip audio inside the segment", "Audio-slip header grip"},
        {"Shift + drag", "Fine dial adjustment", "Dial"},
        {"Ctrl + click / Shift + click", "Toggle / extend song selection", "Library / Playlist"}
    };
    const auto query = shortcutSearch.getText().trim().toLowerCase();
    const auto terms = juce::StringArray::fromTokens(query.replaceCharacter('+', ' '), " \t", "");
    juce::String text; int count = 0;
    for (const auto& entry : entries)
    {
        const auto haystack = (juce::String(entry.keys) + " " + entry.command + " " + entry.context).toLowerCase();
        bool match = true;
        if (query.containsChar('+') && query.length() > 1)
            match = juce::String(entry.keys).toLowerCase().removeCharacters(" \t").contains(query.removeCharacters(" \t"));
        else for (const auto& term : terms) if (term.isNotEmpty() && !haystack.contains(term)) { match = false; break; }
        if (!match || (query.isEmpty() && !showAllShortcuts && !entry.everyday)) continue;
        text += juce::String(entry.keys) + "\n" + entry.command;
        if (!entry.everyday) text += "\n" + juce::String(entry.context);
        text += "\n\n"; ++count;
    }
    shortcutHeading.setText(query.isNotEmpty() ? juce::String(count) + (count == 1 ? " result" : " results") : "Shortcuts", juce::dontSendNotification);
    allShortcuts.setButtonText(showAllShortcuts ? "Common" : "All");
    shortcutResults.setText(count ? text.trimEnd() : "No matching shortcuts.\nTry a command name or key.", false);
    shortcutResults.moveCaretToTop(false);
}

void Workstation::showInspectorPage(int page)
{
    // Keep legacy menu/context actions routed to their new homes.
    if (page == 0) { showToolPage(2); return; }
    showingShortcuts = false;
    inspectorPage = page == 2 ? 2 : 1;
    keyboardButton.setToggleState(inspectorPage == 2, juce::dontSendNotification);
    piano.setVisible(inspectorPage == 2); midiLabel.setVisible(inspectorPage == 2);
    sidebarHelp.setText({}, juce::dontSendNotification);
    pluginList.list.setTooltip("Effects and effect automation apply to every split piece of the selected original track, across cuts and gaps. Separate added tracks keep independent effects. With no segment selected, effects process the whole selected lane.");
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
    browserPage = juce::jlimit(0, 1, page); correctionFromLibrary = browserPage == 0;
    libraryTab.setToggleState(browserPage == 0, juce::dontSendNotification); playlistTab.setToggleState(browserPage == 1, juce::dontSendNotification);
    for (auto* c : std::initializer_list<juce::Component*> {&libraryTable.table, &search, &keyDisplay, &trackInfoButton, &musicScanNotice}) c->setVisible(!layout.libraryCollapsed && browserPage == 0);
    playlistTable.table.setVisible(!layout.libraryCollapsed && browserPage == 1);
    resized(); repaint();
}
void Workstation::resizeSidePanel(bool left, int width)
{
    layout.resizePanel(left, width); workspaceDirty = true; workspaceChangedAt = juce::Time::getMillisecondCounterHiRes(); resized(); repaint();
}
void Workstation::resizeTimeline(int height)
{
    if (consoleCollapsed) { consoleCollapsed = false; layout.timeline = {}; resized(); }
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
NamedWorkspace Workstation::captureNamedWorkspace(const juce::String& name) const
{
    NamedWorkspace preset; preset.name = name; preset.layout = layout; preset.layout.toolPage = toolPage;
    preset.inspectorPage = inspectorPage; preset.browserPage = browserPage; preset.zoom = timeline.pixelsPerSecond;
    preset.view = viewport.getViewPosition();
    const std::array<const juce::DocumentWindow*, 3> windows {performanceWindow.get(), transitionWindow.get(), editingWindow.get()};
    for (int i = 0; i < 3; ++i) if (windows[(size_t) i])
    { preset.detached[(size_t) i] = true; preset.windows[(size_t) i] = windows[(size_t) i]->getBounds(); }
    return preset;
}
juce::Result Workstation::restoreNamedWorkspace(const juce::File& file)
{
    if (busy || engine.exporting) return juce::Result::fail("Wait for the current operation before restoring a workspace layout.");
    NamedWorkspace preset; const auto result = readNamedWorkspace(file, preset); if (result.failed()) return result;
    const bool monitor = layout.monitorMuted;
    for (int page = 0; page < 3; ++page) detachToolPanel(page, false);
    layout = preset.layout; layout.monitorMuted = monitor; toolPage = preset.layout.toolPage;
    timeline.pixelsPerSecond = preset.zoom;
    hints.setHintsEnabled(layout.popupHints); if (performance) performance->setForgeEffects(forgeMotionAllowed());
    showBrowserPage(preset.browserPage); showInspectorPage(preset.inspectorPage);
    const auto& displays = juce::Desktop::getInstance().getDisplays();
    for (int page = 0; page < 3; ++page) if (preset.detached[(size_t) page])
    {
        detachToolPanel(page, true);
        auto* window = page == 0 ? performanceWindow.get() : page == 1 ? transitionWindow.get() : editingWindow.get();
        const auto requested = preset.windows[(size_t) page];
        const auto* display = displays.getDisplayForRect(requested);
        if (!display) display = displays.getPrimaryDisplay();
        const auto area = display ? display->userBounds.getLargestIntegerWithin() : getScreenBounds();
        if (window) window->setBounds(requested.withSize(std::min(requested.getWidth(), area.getWidth()), std::min(requested.getHeight(), area.getHeight())).constrainedWithin(area));
    }
    toolPage = preset.layout.toolPage;
    if (preset.detached[(size_t) toolPage]) for (int page : layout.consoleOrder) if (!preset.detached[(size_t) page]) { toolPage = page; break; }
    trackEditingWorkspace(); workspaceDirty = true; workspaceChangedAt = juce::Time::getMillisecondCounterHiRes();
    resized(); refresh();
    { const juce::ScopedValueSetter<bool> viewGuard(programmaticTimelineView, true); viewport.setViewPosition(preset.view); }
    setFollowPlayhead(layout.followPlayhead);
    setStatus("Workspace layout restored: " + preset.name); return juce::Result::ok();
}
void Workstation::chooseWorkspaceLayout()
{
    if (busy || engine.exporting) return;
    if (workspacePreference == juce::File()) { setStatus("Local workspace storage is unavailable."); return; }
    const auto root = workspacePreference.getSiblingFile("workspace-layouts");
    if (root.isSymbolicLink()) { setStatus("Workspace layout storage is a link. Existing files were kept."); return; }
    juce::Array<juce::File> files; if (root.isDirectory()) files = root.findChildFiles(juce::File::findFiles, false, "*.alloyworkspace");
    juce::StringArray names; juce::Array<juce::File> valid;
    std::vector<std::pair<juce::String, juce::File>> entries;
    for (const auto& file : files)
    {
        NamedWorkspace preset;
        if (readNamedWorkspace(file, preset).wasOk()) entries.emplace_back(preset.name, file);
    }
    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) { return a.first.compareNatural(b.first) < 0; });
    for (const auto& entry : entries) if (valid.size() < 128) { names.add(entry.first); valid.add(entry.second); }
    auto* dialog = new juce::AlertWindow("Workspace layouts", "Save or open a layout.", juce::MessageBoxIconType::NoIcon, this);
    dialog->addComboBox("layouts", names, "Saved layout");
    dialog->getComboBoxComponent("layouts")->setTextWhenNothingSelected(valid.isEmpty() ? "No saved layouts yet" : "Select a layout");
    int suffix = 1; while (names.contains("Workspace " + juce::String(suffix), true)) ++suffix;
    dialog->addTextEditor("name", "Workspace " + juce::String(suffix), "Name for a new layout (1-64 characters)");
    dialog->addButton("Save current", 1); dialog->addButton("Restore", 2); dialog->addButton("Close", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    dialog->getButton(1)->setEnabled(!valid.isEmpty());
    dialog->enterModalState(true, juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<Workstation>(this), dialog, root, valid, names](int answer)
    {
        if (!safe || answer == 0 || safe->busy || safe->engine.exporting) return;
        if (answer == 1)
        {
            if (valid.size() >= 128) { safe->setStatus("The workspace layout list is full (128 layouts). Existing layouts were kept."); return; }
            const auto name = dialog->getTextEditorContents("name").trim();
            if (name.isEmpty() || name.length() > 64) { safe->setStatus("Enter a workspace name of 1-64 characters."); return; }
            if (names.contains(name, true)) { safe->setStatus("That workspace name already exists. Choose a new name; the saved layout was kept."); return; }
            const auto file = root.getChildFile(juce::Uuid().toString() + ".alloyworkspace");
            const auto saved = writeNamedWorkspace(file, safe->captureNamedWorkspace(name));
            safe->setStatus(saved.wasOk() ? "Workspace layout saved: " + name : saved.getErrorMessage());
        }
        else if (answer == 2)
        {
            const int index = dialog->getComboBoxComponent("layouts")->getSelectedItemIndex();
            if (index < 0 || index >= valid.size()) { safe->setStatus("Select a saved workspace layout to restore."); return; }
            const auto loaded = safe->restoreNamedWorkspace(valid[index]); if (loaded.failed()) safe->setStatus(loaded.getErrorMessage());
        }
    }), true);
}
void Workstation::chooseConsoleOrder()
{
    if (busy || engine.exporting) return;
    auto* dialog = new juce::AlertWindow("Arrange console tabs", "Choose the tab order.", juce::MessageBoxIconType::NoIcon, this);
    const juce::StringArray names {"Controller", "Transitions", "Editing"};
    for (int i = 0; i < 3; ++i)
    { const auto id = "position" + juce::String(i); dialog->addComboBox(id, names, "Position " + juce::String(i + 1)); dialog->getComboBoxComponent(id)->setSelectedItemIndex(layout.consoleOrder[(size_t) i]); }
    dialog->addButton("Apply", 1, juce::KeyPress(juce::KeyPress::returnKey)); dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    dialog->enterModalState(true, juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<Workstation>(this), dialog](int answer)
    {
        if (!safe || answer != 1 || safe->busy || safe->engine.exporting) return;
        std::array<int, 3> order; std::array<bool, 3> seen {};
        for (int i = 0; i < 3; ++i)
        {
            const int page = dialog->getComboBoxComponent("position" + juce::String(i))->getSelectedItemIndex();
            if (page < 0 || page > 2 || seen[(size_t) page]) { safe->setStatus("Choose Controller, Transitions and Editing once each. The console order was kept."); return; }
            order[(size_t) i] = page; seen[(size_t) page] = true;
        }
        safe->layout.consoleOrder = order; safe->workspaceDirty = true; safe->workspaceChangedAt = juce::Time::getMillisecondCounterHiRes();
        safe->resized(); safe->repaint(); safe->setStatus("Console tab order updated. Named workspace layouts can save this order.");
    }), true);
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
    g.setColour(colour::orange); g.setFont(juce::FontOptions(15)); g.drawText("ALLOY", brand.removeFromLeft(47), juce::Justification::centredLeft);
    g.setColour(colour::muted); g.setFont(juce::FontOptions(10)); g.drawText("V. " + juce::String(JUCE_APPLICATION_VERSION_STRING), brand, juce::Justification::centredLeft);
    g.setColour(colour::muted); g.setFont(juce::FontOptions(11));
    const int identityWidth = std::min(250, getWidth() / 4);
    g.drawText(juce::String(attractState.enabled ? "ATTRACT LIGHTS / Esc exits | " : "") + (busy ? "WORKING / " : "") + status, layout.status.withTrimmedRight(identityWidth + 12), juce::Justification::centredLeft);
    g.setColour(dirty ? colour::orange : colour::muted); g.drawText((sessionFile == juce::File() ? "Untitled mix" : sessionFile.getFileName()) + (dirty ? " *" : ""), layout.status.withLeft(layout.status.getRight() - identityWidth), juce::Justification::centredRight);
    auto meter = layout.master.withTrimmedTop(70).withHeight(5).reduced(12, 0);
    g.setColour(colour::background); g.fillRect(meter);
    const float peak = engine.masterPeak.load();
    AlloyLookAndFeel::drawMoltenMetal(g, meter.withWidth((int) (meter.getWidth() * juce::jlimit(0.0f, 1.0f, peak))).toFloat(), false, juce::jlimit(0.f, 1.f, peak));
    g.setColour(colour::muted); g.setFont(juce::FontOptions(10));
    g.drawText(peak >= 1 ? "Lower master level" : "Stereo output", layout.master.withTrimmedTop(80).reduced(12, 0), juce::Justification::left);
    peakDisplay.paint(g, layout.peakPanel, forgeMotionAllowed());
}
void Workstation::resized()
{
    const juce::ScopedValueSetter<bool> viewGuard(programmaticTimelineView, true);
    const auto laneShares = layout.laneScale;
    const int savedHeight = layout.preferredTimelineHeight;
    if (consoleCollapsed) { layout.preferredTimelineHeight = 0; layout.timeline = {}; }
    layout.update(getWidth(), getHeight(), !consoleCollapsed && (!performanceWindow || !transitionWindow || !editingWindow), !consoleCollapsed && toolPage == 2 && !editingWindow); menuBar.setBounds(layout.menu);
    if (consoleCollapsed) layout.preferredTimelineHeight = savedHeight;
    statusAccess.setBounds(layout.status);
    if (laneShares != layout.laneScale) { workspaceDirty = true; workspaceChangedAt = juce::Time::getMillisecondCounterHiRes(); }
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
    const int settingsWidth = narrowToolbar ? 184 : 232;
    const int actionBudget = toolbar.getWidth() - (settingsWidth - 120) - 100 - waveformWidth - 4 - 256 - 8 - (narrowToolbar ? 546 : 640);
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
    settingsButton.setBounds(layout.toolbar.withLeft(layout.toolbar.getRight() - settingsWidth));
    autoArrangeButton.setBounds(settingsButton.getX() - 128, settingsButton.getY(), 120, settingsButton.getHeight());
    layout.transport = toolbar.withRight(autoArrangeButton.getX() - 8);
    auto modes=layout.menu.removeFromRight(244); workModeButton.setBounds(modes.removeFromLeft(132).reduced(1,2)); takeButton.setBounds(modes.reduced(1,2));
    menuBar.setBounds(layout.menu);
    auto transport = layout.transport;
    const bool compact = transport.getWidth() < 700;
    button(homeButton, transport, compact ? 26 : 30); button(playButton, transport, compact ? 40 : 52); button(stopButton, transport, compact ? 38 : 48);
    button(timeLabel, transport, compact ? 68 : 116); button(tempoSlider, transport, compact ? 68 : 106); button(snapButton, transport, compact ? 68 : 106);
    button(gridDimmer, transport, compact ? 48 : 112);
    bottomSplitter.setVisible(true);
    bottomSplitter.setEnabled(true); bottomSplitter.setBounds(layout.timelineDivider);
    bottomSplitter.setRange(layout.minimumTimelineHeight, layout.maximumTimelineHeight, 1);
    bottomSplitter.setValue(layout.timeline.getHeight(), juce::dontSendNotification);
    leftSplitter.setBounds(layout.leftDivider); rightSplitter.setBounds(layout.rightDivider);
    const int panelBudget = getWidth() - 44 - WorkspaceLayout::minimumCentreWidth;
    leftSplitter.setRange(0, std::max(240, panelBudget - layout.master.getWidth()), 1);
    rightSplitter.setRange(0, std::max(240, panelBudget - layout.library.getWidth()), 1);
    leftSplitter.setValue(layout.library.getWidth(), juce::dontSendNotification); rightSplitter.setValue(layout.master.getWidth(), juce::dontSendNotification);
    leftSplitter.setTitle(layout.libraryCollapsed ? "Restore library" : "Resize or collapse library");
    leftSplitter.setTooltip(layout.libraryCollapsed ? "Library collapsed. Double-click, Enter or Home restores its previous width; drag right to open." : "Drag left or press End to collapse the library. Enter, Home or double-click restores width.");
    for (auto* c : std::initializer_list<juce::Component*> {&libraryTab, &playlistTab}) c->setVisible(!layout.libraryCollapsed);
    for (auto* c : std::initializer_list<juce::Component*> {&libraryTable.table, &search, &keyDisplay, &trackInfoButton, &musicScanNotice}) c->setVisible(!layout.libraryCollapsed && browserPage == 0);
    playlistTable.table.setVisible(!layout.libraryCollapsed && browserPage == 1);
    rightSplitter.setTitle(layout.inspectorCollapsed ? "Restore right inspector" : "Resize or collapse right inspector");
    rightSplitter.setTooltip(layout.inspectorCollapsed ? "Inspector collapsed. Double-click, Enter or Home restores its previous width; drag left to open. Master level remains beside the peak meter." : "Drag right or press End to collapse the entire inspector; drag left to widen. Enter, Home or double-click restores width.");
    auto library = layout.library.reduced(8, 6); auto row = library.removeFromTop(30);
    button(libraryTab, row, std::min(156, std::max(84, row.getWidth() - 76))); button(playlistTab, row, 68);
    library.removeFromTop(8); search.setBounds(library.removeFromTop(26)); library.removeFromTop(6);
    row = library.removeFromTop(26); button(trackInfoButton, row, 92); keyDisplay.setBounds(row); library.removeFromTop(4);
    musicScanNotice.setBounds(library.removeFromTop(38)); library.removeFromTop(4); libraryTable.table.setBounds(library);
    auto playlist = layout.playlist.reduced(8, 6); playlist.removeFromTop(38); playlistTable.table.setBounds(playlist);
    trackHeaders.setBounds(layout.headers); viewport.setBounds(layout.timeline); timeline.updateSize(); layoutTrackHeaders();
    timelineRuler.setBounds(layout.ruler.withWidth(viewport.getViewWidth())); timelineOverview.setBounds(layout.overview);
    const auto laneControls = layout.headers.withY(layout.ruler.getY()).withHeight(Timeline::rulerHeight);
    if (masterPanelButton) masterPanelButton->setBounds(laneControls.withSize(34, 34).translated(4, 5));
    auto laneTools = laneControls.withTrimmedLeft(42).reduced(2, 0);
    followButton.setBounds(laneTools.withSizeKeepingCentre(laneTools.getWidth(), 26));
    auto master = layout.master.reduced(12, 8); masterLabel.setText("MASTER OUTPUT", juce::dontSendNotification); masterLabel.setBounds(master.removeFromTop(22)); master.removeFromTop(4); auto masterRow = master.removeFromTop(28); monitorMute.setBounds(masterRow.removeFromRight(66)); masterSlider.setBounds(masterRow);
    peakReset.setBounds(layout.peakPanel.reduced(10, 0).withY(layout.peakPanel.getY() + 9).withHeight(23));
    const bool tallMeter = layout.peakPanel.getHeight() >= 240 || layout.inspectorCollapsed;
    masterSlider.setSliderStyle(tallMeter ? juce::Slider::RotaryHorizontalVerticalDrag : juce::Slider::LinearHorizontal);
    masterSlider.setTextBoxStyle(tallMeter ? juce::Slider::TextBoxBelow : juce::Slider::TextBoxRight, false, 72, 20);
    if (tallMeter) masterSlider.setBounds(layout.peakPanel.withTop(layout.peakPanel.getBottom() - 94).reduced(16, 3));
    auto detail = layout.detail.reduced(12, 10);
    auto tabs = detail.removeFromTop(28);
    effectsTab.setBounds(tabs.removeFromLeft(tabs.getWidth() / 2)); tabs.removeFromLeft(4); shortcutsTab.setBounds(tabs);
    effectsTab.setToggleState(!showingShortcuts, juce::dontSendNotification);
    shortcutsTab.setToggleState(showingShortcuts, juce::dontSendNotification);
    effectsTab.setVisible(!layout.inspectorCollapsed); shortcutsTab.setVisible(!layout.inspectorCollapsed);
    detail.removeFromTop(10);
    auto help = detail;
    shortcutSearch.setBounds(help.removeFromTop(28)); help.removeFromTop(8);
    auto heading = help.removeFromTop(26); allShortcuts.setBounds(heading.removeFromRight(showAllShortcuts ? 82 : 44));
    shortcutHeading.setBounds(heading); help.removeFromTop(4); shortcutResults.setBounds(help);
    for (auto* c : std::initializer_list<juce::Component*> {&shortcutSearch, &shortcutHeading, &allShortcuts, &shortcutResults})
        c->setVisible(!layout.inspectorCollapsed && showingShortcuts);
    auto access = detail.removeFromTop(28);
    keyboardButton.setBounds(access.removeFromLeft(std::min(92, access.getWidth() / 2))); access.removeFromLeft(4); instrumentSetupButton.setBounds(access);
    detail.removeFromTop(8);
    sidebarHelp.setBounds({});
    if (inspectorPage == 2)
    {
        auto keys = detail.removeFromBottom(148);
        midiLabel.setBounds(keys.removeFromTop(36)); keys.removeFromTop(6); piano.setBounds(keys);
        detail.removeFromBottom(8);
    }
    pluginLabel.setBounds(detail.removeFromTop(36)); detail.removeFromTop(6);
    addPluginButton.setBounds(detail.removeFromTop(28)); detail.removeFromTop(8);
    auto pluginActions = detail.removeFromBottom(62);
    row = pluginActions.removeFromTop(28);
    button(editPluginButton, row, 62); button(bypassPluginButton, row, 74); removePluginButton.setBounds(row);
    pluginActions.removeFromTop(6); pluginControlsButton.setBounds(pluginActions.removeFromTop(28));
    pluginList.list.setBounds(detail);
    masterLabel.setVisible(!layout.inspectorCollapsed); monitorMute.setVisible(!layout.inspectorCollapsed);
    for (auto* c : std::initializer_list<juce::Component*> {&keyboardButton, &instrumentSetupButton, &sidebarHelp, &pluginLabel,
        &addPluginButton, &editPluginButton, &bypassPluginButton, &removePluginButton, &pluginControlsButton, &pluginList.list}) c->setVisible(!layout.inspectorCollapsed && !showingShortcuts);
    piano.setVisible(!layout.inspectorCollapsed && !showingShortcuts && inspectorPage == 2); midiLabel.setVisible(!layout.inspectorCollapsed && !showingShortcuts && inspectorPage == 2);
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
    performancePanel.setAttractState(attractState); transitionPanel.setAttractState(attractState); editingPanel.setAttractState(attractState);
}
void Workstation::setAttractMode(bool active)
{
    if (attractState.enabled == active) return;
    attractState.enabled = active;
    if (active) attractState.startedAtMs = juce::Time::getMillisecondCounterHiRes();
    attractLights.syncMode(); performancePanel.syncAttract(); transitionPanel.syncAttract(); editingPanel.syncAttract(); repaint();
}
void Workstation::refreshAttractLights()
{
    if (!attractState.enabled || !attractState.allowsMotion()) return;
    if (isShowing()) attractLights.repaint();
    performancePanel.refreshAttract(); transitionPanel.refreshAttract(); editingPanel.refreshAttract();
}
}
