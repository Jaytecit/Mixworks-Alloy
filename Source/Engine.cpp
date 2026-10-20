#include "Engine.h"
#include <filesystem>
#include "Beatmatch.h"
#include <cmath>
#include <map>
#include <limits>

namespace mixworks
{
namespace
{
bool continuousMidiAction(MidiAction a)
{
    return a.kind == MidiActionKind::master || a.kind == MidiActionKind::volume || a.kind == MidiActionKind::pan
        || a.kind == MidiActionKind::lowPass || a.kind == MidiActionKind::deskPot || a.kind == MidiActionKind::bass || a.kind == MidiActionKind::mid || a.kind == MidiActionKind::treble || a.kind == MidiActionKind::pluginParameter;
}
juce::String parameterIdentity(juce::AudioProcessorParameter* parameter)
{
    if (auto* hosted = dynamic_cast<juce::HostedAudioProcessorParameter*>(parameter)) return hosted->getParameterID();
    return {};
}
juce::AudioProcessorParameter* findControl(const PluginControl& control, const std::vector<std::unique_ptr<PluginSlot>>& chain)
{
    if (control.parameterId.isEmpty() || control.insert < 0 || control.insert >= (int) chain.size()) return nullptr;
    const auto& parameters = chain[(size_t) control.insert]->parameters;
    const auto found = parameters.find(control.parameterId); return found == parameters.end() ? nullptr : found->second;
}
void defaultPluginControls(TrackSettings& track, const std::vector<std::unique_ptr<PluginSlot>>& chain)
{
    if (chain.empty()) return;
    std::vector<juce::AudioProcessorParameter*> candidates;
    for (auto* p : chain[0]->processor->getParameters())
        if (p->isAutomatable() && !p->isMetaParameter() && p != chain[0]->processor->getBypassParameter() && parameterIdentity(p).isNotEmpty()) candidates.push_back(p);
    const juce::StringArray names { "cutoff", "resonance", "attack", "release", "decay", "sustain", "drive", "mix" };
    auto priority = [&](auto* p) { for (int i = 0; i < names.size(); ++i) if (p->getName(128).containsIgnoreCase(names[i])) return i; return names.size(); };
    std::stable_sort(candidates.begin(), candidates.end(), [&](auto* a, auto* b) { return priority(a) < priority(b); });
    for (size_t i = 0; i < std::min(track.pluginControls.size(), candidates.size()); ++i) track.pluginControls[i] = { 0, parameterIdentity(candidates[i]) };
}
juce::File testMidiProfile;
juce::File midiSettingsFile()
{
    if (testMidiProfile != juce::File()) return testMidiProfile;
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Mixworks");
    dir.createDirectory(); return dir.getChildFile("midi-mappings.json");
}
juce::String writeWithBackup(const juce::File& target, const juce::String& text)
{
    if (target.existsAsFile())
    {
        auto backup = target.getSiblingFile(target.getFileName() + ".bak").getNonexistentSibling();
        if (!target.copyFileTo(backup)) return "Could not back up " + target.getFullPathName();
    }
    juce::TemporaryFile temp(target);
    if (!temp.getFile().replaceWithText(text) || !temp.overwriteTargetFileWithTemporary()) return "Could not save " + target.getFullPathName();
    return {};
}
}
void Engine::useTestMidiProfile(const juce::File& file) { testMidiProfile = file; }
Engine::Engine(juce::File midiProfile, bool loadMidiMappings) : mappingsFile(midiProfile == juce::File() ? midiSettingsFile() : std::move(midiProfile))
{
    formats.registerBasicFormats();
    pluginFormats.addFormat(std::make_unique<juce::VST3PluginFormat>());
    prepareBuffers(blockSize); midiCollector.reset(sampleRate);
    if (loadMidiMappings && mappingsFile.existsAsFile()) reloadMidiMappings();
}
Engine::~Engine()
{
    onControl = {}; onMidiChanged = {};
    if (attached) devices.removeAudioCallback(this);
    devices.removeMidiInputDeviceCallback({}, this);
    cancelPendingUpdate(); devices.closeAudioDevice();
    const juce::ScopedLock guard(lock);
    eachPlugin([](auto& slot) { slot.processor->releaseResources(); });
}
void Engine::eachPlugin(const std::function<void(PluginSlot&)>& visit)
{
    for (auto& chain : plugins) for (auto& slot : chain) visit(*slot);
    for (auto& c : project.clips) for (auto& slot : c.effects->plugins) visit(*slot);
}
juce::Result Engine::validateWriteDestination(const juce::File& file) const
{
    namespace fs = std::filesystem;
    auto native = [](const juce::File& value)
    {
       #if JUCE_WINDOWS
        return fs::path(value.getFullPathName().toWideCharPointer());
       #else
        return fs::u8path(value.getFullPathName().toRawUTF8());
       #endif
    };
    const auto destination = native(file); std::error_code error;
    const auto destinationStatus = fs::symlink_status(destination, error);
    if (error && error != std::errc::no_such_file_or_directory) return juce::Result::fail("Cannot inspect the output destination: " + juce::String(error.value()));
    if (fs::is_symlink(destinationStatus) || (fs::exists(destinationStatus) && !fs::is_regular_file(destinationStatus)))
        return juce::Result::fail("Choose a regular output file rather than a link or special file.");
    auto check = [&](const juce::File& source)
    {
        if (source == file) return juce::Result::fail("Cannot replace source audio. Choose another destination.");
        if (!fs::exists(destinationStatus)) return juce::Result::ok();
        std::error_code statusError; const auto status = fs::status(native(source), statusError);
        if (statusError == std::errc::no_such_file_or_directory || status.type() == fs::file_type::not_found) return juce::Result::ok();
        if (statusError) return juce::Result::fail("Cannot verify a source file's identity: " + juce::String(statusError.value()));
        std::error_code identityError; const bool same = fs::equivalent(native(source), destination, identityError);
        if (identityError) return juce::Result::fail("Cannot verify source/output identity: " + juce::String(identityError.value()));
        return same ? juce::Result::fail("Cannot replace source audio through an alias. Choose another file.") : juce::Result::ok();
    };
    for (const auto& media : library) { const auto result = check(media->file); if (result.failed()) return result; }
    for (const auto& clip : project.clips) { const auto result = check(clip.media->file); if (result.failed()) return result; }
    return juce::Result::ok();
}

std::shared_ptr<ClipEffects> makeClipEffects() { return std::make_shared<ClipEffects>(); }
Clip* Engine::controlClip(int track)
{
    auto* c = project.clipFor(selectedClip); return c && c->track == track ? c : nullptr;
}
const Clip* Engine::controlClip(int track) const { return const_cast<Engine*>(this)->controlClip(track); }
TrackSettings& Engine::controlSettings(int track) { auto* c = controlClip(track); return c ? c->controls : project.tracks[(size_t) track]; }
std::vector<std::unique_ptr<PluginSlot>>& Engine::pluginChain(int track)
{
    auto* c = controlClip(track); return c ? c->effects->plugins : plugins[(size_t) track];
}
const std::vector<std::unique_ptr<PluginSlot>>& Engine::pluginChain(int track) const { return const_cast<Engine*>(this)->pluginChain(track); }
juce::String Engine::clonePlugin(const PluginSlot& old, std::unique_ptr<PluginSlot>& slot)
{
    const auto error = instantiate(old.description, slot); if (error.isNotEmpty()) return error;
    juce::MemoryBlock state; old.processor->getStateInformation(state);
    if (state.getSize() > (size_t) std::numeric_limits<int>::max()) return "Plugin state is too large.";
    slot->processor->setStateInformation(state.getData(), (int) state.getSize()); slot->bypass = old.bypass; return {};
}
juce::String Engine::cloneClipEffects(Clip& clip)
{
    auto copy = makeClipEffects(); std::map<juce::String, juce::String> ids;
    for (const auto& old : clip.effects->plugins)
    {
        std::unique_ptr<PluginSlot> slot; const auto error = clonePlugin(*old, slot); if (error.isNotEmpty()) return error;
        ids[old->id] = slot->id; copy->plugins.push_back(std::move(slot));
    }
    for (const auto& lane : clip.controls.pluginAutomation) if (!ids.count(lane.slotId)) return "An automated VST effect is unavailable.";
    for (auto& lane : clip.controls.pluginAutomation) lane.slotId = ids.at(lane.slotId);
    clip.effects = std::move(copy); return {};
}
void ToneEq::reset() { for (auto& band : bands) { band.z1.fill(0); band.z2.fill(0); } }
void ToneEq::process(juce::AudioBuffer<float>& audio, int start, int frames, double sampleRate, const std::array<float, 3>& values)
{
    if (rate != sampleRate || gains != values)
    {
        if (rate != sampleRate) reset();
        rate = sampleRate; gains = values;
        using Design = juce::dsp::IIR::ArrayCoefficients<double>;
        const std::array<std::array<double, 6>, 3> designs {
            Design::makeLowShelf(rate, std::min(120.0, rate * 0.4), 0.7071, std::pow(10.0, gains[0] / 20.0)),
            Design::makePeakFilter(rate, std::min(1000.0, rate * 0.4), 0.8, std::pow(10.0, gains[1] / 20.0)),
            Design::makeHighShelf(rate, std::min(8000.0, rate * 0.4), 0.7071, std::pow(10.0, gains[2] / 20.0)) };
        for (size_t i = 0; i < bands.size(); ++i)
        {
            const auto& c = designs[i]; bands[i].coefficients = {c[0] / c[3], c[1] / c[3], c[2] / c[3], c[4] / c[3], c[5] / c[3]};
        }
        if (gains == std::array<float, 3>{}) reset();
    }
    if (gains == std::array<float, 3>{}) return;
    for (auto& band : bands)
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* data = audio.getWritePointer(ch, start); const auto& c = band.coefficients;
            for (int i = 0; i < frames; ++i)
            {
                const double x = data[i], y = c[0] * x + band.z1[(size_t) ch];
                band.z1[(size_t) ch] = c[1] * x - c[3] * y + band.z2[(size_t) ch]; band.z2[(size_t) ch] = c[2] * x - c[4] * y;
                data[i] = (float) y;
            }
        }
}
juce::String Engine::startDevice()
{
    auto error = devices.initialiseWithDefaultDevices(0, 2);
    if (error.isNotEmpty()) return error;
    devices.addAudioCallback(this); attached = true;
    devices.addMidiInputDeviceCallback({}, this);
    midiCallbackRegistered = true;
    for (const auto& port : juce::MidiInput::getAvailableDevices())
        if (port.name.containsIgnoreCase("FLkey") && !port.name.containsIgnoreCase("DAW") && !port.name.startsWithIgnoreCase("MIDIIN2")) enableMidi(port.identifier, true);
    return {};
}
juce::Result Engine::correctSource(const std::shared_ptr<Media>& source, const SourceCorrection& correction)
{
    if (!source) return juce::Result::fail("The selected source is no longer available.");
    const auto valid = correction.validate(source->duration()); if (valid.failed()) return valid;
    const juce::ScopedLock guard(lock);
    const auto matches = [&](const std::shared_ptr<Media>& candidate)
    {
        return candidate && (candidate == source || (source->analysisSource.sha256.isNotEmpty()
            && candidate->file == source->file && candidate->analysisSource == source->analysisSource));
    };
    std::set<Media*> targets;
    for (auto& media : library) if (matches(media)) targets.insert(media.get());
    for (auto& clip : project.clips) if (matches(clip.media)) targets.insert(clip.media.get());
    if (targets.empty()) return juce::Result::fail("This source was removed or replaced. Reopen its grid editor.");
    endTransitionRehearsal();
    // Capture legacy clocks before changing facts; never rebuild PCM or geometry here.
    for (auto& clip : project.clips) if (targets.count(clip.media.get()) && clip.beatmatched && clip.tempoSourceBpm <= 0)
        clip.tempoSourceBpm = clip.media->bpm();
    targets.insert(source.get()); for (auto* media : targets) correction.apply(*media);
    return juce::Result::ok();
}
void Engine::enableMidi(const juce::String& id, bool enabled) { devices.setMidiInputDeviceEnabled(id, enabled); }
void Engine::prepareBuffers(int size)
{
    mixBuffer.setSize(2, size);
    for (auto& b : trackBuffers) b.setSize(2, size);
    for (auto& b : trackMidi) b.ensureSize(4096);
    for (auto& b : segmentMidi) b.ensureSize(4096);
    for (auto& b : processedMidi) b.ensureSize(4096);
    callbackMidi.ensureSize(4096); clipMidi.ensureSize(4096); clipSegmentMidi.ensureSize(4096); clipProcessedMidi.ensureSize(4096);
    clipBuffer.setSize(2, size);
}
void Engine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    const juce::ScopedLock guard(lock);
    endTransitionRehearsal();
    deviceClockValid = false;
    sampleRate = device->getCurrentSampleRate(); blockSize = device->getCurrentBufferSizeSamples();
    prepareBuffers(blockSize); midiCollector.reset(sampleRate);
    eachPlugin([&](auto& p) { p.processor->releaseResources(); p.processor->setRateAndBufferSizeDetails(sampleRate, blockSize); p.processor->prepareToPlay(sampleRate, blockSize); });
}
void Engine::audioDeviceStopped()
{
    playing = false;
    const juce::ScopedLock guard(lock);
    gridAudition.stop();
    eachPlugin([](auto& p) { p.processor->releaseResources(); });
}
void Engine::stop()
{
    const juce::ScopedLock guard(lock); gridAudition.stop(); endTransitionRehearsal();
    finishAutomation(); automationRecordMode = AutomationRecordMode::read;
    playing = false; position = 0; deviceClockValid = false; keyboard.allNotesOff(0); resetPlugins();
}
void Engine::seek(double seconds)
{
    const juce::ScopedLock guard(lock); endTransitionRehearsal();
    finishAutomation();
    position = std::max(0.0, seconds); deviceClockValid = false; resetPlugins();
}
void Engine::prepareClipProcessing()
{
    const juce::ScopedLock guard(lock);
    for (auto it = clipProcessors.begin(); it != clipProcessors.end();) if (it->first.expired()) it = clipProcessors.erase(it); else ++it;
    for (const auto& c : project.clips) clipProcessors.try_emplace(c.effects);
}
void Engine::resetPlugins()
{
    const juce::ScopedLock guard(lock);
    elasticRenderer.reset();
    masterEqualizer.reset();
    for (auto& eq : equalizers) eq.reset();
    for (auto& filter : lowPasses) filter.reset();
    for (auto& entry : clipProcessors) { entry.second.eq.reset(); entry.second.filter.reset(); entry.second.lowPass.reset(); }
    eachPlugin([](auto& p) { p.processor->reset(); });
}
juce::Optional<juce::AudioPlayHead::PositionInfo> Engine::getPosition() const
{
    const juce::ScopedLock guard(lock);
    juce::AudioPlayHead::PositionInfo info;
    const double time = renderingPlugin ? pluginRenderTime.load() : position.load();
    info.setTimeInSamples((int64_t) std::llround(time * sampleRate.load()));
    const auto& source = renderContext ? *renderContext : project;
    info.setTimeInSeconds(time); info.setBpm(source.followTrackTempo ? source.tempo.bpmAt(time) : source.bpm);
    info.setTimeSignature(juce::AudioPlayHead::TimeSignature {4, 4}); info.setPpqPosition(source.followTrackTempo ? source.tempo.beatsAt(time) : time * source.bpm / 60.0);
    info.setIsPlaying(playing.load() || exporting.load()); return info;
}
bool Engine::processClipEffects(const Project& source, const Clip& clip, int frames, double at, const juce::MidiBuffer& incoming)
{
    const double sr = sampleRate.load();
    const bool audition = at < 0;
    auto boundary = [&](double time) { return (int) juce::jlimit(0., (double) frames, std::ceil((time - at) * sr - 1.0e-6)); };
    const auto transition = audition ? TransitionPlayback {} : source.transitionPlaybackFor(clip);
    const double noteEnd = std::min(clip.end(), transition.endCut.value_or(clip.end()));
    const double effectEnd = clip.end() + source.effectTailSeconds;
    const int first = audition ? 0 : boundary(clip.start);
    const int last = audition ? frames : std::max(first, boundary(effectEnd));
    const int audioFirst = audition ? 0 : boundary(std::max(clip.start, transition.startCut.value_or(clip.start)));
    const int audioLast = audition ? frames : std::max(audioFirst, boundary(noteEnd));
    // Gate every source path before inserts, then gate their output below as
    // well. Excluded audio cannot excite an insert or leak back as an EQ tail.
    if (audioFirst > 0) clipBuffer.clear(0, audioFirst);
    if (audioLast < frames) clipBuffer.clear(audioLast, frames - audioLast);
    auto value = [&](AutomationTarget target, double time)
    {
        const float manual = automationValueIn(source, {clip.track, target, {}, {}, clip.id}, time);
        if (target < AutomationTarget::bass || target > AutomationTarget::treble) return manual;
        const float gain = transition.gainAt((int) target, time);
        return gain == 1 ? manual : automationRange(target).clipValue(manual + juce::Decibels::gainToDecibels(gain, -60.f));
    };
    clipMidi.clear();
    if (clip.id == selectedClip && clip.track == selectedTrack)
        clipMidi.addEvents(incoming, audioFirst, audition ? frames : std::max(0, boundary(noteEnd) - audioFirst), 0);
    if (!audition && clip.effects->hasInstrument() && noteEnd >= at && noteEnd < at + frames / sr)
        for (int channel = 1; channel <= 16; ++channel) clipMidi.addEvent(juce::MidiMessage::allNotesOff(channel), boundary(noteEnd));
    for (auto& slot : clip.effects->plugins)
    {
        if (slot->bypass) continue;
        const bool automate = at >= 0 && ((source.followTrackTempo && source.tempo.points.size() > 1)
            || std::any_of(clip.controls.pluginAutomation.begin(), clip.controls.pluginAutomation.end(), [&](const auto& lane) { return lane.slotId == slot->id && lane.curve.active(); }));
        renderingPlugin = true; clipProcessedMidi.clear();
        for (int offset = first; offset < last;)
        {
            const int64_t sample = (int64_t) std::llround(at * sr) + offset;
            const int count = automate ? std::min(last - offset, 32 - (int) (sample % 32 + 32) % 32) : last - offset;
            const double time = audition ? -1 : (sample - (sample % 32 + 32) % 32) / sr;
            for (const auto& lane : clip.controls.pluginAutomation) if (lane.slotId == slot->id && lane.curve.active())
            {
                auto found = slot->parameters.find(lane.parameterId);
                if (found != slot->parameters.end()) found->second->setValue(automationValueIn(source, {clip.track, AutomationTarget::plugin, lane.slotId, lane.parameterId, clip.id}, time));
            }
            pluginRenderTime = std::max(0.0, at + offset / sr);
            juce::AudioBuffer<float> segment(clipBuffer.getArrayOfWritePointers(), 2, offset, count);
            clipSegmentMidi.clear(); clipSegmentMidi.addEvents(clipMidi, offset, count, -offset);
            if (!processPlugin(*slot, segment, clipSegmentMidi)) { renderingPlugin = false; return false; }
            clipProcessedMidi.addEvents(clipSegmentMidi, 0, count, offset); offset += count;
        }
        clipMidi.swapWith(clipProcessedMidi); renderingPlugin = false;
    }
    for (int offset = first; offset < last;)
    {
        const int64_t sample = (int64_t) std::llround(at * sr) + offset;
        const int count = std::min(last - offset, 32 - (int) (sample % 32 + 32) % 32);
        const double time = (sample - (sample % 32 + 32) % 32) / sr;
        std::array<float, 3> eq;
        for (int band = 0; band < 3; ++band) eq[(size_t) band] = value((AutomationTarget) (band + 1), time);
        clipProcessors[clip.effects].eq.process(clipBuffer, offset, count, sr, eq); offset += count;
    }
    clipProcessors[clip.effects].filter.process(clipBuffer, first, last, sr, at, transition.filters);
    auto& lowPass = clipProcessors[clip.effects].lowPass;
    for (int i = first; i < last; ++i) lowPass.process(clipBuffer, i, 1, sr, value(AutomationTarget::lowPass, audition ? -1 : at + i / sr));
    for (int i = 0; i < frames; ++i)
    {
        const double time = at + i / sr;
        const float gain = juce::Decibels::decibelsToGain(value(AutomationTarget::volume, time)) * transition.gainAt(0, time), pan = value(AutomationTarget::pan, time);
        const bool inside = audition || (time >= clip.start && time < effectEnd && transition.audibleAt(time));
        clipBuffer.getWritePointer(0)[i] *= inside ? gain * std::min(1.0f, 1 - pan) : 0;
        clipBuffer.getWritePointer(1)[i] *= inside ? gain * std::min(1.0f, 1 + pan) : 0;
    }
    return true;
}
void Engine::render(juce::AudioBuffer<float>& output, int frames, double at, juce::MidiBuffer& incoming, const Project* renderSource)
{
    const auto& source = renderSource ? *renderSource : project;
    const juce::ScopedValueSetter<const Project*> context(renderContext, &source);
    juce::ScopedNoDenormals noDenormals;
    // Caller owns lock. Sample buffers and decoded media are prepared before rendering.
    output.clear(0, frames);
    latencyBlocked = false;
    if (hasUnsupportedLatency()) { stopForLatency(output, frames); return; }
    elasticRenderer.beginBlock();
    bool anySolo = false; for (const auto& t : source.tracks) anySolo = anySolo || t.solo;
    for (int track = 0; track < trackCount; ++track)
    {
        auto& buffer = trackBuffers[(size_t) track]; buffer.clear(0, frames);
        const auto* selected = source.clipFor(selectedClip);
        if (at < 0 && !pluginChainHasInstrument(plugins[(size_t) track])
            && !(selected && selected->track == track && selected->effects->hasInstrument()))
        { meters[(size_t) track] = 0; continue; }
        for (const auto& clip : source.clips)
        {
            if (clip.pendingPlacement != PendingPlacement::none) continue;
            const bool audition = at < 0 && clip.id == selectedClip && clip.effects->hasInstrument();
            if (clip.track != track || (!audition && (clip.start >= at + frames / sampleRate || clip.end() + source.effectTailSeconds <= at))) continue;
            if (!clip.prepared && !clip.media->hasPlayback()) continue;
            auto& sourceBuffer = clipBuffer; sourceBuffer.clear(0, frames);
            if (!audition && clip.elasticPlayback) elasticRenderer.add(sourceBuffer, frames, at, sampleRate.load(), clip);
            else if (!audition)
            {
            const int sourceFrames = clip.prepared ? clip.prepared->frames() : clip.media->frames();
            const int sourceChannels = clip.prepared ? clip.prepared->channels() : clip.media->channels();
            const float clipGain = juce::Decibels::decibelsToGain(clip.gainDb);
            const auto envelope = clip.envelope();
            for (int i = 0; i < frames; ++i)
            {
                const double time = at + i / sampleRate.load(), local = time - clip.start;
                if (time < clip.start || time >= clip.end()) continue;
                const double sourcePos = clip.prepared && clip.prepared->clipLocal ? (clip.preparedOffset + local - clip.audioSlip) * clip.media->sampleRate
                    : clip.sourceTimeAt(local) / clip.rate * clip.media->sampleRate - (clip.reversed ? 1.0 : 0.0);
                if (!std::isfinite(sourcePos) || sourcePos < 0 || sourcePos >= sourceFrames) continue;
                const int index = (int) sourcePos;
                const float fraction = (float) (sourcePos - index), gain = envelope.gainAt(local) * clipGain;
                for (int ch = 0; ch < 2; ++ch)
                {
                    const int sourceChannel = std::min(ch, sourceChannels - 1);
                    const auto* samples = clip.prepared ? clip.prepared->channelData(sourceChannel) : clip.media->channelData(sourceChannel);
                    const float interpolated = samples[index] + fraction * (samples[std::min(index + 1, sourceFrames - 1)] - samples[index]);
                    sourceBuffer.addSample(ch, i, interpolated * gain);
                }
            }
            }
            if (!processClipEffects(source, clip, frames, at, incoming)) { stopForLatency(output, frames); return; }
            buffer.addFrom(0, 0, sourceBuffer, 0, 0, frames); buffer.addFrom(1, 0, sourceBuffer, 1, 0, frames);
        }
        auto& midi = trackMidi[(size_t) track]; midi.clear();
        if (track == selectedTrack && (!controlClip(track) || !controlClip(track)->effects->hasInstrument())) midi.addEvents(incoming, 0, frames, 0);
        for (auto& slot : plugins[(size_t) track])
        {
            if (slot->bypass) continue;
            const auto& lanes = source.tracks[(size_t) track].pluginAutomation;
            const bool automate = at >= 0 && ((source.followTrackTempo && source.tempo.points.size() > 1) || std::any_of(lanes.begin(), lanes.end(), [&](const auto& lane) { return lane.slotId == slot->id && lane.curve.active(); }));
            renderingPlugin = true;
            if (!automate)
            {
                pluginRenderTime = std::max(0.0, at);
                if (!processPlugin(*slot, buffer, midi)) { renderingPlugin = false; stopForLatency(output, frames); return; }
            }
            else
            {
                auto& processed = processedMidi[(size_t) track]; processed.clear();
                const double sr = sampleRate.load(); const int64_t firstSample = (int64_t) std::llround(at * sr);
                for (int offset = 0; offset < frames;)
                {
                    const auto sample = firstSample + offset, tick = sample - sample % 32;
                    const int count = std::min(frames - offset, 32 - (int) (sample % 32));
                    for (const auto& lane : lanes) if (lane.slotId == slot->id && lane.curve.active())
                    {
                        const auto found = slot->parameters.find(lane.parameterId);
                        if (found != slot->parameters.end()) found->second->setValue(automationValueIn(source, {track, AutomationTarget::plugin, lane.slotId, lane.parameterId}, tick / sr));
                    }
                    juce::AudioBuffer<float> segment(buffer.getArrayOfWritePointers(), 2, offset, count);
                    auto& events = segmentMidi[(size_t) track]; events.clear(); events.addEvents(midi, offset, count, -offset);
                    pluginRenderTime = (firstSample + offset) / sr;
                    if (!processPlugin(*slot, segment, events)) { renderingPlugin = false; stopForLatency(output, frames); return; }
                    processed.addEvents(events, 0, count, offset); offset += count;
                }
                midi.swapWith(processed);
            }
            renderingPlugin = false;
        }
        const auto& settings = source.tracks[(size_t) track];
        const double sr = sampleRate.load();
        const bool automatedEq = at >= 0 && (settings.automation[1].active() || settings.automation[2].active() || settings.automation[3].active());
        if (automatedEq)
        {
            // A session-sample clock, rather than callback boundaries, owns EQ updates.
            // 32-sample control intervals keep live and exported curves identical.
            const int64_t firstSample = (int64_t) std::llround(at * sr);
            for (int offset = 0; offset < frames;)
            {
                const auto sample = firstSample + offset; const auto tick = sample - sample % 32;
                const int count = std::min(frames - offset, 32 - (int) (sample % 32));
                std::array<float, 3> values;
                for (int band = 0; band < 3; ++band) values[(size_t) band] = automationValueIn(source, {track, (AutomationTarget) (band + 1)}, tick / sr);
                equalizers[(size_t) track].process(buffer, offset, count, sr, values); offset += count;
            }
        }
        else equalizers[(size_t) track].process(buffer, 0, frames, sr, settings.eqDb);
        for (int i = 0; i < frames; ++i) lowPasses[(size_t) track].process(buffer, i, 1, sr, automationValueIn(source, {track, AutomationTarget::lowPass}, at < 0 ? -1 : at + i / sr));
        const bool muted = settings.mute || (anySolo && !settings.solo);
        const float gain = muted ? 0.0f : juce::Decibels::decibelsToGain(settings.gainDb);
        // Centre-unity balance law. Identical in live and offline rendering.
        const float left = gain * std::min(1.0f, 1.0f - settings.pan), right = gain * std::min(1.0f, 1.0f + settings.pan);
        if (!muted && at >= 0 && (settings.automation[0].active() || settings.automation[4].active()))
        {
            auto* l = buffer.getWritePointer(0); auto* r = buffer.getWritePointer(1);
            for (int i = 0; i < frames; ++i)
            {
                const double time = at + i / sr; const float automatedGain = juce::Decibels::decibelsToGain(automationValueIn(source, {track, AutomationTarget::volume}, time));
                const float pan = automationValueIn(source, {track, AutomationTarget::pan}, time);
                l[i] *= automatedGain * std::min(1.0f, 1.0f - pan); r[i] *= automatedGain * std::min(1.0f, 1.0f + pan);
            }
        }
        else { buffer.applyGain(0, 0, frames, left); buffer.applyGain(1, 0, frames, right); }
        meters[(size_t) track] = buffer.getMagnitude(0, frames);
        output.addFrom(0, 0, buffer, 0, 0, frames); output.addFrom(1, 0, buffer, 1, 0, frames);
    }
    processMaster(source, output, frames, at);
}
void Engine::audioDeviceIOCallbackWithContext(const float* const*, int, float* const* outputs, int outputChannels, int frames, const juce::AudioIODeviceCallbackContext&)
{
    for (int ch = 0; ch < outputChannels; ++ch) if (outputs[ch]) juce::FloatVectorOperations::clear(outputs[ch], frames);
    if (exporting) return;
    const juce::ScopedTryLock guard(lock);
    if (!guard.isLocked()) return;
    if (frames > mixBuffer.getNumSamples()) return; // Device must announce size changes before rendering.
    auto& midi = callbackMidi; midi.clear(); midiCollector.removeNextBlockOfMessages(midi, frames); keyboard.processNextMidiBuffer(midi, 0, frames, true);
    const bool running = playing.load(); const double at = position.load();
    const auto deliverMonitor = [&]
    {
        outputPeaks.push(mixBuffer, frames);
        spectrum.push(mixBuffer, frames, sampleRate.load());
        if (monitorMuted.load()) return; // Outputs were cleared; still process audio and advance the clock.
        for (int ch = 0; ch < std::min(2, outputChannels); ++ch)
            if (outputs[ch]) juce::FloatVectorOperations::copy(outputs[ch], mixBuffer.getReadPointer(ch), frames);
    };
    if (rehearsalActive() && !gridAudition.active())
    {
        mixBuffer.clear(0, frames);
        if (running) renderTransitionRehearsal(mixBuffer, frames);
        masterPeak = mixBuffer.getMagnitude(0, frames); masterInputPeak = -1;
        if (masterPeak > 1) masterClipped = true;
        for (auto& meter : meters) meter = 0;
        deliverMonitor();
        return;
    }
    if (gridAudition.active())
    {
        gridAudition.render(mixBuffer, frames, sampleRate);
        masterPeak = mixBuffer.getMagnitude(0, frames); masterInputPeak = -1;
        if (masterPeak > 1) masterClipped = true;
        deliverMonitor();
        return;
    }
    // Instruments stay playable when transport is stopped, without replaying audio clips.
    const double audibleEnd = project.audibleEnd();
    const int activeFrames = running && audibleEnd > 0
        ? (int) juce::jlimit(0., (double) frames, std::ceil((audibleEnd - at) * sampleRate.load() - 1.0e-6)) : frames;
    if (activeFrames > 0) render(mixBuffer, activeFrames, running ? at : -frames / sampleRate.load() - 1.0, midi);
    if (activeFrames < frames) mixBuffer.clear(activeFrames, frames - activeFrames);
    deliverMonitor();
    if (running && !latencyBlocked)
    {
        const double rate = sampleRate.load();
        // Re-anchor on seek, device-rate changes, or legacy callers writing position.
        // Adding block durations to a double accumulates drift over long sessions.
        if (!deviceClockValid || at != deviceClockExpected || rate != deviceClockRate)
        {
            deviceClockAnchor = at; deviceClockFrames = 0; deviceClockRate = rate; deviceClockValid = true;
        }
        deviceClockFrames += frames;
        const double next = deviceClockAnchor + (double) deviceClockFrames / rate;
        deviceClockExpected = audibleEnd > 0 ? std::min(next, audibleEnd) : next;
        position = deviceClockExpected;
        if (next >= audibleEnd && audibleEnd > 0) playing = false;
    }
}
juce::String Engine::instantiate(const juce::PluginDescription& desc, std::unique_ptr<PluginSlot>& result)
{
    juce::String error;
    auto processor = pluginFormats.createPluginInstance(desc, sampleRate, blockSize, error);
    if (!processor) return error.isNotEmpty() ? error : "VST3 could not be instantiated.";
    // This first host has a stereo main bus. Sidechains and multichannel buses are not advertised.
    auto layout = processor->getBusesLayout();
    for (int i = 0; i < layout.inputBuses.size(); ++i) layout.inputBuses.getReference(i) = i == 0 && !desc.isInstrument ? juce::AudioChannelSet::stereo() : juce::AudioChannelSet::disabled();
    for (int i = 0; i < layout.outputBuses.size(); ++i) layout.outputBuses.getReference(i) = i == 0 ? juce::AudioChannelSet::stereo() : juce::AudioChannelSet::disabled();
    if (!processor->setBusesLayout(layout) || processor->getTotalNumInputChannels() > 2 || processor->getTotalNumOutputChannels() != 2)
        return "This plugin does not support the host's stereo main bus.";
    processor->setPlayHead(this); processor->setRateAndBufferSizeDetails(sampleRate, blockSize); processor->prepareToPlay(sampleRate, blockSize);
    result = std::make_unique<PluginSlot>(); result->description = desc; result->processor = std::move(processor);
    for (auto* parameter : result->processor->getParameters())
    {
        const auto id = parameterIdentity(parameter);
        if (parameter->isAutomatable() && id.isNotEmpty()) result->parameters.emplace(id, parameter);
    }
    return {};
}
juce::String Engine::addPlugin(int track, const juce::File& file)
{
    if (track < 0 || track >= trackCount) return "Invalid track.";
    juce::OwnedArray<juce::PluginDescription> types;
    pluginFormats.getFormat(0)->findAllTypesForFile(types, file.getFullPathName());
    if (types.isEmpty()) return "No compatible 64-bit VST3 was found in " + file.getFullPathName();
    if (types.size() != 1) return "This module contains multiple plugin classes; class selection is not yet supported.";
    std::unique_ptr<PluginSlot> slot;
    auto error = instantiate(*types[0], slot); if (error.isNotEmpty()) return error;
    const juce::ScopedLock guard(lock); endTransitionRehearsal(); auto& chain = pluginChain(track); chain.push_back(std::move(slot));
    if (chain.size() == 1) defaultPluginControls(controlSettings(track), chain);
    return {};
}
juce::String Engine::pluginControlSummary(int track)
{
    const juce::ScopedLock guard(lock);
    if (track < 0 || track >= trackCount) return {};
    juce::String result;
    for (size_t i = 0; i < controlSettings(track).pluginControls.size(); ++i)
    {
        const auto& control = controlSettings(track).pluginControls[i];
        auto* parameter = findControl(control, pluginChain(track));
        result += "Control " + juce::String((int) i + 1) + ": ";
        if (parameter) result += pluginChain(track)[(size_t) control.insert]->description.name + " / " + parameter->getName(128);
        else result += control.parameterId.isEmpty() ? "Unassigned" : "Parameter unavailable";
        result += "\n";
    }
    return result;
}
juce::String Engine::assignPluginControl(int track, int control, int insert, int parameter)
{
    const juce::ScopedLock guard(lock);
    setAutomationRecordMode(AutomationRecordMode::read);
    if (track < 0 || track >= trackCount || control < 0 || control >= 8) return "Invalid plugin control.";
    auto& target = controlSettings(track).pluginControls[(size_t) control];
    if (parameter == -1) { target = {}; return {}; }
    const auto& chain = pluginChain(track);
    if (insert < 0 || insert >= (int) chain.size()) return "The selected insert is unavailable.";
    const auto& parameters = chain[(size_t) insert]->processor->getParameters();
    if (parameter < 0 || parameter >= parameters.size()) return "The selected parameter is unavailable.";
    auto id = parameterIdentity(parameters[parameter]);
    if (id.isEmpty() || !parameters[parameter]->isAutomatable()) return "This parameter cannot be assigned.";
    target = { insert, id }; return {};
}
juce::var Engine::captureProject()
{
    const juce::ScopedLock guard(lock); project.ensureTransitionLinks(); auto data = projectToJson(project, library);
    auto capture = [](const std::vector<std::unique_ptr<PluginSlot>>& plugins)
    {
        juce::Array<juce::var> chain;
        for (const auto& slot : plugins)
        {
            juce::MemoryBlock state; slot->processor->getStateInformation(state);
            auto* obj = new juce::DynamicObject(); obj->setProperty("description", slot->description.createXml()->toString());
            obj->setProperty("state", state.toBase64Encoding()); obj->setProperty("bypass", slot->bypass); obj->setProperty("id", slot->id); chain.add(juce::var(obj));
        }
        return chain;
    };
    for (int i = 0; i < trackCount; ++i) data["tracks"].getArray()->getReference(i).getDynamicObject()->setProperty("plugins", capture(plugins[(size_t) i]));
    for (int i = 0; i < (int) project.clips.size(); ++i) data["clips"].getArray()->getReference(i).getDynamicObject()->setProperty("plugins", capture(project.clips[(size_t) i].effects->plugins));
    return data;
}
juce::String Engine::saveProject(const juce::File& file)
{
    juce::var document;
    { const juce::ScopedLock guard(lock); if (!playbackReady()) return "Audio is still preparing or unavailable. Wait for the source checks before saving."; const auto destination = validateWriteDestination(file); if (destination.failed()) return destination.getErrorMessage(); document = captureProject(); }
    return writeWithBackup(file, juce::JSON::toString(document, false, 17));
}
juce::String Engine::exportPlaylist(const juce::File& file, PlaylistExport format)
{
    if (!file.hasFileExtension(format == PlaylistExport::m3u ? "m3u;m3u8" : "txt")) return "Choose a matching playlist file extension.";
    PlaylistTextResult result;
    {
        const juce::ScopedLock guard(lock);
        const auto destination = validateWriteDestination(file); if (destination.failed()) return destination.getErrorMessage();
        result = playlistText(project, format);
    }
    if (result.error.isNotEmpty()) return result.error;
    return writeWithBackup(file, result.text);
}
PreparedProject Engine::prepareProject(const juce::String& text, const ProjectProgress& progress, const std::shared_ptr<std::atomic<bool>>& cancelled)
{
    PreparedProject result; result.parsed = parseProject(text);
    if (result.parsed.error.isNotEmpty()) { result.error = result.parsed.error; return result; }
    juce::AudioFormatManager mediaFormats; mediaFormats.registerBasicFormats();
    std::set<juce::String> usedMedia;
    for (const auto& reference : result.parsed.clips) usedMedia.insert(reference.mediaId);
    std::map<juce::String, std::shared_ptr<Media>> byId;
    for (const auto& reference : result.parsed.media)
    {
        if (cancelled && *cancelled) { result.error = "Session opening cancelled. The current session was kept."; return result; }
        const int index = (int) result.library.size(), total = (int) result.parsed.media.size();
        auto loaded = readMedia(mediaFormats, juce::File(reference.path), [&](MediaReadStage phase)
        {
            if (progress) progress(index, total, juce::File(reference.path).getFileName(), phase);
        }, AnalysisCacheMode::readWrite, usedMedia.count(reference.id) ? MediaReadPurpose::playback : MediaReadPurpose::catalogue, nullptr, cancelled);
        if (!loaded.media) { result.error = loaded.error + "\nThe current session was kept."; return result; }
        loaded.media->id = reference.id; loaded.media->name = reference.name; loaded.media->manualBpm = reference.bpm;
        loaded.media->manualBeatOrigin = reference.beatOrigin; loaded.media->manualKey = reference.key;
        if (reference.beatOrigin > loaded.media->duration()) { result.error = "The beat marker exceeds source audio. The current session was kept."; return result; }
        byId[reference.id] = loaded.media; result.library.push_back(loaded.media);
        if (progress) progress(index + 1, total, loaded.media->name, MediaReadStage::cache);
    }
    for (const auto& reference : result.parsed.clips)
    {
        Clip clip = reference.clip; clip.media = byId.at(reference.mediaId);
        if (clip.loopSpan > 0 && clip.loopSpan < 1.0 / clip.media->sampleRate) { result.error = "A loop must contain at least one source sample. The current session was kept."; return result; }
        if (clip.offset + clip.sourceSpan() > clip.media->duration() + 1.0 / clip.media->sampleRate) { result.error = "Clip exceeds source audio. The current session was kept."; return result; }
        result.parsed.project.clips.push_back(clip);
    }
    if (cancelled && *cancelled) { result.error = "Session opening cancelled. The current session was kept."; return result; }
    if (progress) progress((int) result.library.size(), (int) result.library.size(), "Preparing mix playback", MediaReadStage::tempo);
    result.error = prepareSavedClips(result.parsed.project); return result;
}
juce::String Engine::restoreProject(const juce::String& text) { return commitProject(prepareProject(text)); }
juce::String Engine::commitProject(PreparedProject prepared)
{
    if (prepared.error.isNotEmpty()) return prepared.error;
    auto& result = prepared.parsed;
    std::array<std::vector<std::unique_ptr<PluginSlot>>, trackCount> stagedPlugins;
    for (int i = 0; i < result.document["tracks"].getArray()->size(); ++i)
        for (const auto& p : *result.document["tracks"].getArray()->getReference(i)["plugins"].getArray())
        {
            juce::PluginDescription desc;
            auto xml = juce::parseXML(p["description"].toString());
            if (!desc.loadFromXml(*xml) || desc.pluginFormatName != "VST3") return "Invalid VST3 description. The current session was kept.";
            juce::MemoryBlock state;
            if (!state.fromBase64Encoding(p["state"].toString())) return "Invalid plugin state encoding.";
            std::unique_ptr<PluginSlot> slot; auto error = instantiate(desc, slot);
            if (error.isNotEmpty()) return desc.name + ": " + error + "\nThe current session was kept.";
            if (state.getSize() > (size_t) std::numeric_limits<int>::max()) return "Plugin state is too large.";
            slot->processor->setStateInformation(state.getData(), (int) state.getSize()); slot->bypass = (bool) p["bypass"];
            if (p["id"].isString()) slot->id = p["id"].toString();
            stagedPlugins[(size_t) i].push_back(std::move(slot));
        }
    for (int i = 0; i < trackCount; ++i)
        for (const auto& lane : result.project.tracks[(size_t) i].pluginAutomation)
        {
            auto found = std::find_if(stagedPlugins[(size_t) i].begin(), stagedPlugins[(size_t) i].end(), [&](const auto& slot) { return slot->id == lane.slotId; });
            if (found == stagedPlugins[(size_t) i].end() || !(*found)->parameters.count(lane.parameterId)) return "An automated VST parameter is unavailable. The current session was kept.";
        }
    for (int i = 0; i < trackCount; ++i)
        if (i >= result.document["tracks"].getArray()->size() || !result.document["tracks"].getArray()->getReference(i).hasProperty("pluginControls")) defaultPluginControls(result.project.tracks[(size_t) i], stagedPlugins[(size_t) i]);
    if ((int) result.document["version"] >= 11) for (int i = 0; i < (int) result.project.clips.size(); ++i)
    {
        auto& c = result.project.clips[(size_t) i];
        for (const auto& p : *result.document["clips"].getArray()->getReference(i)["plugins"].getArray())
        {
            juce::PluginDescription desc; auto xml = juce::parseXML(p["description"].toString());
            if (!xml || !desc.loadFromXml(*xml) || desc.pluginFormatName != "VST3") return "Invalid audio-track VST3 description.";
            juce::MemoryBlock state; if (!state.fromBase64Encoding(p["state"].toString()) || state.getSize() > (size_t) std::numeric_limits<int>::max()) return "Invalid audio-track plugin state.";
            std::unique_ptr<PluginSlot> slot; auto error = instantiate(desc, slot); if (error.isNotEmpty()) return error;
            slot->processor->setStateInformation(state.getData(), (int) state.getSize()); slot->id = p["id"].toString(); slot->bypass = (bool) p["bypass"];
            c.effects->plugins.push_back(std::move(slot));
        }
        for (const auto& lane : c.controls.pluginAutomation)
        {
            auto found = std::find_if(c.effects->plugins.begin(), c.effects->plugins.end(), [&](const auto& slot) { return slot->id == lane.slotId; });
            if (found == c.effects->plugins.end() || !(*found)->parameters.count(lane.parameterId)) return "An audio-track VST parameter is unavailable.";
        }
    }
    // Earlier sessions stored timeline automation and inserts on mixer lanes.
    // Transfer their established controls to the audio tracks before publishing.
    if ((int) result.document["version"] < 11) for (int track = 0; track < trackCount; ++track)
    {
        auto shared = makeClipEffects(); bool used = false;
        for (auto& c : result.project.clips) if (c.track == track)
        {
            if (!used) shared->plugins = std::move(stagedPlugins[(size_t) track]);
            c.adoptTimelineControls(result.project.tracks[(size_t) track]); c.effects = shared;
            if (used) { auto error = cloneClipEffects(c); if (error.isNotEmpty()) return error; }
            used = true;
        }
        if (used)
        {
            auto& lane = result.project.tracks[(size_t) track]; lane.gainDb = lane.pan = 0; lane.eqDb = {};
            lane.automation = {}; lane.pluginAutomation.clear(); lane.pluginControls = {};
        }
    }
    const juce::ScopedLock guard(lock);
    finishAutomation(); automationRecordMode = AutomationRecordMode::read; automationArmed.fill(false); masterAutomationArmed = false;
    endTransitionRehearsal();
    playing = false; position = 0; deviceClockValid = false; keyboard.allNotesOff(0);
    masterClipped = false; masterPeak = masterInputPeak = 0;
    project = std::move(result.project); tempo = project.bpm; library = std::move(prepared.library); plugins = std::move(stagedPlugins);
    masterEqualizer.reset();
    for (auto& eq : equalizers) eq.reset(); for (auto& filter : lowPasses) filter.reset(); prepareClipProcessing();
    return {};
}

