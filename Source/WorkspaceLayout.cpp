#include "WorkspaceLayout.h"
#include "AlloyLookAndFeel.h"
#include <cstring>

namespace mixworks
{
void WorkspaceLayout::update(int width, int height, bool performanceDocked)
{
    constexpr int margin = 12, divider = 10, headerWidth = 152;
    availableWidth = std::max(1, width - margin * 2 - divider * 2);
    const int panelMin = std::min(minimumPanelWidth, availableWidth / 4);
    const int centreMin = std::min(minimumCentreWidth, availableWidth - 2 * panelMin);
    const int budget = availableWidth - centreMin;
    int left = juce::jlimit(panelMin, budget - panelMin, preferredLibraryWidth);
    int right = juce::jlimit(panelMin, budget - panelMin, preferredInspectorWidth);
    if (left + right > budget)
    {
        const int extra = left + right - 2 * panelMin;
        left = panelMin + (budget - 2 * panelMin) * (left - panelMin) / std::max(1, extra);
        right = budget - left;
    }
    const int centreX = margin + left + divider, centreWidth = availableWidth - left - right;
    const int bodyTop = 76, bodyBottom = std::max(bodyTop, height - 30);
    windowControls = {width - margin - 242, 2, 242, 23};
    brand = {margin, 0, 245, 27}; menu = {268, 0, windowControls.getX() - 276, 27}; toolbar = {margin, 34, width - 2 * margin, 30};
    library = {margin, bodyTop, left, bodyBottom - bodyTop}; playlist = library;
    leftDivider = {library.getRight(), bodyTop, divider, library.getHeight()};
    rightDivider = {centreX + centreWidth, bodyTop, divider, library.getHeight()};
    const int timelineWidth = centreWidth - peakPanelAllocation;
    overview = {centreX, bodyTop, timelineWidth, 28};
    const int timelineY = overview.getBottom() + 2 + timeRulerHeight + labelHeight;
    const int bodyBudget = bodyBottom - timelineY - tempoStripHeight - 8;
    const int controlledHeight = trackCount * minimumLaneHeight + timelineScrollbarThickness;
    constexpr int compactDockHeight = 32 + 32 + 64; // Tabs, panel heading and a usable scrolling body below minimum window size.
    const int deckHeight = performanceDocked ? std::min(200, std::max(compactDockHeight, bodyBudget - controlledHeight)) : 32;
    maximumTimelineHeight = std::max(1, bodyBudget - deckHeight);
    minimumTimelineHeight = std::min(maximumTimelineHeight, controlledHeight);
    const int requestedHeight = preferredTimelineHeight > 0 ? preferredTimelineHeight
        : performanceDocked ? trackCount * laneHeight + timelineScrollbarThickness : maximumTimelineHeight;
    const int timelineHeight = juce::jlimit(minimumTimelineHeight, maximumTimelineHeight, requestedHeight);
    // Overall height changes equalise lanes; lane-edge edits apply within that height.
    if (!timeline.isEmpty() && timelineHeight != timeline.getHeight()) laneScale.fill(1);
    defaultLaneHeight = std::max(1, (timelineHeight - timelineScrollbarThickness) / trackCount);
    headers = {centreX, timelineY, headerWidth, timelineHeight}; timeline = {headers.getRight() + 4, timelineY, std::max(1, timelineWidth - headerWidth - 4), timelineHeight};
    ruler = {timeline.getX(), timelineY - timeRulerHeight - labelHeight, timeline.getWidth(), timeRulerHeight + labelHeight};
    tempo = {timeline.getX(), timeline.getBottom(), timeline.getWidth(), tempoStripHeight};
    peakPanel = {centreX + timelineWidth + 8, bodyTop, peakPanelWidth, tempo.getBottom() - bodyTop};
    timelineDivider = {centreX, tempo.getBottom(), centreWidth, 8};
    performance = {centreX, tempo.getBottom() + 8, centreWidth, bodyBottom - tempo.getBottom() - 8};
    master = {rightDivider.getRight(), bodyTop, right, 96};
    detail = {master.getX(), master.getBottom() + 8, right, std::max(1, bodyBottom - master.getBottom() - 8)};
    status = {margin, height - 26, width - margin * 2, 22};
    fitLanes();
}
void WorkspaceLayout::fitLanes()
{
    const int budget = std::max(trackCount, timeline.getHeight() - timelineScrollbarThickness);
    const int minimum = std::min(minimumLaneHeight, budget / trackCount);
    const int extra = budget - trackCount * minimum;
    double total = 0; for (auto weight : laneScale) total += weight;
    int used = 0; double cumulative = 0;
    for (int i = 0; i < trackCount; ++i)
    {
        cumulative += total > 0 ? laneScale[(size_t) i] / total : 1.0 / trackCount;
        const int boundary = i == trackCount - 1 ? extra : (int) std::floor(extra * cumulative);
        fittedLanes[(size_t) i] = minimum + boundary - used; used = boundary;
    }
}
void WorkspaceLayout::resizeTimeline(int height)
{
    preferredTimelineHeight = height == 0 ? 0 : juce::jlimit(minimumTimelineHeight, maximumTimelineHeight, height);
    laneScale.fill(1);
}
void WorkspaceLayout::resizeLane(int track, int height)
{
    if (track < 0 || track >= trackCount) return;
    const int budget = laneTop(trackCount), minimum = std::min(minimumLaneHeight, budget / trackCount);
    const int extra = budget - minimum * trackCount;
    if (extra <= 0) return;
    const int wanted = juce::jlimit(minimum, budget - (trackCount - 1) * minimum, height) - minimum;
    const int others = extra - (laneSize(track) - minimum);
    for (int i = 0; i < trackCount; ++i)
        laneScale[(size_t) i] = i == track ? (double) wanted / extra
            : (double) (extra - wanted) / extra * (others > 0 ? (double) (laneSize(i) - minimum) / others : 1.0 / (trackCount - 1));
    fitLanes();
}
void WorkspaceLayout::resizePanel(bool left, int requestedWidth)
{
    const int other = left ? master.getWidth() : library.getWidth();
    const int maximum = std::max(minimumPanelWidth, availableWidth - other - minimumCentreWidth);
    if (left) { preferredLibraryWidth = juce::jlimit(minimumPanelWidth, maximum, requestedWidth); preferredInspectorWidth = other; }
    else { preferredInspectorWidth = juce::jlimit(minimumPanelWidth, maximum, requestedWidth); preferredLibraryWidth = other; }
}
namespace
{
// Bound settings reads; preserve unknown properties and malformed/newer files.
juce::Result readDocument(const juce::File& file, juce::var& doc)
{
    if (file.isSymbolicLink() || file.getSize() > 16384) return juce::Result::fail("Workspace preferences are unsupported; the file was kept.");
    auto input = file.createInputStream(); if (!input) return juce::Result::fail("Cannot read workspace preferences.");
    juce::MemoryBlock bytes; input->readIntoMemoryBlock(bytes, 16385);
    if (input->getStatus().failed() || bytes.getSize() > 16384) return juce::Result::fail("Cannot read workspace preferences within the 16 KiB budget.");
    const auto* data = static_cast<const char*>(bytes.getData());
    if (bytes.getSize() == 0 || std::memchr(data, 0, bytes.getSize()) != nullptr || !juce::CharPointer_UTF8::isValidString(data, (int) bytes.getSize())) return juce::Result::fail("Invalid workspace preferences encoding; the file was kept.");
    auto parsed = juce::JSON::parse(juce::String::fromUTF8(data, (int) bytes.getSize()), doc);
    if (parsed.failed() || !doc.isObject() || !doc["format"].isString() || doc["format"].toString() != "mixworks-workspace" || !doc["version"].isInt() || (int) doc["version"] != 1
        || !doc["libraryWidth"].isInt() || !doc["inspectorWidth"].isInt() || (int) doc["libraryWidth"] < WorkspaceLayout::minimumPanelWidth || (int) doc["inspectorWidth"] < WorkspaceLayout::minimumPanelWidth
        || (doc.hasProperty("monitorMuted") && !doc["monitorMuted"].isBool())
        || (doc.hasProperty("followPlayhead") && !doc["followPlayhead"].isBool())
        || (doc.hasProperty("forgeEffects") && !doc["forgeEffects"].isBool())
        || (doc.hasProperty("forgeMotion") && (!doc["forgeMotion"].isInt() || (int) doc["forgeMotion"] < 0 || (int) doc["forgeMotion"] > 2))
        || (doc.hasProperty("visibleLanes") && (!doc["visibleLanes"].isInt() || (int) doc["visibleLanes"] < 1 || (int) doc["visibleLanes"] > 8))
        || (doc.hasProperty("timelineHeight") && (!doc["timelineHeight"].isInt() || (int) doc["timelineHeight"] < 0 || (int) doc["timelineHeight"] > 16384)))
        return juce::Result::fail("Unidentified workspace preferences were kept unchanged.");
    if (doc.hasProperty("laneScales"))
    {
        const auto* values = doc["laneScales"].getArray();
        if (!values || (values->size() != 4 && values->size() != 8)) return juce::Result::fail("Invalid lane sizes; workspace preferences were kept.");
        for (const auto& value : *values)
            if (!(value.isDouble() || value.isInt()) || !std::isfinite((double) value) || (double) value < 0 || (double) value > 2)
                return juce::Result::fail("Invalid lane size; workspace preferences were kept.");
    }
    return juce::Result::ok();
}
}
juce::Result readWorkspacePreferences(const juce::File& file, WorkspaceLayout& layout)
{
    if (!file.exists() && !file.isSymbolicLink()) return juce::Result::ok();
    juce::var doc; auto result = readDocument(file, doc); if (result.failed()) return result;
    layout.preferredLibraryWidth = (int) doc["libraryWidth"]; layout.preferredInspectorWidth = (int) doc["inspectorWidth"];
    layout.monitorMuted = doc.hasProperty("monitorMuted") && (bool) doc["monitorMuted"];
    layout.followPlayhead = doc.hasProperty("followPlayhead") && (bool) doc["followPlayhead"];
    layout.forgeEffects = doc.hasProperty("forgeEffects") && (bool) doc["forgeEffects"];
    layout.forgeMotion = doc.hasProperty("forgeMotion") ? (int) doc["forgeMotion"] : 0;
    layout.laneScale.fill(1);
    if (const auto* sizes = doc["laneScales"].getArray()) for (int i = 0; i < trackCount; ++i) layout.laneScale[(size_t) i] = (double) (*sizes)[i];
    layout.preferredTimelineHeight = doc.hasProperty("timelineHeight") ? (int) doc["timelineHeight"] : 0;
    return juce::Result::ok();
}
juce::Result writeWorkspacePreferences(const juce::File& file, const WorkspaceLayout& layout)
{
    juce::InterProcessLock lock("mixworks-workspace-" + juce::String(file.getFullPathName().toLowerCase().hashCode64()));
    if (!lock.enter(100)) return juce::Result::fail("Workspace preferences are in use by another window.");
    struct Unlock { juce::InterProcessLock& lock; ~Unlock() { lock.exit(); } } unlock {lock};
    juce::var doc;
    if (file.exists() || file.isSymbolicLink()) { auto result = readDocument(file, doc); if (result.failed()) return result; }
    else { auto* object = new juce::DynamicObject(); object->setProperty("format", "mixworks-workspace"); object->setProperty("version", 1); doc = juce::var(object); }
    juce::Array<juce::var> sizes; for (const double scale : layout.laneScale) sizes.add(scale); const juce::var laneSizes(sizes);
    if (doc["followPlayhead"] == juce::var(layout.followPlayhead) && doc["forgeMotion"] == juce::var(layout.forgeMotion) && doc["forgeEffects"] == juce::var(layout.forgeEffects) && doc["timelineHeight"] == juce::var(layout.preferredTimelineHeight) && doc["laneScales"] == laneSizes && doc["libraryWidth"] == juce::var(layout.preferredLibraryWidth) && doc["inspectorWidth"] == juce::var(layout.preferredInspectorWidth) && doc["visibleLanes"] == juce::var(layout.visibleLaneCount) && doc["monitorMuted"] == juce::var(layout.monitorMuted)) return juce::Result::ok();
    auto* object = doc.getDynamicObject(); object->setProperty("followPlayhead", layout.followPlayhead); object->setProperty("forgeMotion", layout.forgeMotion); object->setProperty("forgeEffects", layout.forgeEffects); object->setProperty("libraryWidth", layout.preferredLibraryWidth); object->setProperty("inspectorWidth", layout.preferredInspectorWidth); object->setProperty("visibleLanes", layout.visibleLaneCount); object->setProperty("monitorMuted", layout.monitorMuted); object->setProperty("laneScales", laneSizes); object->setProperty("timelineHeight", layout.preferredTimelineHeight);
    const auto text = juce::JSON::toString(doc);
    if (text.getNumBytesAsUTF8() > 16384) return juce::Result::fail("Workspace preferences exceed the 16 KiB budget; the file was kept.");
    auto result = file.getParentDirectory().createDirectory(); if (result.failed()) return result;
    if (file.existsAsFile() && !file.copyFileTo(file.getSiblingFile(file.getFileName() + ".bak").getNonexistentSibling())) return juce::Result::fail("Cannot back up workspace preferences.");
    juce::TemporaryFile temporary(file);
    if (!temporary.getFile().replaceWithText(text, false, false, "\n") || !temporary.overwriteTargetFileWithTemporary()) return juce::Result::fail("Cannot save workspace preferences.");
    return juce::Result::ok();
}
TimelineSplitter::TimelineSplitter()
{
    setName("Resize timeline height"); setTitle(getName());
    setDescription("Drag up or down. Up/Down resize; Shift changes one pixel. Home or double-click fills available height.");
    setTooltip(getDescription()); setWantsKeyboardFocus(true); setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    setRange(6, 16384, 1); setTextBoxStyle(NoTextBox, false, 0, 0);
}
void TimelineSplitter::paint(juce::Graphics& g)
{
    const auto c = hasKeyboardFocus(true) ? colour::focus : isMouseOver() || dragging ? colour::orange : colour::muted;
    g.setColour(colour::border.withAlpha(.6f)); g.drawHorizontalLine(getHeight() / 2, 0, (float) getWidth());
    g.setColour(c); for (int i = -1; i <= 1; ++i) g.fillRoundedRectangle(getWidth() * .5f + i * 7 - 2, getHeight() * .5f - 1.5f, 4, 3, 1);
    if (hasKeyboardFocus(true)) g.drawRect(getLocalBounds().reduced(1));
}
void TimelineSplitter::mouseDown(const juce::MouseEvent& e)
{ if (!isEnabled()) return; grabKeyboardFocus(); dragging = true; startY = e.getScreenY(); startHeight = getValue(); repaint(); }
void TimelineSplitter::mouseDrag(const juce::MouseEvent& e)
{ if (dragging && isVisible() && isEnabled()) setValue(startHeight + e.getScreenY() - startY, juce::sendNotificationSync); }
void TimelineSplitter::mouseUp(const juce::MouseEvent&) { dragging = false; repaint(); }
void TimelineSplitter::mouseDoubleClick(const juce::MouseEvent&) { if (isEnabled() && reset) reset(); }
bool TimelineSplitter::keyPressed(const juce::KeyPress& key)
{
    if (!isEnabled()) return false;
    if (key == juce::KeyPress::homeKey) { if (reset) reset(); return true; }
    if (key == juce::KeyPress::endKey) { setValue(getMinimum(), juce::sendNotificationSync); return true; }
    if (key.getKeyCode() == juce::KeyPress::upKey || key.getKeyCode() == juce::KeyPress::downKey)
    { setValue(getValue() + (key.getKeyCode() == juce::KeyPress::downKey ? 1 : -1) * (key.getModifiers().isShiftDown() ? 1 : 10), juce::sendNotificationSync); return true; }
    return false;
}
PanelSplitter::PanelSplitter(bool left) : isLeft(left)
{
    setName(left ? "Resize library" : "Resize inspector"); setTitle(getName()); setDescription("Drag left or right. Arrow keys resize. Double-click or Home resets width.");
    setTooltip(getDescription()); setWantsKeyboardFocus(true); setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    setRange(WorkspaceLayout::minimumPanelWidth, 3840, 1); setTextBoxStyle(NoTextBox, false, 0, 0);
}
void PanelSplitter::paint(juce::Graphics& g)
{
    const float x = getWidth() * 0.5f;
    g.setColour(colour::border.withAlpha(0.6f)); g.fillRect(x - 0.5f, 0.0f, 1.0f, (float) getHeight());
    g.setColour(hasKeyboardFocus(true) ? colour::focus : (isMouseOver() || dragging ? colour::orange : colour::muted));
    for (int i = -1; i <= 1; ++i) g.fillRoundedRectangle(x - 2.5f, getHeight() * 0.5f + i * 5 - 1, 5, 2, 1);
    if (hasKeyboardFocus(true)) g.drawRect(getLocalBounds().reduced(1), 1);
}
void PanelSplitter::mouseDown(const juce::MouseEvent& e)
{ if (!isEnabled()) return; grabKeyboardFocus(); startX = e.getScreenX(); startValue = getValue(); dragging = true; repaint(); }
void PanelSplitter::mouseDrag(const juce::MouseEvent& e)
{ if (dragging) setValue(startValue + (e.getScreenX() - startX) * (isLeft ? 1 : -1), juce::sendNotificationSync); }
void PanelSplitter::mouseUp(const juce::MouseEvent&) { dragging = false; repaint(); }
void PanelSplitter::mouseDoubleClick(const juce::MouseEvent&)
{ if (isEnabled()) setValue(isLeft ? WorkspaceLayout::defaultLibraryWidth : WorkspaceLayout::defaultInspectorWidth, juce::sendNotificationSync); }
bool PanelSplitter::keyPressed(const juce::KeyPress& key)
{
    if (!isEnabled()) return false;
    if (key == juce::KeyPress::homeKey) { setValue(isLeft ? WorkspaceLayout::defaultLibraryWidth : WorkspaceLayout::defaultInspectorWidth, juce::sendNotificationSync); return true; }
    if (key.getKeyCode() == juce::KeyPress::leftKey || key.getKeyCode() == juce::KeyPress::rightKey)
    { setValue(getValue() + (key.getKeyCode() == juce::KeyPress::rightKey ? 1 : -1) * (isLeft ? 1 : -1) * (key.getModifiers().isShiftDown() ? 1 : 10), juce::sendNotificationSync); return true; }
    return false;
}
}
