#include "StemJobs.h"
#include "AnalysisCache.h"
#include "ContentDigest.h"

namespace mixworks
{
namespace
{
juce::String sha(const juce::File& file)
{
    auto stream = file.createInputStream();
    return stream ? contentDigest(*stream) : juce::String();
}
bool safeDirectory(const juce::File& directory)
{
    for (auto at = directory; at != at.getParentDirectory(); at = at.getParentDirectory())
        if (at.isSymbolicLink()) return false;
    return true;
}
void require(bool valid, const juce::String& error) { if (!valid) throw error; }
}
juce::File StemJob::defaultRoot() { return analysisStoreRoot().getSiblingFile("Stems-v1"); }
juce::File StemJob::defaultRuntime() { return analysisStoreRoot().getSiblingFile("StemRuntime-v1.json"); }
StemJob::StemJob(juce::File s, juce::File r, juce::File config, juce::String d)
    : Thread("Stem separation"), source(s), root(r), runtime(config), device(d) {}
StemJob::~StemJob() { finish(); }
void StemJob::finish() { cancel(); stopThread(-1); }
void StemJob::begin()
{
    const juce::ScopedLock lock(mutex);
    if (current.running || isThreadRunning()) return;
    current = {}; current.running = true; current.message = "Preparing source"; cancelled = false;
    startThread(juce::Thread::Priority::low);
}
void StemJob::cancel() { cancelled = true; }
StemJobState StemJob::state() const { const juce::ScopedLock lock(mutex); return current; }
void StemJob::update(const juce::String& message, double progress)
{
    const juce::ScopedLock lock(mutex); current.message = message; current.progress = progress;
}
void StemJob::run()
{
    juce::File job;
    juce::ChildProcess process;
    auto checkCancel = [this] { require(!cancelled, "Cancelled"); };
    try
    {
        require(safeDirectory(root) && root.createDirectory().wasOk(), "Cannot create the stem cache");
        const auto config = juce::JSON::parse(runtime);
        const juce::File python(config["python"].toString()), worker(config["worker"].toString());
        require(python.existsAsFile() && worker.existsAsFile(), "Stem runtime is missing. Run scripts/setup-stems.py first.");
        require(sha(worker) == config["workerSha256"].toString(), "Stem worker changed. Run setup again.");
        require(source.existsAsFile(), "Source file is missing");
        const auto sourceHash = sha(source); require(sourceHash.length() == 64, "Cannot identify source"); checkCancel();
        juce::AudioFormatManager formats; formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(source));
        require(reader != nullptr, "Cannot decode source");
        const auto frames = reader->lengthInSamples; const auto channels = reader->numChannels;
        const auto rate = reader->sampleRate; const auto bytes = frames * channels * 4;
        require(channels >= 1 && channels <= 2 && rate >= 8000 && rate <= 192000 && frames > 0 && frames <= rate * 600
                && bytes <= 512LL * 1024 * 1024, "Split supports mono/stereo up to 10 minutes and 512 MB decoded audio");
        require(root.getBytesFreeOnVolume() > bytes * 5 + 512LL * 1024 * 1024, "Not enough free space for four stems");
        job = root.getChildFile("jobs").getChildFile(juce::Uuid().toString());
        require(safeDirectory(job) && !job.exists() && job.createDirectory().wasOk(), "Cannot create a stem job");
        // IEEE float WAV at the exact JUCE-decoded source clock; no peak normalisation.
        {
            juce::FileOutputStream output(job.getChildFile("input.wav"));
            require(output.openedOk(), "Cannot write source PCM");
            output.write("RIFF", 4); output.writeInt((int) bytes + 36); output.write("WAVEfmt ", 8);
            output.writeInt(16); output.writeShort(3); output.writeShort((short) channels);
            output.writeInt((int) rate); output.writeInt((int) rate * (int) channels * 4);
            output.writeShort((short) (channels * 4)); output.writeShort(32); output.write("data", 4); output.writeInt((int) bytes);
            juce::AudioBuffer<float> block((int) channels, 65536);
            std::vector<float> interleaved(65536 * channels);
            for (juce::int64 offset = 0; offset < frames; offset += block.getNumSamples())
            {
                checkCancel(); const int count = (int) std::min<juce::int64>(block.getNumSamples(), frames - offset);
                require(reader->read(&block, 0, count, offset, true, true), "Source decode failed");
                for (int i = 0; i < count; ++i) for (unsigned ch = 0; ch < channels; ++ch)
                {
                    const float value = block.getSample((int) ch, i);
                    require(std::isfinite(value), "Source contains invalid samples");
                    interleaved[(size_t) i * channels + ch] = value;
                }
                require(output.write(interleaved.data(), (size_t) count * channels * sizeof(float)), "Source PCM write failed");
            }
            output.flush(); require(output.getStatus().wasOk(), "Source PCM flush failed");
        }
        reader.reset(); checkCancel();
        auto* object = new juce::DynamicObject(); juce::var request(object);
        object->setProperty("runtime", runtime.getFullPathName()); object->setProperty("cache", root.getFullPathName());
        object->setProperty("sourceSha256", sourceHash); object->setProperty("device", device);
        const auto requestFile = job.getChildFile("job.json");
        require(requestFile.replaceWithText(juce::JSON::toString(request)), "Cannot write stem request");
        require(process.start(juce::StringArray {python.getFullPathName(), worker.getFullPathName(), "--job", requestFile.getFullPathName()}, 0), "Cannot start stem worker");
        const auto started = juce::Time::getMillisecondCounterHiRes(); double cancelledAt = 0;
        while (process.isRunning())
        {
            if (cancelled)
            {
                if (cancelledAt == 0) { cancelledAt = juce::Time::getMillisecondCounterHiRes(); job.getChildFile("cancel").replaceWithText("cancel"); }
                if (juce::Time::getMillisecondCounterHiRes() - cancelledAt > 3000) { process.kill(); process.waitForProcessToFinish(5000); }
            }
            require(juce::Time::getMillisecondCounterHiRes() - started < 4 * 60 * 60 * 1000.0, "Stem worker timed out");
            const auto status = juce::JSON::parse(job.getChildFile("status.json"));
            if (status.isObject()) update(status["state"].toString(), juce::jlimit(0.0, 1.0, (double) status["progress"]));
            wait(50);
        }
        checkCancel();
        const auto status = juce::JSON::parse(job.getChildFile("status.json"));
        require(process.getExitCode() == 0 && (status["state"].toString() == "Prepared" || status["state"].toString() == "Ready"),
                status["error"].toString().isNotEmpty() ? status["error"].toString() : "Stem worker failed; no stems published");
        update("Checking original source", .99);
        require(sha(source) == sourceHash, "Source changed during separation; no stems published"); checkCancel();
        juce::File result(status["directory"].toString());
        if (status["state"].toString() == "Prepared")
        {
            const auto key = status["key"].toString();
            require(key.length() == 64 && key.containsOnly("0123456789abcdef") && result == job.getChildFile("assets"), "Invalid stem result path");
            const auto target = root.getChildFile(key);
            require(!target.exists(), "Another job already published these stems; split again to reuse them");
            require(result.moveFileTo(target), "Cannot publish stems; no completed result was adopted"); result = target;
        }
        require(result.getParentDirectory() == root && result.getChildFile("manifest.json").existsAsFile(), "Invalid completed stem set");
        const juce::ScopedLock lock(mutex);
        current.ready = true; current.progress = 1; current.directory = result;
        current.reused = (bool) status["reused"]; current.message = current.reused ? "Stems ready (reused)" : "Four stems ready";
    }
    catch (const juce::String& error)
    {
        if (process.isRunning()) { process.kill(); process.waitForProcessToFinish(5000); }
        const juce::ScopedLock lock(mutex); current.message = cancelled ? "Separation cancelled" : "Separation failed"; current.error = error;
    }
    // Only this invocation's UUID directory is disposable. Never touch completed sets.
    if (job != juce::File() && job.getParentDirectory() == root.getChildFile("jobs") && safeDirectory(job) && !process.isRunning()) job.deleteRecursively();
    const juce::ScopedLock lock(mutex); current.running = false;
}
}
