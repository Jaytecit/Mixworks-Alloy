#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <memory>

namespace mixworks
{
// Original files are identities, not decoded songs. All instances share one
// bounded read-ahead pool. Undo/catalogue ownership therefore owns no PCM.
class OriginalAudio : public std::enable_shared_from_this<OriginalAudio>
{
public:
    static constexpr int blockFrames = 32768, poolBlocks = 128;
    static constexpr int64_t poolBytes = int64_t(blockFrames) * 2 * sizeof(float) * poolBlocks;
    OriginalAudio(juce::File, double rate, int channels, int frames);
    float sample(int channel, int frame) const;
    bool request(int first, int last) const; // Exclusive end; queues without waiting.
    bool prepare(int first, int last) const; // Non-realtime destination preparation; waits outside the engine lock.
    // Display-only, exclusive end. Queues missing blocks and never waits for
    // decoding or the pool mutex. False means detail is not available yet.
    bool waveformRange(int first, int last, float& peak, float& rms) const;
    bool failed() const { return failure.load(); }
    struct Statistics { uint64_t reads, misses, queued; int64_t bytes; };
    static Statistics statistics();
    static bool isRealtime();
    // Device callbacks never decode or wait. Offline consumers may wait for
    // the same bounded pool, without changing another consumer's read policy.
    struct Realtime
    {
        Realtime(); ~Realtime();
        bool previous;
    };
    const juce::File file;
    const double rate;
    const int channels, frames;
    const int64_t fileBytes, modified;
    const uint64_t identity;
    mutable std::atomic<bool> failure { false };
};
}
