#include "Workstation.h"
#include <juce_cryptography/juce_cryptography.h>

namespace mixworks
{
void runLatencyTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report, const juce::File& module)
{
    const auto root = report.getParentDirectory().getChildFile("latency-" + juce::Uuid().toString()); root.createDirectory();
    check(module.exists(), "actual latency VST3 fixture is available"); if (!module.exists()) return;
    auto media = std::make_shared<Media>(); media->file = root.getChildFile("owned-impulses.wav"); media->sampleRate = 8000; media->name = "Owned stereo impulses"; media->samples.setSize(2, 8000); media->samples.clear();
    media->samples.setSample(0, 0, .25f); media->samples.setSample(1, 0, -.125f);
    {
        juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream = media->file.createOutputStream();
        auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(2).withBitsPerSample(24));
        check(writer && writer->writeFromAudioSampleBuffer(media->samples, 0, 8000), "create owned stereo impulse fixture");
    }
    Engine engine; engine.sampleRate = 8000; engine.project.masterDb = 0;
    Clip dry; dry.media = media; dry.length = 1; Clip wet = dry; wet.id = juce::Uuid().toString(); wet.track = 1; wet.effects = makeClipEffects();
    engine.project.clips = {dry, wet}; engine.selectedClip = wet.id; engine.selectedTrack = 1;
    const auto loaded = engine.addPlugin(1, module); check(loaded.isEmpty(), "retain actual latent processor for editing or explicit bypass: " + loaded); if (loaded.isNotEmpty()) return;
    auto& slot = *engine.project.clips[1].effects->plugins[0];
    check(slot.processor->getLatencySamples() == 64 && slot.processor->getTailLengthSeconds() == 0, "actual plugin distinguishes imposed latency from tail length");
    juce::AudioBuffer<float> raw(2, 512); raw.clear(); raw.setSample(0, 0, .25f); juce::MidiBuffer midi;
    slot.processor->processBlock(raw, midi);
    check(raw.getSample(0, 0) == 0 && raw.getSample(0, 64) == .25f, "actual native processor physically delays an impulse by its reported 64 samples");
    const auto mode = std::find_if(slot.parameters.begin(), slot.parameters.end(), [](const auto& entry) { return entry.second->getName(128) == "Latency enabled"; });
    check(mode != slot.parameters.end(), "actual native latency mode binds by its named parameter contract"); if (mode == slot.parameters.end()) return;
    // Host state is the real VST3 wrapper document, not the fixture processor's
    // internal byte payload. Capture both actual modes through that boundary.
    juce::MemoryBlock zeroState, latentState;
    mode->second->setValueNotifyingHost(0); raw.clear(); slot.processor->processBlock(raw, midi); slot.processor->getStateInformation(zeroState);
    check(slot.processor->getLatencySamples() == 0, "actual zero-latency preset is captured through native host state");
    mode->second->setValueNotifyingHost(1); raw.clear(); slot.processor->processBlock(raw, midi); slot.processor->getStateInformation(latentState);
    check(slot.processor->getLatencySamples() == 64 && zeroState != latentState, "actual latent preset retains distinct wrapper state and its reported latency");
    const auto destination = root.getChildFile("must-keep.wav"); destination.replaceWithText("previous latency destination");
    const auto rejected = engine.exportWav(destination);
    check(rejected.containsIgnoreCase("latency") && destination.loadFileAsString() == "previous latency destination",
          "export explicitly rejects an uncompensated parallel path before replacing the destination");
    engine.seek(0); engine.setPlaying(true); float left[512]{}, right[512]{}; float* outputs[] {left, right};
    engine.audioDeviceIOCallbackWithContext(nullptr, 0, outputs, 2, 512, {});
    bool silent = true; for (int i = 0; i < 512; ++i) silent = silent && left[i] == 0 && right[i] == 0;
    check(silent && !engine.playing, "unsupported active latency cannot silently produce two misaligned parallel impulses in device playback");
    const auto saved = root.getChildFile("latent.mixworks"); const auto originalHash = juce::SHA256(media->file).toHexString();
    check(engine.saveProject(saved).isEmpty() && saved.existsAsFile() && juce::SHA256(media->file).toHexString() == originalHash,
          "unsupported active latency still permits lossless session save without changing source audio");
    slot.bypass = true; const auto bypassed = engine.exportWav(root.getChildFile("bypassed.wav"));
    auto reader = std::unique_ptr<juce::AudioFormatReader>(engine.formats.createReaderFor(root.getChildFile("bypassed.wav"))); juce::AudioBuffer<float> pcm(2, 512); pcm.clear();
    if (reader) reader->read(&pcm, 0, 512, 0, true, true);
    check(bypassed.isEmpty() && reader && pcm.getSample(0, 0) == .5f && pcm.getSample(0, 64) == 0,
          "explicit bypass restores exact alignment and permits transactional export");
    Engine reopened; const auto opened = reopened.restoreProject(saved.loadFileAsString());
    check(opened.isEmpty() && reopened.project.clips.size() == 2 && reopened.project.clips[1].effects->plugins.size() == 1
          && reopened.pluginLatencyError().contains("64"), "real session reopening preserves unsupported plugin state and its explicit restriction: " + opened);
    slot.bypass = false;
    slot.processor->setStateInformation(zeroState.getData(), (int) zeroState.getSize());
    check(slot.processor->getLatencySamples() == 0 && engine.pluginLatencyError().isEmpty(), "actual native state change publishes zero latency through JUCE's host notification");
    engine.seek(0); engine.setPlaying(true); engine.audioDeviceIOCallbackWithContext(nullptr, 0, outputs, 2, 512, {});
    check(engine.playing && left[0] == .5f && left[64] == 0, "choosing a zero-latency mode restores exact live parallel alignment");
    const auto position = engine.position.load(); slot.processor->setStateInformation(latentState.getData(), (int) latentState.getSize());
    engine.audioDeviceIOCallbackWithContext(nullptr, 0, outputs, 2, 512, {}); silent = true;
    for (int i = 0; i < 512; ++i) silent = silent && left[i] == 0 && right[i] == 0;
    check(slot.processor->getLatencySamples() == 64 && silent && !engine.playing && engine.latencyBlocked && engine.position == position,
          "dynamic reported latency stops live output without advancing the clock or changing the saved bypass choice");
    juce::String why; auto invalidRenderer = engine.createExportRenderer(why);
    check(!invalidRenderer && why.contains("64") && !slot.bypass, "offline preparation rejects unsupported state without silently bypassing or cloning it");
    engine.project.transitionLinks = {{dry.id, wet.id}};
    const auto rehearsal = engine.prepareTransitionRehearsal(wet.id, std::make_shared<std::atomic<bool>>(false));
    check(rehearsal.failed() && rehearsal.getErrorMessage().containsIgnoreCase("latency") && !engine.rehearsalActive() && !engine.exporting,
          "rehearsal rejects unsupported latency and releases exclusive rendering ownership");
    slot.processor->setStateInformation(zeroState.getData(), (int) zeroState.getSize()); auto renderer = engine.createExportRenderer(why);
    destination.replaceWithText("keep on dynamic export failure"); WavExportResult changed;
    if (renderer) changed = renderer->writeWav(destination, {}, {}, [&](auto done, const auto&)
    { if (done == 512) renderer->project.clips[1].effects->plugins[0]->processor->setStateInformation(latentState.getData(), (int) latentState.getSize()); });
    check(renderer && changed.outcome == WavExportOutcome::failed && changed.error.containsIgnoreCase("latency") && changed.writtenFrames == 512
          && destination.loadFileAsString() == "keep on dynamic export failure", "a real latency change during rendering rejects the partial WAV and preserves its destination");
    check(slot.processor->getLatencySamples() == 0 && !slot.bypass, "offline clone changes leave live plugin state and bypass unchanged");
    renderer = engine.createExportRenderer(why); destination.replaceWithText("keep final-block latency destination");
    if (renderer) changed = renderer->writeWav(destination, {}, {}, [&](auto done, const auto& plan)
    { if (done == plan.frames) renderer->project.clips[1].effects->plugins[0]->processor->setStateInformation(latentState.getData(), (int) latentState.getSize()); });
    check(renderer && changed.outcome == WavExportOutcome::failed && changed.writtenFrames == 8000 && changed.error.containsIgnoreCase("latency")
          && destination.loadFileAsString() == "keep final-block latency destination", "a reported latency change at the final progress callback still rejects before committing the WAV");

    Engine lane; lane.sampleRate = 8000; lane.project.clips = {dry};
    const auto laneLoaded = lane.addPlugin(2, module);
    check(laneLoaded.isEmpty() && lane.pluginLatencyError().startsWith("Lane 3:"), "lane-owned latency uses the same owner-specific restriction as clip inserts");
    if (laneLoaded.isEmpty())
    {
        lane.project.masterDb = 0; lane.seek(0); lane.setPlaying(true); lane.audioDeviceIOCallbackWithContext(nullptr, 0, outputs, 2, 512, {});
        check(!lane.playing && lane.latencyBlocked && left[0] == 0, "unsupported lane processing cannot leak an apparently valid partial mix from another lane");
        lane.plugins[2][0]->bypass = true;
        check(lane.pluginLatencyError().isEmpty() && lane.exportWav(root.getChildFile("lane-bypass.wav")).isEmpty(), "explicit bypass of a latent lane allows the remaining mix to export");
    }

    Workstation ui(false); ui.stopTimer(); ui.setSize(1440, 940); ui.addToDesktop(0); ui.setVisible(true);
    ui.engine.sampleRate = 8000; auto uiClip = dry; uiClip.effects = makeClipEffects(); ui.engine.project.clips = {uiClip}; ui.engine.selectedClip = dry.id; ui.selectedClip = dry.id;
    const auto uiLoaded = ui.engine.addPlugin(0, module); check(uiLoaded.isEmpty(), "native latency rack loads its own actual fixture");
    if (uiLoaded.isEmpty())
    {
        ui.refresh(); ui.showInspectorPage(1); ui.pluginList.list.selectRow(0); ui.playButton.onClick(); ui.timerCallback();
        check(!ui.engine.playing && ui.status.contains("64") && ui.pluginList.getTooltipForRow(0).contains("zero-latency"),
              "native Play explains the active latency restriction and rack tooltip retains its full actionable message");
        auto image = report.getSiblingFile("latency-blocked.png").createOutputStream(); juce::PNGImageFormat png;
        check(image && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *image), "capture native blocked-latency row and status");
        ui.bypassPluginButton.onClick(); ui.playButton.onClick();
        check(ui.engine.playing && ui.engine.project.clips[0].effects->plugins[0]->bypass && ui.dirty, "actual native Bypass and Play handlers recover playback while preserving an explicit saved choice");
        ui.engine.setPlaying(false); auto& nativeSlot = *ui.engine.project.clips[0].effects->plugins[0];
        nativeSlot.processor->setStateInformation(zeroState.getData(), (int) zeroState.getSize()); ui.bypassPluginButton.onClick();
        const auto nativeMode = std::find_if(nativeSlot.parameters.begin(), nativeSlot.parameters.end(), [](const auto& entry) { return entry.second->getName(128) == "Latency enabled"; });
        if (nativeMode == nativeSlot.parameters.end()) { check(false, "native automation retains the named latency parameter"); return; }
        const AutomationAddress address {0, AutomationTarget::plugin, nativeSlot.id, nativeMode->first, dry.id};
        ui.engine.ensurePluginAutomation(address); auto* curve = ui.engine.project.curve(address);
        if (!curve) { check(false, "native latency automation owns its curve"); return; }
        curve->enabled = true; curve->setPoint(0, 0); curve->setPoint(.992, 0); curve->setPoint(.996, 1);
        const auto finalDestination = root.getChildFile("native-queued-latency.wav"); finalDestination.replaceWithText("keep queued notification destination");
        ui.beginWavExport(finalDestination, {});
        const auto renderedTask = ui.exportTask;
        // Hold only this owned test's message thread until all samples finish.
        // The worker must request the UI-only authoritative native query before commit.
        const auto waitUntil = juce::Time::getMillisecondCounterHiRes() + 2000;
        while (ui.exportTask && ui.exportTask->completed < 8000 && juce::Time::getMillisecondCounterHiRes() < waitUntil) juce::Thread::sleep(1);
        check(ui.exportTask && ui.exportTask->completed == 8000 && ui.busy && finalDestination.loadFileAsString() == "keep queued notification destination",
              "worker cannot commit while its final native latency notification is queued behind the message thread");
        struct Poll final : juce::Timer { std::function<void()> tick; void timerCallback() override { tick(); } } poll;
        const auto deadline = juce::Time::getMillisecondCounterHiRes() + 10000;
        poll.tick = [&]
        {
            if (juce::Time::getMillisecondCounterHiRes() > deadline)
            {
                check(false, "queued native latency export finishes within its test deadline");
                if (ui.exportTask) *ui.exportTask->cancelled = true;
                poll.stopTimer(); juce::MessageManager::getInstance()->stopDispatchLoop(); return;
            }
            if (ui.busy) return;
            check(!ui.exportTask && !ui.engine.exporting && ui.status.containsIgnoreCase("latency")
                  && finalDestination.loadFileAsString() == "keep queued notification destination",
                  "real worker queries authoritative VST3 latency on main, rejects it and preserves the previous output: " + ui.status
                  + " / final latency " + juce::String(renderedTask ? renderedTask->renderer->project.clips[0].effects->plugins[0]->processor->getLatencySamples() : -1));
            if (auto* alert = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent())) alert->exitModalState(0);
            poll.stopTimer(); juce::MessageManager::getInstance()->stopDispatchLoop();
        };
        poll.startTimer(10); juce::MessageManager::getInstance()->runDispatchLoop();
    }
}
}
