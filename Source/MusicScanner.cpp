#include "MusicScanner.h"
#include "Workstation.h"

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
}
juce::File scanPreferenceFile() { return PcmStore::defaultRoot().getParentDirectory().getChildFile("library-scan.json"); }
ScanPreference readScanPreference(const juce::File& file)
{
    if (!file.exists()) return {};
    const auto doc = preferenceDocument(file);
    if (!validPreference(doc)) return {false, "Automatic Music scan settings could not be read; the file was kept."};
    return {(bool) doc["enabled"], {}};
}
juce::String writeScanPreference(const juce::File& file, bool enabled)
{
    juce::var doc;
    if (file.exists()) { doc = preferenceDocument(file); if (!validPreference(doc)) return "Unidentified Music scan settings were kept unchanged."; }
    else { auto* object = new juce::DynamicObject(); object->setProperty("format", "mixworks-library-scan"); object->setProperty("version", 1); doc = juce::var(object); }
    doc.getDynamicObject()->setProperty("enabled", enabled);
    const auto text = juce::JSON::toString(doc);
    if (text.getNumBytesAsUTF8() > 4096) return "Music scan settings exceed their 4 KiB budget; the file was kept.";
    if (file.getParentDirectory().createDirectory().failed()) return "Cannot create the Music scan settings folder.";
    if (file.existsAsFile() && !file.copyFileTo(file.getSiblingFile(file.getFileName() + ".bak").getNonexistentSibling())) return "Cannot back up Music scan settings.";
    juce::TemporaryFile temp(file);
    return temp.getFile().replaceWithText(text, false, false, "\n") && temp.overwriteTargetFileWithTemporary() ? juce::String() : "Cannot save Music scan settings.";
}
juce::String musicFileStamp(const juce::File& file)
{
    const auto record = analysisRecordInputFile(file);
    return juce::String(file.getSize()) + ":" + juce::String(file.getLastModificationTime().toMilliseconds()) + ":" + juce::String(record.getSize()) + ":" + juce::String(record.getLastModificationTime().toMilliseconds()) + ":" + juce::String(analysisRevision);
}
MusicScan::MusicScan(const juce::File& root, std::map<juce::String, ScanKnown> sources, std::map<juce::String, juce::String> previous)
    : cursor(root, true, "*", juce::File::findFiles, juce::File::FollowSymlinks::no), known(std::move(sources)), attempts(std::move(previous))
{ formats.registerBasicFormats(); }
MusicScanStep MusicScan::step()
{
    MusicScanStep result;
    for (int count = 0; count < 128 && cursor != juce::RangedDirectoryIterator(); ++count)
    {
        if (cancelled) { result.done = true; return result; }
        const auto file = (*cursor).getFile(); ++cursor;
        if (file.isSymbolicLink() || !file.hasFileExtension("wav;aif;aiff;flac;ogg;mp3")) continue;
        const auto path = file.getFullPathName(), stamp = musicFileStamp(file);
        if (auto attempted = attempts.find(path); attempted != attempts.end() && attempted->second == stamp) continue;
        if (const auto found = known.find(path); found != known.end() && file.getSize() == found->second.source.bytes && file.getLastModificationTime().toMilliseconds() == found->second.modified
            && readAnalysisRecord(file, found->second.source).reusableAutomatic) continue;
        result.file = file; result.loaded = readMedia(formats, file, {}, AnalysisCacheMode::readWrite, MediaReadPurpose::catalogue); result.stamp = musicFileStamp(file); return result;
    }
    result.done = cancelled || cursor == juce::RangedDirectoryIterator(); return result;
}
void Workstation::stopMusicScan()
{
    if (musicScan) musicScan->cancelled = true;
    ++musicScanGeneration; musicScanManual = false; musicScan.reset(); nextMusicScan = juce::Time::getMillisecondCounterHiRes() + 60000;
}
void Workstation::toggleMusicScan()
{
    const bool enabled = !musicScanEnabled;
    const auto error = writeScanPreference(musicPreference, enabled);
    if (error.isNotEmpty()) { fail(error); return; }
    musicScanEnabled = enabled;
    if (!enabled) stopMusicScan(); else nextMusicScan = 0;
    setStatus(enabled ? "Automatic Music folder scanning enabled (when idle)." : "Automatic Music folder scanning disabled.");
}
void Workstation::beginMusicScan(bool manuallyRequested)
{
    if (busy || !timelineLoads.empty() || engine.playing || engine.exporting || musicScanJobPending) { nextMusicScan = juce::Time::getMillisecondCounterHiRes() + 60000; return; }
    stopMusicScan(); musicAdded = musicRefreshed = musicProblems = 0;
    musicScanManual = manuallyRequested; const auto generation = musicScanGeneration;
    std::map<juce::String, ScanKnown> known;
    for (const auto& media : engine.library) known[media->file.getFullPathName()] = {media->analysisSource, media->sourceModified};
    // Constructing the iterator can touch the filesystem; it belongs to the worker too.
    musicScanJobPending = true;
    auto safe = juce::Component::SafePointer<Workstation>(this); const auto root = musicFolder; const auto attempts = musicAttempts;
    workers.addJob([safe, root, known = std::move(known), attempts, generation, manuallyRequested]
    {
        if (!root.isDirectory())
        {
            juce::MessageManager::callAsync([safe, root, generation, manuallyRequested]
            {
                if (!safe) return; safe->musicScanJobPending = false;
                if (safe->musicScanGeneration != generation) return;
                safe->stopMusicScan(); if (manuallyRequested) safe->setStatus("Music folder is unavailable: " + root.getFullPathName());
            });
            return;
        }
        auto scan = std::make_shared<MusicScan>(root, known, attempts);
        juce::MessageManager::callAsync([safe, scan, generation]
        {
            if (!safe) return; safe->musicScanJobPending = false;
            if (safe->musicScanGeneration != generation || safe->busy || safe->engine.playing || (!safe->musicScanEnabled && !safe->musicScanManual)) return;
            safe->musicScan = scan;
        });
    });
}
void Workstation::tickMusicScan()
{
    if (gridWindow && gridWindow->isVisible()) return;
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
            if (safe->musicScan != scan || scan->cancelled) return;
            if (safe->busy || safe->engine.playing || (!safe->musicScanEnabled && !safe->musicScanManual)) { safe->stopMusicScan(); return; }
            if (result->file != juce::File())
            {
                const auto path = result->file.getFullPathName(); auto loaded = result->loaded.media;
                if (!loaded) { ++safe->musicProblems; safe->musicAttempts[path] = result->stamp; }
                else
                {
                    const auto existing = std::find_if(safe->engine.library.begin(), safe->engine.library.end(), [&](const auto& source) { return source->file == result->file; });
                    if (existing == safe->engine.library.end()) { safe->engine.library.push_back(loaded); ++safe->musicAdded; safe->dirty = true; safe->markCatalogueChanged(); }
                    else if (safe->adoptLibraryRefresh(*existing, loaded)) ++safe->musicRefreshed;
                    if (loaded->analysisCacheWarning.isNotEmpty() || loaded->playbackCacheWarning.isNotEmpty()) { ++safe->musicProblems; safe->musicAttempts[path] = result->stamp; }
                    safe->filterLibrary(); safe->libraryTable.table.updateContent(); safe->libraryTable.table.repaint();
                }
            }
            if (result->done)
            {
                safe->stopMusicScan();
                if (safe->musicAdded || safe->musicRefreshed || safe->musicProblems) safe->setStatus("Music scan: " + juce::String(safe->musicAdded) + " added / " + juce::String(safe->musicRefreshed) + " refreshed / " + juce::String(safe->musicProblems) + " warnings. Existing clips keep their audio; see BPM / Key for cache warnings.");
            }
        });
    });
}
}
