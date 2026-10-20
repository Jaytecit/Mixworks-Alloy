#include "Workstation.h"
#include "ElasticAudio.h"
#include <cmath>

namespace mixworks
{
void runTransitionCutTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    constexpr double sr = 8000, first = 2.0000625, firstEnd = 3.7500625, second = 5.0000625, last = 6.1250625;
    const auto root = report.getParentDirectory().getChildFile("transition-cuts-" + juce::Uuid().toString()); root.createDirectory();
    auto media = std::make_shared<Media>(); media->name = "Transition cutoff source"; media->file = root.getChildFile("source.wav");
    media->sampleRate = sr; media->manualBpm = 120; media->manualBeatOrigin = 0; media->manualKey = 0;
    media->samples.setSize(1, (int) sr * 10); media->peaks.assign(4000, .16f); media->waveformRms.assign(4000, .12f);
    for (int frame = 0; frame < media->frames(); ++frame) media->samples.setSample(0, frame, .12f + .03f * std::sin(frame * .071f));
    {
        juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream = media->file.createOutputStream();
        auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(sr).withNumChannels(1).withBitsPerSample(24));
        check(writer && writer->writeFromAudioSampleBuffer(media->samples, 0, media->frames()), "write owned nonzero source for transition-cut Save/Open/export");
    }
    Project base; base.bpm = base.tempo.fallback = 120; base.followTrackTempo = false; base.masterDb = 0;
    for (int side = 0; side < 3; ++side)
    {
        Clip clip; clip.media = media; clip.track = side; clip.start = side == 0 ? 0 : side == 1 ? 1 : 4; clip.length = 8;
        clip.introAnchor = side == 0 ? 0 : (side == 1 ? first : second) - clip.start;
        clip.outroAnchor = (side == 0 ? first : side == 1 ? second : clip.end()) - clip.start;
        clip.fadeInAnchor = 0; clip.fadeOutAnchor = (side == 0 ? firstEnd : side == 1 ? last : clip.end()) - clip.start;
        base.clips.push_back(clip);
    }
    base.transitionLinksKnown = true;
    base.transitionLinks = {{base.clips[0].id, base.clips[1].id}, {base.clips[1].id, base.clips[2].id}};
    const auto id = base.clips[1].id;
    const auto json = [](const Project& p) { return juce::JSON::toString(projectToJson(p)); };
    const auto isolate = [](Project p, int side) { for (int track = 0; track < trackCount; ++track) p.tracks[(size_t) track].mute = track != side; return p; };
    check(base.validateTransitionLinks().wasOk() && base.transitionRange(base.transitionLinks[0]) == juce::Range<double>(first, firstEnd),
          "cut fixture has independent intro/fade anchors and fractional-sample frame edges");
    const auto head = base.transitionPlaybackFor(base.clips[0]), middle = base.transitionPlaybackFor(base.clips[1]), tail = base.transitionPlaybackFor(base.clips[2]);
    check(!head.startCut && head.endCut == firstEnd && middle.startCut == first && middle.endCut == last && tail.startCut == second && !tail.endCut,
          "a chain compiles each incoming start and outgoing end once, preserving the first head and final tail");
    check(!middle.audibleAt(first - 1.e-9) && middle.audibleAt(first) && middle.audibleAt(last - 1.e-9) && !middle.audibleAt(last),
          "frame cutoffs include the incoming start and exclude the outgoing end exactly");
    auto partial = base; partial.clips = {base.clips[1]}; const auto unresolved = partial.transitionPlaybackFor(partial.clips[0]);
    check(!unresolved.startCut && !unresolved.endCut && partial.validateTransitionLinks().failed(),
          "missing transition partners remain invalid/unknown rather than inventing a cut at zero");
    for (const auto side : {0, 2})
    {
        Engine engine; engine.sampleRate = sr; engine.project = isolate(base, side);
        juce::AudioBuffer<float> audio(2, 128); juce::MidiBuffer midi; engine.render(audio, 128, side == 0 ? .5 : 8, midi);
        check(audio.getMagnitude(0, 128) > .01f, side == 0 ? "the first track keeps audio before its outgoing transition" : "the final track keeps audio after its incoming transition");
    }
    for (const auto mode : {"raw", "prepared", "clip-local", "elastic", "reverse", "loop", "slipped"})
    {
        auto trial = isolate(base, 1); auto& clip = trial.clips[1]; const juce::String name(mode);
        if (name == "prepared" || name == "clip-local")
        {
            auto audio = std::make_shared<PreparedAudio>(); audio->rate = 1; audio->clipLocal = name == "clip-local";
            audio->samples.makeCopyOf(media->samples); clip.prepared = audio;
        }
        if (name == "elastic")
        {
            clip.beatmatched = clip.elasticPlayback = true; clip.tempoSourceBpm = 120;
            clip.tempoMap = std::make_shared<const TempoMap>(base.tempo); clip.elasticSource = std::make_shared<const ElasticSource>(clip);
        }
        if (name == "reverse") clip.reversed = true;
        if (name == "loop") clip.loopSpan = 2;
        if (name == "slipped") clip.audioSlip = .375;
        Engine engine; engine.sampleRate = sr; engine.project = trial;
        const auto state = json(engine.project);
        for (const double edge : {first, last})
        {
            const double at = (std::floor(edge * sr) - 64) / sr;
            juce::AudioBuffer<float> audio(2, 128); juce::MidiBuffer midi; engine.resetPlugins(); engine.render(audio, 128, at, midi);
            bool exact = true; int sounding = 0;
            for (int frame = 0; frame < 128; ++frame)
            {
                const double time = at + frame / sr; const float value = audio.getSample(0, frame);
                const bool audible = time >= first && time < last;
                exact &= std::isfinite(value) && (audible ? std::abs(value) > .001f : value == 0);
                sounding += audible;
            }
            check(exact && sounding > 0 && sounding < 128, name + " samples obey the fractional " + (edge == first ? "incoming start" : "outgoing end"));
        }
        check(json(engine.project) == state, name + " cutoff leaves source clocks, audio slip, automation and frame geometry unchanged");
    }
    for (const auto mode : {TransitionBandMode::none, TransitionBandMode::manual, TransitionBandMode::crossfade, TransitionBandMode::fadeIn, TransitionBandMode::fadeOut})
    {
        auto trial = base; const auto result = trial.setTransitionBand(id, 0, {mode, .25, 0, .5});
        const auto compiled = trial.transitionPlaybackFor(trial.clips[1]);
        check(result.wasOk() && compiled.startCut == first && !compiled.audibleAt(first - .001),
              "volume mode/partial strength cannot disable the incoming cutoff: " + juce::String((int) mode));
    }
    {
        auto trial = isolate(base, 0); trial.effectTailSeconds = .5; trial.clips[0].controls.eqDb[0] = 12;
        Engine engine; engine.sampleRate = sr; engine.project = trial;
        juce::AudioBuffer<float> audio(2, 256); juce::MidiBuffer midi;
        const double at = (std::floor(firstEnd * sr) - 64) / sr; engine.render(audio, 256, at, midi);
        bool silent = audio.getMagnitude(0, 64) > .001f;
        for (int frame = 65; frame < 256; ++frame) silent &= audio.getSample(0, frame) == 0;
        check(silent, "outgoing cutoff also mutes real EQ ringing despite a configured effect-tail window");
        auto prefix = std::make_shared<Media>(); prefix->sampleRate = sr; prefix->samples.makeCopyOf(media->samples);
        const int zeroFrom = (int) std::ceil((first - base.clips[1].start) * sr); prefix->samples.clear(zeroFrom, prefix->frames() - zeroFrom);
        trial = isolate(base, 1); trial.clips[1].media = prefix; trial.clips[1].controls.eqDb[0] = 12;
        engine.project = trial; engine.resetPlugins(); engine.render(audio, 256, (std::floor(first * sr) - 64) / sr, midi);
        check(audio.getMagnitude(0, 256) == 0, "excluded incoming source cannot excite an EQ tail that leaks across its start cutoff");
    }
    auto moved = base;
    check(moved.editTransition(id, Project::TransitionEdit::moveWindow, .25, false).wasOk()
        && moved.transitionPlaybackFor(moved.clips[1]).startCut == first + .25
        && moved.transitionPlaybackFor(moved.clips[0]).endCut == firstEnd + .25,
          "moving a frame updates both cutoffs without modifying the clips' audio clocks");
    const auto resized = moved.editTransition(id, Project::TransitionEdit::resizeEnd, -.25, false);
    check(resized.wasOk()
        && std::abs(moved.transitionPlaybackFor(moved.clips[0]).endCut.value_or(-1) - firstEnd) < 1.e-9,
          "resizing a frame updates its outgoing cutoff immediately");
    auto zero = base;
    check(zero.editTransition(id, Project::TransitionEdit::resizeEnd, first - firstEnd, false).wasOk()
        && zero.transitionPlaybackFor(zero.clips[0]).endCut == first && zero.transitionPlaybackFor(zero.clips[1]).audibleAt(first),
          "a zero-width transition makes an exact outgoing-to-incoming cut");
    auto detached = base; detached.detachTransitionLinks(id);
    check(detached.transitionLinks.empty() && !detached.transitionPlaybackFor(detached.clips[1]).startCut && !detached.transitionPlaybackFor(detached.clips[0]).endCut,
          "unlinked clips regain their original source/fade playback boundaries");
    {
        Engine direct, live; direct.sampleRate = live.sampleRate = sr; direct.project = live.project = isolate(base, 1);
        const double at = (std::floor(first * sr) - 64) / sr; live.position = at; live.setPlaying(true);
        juce::AudioBuffer<float> offline(2, 128); juce::MidiBuffer midi; direct.render(offline, 128, at, midi);
        float l[128] {}, r[128] {}; float* channels[] {l, r}; live.audioDeviceIOCallbackWithContext(nullptr, 0, channels, 2, 128, {});
        bool same = true; for (int frame = 0; frame < 128; ++frame) same &= offline.getSample(0, frame) == l[frame] && offline.getSample(1, frame) == r[frame];
        check(same && offline.getMagnitude(0, 128) > .001f, "live device callback and offline renderer agree exactly across the incoming cutoff");
        const auto session = root.getChildFile("cut.mixworks"); const auto saved = direct.saveProject(session);
        Engine reopened; reopened.sampleRate = sr; const auto opened = reopened.restoreProject(session.loadFileAsString());
        check(saved.isEmpty() && opened.isEmpty() && reopened.project.transitionPlaybackFor(reopened.project.clips[1]).startCut == first,
              "production Save/Open retains frame cutoffs using existing session fields: " + saved + opened);
        const auto file = root.getChildFile("cut.wav"); const auto exported = reopened.exportWav(file);
        auto reader = std::unique_ptr<juce::AudioFormatReader>(reopened.formats.createReaderFor(file));
        bool exact = exported.isEmpty() && reader; juce::AudioBuffer<float> audio(2, 128);
        for (const double edge : {first, last}) if (reader)
        {
            const auto offset = (juce::int64) std::floor(edge * sr) - 64; exact &= reader->read(&audio, 0, 128, offset, true, true);
            for (int frame = 0; frame < 128; ++frame)
            { const double time = (offset + frame) / sr; exact &= time >= first && time < last ? std::abs(audio.getSample(0, frame)) > .001f : audio.getSample(0, frame) == 0; }
        }
        check(exact, "production PCM24 WAV export is silent outside the same transition edges: " + exported);
    }
    {
        Engine engine; engine.sampleRate = sr; engine.project = isolate(base, 1);
        engine.project.clips[2].fadeInAnchor = second - engine.project.clips[2].start;
        const auto outgoingId = engine.project.clips[2].id;
        const auto stored = engine.project.storeTransitionAlternative(outgoingId, 0);
        const auto prepared = stored.wasOk() ? engine.prepareTransitionRehearsal(outgoingId, {}) : stored;
        const auto started = prepared.wasOk() ? engine.auditionTransitionAlternative(0) : prepared;
        bool exact = started.wasOk(), heard = false, cut = false;
        float l[128] {}, r[128] {}; float* channels[] {l, r};
        if (exact) for (int block = 0; block < 205; ++block)
        {
            const double at = engine.position.load(); engine.audioDeviceIOCallbackWithContext(nullptr, 0, channels, 2, 128, {});
            for (int frame = 0; frame < 128; ++frame)
            {
                const double time = at + frame / sr;
                if (time >= last + 1.e-7) { exact &= l[frame] == 0 && r[frame] == 0; cut = true; }
                else heard |= std::abs(l[frame]) > .001f;
            }
        }
        check(exact && heard && cut, "prepared A/B/C rehearsal obeys the outgoing cutoff during actual device playback: " + started.getErrorMessage());
        engine.endTransitionRehearsal();
    }
    Workstation ui(false); ui.stopTimer(); ui.engine.library = {media}; ui.engine.project = base;
    ui.selectedClip = id; ui.timeline.pixelsPerSecond = 100; ui.waveformMode.setSelectedId(2, juce::dontSendNotification); ui.setSize(1920, 1080); ui.refresh();
    const auto shaded = ui.timeline.createComponentSnapshot(ui.timeline.getLocalBounds());
    ui.engine.project.transitionLinks.clear(); const auto unshaded = ui.timeline.createComponentSnapshot(ui.timeline.getLocalBounds()); ui.engine.project = base;
    auto brightness = [&](const juce::Image& image, int side, double time)
    {
        const auto r = ui.timeline.clipBounds(base.clips[(size_t) side]);
        const auto region = juce::Rectangle<int>((int) (time * 100), (int) r.getBottom() - 15, 24, 7).getIntersection(image.getBounds());
        double sum = 0; for (int y = region.getY(); y < region.getBottom(); ++y) for (int x = region.getX(); x < region.getRight(); ++x) sum += image.getPixelAt(x, y).getBrightness();
        return sum;
    };
    check(brightness(shaded, 1, 1.25) < brightness(unshaded, 1, 1.25) * .85,
          "incoming waveform is visibly ghosted before its frame begins");
    check(brightness(shaded, 0, 4.25) < brightness(unshaded, 0, 4.25) * .85,
          "outgoing waveform is visibly ghosted after its frame ends");
    check(std::abs(brightness(shaded, 1, 4.25) - brightness(unshaded, 1, 4.25)) < .01,
          "audible waveform keeps its full-strength display between frame cutoffs");
    auto capture = [&](const juce::String& name, float scale = 1)
    {
        auto output = report.getSiblingFile(name).createOutputStream(); if (output) { output->setPosition(0); output->truncate(); }
        juce::PNGImageFormat png; return output && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds(), true, scale), *output);
    };
    check(capture("transition-cuts-1920.png"), "capture native incoming/outgoing ghost waveforms with unchanged frame and slip handles");
    ui.setSize(1250, 800); check(capture("transition-cuts-1250.png") && capture("transition-cuts-200.png", 2), "cut indication stays legible at narrow and 200-percent native sizes");
}
}
