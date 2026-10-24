#include "Workstation.h"
#include <cmath>
#include <limits>
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace mixworks
{
void runLaunchTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    const auto json = [](const Project& p) { return juce::JSON::toString(projectToJson(p), true, 17); };
    const auto closeEnough = [](double a, double b) { return std::abs(a - b) < 1.e-8; };
    TempoMap clock;
    for (auto timing : {LaunchTiming::nextBeat, LaunchTiming::nextBar, LaunchTiming::nextPhrase})
    {
        LaunchSettings settings{timing, 16};
        const double step = timing == LaunchTiming::nextBeat ? 1. : timing == LaunchTiming::nextBar ? 4. : 64.;
        const double edge = step / 2;
        const auto exact = nextLaunchBoundary(clock, settings, edge);
        const auto before = nextLaunchBoundary(clock, settings, edge - 1.e-6);
        const auto after = nextLaunchBoundary(clock, settings, edge + 1.e-6);
        check(exact && before && after && closeEnough(exact->time, edge * 2) && closeEnough(before->time, edge) && closeEnough(after->time, edge * 2),
            "launch timing selects strictly future exact/closeEnough edges / " + juce::String(launchTimingName(timing)));
    }
    for (int bars : {8, 16, 32})
    {
        clock.origin = .217;
        const auto boundary = nextLaunchBoundary(clock, {LaunchTiming::nextPhrase, bars}, 1);
        const auto preroll = nextLaunchBoundary(clock, {LaunchTiming::nextPhrase, bars}, .1);
        check(boundary && preroll && closeEnough(boundary->time, .217 + bars * 2) && closeEnough(preroll->time, .217),
            "phrase grid is anchored to project bar1, including pre-roll / " + juce::String(bars));
    }
    clock = {120, {{0,120,false},{2,150,true},{5,90,false},{5,130,false},{12,100,true}}, .217};
    bool integrated = true;
    for (double now = .001; now < 30; now += .013)
    {
        const auto boundary = nextLaunchBoundary(clock, {LaunchTiming::nextBar,16}, now);
        integrated &= boundary && boundary->time > now && closeEnough(clock.beatsAt(boundary->time), boundary->beat)
            && closeEnough(boundary->beat / 4, std::round(boundary->beat / 4));
    }
    check(integrated, "shared launch clock follows integrated ramps, held cuts and coincident tempo markers");
    auto invalid = clock; invalid.points[1].bpm = std::numeric_limits<double>::quiet_NaN();
    const auto explicitImmediate = nextLaunchBoundary(invalid, {LaunchTiming::immediate,16}, 1.125);
    check(!nextLaunchBoundary(invalid, {}, 1) && explicitImmediate && explicitImmediate->time == 1.125
        && !nextLaunchBoundary(clock, {(LaunchTiming) 99,16},1) && !nextLaunchBoundary(clock, {LaunchTiming::nextBar,4},1)
        && !nextLaunchBoundary(clock, {}, std::numeric_limits<double>::infinity()), "invalid quantised clock/settings fail; only explicitly chosen Immediate bypasses grid availability");

    auto source = std::make_shared<Media>(); source->name = "Scheduled pause source"; source->sampleRate = 8000;
    source->manualBpm = 120; source->manualBeatOrigin = 0; source->analysis.downbeatOrigin = 0;
    source->samples.setSize(1, 96 * 8000); source->file = report.getSiblingFile("scheduled-pause-source.wav");
    for (int n = 0; n < source->frames(); ++n) source->samples.setSample(0,n,(float) (.2 + .1 * std::sin(n * .137)));
    {
        juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream = source->file.createOutputStream();
        auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(1).withBitsPerSample(24));
        check(writer && writer->writeFromAudioSampleBuffer(source->samples,0,source->frames()), "write owned launch-clock source for actual Save/Open");
    }
    source->analysisSource = {audioDigest(source->file),source->file.getSize(),8000,source->frames(),1};
    source->sourceModified = source->file.getLastModificationTime().toMilliseconds();
    Project base; base.masterDb = 0; base.transitionLinksKnown = true; base.followTrackTempo = false;
    Clip clip; clip.media = source; clip.length = 96; base.clips = {clip};
    for (double rate : {8000.,44100.,48000.}) for (int block : {1,31,127,512})
    {
        auto owned = std::make_unique<Engine>(juce::File(),false); auto& engine = *owned;
        auto reference = std::make_unique<Engine>(juce::File(),false);
        engine.project = reference->project = base; engine.sampleRate = reference->sampleRate = rate;
        const double start = 1.900013; engine.position = start; engine.setPlaying(true);
        const auto before = json(engine.project); const auto admitted = engine.toggleScheduledPause();
        const auto queued = engine.scheduledPauseStatus();
        const auto expectedFrames = (int) std::ceil((2 - start) * rate - 1.e-6);
        juce::AudioBuffer<float> actual(2,block), expected(2,block); juce::MidiBuffer midi;
        int rendered = 0; bool pcm = true;
        while (engine.playing && rendered < expectedFrames + block)
        {
            const double at = engine.position.load(); reference->render(expected,block,at,midi);
            auto* outputs = actual.getArrayOfWritePointers();
            engine.audioDeviceIOCallbackWithContext(nullptr,0,outputs,2,block,{});
            const int audible = std::min(block, expectedFrames - rendered);
            for (int ch = 0; ch < 2; ++ch) for (int n = 0; n < block; ++n)
                pcm &= actual.getSample(ch,n) == (n < audible ? expected.getSample(ch,n) : 0.f);
            rendered += audible;
        }
        const auto stopped = engine.scheduledPauseStatus();
        check(admitted.wasOk() && queued.state == ScheduledPauseState::pending && queued.boundary == 2 && pcm
            && rendered == expectedFrames && !engine.playing && stopped.state == ScheduledPauseState::paused
            && closeEnough(engine.position, start + expectedFrames / rate) && engine.position >= 2 - 1.e-10 && engine.position < 2 + 1. / rate
            && json(engine.project) == before,
            "device callback pauses within one sample, identical PCM prefix/silent suffix, unchanged document / " + juce::String(rate,0) + " Hz / block " + juce::String(block));
        engine.serviceScheduledPause();
    }
    auto owned = std::make_unique<Engine>(juce::File(),false); auto& engine = *owned; engine.project = base;
    auto queue = [&]
    {
        engine.stop(); engine.project = base; engine.sampleRate = 8000; engine.position = 1.49; engine.setPlaying(true);
        return engine.toggleScheduledPause().wasOk();
    };
    check(!engine.toggleScheduledPause().wasOk() && queue() && engine.toggleScheduledPause().wasOk()
        && engine.scheduledPauseStatus().state == ScheduledPauseState::cancelled && engine.playing && engine.position == 1.49,
        "stopped scheduling rejects and same pending pause toggles cancellation without transport movement");
    for (int kind = 0; kind < 9; ++kind)
    {
        const bool admitted = queue();
        switch (kind)
        {
            case 0: engine.seek(1.6); break;
            case 1: engine.setPlaying(false); break;
            case 2: engine.project.tempo.origin += .1; break;
            case 3: engine.project.launch.phraseBars = 8; break;
            case 4: engine.sampleRate = 48000; break;
            case 5: engine.project.clips.clear(); break;
            case 6: engine.exporting = true; break;
            case 7: engine.position = 3; break;
            case 8: engine.audioDeviceStopped(); break;
        }
        engine.serviceScheduledPause();
        check(admitted && engine.scheduledPauseStatus().state == ScheduledPauseState::cancelled && engine.scheduledPauseStatus().reason != ScheduledPauseReason::none,
            "queued pause cancels explicit transport/grid/settings/device/content/export changes without rolling forward / " + juce::String(kind));
        engine.exporting = false;
    }
    check(queue() && engine.setLaunchSettings({LaunchTiming::nextBeat,8}).wasOk()
        && engine.scheduledPauseStatus().state == ScheduledPauseState::cancelled, "canonical timing setter cancels pending action");
    engine.setLaunchSettings({LaunchTiming::immediate,16}); const auto immediateAt = engine.position.load();
    check(engine.toggleScheduledPause().wasOk() && !engine.playing && engine.position == immediateAt, "explicit Immediate pause uses canonical pause and never returns to mix start");
    check(queue(), "admit exclusive-interruption fixture");
    juce::AudioBuffer<float> exclusive(2,64); auto* exclusiveOutputs = exclusive.getArrayOfWritePointers();
    engine.exporting = true; engine.audioDeviceIOCallbackWithContext(nullptr,0,exclusiveOutputs,2,64,{}); engine.exporting = false;
    engine.serviceScheduledPause();
    check(engine.scheduledPauseStatus().state == ScheduledPauseState::cancelled && engine.playing && engine.position == 1.49,
        "an export interruption observed by the device callback cancels after export ends, never silently resuming a stale queue");
    engine.project = base; engine.project.clips[0].length = 1.6; engine.position = 1.49; engine.setPlaying(true);
    check(engine.toggleScheduledPause().failed() && engine.playing && engine.position == 1.49, "unfitting next bar rejects instead of silently choosing earlier timing");
    check(queue(), "admit recording pause fixture");
    engine.project.launch.timing = LaunchTiming::nextBeat; engine.cancelScheduledPause(); engine.toggleScheduledPause();
    engine.setAutomationRecordMode(AutomationRecordMode::write); engine.armAutomation(0,true);
    engine.setControlValue({0,AutomationTarget::volume},-6);
    juce::AudioBuffer<float> out(2,512); auto* outputs = out.getArrayOfWritePointers();
    engine.audioDeviceIOCallbackWithContext(nullptr,0,outputs,2,512,{});
    const auto actualStop = engine.position.load(); const bool deferred = engine.recordingAutomation();
    engine.setPlaying(true); // Must finish the paused take before starting another.
    const auto& points = engine.project.tracks[0].automation[0].points;
    check(deferred && !engine.recordingAutomation() && engine.playing
        && std::any_of(points.begin(),points.end(),[&](const auto& p) { return closeEnough(p.time,actualStop) && p.value == -6; }),
        "callback defers automation mutation; canonical next start finishes the take at the stopped sample cursor first");

    auto doc = projectToJson(base); doc.getDynamicObject()->setProperty("version",32); doc.getDynamicObject()->removeProperty("launch");
    const auto legacy = parseProject(juce::JSON::toString(doc,true,17));
    check(legacy.error.isEmpty() && legacy.project.launch == LaunchSettings{}, "legacy32 session defaults Next bar/16 bars without an active queue");
    bool strict = true;
    for (int kind = 0; kind < 5; ++kind)
    {
        auto bad = projectToJson(base);
        if (kind == 0) bad.getDynamicObject()->removeProperty("launch");
        if (kind == 1) bad["launch"].getDynamicObject()->setProperty("timing",9);
        if (kind == 2) bad["launch"].getDynamicObject()->setProperty("phraseBars",4);
        if (kind == 3) bad["launch"].getDynamicObject()->setProperty("timing",true);
        if (kind == 4) bad["launch"].getDynamicObject()->setProperty("phraseBars",16.0);
        strict &= !parseProject(juce::JSON::toString(bad,true,17)).error.isEmpty();
    }
    check(strict, "session33 strictly admits launch enum/phrase integers and requires complete settings");
    auto workspace = std::make_unique<Workstation>(false); auto& ui = *workspace; ui.stopTimer(); ui.setSize(1920,1080);
    ui.engine.project = base; ui.engine.library = {source}; ui.selectedClip = clip.id; ui.selectTrack(0); ui.refresh();
    ui.addToDesktop(0); ui.setVisible(true); ui.dirty = false; auto& deck = *ui.performance;
    const auto history = ui.undoHistory.size(); deck.performLaunchMenu(203,{});
    check(ui.engine.project.launch.timing == LaunchTiming::nextPhrase && ui.undoHistory.size() == history + 1 && ui.dirty,
        "real Controller launch menu changes saved timing in one Undo");
    const auto current = ui.engine.project.launch; deck.performLaunchMenu(203,current); deck.performLaunchMenu(200,{});
    check(ui.engine.project.launch == current && ui.undoHistory.size() == history + 1, "unchanged/stale launch menus preserve settings and history");
    ui.undo(false); check(ui.engine.project.launch == LaunchSettings{}, "Undo restores prior launch settings");
    ui.engine.position = 1.49; ui.engine.setPlaying(true); ui.refresh(); ui.dirty = false;
    const auto document = json(ui.engine.project); const auto pauseHistory = ui.undoHistory.size();
    auto* pauseAccess = deck.buttons[6].getAccessibilityHandler();
    if (pauseAccess) pauseAccess->getActions().invoke(juce::AccessibilityActionType::press);
    const auto until = juce::Time::getMillisecondCounterHiRes() + 40;
    do {
#if JUCE_WINDOWS
        MSG message; while (PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
#endif
        juce::Thread::sleep(1);
    } while (juce::Time::getMillisecondCounterHiRes() < until);
    check(pauseAccess && ui.engine.scheduledPauseStatus().state == ScheduledPauseState::pending && deck.buttons[6].getTitle().contains("Next bar")
        && json(ui.engine.project) == document && ui.undoHistory.size() == pauseHistory && !ui.dirty,
        "actual native pause button queues explicit programme action without document/history changes");
    deck.performLaunchMenu(400,ui.engine.project.launch);
    check(ui.engine.scheduledPauseStatus().state == ScheduledPauseState::cancelled && ui.engine.playing, "real launch menu explicitly cancels queue without pausing");
    ui.engine.setPlaying(false); ui.engine.setLaunchSettings({LaunchTiming::nextPhrase,8});
    const auto session = report.getSiblingFile("scheduled-pause.mixworks"); const auto saved = ui.engine.saveProject(session);
    auto reopened = std::make_unique<Engine>(juce::File(),false); const auto opened = reopened->restoreProject(session.loadFileAsString());
    check(saved.isEmpty() && opened.isEmpty() && reopened->project.launch == ui.engine.project.launch && !reopened->playing
        && reopened->scheduledPauseStatus().state == ScheduledPauseState::idle, "actual Save/Open33 retains timing/phrase while restoring transport and queue silently: " + saved + opened);
    ui.engine.project.launch = {}; ui.engine.position = 1.49; ui.engine.setPlaying(true); ui.engine.toggleScheduledPause();
    for (int width : {1250,1920})
    {
        ui.setSize(width,1080); ui.refresh(); auto stream = report.getSiblingFile("scheduled-pause-" + juce::String(width) + ".png").createOutputStream(); juce::PNGImageFormat png;
        check(stream && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()),*stream), "capture queued pause timing/boundary and actual controls at width " + juce::String(width));
        if (width == 1250)
        {
            ui.performanceViewport.setViewPosition(deck.getWidth(),0);
            auto controlsStream = report.getSiblingFile("scheduled-pause-1250-controls.png").createOutputStream();
            check(controlsStream && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()),*controlsStream), "capture compact horizontally revealed scheduled-pause controls and boundary");
        }
    }
    ui.engine.setPlaying(false);
}
}
