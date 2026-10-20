#include "Workstation.h"
#include "AutoArrangement.h"
#include "Beatmatch.h"
#include <cmath>
#include <climits>
#include <set>

namespace mixworks
{
bool Workstation::canAutoArrange() const
{
    return !busy && !engine.playing && !engine.exporting && !timelineLoads.size() && pendingTempo <= 0
        && !tempoPreparationPending && !engine.project.hasPendingPlacement() && !engine.project.hasLockedTransitions()
        && !unsavedPromptOpen && !choosingSave;
}
void Workstation::showAutoArrangement()
{
    if (!canAutoArrange()) { setStatus("Pause playback, unlock transitions and wait for audio preparation before Auto Arrange."); return; }
    juce::StringArray titles {"Random starting track"}; std::vector<juce::String> ids {juce::String()};
    std::vector<std::shared_ptr<Media>> choices;
    for (const auto& m : engine.library) if (m && autoArrangementEligible(*m)) choices.push_back(m);
    std::sort(choices.begin(), choices.end(), [](const auto& a, const auto& b) { return a->libraryTitle().compareNatural(b->libraryTitle()) < 0; });
    for (const auto& m : choices)
    { ids.push_back(m->id); titles.add(m->libraryTitle() + " / " + juce::String(m->bpm(), 1) + " BPM / " + keyCamelot(m->key())); }
    if (choices.empty()) { setStatus("Analyse or correct BPM, downbeat and key in the library before Auto Arrange."); return; }
    auto* dialog = new juce::AlertWindow("Auto Arrange",
        "Match tracks and blend up to 32 bars. No repeats.\nReplaces this mix when ready. Undo brings it back.", juce::MessageBoxIconType::NoIcon, this);
    dialog->addComboBox("start", titles, "First track");
    dialog->addComboBox("mode", {"Number of tracks", "Total length (minutes)"}, "Arrange by");
    dialog->addTextEditor("amount", juce::String(std::min(10, (int) choices.size())), "Tracks or minutes");
    dialog->addComboBox("transitions", {"Balanced", "Smooth", "Bold", "Use my settings", "Constant power", "Hard cuts only"}, "Blend style");
    dialog->getComboBoxComponent("transitions")->setTooltip("Balanced/Bold replace the default volume fade with bass/filter treatment. Smooth uses a linear crossfade. Constant power avoids the usual halfway power dip with similarly loud, unrelated tracks. Hard cuts switch at the handover midpoint with a tiny click-prevention ramp. These do not normalise different recordings. All blends stay within 32 bars.");
    dialog->addComboBox("energy", {"Unrestricted", "Keep pace", "Build energy", "Wind down"}, "Energy direction");
    dialog->getComboBoxComponent("energy")->setTooltip("Keep pace: no measured source-energy or BPM drop. Build: at least +1 dB energy without a BPM drop. Wind down: at least -1 dB without a BPM rise. Both halves of the actual overlap must qualify. These use source RMS, not perceived loudness or a dance-energy score. Unknown energy is skipped for these choices; the mix may stop early.");
    dialog->getTextEditor("amount")->setInputRestrictions(7, "0123456789.");
    dialog->getComboBoxComponent("mode")->onChange = [dialog, previousMode = dialog->getComboBoxComponent("mode")->getSelectedItemIndex()]() mutable
    {
        const int mode = dialog->getComboBoxComponent("mode")->getSelectedItemIndex();
        if (mode == previousMode) return;
        previousMode = mode;
        const bool duration = mode == 1;
        dialog->getTextEditor("amount")->setText(duration ? "60" : "10");
        dialog->getTextEditor("amount")->setTooltip(duration ? "Minutes, up to 180" : "Whole number of tracks, limited by the library");
    };
    dialog->addButton("Make mix", 1, juce::KeyPress(juce::KeyPress::returnKey));
    dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    dialog->enterModalState(true, juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<Workstation>(this), dialog, ids](int answer)
    {
        if (!safe || answer != 1) return;
        const auto text = dialog->getTextEditorContents("amount").trim();
        const double amount = text.getDoubleValue();
        const bool duration = dialog->getComboBoxComponent("mode")->getSelectedItemIndex() == 1;
        if (text.isEmpty() || !text.containsOnly("0123456789.") || text.indexOfChar('.') != text.lastIndexOfChar('.')
            || !std::isfinite(amount) || amount <= 0 || (duration ? amount > 180 : amount != std::floor(amount) || amount > INT_MAX))
        { safe->setStatus("Enter a positive whole track count, or minutes up to 180. The current mix was kept."); return; }
        AutoArrangementOptions options;
        options.transitions = static_cast<AutoTransitionStyle>(dialog->getComboBoxComponent("transitions")->getSelectedItemIndex());
        options.energy = static_cast<AutoEnergyPolicy>(dialog->getComboBoxComponent("energy")->getSelectedItemIndex());
        const int start = dialog->getComboBoxComponent("start")->getSelectedItemIndex();
        if (start < 0 || start >= (int) ids.size()) return;
        options.startingId = ids[(size_t) start];
        if (duration) options.targetSeconds = amount * 60; else options.trackCount = (int) amount;
        options.preset = safe->selectedTransitionPreset(); options.overlapBeats = safe->transitionOverlapBeats();
        options.sectionTransitions = safe->transitionIndex == 5;
        safe->startAutoArrangement(options);
    }), true);
}
void Workstation::startAutoArrangement(const AutoArrangementOptions& options)
{
    if (!canAutoArrange()) { setStatus("Pause playback, unlock transitions and wait for preparation before Auto Arrange."); return; }
    auto library = engine.library;
    // Freeze catalogue facts while background verification is suspended by busy.
    for (auto& m : library) if (!m->hasPlayback())
    {
        auto snapshot = m->catalogueCopy(); snapshot->playbackError = m->playbackError; m = std::move(snapshot);
    }
    std::set<const PreparedAudio*> seen; int64_t resident = 0;
    auto count = [&](const Project& p) { for (const auto& c : p.clips) if (c.prepared && seen.insert(c.prepared.get()).second) resident += c.prepared->residentBytes(); };
    count(engine.project); for (const auto& p : undoHistory) count(p); for (const auto& p : redoHistory) count(p);
    auto cancelled = std::make_shared<std::atomic<bool>>(false); importCancellation = cancelled;
    setBusy(true); importProgress.begin(1); importProgress.setAutoArrangement();
    autoArrangeButton.setButtonText("Arranging...");
    setStatus("Choosing tracks by key and BPM. The current mix stays available until the new mix is ready.");
    workers.addJob([safe = juce::Component::SafePointer<Workstation>(this), library = std::move(library), options, cancelled, resident]() mutable
    {
        juce::Random random;
        auto result = planAutoArrangement(library, options, random, cancelled);
        juce::AudioFormatManager formats; formats.registerBasicFormats();
        int completed = 0;
        for (auto& clip : result.project.clips)
        {
            if (result.error.isNotEmpty() || *cancelled) break;
            juce::MessageManager::callAsync([safe, cancelled, completed = completed++, total = (int) result.project.clips.size(), name = clip.media->libraryTitle()]
            {
                if (!safe || safe->importCancellation != cancelled || *cancelled) return;
                safe->importProgress.setCounts(completed, total); safe->importProgress.showFile(name, MediaReadStage::decode);
            });
            if (clip.media->hasPlayback()) continue;
            const auto source = clip.media; const auto record = source->analysisRecord();
            auto loaded = readMedia(formats, source->file, {}, AnalysisCacheMode::readWrite, MediaReadPurpose::playback, &record, cancelled);
            if (!loaded.media) { result.error = loaded.error; break; }
            auto& m = *loaded.media;
            if (!(m.analysisSource == source->analysisSource))
            { result.error = "A chosen source changed. Refresh the library and try again. The current mix was kept."; break; }
            m.id = source->id; m.name = source->name; m.manualBpm = source->manualBpm;
            m.manualBeatOrigin = source->manualBeatOrigin; m.manualKey = source->manualKey;
            if (!autoArrangementEligible(m) || m.bpm() != source->bpm() || m.key() != source->key()
                || m.beatOrigin() != source->beatOrigin() || m.downbeatOrigin() != source->downbeatOrigin() || m.duration() != source->duration())
            { result.error = "Chosen track analysis changed. Refresh the library and try again. The current mix was kept."; break; }
            clip.media = std::move(loaded.media);
        }
        if (result.error.isEmpty() && !*cancelled)
        {
            juce::MessageManager::callAsync([safe, cancelled, count = (int) result.project.clips.size()]
            { if (safe && safe->importCancellation == cancelled && !*cancelled) safe->importProgress.setAutoMixPreparation(count); });
            auto prepared = prepareTempoMap(std::move(result.project), false, options.overlapBeats, resident, 0, {}, options.preset, options.sectionTransitions);
            result.project = std::move(prepared.project); result.error = prepared.error;
        }
        juce::MessageManager::callAsync([safe, cancelled, result = std::move(result)]() mutable
        {
            if (!safe || safe->importCancellation != cancelled) return;
            safe->importCancellation.reset(); safe->importProgress.setVisible(false); safe->setBusy(false);
            safe->autoArrangeButton.setButtonText("Auto Arrange");
            if (*cancelled) { safe->setStatus("Auto arrangement cancelled. The current mix was kept."); return; }
            if (result.error.isNotEmpty())
            {
                safe->setStatus("Auto arrangement: " + result.error + " The current mix was kept.");
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Auto Arrange stopped", safe->status, "OK", safe);
                return;
            }
            safe->engine.setAutomationRecordMode(AutomationRecordMode::read); safe->remember();
            {
                const juce::ScopedLock guard(safe->engine.lock);
                // Preserve the user's mixer, plugins and master settings; replace arrangement-owned data only.
                auto candidate = safe->engine.project;
                candidate.name = result.project.name; candidate.clips = std::move(result.project.clips);
                candidate.tempo = std::move(result.project.tempo); candidate.bpm = result.project.bpm;
                candidate.followTrackTempo = result.project.followTrackTempo; candidate.labels.clear();
                candidate.masterAutomation = {};
                for (auto& track : candidate.tracks)
                {
                    track.automation = {};
                    for (auto& parameter : track.pluginAutomation) parameter.curve = {};
                }
                candidate.transitionLinks = std::move(result.project.transitionLinks); candidate.transitionLinksKnown = true;
                safe->engine.project = std::move(candidate); safe->engine.tempo = safe->engine.project.bpm;
                safe->engine.position = 0; safe->engine.resetPlugins();
            }
            safe->selectedClip = safe->engine.project.clips.front().id; safe->selectedTransition.clear();
            safe->sessionFile = {}; // Save the generated mix as its own document.
            safe->timeline.selectAutomationPoint({}, -1); safe->timeline.selectedLabel.clear(); safe->tempoStrip.selectedTempoPoint = -1;
            safe->refresh(); safe->menuItemSelected(22, 0); safe->setStatus(result.summary);
            safe->attractState.motion = AttractMotion::animated;
            safe->setAttractMode(true);
            if (!safe->layout.forgeEffects) safe->menuItemSelected(81, 0);
            safe->menuItemSelected(83, 0);
            safe->autoArrangeButton.setTooltip(result.summary);
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "Auto Arrange complete", result.summary, "OK", safe);
        });
    });
}
}
