#include "DownbeatReferences.h"
#include <cmath>

namespace mixworks
{
juce::File downbeatReferenceRoot()
{
    return analysisStoreRoot().getSiblingFile("DownbeatReferences-v1");
}
juce::Result writeDownbeatReference(const juce::File& audio, const TrackAnalysisRecord& record, double previous)
{
    const auto& s = record.source;
    const auto duration = s.frames / s.sampleRate;
    auto anchor = [duration](double value) { return std::isfinite(value) && (value == -1 || (value >= 0 && value < duration)); };
    if (s.sha256.length() != 64 || !s.sha256.containsOnly("0123456789abcdef") || s.bytes <= 0
        || !std::isfinite(s.sampleRate) || s.sampleRate < 1000 || s.sampleRate > 768000 || s.frames <= 0 || s.channels <= 0 || s.channels > 64
        || !anchor(record.manualBeatOrigin) || !anchor(previous))
        return juce::Result::fail("Cannot identify the source or downbeat for its comparison reference.");
    if (!audio.existsAsFile() || audio.getSize() != s.bytes || audioDigest(audio) != s.sha256)
        return juce::Result::fail("The source changed; its downbeat reference was not saved.");
    const auto root = downbeatReferenceRoot();
    if (root.getParentDirectory().isSymbolicLink() || root.isSymbolicLink() || root.createDirectory().failed())
        return juce::Result::fail("Cannot create the downbeat reference folder; the previous correction was kept.");
    auto doc = analysisRecordToJson(record); auto* object = doc.getDynamicObject();
    object->removeProperty("peaks"); object->removeProperty("waveformRms");
    object->setProperty("format","mixworks-downbeat-reference"); object->setProperty("version",1);
    object->setProperty("kind","reviewed-choice");
    object->setProperty("recordedUtc",juce::Time::getCurrentTime().toISO8601(true));
    object->setProperty("applicationVersion",JUCE_APPLICATION_VERSION_STRING);
    object->setProperty("sourcePath",audio.getFullPathName());
    object->setProperty("previousManualAnchor",previous);
    const auto text = juce::JSON::toString(doc,false,17);
    if (text.getNumBytesAsUTF8() > 256 * 1024) return juce::Result::fail("The downbeat comparison exceeds its storage budget.");
    // UUID destinations never overwrite another observation, including another
    // instance's simultaneous review of the same source.
    const auto file = root.getChildFile(s.sha256 + "-" + juce::Uuid().toString() + ".json");
    if (file.exists()) return juce::Result::fail("The comparison destination already exists; it was kept.");
    juce::TemporaryFile temporary(file);
    if (!temporary.getFile().replaceWithText(text,false,false,"\n") || !temporary.overwriteTargetFileWithTemporary())
        return juce::Result::fail("Cannot save the downbeat comparison reference; the previous correction was kept.");
    return juce::Result::ok();
}
}
