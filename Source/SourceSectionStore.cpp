#include "SourceSectionStore.h"
#include <cmath>
#include <set>

namespace mixworks
{
namespace
{
constexpr int64_t maxBytes = 1024 * 1024;
bool identityValid(const SourceIdentity& source)
{
    return source.sha256.length() == 64 && source.sha256.containsOnly("0123456789abcdef") && source.bytes > 0
        && std::isfinite(source.sampleRate) && source.sampleRate >= 1000 && source.sampleRate <= 768000 && source.frames > 0 && source.channels > 0 && source.channels <= 64;
}
bool numeric(const juce::var& value)
{ return (value.isInt() || value.isInt64() || value.isDouble()) && std::isfinite((double) value); }
SourceSectionRead readFile(const juce::File& file, const SourceIdentity& source)
{
    SourceSectionRead result; result.exists = file.exists(); if (!result.exists) return result;
    if (file.isSymbolicLink() || file.isDirectory() || file.getSize() > maxBytes) { result.error = "The saved section record is linked, oversized or not a file; it was kept."; return result; }
    auto stream = file.createInputStream(); if (!stream) { result.error = "Cannot read saved source sections."; return result; }
    juce::MemoryBlock bytes; stream->readIntoMemoryBlock(bytes,maxBytes + 1);
    const auto doc = juce::JSON::parse(juce::String::fromUTF8((const char*) bytes.getData(),(int) bytes.getSize()));
    const auto& identity = doc["source"];
    if (bytes.getSize() > maxBytes || stream->getStatus().failed() || !doc.isObject() || doc["format"].toString() != "mixworks-source-sections"
        || !doc["version"].isInt() || ((int) doc["version"] < 1 || (int) doc["version"] > 2) || !identity.isObject() || !identity["sha256"].isString()
        || identity["sha256"].toString() != source.sha256 || (!identity["bytes"].isInt() && !identity["bytes"].isInt64()) || (int64_t) identity["bytes"] != source.bytes
        || !numeric(identity["sampleRate"]) || (double) identity["sampleRate"] != source.sampleRate
        || !identity["frames"].isInt() || (int) identity["frames"] != source.frames || !identity["channels"].isInt() || (int) identity["channels"] != source.channels
        || !doc.hasProperty("sections"))
    { result.error = "The source section record is malformed, incompatible or belongs to different audio; it was kept."; return result; }
    const auto parsed = readSourceSectionsJson(doc["sections"],source.frames / source.sampleRate,result.markers);
    if (parsed.failed()) result.error = parsed.getErrorMessage(); return result;
}
}
juce::var sourceSectionsToJson(const SourceSections& sections)
{
    if (!sections) return {};
    juce::Array<juce::var> array;
    for (const auto& section : *sections)
    {
        auto* item = new juce::DynamicObject(); item->setProperty("id",section.id); item->setProperty("sourceSeconds",section.sourceSeconds);
        item->setProperty("contrastDb",section.contrastDb); item->setProperty("manual",section.manual); item->setProperty("label",section.label); array.add(juce::var(item));
    }
    return array;
}
juce::Result readSourceSectionsJson(const juce::var& value, double duration, SourceSections& out)
{
    out.reset(); if (value.isVoid() || value.isUndefined()) return juce::Result::ok();
    const auto* array = value.getArray();
    if (!array || array->size() > 4096 || !std::isfinite(duration) || duration <= 0) return juce::Result::fail("Invalid saved source section list; it was kept.");
    std::vector<SourceSectionMarker> sections; std::set<juce::String> ids; double previous = 0;
    for (const auto& item : *array)
    {
        if (!item.isObject() || !item["id"].isString() || item["id"].toString().isEmpty() || item["id"].toString().length() > 64
            || !ids.insert(item["id"].toString()).second || !numeric(item["sourceSeconds"]) || (double) item["sourceSeconds"] <= previous
            || (double) item["sourceSeconds"] >= duration || !numeric(item["contrastDb"]) || std::abs((double) item["contrastDb"]) > 1000 || !item["manual"].isBool()
            || (item.hasProperty("label") && (!item["label"].isString() || item["label"].toString().length() > 128 || item["label"].toString().containsAnyOf("\r\n\t"))))
            return juce::Result::fail("Source section boundaries must have unique identities, valid provenance and ordered file positions; saved data was kept.");
        previous = (double) item["sourceSeconds"];
        sections.push_back({item["id"].toString(),previous,(double) item["contrastDb"],(bool) item["manual"],item["label"].toString()});
    }
    out = std::move(sections); return juce::Result::ok();
}
juce::File sourceSectionRecordFile(const SourceIdentity& source)
{
    if (!identityValid(source)) return {};
    return analysisStoreRoot().getChildFile("sections").getChildFile(source.sha256 + ".json");
}
SourceSectionRead readSourceSectionRecord(const SourceIdentity& source)
{
    if (!identityValid(source)) return {};
    const auto file = sourceSectionRecordFile(source);
    if (analysisStoreRoot().isSymbolicLink() || file.getParentDirectory().isSymbolicLink()) return {{},true,"Saved section storage is linked; it was kept."};
    return readFile(file,source);
}
juce::String writeSourceSectionRecord(const juce::File& audio, const SourceIdentity& source, int64_t modified,
                                     const SourceSections& expected, const SourceSections& replacement)
{
    if (!identityValid(source)) return "Wait for verified source audio before saving track section defaults.";
    SourceSections validated; const auto valid = readSourceSectionsJson(sourceSectionsToJson(replacement),source.frames / source.sampleRate,validated);
    if (valid.failed()) return valid.getErrorMessage();
    if (!audio.existsAsFile() || audio.getSize() != source.bytes || (modified > 0 && audio.getLastModificationTime().toMilliseconds() != modified))
        return "The source file changed or disappeared; section defaults were kept.";
    const auto file = sourceSectionRecordFile(source), directory = file.getParentDirectory();
    if (analysisStoreRoot().isSymbolicLink() || directory.isSymbolicLink() || analysisStoreRoot().createDirectory().failed() || directory.createDirectory().failed())
        return "Cannot create safe source section storage.";
    juce::InterProcessLock mutex("mixworks-sections-" + source.sha256);
    if (!mutex.enter(20)) return "Source sections are being saved in another window. Try the edit again.";
    struct Unlock { juce::InterProcessLock& mutex; ~Unlock() { mutex.exit(); } } unlock{mutex};
    const auto previous = readFile(file,source); if (previous.error.isNotEmpty()) return previous.error;
    if (previous.markers != expected) return "Saved track sections changed in another window. Reload the source before editing again.";
    if (previous.exists && previous.markers == replacement) return {};
    auto* doc = new juce::DynamicObject(); doc->setProperty("format","mixworks-source-sections"); doc->setProperty("version",2);
    auto* identity = new juce::DynamicObject(); identity->setProperty("sha256",source.sha256); identity->setProperty("bytes",(juce::int64) source.bytes);
    identity->setProperty("sampleRate",source.sampleRate); identity->setProperty("frames",source.frames); identity->setProperty("channels",source.channels);
    doc->setProperty("source",juce::var(identity)); doc->setProperty("sections",sourceSectionsToJson(replacement));
    const auto text = juce::JSON::toString(juce::var(doc),false,17);
    if (text.getNumBytesAsUTF8() > maxBytes) return "The source section record exceeds its storage budget.";
    if (previous.exists)
    {
        const auto backup = file.getSiblingFile(file.getFileName() + ".bak"); const auto oldBackup = readFile(backup,source);
        if (oldBackup.error.isNotEmpty()) return "An unidentified section backup was kept; the edit was not saved.";
        juce::TemporaryFile saved(backup);
        if (!saved.getFile().replaceWithText(file.loadFileAsString(),false,false,"\n") || !saved.overwriteTargetFileWithTemporary()) return "Could not retain the previous source sections; the edit was not saved.";
    }
    juce::TemporaryFile temporary(file);
    if (!temporary.getFile().replaceWithText(text,false,false,"\n") || !temporary.overwriteTargetFileWithTemporary()) return "Could not save track section defaults; the previous record was kept.";
    return {};
}
}
