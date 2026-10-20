#include "Workstation.h"
#include "Beatmatch.h"
#include <cmath>

namespace mixworks
{
void runTrackOwnershipTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report, const juce::File& plugin)
{
    auto near = [](double a, double b) { return std::abs(a - b) < 1.0e-6; };
    auto media = std::make_shared<Media>(); media->sampleRate = 8000; media->manualBpm = 120; media->manualBeatOrigin = 0.213;
    media->name = "Track ownership"; media->file = report.getSiblingFile("ownership.wav");
    media->samples.setSize(2, 8000 * 120);
    for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < media->frames(); ++i) media->samples.setSample(ch, i, 0.25f);
    {
        juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream = media->file.createOutputStream();
        auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(2).withBitsPerSample(24));
        check(writer && writer->writeFromAudioSampleBuffer(media->samples, 0, media->frames()), "ownership tests use a real source file");
    }
    Clip a; a.media = media; a.length = 120; a.pendingPlacement = PendingPlacement::append;
    Clip b = a; b.id = juce::Uuid().toString(); b.effects = makeClipEffects(); b.track = 1;
    Project p; p.clips = {a, b};
    auto ready = prepareTempoMap(p, false, 32);
    check(ready.error.isEmpty(), "prepare default eight-bar arrangement: " + ready.error);
    if (ready.error.isNotEmpty()) return;
    const auto& first = ready.project.clips[0]; const auto& next = ready.project.clips[1];
    check(ready.project.tempo.points.size() == 2 && near(ready.project.tempo.points.back().time, next.start + next.transitionTime(true))
          && ready.project.tempo.points.back().bpm == next.media->bpm(), "adding two tracks creates exactly two tempo markers, with the incoming target at IN");
    check(near(std::remainder(ready.project.tempo.beatsAt(first.start + first.firstDownbeatTime()), 4), 0)
          && near(std::remainder(ready.project.tempo.beatsAt(next.start + next.firstDownbeatTime()), 4), 0)
          && near(ready.project.tempo.beatsAt(first.end()) - ready.project.tempo.beatsAt(next.start), 32),
          "automatic insertion aligns both bar phases and retains eight bars when source spans permit it");
    check(first.fadeIn == 0 && first.fadeOut == 0 && next.fadeIn == 0 && next.fadeOut == 0,
          "insertion never introduces volume fades");
    check(near(first.start + first.transitionTime(false), next.start + next.transitionTime(true)), "default IN and OUT handles coincide at the overlap start");
    const auto& outgoingBass = first.controls.automation[1]; const auto& incomingBass = next.controls.automation[1];
    const auto firstSwap = ready.project.transitionPlaybackFor(first).pairs[1][1];
    const auto nextSwap = ready.project.transitionPlaybackFor(next).pairs[0][1];
    check(!outgoingBass.active() && !incomingBass.active() && firstSwap.instant && nextSwap.instant
        && firstSwap.gainAt(firstSwap.begin) == 0 && nextSwap.gainAt(nextSwap.begin) == 1,
          "default transition swaps bass vertically without adding another source automation curve");
    Clip third = b; third.id = juce::Uuid().toString(); third.effects = makeClipEffects(); third.pendingPlacement = PendingPlacement::append;
    auto chain = ready.project; chain.clips.push_back(third); auto three = prepareTempoMap(chain, false, 32);
    check(three.error.isEmpty() && three.project.transitionPlaybackFor(three.project.clips[1]).pairs[0][1].instant
          && three.project.transitionPlaybackFor(three.project.clips[1]).pairs[1][1].instant,
          "the middle track keeps both its incoming and outgoing bass swaps");
    auto cutMix = prepareTempoMap(three.project, true, 0);
    check(cutMix.error.isEmpty() && std::all_of(cutMix.project.clips.begin(), cutMix.project.clips.end(), [](const auto& c) { return c.controls.automation[1].points.empty(); }),
          "rebuilding Cut removes only generated bass swaps");
    auto staged = ready.project; staged.clips[1].start += 11; staged.clips[1].pendingPlacement = PendingPlacement::drop;
    staged.clips[1].moveTo(173.123456);
    const auto explicitlyMoved = prepareTempoMap(staged, false, 32);
    check(explicitlyMoved.error.isNotEmpty() && near(staged.clips[1].start, 173.123456),
          "a pending disjoint move cannot move the song back silently or place a generated handoff outside either song: " + explicitlyMoved.error);
    auto free = staged.clips[1]; free.moveTo(4.123456);
    check(near(free.start, 4.123456), "freehand moves preserve an exact requested time");
    auto snapped = next; snapped.moveTo(ready.project.snappedClipStart(snapped, 33.721, true));
    const double beat = ready.project.tempo.beatsAt(snapped.start + snapped.firstBeatTime());
    check(near(beat, std::round(beat)), "beat snapping aligns the source pulse rather than its pickup edge");
    bool pixels = true;
    for (double zoom : {3.7, 7.3, 10.0, 12.7, 27.01, 81.3, 200.0}) for (int n = 0; n < 170; ++n)
    {
        const double source = snapped.sourceElapsed(snapped.firstBeatTime()) + n * 0.5;
        const double time = snapped.beatTimelineTime(source);
        pixels &= Clip::gridPixel(time, zoom) == Clip::gridPixel(ready.project.tempo.timeAtBeat(std::round(ready.project.tempo.beatsAt(time))), zoom);
    }
    check(pixels, "track and background beats rasterize to identical pixels across seven fractional zoom scales");

    Workstation ui(false); ui.stopTimer(); ui.setSize(1440, 940); ui.engine.project.masterDb = 0;
    a.pendingPlacement = PendingPlacement::none; a.start = 2; a.length = 12; a.effects = makeClipEffects(); a.controls = {};
    ui.engine.project.followTrackTempo = false; ui.engine.project.clips = {a}; ui.engine.library = {media}; ui.selectedClip = a.id;
    ui.engine.project.snap = false; ui.refresh();
    ui.performTimelineAction(TimelineAction::volume, 6.123, 0);
    auto address = AutomationAddress {0, AutomationTarget::volume, {}, {}, a.id};
    auto* curve = ui.engine.project.curve(address);
    check(curve && curve->points.size() == 2 && near(curve->points.back().time, 4.123) && ui.engine.project.tracks[0].automation[0].points.empty(),
          "RMB creates automation on its audio track in source coordinates");
    curve->points.back().value = -12;
    auto& voice = ui.engine.project.clips[0]; voice.moveTo(9.327); voice.track = 2; ui.engine.selectedTrack = 2;
    check(near(ui.engine.project.automationSessionTime(address, curve->points.back().time), 13.45)
          && near(ui.engine.automationValue(address, 13.45), -12) && voice.controls.automation[0].active(),
          "automation follows exact free movement and lane changes");
    ui.engine.project.followTrackTempo = true; ui.engine.project.tempo.setPoint(0, 120); ui.engine.project.tempo.setPoint(15, 160);
    voice.beatmatched = true; voice.tempoSourceBpm = 120;
    voice.retime(7.913, std::make_shared<const TempoMap>(ui.engine.project.tempo));
    check(near(ui.engine.project.automationTime(address, ui.engine.project.automationSessionTime(address, 4.123)), 4.123),
          "automation retains its source position through a changing tempo map");
    ui.engine.project.followTrackTempo = false; ui.engine.project.tempo = {}; voice = a; voice.controls.automation[0].enabled = true;
    voice.controls.automation[0].setPoint(0, -12); voice.start = 0; voice.track = 0;
    Clip neighbour = a; neighbour.id = juce::Uuid().toString(); neighbour.start = 0; neighbour.effects = makeClipEffects(); neighbour.controls = {};
    ui.engine.project.clips.push_back(neighbour);
    juce::AudioBuffer<float> output(2, 512); juce::MidiBuffer midi;
    ui.engine.render(output, 512, 1, midi);
    check(near(output.getSample(0, 100), 0.25 * (1 + juce::Decibels::decibelsToGain(-12.0f))),
          "overlapping tracks on one lane retain independent volume automation in actual PCM");
    auto legacy = ui.engine.captureProject(); legacy.getDynamicObject()->setProperty("version", 10); legacy["tracks"].getArray()->removeRange(4, trackCount - 4);
    Engine migrated; const auto migrationError = migrated.restoreProject(juce::JSON::toString(legacy));
    check(migrationError.isEmpty(), "older sessions still reopen before ownership migration: " + migrationError);

    Engine recorder; recorder.project.clips = {a}; recorder.selectedClip = a.id;
    recorder.armAutomation(0, true); recorder.setAutomationRecordMode(AutomationRecordMode::touch); recorder.setPlaying(true); recorder.position = 6.5;
    recorder.setControlValue({0, AutomationTarget::volume}, -9); recorder.position = 6.7; recorder.finishAutomation();
    const auto& recorded = recorder.project.clips[0].controls.automation[0];
    check(recorded.active() && recorder.project.tracks[0].automation[0].points.empty()
          && std::any_of(recorded.points.begin(), recorded.points.end(), [&](const auto& point) { return near(point.time, 4.5) && point.value == -9; }),
          "Touch recording binds control changes to source positions on the selected audio track");
    recorder.project.clips[0].moveTo(10.125);
    check(near(recorder.project.automationSessionTime(address, 4.5), 14.625), "recorded automation follows a later move");
    if (plugin == juce::File()) return;
    ui.engine.project.clips.resize(1); ui.engine.project.clips[0] = a; ui.engine.project.clips[0].start = 0;
    ui.selectedClip = a.id; ui.engine.selectedTrack = 0;
    const auto error = ui.engine.addPlugin(0, plugin);
    check(error.isEmpty() && ui.engine.plugins[0].empty() && ui.engine.project.clips[0].effects->plugins.size() == 1,
          "a VST insert belongs to the selected audio track: " + error);
    if (error.isNotEmpty()) return;
    auto parameter = ui.engine.pluginAddress(0, 0, 0); check(parameter && parameter->clipId == a.id, "VST parameter identity includes its audio track owner");
    if (!parameter) return;
    ui.engine.ensurePluginAutomation(*parameter); auto* lane = ui.engine.project.curve(*parameter); lane->enabled = true; lane->setPoint(0, 0.5f);
    ui.engine.render(output, 512, 1, midi);
    check(near(output.getSample(0, 100), 0.125), "track-owned VST automation changes actual PCM");
    lane->enabled = false; ui.engine.pluginChain(0)[0]->processor->getParameters()[0]->setValue(0.25f);
    ui.engine.render(output, 512, 1, midi);
    check(near(output.getSample(0, 100), 0.0625), "disabled automation preserves manual VST parameter edits");
    lane->enabled = true;
    ui.engine.project.clips.push_back(neighbour); ui.engine.resetPlugins(); ui.engine.render(output, 512, 1, midi);
    check(near(output.getSample(0, 100), 0.375), "a track's VST does not process an overlapping neighbour on the same lane");
    ui.engine.project.clips.resize(1); ui.engine.selectedTrack = 0; ui.timeline.pixelsPerSecond = 30; ui.refresh();
    const auto mouse = juce::Desktop::getInstance().getMainMouseSource();
    auto event = [&](juce::Point<float> position, juce::Point<float> down, bool drag)
    { return juce::MouseEvent(mouse, position, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1, 0, 0, 0, 0, &ui.timeline, &ui.timeline, juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), 1, drag); };
    const juce::Point<float> from {120, (float) ui.timeline.laneTop(0) + 105}, to {159, (float) ui.timeline.laneTop(3) + 105};
    const auto attached = ui.engine.project.clips[0].effects;
    ui.timeline.mouseDown(event(from, from, false)); ui.timeline.mouseDrag(event(to, from, true)); ui.timeline.mouseUp(event(to, from, true));
    check(near(ui.engine.project.clips[0].start, 1.3) && ui.engine.project.clips[0].track == 3 && ui.engine.project.clips[0].effects == attached,
          "native freehand dragging carries the effect chain and its markers to another lane");
    ui.undo(false); check(ui.engine.project.clips[0].start == 0 && ui.engine.project.clips[0].track == 0 && ui.engine.project.clips[0].effects == attached,
          "Undo restores track placement with its effect instance intact");
    ui.undo(true); check(near(ui.engine.project.clips[0].start, 1.3) && ui.engine.project.clips[0].track == 3, "Redo restores the moved track and effects together");
    auto& effected = ui.engine.project.clips[0]; const auto effects = effected.effects; effected.moveTo(7.123); effected.track = 3;
    ui.engine.project.clips.resize(1); ui.engine.selectedTrack = 3; ui.engine.resetPlugins(); ui.engine.render(output, 512, 8.123, midi);
    check(effected.effects == effects && ui.engine.pluginChain(3).size() == 1 && near(output.getSample(0, 100), 0.125),
          "VST chain, state and automation follow free movement onto another lane");
    ui.engine.resetPlugins();
    ui.engine.render(output, 512, std::ceil(7.123 * 44100) / 44100, midi);
    check(near(output.getSample(0, 0), 0.125), "a moved track applies its source-zero VST automation from the first audible sample");
    auto stale = *parameter; stale.clipId = "missing";
    check(!ui.engine.ensurePluginAutomation(stale), "a deleted automation owner cannot fall back to a mixer-lane effect");
    auto saved = juce::JSON::toString(ui.engine.captureProject()); Engine reopened;
    const auto reopenError = reopened.restoreProject(saved);
    reopened.project.masterDb = 0; reopened.render(output, 512, 8.123, midi);
    check(reopenError.isEmpty() && reopened.project.clips.size() == 1 && reopened.project.clips[0].effects->plugins.size() == 1 && near(output.getSample(0, 100), 0.125),
          "save/reopen preserves track-owned VST state, automation and PCM after moving lanes: " + reopenError);
    ui.performTimelineAction(TimelineAction::copy, 8, 3); ui.performTimelineAction(TimelineAction::paste, 30.123, 1);
    check(ui.engine.project.clips.size() == 2 && ui.engine.project.clips[0].effects != ui.engine.project.clips[1].effects
          && ui.engine.project.clips[0].effects->plugins[0]->id != ui.engine.project.clips[1].effects->plugins[0]->id,
          "copy/paste clones VST instances and rebinds their automation instead of sharing mutable DSP");
    ui.engine.render(output, 512, 31.123, midi);
    check(near(output.getSample(0, 100), 0.125), "copied VST automation drives its independently hosted effect");
    auto malformed = juce::JSON::parse(saved); malformed["clips"][0]["controls"]["pluginAutomation"][0].getDynamicObject()->setProperty("slotId", "missing");
    check(parseProject(juce::JSON::toString(malformed)).error.isNotEmpty(), "orphaned clip VST automation is rejected before loading plugins");
    Engine old; old.project.masterDb = 0; old.project.followTrackTempo = false; old.project.clips = {neighbour};
    old.project.tracks[0].automation[0].enabled = true; old.project.tracks[0].automation[0].setPoint(0, -6);
    const auto oldError = old.addPlugin(0, plugin);
    if (oldError.isEmpty()) old.plugins[0][0]->processor->getParameters()[0]->setValue(0.5f);
    auto oldSave = old.captureProject(); oldSave.getDynamicObject()->setProperty("version", 10); oldSave["tracks"].getArray()->removeRange(4, trackCount - 4); Engine upgraded;
    const auto upgradeError = upgraded.restoreProject(juce::JSON::toString(oldSave));
    bool migratedOwnership = oldError.isEmpty() && upgradeError.isEmpty() && upgraded.plugins[0].empty() && upgraded.project.clips.size() == 1 && upgraded.project.clips[0].effects->plugins.size() == 1;
    if (migratedOwnership)
    {
        upgraded.project.clips[0].moveTo(4.321); upgraded.project.clips[0].track = 2; upgraded.render(output, 512, 5.321, midi);
        migratedOwnership = near(output.getSample(0, 100), 0.125 * juce::Decibels::decibelsToGain(-6.0f));
    }
    check(migratedOwnership, "v10 lane inserts and automation migrate to audio tracks and follow later moves: " + upgradeError);
    ui.engine.project.clips.resize(1); ui.selectedClip = a.id; ui.engine.project.snap = false;
    const auto exported = report.getSiblingFile("owned-effects.wav");
    const auto exportError = ui.engine.exportWav(exported);
    std::unique_ptr<juce::AudioFormatReader> reader(ui.engine.formats.createReaderFor(exported));
    juce::AudioBuffer<float> exportedPcm(2, 256);
    if (reader) reader->read(&exportedPcm, 0, 256, (juce::int64) std::llround(8.123 * reader->sampleRate), true, true);
    check(exportError.isEmpty() && reader && near(exportedPcm.getSample(0, 100), 0.125), "offline export contains the moved track's automated VST PCM");
    ui.engine.selectedTrack = 3; ui.showInspectorPage(1); ui.timeline.pixelsPerSecond = 27.01; ui.refresh();
    auto image = report.getSiblingFile("owned-effects.png").createOutputStream(); juce::PNGImageFormat png;
    check(image && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *image), "capture the native track-owned automation and Effects rack");
    ui.selectedAutomationTarget = (int) AutomationTarget::plugin; ui.pluginLane = *parameter;
    ui.removeClip();
    check(ui.engine.project.clips.empty() && ui.displayedAutomationName() == "Choose a VST parameter", "deleting a track safely clears the displayed effect-automation owner");
    ui.undo(false);
    check(ui.engine.project.clips.size() == 1 && ui.engine.project.clips[0].effects->plugins.size() == 1, "Undo restores a deleted track with its effect chain retained");
    const juce::File instrument("C:/Program Files/Common Files/VST3/Vital.vst3");
    if (instrument.exists())
    {
        Engine synth; synth.project.clips = {a}; synth.project.clips[0].effects = makeClipEffects(); synth.selectedClip = a.id;
        const auto instrumentError = synth.addPlugin(0, instrument);
        juce::MidiBuffer notes; notes.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0); float peak = 0;
        if (instrumentError.isEmpty()) for (int block = 0; block < 32; ++block)
        { synth.render(output, 512, -1, notes); peak = std::max(peak, output.getMagnitude(0, 512)); notes.clear(); }
        check(instrumentError.isEmpty() && synth.plugins[0].empty() && peak > 0.0001f,
              "track-owned Vital still receives keyboard MIDI and produces audio with the transport stopped: " + instrumentError);
    }
}
}
