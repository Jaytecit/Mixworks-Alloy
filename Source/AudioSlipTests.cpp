#include "Workstation.h"
#include "ElasticAudio.h"
#include <cmath>
#include <limits>

namespace mixworks
{
void runAudioSlipTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    const auto root = report.getParentDirectory().getChildFile("audio-slip-" + juce::Uuid().toString()); root.createDirectory();
    auto media = std::make_shared<Media>(); media->sampleRate = 8000; media->manualBpm = 120; media->manualBeatOrigin = .213;
    media->name = "Audio slip fixture"; media->file = root.getChildFile("source.wav"); media->samples.setSize(1, 32000);
    for (int i = 0; i < 32000; ++i) media->samples.setSample(0, i, .12f + .05f * std::sin((float) (i * .031)) + (i > 4000 && i < 4100 ? .4f : 0.f));
    {
        juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream = media->file.createOutputStream();
        auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(1).withBitsPerSample(24));
        check(writer && writer->writeFromAudioSampleBuffer(media->samples, 0, 32000), "write owned nonzero-edge source for production slip Save/Open/export");
    }
    Clip clip; clip.media = media; clip.length = 4; clip.introAnchor = 0; clip.outroAnchor = 2;
    Project base; base.bpm = 120; base.tempo.setPoint(0, 120); base.clips.push_back(clip);
    Clip partner = clip; partner.id = juce::Uuid().toString(); partner.start = 2; partner.track = 1;
    base.clips.push_back(partner); base.transitionLinksKnown = true; base.transitionLinks = {{clip.id, partner.id}};
    base.clips[0].controls.automation[0] = {true, {{0, -9}, {1, -3}, {4, -12}}};
    base.clips[0].fadeIn = .2; base.clips[0].fadeOut = .4;
    const auto json = [](const Project& p) { return juce::JSON::toString(projectToJson(p)); };
    auto changed = base; auto expected = base; expected.clips[0].audioSlip = .65;
    check(changed.setClipAudioSlip(clip.id, .65).wasOk() && json(changed) == json(expected)
        && changed.transitionRange(changed.transitionLinks[0]) == base.transitionRange(base.transitionLinks[0]),
        "slipping changes only this clip's audio clock, retaining all automation, fade/IN/OUT anchors, partner and transition geometry");
    check(changed.clips[0].sourceElapsed(1) == base.clips[0].sourceElapsed(1)
        && std::abs(changed.clips[0].sourceTimeAt(1) - .35) < 1.e-9
        && std::abs(changed.clips[0].firstDownbeatTime() - .863) < 1.e-8,
        "audio source and detected downbeat move while the automation clock stays fixed");
    auto invalid = changed; const auto valid = json(invalid);
    check(invalid.setClipAudioSlip(clip.id, std::numeric_limits<double>::quiet_NaN()).failed()
        && invalid.setClipAudioSlip("missing", 0).failed() && json(invalid) == valid, "invalid audio slip is failure-atomic");
    invalid.setTransitionLocked(partner.id, true); const auto locked = json(invalid);
    check(invalid.setClipAudioSlip(clip.id, .2).failed() && invalid.setClipAudioSlip(partner.id, .2).failed() && json(invalid) == locked,
        "transition protection covers either participating audio slip without changing the pair");
    for (const auto mode : {"raw", "reverse", "loop", "prepared", "clip-local", "elastic"})
    {
        Clip c = clip; c.introAnchor.reset(); c.outroAnchor.reset(); c.controls = {}; c.fadeIn = c.fadeOut = 0;
        const juce::String name(mode);
        if (name == "reverse") c.reversed = true;
        if (name == "loop") c.loopSpan = 1.5;
        if (name == "prepared" || name == "clip-local")
        {
            auto audio = std::make_shared<PreparedAudio>(); audio->rate = 1; audio->clipLocal = name == "clip-local";
            audio->samples.makeCopyOf(media->samples); c.prepared = audio;
        }
        if (name == "elastic")
        {
            c.beatmatched = c.elasticPlayback = true; c.tempoSourceBpm = 120;
            c.tempoMap = std::make_shared<const TempoMap>(base.tempo); c.elasticSource = std::make_shared<const ElasticSource>(c);
        }
        Engine reference, slipped; reference.sampleRate = slipped.sampleRate = 8000;
        reference.project.clips = {c}; c.audioSlip = .25; slipped.project.clips = {c};
        juce::AudioBuffer<float> a(2, 256), b(2, 256); juce::MidiBuffer midi;
        reference.render(a, 256, 1, midi); slipped.render(b, 256, 1.25, midi);
        double difference = 0; for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 256; ++i) difference = std::max(difference, (double) std::abs(a.getSample(ch, i) - b.getSample(ch, i)));
        check(difference < 1.e-6 && a.getRMSLevel(0, 0, 256) > .01, "slipped " + name + " audio reads the same payload at its new time: " + juce::String(difference, 8));
        auto sliced = c.slice(.875, 2); slipped.project.clips = {sliced}; slipped.render(b, 256, 1.25, midi);
        difference = 0; for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 256; ++i) difference = std::max(difference, (double) std::abs(a.getSample(ch, i) - b.getSample(ch, i)));
        check(difference < 1.e-6, "splitting retains the slipped " + name + " sound and captured payload");
        if (name == "raw" || name == "clip-local" || name == "elastic")
        {
            slipped.project.clips = {c}; slipped.render(b, 256, 0, midi);
            check(b.getRMSLevel(0, 0, 256) < .00001 && std::isfinite(b.getRMSLevel(0, 0, 256)),
                "slipped " + name + " source head pads with silence instead of holding its nonzero first sample");
        }
    }
    {
        Clip plain = clip; plain.controls = {}; plain.fadeIn = plain.fadeOut = 0; plain.audioSlip = .5;
        Clip automated = plain; automated.controls.automation[0] = {true, {{0, -20}, {.8, -20}, {.81, 0}, {4, 0}}};
        Engine control, moving; control.sampleRate = moving.sampleRate = 8000; control.project.clips = {plain}; moving.project.clips = {automated};
        juce::AudioBuffer<float> a(2, 64), b(2, 64); juce::MidiBuffer midi; control.render(a, 64, 1, midi); moving.render(b, 64, 1, midi);
        double difference = 0; for (int i = 0; i < 64; ++i) difference = std::max(difference, (double) std::abs(a.getSample(0, i) - b.getSample(0, i)));
        check(difference < 1.e-6 && a.getRMSLevel(0, 0, 64) > .01, "audible automation stays at mix time instead of following the slipped audio clock");
    }
    auto forwardMoveTests = [&]
    {
        Project movement; movement.followTrackTempo = false; movement.bpm = 120; movement.snap = false;
        for (int i = 0; i < 4; ++i)
        {
            Clip c; c.media = media; c.track = i; c.start = i == 3 ? 10 : i * 2; c.length = 4;
            c.introAnchor = .5; c.outroAnchor = 2.5; c.fadeInAnchor = .5; c.fadeOutAnchor = 2.5; c.fadeIn = c.fadeOut = 1.5;
            movement.clips.push_back(c);
        }
        movement.transitionLinksKnown = true;
        for (int i = 0; i < 2; ++i)
        {
            auto& out = movement.clips[(size_t) i]; auto& in = movement.clips[(size_t) i + 1];
            movement.linkClips(out.id, in.id);
            auto& a = out.controls.automation[1]; auto& b = in.controls.automation[1]; a.enabled = b.enabled = true;
            a.setPoint(2.75, 0); a.setPoint(3.25, -60); b.setPoint(.75, -60); b.setPoint(1.25, 0);
            movement.transitionLinks[(size_t) i].bassSwap = std::array<double, 4>{2.75, 3.25, .75, 1.25};
        }
        const auto id = movement.clips[1].id;
        const auto incoming = movement.transitionRange(movement.transitionLinks[0]);
        const auto outgoing = movement.transitionRange(movement.transitionLinks[1]);
        const auto earlier = juce::JSON::toString(projectToJson(movement)["clips"][0]);
        for (double delta : {-.2, .2})
        {
            auto moved = movement; const auto result = moved.moveClipFollowing(id, 2 + delta, false);
            check(result.wasOk() && moved.validateTransitionLinks().wasOk(), "forward move retains paired transition ownership: " + result.getErrorMessage());
            if (result.failed()) continue;
            check(std::abs(moved.clips[1].start - 2 - delta) < 1e-8 && std::abs(moved.clips[2].start - 4 - delta) < 1e-8
                && std::abs(moved.clips[3].start - 10 - delta) < 1e-8, "selected box, linked successor and unlinked later track move together");
            check(juce::JSON::toString(projectToJson(moved)["clips"][0]) == earlier && moved.transitionRange(moved.transitionLinks[0]) == incoming,
                "the entire earlier clip and incoming transition stay at their exact original positions");
            const auto after = moved.transitionRange(moved.transitionLinks[1]);
            check(std::abs(after.getStart() - outgoing.getStart() - delta) < 1e-8 && std::abs(after.getEnd() - outgoing.getEnd() - delta) < 1e-8
                && moved.transitionLinks[1] == movement.transitionLinks[1], "later transition window moves intact with its source-owned bass treatment");
            const auto& pair = *moved.transitionLinks[0].bassSwap;
            check(std::abs(moved.clips[1].start + moved.clips[1].localTimeForSource(pair[2]) - 2.75) < 1e-8
                && std::abs(moved.clips[1].start + moved.clips[1].localTimeForSource(pair[3]) - 3.25) < 1e-8,
                "incoming bass endpoints stay fixed while the selected audio and box move");
            const auto parsed = parseProject(json(moved));
            check(parsed.error.isEmpty() && parsed.clips.size() == 4 && std::abs(parsed.clips[1].clip.start - 2 - delta) < 1e-8,
                "forward geometry and pinned anchors survive the existing session format");
        }
        for (double target : {-1., 3., std::numeric_limits<double>::quiet_NaN()})
        {
            auto rejected = movement; const auto before = json(rejected);
            check(rejected.moveClipFollowing(id, target, false).failed() && json(rejected) == before, "invalid/out-of-frame forward movement rejects atomically");
        }
        for (const auto& lockId : {movement.clips[1].id, movement.clips[2].id})
        {
            auto locked = movement; locked.setTransitionLocked(lockId, true); const auto before = json(locked);
            check(locked.moveClipFollowing(id, 2.2, false).failed() && json(locked) == before, "incoming and later transition locks protect the whole forward edit");
        }
        auto pending = movement; pending.clips[3].pendingPlacement = PendingPlacement::append; const auto beforePending = json(pending);
        check(pending.moveClipFollowing(id, 2.2, false).failed() && json(pending) == beforePending, "pending later tracks cannot be partly moved");
        auto snapped = movement; const auto aligned = snapped.moveClipFollowing(id, 2.26, true);
        check(aligned.wasOk() && std::abs(snapped.clips[1].start - 2.5) < 1e-6
            && std::abs(snapped.clips[2].start - 4.5) < 1e-6
            && std::abs(snapped.clips[1].firstBeatTime() - movement.clips[1].firstBeatTime()) < 1e-6,
            "forward Snap moves by whole beats and retains both manually placed source phases");
        auto mapped = movement; mapped.followTrackTempo = true; mapped.tempo = TempoMap {120, {{0, 120, false}, {8, 126, false}}};
        const auto map = std::make_shared<const TempoMap>(mapped.tempo);
        for (auto& c : mapped.clips) { c.beatmatched = true; c.tempoSourceBpm = 120; c.tempoMap = map; c.length = c.localTimeForSource(4); c.rate = 4 / c.length; }
        const auto beforeMapped = mapped;
        const auto mappedResult = mapped.moveClipFollowing(id, 2.2, false);
        check(mappedResult.wasOk() && std::abs(mapped.tempo.beatsAt(mapped.clips[3].start) - mapped.tempo.beatsAt(beforeMapped.clips[3].start) - .4) < 1e-7,
            "an unlinked later track carries musical displacement through a different project tempo");
        auto dry = std::make_unique<Engine>(juce::File(), false); auto wet = std::make_unique<Engine>(juce::File(), false);
        dry->sampleRate = wet->sampleRate = 8000; dry->project = wet->project = movement;
        check(wet->project.moveClipFollowing(id, 2.2, false).wasOk(), "prepare a forward render comparison");
        juce::AudioBuffer<float> a(2, 64), b(2, 64); juce::MidiBuffer midi;
        dry->render(a, 64, 1, midi); wet->render(b, 64, 1, midi); double difference = 0;
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 64; ++i) difference = std::max(difference, (double) std::abs(a.getSample(ch, i) - b.getSample(ch, i)));
        check(difference < 1e-7 && a.getRMSLevel(0, 0, 64) > .001, "audio before the pinned incoming transition remains sample-identical");
        dry->resetPlugins(); wet->resetPlugins(); dry->render(a, 64, 5, midi); wet->render(b, 64, 5.2, midi); difference = 0;
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 64; ++i) difference = std::max(difference, (double) std::abs(a.getSample(ch, i) - b.getSample(ch, i)));
        check(difference < 1e-5 && a.getRMSLevel(0, 0, 64) > .001, "later transition audio and treatment translate together through production rendering");
        auto owned = std::make_unique<Workstation>(false); auto& desk = *owned; desk.stopTimer(); desk.setSize(1920, 1080);
        desk.engine.project = movement; desk.engine.library = {media}; desk.timeline.pixelsPerSecond = 100; desk.refresh();
        const auto mouse = juce::Desktop::getInstance().getMainMouseSource();
        auto event = [&](juce::Point<float> p, juce::Point<float> down, bool drag)
        { return juce::MouseEvent(mouse, p, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::altModifier), 1, 0, 0, 0, 0, &desk.timeline, &desk.timeline, juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), 1, drag); };
        const auto down = desk.timeline.audioSlipBounds(desk.engine.project.clips[1]).getCentre();
        desk.timeline.mouseDown(event(down, down, false)); desk.timeline.mouseDrag(event(down.translated(20, 0), down, true));
        desk.engine.project.masterDb = -7;
        desk.timeline.mouseDrag(event(down.translated(32, 0), down, true)); desk.timeline.mouseDrag(event(down.translated(32, 0), down, true));
        desk.timeline.mouseUp(event(down.translated(32, 0), down, true));
        check(std::abs(desk.engine.project.clips[1].start - 2.32) < 1e-7 && std::abs(desk.engine.project.clips[3].start - 10.32) < 1e-7
            && desk.undoHistory.size() == 1 && desk.engine.project.masterDb == -7 && desk.engine.project.clips[1].audioSlip == 0,
            "default native handle drag moves the boxes repeatedly with one Undo and retains live master changes");
        desk.undo(false); check(json(desk.engine.project) == json(movement), "Undo restores the complete forward handle gesture"); desk.undo(true);
        check(std::abs(desk.engine.project.clips[2].start - 4.32) < 1e-7, "Redo restores the translated following arrangement");
        auto stream = report.getSiblingFile("forward-move-pinned.png").createOutputStream(); juce::PNGImageFormat png;
        check(stream && png.writeImageToStream(desk.createComponentSnapshot(desk.getLocalBounds()), *stream), "capture native forward movement with the fixed incoming transition");
    };
    forwardMoveTests();

    Workstation ui(false); ui.stopTimer(); ui.engine.project = base; ui.engine.library = {media}; ui.setSize(1440, 940);
    ui.timeline.pixelsPerSecond = 100; ui.refresh(); ui.engine.position = 1.125; ui.engine.playing = true;
    const auto mouse = juce::Desktop::getInstance().getMainMouseSource();
    auto event = [&](juce::Point<float> point, juce::Point<float> down, bool drag, int modifiers = 0)
    { return juce::MouseEvent(mouse, point, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::shiftModifier | modifiers), 1, 0, 0, 0, 0, &ui.timeline, &ui.timeline, juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), 1, drag); };
    const auto firstHandle = ui.timeline.audioSlipBounds(ui.engine.project.clips[0]), secondHandle = ui.timeline.audioSlipBounds(ui.engine.project.clips[1]);
    check(!firstHandle.isEmpty() && !secondHandle.isEmpty() && ui.timeline.audioSlipAt(firstHandle.getCentre()) == clip.id
        && ui.timeline.audioSlipAt(secondHandle.getCentre()) == partner.id, "every timeline clip has its own independently hit-testable move / Shift-slip handle");
    const auto down = firstHandle.getCentre();
    ui.timeline.mouseDown(event(down, down, false)); ui.timeline.mouseUp(event(down, down, false));
    check(ui.undoHistory.empty() && ui.engine.position == 1.125, "clicking the slip icon selects its track without moving audio, seeking or creating Undo");
    ui.timeline.mouseDown(event(down, down, false));
    ui.timeline.mouseDrag(event(down.translated(40, 0), down, true, juce::ModifierKeys::altModifier));
    ui.engine.project.masterDb = -8;
    ui.timeline.mouseDrag(event(down.translated(65, 0), down, true, juce::ModifierKeys::altModifier));
    ui.timeline.mouseUp(event(down.translated(65, 0), down, true));
    expected.masterDb = -8;
    check(json(ui.engine.project) == json(expected) && ui.undoHistory.size() == 1 && ui.engine.position == 1.125 && ui.engine.playing,
        "native free slip gesture changes only its audio, retains concurrent edits/live transport and creates one Undo");
    const auto slippedState = json(ui.engine.project); ui.undo(false); ui.undo(true);
    check(json(ui.engine.project) == slippedState, "audio slip round-trips through actual Undo and Redo");
    ui.timeline.mouseDoubleClick(event(down, down, false));
    check(ui.engine.project.clips[0].audioSlip == 0 && ui.engine.position == 1.125 && ui.undoHistory.size() == 2,
        "double-click resets only audio slip with one Undo and no seek");
    ui.undo(false);
    ui.timeline.mouseDown(event(down, down, false)); ui.timeline.mouseDrag(event(down.translated(17, 0), down, true)); ui.timeline.mouseUp(event(down, down, true));
    const auto& snappedClip = ui.engine.project.clips[0];
    check(std::abs(std::remainder(snappedClip.start + snappedClip.firstBeatTime(), .5)) < 1.e-8, "slip snapping aligns an actual musical pulse to the mix beat");
    const double beforeLeft = ui.engine.project.clips[0].audioSlip;
    ui.timeline.mouseDown(event(down, down, false)); ui.timeline.mouseDrag(event(down.translated(-237, 0), down, true, juce::ModifierKeys::altModifier)); ui.timeline.mouseUp(event(down, down, true));
    check(std::abs(ui.engine.project.clips[0].audioSlip - beforeLeft + 2.37) < 1.e-8, "first-track audio slips left past mix zero without moving clip boundaries");
    check(std::abs(base.tempo.snapWithPreroll(-.76, true) + 1) < 1.e-8 && base.tempo.snapWithPreroll(-.76, false) == -.76,
        "signed slip snapping extends the first mix tempo while free edits retain their exact offset");
    ui.setBusy(true); const auto busyState = json(ui.engine.project); const auto busyUndo = ui.undoHistory.size();
    ui.timeline.mouseDown(event(down, down, false)); ui.timeline.mouseDrag(event(down.translated(20, 0), down, true)); ui.timeline.mouseUp(event(down, down, true));
    check(json(ui.engine.project) == busyState && ui.undoHistory.size() == busyUndo, "busy state rejects the audio slip gesture atomically"); ui.setBusy(false);
    ui.engine.playing = false; const auto session = root.getChildFile("slipped.mixworks"); Engine reopened;
    const auto saveError = ui.engine.saveProject(session), openError = reopened.restoreProject(session.loadFileAsString());
    check(saveError.isEmpty() && openError.isEmpty() && reopened.project.clips[0].audioSlip == ui.engine.project.clips[0].audioSlip,
        "production Save/Open retains audio slip: " + saveError + openError);
    const auto aFile = root.getChildFile("slipped.wav"), bFile = root.getChildFile("reopened.wav");
    const auto aError = ui.engine.exportWav(aFile), bError = reopened.exportWav(bFile);
    auto left = std::unique_ptr<juce::AudioFormatReader>(ui.engine.formats.createReaderFor(aFile));
    auto right = std::unique_ptr<juce::AudioFormatReader>(ui.engine.formats.createReaderFor(bFile));
    bool sameRender = aError.isEmpty() && bError.isEmpty() && left && right && left->lengthInSamples == right->lengthInSamples;
    double renderDifference = 0;
    if (sameRender)
    {
        juce::AudioBuffer<float> a(2, 512), b(2, 512);
        for (juce::int64 offset = 0; offset < left->lengthInSamples; offset += 512)
        {
            const int frames = (int) std::min<juce::int64>(512, left->lengthInSamples - offset);
            sameRender &= left->read(&a, 0, frames, offset, true, true) && right->read(&b, 0, frames, offset, true, true);
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < frames; ++i) renderDifference = std::max(renderDifference, (double) std::abs(a.getSample(ch, i) - b.getSample(ch, i)));
        }
    }
    check(sameRender && renderDifference <= 4. / 8388608, "slipped mix survives production Save/Open/export within four PCM24 steps: " + juce::String(renderDifference, 9) + aError + bError);
    auto document = projectToJson(ui.engine.project); auto* row = document["clips"].getArray()->getReference(0).getDynamicObject(); row->setProperty("audioSlip", "bad");
    check(parseProject(juce::JSON::toString(document)).error.isNotEmpty(), "session boundary rejects a malformed audio slip");
    row->removeProperty("audioSlip"); document.getDynamicObject()->setProperty("version", 21); document["tracks"].getArray()->removeRange(4, trackCount - 4);
    const auto old = parseProject(juce::JSON::toString(document));
    check(old.error.isEmpty() && old.clips[0].clip.audioSlip == 0, "format 21 sessions retain their original unslipped playback");
    auto stream = report.getSiblingFile("audio-slip-handles.png").createOutputStream(); juce::PNGImageFormat png;
    check(stream && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *stream), "capture native per-track slip icons and fixed transition/automation layout");
}
}
