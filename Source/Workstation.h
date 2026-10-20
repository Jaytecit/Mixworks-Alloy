#pragma once
#include <optional>
#include "Engine.h"
#include "WorkspaceLayout.h"
#include "PerformanceDeck.h"
#include "ToolPanel.h"
#include "AlloyLookAndFeel.h"
#include "PeakMeter.h"
#include "MusicScanner.h"
#include "LibraryCatalogue.h"
#include "Recovery.h"

namespace mixworks
{
struct AutoArrangementOptions;
class Workstation;
enum class TimelineAction
{
    cut = 200, copy, paste, remove, volume, treble, mid, bass, label, effect,
    loopStart, loopEnd, split, reverse, tweaks, settings, downbeat, tempo,
    curveRead, curvePoint, curveClear, choosePlugin, recordOff, recordTouch, recordLatch, armAutomation, pairBassSwap
};
class TempoStrip final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit TempoStrip(Workstation&);
    static constexpr int stripHeight = WorkspaceLayout::tempoStripHeight;
    int selectedTempoPoint = -1;
    void paint(juce::Graphics&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
private:
    friend void runTempoBrowserTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    Workstation& owner;
    bool draggingTempo = false, moved = false;
    TempoMap tempoDraft;
    float markerX(int index) const;
    static constexpr float tempoY() { return stripHeight * 0.5f; }
    double timeAt(float x) const;
    juce::Rectangle<float> tempoValueBounds(int index) const;
    int tempoValueAt(juce::Point<float>) const;
    int tempoPointAt(juce::Point<float>) const;
    void removeSelected();
    friend void runTempoUiTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
};
class TimelineViewport final : public juce::Viewport
{
public:
    std::function<void()> onViewChanged;
private:
    void visibleAreaChanged(const juce::Rectangle<int>&) override { if (onViewChanged) onViewChanged(); }
};
struct TimelinePlacement
{
    int track = 0;
    double time = 0;
};
class ImportProgressView final : public juce::Component
{
public:
    ImportProgressView();
    void paint(juce::Graphics&) override;
    void resized() override;
    void begin(int totalFiles);
    void showFile(const juce::String&, MediaReadStage);
    void finishFile();
    void setCounts(int completedFiles, int totalFiles);
    void tick();
    void requestStop();
    void setExporting(const juce::File&, const WavExportPlan&);
    void showExportProgress(juce::int64 frames);
    juce::TextButton cancel { "Stop import" };
private:
    juce::Label heading, file, stage, detail;
    double fraction = 0, startedAt = 0;
    juce::ProgressBar bar { fraction };
    int completed = 0, total = 0;
    bool stopping = false, exporting = false;
    juce::int64 renderedFrames = 0;
    WavExportPlan exportPlan;
    juce::String stopMessage;
public:
    void setOpeningSession() { heading.setText("Opening session", juce::dontSendNotification); stopMessage = "Cancelling after this track. Your current session will stay open."; }
    void setAutoArrangement() { heading.setText("Auto Arrange", juce::dontSendNotification); file.setText("Matching library tracks", juce::dontSendNotification); stage.setText("Choosing tracks by key and BPM", juce::dontSendNotification); fraction = -1; cancel.setButtonText("Cancel arrangement"); stopMessage = "Cancelling arrangement. Your current mix will be kept."; }
    void setAutoMixPreparation(int count) { completed = total = count; fraction = -1; stage.setText("Preparing mix audio and transitions...", juce::dontSendNotification); tick(); }
    void setReanalysis() { heading.setText("Re-analysing tracks", juce::dontSendNotification); file.setText("Starting analysis...", juce::dontSendNotification); cancel.setButtonText("Stop analysis"); stopMessage = "Stopping after this track. Completed analyses and all manual corrections will be kept."; }
    void setRehearsal() { heading.setText("Preparing A / B / C rehearsal", juce::dontSendNotification); file.setText("Rendering saved transition alternatives", juce::dontSendNotification); stage.setText("Session effects / matched RMS", juce::dontSendNotification); cancel.setButtonText("Cancel rehearsal"); stopMessage = "Cancelling rehearsal. Your session will be kept."; }
};
class TimelineHeader final : public juce::Component, public juce::SettableTooltipClient
{
public:
    TimelineHeader(Workstation&, bool overview);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;
    juce::Rectangle<float> previewBounds() const;
private:
    Workstation& owner;
    bool overview;
    float grabOffset = 0;
    double overviewWidth() const;
    void pan(float x);
    juce::MouseEvent timelineEvent(const juce::MouseEvent&) const;
};
class Timeline final : public juce::Component, public juce::DragAndDropTarget, public juce::SettableTooltipClient
{
public:
    explicit Timeline(Workstation&);
    static constexpr int timeRulerHeight = WorkspaceLayout::timeRulerHeight, labelHeight = WorkspaceLayout::labelHeight;
    static constexpr int rulerHeight = timeRulerHeight + labelHeight, laneHeight = WorkspaceLayout::laneHeight;
    int contentLaneCount() const;
    int laneTop(int track) const;
    int laneSize(int track) const;
    int trackAt(float y) const;
    float laneY(int track, float defaultY) const;
    void paint(juce::Graphics&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void zoomAt(double factor, double timelineX);
    double pixelsPerSecond = 10;
    void updateSize();
    void selectAutomationPoint(const AutomationAddress&, int point);
    juce::String automationPointLabel(const AutomationAddress&, int point) const;
    bool isInterestedInDragSource(const SourceDetails&) override;
    void itemDragEnter(const SourceDetails&) override;
    void itemDragMove(const SourceDetails&) override;
    void itemDragExit(const SourceDetails&) override;
    void itemDropped(const SourceDetails&) override;
    bool shouldDrawDragImageWhenOver() override { return false; }
    juce::String selectedLabel;
private:
    Workstation& owner;
    int resizingLane = -1, resizeStartHeight = 0;
    int dividerAt(float y) const;
    friend class TimelineHeader;
    std::optional<TimelinePlacement> dropPreview;
    std::optional<TimelinePlacement> placementAt(juce::Point<int>) const;
    void previewDrop(juce::Point<int>);
    void clearDropPreview();
    void paintDropPreview(juce::Graphics&);
    friend class Workstation;
    friend void runTimelineEditingTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runTransitionBandTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runTransitionFilterTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runTransitionNavigationTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runRehearsalContextTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runAudioSlipTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runTransitionCutTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runTransitionSwapTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runBarNavigationTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runPlacementTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    juce::String dragging;
    double dragStart = 0;
    std::optional<Clip> dragClip;
    std::optional<Project> clipDrag;
    std::optional<Project> anchorDrag;
    std::optional<std::pair<juce::String, Project::TransitionEdit>> transitionDrag;
    struct SwapHandle { juce::String incoming; int band = 1; bool incomingSide = false, legacy = false; TransitionBandMode mode = TransitionBandMode::swap; double time = 0; juce::Rectangle<float> bounds; };
    struct SwapGesture { SwapHandle handle; float x = 0; };
    std::optional<SwapGesture> swapDrag;
    struct AudioSlipGesture { Clip baseline; float x = 0; };
    std::optional<AudioSlipGesture> audioSlipDrag;
    juce::Rectangle<float> audioSlipBounds(const Clip&) const;
    std::optional<juce::String> audioSlipAt(juce::Point<float>) const;
    void paintAudioSlipHandles(juce::Graphics&);
    bool beginAudioSlip(const juce::MouseEvent&);
    bool continueAudioSlip(const juce::MouseEvent&);
    bool resetAudioSlip(juce::Point<float>);
    std::vector<SwapHandle> swapHandles() const;
    std::optional<SwapHandle> swapHandleAt(juce::Point<float>) const;
    void paintSwapHandles(juce::Graphics&);
    bool beginSwapDrag(const juce::MouseEvent&);
    bool continueSwapDrag(const juce::MouseEvent&);
    std::optional<std::pair<juce::String, Project::TransitionEdit>> transitionAt(juce::Point<float>) const;
    juce::Rectangle<float> transitionBounds(const Project::TransitionLink&) const;
    juce::Rectangle<float> transitionSongBounds(const Project::TransitionLink&, bool outgoing) const;
    void paintTransitions(juce::Graphics&);
    double transitionReference(const Project&, const std::pair<juce::String, Project::TransitionEdit>&) const;
    bool draggingIntro = false;
    juce::Point<int> origin;
    bool moved = false;
    std::optional<std::pair<AutomationAddress, int>> editingAutomationPoint;
    juce::String draggingLabel;
    int labelAt(juce::Point<float>) const;
    std::optional<std::pair<juce::String, bool>> anchorAt(juce::Point<float>) const;
    std::optional<std::pair<AutomationAddress, int>> automationPointAt(juce::Point<float>) const;
    friend void runTempoUiTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runFusionWorkflowTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    float automationY(int track, float db, int target = -1) const;
    float automationDb(int track, float y, AutomationTarget) const;
    bool hasGeneratedAutomation(const AutomationAddress&) const;
    float automationDisplayValue(const AutomationAddress&, double sessionTime, float manual, const TransitionPlayback* = nullptr) const;
    float automationManualValue(const AutomationAddress&, double sessionTime, float displayed) const;
    juce::Rectangle<float> clipBounds(const Clip&) const;
};
class LibraryTable final : public juce::TableListBoxModel
{
public:
    LibraryTable(Workstation&, bool playlist);
    void rebuildPlaylist();
    Clip* clipAtRow(int row) const;
    int getNumRows() override;
    void paintRowBackground(juce::Graphics&, int, int, int, bool) override;
    void paintCell(juce::Graphics&, int, int, int, int, bool) override;
    juce::Component* refreshComponentForCell(int, int, bool, juce::Component*) override;
    void cellDoubleClicked(int, int, const juce::MouseEvent&) override;
    void selectedRowsChanged(int) override;
    void sortOrderChanged(int, bool) override;
    juce::String getCellTooltip(int, int) override;
    juce::var getDragSourceDescription(const juce::SparseSet<int>&) override;
    bool mayDragToExternalWindows() const override { return false; }
    juce::TableListBox table;
private:
    Workstation& owner;
    bool isPlaylist;
    std::vector<juce::String> playlistRows;
    int playlistSortColumn = 0;
    bool playlistSortForward = true;
};
class PluginWindow final : public juce::DocumentWindow
{
public:
    explicit PluginWindow(PluginSlot&);
    juce::String slotId;
    std::shared_ptr<ClipEffects> effectsOwner;
    ~PluginWindow() override { clearContentComponent(); }
    void closeButtonPressed() override { setVisible(false); }
};
class PluginList final : public juce::ListBoxModel
{
public:
    explicit PluginList(Workstation&);
    int getNumRows() override;
    void paintListBoxItem(int, juce::Graphics&, int, int, bool) override;
    void listBoxItemDoubleClicked(int, const juce::MouseEvent&) override;
    juce::String getTooltipForRow(int) override;
    juce::ListBox list;
private:
    Workstation& owner;
};
class TrackStrip final : public juce::Component, public juce::SettableTooltipClient
{
public:
    TrackStrip(Workstation&, int);
    void paint(juce::Graphics&) override;
    void resized() override;
    void refresh();
    bool keyPressed(const juce::KeyPress&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
private:
    Workstation& owner;
    int index;
    int resizeStartHeight = 0;
    bool resizingLane = false;
    bool controlGesture = false;
    juce::TextButton select, mute { "M" }, solo { "S" };
    juce::Slider gain;
    AlloyDial pan, lowPass;
    friend void runAlloyTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
};
class Workstation final : public juce::Component, public juce::DragAndDropContainer, public juce::FileDragAndDropTarget, private juce::Timer, private juce::MenuBarModel
{
public:
    explicit Workstation(bool openAudioDevice = true);
    ~Workstation() override;
    Engine engine;
    friend void runTempoBrowserTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runTrackOwnershipTests(const std::function<void(bool, const juce::String&)>&, const juce::File&, const juce::File&);
    friend void runAnalysisRefreshTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runRecoveryTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runMasteringTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    juce::String& selectedClip = engine.selectedClip;
    juce::String selectedLibraryId;
    juce::String selectedTransition; // Incoming clip identity selects one explicit relationship.
    juce::String status = "Drop tracks onto a lane to match tempo and align beats. Select a transition frame to edit it in the lower Transitions panel.";
    Timeline timeline;
    TimelineHeader timelineRuler, timelineOverview;
    TempoStrip tempoStrip;
    LibraryTable libraryTable, playlistTable;
    PluginList pluginList;
    std::vector<std::unique_ptr<PluginWindow>> editors;
    void paint(juce::Graphics&) override;
    void resized() override;
    void resizeLane(int lane, int height);
    bool keyPressed(const juce::KeyPress&) override;
    bool isInterestedInFileDrag(const juce::StringArray&) override { return true; }
    void fileDragEnter(const juce::StringArray&, int, int) override;
    void fileDragMove(const juce::StringArray&, int, int) override;
    void fileDragExit(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&, int, int) override;
    void importFiles(const juce::Array<juce::File>&);
    std::vector<std::shared_ptr<Media>> analysisTargets(int scope) const; // 0 selected, 1 needs review, 2 all.
    void reanalyseTracks(int scope);
    void selectTracksNeedingAnalysis();
    void openProjectFile(const juce::File&);
    void requestClose(std::function<void()>);
    void addMedia(int, std::optional<TimelinePlacement> = {});
    void selectTrack(int);
    void refresh();
    void selectTransition(const juce::String& incoming, bool focus = false);
    void remember();
    void showEditor(int);
    void fail(const juce::String&);
    Clip* currentClip();
private:
    AlloyLookAndFeel look;
    AttractState attractState;
    AttractLights attractLights {*this, attractState};
    void setAttractMode(bool);
    void initialiseAttract();
    void refreshAttractLights();
    friend void runAttractTests(Workstation&, const std::function<void(bool, const juce::String&)>&, const juce::File&);
    WorkspaceLayout layout;
    PeakMeterDisplay peakDisplay;
    double peakDisplayTime = 0;
    juce::TextButton peakReset {"PEAK"};
    void advancePeakMeter();
    bool forgeMotionAllowed() const;
    std::unique_ptr<PerformanceDeck> performance;
    PanelViewport performanceViewport;
    ToolPanel performancePanel {performanceViewport, "Controller"};
    std::unique_ptr<juce::DocumentWindow> performanceWindow, transitionWindow;
    juce::TextButton controllerTab {"Controller"};
    int toolPage = 0;
    void showToolPage(int page);
    void layoutToolPanels();
    void detachToolPanel(int page, bool detached);
    void initialisePerformance();
    void layoutPerformance();
    void performAction(MidiAction);
    PanelSplitter leftSplitter {true}, rightSplitter {false};
    TimelineSplitter bottomSplitter;
    juce::TextButton libraryTab {"Library"}, playlistTab {"Playlist"};
    int browserPage = 0;
    juce::File workspacePreference;
    bool workspaceDirty = false;
    double workspaceChangedAt = 0;
    void showBrowserPage(int);
    void resizeSidePanel(bool left, int width);
    void resizeTimeline(int height);
    double displayedTempo() const;
    void saveWorkspacePreferences();
    juce::File recoveryRoot;
    std::shared_ptr<RecoveryStore> recoveryStore;
    std::shared_ptr<RecoveryStore> restoredRecovery;
    juce::String restoredRecoveryFingerprint;
    juce::ThreadPool recoveryWorkers { 1 };
    int recoveryJobs = 0;
    bool recoveryOffered = false, recoveryPromptOpen = false;
    double nextRecovery = 0;
    void tickRecovery();
    void clearRecovery();
    void offerRecovery();
    void openProjectDocument(juce::String, const juce::File&, bool recovered = false,
                             std::shared_ptr<RecoveryStore> recovery = {}, juce::String fingerprint = {});
    juce::MenuBarComponent menuBar {this};
    juce::TextButton controllerConnection;
    double nextControllerPoll = 0;
    void refreshControllerConnection();
    void showControllerConnection(const MidiConnectionStatus&);
    friend void runMidiConnectionTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    juce::TextButton displayButton {"Display"}, windowModeButton {"Windowed"}, minimiseButton {"_"}, closeAppButton {"X"};
    void toggleWindowMode();
    juce::Component trackHeaders;
    juce::TextButton clipTab { "Clip" }, effectsTab { "Effects" }, midiTab { "MIDI" };
    juce::TextButton transitionTab { "Transitions" }, transitionLock { "Lock transition" };
    juce::TextButton previousTransition { "< Previous" }, nextTransition { "Next >" };
    juce::ComboBox selectedTransitionLength, transitionPresetForSelection;
    juce::Label transitionLengthLabel, transitionPresetLabel, transitionDefaultsLabel;
    juce::Component transitionBody;
    PanelViewport transitionViewport;
    ToolPanel transitionPanel {transitionViewport, "Transitions"};
    std::array<juce::Label, 4> transitionBandLabels;
    std::array<juce::ComboBox, 4> transitionBandModes, transitionBandStrengths, transitionBandGaps, transitionBandPositions;
    juce::Label transitionFilterLabel;
    juce::TextButton transitionAdvanced {"Rehearsal & presets..."};
    juce::Component transitionAdvancedBody;
    bool transitionAdvancedOpen = false;
    juce::ComboBox transitionFilterMode, transitionFilterDepth;
    std::array<juce::TextButton, 4> transitionCurveShow, transitionCurveOnly;
    std::array<bool, 4> transitionCurveShown {{true, true, true, true}};
    int transitionCurveOnlyBand = -1;
    bool transitionCurveVisible(int band) const
    { return band >= 0 && band < 4 && transitionCurveShown[(size_t) band] && (transitionCurveOnlyBand < 0 || transitionCurveOnlyBand == band); }
    std::array<juce::TextButton, 3> storeAlternative, auditionAlternative, applyAlternative;
    juce::TextButton prepareRehearsal {"Prepare rehearsal"}, endRehearsal {"End rehearsal"};
    juce::TextButton saveTransitionPreset {"Save preset..."}, removeTransitionPreset {"Delete preset"}, importTransitionPreset {"Import preset..."}, exportTransitionPreset {"Export preset..."};
    juce::ComboBox savedTransitionPresets, rehearsalLeadIn, rehearsalLeadOut;
    juce::Label rehearsalContextLabel;
    void changeSelectedRehearsalContext();
    juce::Label alternativesLabel, savedPresetsLabel;
    void initialiseTransitionRehearsalUI();
    void layoutTransitionRehearsalUI(juce::Rectangle<int>&);
    void refreshTransitionRehearsalUI(bool editable);
    void storeSelectedAlternative(int);
    void applySelectedStyle(const TransitionStyle&);
    void prepareSelectedRehearsal(int slot = -1);
    void chooseTransitionPresetName();
    void chooseTransitionPresetFile(bool writing);
    void initialiseTransitionUI();
    void refreshTransitionUI();
    void layoutTransitionUI();
    void changeSelectedTransitionBand(int band);
    void changeSelectedTransitionFilter();
    bool transitionKeyPressed(const juce::KeyPress&);
    void addBarMenus(juce::PopupMenu&, bool moveTrack);
    bool performBarMenuAction(int);
    void jumpBars(int bars, int direction);
    void moveTrackBars(int bars, int direction);
    juce::String transitionKeySummary(const Project::TransitionLink&) const;
    void navigateTransition(int direction);
    void changeSelectedTransition(std::optional<double> bars, std::optional<TransitionPreset> preset, bool toggleLock = false);
    juce::Label clipDetail, sidebarHelp;
    int inspectorPage = 0;
    TimelineViewport viewport;
    bool programmaticTimelineView = false;
    int lastTimelineViewX = 0;
    juce::TextButton followButton { "Follow playhead" };
    void setFollowPlayhead(bool);
    void followPlayheadIntoView();
    juce::TextButton newButton { "New" }, openButton { "Open" }, saveButton { "Save" }, importButton { "+ Import audio" }, exportButton { "Export WAV" }, settingsButton { "Audio / MIDI" };
    juce::TextButton playButton { "Play" }, stopButton { "Stop" }, homeButton { "|<" }, undoButton { "Undo" }, redoButton { "Redo" }, splitButton { "Split" }, duplicateButton { "Duplicate" }, removeButton { "Remove" };
    juce::Slider zoomSlider;
    juce::TextButton autoArrangeButton {"Auto Arrange"};
    bool canAutoArrange() const;
    void showAutoArrangement();
    void startAutoArrangement(const AutoArrangementOptions&);
    friend void runAutoArrangementTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    juce::TextButton trackInfoButton { "BPM / Key" };
    juce::ToggleButton reverseButton { "Reverse" };
    juce::TextButton loopButton { "Loop..." };
    juce::ComboBox keyDisplay, transitionLength, transitionPreset;
    juce::Label laneAmount;
    TransitionPreset selectedTransitionPreset() const { return (TransitionPreset) std::max(0, transitionPreset.getSelectedId() - 1); }
    juce::TextButton beatmatchButton { "Beatmatch" };
    int transitionIndex = 5;
    double transitionOverlapBeats() const { constexpr double beats[] {0, 16, 32, 64, 128, 256}; return beats[transitionIndex]; }
    juce::ComboBox waveformMode;
    int selectedAutomationTarget = -1;
    AutomationAddress pluginLane { -1, AutomationTarget::plugin };
    juce::ToggleButton snapButton { "Snap to beat" };
    juce::Slider tempoSlider, masterSlider;
    std::unique_ptr<juce::Button> masterPanelButton;
    std::unique_ptr<juce::DocumentWindow> masterWindow;
    void initialiseMasteringUI();
    void showMastering();
    juce::ToggleButton monitorMute { "Mute" };
    juce::Label timeLabel, inspectorLabel, pluginLabel, masterLabel;
    juce::TextEditor search;
    juce::TextButton addPluginButton { "+ VST3" }, editPluginButton { "Open" }, bypassPluginButton { "Bypass" }, removePluginButton { "Remove" }, pluginControlsButton { "MIDI controls" };
    std::array<std::unique_ptr<TrackStrip>, trackCount> strips;
    std::array<AlloyDial, 6> clipSliders;
    std::array<juce::Label, 6> clipLabels;
    juce::MidiKeyboardComponent piano;
    juce::Label midiLabel;
    ImportProgressView importProgress;
    std::shared_ptr<std::atomic<bool>> importCancellation;
    std::shared_ptr<std::atomic<bool>> sourceReadsCancelled = std::make_shared<std::atomic<bool>>(false);
    struct ExportTask
    {
        std::unique_ptr<Engine> renderer;
        juce::File destination;
        WavExportOptions options;
        WavExportPlan plan;
        std::shared_ptr<std::atomic<bool>> cancelled = std::make_shared<std::atomic<bool>>(false);
        std::atomic<juce::int64> completed {0};
    };
    std::shared_ptr<ExportTask> exportTask; // Released on the message thread after its worker has let go.
    juce::File catalogueFile; // Empty in Workstation(false); tests supply an owned fixture explicitly.
    bool catalogueRestorePending = false, catalogueDirty = false, catalogueWritePending = false, catalogueRefreshPending = false;
    std::vector<std::shared_ptr<Media>> catalogueRefreshQueue;
    double catalogueChangedAt = 0;
    void tickLibraryCatalogue();
    void markCatalogueChanged();
    bool adoptLibraryRefresh(const std::shared_ptr<Media>& previous, const std::shared_ptr<Media>& verified);
    void writeLibraryCatalogue();
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::DialogWindow> settingsWindow;
    std::unique_ptr<juce::DialogWindow> gridWindow;
    juce::File sessionFile;
    std::vector<Project> undoHistory, redoHistory;
    std::vector<int> visibleMedia;
    int librarySortColumn = 0;
    bool librarySortForward = true;
    bool refreshing = false, busy = false, dirty = false;
    bool unsavedPromptOpen = false, choosingSave = false;
    struct TimelineSource { juce::File file; std::shared_ptr<Media> existing; };
    void loadTimelineSources(std::vector<TimelineSource>, std::optional<TimelinePlacement>);
    void insertTimelineMedia(const std::vector<std::shared_ptr<Media>>&, std::optional<TimelinePlacement>);
    void prepareTimelineAudio(const std::shared_ptr<Media>&);
    void finishTimelineAudio();
    void cancelTimelineAudio();
    std::map<juce::String, std::shared_ptr<std::atomic<bool>>> timelineLoads;
    juce::ThreadPool audioWorkers { 1 }; // Interactive loads must not wait behind idle scanning/snapshot writes.
    std::function<void()> afterTimelineAudio;
    double pendingTempo = 0, tempoChangedAt = 0;
    bool tempoPreparationPending = false;
    std::optional<Project> tempoEditBaseline;
    std::function<void()> afterTempoPreparation;
    juce::ThreadPool workers { 1 };
    bool musicScanEnabled = false, musicScanJobPending = false;
    bool musicScanManual = false;
    uint64_t musicScanGeneration = 0;
    juce::File musicFolder = juce::File::getSpecialLocation(juce::File::userMusicDirectory);
    juce::File musicPreference = scanPreferenceFile();
    double nextMusicScan = 0;
    int musicAdded = 0, musicRefreshed = 0, musicProblems = 0;
    std::shared_ptr<MusicScan> musicScan;
    std::map<juce::String, juce::String> musicAttempts;
    void beginMusicScan(bool manuallyRequested = false);
    void tickMusicScan();
    void stopMusicScan();
    void toggleMusicScan();
    friend void runExportTests(const std::function<void(bool, const juce::String&)>&, const juce::File&, const juce::File&);
    friend void runLatencyTests(const std::function<void(bool, const juce::String&)>&, const juce::File&, const juce::File&);
    friend void runLongNavigationTests(const std::function<void(bool, const juce::String&)>&, const juce::File&, const juce::StringArray&);
    friend void runLongSessionTests(const std::function<void(bool, const juce::String&)>&, const juce::File&, const juce::StringArray&);
    friend class Timeline;
    friend class TimelineHeader;
    friend void runGridCorrectionTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend class TempoStrip;
    friend void runPlacementTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runAlloyTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runBeatGridTests(const std::function<void(bool, const juce::String&)>&, const juce::File&, const juce::File&);
    friend void runRealTrackTests(const std::function<void(bool, const juce::String&)>&, const juce::File&, const juce::StringArray&);
    friend void runNativePickerTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runWindowTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runTimelineEditingTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend class LibraryTable;
    friend void runPlaylistTransitionTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    bool showEnergySections = true;
    friend class TrackStrip;
    friend class PluginList;
    friend void runTempoUiTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runFusionWorkflowTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runPlaybackCacheTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runTransitionBandTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runTransitionFilterTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runTransitionNavigationTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runRehearsalContextTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runAudioSlipTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runTransitionSwapTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runBarNavigationTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runTransitionRehearsalTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runTransitionCutTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runLibraryAssetTests(const std::function<void(bool, const juce::String&)>&, const juce::File&, const juce::File&, const juce::File&);
    void timerCallback() override;
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex(int, const juce::String&) override;
    void menuItemSelected(int, int) override;
    void showInspectorPage(int);
    void layoutTrackHeaders();
    void chooseImport();
    void chooseSave(std::function<void()> onSaved = {}, bool useSessionFile = false);
    bool saveSessionFile(const juce::File&);
    void chooseOpen();
    void chooseExport();
    void chooseExportEnd(const juce::File&);
    void configureEffectTail(std::function<void()> after = {});
    juce::Result applyEffectTail(double seconds);
    void beginWavExport(const juce::File&, WavExportOptions);
    void finishWavExport(const std::weak_ptr<ExportTask>&, const WavExportResult&);
    void choosePlaylistImport();
    void choosePlaylistExport(bool text);
    void editLabelMarker(juce::String id, double time);
    void removeLabelMarker();
    void navigateLabel(bool next);
    void choosePlugin();
    void configurePluginControls();
    void configureLoop();
    void settings();
    void filterLibrary();
    void setStatus(const juce::String&);
    void setBusy(bool);
    void editClip(int, double);
    void undo(bool redo);
    void splitClip();
    void duplicateClip();
    void removeClip();
    std::optional<Clip> clipClipboard;
    juce::String loopStartClip;
    double loopStartSource = 0;
    juce::PopupMenu timelineMenu(double time, int track);
    void showTimelineMenu(double time, int track);
    void performTimelineAction(TimelineAction, double time, int track);
    juce::PopupMenu automationMenu(int track) const;
    void addTimelineAutomation(AutomationTarget, double time, int track, bool addUndo = true);
    void newSession();
    void syncMix(bool align);
    void prepareTempo(Project, bool align, bool addUndo = true, size_t alignFrom = 0);
    void editTempoPoint(int point, double time);
    void scheduleTempoPreparation();
    bool ensureTempoReady(std::function<void()>);
    void editAnalysis();
    juce::Result applySourceCorrection(const std::shared_ptr<Media>&, const SourceCorrection&);
    void setDownbeatAnchor(const juce::String& clipId, double localTime);
    void saveAnalysisCorrection(const std::shared_ptr<Media>&);
    int automationLane() const { return selectedAutomationTarget; }
    void selectAutomationTarget(int target) { selectedAutomationTarget = target; refresh(); }
    std::vector<AutomationAddress> automationAddresses(int track) const;

    AutomationAddress displayedAddress(int track) const;
    AutomationCurve* displayedCurve(int track);
    juce::String automationText(AutomationTarget, float) const;
    juce::String displayedAutomationName() const;
    void chooseAutomationPlugin(std::optional<double> markerTime = {});
    void editAutomationPoint(int track, int point = -1);
    juce::String keyText(const Media&) const;
    void guardUnsaved(std::function<void()>);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Workstation)
};
}
