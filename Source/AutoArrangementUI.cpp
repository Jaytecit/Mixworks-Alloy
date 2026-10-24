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
    juce::StringArray titles {"Automatic starting song"}; std::vector<juce::String> ids {juce::String()};
    std::vector<std::shared_ptr<Media>> choices;
    for (const auto& m : engine.library) if (m && autoArrangementEligible(*m)) choices.push_back(m);
    std::sort(choices.begin(), choices.end(), [](const auto& a, const auto& b) { return a->libraryTitle().compareNatural(b->libraryTitle()) < 0; });
    for (const auto& m : choices)
    { ids.push_back(m->id); titles.add(m->libraryTitle() + " / " + juce::String(m->bpm(), 1) + " BPM / " + keyCamelot(m->key())); }
    if (choices.empty()) { setStatus("Analyse or correct BPM, downbeat and key in the library before Auto Arrange."); return; }
    auto* dialog = new juce::AlertWindow("Auto Arrange",
        "Replace the current mix?", juce::MessageBoxIconType::NoIcon, this);
    dialog->addComboBox("action", {"Create new arrangement", "Extend current arrangement"}, "Action");
    dialog->getComboBoxComponent("action")->setItemEnabled(2,!engine.project.clips.empty());
    dialog->addComboBox("start", titles, "First song");
    dialog->addComboBox("mode", {"Number of songs", "Total length (minutes)"}, "Arrange by");
    dialog->addTextEditor("amount", juce::String(std::min(10, (int) choices.size())), "Songs or minutes");
    dialog->addComboBox("transitions", {"Balanced", "Smooth", "Bold", "Use my settings", "Constant power", "Hard cuts only", "DJ set (varied)"}, "Blend style");
    dialog->getComboBoxComponent("transitions")->setTooltip("Balanced/Bold use bass/filter treatment at a shared section change. Smooth uses a linear crossfade. Constant power uses an equal-power envelope; it does not normalise recordings. Hard cuts use a short click-prevention ramp. DJ set varies supported phrase, bass and restrained filter handovers using musical activity and recent choices. A genuine section match may need more than 32 bars.");
    dialog->addComboBox("energy", {"Unrestricted", "Keep pace", "Build energy", "Wind down"}, "Energy direction");
    dialog->getComboBoxComponent("energy")->setTooltip("Keep pace favours steady musical activity and tempo. Build energy favours a gradual rise; Wind down favours a descent. Uses rhythm, rhythmic bass and transient dynamics, plus tempo. Missing profiles are analysed before choosing songs; unknown activity remains usable and is reported. Plateaus and necessary reversals are allowed. This is an activity estimate, not a calibrated taste or danceability model. No automatic gain changes.");
    juce::StringArray genres {"Any genre"}; std::set<juce::String> genreKeys;
    for (const auto& media : choices)
    {
        const auto key = normalizedAutoGenre(media->assets.genre);
        if (key.isNotEmpty() && genreKeys.insert(key).second) genres.add(key);
    }
    if (genres.size() > 1) { const auto first = genres[0]; genres.remove(0); genres.sortNatural(); genres.insert(0, first); }
    dialog->addComboBox("genre", genres, "Preferred genre");
    dialog->getComboBoxComponent("genre")->setEditableText(true);
    dialog->getComboBoxComponent("genre")->setTooltip("Choose a tag or type a genre. This is a preference, not a filter: better musical/timing matches, other genres and unlabelled tracks remain usable. Original tags are kept; no genre is guessed from audio.");
    dialog->getTextEditor("amount")->setInputRestrictions(7, "0123456789.");
    dialog->getComboBoxComponent("mode")->onChange = [dialog, previousMode = dialog->getComboBoxComponent("mode")->getSelectedItemIndex()]() mutable
    {
        const int mode = dialog->getComboBoxComponent("mode")->getSelectedItemIndex();
        if (mode == previousMode) return;
        previousMode = mode;
        const bool duration = mode == 1;
        dialog->getTextEditor("amount")->setText(duration ? "60" : "10");
        dialog->getTextEditor("amount")->setTooltip(duration ? "Minutes, up to 180" : "Whole number of songs, limited by the library");
    };
    dialog->getComboBoxComponent("start")->setTooltip("Choose the first song, or Automatic: random for Unrestricted; otherwise starts with room for the chosen progression in the largest compatible group.");
    dialog->getComboBoxComponent("mode")->setTooltip("Choose whether the target is a number of songs or a mix duration.");
    dialog->getTextEditor("amount")->setTooltip("Enter a whole number of songs, limited by the library, or choose Total length for minutes up to 180.");
    dialog->addButton("Make mix", 1, juce::KeyPress(juce::KeyPress::returnKey));
    dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    const auto before = juce::JSON::toString(projectToJson(engine.project));
    dialog->getComboBoxComponent("action")->onChange = [dialog, count = engine.project.clips.size()]
    {
        const bool extend = dialog->getComboBoxComponent("action")->getSelectedItemIndex() == 1;
        dialog->getComboBoxComponent("start")->setEnabled(!extend);
        dialog->setMessage(extend ? "Keep the existing " + juce::String((int) count)
            + " segments and add the requested songs or minutes after the last song. Existing joins stay in place; the new handover uses its tail. Songs already used are excluded. One Undo restores the mix."
            : "Replace the current mix?");
        dialog->getButton(0)->setButtonText(extend ? "Extend mix" : "Make mix");
        dialog->getTextEditor("amount")->setTooltip(extend ? "Additional songs or additional minutes. Total mix limit: 180 minutes." : "Songs or total minutes for the new mix.");
    };
    dialog->getComboBoxComponent("action")->setSelectedItemIndex(engine.project.clips.empty() ? 0 : 1, juce::dontSendNotification);
    dialog->getComboBoxComponent("action")->onChange();
    dialog->enterModalState(true, juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<Workstation>(this), dialog, ids, before](int answer)
    {
        if (!safe || answer != 1) return;
        const auto text = dialog->getTextEditorContents("amount").trim();
        const double amount = text.getDoubleValue();
        const bool duration = dialog->getComboBoxComponent("mode")->getSelectedItemIndex() == 1;
        if (text.isEmpty() || !text.containsOnly("0123456789.") || text.indexOfChar('.') != text.lastIndexOfChar('.')
            || !std::isfinite(amount) || amount <= 0 || (duration ? amount > 180 : amount != std::floor(amount) || amount > INT_MAX))
        { safe->setStatus("Enter a positive whole song count, or minutes up to 180. The current mix was kept."); return; }
        AutoArrangementOptions options;
        options.extendExisting = dialog->getComboBoxComponent("action")->getSelectedItemIndex() == 1;
        if (options.extendExisting && before != juce::JSON::toString(projectToJson(safe->engine.project)))
        { safe->setStatus("The arrangement changed. Open Auto Arrange again before extending it."); return; }
        options.transitions = static_cast<AutoTransitionStyle>(dialog->getComboBoxComponent("transitions")->getSelectedItemIndex());
        options.energy = static_cast<AutoEnergyPolicy>(dialog->getComboBoxComponent("energy")->getSelectedItemIndex());
        const auto genre = dialog->getComboBoxComponent("genre")->getText().trim();
        if (normalizedAutoGenre(genre) != "any genre") options.preferredGenre = genre;
        if (options.preferredGenre.length() > 2048) { safe->setStatus("Use a shorter genre preference. The current mix was kept."); return; }
        const int start = dialog->getComboBoxComponent("start")->getSelectedItemIndex();
        if (start < 0 || start >= (int) ids.size()) return;
        if (!options.extendExisting) options.startingId = ids[(size_t) start];
        if (duration) options.targetSeconds = amount * 60; else options.trackCount = (int) amount;
        options.preset = safe->selectedTransitionPreset(); options.overlapBeats = safe->transitionOverlapBeats();
        options.sectionTransitions = safe->transitionIndex == 5;
        safe->startAutoArrangement(options);
    }), true);
}
void Workstation::startAutoArrangement(const AutoArrangementOptions& options)
{
    if (!canAutoArrange()) { setStatus("Pause playback, unlock transitions and wait for preparation before Auto Arrange."); return; }
    if (options.extendExisting && (engine.project.clips.empty() || engine.rehearsalActive() || engine.recordingAutomation()
        || engine.automationRecordMode != AutomationRecordMode::read))
    { setStatus("Choose an existing arrangement, stop rehearsal and finish automation in Read before extending."); return; }
    const auto original = engine.project;
    const auto before = options.extendExisting ? juce::JSON::toString(projectToJson(original)) : juce::String();
    auto library = engine.library;
    // Freeze catalogue facts while background verification is suspended by busy.
    for (auto& m : library) if (!m->hasPlayback())
    {
        auto snapshot = m->catalogueCopy(); snapshot->playbackError = m->playbackError; m = std::move(snapshot);
    }
    std::set<const PreparedAudio*> seen; int64_t resident = 0;
    auto count = [&](const Project& p) { for (const auto& c : p.clips) { if (c.prepared && seen.insert(c.prepared.get()).second) resident += c.prepared->residentBytes(); for (const auto& stem : c.stemPrepared) if (stem && seen.insert(stem.get()).second) resident += stem->residentBytes(); } };
    count(engine.project); for (const auto& p : undoHistory) count(p); for (const auto& p : redoHistory) count(p);
    auto cancelled = std::make_shared<std::atomic<bool>>(false); importCancellation = cancelled;
    setBusy(true); importProgress.begin(1); importProgress.setAutoArrangement();
    autoArrangeButton.setButtonText("Arranging...");
    setStatus("Preparing musical energy and choosing compatible songs. The current mix stays available until the new mix is ready.");
    workers.addJob([safe = juce::Component::SafePointer<Workstation>(this), library = std::move(library), options, cancelled, resident, original, before]() mutable
    {
        juce::Random random;
        juce::AudioFormatManager formats; formats.registerBasicFormats();
        const auto existingPlayback = library;
        std::vector<std::pair<std::shared_ptr<Media>, MusicalEnergyProfile>> energyUpdates;
        if (options.energy != AutoEnergyPolicy::unrestricted || options.transitions == AutoTransitionStyle::varied)
        {
            int done = 0; const int total = (int) std::count_if(library.begin(), library.end(), [](const auto& m)
            { return m && autoArrangementEligible(*m) && !m->energy.complete(); });
            for (auto& media : library)
            {
                if (*cancelled) break;
                if (!media || !autoArrangementEligible(*media) || media->energy.complete()) continue;
                juce::MessageManager::callAsync([safe, cancelled, done = done++, total, name = media->libraryTitle()]
                {
                    if (!safe || safe->importCancellation != cancelled || *cancelled) return;
                    safe->importProgress.setCounts(done, total); safe->importProgress.showFile(name, MediaReadStage::energy);
                });
                auto upgraded = media->catalogueCopy();
                if (media->hasPlayback()) upgraded->energy = analyseMusicalEnergy(media->audioView(), media->sampleRate, cancelled);
                else
                {
                    const auto record = media->analysisRecord();
                    auto read = readMedia(formats, media->file, {}, AnalysisCacheMode::readWrite, MediaReadPurpose::energyAnalysis, &record, cancelled);
                    if (read.media && read.media->analysisSource == media->analysisSource) upgraded->energy = std::move(read.media->energy);
                    else if (!*cancelled) upgraded->playbackError = read.error.isNotEmpty() ? read.error : "Source changed while preparing musical energy.";
                }
                if (upgraded->energy.complete()) energyUpdates.emplace_back(media, upgraded->energy);
                media = std::move(upgraded);
            }
        }
        auto result = planAutoArrangement(library, options, random, cancelled, options.extendExisting ? &original : nullptr);
        bool refinedTiming = false;
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
            const auto playback = std::find_if(existingPlayback.begin(), existingPlayback.end(), [&](const auto& m)
            { return m && m->id == clip.media->id && m->analysisSource == clip.media->analysisSource && m->hasPlayback(); });
            if (playback != existingPlayback.end()) { clip.media = *playback; continue; }
            const auto source = clip.media; const auto record = source->analysisRecord();
            auto loaded = readMedia(formats, source->file, {}, AnalysisCacheMode::readWrite, MediaReadPurpose::playback, &record, cancelled);
            if (!loaded.media) { result.error = loaded.error; break; }
            auto& m = *loaded.media;
            if (!(m.analysisSource == source->analysisSource))
            { result.error = "A chosen source changed. Refresh the library and try again. The current mix was kept."; break; }
            m.id = source->id; m.name = source->name; m.manualBpm = source->manualBpm;
            m.manualBeatOrigin = source->manualBeatOrigin; m.manualKey = source->manualKey;
            const bool timingChanged = m.bpm() != source->bpm() || m.beatOrigin() != source->beatOrigin() || m.downbeatOrigin() != source->downbeatOrigin();
            const bool measuredRefinement = source->analysis.attackTimingRevision == 0 && m.analysis.attackTimingRevision == 1
                && source->manualBpm == 0 && source->manualBeatOrigin < 0;
            if (!autoArrangementEligible(m) || m.key() != source->key() || m.duration() != source->duration()
                || (timingChanged && !measuredRefinement))
            { result.error = "Chosen song analysis changed. Refresh the library and try again. The current mix was kept."; break; }
            refinedTiming |= timingChanged;
            clip.media = std::move(loaded.media);
        }
        if (result.error.isEmpty() && !*cancelled && refinedTiming)
        {
            // The shortlist was chosen from saved metadata. Rebuild its musical
            // placements using the measured clocks before preparing any audio;
            // never expand this into decoding the rest of the user's library.
            std::vector<std::shared_ptr<Media>> hydrated;
            for (const auto& clip : result.project.clips) hydrated.push_back(clip.media);
            auto correctedOptions = options;
            if (!options.extendExisting && correctedOptions.startingId.isEmpty() && !hydrated.empty()) correctedOptions.startingId = hydrated.front()->id;
            result = planAutoArrangement(hydrated, correctedOptions, random, cancelled, options.extendExisting ? &original : nullptr);
        }
        if (result.error.isEmpty() && !*cancelled)
        {
            juce::MessageManager::callAsync([safe, cancelled, count = (int) result.project.clips.size()]
            { if (safe && safe->importCancellation == cancelled && !*cancelled) safe->importProgress.setAutoMixPreparation(count); });
            if (options.extendExisting) result.error = prepareAppendedClips(result.project,original.clips.size(),resident);
            else
            {
                auto prepared = prepareTempoMap(std::move(result.project), false, options.overlapBeats, resident, 0, {}, options.preset, options.sectionTransitions);
                result.project = std::move(prepared.project); result.error = prepared.error;
            }
        }
        juce::MessageManager::callAsync([safe, cancelled, result = std::move(result), energyUpdates = std::move(energyUpdates), options, before, prefix = original.clips.size()]() mutable
        {
            if (!safe || safe->importCancellation != cancelled) return;
            safe->importCancellation.reset(); safe->importProgress.setVisible(false); safe->setBusy(false);
            safe->autoArrangeButton.setButtonText("Auto Arrange");
            if (*cancelled) { safe->setStatus("Auto arrangement cancelled. The current mix was kept."); return; }
            if (options.extendExisting && (before != juce::JSON::toString(projectToJson(safe->engine.project)) || !safe->canAutoArrange()
                || safe->engine.rehearsalActive() || safe->engine.recordingAutomation() || safe->engine.automationRecordMode != AutomationRecordMode::read))
            { safe->setStatus("The arrangement or playback state changed. Open Auto Arrange again; current mix kept."); return; }
            for (const auto& update : energyUpdates)
                for (const auto& media : safe->engine.library)
                    if (media->id == update.first->id && media->analysisSource == update.first->analysisSource)
                    { const juce::ScopedLock guard(safe->engine.lock); media->energy = update.second; safe->markCatalogueChanged(); }
            if (result.error.isNotEmpty())
            {
                safe->setStatus("Auto arrangement: " + result.error + " The current mix was kept.");
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Auto Arrange stopped", safe->status, "OK", safe);
                return;
            }
            if (options.extendExisting && result.project.clips.size() == prefix)
            {
                safe->setStatus(result.summary);
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,"Auto Arrange complete",result.summary,"OK",safe);
                return;
            }
            safe->engine.setAutomationRecordMode(AutomationRecordMode::read); safe->remember();
            {
                const juce::ScopedLock guard(safe->engine.lock);
                // Preserve the user's mixer, plugins and master settings; replace arrangement-owned data only.
                auto candidate = safe->engine.project;
                candidate.name = result.project.name; candidate.clips = std::move(result.project.clips);
                candidate.tempo = std::move(result.project.tempo); candidate.bpm = result.project.bpm;
                candidate.followTrackTempo = result.project.followTrackTempo;
                if (!options.extendExisting)
                {
                    candidate.labels.clear(); candidate.masterAutomation = {};
                    for (auto& track : candidate.tracks)
                    {
                        track.automation = {};
                        for (auto& parameter : track.pluginAutomation) parameter.curve = {};
                    }
                }
                candidate.transitionLinks = std::move(result.project.transitionLinks); candidate.transitionLinksKnown = true;
                safe->engine.project = std::move(candidate); safe->engine.tempo = safe->engine.project.bpm;
                if (!options.extendExisting) safe->engine.position = safe->engine.project.playbackStart();
                safe->engine.resetPlugins();
            }
            safe->selectedClip = safe->engine.project.clips[options.extendExisting ? prefix : 0].id; safe->selectedTransition.clear();
            if (!options.extendExisting) safe->sessionFile = {}; // A new mix gets its own document.
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
