#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <memory>
#include <optional>
#include <vector>
#include <map>
#include <set>
#include "AnalysisCache.h"
#include "PcmCache.h"
#include "OriginalAudio.h"
#include "LibraryAssets.h"
#include "Mastering.h"
#include "PerformanceTake.h"
#include "VisualScenes.h"

namespace mixworks
{
constexpr int trackCount = 4;
constexpr int projectVersion = 47;
enum class LaunchTiming { immediate, nextBeat, nextBar, nextPhrase };
struct LaunchSettings
{
    LaunchTiming timing = LaunchTiming::nextBar;
    int phraseBars = 16;
    juce::Result validate() const;
    bool operator==(const LaunchSettings& other) const { return timing == other.timing && phraseBars == other.phraseBars; }
};
struct ElasticSource;
struct LabelMarker
{
    juce::String id = juce::Uuid().toString();
    double time = 0;
    juce::String text;
    juce::Result validate() const;
};
struct TempoPoint
{
    double time = 0, bpm = 120; bool ramp = true;
    juce::String clipId; // Empty for independently added or legacy tempo markers.
    bool operator==(const TempoPoint& other) const { return time == other.time && bpm == other.bpm && ramp == other.ramp && clipId == other.clipId; }
};
struct TempoMap
{
    double fallback = 120;
    std::vector<TempoPoint> points;
    double origin = 0; // Engine seconds of musical zero; earlier audio is playable pre-roll.
    double beatsFromStart(double time) const;
    double timeFromStartBeat(double beat) const;
    double bpmAt(double time) const;
    double beatsAt(double time) const;
    double timeAtBeat(double beat) const;
    double snap(double time, bool enabled, double step = 1) const;
    double snapWithPreroll(double time, bool enabled, double step = 1) const; // Signed audio edits extend the first held tempo before mix zero.
    std::vector<TempoPoint> localPoints(double begin, double end) const;
    int setPoint(double time, double bpm, bool ramp = true);
    void movePoint(int index, double time, double bpm);
    int setTrackPoint(const juce::String& clipId, double time, double bpm, bool ramp = true);
    int addTrackPointAtBeat(const juce::String& clipId, double beat, double bpm, bool ramp = true);
    void detachMissingTracks(const std::vector<struct Clip>&);
};
// Bound transient decoding of one file; retained library PCM is mapped from owned cache files.
constexpr int64_t maxDecodedBytes = 1024LL * 1024 * 1024;
struct WaveformRange
{
    float peak = 0, rms = 0;
    bool ready = true; // False: sample detail is loading; do not substitute coarse peaks.
};
struct Media
{
    juce::String id = juce::Uuid().toString(), name;
    juce::File file;
    LibraryAssets assets;
    juce::AudioBuffer<float> samples;
    std::unique_ptr<AudioCache> cache;
    std::shared_ptr<OriginalAudio> original;
    double sampleRate = 44100.0;
    MusicAnalysis analysis;
    int analysisVersion = analysisRevision;
    SourceIdentity analysisSource;
    int64_t sourceModified = 0;
    int64_t analysisFileModified = 0, analysisFileBytes = 0;
    bool analysisFromCache = false;
    bool decodedFromCache = false;
    juce::String playbackCacheWarning;
    juce::String analysisCacheWarning;
    juce::String playbackError; // Transient source-check failure; never silently play a missing clip.
    bool sourceDetailsPending = false; // Fresh insertion: current identity and duration await worker verification.
    bool sessionMetadataPending = false; // Unused saved reference: retain its identity/overrides on first hydration.
    double manualBpm = 0, manualBeatOrigin = -1;
    int manualKey = -1;
    SourceBeatGrid grid() const
    {
        auto grid = resolveSourceGrid(analysis, manualBpm, manualBeatOrigin);
        if (manualBeatOrigin < 0 && grid.hasPulse() && grid.downbeatOrigin < 0 && analysis.downbeatGuess >= 0) grid.downbeatOrigin = analysis.downbeatGuess;
        return grid;
    }
    double bpm() const { return grid().bpm; }
    double beatOrigin() const { return grid().pulseOrigin; }
    double downbeatOrigin() const;
    int key() const { return manualKey >= 0 ? manualKey : analysis.key >= 0 ? analysis.key : analysis.keyGuess; }
    bool keyConfirmed() const { return key() >= 0 && key() < 24 && (manualKey >= 0 || analysis.keyConfidence >= keyReviewConfidence); }
    bool needsAnalysis() const;
    std::vector<float> peaks, waveformRms; // Saved display data, never an audio gain.
    MusicalEnergyProfile energy; // Independently versioned, source-owned activity evidence.
    SourceSections sectionTemplate; // Reusable source defaults; explicit placed snapshots retain priority.
    juce::String sectionTemplateError;
    bool hasPlayback() const { return original != nullptr || cache != nullptr || samples.getNumSamples() > 0; }
    float sample(int ch, int frame) const { return original ? original->sample(ch, frame) : channelData(ch)[frame]; }
    int frames() const { return cache ? cache->frames : samples.getNumSamples() > 0 ? samples.getNumSamples() : analysisSource.frames; }
    int channels() const { return cache ? cache->channels : samples.getNumSamples() > 0 ? samples.getNumChannels() : analysisSource.channels; }
    const float* channelData(int ch) const { return cache ? cache->channelData(ch) : samples.getReadPointer(ch); }
    PcmView audioView() const { return cache ? PcmView(cache->channelData(0), cache->channels, cache->frames, cache->frames) : PcmView(samples); }
    double duration() const { return frames() / sampleRate; }
    WaveformRange waveformRange(double sourceBegin, double sourceEnd) const;
    void prepareWaveformBody(); // Worker-side preparation, before publication to the UI.
    juce::String libraryTitle() const;
    TrackAnalysisRecord analysisRecord() const;
    std::shared_ptr<Media> catalogueCopy() const;
};
constexpr int stemCount = 4;
inline constexpr const char* stemNames[] {"drums", "bass", "other", "vocals"};
struct StemAssets
{
    juce::File directory;
    juce::String manifestHash;
    std::array<std::shared_ptr<Media>, stemCount> sources;
};
juce::Result loadStemAssets(const Media&, const juce::File&, const juce::String& expectedManifest, std::shared_ptr<const StemAssets>&);
juce::Result loadCachedStemAssets(const Media&, const juce::File& root, std::shared_ptr<const StemAssets>&);
struct PreparedClipGeometry
{
    std::shared_ptr<const Media> source;
    double offset = 0, length = 0, rate = 1, loopSpan = 0, loopPhase = 0, sourceBpm = 0;
    bool reversed = false;
    std::vector<TempoPoint> tempo; // Only the tempo segments intersecting this clip, in local seconds.
    double pitchSemitones = 0;
    bool operator==(const PreparedClipGeometry& other) const;
};
struct PreparedAudio
{
    juce::AudioBuffer<float> samples;
    std::unique_ptr<AudioCache> cache;
    bool fromDiskCache = false;
    juce::String cacheWarning;
    int frames() const { return cache ? cache->frames : samples.getNumSamples(); }
    int channels() const { return cache ? cache->channels : samples.getNumChannels(); }
    const float* channelData(int ch) const { return cache ? cache->channelData(ch) : samples.getReadPointer(ch); }
    int64_t byteSize() const { return (int64_t) frames() * channels() * sizeof(float); }
    // Mapped PCM is paged by the OS; only owned sample allocations consume our RAM budget.
    int64_t residentBytes() const { return (int64_t) samples.getNumSamples() * samples.getNumChannels() * sizeof(float); }
    double rate = 1;
    double pitchSemitones = 0;
    bool clipLocal = false;
    PreparedClipGeometry geometry;
};
struct PluginControl
{
    int insert = 0;
    juce::String parameterId;
};
enum class AutomationTarget { volume, bass, mid, treble, pan, cutoff, master, plugin };
enum class AutomationEdit { change, removePoint, clearCurve };
constexpr int trackAutomationCount = 6;
struct AutomationAddress
{
    int track = 0;
    AutomationTarget target = AutomationTarget::volume;
    juce::String slotId, parameterId, clipId;
    int stem = -1; // Parent-owned source clock; never a separate analysed song.
    bool operator==(const AutomationAddress& other) const { return stem == other.stem && track == other.track && target == other.target && slotId == other.slotId && parameterId == other.parameterId && clipId == other.clipId; }
};
// Cutoff storage is affine in Hz on either side of centre, preserving legacy ramps.
float lowPassToCutoff(float hz);
float cutoffFromPosition(double position);
double cutoffPosition(float value);
float cutoffFromMidi(float value);
juce::String cutoffText(float value, bool compact = false);
bool parseCutoffText(const juce::String&, float& value);
juce::Range<float> automationRange(AutomationTarget);
const char* automationId(AutomationTarget);
const char* automationName(AutomationTarget);
// Bend belongs to the following segment: -1 brings the change forward, +1
// holds the first value until the second node. Zero preserves legacy ramps.
double automationProgress(double fraction, double bend, bool exponential = false);
// Exponential spans represent frequency sweeps directly; false retains existing rational bends.
struct AutomationPoint { double time = 0; float value = 0; double bend = 0; bool exponential = false; bool gainSpace = false; };
struct AutomationCurve
{
    bool enabled = false;
    std::vector<AutomationPoint> points; // Session seconds for mixer lanes, source-elapsed seconds for clips; held endpoints.
    bool active() const { return enabled && !points.empty(); }
    float valueAt(double time, float manualValue) const;
    float interpolatedValue(double time, float fallback) const;
    int setPoint(double time, float value);
    void movePoint(int index, double time, float value);
    AutomationCurve sliced(double begin, double end, float fallback) const;
};
struct LaneHandleState
{
    // Empty seed geometry means authored: deleted handles must not regenerate.
    std::map<juce::String, std::vector<double>> seeds;
    std::set<double> generatedPoints;
};
struct PluginAutomation
{
    juce::String slotId, parameterId, name;
    float manualValue = 0;
    AutomationCurve curve;
};
struct TrackSettings
{
    juce::String name;
    float gainDb = 0, pan = 0, filterCutoff = 0;
    bool mute = false, solo = false;
    std::array<float, 3> eqDb {};
    std::array<AutomationCurve, trackAutomationCount> automation;
    std::vector<PluginAutomation> pluginAutomation;
    float manualValue(AutomationTarget target) const { return target == AutomationTarget::volume ? gainDb : target == AutomationTarget::pan ? pan : target == AutomationTarget::cutoff ? filterCutoff : eqDb[(size_t) target - 1]; }
    float valueAt(AutomationTarget target, double time) const { return automation[(size_t) target].valueAt(time, manualValue(target)); }
    std::array<PluginControl, 8> pluginControls;
};
struct ClipEffects;
std::shared_ptr<ClipEffects> makeClipEffects();
const juce::String& effectsIdentity(const std::shared_ptr<ClipEffects>&);
void restoreEffectsIdentity(const std::shared_ptr<ClipEffects>&,const juce::String&);
double naturalEffectTail(const std::shared_ptr<ClipEffects>&);
struct RehearsalContext
{
    int beforeBars = 1, afterBars = 1;
    juce::Result validate() const;
    bool operator==(const RehearsalContext& other) const { return beforeBars == other.beforeBars && afterBars == other.afterBars; }
};
juce::var rehearsalContextToJson(const RehearsalContext&);
juce::Result readRehearsalContext(const juce::var&, RehearsalContext&);
enum class PendingPlacement { none, append, drop, move, failed };
enum class TransitionPreset { alignOnly, bassSwap, crossfade };
enum class TransitionBandMode { none, crossfade, fadeIn, fadeOut, swap, manual, swapFade, constantPower };
struct TransitionBandSettings
{
    TransitionBandMode mode = TransitionBandMode::none;
    double strength = 1, gapBeats = 0, position = 0.5;
    std::array<double, 2> bend {}; // Incoming and outgoing, independently editable.
    juce::Result validate(int band) const;
    bool generated() const { return mode != TransitionBandMode::none && mode != TransitionBandMode::manual; }
    bool swapping() const { return mode == TransitionBandMode::swap || mode == TransitionBandMode::swapFade; }
    bool operator==(const TransitionBandSettings& b) const { return mode == b.mode && strength == b.strength && gapBeats == b.gapBeats && position == b.position && bend == b.bend; }
};
enum class TransitionFilterMode { none, highToLow, lowToHigh };
struct TransitionFilterSettings
{
    TransitionFilterMode mode = TransitionFilterMode::none;
    double strength = 1;
    juce::Result validate() const;
    bool active() const { return mode != TransitionFilterMode::none && strength > 0; }
    bool operator==(const TransitionFilterSettings& b) const { return mode == b.mode && strength == b.strength; }
};
juce::var transitionFilterToJson(const TransitionFilterSettings&);
juce::Result readTransitionFilter(const juce::var&, TransitionFilterSettings&);
// Portable treatment of one existing frame. Positions are fractions of its
// timeline span; manual curves, song geometry and effects are never captured.
struct TransitionStyle
{
    // Presets snapshot ordinary source curves in frame-relative coordinates.
    // Legacy procedural fields below remain input compatibility only.
    struct Nodes
    {
        std::array<std::array<AutomationCurve, trackAutomationCount>, 2> curves;
        juce::Result validate() const;
        bool operator==(const Nodes&) const;
    };
    std::optional<Nodes> nodes;
    std::array<std::optional<TransitionBandSettings>, 4> bands;
    TransitionFilterSettings filter;
    std::array<double, 4> volume {0, 0, 1, 1}; // in begin/end, out begin/end
    std::optional<std::array<double, 4>> bassPositions;
    std::array<float, 4> bassValues {};
    std::array<bool, 2> bassEnabled {true, true};
    juce::Result validate() const;
    bool operator==(const TransitionStyle& b) const { return nodes == b.nodes && bands == b.bands && filter == b.filter && volume == b.volume && bassPositions == b.bassPositions && bassValues == b.bassValues && bassEnabled == b.bassEnabled; }
};
struct NamedTransitionStyle { juce::String name; TransitionStyle style; };
juce::var transitionStyleToJson(const TransitionStyle&);
juce::Result readTransitionStyle(const juce::var&, TransitionStyle&);
juce::Result readNamedTransitionStyles(const juce::var&, std::vector<NamedTransitionStyle>&);
juce::var namedTransitionStylesToJson(const std::vector<NamedTransitionStyle>&);
juce::Result writeTransitionPreset(const juce::File&, const NamedTransitionStyle&);
juce::Result readTransitionPreset(const juce::File&, NamedTransitionStyle&);
// A compiled, allocation-free view of the pair settings, shared by audio and drawing.
struct TransitionBandEnvelope
{
    double begin = 0, end = 0;
    float strength = 0;
    bool rising = false;
    bool constantPower = false;
    bool instant = false;
    double bend = 0;
    float gainAt(double time) const;
};
struct TransitionFilterEnvelope
{
    double begin = 0, end = 0;
    TransitionFilterSettings setting;
    bool incoming = false;
    double amountAt(double time) const;
    bool highPass() const { return (setting.mode == TransitionFilterMode::highToLow) == incoming; }
};
struct TransitionPlayback
{
    std::array<std::array<TransitionBandEnvelope, 4>, 2> pairs {};
    std::array<TransitionFilterEnvelope, 2> filters;
    std::optional<double> startCut, endCut; // Absolute frame edges; independent of band strength and audio slip.
    bool audibleAt(double time) const { return (!startCut || time >= *startCut) && (!endCut || time < *endCut); }
    float gainAt(int band, double time) const;
};
struct ClipEnvelope
{
    double length = 0, intro = 0, outro = 0, fadeIn = 0, fadeOut = 0;
    float gainAt(double localTime) const;
};
struct HotcueMarker
{
    juce::String id;
    double sourceSeconds = 0;
    bool manual = false;
    bool operator==(const HotcueMarker& b) const { return id == b.id && sourceSeconds == b.sourceSeconds && manual == b.manual; }
};
using HotcueSlots = std::array<std::optional<HotcueMarker>, 9>;
struct Clip
{
    TrackSettings controls;
    bool automaticBass = false;
    std::shared_ptr<const StemAssets> stems;
    juce::File stemDirectory; // Saved asset reference, validated before playback admission.
    juce::String stemManifestHash;
    bool stemsEnabled = true, stemsExpanded = true;
    std::array<TrackSettings, stemCount> stemControls;
    std::array<std::shared_ptr<const PreparedAudio>, stemCount> stemPrepared;

