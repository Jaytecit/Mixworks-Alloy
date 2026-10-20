#include "Workstation.h"
#include "Beatmatch.h"
#include <juce_cryptography/juce_cryptography.h>
#include <cmath>

namespace mixworks
{
void runPlaybackCacheTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    const auto folder = report.getParentDirectory().getChildFile("playback-workflows-" + juce::Uuid().toString()); folder.createDirectory();
    const auto source = folder.getChildFile("pulse.wav");
    constexpr int sr = 22050;
    {
        juce::AudioBuffer<float> audio(2, 8 * sr);
        for (int i = 0; i < audio.getNumSamples(); ++i) for (int ch = 0; ch < 2; ++ch) audio.setSample(ch, i, (float) (0.15 * std::sin(i * 440.0 * juce::MathConstants<double>::twoPi / sr) + (i % (sr / 2) < 50 ? 0.5 : 0)));
        juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> output = source.createOutputStream();
        auto writer = wav.createWriterFor(output, juce::AudioFormatWriterOptions().withSampleRate(sr).withNumChannels(2).withBitsPerSample(24));
        check(writer && writer->writeFromAudioSampleBuffer(audio, 0, audio.getNumSamples()), "cache workflows create an owned real audio fixture");
    }
    juce::AudioFormatManager formats; formats.registerBasicFormats(); int decodes = 0, analyses = 0;
    auto progress = [&](MediaReadStage stage) { if (stage == MediaReadStage::decode) ++decodes; if (stage == MediaReadStage::analyse) ++analyses; };
    auto cold = readMedia(formats, source, progress);
    check(cold.media && !cold.media->decodedFromCache && decodes == 1 && analyses == 1, "cold production import decodes, analyses and writes a persistent playback cache"); if (!cold.media) return;
    const auto key = PcmStore::decodedKey(cold.media->analysisSource); const auto cachedFile = cold.media->cache->file;
    cold.media->manualBpm = 120; cold.media->manualBeatOrigin = 0;
    auto warm = readMedia(formats, source, progress);
    check(warm.media && warm.media->decodedFromCache && warm.media->analysisFromCache && decodes == 1 && analyses == 1 && !warm.media->cache->transient
          && warm.media->channelData(0)[1234] == cold.media->channelData(0)[1234], "warm import bypasses PCM decoding and analysis, using the validated mapped cache");
    auto& store = PcmStore::shared();
    check(!store.read(key, sr, 1, cold.media->frames()) && !store.read(key, sr + 1, 2, cold.media->frames()), "cache headers bind dimensions and sample rate before exposing PCM");
    warm.media.reset(); cold.media.reset();
    check(cachedFile.existsAsFile(), "decoded cache survives release of every media reference");
    {
        auto output = cachedFile.createOutputStream(); output->setPosition(PcmStore::headerBytes + 17); output->writeByte(42); output->flush();
    }
    auto repaired = readMedia(formats, source, progress);
    check(repaired.media && !repaired.media->decodedFromCache && decodes == 2 && repaired.media->analysisFromCache, "payload corruption triggers redecoding while retaining independent saved analysis"); if (!repaired.media) return;
    auto reused = readMedia(formats, source, progress);
    check(reused.media && reused.media->decodedFromCache && decodes == 2, "an owned damaged decoded cache is repaired atomically and reused next time");
    repaired.media->manualBpm = 120; repaired.media->manualBeatOrigin = 0;
    Engine engine; engine.project.followTrackTempo = false; engine.project.masterDb = 0; engine.library = {repaired.media};
    Clip clip; clip.media = repaired.media; clip.rate = 1.125; clip.beatmatched = true; clip.length = clip.media->duration() / clip.rate; engine.project.clips = {clip};
    check(prepareSavedClips(engine.project).isEmpty() && engine.project.clips[0].prepared && !engine.project.clips[0].prepared->fromDiskCache, "first fixed-tempo preparation computes and persists SoundTouch output");
    check(engine.project.clips[0].prepared && engine.project.clips[0].prepared->cache && !engine.project.clips[0].prepared->cache->transient
          && engine.project.clips[0].prepared->residentBytes() == 0, "cold fixed-tempo preparation adopts its persistent mapping and releases its RAM buffer");
    check(stretchAudio(*repaired.media, 1.125, 0).error.isEmpty(), "fixed-tempo cache hits need no new resident PCM allocation");
    check(stretchAudio(*repaired.media, 0.000001, maxPreparedBytes).error.isNotEmpty(), "a single oversized tempo output is rejected before allocation");
    auto blocked = stretchAudio(*repaired.media, 1.234567, maxPreparedBytes);
    const auto blockedEntry = blocked.audio && blocked.audio->cache ? blocked.audio->cache->file : juce::File(); blocked.audio.reset();
    const bool blockedReady = blockedEntry.existsAsFile() && blockedEntry.deleteFile() && blockedEntry.replaceWithText("unidentified PCM entry");
    auto fallback = stretchAudio(*repaired.media, 1.234567, maxPreparedBytes);
    const bool temporary = fallback.audio && fallback.audio->cache && fallback.audio->cache->transient;
    check(blockedReady && fallback.error.isEmpty() && temporary && fallback.audio->residentBytes() == 0 && fallback.audio->cacheWarning.isNotEmpty()
          && blockedEntry.loadFileAsString() == "unidentified PCM entry", "an unavailable persistent entry falls back to mapped temporary PCM, releases RAM and preserves unidentified data");
    const auto temporaryFile = temporary ? fallback.audio->cache->file : juce::File(); fallback.audio.reset();
    check(temporary && !temporaryFile.exists(), "temporary prepared PCM is removed after its last playback/undo reference is released");
    juce::MidiBuffer midi; juce::AudioBuffer<float> expected(2, 512), actual(2, 512); engine.render(expected, 512, 2.1, midi);
    const auto fixedSession = folder.getChildFile("fixed.mixworks"); engine.saveProject(fixedSession);
    const auto fixedStart = juce::Time::getMillisecondCounterHiRes();
    check(engine.restoreProject(fixedSession.loadFileAsString()).isEmpty() && engine.library[0]->decodedFromCache && engine.project.clips[0].prepared->fromDiskCache,
          "saved fixed-tempo session reopens with both decoded and tempo PCM caches");
    const auto fixedMs = juce::Time::getMillisecondCounterHiRes() - fixedStart;
    engine.render(actual, 512, 2.1, midi); bool equal = true; for (int i = 0; i < 512; ++i) equal &= expected.getSample(0, i) == actual.getSample(0, i);
    check(equal, "mapped fixed-tempo cache preserves exact rendered seek PCM; warm reopen " + juce::String(fixedMs, 1) + " ms");
    bool fractional = true;
    for (double ratio : {127.3749 / 120, 0.9876543210987654, 1.3333333333333333})
    {
        auto candidate = engine.project; candidate.clips[0].rate = ratio; candidate.clips[0].length = 8 / ratio; candidate.clips[0].prepared.reset();
        fractional &= prepareSavedClips(candidate).isEmpty(); engine.project = std::move(candidate); engine.saveProject(fixedSession);
        fractional &= engine.restoreProject(fixedSession.loadFileAsString()).isEmpty() && engine.project.clips[0].prepared->fromDiskCache;
    }
    check(fractional, "fractional tempo ratios retain exact prepared-cache identity through production session serialization");
    engine.render(actual, 512, 2.1, midi); juce::MemoryOutputStream pcmBytes;
    for (int ch = 0; ch < 2; ++ch) pcmBytes.write(actual.getReadPointer(ch), 512 * sizeof(float));
    juce::StringArray childArgs {juce::File::getSpecialLocation(juce::File::currentExecutableFile).getFullPathName(), "--self-test", "--cache-reopen-only", "--playback-cache-root", store.directory().getFullPathName(), "--session", fixedSession.getFullPathName(), "--expected-pcm", juce::SHA256(pcmBytes.getData(), pcmBytes.getDataSize()).toHexString(), "--report", folder.getChildFile("fresh-process.txt").getFullPathName()};
    juce::ChildProcess child;
    const bool started = child.start(childArgs), finished = started && child.waitForProcessToFinish(15000);
    if (started && !finished) check(child.kill(), "timed-out owned cache test process is terminated");
    check(finished && child.getExitCode() == 0, "a separate application process reopens the saved session from disk caches with exact rendered PCM");
    auto ramp = engine.project; ramp.followTrackTempo = true; ramp.tempo.points = {{0, 120}, {4, 143}, {8, 115}};
    ramp.clips[0].rate = 1; ramp.clips[0].length = 8; ramp.clips[0].prepared.reset();
    auto firstRamp = prepareTempoMap(ramp, false, 0);
    check(firstRamp.error.isEmpty() && firstRamp.project.clips[0].prepared->clipLocal && !firstRamp.project.clips[0].prepared->fromDiskCache, "first tempo-ramp preparation persists the full source/loop/tempo geometry");
    if (firstRamp.error.isNotEmpty()) return;
    check(firstRamp.project.clips[0].prepared->cache && !firstRamp.project.clips[0].prepared->cache->transient && firstRamp.project.clips[0].prepared->residentBytes() == 0,
          "cold tempo-ramp preparation adopts its persistent mapping and releases its RAM buffer");
    engine.project = firstRamp.project; engine.render(expected, 512, 3.2, midi); const auto rampSession = folder.getChildFile("ramp.mixworks"); engine.saveProject(rampSession);
    const double warmStart = juce::Time::getMillisecondCounterHiRes();
    const auto error = engine.restoreProject(rampSession.loadFileAsString());
    check(error.isEmpty() && engine.project.clips[0].prepared->fromDiskCache, "saved variable-tempo session skips SoundTouch preparation on reopen: " + error);
    engine.render(actual, 512, 3.2, midi); equal = true; for (int i = 0; i < 512; ++i) equal &= expected.getSample(0, i) == actual.getSample(0, i);
    check(equal, "mapped tempo-ramp cache preserves exact seek PCM; warm reopen " + juce::String(juce::Time::getMillisecondCounterHiRes() - warmStart, 1) + " ms");
    {
        auto automatic = readMedia(formats, source);
        if (automatic.media)
        {
            const auto previousRecord = automatic.media->analysisRecord();
            automatic.media->manualBpm = 0; automatic.media->analysis.bpm = 120;
            automatic.media->analysis.variableTempo = false; automatic.media->analysis.tempoConfidence = 1;
            Engine retained; retained.sampleRate = sr; retained.library = {automatic.media}; retained.project = firstRamp.project;
            retained.project.clips[0].media = automatic.media;
            const auto geometry = retained.project.clips[0];
            retained.render(expected, 512, 3.2, midi);
            const auto savedAutomatic = juce::JSON::toString(retained.captureProject());
            auto refreshedRecord = automatic.media->analysisRecord(); refreshedRecord.analysis.bpm = 60;
            check(writeAnalysisRecord(source, refreshedRecord).isEmpty(), "publish a changed automatic tempo in an owned session-refresh fixture");
            const auto reopened = retained.restoreProject(savedAutomatic);
            check(reopened.isEmpty() && retained.project.clips[0].start == geometry.start && retained.project.clips[0].length == geometry.length
                  && retained.project.clips[0].rate == geometry.rate && retained.project.clips[0].media->manualBpm == 0
                  && retained.project.clips[0].media->analysis.bpm == 60 && retained.project.clips[0].tempoSourceBpm == 120,
                  "refreshed automatic BPM remains visible without retiming a saved arrangement or inventing a manual correction");
            retained.render(actual, 512, 3.2, midi); bool preserved = true;
            for (int i = 0; i < 512; ++i) preserved &= expected.getSample(0, i) == actual.getSample(0, i);
            check(reopened.isEmpty() && preserved, "saved tempo-mapped playback remains sample-identical after an automatic detector refresh");
            auto preparedAgain = prepareTempoMap(retained.project, false, 0);
            check(preparedAgain.error.isEmpty() && preparedAgain.project.clips[0].tempoSourceBpm == 120
                  && preparedAgain.project.clips[0].start == geometry.start && preparedAgain.project.clips[0].length == geometry.length,
                  "ordinary preparation retains the captured clock after fresh automatic BPM changes");
            auto appended = retained.project; auto newcomer = geometry; newcomer.id = juce::Uuid().toString();
            newcomer.media = firstRamp.project.clips[0].media; newcomer.pendingPlacement = PendingPlacement::append;
            appended.clips.push_back(newcomer); const auto appendedResult = prepareTempoMap(appended, false, 0);
            check(appendedResult.error.isEmpty() && appendedResult.project.clips[0].tempoSourceBpm == 120
                  && std::abs(appendedResult.project.clips[0].length * appendedResult.project.clips[0].rate - geometry.length * geometry.rate) < 1.0e-8,
                  "adding another source cannot adopt refreshed automatic BPM into an existing source clock");
            auto legacy = juce::JSON::parse(savedAutomatic); legacy.getDynamicObject()->setProperty("version", 12); legacy["tracks"].getArray()->removeRange(4, trackCount - 4);
            for (auto& item : *legacy["clips"].getArray()) item.getDynamicObject()->removeProperty("tempoSourceBpm");
            const auto migrated = retained.restoreProject(juce::JSON::toString(legacy));
            check(migrated.isEmpty() && std::abs(retained.project.clips[0].length - geometry.length) < 1.0e-10
                  && std::abs(retained.project.clips[0].sourceElapsed(3) - geometry.sourceElapsed(3)) < 1.0e-10,
                  "version 12 derives its captured source clock from saved geometry instead of refreshed analysis");
            check(writeAnalysisRecord(source, previousRecord).isEmpty(), "restore the owned cache fixture's original automatic analysis");
        }
    }
    auto changed = engine.project; changed.tempo.points[1].bpm += 1;
    auto changedTempo = prepareTempoMap(changed, false, 0);
    check(changedTempo.error.isEmpty() && !changedTempo.project.clips[0].prepared->fromDiskCache, "tempo edits invalidate prepared audio without invalidating decoded source PCM");
    auto backwards = ramp; backwards.clips[0].reversed = true; backwards.clips[0].offset = 1; backwards.clips[0].loopSpan = 1; backwards.clips[0].loopPhase = 0.125; backwards.clips[0].length = 6;
    auto reversed = prepareTempoMap(backwards, false, 0); auto reversedAgain = prepareTempoMap(backwards, false, 0);
    check(reversed.error.isEmpty() && reversedAgain.error.isEmpty() && reversedAgain.project.clips[0].prepared->fromDiskCache, "reverse and loop phase are preserved by tempo-cache reuse");
    backwards.clips[0].loopPhase += 0.125; auto otherPhase = prepareTempoMap(backwards, false, 0);
    check(otherPhase.error.isEmpty() && !otherPhase.project.clips[0].prepared->fromDiskCache, "a changed loop phase cannot select another clip's prepared PCM");
    check(prepareTempoMap(ramp, false, 0, maxPreparedBytes).error.isEmpty(), "warm mapped tempo audio remains usable when the resident preparation budget is exhausted");
    auto batch = ramp; batch.clips.clear();
    for (int i = 0; i < 4; ++i) { auto part = ramp.clips[0]; part.id = juce::Uuid().toString(); part.offset = 0.1 * (i + 1); part.length = 7; batch.clips.push_back(part); }
    check(prepareTempoMap(batch, false, 0, maxPreparedBytes - 2LL * sr * 8 * sizeof(float)).error.isEmpty(),
          "cold multi-clip ramps release each PCM allocation before preparing the next clip");
    auto large = ramp; large.clips.clear();
    const int count = (int) (maxPreparedBytes / firstRamp.project.clips[0].prepared->byteSize()) + 1;
    for (int i = 0; i < count; ++i) { auto part = ramp.clips[0]; part.id = juce::Uuid().toString(); large.clips.push_back(part); }
    auto largePrepared = prepareTempoMap(large, false, 0); int64_t mappedBytes = 0, residentBytes = 0;
    for (const auto& part : largePrepared.project.clips) if (part.prepared) { mappedBytes += part.prepared->byteSize(); residentBytes += part.prepared->residentBytes(); }
    check(largePrepared.error.isEmpty() && mappedBytes > maxPreparedBytes && residentBytes == 0,
          "a complete project with over 1 GiB of mapped tempo PCM prepares without exhausting the RAM budget");
    const auto benchmarkAudio = folder.getChildFile("benchmark.wav"); source.copyFileTo(benchmarkAudio);
    { auto output = benchmarkAudio.createOutputStream(); output->setPosition(benchmarkAudio.getSize() - 1); output->writeByte(13); }
    Engine benchmark; benchmark.project.followTrackTempo = false; benchmark.project.masterDb = 0;
    auto benchmarkMedia = std::make_shared<Media>(); benchmarkMedia->file = benchmarkAudio; benchmarkMedia->manualBpm = 120;
    benchmark.library = {benchmarkMedia}; Clip benchmarkClip; benchmarkClip.media = benchmarkMedia; benchmarkClip.rate = 1.125; benchmarkClip.beatmatched = true; benchmarkClip.length = 8 / 1.125; benchmark.project.clips = {benchmarkClip};
    // A cold-open fixture deliberately has no hydrated audio; the interactive Save
    // boundary now correctly refuses that pending state. Write the session contract fixture directly.
    const auto benchmarkSession = folder.getChildFile("benchmark.mixworks");
    const bool benchmarkSaved = benchmarkSession.replaceWithText(juce::JSON::toString(projectToJson(benchmark.project, benchmark.library), false, 17));
    const auto coldBegin = juce::Time::getMillisecondCounterHiRes(); const auto coldError = benchmark.restoreProject(benchmarkSession.loadFileAsString()); const auto coldTime = juce::Time::getMillisecondCounterHiRes() - coldBegin;
    const bool wasCold = benchmarkSaved && coldError.isEmpty() && !benchmark.library[0]->decodedFromCache && !benchmark.project.clips[0].prepared->fromDiskCache;
    const auto warmBegin = juce::Time::getMillisecondCounterHiRes(); const auto warmError = benchmark.restoreProject(benchmarkSession.loadFileAsString()); const auto warmTime = juce::Time::getMillisecondCounterHiRes() - warmBegin;
    check(wasCold && warmError.isEmpty() && benchmark.library[0]->decodedFromCache && benchmark.project.clips[0].prepared->fromDiskCache,
          "production full-session benchmark: cold " + juce::String(coldTime, 1) + " ms / warm " + juce::String(warmTime, 1) + " ms (one synthetic fixture, not a general guarantee)");

