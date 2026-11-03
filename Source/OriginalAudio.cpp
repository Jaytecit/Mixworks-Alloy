#include "OriginalAudio.h"
#include "WaveformDetailCache.h"
#include <array>
#include <condition_variable>
#include <mutex>
#include <chrono>

namespace mixworks
{
namespace
{
thread_local bool realtime = false;
std::atomic<uint64_t> identities { 1 };
struct Block
{
    uint64_t owner = 0, age = 0;
    int index = -1;
    bool ready = false, loading = false;
    juce::AudioBuffer<float> pcm { 2, OriginalAudio::blockFrames };
};
// Admission pins protect every required block for this callback. Sampling an
// admitted range never contends with the decoder's bookkeeping lock.
thread_local std::array<std::shared_ptr<Block>, 64> admitted;
thread_local std::array<std::shared_ptr<Block>, 64> previousAdmission;
thread_local size_t admittedCount = 0;
thread_local size_t previousCount = 0;
void beginAdmission()
{
    admitted.swap(previousAdmission);
    previousCount = admittedCount; admittedCount = 0;
}
void endAdmission()
{
    while (previousCount) previousAdmission[--previousCount].reset();
}
class Pool final : public juce::Thread
{
public:
    Pool() : Thread("Original music read ahead")
    {
        for (auto& slot : slots) slot = std::make_shared<Block>();
        requests.reserve(256); formats.registerBasicFormats();
        startThread(juce::Thread::Priority::normal);
    }
    ~Pool() override { signalThreadShouldExit(); notify(); stopThread(-1); }
    struct Request { std::shared_ptr<const OriginalAudio> source; int index; };
    struct Reader { std::shared_ptr<const OriginalAudio> source; std::unique_ptr<juce::AudioFormatReader> reader; uint64_t age = 0; };
    std::array<std::shared_ptr<Block>, OriginalAudio::poolBlocks> slots;
    std::array<Reader, 8> readers;
    std::vector<Request> requests;
    juce::AudioFormatManager formats;
    std::mutex mutex;
    std::condition_variable changed;
    uint64_t clock = 0;
    std::atomic<uint64_t> reads {0}, misses {0}, queued {0}, epoch {1};

