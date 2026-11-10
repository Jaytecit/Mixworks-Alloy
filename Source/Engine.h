#pragma once
#include "Model.h"
#include "Playlist.h"
#include "ElasticAudio.h"
#include "GridCorrection.h"
#include "WavExport.h"
#include "OutputPeaks.h"
#include "LaunchClock.h"
#include "MidiConnection.h"
#include "MasterRecorder.h"
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
struct CutoffFilterProcessor
{
    std::array<TransitionFilterProcessor::Stage, 2> stages;
    double rate = 0, smooth = 0;
    void reset() { for (auto& stage : stages) stage.reset(); rate = 0; smooth = 0; }
    void process(juce::AudioBuffer<float>&, int first, int frames, double rate, float value);
};
struct ClipEffects
{
    juce::String id = juce::Uuid().toString();
    std::vector<std::unique_ptr<PluginSlot>> plugins;
    bool hasInstrument() const
    { return pluginChainHasInstrument(plugins); }
};
enum class AutomationRecordMode { read, touch, write };
// Callback-owned counters; readers never acquire the render lock. These measure
// application work, not driver underruns or time lost before callback entry.
struct AudioCallbackDiagnostics
{
    struct Snapshot { uint64_t callbacks, overBudget, lockMisses, oversized, exportSilence, totalTicks, peakTicks; };
    static_assert(std::atomic<uint64_t>::is_always_lock_free, "Audio diagnostics must be lock-free");
    std::atomic<uint64_t> callbacks {0}, overBudget {0}, lockMisses {0}, oversized {0}, exportSilence {0}, totalTicks {0}, peakTicks {0};
    void record(uint64_t elapsed, double budgetTicks)
    {
        totalTicks.fetch_add(elapsed, std::memory_order_relaxed);
        auto peak = peakTicks.load(std::memory_order_relaxed);
        while (elapsed > peak && !peakTicks.compare_exchange_weak(peak, elapsed, std::memory_order_relaxed)) {}
        if ((double) elapsed > budgetTicks) overBudget.fetch_add(1, std::memory_order_relaxed);
        callbacks.fetch_add(1, std::memory_order_relaxed);
    }
    Snapshot snapshot() const
    {
        return {callbacks.load(std::memory_order_relaxed), overBudget.load(std::memory_order_relaxed),
            lockMisses.load(std::memory_order_relaxed), oversized.load(std::memory_order_relaxed), exportSilence.load(std::memory_order_relaxed),
            totalTicks.load(std::memory_order_relaxed), peakTicks.load(std::memory_order_relaxed)};
    }
};
struct MidiBinding { int status = 0, channel = 0, number = 0; MidiAction action; };
enum class ScheduledPauseState { idle, pending, paused, cancelled };
enum class ScheduledPauseReason { none, requested, transportChanged, clockChanged, unavailable, missedBoundary, deviceChanged };
struct ScheduledPauseStatus
{
    ScheduledPauseState state = ScheduledPauseState::idle;
    ScheduledPauseReason reason = ScheduledPauseReason::none;
    double boundary = 0, beat = 0, actualTime = 0;
};
enum class LiveHotcueState { idle, pending, launched, cancelled };
struct LiveHotcueStatus
{
    LiveHotcueState state = LiveHotcueState::idle;
    ScheduledPauseReason reason = ScheduledPauseReason::none;
    int slot = -1;
    double boundary = 0, beat = 0, actualTime = 0;
    bool active = false;
};
struct PreparedProject
{
    ProjectResult parsed;
    std::vector<std::shared_ptr<Media>> library;
    juce::String error;
    std::optional<MediaReference> failedSource; // Only a required source read can request explicit relinking.
};
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
    uint64_t requestLibraryPreview();
    bool startLibraryPreview(juce::AudioSource&, uint64_t request);
    void detachLibraryPreview(juce::AudioSource&);
    bool hasLibraryPreview(juce::AudioSource&);
    bool startPrivatePreview(juce::AudioSource&,int firstChannel);
    void detachPrivatePreview(juce::AudioSource&);
    juce::Result queueProgrammeProject(Project,juce::var snapshot,double boundary,juce::String launchClip = {},double launchShift = 0);
    void cancelPreparedLaunch();
    Project project;
    WorkMode workMode = WorkMode::arrange;
    TakeState takeState() const;
    bool capturingPerformance() const;
    juce::Result setWorkMode(WorkMode);
    juce::Result beginPerformance();
    void finalisePerformance();
    void appendTakeEvent(TakeEvent); // Locked caller; no allocation after reserved capacity.
    void recordProjectCommit(); // Message-thread commit of an audible structural edit.
    void returnToAutomation(const AutomationAddress&);
    void servicePerformance(); // Finalise natural endings/device loss and plugin-state warnings.
    std::unique_ptr<Engine> prepareTakeReplay(int index,juce::String& error);
    void renderTake(juce::AudioBuffer<float>&,int frames,double performanceSeconds);
    bool replayingTake() const { return replayTake.has_value(); }
    juce::Result auditionTake(int index);
    juce::Result installTakeAudition(std::unique_ptr<Engine>);
    void endTakeAudition();
    // Transient intent identity: Stop/Play/seek/mode/document changes and newer
    // audition requests invalidate asynchronous take preparation.
    uint64_t beginTakeAuditionRequest();
    bool isTakeAuditionRequestCurrent(uint64_t) const;
    juce::String performanceStatus() const;
    juce::File masterRecordingDestination; // Optional; new path required for each pass.
    bool recordMasterWav = false;
    juce::String performanceError;
    MasterRecorder masterRecorder;
    bool performanceProjectPending = false;
    bool applyingPerformanceEvent = false;
    std::function<void()> onPerformanceBegin;
    std::function<void()> onPerformanceFinalised;
    juce::String selectedClip;
    Clip* controlClip(int track);
    const Clip* controlClip(int track) const;
    TrackSettings& controlSettings(int track);
    TrackSettings& controlEffectSettings(int track);
    const std::vector<std::unique_ptr<PluginSlot>>& pluginChain(int track) const;
    std::vector<std::unique_ptr<PluginSlot>>& pluginChain(int track);
    juce::String cloneClipEffects(Clip&);
    std::vector<std::shared_ptr<Media>> library;
    std::array<std::vector<std::unique_ptr<PluginSlot>>, trackCount> plugins;
    std::atomic<bool> playing { false }, exporting { false };
    std::atomic<double> position { 0 }, sampleRate { 44100 }, tempo { 120 };
    std::atomic<bool> streamBuffering {false}, streamFailed {false};
    std::atomic<uint64_t> streamWaitCallbacks {0};
    bool requestPlayback(double at, int frames);
    std::array<std::atomic<float>, trackCount> meters {};
    std::atomic<float> masterPeak { 0 };
    StereoPeakFeed outputPeaks; // Device output before monitor mute; offline rendering never publishes here.
    AudioCallbackDiagnostics callbackDiagnostics; // Cumulative for this Engine lifetime; never saved in sessions.
    std::atomic<float> masterInputPeak { 0 }; // -1 when a pre-rendered/source preview has no bus-input measurement.
    std::atomic<bool> masterClipped { false };
    SpectrumFeed spectrum;
    std::atomic<juce::uint64> visualClockRevision {0}; // Seek/reset identity, independent of playback frame cadence.
    SpectrumFeed visualSpectrum {true}; // Independent stamped SPSC feed for visual audio.
    SpectrumFeed studioSpectrum; // Optional feed for the standalone Visual Studio.
    std::atomic<juce::uint64> studioTempo {0}; // One clock snapshot for the Studio publisher.
    std::atomic<bool> monitorMuted { false }; // Device output only; project renders and exports remain audible.
    std::atomic<float> privatePreviewGain {.5f};
    void prepareRender(double rate,int size);
    std::atomic<int> selectedTrack { 0 }, midiNotes { 0 };
    // Surface routing is engine-owned so mouse and mapped MIDI use the same targets.
    std::array<int, 8> deskLanes {0, 1, 0, 1, 0, 1, 0, 1};
    MidiAction deskPotAction(int slot) const;
    AutomationAddress deskPotAddress(int slot) const;
    void cycleDeskLane(int slot);
    std::function<void(bool)> onControl;
    std::function<void(const juce::String&)> onSeekStatus;
    std::atomic<bool> seekPending {false};
    juce::String seekError; // Message-thread result of the latest request; never saved.
    std::function<void()> onMidiChanged;
    std::function<void(MidiAction)> onPerformanceAction;
    std::function<void()> onProjectSwitchBegin; // Before an admitted Snap/mute/solo edit, on the message thread.
    std::function<void()> onAutomationWriteBegin;
    AutomationRecordMode automationRecordMode = AutomationRecordMode::read;
    std::array<bool, trackCount> automationArmed {};
    bool masterAutomationArmed = false;
    bool controlInputEnabled = true; // Message-thread workflows pause performance input during exclusive work.
    bool canWriteAutomation(int track) const { return playing && !exporting && !rehearsalActive() && (capturingPerformance() || automationRecordMode != AutomationRecordMode::read) && (track == -1 ? masterAutomationArmed : track >= 0 && track < trackCount && automationArmed[(size_t) track]); }
    void setAutomationRecordMode(AutomationRecordMode);
    void armAutomation(int track, bool armed); // -1 is master.
    bool recordingAutomation() const { return !activeWrites.empty(); }
    void advanceAutomation(double sessionTime);
    void finishAutomation();
    void setPlaying(bool);
    juce::Result toggleScheduledPause(); // Explicit programme transport action; never a hotcue launch.
    void cancelScheduledPause(ScheduledPauseReason = ScheduledPauseReason::requested);
    juce::Result setLaunchSettings(const LaunchSettings&);
    ScheduledPauseStatus scheduledPauseStatus() const;
    juce::String scheduledPauseText() const;
    void serviceScheduledPause(); // Message thread: finish an executed take before any subsequent start.
    juce::Result triggerLiveHotcue(const juce::String& clipId, int slot);
    void cancelLiveHotcue(const juce::String& clipId);
    void cancelLiveHotcues(ScheduledPauseReason = ScheduledPauseReason::requested, bool clearVoices = false);
    void serviceLiveHotcues();
    bool hasLiveHotcueVoices() const;
    LiveHotcueStatus liveHotcueStatus(const juce::String& clipId) const;
    juce::String liveHotcueText(const juce::String& clipId) const;
    double liveClipTime(const juce::String& clipId, double programmeTime) const;
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

    juce::String startDevice(bool enableMidi = true);
    MidiConnectionStatus midiConnectionStatus();
    juce::Result correctSource(const std::shared_ptr<Media>&, const SourceCorrection&);
    void stop();
    class PreparedSeek
    {
        friend class Engine;
        std::vector<Clip> clips;
        ElasticRenderer renderer;
        const Engine* owner = nullptr;
        double seconds = 0, rate = 0;
        int frames = 0;
        uint64_t request = 0, revision = 0;
        bool running = false;
        juce::String error;
    public:
        uint64_t windowsRendered() const { return renderer.windowsRendered(); }
    };
    PreparedSeek prepareSeek(double seconds); // Snapshot, then read/decode/stretch without holding lock.
    juce::Result commitSeek(PreparedSeek); // Revalidate and swap; displaced streams retire after unlocking.
    juce::Result queueSeek(double seconds); // One worker; newer requests supersede pending destinations.
    juce::Result seek(double seconds);
    void resetPlugins(bool resetElastic = true);
    uint64_t elasticWindowsRendered() const { return elasticRenderer.windowsRendered(); } // Locked diagnostic reader.
    void prepareClipProcessing();
    juce::String addPlugin(int track, const juce::File&);
    juce::String instantiate(const juce::PluginDescription&, std::unique_ptr<PluginSlot>&);
    juce::var captureProject(bool programmeSnapshot = false);
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
    void render(juce::AudioBuffer<float>& output, int frames, double at, juce::MidiBuffer& midi, const Project* source = nullptr, bool liveVoices = false);
    int preparedBlockSize() const { return blockSize; }
    juce::Result prepareTransitionRehearsal(const juce::String& incoming, const std::shared_ptr<std::atomic<bool>>& cancelled);
    juce::Result auditionTransitionAlternative(int slot);
    void endTransitionRehearsal();
    bool rehearsalActive() const { return rehearsalSlot >= 0; } // lock protects rehearsal state
    int transitionRehearsalSlot() const { return rehearsalSlot; }
    juce::String rehearsalSummary() const;
    void enableMidi(const juce::String& identifier, bool enabled);
    void saveMidiMappings();
    juce::Result renameMidiMapping(const juce::String& title);
    juce::Result saveMidiProfile(const juce::String& title = {}, bool saveAs = false);
    juce::Result openMidiProfile(const juce::File&);
    juce::Result importMidiProfile(const juce::File&);
    juce::Result exportMidiProfile(const juce::File&);
    juce::Result deleteMidiProfile();
    juce::Array<juce::File> midiProfileFiles() const;
    juce::File selectedMidiProfile();
    juce::String midiMappingName();
    juce::Result reloadMidiMappings();
    juce::Result clearMidiMappings();
    void beginLearn(const juce::String&, bool keepEnabled = false);
    void setMidiLearnEnabled(bool);
    bool isMidiLearnEnabled();
    bool canSkipControllerStep();
    void beginControllerSetup(bool replaceExisting = false);
    void skipControllerStep();
    juce::String mappingSummary();
    juce::String activeMappingTitle();
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
    class SeekWorker;
    std::unique_ptr<SeekWorker> seekWorker;
    PreparedSeek snapshotSeek(double seconds);
    static void warmSeek(PreparedSeek&, const std::function<bool()>& cancelled = {});
    struct ProcessorCapture final : juce::AudioProcessorListener
    {
        juce::AudioProcessor& processor; std::atomic<bool> changed {false};
        explicit ProcessorCapture(juce::AudioProcessor&);
        ~ProcessorCapture() override;
        void audioProcessorParameterChanged(juce::AudioProcessor*,int,float) override {}
        void audioProcessorChanged(juce::AudioProcessor*,const ChangeDetails&) override;
    };
    std::vector<std::unique_ptr<ProcessorCapture>> processorCaptures;
    std::unique_ptr<Engine> originalEngine;
    juce::AudioSource* privatePreviewSource = nullptr;
    int privatePreviewFirstChannel = -1;
    juce::AudioBuffer<float> privatePreviewBuffer;
    std::unique_ptr<Project> queuedProgramme;
    TakeEvent queuedProgrammeEvent;
    double queuedProgrammeBoundary = 0;
    bool queuedProgrammeCommitted = false;
    struct ParameterCapture final : juce::AudioProcessorParameter::Listener
    {
        Engine& engine; juce::AudioProcessorParameter& parameter; TakeEvent event;
        ParameterCapture(Engine&,juce::AudioProcessorParameter&,TakeEvent);
        ~ParameterCapture() override;
        void parameterValueChanged(int,float) override;
        void parameterGestureChanged(int,bool) override {}
        std::atomic<bool> missed {false};
    };
    std::vector<std::unique_ptr<ParameterCapture>> parameterCaptures;
    std::unique_ptr<Project> performanceOriginal;
    std::atomic<int64_t> performanceFrames {0};
    std::atomic<bool> performanceActive {false}, performanceEnded {false}, performanceCallbackMissed {false};
    int activeTake = -1;
    int64_t takeSequence = 0;
    std::optional<PerformanceTake> replayTake;
    std::unique_ptr<Engine> takeAudition;
    uint64_t takeAuditionRequest = 0; // Under lock; never persisted.
    std::vector<std::unique_ptr<Engine>> replayProjects;
    size_t replayEvent = 0;
    int64_t replayFrame = 0;
    double replaySource = 0;
    bool replayRunning = true;
    std::map<juce::String,double> replayShifts;
    juce::AudioBuffer<float> takeBuffer;
    void applyTakeEvent(const TakeEvent&);
    void attachPerformanceParameters();
    void detachPerformanceParameters();
    void restoreInitialPluginStates(const juce::var&);
    juce::AudioSource* libraryPreviewSource = nullptr; // Under lock; device callback only, never offline render.
    uint64_t libraryPreviewGeneration = 0;
    bool offlineRenderer = false;
    // Admission/copy/reclamation is message-thread-only. Callback commits retain
    // every captured resource and change only scalar state/cursor fields.
    struct LiveHotcueVoice
    {
        Clip captured;
        SourceIdentity source;
        HotcueMarker cue;
        LiveHotcueStatus status;
        TempoMap grid;
        LaunchSettings settings;
        double bpm = 0, rate = 0, expected = 0, destination = 0, shift = 0;
        bool followTempo = false;
        std::unique_ptr<ElasticRenderer> queuedElastic, activeElastic;
    };
    std::vector<LiveHotcueVoice> liveHotcueVoices;
    std::atomic<bool> liveExclusiveInterrupted {false};
    ScheduledPauseReason liveHotcueInvalidReason(const LiveHotcueVoice&, bool queue = true) const;
    LiveHotcueVoice* liveVoiceFor(const juce::String&);
    const LiveHotcueVoice* liveVoiceFor(const juce::String&) const;
    bool renderClipAudio(const Project&, const Clip&, int frames, double at, const juce::MidiBuffer&, ElasticRenderer* = nullptr);
    juce::MidiBuffer liveClipMidi;
    // Device transport owns elapsed integer frames; position remains the public seconds view.
    bool deviceClockValid = false;
    uint64_t seekRequest = 0;
    ScheduledPauseStatus pauseStatus;
    TempoMap pauseGrid;
    LaunchSettings pauseSettings;
    bool pauseFollowTempo = false, pauseFinishPending = false;
    double pauseBpm = 0, pauseRate = 0, pauseExpected = 0;
    std::atomic<bool> pauseExclusiveInterrupted {false};
    ScheduledPauseReason scheduledPauseInvalidReason() const;
    int framesBeforeScheduledPause(int frames, double at); // -1 when no pause falls in this block.
    void commitScheduledPause(double actualTime);
    double deviceClockAnchor = 0, deviceClockExpected = 0, deviceClockRate = 0;
    juce::int64 deviceClockFrames = 0;
    bool hasUnsupportedLatency() const;
    bool processPlugin(PluginSlot&, juce::AudioBuffer<float>&, juce::MidiBuffer&);
    void stopForLatency(juce::AudioBuffer<float>&, int frames);
    ElasticRenderer elasticRenderer;
    std::array<ToneEq, trackCount> equalizers;
    std::array<CutoffFilterProcessor, trackCount> cutoffFilters;
    ToneEq masterEqualizer;
    void processMaster(const Project&, juce::AudioBuffer<float>&, int frames, double at);
    struct ClipProcessing { ToneEq eq; TransitionFilterProcessor filter; CutoffFilterProcessor cutoff; };
    std::map<juce::String, ClipProcessing> clipProcessors;
    std::array<juce::AudioBuffer<float>, trackCount> trackBuffers;
    std::array<juce::MidiBuffer, trackCount> trackMidi;
    std::array<juce::MidiBuffer, trackCount> segmentMidi;
    std::array<juce::MidiBuffer, trackCount> processedMidi;
    juce::MidiBuffer callbackMidi, clipMidi, clipSegmentMidi, clipProcessedMidi;
    juce::AudioBuffer<float> mixBuffer, clipBuffer, effectBuffer;
    bool processTrackEffects(const Project&, const Clip&, int frames, double at, const juce::MidiBuffer&);
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
    juce::String midiNamedProfile;
    MidiReconnectPolicy midiReconnect;
    juce::var midiProfileDocument(const juce::String& title, const juce::String& reference);
    static juce::String writeMidiProfileFile(const juce::File&, const juce::String&);
    juce::Result loadMidiProfileFile(const juce::File&, bool activate, bool named = false);
    juce::Result midiProfileFailure(const juce::String&);
    void applyMidiProfile(std::vector<MidiBinding>, const juce::String&, const juce::String&);
    juce::File midiProfilesDirectory() const;
    juce::String midiProfilePersistenceState();
    juce::String midiProfileError, midiProfileTitle;
    juce::String midiProfileReadEvidence;
    bool midiCallbackRegistered = false;
    std::map<juce::String, double> midiInputActivity;
    std::set<int> pressedMidiButtons; // MIDI lock; edge-trigger mapped buttons, consume their releases.
    struct ControlEvent { MidiAction action; float value; double time; bool running; int targetTrack = -1; double inputMs = juce::Time::getMillisecondCounterHiRes(); };
    std::vector<ControlEvent> pendingControls;
    bool applyControl(const ControlEvent&);
    struct GainSnapGesture { AutomationAddress address; double lastInputMs = -1; bool canExceedUnity = false; };
    std::vector<GainSnapGesture> gainSnapGestures = std::vector<GainSnapGesture>(trackCount * 4 + 1);
    std::atomic<bool> gainSnapResetPending {false};
    bool applyGainControl(const ControlEvent&, AutomationAddress, float db);
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
    enum class MidiLearnMode { off, surface, single, guided };
    MidiLearnMode midiLearnMode = MidiLearnMode::off; // MIDI lock; shared by settings and live desk.
    juce::String midiLearnFeedback;
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
