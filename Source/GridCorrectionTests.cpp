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
void runGridCorrectionTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
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
    check(editor && editor->draft.bpm == 130.5 && editor->draft.key == 3, "reopened source editor shows saved BPM and key");
    editor->bpm.setText("129.99", false); editor->phase.setText("0.125", false); editor->changed();
    editor->rebuild.onClick();
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
