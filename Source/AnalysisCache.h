#pragma once
#include "Analysis.h"
#include "MusicalEnergy.h"
#include <optional>

namespace mixworks
{
// Increment when the detector or the interpretation of its results changes.
constexpr int analysisRevision = 11;
struct SourceSectionMarker
{
    juce::String id;
    double sourceSeconds = 0, contrastDb = 0;
    bool manual = false;
    juce::String label;
    bool operator==(const SourceSectionMarker& b) const { return id == b.id && sourceSeconds == b.sourceSeconds && contrastDb == b.contrastDb && manual == b.manual && label == b.label; }
};
using SourceSections = std::optional<std::vector<SourceSectionMarker>>;
enum class AnalysisCacheMode { readWrite, readOnly, refresh, focused };
struct SourceIdentity
{
    juce::String sha256;
    int64_t bytes = 0;
    double sampleRate = 0;
    int frames = 0, channels = 0;
    bool operator==(const SourceIdentity& other) const { return sha256 == other.sha256 && bytes == other.bytes && sampleRate == other.sampleRate && frames == other.frames && channels == other.channels; }
};
struct TrackAnalysisRecord
{
    SourceIdentity source;
    MusicAnalysis analysis;
    std::vector<float> peaks;
    double manualBpm = 0, manualBeatOrigin = -1;
    int manualKey = -1;
    std::vector<float> waveformRms;
    int revision = analysisRevision;
    MusicalEnergyProfile energy;
};
struct AnalysisRecordRead
{
    std::optional<TrackAnalysisRecord> record;
    bool reusableAutomatic = false;
    bool validAutomatic = false; // Parsed facts may be displayed even when their detector revision is stale.
    bool legacyLocation = false;
};
const juce::File& analysisStoreRoot(const juce::File& initialRoot = {});
juce::File analysisRecordFile(const juce::File& audio);
juce::File legacyAnalysisRecordFile(const juce::File& audio);
juce::File analysisRecordInputFile(const juce::File& audio);
juce::String migrateLegacyAnalysisRecord(const juce::File& audio);
juce::String audioDigest(const juce::File& audio);
AnalysisRecordRead readAnalysisRecord(const juce::File& audio, const SourceIdentity&);
AnalysisRecordRead parseAnalysisRecord(const juce::var&, const std::optional<SourceIdentity>& expected = {});
juce::var analysisRecordToJson(const TrackAnalysisRecord&, bool compactWaveforms = false);
juce::String writeAnalysisRecord(const juce::File& audio, const TrackAnalysisRecord&);
}