    std::shared_ptr<ClipEffects> effects = makeClipEffects();
    double effectOffset = 0; // Original track's source clock, retained across cuts.
    juce::String id = juce::Uuid().toString();
    std::shared_ptr<Media> media;
    // Absolute file seconds, owned by this placed instance. Null means automatic
    // reusable track defaults, then RMS suggestions; viewing never materialises edits.
    std::optional<std::vector<SourceSectionMarker>> sectionMarkers;
    std::optional<HotcueSlots> hotcues;
    bool hotcuesPending = false; // New-instance intent only; never seeded by Open/view/reanalysis.
    int track = 0;
    double start = 0, offset = 0, length = 0, fadeIn = 0, fadeOut = 0;
    double audioSlip = 0; // Timeline seconds of audio-only delay; automation and frame clocks stay fixed.
    // Alignment-only source positions; independent of playback and parameters.
    std::optional<double> introAnchor, outroAnchor;
    // Separate source-elapsed fade positions preserve fades across splits/retiming.
    // Omitted positions use clip edges. Slices may retain off-segment positions.
    std::optional<double> fadeInAnchor, fadeOutAnchor;
    float gainDb = 0;
    double rate = 1; // Source seconds per timeline second; key is preserved by prepared PCM.
    double pitchSemitones = 0; // Segment-only transposition; never changes source key, tempo or placement.
    bool beatmatched = false;
    bool reversed = false;
    double loopSpan = 0, loopPhase = 0; // Source seconds; zero span means a single pass.
    std::shared_ptr<const PreparedAudio> prepared;
    std::shared_ptr<const TempoMap> tempoMap; // Immutable map used to prepare this clip.
    double tempoSourceBpm = 0; // Source BPM captured when this mapping was prepared; never a live override.
    double preparedOffset = 0; // Local PCM offset retained by splits/trims.
    bool elasticPlayback = false; // Position-independent playback after moving through a tempo change.
    std::shared_ptr<const ElasticSource> elasticSource;
    bool awaitingSourceLength = false;
    juce::String pendingPredecessor; // Transient ordering of external files whose durations are not known yet.
    PendingPlacement pendingPlacement = PendingPlacement::none; // Transient; copied with undo, never saved as a completed mix.
    bool pendingAutoTransition = false; // Only Auto Arrange opts into linked placement.
    std::optional<TransitionPreset> pendingTransitionPreset;
    std::optional<double> pendingTransitionBeats;
    std::optional<bool> pendingSectionTransition;
    double sourceElapsed(double localTime) const;
    double localTimeForSource(double sourceSeconds) const;
    double audioSourceElapsed(double localTime) const { return sourceElapsed(localTime - audioSlip); }
    double audioTimelineTime(double sourceSeconds) const { return localTimeForSource(sourceSeconds) + audioSlip; }
    double sourceTimeAt(double localTime) const;
    double firstBeatTime() const;
    double beatTimelineTime(double sourceElapsedSeconds) const;
    static int gridPixel(double timelineTime, double pixelsPerSecond);
    double firstDownbeatTime() const; // -1 when bar identity is unknown or the loop breaks it.
    double transitionTime(bool intro) const; // Local timeline seconds.
    WaveformRange waveformRange(double localBegin, double localEnd) const;
    double sourceSpan() const { return loopSpan > 0 ? loopSpan : length * rate; }
    double maxLength() const;
    Clip slice(double begin, double duration) const;
    void adoptTimelineControls(const TrackSettings&);
    void moveTo(double seconds); // Preserve source beats/fades; never analyse or render audio.
    void applyPlacement(const Clip&);
    juce::Result alignAnchorTo(bool intro, double seconds);
    juce::Result alignIntroTo(double seconds) { return alignAnchorTo(true, seconds); }
    void retime(double seconds, std::shared_ptr<const TempoMap>);
    double end() const { return start + length; }
    ClipEnvelope envelope() const; // Resolve fade times once per render block.
};
std::optional<std::array<double, 4>> initialiseTransition(Clip&, Clip&, double overlap, TransitionPreset, std::optional<juce::Range<double>> window = {});
struct Project
{
    std::vector<PerformanceTake> performanceTakes;
    std::vector<VisualSceneClip> visualScenes;
    juce::String visualImage; // One caller-selected local image shared by scene shaders.
    enum class TransitionEdit { moveWindow, resizeStart, resizeEnd, moveOutgoing, moveIncoming };
    struct TransitionLink
    {
        juce::String outgoing, incoming;
        // Source-elapsed coordinates of the two paired bass ramp endpoints.
        std::optional<std::array<double, 4>> bassSwap;
        bool locked = false;
        std::array<std::optional<TransitionBandSettings>, 4> bands;
        std::array<std::optional<TransitionStyle>, 3> alternatives;
        TransitionFilterSettings filter;
        std::optional<std::array<double, 2>> frame; // Outgoing source-elapsed start/end, independent of fades.
        bool muteOutside = false;
        bool operator==(const TransitionLink& other) const { return outgoing == other.outgoing && incoming == other.incoming && bassSwap == other.bassSwap && locked == other.locked && bands == other.bands && alternatives == other.alternatives && filter == other.filter && frame == other.frame && muteOutside == other.muteOutside; }
    };
    juce::String name = "Untitled session";
    double bpm = 120;
    bool followTrackTempo = true;
    TempoMap tempo;
    float masterDb = -3;
    MasterSettings mastering;
    double effectTailSeconds = 0;
    RehearsalContext rehearsalContext;
    LaunchSettings launch;
    std::map<juce::String, SourceSections> sourceSectionEdits; // Undo ownership only; never auto-applied by session Open.
    juce::Result setRehearsalContext(const RehearsalContext&);
    juce::Result setEffectTailSeconds(double);
    double audibleEnd() const;
    AutomationCurve masterAutomation;
    bool snap = true;
    int snapDivision = 1; // 1 beat, 2 half beats, 4 quarter beats.
    double snapStep() const { return snapDivision == 4 ? 0.25 : snapDivision == 2 ? 0.5 : 1.0; }
    int keyDisplay = 2; // 0 note, 1 Camelot, 2 both.
    std::vector<Clip> clips;
    std::vector<TransitionLink> transitionLinks;
    std::vector<NamedTransitionStyle> transitionPresets;
    bool transitionLinksKnown = true;
    std::vector<LabelMarker> labels; // Absolute session seconds, ordered by time; identity survives moves.
    std::array<TrackSettings, trackCount> tracks;
    std::map<int, LaneHandleState> laneHandles; // Only lane builtins need provenance; keep Clip/AutomationCurve compact.
    std::map<juce::String, TrackSettings> effectControls; // Value-owned for Undo; processors are shared by split pieces.
    TrackSettings& effectSettings(const Clip&);
    const TrackSettings& effectSettings(const Clip&) const;
    const Clip* effectOwner(const Clip&) const;
    juce::Range<double> automationBounds(const AutomationAddress&) const;
    Project();
    TempoMap gridClock() const { return followTrackTempo ? tempo : TempoMap {bpm, {}, tempo.origin}; }
    double playbackStart() const;
    juce::Result setTimelineOrigin(double seconds);
    double duration() const;
    bool hasPendingPlacement() const;
    juce::Result putLabelMarker(const LabelMarker&);
    double snappedTime(double seconds, bool enabled) const;
    double snappedMoveTime(double reference, double seconds, bool enabled) const;
    double snappedClipStart(const Clip&, double seconds, bool enabled, double magnetSeconds = 0) const;
    juce::Result moveTransitionAnchor(const juce::String& clipId, bool intro, double timelineTime, bool snapToBeat);
    void ensureTransitionLinks();
    juce::Result validateTransitionLinks() const;
    juce::Result linkClips(const juce::String& outgoing, const juce::String& incoming);
    juce::Result addTransition(const juce::String& outgoing, const juce::String& incoming, double beats, TransitionPreset);
    juce::Result setTransitionMuteOutside(const juce::String& incoming, bool);
    void detachTransitionLinks(const juce::String& clipId);
    juce::Result unlinkClips(const std::vector<juce::String>& clipIds);
    juce::Result moveClipFollowing(const juce::String& clipId, double start, bool snapping);
    // Global half-open splice. An optional unpaired segment must exactly fill
    // the range; every other segment must lie wholly before or after it.
    juce::Result closeTimeRange(double begin, double end, const juce::String& removeClipId = {});
    // Index -1 inserts a boundary; other indices edit an existing boundary.
    juce::Result setSectionMarkerTime(const juce::String& clipId, int index, double sourceSeconds, std::optional<juce::String> label = {});
    juce::Result resetSectionMarkers(const juce::String& clipId);
    juce::Result putHotcue(const juce::String& clipId, int slot, double sourceSeconds);
    juce::Result removeHotcue(const juce::String& clipId, int slot);
    juce::Result seedHotcues(const juce::String& clipId);
    juce::Result setClipAudioSlip(const juce::String& clipId, double seconds);
    juce::Result moveClip(const juce::String& clipId, double start, bool snapping, double magnetSeconds = 0, bool adoptMarkedNeighbours = true);
    juce::Result moveClipGroup(const std::vector<juce::String>& ids, const juce::String& anchor, double start, int track, bool snapping);
    juce::Result applyPlacements(const Project&);
    juce::Range<double> transitionRange(const TransitionLink&) const;
    juce::Result editTransition(const juce::String& incoming, TransitionEdit, double delta, bool snapping);
    const TransitionLink* transitionFor(const juce::String& incoming) const;
    juce::Result setTransitionLength(const juce::String& incoming, double bars);
    juce::Result applyTransitionPreset(const juce::String& incoming, TransitionPreset);
    juce::Result captureTransitionStyle(const juce::String& incoming, TransitionStyle&) const;
    juce::Result applyTransitionStyle(const juce::String& incoming, const TransitionStyle&);
    juce::Result storeTransitionAlternative(const juce::String& incoming, int slot);
    juce::Result putTransitionPreset(const NamedTransitionStyle&);
    juce::Result setTransitionBand(const juce::String& incoming, int band, const TransitionBandSettings&);
    std::optional<std::array<double, 2>> transitionSwapTimes(const TransitionLink&, int band) const;
    juce::Result setTransitionSwapTime(const juce::String& incoming, int band, bool incomingSide, double timelineTime);
    struct BassSwapPairing { juce::String incoming, sourceClip; bool changed = false; };
    // The clicked clip receives the complement of its linked partner's nearest
    // enabled 0 -> -60 dB manual bass ramp; existing source points stay owned.
    juce::Result pairBassSwap(const juce::String& targetClip, double timelineTime, BassSwapPairing&);

