#include "Workstation.h"
#include <cmath>

namespace mixworks
{
void runBarNavigationTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    const auto json = [](const Project& p) { return juce::JSON::toString(projectToJson(p), juce::JSON::FormatOptions().withMaxDecimalPlaces(17)); };
    Project base; base.followTrackTempo = false; base.bpm = base.tempo.fallback = 120;
    for (int n = 0; n < 2; ++n)
    {
        auto media = std::make_shared<Media>(); media->sampleRate = 8000; media->samples.setSize(1, 160 * 8000); media->samples.clear();
        media->manualBpm = 120; media->manualBeatOrigin = 0; media->name = "Bar source " + juce::String(n + 1); media->file = report.getSiblingFile("bar-source-" + juce::String(n) + ".wav");
        Clip clip; clip.media = media; clip.length = 160; clip.start = 64 + n * 128; clip.track = n;
        clip.introAnchor = clip.fadeInAnchor = 0; clip.outroAnchor = 128; clip.fadeOutAnchor = 160; base.clips.push_back(clip);
    }
    const auto selected = base.clips[1].id; base.transitionLinksKnown = true; base.transitionLinks = {{base.clips[0].id, selected}};
    Workstation ui(false); ui.stopTimer(); ui.setSize(1440, 940);
    const auto reset = [&] { ui.engine.project = base; ui.selectedClip = selected; ui.selectedTransition = selected; ui.undoHistory.clear(); ui.redoHistory.clear(); ui.dirty = false; ui.busy = false; ui.pendingTempo = 0; ui.tempoPreparationPending = false; ui.engine.position = 120; ui.engine.playing = true; ui.refresh(); };
    const auto command = [&](int menuIndex, const juce::String& group, int bars, bool* enabled = nullptr)
    {
        if (enabled) *enabled = false; auto menu = ui.getMenuForIndex(menuIndex, {});
        juce::PopupMenu::MenuItemIterator outer(menu);
        while (outer.next()) if (outer.getItem().text == group && outer.getItem().subMenu)
        {
            const bool parentEnabled = outer.getItem().isEnabled; juce::PopupMenu::MenuItemIterator inner(*outer.getItem().subMenu);
            while (inner.next()) if (inner.getItem().text == juce::String(bars) + " bars")
            { if (enabled) *enabled = parentEnabled && inner.getItem().isEnabled; return inner.getItem().itemID; }
        }
        return 0;
    };
    for (int bars : {4, 8, 16, 32}) for (int direction : {-1, 1})
    {
        reset(); const auto before = json(ui.engine.project);
        const auto id = command(2, direction < 0 ? "Jump backward" : "Jump forward", bars);
        ui.menuItemSelected(id, 2);
        check(id > 0 && std::abs(ui.engine.position - 120 - direction * bars * 2.) < 1e-7 && ui.engine.playing
            && ui.undoHistory.empty() && !ui.dirty && json(ui.engine.project) == before, "native menu jump " + juce::String(direction * bars) + " bars preserves the musical session and running transport");
        reset(); const auto* old = ui.engine.project.clipFor(selected); const auto oldEffects = old->effects; const auto oldIntro = old->introAnchor;
        const auto move = command(1, direction < 0 ? "Move linked track backward" : "Move linked track forward", bars);
        ui.menuItemSelected(move, 1); const auto* now = ui.engine.project.clipFor(selected);
        check(move > 0 && std::abs(now->start - base.clips[1].start - direction * bars * 2.) < 1e-7
            && now->effects == oldEffects && now->introAnchor == oldIntro && ui.engine.position == 120 && ui.engine.playing
            && ui.undoHistory.size() == 1 && ui.dirty && ui.engine.project.validateTransitionLinks().wasOk(), "native menu linked-track move " + juce::String(direction * bars) + " bars keeps clocks, effects and one Undo");
    }
    const auto moved = json(ui.engine.project); ui.engine.playing = false; ui.undo(false); const bool undone = json(ui.engine.project) == json(base); ui.undo(true);
    check(undone && json(ui.engine.project) == moved, "bar movement restores its whole linked graph through Undo/Redo");
    reset(); ui.engine.position = 1; ui.jumpBars(32, -1); check(ui.engine.position == 0 && ui.undoHistory.empty(), "backward bar jump clamps at mix start without history");
    ui.engine.position = base.audibleEnd() - 1; ui.jumpBars(32, 1); check(ui.engine.position == base.audibleEnd() && ui.engine.playing, "forward bar jump clamps at audible end");
    reset(); ui.engine.project.followTrackTempo = true; ui.engine.project.tempo.points = {{0, 100, true}, {500, 160, true}}; const auto clock = ui.engine.project.tempo;
    const double cursorBeat = clock.beatsAt(ui.engine.position); ui.jumpBars(8, 1);
    check(std::abs(clock.beatsAt(ui.engine.position) - cursorBeat - 32) < 1e-7, "bar jump integrates the changing tempo map");
    const double startBeat = clock.beatsAt(ui.engine.project.clipFor(selected)->start); ui.moveTrackBars(4, 1);
    check(std::abs(clock.beatsAt(ui.engine.project.clipFor(selected)->start) - startBeat - 16) < 1e-7, "linked-track step integrates mix beats at the selected start");
    reset(); ui.engine.project.setTransitionLocked(selected, true); const auto locked = json(ui.engine.project); bool enabled = true;
    const auto lockedCommand = command(1, "Move linked track forward", 4, &enabled); ui.menuItemSelected(lockedCommand, 1);
    check(!enabled && json(ui.engine.project) == locked && ui.undoHistory.empty(), "native disabled move menu and handler enforce transition locks");
    reset(); ui.engine.project.moveClip(base.clips[0].id, 0, false, 0, false); const auto edge = json(ui.engine.project);
    ui.moveTrackBars(32, -1); check(json(ui.engine.project) == edge && ui.undoHistory.empty(), "a whole bar step cannot push a linked predecessor before the mix");
    reset(); const auto waiting = json(ui.engine.project); ui.pendingTempo = 121; ui.jumpBars(4, 1); ui.moveTrackBars(4, 1);
    bool pendingEnabled = true; command(2, "Jump forward", 4, &pendingEnabled);
    check(!pendingEnabled && ui.engine.position == 120 && json(ui.engine.project) == waiting && ui.undoHistory.empty(), "pending preparation disables menus and rejects bar commands");
    ui.pendingTempo = 0; ui.busy = true; ui.jumpBars(4, 1); ui.moveTrackBars(4, 1);
    check(ui.engine.position == 120 && json(ui.engine.project) == waiting && ui.undoHistory.empty(), "busy workflow retains position and arrangement");
    reset(); ui.jumpBars(3, 1); ui.moveTrackBars(4, 0);
    check(ui.engine.position == 120 && json(ui.engine.project) == json(base) && ui.undoHistory.empty(), "unsupported private command values leave the session unchanged");

