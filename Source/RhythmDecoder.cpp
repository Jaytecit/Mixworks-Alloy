#include "RhythmDecoder.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <set>

namespace mixworks
{
namespace
{
float activation(float logit)
{
    return logit >= 0 ? 1.0f / (1.0f + std::exp(-logit)) : std::exp(logit) / (1.0f + std::exp(logit));
}
double logProbability(double p) { return std::log(juce::jlimit(1.0e-6, 1 - 1.0e-6, p)); }
double logCategorical(double p) { return p > 0 ? std::log(p) : -std::numeric_limits<double>::infinity(); }
struct Cell { int meter, beat, period, periodIndex, offset, previousCell; };
}
DecodedRhythm decodeRhythm(const RhythmActivations& input, const RhythmDecoderSettings& settings)
{
    DecodedRhythm result;
    auto fail = [&](const juce::String& error) { result.error = error; return result; };
    const bool categorical = !input.classProbabilities.empty();
    if (!std::isfinite(input.framesPerSecond) || input.framesPerSecond <= 0
        || !std::isfinite(settings.minimumBpm) || !std::isfinite(settings.maximumBpm)
        || settings.minimumBpm <= 0 || settings.maximumBpm <= settings.minimumBpm
        || !std::isfinite(settings.tempoChangeCost) || settings.tempoChangeCost < 0
        || !std::isfinite(settings.nonbeatObservationDivisor) || settings.nonbeatObservationDivisor < 1
        || settings.meters.empty() || input.beatLogits.size() != input.downbeatLogits.size()
        || (categorical && !input.beatLogits.empty()))
        return fail("Invalid rhythm observations or decoder settings.");
    if (settings.recoveredAuftaktTopology && (!categorical || input.framesPerSecond != 100))
        return fail("Recovered AUFTAKT topology requires 100 Hz categorical observations.");
    std::set<int> meters;
    for (auto meter : settings.meters)
        if (meter < 1 || meter > 10 || !meters.insert(meter).second) return fail("Meters must be distinct values from 1 to 10.");
    const auto finite = [](float value) { return std::isfinite(value); };
    if (!std::all_of(input.beatLogits.begin(), input.beatLogits.end(), finite)
        || !std::all_of(input.downbeatLogits.begin(), input.downbeatLogits.end(), finite))
        return fail("Non-finite rhythm activation; no partial grid was returned.");
    for (const auto& row : input.classProbabilities)
        if (!std::all_of(row.begin(), row.end(), [](float p) { return std::isfinite(p) && p >= 0 && p <= 1; })
            || std::abs(row[0] + row[1] + row[2] - 1) > 0.0001f)
            return fail("Invalid categorical rhythm probabilities; no partial grid returned.");
    const auto probabilities = [&](size_t frame)
    {
        if (categorical) return input.classProbabilities[frame];
        const float down = activation(input.downbeatLogits[frame]);
        const float beat = std::max(activation(input.beatLogits[frame]), down);
        return std::array<float, 3>{beat * (1 - down), beat * down, 1 - beat};
    };
    // Adjacent frame intervals must straddle the requested period. At 50 Hz,
    // 240 BPM alternates 12 and 13 frames; ceil-only would exclude that tempo.
    const double shortest = std::floor(60 * input.framesPerSecond / settings.maximumBpm);
    const double longest = std::ceil(60 * input.framesPerSecond / settings.minimumBpm);
    constexpr size_t invalid = std::numeric_limits<uint16_t>::max();
    if (!std::isfinite(shortest) || !std::isfinite(longest) || shortest < 2 || longest < shortest || longest >= invalid)
        return fail("Tempo range cannot be represented at this frame rate.");
    const int firstPeriod = (int) shortest, lastPeriod = (int) longest;
    std::vector<int> periodValues;
    if (settings.recoveredAuftaktTopology)
    {
        const double first = std::log(60 * input.framesPerSecond / std::ceil(60 * input.framesPerSecond / settings.minimumBpm));
        const double last = std::log(60 * input.framesPerSecond / std::floor(60 * input.framesPerSecond / settings.maximumBpm));
        const int proposals = std::min(50, (int) std::min(50.0, std::round(settings.maximumBpm - settings.minimumBpm)));
        if (proposals < 2) return fail("Recovered tempo proposal range is too narrow.");
        for (int i = 0; i < proposals; ++i)
        {
            const int period = (int) std::round(60 * input.framesPerSecond / std::exp(first + (last - first) * i / (proposals - 1)));
            if (periodValues.empty() || periodValues.back() != period) periodValues.push_back(period);
        }
    }
    else for (int period = firstPeriod; period <= lastPeriod; ++period) periodValues.push_back(period);
    const int periods = (int) periodValues.size();
    size_t periodSum = 0; for (auto period : periodValues) periodSum += (size_t) period;
    size_t meterSum = 0; for (auto meter : settings.meters) meterSum += (size_t) meter;
    if (periodSum > (invalid - 1) / meterSum) return fail("Rhythm state count exceeds the 16-bit trace representation.");
    const size_t stateCount = periodSum * meterSum, cellsCount = (size_t) periods * meterSum;
    const auto observed = [&](size_t frame)
    {
        if (!categorical) return std::max(input.beatLogits[frame], input.downbeatLogits[frame]) > 0;
        const auto p = probabilities(frame); return p[0] + p[1] > 0.5f;
    };
    size_t firstFrame = 0, endFrame = categorical ? input.classProbabilities.size() : input.beatLogits.size();
    if (input.signalPresent == false) return result;
    if (!settings.recoveredAuftaktTopology)
    {
        while (firstFrame < endFrame && !observed(firstFrame)) ++firstFrame;
        while (endFrame > firstFrame && !observed(endFrame - 1)) --endFrame;
    }
    const size_t frames = endFrame - firstFrame;
    if (frames > settings.maximumTraceBytes / sizeof(uint16_t) / cellsCount)
        return fail("Rhythm trace exceeds the requested memory budget; no grid was substituted.");
    if (frames < 2) return result;
    std::vector<Cell> cells; cells.reserve(cellsCount);
    std::vector<int> stateCell(stateCount), stateTick(stateCount);
    int offset = 0;
    std::vector<int> barPredecessorBases;
    for (auto meter : settings.meters)
    {
        const int base = (int) cells.size();
        barPredecessorBases.push_back(base + (meter - 1) * periods);
        for (int beat = 0; beat < meter; ++beat)
            for (int periodIndex = 0; periodIndex < periods; ++periodIndex)
            {
                const int period = periodValues[(size_t) periodIndex];
                const int index = (int) cells.size();
                cells.push_back({meter, beat, period, periodIndex, offset, base + ((beat + meter - 1) % meter) * periods});
                for (int tick = 0; tick < period; ++tick) { stateCell[(size_t) offset + tick] = index; stateTick[(size_t) offset + tick] = tick; }
                offset += period;
            }
    }
    std::vector<double> transition((size_t) periods * periods);
    for (int old = 0; old < periods; ++old)
    {
        double sum = 0;
        for (int next = 0; next < periods; ++next)
        {
            const double ratio = (double) periodValues[(size_t) next] / periodValues[(size_t) old];
            const double cost = settings.recoveredAuftaktTopology ? -180 * std::abs(ratio - 1)
                : -settings.tempoChangeCost * std::abs(std::log(ratio));
            transition[(size_t) old * periods + next] = cost; sum += std::exp(cost);
        }
        if (!settings.recoveredAuftaktTopology)
            for (int next = 0; next < periods; ++next) transition[(size_t) old * periods + next] -= std::log(sum);
    }
    std::vector<double> previous(stateCount, 0), current(stateCount);
    // Only beat-boundary states branch. All other predecessors are state-1,
    // so tracing costs frames*cells rather than frames*all phase states.
    std::vector<uint16_t> parents(frames * cellsCount, (uint16_t) invalid);
    double scale = 0;
    for (size_t frame = 0; frame < frames; ++frame)
    {
        const auto p = probabilities(frame + firstFrame);
        const double downbeat = categorical ? p[1] : activation(input.downbeatLogits[frame + firstFrame]);
        const double beat = categorical ? p[0] + p[1] : std::max((double) activation(input.beatLogits[frame + firstFrame]), downbeat);
        const double quiet = categorical ? logCategorical(p[2] / settings.nonbeatObservationDivisor) : logProbability(1 - beat);
        for (size_t c = 0; c < cells.size(); ++c)
        {
            const auto& cell = cells[c];
            for (int tick = 1; tick < cell.period; ++tick)
                current[(size_t) cell.offset + tick] = (frame == 0 ? 0 : previous[(size_t) cell.offset + tick - 1]) + quiet;
            double best = 0; int parent = (int) invalid;
            if (frame > 0)
            {
                best = -std::numeric_limits<double>::infinity();
                auto consider = [&](int previousBase, double meterFactor)
                {
                    for (int predecessor = 0; predecessor < periods; ++predecessor)
                    {
                        const auto& before = cells[(size_t) previousBase + predecessor];
                        const int state = before.offset + before.period - 1;
                        const double value = previous[(size_t) state] + transition[(size_t) predecessor * periods + cell.periodIndex] + meterFactor;
                        if (value > best) { best = value; parent = state; }
                    }
                };
                if (settings.recoveredAuftaktTopology && cell.beat == 0)
                    for (int base : barPredecessorBases) consider(base, std::log(cells[(size_t) base].meter == cell.meter ? 0.95 : 0.05));
                else consider(cell.previousCell, 0);
            }
            current[(size_t) cell.offset] = best + (categorical ? logCategorical(p[cell.beat == 0 ? 1 : 0])
                : logProbability(beat) + logProbability(cell.beat == 0 ? downbeat : 1 - downbeat));
            parents[frame * cellsCount + c] = (uint16_t) parent;
        }
        const double maximum = *std::max_element(current.begin(), current.end());
        if (!std::isfinite(maximum)) return fail("Categorical observations admit no valid rhythm path; no partial grid returned.");
        for (auto& value : current) value -= maximum;
        scale += maximum; previous.swap(current);
    }
    int winner = 0; double score = -std::numeric_limits<double>::infinity();
    for (auto meter : settings.meters)
    {
        double best = -std::numeric_limits<double>::infinity(); int state = 0;
        for (const auto& cell : cells) if (cell.meter == meter)
            for (int tick = 0; tick < cell.period; ++tick)
                if (previous[(size_t) cell.offset + tick] > best) { best = previous[(size_t) cell.offset + tick]; state = cell.offset + tick; }
        result.meterScores.emplace_back(meter, best + scale);
        if (best > score) { score = best; winner = state; }
    }
    for (size_t frame = frames; frame-- > 0;)
    {
        const auto& cell = cells[(size_t) stateCell[(size_t) winner]];
        if (stateTick[(size_t) winner] == 0)
        {
            const auto p = probabilities(frame + firstFrame);
            result.beats.push_back({(frame + firstFrame) / input.framesPerSecond, cell.beat + 1, cell.meter,
                                   categorical ? p[0] + p[1] : activation(input.beatLogits[frame + firstFrame]),
                                   categorical ? p[1] : activation(input.downbeatLogits[frame + firstFrame])});
            if (frame > 0) winner = parents[frame * cellsCount + (size_t) stateCell[(size_t) winner]];
        }
        else --winner;
    }
    std::reverse(result.beats.begin(), result.beats.end());
    // Recovered categorical observations can support low-probability beats.
    // Their admission interval belongs to the PCM frontend, not a 0.5 model cutoff.
    if (settings.recoveredAuftaktTopology) return result;
    // The latent path exists through padding and silence too. Admit inferred
    // missing beats only between supported events, never extrapolate its ends.
    const auto supported = [](const auto& event) { return std::max(event.beatActivation, event.downbeatActivation) > 0.5f; };
    const auto first = std::find_if(result.beats.begin(), result.beats.end(), supported);
    if (first == result.beats.end()) result.beats.clear();
    else
    {
        const auto last = std::find_if(result.beats.rbegin(), result.beats.rend(), supported).base();
        result.beats.erase(last, result.beats.end());
        result.beats.erase(result.beats.begin(), result.beats.begin() + std::distance(result.beats.begin(), first));
    }
    return result;
}
}
