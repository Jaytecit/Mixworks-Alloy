#include "Playlist.h"
#include <cmath>
#include <cstring>

namespace mixworks
{
PlaylistReadResult readPlaylist(const juce::File& file, const std::function<bool()>& cancelled)
{
    PlaylistReadResult result;
    auto fail = [&](const juce::String& message) { result.files.clear(); result.error = message; return result; };
    // Bound text expansion before allocating the line inventory (separate from decoded audio budgets).
    constexpr int maxBytes = 8 * 1024 * 1024;
    auto stream = file.createInputStream();
    if (!stream || stream->getTotalLength() > maxBytes) return fail("Cannot read playlist, or it exceeds the 8 MiB text budget: " + file.getFullPathName());
    juce::MemoryBlock bytes; stream->readIntoMemoryBlock(bytes, maxBytes + 1);
    if (stream->getStatus().failed()) return fail("Could not finish reading playlist: " + stream->getStatus().getErrorMessage());
    if (bytes.getSize() > maxBytes || bytes.getSize() == 0) return fail("Playlist is empty or exceeds the 8 MiB text budget.");
    const auto* raw = static_cast<const char*>(bytes.getData());
    if (!juce::CharPointer_UTF8::isValidString(raw, (int) bytes.getSize()) || std::memchr(raw, 0, bytes.getSize())) return fail("Playlist must be UTF-8 text without embedded NUL characters.");
    auto text = juce::String::fromUTF8(raw, (int) bytes.getSize());
    if (text.startsWithChar(0xfeff)) text = text.substring(1);
    const auto lines = juce::StringArray::fromLines(text);
    size_t expandedPathBytes = 0;
    for (int i = 0; i < lines.size(); ++i)
    {
        if (cancelled && cancelled()) return fail("Playlist import cancelled.");
        const auto& line = lines[i];
        auto lineError = [&](const juce::String& reason) { return fail("Playlist line " + juce::String(i + 1) + ": " + reason); };
        if (line.trim().isEmpty()) continue;
        if (line.startsWithIgnoreCase("#EXT-X-")) return lineError("HLS streaming playlists are not supported. Use a local audio playlist.");
        if (line.startsWithChar('#')) continue;
        // Preserve literal spaces and percent escapes in filesystem paths. URLs use different semantics.
        const bool drive = line.length() >= 3 && juce::CharacterFunctions::isLetter(line[0]) && line[1] == ':' && (line[2] == '\\' || line[2] == '/');
        if ((!drive && line.containsChar(':')) || line.startsWithChar('"') || line.containsChar('\t')) return lineError("Use an unquoted local audio path; URLs and drive-relative paths are not supported.");
        const auto source = file.getParentDirectory().getChildFile(line);
        // Short relative names can expand to long absolute paths. Bound retained
        // path storage before adding each File, including its object/allocation overhead.
        expandedPathBytes += source.getFullPathName().getNumBytesAsUTF8() + sizeof(juce::File) + 64;
        if (expandedPathBytes > 64 * 1024 * 1024) return lineError("Resolved paths exceed the 64 MiB playlist path budget.");
        if (source.hasFileExtension("m3u;m3u8")) return lineError("Nested playlists are not supported.");
        if (!source.existsAsFile()) return lineError("Audio file is missing: " + source.getFullPathName());
        result.files.add(source); // Keep order and repeats; the library importer owns deduplication.
    }
    if (result.files.isEmpty()) return fail("Playlist contains no local audio files.");
    return result;
}
PlaylistTextResult playlistText(const Project& project, PlaylistExport format)
{
    PlaylistTextResult result;
    std::vector<const Clip*> ordered;
    for (const auto& clip : project.clips) ordered.push_back(&clip);
    std::stable_sort(ordered.begin(), ordered.end(), [](const auto* a, const auto* b) { return a->start < b->start; });
    if (ordered.empty()) { result.error = "Arrange some clips before exporting a playlist."; return result; }
    auto oneLine = [](const juce::String& value) { return value.replaceCharacters("\r\n\t", "   "); };
    if (format == PlaylistExport::m3u) result.text = "#EXTM3U\n";
    else result.text = "Mixworks tracklist: " + oneLine(project.name) + "\nTimes are absolute mix times. Audio edits remain in the .mixworks session.\n\nSTART\tLENGTH\tLANE\tTITLE\tSOURCE BPM\tKEY\tSOURCE\n";
    for (const auto* clip : ordered)
    {
        const auto& media = *clip->media; const auto path = media.file.getFullPathName();
        if (path.containsAnyOf("\r\n")) { result.text.clear(); result.error = "A source path contains a newline and cannot be represented in this playlist."; return result; }
        if (format == PlaylistExport::m3u)
            result.text += "#EXTINF:" + juce::String((juce::int64) std::round(media.duration())) + "," + oneLine(media.name) + "\n" + path + "\n";
        else
            result.text += timeText(clip->start) + "\t" + timeText(clip->length) + "\t" + juce::String(clip->track + 1) + "\t" + oneLine(media.name) + "\t"
                + (media.bpm() > 0 ? juce::String(media.bpm(), 4) : "Unknown") + "\t" + (media.key() >= 0 ? keyNoteName(media.key()) + " / " + keyCamelot(media.key()) : "Unknown") + "\t" + oneLine(path) + "\n";
    }
    if (format == PlaylistExport::text && !project.labels.empty())
    {
        result.text += "\nLABEL TIME\tLABEL\n";
        for (const auto& label : project.labels) result.text += timeText(label.time) + "\t" + label.text + "\n";
    }
    return result;
}
}
