#include "LibraryCatalogue.h"
#include "SourceSectionStore.h"
#include <juce_cryptography/juce_cryptography.h>
#include <set>
#include <map>
#include <winsqlite/winsqlite3.h>

namespace mixworks
{
namespace
{
constexpr int64_t maxCatalogueBytes = 128 * 1024 * 1024; // Bounded snapshot input, including waveform arrays and embedded 64px covers.
juce::var snapshotJson(const Media& media)
{
    auto result = juce::var(new juce::DynamicObject()); auto* object = result.getDynamicObject();
    object->setProperty("path", media.file.getFullPathName()); object->setProperty("id", media.id); object->setProperty("name", media.name);
    object->setProperty("modified", (juce::int64) media.sourceModified); object->setProperty("record", analysisRecordToJson(media.analysisRecord(), true));
    object->setProperty("sectionTemplate",sourceSectionsToJson(media.sectionTemplate));
    object->setProperty("analysisModified", (juce::int64) media.analysisFileModified); object->setProperty("analysisBytes", (juce::int64) media.analysisFileBytes);
    object->setProperty("title", media.assets.title); object->setProperty("artist", media.assets.artist); object->setProperty("album", media.assets.album);
    if (media.assets.genreRead) object->setProperty("genre", media.assets.genre);
    object->setProperty("coverSource", media.assets.coverSource == juce::File() ? juce::String() : media.assets.coverSource.getFullPathName()); object->setProperty("audioThumbnail", media.assets.audioThumbnail);
    juce::MemoryOutputStream png;
    if (media.assets.cover.isValid() && media.assets.cover.getWidth() <= 64 && media.assets.cover.getHeight() <= 64)
    { juce::PNGImageFormat format; if (format.writeImageToStream(media.assets.cover, png)) object->setProperty("coverPng", juce::Base64::toBase64(png.getData(), png.getDataSize())); }
    return result;
}
std::shared_ptr<Media> parseSnapshot(const juce::var& value)
{
    if (!value.isObject() || !value["path"].isString() || !juce::File::isAbsolutePath(value["path"].toString())
        || !value["id"].isString() || value["id"].toString().isEmpty() || value["id"].toString().length() > 512 || !value["name"].isString()
        || (!value["modified"].isInt64() && !value["modified"].isInt())) return {};
    auto record = parseAnalysisRecord(value["record"]); if (!record.record || !record.validAutomatic) return {};
    auto media = std::make_shared<Media>(); media->file = juce::File(value["path"].toString()); media->id = value["id"].toString(); media->name = value["name"].toString();
    media->sourceModified = (int64_t) value["modified"]; media->analysisSource = record.record->source; media->sampleRate = media->analysisSource.sampleRate;
    for (auto key : {"analysisModified", "analysisBytes"}) if (value.hasProperty(key) && !value[key].isInt64() && !value[key].isInt()) return {};
    media->analysisFileModified = (int64_t) value["analysisModified"]; media->analysisFileBytes = (int64_t) value["analysisBytes"];
    media->analysis = std::move(record.record->analysis); media->peaks = std::move(record.record->peaks); media->waveformRms = std::move(record.record->waveformRms);
    media->analysisVersion = record.record->revision;
    media->energy = std::move(record.record->energy);
    if (!record.reusableAutomatic) media->analysisCacheWarning = "Saved beat analysis awaits a detector refresh; playback checks it before use.";
    media->manualBpm = record.record->manualBpm; media->manualBeatOrigin = record.record->manualBeatOrigin; media->manualKey = record.record->manualKey; media->analysisFromCache = true;
    if (value.hasProperty("sectionTemplate") && readSourceSectionsJson(value["sectionTemplate"],media->duration(),media->sectionTemplate).failed()) return {};
    for (auto key : {"title", "artist", "album", "coverSource"}) if (!value[key].isString() || value[key].toString().length() > 32768) return {};
    media->assets.title = value["title"].toString(); media->assets.artist = value["artist"].toString(); media->assets.album = value["album"].toString();
    if (value.hasProperty("genre"))
    {
        if (!value["genre"].isString() || value["genre"].toString().length() > 2048) return {};
        media->assets.genre = value["genre"].toString(); media->assets.genreRead = true;
    }
    const auto coverSource = value["coverSource"].toString(); if (coverSource.isNotEmpty()) { if (!juce::File::isAbsolutePath(coverSource)) return {}; media->assets.coverSource = juce::File(coverSource); }
    if (!value["audioThumbnail"].isBool()) return {}; media->assets.audioThumbnail = (bool) value["audioThumbnail"];
    if (value.hasProperty("coverPng"))
    {
        if (!value["coverPng"].isString() || value["coverPng"].toString().length() > 32768) return {};
        juce::MemoryOutputStream decoded; if (!juce::Base64::convertFromBase64(decoded, value["coverPng"].toString()) || decoded.getDataSize() < 24) return {};
        const auto& png = decoded.getMemoryBlock();
        juce::MemoryInputStream header(png, false); const unsigned char signature[] {137, 80, 78, 71, 13, 10, 26, 10};
        if (std::memcmp(png.getData(), signature, 8) != 0) return {};
        header.setPosition(16); const auto width = header.readIntBigEndian(), height = header.readIntBigEndian();
        if (width < 1 || width > 64 || height < 1 || height > 64) return {}; // Check expansion before the image decoder allocates.
        media->assets.cover = juce::ImageFileFormat::loadFrom(png.getData(), png.getSize()); if (!media->assets.cover.isValid()) return {};
    }
    media->assets.prepareArtworkGrouping();
    return media;
}
bool representableJson(const juce::MemoryBlock& bytes)
{
    // JUCE strings terminate at NUL. Reject raw/escaped NUL before JSON conversion can lose a suffix.
    const auto* data = static_cast<const unsigned char*>(bytes.getData()); bool quoted = false;
    for (size_t i = 0; i < bytes.getSize(); ++i)
    {
        if (data[i] == 0) return false;
        if (data[i] == '"') quoted = !quoted;
        else if (quoted && data[i] == '\\' && i + 1 < bytes.getSize())
        {
            if (data[i + 1] == 'u' && i + 5 < bytes.getSize() && data[i + 2] == '0' && data[i + 3] == '0' && data[i + 4] == '0' && data[i + 5] == '0') return false;
            ++i; // Skip an escaped quote/backslash, rather than treating it as a delimiter/escape.
        }
    }
    return true;
}

constexpr int storeId = 0x4d574c42; // MWLB
constexpr int maxTrackSnapshotBytes = 2 * 1024 * 1024;
using Database = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>;
using Statement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;

bool execute(sqlite3* db, const char* sql) { return sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK; }
Statement prepare(sqlite3* db, const char* sql)
{
    sqlite3_stmt* statement = nullptr;
    sqlite3_prepare_v2(db, sql, -1, &statement, nullptr);
    return Statement(statement, sqlite3_finalize);
}
int integer(sqlite3* db, const char* sql)
{
    auto statement = prepare(db, sql);
    return statement && sqlite3_step(statement.get()) == SQLITE_ROW ? sqlite3_column_int(statement.get(), 0) : -1;
}
bool bind(sqlite3_stmt* statement, int column, const juce::String& value)
{
    return sqlite3_bind_text(statement, column, value.toRawUTF8(), (int) value.getNumBytesAsUTF8(), SQLITE_TRANSIENT) == SQLITE_OK;
}
bool done(sqlite3_stmt* statement)
{
    const auto status = sqlite3_step(statement);
    sqlite3_reset(statement);
    sqlite3_clear_bindings(statement);
    return status == SQLITE_DONE;
}
juce::String columnText(sqlite3_stmt* statement, int column)
{
    const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(statement, column));
    const int size = sqlite3_column_bytes(statement, column);
    if (text == nullptr || size == 0 || std::memchr(text, 0, (size_t) size) != nullptr
        || !juce::CharPointer_UTF8::isValidString(text, size)) return {};
    return juce::String::fromUTF8(text, size);
}

struct StoreWriter
{
    Statement path, snapshot;
    explicit StoreWriter(sqlite3* db)
        : path(prepare(db, "INSERT OR IGNORE INTO tracks (key, path) VALUES (?, ?)")),
          snapshot(prepare(db, "INSERT OR REPLACE INTO snapshots (key, data) VALUES (?, ?)")) {}

