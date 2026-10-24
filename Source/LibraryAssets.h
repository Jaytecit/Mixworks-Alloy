#pragma once
#include <juce_graphics/juce_graphics.h>

namespace mixworks
{
struct LibraryAssets
{
    juce::String title, artist, album, genre;
    bool genreRead = false;
    juce::Image cover;
    juce::File coverSource;
    bool audioThumbnail = false;
    juce::String artworkGroup; // Derived from normalized thumbnail pixels, not source paths.
    void prepareArtworkGrouping();
};
// Disk/handler access: call on a worker. Never looks up artwork on the web.
LibraryAssets readLibraryAssets(const juce::File& audio);
juce::String readLibraryGenre(const juce::File& audio);
}
