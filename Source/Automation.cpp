#include "Engine.h"
#include <cmath>

namespace mixworks
{
juce::AudioProcessorParameter* Engine::parameterFor(const AutomationAddress& address) const
{
    if (address.track < 0 || address.track >= trackCount || address.target != AutomationTarget::plugin) return nullptr;
    const auto* c = project.clipFor(address.clipId);
    if (address.clipId.isNotEmpty() && !c) return nullptr;
    const auto& chain = c ? c->effects->plugins : plugins[(size_t) address.track];
    for (const auto& slot : chain) if (slot->id == address.slotId)
    {
        const auto found = slot->parameters.find(address.parameterId);
        return found == slot->parameters.end() ? nullptr : found->second;
    }
    return nullptr;
}
juce::Optional<AutomationAddress> Engine::pluginAddress(int track, int insert, int parameter) const
{
    if (track < 0 || track >= trackCount || insert < 0 || insert >= (int) pluginChain(track).size()) return {};
    const auto& slot = *pluginChain(track)[(size_t) insert]; const auto& parameters = slot.processor->getParameters();
    if (parameter < 0 || parameter >= parameters.size()) return {};
    for (const auto& [id, pointer] : slot.parameters) if (pointer == parameters[parameter]) return AutomationAddress {track, AutomationTarget::plugin, slot.id, id, controlClip(track) ? selectedClip : juce::String()};
    return {};
}
bool Engine::ensurePluginAutomation(const AutomationAddress& address)
{
    const juce::ScopedLock guard(lock); auto* parameter = parameterFor(address); if (!parameter) return false;
    if (!project.curve(address)) project.settings(address)->pluginAutomation.push_back({address.slotId, address.parameterId, parameter->getName(128), parameter->getValue(), {}});
    return true;
}
void Engine::restoreManualPluginValues()
{
    const juce::ScopedLock guard(lock);
    for (int track = 0; track < trackCount; ++track) for (const auto& lane : project.tracks[(size_t) track].pluginAutomation)
        if (!lane.curve.active()) if (auto* parameter = parameterFor({track, AutomationTarget::plugin, lane.slotId, lane.parameterId})) parameter->setValueNotifyingHost(lane.manualValue);
    for (const auto& c : project.clips) for (const auto& lane : c.controls.pluginAutomation)
        if (!lane.curve.active()) if (auto* parameter = parameterFor({c.track, AutomationTarget::plugin, lane.slotId, lane.parameterId, c.id})) parameter->setValueNotifyingHost(lane.manualValue);
}
void Engine::removePlugin(int track, int insert)
{
    const juce::ScopedLock guard(lock);
    if (track < 0 || track >= trackCount || insert < 0 || insert >= (int) pluginChain(track).size()) return;
    endTransitionRehearsal(); finishAutomation(); const auto id = pluginChain(track)[(size_t) insert]->id;
    auto& settings = controlSettings(track);
    settings.pluginAutomation.erase(std::remove_if(settings.pluginAutomation.begin(), settings.pluginAutomation.end(), [&](const auto& lane) { return lane.slotId == id; }), settings.pluginAutomation.end());
    for (auto& control : settings.pluginControls) { if (control.insert == insert) control = {}; else if (control.insert > insert) --control.insert; }
    pluginChain(track).erase(pluginChain(track).begin() + insert);
}
float Engine::automationValue(const AutomationAddress& address, double time) const
{
    return automationValueIn(project, address, time);
}
float Engine::automationValueIn(const Project& source, const AutomationAddress& address, double time) const
{
    if (&source == &project && time >= 0 && !exporting)
        for (const auto& write : activeWrites) if (write.address == address) return write.value;
    const auto* curve = source.curve(address); const float manual = source.manualValue(address);
    return curve ? curve->valueAt(time < 0 ? time : std::max(0.0, source.automationTime(address, time)), manual) : manual;
}
void Engine::writeUntil(ActiveWrite& write, double time)
{
    auto* curve = project.curve(write.address); if (!curve || time < write.lastTime) return;
    time = std::min(time, 1.0e9);
    const double curveTime = project.automationTime(write.address, time), last = project.automationTime(write.address, write.lastTime);
    auto begin = std::upper_bound(curve->points.begin(), curve->points.end(), last, [](double t, const auto& p) { return t < p.time; });
    auto end = std::upper_bound(begin, curve->points.end(), curveTime, [](double t, const auto& p) { return t < p.time; });
    for (auto point = begin; point != end; ++point) project.automationEdited(write.address, AutomationEdit::removePoint, point->time);
    curve->points.erase(begin, end); curve->setPoint(curveTime, write.value); write.lastTime = time;
}
void Engine::finishWrite(ActiveWrite& write, double time, bool returnToCurve)
{
    time = std::max(time, write.lastTime); writeUntil(write, time);
    if (returnToCurve)
    {
        auto* curve = project.curve(write.address); if (!curve) return;
        const double returnTime = std::min(time + 0.05, 1.0e9);
        auto begin = std::upper_bound(curve->points.begin(), curve->points.end(), project.automationTime(write.address, time), [](double t, const auto& p) { return t < p.time; });
        auto end = std::upper_bound(begin, curve->points.end(), project.automationTime(write.address, returnTime), [](double t, const auto& p) { return t < p.time; });
        for (auto point = begin; point != end; ++point) project.automationEdited(write.address, AutomationEdit::removePoint, point->time);
        curve->points.erase(begin, end);
        curve->setPoint(project.automationTime(write.address, returnTime), write.original.valueAt(project.automationTime(write.address, returnTime), write.manualBefore));
    }
}
void Engine::finishAutomation()
{
    const juce::ScopedLock guard(lock);
    for (auto& write : activeWrites) finishWrite(write, position.load(), true);
    activeWrites.clear(); automationTake = false;
}
void Engine::setAutomationRecordMode(AutomationRecordMode mode)
{
    const juce::ScopedLock guard(lock); finishAutomation(); automationRecordMode = mode;
}
void Engine::armAutomation(int track, bool armed)
{
    const juce::ScopedLock guard(lock);
    if (!armed)
        for (auto it = activeWrites.begin(); it != activeWrites.end();)
        {
            if (it->address.track == track) { finishWrite(*it, position.load(), true); it = activeWrites.erase(it); } else ++it;
        }
    if (track == -1) masterAutomationArmed = armed;
    else if (track >= 0 && track < trackCount) automationArmed[(size_t) track] = armed;
}
void Engine::setPlaying(bool running)
{
    const juce::ScopedLock guard(lock); if (running && !playbackReady()) return; if (!running) finishAutomation();
    if (running && pluginLatencyError().isNotEmpty()) { playing = false; latencyBlocked = true; return; }
    if (running) latencyBlocked = false;
    if (running) gridAudition.stop(); playing = running;
}
bool Engine::playbackReady() const
{
    return sourceAudioReady() && std::all_of(project.clips.begin(), project.clips.end(), [](const auto& c) { return c.pendingPlacement == PendingPlacement::none && (!c.elasticPlayback || c.elasticSource); });
}
bool Engine::sourceAudioReady() const
{
    return std::all_of(project.clips.begin(), project.clips.end(), [](const auto& c) { return c.prepared || c.media->hasPlayback(); });
}
void Engine::advanceAutomation(double time)
{
    const juce::ScopedLock guard(lock);
    if (!playing || exporting) { if (!activeWrites.empty()) finishAutomation(); return; }
    for (auto it = activeWrites.begin(); it != activeWrites.end();)
    {
        if (time < it->lastInput) { finishWrite(*it, it->lastTime, true); it = activeWrites.erase(it); continue; }
        const bool released = automationRecordMode == AutomationRecordMode::touch && time >= it->lastInput + 0.5;
        writeUntil(*it, released ? it->lastInput + 0.5 : time);
        if (released) { finishWrite(*it, it->lastTime, true); it = activeWrites.erase(it); } else ++it;
    }
}
bool Engine::setControlValue(const AutomationAddress& address, float value)
{
    auto target = address;
    if (target.target != AutomationTarget::master && target.clipId.isEmpty() && controlClip(target.track)) target.clipId = selectedClip;
    return setControlValueAt(target, value, position.load(), playing.load());
}
bool Engine::setControlValueAt(const AutomationAddress& input, float value, double time, bool running)
{
    const juce::ScopedLock guard(lock);
    auto address = input;
    if (address.target != AutomationTarget::master && address.clipId.isEmpty() && controlClip(address.track)) address.clipId = selectedClip;
    if (exporting || !std::isfinite(value) || !std::isfinite(time) || project.automationLocked(address)) return false;
    if (address.target != AutomationTarget::master && (address.track < 0 || address.track >= trackCount)) return false;
    auto* parameter = address.target == AutomationTarget::plugin ? parameterFor(address) : nullptr;
    if (address.target == AutomationTarget::plugin && !parameter) return false;
    if (rehearsalActive()) { endTransitionRehearsal(); time = position.load(); running = false; }
    value = automationRange(address.target).clipValue(value);
    const bool armed = address.target == AutomationTarget::master ? masterAutomationArmed : automationArmed[(size_t) address.track];
    const auto* owned = project.clipFor(address.clipId);
    const bool inside = !owned || (time >= owned->start && time < owned->end());
    const bool record = inside && running && playing && armed && automationRecordMode != AutomationRecordMode::read && time >= 0 && time <= 1.0e9;
    if (record && !automationTake)
    {
        // Materialize mapped targets and their real manual values before taking
        // the undo snapshot, including parameters first recorded later in this pass.
        for (int track = 0; track < trackCount; ++track) if (automationArmed[(size_t) track])
            for (const auto& control : controlSettings(track).pluginControls)
            {
                if (control.parameterId.isEmpty() || control.insert < 0 || control.insert >= (int) pluginChain(track).size()) continue;
                AutomationAddress target {track, AutomationTarget::plugin, pluginChain(track)[(size_t) control.insert]->id, control.parameterId};
                if (controlClip(track)) target.clipId = selectedClip;
                if (ensurePluginAutomation(target) && !project.curve(target)->active()) project.setManualValue(target, parameterFor(target)->getValue());
            }
        if (onAutomationWriteBegin) onAutomationWriteBegin(); automationTake = true;
    }
    if (parameter && record && !ensurePluginAutomation(address)) return false;
    auto* curve = project.curve(address);
    if (record && curve)
    {
        project.automationEdited(address);
        auto found = std::find_if(activeWrites.begin(), activeWrites.end(), [&](const auto& write) { return write.address == address; });
        if (found == activeWrites.end())
        {
            ActiveWrite write; write.address = address; write.original = *curve; write.manualBefore = parameter && !curve->active() ? parameter->getValue() : project.manualValue(address);
            write.value = value; write.lastTime = write.lastInput = time;
            if (!curve->active()) { project.automationEdited(address, AutomationEdit::clearCurve); curve->points.clear(); curve->setPoint(0, write.manualBefore); }
            // Use a real audio-frame boundary. Adjacent floating-point numbers can
            // serialize to the same decimal time and destroy strict point ordering.
            if (time > 1.0 / sampleRate.load())
            {
                const double before = time - 1.0 / sampleRate.load();
                curve->setPoint(project.automationTime(address, before), write.original.valueAt(project.automationTime(address, before), write.manualBefore));
            }
            curve->enabled = true; curve->setPoint(project.automationTime(address, time), value); activeWrites.push_back(std::move(write));
        }
        else
        {
            time = std::max(time, found->lastTime); writeUntil(*found, time); found->value = value; found->lastInput = time; curve->setPoint(project.automationTime(address, time), value);
        }
    }
    if (!record && curve) curve->enabled = false;
    project.setManualValue(address, value);
    if (parameter) { parameter->beginChangeGesture(); parameter->setValueNotifyingHost(value); parameter->endChangeGesture(); }
    return true;
}
}
