#include "Workstation.h"
#include "EnergySections.h"
#include <cmath>
#include <limits>
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace mixworks
{
void runSectionFlagTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    const auto json = [](const Project& p) { return juce::JSON::toString(projectToJson(p), true, 17); };
    auto media = std::make_shared<Media>(); media->name = "Section flag source"; media->sampleRate = 8000;
    media->manualBpm = 120; media->manualBeatOrigin = 0; media->analysis.downbeatOrigin = 0;
    media->file = report.getSiblingFile("section-source.wav"); media->samples.setSize(1, 96 * 8000);
    for (int n = 0; n < media->frames(); ++n)
    {
        const double t = n / media->sampleRate;
        media->samples.setSample(0, n, (float) ((t < 32 || t >= 64 ? .08 : .4) * std::sin(n * .2356194490192345)));
    }
    // Retained RMS is produced from these actual quiet/loud PCM regions.
    media->prepareWaveformBody();
    {
        juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream = media->file.createOutputStream();
        auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(1).withBitsPerSample(24));
        check(writer && writer->writeFromAudioSampleBuffer(media->samples, 0, media->frames()), "section flags write owned real PCM for Save/Open");
    }
    const auto rms = media->waveformRms, peaks = media->peaks;
    media->analysisSource = {audioDigest(media->file),media->file.getSize(),media->sampleRate,media->frames(),media->channels()};
    media->sourceModified = media->file.getLastModificationTime().toMilliseconds();
    const auto automatic = detectEnergySections(*media);
    check(automatic.size() == 2 && std::abs(automatic[0].sourceSeconds - 32) < .001 && std::abs(automatic[1].sourceSeconds - 64) < .001,
        "section-flag fixture uses two measured quiet/loud boundaries in actual PCM");
    if (automatic.size() != 2) return;
    Project base; base.transitionLinksKnown = true;
    Clip clip; clip.media = media; clip.start = 4; clip.length = 95; clip.audioSlip = .017; clip.controls.pan = .15f;
    clip.controls.automation[0] = {true, {{0, -3}, {40, -6}}}; base.clips = {clip};
    const auto id = clip.id;
    check(base.setSectionMarkerTime(id, 0, automatic[0].sourceSeconds).wasOk() && !base.clips[0].sectionMarkers,
        "view/no-op does not materialise automatic annotations or change source data");
    auto edited = base; const auto result = edited.setSectionMarkerTime(id, 0, 33.125);
    check(result.wasOk() && edited.clips[0].sectionMarkers && edited.clips[0].sectionMarkers->size() == 2,
        "first real section edit materialises independent stable source annotations: " + result.getErrorMessage());
    if (!edited.clips[0].sectionMarkers) return;
    const auto markers = *edited.clips[0].sectionMarkers;
    check(markers[0].id.isNotEmpty() && markers[0].id != markers[1].id && markers[0].manual && markers[0].contrastDb == 0
        && !markers[1].manual && markers[1].sourceSeconds == automatic[1].sourceSeconds,
        "manual flag has stable identity and no invented energy evidence; other suggestions retain their provenance");
    check(edited.clips[0].media == media && media->waveformRms == rms && media->peaks == peaks
        && detectEnergySections(*media)[0].sourceSeconds == 32 && edited.clips[0].controls.pan == .15f,
        "annotation edit preserves shared analysis, waveform data, source identity and mixer controls");
    const auto movedAgain = edited.setSectionMarkerTime(id, 0, 34.25);
    check(movedAgain.wasOk() && (*edited.clips[0].sectionMarkers)[0].id == markers[0].id, "subsequent moves retain the same manual flag identity");
    for (int failure = 0; failure < 8; ++failure)
    {
        auto denied = edited; int index = 0; double source = 35; juce::String target = id;
        if (failure == 0) source = 64;
        if (failure == 1) source = std::numeric_limits<double>::quiet_NaN();
        if (failure == 2) source = -1;
        if (failure == 3) index = 5;
        if (failure == 4) target = "removed-segment";
        if (failure == 5) denied.clips[0].reversed = true;
        if (failure == 6) denied.clips[0].loopSpan = 8;
        if (failure == 7) denied.clips[0].pendingPlacement = PendingPlacement::move;
        const auto before = json(denied);
        check(denied.setSectionMarkerTime(target, index, source).failed() && json(denied) == before,
            "invalid/ordering/reverse/loop/preparation section edit rejects atomically " + juce::String(failure));
    }
    auto locked = edited; auto neighbour = clip; neighbour.id = juce::Uuid().toString(); neighbour.start = 90; neighbour.track = 1;
    locked.clips.push_back(neighbour); locked.transitionLinks = {{id, neighbour.id}}; locked.transitionLinks[0].locked = true;
    const auto lockedBefore = json(locked);
    check(locked.setSectionMarkerTime(id, 0, 35).failed() && locked.resetSectionMarkers(id).failed() && json(locked) == lockedBefore,
        "locked participant rejects section moves and reset without changing its transition");
    auto bypass = locked; (*bypass.clips[0].sectionMarkers)[0].sourceSeconds += .5;
    check(locked.validateTransitionLocks(bypass).failed(), "candidate lock validation includes instance-owned annotations");
    auto copied = edited.clips[0]; copied.id = juce::Uuid().toString(); (*copied.sectionMarkers)[0].sourceSeconds += 1;
    const auto sliced = edited.clips[0].slice(24, 48);
    check((*edited.clips[0].sectionMarkers)[0].sourceSeconds == 34.25 && sliced.sectionMarkers == edited.clips[0].sectionMarkers
        && copied.sectionMarkers != edited.clips[0].sectionMarkers, "copy and split inherit independent source coordinates, including annotations trimmed outside view");
    auto mapped = edited.clips[0]; mapped.offset = 20; mapped.audioSlip = -.123; mapped.start = 13;
    mapped.tempoMap = std::make_shared<const TempoMap>(TempoMap{120, {{0, 120, false}, {30, 144, true}, {80, 128, false}}, 0});
    mapped.tempoSourceBpm = 120;
    for (double source : {34.25, 64.0})
    {
        const double local = mapped.audioTimelineTime(source - mapped.offset);
        check(std::abs(mapped.sourceTimeAt(local) - source) < 1.e-8, "absolute section source time maps through trim/slip/changing tempo at " + juce::String(source));
    }
    auto added = edited;
    check(added.setSectionMarkerTime(id,-1,20.125,"Verse").wasOk() && added.clips[0].sectionMarkers->size() == 3
        && (*added.clips[0].sectionMarkers)[0].label == "Verse", "insert a source label before existing sections through trim/slip mapping");
    const auto addedJson = json(added);
    check(added.setSectionMarkerTime(id,-1,20.125,"Duplicate").failed() && added.setSectionMarkerTime(id,-1,0,"Start").failed()
        && added.setSectionMarkerTime(id,-1,50,juce::String::repeatedString("x",129)).failed() && json(added) == addedJson,
        "duplicate, source-start and oversized labels reject atomically");
    auto noSections = edited; noSections.clips[0].sectionMarkers = std::vector<SourceSectionMarker>{};
    check(noSections.setSectionMarkerTime(id,-1,20,"First").wasOk() && noSections.clips[0].sectionMarkers->size() == 1,
        "manual boundary works without automatic suggestions");
    check(locked.setSectionMarkerTime(id,-1,20,"Locked").failed() && json(locked) == lockedBefore,"locked transitions reject boundary insertion");
    auto malformedLabel = projectToJson(added); (*(*malformedLabel["clips"].getArray())[0]["sectionMarkers"].getArray())[0].getDynamicObject()->setProperty("label",42);
    check(parseProject(juce::JSON::toString(malformedLabel)).error.isNotEmpty(),"session rejects malformed section labels");
    const auto labelledRoundTrip = parseProject(addedJson);
    check(labelledRoundTrip.error.isEmpty() && labelledRoundTrip.clips[0].clip.sectionMarkers == added.clips[0].sectionMarkers,"session35 round-trips inserted source labels");
    const auto parsed = parseProject(json(edited));
    check(parsed.error.isEmpty() && parsed.clips.size() == 1 && parsed.clips[0].clip.sectionMarkers == edited.clips[0].sectionMarkers,
        "session31 strictly round-trips section identity, source time and provenance: " + parsed.error);
    auto legacy = projectToJson(edited); legacy.getDynamicObject()->setProperty("version", 30);
    (*legacy["clips"].getArray())[0].getDynamicObject()->removeProperty("sectionMarkers");
    const auto old = parseProject(juce::JSON::toString(legacy));
    check(old.error.isEmpty() && !old.clips[0].clip.sectionMarkers, "session30 opens with automatic section suggestions without migration guesses");
    for (int failure = 0; failure < 7; ++failure)
    {
        auto bad = juce::JSON::parse(json(edited)); auto c = (*bad["clips"].getArray())[0]; auto* records = c["sectionMarkers"].getArray();
        if (failure == 0) c.getDynamicObject()->removeProperty("sectionMarkers");
        if (failure == 1) c.getDynamicObject()->setProperty("sectionMarkers", "bad");
        if (failure == 2) (*records)[1].getDynamicObject()->setProperty("id", (*records)[0]["id"]);
        if (failure == 3) (*records)[1].getDynamicObject()->setProperty("sourceSeconds", 0);
        if (failure == 4) (*records)[0].getDynamicObject()->setProperty("manual", "yes");
        if (failure == 5) (*records)[0].getDynamicObject()->setProperty("sourceSeconds", -1);
        if (failure == 6) (*records)[0].getDynamicObject()->setProperty("id", juce::String());
        check(parseProject(juce::JSON::toString(bad)).error.isNotEmpty(), "malformed session31 annotations reject before commit " + juce::String(failure));
    }
    auto dry = std::make_unique<Engine>(juce::File(), false), wet = std::make_unique<Engine>(juce::File(), false);
    dry->project = base; wet->project = edited; dry->sampleRate = wet->sampleRate = 8000;
    juce::AudioBuffer<float> a(2, 256), b(2, 256); juce::MidiBuffer midi;
    for (double time : {12.0, 36.0, 69.0})
    {
        dry->render(a, 256, time, midi); wet->render(b, 256, time, midi); double error = 0;
        for (int ch = 0; ch < 2; ++ch) for (int n = 0; n < 256; ++n) error = std::max(error, (double) std::abs(a.getSample(ch, n) - b.getSample(ch, n)));
        check(error == 0 && a.getRMSLevel(0, 0, 256) > .001, "editing section annotations leaves production PCM identical at " + juce::String(time));
    }
    auto owned = std::make_unique<Workstation>(false); auto& ui = *owned; ui.stopTimer(); ui.setSize(1920, 1080);
    ui.engine.project = base; ui.engine.library = {media}; ui.selectedClip = id; ui.showEnergySections = true; ui.timeline.pixelsPerSecond = 10;
    ui.selectTrack(0); ui.refresh(); ui.engine.position = 10;
    const auto hiddenDocument = json(ui.engine.project);
    check(ui.timeline.sectionFlags().empty() && ui.timeline.hotcueFlags().empty(), "HC and S waveform labels are hidden by default");
    ui.timeline.chooseCurveTool(8);
    check(ui.timeline.sectionFlags().size() == 2 && ui.timeline.hotcueFlags().empty()
        && json(ui.engine.project) == hiddenDocument && ui.undoHistory.empty(), "S footer toggle reveals only section labels without editing the arrangement");
    ui.timeline.chooseCurveTool(8);
    check(ui.timeline.sectionFlags().empty(), "S footer toggle hides its flags and mouse targets again");
    ui.timeline.chooseCurveTool(8);
    const auto flags = ui.timeline.sectionFlags();
    check(flags.size() == 2 && ui.timeline.sectionFlagAt(flags[0].bounds.getCentre()).has_value(), "title-edge flags expose the actual painted mouse hit targets");
    if (flags.size() != 2) return;
    const auto mouse = juce::Desktop::getInstance().getMainMouseSource();
    auto event = [&](juce::Point<float> at, juce::Point<float> down, bool drag, int extra = 0)
    { return juce::MouseEvent(mouse, at, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier | extra), 1, 0, 0, 0, 0, &ui.timeline, &ui.timeline, juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), 1, drag); };
    const auto point = flags[0].bounds.getCentre(); const auto beforeFocus = json(ui.engine.project);
    ui.timeline.mouseDown(event(point, point, false));
    check(ui.timeline.selectedSection && json(ui.engine.project) == beforeFocus && ui.engine.position == 10 && ui.undoHistory.empty(),
        "mouse flag selection is view-only and does not seek or materialise suggestions");
    ui.timeline.mouseDrag(event(point.translated(13, 0), point, true, juce::ModifierKeys::altModifier));
    ui.timeline.mouseDrag(event(point.translated(17, 0), point, true, juce::ModifierKeys::altModifier)); ui.timeline.mouseUp(event(point.translated(17, 0), point, true));
    check(ui.engine.project.clips[0].sectionMarkers && std::abs((*ui.engine.project.clips[0].sectionMarkers)[0].sourceSeconds - 33.7) < 1.e-7
        && ui.undoHistory.size() == 1 && ui.engine.position == 10, "real Alt flag gesture preserves phase and uses one Undo across drag events");
    ui.undo(false); check(json(ui.engine.project) == beforeFocus, "Undo returns to automatic suggestions without retained materialisation");
    ui.undo(true); check(ui.engine.project.clips[0].sectionMarkers && (*ui.engine.project.clips[0].sectionMarkers)[0].sourceSeconds > 33, "Redo restores edited source flags");
    auto pump = []
    {
        const auto until = juce::Time::getMillisecondCounterHiRes() + 40;
        do {
#if JUCE_WINDOWS
            MSG msg; while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
#endif
            juce::Thread::sleep(1);
        } while (juce::Time::getMillisecondCounterHiRes() < until);
    };
    auto edit = [&](const juce::String& text, int answer)
    {
        const auto current = ui.timeline.sectionFlags(true); ui.timeline.editSectionFlag(current[0]); pump();
        auto* dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        check(dialog && dialog->getName() == "Section flag", "section exact editor is the real guarded native dialog");
        if (dialog) { if (text.isNotEmpty()) dialog->getTextEditor("time")->setText(text); dialog->exitModalState(answer); pump(); }
    };
    const auto oldUndo = ui.undoHistory.size(); const auto beforeCancel = json(ui.engine.project);
    edit("41.25", 0); check(json(ui.engine.project) == beforeCancel && ui.undoHistory.size() == oldUndo, "Cancel keeps section annotations/history");
    edit("", 1); check(json(ui.engine.project) == beforeCancel && ui.undoHistory.size() == oldUndo, "unchanged displayed exact time never quantises a saved source position");
    edit("41.25", 1);
    check(std::abs((*ui.engine.project.clips[0].sectionMarkers)[0].sourceSeconds - (41.25 - clip.start - clip.audioSlip)) < 1.e-8
        && ui.undoHistory.size() == oldUndo + 1, "exact project time maps into source time with slip and creates one Undo");
    const auto exact = json(ui.engine.project); edit("not a time", 1); check(json(ui.engine.project) == exact, "invalid exact entry cannot move a flag");
    {
        ui.timeline.editSectionFlag(ui.timeline.sectionFlags(true)[0]); pump();
        auto* dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        ui.engine.project.labels.push_back({juce::Uuid().toString(), 1, "Changed while open"}); const auto stale = json(ui.engine.project);
        if (dialog) { dialog->getTextEditor("time")->setText("42"); dialog->exitModalState(1); pump(); }
        check(dialog && json(ui.engine.project) == stale, "stale section dialog cannot commit into a changed project");
    }
    edit("", 2); check(!ui.engine.project.clips[0].sectionMarkers && ui.engine.project.labels.size() == 1, "Use suggestions resets only this segment's overrides");
    ui.undo(false); check(ui.engine.project.clips[0].sectionMarkers.has_value(), "reset annotations is undoable");
    ui.addToDesktop(0); ui.setVisible(true); ui.timeline.refreshAccessibleItems(); bool native = false, discovered = false;
    for (auto& item : ui.timeline.accessibleItems)
    {
        if (!item->getComponentID().startsWith("section:")) continue;
        auto* handler = item->getAccessibilityHandler(); auto* value = handler ? handler->getValueInterface() : nullptr;
        if (!handler || !value) continue;
        const auto before = json(ui.engine.project); const auto history = ui.undoHistory.size(); const double position = ui.engine.position;
        auto* parent = ui.timeline.getAccessibilityHandler(); const auto children = parent->getChildren();
        discovered = handler->getParent() == parent && std::find(children.begin(), children.end(), handler) != children.end();
        value->setValue(70); value->setValueAsString("70"); handler->getActions().invoke(juce::AccessibilityActionType::focus);
        native = value->isReadOnly() && value->getCurrentValueAsString().contains("source seconds")
            && json(ui.engine.project) == before && ui.undoHistory.size() == history && ui.engine.position == position;
        break;
    }
    check(discovered, "section flag is discoverable in the actual native timeline list with its correct parent");
    check(native, "native source-value objects use readonly real annotations and select without seeking/history");
    const auto selected = ui.timeline.sectionFlags(true)[0]; ui.timeline.selectSectionFlag(selected); const auto keyboardUndo = ui.undoHistory.size();
    ui.timeline.keyPressed(juce::KeyPress(juce::KeyPress::upKey));
    check(std::abs((*ui.engine.project.clips[0].sectionMarkers)[0].sourceSeconds - selected.source - .5) < 1.e-7
        && ui.undoHistory.size() == keyboardUndo + 1, "selected section Up moves one actual project beat using the guarded setter");
    const auto protectedBefore = json(ui.engine.project); ui.engine.automationRecordMode = AutomationRecordMode::touch;
    ui.timeline.keyPressed(juce::KeyPress(juce::KeyPress::downKey)); check(json(ui.engine.project) == protectedBefore, "record mode blocks section keyboard changes");
    ui.engine.automationRecordMode = AutomationRecordMode::read;
    const auto session = report.getSiblingFile("section-flags.mixworks"); auto reopened = std::make_unique<Engine>(juce::File(), false);
    const auto saved = ui.engine.saveProject(session), opened = reopened->restoreProject(session.loadFileAsString());
    check(saved.isEmpty() && opened.isEmpty() && reopened->project.clips.size() == 1
        && reopened->project.clips[0].sectionMarkers == ui.engine.project.clips[0].sectionMarkers,
        "real Save/Open retains section flags and source playback in format31: " + saved + opened);
    for (const int width : {1250, 1920})
    {
        ui.setSize(width, 1080); ui.refresh(); ui.timeline.selectSectionFlag(ui.timeline.sectionFlags(true)[0]);
        auto stream = report.getSiblingFile("section-flags-" + juce::String(width) + ".png").createOutputStream(); juce::PNGImageFormat png;
        check(stream && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *stream), "capture real title-edge section flags at width " + juce::String(width));
    }
    ui.engine.project.clips[0].reversed = true; ui.refresh(); check(ui.timeline.sectionFlags(true).empty(), "reverse metadata survives while occurrence-aware flags stay unavailable");
    ui.selectedAutomationTarget = (int) AutomationTarget::bass; ui.selectTrack(3); ui.refresh();
    ui.newSession();
    bool noCurves = true;
    for (int lane = 0; lane < trackCount; ++lane) noCurves &= ui.timeline.displayedAutomationAddresses(lane).empty();
    check(ui.engine.project.clips.empty() && ui.automationLane() == -1 && noCurves && ui.undoHistory.empty()
        && ui.timeline.visibleHotcueClips.empty() && ui.timeline.visibleSectionClips.empty(),
        "New after Discard clears the selected bass view and marker visibility on all empty lanes");
    auto emptyStream = report.getSiblingFile("new-empty-timeline.png").createOutputStream(); juce::PNGImageFormat png;
    check(emptyStream && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *emptyStream), "capture empty timeline after New with bass previously selected on lane four");
}
}