    juce::Result setTransitionFilter(const juce::String& incoming, const TransitionFilterSettings&);
    juce::Result validateTransitionFilter(const TransitionLink&) const;
    juce::Result validateTransitionBands(const TransitionLink&) const;
    TransitionPlayback transitionPlaybackFor(const Clip&) const;
    void clearTransitionBass(TransitionLink&, bool preservePoints = false);
    juce::Result setTransitionLocked(const juce::String& incoming, bool);
    bool hasLockedTransitions() const;
    bool clipTransitionLocked(const juce::String&) const;
    bool automationLocked(const AutomationAddress&) const;
    juce::Result validateTransitionLocks(const Project& candidate) const;
    juce::Result reconcileTransitions();
    juce::Result moveAutomationPoint(const AutomationAddress&, int point, double sessionTime, float value);
    juce::Result reconcileBassSwaps();
    Clip* clipFor(const juce::String&);
    const Clip* clipFor(const juce::String&) const;
    TrackSettings* settings(const AutomationAddress&);
    const TrackSettings* settings(const AutomationAddress&) const;
    double automationTime(const AutomationAddress&, double sessionTime) const;
    double automationSessionTime(const AutomationAddress&, double curveTime) const;
    void automationEdited(const AutomationAddress&, AutomationEdit = AutomationEdit::change, double removedSourceTime = 0);
    void updateLaneHandles(); // Message-thread commit, never the audio callback.
    AutomationCurve* curve(const AutomationAddress&);
    const AutomationCurve* curve(const AutomationAddress&) const;
    float manualValue(const AutomationAddress&) const;
    void setManualValue(const AutomationAddress&, float);
};
struct MediaResult { std::shared_ptr<Media> media; juce::String error; };
enum class MediaReadStage { decode, waveform, analyse, cache, lookupAnalysis, savedAnalysis, playlist, savedAudio, tempo, energy };
using MediaProgress = std::function<void(MediaReadStage)>;
enum class MediaReadPurpose { playback, catalogue, energyAnalysis };
MediaResult readMedia(juce::AudioFormatManager&, const juce::File&, const MediaProgress& = {}, AnalysisCacheMode = AnalysisCacheMode::readWrite, MediaReadPurpose = MediaReadPurpose::playback, const TrackAnalysisRecord* libraryRecord = nullptr, const std::shared_ptr<std::atomic<bool>>& cancelled = {});
double snapTime(double seconds, double bpm, bool enabled);
juce::String timeText(double seconds);
enum class MidiActionKind { play, start, stop, home, master, volume, pan, mute, select, pluginParameter, bass, mid, treble,
    solo, record, read, arm, previousTrack, nextTrack, previousTransition, nextTransition, undo, redo, snap, panic, armTrack, armMaster, resetEq, cutoff, deskPot, cyclePot, resetCutoff, resetPans };
struct MidiAction
{
    MidiActionKind kind = MidiActionKind::play;
    int index = 0;
    bool operator==(const MidiAction& other) const { return kind == other.kind && index == other.index; }
};
juce::String midiActionId(MidiAction);
std::vector<MidiAction> midiActions();
juce::Optional<MidiAction> parseMidiAction(const juce::String&);
juce::var projectToJson(const Project&, const std::vector<std::shared_ptr<Media>>& library = {});
juce::var transitionBandsToJson(const Project::TransitionLink&);
juce::Result readTransitionBands(const juce::var&, Project::TransitionLink&);
// Parse validates structure before reading any media or loading any plugin.
struct MediaReference
{
    juce::String id, path, name;
    double bpm = 0, beatOrigin = -1;
    int key = -1;
};
struct ClipReference { Clip clip; juce::String mediaId; };
struct ProjectResult
{
    Project project;
    std::vector<MediaReference> media;
    std::vector<ClipReference> clips;
    juce::var document;
    juce::String error;
};
ProjectResult parseProject(const juce::String& text);
juce::String relinkProjectSource(const ProjectResult&, const juce::String& mediaId, const juce::File& replacement);
}
