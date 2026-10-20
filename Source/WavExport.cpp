#include "Engine.h"
#include <juce_cryptography/juce_cryptography.h>
#include <cmath>
#include <limits>

namespace mixworks
{
juce::Result Engine::planWavExport(const juce::File& file, WavExportOptions options, WavExportPlan& plan)
{
    const juce::ScopedLock guard(lock);
    const auto destination = validateWriteDestination(file); if (destination.failed()) return destination;
    if (!file.getParentDirectory().isDirectory() || file.isDirectory()) return juce::Result::fail("Choose an existing output folder and a file name.");
    if (!playbackReady()) return juce::Result::fail("Audio is still preparing or unavailable. Wait for the source checks before exporting.");
    const auto latency = pluginLatencyError(); if (latency.isNotEmpty()) return juce::Result::fail(latency);
    WavExportPlan candidate; candidate.sampleRate = sampleRate; candidate.arrangementEnd = project.duration();
    candidate.tailSeconds = options.includeTail ? project.effectTailSeconds : 0;
    if (!std::isfinite(candidate.arrangementEnd) || candidate.arrangementEnd <= 0) return juce::Result::fail("Add audio to the timeline before exporting.");
    if (!std::isfinite(candidate.sampleRate) || candidate.sampleRate <= 0 || candidate.sampleRate > std::numeric_limits<juce::uint32>::max()
        || candidate.sampleRate != std::round(candidate.sampleRate)) return juce::Result::fail("WAV export needs a supported integer sample rate.");
    if (!std::isfinite(candidate.tailSeconds) || candidate.tailSeconds < 0 || candidate.arrangementEnd + candidate.tailSeconds > 1.0e9)
        return juce::Result::fail("The effect-tail window is outside the supported mix timeline.");
    const double frames = std::ceil((candidate.arrangementEnd + candidate.tailSeconds) * candidate.sampleRate - 1.0e-6);
    constexpr juce::int64 headerBudget = 4096, bytesPerFrame = 6; // PCM24 stereo; JUCE promotes long WAVs to RF64.
    if (!std::isfinite(frames) || frames <= 0 || frames > (double) ((std::numeric_limits<juce::int64>::max() - headerBudget) / bytesPerFrame))
        return juce::Result::fail("The planned WAV exceeds the file-size budget.");
    candidate.frames = (juce::int64) frames; candidate.estimatedBytes = candidate.frames * bytesPerFrame + headerBudget;
    candidate.available = true; plan = candidate;
    const auto free = file.getParentDirectory().getBytesFreeOnVolume();
    if (free < 0) return juce::Result::fail("Cannot establish available space for this export.");
    const auto previousBytes = file.existsAsFile() ? file.getSize() : 0;
    if (previousBytes > free || candidate.estimatedBytes > free - previousBytes)
        return juce::Result::fail("Not enough free space for the planned WAV and a backup of the existing destination.");
    return juce::Result::ok();
}
std::unique_ptr<Engine> Engine::createExportRenderer(juce::String& error, const std::shared_ptr<std::atomic<bool>>& cancelled)
{
    const juce::ScopedLock guard(lock);
    bool hasPlugins = false; eachPlugin([&](auto&) { hasPlugins = true; });
    if (hasPlugins && !juce::MessageManager::getInstance()->isThisTheMessageThread())
    { error = "Prepare export processors on the message thread."; return {}; }
    if (!playbackReady()) { error = "Wait for all source audio before preparing export."; return {}; }
    error = pluginLatencyError(); if (error.isNotEmpty()) return {};
    auto copy = std::make_unique<Engine>(juce::File(), false); copy->offlineRenderer = true;
    copy->sampleRate = sampleRate.load(); copy->blockSize = blockSize; copy->prepareBuffers(blockSize); copy->midiCollector.reset(copy->sampleRate.load());
    copy->project = project; copy->library = library; copy->tempo = tempo.load();
    auto chain = [&](const std::vector<std::unique_ptr<PluginSlot>>& source, std::vector<std::unique_ptr<PluginSlot>>& target)
    {
        for (const auto& slot : source)
        {
            if (cancelled && *cancelled) { error = "Export preparation cancelled."; return false; }
            std::unique_ptr<PluginSlot> cloned; error = copy->clonePlugin(*slot, cloned); if (error.isNotEmpty()) return false;
            cloned->id = slot->id; target.push_back(std::move(cloned));
        }
        return true;
    };
    for (int lane = 0; lane < trackCount; ++lane) if (!chain(plugins[(size_t) lane], copy->plugins[(size_t) lane])) return {};
    std::map<std::shared_ptr<ClipEffects>, std::shared_ptr<ClipEffects>, std::owner_less<std::shared_ptr<ClipEffects>>> effects;
    for (auto& clip : copy->project.clips)
    {
        auto found = effects.find(clip.effects);
        if (found == effects.end())
        {
            auto cloned = makeClipEffects(); if (!chain(clip.effects->plugins, cloned->plugins)) return {};
            found = effects.emplace(clip.effects, std::move(cloned)).first;
        }
        clip.effects = found->second;
    }
    copy->prepareClipProcessing(); error = copy->pluginLatencyError(); if (error.isNotEmpty()) return {}; return copy;
}
WavExportResult Engine::writeWav(const juce::File& file, WavExportOptions options,
                               const std::shared_ptr<std::atomic<bool>>& cancelled, const WavExportProgress& progress)
{
    WavExportResult result;
    auto fail = [&](const juce::String& error) { result.error = error; return result; };
    auto stop = [&] { result.outcome = WavExportOutcome::cancelled; result.error = "Export cancelled. The destination was kept."; return result; };
    if (!offlineRenderer) return fail("Prepare a private offline renderer before writing audio.");
    const juce::ScopedLock renderGuard(lock); // Worker-private engine; never the live/UI engine lock.
    if (exporting.exchange(true)) return fail("An export is already running.");
    struct Flag { std::atomic<bool>& flag; ~Flag() { flag = false; } } flag {exporting};
    const auto key = file.getFullPathName().toLowerCase();
    juce::InterProcessLock destinationLock("mixworks-wav-" + juce::SHA256(key.toRawUTF8(), key.getNumBytesAsUTF8()).toHexString());
    if (!destinationLock.enter(0)) return fail("Another window is writing this export destination.");
    struct Unlock { juce::InterProcessLock& lock; ~Unlock() { lock.exit(); } } unlock {destinationLock};
    const auto valid = planWavExport(file, options, result.plan); if (valid.failed()) return fail(valid.getErrorMessage());
    if (cancelled && *cancelled) return stop();
    juce::TemporaryFile temporary(file); std::unique_ptr<juce::OutputStream> stream = temporary.getFile().createOutputStream();
    if (!stream) return fail("Cannot create the temporary export file.");
    juce::WavAudioFormat wav;
    auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(result.plan.sampleRate).withNumChannels(2).withBitsPerSample(24));
    if (!writer) return fail("Cannot create a WAV writer.");
    resetPlugins(); eachPlugin([](auto& plugin) { plugin.processor->setNonRealtime(true); });
    struct Restore
    {
        Engine& engine;
        ~Restore() { engine.eachPlugin([](auto& plugin) { plugin.processor->setNonRealtime(false); }); engine.resetPlugins(); }
    } restore {*this};
    juce::MidiBuffer midi;
    if (progress) progress(0, result.plan);
    for (juce::int64 offset = 0; offset < result.plan.frames; offset += blockSize)
    {
        if (cancelled && *cancelled) return stop();
        const int frames = (int) std::min<juce::int64>(blockSize, result.plan.frames - offset);
        position = offset / result.plan.sampleRate; render(mixBuffer, frames, position, midi);
        if (latencyBlocked) { const auto issue = pluginLatencyError(); return fail(issue.isNotEmpty() ? issue : "Plugin latency changed during export. The destination was kept."); }
        for (int channel = 0; channel < 2; ++channel) for (int frame = 0; frame < frames; ++frame)
            if (!std::isfinite(mixBuffer.getSample(channel, frame))) return fail("Rendered audio contains non-finite samples. The destination was kept.");
        result.peak = std::max(result.peak, mixBuffer.getMagnitude(0, frames));
        if (!writer->writeFromAudioSampleBuffer(mixBuffer, 0, frames)) return fail("Export write failed. The destination was kept.");
        result.writtenFrames += frames; if (progress) progress(result.writtenFrames, result.plan);
    }
    if (!writer->flush()) return fail("Cannot finish the WAV header. The destination was kept.");
    writer.reset();
    if (cancelled && *cancelled) return stop();
    // The VST3 latency getter is UI-only, and JUCE's cache can lag nested
    // notifications. Ask main for the actual native value before committing.
    bool hasPlugins = false; eachPlugin([&](auto&) { hasPlugins = true; });
    juce::String finalLatency;
    if (hasPlugins && !juce::MessageManager::getInstance()->isThisTheMessageThread())
    {
        struct Admission { juce::CriticalSection mutex; Engine* engine; juce::String issue; juce::WaitableEvent done; };
        auto admission = std::make_shared<Admission>(); admission->engine = this;
        {
            const juce::ScopedUnlock releasePrivateLock(lock);
            if (!juce::MessageManager::callAsync([admission]
            {
                const juce::ScopedLock guard(admission->mutex);
                if (admission->engine) admission->issue = admission->engine->pluginLatencyError();
                admission->done.signal();
            })) return fail("Cannot confirm native plugin state. The destination was kept.");
            while (!admission->done.wait(10)) if (cancelled && *cancelled)
            {
                // A late callback must never dereference an already closed renderer.
                const juce::ScopedLock guard(admission->mutex); admission->engine = nullptr; return stop();
            }
            const juce::ScopedLock guard(admission->mutex); finalLatency = admission->issue; admission->engine = nullptr;
        }
    }
    else finalLatency = pluginLatencyError();
    if (cancelled && *cancelled) return stop();
    if (finalLatency.isNotEmpty()) return fail(finalLatency);
    if (file.existsAsFile() && !file.copyFileTo(file.getSiblingFile(file.getFileName() + ".bak").getNonexistentSibling()))
        return fail("Cannot back up the previous export. The destination was kept.");
    if (cancelled && *cancelled) return stop();
    if (!temporary.overwriteTargetFileWithTemporary()) return fail("Cannot commit the completed WAV. The destination was kept.");
    result.outcome = WavExportOutcome::success; return result;
}
juce::String Engine::exportWav(const juce::File& file)
{
    WavExportPlan plan; const auto valid = planWavExport(file, {}, plan); if (valid.failed()) return valid.getErrorMessage();
    if (exporting.exchange(true)) return "An export is already running.";
    struct Flag { std::atomic<bool>& flag; ~Flag() { flag = false; } } flag {exporting};
    setPlaying(false); endTransitionRehearsal();
    juce::String error; auto renderer = createExportRenderer(error); if (!renderer) return error;
    const auto result = renderer->writeWav(file, {}); if (result.wasOk() && result.peak > 1) masterClipped = true;
    return result.error;
}
}
