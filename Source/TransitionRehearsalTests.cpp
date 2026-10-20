#include "Workstation.h"
#include <cmath>
#include <limits>

namespace mixworks
{
void runTransitionRehearsalTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    const auto json = [](const Project& p) { return juce::JSON::toString(projectToJson(p), juce::JSON::FormatOptions().withMaxDecimalPlaces(17)); };
    Project base; base.followTrackTempo = false; base.bpm = 120; base.masterDb = -3;
    for (int n = 0; n < 3; ++n)
    {
        auto media = std::make_shared<Media>(); media->sampleRate = 8000; media->manualBpm = 120; media->manualBeatOrigin = 0;
        media->name = "Rehearsal source " + juce::String(n); media->file = report.getSiblingFile("rehearsal-source-" + juce::String(n) + ".wav");
        media->samples.setSize(2, 12 * 8000);
        for (int ch = 0; ch < 2; ++ch) for (int frame = 0; frame < media->frames(); ++frame)
            media->samples.setSample(ch, frame, .12f * (float) std::sin(frame * (n + 1) * .05 + ch * .3));
        Clip clip; clip.media = media; clip.length = 12; clip.start = n * 8; clip.track = n;
        clip.introAnchor = clip.fadeInAnchor = 0; clip.outroAnchor = 8; clip.fadeOutAnchor = 12;
        base.clips.push_back(clip);
    }
    base.transitionLinksKnown = true; const auto id = base.clips[1].id;
    base.transitionLinks = {{base.clips[0].id, id}, {id, base.clips[2].id}};
    base.clips[0].controls.automation[2].enabled = true; base.clips[0].controls.automation[2].setPoint(9, -3);
    Project project = base; TransitionStyle plain, bass, composed;
    check(project.captureTransitionStyle(id, plain).wasOk() && plain.volume == std::array<double, 4> {0, 0, 1, 1}, "capture existing frame without copying manual automation");
    check(project.storeTransitionAlternative(id, 0).wasOk(), "store A from the actual transition owner");
    check(project.applyTransitionPreset(id, TransitionPreset::bassSwap).wasOk() && project.captureTransitionStyle(id, bass).wasOk() && bass.bassPositions.has_value(), "capture legacy paired bass and its exact endpoint values");
    check(project.storeTransitionAlternative(id, 1).wasOk(), "store legacy bass treatment in B");
    check(project.setTransitionBand(id, 0, {TransitionBandMode::crossfade, .8, 0, .5}).wasOk()
        && project.setTransitionBand(id, 1, {TransitionBandMode::swapFade, .7, 1, .5}).wasOk()
        && project.captureTransitionStyle(id, composed).wasOk() && project.storeTransitionAlternative(id, 2).wasOk(), "store C with independent Volume and Bass settings");
    check(project.putTransitionPreset({"Copper", composed}).wasOk() && project.putTransitionPreset({"Copper", bass}).wasOk() && project.transitionPresets.size() == 1, "named preset replacement is explicit and idempotent");
    check(project.putTransitionPreset({"bad\nname", plain}).failed(), "preset names reject line breaks");
    const auto retained = project.transitionLinks[1]; const auto manual = project.clips[0].controls.automation[2].points;
    check(project.applyTransitionStyle(id, bass).wasOk() && project.transitionLinks[0].bassSwap.has_value(), "apply B restores legacy paired bass ownership");
    TransitionStyle roundtrip; project.captureTransitionStyle(id, roundtrip);
    check(roundtrip == bass && project.transitionLinks[1] == retained && project.clips[0].controls.automation[2].points[0].value == manual[0].value,
        "applied treatment round-trips while retaining neighbouring pair and manual mid curve");
    check(project.clips[0].start == base.clips[0].start && project.clips[1].start == base.clips[1].start && project.clips[1].effects == base.clips[1].effects
        && project.clips[0].tempoMap == base.clips[0].tempoMap && project.transitionRange(project.transitionLinks[0]) == base.transitionRange(base.transitionLinks[0]), "style application retains song positions, frame, effects and audio clocks");
    auto conflict = base; conflict.clips[0].controls.automation[1].enabled = true; conflict.clips[0].controls.automation[1].setPoint(10, -2);
    const auto unchanged = json(conflict);
    check(conflict.applyTransitionStyle(id, bass).failed() && json(conflict) == unchanged, "manual bass overlap rejects atomically without deleting user points");
    auto locked = project; locked.setTransitionLocked(base.clips[2].id, true); const auto lockedJson = json(locked);
    check(locked.applyTransitionStyle(id, composed).failed() && json(locked) == lockedJson, "a locked neighbouring track blocks alternative commit");
    check(locked.storeTransitionAlternative(id, 0).wasOk(), "locked transitions allow capturing read-only alternatives");
    const auto parsed = parseProject(json(project));
    check(parsed.error.isEmpty() && parsed.project.transitionLinks == project.transitionLinks && parsed.project.transitionPresets.size() == 1
        && parsed.project.transitionPresets[0].style == bass, "session 17 retains all three alternatives and named presets");
    auto stale = project; auto moved = project;
    moved.setTransitionLength(id, 1); stale.storeTransitionAlternative(id, 0);
    check(stale.applyPlacements(moved).wasOk() && stale.transitionLinks[0].alternatives[0] == project.transitionLinks[0].alternatives[1], "geometry gesture retains alternatives captured after its initial snapshot");
    auto old = projectToJson(base); old.getDynamicObject()->setProperty("version", 16); old["tracks"].getArray()->removeRange(4, trackCount - 4); old.getDynamicObject()->removeProperty("transitionPresets");
    check(parseProject(juce::JSON::toString(old)).error.isEmpty(), "session 16 remains readable without named presets");
    for (const auto bad : {juce::var(true), juce::var("bad"), juce::var(juce::Array<juce::var> {})})
    {
        auto data = projectToJson(project); data["transitionLinks"][0].getDynamicObject()->setProperty("alternatives", bad);
        check(parseProject(juce::JSON::toString(data)).error.isNotEmpty(), "malformed alternatives cannot become an empty accepted slot");
    }
    auto malformed = transitionStyleToJson(bass); malformed["bassPositions"].getArray()->set(1, 0.0);
    TransitionStyle kept = composed;
    check(readTransitionStyle(malformed, kept).failed() && kept == composed, "malformed portable style leaves caller's previous treatment intact");
    auto broken = composed; broken.bands[0]->strength = std::numeric_limits<double>::quiet_NaN();
    const auto before = json(project); check(project.applyTransitionStyle(id, broken).failed() && json(project) == before, "non-finite treatment fails before mutation");
    const auto portable = report.getSiblingFile("Copper.alloypreset");
    check(writeTransitionPreset(portable, {"Copper", bass}).wasOk(), "export portable named preset through atomic writer");
    NamedTransitionStyle imported;
    check(readTransitionPreset(portable, imported).wasOk() && imported.style == bass && imported.name == "Copper", "portable preset reopens with exact treatment");
    check(writeTransitionPreset(portable, {"Copper", composed}).wasOk() && portable.getSiblingFile("Copper.alloypreset.bak").existsAsFile(), "replacing a portable preset backs up its previous bytes");
    const auto backup = portable.getSiblingFile("Copper.alloypreset.bak").loadFileAsString();
    check(juce::JSON::parse(backup)["presets"][0]["style"]["bassPositions"].isArray(), "preset backup retains the previous legacy bass treatment");
    portable.replaceWithText("{\"format\":\"alloy-transition-preset\",\"version\":3}");
    check(readTransitionPreset(portable, imported).failed(), "newer portable preset versions are rejected without mutation");

