#include "Workstation.h"
#include "SourceSectionStore.h"
#include "DownbeatReferences.h"
#include <cmath>
#include <cstdlib>

namespace mixworks
{
void Workstation::editSavedDownbeat(const juce::String& id)
{
    if (busy || engine.exporting || pendingTempo > 0 || tempoPreparationPending || engine.rehearsalActive()
        || engine.automationRecordMode != AutomationRecordMode::read || engine.recordingAutomation() || engine.project.clipTransitionLocked(id)) return;
    const auto* clip = engine.project.clipFor(id);
    if (!clip || !clip->media || !clip->media->hasPlayback() || clip->media->sourceDetailsPending) return;
    const auto media = clip->media; const auto original = SourceCorrection::from(*media);
    const auto before = juce::JSON::toString(projectToJson(engine.project),true,17);
    const double anchor = media->downbeatOrigin();
    const auto exact = std::make_shared<std::pair<double,juce::String>>(anchor >= 0 ? anchor : 0,juce::String(anchor >= 0 ? anchor : 0,6));
    auto* dialog = new juce::AlertWindow("Save downbeat", media->libraryTitle(), juce::MessageBoxIconType::NoIcon, this);
    dialog->addTextEditor("source", exact->second, "Beat one (seconds)");
    dialog->getTextEditor("source")->setTooltip("Choose a known beat one anywhere in the track. Save keeps BPM and key; Beatmatch applies the timing to the mix.");
    dialog->addButton("Save to track",1,juce::KeyPress(juce::KeyPress::returnKey));
    dialog->addButton("Use playhead",2); dialog->addButton("Use IN marker",3);
    dialog->addButton("Cancel",0,juce::KeyPress(juce::KeyPress::escapeKey));
    auto choose = [safe = juce::Component::SafePointer<Workstation>(this),dialog,id,exact](bool playhead)
    {
        if (!safe) return; const auto* current = safe->engine.project.clipFor(id); if (!current) return;
        if (current->reversed) { safe->setStatus("For a reversed segment, enter the known beat-one source seconds directly."); return; }
        const double local = playhead ? safe->engine.position.load() - current->start : current->transitionTime(true);
        if (local < 0 || local >= current->length) { safe->setStatus("Place the playhead inside this segment before using its source position."); return; }
        exact->first = current->sourceTimeAt(local); exact->second = juce::String(exact->first,6); dialog->getTextEditor("source")->setText(exact->second);
    };
    dialog->getButton(1)->onClick = [choose] { choose(true); }; dialog->getButton(2)->onClick = [choose] { choose(false); };
    dialog->enterModalState(true,juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<Workstation>(this),dialog,id,media,original,before,exact](int answer)
    {
        if (!safe || answer != 1) return;
        const auto* current = safe->engine.project.clipFor(id); const auto correction = SourceCorrection::from(*media);
        if (!current || current->media != media || before != juce::JSON::toString(projectToJson(safe->engine.project),true,17)
            || original.bpm != correction.bpm || original.downbeat != correction.downbeat || original.key != correction.key
            || safe->engine.project.clipTransitionLocked(id) || safe->engine.automationRecordMode != AutomationRecordMode::read || safe->engine.recordingAutomation())
        { safe->setStatus("The source or arrangement changed. Reopen Save downbeat; the previous anchor was kept."); return; }
        const auto text = dialog->getTextEditorContents("source").trim().toStdString(); char* end = nullptr; const double source = std::strtod(text.c_str(),&end);
        if (text.empty() || end != text.c_str() + text.size() || !std::isfinite(source)) { safe->setStatus("Enter valid source seconds; the saved downbeat was kept."); return; }
        const double value = text == exact->second.toStdString() ? exact->first : source;
        auto edited = correction; edited.downbeat = value; const auto result = safe->applySourceCorrection(media,edited);
        safe->setStatus(result.failed() ? "Downbeat was not saved. " + result.getErrorMessage()
            : "Downbeat saved at " + juce::String(value,3) + " s.");
    }),true);
}
void Workstation::rememberSourceSectionBaseline(const std::shared_ptr<Media>& media, const SourceSections& old)
{
    const auto& key = media->analysisSource.sha256;
    if (engine.project.sourceSectionEdits.count(key) == 0)
    { sourceSectionUndoBaselines[key] = old; engine.project.sourceSectionEdits[key] = old; }
}
void Workstation::publishSourceSectionDefaults(const std::shared_ptr<Media>& source, const SourceSections& sections)
{
    {
        const juce::ScopedLock guard(engine.lock);
        auto apply = [&](const std::shared_ptr<Media>& media)
        { if (media && media->analysisSource == source->analysisSource) { media->sectionTemplate = sections; media->sectionTemplateError.clear(); } };
        apply(source); for (auto& media : engine.library) apply(media); for (auto& clip : engine.project.clips) apply(clip.media);
    }
    markCatalogueChanged();
}
bool Workstation::persistSourceSectionDefaults(const std::shared_ptr<Media>& source, const SourceSections& sections)
{
    if (!source) return false;
    if (source->sectionTemplateError.isNotEmpty()) { setStatus(source->sectionTemplateError + " Source sections were kept."); return false; }
    const auto error = writeSourceSectionRecord(source->file,source->analysisSource,source->sourceModified,source->sectionTemplate,sections);
    if (error.isNotEmpty()) { setStatus("Section edit was not saved. " + error); return false; }
    publishSourceSectionDefaults(source,sections); return true;
}
bool Workstation::persistSourceSectionHistory(const Project& target)
{
    struct Change { std::shared_ptr<Media> media; SourceSections old, next; };
    std::set<juce::String> keys; for (const auto& pair : engine.project.sourceSectionEdits) keys.insert(pair.first);
    for (const auto& pair : target.sourceSectionEdits) keys.insert(pair.first);
    std::vector<Change> changes;
    for (const auto& key : keys)
    {
        const auto baseline = sourceSectionUndoBaselines.find(key);
        if (baseline == sourceSectionUndoBaselines.end()) continue;
        const auto current = engine.project.sourceSectionEdits.find(key);
        const auto next = target.sourceSectionEdits.find(key);
        const auto& before = current == engine.project.sourceSectionEdits.end() ? baseline->second : current->second;
        const auto& after = next == target.sourceSectionEdits.end() ? baseline->second : next->second;
        if (before == after) continue;
        std::shared_ptr<Media> source;
        for (const auto& media : engine.library) if (media->analysisSource.sha256 == key) { source = media; break; }
        if (!source) for (const auto& clip : engine.project.clips) if (clip.media->analysisSource.sha256 == key) { source = clip.media; break; }
        if (!source || source->sectionTemplate != before || source->sectionTemplateError.isNotEmpty())
        { setStatus("Track section defaults changed or their source is unavailable. Undo kept the current data."); return false; }
        const auto disk = readSourceSectionRecord(source->analysisSource);
        if (disk.error.isNotEmpty() || disk.markers != before)
        { setStatus("Saved source sections changed in another window or cannot be read. Undo kept the current data."); return false; }
        changes.push_back({source,before,after});
    }
    for (size_t i = 0; i < changes.size(); ++i)
    {
        const auto& change = changes[i];
        const auto error = writeSourceSectionRecord(change.media->file,change.media->analysisSource,change.media->sourceModified,change.old,change.next);
        if (error.isEmpty()) continue;
        juce::String rollbackError;
        while (i > 0)
        {
            const auto& done = changes[--i]; const auto restored = writeSourceSectionRecord(done.media->file,done.media->analysisSource,done.media->sourceModified,done.next,done.old);
            if (restored.isNotEmpty()) rollbackError += " " + restored;
        }
        setStatus("Undo could not save track section defaults. " + error + (rollbackError.isNotEmpty() ? " Earlier records could not all be restored:" + rollbackError : juce::String())); return false;
    }
    for (const auto& change : changes) publishSourceSectionDefaults(change.media,change.next);
    return true;
}
}
