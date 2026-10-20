#include "Engine.h"
#include <cmath>
#include <limits>

namespace mixworks
{
void Engine::endTransitionRehearsal()
{
    const juce::ScopedLock guard(lock);
    if (rehearsalActive()) { playing = false; position = rehearsalReturn; }
    rehearsalSlot = -1; rehearsalCursor = 0;
    for (auto& item : rehearsalAudio) { item.audio.setSize(0, 0); item.rms = 0; item.gain = 1; }
}
juce::Result Engine::auditionTransitionAlternative(int slot)
{
    const juce::ScopedLock guard(lock);
    if (slot < 0 || slot >= 3 || rehearsalAudio[(size_t) slot].audio.getNumSamples() == 0 || rehearsalRate != sampleRate.load())
        return juce::Result::fail("Store this alternative, then prepare rehearsal again.");
    if (!rehearsalActive()) rehearsalReturn = position.load();
    gridAudition.stop(); finishAutomation(); automationRecordMode = AutomationRecordMode::read;
    keyboard.allNotesOff(0); rehearsalSlot = slot; rehearsalCursor = 0; position = rehearsalBegin; playing = true;
    return juce::Result::ok();
}
void Engine::renderTransitionRehearsal(juce::AudioBuffer<float>& output, int frames)
{
    const auto& item = rehearsalAudio[(size_t) rehearsalSlot]; const int length = item.audio.getNumSamples();
    if (length <= 0) return;
    for (int offset = 0; offset < frames;)
    {
        const int count = std::min(frames - offset, length - rehearsalCursor);
        for (int ch = 0; ch < 2; ++ch) output.copyFrom(ch, offset, item.audio.getReadPointer(ch, rehearsalCursor), count, item.gain);
        offset += count; rehearsalCursor = (rehearsalCursor + count) % length;
    }
    position = rehearsalBegin + rehearsalCursor / rehearsalRate;
}
juce::String Engine::rehearsalSummary() const
{
    const juce::ScopedLock guard(lock); juce::String text = "RMS-matched rehearsal";
    for (int i = 0; i < 3; ++i) if (rehearsalAudio[(size_t) i].audio.getNumSamples() > 0)
        text += " / " + juce::String::charToString((juce::juce_wchar) ('A' + i)) + (rehearsalAudio[(size_t) i].rms <= 1e-9 ? " silent" : " " + juce::String(juce::Decibels::gainToDecibels(rehearsalAudio[(size_t) i].gain), 1) + " dB");
    return text;
}
juce::Result Engine::prepareTransitionRehearsal(const juce::String& incoming, const std::shared_ptr<std::atomic<bool>>& cancelled)
{
    if (exporting.exchange(true)) return juce::Result::fail("An audio render is already running.");
    struct Flag { std::atomic<bool>& value; ~Flag() { value = false; } } flag {exporting};
    Project baseline; double rate = 0, begin = 0, end = 0; int renderBlockSize = 0;
    std::array<std::optional<TransitionStyle>, 3> styles;
    std::vector<std::pair<juce::AudioPluginInstance*, juce::MemoryBlock>> pluginStates;
    {
        const juce::ScopedLock guard(lock); endTransitionRehearsal(); finishAutomation(); automationRecordMode = AutomationRecordMode::read;
        playing = false; gridAudition.stop(); keyboard.allNotesOff(0);
        if (!playbackReady()) return juce::Result::fail("Wait for all session audio to be ready.");
        const auto latency = pluginLatencyError(); if (latency.isNotEmpty()) return juce::Result::fail(latency);
        const auto* link = project.transitionFor(incoming); if (!link) return juce::Result::fail("Select a transition first.");
        const auto contextValid = project.rehearsalContext.validate(); if (contextValid.failed()) return contextValid;
        styles = link->alternatives; baseline = project; rate = sampleRate.load(); renderBlockSize = blockSize;
        const auto range = project.transitionRange(*link); const auto clock = project.followTrackTempo ? project.tempo : TempoMap {project.bpm, {}};
        begin = std::max(0., clock.timeAtBeat(clock.beatsAt(range.getStart()) - 4. * project.rehearsalContext.beforeBars)); end = std::min(project.audibleEnd(), clock.timeAtBeat(clock.beatsAt(range.getEnd()) + 4. * project.rehearsalContext.afterBars));
        eachPlugin([&](auto& p) { pluginStates.emplace_back(p.processor.get(), juce::MemoryBlock()); p.processor->getStateInformation(pluginStates.back().second); });
    }
    const size_t count = (size_t) std::count_if(styles.begin(), styles.end(), [](const auto& x) { return x.has_value(); });
    const double frames = std::ceil((end - begin) * rate);
    // Three stereo float buffers share a 256 MiB preparation budget. Check
    // multiplication in double before converting to JUCE's int sample count.
    if (!count) return juce::Result::fail("Store at least one A/B/C alternative first.");
    if (!std::isfinite(frames) || frames <= 0 || frames > std::numeric_limits<int>::max() || frames * 2 * sizeof(float) * count > 256. * 1024 * 1024)
        return juce::Result::fail("This rehearsal exceeds its 256 MiB audio budget. Shorten the transition.");
    struct Restore
    {
        Engine& engine; std::vector<std::pair<juce::AudioPluginInstance*, juce::MemoryBlock>>& states;
        float inputPeak, outputPeak; bool clipped; std::array<float, trackCount> lanePeaks;
        Restore(Engine& e, std::vector<std::pair<juce::AudioPluginInstance*, juce::MemoryBlock>>& s)
            : engine(e), states(s), inputPeak(e.masterInputPeak), outputPeak(e.masterPeak), clipped(e.masterClipped)
        { for (int i = 0; i < trackCount; ++i) lanePeaks[(size_t) i] = e.meters[(size_t) i]; }
        ~Restore()
        {
            const juce::ScopedLock guard(engine.lock);
            for (auto& [plugin, state] : states) { plugin->setNonRealtime(false); plugin->setStateInformation(state.getData(), (int) state.getSize()); }
            engine.resetPlugins(); engine.masterInputPeak = inputPeak; engine.masterPeak = outputPeak; engine.masterClipped = clipped;
            for (int i = 0; i < trackCount; ++i) engine.meters[(size_t) i] = lanePeaks[(size_t) i];
        }
    } restore {*this, pluginStates};
    std::array<RehearsalAudio, 3> audio;
    juce::AudioBuffer<float> block(2, renderBlockSize); juce::MidiBuffer midi;
    for (size_t i = 0; i < 3; ++i) if (styles[i])
    {
        auto candidate = baseline;
        // Locks prohibit commits, not non-destructive comparisons.
        for (auto& link : candidate.transitionLinks) link.locked = false;
        const auto valid = candidate.applyTransitionStyle(incoming, *styles[i]); if (valid.failed()) return valid;
        {
            const juce::ScopedLock guard(lock); resetPlugins();
            for (auto& [plugin, state] : pluginStates) { plugin->setStateInformation(state.getData(), (int) state.getSize()); plugin->setNonRealtime(true); }
        }
        auto& item = audio[i]; item.audio.setSize(2, (int) frames); double energy = 0;
        for (int offset = 0; offset < (int) frames;)
        {
            if (cancelled && cancelled->load()) return juce::Result::fail("Rehearsal preparation cancelled; session kept.");
            const int n = std::min(renderBlockSize, (int) frames - offset);
            {
                const juce::ScopedLock guard(lock);
                if (sampleRate.load() != rate || blockSize != renderBlockSize) return juce::Result::fail("Audio device changed during rehearsal preparation. Prepare again.");
                render(block, n, begin + offset / rate, midi, &candidate);
                if (latencyBlocked) { const auto issue = pluginLatencyError(); return juce::Result::fail(issue.isNotEmpty() ? issue : "Plugin latency changed during rehearsal. Prepare again in zero-latency mode."); }
            }
            for (int ch = 0; ch < 2; ++ch)
            {
                const auto* values = block.getReadPointer(ch);
                for (int sample = 0; sample < n; ++sample) { if (!std::isfinite(values[sample])) return juce::Result::fail("Non-finite rehearsal audio; session kept."); energy += (double) values[sample] * values[sample]; }
                item.audio.copyFrom(ch, offset, block, ch, 0, n);
            }
            offset += n;
        }
        item.rms = std::sqrt(energy / (frames * 2));
    }
    double target = std::numeric_limits<double>::infinity();
    for (const auto& item : audio) if (item.rms > 1e-9) target = std::min(target, item.rms);
    for (auto& item : audio) if (item.rms > 1e-9) item.gain = (float) (target / item.rms);
    {
        const juce::ScopedLock guard(lock);
        if (sampleRate.load() != rate || blockSize != renderBlockSize) return juce::Result::fail("Audio device changed during rehearsal preparation. Prepare again.");
        if (cancelled && cancelled->load()) return juce::Result::fail("Rehearsal preparation cancelled; session kept.");
        rehearsalAudio = std::move(audio); rehearsalBegin = begin; rehearsalRate = rate;
    }
    return juce::Result::ok();
}
}