    Engine engine; engine.project = project; engine.sampleRate = 8000; engine.position = 3;
    const auto original = json(engine.project); auto cancel = std::make_shared<std::atomic<bool>>(true);
    check(engine.prepareTransitionRehearsal(id, cancel).failed() && !engine.rehearsalActive() && !engine.exporting && json(engine.project) == original, "cancelled render publishes no preview and retains session");
    cancel->store(false);
    engine.masterInputPeak = .123f; engine.masterPeak = .234f; engine.masterClipped = false;
    for (auto& meter : engine.meters) meter = .345f;
    check(engine.prepareTransitionRehearsal(id, cancel).wasOk() && json(engine.project) == original && engine.position == 3, "production renderer prepares A/B/C without modifying session or cursor");
    check(engine.masterInputPeak == .123f && engine.masterPeak == .234f && !engine.masterClipped
          && std::all_of(engine.meters.begin(), engine.meters.end(), [](const auto& meter) { return meter.load() == .345f; }),
          "worker-rendered alternatives restore all live meters and clipping history before audition");
    std::array<double, 3> rms {};
    for (int slot = 0; slot < 3; ++slot)
    {
        check(engine.auditionTransitionAlternative(slot).wasOk(), "audition stored alternative " + juce::String(slot));
        std::array<float, 512> left {}, right {}; float* channels[] {left.data(), right.data()}; double energy = 0;
        // Rehearsal window is 6..14 seconds: exactly 125 callback blocks at 8 kHz.
        for (int block = 0; block < 125; ++block)
        {
            engine.audioDeviceIOCallbackWithContext(nullptr, 0, channels, 2, 512, {});
            for (int sample = 0; sample < 512; ++sample) energy += (double) left[(size_t) sample] * left[(size_t) sample] + (double) right[(size_t) sample] * right[(size_t) sample];
        }
        rms[(size_t) slot] = std::sqrt(energy / (2 * 64000));
        check(std::abs(engine.position.load() - 6) < 1e-8 && json(engine.project) == original, "rehearsal loops exactly across callbacks without applying its alternative");
        check(!engine.canWriteAutomation(-1), "rehearsal cannot record automation over the mix");
    }
    check(rms[0] > .01 && std::abs(rms[0] - rms[1]) < 1e-7 && std::abs(rms[0] - rms[2]) < 1e-7, "actual monitor callbacks deliver equally matched non-silent RMS for A/B/C");
    {
        engine.auditionTransitionAlternative(0); engine.monitorMuted = true;
        float left[512], right[512]; std::fill_n(left, 512, 1.0f); std::fill_n(right, 512, 1.0f); float* channels[] {left, right};
        const auto beforeMute = engine.position.load(); engine.audioDeviceIOCallbackWithContext(nullptr, 0, channels, 2, 512, {});
        check(juce::FloatVectorOperations::findMinAndMax(left, 512) == juce::Range<float>() && juce::FloatVectorOperations::findMinAndMax(right, 512) == juce::Range<float>() && engine.position > beforeMute,
              "monitor mute silences actual rehearsal callbacks while the audition clock continues");
        engine.monitorMuted = false; engine.audioDeviceIOCallbackWithContext(nullptr, 0, channels, 2, 512, {});
        check(juce::FloatVectorOperations::findMinAndMax(left, 512).getLength() > .001f, "unmuting rehearsal resumes its prepared audio");
    }
    engine.endTransitionRehearsal(); check(!engine.rehearsalActive() && !engine.playing && engine.position == 3 && json(engine.project) == original, "end rehearsal returns to original cursor paused with exact project");
    check(engine.auditionTransitionAlternative(0).failed(), "ending rehearsal invalidates its cached audio");
    engine.project.setTransitionLocked(id, true);
    check(engine.prepareTransitionRehearsal(id, cancel).wasOk() && engine.project.transitionLinks[0].locked, "read-only rehearsal can compare a locked pair without unlocking the session");
    engine.auditionTransitionAlternative(1); engine.seek(2);
    check(!engine.rehearsalActive() && engine.position == 2, "ordinary seeking exits rehearsal and uses requested timeline position");
    engine.sampleRate = 2.0e9;
    check(engine.prepareTransitionRehearsal(id, cancel).failed() && !engine.exporting, "rehearsal rejects expanded audio before allocation exceeds its memory budget");
    engine.sampleRate = 8000; for (auto& track : engine.project.tracks) track.mute = true;
    check(engine.prepareTransitionRehearsal(id, cancel).wasOk() && engine.rehearsalSummary().contains("silent"), "silent alternatives remain explicit without infinite matching gain");
    engine.auditionTransitionAlternative(0);
    std::array<float, 512> silenceLeft {}, silenceRight {}; float* silenceChannels[] {silenceLeft.data(), silenceRight.data()};
    engine.audioDeviceIOCallbackWithContext(nullptr, 0, silenceChannels, 2, 512, {});
    check(std::all_of(silenceLeft.begin(), silenceLeft.end(), [](float x) { return x == 0; }), "silent rehearsal renders finite zero output");
    engine.stop(); check(!engine.rehearsalActive() && engine.position == 0, "ordinary Stop cancels rehearsal and rewinds the actual transport");

