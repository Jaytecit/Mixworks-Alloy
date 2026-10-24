#include "Workstation.h"
#include "Hotcues.h"
#include <cmath>

namespace mixworks
{
std::vector<Timeline::CueFlag> Timeline::hotcueFlags(bool all) const
{
    std::vector<CueFlag> result; const auto view = owner.viewport.getViewArea().toFloat();
    const double near = owner.engine.position.load();
    for (const auto& clip : owner.engine.project.clips)
    {
        if (!all && !visibleHotcueClips.count(clip.id)) continue;
        const auto rect = clipBounds(clip);
        if (!clip.hotcues || (!rect.intersects(view) && !(all && (owner.selectedClip == clip.id || (selectedHotcue && selectedHotcue->clipId == clip.id))))) continue;
        std::vector<juce::Rectangle<float>> occupied;
        for (int slot = 0; slot < 9; ++slot)
        {
            const auto* cue = hotcueFor(clip, slot); if (!cue) continue;
            const auto time = hotcueTimelineTime(clip, slot, near);
            CueFlag flag{clip.id, cue->id, slot, cue->sourceSeconds, time.value_or(clip.start), time.has_value(), {time ? (float) (*time * pixelsPerSecond) - 4 : rect.getX() + 5, rect.getY() + 5, 8, 8}};
            const float x = (float) (flag.time * pixelsPerSecond);
            bool placed = false;
            if (time && laneSize(clip.track) >= 65 && (all || (x >= view.getX() && x <= view.getRight())))
            {
                for (float offset : {0.f, 34.f, -34.f, 68.f, -68.f, 102.f, -102.f, 136.f, -136.f})
                {
                    const auto box = juce::Rectangle<float>{x - 4 + offset, rect.getY() + 36, 30, 12}.getIntersection(rect);
                    if (box.getWidth() < 20 || automationPointAt(box.getCentre()) || std::any_of(occupied.begin(), occupied.end(), [&](const auto& b) { return b.expanded(2).intersects(box); })) continue;
                    flag.bounds = box; occupied.push_back(box); placed = true; break;
                }
            }
            if (all || placed) result.push_back(flag);
        }
    }
    return result;
}
std::optional<Timeline::CueFlag> Timeline::hotcueAt(juce::Point<float> p) const
{
    if (automationPointAt(p)) return {};
    const auto flags = hotcueFlags();
    for (auto i = flags.rbegin(); i != flags.rend(); ++i) if (i->available && i->bounds.contains(p)) return *i;
    return {};
}
void Timeline::paintHotcues(juce::Graphics& g)
{
    for (const auto& flag : hotcueFlags())
    {
        const auto ink = hotcueColour(flag.slot); const float x = (float) (flag.time * pixelsPerSecond);
        const bool selected = owner.selectedClip == flag.clipId && selectedHotcue && selectedHotcue->clipId == flag.clipId && selectedHotcue->id == flag.id;
        g.setColour(ink.withAlpha(.55f)); g.drawLine(x, flag.bounds.getY() - 3, x, flag.bounds.getBottom(), 1);
        g.drawLine(x, flag.bounds.getCentreY(), flag.bounds.getX(), flag.bounds.getCentreY(), 1);
        g.setColour(colour::panel); g.fillRoundedRectangle(flag.bounds, 2); g.setColour(ink); g.drawRoundedRectangle(flag.bounds, 2, selected ? 1.8f : .8f);
        g.setFont(juce::FontOptions(8).withStyle("Bold")); g.drawText("HC" + juce::String(flag.slot + 1), flag.bounds, juce::Justification::centred);
    }
}
void Timeline::selectHotcue(const CueFlag& flag)
{
    const auto* clip = owner.engine.project.clipFor(flag.clipId); const auto* cue = clip ? hotcueFor(*clip, flag.slot) : nullptr;
    if (!cue || cue->id != flag.id || owner.busy || owner.engine.exporting) return;
    visibleHotcueClips.insert(clip->id);
    selectedHotcue = flag; selectedSection.reset(); editingAutomationPoint.reset(); selectedCutAmount.reset(); selectedLabel.clear();
    owner.selectedClip = clip->id; owner.selectedAutomationTarget = -1; owner.correctionFromLibrary = false; owner.selectTrack(clip->track);
    const auto view = owner.viewport.getViewArea(); const int x = Clip::gridPixel(flag.available ? flag.time : clip->start, pixelsPerSecond);
    if (x < view.getX() + 90 || x > view.getRight() - 90)
    { owner.setFollowPlayhead(false); owner.viewport.setViewPosition(std::max(0, x - view.getWidth() / 2), view.getY()); }
    owner.setStatus("HC" + juce::String(flag.slot + 1) + " / source " + juce::String(cue->sourceSeconds, 6)
        + " s / Enter: exact source position; Delete: clear slot; drag: move cue; Controller pad: locate while stopped.");
    refreshCurveTools(); repaint();
}
bool Timeline::beginHotcue(const juce::MouseEvent& event)
{
    const auto flag = hotcueAt(event.position); if (!flag) return false;
    selectHotcue(*flag); grabKeyboardFocus();
    if (event.mods.isPopupMenu()) owner.editHotcue(flag->clipId, flag->slot);
    else hotcueGesture = CueGesture{*flag, event.position.x, false};
    return true;
}
bool Timeline::continueHotcue(const juce::MouseEvent& event)
{
    if (!hotcueGesture) return false;
    if (owner.engine.playing) { owner.setStatus("Stop playback to drag hotcue positions. The cue data and running mix were kept."); return true; }
    if (!event.mouseWasDraggedSinceMouseDown()) return true;
    const auto gesture = *hotcueGesture; const auto* clip = owner.engine.project.clipFor(gesture.flag.clipId);
    const auto* cue = clip ? hotcueFor(*clip, gesture.flag.slot) : nullptr; if (!cue || cue->id != gesture.flag.id) return true;
    const double wanted = gesture.flag.time + (event.position.x - gesture.x) / pixelsPerSecond;
    double time = owner.engine.project.snappedMoveTime(gesture.flag.time, wanted, owner.engine.project.snap && !event.mods.isAltDown());
    double low = clip->start, high = clip->end() - 1.e-6;
    if (clip->loopSpan > 0)
    {
        const double occurrence = std::floor((clip->loopPhase + clip->audioSourceElapsed(gesture.flag.time - clip->start)) / clip->loopSpan + 1.e-9);
        low = std::max(low, clip->start + clip->audioTimelineTime(occurrence * clip->loopSpan - clip->loopPhase));
        high = std::min(high, clip->start + clip->audioTimelineTime((occurrence + 1) * clip->loopSpan - clip->loopPhase - 1.e-6));
    }
    if (high < low) return true;
    time = juce::jlimit(low, high, time);
    const double source = juce::jlimit(0., std::max(0., clip->media->duration() - 1.e-6), clip->sourceTimeAt(time - clip->start));
    if (owner.changeHotcue(clip->id, gesture.flag.slot, source, !gesture.changed)) hotcueGesture->changed = true;
    return true;
}
bool Timeline::hotcueKeyPressed(const juce::KeyPress& key)
{
    if (!selectedHotcue || key.getModifiers().isCtrlDown() || key.getModifiers().isCommandDown() || key.getModifiers().isAltDown()) return false;
    if (owner.selectedClip != selectedHotcue->clipId) { selectedHotcue.reset(); return false; }
    const auto flags = hotcueFlags(true);
    const auto current = std::find_if(flags.begin(), flags.end(), [&](const auto& f) { return f.clipId == selectedHotcue->clipId && f.id == selectedHotcue->id; });
    if (current == flags.end()) { selectedHotcue.reset(); return false; }
    const int code = key.getKeyCode();
    if (code == juce::KeyPress::escapeKey) { selectedHotcue.reset(); hotcueGesture.reset(); refreshAccessibleItems(); repaint(); return true; }
    if (code == juce::KeyPress::returnKey) { owner.editHotcue(current->clipId, current->slot); return true; }
    if (code == juce::KeyPress::deleteKey)
    { owner.changeHotcue(current->clipId, current->slot, {}, true); selectedHotcue.reset(); hotcueGesture.reset(); return true; }
    if (code == juce::KeyPress::leftKey || code == juce::KeyPress::rightKey)
    {
        const int direction = code == juce::KeyPress::leftKey ? -1 : 1;
        for (int slot = current->slot + direction; slot >= 0 && slot < 9; slot += direction)
        { const auto next = std::find_if(flags.begin(), flags.end(), [&](const auto& f) { return f.clipId == current->clipId && f.slot == slot; }); if (next != flags.end()) { selectHotcue(*next); break; } }
        return true;
    }
    if (code == juce::KeyPress::upKey || code == juce::KeyPress::downKey)
    {
        if (!current->available) { owner.setStatus("This cue is outside the segment. Enter edits its source position."); return true; }
        const double delta = (code == juce::KeyPress::upKey ? 1 : -1) * (key.getModifiers().isShiftDown() ? 4 : 1);
        const auto clock = owner.engine.project.gridClock(); const auto* clip = owner.engine.project.clipFor(current->clipId);
        const double at = clock.timeAtBeat(clock.beatsAt(current->time) + delta);
        if (clip && at >= clip->start && at < clip->end()) owner.changeHotcue(clip->id, current->slot, clip->sourceTimeAt(at - clip->start), true);
        return true;
    }
    return false;
}
void Timeline::refreshAccessibleHotcues(const std::function<TimelineAccessItem&(const juce::String&, juce::Rectangle<int>, bool)>& itemFor)
{
    const auto flags = hotcueFlags(true); std::vector<size_t> admitted;
    auto admit = [&](size_t i) { if (visibleHotcueClips.count(flags[i].clipId) && admitted.size() < 256 && std::find(admitted.begin(), admitted.end(), i) == admitted.end()) admitted.push_back(i); };
    for (size_t i = 0; i < flags.size(); ++i) if (selectedHotcue && flags[i].clipId == selectedHotcue->clipId && flags[i].id == selectedHotcue->id) admit(i);
    for (size_t i = 0; i < flags.size() && admitted.size() < 256; ++i) if (flags[i].bounds.intersects(owner.viewport.getViewArea().toFloat())) admit(i);
    for (size_t i = 0; i < flags.size() && admitted.size() < 256; ++i) admit(i);
    for (size_t i : admitted)
    {
        const auto flag = flags[i]; auto& item = itemFor("hotcue:" + juce::String(flag.clipId.length()) + ":" + flag.clipId + ":" + flag.id, flag.bounds.toNearestInt(), true);
        auto find = [this, flag]() -> std::optional<CueFlag>
        {
            const auto* clip = owner.engine.project.clipFor(flag.clipId); const auto* cue = clip ? hotcueFor(*clip, flag.slot) : nullptr;
            if (!cue || cue->id != flag.id || !visibleHotcueClips.count(flag.clipId)) return {};
            auto fresh = flag; fresh.source = cue->sourceSeconds;
            const auto time = hotcueTimelineTime(*clip, flag.slot, owner.engine.position.load());
            fresh.available = time.has_value(); fresh.time = time.value_or(clip->start); return fresh;
        };
        item.setHelpText("Source-owned hotcue slot. Focus selects/reveals without seeking; Enter edits source seconds. Delete clears only this slot. Controller pad locates while stopped. Live cue launch is not available yet.");
        item.read = [this, find]
        { const auto f = find(); if (!f) return juce::String("Removed hotcue"); const auto* clip = owner.engine.project.clipFor(f->clipId); const auto* cue = hotcueFor(*clip, f->slot); return "HC" + juce::String(f->slot + 1) + " / " + clip->media->libraryTitle() + (cue->manual ? " / manual" : " / initial default") + (f->available ? " / project " + timeText(f->time - owner.engine.project.tempo.origin) : " / outside segment") + (owner.engine.project.clipTransitionLocked(f->clipId) ? " / locked" : ""); };
        item.value = [find] { const auto f = find(); return f ? f->source : 0.; };
        item.valueText = [value = item.value] { return juce::String(value(), 6) + " source seconds"; };
        item.selected = [this, find] { const auto f = find(); return f && owner.selectedClip == f->clipId && selectedHotcue && f->clipId == selectedHotcue->clipId && f->id == selectedHotcue->id; };
        item.choose = [this, find] { const auto f = find(); if (f) selectHotcue(*f); };
        item.open = [this, find] { const auto f = find(); if (f) owner.editHotcue(f->clipId, f->slot); };
    }
}
}
