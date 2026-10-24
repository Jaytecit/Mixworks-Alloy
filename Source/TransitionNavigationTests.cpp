#include "Workstation.h"
#include <cmath>

namespace mixworks
{
void runZoomHandleTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    Workstation ui(false); ui.stopTimer(); ui.setSize(1250,900);
    Project base; base.followTrackTempo = false; base.bpm = base.tempo.fallback = 120; base.snap = false;
    auto media = std::make_shared<Media>(); media->sampleRate = 8000; media->samples.setSize(1,48*8000); media->samples.clear(); media->manualBpm = 120; media->name = "Zoomed transition anchors";
    Clip clip; clip.media = media; clip.length = 48; clip.introAnchor = 4; clip.outroAnchor = 32;
    base.clips.push_back(clip); base.transitionLinksKnown = true;
    ui.engine.project = base; ui.selectedClip = clip.id; ui.timeline.pixelsPerSecond = 500; ui.refresh(); ui.viewport.setViewPosition(8000,0);
    const auto json = [&] { return juce::JSON::toString(projectToJson(ui.engine.project)); };
    const auto before = json();
    const auto in = ui.timeline.offscreenAnchorBounds(clip,true), out = ui.timeline.offscreenAnchorBounds(clip,false);
    check(!in.isEmpty() && !out.isEmpty() && in.getRight() < out.getX(),"zoomed view offers separate left IN and right OUT proxy tabs");
    check(json() == before && ui.undoHistory.empty(),"zooming and displaying off-screen handles do not change real anchors");
    const auto mouse = juce::Desktop::getInstance().getMainMouseSource();
    auto event = [&](juce::Point<float> at,juce::Point<float> down,bool drag)
    { return juce::MouseEvent(mouse,at,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::altModifier),1,0,0,0,0,&ui.timeline,&ui.timeline,juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),1,drag); };
    for (bool intro : {true,false})
    {
        ui.engine.project = base; ui.selectedClip = clip.id; ui.undoHistory.clear(); ui.redoHistory.clear(); ui.refresh(); ui.viewport.setViewPosition(8000,0); ui.engine.position = 11.125;
        const auto tab = ui.timeline.offscreenAnchorBounds(clip,intro); const auto down = tab.getCentre(), to = down.translated(intro ? 50.f : -50.f,0);
        const auto hit = ui.timeline.anchorAt(down); check(hit && hit->first == clip.id && hit->second == intro,"proxy hit targets the correct real IN/OUT anchor");
        ui.timeline.mouseDown(event(down,down,false)); check(json() == before,"grabbing the proxy does not jump its actual source anchor");
        ui.timeline.mouseDrag(event(to,down,true)); ui.timeline.mouseUp(event(to,down,true));
        const auto& changed = ui.engine.project.clips.front();
        check(std::abs(changed.transitionTime(intro)-(clip.transitionTime(intro)+(intro ? .1 : -.1))) < 1.e-7 && ui.undoHistory.size() == 1 && ui.engine.position == 11.125,
            "proxy drag adjusts actual anchor by pointer delta with one Undo and no seek");
        ui.undo(false); check(json() == before,"Undo restores the complete off-screen anchor edit");
    }
    ui.engine.project = base; ui.refresh(); ui.viewport.setViewPosition(0,0);
    const auto sameIn = ui.timeline.offscreenAnchorBounds(clip,true), sameOut = ui.timeline.offscreenAnchorBounds(clip,false);
    check(!sameIn.isEmpty() && !sameOut.isEmpty() && !sameIn.intersects(sameOut),"two anchors off the same edge keep distinct reachable tabs");
    ui.viewport.setViewPosition(8000,0);
    auto capture = report.getSiblingFile("zoomed-in-out-handles.png").createOutputStream();
    check(capture && juce::PNGImageFormat().writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()),*capture),"capture visible zoomed IN/OUT edge tabs");
}
void runTransitionNavigationTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    const auto json = [](const Project& p) { return juce::JSON::toString(projectToJson(p), juce::JSON::FormatOptions().withMaxDecimalPlaces(17)); };
    Project base; base.followTrackTempo = false; base.bpm = base.tempo.fallback = 120;
    for (int n = 0; n < 3; ++n)
    {
        auto media = std::make_shared<Media>(); media->sampleRate = 8000; media->samples.setSize(1, 12 * 8000); media->samples.clear();
        media->name = "Navigation source " + juce::String(n + 1); media->manualBpm = 120; media->manualBeatOrigin = 0;
        media->manualKey = n == 0 ? 11 : n == 1 ? 6 : -1;
        Clip clip; clip.media = media; clip.start = n * 8; clip.track = n; clip.length = 12;
        clip.introAnchor = clip.fadeInAnchor = 0; clip.outroAnchor = 8; clip.fadeOutAnchor = 9; clip.fadeOut = 1;
        base.clips.push_back(clip);
    }
    base.transitionLinksKnown = true;
    base.transitionLinks = {{base.clips[0].id, base.clips[1].id}, {base.clips[1].id, base.clips[2].id}};
    Workstation ui(false); ui.stopTimer(); ui.setSize(1440, 940); ui.engine.project = base;
    const int alt = juce::ModifierKeys::altModifier, ctrl = juce::ModifierKeys::ctrlModifier, shift = juce::ModifierKeys::shiftModifier;
    const auto key = [&](int code, int mods) { return ui.keyPressed(juce::KeyPress(code, juce::ModifierKeys(mods), 0)); };
    check(base.validateTransitionLinks().wasOk(), "keyboard transition fixture has interior editable frames");
    const auto id = base.clips[1].id; ui.selectTransition(id);
    check(ui.selectedTransitionLength.getTooltip().contains("Wheel +1") && ui.selectedTransitionLength.getTooltip().contains("same mode"), "inspector derives source key movement using the existing wheel facts");
    ui.engine.project.clips[0].media->manualKey = 4; ui.engine.project.clips[1].media->manualKey = 11; ui.refresh();
    check(ui.selectedTransitionLength.getTooltip().contains("Wheel +1"), "wheel movement wraps from twelve to one without reporting eleven steps");
    ui.engine.project.clips[0].media->manualKey = 11;
    ui.engine.project.clips[1].media->manualKey = 20; ui.refresh();
    check(ui.selectedTransitionLength.getTooltip().contains("Relative major / minor"), "same wheel with another mode is described as relative keys");
    ui.engine.project.clips[1].media->manualKey = -1; ui.engine.project.clips[1].media->analysis.key = 6;
    ui.engine.project.clips[1].media->analysis.keyConfidence = .3; ui.refresh();
    check(ui.selectedTransitionLength.getTooltip().contains("In: " + ui.keyText(*ui.engine.project.clips[1].media)) && !ui.selectedTransitionLength.getTooltip().contains("?") && ui.selectedTransitionLength.getTooltip().contains("Movement unknown"), "uncertain key stays visibly uncertain without a derived movement");
    ui.engine.project.clips[1].media->analysis.keyConfidence = .55; ui.refresh();
    check(ui.selectedTransitionLength.getTooltip().contains("Wheel +1") && !ui.selectedTransitionLength.getTooltip().contains("?"), "confirmed key and key formatting share the same owning confidence boundary");
    ui.engine.project.clips[1].media->analysis.key = -1; ui.refresh();
    check(ui.selectedTransitionLength.getTooltip().contains("Unknown") && ui.selectedTransitionLength.getTooltip().contains("Movement unknown"), "unavailable key is retained as unknown");
    ui.engine.project.clips[1].media->manualKey = 6; ui.engine.project.clips[1].media->analysis.keyConfidence = 0; ui.refresh();
    const auto initial = json(ui.engine.project); const auto undos = ui.undoHistory.size();
    check(key(juce::KeyPress::rightKey, alt) && ui.selectedTransition == base.clips[2].id && ui.undoHistory.size() == undos, "Alt Right navigates by transition identity without Undo");
    check(key(juce::KeyPress::rightKey, alt) && ui.selectedTransition == base.clips[2].id, "keyboard Next remains bounded at the last pair");
    check(key(juce::KeyPress::leftKey, alt) && ui.selectedTransition == id && json(ui.engine.project) == initial, "Alt Left restores the previous selection without editing the session");
    auto emptyFrame = base; emptyFrame.clips[0].fadeOutAnchor = 8; emptyFrame.clips[0].fadeOut = 0;
    ui.engine.project = emptyFrame; ui.selectTransition(id); const auto emptyBefore = json(ui.engine.project);
    key(juce::KeyPress::leftKey, shift | alt);
    check(json(ui.engine.project) == emptyBefore && ui.undoHistory.size() == undos, "keyboard resize before the frame start rejects without an Undo");
    ui.engine.project = base; ui.selectTransition(id);
    ui.busy = true;
    check(!key(juce::KeyPress::rightKey, ctrl | alt) && json(ui.engine.project) == initial, "busy workflow leaves keyboard geometry untouched"); ui.busy = false;
    ui.engine.position = 6.125; const auto before = ui.engine.project.transitionRange(*ui.engine.project.transitionFor(id));
    check(key(juce::KeyPress::rightKey, ctrl | alt), "Ctrl Alt Right handles frame movement");
    const auto moved = ui.engine.project.transitionRange(*ui.engine.project.transitionFor(id));
    check(std::abs(moved.getStart() - before.getStart() - .5) < 1e-7 && std::abs(moved.getLength() - before.getLength()) < 1e-7
        && ui.undoHistory.size() == undos + 1 && ui.engine.position == 6.125, "one-beat keyboard move keeps frame length, transport and one Undo");
    check(ui.engine.project.clips[0].start == base.clips[0].start && ui.engine.project.clips[1].start == base.clips[1].start + .5
        && ui.engine.project.clips[2].start == base.clips[2].start + .5 && ui.engine.project.clips[0].effects == base.clips[0].effects
        && ui.engine.project.clips[1].introAnchor == base.clips[1].introAnchor,
        "keyboard frame edit carries the aligned incoming chain and retains source IN and effect ownership");
    const auto edited = json(ui.engine.project); ui.undo(false); const bool undone = json(ui.engine.project) == initial; ui.undo(true);
    check(undone && json(ui.engine.project) == edited, "keyboard frame movement round-trips through native Undo/Redo");
    check(key(juce::KeyPress::leftKey, shift | alt), "Shift Alt Left handles end resize");
    const auto resized = ui.engine.project.transitionRange(*ui.engine.project.transitionFor(id));
    check(std::abs(resized.getStart() - moved.getStart()) < 1e-7 && std::abs(resized.getEnd() - moved.getEnd() + .5) < 1e-7, "keyboard resize keeps start fixed and changes the end by a beat");
    key('L', alt); const auto locked = json(ui.engine.project); const auto lockedUndo = ui.undoHistory.size();
    check(ui.engine.project.transitionFor(id)->locked && !ui.transitionFilterMode.isEnabled(), "Alt L locks the pair and refreshes protected controls");
    key(juce::KeyPress::leftKey, ctrl | alt); key(juce::KeyPress::rightKey, shift | alt);
    check(json(ui.engine.project) == locked && ui.undoHistory.size() == lockedUndo, "keyboard edits cannot bypass a transition lock");
    key(juce::KeyPress::rightKey, alt); key(juce::KeyPress::rightKey, ctrl | alt);
    check(json(ui.engine.project) == locked && ui.undoHistory.size() == lockedUndo, "keyboard geometry on an unlocked neighbour cannot change a protected track");
    key(juce::KeyPress::leftKey, alt);
    key('L', alt); check(!ui.engine.project.transitionFor(id)->locked, "Alt L unlocks the pair");
    ui.pendingTempo = 121; const auto pending = json(ui.engine.project); const auto pendingUndo = ui.undoHistory.size();
    key(juce::KeyPress::rightKey, ctrl | alt);
    check(json(ui.engine.project) == pending && ui.undoHistory.size() == pendingUndo, "pending tempo preparation rejects keyboard geometry without history"); ui.pendingTempo = 0;
    check(!key(juce::KeyPress::rightKey, ctrl | alt | shift) && !key(juce::KeyPress::rightKey, 0), "conflicting modifiers and ordinary arrows are left to focused controls");
    ui.engine.project = base; ui.engine.project.followTrackTempo = true; ui.engine.project.tempo.points = {{0, 100, true}, {28, 160, true}}; ui.refresh();
    const auto clock = ui.engine.project.tempo; const auto rampBefore = ui.engine.project.transitionRange(*ui.engine.project.transitionFor(id));
    key(juce::KeyPress::rightKey, ctrl | alt); const auto rampAfter = ui.engine.project.transitionRange(*ui.engine.project.transitionFor(id));
    check(std::abs(clock.beatsAt(rampAfter.getStart()) - clock.beatsAt(rampBefore.getStart()) - 1) < 1e-7, "keyboard movement integrates one beat through a tempo ramp");
    // Canvas editing must not invoke the explicit transition navigation seek.
    const auto mouse = juce::Desktop::getInstance().getMainMouseSource();
    auto event = [&](juce::Point<float> position, juce::Point<float> origin, bool drag)
    { return juce::MouseEvent(mouse, position, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1, 0, 0, 0, 0, &ui.timeline, &ui.timeline, juce::Time::getCurrentTime(), origin, juce::Time::getCurrentTime(), 1, drag); };
    for (bool playing : {false, true}) for (bool alreadySelected : {false, true})
        for (auto edit : {Project::TransitionEdit::resizeStart, Project::TransitionEdit::resizeEnd, Project::TransitionEdit::moveWindow})
        {
            ui.engine.setPlaying(false); ui.engine.project = base; ui.engine.project.snap = false;
            ui.selectedTransition = alreadySelected ? id : base.clips[2].id; ui.selectedClip = id;
            ui.undoHistory.clear(); ui.redoHistory.clear(); ui.timeline.pixelsPerSecond = 30; ui.refresh();
            ui.viewport.setViewPosition(16, 0); ui.engine.seek(6.125); ui.engine.setPlaying(playing);
            const auto view = ui.viewport.getViewPosition(); const auto box = ui.timeline.transitionBounds(base.transitionLinks[0]);
            const juce::Point<float> from {edit == Project::TransitionEdit::resizeStart ? box.getX() : edit == Project::TransitionEdit::resizeEnd ? box.getRight() : box.getCentreX(), box.getY() + 8};
            const auto hit = ui.timeline.transitionAt(from);
            check(hit && hit->first == id && hit->second == edit, "transport fixture hits the actual transition width/header control");
            const auto original = json(ui.engine.project);
            ui.timeline.mouseDown(event(from, from, false));
            check(ui.selectedTransition == id && ui.engine.position == 6.125 && ui.engine.playing == playing && ui.viewport.getViewPosition() == view
                && ui.undoHistory.empty() && json(ui.engine.project) == original,
                "grabbing a transition width/header preserves playhead, playback, view and history: " + juce::String((int) edit) + "/" + juce::String((int) playing) + "/" + juce::String((int) alreadySelected));
            const auto to = from.translated(edit == Project::TransitionEdit::resizeEnd ? -15.f : 15.f, 0);
            ui.timeline.mouseDrag(event(to, from, true)); ui.timeline.mouseUp(event(to, from, true));
            const auto range = ui.engine.project.transitionRange(*ui.engine.project.transitionFor(id));
            check(std::abs(range.getStart() - (edit == Project::TransitionEdit::resizeEnd ? 8 : 8.5)) < 1e-7
                && std::abs(range.getEnd() - (edit == Project::TransitionEdit::resizeStart ? 10 : edit == Project::TransitionEdit::resizeEnd ? 9.5 : 10.5)) < 1e-7
                && ui.engine.position == 6.125 && ui.engine.playing == playing && ui.undoHistory.size() == 1,
                "native width/header drag changes only its geometry with one Undo and no transport jump");
            juce::AudioBuffer<float> output(2, 256);
            ui.engine.audioDeviceIOCallbackWithContext(nullptr, 0, output.getArrayOfWritePointers(), 2, 256, {});
            check(std::abs(ui.engine.position - (6.125 + (playing ? 256 / ui.engine.sampleRate.load() : 0))) < 1e-7,
                "next device callback continues from the pre-grab playback clock");
            ui.engine.setPlaying(false); const auto editedGeometry = json(ui.engine.project);
            ui.undo(false); const bool restored = json(ui.engine.project) == original; ui.undo(true);
            check(restored && json(ui.engine.project) == editedGeometry, "transition handle editing retains complete Undo/Redo");
        }
    ui.engine.setPlaying(false); ui.engine.project = base; ui.timeline.pixelsPerSecond = 30; ui.selectTransition(id, true);
    check(ui.engine.position == 8, "explicit transition navigation still seeks to its start");
    auto image = report.getSiblingFile("transition-width-transport.png").createOutputStream(); juce::PNGImageFormat png;
    check(image && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *image), "capture transition width controls after transport-preserving editing");
    ui.engine.project = base; ui.selectTransition(id); ui.setSize(1000, 620);
    check(ui.selectedTransitionLength.getWidth() >= 100 && ui.transitionPresetForSelection.getWidth() >= 120,
        "compact transition panel keeps primary edits reachable without repeated track information");
    check(ui.previousTransition.getTooltip().contains("Ctrl + Alt"), "transition keyboard help remains available on navigation tooltips");

}
}