    bool putPath(const juce::File& file)
    {
        const auto name = file.getFullPathName();
        return path && bind(path.get(), 1, name.toLowerCase()) && bind(path.get(), 2, name) && done(path.get());
    }
    bool putSnapshot(const Media& media)
    {
        if (media.sourceDetailsPending || media.analysisSource.sha256.isEmpty()) return true;
        const auto document = snapshotJson(media);
        if (!parseAnalysisRecord(document["record"]).validAutomatic) return true;
        const auto text = juce::JSON::toString(document, true, 17);
        return text.getNumBytesAsUTF8() <= maxTrackSnapshotBytes && snapshot
            && bind(snapshot.get(), 1, media.file.getFullPathName().toLowerCase())
            && bind(snapshot.get(), 2, text) && done(snapshot.get());
    }
};

Database connectStore(const juce::File& legacyFile)
{
    const auto file = libraryStoreFile(legacyFile);
    sqlite3* raw = nullptr;
    if (file.isSymbolicLink() || file.getSiblingFile(file.getFileName() + "-journal").isSymbolicLink()
        || file.getParentDirectory().createDirectory().failed()) return {nullptr, sqlite3_close};
    const auto opened = sqlite3_open_v2(file.getFullPathName().toRawUTF8(), &raw, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    Database db(raw, sqlite3_close);
    if (opened != SQLITE_OK) return {nullptr, sqlite3_close};
    sqlite3_busy_timeout(db.get(), 3000);
    return db;
}

juce::String initialiseStore(sqlite3* db, const juce::File& legacyFile, CatalogueResult* fallback = nullptr)
{
    const auto busy = "The saved Library is busy. Alloy will retry pending saves.";
    const auto migrationError = "The Library migration could not be committed. The original catalogue was kept.";
    if (integer(db, "PRAGMA application_id") == storeId && integer(db, "PRAGMA user_version") == 1) return {};
    if (!execute(db, "BEGIN IMMEDIATE")) return busy;
    const auto id = integer(db, "PRAGMA application_id"), version = integer(db, "PRAGMA user_version");
    if (id == storeId && version == 1)
    {
        execute(db, "ROLLBACK");
        return {};
    }
    if (id != 0 || version != 0 || integer(db, "SELECT count(*) FROM sqlite_master") != 0)
    {
        execute(db, "ROLLBACK");
        return "The Library database is unreadable or uses an unsupported format. It was kept.";
    }
    auto legacy = readLibraryCatalogue(legacyFile);
    if (legacy.error.isNotEmpty())
    {
        execute(db, "ROLLBACK");
        return legacy.error;
    }
    const bool schema = execute(db, "CREATE TABLE tracks (key TEXT PRIMARY KEY, path TEXT NOT NULL) WITHOUT ROWID;"
                                    "CREATE TABLE snapshots (key TEXT PRIMARY KEY, data TEXT NOT NULL) WITHOUT ROWID;");
    bool written = schema;
    if (schema)
    {
        StoreWriter writer(db);
        for (const auto& file : legacy.files) if (written) written = writer.putPath(file);
        for (const auto& media : legacy.media) if (written) written = writer.putSnapshot(*media);
    }
    const auto finish = "PRAGMA application_id=" + juce::String(storeId) + "; PRAGMA user_version=1; COMMIT";
    if (written && execute(db, finish.toRawUTF8()))
    {
        legacy.document = juce::var();
        if (fallback != nullptr) *fallback = std::move(legacy);
        return {};
    }
    execute(db, "ROLLBACK");
    if (fallback != nullptr) *fallback = std::move(legacy);
    return migrationError;
}
}
juce::File libraryCatalogueFile() { return juce::File::getSpecialLocation(juce::File::windowsLocalAppData).getChildFile("Mixworks/library.json"); }
CatalogueResult readLibraryCatalogue(const juce::File& file)
{
    CatalogueResult result;
    if (!file.exists()) return result;
    const auto fail = [&]() { result.files.clear(); result.media.clear(); result.error = "The saved library catalogue is unreadable, malformed or belongs to another version; it was kept: " + file.getFullPathName(); return result; };
    if (file.isSymbolicLink() || !file.existsAsFile() || file.getSize() > maxCatalogueBytes) return fail();
    auto input = file.createInputStream(); if (!input) return fail();
    juce::MemoryBlock bytes; input->readIntoMemoryBlock(bytes, maxCatalogueBytes + 1);
    if (input->getStatus().failed() || bytes.getSize() > maxCatalogueBytes || !representableJson(bytes) || !juce::CharPointer_UTF8::isValidString((const char*) bytes.getData(), (int) bytes.getSize())) return fail();
    result.document = juce::JSON::parse(juce::String::fromUTF8((const char*) bytes.getData(), (int) bytes.getSize()));
    const auto& doc = result.document;
    if (!doc.isObject() || !doc["format"].isString() || doc["format"].toString() != "mixworks-library" || !doc["version"].isInt() || (int) doc["version"] != 1 || !doc["files"].isArray()) return fail();
    std::set<juce::String> seen;
    for (const auto& path : *doc["files"].getArray())
    {
        if (!path.isString() || path.toString().isEmpty() || !juce::File::isAbsolutePath(path.toString())) return fail();
        juce::File source(path.toString()); if (seen.insert(source.getFullPathName().toLowerCase()).second) result.files.add(source);
    }
    if (doc.hasProperty("snapshots"))
    {
        if (!doc["snapshots"].isArray()) return fail();
        std::set<juce::String> paths, ids;
        for (const auto& value : *doc["snapshots"].getArray())
            if (auto media = parseSnapshot(value))
                if (seen.count(media->file.getFullPathName().toLowerCase()) && paths.insert(media->file.getFullPathName().toLowerCase()).second && ids.insert(media->id).second)
                    result.media.push_back(std::move(media));
    }
    return result;
}
juce::String rememberLibraryCatalogue(const juce::File& file, const juce::Array<juce::File>& additions, const std::vector<std::shared_ptr<Media>>& snapshots)
{
    if (file == juce::File() || additions.isEmpty()) return {};
    const auto name = file.getFullPathName().toLowerCase(); juce::InterProcessLock mutex("mixworks-library-" + juce::SHA256(name.toRawUTF8(), name.getNumBytesAsUTF8()).toHexString());
    if (!mutex.enter(2000)) return "The library catalogue is busy in another Mixworks instance.";
    struct Unlock { juce::InterProcessLock& mutex; ~Unlock() { mutex.exit(); } } unlock {mutex};
    auto current = readLibraryCatalogue(file); if (current.error.isNotEmpty()) return current.error;
    bool changed = !file.exists(); std::set<juce::String> seen;
    for (const auto& source : current.files) seen.insert(source.getFullPathName().toLowerCase());
    for (const auto& source : additions) if (source != juce::File() && seen.insert(source.getFullPathName().toLowerCase()).second) { current.files.add(source); changed = true; }
    if (!changed && snapshots.empty()) return {};
    if (!current.document.isObject()) { current.document = juce::var(new juce::DynamicObject()); current.document.getDynamicObject()->setProperty("format", "mixworks-library"); current.document.getDynamicObject()->setProperty("version", 1); }
    juce::Array<juce::var> paths; for (const auto& source : current.files) paths.add(source.getFullPathName());
    current.document.getDynamicObject()->setProperty("files", paths);
    std::map<juce::String, juce::var> entries;
    if (const auto* previous = current.document["snapshots"].getArray()) for (const auto& entry : *previous) entries[entry["path"].toString().toLowerCase()] = entry;
    for (const auto& media : snapshots) if (media && seen.count(media->file.getFullPathName().toLowerCase()))
    {
        const auto value = snapshotJson(*media);
        if (parseAnalysisRecord(value["record"]).validAutomatic) entries[media->file.getFullPathName().toLowerCase()] = value;
    }
    if (!entries.empty()) { juce::Array<juce::var> values; for (const auto& entry : entries) values.add(entry.second); current.document.getDynamicObject()->setProperty("snapshots", values); }
    const auto text = juce::JSON::toString(current.document, true, 17);
    if (text.getNumBytesAsUTF8() > maxCatalogueBytes) return "The library snapshot exceeds its 128 MiB storage budget; existing data was kept.";
    if (file.existsAsFile() && file.loadFileAsString() == text) return {};
    if (file.getParentDirectory().createDirectory().failed()) return "Cannot create the saved library folder.";
    if (file.existsAsFile())
    {
        // One previous snapshot, not another full catalogue on every scan save.
        const auto backup = file.getSiblingFile(file.getFileName() + ".previous");
        if (backup.isSymbolicLink()) return "Linked library backups are unsupported.";
        juce::TemporaryFile previous(backup);
        if (!file.copyFileTo(previous.getFile()) || !previous.overwriteTargetFileWithTemporary()) return "Cannot back up the library catalogue.";
    }
    juce::TemporaryFile temporary(file);
    if (!temporary.getFile().replaceWithText(text, false, false, "\n") || !temporary.overwriteTargetFileWithTemporary()) return "Cannot save the library catalogue; the previous catalogue was kept.";
    return {};
}

juce::File libraryStoreFile(const juce::File& legacyFile) { return legacyFile.withFileExtension(".sqlite3"); }

CatalogueResult openLibraryStore(const juce::File& legacyFile)
{
    CatalogueResult result;
    auto db = connectStore(legacyFile);
    if (!db)
    {
        result = readLibraryCatalogue(legacyFile);
        if (result.error.isEmpty()) result.error = "The Library database could not be opened.";
        return result;
    }
    result.error = initialiseStore(db.get(), legacyFile, &result);
    if (result.error.isNotEmpty() || !result.files.isEmpty()) return result;
    auto rows = prepare(db.get(), "SELECT tracks.path, CASE WHEN length(CAST(snapshots.data AS BLOB)) <= 2097152 THEN snapshots.data END"
                                  " FROM tracks LEFT JOIN snapshots USING (key)");
    if (rows)
    {
        std::set<juce::String> ids;
        int status = SQLITE_OK;
        while ((status = sqlite3_step(rows.get())) == SQLITE_ROW)
        {
            const auto path = columnText(rows.get(), 0);
            if (!juce::File::isAbsolutePath(path)) break;
            const juce::File source(path);
            result.files.add(source);
            if (const auto snapshot = columnText(rows.get(), 1); snapshot.isNotEmpty())
                if (auto media = parseSnapshot(juce::JSON::parse(snapshot)); media && media->file == source && ids.insert(media->id).second)
                    result.media.push_back(std::move(media));
        }
        if (status == SQLITE_DONE) return result;
    }
    result.error = "The Library database could not be read. It was kept.";
    return result;
}

juce::String saveLibraryStore(const juce::File& legacyFile, const std::vector<std::shared_ptr<Media>>& changes)
{
    if (changes.empty()) return {};
    auto db = connectStore(legacyFile);
    if (!db) return "The Library database could not be opened.";
    if (auto error = initialiseStore(db.get(), legacyFile); error.isNotEmpty()) return error;
    if (!execute(db.get(), "BEGIN IMMEDIATE")) return "The saved Library is busy. Alloy will retry pending saves.";
    StoreWriter writer(db.get());
    for (const auto& media : changes)
        if (!writer.putPath(media->file) || !writer.putSnapshot(*media))
            return "The Library update could not be written. Alloy will retry pending saves.";
    if (!execute(db.get(), "COMMIT")) return "The Library update could not be committed. Alloy will retry pending saves.";
    return {};
}
}
