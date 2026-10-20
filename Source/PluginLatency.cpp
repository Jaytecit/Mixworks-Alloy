#include "Engine.h"
#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/ivstcomponent.h>

namespace mixworks
{
juce::String PluginSlot::latencyError() const
{
    if (bypass) return {};
    std::optional<juce::int64> observed;
    if (juce::MessageManager::getInstance()->isThisTheMessageThread())
    {
        // The VST3 contract permits this authoritative query on UI only. JUCE's
        // AudioProcessor cache can lag nested native restart notifications.
        if (auto* client = processor->getVST3Client())
            if (auto* component = client->getIComponentPtr())
            {
                Steinberg::FUnknownPtr<Steinberg::Vst::IAudioProcessor> native(component);
                if (native) observed = (juce::int64) native->getLatencySamples();
            }
    }
    else observed = std::max(0, processor->getLatencySamples()); // Published cache; safe during processing.
    if (!observed) return "Cannot verify native processing latency for " + description.name + ". Bypass this insert before playback or export.";
    return *observed > 0 ? description.name + " reports " + juce::String(*observed)
        + " samples of latency. Active plugins currently need zero reported latency. Bypass this insert or choose its zero-latency mode." : juce::String();
}
bool Engine::hasUnsupportedLatency() const
{
    for (const auto& chain : plugins) for (const auto& slot : chain) if (slot->unsupportedLatency() > 0) return true;
    for (const auto& clip : project.clips) for (const auto& slot : clip.effects->plugins) if (slot->unsupportedLatency() > 0) return true;
    return false;
}
juce::String Engine::pluginLatencyError() const
{
    const juce::ScopedLock guard(lock);
    auto issue = [](const PluginSlot& slot, const juce::String& owner)
    {
        const auto detail = slot.latencyError(); return detail.isNotEmpty() ? owner + ": " + detail : juce::String();
    };
    for (int lane = 0; lane < trackCount; ++lane) for (const auto& slot : plugins[(size_t) lane])
    { const auto error = issue(*slot, "Lane " + juce::String(lane + 1)); if (error.isNotEmpty()) return error; }
    for (const auto& clip : project.clips) for (const auto& slot : clip.effects->plugins)
    { const auto error = issue(*slot, "Clip " + clip.media->name); if (error.isNotEmpty()) return error; }
    return {};
}
bool Engine::processPlugin(PluginSlot& slot, juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    if (slot.unsupportedLatency() > 0) return false;
    slot.processor->processBlock(buffer, midi);
    return slot.unsupportedLatency() == 0;
}
void Engine::stopForLatency(juce::AudioBuffer<float>& output, int frames)
{
    output.clear(0, frames); playing = false; latencyBlocked = true;
    masterPeak = masterInputPeak = 0; for (auto& meter : meters) meter = 0;
}
}
