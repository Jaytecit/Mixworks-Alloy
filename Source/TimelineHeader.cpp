#include "Workstation.h"
#include <cmath>

namespace mixworks
{
TimelineHeader::TimelineHeader(Workstation& w, bool isOverview) : owner(w), overview(isOverview)
{
    setOpaque(true);
    setWantsKeyboardFocus(true);
    if (!overview) setFocusContainerType(FocusContainerType::focusContainer);
    setTitle(overview ? "Mix navigator" : "Timeline ruler and labels");
    setTooltip(overview ? "Drag an edge to zoom or the middle to move the view. Double-click to fit the mix."
                        : "Click to move the playhead; double-click to play. Guide toggles an independent alignment line. Shift-click places it; drag its cyan handle to align. Snap applies; Alt gives free placement.");
    if (!overview)
    {
        addAndMakeVisible(guideButton); guideButton.setClickingTogglesState(true);
        addChildComponent(selectionTools); addChildComponent(pairTools); addAndMakeVisible(rangePolicy);
        selectionTools.onClick = [this]
        {
            if (const auto* clip = owner.currentClip())
                owner.showTimelineMenu(owner.engine.position.load(), clip->track, selectionTools.getScreenBounds());
        };
        pairTools.onClick = [this] { owner.openTransitionEditor(owner.selectedTransition); };
        rangePolicy.setTooltip("Range drag mode and fixed musical lengths. Alt bypasses snapping.");
        rangePolicy.onClick = [this]
        {
            owner.rangePolicyMenu().showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&rangePolicy),
                [safe = juce::Component::SafePointer<TimelineHeader>(this)](int id)
                {
                    if (safe && id) safe->owner.setRangePolicy(id - (int) TimelineAction::rangeDragFree + 1);
                });
        };
        guideButton.setTitle("Show alignment guide");
        guideButton.setTooltip("Show/hide an independent alignment guide. Shift-click the ruler to place it; drag the cyan handle. Uses Snap; hold Alt for free placement. Never seeks or moves audio.");
        guideButton.onClick=[this]
        {
            if (guideButton.getToggleState()) alignmentGuide=owner.engine.project.snappedTime(
                (owner.viewport.getViewPositionX()+owner.viewport.getViewWidth()*.5)/owner.timeline.pixelsPerSecond,owner.engine.project.snap);
            else alignmentGuide.reset();
            repaint(); owner.timeline.repaint();
        };
    }
}
void TimelineHeader::resized()
{
    if (overview) return;
    auto row = getLocalBounds().withY(1).withHeight(20).withTrimmedRight(2);
    guideButton.setBounds(row.removeFromRight(64)); row.removeFromRight(4);
    if (pairTools.isVisible()) { pairTools.setBounds(row.removeFromRight(108)); row.removeFromRight(4); }
    if (selectionTools.isVisible()) { selectionTools.setBounds(row.removeFromRight(102)); row.removeFromRight(4); }
    rangePolicy.setBounds(row.removeFromRight(112));
}
void TimelineHeader::refreshEditTools()
{
    if (overview) return;
    selectionTools.setVisible(owner.editSelection.multiple());
    selectionTools.setTooltip(owner.editingSelectionInfo.getText());
    pairTools.setVisible(owner.engine.project.transitionFor(owner.selectedTransition) != nullptr);
    const int mode = owner.editingRangeSize.getSelectedId();
    const juce::StringArray labels {"", "Range: Free", "Range: 1 beat", "Range: 1 bar", "Range: 8 bars",
        "Range: 16 bars", "Range: 32 bars", "Range: Beats", "Range: Snap"};
    rangePolicy.setButtonText(labels[mode]);
    resized();
}
void TimelineHeader::placeGuide(const juce::MouseEvent& e)
{
    const double at=std::max(0.,(e.position.x+owner.viewport.getViewPositionX())/owner.timeline.pixelsPerSecond);
    alignmentGuide=owner.engine.project.snappedTime(at,owner.engine.project.snap && !e.mods.isAltDown());
    guideButton.setToggleState(true,juce::dontSendNotification);
    repaint(); owner.timeline.repaint();
}
double TimelineHeader::overviewWidth() const { return std::max(1., (double) owner.timeline.getWidth()); }
std::unique_ptr<juce::AccessibilityHandler> TimelineHeader::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler>(*this, overview ? juce::AccessibilityRole::group : juce::AccessibilityRole::list);
}
void TimelineHeader::refreshAccessibleMarkers()
{
    if (overview) return;
    juce::StringArray retained; int order = 1;
    auto find = [this](const juce::String& id) -> const LabelMarker*
    {
        const auto& labels = owner.engine.project.labels;
        const auto found = std::find_if(labels.begin(), labels.end(), [&](const auto& label) { return label.id == id; });
        return found == labels.end() ? nullptr : &*found;
    };
    const auto& labels = owner.engine.project.labels;
    for (size_t index = 0; index < labels.size(); ++index)
    {
        const auto id = labels[index].id, key = "label:" + id; retained.add(key);
        const auto existing = std::find_if(accessibleMarkers.begin(), accessibleMarkers.end(), [&](const auto& item) { return item->getComponentID() == key; });
        TimelineAccessItem* item = existing == accessibleMarkers.end() ? nullptr : existing->get();
        if (!item)
        {
            auto added = std::make_unique<TimelineAccessItem>(); item = added.get(); item->setComponentID(key);
            addAndMakeVisible(*item); accessibleMarkers.push_back(std::move(added));
        }
        const int x = Clip::gridPixel(labels[index].time, owner.timeline.pixelsPerSecond) - owner.viewport.getViewPositionX();
        const int right = index + 1 < labels.size() ? std::min(x + 190, Clip::gridPixel(labels[index + 1].time, owner.timeline.pixelsPerSecond) - owner.viewport.getViewPositionX() - 4) : x + 190;
        item->setBounds(x - 3, Timeline::timeRulerHeight, std::max(12, right - x + 3), Timeline::labelHeight);
        item->setEnabled(!owner.busy && !owner.engine.exporting); item->setExplicitFocusOrder(order++);
        item->setHelpText("Focus selects this label without seeking. Enter edits its name/time through the existing label editor. Delete removes the selected label; Undo restores it. Previous/Next label commands seek explicitly.");
        item->read = [this, id, find]
        {
            const auto* marker = find(id); if (!marker) return juce::String("Removed label marker");
            return "Label marker, " + marker->text + ", project time " + timeText(marker->time - owner.engine.project.tempo.origin);
        };
        item->selected = [this, id] { return owner.timeline.selectedLabel == id; };
        item->choose = [this, id, find]
        {
            if (owner.busy || owner.engine.exporting) return;
            const auto* marker = find(id); if (!marker) return;
            const double time = marker->time;
            owner.timeline.selectedLabel = id; owner.timeline.selectAutomationPoint({}, -1); owner.tempoStrip.selectedTempoPoint = -1;
            owner.correctionFromLibrary = false;
            const int x = Clip::gridPixel(time, owner.timeline.pixelsPerSecond);
            if (x < owner.viewport.getViewPositionX() || x > owner.viewport.getViewPositionX() + owner.viewport.getViewWidth() - 24)
            { owner.setFollowPlayhead(false); owner.viewport.setViewPosition(std::max(0, x - owner.viewport.getViewWidth() / 3), owner.viewport.getViewPositionY()); }
            owner.refresh();
        };
        item->open = [this, id, find]
        {
            if (owner.busy || owner.engine.exporting) return;
            if (const auto* marker = find(id)) owner.editLabelMarker(id, marker->time);
        };
    }
    for (const auto& item : accessibleMarkers)
        if (!retained.contains(item->getComponentID()) && item->hasKeyboardFocus(true)) grabKeyboardFocus();
    accessibleMarkers.erase(std::remove_if(accessibleMarkers.begin(), accessibleMarkers.end(), [&](const auto& item) { return !retained.contains(item->getComponentID()); }), accessibleMarkers.end());
}
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
    const TempoMap grid = p.gridClock();
    double fastest = p.bpm; for (const auto& point : grid.points) fastest = std::max(fastest, point.bpm);
    int stride = p.tempo.origin > 0 ? 4 : 16; while (60 / fastest * stride * scale < 110) stride *= 2;
    const int64_t first = (int64_t) std::floor(grid.beatsAt(scroll / scale) / stride);
    const float zero = (float) (p.tempo.origin * scale - scroll);
    if (p.tempo.origin > 0 && zero > 0)
    {
        g.setColour(colour::raised.withAlpha(.4f)); g.fillRect(0.f, 0.f, std::min((float) getWidth(), zero), overview ? 12.f : (float) Timeline::timeRulerHeight);
    }
    g.setFont(juce::FontOptions(11));
    for (int64_t i = first;; ++i)
    {
        const double beat = (double) i * stride; if (beat < grid.beatsAt(0) - 1e-8) continue;
        const double time = grid.timeAtBeat(beat);
        const int x = Clip::gridPixel(time, scale) - scroll; if (x >= getWidth()) break;
        g.setColour(colour::border); g.drawVerticalLine(x, 0, overview ? 12.f : (float) Timeline::timeRulerHeight);
        g.setColour(colour::muted);
        g.drawText(juce::String((juce::int64) (beat / 4 + 1)) + " | " + timeText(time - p.tempo.origin).dropLastCharacters(4), x + 5, 0, 108, overview ? 13 : Timeline::timeRulerHeight, juce::Justification::centredLeft);
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
        for (const float x : {preview.getX() + 2, preview.getRight() - 4}) g.fillRect(x, preview.getY() + 5, 2.f, std::max(2.f, preview.getHeight() - 10));
        const float x = (float) (owner.engine.position.load() * scale); g.setColour(colour::text); g.drawVerticalLine((int) x, 0, (float) getHeight());
    }
    else
    {
        if (p.labels.empty())
        {
            g.setColour(colour::muted);
            if (p.tempo.origin > 0 && zero > 60) g.drawText("PRE-ROLL", 8, Timeline::timeRulerHeight, std::min(getWidth() - 8, (int) zero - 16), Timeline::labelHeight, juce::Justification::centredLeft);
            const int hint = p.tempo.origin > 0 ? std::max(8, (int) zero + 8) : 8;
            g.drawText("Labels", hint, Timeline::timeRulerHeight, std::max(0, getWidth() - hint), Timeline::labelHeight, juce::Justification::centredLeft);
        }
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
    if (!overview && alignmentGuide)
    {
        const float x=(float)(*alignmentGuide*scale-scroll);
        g.setColour(juce::Colour(0xff67dce5)); g.drawVerticalLine((int)x,0,(float)getHeight());
        juce::Path handle; handle.addTriangle(x-7,2,x+7,2,x,14); g.fillPath(handle);
        g.setColour(colour::panel); g.fillRect((int)x+7,2,38,16);
        g.setColour(juce::Colour(0xff67dce5)); g.setFont(11.f); g.drawText("Align",(int)x+8,3,36,14,juce::Justification::centredLeft);
    }
}
juce::MouseEvent TimelineHeader::timelineEvent(const juce::MouseEvent& e) const
{ return e.withNewPosition(juce::Point<float>{e.position.x + owner.viewport.getViewPositionX(), e.position.y - Timeline::rulerHeight}); }
void TimelineHeader::pan(float x)
{
    owner.setFollowPlayhead(false);
    const auto preview = previewBounds();
    const double fraction = juce::jlimit(0., 1., (double) (x - grabOffset) / std::max(1.f, getWidth() - preview.getWidth()));
    owner.viewport.setViewPosition((int) std::lround(fraction * std::max(0., overviewWidth() - owner.viewport.getViewWidth())), owner.viewport.getViewPositionY());
}
void TimelineHeader::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    if (!overview)
    {
        const double x=alignmentGuide ? *alignmentGuide*owner.timeline.pixelsPerSecond-owner.viewport.getViewPositionX() : -1000;
        if (e.mods.isLeftButtonDown() && e.y<Timeline::timeRulerHeight && (e.mods.isShiftDown() || (alignmentGuide && std::abs(e.position.x-x)<=8)))
        { draggingGuide=true; placeGuide(e); return; }
        if (owner.engine.exporting || (e.y < Timeline::timeRulerHeight && e.getNumberOfClicks() > 1)) return;
        owner.timeline.mouseDown(timelineEvent(e)); return;
    }
    zoomEdge = edgeAt(e.position.x);
    if (zoomEdge != 0)
    {
        zoomStartX = e.position.x; zoomStart = owner.viewport.getViewPositionX() / owner.timeline.pixelsPerSecond;
        zoomEnd = zoomStart + owner.viewport.getViewWidth() / owner.timeline.pixelsPerSecond;
        zoomExtent = overviewWidth() / owner.timeline.pixelsPerSecond; return;
    }
    const auto bounds = previewBounds(); grabOffset = bounds.contains(e.position) ? e.position.x - bounds.getX() : bounds.getWidth() * .5f; pan(e.position.x);
}
void TimelineHeader::mouseMove(const juce::MouseEvent& e)
{ if (overview) setMouseCursor(edgeAt(e.position.x) != 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::DraggingHandCursor); }
int TimelineHeader::edgeAt(float x) const
{
    const auto bounds = previewBounds(); const float reach = std::min(6.f, bounds.getWidth() / 3);
    if (std::abs(x - bounds.getX()) <= reach) return -1;
    if (std::abs(x - bounds.getRight()) <= reach) return 1;
    return 0;
}
void TimelineHeader::resizePreview(double begin, double end)
{
    const double width = owner.viewport.getViewWidth(), minimum = width / WorkspaceLayout::maximumTimelineZoom;
    if (zoomEdge < 0) begin = juce::jlimit(std::max(0., end - width / .1), std::max(0., end - minimum), begin);
    else end = juce::jlimit(begin + minimum, std::max(begin + minimum, std::min(zoomExtent, begin + width / .1)), end);
    if (end <= begin) return;
    owner.setFollowPlayhead(false);
    owner.timeline.pixelsPerSecond = juce::jlimit(.1, WorkspaceLayout::maximumTimelineZoom, width / (end - begin)); owner.timeline.updateSize();
    owner.viewport.setViewPosition((int) std::lround(begin * owner.timeline.pixelsPerSecond), owner.viewport.getViewPositionY());
    repaint(); owner.timelineRuler.repaint();
}
void TimelineHeader::mouseDrag(const juce::MouseEvent& e)
{
    if (!overview) { if (draggingGuide) placeGuide(e); else owner.timeline.mouseDrag(timelineEvent(e)); return; }
    if (zoomEdge == 0) { pan(e.position.x); return; }
    const double delta = (e.position.x - zoomStartX) * zoomExtent / std::max(1, getWidth());
    resizePreview(zoomStart + (zoomEdge < 0 ? delta : 0), zoomEnd + (zoomEdge > 0 ? delta : 0));
}
void TimelineHeader::mouseUp(const juce::MouseEvent& e)
{ zoomEdge = 0; if (draggingGuide) { draggingGuide=false; return; } if (!overview) owner.timeline.mouseUp(timelineEvent(e)); }
void TimelineHeader::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (overview) { owner.menuItemSelected(22, 4); return; }
    if (e.mods.isShiftDown() || (alignmentGuide && std::abs(e.position.x-(*alignmentGuide*owner.timeline.pixelsPerSecond-owner.viewport.getViewPositionX()))<=8)) return;
    if (owner.busy || owner.engine.exporting) return;
    if (e.y >= 0 && e.y < Timeline::timeRulerHeight)
    {
        if (!e.mods.isLeftButtonDown() || e.mods.isPopupMenu()) return;
        owner.engine.flushMidiControls();
        owner.engine.seek(std::max(0., (e.position.x + owner.viewport.getViewPositionX()) / owner.timeline.pixelsPerSecond));
        owner.engine.setPlaying(true); owner.refresh(); return;
    }
    owner.timeline.mouseDoubleClick(timelineEvent(e));
}
void TimelineHeader::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{ owner.timeline.mouseWheelMove(e.withNewPosition(juce::Point<float>{(float) owner.viewport.getViewPositionX() + (overview ? owner.viewport.getViewWidth() * .5f : e.position.x), 0}), wheel); }
bool TimelineHeader::keyPressed(const juce::KeyPress& key)
{
    if (overview && !key.getModifiers().isAltDown() && (key.getModifiers().isShiftDown() || key.getModifiers().isCtrlDown())
        && (key.getKeyCode() == juce::KeyPress::leftKey || key.getKeyCode() == juce::KeyPress::rightKey))
    {
        zoomEdge = key.getModifiers().isCtrlDown() ? 1 : -1;
        zoomExtent = overviewWidth() / owner.timeline.pixelsPerSecond;
        const double begin = owner.viewport.getViewPositionX() / owner.timeline.pixelsPerSecond;
        const double end = begin + owner.viewport.getViewWidth() / owner.timeline.pixelsPerSecond;
        const double delta = (end - begin) * .05 * (key.getKeyCode() == juce::KeyPress::leftKey ? -1 : 1);
        resizePreview(begin + (zoomEdge < 0 ? delta : 0), end + (zoomEdge > 0 ? delta : 0)); zoomEdge = 0; return true;
    }
    if (key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown() || key.getModifiers().isAltDown()) return owner.keyPressed(key);
    const int code = key.getKeyCode(), x = owner.viewport.getViewPositionX(), y = owner.viewport.getViewPositionY();
    if (code == juce::KeyPress::leftKey || code == juce::KeyPress::rightKey)
    { owner.setFollowPlayhead(false); owner.viewport.setViewPosition(x + (code == juce::KeyPress::leftKey ? -1 : 1) * std::max(1, owner.viewport.getViewWidth() / 4), y); return true; }
    if (code == juce::KeyPress::homeKey || code == juce::KeyPress::endKey)
    { owner.setFollowPlayhead(false); owner.viewport.setViewPosition(code == juce::KeyPress::homeKey ? 0 : owner.timeline.getWidth(), y); return true; }
    if (key.getTextCharacter() == '+' || key.getTextCharacter() == '-')
    { owner.timeline.zoomAt(key.getTextCharacter() == '+' ? 2 : .5, x + owner.viewport.getViewWidth() * .5); return true; }
    return owner.keyPressed(key);
}
}
