#include "Workstation.h"
#include <juce_cryptography/juce_cryptography.h>
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace mixworks
{
namespace
{
void pumpMidiMessages(int milliseconds)
{
    const auto until = juce::Time::getMillisecondCounterHiRes() + milliseconds;
    do
    {
       #if JUCE_WINDOWS
        MSG message;
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
       #endif
        juce::Thread::sleep(1);
    } while (juce::Time::getMillisecondCounterHiRes() < until);
}
}

void runMidiConnectionTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    using State = MidiConnectionStatus::State;
    MidiConnectionStatus connection;
    check(connection.state() == State::absent && connection.caption() == "Audio / MIDI", "missing inputs remain explicit without claiming a connection");
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
    check(connection.caption().contains("+1 / receiving"), "the receiving label identifies the port that actually received a message");
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
    profile.replaceWithText("[{\"status\":176,\"channel\":8,\"number\":21,\"action\":\"pan-1\"},{\"status\":176,\"channel\":8,\"number\":22,\"action\":\"pan-1\"}]");
    check(engine.reloadMidiMappings().failed() && engine.mappings.size() == 1 && engine.mappings[0].action.kind == MidiActionKind::deskPot,
        "duplicate action assignments reject the entire profile rather than silently routing two controls to one action");
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
    check(engine.activeMappingTitle() == "Unnamed mapping", "legacy profiles are explicitly unnamed rather than named after their filename");
    const auto named = [&] (const juce::String& title) {
        auto* doc = new juce::DynamicObject(); doc->setProperty("format", "mixworks-midi-profile"); doc->setProperty("version", 1);
        doc->setProperty("title", title); doc->setProperty("mappings", juce::JSON::parse(restored)); return juce::JSON::toString(juce::var(doc));
    };
    profile.replaceWithText(named("Alloy FLkey desk"));
    check(engine.reloadMidiMappings().wasOk() && engine.activeMappingTitle() == "Alloy FLkey desk"
        && engine.midiConnectionStatus().mappingTitle == "Alloy FLkey desk", "reload publishes a title only with its successfully loaded assignments");
    profile.replaceWithText(named("Different on disk"));
    check(engine.activeMappingTitle() == "Alloy FLkey desk", "changing the disk title does not mislabel the active in-memory assignments");
    profile.replaceWithText(named("bad\nname"));
    check(engine.reloadMidiMappings().failed() && engine.activeMappingTitle() == "Alloy FLkey desk", "failed metadata validation retains the active name and routing");
    engine.saveMidiMappings(); Engine reopened(profile);
    check(reopened.activeMappingTitle() == "Alloy FLkey desk" && reopened.mappings.size() == 1, "automatic mapping saves and startup retain title metadata");
    profile.replaceWithText(restored); engine.reloadMidiMappings();
    check(engine.activeMappingTitle() == "Unnamed mapping", "switching back to a legacy profile clears the previous active title");
    profile.replaceWithText("[]"); engine.reloadMidiMappings();
    check(engine.activeMappingTitle() == "Default / no mappings", "empty legacy mappings have an explicit default state");
    {
        Engine walkthrough(root.getChildFile("walkthrough.json")); walkthrough.beginControllerSetup(true);
        for (int i = 0; i < 46; ++i) { walkthrough.testMidi(juce::MidiMessage::controllerEvent(8, i, 127)); walkthrough.flushMidiControls(); }
        check(!walkthrough.learnAction && !walkthrough.isMidiLearnEnabled() && !walkthrough.canSkipControllerStep()
            && walkthrough.mappings.size() == 46, "full current walkthrough completes without asking to learn already-standard realtime Play and Stop");
        check(walkthrough.mappings.size() >= 17 && walkthrough.mappings[0].action == MidiAction {MidiActionKind::deskPot, 0}
            && walkthrough.mappings[12].action == MidiAction {MidiActionKind::pan, 0}
            && walkthrough.mappings[16].action.kind == MidiActionKind::master,
            "full walkthrough teaches the paired pot identity, Pan faders and master in physical order");
        const auto saved = walkthrough.midiConnectionStatus().profile.loadFileAsString();
        walkthrough.beginLearn("play"); walkthrough.testMidi(juce::MidiMessage::midiStart()); walkthrough.flushMidiControls();
        check(!walkthrough.learnAction && !walkthrough.playing && walkthrough.midiConnectionStatus().profile.loadFileAsString() == saved,
            "individual Learn recognises realtime Start without playing audio or storing an unsupported binding");
        walkthrough.beginLearn("stop"); walkthrough.testMidi(juce::MidiMessage::midiStop()); walkthrough.flushMidiControls();
        check(!walkthrough.learnAction && walkthrough.mappingSummary().contains("standard MIDI"),
            "individual Learn confirms realtime Stop is already supported instead of waiting indefinitely");
        walkthrough.beginLearn({}); walkthrough.testMidi(juce::MidiMessage::midiStart()); walkthrough.flushMidiControls();
        check(walkthrough.playing, "standard realtime transport remains operational after Learn ends");
        walkthrough.stop();
        walkthrough.setMidiLearnEnabled(true); walkthrough.testMidi(juce::MidiMessage::midiStart()); walkthrough.flushMidiControls();
        check(walkthrough.playing && !walkthrough.canSkipControllerStep(), "enabling surface selection alone preserves transport until a target is chosen and cannot skip a stale guide");
        walkthrough.beginLearn({}); walkthrough.stop();
    }

    {
        MidiReconnectPolicy reconnect;
        auto requests = reconnect.update({{"main", "FLkey MIDI", false}, {"daw", "MIDIIN2 (FLkey MIDI)", false}});
        check(requests.size() == 1 && requests[0].id == "main" && requests[0].enabled, "initial FLkey detection opens only the primary input");
        reconnect.update({{"main", "FLkey MIDI", true}, {"daw", "MIDIIN2 (FLkey MIDI)", false}});
        requests = reconnect.update({});
        check(requests.size() == 1 && requests[0].id == "main" && !requests[0].enabled, "disconnect releases the previously enabled handle without touching assignments");
        requests = reconnect.update({{"changed-id", "FLkey MIDI", false}, {"daw", "MIDIIN2 (FLkey MIDI)", false}});
        check(requests.size() == 1 && requests[0].id == "changed-id" && requests[0].enabled, "unique controller name reconnects after a Windows identifier change");
        reconnect.update({{"changed-id", "FLkey MIDI", true}});
        requests = reconnect.update({{"changed-id", "FLkey MIDI", false}});
        check(requests.size() == 1 && !requests[0].enabled, "a manual input disable resets held controls without being overridden by polling");
        reconnect.update({});
        check(reconnect.update({{"third-id", "FLkey MIDI", false}}).empty(), "manual disable survives disconnect and a unique-name identifier change within this run");
        MidiReconnectPolicy generic;
        generic.update({{"other", "Other controller", true}}); generic.update({});
        requests = generic.update({{"other", "Other controller", false}});
        check(requests.size() == 1 && requests[0].enabled, "a previously enabled non-FLkey input reconnects without a name-specific remap");
        MidiReconnectPolicy ambiguous;
        ambiguous.update({{"old", "Keys", true}}); ambiguous.update({});
        check(ambiguous.update({{"one", "Keys", false}, {"two", "Keys", false}}).empty(), "ambiguous names never select a different controller automatically");
    }
    {
        auto lifecycle = std::make_unique<Engine>(root.getChildFile("lifecycle.json"));
        lifecycle->mappings = {{0xb0, 8, 75, {MidiActionKind::pan, 0}}}; lifecycle->saveMidiMappings();
        const auto musical = juce::JSON::toString(projectToJson(lifecycle->project));
        check(lifecycle->saveMidiProfile().failed() && lifecycle->renameMidiMapping("bad\nname").failed(), "unnamed Save and control-character names fail without changing active assignments");
        check(lifecycle->saveMidiProfile("  FLkey main  ", true).wasOk() && lifecycle->midiMappingName() == "FLkey main" && lifecycle->midiProfileFiles().size() == 1,
            "Save as creates a named profile from active assignments and trims its display name");
        const auto selected = lifecycle->selectedMidiProfile(); const auto saved = selected.loadFileAsString();
        check(lifecycle->saveMidiProfile("flKEY MAIN", true).failed() && lifecycle->selectedMidiProfile() == selected && selected.loadFileAsString() == saved,
            "case-insensitive duplicate names never overwrite a saved profile");
        auto reopenedProfile = std::make_unique<Engine>(root.getChildFile("lifecycle.json"));
        check(reopenedProfile->selectedMidiProfile() == selected && reopenedProfile->mappings.size() == 1 && reopenedProfile->midiMappingName() == "FLkey main",
            "fresh startup restores the active snapshot and named identity without implicitly opening its disk copy");
        lifecycle->beginLearn("desk-pot-1"); lifecycle->testMidi(juce::MidiMessage::controllerEvent(8, 21, 127)); lifecycle->flushMidiControls();
        check(lifecycle->mappings.size() == 2 && selected.loadFileAsString() == saved && lifecycle->mappingSummary().contains("differs from active"),
            "Learn autosaves the startup snapshot while leaving the named copy unchanged and visibly different");
        lifecycle->beginLearn("pan-1"); lifecycle->testMidi(juce::MidiMessage::controllerEvent(8, 21, 127)); lifecycle->flushMidiControls();
        check(lifecycle->mappings.size() == 1 && lifecycle->mappings[0].action.kind == MidiActionKind::pan && lifecycle->mappings[0].number == 21,
            "individual remapping replaces both the action and physical-control conflict without unrelated assignments");
        check(lifecycle->renameMidiMapping("Renamed desk").wasOk() && selected.loadFileAsString() == saved && lifecycle->saveMidiProfile().wasOk()
            && juce::JSON::parse(selected.loadFileAsString())["title"].toString() == "Renamed desk", "naming updates the active snapshot and explicit Save updates the named copy");
        const auto external = root.getChildFile("exported.json"); const auto activeBeforeExport = root.getChildFile("lifecycle.json").loadFileAsString();
        check(lifecycle->exportMidiProfile(external).wasOk() && lifecycle->selectedMidiProfile() == selected
            && !juce::JSON::parse(external.loadFileAsString()).hasProperty("savedProfile") && root.getChildFile("lifecycle.json").loadFileAsString() == activeBeforeExport,
            "Export writes a portable profile without active-file references or changes to startup identity");
        check(lifecycle->exportMidiProfile(selected).failed() && lifecycle->exportMidiProfile(root.getChildFile("lifecycle.json")).failed(),
            "Export cannot overwrite the internal snapshot or managed named profiles");
        const auto importedBytes = external.loadFileAsString(); lifecycle->beginLearn("mid-1");
        check(lifecycle->importMidiProfile(external).wasOk() && !lifecycle->learnAction && lifecycle->selectedMidiProfile() == juce::File()
            && external.loadFileAsString() == importedBytes, "Import validates and activates a copy, cancels Learn and leaves its original file unchanged");
        const auto invalid = root.getChildFile("invalid.json"); invalid.replaceWithText("[{\"status\":176}]"); lifecycle->beginLearn("mid-1");
        const auto activeBeforeFailure = root.getChildFile("lifecycle.json").loadFileAsString();
        check(lifecycle->importMidiProfile(invalid).failed() && lifecycle->learnAction && lifecycle->midiMappingName() == "Renamed desk"
            && root.getChildFile("lifecycle.json").loadFileAsString() == activeBeforeFailure, "malformed Import preserves active routing, Learn, identity and startup bytes");
        lifecycle->beginLearn({});
        lifecycle->exporting = true;
        check(lifecycle->openMidiProfile(selected).failed() && lifecycle->saveMidiProfile("Blocked", true).failed() && lifecycle->importMidiProfile(external).failed()
            && lifecycle->renameMidiMapping("Blocked").failed() && lifecycle->exportMidiProfile(root.getChildFile("blocked-export.json")).failed(), "profile mutation and export reject exclusive audio export");
        lifecycle->exporting = false;
        check(lifecycle->openMidiProfile(selected).wasOk() && lifecycle->selectedMidiProfile() == selected, "Open switches to a fully validated saved profile and records its named identity");
        const auto beforeDeletion = lifecycle->mappings.size(); lifecycle->beginLearn("mid-1");
        check(lifecycle->deleteMidiProfile().wasOk() && !selected.existsAsFile() && lifecycle->midiProfileFiles().isEmpty()
            && lifecycle->mappings.size() == beforeDeletion && lifecycle->learnAction && lifecycle->midiMappingName() == "Renamed desk",
            "active-profile deletion archives only the named file and retains active assignments, name and Learn");
        bool archived = false; for (const auto& backup : selected.getParentDirectory().findChildFiles(juce::File::findFiles, false, "*.deleted*.bak")) archived |= backup.loadFileAsString() == importedBytes;
        reopenedProfile = std::make_unique<Engine>(root.getChildFile("lifecycle.json"));
        check(archived && reopenedProfile->mappings.size() == beforeDeletion && reopenedProfile->selectedMidiProfile() == juce::File()
            && juce::JSON::toString(projectToJson(lifecycle->project)) == musical, "deletion keeps exact archived bytes and reopening retains routing without changing musical state");
        check(lifecycle->saveMidiProfile("Replacement", true).wasOk(), "Save as after deletion creates an independent named profile");
        const auto namedMissing = lifecycle->selectedMidiProfile(); namedMissing.deleteFile();
        check(lifecycle->mappingSummary().contains("missing; active assignments retained") && lifecycle->deleteMidiProfile().failed()
            && lifecycle->saveMidiProfile().wasOk() && namedMissing.existsAsFile(), "an externally missing named file is explicit and can be recreated only by Save");
        auto blocked = root.getChildFile("blocked-parent"); blocked.replaceWithText("not a directory");
        auto unwritable = std::make_unique<Engine>(blocked.getChildFile("active.json"), false);
        unwritable->mappings = {{0xb0, 8, 12, {MidiActionKind::volume, 0}}}; unwritable->beginLearn("pan-1");
        check(unwritable->importMidiProfile(external).failed() && unwritable->mappings[0].number == 12 && unwritable->learnAction
            && unwritable->renameMidiMapping("Cannot save").failed() && unwritable->midiMappingName().isEmpty()
            && unwritable->saveMidiProfile("Cannot save", true).failed(), "unwritable destinations reject activation and naming before replacing working routing or Learn");
        invalid.replaceWithText(juce::String::repeatedString(" ", 1024 * 1024 + 1));
        const auto savedIdentity = lifecycle->selectedMidiProfile(); const auto savedCount = lifecycle->mappings.size();
        check(lifecycle->importMidiProfile(invalid).failed() && lifecycle->selectedMidiProfile() == savedIdentity && lifecycle->mappings.size() == savedCount,
            "oversized imported profiles fail before parsing while preserving active assignments and identity");
        invalid.replaceWithText("{\"format\":\"mixworks-midi-profile\",\"version\":1,\"title\":\"bad ref\",\"savedProfile\":\"../outside.json\",\"mappings\":[]}");
        auto invalidStartup = std::make_unique<Engine>(invalid);
        check(invalidStartup->midiConnectionStatus().profileError.isNotEmpty() && invalidStartup->selectedMidiProfile() == juce::File(), "startup rejects traversal in named-profile identity");
    }

    auto ownedStackFixture_ui = std::make_unique<Workstation>(false); auto& ui = *ownedStackFixture_ui; ui.stopTimer(); ui.setSize(1250, 800);
    const auto project = juce::JSON::toString(projectToJson(ui.engine.project)); const auto history = ui.undoHistory.size(); const bool dirty = ui.dirty;
    connection.profile = profile; connection.mappings = 47; connection.listening = true;
    connection.ports = {{"primary", "FLkey MIDI", true, false, false}};
    ui.showControllerConnection(connection);
    check(ui.settingsButton.isVisible() && ui.settingsButton.getButtonText().contains("FLkey MIDI / connected")
        && ui.layout.toolbar.contains(ui.settingsButton.getBounds()) && ui.settingsButton.getTooltip().contains("47 active mappings"),
        "persistent native controller indicator is visible and reachable at minimum window size");
    const auto capture = [&](const juce::String& name)
    { auto stream = report.getParentDirectory().getChildFile(name).createOutputStream(); juce::PNGImageFormat png;
      return stream && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *stream); };
    check(capture("controller-connected-1250.png"), "capture the native connected indicator at minimum width using explicit fixture facts");
    connection.ports[0].received = connection.ports[0].recent = true; ui.showControllerConnection(connection); ui.setSize(1920, 1080);
    check(capture("controller-receiving-1920.png") && ui.settingsButton.getButtonText().contains("receiving"), "capture the separate receiving state at desktop width");
    connection.ports[0].open = false; ui.showControllerConnection(connection);
    check(ui.settingsButton.getButtonText().contains("detected"), "disabling a detected input immediately removes the connected label");
    connection.ports.clear(); ui.showControllerConnection(connection);
    check(ui.settingsButton.getButtonText() == "Audio / MIDI" && juce::JSON::toString(projectToJson(ui.engine.project)) == project
        && ui.undoHistory.size() == history && ui.dirty == dirty, "connection status changes do not modify the mix, dirty state or musical Undo");
    // This workflow checks the full settings layout. A small auxiliary primary
    // display may clamp a modal even when a supported larger display exists.
    for (const auto& display : juce::Desktop::getInstance().getDisplays().displays)
        if (display.userBounds.getWidth() >= 1000 && display.userBounds.getHeight() >= 900)
        { ui.setTopLeftPosition(display.userBounds.getLargestIntegerWithin().getPosition()); break; }
    ui.settingsButton.onClick();
    check(ui.settingsWindow && ui.settingsWindow->isVisible(), "clicking the connection confirmation opens existing Audio / MIDI settings without changing mappings");
    {
        auto* setup = dynamic_cast<juce::TextButton*>(ui.settingsWindow->getContentComponent()->findChildWithID("guided-midi-setup"));
        auto* cancel = dynamic_cast<juce::TextButton*>(ui.settingsWindow->getContentComponent()->findChildWithID("cancel-midi-learn"));
        auto* learn = dynamic_cast<juce::TextButton*>(ui.settingsWindow->getContentComponent()->findChildWithID("learn-midi-control"));
        auto* action = dynamic_cast<juce::ComboBox*>(ui.settingsWindow->getContentComponent()->findChildWithID("midi-learn-action"));
        check(setup != nullptr, "settings expose the shared current live-desk setup entry point");
        // Match the user's partial setup: four lane levels and master already learned.
        ui.engine.mappings.clear();
        for (int i = 0; i < 4; ++i) ui.engine.mappings.push_back({0xb0, 8, 71 + i, {MidiActionKind::volume, i}});
        ui.engine.mappings.push_back({0xb0, 8, 79, {MidiActionKind::master}});
        const auto five = ui.engine.mappings;
        if (setup) setup->onClick(); else ui.engine.beginControllerSetup();
        check(ui.engine.mappingSummary().contains("Pot 1") && ui.engine.learnAction == juce::Optional<MidiAction>(MidiAction {MidiActionKind::deskPot, 0}),
            "resuming from five faders teaches the current paired desk pots, never the legacy Mid faders");
        check(cancel && learn && action, "native settings expose Cancel, action selection and individual Learn");
        if (cancel) cancel->onClick();
        ui.engine.testMidi(juce::MidiMessage::controllerEvent(8, 71, 0)); ui.engine.flushMidiControls();
        check(ui.engine.mappings.size() == five.size() && ui.engine.project.tracks[0].gainDb == -60,
            "cancelling partial setup retains all five assignments and immediately routes an existing fader");
        if (action) for (int i = 0; i < action->getNumItems(); ++i)
            if (action->getItemText(i) == "pan-1") action->setSelectedId(action->getItemId(i), juce::dontSendNotification);
        if (learn) learn->onClick();
        ui.engine.testMidi(juce::MidiMessage::controllerEvent(8, 75, 127)); ui.engine.flushMidiControls();
        ui.engine.testMidi(juce::MidiMessage::controllerEvent(8, 71, 127));
        ui.engine.testMidi(juce::MidiMessage::controllerEvent(8, 75, 0)); ui.engine.flushMidiControls();
        check(ui.engine.mappings.size() == 6 && ui.engine.project.tracks[0].gainDb == 0 && ui.engine.project.tracks[0].pan == -1,
            "individual Learn after cancel adds Pan and leaves the existing fader operational with its first-sweep unity stop");
        ui.engine.reloadMidiMappings();
        check(ui.engine.mappings.size() == 6 && ui.engine.mappings.back().action.kind == MidiActionKind::pan,
            "partial guided setup followed by individual Learn persists the current mapping on reload");
        check(ui.engine.mappingSummary().contains("Last MIDI: " + ui.engine.lastMidiText())
            && ui.engine.mappingSummary().contains("Last profile read:"), "settings retain the incoming MIDI observation separately from the last profile read");
        ui.engine.beginControllerSetup();
        while (ui.engine.learnAction && ui.engine.learnAction->kind != MidiActionKind::pan) ui.engine.skipControllerStep();
        check(ui.engine.learnAction == juce::Optional<MidiAction>(MidiAction {MidiActionKind::pan, 1})
            && !ui.engine.mappingSummary().contains("mid EQ"), "resumed current setup skips the six existing actions and teaches remaining Pan faders");
        ui.engine.beginLearn({});
        auto screenshot = report.getSiblingFile("midi-partial-learn-result.png").createOutputStream(); juce::PNGImageFormat png;
        check(screenshot && png.writeImageToStream(ui.settingsWindow->getContentComponent()->createComponentSnapshot(ui.settingsWindow->getContentComponent()->getLocalBounds()), *screenshot),
            "capture actual guided-cancel-individual Learn settings and preserved routing");
        ui.performance->setup.onClick(); ui.performance->refresh(true);
        if (cancel) cancel->onClick(); ui.performance->refresh(true);
        check(!ui.engine.learnAction && !ui.performance->learn.getToggleState(),
            "settings Cancel also exits live desk Map mode instead of leaving a stale Learn checkbox");
        ui.performance->faders[0].setValue(-12, juce::sendNotificationSync);
        check(!ui.engine.learnAction && ui.engine.controlSettings(0).gainDb == -12,
            "a desk fader after cross-window cancellation performs normally rather than silently rearming Learn");
        ui.performance->learn.setToggleState(false, juce::dontSendNotification);
        if (learn) learn->onClick(); ui.performance->refresh(true);
        check(ui.engine.learnAction && ui.performance->learn.getToggleState(),
            "individual settings Learn is reflected by the live desk rather than creating two conflicting modes");
        ui.performance->cancelLearn.onClick(); ui.performance->refresh(true);
        check(!ui.engine.learnAction && !ui.performance->learn.getToggleState(), "desk Cancel also cancels individual settings Learn");
    }
    auto* reload = dynamic_cast<juce::TextButton*>(ui.settingsWindow->getContentComponent()->findChildWithID("reload-midi-mappings"));
    check(reload && reload->isVisible() && reload->getButtonText() == "Reload saved mappings"
        && ui.settingsWindow->getContentComponent()->getLocalBounds().contains(reload->getBounds()), "MIDI settings expose the saved-profile reload beside guided setup");
    {
        const auto liveProfile = ui.engine.midiConnectionStatus().profile;
        const bool existed = liveProfile.existsAsFile(); const auto previous = liveProfile.loadFileAsString();
        ui.engine.mappings.assign(39, {0xb0, 8, 75, {MidiActionKind::mid, 0}});
        ui.engine.beginLearn("desk-pot-1"); liveProfile.replaceWithText("[]\n");
        if (reload) reload->onClick();
        check(ui.engine.mappings.empty() && !ui.engine.learnAction.hasValue() && ui.engine.midiConnectionStatus().mappings == 0
            && ui.engine.mappingSummary().contains("Active mappings: 0"),
            "actual settings Reload callback clears 39 stale active mappings and Learn from an empty saved file");
        check(ui.engine.mappingSummary().contains(juce::SHA256(liveProfile).toHexString())
            && ui.engine.mappingSummary().contains("Read " + juce::String(liveProfile.getSize()) + " bytes"), "reload diagnostics identify the exact profile bytes actually consumed");
        auto* clearButton = dynamic_cast<juce::TextButton*>(ui.settingsWindow->getContentComponent()->findChildWithID("clear-midi-mappings"));
        liveProfile.replaceWithText(original); ui.engine.reloadMidiMappings(); ui.engine.beginLearn("mid-1");
        const auto liveBefore = liveProfile.loadFileAsString();
        ui.engine.exporting = true;
        if (clearButton) clearButton->onClick();
        check(ui.engine.mappings.size() == 1 && liveProfile.loadFileAsString() == liveBefore, "exclusive export rejects clearing active and saved mappings");
        ui.engine.exporting = false;
        const auto clearSession = juce::JSON::toString(projectToJson(ui.engine.project)); const auto clearHistory = ui.undoHistory.size();
        if (clearButton) clearButton->onClick();
        Engine emptyReopened(liveProfile);
        check(clearButton && ui.engine.mappings.empty() && emptyReopened.mappings.empty() && !ui.engine.learnAction.hasValue()
            && liveProfile.loadFileAsString().trim() == "[]" && ui.engine.activeMappingTitle() == "Default / no mappings",
            "actual Clear mappings button atomically empties active and saved routing and survives fresh startup");
        bool backupFound = false;
        for (const auto& backup : liveProfile.getParentDirectory().findChildFiles(juce::File::findFiles, false, liveProfile.getFileName() + "*.bak"))
            backupFound |= backup.loadFileAsString() == liveBefore;
        check(backupFound && juce::JSON::toString(projectToJson(ui.engine.project)) == clearSession && ui.undoHistory.size() == clearHistory,
            "clear retains the exact previous profile backup, musical session and Undo");
        auto screenshot = report.getSiblingFile("midi-clear-result.png").createOutputStream(); juce::PNGImageFormat png;
        check(clearButton && clearButton->isVisible() && clearButton->getWidth() >= 180 && screenshot
            && png.writeImageToStream(ui.settingsWindow->getContentComponent()->createComponentSnapshot(ui.settingsWindow->getContentComponent()->getLocalBounds()), *screenshot),
            "capture the actual Clear mappings control and its zero-assignment result");
        if (existed) liveProfile.replaceWithText(previous); else liveProfile.deleteFile();
    }
    {
        auto* panel = ui.settingsWindow->getContentComponent();
        auto* name = dynamic_cast<juce::TextEditor*>(panel->findChildWithID("midi-profile-name"));
        auto* choices = dynamic_cast<juce::ComboBox*>(panel->findChildWithID("midi-profiles"));
        const auto press = [&](const char* id) { auto* button = dynamic_cast<juce::TextButton*>(panel->findChildWithID(id)); if (button && button->onClick) button->onClick(); return button; };
        const auto activeFile = ui.engine.midiConnectionStatus().profile; const auto before = activeFile.loadFileAsString();
        const auto beforeProject = juce::JSON::toString(projectToJson(ui.engine.project)); const auto beforeUndo = ui.undoHistory.size(); const bool beforeDirty = ui.dirty;
        ui.engine.reloadMidiMappings();
        if (name) name->setText("Native FLkey profile", false);
        check(name && choices && activeFile.loadFileAsString() == before && ui.engine.midiMappingName() != "Native FLkey profile", "typing a profile name and choosing a list item alone never modify active routing");
        press("save-as-midi-profile"); const auto nativeProfile = ui.engine.selectedMidiProfile();
        check(nativeProfile.existsAsFile() && ui.engine.midiMappingName() == "Native FLkey profile" && choices && choices->getNumItems() >= 1,
            "actual settings Save as creates and lists a named profile");
        if (name) name->setText("Native renamed desk", false); press("name-midi-profile"); press("save-midi-profile");
        check(ui.engine.midiMappingName() == "Native renamed desk" && juce::JSON::parse(nativeProfile.loadFileAsString())["title"].toString() == "Native renamed desk", "actual naming and Save callbacks persist the active named profile");
        ui.engine.mappings.clear();
        press("open-midi-profile");
        check(ui.engine.selectedMidiProfile() == nativeProfile && ui.engine.midiMappingName() == "Native renamed desk", "actual Open callback activates the chosen disk profile");
        bool geometry = true;
        juce::Array<juce::var> profileGeometry;
        for (const auto* id : {"midi-profiles", "midi-profile-name", "open-midi-profile", "save-midi-profile", "save-as-midi-profile", "name-midi-profile", "import-midi-profile", "export-midi-profile", "delete-midi-profile"})
        { auto* child = panel->findChildWithID(id); geometry &= child && child->isVisible() && panel->getLocalBounds().contains(child->getBounds()) && child->getWidth() >= 100 && child->getHeight() >= 24;
          auto* item = new juce::DynamicObject(); item->setProperty("id",id); item->setProperty("panel",panel->getLocalBounds().toString());
          if (child) { item->setProperty("bounds",child->getBounds().toString()); item->setProperty("visible",child->isVisible()); } profileGeometry.add(juce::var(item)); }
        report.getSiblingFile("p08-midi-profile-geometry.json").replaceWithText(juce::JSON::toString(profileGeometry));
        auto stream = report.getSiblingFile("p08-midi-profiles.png").createOutputStream(); juce::PNGImageFormat png;
        const bool profileCapture = stream && png.writeImageToStream(panel->createComponentSnapshot(panel->getLocalBounds()), *stream);
        check(geometry && profileCapture, "capture actual profile controls with readable bounded targets in the native settings panel");
        // Exercise both real non-native file pickers and cancellation callbacks.
        for (const auto* id : {"import-midi-profile", "export-midi-profile"})
        {
            const auto snapshot = activeFile.loadFileAsString(); const auto activeName = ui.engine.midiMappingName();
            press(id);
            auto* modal = juce::Component::getCurrentlyModalComponent();
            const bool picker = dynamic_cast<juce::FileChooserDialogBox*>(modal) != nullptr;
            if (modal) modal->exitModalState(0);
            pumpMidiMessages(30);
            auto* status = dynamic_cast<juce::TextEditor*>(panel->findChildWithID("midi-profile-status"));
            check(status && status->getText().contains("Profile file selection cancelled."), "actual cancelled picker callback reports completion without a profile write");
            check(picker && activeFile.loadFileAsString() == snapshot && ui.engine.midiMappingName() == activeName && ui.engine.selectedMidiProfile() == nativeProfile,
                "native " + juce::String(id) + " picker cancellation retains assignments and identity");
        }
        std::function<juce::FileBrowserComponent*(juce::Component*)> findBrowser = [&](juce::Component* component) -> juce::FileBrowserComponent*
        {
            if (auto* browser = dynamic_cast<juce::FileBrowserComponent*>(component)) return browser;
            if (component) for (auto* child : component->getChildren()) if (auto* browser = findBrowser(child)) return browser;
            return nullptr;
        };
        const auto nativeExport = report.getSiblingFile("p08-native-export.json");
        const auto completePicker = [&](const char* id, const juce::File& file)
        {
            press(id); pumpMidiMessages(30);
            auto* modal = juce::Component::getCurrentlyModalComponent(); auto* browser = findBrowser(modal);
            if (browser) { browser->setRoot(file.getParentDirectory()); browser->setFileName(file.getFileName()); pumpMidiMessages(30); }
            if (modal) modal->exitModalState(browser ? 1 : 0);
            pumpMidiMessages(50); return browser != nullptr;
        };
        const auto preExport = activeFile.loadFileAsString();
        check(completePicker("export-midi-profile", nativeExport) && nativeExport.existsAsFile() && activeFile.loadFileAsString() == preExport
            && ui.engine.selectedMidiProfile() == nativeProfile, "actual Export picker writes a portable JSON while preserving active identity");
        auto portable = juce::JSON::parse(nativeExport.loadFileAsString());
        if (auto* object = portable.getDynamicObject()) object->setProperty("title", "Native imported desk");
        nativeExport.replaceWithText(juce::JSON::toString(portable)); const auto importBytes = nativeExport.loadFileAsString();
        check(completePicker("import-midi-profile", nativeExport) && ui.engine.midiMappingName() == "Native imported desk"
            && ui.engine.selectedMidiProfile() == juce::File() && nativeExport.loadFileAsString() == importBytes,
            "actual Import picker activates validated content without rewriting its portable original");
        if (choices) choices->setSelectedId(1, juce::dontSendNotification); press("open-midi-profile");
        press("delete-midi-profile");
        check(!nativeProfile.existsAsFile() && ui.engine.selectedMidiProfile() == juce::File() && ui.engine.midiMappingName() == "Native renamed desk"
            && juce::JSON::toString(projectToJson(ui.engine.project)) == beforeProject && ui.undoHistory.size() == beforeUndo && ui.dirty == beforeDirty,
            "actual Delete archives the active named copy while preserving musical state, dirty flag and Undo");
        activeFile.replaceWithText(before); ui.engine.reloadMidiMappings();
    }

    ui.settingsWindow.reset();
    const auto title = ui.performance->findChildWithID("active-midi-profile");
    const auto deskProfile = ui.engine.midiConnectionStatus().profile; const bool hadDeskProfile = deskProfile.existsAsFile(); const auto deskProfileBytes = deskProfile.loadFileAsString();
    deskProfile.replaceWithText(named("Alloy FLkey desk")); ui.engine.reloadMidiMappings(); ui.performance->refresh(true);
    check(title && dynamic_cast<juce::Label*>(title)->getText().contains("Mapping: Alloy FLkey desk"), "live desk displays the title of the actual active mapping");
    for (const int width : {1250, 1440, 1920, 2560})
    {
        connection.ports = {{"long", "A very long controller input name requiring truncation", true, true, true}, {"second", "Second controller", true, false, false}};
        ui.showControllerConnection(connection); ui.setSize(width, 1080);
        check(ui.layout.toolbar.contains(ui.settingsButton.getBounds()) && ui.settingsButton.getButtonText().contains("+1 / receiving")
            && title && title->getRight() <= ui.performanceViewport.getViewWidth()
            && ui.settingsButton.getTooltip().contains("Second controller") && ui.settingsButton.getTooltip().contains("requiring truncation"),
            "single controller/settings button handles long names and additional ports at " + juce::String(width));
        check(capture("stage1-toolbar-" + juce::String(width) + ".png"), "capture merged settings and active mapping at compact/wide width");
    }
    if (hadDeskProfile) deskProfile.replaceWithText(deskProfileBytes); else deskProfile.deleteFile();
}
}