    std::shared_ptr<Block> find(const OriginalAudio& source, int index)
    {
        for (auto& slot : slots) if (slot->ready && slot->owner == source.identity && slot->index == index)
        { slot->age = ++clock; return slot; }
        return {};
    }
    void enqueue(const OriginalAudio& source, int index)
    {
        if (source.failed()) return;
        for (const auto& slot : slots) if (slot->loading && slot->owner == source.identity && slot->index == index) return;
        for (const auto& request : requests) if (request.source->identity == source.identity && request.index == index) return;
        if (requests.size() < requests.capacity()) { requests.push_back({source.shared_from_this(), index}); ++queued; notify(); }
    }
    std::shared_ptr<Block> get(const OriginalAudio& source, int index, bool wait, bool tryLock = false)
    {
        std::unique_lock<std::mutex> guard(mutex, std::defer_lock);
        if (realtime || tryLock) { if (!guard.try_lock()) return {}; } else guard.lock();
        auto block = find(source, index);
        if (block || source.failed()) return block;
        enqueue(source, index);
        if (!wait) return {};
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
        while (!source.failed() && !threadShouldExit())
        {
            if (changed.wait_until(guard, deadline) == std::cv_status::timeout) { source.failure = true; break; }
            if ((block = find(source, index))) return block;
            enqueue(source, index);
        }
        return {};
    }
    void run() override
    {
        while (!threadShouldExit())
        {
            Request task; std::shared_ptr<Block> block;
            {
                std::lock_guard<std::mutex> guard(mutex);
                if (!requests.empty())
                {
                    for (auto& slot : slots) if (slot.use_count() == 1 && !slot->loading && (!block || slot->age < block->age)) block = slot;
                    if (block)
                    {
                        task = requests.front(); requests.erase(requests.begin());
                        block->ready = false; block->loading = true; block->owner = task.source->identity; block->index = task.index;
                    }
                }
            }
            if (!block) { wait(5); continue; }
            const auto& source = *task.source;
            auto found = std::find_if(readers.begin(), readers.end(), [&](const auto& entry) { return entry.source && entry.source->identity == source.identity; });
            if (found == readers.end())
            {
                found = std::min_element(readers.begin(), readers.end(), [](const auto& a, const auto& b) { return a.age < b.age; });
                found->reader.reset(); found->source = task.source;
                found->reader.reset(formats.createReaderFor(source.file));
            }
            found->age = ++readerClock;
            bool ok = !source.failed() && source.file.getSize() == source.fileBytes
                && source.file.getLastModificationTime().toMilliseconds() == source.modified && found->reader
                && found->reader->sampleRate == source.rate && found->reader->lengthInSamples == source.frames
                && (int)std::min(found->reader->numChannels, 2u) == source.channels;
            block->pcm.clear();
            // Some compressed readers report failure when asked past EOF.
            // The final pool block is already zeroed; decode only valid frames.
            const int64_t first = int64_t(task.index) * OriginalAudio::blockFrames;
            const int count = (int)std::min<int64_t>(OriginalAudio::blockFrames, source.frames - first);
            if (ok) ok = count > 0 && found->reader->read(&block->pcm, 0, count,
                                                         first, true, source.channels > 1);
            if (ok) for (int ch = 0; ch < source.channels; ++ch)
                for (int i = 0; i < OriginalAudio::blockFrames; ++i)
                    if (!std::isfinite(block->pcm.getSample(ch, i))) { ok = false; break; }
            if (!ok) { source.failure = true; block->pcm.clear(); }
            {
                std::lock_guard<std::mutex> guard(mutex);
                block->loading = false; block->ready = ok; block->age = ++clock;
                ++reads; ++epoch;
            }
            changed.notify_all();
        }
    }
    uint64_t readerClock = 0;
};
Pool& pool() { static Pool instance; return instance; }
}
OriginalAudio::OriginalAudio(juce::File source, double sr, int ch, int count)
    : file(std::move(source)), rate(sr), channels(ch), frames(count), fileBytes(file.getSize()),
      modified(file.getLastModificationTime().toMilliseconds()), identity(identities.fetch_add(1)) { (void)pool(); }
bool OriginalAudio::isRealtime() { return realtime; }
OriginalAudio::Realtime::Realtime() : previous(realtime) { if (!previous) beginAdmission(); realtime = true; }
OriginalAudio::Realtime::~Realtime() { if (!previous) endAdmission(); realtime = previous; }
float OriginalAudio::sample(int channel, int frame) const
{
    if (frame < 0 || frame >= frames || channel < 0 || channel >= channels || failed()) return 0;
    const int index = frame / blockFrames;
    if (realtime) for (size_t i = 0; i < admittedCount; ++i)
        if (admitted[i]->owner == identity && admitted[i]->index == index)
            return admitted[i]->pcm.getSample(channel, frame % blockFrames);
    struct Recent { uint64_t owner = 0; int index = -1; std::shared_ptr<Block> block; };
    // Bounded pins keep immutable blocks safe during worker eviction, including
    // overlapping sources/stems and a simultaneous offline render.
    thread_local std::array<Recent, 8> recent;
    thread_local size_t next = 0;
    for (const auto& item : recent) if (item.owner == identity && item.index == index && item.block)
        return item.block->pcm.getSample(channel, frame % blockFrames);
    auto block = pool().get(*this, index, !realtime);
    if (!block) { if (realtime) ++pool().misses; return 0; }
    auto& item = recent[next++ % recent.size()]; item = {identity, index, block};
    return block->pcm.getSample(channel, frame % blockFrames);
}
bool OriginalAudio::request(int first, int last) const
{
    if (failed()) return false;
    first = juce::jlimit(0, frames, first); last = juce::jlimit(first, frames, last);
    bool ready = true;
    for (int index = first / blockFrames; index <= (last - 1) / blockFrames && last > first; ++index)
    {
        bool pinned = false;
        if (realtime) for (size_t i = 0; i < admittedCount; ++i)
            if (admitted[i]->owner == identity && admitted[i]->index == index) { pinned = true; break; }
        if (pinned) continue;
        std::shared_ptr<Block> block;
        if (realtime) for (size_t i = 0; i < previousCount; ++i)
            if (previousAdmission[i]->owner == identity && previousAdmission[i]->index == index) { block = previousAdmission[i]; break; }
        if (!block) block = pool().get(*this, index, false);
        if (!block) ready = false;
        else if (realtime)
        {
            if (admittedCount == admitted.size()) ready = false;
            else admitted[admittedCount++] = std::move(block);
        }
    }
    return ready;
}
bool OriginalAudio::waveformRange(int first, int last, float& peak, float& rms) const
{
    peak = rms = 0;
    first = juce::jlimit(0, frames, first); last = juce::jlimit(first, frames, last);
    if (first == last) return true;
    if (failed() || realtime) return false; // Never a render/callback operation.
    // A repaint must not turn a previously measured column into silence just
    // because the playback pool is momentarily locked or has recycled its PCM.
    // Cache only the exact display measurement, independently of audio ownership.
    static thread_local WaveformDetailCache display;
    return display.query(identity,first,last,peak,rms,[&](float& measuredPeak,float& measuredRms)
    {
        float maximum = 0; double energy = 0;
        for (int start = first; start < last;)
        {
            const int index = start / blockFrames;
            const auto block = pool().get(*this, index, false, true);
            if (!block) return false;
            const int finish = (int)std::min<int64_t>(last, (int64_t(index) + 1) * blockFrames);
            for (int ch = 0; ch < channels; ++ch)
                for (int i = start; i < finish; ++i)
                {
                    const float value = block->pcm.getSample(ch, i % blockFrames);
                    maximum = std::max(maximum, std::abs(value)); energy += double(value) * value;
                }
            start = finish;
        }
        measuredPeak = maximum; measuredRms = (float)std::sqrt(energy / ((last - first) * double(channels)));
        return true;
    });
}
OriginalAudio::Statistics OriginalAudio::statistics()
{ auto& p = pool(); return {p.reads.load(), p.misses.load(), p.queued.load(), poolBytes}; }
}
