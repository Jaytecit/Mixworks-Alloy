#include "Workstation.h"

namespace mixworks
{
void Workstation::clearRecovery()
{
    auto own = recoveryStore, restored = restoredRecovery; const auto fingerprint = restoredRecoveryFingerprint;
    if (!own && !restored) return;
    ++recoveryJobs;
    recoveryWorkers.addJob([safe = juce::Component::SafePointer<Workstation>(this), own, restored, fingerprint]
    {
        auto result = own ? own->discard() : juce::Result::ok();
        if (result.wasOk() && restored) result = restored->discard(fingerprint);
        juce::MessageManager::callAsync([safe, own, restored, result]
        {
            if (!safe) return;
            --safe->recoveryJobs;
            if (result.failed()) safe->setStatus(result.getErrorMessage());
            else if (safe->restoredRecovery == restored) { safe->restoredRecovery.reset(); safe->restoredRecoveryFingerprint.clear(); }
        });
    });
}
void Workstation::tickRecovery()
{
    if (recoveryRoot == juce::File()) return;
    if (!recoveryOffered && !busy && !unsavedPromptOpen && !choosingSave && !juce::Component::getCurrentlyModalComponent())
    { recoveryOffered = true; offerRecovery(); }
    const auto now = juce::Time::getMillisecondCounterHiRes();
    if (!recoveryStore || recoveryJobs > 0 || !dirty || now < nextRecovery || busy || recoveryPromptOpen || unsavedPromptOpen || choosingSave
        || engine.playing || engine.exporting || engine.recordingAutomation() || engine.rehearsalActive() || !timelineLoads.empty()
        || pendingTempo > 0 || tempoPreparationPending || !engine.playbackReady()) return;
    nextRecovery = now + 30000;
    // Snapshot only while paused: a plugin's state serializer may block, and
    // captureProject locks the shared renderer. Never inject periodic dropouts.
    // Plugin state is captured on the message thread, never in the audio callback.
    // Only immutable session text is passed to the disk worker.
    const auto document = juce::JSON::toString(engine.captureProject(), false, 17); const auto file = sessionFile;
    auto own = recoveryStore, restored = restoredRecovery; const auto fingerprint = restoredRecoveryFingerprint;
    ++recoveryJobs;
    recoveryWorkers.addJob([safe = juce::Component::SafePointer<Workstation>(this), document, file, own, restored, fingerprint]
    {
        auto result = own->write(document, file);
        if (result.wasOk() && restored) result = restored->discard(fingerprint);
        juce::MessageManager::callAsync([safe, own, restored, result]
        {
            if (!safe) return;
            --safe->recoveryJobs;
            if (result.failed()) safe->setStatus("Recovery: " + result.getErrorMessage());
            else if (safe->restoredRecovery == restored) { safe->restoredRecovery.reset(); safe->restoredRecoveryFingerprint.clear(); }
        });
    });
}
void Workstation::offerRecovery()
{
    if (busy || recoveryPromptOpen || unsavedPromptOpen || choosingSave || recoveryRoot == juce::File()) return;
    const auto candidates = RecoveryStore::scan(recoveryRoot);
    if (candidates.empty()) { setStatus("No unsaved recovery snapshots from closed windows."); return; }
    const auto candidate = candidates.front(); juce::String error;
    std::shared_ptr<RecoveryStore> store = RecoveryStore::claim(candidate.folder, error);
    if (!store) { setStatus(error); return; }
    if (candidate.error.isNotEmpty()) { setStatus("Recovery retained: " + candidate.error); return; }
    recoveryPromptOpen = true;
    const auto name = candidate.sessionFile == juce::File() ? juce::String("Untitled mix") : candidate.sessionFile.getFileName();
    auto* dialog = new juce::AlertWindow("Recover unsaved mix",
        name + "\nSaved: " + candidate.timestamp + (candidate.previous ? "\nUsing the previous backup." : "")
        + "\nBring this mix back?",
        juce::MessageBoxIconType::QuestionIcon, this);
    dialog->addButton("Restore", 1); dialog->addButton("Discard recovery", 2); dialog->addButton("Later", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    dialog->enterModalState(true,
        juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<Workstation>(this), store, candidate](int result)
        {
            if (!safe) return; safe->recoveryPromptOpen = false;
            if (result == 2) { const auto removed = store->discard(candidate.fingerprint); safe->setStatus(removed.wasOk() ? "Recovery snapshots discarded." : removed.getErrorMessage()); }
            else if (result == 1) safe->guardUnsaved([safe, store, candidate]
            {
                if (!safe) return;
                const auto current = store->read();
                if (current.fingerprint != candidate.fingerprint || current.error.isNotEmpty()) { safe->setStatus("Recovery changed or became unavailable. The current mix was kept."); return; }
                safe->openProjectDocument(current.document, current.sessionFile, true, store, current.fingerprint);
            });
        }), true);
}
}

