#include "Engine.h"
#include "Beatmatch.h"
#include <cmath>

namespace mixworks
{
void runTempoTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    auto near = [](double a, double b, double tolerance = 1.0e-7) { return std::abs(a - b) < tolerance; };
    TempoMap map; map.points = {{0, 120}, {8, 150}, {16, 90}, {20, 90}, {24, 130, false}};
    check(near(map.bpmAt(4), 135) && near(map.bpmAt(12), 120) && near(map.bpmAt(23), 90) && near(map.bpmAt(24), 130), "tempo holds, rising/falling ramps and explicit cuts");
    bool inverse = true; for (double t = 0; t < 60; t += 0.017) inverse = inverse && near(map.timeAtBeat(map.beatsAt(t)), t);
    check(inverse && near(map.beatsAt(8), 18), "tempo integration and stable inverse agree across ramps/cuts and held endpoints");
    bool snaps = true; for (double t = 0.1; t < 32; t += 0.103) { const double snapped = map.snap(t, true); snaps = snaps && near(map.beatsAt(snapped), std::round(map.beatsAt(t))) && near(map.snap(t, false), t); }
    check(snaps, "snap uses the displayed variable-tempo beat positions; Alt retains exact time");
    TempoMap close; close.points = {{0, 120}, {0.000002, 400}, {1, 20}};
    check(near(close.timeAtBeat(close.beatsAt(0.25)), 0.25) && near(close.timeAtBeat(close.beatsAt(0.000001)), 0.000001), "tempo inverse remains finite for adjacent markers and extreme legal BPMs");
    TempoMap narrow; narrow.points = {{0, 120}, {0.0000001, 125}, {0.0000002, 130}}; narrow.movePoint(1, 0.00000015, 127);
    check(narrow.points[1].time > narrow.points[0].time && narrow.points[1].time < narrow.points[2].time && narrow.points[1].bpm == 127, "editing tightly spaced imported tempo points preserves strict ordering");
    const double rate = 44100;
    auto source = std::make_shared<Media>(); source->name = "120 BPM tempo fixture"; source->sampleRate = rate; source->manualBpm = 120; source->manualBeatOrigin = 0;
    source->samples.setSize(2, (int) (24 * rate));
    for (int i = 0; i < source->frames(); ++i)
    {
        const double t = i / rate, phase = std::fmod(t, 0.5);
        const float sample = (float) (0.1 * std::sin(2 * juce::MathConstants<double>::pi * 440 * t) + (phase < 0.004 ? 0.65 * std::exp(-phase * 1400) : 0));
        for (int ch = 0; ch < 2; ++ch) source->samples.setSample(ch, i, sample);
    }
    source->file = report.getSiblingFile("tempo-source.wav");
    { juce::WavAudioFormat format; std::unique_ptr<juce::OutputStream> stream = source->file.createOutputStream(); auto writer = format.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(rate).withNumChannels(2).withBitsPerSample(24)); if (writer) writer->writeFromAudioSampleBuffer(source->samples, 0, source->frames()); }
    Project project; project.masterDb = 0; project.tempo = map;
    Clip clip; clip.media = source; clip.length = source->duration(); clip.beatmatched = true; project.clips = {clip};
    auto prepared = prepareTempoMap(project, false, 0);
    check(prepared.error.isEmpty() && prepared.project.clips[0].prepared && prepared.project.clips[0].prepared->clipLocal, "production tempo-ramp preparation creates clip-local pitch-preserving PCM: " + prepared.error);
    if (prepared.error.isNotEmpty()) return;
    const auto& c = prepared.project.clips[0];
    check(near(c.length * c.rate, 24) && near(map.beatsAt(c.end()), 48) && near(c.sourceTimeAt(map.timeAtBeat(12)), 6), "tempo reflow preserves source content and maps waveform/clip end to the same beat clock");
    double worst = 0;
    const auto* pcm = c.prepared->channelData(0);
    for (int beat = 2; beat < 40; ++beat)
    {
        const double expected = map.timeAtBeat(beat); const int centre = (int) std::round(expected * rate), radius = (int) (0.08 * rate);
        float largest = 0; int found = centre;
        for (int i = std::max(1, centre - radius); i < std::min(c.prepared->frames(), centre + radius); ++i)
        { const float delta = std::abs(pcm[i] - pcm[i - 1]); if (delta > largest) { largest = delta; found = i; } }
        worst = std::max(worst, std::abs(found / rate - expected));
    }
    check(worst < 0.05, "actual stretched waveform pulses follow tempo-grid positions within 50 ms; worst " + juce::String(worst * 1000, 2) + " ms");
    int crossings = 0; const int begin = (int) (2.1 * rate), end = (int) (5.9 * rate);
    for (int i = begin; i < end; ++i) if (pcm[i] <= 0 && pcm[i + 1] > 0) ++crossings;
    check(std::abs(crossings * rate / (end - begin) - 440) < 2, "tempo ramp retains the source 440 Hz pitch");
    Engine engine; engine.project = prepared.project; engine.library = {source}; juce::MidiBuffer midi;
    juce::AudioBuffer<float> live(2, 512), split(2, 512); const double splitAt = 6.003;
    engine.render(live, 512, splitAt - 128 / rate, midi);
    auto left = c.slice(0, splitAt), right = c.slice(splitAt, c.length - splitAt); left.fadeOut = right.fadeIn = 0;
    engine.project.clips = {left, right}; engine.render(split, 512, splitAt - 128 / rate, midi);
    bool identical = true; for (int i = 0; i < 512; ++i) identical = identical && near(live.getSample(0, i), split.getSample(0, i), 0.00001);
    check(identical && near(left.length * left.rate + right.length * right.rate, 24), "splitting a ramp preserves actual PCM, source span and prepared-audio offsets");
    engine.project = prepared.project;
    engine.position = 12; const auto info = engine.getPosition();
    check(info && near(*info->getBpm(), 120) && near(*info->getPpqPosition(), map.beatsAt(12)), "VST playhead reports instantaneous BPM and integrated musical position");
    auto wav = report.getSiblingFile("tempo-render.wav"); check(engine.exportWav(wav).isEmpty(), "tempo-ramp PCM exports through the production mixer");
    std::unique_ptr<juce::AudioFormatReader> reader(engine.formats.createReaderFor(wav)); bool equal = reader && std::abs(reader->lengthInSamples - std::ceil(c.length * rate)) <= 1;
    for (double at : {3.1, 7.99, 12.1, 16.1})
    {
        const auto frame = (int64_t) std::llround(at * rate); engine.render(live, 512, frame / rate, midi);
        if (reader) { reader->read(&split, 0, 512, frame, true, true); for (int i = 0; i < 512; ++i) equal = equal && near(live.getSample(0, i), split.getSample(0, i), 0.00001); }
    }
    check(equal, "seeked live playback and exported WAV match before, within and after tempo ramps");
    auto saved = juce::JSON::toString(engine.captureProject()); report.getSiblingFile("tempo-demo.mixworks").replaceWithText(saved);
    check(engine.restoreProject(saved).isEmpty() && engine.project.tempo.points.size() == map.points.size() && near(engine.project.clips[0].length, c.length), "tempo map, source geometry and prepared PCM survive session reopening");
    for (const auto& field : {juce::String("bpm"), juce::String("time")})
    {
        auto bad = juce::JSON::parse(saved); bad["tempo"].getArray()->getReference(1).getDynamicObject()->setProperty(field, field == "time" ? 0 : 401);
        check(engine.restoreProject(juce::JSON::toString(bad)).isNotEmpty() && engine.project.tempo.points.size() == map.points.size(), "malformed tempo " + field + " preserves the current session");
    }
    auto old = juce::JSON::parse(saved); old.getDynamicObject()->setProperty("version", 5); old["tracks"].getArray()->removeRange(4, trackCount - 4); old.getDynamicObject()->removeProperty("tempo"); old.getDynamicObject()->removeProperty("followTrackTempo");
    auto legacy = parseProject(juce::JSON::toString(old)); check(legacy.error.isEmpty() && !legacy.project.followTrackTempo && legacy.project.tempo.points.empty(), "version 5 sessions retain fixed-tempo behavior until Beatmatch or a tempo edit");
    check(c.prepared->cache && c.prepared->cache->transient && c.prepared->residentBytes() == 0, "preparations without a persistent source identity release RAM into an owned temporary mapping");
    check(prepareTempoMap(project, false, 0, maxPreparedBytes).error.isNotEmpty(), "an uncached tempo expansion is rejected before allocation when actual resident PCM exhausts the budget");
    auto loopProject = project; auto& loop = loopProject.clips[0]; loop.offset = 0.25; loop.loopSpan = 1; loop.loopPhase = 0.13; loop.reversed = true;
    auto loopResult = prepareTempoMap(loopProject, false, 0);
    check(loopResult.error.isEmpty() && near(loopResult.project.clips[0].sourceTimeAt(map.timeAtBeat(2)), 1.12), "reversed looping sections retain source phase under tempo ramps");
    if (loopResult.error.isEmpty())
    {
        engine.project = loopResult.project; engine.render(live, 512, 6 - 128 / rate, midi);
        const auto whole = engine.project.clips[0]; left = whole.slice(0, 6); right = whole.slice(6, whole.length - 6); left.fadeOut = right.fadeIn = 0;
        engine.project.clips = {left, right}; engine.render(split, 512, 6 - 128 / rate, midi); bool same = true;
        for (int i = 0; i < 512; ++i) same = same && near(live.getSample(0, i), split.getSample(0, i), 0.00001);
        check(same && live.getMagnitude(0, 512) > 0.01, "reverse/loop tempo-ramp splits preserve audible production PCM");
    }
    Project arrangement; arrangement.masterDb = 0;
    auto incoming = std::make_shared<Media>(); incoming->sampleRate = rate; incoming->manualBpm = 144; incoming->manualBeatOrigin = 0.213; incoming->samples = source->samples;
    clip.length = 24; clip.rate = 1; clip.beatmatched = false; clip.prepared.reset(); clip.tempoMap.reset(); arrangement.clips = {clip};
    clip.media = incoming; clip.track = 1; clip.id = juce::Uuid().toString(); arrangement.clips.push_back(clip);
    auto automatic = prepareTempoMap(arrangement, true, 16);
    check(automatic.error.isEmpty() && near(automatic.project.tempo.bpmAt(0), 120) && near(automatic.project.tempo.points.back().bpm, 144), "automatic mix starts at the outgoing source BPM and reaches the incoming source BPM");
    if (automatic.error.isEmpty())
    {
        bool aligned = true;
        for (auto item : automatic.project.clips) { item.moveTo(automatic.project.snappedClipStart(item, item.start, true)); const double beat = automatic.project.tempo.beatsAt(item.start + item.firstBeatTime()); aligned = aligned && near(beat, std::round(beat)) && near(item.length * item.rate, 24); }
        check(aligned, "beat-snapped moves align source phases, waveform markers and global grid across the transition");
        const auto& second = automatic.project.clips[1];
        check(near(second.start + second.transitionTime(true), automatic.project.tempo.points.back().time) && near(automatic.project.clips[0].fadeOut, second.fadeIn), "tempo reaches its incoming target at musical IN without adding volume fades");
        const double placed = automatic.project.snappedClipStart(second, second.start + 0.27, true);
        Clip moved = second; moved.start = placed; const double beat = automatic.project.tempo.beatsAt(placed + moved.firstBeatTime());
        check(near(beat, std::round(beat)), "drag snapping aligns a clip's measured first pulse, including a nonzero source beat phase");
        auto edited = automatic.project; edited.tempo.setPoint(2, 123);
        auto custom = prepareTempoMap(edited, false, 0);
        Clip third = arrangement.clips[0]; third.id = juce::Uuid().toString(); third.track = 2; custom.project.clips.push_back(third);
        auto appended = prepareTempoMap(custom.project, true, 16, 0, 2);
        check(appended.error.isEmpty() && appended.project.tempo.points[1].time == 2 && appended.project.tempo.points[1].bpm == 123,
              "appending a track retains earlier user-edited tempo markers");
        if (appended.error.isEmpty())
        {
            const auto before = custom.project.clips[0].prepared, after = appended.project.clips[0].prepared;
            check(before && after && (before->geometry == after->geometry ? before == after : before != after)
                  && near(appended.project.clips[0].length * appended.project.clips[0].rate, custom.project.clips[0].length * custom.project.clips[0].rate),
                  "appending reuses unchanged PCM and rebuilds affected tempo geometry while preserving source content");
        }
    }
    auto cut = prepareTempoMap(arrangement, true, 0);
    check(cut.error.isEmpty() && !cut.project.tempo.points.back().ramp && near(cut.project.clips[0].end(), cut.project.clips[1].start, 0.5), "Cut switches source BPM without introducing a tempo ramp");
    Project unsynced; clip.media = incoming; clip.start = 1; clip.length = 10; clip.rate = 1; clip.beatmatched = false; clip.tempoMap.reset(); unsynced.clips = {clip};
    check(near(unsynced.snappedTime(1.62, true), 1.5), "ruler, labels and automation snap to the displayed background grid even over an unsynced clip");

    auto moved = c; moved.fadeIn = 1; moved.fadeOut = 2;
    const double fadeInSource = moved.sourceElapsed(moved.fadeIn), fadeOutSource = moved.sourceElapsed(moved.length) - moved.sourceElapsed(moved.length - moved.fadeOut);
    moved.moveTo(project.snappedClipStart(moved, 4.37, true));
    check(moved.elasticPlayback && moved.elasticSource && !moved.prepared && near(moved.length * moved.rate, 24)
          && near(map.beatsAt(moved.start + moved.firstBeatTime()), std::round(map.beatsAt(moved.start + moved.firstBeatTime())))
          && near(moved.sourceElapsed(moved.fadeIn), fadeInSource) && near(moved.sourceElapsed(moved.length) - moved.sourceElapsed(moved.length - moved.fadeOut), fadeOutSource),
          "moving through a ramp preserves source beats/fades and aligns immediately without rendering a replacement track");
    moved.fadeIn = moved.fadeOut = 0; engine.project = prepared.project; engine.project.clips = {moved};
    auto movedWav = report.getSiblingFile("moved-tempo-render.wav");
    const auto renderBegin = juce::Time::getMillisecondCounterHiRes();
    check(engine.exportWav(movedWav).isEmpty(), "moved elastic clip exports through the same production playback path");
    const double renderMs = juce::Time::getMillisecondCounterHiRes() - renderBegin;
    check(renderMs < moved.length * 1000 / 2,
          "elastic render of " + juce::String(moved.length, 2) + " s took " + juce::String(renderMs, 1) + " ms (faster than twice real time on this machine)");
    std::unique_ptr<juce::AudioFormatReader> movedReader(engine.formats.createReaderFor(movedWav));
    juce::AudioBuffer<float> rendered(2, (int) std::ceil(engine.project.duration() * rate));
    if (movedReader) movedReader->read(&rendered, 0, rendered.getNumSamples(), 0, true, true);
    double moveWorst = 0;
    for (int beat = 2; beat < 40; ++beat)
    {
        const double expected = moved.start + moved.localTimeForSource(beat * 0.5);
        const int centre = (int) std::llround(expected * rate), radius = (int) (0.06 * rate);
        float largest = 0; int found = centre;
        for (int i = centre - radius; i < std::min(rendered.getNumSamples(), centre + radius); ++i)
        {
            const float delta = std::abs(rendered.getSample(0, i) - rendered.getSample(0, i - 1));
            if (delta > largest) { largest = delta; found = i; }
        }
        moveWorst = std::max(moveWorst, std::abs(found / rate - expected));
    }
    check(movedReader && moveWorst < 0.025, "moved clip attacks follow the shared ramp grid within 25 ms; worst " + juce::String(moveWorst * 1000, 2) + " ms");
    crossings = 0; const int pitchBegin = (int) ((moved.start + 2.1) * rate), pitchEnd = (int) ((moved.start + 5.9) * rate);
    for (int i = pitchBegin; i < pitchEnd; ++i) if (rendered.getSample(0, i) <= 0 && rendered.getSample(0, i + 1) > 0) ++crossings;
    check(std::abs(crossings * rate / (pitchEnd - pitchBegin) - 440) < 2, "moving across a ramp preserves 440 Hz pitch in playback");
    // Continuous playback and export share DSP history. A seek restarts with
    // bounded preroll, so its waveform phase need not equal a render from zero.
    Engine continuous; continuous.project = engine.project;
    bool movedEqual = true;
    for (int frame = 0; frame < rendered.getNumSamples(); frame += 512)
    {
        const int count = std::min(512, rendered.getNumSamples() - frame); continuous.render(live, count, frame / rate, midi);
        for (int i = 0; i < count; ++i) movedEqual &= near(live.getSample(0, i), rendered.getSample(0, frame + i), 0.00001);
    }
    check(movedEqual, "continuous elastic playback is sample-identical to the exported mix across cache boundaries");
    double seamRms = 1;
    for (double at = std::ceil(moved.start + 1); at < moved.end() - 1; at += ElasticRenderer::windowSeconds)
    {
        if (std::abs(std::remainder(moved.sourceElapsed(at - moved.start), 0.5)) < 0.08) continue;
        seamRms = std::min(seamRms, (double) rendered.getRMSLevel(0, (int) (at * rate), (int) (0.008 * rate)));
    }
    check(seamRms > 0.06, "continuous stretch history avoids tone cancellation at cache boundaries; minimum RMS " + juce::String(seamRms, 5));
    bool repeatable = true; double seekWorst = 0;
    for (int beat : {27, 8, 39, 17, 3})
    {
        const double expected = moved.start + moved.localTimeForSource(beat * 0.5);
        const double at = std::floor((expected - 0.06) * rate) / rate;
        engine.seek(at); engine.render(live, 512, at, midi); engine.seek(at); engine.render(split, 512, at, midi);
        for (int i = 0; i < 512; ++i) repeatable &= near(live.getSample(0, i), split.getSample(0, i), 0.00001);
        engine.seek(at); float previousSample = 0, largest = 0; double found = at;
        for (int frame = 0; frame < (int) (0.12 * rate); frame += 512)
        {
            engine.render(live, 512, at + frame / rate, midi);
            for (int i = 0; i < 512; ++i)
            {
                const float value = live.getSample(0, i), delta = std::abs(value - previousSample); previousSample = value;
                if (frame + i > 0 && delta > largest) { largest = delta; found = at + (frame + i) / rate; }
            }
        }
        seekWorst = std::max(seekWorst, std::abs(found - expected));
    }
    check(repeatable && seekWorst < 0.025, "seeks restart reproducibly with attacks within 25 ms of the grid; worst " + juce::String(seekWorst * 1000, 2) + " ms");
    const double splitTime = moved.start + 6.003;
    engine.render(live, 512, splitTime - 128 / rate, midi);
    left = moved.slice(0, 6.003); right = moved.slice(6.003, moved.length - 6.003); right.id = juce::Uuid().toString();
    engine.project.clips = {left, right}; engine.render(split, 512, splitTime - 128 / rate, midi);
    identical = true; for (int i = 0; i < 512; ++i) identical &= near(live.getSample(0, i), split.getSample(0, i), 0.00001);
    check(identical, "splitting an elastic clip retains the original mapping and exact playback samples");
    engine.project.clips = {moved}; saved = juce::JSON::toString(engine.captureProject(), false, 17);
    engine.seek(12.501); engine.render(live, 512, 12.501, midi);
    const auto reopenError = engine.restoreProject(saved); engine.render(split, 512, 12.501, midi);
    identical = true; for (int i = 0; i < 512; ++i) identical &= near(live.getSample(0, i), split.getSample(0, i), 0.00001);
    check(reopenError.isEmpty() && identical && engine.project.clips[0].elasticPlayback, "saving/reopening a moved clip retains elastic timing and audio: " + reopenError);
    auto invalidElastic = juce::JSON::parse(saved); invalidElastic["clips"].getArray()->getReference(0).getDynamicObject()->setProperty("elasticPlayback", "true");
    check(parseProject(juce::JSON::toString(invalidElastic)).error.isNotEmpty(), "malformed elastic playback settings are rejected at the session boundary");
    auto phaseClip = automatic.project.clips.back(); const auto atZero = automatic.project.snappedClipStart(phaseClip, 0, true); phaseClip.moveTo(atZero);
    check(atZero >= 0 && near(std::remainder(automatic.project.tempo.beatsAt(atZero + phaseClip.firstBeatTime()), 1), 0), "snapping against the timeline's left edge preserves the first pulse instead of clamping it off-grid");
    auto flat = c; flat.moveTo(30); const auto flatLength = flat.length; flat.moveTo(32.123456);
    check(near(flat.length, flatLength) && near(flat.start, 32.123456), "free moves preserve exact placement and source duration in a held-tempo region");
    auto flatProject = project; flatProject.bpm = 130; flatProject.tempo.points = {{0, 130}};
    auto flatPrepared = prepareTempoMap(flatProject, false, 0); const auto cachedPcm = flatPrepared.project.clips[0].prepared;
    auto flatClip = flatPrepared.project.clips[0]; flatClip.moveTo(12.345);
    check(flatPrepared.error.isEmpty() && cachedPcm && flatClip.prepared == cachedPcm && !flatClip.elasticPlayback,
          "moving an already matched track at a held tempo reuses the exact pitch-preserved PCM");
    auto freeNeighbour = flatClip; freeNeighbour.id = juce::Uuid().toString(); freeNeighbour.moveTo(20.137); flatPrepared.project.clips = {freeNeighbour};
    const double targetPulse = freeNeighbour.start + freeNeighbour.firstBeatTime();
    const double magnet = flatPrepared.project.snappedClipStart(flatClip, targetPulse - flatClip.firstBeatTime() + 0.005, true, 0.02);
    check(near(magnet + flatClip.firstBeatTime(), targetPulse), "a nearby deliberately free-positioned clip supplies an exact beat magnet");
    auto reverseMoved = loopResult.project.clips[0]; reverseMoved.moveTo(3.173); reverseMoved.fadeIn = reverseMoved.fadeOut = 0;
    engine.project = loopResult.project; engine.project.clips = {reverseMoved}; engine.render(live, 512, 6.003, midi);
    left = reverseMoved.slice(0, 6.006 - reverseMoved.start); right = reverseMoved.slice(6.006 - reverseMoved.start, reverseMoved.end() - 6.006);
    engine.project.clips = {left, right}; engine.render(split, 512, 6.003, midi);
    identical = true; for (int i = 0; i < 512; ++i) identical &= near(live.getSample(0, i), split.getSample(0, i), 0.00001);
    check(identical && live.getMagnitude(0, 512) > 0.01, "moving and splitting reversed loops preserves their audible elastic mapping");
    ElasticRenderer renderer; double worstBlockMs = 0; juce::AudioBuffer<float> performance(2, 512);
    for (int frame = 0; frame < (int) (moved.length * rate); frame += 512)
    {
        performance.clear(); renderer.beginBlock(); const auto began = juce::Time::getMillisecondCounterHiRes();
        renderer.add(performance, 512, moved.start + frame / rate, rate, moved);
        worstBlockMs = std::max(worstBlockMs, juce::Time::getMillisecondCounterHiRes() - began);
    }
    check(renderer.windowsRendered() < (uint64_t) std::ceil(moved.length / ElasticRenderer::windowSeconds) + 4,
          "elastic cache renders only bounded playback windows; worst measured 512-frame block " + juce::String(worstBlockMs, 3) + " ms vs 11.610 ms callback duration");
    ElasticRenderer overlapping; std::vector<Clip> voices;
    for (int i = 0; i < 12; ++i) { auto voice = moved; voice.moveTo(moved.start + (i + 1) * 0.001); voices.push_back(voice); }
    for (int frame = 0; frame < (int) (0.3 * rate); frame += 512)
    {
        overlapping.beginBlock(); performance.clear();
        for (const auto& voice : voices) overlapping.add(performance, 512, 10 + frame / rate, rate, voice);
    }
    check(overlapping.windowsRendered() <= 12 * 5, "overlapping elastic clips keep independent continuous streams without a fixed voice-limit eviction loop");
}
}
