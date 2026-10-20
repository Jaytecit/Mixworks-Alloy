#include "Engine.h"
#include <cmath>

namespace mixworks
{
void runNeuralFailureTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    const auto directory = report.getSiblingFile("model-failure-" + juce::Uuid().toString()); directory.createDirectory();
    const auto audio = directory.getChildFile("owned.wav");
    constexpr int rate = 8000, frames = rate * 4;
    juce::AudioBuffer<float> pcm(2, frames);
    for (int i = 0; i < frames; ++i) for (int ch = 0; ch < 2; ++ch)
        pcm.setSample(ch, i, (float) (0.2 * std::sin(i * 0.09) * std::exp(-(i % 4000) / 400.0)));
    {
        juce::WavAudioFormat format; std::unique_ptr<juce::OutputStream> stream = audio.createOutputStream();
        auto writer = format.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(rate).withNumChannels(2).withBitsPerSample(24));
        check(writer && writer->writeFromAudioSampleBuffer(pcm, 0, frames), "model failure fixture owns its audio");
    }
    TrackAnalysisRecord legacy; legacy.revision = 6; legacy.source = {audioDigest(audio), audio.getSize(), rate, frames, 2};
    legacy.manualBpm = 126.75; legacy.manualBeatOrigin = 0.213; legacy.manualKey = 17;
    check(writeAnalysisRecord(audio, legacy).isEmpty(), "model failure fixture owns stale automatic analysis and valid independent corrections");
    const auto file = analysisRecordFile(audio); const auto original = file.loadFileAsString();
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    auto loaded = readMedia(formats, audio);
    check(loaded.media && loaded.media->analysis.rhythmMethod == "unavailable" && loaded.media->analysis.rhythmIssue.isNotEmpty()
          && loaded.media->analysis.variableTempo && loaded.media->analysis.tempoConfidence == 0 && loaded.media->analysis.downbeatOrigin < 0,
          "a missing or corrupt model remains a visible unavailable automatic grid");
    check(loaded.media && loaded.media->manualBpm == 126.75 && loaded.media->manualBeatOrigin == 0.213 && loaded.media->manualKey == 17
          && loaded.media->analysisCacheWarning.isNotEmpty(), "model failure preserves manual BPM, beat and key corrections and reports its reason");
    check(file.loadFileAsString() == original && !file.getSiblingFile(file.getFileName() + ".bak").exists()
          && audioDigest(audio) == legacy.source.sha256, "failed analysis preserves source audio and stale evidence instead of caching an unavailable result");
}
}
