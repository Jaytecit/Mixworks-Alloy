#include "Workstation.h"
#include "GridCorrectionUI.h"
#include "Beatmatch.h"
#include <limits>
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace mixworks
{
void runGridAlignmentTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    // Small authored PCM; no detector, hardware device, Workstation, production store or tempo worker.
    auto engine = std::make_unique<Engine>(report.getSiblingFile("alignment-midi.json"), false);
    auto media = std::make_shared<Media>(); media->name = "Manual alignment / authored 128 BPM";
    media->file = report.getSiblingFile("alignment-source.wav"); media->sampleRate = 1000;
    media->samples.setSize(1, 90000); media->samples.clear();
    for (int i = 0; i < media->frames(); ++i)
    {
        const double beat = (i / media->sampleRate - 8.0) * 128 / 60;
        const double phase = beat - std::floor(beat);
        media->samples.setSample(0, i, (float) (.65 * std::exp(-phase * 50) * std::sin(i * .35)));
    }
    { juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream = media->file.createOutputStream();
      auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(1000).withNumChannels(1).withBitsPerSample(24));
      check(writer && writer->writeFromAudioSampleBuffer(media->samples, 0, media->frames()), "write tiny owned alignment fixture"); }
    media->analysisSource = {audioDigest(media->file), media->file.getSize(), 1000, media->frames(), 1};
    media->analysis.bpm = 120; media->analysis.beatOrigin = .213; media->analysis.tempoConfidence = .9;
    media->analysis.key = 5; media->analysis.keyConfidence = .8; media->manualKey = 5;
    media->prepareWaveformBody();
    const auto digest = audioDigest(media->file);
    engine->library.push_back(media); Clip clip; clip.media = media; clip.start = 3; clip.length = 30; engine->project.clips = {clip};
    const auto document = juce::JSON::toString(projectToJson(engine->project), true, 17);
    int commits = 0; bool rebuilt = false;
    auto editor = std::make_unique<GridCorrectionEditor>(*engine, media, "", [&](const SourceCorrection& value, bool rebuild)
    {
        ++commits; rebuilt = rebuild;
        const auto result = engine->correctSource(media, value); if (result.failed()) return result;
        const auto error = writeAnalysisRecord(media->file, media->analysisRecord());
        return error.isEmpty() ? juce::Result::ok() : juce::Result::fail(error);
    });
    editor->stopTimer();
    check(editor->page==4 && editor->setPoint[0].isVisible() && !editor->setPoint[1].isEnabled()
        && !editor->setPoint[2].isEnabled() && !editor->bpm.isVisible() && !editor->tempoPage.isVisible()
        && !editor->save.isEnabled(), "A/B/C is the first screen; later steps and save wait for measured marks");
    check(editor->sourceAudio.getToggleState() && !editor->click.getToggleState(), "locating downbeats starts with source audio alone; grid click is an explicit check");
    {
        const auto captureSimple = [&](const juce::String& name)
        { auto out = report.getSiblingFile(name + ".png").createOutputStream(); juce::PNGImageFormat png;
          check(out && png.writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds()), *out), "capture simple fixed-marker workflow / " + name); };
        captureSimple("grid-simple-start");
        const auto initial = editor->gridEdit();
        const auto gesture = [&](juce::Point<float> point, juce::Point<float> origin, bool moved, bool fine = false)
        { return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), point,
            juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier | (fine ? juce::ModifierKeys::shiftModifier : 0)),
            1, 0, 0, 0, 0, editor.get(), editor.get(), juce::Time::getCurrentTime(), origin, juce::Time::getCurrentTime(), 1, moved); };
        for (double at : {0., 8., 89.999})
        { editor->setCursor(at, false); check(std::abs(editor->xAtTime(at) - editor->waveform.getX() - editor->waveform.getWidth() * .5) < .001,
            "selection stays beneath fixed centre marker, including source edges"); }
        editor->setCursor(8, false);
        engine->gridAudition.start(media, editor->draft.grid(*media), 8, true, false, -12);
        juce::AudioBuffer<float> audition(2, 100); engine->gridAudition.render(audition, 100, 1000);
        editor->setPoint[0].onClick();
        check(std::abs(editor->points[0] - 8.1) < 1.e-8 && !engine->gridAudition.active(), "Set captures the actual playing source clock and pauses audition");
        editor->undoGrid.onClick();
        editor->setCursor(8, false); editor->setPoint[0].onClick();
        check(editor->points[0] == 8 && editor->draft.downbeat == 8 && editor->setPoint[1].isEnabled() && !editor->setPoint[2].isEnabled(), "Set A pins a source downbeat and enables B only");
        editor->zoom.setSelectedId(6, juce::sendNotificationSync);
        const auto beforeNavigation = editor->gridEdit(); const auto history = editor->gridUndo.size();
        const auto from = editor->waveform.getCentre().toFloat(); const auto to = from.translated(40, 0);
        editor->mouseDown(gesture(from, from, false)); editor->mouseDrag(gesture(to, from, true)); editor->mouseUp(gesture(to, from, true));
        check(std::abs(editor->cursor.getValue() - 8 + 40 * .25 / editor->waveform.getWidth()) < 1.e-8
            && editor->gridEdit() == beforeNavigation && editor->gridUndo.size() == history,
            "simple waveform drag navigates under marker without moving saved marks, grid or history");
        const auto beforeFine = editor->cursor.getValue();
        editor->mouseDown(gesture(from, from, false)); editor->mouseDrag(gesture(to, from, true, true)); editor->mouseUp(gesture(to, from, true, true));
        check(std::abs(editor->cursor.getValue() - beforeFine + 4 * .25 / editor->waveform.getWidth()) < 1.e-8, "Shift navigation is one tenth speed");
        const auto beforeEscape = editor->cursor.getValue();
        editor->mouseDown(gesture(from, from, false)); editor->mouseDrag(gesture(to, from, true)); editor->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));
        check(editor->cursor.getValue() == beforeEscape && editor->gridEdit() == beforeNavigation, "Escape restores navigation without changing marks");
        editor->setCursor(38, false); editor->setPoint[1].onClick();
        check(editor->points[1] == 38 && editor->barsAB.getValue() == 15 && !editor->countsConfirmed.getToggleState(), "B suggests a bar count from prior tempo without claiming it is confirmed");
        editor->barsAB.setValue(16, juce::sendNotificationSync);
        check(editor->draft.bpm == 128 && editor->points[0] == 8 && editor->points[1] == 38, "correcting A/B bar count gives independent 128 BPM while retaining both marks");
        editor->setCursor(68, false); editor->setPoint[2].onClick();
        check(editor->barsBC.getValue() == 16 && editor->points[2] == 68 && !editor->save.isEnabled(), "C suggests later count; confirmation is required before saving");
        editor->countsConfirmed.setToggleState(true, juce::sendNotificationSync);
        check(editor->simpleAligned() && editor->save.isEnabled() && editor->simpleResult.getText().contains("0.0 ms"), "three authored downbeats agree on a constant 128 BPM grid");
        editor->setCursor(8, false); captureSimple("grid-simple-aligned");
        editor->zoom.setSelectedId(5, juce::sendNotificationSync); editor->setCursor(38, false); captureSimple("grid-simple-overview");
        editor->zoom.setSelectedId(6, juce::sendNotificationSync);
        editor->setCursor(68.12, false); editor->setPoint[2].onClick(); editor->countsConfirmed.setToggleState(true, juce::sendNotificationSync);
        check(!editor->save.isEnabled() && !editor->simpleAligned() && editor->draft.bpm == 128
            && editor->simpleResult.getText().contains("120.0 ms"), "inconsistent C exposes drift without bending the A/B grid");
        editor->save.onClick();
        check(commits == 0 && !editor->closed, "a mismatched simple measurement cannot commit even if its save handler is invoked");
        captureSimple("grid-simple-mismatch");
        editor->undoGrid.onClick(); check(!editor->countsConfirmed.getToggleState(), "Undo restores C confirmation independently");
        editor->undoGrid.onClick(); check(editor->points[2] == 68 && editor->simpleAligned(), "Undo restores earlier C, bar counts and confirmation as one mark edit");
        editor->barsBC.setValue(15, juce::sendNotificationSync);
        check(!editor->countsConfirmed.getToggleState() && !editor->save.isEnabled(), "bar recount invalidates confirmation");
        editor->undoGrid.onClick();
        editor->visitPoint[0].onClick(); check(editor->cursor.getValue() == 8, "Go to A returns its exact source position under the centre marker");
        const auto prior = editor->gridEdit(); editor->setCursor(7, false); editor->setPoint[1].onClick();
        check(editor->gridEdit() == prior, "out-of-order B preserves all existing marks and measurements");
        editor->setCursor(9, false); editor->setPoint[0].onClick();
        check(editor->points[1] < 0 && editor->points[2] < 0 && !editor->save.isEnabled(), "replacing A clears later marks and their confirmation");
        editor->undoGrid.onClick();
        editor->advancedPage.onClick(); const auto priorAdvanced = editor->gridEdit(); editor->later.onClick(); editor->simplePage.onClick();
        check(!editor->simpleAligned() && !editor->save.isEnabled(), "advanced grid changes cannot leave a stale successful A/B/C result");
        editor->undoGrid.onClick(); check(editor->gridEdit() == priorAdvanced && editor->simpleAligned(), "Undo restores the validated grid across simple and Advanced views");
        editor->precision.setSelectedId(2, juce::dontSendNotification); const auto beforeArrow = editor->cursor.getValue();
        editor->keyPressed(juce::KeyPress(juce::KeyPress::rightKey));
        check(std::abs(editor->cursor.getValue() - beforeArrow - .001) < 1.e-8 && editor->simpleAligned(),
            "simple arrow navigation stays at 1 ms even after Advanced selected 10 ms grid edits");
        editor->precision.setSelectedId(1, juce::dontSendNotification);
        editor->restoreGrid(initial); editor->gridUndo.clear(); editor->gridRedo.clear();
        check(commits == 0 && engine->project.clips[0].start == 3, "all simple mark edits and navigation leave source records and arrangement untouched");
    }
    editor->tempoPage.onClick();
    check(editor->page==0 && editor->bpm.isVisible() && editor->tap.isVisible() && !editor->phase.isVisible()
        && !editor->key.isVisible() && !editor->analysisDetails.isVisible(), "guided tempo view hides exact timing, keys and diagnostics");
    const auto openingDraft=editor->gridEdit();
    editor->beatPage.onClick();
    check(editor->beatOne.isVisible() && editor->barLater.isVisible() && !editor->bars.isVisible() && !editor->bpm.isVisible(), "beat-one page exposes alignment and bar accent only");
    editor->checkPage.onClick();
    check(editor->bars.isVisible() && editor->matchB.isVisible() && !editor->phase.isVisible() && !editor->beatOne.isVisible(), "drift page exposes counted later-beat measurement");
    check(editor->gridEdit()==openingDraft && editor->gridUndo.empty(), "changing guide pages does not edit musical facts or create Undo");
    check(!editor->spacing.isEnabled() && !editor->goB.isEnabled(), "spacing requires a chosen downbeat anchor");
    editor->beatPage.onClick();
    editor->setCursor(8); editor->beatOne.onClick();
    check(editor->draft.downbeat == 8 && editor->secondAnchor() == 40 && editor->bars.getValue() == 16,
          "A pins any known downbeat and B identifies bar17 sixteen bars later");
    editor->checkPage.onClick(); editor->setCursor(38); editor->matchB.onClick();
    check(editor->draft.bpm == 128 && editor->calculated,
          "independent oracle: 16 authored 4/4 bars in30 seconds calculate128 BPM");
    check(editor->draft.downbeat == 8 && std::abs(editor->draft.grid(*media).timeAtBeat(-4) - 6.125) < 1.e-8,
          "spacing keeps A fixed and extends bar phase backwards");
    editor->beatPage.onClick(); const auto aligned=editor->draft.grid(*media);
    editor->barLater.onClick();
    check(std::abs(editor->draft.downbeat-8-60.0/128)<1e-7 && editor->draft.bpm==128
        && std::abs(std::remainder(editor->draft.grid(*media).beatAt(30)-aligned.beatAt(30),1.0))<1e-7,
        "changing the bar accent preserves the pulse everywhere and leaves BPM unchanged");
    editor->undoGrid.onClick();
    check(editor->draft.downbeat==8, "bar accent adjustment is reversible as one grid edit");
    editor->advancedPage.onClick();
    check(editor->phase.isVisible() && editor->key.isVisible() && editor->analysisDetails.isVisible(), "Advanced retains the existing precise controls");
    editor->key.setSelectedId(15, juce::dontSendNotification); editor->changed();
    editor->bars.setValue(8, juce::sendNotificationSync);
    check(editor->draft.bpm == 64 && std::abs(editor->secondAnchor() - 38) < 1.e-8,
          "correcting B bar identity preserves its measured source position and recalculates BPM");
    editor->undoGrid.onClick();
    check(editor->draft.bpm == 128 && editor->bars.getValue() == 16 && editor->draft.key == 13,
          "Undo restores grid spacing and bar count without undoing a key correction");
    editor->redoGrid.onClick(); check(editor->draft.bpm == 64, "Redo restores calculated spacing");
    editor->undoGrid.onClick();
    const auto beforeBad = editor->gridEdit();
    for (double at : {8.0, 7.0, 8.001, 91.0, std::numeric_limits<double>::quiet_NaN()})
        check(!editor->alignSecond(at) && editor->gridEdit() == beforeBad, "invalid or out-of-range B retains the previous grid");
    editor->bars.setValue(1, juce::sendNotificationSync);
    check(editor->bars.getValue() == 16 && editor->gridEdit() == beforeBad, "invalid bar recount rolls back atomically");
    editor->precision.setSelectedId(2, juce::dontSendNotification); editor->later.onClick();
    check(std::abs(editor->draft.downbeat - 8.01) < 1.e-8 && editor->draft.bpm == 128,
          "visible10 ms precision shifts the whole grid without changing BPM");
    editor->earlier.onClick(); editor->precision.setSelectedId(1, juce::dontSendNotification);
    const auto atB = editor->secondAnchor(); editor->wider.onClick();
    check(std::abs(editor->secondAnchor() - atB - .001) < .00002 && editor->draft.downbeat == 8 && editor->draft.bpm < 128,
          "Wider moves B by1 ms while A stays pinned");
    editor->narrower.onClick(); check(std::abs(editor->draft.bpm - 128) < .0001, "Narrower reverses the precision spacing adjustment");
    editor->goB.onClick(); editor->zoom.setSelectedId(5, juce::sendNotificationSync);
    const auto event = [&](juce::Point<float> point, juce::Point<float> down, bool dragged, bool right = false, bool fine = false)
    {
        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), point,
            juce::ModifierKeys((right ? juce::ModifierKeys::rightButtonModifier : juce::ModifierKeys::leftButtonModifier)
                | (fine ? juce::ModifierKeys::shiftModifier : 0)),
            1, 0, 0, 0, 0, editor.get(), editor.get(), juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), 1, dragged);
    };
    const auto drag = [&](int handle, double delta)
    {
        const double at = handle == 1 ? editor->draft.downbeat : editor->secondAnchor();
        const auto from = editor->handleBounds(at, handle).getCentre();
        const auto to = from.translated((float) (delta * editor->waveform.getWidth() / std::min(editor->viewSpan, media->duration())), 0);
        editor->mouseDown(event(from, from, false)); editor->mouseDrag(event(to, from, true)); editor->mouseUp(event(to, from, true));
    };
    auto history = editor->gridUndo.size(); const double a = editor->draft.downbeat;
    drag(1, 1.5);
    check(std::abs(editor->draft.downbeat - a - 1.5) < 1.e-5 && std::abs(editor->draft.bpm - 128) < .0001,
          "native A label drag moves the grid without changing BPM");
    check(editor->gridUndo.size() == history + 1, "one completed drag creates one grid Undo entry");
    editor->undoGrid.onClick(); check(std::abs(editor->draft.downbeat - 8) < 1.e-5, "Undo restores an entire A drag");
    const double b = editor->secondAnchor(); history = editor->gridUndo.size();
    drag(2, 2);
    check(std::abs(editor->secondAnchor() - b - 2) < .00003 && std::abs(editor->draft.downbeat - 8) < 1.e-5
          && std::abs(editor->draft.bpm - 120) < .0001, "native B label drag compacts or stretches around pinned A and calculates BPM");
    check(editor->gridUndo.size() == history + 1, "B drag coalesces into one Undo entry");
    editor->undoGrid.onClick();
    auto from = editor->handleBounds(editor->secondAnchor(), 2).getCentre(), to = from.translated(20, 0);
    const auto beforeEscape = editor->gridEdit(); history = editor->gridUndo.size();
    editor->mouseDown(event(from, from, false)); editor->mouseDrag(event(to, from, true));
    check(editor->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)) && editor->gridEdit() == beforeEscape,
          "Escape cancels a live drag and restores its original draft");
    editor->mouseUp(event(to, from, true)); check(editor->gridUndo.size() == history, "cancelled drag does not create Undo history");
    editor->mouseDown(event(from, from, false, true)); editor->mouseDrag(event(to, from, true, true)); editor->mouseUp(event(to, from, true, true));
    check(editor->gridEdit() == beforeEscape, "right button cannot start a grid drag");
    editor->beatPage.onClick(); editor->zoom.setSelectedId(6, juce::sendNotificationSync);
    check(editor->viewSpan == .25, "250 ms zoom exposes sub-millisecond waveform alignment");
    auto waveformDrag = [&](float pixels, bool fine, bool cancel)
    {
        editor->setCursor(editor->draft.downbeat, false);
        const auto origin = juce::Point<float>(editor->xAtTime(editor->draft.downbeat), (float)editor->waveform.getY() + 100);
        const auto target = origin.translated(pixels, 0);
        editor->mouseDown(event(origin, origin, false, false, fine));
        editor->mouseDrag(event(target, origin, true, false, fine));
        if (cancel) editor->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));
        editor->mouseUp(event(target, origin, true, false, fine));
    };
    const double sampleAnchor = editor->draft.downbeat, programmeCursor = engine->position;
    const double secondsPerPixel = .25 / editor->waveform.getWidth(); history = editor->gridUndo.size();
    waveformDrag(20, false, false);
    check(std::abs(editor->draft.downbeat - sampleAnchor + 20 * secondsPerPixel) < 1.e-7 && editor->draft.bpm == 128,
        "dragging waveform right moves its audio beneath the fixed grid without changing BPM");
    check(std::abs(editor->xAtTime(editor->draft.downbeat) - .35 * editor->waveform.getWidth() - editor->waveform.getX()) < .001
        && editor->cursor.getValue() == sampleAnchor && engine->position == programmeCursor,
        "waveform drag keeps the grid stationary onscreen and never seeks source or programme audio");
    check(editor->gridUndo.size() == history + 1, "waveform alignment is one reversible grid edit");
    editor->undoGrid.onClick(); check(editor->draft.downbeat == sampleAnchor, "Undo restores waveform phase adjustment");
    waveformDrag(20, true, false);
    check(std::abs(editor->draft.downbeat - sampleAnchor + 2 * secondsPerPixel) < 1.e-7 && sampleAnchor - editor->draft.downbeat < .001,
        "Shift waveform dragging provides a sub-millisecond tenth-speed adjustment");
    { auto out = report.getSiblingFile("grid-waveform-fine-drag.png").createOutputStream(); juce::PNGImageFormat png;
      check(out && png.writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds()), *out), "capture fine waveform drag and 4/4 colour legend"); }
    editor->undoGrid.onClick();
    history = editor->gridUndo.size(); editor->setCursor(sampleAnchor, false); const auto priorView = editor->viewStart;
    waveformDrag(-20, true, true);
    check(editor->draft.downbeat == sampleAnchor && editor->viewStart == priorView && editor->gridUndo.size() == history,
        "Escape cancels waveform phase and viewport movement without adding Undo");
    editor->setCursor(sampleAnchor, false);
    const auto fineOrigin = juce::Point<float>(editor->xAtTime(sampleAnchor), (float)editor->waveform.getY() + 100);
    editor->mouseDown(event(fineOrigin, fineOrigin, false));
    editor->mouseDrag(event(fineOrigin.translated(10, 0), fineOrigin, true));
    editor->mouseDrag(event(fineOrigin.translated(20, 0), fineOrigin, true, false, true));
    editor->mouseUp(event(fineOrigin.translated(20, 0), fineOrigin, true, false, true));
    check(std::abs(editor->draft.downbeat - sampleAnchor + 11 * secondsPerPixel) < 1.e-7,
        "enabling Shift during a waveform drag changes sensitivity without a phase jump");
    editor->undoGrid.onClick();
    editor->setCursor(sampleAnchor, false);
    const auto bodyPoint = juce::Point<float>(editor->xAtTime(sampleAnchor + .02), (float)editor->waveform.getY() + 100);
    editor->mouseDown(event(bodyPoint, bodyPoint, false));
    check(editor->cursor.getValue() == sampleAnchor, "waveform press defers audition until click or drag is known");
    editor->mouseUp(event(bodyPoint, bodyPoint, false));
    check(std::abs(editor->cursor.getValue() - sampleAnchor - .02) < 1.e-7 && editor->draft.downbeat == sampleAnchor,
        "waveform click retains source audition positioning without editing the grid");
    const auto gridBeforeZoom = editor->gridEdit();
    editor->zoom.setSelectedId(7, juce::sendNotificationSync); check(editor->viewSpan == .05, "50 ms zoom is available");
    editor->zoom.setSelectedId(8, juce::sendNotificationSync); check(editor->viewSpan == .01 && editor->gridEdit() == gridBeforeZoom, "10 ms zoom preserves musical timing and draft history");
    editor->advancedPage.onClick(); editor->zoom.setSelectedId(5, juce::sendNotificationSync);
    {
        auto detected = media->catalogueCopy(); detected->manualBpm = 0; detected->manualBeatOrigin = -1;
        detected->analysis.bpm = 128; detected->analysis.beatOrigin = detected->analysis.downbeatOrigin = 8;
        auto automaticEditor = std::make_unique<GridCorrectionEditor>(*engine, detected, "", [](const auto&, bool) { return juce::Result::ok(); });
        automaticEditor->stopTimer(); automaticEditor->beatPage.onClick(); automaticEditor->zoom.setSelectedId(6, juce::sendNotificationSync); automaticEditor->setCursor(8, false);
        const auto origin = juce::Point<float>(automaticEditor->xAtTime(8), (float)automaticEditor->waveform.getY() + 100);
        auto autoEvent = [&](juce::Point<float> point, bool moved)
        {
            return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), point,
                juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::shiftModifier),
                1, 0, 0, 0, 0, automaticEditor.get(), automaticEditor.get(), juce::Time::getCurrentTime(), origin, juce::Time::getCurrentTime(), 1, moved);
        };
        automaticEditor->mouseDown(autoEvent(origin, false)); automaticEditor->mouseDrag(autoEvent(origin.translated(20, 0), true)); automaticEditor->mouseUp(autoEvent(origin.translated(20, 0), true));
        check(std::abs(automaticEditor->draft.downbeat - 8 + 2 * secondsPerPixel) < 1.e-7 && automaticEditor->draft.bpm == 0
            && automaticEditor->draft.grid(*detected).bpm == 128, "fine waveform drag works directly from detected beat one without overriding detected BPM");
        automaticEditor->undoGrid.onClick();
        check(automaticEditor->draft.downbeat == -1 && automaticEditor->phase.getText().isEmpty(), "Undo restores a detected grid without leaving a manual correction");
    }
    editor->spacing.setToggleState(true, juce::dontSendNotification);
    const auto ruler = juce::Point<float>(editor->xAtTime(23), (float) editor->waveform.getY() + 54);
    editor->mouseDown(event(ruler, ruler, false)); editor->mouseUp(event(ruler, ruler, false));
    check(editor->bars.getValue() == 8 && !editor->calculated, "spacing ruler selects a different later bar as B without changing BPM");
    editor->undoGrid.onClick();
    editor->moveGrid.setToggleState(true, juce::dontSendNotification); editor->keyPressed(juce::KeyPress(juce::KeyPress::rightKey));
    check(std::abs(editor->draft.downbeat - 8.001) < 1.e-5, "keyboard precision moves the grid in Move grid mode");
    editor->undoGrid.onClick(); editor->spacing.setToggleState(true, juce::dontSendNotification);
    editor->keyPressed(juce::KeyPress(juce::KeyPress::rightKey));
    check(editor->draft.downbeat == 8 && editor->draft.bpm < 128, "keyboard precision spaces B around pinned A in Adjust spacing mode");
    editor->undoGrid.onClick();
    editor->revertGrid.onClick();
    check(editor->draft.bpm == 0 && editor->draft.downbeat == -1 && editor->draft.key == 13,
          "Revert grid restores opening timing while preserving the key draft");
    editor->undoGrid.onClick(); check(editor->draft.bpm == 128 && editor->draft.downbeat == 8, "Revert grid itself is undoable");
    editor->automatic.onClick(); check(editor->draft.key == 13, "Detected grid never clears the key correction"); editor->undoGrid.onClick();
    editor->phase.setText(juce::String(editor->cursor.getMaximum(), 8), false); editor->changed(); editor->later.onClick();
    check(editor->validDraft && editor->draft.downbeat < media->duration(), "precision at source end cannot round the anchor outside audio");
    editor->undoGrid.onClick();
    editor->bpm.setText("120..2", false); editor->changed();
    check(!editor->save.isEnabled() && !editor->spacing.isEnabled() && !editor->wider.isEnabled(), "malformed input disables saving and alignment gestures");
    editor->undoGrid.onClick(); editor->alignSecond(38);
    check(commits == 0 && media->manualBpm == 0 && media->manualBeatOrigin == -1 && media->manualKey == 5
          && document == juce::JSON::toString(projectToJson(engine->project), true, 17),
          "all grid drafts leave source corrections and the existing timeline untouched");
    editor->goB.onClick(); editor->zoom.setSelectedId(4, juce::sendNotificationSync);
    const auto capture = [&](const juce::String& name)
    {
        auto stream = report.getSiblingFile(name + ".png").createOutputStream(); juce::PNGImageFormat png;
        check(stream && png.writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds()), *stream), "capture actual alignment editor / " + name);
    };
    capture("grid-alignment-B"); editor->zoom.setSelectedId(5, juce::sendNotificationSync); capture("grid-alignment-overview");
    for (int page=0;page<3;++page)
    {
        editor->selectPage(page); capture("grid-guide-"+juce::String(page+1));
        check(editor->waveform.getHeight()>=250 && editor->getLocalBounds().contains(editor->save.getBounds()), "guided page keeps waveform and save visible without scrolling");
    }
    editor->simplePage.onClick();
    editor->setCursor(8, false); editor->setPoint[0].onClick();
    editor->setCursor(38, false); editor->setPoint[1].onClick(); editor->barsAB.setValue(16, juce::sendNotificationSync);
    editor->setCursor(68, false); editor->setPoint[2].onClick(); editor->barsBC.setValue(16, juce::sendNotificationSync);
    editor->countsConfirmed.setToggleState(true, juce::sendNotificationSync);
    check(editor->simpleAligned() && editor->save.isEnabled(), "final source save uses the completed simple A/B/C workflow");
    const auto finishModal=[&](int answer)
    {
        auto* modal=juce::Component::getCurrentlyModalComponent(); check(modal!=nullptr,"rebuild consequence confirmation is visible");
        if (modal) modal->exitModalState(answer);
        for (int i=0;i<10;++i)
        {
           #if JUCE_WINDOWS
            MSG message; while (PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
           #endif
            juce::Thread::sleep(2);
        }
    };
    editor->rebuild.onClick(); finishModal(0);
    check(commits==0 && !editor->closed, "cancelling rebuild confirmation keeps draft and mix unchanged");
    editor->save.onClick();
    const auto saved = readAnalysisRecord(media->file, media->analysisSource);
    check(commits == 1 && !rebuilt && saved.record && saved.record->manualBpm == 128 && saved.record->manualBeatOrigin == 8 && saved.record->manualKey == 13,
          "Save source corrections persists calculated BPM and anchor through the existing source-record format");
    check(engine->project.clips[0].start == 3 && engine->project.clips[0].length == 30, "save source only preserves timeline placement and duration");
    editor.reset();
    auto reopened = std::make_unique<GridCorrectionEditor>(*engine, media, "", [](const SourceCorrection&, bool) { return juce::Result::fail("owned failure"); });
    check(reopened->draft.bpm == 128 && reopened->draft.downbeat == 8 && reopened->draft.key == 13, "reopened editor starts from saved timing and key");
    reopened->advancedPage.onClick(); reopened->later.onClick(); reopened->save.onClick();
    check(!reopened->closed && reopened->draft.downbeat > 8 && media->manualBeatOrigin == 8, "failed save retains the unsaved alignment draft");
    reopened->cancel.onClick(); check(reopened->closed && media->manualBeatOrigin == 8, "Cancel discards alignment without modifying saved source facts");
    auto undetected = media->catalogueCopy(); undetected->analysis.bpm = 0; undetected->manualBpm = 0; undetected->manualBeatOrigin = -1;
    auto manual = std::make_unique<GridCorrectionEditor>(*engine, undetected, "", [](const SourceCorrection&, bool) { return juce::Result::ok(); });
    manual->setCursor(8); manual->beatOne.onClick();
    check(manual->bars.isEnabled() && !manual->spacing.isEnabled(), "missing detected BPM still allows a manual bar count before B is known");
    manual->bars.setValue(8, juce::sendNotificationSync); manual->setCursor(23); manual->matchB.onClick();
    check(manual->draft.bpm == 128 && manual->draft.downbeat == 8 && manual->spacing.isEnabled(),
          "two exact manual downbeats calculate BPM without any detected tempo");
    manual->bpm.setText({}, false); manual->phase.setText({}, false); manual->changed();
    check(!manual->draft.grid(*undetected).hasPulse(), "simple manual measurement starts with no detected or supplied tempo");
    manual->setCursor(8, false); manual->setPoint[0].onClick(); manual->setCursor(23, false); manual->setPoint[1].onClick();
    manual->barsAB.setValue(8, juce::sendNotificationSync); manual->setCursor(38, false); manual->setPoint[2].onClick();
    manual->countsConfirmed.setToggleState(true, juce::sendNotificationSync);
    check(manual->simpleAligned() && manual->draft.bpm == 128, "simple three-point correction also works without a detected tempo");
    int rebuildRequests=0;
    auto confirm=std::make_unique<GridCorrectionEditor>(*engine,media,"",[&](const SourceCorrection&,bool rebuild)
    { if (rebuild) ++rebuildRequests; return juce::Result::ok(); });
    confirm->advancedPage.onClick();
    confirm->rebuild.onClick(); finishModal(1);
    check(rebuildRequests==1 && confirm->closed, "explicit rebuild confirmation commits exactly once");
    check(audioDigest(media->file) == digest, "dragging, precision, saving and Cancel leave original audio bytes unchanged");
}
void runGridCorrectionTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    runGridAlignmentTests(check, report);
    const auto hasItem = [](const juce::PopupMenu& menu, int id)
    {
        for (juce::PopupMenu::MenuItemIterator it(menu, true); it.next();)
            if (it.getItem().itemID == id) return true;
        return false;
    };
    SourceCorrection parsed {123, 0.25, 2};
    for (const auto& text : {"120..2", "nan", "inf", "120 bpm", "0", "401"})
        check(SourceCorrection::parse(text, "0.2", 0, 12, parsed).failed() && parsed.bpm == 123,
              juce::String("invalid correction is rejected atomically: ") + text);
    check(SourceCorrection::parse("120.1234", "12", 0, 12, parsed).failed(), "beat-one correction excludes the source's end boundary");
    check(SourceCorrection::parse("", "", -1, 12, parsed).wasOk() && parsed.bpm == 0 && parsed.downbeat == -1,
          "blank fields explicitly restore automatic facts without inventing a bar");
    TapTempo taps;
    check(!taps.tap(1) && !taps.tap(1.5) && taps.tap(2) && std::abs(*taps.tap(2.5) - 120) < 1.0e-8,
          "steady quarter-note taps estimate tempo independently of bar phase");
    const auto count = taps.count(); taps.tap(2.55);
    check(taps.count() == count && !taps.tap(10) && taps.count() == 1, "tap bounce is ignored and a long pause starts a fresh estimate");
    SourceBeatGrid unknown {120, 0.213, -1}, known {120, 0.213, 0.213};
    check(std::abs(known.timeAtBeat(9) - 4.713) < 1.0e-9 && std::abs(known.beatAt(4.713) - 9) < 1.0e-9,
          "source drawing and click use the same reversible beat clock");
    check(std::abs(unknown.clickAt(0.21325) - unknown.clickAt(0.71325)) < 1.0e-6
          && std::abs(known.clickAt(0.21325)) > std::abs(known.clickAt(0.71325)),
          "unknown bars have equal clicks while known beat one is accented");
    SourceBeatGrid malformed {120, std::numeric_limits<double>::quiet_NaN(), -1};
    check(malformed.clickAt(1) == 0 && unknown.clickAt(std::numeric_limits<double>::infinity()) == 0,
          "malformed/nonfinite grid clocks cannot emit nonfinite audio");

    auto media = std::make_shared<Media>(); media->name = "Source grid fixture"; media->sampleRate = 44100;
    media->file = report.getSiblingFile("grid-source.wav"); media->samples.setSize(1, 44100 * 12);
    for (int i = 0; i < media->frames(); ++i)
        media->samples.setSample(0, i, (float) (0.3 * std::sin(juce::MathConstants<double>::twoPi * 440 * i / media->sampleRate)));
    { juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream = media->file.createOutputStream();
      auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(44100).withNumChannels(1).withBitsPerSample(24));
      check(writer && writer->writeFromAudioSampleBuffer(media->samples, 0, media->frames()), "write owned source-grid audio fixture"); }
    media->analysisSource = {audioDigest(media->file), media->file.getSize(), 44100, media->frames(), 1};
    media->analysis.bpm = 120; media->analysis.beatOrigin = 0.213; media->analysis.tempoConfidence = 0.9;
    media->analysis.key = 0; media->analysis.keyConfidence = 0.8; media->analysis.examinedSeconds = 12;
    media->peaks.assign(512, 0.3f); media->prepareWaveformBody();
    const auto sourceDigest = audioDigest(media->file);
    GridAudition audition; juce::AudioBuffer<float> audio(2, 2048), whole(2, 2048), partial(2, 512);
    check(audition.start(media, known, 0.1, true, false, -6).wasOk(), "source audition accepts decoded source audio");
    audition.render(audio, 2048, 48000);
    check(std::abs(audio.getSample(0, 0) - media->samples.getSample(0, 4410) * juce::Decibels::decibelsToGain(-6.0f)) < 1.0e-6
          && audio.getSample(0, 123) == audio.getSample(1, 123) && std::abs(audition.position() - (0.1 + 2048.0 / 48000)) < 1.0e-8,
          "monitor audition resamples the raw source in source seconds and maps mono to stereo");
    audition.start(media, known, 0.2, false, true, -12); audition.render(whole, 2048, 44100);
    audition.start(media, known, 0.2, false, true, -12);
    double difference = 0;
    for (int block = 0; block < 4; ++block)
    {
        audition.render(partial, 512, 44100);
        for (int i = 0; i < 512; ++i) difference = std::max(difference, (double) std::abs(partial.getSample(0, i) - whole.getSample(0, block * 512 + i)));
    }
    check(difference == 0 && whole.getMagnitude(0, 2048) > 0.01, "audible clicks are invariant across callback block boundaries");
    audition.start(media, known, media->duration() - 0.001, true, true, -12); audition.render(audio, 2048, 44100);
    check(!audition.active() && audio.getSample(0, 2047) == 0, "source end stops audition with a silent remainder");

    Workstation ui(false); ui.setSize(1250, 800); ui.engine.library.push_back(media); ui.selectedLibraryId = media->id;
    Clip clip; clip.media = media; clip.length = media->duration(); clip.beatmatched = true;
    Project input; input.clips = {clip}; auto prepared = prepareTempoMap(input, false, 0);
    check(prepared.error.isEmpty(), "prepare captured arrangement before source correction: " + prepared.error);
    if (prepared.error.isNotEmpty()) return;
    ui.engine.project = prepared.project; ui.refresh();
    check(hasItem(ui.getMenuForIndex(3, {}), 3011), "Beatmatch menu exposes half-beat snapping");
    const auto zoomDocument = juce::JSON::toString(projectToJson(ui.engine.project),true,17);
    const auto zoomHistory = ui.undoHistory.size(); const auto zoomDirty = ui.dirty;
    const auto zoomSelection = ui.selectedClip; const auto zoomPosition = ui.engine.position.load();
    ui.setFollowPlayhead(false);
    ui.timeline.pixelsPerSecond = 800; ui.timeline.updateSize(); ui.viewport.setViewPosition(1600,0);
    const double anchor = ui.viewport.getViewPositionX()+ui.viewport.getViewWidth()*.5;
    const double anchorTime = anchor/800, screen = anchor-ui.viewport.getViewPositionX();
    ui.timeline.zoomAt(2,anchor);
    check(ui.timeline.pixelsPerSecond == 1600 && std::abs(anchorTime*1600-ui.viewport.getViewPositionX()-screen) <= .51,
        "timeline doubles to1600 while retaining the time beneath the zoom anchor");
    ui.timeline.zoomAt(1.e6,ui.viewport.getViewPositionX()+screen);
    check(ui.timeline.pixelsPerSecond == 1600,"repeated zoom saturates safely at the doubled limit");
    check(ui.timelineOverview.keyPressed(juce::KeyPress('-',juce::ModifierKeys(),'-')) && ui.timeline.pixelsPerSecond == 800
        && ui.timelineOverview.keyPressed(juce::KeyPress('+',juce::ModifierKeys(),'+')) && ui.timeline.pixelsPerSecond == 1600,
        "overview keyboard zoom reaches the new limit and zooms back out normally");
    ui.menuItemSelected(24,4); ui.menuItemSelected(23,4); ui.menuItemSelected(23,4);
    check(ui.timeline.pixelsPerSecond == 1600,"View menu zoom shares the new saturation limit");
    const auto mouse = juce::Desktop::getInstance().getMainMouseSource();
    auto zoomEvent = [&](juce::Point<float> at,juce::Point<float> down,bool drag)
    { return juce::MouseEvent(mouse,at,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,&ui.timelineOverview,&ui.timelineOverview,juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),1,drag); };
    for (int edge : {-1,1})
    {
        ui.timeline.pixelsPerSecond = 800; ui.timeline.updateSize(); ui.viewport.setViewPosition(4000,0);
        const double begin = ui.viewport.getViewPositionX()/800., end = begin+ui.viewport.getViewWidth()/800.;
        const auto box = ui.timelineOverview.previewBounds();
        const juce::Point<float> down{edge < 0 ? box.getX() : box.getRight(),box.getCentreY()};
        const auto to = down.translated(edge < 0 ? 10000.f : -10000.f,0);
        ui.timelineOverview.mouseDown(zoomEvent(down,down,false)); ui.timelineOverview.mouseDrag(zoomEvent(to,down,true)); ui.timelineOverview.mouseUp(zoomEvent(to,down,true));
        const double after = ui.viewport.getViewPositionX()/ui.timeline.pixelsPerSecond;
        check(std::abs(ui.timeline.pixelsPerSecond-1600) < 1.e-8
            && std::abs(edge < 0 ? after+ui.viewport.getViewWidth()/ui.timeline.pixelsPerSecond-end : after-begin) <= 1./1600,
            "actual overview edge reaches1600 with its opposite time anchored / " + juce::String(edge));
    }
    const auto box = ui.timelineOverview.previewBounds(); const auto down = box.getCentre(), to = down.translated(15,0);
    const int panStart = ui.viewport.getViewPositionX();
    ui.timelineOverview.mouseDown(zoomEvent(down,down,false)); ui.timelineOverview.mouseDrag(zoomEvent(to,down,true)); ui.timelineOverview.mouseUp(zoomEvent(to,down,true));
    check(std::abs(ui.timeline.pixelsPerSecond-1600) < 1.e-8 && ui.viewport.getViewPositionX() != panStart,
        "narrow overview body still pans at maximum zoom");
    ui.timeline.zoomAt(1600/ui.timeline.pixelsPerSecond,0);
    const auto zoomWorkspace = report.getSiblingFile("maximum-zoom.alloyworkspace");
    check(writeNamedWorkspace(zoomWorkspace,ui.captureNamedWorkspace("Maximum zoom")).wasOk(),"save a named workspace at1600");
    NamedWorkspace loadedZoom;
    check(readNamedWorkspace(zoomWorkspace,loadedZoom).wasOk() && loadedZoom.zoom == 1600,"named workspace reads the doubled view bound");
    ui.timeline.zoomAt(.5,0);
    check(ui.restoreNamedWorkspace(zoomWorkspace).wasOk() && ui.timeline.pixelsPerSecond == 1600,"restoring a named workspace reinstates1600 through the actual UI owner");
    auto invalidZoom = juce::JSON::parse(zoomWorkspace.loadFileAsString()); invalidZoom.getDynamicObject()->setProperty("zoom",1600.1);
    const auto invalidWorkspace = report.getSiblingFile("invalid-zoom.alloyworkspace"); invalidWorkspace.replaceWithText(juce::JSON::toString(invalidZoom));
    check(readNamedWorkspace(invalidWorkspace,loadedZoom).failed() && loadedZoom.zoom == 1600,"out-of-bound workspace zoom rejects without replacing the usable view");
    check(juce::JSON::toString(projectToJson(ui.engine.project),true,17) == zoomDocument && ui.undoHistory.size() == zoomHistory && ui.dirty == zoomDirty
        && ui.selectedClip == zoomSelection && ui.engine.position == zoomPosition,"zoom/menu/overview/layout workflows preserve musical data, Undo, dirty state, selection and transport");
    ui.timeline.zoomAt(12/ui.timeline.pixelsPerSecond,0); ui.viewport.setViewPosition(0,0);
    // Snap follows integrated beats, including tempo ramps and signed audio slips.
    for (int division : {1, 2, 4})
    {
        ui.menuItemSelected(3010 + (division == 1 ? 0 : division == 2 ? 1 : 2), 3);
        check(ui.engine.project.snapDivision == division, "Beatmatch selector updates the shared snap subdivision");
        for (bool mapped : {false, true})
        {
            auto p = ui.engine.project; p.snapDivision = division; p.followTrackTempo = mapped;
            p.tempo.points = {{0, 120, true}, {5, 150, false}};
            const auto clock = mapped ? p.tempo : TempoMap {p.bpm, {}};
            const double value = clock.timeAtBeat(3.14);
            const double expected = std::round(3.14 * division) / division;
            check(std::abs(clock.beatsAt(p.snappedTime(value, true)) - expected) < 1e-8 && p.snappedTime(value, false) == value,
                  "snap uses selected subdivisions on constant and ramped grids; bypass remains exact");
            check(std::abs(clock.snapWithPreroll(-0.14, true, p.snapStep()) - std::round(-0.28 * division) / division * 0.5) < 1e-8,
                  "audio-only slip snapping uses the same subdivision before mix zero");
            const auto placed = p.snappedClipStart(p.clips[0], value, true);
            const auto& c = p.clips[0];
            const double phase = c.sourceElapsed(c.firstBeatTime()) * c.tempoSourceBpm / 60;
            const double beat = mapped ? clock.beatsAt(placed) + phase : clock.beatsAt(placed + c.firstBeatTime());
            check(std::abs(beat * division - std::round(beat * division)) < 1e-7, "clip pulse alignment honours the selected snap subdivision");
        }
        const auto saved = parseProject(juce::JSON::toString(projectToJson(ui.engine.project, ui.engine.library)));
        check(saved.error.isEmpty() && saved.project.snapDivision == division, "saved mix retains the snap subdivision");
    }
    ui.undo(false); check(ui.engine.project.snapDivision == 2, "snap subdivision is undoable");
    ui.undo(true); check(ui.engine.project.snapDivision == 4, "snap subdivision supports redo");
    auto legacy = projectToJson(ui.engine.project, ui.engine.library);
    legacy.getDynamicObject()->setProperty("version", 27); legacy.getDynamicObject()->removeProperty("snapDivision");
    const auto oldSession = parseProject(juce::JSON::toString(legacy));
    check(oldSession.error.isEmpty() && oldSession.project.snapDivision == 1, "older sessions default to whole-beat snapping");
    for (const auto& value : {juce::var(3), juce::var(0), juce::var(2.0), juce::var("4"), juce::var(true)})
    {
        auto invalid = projectToJson(ui.engine.project, ui.engine.library); invalid.getDynamicObject()->setProperty("snapDivision", value);
        check(parseProject(juce::JSON::toString(invalid)).error.isNotEmpty(), "invalid snap subdivisions reject the session");
    }
    ui.engine.project.snapDivision = 1;
    check(hasItem(ui.libraryTable.beatmatchMenu(0), 25) && hasItem(ui.playlistTable.beatmatchMenu(0), 25)
          && ui.libraryTable.beatmatchMenu(-1).getNumItems() == 0, "library and playlist row menus expose Beatmatch only for a valid clicked track");
    const auto rightClick = [](juce::Component& component)
    {
        const auto point = juce::Point<float> {40, 12};
        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), point,
            juce::ModifierKeys(juce::ModifierKeys::rightButtonModifier), 1, 0, 0, 0, 0,
            &component, &component, juce::Time::getCurrentTime(), point, juce::Time::getCurrentTime(), 1, false);
    };
    ui.libraryTable.cellClicked(0, 1, rightClick(ui.libraryTable.table));
    check(ui.correctionFromLibrary && ui.selectedLibraryId == media->id && juce::PopupMenu::dismissAllActiveMenus(),
          "native Library RMB handler opens the Beatmatch popup for the clicked source");
    ui.playlistTable.cellClicked(0, 1, rightClick(ui.playlistTable.table));
    check(!ui.correctionFromLibrary && ui.selectedClip == clip.id && juce::PopupMenu::dismissAllActiveMenus(),
          "native Playlist RMB handler opens the Beatmatch popup for the clicked clip");
    auto before = ui.engine.project.clips[0]; const auto tempo = ui.engine.project.tempo.points;
    audio.setSize(2, 512); juce::MidiBuffer midi; ui.engine.render(audio, 512, 0.3, midi); whole.makeCopyOf(audio);
    check(ui.engine.correctSource(media, {130.5, 0.213, 3}).wasOk(), "production engine accepts a validated source correction");
    const auto& after = ui.engine.project.clips[0];
    ui.engine.render(audio, 512, 0.3, midi); difference = 0;
    for (int i = 0; i < 512; ++i) difference = std::max(difference, (double) std::abs(audio.getSample(0, i) - whole.getSample(0, i)));
    check(after.start == before.start && after.offset == before.offset && after.length == before.length && after.prepared == before.prepared
          && after.tempoMap == before.tempoMap && after.tempoSourceBpm == before.tempoSourceBpm && ui.engine.project.tempo.points == tempo && difference == 0,
          "changing source facts preserves placement, captured clock, prepared audio, tempo map and actual PCM");
    auto catalogue = media->catalogueCopy(); ui.engine.library = {catalogue};
    check(ui.engine.correctSource(catalogue, {128, 0.125, 5}).wasOk() && media->manualBpm == 128 && media->manualBeatOrigin == 0.125,
          "one source identity owns corrections across catalogue and hydrated arrangement objects");
    auto foreign = media->catalogueCopy(); foreign->analysisSource.sha256 = juce::String::repeatedString("a", 64);
    check(ui.engine.correctSource(foreign, {140, 0.5, 1}).failed() && media->manualBpm == 128,
          "a different source identity cannot correct an existing arrangement by filename");
    ui.engine.library = {media}; ui.engine.correctSource(media, {0, -1, -1});
    ui.editAnalysis(media->id);
    auto* editor = dynamic_cast<GridCorrectionEditor*>(ui.gridWindow->getContentComponent());
    check(editor && ui.gridWindow->isVisible(), "BPM / Key opens the native source-grid correction window");
    if (!editor) return;
    editor->advancedPage.onClick();
    bool fineTempo = false, rebuildAction = false;
    for (auto* child : editor->getChildren())
    { fineTempo |= child->getName() == "Increase source tempo by 0.01 BPM"; rebuildAction |= child->getName() == "Save corrections and rebuild timeline"; }
    check(fineTempo && rebuildAction, "source editor exposes fine tempo and explicit save-and-rebuild actions");
    editor->faster.onClick();
    check(std::abs(editor->draft.bpm - 120.01) < 1e-8 && media->manualBpm == 0, "fine tempo starts from detected BPM and remains a draft");
    editor->slower.onClick();
    check(std::abs(editor->draft.bpm - 120) < 1e-8, "fine tempo decrement returns exactly to the source tempo");
    editor->automatic.onClick();
    editor->middle.onClick(); editor->beatOne.onClick(); editor->bpm.setText("120..2", false); editor->changed();
    check(!editor->save.isEnabled() && media->manualBeatOrigin == -1, "invalid draft disables Apply and leaves source facts unchanged");
    editor->bpm.setText("120", false); editor->changed(); editor->half.onClick();
    check(editor->draft.bpm == 60 && editor->draft.downbeat == 6, "half-tempo and Beat 1 here operate on the source cursor draft");
    editor->automatic.onClick(); editor->play.onClick();
    const double position = ui.engine.position; float left[256] {}, right[256] {}; float* outputs[] {left, right};
    ui.engine.audioDeviceIOCallbackWithContext(nullptr, 0, outputs, 2, 256, {});
    check(ui.engine.gridAudition.active() && !ui.engine.playing && ui.engine.position == position
          && juce::FloatVectorOperations::findMinAndMax(left, 256).getLength() > 0,
          "real audio callback auditions the source while leaving the mix clock paused");
    ui.engine.project.masterDb = -3;
    const auto exported = report.getSiblingFile("no-preview-in-export.wav");
    check(ui.engine.exportWav(exported).isEmpty(), "offline render remains available with monitoring state separate from the project");
    auto stream = report.getSiblingFile("source-grid-editor.png").createOutputStream(); juce::PNGImageFormat png;
    check(stream && png.writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds()), *stream), "capture native source-grid editor for visual verification");
    editor->cancel.onClick();
    check(!ui.engine.gridAudition.active() && !ui.gridWindow->isVisible() && media->manualBeatOrigin == -1 && media->manualBpm == 0,
          "Cancel closes monitoring and discards corrections without changing source facts");
    ui.editAnalysis(media->id); editor = dynamic_cast<GridCorrectionEditor*>(ui.gridWindow->getContentComponent());
    editor->advancedPage.onClick();
    editor->bpm.setText("130.5", false); editor->phase.setText("0.213", false); editor->key.setSelectedId(5, juce::dontSendNotification);
    editor->changed(); editor->save.onClick(); ui.workers.removeAllJobs(false, 10000);
    auto record = readAnalysisRecord(media->file, media->analysisSource);
    check(!ui.busy && ui.dirty && record.record && record.record->manualBpm == 130.5 && record.record->manualBeatOrigin == 0.213 && record.record->manualKey == 3,
          "native Apply persists source corrections once without preparing or rearranging the mix");
    // A failed durable write must leave the editor open and the source unchanged.
    const auto recordFile = analysisRecordFile(media->file), recordBackup = report.getSiblingFile("correction-before-write-failure.json");
    const bool isolated = recordFile.isAChildOf(report.getParentDirectory());
    check(isolated, "failed-save probe is confined to the owned native-test analysis root");
    if (!isolated) return;
    check(recordFile.moveFileTo(recordBackup) && recordFile.createDirectory().wasOk(), "temporarily block this fixture's analysis-record path");
    ui.editAnalysis(media->id); editor = dynamic_cast<GridCorrectionEditor*>(ui.gridWindow->getContentComponent());
    editor->advancedPage.onClick();
    editor->bpm.setText("140", false); editor->changed(); editor->save.onClick(); ui.workers.removeAllJobs(false, 10000);
    check(ui.gridWindow->isVisible() && media->manualBpm == 130.5 && media->manualBeatOrigin == 0.213,
          "failed source-record write keeps the draft open and rolls back the unsaved correction");
    editor->cancel.onClick();
    check(recordFile.deleteFile() && recordBackup.moveFileTo(recordFile), "restore the owned fixture's saved analysis after failed-write probe");
    ui.engine.correctSource(media, {130.5, 0.213, 3});
    const auto session = report.getSiblingFile("corrected.mixworks");
    check(ui.engine.saveProject(session).isEmpty() && ui.engine.restoreProject(session.loadFileAsString()).isEmpty(), "corrected source and captured arrangement save/reopen through production APIs");
    check(ui.engine.project.clips[0].media->manualBpm == 130.5 && ui.engine.project.clips[0].tempoSourceBpm == before.tempoSourceBpm
          && ui.engine.project.clips[0].start == before.start && ui.engine.project.clips[0].length == before.length,
          "reopening retains the correction separately from the original arranged source clock");
    ui.editAnalysis(media->id); editor = dynamic_cast<GridCorrectionEditor*>(ui.gridWindow->getContentComponent());
    editor->advancedPage.onClick();
    check(editor && editor->draft.bpm == 130.5 && editor->draft.key == 3, "reopened source editor shows saved BPM and key");
    editor->bpm.setText("129.99", false); editor->phase.setText("0.125", false); editor->changed();
    const double preApplyClock = ui.engine.project.clips[0].tempoSourceBpm;
    check(editor->rebuild.getButtonText() == "Save + rebuild mix..." && editor->rebuild.isEnabled(),
          "review explicitly names the save and whole-mix rebuild action");
    editor->rebuild.onClick();
    auto* rebuildConfirmation=juce::Component::getCurrentlyModalComponent();
    check(rebuildConfirmation!=nullptr,"whole-mix rebuild requires consequence confirmation");
    if (rebuildConfirmation) rebuildConfirmation->exitModalState(1);
    for (int i=0;i<10;++i)
    {
       #if JUCE_WINDOWS
        MSG message; while (PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
       #endif
        juce::Thread::sleep(2);
    }
    const double deadline = juce::Time::getMillisecondCounterHiRes() + 30000;
    while (ui.busy && juce::Time::getMillisecondCounterHiRes() < deadline)
    {
       #if JUCE_WINDOWS
        MSG message; while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
       #endif
        juce::Thread::sleep(1);
    }
    ui.workers.removeAllJobs(false, 10000);
    check(!ui.busy && !ui.gridWindow->isVisible() && std::abs(ui.engine.project.clips[0].tempoSourceBpm - 129.99) < 1e-8,
          "save-and-rebuild applies the corrected source clock to the actual timeline");
    check(std::abs(ui.engine.project.tempo.bpmAt(0) - 129.99) < 1e-8,
          "save-and-rebuild updates the timeline beat grid from the corrected source tempo");
    const auto& appliedClip = ui.engine.project.clips[0];
    check(std::abs(std::remainder(ui.engine.project.gridClock().beatsAt(appliedClip.start + appliedClip.firstDownbeatTime()), 4.0)) < 1.e-6,
          "Apply places the corrected source downbeat on a shared arrangement bar boundary");
    ui.undo(false);
    check(ui.engine.project.clips[0].tempoSourceBpm == preApplyClock && ui.engine.project.clips[0].media->manualBpm == 129.99,
          "Undo restores the previous arrangement clock while keeping the saved source correction");
    ui.undo(true);
    check(ui.engine.project.clips[0].tempoSourceBpm == 129.99, "Redo reinstates arrangement retiming");
    const auto rebuiltSession = report.getSiblingFile("rebuilt.mixworks");
    check(ui.engine.saveProject(rebuiltSession).isEmpty() && ui.engine.restoreProject(rebuiltSession.loadFileAsString()).isEmpty()
          && std::abs(ui.engine.project.clips[0].tempoSourceBpm - 129.99) < 1e-8,
          "updated timeline and corrected source clock survive save/reopen");
    ui.engine.project.snapDivision = 4; ui.timeline.zoomAt(1600 / ui.timeline.pixelsPerSecond, 0); ui.refresh();
    ui.viewport.setViewPosition((int) std::max(0.0, (ui.engine.project.clips[0].start - 0.05) * 1600), 0);
    auto timelineImage = report.getSiblingFile("quarter-beat-timeline.png").createOutputStream();
    check(timelineImage && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *timelineImage), "capture quarter-beat timeline at doubled maximum zoom");
    for (int width : {1250,1920})
    {
        ui.setSize(width,1080); ui.refresh();
        ui.viewport.setViewPosition((int) std::max(0.,(ui.engine.project.clips[0].start-.05)*1600),0);
        auto output = report.getSiblingFile("maximum-zoom-"+juce::String(width)+".png").createOutputStream();
        check(output && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()),*output),"capture actual maximum zoom / "+juce::String(width));
    }
    check(audioDigest(media->file) == sourceDigest, "audition, correction, cache writes and reopen leave original audio unchanged");
}
}
