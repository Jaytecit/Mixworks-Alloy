#include "PcmCache.h"
#include "ContentDigest.h"
#include <juce_cryptography/juce_cryptography.h>
#include <cmath>
#include <cstring>
#include <limits>

namespace mixworks
{
namespace
{
constexpr char magic[] = "MWPCM01!";
const juce::String ownerText = "Mixworks PCM cache namespace version 1\n";
bool digestName(const juce::String& key) { return key.length() == 64 && key.containsOnly("0123456789abcdef"); }
juce::String cacheLockName(const juce::File& root)
{
    const auto path = root.getFullPathName(); return "mixworks-pcm-" + juce::SHA256(path.toRawUTF8(), path.getNumBytesAsUTF8()).toHexString();
}
}
AudioCache::~AudioCache()
{
    mapping.reset();
    if (transient && file.existsAsFile() && !file.deleteFile()) juce::Logger::writeToLog("Mixworks could not remove its PCM cache: " + file.getFullPathName());
}
std::unique_ptr<AudioCache> AudioCache::create(const juce::AudioBuffer<float>& audio, juce::String& error)
{
    auto result = std::make_unique<AudioCache>();
    const auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("Mixworks-PCM");
    if (folder.createDirectory().failed()) { error = "Cannot create the Mixworks audio cache folder."; return {}; }
    result->file = folder.getChildFile(juce::Uuid().toString() + ".pcm");
    result->channels = audio.getNumChannels(); result->frames = audio.getNumSamples();
    auto stream = result->file.createOutputStream();
    if (!stream) { error = "Cannot create a decoded audio cache."; return {}; }
    const auto bytesPerChannel = (size_t) result->frames * sizeof(float);
    for (int ch = 0; ch < result->channels; ++ch)
        if (!stream->write(audio.getReadPointer(ch), bytesPerChannel)) { error = "Cannot write decoded audio cache. Check free disk space."; return {}; }
    stream->flush(); const auto status = stream->getStatus(); stream.reset();
    if (status.failed()) { error = status.getErrorMessage(); return {}; }
    result->mapping = std::make_unique<juce::MemoryMappedFile>(result->file, juce::MemoryMappedFile::readOnly);
    if (!result->mapping->getData() || result->mapping->getSize() != bytesPerChannel * result->channels) { error = "Cannot map the decoded audio cache."; return {}; }
    return result;
}
std::unique_ptr<AudioCache> AudioCache::decode(juce::AudioFormatReader& reader, int channels, int frames, juce::String& error,
                                            const std::shared_ptr<std::atomic<bool>>& cancelled, const juce::File& requestedFolder)
{
    error.clear();
    if (channels < 1 || channels > 2 || (unsigned) channels > reader.numChannels || frames < 1 || frames > reader.lengthInSamples
        || !std::isfinite(reader.sampleRate) || reader.sampleRate <= 0)
    { error = "Unsupported mapped decode dimensions."; return {}; }
    auto stop = [&] { if (cancelled && *cancelled) { error = "Audio decoding cancelled."; return true; } return false; };
    if (stop()) return {};
    const auto folder = requestedFolder == juce::File() ? juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("Mixworks-PCM") : requestedFolder;
    if (folder.createDirectory().failed()) { error = "Cannot create the mapped audio folder."; return {}; }
    const int64_t bytes = (int64_t) channels * frames * (int64_t) sizeof(float);
    const auto available = folder.getBytesFreeOnVolume();
    if (available < 0) { error = "Cannot establish available disk space for mapped decoding."; return {}; }
    if (available < bytes) { error = "Not enough disk space for decoded audio."; return {}; }
    auto result = std::make_unique<AudioCache>(); result->channels = channels; result->frames = frames;
    result->file = folder.getChildFile(juce::Uuid().toString() + ".pcm");
    auto output = result->file.createOutputStream();
    if (!output) { error = "Cannot create mapped decode output."; return {}; }
    constexpr int chunkFrames = 32768; // At most 256 KiB of stereo float decode scratch.
    juce::AudioBuffer<float> chunk(channels, std::min(chunkFrames, frames));
    for (int offset = 0; offset < frames;)
    {
        if (stop()) return {};
        const int count = std::min(chunkFrames, frames - offset);
        if (!reader.read(&chunk, 0, count, offset, true, true)) { error = "Audio read failed during mapped decoding."; return {}; }
        if (stop()) return {};
        for (int channel = 0; channel < channels; ++channel)
        {
            const auto* samples = chunk.getReadPointer(channel);
            for (int i = 0; i < count; ++i) if (!std::isfinite(samples[i])) { error = "Non-finite audio sample during mapped decoding."; return {}; }
            const auto position = ((int64_t) channel * frames + offset) * (int64_t) sizeof(float);
            if (!output->setPosition(position) || !output->write(samples, (size_t) count * sizeof(float)))
            { error = "Cannot write decoded audio. Check available disk space."; return {}; }
        }
        offset += count;
    }
    output->flush(); const auto status = output->getStatus(); output.reset();
    if (status.failed()) { error = status.getErrorMessage(); return {}; }
    if (stop()) return {};
    result->mapping = std::make_unique<juce::MemoryMappedFile>(result->file, juce::MemoryMappedFile::readOnly);
    if (!result->mapping->getData() || result->mapping->getSize() != (size_t) bytes)
    { error = "Cannot map the completed decoded audio."; return {}; }
    return result;
}
juce::File PcmStore::defaultRoot()
{
    return juce::File::getSpecialLocation(juce::File::windowsLocalAppData).getChildFile("Mixworks/AudioCache-v1");
}
PcmStore& PcmStore::shared(const juce::File& initialRoot) { static PcmStore store(initialRoot); return store; }
PcmStore::PcmStore(juce::File directory, int64_t limit) : root(std::move(directory)), budget(limit)
{
    juce::InterProcessLock mutex(cacheLockName(root));
    if (!mutex.enter(2000)) { problem = "Playback cache initialization is busy in another Mixworks instance."; return; }
    struct Unlock { juce::InterProcessLock& lock; ~Unlock() { lock.exit(); } } unlock {mutex};
    if (root.isSymbolicLink()) { problem = "Playback cache folder is a link; cache writes are disabled."; return; }
    const auto marker = root.getChildFile(".mixworks-cache");
    if (root.exists() && (!marker.existsAsFile() || marker.isSymbolicLink() || marker.getSize() != (int64_t) ownerText.getNumBytesAsUTF8() || marker.loadFileAsString() != ownerText))
    { problem = "Playback cache folder is not identified as owned by this version of Mixworks."; return; }
    if (!root.exists())
    {
        if (root.createDirectory().failed() || !marker.replaceWithData(ownerText.toRawUTF8(), ownerText.getNumBytesAsUTF8())) problem = "Cannot create the persistent playback cache.";
    }
}
juce::String PcmStore::decodedKey(const SourceIdentity& source)
{
    if (!digestName(source.sha256) || source.frames < 1 || source.channels < 1 || source.channels > 2) return {};
    juce::MemoryOutputStream stream;
    stream.writeString("Mixworks decoded planar F32LE / JUCE 8.0.16-security / revision 1"); stream.writeString(source.sha256);
    stream.writeInt64(source.bytes); stream.writeDouble(source.sampleRate); stream.writeInt(source.frames); stream.writeInt(source.channels);
    return juce::SHA256(stream.getData(), stream.getDataSize()).toHexString();
}
bool PcmStore::ownedEntry(const juce::File& file) const
{
    if (file.getParentDirectory() != root || file.isSymbolicLink() || !file.hasFileExtension("mwpcm") || !digestName(file.getFileNameWithoutExtension())) return false;
    auto input = file.createInputStream(); char id[8] {};
    return input && input->read(id, 8) == 8 && std::memcmp(id, magic, 8) == 0 && input->readInt() == 1;
}
std::unique_ptr<AudioCache> PcmStore::read(const juce::String& key, double rate, int channels, int frames, bool touch)
{
    if (problem.isNotEmpty() || !digestName(key) || channels < 1 || channels > 2 || frames < 1 || !std::isfinite(rate) || rate <= 0) return {};
    const auto file = entryFile(key); const int64_t payload = (int64_t) channels * frames * sizeof(float);
    if (payload > 1024LL * 1024 * 1024 || file.isSymbolicLink() || file.getSize() != payload + headerBytes) return {};
    auto result = std::make_unique<AudioCache>(); result->transient = false; result->file = file;
    result->mapping = std::make_unique<juce::MemoryMappedFile>(file, juce::MemoryMappedFile::readOnly);
    if (!result->mapping->getData() || result->mapping->getSize() != (size_t) (payload + headerBytes)) return {};
    juce::MemoryInputStream input(result->mapping->getData(), headerBytes, false); char id[8] {}, storedKey[65] {}, hash[65] {};
    if (input.read(id, 8) != 8 || std::memcmp(id, magic, 8) != 0 || input.readInt() != 1 || input.readInt() != channels || input.readInt() != frames || input.readInt() != 0 || input.readDouble() != rate
        || input.read(storedKey, 64) != 64 || juce::String::fromUTF8(storedKey, 64) != key || input.read(hash, 64) != 64) return {};
    result->channels = channels; result->frames = frames; result->payloadOffset = headerBytes;
    juce::MemoryInputStream payloadInput(result->channelData(0), (size_t) payload, false);
    if (contentDigest(payloadInput) != juce::String::fromUTF8(hash, 64)) return {};
    for (int ch = 0; ch < channels; ++ch) for (int i = 0; i < frames; ++i) if (!std::isfinite(result->channelData(ch)[i])) return {};
    if (touch) file.setLastAccessTime(juce::Time::getCurrentTime());
    return result;
}
bool PcmStore::makeRoom(int64_t bytes, const juce::File& target)
{
    if (bytes > budget || budget <= 0) return false;
    auto files = root.findChildFiles(juce::File::findFiles, false);
    int64_t used = 0; for (const auto& file : files) used += file.getSize();
    std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) { return a.getLastAccessTime() < b.getLastAccessTime(); });
    for (const auto& file : files)
    {
        if (used + bytes <= budget) break;
        if (file == target || !ownedEntry(file)) continue;
        const auto size = file.getSize(); if (file.deleteFile()) used -= size; // Active Windows mappings remain owned and cannot be deleted.
    }
    return used + bytes <= budget;
}
std::unique_ptr<AudioCache> PcmStore::write(const juce::String& key, double rate, const juce::AudioBuffer<float>& audio, juce::String& warning)
{
    if (key.isEmpty()) return {};
    if (problem.isNotEmpty()) { warning = problem; return {}; }
    const int channels = audio.getNumChannels(), frames = audio.getNumSamples();
    if (!digestName(key) || channels < 1 || channels > 2 || frames < 1 || !std::isfinite(rate) || rate <= 0) { warning = "Invalid playback cache dimensions."; return {}; }
    const int64_t payload = (int64_t) frames * channels * sizeof(float);
    if (payload > 1024LL * 1024 * 1024) { warning = "Playback cache entry exceeds 1 GiB."; return {}; }
    for (int ch = 0; ch < channels; ++ch) for (int i = 0; i < frames; ++i)
        if (!std::isfinite(audio.getSample(ch, i))) { warning = "Non-finite playback PCM was not cached."; return {}; }
    juce::InterProcessLock mutex(cacheLockName(root));
    if (!mutex.enter(2000)) { warning = "Playback cache is busy in another Mixworks instance."; return {}; }
    struct Unlock { juce::InterProcessLock& lock; ~Unlock() { lock.exit(); } } unlock {mutex};
    if (auto existing = read(key, rate, channels, frames)) return existing;
    const auto target = entryFile(key);
    if (target.exists() && !ownedEntry(target)) { warning = "An unidentified playback cache entry was kept."; return {}; }
    if (target.exists() && !target.deleteFile()) { warning = "An invalid playback cache is still in use; this session will use temporary audio."; return {}; }
    if (!makeRoom(payload + headerBytes, target)) { warning = "Playback cache disk budget is full; this audio is available for this session only."; return {}; }
    // Hash channels incrementally through a stream without assembling a second PCM buffer.
    class Samples final : public juce::InputStream
    {
    public:
        explicit Samples(const juce::AudioBuffer<float>& b) : audio(b) {}
        int64_t getTotalLength() override { return (int64_t) audio.getNumChannels() * audio.getNumSamples() * sizeof(float); }
        int64_t getPosition() override { return position; }
        bool setPosition(int64_t p) override { position = juce::jlimit<int64_t>(0, getTotalLength(), p); return true; }
        bool isExhausted() override { return position == getTotalLength(); }
        int read(void* destination, int requested) override
        {
            const int64_t stride = (int64_t) audio.getNumSamples() * sizeof(float); int written = 0;
            while (written < requested && !isExhausted()) { const int ch = (int) (position / stride); const auto offset = position % stride; const int count = (int) std::min<int64_t>(requested - written, stride - offset);
                std::memcpy(static_cast<char*>(destination) + written, reinterpret_cast<const char*>(audio.getReadPointer(ch)) + offset, (size_t) count); position += count; written += count; }
            return written;
        }
    private: const juce::AudioBuffer<float>& audio; int64_t position = 0;
    } samples(audio);
    const auto hash = contentDigest(samples);
    if (hash.isEmpty()) { warning = "Cannot verify playback PCM for caching."; return {}; }
    juce::TemporaryFile temp(target); auto output = temp.getFile().createOutputStream();
    if (!output) { warning = "Cannot create playback cache output."; return {}; }
    output->write(magic, 8); output->writeInt(1); output->writeInt(channels); output->writeInt(frames); output->writeInt(0); output->writeDouble(rate);
    output->write(key.toRawUTF8(), 64); output->write(hash.toRawUTF8(), 64); output->writeRepeatedByte(0, headerBytes - 160);
    bool written = true; for (int ch = 0; ch < channels; ++ch) written &= output->write(audio.getReadPointer(ch), (size_t) frames * sizeof(float));
    output->flush(); const auto status = output->getStatus(); output.reset();
    if (!written || status.failed() || !temp.overwriteTargetFileWithTemporary()) { warning = "Cannot persist playback audio; using the session's temporary audio."; return {}; }
    auto cached = read(key, rate, channels, frames); if (!cached) warning = "Playback cache failed validation after writing.";
    return cached;
}
}
