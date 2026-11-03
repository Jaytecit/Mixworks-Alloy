#include "WorkspaceLayout.h"
#include "AlloyLookAndFeel.h"
#include <cstring>
#include <limits>
#include <cmath>

namespace mixworks
{
void WorkspaceLayout::update(int width, int height, bool performanceDocked, bool editingDocked)
{
    constexpr int margin = 12, divider = 10, headerWidth = 152;
    availableWidth = std::max(1, width - margin * 2 - divider * 2);
    const int panelMin = std::min(minimumPanelWidth, availableWidth / 4);
    const int centreMin = std::min(minimumCentreWidth, availableWidth - 2 * panelMin);
    const int budget = availableWidth - centreMin;
    int left = libraryCollapsed ? 0 : juce::jlimit(panelMin, budget - (inspectorCollapsed ? 0 : panelMin), preferredLibraryWidth);
    int right = inspectorCollapsed ? 0 : juce::jlimit(panelMin, budget - (libraryCollapsed ? 0 : panelMin), preferredInspectorWidth);
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
    const int controlledHeight = trackCount * minimumLaneHeight + timelineScrollbarThickness + visualLaneHeight;
    constexpr int compactDockHeight = 32 + 32 + 64; // Tabs, panel heading and a usable scrolling body below minimum window size.
    // Editing must reserve its complete workflow before applying the ordinary
    // lane minimum, otherwise smaller windows push the primary actions offscreen.
    const int deckHeight = !performanceDocked ? 32 : editingDocked
        ? std::min(240, std::max(compactDockHeight, bodyBudget - trackCount * 40 - visualLaneHeight - timelineScrollbarThickness))
        : std::min(200, std::max(compactDockHeight, bodyBudget - controlledHeight));
    maximumTimelineHeight = std::max(1, bodyBudget - deckHeight);
    // Editing pages share a compact audio workflow; expand vertically for precise fields when wanted.
    // Other consoles retain their existing lane minimum. Musical clocks are untouched.
    const int editingMinimum = std::max(trackCount * 24 + timelineScrollbarThickness + visualLaneHeight, bodyBudget - 400);
    minimumTimelineHeight = std::min(maximumTimelineHeight, editingDocked ? std::min(controlledHeight, editingMinimum) : controlledHeight);
    const int requestedHeight = preferredTimelineHeight > 0 ? preferredTimelineHeight
        : performanceDocked ? trackCount * laneHeight + timelineScrollbarThickness + visualLaneHeight : maximumTimelineHeight;
    const int timelineHeight = juce::jlimit(minimumTimelineHeight, maximumTimelineHeight, requestedHeight);
    // Overall height changes equalise lanes; lane-edge edits apply within that height.
    if (!timeline.isEmpty() && timelineHeight != timeline.getHeight() + visualLaneHeight) laneScale.fill(1);
    defaultLaneHeight = std::max(1, (timelineHeight - visualLaneHeight - timelineScrollbarThickness) / trackCount);
    headers = {centreX, timelineY, headerWidth, timelineHeight}; timeline = {headers.getRight() + 4, timelineY, std::max(1, timelineWidth - headerWidth - 4), timelineHeight};
    visualHeader = headers.removeFromBottom(std::min(visualLaneHeight, headers.getHeight() / 2));
    visuals = timeline.removeFromBottom(visualHeader.getHeight());
    ruler = {timeline.getX(), timelineY - timeRulerHeight - labelHeight, timeline.getWidth(), timeRulerHeight + labelHeight};
    tempo = {timeline.getX(), visuals.getBottom(), timeline.getWidth(), tempoStripHeight};
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
    if (left)
    {
        libraryCollapsed = requestedWidth <= minimumPanelWidth / 2;
        if (!libraryCollapsed) preferredLibraryWidth = juce::jlimit(minimumPanelWidth, maximum, requestedWidth);
        if (!inspectorCollapsed) preferredInspectorWidth = other;
    }
    else
    {
        inspectorCollapsed = requestedWidth <= minimumPanelWidth / 2;
        if (!inspectorCollapsed) preferredInspectorWidth = juce::jlimit(minimumPanelWidth, maximum, requestedWidth);
        if (!libraryCollapsed) preferredLibraryWidth = other;
    }
}
namespace
{
// Bound settings reads; preserve unknown properties and malformed/newer files.
juce::Result validateLayoutDocument(const juce::var&);
juce::Result readDocument(const juce::File& file, juce::var& doc)
{
    if (file.isSymbolicLink() || file.getSize() > 16384) return juce::Result::fail("Workspace preferences are unsupported; the file was kept.");
    auto input = file.createInputStream(); if (!input) return juce::Result::fail("Cannot read workspace preferences.");
    juce::MemoryBlock bytes; input->readIntoMemoryBlock(bytes, 16385);
    if (input->getStatus().failed() || bytes.getSize() > 16384) return juce::Result::fail("Cannot read workspace preferences within the 16 KiB budget.");
    const auto* data = static_cast<const char*>(bytes.getData());
    if (bytes.getSize() == 0 || std::memchr(data, 0, bytes.getSize()) != nullptr || !juce::CharPointer_UTF8::isValidString(data, (int) bytes.getSize())) return juce::Result::fail("Invalid workspace preferences encoding; the file was kept.");
    auto parsed = juce::JSON::parse(juce::String::fromUTF8(data, (int) bytes.getSize()), doc);
    if (parsed.failed()) return juce::Result::fail("Invalid workspace preferences JSON; file was kept.");
    return validateLayoutDocument(doc);
}
juce::Result validateLayoutDocument(const juce::var& doc)
{
    if (!doc.isObject() || !doc["format"].isString() || doc["format"].toString() != "mixworks-workspace" || !doc["version"].isInt() || (int) doc["version"] != 1
        || !doc["libraryWidth"].isInt() || !doc["inspectorWidth"].isInt() || (int) doc["libraryWidth"] < WorkspaceLayout::minimumPanelWidth || (int) doc["inspectorWidth"] < WorkspaceLayout::minimumPanelWidth
        || (doc.hasProperty("monitorMuted") && !doc["monitorMuted"].isBool())
        || (doc.hasProperty("inspectorCollapsed") && !doc["inspectorCollapsed"].isBool())
        || (doc.hasProperty("libraryCollapsed") && !doc["libraryCollapsed"].isBool())
        || (doc.hasProperty("gridBrightness") && (!(doc["gridBrightness"].isDouble() || doc["gridBrightness"].isInt()) || !std::isfinite((double) doc["gridBrightness"]) || (double) doc["gridBrightness"] < 0 || (double) doc["gridBrightness"] > 1))
        || (doc.hasProperty("toolPage") && (!doc["toolPage"].isInt() || (int) doc["toolPage"] < 0 || (int) doc["toolPage"] > 2))
        || (doc.hasProperty("editingDetached") && !doc["editingDetached"].isBool())
        || (doc.hasProperty("popupHints") && !doc["popupHints"].isBool())
        || (doc.hasProperty("followPlayhead") && !doc["followPlayhead"].isBool())
        || (doc.hasProperty("forgeEffects") && !doc["forgeEffects"].isBool())
        || (doc.hasProperty("forgeMotion") && (!doc["forgeMotion"].isInt() || (int) doc["forgeMotion"] < 0 || (int) doc["forgeMotion"] > 2))
        || (doc.hasProperty("visibleLanes") && (!doc["visibleLanes"].isInt() || (int) doc["visibleLanes"] < 1 || (int) doc["visibleLanes"] > 8))
        || (doc.hasProperty("timelineHeight") && (!doc["timelineHeight"].isInt() || (int) doc["timelineHeight"] < 0 || (int) doc["timelineHeight"] > 16384)))
        return juce::Result::fail("Unidentified workspace preferences were kept unchanged.");
    if (doc.hasProperty("editingWindowBounds"))
    {
        const auto* bounds = doc["editingWindowBounds"].getArray();
        if (!bounds || bounds->size() != 4) return juce::Result::fail("Invalid Editing window bounds; workspace preferences were kept.");
        for (const auto& value : *bounds) if (!value.isInt() || (int) value < -65536 || (int) value > 65536)
            return juce::Result::fail("Invalid Editing window bounds; workspace preferences were kept.");
        if ((int) (*bounds)[2] < 640 || (int) (*bounds)[2] > 7680 || (int) (*bounds)[3] < 360 || (int) (*bounds)[3] > 2160)
            return juce::Result::fail("Invalid Editing window size; workspace preferences were kept.");
    }
    if (doc.hasProperty("laneScales"))
    {
        const auto* values = doc["laneScales"].getArray();
        if (!values || (values->size() != 4 && values->size() != 8)) return juce::Result::fail("Invalid lane sizes; workspace preferences were kept.");
        for (const auto& value : *values)
            if (!(value.isDouble() || value.isInt()) || !std::isfinite((double) value) || (double) value < 0 || (double) value > 2)
                return juce::Result::fail("Invalid lane size; workspace preferences were kept.");
    }
    if (doc.hasProperty("consoleOrder"))
    {
        const auto* order = doc["consoleOrder"].getArray();
        if (!order || order->size() != 3) return juce::Result::fail("Invalid console order; workspace was kept.");
        std::array<bool, 3> seen {};
        for (const auto& page : *order)
        { if (!page.isInt() || (int) page < 0 || (int) page > 2 || seen[(size_t) (int) page]) return juce::Result::fail("Invalid console order; workspace was kept."); seen[(size_t) (int) page] = true; }
    }
    return juce::Result::ok();
}
void decodeLayoutDocument(const juce::var& doc, WorkspaceLayout& layout)
{
    layout.preferredLibraryWidth = (int) doc["libraryWidth"]; layout.preferredInspectorWidth = (int) doc["inspectorWidth"];
    layout.monitorMuted = doc.hasProperty("monitorMuted") && (bool) doc["monitorMuted"];
    layout.inspectorCollapsed = doc.hasProperty("inspectorCollapsed") && (bool) doc["inspectorCollapsed"];
    layout.libraryCollapsed = doc.hasProperty("libraryCollapsed") && (bool) doc["libraryCollapsed"];
    layout.gridBrightness = doc.hasProperty("gridBrightness") ? (double) doc["gridBrightness"] : 0.45;
    layout.toolPage = doc.hasProperty("toolPage") ? (int) doc["toolPage"] : 0;
    layout.editingDetached = doc.hasProperty("editingDetached") && (bool) doc["editingDetached"];
    layout.editingWindowBounds = {};
    if (const auto* bounds = doc["editingWindowBounds"].getArray())
        layout.editingWindowBounds = {(int) (*bounds)[0], (int) (*bounds)[1], (int) (*bounds)[2], (int) (*bounds)[3]};
    layout.popupHints = !doc.hasProperty("popupHints") || (bool) doc["popupHints"];
    if (doc["workMode"].isInt() && (int)doc["workMode"]>=0 && (int)doc["workMode"]<=1) layout.workMode=(int)doc["workMode"];
    if (doc["privateDeviceType"].isString() && doc["privateDeviceName"].isString()) { layout.privateDeviceType=doc["privateDeviceType"].toString(); layout.privateDeviceName=doc["privateDeviceName"].toString(); }
    if (doc["privateFirstChannel"].isInt() && (int)doc["privateFirstChannel"]>=-1 && (int)doc["privateFirstChannel"]<256) layout.privateFirstChannel=(int)doc["privateFirstChannel"];
    if ((doc["privateLevel"].isDouble() || doc["privateLevel"].isInt()) && std::isfinite((double)doc["privateLevel"]) && (double)doc["privateLevel"]>=0 && (double)doc["privateLevel"]<=1) layout.privateLevel=(float)doc["privateLevel"];
    layout.followPlayhead = doc.hasProperty("followPlayhead") && (bool) doc["followPlayhead"];
    layout.forgeEffects = doc.hasProperty("forgeEffects") && (bool) doc["forgeEffects"];
    layout.forgeMotion = doc.hasProperty("forgeMotion") ? (int) doc["forgeMotion"] : 0;
    layout.laneScale.fill(1);
    if (const auto* sizes = doc["laneScales"].getArray()) for (int i = 0; i < trackCount; ++i) layout.laneScale[(size_t) i] = (double) (*sizes)[i];
    layout.preferredTimelineHeight = doc.hasProperty("timelineHeight") ? (int) doc["timelineHeight"] : 0;
    layout.consoleOrder = {0, 1, 2};
    if (const auto* order = doc["consoleOrder"].getArray()) for (int i = 0; i < 3; ++i) layout.consoleOrder[(size_t) i] = (int) (*order)[i];
}
juce::var layoutDocument(const WorkspaceLayout& layout)
{
    auto* root = new juce::DynamicObject(); root->setProperty("format", "mixworks-workspace"); root->setProperty("version", 1); juce::var doc(root);
    root->setProperty("libraryCollapsed", layout.libraryCollapsed);
    root->setProperty("workMode",layout.workMode); root->setProperty("privateDeviceType",layout.privateDeviceType); root->setProperty("privateDeviceName",layout.privateDeviceName);
    root->setProperty("privateFirstChannel",layout.privateFirstChannel); root->setProperty("privateLevel",layout.privateLevel);
    juce::Array<juce::var> sizes; for (const double scale : layout.laneScale) sizes.add(scale); const juce::var laneSizes(sizes);
    juce::var editingBounds;
    if (!layout.editingWindowBounds.isEmpty()) editingBounds = juce::var(juce::Array<juce::var> {layout.editingWindowBounds.getX(), layout.editingWindowBounds.getY(), layout.editingWindowBounds.getWidth(), layout.editingWindowBounds.getHeight()});
    auto* object = doc.getDynamicObject(); object->setProperty("inspectorCollapsed", layout.inspectorCollapsed); object->setProperty("popupHints", layout.popupHints); object->setProperty("gridBrightness", layout.gridBrightness); object->setProperty("followPlayhead", layout.followPlayhead); object->setProperty("forgeMotion", layout.forgeMotion); object->setProperty("forgeEffects", layout.forgeEffects); object->setProperty("libraryWidth", layout.preferredLibraryWidth); object->setProperty("inspectorWidth", layout.preferredInspectorWidth); object->setProperty("visibleLanes", layout.visibleLaneCount); object->setProperty("monitorMuted", layout.monitorMuted); object->setProperty("laneScales", laneSizes); object->setProperty("timelineHeight", layout.preferredTimelineHeight);
    object->setProperty("toolPage", layout.toolPage); object->setProperty("editingDetached", layout.editingDetached);
    if (editingBounds.isVoid()) object->removeProperty("editingWindowBounds"); else object->setProperty("editingWindowBounds", editingBounds);
    juce::Array<juce::var> order; for (int page : layout.consoleOrder) order.add(page); object->setProperty("consoleOrder", order);
    return doc;
}
}
juce::Result readWorkspacePreferences(const juce::File& file, WorkspaceLayout& layout)
{
    if (!file.exists() && !file.isSymbolicLink()) return juce::Result::ok();
    juce::var doc; auto result = readDocument(file, doc); if (result.failed()) return result;
    decodeLayoutDocument(doc, layout);
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
    const auto before = doc.clone();
    const auto known = layoutDocument(layout);
    for (const auto& property : known.getDynamicObject()->getProperties()) doc.getDynamicObject()->setProperty(property.name, property.value);
    if (!known.hasProperty("editingWindowBounds")) doc.getDynamicObject()->removeProperty("editingWindowBounds");
    if (juce::JSONUtils::deepEqual(before, doc)) return juce::Result::ok();
    const auto text = juce::JSON::toString(doc);
    if (text.getNumBytesAsUTF8() > 16384) return juce::Result::fail("Workspace preferences exceed the 16 KiB budget; the file was kept.");
    auto result = file.getParentDirectory().createDirectory(); if (result.failed()) return result;
    if (file.existsAsFile())
    {
        const auto backup = file.getSiblingFile(file.getFileName() + ".previous");
        if (backup.isSymbolicLink()) return juce::Result::fail("Linked workspace backups are unsupported.");
        juce::TemporaryFile previous(backup);
        if (!file.copyFileTo(previous.getFile()) || !previous.overwriteTargetFileWithTemporary()) return juce::Result::fail("Cannot back up workspace preferences.");
    }
    juce::TemporaryFile temporary(file);
    if (!temporary.getFile().replaceWithText(text, false, false, "\n") || !temporary.overwriteTargetFileWithTemporary()) return juce::Result::fail("Cannot save workspace preferences.");
    return juce::Result::ok();
}
namespace
{
juce::var windowBounds(const juce::Rectangle<int>& r)
{ return juce::Array<juce::var> {r.getX(), r.getY(), r.getWidth(), r.getHeight()}; }
juce::Result decodeNamedDocument(const juce::var& doc, NamedWorkspace& destination)
{
    const auto core = validateLayoutDocument(doc); if (core.failed()) return core;
    const auto fail = [] { return juce::Result::fail("Unsupported named workspace layout. The current workspace was kept."); };
    if (!doc["namedLayoutVersion"].isInt() || (int) doc["namedLayoutVersion"] != 1 || !doc["layoutName"].isString()) return fail();
    NamedWorkspace candidate; candidate.name = doc["layoutName"].toString().trim();
    if (candidate.name.isEmpty() || candidate.name.length() > 64) return fail();
    for (auto c : candidate.name) if (c < 32 || c == 127) return fail();
    const auto number = [](const juce::var& v) { return (v.isDouble() || v.isInt()) && std::isfinite((double) v); };
    if (!doc["inspectorPage"].isInt() || (int) doc["inspectorPage"] < 1 || (int) doc["inspectorPage"] > 2
        || !doc["browserPage"].isInt() || (int) doc["browserPage"] < 0 || (int) doc["browserPage"] > 1
        || !number(doc["zoom"]) || (double) doc["zoom"] < .1 || (double) doc["zoom"] > WorkspaceLayout::maximumTimelineZoom
        || !doc["viewX"].isInt() || (int) doc["viewX"] < 0 || (int) doc["viewX"] > std::numeric_limits<int>::max() / 4
        || !doc["viewY"].isInt() || (int) doc["viewY"] < 0 || (int) doc["viewY"] > std::numeric_limits<int>::max() / 4) return fail();
    decodeLayoutDocument(doc, candidate.layout);
    candidate.inspectorPage = (int) doc["inspectorPage"]; candidate.browserPage = (int) doc["browserPage"];
    candidate.zoom = (double) doc["zoom"]; candidate.view = {(int) doc["viewX"], (int) doc["viewY"]};
    const auto* windows = doc["consoles"].getArray(); if (!windows || windows->size() != 3) return fail();
    for (int i = 0; i < 3; ++i)
    {
        const auto& window = (*windows)[i]; if (!window.isObject() || !window["detached"].isBool()) return fail();
        candidate.detached[(size_t) i] = (bool) window["detached"];
        if (!candidate.detached[(size_t) i]) continue;
        const auto* bounds = window["bounds"].getArray(); if (!bounds || bounds->size() != 4) return fail();
        for (const auto& n : *bounds) if (!n.isInt() || (int) n < -65536 || (int) n > 65536) return fail();
        if ((int) (*bounds)[2] < 640 || (int) (*bounds)[2] > 7680 || (int) (*bounds)[3] < 360 || (int) (*bounds)[3] > 2160) return fail();
        candidate.windows[(size_t) i] = {(int) (*bounds)[0], (int) (*bounds)[1], (int) (*bounds)[2], (int) (*bounds)[3]};
    }
    candidate.layout.editingDetached = candidate.detached[2]; candidate.layout.editingWindowBounds = candidate.windows[2];
    destination = std::move(candidate); return juce::Result::ok();
}
}
juce::Result readNamedWorkspace(const juce::File& file, NamedWorkspace& destination)
{
    juce::var doc; const auto result = readDocument(file, doc); if (result.failed()) return result;
    return decodeNamedDocument(doc, destination);
}
juce::Result writeNamedWorkspace(const juce::File& file, const NamedWorkspace& preset)
{
    juce::InterProcessLock lock("mixworks-named-workspace-" + juce::String(file.getFullPathName().toLowerCase().hashCode64()));
    if (!lock.enter(100)) return juce::Result::fail("The named workspace is in use.");
    struct Unlock { juce::InterProcessLock& lock; ~Unlock() { lock.exit(); } } unlock {lock};
    if (file.exists() || file.isSymbolicLink() || file.getParentDirectory().isSymbolicLink()) return juce::Result::fail("Choose a new workspace layout file. Existing files were kept.");
    auto doc = layoutDocument(preset.layout); auto* object = doc.getDynamicObject();
    object->removeProperty("monitorMuted"); object->removeProperty("editingDetached"); object->removeProperty("editingWindowBounds");
    object->setProperty("namedLayoutVersion", 1); object->setProperty("layoutName", preset.name);
    object->setProperty("inspectorPage", preset.inspectorPage); object->setProperty("browserPage", preset.browserPage);
    object->setProperty("zoom", preset.zoom); object->setProperty("viewX", preset.view.x); object->setProperty("viewY", preset.view.y);
    juce::Array<juce::var> consoles;
    for (int i = 0; i < 3; ++i)
    { auto* window = new juce::DynamicObject(); window->setProperty("detached", preset.detached[(size_t) i]); if (preset.detached[(size_t) i]) window->setProperty("bounds", windowBounds(preset.windows[(size_t) i])); consoles.add(juce::var(window)); }
    object->setProperty("consoles", consoles);
    NamedWorkspace validated; auto result = decodeNamedDocument(doc, validated); if (result.failed()) return result;
    const auto text = juce::JSON::toString(doc, false, 17);
    if (text.getNumBytesAsUTF8() > 16384) return juce::Result::fail("Named workspace exceeds the 16 KiB budget.");
    result = file.getParentDirectory().createDirectory(); if (result.failed()) return result;
    juce::TemporaryFile temporary(file);
    if (!temporary.getFile().replaceWithText(text, false, false, "\n") || file.exists() || !temporary.overwriteTargetFileWithTemporary())
        return juce::Result::fail("Cannot save this named workspace. Existing layouts were kept.");
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
{ if (isEnabled()) { if (restore) restore(); else setValue(isLeft ? WorkspaceLayout::defaultLibraryWidth : WorkspaceLayout::defaultInspectorWidth, juce::sendNotificationSync); } }
bool PanelSplitter::keyPressed(const juce::KeyPress& key)
{
    if (!isEnabled()) return false;
    if (key == juce::KeyPress::endKey) { setValue(0, juce::sendNotificationSync); return true; }
    if (key == juce::KeyPress::homeKey || key == juce::KeyPress::returnKey) { if (restore) restore(); else setValue(isLeft ? WorkspaceLayout::defaultLibraryWidth : WorkspaceLayout::defaultInspectorWidth, juce::sendNotificationSync); return true; }
    if (key.getKeyCode() == juce::KeyPress::leftKey || key.getKeyCode() == juce::KeyPress::rightKey)
    { setValue(getValue() + (key.getKeyCode() == juce::KeyPress::rightKey ? 1 : -1) * (isLeft ? 1 : -1) * (key.getModifiers().isShiftDown() ? 1 : 10), juce::sendNotificationSync); return true; }
    return false;
}
}
