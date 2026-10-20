#include "Workstation.h"
#include <cmath>
#include <limits>

namespace mixworks
{
void runTransitionFilterTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    const auto json = [](const Project& p) { return juce::JSON::toString(projectToJson(p), juce::JSON::FormatOptions().withMaxDecimalPlaces(17)); };
    Project base; base.followTrackTempo = false; base.bpm = base.tempo.fallback = 120; base.masterDb = 0;
    for (int n = 0; n < 3; ++n)
    {
        auto media = std::make_shared<Media>(); media->sampleRate = 8000; media->manualBpm = 120; media->manualBeatOrigin = 0;
        media->name = "Filter source " + juce::String(n); media->file = report.getSiblingFile("filter-source-" + juce::String(n) + ".wav");
        media->samples.setSize(2, 4 * 8000);
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < media->frames(); ++i)
            media->samples.setSample(ch, i, ch == 0 ? .1f * (float) (std::sin(i * 2 * juce::MathConstants<double>::pi * 40 / 8000)
                + std::sin(i * 2 * juce::MathConstants<double>::pi * 3000 / 8000)) : 0.f);
        std::unique_ptr<juce::OutputStream> stream = media->file.createOutputStream(); juce::WavAudioFormat wav;
        auto writer = stream ? wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(2).withBitsPerSample(24)) : nullptr;
        check(writer && writer->writeFromAudioSampleBuffer(media->samples, 0, media->frames()), "write owned filter workflow source"); writer.reset();
        Clip clip; clip.media = media; clip.length = 4; clip.start = n * 2; clip.track = n;
        clip.introAnchor = clip.fadeInAnchor = 0; clip.outroAnchor = 2; clip.fadeOutAnchor = 4; base.clips.push_back(clip);
    }
    base.transitionLinksKnown = true;
    base.transitionLinks = {{base.clips[0].id, base.clips[1].id}, {base.clips[1].id, base.clips[2].id}};
    const auto id = base.clips[1].id;
    auto project = base; const auto original = json(base);
    check(project.setTransitionFilter(id, {TransitionFilterMode::highToLow, 1}).wasOk(), "apply paired High to low sweep");
    const auto out = project.transitionPlaybackFor(project.clips[0]).filters[1], in = project.transitionPlaybackFor(project.clips[1]).filters[0];
    check(!out.highPass() && in.highPass() && out.amountAt(1) == 0 && out.amountAt(5) == 1 && in.amountAt(1) == 1 && in.amountAt(5) == 0,
        "paired filters use complementary directions and hold endpoint states outside the frame");
    check(out.amountAt(3) == .5 && in.amountAt(3) == .5, "paired sweep follows the shared frame midpoint");
    auto invalid = project; invalid.clips[0].fadeOutAnchor = 2;
    check(invalid.validateTransitionLinks().failed(), "active filters reject zero-length transition geometry");
    const auto retained = json(project);
    check(project.setTransitionFilter(id, {TransitionFilterMode::highToLow, std::numeric_limits<double>::quiet_NaN()}).failed()
        && project.setTransitionFilter(id, {(TransitionFilterMode) 99, 1}).failed() && json(project) == retained, "invalid filter input is rejected atomically");
    auto gesture = project; check(gesture.editTransition(id, Project::TransitionEdit::resizeEnd, -.25, false).wasOk(), "filter frame resizes through the existing owner");
    project.setTransitionFilter(id, {TransitionFilterMode::lowToHigh, .75});
    check(project.applyPlacements(gesture).wasOk() && project.transitionLinks[0].filter.mode == TransitionFilterMode::lowToHigh
        && project.transitionLinks[0].filter.strength == .75, "geometry gesture retains the latest filter instead of its mouse-down snapshot");
    check(project.setTransitionLocked(id, true).wasOk(), "filter transition locks");
    const auto locked = json(project);
    check(project.setTransitionFilter(id, {}).failed() && project.setTransitionFilter(base.clips[2].id, {TransitionFilterMode::highToLow, 1}).failed()
        && json(project) == locked, "both direct and neighbouring filter edits respect locked tracks");
    auto changed = project; changed.transitionLinks[1].filter = {TransitionFilterMode::lowToHigh, .5};
    check(project.validateTransitionLocks(changed).failed(), "indirect commits cannot add a filter to a locked neighbour");
    project.setTransitionLocked(id, false);
    TransitionStyle style; check(project.captureTransitionStyle(id, style).wasOk() && style.filter == project.transitionLinks[0].filter, "capture filter into a reusable treatment");
    check(project.storeTransitionAlternative(id, 0).wasOk() && project.putTransitionPreset({"Copper sweep", style}).wasOk(), "store filter in A and a named preset");
    auto parsed = parseProject(json(project));
    check(parsed.error.isEmpty() && parsed.project.transitionLinks == project.transitionLinks && parsed.project.transitionPresets[0].style == style, "session 20 round-trips filter, A/B/C and named presets");
    for (const auto& bad : {juce::var(true), juce::var("1"), juce::var(-.1), juce::var(1.1)})
    {
        auto data = projectToJson(project); data["transitionLinks"][0]["filter"].getDynamicObject()->setProperty("strength", bad);
        check(parseProject(juce::JSON::toString(data)).error.isNotEmpty(), "malformed persisted filter depth is rejected");
    }
    auto missing = projectToJson(project); missing["transitionLinks"][0].getDynamicObject()->removeProperty("filter");
    check(parseProject(juce::JSON::toString(missing)).error.isNotEmpty(), "session 20 requires its filter descriptor");
    auto legacy = projectToJson(base); legacy.getDynamicObject()->setProperty("version", 19); legacy["tracks"].getArray()->removeRange(4, trackCount - 4);
    for (auto& link : *legacy["transitionLinks"].getArray()) link.getDynamicObject()->removeProperty("filter");
    parsed = parseProject(juce::JSON::toString(legacy));
    check(parsed.error.isEmpty() && !parsed.project.transitionLinks[0].filter.active(), "session 19 opens with exact dry filter default");
    const auto portable = report.getSiblingFile("copper-sweep.alloypreset"); NamedTransitionStyle imported;
    check(writeTransitionPreset(portable, {"Copper sweep", style}).wasOk() && readTransitionPreset(portable, imported).wasOk() && imported.style == style, "portable version 2 carries the filter treatment");
    auto oldStyle = transitionStyleToJson(TransitionStyle {}); oldStyle.getDynamicObject()->removeProperty("filter");
    check(readTransitionStyle(oldStyle, style).wasOk() && !style.filter.active(), "legacy portable treatment defaults to dry filters");
    check(project.applyTransitionPreset(id, TransitionPreset::alignOnly).wasOk() && !project.transitionLinks[0].filter.active(), "whole-pair None intentionally clears its filter");

    const auto render = [](Engine& engine, int blockSize)
    {
        const int frames = (int) std::llround(engine.project.duration() * 8000); juce::AudioBuffer<float> result(2, frames), block(2, 512); juce::MidiBuffer midi;
        for (int offset = 0; offset < frames; offset += blockSize)
        { const int count = std::min(blockSize, frames - offset); engine.render(block, count, offset / 8000., midi);
          for (int ch = 0; ch < 2; ++ch) result.copyFrom(ch, offset, block, ch, 0, count); }
        return result;
    };
    Engine dryEngine; dryEngine.sampleRate = 8000; dryEngine.project = base; const auto dry = render(dryEngine, 256);
    Engine zero; zero.sampleRate = 8000; zero.project = base; zero.project.setTransitionFilter(id, {TransitionFilterMode::highToLow, 0});
    const auto zeroPcm = render(zero, 256); bool exact = true;
    for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < dry.getNumSamples(); ++i) exact &= dry.getSample(ch, i) == zeroPcm.getSample(ch, i);
    check(exact, "zero-depth filter retains every dry sample at the same callback partition");
    dryEngine.seek(0); zero.seek(0); const auto dryUneven = render(dryEngine, 137), zeroUneven = render(zero, 137);
    bool unevenExact = true; double dryPartitionDifference = 0;
    for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < dry.getNumSamples(); ++i)
    { unevenExact &= dryUneven.getSample(ch, i) == zeroUneven.getSample(ch, i);
      dryPartitionDifference = std::max(dryPartitionDifference, std::abs((double) dry.getSample(ch, i) - dryUneven.getSample(ch, i))); }
    check(unevenExact, "zero-depth and None remain sample-exact under the same uneven partition");
    check(dryPartitionDifference < 1e-6, "unfiltered source interpolation remains within tolerance across partitions: " + juce::String(dryPartitionDifference, 12));
    Engine wet; wet.sampleRate = 8000; wet.project = base;
    wet.project.setTransitionFilter(id, {TransitionFilterMode::highToLow, 1});
    wet.project.setTransitionFilter(base.clips[2].id, {TransitionFilterMode::lowToHigh, .75});
    const auto wetPcm = render(wet, 256); wet.seek(0); const auto uneven = render(wet, 137);
    bool finite = true, isolated = true; double maxDifference = 0, changedPcm = 0;
    for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < wetPcm.getNumSamples(); ++i)
    { const double a = wetPcm.getSample(ch, i); finite &= std::isfinite(a); maxDifference = std::max(maxDifference, std::abs(a - uneven.getSample(ch, i)));
      changedPcm += std::abs(a - dry.getSample(ch, i)); if (ch == 1) isolated &= a == 0; }
    check(finite && isolated && changedPcm > 10, "production renderer applies both neighbouring sweeps with finite stereo-isolated PCM");
    check(maxDifference < 1e-6, "filter processing is independent of callback partition after seek/reset");
    juce::String error; auto privateRenderer = wet.createExportRenderer(error);
    check(privateRenderer && error.isEmpty(), "private export renderer captures the filter graph");
    if (privateRenderer)
    {
        const auto offline = render(*privateRenderer, 511); double maximum = 0;
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < offline.getNumSamples(); ++i) maximum = std::max(maximum, std::abs((double) offline.getSample(ch, i) - wetPcm.getSample(ch, i)));
        check(maximum < 1e-6, "private export and playback share sample-equivalent filter DSP");
    }
    const auto exported = report.getSiblingFile("paired-filter-export.wav");
    check(wet.exportWav(exported).isEmpty(), "write actual paired-filter WAV through production export");
    auto reader = std::unique_ptr<juce::AudioFormatReader>(wet.formats.createReaderFor(exported));
    juce::AudioBuffer<float> encoded(2, wetPcm.getNumSamples()); bool encodedValid = reader && reader->lengthInSamples == wetPcm.getNumSamples()
        && reader->read(&encoded, 0, encoded.getNumSamples(), 0, true, true); double encodedDifference = 0;
    if (encodedValid) for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < encoded.getNumSamples(); ++i)
    { encodedValid &= std::isfinite(encoded.getSample(ch, i)); encodedDifference = std::max(encodedDifference, std::abs((double) encoded.getSample(ch, i) - wetPcm.getSample(ch, i))); }
    check(encodedValid && encodedDifference < 4. / 8388608, "every encoded filter WAV sample matches production playback within PCM24 tolerance");
    // Constant endpoint spectral probes, with a full second of state settling.
    for (double rate : {8000., 44100., 48000., 96000.}) for (bool highPass : {false, true})
    {
        const int frames = (int) rate * 2; juce::AudioBuffer<float> signal(2, frames); signal.clear();
        const double upperTone = rate == 8000 ? 3000 : 12000;
        for (int i = 0; i < frames; ++i) signal.setSample(0, i, (float) (.1 * (std::sin(i * 2 * juce::MathConstants<double>::pi * 40 / rate) + std::sin(i * 2 * juce::MathConstants<double>::pi * upperTone / rate))));
        TransitionFilterProcessor processor;
        processor.process(signal, 0, frames, rate, 2, {{{0, 1, {highPass ? TransitionFilterMode::lowToHigh : TransitionFilterMode::highToLow, 1}, false}, {}}});
        const auto amplitude = [&](double frequency)
        { double real = 0, imag = 0; for (int i = (int) rate; i < frames; ++i) { const double angle = i * 2 * juce::MathConstants<double>::pi * frequency / rate;
          real += signal.getSample(0, i) * std::cos(angle); imag += signal.getSample(0, i) * std::sin(angle); } return 2 * std::sqrt(real * real + imag * imag) / rate; };
        const double bass = amplitude(40), high = amplitude(upperTone);
        check(highPass ? high > .085 && bass < .002 : bass > .085 && high < .002, "measured " + juce::String(highPass ? "high-pass" : "low-pass") + " spectrum at " + juce::String(rate, 0) + " Hz");
    }
    wet.project.storeTransitionAlternative(id, 0); auto cancelled = std::make_shared<std::atomic<bool>>(false);
    const auto beforeRehearsal = json(wet.project);
    check(wet.prepareTransitionRehearsal(id, cancelled).wasOk() && wet.auditionTransitionAlternative(0).wasOk() && json(wet.project) == beforeRehearsal, "filter alternative renders and auditions without applying it");
    double rehearsalDifference = 0; std::array<float, 512> left {}, right {}; float* channels[] {left.data(), right.data()};
    for (int offset = 0; offset < 48000; offset += 512)
    {
        const int count = std::min(512, 48000 - offset); wet.audioDeviceIOCallbackWithContext(nullptr, 0, channels, 2, count, {});
        for (int i = 0; i < count; ++i) for (int ch = 0; ch < 2; ++ch)
            rehearsalDifference = std::max(rehearsalDifference, std::abs((double) channels[ch][i] - wetPcm.getSample(ch, offset + i)));
    }
    check(rehearsalDifference < 1e-6, "single-alternative rehearsal callbacks match the same complete filter window sample by sample");
    wet.endTransitionRehearsal();
    const auto session = report.getSiblingFile("filter-workflow.mixworks");
    check(wet.saveProject(session).isEmpty(), "save actual filter workflow session");
    Engine reopened; check(reopened.restoreProject(session.loadFileAsString()).isEmpty() && reopened.project.transitionLinks == wet.project.transitionLinks, "production reopen retains both filters and A/B/C settings");

    Workstation ui(false); ui.stopTimer(); ui.setSize(1440, 940); ui.engine.project = base; ui.selectTransition(id); ui.engine.position = 1;
    ui.transitionFilterMode.setSelectedId(2, juce::sendNotificationSync); ui.transitionFilterDepth.setSelectedId(75, juce::sendNotificationSync);
    check(ui.undoHistory.size() == 2 && ui.dirty && ui.engine.position == 1 && !ui.busy && ui.engine.project.transitionLinks[0].filter.strength == .75, "native filter controls commit one Undo per edit and retain transport");
    const auto edited = json(ui.engine.project); ui.undo(false); ui.undo(true);
    check(json(ui.engine.project) == edited, "filter Undo/Redo retains the complete graph");
    ui.transitionLock.onClick(); check(!ui.transitionFilterMode.isEnabled() && !ui.transitionFilterDepth.isEnabled(), "native lock disables filter editing");
    ui.transitionLock.onClick(); ui.setSize(1000, 620); ui.transitionViewport.setViewPosition(0, ui.transitionFilterLabel.getY());
    check(ui.transitionFilterMode.getWidth() > 100 && ui.transitionViewport.getViewArea().intersects(ui.transitionFilterMode.getBounds()), "narrow native inspector keeps filters reachable by scrolling");
    auto screenshot = report.getSiblingFile("paired-filter-inspector.png").createOutputStream(); juce::PNGImageFormat png;
    check(screenshot && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *screenshot), "capture native paired filter controls");
    check(json(base) == original, "all filter commands preserve the original fixture project");
}
}
