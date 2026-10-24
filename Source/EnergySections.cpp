#include "EnergySections.h"
#include <cmath>
#include <set>
#include <limits>

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
        // A two-bar break/return can disappear inside a four-bar average.
        // Require both bars on each side to corroborate a strong short change;
        // an isolated fill or spike must not establish a section by itself.
        const double shortBefore = power[(size_t)b - 2] + power[(size_t)b - 1];
        const double shortAfter = power[(size_t)b] + power[(size_t)b + 1];
        const double shortContrast = 10 * std::log10(std::max(shortAfter, 2e-10) / std::max(shortBefore, 2e-10));
        const bool sustained = shortContrast > 0
            ? std::min(power[(size_t)b], power[(size_t)b + 1]) >= 4 * std::max(power[(size_t)b - 2], power[(size_t)b - 1])
            : std::min(power[(size_t)b - 2], power[(size_t)b - 1]) >= 4 * std::max(power[(size_t)b], power[(size_t)b + 1]);
        const double evidence = sustained && std::abs(shortContrast) >= 6 && std::abs(shortContrast) * .8 > std::abs(contrast)
            ? shortContrast * .8 : contrast;
        if (std::abs(evidence) >= 3) candidates.push_back({first + b * bar, evidence});
    }
    // Near-equal break/return evidence favours the return, so a brief fill does
    // not suppress the following full-energy section merely by coming first.
    std::stable_sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b)
    { return std::abs(a.contrastDb) * (a.contrastDb > 0 ? 1.02 : 1) > std::abs(b.contrastDb) * (b.contrastDb > 0 ? 1.02 : 1); });
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
std::vector<EnergySection> clipEnergySections(const Clip& clip)
{
    const auto* markers = clip.sectionMarkers ? &*clip.sectionMarkers : clip.media && clip.media->sectionTemplate ? &*clip.media->sectionTemplate : nullptr;
    if (!markers) return clip.media && clip.media->sectionTemplateError.isEmpty() ? detectEnergySections(*clip.media) : std::vector<EnergySection>{};
    std::vector<EnergySection> result; result.reserve(markers->size());
    for (const auto& marker : *markers) result.push_back({marker.sourceSeconds, marker.contrastDb, marker.id, marker.manual, marker.label});
    return result;
}
juce::Result Project::setSectionMarkerTime(const juce::String& id, int index, double sourceSeconds, std::optional<juce::String> label)
{
    auto* clip = clipFor(id);
    if (!clip || !clip->media || !clip->media->hasPlayback() || clip->media->sourceDetailsPending || hasPendingPlacement())
        return juce::Result::fail("Wait for this segment's source before editing section flags.");
    if (clipTransitionLocked(id)) return juce::Result::fail("Unlock the transition before changing its section flags.");
    if (clip->reversed || clip->loopSpan > 0) return juce::Result::fail("Section-flag editing currently requires a forward segment without a loop. Saved annotations were kept.");
    const auto sections = clipEnergySections(*clip);
    const bool adding = index == -1;
    if (index < -1 || (!adding && index >= (int) sections.size()) || !std::isfinite(sourceSeconds)) return juce::Result::fail("Select an available section flag.");
    if (label) { *label = label->trim(); if (label->length() > 128 || label->containsAnyOf("\r\n\t")) return juce::Result::fail("Use a single-line section label of up to 128 characters."); }
    if (adding)
    {
        if (sections.size() >= 4096) return juce::Result::fail("This source already has the maximum number of section boundaries.");
        index = (int) std::distance(sections.begin(), std::lower_bound(sections.begin(), sections.end(), sourceSeconds,
            [](const auto& section, double at) { return section.sourceSeconds < at; }));
    }
    const double low = index == 0 ? .000001 : sections[(size_t) index - 1].sourceSeconds + .000001;
    const int next = index + (adding ? 0 : 1);
    const double high = next == (int) sections.size() ? clip->media->duration() - .000001 : sections[(size_t) next].sourceSeconds - .000001;
    if (sourceSeconds < low || sourceSeconds > high) return juce::Result::fail("Keep the section flag between its neighbours and inside the source.");
    const double local = clip->audioTimelineTime(sourceSeconds - clip->offset);
    if (local < 0 || local >= clip->length) return juce::Result::fail("Keep this flag inside the segment's current source range. Trimmed annotations remain saved.");
    const bool moved = adding || std::abs(sourceSeconds - sections[(size_t) index].sourceSeconds) >= 1.e-9;
    if (!moved && (!label || *label == sections[(size_t) index].label)) return juce::Result::ok();
    if (!clip->sectionMarkers)
    {
        clip->sectionMarkers.emplace();
        for (const auto& section : sections) clip->sectionMarkers->push_back({section.id.isEmpty() ? juce::Uuid().toString() : section.id, section.sourceSeconds, section.contrastDb, section.manual, section.label});
    }
    if (adding) clip->sectionMarkers->insert(clip->sectionMarkers->begin() + index, {juce::Uuid().toString(), sourceSeconds, 0, true, label.value_or(juce::String())});
    else
    {
        auto& marker = (*clip->sectionMarkers)[(size_t) index];
        if (moved) { marker.sourceSeconds = sourceSeconds; marker.manual = true; marker.contrastDb = 0; }
        if (label) marker.label = *label;
    }
    return juce::Result::ok();
}
juce::Result Project::resetSectionMarkers(const juce::String& id)
{
    auto* clip = clipFor(id); if (!clip) return juce::Result::fail("Select an available segment.");
    if (hasPendingPlacement() || clipTransitionLocked(id)) return juce::Result::fail("Wait for preparation and unlock this transition before resetting its section flags.");
    clip->sectionMarkers.reset(); return juce::Result::ok();
}
std::optional<SectionTransitionChoice> chooseSectionTransition(const Clip& outgoing, const Clip& incoming, double maxBeats)
{
    const auto clock = [](const Clip& clip) { return clip.tempoSourceBpm > 0 && clip.media->manualBpm <= 0 ? clip.tempoSourceBpm : clip.media->bpm(); };
    const double outBpm = clock(outgoing), inBpm = clock(incoming);
    if (!std::isfinite(outBpm) || !std::isfinite(inBpm) || outBpm <= 0 || inBpm <= 0 || !std::isfinite(maxBeats) || maxBeats < 32 || maxBeats > 256) return {};
    const double capacity = std::min(outgoing.length * outgoing.rate * outBpm / 60, incoming.length * incoming.rate * inBpm / 60);
    if (capacity < 32 - 1e-7) return {};
    SectionTransitionChoice best;
    const auto consider = [&](double overlap, int matches, std::optional<std::array<double, 2>> boundaries = {})
    {
        if (!std::isfinite(overlap) || overlap < 32 - 1e-7 || overlap > capacity + 1e-7) return;
        if (matches < 2 && overlap > maxBeats + 1e-7) return;
        const bool preferred = overlap <= maxBeats + 1e-7, bestPreferred = best.overlapBeats <= maxBeats + 1e-7;
        if (matches > best.matchedSections || (matches == best.matchedSections
            && ((preferred && !bestPreferred) || (preferred == bestPreferred && overlap < best.overlapBeats - 1e-7))))
            best = {overlap, matches, boundaries};
    };
    // Bound the tail/head by usable overlap, retaining at most 256 nearest
    // changes per side. The normal limit is a preference for genuine pairs.
    std::vector<std::pair<double, double>> outChanges, inChanges;
    if (!outgoing.reversed && outgoing.loopSpan <= 0)
        for (const auto& section : clipEnergySections(outgoing))
        {
            const double remaining = (outgoing.audioSourceElapsed(outgoing.length) - (section.sourceSeconds - outgoing.offset)) * outBpm / 60;
            if (remaining >= -1e-7 && remaining <= capacity + 1e-7) outChanges.emplace_back(remaining, section.sourceSeconds);
        }
    if (!incoming.reversed && incoming.loopSpan <= 0)
        for (const auto& section : clipEnergySections(incoming))
        {
            const double offset = (section.sourceSeconds - incoming.offset - incoming.audioSourceElapsed(0)) * inBpm / 60;
            if (offset >= 0 && offset <= capacity + 1e-7) inChanges.emplace_back(offset, section.sourceSeconds);
        }
    std::sort(outChanges.begin(), outChanges.end()); std::sort(inChanges.begin(), inChanges.end());
    if (outChanges.size() > 256) outChanges.resize(256);
    if (inChanges.size() > 256) inChanges.resize(256);
    for (const auto& [remaining, outSource] : outChanges)
    {
        consider(remaining, 1);
        for (const auto& [offset, inSource] : inChanges)
            if (remaining >= 4 && offset >= 4) consider(remaining + offset, 2, std::array<double, 2>{outSource, inSource});
    }
    for (const auto& change : inChanges) consider(change.first, 1);
    return best;
}
namespace
{
std::optional<double> sharedChange(const Clip& out, const Clip& in, const TempoMap& clock, juce::Range<double> range)
{
    if (out.reversed || in.reversed || out.loopSpan > 0 || in.loopSpan > 0 || range.getLength() <= 0) return {};
    const auto a = clipEnergySections(out), b = clipEnergySections(in);
    std::optional<double> best; double distance = std::numeric_limits<double>::max();
    const double middle = (clock.beatsAt(range.getStart()) + clock.beatsAt(range.getEnd())) / 2;
    for (const auto& x : a)
    {
        const double at = out.start + out.audioTimelineTime(x.sourceSeconds - out.offset);
        if (at <= range.getStart() + 1e-7 || at >= range.getEnd() - 1e-7) continue;
        for (const auto& y : b)
        {
            const double other = in.start + in.audioTimelineTime(y.sourceSeconds - in.offset);
            if (std::abs(clock.beatsAt(at) - clock.beatsAt(other)) > 1e-5) continue;
            const double next = std::abs(clock.beatsAt(at) - middle);
            if (next < distance) { best = at; distance = next; }
        }
    }
    return best;
}
}
std::optional<juce::Range<double>> sectionTransitionWindow(const Clip& out, const Clip& in, const TempoMap& clock)
{
    const double low = std::max(out.start, in.start), high = std::min(out.end(), in.end());
    const auto shared = sharedChange(out, in, clock, {low, high}); if (!shared) return {};
    const double phase = clock.beatsAt(*shared);
    const double first = phase + std::ceil((clock.beatsAt(low) - phase) / 4 - 1e-8) * 4;
    const double last = phase + std::floor((clock.beatsAt(high) - phase) / 4 + 1e-8) * 4;
    if (last - first < 32 - 1e-7 || phase <= first + 1e-7 || phase >= last - 1e-7) return {};
    return juce::Range<double>{clock.timeAtBeat(first), clock.timeAtBeat(last)};
}
double sectionSwapPosition(const Project& project, const Project::TransitionLink& link)
{
    const auto* out = project.clipFor(link.outgoing); const auto* in = project.clipFor(link.incoming);
    if (!out || !in) return .5;
    const auto range = project.transitionRange(link); const auto clock = project.gridClock();
    const auto at = sharedChange(*out, *in, clock, range); if (!at) return .5;
    const double first = clock.beatsAt(range.getStart()), last = clock.beatsAt(range.getEnd());
    return (clock.beatsAt(*at) - first) / (last - first);
}
void paintEnergySections(juce::Graphics& g, const Clip& clip, juce::Rectangle<float> bounds, double scale)
{
    // Loops/reverse require occurrence-specific section identities; leave them unknown.
    if (clip.reversed || clip.loopSpan > 0) return;
    const auto sections = clipEnergySections(clip);
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
        if (strip.getWidth() > 48) { g.setColour(juce::Colour(0xffeef0ed)); g.setFont(juce::FontOptions(9)); g.drawText("S" + juce::String((int) i + 1) + (i > 0 && sections[i - 1].label.isNotEmpty() ? " / " + sections[i - 1].label : juce::String()), strip.reduced(3, 0), juce::Justification::centredLeft); }
    }
}
}
