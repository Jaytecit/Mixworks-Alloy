#pragma once
#include "Model.h"
#include <map>

namespace mixworks
{
struct ScanKnown
{
    int64_t bytes = 0, modified = 0, analysisBytes = 0, analysisModified = 0;
    bool current = false, queued = false;
    explicit ScanKnown(const Media&);
};
struct ScanStamp
{
    int64_t bytes = 0, modified = 0, analysisBytes = 0, analysisModified = 0;
    bool matches(const ScanKnown&) const;
    juce::String key() const;
};
struct MusicScanStep { juce::File file; MediaResult loaded; juce::String stamp; bool done = false; juce::File report; juce::String reportError; juce::Array<juce::File> missing; };
struct MusicScanProgress { int checked = 0, unchanged = 0; juce::File file; MediaReadStage stage = MediaReadStage::lookupAnalysis; };
ScanStamp musicFileStamp(const juce::File&);
struct ScanPreference { bool enabled = true; juce::String error; juce::File folder; };
ScanPreference readScanPreference(const juce::File&);
juce::String writeScanPreference(const juce::File&, bool enabled, const juce::File& folder = {});
juce::File scanPreferenceFile();
juce::File musicScanReportsFolder(const juce::File& preference);
juce::Result writeMusicScanIssue(const juce::File& report, const juce::File& source, const MediaResult&);
class MusicFolderWatch
{
public:
    explicit MusicFolderWatch(const juce::File&);
    ~MusicFolderWatch();
    bool changed();
    bool available() const;
private:
    void* handle = nullptr;
};
class MusicScan
{
public:
    MusicScan(const juce::File& root, std::map<juce::String, ScanKnown> known, std::map<juce::String, juce::String> attempts, juce::File reports = {}, bool discovery = false);
    MusicScanStep step(); // Worker-owned iterator. One audio file per step, at most 128 directory entries.
    MusicScanProgress progress() const;
    std::shared_ptr<std::atomic<bool>> cancelled = std::make_shared<std::atomic<bool>>(false);
private:
    mutable juce::CriticalSection progressLock;
    MusicScanProgress latest;
    juce::RangedDirectoryIterator cursor;
    std::map<juce::String, ScanKnown> known;
    std::map<juce::String, juce::String> attempts;
    juce::AudioFormatManager formats;
    juce::File report;
    bool discoveryOnly = false;
};
}
