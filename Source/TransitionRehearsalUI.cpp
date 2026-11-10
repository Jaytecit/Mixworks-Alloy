#include "Workstation.h"

namespace mixworks
{
void Workstation::initialiseTransitionRehearsalUI()
{
    alternativesLabel.setText("A / B / C", juce::dontSendNotification);
    savedPresetsLabel.setText("Saved presets", juce::dontSendNotification);
    for (auto* component : std::initializer_list<juce::Component*> {&alternativesLabel, &savedPresetsLabel, &prepareRehearsal, &endRehearsal, &savedTransitionPresets, &saveTransitionPreset, &removeTransitionPreset, &importTransitionPreset, &exportTransitionPreset}) transitionAdvancedBody.addAndMakeVisible(component);
    rehearsalContextLabel.setText("Rehearsal context / bars", juce::dontSendNotification);
    for (auto* component : std::initializer_list<juce::Component*> {&rehearsalContextLabel, &rehearsalLeadIn, &rehearsalLeadOut}) transitionAdvancedBody.addAndMakeVisible(component);
    rehearsalLeadIn.setTitle("Rehearsal bars before frame"); rehearsalLeadOut.setTitle("Rehearsal bars after frame");
    for (int bars : {0, 1, 2, 4, 8})
    {
        const auto amount = bars == 0 ? juce::String("none") : juce::String(bars) + (bars == 1 ? " bar" : " bars");
        rehearsalLeadIn.addItem("Before: " + amount, bars + 1); rehearsalLeadOut.addItem("After: " + amount, bars + 1);
    }
    for (auto* combo : {&rehearsalLeadIn, &rehearsalLeadOut})
    {
        combo->setTooltip("How much audio to hear before and after each comparison. A short preview may cut off an effect ending.");
        combo->onChange = [this] { if (!refreshing) changeSelectedRehearsalContext(); };
    }
    for (int i = 0; i < 3; ++i)
    {
        const auto letter = juce::String::charToString((juce::juce_wchar) ('A' + i));
        auto& store = storeAlternative[(size_t) i]; auto& audition = auditionAlternative[(size_t) i]; auto& apply = applyAlternative[(size_t) i];
        for (auto* button : {&store, &audition, &apply}) transitionAdvancedBody.addAndMakeVisible(button);
        store.setButtonText("Store " + letter); audition.setButtonText("Hear " + letter); apply.setButtonText("Apply " + letter);
        audition.getProperties().set("alloyMolten", true);
        store.setTooltip("Replace this slot with the selected frame's current treatment. Save the session to retain it.");
        audition.setTooltip("Loop this comparison at matched listening volume. The mix stays unchanged.");
        apply.setTooltip("Apply this treatment to the existing frame with one Undo. Manual automation, effects and song positions stay in place.");
        store.onClick = [this, i] { storeSelectedAlternative(i); };
        audition.onClick = [this, i]
        {
            if (busy) return;
            if (engine.workMode==WorkMode::recordPerform) { prepareSelectedRehearsal(i); return; }
            const auto result = engine.auditionTransitionAlternative(i);
            if (result.failed()) prepareSelectedRehearsal(i);
            else { setStatus(engine.rehearsalSummary()); refresh(); }
        };
        apply.onClick = [this, i]
        { const auto* link = engine.project.transitionFor(selectedTransition); if (link && link->alternatives[(size_t) i]) applySelectedStyle(*link->alternatives[(size_t) i], "Alternative " + juce::String::charToString((juce::juce_wchar) ('A' + i))); };
    }
    prepareRehearsal.setTooltip("Prepare saved A/B/C blends at matched listening volume.");
    endRehearsal.setTooltip("End comparison playback and restore the previous mix cursor, paused.");
    savedTransitionPresets.setTooltip("Choose a session preset to apply its treatment to the selected frame. One Undo restores the previous treatment.");
    saveTransitionPreset.setTooltip("Name and save the selected transition treatment as a preset in this session.");
    removeTransitionPreset.setTooltip("Remove the chosen preset from this session. Undo restores it.");
    importTransitionPreset.setTooltip("Load a transition preset file into this session.");
    exportTransitionPreset.setTooltip("Save the chosen transition preset to a separate file for reuse.");
    prepareRehearsal.onClick = [this] { prepareSelectedRehearsal(); };
    endRehearsal.onClick = [this] { if (engine.workMode==WorkMode::recordPerform) libraryPreview.stop(); else engine.endTransitionRehearsal(); setStatus("Rehearsal stopped."); refresh(); };
    savedTransitionPresets.setTextWhenNothingSelected("Choose saved preset..."); savedTransitionPresets.setTitle("Saved session transition preset");
    savedTransitionPresets.onChange = [this]
    {
        const int index = savedTransitionPresets.getSelectedId() - 1;
        if (!refreshing && index >= 0 && index < (int) engine.project.transitionPresets.size())
        {
            const auto& preset = engine.project.transitionPresets[(size_t)index];
            applySelectedStyle(preset.style,preset.name);
        }
    };
    saveTransitionPreset.onClick = [this] { chooseTransitionPresetName(); };
    removeTransitionPreset.onClick = [this]
    {
        if (busy) return; const int index = savedTransitionPresets.getSelectedId() - 1;
        if (index < 0 || index >= (int) engine.project.transitionPresets.size()) return;
        remember(); { const juce::ScopedLock guard(engine.lock); engine.project.transitionPresets.erase(engine.project.transitionPresets.begin() + index); }
        savedTransitionPresets.setSelectedId(0, juce::dontSendNotification); refresh(); setStatus("Preset removed.");
    };
    importTransitionPreset.onClick = [this] { chooseTransitionPresetFile(false); };
    exportTransitionPreset.onClick = [this] { chooseTransitionPresetFile(true); };
}
void Workstation::layoutTransitionRehearsalUI(juce::Rectangle<int>& body)
{
    auto rehearsal = body;
    const bool wide = body.getWidth() >= 680;
    auto presets = body;
    if (wide) { rehearsal = body.removeFromLeft((body.getWidth() - 20) / 2); body.removeFromLeft(20); presets = body; }
    auto row = rehearsal.removeFromTop(26);
    prepareRehearsal.setBounds(row.removeFromLeft(row.getWidth() / 2).withTrimmedRight(2)); endRehearsal.setBounds(row);
    rehearsal.removeFromTop(4);
    auto contextRow = rehearsal.removeFromTop(26);
    rehearsalContextLabel.setBounds(contextRow.removeFromLeft(146));
    rehearsalLeadIn.setBounds(contextRow.removeFromLeft(contextRow.getWidth() / 2).withTrimmedRight(2)); rehearsalLeadOut.setBounds(contextRow);
    rehearsal.removeFromTop(4);
    alternativesLabel.setBounds(rehearsal.removeFromTop(18));
    auto three = [&](auto& buttons)
    {
        auto row = rehearsal.removeFromTop(24); const int width = row.getWidth() / 3;
        for (size_t i = 0; i < 3; ++i) buttons[i].setBounds(i == 2 ? row : row.removeFromLeft(width).withTrimmedRight(2));
        rehearsal.removeFromTop(2);
    };
    three(storeAlternative); three(auditionAlternative); three(applyAlternative);
    if (!wide) presets = rehearsal;
    savedPresetsLabel.setBounds(presets.removeFromTop(20)); savedTransitionPresets.setBounds(presets.removeFromTop(26)); presets.removeFromTop(3);
    row = presets.removeFromTop(26); saveTransitionPreset.setBounds(row.removeFromLeft(row.getWidth() / 2).withTrimmedRight(2)); removeTransitionPreset.setBounds(row); presets.removeFromTop(3);
    row = presets.removeFromTop(26); importTransitionPreset.setBounds(row.removeFromLeft(row.getWidth() / 2).withTrimmedRight(2)); exportTransitionPreset.setBounds(row); presets.removeFromTop(10);
}
void Workstation::refreshTransitionRehearsalUI(bool editable)
{
    const auto* link = engine.project.transitionFor(selectedTransition);
    if (!link) engine.endTransitionRehearsal();
    const bool ready = link && !busy && !engine.project.hasPendingPlacement(); bool any = false;
    const auto context = engine.project.rehearsalContext;
    for (auto* combo : {&rehearsalLeadIn, &rehearsalLeadOut})
    {
        const bool before = combo == &rehearsalLeadIn; const int bars = before ? context.beforeBars : context.afterBars;
        combo->setSelectedId(0, juce::dontSendNotification);
        combo->setText(juce::String(before ? "Before: " : "After: ") + juce::String(bars) + (bars == 1 ? " bar" : " bars"), juce::dontSendNotification);
        for (int choice : {0, 1, 2, 4, 8}) if (choice == bars) combo->setSelectedId(choice + 1, juce::dontSendNotification);
        combo->setEnabled(ready && pendingTempo <= 0 && !tempoPreparationPending);
    }
    for (size_t i = 0; i < 3; ++i)
    {
        const bool stored = link && link->alternatives[i].has_value(); any |= stored;
        storeAlternative[i].setEnabled(ready); auditionAlternative[i].setEnabled(ready && stored);
        auditionAlternative[i].setToggleState(engine.transitionRehearsalSlot() == (int) i, juce::dontSendNotification);
        applyAlternative[i].setEnabled(editable && stored);
    }
    prepareRehearsal.setEnabled(ready && any); endRehearsal.setEnabled(!busy && engine.rehearsalActive());
    const int selected = savedTransitionPresets.getSelectedId(); savedTransitionPresets.clear(juce::dontSendNotification);
    int id = 1; for (const auto& preset : engine.project.transitionPresets) savedTransitionPresets.addItem(preset.name, id++);
    savedTransitionPresets.setSelectedId(selected < id ? selected : 0, juce::dontSendNotification); savedTransitionPresets.setEnabled(editable);
    saveTransitionPreset.setEnabled(ready); removeTransitionPreset.setEnabled(!busy && selected > 0 && selected < id);
    importTransitionPreset.setEnabled(!busy); exportTransitionPreset.setEnabled(!busy && selected > 0 && selected < id);
}
void Workstation::changeSelectedRehearsalContext()
{
    if (busy) return;
    if (pendingTempo > 0 || tempoPreparationPending || engine.project.hasPendingPlacement())
    { setStatus("Wait for preparation before changing rehearsal context."); refresh(); return; }
    const juce::ScopedLock guard(engine.lock); auto context = engine.project.rehearsalContext;
    if (rehearsalLeadIn.getSelectedId() > 0) context.beforeBars = rehearsalLeadIn.getSelectedId() - 1;
    if (rehearsalLeadOut.getSelectedId() > 0) context.afterBars = rehearsalLeadOut.getSelectedId() - 1;
    auto candidate = engine.project; const auto result = candidate.setRehearsalContext(context);
    if (result.failed()) { setStatus(result.getErrorMessage()); refresh(); return; }
    if (!(context == engine.project.rehearsalContext))
    { remember(); engine.project = std::move(candidate); setStatus("Rehearsal context saved; prepare A/B/C again to use the new window."); }
    refresh();
}
void Workstation::storeSelectedAlternative(int slot)
{
    if (busy || pendingTempo > 0 || tempoPreparationPending) return;
    engine.flushMidiControls(); engine.finishAutomation(); const juce::ScopedLock guard(engine.lock);
    auto candidate = engine.project; const auto result = candidate.storeTransitionAlternative(selectedTransition, slot);
    if (result.failed()) { setStatus(result.getErrorMessage()); return; }
    if (juce::JSON::toString(projectToJson(candidate)) != juce::JSON::toString(projectToJson(engine.project))) { remember(); engine.project = std::move(candidate); }
    refresh(); setStatus("Stored " + juce::String::charToString((juce::juce_wchar) ('A' + slot)) + ". Save the session to retain alternatives.");
}
void Workstation::applySelectedStyle(const TransitionStyle& style,const juce::String& name)
{
    if(busy || engine.exporting || pendingTempo>0 || tempoPreparationPending || engine.rehearsalActive()
        || (engine.workMode!=WorkMode::recordPerform && (engine.automationRecordMode!=AutomationRecordMode::read || engine.recordingAutomation())))
    { setStatus("Finish preparation, export or rehearsal. In Arrange choose Read before applying a saved transition."); return; }
    const auto treatment=style; engine.flushMidiControls(); engine.finishAutomation();
    Project before; { const juce::ScopedLock guard(engine.lock); before=engine.project; }
    auto candidate=before; const auto result=candidate.applyNamedTransitionStyle(selectedTransition,treatment,name);
    if(result.failed()) { setStatus(result.getErrorMessage()); refresh(); return; }
    if(juce::JSON::toString(projectToJson(candidate))==juce::JSON::toString(projectToJson(before))) { refresh(); return; }
    { const juce::ScopedLock guard(engine.lock); remember(); candidate.performanceTakes.swap(engine.project.performanceTakes); std::swap(engine.project,candidate); }
    refresh(); setStatus("Transition applied in the selected frame. Mixer-lane curves still combine with its track curves.");
}
void Workstation::prepareSelectedRehearsal(int slot)
{
    if (busy || pendingTempo > 0 || tempoPreparationPending) return;
    if (engine.workMode==WorkMode::recordPerform)
    {
        libraryPreview.stop(); auto result=privatePreparation.prepareTransition(selectedTransition);
        if (result.wasOk() && slot>=0) result=privatePreparation.alternative(slot);
        if (result.wasOk()) result=libraryPreview.playPrepared(privatePreparation,"private-transition");
        setStatus(result.failed() ? result.getErrorMessage() : "Private transition playing. Audience programme and MIDI target are unchanged."); return;
    }
    engine.flushMidiControls(); const auto incoming = selectedTransition;
    if (!engine.project.transitionFor(incoming)) return;
    setBusy(true); engine.setPlaying(false);
    auto cancelled = std::make_shared<std::atomic<bool>>(false); importCancellation = cancelled;
    importProgress.begin(0); importProgress.setRehearsal(); importProgress.setVisible(true);
    setStatus("Rendering A/B/C rehearsal with the session's effects. Cancel stops preparation.");
    workers.addJob([safe = juce::Component::SafePointer<Workstation>(this), incoming, slot, cancelled]
    {
        if (!safe) return;
        const auto result = safe->engine.prepareTransitionRehearsal(incoming, cancelled);
        juce::MessageManager::callAsync([safe, incoming, slot, result]
        {
            if (!safe) return;
            safe->importCancellation.reset(); safe->importProgress.setVisible(false); safe->setBusy(false);
            if (result.failed()) { safe->setStatus(result.getErrorMessage()); safe->refresh(); return; }
            if (safe->selectedTransition != incoming) { safe->engine.endTransitionRehearsal(); safe->refresh(); return; }
            int selected = slot;
            if (selected < 0) { const auto* link = safe->engine.project.transitionFor(incoming); if (link) for (int i = 0; i < 3; ++i) if (link->alternatives[(size_t) i]) { selected = i; break; } }
            const auto audition = safe->engine.auditionTransitionAlternative(selected);
            safe->setStatus(audition.failed() ? audition.getErrorMessage() : safe->engine.rehearsalSummary()); safe->refresh();
        });
    });
}
void Workstation::chooseTransitionPresetName()
{
    if (busy) return; TransitionStyle style; const auto result = engine.project.captureTransitionStyle(selectedTransition, style);
    if (result.failed()) { setStatus(result.getErrorMessage()); return; }
    auto* dialog = new juce::AlertWindow("Save transition preset", "Name this blend.", juce::MessageBoxIconType::NoIcon, this);
    dialog->addTextEditor("name", {}, "Preset name"); dialog->getTextEditor("name")->setInputRestrictions(128);
    dialog->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey)); dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    dialog->enterModalState(true, juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<Workstation>(this), dialog, style](int answer)
    {
        if (!safe || safe->busy || answer != 1) return;
        NamedTransitionStyle preset {dialog->getTextEditorContents("name").trim(), style};
        auto candidate = safe->engine.project; const auto valid = candidate.putTransitionPreset(preset);
        if (valid.failed()) { safe->setStatus(valid.getErrorMessage()); return; }
        safe->remember(); { const juce::ScopedLock guard(safe->engine.lock); safe->engine.project.transitionPresets = std::move(candidate.transitionPresets); }
        safe->refresh(); safe->setStatus("Saved session preset: " + preset.name);
    }), true);
}
void Workstation::chooseTransitionPresetFile(bool writing)
{
    if (busy) return; const int selected = savedTransitionPresets.getSelectedId() - 1;
    if (writing && (selected < 0 || selected >= (int) engine.project.transitionPresets.size())) return;
    const auto preset = writing ? engine.project.transitionPresets[(size_t) selected] : NamedTransitionStyle {};
    const auto initial = writing ? defaultSaveLocation("Transition.alloypreset") : defaultSaveFolder();
    if (initial == juce::File()) return;
    chooser = std::make_unique<juce::FileChooser>(writing ? "Export transition preset" : "Import transition preset", initial, "*.alloypreset", false);
    chooser->launchAsync((writing ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting : juce::FileBrowserComponent::openMode) | juce::FileBrowserComponent::canSelectFiles,
        [safe = juce::Component::SafePointer<Workstation>(this), writing, preset](const auto& files)
    {
        if (!safe || safe->busy || files.getResult() == juce::File()) return;
        if (writing) { const auto result = writeTransitionPreset(files.getResult().withFileExtension("alloypreset"), preset); safe->setStatus(result.failed() ? result.getErrorMessage() : "Exported portable transition preset."); return; }
        NamedTransitionStyle imported; const auto result = readTransitionPreset(files.getResult(), imported);
        if (result.failed()) { safe->setStatus(result.getErrorMessage()); return; }
        // Imported duplicates get a distinct name; never silently replace user work.
        const auto name = imported.name; int suffix = 2;
        while (std::any_of(safe->engine.project.transitionPresets.begin(), safe->engine.project.transitionPresets.end(), [&](const auto& x) { return x.name == imported.name; })) imported.name = name.substring(0, 110) + " (" + juce::String(suffix++) + ")";
        safe->remember(); { const juce::ScopedLock guard(safe->engine.lock); safe->engine.project.putTransitionPreset(imported); }
        safe->refresh(); safe->setStatus("Imported preset: " + imported.name + ". Choose it to apply; Save retains it.");
    });
}
}
