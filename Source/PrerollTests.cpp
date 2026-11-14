#include "Workstation.h"
#include "Beatmatch.h"
#include "ElasticAudio.h"
#include <juce_cryptography/juce_cryptography.h>
#include <cmath>
#include <limits>

namespace mixworks
{
void runPrerollTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    auto near = [](double a, double b) { return std::abs(a - b) < 1.e-7; };
    auto json = [](const Project& project) { return juce::JSON::toString(projectToJson(project)); };
    for (bool ramp : {false, true})
    {
        TempoMap clock {131, {{0, 131, false}, {12, ramp ? 150. : 131., true}}, 4};
        bool inverse = near(clock.beatsAt(4), 0) && near(clock.timeAtBeat(0), 4);
        for (double time = 0; time < 24; time += .137) inverse &= near(clock.timeAtBeat(clock.beatsAt(time)), time);
        check(inverse, "pre-roll musical beat integration/inverse agrees before and after zero, ramp " + juce::String((int) ramp));
        check(clock.beatsAt(1) < 0 && clock.snap(1.01, true) >= 0, "negative musical beats remain playable nonnegative engine times");
        Project project; project.tempo = clock; project.tempo.origin = 0;
        auto media = std::make_shared<Media>(); media->sampleRate = 8000; media->manualBpm = 131; media->manualBeatOrigin = .19;
        media->samples.setSize(1, 8000 * 24); media->samples.clear();
        Clip clip; clip.media = media; clip.start = 1; clip.length = 8; clip.beatmatched = true;
        clip.tempoMap = std::make_shared<const TempoMap>(project.tempo); clip.tempoSourceBpm = 131;
        clip.rate = clip.sourceElapsed(clip.length) / clip.length;
        auto payload = std::make_shared<PreparedAudio>(); payload->samples.setSize(1, 1024); payload->samples.clear(); clip.prepared = payload;
        clip.controls.automation[1].enabled = true; clip.controls.automation[1].points = {{0, -60, .5}, {3, 0}};
        auto originalElastic = std::make_shared<const ElasticSource>(clip);
        auto firstSplit = clip.slice(1, 3); firstSplit.id = juce::Uuid().toString(); firstSplit.track = 1;
        auto secondSplit = clip.slice(4, 3); secondSplit.id = juce::Uuid().toString(); secondSplit.track = 2;
        firstSplit.elasticPlayback = secondSplit.elasticPlayback = true;
        firstSplit.elasticSource = secondSplit.elasticSource = originalElastic;
        project.clips = {clip, firstSplit, secondSplit}; project.masterAutomation.enabled = true; project.masterAutomation.points = {{0, -3}, {5, 0}};
        project.labels.push_back({juce::Uuid().toString(), .4, "Intro"});
        check(project.setTimelineOrigin(4).wasOk(), "enable real pre-roll on a captured source clock");
        const auto& moved = project.clips[0]; bool retained = moved.media == media && moved.prepared == payload && moved.effects == clip.effects
            && near(moved.start, 5) && moved.offset == clip.offset && moved.controls.automation[1].points[0].bend == .5
            && near(project.labels[0].time, 4.4) && near(project.masterAutomation.points[0].time, 4);
        for (double local = -.5; local <= 8; local += .173) retained &= near(moved.sourceElapsed(local), clip.sourceElapsed(local));
        check(retained, "rebase preserves source/audio preparation/effects/curves while moving all session-owned clocks together");
        bool elasticSame = project.clips[1].elasticSource == project.clips[2].elasticSource && project.clips[1].elasticSource != originalElastic;
        for (double time = 1; time < 8; time += .173)
            elasticSame &= near(project.clips[1].elasticSource->progressAt(time + 4), originalElastic->progressAt(time));
        for (double source = 0; source < 8; source += .137)
            elasticSame &= near(project.clips[1].elasticSource->timeAtProgress(source), originalElastic->timeAtProgress(source) + 4);
        check(elasticSame && originalElastic->clip.start == 1, "shared split elastic clocks rebase their original mapping without changing source progress or the old snapshot");
        auto preZero = moved; preZero.moveTo(3);
        bool sourceInverse = preZero.start < preZero.tempoMap->origin;
        for (double source = -.7; source < 8; source += .137) sourceInverse &= near(preZero.sourceElapsed(preZero.localTimeForSource(source)), source);
        check(sourceInverse, "captured source clocks invert correctly when the clip is before musical zero");
    }
    const double bpm = 131, beat = 60 / bpm, attack = 5.5 * beat;
    auto media = std::make_shared<Media>(); media->sampleRate = 8000; media->manualBpm = bpm; media->manualBeatOrigin = attack;
    media->name = "Quiet intro / chosen downbeat"; media->file = report.getSiblingFile("preroll-source.wav");
    media->samples.setSize(1, 12 * 8000);
    for (int i = 0; i < media->frames(); ++i) media->samples.setSample(0, i, (i < attack * 8000 ? .03f : .3f) * (float) std::sin(i * 2 * juce::MathConstants<double>::pi * 440 / 8000));
    {
        std::unique_ptr<juce::OutputStream> stream = media->file.createOutputStream(); juce::WavAudioFormat wav;
        auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(1).withBitsPerSample(24));
        check(writer && writer->writeFromAudioSampleBuffer(media->samples, 0, media->frames()), "write disposable full-intro pre-roll fixture");
    }
    const auto sourceHash = juce::SHA256(media->file).toHexString();
    auto workstation = std::make_unique<Workstation>(false); auto& ui = *workstation; ui.stopTimer(); ui.setSize(1440, 940);
    ui.engine.project = {}; ui.engine.project.followTrackTempo = false; ui.engine.project.bpm = ui.engine.project.tempo.fallback = bpm; ui.engine.project.masterDb = 0;
    Clip clip; clip.media = media; clip.length = 12;
    clip.controls.automation[(size_t) AutomationTarget::pan].enabled = true;
    clip.controls.automation[(size_t) AutomationTarget::pan].points = {{0, 0}, {12, 0}};
    ui.engine.project.clips = {clip}; ui.engine.library = {media}; ui.selectedClip = clip.id; ui.undoHistory.clear(); ui.redoHistory.clear(); ui.refresh();
    const auto original = json(ui.engine.project);
    juce::AudioBuffer<float> before(2, 256), after(2, 256); juce::MidiBuffer midi;
    ui.engine.resetPlugins(); ui.engine.render(before, 256, .4, midi);
    ui.menuItemSelected(2491, 4);
    const double origin = ui.engine.project.tempo.origin;
    check(near(origin, 8 * beat) && near(ui.engine.project.clips[0].start, origin) && ui.undoHistory.size() == 1,
        "View Add 2 bars rebases the full existing song as one Undo");
    ui.engine.resetPlugins(); ui.engine.render(after, 256, origin + .4, midi);
    bool pcmSame = true; for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 256; ++i) pcmSame &= std::abs(before.getSample(ch, i) - after.getSample(ch, i)) < 1.e-5f;
    check(pcmSame, "adding pre-roll preserves audible PCM and the complete quiet intro");
    const auto rebased = json(ui.engine.project);
    const double positionBeforeUndo = origin + .4, rateBeforeUndo = ui.engine.sampleRate.load();
    ui.engine.position = positionBeforeUndo;
    ui.engine.sampleRate = 0; ui.undo(false);
    check(json(ui.engine.project) == rebased && ui.engine.position == positionBeforeUndo
        && ui.undoHistory.size() == 1 && ui.redoHistory.empty() && ui.status.contains("clock is unavailable"),
        "Undo of a pre-roll rebase keeps both history stacks, geometry and cursor if the destination cannot be prepared");
    ui.engine.sampleRate = rateBeforeUndo;
    ui.undo(false);
    check(json(ui.engine.project) == original && near(ui.engine.position,.4), "Undo atomically restores the original mix and cursor at the same source time");
    ui.undo(true);
    check(json(ui.engine.project) == rebased && near(ui.engine.position,positionBeforeUndo), "Redo atomically restores the rebased mix and its corresponding cursor");
    // A beat is exactly 60 native integer pixels at this zoom. Free alignment
    // must not rely on source-phase snapping to hide pointer rasterisation.
    constexpr double pixels = 131;
    ui.timeline.pixelsPerSecond = pixels; ui.refresh(); ui.viewport.setViewPosition(0, 0);
    const auto mouse = juce::Desktop::getInstance().getMainMouseSource();
    auto event = [&](juce::Point<float> position, juce::Point<float> down, bool drag, bool alt = false)
    { return juce::MouseEvent(mouse, position, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier | (alt ? juce::ModifierKeys::altModifier : 0)), 1, 0, 0, 0, 0, &ui.timeline, &ui.timeline, juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), 1, drag); };
    const juce::Point<float> from {(float) ((origin + .7) * pixels), ui.timeline.clipBounds(ui.engine.project.clips[0]).getY() + 10};
    const auto to = from.translated((float) (-attack * pixels), 0);
    ui.timeline.mouseDown(event(from, from, false, true)); ui.timeline.mouseDrag(event(to, from, true, true)); ui.timeline.mouseUp(event(to, from, true, true));
    const auto& aligned = ui.engine.project.clips[0];
    check(near(aligned.start + attack, origin) && near(ui.engine.project.gridClock().beatsAt(aligned.start + attack), 0)
        && aligned.start >= 0 && aligned.start < origin && aligned.offset == 0 && near(aligned.length, 12) && ui.undoHistory.size() == 2,
        "native Alt left drag manually aligns the chosen downbeat at bar-one zero without trimming the intro");
    // The final 1.5-beat correction can be made independently of the full quiet bar.
    auto partial = ui.engine.project; partial.clips[0].moveTo(origin - 4 * beat);
    const auto corrected = partial.moveClip(clip.id, origin - attack, false, 0, false);
    check(corrected.wasOk() && near(partial.clips[0].start + attack, origin), "free movement allows the required 1.5-beat manual alignment inside pre-roll");
    const auto alignedDocument = json(ui.engine.project);
    const auto snappedTo = to.translated((float) (1.18 * beat * pixels), 0);
    ui.timeline.mouseDown(event(to, to, false)); ui.timeline.mouseDrag(event(snappedTo, to, true)); ui.timeline.mouseUp(event(snappedTo, to, true));
    check(near(ui.engine.project.gridClock().beatsAt(ui.engine.project.clips[0].start + attack), 1),
        "native snapped movement after manual pre-roll alignment retains the corrected downbeat phase");
    ui.undo(false);
    check(json(ui.engine.project) == alignedDocument, "Undo restores the exact manually aligned pre-roll position");
    auto rejected = ui.engine.project; const auto removed = rejected.setTimelineOrigin(0);
    check(removed.failed() && json(rejected) == alignedDocument, "removing occupied pre-roll rejects atomically and keeps the quiet intro");
    check(ui.engine.project.setTimelineOrigin(std::numeric_limits<double>::quiet_NaN()).failed() && json(ui.engine.project) == alignedDocument,
        "invalid pre-roll origin leaves arrangement untouched");
    ui.undo(false); check(near(ui.engine.project.clips[0].start, origin), "Undo restores the pre-roll song movement");
    const double positionBeforeRebaseUndo = ui.engine.position.load();
    ui.undo(false); check(json(ui.engine.project) == original && near(ui.engine.position,std::max(0.,positionBeforeRebaseUndo - origin)),
        "Undo removes the rebase and atomically restores the original session clocks and mapped cursor");
    ui.undo(true); ui.undo(true); check(json(ui.engine.project) == alignedDocument, "Redo restores pre-roll and the full-intro alignment");
    const auto session = report.getSiblingFile("preroll.mixworks");
    const auto saveError = ui.engine.saveProject(session);
    check(saveError.isEmpty() && session.existsAsFile(), "save the aligned pre-roll session to an actual arrangement file");
    const auto document = session.loadFileAsString();
    auto reopened = std::make_unique<Engine>(); const auto reopenError = reopened->restoreProject(document);
    check(reopenError.isEmpty() && near(reopened->project.tempo.origin, origin) && reopened->project.clips.size() == 1
        && near(reopened->project.clips[0].start, ui.engine.project.clips[0].start) && reopened->project.clips[0].offset == 0,
        "actual save/reopen preserves signed musical placement and full source: " + reopenError);
    auto legacy = juce::JSON::parse(original); legacy.getDynamicObject()->setProperty("version", 29); legacy.getDynamicObject()->removeProperty("timelineOrigin");
    const auto old = parseProject(juce::JSON::toString(legacy));
    check(old.error.isEmpty() && old.project.tempo.origin == 0 && old.clips.size() == 1,
        "version 29 sessions retain their original zero without implicit migration/repositioning");
    auto bad = juce::JSON::parse(document); bad.getDynamicObject()->setProperty("timelineOrigin", -1.);
    check(parseProject(juce::JSON::toString(bad)).error.isNotEmpty(), "negative persisted engine origin is rejected");
    ui.engine.stop(); const double start = ui.engine.project.playbackStart();
    check(near(ui.engine.position, start) && near(start, origin - attack), "Stop returns to the first retained intro sample, before musical zero");
    ui.engine.seek(origin + 1); ui.homeButton.onClick(); check(near(ui.engine.position, start), "Home includes the complete retained pre-roll intro");
    ui.engine.setPlaying(true); ui.engine.audioDeviceIOCallbackWithContext(nullptr, 0, after.getArrayOfWritePointers(), 2, 256, {});
    check(ui.engine.playing && near(ui.engine.position, start + 256 / ui.engine.sampleRate.load()) && after.getMagnitude(0, 256) > .001f,
        "device playback advances audibly through pre-roll instead of using stopped-instrument audition");
    const auto host = ui.engine.getPosition();
    check(host && *host->getPpqPosition() < 0 && *host->getTimeInSeconds() < 0, "plugin host sees matching negative musical time and PPQ during the intro");
    ui.engine.stop(); ui.timerCallback();
    auto capture = report.getSiblingFile("preroll-downbeat-aligned.png").createOutputStream(); juce::PNGImageFormat png;
    check(capture && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *capture), "capture complete quiet intro before zero and chosen downbeat on grey-section start");
    const auto exported = report.getSiblingFile("preroll-export.wav"); WavExportPlan plan;
    check(ui.engine.planWavExport(exported, {}, plan).wasOk() && near(plan.arrangementStart, start) && near(plan.seconds(), 12),
        "export plan includes all source audio and omits unused pre-roll padding");
    const auto exportError = ui.engine.exportWav(exported);
    std::unique_ptr<juce::AudioFormatReader> reader(ui.engine.formats.createReaderFor(exported));
    juce::AudioBuffer<float> quiet(2, 512), loud(2, 512);
    if (reader) { reader->read(&quiet, 0, 512, (juce::int64) std::llround(.5 * reader->sampleRate), true, true); reader->read(&loud, 0, 512, (juce::int64) std::llround((attack + .5) * reader->sampleRate), true, true); }
    check(exportError.isEmpty() && reader && reader->lengthInSamples == plan.frames && quiet.getRMSLevel(0, 0, 512) > .005f
        && loud.getRMSLevel(0, 0, 512) > quiet.getRMSLevel(0, 0, 512) * 5,
        "actual WAV retains the quiet opening at file start followed by the aligned loud downbeat: " + exportError);
    check(juce::SHA256(media->file).toHexString() == sourceHash, "pre-roll movement and export leave the original source bytes unchanged");
    {
        auto pending = Project(); pending.masterDb = 0; pending.setTimelineOrigin(origin);
        auto first = clip; first.pendingPlacement = PendingPlacement::append; pending.clips = {first};
        const auto placed = prepareTempoMap(pending, false, 0, 0, 0, {}, TransitionPreset::alignOnly);
        bool phase = placed.error.isEmpty() && placed.project.clips.size() == 1;
        juce::String placementFacts;
        if (phase)
        {
            const auto& voice = placed.project.clips[0];
            placementFacts = " start=" + juce::String(voice.start, 8) + " origin=" + juce::String(placed.project.tempo.origin, 8) + " downbeat=" + juce::String(voice.firstDownbeatTime(), 8) + " span=" + juce::String(voice.sourceSpan(), 8);
            phase = near(placed.project.tempo.origin, origin) && voice.start < origin && voice.offset == 0 && near(voice.sourceSpan(), 12)
                && near(placed.project.gridClock().beatsAt(voice.start + voice.firstDownbeatTime()), 0);
        }
        check(phase, "first normal insertion uses pre-roll for its source downbeat instead of clamping/trimming at zero: " + placed.error + placementFacts);
        auto mapped = ui.engine.project; mapped.followTrackTempo = true;
        mapped.tempo.points = {{0, bpm, false}, {origin, bpm, false}, {origin + 6, 145, true}};
        auto& voice = mapped.clips[0]; voice.beatmatched = true; voice.tempoSourceBpm = bpm;
        voice.tempoMap = std::make_shared<const TempoMap>(mapped.tempo); voice.prepared.reset();
        voice.length = voice.localTimeForSource(12); voice.rate = 12 / voice.length;
        auto renderer = std::make_unique<Engine>(); renderer->project = mapped;
        const auto mappedSave = juce::JSON::toString(renderer->captureProject());
        auto restoredClock = std::make_unique<Engine>(); const auto clockError = restoredClock->restoreProject(mappedSave);
        bool clockSame = clockError.isEmpty() && restoredClock->project.clips.size() == 1;
        if (clockSame)
        {
            const auto& restoredVoice = restoredClock->project.clips[0];
            clockSame = restoredVoice.start < origin && near(restoredVoice.start, voice.start) && near(restoredVoice.sourceElapsed(.5), voice.sourceElapsed(.5))
                && near(restoredVoice.localTimeForSource(4), voice.localTimeForSource(4));
        }
        check(clockSame, "actual mapped/ramped save-reopen retains the captured source clock before zero: " + clockError);
    }
    auto empty = Project(); check(empty.setTimelineOrigin(4).wasOk() && empty.playbackStart() == 4, "empty pre-roll starts at musical zero");
    empty.clips = {clip}; empty.clips[0].start = 4; empty.clips[0].pendingPlacement = PendingPlacement::append;
    check(empty.setTimelineOrigin(8).failed(), "pending placement cannot be rebased midway");
    auto locked = ui.engine.project; auto partner = locked.clips[0]; partner.id = juce::Uuid().toString(); partner.start += 6; partner.track = 1;
    locked.clips.push_back(partner); locked.transitionLinks = {{clip.id, partner.id}}; locked.transitionLinks[0].locked = true;
    const auto lockedDocument = json(locked);
    check(locked.setTimelineOrigin(origin + 2).failed() && json(locked) == lockedDocument, "transition locks protect origin changes");
    ui.engine.stop();
    const bool hadIntroStart = ui.engine.position > 0 && ui.engine.project.tempo.origin > 0;
    ui.newSession();
    check(hadIntroStart && !ui.engine.playing && ui.engine.position == 0 && ui.engine.project.tempo.origin == 0
        && ui.engine.project.clips.empty(), "New mix clears the previous pre-roll start and returns the fresh transport to zero");
}
}
