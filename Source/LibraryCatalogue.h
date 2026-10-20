#pragma once
#include "Model.h"

namespace mixworks
{
struct CatalogueResult { juce::Array<juce::File> files; std::vector<std::shared_ptr<Media>> media; juce::String error; juce::var document; };
juce::File libraryCatalogueFile();
CatalogueResult readLibraryCatalogue(const juce::File&);
// Additive union under an interprocess lock; refuses unidentified/newer/malformed data.
juce::String rememberLibraryCatalogue(const juce::File&, const juce::Array<juce::File>&, const std::vector<std::shared_ptr<Media>>& snapshots = {});
}
