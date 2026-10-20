#include "Workstation.h"
#include "GridCorrectionUI.h"
#include "Beatmatch.h"
#include <limits>

namespace mixworks
{
void runGridCorrectionTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    SourceCorrection parsed {123, 0.25, 2};
    for (const auto& text : {"120..2", "nan", "inf", "120 bpm", "0", "401"})
        check(SourceCorrection::parse(text, "0.2", 0, 12, parsed).failed() && parsed.bpm == 123,
              juce::String("invalid correction is rejected atomically: ") + text);
    check(SourceCorrection::parse("120.1234", "12", 0, 12, parsed).failed(), "beat-one correction excludes the source's end boundary");
    check(SourceCorrection::parse("", "", -1, 12, parsed).wasOk() && parsed.bpm == 0 && parsed.downbeat == -1,
          "blank fields explicitly restore automatic facts without inventing a bar");
    TapTempo taps;
    check(!taps.tap(1) && !taps.tap(1.5) && taps.tap(2) && std::abs(*taps.tap(2.5) - 120) < 1.0e-8,
          "steady quarter-note taps estimate tempo independently of bar phase");
    const auto count = taps.count(); taps.tap(2.55);
    check(taps.count() == count && !taps.tap(10) && taps.count() == 1, "tap bounce is ignored and a long pause starts a fresh estimate");
    SourceBeatGrid unknown {120, 0.213, -1}, known {120, 0.213, 0.213};
    check(std::abs(known.timeAtBeat(9) - 4.713) < 1.0e-9 && std::abs(known.beatAt(4.713) - 9) < 1.0e-9,
          "source drawing and click use the same reversible beat clock");
    check(std::abs(unknown.clickAt(0.21325) - unknown.clickAt(0.71325)) < 1.0e-6
          && std::abs(known.clickAt(0.21325)) > std::abs(known.clickAt(0.71325)),
          "unknown bars have equal clicks while known beat one is accented");
    SourceBeatGrid malformed {120, std::numeric_limits<double>::quiet_NaN(), -1};
    check(malformed.clickAt(1) == 0 && unknown.clickAt(std::numeric_limits<double>::infinity()) == 0,
          "malformed/nonfinite grid clocks cannot emit nonfinite audio");

