#include "Workstation.h"
#include "Spectrum.h"
#include <cmath>
#include <limits>

namespace mixworks
{
void runMasteringTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    const auto root = report.getParentDirectory().getChildFile("mastering-" + juce::Uuid().toString()); root.createDirectory();
    const auto json = [](const Project& p) { return juce::JSON::toString(projectToJson(p)); };
    MasterSettings settings; settings.eqDb = {3, -2, 5}; settings.width = 1.4f; settings.saturation = true; settings.saturationDriveDb = 4;
    MasterSettings parsed;
    check(readMasterSettings(masterSettingsToJson(settings), parsed).wasOk() && parsed.eqDb == settings.eqDb && parsed.width == settings.width && parsed.saturation,
          "master settings round-trip through their owning contract");
    for (const auto& field : {juce::String("width"), juce::String("saturationDriveDb"), juce::String("saturation")})
    {
        auto bad = masterSettingsToJson(settings); bad.getDynamicObject()->setProperty(field, "invalid");
        const auto before = juce::JSON::toString(masterSettingsToJson(parsed));
        check(readMasterSettings(bad, parsed).failed() && juce::JSON::toString(masterSettingsToJson(parsed)) == before, "wrongly typed master " + field + " rejects without a partial mutation");
    }
    auto boundary = masterSettingsToJson(settings); boundary.getDynamicObject()->setProperty("width", 2.000000001);
    check(readMasterSettings(boundary, parsed).failed(), "out-of-range master width rejects before float rounding can conceal it");
    Project p; p.mastering = settings; const auto roundTrip = parseProject(json(p));
    check(roundTrip.error.isEmpty() && roundTrip.project.mastering.eqDb == settings.eqDb && roundTrip.project.mastering.saturation, "version-18 session owns its master processing settings");
    auto old = projectToJson(p); old.getDynamicObject()->setProperty("version", 17); old["tracks"].getArray()->removeRange(4, trackCount - 4); old.getDynamicObject()->removeProperty("mastering");
    const auto legacy = parseProject(juce::JSON::toString(old));
    check(legacy.error.isEmpty() && legacy.project.mastering.eqDb == std::array<float, 3>{} && legacy.project.mastering.width == 1 && !legacy.project.mastering.saturation,
          "version-17 session opens with exact bypass defaults");
    auto badProject = projectToJson(p); badProject.getDynamicObject()->removeProperty("mastering");
    check(parseProject(juce::JSON::toString(badProject)).error.isNotEmpty(), "version-18 session cannot silently omit its master settings");

    SpectrumFeed feed; SpectrumAnalysis analysis; juce::AudioBuffer<float> tone(2, SpectrumAnalysis::size);
    for (int i = 0; i < tone.getNumSamples(); ++i)
    {
        const auto value = .25f * (float) std::sin(i * 2 * juce::MathConstants<double>::pi * 128 / SpectrumAnalysis::size);
        tone.setSample(0, i, value); tone.setSample(1, i, value);
    }
    check(feed.push(tone, tone.getNumSamples(), 8192) == 0 && !analysis.update(feed), "hidden spectrum does not enqueue or analyze audio"); feed.enabled = true;
    check(feed.push(tone, tone.getNumSamples(), 8192) == tone.getNumSamples() && analysis.update(feed), "UI analyzer consumes a complete independently queued stereo window");
    const auto peak = (int) std::distance(analysis.levels().begin(), std::max_element(analysis.levels().begin(), analysis.levels().end()));
    const float coherent = analysis.levels()[128];
    check(peak == 128 && std::abs(coherent - 20 * std::log10(.25)) < .03 && analysis.rate() == 8192,
          "Hann FFT reports the actual bin and coherent dBFS amplitude: " + juce::String(coherent, 4));
    tone.applyGain(1, 0, tone.getNumSamples(), -1);
    feed.push(tone, tone.getNumSamples(), 8192); analysis.update(feed);
    check(std::abs(analysis.levels()[128] - coherent) < .001, "anti-phase stereo remains visible with unchanged spectral power");
    tone.clear(1, 0, tone.getNumSamples()); feed.push(tone, tone.getNumSamples(), 8192); analysis.update(feed);
    check(std::abs(analysis.levels()[128] - coherent + 3.0103f) < .01, "one active stereo channel has the defined mean-power calibration");
    feed.push(tone, 2048, 8192); feed.push(tone, tone.getNumSamples(), 16000); analysis.update(feed);
    check(analysis.ready() && analysis.rate() == 16000 && std::abs(analysis.levels()[128] - coherent + 3.0103f) < .01,
          "queued old-rate samples cannot relabel a new-rate FFT history");
    const int acceptedA = feed.push(tone, tone.getNumSamples(), 16000), acceptedB = feed.push(tone, tone.getNumSamples(), 16000), acceptedC = feed.push(tone, tone.getNumSamples(), 16000);
    check(acceptedA + acceptedB + acceptedC + (int) feed.dropped.load() == tone.getNumSamples() * 3 && feed.dropped > 0,
          "full spectrum queue drops counted display samples without blocking the audio producer");
    check(!analysis.update(feed) && !analysis.ready(), "a discontinuous spectrum discards its stale history");
    feed.push(tone, tone.getNumSamples(), 16000); check(analysis.update(feed), "fresh complete audio restores the spectrum after overflow");

