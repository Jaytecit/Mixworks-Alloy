#include "Engine.h"
#include <cmath>

namespace mixworks
{
juce::Result MasterSettings::validate() const
{
    for (auto value : eqDb) if (!std::isfinite(value) || value < -12 || value > 12) return juce::Result::fail("Master EQ must be between -12 and +12 dB.");
    if (!std::isfinite(width) || width < 0 || width > 2) return juce::Result::fail("Master width must be between 0 and 200 percent.");
    if (!std::isfinite(saturationDriveDb) || saturationDriveDb < 0 || saturationDriveDb > 12) return juce::Result::fail("Master saturation drive must be between 0 and 12 dB.");
    return juce::Result::ok();
}
juce::var masterSettingsToJson(const MasterSettings& settings)
{
    auto* object = new juce::DynamicObject(); juce::Array<juce::var> eq;
    for (auto value : settings.eqDb) eq.add(value);
    object->setProperty("eqDb", eq); object->setProperty("width", settings.width);
    object->setProperty("saturation", settings.saturation); object->setProperty("saturationDriveDb", settings.saturationDriveDb);
    return juce::var(object);
}
juce::Result readMasterSettings(const juce::var& input, MasterSettings& settings)
{
    auto numeric = [](const juce::var& v, double low, double high)
    { return (v.isInt() || v.isInt64() || v.isDouble()) && std::isfinite((double) v) && (double) v >= low && (double) v <= high; };
    const auto* eq = input["eqDb"].getArray();
    if (!input.isObject() || !eq || eq->size() != 3 || !input["saturation"].isBool()
        || !numeric(input["width"], 0, 2) || !numeric(input["saturationDriveDb"], 0, 12)) return juce::Result::fail("Invalid master processing settings.");
    MasterSettings parsed;
    for (int i = 0; i < 3; ++i) { if (!numeric((*eq)[i], -12, 12)) return juce::Result::fail("Invalid master EQ."); parsed.eqDb[(size_t) i] = (float) (*eq)[i]; }
    parsed.width = (float) input["width"]; parsed.saturation = (bool) input["saturation"]; parsed.saturationDriveDb = (float) input["saturationDriveDb"];
    const auto result = parsed.validate(); if (result.wasOk()) settings = parsed; return result;
}
void Engine::processMaster(const Project& source, juce::AudioBuffer<float>& output, int frames, double at)
{
    masterInputPeak = output.getMagnitude(0, frames);
    const double rate = sampleRate.load();
    masterEqualizer.process(output, 0, frames, rate, source.mastering.eqDb);
    if (source.mastering.width != 1)
        for (int i = 0; i < frames; ++i)
        {
            const float left = output.getSample(0, i), right = output.getSample(1, i);
            const float mid = (left + right) * .5f, side = (left - right) * .5f * source.mastering.width;
            output.setSample(0, i, mid + side); output.setSample(1, i, mid - side);
        }
    if (at >= 0 && source.masterAutomation.active())
        for (int i = 0; i < frames; ++i)
        {
            const auto gain = juce::Decibels::decibelsToGain(automationValueIn(source, {-1, AutomationTarget::master}, at + i / rate));
            output.getWritePointer(0)[i] *= gain; output.getWritePointer(1)[i] *= gain;
        }
    else output.applyGain(0, frames, juce::Decibels::decibelsToGain(source.masterDb));
    if (source.mastering.saturation)
    {
        const double drive = juce::Decibels::decibelsToGain((double) source.mastering.saturationDriveDb);
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < frames; ++i)
            output.setSample(ch, i, (float) std::tanh(output.getSample(ch, i) * drive));
    }
    const auto peak = output.getMagnitude(0, frames); masterPeak = peak;
    if (peak > 1) masterClipped = true;
}
SpectrumFeed::SpectrumFeed() : data(2, capacity), sampleRates(capacity), fifo(capacity) {}
int SpectrumFeed::push(const juce::AudioBuffer<float>& input, int frames, double rate)
{
    if (!enabled.load(std::memory_order_relaxed) || frames <= 0 || frames > input.getNumSamples() || input.getNumChannels() < 1 || !std::isfinite(rate) || rate <= 0) return 0;
    int start1, size1, start2, size2; fifo.prepareToWrite(frames, start1, size1, start2, size2);
    auto copy = [&](int start, int count, int offset)
    {
        for (int ch = 0; ch < 2; ++ch) juce::FloatVectorOperations::copy(data.getWritePointer(ch, start), input.getReadPointer(std::min(ch, input.getNumChannels() - 1), offset), count);
        std::fill_n(sampleRates.data() + start, count, rate);
    };
    copy(start1, size1, 0); copy(start2, size2, size1); const int written = size1 + size2;
    fifo.finishedWrite(written); dropped.fetch_add((juce::uint64) (frames - written), std::memory_order_relaxed); return written;
}
int SpectrumFeed::read(float* left, float* right, double* rates, int wanted)
{
    if (wanted <= 0) return 0;
    int start1, size1, start2, size2; fifo.prepareToRead(wanted, start1, size1, start2, size2);
    auto copy = [&](int start, int count, int offset)
    {
        juce::FloatVectorOperations::copy(left + offset, data.getReadPointer(0, start), count);
        juce::FloatVectorOperations::copy(right + offset, data.getReadPointer(1, start), count);
        std::copy_n(sampleRates.data() + start, count, rates + offset);
    };
    copy(start1, size1, 0); copy(start2, size2, size1); fifo.finishedRead(size1 + size2); return size1 + size2;
}
void SpectrumFeed::discardAvailable()
{
    int start1, size1, start2, size2; fifo.prepareToRead(capacity, start1, size1, start2, size2); fifo.finishedRead(size1 + size2);
}
}
