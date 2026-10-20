#include "Workstation.h"
#include <cstdlib>
#include <cmath>

namespace mixworks
{
juce::PopupMenu Workstation::automationMenu(int track) const
{
    juce::PopupMenu menu;
    const auto address = displayedAddress(track);
    const auto* curve = automationLane() >= 0 ? engine.project.curve(address) : nullptr;
    const bool available = !busy && !engine.exporting;
    const bool editable = available && curve && !engine.project.automationLocked(address);
    auto add = [&](TimelineAction action, const juce::String& label, bool enabled, bool ticked = false)
    { menu.addItem((int) action, label, enabled, ticked); };
    menu.addSectionHeader(automationLane() >= 0 ? displayedAutomationName() : "Select a point to edit");
    add(TimelineAction::curveRead, "Read selected curve", editable && !curve->points.empty(), curve && curve->enabled);
    add(TimelineAction::curvePoint, "Point at playhead...", editable);
    add(TimelineAction::curveClear, "Clear selected curve", editable && !curve->points.empty());
    add(TimelineAction::choosePlugin, "Choose effect control...", available);
    menu.addSeparator();
    juce::PopupMenu recording;
    recording.addItem((int) TimelineAction::recordOff, "Off", available, engine.automationRecordMode == AutomationRecordMode::read);
    recording.addItem((int) TimelineAction::recordTouch, "Touch", available, engine.automationRecordMode == AutomationRecordMode::touch);
    recording.addItem((int) TimelineAction::recordLatch, "Latch", available, engine.automationRecordMode == AutomationRecordMode::write);
    const bool master = automationLane() == (int) AutomationTarget::master;
    recording.addSeparator(); recording.addItem((int) TimelineAction::armAutomation, master ? "Arm master" : "Arm lane " + juce::String(track + 1), available,
        master ? engine.masterAutomationArmed : engine.automationArmed[(size_t) track]);
    menu.addSubMenu("Recording options", recording);
    return menu;
}
std::vector<AutomationAddress> Workstation::automationAddresses(int track) const
{
    std::vector<AutomationAddress> addresses;
    auto add = [&](const TrackSettings& settings, const juce::String& clipId)
    {
        for (int target = 0; target < trackAutomationCount; ++target)
            if (!settings.automation[(size_t) target].points.empty()) addresses.push_back({track, (AutomationTarget) target, {}, {}, clipId});
        for (const auto& lane : settings.pluginAutomation)
            if (!lane.curve.points.empty()) addresses.push_back({track, AutomationTarget::plugin, lane.slotId, lane.parameterId, clipId});
    };
    add(engine.project.tracks[(size_t) track], {});
    for (const auto& clip : engine.project.clips) if (clip.track == track) add(clip.controls, clip.id);
    if (track == 0 && !engine.project.masterAutomation.points.empty()) addresses.push_back({-1, AutomationTarget::master});
    return addresses;
}
void Workstation::editTempoPoint(int point, double time)
{
    if (busy) return;
    if (engine.project.hasLockedTransitions()) { setStatus("Unlock transitions before changing tempo."); return; }
    const auto& map = engine.project.tempo;
    if (point >= (int) map.points.size()) return;
    if (point >= 0) time = map.points[(size_t) point].time;
    const double bpm = point >= 0 ? map.points[(size_t) point].bpm : map.points.empty() ? engine.project.bpm : map.bpmAt(time);
    auto* dialog = new juce::AlertWindow("Tempo marker", "Set the speed here. Drag the marker to move it.", juce::MessageBoxIconType::NoIcon, this);
    dialog->addTextEditor("time", juce::String(time, 6), "Time (seconds)");
    dialog->addTextEditor("bpm", juce::String(bpm, 4), "Speed (BPM, 20-400)");
    dialog->addButton("Apply", 1, juce::KeyPress(juce::KeyPress::returnKey)); dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    dialog->enterModalState(true, juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<Workstation>(this), dialog, point](int answer)
    {
        if (!safe || answer != 1 || safe->busy) return;
        auto number = [](const juce::String& text, double& value) { const auto raw = text.trim().toStdString(); char* end = nullptr; value = std::strtod(raw.c_str(), &end); return !raw.empty() && end == raw.c_str() + raw.size() && std::isfinite(value); };
        double seconds = 0, target = 0;
        if (!number(dialog->getTextEditorContents("time"), seconds) || !number(dialog->getTextEditorContents("bpm"), target) || seconds < 0 || seconds > 1.0e9 || target < 20 || target > 400)
        { safe->fail("Enter a valid time and BPM between 20 and 400. The tempo map was kept."); return; }
        auto candidate = safe->engine.project; auto& tempo = candidate.tempo;
        if (tempo.points.empty()) tempo.setPoint(0, candidate.bpm);
        if (point >= 0)
        {
            if ((point == 0 && seconds != 0) || (point > 0 && (seconds < tempo.points[(size_t) point - 1].time || (seconds == tempo.points[(size_t) point - 1].time && tempo.points[(size_t) point].clipId.isEmpty() && tempo.points[(size_t) point - 1].clipId.isEmpty()))) || (point + 1 < (int) tempo.points.size() && (seconds > tempo.points[(size_t) point + 1].time || (seconds == tempo.points[(size_t) point + 1].time && tempo.points[(size_t) point].clipId.isEmpty() && tempo.points[(size_t) point + 1].clipId.isEmpty()))))
            { safe->fail("Keep the first marker at zero and other markers between their neighbours."); return; }
            tempo.movePoint(point, seconds, target); safe->tempoStrip.selectedTempoPoint = point;
        }
        else safe->tempoStrip.selectedTempoPoint = tempo.setPoint(seconds, target);
        safe->prepareTempo(std::move(candidate), false);
    }), true);
}
AutomationAddress Workstation::displayedAddress(int track) const
{
    const auto target = (AutomationTarget) std::max(0, automationLane());
    if (target == AutomationTarget::plugin) { auto address = pluginLane; if (track != address.track) address.track = -2; return address; }
    return {target == AutomationTarget::master ? -1 : track, target, {}, {}, target != AutomationTarget::master && engine.controlClip(track) ? selectedClip : juce::String()};
}
AutomationCurve* Workstation::displayedCurve(int track)
{
    if (automationLane() < 0 || (automationLane() == (int) AutomationTarget::master && track != 0)) return nullptr;
    return engine.project.curve(displayedAddress(track));
}
juce::String Workstation::automationText(AutomationTarget target, float value) const
{
    const auto range = automationRange(target); value = range.clipValue(value);
    const float level = target == AutomationTarget::lowPass ? (value - range.getStart()) / range.getLength()
        : target == AutomationTarget::pan || target == AutomationTarget::plugin ? value
        : value <= range.getStart() ? 0 : juce::Decibels::decibelsToGain(value);
    return juce::String(level * 100, 0) + "%";
}
juce::String Workstation::displayedAutomationName() const
{
    if (automationLane() != (int) AutomationTarget::plugin) return automationName((AutomationTarget) std::max(0, automationLane()));
    const auto* settings = engine.project.settings(pluginLane);
    if (settings && pluginLane.track >= 0 && pluginLane.track < trackCount)
        for (const auto& lane : settings->pluginAutomation)
            if (lane.slotId == pluginLane.slotId && lane.parameterId == pluginLane.parameterId)
                for (const auto& slot : (engine.project.clipFor(pluginLane.clipId) ? engine.project.clipFor(pluginLane.clipId)->effects->plugins : engine.plugins[(size_t) pluginLane.track])) if (slot->id == lane.slotId) return slot->description.name + " / " + lane.name;
    return "Choose a VST parameter";
}
void Workstation::chooseAutomationPlugin(std::optional<double> markerTime)
{
    if (busy) return; const int track = engine.selectedTrack;
    juce::StringArray choices; std::vector<AutomationAddress> addresses;
    const auto& chain = engine.pluginChain(track);
    for (int insert = 0; insert < (int) chain.size(); ++insert)
        for (const auto& [id, parameter] : chain[(size_t) insert]->parameters)
        {
            choices.add(juce::String(insert + 1) + ". " + chain[(size_t) insert]->description.name + " / " + parameter->getName(128));
            addresses.push_back({track, AutomationTarget::plugin, chain[(size_t) insert]->id, id, engine.controlClip(track) ? selectedClip : juce::String()});
        }
    if (choices.isEmpty()) { setStatus("Add a VST3 with automatable parameters to the selected track first."); return; }
    auto* dialog = new juce::AlertWindow("Effect control", "Choose an effect control to change over time.", juce::MessageBoxIconType::NoIcon, this);
    dialog->addComboBox("parameter", choices, "Parameter"); dialog->addButton("Show curve", 1, juce::KeyPress(juce::KeyPress::returnKey)); dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    dialog->enterModalState(true, juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<Workstation>(this), dialog, addresses, markerTime](int result)
    {
        if (!safe || result != 1) return; const int choice = dialog->getComboBoxComponent("parameter")->getSelectedItemIndex();
        if (choice < 0 || choice >= (int) addresses.size()) return;
        if (markerTime) safe->engine.finishAutomation(); else safe->engine.setAutomationRecordMode(AutomationRecordMode::read);
        safe->remember();
        if (!safe->engine.ensurePluginAutomation(addresses[(size_t) choice])) { safe->setStatus("This plugin parameter is no longer available."); return; }
        const auto previousLane = safe->pluginLane;
        safe->pluginLane = addresses[(size_t) choice];
        if (markerTime) { safe->addTimelineAutomation(AutomationTarget::plugin, *markerTime, addresses[(size_t) choice].track, false); safe->pluginLane = previousLane; return; }
        safe->selectedAutomationTarget = (int) AutomationTarget::plugin;
        safe->refresh(); safe->setStatus("VST curve ready. Add timeline points, or arm this track, choose Touch/Write and move its mapped FLkey knob while playing.");
    }), true);
}
void Workstation::editAutomationPoint(int track, int point)
{
    if (busy || automationLane() < 0) return;
    const auto address = displayedAddress(track); if (engine.project.automationLocked(address)) { setStatus("Unlock the transition before editing automation."); return; } const auto* curve = engine.project.curve(address); if (!curve || point >= (int) curve->points.size()) return;
    engine.setAutomationRecordMode(AutomationRecordMode::read);
    const auto range = automationRange(address.target); const bool percent = address.target == AutomationTarget::pan || address.target == AutomationTarget::plugin;
    const double scale = percent ? 100 : 1;
    const double time = point >= 0 ? engine.project.automationSessionTime(address, curve->points[(size_t) point].time) : engine.position.load();
    const float manual = point >= 0 ? curve->points[(size_t) point].value : curve->interpolatedValue(engine.project.automationTime(address, time), engine.project.manualValue(address));
    const float value = timeline.automationDisplayValue(address, time, manual);
    auto* dialog = new juce::AlertWindow("Automation point", displayedAutomationName(), juce::MessageBoxIconType::NoIcon, this);
    dialog->addTextEditor("time", juce::String(time, 6), "Session seconds (precise entry bypasses beat snap)");
    dialog->addTextEditor("value", juce::String(value * scale, 3), "Value: " + juce::String(range.getStart() * scale) + " to " + juce::String(range.getEnd() * scale) + (address.target == AutomationTarget::lowPass ? " Hz" : percent ? " %" : " dB"));
    dialog->addButton("Apply", 1, juce::KeyPress(juce::KeyPress::returnKey)); dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    dialog->enterModalState(true, juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<Workstation>(this), dialog, address, range, scale, point](int result)
    {
        if (!safe || result != 1 || safe->busy || safe->engine.project.automationLocked(address)) return;
        auto number = [](const juce::String& text, double& value)
        {
            const auto raw = text.trim().toStdString(); char* end = nullptr; value = std::strtod(raw.c_str(), &end);
            return !raw.empty() && end == raw.c_str() + raw.size() && std::isfinite(value);
        };
        double timeValue = 0, value = 0;
        if (!number(dialog->getTextEditorContents("time"), timeValue) || !number(dialog->getTextEditorContents("value"), value) || timeValue < 0 || timeValue > 1.0e9 || value / scale < range.getStart() || value / scale > range.getEnd())
        { safe->fail("Enter a valid session time and a value inside the displayed range. The curve was kept."); return; }
        value = safe->timeline.automationManualValue(address, timeValue, (float) (value / scale));
        timeValue = safe->engine.project.automationTime(address, timeValue);
        if (timeValue < 0) { safe->fail("Place the marker inside its audio track."); return; }
        const juce::ScopedLock guard(safe->engine.lock); auto* envelope = safe->engine.project.curve(address); if (!envelope) return;
        if (point >= 0 && (point >= (int) envelope->points.size() || (point > 0 && timeValue <= envelope->points[(size_t) point - 1].time)
            || (point + 1 < (int) envelope->points.size() && timeValue >= envelope->points[(size_t) point + 1].time)))
        { safe->fail("Keep this point between its neighbouring points. The curve was kept."); return; }
        safe->remember();
        if (point >= 0)
        {
            const auto moved = safe->engine.project.moveAutomationPoint(address, point, safe->engine.project.automationSessionTime(address, timeValue), (float) value);
            if (moved.failed()) { safe->fail(moved.getErrorMessage()); return; }
        }
        else
        {
            safe->engine.project.automationEdited(address);
            if (envelope->points.empty()) { envelope->enabled = true; if (timeValue > 0) envelope->setPoint(0, safe->engine.project.manualValue(address)); }
            envelope->setPoint(timeValue, (float) value);
        }
        const auto* updated = safe->engine.project.curve(address);
        if (updated)
            for (int i = 0; i < (int) updated->points.size(); ++i)
                if (updated->points[(size_t) i].time == timeValue) { safe->timeline.selectAutomationPoint(address, i); break; }
        safe->refresh(); safe->setStatus("Automation point saved at " + timeText(timeValue));
    }), true);
}
}
