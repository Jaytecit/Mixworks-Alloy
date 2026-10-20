#include "Workstation.h"
#include <cmath>

namespace mixworks
{
void runTempoUiTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    Workstation ui(false); ui.stopTimer();
    const auto error = ui.engine.restoreProject(report.getSiblingFile("tempo-demo.mixworks").loadFileAsString());
    check(error.isEmpty(), "native tempo UI opens the saved production tempo fixture without opening an audio device: " + error);
    if (error.isNotEmpty()) return;
    ui.selectedClip = ui.engine.project.clips[0].id; ui.timeline.pixelsPerSecond = 35; ui.refresh();
    auto& timeline = ui.timeline; auto& strip = ui.tempoStrip;
    const auto source = juce::Desktop::getInstance().getMainMouseSource();
    auto event = [&](juce::Component& component, juce::Point<float> position, juce::Point<float> down, bool dragged, int modifiers = 0)
    { return juce::MouseEvent(source, position, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier | modifiers), 1, 0, 0, 0, 0, &component, &component, juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), 1, dragged); };
    const auto originalProject = ui.engine.project;
    {
        Workstation points(false); points.stopTimer(); points.setSize(1250, 800);
        points.timeline.pixelsPerSecond = 40;
        const AutomationAddress volume {0, AutomationTarget::volume}, filter {1, AutomationTarget::lowPass};
        auto& gain = points.engine.project.tracks[0].automation[0]; gain.setPoint(4, -6.0206f); gain.setPoint(8, -12);
        points.engine.project.tracks[1].automation[(size_t) AutomationTarget::lowPass].setPoint(4, 10010);
        points.refresh();
        const auto baseline = juce::JSON::toString(projectToJson(points.engine.project)); const auto history = points.undoHistory.size();
        check(points.timeline.automationPointLabel(volume, 0).isEmpty() && points.timeline.automationPointLabel(filter, 0).isEmpty(),
            "unselected automation nodes have no value captions");
        auto click = [&](juce::Point<float> location)
        { points.timeline.mouseDown(event(points.timeline, location, location, false)); points.timeline.mouseUp(event(points.timeline, location, location, false)); };
        click({160, points.timeline.automationY(0, -6.0206f, (int) AutomationTarget::volume)});
        check(points.timeline.automationPointLabel(volume, 0) == "50%" && points.timeline.automationPointLabel(volume, 1).isEmpty()
            && points.timeline.automationPointLabel(filter, 0).isEmpty(), "native node selection shows exactly its percentage after mouse release");
        click({160, points.timeline.automationY(1, 10010, (int) AutomationTarget::lowPass)});
        check(points.timeline.automationPointLabel(volume, 0).isEmpty() && points.timeline.automationPointLabel(filter, 0) == "50%",
            "selecting another curve moves the one value caption to its actual node");
        auto stream = report.getSiblingFile("selected-node-percentage-1250.png").createOutputStream(); juce::PNGImageFormat png;
        check(stream && png.writeImageToStream(points.createComponentSnapshot(points.getLocalBounds()), *stream), "capture only the selected node's percentage in the native timeline");
        click({300, (float) points.timeline.laneTop(2) + 40});
        check(points.timeline.automationPointLabel(volume, 0).isEmpty() && points.timeline.automationPointLabel(filter, 0).isEmpty()
            && juce::JSON::toString(projectToJson(points.engine.project)) == baseline && points.undoHistory.size() == history,
            "background selection clears the node caption without changing automation or Undo");
    }
    const auto originalClip = originalProject.clips[0];
    juce::Point<float> clipDown {70, (float) ui.timeline.laneTop(0) + 65}, clipTo {225, clipDown.y};
    timeline.mouseDown(event(timeline, clipDown, clipDown, false));
    timeline.mouseDrag(event(timeline, clipTo, clipDown, true)); timeline.mouseUp(event(timeline, clipTo, clipDown, true));
    const auto movedClip = ui.engine.project.clips[0];
    check(movedClip.elasticPlayback && !ui.busy && !ui.tempoPreparationPending && !ui.tempoEditBaseline
          && ui.engine.project.tempo.points == originalProject.tempo.points
          && std::abs(std::remainder(ui.engine.project.tempo.beatsAt(movedClip.start + movedClip.firstBeatTime()), 1)) < 1.0e-7,
          "native clip drag aligns to the background grid through a ramp without queuing a tempo/analysis worker");
    ui.undo(false); check(ui.engine.project.clips[0].prepared == originalClip.prepared && ui.engine.project.clips[0].start == originalClip.start, "Undo of a clip move restores the existing prepared audio without processing");
    ui.undo(true); check(ui.engine.project.clips[0].elasticSource == movedClip.elasticSource && !ui.tempoPreparationPending, "Redo of a clip move restores its exact playback mapping without processing");
    ui.engine.project = originalProject; ui.selectedClip = originalClip.id; ui.engine.project.snap = false; ui.refresh();
    clipTo = {173, clipDown.y}; timeline.mouseDown(event(timeline, clipDown, clipDown, false));
    timeline.mouseDrag(event(timeline, clipTo, clipDown, true)); timeline.mouseUp(event(timeline, clipTo, clipDown, true));
    check(std::abs(ui.engine.project.clips[0].start - (originalClip.start + 103 / 35.0)) < 1.0e-8 && !ui.tempoPreparationPending, "Snap off gives exact free placement through the native drag gesture");
    ui.engine.project = originalProject; ui.refresh();
    timeline.mouseDown(event(timeline, clipDown, clipDown, false)); timeline.mouseDrag(event(timeline, clipTo, clipDown, true, juce::ModifierKeys::altModifier)); timeline.mouseUp(event(timeline, clipTo, clipDown, true));
    check(std::abs(ui.engine.project.clips[0].start - (originalClip.start + 103 / 35.0)) < 1.0e-8 && !ui.tempoPreparationPending, "Alt temporarily bypasses clip snapping without changing the Snap toggle");
    ui.engine.project = originalProject; ui.refresh();
    const juce::Point<float> laneTo {clipDown.x, (float) ui.timeline.laneTop(1) + 65};
    timeline.mouseDown(event(timeline, clipDown, clipDown, false)); timeline.mouseDrag(event(timeline, laneTo, clipDown, true)); timeline.mouseUp(event(timeline, laneTo, clipDown, true));
    check(ui.engine.project.clips[0].track == 1 && ui.engine.project.clips[0].start == originalClip.start && ui.engine.project.clips[0].prepared == originalClip.prepared && !ui.tempoPreparationPending,
          "a lane-only move retains timing and the exact prepared PCM");
    ui.engine.project = originalProject; ui.refresh();
    ui.editClip(0, 4.37);
    check(ui.engine.project.clips[0].elasticPlayback && !ui.tempoPreparationPending, "numeric clip-start editing uses the same immediate placement path");
    ui.engine.project = originalProject; ui.refresh(); ui.duplicateClip();
    check(ui.engine.project.clips.size() == 2 && !ui.tempoPreparationPending && !ui.busy, "duplicating a matched clip uses the same placement path without queuing audio preparation");
    ui.engine.project = originalProject; ui.selectedClip = originalClip.id; ui.undoHistory.clear(); ui.redoHistory.clear(); ui.refresh();
    timeline.mouseDown(event(timeline, clipDown, clipDown, false));
    for (float x : {420.0f, 985.0f, 123.0f, 612.0f, clipDown.x}) timeline.mouseDrag(event(timeline, {x, clipDown.y}, clipDown, true));
    timeline.mouseUp(event(timeline, clipDown, clipDown, true));
    check(ui.engine.project.clips[0].start == originalClip.start && ui.engine.project.clips[0].length == originalClip.length
          && ui.engine.project.clips[0].prepared == originalClip.prepared && !ui.tempoPreparationPending,
          "dragging repeatedly across ramps and back introduces no accumulated source/timing drift");
    ui.undoHistory.clear(); ui.redoHistory.clear();
    juce::Point<float> point {280, strip.tempoY()};
    strip.mouseDown(event(strip, point, point, false)); strip.mouseUp(event(strip, point, point, false));
    check(strip.selectedTempoPoint == 1 && std::abs(ui.tempoSlider.getValue() - 150) < 0.001, "clicking a native tempo marker selects its BPM in the editable drag box");
    strip.mouseDown(event(strip, point, point, false));
    const juce::Point<float> moved {315, strip.tempoY() - 30};
    strip.mouseDrag(event(strip, moved, point, true));
    check(strip.draggingTempo && strip.tempoDraft.points[1].time > 8 && strip.tempoDraft.points[1].bpm == 150
          && ui.engine.project.tempo.points[1].bpm == 150, "tempo marker drag changes time while vertical movement leaves BPM and committed audio unchanged");
    strip.draggingTempo = false; strip.moved = false;
    check(Timeline::rulerHeight == 44 && strip.tempoValueBounds(0).getY() == strip.tempoValueBounds(1).getY()
          && strip.tempoValueAt(strip.tempoValueBounds(1).getCentre()) == 1, "bottom tempo values share one height and the numeric label selects the correct edit target");
    check(strip.getHeight() == 22 && strip.getY() == ui.viewport.getBottom() && strip.getParentComponent() == &ui,
          "tempo markers occupy a compact fixed footer instead of the upper timeline header");
    {
        const auto before = juce::JSON::toString(projectToJson(ui.engine.project)); const auto undo = ui.undoHistory.size();
        ui.pendingTempo = 0; ui.tempoPreparationPending = false; ui.engine.playing = true;
        for (double time : {0.0, 2.0, 5.0, 8.0, 12.0, 20.0})
        {
            ui.engine.position = time; ui.timerCallback();
            check(std::abs(ui.tempoSlider.getValue() - ui.engine.project.tempo.bpmAt(time)) <= .0051,
                "global BPM follows the live tempo map at " + juce::String(time) + " seconds with a different marker selected");
        }
        ui.refresh(); check(std::abs(ui.tempoSlider.getValue() - ui.engine.project.tempo.bpmAt(ui.engine.position)) <= .0051, "full UI refresh retains live BPM during playback");
        ui.engine.project.followTrackTempo = false; ui.engine.project.bpm = 137.25; ui.timerCallback();
        check(ui.tempoSlider.getValue() == 137.25, "fixed-tempo playback displays the actual global BPM");
        ui.engine.project = originalProject; ui.engine.playing = false; ui.timerCallback();
        check(ui.tempoSlider.getValue() == 150 && ui.tempoStrip.selectedTempoPoint == 1 && ui.undoHistory.size() == undo
            && juce::JSON::toString(projectToJson(ui.engine.project)) == before, "stopping restores selected-marker editing without changing the mix or Undo");
    }
    auto& box = ui.tempoSlider;
    juce::Point<float> boxStart {80, 15}, boxEnd {80, -35};
    box.mouseDown(event(box, boxStart, boxStart, false)); box.mouseDrag(event(box, boxEnd, boxStart, true)); box.mouseUp(event(box, boxEnd, boxStart, true));
    check(std::abs(box.getValue() - 155) < 0.02 && std::abs(ui.pendingTempo - 155) < 0.02, "native BPM box drags up relatively by 0.1 BPM per pixel without jumping to an absolute value");
    box.showTextBox(); juce::TextEditor* editor = nullptr;
    for (int i = 0; i < box.getNumChildComponents(); ++i)
        if (auto* label = dynamic_cast<juce::Label*>(box.getChildComponent(i))) if (label->getCurrentTextEditor()) editor = label->getCurrentTextEditor();
    if (editor) { editor->setText("127.35"); ui.timerCallback(); check(editor->getText() == "127.35", "transport refresh preserves in-progress BPM typing"); box.hideTextBox(false); }
    check(editor != nullptr && std::abs(box.getValue() - 127.35) < 0.001 && std::abs(ui.pendingTempo - 127.35) < 0.001, "typing and committing a precise BPM updates the native control and pending tempo edit");
    ui.pendingTempo = 0; ui.refresh();
    bool images = true;
    for (const auto size : {juce::Point<int> {1440, 940}, juce::Point<int> {1250, 800}})
    {
        ui.setSize(size.x, size.y); ui.refresh();
        const auto image = ui.createComponentSnapshot(ui.getLocalBounds());
        auto file = report.getSiblingFile("tempo-ui-" + juce::String(size.x) + ".png"); auto stream = file.createOutputStream(); juce::PNGImageFormat format;
        if (stream) { stream->setPosition(0); stream->truncate(); }
        images = images && stream && format.writeImageToStream(image, *stream);
        const int headerY = ui.timeline.laneTop(1) - ui.viewport.getViewPositionY() + ui.viewport.getY();
        images = images && headerY >= ui.viewport.getY() && ui.timeline.laneTop(0) == 0
            && strip.getY() == ui.viewport.getBottom() && strip.getBottom() <= ui.layout.status.getY();
        bool contained = true, noOverlap = true;
        for (int i = 0; i < ui.getNumChildComponents(); ++i)
        {
            const auto* child = ui.getChildComponent(i); if (!child->isVisible()) continue;
            contained = contained && ui.getLocalBounds().contains(child->getBounds());
            for (int j = i + 1; j < ui.getNumChildComponents(); ++j) { const auto* other = ui.getChildComponent(j); if (other->isVisible()) noOverlap = noOverlap && !child->getBounds().intersects(other->getBounds()); }
        }
        check(contained && noOverlap, "visible workstation tools fit without overlapping at " + juce::String(size.x) + " pixels");
    }
    check(images, "native timeline snapshots render at desktop and minimum window sizes with shared tempo/track header geometry");
    
    ui.selectAutomationTarget(0);
    const int footerY = strip.getY(); ui.viewport.setViewPosition(140, 120);
    check(ui.viewport.getViewPositionY() == 0 && strip.getY() == footerY && strip.getWidth() == ui.viewport.getViewWidth()
          && std::abs(strip.markerX(1) + ui.viewport.getViewPositionX() - 280) < 0.01,
          "all four lanes stay visible while footer pins follow horizontal scrolling immediately");
    check(ui.getLocalPoint(ui.strips[1].get(), juce::Point<int>{0, 0}).y == ui.viewport.getY() + ui.timeline.laneTop(1) && ui.strips[1]->getParentComponent() == &ui.trackHeaders, "lane controls stay aligned with the four fitted timeline rows");
    const auto pin = juce::Point<float>(strip.markerX(1), strip.tempoY());
    strip.mouseDown(event(strip, pin, pin, false));
    const auto target = juce::Point<float>((float) (9.1234 * ui.timeline.pixelsPerSecond - ui.viewport.getViewPositionX()), -20);
    strip.mouseDrag(event(strip, target, pin, true, juce::ModifierKeys::altModifier));
    check(std::abs(strip.tempoDraft.points[1].time - 9.1234) < 0.00001 && strip.tempoDraft.points[1].bpm == 150,
          "a scrolled tempo pin uses source time, preserves BPM on vertical movement and honours Alt to bypass snapping");
    strip.draggingTempo = strip.moved = false;
    const auto value = strip.tempoValueBounds(1).getCentre(); strip.mouseDown(event(strip, value, value, false));
    auto* dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
    check(dialog && std::abs(dialog->getTextEditorContents("bpm").getDoubleValue() - 150) < 0.001, "clicking the footer BPM label opens the existing precise numeric editor");
    if (dialog) dialog->exitModalState(0);
    const juce::Point<float> empty {300, strip.tempoY()}; strip.mouseDoubleClick(event(strip, empty, empty, false));
    dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
    check(dialog && std::abs(dialog->getTextEditorContents("time").getDoubleValue() - (300 + ui.viewport.getViewPositionX()) / 35.0) < 0.000001,
          "adding a footer marker opens at the correct time after horizontal scrolling");
    if (dialog) dialog->exitModalState(0);
    const auto savedTempo = ui.engine.project.tempo;
    strip.selectedTempoPoint = 0; strip.removeSelected();
    check(!ui.busy && ui.engine.project.tempo.points == savedTempo.points, "the first tempo marker cannot be removed from the footer");
    ui.engine.project.tempo.setPoint(900, 125); timeline.updateSize(); ui.viewport.setViewPosition(31500 - strip.getWidth() / 2, 0);
    check(std::abs(strip.markerX((int) ui.engine.project.tempo.points.size() - 1) - strip.getWidth() / 2) < 0.01,
          "tempo markers beyond the last clip remain reachable by horizontal scrolling");
    ui.engine.project.tempo = savedTempo; timeline.updateSize(); strip.selectedTempoPoint = 1;
    ui.selectAutomationTarget(-1);
    ui.viewport.setViewPosition(0, 0); ui.layoutTrackHeaders();
    for (int page : {1, 2})
    {
        ui.menuItemSelected(page == 1 ? 20 : 21, 3);
        check(ui.masterSlider.isVisible() && !ui.clipSliders[0].isVisible() && (page == 1 ? ui.pluginList.list.isVisible() && !ui.piano.isVisible() : ui.piano.isVisible() && !ui.pluginList.list.isVisible()),
              page == 1 ? "View / Effects exposes VST tools while retaining master output" : "View / MIDI exposes the keyboard while retaining master output");
        const auto image = ui.createComponentSnapshot(ui.getLocalBounds()); auto file = report.getSiblingFile(page == 1 ? "workspace-effects-1250.png" : "workspace-midi-1250.png"); auto stream = file.createOutputStream();
        if (stream) { stream->setPosition(0); stream->truncate(); juce::PNGImageFormat format; format.writeImageToStream(image, *stream); }
    }
    ui.menuItemSelected(19, 3);
    // Run the real message queue once to exercise worker preparation and UI commit/undo,
    // including the continuation used by Save/Export when an edit is still pending.
    bool continued = false;
    ui.tempoSlider.setValue(127.35, juce::sendNotificationSync);
    check(!ui.ensureTempoReady([&] { continued = true; }), "save/export waits for a pending tempo edit to finish preparation");
    struct Poll final : juce::Timer { std::function<void()> tick; void timerCallback() override { tick(); } } poll;
    int phase = 0; const auto deadline = juce::Time::getMillisecondCounterHiRes() + 15000;
    poll.tick = [&]
    {
        if (juce::Time::getMillisecondCounterHiRes() > deadline) { check(false, "native tempo worker completes before the test deadline"); poll.stopTimer(); juce::MessageManager::getInstance()->stopDispatchLoop(); return; }
        if (ui.busy) return;
        if (phase == 0)
        {
            check(continued && std::abs(ui.engine.project.tempo.points[1].bpm - 127.35) < 0.001 && !ui.undoHistory.empty(), "typed BPM commits through the real worker/message queue before save/export continues");
            const auto changedPcm = ui.engine.project.clips[0].prepared;
            bool mappedUndo = changedPcm && changedPcm->cache && changedPcm->residentBytes() == 0;
            for (const auto& snapshot : ui.undoHistory) for (const auto& clip : snapshot.clips) if (clip.prepared) mappedUndo &= clip.prepared->cache && clip.prepared->residentBytes() == 0;
            check(mappedUndo, "native tempo editing retains mapped playback in the current project and undo history without PCM RAM copies");
            ui.undo(false); check(ui.engine.project.tempo.points[1].bpm == 150, "Undo restores the tempo map and prepared playback");
            ui.undo(true); check(std::abs(ui.engine.project.tempo.points[1].bpm - 127.35) < 0.001 && ui.engine.project.clips[0].prepared == changedPcm, "Redo restores the prepared tempo edit without reprocessing audio");
            const juce::Point<float> from {280, strip.tempoY()}, to {315, strip.tempoY() - 30};
            strip.mouseDown(event(strip, from, from, false)); strip.mouseDrag(event(strip, to, from, true)); strip.mouseUp(event(strip, to, from, true)); ++phase; return;
        }
        if (phase == 1)
        {
            check(ui.engine.project.tempo.points[1].time > 8 && std::abs(ui.engine.project.tempo.points[1].bpm - 127.35) < 0.02, "releasing a dragged tempo marker prepares audio for its new time while retaining the numeric BPM");
            strip.selectedTempoPoint = 1; strip.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey)); ++phase; return;
        }
        check(ui.engine.project.tempo.points.size() == savedTempo.points.size() - 1, "Delete on the focused footer removes the selected tempo marker through audio preparation");
        ui.undo(false);
        check(ui.engine.project.tempo.points.size() == savedTempo.points.size() && std::abs(ui.engine.project.tempo.points[1].bpm - 127.35) < 0.02,
              "Undo restores a tempo marker deleted from the footer");
        poll.stopTimer(); juce::MessageManager::getInstance()->stopDispatchLoop();
    };
    poll.startTimer(20); juce::MessageManager::getInstance()->runDispatchLoop();
}
}
