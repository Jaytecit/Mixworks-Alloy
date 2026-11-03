#include "Engine.h"
#include "EffectTail.h"
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
        || a.kind == MidiActionKind::cutoff || a.kind == MidiActionKind::deskPot || a.kind == MidiActionKind::bass || a.kind == MidiActionKind::mid || a.kind == MidiActionKind::treble || a.kind == MidiActionKind::pluginParameter;
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
const juce::String liveInputRequiresWav="Live instrument/MIDI input needs the optional master WAV for the audible record; note recording is not implemented.";
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
    onPerformanceBegin={}; onPerformanceFinalised={};
    onControl = {}; onMidiChanged = {};
    if (attached) devices.removeAudioCallback(this);
    finalisePerformance(); detachPerformanceParameters();
    devices.removeMidiInputDeviceCallback({}, this);
    cancelPendingUpdate(); devices.closeAudioDevice();
    const juce::ScopedLock guard(lock);
    eachPlugin([](auto& slot) { slot.processor->releaseResources(); });
}
void Engine::eachPlugin(const std::function<void(PluginSlot&)>& visit)
{
    for (auto& chain : plugins) for (auto& slot : chain) visit(*slot);
    for (auto& c : project.clips) if (project.effectOwner(c) == &c) for (auto& slot : c.effects->plugins) visit(*slot);
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
const juce::String& effectsIdentity(const std::shared_ptr<ClipEffects>& effects) { return effects->id; }
void restoreEffectsIdentity(const std::shared_ptr<ClipEffects>& effects,const juce::String& id) { effects->id=id; }
namespace
{
double chainTail(const std::vector<std::unique_ptr<PluginSlot>>& plugins)
{
    double seconds = 0;
    for (const auto& slot : plugins) if (!slot->bypass)
    {
        const double tail = slot->processor->getTailLengthSeconds();
        if (std::isfinite(tail) && tail > 0) seconds += std::min(tail, 1.e9);
    }
    return std::min(seconds, 1.e9);
}
}
double naturalEffectTail(const std::shared_ptr<ClipEffects>& effects) { return chainTail(effects->plugins); }
double effectAudibleEnd(const Engine& engine)
{
    const auto& project = engine.project;
    double end = project.audibleEnd();
    std::array<double, trackCount> tails {};
    for (int lane = 0; lane < trackCount; ++lane)
    { tails[(size_t)lane] = chainTail(engine.plugins[(size_t)lane]); end = std::max(end, project.duration() + tails[(size_t)lane]); }
    for (const auto& clip : project.clips)
        end = std::max(end, clip.end() + std::max(project.effectTailSeconds, naturalEffectTail(clip.effects)) + tails[(size_t)clip.track]);
    return end;
}
Clip* Engine::controlClip(int track)
{
    auto* c = project.clipFor(selectedClip); return c && c->track == track ? c : nullptr;
}
const Clip* Engine::controlClip(int track) const { return const_cast<Engine*>(this)->controlClip(track); }
TrackSettings& Engine::controlSettings(int track) { return project.tracks[(size_t) track]; }
TrackSettings& Engine::controlEffectSettings(int track) { auto* c = controlClip(track); return c ? project.effectSettings(*c) : project.tracks[(size_t)track]; }
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
    const auto settings = std::as_const(project).effectSettings(clip);
    clip.controls = settings;
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
juce::String Engine::startDevice(bool enableMidi)
{
    auto error = devices.initialiseWithDefaultDevices(0, 2);
    if (error.isNotEmpty()) return error;
    devices.addAudioCallback(this); attached = true;
    if (enableMidi)
    {
        devices.addMidiInputDeviceCallback({}, this);
        midiCallbackRegistered = true;
        midiConnectionStatus(); // The same policy handles initial detection and later reconnection.
    }
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
    privatePreviewBuffer.setSize(2,size);
    takeBuffer.setSize(2,size);
    for (auto& b : trackBuffers) b.setSize(2, size);
    for (auto& b : trackMidi) b.ensureSize(4096);
    for (auto& b : segmentMidi) b.ensureSize(4096);
    for (auto& b : processedMidi) b.ensureSize(4096);
    liveClipMidi.ensureSize(4096); callbackMidi.ensureSize(4096); clipMidi.ensureSize(4096); clipSegmentMidi.ensureSize(4096); clipProcessedMidi.ensureSize(4096);
    clipBuffer.setSize(2, size); effectBuffer.setSize(2, size);
}
void Engine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    const juce::ScopedLock guard(lock);
    privatePreviewSource=nullptr; libraryPreviewSource = nullptr; ++libraryPreviewGeneration;
    cancelScheduledPause(ScheduledPauseReason::deviceChanged); cancelLiveHotcues(ScheduledPauseReason::deviceChanged,true);
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
    privatePreviewSource=nullptr; libraryPreviewSource = nullptr; ++libraryPreviewGeneration;
    if (performanceActive) performanceEnded=true;
    cancelScheduledPause(ScheduledPauseReason::deviceChanged); cancelLiveHotcues(ScheduledPauseReason::deviceChanged,true);
    gridAudition.stop();
    eachPlugin([](auto& p) { p.processor->releaseResources(); });
}
void Engine::stop()
{
    const juce::ScopedLock guard(lock); finalisePerformance(); endTakeAudition(); gridAudition.stop(); endTransitionRehearsal();
    libraryPreviewSource = nullptr; ++libraryPreviewGeneration;
    serviceScheduledPause(); cancelScheduledPause(ScheduledPauseReason::transportChanged); cancelLiveHotcues(ScheduledPauseReason::transportChanged,true);
    finishAutomation(); automationRecordMode = AutomationRecordMode::read; replayShifts.clear(); cancelPreparedLaunch();
    playing = false; position = project.playbackStart(); ++visualClockRevision; deviceClockValid = false; keyboard.allNotesOff(0); resetPlugins();
}
void Engine::seek(double seconds)
{
    const juce::ScopedLock guard(lock); endTakeAudition(); endTransitionRehearsal();
    if (capturingPerformance()) { TakeEvent event; event.kind=TakeEventKind::seek; event.value=std::max(0.,seconds); event.sourcePosition=position.load(); appendTakeEvent(std::move(event)); }
    cancelPreparedLaunch(); libraryPreviewSource = nullptr; ++libraryPreviewGeneration;
    serviceScheduledPause(); cancelScheduledPause(ScheduledPauseReason::transportChanged); cancelLiveHotcues(ScheduledPauseReason::transportChanged,true);
    finishAutomation();
    for (auto& shift:replayShifts) shift.second=0;
    position = std::max(0.0, seconds); ++visualClockRevision; deviceClockValid = false; resetPlugins();
}
void Engine::prepareClipProcessing()
{
    const juce::ScopedLock guard(lock);
    for (auto it = clipProcessors.begin(); it != clipProcessors.end();)
        if (!project.clipFor(it->first) && std::none_of(project.clips.begin(),project.clips.end(),[&](const auto& c) { return effectsIdentity(c.effects)==it->first; })) it=clipProcessors.erase(it); else ++it;
    for (const auto& c : project.clips) { clipProcessors.try_emplace(c.id); clipProcessors.try_emplace(effectsIdentity(c.effects)); }
}
void Engine::resetPlugins()
{
    const juce::ScopedLock guard(lock);
    elasticRenderer.reset();
    masterEqualizer.reset();
    for (auto& eq : equalizers) eq.reset();
    for (auto& filter : cutoffFilters) filter.reset();
    for (auto& entry : clipProcessors) { entry.second.eq.reset(); entry.second.filter.reset(); entry.second.cutoff.reset(); }
    eachPlugin([](auto& p) { p.processor->reset(); });
}
juce::Optional<juce::AudioPlayHead::PositionInfo> Engine::getPosition() const
{
    const juce::ScopedLock guard(lock);
    juce::AudioPlayHead::PositionInfo info;
    const double time = renderingPlugin ? pluginRenderTime.load() : position.load();
    const auto& source = renderContext ? *renderContext : project;
    info.setTimeInSamples((int64_t) std::llround((time - source.tempo.origin) * sampleRate.load()));
    info.setTimeInSeconds(time - source.tempo.origin); info.setBpm(source.followTrackTempo ? source.tempo.bpmAt(time) : source.bpm);
    info.setTimeSignature(juce::AudioPlayHead::TimeSignature {4, 4}); info.setPpqPosition(source.followTrackTempo ? source.tempo.beatsAt(time) : (time - source.tempo.origin) * source.bpm / 60.0);
    info.setIsPlaying(replayTake ? replayRunning : playing.load() || exporting.load()); return info;
}
bool Engine::processTrackEffects(const Project& source, const Clip& clip, int frames, double at, const juce::MidiBuffer& incoming)
{
    const double sr = sampleRate.load();
    const bool audition = at < 0;
    const bool pausedTake=(capturingPerformance() && !playing) || (replayTake && !replayRunning);
    auto boundary = [&](double time) { return (int)juce::jlimit(0., (double)frames, std::ceil((time - at) * sr - 1.e-6)); };
    auto bounds = source.automationBounds({clip.track, AutomationTarget::plugin, {}, {}, clip.id});
    for (const auto& member:source.clips) if (member.effects==clip.effects)
    {
        const auto shifted=replayShifts.find(member.id); const auto* voice=liveVoiceFor(member.id);
        const double shift=voice && voice->status.active ? voice->shift : shifted!=replayShifts.end() ? shifted->second : 0;
        if (shift!=0) { bounds={std::min(bounds.getStart(),member.start-shift),std::max(bounds.getEnd(),member.end()-shift+std::max(source.effectTailSeconds,naturalEffectTail(member.effects)))}; }
    }
    double noteEnd = clip.end();
    for (const auto& member : source.clips) if (member.effects == clip.effects) noteEnd = std::max(noteEnd, member.end());
    const int first = audition || pausedTake ? 0 : boundary(bounds.getStart());
    const int last = audition || pausedTake ? frames : boundary(bounds.getEnd());
    const int audioFirst = first;
    clipMidi.clear();
    if (const auto* selected = source.clipFor(selectedClip); selected && selected->effects == clip.effects)
        clipMidi.addEvents(incoming, audioFirst, audition ? frames : std::max(0, boundary(noteEnd) - audioFirst), 0);
    if (!audition && clip.effects->hasInstrument() && noteEnd >= at && noteEnd < at + frames / sr)
        for (int channel = 1; channel <= 16; ++channel) clipMidi.addEvent(juce::MidiMessage::allNotesOff(channel), boundary(noteEnd));
    for (auto& slot : clip.effects->plugins)
    {
        if (slot->bypass) continue;
        const bool automate = at >= 0 && ((source.followTrackTempo && source.tempo.points.size() > 1)
            || std::any_of(source.effectSettings(clip).pluginAutomation.begin(), source.effectSettings(clip).pluginAutomation.end(), [&](const auto& lane) { return lane.slotId == slot->id && lane.curve.active(); }));
        renderingPlugin = true; clipProcessedMidi.clear();
        for (int offset = first; offset < last;)
        {
            const int64_t sample = (int64_t) std::llround(at * sr) + offset;
            const int count = automate ? std::min(last - offset, 32 - (int) (sample % 32 + 32) % 32) : last - offset;
            const double time = audition ? -1 : (sample - (sample % 32 + 32) % 32) / sr;
            for (const auto& lane : source.effectSettings(clip).pluginAutomation) if (lane.slotId == slot->id)
            {
                auto found = slot->parameters.find(lane.parameterId);
                if (found != slot->parameters.end()) found->second->setValue(automationValueIn(source, {clip.track, AutomationTarget::plugin, lane.slotId, lane.parameterId, clip.id}, time));
            }
            pluginRenderTime = std::max(0.0, pausedTake ? at : at + offset / sr);
            juce::AudioBuffer<float> segment(effectBuffer.getArrayOfWritePointers(), 2, offset, count);
            clipSegmentMidi.clear(); clipSegmentMidi.addEvents(clipMidi, offset, count, -offset);
            if (!processPlugin(*slot, segment, clipSegmentMidi)) { renderingPlugin = false; return false; }
            clipProcessedMidi.addEvents(clipSegmentMidi, 0, count, offset); offset += count;
        }
        clipMidi.swapWith(clipProcessedMidi); renderingPlugin = false;
    }
    auto& shared = clipProcessors[effectsIdentity(clip.effects)];
    for (int offset = first; offset < last;)
    {
        const int64_t sample=(int64_t)std::llround(at*sr)+offset;
        const int count=std::min(last-offset,32-(int)((sample%32+32)%32));
        const double time=audition ? -1 : (sample-(sample%32+32)%32)/sr;
        std::array<float,3> eq;
        for (int band=0;band<3;++band) eq[(size_t)band]=automationValueIn(source,{clip.track,(AutomationTarget)(band+1),{},{},clip.id},time);
        shared.eq.process(effectBuffer,offset,count,sr,eq);
        for (int i=offset;i<offset+count;++i)
        {
            const double tick=audition ? -1 : at+i/sr;
            shared.cutoff.process(effectBuffer,i,1,sr,automationValueIn(source,{clip.track,AutomationTarget::cutoff,{},{},clip.id},tick));
            const float db=automationValueIn(source,{clip.track,AutomationTarget::volume,{},{},clip.id},tick);
            const float gain=db<=-60 ? 0 : juce::Decibels::decibelsToGain(db);
            const float pan=automationValueIn(source,{clip.track,AutomationTarget::pan,{},{},clip.id},tick);
            effectBuffer.getWritePointer(0)[i]*=gain*std::min(1.f,1-pan);
            effectBuffer.getWritePointer(1)[i]*=gain*std::min(1.f,1+pan);
        }
        offset+=count;
    }
    return true;
}
bool Engine::processClipEffects(const Project& source, const Clip& clip, int frames, double at, const juce::MidiBuffer&)
{
    const double sr = sampleRate.load();
    const bool audition = at < 0;
    auto boundary = [&](double time) { return (int) juce::jlimit(0., (double) frames, std::ceil((time - at) * sr - 1.0e-6)); };
    const auto transition = audition ? TransitionPlayback {} : source.transitionPlaybackFor(clip);
    const double noteEnd = std::min(clip.end(), transition.endCut.value_or(clip.end()));
    const double effectEnd = clip.end();
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
        // Shared original-track controls run once after its source pieces are
        // summed. Here only piece-local transition envelopes shape source audio.
        const float manual = 0;
        if (target < AutomationTarget::bass || target > AutomationTarget::treble) return manual;
        const float gain = transition.gainAt((int) target, time);
        return gain == 1 ? manual : automationRange(target).clipValue(manual + juce::Decibels::gainToDecibels(gain, -60.f));
    };
    for (int offset = first; offset < last;)
    {
        const int64_t sample = (int64_t) std::llround(at * sr) + offset;
        int count = std::min(last - offset, 32 - (int) (sample % 32 + 32) % 32);
        const auto gridSample = sample - (sample % 32 + 32) % 32;
        // An EQ swap changes at its exact sample, even inside a coefficient block.
        for (const auto& pair : transition.pairs) for (int band = 1; band < 4; ++band)
            if (pair[(size_t) band].instant)
            {
                const auto edge = (int64_t) std::ceil(pair[(size_t) band].begin * sr - 1e-9);
                if (edge > sample && edge < sample + count) count = (int) (edge - sample);
            }
        // Ordinary hard-cut nodes have the same sample boundary as historical
        // preset swaps. Only this clip's EQ segment splits; plugin/other voice
        // blocks retain their existing ownership and block sizes.
        std::array<int64_t,3> manualSamples {gridSample,gridSample,gridSample};
        if (!audition) for (int band = 0; band < 3; ++band)
        {
            const auto& curve = source.effectSettings(clip).automation[(size_t) band + 1]; if (!curve.active()) continue;
            const AutomationAddress address {clip.track,(AutomationTarget) (band+1),{},{},clip.id};
            const double from = source.automationTime(address,gridSample / sr), until = source.automationTime(address,(sample+count) / sr);
            auto point = std::upper_bound(curve.points.begin(),curve.points.end(),from,[](double t,const auto& node) { return t < node.time; });
            for (;point != curve.points.end() && point->time <= until;++point)
            {
                if (point == curve.points.begin() || std::prev(point)->bend < 1) continue;
                const auto edge = (int64_t) std::ceil(source.automationSessionTime(address,point->time) * sr - 1.e-9);
                if (edge > sample && edge < sample+count) count = (int) (edge-sample);
                if (edge > gridSample && edge <= sample) manualSamples[(size_t) band] = std::max(manualSamples[(size_t) band],edge);
            }
        }
        std::array<float, 3> eq;
        for (int band = 0; band < 3; ++band)
        {
            auto atSample = manualSamples[(size_t) band];
            for (const auto& pair : transition.pairs) if (pair[(size_t) band + 1].instant)
            {
                const auto edge = (int64_t) std::ceil(pair[(size_t) band + 1].begin * sr - 1e-9);
                if (edge > gridSample && edge <= sample) atSample = std::max(atSample, edge);
            }
            eq[(size_t) band] = value((AutomationTarget) (band + 1), atSample / sr);
        }
        clipProcessors[clip.id].eq.process(clipBuffer, offset, count, sr, eq); offset += count;
    }
    clipProcessors[clip.id].filter.process(clipBuffer, first, last, sr, at, transition.filters);
    auto& cutoff = clipProcessors[clip.id].cutoff;
    for (int i = first; i < last; ++i) cutoff.process(clipBuffer, i, 1, sr, value(AutomationTarget::cutoff, audition ? -1 : at + i / sr));
    for (int i = 0; i < frames; ++i)
    {
        const double time = at + i / sr;
        const float db = value(AutomationTarget::volume,time);
        // The ordinary node editor represents zero percent at its -60dB
        // floor. A full Volume cut must mute, as the preset envelope did.
        const float gain = (db <= -60 ? 0.f : juce::Decibels::decibelsToGain(db)) * transition.gainAt(0,time), pan = value(AutomationTarget::pan,time);
        const bool inside = audition || (time >= clip.start && time < effectEnd && transition.audibleAt(time));
        clipBuffer.getWritePointer(0)[i] *= inside ? gain * std::min(1.0f, 1 - pan) : 0;
        clipBuffer.getWritePointer(1)[i] *= inside ? gain * std::min(1.0f, 1 + pan) : 0;
    }
    return true;
}
bool Engine::renderClipAudio(const Project& source, const Clip& clip, int frames, double at, const juce::MidiBuffer& incoming, ElasticRenderer* liveElastic)
{
    clipBuffer.clear(0,frames);
    const bool audition = at < 0 && clip.id == selectedClip && clip.effects->hasInstrument();
    if (clip.start >= at + frames / sampleRate || clip.end() <= at)
        if (!audition) return true;
    if (!clip.prepared && !clip.media->hasPlayback()) return true;
    auto& sourceBuffer = clipBuffer; sourceBuffer.clear(0, frames);
    const bool stems = clip.stems && clip.stemsEnabled;
    const bool solo = stems && std::any_of(clip.stemControls.begin(), clip.stemControls.end(), [](const auto& c) { return c.solo; });
    auto audible = [&](int s) { return !clip.stemControls[(size_t) s].mute && (!solo || clip.stemControls[(size_t) s].solo); };
    if (!audition && clip.elasticPlayback)
    {
        auto& renderer = liveElastic ? *liveElastic : elasticRenderer;
        if (stems) { for (int s = 0; s < stemCount; ++s) if (audible(s)) renderer.add(sourceBuffer, frames, at, sampleRate.load(), clip, s); }
        else renderer.add(sourceBuffer, frames, at, sampleRate.load(), clip);
    }
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
        double sourcePos = clip.prepared && clip.prepared->clipLocal ? (clip.preparedOffset + local - clip.audioSlip) * clip.media->sampleRate
            : clip.sourceTimeAt(local) / clip.rate * clip.media->sampleRate - (clip.reversed ? 1.0 : 0.0);
        if (sourcePos < 0 && sourcePos > -64*std::numeric_limits<double>::epsilon()*std::max(1.,std::abs(local)*clip.media->sampleRate)) sourcePos = 0;
        if (!std::isfinite(sourcePos) || sourcePos < 0 || sourcePos >= sourceFrames) continue;
        const int index = (int) sourcePos;
        const float fraction = (float) (sourcePos - index), gain = envelope.gainAt(local) * clipGain;
        for (int ch = 0; ch < 2; ++ch)
        {
            const int sourceChannel = std::min(ch, sourceChannels - 1);
            float interpolated = 0;
            const int count = stems ? stemCount : 1;
            for (int s = 0; s < count; ++s)
            {
                if (stems && !audible(s)) continue;
                const auto& prepared = stems ? clip.stemPrepared[(size_t) s] : clip.prepared;
                const auto& media = stems ? clip.stems->sources[(size_t) s] : clip.media;
                const int available = prepared ? prepared->frames() : media->frames();
                if (index >= available) continue;
                const float a = prepared ? prepared->channelData(sourceChannel)[index] : media->sample(sourceChannel, index);
                const int next = std::min(index + 1, available - 1);
                const float b = prepared ? prepared->channelData(sourceChannel)[next] : media->sample(sourceChannel, next);
                float level = 1;
                if (stems)
                {
                    const auto& settings = clip.stemControls[(size_t) s]; const double sourceTime = clip.sourceElapsed(local);
                    const float db = settings.valueAt(AutomationTarget::volume, sourceTime), pan = settings.valueAt(AutomationTarget::pan, sourceTime);
                    level = (db <= -60 ? 0.f : juce::Decibels::decibelsToGain(db)) * std::min(1.f, ch == 0 ? 1 - pan : 1 + pan);
                }
                interpolated += (a + fraction * (b - a)) * level;
            }
            sourceBuffer.addSample(ch, i, interpolated * gain);
        }
    }
    }
    if (!processClipEffects(source, clip, frames, at, incoming)) return false;
    return true;
}
void Engine::render(juce::AudioBuffer<float>& output, int frames, double at, juce::MidiBuffer& incoming, const Project* renderSource, bool liveVoices)
{
    liveVoices = liveVoices && renderSource == nullptr && !offlineRenderer;
    const auto& source = renderSource ? *renderSource : project;
    const bool pausedTake=(capturingPerformance() && !playing) || (replayTake && !replayRunning);
    const juce::ScopedValueSetter<const Project*> context(renderContext, &source);
    juce::ScopedNoDenormals noDenormals;
    // Caller owns lock. Sample buffers and decoded media are prepared before rendering.
    output.clear(0, frames);
    latencyBlocked = false;
    if (hasUnsupportedLatency()) { stopForLatency(output, frames); return; }
    elasticRenderer.beginBlock();
    if (liveVoices) for (auto& voice : liveHotcueVoices)
        if (voice.status.active && voice.activeElastic) voice.activeElastic->beginBlock();
    bool anySolo = false; for (const auto& t : source.tracks) anySolo = anySolo || t.solo;
    const auto stemSolo = [](const Clip& clip)
    {
        return clip.pendingPlacement == PendingPlacement::none && clip.stems && clip.stemsEnabled
            && std::any_of(clip.stemControls.begin(), clip.stemControls.end(), [](const auto& controls) { return controls.solo; });
    };
    // Stem Solo isolates sections only within their own lane. Ordinary lane
    // mute/solo remains independent; other lanes keep their audio and effects.
    std::array<bool, trackCount> stemSoloLanes {};
    for (const auto& clip : source.clips) if (stemSolo(clip))
        stemSoloLanes[(size_t) clip.track] = true;
    const auto stemAudible = [&](const Clip& clip) { return !stemSoloLanes[(size_t) clip.track] || stemSolo(clip); };
    for (auto& buffer : trackBuffers) buffer.clear(0, frames);
    for (const auto& group : source.clips)
    {
        if (source.effectOwner(group) != &group) continue;
        if (std::none_of(source.clips.begin(), source.clips.end(), [&](const auto& member)
            { return member.effects == group.effects && member.pendingPlacement == PendingPlacement::none && stemAudible(member); })) continue;
        const bool inserted = true; // Built-in controls and VSTs share one continuous original-track bus.
        effectBuffer.clear(0, frames);
        for (const auto& clip : source.clips)
        {
            if (pausedTake) continue; // Keep processing the shared returns with silent input.
            if (clip.effects != group.effects || clip.pendingPlacement != PendingPlacement::none) continue;
            if (!stemAudible(clip)) continue;
            auto* voice = liveVoices && at >= 0 ? liveVoiceFor(clip.id) : nullptr;
            auto add = [&](int offset, int count, double mapped)
            {
                if (count <= 0) return true;
                const juce::MidiBuffer* events = &incoming;
                if (offset != 0 || count != frames)
                { liveClipMidi.clear(); liveClipMidi.addEvents(incoming,offset,count,-offset); events = &liveClipMidi; }
                if (!renderClipAudio(source,clip,count,mapped,*events,voice && voice->status.active ? voice->activeElastic.get() : nullptr)) return false;
                auto& target = inserted ? effectBuffer : trackBuffers[(size_t)clip.track];
                target.addFrom(0,offset,clipBuffer,0,0,count); target.addFrom(1,offset,clipBuffer,1,0,count);
                return true;
            };
            int edge = frames;
            if (voice && voice->status.state == LiveHotcueState::pending)
            {
                const double remaining = (voice->status.boundary-at) * sampleRate.load();
                // A boundary between samples belongs to the first sample after
                // it, which can be the next callback even with a one-frame block.
                if (remaining < -1.000001)
                { voice->status.state = LiveHotcueState::cancelled; voice->status.reason = ScheduledPauseReason::missedBoundary; }
                else if (remaining < frames)
                    edge = juce::jlimit(0,frames,(int) std::ceil(remaining-1.e-6));
            }
            const auto replayShift=replayShifts.find(clip.id);
            const double shift = voice && voice->status.active ? voice->shift : replayShift!=replayShifts.end() ? replayShift->second : 0.;
            if (!add(0,edge,at+shift)) { stopForLatency(output,frames); return; }
            if (edge < frames && voice)
            {
                const double actual = at + edge / sampleRate.load();
                voice->shift = voice->destination-actual;
                voice->status.state = LiveHotcueState::launched; voice->status.reason = ScheduledPauseReason::none;
                voice->status.actualTime = actual; voice->status.active = true;
                if (capturingPerformance())
                { TakeEvent event; event.kind=TakeEventKind::clipJump; event.owner=clip.id; event.value=voice->shift; event.sourcePosition=actual; event.frame=performanceFrames.load()+edge; appendTakeEvent(std::move(event)); }
                voice->activeElastic.swap(voice->queuedElastic); // Retain old resources until message-thread admission.
                // Keep effect histories, including deliberately shared split
                // owners. No processor or other voice resets at this jump.
                if (!add(edge,frames-edge,voice->destination)) { stopForLatency(output,frames); return; }
            }
        }
        if (inserted)
        {
            const auto* selected = source.clipFor(selectedClip);
            if (at < 0 && (!selected || selected->effects != group.effects || !group.effects->hasInstrument())) continue;
            if (!processTrackEffects(source, group, frames, at, incoming)) { stopForLatency(output, frames); return; }
            // A shared track return follows its latest started piece when pieces
            // move between lanes. The processor advances once, including gaps.
            for (int sample = 0; sample < frames;)
            {
                const double time = at + sample / sampleRate.load();
                const Clip* route = &group;
                double next = at + frames / sampleRate.load();
                for (const auto& member : source.clips) if (member.effects == group.effects)
                {
                    if (member.start <= time && (route->start > time || member.start > route->start)) route = &member;
                    else if (member.start > time) next = std::min(next, member.start);
                }
                const int count = juce::jlimit(1, frames - sample, (int)std::ceil((next - time) * sampleRate.load() - 1.e-6));
                auto& buffer = trackBuffers[(size_t)route->track];
                buffer.addFrom(0, sample, effectBuffer, 0, sample, count);
                buffer.addFrom(1, sample, effectBuffer, 1, sample, count); sample += count;
            }
        }
    }
    for (int track = 0; track < trackCount; ++track)
    {
        auto& buffer = trackBuffers[(size_t)track];
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
        for (int i = 0; i < frames; ++i) cutoffFilters[(size_t) track].process(buffer, i, 1, sr, automationValueIn(source, {track, AutomationTarget::cutoff}, at < 0 ? -1 : at + i / sr));
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
uint64_t Engine::requestLibraryPreview()
{
    const juce::ScopedLock guard(lock);
    setPlaying(false); gridAudition.stop(); endTransitionRehearsal();
    cancelLiveHotcues(ScheduledPauseReason::unavailable,true);
    return libraryPreviewGeneration;
}
bool Engine::startLibraryPreview(juce::AudioSource& source, uint64_t request)
{
    const juce::ScopedLock guard(lock);
    if (request != libraryPreviewGeneration || exporting || playing || gridAudition.active()
        || rehearsalActive() || !devices.getCurrentAudioDevice()) return false;
    source.prepareToPlay(blockSize,sampleRate);
    libraryPreviewSource = &source;
    return true;
}
void Engine::detachLibraryPreview(juce::AudioSource& source)
{
    const juce::ScopedLock guard(lock);
    if (libraryPreviewSource == &source) libraryPreviewSource = nullptr;
}
bool Engine::hasLibraryPreview(juce::AudioSource& source)
{
    const juce::ScopedLock guard(lock);
    return libraryPreviewSource == &source && !exporting;
}
bool Engine::requestPlayback(double at, int frames)
{
    streamBuffering = false;
    bool ready = true, failed = false;
    const auto admit = [&](const Clip& clip, double position)
    {
        ready = requestOriginalClip(clip, position, .25, .35 + frames / sampleRate.load()) && ready;
        failed = failed || (clip.media->original && clip.media->original->failed());
        if (clip.stems && clip.stemsEnabled) for (const auto& stem : clip.stems->sources)
            failed = failed || (stem->original && stem->original->failed());
    };
    for (const auto& clip : project.clips)
    {
        const auto* voice = liveVoiceFor(clip.id); const auto replay = replayShifts.find(clip.id);
        const double shift = voice && voice->status.active ? voice->shift : replay != replayShifts.end() ? replay->second : 0.;
        if (clip.end() <= at + shift || clip.start >= at + shift + frames / sampleRate.load()) continue;
        admit(clip, at + shift);
    }
    // A queued project/cue may become active partway through this callback.
    // Admit its destination before rendering any part of the block.
    for (const auto& clip : project.clips)
        if (const auto* voice = liveVoiceFor(clip.id); voice && voice->status.state == LiveHotcueState::pending
            && voice->status.boundary < at + frames / sampleRate.load())
            admit(clip, voice->destination);
    if (queuedProgramme && !queuedProgrammeCommitted && queuedProgrammeBoundary < at + frames / sampleRate.load())
        for (const auto& clip : queuedProgramme->clips)
        {
            const double shift = clip.id == queuedProgrammeEvent.owner ? queuedProgrammeEvent.value : 0.;
            admit(clip, at + shift);
        }
    if (failed) { streamFailed = true; playing = false; deviceClockValid = false; return false; }
    if (!ready) { streamBuffering = true; ++streamWaitCallbacks; deviceClockValid = false; return false; }
    for (const auto& clip : project.clips)
    {
        const auto* voice = liveVoiceFor(clip.id); const auto replay = replayShifts.find(clip.id);
        const double shift = voice && voice->status.active ? voice->shift : replay != replayShifts.end() ? replay->second : 0.;
        requestOriginalClip(clip, at + shift, .25, 1.);
        if (voice && voice->status.state == LiveHotcueState::pending)
            requestOriginalClip(clip, voice->destination, .25, .5);
    }
    if (queuedProgramme && !queuedProgrammeCommitted)
        for (const auto& clip : queuedProgramme->clips)
            requestOriginalClip(clip, at + (clip.id == queuedProgrammeEvent.owner ? queuedProgrammeEvent.value : 0.), .25, 1.);
    return true;
}
void Engine::audioDeviceIOCallbackWithContext(const float* const*, int, float* const* outputs, int outputChannels, int frames, const juce::AudioIODeviceCallbackContext&)
{
    OriginalAudio::Realtime realtimeReads;
    struct Measure
    {
        AudioCallbackDiagnostics& diagnostics;
        juce::int64 start = juce::Time::getHighResolutionTicks();
        double budget;
        ~Measure() { diagnostics.record((uint64_t) (juce::Time::getHighResolutionTicks() - start), budget); }
    } measure {callbackDiagnostics, juce::Time::getHighResolutionTicks(),
        (double) frames / sampleRate.load() * (double) juce::Time::getHighResolutionTicksPerSecond()};
    for (int ch = 0; ch < outputChannels; ++ch) if (outputs[ch]) juce::FloatVectorOperations::clear(outputs[ch], frames);
    if (exporting) { liveExclusiveInterrupted = true; pauseExclusiveInterrupted = true; callbackDiagnostics.exportSilence.fetch_add(1, std::memory_order_relaxed); return; }
    const juce::ScopedTryLock guard(lock);
    if (!guard.isLocked()) { callbackDiagnostics.lockMisses.fetch_add(1, std::memory_order_relaxed); if (performanceActive) { performanceFrames.fetch_add(frames); performanceCallbackMissed=true; } return; }
    if (frames > mixBuffer.getNumSamples()) { callbackDiagnostics.oversized.fetch_add(1, std::memory_order_relaxed); return; } // Device must announce size changes before rendering.
    auto& midi = callbackMidi; midi.clear(); midiCollector.removeNextBlockOfMessages(midi, frames); keyboard.processNextMidiBuffer(midi, 0, frames, true);
    const bool running = playing.load(); const double at = position.load();
    serviceLiveHotcues();
    if (running && !gridAudition.active() && !rehearsalActive() && !takeAudition && !requestPlayback(at, frames)) return;
    if (rehearsalActive() || gridAudition.active()) { cancelScheduledPause(ScheduledPauseReason::unavailable); cancelLiveHotcues(ScheduledPauseReason::unavailable,true); }
    const auto deliverMonitor = [&]
    {
        outputPeaks.push(mixBuffer, frames);
        spectrum.push(mixBuffer, frames, sampleRate.load());
        visualSpectrum.push(mixBuffer, frames, sampleRate.load());
        if (!monitorMuted.load()) for (int ch = 0; ch < std::min(2, outputChannels); ++ch)
            if (outputs[ch]) juce::FloatVectorOperations::copy(outputs[ch], mixBuffer.getReadPointer(ch), frames);
        if (privatePreviewSource && privatePreviewFirstChannel>=2 && privatePreviewFirstChannel+1<outputChannels)
        {
            privatePreviewBuffer.clear(0,frames); privatePreviewSource->getNextAudioBlock({&privatePreviewBuffer,0,frames});
            for (int ch=0;ch<2;++ch) if (outputs[privatePreviewFirstChannel+ch])
                juce::FloatVectorOperations::copyWithMultiply(outputs[privatePreviewFirstChannel+ch],privatePreviewBuffer.getReadPointer(ch),privatePreviewGain.load(),frames);
        }
    };
    if (takeAudition)
    {
        takeAudition->renderTake(mixBuffer,frames,takeAudition->replayFrame/takeAudition->sampleRate.load());
        deliverMonitor(); return;
    }
    if (running || rehearsalActive() || gridAudition.active()) libraryPreviewSource = nullptr;
    if (libraryPreviewSource)
    {
        mixBuffer.clear(0,frames);
        libraryPreviewSource->getNextAudioBlock({&mixBuffer,0,frames});
        mixBuffer.applyGain(0,frames,juce::Decibels::decibelsToGain(project.masterDb));
        masterPeak = mixBuffer.getMagnitude(0,frames); masterInputPeak = -1;
        if (masterPeak > 1) masterClipped = true;
        for (auto& meter : meters) meter = 0;
        deliverMonitor();
        return;
    }
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
    double audibleEnd = effectAudibleEnd(*this);
    for (const auto& clip:project.clips)
    {
        const auto shifted=replayShifts.find(clip.id); const auto* voice=liveVoiceFor(clip.id);
        const double shift=voice && voice->status.active ? voice->shift : shifted!=replayShifts.end() ? shifted->second : 0;
        audibleEnd=std::max(audibleEnd,clip.end()-shift+std::max(project.effectTailSeconds,naturalEffectTail(clip.effects)));
        if (voice && voice->status.state==LiveHotcueState::pending)
            audibleEnd=std::max(audibleEnd,clip.end()-(voice->destination-voice->status.boundary)+std::max(project.effectTailSeconds,naturalEffectTail(clip.effects)));
    }
    const int pauseFrames = framesBeforeScheduledPause(frames, at);
    const int transportFrames = running && pauseFrames >= 0 ? pauseFrames : frames;
    const int activeFrames = running && audibleEnd > 0
        ? (int) juce::jlimit(0., (double) transportFrames, std::ceil((audibleEnd - at) * sampleRate.load() - 1.0e-6)) : frames;
    int split=activeFrames;
    if (running && queuedProgramme && !queuedProgrammeCommitted)
        split=juce::jlimit(0,activeFrames,(int)std::ceil((queuedProgrammeBoundary-at)*sampleRate.load()-1.e-6));
    if (split>0) render(mixBuffer,split,running || capturingPerformance() ? at : -frames/sampleRate.load()-1,midi,nullptr,true);
    if (split<activeFrames && queuedProgramme && !queuedProgrammeCommitted)
    {
        // Retain the previous project in the queue until the message thread can
        // reclaim it. All maps/buffers/plugin instances were prepared beforehand.
        project.performanceTakes.swap(queuedProgramme->performanceTakes);
        std::swap(project,*queuedProgramme); queuedProgrammeCommitted=true;
        queuedProgrammeEvent.frame=performanceFrames.load()+split; queuedProgrammeEvent.sourcePosition=at+split/sampleRate.load(); appendTakeEvent(queuedProgrammeEvent);
        if (queuedProgrammeEvent.owner.isNotEmpty())
        {
            replayShifts.at(queuedProgrammeEvent.owner)=queuedProgrammeEvent.value;
            if (auto* voice=liveVoiceFor(queuedProgrammeEvent.owner)) voice->status.active=false;
            auto cue=queuedProgrammeEvent; cue.kind=TakeEventKind::clipJump; cue.payload=juce::var(); appendTakeEvent(std::move(cue));
        }
        render(takeBuffer,activeFrames-split,at+split/sampleRate.load(),midi,nullptr,true);
        for (int ch=0;ch<2;++ch) mixBuffer.copyFrom(ch,split,takeBuffer,ch,0,activeFrames-split);
    }
    if (activeFrames < frames) mixBuffer.clear(activeFrames, frames - activeFrames);
    if (capturingPerformance())
    {
        const auto* selected=project.clipFor(selectedClip);
        const bool instrument=(selected && selected->effects->hasInstrument()) || std::any_of(plugins.begin(),plugins.end(),[](const auto& chain) { return pluginChainHasInstrument(chain); });
        if (instrument) for (const auto metadata:midi) if (metadata.getMessage().isNoteOnOrOff() || metadata.getMessage().isPitchWheel() || metadata.getMessage().isProgramChange())
        { auto& take=project.performanceTakes[(size_t)activeTake]; take.complete=false; take.issue=liveInputRequiresWav; break; }
        if (pauseFrames>=0)
        { TakeEvent pause; pause.kind=TakeEventKind::transport; pause.frame=performanceFrames.load()+pauseFrames; pause.sourcePosition=at+pauseFrames/sampleRate.load(); pause.value=0; appendTakeEvent(std::move(pause)); }
        if (masterRecorder.active()) masterRecorder.push(mixBuffer,frames);
        performanceFrames.fetch_add(frames);
    }
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
        deviceClockFrames += transportFrames;
        const double next = deviceClockAnchor + (double) deviceClockFrames / rate;
        deviceClockExpected = audibleEnd > 0 ? std::min(next, audibleEnd) : next;
        position = deviceClockExpected;
        if (pauseFrames >= 0) commitScheduledPause(deviceClockExpected);
        if (pauseStatus.state == ScheduledPauseState::pending) pauseExpected = deviceClockExpected;
        for (auto& voice : liveHotcueVoices) voice.expected = deviceClockExpected;
        if (next >= audibleEnd && audibleEnd > 0) { playing = false; if (capturingPerformance()) performanceEnded=true; }
    }
    if (latencyBlocked) { cancelScheduledPause(ScheduledPauseReason::unavailable); cancelLiveHotcues(ScheduledPauseReason::unavailable,true); }
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
    if (chain.size() == 1) defaultPluginControls(controlEffectSettings(track), chain);
    return {};
}
juce::String Engine::pluginControlSummary(int track)
{
    const juce::ScopedLock guard(lock);
    if (track < 0 || track >= trackCount) return {};
    juce::String result;
    for (size_t i = 0; i < controlEffectSettings(track).pluginControls.size(); ++i)
    {
        const auto& control = controlEffectSettings(track).pluginControls[i];
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
    auto& target = controlEffectSettings(track).pluginControls[(size_t) control];
    if (parameter == -1) { target = {}; return {}; }
    const auto& chain = pluginChain(track);
    if (insert < 0 || insert >= (int) chain.size()) return "The selected insert is unavailable.";
    const auto& parameters = chain[(size_t) insert]->processor->getParameters();
    if (parameter < 0 || parameter >= parameters.size()) return "The selected parameter is unavailable.";
    auto id = parameterIdentity(parameters[parameter]);
    if (id.isEmpty() || !parameters[parameter]->isAutomatable()) return "This parameter cannot be assigned.";
    target = { insert, id }; return {};
}
juce::var Engine::captureProject(bool programmeSnapshot)
{
    const juce::ScopedLock captureGuard(lock);
    if (capturingPerformance() && !programmeSnapshot)
    {
        auto document=project.performanceTakes[(size_t)activeTake].initial.clone(); juce::Array<juce::var> takes;
        project.performanceTakes[(size_t)activeTake].frames=performanceFrames.load();
        for (const auto& take:project.performanceTakes) takes.add(takeToJson(take)); document.getDynamicObject()->setProperty("performanceTakes",takes); return document;
    }
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
    for (int i = 0; i < (int) project.clips.size(); ++i)
        if (project.effectOwner(project.clips[(size_t)i]) == &project.clips[(size_t)i])
            data["clips"].getArray()->getReference(i).getDynamicObject()->setProperty("plugins", capture(project.clips[(size_t) i].effects->plugins));
    juce::Array<juce::var> bindings; std::set<juce::String> sources;
    for (const auto& clip:project.clips) if (sources.insert(clip.media->file.getFullPathName()).second)
    {
        juce::var item(new juce::DynamicObject()); auto* source=item.getDynamicObject(); source->setProperty("path",clip.media->file.getFullPathName()); source->setProperty("sha256",clip.media->analysisSource.sha256);
        source->setProperty("frames",clip.media->frames()); source->setProperty("sampleRate",clip.media->sampleRate); bindings.add(item);
    }
    data.getDynamicObject()->setProperty("sourceBindings",bindings);
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
    const int total = (int) usedMedia.size(); int index = 0;
    for (const auto& reference : result.parsed.media)
    {
        if (cancelled && *cancelled) { result.error = "Session opening cancelled. The current session was kept."; return result; }
        if (!usedMedia.count(reference.id)) continue; // Catalogue availability never gates arrangement admission.
        auto loaded = readMedia(mediaFormats, juce::File(reference.path), [&](MediaReadStage phase)
        {
            if (progress) progress(index, total, juce::File(reference.path).getFileName(), phase);
        }, AnalysisCacheMode::readWrite, MediaReadPurpose::playback, nullptr, cancelled);
        if (!loaded.media)
        {
            if (!cancelled || !*cancelled) result.failedSource = reference;
            result.error = "Required source: " + reference.name + "\n" + reference.path + "\n" + loaded.error
                + "\nThe current session was kept. Locate this source or restore it at the saved path, then retry opening.";
            return result;
        }
        if (result.parsed.document.hasProperty("sourceBindings"))
        {
            const auto* bindings=result.parsed.document["sourceBindings"].getArray();
            if (!bindings) { result.error="Invalid saved source identities."; return result; }
            for (const auto& binding:*bindings) if (binding["path"].toString()==reference.path)
            {
                if (!binding["sha256"].isString() || !binding["frames"].isInt() || (!binding["sampleRate"].isDouble() && !binding["sampleRate"].isInt())) { result.error="Invalid saved source identity."; return result; }
                const auto expected=binding["sha256"].toString();
                if (expected.isNotEmpty() && (expected!=loaded.media->analysisSource.sha256 || (int)binding["frames"]!=loaded.media->frames() || (double)binding["sampleRate"]!=loaded.media->sampleRate))
                { result.error="That file differs from the saved audio. Locate the original file and open the mix again.\nA source changed since this arrangement/take was saved: "+reference.path+". The saved performance and current session were kept."; return result; }
            }
        }
        loaded.media->id = reference.id; loaded.media->name = reference.name; loaded.media->manualBpm = reference.bpm;
        loaded.media->manualBeatOrigin = reference.beatOrigin; loaded.media->manualKey = reference.key;
        if (reference.beatOrigin > loaded.media->duration()) { result.error = "The beat marker exceeds source audio. The current session was kept."; return result; }
        byId[reference.id] = loaded.media;
        if (progress) progress(++index, total, loaded.media->name, MediaReadStage::cache);
    }
    // Retain the complete catalogue and its saved order/metadata without touching unused files.
    for (const auto& reference : result.parsed.media)
    {
        auto found = byId.find(reference.id);
        if (found != byId.end()) result.library.push_back(found->second);
        else
        {
            auto media = std::make_shared<Media>(); media->id = reference.id; media->file = juce::File(reference.path);
            media->name = reference.name; media->manualBpm = reference.bpm; media->manualBeatOrigin = reference.beatOrigin; media->manualKey = reference.key;
            media->sourceDetailsPending = media->sessionMetadataPending = true; result.library.push_back(std::move(media));
        }
    }
    for (const auto& reference : result.parsed.clips)
    {
        Clip clip = reference.clip; clip.media = byId.at(reference.mediaId);
        if (clip.stemDirectory != juce::File())
        {
            const auto previous = std::find_if(result.parsed.project.clips.begin(), result.parsed.project.clips.end(), [&](const auto& c)
            { return c.media == clip.media && c.stemDirectory == clip.stemDirectory && c.stemManifestHash == clip.stemManifestHash; });
            if (previous != result.parsed.project.clips.end()) clip.stems = previous->stems;
            else { const auto loaded = loadStemAssets(*clip.media, clip.stemDirectory, clip.stemManifestHash, clip.stems);
                if (loaded.failed()) { result.error = loaded.getErrorMessage(); return result; } }
        }
        if (clip.loopSpan > 0 && clip.loopSpan < 1.0 / clip.media->sampleRate) { result.error = "A loop must contain at least one source sample. The current session was kept."; return result; }
        if (clip.offset + clip.sourceSpan() > clip.media->duration() + 1.0 / clip.media->sampleRate) { result.error = "Segment exceeds source audio. The current session was kept."; return result; }
        result.parsed.project.clips.push_back(clip);
    }
    if (cancelled && *cancelled) { result.error = "Session opening cancelled. The current session was kept."; return result; }
    if (progress) progress(total, total, "Preparing mix playback", MediaReadStage::tempo);
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
    serviceScheduledPause(); cancelScheduledPause(ScheduledPauseReason::transportChanged); cancelLiveHotcues(ScheduledPauseReason::transportChanged,true);
    liveHotcueVoices.clear(); // Document commit is message-thread-owned; no runtime state crosses Open.
    endTakeAudition();
    finishAutomation(); automationRecordMode = AutomationRecordMode::read; automationArmed.fill(false); masterAutomationArmed = false;
    if (workMode==WorkMode::recordPerform) { automationRecordMode=AutomationRecordMode::write; automationArmed.fill(true); masterAutomationArmed=true; }
    endTransitionRehearsal();
    playing = false; position = 0; deviceClockValid = false; keyboard.allNotesOff(0);
    masterClipped = false; masterPeak = masterInputPeak = 0;
    project = std::move(result.project); ++visualClockRevision; position = project.playbackStart(); tempo = project.bpm; library = std::move(prepared.library); plugins = std::move(stagedPlugins); gainSnapResetPending = true;
    masterEqualizer.reset();
    for (auto& eq : equalizers) eq.reset(); for (auto& filter : cutoffFilters) filter.reset(); prepareClipProcessing();
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
    const bool realtimeStart = message.isMidiStart() || message.isMidiContinue(), realtimeStop = message.isMidiStop();
    if ((realtimeStart || realtimeStop) && learnAction)
    {
        const bool matches = learnAction && (realtimeStart
            ? learnAction->kind == MidiActionKind::play || learnAction->kind == MidiActionKind::start
            : learnAction->kind == MidiActionKind::stop);
        if (matches)
        {
            learnAction.reset();
            if (!learnSteps.empty() && ++learnStep < learnSteps.size()) learnAction = learnSteps[learnStep].action;
            else if (midiLearnMode != MidiLearnMode::surface) midiLearnMode = MidiLearnMode::off;
            midiLearnFeedback = "This button uses standard MIDI " + juce::String(realtimeStart ? "Start/Continue" : "Stop")
                + ". Transport already supports it; no learned assignment is needed.";
        }
        else midiLearnFeedback = "Received standard MIDI transport. Playback is held while mapping; move the named control or cancel Learn.";
        triggerAsyncUpdate(); return;
    }
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
            if (!learnSteps.empty() && learnedControls.count(physical) != 0)
            { midiLearnFeedback = "That control already filled a setup step. Move the next named control, or Skip."; triggerAsyncUpdate(); return; }
            if (!learnSteps.empty() && !replaceControllerProfile)
                for (const auto& b : mappings)
                    if (b.status == status && b.channel == channel && b.number == number)
                    { midiLearnFeedback = "Already assigned to " + midiActionId(b.action) + ". Move an unused control, or use individual Learn to replace it."; triggerAsyncUpdate(); return; }
            const auto action = *learnAction;
            mappings.erase(std::remove_if(mappings.begin(), mappings.end(), [&](const MidiBinding& b) { return b.action == action || (b.status == status && b.channel == channel && b.number == number); }), mappings.end());
            mappings.push_back({ status, channel, number, action }); learnAction.reset(); consumed = true;
            midiLearnFeedback.clear();
            if (midiLearnMode == MidiLearnMode::single) midiLearnMode = MidiLearnMode::off;
            if (!learnSteps.empty()) learnedControls.insert(physical);
            if (!continuousMidiAction(action)) pressedMidiButtons.insert(status * 65536 + channel * 256 + number);
            mappingsDirty = true;
            if (!learnSteps.empty())
            {
                ++learnStep;
                if (learnStep < learnSteps.size()) learnAction = learnSteps[learnStep].action;
                else midiLearnMode = MidiLearnMode::off;
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
juce::String Engine::writeMidiProfileFile(const juce::File& file, const juce::String& text)
{
    if (file.isSymbolicLink() || file.getParentDirectory().isSymbolicLink()) return "Cannot save a MIDI profile through a symbolic link.";
    return writeWithBackup(file, text);
}
void Engine::saveMidiMappings()
{
    const juce::ScopedLock guard(midiLock);
    midiProfileError = writeMidiProfileFile(mappingsFile, juce::JSON::toString(midiProfileDocument(midiProfileTitle, midiNamedProfile), false));
}
juce::Result Engine::clearMidiMappings()
{
    const juce::ScopedLock guard(midiLock);
    if (exporting || automationTake) return juce::Result::fail("Finish export or Record before clearing mappings.");
    const auto error = writeWithBackup(mappingsFile, "[]\n");
    if (error.isNotEmpty()) { midiProfileError = error; return juce::Result::fail(error); }
    mappings.clear(); midiProfileTitle.clear(); midiNamedProfile.clear(); midiProfileError.clear(); mappingsDirty = false;
    pendingControls.clear(); pressedMidiButtons.clear(); learnAction.reset(); learnSteps.clear(); learnStep = 0; gainSnapResetPending = true;
    midiLearnMode = MidiLearnMode::off; midiLearnFeedback.clear();
    learnedControls.clear(); replaceControllerProfile = false;
    midiProfileReadEvidence = "Cleared active and saved assignments; previous file backed up beside the profile.";
    return juce::Result::ok();
}
juce::String Engine::activeMappingTitle()
{
    const juce::ScopedLock guard(midiLock);
    return midiProfileTitle.isNotEmpty() ? midiProfileTitle : mappings.empty() ? "Default / no mappings" : "Unnamed mapping";
}
juce::String Engine::midiSummary() const { return juce::String(midiNotes.load()) + " notes received"; }
void Engine::beginLearn(const juce::String& action, bool keepEnabled)
{
    const juce::ScopedLock guard(midiLock); learnSteps.clear(); learnStep = 0; learnedControls.clear(); replaceControllerProfile = false;
    learnAction = parseMidiAction(action); midiLearnFeedback.clear();
    midiLearnMode = !learnAction ? MidiLearnMode::off : keepEnabled ? MidiLearnMode::surface : MidiLearnMode::single;
}
void Engine::setMidiLearnEnabled(bool enabled)
{
    const juce::ScopedLock guard(midiLock); beginLearn({});
    midiLearnMode = enabled ? MidiLearnMode::surface : MidiLearnMode::off;
}
bool Engine::isMidiLearnEnabled() { const juce::ScopedLock guard(midiLock); return midiLearnMode != MidiLearnMode::off; }
bool Engine::canSkipControllerStep() { const juce::ScopedLock guard(midiLock); return midiLearnMode == MidiLearnMode::guided && learnStep < learnSteps.size(); }
void Engine::beginControllerSetup(bool replaceExisting)
{
    const juce::ScopedLock guard(midiLock); learnSteps.clear(); learnStep = 0; replaceControllerProfile = replaceExisting; learnedControls.clear(); midiLearnFeedback.clear();
    auto add = [&](MidiAction a, const juce::String& control)
    {
        if (!replaceExisting) for (const auto& binding : mappings) if (binding.action == a) return;
        learnSteps.push_back({a, control});
    };
    // Both setup entry points teach the current paired live desk. Resume skips
    // mapped actions; the desk's explicit full walkthrough permits replacement.
    for (int i = 0; i < 8; ++i) add({MidiActionKind::deskPot, i}, "Pot " + juce::String(i + 1));
    for (int i = 0; i < 8; ++i) add({i < 4 ? MidiActionKind::volume : MidiActionKind::pan, i % 4}, "Fader " + juce::String(i + 1));
    add({MidiActionKind::master}, "Fader 9 (master)");
    for (int i = 0; i < 8; ++i) add({MidiActionKind::cyclePot, i}, "Top pad " + juce::String(i + 1));
    for (int i = 0; i < 8; ++i) add({i < 4 ? MidiActionKind::mute : MidiActionKind::solo, i % 4}, "Bottom pad " + juce::String(i + 1));
    for (int i = 0; i < 8; ++i) add({i < 4 ? MidiActionKind::resetEq : MidiActionKind::resetCutoff, i % 4}, "Fader button " + juce::String(i + 1));
    add({MidiActionKind::resetPans}, "Master button");
    // FLkey Play/Stop send standard realtime transport, already handled without a binding.
    for (auto kind : {MidiActionKind::undo, MidiActionKind::redo, MidiActionKind::snap, MidiActionKind::record}) add({kind}, midiActionId({kind}));
    learnAction = learnSteps.empty() ? juce::Optional<MidiAction>() : juce::Optional<MidiAction>(learnSteps[0].action);
    midiLearnMode = learnAction ? MidiLearnMode::guided : MidiLearnMode::off;
}
void Engine::skipControllerStep()
{
    const juce::ScopedLock guard(midiLock);
    if (midiLearnMode != MidiLearnMode::guided) return;
    if (learnStep < learnSteps.size()) ++learnStep;
    learnAction = learnStep < learnSteps.size() ? juce::Optional<MidiAction>(learnSteps[learnStep].action) : juce::Optional<MidiAction>();
    midiLearnFeedback.clear(); if (!learnAction) midiLearnMode = MidiLearnMode::off;
}
juce::String Engine::lastMidiText() { const juce::ScopedLock guard(midiLock); return midiLastMessage; }
juce::String Engine::mappingSummary()
{
    const juce::ScopedLock guard(midiLock);
    juce::String text = learnAction.hasValue() ? "Waiting for a knob, fader, pad or button for: " + midiActionId(*learnAction) + "\n\n" : "Select an action, click Learn, then move a control. Completed mappings autosave to the startup snapshot; named copies need Save.\n\n";
    if (midiLearnMode == MidiLearnMode::surface && !learnAction) text = "Click an on-screen live desk control, then move its FLkey control.\n\n";
    if (!learnSteps.empty()) text = learnStep < learnSteps.size() ? "FLkey setup " + juce::String((int) learnStep + 1) + "/" + juce::String((int) learnSteps.size()) + ": move " + learnSteps[learnStep].control + " -> " + midiActionId(learnSteps[learnStep].action) + "\nUse Custom Mode. Only move the named control; repeated events are ignored.\n\n" : "FLkey setup complete. Assignments autosave to the startup snapshot; use Save to update a named copy.\n\n";
    text += "Mapping: " + activeMappingTitle() + "\nActive mappings: " + juce::String((int) mappings.size()) + "\nProfile: " + mappingsFile.getFullPathName() + "\n";
    text += "Last MIDI: " + (midiLastMessage.isEmpty() ? juce::String("No control or note received this run") : midiLastMessage) + "\n";
    text += midiProfilePersistenceState() + "\n";
    if (midiProfileReadEvidence.isNotEmpty()) text += midiProfileReadEvidence + "\n";
    if (midiProfileError.isNotEmpty()) text += "Profile error: " + midiProfileError + "\n";
    if (midiLearnFeedback.isNotEmpty()) text += midiLearnFeedback + "\n";
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
bool Engine::applyGainControl(const ControlEvent& event, AutomationAddress address, float db)
{
    if (!std::isfinite(db) || !std::isfinite(event.inputMs)) return false;
    if (gainSnapResetPending.exchange(false)) gainSnapGestures.assign(trackCount * 4 + 1, {});
    if (project.automationLocked(address)) return false;
    const auto index = address.target == AutomationTarget::master ? 0 : 1 + address.track * 4 + (int) address.target;
    if (index < 0 || index >= (int) gainSnapGestures.size()) return false;
    auto gesture = gainSnapGestures[(size_t) index];
    // Non-touch MIDI faders have no release. Group by the arrival clock, not
    // playback time or async delivery, so stopped/queued input behaves equally.
    if (!(gesture.address == address) || gesture.lastInputMs < 0 || event.inputMs - gesture.lastInputMs >= 350 || event.inputMs < gesture.lastInputMs)
        gesture.canExceedUnity = automationValue(address, event.time) >= -1.e-5f;
    gesture.address = address; gesture.lastInputMs = event.inputMs;
    if (db < 0) gesture.canExceedUnity = false;
    db = std::min(db, gesture.canExceedUnity ? juce::Decibels::gainToDecibels(1.3f) : 0.f);
    if (!setControlValueAt(address, db, event.time, event.running)) return false;
    gainSnapGestures[(size_t) index] = gesture;
    return true;
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
                if (workMode==WorkMode::recordPerform) { if (capturingPerformance()) stop(); else setPlaying(true); return false; }
                const bool arm = automationRecordMode == AutomationRecordMode::read;
                if (arm && hasLiveHotcueVoices()) return false;
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
        case MidiActionKind::home: if (value > 0) seek(project.playbackStart()); return false;
        case MidiActionKind::master: return applyGainControl(event, {-1, AutomationTarget::master}, -60 + value * 66);
        case MidiActionKind::pluginParameter:
        {
            const int track = event.targetTrack >= 0 ? event.targetTrack : selectedTrack.load();
            if (track < 0 || track >= trackCount || action.index < 0 || action.index >= 8) return false;
            const auto& control = controlEffectSettings(track).pluginControls[(size_t) action.index];
            if (control.insert < 0 || control.insert >= (int) pluginChain(track).size() || control.parameterId.isEmpty()) return false;
            return setControlValueAt({track, AutomationTarget::plugin, pluginChain(track)[(size_t) control.insert]->id, control.parameterId, controlClip(track) ? selectedClip : juce::String()}, value, event.time, event.running);
        }
        default: break;
    }
    if (action.index < 0 || action.index >= trackCount) return false;
    auto& track = project.tracks[(size_t) action.index];
    switch (action.kind)
    {
        case MidiActionKind::cutoff: return setControlValueAt({action.index, AutomationTarget::cutoff}, cutoffFromMidi(value), event.time, event.running);
        case MidiActionKind::resetCutoff: return value > 0 && setControlValueAt({action.index, AutomationTarget::cutoff}, 0, event.time, event.running);
        case MidiActionKind::volume: return applyGainControl(event, {action.index, AutomationTarget::volume}, -60 + value * 66);
        case MidiActionKind::pan: return setControlValueAt({action.index, AutomationTarget::pan}, value * 2 - 1, event.time, event.running);
        case MidiActionKind::bass: case MidiActionKind::mid: case MidiActionKind::treble:
            return applyGainControl(event, {action.index, action.kind == MidiActionKind::bass ? AutomationTarget::bass : action.kind == MidiActionKind::mid ? AutomationTarget::mid : AutomationTarget::treble}, -60 + value * 72);
        case MidiActionKind::mute: case MidiActionKind::solo:
            if (value > 0)
            {
                if (onProjectSwitchBegin) onProjectSwitchBegin();
                auto& enabled = action.kind == MidiActionKind::mute ? track.mute : track.solo; enabled = !enabled;
                if (capturingPerformance()) { TakeEvent recorded; recorded.kind=action.kind==MidiActionKind::mute ? TakeEventKind::mute : TakeEventKind::solo; recorded.lane=action.index; recorded.value=enabled ? 1 : 0; recorded.sourcePosition=position.load(); appendTakeEvent(std::move(recorded)); }
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
    const AutomationTarget targets[] {AutomationTarget::bass, AutomationTarget::mid, AutomationTarget::treble, AutomationTarget::cutoff};
    jassert(slot >= 0 && slot < 8);
    return {deskLanes[(size_t) slot], targets[slot / 2]};
}
MidiAction Engine::deskPotAction(int slot) const
{
    const auto address = deskPotAddress(slot);
    const MidiActionKind kinds[] {MidiActionKind::bass, MidiActionKind::mid, MidiActionKind::treble, MidiActionKind::cutoff};
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