    auto media = std::make_shared<Media>(); media->file = root.getChildFile("master-source.wav"); media->sampleRate = 8000;
    media->manualBpm = 120; media->manualBeatOrigin = 0; media->samples.setSize(2, 16000);
    for (int i = 0; i < media->frames(); ++i)
    {
        media->samples.setSample(0, i, .18f * (float) std::sin(i * .07) + .07f * (float) std::sin(i * .63));
        media->samples.setSample(1, i, .16f * (float) std::sin(i * .13) - .08f * (float) std::sin(i * .63));
    }
    Clip clip; clip.media = media; clip.length = 2; p = Project(); p.masterDb = 0; p.clips = {clip};
    Engine dry; dry.sampleRate = 8000; dry.project = p; juce::AudioBuffer<float> plain(2, 512), altered(2, 512); juce::MidiBuffer midi;
    dry.render(plain, 512, 0, midi); bool exact = true;
    for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 512; ++i) exact &= plain.getSample(ch, i) == media->samples.getSample(ch, i);
    check(exact, "master bypass defaults preserve every original PCM sample exactly");
    Engine mono; mono.sampleRate = 8000; mono.project = p; mono.project.mastering.width = 0; mono.render(altered, 512, 0, midi);
    bool monoExact = true; for (int i = 0; i < 512; ++i) monoExact &= altered.getSample(0, i) == altered.getSample(1, i);
    check(monoExact && altered.getMagnitude(0, 512) > .01, "master width zero collapses the actual stereo renderer to mono");
    Engine projected; projected.sampleRate = 8000; projected.project = p; auto alternate = p; alternate.mastering.width = 0;
    const auto liveModel = json(projected.project); projected.render(altered, 512, 0, midi, &alternate); bool projectionMono = true;
    for (int i = 0; i < 512; ++i) projectionMono &= altered.getSample(0, i) == altered.getSample(1, i);
    check(projectionMono && json(projected.project) == liveModel, "explicit rehearsal/render project owns master settings without swapping the live project");
    auto lowMedia = std::make_shared<Media>(); lowMedia->sampleRate = 8000; lowMedia->samples.setSize(1, 16000);
    for (int i = 0; i < lowMedia->frames(); ++i) lowMedia->samples.setSample(0, i, .2f * (float) std::sin(i * 2 * juce::MathConstants<double>::pi * 40 / 8000));
    auto lowClip = clip; lowClip.media = lowMedia; Engine flatLow, raisedLow; flatLow.sampleRate = raisedLow.sampleRate = 8000;
    flatLow.project = raisedLow.project = p; flatLow.project.clips = raisedLow.project.clips = {lowClip}; raisedLow.project.mastering.eqDb[0] = 6;
    double flatEnergy = 0, raisedEnergy = 0;
    for (int block = 0; block < 16; ++block)
    {
        flatLow.render(plain, 512, block * 512 / 8000., midi); raisedLow.render(altered, 512, block * 512 / 8000., midi);
        if (block >= 8) for (int i = 0; i < 512; ++i) { flatEnergy += plain.getSample(0, i) * plain.getSample(0, i); raisedEnergy += altered.getSample(0, i) * altered.getSample(0, i); }
    }
    const auto lowGainDb = 10 * std::log10(raisedEnergy / flatEnergy);
    check(lowGainDb > 5.5 && lowGainDb < 6.2 && raisedLow.masterInputPeak < .201f && raisedLow.masterPeak > .35f,
          "master low shelf changes actual low-frequency PCM and exposes before/after levels: " + juce::String(lowGainDb, 3) + " dB");
    Engine full, chunks; full.sampleRate = chunks.sampleRate = 8000; full.project = chunks.project = p;
    full.project.mastering = chunks.project.mastering = settings;
    full.project.masterAutomation.enabled = chunks.project.masterAutomation.enabled = true;
    full.project.masterAutomation.setPoint(0, -3); full.project.masterAutomation.setPoint(2, -9); chunks.project.masterAutomation = full.project.masterAutomation;
    juce::AudioBuffer<float> whole(2, 2048), piece(2, 512);
    for (int offset = 0; offset < 2048; offset += 512)
    {
        full.render(piece, 512, offset / 8000., midi);
        for (int ch = 0; ch < 2; ++ch) whole.copyFrom(ch, offset, piece, ch, 0, 512);
    }
    double difference = 0;
    for (int offset = 0; offset < 2048;)
    {
        const int count = std::min(2048 - offset, offset % 2 ? 79 : 127); chunks.render(piece, count, offset / 8000., midi);
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < count; ++i) difference = std::max(difference, (double) std::abs(whole.getSample(ch, offset + i) - piece.getSample(ch, i)));
        offset += count;
    }
    check(difference < 3e-6 && whole.getMagnitude(0, 2048) > .01, "master EQ, width, gain automation and saturation are callback-size independent: " + juce::String(difference, 9));
    full.seek(0); full.render(altered, 512, 0, midi); bool resetExact = true;
    for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 512; ++i) resetExact &= altered.getSample(ch, i) == whole.getSample(ch, i);
    check(resetExact, "seek resets master filter history for deterministic rendering");
    Engine hot; hot.sampleRate = 8000; hot.project = p; hot.project.masterDb = 12; hot.project.clips.clear();
    for (int track = 0; track < trackCount; ++track) { auto loud = clip; loud.id = juce::Uuid().toString(); loud.track = track; loud.controls.gainDb = 6; hot.project.clips.push_back(loud); }
    hot.render(altered, 512, 0, midi); check(hot.masterClipped && hot.masterInputPeak > 1 && hot.masterPeak > 1, "master meters distinguish bus input, output and actual clipping risk");
    hot.masterClipped = false; hot.project.mastering.saturation = true; hot.project.mastering.saturationDriveDb = 6; hot.seek(0); hot.render(altered, 512, 0, midi);
    check(!hot.masterClipped && altered.getMagnitude(0, 512) <= 1 && hot.masterInputPeak > 1, "optional soft saturation bounds hot output while retaining the input measurement");
    {
        juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream = media->file.createOutputStream();
        auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(2).withBitsPerSample(24));
        check(writer && writer->writeFromAudioSampleBuffer(media->samples, 0, media->frames()), "write owned stereo PCM for the actual mastering save/export workflow");
    }
    const auto loaded = readMedia(full.formats, media->file);
    check(loaded.media != nullptr, "production loader opens the mastering workflow source: " + loaded.error);
    if (!loaded.media) return;
    loaded.media->manualBpm = 120; loaded.media->manualBeatOrigin = 0;
    full.project.clips[0].media = loaded.media; full.library = {loaded.media};
    const auto session = root.getChildFile("mastered.mixworks"); Engine reopened; reopened.sampleRate = 8000;
    check(full.saveProject(session).isEmpty() && reopened.restoreProject(session.loadFileAsString()).isEmpty()
          && reopened.project.mastering.eqDb == settings.eqDb && reopened.project.mastering.width == settings.width && reopened.project.masterAutomation.active(),
          "production session save/reopen retains all master tone settings and gain automation");
    const auto aFile = root.getChildFile("mastered-original.wav"), bFile = root.getChildFile("mastered-reopened.wav");
    full.monitorMuted = true;
    const auto aError = full.exportWav(aFile), bError = reopened.exportWav(bFile);
    auto a = std::unique_ptr<juce::AudioFormatReader>(full.formats.createReaderFor(aFile));
    auto b = std::unique_ptr<juce::AudioFormatReader>(full.formats.createReaderFor(bFile));
    bool sameWav = aError.isEmpty() && bError.isEmpty() && a && b && a->lengthInSamples == 16000 && b->lengthInSamples == 16000;
    float exportDifference = 0, exportedPeak = 0;
    if (sameWav)
    {
        juce::AudioBuffer<float> leftAudio(2, 512), rightAudio(2, 512);
        for (int offset = 0; offset < 16000; offset += 512)
        {
            const int count = std::min(512, 16000 - offset); sameWav &= a->read(&leftAudio, 0, count, offset, true, true) && b->read(&rightAudio, 0, count, offset, true, true);
            exportedPeak = std::max(exportedPeak, leftAudio.getMagnitude(0, count));
            for (int ch = 0; ch < 2; ++ch) for (int frame = 0; frame < count; ++frame)
                exportDifference = std::max(exportDifference, std::abs(leftAudio.getSample(ch, frame) - rightAudio.getSample(ch, frame)));
        }
    }
    check(sameWav && exportDifference == 0 && exportedPeak > .01f, "muted original and reopened sessions export exact identical non-silent mastered WAV PCM");
    juce::AudioBuffer<float> offlineFirst(2, 512); const bool readFirst = a && a->read(&offlineFirst, 0, 512, 0, true, true);
    a.reset(); b.reset();
    full.seek(0); full.render(plain, 512, 0, midi); reopened.seek(0); reopened.render(altered, 512, 0, midi); bool postExport = true;
    for (int ch = 0; ch < 2; ++ch) for (int frame = 0; frame < 512; ++frame) postExport &= plain.getSample(ch, frame) == altered.getSample(ch, frame);
    check(postExport, "export and seek leave master processing reset consistently for resumed playback");
    float liveExportError = 0; for (int ch = 0; ch < 2; ++ch) for (int frame = 0; frame < 512; ++frame)
        liveExportError = std::max(liveExportError, std::abs(plain.getSample(ch, frame) - offlineFirst.getSample(ch, frame)));
    check(readFirst && liveExportError < 2e-7f, "actual mastered WAV agrees with live-render PCM within 24-bit quantization: " + juce::String(liveExportError, 9));
    Engine monitored, control; monitored.sampleRate = control.sampleRate = 8000; monitored.project = control.project = full.project;
    monitored.monitorMuted = true; monitored.spectrum.enabled = true; monitored.playing = true;
    float left[512] {}, right[512] {}; float* outputs[] {left, right}; monitored.audioDeviceIOCallbackWithContext(nullptr, 0, outputs, 2, 512, {});
    control.render(plain, 512, 0, midi); std::array<double, 512> rates {};
    const int received = monitored.spectrum.read(altered.getWritePointer(0), altered.getWritePointer(1), rates.data(), 512); bool sameFeed = received == 512;
    for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < received; ++i) sameFeed &= plain.getSample(ch, i) == altered.getSample(ch, i) && rates[(size_t) i] == 8000;
    check(sameFeed && juce::FloatVectorOperations::findMinAndMax(left, 512) == juce::Range<float>() && monitored.position > 0,
          "real callback feeds post-master PCM and rate before monitor mute while playback continues");

    Workstation ui(false); ui.stopTimer(); ui.engine.project = p; ui.engine.sampleRate = 8000;
    ui.workspacePreference = root.getChildFile("workspace.json"); ui.refresh(); const auto beforeOpen = json(ui.engine.project);
    ui.addToDesktop(0); ui.setVisible(true); ui.masterPanelButton->onClick();
    check(ui.masterWindow && ui.masterWindow->isVisible() && ui.masterPanelButton->getBounds().getRight() <= ui.laneAmount.getX()
          && ui.masterPanelButton->getAccessibilityHandler() && json(ui.engine.project) == beforeOpen,
          "native timeline hexagon opens the master panel without obscuring lane count or changing the mix; visible=" + juce::String((int) (ui.masterWindow && ui.masterWindow->isVisible()))
          + ", geometry=" + juce::String((int) (ui.masterPanelButton->getBounds().getRight() <= ui.laneAmount.getX())) + ", accessible=" + juce::String((int) (ui.masterPanelButton->getAccessibilityHandler() != nullptr))
          + ", sameModel=" + juce::String((int) (json(ui.engine.project) == beforeOpen)));
    if (!ui.masterWindow) return;
    auto* panel = ui.masterWindow->getContentComponent();
    auto* low = dynamic_cast<juce::Slider*>(panel->findChildWithID("master-low"));
    auto* width = dynamic_cast<juce::Slider*>(panel->findChildWithID("master-width"));
    auto* drive = dynamic_cast<juce::Slider*>(panel->findChildWithID("master-drive"));
    auto* gain = dynamic_cast<juce::Slider*>(panel->findChildWithID("master-gain"));
    auto* saturation = dynamic_cast<juce::ToggleButton*>(panel->findChildWithID("master-saturation"));
    auto* mute = dynamic_cast<juce::ToggleButton*>(panel->findChildWithID("master-monitor-mute"));
    auto* reset = dynamic_cast<juce::TextButton*>(panel->findChildWithID("master-reset-tone"));
    check(low && width && drive && gain && saturation && mute && reset, "master panel exposes all native tone, gain and monitoring controls");
    if (!low || !width || !drive || !gain || !saturation || !mute || !reset) return;
    low->setValue(6, juce::sendNotificationSync); width->setValue(150, juce::sendNotificationSync);
    saturation->setToggleState(true, juce::dontSendNotification); saturation->onClick(); drive->setValue(3, juce::sendNotificationSync); gain->setValue(-6, juce::sendNotificationSync);
    check(ui.engine.project.mastering.eqDb[0] == 6 && ui.engine.project.mastering.width == 1.5f && ui.engine.project.mastering.saturation
          && ui.engine.project.mastering.saturationDriveDb == 3 && ui.engine.project.masterDb == -6 && ui.undoHistory.size() == 5,
          "native master controls write one owned change each and synchronize the shared gain");
    reset->onClick();
    check(ui.engine.project.mastering.eqDb == std::array<float, 3>{} && ui.engine.project.mastering.width == 1 && !ui.engine.project.mastering.saturation && ui.engine.project.masterDb == -6,
          "Reset tone restores bypass settings without changing master gain");
    ui.undo(false); check(ui.engine.project.mastering.eqDb[0] == 6 && ui.engine.project.mastering.saturation, "Undo restores the complete master treatment");
    const auto dragUndo = ui.undoHistory.size(); low->onDragStart(); low->setValue(5, juce::sendNotificationSync); low->setValue(7, juce::sendNotificationSync); low->onDragEnd();
    check(ui.undoHistory.size() == dragUndo + 1, "one master EQ drag produces one Undo entry");
    ui.engine.setAutomationRecordMode(AutomationRecordMode::touch); ui.engine.armAutomation(-1, true); ui.engine.playing = true; ui.engine.position = .5;
    const auto takeUndo = ui.undoHistory.size(); gain->setValue(-5, juce::sendNotificationSync); ui.engine.position = .75; ui.engine.advanceAutomation(.75); gain->setValue(-8, juce::sendNotificationSync); ui.engine.setPlaying(false);
    check(ui.engine.project.masterAutomation.active() && ui.undoHistory.size() == takeUndo + 1, "master-panel gain records through the existing automation owner with one take Undo");
    const auto beforeMute = json(ui.engine.project); const auto muteUndo = ui.undoHistory.size(); mute->setToggleState(true, juce::dontSendNotification); mute->onClick(); ui.saveWorkspacePreferences();
    WorkspaceLayout preference;
    check(ui.engine.monitorMuted && ui.monitorMute.getToggleState() && readWorkspacePreferences(ui.workspacePreference, preference).wasOk() && preference.monitorMuted
          && json(ui.engine.project) == beforeMute && ui.undoHistory.size() == muteUndo, "master-panel mute uses the same persistent monitoring owner without a musical edit");
    const auto beforeBusy = json(ui.engine.project); ui.setBusy(true); low->setValue(4, juce::sendNotificationSync);
    check(json(ui.engine.project) == beforeBusy && !panel->isEnabled(), "exclusive workflows disable master editing without a partial setting change"); ui.setBusy(false);
    ui.engine.playing = true; float outL[512] {}, outR[512] {}; float* monitorOutputs[] {outL, outR};
    for (int block = 0; block < 8; ++block) ui.engine.audioDeviceIOCallbackWithContext(nullptr, 0, monitorOutputs, 2, 512, {});
    struct Capture : juce::Timer { std::function<void()> done; void timerCallback() override { stopTimer(); done(); } } capture;
    int capturePhase = 0;
    auto warmPixels = [](const juce::Image& image)
    {
        int count = 0;
        for (int y = 0; y < image.getHeight(); ++y) for (int x = 0; x < image.getWidth(); ++x)
        { const auto c = image.getPixelAt(x, y); count += c.getRed() > 150 && c.getRed() > c.getGreen() + 10 && c.getGreen() > c.getBlue() + 10; }
        return count;
    };
    capture.done = [&]
    {
        if (capturePhase == 1)
        {
            ui.masterPanelButton->onClick(); auto* reopenedGraph = panel->findChildWithID("master-spectrum");
            check(warmPixels(reopenedGraph->createComponentSnapshot(reopenedGraph->getLocalBounds())) == 0,
                  "reopening a stopped spectrum cannot present stale audio as a current trace");
            ui.masterWindow->closeButtonPressed(); ui.removeFromDesktop(); juce::MessageManager::getInstance()->stopDispatchLoop(); return;
        }
        auto* graph = panel->findChildWithID("master-spectrum"); const auto image = graph->createComponentSnapshot(graph->getLocalBounds()); int orangePixels = 0;
        for (int y = 0; y < image.getHeight(); ++y) for (int x = 0; x < image.getWidth(); ++x)
        { const auto c = image.getPixelAt(x, y); orangePixels += c.getRed() > 150 && c.getRed() > c.getGreen() + 10 && c.getGreen() > c.getBlue() + 10; }
        check(orangePixels > 20 && juce::FloatVectorOperations::findMinAndMax(outL, 512) == juce::Range<float>(), "native spectrum plots actual processed audio while monitor output stays muted");
        auto stream = root.getChildFile("master-panel-native.png").createOutputStream(); juce::PNGImageFormat png;
        check(stream && png.writeImageToStream(panel->createComponentSnapshot(panel->getLocalBounds()), *stream), "capture native master mixer and spectrum");
        ui.masterWindow->setSize(800, 620);
        check(panel->getLocalBounds().contains(drive->getBounds()) && panel->getLocalBounds().contains(reset->getBounds()), "small master window retains drive and reset controls");
        const auto closing = json(ui.engine.project); ui.masterWindow->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));
        check(!ui.masterWindow->isVisible() && !ui.engine.spectrum.enabled && json(ui.engine.project) == closing, "Escape closes the master panel and stops spectrum work without changing the mix");
        capturePhase = 1; capture.startTimer(150);
    };
    capture.startTimer(300); juce::MessageManager::getInstance()->runDispatchLoop();

}
}
