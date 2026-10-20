#include "Workstation.h"
#include <cmath>

namespace mixworks
{
namespace
{
struct BarCommand { int id; bool move; int direction, bars; };
constexpr std::array<BarCommand, 16> commands {{{2400, false, -1, 4}, {2401, false, -1, 8}, {2402, false, -1, 16}, {2403, false, -1, 32},
    {2404, false, 1, 4}, {2405, false, 1, 8}, {2406, false, 1, 16}, {2407, false, 1, 32},
    {2408, true, -1, 4}, {2409, true, -1, 8}, {2410, true, -1, 16}, {2411, true, -1, 32},
    {2412, true, 1, 4}, {2413, true, 1, 8}, {2414, true, 1, 16}, {2415, true, 1, 32}}};
bool validStep(int bars, int direction) { return (bars == 4 || bars == 8 || bars == 16 || bars == 32) && (direction == -1 || direction == 1); }
}
void Workstation::addBarMenus(juce::PopupMenu& menu, bool moveTrack)
{
    const auto* clip = currentClip();
    const bool ready = !busy && pendingTempo <= 0 && !tempoPreparationPending && !engine.project.hasPendingPlacement();
    const bool enabled = ready && (moveTrack ? clip && !clip->media->sourceDetailsPending && !engine.project.clipTransitionLocked(clip->id) : engine.project.audibleEnd() > 0);
    menu.addSeparator();
    for (int direction : {-1, 1})
    {
        juce::PopupMenu choices;
        for (const auto& command : commands) if (command.move == moveTrack && command.direction == direction)
            choices.addItem(command.id, juce::String(command.bars) + " bars", enabled);
        menu.addSubMenu(juce::String(moveTrack ? "Move linked track " : "Jump ") + (direction < 0 ? "backward" : "forward"), choices, enabled);
    }
}
bool Workstation::performBarMenuAction(int id)
{
    const auto found = std::find_if(commands.begin(), commands.end(), [&](const auto& command) { return command.id == id; });
    if (found == commands.end()) return false;
    if (found->move) moveTrackBars(found->bars, found->direction); else jumpBars(found->bars, found->direction);
    return true;
}
void Workstation::jumpBars(int bars, int direction)
{
    if (!validStep(bars, direction) || busy) return;
    if (pendingTempo > 0 || tempoPreparationPending || engine.project.hasPendingPlacement())
    { setStatus("Wait for preparation before jumping by mix bars."); return; }
    const juce::ScopedLock guard(engine.lock);
    const auto clock = engine.project.followTrackTempo ? engine.project.tempo : TempoMap {engine.project.bpm, {}};
    const double target = clock.timeAtBeat(clock.beatsAt(engine.position) + direction * bars * 4.);
    if (!std::isfinite(target)) { setStatus("This bar position is unavailable."); return; }
    engine.seek(juce::jlimit(0., engine.project.audibleEnd(), target));
    viewport.setViewPosition(std::max(0, (int) (engine.position * timeline.pixelsPerSecond) - viewport.getViewWidth() / 2), viewport.getViewPositionY());
    setStatus("Jumped " + juce::String(bars) + " mix bars " + (direction < 0 ? "backward." : "forward.")); refresh();
}
void Workstation::moveTrackBars(int bars, int direction)
{
    if (!validStep(bars, direction) || busy || !currentClip()) return;
    if (pendingTempo > 0 || tempoPreparationPending || engine.project.hasPendingPlacement() || currentClip()->media->sourceDetailsPending)
    { setStatus("Wait for preparation before moving tracks by bars."); return; }
    engine.flushMidiControls(); engine.finishAutomation(); const juce::ScopedLock guard(engine.lock);
    const auto id = selectedClip; const auto* clip = engine.project.clipFor(id); if (!clip) return;
    const auto clock = engine.project.followTrackTempo ? engine.project.tempo : TempoMap {engine.project.bpm, {}};
    const double target = clock.timeAtBeat(clock.beatsAt(clip->start) + direction * bars * 4.);
    if (!std::isfinite(target) || target < 0) { setStatus("That whole bar step would put the track before the mix."); return; }
    auto candidate = engine.project; const auto result = candidate.moveClip(id, target, false, 0, false);
    if (result.failed()) { setStatus(result.getErrorMessage()); return; }
    if (std::abs(candidate.clipFor(id)->start - clip->start) < 1e-9) return;
    remember(); engine.project = std::move(candidate);
    setStatus("Linked track moved " + juce::String(bars) + " mix bars " + (direction < 0 ? "backward." : "forward.")); refresh();
}
}
