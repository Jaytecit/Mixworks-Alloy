#include "GridCorrectionUI.h"
#include "UiMessages.h"

namespace mixworks
{
GridCorrectionEditor::GridCorrectionEditor(Engine& e, std::shared_ptr<const Media> media, const juce::String& details,
                                           std::function<juce::Result(const SourceCorrection&, bool)> commit)
    : engine(e), source(std::move(media)), apply(std::move(commit)), draft(SourceCorrection::from(*source))
{
    setSize(980, 780); setWantsKeyboardFocus(true);
    setTooltip("Drag the audio beneath the fixed centre marker. Set A, B and C on three downbeats, confirm the bar counts, then check and save.");
    addAndMakeVisible(simplePage);
    simplePage.setClickingTogglesState(true); simplePage.setRadioGroupId(2);
    simplePage.onClick = [this] { selectPage(4); };
    for (int i = 0; i < 3; ++i)
    {
        const auto name = juce::String::charToString((juce::juce_wchar) ('A' + i));
        addAndMakeVisible(setPoint[(size_t)i]); addAndMakeVisible(visitPoint[(size_t)i]); addAndMakeVisible(pointInfo[(size_t)i]);
        setPoint[(size_t)i].setButtonText("Set " + name);
        setPoint[(size_t)i].setTooltip("Pause and pin " + name + " at the fixed centre marker. Replacing A or B clears the later marks.");
        setPoint[(size_t)i].onClick = [this, i] { setPointAtCursor(i); };
        visitPoint[(size_t)i].setButtonText("Hear / go to " + name);
        visitPoint[(size_t)i].onClick = [this, i] { selectedHandle = 0; setCursor(points[(size_t)i]); };
        visitPoint[(size_t)i].setTooltip("Return this mark to the centre. While playing, listen again from here.");
    }
    for (auto* c : std::initializer_list<juce::Component*> {&barsAB, &barsBC, &countAB, &countBC, &countsConfirmed, &simpleResult}) addAndMakeVisible(c);
    countAB.setText("Bars A to B", juce::dontSendNotification); countBC.setText("Bars B to C", juce::dontSendNotification);
    for (auto* count : {&barsAB, &barsBC})
    {
        count->setRange(1, 4096, 1); count->setValue(16, juce::dontSendNotification);
        count->setSliderStyle(juce::Slider::IncDecButtons); count->setTextBoxStyle(juce::Slider::TextBoxLeft, false, 60, 28);
        count->setTooltip("Complete 4/4 bars between these marks, not including the final bar. 16 means the later mark starts bar 17. Suggested counts need checking by ear.");
        count->onValueChange = [this] { countsConfirmed.setToggleState(false, juce::dontSendNotification); measurePoints(); };
    }
    barsAB.setName("Complete bars A to B"); barsBC.setName("Complete bars B to C");
    countsConfirmed.onClick = [this] { changed(); };
    simpleResult.setFont(juce::FontOptions(12));
    for (auto* component : std::initializer_list<juce::Component*> {&heading, &instructions, &effectiveTempo, &slower, &faster, &rebuild, &info, &bpm, &phase, &key, &zoom, &bpmLabel, &phaseLabel, &keyLabel, &status,
        &half, &twice, &tap, &resetTap, &beatOne, &earlier, &later, &automatic, &beginning, &middle, &end, &quiet, &play, &save, &cancel, &click, &sourceAudio, &cursor, &level,
        &precision, &anchorInfo, &barsLabel, &bars, &moveGrid, &spacing, &narrower, &wider, &goA, &goB, &matchB, &undoGrid, &redoGrid, &revertGrid, &applyNotice, &keyByEar, &analysisDetails, &failures, &lastRetry})
        addAndMakeVisible(component);
    int tab = 0;
    for (auto* button : {&tempoPage, &beatPage, &checkPage, &advancedPage})
    {
        addAndMakeVisible(button); button->setClickingTogglesState(true); button->setRadioGroupId(2);
        button->onClick = [this, tab] { selectPage(tab); }; ++tab;
    }
    addChildComponent(barEarlier); addChildComponent(barLater);
    barEarlier.onClick = [this] { shiftBar(-1); }; barLater.onClick = [this] { shiftBar(1); };
    barEarlier.setTooltip("Move the beat-one accent back one beat. Beat spacing and pulse alignment stay the same.");
    barLater.setTooltip("Move the beat-one accent forward one beat. Beat spacing and pulse alignment stay the same.");
    heading.setText("SOURCE BEAT GRID  /  " + source->name, juce::dontSendNotification);
    info.setVisible(false); info.setText(details, false); heading.setTooltip("Review this track's BPM, beat one and key.");
    analysisDetails.onClick = [this] { showUiDetails(*this, "Analysis details", info.getText()); };
    analysisDetails.setTooltip("Open the full analysis report.");
    failures.setName("Retry failures"); failures.setTextWhenNothingSelected("No retry failures"); failures.setEnabled(false);
    failures.setTooltip("Open a track that still needs correction after retry."); lastRetry.setFont(juce::FontOptions(12));
    instructions.setText("1  Set A at a known beat one.     2  Drag B to a later downbeat.     3  Check across the track and save.", juce::dontSendNotification);
    instructions.setFont(juce::FontOptions(12));
    bpmLabel.setText("BPM", juce::dontSendNotification);
    phaseLabel.setText("A / beat one (source seconds)", juce::dontSendNotification);
    keyLabel.setText("Key", juce::dontSendNotification);
    setDraftBpm(draft.bpm);
    bpm.setTextToShowWhenEmpty(source->analysis.bpm > 0 ? "Detected: " + preciseBpmText(source->analysis.bpm) : "Tap or enter BPM", colour::muted);
    phase.setText(draft.downbeat >= 0 ? juce::String(draft.downbeat, 17) : juce::String(), false);
    bpm.setName("BPM override"); phase.setName("Downbeat anchor in source seconds");
    key.addItem("Automatic key", 1); for (int i = 0; i < 24; ++i) key.addItem(keyNoteName(i) + " / " + keyCamelot(i), i + 2);
    key.setSelectedId(draft.key + 2, juce::dontSendNotification);
    zoom.addItem("10 ms", 8); zoom.addItem("50 ms", 7); zoom.addItem("250 ms", 6);
    zoom.addItemList({"1 second", "4 seconds", "12 seconds", "32 seconds", "120 seconds"}, 1); zoom.setSelectedId(3, juce::dontSendNotification);
    zoom.onChange = [this]
    {
        const double spans[] {1, 4, 12, 32, 120, .25, .05, .01}; viewSpan = spans[zoom.getSelectedId() - 1];
        const double at = page == 4 ? cursor.getValue() : selectedHandle == 1 && draft.downbeat >= 0 ? draft.downbeat : selectedHandle == 2 ? secondAnchor() : cursor.getValue();
        setCursor(at >= 0 && at < source->duration() ? at : cursor.getValue());
    };
    precision.addItem("1 ms steps", 1); precision.addItem("10 ms steps", 2); precision.setSelectedId(1, juce::dontSendNotification);
    precision.setName("Grid precision step"); precision.setTooltip("Step used by Earlier, Later, Narrower, Wider and the left/right arrow keys.");
    barsLabel.setText("Bars A to B", juce::dontSendNotification);
    bars.setRange(1, 256, 1); bars.setValue(16, juce::dontSendNotification);
    bars.setSliderStyle(juce::Slider::IncDecButtons); bars.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 48, 26);
    bars.setName("Whole bars between A and B");
    bars.setTooltip("Whole 4/4 bars between A and B: 16 means B is bar 17. After aligning B, changing this count recalculates BPM while keeping B at the same source time.");
    bars.onValueChange = [this] { changeBars(); };
    beatOne.setButtonText("Set A / beat 1"); automatic.setButtonText("Detected grid");
    moveGrid.setClickingTogglesState(true); spacing.setClickingTogglesState(true);
    moveGrid.setRadioGroupId(1); spacing.setRadioGroupId(1); moveGrid.setToggleState(true, juce::dontSendNotification);
    moveGrid.setTooltip("Drag A or the waveform ruler to move the whole grid without changing BPM. Left/right arrows use the selected precision step.");
    spacing.setTooltip("Keep A pinned. Drag B, or select and drag a later downbeat in the ruler, to stretch or compact the grid and calculate BPM.");
    moveGrid.onClick = spacing.onClick = [this] { updateAlignmentControls(); };
    goA.onClick = [this] { selectedHandle = 1; setCursor(draft.downbeat); };
    goB.onClick = [this] { selectedHandle = 2; setCursor(secondAnchor()); };
    matchB.onClick = [this] { alignSecond(cursor.getValue()); };
    matchB.setTooltip("Calculate BPM by placing B at the exact source cursor time. Check Bars A to B first. A stays pinned.");
    goA.setTooltip("Show A / beat one. Zoom then stays centred near this handle.");
    goB.setTooltip("Show the later B downbeat. Check its bar count, then drag its handle onto the matching waveform onset.");
    narrower.setTooltip("Move B earlier by the selected step, keeping A pinned. Shorter bars increase calculated BPM.");
    wider.setTooltip("Move B later by the selected step, keeping A pinned. Longer bars decrease calculated BPM.");
    narrower.onClick = [this] { if (validDraft) alignSecond(secondAnchor() - precisionSeconds()); };
    wider.onClick = [this] { if (validDraft) alignSecond(secondAnchor() + precisionSeconds()); };
    undoGrid.onClick = [this] { undo(false); }; redoGrid.onClick = [this] { undo(true); };
    revertGrid.onClick = [this] { restoreGrid(initialGrid); };
    undoGrid.setTooltip("Undo one grid edit or completed drag. Key corrections are kept.");
    redoGrid.setTooltip("Redo a grid edit. Key corrections are kept.");
    revertGrid.setTooltip("Restore the BPM and downbeat shown when this editor opened. Key corrections are kept; this action is undoable.");
    cursor.setRange(0, std::max(0.0001, source->duration() - 1 / source->sampleRate), 0);
    cursor.setNumDecimalPlacesToDisplay(6);
    cursor.setSliderStyle(juce::Slider::LinearHorizontal); cursor.setTextBoxStyle(juce::Slider::TextBoxRight, false, 100, 24); cursor.setName("Source cursor in seconds");
    cursor.onValueChange = [this] { if (!updating) setCursor(cursor.getValue()); };
    level.setRange(-36, -3, 0.1); level.setValue(-12, juce::dontSendNotification); level.setTextValueSuffix(" dB");
    level.setSliderStyle(juce::Slider::LinearHorizontal); level.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 24); level.setName("Source audition level");
    click.setToggleState(false, juce::dontSendNotification); sourceAudio.setToggleState(true, juce::dontSendNotification);
    bpm.setTooltip("Override constant source BPM. Leave blank to use detection; Apply correction commits the draft.");
    phase.setTooltip("Time of a known beat one in the original track, in seconds. Leave blank to use the detected position.");
    key.setTooltip("Choose a manual musical key, or Automatic key to use detection.");
    keyByEar.setTooltip("Compare reference sounds with a musical loop, then save a manually chosen source key without changing timing or pitch.");
    keyByEar.setEnabled(false); keyByEar.onClick = [this] { if (openKey && playback) { pauseAudition(); openKey(playback); } };
    zoom.setTooltip("Choose the visible source interval, from 10 ms sample detail to a 120-second overview. Click to listen; drag the waveform to align it beneath the grid. Hold Shift for finer movement.");
    cursor.setTooltip("Move the source audition cursor in seconds. This does not move the mix cursor.");
    level.setTooltip("Set audition volume for source audio and grid click. This does not change segment or export gain.");
    click.setTooltip("Include the beat-grid click in source audition to check alignment.");
    sourceAudio.setTooltip("Include the original source audio in audition. Turn off to hear only the grid click.");
    half.setTooltip("Halve the draft BPM to correct double-time detection.");
    twice.setTooltip("Double the draft BPM to correct half-time detection.");
    tap.setTooltip("Tap at least three beats to estimate the draft BPM. Listen across the track before applying.");
    resetTap.setTooltip("Clear the tap history before starting a new BPM estimate.");
    beatOne.setTooltip("Pin A at the cursor on any known downbeat. The 4/4 bar grid extends both ways. Use Go to B to align a later downbeat.");
    automatic.setTooltip("Restore detected BPM and beat one in this draft, keeping your key choice. Save or Apply keeps the changes.");
    beginning.setTooltip("Move source audition to the beginning to check the grid.");
    middle.setTooltip("Move source audition to the middle to check timing drift.");
    end.setTooltip("Move source audition near the end to check timing drift.");
    quiet.setTooltip("Find a quiet passage to check the grid.");
    play.setTooltip("Play or pause this source audition. Wait for source audio preparation if unavailable.");
    save.setTooltip("Save BPM, downbeat and key for this source without retiming the current arrangement. Use Apply to arrangement to save and rebuild Beatmatch with these corrections.");
    rebuild.setName("Save corrections and rebuild timeline");
    rebuild.setTooltip("Save corrections and retime the whole mix; tracks and transitions may move. Undo restores the mix, while source corrections stay saved.");
    rebuild.setColour(juce::TextButton::buttonColourId, colour::orange.darker(.65f));
    applyNotice.setFont(juce::FontOptions(11));
    slower.setName("Decrease source tempo by 0.01 BPM"); faster.setName("Increase source tempo by 0.01 BPM");
    slower.setTooltip("Decrease source BPM by 0.01 to fine tune steady timing drift. The downbeat anchor stays fixed until saved. You can also type an exact BPM.");
    faster.setTooltip("Increase source BPM by 0.01 to fine tune steady timing drift. The downbeat anchor stays fixed until saved. You can also type an exact BPM.");
    slower.onClick = [this] { adjustTempo(-0.01); }; faster.onClick = [this] { adjustTempo(0.01); };
    cancel.setTooltip("Close source-grid review without applying the draft corrections.");
    bpm.onTextChange = [this] { exactBpm = 0; calculated = false; changed(); };
    bpm.onFocusLost = [this]
    {
        if (readDraft().wasOk())
        {
            setDraftBpm(draft.bpm);
            lastGrid = gridEdit();
        }
    };
    phase.onTextChange = key.onChange = click.onClick = sourceAudio.onClick = level.onValueChange = [this] { changed(); };
    half.onClick = [this] { changeRate(0.5); }; twice.onClick = [this] { changeRate(2); };
    tap.onClick = [this]
    {
        if (auto value = taps.tap(juce::Time::getMillisecondCounterHiRes() / 1000)) { calculated = false; setDraftBpm(*value); changed(); }
        else status.setText("Tap three or more steady beats.", juce::dontSendNotification);
    };
    resetTap.onClick = [this] { taps.reset(); status.setText("Tap tempo reset.", juce::dontSendNotification); };
    beatOne.onClick = [this] { setAnchor(); };
    earlier.setButtonText("Earlier"); later.setButtonText("Later");
    earlier.onClick = [this] { nudge(-precisionSeconds()); }; later.onClick = [this] { nudge(precisionSeconds()); };
    earlier.setTooltip("Move the whole grid earlier by the selected 1 ms or 10 ms step. BPM stays fixed.");
    later.setTooltip("Move the whole grid later by the selected 1 ms or 10 ms step. BPM stays fixed.");
    automatic.onClick = [this] { calculated = false; setDraftBpm(0); phase.setText({}, false); taps.reset(); changed(); };
    beginning.onClick = [this] { setCursor(0); }; middle.onClick = [this] { setCursor(source->duration() * 0.5); };
    end.onClick = [this] { setCursor(std::max(0.0, source->duration() - 12)); };
    quiet.onClick = [this]
    {
        double best = 0; float lowest = std::numeric_limits<float>::max();
        for (int i = 0; i < 32; ++i)
        {
            const double at = std::max(0.0, source->duration() - 12) * i / 31;
            const auto range = source->waveformRange(at, std::min(source->duration(), at + 12));
            if (range.rms < lowest) { lowest = range.rms; best = at; }
        }
        setCursor(best); status.setText("Quiet passage found.", juce::dontSendNotification);
    };
    play.onClick = [this] { togglePlay(); }; cancel.onClick = [this] { dismiss(); };
    save.onClick = [this] { this->commit(false); };
    save.setButtonText("Save for this track");
    rebuild.setButtonText("Save + rebuild mix...");
    rebuild.onClick = [this]
    {
        auto* dialog = new juce::AlertWindow("Save and rebuild the mix?",
            "This saves the track's correction and rebuilds Beatmatch for the whole arrangement. Tracks and transitions may move.\n\nUndo restores the mix; the saved correction stays. To keep the arrangement as it is, cancel and use Save for this track.",
            juce::MessageBoxIconType::QuestionIcon, this);
        dialog->addButton("Save + rebuild", 1); dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        dialog->enterModalState(true, juce::ModalCallbackFunction::create([safe=juce::Component::SafePointer<GridCorrectionEditor>(this)](int result)
        { if (safe && result==1 && safe->rebuild.isEnabled()) safe->commit(true); }), true);
    };
    status.setColour(juce::Label::textColourId, colour::muted);
    if (source->hasPlayback()) playback = source;
    play.setEnabled(playback != nullptr); changed(); selectPage(4); setCursor(0, false); startTimerHz(25);
}
void GridCorrectionEditor::selectPage(int selected)
{
    if (dragging != 0) return;
    const double at = cursor.getValue();
    page = juce::jlimit(0, 4, selected);
    simplePage.setToggleState(page==4, juce::dontSendNotification);
    tempoPage.setToggleState(page==0, juce::dontSendNotification); beatPage.setToggleState(page==1, juce::dontSendNotification);
    checkPage.setToggleState(page==2, juce::dontSendNotification); advancedPage.setToggleState(page==3, juce::dontSendNotification);
    if (page<3) (page==2 ? spacing : moveGrid).setToggleState(true, juce::dontSendNotification);
    beatOne.setButtonText(page==3 ? "Set A / beat 1" : "This is beat 1");
    goA.setButtonText(page==3 ? "Go to A" : "Back to beat 1");
    goB.setButtonText(page==3 ? "Go to B" : "Go to check beat");
    matchB.setButtonText(page==3 ? "Set B at cursor" : "Match check beat here");
    barsLabel.setText(page==3 ? "Bars A to B" : "Check after bars", juce::dontSendNotification);
    const juce::StringArray guidance {
        "Play a steady section. Keep the detected BPM if the clicks keep time, or tap along. Half / Double fixes twice or half as many clicks.",
        "Count 1, 2, 3, 4 with the music. Click a clear ONE, then This is beat 1. Any bar works; a kick alone does not prove beat one.",
        "Count the chosen number of complete bars from your marked beat 1. Go to check beat, click that later ONE, then Match check beat here.",
        "Exact source timing, A/B measurement, key corrections and analysis details. These controls edit the same unsaved draft.",
        "Drag audio beneath the centre marker; Shift drags finely. Listen for ONE, then Set A, a later B, and a still later C."
    };
    instructions.setText(guidance[page], juce::dontSendNotification);
    changed(); resized(); setCursor(at, false); repaint();
}
void GridCorrectionEditor::shiftBar(int beats)
{
    if (!validDraft) return;
    const auto grid = draft.grid(*source);
    if (!grid.hasBar()) return;
    const double anchor = grid.downbeatOrigin + beats * 60.0 / grid.bpm;
    if (anchor < 0 || anchor > cursor.getMaximum())
    { status.setText("That beat is outside the track. Choose a beat one further inside the music.", juce::dontSendNotification); return; }
    phase.setText(juce::String(anchor, 8), false); changed();
}
GridCorrectionEditor::~GridCorrectionEditor() { closePreview(); }
void GridCorrectionEditor::setDraftBpm(double value)
{
    exactBpm = value;
    bpm.setText(value > 0 ? preciseBpmText(value) : juce::String(), false);
}