    const auto oldModified = source.getLastModificationTime(); const auto oldSize = source.getSize();
    { auto output = source.createOutputStream(); output->setPosition(oldSize - 1); output->writeByte(17); } source.setLastModificationTime(oldModified);
    auto changedBytes = readMedia(formats, source);
    check(changedBytes.media && !changedBytes.media->decodedFromCache && !changedBytes.media->analysisFromCache && source.getSize() == oldSize, "source content changes invalidate decoded caches even when size and modification time are retained");
    const auto readOnlyAudio = folder.getChildFile("readonly.wav"); source.copyFileTo(readOnlyAudio);
    { auto output = readOnlyAudio.createOutputStream(); output->setPosition(oldSize - 1); output->writeByte(19); }
    auto readOnly = readMedia(formats, readOnlyAudio, {}, AnalysisCacheMode::readOnly);
    check(readOnly.media && !analysisRecordFile(readOnlyAudio).exists() && !store.entryFile(PcmStore::decodedKey(readOnly.media->analysisSource)).exists(), "read-only analysis does not create persistent decoded audio or sidecar records on a miss");
    int catalogueDecodes = 0;
    auto catalogue = readMedia(formats, source, [&](MediaReadStage stage) { if (stage == MediaReadStage::decode) ++catalogueDecodes; }, AnalysisCacheMode::readWrite, MediaReadPurpose::catalogue);
    check(catalogue.media && !catalogue.media->hasPlayback() && catalogue.media->duration() == 8 && !catalogue.media->peaks.empty() && catalogueDecodes == 0,
          "warm catalogue loading retains source facts and waveforms without decoding, pinning PCM or creating a temporary playback file");
    if (catalogue.media) check(stretchAudio(*catalogue.media, 1.1, maxPreparedBytes).error.isNotEmpty(), "tempo preparation explicitly rejects an unhydrated catalogue source");

