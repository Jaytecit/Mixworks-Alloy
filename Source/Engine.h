#pragma once
#include "Model.h"
#include "Playlist.h"
#include "ElasticAudio.h"
#include "GridCorrection.h"
#include "WavExport.h"
#include "OutputPeaks.h"
#include "MidiConnection.h"
#include <juce_dsp/juce_dsp.h>
#include <map>
#include <set>

namespace mixworks
{
struct PluginSlot
{
    juce::String id = juce::Uuid().toString();
    juce::PluginDescription description;
    std::unique_ptr<juce::AudioPluginInstance> processor;
    bool bypass = false;
    int unsupportedLatency() const { return bypass ? 0 : std::max(0, processor->getLatencySamples()); }
    juce::String latencyError() const;
    std::map<juce::String, juce::AudioProcessorParameter*> parameters;
};
struct ToneEq
    {
        struct Band { std::array<double, 5> coefficients {1, 0, 0, 0, 0}; std::array<double, 2> z1 {}, z2 {}; };
        std::array<Band, 3> bands;
        std::array<float, 3> gains {};
        double rate = 0;
        void reset();
        void process(juce::AudioBuffer<float>&, int start, int frames, double, const std::array<float, 3>&);
    };
inline bool pluginChainHasInstrument(const std::vector<std::unique_ptr<PluginSlot>>& plugins)
{ return std::any_of(plugins.begin(), plugins.end(), [](const auto& slot) { return !slot->bypass && slot->description.isInstrument; }); }
struct TransitionFilterProcessor
{
    struct Stage
    {
        std::array<double, 2> low {}, band {};
        double lastCutoff = -1, g = 0, h = 1;
        bool highPass = false;
        void reset();
    };
    std::array<Stage, 2> stages;
    double rate = 0;
    void reset();
    void process(juce::AudioBuffer<float>&, int first, int last, double rate, double at, const std::array<TransitionFilterEnvelope, 2>&);
};
struct LowPassProcessor
{
    std::array<double, 2> low {}, band {};
    double rate = 0, cutoff = -1, g = 0, h = 1, smooth = 20000;
    void reset() { low = {}; band = {}; rate = 0; cutoff = -1; smooth = 20000; }
    void process(juce::AudioBuffer<float>&, int first, int frames, double rate, float hz);
};
struct ClipEffects
{
    std::vector<std::unique_ptr<PluginSlot>> plugins;
    bool hasInstrument() const
    { return pluginChainHasInstrument(plugins); }
};
enum class AutomationRecordMode { read, touch, write };
struct MidiBinding { int status = 0, channel = 0, number = 0; MidiAction action; };
struct PreparedProject { ProjectResult parsed; std::vector<std::shared_ptr<Media>> library; juce::String error; };
using ProjectProgress = std::function<void(int, int, const juce::String&, MediaReadStage)>;
class Engine : public juce::AudioIODeviceCallback, public juce::AudioPlayHead,
               private juce::MidiInputCallback, private juce::AsyncUpdater
{
public:
    explicit Engine(juce::File midiProfile = {}, bool loadMidiMappings = true);
    ~Engine() override;
    juce::AudioDeviceManager devices;
    juce::AudioFormatManager formats;
    juce::AudioPluginFormatManager pluginFormats;
    juce::MidiKeyboardState keyboard;
    juce::CriticalSection lock;
    GridAudition gridAudition; // Monitor-only source audition, protected by lock; never part of project/export.
    Project project;
    juce::String selectedClip;
    Clip* controlClip(int track);
    const Clip* controlClip(int track) const;
    TrackSettings& controlSettings(int track);
    const std::vector<std::unique_ptr<PluginSlot>>& pluginChain(int track) const;
    std::vector<std::unique_ptr<PluginSlot>>& pluginChain(int track);
    juce::String cloneClipEffects(Clip&);
    std::vector<std::shared_ptr<Media>> library;
    std::array<std::vector<std::unique_ptr<PluginSlot>>, trackCount> plugins;
    std::atomic<bool> playing { false }, exporting { false };
    std::atomic<double> position { 0 }, sampleRate { 44100 }, tempo { 120 };
    std::array<std::atomic<float>, trackCount> meters {};
    std::atomic<float> masterPeak { 0 };
    StereoPeakFeed outputPeaks; // Device output before monitor mute; offline rendering never publishes here.
    std::atomic<float> masterInputPeak { 0 }; // -1 when a pre-rendered/source preview has no bus-input measurement.
    std::atomic<bool> masterClipped { false };
    SpectrumFeed spectrum;
    std::atomic<bool> monitorMuted { false }; // Device output only; project renders and exports remain audible.
    std::atomic<int> selectedTrack { 0 }, midiNotes { 0 };
    // Surface routing is engine-owned so mouse and mapped MIDI use the same targets.
    std::array<int, 8> deskLanes {0, 1, 0, 1, 0, 1, 0, 1};
    MidiAction deskPotAction(int slot) const;
    AutomationAddress deskPotAddress(int slot) const;
    void cycleDeskLane(int slot);
    std::function<void(bool)> onControl;
    std::function<void()> onMidiChanged;
    std::function<void(MidiAction)> onPerformanceAction;
    std::function<void()> onProjectSwitchBegin; // Before an admitted Snap/mute/solo edit, on the message thread.
    std::function<void()> onAutomationWriteBegin;
    AutomationRecordMode automationRecordMode = AutomationRecordMode::read;
    std::array<bool, trackCount> automationArmed {};
    bool masterAutomationArmed = false;
    bool controlInputEnabled = true; // Message-thread workflows pause performance input during exclusive work.
    bool canWriteAutomation(int track) const { return playing && !exporting && !rehearsalActive() && automationRecordMode != AutomationRecordMode::read && (track == -1 ? masterAutomationArmed : track >= 0 && track < trackCount && automationArmed[(size_t) track]); }
    void setAutomationRecordMode(AutomationRecordMode);
    void armAutomation(int track, bool armed); // -1 is master.
    bool recordingAutomation() const { return !activeWrites.empty(); }
    void advanceAutomation(double sessionTime);
    void finishAutomation();
    void setPlaying(bool);
    bool playbackReady() const;
    bool sourceAudioReady() const;
    bool setControlValue(const AutomationAddress&, float value);
    bool ensurePluginAutomation(const AutomationAddress&);
    juce::Optional<AutomationAddress> pluginAddress(int track, int insert, int parameter) const;
    void removePlugin(int track, int insert);
    void restoreManualPluginValues();
    float automationValue(const AutomationAddress&, double time) const;
    juce::Optional<MidiAction> learnAction;
    std::vector<MidiBinding> mappings;
    juce::String midiLastMessage;

    juce::String startDevice();
    MidiConnectionStatus midiConnectionStatus();
    juce::Result correctSource(const std::shared_ptr<Media>&, const SourceCorrection&);
    void stop();
    void seek(double seconds);
    void resetPlugins();
    void prepareClipProcessing();
    juce::String addPlugin(int track, const juce::File&);
    juce::String instantiate(const juce::PluginDescription&, std::unique_ptr<PluginSlot>&);
    juce::var captureProject();
    juce::String restoreProject(const juce::String&);
    PreparedProject prepareProject(const juce::String&, const ProjectProgress& = {}, const std::shared_ptr<std::atomic<bool>>& cancelled = {});
    juce::String commitProject(PreparedProject);
    juce::String saveProject(const juce::File&);
    juce::String exportWav(const juce::File&);
    juce::String pluginLatencyError() const;
    std::atomic<bool> latencyBlocked {false};
    juce::Result planWavExport(const juce::File&, WavExportOptions, WavExportPlan&);
    std::unique_ptr<Engine> createExportRenderer(juce::String& error, const std::shared_ptr<std::atomic<bool>>& cancelled = {});
    WavExportResult writeWav(const juce::File&, WavExportOptions, const std::shared_ptr<std::atomic<bool>>& cancelled = {}, const WavExportProgress& = {});
    juce::String exportPlaylist(const juce::File&, PlaylistExport);
    // Locked caller; frames must fit the block prepared for this device/renderer.
    void render(juce::AudioBuffer<float>& output, int frames, double at, juce::MidiBuffer& midi, const Project* source = nullptr);
    juce::Result prepareTransitionRehearsal(const juce::String& incoming, const std::shared_ptr<std::atomic<bool>>& cancelled);
    juce::Result auditionTransitionAlternative(int slot);
    void endTransitionRehearsal();
    bool rehearsalActive() const { return rehearsalSlot >= 0; } // lock protects rehearsal state
    int transitionRehearsalSlot() const { return rehearsalSlot; }
    juce::String rehearsalSummary() const;
    void enableMidi(const juce::String& identifier, bool enabled);
    void saveMidiMappings();
    juce::Result reloadMidiMappings();
    void beginLearn(const juce::String&);
    void beginControllerSetup(bool liveDesk = false);
    void skipControllerStep();
    juce::String mappingSummary();
    juce::String lastMidiText();
    void silenceNotes();
    bool applyMidiControl(MidiAction, float normalizedValue);
    juce::String pluginControlSummary(int track);
    juce::String assignPluginControl(int track, int control, int insert, int parameter);
    juce::String midiSummary() const;
    void testMidi(const juce::MidiMessage& m) { handleIncomingMidiMessage(nullptr, m); }
    static void useTestMidiProfile(const juce::File&); // Self-test process only; keeps Learn out of the user's profile.
    void flushMidiControls() { handleUpdateNowIfNeeded(); }
    juce::Optional<juce::AudioPlayHead::PositionInfo> getPosition() const override;
    void audioDeviceAboutToStart(juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext(const float* const*, int, float* const*, int, int, const juce::AudioIODeviceCallbackContext&) override;
private:
    bool offlineRenderer = false;
    // Device transport owns elapsed integer frames; position remains the public seconds view.
    bool deviceClockValid = false;
    double deviceClockAnchor = 0, deviceClockExpected = 0, deviceClockRate = 0;
    juce::int64 deviceClockFrames = 0;
    bool hasUnsupportedLatency() const;
    bool processPlugin(PluginSlot&, juce::AudioBuffer<float>&, juce::MidiBuffer&);
    void stopForLatency(juce::AudioBuffer<float>&, int frames);
    ElasticRenderer elasticRenderer;
    std::array<ToneEq, trackCount> equalizers;
    std::array<LowPassProcessor, trackCount> lowPasses;
    ToneEq masterEqualizer;
    void processMaster(const Project&, juce::AudioBuffer<float>&, int frames, double at);
    struct ClipProcessing { ToneEq eq; TransitionFilterProcessor filter; LowPassProcessor lowPass; };
    std::map<std::weak_ptr<ClipEffects>, ClipProcessing, std::owner_less<std::weak_ptr<ClipEffects>>> clipProcessors;
    std::array<juce::AudioBuffer<float>, trackCount> trackBuffers;
    std::array<juce::MidiBuffer, trackCount> trackMidi;
    std::array<juce::MidiBuffer, trackCount> segmentMidi;
    std::array<juce::MidiBuffer, trackCount> processedMidi;
    juce::MidiBuffer callbackMidi, clipMidi, clipSegmentMidi, clipProcessedMidi;
    juce::AudioBuffer<float> mixBuffer, clipBuffer;
    bool processClipEffects(const Project&, const Clip&, int frames, double at, const juce::MidiBuffer& incoming);
    float automationValueIn(const Project&, const AutomationAddress&, double time) const;
    const Project* renderContext = nullptr; // borrowed only during a locked synchronous render
    struct RehearsalAudio { juce::AudioBuffer<float> audio; double rms = 0; float gain = 1; };
    std::array<RehearsalAudio, 3> rehearsalAudio;
    int rehearsalSlot = -1, rehearsalCursor = 0;
    double rehearsalBegin = 0, rehearsalRate = 0, rehearsalReturn = 0;
    void renderTransitionRehearsal(juce::AudioBuffer<float>&, int frames);
    void eachPlugin(const std::function<void(PluginSlot&)>&);
    juce::Result validateWriteDestination(const juce::File&) const; // Caller holds lock; one rule for all source references.
    juce::String clonePlugin(const PluginSlot&, std::unique_ptr<PluginSlot>&);
    juce::MidiMessageCollector midiCollector;
    juce::CriticalSection midiLock;
    juce::File mappingsFile;
    juce::String midiProfileError;
    bool midiCallbackRegistered = false;
    std::map<juce::String, double> midiInputActivity;
    std::set<int> pressedMidiButtons; // MIDI lock; edge-trigger mapped buttons, consume their releases.
    struct ControlEvent { MidiAction action; float value; double time; bool running; int targetTrack = -1; };
    std::vector<ControlEvent> pendingControls;
    bool applyControl(const ControlEvent&);
    bool setControlValueAt(const AutomationAddress&, float value, double time, bool running);
    juce::AudioProcessorParameter* parameterFor(const AutomationAddress&) const;
    struct ActiveWrite
    {
        AutomationAddress address;
        AutomationCurve original;
        float manualBefore = 0, value = 0;
        double lastTime = 0, lastInput = 0;
    };
    std::vector<ActiveWrite> activeWrites;
    bool automationTake = false;
    void writeUntil(ActiveWrite&, double time);
    void finishWrite(ActiveWrite&, double time, bool returnToCurve);
    std::atomic<double> pluginRenderTime {0};
    std::atomic<bool> renderingPlugin {false};
    bool mappingsDirty = false;
    struct LearnStep { MidiAction action; juce::String control; };
    std::vector<LearnStep> learnSteps;
    size_t learnStep = 0;
    bool replaceControllerProfile = false;
    std::set<int> learnedControls;
    int blockSize = 512;
    bool attached = false;
    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage&) override;
    void handleAsyncUpdate() override;
    void prepareBuffers(int size);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Engine)
};
}
