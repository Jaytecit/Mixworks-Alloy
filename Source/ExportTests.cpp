#include "Workstation.h"
#include <juce_cryptography/juce_cryptography.h>
#include <limits>
#include <filesystem>

namespace mixworks
{
void runExportTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report, const juce::File& delayPlugin)
{
    {
        Engine clock; float left[512] {}, right[512] {}; float* outputs[] {left, right};
        bool exact = true;
        for (const double rate : {44100., 48000.})
        {
            clock.sampleRate = rate; clock.seek(0); clock.setPlaying(true);
            const juce::int64 target = (juce::int64) (90 * 60 * rate); juce::int64 frames = 0;
            while (frames < target)
            {
                const int count = (int) std::min<juce::int64>(512, target - frames);
                clock.audioDeviceIOCallbackWithContext(nullptr, 0, outputs, 2, count, {}); frames += count;
                exact &= std::abs(clock.position.load() * rate - (double) frames) <= 8 * std::numeric_limits<double>::epsilon() * target;
            }
        }
        check(exact, "90-minute production callback clocks retain integer-frame accuracy at 44100 and 48000 Hz");
        const double start = 37.123456789; clock.seek(start); clock.setPlaying(true);
        clock.audioDeviceIOCallbackWithContext(nullptr, 0, outputs, 2, 257, {});
        const double expected = start + 257. / 48000.; const double paused = clock.position.load(); clock.setPlaying(false);
        clock.audioDeviceIOCallbackWithContext(nullptr, 0, outputs, 2, 512, {});
        const bool held = clock.position.load() == paused; clock.setPlaying(true);
        clock.audioDeviceIOCallbackWithContext(nullptr, 0, outputs, 2, 511, {});
        check(std::abs(paused - expected) < 1e-12 && held && std::abs(clock.position.load() - start - 768. / 48000.) < 1e-12,
              "fractional seek and variable callbacks preserve their exact anchor across pause and resume");
        clock.position = 19.25; clock.audioDeviceIOCallbackWithContext(nullptr, 0, outputs, 2, 512, {});
        const bool rebased = std::abs(clock.position.load() - 19.25 - 512. / 48000.) < 1e-12;
        const auto beforeRate = clock.position.load(); clock.sampleRate = 44100;
        clock.audioDeviceIOCallbackWithContext(nullptr, 0, outputs, 2, 512, {});
        check(rebased && std::abs(clock.position.load() - beforeRate - 512. / 44100.) < 1e-12,
              "explicit position and sample-rate changes re-anchor the callback clock");
        clock.stop(); clock.setPlaying(true); clock.audioDeviceIOCallbackWithContext(nullptr, 0, outputs, 2, 512, {});
        check(std::abs(clock.position.load() - 512. / 44100.) < 1e-12, "Stop discards the earlier elapsed-frame anchor");
    }
    const auto root = report.getParentDirectory().getChildFile("export-" + juce::Uuid().toString()); root.createDirectory();
    auto media = std::make_shared<Media>(); media->file = root.getChildFile("arrangement-only-source.wav"); media->sampleRate = 8000; media->samples.setSize(2, 8000);
    for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < media->frames(); ++i) media->samples.setSample(ch, i, ch == 0 ? .25f : -.125f);
    {
        juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream = media->file.createOutputStream();
        auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(2).withBitsPerSample(24));
        check(writer && writer->writeFromAudioSampleBuffer(media->samples, 0, media->frames()), "create owned source for export-protection regression");
    }
    Engine engine; engine.sampleRate = 8000; Clip clip; clip.media = media; clip.length = 1; engine.project.clips = {clip};
    const auto before = juce::SHA256(media->file).toHexString();
    check(engine.saveProject(media->file).isNotEmpty() && juce::SHA256(media->file).toHexString() == before,
          "session save also protects audio owned only by the arrangement");
    const auto exported = root.getChildFile("ordinary.wav"); const auto ordinary = engine.exportWav(exported);
    auto reader = std::unique_ptr<juce::AudioFormatReader>(engine.formats.createReaderFor(exported));
    check(ordinary.isEmpty() && reader && reader->lengthInSamples == 8000 && reader->numChannels == 2,
          "the private snapshot renderer still exports an ordinary exact-duration stereo WAV: " + ordinary);
    const auto invalid = root.getChildFile("keep-invalid.wav"); invalid.replaceWithText("previous output must remain");
    media->samples.setSample(0, 0, std::numeric_limits<float>::quiet_NaN());
    const auto nonfinite = engine.exportWav(invalid);
    check(nonfinite.isNotEmpty() && invalid.loadFileAsString() == "previous output must remain", "non-finite rendered samples reject before encoding or replacing the destination");
    media->samples.setSample(0, 0, .25f);
    const auto alias = root.getChildFile("source-hardlink.wav"); std::error_code linkError;
   #if JUCE_WINDOWS
    std::filesystem::create_hard_link(std::filesystem::path(media->file.getFullPathName().toWideCharPointer()), std::filesystem::path(alias.getFullPathName().toWideCharPointer()), linkError);
   #else
    std::filesystem::create_hard_link(std::filesystem::u8path(media->file.getFullPathName().toRawUTF8()), std::filesystem::u8path(alias.getFullPathName().toRawUTF8()), linkError);
   #endif
    check(!linkError, "create a real filesystem alias for source identity coverage");
    if (!linkError) check(engine.exportWav(alias).isNotEmpty() && juce::SHA256(media->file).toHexString() == before && juce::SHA256(alias).toHexString() == before,
                          "export rejects a different pathname that identifies the same source file");
    const auto error = engine.exportWav(media->file);
    check(error.isNotEmpty() && juce::SHA256(media->file).toHexString() == before,
          "export protects arrangement-only source audio even when the library has no entry");

    Project policy; check(policy.setEffectTailSeconds(.5).wasOk() && policy.duration() == 0 && policy.audibleEnd() == .5, "tail policy preserves musical duration");
    const auto session = projectToJson(policy);
    auto parsed = parseProject(juce::JSON::toString(session));
    check(parsed.error.isEmpty() && parsed.project.effectTailSeconds == .5, "version-19 sessions preserve their explicit tail window");
    auto legacy = session.clone(); legacy.getDynamicObject()->setProperty("version", 18); legacy["tracks"].getArray()->removeRange(4, trackCount - 4); legacy.getDynamicObject()->removeProperty("effectTailSeconds");
    parsed = parseProject(juce::JSON::toString(legacy));
    check(parsed.error.isEmpty() && parsed.project.effectTailSeconds == 0, "version-18 sessions retain last-clip cutoff defaults");
    for (const auto& value : {juce::var(-1.), juce::var("0.5"), juce::var(1.0e9 + 1)})
    {
        auto bad = session.clone(); bad.getDynamicObject()->setProperty("effectTailSeconds", value);
        check(parseProject(juce::JSON::toString(bad)).error.isNotEmpty(), "invalid persisted tail policy rejects at its owning boundary");
    }
    check(policy.setEffectTailSeconds(std::numeric_limits<double>::infinity()).failed() && policy.effectTailSeconds == .5,
          "invalid tail edits preserve the previous setting");
    WavExportPlan huge; engine.project.setEffectTailSeconds(1.0e9 - 1);
    check(engine.planWavExport(exported, {true}, huge).failed() && huge.available && huge.frames == (juce::int64) 8000000000000LL
          && juce::SHA256(media->file).toHexString() == before, "space admission preserves the calculated long-file plan and all sources");
    engine.project.setEffectTailSeconds(0);

    check(delayPlugin.exists(), "actual delay VST3 fixture is available"); if (!delayPlugin.exists()) return;
    auto shortMedia = std::make_shared<Media>(); shortMedia->file = root.getChildFile("short-source.wav"); shortMedia->sampleRate = 8000; shortMedia->samples.setSize(2, 400);
    for (int i = 0; i < 400; ++i) { shortMedia->samples.setSample(0, i, .5f); shortMedia->samples.setSample(1, i, -.25f); }
    {
        juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream = shortMedia->file.createOutputStream();
        auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(2).withBitsPerSample(24));
        check(writer && writer->writeFromAudioSampleBuffer(shortMedia->samples, 0, 400), "create owned short stereo source");
    }
    Engine delayed; delayed.sampleRate = 8000; delayed.project.masterDb = 0; Clip shortClip; shortClip.media = shortMedia; shortClip.length = .05;
    delayed.project.clips = {shortClip}; delayed.selectedClip = shortClip.id; delayed.project.setEffectTailSeconds(.5);
    const auto loaded = delayed.addPlugin(0, delayPlugin); check(loaded.isEmpty(), "load the actual finite-delay VST3: " + loaded); if (loaded.isNotEmpty()) return;
    auto& slot = *delayed.project.clips[0].effects->plugins[0];
    check(!delayed.project.clips[0].effects->hasInstrument() && std::abs(slot.processor->getTailLengthSeconds() - .375) < .0001,
          "real effect advertises a finite tail without becoming a paused instrument");
    auto renderFile = [&](const juce::String& name, bool includeTail)
    {
        juce::String why; auto renderer = delayed.createExportRenderer(why);
        check(renderer && why.isEmpty() && renderer->project.clips[0].effects->plugins[0]->processor.get() != slot.processor.get()
              && renderer->project.clips[0].effects->plugins[0]->id == slot.id, "export owns an independent real processor with stable automation identity");
        auto destination = root.getChildFile(name); WavExportResult result;
        juce::int64 previous = -1; bool progressValid = true;
        if (renderer) result = renderer->writeWav(destination, {includeTail}, {}, [&](auto done, const auto& plan)
        { progressValid = progressValid && done >= previous && done <= plan.frames; previous = done; });
        check(result.wasOk() && previous == result.plan.frames && progressValid, "completed export reports monotonic exact frame progress: " + result.error);
        return destination;
    };
    auto read = [&](const juce::File& file)
    {
        auto wav = std::unique_ptr<juce::AudioFormatReader>(delayed.formats.createReaderFor(file)); juce::AudioBuffer<float> data;
        if (wav && wav->lengthInSamples <= 10000) { data.setSize(2, (int) wav->lengthInSamples); wav->read(&data, 0, data.getNumSamples(), 0, true, true); }
        return data;
    };
    const auto originalPosition = delayed.position.load(); juce::MemoryBlock originalState; slot.processor->getStateInformation(originalState);
    auto tail = read(renderFile("with-tail.wav", true)); bool echoes = tail.getNumSamples() == 4400;
    for (int i = 0; i < tail.getNumSamples(); ++i)
    {
        float expected = i >= 1000 && i < 1400 ? .3f : i >= 2000 && i < 2400 ? .15f : i >= 3000 && i < 3400 ? .05f : 0;
        echoes = echoes && std::abs(tail.getSample(0, i) - expected) < 2e-6f && std::abs(tail.getSample(1, i) + expected / 2) < 2e-6f;
    }
    check(echoes, "actual clip-owned VST3 receives zero input after source end and writes all three stereo echoes plus final silence");
    juce::MemoryBlock afterState; slot.processor->getStateInformation(afterState);
    check(originalState == afterState && delayed.position == originalPosition && delayed.project.duration() == .05,
          "offline processing leaves live processor state, position and arrangement geometry unchanged");
    auto cutoff = read(renderFile("cutoff.wav", false));
    check(cutoff.getNumSamples() == 400 && cutoff.getMagnitude(0, 400) == 0, "last-clip cutoff excludes the real delayed signal");
    delayed.project.setEffectTailSeconds(.2); auto capped = read(renderFile("capped.wav", true));
    check(capped.getNumSamples() == 2000 && std::abs(capped.getSample(0, 1000) - .3f) < 2e-6f && capped.getSample(0, 1999) == 0,
          "caller-selected tail window truncates later echoes at the exact sample boundary");
    delayed.project.setEffectTailSeconds(.5); slot.bypass = true; auto bypassed = read(renderFile("bypassed.wav", true));
    check(bypassed.getNumSamples() == 4400 && bypassed.getSample(0, 0) == .5f && bypassed.getMagnitude(400, 4000) == 0,
          "bypassed actual effect preserves raw source and appends only silence"); slot.bypass = false;
    shortMedia->samples.clear(); auto silent = read(renderFile("silent.wav", true));
    check(silent.getNumSamples() == 4400 && silent.getMagnitude(0, 4400) == 0, "zero-input export has no stale echo from an earlier renderer");
    for (int i = 0; i < 400; ++i) { shortMedia->samples.setSample(0, i, .5f); shortMedia->samples.setSample(1, i, -.25f); }
    const auto kept = root.getChildFile("cancelled.wav"); kept.replaceWithText("previous destination");
    juce::String why; auto renderer = delayed.createExportRenderer(why); auto cancel = std::make_shared<std::atomic<bool>>(false);
    WavExportResult cancelled;
    if (renderer) cancelled = renderer->writeWav(kept, {true}, cancel, [&](auto done, const auto&) { if (done >= 1024) *cancel = true; });
    check(renderer && cancelled.outcome == WavExportOutcome::cancelled && cancelled.writtenFrames == 1024 && kept.loadFileAsString() == "previous destination",
          "cooperative mid-file cancellation leaves the existing destination intact");
    if (renderer) cancelled = renderer->writeWav(kept, {true}, cancel);
    check(renderer && cancelled.outcome == WavExportOutcome::cancelled && cancelled.writtenFrames == 0 && kept.loadFileAsString() == "previous destination",
          "pre-cancelled export writes no samples and preserves the destination");
    *cancel = false;
    if (renderer) cancelled = renderer->writeWav(kept, {true}, cancel, [&](auto done, const auto& plan) { if (done == plan.frames) *cancel = true; });
    check(renderer && cancelled.outcome == WavExportOutcome::cancelled && cancelled.writtenFrames == 4400 && kept.loadFileAsString() == "previous destination"
          && !kept.getSiblingFile(kept.getFileName() + ".bak").exists(), "late cancellation after all samples still preserves destination before backup and commit");
    *cancel = false;
    WavExportResult committed; if (renderer) committed = renderer->writeWav(kept, {true}, cancel);
    check(committed.wasOk() && kept.getSiblingFile(kept.getFileName() + ".bak").loadFileAsString() == "previous destination",
          "successful retry replaces only the destination and keeps an additive backup of its previous bytes");
    delayed.seek(0); delayed.setPlaying(true); juce::AudioBuffer<float> live(2, 4608); live.clear(); float left[512]{}, right[512]{}; float* outputs[] {left, right};
    for (int offset = 0; offset < 4608; offset += 512)
    {
        delayed.audioDeviceIOCallbackWithContext(nullptr, 0, outputs, 2, 512, {});
        live.copyFrom(0, offset, left, 512); live.copyFrom(1, offset, right, 512);
    }
    bool same = tail.getNumSamples() == 4400;
    for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < tail.getNumSamples(); ++i) same = same && std::abs(live.getSample(ch, i) - tail.getSample(ch, i)) < 2e-6f;
    check(same && !delayed.playing && std::abs(delayed.position.load() - .55) < 1e-9, "continuous device playback matches exported tail PCM and stops at the audible end");
    delayed.audioDeviceIOCallbackWithContext(nullptr, 0, outputs, 2, 512, {});
    check(juce::FloatVectorOperations::findMinAndMax(left, 512).getLength() == 0 && juce::FloatVectorOperations::findMinAndMax(right, 512).getLength() == 0 && left[0] == 0 && right[0] == 0,
          "an ordinary clip effect cannot continue as paused instrument audition after auto-stop");
    Engine lane; lane.sampleRate = 8000; lane.project.masterDb = 0; shortClip.effects = makeClipEffects(); lane.project.clips = {shortClip}; lane.project.setEffectTailSeconds(.2);
    const auto laneLoaded = lane.addPlugin(0, delayPlugin); check(laneLoaded.isEmpty(), "load actual delay in the lane bus independently of clip inserts");
    if (laneLoaded.isEmpty())
    {
        lane.seek(0); lane.setPlaying(true);
        for (int block = 0; block < 4; ++block) lane.audioDeviceIOCallbackWithContext(nullptr, 0, outputs, 2, 512, {});
        bool bounded = true; for (int i = 464; i < 512; ++i) bounded = bounded && left[i] == 0 && right[i] == 0;
        check(bounded && !lane.playing && std::abs(lane.position.load() - .25) < 1e-9,
              "device's final partial block cuts lane-effect echoes at the exact selected audible end");
        lane.audioDeviceIOCallbackWithContext(nullptr, 0, outputs, 2, 512, {});
        bool stopped = true; for (int i = 0; i < 512; ++i) stopped = stopped && left[i] == 0 && right[i] == 0;
        check(stopped, "non-instrument lane effects cannot resume their truncated tail during paused monitoring");
        juce::String reason; auto copy = lane.createExportRenderer(reason); const auto file = root.getChildFile("lane-tail.wav");
        const auto result = copy ? copy->writeWav(file, {true}) : WavExportResult{}; auto pcm = read(file);
        check(result.wasOk() && pcm.getNumSamples() == 2000 && std::abs(pcm.getSample(0, 1000) - .3f) < 2e-6f,
              "lane-owned actual delay drains through the same explicit output window in export");
    }

    auto ui = std::make_unique<Workstation>(false); ui->stopTimer(); ui->setSize(1440, 940); ui->addToDesktop(0); ui->setVisible(true);
    ui->engine.sampleRate = 8000; ui->engine.project.masterDb = 0; shortClip.effects = makeClipEffects(); ui->engine.project.clips = {shortClip};
    ui->engine.selectedClip = shortClip.id; ui->selectedClip = shortClip.id;
    const auto nativeLoaded = ui->engine.addPlugin(0, delayPlugin); check(nativeLoaded.isEmpty(), "native export loads its own real VST3"); if (nativeLoaded.isNotEmpty()) return;
    const auto undoBefore = ui->undoHistory.size();
    check(ui->applyEffectTail(.5).wasOk() && ui->dirty && ui->undoHistory.size() == undoBefore + 1 && ui->engine.project.effectTailSeconds == .5,
          "explicit native tail configuration makes one saved mix edit and Undo entry");
    ui->applyEffectTail(.5); check(ui->undoHistory.size() == undoBefore + 1, "unchanged tail configuration does not add redundant Undo");
    ui->undo(false); check(ui->engine.project.effectTailSeconds == 0, "Undo restores the previous end policy");
    ui->undo(true); check(ui->engine.project.effectTailSeconds == .5, "Redo restores the selected end policy");
    check(ui->applyEffectTail(-.1).failed() && ui->engine.project.effectTailSeconds == .5, "invalid native tail edit preserves the saved policy");
    auto& nativeSlot = *ui->engine.project.clips[0].effects->plugins[0];
    const auto slow = std::find_if(nativeSlot.parameters.begin(), nativeSlot.parameters.end(), [](const auto& entry) { return entry.second->getName(128) == "Slow export fixture"; });
    check(slow != nativeSlot.parameters.end(), "slow real-time fixture binds by its named parameter contract"); if (slow == nativeSlot.parameters.end()) return;
    slow->second->setValueNotifyingHost(1); ui->applyEffectTail(60);
    const auto nativeDestination = root.getChildFile("native-cancel.wav"); nativeDestination.replaceWithText("keep native destination");
    const auto nativeBefore = juce::JSON::toString(ui->engine.captureProject()); const auto nativeUndo = ui->undoHistory.size();
    ui->chooseExportEnd(nativeDestination);
    auto* prompt = dynamic_cast<juce::AlertWindow*>(juce::ModalComponentManager::getInstance()->getModalComponent(0));
    check(prompt && prompt->getButton("Last clip") && prompt->getButton("Include tail") && prompt->getButton("Set tail..."), "native export makes cutoff versus configured tail an explicit choice");
    if (prompt) prompt->triggerButtonClick("Cancel");
    struct Poll final : juce::Timer { std::function<void()> tick; void timerCallback() override { tick(); } } poll;
    const double began = juce::Time::getMillisecondCounterHiRes(); int phase = 0; bool sawProgress = false, lateAdmissionDrained = false;
    const auto pendingDestination = root.getChildFile("close-pending-admission.wav");
    std::weak_ptr<Workstation::ExportTask> pendingAdmission;
    poll.tick = [&]
    {
        if (juce::Time::getMillisecondCounterHiRes() - began > 20000)
        {
            check(false, "native export workflow finishes within its bounded timeout");
            if (ui && ui->exportTask) *ui->exportTask->cancelled = true;
            poll.stopTimer(); juce::MessageManager::getInstance()->stopDispatchLoop(); return;
        }
        if (phase == 0 && juce::ModalComponentManager::getInstance()->getNumModalComponents() == 0)
        {
            check(!ui->busy && juce::JSON::toString(ui->engine.captureProject()) == nativeBefore && ui->undoHistory.size() == nativeUndo,
                  "cancelling the export choice does not edit tail policy or mix state");
            ui->beginWavExport(nativeDestination, {true});
            check(ui->busy && ui->engine.exporting && ui->importProgress.isVisible() && ui->importProgress.cancel.isEnabled(), "native worker starts with accessible progress and Cancel");
            auto task = ui->exportTask; ui->beginWavExport(nativeDestination, {});
            check(task && task == ui->exportTask, "repeated export cannot replace the active owner"); ++phase;
        }
        else if (phase == 1 && ui->exportTask && ui->exportTask->completed >= 1024)
        {
            sawProgress = true; ui->timerCallback(); const double response = juce::Time::getMillisecondCounterHiRes();
            ui->engine.advanceAutomation(0); ui->importProgress.cancel.onClick();
            check(juce::Time::getMillisecondCounterHiRes() - response < 250 && *ui->exportTask->cancelled && !ui->importProgress.cancel.isEnabled(),
                  "native main timer and actual Cancel handler stay responsive during real slow VST3 export");
            auto stream = report.getSiblingFile("export-cancelling.png").createOutputStream(); juce::PNGImageFormat png;
            check(stream && png.writeImageToStream(ui->createComponentSnapshot(ui->getLocalBounds()), *stream), "capture native export progress and cancellation state"); ++phase;
        }
        else if (phase == 2 && !ui->busy)
        {
            check(sawProgress && !ui->exportTask && !ui->engine.exporting && !ui->importProgress.isVisible() && nativeDestination.loadFileAsString() == "keep native destination"
                  && ui->status.containsIgnoreCase("cancelled") && ui->undoHistory.size() == nativeUndo && juce::JSON::toString(ui->engine.captureProject()) == nativeBefore,
                  "native cancellation cleans ownership and restores controls without altering mix or destination");
            ui->applyEffectTail(.5); ui->beginWavExport(root.getChildFile("native-success.wav"), {true}); ++phase;
        }
        else if (phase == 3 && !ui->busy)
        {
            auto wav = std::unique_ptr<juce::AudioFormatReader>(ui->engine.formats.createReaderFor(root.getChildFile("native-success.wav")));
            check(!ui->exportTask && !ui->engine.exporting && wav && wav->lengthInSamples == 4400 && ui->status.startsWith("Exported"),
                  "native retry after cancellation commits an exact tail-length WAV and releases the job");
            ui->applyEffectTail(60); ui->beginWavExport(root.getChildFile("native-close.wav"), {true});
            const std::weak_ptr<Workstation::ExportTask> weak = ui->exportTask;
            const auto closeStart = juce::Time::getMillisecondCounterHiRes(); ui.reset();
            check(weak.expired() && juce::Time::getMillisecondCounterHiRes() - closeStart < 2000 && !root.getChildFile("native-close.wav").exists(),
                  "closing an owned export cancels and joins before destroying native processors on the message thread");
            ui = std::make_unique<Workstation>(false); ui->stopTimer(); ui->engine.sampleRate = 8000;
            auto pendingClip = shortClip; pendingClip.effects = makeClipEffects(); ui->engine.project.clips = {pendingClip}; ui->engine.selectedClip = pendingClip.id;
            const auto pendingLoaded = ui->engine.addPlugin(0, delayPlugin);
            check(pendingLoaded.isEmpty(), "final-admission close test owns a separate actual VST3 instance");
            if (pendingLoaded.isNotEmpty()) { poll.stopTimer(); juce::MessageManager::getInstance()->stopDispatchLoop(); return; }
            ui->beginWavExport(pendingDestination, {}); pendingAdmission = ui->exportTask;
            const auto waitUntil = juce::Time::getMillisecondCounterHiRes() + 2000; bool awaitingMain = false;
            while (ui->exportTask && juce::Time::getMillisecondCounterHiRes() < waitUntil)
            {
                auto& task = *ui->exportTask;
                if (task.completed == 400 && task.renderer->lock.tryEnter())
                {
                    // After all frames, the exclusive private lock is released
                    // only for the UI admission. Its export flag stays set.
                    awaitingMain = task.renderer->exporting; task.renderer->lock.exit();
                    if (awaitingMain) break;
                }
                juce::Thread::sleep(1);
            }
            check(awaitingMain && ui->busy && !pendingDestination.exists(), "completed worker remains cancellable while final UI-only admission is queued");
            const auto closingAt = juce::Time::getMillisecondCounterHiRes(); ui.reset();
            check(pendingAdmission.expired() && juce::Time::getMillisecondCounterHiRes() - closingAt < 2000 && !pendingDestination.exists(),
                  "close disarms pending native admission before joining and releasing its renderer");
            juce::MessageManager::callAsync([&] { lateAdmissionDrained = true; }); ++phase;
        }
        else if (phase == 4 && lateAdmissionDrained)
        {
            check(pendingAdmission.expired() && !pendingDestination.exists(), "queued native admission and completion callbacks safely drain after their renderer and window are destroyed");
            poll.stopTimer(); juce::MessageManager::getInstance()->stopDispatchLoop();
        }
    };
    poll.startTimer(10); juce::MessageManager::getInstance()->runDispatchLoop();
}
}
