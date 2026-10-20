#pragma once
#include "Engine.h"
#include "AlloyLookAndFeel.h"

namespace mixworks
{
class GridCorrectionEditor final : public juce::Component, private juce::Timer
{
public:
    GridCorrectionEditor(Engine&, std::shared_ptr<const Media>, const juce::String& details,
                         std::function<juce::Result(const SourceCorrection&)> apply);
    ~GridCorrectionEditor() override;
    void setPlaybackMedia(std::shared_ptr<const Media>, const juce::String& error = {});
    void closePreview();
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
private:
    Engine& engine;
    std::shared_ptr<const Media> source, playback;
    std::function<juce::Result(const SourceCorrection&)> apply;
    TapTempo taps;
    SourceCorrection draft;
    juce::TextEditor info, bpm, phase;
    juce::ComboBox key, zoom;
    juce::Label heading, bpmLabel, phaseLabel, keyLabel, status;
    juce::TextButton half {"Half BPM"}, twice {"Double BPM"}, tap {"Tap 3+"}, resetTap {"Reset taps"}, beatOne {"Beat 1 here"},
        earlier {"-1 ms"}, later {"+1 ms"}, automatic {"Automatic"}, beginning {"Beginning"}, middle {"Middle"}, end {"End"}, quiet {"Quiet passage"},
        play {"Play source"}, save {"Apply correction"}, cancel {"Cancel"};
    juce::ToggleButton click {"Grid click"}, sourceAudio {"Source audio"};
    juce::Slider cursor, level;
    juce::Rectangle<int> waveform;
    double viewStart = 0, viewSpan = 12;
    bool validDraft = true, updating = false, closed = false;
    juce::Result readDraft();
    void changed();
    void setCursor(double, bool restart = true);
    void togglePlay();
    void changeRate(double);
    void nudge(double);
    void timerCallback() override;
    void dismiss();
    friend void runGridCorrectionTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
};
class GridCorrectionWindow final : public juce::DialogWindow
{
public:
    explicit GridCorrectionWindow(GridCorrectionEditor*);
    void closeButtonPressed() override;
};
}