    juce::AudioBuffer<float> tiny(2, 256); tiny.clear();
    const auto quotaRoot = folder.getChildFile("quota"); const int64_t entrySize = 2 * 256 * sizeof(float) + PcmStore::headerBytes;
    PcmStore limited(quotaRoot, 2 * entrySize + 100);
    const auto aKey = juce::String::repeatedString("a", 64), bKey = juce::String::repeatedString("b", 64), cKey = juce::String::repeatedString("c", 64); juce::String warning;
    auto a = limited.write(aKey, sr, tiny, warning); auto b = limited.write(bKey, sr, tiny, warning); a.reset();
    auto c = limited.write(cKey, sr, tiny, warning);
    check(b && c && limited.entryFile(bKey).existsAsFile() && !limited.entryFile(aKey).existsAsFile(), "disk quota evicts an unused owned entry while retaining a live mapped cache");
    PcmStore tooSmall(folder.getChildFile("too-small"), 100);
    check(!tooSmall.write(aKey, sr, tiny, warning) && warning.isNotEmpty(), "a full disk budget is a nonfatal cache miss with a meaningful reason");
    const auto foreignRoot = folder.getChildFile("foreign"); foreignRoot.createDirectory(); foreignRoot.getChildFile("keep.txt").replaceWithText("user data"); PcmStore foreign(foreignRoot);
    check(!foreign.write(aKey, sr, tiny, warning) && foreignRoot.getChildFile("keep.txt").loadFileAsString() == "user data", "an unidentified cache directory is never adopted or cleaned");
    PcmStore guarded(folder.getChildFile("guarded")); guarded.entryFile(aKey).replaceWithText("unidentified file");
    check(!guarded.write(aKey, sr, tiny, warning) && guarded.entryFile(aKey).loadFileAsString() == "unidentified file", "an unidentified file at a cache key is kept rather than overwritten");

