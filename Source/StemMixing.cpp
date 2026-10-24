#include "Model.h"
#include "ContentDigest.h"
#include <cmath>

namespace mixworks
{
juce::Result loadCachedStemAssets(const Media& parent, const juce::File& root, std::shared_ptr<const StemAssets>& output)
{
    // Read completed sets only. Never start a separation worker or trust a
    // catalogue path as proof of source identity. Full validation stays shared
    // with explicit attachment and session reopen.
    if (!root.isDirectory()) return juce::Result::ok();
    for (auto at = root; at != at.getParentDirectory(); at = at.getParentDirectory())
        if (at.isSymbolicLink()) return juce::Result::fail("Linked stem cache is unsupported.");
    const auto sourceHash = audioDigest(parent.file);
    if (sourceHash.length() != 64 || (parent.analysisSource.sha256.isNotEmpty() && sourceHash != parent.analysisSource.sha256))
        return juce::Result::fail("Source changed; previous stems were not restored.");
    auto directories = root.findChildFiles(juce::File::findDirectories, false);
    std::sort(directories.begin(), directories.end(), [](const auto& a, const auto& b)
    { const auto ta = a.getLastModificationTime(), tb = b.getLastModificationTime(); return ta == tb ? a.getFullPathName() < b.getFullPathName() : ta > tb; });
    juce::String error;
    for (const auto& directory : directories)
    {
        const auto key = directory.getFileName();
        if (key.length() != 64 || !key.containsOnly("0123456789abcdef") || directory.isSymbolicLink()) continue;
        const auto manifest = directory.getChildFile("manifest.json");
        if (!manifest.existsAsFile() || manifest.isSymbolicLink() || manifest.getSize() > 65536) continue;
        const auto doc = juce::JSON::parse(manifest);
        if (doc["sourceSha256"].toString() != sourceHash) continue;
        const auto loaded = loadStemAssets(parent, directory, {}, output);
        if (loaded.wasOk()) return loaded;
        error = loaded.getErrorMessage();
    }
    return error.isEmpty() ? juce::Result::ok() : juce::Result::fail(error);
}
juce::Result loadStemAssets(const Media& parent, const juce::File& directory, const juce::String& expected, std::shared_ptr<const StemAssets>& output)
{
    auto fail = [](const juce::String& text) { return juce::Result::fail("Stems: " + text + " The current mix was kept."); };
    auto hash = [](const juce::File& f) { auto in = f.createInputStream(); return in ? contentDigest(*in) : juce::String(); };
    for (auto at = directory; at != at.getParentDirectory(); at = at.getParentDirectory())
        if (at.isSymbolicLink()) return fail("Linked asset directories are unsupported.");
    const auto file = directory.getChildFile("manifest.json");
    if (!file.existsAsFile() || file.isSymbolicLink() || file.getSize() > 65536) return fail("Missing or invalid manifest.");
    const auto identity = hash(file); const auto doc = juce::JSON::parse(file);
    if ((!expected.isEmpty() && expected != identity) || (int) doc["schema"] != 1 || (int) doc["offsetSamples"] != 0
        || (int) doc["frames"] != parent.frames() || (int) doc["channels"] != parent.channels()
        || (parent.analysisSource.sha256.isNotEmpty() && parent.analysisSource.sha256 != doc["sourceSha256"].toString())
        || (double) doc["sampleRate"] != parent.sampleRate || doc["sourceSha256"].toString() != hash(parent.file))
        return fail("Source identity or sample clock changed.");
    if (!parent.hasPlayback() || parent.frames() <= 0 || parent.channels() < 1 || parent.channels() > 2
        || (int64_t) parent.frames() * parent.channels() * 4 > 512LL * 1024 * 1024) return fail("Unsupported source dimensions.");
    auto assets = std::make_shared<StemAssets>(); assets->directory = directory; assets->manifestHash = identity;
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    for (int s = 0; s < stemCount; ++s)
    {
        const auto path = directory.getChildFile(juce::String(stemNames[s]) + ".wav");
        const auto digest = hash(path);
        if (path.isSymbolicLink() || digest.length() != 64 || digest != doc["stems"][stemNames[s]]["sha256"].toString()) return fail("Missing or changed " + juce::String(stemNames[s]) + " audio.");
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(path));
        if (!reader || reader->lengthInSamples != parent.frames() || reader->sampleRate != parent.sampleRate || (int) reader->numChannels != parent.channels()) return fail("Stem dimensions do not match the original.");
        auto media = std::make_shared<Media>(); media->file = path; media->name = stemNames[s]; media->sampleRate = parent.sampleRate;
        media->samples.setSize(parent.channels(), parent.frames());
        if (!reader->read(&media->samples, 0, parent.frames(), 0, true, true)) return fail("Cannot read stem audio.");
        const int bucket = std::max(1, media->frames() / 4000);
        for (int first = 0; first < media->frames(); first += bucket)
        {
            float peak = 0;
            for (int ch = 0; ch < media->channels(); ++ch) for (int i = first; i < std::min(first + bucket, media->frames()); ++i)
            {
                const float value = media->samples.getSample(ch, i);
                if (!std::isfinite(value)) return fail("Invalid stem sample.");
                peak = std::max(peak, std::abs(value));
            }
            media->peaks.push_back(peak);
        }
        media->analysis = parent.analysis; media->manualBpm = parent.manualBpm; media->manualBeatOrigin = parent.manualBeatOrigin; media->manualKey = parent.manualKey;
        media->analysisSource = {digest, path.getSize(), media->sampleRate, media->frames(), media->channels()};
        media->prepareWaveformBody();
        juce::String error; media->cache = AudioCache::create(media->samples, error);
        if (!media->cache) return fail("Cannot map stem playback: " + error);
        media->samples.setSize(0, 0); assets->sources[(size_t) s] = std::move(media);
    }
    if (identity != hash(file) || doc["sourceSha256"].toString() != hash(parent.file)) return fail("Source changed while loading.");
    output = std::move(assets); return juce::Result::ok();
}
}
