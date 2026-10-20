#include "Workstation.h"
#include <cmath>

namespace mixworks
{
TempoStrip::TempoStrip(Workstation& workstation) : owner(workstation)
{
    setWantsKeyboardFocus(true); setTitle("Tempo markers");
    setTooltip("Drag a tempo pin horizontally to move it. Click a BPM number to edit. Double-click empty space or press P to add. Right-click for Add, Edit and Remove.");
}
double TempoStrip::timeAt(float x) const { return std::max(0.0, (x + owner.viewport.getViewPositionX()) / owner.timeline.pixelsPerSecond); }
float TempoStrip::markerX(int index) const
{
    const auto& points = (draggingTempo ? tempoDraft : owner.engine.project.tempo).points;
    return (float) (points[(size_t) index].time * owner.timeline.pixelsPerSecond - owner.viewport.getViewPositionX());
}
juce::Rectangle<float> TempoStrip::tempoValueBounds(int index) const
{
    const auto& points = (draggingTempo ? tempoDraft : owner.engine.project.tempo).points;
    if (index < 0 || index >= (int) points.size()) return {};
    const float x = markerX(index); if (x < -4 || x >= getWidth()) return {};
    int group = index, after = index + 1;
    while (group > 0 && points[(size_t) group - 1].time == points[(size_t) index].time) --group;
    while (after < (int) points.size() && points[(size_t) after].time == points[(size_t) index].time) ++after;
    const float labelX = x + (index - group) * 68.0f;
    const float next = after < (int) points.size() ? markerX(after) - 5 : (float) getWidth();
    const float width = index == selectedTempoPoint ? std::min(60.0f, (float) getWidth()) : std::min(60.0f, next - labelX - 8);
    if (width < 42) return {};
    const juce::Rectangle<float> bounds {std::max(0.0f, std::min(labelX + 7, getWidth() - width)), 1, width, (float) stripHeight - 2};
    if (index != selectedTempoPoint && bounds.intersects(tempoValueBounds(selectedTempoPoint))) return {};
    return bounds;
}
int TempoStrip::tempoValueAt(juce::Point<float> position) const
{
    if (tempoValueBounds(selectedTempoPoint).contains(position)) return selectedTempoPoint;
    for (int i = (int) owner.engine.project.tempo.points.size() - 1; i >= 0; --i)
        if (tempoValueBounds(i).contains(position)) return i;
    return -1;
}
int TempoStrip::tempoPointAt(juce::Point<float> position) const
{
    if (position.y < 0 || position.y >= getHeight()) return -1;
    int nearest = -1; float distance = 7;
    for (int i = 0; i < (int) owner.engine.project.tempo.points.size(); ++i)
    { const float dx = std::abs(position.x - markerX(i)); if (dx <= distance) { nearest = i; distance = dx; } }
    return nearest;
}
void TempoStrip::paint(juce::Graphics& g)
{
    g.fillAll(colour::panel); g.setColour(colour::border); g.drawHorizontalLine(0, 0, (float) getWidth());
    const auto& project = owner.engine.project; const auto& tempo = draggingTempo ? tempoDraft : project.tempo;
    g.setFont(juce::FontOptions(11));
    auto value = [&](int index)
    {
        const auto bounds = tempoValueBounds(index); if (bounds.isEmpty()) return;
        if (index == selectedTempoPoint)
        {
            g.setColour(colour::raised); g.fillRect(bounds);
            AlloyLookAndFeel::drawMoltenMetal(g, bounds.withTop(bounds.getBottom() - 1.5f).reduced(2, 0), false, .55f);
        }
        g.setColour(index == selectedTempoPoint ? colour::orange : colour::text);
        g.drawText(juce::String(tempo.points[(size_t) index].bpm, 2), bounds, juce::Justification::centredLeft);
    };
    for (int i = 0; i < (int) tempo.points.size(); ++i) value(i);
    // Small hourglass pins keep values aligned to time without turning the footer into a graph.
    for (int i = 0; i < (int) tempo.points.size(); ++i)
    {
        const float x = markerX(i); if (x < -4 || x > getWidth() + 4) continue;
        g.setColour(i == selectedTempoPoint ? colour::text : colour::orange);
        juce::Path pin; pin.addTriangle(x - 3, tempoY() - 5, x + 3, tempoY() - 5, x, tempoY() - 1);
        pin.addTriangle(x - 3, tempoY() + 5, x + 3, tempoY() + 5, x, tempoY() + 1); g.fillPath(pin);
        g.drawVerticalLine((int) std::round(x), tempoY() - 2, tempoY() + 2);
    }
    if (tempo.points.empty() || std::none_of(tempo.points.begin(), tempo.points.end(), [&](const auto& p) { const auto x = p.time * owner.timeline.pixelsPerSecond - owner.viewport.getViewPositionX(); return x >= -4 && x < 90; }))
    {
        g.setColour(colour::muted); const double bpm = project.followTrackTempo ? tempo.bpmAt(timeAt(0)) : project.bpm;
        g.drawText("BPM " + juce::String(bpm, 2), 7, 0, 80, stripHeight, juce::Justification::centredLeft);
    }
    if (hasKeyboardFocus(true)) { g.setColour(colour::focus); g.drawRect(getLocalBounds(), 1); }
}
void TempoStrip::mouseDown(const juce::MouseEvent& event)
{
    if (owner.busy) return;
    if (owner.engine.project.hasLockedTransitions()) { owner.setStatus("Unlock transitions before changing tempo."); return; }
    grabKeyboardFocus(); draggingTempo = moved = false;
    owner.timeline.selectedLabel.clear();
    const int pin = tempoPointAt(event.position), label = tempoValueAt(event.position);
    selectedTempoPoint = pin >= 0 ? pin : label;
    if (pin < 0 && label >= 0 && !event.mods.isPopupMenu()) { owner.refresh(); owner.editTempoPoint(selectedTempoPoint, 0); return; }
    if (event.mods.isPopupMenu())
    {
        juce::PopupMenu menu; menu.addItem(1, "Add Tempo Marker (P)"); menu.addItem(2, "Edit Tempo Marker...", selectedTempoPoint >= 0);
        menu.addItem(3, "Remove Tempo Marker", selectedTempoPoint > 0);
        const auto& points = owner.engine.project.tempo.points;
        const auto selected = selectedTempoPoint >= 0 ? points[(size_t) selectedTempoPoint] : TempoPoint {};
        const double time = timeAt(event.position.x);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this), [safe = juce::Component::SafePointer<Workstation>(&owner), selected, time](int action)
        {
            if (!safe || safe->busy) return;
            if (action == 1) { safe->editTempoPoint(-1, safe->engine.project.snappedTime(time, safe->engine.project.snap)); return; }
            const auto& current = safe->engine.project.tempo.points;
            const auto point = std::find_if(current.begin(), current.end(), [&](const auto& p) { return p.clipId == selected.clipId && (p.clipId.isNotEmpty() || p.time == selected.time); });
            if (point == current.end()) return;
            const int index = (int) std::distance(current.begin(), point); safe->tempoStrip.selectedTempoPoint = index;
            if (action == 2) safe->editTempoPoint(index, 0); else if (action == 3) safe->tempoStrip.removeSelected();
        }); return;
    }
    if (selectedTempoPoint >= 0) { tempoDraft = owner.engine.project.tempo; tempoDraft.fallback = owner.engine.project.bpm; draggingTempo = true; }
    owner.refresh(); repaint();
}
void TempoStrip::mouseMove(const juce::MouseEvent& event)
{
    const int pin = tempoPointAt(event.position);
    setMouseCursor(pin > 0 ? juce::MouseCursor::LeftRightResizeCursor
        : tempoValueAt(event.position) >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}
void TempoStrip::mouseDrag(const juce::MouseEvent& event)
{
    if (!draggingTempo || selectedTempoPoint < 0 || owner.busy || !event.mouseWasDraggedSinceMouseDown()) return;
    moved = true;
    tempoDraft.movePoint(selectedTempoPoint, owner.engine.project.snappedTime(timeAt(event.position.x), owner.engine.project.snap && !event.mods.isAltDown()), tempoDraft.points[(size_t) selectedTempoPoint].bpm);
    const auto& point = tempoDraft.points[(size_t) selectedTempoPoint];
    owner.setStatus(timeText(point.time) + " / " + juce::String(point.bpm, 2) + " BPM"); repaint();
}
void TempoStrip::mouseUp(const juce::MouseEvent&)
{
    if (!draggingTempo) return; draggingTempo = false;
    if (moved) { auto candidate = owner.engine.project; candidate.tempo = tempoDraft; owner.prepareTempo(std::move(candidate), false); }
    else owner.refresh();
    moved = false; repaint();
}
void TempoStrip::mouseDoubleClick(const juce::MouseEvent& event)
{
    if (owner.busy) return; draggingTempo = moved = false;
    const int point = tempoPointAt(event.position); owner.editTempoPoint(point >= 0 ? point : tempoValueAt(event.position), timeAt(event.position.x));
}
void TempoStrip::removeSelected()
{
    if (selectedTempoPoint <= 0 || selectedTempoPoint >= (int) owner.engine.project.tempo.points.size()) { owner.refresh(); return; }
    auto candidate = owner.engine.project; candidate.tempo.points.erase(candidate.tempo.points.begin() + selectedTempoPoint); selectedTempoPoint = -1;
    owner.prepareTempo(std::move(candidate), false);
}
bool TempoStrip::keyPressed(const juce::KeyPress& key)
{
    if (owner.busy) return false;
    if (key.getKeyCode() == juce::KeyPress::returnKey && selectedTempoPoint >= 0) { owner.editTempoPoint(selectedTempoPoint, 0); return true; }
    if (key.getKeyCode() == juce::KeyPress::deleteKey) { removeSelected(); return true; }
    if (key.getKeyCode() == juce::KeyPress::escapeKey) { selectedTempoPoint = -1; draggingTempo = moved = false; owner.refresh(); return true; }
    return false;
}
}
