#include "Workstation.h"
#include <cmath>
#include <limits>

namespace mixworks
{
void runTransitionSwapTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    const auto json = [](const Project& p) { return juce::JSON::toString(projectToJson(p), juce::JSON::FormatOptions().withMaxDecimalPlaces(17)); };
    Project base; base.followTrackTempo = false; base.bpm = base.tempo.fallback = 120;
    for (int n = 0; n < 3; ++n)
    {
        auto media = std::make_shared<Media>(); media->sampleRate = 8000; media->manualBpm = 120; media->manualBeatOrigin = 0; media->name = "Paired source " + juce::String(n + 1);
        media->file = report.getSiblingFile("paired-source-" + juce::String(n) + ".wav");
        media->samples.setSize(1, 48 * 8000); for (int i = 0; i < media->frames(); ++i) media->samples.setSample(0, i, .1f * (float) std::sin(i * (.03 + n * .02)));
        Clip clip; clip.media = media; clip.length = 48; clip.start = n * 32; clip.track = n;
        clip.introAnchor = clip.fadeInAnchor = 0; clip.outroAnchor = 32; clip.fadeOutAnchor = 48; base.clips.push_back(clip);
    }
    base.transitionLinksKnown = true; base.transitionLinks = {{base.clips[0].id, base.clips[1].id}, {base.clips[1].id, base.clips[2].id}};
    const auto id = base.clips[1].id;
    // Pair the right-clicked receiver from an existing full cut on its partner.
    auto manual = base;
    auto& sourceBass = manual.clips[0].controls.automation[1]; sourceBass.enabled = true;
    for (const auto& point : std::array<AutomationPoint, 6>{{{0, 0}, {36, 0}, {36.004, -60}, {39, -60}, {39.004, 0}, {44, 0}}}) sourceBass.setPoint(point.time, point.value);
    sourceBass.setPoint(44.004, -60);
    Project::BassSwapPairing pairing;
    auto pairedManual = manual; const auto sourceBefore = juce::JSON::toString(projectToJson(manual)["clips"][0]);
    const auto pairedResult = pairedManual.pairBassSwap(id, 43.9, pairing);
    const auto* newLink = pairedManual.transitionFor(id);
    check(pairedResult.wasOk() && pairing.changed && pairing.incoming == id && pairing.sourceClip == base.clips[0].id && newLink->bassSwap
        && std::abs((*newLink->bassSwap)[0] - 44) < 1e-7 && std::abs((*newLink->bassSwap)[2] - 12) < 1e-7,
        "Pair bass swap chooses the nearest full cut by mix time and writes source coordinates to the clicked receiver: " + pairedResult.getErrorMessage());
    if (pairedResult.failed()) return;
    check(juce::JSON::toString(projectToJson(pairedManual)["clips"][0]) == sourceBefore
        && pairedManual.clips[1].controls.automation[1].active() && pairedManual.clips[1].controls.automation[1].points[0].value == -60
        && pairedManual.clips[1].controls.automation[1].points[1].value == 0
        && pairedManual.clips[1].start == manual.clips[1].start && pairedManual.clips[1].effects == manual.clips[1].effects,
        "pairing preserves the source cut, placements and effects and makes the receiving bass change complementary");
    const auto pairedJson = json(pairedManual); const auto parsedPair = parseProject(pairedJson);
    check(parsedPair.error.isEmpty() && parsedPair.project.transitionLinks == pairedManual.transitionLinks, "newly paired manual bass persists in the existing session representation: " + parsedPair.error);
    check(pairedManual.pairBassSwap(id, 43.9, pairing).wasOk() && !pairing.changed && json(pairedManual) == pairedJson, "repeating the same pairing is an idempotent operation");
    auto firstCut = manual; check(firstCut.pairBassSwap(id, 36.1, pairing).wasOk() && std::abs((*firstCut.transitionFor(id)->bassSwap)[0] - 36) < 1e-7,
        "the click location selects an earlier cut without using curve or lane order");
    auto reverseRole = base; reverseRole.clips[1].controls.automation[1].enabled = true;
    reverseRole.clips[1].controls.automation[1].setPoint(8, 0); reverseRole.clips[1].controls.automation[1].setPoint(8.004, -60);
    check(reverseRole.pairBassSwap(base.clips[0].id, 40, pairing).wasOk() && reverseRole.clips[0].controls.automation[1].points[0].value == -60,
        "right-clicking an outgoing track still makes that clicked track the receiver");
    auto rampPair = manual; rampPair.followTrackTempo = true; rampPair.tempo.points = {{0, 90, true}, {100, 180, true}};
    rampPair.clips[1].tempoSourceBpm = 100; rampPair.clips[1].beatmatched = true; rampPair.clips[1].tempoMap = std::make_shared<const TempoMap>(rampPair.tempo);
    check(rampPair.pairBassSwap(id, 43.9, pairing).wasOk()
        && std::abs(rampPair.clips[1].start + rampPair.clips[1].localTimeForSource((*rampPair.transitionFor(id)->bassSwap)[2]) - 44) < 1e-7,
        "pairing uses each clip's source clock across a tempo ramp");
    auto reusable = manual; auto& reuse = reusable.clips[1].controls.automation[1]; reuse.enabled = true;
    reuse.setPoint(1, -12); reuse.setPoint(12 + 1e-9, -60); reuse.setPoint(12.004, 0); reuse.setPoint(15, -3);
    check(reusable.pairBassSwap(id, 43.9, pairing).wasOk() && reusable.clips[1].controls.automation[1].points.size() == 4
        && (*reusable.transitionFor(id)->bassSwap)[2] == 12 + 1e-9 && reusable.clips[1].controls.automation[1].points.front().value == -12 && reusable.clips[1].controls.automation[1].points.back().value == -3,
        "compatible receiving points retain exact identities and unrelated manual points");
    // Failure cases retain complete musical state rather than choosing a different cut.
    auto reject = [&](Project trial, const juce::String& target, double at, const juce::String& label)
    { const auto before = json(trial); const auto result = trial.pairBassSwap(target, at, pairing); check(result.failed() && json(trial) == before, label + ": " + result.getErrorMessage()); };
    reject(manual, id, 40.004, "equidistant full cuts reject ambiguous pairing atomically");
    auto partialCut = base; partialCut.clips[0].controls.automation[1].enabled = true; partialCut.clips[0].controls.automation[1].setPoint(40, 0); partialCut.clips[0].controls.automation[1].setPoint(40.004, -59.9f);
    reject(partialCut, id, 40, "a partial bass cut is not silently treated as a full cut");
    auto dormant = manual; dormant.clips[0].controls.automation[1].enabled = false; reject(dormant, id, 44, "disabled source automation is not an audible cut");
    auto blocked = manual; blocked.clips[1].controls.automation[1].enabled = true; blocked.clips[1].controls.automation[1].setPoint(12.002, -7);
    reject(blocked, id, 44, "receiving manual points are preserved instead of overwritten");
    blocked.clips[1].controls.automation[1].enabled = false; reject(blocked, id, 44, "pairing cannot reactivate an unrelated dormant receiver curve");
    auto unlinked = manual; unlinked.transitionLinks.clear(); reject(unlinked, id, 44, "unlinked tracks do not acquire guessed partners");
    auto protectedPair = manual; protectedPair.setTransitionLocked(base.clips[2].id, true); reject(protectedPair, id, 44, "a neighbouring lock protects the receiving track");
    auto generatedCut = manual; generatedCut.setTransitionBand(id, 1, {TransitionBandMode::fadeOut, 1, 0, .5}); reject(generatedCut, id, 44, "pairing does not silently replace generated Bass treatment");
    reject(pairedManual, id, 36, "a different existing pair keeps its ownership");
    reject(manual, "missing", 44, "missing clicked identity does not fall back to a lane");
    reject(manual, id, std::numeric_limits<double>::quiet_NaN(), "nonfinite pairing locations reject atomically");
    const auto isolatedEnergy = [](Project p, int lane, double time)
    {
        for (int i = 0; i < trackCount; ++i) p.tracks[(size_t) i].mute = i != lane;
        Engine e; e.sampleRate = 8000; e.project = std::move(p); juce::AudioBuffer<float> block(2, 512); juce::MidiBuffer midi;
        double energy = 0; for (int n = 0; n < 4; ++n) { e.render(block, 512, time + n * 512 / 8000., midi); for (int i = 128; i < 512; ++i) energy += block.getSample(0, i) * block.getSample(0, i); } return energy;
    };
    check(isolatedEnergy(pairedManual, 1, 40) < isolatedEnergy(manual, 1, 40) * .15
        && std::abs(isolatedEnergy(pairedManual, 1, 45) - isolatedEnergy(manual, 1, 45)) < 1e-6
        && isolatedEnergy(pairedManual, 0, 40) == isolatedEnergy(manual, 0, 40),
        "actual renderer cuts the receiving bass before the handoff, restores it after, and leaves source audio unchanged");
    {
        Workstation pairUi(false); pairUi.stopTimer(); pairUi.engine.project = manual; pairUi.selectedClip = id; pairUi.engine.selectedTrack = 3;
        pairUi.engine.position = 43; pairUi.engine.playing = true; pairUi.refresh(); const auto history = pairUi.undoHistory.size();
        const auto menu = pairUi.timelineMenu(43.9, 1);
        bool found = false; for (juce::PopupMenu::MenuItemIterator it(menu); it.next();)
            if (it.getItem().itemID == (int) TimelineAction::pairBassSwap) found = it.getItem().isEnabled;
        check(found, "RMB menu exposes Pair bass swap on the clicked linked track");
        pairUi.performTimelineAction(TimelineAction::pairBassSwap, 43.9, 1);
        check(pairUi.engine.project.transitionFor(id)->bassSwap && pairUi.undoHistory.size() == history + 1 && pairUi.selectedClip == id
            && pairUi.engine.selectedTrack == 1 && pairUi.engine.position == 43 && pairUi.engine.playing && pairUi.timeline.swapHandles().size() == 2,
            "actual RMB handler uses clicked clip identity, exposes linked handles and records one Undo without seeking");
        pairUi.performTimelineAction(TimelineAction::pairBassSwap, 43.9, 1);
        check(pairUi.undoHistory.size() == history + 1, "repeating RMB pairing adds no empty Undo transaction");
        const auto edited = json(pairUi.engine.project); pairUi.undo(false);
        check(json(pairUi.engine.project) == json(manual), "Undo restores the unpaired receiver and original source");
        pairUi.undo(true); check(json(pairUi.engine.project) == edited, "Redo restores the same explicit pair");
        pairUi.setSize(1920, 1080); pairUi.timeline.pixelsPerSecond = 48; pairUi.refresh();
        pairUi.viewport.setViewPosition(1700, 0);
        auto output = report.getSiblingFile("rmb-pair-bass-swap.png").createOutputStream(); juce::PNGImageFormat png;
        check(output && png.writeImageToStream(pairUi.createComponentSnapshot(pairUi.getLocalBounds()), *output), "capture the RMB-created bass pair and its existing linked handles");
        pairUi.engine.project = blocked; pairUi.selectedClip = id; const auto kept = json(pairUi.engine.project); const auto undos = pairUi.undoHistory.size();
        pairUi.performTimelineAction(TimelineAction::pairBassSwap, 43.9, 1);
        check(json(pairUi.engine.project) == kept && pairUi.undoHistory.size() == undos, "failed native pairing keeps all data without adding Undo");
    }
    auto legacy = base; legacy.clips[0].controls.automation[1].enabled = true; legacy.clips[0].controls.automation[1].setPoint(0, -3);
    legacy.transitionLinks[0].bassSwap = initialiseTransition(legacy.clips[0], legacy.clips[1], 16, TransitionPreset::bassSwap, legacy.transitionRange(legacy.transitionLinks[0]));
    check(legacy.transitionLinks[0].bassSwap.has_value(), "prepare explicitly owned historical bass pair");
    const auto oldPositions = *legacy.transitionFor(id)->bassSwap; const auto oldTimes = *legacy.transitionSwapTimes(*legacy.transitionFor(id), 1);
    for (bool side : {false, true})
    {
        auto trial = legacy; const auto result = trial.setTransitionSwapTime(id, 1, side, oldTimes[side ? 1 : 0] + 1);
        const auto& positions = *trial.transitionFor(id)->bassSwap; bool paired = result.wasOk();
        for (size_t i = 0; i < 4; ++i) paired &= std::abs(positions[i] - oldPositions[i] - 1) < 1e-7;
        check(paired && trial.clips[0].controls.automation[1].points.front().value == -3, "either legacy handle shifts all four owned points and retains manual data");
        auto parsed = parseProject(json(trial)); check(parsed.error.isEmpty() && parsed.project.transitionLinks == trial.transitionLinks, "moved legacy pair persists through the session parser: " + parsed.error);
    }
    auto conflict = legacy; conflict.clips[0].controls.automation[1].setPoint(41, -7); const auto conflictBefore = json(conflict);
    check(conflict.setTransitionSwapTime(id, 1, false, 42).failed() && json(conflict) == conflictBefore, "legacy pair cannot cross an unrelated manual bass point");
    auto invalid = legacy; const auto invalidBefore = json(invalid);
    check(invalid.setTransitionSwapTime(id, 1, false, 100).failed() && json(invalid) == invalidBefore, "legacy pair outside its frame rejects atomically");
    check(invalid.setTransitionSwapTime(id, 1, false, std::numeric_limits<double>::quiet_NaN()).failed()
        && invalid.setTransitionSwapTime(id, 3, false, 40).failed(), "nonfinite and unpaired targets reject without inferred ownership");
    auto generated = base;
    for (int band = 0; band < 4; ++band) generated.setTransitionBand(id, band, {TransitionBandMode::swap, .75, 2, .5});
    for (int band = 0; band < 4; ++band) for (bool side : {false, true})
    {
        auto trial = generated; const auto times = *trial.transitionSwapTimes(*trial.transitionFor(id), band);
        const auto result = trial.setTransitionSwapTime(id, band, side, times[side ? 1 : 0] + 1);
        const auto moved = *trial.transitionSwapTimes(*trial.transitionFor(id), band); const auto setting = *trial.transitionFor(id)->bands[(size_t) band];
        check(result.wasOk() && std::abs(moved[0] - times[0] - 1) < 1e-7 && std::abs(moved[1] - times[1] - 1) < 1e-7
            && setting.gapBeats == 2 && setting.strength == .75 && setting.mode == TransitionBandMode::swap,
            "paired generated band " + juce::String(band) + " moves from either side with retained gap/depth");
    }
    auto fade = base; fade.setTransitionBand(id, 1, {TransitionBandMode::swapFade, 1, 2, .5});
    const auto fadeTimes = *fade.transitionSwapTimes(*fade.transitionFor(id), 1);
    check(fade.setTransitionSwapTime(id, 1, true, fadeTimes[1] + .5).wasOk() && fade.transitionFor(id)->bands[1]->mode == TransitionBandMode::swapFade, "Bass swap fade retains its ramp while moving the pair");
    auto ramp = generated; ramp.followTrackTempo = true; ramp.tempo.points = {{0, 100, true}, {100, 160, true}};
    const auto clock = ramp.tempo; const auto rampTimes = *ramp.transitionSwapTimes(*ramp.transitionFor(id), 2);
    const auto rampResult = ramp.setTransitionSwapTime(id, 2, true, clock.timeAtBeat(clock.beatsAt(rampTimes[1]) + 2));
    const auto rampMoved = *ramp.transitionSwapTimes(*ramp.transitionFor(id), 2);
    check(rampResult.wasOk() && std::abs(clock.beatsAt(rampMoved[0]) - clock.beatsAt(rampTimes[0]) - 2) < 1e-7
        && std::abs(clock.beatsAt(rampMoved[1]) - clock.beatsAt(rampMoved[0]) - 2) < 1e-7, "pair displacement and gap follow integrated beats through tempo ramps");
    generated.setTransitionLocked(base.clips[2].id, true); const auto protectedBefore = json(generated);
    check(generated.setTransitionSwapTime(id, 1, false, 41).failed() && json(generated) == protectedBefore, "neighbour locks protect a paired swap's shared track"); generated.setTransitionLocked(base.clips[2].id, false);
    const auto energy = [](Project project)
    { Engine engine; engine.project = std::move(project); engine.sampleRate = 8000; juce::AudioBuffer<float> block(2, 512); juce::MidiBuffer midi; double sum = 0;
      for (int n = 0; n < 4; ++n) { engine.render(block, 512, 40.25 + n * 512 / 8000., midi); for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 512; ++i) sum += block.getSample(ch, i) * block.getSample(ch, i); } return sum; };
    auto audible = base; audible.setTransitionBand(id, 0, {TransitionBandMode::swap, 1, 4, .5}); const auto silent = energy(audible);
    const auto audibleTimes = *audible.transitionSwapTimes(*audible.transitionFor(id), 0); audible.setTransitionSwapTime(id, 0, false, audibleTimes[0] + 2);
    check(silent < 1e-12 && energy(audible) > 1, "production PCM follows the moved off/on pair instead of a display-only handle");

    Workstation ui(false); ui.stopTimer(); ui.setSize(1440, 940); ui.engine.project = generated; ui.selectTransition(id);
    ui.timeline.pixelsPerSecond = 30; ui.refresh(); ui.viewport.setViewPosition(900, 0); ui.engine.position = 10.25; ui.engine.playing = true;
    auto handles = ui.timeline.swapHandles(); check(handles.size() == 8, "selected generated pair exposes two labelled handles for every swap band");
    const auto handle = *std::find_if(handles.begin(), handles.end(), [](const auto& h) { return h.band == 1 && h.incomingSide; });
    const auto mouse = juce::Desktop::getInstance().getMainMouseSource();
    const auto event = [&](juce::Point<float> point, juce::Point<float> down, bool drag, int modifiers = 0)
    { return juce::MouseEvent(mouse, point, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier | modifiers), 1, 0, 0, 0, 0, &ui.timeline, &ui.timeline, juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), 1, drag); };
    const auto down = handle.bounds.getCentre(); const auto firstTarget = down.translated(27, 0), lastTarget = down.translated(39, 0);
    const auto undos = ui.undoHistory.size(); ui.timeline.mouseDown(event(down, down, false)); ui.timeline.mouseUp(event(down, down, false));
    check(ui.undoHistory.size() == undos && ui.engine.position == 10.25, "clicking a paired handle neither creates Undo nor seeks the transport");
    ui.timeline.mouseDown(event(down, down, false)); ui.timeline.mouseDrag(event(firstTarget, down, true, juce::ModifierKeys::altModifier));
    ui.engine.project.masterDb = -8; ui.engine.project.setTransitionBand(id, 2, {TransitionBandMode::swap, .25, 2, .5});
    ui.timeline.mouseDrag(event(lastTarget, down, true, juce::ModifierKeys::altModifier)); ui.timeline.mouseUp(event(lastTarget, down, true, juce::ModifierKeys::altModifier));
    const auto nativeTimes = *ui.engine.project.transitionSwapTimes(*ui.engine.project.transitionFor(id), 1);
    check(std::abs(nativeTimes[1] - handle.time - 1.3) < 1e-6 && std::abs(nativeTimes[1] - nativeTimes[0] - 1) < 1e-6
        && ui.undoHistory.size() == undos + 1 && ui.engine.position == 10.25 && ui.engine.playing,
        "actual free native handle gesture moves both sides with one Undo and retains live transport");
    check(ui.engine.project.masterDb == -8 && ui.engine.project.transitionFor(id)->bands[2]->strength == .25
        && ui.engine.project.clips[1].start == base.clips[1].start && ui.engine.project.clips[1].effects == base.clips[1].effects,
        "paired drag retains unrelated live controls, song placements and effect ownership");
    ui.engine.playing = false; const auto edited = json(ui.engine.project); ui.undo(false); ui.undo(true);
    check(json(ui.engine.project) == edited, "native paired gesture round-trips through Undo/Redo");
    auto parsed = parseProject(edited); check(parsed.error.isEmpty() && parsed.project.transitionLinks == ui.engine.project.transitionLinks, "native continuous swap positions persist exactly: " + parsed.error);
    ui.transitionLock.onClick(); const auto locked = json(ui.engine.project); const auto lockedUndo = ui.undoHistory.size();
    const auto lockedHandles = ui.timeline.swapHandles(); const auto lockedDown = lockedHandles.front().bounds.getCentre();
    ui.timeline.mouseDown(event(lockedDown, lockedDown, false)); ui.timeline.mouseDrag(event(lockedDown.translated(30, 0), lockedDown, true)); ui.timeline.mouseUp(event(lockedDown, lockedDown, true));
    check(json(ui.engine.project) == locked && ui.undoHistory.size() == lockedUndo, "native locked handles reject a drag without an Undo entry"); ui.transitionLock.onClick();
    ui.transitionCurveShow[1].setToggleState(false, juce::dontSendNotification); ui.transitionCurveShow[1].onClick();
    const auto hidden = ui.timeline.swapHandles(); check(hidden.size() == 6 && std::none_of(hidden.begin(), hidden.end(), [](const auto& h) { return h.band == 1; }), "hidden swap curves remove their handles and hit targets");
    ui.transitionCurveOnly[2].onClick(); check(ui.timeline.swapHandles().size() == 2, "curve isolation exposes only its own paired handles"); ui.transitionCurveOnly[2].onClick();
    ui.transitionCurveShow[1].setToggleState(true, juce::dontSendNotification); ui.transitionCurveShow[1].onClick();
    auto stream = report.getSiblingFile("paired-swap-drag-handles.png").createOutputStream(); juce::PNGImageFormat png;
    check(stream && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *stream), "capture native labelled paired swap handles");
    ui.engine.project = legacy; ui.selectTransition(id); ui.timeline.pixelsPerSecond = 48; ui.refresh();
    const auto legacyHandles = ui.timeline.swapHandles(); check(legacyHandles.size() == 2 && legacyHandles.front().legacy, "legacy Bass swap exposes its same paired handle workflow");
    const auto legacyDown = legacyHandles.front().bounds.getCentre(); ui.engine.project.snap = true;
    ui.timeline.mouseDown(event(legacyDown, legacyDown, false)); ui.timeline.mouseDrag(event(legacyDown.translated(28, 0), legacyDown, true)); ui.timeline.mouseUp(event(legacyDown, legacyDown, true));
    const auto snapped = *ui.engine.project.transitionSwapTimes(*ui.engine.project.transitionFor(id), 1);
    check(std::abs(std::remainder(snapped[0] * 2, 1)) < 1e-7 && std::abs(snapped[1] - snapped[0]) < 1e-7, "native legacy drag snaps both paired lines to the same mix beat");
    ui.viewport.setViewPosition(std::max(0, (int) (snapped[0] * ui.timeline.pixelsPerSecond) - ui.viewport.getViewWidth() / 2), 0);
    auto legacyStream = report.getSiblingFile("legacy-bass-swap-handles.png").createOutputStream();
    check(legacyStream && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *legacyStream), "capture native legacy Bass swap paired handles");
    ui.setSize(1000, 620); ui.timeline.pixelsPerSecond = 8; ui.refresh();
    const auto narrow = ui.timeline.swapHandles();
    if (!narrow.empty()) ui.viewport.setViewPosition(std::max(0, (int) narrow.front().bounds.getCentreX() - ui.viewport.getViewWidth() / 2), 0);
    check(narrow.size() == 2 && ui.timeline.swapHandleAt(narrow.front().bounds.getCentre()).has_value()
        && ui.viewport.getViewArea().contains(narrow.front().bounds.getCentre().toInt()), "paired handles remain visible and hit-testable at narrow size and low zoom");
}
}
