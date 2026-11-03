#pragma once
#include "Engine.h"
#include "LibraryPreview.h"
#include "AlloyLookAndFeel.h"

namespace mixworks
{
// A listening choice is deliberate evidence, never an automatic key verdict.
struct EarKeyChoice
{
    int winner = 0, next = 1, stage = 0, selected = -1;
    std::array<int, 12> order {};
    explicit EarKeyChoice(int suggestion = -1);
    int candidate(int sound) const;
    void prefer(int sound);
};
class EarLoopPlayer final : public juce::AudioSource
{
public:
    void source(std::shared_ptr<const Media>, double begin, double end);
    void pcm(std::shared_ptr<const juce::AudioBuffer<float>>, double sampleRate);
    void reference(int key, bool chord, int octave, float db);
    void hear(int mode); // 0 music, 1 reference, 2 together.
    void level(float db);
    void prepareToPlay(int, double) override;
    void releaseResources() override {}
    void getNextAudioBlock(const juce::AudioSourceChannelInfo&) override;
private:
    juce::CriticalSection mutex;
    std::shared_ptr<const Media> media;
    std::shared_ptr<const juce::AudioBuffer<float>> samples;
    double begin = 0, end = 0, cursor = 0, sourceRate = 44100, outputRate = 44100;
    int key = 0, octave = 3, mode = 2;
    bool chord = false;
    float gain = .25f, referenceGain = .12f;
};
struct HarmonyAudio
{
    std::array<std::shared_ptr<juce::AudioBuffer<float>>, 3> hear; // reference, adjusted, together.
    double sampleRate = 44100, pitch = 0;
};
juce::Result renderHarmony(Engine&, const juce::String& reference, const juce::String& adjusted,
    juce::Range<double>, double pitch, const std::atomic<bool>& cancel, HarmonyAudio&);
void matchHarmonyLevel(HarmonyAudio&, const HarmonyAudio&);
juce::Result commitHarmony(Engine&, const juce::String& baseline, const Clip&, const std::function<void()>& remember);

class EarPreviewControls final : public juce::Component
{
public:
    EarPreviewControls(Engine&, LibraryPreview&, EarLoopPlayer&);
    ~EarPreviewControls() override;
    void stop();
    void resized() override;
    std::function<void(const juce::String&)> feedback;
    juce::TextButton play {"Play / Stop"}, output {"Choose headphones..."};
private:
    Engine& engine;
    LibraryPreview& route;
    EarLoopPlayer& player;
    WorkMode mode;
    bool ownsRouting = false;
    juce::String id = "ear-" + juce::Uuid().toString();
    juce::ComboBox destination;
    juce::Label description;
    juce::Slider volume;
};
class EarPassage final : public juce::Component
{
public:
    explicit EarPassage(std::shared_ptr<const Media>);
    std::shared_ptr<const Media> media;
    juce::Range<double> range;
    std::function<void()> onChange;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
private:
    double down = 0;
};
class KeyEarEditor final : public juce::Component
{
public:
    KeyEarEditor(Engine&, LibraryPreview&, std::shared_ptr<const Media>, double at,
        std::function<juce::Result(int)>);
    ~KeyEarEditor() override;
    void resized() override;
    void paint(juce::Graphics&) override;
    void close();
private:
    EarLoopPlayer player;
    EarPreviewControls preview;
    EarPassage passage;
    EarKeyChoice choice;
    std::vector<EarKeyChoice> history;
    std::function<juce::Result(int)> saveKey;
    juce::Label heading, instructions, result, status;
    juce::TextButton sound1 {"Sound 1"}, sound2 {"Sound 2"}, prefer1 {"1 fits better"}, prefer2 {"2 fits better"},
        unsure {"Unsure"}, other {"Try other sounds"}, useHome {"Use home sound"}, back {"Back"}, restart {"Start again"},
        previous {"Previous passage"}, next {"Next passage"}, save {"Save source key"}, cancel {"Cancel"};
    juce::ToggleButton details {"Show musical names"};
    juce::ComboBox hear, registerChoice;
    juce::Slider begin, end, referenceLevel;
    int sound = 1;
    bool updating = false;
    void refresh();
    void audition(int);
    void prefer(int);
    void setPassage(double, double);
    friend void runEarToolTests(const std::function<void(bool,const juce::String&)>&, const juce::File&);
};
class HarmonyEarEditor final : public juce::Component
{
public:
    HarmonyEarEditor(Engine&, LibraryPreview&, juce::ThreadPool&, const juce::String& clip,
        std::function<juce::Result(const juce::String&, const Clip&)>);
    ~HarmonyEarEditor() override;
    void resized() override;
    void paint(juce::Graphics&) override;
    void close();
private:
    Engine& engine;
    juce::ThreadPool& workers;
    EarLoopPlayer player;
    EarPreviewControls preview;
    Project baseline;
    juce::String fingerprint;
    std::function<juce::Result(const juce::String&, const Clip&)> apply;
    std::shared_ptr<std::atomic<bool>> cancellation;
    uint64_t generation = 0;
    std::shared_ptr<HarmonyAudio> original, kept, trial;
    double keptPitch = 0;
    bool pending = false, applying = false, closed = false, updating = false;
    juce::Label heading, instructions, result, status, referenceLabel, adjustedLabel, loopLabel;
    juce::ComboBox reference, adjusted, hear;
    juce::Slider begin, end, pitch, cents;
    juce::ToggleButton fine {"Fine tuning"};
    juce::TextButton lower {"Lower"}, higher {"Higher"}, originalButton {"Original"}, keptButton {"Kept version"}, trialButton {"Trial"},
        keep {"Keep this version"}, prepare {"Prepare loop"}, applyButton {"Apply to this segment"}, cancel {"Cancel"};
    int version = 2;
    void changedPair();
    void request();
    void listen(int);
    void refresh();
    void applyPitch();
    double trialPitch() const { return pitch.getValue() + cents.getValue() / 100; }
    std::shared_ptr<HarmonyAudio> selectedAudio() const;
    friend void runEarToolTests(const std::function<void(bool,const juce::String&)>&, const juce::File&);
};
class EarToolWindow final : public AlloyPopupWindow<juce::DialogWindow>
{
public:
    explicit EarToolWindow(juce::Component*, const juce::String& title);
    void closeButtonPressed() override;
};
}
