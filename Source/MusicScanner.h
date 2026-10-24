#pragma once
#include "Model.h"
#include <map>

namespace mixworks
{
struct ScanKnown { SourceIdentity source; int64_t modified = 0; };
struct MusicScanStep { juce::File file; MediaResult loaded; juce::String stamp; bool done = false; juce::File report; juce::String reportError; };
struct MusicScanProgress { int checked = 0; juce::File file; MediaReadStage stage = MediaReadStage::lookupAnalysis; };
juce::String musicFileStamp(const juce::File&);
struct ScanPreference { bool enabled = true; juce::String error; };
ScanPreference readScanPreference(const juce::File&);
juce::String writeScanPreference(const juce::File&, bool enabled);
juce::File scanPreferenceFile();
juce::File musicScanReportsFolder(const juce::File& preference);
juce::Result writeMusicScanIssue(const juce::File& report, const juce::File& source, const MediaResult&);
class MusicScan
{
public:
    MusicScan(const juce::File& root, std::map<juce::String, ScanKnown> known, std::map<juce::String, juce::String> attempts, juce::File reports = {});
    MusicScanStep step(); // Worker-owned iterator. One audio file per step, at most 128 directory entries.
    MusicScanProgress progress() const;
    std::atomic<bool> cancelled {false};
private:
    mutable juce::CriticalSection progressLock;
    MusicScanProgress latest;
    juce::RangedDirectoryIterator cursor;
    std::map<juce::String, ScanKnown> known;
    std::map<juce::String, juce::String> attempts;
    juce::AudioFormatManager formats;
    juce::File report;
};
}
