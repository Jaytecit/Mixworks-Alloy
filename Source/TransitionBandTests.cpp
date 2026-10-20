#include "Workstation.h"
#include "Beatmatch.h"
#include <cmath>
#include <limits>

namespace mixworks
{
void runTransitionBandTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    Project base; base.followTrackTempo = false; base.bpm = base.tempo.fallback = 120;
    for (int i = 0; i < 3; ++i)
    {
        auto media = std::make_shared<Media>(); media->file = report.getSiblingFile("band-source-" + juce::String(i) + ".wav");
        media->sampleRate = 8000; media->manualBpm = 120; media->manualBeatOrigin = 0; media->name = "Band source " + juce::String(i + 1);
        media->samples.setSize(1, 48 * 8000);
        for (int frame = 0; frame < media->frames(); ++frame)
            media->samples.setSample(0, frame, (float) (.1 * (std::sin(frame * 2 * juce::MathConstants<double>::pi * 40 / 8000)
                + std::sin(frame * 2 * juce::MathConstants<double>::pi * 1000 / 8000) + std::sin(frame * 2 * juce::MathConstants<double>::pi * 3000 / 8000))));
        Clip clip; clip.media = media; clip.track = i; clip.start = i * 32; clip.length = 48;
        clip.introAnchor = 0; clip.outroAnchor = 32; clip.fadeInAnchor = 0; clip.fadeOutAnchor = 48;
        base.clips.push_back(clip);
    }
    base.transitionLinksKnown = true;
    base.transitionLinks = {{base.clips[0].id, base.clips[1].id}, {base.clips[1].id, base.clips[2].id}};
    const auto id = base.clips[1].id;
    const auto stableJson = [](const Project& p) { return juce::JSON::toString(projectToJson(p)); };
    const auto near = [](double a, double b) { return std::abs(a - b) < 1e-6; };
    check(base.validateTransitionLinks().wasOk(), "independent-band fixture has valid source-owned frames");
    Project project = base;
    project.clips[0].controls.automation[2].enabled = true; project.clips[0].controls.automation[2].setPoint(4, -3);
    const auto manual = project.clips[0].controls.automation[2].points;
    const auto nextLink = project.transitionLinks[1];
    for (int band = 0; band < 4; ++band)
    {
        const auto result = project.setTransitionBand(id, band, {band == 1 ? TransitionBandMode::swapFade : TransitionBandMode::crossfade, .75, band == 1 ? 1.0 : 0.0, .5});
        check(result.wasOk(), "apply independent band " + juce::String(band) + ": " + result.getErrorMessage());
    }
    check(project.transitionLinks[1] == nextLink && project.clips[0].controls.automation[2].points.size() == manual.size()
        && project.clips[0].controls.automation[2].points[0].value == manual[0].value, "composed bands preserve the next pair and manual EQ");
    bool stable = true; for (size_t i = 0; i < 3; ++i) stable &= project.clips[i].start == base.clips[i].start && project.clips[i].length == base.clips[i].length
        && project.clips[i].prepared == base.clips[i].prepared && project.clips[i].tempoMap == base.clips[i].tempoMap;
    check(stable && near(project.transitionRange(project.transitionLinks[0]).getEnd(), 48), "band commands retain placements, clocks, PCM and frame geometry");
    for (const auto mode : {TransitionBandMode::crossfade, TransitionBandMode::fadeIn, TransitionBandMode::fadeOut, TransitionBandMode::swap})
    {
        auto trial = base; check(trial.setTransitionBand(id, 0, {mode, 1, mode == TransitionBandMode::swap ? 2.0 : 0.0, .5}).wasOk(), "volume mode is accepted: " + juce::String((int) mode));
        const auto out = trial.transitionPlaybackFor(trial.clips[0]), in = trial.transitionPlaybackFor(trial.clips[1]);
        const bool fadesOut = mode != TransitionBandMode::fadeIn, fadesIn = mode != TransitionBandMode::fadeOut;
        check(near(out.gainAt(0, 48), fadesOut ? 0 : 1) && near(in.gainAt(0, 32), fadesIn ? 0 : 1), "mode treats only its specified side: " + juce::String((int) mode));
        if (mode == TransitionBandMode::swap) check(out.gainAt(0, 40) == 0 && in.gainAt(0, 40) == 0, "positive swap gap cuts both sides at the crossing");
    }
    for (double strength : {.25, .5, .75, 1.})
    {
        auto trial = base; trial.setTransitionBand(id, 2, {TransitionBandMode::fadeOut, strength, 0, .5});
        const auto state = trial.transitionPlaybackFor(trial.clips[0]);
        check(near(state.gainAt(2, 48), 1 - strength) && state.gainAt(1, 48) == 1, "strength changes only the selected band's attenuation depth");
    }
    const auto untouched = stableJson(project);
    check(project.setTransitionBand(id, 1, {TransitionBandMode::swap, 1, 32, .25}).failed() && stableJson(project) == untouched, "an impossible gap is rejected atomically");
    check(project.setTransitionBand(id, 3, {TransitionBandMode::swapFade, 1, 0, .5}).failed()
        && project.setTransitionBand(id, 0, {TransitionBandMode::crossfade, std::numeric_limits<double>::quiet_NaN(), 0, .5}).failed(), "invalid modes and nonfinite strengths are rejected");
    check(project.setTransitionLength(id, 4).wasOk() && project.validateTransitionLinks().wasOk(), "resizing reprojects every generated band through the existing geometry owner");
    auto ramp = project; ramp.followTrackTempo = true; ramp.tempo.points = {{0, 100, true}, {60, 160, true}};
    check(ramp.setTransitionBand(id, 0, {TransitionBandMode::swap, 1, 2, .5}).wasOk(), "beat gap works with a changing tempo map");
    const auto outgoing = ramp.transitionPlaybackFor(ramp.clips[0]).pairs[1][0], incoming = ramp.transitionPlaybackFor(ramp.clips[1]).pairs[0][0];
    check(near(ramp.tempo.beatsAt((incoming.begin + incoming.end) / 2) - ramp.tempo.beatsAt((outgoing.begin + outgoing.end) / 2), 2), "swap separation follows integrated mix beats rather than a fixed seconds estimate");
    const auto parsed = parseProject(stableJson(project));
    check(parsed.error.isEmpty() && parsed.project.transitionLinks == project.transitionLinks, "format 16 round-trips all independent modes, strengths, gaps and positions");
    for (const auto& bad : {juce::var("bad"), juce::var(true), juce::var(-1), juce::var(2)})
    {
        auto value = projectToJson(project); auto& band = value["transitionLinks"][0]["bands"][0];
        band.getDynamicObject()->setProperty("strength", bad);
        check(parseProject(juce::JSON::toString(value)).error.isNotEmpty(), "session rejects malformed band strength");
    }
    auto legacy = projectToJson(base); legacy.getDynamicObject()->setProperty("version", 15); legacy["tracks"].getArray()->removeRange(4, trackCount - 4);
    check(parseProject(juce::JSON::toString(legacy)).error.isEmpty(), "format 15 opens without manufacturing band settings");
    auto gesture = project; check(gesture.editTransition(id, Project::TransitionEdit::resizeEnd, -1, false).wasOk(), "a composed frame can resize");
    project.setTransitionBand(id, 3, {TransitionBandMode::fadeIn, .25, 0, .5});
    check(project.applyPlacements(gesture).wasOk() && project.transitionLinks[0].bands[3]->strength == .25, "geometry commits retain live band settings instead of their mouse-down snapshot");
    check(project.setTransitionLocked(id, true).wasOk(), "composed transition locks");
    const auto locked = stableJson(project);
    check(project.setTransitionBand(id, 1, {}).failed() && stableJson(project) == locked, "locks enforce band protection in the model");
    auto tampered = project; tampered.transitionLinks[0].bands[3]->strength = .75;
    check(project.validateTransitionLocks(tampered).failed(), "indirect commits cannot replace locked settings");
    check(project.setTransitionBand(base.clips[2].id, 2, {TransitionBandMode::crossfade, .5, 0, .5}).failed(), "a neighbouring pair cannot edit bands on a track protected by a lock");
    tampered = project; tampered.transitionLinks[1].bands[2] = TransitionBandSettings {TransitionBandMode::crossfade, .5, 0, .5};
    check(project.validateTransitionLocks(tampered).failed(), "indirect commits also protect neighbour band settings on a locked track");
    project.setTransitionLocked(id, false);
    check(project.applyTransitionPreset(id, TransitionPreset::alignOnly).wasOk()
        && std::none_of(project.transitionLinks[0].bands.begin(), project.transitionLinks[0].bands.end(), [](const auto& b) { return b.has_value(); }), "whole-pair preset resets the composed treatment intentionally");
    auto oldBass = base; check(oldBass.applyTransitionPreset(id, TransitionPreset::bassSwap).wasOk(), "legacy bass producer remains available");
    check(oldBass.setTransitionBand(id, 0, {TransitionBandMode::crossfade, .75, 0, .5}).wasOk()
        && oldBass.transitionLinks[0].bassSwap.has_value(), "independent Volume leaves an existing paired bass handoff intact");
    auto contradictory = oldBass; contradictory.transitionLinks[0].bands[1] = TransitionBandSettings {};
    check(parseProject(stableJson(contradictory)).error.isNotEmpty(), "a session cannot claim both an old bass handoff and a new None setting");
    check(oldBass.setTransitionBand(id, 1, {TransitionBandMode::swap, 1, 2, .5}).wasOk() && !oldBass.transitionLinks[0].bassSwap
        && oldBass.clips[0].controls.automation[1].points.empty() && oldBass.clips[1].controls.automation[1].points.empty(), "explicit Bass replacement removes old owned points to avoid double processing");
    auto manualBand = base; manualBand.clips[0].controls.automation[2].enabled = true; manualBand.clips[0].controls.automation[2].setPoint(36, -7);
    manualBand.setTransitionBand(id, 2, {TransitionBandMode::fadeOut, .5, 0, .5});
    check(manualBand.setTransitionBand(id, 2, {TransitionBandMode::manual, .5, 0, .5}).wasOk()
        && manualBand.transitionPlaybackFor(manualBand.clips[0]).gainAt(2, 47) == 1 && manualBand.clips[0].controls.automation[2].points[0].value == -7,
        "Manual removes the generated treatment while retaining ordinary editable automation");
    // Exercise the real mixer rather than only the curve evaluator.
    const auto render = [&](Project p, double at, int solo = 0)
    {
        p.masterDb = 0; p.tracks[0].mute = solo != 0; p.tracks[1].mute = solo != 1; p.tracks[2].mute = true;
        Engine engine; engine.project = std::move(p); engine.sampleRate = 8000;
        juce::AudioBuffer<float> pcm(2, 512); juce::MidiBuffer midi; double energy = 0;
        for (int n = 0; n < 8; ++n) { engine.render(pcm, 512, at + n * 512. / 8000, midi);
            if (n >= 4) for (int i = 0; i < 512; ++i) energy += pcm.getSample(0, i) * pcm.getSample(0, i); }
        return energy;
    };
    const double dry = render(base, 43);
    auto powerBlend = base;
    check(powerBlend.setTransitionBand(id, 0, {TransitionBandMode::constantPower, 1, 0, .5}).wasOk(), "constant-power mode is available through the shared Volume owner");
    const auto energy = render(powerBlend, 43, 0) + render(powerBlend, 43, 1);
    check(dry > 1 && std::abs(energy / dry - 1) < .01,
        "production mixer retains summed isolated-source power through a constant-power blend without an inherited linear fade");
    const auto savedPower = parseProject(stableJson(powerBlend));
    check(savedPower.error.isEmpty() && savedPower.project.transitionLinks[0].bands == powerBlend.transitionLinks[0].bands,
        "session format 26 retains the constant-power mode without flattening it to a linear crossfade");
    auto oldFormat = projectToJson(base); oldFormat.getDynamicObject()->setProperty("version", 25);
    check(parseProject(juce::JSON::toString(oldFormat)).error.isEmpty(), "existing format-25 sessions remain readable without rewriting analysis");
    check(powerBlend.setTransitionBand(id, 1, {TransitionBandMode::constantPower, 1, 0, .5}).failed(), "constant power rejects unsupported EQ-band use");
    for (int band = 0; band < 4; ++band)
    {
        auto wet = base; wet.setTransitionBand(id, band, {TransitionBandMode::swap, 1, 0, .5});
        check(render(wet, 43) < dry * .97, "production mixer audibly attenuates selected band " + juce::String(band));
    }
    auto gap = base; gap.setTransitionBand(id, 0, {TransitionBandMode::swap, 1, 4, .5});
    gap.clips[0].controls.gainDb = gap.clips[1].controls.gainDb = 6;
    check(render(gap, 39.6) < 1e-10 && dry > 1, "real mixer produces silence inside a full-strength volume gap");
    auto cut = base; cut.setTransitionBand(id, 0, {TransitionBandMode::swap, 1, 0, .5});
    check(render(cut, 36, 0) > 1 && render(cut, 36, 1) < 1e-10 && render(cut, 43, 0) < 1e-10 && render(cut, 43, 1) > 1,
        "production hard-cut playback switches source at the midpoint instead of running both through the full overlap");
    Workstation ui(false); ui.stopTimer(); ui.setSize(1440, 940); ui.engine.project = base; ui.selectTransition(id); ui.engine.position = 5;
    ui.transitionBandModes[0].setSelectedId(2, juce::sendNotificationSync);
    ui.transitionBandModes[1].setSelectedId(5, juce::sendNotificationSync);
    ui.transitionBandGaps[1].setSelectedId(3, juce::sendNotificationSync);
    ui.transitionBandStrengths[0].setSelectedId(75, juce::sendNotificationSync);
    check(ui.undoHistory.size() == 4 && ui.engine.position == 5 && !ui.busy && ui.engine.project.transitionLinks[0].bands[1]->gapBeats == 1
        && ui.engine.project.transitionLinks[0].bands[0]->strength == .75, "native independent controls make one Undo each without seeking or preparing audio");
    const auto viewState = stableJson(ui.engine.project); const auto viewUndo = ui.undoHistory.size(); const auto viewDirty = ui.dirty;
    juce::AudioBuffer<float> audibleBefore(2, 256), audibleAfter(2, 256), uninterrupted(2, 256); juce::MidiBuffer noMidi;
    Engine reference; reference.project = ui.engine.project; reference.sampleRate = ui.engine.sampleRate = 8000;
    reference.render(uninterrupted, 256, 39.6, noMidi); ui.engine.render(audibleBefore, 256, 39.6, noMidi);
    ui.timeline.pixelsPerSecond = 18; ui.refresh(); ui.viewport.setViewPosition(500, 0);
    const auto allCurvesImage = ui.createComponentSnapshot(ui.viewport.getBounds());
    ui.transitionCurveShow[0].setToggleState(false, juce::dontSendNotification); ui.transitionCurveShow[0].onClick();
    check(!ui.transitionCurveVisible(0) && ui.transitionCurveVisible(1), "Show curve hides Volume independently of Bass");
    ui.transitionCurveOnly[1].onClick();
    check(!ui.transitionCurveVisible(0) && ui.transitionCurveVisible(1) && !ui.transitionCurveVisible(2) && !ui.transitionCurveVisible(3),
          "Only curve isolates Bass without an audio-solo command");
    const auto isolatedImage = ui.createComponentSnapshot(ui.viewport.getBounds()); int changedPixels = 0;
    for (int y = 0; y < allCurvesImage.getHeight(); ++y) for (int x = 0; x < allCurvesImage.getWidth(); ++x)
        changedPixels += allCurvesImage.getPixelAt(x, y) != isolatedImage.getPixelAt(x, y);
    check(changedPixels > 20, "native timeline rendering actually hides the other generated curves");
    ui.transitionCurveOnly[2].onClick();
    check(ui.transitionCurveOnly[2].getToggleState() && !ui.transitionCurveOnly[1].getToggleState() && ui.transitionCurveVisible(2) && !ui.transitionCurveVisible(1),
          "isolating Mid replaces Bass isolation with one visible control state");
    ui.transitionCurveOnly[2].onClick();
    check(!ui.transitionCurveVisible(0) && ui.transitionCurveVisible(1) && ui.transitionCurveVisible(2) && ui.transitionCurveVisible(3),
          "ending isolation restores the other visibility choices");
    ui.transitionCurveOnly[0].onClick();
    check(ui.transitionCurveShown[0] && ui.transitionCurveVisible(0), "isolating a hidden curve makes that curve visible");
    ui.transitionCurveShow[0].setToggleState(false, juce::dontSendNotification); ui.transitionCurveShow[0].onClick();
    check(ui.transitionCurveOnlyBand == -1 && ui.transitionCurveVisible(1), "hiding the isolated curve returns to the remaining visible curves");
    ui.transitionCurveShow[0].setToggleState(true, juce::dontSendNotification); ui.transitionCurveShow[0].onClick();
    reference.render(uninterrupted, 256, 39.632, noMidi); ui.engine.render(audibleAfter, 256, 39.632, noMidi); bool identicalAudio = true;
    for (int ch = 0; ch < 2; ++ch) for (int f = 0; f < 256; ++f) identicalAudio &= uninterrupted.getSample(ch, f) == audibleAfter.getSample(ch, f);
    check(identicalAudio && stableJson(ui.engine.project) == viewState && ui.undoHistory.size() == viewUndo && ui.dirty == viewDirty && ui.engine.position == 5,
          "curve visibility and isolation retain exact renderer PCM, session, Undo, dirty state and playhead");
    const auto beforeUndo = stableJson(ui.engine.project); ui.undo(false); ui.undo(true);
    check(stableJson(ui.engine.project) == beforeUndo, "Undo and Redo retain the complete composed transition");
    ui.transitionLock.onClick(); check(!ui.transitionBandModes[0].isEnabled() && !ui.transitionBandGaps[1].isEnabled(), "native lock disables all band modes and settings");
    check(ui.transitionCurveShow[0].isEnabled() && ui.transitionCurveOnly[1].isEnabled(), "locked transitions still allow display-only curve controls");
    ui.transitionLock.onClick(); ui.selectTransition(base.clips[2].id);
    check(ui.transitionBandModes[0].getSelectedId() == 0, "Next pair does not inherit another pair's band treatment");
    ui.selectTransition(id); ui.timeline.pixelsPerSecond = 18; ui.viewport.setViewPosition(500, 0); ui.refresh();
    auto stream = report.getSiblingFile("independent-transition-bands.png").createOutputStream(); juce::PNGImageFormat png;
    check(stream && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *stream), "capture composed controls and shared-renderer curve preview");
    ui.setSize(1000, 620); ui.transitionViewport.setViewPosition(0, ui.transitionBandModes[3].getY());
    check(ui.transitionCurveShow[3].getWidth() > 65 && ui.transitionCurveOnly[3].getWidth() > 65 && ui.transitionCurveShow[3].getBottom() < ui.transitionBandPositions[3].getBottom(),
          "narrow inspector retains usable Show and Only buttons alongside all High settings");
    check(ui.transitionViewport.getViewedComponent()->getHeight() > ui.transitionViewport.getHeight()
        && ui.transitionViewport.getViewArea().intersects(ui.transitionBandModes[3].getBounds()), "small windows retain a scrollable path to High controls");
}
}