juce::Result GridCorrectionEditor::readDraft()
{
    const auto text = bpm.getText();
    // Opening the editor, measuring the grid or editing another field must
    // not replace the source tempo with the rounded text shown to the user.
    const auto input = exactBpm > 0 && text == preciseBpmText(exactBpm) ? juce::String(exactBpm, 17) : text;
    return SourceCorrection::parse(input, phase.getText(), key.getSelectedId() - 2, source->duration(), draft);
}
void GridCorrectionEditor::commit(bool rebuildTimeline)
{
    if (page == 4 && !simpleAligned()) { updateSimpleControls(); return; }
    const auto result = readDraft(); if (result.failed()) { status.setText(result.getErrorMessage(), juce::dontSendNotification); return; }
    { const juce::ScopedLock guard(engine.lock); engine.gridAudition.stop(); }
    const auto committed = apply(draft, rebuildTimeline);
    if (committed.failed()) { status.setText(committed.getErrorMessage(), juce::dontSendNotification); return; }
    dismiss();
}
void GridCorrectionEditor::changed()
{
    const auto edit = gridEdit();
    if (historyReady && !restoring && dragging == 0 && !(edit == lastGrid))
    {
        gridUndo.push_back(lastGrid); if (gridUndo.size() > 128) gridUndo.erase(gridUndo.begin()); gridRedo.clear();
    }
    lastGrid = edit;
    if (!historyReady) { initialGrid = edit; historyReady = true; }
    const auto result = readDraft(); validDraft = result.wasOk(); save.setEnabled(validDraft);
    const juce::ScopedLock guard(engine.lock);
    const bool inTimeline = std::any_of(engine.project.clips.begin(), engine.project.clips.end(), [this](const auto& clip)
    { return clip.media && (clip.media == source || (clip.media->file == source->file && clip.media->analysisSource == source->analysisSource)); });
    rebuild.setEnabled(validDraft && inTimeline && !engine.project.hasLockedTransitions());
    applyNotice.setText(!inTimeline ? "Save keeps this track's correction for future use. Add the track to a mix to enable rebuilding."
        : engine.project.hasLockedTransitions() ? "Save keeps the current mix unchanged. Unlock transitions before rebuilding the mix."
        : "Save keeps the current mix unchanged. Save + rebuild uses the correction now, but can move tracks and transitions.", juce::dontSendNotification);
    effectiveTempo.setText(validDraft ? preciseBpmText(draft.grid(*source).bpm) + " BPM" + (calculated ? "  /  measured" : draft.bpm > 0 ? "  /  manual" : "  /  detected") : "Correct the invalid fields before saving.", juce::dontSendNotification);
    if (!validDraft) { engine.gridAudition.stop(); status.setText(result.getErrorMessage(), juce::dontSendNotification); }
    else
    {
        const auto grid = draft.grid(*source); engine.gridAudition.configure(grid, sourceAudio.getToggleState(), click.getToggleState(), (float) level.getValue());
        status.setText(page==4 ? "A pins beat one; A to B measures tempo; C checks it. Confirm complete bar counts, then listen with Grid click before saving."
            : page!=3 ? !grid.hasPulse() ? "No tempo yet: tap along in Tempo, or count bars between two marked beats in Check drift."
                : page==1 ? "All clicks early or late? Move the grid. Clicks line up but ONE is wrong? Use Beat 1 back / forward."
                : page==2 ? draft.downbeat<0 ? "First mark a clear ONE on the Beat one page, then return here to measure the later beat."
                    : secondAnchor()>cursor.getMaximum() ? "The check beat is beyond the track. Reduce the bar count or mark beat one earlier in the music."
                    : "Check beginning, middle and end. A growing offset means tempo drift; an equal offset everywhere means the grid needs moving."
                : "Tempo sets the distance between beats. Next, Beat one aligns the clicks and the four-beat count."
            : !grid.hasPulse() ? draft.downbeat >= 0
                ? "Choose Bars A to B, position the source cursor on B, then Set B at cursor to calculate BPM."
                : "Set A at a known downbeat, then match a later B to calculate BPM, or use Tap tempo."
            : draft.downbeat < 0 ? "Click a known downbeat, then Set A / beat 1. A can be anywhere in the source."
            : "Check the click at the beginning, middle and end. A matching waveform peak alone does not prove beat one.", juce::dontSendNotification);
    }
    updateAlignmentControls();
    updateSimpleControls();
    repaint();
}
GridCorrectionEditor::GridEdit GridCorrectionEditor::gridEdit() const
{
    return {bpm.getText(), phase.getText(), exactBpm, (int) bars.getValue(), calculated, points,
        (int) barsAB.getValue(), (int) barsBC.getValue(), countsConfirmed.getToggleState()};
}
void GridCorrectionEditor::restoreGrid(const GridEdit& edit)
{
    exactBpm = edit.exactBpm;
    bpm.setText(edit.bpm, false); phase.setText(edit.phase, false); bars.setValue(edit.bars, juce::dontSendNotification);
    points = edit.points; barsAB.setValue(edit.barsAB, juce::dontSendNotification); barsBC.setValue(edit.barsBC, juce::dontSendNotification);
    countsConfirmed.setToggleState(edit.confirmed, juce::dontSendNotification);
    calculated = edit.calculated; changed();
}
void GridCorrectionEditor::setPointAtCursor(int index)
{
    if (index < 0 || index > 2 || (index > 0 && points[(size_t)index - 1] < 0)) return;
    double at = cursor.getValue();
    { const juce::ScopedLock guard(engine.lock);
      if (engine.gridAudition.active()) at = engine.gridAudition.position();
      engine.gridAudition.stop(); }
    setCursor(at, false);
    if (index > 0 && at <= points[(size_t)index - 1])
    { simpleResult.setText("Choose a downbeat later than the previous mark. Existing marks were kept.", juce::dontSendNotification); return; }
    if (readDraft().failed()) { changed(); return; }
    const double estimate = draft.grid(*source).bpm;
    points[(size_t)index] = at;
    for (int i = index + 1; i < 3; ++i) points[(size_t)i] = -1;
    if (index == 0) phase.setText(juce::String(at, 8), false);
    else if (estimate > 0)
        (index == 1 ? barsAB : barsBC).setValue(juce::jlimit(1.0, 4096.0,
            std::round((at - points[(size_t)index - 1]) * estimate / 240.0)), juce::dontSendNotification);
    countsConfirmed.setToggleState(false, juce::dontSendNotification);
    selectedHandle = 0; measurePoints();
}
void GridCorrectionEditor::measurePoints()
{
    if (points[0] >= 0) phase.setText(juce::String(points[0], 8), false);
    if (points[1] > points[0] && points[0] >= 0)
    {
        const double measured = barsAB.getValue() * 240.0 / (points[1] - points[0]);
        if (std::isfinite(measured) && measured >= 20 && measured <= 400)
        { setDraftBpm(measured); calculated = true; }
        else calculated = false;
    }
    else calculated = false;
    changed();
}
bool GridCorrectionEditor::simpleAligned() const
{
    if (!validDraft || !countsConfirmed.getToggleState() || points[0] < 0 || points[1] <= points[0] || points[2] <= points[1]) return false;
    const double measured = barsAB.getValue() * 240.0 / (points[1] - points[0]);
    const double predicted = points[1] + barsBC.getValue() * 240.0 / measured;
    return measured >= 20 && measured <= 400 && std::abs(draft.bpm - measured) < 1.e-6
        && std::abs(draft.downbeat - points[0]) < 1.e-6 && std::abs(points[2] - predicted) <= .030;
}
void GridCorrectionEditor::updateSimpleControls()
{
    for (size_t i = 0; i < 3; ++i)
    {
        setPoint[i].setEnabled(i == 0 || points[i - 1] >= 0);
        visitPoint[i].setEnabled(points[i] >= 0);
        pointInfo[i].setText(points[i] < 0 ? "Not set" : juce::String(points[i], 3) + " s / downbeat", juce::dontSendNotification);
        setPoint[i].setColour(juce::TextButton::buttonColourId, points[i] >= 0 ? colour::orange.darker(.6f) : colour::raised);
    }
    barsAB.setEnabled(points[1] >= 0); barsBC.setEnabled(points[2] >= 0); countsConfirmed.setEnabled(points[2] >= 0);
    juce::String result;
    if (points[0] < 0) result = "Start: find a clear ONE under the centre marker, then Set A. A can be anywhere in the track.";
    else if (points[1] < 0) result = "A is pinned. Find a later ONE and Set B. A longer distance gives a more precise tempo.";
    else
    {
        const double ab = barsAB.getValue() * 240.0 / (points[1] - points[0]);
        if (ab < 20 || ab > 400) result = "A to B gives a tempo outside 20-400 BPM. Check the marks and complete bar count.";
        else if (points[2] < 0) result = "A to B: " + preciseBpmText(ab) + " BPM / suggested count. Set a still later C to check it.";
        else
        {
            const double bc = barsBC.getValue() * 240.0 / (points[2] - points[1]);
            const double error = points[2] - points[1] - barsBC.getValue() * 240.0 / ab;
            result = "A-B: " + preciseBpmText(ab) + " / B-C: " + preciseBpmText(bc) + " BPM. C is "
                + juce::String(std::abs(error) * 1000, 1) + " ms " + (error >= 0 ? "after" : "before") + " the grid. ";
            const bool gridMatches = validDraft && std::abs(draft.bpm - ab) < 1.e-6 && std::abs(draft.downbeat - points[0]) < 1.e-6;
            result += !gridMatches ? "Grid changed in other tools: Set B again to remeasure."
                : !countsConfirmed.getToggleState() ? "Check both bar counts."
                : simpleAligned() ? "Within 30 ms: listen, then save."
                : "C does not match: recheck marks and bar counts.";
        }
    }
    simpleResult.setText(result, juce::dontSendNotification);
    simpleResult.setColour(juce::Label::textColourId, simpleAligned() ? colour::text : colour::orange);
    if (page == 4) { save.setEnabled(simpleAligned()); rebuild.setEnabled(rebuild.isEnabled() && simpleAligned()); }
}
void GridCorrectionEditor::undo(bool redo)
{
    if (dragging != 0) return;
    auto& from = redo ? gridRedo : gridUndo; auto& to = redo ? gridUndo : gridRedo;
    if (from.empty()) return;
    const auto edit = from.back(); from.pop_back(); to.push_back(gridEdit());
    restoring = true; restoreGrid(edit); restoring = false;
}
double GridCorrectionEditor::precisionSeconds() const { return precision.getSelectedId() == 2 ? 0.010 : 0.001; }
double GridCorrectionEditor::secondAnchor() const
{
    const auto grid = draft.grid(*source);
    return validDraft && draft.downbeat >= 0 && grid.hasPulse() ? draft.downbeat + bars.getValue() * 240.0 / grid.bpm : -1;
}
double GridCorrectionEditor::timeAtX(float x) const
{
    return viewStart + (x - waveform.getX()) * std::min(viewSpan, source->duration()) / std::max(1, waveform.getWidth());
}
float GridCorrectionEditor::xAtTime(double time) const
{
    return (float) (waveform.getX() + (time - viewStart) * waveform.getWidth() / std::min(viewSpan, source->duration()));
}
juce::Rectangle<float> GridCorrectionEditor::handleBounds(double time, int handle) const
{
    const float left = juce::jlimit((float) waveform.getX(), (float) waveform.getRight() - 104, xAtTime(time) - 12);
    return {left, (float) waveform.getY() + (handle == 1 ? 3.0f : 29.0f), 104, 23};
}
void GridCorrectionEditor::setAnchor()
{
    if (readDraft().failed()) { changed(); return; }
    phase.setText(juce::String(cursor.getValue(), 8), false); calculated = false; selectedHandle = 1;
    moveGrid.setToggleState(true, juce::dontSendNotification);
    const auto grid = draft.grid(*source);
    if (grid.hasPulse())
    {
        const int available = (int) std::floor((cursor.getMaximum() - cursor.getValue()) * grid.bpm / 240.0);
        bars.setValue(std::max(1, std::min((int) bars.getValue(), available)), juce::dontSendNotification);
    }
    changed();
}
bool GridCorrectionEditor::alignSecond(double at)
{
    if (!validDraft || draft.downbeat < 0) return false;
    const double elapsed = at - draft.downbeat;
    const double value = elapsed > 0 ? bars.getValue() * 240.0 / elapsed : 0;
    if (!std::isfinite(at) || at < 0 || at > cursor.getMaximum() || !std::isfinite(value) || value < 20 || value > 400)
    {
        status.setText("Keep B after A, inside the source, with BPM between 20 and 400. Check the bars A to B count.", juce::dontSendNotification);
        return false;
    }
    setDraftBpm(value); calculated = true; selectedHandle = 2;
    spacing.setToggleState(true, juce::dontSendNotification); changed(); return true;
}
void GridCorrectionEditor::changeBars()
{
    // A measured B stays at its source time when the user corrects its bar identity.
    if (calculated && validDraft && draft.downbeat >= 0 && historyReady)
    {
        const double at = draft.downbeat + lastGrid.bars * 240.0 / draft.grid(*source).bpm;
        if (!alignSecond(at)) bars.setValue(lastGrid.bars, juce::dontSendNotification);
    }
    else changed();
}
void GridCorrectionEditor::updateAlignmentControls()
{
    const auto grid = draft.grid(*source);
    const bool anchored = validDraft && draft.downbeat >= 0;
    const double b = secondAnchor(); const bool inside = b >= 0 && b <= cursor.getMaximum();
    spacing.setEnabled(anchored && grid.hasPulse()); moveGrid.setEnabled(anchored);
    bars.setEnabled(anchored); goA.setEnabled(anchored); goB.setEnabled(inside);
    matchB.setEnabled(anchored);
    narrower.setEnabled(inside); wider.setEnabled(inside);
    earlier.setEnabled(validDraft && grid.hasBar()); later.setEnabled(validDraft && grid.hasBar());
    barEarlier.setEnabled(validDraft && grid.hasBar()); barLater.setEnabled(validDraft && grid.hasBar());
    undoGrid.setEnabled(!gridUndo.empty()); redoGrid.setEnabled(!gridRedo.empty());
    revertGrid.setEnabled(historyReady && !(gridEdit() == initialGrid));
    anchorInfo.setText(!anchored ? "A is not pinned. Click a known beat one in the waveform, then Set A / beat 1."
        : "A  " + juce::String(draft.downbeat, 4) + " s    |    B  " + (b >= 0 ? juce::String(b, 4) + " s" : juce::String("needs BPM"))
            + "    |    B is bar " + juce::String((int) bars.getValue() + 1) + " (" + juce::String((int) bars.getValue()) + " bars after A)."
            + (b >= 0 && !inside ? " B is outside the source: reduce the bar count or place A earlier."
                : spacing.getToggleState() ? " Drag B; A stays pinned." : " Drag A or the ruler; BPM stays fixed."), juce::dontSendNotification);
}
void GridCorrectionEditor::setPlaybackMedia(std::shared_ptr<const Media> media, const juce::String& error)
{
    if (closed) return;
    playback = std::move(media); play.setEnabled(playback != nullptr); keyByEar.setEnabled(playback != nullptr && (bool) openKey);
    if (error.isNotEmpty()) status.setText(error, juce::dontSendNotification);
    repaint(waveform);
}
void GridCorrectionEditor::setKeyByEarAction(std::function<void(std::shared_ptr<const Media>)> action)
{ openKey = std::move(action); keyByEar.setEnabled(playback != nullptr && (bool) openKey); }
void GridCorrectionEditor::setReviewedKey(int value)
{ key.setSelectedId(value + 2, juce::dontSendNotification); changed(); if (refreshFailures) refreshFailures(); }
void GridCorrectionEditor::setRetryFailures(const std::vector<RetryIssue>& rows, const juce::String& result, std::function<void(juce::String)> open)
{
    failures.onChange = {}; failures.clear(juce::dontSendNotification); failureRows = rows;
    for (size_t i = 0; i < rows.size(); ++i) failures.addItem(shortUiMessage(rows[i].media->name, 10, 55) + " - " + rows[i].reason, (int) i + 1);
    failures.setTextWhenNothingSelected(rows.empty() ? "No retry failures" : "Retry failures (" + juce::String((int) rows.size()) + ")");
    failures.setEnabled(!rows.empty()); lastRetry.setText(result, juce::dontSendNotification); resized();
    failures.onChange = [this, rows, open]
    {
        const int index = failures.getSelectedItemIndex(); failures.setSelectedId(0, juce::dontSendNotification);
        if (index < 0 || index >= (int) rows.size() || rows[(size_t) index].media->id == source->id) return;
        const auto next = rows[(size_t) index].media->id;
        const bool valid = readDraft().wasOk(); const auto saved = SourceCorrection::from(*source);
        const bool dirty = !valid || saved.bpm != draft.bpm || saved.downbeat != draft.downbeat || saved.key != draft.key;
        if (!dirty) { open(next); return; }
        auto* dialog = new juce::AlertWindow("Unsaved corrections", "Save or discard your changes before opening another track.", juce::MessageBoxIconType::NoIcon, this);
        dialog->addButton("Save and open", 1); dialog->getButton(0)->setEnabled(valid);
        dialog->addButton("Discard and open", 2); dialog->addButton("Stay here", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        dialog->enterModalState(true, juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<GridCorrectionEditor>(this), open, next](int answer)
        {
            if (!safe || safe->closed || answer == 0) return;
            if (answer == 1)
            {
                const auto parsed = safe->readDraft(); if (parsed.failed()) return;
                const auto result = safe->apply(safe->draft, false);
                if (result.failed()) { safe->status.setText(result.getErrorMessage(), juce::dontSendNotification); return; }
            }
            open(next);
        }), true);
    };
}
void GridCorrectionEditor::pauseAudition() { const juce::ScopedLock guard(engine.lock); engine.gridAudition.stop(); }
void GridCorrectionEditor::closePreview()
{
    closed = true; stopTimer(); const juce::ScopedLock guard(engine.lock); engine.gridAudition.stop(); playback.reset();
}
void GridCorrectionEditor::dismiss()
{
    closePreview(); if (auto* window = findParentComponentOfClass<juce::DialogWindow>()) window->closeButtonPressed();
}
void GridCorrectionEditor::togglePlay()
{
    bool active = false; { const juce::ScopedLock guard(engine.lock); active = engine.gridAudition.active(); }
    if (!active && (!validDraft || closed)) return;
    if (!active && beforeAudition)
    { const auto result = beforeAudition(); if (result.failed()) { status.setText(result.getErrorMessage(), juce::dontSendNotification); return; } }
    const juce::ScopedLock guard(engine.lock);
    if (engine.gridAudition.active()) { engine.gridAudition.stop(); play.setButtonText("Play source"); return; }
    if (!validDraft || closed) return;
    engine.endTransitionRehearsal();
    engine.setPlaying(false);
    const auto result = engine.gridAudition.start(playback, draft.grid(*source), cursor.getValue(), sourceAudio.getToggleState(), click.getToggleState(), (float) level.getValue());
    if (result.failed()) status.setText(result.getErrorMessage(), juce::dontSendNotification);
    else play.setButtonText("Stop source");
}
void GridCorrectionEditor::setCursor(double time, bool restart)
{
    const double at = juce::jlimit(cursor.getMinimum(), cursor.getMaximum(), time);
    updating = true; cursor.setValue(at, juce::dontSendNotification); updating = false;
    viewStart = page == 4 ? at - std::min(viewSpan, source->duration()) * .5
        : juce::jlimit(0.0, std::max(0.0, source->duration() - viewSpan), at - viewSpan * 0.35);
    const juce::ScopedLock guard(engine.lock);
    if (restart && engine.gridAudition.active()) engine.gridAudition.start(playback, draft.grid(*source), at, sourceAudio.getToggleState(), click.getToggleState(), (float) level.getValue());
    repaint();
}
void GridCorrectionEditor::changeRate(double factor)
{
    if (readDraft().failed()) return;
    const double value = draft.grid(*source).bpm * factor;
    if (value < 20 || value > 400) { status.setText("The corrected BPM must remain between 20 and 400.", juce::dontSendNotification); return; }
    calculated = false; setDraftBpm(value); changed();
}
void GridCorrectionEditor::adjustTempo(double delta)
{
    if (readDraft().failed()) return;
    const double value = draft.grid(*source).bpm + delta;
    if (value < 20 || value > 400) { status.setText("Source BPM must remain between 20 and 400.", juce::dontSendNotification); return; }
    calculated = false; setDraftBpm(value); changed();
}
void GridCorrectionEditor::nudge(double seconds)
{
    if (readDraft().failed()) return;
    const auto grid = draft.grid(*source);
    if (!grid.hasBar()) { status.setText("Choose beat one first.", juce::dontSendNotification); return; }
    moveGrid.setToggleState(true, juce::dontSendNotification);
    phase.setText(juce::String(juce::jlimit(0.0, cursor.getMaximum(), grid.downbeatOrigin + seconds), 8), false); changed();
}
void GridCorrectionEditor::timerCallback()
{
    if (waveformDetailPending) repaint(waveform);
    const juce::ScopedLock guard(engine.lock);
    play.setButtonText(engine.gridAudition.active() ? "Stop source" : "Play source");
    if (engine.gridAudition.active())
    {
        if (dragging == 0) setCursor(engine.gridAudition.position(), false);
        else { updating = true; cursor.setValue(engine.gridAudition.position(), juce::dontSendNotification); updating = false; repaint(); }
    }
}
bool GridCorrectionEditor::keyPressed(const juce::KeyPress& keyPress)
{
    if (keyPress == juce::KeyPress::escapeKey && dragging != 0)
    {
        if (dragging == 5) { dragging = 0; setCursor(dragAnchor, false); return true; }
        if (dragging >= 3) viewStart = dragViewStart;
        dragging = 0; restoring = true; restoreGrid(dragBefore); restoring = false; return true;
    }
    if (bpm.hasKeyboardFocus(true) || phase.hasKeyboardFocus(true) || bars.hasKeyboardFocus(true)
        || barsAB.hasKeyboardFocus(true) || barsBC.hasKeyboardFocus(true)) return false;
    if (keyPress.getModifiers().isCommandDown() && keyPress.getKeyCode() == 'Z') { undo(keyPress.getModifiers().isShiftDown()); return true; }
    if ((page!=0) && (keyPress == juce::KeyPress::leftKey || keyPress == juce::KeyPress::rightKey))
    {
        const double delta = (keyPress == juce::KeyPress::leftKey ? -1 : 1) * (page == 4 ? .001 : precisionSeconds());
        if (page == 4) { pauseAudition(); setCursor(cursor.getValue() + delta, false); return true; }
        if (spacing.getToggleState()) { if (validDraft) alignSecond(secondAnchor() + delta); }
        else nudge(delta);
        return true;
    }
    if (keyPress == juce::KeyPress::spaceKey && !bpm.hasKeyboardFocus(true) && !phase.hasKeyboardFocus(true)) { togglePlay(); return true; }
    return false;
}
void GridCorrectionEditor::mouseDown(const juce::MouseEvent& event)
{
    if (!event.mods.isLeftButtonDown() || !waveform.contains(event.getPosition())) return;
    grabKeyboardFocus();
    if (page == 4)
    {
        pauseAudition(); dragging = 5; selectedHandle = 0;
        dragStartX = dragLastX = event.position.x; dragDelta = 0; dragAnchor = cursor.getValue();
        dragSecondsPerPixel = std::min(viewSpan, source->duration()) / waveform.getWidth();
        return;
    }
    const bool ruler = event.y < waveform.getY() + 56;
    if (ruler && page!=0 && validDraft && draft.downbeat >= 0)
    {
        const bool atA = draft.downbeat >= viewStart && draft.downbeat <= viewStart + std::min(viewSpan, source->duration())
            && handleBounds(draft.downbeat, 1).contains(event.position);
        const double b = secondAnchor();
        const bool atB = page!=1 && b >= viewStart && b <= viewStart + std::min(viewSpan, source->duration()) && handleBounds(b, 2).contains(event.position);
        const bool stretch = !atA && (atB || spacing.getToggleState());
        dragBefore = gridEdit();
        if (stretch && !draft.grid(*source).hasPulse()) return;
        dragging = stretch ? 2 : 1;
        if (stretch && !atB)
        {
            const int count = (int) std::llround(draft.grid(*source).beatAt(timeAtX(event.position.x)) / 4.0);
            if (count < 1 || count > 256) { dragging = 0; return; }
            bars.setValue(count, juce::dontSendNotification); calculated = false; changed();
        }
        selectedHandle = dragging;
        (stretch ? spacing : moveGrid).setToggleState(true, juce::dontSendNotification);
        dragStartX = dragLastX = event.position.x; dragDelta = 0; dragViewStart = viewStart; dragAnchor = draft.downbeat; dragB = secondAnchor();
        dragSecondsPerPixel = std::min(viewSpan, source->duration()) / waveform.getWidth();
        updateAlignmentControls(); repaint(); return;
    }
    if (!ruler && validDraft && draft.grid(*source).hasBar())
    {
        // A click still auditions. Only a deliberate drag changes phase, and it
        // never seeks/restarts source audio or alters the programme cursor.
        dragBefore = gridEdit(); dragging = 3; selectedHandle = 0;
        dragStartX = dragLastX = event.position.x; dragDelta = 0; dragViewStart = viewStart;
        dragAnchor = draft.grid(*source).downbeatOrigin;
        dragSecondsPerPixel = std::min(viewSpan, source->duration()) / waveform.getWidth();
        return;
    }
    selectedHandle = 0; setCursor(timeAtX(event.position.x));
}
void GridCorrectionEditor::mouseDrag(const juce::MouseEvent& event)
{
    if (dragging == 0) return;
    if (dragging == 3)
    {
        if (std::abs(event.position.x - dragStartX) < 3) return;
        dragging = 4; selectedHandle = 1; moveGrid.setToggleState(true, juce::dontSendNotification);
    }
    // Integrate movement so pressing/releasing Shift mid-drag never jumps.
    dragDelta += (event.position.x - dragLastX) * dragSecondsPerPixel * (event.mods.isShiftDown() ? .1 : 1);
    dragLastX = event.position.x;
    const double delta = dragDelta;
    if (dragging == 5) { setCursor(dragAnchor - delta, false); return; }
    if (dragging == 2) alignSecond(dragB + delta);
    else
    {
        const double anchor = juce::jlimit(0.0, cursor.getMaximum(), dragAnchor + (dragging == 4 ? -delta : delta));
        if (dragging == 4) viewStart = dragViewStart + anchor - dragAnchor;
        phase.setText(juce::String(anchor, 8), false); changed();
    }
}
void GridCorrectionEditor::mouseUp(const juce::MouseEvent& event)
{
    if (dragging == 0) return;
    if (dragging == 5)
    {
        const bool clicked = std::abs(event.position.x - dragStartX) < 3 && std::abs(dragDelta) < 1.e-12;
        dragging = 0; if (clicked) setCursor(timeAtX(event.position.x));
        repaint(); return;
    }
    const bool clicked = dragging == 3;
    dragging = 0;
    if (clicked) { setCursor(timeAtX(event.position.x)); return; }
    if (!(gridEdit() == dragBefore)) { gridUndo.push_back(dragBefore); gridRedo.clear(); }
    updateAlignmentControls(); repaint();
}
void GridCorrectionEditor::paint(juce::Graphics& g)
{
    g.fillAll(colour::background); g.setColour(colour::panel); g.fillRect(waveform);
    if (waveform.isEmpty()) return;
    const double span = std::min(viewSpan, source->duration()); const float mid = waveform.getCentreY() + 24.0f;
    const auto& displayed = playback ? *playback : *source;
    waveformDetailPending = !displayed.hasPlayback();
    for (int x = 0; x < waveform.getWidth(); ++x)
    {
        // A grid tick and its waveform column describe the same pixel centre.
        const double a = viewStart + span * (x - .5) / waveform.getWidth(), b = viewStart + span * (x + .5) / waveform.getWidth();
        if (!displayed.hasPlayback()) continue;
        if (b < 0 || a >= source->duration()) continue;
        const auto range = displayed.waveformRange(std::max(0.0, a), std::min(source->duration(), b));
        if (!range.ready) { waveformDetailPending = true; continue; }
        const float height = juce::jlimit(0.0f, (waveform.getHeight() - 104) * 0.5f, range.peak * (waveform.getHeight() - 104) * 0.5f);
        const float density = range.peak > 0 ? juce::jlimit(0.f, 1.f, range.rms / range.peak) : 0;
        const auto tint = juce::Colour(0xff54acc2).interpolatedWith(juce::Colour(0xffaa91d2), density);
        g.setColour(tint.withAlpha(.7f)); g.drawVerticalLine(waveform.getX() + x, mid - height, mid + height);
        const float core = height * density;
        g.setColour(tint.brighter(.2f)); g.drawVerticalLine(waveform.getX() + x, mid - core, mid + core);
    }
    const auto grid = draft.grid(*source);
    if (validDraft && grid.hasPulse())
    {
        const double pixels = waveform.getWidth() * 60 / grid.bpm / span;
        const double stride = pixels >= 6 ? 1 : 4 * std::ceil(6 / (pixels * 4));
        for (double beat = std::ceil(grid.beatAt(viewStart) / stride) * stride; grid.timeAtBeat(beat) <= viewStart + span; beat += stride)
        {
            if (grid.timeAtBeat(beat) < 0 || grid.timeAtBeat(beat) >= source->duration()) continue;
            const int x = waveform.getX() + juce::roundToInt((grid.timeAtBeat(beat) - viewStart) / span * waveform.getWidth());
            const bool bar = grid.hasBar() && std::abs(std::remainder(beat, 4)) < 0.1;
            g.setColour(bar ? colour::orange : juce::Colour(0xff63808c)); g.drawVerticalLine(x, (float) waveform.getY() + 56, (float) waveform.getBottom() - 22);
            if (bar) { g.setColour(colour::orange.withAlpha(.35f)); g.drawVerticalLine(x - 1, (float) waveform.getY() + 56, (float) waveform.getBottom() - 22); }
            if (pixels > 25 && grid.hasBar())
            {
                const int withinBar = (int) (std::fmod(std::fmod(beat, 4) + 4, 4)) + 1;
                const auto label = juce::String((int)std::floor(beat / 4) + 1) + "." + juce::String(withinBar);
                g.setColour(bar ? colour::orange : colour::text); g.drawText(label, x + 3, waveform.getY() + 56, (int)std::min(76., pixels - 5), 18, juce::Justification::left);
            }
        }
    }
    g.setColour(colour::raised); g.fillRect(waveform.withHeight(56));
    g.setColour(colour::text); g.setFont(juce::FontOptions(12));
    g.drawText(page == 4 ? "Drag to find downbeat / Shift: fine / arrows: 1 ms / Space: play or pause"
        : grid.hasBar() ? "Waveform: drag to align / Shift: fine / click to listen" : "Click the waveform to choose where to listen", waveform.withHeight(28).reduced(6, 0), juce::Justification::right);
    const auto handle = [&](double time, int index, const juce::String& text, juce::Colour tint)
    {
        if (time < viewStart || time > viewStart + span) return;
        const float x = xAtTime(time);
        const auto bounds = handleBounds(time, index);
        g.setColour(tint); g.fillRect(x - 1.0f, bounds.getBottom(), 2.0f, waveform.getBottom() - bounds.getBottom());
        g.fillRoundedRectangle(bounds, 3);
        // The stem at x is the exact drag position, even when the label is clamped at an edge.
        g.setColour(colour::background); g.drawText(text, bounds.reduced(5, 0), juce::Justification::centredLeft);
    };
    if (page == 4)
    {
        for (size_t i = 0; i < 3; ++i)
            if (points[i] >= 0) handle(points[i], 2, juce::String::charToString((juce::juce_wchar) ('A' + (int)i)) + " / marked", colour::tracks[2]);
        const float centre = waveform.getX() + waveform.getWidth() * .5f;
        g.setColour(colour::orange.withAlpha(.18f)); g.fillRect(centre - 4, (float)waveform.getY() + 28, 8.f, (float)waveform.getHeight() - 28);
        g.setColour(colour::text); g.fillRect(centre - 1, (float)waveform.getY() + 28, 2.f, (float)waveform.getHeight() - 28);
        juce::Path tip; tip.addTriangle(centre - 7, (float)waveform.getY() + 28, centre + 7, (float)waveform.getY() + 28, centre, (float)waveform.getY() + 38);
        g.setColour(colour::orange); g.fillPath(tip);
    }
    else if (page!=0 && validDraft && draft.downbeat >= 0)
    {
        handle(draft.downbeat, 1, page==3 ? "A  /  move grid" : "Beat 1", colour::orange);
        const double b = secondAnchor(); if (page!=1 && b >= 0) handle(b, 2, page==3 ? "B  /  spacing" : "Check beat", colour::tracks[2]);
    }
    g.setColour(colour::text); const int x = juce::roundToInt(xAtTime(cursor.getValue()));
    if (page != 4 && waveform.contains(x, waveform.getCentreY())) g.drawVerticalLine(x, (float) waveform.getY() + 56, (float) waveform.getBottom());
    if (waveformDetailPending)
    {
        g.setColour(colour::muted);
        g.drawText(displayed.original && displayed.original->failed() ? "Waveform unavailable. Reopen the source." : "Loading waveform detail...",
                   waveform.reduced(8).withTrimmedTop(72).removeFromTop(20), juce::Justification::left);
    }
    g.setColour(colour::text); g.drawText(juce::String(viewStart, 3) + " s", waveform.withHeight(18).translated(4, waveform.getHeight() - 20), juce::Justification::left);
    const auto provenance = !grid.hasBar() ? juce::String("Pulse only / beat one not set")
        : draft.downbeat < 0 ? juce::String("Detected 4/4") : draft.downbeat == source->manualBeatOrigin ? juce::String("Saved 4/4") : juce::String("Draft 4/4");
    g.setFont(juce::FontOptions(11)); g.setColour(colour::muted);
    g.drawText(provenance + " / bar.beat / orange: beat 1 / blue: other beats / waveform colour: body energy",
               waveform.withHeight(18).translated(-6, waveform.getHeight() - 20).withTrimmedLeft(90), juce::Justification::right);
}
void GridCorrectionEditor::resized()
{
    const bool advanced = page==3;
    const bool simple = page==4;
    for (size_t i = 0; i < 3; ++i) { setPoint[i].setVisible(simple); visitPoint[i].setVisible(simple); pointInfo[i].setVisible(simple); }
    for (auto* c : std::initializer_list<juce::Component*> {&barsAB, &barsBC, &countAB, &countBC, &countsConfirmed, &simpleResult}) c->setVisible(simple);
    tempoPage.setVisible(!simple); beatPage.setVisible(!simple); checkPage.setVisible(!simple);
    advancedPage.setButtonText(simple ? "More tools / Advanced" : "Advanced");
    for (auto* c : std::initializer_list<juce::Component*> {&bpm, &bpmLabel, &phase, &phaseLabel, &key, &keyLabel, &keyByEar,
        &slower, &faster, &half, &twice, &tap, &resetTap, &automatic, &beatOne, &moveGrid, &spacing, &precision,
        &earlier, &later, &narrower, &wider, &bars, &barsLabel, &goA, &goB, &matchB, &revertGrid, &anchorInfo,
        &analysisDetails, &failures, &lastRetry, &quiet}) c->setVisible(advanced);
    barEarlier.setVisible(page==1); barLater.setVisible(page==1);
    auto r = getLocalBounds().reduced(14); auto title = r.removeFromTop(26);
    if (advanced) failures.setBounds(title.removeFromRight(350)); heading.setBounds(title);
    if (advanced) lastRetry.setBounds(lastRetry.getText().isNotEmpty() ? r.removeFromTop(25) : juce::Rectangle<int>());
    r.removeFromTop(6); auto tabs = r.removeFromTop(32);
    simplePage.setBounds(tabs.removeFromLeft(190).reduced(2,0));
    for (auto* button : {&tempoPage, &beatPage, &checkPage, &advancedPage}) if (button->isVisible()) button->setBounds(tabs.removeFromLeft(simple ? 220 : 180).reduced(2,0));
    instructions.setBounds(r.removeFromTop(42));
    if (simple)
    {
        auto cards = r.removeFromTop(57);
        const int width = cards.getWidth() / 3;
        for (size_t i = 0; i < 3; ++i)
        {
            auto card = cards.removeFromLeft(width).reduced(3,0); auto buttons = card.removeFromTop(32);
            setPoint[i].setBounds(buttons.removeFromLeft(115)); buttons.removeFromLeft(5); visitPoint[i].setBounds(buttons);
            pointInfo[i].setBounds(card);
        }
        r.removeFromTop(4); auto counts = r.removeFromTop(30);
        countAB.setBounds(counts.removeFromLeft(90)); barsAB.setBounds(counts.removeFromLeft(112)); counts.removeFromLeft(16);
        countBC.setBounds(counts.removeFromLeft(90)); barsBC.setBounds(counts.removeFromLeft(112)); counts.removeFromLeft(20);
        countsConfirmed.setBounds(counts);
        simpleResult.setBounds(r.removeFromTop(36)); auto summary = r.removeFromTop(28);
        undoGrid.setBounds(summary.removeFromRight(110).reduced(2,0)); redoGrid.setBounds(summary.removeFromRight(110).reduced(2,0));
        effectiveTempo.setBounds(summary); r.removeFromTop(6);
    }
    else if (!advanced)
    {
        auto controls = r.removeFromTop(32);
        const auto place = [&](juce::Component& c, int width) { c.setVisible(true); c.setBounds(controls.removeFromLeft(width).reduced(3,0)); };
        if (page==0)
        {
            place(bpmLabel,45); place(bpm,120); place(tap,110); place(resetTap,110); place(half,110); place(twice,110);
        }
        else if (page==1)
        {
            place(beatOne,155); place(earlier,100); place(later,100); place(precision,130); place(barEarlier,150); place(barLater,150);
        }
        else
        {
            place(barsLabel,115); place(bars,95); place(goA,140); place(goB,155); place(matchB,190);
        }
        r.removeFromTop(8); auto summary = r.removeFromTop(30);
        undoGrid.setBounds(summary.removeFromRight(110).reduced(2,0)); redoGrid.setBounds(summary.removeFromRight(110).reduced(2,0));
        effectiveTempo.setBounds(summary); r.removeFromTop(10);
    }
    else
    {
    auto labels = r.removeFromTop(20); bpmLabel.setBounds(labels.removeFromLeft(210)); phaseLabel.setBounds(labels.removeFromLeft(385)); keyLabel.setBounds(labels);
    auto fields = r.removeFromTop(27); bpm.setBounds(fields.removeFromLeft(200)); fields.removeFromLeft(10); phase.setBounds(fields.removeFromLeft(375)); fields.removeFromLeft(10);
    keyByEar.setBounds(fields.removeFromRight(118).reduced(2, 0)); key.setBounds(fields);
    r.removeFromTop(6); auto fine = r.removeFromTop(28);
    analysisDetails.setBounds(fine.removeFromRight(140));
    slower.setBounds(fine.removeFromLeft(105).reduced(2, 0)); faster.setBounds(fine.removeFromLeft(105).reduced(2, 0)); effectiveTempo.setBounds(fine);
    r.removeFromTop(5); auto actions = r.removeFromTop(28);
    beatOne.setBounds(actions.removeFromLeft(140).reduced(2, 0));
    for (auto* button : {&half, &twice, &tap, &resetTap, &automatic}) button->setBounds(actions.removeFromLeft(110).reduced(2, 0));
    r.removeFromTop(8); auto tools = r.removeFromTop(29);
    moveGrid.setBounds(tools.removeFromLeft(125).reduced(2, 0)); spacing.setBounds(tools.removeFromLeft(135).reduced(2, 0));
    tools.removeFromLeft(10); precision.setBounds(tools.removeFromLeft(120)); tools.removeFromLeft(8);
    for (auto* button : {&earlier, &later, &narrower, &wider}) button->setBounds(tools.removeFromLeft(100).reduced(2, 0));
    r.removeFromTop(5); auto anchors = r.removeFromTop(30); barsLabel.setBounds(anchors.removeFromLeft(90)); bars.setBounds(anchors.removeFromLeft(100)); anchors.removeFromLeft(12);
    for (auto* button : {&goA, &goB, &matchB, &undoGrid, &redoGrid, &revertGrid}) button->setBounds(anchors.removeFromLeft(button == &matchB ? 150 : 110).reduced(2, 0));
    anchorInfo.setBounds(r.removeFromTop(38)); r.removeFromTop(4);
    }
    auto footer = r.removeFromBottom(32); cancel.setBounds(footer.removeFromRight(110)); footer.removeFromRight(8); rebuild.setBounds(footer.removeFromRight(200)); footer.removeFromRight(8); save.setBounds(footer.removeFromRight(200));
    applyNotice.setBounds(r.removeFromBottom(26)); r.removeFromBottom(4); status.setBounds(r.removeFromBottom(38));
    auto monitor = r.removeFromBottom(32); play.setBounds(monitor.removeFromLeft(125)); click.setBounds(monitor.removeFromLeft(125)); sourceAudio.setBounds(monitor.removeFromLeft(140)); level.setBounds(monitor);
    r.removeFromBottom(6); auto checks = r.removeFromBottom(29);
    for (auto* button : {&beginning, &middle, &end, &quiet}) if (button->isVisible()) button->setBounds(checks.removeFromLeft(140).reduced(2, 0)); zoom.setBounds(checks);
    cursor.setBounds(r.removeFromBottom(30)); waveform = r;
}
GridCorrectionWindow::GridCorrectionWindow(GridCorrectionEditor* editor) : AlloyPopupWindow("Source beat grid / corrections", colour::background, true)
{
    setUsingNativeTitleBar(false); setContentOwned(editor, true); setResizable(false, false);
    centreWithSize(editor->getWidth(), editor->getHeight());
}
void GridCorrectionWindow::closeButtonPressed()
{
    if (auto* editor = dynamic_cast<GridCorrectionEditor*>(getContentComponent())) editor->closePreview();
    setVisible(false); exitModalState(0);
}
}
