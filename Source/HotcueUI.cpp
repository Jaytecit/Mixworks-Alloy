#include "Workstation.h"
#include "Hotcues.h"
#include <cmath>
#include <cstdlib>

namespace mixworks
{
bool Workstation::changeHotcue(const juce::String& id, int slot, std::optional<double> source, bool newUndo, bool seed)
{
    if (busy || engine.exporting || pendingTempo > 0 || tempoPreparationPending || engine.rehearsalActive()
        || engine.automationRecordMode != AutomationRecordMode::read || engine.recordingAutomation())
    { setStatus("Finish preparation and choose Read before editing hotcue slots."); return false; }
    const juce::ScopedLock guard(engine.lock); auto candidate = engine.project;
    const auto result = seed ? candidate.seedHotcues(id) : source ? candidate.putHotcue(id, slot, *source) : candidate.removeHotcue(id, slot);
    if (result.failed()) { setStatus(result.getErrorMessage()); return false; }
    auto* current = engine.project.clipFor(id); const auto* changed = candidate.clipFor(id);
    if (!current || !changed || current->hotcues == changed->hotcues) return false;
    if (newUndo) remember(); current = engine.project.clipFor(id); if (!current) return false;
    current->hotcues = changed->hotcues; current->hotcuesPending = changed->hotcuesPending;
    if (seed && timeline.selectedHotcue && timeline.selectedHotcue->clipId == id) timeline.selectedHotcue.reset();
    if (!hotcueFor(*current, slot) && timeline.selectedHotcue && timeline.selectedHotcue->clipId == id && timeline.selectedHotcue->slot == slot)
        timeline.selectedHotcue.reset();
    refresh(); return true;
}
void Workstation::editHotcue(const juce::String& id, int slot)
{
    if (busy || engine.exporting || slot < 0 || slot >= 9) return;
    const auto* clip = engine.project.clipFor(id); if (!clip || !clip->media) return;
    const auto* cue = hotcueFor(*clip, slot);
    const double seed = juce::jlimit(0., std::max(0., clip->media->duration() - 1.e-6), clip->sourceTimeAt(juce::jlimit(0., clip->length, engine.position.load() - clip->start)));
    const auto initial = juce::String(cue ? cue->sourceSeconds : seed, 6);
    const auto before = juce::JSON::toString(projectToJson(engine.project), true, 17);
    const auto occurrence = hotcueTimelineTime(*clip, slot, engine.position);
    auto* dialog = new juce::AlertWindow("Hotcue HC" + juce::String(slot + 1), clip->media->libraryTitle()
        + ""
        + (cue && !occurrence ? "\nOutside this segment." : "")
        + "", juce::MessageBoxIconType::NoIcon, this);
    dialog->addTextEditor("source", initial, "Track position (seconds)");
    dialog->addButton("Apply", 1, juce::KeyPress(juce::KeyPress::returnKey)); dialog->addButton("Clear slot", 2);
    dialog->addButton("Use section defaults", 3); dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    dialog->enterModalState(true, juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<Workstation>(this), dialog, id, slot, before, initial, existed = cue != nullptr](int answer)
    {
        if (!safe || answer == 0) return;
        if (before != juce::JSON::toString(projectToJson(safe->engine.project), true, 17))
        { safe->setStatus("The segment or project changed. Open this hotcue editor again."); return; }
        if (answer == 2) { safe->changeHotcue(id, slot, {}, true); return; }
        if (answer == 3) { safe->changeHotcue(id, slot, {}, true, true); return; }
        const auto text = dialog->getTextEditorContents("source").trim(); if (existed && text == initial) return;
        const auto raw = text.toStdString(); char* end = nullptr; const double value = std::strtod(raw.c_str(), &end);
        if (raw.empty() || end != raw.c_str() + raw.size() || !std::isfinite(value)) { safe->setStatus("Enter valid source seconds. Hotcue kept."); return; }
        safe->changeHotcue(id, slot, value, true);
    }), true);
}
void Workstation::locateHotcue(int slot)
{
    if (busy || engine.exporting || pendingTempo > 0 || tempoPreparationPending || engine.rehearsalActive()) return;
    const juce::ScopedLock guard(engine.lock);
    const auto* clip = currentClip();
    if (!clip || clip->track != engine.selectedTrack) { setStatus("Select the song segment that owns this hotcue."); return; }
    if (engine.playing)
    {
        const auto result = engine.triggerLiveHotcue(clip->id,slot);
        setStatus(result.failed() ? result.getErrorMessage() : engine.liveHotcueText(clip->id)); refresh(); return;
    }
    if (engine.recordingAutomation()) { setStatus("Finish the current automation take before locating a cue."); return; }
    if (engine.isMidiLearnEnabled()) { setStatus("Hotcue MIDI launch mapping is not available yet. Reset actions remain learnable from Cues / resets."); return; }
    if (!hotcueFor(*clip, slot)) { editHotcue(clip->id, slot); return; }
    const auto time = hotcueTimelineTime(*clip, slot, engine.position);
    if (!time) { setStatus("HC" + juce::String(slot + 1) + " is saved outside this segment. Use Cues / resets to edit it."); return; }
    engine.seek(*time); setFollowPlayhead(false);
    viewport.setViewPosition(std::max(0, Clip::gridPixel(*time, timeline.pixelsPerSecond) - viewport.getViewWidth() / 2), viewport.getViewPositionY());
    const auto flags = timeline.hotcueFlags(true);
    for (const auto& flag : flags) if (flag.clipId == clip->id && flag.slot == slot) { timeline.selectHotcue(flag); break; }
    setStatus("HC" + juce::String(slot + 1) + " / " + clip->media->libraryTitle() + " / located on programme timeline; playback stays stopped.");
    refresh();
}
}
