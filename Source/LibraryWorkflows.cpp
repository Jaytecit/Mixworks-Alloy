#include "Workstation.h"
#include "RetryReview.h"
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
    setStatus("Selected " + juce::String(rows.size()) + " songs with missing, outdated or uncertain analysis. Ctrl/Shift selects individual songs.");
}
bool Workstation::canRetryAnalysis(const Media& media)
{
    return (media.manualBpm <= 0 && media.manualBeatOrigin < 0 && needsFocusedTempo(media.analysis))
        || (media.manualKey < 0 && needsFocusedKey(media.analysis));
}
void Workstation::reanalyseTracks(int scope, bool focused, std::vector<std::shared_ptr<Media>> targets)
{
    if (busy) return;
    if (engine.playing) { setStatus("Pause playback before re-analysing songs."); return; }
    if (!timelineLoads.empty() || pendingTempo > 0 || tempoPreparationPending || engine.exporting)
    { setStatus("Wait for timeline audio and tempo preparation before re-analysing songs."); return; }
    if (targets.empty()) targets = analysisTargets(scope);
    const int selected = (int) targets.size();
    if (focused) targets.erase(std::remove_if(targets.begin(), targets.end(), [](const auto& m) { return !canRetryAnalysis(*m); }), targets.end());
    if (targets.empty()) { setStatus(focused ? "No tracks need another retry. Review remaining failures in Source corrections." : "Select tracks to analyse first."); return; }
    setBusy(true); setStatus(focused ? "Retrying BPM and key. Your manual corrections will be kept."
                                   : "Checking BPM, beat one and key. Your manual corrections will be kept.");
    auto cancelled = std::make_shared<std::atomic<bool>>(false); importCancellation = cancelled;
    importProgress.begin((int) targets.size()); importProgress.setReanalysis(focused);
    auto safe = juce::Component::SafePointer<Workstation>(this);
    const auto retryReport=catalogueFile.getSiblingFile("AnalysisRetries").getChildFile("retry-"+juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S")+"-"+juce::Uuid().toString().substring(0,8)+".json");
    workers.addJob([safe, targets, cancelled, focused, retryReport, skipped = selected - (int) targets.size()]
    {
        juce::Array<juce::var> evidence;
        juce::AudioFormatManager formats; formats.registerBasicFormats(); juce::StringArray failures; int processed = 0, improved = 0;
        for (const auto& previous : targets)
        {
            if (*cancelled) break;
            const auto record = previous->analysisRecord();
            auto result = readMedia(formats, previous->file, [safe, cancelled, name = previous->file.getFileName()](MediaReadStage stage)
            {
                juce::MessageManager::callAsync([safe, cancelled, name, stage] { if (safe && safe->importCancellation == cancelled) safe->importProgress.showFile(name, stage); });
            }, focused ? AnalysisCacheMode::focused : AnalysisCacheMode::refresh, MediaReadPurpose::catalogue, &record, cancelled);
            if (*cancelled && !result.media) break; // Completed writes remain visible; cancelled in-flight work is not a failure.
            if (!result.media) failures.add(result.error);
            else if (!focused && result.media->analysis.rhythmIssue.isNotEmpty()) { failures.add(previous->file.getFileName() + ": " + result.media->analysis.rhythmIssue); result.media.reset(); }
            else if (focused)
            {
                const auto& a = result.media->analysis;
                improved += (a.focusedTempo == 2 && needsFocusedTempo(record.analysis)) || (a.focusedKey == 2 && needsFocusedKey(record.analysis));
            }
            ++processed;
            juce::var review;
            if (focused)
            {
                review = retryReviewRow(*previous, result.media.get(), result.error);
                const auto saved = saveRetryReview(review); review.getDynamicObject()->setProperty("volatile", saved.failed());
                juce::var row(new juce::DynamicObject()); auto* e=row.getDynamicObject(); e->setProperty("source",previous->file.getFullPathName()); e->setProperty("before",analysisRecordToJson(record));
                if (result.media) { e->setProperty("after",analysisRecordToJson(result.media->analysisRecord())); e->setProperty("decision",result.media->analysis.retryEvidence); }
                else e->setProperty("failure",result.error); evidence.add(row);
                if (retryReport.getParentDirectory().createDirectory().wasOk()) retryReport.replaceWithText(juce::JSON::toString(evidence),false,false,"\n");
            }
            juce::MessageManager::callAsync([safe, cancelled, previous, review, fresh = std::move(result.media)]
            {
                if (!safe || safe->importCancellation != cancelled) return;
                if (fresh)
                {
                    clearRetryError(*fresh, true, safe.getComponent());
                    if (fresh->analysisCacheWarning.isEmpty()) clearRetryError(*fresh, false, safe.getComponent());
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
                if (review.isObject())
                {
                    publishRetryResult(*safe, review); const auto message = retryResultText(review);
                    safe->importProgress.showResult(message);
                    safe->setStatus(message + ((bool) review["volatile"] ? " Retry list could not be saved." : juce::String()));
                }
            });
        }
        juce::MessageManager::callAsync([safe, cancelled, processed, total = (int) targets.size(), failures, focused, improved, skipped]
        {
            if (!safe || safe->importCancellation != cancelled) return;
            safe->importProgress.setVisible(false); safe->importCancellation.reset(); safe->setBusy(false); safe->refresh();
            safe->setStatus(juce::String(processed < total ? "Analysis stopped. " : "Re-analysis complete. ") + juce::String(processed - failures.size()) + " refreshed; " + juce::String(failures.size()) + " failed. Manual corrections and arrangements retained.");
            if (focused)
            {
                auto sources = safe->engine.library; for (const auto& c : safe->engine.project.clips) sources.push_back(c.media);
                const auto remaining = retryIssues(sources, safe->getProperties()["focusedRetryRows"]);
                safe->setStatus(juce::String(processed < total ? "Retry stopped. " : "Retry complete. ") + juce::String((int) remaining.size())
                    + " tracks need review in Source corrections.");
            }
            if (std::any_of(safe->engine.project.clips.begin(), safe->engine.project.clips.end(), [](const auto& clip)
                { return clip.tempoSourceBpm > 0 && clip.media->manualBpm <= 0 && std::abs(clip.tempoSourceBpm - clip.media->bpm()) > 0.01; }))
                safe->setStatus(safe->status + " Apply source corrections to update the mix.");
            if (!focused && !failures.isEmpty()) safe->fail(failures.joinIntoString("\n"));
        });
    });
}
bool Workstation::adoptLibraryRefresh(const std::shared_ptr<Media>& previous, const std::shared_ptr<Media>& verified)
{
    const auto found = std::find(engine.library.begin(), engine.library.end(), previous);
    if (found == engine.library.end() || timelineLoads.count(previous->id)) return false;
    if (previous->sessionMetadataPending || previous->analysisSource == verified->analysisSource)
    {
        verified->id = previous->id; verified->manualBpm = previous->manualBpm;
        verified->manualBeatOrigin = previous->manualBeatOrigin; verified->manualKey = previous->manualKey;
        verified->name = previous->name;
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
                    else
                    {
                        auto pending = std::make_shared<Media>(); pending->file = source; pending->name = source.getFileNameWithoutExtension(); pending->sourceDetailsPending = true;
                        safe->engine.library.push_back(pending); safe->catalogueRefreshQueue.push_back(std::move(pending));
                    }
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
            std::optional<juce::String> genre;
            const auto sidecar = analysisRecordInputFile(previous->file);
            const bool sameSidecar = sidecar.getSize() == previous->analysisFileBytes && sidecar.getLastModificationTime().toMilliseconds() == previous->analysisFileModified;
            const bool unchanged = !previous->sessionMetadataPending && previous->analysisVersion == analysisRevision && !previous->analysisSource.sha256.isEmpty() && previous->file.existsAsFile()
                && previous->file.getSize() == previous->analysisSource.bytes && previous->file.getLastModificationTime().toMilliseconds() == previous->sourceModified
                && sameSidecar;
            if (unchanged && !previous->assets.genreRead) genre = readLibraryGenre(previous->file);
            if (!unchanged)
            {
                juce::AudioFormatManager formats; formats.registerBasicFormats(); const auto record = previous->analysisRecord();
                result = readMedia(formats, previous->file, {}, AnalysisCacheMode::readWrite, MediaReadPurpose::catalogue,
                    previous->analysisVersion != analysisRevision && !record.source.sha256.isEmpty() && (sameSidecar || !sidecar.exists()) ? &record : nullptr, cancelled);
                if (result.media && previous->sessionMetadataPending && previous->manualBeatOrigin > result.media->duration())
                { result.media.reset(); result.error = "Saved beat marker exceeds source audio: " + previous->file.getFullPathName(); }
            }
            juce::MessageManager::callAsync([safe, previous, result = std::move(result), unchanged, genre]() mutable
            {
                if (!safe) return; safe->catalogueRefreshPending = false;
                if (unchanged)
                {
                    if (genre)
                    {
                        const juce::ScopedLock guard(safe->engine.lock);
                        previous->assets.genre = *genre; previous->assets.genreRead = true;
                        safe->markCatalogueChanged(); safe->refresh();
                    }
                    return;
                }
                auto found = std::find(safe->engine.library.begin(), safe->engine.library.end(), previous);
                if (found == safe->engine.library.end()) return; // An older session's worker cannot publish into a newer entry.
                if (result.media)
                {
                    safe->adoptLibraryRefresh(previous, result.media);
                    safe->markCatalogueChanged(); safe->refresh();
                }
                else if (found != safe->engine.library.end() && *found == previous && !previous->hasPlayback())
                { const juce::ScopedLock guard(safe->engine.lock); previous->sourceDetailsPending = false; previous->playbackError = result.error; safe->refresh(); }
            });
        });
    }
}
}