    const auto arguments = juce::StringArray::fromTokens(juce::JUCEApplicationBase::getCommandLineParameters(), true); const int pluginFlag = arguments.indexOf("--plugin");
    if (pluginFlag >= 0 && pluginFlag + 1 < arguments.size())
    {
        Engine effects; effects.project = project; effects.sampleRate = 8000;
        for (auto& clip : effects.project.clips) clip.effects = makeClipEffects();
        effects.selectedClip = effects.project.clips[0].id;
        const auto error = effects.addPlugin(0, juce::File(arguments[pluginFlag + 1].unquoted()));
        check(error.isEmpty(), "load real VST3 into rehearsal's participating clip: " + error);
        if (error.isEmpty())
        {
            auto& slot = *effects.pluginChain(0).front();
            const auto gain = std::find_if(slot.parameters.begin(), slot.parameters.end(), [](const auto& x) { return x.second->getName(128) == "Gain"; });
            check(gain != slot.parameters.end(), "rehearsal plugin fixture binds Gain by its named contract rather than parameter ordering");
            if (gain == slot.parameters.end()) return;
            const auto& entry = *gain; entry.second->setValueNotifyingHost(.77f);
            const AutomationAddress address {0, AutomationTarget::plugin, slot.id, entry.first, effects.selectedClip}; effects.ensurePluginAutomation(address);
            auto* curve = effects.project.curve(address); curve->enabled = true; curve->setPoint(0, .3f); curve->setPoint(12, .6f);
            juce::MemoryBlock beforeState, afterState; slot.processor->getStateInformation(beforeState);
            const auto result = effects.prepareTransitionRehearsal(id, cancel); slot.processor->getStateInformation(afterState);
            check(result.wasOk() && beforeState == afterState && std::abs(entry.second->getValue() - .77f) < 1e-6, "real automated VST3 state survives three independent rehearsal renders exactly: " + result.getErrorMessage()
                + " state equal=" + juce::String(beforeState == afterState ? 1 : 0) + " value=" + juce::String(entry.second->getValue(), 8));
            effects.auditionTransitionAlternative(0); effects.setControlValue({-1, AutomationTarget::master}, -4);
            check(!effects.rehearsalActive() && effects.project.masterDb == -4 && !effects.playing, "actual mixer control changes cancel stale rehearsal before editing the session");
        }
    }

