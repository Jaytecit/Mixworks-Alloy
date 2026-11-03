#include "EarTools.h"
#include "Workstation.h"
#include "GridCorrectionUI.h"
#include "Beatmatch.h"
#include <cmath>
#include <limits>
#include <set>
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace mixworks
{
void runEarToolTests(const std::function<void(bool,const juce::String&)>& check, const juce::File& report)
{
    const auto frequency = [](const float* samples, int frames, double rate)
    {
        int crossings = 0; const int first = (int) (.3 * rate), last = std::min(frames, (int) (1.8 * rate));
        for (int i = first + 1; i < last; ++i) if (samples[i - 1] <= 0 && samples[i] > 0) ++crossings;
        return crossings * rate / std::max(1, last - first);
    };
    EarKeyChoice choice(14); std::set<int> heard;
    for (int i = 0; i < 11; ++i) { heard.insert(choice.candidate(1)); heard.insert(choice.candidate(2)); choice.prefer(1); }
    check(heard.size() == 12 && choice.stage == 1 && choice.selected == -1, "guided home-sound comparison exposes all12 tones without auto-saving a key");
    choice.prefer(2); check(choice.selected == 14 && choice.stage == 2, "major/minor choice is explicit and can retain a minor home key");
    choice.prefer(1); check(choice.selected == 2, "review can change the chosen key character");
    const auto fixture = [&](const juce::String& name, double hz)
    {
        auto media = std::make_shared<Media>(); media->name = name; media->sampleRate = 44100;
        media->file = report.getSiblingFile(name + ".wav"); media->samples.setSize(2, 44100 * 4);
        for (int i = 0; i < media->frames(); ++i)
        { const float value = (float) (.12 * std::sin(juce::MathConstants<double>::twoPi * hz * i / 44100)); media->samples.setSample(0, i, value); media->samples.setSample(1, i, value); }
        { juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream = media->file.createOutputStream();
          auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(44100).withNumChannels(2).withBitsPerSample(24));
          check(writer && writer->writeFromAudioSampleBuffer(media->samples, 0, media->frames()), "write tiny owned ear-tool fixture / " + name); }
        media->analysisSource = {audioDigest(media->file), media->file.getSize(), 44100, media->frames(), 2};
        media->analysis.bpm = 120; media->analysis.beatOrigin = .125; media->analysis.downbeatOrigin = .125;
        media->analysis.key = 0; media->analysis.keyConfidence = .8; media->analysis.tempoConfidence = .9;
        media->prepareWaveformBody(); media->peaks.assign(media->waveformRms.size(), .12f);
        check(writeAnalysisRecord(media->file, media->analysisRecord()).isEmpty(), "save owned analysis without running a detector / " + name);
        return media;
    };
    auto a = fixture("ear-reference", 330), b = fixture("ear-adjusted", 440);
    const auto hashA = audioDigest(a->file), hashB = audioDigest(b->file);
    auto shifted = stretchAudio(*b, 1, maxPreparedBytes, 3);
    check(shifted.error.isEmpty() && shifted.audio && shifted.audio->frames() == b->frames(), "pitch preparation preserves exact duration at unchanged tempo");
    if (!shifted.audio) return;
    check(std::abs(frequency(shifted.audio->channelData(0), shifted.audio->frames(), 44100) - 440 * std::pow(2., 3. / 12)) < 3,
          "independent frequency oracle: +3 steps shifts440 Hz to523 Hz");
    check(shifted.audio->pitchSemitones == 3 && shifted.audio->rate == 1, "prepared pitch identity is separate from source tempo");
    auto repeated = stretchAudio(*b, 1, maxPreparedBytes, 3);
    check(repeated.error.isEmpty() && repeated.audio && repeated.audio->fromDiskCache, "same pitch reuses its exact prepared cache");
    auto unshifted = stretchAudio(*b, 1, maxPreparedBytes);
    check(unshifted.error.isEmpty() && !unshifted.audio, "zero-pitch unity still uses original PCM without invalidating existing caches");
    auto lower = stretchAudio(*b, 1.1, maxPreparedBytes, -2.5);
    check(lower.error.isEmpty() && lower.audio && std::abs(lower.audio->frames() - b->frames() / 1.1) <= 1
        && std::abs(frequency(lower.audio->channelData(0), lower.audio->frames(), 44100) - 440 * std::pow(2., -2.5 / 12)) < 3,
          "tempo and fractional pitch are independent and preserve their expected duration/frequency");
    check(stretchAudio(*b, 1, maxPreparedBytes, std::numeric_limits<double>::quiet_NaN()).error.isNotEmpty()
        && stretchAudio(*b, 1, maxPreparedBytes, 7).error.isNotEmpty(), "invalid pitch is rejected before DSP");

    auto ui = std::make_unique<Workstation>(false); ui->setSize(1250, 800); ui->engine.library = {a, b};
    Clip reference; reference.media = a; reference.length = 4;
    Clip target; target.media = b; target.track = 1; target.length = 4;
    ui->engine.project.followTrackTempo = false; ui->engine.project.clips = {reference, target}; ui->selectedClip = target.id; ui->refresh();
    auto prepared = target; prepared.pitchSemitones = 3; prepared.prepared = shifted.audio;
    const auto before = juce::JSON::toString(projectToJson(ui->engine.project), true, 17);
    auto bad = prepared; bad.prepared.reset();
    check(commitHarmony(ui->engine, before, bad, [] {}).failed(), "unprepared pitch cannot be applied to the programme");
    int remembers = 0;
    check(commitHarmony(ui->engine, before, prepared, [&] { ++remembers; ui->remember(); }).wasOk()
        && remembers == 1 && ui->engine.project.clips[1].pitchSemitones == 3 && ui->engine.project.clips[0].pitchSemitones == 0,
          "Apply commits pitch to only the original selected segment with one Undo");
    check(ui->engine.project.clips[1].start == 0 && ui->engine.project.clips[1].length == 4 && b->manualKey == -1 && b->analysis.key == 0,
          "segment pitch preserves placement, duration and source key identity");
    check(commitHarmony(ui->engine, before, prepared, [&] { ++remembers; }).failed() && remembers == 1, "stale arrangement comparisons cannot retarget or overwrite edits");
    auto document = projectToJson(ui->engine.project); const auto parsed = parseProject(juce::JSON::toString(document));
    check(parsed.error.isEmpty(), "current pitch document parses: " + parsed.error);
    check(parsed.clips.size() == 2 && parsed.clips[1].clip.pitchSemitones == 3, "saved segment pitch remains separate from source facts");
    check((int) document["version"] == projectVersion, "pitch document uses the current session writer version");
    for (const auto& value : {juce::var("2"), juce::var(7), juce::var(true)})
    {
        auto invalid = document.clone(); (*invalid["clips"].getArray())[1].getDynamicObject()->setProperty("pitchSemitones", value);
        check(parseProject(juce::JSON::toString(invalid)).error.isNotEmpty(), "malformed saved pitch rejects the session");
    }
    ui->undo(false); check(ui->engine.project.clips[1].pitchSemitones == 0, "arrangement Undo restores original pitch");
    ui->undo(true); check(ui->engine.project.clips[1].pitchSemitones == 3, "arrangement Redo restores chosen pitch and PCM");
    const auto session = report.getSiblingFile("ear-pitch.mixworks");
    check(ui->engine.saveProject(session).isEmpty() && ui->engine.restoreProject(session.loadFileAsString()).isEmpty()
        && ui->engine.project.clips[1].pitchSemitones == 3, "actual session save/reopen prepares and restores pitched audio");
    ui->engine.project.clips.erase(ui->engine.project.clips.begin());
    const auto exported = report.getSiblingFile("ear-pitch-export.wav"); check(ui->engine.exportWav(exported).isEmpty(), "pitched segment exports through the real WAV path");
    juce::AudioFormatManager formats; formats.registerBasicFormats(); std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(exported));
    juce::AudioBuffer<float> wav(2, 88200); wav.clear(); if (reader) reader->read(&wav, 0, wav.getNumSamples(), 0, true, true);
    check(reader && std::abs(frequency(wav.getReadPointer(0), wav.getNumSamples(), reader->sampleRate) - 440 * std::pow(2., 3. / 12)) < 3,
          "WAV export contains the applied pitch, not an audition-only effect");
    ui->engine.project.clips = {reference, target};
    Project ramp; ramp.tempo.points = {{0,120,true},{4,140,false}}; auto mapped = target; mapped.pitchSemitones = 2;
    mapped.length = ramp.tempo.timeAtBeat(8); mapped.rate = 4 / mapped.length; mapped.tempoSourceBpm = 120;
    mapped.tempoMap = std::make_shared<const TempoMap>(ramp.tempo); mapped.beatmatched = true; ramp.clips = {mapped};
    check(prepareSavedClips(ramp).isEmpty() && ramp.clips[0].prepared && ramp.clips[0].prepared->clipLocal
        && ramp.clips[0].prepared->geometry.pitchSemitones == 2, "tempo-ramp preparation owns pitch in its geometry/cache identity");
    if (ramp.clips[0].prepared) check(std::abs(frequency(ramp.clips[0].prepared->channelData(0), ramp.clips[0].prepared->frames(), 44100)
        - 440 * std::pow(2., 2. / 12)) < 5, "pitch remains independent of a changing arrangement tempo");

    EarLoopPlayer player; player.source(b, .2, 2.2); player.reference(0, false, 3, -12); player.hear(0); player.level(-3); player.prepareToPlay(512, 48000);
    juce::AudioBuffer<float> loop(2, 48000 * 3); player.getNextAudioBlock({&loop, 0, loop.getNumSamples()});
    check(std::abs(frequency(loop.getReadPointer(0), loop.getNumSamples(), 48000) - 440) < 3, "source listening resamples and loops original pitch independently of the arrangement");
    player.hear(1); player.getNextAudioBlock({&loop, 0, loop.getNumSamples()});
    check(std::abs(frequency(loop.getReadPointer(0), loop.getNumSamples(), 48000) - 130.8128) < 3, "reference-only mode synthesises the chosen home tone");
    player.hear(2); player.getNextAudioBlock({&loop, 0, loop.getNumSamples()}); check(loop.getMagnitude(0, loop.getNumSamples()) > .05, "Together combines source and reference at controlled listening levels");
    check(ui->libraryPreview.playPrepared(player, "ear-owned-test").failed() && !ui->engine.hasLibraryPreview(player), "master listening without an output device is refused safely");
    ui->libraryPreview.stop(); check(!ui->engine.hasLibraryPreview(player), "Stop detaches prepared master audio before its owner is destroyed");
    ui->engine.workMode = WorkMode::recordPerform; ui->libraryPreview.setPrivateOutput(false);
    check(ui->libraryPreview.playPrepared(player, "ear-no-fallback").failed() && !ui->engine.hasLibraryPreview(player), "Record/Perform cannot send prepared listening audio to master");
    ui->libraryPreview.setPrivateOutput(true); check(ui->libraryPreview.playPrepared(player, "ear-missing-route").failed(), "missing headphones fail without audience fallback");
    ui->engine.workMode = WorkMode::arrange; ui->libraryPreview.setPrivateOutput(false);

    const auto pump = [&]
    {
       #if JUCE_WINDOWS
        MSG message; while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
       #endif
    };
    const auto capture = [&](juce::Component& component, const juce::String& name)
    {
        auto stream = report.getSiblingFile(name + ".png").createOutputStream(); juce::PNGImageFormat png;
        check(stream && png.writeImageToStream(component.createComponentSnapshot(component.getLocalBounds()), *stream), "capture actual listening UI / " + name);
    };
    ui->selectedLibraryId = b->id; ui->engine.library = {a, b}; ui->editAnalysis(b->id);
    auto* grid = dynamic_cast<GridCorrectionEditor*>(ui->gridWindow->getContentComponent());
    check(grid != nullptr, "source corrections remain available beside key listening");
    if (!grid) return;
    for (auto* child : grid->getChildren()) if (auto* button=dynamic_cast<juce::TextButton*>(child))
        if (button->getButtonText().contains("Advanced")) { button->onClick(); break; }
    bool keyEntry = false; for (auto* child : grid->getChildren()) if (auto* button = dynamic_cast<juce::TextButton*>(child))
        if (button->getButtonText() == "Key by ear...") { keyEntry = button->isEnabled(); button->onClick(); break; }
    auto* key = ui->earWindow ? dynamic_cast<KeyEarEditor*>(ui->earWindow->getContentComponent()) : nullptr;
    check(keyEntry && key && !key->save.isEnabled(), "Key by ear opens from the actual source key controls with Save disabled until a deliberate choice");
    if (!key) return;
    const auto waveEvent = [&](float x, float down, bool drag)
    {
        const juce::Point<float> at{x, 50}, start{down, 50};
        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), at, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),
            1,0,0,0,0,&key->passage,&key->passage,juce::Time::getCurrentTime(),start,juce::Time::getCurrentTime(),1,drag);
    };
    const float waveStart = key->passage.getWidth() * .125f, waveEnd = key->passage.getWidth() * .625f;
    key->passage.mouseDown(waveEvent(waveStart, waveStart, false)); key->passage.mouseDrag(waveEvent(waveEnd, waveStart, true)); key->passage.mouseUp(waveEvent(waveEnd, waveStart, true));
    check(std::abs(key->passage.range.getStart() - .5) < 1.e-6 && std::abs(key->passage.range.getEnd() - 2.5) < 1.e-6,
          "native source-waveform drag defines a musical listening loop without requiring beat analysis");
    capture(*key, "key-by-ear-home");
    const auto keyBefore = SourceCorrection::from(*b); key->unsure.onClick(); check(b->manualKey == keyBefore.key && key->choice.selected == -1, "Unsure never invents a manual key");
    key->useHome.onClick(); key->prefer2.onClick();
    check(key->choice.stage == 2 && key->save.isEnabled(), "Use home sound skips unnecessary comparisons while keeping the chord choice explicit");
    key->unsure.onClick(); check(key->choice.selected == -1 && !key->save.isEnabled(), "Unsure after review clears the key draft and disables Save");
    key->back.onClick();
    const auto actualSave = key->saveKey; key->saveKey = [](int) { return juce::Result::fail("Owned source-key write failure."); };
    key->save.onClick(); check(b->manualKey == keyBefore.key && key->save.isEnabled() && key->status.getText().contains("write failure"),
          "failed source-key save keeps the listening choice without modifying saved source facts");
    key->saveKey = actualSave; capture(*key, "key-by-ear-review");
    const auto selectedKey = key->choice.selected; key->save.onClick();
    check(b->manualKey == selectedKey && b->manualBpm == keyBefore.bpm && b->manualBeatOrigin == keyBefore.downbeat,
          "actual source-key Save preserves BPM/downbeat corrections and never retimes the mix");
    const auto savedKey = readAnalysisRecord(b->file, b->analysisSource);
    check(savedKey.record && savedKey.record->manualKey == selectedKey, "listening key survives the real source-record write");
    ui->gridWindow->closeButtonPressed(); ui->earWindow.reset();
    {
        auto missing = std::make_unique<HarmonyEarEditor>(ui->engine, ui->libraryPreview, ui->audioWorkers, "removed-segment",
            [](const juce::String&, const Clip&) { return juce::Result::ok(); });
        check(missing->adjusted.getSelectedId() == 0 && !missing->applyButton.isEnabled(),
              "a missing opening target never silently defaults to another segment");
    }
    ui->selectedClip = target.id; ui->showHarmoniseByEar();
    auto* harmony = ui->earWindow ? dynamic_cast<HarmonyEarEditor*>(ui->earWindow->getContentComponent()) : nullptr;
    check(harmony && !harmony->applyButton.isEnabled(), "Harmonise by ear opens for the selected segment without pre-applying pitch");
    if (!harmony) return;
    const auto wait = [&]
    {
        const double deadline = juce::Time::getMillisecondCounterHiRes() + 20000;
        while ((harmony->pending || harmony->applying) && juce::Time::getMillisecondCounterHiRes() < deadline) { pump(); juce::Thread::sleep(2); }
    };
    harmony->prepare.onClick(); wait();
    check(!harmony->pending && harmony->original && harmony->trial && harmony->applyButton.isEnabled(), "bounded async overlap preparation publishes a listening candidate");
    if (!harmony->trial) return;
    harmony->higher.onClick(); wait();
    check(harmony->trial && harmony->trial->pitch == 1 && ui->engine.project.clipFor(target.id)->pitchSemitones == 0,
          "Higher prepares a real pitch trial while programme audio remains unchanged");
    harmony->keep.onClick(); check(harmony->keptPitch == 1, "Keep retains a comparison without applying it");
    harmony->listen(0); check(harmony->selectedAudio()->pitch == 0, "Original compares against zero pitch at the same loop");
    harmony->listen(1); harmony->fine.setToggleState(true, juce::dontSendNotification); harmony->fine.onClick(); capture(*harmony, "harmonise-by-ear");
    const auto beforeApply = ui->undoHistory.size(); harmony->applyButton.onClick(); wait();
    check(!harmony->applying && ui->engine.project.clipFor(target.id)->pitchSemitones == 1 && ui->undoHistory.size() == beforeApply + 1,
          "actual Apply prepares playback/export PCM and creates exactly one musical Undo");
    check(ui->engine.project.clipFor(reference.id)->pitchSemitones == 0 && b->manualKey == selectedKey,
          "harmonising preserves the reference and original source key label");
    ui->undo(false); check(ui->engine.project.clipFor(target.id)->pitchSemitones == 0, "actual harmony Undo restores the original arrangement pitch");
    ui->undo(true); check(ui->engine.project.clipFor(target.id)->pitchSemitones == 1, "actual harmony Redo restores the chosen pitch");
    ui->earWindow.reset(); ui->showHarmoniseByEar();
    harmony = ui->earWindow ? dynamic_cast<HarmonyEarEditor*>(ui->earWindow->getContentComponent()) : nullptr;
    const auto beforeCancel = juce::JSON::toString(projectToJson(ui->engine.project), true, 17);
    if (harmony)
    {
        harmony->prepare.onClick(); harmony->cancel.onClick(); ui->earWindow.reset();
        const double deadline = juce::Time::getMillisecondCounterHiRes() + 5000;
        while (ui->audioWorkers.getNumJobs() > 0 && juce::Time::getMillisecondCounterHiRes() < deadline) { pump(); juce::Thread::sleep(2); }
        pump();
        check(ui->audioWorkers.getNumJobs() == 0 && juce::JSON::toString(projectToJson(ui->engine.project), true, 17) == beforeCancel,
              "closing during async listening preparation cancels publication safely and preserves the entire arrangement");
    }
    check(audioDigest(a->file) == hashA && audioDigest(b->file) == hashB, "all listening, saves and pitch preparation leave original audio bytes unchanged");
    ui->earWindow.reset();
}
}
