#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <list>
#include <unordered_map>

namespace mixworks
{
// Display summaries only: immutable source identity plus exact sample interval.
// Successful reads survive transient PCM-pool contention/eviction. Pending reads
// are retried, never remembered as silence. No decoded blocks or media are owned.
class WaveformDetailCache
{
public:
    explicit WaveformDetailCache(size_t maximum = 16384) : capacity(std::max(size_t(1), maximum)) {}
    WaveformDetailCache(const WaveformDetailCache&) = delete;
    WaveformDetailCache& operator=(const WaveformDetailCache&) = delete;
    template<class Read>
    bool query(uint64_t source, int first, int last, float& peak, float& rms, Read&& read)
    {
        const Key key {source,first,last};
        if (auto found=entries.find(key); found!=entries.end())
        {
            recent.splice(recent.begin(),recent,found->second.position);
            peak=found->second.peak; rms=found->second.rms; return true;
        }
        peak=rms=0;
        float nextPeak=0,nextRms=0;
        if (!read(nextPeak,nextRms)) return false;
        if (entries.size()==capacity) { entries.erase(recent.back()); recent.pop_back(); }
        recent.push_front(key);
        entries.emplace(key,Value{nextPeak,nextRms,recent.begin()});
        peak=nextPeak; rms=nextRms; return true;
    }
private:
    struct Key
    {
        uint64_t source; int first,last;
        bool operator==(const Key& other) const { return source==other.source && first==other.first && last==other.last; }
    };
    struct Hash
    {
        size_t operator()(const Key& key) const
        { return std::hash<uint64_t>{}(key.source) ^ (std::hash<int>{}(key.first)<<1) ^ (std::hash<int>{}(key.last)<<2); }
    };
    struct Value { float peak,rms; std::list<Key>::iterator position; };
    size_t capacity;
    std::list<Key> recent;
    std::unordered_map<Key,Value,Hash> entries;
};
}