    Workstation ui(false); ui.stopTimer(); ui.setSize(1440, 940); ui.engine.project = project; ui.selectTransition(id); ui.engine.position = 3;
    ui.storeAlternative[0].onClick();
    const auto stored = json(ui.engine.project); const auto undoCount = ui.undoHistory.size();
    ui.applyAlternative[2].onClick();
    check(ui.undoHistory.size() == undoCount + 1 && ui.engine.project.transitionLinks[0].bands == composed.bands && ui.engine.position == 3, "native Apply C commits one Undo without seeking or preparing audio");
    ui.undo(false); check(json(ui.engine.project) == stored, "native Undo restores the previously applied treatment and retained alternatives");
    ui.undo(true); check(ui.engine.project.transitionLinks[0].bands == composed.bands, "native Redo restores the chosen alternative");
    ui.transitionLock.onClick(); check(!ui.applyAlternative[0].isEnabled() && ui.auditionAlternative[0].isEnabled() && ui.storeAlternative[0].isEnabled(), "native locked pair permits capture and audition while disabling Apply");
    ui.transitionLock.onClick();
    ui.transitionViewport.setViewPosition(0, 170);
    auto stream = report.getSiblingFile("transition-rehearsal.png").createOutputStream(); juce::PNGImageFormat png;
    check(stream && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *stream), "capture native rehearsal and saved preset controls");
    ui.setSize(1000, 620); ui.transitionViewport.setViewPosition(0, ui.applyAlternative[0].getY());
    check(ui.transitionViewport.getViewArea().intersects(ui.applyAlternative[0].getBounds()) && ui.applyAlternative[0].getWidth() > 30, "small windows retain accessible A/B/C controls through inspector scrolling");
    const auto workerBaseline = json(ui.engine.project); const auto workerUndo = ui.undoHistory.size();
    ui.prepareRehearsal.onClick();
    check(ui.busy && ui.importProgress.isVisible() && ui.importProgress.cancel.isEnabled(), "actual Prepare rehearsal handler enters cancellable background preparation");
    struct Poll final : juce::Timer { std::function<void()> tick; void timerCallback() override { tick(); } } poll;
    const double began = juce::Time::getMillisecondCounterHiRes(); int phase = 0;
    poll.tick = [&]
    {
        if (juce::Time::getMillisecondCounterHiRes() - began > 20000)
        {
            check(false, "native rehearsal worker completes within its bounded test timeout");
            if (ui.importCancellation) ui.importCancellation->store(true);
            poll.stopTimer(); juce::MessageManager::getInstance()->stopDispatchLoop(); return;
        }
        if (ui.busy) return;
        if (phase == 0)
        {
            check(ui.engine.rehearsalActive() && !ui.importProgress.isVisible() && json(ui.engine.project) == workerBaseline && ui.undoHistory.size() == workerUndo,
                "actual worker publishes and starts rehearsal without an arrangement edit or Undo entry");
            ui.auditionAlternative[1].onClick();
            check(ui.engine.transitionRehearsalSlot() == 1 && !ui.busy, "native Hear B switches cached rehearsal without another render job");
            ui.endRehearsal.onClick(); ui.prepareRehearsal.onClick(); ui.importProgress.cancel.onClick(); phase = 1;
        }
        else
        {
            check(!ui.engine.rehearsalActive() && !ui.importProgress.isVisible() && json(ui.engine.project) == workerBaseline && ui.undoHistory.size() == workerUndo,
                "actual Cancel rehearsal handler completes worker cleanup and retains the mix and Undo history");
            poll.stopTimer(); juce::MessageManager::getInstance()->stopDispatchLoop();
        }
    };
    poll.startTimer(20); juce::MessageManager::getInstance()->runDispatchLoop();
}
}
