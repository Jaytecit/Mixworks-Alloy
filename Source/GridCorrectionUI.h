#pragma once
#include "Engine.h"
#include "AlloyLookAndFeel.h"
#include "RetryReview.h"
#include <array>

namespace mixworks
{
class GridCorrectionEditor final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    GridCorrectionEditor(Engine&, std::shared_ptr<const Media>, const juce::String& details,
                         std::function<juce::Result(const SourceCorrection&, bool)> apply);
    ~GridCorrectionEditor() override;
    void setPlaybackMedia(std::shared_ptr<const Media>, const juce::String& error = {});
    void closePreview();
    void setKeyByEarAction(std::function<void(std::shared_ptr<const Media>)>);
    void setReviewedKey(int);
    void pauseAudition();
    std::function<juce::Result()> beforeAudition;
    std::function<void()> refreshFailures;
    void setRetryFailures(const std::vector<RetryIssue>&, const juce::String&, std::function<void(juce::String)>);
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
private:
    Engine& engine;
    std::shared_ptr<const Media> source, playback;
    std::function<juce::Result(const SourceCorrection&, bool)> apply;
    TapTempo taps;
    SourceCorrection draft;
    juce::TextEditor info, bpm, phase;
    juce::ComboBox key, zoom, precision;
    juce::Label heading, instructions, effectiveTempo, bpmLabel, phaseLabel, keyLabel, status, anchorInfo, barsLabel, applyNotice;
    juce::TextButton half {"Half BPM"}, twice {"Double BPM"}, tap {"Tap 3+"}, resetTap {"Reset taps"}, beatOne {"Beat 1 here"},
        earlier {"-1 ms"}, later {"+1 ms"}, automatic {"Automatic"}, beginning {"Beginning"}, middle {"Middle"}, end {"End"}, quiet {"Quiet passage"},
        play {"Play source"}, save {"Save source only"}, rebuild {"Apply to arrangement"}, cancel {"Cancel"}, slower {"-0.01 BPM"}, faster {"+0.01 BPM"},
        moveGrid {"Move grid"}, spacing {"Adjust spacing"}, narrower {"Narrower"}, wider {"Wider"}, goA {"Go to A"}, goB {"Go to B"}, matchB {"Set B at cursor"},
        undoGrid {"Undo grid"}, redoGrid {"Redo grid"}, revertGrid {"Revert grid"};
    juce::TextButton keyByEar {"Key by ear..."};
    juce::TextButton analysisDetails {"Analysis details..."};
    juce::TextButton tempoPage {"1. Tempo"}, beatPage {"2. Beat one"}, checkPage {"3. Check drift"}, advancedPage {"Advanced"};
    juce::TextButton simplePage {"Simple: A / B / C"};
    std::array<juce::TextButton, 3> setPoint, visitPoint;
    std::array<juce::Label, 3> pointInfo;
    juce::Slider barsAB, barsBC;
    juce::Label countAB, countBC, simpleResult;
    juce::ToggleButton countsConfirmed {"I have checked both bar counts"};
    std::array<double, 3> points {{-1, -1, -1}};
    void setPointAtCursor(int);
    void measurePoints();
    void updateSimpleControls();
    bool simpleAligned() const;
    juce::TextButton barEarlier {"Beat 1 back"}, barLater {"Beat 1 forward"};
    int page = 0;
    void selectPage(int);
    void shiftBar(int beats);
    juce::ComboBox failures;
    juce::Label lastRetry;
    std::vector<RetryIssue> failureRows;
    std::function<void(std::shared_ptr<const Media>)> openKey;
    juce::ToggleButton click {"Grid click"}, sourceAudio {"Source audio"};
    juce::Slider cursor, level, bars;
    juce::Rectangle<int> waveform;
    double viewStart = 0, viewSpan = 12;
    bool validDraft = true, updating = false, closed = false;
    bool waveformDetailPending = false;
    struct GridEdit
    {
        juce::String bpm, phase;
        int bars = 16;
        bool calculated = false;
        std::array<double, 3> points {{-1, -1, -1}};
        int barsAB = 16, barsBC = 16;
        bool confirmed = false;
        bool operator==(const GridEdit& other) const
        { return bpm == other.bpm && phase == other.phase && bars == other.bars && calculated == other.calculated
            && points == other.points && barsAB == other.barsAB && barsBC == other.barsBC && confirmed == other.confirmed; }
    };
    GridEdit initialGrid, lastGrid, dragBefore;
    std::vector<GridEdit> gridUndo, gridRedo;
    bool historyReady = false, restoring = false, calculated = false;
    int dragging = 0, selectedHandle = 0; // 1: A; 2: B; 3: pending click; 4: waveform alignment; 5: simple source navigation.
    double dragStartX = 0, dragLastX = 0, dragDelta = 0, dragViewStart = 0, dragAnchor = 0, dragB = 0, dragSecondsPerPixel = 0;
    juce::Result readDraft();
    void changed();
    void setCursor(double, bool restart = true);
    void togglePlay();
    void changeRate(double);
    void adjustTempo(double);
    void commit(bool rebuildTimeline);
    void nudge(double);
    double secondAnchor() const;
    double precisionSeconds() const;
    double timeAtX(float) const;
    float xAtTime(double) const;
    juce::Rectangle<float> handleBounds(double, int) const;
    GridEdit gridEdit() const;
    void restoreGrid(const GridEdit&);
    void undo(bool redo);
    void setAnchor();
    bool alignSecond(double);
    void changeBars();
    void updateAlignmentControls();
    void timerCallback() override;
    void dismiss();
    friend void runGridCorrectionTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runGridAlignmentTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runBeatGridTests(const std::function<void(bool, const juce::String&)>&, const juce::File&, const juce::File&);
};
class GridCorrectionWindow final : public AlloyPopupWindow<juce::DialogWindow>
{
public:
    explicit GridCorrectionWindow(GridCorrectionEditor*);
    void closeButtonPressed() override;
};
}
