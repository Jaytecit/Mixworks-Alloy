#include "Workstation.h"
#include <cstdlib>
#include <cmath>
#include <limits>

namespace mixworks
{
void Workstation::editLabelMarker(juce::String id, double time)
{
    if (busy) return;
    LabelMarker marker; marker.time = time; marker.text = "Label";
    if (id.isNotEmpty())
    {
        const auto& labels = engine.project.labels;
        const auto found = std::find_if(labels.begin(), labels.end(), [&](const auto& item) { return item.id == id; });
        if (found == labels.end()) return; marker = *found;
    }
    auto* dialog = new juce::AlertWindow("Label marker", "Name this part of your mix.", juce::MessageBoxIconType::NoIcon, this);
    dialog->addTextEditor("name", marker.text, "Label (1-256 characters)");
    dialog->getTextEditor("name")->setInputRestrictions(256);
    dialog->addTextEditor("time", juce::String(marker.time, 6), "Session seconds (precise entry bypasses snap)");
    dialog->addButton("Apply", 1, juce::KeyPress(juce::KeyPress::returnKey)); dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    dialog->enterModalState(true, juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<Workstation>(this), dialog, marker, editing = id.isNotEmpty()](int answer) mutable
    {
        if (!safe || safe->busy || answer != 1) return;
        const auto raw = dialog->getTextEditorContents("time").trim().toStdString(); char* end = nullptr;
        marker.time = std::strtod(raw.c_str(), &end); marker.text = dialog->getTextEditorContents("name");
        if (raw.empty() || end != raw.c_str() + raw.size() || !std::isfinite(marker.time)) { safe->fail("Enter a valid numeric session time. The labels were kept."); return; }
        auto candidate = safe->engine.project;
        if (editing && std::none_of(candidate.labels.begin(), candidate.labels.end(), [&](const auto& item) { return item.id == marker.id; })) return;
        const auto result = candidate.putLabelMarker(marker); if (result.failed()) { safe->fail(result.getErrorMessage()); return; }
        safe->engine.setAutomationRecordMode(AutomationRecordMode::read); safe->remember();
        { const juce::ScopedLock guard(safe->engine.lock); safe->engine.project.labels = std::move(candidate.labels); }
        safe->timeline.selectedLabel = marker.id; safe->refresh(); safe->setStatus("Label saved: " + marker.text + " / " + timeText(marker.time));
    }), true);
}
void Workstation::removeLabelMarker()
{
    if (busy) return;
    auto& labels = engine.project.labels;
    const auto found = std::find_if(labels.begin(), labels.end(), [&](const auto& item) { return item.id == timeline.selectedLabel; });
    if (found == labels.end()) return;
    engine.setAutomationRecordMode(AutomationRecordMode::read); remember();
    { const juce::ScopedLock guard(engine.lock); labels.erase(found); }
    timeline.selectedLabel.clear(); refresh(); setStatus("Label removed. Undo restores it.");
}
void Workstation::navigateLabel(bool next)
{
    if (busy) return;
    const auto& labels = engine.project.labels; const double at = engine.position;
    const LabelMarker* target = nullptr;
    if (next) { for (const auto& marker : labels) if (marker.time > at + 0.000001) { target = &marker; break; } }
    else { for (auto it = labels.rbegin(); it != labels.rend(); ++it) if (it->time < at - 0.000001) { target = &*it; break; } }
    if (!target) { setStatus(next ? "No later label." : "No earlier label."); return; }
    timeline.selectedLabel = target->id; tempoStrip.selectedTempoPoint = -1; engine.seek(target->time);
    const double x = target->time * timeline.pixelsPerSecond;
    if (x < viewport.getViewPositionX() || x > viewport.getViewPositionX() + viewport.getWidth() - 24)
        viewport.setViewPosition((int) std::min((double) std::numeric_limits<int>::max() / 4, std::max(0.0, x - viewport.getWidth() * 0.3)), viewport.getViewPositionY());
    refresh(); setStatus(target->text + " / " + timeText(target->time));
}
void Workstation::choosePlaylistImport()
{
    if (busy) return;
    chooser = std::make_unique<juce::FileChooser>("Import playlist", juce::File::getSpecialLocation(juce::File::userMusicDirectory), "*.m3u;*.m3u8", false);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe = juce::Component::SafePointer<Workstation>(this)](const auto& files)
    { if (safe && files.getResult() != juce::File()) safe->importFiles(files.getResults()); });
}
void Workstation::choosePlaylistExport(bool text)
{
    if (busy) return;
    if (!ensureTempoReady([safe = juce::Component::SafePointer<Workstation>(this), text] { if (safe) safe->choosePlaylistExport(text); })) return;
    if (engine.project.clips.empty()) { setStatus("Arrange some clips before exporting a playlist."); return; }
    chooser = std::make_unique<juce::FileChooser>(text ? "Export track list" : "Export source playlist",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(text ? "Mix tracklist.txt" : "Mix sources.m3u8"), text ? "*.txt" : "*.m3u8;*.m3u", false);
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
        [safe = juce::Component::SafePointer<Workstation>(this), text](const auto& files)
    {
        if (!safe || safe->busy || files.getResult() == juce::File()) return;
        auto destination = files.getResult();
        if (!destination.hasFileExtension(text ? "txt" : "m3u;m3u8")) destination = destination.withFileExtension(text ? "txt" : "m3u8");
        const auto error = safe->engine.exportPlaylist(destination, text ? PlaylistExport::text : PlaylistExport::m3u);
        if (error.isNotEmpty()) safe->fail(error);
        else safe->setStatus("Exported " + destination.getFullPathName() + (text ? " / mix times and labels" : " / original sources only; mix edits stay in your session"));
    });
}
}
