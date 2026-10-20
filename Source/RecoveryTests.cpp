#include "Workstation.h"

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

    Workstation ui(false); ui.stopTimer(); ui.recoveryRoot = root; ui.recoveryOffered = true;
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
