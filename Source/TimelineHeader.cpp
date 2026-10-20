#include "Workstation.h"
#include <cmath>

namespace mixworks
{
TimelineHeader::TimelineHeader(Workstation& w, bool isOverview) : owner(w), overview(isOverview)
{
    setWantsKeyboardFocus(true);
    setTitle(overview ? "Mix navigator" : "Timeline ruler and labels");
    setTooltip(overview ? "Drag the preview window to pan the timeline. Click to centre it; double-click fits the whole mix. Arrow keys pan; + and - zoom."
                        : "Click the ruler to seek. Drag a label to move it; double-click the label row to add or edit a label.");
}
double TimelineHeader::overviewWidth() const { return std::max(1., (double) owner.timeline.getWidth()); }
juce::Rectangle<float> TimelineHeader::previewBounds() const
{
    const double total = overviewWidth();
    const float width = (float) std::min((double) getWidth(), std::max(12., getWidth() * owner.viewport.getViewWidth() / total));
    const double travel = std::max(1., total - owner.viewport.getViewWidth());
    return {(float) (owner.viewport.getViewPositionX() / travel * (getWidth() - width)), 1, width, (float) getHeight() - 2};
}
void TimelineHeader::paint(juce::Graphics& g)
{
    g.fillAll(colour::panel);
    const auto& p = owner.engine.project;
    const double scale = overview ? getWidth() / overviewWidth() * owner.timeline.pixelsPerSecond : owner.timeline.pixelsPerSecond;
    const int scroll = overview ? 0 : owner.viewport.getViewPositionX();
    const TempoMap grid = p.followTrackTempo ? p.tempo : TempoMap {p.bpm, {}};
    double fastest = p.bpm; for (const auto& point : grid.points) fastest = std::max(fastest, point.bpm);
    int stride = 16; while (60 / fastest * stride * scale < 110) stride *= 2;
    const int64_t first = std::max<int64_t>(0, (int64_t) std::floor(grid.beatsAt(scroll / scale) / stride));
    g.setFont(juce::FontOptions(11));
    for (int64_t i = first;; ++i)
    {
        const double beat = (double) i * stride, time = grid.timeAtBeat(beat);
        const int x = Clip::gridPixel(time, scale) - scroll; if (x >= getWidth()) break;
        g.setColour(colour::border); g.drawVerticalLine(x, 0, overview ? 12.f : (float) Timeline::timeRulerHeight);
        g.setColour(colour::muted);
        g.drawText(juce::String((juce::int64) (beat / 4 + 1)) + " | " + timeText(time).dropLastCharacters(4), x + 5, 0, 108, overview ? 13 : Timeline::timeRulerHeight, juce::Justification::centredLeft);
    }
    if (overview)
    {
        for (const auto& clip : p.clips)
        {
            g.setColour(colour::tracks[(size_t) clip.track].withAlpha(.65f));
            g.fillRect((float) (clip.start * scale), 14.f + clip.track * 3, std::max(1.f, (float) (clip.length * scale)), 2.f);
        }
        const auto preview = previewBounds();
        g.setColour(colour::orange.withAlpha(.10f)); g.fillRect(preview);
        g.setColour(hasKeyboardFocus(true) ? colour::focus : colour::orange); g.drawRect(preview, 1.f);
        const float x = (float) (owner.engine.position.load() * scale); g.setColour(colour::text); g.drawVerticalLine((int) x, 0, (float) getHeight());
    }
    else
    {
        if (p.labels.empty()) { g.setColour(colour::muted); g.drawText("LABELS / double-click here or press L", 8, Timeline::timeRulerHeight, getWidth() - 8, Timeline::labelHeight, juce::Justification::centredLeft); }
        for (size_t i = 0; i < p.labels.size(); ++i)
        {
            const auto& marker = p.labels[i]; const float x = (float) (marker.time * scale - scroll);
            const float right = i + 1 < p.labels.size() ? std::min(x + 190, (float) (p.labels[i + 1].time * scale - scroll) - 4) : x + 190;
            g.setColour(marker.id == owner.timeline.selectedLabel ? colour::orange : colour::muted);
            if (x >= 0 && x < getWidth()) g.fillRect(std::max(4.f, x) - 3, (float) Timeline::timeRulerHeight + 4, 6.f, (float) Timeline::labelHeight - 8);
            if (right > 0 && x < getWidth() && right > x + 10)
            { g.setColour(marker.id == owner.timeline.selectedLabel ? colour::orange : colour::text); g.drawText(marker.text, juce::Rectangle<float>(x + 9, (float) Timeline::timeRulerHeight, right - x - 9, (float) Timeline::labelHeight), juce::Justification::centredLeft); }
        }
        const int x = Clip::gridPixel(owner.engine.position.load(), scale) - scroll;
        g.setColour(colour::orange); g.drawVerticalLine(x, 0, (float) getHeight());
        if (owner.forgeMotionAllowed() && owner.engine.playing)
        {
            juce::Path marker; marker.addTriangle(x - 6.f, 2, x + 6.f, 2, (float) x, 13);
            g.setColour(colour::moltenCore); g.fillPath(marker);
        }
    }
    g.setColour(colour::border); g.drawHorizontalLine(getHeight() - 1, 0, (float) getWidth());
}
juce::MouseEvent TimelineHeader::timelineEvent(const juce::MouseEvent& e) const
{ return e.withNewPosition(juce::Point<float>{e.position.x + owner.viewport.getViewPositionX(), e.position.y - Timeline::rulerHeight}); }
void TimelineHeader::pan(float x)
{
    const auto preview = previewBounds();
    const double fraction = juce::jlimit(0., 1., (double) (x - grabOffset) / std::max(1.f, getWidth() - preview.getWidth()));
    owner.viewport.setViewPosition((int) std::lround(fraction * std::max(0., overviewWidth() - owner.viewport.getViewWidth())), owner.viewport.getViewPositionY());
}
void TimelineHeader::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    if (!overview) { owner.timeline.mouseDown(timelineEvent(e)); return; }
    const auto bounds = previewBounds(); grabOffset = bounds.contains(e.position) ? e.position.x - bounds.getX() : bounds.getWidth() * .5f; pan(e.position.x);
}
void TimelineHeader::mouseDrag(const juce::MouseEvent& e) { if (overview) pan(e.position.x); else owner.timeline.mouseDrag(timelineEvent(e)); }
void TimelineHeader::mouseUp(const juce::MouseEvent& e) { if (!overview) owner.timeline.mouseUp(timelineEvent(e)); }
void TimelineHeader::mouseDoubleClick(const juce::MouseEvent& e) { if (overview) owner.menuItemSelected(22, 4); else owner.timeline.mouseDoubleClick(timelineEvent(e)); }
void TimelineHeader::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{ owner.timeline.mouseWheelMove(e.withNewPosition(juce::Point<float>{(float) owner.viewport.getViewPositionX() + (overview ? owner.viewport.getViewWidth() * .5f : e.position.x), 0}), wheel); }
bool TimelineHeader::keyPressed(const juce::KeyPress& key)
{
    if (key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown() || key.getModifiers().isAltDown()) return owner.keyPressed(key);
    const int code = key.getKeyCode(), x = owner.viewport.getViewPositionX(), y = owner.viewport.getViewPositionY();
    if (code == juce::KeyPress::leftKey || code == juce::KeyPress::rightKey)
    { owner.viewport.setViewPosition(x + (code == juce::KeyPress::leftKey ? -1 : 1) * std::max(1, owner.viewport.getViewWidth() / 4), y); return true; }
    if (code == juce::KeyPress::homeKey || code == juce::KeyPress::endKey)
    { owner.viewport.setViewPosition(code == juce::KeyPress::homeKey ? 0 : owner.timeline.getWidth(), y); return true; }
    if (key.getTextCharacter() == '+' || key.getTextCharacter() == '-')
    { owner.timeline.zoomAt(key.getTextCharacter() == '+' ? 2 : .5, x + owner.viewport.getViewWidth() * .5); return true; }
    return owner.keyPressed(key);
}
}
