#pragma once
#include "AnalysisCache.h"

namespace mixworks
{
struct SourceSectionRead { SourceSections markers; bool exists = false; juce::String error; };
juce::var sourceSectionsToJson(const SourceSections&);
juce::Result readSourceSectionsJson(const juce::var&, double duration, SourceSections&);
juce::File sourceSectionRecordFile(const SourceIdentity&);
SourceSectionRead readSourceSectionRecord(const SourceIdentity&);
// Writes only compact annotation data. The verified loaded PCM identity owns
// this record; changed file facts or a concurrently replaced template reject it.
juce::String writeSourceSectionRecord(const juce::File& audio, const SourceIdentity&, int64_t modified,
                                     const SourceSections& expected, const SourceSections& replacement);
}