    auto media = std::make_shared<Media>(); media->name = "Source grid fixture"; media->sampleRate = 44100;
    media->file = report.getSiblingFile("grid-source.wav"); media->samples.setSize(1, 44100 * 12);
    for (int i = 0; i < media->frames(); ++i)
        media->samples.setSample(0, i, (float) (0.3 * std::sin(juce::MathConstants<double>::twoPi * 440 * i / media->sampleRate)));
    { juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream = media->file.createOutputStream();
      auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(44100).withNumChannels(1).withBitsPerSample(24));
      check(writer && writer->writeFromAudioSampleBuffer(media->samples, 0, media->frames()), "write owned source-grid audio fixture"); }
    media->analysisSource = {audioDigest(media->file), media->file.getSize(), 44100, media->frames(), 1};
    media->analysis.bpm = 120; media->analysis.beatOrigin = 0.213; media->analysis.tempoConfidence = 0.9;
    media->analysis.key = 0; media->analysis.keyConfidence = 0.8; media->analysis.examinedSeconds = 12;
    media->peaks.assign(512, 0.3f); media->prepareWaveformBody();
    const auto sourceDigest = audioDigest(media->file);
    GridAudition audition; juce::AudioBuffer<float> audio(2, 2048), whole(2, 2048), partial(2, 512);
    check(audition.start(media, known, 0.1, true, false, -6).wasOk(), "source audition accepts decoded source audio");
    audition.render(audio, 2048, 48000);
    check(std::abs(audio.getSample(0, 0) - media->samples.getSample(0, 4410) * juce::Decibels::decibelsToGain(-6.0f)) < 1.0e-6
          && audio.getSample(0, 123) == audio.getSample(1, 123) && std::abs(audition.position() - (0.1 + 2048.0 / 48000)) < 1.0e-8,
          "monitor audition resamples the raw source in source seconds and maps mono to stereo");
    audition.start(media, known, 0.2, false, true, -12); audition.render(whole, 2048, 44100);
    audition.start(media, known, 0.2, false, true, -12);
    double difference = 0;
    for (int block = 0; block < 4; ++block)
    {
        audition.render(partial, 512, 44100);
        for (int i = 0; i < 512; ++i) difference = std::max(difference, (double) std::abs(partial.getSample(0, i) - whole.getSample(0, block * 512 + i)));
    }
    check(difference == 0 && whole.getMagnitude(0, 2048) > 0.01, "audible clicks are invariant across callback block boundaries");
    audition.start(media, known, media->duration() - 0.001, true, true, -12); audition.render(audio, 2048, 44100);
    check(!audition.active() && audio.getSample(0, 2047) == 0, "source end stops audition with a silent remainder");

    Workstation ui(false); ui.setSize(1250, 800); ui.engine.library.push_back(media); ui.selectedLibraryId = media->id;
    Clip clip; clip.media = media; clip.length = media->duration();
    Project input; input.clips = {clip}; auto prepared = prepareTempoMap(input, false, 0);
    check(prepared.error.isEmpty(), "prepare captured arrangement before source correction: " + prepared.error);
    if (prepared.error.isNotEmpty()) return;
    ui.engine.project = prepared.project; ui.refresh();
    auto before = ui.engine.project.clips[0]; const auto tempo = ui.engine.project.tempo.points;
    audio.setSize(2, 512); juce::MidiBuffer midi; ui.engine.render(audio, 512, 0.3, midi); whole.makeCopyOf(audio);
    check(ui.engine.correctSource(media, {130.5, 0.213, 3}).wasOk(), "production engine accepts a validated source correction");
    const auto& after = ui.engine.project.clips[0];
    ui.engine.render(audio, 512, 0.3, midi); difference = 0;
    for (int i = 0; i < 512; ++i) difference = std::max(difference, (double) std::abs(audio.getSample(0, i) - whole.getSample(0, i)));
    check(after.start == before.start && after.offset == before.offset && after.length == before.length && after.prepared == before.prepared
          && after.tempoMap == before.tempoMap && after.tempoSourceBpm == before.tempoSourceBpm && ui.engine.project.tempo.points == tempo && difference == 0,
          "changing source facts preserves placement, captured clock, prepared audio, tempo map and actual PCM");
    auto catalogue = media->catalogueCopy(); ui.engine.library = {catalogue};
    check(ui.engine.correctSource(catalogue, {128, 0.125, 5}).wasOk() && media->manualBpm == 128 && media->manualBeatOrigin == 0.125,
          "one source identity owns corrections across catalogue and hydrated arrangement objects");
    auto foreign = media->catalogueCopy(); foreign->analysisSource.sha256 = juce::String::repeatedString("a", 64);
    check(ui.engine.correctSource(foreign, {140, 0.5, 1}).failed() && media->manualBpm == 128,
          "a different source identity cannot correct an existing arrangement by filename");
    ui.engine.library = {media}; ui.engine.correctSource(media, {0, -1, -1});
    ui.editAnalysis();
    auto* editor = dynamic_cast<GridCorrectionEditor*>(ui.gridWindow->getContentComponent());
    check(editor && ui.gridWindow->isVisible(), "BPM / Key opens the native source-grid correction window");
    if (!editor) return;
    editor->middle.onClick(); editor->beatOne.onClick(); editor->bpm.setText("120..2", false); editor->changed();
    check(!editor->save.isEnabled() && media->manualBeatOrigin == -1, "invalid draft disables Apply and leaves source facts unchanged");
    editor->bpm.setText("120", false); editor->changed(); editor->half.onClick();
    check(editor->draft.bpm == 60 && editor->draft.downbeat == 6, "half-tempo and Beat 1 here operate on the source cursor draft");
    editor->automatic.onClick(); editor->play.onClick();
    const double position = ui.engine.position; float left[256] {}, right[256] {}; float* outputs[] {left, right};
    ui.engine.audioDeviceIOCallbackWithContext(nullptr, 0, outputs, 2, 256, {});
    check(ui.engine.gridAudition.active() && !ui.engine.playing && ui.engine.position == position
          && juce::FloatVectorOperations::findMinAndMax(left, 256).getLength() > 0,
          "real audio callback auditions the source while leaving the mix clock paused");
    ui.engine.project.masterDb = -3;
    const auto exported = report.getSiblingFile("no-preview-in-export.wav");
    check(ui.engine.exportWav(exported).isEmpty(), "offline render remains available with monitoring state separate from the project");
    auto stream = report.getSiblingFile("source-grid-editor.png").createOutputStream(); juce::PNGImageFormat png;
    check(stream && png.writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds()), *stream), "capture native source-grid editor for visual verification");
    editor->cancel.onClick();
    check(!ui.engine.gridAudition.active() && !ui.gridWindow->isVisible() && media->manualBeatOrigin == -1 && media->manualBpm == 0,
          "Cancel closes monitoring and discards corrections without changing source facts");
    ui.editAnalysis(); editor = dynamic_cast<GridCorrectionEditor*>(ui.gridWindow->getContentComponent());
    editor->bpm.setText("130.5", false); editor->phase.setText("0.213", false); editor->key.setSelectedId(5, juce::dontSendNotification);
    editor->changed(); editor->save.onClick(); ui.workers.removeAllJobs(false, 10000);
    auto record = readAnalysisRecord(media->file, media->analysisSource);
    check(!ui.busy && ui.dirty && record.record && record.record->manualBpm == 130.5 && record.record->manualBeatOrigin == 0.213 && record.record->manualKey == 3,
          "native Apply persists source corrections once without preparing or rearranging the mix");
    const auto session = report.getSiblingFile("corrected.mixworks");
    check(ui.engine.saveProject(session).isEmpty() && ui.engine.restoreProject(session.loadFileAsString()).isEmpty(), "corrected source and captured arrangement save/reopen through production APIs");
    check(ui.engine.project.clips[0].media->manualBpm == 130.5 && ui.engine.project.clips[0].tempoSourceBpm == before.tempoSourceBpm
          && ui.engine.project.clips[0].start == before.start && ui.engine.project.clips[0].length == before.length,
          "reopening retains the correction separately from the original arranged source clock");
    check(audioDigest(media->file) == sourceDigest, "audition, correction, cache writes and reopen leave original audio unchanged");
}
}
