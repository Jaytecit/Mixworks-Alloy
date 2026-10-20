#include "Workstation.h"
#include "Beatmatch.h"
#include <cmath>

namespace mixworks
{
namespace
{
// Exercise production rendering, with each stem kept separately for independent
// onset measurements. No audio device, user session or writable analysis is opened.
bool renderWindow(Engine& engine, const juce::File& file, double begin, double length)
{
    if (file.exists()) return false;
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    if (!stream) return false;
    juce::WavAudioFormat format;
    auto writer = format.createWriterFor(stream, juce::AudioFormatWriterOptions()
        .withSampleRate(engine.sampleRate).withNumChannels(2).withBitsPerSample(24));
    if (!writer) return false;
    juce::AudioBuffer<float> pcm(2, 512); juce::MidiBuffer midi;
    const auto total = (int64_t) std::ceil(length * engine.sampleRate.load());
    for (int64_t frame = 0; frame < total; frame += 512)
    {
        const int count = (int) std::min<int64_t>(512, total - frame);
        engine.render(pcm, count, begin + frame / engine.sampleRate.load(), midi);
        if (!writer->writeFromAudioSampleBuffer(pcm, 0, count)) return false;
    }
    return true;
}
}

void runRealTrackTests(const std::function<void(bool, const juce::String&)>& check,
                      const juce::File& report, const juce::StringArray& args)
{
    const auto output = report.getParentDirectory();
    auto metadataFile = output.getChildFile("measurements-input.json");
    check(!metadataFile.exists(), "real-track output directory has no previous measurements");
    if (metadataFile.exists()) return;
    juce::var document(new juce::DynamicObject()); auto* root = document.getDynamicObject();
    juce::Array<juce::var> readinessChecks;
    const auto readinessCheck = [&](bool passed, const juce::String& name)
    {
        juce::var value(new juce::DynamicObject());
        value.getDynamicObject()->setProperty("name", name);
        value.getDynamicObject()->setProperty("passed", passed);
        readinessChecks.add(value);
        root->setProperty("readinessChecks", readinessChecks);
        check(passed, name);
    };
    root->setProperty("analysisRevision", analysisRevision);
    root->setProperty("applicationVersion", JUCE_APPLICATION_VERSION_STRING);
    root->setProperty("purpose", "Real audio readiness probe; generated beat positions are predictions, not ground truth.");
    juce::var references;
    const int referenceFlag = args.indexOf("--downbeat-reference");
    if (referenceFlag >= 0)
    {
        const bool valid = referenceFlag + 1 < args.size() && juce::File::isAbsolutePath(args[referenceFlag + 1].unquoted());
        if (valid) references = juce::JSON::parse(juce::File(args[referenceFlag + 1].unquoted()))["references"];
        check(valid && references.isArray(), "load explicit downbeat reference annotations");
        if (!references.isArray()) return;
    }
    juce::Array<juce::var> sources, scenarios;
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    Project input; input.masterDb = 0;
    const bool bandControls = args.contains("--transition-bands-only");
    for (int track = 0; track < 2; ++track)
    {
        const auto flag = track == 0 ? "--track-a" : "--track-b";
        const int index = args.indexOf(flag);
        const bool validPath = index >= 0 && index + 1 < args.size() && juce::File::isAbsolutePath(args[index + 1].unquoted());
        check(validPath, juce::String(flag) + " has an explicit absolute input path");
        if (!validPath) return;
        const juce::File path(args[index + 1].unquoted());
        juce::Array<juce::var> coldStages, warmStages;
        const auto timedRead = [&](juce::Array<juce::var>& stages)
        {
            auto previous = juce::Time::getMillisecondCounterHiRes(); auto phase = MediaReadStage::lookupAnalysis;
            const auto append = [&](double now)
            {
                auto* value = new juce::DynamicObject(); value->setProperty("stage", (int) phase); value->setProperty("milliseconds", now - previous);
                stages.add(juce::var(value)); previous = now;
            };
            auto result = readMedia(formats, path, [&](MediaReadStage next)
            { const auto now = juce::Time::getMillisecondCounterHiRes(); append(now); phase = next; }, AnalysisCacheMode::readWrite);
            append(juce::Time::getMillisecondCounterHiRes()); return result;
        };
        const auto coldStarted = juce::Time::getMillisecondCounterHiRes();
        auto loaded = bandControls ? timedRead(coldStages) : readMedia(formats, path, {}, AnalysisCacheMode::readOnly);
        const double coldMs = juce::Time::getMillisecondCounterHiRes() - coldStarted;
        check(loaded.media != nullptr, "production decoder loads " + path.getFileName() + ": " + loaded.error);
        if (!loaded.media) return;
        auto media = loaded.media;
        const bool cachedAtLoad = media->analysisFromCache;
        double warmMs = 0;
        if (bandControls)
        {
            const auto warmStarted = juce::Time::getMillisecondCounterHiRes(); auto warm = timedRead(warmStages);
            warmMs = juce::Time::getMillisecondCounterHiRes() - warmStarted;
            check(warm.media && warm.media->analysisFromCache && warm.media->decodedFromCache
                && warm.media->analysisSource == media->analysisSource, "warm real loading reuses preserved analysis and decoded PCM");
        }
        if (cachedAtLoad && !bandControls)
        {
            juce::AudioBuffer<float> decoded(media->channels(), media->frames());
            for (int ch = 0; ch < media->channels(); ++ch) decoded.copyFrom(ch, 0, media->channelData(ch), media->frames());
            media->analysis = analyseMusic(decoded, media->sampleRate);
        }
        // Test automatic results independently of any saved user correction.
        media->manualBpm = 0; media->manualBeatOrigin = -1;
        Clip clip; clip.media = media; clip.track = track; clip.length = media->duration();
        input.clips.push_back(clip);
        juce::var source(new juce::DynamicObject()); auto* row = source.getDynamicObject();
        row->setProperty("file", path.getFullPathName()); row->setProperty("sha256", media->analysisSource.sha256);
        row->setProperty("sampleRate", media->sampleRate); row->setProperty("frames", media->frames());
        row->setProperty("duration", media->duration()); row->setProperty("bpm", media->analysis.bpm);
        row->setProperty("beatOrigin", media->analysis.beatOrigin); row->setProperty("downbeatOrigin", media->analysis.downbeatOrigin);
        row->setProperty("downbeatConfidence", media->analysis.downbeatConfidence);
        row->setProperty("downbeatExaminedSeconds", media->analysis.downbeatExaminedSeconds);
        row->setProperty("tempoConfidence", media->analysis.tempoConfidence); row->setProperty("variableTempo", media->analysis.variableTempo);
        row->setProperty("analysisFromCacheAtLoad", cachedAtLoad); row->setProperty("recomputedForTest", !bandControls || !cachedAtLoad);
        if (bandControls)
        {
            row->setProperty("firstLoadMs", coldMs); row->setProperty("warmLoadMs", warmMs);
            row->setProperty("firstLoadStages", coldStages); row->setProperty("warmLoadStages", warmStages);
            root->setProperty("loadStageNames", "0 decode, 1 waveform, 2 analysis, 3 cache, 4 source/analysis lookup, 5 saved analysis, 6 playlist, 7 saved audio, 8 tempo");
        }
        row->setProperty("gridErrorMs", media->analysis.gridErrorMs);
        row->setProperty("rhythmMethod", media->analysis.rhythmMethod); row->setProperty("rhythmIssue", media->analysis.rhythmIssue);
        row->setProperty("neuralPhaseCorrectionMs", media->analysis.neuralPhaseCorrectionMs);
        row->setProperty("neuralBpm", media->analysis.neuralBpm); row->setProperty("signalBpm", media->analysis.signalBpm);
        row->setProperty("signalTempoConfidence", media->analysis.signalTempoConfidence); row->setProperty("signalGridNeedsReview", media->analysis.signalGridNeedsReview);
        row->setProperty("downbeatMethod", media->analysis.downbeatMethod);
        row->setProperty("key", keyNoteName(media->analysis.key)); row->setProperty("camelot", keyCamelot(media->analysis.key));
        row->setProperty("keyConfidence", media->analysis.keyConfidence); row->setProperty("alternativeKey", keyNoteName(media->analysis.alternativeKey));
        const auto decoded = output.getChildFile("source-" + juce::String(track) + ".wav");
        row->setProperty("decodedWav", decoded.getFileName());
        Engine engine; engine.sampleRate = media->sampleRate; engine.project.masterDb = 0; engine.project.clips = {clip};
        const double decodedLength = args.contains("--analysis-only") ? std::min(30.0, media->duration()) : media->duration();
        row->setProperty("decodedWavSeconds", decodedLength);
        check(renderWindow(engine, decoded, 0, decodedLength), "render decoded source " + juce::String(track));
        readinessCheck(media->analysis.bpm > 0 && !media->analysis.variableTempo && media->analysis.tempoConfidence >= 0.2,
              "source " + juce::String(track) + " has a usable automatic pulse grid");
        readinessCheck(media->analysis.downbeatOrigin >= 0, "source " + juce::String(track)
              + " automatic first downbeat is available (accuracy still requires annotation)");
        if (references.isArray())
        {
            juce::var reference; int matches = 0;
            for (const auto& candidate : *references.getArray())
                if (candidate["sha256"].toString() == media->analysisSource.sha256) { reference = candidate; ++matches; }
            const auto time = reference["firstDownbeatSeconds"], tolerance = reference["toleranceSeconds"];
            const bool valid = matches == 1 && (time.isDouble() || time.isInt()) && (double) time >= 0 && (double) time < media->duration()
                && (tolerance.isDouble() || tolerance.isInt()) && (double) tolerance > 0 && (double) tolerance <= 0.05;
            check(valid, "source " + juce::String(track) + " matches one digest-bound reference");
            if (valid)
            {
                const double difference = media->analysis.downbeatOrigin - (double) time;
                row->setProperty("downbeatReference", reference); row->setProperty("downbeatReferenceDifferenceMs", difference * 1000);
                check(media->analysis.downbeatOrigin >= 0 && std::abs(difference) <= (double) tolerance,
                      "source " + juce::String(track) + " absolute first downbeat agrees with " + reference["basis"].toString()
                      + ": " + juce::String(media->analysis.downbeatOrigin, 6) + " s, difference " + juce::String(difference * 1000, 3) + " ms");
            }
        }
        sources.add(source);
    }
    root->setProperty("sources", sources);
    if (args.contains("--replacement-grid-only"))
    {
        auto initial = input;
        for (auto& clip : initial.clips) clip.pendingPlacement = PendingPlacement::append;
        const auto original = prepareTempoMap(initial, false, 32);
        check(original.error.isEmpty(), "prepare real recordings for removal/replacement: " + original.error);
        if (original.error.isNotEmpty()) return;
        Workstation ui(false); ui.stopTimer(); ui.setSize(1440, 940);
        for (int removed : {0, 1}) for (bool live : {false, true})
        {
            ui.engine.project = original.project; ui.selectedClip = original.project.clips[(size_t) removed].id;
            ui.removeClip();
            auto added = input.clips[(size_t) (1 - removed)]; added.id = juce::Uuid().toString();
            added.track = removed; added.pendingPlacement = PendingPlacement::append;
            ui.engine.project.clips.push_back(added);
            const auto result = prepareTempoMap(ui.engine.project, false, 32, 0, 0,
                                                live ? std::optional<double>(2) : std::nullopt);
            double worstBeats = 0; bool pixels = result.error.isEmpty(); int count = 0;
            for (const auto& clip : result.project.clips)
                for (double source = clip.sourceElapsed(clip.firstBeatTime()); source < clip.sourceElapsed(clip.length); source += 60 / clip.media->bpm())
                {
                    const double time = clip.beatTimelineTime(source), beat = result.project.tempo.beatsAt(time);
                    worstBeats = std::max(worstBeats, std::abs(std::remainder(beat, 1))); ++count;
                    for (double zoom : {7.3, 27.01, 81.3})
                        pixels &= Clip::gridPixel(time, zoom) == Clip::gridPixel(result.project.tempo.timeAtBeat(std::round(beat)), zoom);
                }
            const auto name = "remove-" + juce::String(removed) + (live ? "-live" : "-stopped");
            check(pixels && worstBeats < 1.0e-7 && count > 0, name + " every real-source pulse aligns at three zooms; "
                  + juce::String(count) + " pulses, worst error " + juce::String(worstBeats, 9) + " beats: " + result.error);
            ui.engine.project = result.project; ui.timeline.pixelsPerSecond = 27.01; ui.refresh();
            ui.viewport.setViewPosition((int) (ui.engine.project.duration() * 0.6 * 27.01), 0);
            auto image = output.getChildFile(name + ".png").createOutputStream(); juce::PNGImageFormat png;
            check(image && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *image), name + " captures native grids late in the arrangement");
        }
        root->setProperty("scenarios", scenarios);
        check(metadataFile.replaceWithText(juce::JSON::toString(document)), "write removal/replacement source evidence");
        return;
    }
    if (bandControls)
    {
        root->setProperty("purpose", "Independent transition bands, production renders and isolated cold/warm loading profile; no musical-ground-truth claim.");
        const auto began = juce::Time::getMillisecondCounterHiRes();
        auto prepared = prepareTempoMap(input, true, 64, 0, 0, {}, TransitionPreset::alignOnly);
        root->setProperty("firstTempoPreparationMs", juce::Time::getMillisecondCounterHiRes() - began);
        check(prepared.error.isEmpty() && prepared.project.transitionLinks.size() == 1, "prepare original recordings for independent transition bands: " + prepared.error);
        if (prepared.error.isNotEmpty() || prepared.project.transitionLinks.size() != 1) return;
        auto project = prepared.project; const auto id = project.transitionLinks[0].incoming;
        check(project.setTransitionLength(id, 8).wasOk(), "set an eight-bar real transition");
        check(project.setTransitionBand(id, 0, {TransitionBandMode::crossfade, .75, 0, .5}).wasOk()
            && project.setTransitionBand(id, 1, {TransitionBandMode::swapFade, 1, 1, .5}).wasOk()
            && project.setTransitionBand(id, 2, {TransitionBandMode::fadeIn, .25, 0, .5}).wasOk()
            && project.setTransitionBand(id, 3, {TransitionBandMode::fadeOut, .5, 0, .5}).wasOk(), "compose four independent real-track treatments");
        bool stable = true;
        for (size_t i = 0; i < project.clips.size(); ++i) stable &= project.clips[i].start == prepared.project.clips[i].start
            && project.clips[i].length == prepared.project.clips[i].length && project.clips[i].prepared == prepared.project.clips[i].prepared
            && project.clips[i].tempoMap == prepared.project.clips[i].tempoMap;
        check(stable, "real band editing retains exact placements, source clocks and prepared PCM");
        const auto render = [&](const juce::String& name)
        {
            Engine engine; engine.project = project; engine.sampleRate = 44100;
            const auto range = project.transitionRange(project.transitionLinks[0]);
            check(renderWindow(engine, output.getChildFile(name + ".wav"), std::max(0.0, range.getStart() - 1), range.getLength() + 2), "render real independent transition: " + name);
        };
        render("bands-composed");
        check(project.setTransitionBand(id, 1, {TransitionBandMode::swap, 1, 2, .75}).wasOk(), "change only real Bass gap and swap position");
        render("bands-late-bass-gap");
        check(project.setTransitionBand(id, 0, {TransitionBandMode::swap, 1, 4, .5}).wasOk(), "apply a full-strength real Volume swap gap");
        render("bands-volume-gap");
        const auto range = project.transitionRange(project.transitionLinks[0]);
        const auto& clock = project.tempo; const double middle = clock.timeAtBeat((clock.beatsAt(range.getStart()) + clock.beatsAt(range.getEnd())) / 2);
        Engine gap; gap.project = project; gap.sampleRate = 44100;
        juce::AudioBuffer<float> pcm(2, 512); juce::MidiBuffer midi; gap.render(pcm, 512, middle, midi);
        check(pcm.getMagnitude(0, 512) < 1e-7f, "actual real-track mixer is silent inside the full-strength volume gap");
        project.setTransitionBand(id, 0, {TransitionBandMode::crossfade, .5, 0, .5});
        render("bands-half-strength");
        if (args.contains("--transition-rehearsal-only"))
        {
            check(project.storeTransitionAlternative(id, 0).wasOk(), "store real composed transition in A");
            project.applyTransitionPreset(id, TransitionPreset::bassSwap); project.storeTransitionAlternative(id, 1);
            project.setTransitionBand(id, 0, {TransitionBandMode::crossfade, 1, 0, .5}); project.storeTransitionAlternative(id, 2);
            TransitionStyle style; const auto capture = project.captureTransitionStyle(id, style);
            check(capture.wasOk() && project.putTransitionPreset({"Real track crossfade", style}).wasOk(), "save real named transition preset without capturing manual effects");
            const auto unchanged = juce::JSON::toString(projectToJson(project));
            Engine preview; preview.project = project; preview.sampleRate = 44100; preview.position = 7;
            const auto result = preview.prepareTransitionRehearsal(id, std::make_shared<std::atomic<bool>>(false));
            check(result.wasOk(), "production renderer prepares three original-recording rehearsals: " + result.getErrorMessage());
            for (int slot = 0; slot < 3 && result.wasOk(); ++slot)
            {
                check(preview.auditionTransitionAlternative(slot).wasOk(), "audition original recording alternative " + juce::String(slot));
                std::unique_ptr<juce::OutputStream> stream = output.getChildFile("rehearsal-" + juce::String(slot) + ".wav").createOutputStream();
                juce::WavAudioFormat wav; auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(44100).withNumChannels(2).withBitsPerSample(24));
                juce::AudioBuffer<float> monitor(2, 512); float* channels[] {monitor.getWritePointer(0), monitor.getWritePointer(1)};
                bool ok = writer != nullptr;
                for (int block = 0; block < 1000 && ok; ++block) { preview.audioDeviceIOCallbackWithContext(nullptr, 0, channels, 2, 512, {}); ok = writer->writeFromAudioSampleBuffer(monitor, 0, 512); }
                check(ok, "retain actual rehearsal callback PCM for original recordings");
            }
            preview.endTransitionRehearsal();
            check(juce::JSON::toString(projectToJson(preview.project)) == unchanged && preview.position == 7, "real rehearsal ends without modifying mix and returns its original cursor");
            root->setProperty("rehearsal", "A/B/C compared by rendered callback PCM; RMS matching is tested over complete synthetic loops, no listening claimed.");
        }
        check(project.setTransitionLocked(id, true).wasOk() && project.setTransitionBand(id, 3, {}).failed(), "real lock protects composed settings");
        const auto file = output.getChildFile("independent-bands.mixworks");
        Engine saved; saved.project = project;
        const auto saveError = file.exists() ? juce::String("The inspection fixture already exists.") : saved.saveProject(file);
        check(saveError.isEmpty(), "public Save writes a new real transition-band inspection session: " + saveError);
        const auto text = file.loadFileAsString();
        Engine restored; const auto reopenBegan = juce::Time::getMillisecondCounterHiRes(); const auto error = restored.restoreProject(text);
        root->setProperty("warmSessionReopenMs", juce::Time::getMillisecondCounterHiRes() - reopenBegan);
        check(error.isEmpty() && restored.project.transitionLinks == project.transitionLinks, "real public session reopen preserves all composed settings and locks: " + error);
        if (args.contains("--transition-rehearsal-only")) check(restored.project.transitionPresets.size() == 1
            && restored.project.transitionPresets[0].style == project.transitionPresets[0].style, "real public Save/Open retains named presets and all A/B/C alternatives");
        bool cacheHit = error.isEmpty();
        juce::Array<juce::var> reopenedCaches;
        for (const auto& clip : restored.project.clips)
        {
            cacheHit &= clip.media->analysisFromCache && clip.media->decodedFromCache && (!clip.prepared || clip.prepared->fromDiskCache);
            auto* state = new juce::DynamicObject(); state->setProperty("source", clip.media->file.getFullPathName());
            state->setProperty("analysisHit", clip.media->analysisFromCache); state->setProperty("decodedHit", clip.media->decodedFromCache);
            state->setProperty("tempoHit", clip.prepared && clip.prepared->fromDiskCache);
            if (clip.prepared) { state->setProperty("tempoWarning", clip.prepared->cacheWarning); if (clip.prepared->cache) state->setProperty("tempoFile", clip.prepared->cache->file.getFullPathName()); }
            reopenedCaches.add(juce::var(state));
        }
        root->setProperty("reopenedCaches", reopenedCaches);
        check(cacheHit, "real session reopen reuses analysis, decoded audio and applicable tempo caches");
        Workstation ui(false); ui.stopTimer(); ui.setSize(1440, 940); ui.engine.project = project;
        ui.timeline.pixelsPerSecond = 16; ui.selectTransition(id, true);
        auto image = output.getChildFile("independent-real-transition.png").createOutputStream(); juce::PNGImageFormat png;
        check(image && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *image), "capture real composed controls and locked frame");
        check(metadataFile.replaceWithText(juce::JSON::toString(document)), "retain real source identities, load stages and transition measurements");
        return;
    }
    if (args.contains("--transition-controls-only"))
    {
        auto base = prepareTempoMap(input, true, 64, 0, 0, {}, TransitionPreset::alignOnly);
        check(base.error.isEmpty() && base.project.transitionLinks.size() == 1, "prepare real recordings for selected-transition control checks: " + base.error);
        if (base.error.isNotEmpty() || base.project.transitionLinks.empty()) return;
        auto project = base.project; const auto id = project.transitionLinks[0].incoming;
        const auto start = project.transitionRange(project.transitionLinks[0]).getStart();
        check(project.setTransitionLength(id, 8).wasOk(), "resize the selected real transition to eight bars");
        bool stable = true;
        for (size_t i = 0; i < project.clips.size(); ++i) stable &= project.clips[i].start == base.project.clips[i].start
            && project.clips[i].prepared == base.project.clips[i].prepared && project.clips[i].tempoMap == base.project.clips[i].tempoMap;
        check(stable && std::abs(project.transitionRange(project.transitionLinks[0]).getStart() - start) < 1e-7,
            "real local edits retain song placement, source clocks, PCM and the frame start");
        for (const auto preset : {TransitionPreset::bassSwap, TransitionPreset::crossfade, TransitionPreset::alignOnly})
        {
            const auto applied = project.applyTransitionPreset(id, preset);
            check(applied.wasOk(), "apply selected real preset " + juce::String((int) preset) + ": " + applied.getErrorMessage());
            if (applied.failed()) continue;
            Engine rendered; rendered.project = project; rendered.sampleRate = 44100;
            const auto range = project.transitionRange(project.transitionLinks[0]);
            check(renderWindow(rendered, output.getChildFile("selected-preset-" + juce::String((int) preset) + ".wav"), std::max(0.0, range.getStart() - 1), range.getLength() + 2),
                "render selected preset through the actual real-track production mixer");
            check(parseProject(juce::JSON::toString(projectToJson(project))).error.isEmpty(), "real local preset survives session validation");
        }
        check(project.setTransitionLocked(id, true).wasOk(), "lock the selected real transition");
        check(project.setTransitionLength(id, 4).failed() && project.applyTransitionPreset(id, TransitionPreset::bassSwap).failed(), "real lock blocks local length and preset changes");
        check(prepareTempoMap(project, true, 32, 0).error.isNotEmpty(), "real lock prevents a whole-mix rebuild");
        const auto session = output.getChildFile("selected-transition.mixworks");
        check(!session.exists() && session.replaceWithText(juce::JSON::toString(projectToJson(project))), "save the locked real session as a new inspection fixture");
        Workstation ui(false); ui.stopTimer(); ui.setSize(1440, 940); ui.engine.project = project;
        ui.timeline.pixelsPerSecond = 16; ui.selectTransition(id, true);
        auto stream = output.getChildFile("selected-real-transition.png").createOutputStream(); juce::PNGImageFormat png;
        check(stream && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *stream), "capture real selected-transition controls and locked frame");
        check(metadataFile.replaceWithText(juce::JSON::toString(document)), "write selected-transition source evidence");
        return;
    }
    if (args.contains("--transition-box-only"))
    {
        const auto base = prepareTempoMap(input, true, 64, 0, 0, {}, TransitionPreset::bassSwap);
        check(base.error.isEmpty() && base.project.transitionLinks.size() == 1 && base.project.transitionLinks[0].bassSwap.has_value(),
            "real recordings prepare a transition window with paired bass: " + base.error);
        if (base.error.isNotEmpty() || base.project.transitionLinks.empty()) return;
        for (const auto edit : {Project::TransitionEdit::moveWindow, Project::TransitionEdit::resizeStart, Project::TransitionEdit::resizeEnd,
             Project::TransitionEdit::moveOutgoing, Project::TransitionEdit::moveIncoming})
        {
            auto project = base.project; const auto edge = project.transitionLinks[0]; const auto before = project.transitionRange(edge);
            const auto reference = edit == Project::TransitionEdit::resizeEnd ? before.getEnd()
                : edit == Project::TransitionEdit::moveOutgoing ? project.clips[0].start
                : edit == Project::TransitionEdit::moveIncoming ? project.clips[1].start : before.getStart();
            const double target = project.tempo.timeAtBeat(project.tempo.beatsAt(reference) + (edit == Project::TransitionEdit::moveOutgoing ? 4 : -4));
            const auto result = project.editTransition(edge.incoming, edit, target - reference, false);
            const auto range = project.transitionRange(project.transitionLinks[0]); const auto bass = project.transitionLinks[0].bassSwap;
            check(result.wasOk() && bass && std::abs(project.clips[0].start + project.clips[0].localTimeForSource((*bass)[0])
                - project.clips[1].start - project.clips[1].localTimeForSource((*bass)[2])) < 1e-7,
                "real transition action " + juce::String((int) edit) + " preserves paired bass timing: " + result.getErrorMessage());
            if (result.failed()) continue;
            Engine rendered; rendered.project = project; rendered.sampleRate = 44100;
            const auto name = "transition-box-" + juce::String((int) edit);
            check(renderWindow(rendered, output.getChildFile(name + "-start.wav"), std::max(0.0, range.getStart() - 1), 4)
                && renderWindow(rendered, output.getChildFile(name + "-end.wav"), std::max(0.0, range.getEnd() - 2), 4),
                "render real transition start and end through the production mixer");
            check(parseProject(juce::JSON::toString(projectToJson(project))).error.isEmpty(), "real transition edits survive session validation");
            juce::var row(new juce::DynamicObject()); row.getDynamicObject()->setProperty("name", name);
            row.getDynamicObject()->setProperty("begin", range.getStart()); row.getDynamicObject()->setProperty("end", range.getEnd()); scenarios.add(row);
            if (edit == Project::TransitionEdit::moveIncoming)
            {
                Workstation ui(false); ui.stopTimer(); ui.setSize(1440, 940); ui.engine.project = project;
                ui.timeline.pixelsPerSecond = 16; ui.refresh();
                if (auto* viewport = ui.timeline.findParentComponentOfClass<juce::Viewport>()) viewport->setViewPosition((int) ((range.getStart() - 3) * 16), 0);
                auto stream = output.getChildFile("real-transition-box.png").createOutputStream(); juce::PNGImageFormat png;
                check(stream && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *stream), "capture the real-track transition frame");
            }
        }
        root->setProperty("scenarios", scenarios);
        check(metadataFile.replaceWithText(juce::JSON::toString(document)), "write real transition-box source evidence");
        return;
    }
    Project dropped;
    const bool movesOnly = args.contains("--moves-only"), anchorsOnly = args.contains("--anchors-only");
    for (int scenario = args.contains("--analysis-only") ? 9 : args.contains("--workflow-only") || movesOnly || anchorsOnly ? 3 : 0; scenario < 9; ++scenario)
    {
        if ((movesOnly && (scenario == 4 || scenario >= 7)) || (anchorsOnly && scenario > 3 && scenario < 7)) continue;
        const juce::String name = scenario == 8 ? "intro-anchor" : scenario == 7 ? "outro-anchor" : scenario == 6 ? "free-move" : scenario == 5 ? "snapped-move" : scenario == 4 ? "edited-transition" : scenario == 3 ? "dropped-transition" : scenario == 2 ? "native-ramp" : scenario == 0 ? "fixed-120" : "fixed-128";
        MatchResult matched;
        if (scenario == 3)
        {
            auto placed = input;
            for (auto& clip : placed.clips) clip.pendingPlacement = PendingPlacement::append;
            matched = prepareTempoMap(placed, false, 64); dropped = matched.project;
            check(matched.error.isEmpty() && dropped.tempo.points.size() == 2
                  && dropped.tempo.points.front().bpm == placed.clips[0].media->bpm()
                  && dropped.tempo.points[1].bpm == placed.clips[1].media->bpm()
                  && std::abs(dropped.tempo.points[1].time - dropped.clips[1].start - dropped.clips[1].transitionTime(true)) < 1.0e-7,
                  "real automatic insertions align bars and create one editable native-tempo target per track, with the incoming target at IN");
        }
        else if (scenario == 4)
        {
            if (dropped.tempo.points.size() != 2) continue;
            auto edited = dropped;
            edited.tempo.setPoint(std::max(0.0, dropped.tempo.points[1].time - 12), dropped.clips[0].media->bpm());
            edited.tempo.movePoint(2, dropped.tempo.points[1].time + 3, 128);
            matched = prepareTempoMap(edited, false, 64);
            check(matched.error.isEmpty() && matched.project.tempo.points == edited.tempo.points,
                  "both real-track transition points and a 128 BPM target survive audio preparation");
        }
        else if (scenario >= 7)
        {
            if (dropped.clips.size() != 2) continue;
            matched.project = dropped; matched.matched = 2;
            const auto& original = dropped.clips[scenario == 8 ? 1 : 0]; const bool intro = scenario == 8;
            const double cue = original.start + original.transitionTime(intro);
            const double target = dropped.tempo.timeAtBeat(dropped.tempo.beatsAt(cue) + (intro ? 16 : -16));
            const auto began = juce::Time::getMillisecondCounterHiRes();
            const auto result = matched.project.moveTransitionAnchor(original.id, intro, target, true);
            const double elapsed = juce::Time::getMillisecondCounterHiRes() - began;
            matched.error = result.getErrorMessage();
            const auto& outgoing = matched.project.clips[0]; const auto& incoming = matched.project.clips[1];
            check(result.wasOk() && outgoing.start == dropped.clips[0].start
                  && std::abs(outgoing.start + outgoing.transitionTime(false) - incoming.start - incoming.transitionTime(true)) < 1.0e-7
                  && std::abs(incoming.length * incoming.rate - dropped.clips[1].length * dropped.clips[1].rate) < 1.0e-7
                  && matched.project.tempo.points == dropped.tempo.points && elapsed < 50,
                  name + " aligns IN to OUT by repositioning the next track in " + juce::String(elapsed, 3) + " ms: " + matched.error);
        }
        else if (scenario >= 5)
        {
            if (dropped.clips.size() != 2) continue;
            matched.project = dropped; matched.matched = 2;
            double moveMs = 0;
            for (int i = 0; i < 2; ++i)
            {
                const auto id = matched.project.clips[(size_t) i].id;
                const double oldStart = matched.project.clips[(size_t) i].start;
                const double requested = scenario == 5 ? dropped.tempo.timeAtBeat(dropped.tempo.beatsAt(oldStart) + (i == 0 ? 4 : 8)) : oldStart + (i == 0 ? 0.137 : 3.173);
                const auto began = juce::Time::getMillisecondCounterHiRes();
                const auto moved = matched.project.moveClip(id, requested, scenario == 5);
                moveMs += juce::Time::getMillisecondCounterHiRes() - began;
                const auto& clip = *matched.project.clipFor(id);
                check(moved.wasOk() && clip.elasticPlayback && clip.elasticSource && clip.media == dropped.clips[(size_t) i].media
                      && std::abs(clip.length * clip.rate - dropped.clips[(size_t) i].length * dropped.clips[(size_t) i].rate) < 1.0e-7,
                      name + " moves source " + juce::String(i) + " without analysis or a whole-track render");
                if (scenario == 6) check(std::abs(clip.start - requested) < 1.0e-8, "free move retains exact requested position for source " + juce::String(i));
            }
            check(matched.project.tempo.points == dropped.tempo.points && moveMs < 50,
                  name + " preserves the shared tempo controls; both placements took " + juce::String(moveMs, 3) + " ms");
        }
        else matched = scenario == 2 ? prepareTempoMap(input, true, 64) : prepareBeatmatch(input, scenario == 0 ? 120 : 128, true, 64);
        check(matched.error.isEmpty() && matched.matched == 2, name + " prepares both real tracks: " + matched.error);
        if (matched.error.isNotEmpty() || matched.matched != 2) continue;
        if ((movesOnly && scenario < 5) || (anchorsOnly && scenario < 7)) continue;
        if (scenario >= 3 && scenario != 6) for (const auto& clip : matched.project.clips)
        {
            check(std::abs(std::remainder(matched.project.tempo.beatsAt(clip.start + clip.firstBeatTime()), 1)) < 1.0e-7,
                  name + " keeps source " + juce::String(clip.track) + " on the shared pulse grid");
            check(clip.firstDownbeatTime() >= 0 && std::abs(std::remainder(matched.project.tempo.beatsAt(clip.start + clip.firstDownbeatTime()), 4)) < 1.0e-7,
                  name + " aligns source " + juce::String(clip.track) + " bar one to the shared changing-tempo grid");
        }
        juce::var result(new juce::DynamicObject()); auto* row = result.getDynamicObject(); row->setProperty("name", name);
        juce::Array<juce::var> clips, tempo;
        for (const auto& point : matched.project.tempo.points)
        {
            juce::var value(new juce::DynamicObject()); value.getDynamicObject()->setProperty("time", point.time);
            value.getDynamicObject()->setProperty("bpm", point.bpm); value.getDynamicObject()->setProperty("ramp", point.ramp); tempo.add(value);
        }
        row->setProperty("tempo", tempo); row->setProperty("bpm", matched.project.bpm);
        const double begin = matched.project.clips[1].start + matched.project.clips[1].envelope().intro;
        const double end = matched.project.clips[0].start + matched.project.clips[0].envelope().outro + matched.project.clips[0].fadeOut;
        if (scenario >= 3)
        {
            Workstation ui(false); ui.setSize(1440, 940); ui.engine.project = matched.project;
            for (const auto& clip : input.clips) ui.engine.library.push_back(clip.media);
            ui.timeline.pixelsPerSecond = 16; ui.refresh();
            if (auto* viewport = ui.timeline.findParentComponentOfClass<juce::Viewport>())
                viewport->setViewPosition((int) ((begin - 5) * ui.timeline.pixelsPerSecond), 0);
            auto stream = output.getChildFile(name + ".png").createOutputStream(); juce::PNGImageFormat png;
            check(stream && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *stream),
                  name + " captures the real timeline, beat markers and two tempo controls");
            if (scenario == 3 || scenario == 7)
            {
                auto* viewport = ui.timeline.findParentComponentOfClass<juce::Viewport>();
                ui.timeline.pixelsPerSecond = (viewport->getWidth() - 24) / ui.engine.project.duration(); ui.refresh(); viewport->setViewPosition(0, 0);
                auto overview = output.getChildFile(name + "-overview.png").createOutputStream();
                check(overview && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *overview), name + " captures both full energy contours against the four-bar grid");
            }
        }
        row->setProperty("overlapStart", begin); row->setProperty("overlapEnd", end);
        Engine engine; engine.sampleRate = 44100; engine.project = matched.project;
        engine.project.masterDb = -6; // Listening mix has headroom; measurements use isolated stems.
        if (scenario == 3 || scenario == 4)
        {
            const auto session = output.getChildFile(name + ".mixworks");
            const auto saveError = engine.saveProject(session);
            Engine reopened; reopened.sampleRate = 44100;
            const auto openError = saveError.isEmpty() ? reopened.restoreProject(session.loadFileAsString()) : saveError;
            bool geometry = openError.isEmpty() && reopened.project.clips.size() == engine.project.clips.size()
                && reopened.project.tempo.points == engine.project.tempo.points && reopened.project.transitionLinks == engine.project.transitionLinks;
            for (size_t i = 0; geometry && i < engine.project.clips.size(); ++i)
            {
                const auto& before = engine.project.clips[i]; const auto& after = reopened.project.clips[i];
                geometry &= before.start == after.start && before.length == after.length && before.rate == after.rate
                    && before.tempoSourceBpm == after.tempoSourceBpm && before.offset == after.offset;
                for (double local : {0.0, before.length * 0.1, before.length * 0.5, before.length * 0.9})
                    geometry &= std::abs(before.sourceElapsed(local) - after.sourceElapsed(local)) < 1.0e-9;
            }
            check(geometry, name + " real source files save/reopen with identical placement, tempo and source clocks: " + openError);
            double difference = 0;
            if (geometry)
            {
                juce::MidiBuffer midi; juce::AudioBuffer<float> originalPcm(2, 512), reopenedPcm(2, 512);
                for (const auto& clip : engine.project.clips) for (double fraction : {0.1, 0.5, 0.9})
                {
                    const double position = clip.start + clip.length * fraction;
                    engine.render(originalPcm, 512, position, midi); reopened.render(reopenedPcm, 512, position, midi);
                    for (int ch = 0; ch < 2; ++ch) for (int frame = 0; frame < 512; ++frame)
                        difference = std::max(difference, (double) std::abs(originalPcm.getSample(ch, frame) - reopenedPcm.getSample(ch, frame)));
                }
            }
            check(geometry && difference < 1.0e-6, name + " reopened real-audio playback matches before saving; maximum sample difference " + juce::String(difference, 9));
            auto moved = reopened.project;
            const auto movedResult = geometry ? moved.moveClip(moved.clips.back().id, moved.clips.back().start + 4, true) : juce::Result::fail("Session did not reopen.");
            bool linked = movedResult.wasOk() && !moved.transitionLinks.empty();
            for (const auto& link : moved.transitionLinks)
                linked &= std::abs(moved.clipFor(link.outgoing)->start + moved.clipFor(link.outgoing)->transitionTime(false)
                    - moved.clipFor(link.incoming)->start - moved.clipFor(link.incoming)->transitionTime(true)) < 1.0e-7;
            check(linked, name + " reopened real tracks still move together through their saved IN/OUT relationship");
        }
        const double listenBegin = std::max(0.0, begin - 4), listenEnd = std::min(engine.project.duration(), end + 4);
        row->setProperty("listeningStart", listenBegin);
        const auto mixFile = output.getChildFile(name + "-mix.wav"); row->setProperty("mixWav", mixFile.getFileName());
        check(renderWindow(engine, mixFile, listenBegin, listenEnd - listenBegin), name + " renders its transition through the mixer");
        engine.project.masterDb = 0;
        for (int track = 0; track < 2; ++track)
        {
            for (int i = 0; i < trackCount; ++i) engine.project.tracks[(size_t) i].mute = i != track;
            auto& clip = engine.project.clips[(size_t) track]; clip.fadeIn = clip.fadeOut = 0;
            clip.fadeInAnchor.reset(); clip.fadeOutAnchor.reset(); // Full unattenuated source for independent timing measurement.
            const auto stem = output.getChildFile(name + "-track-" + juce::String(track) + ".wav");
            check(renderWindow(engine, stem, clip.start, clip.length), name + " renders isolated stem " + juce::String(track));
            juce::var detail(new juce::DynamicObject()); auto* item = detail.getDynamicObject();
            item->setProperty("track", track); item->setProperty("start", clip.start); item->setProperty("length", clip.length);
            item->setProperty("wav", stem.getFileName()); item->setProperty("firstBeatTime", clip.firstBeatTime());
            item->setProperty("firstDownbeatTime", clip.firstDownbeatTime());
            juce::Array<juce::var> beats;
            const auto& media = *clip.media;
            for (int n = 0; media.analysis.bpm > 0 && media.beatOrigin() + n * 60 / media.bpm() < media.duration(); ++n)
            {
                const double source = media.beatOrigin() + n * 60 / media.bpm();
                juce::var beat(new juce::DynamicObject()); auto* point = beat.getDynamicObject();
                point->setProperty("ordinal", n); point->setProperty("source", source);
                point->setProperty("local", clip.localTimeForSource(source)); beats.add(beat);
            }
            item->setProperty("predictedBeats", beats); clips.add(detail);
        }
        row->setProperty("clips", clips); scenarios.add(result);
    }
    root->setProperty("scenarios", scenarios);
    check(metadataFile.replaceWithText(juce::JSON::toString(document)), "write independent measurement inputs");
}
}
