#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Model.h"

namespace mixworks
{
struct WorkspaceLayout
{
    static constexpr double maximumTimelineZoom = 1600.;
    static constexpr int peakPanelWidth = 116, peakPanelAllocation = peakPanelWidth + 8;
    static constexpr int defaultLibraryWidth = 320, defaultInspectorWidth = 270, minimumPanelWidth = 240, minimumCentreWidth = 620 + peakPanelAllocation;
    static constexpr int tempoStripHeight = 22, timelineScrollbarThickness = 14;
    static constexpr int timeRulerHeight = 24, labelHeight = 20, laneHeight = 132;
    int preferredLibraryWidth = defaultLibraryWidth, preferredInspectorWidth = defaultInspectorWidth;
    bool libraryCollapsed = false, inspectorCollapsed = false;
    static constexpr int minimumLaneHeight = 100;
    int visibleLaneCount = trackCount;
    int defaultLaneHeight = laneHeight;
    int preferredTimelineHeight = 0, minimumTimelineHeight = 0, maximumTimelineHeight = 0;
    std::array<double, trackCount> laneScale {1, 1, 1, 1};
    int laneSize(int track) const { return fittedLanes[(size_t) track]; }
    int laneTop(int track) const { int y = 0; for (int i = 0; i < track; ++i) y += laneSize(i); return y; }
    bool monitorMuted = false;
    bool followPlayhead = false;
    bool popupHints = true;
    juce::String saveFolder; // Empty selects this user's Desktop/Mixworks; independent of mixes.
    int workMode = 0;
    juce::String privateDeviceType, privateDeviceName;
    int privateFirstChannel = -1;
    float privateLevel = .5f;
    int toolPage = 0;
    std::array<int, 3> consoleOrder {0, 1, 2}; // Stable command/page identities, visual order only.
    bool editingDetached = false;
    juce::Rectangle<int> editingWindowBounds;
    double gridBrightness = 0.45; // Visual preference, independent of musical state.
    bool forgeEffects = false;
    int forgeMotion = 0; // 0 follows Windows, 1 explicit animation, 2 steady.
    juce::Rectangle<int> brand, menu, windowControls, toolbar, transport, overview, ruler, library, playlist, headers, timeline, tempo, peakPanel, performance, master, detail, status, leftDivider, rightDivider, timelineDivider;
    static constexpr int visualLaneHeight = 0;
    juce::Rectangle<int> visuals, visualHeader;
    void update(int width, int height, bool performanceDocked = true, bool editingDocked = false);
    void resizePanel(bool left, int requestedWidth);
    void resizeTimeline(int height);
    void resizeLane(int track, int height);
private:
    int availableWidth = 0;
    std::array<int, trackCount> fittedLanes {laneHeight, laneHeight, laneHeight, laneHeight};
    void fitLanes();
};

juce::Result readWorkspacePreferences(const juce::File&, WorkspaceLayout&);
juce::Result writeWorkspacePreferences(const juce::File&, const WorkspaceLayout&);
struct NamedWorkspace
{
    juce::String name;
    WorkspaceLayout layout;
    int inspectorPage = 1, browserPage = 0;
    double zoom = 10;
    juce::Point<int> view;
    std::array<bool, 3> detached {};
    std::array<juce::Rectangle<int>, 3> windows;
};
juce::Result readNamedWorkspace(const juce::File&, NamedWorkspace&);
juce::Result writeNamedWorkspace(const juce::File&, const NamedWorkspace&);

class TimelineSplitter final : public juce::Slider
{
public:
    TimelineSplitter();
    std::function<void()> reset;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override {}
    bool keyPressed(const juce::KeyPress&) override;
private:
    bool dragging = false;
    int startY = 0;
    double startHeight = 0;
};

class PanelSplitter final : public juce::Slider
{
public:
    explicit PanelSplitter(bool left);
    std::function<void()> restore;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override {}
    bool keyPressed(const juce::KeyPress&) override;
private:
    bool isLeft, dragging = false;
    int startX = 0;
    double startValue = 0;
};
}
