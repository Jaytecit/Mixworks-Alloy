#include "Workstation.h"
#include "VerificationMetrics.h"
#include <juce_cryptography/juce_cryptography.h>
#include <cmath>
#include <limits>

namespace mixworks
{
namespace
{
juce::String argument(const juce::StringArray& args, const juce::String& name)
{ const int at = args.indexOf(name); return at >= 0 && at + 1 < args.size() ? args[at + 1].unquoted() : juce::String(); }
int minutesArgument(const juce::StringArray& args)
{ const auto value = argument(args, "--minutes"); return value == "60" ? 60 : value == "90" ? 90 : 0; }
juce::var object() { return juce::var(new juce::DynamicObject()); }
void property(juce::var& value, const juce::Identifier& key, const juce::var& entry) { value.getDynamicObject()->setProperty(key, entry); }
void writeMetrics(const juce::File& file, const juce::var& data) { file.replaceWithText(juce::JSON::toString(data, true)); }
juce::String modelJson(const Engine& engine) { return juce::JSON::toString(projectToJson(engine.project, engine.library)); }

juce::Result renderDeviceFile(Engine& engine, const juce::File& file, const WavExportPlan& plan, juce::var& metrics, const juce::File& progress)
{
    if (file.exists()) return juce::Result::fail("Keep the existing device-render evidence; choose a fresh output directory.");
    juce::WavAudioFormat format; std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    auto writer = format.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(plan.sampleRate).withNumChannels(2).withBitsPerSample(24));
    if (!writer) return juce::Result::fail("Cannot create the owned device-render WAV.");
    float left[512] {}, right[512] {}; float* outputs[] {left, right}; juce::AudioBuffer<float> block(outputs, 2, 512);
    engine.monitorMuted = false; engine.spectrum.enabled = false; engine.seek(0); engine.setPlaying(true);
    const auto began = juce::Time::getMillisecondCounterHiRes(); double maxClockError = 0, maxBlock = 0; float peak = 0;
    juce::int64 nextProgress = 0;
    for (juce::int64 offset = 0; offset < plan.frames; offset += 512)
    {
        if (!engine.playing) return juce::Result::fail("Device rendering stopped before the planned end: " + engine.pluginLatencyError());
        const auto started = juce::Time::getMillisecondCounterHiRes();
        engine.audioDeviceIOCallbackWithContext(nullptr, 0, outputs, 2, 512, {});
        maxBlock = std::max(maxBlock, juce::Time::getMillisecondCounterHiRes() - started);
        const int count = (int) std::min<juce::int64>(512, plan.frames - offset);
        for (int channel = 0; channel < 2; ++channel) for (int i = 0; i < count; ++i)
            if (!std::isfinite(block.getSample(channel, i))) return juce::Result::fail("Non-finite device-render PCM.");
        peak = std::max(peak, block.getMagnitude(0, count));
        if (!writer->writeFromAudioSampleBuffer(block, 0, count)) return juce::Result::fail("Cannot write the owned device-render WAV.");
        const double expected = (double) std::min<juce::int64>(offset + 512, plan.frames);
        maxClockError = std::max(maxClockError, std::abs(engine.position.load() * plan.sampleRate - expected));
        if (offset >= nextProgress)
        {
            auto state = object(); property(state, "stage", "continuous numerical device callback"); property(state, "frames", offset);
            property(state, "totalFrames", plan.frames); property(state, "elapsedMilliseconds", juce::Time::getMillisecondCounterHiRes() - began);
            writeMetrics(progress, state); nextProgress += (juce::int64) (plan.sampleRate * 30);
        }
    }
    if (!writer->flush()) return juce::Result::fail("Cannot finish device-render WAV."); writer.reset(); engine.setPlaying(false);
    property(metrics, "milliseconds", juce::Time::getMillisecondCounterHiRes() - began); property(metrics, "maximumCallbackMilliseconds", maxBlock);
    property(metrics, "maximumClockErrorSamples", maxClockError); property(metrics, "peak", peak);
    property(metrics, "scope", "Manually driven production audio callback; no physical audio driver or real-time underrun claim.");
    return juce::Result::ok();
}
juce::Result compareFiles(juce::AudioFormatManager& formats, const juce::File& first, const juce::File& second, const WavExportPlan& plan, juce::var& metrics)
{
    auto a = std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(first));
    auto b = std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(second));
    if (!a || !b || a->lengthInSamples != plan.frames || b->lengthInSamples != plan.frames || a->numChannels != 2 || b->numChannels != 2)
        return juce::Result::fail("WAV readers do not match the planned stereo dimensions.");
    juce::AudioBuffer<float> aa(2, 32768), bb(2, 32768); float maximum = 0; double energy = 0;
    for (juce::int64 offset = 0; offset < plan.frames; offset += 32768)
    {
        const int count = (int) std::min<juce::int64>(32768, plan.frames - offset);
        if (!a->read(&aa, 0, count, offset, true, true) || !b->read(&bb, 0, count, offset, true, true)) return juce::Result::fail("Incomplete WAV comparison.");
        for (int channel = 0; channel < 2; ++channel) for (int i = 0; i < count; ++i)
        { const auto difference = aa.getSample(channel, i) - bb.getSample(channel, i); maximum = std::max(maximum, std::abs(difference)); energy += (double) difference * difference; }
    }
    property(metrics, "maximumDifference", maximum); property(metrics, "rmsDifference", std::sqrt(energy / (2. * plan.frames)));
    property(metrics, "frames", plan.frames); return juce::Result::ok();
}
}

void runLongOpenTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report, const juce::StringArray& args)
{
    const auto path = argument(args, "--session"); const int minutes = minutesArgument(args);
    check(minutes > 0 && juce::File::isAbsolutePath(path), "long-open probe has an explicit session and duration");
    if (minutes == 0 || !juce::File::isAbsolutePath(path)) return;
    const juce::File session(path); const auto before = readVerificationMetrics(); const auto began = juce::Time::getMillisecondCounterHiRes();
    Engine engine; const auto error = engine.restoreProject(session.loadFileAsString());
    const auto after = readVerificationMetrics(); const auto elapsed = juce::Time::getMillisecondCounterHiRes() - began;
    check(error.isEmpty() && engine.playbackReady() && std::abs(engine.project.duration() - minutes * 60.) < 1e-8,
          "fresh process restores the complete real-source long session: " + error);
    int cachedAnalysis = 0, cachedAudio = 0; juce::int64 decodedBytes = 0;
    for (const auto& media : engine.library)
    { cachedAnalysis += media->analysisFromCache ? 1 : 0; cachedAudio += media->decodedFromCache ? 1 : 0; decodedBytes += (juce::int64) media->frames() * media->channels() * sizeof(float); }
    check(engine.library.size() == 7 && engine.project.mastering.saturation && engine.project.effectTailSeconds == .5 && engine.plugins[0].size() == 1 && engine.plugins[2].size() == 1
          && engine.project.labels.size() == engine.project.clips.size() && !engine.project.transitionLinks.empty() && engine.project.transitionLinks.front().locked,
          "long-session reopen retains all seven sources, master settings, tail policy and actual lane inserts");
    if (args.contains("--expect-warm")) check(cachedAnalysis == 7 && cachedAudio == 7, "warm fresh-process opening reuses all seven source analyses and decoded PCM entries");
    auto metrics = object(); property(metrics, "format", "mixworks-long-open"); property(metrics, "session", path); property(metrics, "minutes", minutes);
    property(metrics, "milliseconds", elapsed); property(metrics, "before", verificationMetricsJson(before)); property(metrics, "after", verificationMetricsJson(after));
    property(metrics, "analysisCacheHits", cachedAnalysis); property(metrics, "pcmCacheHits", cachedAudio); property(metrics, "decodedBytes", decodedBytes);
    property(metrics, "sourceCount", (int) engine.library.size()); property(metrics, "clipCount", (int) engine.project.clips.size());
    property(metrics, "labelCount", (int) engine.project.labels.size()); property(metrics, "transitionCount", (int) engine.project.transitionLinks.size());
    property(metrics, "lockedTransitions", engine.project.hasLockedTransitions());
    property(metrics, "error", error); property(metrics, "cacheScope", "Application cache cold/warm; OS page cache is not cleared.");
    writeMetrics(report.getSiblingFile("reopen-metrics.json"), metrics);
}

void runLongSessionTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report, const juce::StringArray& args)
{
    const int minutes = minutesArgument(args); const auto sourceList = argument(args, "--development-paths");
    const auto gainPath = argument(args, "--plugin"), delayPath = argument(args, "--delay-plugin");
    check(minutes > 0 && juce::File::isAbsolutePath(sourceList) && juce::File::isAbsolutePath(gainPath) && juce::File::isAbsolutePath(delayPath), "long workflow has explicit 60/90-minute, seven-source and actual plugin inputs");
    if (!minutes || !juce::File::isAbsolutePath(sourceList) || !juce::File::isAbsolutePath(gainPath) || !juce::File::isAbsolutePath(delayPath)) return;
    const juce::File listFile(sourceList); const auto list = listFile.getSize() <= 2 * 1024 * 1024 ? juce::JSON::parse(listFile) : juce::var();
    check(list.isArray() && list.getArray()->size() == 7, "explicit development manifest has the seven previously used recordings"); if (!list.isArray() || list.getArray()->size() != 7) return;
    auto metrics = object(); const auto metricsFile = report.getSiblingFile("long-session-metrics.json"); property(metrics, "minutes", minutes);
    property(metrics, "scope", "Real-source repeated manual arrangement; no automatic beatmatching, independent musical review, listening or physical-driver soak claim.");
    property(metrics, "executableSha256", juce::SHA256(juce::File::getSpecialLocation(juce::File::currentExecutableFile)).toHexString());
    const auto before = readVerificationMetrics(); property(metrics, "before", verificationMetricsJson(before));
    Workstation ui(false); ui.stopTimer(); ui.setSize(1440, 940); ui.addToDesktop(0); ui.setVisible(true); auto& engine = ui.engine;
    engine.sampleRate = 44100; engine.project.followTrackTempo = false; engine.project.snap = false; engine.project.bpm = 120;
    engine.project.name = "Owned " + juce::String(minutes) + " minute acceptance";
    juce::Array<juce::var> sources; std::vector<juce::String> sourceHashes; juce::int64 decodedBytes = 0, largestSource = 0;
    property(metrics, "freeDiskBytesBefore", report.getParentDirectory().getBytesFreeOnVolume());
    for (const auto& entry : *list.getArray())
    {
        if (!entry.isString() || !juce::File::isAbsolutePath(entry.toString())) { check(false, "development source has an absolute original path"); return; }
        const juce::File file(entry.toString()); const auto began = juce::Time::getMillisecondCounterHiRes();
        auto loaded = readMedia(engine.formats, file); check(loaded.media && loaded.media->hasPlayback() && loaded.media->analysisSource.sha256.length() == 64, "production import loads development source " + file.getFileName() + ": " + loaded.error); if (!loaded.media || loaded.media->analysisSource.sha256.length() != 64) return;
        auto row = object(); property(row, "path", file.getFullPathName()); property(row, "sha256", loaded.media->analysisSource.sha256);
        property(row, "frames", loaded.media->frames()); property(row, "sampleRate", loaded.media->sampleRate); property(row, "milliseconds", juce::Time::getMillisecondCounterHiRes() - began);
        property(row, "rhythmIssue", loaded.media->analysis.rhythmIssue); property(row, "variableTempo", loaded.media->analysis.variableTempo);
        const auto bytes = (juce::int64) loaded.media->frames() * loaded.media->channels() * (juce::int64) sizeof(float);
        decodedBytes += bytes; largestSource = std::max(largestSource, bytes); property(row, "decodedBytes", bytes);
        sourceHashes.push_back(loaded.media->analysisSource.sha256); engine.library.push_back(loaded.media); sources.add(row); property(metrics, "sources", sources); writeMetrics(metricsFile, metrics);
    }
    const double duration = minutes * 60.; double cursor = 0; size_t index = 0;
    while (cursor < duration)
    {
        Clip clip; clip.media = engine.library[index % engine.library.size()]; clip.track = (int) (index % trackCount); clip.start = cursor;
        clip.length = std::min(clip.media->duration(), duration - cursor); clip.gainDb = -15;
        if (clip.length <= 16) { check(false, "development source has enough audio for finite overlaps"); return; }
        clip.fadeIn = index == 0 ? 0 : 16; clip.fadeOut = std::min(16., clip.length / 2);
        clip.controls.automation[0].enabled = true; clip.controls.automation[0].setPoint(0, -2); clip.controls.automation[0].setPoint(clip.length, -4);
        LabelMarker label; label.time = cursor; label.text = clip.media->name.substring(0, 256); const auto labelResult = engine.project.putLabelMarker(label); if (labelResult.failed()) { check(false, labelResult.getErrorMessage()); return; }
        engine.project.clips.push_back(clip); ++index; if (clip.end() >= duration) break; cursor = clip.end() - 16;
    }
    const auto linked = engine.project.linkClips(engine.project.clips[0].id, engine.project.clips[1].id);
    const auto locked = engine.project.setTransitionLocked(engine.project.clips[1].id, true);
    check(linked.wasOk() && locked.wasOk(), "explicitly link and lock the first manual overlap before persistence");
    if (linked.failed() || locked.failed()) return;
    engine.project.setEffectTailSeconds(.5); engine.project.mastering.eqDb = {1, -.5f, -1}; engine.project.mastering.width = 1.1f;
    engine.project.mastering.saturation = true; engine.project.mastering.saturationDriveDb = 2;
    engine.project.masterAutomation.enabled = true; engine.project.masterAutomation.setPoint(0, -6); engine.project.masterAutomation.setPoint(duration / 2, -4); engine.project.masterAutomation.setPoint(duration, -6);
    engine.selectedClip = {}; const auto gainError = engine.addPlugin(0, juce::File(gainPath)); const auto delayError = engine.addPlugin(2, juce::File(delayPath));
    check(gainError.isEmpty() && delayError.isEmpty(), "long workflow uses actual zero-latency Gain and finite Delay VST3s: " + gainError + delayError); if (gainError.isNotEmpty() || delayError.isNotEmpty()) return;
    auto parameter = [](PluginSlot& slot, const juce::String& name) { return std::find_if(slot.parameters.begin(), slot.parameters.end(), [&](const auto& item) { return item.second->getName(128) == name; }); };
    auto& gain = *engine.plugins[0][0]; auto& delay = *engine.plugins[2][0]; const auto gainParameter = parameter(gain, "Gain"), wetParameter = parameter(delay, "Wet mix");
    check(gainParameter != gain.parameters.end() && wetParameter != delay.parameters.end(), "long workflow binds actual inserts by named parameter contracts"); if (gainParameter == gain.parameters.end() || wetParameter == delay.parameters.end()) return;
    gainParameter->second->setValueNotifyingHost(.65f); wetParameter->second->setValueNotifyingHost(.15f);
    const AutomationAddress gainAddress {0, AutomationTarget::plugin, gain.id, gainParameter->first}; engine.ensurePluginAutomation(gainAddress);
    auto* gainCurve = engine.project.curve(gainAddress); gainCurve->enabled = true; gainCurve->setPoint(0, .45f); gainCurve->setPoint(duration, .75f);
    engine.prepareClipProcessing(); ui.refresh(); property(metrics, "clips", (int) engine.project.clips.size()); property(metrics, "musicalDuration", engine.project.duration());
    property(metrics, "uniqueDecodedBytes", decodedBytes); property(metrics, "largestDecodedSourceBytes", largestSource);
    juce::int64 preparedBytes = 0; for (const auto& clip : engine.project.clips) if (clip.prepared) preparedBytes += (juce::int64) clip.prepared->frames() * clip.prepared->channels() * (juce::int64) sizeof(float);
    property(metrics, "preparedBytes", preparedBytes); property(metrics, "afterImport", verificationMetricsJson(readVerificationMetrics()));
    check(std::abs(engine.project.duration() - duration) < 1e-8 && engine.playbackReady(), "manual overlap arrangement reaches the exact requested duration with all real audio ready");
    const auto baseline = modelJson(engine); const auto editable = std::find_if(engine.project.clips.rbegin(), engine.project.clips.rend(), [&](const auto& clip) { return !engine.project.clipTransitionLocked(clip.id); });
    if (editable == engine.project.clips.rend()) { check(false, "long fixture retains an editable song outside its locked pair"); return; }
    const auto editId = editable->id; const auto oldStart = editable->start; ui.selectedClip = editId; engine.selectedClip = editId; engine.selectedTrack = editable->track; ui.refresh();
    check(std::abs(ui.clipSliders[0].getValue() - oldStart) < .001, "native inspector displays the actual long-timeline song start");
    const auto undoCount = ui.undoHistory.size(); ui.clipSliders[0].onDragStart(); ui.clipSliders[0].setValue(oldStart + .125, juce::sendNotificationSync); ui.clipSliders[0].onDragEnd();
    check(ui.undoHistory.size() == undoCount + 1 && std::abs(engine.project.clipFor(editId)->start - oldStart - .125) < .001, "native numerical move on a long timeline records one Undo and the requested offset");
    ui.undo(false); check(modelJson(engine) == baseline, "Undo restores the complete long arrangement and automation"); ui.undo(true); ui.undo(false);
    check(modelJson(engine) == baseline, "Redo and a second Undo retain the original long mix exactly");
    const auto session = report.getSiblingFile("long-session.mixworks"); ui.dirty = true;
    check(ui.saveSessionFile(session) && !ui.dirty, "native save writes the owned long session and clears its dirty state");
    writeMetrics(metricsFile, metrics);
    auto image = report.getSiblingFile("long-session.png").createOutputStream(); juce::PNGImageFormat png;
    check(image && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *image), "capture actual long-session native workspace");
    image.reset();
    for (int pass = 0; pass < 2; ++pass)
    {
        const auto folder = report.getParentDirectory().getChildFile(pass == 0 ? "cold-open" : "warm-open"); folder.createDirectory();
        juce::StringArray command {juce::File::getSpecialLocation(juce::File::currentExecutableFile).getFullPathName(), "--self-test", "--long-open-only", "--session", session.getFullPathName(), "--minutes", juce::String(minutes), "--playback-cache-root", report.getSiblingFile("reopen-cache").getFullPathName(), "--report", folder.getChildFile("checks.txt").getFullPathName()};
        if (pass == 1) command.add("--expect-warm"); juce::ChildProcess child; const bool started = child.start(command, 0); const auto deadline = juce::Time::getMillisecondCounterHiRes() + 600000;
        while (started && child.isRunning() && juce::Time::getMillisecondCounterHiRes() < deadline) child.waitForProcessToFinish(1000);
        bool timedOut = false;
        if (started && child.isRunning()) { timedOut = true; child.kill(); child.waitForProcessToFinish(5000); }
        const auto result = juce::JSON::parse(folder.getChildFile("reopen-metrics.json"));
        check(started && !timedOut && !child.isRunning() && child.getExitCode() == 0 && result.isObject() && result["error"].toString().isEmpty(), pass == 0 ? "cache-cold fresh native process opens the complete long session" : "cache-warm fresh native process opens the complete long session");
        property(metrics, pass == 0 ? "coldOpen" : "warmOpen", result); writeMetrics(metricsFile, metrics);
    }
    const auto output = report.getSiblingFile("offline.wav"), device = report.getSiblingFile("device.wav"); WavExportPlan plan;
    const auto planned = engine.planWavExport(output, {true}, plan); check(planned.wasOk(), "plan the exact full long stereo WAV and selected tail: " + planned.getErrorMessage()); if (planned.failed()) return;
    auto deviceMetrics = object(); const auto deviceResult = renderDeviceFile(engine, device, plan, deviceMetrics, report.getSiblingFile("long-session-progress.json"));
    check(deviceResult.wasOk(), "full continuous production device callback renders the long mix: " + deviceResult.getErrorMessage()); property(metrics, "device", deviceMetrics); writeMetrics(metricsFile, metrics);
    const double clockTolerance = std::max(1.0e-7, 8 * std::numeric_limits<double>::epsilon() * plan.frames);
    check(deviceResult.wasOk() && (double) deviceMetrics["maximumClockErrorSamples"] <= clockTolerance, "continuous playback clock retains integer-frame time across the entire long mix");
    juce::String renderError; const auto originalState = juce::JSON::toString(engine.captureProject()); auto renderer = engine.createExportRenderer(renderError);
    const auto exportStarted = juce::Time::getMillisecondCounterHiRes(); WavExportResult exported; juce::int64 nextExportProgress = 0;
    if (renderer) exported = renderer->writeWav(output, {true}, {}, [&](auto done, const auto& detail)
    {
        if (done < nextExportProgress) return; auto state = object(); property(state, "stage", "private offline render");
        property(state, "frames", done); property(state, "totalFrames", detail.frames); property(state, "elapsedMilliseconds", juce::Time::getMillisecondCounterHiRes() - exportStarted);
        writeMetrics(report.getSiblingFile("long-session-progress.json"), state); nextExportProgress += (juce::int64) (plan.sampleRate * 30);
    });
    check(renderer && exported.wasOk() && exported.writtenFrames == plan.frames, "private offline renderer writes the entire real-source mix and tail: " + renderError + exported.error);
    property(metrics, "offlineMilliseconds", juce::Time::getMillisecondCounterHiRes() - exportStarted);
    check(juce::JSON::toString(engine.captureProject()) == originalState, "full long export preserves live plugin state and model");
    auto comparison = object(); const auto compared = compareFiles(engine.formats, device, output, plan, comparison);
    check(compared.wasOk() && (double) comparison["maximumDifference"] <= 4. / 8388608, "every live-callback and offline WAV sample agrees within four PCM24 steps: " + compared.getErrorMessage());
    property(metrics, "comparison", comparison); property(metrics, "outputFrames", plan.frames); property(metrics, "outputBytes", output.getSize());
    property(metrics, "offlineSha256", audioDigest(output)); property(metrics, "deviceSha256", audioDigest(device)); writeMetrics(metricsFile, metrics);
    const auto reimportStarted = juce::Time::getMillisecondCounterHiRes(); auto reimported = readMedia(engine.formats, output, {}, AnalysisCacheMode::readOnly);
    check(reimported.media && reimported.media->frames() == plan.frames && reimported.media->sampleRate == plan.sampleRate && reimported.media->channels() == 2 && reimported.media->cache,
          "production importer reopens the actual complete long WAV without splitting it: " + reimported.error);
    property(metrics, "reimportMilliseconds", juce::Time::getMillisecondCounterHiRes() - reimportStarted);
    if (reimported.media)
    {
        property(metrics, "reimportRhythmIssue", reimported.media->analysis.rhythmIssue); property(metrics, "reimportPlaybackWarning", reimported.media->playbackCacheWarning);
        auto encoded = std::unique_ptr<juce::AudioFormatReader>(engine.formats.createReaderFor(output)); juce::AudioBuffer<float> chunk(2, 32768); bool exact = encoded != nullptr;
        for (juce::int64 offset = 0; exact && offset < plan.frames; offset += 32768)
        {
            const int count = (int) std::min<juce::int64>(32768, plan.frames - offset); exact = encoded->read(&chunk, 0, count, offset, true, true);
            for (int channel = 0; exact && channel < 2; ++channel) for (int i = 0; exact && i < count; ++i)
                exact = chunk.getSample(channel, i) == reimported.media->channelData(channel)[offset + i];
        }
        check(exact, "every reimported real-mix sample matches its actual exported PCM24 data");
    }
    bool unchanged = true; for (size_t i = 0; i < engine.library.size(); ++i) unchanged = unchanged && audioDigest(engine.library[i]->file) == sourceHashes[i];
    check(unchanged, "long arrangement, save/open, render and reimport preserve every original development recording");
    property(metrics, "originalSourcesUnchanged", unchanged); property(metrics, "after", verificationMetricsJson(readVerificationMetrics()));
    property(metrics, "freeDiskBytesAfter", report.getParentDirectory().getBytesFreeOnVolume()); writeMetrics(metricsFile, metrics);
}
void runLongNavigationTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report, const juce::StringArray& args)
{
    const auto sessionPath = argument(args, "--session"), referencePath = argument(args, "--reference"); const int minutes = minutesArgument(args);
    const bool valid = minutes > 0 && juce::File::isAbsolutePath(sessionPath) && juce::File::isAbsolutePath(referencePath);
    check(valid, "long navigation selects an explicit owned session, accepted WAV and duration"); if (!valid) return;
    const juce::File session(sessionPath), reference(referencePath);
    check(session.existsAsFile() && reference.existsAsFile(), "the accepted long-session inputs are available read-only");
    if (!session.existsAsFile() || !reference.existsAsFile()) return;
    const auto sessionHash = audioDigest(session), referenceHash = audioDigest(reference);
    auto metrics = object(); const auto metricsFile = report.getSiblingFile("navigation-metrics.json");
    property(metrics, "scope", "Numerical production callbacks and normal live insertion controller; no physical device, listening, OS picker or automatic musical alignment claim.");
    property(metrics, "session", sessionPath); property(metrics, "sessionSha256", sessionHash); property(metrics, "reference", referencePath); property(metrics, "referenceSha256", referenceHash);
    property(metrics, "before", verificationMetricsJson(readVerificationMetrics()));
    Workstation ui(false); ui.stopTimer(); ui.setSize(1440, 940); auto& engine = ui.engine;
    const auto began = juce::Time::getMillisecondCounterHiRes(); const auto error = engine.restoreProject(session.loadFileAsString());
    property(metrics, "openMilliseconds", juce::Time::getMillisecondCounterHiRes() - began);
    check(error.isEmpty() && engine.playbackReady() && std::abs(engine.project.duration() - minutes * 60.) < 1e-8,
          "restore the complete saved long project through the current source-analysis boundary: " + error);
    if (error.isNotEmpty() || !engine.playbackReady()) return;
    auto reader = std::unique_ptr<juce::AudioFormatReader>(engine.formats.createReaderFor(reference));
    const double rate = engine.sampleRate.load(); const auto frames = (juce::int64) std::ceil(engine.project.audibleEnd() * rate - 1e-6);
    const bool dimensions = reader && reader->sampleRate == rate && reader->numChannels == 2 && reader->lengthInSamples == frames && reader->bitsPerSample == 24;
    check(dimensions, "reference PCM24 dimensions and selected effect tail match the restored session"); if (!dimensions) return;
    std::vector<std::pair<juce::File, juce::String>> sourceHashes;
    for (const auto& source : engine.library) sourceHashes.push_back({source->file, audioDigest(source->file)});
    const bool identities = sessionHash.isNotEmpty() && referenceHash.isNotEmpty()
        && std::all_of(sourceHashes.begin(), sourceHashes.end(), [](const auto& source) { return source.second.isNotEmpty(); });
    check(identities, "every original input has a complete source identity before navigation"); if (!identities) return;
    const auto baseline = modelJson(engine); juce::Array<juce::var> windows;
    struct Rendered { bool complete = true, stoppedNaturally = false; double difference = 0, clockError = 0; };
    auto renderWindow = [&](juce::int64 first, int count, int preroll, bool seekFirst, bool stopAfter)
    {
        Rendered rendered; float left[512] {}, right[512] {}; float* channels[] {left, right}; juce::AudioBuffer<float> expected(2, 512);
        if (seekFirst) { engine.seek(first / rate); engine.setPlaying(true); }
        for (int offset = 0; offset < count; offset += 512)
        {
            const int block = std::min(512, count - offset);
            if (!engine.playing) { rendered.complete = false; break; }
            engine.audioDeviceIOCallbackWithContext(nullptr, 0, channels, 2, block, {});
            const auto target = std::min<juce::int64>(frames, first + offset + block);
            rendered.clockError = std::max(rendered.clockError, std::abs(engine.position.load() * rate - target));
            if (offset + block <= preroll) continue;
            if (!reader->read(&expected, 0, block, first + offset, true, true)) { rendered.complete = false; break; }
            for (int channel = 0; channel < 2; ++channel) for (int i = std::max(0, preroll - offset); i < block; ++i)
            {
                const float actual = channels[channel][i], value = expected.getSample(channel, i);
                if (!std::isfinite(actual) || !std::isfinite(value)) rendered.complete = false;
                else rendered.difference = std::max(rendered.difference, (double) std::abs(actual - value));
            }
        }
        rendered.stoppedNaturally = !engine.playing;
        if (stopAfter) engine.setPlaying(false); return rendered;
    };
    const int sixSeconds = (int) std::llround(6 * rate), preroll = (int) std::llround(2 * rate);
    const double tolerance = 4. / 8388608, clockTolerance = 8 * std::numeric_limits<double>::epsilon() * frames;
    const juce::int64 starts[] {(juce::int64) (frames * .95), (juce::int64) (frames * .10), (juce::int64) (frames * .50), frames - sixSeconds};
    bool endingStopped = false;
    for (const auto start : starts)
    {
        const auto result = renderWindow(start, sixSeconds, preroll, true, true);
        if (start == frames - sixSeconds) endingStopped = result.stoppedNaturally;
        check(result.complete && result.difference <= tolerance, "long seek at " + juce::String(start / rate, 3) + "s agrees with accepted continuous PCM after two-second effect-history preroll: " + juce::String(result.difference, 9));
        check(result.complete && result.clockError <= clockTolerance, "long seek retains its integer-frame anchor across every device callback");
        auto row = object(); property(row, "startFrame", start); property(row, "maximumDifference", result.difference); property(row, "maximumClockErrorSamples", result.clockError);
        windows.add(row); property(metrics, "windows", windows); writeMetrics(metricsFile, metrics);
    }
    check(endingStopped && std::abs(engine.position.load() - engine.project.audibleEnd()) < 1e-10, "last partial callback stops exactly at the accepted long audible ending");
    check(modelJson(engine) == baseline, "random forward/back/end seeks preserve the complete musical model");
    // The owned fixture's lock correctly forbids global tempo changes. Explicitly
    // unlock it before testing a successful live append that adds a tempo target.
    for (const auto& link : engine.project.transitionLinks) if (link.locked) engine.project.setTransitionLocked(link.incoming, false);
    ui.refresh(); const auto unlocked = engine.project; const auto unlockedJson = modelJson(engine); const auto undoCount = ui.undoHistory.size();
    const auto middle = (juce::int64) (frames * .5); const auto primed = renderWindow(middle, (int) (3 * rate), preroll, true, false);
    check(primed.complete && primed.difference <= tolerance && engine.playing, "prime actual long-mix Gain/Delay/EQ history while keeping transport running");
    const auto at = engine.position.load(); const auto currentFrame = (juce::int64) std::llround(at * rate);
    ui.addMedia(0);
    const bool appended = engine.project.clips.size() == unlocked.clips.size() + 1 && !engine.project.hasPendingPlacement() && !ui.busy;
    auto expectedOriginals = unlocked;
    const Clip* incoming = appended ? &engine.project.clips.back() : nullptr;
    const auto* relationship = incoming ? engine.project.transitionFor(incoming->id) : nullptr;
    const auto& oldOut = unlocked.clips.back(); const auto* currentOut = engine.project.clipFor(oldOut.id);
    const bool outRetained = relationship && relationship->outgoing == oldOut.id && currentOut
        && std::abs(oldOut.start + oldOut.transitionTime(false) - currentOut->start - currentOut->transitionTime(false)) < 1e-7;
    const double cue = incoming ? (incoming->firstDownbeatTime() >= 0 ? incoming->firstDownbeatTime() : incoming->firstBeatTime()) : 0;
    const bool inRetained = incoming && incoming->introAnchor && std::abs(*incoming->introAnchor - incoming->sourceElapsed(cue)) < 1e-7;
    check(outRetained, "live alignment preserves the outgoing source OUT even when incoming cue is exactly at its end");
    check(inRetained, "live alignment retains the incoming source cue instead of replacing it with file start");
    // Adopting an equivalent explicit OUT is allowed; its source-time meaning is
    // checked above. Every other old clip field must remain exactly unchanged.
    if (outRetained && !oldOut.outroAnchor) expectedOriginals.clipFor(oldOut.id)->outroAnchor = currentOut->outroAnchor;
    bool originalsRetained = true;
    const auto originalClips = projectToJson(expectedOriginals)["clips"], currentClips = projectToJson(engine.project)["clips"];
    const auto rawOriginalClips = projectToJson(unlocked)["clips"];
    originalsRetained = currentClips.size() >= originalClips.size();
    for (int i = 0; originalsRetained && i < originalClips.size(); ++i)
        originalsRetained = originalsRetained && juce::JSON::toString(originalClips[i]) == juce::JSON::toString(currentClips[i]);
    auto appendFacts = object(); property(appendFacts, "appended", appended); property(appendFacts, "playing", engine.playing.load());
    property(appendFacts, "positionBefore", at); property(appendFacts, "positionAfter", engine.position.load());
    property(appendFacts, "oldClipCount", (int) unlocked.clips.size()); property(appendFacts, "newClipCount", (int) engine.project.clips.size());
    property(appendFacts, "pendingPlacement", engine.project.hasPendingPlacement()); property(appendFacts, "status", ui.status);
    property(appendFacts, "originalOutTime", oldOut.start + oldOut.transitionTime(false));
    property(appendFacts, "currentOutTime", currentOut ? juce::var(currentOut->start + currentOut->transitionTime(false)) : juce::var());
    property(appendFacts, "expectedIncomingSourceCue", incoming ? juce::var(incoming->sourceElapsed(cue)) : juce::var());
    property(appendFacts, "actualIncomingIN", incoming && incoming->introAnchor ? juce::var(*incoming->introAnchor) : juce::var());
    juce::Array<juce::var> differences;
    for (int i = 0; i < rawOriginalClips.size() && i < currentClips.size(); ++i)
        if (juce::JSON::toString(rawOriginalClips[i]) != juce::JSON::toString(currentClips[i]))
        {
            auto entry = object(); property(entry, "index", i); property(entry, "before", rawOriginalClips[i]); property(entry, "after", currentClips[i]); differences.add(entry);
        }
    property(appendFacts, "originalClipDifferences", differences); property(metrics, "liveAppend", appendFacts); writeMetrics(metricsFile, metrics);
    check(appended && engine.playing && engine.position.load() == at && originalsRetained,
          "normal live append adds an already hydrated source while retaining transport and every original long-mix clip");
    const auto live = renderWindow(currentFrame, (int) (2 * rate), 0, false, true);
    check(live.complete && live.difference <= tolerance && live.clockError <= clockTolerance,
          "live insertion retains continuous original-region PCM, automation and actual effect history: " + juce::String(live.difference, 9));
    check(ui.undoHistory.size() == undoCount + 1, "successful long live insertion records exactly one Undo");
    const auto appendedJson = modelJson(engine); ui.undo(false);
    check(modelJson(engine) == unlockedJson, "paused Undo restores the complete pre-insertion long model"); ui.undo(true);
    check(modelJson(engine) == appendedJson, "Redo restores the exact appended long model");
    const auto savedInsertion = report.getSiblingFile("live-insertion.mixworks"); ui.dirty = true;
    const bool saved = ui.saveSessionFile(savedInsertion);
    check(saved && savedInsertion.existsAsFile() && !ui.dirty, "normal Save persists the completed long live insertion into an owned new session");
    Engine reopened; const auto reopenBegan = juce::Time::getMillisecondCounterHiRes();
    const auto reopenError = saved ? reopened.restoreProject(savedInsertion.loadFileAsString()) : juce::String("Save did not complete.");
    property(metrics, "postInsertionReopenMilliseconds", juce::Time::getMillisecondCounterHiRes() - reopenBegan);
    check(reopenError.isEmpty() && reopened.playbackReady() && modelJson(reopened) == appendedJson,
          "Save/reopen retains the full post-insertion graph and captured source clocks: " + reopenError);
    if (reopenError.isEmpty() && reopened.playbackReady())
    {
        const auto* added = reopened.project.clipFor(engine.project.clips.back().id); bool finite = added != nullptr; double energy = 0; int samples = 0;
        float left[512] {}, right[512] {}; float* outputs[] {left, right};
        if (added) { reopened.seek(added->start + 10); reopened.setPlaying(true); }
        const int total = (int) (3 * rate), history = (int) (2 * rate);
        for (int offset = 0; finite && offset < total; offset += 512)
        {
            const int block = std::min(512, total - offset); finite = reopened.playing;
            if (!finite) break; reopened.audioDeviceIOCallbackWithContext(nullptr, 0, outputs, 2, block, {});
            for (int channel = 0; channel < 2; ++channel) for (int i = std::max(0, history - offset); i < block; ++i)
            {
                const float value = outputs[channel][i]; finite = finite && std::isfinite(value); energy += (double) value * value; ++samples;
            }
        }
        reopened.setPlaying(false); const double rms = samples > 0 ? std::sqrt(energy / samples) : 0;
        check(finite && rms > 1e-5, "the reopened incoming material produces finite audible PCM through its saved source clock and actual inserts");
        property(metrics, "reopenedIncomingRms", rms);
    }
    property(metrics, "postInsertionSession", savedInsertion.getFullPathName());
    if (saved) property(metrics, "postInsertionSessionSha256", audioDigest(savedInsertion));
    bool unchanged = audioDigest(session) == sessionHash && audioDigest(reference) == referenceHash;
    for (const auto& source : sourceHashes) unchanged = unchanged && audioDigest(source.first) == source.second;
    check(unchanged && engine.library.size() == sourceHashes.size(), "seek/live insertion/Undo/Redo leave every original session, reference and development source byte unchanged");
    property(metrics, "liveInsertionDifference", live.difference); property(metrics, "originalInputsUnchanged", unchanged);
    property(metrics, "after", verificationMetricsJson(readVerificationMetrics())); writeMetrics(metricsFile, metrics);
}

}