void Engine::handleIncomingMidiMessage(juce::MidiInput* input, const juce::MidiMessage& message)
{
    if (exporting) return; // Do not replay notes queued during an exclusive offline job.
    // Clock is frequent and must not overwrite the last useful control/note observation.
    if (message.isMidiClock() || message.isActiveSense()) { midiCollector.addMessageToQueue(message); return; }
    const juce::ScopedLock guard(midiLock);
    if (input) midiInputActivity[input->getIdentifier()] = juce::Time::getMillisecondCounterHiRes();
    if (message.isNoteOn()) ++midiNotes;
    midiLastMessage = message.getDescription();
    const bool cc = message.isController(), note = message.isNoteOn();
    bool consumed = false;
    if (cc || note || message.isNoteOff())
    {
        const int status = cc ? 0xb0 : 0x90, channel = message.getChannel(), number = cc ? message.getControllerNumber() : message.getNoteNumber();
        if (learnAction.hasValue() && (note || (cc && (continuousMidiAction(*learnAction) || message.getControllerValue() > 0))))
        {
            // Guided setup advances only on a different, unassigned physical message.
            // A moving fader emits many events; those must not fill subsequent steps.
            const int physical = status * 65536 + channel * 256 + number;
            if (!learnSteps.empty() && learnedControls.count(physical) != 0) { triggerAsyncUpdate(); return; }
            if (!learnSteps.empty() && !replaceControllerProfile)
                for (const auto& b : mappings)
                    if (b.status == status && b.channel == channel && b.number == number) { triggerAsyncUpdate(); return; }
            const auto action = *learnAction;
            mappings.erase(std::remove_if(mappings.begin(), mappings.end(), [&](const MidiBinding& b) { return b.action == action || (b.status == status && b.channel == channel && b.number == number); }), mappings.end());
            mappings.push_back({ status, channel, number, action }); learnAction.reset(); consumed = true;
            if (!learnSteps.empty()) learnedControls.insert(physical);
            if (!continuousMidiAction(action)) pressedMidiButtons.insert(status * 65536 + channel * 256 + number);
            mappingsDirty = true;
            if (!learnSteps.empty())
            {
                ++learnStep;
                if (learnStep < learnSteps.size()) learnAction = learnSteps[learnStep].action;
            }
        }
        else if (learnAction.hasValue())
        {
            // Button releases must neither fill the next step nor operate an old mapping.
            pressedMidiButtons.erase(status * 65536 + channel * 256 + number); consumed = true;
        }
        else
            for (const auto& b : mappings)
                if (b.status == status && b.channel == channel && b.number == number)
                {
                    consumed = true;
                    const bool continuous = continuousMidiAction(b.action);
                    const bool down = note || (cc && message.getControllerValue() > 0);
                    const int buttonKey = status * 65536 + channel * 256 + number;
                    if (!continuous)
                    {
                        if (!down) { pressedMidiButtons.erase(buttonKey); break; }
                        if (!pressedMidiButtons.insert(buttonKey).second) break;
                    }
                    if (message.isNoteOff()) break;
                    int routingTrack = selectedTrack.load();
                    for (const auto& pending : pendingControls) if (pending.value > 0)
                    {
                        if (pending.action.kind == MidiActionKind::select) routingTrack = pending.action.index;
                        if (pending.action.kind == MidiActionKind::previousTrack) routingTrack = (routingTrack + trackCount - 1) % trackCount;
                        if (pending.action.kind == MidiActionKind::nextTrack) routingTrack = (routingTrack + 1) % trackCount;
                    }
                    pendingControls.push_back({ b.action, (cc ? message.getControllerValue() : message.getVelocity()) / 127.0f, position.load(), playing.load(), routingTrack }); consumed = true; break;
                }
    }
    if (message.isMidiStart() || message.isMidiContinue()) { pendingControls.push_back({{MidiActionKind::start}, 1.0f, position.load(), playing.load()}); consumed = true; }
    if (message.isMidiStop()) { pendingControls.push_back({{MidiActionKind::stop}, 1.0f, position.load(), playing.load()}); consumed = true; }
    if (!consumed) midiCollector.addMessageToQueue(message);
    triggerAsyncUpdate();
}
void Engine::handleAsyncUpdate()
{
    std::vector<ControlEvent> controls;
    bool saveMappings = false;
    { const juce::ScopedLock guard(midiLock); controls.swap(pendingControls); saveMappings = mappingsDirty; mappingsDirty = false; }
    if (saveMappings) saveMidiMappings();
    bool changed = false;
    for (const auto& event : controls) changed = applyControl(event) || changed;
    if (!controls.empty() && onControl) onControl(changed);
    if (onMidiChanged) onMidiChanged();
}
void Engine::saveMidiMappings()
{
    juce::Array<juce::var> values;
    { const juce::ScopedLock guard(midiLock);
      for (const auto& b : mappings) { auto* o = new juce::DynamicObject(); o->setProperty("status", b.status); o->setProperty("channel", b.channel); o->setProperty("number", b.number); o->setProperty("action", midiActionId(b.action)); values.add(juce::var(o)); } }
    const auto error = writeWithBackup(mappingsFile, juce::JSON::toString(values, false));
    const juce::ScopedLock guard(midiLock); midiProfileError = error;
}
juce::String Engine::midiSummary() const { return juce::String(midiNotes.load()) + " notes received"; }
void Engine::beginLearn(const juce::String& action) { const juce::ScopedLock guard(midiLock); learnSteps.clear(); learnStep = 0; learnAction = parseMidiAction(action); }
void Engine::beginControllerSetup(bool liveDesk)
{
    const juce::ScopedLock guard(midiLock); learnSteps.clear(); learnStep = 0; replaceControllerProfile = liveDesk; learnedControls.clear();
    auto add = [&](MidiAction a, const juce::String& control)
    {
        if (!liveDesk) for (const auto& binding : mappings) if (binding.action == a) return;
        learnSteps.push_back({a, control});
    };
    if (liveDesk)
    {
        for (int i = 0; i < 8; ++i) add({MidiActionKind::deskPot, i}, "Pot " + juce::String(i + 1));
        for (int i = 0; i < 8; ++i) add({i < 4 ? MidiActionKind::volume : MidiActionKind::pan, i % 4}, "Fader " + juce::String(i + 1));
        add({MidiActionKind::master}, "Fader 9 (master)");
        for (int i = 0; i < 8; ++i) add({MidiActionKind::cyclePot, i}, "Top pad " + juce::String(i + 1));
        for (int i = 0; i < 8; ++i) add({i < 4 ? MidiActionKind::mute : MidiActionKind::solo, i % 4}, "Bottom pad " + juce::String(i + 1));
        for (int i = 0; i < 8; ++i) add({i < 4 ? MidiActionKind::resetEq : MidiActionKind::resetLowPass, i % 4}, "Fader button " + juce::String(i + 1));
        add({MidiActionKind::resetPans}, "Master button");
        for (auto kind : {MidiActionKind::undo, MidiActionKind::redo, MidiActionKind::play, MidiActionKind::stop, MidiActionKind::snap, MidiActionKind::record}) add({kind}, midiActionId({kind}));
        learnAction = learnSteps.empty() ? juce::Optional<MidiAction>() : juce::Optional<MidiAction>(learnSteps[0].action); return;
    }
    for (int i = 0; i < 4; ++i) add({MidiActionKind::volume, i}, "Fader " + juce::String(i + 1));
    add({MidiActionKind::master}, "Fader 9 (master)");
    for (int i = 0; i < 4; ++i) add({MidiActionKind::mid, i}, "Fader " + juce::String(i + 5) + " (mid EQ)");
    for (int i = 0; i < 4; ++i) add({MidiActionKind::bass, i}, "Pot " + juce::String(i + 1) + " (bass)");
    for (int i = 0; i < 4; ++i) add({MidiActionKind::treble, i}, "Pot " + juce::String(i + 5) + " (high)");
    for (int i = 0; i < 4; ++i) add({MidiActionKind::select, i}, "Pad " + juce::String(i + 1));
    for (int i = 0; i < 4; ++i) add({MidiActionKind::mute, i}, "Pad " + juce::String(i + 5));
    for (int i = 0; i < 4; ++i) add({MidiActionKind::solo, i}, "Pad " + juce::String(i + 9));
    add({MidiActionKind::arm}, "Pad 13 (arm selected lane)");
    add({MidiActionKind::home}, "Pad 14 (return to start)"); add({MidiActionKind::play}, "Pad 15 (play/pause)"); add({MidiActionKind::stop}, "Pad 16 (stop)");
    add({MidiActionKind::record}, "Record button");
    for (int i = 0; i < 4; ++i) add({MidiActionKind::armTrack, i}, "Fader button " + juce::String(i + 1) + " (arm lane)");
    for (int i = 0; i < 4; ++i) add({MidiActionKind::resetEq, i}, "Fader button " + juce::String(i + 5) + " (reset EQ)");
    add({MidiActionKind::armMaster}, "Master fader button (arm master)");
    add({MidiActionKind::previousTrack}, "Mixer left button"); add({MidiActionKind::nextTrack}, "Mixer right button");
    add({MidiActionKind::previousTransition}, "Preset previous button"); add({MidiActionKind::nextTransition}, "Preset next button");
    add({MidiActionKind::undo}, "Undo button"); add({MidiActionKind::redo}, "Redo button");
    learnAction = learnSteps.empty() ? juce::Optional<MidiAction>() : juce::Optional<MidiAction>(learnSteps[0].action);
}
void Engine::skipControllerStep()
{
    const juce::ScopedLock guard(midiLock);
    if (learnStep < learnSteps.size()) ++learnStep;
    learnAction = learnStep < learnSteps.size() ? juce::Optional<MidiAction>(learnSteps[learnStep].action) : juce::Optional<MidiAction>();
}
juce::String Engine::lastMidiText() { const juce::ScopedLock guard(midiLock); return midiLastMessage; }
juce::String Engine::mappingSummary()
{
    const juce::ScopedLock guard(midiLock);
    juce::String text = learnAction.hasValue() ? "Waiting for a knob, fader, pad or button for: " + midiActionId(*learnAction) + "\n\n" : "Select an action, click Learn, then move a control. Mappings save automatically.\n\n";
    if (!learnSteps.empty()) text = learnStep < learnSteps.size() ? "FLkey setup " + juce::String((int) learnStep + 1) + "/" + juce::String((int) learnSteps.size()) + ": move " + learnSteps[learnStep].control + " -> " + midiActionId(learnSteps[learnStep].action) + "\nUse Custom Mode. Only move the named control; repeated events are ignored.\n\n" : "FLkey setup complete. All assignments are saved automatically.\n\n";
    text += "Active mappings: " + juce::String((int) mappings.size()) + "\nProfile: " + mappingsFile.getFullPathName() + "\n";
    if (midiProfileError.isNotEmpty()) text += "Profile error: " + midiProfileError + "\n";
    text += "\n";
    for (const auto& b : mappings) text += midiActionId(b.action) + "  <-  " + (b.status == 0xb0 ? "CC " : "Note ") + juce::String(b.number) + " / channel " + juce::String(b.channel) + "\n";
    return text;
}
void Engine::silenceNotes()
{
    const juce::ScopedLock guard(lock);
    keyboard.allNotesOff(0);
    midiCollector.reset(sampleRate.load());
    juce::MidiBuffer notes;
    for (int ch = 1; ch <= 16; ++ch) { notes.addEvent(juce::MidiMessage::allNotesOff(ch), 0); notes.addEvent(juce::MidiMessage::allSoundOff(ch), 0); }
    eachPlugin([&](auto& p) { mixBuffer.clear(); p.processor->processBlock(mixBuffer, notes); p.processor->reset(); });
}
bool Engine::applyMidiControl(MidiAction action, float value)
{
    return applyControl({action, value, position.load(), playing.load()});
}
bool Engine::applyControl(const ControlEvent& event)
{
    if (exporting || !controlInputEnabled) return false;
    const juce::ScopedLock guard(lock);
    const auto action = event.action; const float value = juce::jlimit(0.0f, 1.0f, event.value);
    if (rehearsalActive() && action.kind != MidiActionKind::play && action.kind != MidiActionKind::start && action.kind != MidiActionKind::stop) endTransitionRehearsal();
    switch (action.kind)
    {
        case MidiActionKind::deskPot:
        {
            if (action.index < 0 || action.index >= 8) return false;
            auto routed = event; routed.action = deskPotAction(action.index); return applyControl(routed);
        }
        case MidiActionKind::cyclePot: if (value > 0) cycleDeskLane(action.index); return false;
        case MidiActionKind::resetPans:
        {
            bool changed = false; if (value > 0) for (int i = 0; i < trackCount; ++i) changed = setControlValueAt({i, AutomationTarget::pan}, 0, event.time, event.running) || changed;
            return changed;
        }
        case MidiActionKind::record:
            if (value > 0)
            {
                const bool arm = automationRecordMode == AutomationRecordMode::read;
                setAutomationRecordMode(arm ? AutomationRecordMode::touch : AutomationRecordMode::read);
                if (arm) { for (int i = 0; i < trackCount; ++i) armAutomation(i, true); armAutomation(-1, true); }
            }
            return false;
        case MidiActionKind::read: if (value > 0) setAutomationRecordMode(AutomationRecordMode::read); return false;
        case MidiActionKind::arm: if (value > 0) armAutomation(selectedTrack, !automationArmed[(size_t) selectedTrack.load()]); return false;
        case MidiActionKind::armTrack: if (value > 0 && action.index >= 0 && action.index < trackCount) armAutomation(action.index, !automationArmed[(size_t) action.index]); return false;
        case MidiActionKind::armMaster: if (value > 0) armAutomation(-1, !masterAutomationArmed); return false;
        case MidiActionKind::previousTrack: case MidiActionKind::nextTrack:
            if (value > 0) { silenceNotes(); selectedTrack = (selectedTrack + (action.kind == MidiActionKind::nextTrack ? 1 : trackCount - 1)) % trackCount; } return false;
        case MidiActionKind::panic: if (value > 0) silenceNotes(); return false;
        case MidiActionKind::previousTransition: case MidiActionKind::nextTransition: case MidiActionKind::undo: case MidiActionKind::redo:
            if (value > 0 && onPerformanceAction) onPerformanceAction(action); return false;
        case MidiActionKind::snap:
            if (value > 0) { if (onProjectSwitchBegin) onProjectSwitchBegin(); project.snap = !project.snap; } return value > 0;
        case MidiActionKind::play: if (value > 0) setPlaying(!playing); return false;
        case MidiActionKind::start: if (value > 0) setPlaying(true); return false;
        case MidiActionKind::stop: if (value > 0) stop(); return false;
        case MidiActionKind::home: if (value > 0) seek(0); return false;
        case MidiActionKind::master: return setControlValueAt({-1, AutomationTarget::master}, -60 + value * 66, event.time, event.running);
        case MidiActionKind::pluginParameter:
        {
            const int track = event.targetTrack >= 0 ? event.targetTrack : selectedTrack.load();
            if (track < 0 || track >= trackCount || action.index < 0 || action.index >= 8) return false;
            const auto& control = controlSettings(track).pluginControls[(size_t) action.index];
            if (control.insert < 0 || control.insert >= (int) pluginChain(track).size() || control.parameterId.isEmpty()) return false;
            return setControlValueAt({track, AutomationTarget::plugin, pluginChain(track)[(size_t) control.insert]->id, control.parameterId, controlClip(track) ? selectedClip : juce::String()}, value, event.time, event.running);
        }
        default: break;
    }
    if (action.index < 0 || action.index >= trackCount) return false;
    auto& track = project.tracks[(size_t) action.index];
    switch (action.kind)
    {
        case MidiActionKind::lowPass: return setControlValueAt({action.index, AutomationTarget::lowPass}, (float) (20 * std::pow(1000., value)), event.time, event.running);
        case MidiActionKind::resetLowPass: return value > 0 && setControlValueAt({action.index, AutomationTarget::lowPass}, 20000, event.time, event.running);
        case MidiActionKind::volume: return setControlValueAt({action.index, AutomationTarget::volume}, -60 + value * 66, event.time, event.running);
        case MidiActionKind::pan: return setControlValueAt({action.index, AutomationTarget::pan}, value * 2 - 1, event.time, event.running);
        case MidiActionKind::bass: case MidiActionKind::mid: case MidiActionKind::treble:
            return setControlValueAt({action.index, action.kind == MidiActionKind::bass ? AutomationTarget::bass : action.kind == MidiActionKind::mid ? AutomationTarget::mid : AutomationTarget::treble}, -60 + value * 72, event.time, event.running);
        case MidiActionKind::mute: case MidiActionKind::solo:
            if (value > 0)
            {
                if (onProjectSwitchBegin) onProjectSwitchBegin();
                auto& enabled = action.kind == MidiActionKind::mute ? track.mute : track.solo; enabled = !enabled;
            }
            return value > 0;
        case MidiActionKind::resetEq:
        {
            bool changed = false;
            if (value > 0) for (auto target : {AutomationTarget::bass, AutomationTarget::mid, AutomationTarget::treble})
                changed = setControlValueAt({action.index, target}, 0, event.time, event.running) || changed;
            return changed;
        }
        case MidiActionKind::select: if (value > 0) { silenceNotes(); selectedTrack = action.index; } return false;
        default: return false;
    }
}
AutomationAddress Engine::deskPotAddress(int slot) const
{
    const AutomationTarget targets[] {AutomationTarget::bass, AutomationTarget::mid, AutomationTarget::treble, AutomationTarget::lowPass};
    jassert(slot >= 0 && slot < 8);
    return {deskLanes[(size_t) slot], targets[slot / 2]};
}
MidiAction Engine::deskPotAction(int slot) const
{
    const auto address = deskPotAddress(slot);
    const MidiActionKind kinds[] {MidiActionKind::bass, MidiActionKind::mid, MidiActionKind::treble, MidiActionKind::lowPass};
    return {kinds[slot / 2], address.track};
}
void Engine::cycleDeskLane(int slot)
{
    const juce::ScopedLock guard(lock);
    if (slot < 0 || slot >= 8) return;
    auto& lane = deskLanes[(size_t) slot];
    do { lane = (lane + 1) % trackCount; } while (lane == deskLanes[(size_t) (slot ^ 1)]);
}

}
