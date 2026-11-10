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
                    safe->dirty = true; safe->refresh();
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
        verified->id = previous->id;
        if (previous->sessionMetadataPending || (previous->analysisFileModified == verified->analysisFileModified
                                               && previous->analysisFileBytes == verified->analysisFileBytes))
        {
            verified->manualBpm = previous->manualBpm;
            verified->manualBeatOrigin = previous->manualBeatOrigin; verified->manualKey = previous->manualKey;
        }
        verified->name = previous->name;
    }
    if (selectedLibraryId == previous->id) selectedLibraryId = verified->id;
    // Only replace the catalogue entry. Arranged/Undo audio keeps its verified source object.
    *found = verified; markCatalogueChanged(verified); return true;
}
void Workstation::markCatalogueChanged(const std::shared_ptr<Media>& media, uint64_t scan)
{
    if (catalogueFile == juce::File() || !media || media->file == juce::File()) return;
    auto& change = catalogueChanges[media->file.getFullPathName().toLowerCase()];
    change.media = media;
    if (scan != 0) change.scan = scan;
    if (!catalogueDirty) catalogueChangedAt = juce::Time::getMillisecondCounterHiRes();
    catalogueDirty = true;
}
void Workstation::writeLibraryCatalogue()
{
    if (catalogueFile == juce::File() || catalogueWritePending || catalogueChanges.empty()) return;
    catalogueWriting = std::move(catalogueChanges);
    catalogueChanges.clear();
    std::vector<std::shared_ptr<Media>> snapshots;
    for (const auto& entry : catalogueWriting) snapshots.push_back(entry.second.media->catalogueCopy());
    catalogueWritePending = true; catalogueDirty = false;
    const auto file = catalogueFile; auto safe = juce::Component::SafePointer<Workstation>(this);
    workers.addJob([safe, file, snapshots = std::move(snapshots)]
    {
        const auto error = saveLibraryStore(file, snapshots);
        juce::MessageManager::callAsync([safe, error]
        {
            if (!safe) return; safe->catalogueWritePending = false;
            if (error.isNotEmpty())
            {
                for (const auto& entry : safe->catalogueWriting)
                {
                    auto& pending = safe->catalogueChanges[entry.first];
                    if (!pending.media) pending = entry.second;
                    else if (!pending.scan) pending.scan = entry.second.scan;
                }
                safe->catalogueDirty = true;
                safe->catalogueChangedAt = juce::Time::getMillisecondCounterHiRes();
                if (safe->catalogueSaveError != error) safe->setStatus("Library save warning: " + error);
            }
            else
                for (const auto& entry : safe->catalogueWriting)
                    if (entry.second.scan != 0 && entry.second.scan == safe->musicScanSerial) ++safe->musicSaved;
            safe->catalogueSaveError = error;
            safe->catalogueWriting.clear();
        });
    });
}
void Workstation::flushLibraryCatalogue()
{
    for (const auto& entry : catalogueChanges) catalogueWriting[entry.first] = entry.second;
    std::vector<std::shared_ptr<Media>> snapshots;
    for (const auto& entry : catalogueWriting) snapshots.push_back(entry.second.media);
    if (const auto error = saveLibraryStore(catalogueFile, snapshots); error.isNotEmpty()) juce::Logger::writeToLog(error);
}
void Workstation::tickLibraryCatalogue()
{
    if (gridWindow && gridWindow->isVisible()) return;
    const auto now = juce::Time::getMillisecondCounterHiRes();
    if (catalogueTablePending && now >= nextCatalogueTableUpdate)
    {
        catalogueTablePending = false; nextCatalogueTableUpdate = now + 200;
        filterLibrary(); libraryTable.table.updateContent(); libraryTable.table.repaint();
    }
    if (catalogueRestorePending)
    {
        catalogueRestorePending = false; catalogueOpening = catalogueRefreshPending = true; setStatus("Opening the saved music library...");
        const auto file = catalogueFile; auto safe = juce::Component::SafePointer<Workstation>(this);
        workers.addJob([safe, file]
        {
            auto saved = openLibraryStore(file);
            juce::MessageManager::callAsync([safe, saved = std::move(saved)]() mutable
            {
                if (!safe) return; safe->catalogueOpening = safe->catalogueRefreshPending = false;
                std::set<juce::String> known;
                std::map<juce::String, std::shared_ptr<Media>> snapshots;
                for (auto& media : saved.media) snapshots.emplace(media->file.getFullPathName().toLowerCase(), std::move(media));
                for (const auto& media : safe->engine.library) known.insert(media->file.getFullPathName().toLowerCase());
                for (const auto& source : saved.files)
                {
                    const auto path = source.getFullPathName().toLowerCase();
                    if (!known.insert(path).second) continue;
                    const auto found = snapshots.find(path);
                    if (found != snapshots.end()) { safe->engine.library.push_back(found->second); safe->catalogueRefreshQueue.push_back(found->second); }
                    else
                    {
                        auto pending = std::make_shared<Media>(); pending->file = source; pending->name = source.getFileNameWithoutExtension(); pending->sourceDetailsPending = true;
                        safe->engine.library.push_back(pending); safe->catalogueRefreshQueue.push_back(std::move(pending));
                    }
                }
                safe->refresh(); safe->setStatus(saved.error.isNotEmpty() ? saved.error : "Music library ready. Source checks run in the background.");
            });
        });
        return;
    }
    if (catalogueDirty && !catalogueWritePending
        && now - catalogueChangedAt >= (catalogueSaveError.isEmpty() ? 350 : 5000))
        writeLibraryCatalogue();
    if (busy || engine.playing || engine.exporting || musicScanJobPending || pendingTempo > 0 || tempoPreparationPending) return;
    if (!catalogueRefreshPending && !catalogueRefreshQueue.empty() && timelineLoads.empty())
    {
        struct Candidate
        {
            std::shared_ptr<Media> media;
            ScanKnown known;
            bool readGenre;
            double sessionOrigin;
            std::optional<TrackAnalysisRecord> oldRecord;
        };
        std::vector<Candidate> candidates;
        const auto count = std::min<size_t>(128, catalogueRefreshQueue.size());
        for (size_t i = 0; i < count; ++i)
        {
            const auto& media = catalogueRefreshQueue[i];
            std::optional<TrackAnalysisRecord> oldRecord;
            if (media->analysisVersion != analysisRevision && media->analysisSource.sha256.isNotEmpty()) oldRecord = media->analysisRecord();
            candidates.push_back({media, ScanKnown(*media), !media->assets.genreRead,
                                  media->sessionMetadataPending ? media->manualBeatOrigin : -1, std::move(oldRecord)});
        }
        catalogueRefreshPending = true;
        auto safe = juce::Component::SafePointer<Workstation>(this);
        workers.addJob([safe, candidates = std::move(candidates), cancelled = sourceReadsCancelled]
        {
            size_t checked = 0;
            std::vector<std::shared_ptr<Media>> missing;
            std::shared_ptr<Media> previous;
            MediaResult result;
            std::optional<juce::String> genre;
            for (const auto& item : candidates)
            {
                if (*cancelled) break;
                ++checked;
                const auto stamp = musicFileStamp(item.media->file);
                if (stamp.bytes == 0 && stamp.modified == 0) { missing.push_back(item.media); continue; }
                const bool unchanged = stamp.matches(item.known);
                if (unchanged && !item.readGenre) continue;
                previous = item.media;
                if (unchanged) genre = readLibraryGenre(previous->file);
                else
                {
                    const bool sameSidecar = stamp.analysisBytes == item.known.analysisBytes && stamp.analysisModified == item.known.analysisModified;
                    juce::AudioFormatManager formats; formats.registerBasicFormats();
                    result = readMedia(formats, previous->file, {}, AnalysisCacheMode::readWrite, MediaReadPurpose::catalogue,
                        item.oldRecord && (sameSidecar || stamp.analysisBytes == 0) ? &*item.oldRecord : nullptr, cancelled);
                    if (result.media && item.sessionOrigin > result.media->duration())
                    { result.media.reset(); result.error = "Saved beat marker exceeds source audio: " + previous->file.getFullPathName(); }
                }
                break;
            }
            auto first = candidates.front().media;
            juce::MessageManager::callAsync([safe, first, checked, missing = std::move(missing), previous, result = std::move(result), genre]() mutable
            {
                if (!safe) return; safe->catalogueRefreshPending = false;
                auto& queue = safe->catalogueRefreshQueue;
                if (queue.empty() || queue.front() != first || queue.size() < checked) return;
                queue.erase(queue.begin(), queue.begin() + (ptrdiff_t) checked);
                bool changed = false;
                const auto present = [&](const auto& media)
                {
                    return std::find(safe->engine.library.begin(), safe->engine.library.end(), media) != safe->engine.library.end();
                };
                const auto unavailable = [&](const auto& media, const auto& reason)
                {
                    if (!media->hasPlayback())
                    {
                        const juce::ScopedLock guard(safe->engine.lock);
                        media->sourceDetailsPending = false; media->playbackError = reason; changed = true;
                    }
                };
                for (const auto& media : missing)
                    if (present(media)) unavailable(media, "Missing audio file: " + media->file.getFullPathName());
                if (previous && present(previous))
                {
                    if (genre)
                    {
                        const juce::ScopedLock guard(safe->engine.lock);
                        previous->assets.genre = *genre; previous->assets.genreRead = true;
                        safe->markCatalogueChanged(previous); changed = true;
                    }
                    else if (result.media) changed |= safe->adoptLibraryRefresh(previous, result.media);
                    else unavailable(previous, result.error);
                }
                if (changed) safe->catalogueTablePending = true;
            });
        });
    }
}
}
