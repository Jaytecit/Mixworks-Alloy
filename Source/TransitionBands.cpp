#include "Model.h"
#include <cmath>

namespace mixworks
{
namespace
{
const juce::StringArray modeIds {"none", "crossfade", "fade-in", "fade-out", "swap", "manual", "swap-fade", "constant-power"};
TransitionBandEnvelope compileBand(const TransitionBandSettings& setting, juce::Range<double> range, const TempoMap& clock, bool incoming, int band)
{
    if (!setting.generated() || (setting.mode == TransitionBandMode::fadeIn && !incoming)
        || (setting.mode == TransitionBandMode::fadeOut && incoming)) return {};
    double begin = range.getStart(), end = range.getEnd();
    if (setting.swapping())
    {
        const double first = clock.beatsAt(begin), span = clock.beatsAt(end) - first;
        const double middle = first + span * setting.position + (incoming ? 1 : -1) * setting.gapBeats / 2;
        // Swap has a short anti-click ramp; Bass Swap fade uses one musical beat.
        const double width = setting.mode == TransitionBandMode::swap && band > 0 ? 0 : setting.mode == TransitionBandMode::swapFade ? std::min(1.0, span / 8)
            : std::min(clock.bpmAt(clock.timeAtBeat(middle)) / 60 * 0.004, span / 8);
        begin = clock.timeAtBeat(middle - width / 2); end = clock.timeAtBeat(middle + width / 2);
    }
    return {begin, end, (float) setting.strength, incoming, setting.mode == TransitionBandMode::constantPower, setting.mode == TransitionBandMode::swap && band > 0};
}
}
juce::Result TransitionBandSettings::validate(int band) const
{
    if (band < 0 || band >= 4 || (int) mode < 0 || (int) mode >= modeIds.size()
        || (mode == TransitionBandMode::swapFade && band != 1)
        || (mode == TransitionBandMode::constantPower && band != 0)
        || !std::isfinite(strength) || strength < 0 || strength > 1
        || !std::isfinite(gapBeats) || gapBeats < 0 || gapBeats > 32
        || !std::isfinite(position) || position <= 0 || position >= 1)
        return juce::Result::fail("Invalid transition band mode, strength, gap or position.");
    return juce::Result::ok();
}
float TransitionBandEnvelope::gainAt(double time) const
{
    if (strength == 0) return 1;
    if (instant) return time < begin ? (rising ? 1 - strength : 1) : (rising ? 1 : 1 - strength);
    if (end <= begin) return 1;
    const double progress = juce::jlimit(0.0, 1.0, (time - begin) / (end - begin));
    const double linear = rising ? progress : 1 - progress;
    const double gain = constantPower ? std::sin(linear * juce::MathConstants<double>::halfPi) : linear;
    return (float) (1 - strength * (1 - gain));
}
float TransitionPlayback::gainAt(int band, double time) const
{
    if (band < 0 || band >= 4) return 1;
    return pairs[0][(size_t) band].gainAt(time) * pairs[1][(size_t) band].gainAt(time);
}
juce::Result Project::validateTransitionBands(const TransitionLink& link) const
{
    const TempoMap fixed {bpm, {}}; const auto& clock = followTrackTempo ? tempo : fixed;
    for (int band = 0; band < 4; ++band) if (link.bands[(size_t) band])
    {
        const auto& setting = *link.bands[(size_t) band]; const auto valid = setting.validate(band); if (valid.failed()) return valid;
        if (band == 1 && link.bassSwap)
            return juce::Result::fail("Conflicting legacy and generated bass ownership.");
        const auto* out = clipFor(link.outgoing); const auto* in = clipFor(link.incoming);
        // parseProject checks settings before it loads media; runtime validation adds geometry.
        if (!setting.generated() || !out || !in || !out->media || !in->media) continue;
        const auto range = transitionRange(link);
        if (range.getLength() <= 1e-6 || range.getStart() < std::max(out->start, in->start) - 1e-7
            || range.getEnd() > std::min(out->end(), in->end()) + 1e-7)
            return juce::Result::fail("Keep this transition inside both songs before applying band controls.");
        if (setting.swapping()) for (bool incoming : {false, true})
        {
            const auto curve = compileBand(setting, range, clock, incoming, band);
            if (curve.begin < range.getStart() - 1e-7 || curve.end > range.getEnd() + 1e-7)
                return juce::Result::fail("This swap gap cannot fit at that position. Use a smaller gap, move the swap or lengthen the transition.");
        }
    }
    return juce::Result::ok();
}
TransitionPlayback Project::transitionPlaybackFor(const Clip& clip) const
{
    TransitionPlayback result;
    const TempoMap fixed {bpm, {}}; const auto& clock = followTrackTempo ? tempo : fixed;
    for (const auto& link : transitionLinks)
    {
        const bool incoming = link.incoming == clip.id;
        if (!incoming && link.outgoing != clip.id) continue;
        // A missing partner does not establish a zero-second cut. This also
        // keeps partial rendering contexts from manufacturing a frame at zero.
        if (!clipFor(link.outgoing) || !clipFor(link.incoming)) continue;
        // A valid chain has at most one incoming and one outgoing relationship.
        auto& pair = result.pairs[incoming ? 0 : 1];
        const auto range = transitionRange(link);
        if (incoming) result.startCut = range.getStart(); else result.endCut = range.getEnd();
        result.filters[incoming ? 0 : 1] = {range.getStart(), range.getEnd(), link.filter, incoming};
        for (size_t band = 0; band < 4; ++band) if (link.bands[band]) pair[band] = compileBand(*link.bands[band], range, clock, incoming, (int) band);
    }
    return result;
}
juce::Result Project::setTransitionBand(const juce::String& incoming, int band, const TransitionBandSettings& setting)
{
    const auto valid = setting.validate(band); if (valid.failed()) return valid;
    if (hasPendingPlacement()) return juce::Result::fail("Wait for track placement to finish.");
    const auto links = validateTransitionLinks(); if (links.failed()) return links;
    const auto* current = transitionFor(incoming); if (!current) return juce::Result::fail("Select a transition first.");
    if (clipTransitionLocked(current->outgoing) || clipTransitionLocked(current->incoming))
        return juce::Result::fail("Unlock transitions on both participating tracks before changing their bands.");
    Project candidate = *this;
    auto& link = candidate.transitionLinks[(size_t) (current - transitionLinks.data())];
    const auto range = transitionRange(*current);
    if (band == 1) candidate.clearTransitionBass(link, setting.mode == TransitionBandMode::manual);
    if (band == 0 && setting.mode != TransitionBandMode::manual)
    {
        auto* out = candidate.clipFor(link.outgoing); auto* in = candidate.clipFor(link.incoming);
        // Replace the old pair's volume treatment, retaining its source-owned frame boundaries.
        out->fadeOut = in->fadeIn = 0;
        out->fadeOutAnchor = out->sourceElapsed(range.getEnd() - out->start);
        in->fadeInAnchor = in->sourceElapsed(range.getStart() - in->start);
    }
    link.bands[(size_t) band] = setting;
    const auto checked = candidate.validateTransitionLinks(); if (checked.failed()) return checked;
    const auto protection = validateTransitionLocks(candidate); if (protection.failed()) return protection;
    *this = std::move(candidate); return juce::Result::ok();
}
juce::var transitionBandsToJson(const Project::TransitionLink& link)
{
    juce::Array<juce::var> bands;
    for (const auto& setting : link.bands)
    {
        if (!setting) { bands.add(juce::var()); continue; }
        auto* value = new juce::DynamicObject(); value->setProperty("mode", modeIds[(int) setting->mode]);
        value->setProperty("strength", setting->strength); value->setProperty("gapBeats", setting->gapBeats); value->setProperty("position", setting->position);
        bands.add(juce::var(value));
    }
    return bands;
}
juce::Result readTransitionBands(const juce::var& value, Project::TransitionLink& link)
{
    if (!value.isArray() || value.size() != 4) return juce::Result::fail("Invalid transition bands.");
    for (int band = 0; band < 4; ++band)
    {
        const auto& item = value[band]; if (item.isVoid()) continue;
        const auto numeric = [](const juce::var& x) { return x.isInt() || x.isInt64() || x.isDouble(); };
        if (!item.isObject() || !item["mode"].isString() || !modeIds.contains(item["mode"].toString())
            || !numeric(item["strength"]) || !numeric(item["gapBeats"]) || !numeric(item["position"])) return juce::Result::fail("Invalid transition band settings.");
        TransitionBandSettings setting {(TransitionBandMode) modeIds.indexOf(item["mode"].toString()), (double) item["strength"], (double) item["gapBeats"], (double) item["position"]};
        const auto valid = setting.validate(band); if (valid.failed()) return valid;
        link.bands[(size_t) band] = setting;
    }
    return juce::Result::ok();
}
}
