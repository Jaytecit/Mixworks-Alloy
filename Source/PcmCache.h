#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "AnalysisCache.h"
#include <atomic>

namespace mixworks
{
struct AudioCache
{
    juce::File file;
    std::unique_ptr<juce::MemoryMappedFile> mapping;
    int channels = 0, frames = 0;
    size_t payloadOffset = 0;
    bool transient = true;
    ~AudioCache();
    const float* channelData(int ch) const { return reinterpret_cast<const float*>(static_cast<const char*>(mapping->getData()) + payloadOffset) + (size_t) ch * frames; }
    static std::unique_ptr<AudioCache> create(const juce::AudioBuffer<float>&, juce::String& error);
    static std::unique_ptr<AudioCache> decode(juce::AudioFormatReader&, int channels, int frames, juce::String& error,
        const std::shared_ptr<std::atomic<bool>>& cancelled = {}, const juce::File& folder = {});
};
class PcmStore
{
public:
    explicit PcmStore(juce::File root, int64_t budget = 8LL * 1024 * 1024 * 1024);
    // The first caller selects the root for this process; tests select an isolated root before starting workers.
    static PcmStore& shared(const juce::File& initialRoot = defaultRoot());
    static juce::File defaultRoot();
    static juce::String decodedKey(const SourceIdentity&);
    static constexpr int headerBytes = 256;
    juce::File entryFile(const juce::String& key) const { return root.getChildFile(key + ".mwpcm"); }
    std::unique_ptr<AudioCache> read(const juce::String& key, double rate, int channels, int frames, bool touch = true);
    std::unique_ptr<AudioCache> write(const juce::String& key, double rate, const juce::AudioBuffer<float>&, juce::String& warning);
    const juce::String& error() const { return problem; }
    const juce::File& directory() const { return root; }
private:
    juce::File root;
    int64_t budget;
    juce::String problem;
    bool ownedEntry(const juce::File&) const;
    bool makeRoom(int64_t bytes, const juce::File& target);
};
}
