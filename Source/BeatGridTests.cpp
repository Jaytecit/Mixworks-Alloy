#include "Workstation.h"
#include "Beatmatch.h"
#include <cmath>

namespace mixworks
{
void runBeatGridTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report, const juce::File& reference)
{
    auto first = std::make_shared<Media>(); first->sampleRate = 8000; first->manualBpm = 120; first->manualBeatOrigin = 0.213;
    first->samples.setSize(1, 8000 * 24); first->samples.clear();
    auto second = std::make_shared<Media>(); second->sampleRate = 8000; second->manualBpm = 144; second->manualBeatOrigin = 1.213;
    second->samples = first->samples;
    Clip a; a.media = first; a.length = 24;
    Clip b; b.media = second; b.track = 1; b.length = 24;
    Project project; project.clips = {a, b};
    auto matched = prepareTempoMap(project, true, 16);
    bool bars = matched.error.isEmpty();
    for (const auto& clip : matched.project.clips)
    {
        const double beat = matched.project.tempo.beatsAt(clip.start) + clip.media->beatOrigin() * clip.media->bpm() / 60;
        bars = bars && std::abs(std::remainder(beat, 4.0)) < 1.0e-6;
    }
    check(bars, "automatic tempo transitions align each confirmed first downbeat to a common four-beat bar: " + matched.error);
    bool aligned = matched.error.isEmpty();
    for (const auto& clip : matched.project.clips)
        for (int beat = 0; beat < 20; ++beat)
        {
            const double local = clip.localTimeForSource(clip.media->beatOrigin() + beat * 60 / clip.media->bpm());
            if (local < clip.length) aligned = aligned && std::abs(std::remainder(matched.project.tempo.beatsAt(clip.start + local), 1.0)) < 1.0e-6;
        }
    check(aligned, "both source beat grids stay on the integrated mix grid throughout the BPM transition");
    auto inserted = project;
    for (auto& clip : inserted.clips) clip.pendingPlacement = PendingPlacement::append;
    auto automatic = prepareTempoMap(inserted, false, 16);
    bool automaticBars = automatic.error.isEmpty();
    for (const auto& clip : automatic.project.clips)
        automaticBars &= clip.beatmatched && clip.offset == 0 && std::abs(clip.length * clip.rate - 24) < 1.0e-7
            && std::abs(std::remainder(automatic.project.tempo.beatsAt(clip.start + clip.firstDownbeatTime()), 4)) < 1.0e-7;
    check(automaticBars, "normal insertion aligns different source bar phases through a tempo ramp without trimming audio or requiring Beatmatch");
    {
        Workstation guides(false); guides.stopTimer(); guides.setSize(1440, 940);
        Clip offset; offset.media = first; offset.start = 0.137; offset.length = 24; offset.beatmatched = true; offset.tempoSourceBpm = 120;
        guides.engine.project.tempo.points = {{0, 120}}; offset.tempoMap = std::make_shared<const TempoMap>(guides.engine.project.tempo);
        guides.engine.project.clips = {offset}; guides.timeline.pixelsPerSecond = 81.3; guides.refresh();
        const auto snapshot = guides.timeline.createComponentSnapshot({0, 0, 650, guides.timeline.laneTop(1)});
        const int beatX = Clip::gridPixel(4.5, 81.3), sourceX = Clip::gridPixel(4.35, 81.3);
        const int middle = guides.timeline.laneTop(0) + 65, tick = guides.timeline.laneTop(0) + Timeline::laneHeight - 18;
        check(snapshot.getPixelAt(beatX, middle).getPerceivedBrightness() > snapshot.getPixelAt(beatX + 2, middle).getPerceivedBrightness()
              && snapshot.getPixelAt(sourceX, middle) == snapshot.getPixelAt(sourceX + 2, middle),
              "native full-height track guides use the timeline grid even when a source is deliberately offset");
        check(snapshot.getPixelAt(sourceX, tick) != snapshot.getPixelAt(sourceX + 2, tick),
              "a short source-beat tick still exposes the real timing offset without drawing a second full-height grid");
        auto image = report.getSiblingFile("shared-grid-offset-source.png").createOutputStream(); juce::PNGImageFormat png;
        check(image && png.writeImageToStream(snapshot, *image), "capture shared timeline guides with independently visible source timing");
    }
    {
        Workstation pixelUi(false); pixelUi.stopTimer();
        auto pulse = std::make_shared<Media>(); pulse->sampleRate = 8000; pulse->manualBpm = 120; pulse->manualBeatOrigin = .218;
        pulse->samples.setSize(1, 16000); pulse->samples.clear(); pulse->samples.setSample(0, 1744, .95f);
        Clip clip; clip.media = pulse; clip.start = .137; clip.length = 2;
        pixelUi.engine.project.clips = {clip}; pixelUi.waveformMode.setSelectedId(2, juce::dontSendNotification);
        pixelUi.timeline.pixelsPerSecond = 81.3; pixelUi.refresh();
        const auto before = juce::JSON::toString(projectToJson(pixelUi.engine.project));
        const auto capture = pixelUi.timeline.createComponentSnapshot({0, 0, 170, 132});
        const int x = Clip::gridPixel(clip.start + .218, 81.3);
        check(x == 29 && capture.getPixelAt(x, 45).getBrightness() > capture.getPixelAt(x - 1, 45).getBrightness() + .1f
              && before == juce::JSON::toString(projectToJson(pixelUi.engine.project)),
              "fractional-position waveform pulse and source tick use the same nearest pixel without changing musical timing");
    }
    // Reproduce removal followed by replacement, retaining the deleted source's
    // tempo curve just as the real Remove command does. Check the whole clip,
    // rather than only the first beat at the insertion boundary.
    {
        Workstation replacement(false); replacement.stopTimer(); replacement.setSize(1440, 940);
        for (int removed : {0, 1}) for (bool live : {false, true})
        {
            replacement.engine.project = automatic.project;
            replacement.selectedClip = automatic.project.clips[(size_t) removed].id;
            const auto retained = replacement.engine.project.tempo;
            replacement.removeClip();
            bool clock = true;
            for (double time = 0; time < 80; time += 0.137)
                clock &= std::abs(retained.beatsAt(time) - replacement.engine.project.tempo.beatsAt(time)) < 1.0e-8;
            auto added = b; added.id = juce::Uuid().toString(); added.pendingPlacement = PendingPlacement::append;
            replacement.engine.project.clips.push_back(added);
            const auto result = prepareTempoMap(replacement.engine.project, false, 16, 0, 0,
                                                live ? std::optional<double>(2) : std::nullopt);
            bool pulses = result.error.isEmpty();
            for (const auto& clip : result.project.clips)
                for (double source = clip.sourceElapsed(clip.firstBeatTime()); source < clip.sourceElapsed(clip.length); source += 60 / clip.media->bpm())
                {
                    const auto time = clip.beatTimelineTime(source);
                    const double beat = result.project.tempo.beatsAt(time);
                    pulses &= std::abs(std::remainder(beat, 1)) < 1.0e-7;
                    for (double zoom : {7.3, 27.01, 81.3})
                        pulses &= Clip::gridPixel(time, zoom) == Clip::gridPixel(result.project.tempo.timeAtBeat(std::round(beat)), zoom);
                }
            check(clock && pulses, "remove source " + juce::String(removed) + (live ? " then add during playback" : " then add while stopped")
                  + " retains the clock and aligns every drawn pulse: " + result.error);
        }
    }
    {
        first->file = report.getSiblingFile("grid-source-0.wav"); second->file = report.getSiblingFile("grid-source-1.wav");
        auto coincident = automatic.project;
        // An owned incoming ramp target and an independent cut can legally
        // occupy one timestamp. Both endpoints carry timing information.
        coincident.tempo.points = {{0, 120}, {12, 150, true, coincident.clips[1].id}, {12, 120, false}};
        auto prepared = prepareTempoMap(coincident, false, 16);
        Workstation removal(false); removal.stopTimer(); removal.engine.project = prepared.project;
        removal.selectedClip = coincident.clips[1].id; removal.removeClip();
        bool clock = prepared.error.isEmpty();
        for (double time = 0; time < 36; time += 0.137)
            clock &= std::abs(coincident.tempo.beatsAt(time) - removal.engine.project.tempo.beatsAt(time)) < 1.0e-8;
        check(clock, "removing a coincident owned ramp endpoint retains the preceding curve and remaining clip grid");
        check(removal.engine.project.tempo.points.size() == 3 && parseProject(juce::JSON::toString(projectToJson(removal.engine.project))).error.isEmpty(),
              "detached coincident ramp and cut endpoints remain saveable without collapsing their timing");
        removal.undo(false);
        const bool undo = removal.engine.project.clips.size() == 2 && removal.engine.project.tempo.points == prepared.project.tempo.points;
        removal.undo(true);
        check(undo && removal.engine.project.clips.size() == 1 && removal.engine.project.tempo.points.size() == 3,
              "Undo and Redo retain both coincident timing endpoints and restore their track ownership");
        auto added = a; added.id = juce::Uuid().toString(); added.pendingPlacement = PendingPlacement::append;
        removal.engine.project.clips.push_back(added);
        const auto live = prepareTempoMap(removal.engine.project, false, 16, 0, 0, 2.0);
        bool shared = live.error.isEmpty();
        const auto& playing = live.project.clips[0];
        for (double source = playing.sourceElapsed(playing.firstBeatTime()); source < playing.sourceElapsed(playing.length); source += 60 / playing.media->bpm())
            shared &= std::abs(std::remainder(live.project.tempo.beatsAt(playing.beatTimelineTime(source)), 1)) < 1.0e-7;
        check(shared, "live replacement after deleting a coincident tempo owner keeps established beat lines on the ruler");
        removal.engine.project = live.project; removal.setSize(1440, 940); removal.timeline.pixelsPerSecond = 81.3; removal.refresh();
        removal.viewport.setViewPosition(400, 0);
        auto image = report.getSiblingFile("coincident-removal-grid.png").createOutputStream(); juce::PNGImageFormat png;
        check(image && png.writeImageToStream(removal.createComponentSnapshot(removal.getLocalBounds()), *image),
              "capture native track and timeline grids after removing a coincident target and replacing it live");
    }
    auto unknownBar = first->catalogueCopy(); unknownBar->samples = first->samples; unknownBar->manualBeatOrigin = -1;
    unknownBar->analysis.bpm = 120; unknownBar->analysis.beatOrigin = 0.213; unknownBar->analysis.tempoConfidence = 0.8;
    inserted.clips.resize(1); inserted.clips[0].media = unknownBar;
    const auto pulseOnly = prepareTempoMap(inserted, false, 16);
    const auto& pulseClip = pulseOnly.project.clips.front();
    check(pulseOnly.error.isEmpty() && pulseClip.firstDownbeatTime() < 0 && pulseClip.beatmatched
          && std::abs(std::remainder(pulseOnly.project.tempo.beatsAt(pulseClip.start + pulseClip.firstBeatTime()), 1)) < 1.0e-7,
          "unknown downbeat still aligns measured pulses during automatic insertion without inventing a bar phase");
    // Real overlapping PCM, with different source tempos and non-zero first beats.
    for (const auto& media : {first, second})
    {
        media->sampleRate = 22050; media->samples.setSize(1, 22050 * 40); media->samples.clear();
        media->name = media == first ? "Outgoing / 120 BPM" : "Incoming / 144 BPM";
        for (double pulse = media->beatOrigin(); pulse < 39.9; pulse += 60 / media->bpm())
            for (int i = 0; i < 1800; ++i)
            {
                const int frame = (int) std::round(pulse * media->sampleRate) + i;
                if (frame < media->frames()) media->samples.setSample(0, frame, (float) (0.8 * std::exp(-i / (0.018 * media->sampleRate))));
            }
    }
    project.clips[0].length = project.clips[1].length = 40;
    auto audible = prepareTempoMap(project, true, 16);
    check(audible.error.isEmpty(), "real two-tempo overlap prepares both source tracks: " + audible.error);
    if (audible.error.isEmpty())
    {
        Engine render; render.sampleRate = 22050; render.project = audible.project; render.project.masterDb = 0;
        const auto& map = audible.project.tempo; double worst = 0, separation = 0; int measured = 0;
        for (int beat = (int) std::ceil(map.beatsAt(audible.project.clips[1].start)) + 5; map.timeAtBeat(beat) < audible.project.clips[0].end() - 0.2; ++beat)
        {
            const double expected = map.timeAtBeat(beat); double found[2] {};
            for (int track = 0; track < 2; ++track)
            {
                auto solo = audible.project.clips[(size_t) track]; solo.fadeIn = solo.fadeOut = 0; render.project.clips = {solo};
                juce::AudioBuffer<float> pcm(2, 4410); juce::MidiBuffer midi;
                for (int begin = 0; begin < pcm.getNumSamples(); begin += 512)
                {
                    const int count = std::min(512, pcm.getNumSamples() - begin); juce::AudioBuffer<float> block(2, count);
                    render.render(block, count, expected - 0.1 + begin / 22050.0, midi);
                    for (int ch = 0; ch < 2; ++ch) pcm.copyFrom(ch, begin, block, ch, 0, count);
                }
                float strongest = 0; int index = 0;
                for (int i = 1; i < pcm.getNumSamples(); ++i) { const float rise = pcm.getSample(0, i) - pcm.getSample(0, i - 1); if (rise > strongest) { strongest = rise; index = i; } }
                found[track] = expected - 0.1 + index / 22050.0;
                if (strongest < 0.05f) worst = 1;
                worst = std::max(worst, std::abs(found[track] - expected));
            }
            separation = std::max(separation, std::abs(found[0] - found[1])); ++measured;
        }
        check(measured >= 6 && worst < 0.035 && separation < 0.035,
              "rendered transients remain aligned during the BPM ramp; grid error " + juce::String(worst * 1000, 2) + " ms, track separation " + juce::String(separation * 1000, 2) + " ms");
        Workstation transitionUi(false); transitionUi.stopTimer(); transitionUi.setSize(1440, 940);
        transitionUi.engine.project = audible.project; transitionUi.timeline.pixelsPerSecond = 150; transitionUi.refresh();
        transitionUi.viewport.setViewPosition((int) ((audible.project.clips[1].start - 0.5) * 150), 0);
        const auto snapshot = transitionUi.createComponentSnapshot(transitionUi.getLocalBounds()); auto out = report.getSiblingFile("beat-grid-transition.png").createOutputStream();
        if (out) { out->setPosition(0); out->truncate(); juce::PNGImageFormat format; format.writeImageToStream(snapshot, *out); }
    }

    // A real timeline paint must distinguish PCM transients within one overview bucket.
    Workstation ui(false); ui.stopTimer(); ui.setSize(1440, 940);
    auto detail = std::make_shared<Media>(); detail->sampleRate = 8000;
    detail->samples.setSize(1, 8000 * 360); detail->samples.clear();
    detail->samples.setSample(0, 800, 0.9f); // 100 ms: the overview bucket spans 90 ms.
    detail->peaks.assign(4000, 0); detail->peaks[1] = 0.9f;
    Clip shown; shown.media = detail; shown.length = 360;
    ui.engine.project.clips = {shown}; ui.timeline.pixelsPerSecond = 400; ui.refresh();
    auto image = ui.timeline.createComponentSnapshot({0, 0, 320, Timeline::rulerHeight + 95});
    auto differentRows = [&ui](const juce::Image& capture, int a, int b)
    {
        int different = 0;
        for (int y = ui.timeline.laneTop(0) + 32; y < std::min(capture.getHeight(), ui.timeline.laneTop(0) + 120); ++y)
            if (capture.getPixelAt(a, y) != capture.getPixelAt(b, y)) ++different;
        return different;
    };
    check(differentRows(image, 40, 56) > 15, "zoomed native waveform resolves a transient and silence within the same overview bucket");
    check(detail->waveformRange(0.1, 0.1025).peak > 0.8 && detail->waveformRange(0.14, 0.1425).peak == 0
        && detail->waveformRange(0, 0.3).peak > 0.8, "waveform owner uses exact PCM for detail and interval maxima for the overview");
    shown.offset = 0.09; shown.loopSpan = 0.09; shown.loopPhase = 0.085;
    check(shown.waveformRange(0, 0.017).peak > 0.8, "loop seam waveform queries retain a transient after wrapping");
    shown.reversed = true; shown.loopPhase = 0;
    check(shown.waveformRange(0.077, 0.081).peak > 0.8, "reverse waveform queries follow the audible source direction");
    auto pcm = std::make_shared<PreparedAudio>(); pcm->clipLocal = true; pcm->samples.setSize(1, 24000); pcm->samples.clear(); pcm->samples.setSample(0, 1600, 0.75f);
    shown.prepared = pcm; shown.preparedOffset = 0.1;
    check(shown.waveformRange(0.1, 0.102).peak > 0.7 && shown.waveformRange(0.2, 0.202).peak == 0,
          "detailed prepared/split waveforms read the actual pitch-preserved playback PCM and its retained offset");
    auto file = report.getSiblingFile("beat-grid-detail.png"); auto stream = file.createOutputStream();
    if (stream) { stream->setPosition(0); stream->truncate(); juce::PNGImageFormat format; format.writeImageToStream(image, *stream); }

    // Equal limited peaks can hide different beat energy. The native display
    // must retain the peak outline while exposing the denser body underneath.
    auto limited = std::make_shared<Media>(); limited->sampleRate = 8000;
    limited->samples.setSize(1, 80000);
    for (int i = 0; i < 80000; ++i) limited->samples.setSample(0, i, i % 20 == 0 ? 0.98f : ((i / 800) % 2 == 0 ? 0.1f : 0.8f));
    Clip dense; dense.media = limited; dense.length = 10;
    ui.engine.project.clips = {dense}; ui.timeline.pixelsPerSecond = 400; ui.refresh();
    const auto limitedImage = ui.timeline.createComponentSnapshot({0, 0, 320, Timeline::rulerHeight + 132});
    check(differentRows(limitedImage, 20, 60) > 15, "native waveform exposes different beat energy even when maximum peaks are equally limited");
    const auto quietBody = dense.waveformRange(0.05, 0.0525), loudBody = dense.waveformRange(0.15, 0.1525);
    check(quietBody.peak == loudBody.peak && quietBody.rms < 0.3 && loudBody.rms > 0.8 && limited->samples.getSample(0, 1) == 0.1f,
          "waveform detail measures energy without normalizing, limiting or changing source PCM");
    limited->peaks.assign(4000, 0.98f); limited->prepareWaveformBody();
    check(dense.waveformRange(0.01, 0.09).rms < 0.3 && dense.waveformRange(0.11, 0.19).rms > 0.8,
          "overview waveform body retains energy differences through worker-prepared buckets");
    ui.waveformMode.setSelectedId(2, juce::sendNotificationSync);
    const auto peaksImage = ui.timeline.createComponentSnapshot({0, 0, 320, Timeline::rulerHeight + 132});
    check(differentRows(peaksImage, 20, 60) == 0, "peak-only display remains available without changing playback");
    ui.waveformMode.setSelectedId(1, juce::sendNotificationSync);
    { auto out = report.getSiblingFile("limited-waveform.png").createOutputStream(); if (out) { out->setPosition(0); out->truncate(); juce::PNGImageFormat png; png.writeImageToStream(limitedImage, *out); } }

    auto structure = std::make_shared<Media>(); structure->sampleRate = 8000;
    structure->samples.setSize(1, 8000 * 64); structure->peaks.assign(4000, 0.98f);
    for (int i = 0; i < structure->frames(); ++i)
    {
        const double t = i / 8000.0; const float energy = t < 8 ? 0.05f : t < 16 ? (float) (0.05 + (t - 8) * 0.07) : t < 24 ? 0.61f : t < 32 ? 0.025f : 0.45f;
        structure->samples.setSample(0, i, i % 800 == 0 ? 0.98f : energy);
    }
    structure->prepareWaveformBody(); Clip structural; structural.media = structure; structural.length = 64;
    ui.engine.project.clips = {structural}; ui.engine.project.tempo = {120, {}}; ui.timeline.pixelsPerSecond = 8; ui.refresh();
    const auto overview = ui.timeline.createComponentSnapshot({0, 0, 520, ui.timeline.laneTop(4)});
    check(differentRows(overview, 19, 147) > 35 && differentRows(overview, 147, 211) > 35,
          "overview contour reveals a rise and a quiet breakdown despite equally limited peaks");
    auto gridVisible = [&](const juce::Image& capture, int x)
    { const int y = ui.timeline.laneTop(3) + 35; return capture.getPixelAt(x, y).getBrightness() > capture.getPixelAt(x + 1, y).getBrightness() + 0.1f; };
    check(gridVisible(overview, 64) && gridVisible(overview, 128) && gridVisible(overview, 192)
          && overview.getPixelAt(64, ui.timeline.laneTop(3) + 35).getBrightness() > overview.getPixelAt(16, ui.timeline.laneTop(3) + 35).getBrightness(),
          "native grid accents repeat every four bars (16 beats), stronger than individual bars");
    ui.engine.project.tempo.points = {{0, 120}, {40, 180}}; ui.refresh();
    const auto rampGrid = ui.timeline.createComponentSnapshot({0, 0, 520, ui.timeline.laneTop(4)});
    bool gridFollowsRamp = true;
    for (int beat : {16, 32, 48, 64}) gridFollowsRamp &= gridVisible(rampGrid, Clip::gridPixel(ui.engine.project.tempo.timeAtBeat(beat), 8));
    check(gridFollowsRamp, "four-bar guide spacing follows the integrated tempo ramp rather than fixed seconds");

    auto rhythm = [&](bool accented, double intro = 0.213)
    {
        juce::AudioBuffer<float> audio(1, 320000); audio.clear();
        for (int beat = 0; intro + beat * 0.5 < 39.8; ++beat)
            for (int i = 0; i < 720; ++i)
            {
                const double time = i / 8000.0; const float gain = accented && beat % 4 == 2 ? 0.9f : 0.4f;
                const int index = (int) std::round((intro + beat * 0.5) * 8000) + i;
                if (index < audio.getNumSamples()) audio.setSample(0, index, gain * (float) (std::sin(2 * juce::MathConstants<double>::pi * 70 * time) * std::exp(-time * 40)));
            }
        return audio;
    };
    const auto accented = rhythm(true); const auto measured = analyseMusic(accented, 8000);
    check(std::abs(measured.bpm - 120) < 0.2 && std::abs(measured.downbeatOrigin - 1.213) < 0.035 && measured.downbeatConfidence > 0,
          "4/4 accent inference retains the first downbeat's absolute source position: " + juce::String(measured.downbeatOrigin, 4));
    const auto equal = analyseMusic(rhythm(false), 8000), intro = analyseMusic(rhythm(true, 5.213), 8000);
    check(equal.downbeatOrigin < 0 && equal.downbeatConfidence == 0, "equal pulses retain an unknown downbeat instead of declaring the first pulse to be bar one");
    check(std::abs(intro.downbeatOrigin - 6.213) < 0.035, "silent intro does not wrap the first observed downbeat back into the initial beat period");

    // Persist through the same source sidecar and session boundaries used by the app.
    const auto folder = report.getSiblingFile("beat-grid-fixture-" + juce::Uuid().toString()); folder.createDirectory();
    const auto wav = folder.getChildFile("accented.wav");
    { juce::WavAudioFormat format; std::unique_ptr<juce::OutputStream> out = wav.createOutputStream(); auto writer = format.createWriterFor(out, juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(1).withBitsPerSample(24)); if (writer) writer->writeFromAudioSampleBuffer(accented, 0, accented.getNumSamples()); }
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    auto cold = readMedia(formats, wav), warm = readMedia(formats, wav);
    check(cold.media && warm.media && warm.media->analysisFromCache && cold.media->analysis.downbeatOrigin == warm.media->analysis.downbeatOrigin
        && cold.media->analysis.downbeatConfidence == warm.media->analysis.downbeatConfidence, "inferred downbeat position and uncertainty survive a warm source-analysis reopen");
    if (!warm.media) return;
    Clip inferred; inferred.media = cold.media; inferred.length = cold.media->duration(); Project inferredProject; inferredProject.clips = {inferred};
    auto inferredMatch = prepareTempoMap(inferredProject, true, 16);
    check(inferredMatch.error.isEmpty() && inferredMatch.project.clips[0].firstDownbeatTime() >= 0
        && std::abs(std::remainder(inferredMatch.project.tempo.beatsAt(inferredMatch.project.clips[0].start)
            + cold.media->analysis.downbeatOrigin * cold.media->bpm() / 60, 4.0)) < 1.0e-6,
          "automatic placement consumes an inferred downbeat without requiring a manual override");
    ui.engine.library = {warm.media}; shown = {}; shown.media = warm.media; shown.length = warm.media->duration(); ui.engine.project.clips = {shown};
    ui.setDownbeatAnchor(shown.id, 2.213);
    ui.workers.removeAllJobs(false, -1);
    writeAnalysisRecord(wav, warm.media->analysisRecord());
    check(warm.media->manualBeatOrigin == 2.213 && ui.dirty, "native waveform downbeat correction retains the exact clicked source time");
    const auto saved = juce::JSON::toString(ui.engine.captureProject()); Engine reopened;
    check(reopened.restoreProject(saved).isEmpty() && reopened.library[0]->manualBeatOrigin == 2.213,
          "manual first downbeat correction survives the production session round trip");
    const auto old = juce::JSON::parse(analysisRecordFile(wav)); auto stale = juce::JSON::parse(juce::JSON::toString(old));
    stale.getDynamicObject()->setProperty("analysisRevision", 1); stale["analysis"].getDynamicObject()->removeProperty("downbeatOrigin");
    analysisRecordFile(wav).replaceWithText(juce::JSON::toString(stale));
    auto refreshed = readMedia(formats, wav);
    check(refreshed.media && !refreshed.media->analysisFromCache && refreshed.media->manualBeatOrigin == 2.213,
          "old automatic analysis is refreshed while the saved manual first downbeat is retained");
    auto invalid = juce::JSON::parse(analysisRecordFile(wav)); invalid["analysis"].getDynamicObject()->setProperty("downbeatOrigin", -0.5);
    analysisRecordFile(wav).replaceWithText(juce::JSON::toString(invalid));
    const auto invalidRead = readAnalysisRecord(wav, refreshed.media->analysisSource);
    check(invalidRead.record && !invalidRead.reusableAutomatic && invalidRead.record->manualBeatOrigin == 2.213,
          "malformed automatic downbeat facts force refresh without discarding valid manual corrections");
    ui.workers.removeAllJobs(true, -1);
    if (reference != juce::File())
    {
        const auto digest = audioDigest(reference), sidecar = analysisRecordFile(reference).loadFileAsString();
        auto actual = readMedia(formats, reference, {}, AnalysisCacheMode::readOnly);
        check(actual.media != nullptr, "read the requested waveform reference without changing source analysis: " + actual.error);
        if (actual.media)
        {
            Workstation preview(false); preview.stopTimer(); preview.setSize(1440, 940);
            preview.engine.library = {actual.media}; Clip clip; clip.media = actual.media; clip.length = actual.media->duration();
            Project referenceProject; referenceProject.clips = {clip};
            auto placed = prepareTempoMap(referenceProject, true, 32);
            preview.engine.project = placed.error.isEmpty() ? placed.project : referenceProject; preview.selectedClip = clip.id;
            preview.timeline.pixelsPerSecond = 80; preview.refresh();
            preview.viewport.setViewPosition((int) (std::min(120.0, clip.length * 0.4) * 80), 0);
            bool referenceSaved = true;
            for (int mode : {1, 2, 3})
            {
                preview.waveformMode.setSelectedId(mode, juce::sendNotificationSync);
                const auto capture = preview.createComponentSnapshot(preview.getLocalBounds());
                auto out = report.getSiblingFile(mode == 1 ? "reference-energy.png" : mode == 2 ? "reference-peaks.png" : "reference-detail.png").createOutputStream();
                if (out) { out->setPosition(0); out->truncate(); }
                juce::PNGImageFormat png; referenceSaved = referenceSaved && out && png.writeImageToStream(capture, *out);
            }
            preview.waveformMode.setSelectedId(1, juce::sendNotificationSync);
            preview.timeline.pixelsPerSecond = (preview.viewport.getWidth() - 24) / preview.engine.project.duration(); preview.refresh(); preview.viewport.setViewPosition(0, 0);
            { auto out = report.getSiblingFile("reference-overview.png").createOutputStream(); juce::PNGImageFormat png;
                referenceSaved &= out && png.writeImageToStream(preview.createComponentSnapshot(preview.getLocalBounds()), *out); }
            { auto out = report.getSiblingFile("reference-waveform.png").createOutputStream(); juce::PNGImageFormat png;
                preview.setSize(1850, 940); preview.timeline.pixelsPerSecond = 1000 / preview.engine.project.duration(); preview.refresh();
                referenceSaved &= out && png.writeImageToStream(preview.timeline.createComponentSnapshot({0, 0, 1010, preview.timeline.laneTop(1)}), *out); }
            check(referenceSaved && !actual.media->waveformRms.empty(), "real reference renders energy overview, four-bar grid, transient detail and peak views");
        }
        check(audioDigest(reference) == digest && analysisRecordFile(reference).loadFileAsString() == sidecar,
              "reference audio and its analysis sidecar remain unchanged after verification");
    }
}
}
