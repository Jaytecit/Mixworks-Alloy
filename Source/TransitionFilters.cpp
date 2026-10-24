#include "Engine.h"
#include <cmath>

namespace mixworks
{
namespace
{
const juce::StringArray filterIds {"none", "high-to-low", "low-to-high"};
}
juce::Result TransitionFilterSettings::validate() const
{
    if ((int) mode < 0 || (int) mode >= filterIds.size() || !std::isfinite(strength) || strength < 0 || strength > 1)
        return juce::Result::fail("Invalid transition filter mode or depth.");
    return juce::Result::ok();
}
juce::var transitionFilterToJson(const TransitionFilterSettings& setting)
{
    auto* obj = new juce::DynamicObject(); obj->setProperty("mode", filterIds[(int) setting.mode]); obj->setProperty("strength", setting.strength);
    return juce::var(obj);
}
juce::Result readTransitionFilter(const juce::var& data, TransitionFilterSettings& result)
{
    const auto value = data["strength"];
    if (!data.isObject() || !data["mode"].isString() || !filterIds.contains(data["mode"].toString())
        || !(value.isInt() || value.isInt64() || value.isDouble())) return juce::Result::fail("Invalid transition filter.");
    TransitionFilterSettings setting {(TransitionFilterMode) filterIds.indexOf(data["mode"].toString()), (double) value};
    const auto valid = setting.validate(); if (valid.failed()) return valid;
    result = setting; return juce::Result::ok();
}
double TransitionFilterEnvelope::amountAt(double time) const
{
    if (!setting.active() || end <= begin) return 0;
    const double progress = juce::jlimit(0., 1., (time - begin) / (end - begin));
    return setting.strength * (incoming ? 1 - progress : progress);
}
juce::Result Project::validateTransitionFilter(const TransitionLink& link) const
{
    const auto valid = link.filter.validate(); if (valid.failed() || !link.filter.active()) return valid;
    const auto* out = clipFor(link.outgoing); const auto* in = clipFor(link.incoming);
    // The parser validates syntax before loading source clocks/PCM.
    if (!out || !in || !out->media || !in->media) return juce::Result::ok();
    const auto range = transitionRange(link);
    if (range.getLength() <= 1e-6 || range.getStart() < std::max(out->start, in->start) - 1e-7
        || range.getEnd() > std::min(out->end(), in->end()) + 1e-7)
        return juce::Result::fail("Keep the transition inside both songs before applying a filter.");
    return juce::Result::ok();
}
juce::Result Project::setTransitionFilter(const juce::String& incoming, const TransitionFilterSettings& setting)
{
    auto valid = setting.validate(); if (valid.failed()) return valid;
    if (hasPendingPlacement()) return juce::Result::fail("Wait for track placement to finish.");
    valid = validateTransitionLinks(); if (valid.failed()) return valid;
    const auto* current = transitionFor(incoming); if (!current) return juce::Result::fail("Select a transition first.");
    if (clipTransitionLocked(current->outgoing) || clipTransitionLocked(incoming))
        return juce::Result::fail("Unlock transitions on both participating tracks before changing their filter.");
    auto candidate = *this; candidate.transitionLinks[(size_t) (current - transitionLinks.data())].filter = setting;
    valid = candidate.validateTransitionLinks(); if (valid.failed()) return valid;
    valid = validateTransitionLocks(candidate); if (valid.failed()) return valid;
    *this = std::move(candidate); return juce::Result::ok();
}

void TransitionFilterProcessor::Stage::reset()
{
    low = {}; band = {}; lastCutoff = -1; g = 0; h = 1;
}
void TransitionFilterProcessor::reset()
{
    for (auto& stage : stages) stage.reset(); rate = 0;
}
void TransitionFilterProcessor::process(juce::AudioBuffer<float>& audio, int first, int last, double sr, double at,
    const std::array<TransitionFilterEnvelope, 2>& envelopes)
{
    if (rate != sr) { reset(); rate = sr; }
    for (size_t side = 0; side < stages.size(); ++side)
    {
        auto& stage = stages[side]; const auto& envelope = envelopes[side];
        if (!envelope.setting.active() || envelope.end <= envelope.begin) { stage.reset(); continue; }
        const bool highPass = envelope.highPass();
        if (stage.highPass != highPass) { stage.reset(); stage.highPass = highPass; }
        // Independent Mixworks design: Butterworth two-pole TPT SVF, logarithmic
        // 20 Hz..min(20 kHz, .45*rate), limited to 80% travel. A neutral crossfade
        // keeps exact dry endpoints, while continuously fed state avoids cold entry.
        const double upper = std::min(20000., sr * .45), ratio = upper / 20.;
        constexpr double damping = 1.4142135623730951;
        for (int i = first; i < last; ++i)
        {
            const double amount = envelope.amountAt(at + i / sr), wet = std::min(1., amount / .02);
            const double cutoff = highPass ? 20 * std::pow(ratio, .8 * amount) : upper * std::pow(ratio, -.8 * amount);
            if (cutoff != stage.lastCutoff)
            {
                stage.g = std::tan(juce::MathConstants<double>::pi * cutoff / sr);
                stage.h = 1 / (1 + damping * stage.g + stage.g * stage.g); stage.lastCutoff = cutoff;
            }
            for (int channel = 0; channel < 2; ++channel)
            {
                auto* samples = audio.getWritePointer(channel); const double dry = samples[i];
                const double high = (dry - (damping + stage.g) * stage.band[(size_t) channel] - stage.low[(size_t) channel]) * stage.h;
                const double band = stage.g * high + stage.band[(size_t) channel];
                const double low = stage.g * band + stage.low[(size_t) channel];
                stage.band[(size_t) channel] = stage.g * high + band;
                stage.low[(size_t) channel] = stage.g * band + low;
                // Do not round-trip dry float through arithmetic at neutral.
                if (wet > 0) samples[i] = (float) (dry + wet * ((highPass ? high : low) - dry));
            }
        }
    }
}
void CutoffFilterProcessor::process(juce::AudioBuffer<float>& audio, int first, int frames, double sr, float value)
{
    if (!std::isfinite(sr) || sr <= 0 || frames <= 0) return;
    const double target = std::isfinite(value) ? juce::jlimit(-1., 1., (double) value) : 0;
    if (rate != sr) { reset(); rate = sr; smooth = target; }
    const double coefficient = 1 - std::exp(-1 / (.005 * sr));
    constexpr double damping = 1.4142135623730951;
    for (int i = first; i < first + frames; ++i)
    {
        smooth += coefficient * (target - smooth);
        if (std::abs(target - smooth) < 1e-8) smooth = target;
        const double lowHz = 20000 + std::min(0., smooth) * 19980;
        const double highHz = 20 + std::max(0., smooth) * 19980;
        const double wet = juce::jlimit(0., 1., smooth < 0 ? (20000 - lowHz) / 1000 : (highHz - 20) / 20);
        for (int side = 0; side < 2; ++side)
        {
            auto& stage = stages[(size_t) side];
            const double hz = std::min(side == 0 ? lowHz : highHz, sr * .45);
            if (hz != stage.lastCutoff)
            {
                stage.lastCutoff = hz; stage.g = std::tan(juce::MathConstants<double>::pi * hz / sr);
                stage.h = 1 / (1 + damping * stage.g + stage.g * stage.g);
            }
        }
        for (int ch = 0; ch < std::min(2, audio.getNumChannels()); ++ch)
        {
            auto* samples = audio.getWritePointer(ch); const double dry = samples[i]; double filtered = dry;
            // Independent states stay warm through neutral and changes of direction.
            for (int side = 0; side < 2; ++side)
            {
                auto& stage = stages[(size_t) side];
                const double high = (dry - (damping + stage.g) * stage.band[(size_t) ch] - stage.low[(size_t) ch]) * stage.h;
                const double b = stage.g * high + stage.band[(size_t) ch], low = stage.g * b + stage.low[(size_t) ch];
                stage.band[(size_t) ch] = stage.g * high + b; stage.low[(size_t) ch] = stage.g * b + low;
                if ((smooth < 0 && side == 0) || (smooth > 0 && side == 1)) filtered = side == 0 ? low : high;
            }
            // Exact dry centre, including float bit patterns, once smoothing settles.
            if (wet > 0) samples[i] = (float) (dry + wet * (filtered - dry));
        }
    }
}

}
