#pragma once
#include "Model.h"
#include <map>

namespace mixworks
{
struct ScanKnown { SourceIdentity source; int64_t modified = 0; };
struct MusicScanStep { juce::File file; MediaResult loaded; juce::String stamp; bool done = false; };
juce::String musicFileStamp(const juce::File&);
struct ScanPreference { bool enabled = true; juce::String error; };
ScanPreference readScanPreference(const juce::File&);
juce::String writeScanPreference(const juce::File&, bool enabled);
juce::File scanPreferenceFile();
class MusicScan
{
public:
    MusicScan(const juce::File& root, std::map<juce::String, ScanKnown> known, std::map<juce::String, juce::String> attempts);
    MusicScanStep step(); // Worker-owned iterator. One audio file per step, at most 128 directory entries.
    std::atomic<bool> cancelled {false};
private:
    juce::RangedDirectoryIterator cursor;
    std::map<juce::String, ScanKnown> known;
    std::map<juce::String, juce::String> attempts;
    juce::AudioFormatManager formats;
};
}
