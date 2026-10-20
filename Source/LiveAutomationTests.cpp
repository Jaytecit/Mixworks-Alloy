#include "Engine.h"
#include <cmath>

namespace mixworks
{
void runLiveAutomationTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report, const juce::File& plugin)
{
    Engine controls; controls.project.masterDb = 0;
    const AutomationAddress volume {0, AutomationTarget::volume};
    auto& curve = controls.project.tracks[0].automation[0]; curve.enabled = true; curve.setPoint(0, 0); curve.setPoint(0.5, -12); curve.setPoint(1, 0);
    const auto original = curve;
    controls.mappings = {{0xb0, 8, 71, {MidiActionKind::volume, 0}}, {0xb0, 8, 28, {MidiActionKind::pan, 0}}, {0xb0, 8, 79, {MidiActionKind::master}}, {0xb0, 8, 20, {MidiActionKind::bass, 0}}};
    auto cc = [&](int number, int value) { controls.testMidi(juce::MidiMessage::controllerEvent(8, number, value)); controls.flushMidiControls(); };
    int takes = 0; Project undoSnapshot; controls.onAutomationWriteBegin = [&] { ++takes; undoSnapshot = controls.project; };
    controls.armAutomation(0, true); controls.setPlaying(true); controls.position = 0.2; cc(71, 100);
    check(!controls.recordingAutomation() && curve.points.size() == original.points.size(), "Read mode never records armed FLkey fader movements");
    curve = original; controls.project.tracks[0].gainDb = 0; controls.setAutomationRecordMode(AutomationRecordMode::touch);
    controls.position = 0.2; controls.testMidi(juce::MidiMessage::controllerEvent(8, 71, 100)); controls.position = 0.25; controls.flushMidiControls();
    check(controls.recordingAutomation() && std::any_of(curve.points.begin(), curve.points.end(), [](const auto& point) { return point.time == 0.2; }), "recording uses the MIDI arrival playhead time rather than delayed UI dispatch time");
    controls.position = 0.3; cc(71, 80); const float held = -60 + 80.0f / 127 * 66;
    check(std::abs(controls.automationValue(volume, 0.4) - held) < 1.0e-5 && takes == 1 && undoSnapshot.tracks[0].automation[0].points.size() == 3,
          "Touch holds the moved fader audibly and captures one undo snapshot before the pass");
    controls.position = 0.9; controls.advanceAutomation(0.9);
    check(!controls.recordingAutomation() && std::abs(curve.valueAt(0.9, 0) - original.valueAt(0.9, 0)) < 1.0e-5
          && std::abs(curve.valueAt(0.1, 0) - original.valueAt(0.1, 0)) < 1.0e-5,
          "Touch releases after 500 ms, returns over 50 ms and preserves the curve outside the punch");
    controls.setPlaying(false); controls.project = undoSnapshot;
    check(controls.project.tracks[0].automation[0].points.size() == 3 && controls.project.tracks[0].gainDb == 0, "the recording undo snapshot restores both curve and manual value");
    controls.setAutomationRecordMode(AutomationRecordMode::write); controls.setPlaying(true); controls.position = 0.2; cc(71, 64);
    controls.position = 0.9; controls.advanceAutomation(0.9); controls.armAutomation(1, true);
    check(controls.recordingAutomation() && std::abs(controls.automationValue(volume, 0.9) - (-60 + 64.0f / 127 * 66)) < 1.0e-5,
          "Write/latch holds after movement until stop and arming another track does not interrupt it");
    cc(28, 100); controls.armAutomation(-1, true); cc(79, 90); cc(20, 32);
    check(controls.project.tracks[0].automation[4].active() && controls.project.masterAutomation.active() && controls.project.tracks[0].automation[1].active(),
          "mapped knobs, master fader and optional EQ MIDI actions record their own curves");
    controls.stop();
    check(!controls.recordingAutomation() && controls.automationRecordMode == AutomationRecordMode::read && controls.position == 0,
          "Stop finishes automation before rewinding and returns to safe Read mode");
    controls.setAutomationRecordMode(AutomationRecordMode::touch); cc(71, 20);
    check(!controls.recordingAutomation(), "moving a fader while stopped does not record");

    Engine engine; auto media = readMedia(engine.formats, report.getSiblingFile("fixture.wav"));
    if (!media.media) { check(false, "load source fixture for VST automation"); return; }
    engine.library = {media.media}; Clip clip; clip.media = media.media; clip.length = 1; engine.project.clips = {clip}; engine.project.masterDb = 0;
    auto error = engine.addPlugin(0, plugin); if (error.isEmpty()) error = engine.addPlugin(0, plugin);
    check(error.isEmpty(), "real VST3 chain for parameter automation: " + error); if (error.isNotEmpty()) return;
    auto address = engine.pluginAddress(0, 1, 0);
    check(address.hasValue() && engine.ensurePluginAutomation(*address), "automatable parameter resolves to a stable plugin-instance and parameter identity"); if (!address) return;
    auto* parameterCurve = engine.project.curve(*address); parameterCurve->enabled = true; parameterCurve->setPoint(0, 0); parameterCurve->setPoint(1, 1);
    juce::AudioBuffer<float> output(2, 512); juce::MidiBuffer midi;
    auto trace = [&](int size)
    {
        engine.resetPlugins(); std::vector<float> values; values.reserve(44100);
        for (int offset = 0; offset < 44100; offset += size)
        {
            const int count = std::min(size, 44100 - offset); engine.render(output, count, offset / 44100.0, midi);
            for (int i = 0; i < count; ++i) values.push_back(output.getSample(0, i));
        }
        return values;
    };
    const auto live = trace(173), regular = trace(512); double maximum = 0, rampError = 0; int worst = 0; bool expected = true;
    for (int i = 0; i < 44100; ++i)
    {
        const double value = 0.125 * (i - i % 32) / 44100.0;
        expected = expected && std::abs(live[(size_t) i] - value) < 2.0e-6;
        if (std::abs(live[(size_t) i] - value) > rampError) { rampError = std::abs(live[(size_t) i] - value); worst = i; }
        maximum = std::max(maximum, (double) std::abs(live[(size_t) i] - regular[(size_t) i]));
    }
    check(expected, "real VST3 parameter automation produces the expected gain ramp in actual PCM; error " + juce::String(rampError, 8) + " at " + juce::String(worst) + "; mid=" + juce::String(live[22050], 8));
    check(maximum < 2.0e-6, "VST parameter timing is independent of callback size; PCM error " + juce::String(maximum, 9));
    auto exported = report.getSiblingFile("vst-automation-render.wav"); check(engine.exportWav(exported).isEmpty(), "VST automation exports through the real hosted chain");
    std::unique_ptr<juce::AudioFormatReader> reader(engine.formats.createReaderFor(exported)); bool same = reader && reader->lengthInSamples == 44100;
    if (reader) { juce::AudioBuffer<float> wave(2, 44100); reader->read(&wave, 0, 44100, 0, true, true); for (int i = 0; i < 44100; ++i) same = same && std::abs(wave.getSample(0, i) - live[(size_t) i]) < 2.0e-6; }
    check(same, "live VST automation and exported WAV agree across different block sizes");
    const auto saved = juce::JSON::toString(engine.captureProject()); const auto slotId = address->slotId;
    check(engine.restoreProject(saved).isEmpty() && engine.plugins[0][1]->id == slotId && engine.project.curve(*address)->active(), "VST curve and stable instance identity survive save/reopen");
    auto invalid = juce::JSON::parse(saved); auto& lane = invalid["tracks"].getArray()->getReference(0)["pluginAutomation"].getArray()->getReference(0);
    lane.getDynamicObject()->setProperty("parameterId", "nonexistent-parameter");
    check(engine.restoreProject(juce::JSON::toString(invalid)).isNotEmpty() && engine.plugins[0][1]->id == slotId, "unavailable VST parameter cannot replace the current session or target another parameter");
    invalid = juce::JSON::parse(saved); invalid["tracks"].getArray()->getReference(0)["pluginAutomation"].getArray()->getReference(0)["curve"]["points"].getArray()->getReference(0).getDynamicObject()->setProperty("value", 1.01);
    check(parseProject(juce::JSON::toString(invalid)).error.isNotEmpty(), "VST automation rejects values outside the normalized range");
    engine.removePlugin(0, 0); engine.render(output, 512, 0.5, midi);
    check(engine.plugins[0][0]->id == slotId && engine.project.curve(*address) && std::abs(output.getSample(0, 0) - 0.25 * (22050 - 22050 % 32) / 44100) < 2.0e-6,
          "removing a preceding insert does not redirect VST automation; PCM=" + juce::String(output.getSample(0, 0), 8));
    engine.assignPluginControl(0, 0, 0, 0); engine.mappings = {{0xb0, 8, 21, {MidiActionKind::pluginParameter, 0}}};
    engine.project.curve(*address)->enabled = false; engine.plugins[0][0]->processor->getParameters()[0]->setValueNotifyingHost(0.33f);
    Project vstUndo; engine.onAutomationWriteBegin = [&] { vstUndo = engine.project; };
    engine.armAutomation(0, true); engine.setAutomationRecordMode(AutomationRecordMode::write); engine.setPlaying(true); engine.position = 0.2;
    engine.testMidi(juce::MidiMessage::controllerEvent(8, 21, 100)); engine.selectedTrack = 1; engine.flushMidiControls(); engine.selectedTrack = 0;
    engine.position = 0.4; engine.advanceAutomation(0.4); engine.render(output, 512, 0.4, midi);
    check(engine.recordingAutomation() && std::abs(output.getSample(0, 0) - 0.25 * 100.0 / 127) < 2.0e-6, "mapped FLkey knob records a named VST parameter and changes actual hosted audio; PCM=" + juce::String(output.getSample(0, 0), 8));
    engine.seek(0.1); check(!engine.recordingAutomation(), "seeking closes an active recording region before changing the playhead");
    const auto recorded = juce::JSON::toString(engine.captureProject());
    check(engine.restoreProject(recorded).isEmpty() && engine.project.curve(*address)->active(), "recorded punch boundaries survive decimal JSON serialization and real session reopening");
    engine.project = vstUndo; engine.restoreManualPluginValues(); engine.render(output, 512, 0.4, midi);
    check(std::abs(output.getSample(0, 0) - 0.25 * 0.33) < 2.0e-6 && !engine.project.curve(*address)->active(), "undo restores an unautomated VST's real manual parameter value and its previous curve");
    engine.stop();
    auto& pan = engine.project.tracks[0].automation[4]; pan.enabled = true; pan.setPoint(0, -1); pan.setPoint(1, 1);
    auto& master = engine.project.masterAutomation; master.enabled = true; master.setPoint(0, -6.0206f);
    engine.project.curve(*address)->enabled = false; engine.plugins[0][0]->processor->getParameters()[0]->setValue(1);
    engine.render(output, 512, 0, midi);
    check(std::abs(output.getSample(0, 0) - 0.125) < 2.0e-6 && output.getSample(1, 0) == 0, "pan and master automation affect rendered stereo PCM");
    auto masterSaved = juce::JSON::toString(engine.captureProject());
    const auto reload = engine.restoreProject(masterSaved);
    check(reload.isEmpty() && engine.project.masterAutomation.active() && engine.project.tracks[0].automation[4].active() && !engine.automationArmed[0]
          && engine.automationRecordMode == AutomationRecordMode::read, "sessions preserve pan/master but reopen disarmed in Read mode: " + reload);
    engine.removePlugin(0, 0); check(engine.project.tracks[0].pluginAutomation.empty(), "removing the automated plugin removes its owned lanes");
    auto v4 = juce::JSON::parse(masterSaved); v4.getDynamicObject()->setProperty("version", 4); v4["tracks"].getArray()->removeRange(4, trackCount - 4);
    for (auto& t : *v4["tracks"].getArray()) for (int n = 0; n < 4; ++n)
        for (auto& point : *t["automation"][automationId((AutomationTarget) n)]["points"].getArray())
        { point.getDynamicObject()->setProperty("db", point["value"]); point.getDynamicObject()->removeProperty("value"); }
    check(engine.restoreProject(juce::JSON::toString(v4)).isEmpty() && !engine.project.masterAutomation.active(), "version 4 dB curves migrate to typed automation values");
    // Native UI fixture uses the real test VST and controller profile loaded by normal startup.
    engine.project.clips[0].length = 16; engine.project.clips[0].loopSpan = 1;
    engine.project.tracks[0].automation = {}; engine.project.masterAutomation = {};
    auto demo = engine.pluginAddress(0, 0, 0);
    if (demo && engine.ensurePluginAutomation(*demo)) { auto* c = engine.project.curve(*demo); c->enabled = true; c->setPoint(0, 0.2f); c->setPoint(8, 0.8f); }
    engine.saveProject(report.getSiblingFile("live-automation-demo.mixworks"));
}
}
