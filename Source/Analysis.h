#pragma once
#include <cmath>
#include <atomic>
#include <array>
#include <optional>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PcmView.h"

namespace mixworks
{
constexpr int maxAnalysisWindows = 9;
struct BeatEstimate
{
    double sourceStart = 0, bpm = 0, origin = 0, confidence = 0;
};
// Optional source evidence for bounded detector comparisons, never a confidence probability.
struct LocalTimingEvidence
{
    double sourceStart = 0, sourceEnd = 0, examinedPhysicalSeconds = 0;
    bool hasPhysicalFit = false, conflict = false;
    double physicalBpm = 0, physicalOrigin = 0, physicalAgreement = 0, physicalCoverage = 0;
    std::optional<double> globalPhysicalSupport;
    double neuralAgreement = 0, neuralCoverage = 0;
    int physicalEvents = 0, physicalInliers = 0;
};
struct RhythmDiagnostics
{
    std::vector<LocalTimingEvidence> localFits;
    std::vector<std::array<double, 3>> physicalTransients; // source seconds, novelty, attack contrast
};
struct MusicAnalysis
{
    double bpm = 0, beatOrigin = 0, tempoConfidence = 0, gridErrorMs = 0;
    double downbeatOrigin = -1, downbeatConfidence = 0, downbeatExaminedSeconds = 0; // Any source downbeat anchoring inferred 4/4 phase; -1 means unknown.
    int key = -1; // C=0..B=11 major, C=12..B=23 minor; -1 is unknown.
    int alternativeKey = -1;
    double keyConfidence = 0, tuningCents = 0, examinedSeconds = 0;
    bool variableTempo = false;
    juce::String rhythmMethod = "dsp", rhythmIssue;
    double neuralPhaseCorrectionMs = 0;
    double neuralBpm = 0, signalBpm = 0, signalTempoConfidence = 0;
    bool signalGridNeedsReview = false;
    juce::String downbeatMethod = "unknown";
    std::vector<BeatEstimate> beatWindows;
};
MusicAnalysis analyseMusic(const PcmView&, double sampleRate, const std::shared_ptr<std::atomic<bool>>& cancelled = {}, RhythmDiagnostics* diagnostics = nullptr);
struct SourceBeatGrid
{
    double bpm = 0, pulseOrigin = 0, downbeatOrigin = -1;
    bool hasPulse() const { return std::isfinite(bpm) && bpm > 0 && std::isfinite(pulseOrigin)
        && (downbeatOrigin == -1 || (std::isfinite(downbeatOrigin) && downbeatOrigin >= 0)); }
    bool hasBar() const { return hasPulse() && downbeatOrigin >= 0; }
    double beatAt(double sourceSeconds) const;
    double timeAtBeat(double beat) const;
    float clickAt(double sourceSeconds) const;
};
SourceBeatGrid resolveSourceGrid(const MusicAnalysis&, double manualBpm, double manualBeatOrigin);
juce::String keyNoteName(int key, bool flats = true);
juce::String keyCompactName(int key, bool flats = true);
juce::String keyCamelot(int key);
int keyWheelNumber(int key);
bool harmonicallyCompatible(int firstKey, int secondKey);
}
