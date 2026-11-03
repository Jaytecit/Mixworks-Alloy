#include "Workstation.h"
#include <juce_cryptography/juce_cryptography.h>
#if JUCE_WINDOWS
#include <windows.h>
#endif

namespace mixworks
{
void runRecoveryTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    auto& defaultLook = juce::LookAndFeel::getDefaultLookAndFeel();
    const bool nativeAlerts = defaultLook.isUsingNativeAlertWindows(); defaultLook.setUsingNativeAlertWindows(false);
    struct RestoreAlerts { juce::LookAndFeel& look; bool enabled; ~RestoreAlerts() { look.setUsingNativeAlertWindows(enabled); } } restoreAlerts {defaultLook, nativeAlerts};
    const auto root = report.getParentDirectory().getChildFile("recovery-" + juce::Uuid().toString()); juce::String error;
    auto owner = RecoveryStore::create(root, error); check(owner != nullptr, "recovery creates an identified independent window lease: " + error);
    if (!owner) return;
    const auto folder = owner->directory();
    auto child = [&](juce::StringArray flags, const juce::File& result)
    {
        juce::StringArray command {juce::File::getSpecialLocation(juce::File::currentExecutableFile).getFullPathName(), "--self-test"};
        command.addArray(flags); command.add("--report"); command.add(result.getFullPathName()); juce::ChildProcess process;
        if (!process.start(command)) return false;
        if (!process.waitForProcessToFinish(10000)) { process.kill(); return false; } // Owned child handle only.
        return process.getExitCode() == 0;
    };
    check(child({"--recovery-lease-probe", "--recovery-folder", folder.getFullPathName()}, root.getChildFile("live-lease.txt")),
          "a separate native process cannot claim a running window's lease");
    const auto crashReport = root.getChildFile("crashed-folder.txt");
    check(child({"--recovery-crash-fixture", "--recovery-folder", root.getFullPathName()}, crashReport), "native recovery fixture exits without C++ cleanup");
    const auto crashedFolder = juce::File(crashReport.loadFileAsString());
    check(child({"--recovery-lease-probe", "--recovery-folder", crashedFolder.getFullPathName(), "--expect-available"}, root.getChildFile("dead-lease.txt")),
          "the operating system releases a crashed process lease without PID heuristics");
    auto crashed = RecoveryStore::claim(crashedFolder, error);
    check(crashed && crashed->read().error.isEmpty() && crashed->discard().wasOk(), "a real abnormal-exit snapshot remains valid and explicitly discardable"); crashed.reset();

    Project p; p.name = "First durable mix"; const auto first = juce::JSON::toString(projectToJson(p));
    check(owner->write(first, {}).wasOk() && owner->read().document == first, "first transactional recovery snapshot reads exact session text");
    const auto unchanged = folder.getChildFile("current.json").loadFileAsString();
    check(owner->write(first, {}).wasOk() && folder.getChildFile("current.json").loadFileAsString() == unchanged && !folder.getChildFile("previous.json").exists(),
          "unchanged snapshot is idempotent and keeps its previous-generation budget");
    check(RecoveryStore::scan(root).empty() && !RecoveryStore::claim(folder, error), "a running window cannot be offered or claimed as crashed");
    p.name = "Latest unsaved mix"; const auto second = juce::JSON::toString(projectToJson(p));
    check(owner->write(second, {}).wasOk() && owner->read().document == second && folder.getChildFile("previous.json").existsAsFile(), "second snapshot preserves the previous valid generation");
    const auto blocked = RecoveryStore::create(root, error); blocked->directory().getChildFile("current.json").createDirectory();
    check(blocked->write(first, {}).failed() && blocked->directory().getChildFile("current.json").isDirectory(), "unwritable recovery destination reports failure and preserves the obstruction");
    const auto before = folder.getChildFile("current.json").loadFileAsString();
    check(owner->write("invalid JSON", {}).failed() && folder.getChildFile("current.json").loadFileAsString() == before, "invalid session cannot replace a durable recovery");
    folder.getChildFile("current.json").replaceWithText("interrupted write");
    const auto fallback = owner->read();
    check(fallback.error.isEmpty() && fallback.previous && fallback.document == first, "damaged latest generation falls back to the previous validated session");
    owner.reset(); auto candidates = RecoveryStore::scan(root);
    check(candidates.size() == 1 && candidates[0].previous, "released process lease exposes its recoverable previous generation");
    owner = RecoveryStore::claim(folder, error);
    check(owner && owner->discard("stale fingerprint").failed() && folder.getChildFile("current.json").exists(), "stale recovery selection cannot discard newer data");
    check(owner && owner->discard(fallback.fingerprint).wasOk() && !folder.getChildFile("current.json").exists() && !folder.getChildFile("previous.json").exists(), "explicit discard removes only the two owned generations");
    folder.getChildFile("keep.txt").replaceWithText("foreign local note");
    check(owner->discard().wasOk() && folder.getChildFile("keep.txt").loadFileAsString() == "foreign local note", "recovery cleanup preserves unrelated files inside its folder");
    owner.reset();
    const auto foreign = root.getChildFile(juce::Uuid().toString()); foreign.createDirectory(); foreign.getChildFile("current.json").replaceWithText(first);
    check(!RecoveryStore::claim(foreign, error) && RecoveryStore::scan(root).empty() && foreign.getChildFile("current.json").exists(), "a UUID folder without an owner record remains foreign and untouched");

