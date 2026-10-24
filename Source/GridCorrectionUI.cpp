#include "GridCorrectionUI.h"

namespace mixworks
{
GridCorrectionEditor::GridCorrectionEditor(Engine& e, std::shared_ptr<const Media> media, const juce::String& details,
                                           std::function<juce::Result(const SourceCorrection&, bool)> commit)
    : engine(e), source(std::move(media)), apply(std::move(commit)), draft(SourceCorrection::from(*source))
{
    setSize(900, 600); setWantsKeyboardFocus(true);
    setTooltip("Click the source waveform to move the audition cursor. Review the beat and downbeat grid at the beginning, middle and end before applying corrections.");
    for (auto* component : std::initializer_list<juce::Component*> {&heading, &instructions, &effectiveTempo, &slower, &faster, &rebuild, &info, &bpm, &phase, &key, &zoom, &bpmLabel, &phaseLabel, &keyLabel, &status,
        &half, &twice, &tap, &resetTap, &beatOne, &earlier, &later, &automatic, &beginning, &middle, &end, &quiet, &play, &save, &cancel, &click, &sourceAudio, &cursor, &level})
        addAndMakeVisible(component);
    heading.setText("SOURCE BEAT GRID  /  " + source->name, juce::dontSendNotification);
    info.setVisible(false); heading.setTooltip(details);
    instructions.setVisible(false);
    instructions.setFont(juce::FontOptions(12));
    bpmLabel.setText("BPM", juce::dontSendNotification);
    phaseLabel.setText("Beat one (seconds)", juce::dontSendNotification);
    keyLabel.setText("Key", juce::dontSendNotification);
    bpm.setText(draft.bpm > 0 ? juce::String(draft.bpm, 4) : juce::String(), false);
    phase.setText(draft.downbeat >= 0 ? juce::String(draft.downbeat, 4) : juce::String(), false);
    bpm.setName("BPM override"); phase.setName("Downbeat anchor in source seconds");
    key.addItem("Automatic key", 1); for (int i = 0; i < 24; ++i) key.addItem(keyNoteName(i) + " / " + keyCamelot(i), i + 2);
    key.setSelectedId(draft.key + 2, juce::dontSendNotification);
    zoom.addItemList({"4 seconds", "12 seconds", "32 seconds", "120 seconds"}, 1); zoom.setSelectedId(2, juce::dontSendNotification);
    zoom.onChange = [this] { const double spans[] {4, 12, 32, 120}; viewSpan = spans[zoom.getSelectedId() - 1]; setCursor(cursor.getValue(), false); };
    cursor.setRange(0, std::max(0.0001, source->duration() - 1 / source->sampleRate), 0.0001);
    cursor.setSliderStyle(juce::Slider::LinearHorizontal); cursor.setTextBoxStyle(juce::Slider::TextBoxRight, false, 100, 24); cursor.setName("Source cursor in seconds");
    cursor.onValueChange = [this] { if (!updating) setCursor(cursor.getValue()); };
    level.setRange(-36, -3, 0.1); level.setValue(-12, juce::dontSendNotification); level.setTextValueSuffix(" dB");
    level.setSliderStyle(juce::Slider::LinearHorizontal); level.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 24); level.setName("Source audition level");
    click.setToggleState(true, juce::dontSendNotification); sourceAudio.setToggleState(true, juce::dontSendNotification);
    bpm.setTooltip("Override constant source BPM. Leave blank to use detection; Apply correction commits the draft.");
    phase.setTooltip("Source time of any known downbeat, in seconds. The 4/4 bar grid extends both ways from it. Leave blank for inferred or unknown phase.");
    key.setTooltip("Choose a manual musical key, or Automatic key to use detection.");
    zoom.setTooltip("Choose how many source seconds the waveform shows around the cursor.");
    cursor.setTooltip("Move the source audition cursor in seconds. This does not move the mix cursor.");
    level.setTooltip("Set audition volume for source audio and grid click. This does not change segment or export gain.");
    click.setTooltip("Include the beat-grid click in source audition to check alignment.");
    sourceAudio.setTooltip("Include the original source audio in audition. Turn off to hear only the grid click.");
    half.setTooltip("Halve the draft BPM to correct double-time detection.");
    twice.setTooltip("Double the draft BPM to correct half-time detection.");
    tap.setTooltip("Tap at least three beats to estimate the draft BPM. Listen across the track before applying.");
    resetTap.setTooltip("Clear the tap history before starting a new BPM estimate.");
    beatOne.setTooltip("Set the draft downbeat anchor at the current source cursor.");
    automatic.setTooltip("Clear draft BPM, downbeat and key overrides and use detected values after applying.");
    beginning.setTooltip("Move source audition to the beginning to check the grid.");
    middle.setTooltip("Move source audition to the middle to check timing drift.");
    end.setTooltip("Move source audition near the end to check timing drift.");
    quiet.setTooltip("Suggest a quiet passage from waveform RMS for grid review. This is an approximate location.");
    play.setTooltip("Play or pause this source audition. Wait for source audio preparation if unavailable.");
    save.setTooltip("Save BPM, downbeat and key for this source. Existing timeline audio and placement keep their prepared clock. Use Save + rebuild timeline to apply corrections to the mix.");
    rebuild.setName("Save corrections and rebuild timeline");
    rebuild.setTooltip("Save corrections, then Beatmatch the whole mix using the current transition preset and length. This can move tracks and replaces manual transition placement. Timeline rebuilding is undoable; saved source corrections remain. Unlock all transitions first.");
    slower.setName("Decrease source tempo by 0.01 BPM"); faster.setName("Increase source tempo by 0.01 BPM");
    slower.setTooltip("Decrease source BPM by 0.01 to fine tune steady timing drift. The downbeat anchor stays fixed until saved. You can also type an exact BPM.");
    faster.setTooltip("Increase source BPM by 0.01 to fine tune steady timing drift. The downbeat anchor stays fixed until saved. You can also type an exact BPM.");
    slower.onClick = [this] { adjustTempo(-0.01); }; faster.onClick = [this] { adjustTempo(0.01); };
    cancel.setTooltip("Close source-grid review without applying the draft corrections.");
    bpm.onTextChange = phase.onTextChange = key.onChange = click.onClick = sourceAudio.onClick = level.onValueChange = [this] { changed(); };
    half.onClick = [this] { changeRate(0.5); }; twice.onClick = [this] { changeRate(2); };
    tap.onClick = [this]
    {
        if (auto value = taps.tap(juce::Time::getMillisecondCounterHiRes() / 1000)) { bpm.setText(juce::String(*value, 4), false); changed(); }
        else status.setText("Tap three or more steady beats.", juce::dontSendNotification);
    };
    resetTap.onClick = [this] { taps.reset(); status.setText("Tap tempo reset.", juce::dontSendNotification); };
    beatOne.onClick = [this] { phase.setText(juce::String(cursor.getValue(), 4), false); changed(); };
    earlier.onClick = [this] { nudge(-0.001); }; later.onClick = [this] { nudge(0.001); };
    earlier.setTooltip("Move the source grid 1 ms earlier; hold Shift for 10 ms."); later.setTooltip("Move the source grid 1 ms later; hold Shift for 10 ms.");
    automatic.onClick = [this] { bpm.setText({}, false); phase.setText({}, false); key.setSelectedId(1, juce::dontSendNotification); taps.reset(); changed(); };
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
    rebuild.onClick = [this] { this->commit(true); };
    status.setColour(juce::Label::textColourId, colour::muted);
    if (source->hasPlayback()) playback = source;
    play.setEnabled(playback != nullptr); changed(); setCursor(0, false); startTimerHz(25);
}
GridCorrectionEditor::~GridCorrectionEditor() { closePreview(); }
juce::Result GridCorrectionEditor::readDraft()
{
    return SourceCorrection::parse(bpm.getText(), phase.getText(), key.getSelectedId() - 2, source->duration(), draft);
}
void GridCorrectionEditor::commit(bool rebuildTimeline)
{
    const auto result = readDraft(); if (result.failed()) { status.setText(result.getErrorMessage(), juce::dontSendNotification); return; }
    { const juce::ScopedLock guard(engine.lock); engine.gridAudition.stop(); }
    const auto committed = apply(draft, rebuildTimeline);
    if (committed.failed()) { status.setText(committed.getErrorMessage(), juce::dontSendNotification); return; }
    dismiss();
}
void GridCorrectionEditor::changed()
{
    const auto result = readDraft(); validDraft = result.wasOk(); save.setEnabled(validDraft);
    const juce::ScopedLock guard(engine.lock);
    const bool inTimeline = std::any_of(engine.project.clips.begin(), engine.project.clips.end(), [this](const auto& clip)
    { return clip.media && (clip.media == source || (clip.media->file == source->file && clip.media->analysisSource == source->analysisSource)); });
    rebuild.setEnabled(validDraft && inTimeline && !engine.project.hasLockedTransitions());
    effectiveTempo.setText(validDraft ? "Tempo: " + juce::String(draft.grid(*source).bpm, 4) + " BPM" + (draft.bpm > 0 ? " (manual)" : " (detected)") : "Correct the invalid fields before saving.", juce::dontSendNotification);
    if (!validDraft) { engine.gridAudition.stop(); status.setText(result.getErrorMessage(), juce::dontSendNotification); }
    else
    {
        const auto grid = draft.grid(*source); engine.gridAudition.configure(grid, sourceAudio.getToggleState(), click.getToggleState(), (float) level.getValue());
        status.setText(!grid.hasPulse() ? "Tap a steady beat to set the tempo."
            : grid.hasBar() ? "Beat one is set."
            : "Choose beat one.", juce::dontSendNotification);
    }
    repaint();
}
void GridCorrectionEditor::setPlaybackMedia(std::shared_ptr<const Media> media, const juce::String& error)
{
    if (closed) return;
    playback = std::move(media); play.setEnabled(playback != nullptr);
    if (error.isNotEmpty()) status.setText(error, juce::dontSendNotification);
}
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
    viewStart = juce::jlimit(0.0, std::max(0.0, source->duration() - viewSpan), at - viewSpan * 0.35);
    const juce::ScopedLock guard(engine.lock);
    if (restart && engine.gridAudition.active()) engine.gridAudition.start(playback, draft.grid(*source), at, sourceAudio.getToggleState(), click.getToggleState(), (float) level.getValue());
    repaint();
}
void GridCorrectionEditor::changeRate(double factor)
{
    if (readDraft().failed()) return;
    const double value = draft.grid(*source).bpm * factor;
    if (value < 20 || value > 400) { status.setText("The corrected BPM must remain between 20 and 400.", juce::dontSendNotification); return; }
    bpm.setText(juce::String(value, 4), false); changed();
}
void GridCorrectionEditor::adjustTempo(double delta)
{
    if (readDraft().failed()) return;
    const double value = draft.grid(*source).bpm + delta;
    if (value < 20 || value > 400) { status.setText("Source BPM must remain between 20 and 400.", juce::dontSendNotification); return; }
    bpm.setText(juce::String(value, 4), false); changed();
}
void GridCorrectionEditor::nudge(double seconds)
{
    if (readDraft().failed()) return;
    const auto grid = draft.grid(*source);
    if (!grid.hasBar()) { status.setText("Choose beat one first.", juce::dontSendNotification); return; }
    const double factor = juce::ModifierKeys::getCurrentModifiersRealtime().isShiftDown() ? 10 : 1;
    phase.setText(juce::String(juce::jlimit(0.0, cursor.getMaximum(), grid.downbeatOrigin + seconds * factor), 4), false); changed();
}
void GridCorrectionEditor::timerCallback()
{
    const juce::ScopedLock guard(engine.lock);
    play.setButtonText(engine.gridAudition.active() ? "Stop source" : "Play source");
    if (engine.gridAudition.active()) setCursor(engine.gridAudition.position(), false);
}
bool GridCorrectionEditor::keyPressed(const juce::KeyPress& keyPress)
{
    if (keyPress == juce::KeyPress::spaceKey && !bpm.hasKeyboardFocus(true) && !phase.hasKeyboardFocus(true)) { togglePlay(); return true; }
    return false;
}
void GridCorrectionEditor::mouseDown(const juce::MouseEvent& event)
{
    if (waveform.contains(event.getPosition())) setCursor(viewStart + (event.x - waveform.getX()) * std::min(viewSpan, source->duration()) / waveform.getWidth());
}
void GridCorrectionEditor::paint(juce::Graphics& g)
{
    g.fillAll(colour::background); g.setColour(colour::panel); g.fillRect(waveform);
    if (waveform.isEmpty()) return;
    const double span = std::min(viewSpan, source->duration()); const float mid = waveform.getCentreY() + 8.0f;
    juce::Path body;
    for (int x = 0; x < waveform.getWidth(); ++x)
    {
        const double a = viewStart + span * x / waveform.getWidth(), b = viewStart + span * (x + 1) / waveform.getWidth();
        const auto range = source->waveformRange(a, b);
        const float height = juce::jlimit(0.0f, waveform.getHeight() * 0.40f, range.peak * waveform.getHeight() * 0.40f);
        body.startNewSubPath((float) (waveform.getX() + x), mid - height); body.lineTo((float) (waveform.getX() + x), mid + height);
    }
    g.setColour(colour::muted.withAlpha(0.65f)); g.strokePath(body, juce::PathStrokeType(1));
    const auto grid = draft.grid(*source);
    if (validDraft && grid.hasPulse())
    {
        const double pixels = waveform.getWidth() * 60 / grid.bpm / span;
        const double stride = pixels >= 6 ? 1 : 4 * std::ceil(6 / (pixels * 4));
        for (double beat = std::ceil(grid.beatAt(viewStart) / stride) * stride; grid.timeAtBeat(beat) <= viewStart + span; beat += stride)
        {
            const int x = waveform.getX() + juce::roundToInt((grid.timeAtBeat(beat) - viewStart) / span * waveform.getWidth());
            const bool bar = grid.hasBar() && std::abs(std::remainder(beat, 4)) < 0.1;
            g.setColour(bar ? colour::orange : colour::border); g.drawVerticalLine(x, (float) waveform.getY(), (float) waveform.getBottom());
            if (pixels > 25 && grid.hasBar()) { g.setColour(colour::text); g.drawText(juce::String((int) (std::fmod(std::fmod(beat, 4) + 4, 4)) + 1), x + 3, waveform.getY(), 22, 18, juce::Justification::left); }
        }
    }
    g.setColour(colour::orange); const int x = waveform.getX() + juce::roundToInt((cursor.getValue() - viewStart) / span * waveform.getWidth());
    g.drawVerticalLine(x, (float) waveform.getY(), (float) waveform.getBottom());
    g.setColour(colour::text); g.drawText(juce::String(viewStart, 3) + " s", waveform.withHeight(18).translated(4, waveform.getHeight() - 20), juce::Justification::left);
}
void GridCorrectionEditor::resized()
{
    auto r = getLocalBounds().reduced(14); heading.setBounds(r.removeFromTop(26)); r.removeFromTop(6);
    auto labels = r.removeFromTop(21); bpmLabel.setBounds(labels.removeFromLeft(210)); phaseLabel.setBounds(labels.removeFromLeft(385)); keyLabel.setBounds(labels);
    auto fields = r.removeFromTop(27); bpm.setBounds(fields.removeFromLeft(200)); fields.removeFromLeft(10); phase.setBounds(fields.removeFromLeft(375)); fields.removeFromLeft(10); key.setBounds(fields);
    r.removeFromTop(6); auto fine = r.removeFromTop(28);
    slower.setBounds(fine.removeFromLeft(105).reduced(2, 0)); faster.setBounds(fine.removeFromLeft(105).reduced(2, 0)); effectiveTempo.setBounds(fine);
    r.removeFromTop(4); auto actions = r.removeFromTop(28);
    for (auto* button : {&half, &twice, &tap, &resetTap, &beatOne, &earlier, &later, &automatic}) { button->setBounds(actions.removeFromLeft(105).reduced(2, 0)); }
    r.removeFromTop(10); waveform = r.removeFromTop(150); cursor.setBounds(r.removeFromTop(31));
    auto checks = r.removeFromTop(29); for (auto* button : {&beginning, &middle, &end, &quiet}) button->setBounds(checks.removeFromLeft(140).reduced(2, 0)); zoom.setBounds(checks);
    r.removeFromTop(7); auto monitor = r.removeFromTop(32); play.setBounds(monitor.removeFromLeft(125)); click.setBounds(monitor.removeFromLeft(125)); sourceAudio.setBounds(monitor.removeFromLeft(140)); level.setBounds(monitor);
    status.setBounds(r.removeFromTop(43)); auto footer = r.removeFromBottom(32); cancel.setBounds(footer.removeFromRight(110)); footer.removeFromRight(8); rebuild.setBounds(footer.removeFromRight(200)); footer.removeFromRight(8); save.setBounds(footer.removeFromRight(160));
}
GridCorrectionWindow::GridCorrectionWindow(GridCorrectionEditor* editor) : AlloyPopupWindow("Source beat grid / corrections", colour::background, true)
{
    setUsingNativeTitleBar(true); setContentOwned(editor, true); setResizable(false, false);
    centreWithSize(editor->getWidth(), editor->getHeight());
}
void GridCorrectionWindow::closeButtonPressed()
{
    if (auto* editor = dynamic_cast<GridCorrectionEditor*>(getContentComponent())) editor->closePreview();
    setVisible(false); exitModalState(0);
}
}
