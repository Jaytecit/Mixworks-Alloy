#pragma once
#include <optional>
#include <set>
#include "Engine.h"
#include "PrivatePreparation.h"
#include "WorkspaceLayout.h"
#include "PerformanceDeck.h"
#include "ToolPanel.h"
#include "AlloyLookAndFeel.h"
#include "PeakMeter.h"
#include "MusicScanner.h"
#include "LibraryCatalogue.h"
#include "Recovery.h"
#include "EditSelection.h"
#include "StemJobs.h"
#include "LibraryPreview.h"

namespace mixworks
{
struct AutoArrangementOptions;
struct ArrangementInsertion;
class Workstation;
class TimelineAccessItem;
enum class TimelineAction
{
    cut = 200, copy, paste, remove, volume, treble, mid, bass, label, effect,
    loopStart, loopEnd, split, reverse, tweaks, settings, downbeat, tempo,
    curveRead, curvePoint, curveClear, choosePlugin, recordOff, recordTouch, recordLatch, armAutomation, pairBassSwap,
    filter, rippleDelete, closeGap, insertBefore, insertAfter, sectionBoundary, stems, repeatSelection, cropSelection, replaceSelection,
    placeVolumeSwap, placeBassSwap, placeMidSwap, placeTrebleSwap, unlinkSelection, addTransition, removePiece
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
    void refreshAccessibleMarkers();
    void resized() override { refreshAccessibleMarkers(); }
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;
private:
    friend void runTempoBrowserTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    Workstation& owner;
    bool draggingTempo = false, moved = false;
    TempoMap tempoDraft;
    std::vector<std::unique_ptr<TimelineAccessItem>> accessibleMarkers;
    void selectAccessibleMarker(int);
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
    void setReanalysis(bool focused = false) { heading.setText(focused ? "Focused BPM / key retry" : "Re-analysing tracks", juce::dontSendNotification); file.setText("Starting analysis...", juce::dontSendNotification); cancel.setButtonText("Stop analysis"); stopMessage = "Stopping analysis. Completed results and all manual corrections will be kept."; }
    void setRehearsal() { heading.setText("Preparing A / B / C rehearsal", juce::dontSendNotification); file.setText("Rendering saved transition alternatives", juce::dontSendNotification); stage.setText("Session effects / matched RMS", juce::dontSendNotification); cancel.setButtonText("Cancel rehearsal"); stopMessage = "Cancelling rehearsal. Your session will be kept."; }
};
class TimelineAccessItem;
class TimelineHeader final : public juce::Component, public juce::SettableTooltipClient
{
public:
    TimelineHeader(Workstation&, bool overview);
    void paint(juce::Graphics&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;
    juce::Rectangle<float> previewBounds() const;
    void refreshAccessibleMarkers();
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;
private:
    Workstation& owner;
    bool overview;
    std::vector<std::unique_ptr<TimelineAccessItem>> accessibleMarkers;
    float grabOffset = 0;
    int zoomEdge = 0;
    float zoomStartX = 0;
    double zoomStart = 0, zoomEnd = 0, zoomExtent = 0;
    double overviewWidth() const;
    int edgeAt(float x) const;
    void resizePreview(double begin, double end);
    void pan(float x);
    juce::MouseEvent timelineEvent(const juce::MouseEvent&) const;
};
class CurveToolButton final : public juce::TextButton
{
public:
    bool hasCurve = false;
    juce::Colour ink;
    void paintButton(juce::Graphics& g, bool over, bool down) override
    {
        juce::TextButton::paintButton(g, over, down);
        g.setColour(ink.withMultipliedAlpha(isEnabled() ? 1.f : .4f));
        const juce::Rectangle<float> dot {4.f, getHeight() * .5f - 2, 4, 4};
        if (hasCurve) g.fillEllipse(dot); else g.drawEllipse(dot, .8f);
    }
};
struct ClipCurveTools final : public juce::Component
{
    std::array<CurveToolButton, 5> buttons;
      juce::TextButton saveDownbeat {juce::String::fromUTF8("\xe2\x86\x93") + "1"};
    juce::TextButton fx {"FX..."};
    juce::TextButton hotcues {"HC"}, sections {"S"};
    juce::ComboBox selector;
};
class TimelineAccessItem final : public juce::Component
{
public:
    explicit TimelineAccessItem(bool valueObject = false);
    std::function<juce::String()> read;
    std::function<bool()> selected;
    std::function<void()> choose;
    std::function<void()> open;
    std::function<double()> value;
    std::function<juce::String()> valueText;
    const bool valueObject;
    void activate();
    void invoke();
    void focusGained(FocusChangeType) override;
    void focusLost(FocusChangeType) override { repaint(); }
    bool keyPressed(const juce::KeyPress&) override;
    void paint(juce::Graphics&) override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;
};
class Timeline final : public juce::Component, public juce::DragAndDropTarget, public juce::SettableTooltipClient
{
public:
    explicit Timeline(Workstation&);
    bool keyPressed(const juce::KeyPress&) override;
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
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;
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
    std::set<juce::String> visibleHotcueClips, visibleSectionClips;
    std::vector<std::unique_ptr<TimelineAccessItem>> accessibleItems;
    void refreshAccessibleItems();
    struct SectionFlag { juce::String clipId, key; int index = -1; double source = 0, time = 0; bool manual = false; juce::Rectangle<float> bounds; juce::String label; };
    std::optional<std::pair<juce::String, int>> selectedSection;
    struct SectionGesture
    {
        SectionFlag flag; float x = 0; bool changed = false, wasDirty = false, hadSourceEdit = false;
        size_t undoSize = 0; SourceSections original, expectedTemplate, originalSourceEdit;
    };
    std::optional<SectionGesture> sectionGesture;
    std::vector<SectionFlag> sectionFlags(bool includeOffscreen = false) const;
    std::optional<SectionFlag> sectionFlagAt(juce::Point<float>) const;
    void paintSectionFlags(juce::Graphics&);
    void selectSectionFlag(const SectionFlag&);
    bool beginSectionFlag(const juce::MouseEvent&);
    bool continueSectionFlag(const juce::MouseEvent&);
    bool sectionKeyPressed(const juce::KeyPress&);
    bool setSectionFlag(const SectionFlag&, double source, bool newUndo, std::optional<juce::String> label = {});
    void addSectionBoundary(const juce::String& clipId, double time);
    void finishSectionGesture(bool cancel = false);
    void resetSectionFlags(const juce::String&);
    void editSectionFlag(const SectionFlag&);
    void refreshAccessibleSections(const std::function<TimelineAccessItem&(const juce::String&, juce::Rectangle<int>, bool)>&);
    struct CueFlag { juce::String clipId, id; int slot = -1; double source = 0, time = 0; bool available = false; juce::Rectangle<float> bounds; };
    std::optional<CueFlag> selectedHotcue;
    struct CueGesture { CueFlag flag; float x = 0; bool changed = false; };
    std::optional<CueGesture> hotcueGesture;
    std::vector<CueFlag> hotcueFlags(bool includeOffscreen = false) const;
    std::optional<CueFlag> hotcueAt(juce::Point<float>) const;
    void paintHotcues(juce::Graphics&);
    void selectHotcue(const CueFlag&);
    bool beginHotcue(const juce::MouseEvent&);
    bool continueHotcue(const juce::MouseEvent&);
    bool hotcueKeyPressed(const juce::KeyPress&);
    void refreshAccessibleHotcues(const std::function<TimelineAccessItem&(const juce::String&, juce::Rectangle<int>, bool)>&);
    std::unique_ptr<ClipCurveTools> curveTools;
    int curveToolsReserve(int track) const;
    juce::Rectangle<int> clipFooterBounds(const Clip&) const;
    juce::Rectangle<int> curveToolsBounds() const;
    void initialiseCurveTools();
    void refreshCurveTools();
    void chooseCurveTool(int index);
    int resizingLane = -1, resizeStartHeight = 0;
    int dividerAt(float y) const;
    friend class TimelineHeader;
    std::optional<TimelinePlacement> dropPreview;
    std::optional<TimelinePlacement> placementAt(juce::Point<int>) const;
    void previewDrop(juce::Point<int>);
    void clearDropPreview();
    void paintDropPreview(juce::Graphics&);
    friend class Workstation;
    friend void runEditSelectionTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runUnlinkTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    bool beginEditSelection(const juce::MouseEvent&);
    bool continueEditSelection(const juce::MouseEvent&);
    void finishEditSelection();
    void paintEditSelection(juce::Graphics&);
    std::optional<double> rangeDragAnchor;
    EditSelection rangeDragBefore;
    struct GroupMoveGesture
    {
        Project baseline;
        EditSelection selection;
        juce::String anchor, document;
        juce::Point<float> origin;
        std::optional<Project> preview;
        double target = 0;
        int track = 0;
        bool snapping = false;
    };
    std::optional<GroupMoveGesture> groupMove;
    bool beginGroupMove(const juce::MouseEvent&, const juce::String&);
    bool continueGroupMove(const juce::MouseEvent&);
    void finishGroupMove();
    bool groupMoveAllowed() const;
    friend void runTimelineEditingTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runStemTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runRippleTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runSectionFlagTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runSourceSectionTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runHotcueTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runLaunchTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runPresetNodeTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runPrerollTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runTransitionBandTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runTransitionFilterTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runTransitionNavigationTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runZoomHandleTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
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
    struct AudioSlipGesture { Clip baseline; float x = 0; bool audioOnly = false; double lastStart = 0, lastLength = 0; };
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
    bool automationCanExceedUnity = false;
    struct CurveBendGesture { AutomationAddress address; int segment = -1; juce::String incoming; int side = 0; double bend = 0; float y = 0; bool rising = true; };
    std::optional<CurveBendGesture> curveBendDrag;
    struct CutAmountHandle
    {
        AutomationAddress address;
        juce::String incoming;
        int segment = -1, node = -1;
        bool before = false, legacy = false;
        double time = 0;
        float value = 0; // Remaining linear gain, or the parameter's own normalised value.
        juce::Rectangle<float> bounds;
        bool sameOwner(const CutAmountHandle& h) const
        { return address == h.address && incoming == h.incoming && segment == h.segment && node == h.node && before == h.before && legacy == h.legacy && time == h.time; }
    };
    struct CutAmountGesture { CutAmountHandle handle; float y = 0, expected = 0; };
    std::optional<CutAmountGesture> cutAmountDrag;
    std::optional<CutAmountHandle> selectedCutAmount;
    std::vector<CutAmountHandle> cutAmountHandles(bool includeOffscreen = false) const;
    std::optional<CutAmountHandle> cutAmountAt(juce::Point<float>) const;
    juce::String cutAmountText(const CutAmountHandle&) const;
    void paintCutAmounts(juce::Graphics&);
    bool beginCutAmount(const juce::MouseEvent&);
    bool continueCutAmount(const juce::MouseEvent&);
    bool setCutAmount(const CutAmountHandle&, float value, bool newUndo);
    void editCutAmount(const CutAmountHandle&);
    void beginCurveBend(const AutomationAddress&, juce::Point<float>);
    bool continueCurveBend(const juce::MouseEvent&);
    std::optional<std::pair<AutomationAddress, int>> editingAutomationPoint;
    void selectAutomationCurve(const AutomationAddress&);
    bool automationVisible(const AutomationAddress&) const;
    bool automationNodesVisible(const AutomationAddress&) const;
    std::vector<AutomationAddress> visibleAutomationAddresses(int track) const;
    AutomationCurve automationViewCurve(const AutomationAddress&) const;
    juce::Path automationPath(const AutomationAddress&, double begin, double end) const;
    std::optional<AutomationAddress> automationCurveAt(juce::Point<float>) const;
    juce::Rectangle<float> automationCaptionBounds() const;
    void paintStems(juce::Graphics&, const Clip&);
    bool stemMouseDown(const juce::MouseEvent&);
    void paintAutomationCaption(juce::Graphics&);
    juce::String draggingLabel;
    int labelAt(juce::Point<float>) const;
    std::optional<std::pair<juce::String, bool>> anchorAt(juce::Point<float>) const;
    juce::Rectangle<float> offscreenAnchorBounds(const Clip&, bool intro) const;
    std::optional<std::pair<AutomationAddress, int>> automationPointAt(juce::Point<float>) const;
    friend void runTempoUiTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runFusionWorkflowTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runBeatGridTests(const std::function<void(bool, const juce::String&)>&, const juce::File&, const juce::File&);
    float automationY(int track, float db, int target = -1) const;
    float automationDb(int track, float y, AutomationTarget) const;
    bool automationUsesGain(AutomationTarget) const;
    float automationNodeMaximum(AutomationTarget) const;
    void paintAutomationNodes(juce::Graphics&);
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
    juce::Component* accessibleCell(int, int, bool, juce::Component*);
    void cellClicked(int, int, const juce::MouseEvent&) override;
    juce::PopupMenu beatmatchMenu(int row);
    void cellDoubleClicked(int, int, const juce::MouseEvent&) override;
    void selectedRowsChanged(int) override;
    void sortOrderChanged(int, bool) override;
    juce::String getCellTooltip(int, int) override;
    juce::var getDragSourceDescription(const juce::SparseSet<int>&) override;
    bool mayDragToExternalWindows() const override { return false; }
    class AccessibleTable final : public juce::TableListBox
    {
    public:
        std::function<juce::String(int)> rowName;
        juce::String getNameForRow(int row) override { return rowName ? rowName(row) : juce::String(); }
    };
    AccessibleTable table;
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
    AlloyDial pan, cutoff;
    friend void runAlloyTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
};
class Workstation final : public juce::Component, public juce::DragAndDropContainer, public juce::FileDragAndDropTarget, private juce::Timer, private juce::MenuBarModel
{
    friend class PromoControl;
    friend void runStemIntegration(const std::function<void(bool, const juce::String&)>&, const juce::File&, const juce::StringArray&);
public:
    explicit Workstation(bool openAudioDevice = true);
    ~Workstation() override;
    Engine engine;
    LibraryPreview libraryPreview {engine};
    PrivatePreparation privatePreparation {engine};
    juce::TextButton workModeButton {"Arrange"}, takeButton {"Takes / preview"};
    void initialiseWorkModes();
    void showTakeMenu();
    void editTakeEvent(int take,int event);
    void showPrivatePreparationMenu();
    int selectedExportTake = -1;
    friend void runLibraryPreviewTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runPerformanceTakeTests(const std::function<void(bool,const juce::String&)>&,const juce::File&);
    friend void runZoomHandleTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    void previewLibraryTrack(const juce::String&);
    friend void runLiveHotcueTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
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
    class FeedbackText final : public juce::Component
    {
    public:
        std::function<juce::String()> read;
        std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;
    };
    FeedbackText statusAccess;
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
    void splitStems(const juce::String& sourceId, bool cpu = false);
    std::vector<std::shared_ptr<Media>> analysisTargets(int scope) const; // 0 selected, 1 needs review, 2 all.
    void reanalyseTracks(int scope, bool focused = false, std::vector<std::shared_ptr<Media>> targets = {});
    static bool canRetryAnalysis(const Media&);
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
    std::shared_ptr<StemJob> stemJob;
    juce::String stemSourceName, stemSourceId;
    void attachStems(const juce::String& sourceId, const juce::File&);
    void showStemMenu(const juce::String& clipId, int stem, double time);
    void toggleStemSolo(const juce::String& clipId, int stem);
    std::set<juce::String> pendingStemDiscovery;
    bool restoreTimelineStems();
    friend void runStemTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    int selectedStem = -1;
    bool consoleCollapsed = true;
    void toggleConsole(int page);