    // Tiny source-bound current-format arrangement and take. Playback-purpose
    // loading computes identity without detector/model or device work.
    const auto original = root.getChildFile("source.wav"), moved = root.getChildFile("located.wav"), wrong = root.getChildFile("different.wav");
    auto writeAudio = [&](const juce::File& file, float value)
    {
        juce::AudioBuffer<float> audio(2, 1600);
        for (int i = 0; i < 1600; ++i) { audio.setSample(0, i, value); audio.setSample(1, i, -value); }
        juce::WavAudioFormat format; std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
        auto writer = format.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(2).withBitsPerSample(24));
        return writer && writer->writeFromAudioSampleBuffer(audio, 0, 1600);
    };
    check(writeAudio(original, .125f) && writeAudio(wrong, .25f), "write two dimension-matched, independently tagged tiny relink sources");
    Engine source; auto loaded = readMedia(source.formats, original, {}, AnalysisCacheMode::readWrite, MediaReadPurpose::playback);
    check(loaded.media && loaded.media->analysisSource.sha256.length() == 64, "playback preparation binds the fixture to its actual source hash");
    if (!loaded.media) return;
    source.library = {loaded.media}; source.project.masterDb = 0; Clip sourceClip; sourceClip.media = loaded.media; sourceClip.length = .2;
    source.project.clips = {sourceClip};
    const auto snapshot = source.captureProject();
    PerformanceTake take; take.name = "Relocated take"; take.initial = snapshot.clone(); take.sampleRate = 8000; take.frames = 800; take.state = TakeState::complete;
    take.masterWav = root.getChildFile("export-kept.wav").getFullPathName();
    TakeEvent event; event.kind = TakeEventKind::project; event.frame = 64; event.sequence = 1; event.payload = snapshot.clone(); take.events.push_back(event);
    source.project.performanceTakes = {take};
    const auto saved = juce::JSON::toString(source.captureProject(), false, 17);
    const auto savedFile = root.getChildFile("missing-source.mixworks");
    check(savedFile.replaceWithText(saved) && original.moveFileTo(moved), "save current arrangement and take before moving its source");
    const auto failed = source.prepareProject(saved);
    check(failed.failedSource && failed.error.contains(original.getFullPathName()), "missing required source identifies the owned file before relinking");
    const auto wrongDocument = relinkProjectSource(failed.parsed, loaded.media->id, wrong);
    const auto correctDocument = relinkProjectSource(failed.parsed, loaded.media->id, moved);
    const auto keptSource = juce::JSON::toString(source.captureProject());
    check(source.restoreProject(wrongDocument).contains("source changed") && juce::JSON::toString(source.captureProject()) == keptSource,
        "Locate refuses different audio even with identical rate, channels and length, preserving the active document");
    const auto relocated = juce::JSON::parse(correctDocument);
    auto bound = [&](const juce::var& document)
    {
        return document["media"][0]["file"].toString() == moved.getFullPathName()
            && document["sourceBindings"][0]["path"].toString() == moved.getFullPathName()
            && document["sourceBindings"][0]["sha256"].toString() == loaded.media->analysisSource.sha256;
    };
    check(bound(relocated) && bound(relocated["performanceTakes"][0]["initial"])
        && bound(relocated["performanceTakes"][0]["events"][0]["payload"]), "relink keeps source identity in the arrangement, take initial state and later project event");
    check(relocated["performanceTakes"][0]["masterWav"].toString() == take.masterWav
        && juce::JSON::toString(failed.parsed.document, false, 17) == saved, "relink leaves the failed document and exported-take destination unchanged");
    check(source.restoreProject(correctDocument).isEmpty() && source.playbackReady(), "exact moved source reopens through normal transactional admission");
    Engine replay;
    check(replay.restoreProject(juce::JSON::toString(relocated["performanceTakes"][0]["initial"])).isEmpty()
        && replay.restoreProject(juce::JSON::toString(relocated["performanceTakes"][0]["events"][0]["payload"])).isEmpty(), "relocated take initial and later snapshots independently prepare without the old path");
    replay.prepareRender(8000, 64); juce::AudioBuffer<float> pcm(2, 64); juce::MidiBuffer midi; replay.render(pcm, 64, 0, midi);
    bool exact = true; for (int i = 0; i < 64; ++i) exact = exact && std::abs(pcm.getSample(0, i) - .125f) < 1.e-6f && std::abs(pcm.getSample(1, i) + .125f) < 1.e-6f;
    check(exact, "relocated saved-take snapshot renders the independent stereo PCM oracle");

    Workstation ui(false); ui.stopTimer(); ui.recoveryRoot = root; ui.recoveryOffered = true;
    ui.setSize(1250, 800);
    auto capture = [&](juce::Component* component, const juce::String& name)
    {
        auto stream = report.getSiblingFile(name + ".png").createOutputStream(); juce::PNGImageFormat png;
        check(component && stream && png.writeImageToStream(component->createComponentSnapshot(component->getLocalBounds()), *stream), "capture owned " + name);
    };
    auto pump = []
    {
        const auto until = juce::Time::getMillisecondCounterHiRes() + 25;
        do
        {
           #if JUCE_WINDOWS
            MSG message; while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
           #endif
            juce::Thread::sleep(1);
        } while (juce::Time::getMillisecondCounterHiRes() < until);
    };
    auto await = [&](const std::function<bool()>& ready, const juce::String& name)
    {
        const auto until = juce::Time::getMillisecondCounterHiRes() + 5000;
        while (!ready() && juce::Time::getMillisecondCounterHiRes() < until) pump();
        check(ready(), name); return ready();
    };
    auto complete = [&](int answer)
    {
        auto* modal = juce::Component::getCurrentlyModalComponent(); check(modal != nullptr, "owned open/relink dialog exists before completion");
        if (modal) modal->exitModalState(answer); pump();
    };
    std::function<juce::FileBrowserComponent*(juce::Component*)> browser = [&](juce::Component* component) -> juce::FileBrowserComponent*
    {
        if (auto* b = dynamic_cast<juce::FileBrowserComponent*>(component)) return b;
        if (component) for (auto* child : component->getChildren()) if (auto* b = browser(child)) return b;
        return nullptr;
    };
    const auto kept = juce::JSON::toString(ui.engine.captureProject());
    const auto currentSession = ui.sessionFile; const auto historySize = ui.undoHistory.size();
    auto unchangedMix = [&]
    {
        return juce::JSON::toString(ui.engine.captureProject()) == kept && !ui.dirty && ui.sessionFile == currentSession
            && ui.undoHistory.size() == historySize && savedFile.loadFileAsString() == saved;
    };
    auto openMissing = [&]
    {
        ui.openProjectFile(savedFile);
        return await([&] { return juce::Component::getCurrentlyModalComponent() != nullptr; }, "missing-source preparation reaches an explicit native decision");
    };
    if (openMissing())
    {
        capture(juce::Component::getCurrentlyModalComponent(), "missing-source-choice"); complete(0);
        check(!ui.busy && unchangedMix(), "Cancel missing-source decision preserves current mix, file, dirty state and Undo");
    }
    if (openMissing())
    {
        complete(1); check(browser(juce::Component::getCurrentlyModalComponent()) != nullptr, "Locate opens the real source chooser"); complete(0);
        check(!ui.busy && unchangedMix(), "Cancel Locate keeps the current mix and original saved document");
    }
    auto locate = [&](const juce::File& file)
    {
        if (!openMissing()) return;
        complete(1); auto* b = browser(juce::Component::getCurrentlyModalComponent());
        check(b != nullptr, "Locate chooser exposes the owned filename field");
        if (b) { b->setRoot(file.getParentDirectory()); b->setFileName(file.getFileName()); }
        complete(1);
        await([&] { return !ui.busy; }, "relink preparation completes within its functional deadline");
    };
    locate(wrong);
    capture(&ui, "relink-identity-error");
    check(unchangedMix() && ui.getProperties()["lastMessageDetails"].toString().contains("source changed")
        && ui.getProperties()["lastMessageDetails"].toString().contains(wrong.getFullPathName())
        && ui.status.contains("original file") && ui.status.contains("again") && !ui.status.contains(wrong.getFullPathName()),
        "native wrong-source retry shows the recovery action, retains full path details and preserves the mix");
    locate(moved);
    check(ui.engine.project.clips.size() == 1 && ui.engine.project.clips[0].media->file == moved && ui.dirty
        && ui.sessionFile == savedFile && ui.status.contains("Save to keep the new file location"), "native exact-source retry opens successfully and requires explicit save");
    check(savedFile.loadFileAsString() == saved, "successful Locate does not overwrite the saved arrangement automatically");
    const auto relocatedFile = root.getChildFile("relocated.mixworks");
    check(ui.saveSessionFile(relocatedFile) && !ui.dirty, "Save As durably keeps the relocated arrangement and take");
    Engine reopened; const auto reopenedError = reopened.restoreProject(relocatedFile.loadFileAsString());
    check(reopenedError.isEmpty() && reopened.project.clips.size() == 1 && reopened.project.performanceTakes.size() == 1
        && bound(reopened.project.performanceTakes[0].initial), "independent saved-file reopen retains relocated take identity");
    await([&] { return ui.recoveryJobs == 0; }, "recovery cleanup finishes before owned UI teardown");
    ui.newSession();
    ui.recoveryStore = RecoveryStore::create(root, error);
    ui.engine.project.name = "Recovered UI mix"; ui.engine.project.masterDb = -11; ui.engine.project.labels.push_back({juce::Uuid().toString(), 3.25, "Keep my cue"});
    ui.engine.project.masterAutomation.enabled = true; ui.engine.project.masterAutomation.setPoint(0, -8); ui.engine.project.masterAutomation.setPoint(10, -2);
    const auto expected = juce::JSON::toString(projectToJson(ui.engine.project));
    ui.dirty = true; ui.nextRecovery = 0;
    ui.busy = true; ui.tickRecovery();
    check(ui.recoveryJobs == 0 && !ui.recoveryStore->directory().getChildFile("current.json").exists(), "busy workflow defers recovery instead of capturing a partial project");
    ui.busy = false; ui.engine.playing = true; ui.tickRecovery();
    check(ui.recoveryJobs == 0, "playing defers plugin state capture instead of blocking the audio callback");
    ui.engine.playing = false; ui.tickRecovery(); check(ui.recoveryJobs == 1, "periodic dirty-mix tick starts an asynchronous durable snapshot");
    struct Poll : juce::Timer { std::function<void()> tick; void timerCallback() override { tick(); } } poll;
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + 20000; int phase = 0;
    poll.tick = [&]
    {
        if (juce::Time::getMillisecondCounterHiRes() > deadline) { check(false, "recovery workflow finishes within deadline"); poll.stopTimer(); juce::MessageManager::getInstance()->stopDispatchLoop(); return; }
        if (ui.recoveryJobs > 0 || ui.busy) return;
        if (phase == 0)
        {
            check(ui.recoveryStore->read().error.isEmpty(), "background snapshot completes a validated session record");
            ui.recoveryStore.reset(); // Emulate lease release after abnormal exit; do not run clean-close cleanup.
            ui.engine.project = Project(); ui.engine.project.name = "Current mix"; ui.dirty = false;
            ui.offerRecovery(); auto* dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
            check(dialog != nullptr, "closed-window snapshot opens the explicit Restore / Discard / Later dialog: " + ui.status);
            capture(dialog, "recovery-choice");
            if (dialog) dialog->exitModalState(0); phase = 1;
        }
        else if (phase == 1)
        {
            if (ui.recoveryPromptOpen) return;
            check(ui.engine.project.name == "Current mix" && !ui.dirty && RecoveryStore::scan(root).size() == 1, "Later preserves both the current mix and durable recovery");
            ui.recoveryStore = RecoveryStore::create(root, error); ui.offerRecovery();
            if (auto* dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent())) dialog->exitModalState(1);
            else check(false, "Restore dialog reopens"); phase = 2;
        }
        else if (phase == 2)
        {
            if (ui.recoveryPromptOpen || ui.engine.project.name != "Recovered UI mix") return;
            check(ui.dirty && ui.sessionFile == juce::File() && juce::JSON::toString(projectToJson(ui.engine.project)) == expected,
                  "native Restore transaction recovers exact labels, master gain and automation as an unsaved mix");
            check(ui.restoredRecovery != nullptr, "source recovery lease is retained until a save or durable successor");
            ui.nextRecovery = 0; ui.tickRecovery(); phase = 3;
        }
        else if (phase == 3)
        {
            check(ui.recoveryStore->read().error.isEmpty() && !ui.restoredRecovery && RecoveryStore::scan(root).empty(), "durable successor releases the old recovery without duplicate offers");
            ui.engine.project.name = "Keep current mix"; ui.dirty = false;
            ui.openProjectDocument(expected, {}, true);
            check(ui.importCancellation != nullptr, "recovery load exposes its normal Cancel operation");
            ui.importProgress.cancel.onClick(); phase = 4;
        }
        else if (phase == 4)
        {
            if (auto* dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent())) dialog->exitModalState(0);
            check(ui.engine.project.name == "Keep current mix" && !ui.dirty && ui.recoveryStore->read().error.isEmpty(),
                  "Cancel during recovery preparation preserves current mix and durable snapshot");
            Engine missing; auto media = std::make_shared<Media>(); media->file = root.getChildFile("unavailable.wav"); media->sampleRate = 8000; media->samples.setSize(1, 8000);
            Clip clip; clip.media = media; clip.length = 1; missing.project.clips = {clip};
            const auto staged = ui.engine.prepareProject(juce::JSON::toString(missing.captureProject()));
            check(staged.error.isNotEmpty() && ui.engine.project.name == "Keep current mix", "missing audio in a recovery rejects preparation without replacing the current project");
            ui.clearRecovery(); phase = 5;
        }
        else
        {
            check(ui.recoveryStore->read().document.isEmpty() && !ui.recoveryStore->directory().getChildFile("current.json").exists(), "successful save/discard cleanup removes this window's recovery generations");
            poll.stopTimer(); juce::MessageManager::getInstance()->stopDispatchLoop();
        }
    };
    poll.startTimer(20); juce::MessageManager::getInstance()->runDispatchLoop();


}
}
