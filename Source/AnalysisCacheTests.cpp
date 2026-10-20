#include "Engine.h"
#include "ContentDigest.h"
#include <juce_cryptography/juce_cryptography.h>
#include <cmath>

namespace mixworks
{
void runAnalysisCacheTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    const char abc[] = "abc";
    juce::MemoryInputStream known(abc, 3, false);
    check(contentDigest(known) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
          "accelerated digest matches the published SHA-256 abc vector");
    bool boundaries = true;
    for (size_t size : {size_t(0), size_t(1), size_t(55), size_t(56), size_t(63), size_t(64), size_t(65), size_t(262143), size_t(262144), size_t(262145)})
    {
        std::vector<unsigned char> bytes(size);
        for (size_t i = 0; i < size; ++i) bytes[i] = (unsigned char) (i * 131 + i / 7);
        juce::MemoryInputStream input(bytes.data(), size, false);
        boundaries &= contentDigest(input) == juce::SHA256(bytes.data(), size).toHexString() && input.getPosition() == (int64_t) size;
    }
    check(boundaries, "accelerated digest preserves every byte across SHA padding and streaming block boundaries");
    struct ShortReads final : juce::MemoryInputStream
    {
        ShortReads(const void* bytes, size_t size) : MemoryInputStream(bytes, size, false) {}
        int read(void* dest, int count) override { return MemoryInputStream::read(dest, std::min(7, count)); }
    } shortReads("0123456789abcdef", 16);
    check(shortReads.setPosition(1) && contentDigest(shortReads) == juce::SHA256("123456789abcdef", 15).toHexString(),
          "digest consumes only the remaining stream through short reads");
    struct FailedRead final : juce::MemoryInputStream
    {
        FailedRead(const void* bytes, size_t size) : MemoryInputStream(bytes, size, false) {}
        int read(void*, int) override { return 0; }
    } failedRead(abc, 3);
    check(contentDigest(failedRead).isEmpty(), "incomplete input cannot publish the hash of a partial or empty prefix");
    const auto folder = report.getSiblingFile("analysis-record-tests-" + juce::Uuid().toString()); folder.createDirectory();
    const auto audio = folder.getChildFile("track.wav");
    { juce::AudioBuffer<float> pcm(2, 8000 * 4); pcm.clear();
      for (int i = 0; i < pcm.getNumSamples(); ++i) for (int ch = 0; ch < 2; ++ch)
          pcm.setSample(ch, i, (float) (0.2 * std::sin(i * 0.09) * std::exp(-(i % 4000) / 400.0)));
      juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream = audio.createOutputStream();
      auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(2).withBitsPerSample(24));
      check(writer && writer->writeFromAudioSampleBuffer(pcm, 0, pcm.getNumSamples()), "analysis cache fixture owns its audio"); }
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    int analysed = 0, reused = 0;
    auto progress = [&](MediaReadStage phase) { if (phase == MediaReadStage::analyse) ++analysed; if (phase == MediaReadStage::savedAnalysis) ++reused; };
    const double coldStart = juce::Time::getMillisecondCounterHiRes(); auto first = readMedia(formats, audio, progress);
    const double coldMs = juce::Time::getMillisecondCounterHiRes() - coldStart;
    const auto recordFile = analysisRecordFile(audio);
    check(recordFile.getParentDirectory() != audio.getParentDirectory(), "analysis storage stays outside the music directory");
    check(first.media && analysed == 1 && recordFile.existsAsFile() && first.media->analysisCacheWarning.isEmpty(), "cold production import saves analysis in the application store");
    if (!first.media) return;
    const auto originalRecord = recordFile.loadFileAsString(), originalDigest = audioDigest(audio);
    const double warmStart = juce::Time::getMillisecondCounterHiRes(); auto second = readMedia(formats, audio, progress);
    const double warmMs = juce::Time::getMillisecondCounterHiRes() - warmStart;
    check(second.media && second.media->analysisFromCache && analysed == 1 && reused == 1, "a second production import bypasses detector analysis; cold " + juce::String(coldMs, 1) + " ms / warm " + juce::String(warmMs, 1) + " ms");
    if (!second.media) return;
    const auto& a = first.media->analysis; const auto& b = second.media->analysis;
    auto near = [](double x, double y) { return std::abs(x - y) < 1.0e-12; };
    bool same = near(a.bpm, b.bpm) && near(a.beatOrigin, b.beatOrigin) && near(a.tempoConfidence, b.tempoConfidence) && near(a.gridErrorMs, b.gridErrorMs)
        && near(a.downbeatOrigin, b.downbeatOrigin) && near(a.downbeatConfidence, b.downbeatConfidence) && near(a.downbeatExaminedSeconds, b.downbeatExaminedSeconds)
        && a.key == b.key && a.alternativeKey == b.alternativeKey && near(a.keyConfidence, b.keyConfidence) && near(a.tuningCents, b.tuningCents)
        && near(a.examinedSeconds, b.examinedSeconds) && a.variableTempo == b.variableTempo && a.beatWindows.size() == b.beatWindows.size() && first.media->peaks == second.media->peaks
        && a.rhythmMethod == b.rhythmMethod && a.rhythmIssue == b.rhythmIssue && near(a.neuralPhaseCorrectionMs, b.neuralPhaseCorrectionMs);
    for (size_t i = 0; same && i < a.beatWindows.size(); ++i) same = near(a.beatWindows[i].sourceStart, b.beatWindows[i].sourceStart) && near(a.beatWindows[i].bpm, b.beatWindows[i].bpm) && near(a.beatWindows[i].origin, b.beatWindows[i].origin) && near(a.beatWindows[i].confidence, b.beatWindows[i].confidence);
    check(same && recordFile.loadFileAsString() == originalRecord && !recordFile.getSiblingFile(recordFile.getFileName() + ".bak").exists(), "warm import retains all measured facts and waveform buckets without rewriting the record");
    auto unavailable = first.media->analysisRecord(); unavailable.manualBpm = 126.75; unavailable.manualKey = 17;
    unavailable.analysis.rhythmMethod = "unavailable"; unavailable.analysis.rhythmIssue = "Missing model";
    const auto unavailableRead = parseAnalysisRecord(analysisRecordToJson(unavailable), unavailable.source);
    check(unavailableRead.record && !unavailableRead.reusableAutomatic && unavailableRead.record->manualBpm == 126.75 && unavailableRead.record->manualKey == 17,
          "an unavailable model cannot become a reusable automatic cache result; independent corrections survive");
    check(writeAnalysisRecord(audio, first.media->analysisRecord()).isEmpty() && recordFile.loadFileAsString() == originalRecord
          && !recordFile.getSiblingFile(recordFile.getFileName() + ".bak").exists(), "saving identical analysis is idempotent across file newline conventions");
    const auto futureAudio = folder.getChildFile("future-detector.wav"); audio.copyFileTo(futureAudio);
    auto futureRecord = first.media->analysisRecord(); futureRecord.revision = analysisRevision + 1;
    check(writeAnalysisRecord(futureAudio, futureRecord).isEmpty(), "future detector fixture is saved");
    const auto futureFile = analysisRecordFile(futureAudio); const auto futureBytes = futureFile.loadFileAsString();
    check(writeAnalysisRecord(futureAudio, first.media->analysisRecord()).isNotEmpty() && futureFile.loadFileAsString() == futureBytes,
          "an older detector cannot overwrite a newer analysis record");
    const auto existingBackup = recordFile.getSiblingFile(recordFile.getFileName() + ".bak"); existingBackup.replaceWithText("preserve existing backup");
    first.media->manualBpm = 126.75; first.media->manualBeatOrigin = 0.213; first.media->manualKey = 17;
    check(writeAnalysisRecord(audio, first.media->analysisRecord()).isNotEmpty() && recordFile.loadFileAsString() == originalRecord
          && existingBackup.loadFileAsString() == "preserve existing backup", "an unidentified backup is preserved without numbered backup growth or replacing the current record");
    existingBackup.deleteFile(); // Only this test's sentinel, never a user's backup.
    check(writeAnalysisRecord(audio, first.media->analysisRecord()).isEmpty(), "manual BPM, source beat and key corrections persist in the application store");
    auto corrected = readMedia(formats, audio, progress);
    check(corrected.media && corrected.media->analysisFromCache && corrected.media->manualBpm == 126.75 && corrected.media->manualBeatOrigin == 0.213 && corrected.media->manualKey == 17, "new imports reuse saved manual corrections as well as automatic analysis");
    bool updatesSaved = true;
    for (int i = 0; i < 6; ++i) { auto edited = first.media->analysisRecord(); edited.manualBpm += i; updatesSaved = writeAnalysisRecord(audio, edited).isEmpty() && updatesSaved; }
    updatesSaved = writeAnalysisRecord(audio, first.media->analysisRecord()).isEmpty() && updatesSaved;
    check(updatesSaved && recordFile.getParentDirectory().findChildFiles(juce::File::findFiles, false, recordFile.getFileName() + "*.bak").size() == 1
          && folder.findChildFiles(juce::File::findFiles, false, "*.bak").isEmpty(), "repeated corrections retain one previous record without adding backups to Music");
    const auto legacyAudio = folder.getChildFile("legacy.wav"); audio.copyFileTo(legacyAudio);
    const auto legacyFile = legacyAnalysisRecordFile(legacyAudio); legacyFile.replaceWithText(juce::JSON::toString(analysisRecordToJson(first.media->analysisRecord())));
    const auto legacyText = legacyFile.loadFileAsString();
    auto legacyReadOnly = readMedia(formats, legacyAudio, {}, AnalysisCacheMode::readOnly, MediaReadPurpose::catalogue);
    check(legacyReadOnly.media && legacyReadOnly.media->analysisFromCache && !analysisRecordFile(legacyAudio).exists()
          && legacyFile.loadFileAsString() == legacyText, "read-only legacy reuse does not migrate or change source records");
    auto migrated = readMedia(formats, legacyAudio, {}, AnalysisCacheMode::readWrite, MediaReadPurpose::catalogue);
    check(migrated.media && migrated.media->analysisFromCache && migrated.media->manualBpm == 126.75 && migrated.media->manualKey == 17
          && analysisRecordFile(legacyAudio).existsAsFile() && !readAnalysisRecord(legacyAudio, first.media->analysisSource).legacyLocation
          && legacyFile.loadFileAsString() == legacyText, "legacy reuse migrates verified analysis and corrections without rewriting the legacy file");
    Engine engine; second.media->manualBpm = 0; second.media->manualKey = -1; engine.library = {second.media};
    const auto saved = juce::JSON::toString(engine.captureProject());
    check(engine.restoreProject(saved).isEmpty() && engine.library[0]->analysisFromCache && engine.library[0]->manualBpm == 0 && engine.library[0]->manualKey == -1,
          "session reopen uses saved analysis while explicit session corrections remain authoritative");
    const auto time = audio.getLastModificationTime(); const auto size = audio.getSize();
    { auto stream = audio.createOutputStream(); stream->setPosition(size - 1); stream->writeByte(1); stream->flush(); }
    audio.setLastModificationTime(time);
    auto changed = readMedia(formats, audio, progress);
    check(changed.media && !changed.media->analysisFromCache && audio.getSize() == size && audioDigest(audio) != originalDigest && analysed == 2,
          "content hashing invalidates changed audio even when its size and modification time are retained");
    check(recordFile.getSiblingFile(recordFile.getFileName() + ".bak").existsAsFile(), "replacing an owned stale analysis record retains a backup");
    auto corrupt = juce::JSON::parse(recordFile); corrupt["analysis"].getDynamicObject()->setProperty("bpm", "120"); recordFile.replaceWithText(juce::JSON::toString(corrupt));
    auto malformed = readMedia(formats, audio, progress);
    check(malformed.media && !malformed.media->analysisFromCache && malformed.media->analysisCacheWarning.isEmpty() && analysed == 3, "malformed typed analysis is rejected and an owned record is rebuilt safely");
    auto stale = juce::JSON::parse(recordFile); stale.getDynamicObject()->setProperty("analysisRevision", 0); stale["manual"].getDynamicObject()->setProperty("bpm", 127.5); recordFile.replaceWithText(juce::JSON::toString(stale));
    auto revised = readMedia(formats, audio, progress);
    check(revised.media && !revised.media->analysisFromCache && analysed == 4, "detector revision changes invalidate previous analysis");
    check(revised.media && revised.media->manualBpm == 127.5, "recomputing stale automatic analysis retains independently valid manual corrections");
    auto future = juce::JSON::parse(recordFile); future.getDynamicObject()->setProperty("version", 999); const auto futureText = juce::JSON::toString(future); recordFile.replaceWithText(futureText);
    auto newer = readMedia(formats, audio, progress);
    check(newer.media && !newer.media->analysisFromCache && newer.media->analysisCacheWarning.isNotEmpty() && recordFile.loadFileAsString() == futureText, "a newer record is left intact while audio remains playable");
    const auto foreignText = juce::String("{\"format\":\"another-program\",\"keep\":true}"); recordFile.replaceWithText(foreignText);
    auto foreign = readMedia(formats, audio, progress);
    check(foreign.media && foreign.media->analysisCacheWarning.isNotEmpty() && recordFile.loadFileAsString() == foreignText, "a foreign file at the sidecar path is never overwritten or mistaken for analysis");
    recordFile.replaceWithText("{truncated"); auto truncated = readMedia(formats, audio, progress);
    check(truncated.media && !truncated.media->analysisFromCache && truncated.media->analysisCacheWarning.isNotEmpty() && recordFile.loadFileAsString() == "{truncated", "unidentified truncated records cannot replace or block valid playback");
    const auto readOnlyAudio = folder.getChildFile("read-only-analysis.wav"); audio.copyFileTo(readOnlyAudio);
    auto readOnly = readMedia(formats, readOnlyAudio, {}, AnalysisCacheMode::readOnly);
    check(readOnly.media && !analysisRecordFile(readOnlyAudio).exists(), "read-only analysis workflows do not create records beside input audio");
    const auto blockedAudio = folder.getChildFile("blocked-record.wav"); audio.copyFileTo(blockedAudio); analysisRecordFile(blockedAudio).createDirectory();
    auto blocked = readMedia(formats, blockedAudio);
    check(blocked.media && blocked.media->analysisCacheWarning.isNotEmpty() && analysisRecordFile(blockedAudio).isDirectory(), "an unwritable record destination reports a nonfatal warning without changing it");
    const auto oversizedAudio = folder.getChildFile("oversized-record.wav"); audio.copyFileTo(oversizedAudio); const auto oversizedRecord = analysisRecordFile(oversizedAudio);
    { auto stream = oversizedRecord.createOutputStream(); stream->setPosition(1024 * 1024); stream->writeByte(0); }
    auto oversized = readMedia(formats, oversizedAudio);
    check(oversized.media && !oversized.media->analysisFromCache && oversized.media->analysisCacheWarning.isNotEmpty() && oversizedRecord.getSize() == 1024 * 1024 + 1,
          "oversized analysis records are rejected before parsing and retained without blocking playback");
    const auto silent = folder.getChildFile("silence.wav");
    { juce::AudioBuffer<float> samples(2, 44100); samples.clear(); juce::WavAudioFormat format; std::unique_ptr<juce::OutputStream> stream = silent.createOutputStream(); auto writer = format.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(44100).withNumChannels(2).withBitsPerSample(24)); if (writer) writer->writeFromAudioSampleBuffer(samples, 0, samples.getNumSamples()); }
    auto unknown = readMedia(formats, silent); auto unknownAgain = readMedia(formats, silent);
    check(unknown.media && unknownAgain.media && unknownAgain.media->analysisFromCache && unknownAgain.media->bpm() == 0 && unknownAgain.media->key() == -1,
          "unknown BPM and key remain explicit through persistent analysis reuse");
    check(analysisRecordFile(folder.getChildFile("track.wav")) != analysisRecordFile(folder.getChildFile("track.flac")), "same-name tracks with different extensions own distinct analysis records");
}
}
