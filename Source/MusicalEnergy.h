#pragma once
#include "PcmView.h"
#include <atomic>
#include <memory>
#include <optional>
#include <vector>

namespace mixworks
{
// Supplementary activity analysis; independent of the BPM/downbeat detector.
constexpr int musicalEnergyRevision = 1;
struct MusicalEnergyWindow
{
    double start = 0, end = 0, coverage = 0;
    double onsetsPerSecond = 0, bassOnsetsPerSecond = 0, bassRatio = 0;
    double crestDb = 0, dynamicRangeDb = 0;
    double rhythm() const;
    double bass() const;
    double dynamics() const;
    double score() const;
};
struct MusicalEnergyProfile
{
    int revision = 0;
    double duration = 0;
    std::vector<MusicalEnergyWindow> windows;
    bool complete() const;
};
MusicalEnergyProfile analyseMusicalEnergy(const PcmView&, double sampleRate,
    const std::shared_ptr<std::atomic<bool>>& cancelled = {});
std::optional<double> musicalEnergyAt(const MusicalEnergyProfile&, double begin, double end);
std::optional<double> musicalEnergyBody(const MusicalEnergyProfile&);
juce::var musicalEnergyToJson(const MusicalEnergyProfile&);
MusicalEnergyProfile parseMusicalEnergy(const juce::var&, double sourceDuration);
}