    // Malformed but owned automation is still caller data. Cloning must reject
    // it without recording an arrangement edit or fabricating a replacement slot.
    reset(); ui.engine.playing = false; PluginAutomation missing; missing.slotId = "owned-unavailable-slot"; missing.parameterId = "owned-parameter";
    ui.engine.project.clipFor(selected)->controls.pluginAutomation.push_back(missing);
    const auto rejected = json(ui.engine.project); const auto rejectedEffects = ui.currentClip()->effects;
    ui.duplicateClip();
    check(json(ui.engine.project) == rejected && ui.selectedClip == selected && ui.currentClip()->effects == rejectedEffects
        && ui.undoHistory.empty() && ui.redoHistory.empty() && !ui.dirty && ui.status.containsIgnoreCase("unavailable"), "failed owned-effect clone preserves project, selection, ownership, history and dirty state");
    reset(); ui.engine.playing = false; const auto sourceEffects = ui.currentClip()->effects; ui.duplicateClip();
    check(ui.engine.project.clips.size() == 3 && ui.currentClip() && ui.currentClip()->effects != sourceEffects
        && ui.undoHistory.size() == 1 && ui.dirty && ui.engine.project.validateTransitionLinks().wasOk(), "successful duplicate prepares a distinct owner before one atomic commit");
    const auto duplicated = json(ui.engine.project); ui.undo(false); const bool duplicateUndone = json(ui.engine.project) == json(base); ui.undo(true);
    check(duplicateUndone && json(ui.engine.project) == duplicated, "successful duplicate still round-trips through Undo/Redo");
}
}
