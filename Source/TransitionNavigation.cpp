#include "Workstation.h"

namespace mixworks
{
juce::String Workstation::transitionKeySummary(const Project::TransitionLink& link) const
{
    const auto* out = engine.project.clipFor(link.outgoing); const auto* in = engine.project.clipFor(link.incoming);
    if (!out || !in || !out->media || !in->media) return "Key movement: unavailable";
    const auto& a = *out->media; const auto& b = *in->media;
    const auto text = "Out: " + keyText(a) + "\nIn: " + keyText(b);
    if (!a.keyConfirmed() || !b.keyConfirmed()) return text + "\nMovement unknown / review source keys";
    const int first = keyWheelNumber(a.key()), last = keyWheelNumber(b.key());
    int difference = (last - first + 12) % 12; if (difference > 6) difference -= 12;
    const bool sameMode = (a.key() < 12) == (b.key() < 12);
    if (difference == 0) return text + (sameMode ? "\nSame source key" : "\nRelative major / minor");
    return text + "\nWheel " + (difference == 6 ? "6 steps" : (difference > 0 ? "+" : "") + juce::String(difference))
        + (sameMode ? " / same mode" : " / mode changes");
}
bool Workstation::transitionKeyPressed(const juce::KeyPress& key)
{
    const auto modifiers = key.getModifiers();
    if (!modifiers.isAltDown()) return false;
    const int code = key.getKeyCode();
    if (code == 'L' && !modifiers.isCtrlDown() && !modifiers.isShiftDown())
    { changeSelectedTransition({}, {}, true); return true; }
    if (code != juce::KeyPress::leftKey && code != juce::KeyPress::rightKey) return false;
    if (modifiers.isCtrlDown() && modifiers.isShiftDown()) return false;
    const int direction = code == juce::KeyPress::leftKey ? -1 : 1;
    if (!modifiers.isCtrlDown() && !modifiers.isShiftDown()) { navigateTransition(direction); return true; }
    if (pendingTempo > 0 || tempoPreparationPending || engine.project.hasPendingPlacement())
    { setStatus("Wait for preparation before editing a transition frame."); return true; }
    engine.flushMidiControls(); engine.finishAutomation();
    const juce::ScopedLock guard(engine.lock);
    const auto* link = engine.project.transitionFor(selectedTransition);
    if (!link) { setStatus("Select a transition frame first."); return true; }
    const auto range = engine.project.transitionRange(*link);
    const auto clock = engine.project.followTrackTempo ? engine.project.tempo : TempoMap {engine.project.bpm, {}};
    const auto edit = modifiers.isShiftDown() ? Project::TransitionEdit::resizeEnd : Project::TransitionEdit::moveWindow;
    const double reference = modifiers.isShiftDown() ? range.getEnd() : range.getStart();
    const double delta = clock.timeAtBeat(clock.beatsAt(reference) + direction) - reference;
    auto candidate = engine.project; const auto result = candidate.editTransition(selectedTransition, edit, delta, false);
    if (result.failed()) { setStatus(result.getErrorMessage()); refresh(); return true; }
    if (juce::JSON::toString(projectToJson(candidate)) != juce::JSON::toString(projectToJson(engine.project)))
    { remember(); engine.project = std::move(candidate); setStatus(modifiers.isShiftDown() ? "Transition end resized by one mix beat." : "Transition frame moved by one mix beat; its aligned incoming chain followed."); }
    refresh(); return true;
}
}
