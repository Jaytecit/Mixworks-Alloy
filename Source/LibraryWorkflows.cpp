#include "Workstation.h"
#include <set>

namespace mixworks
{
std::vector<std::shared_ptr<Media>> Workstation::analysisTargets(int scope) const
{
    std::vector<std::shared_ptr<Media>> targets;
    auto add = [&](const auto& media)
    {
        if (media && std::none_of(targets.begin(), targets.end(), [&](const auto& previous) { return previous->file == media->file; })) targets.push_back(media);
    };
    if (scope == 1 || scope == 2)
    {
        for (const auto& media : engine.library) if (scope == 2 || media->needsAnalysis()) add(media);
    }
    else if (scope == 0)
    {
        const auto& table = browserPage == 1 ? playlistTable : libraryTable;
        const auto rows = table.table.getSelectedRows();
        for (int i = 0; i < rows.size(); ++i)
        {
            const int row = rows[i];
            if (browserPage == 1) { if (const auto* clip = table.clipAtRow(row)) add(clip->media); }
            else if (row >= 0 && row < (int) visibleMedia.size()) add(engine.library[(size_t) visibleMedia[(size_t) row]]);
        }
    }
    return targets;
}
void Workstation::selectTracksNeedingAnalysis()
{
    if (busy) return;
    search.setText({}, juce::dontSendNotification); showBrowserPage(0); filterLibrary();
    juce::SparseSet<int> rows;
    for (int row = 0; row < (int) visibleMedia.size(); ++row)
        if (engine.library[(size_t) visibleMedia[(size_t) row]]->needsAnalysis()) rows.addRange({row, row + 1});
    libraryTable.table.setSelectedRows(rows, juce::dontSendNotification);
    selectedLibraryId = rows.isEmpty() ? juce::String() : engine.library[(size_t) visibleMedia[(size_t) rows[0]]]->id;
    setStatus("Selected " + juce::String(rows.size()) + " tracks with missing, outdated or uncertain analysis. Ctrl/Shift selects individual tracks.");
}
void Workstation::reanalyseTracks(int scope)
{
    if (busy) return;
    if (engine.playing) { setStatus("Pause playback before re-analysing tracks."); return; }
    if (!timelineLoads.empty() || pendingTempo > 0 || tempoPreparationPending || engine.exporting)
    { setStatus("Wait for timeline audio and tempo preparation before re-analysing tracks."); return; }
    const auto targets = analysisTargets(scope);
    if (targets.empty()) { setStatus("No tracks selected for re-analysis."); return; }
    setBusy(true); setStatus("Re-analysing automatic BPM, key and downbeat. Manual corrections and arrangements are retained.");
    auto cancelled = std::make_shared<std::atomic<bool>>(false); importCancellation = cancelled;
    importProgress.begin((int) targets.size()); importProgress.setReanalysis();
    auto safe = juce::Component::SafePointer<Workstation>(this);
    workers.addJob([safe, targets, cancelled]
    {
        juce::AudioFormatManager formats; formats.registerBasicFormats(); juce::StringArray failures; int processed = 0;
        for (const auto& previous : targets)
        {
            if (*cancelled) break;
            const auto record = previous->analysisRecord();
            auto result = readMedia(formats, previous->file, [safe, cancelled, name = previous->file.getFileName()](MediaReadStage stage)
            {
                juce::MessageManager::callAsync([safe, cancelled, name, stage] { if (safe && safe->importCancellation == cancelled) safe->importProgress.showFile(name, stage); });
            }, AnalysisCacheMode::refresh, MediaReadPurpose::catalogue, &record, cancelled);
            if (!result.media) failures.add(result.error);
            else if (result.media->analysis.rhythmIssue.isNotEmpty()) { failures.add(previous->file.getFileName() + ": " + result.media->analysis.rhythmIssue); result.media.reset(); }
            ++processed;
            juce::MessageManager::callAsync([safe, cancelled, previous, fresh = std::move(result.media)]
            {
                if (!safe || safe->importCancellation != cancelled) return;
                if (fresh)
                {
                    const juce::ScopedLock guard(safe->engine.lock); std::set<Media*> updated;
                    auto update = [&](const std::shared_ptr<Media>& media)
                    {
                        if (media->file != previous->file || !(media->analysisSource == fresh->analysisSource) || !updated.insert(media.get()).second) return;
                        media->analysis = fresh->analysis; media->analysisVersion = fresh->analysisVersion; media->analysisFromCache = false;
                        media->analysisCacheWarning = fresh->analysisCacheWarning; media->analysisFileBytes = fresh->analysisFileBytes; media->analysisFileModified = fresh->analysisFileModified;
                    };
                    auto project = [&](Project& value) { for (auto& clip : value.clips) update(clip.media); };
                    project(safe->engine.project); for (auto& value : safe->undoHistory) project(value); for (auto& value : safe->redoHistory) project(value);
                    if (safe->tempoEditBaseline) project(*safe->tempoEditBaseline);
                    const auto catalogue = std::find_if(safe->engine.library.begin(), safe->engine.library.end(), [&](const auto& media) { return media->file == previous->file; });
                    if (catalogue != safe->engine.library.end()) safe->adoptLibraryRefresh(*catalogue, fresh);
                    safe->dirty = true; safe->markCatalogueChanged(); safe->refresh();
                }
                safe->importProgress.finishFile();
            });
        }
        juce::MessageManager::callAsync([safe, cancelled, processed, total = (int) targets.size(), failures]
        {
            if (!safe || safe->importCancellation != cancelled) return;
            safe->importProgress.setVisible(false); safe->importCancellation.reset(); safe->setBusy(false); safe->refresh();
            safe->setStatus(juce::String(processed < total ? "Analysis stopped. " : "Re-analysis complete. ") + juce::String(processed - failures.size()) + " refreshed; " + juce::String(failures.size()) + " failed. Manual corrections and arrangements retained.");
            if (std::any_of(safe->engine.project.clips.begin(), safe->engine.project.clips.end(), [](const auto& clip)
                { return clip.tempoSourceBpm > 0 && clip.media->manualBpm <= 0 && std::abs(clip.tempoSourceBpm - clip.media->bpm()) > 0.01; }))
                safe->setStatus(safe->status + " Beatmatch applies refreshed tempos to this mix.");
            if (!failures.isEmpty()) safe->fail(failures.joinIntoString("\n"));
        });
    });
}
bool Workstation::adoptLibraryRefresh(const std::shared_ptr<Media>& previous, const std::shared_ptr<Media>& verified)
{
    const auto found = std::find(engine.library.begin(), engine.library.end(), previous);
    if (found == engine.library.end() || timelineLoads.count(previous->id)) return false;
    if (previous->analysisSource == verified->analysisSource)
    {
        verified->id = previous->id; verified->manualBpm = previous->manualBpm;
        verified->manualBeatOrigin = previous->manualBeatOrigin; verified->manualKey = previous->manualKey;
    }
    if (selectedLibraryId == previous->id) selectedLibraryId = verified->id;
    // Only replace the catalogue entry. Arranged/Undo audio keeps its verified source object.
    *found = verified; markCatalogueChanged(); return true;
}
void Workstation::markCatalogueChanged()
{
    if (catalogueFile == juce::File()) return;
    catalogueDirty = true; catalogueChangedAt = juce::Time::getMillisecondCounterHiRes();
}
void Workstation::writeLibraryCatalogue()
{
    if (catalogueFile == juce::File() || catalogueWritePending) return;
    juce::Array<juce::File> sources; for (const auto& media : engine.library) sources.add(media->file);
    std::vector<std::shared_ptr<Media>> snapshots; for (const auto& media : engine.library) snapshots.push_back(media->catalogueCopy());
    catalogueWritePending = true; catalogueDirty = false;
    const auto file = catalogueFile; auto safe = juce::Component::SafePointer<Workstation>(this);
    workers.addJob([safe, file, sources, snapshots]
    {
        const auto error = rememberLibraryCatalogue(file, sources, snapshots);
        juce::MessageManager::callAsync([safe, error]
        {
            if (!safe) return; safe->catalogueWritePending = false;
            if (error.isNotEmpty()) safe->setStatus("Library catalogue warning: " + error);
        });
    });
}
void Workstation::tickLibraryCatalogue()
{
    if (gridWindow && gridWindow->isVisible()) return;
    if (catalogueRestorePending)
    {
        catalogueRestorePending = false; catalogueRefreshPending = true; setStatus("Opening the saved music library...");
        const auto file = catalogueFile; auto safe = juce::Component::SafePointer<Workstation>(this);
        workers.addJob([safe, file]
        {
            auto saved = readLibraryCatalogue(file);
            juce::MessageManager::callAsync([safe, saved = std::move(saved)]() mutable
            {
                if (!safe) return; safe->catalogueRefreshPending = false;
                if (saved.error.isNotEmpty()) { safe->setStatus(saved.error); return; }
                for (const auto& source : saved.files)
                {
                    if (std::any_of(safe->engine.library.begin(), safe->engine.library.end(), [&](const auto& m) { return m->file == source; })) continue;
                    const auto found = std::find_if(saved.media.begin(), saved.media.end(), [&](const auto& m) { return m->file == source; });
                    if (found != saved.media.end()) { safe->engine.library.push_back(*found); safe->catalogueRefreshQueue.push_back(*found); }
                    else { auto pending = std::make_shared<Media>(); pending->file = source; safe->catalogueRefreshQueue.push_back(std::move(pending)); }
                }
                safe->refresh(); safe->setStatus("Music library ready. Source checks run in the background.");
            });
        });
        return;
    }
    if (busy || engine.playing || engine.exporting || musicScanJobPending || pendingTempo > 0 || tempoPreparationPending) return;
    if (catalogueDirty && !catalogueWritePending && !catalogueRefreshPending && catalogueRefreshQueue.empty()
        && juce::Time::getMillisecondCounterHiRes() - catalogueChangedAt >= 1500) writeLibraryCatalogue();
    if (!catalogueRefreshPending && !catalogueRefreshQueue.empty() && timelineLoads.empty())
    {
        auto previous = catalogueRefreshQueue.front(); catalogueRefreshQueue.erase(catalogueRefreshQueue.begin()); catalogueRefreshPending = true;
        auto safe = juce::Component::SafePointer<Workstation>(this);
        workers.addJob([safe, previous, cancelled = sourceReadsCancelled]
        {
            MediaResult result;
            const auto sidecar = analysisRecordInputFile(previous->file);
            const bool sameSidecar = sidecar.getSize() == previous->analysisFileBytes && sidecar.getLastModificationTime().toMilliseconds() == previous->analysisFileModified;
            const bool unchanged = previous->analysisVersion == analysisRevision && !previous->analysisSource.sha256.isEmpty() && previous->file.existsAsFile()
                && previous->file.getSize() == previous->analysisSource.bytes && previous->file.getLastModificationTime().toMilliseconds() == previous->sourceModified
                && sameSidecar;
            if (!unchanged)
            {
                juce::AudioFormatManager formats; formats.registerBasicFormats(); const auto record = previous->analysisRecord();
                result = readMedia(formats, previous->file, {}, AnalysisCacheMode::readWrite, MediaReadPurpose::catalogue,
                    previous->analysisVersion != analysisRevision && !record.source.sha256.isEmpty() && (sameSidecar || !sidecar.exists()) ? &record : nullptr, cancelled);
            }
            juce::MessageManager::callAsync([safe, previous, result = std::move(result), unchanged]() mutable
            {
                if (!safe) return; safe->catalogueRefreshPending = false; if (unchanged) return;
                auto found = std::find_if(safe->engine.library.begin(), safe->engine.library.end(), [&](const auto& m) { return m->file == previous->file; });
                if (result.media)
                {
                    if (found == safe->engine.library.end()) safe->engine.library.push_back(result.media);
                    else safe->adoptLibraryRefresh(previous, result.media);
                    safe->markCatalogueChanged(); safe->refresh();
                }
                else if (found != safe->engine.library.end() && *found == previous && !previous->hasPlayback())
                { const juce::ScopedLock guard(safe->engine.lock); previous->playbackError = result.error; safe->timeline.repaint(); }
            });
        });
    }
}
}