    const auto scanRoot = folder.getChildFile("Music"); scanRoot.createDirectory(); source.copyFileTo(scanRoot.getChildFile("new.wav"));
    const auto subfolder = scanRoot.getChildFile("nested"); subfolder.createDirectory(); source.copyFileTo(subfolder.getChildFile("nested.wav")); scanRoot.getChildFile("ignore.txt").replaceWithText("not audio");
    MusicScan scan(scanRoot, {}, {}); int discovered = 0;
    for (int guard = 0; guard < 10; ++guard) { auto step = scan.step(); if (step.loaded.media) ++discovered; if (step.done) break; }
    check(discovered == 2 && analysisRecordFile(scanRoot.getChildFile("new.wav")).existsAsFile(), "Music scan recursively discovers and analyses new supported audio without importing unrelated files");
    MusicScan cancelled(scanRoot, {}, {}); cancelled.cancelled = true; auto cancelledStep = cancelled.step();
    check(cancelledStep.done && !cancelledStep.loaded.media, "cancelled Music scans stop before decoding a candidate");
    const auto preferences = folder.getChildFile("scan-preferences.json");
    check(readScanPreference(preferences).enabled && writeScanPreference(preferences, false).isEmpty() && !readScanPreference(preferences).enabled, "automatic Music scanning defaults on and its disabled preference persists");
    auto preference = juce::JSON::parse(preferences); preference.getDynamicObject()->setProperty("extra", "retained"); preferences.replaceWithText(juce::JSON::toString(preference));
    check(writeScanPreference(preferences, true).isEmpty() && juce::JSON::parse(preferences)["extra"].toString() == "retained", "scan preference updates are additive and retain unknown fields");
    preferences.replaceWithText("{unknown"); check(writeScanPreference(preferences, true).isNotEmpty() && preferences.loadFileAsString() == "{unknown", "malformed scan preferences are preserved and reported");

