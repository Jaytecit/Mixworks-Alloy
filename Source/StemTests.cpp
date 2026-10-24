#include "StemJobs.h"
#include "AnalysisCache.h"
#include "Workstation.h"
#include "Beatmatch.h"
#include "AutoArrangement.h"
#include <cmath>
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace mixworks
{
void runStemTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    const auto root = report.getParentDirectory().getChildFile("stem-tests"); root.createDirectory();
    const auto analysis = root.getChildFile("analysis.json"); analysis.replaceWithText("preserve reviewed grid");
    const auto source = root.getChildFile("source.wav"); source.replaceWithText("owned invalid audio");
    auto awaitJob = [](StemJob& job)
    {
        const auto until = juce::Time::getMillisecondCounterHiRes() + 10000;
        while (job.state().running && juce::Time::getMillisecondCounterHiRes() < until) juce::Thread::sleep(10);
        return job.state();
    };
    StemJob missing(source, root.getChildFile("cache"), root.getChildFile("missing-runtime.json"));
    missing.begin(); const auto failed = awaitJob(missing);
    check(!failed.running && !failed.ready && failed.directory == juce::File() && failed.error.isNotEmpty(), "stem job refuses missing runtime without a Ready result");
    check(analysis.loadFileAsString() == "preserve reviewed grid" && source.loadFileAsString() == "owned invalid audio", "failed separation preserves source and existing analysis");
    check(!root.getChildFile("cache/jobs").exists(), "runtime failure creates no partial audio job");
    check(StemJob::defaultRoot() == analysisStoreRoot().getSiblingFile("Stems-v1") && StemJob::defaultRoot() != analysisStoreRoot(), "stem cache is separate and inherits test/bridge isolation");
    StemJob cancelled(source, root.getChildFile("cancel-cache"), root.getChildFile("missing-runtime.json"));
    cancelled.begin(); cancelled.cancel(); const auto stopped = awaitJob(cancelled);
    check(!stopped.running && !stopped.ready, "cancelled or failed job cannot publish a ready result");

    auto parent = std::make_shared<Media>(); parent->name = "Source-aligned stem fixture";
    parent->file = root.getChildFile("original.wav"); parent->sampleRate = 8000;
    parent->manualBpm = 120; parent->manualBeatOrigin = .125; parent->manualKey = 3;
    parent->analysis.bpm = 120; parent->analysis.tempoConfidence = .9; parent->analysis.downbeatOrigin = .625;
    parent->samples.setSize(2, 96000); parent->samples.clear();
    auto assets = std::make_shared<StemAssets>(); assets->directory = root.getChildFile("ready"); assets->directory.createDirectory();
    auto writeWave = [&](const std::shared_ptr<Media>& media)
    {
        juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream = media->file.createOutputStream();
        auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(2).withBitsPerSample(24));
        return writer && writer->writeFromAudioSampleBuffer(media->samples, 0, media->frames());
    };
    auto* manifest = new juce::DynamicObject(); auto* entries = new juce::DynamicObject();
    for (int s = 0; s < stemCount; ++s)
    {
        auto m = std::make_shared<Media>(); m->sampleRate = 8000; m->file = assets->directory.getChildFile(juce::String(stemNames[s]) + ".wav");
        m->samples.setSize(2, 96000);
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < m->frames(); ++i)
        {
            const float value = .0125f * (s + 1) + (i % (4000 + 100 * s) < 20 ? .1f : 0.f);
            m->samples.setSample(ch, i, value); parent->samples.addSample(ch, i, value);
        }
        check(writeWave(m), "write owned " + juce::String(stemNames[s]) + " fixture");
        auto* entry = new juce::DynamicObject(); entry->setProperty("sha256", audioDigest(m->file)); entries->setProperty(stemNames[s], juce::var(entry));
        assets->sources[(size_t) s] = m;
    }
    check(writeWave(parent), "write owned original stem source");
    parent->analysisSource = {audioDigest(parent->file), parent->file.getSize(), 8000, 96000, 2}; parent->prepareWaveformBody();
    manifest->setProperty("schema", 1); manifest->setProperty("offsetSamples", 0); manifest->setProperty("frames", 96000);
    manifest->setProperty("channels", 2); manifest->setProperty("sampleRate", 8000); manifest->setProperty("sourceSha256", parent->analysisSource.sha256);
    manifest->setProperty("stems", juce::var(entries)); const auto manifestFile = assets->directory.getChildFile("manifest.json");
    manifestFile.replaceWithText(juce::JSON::toString(juce::var(manifest)));
    std::shared_ptr<const StemAssets> loaded;
    const auto load = loadStemAssets(*parent, assets->directory, {}, loaded);
    check(load.wasOk() && loaded, "load validated stem set without analysing the four files: " + load.getErrorMessage());
    if (!loaded) return;
    // Re-adding a previously separated source must discover the completed set,
    // including in a fresh Workstation with no previous in-memory stem job.
    const auto cachedStems = StemJob::defaultRoot().getChildFile(juce::String::repeatedString("a",64));
    check(assets->directory.copyDirectoryTo(cachedStems), "publish owned completed extraction fixture");
    {
        Workstation reload(false); reload.engine.library = {parent}; reload.addMedia(0);
        const auto deadline = juce::Time::getMillisecondCounterHiRes()+30000;
        while ((reload.busy || !reload.timelineLoads.empty() || reload.engine.project.hasPendingPlacement())
            && juce::Time::getMillisecondCounterHiRes() < deadline)
        {
#if JUCE_WINDOWS
            MSG message; while (PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
#endif
            juce::Thread::sleep(2);
        }
        check(!reload.busy && reload.engine.project.clips.size() == 1 && reload.engine.project.clips[0].stems
            && reload.engine.project.clips[0].stemDirectory == cachedStems,
            "re-adding a song restores its completed stems without a separation runtime");
    }
    const auto originalHash = audioDigest(parent->file);
    Clip c; c.media = parent; c.length = 10; c.offset = 1; c.start = 2; c.audioSlip = .05;
    c.stems = loaded; c.stemDirectory = loaded->directory; c.stemManifestHash = loaded->manifestHash;
    Engine engine; engine.project.clips = {c}; engine.project.masterDb = 0; engine.project.followTrackTempo = false;
    juce::AudioBuffer<float> sum(2, 512), original(2, 512); juce::MidiBuffer events;
    engine.render(sum, 512, 3, events); engine.project.clips[0].stemsEnabled = false;
    engine.render(original, 512, 3, events);
    float difference = 0; for (int i = 0; i < 512; ++i) difference = std::max(difference, std::abs(sum.getSample(0, i) - original.getSample(0, i)));
    check(difference < 1e-6, "stem sum uses original offset/slip/sample clock and replaces rather than doubles original audio");
    for (int mode = 0; mode < 3; ++mode)
    {
        auto variant = c; variant.reversed = mode != 0; variant.loopSpan = mode == 1 ? 0 : 2;
        variant.loopPhase = variant.loopSpan > 0 ? .25 : 0; engine.project.clips[0] = variant;
        engine.render(sum, 512, 4.5, events); engine.project.clips[0].stemsEnabled = false; engine.render(original, 512, 4.5, events);
        difference = 0; for (int i = 0; i < 512; ++i) difference = std::max(difference, std::abs(sum.getSample(0,i) - original.getSample(0,i)));
        check(difference < 1e-6, "stem sum matches parent reverse/loop mapping " + juce::String(mode));
    }
    engine.project.clips[0] = c;
    // A stem solo must isolate the whole timeline, including overlapping
    // original-only sections on the same lane and stem mixes on other lanes.
    for (int selected = 0; selected < stemCount; ++selected)
    {
        auto isolated = c; isolated.stemControls[(size_t) selected].solo = true;
        Engine expected; expected.project = engine.project; expected.project.clips = {isolated};
        auto sameLane = c; sameLane.id = juce::Uuid().toString(); sameLane.stems.reset();
        auto otherLane = c; otherLane.id = juce::Uuid().toString(); otherLane.track = 1;
        engine.project.clips = {isolated, sameLane, otherLane};
        engine.render(sum, 512, 3, events); expected.render(original, 512, 3, events);
        difference = 0;
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 512; ++i)
            difference = std::max(difference, std::abs(sum.getSample(ch,i) - original.getSample(ch,i)));
        check(difference < 1e-6 && sum.getMagnitude(0,512) > 0,
            "whole-timeline solo isolates " + juce::String(stemNames[selected]) + " from same/other-lane sections");
        engine.project.tracks[1].solo = true;
        engine.render(sum,512,3,events);
        difference = 0;
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 512; ++i)
            difference = std::max(difference,std::abs(sum.getSample(ch,i)-original.getSample(ch,i)));
        check(difference < 1e-6, "stem solo takes precedence over another lane's solo: " + juce::String(stemNames[selected]));
        engine.project.tracks[1].solo = false;
        engine.project.clips[0].start = 20;
        engine.render(sum,512,3,events);
        check(sum.getMagnitude(0,512) == 0, "other sections stay silent outside soloed stem's time span");
        engine.project.clips[0].stemsEnabled = false;
        expected.project.clips = {sameLane,otherLane};
        engine.render(sum,512,3,events); expected.render(original,512,3,events);
        difference = 0;
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 512; ++i)
            difference = std::max(difference,std::abs(sum.getSample(ch,i)-original.getSample(ch,i)));
        check(difference < 1e-6 && sum.getMagnitude(0,512) > 0, "original-only playback ignores dormant stem solo");
        engine.project.clips[0].stemsEnabled = true;
        engine.project.clips[0].pendingPlacement = PendingPlacement::move;
        engine.render(sum,512,3,events);
        difference = 0;
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 512; ++i)
            difference = std::max(difference,std::abs(sum.getSample(ch,i)-original.getSample(ch,i)));
        check(difference < 1e-6, "pending stem placement cannot silence the ready timeline");
    }
    engine.project.clips = {c};
    engine.project.clips[0].stemsEnabled = true;
    for (auto& controls : engine.project.clips[0].stemControls) controls.mute = true;
    engine.render(sum, 512, 3, events); check(sum.getMagnitude(0, 512) == 0, "muting every stem is silent");
    auto& controls = engine.project.clips[0].stemControls[0]; controls.mute = false; controls.solo = true; controls.pan = -1;
    controls.automation[0].enabled = true; controls.automation[0].setPoint(0, -12); controls.automation[0].setPoint(10, 0);
    engine.render(sum, 512, 3, events);
    check(sum.getMagnitude(1, 0, 512) == 0 && sum.getMagnitude(0, 0, 512) > 0, "stem solo, volume automation and pan reach actual renderer");
    const AutomationAddress address {0, AutomationTarget::volume, {}, {}, c.id, 0};
    const auto slice = engine.project.clips[0].slice(1, 4);
    check(std::abs(slice.stemControls[0].automation[0].valueAt(.5, 0) - controls.automation[0].valueAt(1.5, 0)) < 1e-6
        && slice.stems == loaded && slice.media == parent, "slicing stems preserves parent source and slices their automation once");
    check(engine.project.curve(address) == &controls.automation[0] && engine.project.curve({0, AutomationTarget::volume, {}, {}, c.id}) != engine.project.curve(address), "stem address is independent from parent automation");
    const auto saved = juce::JSON::toString(engine.captureProject(), true, 17);
    auto parsed = parseProject(saved);
    check(parsed.error.isEmpty() && parsed.clips[0].clip.stemControls[0].solo && parsed.clips[0].clip.stemControls[0].automation[0].points.size() == 2, "session 37 retains stem identity, solo and authored curves: " + parsed.error);
    Engine reopened; const auto reopen = reopened.restoreProject(saved);
    check(reopen.isEmpty() && reopened.project.clips[0].stems && reopened.project.clips[0].media->manualBeatOrigin == .125, "actual save/open validates stems and preserves reviewed downbeat: " + reopen);
    if (reopen.isEmpty())
    {
        reopened.render(original, 512, 3, events); difference = 0;
        for (int i = 0; i < 512; ++i) difference = std::max(difference, std::abs(sum.getSample(0, i) - original.getSample(0, i)));
        check(difference < 1e-6, "saved stem mix renders the same audio after reopen");
    }
    auto invalid = juce::JSON::parse(saved); invalid["clips"][0]["stems"].getDynamicObject()->setProperty("manifestHash", juce::String::repeatedString("0", 64));
    const auto before = juce::JSON::toString(reopened.captureProject(), true, 17);
    check(reopened.restoreProject(juce::JSON::toString(invalid)).isNotEmpty() && before == juce::JSON::toString(reopened.captureProject(), true, 17), "changed stem identity rejects Open atomically");
    auto rateProject = engine.project; rateProject.clips[0].rate = 1.1; rateProject.clips[0].length /= 1.1;
    const auto prepared = prepareSavedClips(rateProject);
    check(prepared.isEmpty() && rateProject.clips[0].stemPrepared[0] && rateProject.clips[0].stemPrepared[0]->frames() == rateProject.clips[0].prepared->frames(), "fixed-tempo preparation uses identical stem sample dimensions: " + prepared);
    auto ramp = engine.project; ramp.followTrackTempo = true; ramp.tempo = {120, {{0,120,true},{12,128,false}}};
    ramp.clips[0].beatmatched = true; auto mapped = prepareTempoMap(ramp, false, 0);
    check(mapped.error.isEmpty() && mapped.project.clips[0].stemPrepared[0] && mapped.project.clips[0].stemPrepared[0]->clipLocal, "tempo-ramp stem preparation shares parent geometry: " + mapped.error);
    if (mapped.error.isEmpty())
    {
        auto& clip = mapped.project.clips[0]; clip.moveTo(3.25);
        Engine live; live.project = mapped.project; live.render(sum, 512, 4.5, events);
        check(clip.elasticPlayback && clip.elasticSource && sum.getMagnitude(0,0,512) > 0 && sum.getMagnitude(1,0,512) == 0, "moved elastic stem mix retains audible solo/pan and source clock");
    }
    Project ordered; auto a = c, b = c, d = c; b.id = juce::Uuid().toString(); d.id = juce::Uuid().toString();
    a.start = 30; b.start = 0; d.start = 60; b.track = 1; d.track = 2; ordered.clips = {a,d,b};
    const auto mixed = prepareOrderedTimeline(ordered);
    check(mixed.error.isEmpty() && mixed.project.clips.size() == 3 && mixed.project.clips[0].id == b.id && mixed.project.clips[1].id == a.id && mixed.project.clips[2].id == d.id,
        "Mix timeline preserves chronological order rather than insertion-vector order: " + mixed.error);
    if (mixed.error.isEmpty())
    {
        check(mixed.project.transitionLinks.size() == 2 && mixed.project.clips[1].controls.automation[0].active(), "short tracks fall back to editable crossfades");
        auto locked = mixed.project; locked.transitionLinks[0].locked = true;
        check(prepareOrderedTimeline(locked).error.isNotEmpty(), "Mix timeline refuses locked transitions");
    }
    check(parent->manualBeatOrigin == .125 && parent->analysis.downbeatOrigin == .625 && audioDigest(parent->file) == originalHash, "all stem and ordered mixing operations preserve reviewed/detected downbeats and original bytes");
    Media guess; guess.analysis.bpm = 120; guess.analysis.keyGuess = 7; guess.analysis.downbeatGuess = 1.5;
    check(guess.key() == 7 && guess.grid().downbeatOrigin == 1.5 && !guess.keyConfirmed(), "best guesses are available without being labelled confirmed");
    guess.manualKey = 3; guess.manualBeatOrigin = .125;
    check(guess.key() == 3 && guess.downbeatOrigin() == .125, "manual reviews override every best guess");
    auto record = parent->analysisRecord(); record.analysis.keyGuess = 7; record.analysis.downbeatGuess = 1.5;
    record.peaks.assign(4000, .2f);
    const auto cachedGuess = parseAnalysisRecord(analysisRecordToJson(record), record.source);
    check(cachedGuess.record && cachedGuess.reusableAutomatic && cachedGuess.record->analysis.keyGuess == 7
        && cachedGuess.record->analysis.downbeatGuess == 1.5 && cachedGuess.record->manualBeatOrigin == .125,
        "optional guesses round-trip in compatible analysis without changing manual reviews");
    {
        Workstation attach(false); auto originalClip = c; originalClip.stems.reset(); originalClip.stemDirectory = {}; originalClip.stemManifestHash.clear();
        attach.engine.project.clips = {originalClip}; attach.engine.library = {parent}; attach.refresh();
        const auto history = attach.undoHistory.size(); attach.attachStems(parent->id, loaded->directory);
        const auto deadline = juce::Time::getMillisecondCounterHiRes() + 30000;
        while (attach.busy && juce::Time::getMillisecondCounterHiRes() < deadline)
        {
#if JUCE_WINDOWS
            MSG message; while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
#endif
            juce::Thread::sleep(2);
        }
        check(!attach.busy && attach.engine.project.clips[0].stems && attach.undoHistory.size() == history + 1
            && attach.engine.project.clips[0].start == c.start && attach.engine.project.clips[0].offset == c.offset
            && attach.engine.project.clips[0].media->manualBeatOrigin == .125,
            "native Use on timeline attaches asynchronously with one Undo and unchanged parent geometry: " + attach.status);
        if (!attach.busy)
        {
            attach.undo(false); check(!attach.engine.project.clips[0].stems, "Undo restores original-only playback after attaching stems");
            attach.undo(true); check(attach.engine.project.clips[0].stems != nullptr, "Redo restores validated stem ownership");
        }
    }
    {
        Workstation ui(false); ui.engine.project = engine.project; ui.engine.library = {parent}; ui.selectedClip = c.id; ui.refresh(); ui.setSize(1920,1080); ui.timeline.pixelsPerSecond = 50; ui.resized();
        check(ui.consoleCollapsed && ui.layout.performance.getHeight() == 32 && !ui.performancePanel.isVisible(), "lower console defaults to headers only");
        check(ui.timeline.laneSize(0) >= 224, "expanded stems have readable row height");
        ui.toggleConsole(0); check(!ui.consoleCollapsed && ui.performancePanel.isVisible(), "console header opens the selected panel");
        ui.toggleConsole(0); check(ui.consoleCollapsed && !ui.performancePanel.isVisible(), "repeated console header collapses its panel");
        ui.selectedStem = 0; ui.selectedAutomationTarget = 0; ui.refresh();
        check(ui.displayedAddress(0) == address && ui.insertAutomationNode(address, 4, false), "stem automation uses native timeline node editor");
        ui.undo(false); check(ui.engine.project.clips[0].stemControls[0].automation[0].points.size() == 2, "Undo restores stem curve independently");
        const auto beforeSolo = ui.engine.project;
        auto neighbour = c; neighbour.id = juce::Uuid().toString(); neighbour.track = 1;
        ui.engine.project.clips = {c,neighbour}; ui.engine.project.clips[0].stemsEnabled = false;
        for (auto& control : ui.engine.project.clips[0].stemControls) control.mute = true;
        ui.refresh();
        const auto soloBefore = juce::JSON::toString(ui.engine.captureProject(),true,17);
        const auto mouse = juce::Desktop::getInstance().getMainMouseSource();
        auto clickSolo = [&](int clipIndex, int stem)
        {
            const auto bounds = ui.timeline.clipBounds(ui.engine.project.clips[(size_t) clipIndex]);
            const float left = std::max(bounds.getX(),(float) ui.viewport.getViewPositionX());
            const juce::Point<float> position {left+139,bounds.getY()+43+stem*(bounds.getHeight()-47)/stemCount+10};
            const juce::MouseEvent event(mouse,position,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,
                &ui.timeline,&ui.timeline,juce::Time::getCurrentTime(),position,juce::Time::getCurrentTime(),1,false);
            ui.timeline.mouseDown(event); ui.timeline.mouseUp(event);
        };
        auto soloCount = [&]
        {
            int count = 0; for (const auto& clip : ui.engine.project.clips) for (const auto& control : clip.stemControls) if (control.solo) ++count;
            return count;
        };
        auto history = ui.undoHistory.size(); clickSolo(0,0);
        check(soloCount() == 1 && ui.engine.project.clips[0].stemsEnabled && !ui.engine.project.clips[0].stemControls[0].mute
            && ui.undoHistory.size() == history+1, "native Solo button enables a muted stem in original mode with one Undo");
        ui.undo(false);
        check(juce::JSON::toString(ui.engine.captureProject(),true,17) == soloBefore, "Undo restores original mode and every mute/solo flag");
        ui.undo(true);
        check(soloCount() == 1 && ui.engine.project.clips[0].stemControls[0].solo, "Redo restores whole-timeline stem solo");
        for (int selected = 1; selected < stemCount; ++selected)
        {
            clickSolo(0,selected);
            check(soloCount() == 1 && ui.engine.project.clips[0].stemControls[(size_t) selected].solo
                && !ui.engine.project.clips[0].stemControls[(size_t) selected].mute,
                "native Solo switches exclusively to " + juce::String(stemNames[selected]));
        }
        clickSolo(1,1);
        check(soloCount() == 1 && ui.engine.project.clips[1].stemControls[1].solo, "native Solo switches exclusively across sections");
        const auto soloSaved = juce::JSON::toString(ui.engine.captureProject(),true,17);
        Engine soloReopened; const auto soloOpen = soloReopened.restoreProject(soloSaved);
        ui.engine.render(sum,512,3,events); soloReopened.render(original,512,3,events);
        difference = 0;
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 512; ++i)
            difference = std::max(difference,std::abs(sum.getSample(ch,i)-original.getSample(ch,i)));
        check(soloOpen.isEmpty() && difference < 1e-6 && sum.getMagnitude(0,512) > 0, "saved multi-section stem solo reopens with identical isolated audio");
        juce::String exportError; auto exportRenderer = ui.engine.createExportRenderer(exportError);
        if (exportRenderer) exportRenderer->render(original,512,3,events);
        difference = 0;
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 512; ++i)
            difference = std::max(difference,std::abs(sum.getSample(ch,i)-original.getSample(ch,i)));
        check(exportRenderer && exportError.isEmpty() && difference < 1e-6, "offline export uses the same whole-timeline stem isolation");
        clickSolo(1,1); check(soloCount() == 0, "clicking active Solo restores the timeline mix");
        ui.busy = true; history = ui.undoHistory.size(); clickSolo(0,0); ui.busy = false;
        check(soloCount() == 0 && ui.undoHistory.size() == history, "busy timeline refuses stem Solo without history changes");
        ui.engine.exporting = true; clickSolo(0,0); ui.engine.exporting = false;
        check(soloCount() == 0 && ui.undoHistory.size() == history, "exporting timeline refuses stem Solo");
        if (mixed.error.isEmpty() && !mixed.project.transitionLinks.empty())
        {
            ui.engine.project = mixed.project; ui.engine.project.transitionLinks[0].locked = true;
            const auto lockedId = ui.engine.project.transitionLinks[0].incoming;
            ui.toggleStemSolo(lockedId,0);
            check(soloCount() == 0 && ui.undoHistory.size() == history, "locked transition refuses stem Solo");
        }
        ui.engine.project = beforeSolo; ui.selectedClip = c.id; ui.refresh();
        juce::PNGImageFormat png;
        for (int width : {1920,1250})
        {
            ui.setSize(width, width == 1920 ? 1080 : 800); ui.resized();
            auto stream = report.getSiblingFile("stem-timeline-" + juce::String(width) + ".png").createOutputStream();
            check(stream && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *stream), "capture actual stem timeline at width " + juce::String(width));
        }
    }
}
void runStemIntegration(const std::function<void(bool, const juce::String&)>& check, const juce::File& report, const juce::StringArray& args)
{
    auto argument = [&](const juce::String& key) { const int i = args.indexOf(key); return i >= 0 && i + 1 < args.size() ? juce::File(args[i+1].unquoted()) : juce::File(); };
    const auto runtime = argument("--stem-runtime"), input = argument("--stem-source");
    if (runtime == juce::File() || input == juce::File()) return;
    const auto root = report.getParentDirectory();
    const auto source = root.getChildFile("stem-original" + input.getFileExtension());
    check(input.copyFileTo(source), "copy disposable stem probe source");
    StemJob::defaultRuntime().getParentDirectory().createDirectory();
    check(runtime.copyFileTo(StemJob::defaultRuntime()), "install runtime config in isolated test root");
    const auto reference = analysisStoreRoot().getSiblingFile("DownbeatReferences-v1/review.json");
    reference.getParentDirectory().createDirectory(); reference.replaceWithText("reviewed source downbeat 1.25");
    const auto sourceBefore = audioDigest(source);
    {
        StemJob cancelled(source, root.getChildFile("cancelled-stems"), runtime); cancelled.begin();
        const auto deadline = juce::Time::getMillisecondCounterHiRes() + 120000;
        while (cancelled.state().running && !cancelled.state().message.startsWith("Separating") && juce::Time::getMillisecondCounterHiRes() < deadline) juce::Thread::sleep(25);
        check(cancelled.state().running && cancelled.state().message.startsWith("Separating"), "real worker reaches inference before cancellation");
        cancelled.cancel();
        while (cancelled.state().running && juce::Time::getMillisecondCounterHiRes() < deadline) juce::Thread::sleep(25);
        check(!cancelled.state().running && !cancelled.state().ready, "real inference cancellation reaps worker without publication");
        juce::Array<juce::File> manifests; root.getChildFile("cancelled-stems").findChildFiles(manifests, juce::File::findFiles, true, "manifest.json");
        check(manifests.isEmpty(), "cancelled job leaves no ready manifest");
    }
    auto desk = std::make_unique<Workstation>(false); desk->setSize(1250, 800);
    auto media = std::make_shared<Media>(); media->file = source; media->name = "Stem alignment probe";
    media->manualBpm = 128; media->manualBeatOrigin = 1.25;
    desk->engine.library.push_back(media); desk->refresh();
    const auto projectBefore = juce::JSON::toString(projectToJson(desk->engine.project));
    const auto menu = desk->libraryTable.beatmatchMenu(0);
    bool split = false; for (juce::PopupMenu::MenuItemIterator i(menu, true); i.next();) if (i.getItem().itemID == 8600) split = true;
    check(split, "native library context menu exposes Split track");
    desk->splitStems(media->id);
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + 240000;
    bool captured = false;
    std::function<void()> poll;
    poll = [&]
    {
        const auto state = desk->stemJob->state();
        if ((!captured && state.message == "Separating") || !state.running)
        {
            for (int i = 0; i < juce::Desktop::getInstance().getNumComponents(); ++i)
                if (auto* dialog = dynamic_cast<juce::DialogWindow*>(juce::Desktop::getInstance().getComponent(i)))
                    if (dialog->getName() == "Split track")
                    {
                        const auto file = root.getChildFile(state.running ? "stems-progress.png" : "stems-ready.png");
                        auto stream = file.createOutputStream(); juce::PNGImageFormat png;
                        check(stream && png.writeImageToStream(dialog->createComponentSnapshot(dialog->getLocalBounds()), *stream), "capture actual stem dialog " + state.message);
                        if (!state.running) dialog->exitModalState(0);
                    }
            captured = true;
        }
        if (!state.running || juce::Time::getMillisecondCounterHiRes() > deadline)
        {
            check(state.ready, "real native separation publishes validated stems: " + state.message + " " + state.error);
            if (state.ready) report.getSiblingFile("stem-result.json").replaceWithText(state.directory.getChildFile("manifest.json").loadFileAsString());
            juce::MessageManager::getInstance()->stopDispatchLoop(); return;
        }
        juce::Timer::callAfterDelay(200, poll);
    };
    juce::Timer::callAfterDelay(200, poll); juce::MessageManager::getInstance()->runDispatchLoop();
    check(projectBefore == juce::JSON::toString(projectToJson(desk->engine.project)) && media->manualBpm == 128 && media->manualBeatOrigin == 1.25,
          "native split preserves exact project and manual source grid");
    const auto first = desk->stemJob->state();
    if (first.ready)
    {
        StemJob reused(source, StemJob::defaultRoot(), runtime); reused.begin();
        const auto until = juce::Time::getMillisecondCounterHiRes() + 60000;
        while (reused.state().running && juce::Time::getMillisecondCounterHiRes() < until) juce::Thread::sleep(25);
        check(reused.state().ready && reused.state().reused && reused.state().directory == first.directory, "repeated source validates and reuses the exact stem set");
        const auto changedSource = root.getChildFile("changed-source" + source.getFileExtension()); source.copyFileTo(changedSource);
        StemJob changed(changedSource, root.getChildFile("changed-stems"), runtime); changed.begin();
        const auto deadline2 = juce::Time::getMillisecondCounterHiRes() + 240000;
        while (changed.state().running && !changed.state().message.startsWith("Separating") && juce::Time::getMillisecondCounterHiRes() < deadline2) juce::Thread::sleep(25);
        check(changed.state().running && changed.state().message.startsWith("Separating"), "source-change probe reaches real inference");
        changedSource.appendText("source changed during inference");
        while (changed.state().running && juce::Time::getMillisecondCounterHiRes() < deadline2) juce::Thread::sleep(25);
        check(!changed.state().running && !changed.state().ready && changed.state().error.contains("Source changed"), "changed original refuses prepared stems before publication");
    }
    check(audioDigest(source) == sourceBefore && reference.loadFileAsString() == "reviewed source downbeat 1.25", "real split and reuse preserve original bytes and reviewed reference file");
}
}
