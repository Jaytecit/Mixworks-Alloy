#include "Recovery.h"
#include <juce_cryptography/juce_cryptography.h>
#include <mutex>
#include <set>

namespace mixworks
{
namespace
{
constexpr juce::int64 recoveryBudget = 128 * 1024 * 1024;
std::mutex localMutex;
std::set<juce::String> heldFolders;
juce::String digest(const juce::String& s) { return juce::SHA256(s.toRawUTF8(), s.getNumBytesAsUTF8()).toHexString(); }
juce::Result boundedText(const juce::File& f, juce::String& text)
{
    if (f.isSymbolicLink() || !f.existsAsFile() || f.getSize() > recoveryBudget) return juce::Result::fail("Recovery data is missing, linked or too large; it was kept.");
    auto stream = f.createInputStream(); if (!stream) return juce::Result::fail("Cannot read recovery data; it was kept.");
    juce::MemoryBlock bytes; stream->readIntoMemoryBlock(bytes, recoveryBudget + 1);
    const auto* data = (const char*) bytes.getData();
    if (stream->getStatus().failed() || bytes.getSize() == 0 || bytes.getSize() > (size_t) recoveryBudget
        || std::memchr(data, 0, bytes.getSize()) || !juce::CharPointer_UTF8::isValidString(data, (int) bytes.getSize()))
        return juce::Result::fail("Invalid recovery encoding or size; data was kept.");
    text = juce::String::fromUTF8(data, (int) bytes.getSize()); return juce::Result::ok();
}
bool identified(const juce::File& folder)
{
    const auto id = folder.getFileName();
    if (folder.isSymbolicLink() || !folder.isDirectory() || juce::Uuid(id).isNull() || juce::Uuid(id).toString() != id) return false;
    const auto owner = folder.getChildFile("owner.json");
    if (owner.isSymbolicLink() || owner.getSize() > 1024) return false;
    const auto v = juce::JSON::parse(owner);
    return v.isObject() && v["format"].toString() == "mixworks-recovery-owner" && v["version"].isInt() && (int) v["version"] == 1 && v["instance"].toString() == id;
}
juce::Result atomicText(const juce::File& file, const juce::String& text)
{
    if (file.isSymbolicLink()) return juce::Result::fail("Recovery destination is linked; it was kept.");
    juce::TemporaryFile temporary(file);
    if (!temporary.getFile().replaceWithText(text, false, false, "\n") || !temporary.overwriteTargetFileWithTemporary()) return juce::Result::fail("Cannot commit recovery snapshot. The previous snapshot was kept.");
    return juce::Result::ok();
}
juce::Result decode(const juce::String& text, const juce::String& id, RecoveryCandidate& c)
{
    auto v = juce::JSON::parse(text);
    if (!v.isObject() || v["format"].toString() != "mixworks-recovery" || !v["version"].isInt() || (int) v["version"] != 1
        || v["instance"].toString() != id || !v["document"].isString() || !v["sessionPath"].isString() || !v["savedAt"].isString()
        || v["sha256"].toString() != digest(v["document"].toString())) return juce::Result::fail("Unidentified or damaged recovery snapshot; data was kept.");
    const auto path = v["sessionPath"].toString();
    if (path.isNotEmpty() && !juce::File::isAbsolutePath(path)) return juce::Result::fail("Recovery session path is invalid; data was kept.");
    c.document = v["document"].toString(); const auto parsed = parseProject(c.document);
    if (parsed.error.isNotEmpty()) return juce::Result::fail(parsed.error);
    c.sessionFile = path.isEmpty() ? juce::File() : juce::File(path); c.timestamp = v["savedAt"].toString(); return juce::Result::ok();
}
}
RecoveryStore::RecoveryStore(juce::File f) : folder(std::move(f)), id(folder.getFileName()), lease("mixworks-recovery-" + digest(folder.getFullPathName().toLowerCase())) {}
RecoveryStore::~RecoveryStore()
{
    if (held) { std::lock_guard<std::mutex> guard(localMutex); heldFolders.erase(folder.getFullPathName().toLowerCase()); lease.exit(); }
}
std::unique_ptr<RecoveryStore> RecoveryStore::claim(const juce::File& folder, juce::String& error)
{
    if (!identified(folder)) { error = "Unidentified recovery folder was kept."; return {}; }
    auto store = std::unique_ptr<RecoveryStore>(new RecoveryStore(folder));
    std::lock_guard<std::mutex> guard(localMutex);
    const auto key = folder.getFullPathName().toLowerCase();
    if (heldFolders.count(key) || !store->lease.enter(0)) { error = "Recovery belongs to a running window."; return {}; }
    store->held = true; heldFolders.insert(key); return store;
}
std::unique_ptr<RecoveryStore> RecoveryStore::create(const juce::File& root, juce::String& error)
{
    if (root.isSymbolicLink()) { error = "Recovery root is linked."; return {}; }
    auto folder = root.getChildFile(juce::Uuid().toString());
    auto result = folder.createDirectory(); if (result.failed()) { error = result.getErrorMessage(); return {}; }
    auto* owner = new juce::DynamicObject(); owner->setProperty("format", "mixworks-recovery-owner"); owner->setProperty("version", 1); owner->setProperty("instance", folder.getFileName());
    result = atomicText(folder.getChildFile("owner.json"), juce::JSON::toString(juce::var(owner)));
    if (result.failed()) { error = result.getErrorMessage(); return {}; }
    return claim(folder, error);
}
RecoveryCandidate RecoveryStore::read() const
{
    RecoveryCandidate c; c.folder = folder; juce::String current, previous;
    const auto a = folder.getChildFile("current.json"), b = folder.getChildFile("previous.json");
    const auto ar = boundedText(a, current), br = boundedText(b, previous);
    c.fingerprint = digest(current + "\n--previous--\n" + previous);
    auto valid = ar.wasOk() ? decode(current, id, c) : ar;
    if (valid.wasOk()) return c;
    if (br.wasOk() && decode(previous, id, c).wasOk()) { c.previous = true; return c; }
    c.document.clear(); c.error = valid.getErrorMessage(); return c;
}
juce::Result RecoveryStore::write(const juce::String& document, const juce::File& sessionFile)
{
    if (!held || !identified(folder)) return juce::Result::fail("Recovery ownership changed; data was kept.");
    // JSON string escaping can expand each input byte to six output bytes.
    // Reserve envelope space before allocation, rather than checking only after encoding.
    if (document.getNumBytesAsUTF8() > (recoveryBudget - 4096) / 6) return juce::Result::fail("Recovery document exceeds its bounded encoding budget (21 MiB).");
    const auto parsed = parseProject(document); if (parsed.error.isNotEmpty()) return juce::Result::fail(parsed.error);
    auto* object = new juce::DynamicObject(); object->setProperty("format", "mixworks-recovery"); object->setProperty("version", 1); object->setProperty("instance", id);
    object->setProperty("savedAt", juce::Time::getCurrentTime().toISO8601(true)); object->setProperty("sessionPath", sessionFile == juce::File() ? juce::String() : sessionFile.getFullPathName());
    object->setProperty("document", document); object->setProperty("sha256", digest(document));
    const auto text = juce::JSON::toString(juce::var(object), false, 17);
    if (text.getNumBytesAsUTF8() > recoveryBudget) return juce::Result::fail("Encoded recovery exceeds its 128 MiB budget.");
    const auto current = folder.getChildFile("current.json");
    if (current.exists())
    {
        juce::String old; RecoveryCandidate c; auto result = boundedText(current, old); if (result.failed()) return result;
        result = decode(old, id, c); if (result.failed()) return result;
        if (c.document == document && c.sessionFile == sessionFile) return juce::Result::ok();
        result = atomicText(folder.getChildFile("previous.json"), old); if (result.failed()) return result;
    }
    return atomicText(current, text);
}
juce::Result RecoveryStore::discard(const juce::String& expectedFingerprint)
{
    if (!held || !identified(folder)) return juce::Result::fail("Recovery ownership changed; data was kept.");
    if (expectedFingerprint.isNotEmpty() && read().fingerprint != expectedFingerprint) return juce::Result::fail("Recovery changed since it was offered; data was kept.");
    for (const auto* name : {"current.json", "previous.json"})
    {
        const auto file = folder.getChildFile(name);
        if (file.isSymbolicLink() || (file.exists() && !file.deleteFile())) return juce::Result::fail("Cannot remove recovery data; ownership was retained.");
    }
    return juce::Result::ok(); // Keep the small owner record; never recursively remove unknown files.
}
std::vector<RecoveryCandidate> RecoveryStore::scan(const juce::File& root)
{
    std::vector<RecoveryCandidate> candidates; if (root.isSymbolicLink()) return candidates;
    for (const auto& entry : juce::RangedDirectoryIterator(root, false, "*", juce::File::findDirectories, juce::File::FollowSymlinks::no))
    {
        juce::String error; auto store = claim(entry.getFile(), error); if (!store) continue;
        if (!store->directory().getChildFile("current.json").exists() && !store->directory().getChildFile("previous.json").exists()) continue;
        candidates.push_back(store->read());
    }
    std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) { return a.timestamp > b.timestamp; }); return candidates;
}
}
