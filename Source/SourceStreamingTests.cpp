#include "Model.h"
#include "NeuralRhythm.h"
#include "VerificationMetrics.h"
#include <juce_cryptography/juce_cryptography.h>
#include <limits>
#include <thread>

namespace mixworks
{
namespace
{
class ControlledReader final : public juce::AudioFormatReader
{
public:
    ControlledReader(std::unique_ptr<juce::AudioFormatReader> source, int failRead = 0, int corruptRead = 0,
                     std::shared_ptr<std::atomic<bool>> cancellation = {})
        : AudioFormatReader(nullptr, "controlled real WAV"), inner(std::move(source)), fail(failRead), corrupt(corruptRead), cancelled(std::move(cancellation))
    {
        sampleRate = inner->sampleRate; lengthInSamples = inner->lengthInSamples; numChannels = inner->numChannels;
        bitsPerSample = 32; usesFloatingPointData = true;
    }
    bool readSamples(int* const* destination, int destinationChannels, int startOffset, juce::int64 start, int count) override
    {
        ++reads; if (reads == fail) return false;
        juce::AudioBuffer<float> decoded(destinationChannels, count);
        if (!inner->read(&decoded, 0, count, start, true, true)) return false;
        for (int channel = 0; channel < destinationChannels; ++channel) if (destination[channel])
            std::copy_n(decoded.getReadPointer(channel), count, reinterpret_cast<float*>(destination[channel]) + startOffset);
        if (reads == corrupt && destination[0]) reinterpret_cast<float*>(destination[0])[startOffset] = std::numeric_limits<float>::quiet_NaN();
        if (cancelled && reads == 2) *cancelled = true;
        return true;
    }
    int reads = 0;
private:
    std::unique_ptr<juce::AudioFormatReader> inner;
    int fail, corrupt;
    std::shared_ptr<std::atomic<bool>> cancelled;
};

}
void runSourceStreamingTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report, const juce::File& largeSource)
{
    const auto root = report.getParentDirectory().getChildFile("streaming-" + juce::Uuid().toString()); root.createDirectory();
    const auto temporary = root.getChildFile("owned-temporary"); temporary.createDirectory();
    const auto source = root.getChildFile("actual-stereo.wav"); juce::AudioBuffer<float> generated(2, 100001);
    for (int i = 0; i < generated.getNumSamples(); ++i)
    {
        generated.setSample(0, i, .25f * std::sin(i * .00731f)); generated.setSample(1, i, -.125f * std::cos(i * .00479f));
    }
    {
        juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> output = source.createOutputStream();
        auto writer = wav.createWriterFor(output, juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(2).withBitsPerSample(24));
        check(writer && writer->writeFromAudioSampleBuffer(generated, 0, generated.getNumSamples()), "create real stereo WAV across several mapped-decode chunks");
    }
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    auto open = [&] { return std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(source)); };
    auto original = open(); check(original != nullptr, "ordinary production WAV reader opens the boundary fixture"); if (!original) return;
    juce::AudioBuffer<float> reference(2, generated.getNumSamples()); const auto readReference = original->read(&reference, 0, reference.getNumSamples(), 0, true, true);
    juce::String error; auto mapped = AudioCache::decode(*original, 2, reference.getNumSamples(), error, {}, temporary);
    bool exact = readReference && mapped && mapped->mapping && mapped->mapping->getSize() == (size_t) reference.getNumSamples() * 2 * sizeof(float);
    if (mapped) for (int channel = 0; channel < 2; ++channel) for (int i = 0; i < reference.getNumSamples(); ++i)
        exact = exact && mapped->channelData(channel)[i] == reference.getSample(channel, i);
    check(exact, "actual streaming decode preserves every stereo sample across chunk boundaries and the final partial chunk: " + error);
    const auto mappedFile = mapped ? mapped->file : juce::File(); mapped.reset();
    check(mappedFile != juce::File() && !mappedFile.exists(), "released session mapping removes only its owned decoded file");
    for (int kind = 0; kind < 3; ++kind)
    {
        auto cancel = kind == 2 ? std::make_shared<std::atomic<bool>>(false) : nullptr;
        ControlledReader reader(open(), kind == 0 ? 2 : 0, kind == 1 ? 2 : 0, cancel);
        auto failed = AudioCache::decode(reader, 2, reference.getNumSamples(), error, cancel, temporary);
        check(!failed && reader.reads == 2 && error.isNotEmpty() && temporary.findChildFiles(juce::File::findFiles, false).isEmpty(),
              kind == 0 ? "mid-stream decoder failure cleans partial owned PCM" : kind == 1 ? "mid-stream non-finite data rejects and cleans partial owned PCM" : "mid-stream cancellation cleans partial owned PCM without finishing the source");
    }
    auto cancel = std::make_shared<std::atomic<bool>>(true);
    auto cancelled = AudioCache::decode(*original, 2, reference.getNumSamples(), error, cancel, temporary);
    check(!cancelled && error.containsIgnoreCase("cancel") && temporary.findChildFiles(juce::File::findFiles, false).isEmpty(), "pre-cancelled mapping allocates no source-sized output");
    auto invalid = AudioCache::decode(*original, 3, reference.getNumSamples(), error, {}, temporary);
    check(!invalid && error.isNotEmpty() && temporary.findChildFiles(juce::File::findFiles, false).isEmpty(), "unsupported dimensions reject before creating decoded files");
    const auto obstruction = root.getChildFile("keep-existing-file"); obstruction.replaceWithText("kept output obstruction");
    invalid = AudioCache::decode(*original, 2, reference.getNumSamples(), error, {}, obstruction);
    check(!invalid && obstruction.loadFileAsString() == "kept output obstruction", "blocked decode folder preserves the existing unrelated file");

    const auto manifestFile = largeSource.getSiblingFile("owned-90-minute-manifest.json");
    const auto manifest = manifestFile.getSize() <= 256 * 1024 ? juce::JSON::parse(manifestFile) : juce::var();
    check(largeSource.existsAsFile() && manifest.isObject() && (juce::int64) manifest["frames"] == 238140000
          && (int) manifest["sampleRate"] == 44100 && (int) manifest["channels"] == 2 && largeSource.getSize() == 1428840044,
          "explicit owned 90-minute capacity fixture and dimensions are available");
    if (!largeSource.existsAsFile() || !manifest.isObject()) return;
    const auto sourceHash = juce::SHA256(largeSource).toHexString();
    check(sourceHash == manifest["sha256"].toString(), "owned long source matches its immutable creation digest");
    if (sourceHash != manifest["sha256"].toString()) return;
    const auto before = readVerificationMetrics(); const auto began = juce::Time::getMillisecondCounterHiRes();
    auto loaded = readMedia(formats, largeSource, {}, AnalysisCacheMode::readOnly);
    const auto elapsed = juce::Time::getMillisecondCounterHiRes() - began; const auto after = readVerificationMetrics();
    check(loaded.media && loaded.media->cache && loaded.media->samples.getNumSamples() == 0 && loaded.media->frames() == 238140000
          && loaded.media->channels() == 2 && loaded.media->duration() == 5400, "production importer accepts 90-minute stereo audio without retaining a whole decoded RAM buffer: " + loaded.error);
    auto* measurements = new juce::DynamicObject(); measurements->setProperty("source", largeSource.getFullPathName()); measurements->setProperty("sourceSha256", sourceHash);
    measurements->setProperty("importMilliseconds", elapsed); measurements->setProperty("before", verificationMetricsJson(before)); measurements->setProperty("after", verificationMetricsJson(after));
    // Counter definitions: https://learn.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-process_memory_counters_ex
    if (loaded.media)
    {
        const auto& media = *loaded.media; bool pcmMatches = true;
        for (int i = 0; i < media.frames(); ++i)
        {
            const bool impulse = i % 44100 == 0;
            pcmMatches = pcmMatches && media.channelData(0)[i] == (impulse ? .25f : 0) && media.channelData(1)[i] == (impulse ? -.125f : 0);
        }
        check(pcmMatches, "every decoded sample in the actual 90-minute mapped file matches its independent encoded impulse construction");
        check(media.playbackCacheWarning.isNotEmpty() && media.analysis.rhythmIssue.isNotEmpty(), "large temporary retention and the independent neural feature budget remain explicit facts");
        measurements->setProperty("frames", media.frames()); measurements->setProperty("mappedBytes", (juce::int64) media.cache->mapping->getSize());
        measurements->setProperty("retainedSampleBufferFrames", media.samples.getNumSamples()); measurements->setProperty("playbackWarning", media.playbackCacheWarning);
        measurements->setProperty("rhythmIssue", media.analysis.rhythmIssue);
        const auto ownedMappedFile = media.cache->file; loaded.media.reset();
        check(!ownedMappedFile.exists(), "releasing the long source removes its complete session-owned mapping");
    }
   #if JUCE_WINDOWS
    check(before.available && after.available && after.peakCommitted - before.peakCommitted < 1024LL * 1024 * 1024,
          "Windows peak private-commit growth stays below the old 1 GiB allocation ceiling while decoding 1.905 GB of float PCM");
   #endif
    auto preCancelled = readMedia(formats, largeSource, {}, AnalysisCacheMode::readOnly, MediaReadPurpose::playback, nullptr, cancel);
    check(!preCancelled.media && preCancelled.error.containsIgnoreCase("cancel"), "production cancellation rejects a long import before decoding it");
    const auto cancelledAnalysis = analyseMusic(reference, 8000, cancel);
    check(cancelledAnalysis.rhythmIssue.containsIgnoreCase("cancel") && cancelledAnalysis.tempoConfidence == 0 && cancelledAnalysis.downbeatOrigin < 0,
          "cancelled analysis preserves an explicit unavailable result instead of a partially confident grid");
    RhythmActivations stale; stale.beatLogits = {99};
    const auto cancelledNeural = analyseNeuralRhythm(reference, 8000, &stale, cancel);
    check(cancelledNeural.error.containsIgnoreCase("cancel") && stale.beatLogits.empty(), "pre-cancelled neural work clears previous research output before acquiring the model");
    juce::AudioBuffer<float> tooShort(1, 1); tooShort.clear();
    const auto shortNeural = analyseNeuralRhythm(tooShort, 44100);
    check(shortNeural.error.isNotEmpty() && shortNeural.beats.empty(), "unsupported short reflection input fails explicitly instead of indexing outside PCM");
    juce::AudioBuffer<float> longAnalysis(1, 8000 * 600); longAnalysis.clear();
    for (int i = 0; i < longAnalysis.getNumSamples(); i += 4000) longAnalysis.setSample(0, i, .25f);
    auto cancelOwner = std::make_shared<std::atomic<bool>>(false), cancelWaiter = std::make_shared<std::atomic<bool>>(false);
    juce::WaitableEvent enteredOwner, enteredWaiter, ownerDone, waiterDone; NeuralRhythm ownerResult, waiterResult;
    std::thread owner([&] { enteredOwner.signal(); ownerResult = analyseNeuralRhythm(longAnalysis, 8000, nullptr, cancelOwner); ownerDone.signal(); });
    const bool ownerStarted = enteredOwner.wait(1000); juce::Thread::sleep(20);
    std::thread waiter([&] { enteredWaiter.signal(); waiterResult = analyseNeuralRhythm(longAnalysis, 8000, nullptr, cancelWaiter); waiterDone.signal(); });
    const bool waiterStarted = enteredWaiter.wait(1000); juce::Thread::sleep(20); *cancelWaiter = true;
    const bool waiterStopped = waiterDone.wait(2000); *cancelOwner = true; const bool ownerStopped = ownerDone.wait(2000);
    owner.join(); waiter.join();
    check(ownerStarted && waiterStarted && waiterStopped && waiterResult.error.containsIgnoreCase("cancel"), "queued neural analysis can cancel while another source owns the shared model");
    check(ownerStopped && ownerResult.error.containsIgnoreCase("cancel"), "active long neural conversion/feature/inference work cooperatively releases its model and buffers");
    check(juce::SHA256(largeSource).toHexString() == sourceHash, "all long-source success and cancellation paths preserve the original encoded audio");
    report.getSiblingFile("source-streaming-metrics.json").replaceWithText(juce::JSON::toString(juce::var(measurements), true));
}
}
