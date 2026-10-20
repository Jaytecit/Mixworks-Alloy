#pragma once
#include "Model.h"

namespace mixworks
{
struct PlaylistReadResult { juce::Array<juce::File> files; juce::String error; };
// Local UTF-8 M3U/M3U8 only. Metadata is not trusted as source analysis.
PlaylistReadResult readPlaylist(const juce::File&, const std::function<bool()>& cancelled = {});
enum class PlaylistExport { m3u, text };
struct PlaylistTextResult { juce::String text, error; };
PlaylistTextResult playlistText(const Project&, PlaylistExport);
}
