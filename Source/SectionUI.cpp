#include "Workstation.h"
#include "EnergySections.h"
#include <cmath>
#include <cstring>
#include <cstdlib>

namespace mixworks
{
std::vector<Timeline::SectionFlag> Timeline::sectionFlags(bool includeOffscreen) const
{
    std::vector<SectionFlag> result;
    if (!owner.showEnergySections) return result;
    const auto view = owner.viewport.getViewArea().toFloat();
    for (const auto& clip : owner.engine.project.clips)
    {
        if (!includeOffscreen && !visibleSectionClips.count(clip.id)) continue;
        const auto rect = clipBounds(clip);
        const bool chosen = selectedSection && selectedSection->first == clip.id;
        if (!clip.media || clip.reversed || clip.loopSpan > 0 || laneSize(clip.track) < 55
            || (!rect.intersects(view) && !(includeOffscreen && (chosen || owner.selectedClip == clip.id)))) continue;
        const auto sections = clipEnergySections(clip);
        std::vector<juce::Rectangle<float>> occupied;
        for (const auto& link : owner.engine.project.transitionLinks)
        {
            const auto header = transitionBounds(link).withHeight(18).getIntersection(view);
            if (!header.isEmpty()) occupied.push_back(header.withWidth(std::min(130.f, header.getWidth())));
        }
        for (size_t index = 0; index < sections.size(); ++index)
        {
            const auto& section = sections[index];
            const double time = clip.start + clip.audioTimelineTime(section.sourceSeconds - clip.offset);
            if (time < clip.start || time >= clip.end()) continue; // Retain trimmed annotations in the document.
            const float x = (float) (time * pixelsPerSecond);
            if (!includeOffscreen && (x < view.getX() || x > view.getRight())) continue;
            juce::int64 bits = 0; std::memcpy(&bits, &section.sourceSeconds, sizeof(bits));
            SectionFlag flag {clip.id, "section:" + clip.id + ":" + (section.id.isEmpty() ? "auto:" + juce::String::toHexString(bits) : section.id),
                              (int) index, section.sourceSeconds, time, section.manual, {x - 4, rect.getY() + 22, 22, 11}, section.label};
            // Small flags share the title's lower edge, leaving text, existing
            // frame counts and automation targets their own hit regions.
            for (float offset : {0.f, 24.f, -24.f, 48.f, -48.f, 72.f, -72.f})
            {
                auto placed = flag.bounds.translated(offset, 0).getIntersection(rect);
                if (placed.getWidth() < 10 || placed.getHeight() < 8 || automationPointAt(placed.getCentre())) continue;
                if (std::none_of(occupied.begin(), occupied.end(), [&](const auto& box) { return box.expanded(2).intersects(placed); })) { flag.bounds = placed; break; }
            }
            occupied.push_back(flag.bounds); result.push_back(flag);
        }
    }
    return result;
}
std::optional<Timeline::SectionFlag> Timeline::sectionFlagAt(juce::Point<float> point) const
{
    if (point.y < 0 || automationPointAt(point)) return {};
    const auto flags = sectionFlags();
    for (auto i = flags.rbegin(); i != flags.rend(); ++i) if (i->bounds.contains(point)) return *i;
    return {};
}
void Timeline::paintSectionFlags(juce::Graphics& g)
{
    for (const auto& flag : sectionFlags())
    {
        const auto* clip = owner.engine.project.clipFor(flag.clipId); if (!clip) continue;
        const bool selected = selectedSection && selectedSection->first == flag.clipId && selectedSection->second == flag.index;
        const auto ink = owner.engine.project.clipTransitionLocked(flag.clipId) ? colour::muted : flag.manual ? colour::orange : juce::Colour(0xff8ac4d4);
        const float x = (float) (flag.time * pixelsPerSecond);
        g.setColour(ink.withAlpha(.6f)); g.drawLine(x, flag.bounds.getY() - 3, x, flag.bounds.getBottom(), 1);
        g.drawLine(x, flag.bounds.getCentreY(), flag.bounds.getX(), flag.bounds.getCentreY(), 1);
        g.setColour(colour::panel); g.fillRoundedRectangle(flag.bounds, 2);
        g.setColour(ink); g.drawRoundedRectangle(flag.bounds, 2, selected ? 1.6f : .8f);
        g.setFont(juce::FontOptions(8).withStyle("Bold")); g.drawText("S" + juce::String(flag.index + 2), flag.bounds, juce::Justification::centred);
    }
}
void Timeline::selectSectionFlag(const SectionFlag& flag)
{
    const auto* clip = owner.engine.project.clipFor(flag.clipId); if (!clip || owner.busy || owner.engine.exporting) return;
    const int track = clip->track;
    visibleSectionClips.insert(clip->id); owner.showEnergySections = true;
    selectedSection = {flag.clipId, flag.index}; editingAutomationPoint.reset(); selectedCutAmount.reset(); selectedLabel.clear(); selectedHotcue.reset(); hotcueGesture.reset();
    owner.selectedClip = flag.clipId; owner.selectedAutomationTarget = -1; owner.correctionFromLibrary = false;
    owner.selectTrack(track);
    const auto view = owner.viewport.getViewArea(); const int x = Clip::gridPixel(flag.time, pixelsPerSecond);
    if (x < view.getX() + 90 || x > view.getRight() - 90)
    { owner.setFollowPlayhead(false); owner.viewport.setViewPosition(std::max(0, x - view.getWidth() / 2), view.getY()); }
    owner.setStatus("Section S" + juce::String(flag.index + 2) + (flag.label.isNotEmpty() ? " / " + flag.label : juce::String()) + (flag.manual ? " / manual" : " / energy-change suggestion")
        + " / source " + juce::String(flag.source, 6) + " s / drag horizontally; Enter: exact time; Left/Right: adjacent flag; Up/Down: beat step.");
    refreshCurveTools(); repaint();
}
bool Timeline::setSectionFlag(const SectionFlag& flag, double source, bool newUndo, std::optional<juce::String> label)
{
    if (owner.busy || owner.engine.exporting || owner.pendingTempo > 0 || owner.tempoPreparationPending
        || owner.engine.rehearsalActive() || owner.engine.automationRecordMode != AutomationRecordMode::read || owner.engine.recordingAutomation())
    { owner.setStatus("Finish preparation and choose Read before editing section flags."); return false; }
    auto candidate = owner.engine.project;
    const auto result = candidate.setSectionMarkerTime(flag.clipId, flag.index, source, label);
    if (result.failed()) { owner.setStatus(result.getErrorMessage()); return false; }
    auto* current = owner.engine.project.clipFor(flag.clipId); const auto* changed = candidate.clipFor(flag.clipId);
    if (!current || !changed || current->sectionMarkers == changed->sectionMarkers) return false;
    const auto media = current->media; const auto oldTemplate = media->sectionTemplate;
    if (!sectionGesture && !owner.persistSourceSectionDefaults(media,changed->sectionMarkers)) return false;
    const juce::ScopedLock guard(owner.engine.lock);
    current = owner.engine.project.clipFor(flag.clipId); if (!current) return false;
    owner.rememberSourceSectionBaseline(media,oldTemplate);
    if (newUndo)
    {
        owner.remember();
    }
    current->sectionMarkers = changed->sectionMarkers;
    owner.engine.project.sourceSectionEdits[media->analysisSource.sha256] = changed->sectionMarkers;
    if (!sectionGesture) owner.setStatus("Section boundary saved with the source track for future use. HC points stay independently editable.");
    if (flag.index == -1)
    {
        owner.showEnergySections = true;
        visibleSectionClips.insert(flag.clipId);
        const auto& markers = *current->sectionMarkers;
        const auto found = std::find_if(markers.begin(), markers.end(), [source](const auto& marker) { return marker.sourceSeconds == source; });
        selectedSection = {flag.clipId, (int) std::distance(markers.begin(), found)};
    }
    owner.refresh(); return true;
}
void Timeline::finishSectionGesture(bool cancel)
{
    if (!sectionGesture) return;
    const auto gesture = std::move(*sectionGesture); sectionGesture.reset(); if (!gesture.changed) return;
    auto* clip = owner.engine.project.clipFor(gesture.flag.clipId); if (!clip) return;
    const auto media = clip->media;
    if (!cancel && media->sectionTemplate == gesture.expectedTemplate && owner.persistSourceSectionDefaults(media,clip->sectionMarkers))
    { owner.setStatus("Section boundary saved with the source track for future use. HC points stay independently editable."); owner.refresh(); return; }
    if (!cancel && media->sectionTemplate != gesture.expectedTemplate) owner.setStatus("Track defaults changed during this drag. The section edit was kept unchanged.");
    {
        const juce::ScopedLock guard(owner.engine.lock); clip = owner.engine.project.clipFor(gesture.flag.clipId); if (!clip) return;
        clip->sectionMarkers = gesture.original;
        if (gesture.hadSourceEdit) owner.engine.project.sourceSectionEdits[media->analysisSource.sha256] = gesture.originalSourceEdit;
        else owner.engine.project.sourceSectionEdits.erase(media->analysisSource.sha256);
        if (owner.undoHistory.size() == gesture.undoSize + 1) { owner.undoHistory.pop_back(); owner.dirty = gesture.wasDirty; }
    }
    if (cancel) owner.setStatus("Section drag cancelled; track defaults kept."); owner.refresh();
}
void Timeline::resetSectionFlags(const juce::String& id)
{
    if (owner.busy || owner.engine.exporting || owner.pendingTempo > 0 || owner.tempoPreparationPending || owner.engine.rehearsalActive()
        || owner.engine.automationRecordMode != AutomationRecordMode::read || owner.engine.recordingAutomation()) return;
    auto candidate = owner.engine.project; const auto result = candidate.resetSectionMarkers(id);
    if (result.failed()) { owner.setStatus(result.getErrorMessage()); return; }
    auto* clip = owner.engine.project.clipFor(id); if (!clip || (!clip->sectionMarkers && !clip->media->sectionTemplate)) return;
    const auto media = clip->media; const auto old = media->sectionTemplate;
    if (!owner.persistSourceSectionDefaults(media,{})) return;
    const juce::ScopedLock guard(owner.engine.lock);
    owner.rememberSourceSectionBaseline(media,old); owner.remember();
    if (auto* current = owner.engine.project.clipFor(id)) current->sectionMarkers.reset();
    owner.engine.project.sourceSectionEdits[media->analysisSource.sha256].reset();
    owner.setStatus("Automatic section suggestions restored for this segment and future uses of the source track."); owner.refresh();
}
bool Timeline::beginSectionFlag(const juce::MouseEvent& event)
{
    const auto flag = sectionFlagAt(event.position); if (!flag) return false;
    finishSectionGesture();
    selectSectionFlag(*flag); grabKeyboardFocus();
    if (event.mods.isPopupMenu()) editSectionFlag(*flag);
    else
    {
        SectionGesture gesture; gesture.flag = *flag; gesture.x = event.position.x; gesture.wasDirty = owner.dirty; gesture.undoSize = owner.undoHistory.size();
        if (const auto* clip = owner.engine.project.clipFor(flag->clipId))
        {
            gesture.original = clip->sectionMarkers; gesture.expectedTemplate = clip->media->sectionTemplate;
            const auto old = owner.engine.project.sourceSectionEdits.find(clip->media->analysisSource.sha256);
            gesture.hadSourceEdit = old != owner.engine.project.sourceSectionEdits.end(); if (gesture.hadSourceEdit) gesture.originalSourceEdit = old->second;
        }
        sectionGesture = std::move(gesture);
    }
    return true;
}
bool Timeline::continueSectionFlag(const juce::MouseEvent& event)
{
    if (!sectionGesture) return false;
    if (!event.mouseWasDraggedSinceMouseDown()) return true;
    const auto gesture = *sectionGesture; const auto* clip = owner.engine.project.clipFor(gesture.flag.clipId); if (!clip) return true;
    const double wanted = gesture.flag.time + (event.position.x - gesture.x) / pixelsPerSecond;
    const double time = owner.engine.project.snappedMoveTime(gesture.flag.time, wanted, owner.engine.project.snap && !event.mods.isAltDown());
    const auto sections = clipEnergySections(*clip); if (gesture.flag.index < 0 || gesture.flag.index >= (int) sections.size()) return true;
    const double low = std::max({.000001, clip->offset + clip->audioSourceElapsed(0), gesture.flag.index > 0 ? sections[(size_t) gesture.flag.index - 1].sourceSeconds + .000001 : .000001});
    const double high = std::min({clip->media->duration() - .000001, clip->offset + clip->audioSourceElapsed(clip->length) - .000001,
        gesture.flag.index + 1 < (int) sections.size() ? sections[(size_t) gesture.flag.index + 1].sourceSeconds - .000001 : clip->media->duration()});
    if (high <= low) return true;
    const double source = juce::jlimit(low, high, clip->sourceTimeAt(time - clip->start));
    if (setSectionFlag(gesture.flag, source, !gesture.changed)) sectionGesture->changed = true;
    return true;
}
void Timeline::addSectionBoundary(const juce::String& clipId, double time)
{
    finishSectionGesture();
    const auto* clip = owner.engine.project.clipFor(clipId); if (!clip) return;
    const double snapped = owner.engine.project.snappedTime(time, true);
    const double source = clip->sourceTimeAt(snapped - clip->start);
    auto candidate = owner.engine.project; const auto valid = candidate.setSectionMarkerTime(clipId, -1, source);
    if (valid.failed()) { owner.setStatus(valid.getErrorMessage()); return; }
    editSectionFlag({clipId, {}, -1, source, snapped, true, {}, {}});
}
void Timeline::editSectionFlag(const SectionFlag& flag)
{
    if (owner.busy || owner.engine.exporting || owner.engine.project.clipTransitionLocked(flag.clipId)) return;
    const auto* clip = owner.engine.project.clipFor(flag.clipId); if (!clip) return;
    const auto before = juce::JSON::toString(projectToJson(owner.engine.project), true, 17);
    const bool adding = flag.index == -1;
    auto* dialog = new juce::AlertWindow(adding ? "Add section boundary" : "Section flag", clip->media->libraryTitle()
        + (adding ? " / clicked grid point" : " / S" + juce::String(flag.index + 2)), juce::MessageBoxIconType::NoIcon, &owner);
    dialog->addTextEditor("label", flag.label, "Section label");
    dialog->getTextEditor("label")->setInputRestrictions(128);
    dialog->getTextEditor("label")->setTooltip("Name the section beginning at this boundary, for example Verse, Chorus or Drop. Saved with this source track.");
    const auto initialTime = juce::String(flag.time - owner.engine.project.tempo.origin, 6);
    dialog->addTextEditor("time", initialTime, "Mix position (seconds)");
    dialog->getTextEditor("time")->setTooltip("Position in mix seconds. The source boundary and label are saved for future placements.");
    dialog->addButton(adding ? "Add" : "Apply", 1, juce::KeyPress(juce::KeyPress::returnKey));
    if (!adding) dialog->addButton("Use suggestions", 2);
    dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    dialog->enterModalState(true, juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<Timeline>(this), dialog, flag, before, initialTime](int answer)
    {
        if (!safe || answer == 0) return;
        auto& owner = safe->owner;
        if (before != juce::JSON::toString(projectToJson(owner.engine.project), true, 17) || owner.busy || owner.engine.exporting
            || owner.pendingTempo > 0 || owner.tempoPreparationPending || owner.engine.rehearsalActive()
            || owner.engine.automationRecordMode != AutomationRecordMode::read || owner.engine.recordingAutomation())
        { owner.setStatus("The project changed or is busy. Select the section flag again."); return; }
        if (answer == 2)
        {
            safe->resetSectionFlags(flag.clipId);
            return;
        }
        const auto label = dialog->getTextEditorContents("label").trim();
        const bool sameTime = dialog->getTextEditorContents("time").trim() == initialTime;
        if (flag.index >= 0 && sameTime && label == flag.label) return;
        const auto text = dialog->getTextEditorContents("time").trim().toStdString(); char* end = nullptr; const double time = std::strtod(text.c_str(), &end) + owner.engine.project.tempo.origin;
        const auto* clip = owner.engine.project.clipFor(flag.clipId);
        if (text.empty() || end != text.c_str() + text.size() || !std::isfinite(time) || !clip) { owner.setStatus("Enter a valid project time. Section flag kept."); return; }
        safe->setSectionFlag(flag, sameTime ? flag.source : clip->sourceTimeAt(time - clip->start), true, label);
    }), true);
}
bool Timeline::sectionKeyPressed(const juce::KeyPress& key)
{
    if (!selectedSection || key.getModifiers().isCtrlDown() || key.getModifiers().isCommandDown() || key.getModifiers().isAltDown()) return false;
    const auto flags = sectionFlags(true);
    const auto current = std::find_if(flags.begin(), flags.end(), [&](const auto& f) { return f.clipId == selectedSection->first && f.index == selectedSection->second; });
    if (current == flags.end()) { selectedSection.reset(); return false; }
    const int code = key.getKeyCode();
    if (code == juce::KeyPress::escapeKey) { finishSectionGesture(true); selectedSection.reset(); refreshAccessibleItems(); repaint(); return true; }
    finishSectionGesture();
    if (code == juce::KeyPress::deleteKey) { owner.setStatus("Section flag kept. Use Enter for exact entry or to return this segment to automatic suggestions."); return true; }
    if (code == juce::KeyPress::returnKey) { editSectionFlag(*current); return true; }
    if (code == juce::KeyPress::leftKey || code == juce::KeyPress::rightKey)
    {
        const int next = current->index + (code == juce::KeyPress::leftKey ? -1 : 1);
        for (const auto& flag : flags) if (flag.clipId == current->clipId && flag.index == next) { selectSectionFlag(flag); break; }
        return true;
    }
    if (code == juce::KeyPress::upKey || code == juce::KeyPress::downKey)
    {
        const auto clock = owner.engine.project.gridClock(); const double delta = (code == juce::KeyPress::upKey ? 1 : -1) * (key.getModifiers().isShiftDown() ? 4 : 1);
        const double time = clock.timeAtBeat(clock.beatsAt(current->time) + delta); const auto* clip = owner.engine.project.clipFor(current->clipId);
        if (clip) setSectionFlag(*current, clip->sourceTimeAt(time - clip->start), true);
        return true;
    }
    return false;
}
void Timeline::refreshAccessibleSections(const std::function<TimelineAccessItem&(const juce::String&, juce::Rectangle<int>, bool)>& itemFor)
{
    const auto flags = sectionFlags(true); std::vector<size_t> admitted;
    auto admit = [&](size_t i) { if (visibleSectionClips.count(flags[i].clipId) && admitted.size() < 256 && std::find(admitted.begin(), admitted.end(), i) == admitted.end()) admitted.push_back(i); };
    for (size_t i = 0; i < flags.size(); ++i) if (selectedSection && flags[i].clipId == selectedSection->first && flags[i].index == selectedSection->second) admit(i);
    for (size_t i = 0; i < flags.size() && admitted.size() < 256; ++i) if (flags[i].bounds.intersects(owner.viewport.getViewArea().toFloat())) admit(i);
    for (size_t i = 0; i < flags.size() && admitted.size() < 256; ++i) admit(i);
    for (size_t i : admitted)
    {
        const auto flag = flags[i]; auto& item = itemFor(flag.key, flag.bounds.toNearestInt(), true);
        // Native queries validate identity against data, without rebuilding
        // every flag's collision layout for each screen-reader value query.
        auto find = [this, flag]() -> std::optional<SectionFlag>
        {
            const auto* clip = owner.engine.project.clipFor(flag.clipId);
            if (!owner.showEnergySections || !visibleSectionClips.count(flag.clipId) || !clip || clip->reversed || clip->loopSpan > 0) return {};
            const auto sections = clipEnergySections(*clip);
            for (size_t index = 0; index < sections.size(); ++index)
            {
                const auto& section = sections[index];
                if (section.id.isEmpty() ? !flag.key.contains(":auto:") || section.sourceSeconds != flag.source : !flag.key.endsWith(":" + section.id)) continue;
                auto current = flag; current.index = (int) index; current.source = section.sourceSeconds; current.manual = section.manual; current.label = section.label;
                current.time = clip->start + clip->audioTimelineTime(section.sourceSeconds - clip->offset);
                return current;
            }
            return {};
        };
        item.setHelpText("Source section boundary. Edits also save reusable track defaults; existing hotcues stay independent. Focus selects/reveals without seeking. Enter edits precisely; Left/Right selects adjacent flags, Up/Down moves by a beat. Analysis and current transitions stay intact. Delete keeps the segment.");
        item.read = [this, find]
        { const auto f = find(); if (!f) return juce::String("Removed section flag"); const auto* clip = owner.engine.project.clipFor(f->clipId); return clip->media->libraryTitle() + " / section S" + juce::String(f->index + 2) + (f->label.isNotEmpty() ? " / " + f->label : juce::String()) + (f->manual ? " / manual" : " / energy-change suggestion") + ", project " + timeText(f->time - owner.engine.project.tempo.origin) + (owner.engine.project.clipTransitionLocked(f->clipId) ? ", locked" : ""); };
        item.value = [find] { const auto f = find(); return f ? f->source : 0.; };
        item.valueText = [value = item.value] { return juce::String(value(), 6) + " source seconds"; };
        item.selected = [this, find] { const auto f = find(); return f && selectedSection && f->clipId == selectedSection->first && f->index == selectedSection->second; };
        item.choose = [this, find] { const auto f = find(); if (f) selectSectionFlag(*f); };
        item.open = [this, find] { const auto f = find(); if (f) editSectionFlag(*f); };
    }
}
}
