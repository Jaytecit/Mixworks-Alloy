#include "EnergySections.h"
#include <cmath>
#include <set>

namespace mixworks
{
std::optional<double> sourceEnergy(const Media& media, double begin, double end)
{
    if (!std::isfinite(media.sampleRate) || media.sampleRate <= 0 || media.frames() <= 0
        || !std::isfinite(begin) || !std::isfinite(end) || begin < -1e-7 || end > media.duration() + 1e-7 || end <= begin) return {};
    const int bucket = std::max(1, media.frames() / 4000);
    if (media.waveformRms.size() != (size_t) ((media.frames() + (int64_t) bucket - 1) / bucket)) return {};
    const double first = std::max(0., begin * media.sampleRate), last = std::min((double) media.frames(), end * media.sampleRate);
    if (last <= first) return {};
    double power = 0;
    for (int i = (int) std::floor(first / bucket); i < (int) std::ceil(last / bucket); ++i)
    {
        const double rms = media.waveformRms[(size_t) i];
        if (!std::isfinite(rms) || rms < 0) return {};
        const double weight = std::max(0., std::min(last, (i + 1.) * bucket) - std::max(first, (double) i * bucket));
        power += rms * rms * weight / (last - first);
    }
    return power;
}
std::vector<EnergySection> detectEnergySections(const Media& media)
{
    const double bpm = media.bpm(), anchor = media.downbeatOrigin(), duration = media.duration();
    if (!std::isfinite(bpm) || bpm < 20 || bpm > 400 || !std::isfinite(anchor) || anchor < 0
        || !std::isfinite(duration) || duration <= 0 || anchor >= duration || !std::isfinite(media.sampleRate) || media.sampleRate <= 0
        || (media.analysis.variableTempo && media.manualBpm <= 0) || media.waveformRms.empty()) return {};
    const double bar = 240 / bpm;
    const double first = anchor - std::floor(anchor / bar) * bar;
    const double count = std::floor((duration - first) / bar);
    if (count < 16 || count > 32768) return {};
    const int bars = (int) count, bucket = std::max(1, media.frames() / 4000);
    // Reject incomplete old displays rather than manufacturing quiet sections.
    if (media.waveformRms.size() != (size_t) ((media.frames() + (int64_t) bucket - 1) / bucket)) return {};
    for (float rms : media.waveformRms) if (!std::isfinite(rms) || rms < 0) return {};
    std::vector<double> power((size_t) bars, 0);
    for (int b = 0; b < bars; ++b)
    {
        const auto energy = sourceEnergy(media, first + b * bar, first + (b + 1) * bar);
        if (!energy) return {};
        power[(size_t) b] = *energy;
    }
    std::vector<EnergySection> candidates;
    for (int b = 4; b <= bars - 4; ++b)
    {
        double before = 0, after = 0;
        for (int i = 0; i < 4; ++i) { before += power[(size_t) (b - i - 1)]; after += power[(size_t) (b + i)]; }
        // Absolute silence/quantisation noise cannot establish a meaningful section.
        if (std::max(before, after) < 4e-8) continue;
        const double contrast = 10 * std::log10(std::max(after, 4e-10) / std::max(before, 4e-10));
        if (std::abs(contrast) >= 3) candidates.push_back({first + b * bar, contrast});
    }
    std::stable_sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) { return std::abs(a.contrastDb) > std::abs(b.contrastDb); });
    std::vector<EnergySection> sections;
    std::set<double> accepted;
    for (const auto& candidate : candidates)
    {
        const auto next = accepted.lower_bound(candidate.sourceSeconds);
        const double spacing = 8 * bar - 1e-7;
        if (next != accepted.end() && *next - candidate.sourceSeconds < spacing) continue;
        if (next != accepted.begin() && candidate.sourceSeconds - *std::prev(next) < spacing) continue;
        accepted.insert(candidate.sourceSeconds); sections.push_back(candidate);
    }
    std::sort(sections.begin(), sections.end(), [](const auto& a, const auto& b) { return a.sourceSeconds < b.sourceSeconds; });
    return sections;
}
std::optional<SectionTransitionChoice> chooseSectionTransition(const Clip& outgoing, const Clip& incoming, double maxBeats)
{
    const auto clock = [](const Clip& clip) { return clip.tempoSourceBpm > 0 && clip.media->manualBpm <= 0 ? clip.tempoSourceBpm : clip.media->bpm(); };
    const double outBpm = clock(outgoing), inBpm = clock(incoming);
    if (!std::isfinite(outBpm) || !std::isfinite(inBpm) || outBpm <= 0 || inBpm <= 0 || !std::isfinite(maxBeats) || maxBeats < 32 || maxBeats > 256) return {};
    const double capacity = std::min({maxBeats, outgoing.length * outgoing.rate * outBpm / 60, incoming.length * incoming.rate * inBpm / 60});
    if (capacity < 32 - 1e-7) return {};
    SectionTransitionChoice best;
    const auto consider = [&](double overlap, int matches)
    {
        if (!std::isfinite(overlap) || overlap < 32 - 1e-7 || overlap > capacity + 1e-7) return;
        if (matches > best.matchedSections || (matches == best.matchedSections && overlap < best.overlapBeats - 1e-7)) best = {overlap, matches};
    };
    // Only the bounded tail/head windows participate; never form a full-track cross product.
    std::vector<double> outChanges, inChanges;
    if (!outgoing.reversed && outgoing.loopSpan <= 0)
        for (const auto& section : detectEnergySections(*outgoing.media))
        {
            const double remaining = (outgoing.audioSourceElapsed(outgoing.length) - (section.sourceSeconds - outgoing.offset)) * outBpm / 60;
            if (remaining >= -1e-7 && remaining <= capacity + 1e-7) outChanges.push_back(remaining);
        }
    if (!incoming.reversed && incoming.loopSpan <= 0)
        for (const auto& section : detectEnergySections(*incoming.media))
        {
            const double offset = (section.sourceSeconds - incoming.offset - incoming.audioSourceElapsed(0)) * inBpm / 60;
            if (offset >= 0 && offset <= capacity + 1e-7) inChanges.push_back(offset);
        }
    for (double remaining : outChanges)
    {
        consider(remaining, 1);
        for (double offset : inChanges) consider(remaining + offset, 2);
    }
    for (double offset : inChanges) consider(offset, 1);
    return best;
}
void paintEnergySections(juce::Graphics& g, const Clip& clip, juce::Rectangle<float> bounds, double scale)
{
    // Loops/reverse require occurrence-specific section identities; leave them unknown.
    if (clip.reversed || clip.loopSpan > 0) return;
    const auto sections = detectEnergySections(*clip.media);
    if (sections.empty()) return;
    juce::Graphics::ScopedSaveState saved(g); g.reduceClipRegion(bounds.toNearestInt());
    for (size_t i = 0; i <= sections.size(); ++i)
    {
        const double begin = i == 0 ? 0 : sections[i - 1].sourceSeconds;
        const double end = i == sections.size() ? clip.media->duration() : sections[i].sourceSeconds;
        const float x = (float) ((clip.start + clip.audioTimelineTime(begin - clip.offset)) * scale);
        const float next = (float) ((clip.start + clip.audioTimelineTime(end - clip.offset)) * scale);
        auto strip = juce::Rectangle<float>(x, bounds.getBottom() - 16, next - x, 12).getIntersection(bounds);
        if (strip.isEmpty()) continue;
        g.setColour(juce::Colour(i % 2 == 0 ? 0xff5894a6 : 0xff9c8149).withAlpha(.8f)); g.fillRect(strip);
        if (i > 0) { g.setColour(juce::Colour(0xffc7d7da).withAlpha(.25f)); g.drawVerticalLine((int) std::round(x), bounds.getY() + 24, strip.getY()); }
        if (strip.getWidth() > 48) { g.setColour(juce::Colour(0xffeef0ed)); g.setFont(juce::FontOptions(9)); g.drawText("S" + juce::String((int) i + 1) + " ?", strip.reduced(3, 0), juce::Justification::centredLeft); }
    }
}
}
