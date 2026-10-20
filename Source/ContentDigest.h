#pragma once
#include <juce_core/juce_core.h>

namespace mixworks
{
// SHA-256 of the remaining stream, using the OS implementation where available.
// Identical digest format and full-byte coverage to juce::SHA256.
juce::String contentDigest(juce::InputStream&);
}
