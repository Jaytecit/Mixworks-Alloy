#include "Workstation.h"

namespace mixworks
{
void runMidiConnectionTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    using State = MidiConnectionStatus::State;
    MidiConnectionStatus connection;
    check(connection.state() == State::absent && connection.caption() == "No MIDI controller", "missing inputs remain explicit without claiming a connection");
    connection.ports = {{"primary", "FLkey MIDI", false, false, false}, {"secondary", "MIDIIN2 (FLkey MIDI)", false, false, false}};
    check(connection.state() == State::detected && connection.description().contains("input disabled"), "enumerating a controller alone does not establish an open input");
    connection.ports[0].open = true;
    check(connection.state() == State::detected && connection.description().contains("Alloy is not listening"), "an opened driver without Alloy's callback cannot claim a working connection");
    connection.listening = true;
    check(connection.state() == State::connected && connection.description().contains("no messages received yet"), "open input and registered callback are distinguished from message reception");
    connection.ports[1].received = connection.ports[1].recent = true;
    check(connection.state() == State::connected, "activity on a disabled input cannot confirm the active controller");
    connection.ports[0].received = connection.ports[0].recent = true;
    check(connection.state() == State::receiving && connection.caption().contains("receiving"), "physical-input activity gives a separate receiving confirmation");
    connection.ports[0].recent = false;
    check(connection.state() == State::connected && connection.description().contains("received earlier"), "idle after real activity retains the connection without claiming continuing MIDI traffic");
    connection.ports[1].open = true;
    check(connection.caption().startsWith("MIDIIN2 (FLkey MIDI) / receiving"), "the receiving label identifies the port that actually received a message");
    connection.ports.clear();
    check(connection.state() == State::absent, "unplugging a controller removes an earlier reception confirmation");

    const auto root = report.getParentDirectory().getChildFile("midi-status-" + juce::Uuid().toString()); root.createDirectory();
    const auto profile = root.getChildFile("profile.json");
    const juce::String original = "[{\"status\":176,\"channel\":8,\"number\":21,\"action\":\"desk-pot-1\"}]";
    profile.replaceWithText(original);
    Engine engine(profile); const auto observed = engine.midiConnectionStatus();
    check(observed.mappings == 1 && observed.profile == profile && !observed.listening && profile.loadFileAsString() == original,
        "production connection snapshot reports the actual loaded profile and count without opening inputs or changing assignments");
    check(engine.mappingSummary().contains(profile.getFullPathName()) && engine.mappingSummary().contains("Active mappings: 1"),
        "MIDI settings identify the actual active mapping file rather than inferring it from installation state");

