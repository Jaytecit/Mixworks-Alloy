#include "AnalysisCache.h"
#include "ContentDigest.h"
#include <juce_cryptography/juce_cryptography.h>
#include <cmath>

namespace mixworks
{
namespace
{
constexpr int cacheVersion = 1;
constexpr int64_t maxRecordBytes = 1024 * 1024; // At most 8000 waveform buckets and three detector windows.
const char* recordFormat = "mixworks-track-analysis";
struct RecordDocument { juce::String text; juce::var value; };
std::optional<RecordDocument> readRecordDocument(const juce::File& file)
{
    auto input = file.createInputStream(); if (!input) return {};
    juce::MemoryBlock data; input->readIntoMemoryBlock(data, maxRecordBytes + 1);
    if (data.getSize() > (size_t) maxRecordBytes || input->getStatus().failed()) return {};
    auto text = juce::String::fromUTF8((const char*) data.getData(), (int) data.getSize());
    return RecordDocument {text, juce::JSON::parse(text)};
}
bool number(const juce::var& value, double low, double high)
{
    return (value.isDouble() || value.isInt() || value.isInt64()) && std::isfinite((double) value) && (double) value >= low && (double) value <= high;
}
bool key(const juce::var& value) { return value.isInt() && number(value, -1, 23); }
bool recordPathMatches(const juce::var& value, const juce::File& audio)
{
    return value.isString() && juce::File::isAbsolutePath(value.toString()) && juce::File(value.toString()) == audio;
}
juce::var identityJson(const SourceIdentity& s)
{
    auto* object = new juce::DynamicObject(); object->setProperty("sha256", s.sha256); object->setProperty("bytes", (juce::int64) s.bytes);
    object->setProperty("sampleRate", s.sampleRate); object->setProperty("frames", s.frames); object->setProperty("channels", s.channels); return juce::var(object);
}
}
const juce::File& analysisStoreRoot(const juce::File& initialRoot)
{
    static const auto root = initialRoot == juce::File()
        ? juce::File::getSpecialLocation(juce::File::windowsLocalAppData).getChildFile("Mixworks/Analysis-v1") : initialRoot;
    return root;
}
juce::File analysisRecordFile(const juce::File& audio)
{
    auto path = audio.getFullPathName();
#if JUCE_WINDOWS
    path = path.toLowerCase();
#endif
    return analysisStoreRoot().getChildFile(juce::SHA256(path.toRawUTF8(), path.getNumBytesAsUTF8()).toHexString() + ".json");
}
juce::File legacyAnalysisRecordFile(const juce::File& audio) { return audio.getSiblingFile(audio.getFileName() + ".mixworks-analysis.json"); }
juce::File analysisRecordInputFile(const juce::File& audio)
{
    const auto current = analysisRecordFile(audio);
    return current.exists() ? current : legacyAnalysisRecordFile(audio);
}
juce::String audioDigest(const juce::File& audio)
{
    auto input = audio.createInputStream(); if (!input) return {};
    // Keep file reads buffered for both the native and portable digest backends.
    juce::BufferedInputStream buffered(*input, 256 * 1024);
    const auto digest = contentDigest(buffered);
    return input->getStatus().wasOk() ? digest : juce::String();
}
AnalysisRecordRead readAnalysisRecord(const juce::File& audio, const SourceIdentity& source)
{
    const auto file = analysisRecordInputFile(audio);
    if (!file.existsAsFile() || file.getSize() > maxRecordBytes || file.isSymbolicLink()) return {};
    const auto document = readRecordDocument(file); if (!document) return {};
    auto parsed = parseAnalysisRecord(document->value, source);
    parsed.legacyLocation = file == legacyAnalysisRecordFile(audio); return parsed;
}
AnalysisRecordRead parseAnalysisRecord(const juce::var& root, const std::optional<SourceIdentity>& expected)
{
    const auto identity = root["source"], a = root["analysis"], manual = root["manual"];
    if (!identity.isObject() || !identity["sha256"].isString() || identity["sha256"].toString().length() != 64
        || !identity["sha256"].toString().containsOnly("0123456789abcdef") || !number(identity["bytes"], 1, 1.0e15)
        || !number(identity["sampleRate"], 1, 1.0e9) || !identity["frames"].isInt() || !number(identity["frames"], 1, 2147483646)
        || !identity["channels"].isInt() || !number(identity["channels"], 1, 2)) return {};
    SourceIdentity source {identity["sha256"].toString(), (int64_t) identity["bytes"], (double) identity["sampleRate"], (int) identity["frames"], (int) identity["channels"]};
    if ((int64_t) source.frames * source.channels * sizeof(float) > 1024LL * 1024 * 1024) return {};
    if (expected && !(source == *expected)) return {};
    if (!root.isObject() || root["format"].toString() != recordFormat || !root["version"].isInt() || (int) root["version"] != cacheVersion) return {};
    const double duration = source.frames / source.sampleRate;
    if (!manual.isObject() || !number(manual["bpm"], 0, 400) || ((double) manual["bpm"] > 0 && (double) manual["bpm"] < 20)
        || !number(manual["beatOrigin"], -1, duration) || ((double) manual["beatOrigin"] < 0 && (double) manual["beatOrigin"] != -1) || !key(manual["key"])) return {};
    TrackAnalysisRecord record; record.source = source; record.manualBpm = (double) manual["bpm"]; record.manualBeatOrigin = (double) manual["beatOrigin"]; record.manualKey = (int) manual["key"];
    record.revision = root["analysisRevision"].isInt() ? (int) root["analysisRevision"] : 0;
    auto refresh = [&]() -> AnalysisRecordRead { return {record, false}; };
    if (!number(a["downbeatOrigin"], -1, duration) || ((double) a["downbeatOrigin"] < 0 && (double) a["downbeatOrigin"] != -1)
        || !number(a["downbeatConfidence"], 0, 1)) return refresh();
    if ((record.revision >= 4 || a.hasProperty("downbeatExaminedSeconds"))
        && !number(a["downbeatExaminedSeconds"], 0, duration)) return refresh();
    if (!a.isObject() || !number(a["bpm"], 0, 400) || ((double) a["bpm"] > 0 && (double) a["bpm"] < 20) || !number(a["beatOrigin"], 0, duration)
        || !number(a["tempoConfidence"], 0, 1) || !number(a["gridErrorMs"], 0, duration * 1000) || !key(a["key"]) || !key(a["alternativeKey"])
        || !number(a["keyConfidence"], 0, 1) || !number(a["tuningCents"], -50.0001, 50.0001) || !number(a["examinedSeconds"], 0, std::min(180.0, duration) + 0.01)
        || !a["variableTempo"].isBool() || !a["beatWindows"].isArray() || a["beatWindows"].size() > maxAnalysisWindows) return refresh();
    const int bucket = std::max(1, source.frames / 4000), expectedPeaks = (source.frames + bucket - 1) / bucket;
    auto& result = record.analysis;
    if (record.revision >= 7)
    {
        if (!a["rhythmMethod"].isString() || !a["rhythmIssue"].isString() || a["rhythmIssue"].toString().length() > 4096) return refresh();
        result.rhythmMethod = a["rhythmMethod"].toString(); result.rhythmIssue = a["rhythmIssue"].toString();
        if (!number(a["neuralPhaseCorrectionMs"], -1500.0001, 1500.0001)) return refresh();
        result.neuralPhaseCorrectionMs = (double) a["neuralPhaseCorrectionMs"];
        if (!number(a["neuralBpm"], 0, 400) || !number(a["signalBpm"], 0, 400) || !number(a["signalTempoConfidence"], 0, 1) || !a["signalGridNeedsReview"].isBool()) return refresh();
        result.neuralBpm = (double) a["neuralBpm"]; result.signalBpm = (double) a["signalBpm"];
        result.signalTempoConfidence = (double) a["signalTempoConfidence"]; result.signalGridNeedsReview = (bool) a["signalGridNeedsReview"];
        if (!a["downbeatMethod"].isString()) return refresh();
        result.downbeatMethod = a["downbeatMethod"].toString();
        if (result.downbeatMethod != "unknown" && result.downbeatMethod != "signal-structure" && result.downbeatMethod != "signal-accent"
            && result.downbeatMethod != "signal-conflict" && result.downbeatMethod != "beat-this-final0") return refresh();
        if (result.downbeatMethod == "signal-conflict" && (double) a["downbeatOrigin"] >= 0) return refresh();
        if (result.rhythmMethod != "dsp" && result.rhythmMethod != "beat-this-final0" && result.rhythmMethod != "unavailable") return refresh();
        if (result.rhythmIssue.isNotEmpty() || result.rhythmMethod == "unavailable") return refresh();
    }
    result.bpm = (double) a["bpm"]; result.beatOrigin = (double) a["beatOrigin"]; result.tempoConfidence = (double) a["tempoConfidence"]; result.gridErrorMs = (double) a["gridErrorMs"];
    result.downbeatOrigin = (double) a["downbeatOrigin"]; result.downbeatConfidence = (double) a["downbeatConfidence"];
    result.downbeatExaminedSeconds = a.hasProperty("downbeatExaminedSeconds") ? (double) a["downbeatExaminedSeconds"] : 0;
    result.key = (int) a["key"]; result.alternativeKey = (int) a["alternativeKey"]; result.keyConfidence = (double) a["keyConfidence"];
    result.tuningCents = (double) a["tuningCents"]; result.examinedSeconds = (double) a["examinedSeconds"]; result.variableTempo = (bool) a["variableTempo"];
    double previous = -1;
    for (const auto& w : *a["beatWindows"].getArray())
    {
        if (!w.isObject() || !number(w["sourceStart"], 0, duration) || (double) w["sourceStart"] <= previous || !number(w["bpm"], 0, 400)
            || !number(w["origin"], -3, duration + 3) || !number(w["confidence"], 0, 1)) return refresh();
        previous = (double) w["sourceStart"]; result.beatWindows.push_back({previous, (double) w["bpm"], (double) w["origin"], (double) w["confidence"]});
    }
    auto waveform = [&](const char* name, std::vector<float>& values)
    {
        const juce::Identifier packedName(juce::String(name) + "F32");
        if (root.hasProperty(packedName))
        {
            const auto value = root[packedName]; if (root.hasProperty(name) || !value.isString() || value.toString().length() > ((expectedPeaks * 4 + 2) / 3) * 4) return false;
            juce::MemoryOutputStream decoded;
            if (!juce::Base64::convertFromBase64(decoded, value.toString()) || decoded.getDataSize() != (size_t) expectedPeaks * 4) return false;
            juce::MemoryInputStream input(decoded.getData(), decoded.getDataSize(), false);
            for (int i = 0; i < expectedPeaks; ++i) { const auto sample = input.readFloat(); if (!std::isfinite(sample) || sample < 0) return false; values.push_back(sample); }
        }
        else
        {
            const auto* array = root[name].getArray(); if (!array || array->size() != expectedPeaks) return false;
            for (const auto& value : *array) { if (!number(value, 0, std::numeric_limits<float>::max())) return false; values.push_back((float) value); }
        }
        return true;
    };
    if (!waveform("peaks", record.peaks)) return refresh();
    if ((root.hasProperty("waveformRms") || root.hasProperty("waveformRmsF32")) && !waveform("waveformRms", record.waveformRms)) return refresh();
    const bool current = record.revision == analysisRevision;
    return {std::move(record), current, true};
}
juce::var analysisRecordToJson(const TrackAnalysisRecord& record, bool compactWaveforms)
{
    auto* root = new juce::DynamicObject(); juce::var document(root);
    root->setProperty("format", recordFormat); root->setProperty("version", cacheVersion); root->setProperty("analysisRevision", record.revision); root->setProperty("source", identityJson(record.source));
    const auto& a = record.analysis; auto* analysis = new juce::DynamicObject();
    analysis->setProperty("bpm", a.bpm); analysis->setProperty("beatOrigin", a.beatOrigin); analysis->setProperty("tempoConfidence", a.tempoConfidence); analysis->setProperty("gridErrorMs", a.gridErrorMs);
    analysis->setProperty("downbeatOrigin", a.downbeatOrigin); analysis->setProperty("downbeatConfidence", a.downbeatConfidence);
    analysis->setProperty("downbeatExaminedSeconds", a.downbeatExaminedSeconds);
    analysis->setProperty("key", a.key); analysis->setProperty("alternativeKey", a.alternativeKey); analysis->setProperty("keyConfidence", a.keyConfidence); analysis->setProperty("tuningCents", a.tuningCents);
    analysis->setProperty("examinedSeconds", a.examinedSeconds); analysis->setProperty("variableTempo", a.variableTempo);
    analysis->setProperty("rhythmMethod", a.rhythmMethod); analysis->setProperty("rhythmIssue", a.rhythmIssue);
    analysis->setProperty("neuralPhaseCorrectionMs", a.neuralPhaseCorrectionMs);
    analysis->setProperty("neuralBpm", a.neuralBpm); analysis->setProperty("signalBpm", a.signalBpm);
    analysis->setProperty("signalTempoConfidence", a.signalTempoConfidence); analysis->setProperty("signalGridNeedsReview", a.signalGridNeedsReview);
    analysis->setProperty("downbeatMethod", a.downbeatMethod);
    juce::Array<juce::var> windows;
    for (const auto& w : a.beatWindows) { auto* object = new juce::DynamicObject(); object->setProperty("sourceStart", w.sourceStart); object->setProperty("bpm", w.bpm); object->setProperty("origin", w.origin); object->setProperty("confidence", w.confidence); windows.add(juce::var(object)); }
    auto waveform = [&](const char* name, const std::vector<float>& values)
    {
        if (compactWaveforms) { juce::MemoryOutputStream data; for (auto value : values) data.writeFloat(value); root->setProperty(juce::String(name) + "F32", juce::Base64::toBase64(data.getData(), data.getDataSize())); }
        else { juce::Array<juce::var> array; for (auto value : values) array.add(value); root->setProperty(name, array); }
    };
    waveform("peaks", record.peaks);
    analysis->setProperty("beatWindows", windows); root->setProperty("analysis", juce::var(analysis));
    auto* manual = new juce::DynamicObject(); manual->setProperty("bpm", record.manualBpm); manual->setProperty("beatOrigin", record.manualBeatOrigin); manual->setProperty("key", record.manualKey); root->setProperty("manual", juce::var(manual));
    if (!record.waveformRms.empty()) waveform("waveformRms", record.waveformRms);
    return document;
}
juce::String writeAnalysisRecord(const juce::File& audio, const TrackAnalysisRecord& record)
{
    if (record.source.sha256.isEmpty()) return "Cannot verify this audio's identity; its analysis could not be saved.";
    const auto digest = audioDigest(audio);
    if (digest.isEmpty()) return "Cannot read the audio to verify its analysis record.";
    if (digest != record.source.sha256) return "Audio changed while analysis was being saved; import it again to refresh its record.";
    const auto file = analysisRecordFile(audio);
    juce::InterProcessLock mutex("mixworks-analysis-" + file.getFileNameWithoutExtension());
    if (!mutex.enter(2000)) return "Analysis storage is busy in another Mixworks instance.";
    struct Unlock { juce::InterProcessLock& lock; ~Unlock() { lock.exit(); } } unlock {mutex};
    if (analysisStoreRoot().isSymbolicLink() || analysisStoreRoot().createDirectory().failed()) return "Cannot create the Mixworks analysis folder.";
    std::optional<RecordDocument> previous;
    if (file.isSymbolicLink() || file.isDirectory()) return "The analysis record path is a link or directory; it was left unchanged.";
    if (file.existsAsFile())
    {
        if (file.getSize() > maxRecordBytes) return "The existing analysis record is too large to validate; it was left unchanged.";
        previous = readRecordDocument(file); if (!previous) return "Cannot validate the existing analysis record; it was left unchanged.";
        const auto& existing = previous->value;
        if (!existing.isObject() || existing["format"].toString() != recordFormat) return "The existing analysis record cannot be identified as a Mixworks record; it was left unchanged.";
        if (!existing["version"].isInt() || (int) existing["version"] > cacheVersion) return "The analysis record belongs to a newer Mixworks build; it was left unchanged.";
        if (existing.hasProperty("sourcePath") && !recordPathMatches(existing["sourcePath"], audio)) return "The analysis record belongs to another source path; it was left unchanged.";
        if (existing["analysisRevision"].isInt() && (int) existing["analysisRevision"] > record.revision) return "A newer detector's analysis was kept; this build will not downgrade it.";
    }
    else if (const auto legacy = readRecordDocument(legacyAnalysisRecordFile(audio)))
    {
        if (legacy->value["format"].toString() == recordFormat && legacy->value["analysisRevision"].isInt()
            && (int) legacy->value["analysisRevision"] > record.revision) return "A newer detector's legacy analysis was kept; this build will not downgrade it.";
    }
    const auto document = analysisRecordToJson(record, true);
    document.getDynamicObject()->setProperty("sourcePath", audio.getFullPathName());
    const auto text = juce::JSON::toString(document, false, 17);
    if (text.getNumBytesAsUTF8() > maxRecordBytes) return "The analysis record exceeds its storage budget.";
    if (previous && juce::JSON::toString(previous->value, false, 17) == text) return {};
    if (file.existsAsFile())
    {
        const auto backup = file.getSiblingFile(file.getFileName() + ".bak");
        if (backup.exists())
        {
            const auto old = readRecordDocument(backup);
            if (backup.isSymbolicLink() || !old || old->value["format"].toString() != recordFormat
                || !recordPathMatches(old->value["sourcePath"], audio)) return "An unidentified analysis backup was kept; the analysis record was not replaced.";
        }
        juce::TemporaryFile savedBackup(backup);
        if (!savedBackup.getFile().replaceWithText(previous->text, false, false, "\n") || !savedBackup.overwriteTargetFileWithTemporary())
            return "Could not retain the previous analysis record; the existing record was kept.";
    }
    juce::TemporaryFile temporary(file);
    if (!temporary.getFile().replaceWithText(text, false, false, "\n") || !temporary.overwriteTargetFileWithTemporary()) return "Cannot save analysis in the Mixworks data folder; playback remains available.";
    return {};
}
juce::String migrateLegacyAnalysisRecord(const juce::File& audio)
{
    // Migration never removes the legacy file and never replaces an existing central record.
    if (analysisRecordFile(audio).exists()) return "Central record already exists; kept unchanged.";
    const auto file = legacyAnalysisRecordFile(audio);
    if (file.isSymbolicLink() || !file.existsAsFile() || file.getSize() > maxRecordBytes) return "Legacy analysis is unavailable or oversized.";
    const auto doc = readRecordDocument(file); if (!doc) return "Cannot read legacy analysis.";
    auto parsed = parseAnalysisRecord(doc->value);
    if (!parsed.record || !parsed.validAutomatic) return "Legacy analysis is not a valid supported record; kept unchanged.";
    return writeAnalysisRecord(audio, *parsed.record);
}
}