    Workstation ui(false); ui.stopTimer(); ui.musicFolder = scanRoot; ui.musicPreference = folder.getChildFile("ui-scan.json"); ui.musicScanEnabled = true; ui.nextMusicScan = 0;
    ui.engine.project.labels.push_back({"keep-label", 1, "Keep session"});
    struct Poll final : juce::Timer { std::function<void()> tick; void timerCallback() override { tick(); } } poll;
    const double deadline = juce::Time::getMillisecondCounterHiRes() + 20000; int phase = 0; bool tempoContinued = false;
    std::shared_ptr<Media> oldScanSource;
    poll.tick = [&]
    {
        if (juce::Time::getMillisecondCounterHiRes() > deadline) { check(false, "background Music UI workflow completes within deadline"); poll.stopTimer(); juce::MessageManager::getInstance()->stopDispatchLoop(); return; }
        ui.tickMusicScan();
        if (ui.musicScan || ui.musicScanJobPending) return;
        if (phase == 0)
        {
            if (ui.engine.library.size() != 2) return;
            check(ui.engine.library.size() == 2 && ui.engine.project.clips.empty() && ui.engine.project.labels[0].id == "keep-label"
                  && std::none_of(ui.engine.library.begin(), ui.engine.library.end(), [](const auto& media) { return media->hasPlayback(); }), "automatic scan commits lightweight library entries through the native worker/message queue without changing the arrangement");
            ui.engine.library[0]->manualBpm = 123.4; analysisRecordFile(ui.engine.library[0]->file).deleteFile(); ui.nextMusicScan = 0; ++phase;
        }
        else if (phase == 1)
        {
            if (ui.musicRefreshed == 0) return;
            check(ui.engine.library.size() == 2 && ui.engine.library[0]->manualBpm == 123.4, "scanning an unanalysed existing source regenerates its record and retains session corrections without duplicates");
            oldScanSource = ui.engine.library[0]; Clip retained; retained.media = oldScanSource; retained.length = oldScanSource->duration();
            ui.engine.project.clips = {retained}; oldScanSource->file.appendText("updated metadata"); ui.nextMusicScan = 0; phase = 11; return;
        }
        else if (phase == 11)
        {
            if (ui.musicRefreshed == 0) return;
            check(ui.engine.library[0] != oldScanSource && ui.engine.library[0]->analysisSource.sha256 != oldScanSource->analysisSource.sha256
                  && ui.engine.library[0]->manualBpm == 0 && ui.engine.project.clips[0].media == oldScanSource && oldScanSource->manualBpm == 123.4,
                  "scan refresh adopts changed files for future drops while arranged sources and their corrections remain unchanged");
            ui.engine.project.clips.clear(); ui.engine.library[0]->manualBpm = 123.4;
            ui.menuItemSelected(38, 4); check(!ui.musicScanEnabled && !readScanPreference(ui.musicPreference).enabled, "Tools disables automatic scanning and persists the preference");
            source.copyFileTo(scanRoot.getChildFile("later.wav")); ui.menuItemSelected(37, 4); phase = 2;
        }
        else if (phase == 2)
        {
            if (ui.engine.library.size() != 3) return;
            check(!ui.musicScanEnabled && ui.engine.library.size() == 3, "Scan Music folder now performs one pass while automatic scanning stays disabled");
            ui.engine.playing = true; ui.nextMusicScan = 0; ui.musicScanEnabled = true; ui.tickMusicScan();
            check(!ui.musicScan && !ui.musicScanJobPending, "automatic scanning does not start during playback"); ui.engine.playing = false;
            ui.beginMusicScan(); ui.stopMusicScan(); ++phase; return;
        }
        else if (phase == 3)
        {
            check(!ui.musicScan && !ui.musicScanJobPending && ui.engine.library.size() == 3, "Stop rejects a stale scan-construction callback instead of reviving the cancelled scan");
            ui.addMedia(0); ++phase; return;
        }
        else if (phase == 4)
        {
            if (ui.busy || ui.engine.project.clips.empty() || !ui.engine.project.clips[0].beatmatched) return;
            check(ui.engine.library[0]->hasPlayback() && ui.engine.library[0]->decodedFromCache && ui.engine.project.clips[0].media == ui.engine.library[0]
                  && ui.engine.library[0]->manualBpm == 123.4, "adding a scanned catalogue track hydrates its decoded cache on the worker, retains corrections and completes normal tempo preparation");
            const auto metadataSession = folder.getChildFile("catalogue-session.mixworks"); ui.engine.saveProject(metadataSession);
            check(ui.engine.restoreProject(metadataSession.loadFileAsString()).isEmpty() && ui.engine.library[0]->hasPlayback() && !ui.engine.library[1]->hasPlayback()
                  && !ui.engine.library[2]->hasPlayback() && ui.engine.library[1]->frames() > 0, "session reopening hydrates timeline sources and keeps unused library entries lightweight");
            ui.musicScanEnabled = false; ui.musicFolder = folder.getChildFile("unavailable-Music"); ui.menuItemSelected(37, 4); ++phase; return;
        }
        else if (phase == 5)
        {
            check(!ui.musicScan && !ui.musicScanJobPending && !ui.musicScanManual && ui.status.contains("unavailable") && !ui.musicFolder.exists(), "an unavailable Music folder ends a manual scan with a reason without creating folders or enabling repeated scans");
            ui.undoHistory = {largePrepared.project}; ui.redoHistory.clear(); ui.engine.project = firstRamp.project; ui.engine.library = {repaired.media};
            auto revised = firstRamp.project; revised.tempo.points[1].bpm += 0.7654321;
            ui.afterTempoPreparation = [&] { tempoContinued = true; }; ui.prepareTempo(std::move(revised), false); ++phase; return;
        }
        else
        {
            if (ui.busy) return;
            check(tempoContinued && std::abs(ui.engine.project.tempo.points[1].bpm - (143 + 0.7654321)) < 1.0e-7,
                  "native worker commits a new cold tempo edit with over 1 GiB of mapped PCM retained by undo");
            const auto revisedPcm = ui.engine.project.clips[0].prepared;
            ui.undo(false); const bool undone = ui.engine.project.clips[0].prepared == firstRamp.project.clips[0].prepared;
            ui.undo(true);
            check(undone && ui.engine.project.clips[0].prepared == revisedPcm && revisedPcm && revisedPcm->cache && revisedPcm->residentBytes() == 0,
                  "Undo and Redo preserve exact mapped PCM after the large-history tempo edit");
            poll.stopTimer(); juce::MessageManager::getInstance()->stopDispatchLoop();
        }
    };
    poll.startTimer(20); juce::MessageManager::getInstance()->runDispatchLoop();
}
}