    const juce::String restored = "[{\"status\":176,\"channel\":8,\"number\":75,\"action\":\"pan-1\"}]";
    profile.replaceWithText(restored);
    for (const auto& invalid : juce::StringArray {"not JSON", "{}", "[{\"status\":176,\"channel\":17,\"number\":75,\"action\":\"pan-1\"}]",
        "[{\"status\":176,\"channel\":8,\"number\":75,\"action\":\"future-action\"}]",
        "[{\"status\":176,\"channel\":8,\"number\":75,\"action\":\"pan-1\"},{\"status\":176,\"channel\":8,\"number\":75,\"action\":\"mid-1\"}]"})
    {
        profile.replaceWithText(invalid);
        check(engine.reloadMidiMappings().failed() && engine.mappings.size() == 1 && engine.mappings[0].action.kind == MidiActionKind::deskPot
            && engine.midiConnectionStatus().profileError.isNotEmpty() && profile.loadFileAsString() == invalid,
            "malformed, unsupported or conflicting profiles retain active routing and report failure without file writes");
    }
    profile.deleteFile();
    check(engine.reloadMidiMappings().failed() && engine.mappings.size() == 1, "an unavailable saved profile retains active assignments");
    profile.replaceWithText(restored); engine.exporting = true;
    check(engine.reloadMidiMappings().failed() && engine.mappings.size() == 1, "an exclusive export rejects mapping reload without changing routing");
    engine.exporting = false; engine.beginLearn("mid-1");
    check(engine.reloadMidiMappings().wasOk() && !engine.learnAction.hasValue() && engine.midiConnectionStatus().profileError.isEmpty(),
        "successful reload cancels stale Learn and clears earlier diagnostics");
    engine.mappings = {{0xb0, 8, 75, {MidiActionKind::mid, 0}}};
    engine.testMidi(juce::MidiMessage::controllerEvent(8, 75, 0)); // Queued against the old layout.
    check(engine.reloadMidiMappings().wasOk(), "reload replaces stale active assignments from the selected saved profile");
    engine.flushMidiControls();
    check(engine.project.tracks[0].eqDb[1] == 0, "reloading discards controls queued under the superseded layout");
    engine.testMidi(juce::MidiMessage::controllerEvent(8, 75, 127)); engine.flushMidiControls();
    check(engine.project.tracks[0].pan > 0.99f && engine.project.tracks[0].eqDb[1] == 0 && profile.loadFileAsString() == restored,
        "a fader formerly assigned to Mid operates Pan after reload without writing the saved profile");
    profile.replaceWithText("[{\"status\":176,\"channel\":8,\"number\":75,\"action\":\"pan-1\"},{\"bad\":true}]");
    check(engine.reloadMidiMappings().failed() && engine.mappings.size() == 1 && engine.mappings[0].action.kind == MidiActionKind::pan,
        "an invalid saved entry rejects the whole reload while retaining the working active layout");
    profile.replaceWithText(restored);

    Workstation ui(false); ui.stopTimer(); ui.setSize(1250, 800);
    const auto project = juce::JSON::toString(projectToJson(ui.engine.project)); const auto history = ui.undoHistory.size(); const bool dirty = ui.dirty;
    connection.profile = profile; connection.mappings = 47; connection.listening = true;
    connection.ports = {{"primary", "FLkey MIDI", true, false, false}};
    ui.showControllerConnection(connection);
    check(ui.controllerConnection.isVisible() && ui.controllerConnection.getButtonText() == "FLkey MIDI / connected"
        && ui.layout.status.contains(ui.controllerConnection.getBounds()) && ui.controllerConnection.getTooltip().contains("47 active mappings"),
        "persistent native controller indicator is visible and reachable at minimum window size");
    const auto capture = [&](const juce::String& name)
    { auto stream = report.getParentDirectory().getChildFile(name).createOutputStream(); juce::PNGImageFormat png;
      return stream && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *stream); };
    check(capture("controller-connected-1250.png"), "capture the native connected indicator at minimum width using explicit fixture facts");
    connection.ports[0].received = connection.ports[0].recent = true; ui.showControllerConnection(connection); ui.setSize(1920, 1080);
    check(capture("controller-receiving-1920.png") && ui.controllerConnection.getButtonText().contains("receiving"), "capture the separate receiving state at desktop width");
    connection.ports[0].open = false; ui.showControllerConnection(connection);
    check(ui.controllerConnection.getButtonText().contains("detected"), "disabling a detected input immediately removes the connected label");
    connection.ports.clear(); ui.showControllerConnection(connection);
    check(ui.controllerConnection.getButtonText() == "No MIDI controller" && juce::JSON::toString(projectToJson(ui.engine.project)) == project
        && ui.undoHistory.size() == history && ui.dirty == dirty, "connection status changes do not modify the mix, dirty state or musical Undo");
    ui.controllerConnection.onClick();
    check(ui.settingsWindow && ui.settingsWindow->isVisible(), "clicking the connection confirmation opens existing Audio / MIDI settings without changing mappings");
    auto* reload = dynamic_cast<juce::TextButton*>(ui.settingsWindow->getContentComponent()->findChildWithID("reload-midi-mappings"));
    check(reload && reload->isVisible() && reload->getButtonText() == "Reload saved mappings"
        && ui.settingsWindow->getContentComponent()->getLocalBounds().contains(reload->getBounds()), "MIDI settings expose the saved-profile reload beside guided setup");
    ui.settingsWindow.reset();
}
}
