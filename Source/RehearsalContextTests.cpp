#include "Workstation.h"
#include <juce_cryptography/juce_cryptography.h>
#include <cmath>
#include <limits>

namespace mixworks
{
void runRehearsalContextTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    const auto json = [](const Project& p) { return juce::JSON::toString(projectToJson(p), juce::JSON::FormatOptions().withMaxDecimalPlaces(17)); };
    Project base; base.followTrackTempo = false; base.bpm = base.tempo.fallback = 120;
    for (int n = 0; n < 2; ++n)
    {
        auto media = std::make_shared<Media>(); media->sampleRate = 8000; media->manualBpm = 120; media->manualBeatOrigin = 0;
        media->name = "Context source " + juce::String(n + 1); media->file = report.getSiblingFile("context-source-" + juce::String(n) + ".wav");
        media->samples.setSize(2, 8 * 8000);
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < media->frames(); ++i) media->samples.setSample(ch, i, .1f * (float) std::sin(i * (.03 + n * .01) + ch * .3));
        std::unique_ptr<juce::OutputStream> stream = media->file.createOutputStream(); juce::WavAudioFormat wav;
        auto writer = stream ? wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(2).withBitsPerSample(24)) : nullptr;
        check(writer && writer->writeFromAudioSampleBuffer(media->samples, 0, media->frames()), "write owned rehearsal-context source"); writer.reset();
        Clip clip; clip.media = media; clip.start = n * 4; clip.length = 8; clip.track = n;
        clip.introAnchor = clip.fadeInAnchor = 0; clip.outroAnchor = 4; clip.fadeOutAnchor = 8; base.clips.push_back(clip);
    }
    const auto id = base.clips[1].id; base.transitionLinksKnown = true; base.transitionLinks = {{base.clips[0].id, id}};
    check(base.storeTransitionAlternative(id, 0).wasOk(), "store context comparison alternative");
    check(base.rehearsalContext == RehearsalContext {1, 1}, "default rehearsal retains one bar before and after");
    auto project = base; const auto original = json(project);
    check(project.setRehearsalContext({-1, 1}).failed() && json(project) == original, "negative context rejects atomically");
    check(project.setRehearsalContext({2, 0}).wasOk(), "asymmetric context is valid");
    auto parsed = parseProject(json(project));
    check(parsed.error.isEmpty() && parsed.project.rehearsalContext == RehearsalContext {2, 0}, "session 21 persists asymmetric rehearsal context");
    for (const auto& bad : {juce::var(true), juce::var("2"), juce::var(-1), juce::var(.5), juce::var((juce::int64) std::numeric_limits<int>::max() + 1)})
    {
        auto data = projectToJson(project); data["rehearsalContext"].getDynamicObject()->setProperty("beforeBars", bad);
        check(parseProject(juce::JSON::toString(data)).error.isNotEmpty(), "invalid context count rejects before integer conversion");
    }
    auto missing = projectToJson(project); missing.getDynamicObject()->removeProperty("rehearsalContext");
    check(parseProject(juce::JSON::toString(missing)).error.isNotEmpty(), "session 21 requires its context object");
    missing.getDynamicObject()->setProperty("version", 20); missing["tracks"].getArray()->removeRange(4, trackCount - 4); parsed = parseProject(juce::JSON::toString(missing));
    check(parsed.error.isEmpty() && parsed.project.rehearsalContext == RehearsalContext {1, 1}, "session 20 retains legacy one-bar context without manufacturing a new choice");
    RehearsalContext retained {4, 8}; auto malformed = rehearsalContextToJson({1, 1}); malformed.getDynamicObject()->setProperty("afterBars", std::numeric_limits<double>::infinity());
    check(readRehearsalContext(malformed, retained).failed() && retained == RehearsalContext {4, 8}, "nonfinite context preserves the caller's prior settings");
    project.setTransitionLocked(id, true); auto monitor = project; monitor.setRehearsalContext({0, 4});
    check(project.validateTransitionLocks(monitor).wasOk() && monitor.transitionFor(id)->locked, "locked pairs permit monitor-only context changes");
    TransitionStyle first, second; project.captureTransitionStyle(id, first); monitor.captureTransitionStyle(id, second);
    check(first == second && !transitionStyleToJson(first).hasProperty("rehearsalContext"), "portable musical treatments stay independent of monitor context");

    auto cancelled = std::make_shared<std::atomic<bool>>(false);
    const std::array<RehearsalContext, 3> contexts {{{0, 0}, {2, 0}, {0, 2}}};
    const std::array<double, 3> starts {4, 0, 4}, lengths {4, 8, 8};
    for (size_t test = 0; test < contexts.size(); ++test)
    {
        Engine engine; engine.project = base; engine.project.setRehearsalContext(contexts[test]); engine.sampleRate = 8000; engine.position = 3.125;
        const auto saved = json(engine.project);
        check(engine.prepareTransitionRehearsal(id, cancelled).wasOk() && engine.auditionTransitionAlternative(0).wasOk()
            && engine.position == starts[test], "context starts at its exact available window " + juce::String((int) test));
        const int frames = (int) (lengths[test] * 8000); std::array<float, 512> left {}, right {}; float* channels[] {left.data(), right.data()};
        double energy = 0; bool finite = true;
        for (int offset = 0; offset < frames; offset += 511)
        {
            const int count = std::min(511, frames - offset); engine.audioDeviceIOCallbackWithContext(nullptr, 0, channels, 2, count, {});
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < count; ++i) { finite &= std::isfinite(channels[ch][i]); energy += channels[ch][i] * channels[ch][i]; }
        }
        check(finite && energy > 1 && engine.position == starts[test] && json(engine.project) == saved, "actual callbacks loop the known context length without applying it " + juce::String((int) test));
        engine.endTransitionRehearsal(); check(engine.position == 3.125 && !engine.playing, "context audition returns to its original cursor paused");
    }
    Engine ramp; ramp.project = base; ramp.sampleRate = 8000; ramp.project.followTrackTempo = true;
    ramp.project.tempo.points = {{0, 80, true}, {12, 160, true}}; ramp.project.setRehearsalContext({1, 0});
    check(ramp.prepareTransitionRehearsal(id, cancelled).wasOk() && ramp.auditionTransitionAlternative(0).wasOk()
        && std::abs(ramp.project.tempo.beatsAt(4) - ramp.project.tempo.beatsAt(ramp.position) - 4) < 1e-7, "context bars integrate through a changing mix tempo"); ramp.endTransitionRehearsal();
    cancelled->store(true); const auto rampSaved = json(ramp.project);
    check(ramp.prepareTransitionRehearsal(id, cancelled).failed() && !ramp.rehearsalActive() && json(ramp.project) == rampSaved, "cancelled context preparation publishes no cache or session edit"); cancelled->store(false);
    Engine budget; budget.project = base; budget.sampleRate = 8000; budget.project.clips[1].length = 100000;
    budget.project.setRehearsalContext({0, std::numeric_limits<int>::max()}); const auto budgetError = budget.prepareTransitionRehearsal(id, cancelled);
    check(budgetError.failed() && budgetError.getErrorMessage().contains("256") && !budget.rehearsalActive(), "actual expanded context exceeds the existing audio budget before allocation");
    Engine silent; silent.project = base; silent.sampleRate = 8000; silent.project.setRehearsalContext({0, 0}); for (auto& track : silent.project.tracks) track.mute = true;
    check(silent.prepareTransitionRehearsal(id, cancelled).wasOk() && silent.rehearsalSummary().contains("silent"), "short silent comparison remains explicitly labelled");

    Engine output; output.project = base; output.sampleRate = 8000;
    const auto before = report.getSiblingFile("context-export-before.wav"), after = report.getSiblingFile("context-export-after.wav");
    const auto beforeError = output.exportWav(before); output.project.setRehearsalContext({4, 8}); const auto afterError = output.exportWav(after);
    check(beforeError.isEmpty() && afterError.isEmpty() && juce::SHA256(before) == juce::SHA256(after), "ordinary exported WAV remains byte-identical across rehearsal context choices");
    const auto file = report.getSiblingFile("rehearsal-context.mixworks"); check(output.saveProject(file).isEmpty(), "save actual context session");
    Engine reopened; check(reopened.restoreProject(file.loadFileAsString()).isEmpty() && reopened.project.rehearsalContext == RehearsalContext {4, 8}, "production reopen retains context independently of source preparation");

    Workstation ui(false); ui.stopTimer(); ui.setSize(1440, 940); ui.engine.project = base; ui.selectTransition(id); ui.engine.position = 6.25; ui.engine.playing = true;
    ui.engine.setAutomationRecordMode(AutomationRecordMode::touch); const auto link = *ui.engine.project.transitionFor(id);
    ui.rehearsalLeadIn.setSelectedId(3, juce::sendNotificationSync); ui.rehearsalLeadOut.setSelectedId(1, juce::sendNotificationSync);
    check(ui.engine.project.rehearsalContext == RehearsalContext {2, 0} && ui.undoHistory.size() == 2 && ui.dirty
        && ui.engine.position == 6.25 && ui.engine.playing && ui.engine.automationRecordMode == AutomationRecordMode::touch
        && *ui.engine.project.transitionFor(id) == link, "native context controls keep live transport, automation mode and musical treatment with one Undo per change");
    ui.engine.playing = false; ui.engine.setAutomationRecordMode(AutomationRecordMode::read); const auto edited = json(ui.engine.project); ui.undo(false); ui.undo(true);
    check(json(ui.engine.project) == edited, "context Undo/Redo restores its exact session data");
    ui.transitionLock.onClick(); check(ui.rehearsalLeadIn.isEnabled() && ui.rehearsalLeadOut.isEnabled(), "native locked pair keeps monitor context controls available");
    ui.rehearsalLeadIn.setSelectedId(5, juce::sendNotificationSync); check(ui.engine.project.rehearsalContext.beforeBars == 4 && ui.engine.project.transitionFor(id)->locked, "monitor context edit retains its musical lock");
    const auto protectedProject = json(ui.engine.project); const auto history = ui.undoHistory.size(); ui.busy = true; ui.rehearsalLeadOut.setSelectedId(9, juce::sendNotificationSync);
    check(json(ui.engine.project) == protectedProject && ui.undoHistory.size() == history, "busy preparation cannot change context or history"); ui.busy = false; ui.refresh();
    ui.engine.position = 3.125; check(ui.engine.prepareTransitionRehearsal(id, cancelled).wasOk() && ui.engine.auditionTransitionAlternative(0).wasOk(), "prepare locked context through the production renderer");
    ui.rehearsalLeadIn.setSelectedId(2, juce::sendNotificationSync);
    check(!ui.engine.rehearsalActive() && !ui.engine.playing && ui.engine.position == 3.125 && ui.engine.auditionTransitionAlternative(0).failed(), "changing context invalidates cached audition and returns to its saved cursor");
    ui.setSize(1000, 620); ui.transitionAdvanced.onClick();
    const auto contextBounds = ui.transitionBody.getLocalArea(&ui.rehearsalLeadOut, ui.rehearsalLeadOut.getLocalBounds());
    ui.transitionViewport.setViewPosition(0, contextBounds.getY());
    check(ui.rehearsalLeadIn.getWidth() > 80 && ui.rehearsalLeadOut.getWidth() > 80 && ui.transitionAdvancedBody.isVisible()
        && ui.transitionViewport.getViewArea().intersects(contextBounds), "expanded narrow inspector retains both context choices");
    auto stream = report.getSiblingFile("rehearsal-context-inspector.png").createOutputStream(); juce::PNGImageFormat png;
    check(stream && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *stream), "capture native rehearsal context choices");
}
}