    AlloyLookAndFeel look;
    AlloyHints hints;
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
    std::unique_ptr<juce::DocumentWindow> performanceWindow, transitionWindow, editingWindow;
    juce::TextButton controllerTab {"Controller"}, editingTab {"Editing"};
    juce::Component editingBody;
    PanelViewport editingViewport;
    ToolPanel editingPanel {editingViewport, "Editing"};
    void initialiseEditing();
    void layoutEditing();
    void restoreEditingWorkspace();
    void trackEditingWorkspace();
    juce::ComboBox automationOwner, automationParameter, automationMode;
    juce::Label automationHeading, automationState, automationNode;
    juce::TextButton automationAdd {"Add"}, automationPrevious {"<"}, automationNext {">"},
        automationEdit {"Edit..."}, automationDelete {"Delete"}, automationRead {"Curve on"},
        automationArm {"Arm lane"}, automationPairedFilter {"Paired filter..."};
    std::vector<AutomationAddress> automationOwnerChoices, automationParameterChoices;
    void initialiseAutomationEditor();
    void refreshAutomationEditor(bool rebuildChoices = true);
    bool automationEditable(const AutomationAddress&) const;
    juce::AudioProcessorParameter* automationPluginParameter(const AutomationAddress&) const;
    AutomationCurve* materialiseAutomation(Project&, const AutomationAddress&);
    bool insertAutomationNode(const AutomationAddress&, double time, bool snap = true);
    bool deleteAutomationNode(const AutomationAddress&, int index);
    void showAutomationNodeMenu(const AutomationAddress&, int index, double time);
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
    juce::Label musicScanNotice;
    int browserPage = 0;
    juce::File workspacePreference;
    bool workspaceDirty = false;
    double workspaceChangedAt = 0;
    void showBrowserPage(int);
    void resizeSidePanel(bool left, int width);
    void resizeTimeline(int height);
    double displayedTempo() const;
    void saveWorkspacePreferences();
    NamedWorkspace captureNamedWorkspace(const juce::String&) const;
    juce::Result restoreNamedWorkspace(const juce::File&);
    void chooseWorkspaceLayout();
    void chooseConsoleOrder();
    void changePreroll(int bars);
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
                             std::shared_ptr<RecoveryStore> recovery = {}, juce::String fingerprint = {}, bool relinked = false);
    juce::MenuBarComponent menuBar {this};
    double nextControllerPoll = 0;
    void refreshControllerConnection();
    void showControllerConnection(const MidiConnectionStatus&);
    friend void runMidiConnectionTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    juce::TextButton displayButton {"Display"}, windowModeButton {"Windowed"}, minimiseButton {"_"}, closeAppButton {"X"};
    void toggleWindowMode();
    juce::Component trackHeaders;
    juce::TextButton keyboardButton {"Keyboard"}, instrumentSetupButton {"Audio / MIDI..."};
    juce::TextButton transitionTab { "Transitions" }, transitionLock { "Lock transition" };
    juce::TextButton addTransitionButton {"Add Transition"};
    juce::ToggleButton transitionMuteOutside {"Mute outside bounds"};
    void chooseAddTransition();
    void addSelectedTransition(const juce::String& outgoing, const juce::String& incoming);
    friend void runIndependentTimelineTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
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
    juce::String transitionNodeOptionsFor;
    std::array<TransitionBandSettings,4> transitionNodeOptions;
    TransitionFilterSettings transitionFilterNodeOptions;
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
    int inspectorPage = 1;
    TimelineViewport viewport;
    bool programmaticTimelineView = false;
    int lastTimelineViewX = 0;
    juce::TextButton followButton { "Follow playhead" };
    void setFollowPlayhead(bool);
    void followPlayheadIntoView();
    juce::TextButton newButton { "New" }, openButton { "Open" }, saveButton { "Save" }, importButton { "+ Import audio" }, exportButton { "Export WAV" }, settingsButton { "Audio / MIDI" };
    juce::TextButton playButton { "Play" }, stopButton { "Stop" }, homeButton { "|<" }, undoButton { "Undo" }, redoButton { "Redo" }, splitButton { "Split" }, duplicateButton { "Duplicate" }, removeButton { "Remove" };
    juce::Slider gridDimmer;
    juce::TextButton autoArrangeButton {"Auto Arrange"};
    bool canAutoArrange() const;
    void showAutoArrangement();
    void startAutoArrangement(const AutoArrangementOptions&);
    friend void runAutoArrangementTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    juce::TextButton trackInfoButton { "BPM / Key" };
    juce::ToggleButton reverseButton { "Reverse" };
    juce::TextButton loopButton { "Loop..." };
    juce::TextButton editingSelect {"Select segment..."}, editingCut {"Cut"}, editingCopy {"Copy"}, editingPaste {"Paste at playhead"};
    juce::TextButton editingRipple {"Ripple delete segment"}, editingCloseGap {"Close gap before segment"};
    juce::ToggleButton editingRange {"Range tool"};
    juce::TextButton editingRangeTime {"Exact range..."}, editingAll {"Select all"}, editingClear {"Clear selection"};
    juce::TextButton editingRepeat {"Repeat..."}, editingCrop {"Crop to range"}, editingReplace {"Replace..."};
    juce::TextButton editingUnlink {"Unlink"};
    juce::ComboBox editingRangeSize;
    juce::Label editingSelectionInfo;
    juce::ComboBox keyDisplay, transitionLength, transitionPreset;
    juce::TextButton effectsTab {"VST"}, shortcutsTab {"Shortcuts"}, allShortcuts {"All"};
    juce::TextEditor shortcutSearch, shortcutResults;
    juce::Label shortcutHeading;
    bool showingShortcuts = false, showAllShortcuts = false;
    void initialiseShortcuts();
    void refreshShortcuts();
    void showShortcuts(bool);
    void tickMasterEmblem();
    TransitionPreset selectedTransitionPreset() const { return (TransitionPreset) std::max(0, transitionPreset.getSelectedId() - 1); }
    juce::TextButton beatmatchButton { "Auto Arrange timeline" };
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
    juce::String librarySortReferenceId;
    bool refreshing = false, busy = false, dirty = false;
    juce::String lastStatusAccessText;
    double lastStatusAccessAt = 0;
    void refreshStatusAccess();
    bool unsavedPromptOpen = false, choosingSave = false;
    struct TimelineSource { juce::File file; std::shared_ptr<Media> existing; };
    void loadTimelineSources(std::vector<TimelineSource>, std::optional<TimelinePlacement>);
    void insertTimelineMedia(const std::vector<std::shared_ptr<Media>>&, std::optional<TimelinePlacement>);
    bool canInsertArrangement() const;
    void chooseArrangementInsertion(const juce::String& anchor, bool after);
    void prepareChosenInsertion(std::shared_ptr<Media>, const juce::String& anchor, bool after);
    void previewArrangementInsertion(ArrangementInsertion, const juce::String& baseline, std::shared_ptr<Media>);
    bool commitArrangementInsertion(ArrangementInsertion, const juce::String& baseline, std::shared_ptr<Media>);
    bool changeHotcue(const juce::String&, int slot, std::optional<double> source, bool newUndo, bool seed = false);
    void editHotcue(const juce::String&, int slot);
    void locateHotcue(int slot);
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
    int musicAdded = 0, musicRefreshed = 0, musicProblems = 0, musicFailures = 0;
    juce::File musicReport;
    juce::String musicLastIssue;
    juce::String musicScanSummary;
    std::shared_ptr<MusicScan> musicScan;
    std::map<juce::String, juce::String> musicAttempts;
    void beginMusicScan(bool manuallyRequested = false);
    void tickMusicScan();
    void stopMusicScan(bool completed = false);
    void refreshMusicScanStatus();
    void toggleMusicScan();
    friend void runExportTests(const std::function<void(bool, const juce::String&)>&, const juce::File&, const juce::File&);
    friend void runLatencyTests(const std::function<void(bool, const juce::String&)>&, const juce::File&, const juce::File&);
    friend void runLongNavigationTests(const std::function<void(bool, const juce::String&)>&, const juce::File&, const juce::StringArray&);
    friend void runLongSessionTests(const std::function<void(bool, const juce::String&)>&, const juce::File&, const juce::StringArray&);
    friend class Timeline;
    std::map<juce::String, SourceSections> sourceSectionUndoBaselines;
    void rememberSourceSectionBaseline(const std::shared_ptr<Media>&, const SourceSections&);
    bool persistSourceSectionDefaults(const std::shared_ptr<Media>&, const SourceSections&);
    void publishSourceSectionDefaults(const std::shared_ptr<Media>&, const SourceSections&);
    bool persistSourceSectionHistory(const Project&);
    void editSavedDownbeat(const juce::String&);
    friend class TimelineHeader;
    friend void runGridCorrectionTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend class TempoStrip;
    friend void runPlacementTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runAlloyTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runBeatGridTests(const std::function<void(bool, const juce::String&)>&, const juce::File&, const juce::File&);
    friend void runRealTrackTests(const std::function<void(bool, const juce::String&)>&, const juce::File&, const juce::StringArray&);
    friend void runNativePickerTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runSectionFlagTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runSourceSectionTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runArrangementInsertionTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runHotcueTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runLaunchTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runWindowTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runPresetNodeTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runTimelineEditingTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runRippleTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runPrerollTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
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
    void chooseExport(int take = -1);
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
    double pasteEnd = -1; // Exact continuation cursor; first paste still follows Snap.
    EditSelection editSelection;
    std::optional<EditFragment> editClipboard;
    uint64_t editClipboardRevision = 0;
    void clearEditSelection();
    void refreshEditSelection();
    void selectAllSegments();
    void setEditRange(double begin, double end);
    void editRangeTime();
    bool performSelectionAction(TimelineAction, double time, int track, int repeatCopies = 0);
    void replaceSelectedAudio(bool commit = false);
    friend void runEditSelectionTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runUnlinkTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    juce::String loopStartClip;
    double loopStartSource = 0;
    juce::PopupMenu timelineMenu(double time, int track);
    void showTimelineMenu(double time, int track);
    void performTimelineAction(TimelineAction, double time, int track);
    juce::PopupMenu automationMenu(int track) const;
    void addTimelineAutomation(AutomationTarget, double time, int track);
    void newSession();
    void syncMix(bool align);
    void prepareTempo(Project, bool align, bool addUndo = true, size_t alignFrom = 0, bool ordered = false);
    void editTempoPoint(int point, double time);
    void scheduleTempoPreparation();
    bool ensureTempoReady(std::function<void()>);
    bool correctionFromLibrary = true; // Last active source-selection surface, outside musical state.
    juce::String analysisSelectionId();
    void editAnalysis(std::optional<juce::String> sourceId = {});
    juce::Result applySourceCorrection(const std::shared_ptr<Media>&, const SourceCorrection&, bool rebuildTimeline = false);
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
    void editAutomationPoint(int track, int point = -1, std::optional<double> time = {});
    juce::String keyText(const Media&) const;
    static int keyEvidenceLevel(const Media&); // 0 = manual; 1..4 = detector evidence bands.
    static juce::String keyEvidenceText(const Media&);
    static juce::String keyEvidenceHint(const Media&);
    void guardUnsaved(std::function<void()>);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Workstation)
};
}
