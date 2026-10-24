#include "Workstation.h"
#include "ArrangementInsertion.h"
#include <set>

namespace mixworks
{
bool Workstation::canInsertArrangement() const
{
    return !busy && !engine.playing && !engine.exporting && !engine.rehearsalActive()
        && pendingTempo <= 0 && !tempoPreparationPending && timelineLoads.empty()
        && !engine.project.hasPendingPlacement() && engine.automationRecordMode == AutomationRecordMode::read
        && !engine.recordingAutomation();
}
void Workstation::chooseArrangementInsertion(const juce::String& anchor, bool after)
{
    const auto* destination = engine.project.clipFor(anchor);
    if (!destination || !canInsertArrangement()) { setStatus("Stop playback and finish preparation/recording before inserting into an arrangement."); return; }
    if (engine.library.empty()) { setStatus("Import the new song into Library before inserting it."); return; }
    const auto choices = engine.library;
    auto* dialog = new juce::AlertWindow(after ? "Insert song after" : "Insert song before",
        destination->media->libraryTitle() + "\nArrangement only. Choose the new source; the next step previews independent placement and later movement.", juce::MessageBoxIconType::NoIcon, this);
    juce::StringArray names;
    for (const auto& m : choices) names.add(m->libraryTitle() + " / " + juce::String(m->bpm(), 2) + " BPM / " + m->file.getFileName());
    dialog->addComboBox("source", names, "New song from Library");
    for (size_t n = 0; n < choices.size(); ++n) if (choices[n]->id == selectedLibraryId) dialog->getComboBoxComponent("source")->setSelectedItemIndex((int) n);
    dialog->addButton("Preview insertion", 1, juce::KeyPress(juce::KeyPress::returnKey)); dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    dialog->enterModalState(true, juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<Workstation>(this), dialog, choices, anchor, after](int answer)
    {
        if (!safe || answer != 1) return;
        const int choice = dialog->getComboBoxComponent("source")->getSelectedItemIndex();
        if (choice >= 0 && choice < (int) choices.size()) safe->prepareChosenInsertion(choices[(size_t) choice], anchor, after);
    }), true);
}
void Workstation::prepareChosenInsertion(std::shared_ptr<Media> source, const juce::String& anchor, bool after)
{
    if (!canInsertArrangement() || !source || !engine.project.clipFor(anchor)) { setStatus("The insertion destination changed or the arrangement is busy."); return; }
    engine.flushMidiControls();
    if (!source->hasPlayback()) source = source->catalogueCopy();
    const auto baseline = engine.project;
    const auto before = juce::JSON::toString(projectToJson(baseline), true, 17);
    const auto preset = selectedTransitionPreset(); const double overlap = transitionOverlapBeats();
    std::set<const PreparedAudio*> seen; int64_t resident = 0;
    auto count = [&](const Project& p) { for (const auto& c : p.clips) { if (c.prepared && seen.insert(c.prepared.get()).second) resident += c.prepared->residentBytes(); for (const auto& stem : c.stemPrepared) if (stem && seen.insert(stem.get()).second) resident += stem->residentBytes(); } };
    count(baseline); for (const auto& p : undoHistory) count(p); for (const auto& p : redoHistory) count(p);
    const auto cancelled = std::make_shared<std::atomic<bool>>(false); importCancellation = cancelled;
    setBusy(true); importProgress.begin(1); importProgress.setAutoArrangement(); importProgress.cancel.setButtonText("Cancel insertion"); importProgress.setVisible(true);
    setStatus("Preparing the new song and insertion preview. The current arrangement is kept until Apply.");
    workers.addJob([safe = juce::Component::SafePointer<Workstation>(this), baseline, source, anchor, after, before, preset, overlap, resident, cancelled]() mutable
    {
        juce::String error;
        if (source->hasPlayback() && source->analysisSource.sha256.isNotEmpty()
            && (!source->file.existsAsFile() || audioDigest(source->file) != source->analysisSource.sha256))
            error = "The chosen source file changed or is missing. Refresh Library before inserting it.";
        if (error.isEmpty() && !source->hasPlayback())
        {
            juce::AudioFormatManager formats; formats.registerBasicFormats(); const auto record = source->analysisRecord();
            auto loaded = readMedia(formats, source->file, {}, AnalysisCacheMode::readWrite, MediaReadPurpose::playback, &record, cancelled);
            if (!loaded.media) error = loaded.error;
            else if (!record.source.sha256.isEmpty() && !(loaded.media->analysisSource == record.source)) error = "The chosen source changed. Refresh Library before inserting it.";
            else { loaded.media->id = source->id; source = std::move(loaded.media); }
        }
        ArrangementInsertion proposal;
        if (error.isEmpty() && !*cancelled)
        {
            proposal = planArrangementInsertion(baseline, source, anchor, after, overlap, preset);
            error = proposal.error;
            if (error.isEmpty()) error = prepareArrangementInsertion(proposal, resident);
        }
        juce::MessageManager::callAsync([safe, cancelled, source, before, error, proposal = std::move(proposal)]() mutable
        {
            if (!safe || safe->importCancellation != cancelled) return;
            safe->importCancellation.reset(); safe->importProgress.setVisible(false); safe->setBusy(false);
            if (*cancelled) { safe->setStatus("Insertion cancelled. The current mix was kept."); return; }
            if (error.isNotEmpty()) { safe->setStatus("Insertion unavailable: " + error); return; }
            if (before != juce::JSON::toString(projectToJson(safe->engine.project), true, 17) || !safe->canInsertArrangement())
            { safe->setStatus("The arrangement changed during preparation. Open insertion again."); return; }
            safe->previewArrangementInsertion(std::move(proposal), before, source);
        });
    });
}
void Workstation::previewArrangementInsertion(ArrangementInsertion proposal, const juce::String& before, std::shared_ptr<Media> source)
{
    const auto* inserted = proposal.project.clipFor(proposal.inserted); if (!inserted) return;
    const auto origin = proposal.project.tempo.origin;
    const auto title = [&](const juce::String& id) { const auto* c = proposal.project.clipFor(id); return c ? c->media->libraryTitle() + " [" + timeText(c->start - origin) + "]" : juce::String("Mix boundary"); };
    juce::String message = "Before: " + title(proposal.outgoing) + "\nInsert: " + title(proposal.inserted) + "\nAfter: " + title(proposal.incoming)
        + "\n\nNew segment: " + timeText(inserted->start - origin) + " - " + timeText(inserted->end() - origin) + " / lane " + juce::String(inserted->track + 1)
        + "\n" + juce::String(proposal.moved) + " later segments move by " + juce::String(proposal.displacement, 3) + " seconds across all lanes."
        + "\nLater transitions, source edits, labels and session automation move together. The new track is independent; use Add Transition for joins."
        + "\nNew song uses the mix tempo; source 0 - " + juce::String(proposal.sourceEnd, 3) + " s. Tail omitted for bar phase: " + juce::String(std::max(0., source->duration() - proposal.sourceEnd), 3) + " s."
        + "\nNo new transitions are created. Apply creates one Undo and shows Playlist in timeline order.";
    for (const auto& incoming : {proposal.inserted, proposal.incoming})
        if (const auto* link = proposal.project.transitionFor(incoming)) { const auto range = proposal.project.transitionRange(*link); message += "\nJoin into " + title(incoming) + ": " + timeText(range.getStart() - origin) + " - " + timeText(range.getEnd() - origin); }
    auto* dialog = new juce::AlertWindow("Preview song insertion", message, juce::MessageBoxIconType::NoIcon, this);
    dialog->addButton("Apply insertion", 1, juce::KeyPress(juce::KeyPress::returnKey)); dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    dialog->enterModalState(true, juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<Workstation>(this), proposal = std::move(proposal), before, source](int answer) mutable
    {
        if (!safe || answer != 1) return;
        safe->commitArrangementInsertion(std::move(proposal), before, source);
    }), true);
}
bool Workstation::commitArrangementInsertion(ArrangementInsertion proposal, const juce::String& before, std::shared_ptr<Media> source)
{
    if (!canInsertArrangement() || before != juce::JSON::toString(projectToJson(engine.project), true, 17) || proposal.error.isNotEmpty())
    { setStatus("The arrangement changed or is protected. Open insertion again; the current mix was kept."); return false; }
    const auto admitted = parseProject(juce::JSON::toString(projectToJson(proposal.project), true, 17));
    if (admitted.error.isNotEmpty()) { setStatus("Insertion could not be saved safely: " + admitted.error); return false; }
    const auto protection = engine.project.validateTransitionLocks(proposal.project);
    if (protection.failed()) { setStatus(protection.getErrorMessage()); return false; }
    const auto* clip = proposal.project.clipFor(proposal.inserted); if (!clip || !source || clip->media != source) return false;
    if (!source->file.existsAsFile() || (source->analysisSource.bytes > 0 && source->file.getSize() != source->analysisSource.bytes)
        || (source->sourceModified > 0 && source->file.getLastModificationTime().toMilliseconds() != source->sourceModified))
    { setStatus("The new source file changed after preparation. Open insertion again; the current mix was kept."); return false; }
    const int lane = clip->track; const double start = clip->start;
    remember();
    {
        const juce::ScopedLock guard(engine.lock); engine.project = std::move(proposal.project); engine.tempo = engine.project.tempo.bpmAt(start);
        const auto existing = std::find_if(engine.library.begin(), engine.library.end(), [&](const auto& m) { return m->id == source->id; });
        if (existing == engine.library.end()) engine.library.push_back(source); else *existing = source;
        engine.resetPlugins();
    }
    selectedClip = proposal.inserted; selectedTransition.clear(); correctionFromLibrary = false; timeline.selectedLabel.clear(); timeline.selectedSection.reset();
    playlistTable.table.getHeader().setSortColumnId(3, true);
    markCatalogueChanged(); selectTrack(lane); refresh(); setFollowPlayhead(false); viewport.setViewPosition(std::max(0, Clip::gridPixel(start, timeline.pixelsPerSecond) - 70), viewport.getViewPositionY());
    setStatus("Song inserted. Later segments and their transitions moved together. Undo restores the previous mix."); return true;
}
}
