#include "Engine.h"
#include "Beatmatch.h"
#include <cmath>

namespace mixworks
{
void runEditingTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    Engine engine; engine.project.masterDb = 0;
    auto media = std::make_shared<Media>(); media->name = "editing fixture"; media->sampleRate = 44100; media->manualBpm = 120;
    media->samples.setSize(2, 44100);
    for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 44100; ++i) media->samples.setSample(ch, i, (float) i / 44100);
    media->file = report.getSiblingFile("editing-source.wav");
    {
        juce::WavAudioFormat format; std::unique_ptr<juce::OutputStream> stream = media->file.createOutputStream();
        auto writer = format.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(44100).withNumChannels(2).withBitsPerSample(24));
        if (writer) writer->writeFromAudioSampleBuffer(media->samples, 0, 44100);
    }
    engine.library = {media}; Clip clip; clip.media = media; clip.length = 1024.0 / 44100; clip.reversed = true;
    engine.project.clips = {clip}; juce::MidiBuffer midi; juce::AudioBuffer<float> original(2, 512), edited(2, 512);
    engine.render(original, 512, 0, midi);
    check(std::abs(original.getSample(0, 0) - 1023.0 / 44100) < 0.000001, "reverse playback reads the final source sample first");
    auto left = clip.slice(0, 256.0 / 44100), right = clip.slice(256.0 / 44100, 768.0 / 44100);
    engine.project.clips = {left, right}; engine.render(edited, 512, 0, midi);
    bool same = true; for (int i = 0; i < 512; ++i) same = same && std::abs(original.getSample(0, i) - edited.getSample(0, i)) < 0.000001;
    check(same, "splitting a reversed clip preserves actual PCM across the edit");
    clip.reversed = false; clip.loopSpan = 128.0 / 44100; clip.loopPhase = 13.0 / 44100; engine.project.clips = {clip};
    engine.render(original, 512, 0, midi);
    check(std::abs(original.getSample(0, 0) - 13.0 / 44100) < 0.000001 && std::abs(original.getSample(0, 115)) < 0.000001, "loop PCM wraps exactly at its source-section boundary");
    for (bool reversed : {false, true})
    {
        clip.reversed = reversed; engine.project.clips = {clip}; engine.render(original, 512, 0, midi);
        left = clip.slice(0, 257.0 / 44100); right = clip.slice(257.0 / 44100, clip.length - 257.0 / 44100);
        engine.project.clips = {left, right}; engine.render(edited, 512, 0, midi); same = true; float maximum = 0; int worst = 0;
        for (int i = 0; i < 512; ++i) { const auto difference = std::abs(original.getSample(0, i) - edited.getSample(0, i)); if (difference > maximum) { maximum = difference; worst = i; } same = same && difference < 0.000001; }
        check(same, juce::String(reversed ? "reversed" : "forward") + " loop split retains its source phase in actual PCM; max error " + juce::String(maximum, 7) + " at frame " + juce::String(worst));
    }
    clip.loopSpan = 0.5; clip.loopPhase = 0.1; clip.length = 3; clip.offset = 0.1; engine.project.clips = {clip};
    check(clip.firstBeatTime() >= 0 && clip.firstBeatTime() < 0.5, "reversed loop beat grid starts within one beat of the clip beginning");
    auto unused = readMedia(engine.formats, media->file); unused.media->id = "unused-fixture"; unused.media->name = "Unused imported track"; unused.media->manualKey = 17;
    engine.library.push_back(unused.media); engine.project.tracks[0].eqDb = {-6, -3, 2};
    auto saved = juce::JSON::toString(engine.captureProject());
    report.getSiblingFile("editing-roundtrip.mixworks").replaceWithText(saved);
    auto tempoMatched = prepareBeatmatch(engine.project, 144, false, 0);
    check(tempoMatched.error.isEmpty() && std::abs(tempoMatched.project.clips[0].loopSpan - 0.5) < 1.0e-8 && std::abs(tempoMatched.project.clips[0].loopPhase - 0.1) < 1.0e-8
          && tempoMatched.project.clips[0].reversed, "tempo preparation retains loop source span, phase and reverse state");
    check(engine.restoreProject(saved).isEmpty() && engine.library.size() == 2 && engine.library[1]->name == "Unused imported track" && engine.library[1]->manualKey == 17,
          "session persists unused library entries, order and manual metadata");
    check(engine.project.clips[0].reversed && std::abs(engine.project.clips[0].loopPhase - 0.1) < 1.0e-8 && engine.project.tracks[0].eqDb == std::array<float, 3>{-6, -3, 2},
          "reverse, loop span/phase and EQ survive session reopening");
    engine.resetPlugins(); engine.render(original, 512, 0, midi);
    auto rendered = report.getSiblingFile("editing-render.wav"); check(engine.exportWav(rendered).isEmpty(), "reverse, looping and built-in EQ render through WAV export");
    std::unique_ptr<juce::AudioFormatReader> reader(engine.formats.createReaderFor(rendered));
    same = reader && reader->lengthInSamples == 132300;
    if (reader) { reader->read(&edited, 0, 512, 0, true, true); for (int i = 0; i < 512; ++i) same = same && std::abs(edited.getSample(0, i) - original.getSample(0, i)) < 0.00001; }
    check(same, "offline EQ/loop/reverse PCM matches live rendering from reset state");
    auto corrupted = juce::JSON::parse(saved); corrupted["clips"].getArray()->getReference(0).getDynamicObject()->setProperty("mediaId", "missing-source");
    check(engine.restoreProject(juce::JSON::toString(corrupted)).isNotEmpty() && engine.library.size() == 2, "dangling source IDs cannot replace a valid session");
    corrupted = juce::JSON::parse(saved); corrupted["clips"].getArray()->getReference(0).getDynamicObject()->setProperty("loopSpan", 1.0e-20); corrupted["clips"].getArray()->getReference(0).getDynamicObject()->setProperty("loopPhase", 0);
    check(engine.restoreProject(juce::JSON::toString(corrupted)).isNotEmpty(), "loop spans smaller than one source sample are rejected before playback");
    corrupted = juce::JSON::parse(saved); corrupted["media"].getArray()->getReference(1).getDynamicObject()->setProperty("file", report.getSiblingFile("absent-owned-test-audio.wav").getFullPathName());
    check(engine.restoreProject(juce::JSON::toString(corrupted)).isEmpty() && engine.library.size() == 2
          && engine.library[1]->id == "unused-fixture" && engine.library[1]->manualKey == 17
          && engine.library[1]->file.getFileName() == "absent-owned-test-audio.wav" && engine.playbackReady(),
          "unused missing audio cannot block playable clips or discard library metadata");
    if (engine.project.clips.size() == 1)
    {
        auto secondClip = engine.project.clips[0]; secondClip.id = juce::Uuid().toString(); secondClip.start = secondClip.end();
        engine.project.clips.push_back(secondClip);
        const auto playable = report.getSiblingFile("A02-two-valid-clips.mixworks");
        const auto opened = engine.saveProject(playable).isEmpty() && engine.restoreProject(playable.loadFileAsString()).isEmpty();
        const auto exported = report.getSiblingFile("A02-two-valid-clips.wav"); const auto exportError = engine.exportWav(exported);
        std::unique_ptr<juce::AudioFormatReader> validReader(engine.formats.createReaderFor(exported));
        check(opened && engine.project.clips.size() == 2 && exportError.isEmpty() && validReader && validReader->lengthInSamples == 264600,
            "two valid clips save, reopen and export six seconds despite an unused missing library source");
    }
    auto deferred = juce::JSON::parse(saved);
    for (int i = 0; i < 358; ++i)
    {
        auto entry = juce::JSON::parse(juce::JSON::toString(deferred["media"].getArray()->getReference(1)));
        entry.getDynamicObject()->setProperty("id", "deferred-" + juce::String(i));
        entry.getDynamicObject()->setProperty("file", report.getSiblingFile("unavailable-" + juce::String(i) + ".wav").getFullPathName());
        deferred["media"].getArray()->add(entry);
    }
    int sourceChecks = 0; juce::StringArray inspected;
    auto staged = engine.prepareProject(juce::JSON::toString(deferred), [&](int, int, const auto& name, auto phase)
    { if (phase == MediaReadStage::lookupAnalysis) { ++sourceChecks; inspected.addIfNotAlreadyThere(name); } });
    check(staged.error.isEmpty() && staged.library.size() == 360 && staged.parsed.project.clips.size() == 1
          && inspected.size() == 1 && sourceChecks == 2 && staged.library[0]->analysisFromCache
          && !staged.library[1]->hasPlayback() && !staged.library.back()->hasPlayback(),
          "arrangement readiness reads only required audio with 359 deferred catalogue entries and reuses valid analysis");
    check(staged.error.isEmpty() && engine.commitProject(std::move(staged)).isEmpty() && engine.playbackReady()
          && engine.captureProject()["media"].getArray()->size() == 360,
          "all deferred catalogue identities survive transactional admission and capture");
    auto required = juce::JSON::parse(saved);
    const auto missingPath = report.getSiblingFile("required-missing.wav").getFullPathName();
    required["media"].getArray()->getReference(0).getDynamicObject()->setProperty("file", missingPath);
    const auto kept = juce::JSON::toString(engine.captureProject());
    const auto requiredError = engine.restoreProject(juce::JSON::toString(required));
    check(requiredError.contains(missingPath) && requiredError.containsIgnoreCase("missing")
          && juce::JSON::toString(engine.captureProject()) == kept,
          "required missing audio reports its full path and keeps the complete current document");
    const auto failed = engine.prepareProject(juce::JSON::toString(required));
    const auto located = relinkProjectSource(failed.parsed, media->id, media->file);
    check(failed.failedSource && failed.failedSource->id == media->id && !located.isEmpty()
          && engine.restoreProject(located).isEmpty() && engine.playbackReady()
          && failed.parsed.document["media"].getArray()->getReference(0)["file"].toString() == missingPath,
          "explicit relink retries the required source without mutating the failed document");
    const auto keptAfterRelink = juce::JSON::toString(engine.captureProject());
    const auto badAudio = report.getSiblingFile("required-corrupt.wav"); badAudio.replaceWithText("not audio");
    required["media"].getArray()->getReference(0).getDynamicObject()->setProperty("file", badAudio.getFullPathName());
    const auto corruptError = engine.restoreProject(juce::JSON::toString(required));
    check(corruptError.contains(badAudio.getFullPathName()) && corruptError.containsIgnoreCase("corrupt")
          && juce::JSON::toString(engine.captureProject()) == keptAfterRelink, "required corrupt audio fails atomically with its full path");
    corrupted["media"].getArray()->getReference(1).getDynamicObject()->setProperty("file", badAudio.getFullPathName());
    check(engine.restoreProject(juce::JSON::toString(corrupted)).isEmpty() && engine.playbackReady(),
          "unused corrupt audio is deferred without rejecting the arrangement");
    const auto unsupported = report.getSiblingFile("unsupported-source.bin"); unsupported.replaceWithText("not an audio format");
    required["media"].getArray()->getReference(0).getDynamicObject()->setProperty("file", unsupported.getFullPathName());
    const auto unsupportedBefore = juce::JSON::toString(engine.captureProject());
    const auto unsupportedError = engine.restoreProject(juce::JSON::toString(required));
    check(unsupportedError.containsIgnoreCase("Unsupported audio format") && unsupportedError.contains(unsupported.getFullPathName())
        && juce::JSON::toString(engine.captureProject()) == unsupportedBefore, "unsupported required files remain distinct from missing and corrupt audio");
    auto cancelled = std::make_shared<std::atomic<bool>>(true);
    check(engine.prepareProject(saved, {}, cancelled).error.isNotEmpty() && engine.library.size() == 2, "cancelled session loading keeps the current session");
    *cancelled = false;
    const auto midCancelled = engine.prepareProject(saved, [&](int, int, const auto&, auto) { *cancelled = true; }, cancelled);
    check(midCancelled.error.containsIgnoreCase("cancel") && !midCancelled.failedSource && engine.commitProject(midCancelled).isNotEmpty()
        && juce::JSON::toString(engine.captureProject()) == unsupportedBefore, "mid-read cancellation cannot commit or request source relinking");
    for (int version : {1, 2})
    {
        auto legacy = juce::JSON::parse(saved); legacy.getDynamicObject()->setProperty("version", version); legacy["tracks"].getArray()->removeRange(4, trackCount - 4);
        auto& c = legacy["clips"].getArray()->getReference(0); auto* object = c.getDynamicObject(); const auto& m = legacy["media"].getArray()->getReference(0);
        for (const char* field : {"file", "name", "manualBpm", "manualBeatOrigin", "manualKey"}) object->setProperty(field, m[field]);
        object->setProperty("bpm", 120); object->setProperty("length", 0.5); object->setProperty("offset", 0);
        check(engine.restoreProject(juce::JSON::toString(legacy)).isEmpty() && !engine.project.clips[0].reversed && engine.project.clips[0].loopSpan == 0,
              "version " + juce::String(version) + " sessions migrate to canonical media/clip facts");
    }
    // Measure each EQ band through the production mixer, away from filter startup transients.
    engine.project = Project(); engine.project.masterDb = 0; media->samples.setSize(2, 44100 * 2); clip = Clip(); clip.media = media; clip.length = 2;
    engine.project.clips = {clip};
    for (int band = 0; band < 3; ++band)
    {
        const double frequency = band == 0 ? 30.0 : band == 1 ? 1000.0 : 16000.0;
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < media->samples.getNumSamples(); ++i) media->samples.setSample(ch, i, (float) (0.2 * std::sin(2 * juce::MathConstants<double>::pi * frequency * i / 44100)));
        engine.project.tracks[0].eqDb.fill(0); engine.resetPlugins(); double dry = 0, wet = 0;
        for (int block = 0; block < 80; ++block) { engine.render(original, 512, block * 512.0 / 44100, midi); if (block >= 40) for (int i = 0; i < 512; ++i) dry += std::pow(original.getSample(0, i), 2); }
        engine.project.tracks[0].eqDb[(size_t) band] = -12; engine.resetPlugins();
        for (int block = 0; block < 80; ++block) { engine.render(original, 512, block * 512.0 / 44100, midi); if (block >= 40) for (int i = 0; i < 512; ++i) wet += std::pow(original.getSample(0, i), 2); }
        double attenuation = 10 * std::log10(wet / dry);
        check(std::abs(attenuation + 12) < 1.5, "built-in EQ band " + juce::String(band + 1) + " produces measured attenuation " + juce::String(attenuation, 2) + " dB");
    }
}
}
