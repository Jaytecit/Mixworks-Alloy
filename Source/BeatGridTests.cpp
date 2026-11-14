#include "Workstation.h"
#include "Beatmatch.h"
#include "AutoArrangement.h"
#include "GridCorrectionUI.h"
#include "WaveformDetailCache.h"
#include <cmath>

namespace mixworks
{
void runBeatGridTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report, const juce::File& reference)
{
    const auto args = juce::StringArray::fromTokens(juce::JUCEApplicationBase::getCommandLineParameters(), true);
    if (args.contains("--waveform-alignment-only"))
    {
        // Authored original-file playback, not the in-memory PCM path covered by
        // older waveform tests. No detector, audio device or timing benchmark.
        constexpr int sr = 8000, count = sr * 160, bucket = count / 4000;
        const std::array<int, 3> onsets {590, 480590, 1148590};
        auto media = std::make_shared<Media>(); media->name = "Known sample onsets / streamed original";
        media->file = report.getSiblingFile("waveform-original.wav"); media->sampleRate = sr;
        juce::AudioBuffer<float> audio(1, count); audio.clear();
        media->peaks.assign(4000, 0); media->waveformRms.assign(4000, 0);
        for (int first : onsets) for (int i = first; i < first + 8; ++i)
        { audio.setSample(0, i, .8f); media->peaks[(size_t)(i / bucket)] = .8f; }
        for (int i : {OriginalAudio::blockFrames - 1, OriginalAudio::blockFrames})
        { audio.setSample(0, i, .5f); media->peaks[(size_t)(i / bucket)] = .5f; }
        { juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream = media->file.createOutputStream();
          auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(sr).withNumChannels(1).withBitsPerSample(24));
          check(writer && writer->writeFromAudioSampleBuffer(audio, 0, count), "write bounded original with three independent sample-stamped transients"); }
        audio.setSize(0, 0);
        const auto digest = audioDigest(media->file);
        media->analysisSource = {digest, media->file.getSize(), sr, count, 1};
        media->manualBpm = 120; media->manualBeatOrigin = onsets[0] / double(sr);
        media->original = std::make_shared<OriginalAudio>(media->file, sr, 1, count);
        const auto cold = media->waveformRange(.06, .062);
        check(!cold.ready && cold.peak == 0, "cold waveform detail queues without presenting an inaccurate overview peak");
        auto uiOwner = std::make_unique<Workstation>(false); auto& ui = *uiOwner; ui.stopTimer(); ui.setSize(1250, 800);
        Clip clip; clip.media = media; clip.length = media->duration(); ui.engine.project.clips = {clip}; ui.refresh();
        const auto document = juce::JSON::toString(projectToJson(ui.engine.project));
        auto catalogue = media->catalogueCopy();
        auto editor = std::make_unique<GridCorrectionEditor>(ui.engine, catalogue, "", [](const auto&, bool) { return juce::Result::ok(); });
        editor->stopTimer(); editor->setPlaybackMedia(media); editor->selectPage(1); editor->zoom.setSelectedId(1, juce::sendNotificationSync);
        check(editor->draft.downbeat == media->manualBeatOrigin, "opening source grid retains the full saved sample-position precision");
        auto awaitRange = [&](const std::function<WaveformRange()>& query)
        {
            auto range = query(); const auto deadline = juce::Time::getMillisecondCounterHiRes() + 5000;
            while (!range.ready && juce::Time::getMillisecondCounterHiRes() < deadline) { juce::Thread::sleep(1); range = query(); }
            return range;
        };
        auto saveImage = [&](const juce::Image& image, const juce::String& name)
        {
            auto stream = report.getSiblingFile(name + ".png").createOutputStream(); juce::PNGImageFormat png;
            check(stream && png.writeImageToStream(image, *stream), "capture " + name);
        };
        for (int first : onsets)
        {
            const double at = first / double(sr);
            check(std::abs(media->original->sample(0, first) - .8f) < 1.e-6f, "independent original sample confirms authored transient at " + juce::String(first));
            const auto before = awaitRange([&] { return media->waveformRange(at - .004, at - .002); });
            const auto after = awaitRange([&] { return media->waveformRange(at + .003, at + .005); });
            const auto transient = awaitRange([&] { return media->waveformRange(at, at + .0005); });
            check(before.ready && after.ready && before.peak == 0 && after.peak == 0,
                "zoomed original waveform cannot smear a transient into neighbouring silence / " + juce::String(first));
            check(transient.ready && transient.peak > .79f, "zoomed original waveform retains the actual transient / " + juce::String(first));
            editor->cursor.setValue(at + .1, juce::dontSendNotification); editor->viewStart = std::max(0.0, at - .4);
            auto image = editor->createComponentSnapshot(editor->getLocalBounds());
            const auto deadline = juce::Time::getMillisecondCounterHiRes() + 5000;
            while (editor->waveformDetailPending && juce::Time::getMillisecondCounterHiRes() < deadline)
            { juce::Thread::sleep(5); image = editor->createComponentSnapshot(editor->getLocalBounds()); }
            check(!editor->waveformDetailPending, "bounded background waveform detail becomes ready / " + juce::String(first));
            const int x = juce::roundToInt(editor->xAtTime(at - .008));
            const int y = editor->waveform.getCentreY() + 24 - (editor->waveform.getHeight() - 104) / 4;
            check(image.getPixelAt(x, y) == colour::panel, "source editor shows silence before the sample-aligned marker / " + juce::String(first));
            const int sampleX = juce::roundToInt(editor->waveform.getX() + (first / double(sr) - editor->viewStart) * editor->waveform.getWidth() / editor->viewSpan);
            const auto marker = (first - onsets[0]) % (sr * 2) == 0 ? colour::orange : juce::Colour(0xff63808c);
            check(image.getPixelAt(sampleX, y) == marker, "painted 4/4 marker occupies the independently authored sample column / " + juce::String(first));
            saveImage(image, "waveform-source-" + juce::String(first));
        }
        editor->zoom.setSelectedId(8, juce::sendNotificationSync); editor->setCursor(onsets[0] / double(sr), false);
        saveImage(editor->createComponentSnapshot(editor->getLocalBounds()), "waveform-source-10ms");
        const auto boundary = awaitRange([&]
        {
            WaveformRange range;
            range.ready = media->original->waveformRange(OriginalAudio::blockFrames - 2, OriginalAudio::blockFrames + 2, range.peak, range.rms);
            return range;
        });
        check(boundary.ready && std::abs(boundary.peak - .5f) < 1.e-7 && std::abs(boundary.rms - std::sqrt(.125)) < 1.e-7,
            "sample detail across a streaming block boundary has independently known peak and RMS");
        check(media->waveformRange(0, .3).ready && media->waveformRange(0, .3).peak > .79f,
            "wide views retain the compact source overview");
        // Keep the grid, clip and IN/OUT overlays in the same columns when
        // comparing pixels. The short onset can sit directly below the IN tab.
        auto silence = media->catalogueCopy();
        silence->samples.setSize(1, count); silence->samples.clear();
        silence->peaks.clear(); silence->waveformRms.clear();
        auto differentRows = [&](const juce::Image& image, const juce::Image& silentImage, int x)
        {
            int different = 0;
            const auto bounds = ui.timeline.clipBounds(ui.engine.project.clips[0]);
            for (int y = (int)bounds.getY() + 32; y < std::min(image.getHeight(), (int)bounds.getBottom() - 30); ++y)
                if (image.getPixelAt(x, y) != silentImage.getPixelAt(x, y)) ++different;
            return different;
        };
        for (double scale : {200., 800.})
        {
            ui.timeline.pixelsPerSecond = scale; ui.refresh();
            const juce::Rectangle<int> capture {0, 0, 700, Timeline::rulerHeight + 132};
            const auto image = ui.timeline.createComponentSnapshot(capture);
            ui.engine.project.clips.front().media = silence;
            const auto silentImage = ui.timeline.createComponentSnapshot(capture);
            ui.engine.project.clips.front().media = media;
            const double at = onsets[0] / double(sr);
            check(differentRows(image, silentImage, Clip::gridPixel(at - .008, scale)) == 0, "timeline original waveform does not precede the true onset at scale " + juce::String(scale));
            check(differentRows(image, silentImage, Clip::gridPixel(at, scale)) > 3, "timeline waveform retains the true onset at its source grid marker / " + juce::String(scale));
            saveImage(image, "waveform-timeline-" + juce::String((int)scale));
        }
        clip.offset = .02; clip.start = 3;
        const double local = onsets[0] / double(sr) - clip.offset;
        check(clip.waveformRange(local - .004, local - .002).peak == 0 && clip.waveformRange(local, local + .0005).peak > .79f,
            "trimmed and moved original uses the same sample clock for waveform detail");
        clip.length -= clip.offset; clip.reversed = true;
        const double reverse = clip.length - (onsets[0] / double(sr) - clip.offset);
        check(clip.waveformRange(reverse + .002, reverse + .004).peak == 0 && clip.waveformRange(reverse - .0005, reverse).peak > .79f,
            "reversed original detail retains exact source position and neighbouring silence");
        check(audioDigest(media->file) == digest && media->manualBeatOrigin == onsets[0] / double(sr)
            && document == juce::JSON::toString(projectToJson(ui.engine.project)), "waveform review preserves original bytes, saved grid and timeline geometry");
        saveImage(ui.createComponentSnapshot(ui.getLocalBounds()), "waveform-workspace-version");
        // Deterministic contention/eviction substitute, without a capacity run:
        // exact display measurements must survive unavailable future PCM reads.
        WaveformDetailCache cache(2);
        int reads=0; bool available=false; float peak=1,rms=1;
        const auto measure=[&](float& p,float& r) { ++reads; if(!available) return false; p=.75f; r=.25f; return true; };
        check(!cache.query(10,100,110,peak,rms,measure) && peak==0 && rms==0, "pending detail is not remembered as silence");
        available=true;
        check(cache.query(10,100,110,peak,rms,measure) && peak==.75f && rms==.25f && reads==2,
              "pending column retries and retains its measured peak and RMS");
        available=false;
        check(cache.query(10,100,110,peak,rms,measure) && peak==.75f && rms==.25f && reads==2,
              "repaint keeps measured detail when the PCM reader becomes unavailable");
        check(!cache.query(10,101,110,peak,rms,measure) && !cache.query(11,100,110,peak,rms,measure),
              "changed sample interval and new source identity never reuse stale columns");
        available=true; cache.query(10,110,120,peak,rms,measure); cache.query(10,100,110,peak,rms,measure);
        cache.query(10,120,130,peak,rms,measure); available=false;
        check(cache.query(10,100,110,peak,rms,measure) && !cache.query(10,110,120,peak,rms,measure),
              "bounded summary cache retains recent columns and retries an evicted interval");
        const int first=onsets[0]; float p=0,r=0;
        const auto originalDetail=awaitRange([&] {
            WaveformRange value; value.ready=media->original->waveformRange(first,first+8,p,r); value.peak=p; value.rms=r; return value;
        });
        check(originalDetail.ready && originalDetail.peak>.79f, "original waveform path supplies exact cached detail");
        { OriginalAudio::Realtime callback;
          check(!media->original->waveformRange(first,first+8,p,r), "display cache is not admitted to the audio callback"); }
        media->original->failure=true;
        check(!media->original->waveformRange(first,first+8,p,r) && p==0 && r==0,
              "failed source does not hide behind previously measured display detail");
        media->original->failure=false;
        return;
    }
    if (args.contains("--grid-attack-only"))
    {
        constexpr int sr = 22050, seconds = 24;
        constexpr double onset = .07375, period = .5;
        auto media = std::make_shared<Media>(); media->name = "Measured kick attacks"; media->sampleRate = sr;
        media->samples.setSize(1, sr * seconds); media->samples.clear();
        for (double beat = onset; beat < seconds; beat += period)
            for (int i = 0; i < (int)(.23 * sr) && (int)(beat * sr) + i < media->frames(); ++i)
            {
                const double t = i / (double)sr, envelope = std::min(1., t / .0015) * std::exp(-10 * t);
                media->samples.setSample(0, (int)(beat * sr) + i, (float)(.8 * envelope * std::sin(juce::MathConstants<double>::twoPi * (150 * t - 180 * t * t))));
            }
        MusicAnalysis old; old.bpm = 120.025; old.beatOrigin = onset + .060; old.downbeatOrigin = old.beatOrigin + 8 * 60 / old.bpm;
        old.downbeatConfidence = .8; old.tempoConfidence = .9; old.gridErrorMs = 15; old.key = 4; old.keyConfidence = .8;
        media->analysis = old;
        const bool refined = refineAttackTiming(media->audioView(), sr, media->analysis, 0, -1);
        check(refined
              && std::abs(media->analysis.beatOrigin - onset) < .003 && std::abs(media->analysis.bpm - 120) < .01,
              "late cached grid is refitted to clear PCM attack beginnings: " + juce::String(media->analysis.beatOrigin, 6) + " s / " + juce::String(media->analysis.bpm, 5) + " BPM");
        check(std::abs(media->analysis.downbeatOrigin - onset - 4) < .003 && media->analysis.key == old.key && media->analysis.keyConfidence == old.keyConfidence,
              "refinement preserves the chosen bar ordinal and existing key analysis");
        const auto corrected = media->analysis;
        check(!refineAttackTiming(media->audioView(), sr, media->analysis, 0, -1) && media->analysis.beatOrigin == corrected.beatOrigin,
              "completed timing attempts are reused without repeat refinement");
        auto manual = old;
        check(!refineAttackTiming(media->audioView(), sr, manual, 126, -1) && manual.bpm == old.bpm && manual.beatOrigin == old.beatOrigin
              && !refineAttackTiming(media->audioView(), sr, manual, 0, .37) && manual.attackTimingRevision == 0,
              "manual BPM and reviewed downbeat choices bypass automatic timing changes");
        juce::AudioBuffer<float> ambiguous(1, sr * seconds); juce::FloatVectorOperations::fill(ambiguous.getWritePointer(0), .2f, ambiguous.getNumSamples());
        auto retained = old; refineAttackTiming(ambiguous, sr, retained, 0, -1);
        check(retained.beatOrigin == old.beatOrigin && retained.bpm == old.bpm && retained.attackTimingRevision == 1,
              "audio without distinct repeating attack boundaries keeps its existing grid");
        auto cancelled = old;
        check(!refineAttackTiming(media->audioView(), sr, cancelled, 0, -1, std::make_shared<std::atomic<bool>>(true)) && cancelled.attackTimingRevision == 0,
              "cancelled timing refinement leaves the stored attempt and grid untouched");
        media->analysisSource = {juce::String::repeatedString("a",64), 1000, sr, media->frames(), 1};
        const int bucket = media->frames() / 4000; media->peaks.resize((size_t)((media->frames() + bucket - 1) / bucket), .8f);
        const auto decoded = parseAnalysisRecord(analysisRecordToJson(media->analysisRecord()));
        check(decoded.reusableAutomatic && decoded.record && decoded.record->analysis.attackTimingRevision == 1
              && decoded.record->analysis.beatOrigin == corrected.beatOrigin && decoded.record->analysis.key == old.key,
              "timing refinement round-trips in the existing analysis cache without discarding other analysis");
        {
            const auto file = report.getSiblingFile("cached-grid.wav");
            juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> output = file.createOutputStream();
            auto writer = wav.createWriterFor(output, juce::AudioFormatWriterOptions().withSampleRate(sr).withNumChannels(1).withBitsPerSample(24));
            const bool written = writer && writer->writeFromAudioSampleBuffer(media->samples, 0, media->frames()); writer.reset();
            auto stale = media->catalogueCopy(); stale->file = file; stale->analysis = old;
            stale->analysisSource = {audioDigest(file), file.getSize(), sr, media->frames(), 1};
            const auto stored = writeAnalysisRecord(file, stale->analysisRecord());
            check(written && stored.isEmpty(), "seed a small source with existing late-grid analysis in the isolated test store");
            if (written && stored.isEmpty())
            {
                Workstation ui(false); ui.stopTimer(); ui.engine.library = {stale};
                AutoArrangementOptions options; options.trackCount = 1; options.startingId = stale->id;
                options.preset = TransitionPreset::alignOnly; options.sectionTransitions = false; options.transitions = AutoTransitionStyle::current;
                struct Wait final : juce::Timer
                {
                    Workstation& ui; double started = juce::Time::getMillisecondCounterHiRes();
                    explicit Wait(Workstation& w) : ui(w) {}
                    void timerCallback() override
                    {
                        if (!ui.busy || juce::Time::getMillisecondCounterHiRes() - started > 10000)
                        { stopTimer(); juce::MessageManager::getInstance()->stopDispatchLoop(); }
                    }
                } wait(ui);
                ui.startAutoArrangement(options); wait.startTimer(10); juce::MessageManager::getInstance()->runDispatchLoop();
                bool aligned = !ui.busy && ui.engine.project.clips.size() == 1;
                if (aligned)
                {
                    const auto& clip = ui.engine.project.clips.front();
                    aligned = clip.media->analysisFromCache && clip.media->analysis.attackTimingRevision == 1
                        && std::abs(std::remainder(ui.engine.project.gridClock().beatsAt(clip.start + clip.audioTimelineTime(onset)),1.0)) < .006;
                }
                check(aligned, "actual Auto Arrange hydrates saved analysis and rebuilds its shortlist on refined timing: " + ui.status.replaceCharacters("\r\n", "  "));
                juce::AudioFormatManager formats; formats.registerBasicFormats(); int analysed = 0;
                const auto warm = readMedia(formats, file, [&](MediaReadStage stage) { if (stage == MediaReadStage::analyse) ++analysed; });
                check(warm.media && warm.media->original && !warm.media->cache && warm.media->analysisFromCache && analysed == 0
                      && warm.media->analysis.attackTimingRevision == 1 && warm.media->analysis.key == old.key
                      && warm.media->manualBeatOrigin == -1 && warm.media->analysisSource == stale->analysisSource,
                      "subsequent loads reuse refined analysis and original audio without neural analysis or loss of source identity");
            }
        }
        Project visual;
        for (int route = 0; route < 3; ++route)
        {
            Project p; p.tempo.origin = 0; p.tempo.points.clear(); p.bpm = media->bpm();
            Clip clip; clip.media = media; clip.length = media->duration(); clip.start = route == 1 ? 2 : 0;
            clip.pendingPlacement = route == 2 ? PendingPlacement::none : route == 1 ? PendingPlacement::drop : PendingPlacement::append;
            p.clips = {clip};
            auto placed = prepareTempoMap(p, route == 2, 0, 0, 0, {}, TransitionPreset::alignOnly, false);
            bool aligned = placed.error.isEmpty();
            if (aligned)
            {
                const auto& c = placed.project.clips.front(); const auto clock = placed.project.gridClock();
                for (double source : {onset, onset + 4, onset + 12})
                {
                    const double time = c.start + c.audioTimelineTime(source);
                    aligned &= std::abs(std::remainder(clock.beatsAt(time), 1.0)) < .006;
                }
                Engine render; render.sampleRate = sr; render.project = placed.project; render.project.masterDb = 0;
                const double time = c.start + c.audioTimelineTime(onset + 4), grid = clock.timeAtBeat(std::round(clock.beatsAt(time)));
                juce::AudioBuffer<float> pcm(2, 512); juce::MidiBuffer midi; render.render(pcm, 512, grid - .01, midi);
                int first = 0; while (first < pcm.getNumSamples() && std::abs(pcm.getSample(0, first)) < .01f) ++first;
                aligned &= first < pcm.getNumSamples() && std::abs(first / (double)sr - .01) < .004;
                aligned &= c.waveformRange(time - c.start + .002, time - c.start + .006).peak > .05f;
                if (route == 0) visual = placed.project;
            }
            check(aligned, "source attacks, rendered PCM and waveform agree with the ruler after " + juce::String(route == 0 ? "append" : route == 1 ? "drop" : "Auto Arrange") + ": " + placed.error);
        }
        if (!visual.clips.empty())
        {
            Workstation ui(false); ui.stopTimer(); ui.setSize(1250, 850); ui.engine.project = visual;
            Clip reference = visual.clips.front(); reference.id = juce::Uuid().toString(); reference.track = 1;
            reference.start += media->analysis.beatOrigin - onset; ui.engine.project.clips.push_back(reference);
            ui.refresh(); ui.timeline.pixelsPerSecond = 800; ui.timeline.updateSize();
            const int x = (int)((visual.clips.front().start + onset - .1) * 800);
            ui.viewport.setViewPosition(std::max(0,x),0);
            auto stream = report.getSiblingFile("attack-grid.png").createOutputStream(); juce::PNGImageFormat png;
            check(stream && png.writeImageToStream(ui.timeline.createComponentSnapshot({std::max(0,x),0,1050,ui.timeline.laneTop(2)}), *stream),
                  "capture native automatic placement beside an independently positioned onset reference");
        }
        const int exampleIndex = args.indexOf("--grid-example"), pcmIndex = args.indexOf("--grid-pcm");
        if (exampleIndex >= 0 && exampleIndex + 1 < args.size() && pcmIndex >= 0 && pcmIndex + 1 < args.size())
        {
            const auto example = juce::JSON::parse(juce::File(args[exampleIndex + 1].unquoted()));
            const auto parsed = parseAnalysisRecord(example["record"]);
            juce::MemoryMappedFile mapped(juce::File(args[pcmIndex + 1].unquoted()), juce::MemoryMappedFile::readOnly);
            const bool valid = parsed.reusableAutomatic && parsed.record && mapped.getData()
                && mapped.getSize() == PcmStore::headerBytes + (size_t)parsed.record->source.frames * parsed.record->source.channels * sizeof(float);
            check(valid, "read the exact saved example PCM without loading the production app or changing its cache");
            if (valid)
            {
                const auto& s = parsed.record->source; auto analysis = parsed.record->analysis;
                const PcmView audio(reinterpret_cast<const float*>(static_cast<const char*>(mapped.getData()) + PcmStore::headerBytes),s.channels,s.frames,s.frames);
                refineAttackTiming(audio,s.sampleRate,analysis,0,-1);
                check(std::abs(analysis.beatOrigin - .071633) < .006 && std::abs(analysis.bpm - 126) < .01,
                      "real cached example grid meets the independently measured opening attack: " + juce::String(analysis.beatOrigin,6) + " s / " + juce::String(analysis.bpm,5) + " BPM");
                auto* result = new juce::DynamicObject(); result->setProperty("beforeOrigin",parsed.record->analysis.beatOrigin); result->setProperty("afterOrigin",analysis.beatOrigin);
                result->setProperty("beforeBpm",parsed.record->analysis.bpm); result->setProperty("afterBpm",analysis.bpm); result->setProperty("rmsMs",analysis.gridErrorMs);
                report.getSiblingFile("real-attack-timing.json").replaceWithText(juce::JSON::toString(juce::var(result)));
            }
        }
        return;
    }
    auto first = std::make_shared<Media>(); first->sampleRate = 8000; first->manualBpm = 120; first->manualBeatOrigin = 0.213;
    first->samples.setSize(1, 8000 * 24); first->samples.clear();
    auto second = std::make_shared<Media>(); second->sampleRate = 8000; second->manualBpm = 144; second->manualBeatOrigin = 1.213;
    second->samples = first->samples;
    Clip a; a.media = first; a.length = 24;
    Clip b; b.media = second; b.track = 1; b.length = 24;
    Project project; project.clips = {a, b};
    auto matched = prepareTempoMap(project, true, 16);
    const bool matchedBoth = matched.error.isEmpty() && matched.matched == 2 && matched.project.clips.size() == 2;
    check(matchedBoth, "two sources prepare for grid and transition checks: " + matched.error);
    if (!matchedBoth) return;
    {
        Project moved = matched.project;
        const auto id = moved.clips[0].id;
        const double freeStart = moved.clips[0].start + .017;
        const auto free = moved.moveClip(id, freeStart, false);
        const auto clock = moved.gridClock();
        const auto& clip = moved.clips[0];
        const double pulse = clock.beatsAt(clip.start + clip.firstBeatTime());
        check(free.wasOk() && std::abs(clip.start - freeStart) < 1e-7 && std::abs(std::remainder(pulse, 1.0)) > .01,
              "a free body move retains the requested off-grid source pulse: " + free.getErrorMessage());
        const double target = clock.timeAtBeat(clock.beatsAt(clip.start) + 8.18);
        const auto result = moved.moveClip(id, target, true, 0, false);
        const auto& snapped = moved.clips[0];
        check(result.wasOk() && std::abs(clock.beatsAt(snapped.start + snapped.firstBeatTime()) - std::round(pulse + 8.18)) < 1e-7,
              "snapped body movement puts the source pulse on the project grid: " + result.getErrorMessage());
    }
    {
        auto manual = matched.project;
        auto& out = manual.clips[0]; auto& in = manual.clips[1];
        const auto clock = manual.gridClock();
        const double begin = clock.timeAtBeat(clock.beatsAt(std::max(out.start, in.start)) + 4.17);
        const double end = clock.timeAtBeat(clock.beatsAt(std::min(out.end(), in.end())) - 4.17);
        out.outroAnchor = out.sourceElapsed(begin - out.start); in.introAnchor = in.sourceElapsed(begin - in.start);
        const Project::TransitionLink pair {out.id, in.id};
        manual.transitionLinks.push_back(pair);
        manual.transitionLinks.back().frame = std::array<double, 2>{*out.outroAnchor, out.sourceElapsed(end - out.start)};
        const auto valid = manual.validateTransitionLinks();
        check(valid.wasOk() && end > begin, "authored transition frame fits inside both sources: " + valid.getErrorMessage());
        if (valid.failed() || end <= begin) return;

        auto following = manual;
        const double outBeat = clock.beatsAt(out.start), inBeat = clock.beatsAt(in.start);
        const double targetBeat = std::round(outBeat + 1.18);
        const auto followed = following.moveClipFollowing(pair.outgoing, clock.timeAtBeat(outBeat + 1.18), true);
        check(followed.wasOk() && std::abs(clock.beatsAt(following.clips[0].start) - targetBeat) < 1e-7
              && std::abs(clock.beatsAt(following.clips[1].start) - inBeat - (targetBeat - outBeat)) < 1e-7,
              "snapped title-handle movement puts the edge on the project grid and carries the following track: " + followed.getErrorMessage());
        for (const auto edit : {Project::TransitionEdit::moveWindow, Project::TransitionEdit::resizeStart, Project::TransitionEdit::resizeEnd,
             Project::TransitionEdit::moveOutgoing, Project::TransitionEdit::moveIncoming})
        {
            auto trial = manual;
            const bool movingOut = edit == Project::TransitionEdit::moveOutgoing, movingIn = edit == Project::TransitionEdit::moveIncoming;
            const bool resizingEnd = edit == Project::TransitionEdit::resizeEnd;
            const double gestureStart = movingOut ? out.start : movingIn ? in.start : resizingEnd ? end : begin;
            const double requestedBeat = clock.beatsAt(gestureStart) + (movingOut ? 1.18 : -1.18);
            const auto result = trial.editTransition(pair.incoming, edit, clock.timeAtBeat(requestedBeat) - gestureStart, true);
            const auto* link = trial.transitionFor(pair.incoming);
            const auto after = link ? trial.transitionRange(*link) : juce::Range<double>();
            const double actual = movingOut ? trial.clips[0].start : movingIn ? trial.clips[1].start
                : resizingEnd ? after.getEnd() : after.getStart();
            check(result.wasOk() && link && std::abs(clock.beatsAt(actual) - std::round(requestedBeat)) < 1e-7,
                  "transition gesture " + juce::String((int) edit) + " snaps to the project grid: " + result.getErrorMessage());
        }
        for (bool intro : {false, true})
        {
            auto trial = manual;
            const auto& selected = intro ? in : out;
            const double requestedBeat = clock.beatsAt(begin) + 1.18;
            const double snappedTime = clock.timeAtBeat(std::round(requestedBeat));
            const auto result = trial.moveTransitionAnchor(selected.id, intro, clock.timeAtBeat(requestedBeat), true);
            const auto& editedOut = trial.clips[0]; const auto& editedIn = trial.clips[1];
            const auto& anchor = intro ? editedIn.introAnchor : editedOut.outroAnchor;
            const double join = intro ? begin : snappedTime;
            check(result.wasOk() && anchor && std::abs(*anchor - selected.sourceElapsed(snappedTime - selected.start)) < 1e-7
                  && editedOut.start == out.start && std::abs(editedOut.start + editedOut.transitionTime(false) - join) < 1e-7
                  && std::abs(editedIn.start + editedIn.transitionTime(true) - join) < 1e-7,
                  juce::String(intro ? "snapped IN selects a source point at fixed OUT: " : "snapped OUT moves the join onto the project grid: ") + result.getErrorMessage());
        }
    }
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
        auto guidesOwner = std::make_unique<Workstation>(false); auto& guides = *guidesOwner; guides.stopTimer(); guides.setSize(1440, 940);
        Clip offset; offset.media = first; offset.start = 0.137; offset.length = 24; offset.beatmatched = true; offset.tempoSourceBpm = 120;
        guides.engine.project.tempo.points = {{0, 120}}; offset.tempoMap = std::make_shared<const TempoMap>(guides.engine.project.tempo);
        guides.engine.project.clips = {offset}; guides.timeline.pixelsPerSecond = 81.3; guides.refresh();
        const auto snapshot = guides.timeline.createComponentSnapshot({0, 0, 650, guides.timeline.laneTop(1)});
        const int beatX = Clip::gridPixel(4.5, 81.3), sourceX = Clip::gridPixel(4.35, 81.3);
        const int middle = guides.timeline.laneTop(0) + 65, tick = juce::roundToInt(guides.timeline.clipBounds(offset).getBottom() - 9);
        check(snapshot.getPixelAt(beatX, middle).getPerceivedBrightness() > snapshot.getPixelAt(beatX + 2, middle).getPerceivedBrightness()
              && snapshot.getPixelAt(sourceX, middle) == snapshot.getPixelAt(sourceX + 2, middle),
              "native full-height track guides use the timeline grid even when a source is deliberately offset");
        check(snapshot.getPixelAt(sourceX, tick) != snapshot.getPixelAt(sourceX + 2, tick),
              "a short source-beat tick still exposes the real timing offset without drawing a second full-height grid");
        auto image = report.getSiblingFile("shared-grid-offset-source.png").createOutputStream(); juce::PNGImageFormat png;
        check(image && png.writeImageToStream(snapshot, *image), "capture shared timeline guides with independently visible source timing");
    }
    {
        auto pixelOwner = std::make_unique<Workstation>(false); auto& pixelUi = *pixelOwner; pixelUi.stopTimer();
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
    // An independent tempo map survives removal and replacement. Measure each
    // surviving clip's full grid through both stopped and live placement.
    {
        auto replacementOwner = std::make_unique<Workstation>(false); auto& replacement = *replacementOwner; replacement.stopTimer(); replacement.setSize(1440, 940);
        for (int removed : {0, 1}) for (bool live : {false, true})
        {
            replacement.engine.project = automatic.project;
            for (auto& point : replacement.engine.project.tempo.points) point.clipId.clear();
            const auto removedId = automatic.project.clips[(size_t) removed].id;
            replacement.selectedClip = removedId;
            const auto retained = replacement.engine.project.tempo;
            replacement.removeClip();
            bool clock = !replacement.busy && replacement.engine.project.clips.size() == 1 && !replacement.engine.project.clipFor(removedId);
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
        auto removalOwner = std::make_unique<Workstation>(false); auto& removal = *removalOwner; removal.stopTimer(); removal.engine.project = prepared.project;
        const auto removedId = coincident.clips[1].id;
        removal.selectedClip = removedId; removal.removeClip();
        if (removal.busy)
        {
            struct Wait final : juce::Timer
            {
                Workstation& ui;
                const double deadline = juce::Time::getMillisecondCounterHiRes() + 10000;
                explicit Wait(Workstation& owner) : ui(owner) {}
                void timerCallback() override
                {
                    if (!ui.busy || juce::Time::getMillisecondCounterHiRes() >= deadline)
                    { stopTimer(); juce::MessageManager::getInstance()->stopDispatchLoop(); }
                }
            } wait(removal);
            wait.startTimer(10); juce::MessageManager::getInstance()->runDispatchLoop();
        }
        const std::vector<TempoPoint> manualPoints {{0, 120}, {12, 120, false}};
        const bool removed = prepared.error.isEmpty() && !removal.busy && removal.engine.project.clips.size() == 1
            && !removal.engine.project.clipFor(removedId) && removal.engine.project.tempo.points == manualPoints;
        check(removed, "removing an owned ramp target retains the coincident manual tempo marker: " + removal.status);
        if (!removed) return;
        const auto saved = parseProject(juce::JSON::toString(projectToJson(removal.engine.project)));
        check(saved.error.isEmpty() && saved.project.tempo.points == manualPoints,
              "the surviving manual tempo marker persists at its original time: " + saved.error);
        removal.undo(false);
        const bool undo = removal.engine.project.clips.size() == 2 && removal.engine.project.clipFor(removedId)
            && removal.engine.project.tempo.points == prepared.project.tempo.points;
        removal.undo(true);
        check(undo && removal.engine.project.clips.size() == 1 && removal.engine.project.tempo.points == manualPoints,
              "Undo restores both coincident markers and Redo removes only the owned marker: " + removal.status);
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
        auto renderOwner = std::make_unique<Engine>(); auto& render = *renderOwner; render.sampleRate = 22050; render.project = audible.project; render.project.masterDb = 0;
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
        auto transitionOwner = std::make_unique<Workstation>(false); auto& transitionUi = *transitionOwner; transitionUi.stopTimer(); transitionUi.setSize(1440, 940);
        transitionUi.engine.project = audible.project; transitionUi.timeline.pixelsPerSecond = 150; transitionUi.refresh();
        transitionUi.viewport.setViewPosition((int) ((audible.project.clips[1].start - 0.5) * 150), 0);
        const auto snapshot = transitionUi.createComponentSnapshot(transitionUi.getLocalBounds()); auto out = report.getSiblingFile("beat-grid-transition.png").createOutputStream();
        if (out) { out->setPosition(0); out->truncate(); juce::PNGImageFormat format; format.writeImageToStream(snapshot, *out); }
    }

    // A real timeline paint must distinguish PCM transients within one overview bucket.
    auto uiOwner = std::make_unique<Workstation>(false); auto& ui = *uiOwner; ui.stopTimer(); ui.setSize(1440, 940);
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
    // Both columns are inside the clip, clear of the IN tab and project guides.
    constexpr int quietColumn = 100, loudColumn = 140;
    check(differentRows(limitedImage, quietColumn, loudColumn) > 15, "native waveform exposes different beat energy even when maximum peaks are equally limited");
    const auto quietBody = dense.waveformRange(0.25, 0.2525), loudBody = dense.waveformRange(0.35, 0.3525);
    check(quietBody.peak == loudBody.peak && quietBody.rms < 0.3 && loudBody.rms > 0.8 && limited->samples.getSample(0, 1) == 0.1f,
          "waveform detail measures energy without normalizing, limiting or changing source PCM");
    limited->peaks.assign(4000, 0.98f); limited->prepareWaveformBody();
    check(dense.waveformRange(0.01, 0.09).rms < 0.3 && dense.waveformRange(0.11, 0.19).rms > 0.8,
          "overview waveform body retains energy differences through worker-prepared buckets");
    ui.waveformMode.setSelectedId(2, juce::sendNotificationSync);
    const auto peaksImage = ui.timeline.createComponentSnapshot({0, 0, 320, Timeline::rulerHeight + 132});
    check(differentRows(peaksImage, quietColumn, loudColumn) == 0, "peak-only display gives equal columns to equally limited peaks");
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
    // This fixture checks the original full-strength accent contrast and tempo-map spacing.
    // The dimmer/default/off pixel and persistence cases live in AlloyTests.
    ui.gridDimmer.setValue(1, juce::sendNotificationSync);
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
    const auto saved = juce::JSON::toString(ui.engine.captureProject()); auto reopenOwner = std::make_unique<Engine>(); auto& reopened = *reopenOwner;
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
            auto previewOwner = std::make_unique<Workstation>(false); auto& preview = *previewOwner; preview.stopTimer(); preview.setSize(1440, 940);
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
