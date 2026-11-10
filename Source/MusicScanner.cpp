#include "MusicScanner.h"
#include "Workstation.h"
#include <windows.h>

namespace mixworks
{
namespace
{
juce::var preferenceDocument(const juce::File& file)
{
    if (file.isSymbolicLink() || file.getSize() > 4096) return {};
    auto input = file.createInputStream(); if (!input) return {};
    juce::MemoryBlock data; input->readIntoMemoryBlock(data, 4097);
    if (input->getStatus().failed() || data.getSize() > 4096) return {};
    return juce::JSON::parse(juce::String::fromUTF8((const char*) data.getData(), (int) data.getSize()));
}
bool validPreference(const juce::var& doc) { return doc.isObject() && doc["format"].toString() == "mixworks-library-scan" && doc["version"].isInt() && (int) doc["version"] == 1 && doc["enabled"].isBool(); }
std::pair<int64_t, int64_t> fileStamp(const juce::File& file)
{
    WIN32_FILE_ATTRIBUTE_DATA data {};
    if (!GetFileAttributesExW(file.getFullPathName().toWideCharPointer(), GetFileExInfoStandard, &data)
        || (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) return {};
    const auto bytes = (uint64_t(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
    const auto ticks = (uint64_t(data.ftLastWriteTime.dwHighDateTime) << 32) | data.ftLastWriteTime.dwLowDateTime;
    return {int64_t(bytes), int64_t(ticks / 10000) - 11644473600000LL};
}
ScanStamp stampWithSource(const juce::File& file, int64_t size, int64_t modified)
{
    const auto sidecar = fileStamp(analysisRecordInputFile(file));
    return {size, modified, sidecar.first, sidecar.second};
}
constexpr double watchedInterval = 10 * 60 * 1000;
constexpr double fallbackInterval = 60 * 1000;
}
juce::File scanPreferenceFile() { return PcmStore::defaultRoot().getParentDirectory().getChildFile("library-scan.json"); }
juce::File musicScanReportsFolder(const juce::File& preference) { return preference.getSiblingFile("ScanReports"); }
juce::Result writeMusicScanIssue(const juce::File& report, const juce::File& source, const MediaResult& loaded)
{
    juce::String reason = loaded.error;
    if (loaded.media) reason = loaded.media->analysisCacheWarning + "\n" + loaded.media->playbackCacheWarning;
    reason = reason.trim();
    if (loaded.media && reason.isEmpty()) return juce::Result::ok();
    if (reason.isEmpty()) reason = "The reader returned no audio. The source file was kept.";
    const auto directory = report.getParentDirectory().createDirectory();
    if (directory.failed()) return directory;
    juce::FileOutputStream output(report);
    if (!output.openedOk()) return output.getStatus();
    if (output.getPosition() == 0)
        output.writeText("Mixworks Music scan issues / " + juce::Time::getCurrentTime().toISO8601(true)
            + "\nFAILED means no library entry was added or refreshed. CACHE WARNING means the file was read successfully."
              "\nExisting analysis and audio are kept. Tools / Scan Music folder now retries failed files.\n\n",false,false,nullptr);
    output.writeText(juce::String(loaded.media ? "CACHE WARNING" : "FAILED") + "\n" + source.getFullPathName() + "\n" + reason + "\n\n",false,false,nullptr);
    output.flush(); return output.getStatus();
}
ScanPreference readScanPreference(const juce::File& file)
{
    if (!file.exists()) return {};
    const auto doc = preferenceDocument(file);
    if (!validPreference(doc) || (doc.hasProperty("folder") && (!doc["folder"].isString() || !juce::File::isAbsolutePath(doc["folder"].toString()))))
        return {false, "Automatic Music scan settings could not be read; the file was kept.", {}};
    return {(bool) doc["enabled"], {}, doc.hasProperty("folder") ? juce::File(doc["folder"].toString()) : juce::File()};
}
juce::String writeScanPreference(const juce::File& file, bool enabled, const juce::File& folder)
{
    juce::var doc;
    if (file.exists()) { doc = preferenceDocument(file); if (!validPreference(doc)) return "Unidentified Music scan settings were kept unchanged."; }
    else { auto* object = new juce::DynamicObject(); object->setProperty("format", "mixworks-library-scan"); object->setProperty("version", 1); doc = juce::var(object); }
    doc.getDynamicObject()->setProperty("enabled", enabled);
    if (folder == juce::File()) doc.getDynamicObject()->removeProperty("folder");
    else doc.getDynamicObject()->setProperty("folder", folder.getFullPathName());
    const auto text = juce::JSON::toString(doc);
    if (text.getNumBytesAsUTF8() > 4096) return "Music scan settings exceed their 4 KiB budget; the file was kept.";
    if (file.getParentDirectory().createDirectory().failed()) return "Cannot create the Music scan settings folder.";
    if (file.existsAsFile() && !file.copyFileTo(file.getSiblingFile(file.getFileName() + ".bak").getNonexistentSibling())) return "Cannot back up Music scan settings.";
    juce::TemporaryFile temp(file);
    return temp.getFile().replaceWithText(text, false, false, "\n") && temp.overwriteTargetFileWithTemporary() ? juce::String() : "Cannot save Music scan settings.";
}
ScanKnown::ScanKnown(const Media& media)
    : bytes(media.analysisSource.bytes), modified(media.sourceModified), analysisBytes(media.analysisFileBytes),
      analysisModified(media.analysisFileModified),
      current(!media.sourceDetailsPending && !media.sessionMetadataPending && media.analysisVersion == analysisRevision
              && media.playbackError.isEmpty() && media.analysisCacheWarning.isEmpty()
              && !media.analysisSource.sha256.isEmpty()) {}
bool ScanStamp::matches(const ScanKnown& known) const
{
    return known.current && bytes > 0 && bytes == known.bytes && modified == known.modified
        && analysisBytes == known.analysisBytes && analysisModified == known.analysisModified;
}
juce::String ScanStamp::key() const
{
    return juce::String(bytes) + ":" + juce::String(modified) + ":" + juce::String(analysisBytes) + ":" + juce::String(analysisModified);
}
ScanStamp musicFileStamp(const juce::File& file)
{
    const auto source = fileStamp(file);
    return stampWithSource(file, source.first, source.second);
}
MusicFolderWatch::MusicFolderWatch(const juce::File& root)
    : handle(FindFirstChangeNotificationW(root.getFullPathName().toWideCharPointer(), TRUE,
        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE)) {}
MusicFolderWatch::~MusicFolderWatch() { if (available()) FindCloseChangeNotification(handle); }
bool MusicFolderWatch::available() const { return handle != nullptr && handle != INVALID_HANDLE_VALUE; }
bool MusicFolderWatch::changed()
{
    if (!available() || WaitForSingleObject(handle, 0) != WAIT_OBJECT_0) return false;
    if (!FindNextChangeNotification(handle)) { FindCloseChangeNotification(handle); handle = nullptr; }
    return true;
}
MusicScan::MusicScan(const juce::File& root, std::map<juce::String, ScanKnown> sources, std::map<juce::String, juce::String> previous, juce::File reports, bool discovery)
    : cursor(root, true, "*", juce::File::findFiles, juce::File::FollowSymlinks::no), known(std::move(sources)), attempts(std::move(previous)), discoveryOnly(discovery)
{
    formats.registerBasicFormats();
    if (reports == juce::File()) reports = PcmStore::shared().directory().getSiblingFile("ScanReports");
    report = reports.getChildFile("scan-" + juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S") + "-" + juce::Uuid().toString().substring(0,8) + ".txt");
}
MusicScanProgress MusicScan::progress() const
{
    const juce::ScopedLock guard(progressLock); return latest;
}
MusicScanStep MusicScan::step()
{
    MusicScanStep result;
    for (int count = 0; count < 128 && cursor != juce::RangedDirectoryIterator(); ++count)
    {
        if (*cancelled) { result.done = true; return result; }
        const auto entry = *cursor; ++cursor;
        const auto file = entry.getFile();
        if (!file.hasFileExtension("wav;aif;aiff;flac;ogg;mp3") || file.isSymbolicLink()) continue;
        { const juce::ScopedLock guard(progressLock); ++latest.checked; latest.file = file; latest.stage = MediaReadStage::lookupAnalysis; }
        const auto path = file.getFullPathName().toLowerCase();
        auto found = known.extract(path);
        if (found && (discoveryOnly || found.mapped().queued)) continue;
        const auto stamp = stampWithSource(file, entry.getFileSize(), entry.getModificationTime().toMilliseconds());
        if (auto attempted = attempts.find(path); attempted != attempts.end() && attempted->second == stamp.key()) continue;
        if (found && stamp.matches(found.mapped()))
        {
            const juce::ScopedLock guard(progressLock);
            ++latest.unchanged;
            continue;
        }
        result.file = file;
        result.loaded = readMedia(formats, file, [this](MediaReadStage stage)
        { const juce::ScopedLock guard(progressLock); latest.stage = stage; }, AnalysisCacheMode::readWrite, MediaReadPurpose::catalogue, nullptr, cancelled);
        if (*cancelled) { result.file = juce::File(); result.done = true; return result; }
        result.stamp = result.loaded.media ? musicFileStamp(file).key() : stamp.key();
        if (!result.loaded.media || result.loaded.media->analysisCacheWarning.isNotEmpty() || result.loaded.media->playbackCacheWarning.isNotEmpty())
        {
            result.report = report;
            result.reportError = writeMusicScanIssue(report,file,result.loaded).getErrorMessage();
        }
        return result;
    }
    if (cursor == juce::RangedDirectoryIterator() && !discoveryOnly)
        for (int count = 0; count < 128 && !known.empty() && !*cancelled; ++count)
        {
            auto entry = known.extract(known.begin());
            const juce::File source(entry.key());
            if (!entry.mapped().queued && !source.existsAsFile()) result.missing.add(source);
        }
    result.done = *cancelled || (cursor == juce::RangedDirectoryIterator() && (discoveryOnly || known.empty()));
    return result;
}
void Workstation::stopMusicScan(bool completed)
{
    if (completed || musicScan || musicScanJobPending || musicScanManual)
        musicScanSummary = completed ? "Scan complete" : "Scan stopped";
    if (musicScan)
    {
        const auto progress = musicScan->progress();
        musicChecked = progress.checked; musicUnchanged = progress.unchanged;
        *musicScan->cancelled = true;
    }
    ++musicScanGeneration; musicScanManual = false; musicScan.reset();
    nextMusicScan = juce::Time::getMillisecondCounterHiRes() + (musicFolderWatch && musicFolderWatch->available() ? watchedInterval : fallbackInterval);
    if (completed && catalogueDirty && !catalogueWritePending) writeLibraryCatalogue();
    refreshMusicScanStatus();
}
void Workstation::toggleMusicScan()
{
    const bool enabled = !musicScanEnabled;
    const auto error = writeScanPreference(musicPreference, enabled, musicFolderPreference);
    if (error.isNotEmpty()) { fail(error); return; }
    musicScanEnabled = enabled;
    if (!enabled) { musicFolderWatch.reset(); stopMusicScan(); musicScanSummary.clear(); }
    else nextMusicScan = 0;
    setStatus(enabled ? "Automatic Music folder scanning enabled (when idle)." : "Automatic Music folder scanning disabled.");
}
void Workstation::selectMusicScanFolder(const juce::File& folder)
{
    const auto error = writeScanPreference(musicPreference, musicScanEnabled, folder);
    if (error.isNotEmpty()) { fail(error); return; }
    stopMusicScan(); musicFolderWatch.reset(); musicAttempts.clear(); musicScanSummary.clear();
    musicFolderPreference = folder;
    musicFolder = folder == juce::File() ? juce::File::getSpecialLocation(juce::File::userMusicDirectory) : folder;
    nextMusicScan = 0;
    setStatus("Music scan folder: " + musicFolder.getFullPathName());
}
void Workstation::beginMusicScan(bool manuallyRequested)
{
    if (busy || !timelineLoads.empty() || engine.playing || engine.exporting || musicScanJobPending) { nextMusicScan = juce::Time::getMillisecondCounterHiRes() + 60000; return; }
    stopMusicScan(); musicAdded = musicRefreshed = musicProblems = musicFailures = 0;
    musicChecked = musicUnchanged = musicReused = musicAnalysed = musicSaved = 0;
    if (manuallyRequested) musicAttempts.clear();
    musicLastIssue.clear();
    musicScanSummary.clear();
    musicScanManual = manuallyRequested; const auto generation = musicScanSerial = musicScanGeneration;
    std::map<juce::String, ScanKnown> known;
    for (const auto& media : engine.library)
        if (media->file.isAChildOf(musicFolder)) known.try_emplace(media->file.getFullPathName().toLowerCase(), *media);
    for (const auto& media : catalogueRefreshQueue)
        if (const auto found = known.find(media->file.getFullPathName().toLowerCase()); found != known.end()) found->second.queued = true;
    // Constructing the iterator can touch the filesystem; it belongs to the worker too.
    musicScanJobPending = true;
    auto safe = juce::Component::SafePointer<Workstation>(this); const auto root = musicFolder; const auto attempts = musicAttempts;
    const auto reports = musicScanReportsFolder(musicPreference);
    const bool discovery = discoverLibraryOnStartup && !manuallyRequested;
    const bool startWatch = musicScanEnabled && (!musicFolderWatch || !musicFolderWatch->available());
    workers.addJob([safe, root, known = std::move(known), attempts, generation, manuallyRequested, reports, discovery, startWatch]() mutable
    {
        if (!root.isDirectory())
        {
            juce::MessageManager::callAsync([safe, root, generation, manuallyRequested]
            {
                if (!safe) return; safe->musicScanJobPending = false;
                if (safe->musicScanGeneration != generation) return;
                safe->stopMusicScan(); safe->musicScanSummary = "Music folder unavailable"; safe->refreshMusicScanStatus();
                if (manuallyRequested) safe->setStatus("Music folder is unavailable: " + root.getFullPathName());
            });
            return;
        }
        auto watch = startWatch ? std::make_shared<MusicFolderWatch>(root) : nullptr;
        auto scan = std::make_shared<MusicScan>(root, std::move(known), std::move(attempts), reports, discovery);
        juce::MessageManager::callAsync([safe, scan, watch, generation]
        {
            if (!safe) return; safe->musicScanJobPending = false;
            if (safe->musicScanGeneration != generation || (!safe->musicScanEnabled && !safe->musicScanManual)) return;
            if (watch) safe->musicFolderWatch = watch;
            safe->musicScan = scan;
            safe->discoverLibraryOnStartup = false;
        });
    });
}
void Workstation::tickMusicScan()
{
    if (gridWindow && gridWindow->isVisible()) return;
    if (!musicScan && !musicScanJobPending && musicScanEnabled && musicFolderWatch && musicFolderWatch->changed())
        nextMusicScan = juce::Time::getMillisecondCounterHiRes() + 1500;
    if (catalogueOpening || catalogueRestorePending) return;
    if ((!musicScanEnabled && !musicScanManual) || busy || !timelineLoads.empty() || engine.playing || engine.exporting || pendingTempo > 0 || tempoPreparationPending || musicScanJobPending) return;
    if (!musicScan)
    {
        if (juce::Time::getMillisecondCounterHiRes() >= nextMusicScan) beginMusicScan(musicScanManual);
        return;
    }
    auto scan = musicScan; auto safe = juce::Component::SafePointer<Workstation>(this); musicScanJobPending = true;
    workers.addJob([safe, scan]
    {
        auto result = std::make_shared<MusicScanStep>(scan->step());
        juce::MessageManager::callAsync([safe, scan, result]
        {
            if (!safe) return; safe->musicScanJobPending = false;
            if (safe->musicScan != scan || *scan->cancelled) return;
            for (const auto& file : result->missing)
                for (const auto& media : safe->engine.library)
                    if (media->file == file && !media->hasPlayback())
                    {
                        const juce::ScopedLock guard(safe->engine.lock);
                        media->playbackError = "Missing audio file: " + media->file.getFullPathName();
                        safe->catalogueTablePending = true;
                        break;
                    }
            if (result->file != juce::File())
            {
                const auto path = result->file.getFullPathName().toLowerCase(); auto loaded = result->loaded.media;
                if (result->report != juce::File()) safe->musicReport = result->report;
                if (!loaded) { ++safe->musicFailures; safe->musicAttempts[path] = result->stamp; safe->musicLastIssue = result->loaded.error; }
                else
                {
                    const auto existing = std::find_if(safe->engine.library.begin(), safe->engine.library.end(), [&](const auto& source) { return source->file == result->file; });
                    bool published = false;
                    if (existing == safe->engine.library.end()) { safe->engine.library.push_back(loaded); ++safe->musicAdded; published = true; }
                    else if (safe->adoptLibraryRefresh(*existing, loaded)) { ++safe->musicRefreshed; published = true; }
                    if (published)
                    {
                        safe->markCatalogueChanged(loaded, safe->musicScanSerial);
                        if (loaded->analysisFromCache) ++safe->musicReused; else ++safe->musicAnalysed;
                        safe->catalogueTablePending = true;
                    }
                    if (loaded->analysisCacheWarning.isNotEmpty() || loaded->playbackCacheWarning.isNotEmpty()) { ++safe->musicProblems; safe->musicAttempts[path] = result->stamp; }
                }
                if (result->reportError.isNotEmpty()) safe->musicLastIssue = "Could not save the scan report: " + result->reportError;
            }
            if (result->done)
            {
                safe->stopMusicScan(true);
                // The library notice is persistent; do not overwrite an unrelated editing status every idle pass.
                if (safe->musicFailures || safe->musicProblems) safe->setStatus("Music scan finished with issues. Tools / Music scan reports lists affected files and reasons.");
            }
        });
    });
}
}
