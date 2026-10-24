#include "Workstation.h"
#include "ArrangementInsertion.h"
#include "Beatmatch.h"
#include "Hotcues.h"
#include <cmath>
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace mixworks
{
void runArrangementInsertionTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    const auto json = [](const Project& p) { return juce::JSON::toString(projectToJson(p), true, 17); };
    auto source = std::make_shared<Media>(); source->name = "Established mix source"; source->sampleRate = 8000;
    source->manualBpm = 120; source->manualBeatOrigin = .213; source->analysis.downbeatOrigin = .213;
    source->file = report.getSiblingFile("insertion-old.wav"); source->samples.setSize(1, 96 * 8000);
    for (int n = 0; n < source->frames(); ++n) source->samples.setSample(0, n, .15f * (float) std::sin(n * .043));
    auto extra = source->catalogueCopy(); extra->id = juce::Uuid().toString(); extra->name = "New song";
    extra->manualBpm = 128; extra->manualBeatOrigin = .173; extra->analysis.downbeatOrigin = .173;
    extra->file = report.getSiblingFile("insertion-new.wav"); extra->samples.makeCopyOf(source->samples);
    for (const auto& media : {source, extra})
    {
        juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream = media->file.createOutputStream();
        auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(1).withBitsPerSample(24));
        check(writer && writer->writeFromAudioSampleBuffer(media->samples, 0, media->frames()), "write owned insertion source " + media->name);
        writer.reset();
        media->analysisSource = {audioDigest(media->file), media->file.getSize(), media->sampleRate, media->frames(), media->channels()};
        media->sourceModified = media->file.getLastModificationTime().toMilliseconds();
    }
    Project base; base.transitionLinksKnown = true;
    base.tempo = {120, {{0, 120, false}, {40, 132, true}, {80, 124, true}, {140, 128, false}}, .213};
    const auto clock = std::make_shared<const TempoMap>(base.tempo);
    for (int n = 0; n < 3; ++n)
    {
        Clip c; c.media = source; c.start = 40 * n; c.length = 48; c.track = n; c.beatmatched = true;
        c.tempoSourceBpm = 120; c.tempoMap = clock; c.rate = c.sourceElapsed(c.length) / c.length;
        c.audioSlip = .017; c.offset = n == 1 ? .5 : 0; c.controls.pan = .1f * n;
        c.sectionMarkers = std::vector<SourceSectionMarker>{{juce::Uuid().toString(), 24, 0, true}};
        c.controls.automation[2] = {true, {{0, -2}, {32, -4}}}; base.clips.push_back(c);
        initialiseHotcues(base.clips.back());
    }
    for (int n = 1; n < 3; ++n)
    {
        auto& a = base.clips[(size_t)n - 1]; auto& b = base.clips[(size_t)n]; const juce::Range<double> range{b.start, a.end()};
        a.outroAnchor = a.sourceElapsed(range.getStart() - a.start); b.introAnchor = 0;
        initialiseTransition(a, b, range.getLength(), TransitionPreset::crossfade, range);
        base.transitionLinks.push_back({a.id, b.id});
    }
    base.labels = {{juce::Uuid().toString(), 12, "Earlier"}, {juce::Uuid().toString(), 44, "Incoming"}, {juce::Uuid().toString(), 100, "Later"}};
    for (size_t n = 0; n < base.clips.size(); ++n) base.tempo.points[n].clipId = base.clips[n].id;
    base.masterAutomation = {true, {{0, -3, -.5}, {39, -6, .3}, {70, -4, 1}, {80, -8}, {110, -2}}};
    base.tracks[1].automation[0] = {true, {{0, -2, .4}, {30, -5}, {45, -3, -.4}, {80, -8}}};
    const auto prepared = prepareSavedClips(base);
    check(prepared.isEmpty() && base.clips[0].prepared && base.clips[1].prepared, "existing insertion fixture has genuine mapped prepared PCM: " + prepared);
    if (prepared.isNotEmpty()) return;
    const auto original = json(base); const auto aId = base.clips[0].id, bId = base.clips[1].id, cId = base.clips[2].id;
    auto middle = planArrangementInsertion(base, extra, bId, false, 16, TransitionPreset::bassSwap);
    check(middle.error.isEmpty(), "insert a song between two prepared overlapping songs across tempo ramps: " + middle.error);
    if (middle.error.isNotEmpty()) return;
    check(json(base) == original && middle.project.clips.size() == 4 && middle.moved == 2 && middle.displacement > 0,
        "planning is atomic and shifts the whole later suffix across lanes without altering the input");
    auto afterA = planArrangementInsertion(base, extra, aId, true, 16, TransitionPreset::bassSwap);
    check(afterA.error.isEmpty() && std::abs(afterA.displacement - middle.displacement) < 1.e-8,
        "Insert after A and Insert before B use the same chronological boundary");
    const auto* newClip = middle.project.clipFor(middle.inserted);
    check(newClip && newClip->track != 0 && newClip->track != 1 && middle.sourceEnd <= extra->duration()
        && extra->duration() - middle.sourceEnd < 4 * 60 / extra->bpm() + 1.e-7,
        "new source has a free lane and at most one displayed bar of phase-preserving tail trim");
    bool clocks = true, ownership = true;
    for (const auto& old : base.clips)
    {
        const auto* now = middle.project.clipFor(old.id); const double delta = old.start >= middle.suffixStart ? middle.displacement : 0;
        ownership &= now && now->prepared == old.prepared && now->effects == old.effects && now->sectionMarkers == old.sectionMarkers && now->hotcues == old.hotcues
            && now->offset == old.offset && now->audioSlip == old.audioSlip && now->length == old.length && now->start == old.start + delta;
        for (int n = 0; now && n <= 48; ++n) clocks &= std::abs(old.sourceTimeAt(n) - now->sourceTimeAt(n)) < 1.e-7;
    }
    check(clocks && ownership, "every existing voice retains prepared/effects/annotations/geometry and its exact local source clock, including the retained overlapping outgoing voice");
    auto ahead = base; ahead.clips[0].audioSlip = -.317; ahead.clips[1].audioSlip = .413;
    const auto slipped = planArrangementInsertion(ahead, extra, bId, false, 16, TransitionPreset::crossfade);
    double slipError = 0;
    if (slipped.error.isEmpty()) for (const auto& old : ahead.clips)
        for (double local : {0., .001, .1, 24., 47.99, 48.})
            slipError = std::max(slipError, std::abs(old.sourceTimeAt(local) - slipped.project.clipFor(old.id)->sourceTimeAt(local)));
    check(slipped.error.isEmpty() && slipError < 1.e-7, "translated clock covers positive/negative slip beyond visible edges, including source-offset audio: " + slipped.error + " / " + juce::String(slipError, 10));
    check(middle.project.transitionFor(cId) && *middle.project.transitionFor(cId) == base.transitionLinks[1]
        && middle.project.transitionFor(bId)->outgoing == middle.inserted && middle.project.transitionFor(middle.inserted)->outgoing == aId,
        "only A-B is replaced by A-new/new-B; later B-C treatment/identities stay intact");
    double curves = 0, tempo = 0;
    const double prefixEnd = base.clips[0].end();
    for (double t = 0; t < 128; t += .137)
    {
        if (t < prefixEnd)
        {
            curves = std::max(curves, (double) std::abs(base.masterAutomation.interpolatedValue(t, 0) - middle.project.masterAutomation.interpolatedValue(t, 0)));
            tempo = std::max(tempo, std::abs(base.tempo.bpmAt(t) - middle.project.tempo.bpmAt(t)));
        }
        if (t >= middle.suffixStart)
        {
            curves = std::max(curves, (double) std::abs(base.masterAutomation.interpolatedValue(t, 0) - middle.project.masterAutomation.interpolatedValue(t + middle.displacement, 0)));
            curves = std::max(curves, (double) std::abs(base.tracks[1].automation[0].interpolatedValue(t, 0) - middle.project.tracks[1].automation[0].interpolatedValue(t + middle.displacement, 0)));
            tempo = std::max(tempo, std::abs(base.tempo.bpmAt(t) - middle.project.tempo.bpmAt(t + middle.displacement)));
        }
    }
    check(curves < 2.e-6 && tempo < 1.e-7, "prefix and duplicated/translated suffix preserve curved/stepped session automation and ramped BPM: " + juce::String(curves, 8));
    check(middle.project.labels[0].time == 12 && middle.project.labels[1].time == 44 + middle.displacement
        && middle.project.labels[2].id == base.labels[2].id, "labels follow the later arrangement with their original identities");
    bool tempoOwners = true;
    for (const auto& point : base.tempo.points) if (point.clipId.isNotEmpty())
        tempoOwners &= std::any_of(middle.project.tempo.points.begin(), middle.project.tempo.points.end(), [&](const auto& now)
            { return now.clipId == point.clipId && now.time == point.time + (point.time >= middle.suffixStart ? middle.displacement : 0); });
    check(tempoOwners, "existing tempo-marker owners retain their identities and translated positions");
    const auto parsed = parseProject(json(middle.project));
    check(parsed.error.isEmpty(), "planned insertion already satisfies strict saved clocks/automation/frame admission: " + parsed.error);
    auto first = planArrangementInsertion(base, extra, aId, false, 16, TransitionPreset::crossfade);
    check(first.error.isEmpty() && first.moved == 3 && first.project.clipFor(first.inserted)->start == 0
        && first.project.transitionFor(aId)->outgoing == first.inserted, "prepend a new first song while the entire established mix moves together: " + first.error);
    if (first.error.isEmpty())
    {
        const auto* n = first.project.clipFor(first.inserted);
        check(std::abs(first.project.tempo.beatsAt(n->start + n->firstDownbeatTime())) < 1.e-7,
            "new first downbeat owns the visible origin while existing suffix keeps its bar phase");
        double grid = 0;
        for (const auto& c : base.clips)
            grid = std::max(grid, std::abs(std::remainder(first.project.tempo.beatsAt(c.start + first.displacement) - base.tempo.beatsAt(c.start), 4)));
        check(grid < 1.e-7 && parseProject(json(first.project)).error.isEmpty(), "prepend preserves all existing bar phases and saved origin/preroll clocks");
    }
    auto appended = planArrangementInsertion(base, extra, cId, true, 16, TransitionPreset::crossfade);
    check(appended.error.isEmpty() && appended.moved == 0 && appended.project.clips[0].start == 0,
        "explicit after-last insertion shares the preview owner and preserves established positions: " + appended.error);
    auto lateAnchor = extra->catalogueCopy(); lateAnchor->samples.makeCopyOf(extra->samples); lateAnchor->manualBeatOrigin = -1;
    lateAnchor->analysis.bpm = 128; lateAnchor->analysis.beatOrigin = .173; lateAnchor->analysis.downbeatOrigin = .173 + 26 * 240.0 / 128;
    const auto lateFirst = planArrangementInsertion(base, lateAnchor, aId, false, 16, TransitionPreset::crossfade);
    const auto* lateClip = lateFirst.project.clipFor(lateFirst.inserted);
    check(lateFirst.error.isEmpty() && lateClip && std::abs(lateFirst.project.tempo.beatsAt(lateClip->start + lateClip->firstDownbeatTime())) < 1.e-7,
        "a later reliable source downbeat extends backwards to the new first project bar instead of creating artificial long preroll: " + lateFirst.error);
    auto zeroOrigin = base; zeroOrigin.tempo.origin = 0;
    auto zeroMedia = source->catalogueCopy(); zeroMedia->samples.makeCopyOf(source->samples); zeroMedia->manualBeatOrigin = 0; zeroMedia->analysis.downbeatOrigin = 0;
    for (auto& c : zeroOrigin.clips) { c.media = zeroMedia; c.audioSlip = 0; }
    const auto noOverlap = planArrangementInsertion(zeroOrigin, extra, aId, false, 0, TransitionPreset::alignOnly);
    check(noOverlap.error.isEmpty() && parseProject(json(noOverlap.project)).error.isEmpty(), "zero-length alignment-only prepend preserves strict session curves at session zero: " + noOverlap.error);
    auto hard = base; auto& hardOut = hard.clips[0]; auto& hardIn = hard.clips[1];
    hardOut.outroAnchor = hardOut.sourceElapsed(hardOut.length); hardOut.fadeOutAnchor = hardOut.outroAnchor; hardOut.fadeOut = 0;
    hardIn.introAnchor = hardIn.sourceElapsed(hardOut.end() - hardIn.start); hardIn.fadeInAnchor = hardIn.introAnchor; hardIn.fadeIn = 0;
    const auto fromHardCut = planArrangementInsertion(hard, extra, bId, false, 16, TransitionPreset::crossfade);
    check(fromHardCut.error.isEmpty() && fromHardCut.project.transitionRange(*fromHardCut.project.transitionFor(fromHardCut.inserted)).getLength() > 0
        && std::abs(fromHardCut.project.clipFor(bId)->transitionTime(true) - hardIn.transitionTime(true)) < 1.e-7,
        "insertion can replace an existing zero-length hard-cut join and retains the old incoming source entry: " + fromHardCut.error);
    for (int failure = 0; failure < 5; ++failure)
    {
        auto rejected = base; auto incoming = extra; juce::String target = bId;
        if (failure == 0) rejected.transitionLinks[1].locked = true;
        if (failure == 1) target = "removed-segment";
        if (failure == 2) rejected.clips[2].pendingPlacement = PendingPlacement::move;
        if (failure == 3) rejected.clips[0].elasticPlayback = true;
        if (failure == 4) { incoming = extra->catalogueCopy(); incoming->samples.setSize(1, 8000); incoming->manualBeatOrigin = 0; }
        const auto before = json(rejected); const auto denied = planArrangementInsertion(rejected, incoming, target, false, 16, TransitionPreset::crossfade);
        check(denied.error.isNotEmpty() && json(rejected) == before && json(denied.project) == before,
            "insertion rejects protected/stale/preparing/elastic/short-source cases atomically " + juce::String(failure));
    }
    const auto prepareNew = prepareArrangementInsertion(middle);
    check(prepareNew.isEmpty() && middle.project.clipFor(middle.inserted)->prepared, "prepare only the new mapped voice: " + prepareNew);
    if (prepareNew.isNotEmpty()) return;
    auto dry = std::make_unique<Engine>(juce::File(), false), wet = std::make_unique<Engine>(juce::File(), false);
    dry->project = base; wet->project = middle.project; dry->sampleRate = wet->sampleRate = 8000;
    juce::AudioBuffer<float> oldPcm(2, 256), newPcm(2, 256); juce::MidiBuffer midi;
    for (double at : {12.0, 30.0, 65.0, 86.0, 110.0})
    {
        dry->resetPlugins(); wet->resetPlugins(); dry->render(oldPcm, 256, at, midi);
        wet->render(newPcm, 256, at < middle.suffixStart ? at : at + middle.displacement, midi); double error = 0;
        for (int ch = 0; ch < 2; ++ch) for (int n = 0; n < 256; ++n) error = std::max(error, (double) std::abs(oldPcm.getSample(ch, n) - newPcm.getSample(ch, n)));
        check(error < 2.e-6 && oldPcm.getRMSLevel(0, 0, 256) > .001, "production mix PCM outside replaced joins survives insertion at " + juce::String(at) + ": " + juce::String(error, 8));
    }
    auto owned = std::make_unique<Workstation>(false); auto& ui = *owned; ui.stopTimer(); ui.setSize(1920, 1080);
    ui.engine.project = base; ui.engine.library = {source, extra}; ui.selectedClip = bId; ui.selectedLibraryId = extra->id;
    ui.selectTrack(1); ui.showBrowserPage(1); ui.refresh(); ui.addToDesktop(0); ui.setVisible(true);
    auto pump = []
    {
        const auto until = juce::Time::getMillisecondCounterHiRes() + 40;
        do {
#if JUCE_WINDOWS
            MSG m; while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageW(&m); }
#endif
            juce::Thread::sleep(1);
        } while (juce::Time::getMillisecondCounterHiRes() < until);
    };
    auto modal = [] { return dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent()); };
    ui.playlistTable.sortOrderChanged(1, false);
    const auto menu = ui.playlistTable.beatmatchMenu(0); bool menuHasInsertion = false;
    for (juce::PopupMenu::MenuItemIterator it(menu, true); it.next();) if (it.getItem().itemID == (int) TimelineAction::insertBefore) menuHasInsertion = it.getItem().isEnabled;
    check(menuHasInsertion, "sorted Playlist exposes arrangement insertion on an actual row");
    ui.menuItemSelected(94, 3); pump(); check(!modal(), "insertion menu command rejects an unrelated top-level owner");
    ui.menuItemSelected(94, 1); pump(); auto* picker = modal();
    check(picker && picker->getName() == "Insert song before" && picker->getComboBoxComponent("source")->getSelectedItemIndex() == 1,
        "Edit/Playlist insertion opens the real Library source chooser bound to the selected segment ID");
    if (picker) { picker->exitModalState(0); pump(); }
    check(json(ui.engine.project) == original && ui.undoHistory.empty(), "cancel source selection leaves completed mix/history intact");
    auto waitForPreparation = [&]
    {
        const auto until = juce::Time::getMillisecondCounterHiRes() + 120000;
        while (ui.busy && juce::Time::getMillisecondCounterHiRes() < until) pump();
        pump();
    };
    auto catalogue = extra->catalogueCopy(); ui.engine.library[1] = catalogue;
    ui.chooseArrangementInsertion(bId, false); pump(); picker = modal();
    if (picker) { picker->getComboBoxComponent("source")->setSelectedItemIndex(1); picker->exitModalState(1); pump(); }
    waitForPreparation(); auto* hydratedPreview = modal();
    check(hydratedPreview && hydratedPreview->getName() == "Preview song insertion" && !catalogue->hasPlayback()
        && json(ui.engine.project) == original && ui.undoHistory.empty(), "real Library picker asynchronously hydrates a detached source and prepares preview without changing current mix/catalogue");
    if (hydratedPreview) { hydratedPreview->exitModalState(0); pump(); }
    auto missing = catalogue->catalogueCopy(); missing->file = report.getSiblingFile("missing-insertion-source.wav");
    ui.prepareChosenInsertion(missing, bId, false); waitForPreparation();
    check(!ui.busy && !modal() && json(ui.engine.project) == original && ui.undoHistory.empty()
        && ui.status.contains("Insertion unavailable"), "failed asynchronous source load preserves the complete mix/history and reports the failure");
    ui.prepareChosenInsertion(catalogue, bId, false); if (ui.importProgress.cancel.onClick) ui.importProgress.cancel.onClick(); waitForPreparation();
    check(!ui.busy && !modal() && json(ui.engine.project) == original && ui.undoHistory.empty()
        && ui.status.contains("cancelled"), "actual progress Cancel prevents insertion commit even if source preparation finishes");
    ui.previewArrangementInsertion(middle, original, extra); pump(); auto* preview = modal();
    if (preview)
    {
        auto stream = report.getSiblingFile("insertion-preview.png").createOutputStream(); juce::PNGImageFormat png;
        check(stream && png.writeImageToStream(preview->createComponentSnapshot(preview->getLocalBounds()), *stream), "capture actual insertion order/timing/source-tail preview");
        preview->exitModalState(0); pump();
    }
    check(preview && json(ui.engine.project) == original && ui.undoHistory.empty(), "cancel prepared preview preserves original mix/history");
    const auto heldSource = report.getSiblingFile("insertion-new-held.wav");
    const bool movedSource = extra->file.moveFileTo(heldSource); bool missingGuard = false;
    if (movedSource)
    {
        ui.previewArrangementInsertion(middle, original, extra); pump(); auto* changedPreview = modal();
        if (changedPreview) { changedPreview->exitModalState(1); pump(); }
        missingGuard = changedPreview && json(ui.engine.project) == original && ui.undoHistory.empty();
    }
    const bool restoredSource = movedSource && heldSource.moveFileTo(extra->file);
    check(missingGuard && restoredSource, "a new source disappearing after preview rejects Apply before history/mix mutation; owned fixture restored");
    ui.previewArrangementInsertion(middle, original, extra); pump(); preview = modal();
    if (preview) { preview->exitModalState(1); pump(); }
    check(preview && json(ui.engine.project) == json(middle.project) && ui.undoHistory.size() == 1 && ui.selectedClip == middle.inserted,
        "real preview Apply commits the canonical insertion as one Undo and selects the new segment");
    check(ui.playlistTable.clipAtRow(0)->id == aId && ui.playlistTable.clipAtRow(1)->id == middle.inserted
        && ui.playlistTable.clipAtRow(2)->id == bId && ui.playlistTable.table.getHeader().getSortColumnId() == 3,
        "Apply shows the actual A-new-B timeline order even when Playlist previously used a title sort");
    ui.undo(false); check(json(ui.engine.project) == original, "Undo restores complete original mix including tempo/session curves/labels/pairs/source annotations");
    ui.undo(true); check(json(ui.engine.project) == json(middle.project), "Redo restores identical planned insertion");
    const auto current = json(ui.engine.project); ui.engine.playing = true;
    check(!ui.canInsertArrangement() && !ui.commitArrangementInsertion(first, current, extra) && json(ui.engine.project) == current,
        "live playing cannot admit an arrangement insertion"); ui.engine.playing = false;
    check(!ui.commitArrangementInsertion(first, original, extra) && json(ui.engine.project) == current, "stale preview cannot overwrite edits made after its baseline");
    const auto session = report.getSiblingFile("inserted-mix.mixworks"); auto reopened = std::make_unique<Engine>(juce::File(), false);
    const auto saved = ui.engine.saveProject(session), opened = reopened->restoreProject(session.loadFileAsString());
    check(saved.isEmpty() && opened.isEmpty() && reopened->project.clips.size() == 4 && reopened->project.transitionLinks == ui.engine.project.transitionLinks,
        "real Save/Open retains inserted track/order/links and source-owned annotations: " + saved + opened);
    const auto exported = report.getSiblingFile("inserted-export.wav"); const auto exportError = reopened->exportWav(exported);
    check(exportError.isEmpty() && exported.getSize() > 1000, "export reopened inserted arrangement through the production mixer: " + exportError);
    for (const int width : {1250, 1920})
    {
        ui.setSize(width, 1080); ui.menuItemSelected(22, 4); ui.refresh();
        auto stream = report.getSiblingFile("inserted-playlist-" + juce::String(width) + ".png").createOutputStream(); juce::PNGImageFormat png;
        check(stream && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *stream), "capture inserted Playlist/timeline at width " + juce::String(width));
    }
}
}
