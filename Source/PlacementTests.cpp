#include "Workstation.h"
#include <cmath>

namespace mixworks
{
void runPlacementTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    {
        auto media = std::make_shared<Media>(); media->sampleRate = 8000; media->samples.setSize(2, 64000); media->samples.clear();
        media->manualBpm = 120; media->manualBeatOrigin = .25;
        for (const double start : {3.75, std::nextafter(3.75, 4.), std::nextafter(3.75, 3.)})
        {
            Clip out; out.media = media; out.length = 4; out.fadeOut = 1;
            Clip in; in.media = media; in.start = start; in.length = 4; in.fadeIn = .4;
            const auto cue = in.firstDownbeatTime();
            initialiseTransition(out, in, out.end() - in.start, TransitionPreset::alignOnly);
            check(in.introAnchor && std::abs(*in.introAnchor - .25) < 1e-9
                  && std::abs(out.start + out.transitionTime(false) - 4) < 1e-9
                  && std::abs(in.start + in.transitionTime(true) - in.start - cue) < 1e-9,
                  "alignment-only retains a source cue at the outgoing endpoint, including adjacent floating representations");
            check(out.start == 0 && out.length == 4 && out.fadeOut == 1 && in.start == start && in.length == 4 && in.fadeIn == .4,
                  "endpoint alignment leaves audio placement, source spans and manual fades unchanged");
        }
    }
    const auto folder = report.getParentDirectory().getChildFile("placement-fixture-" + juce::Uuid().toString()); folder.createDirectory();
    const auto aFile = folder.getChildFile("First.wav"), bFile = folder.getChildFile("Second.wav"), cFile = folder.getChildFile("External.wav"), dFile = folder.getChildFile("Cancelled.wav");
    bool written = false;
    {
        juce::AudioBuffer<float> pcm(2, 8000 * 8); for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < pcm.getNumSamples(); ++i) pcm.setSample(ch, i, 0.2f);
        juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream = aFile.createOutputStream();
        auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(2).withBitsPerSample(24));
        written = writer && writer->writeFromAudioSampleBuffer(pcm, 0, pcm.getNumSamples());
    }
    check(written && aFile.copyFileTo(bFile) && aFile.copyFileTo(cFile) && aFile.copyFileTo(dFile), "manual placement uses owned real audio files");
    {
        juce::AudioBuffer<float> pcm(2, 8000 * 3); pcm.clear(); cFile.deleteFile();
        juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream = cFile.createOutputStream();
        auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(2).withBitsPerSample(24));
        check(writer && writer->writeFromAudioSampleBuffer(pcm, 0, pcm.getNumSamples()), "external batch includes a different duration to exercise pending placement correction");
    }
    Workstation ui(false); ui.stopTimer(); ui.setSize(1250, 800);
    check(ui.selectedTransitionPreset() == TransitionPreset::crossfade && ui.transitionIndex == 5 && ui.transitionPreset.isVisible()
        && ui.transitionPreset.getNumItems() == 3, "native new-track defaults are Crossfade and section-driven 8-64 bars");
    // Retain legacy overlap/loading coverage with that preset explicitly chosen.
    ui.transitionPreset.setSelectedId(2, juce::dontSendNotification);
    ui.transitionIndex = 2; ui.transitionLength.setSelectedId(3, juce::dontSendNotification);
    auto a = readMedia(ui.engine.formats, aFile), b = readMedia(ui.engine.formats, bFile, {}, AnalysisCacheMode::readWrite, MediaReadPurpose::catalogue);
    check(a.media && b.media && a.media->hasPlayback() && !b.media->hasPlayback(), "fixture covers loaded audio and lazy catalogue hydration"); if (!a.media || !b.media) return;
    a.media->manualBpm = 120; a.media->manualBeatOrigin = 0; b.media->manualBpm = 135; b.media->manualBeatOrigin = 0.25; b.media->manualKey = 17;
    ui.engine.library = {a.media, b.media}; ui.engine.project.bpm = 130; ui.engine.project.tempo.setPoint(0, 130); ui.engine.project.tempo.setPoint(50, 140);
    Clip existing; existing.media = a.media; existing.start = 80; existing.length = 8; existing.fadeOut = 1; existing.gainDb = -2;
    ui.engine.project.clips = {existing}; ui.refresh(); ui.selectAutomationTarget(0);
    ui.timeline.pixelsPerSecond = 40; ui.timeline.updateSize(); ui.viewport.setViewPosition(1200, 16);
    const auto original = juce::JSON::toString(projectToJson(ui.engine.project));
    auto originalKept = [&]
    {
        const auto& c = ui.engine.project.clips.front(); return c.id == existing.id && c.start == existing.start && c.length == existing.length && c.gainDb == existing.gainDb;
    };
    check(ui.transitionIndex == 2 && ui.getMenuBarNames().contains("Beatmatch"), "explicit 8-bar insertion choice and rebuild remain available through the Beatmatch menu");
    ui.showBrowserPage(1); check(ui.getMenuBarNames().contains("Beatmatch"), "Beatmatch menu remains available on Playlist"); ui.showBrowserPage(0);
    juce::SparseSet<int> rows;
    for (int i = 0; i < (int) ui.visibleMedia.size(); ++i) if (ui.engine.library[(size_t) ui.visibleMedia[(size_t) i]]->id == b.media->id) rows.addRange({i, i + 1});
    const auto description = ui.libraryTable.getDragSourceDescription(rows);
    check(description.isString() && description.toString() == b.media->id && ui.playlistTable.getDragSourceDescription(rows).isVoid(), "library drag carries stable source identity; playlist rows cannot create accidental duplicates");
    ui.search.setText("First", false); ui.filterLibrary();
    auto& timeline = ui.timeline;
    const juce::Point<int> drop {1517, ui.timeline.laneTop(2) + 40}; const double expectedTime = 80;
    juce::DragAndDropTarget::SourceDetails details(description, &ui.libraryTable.table, drop);
    check(ui.viewport.getViewPositionX() > 0 && ui.viewport.getViewPositionY() == 0 && timeline.isInterestedInDragSource(details)
          && !timeline.isInterestedInDragSource({description, &ui.playlistTable.table, drop})
          && !timeline.isInterestedInDragSource({"missing-id", &ui.libraryTable.table, drop}), "filtered rows remain draggable by identity while foreign and stale payloads are rejected");
    timeline.itemDropped({description, &ui.libraryTable.table, {1517, -10}});
    check(ui.engine.project.clips.size() == 1 && !ui.busy, "ruler drop does not append or rearrange clips");
    timeline.itemDragEnter(details);
    check(timeline.dropPreview && timeline.dropPreview->track == 2 && timeline.dropPreview->time == expectedTime,
          "drop preview resolves the chosen fully visible lane and default overlap after horizontal scrolling");
    {
        const auto snapshot = ui.createComponentSnapshot(ui.getLocalBounds()); auto stream = report.getSiblingFile("drop-preview.png").createOutputStream(); juce::PNGImageFormat png;
        check(stream && png.writeImageToStream(snapshot, *stream), "capture real native drop feedback with a filtered library and scrolled timeline");
    }
    timeline.itemDropped(details);
    check(!ui.busy && !timeline.dropPreview && ui.engine.project.clips.size() == 2 && !ui.engine.project.clips.back().media->hasPlayback(), "a catalogue drop displays its clip immediately without waiting for playback loading: busy=" + juce::String((int) ui.busy) + " clips=" + juce::String((int) ui.engine.project.clips.size()) + " loaded=" + juce::String((int) ui.engine.project.clips.back().media->hasPlayback()));
    {
        const auto snapshot = ui.createComponentSnapshot(ui.getLocalBounds()); auto stream = report.getSiblingFile("pending-audio.png").createOutputStream(); juce::PNGImageFormat png;
        check(stream && png.writeImageToStream(snapshot, *stream), "capture the native preparing-audio clip before any worker completion");
    }
    ui.engine.setPlaying(true);
    const auto pendingExport = folder.getChildFile("pending.wav");
    check(!ui.engine.playing && ui.engine.exportWav(pendingExport).isNotEmpty() && !pendingExport.existsAsFile()
          && ui.engine.saveProject(folder.getChildFile("pending.mixworks")).isNotEmpty() && !folder.getChildFile("pending.mixworks").existsAsFile(), "pending audio cannot silently play or produce an incomplete export/save");
    ui.undo(false); check(ui.engine.project.clips.size() == 1, "Undo can remove a clip before background loading finishes: clips=" + juce::String((int) ui.engine.project.clips.size()));
    ui.undo(true); check(ui.engine.project.clips.size() == 2, "Redo restores a pending clip without waiting for audio");
    ui.viewport.setViewPosition(0, 0); ui.search.setText({}, false); ui.libraryTable.sortOrderChanged(1, false);
    int phase = 0; const double started = juce::Time::getMillisecondCounterHiRes(); auto cancelledLoadFinished = std::make_shared<std::atomic<bool>>(false);
    bool liveContinuous = true;
    struct Poll final : juce::Timer { std::function<void()> tick; void timerCallback() override { tick(); } } poll;
    poll.tick = [&]
    {
        if (phase >= 11)
        {
            const double before = ui.engine.position;
            juce::AudioBuffer<float> output(2, 256); auto* channels = output.getArrayOfWritePointers();
            ui.engine.audioDeviceIOCallbackWithContext(nullptr, 0, channels, 2, 256, {});
            liveContinuous &= ui.engine.playing && ui.engine.position > before && output.getMagnitude(0, 256) > 0.01f && !ui.busy && ui.playButton.isEnabled();
        }
        if (juce::Time::getMillisecondCounterHiRes() - started > 60000)
        { check(false, "placement worker completion timeout at phase " + juce::String(phase)); poll.stopTimer(); juce::MessageManager::getInstance()->stopDispatchLoop(); return; }
        if (ui.busy || !ui.timelineLoads.empty()) return;
        if (phase == 0)
        {
            const auto* placed = ui.currentClip();
            check(placed && placed->media->id == b.media->id && placed->media->hasPlayback() && placed->media->manualBpm == 135 && placed->media->manualBeatOrigin == 0.25 && placed->media->manualKey == 17
                  && placed->track == 2 && std::abs(placed->start - expectedTime) < 1 && std::abs(placed->length * placed->rate - 8) < 1.0e-7 && placed->beatmatched && placed->tempoMap && (placed->prepared || std::abs(placed->rate - 1) < 1.0e-7) && placed->fadeIn == 0,
                  "lazy drop retains source content and lane, and applies default placement after Undo/Redo during loading: " + (placed ? "lane=" + juce::String(placed->track) + " start=" + juce::String(placed->start) + " len=" + juce::String(placed->length * placed->rate) : juce::String("missing")));
            const double placedStart = placed ? placed->start : -1;
            check(placed && placed->firstDownbeatTime() >= 0
                  && std::abs(std::remainder(ui.engine.project.tempo.beatsAt(placed->start + placed->firstDownbeatTime()), 4)) < 1.0e-7,
                  "library drag and background hydration automatically align the source bar to the timeline bar");
            check(originalKept() && ui.undoHistory.size() == 1 && ui.engine.project.tempo.bpmAt(0) == 130,
                  "automatic append retains starting tempo, established geometry and one-step undo");
            juce::AudioBuffer<float> output(2, 512); juce::MidiBuffer midi; ui.engine.render(output, 512, placedStart + 0.5, midi);
            check(output.getMagnitude(0, 512) > 0.1f, "manually placed hydrated audio reaches the real mixer");
            const auto saved = folder.getChildFile("manual.mixworks"); Engine reopened;
            const auto saveError = ui.engine.saveProject(saved);
            const auto reopenError = saveError.isEmpty() ? reopened.restoreProject(saved.loadFileAsString()) : saveError;
            check(reopenError.isEmpty() && reopened.project.clips.size() == 2
                  && std::abs(reopened.project.clips.back().start - placedStart) < 1.0e-7 && reopened.project.clips.back().track == 2 && reopened.project.clips.back().beatmatched,
                  "save and reopen retain matched timing, chosen lane and shared tempo map: " + reopenError);
            ui.undo(false); check(ui.engine.project.clips.size() == 1 && juce::JSON::toString(projectToJson(ui.engine.project)) == original, "Undo removes the dropped clip and restores the original tempo map");
            ui.undo(true); check(ui.engine.project.clips.size() == 2 && ui.engine.project.clips.back().start == placedStart, "Redo restores the same matched drop position");
            ui.viewport.setViewPosition(3000, 0);
            timeline.itemDropped({a.media->id, &ui.libraryTable.table, {3287, ui.timeline.laneTop(0) + 40}});
            check(ui.busy && ui.engine.project.clips.size() == 3 && ui.engine.project.clips.back().track == 0 && !ui.engine.playbackReady(),
                  "already loaded dropped audio waits for tempo preparation before playback");
            phase = 10;
        }
        else if (phase == 10)
        {
            
            const auto& placed = ui.engine.project.clips.back();
            check(placed.beatmatched && placed.start >= 0 && placed.track == 0 && placed.fadeIn == 0 && placed.controls.automation[1].active() && originalKept(),
                  "an occupied-lane drop uses the selected bass swap without volume fades: " + ui.status);
            ui.viewport.setViewPosition(0, 0);
            const auto point = ui.getLocalPoint(&timeline, juce::Point<int>(237, ui.timeline.laneTop(3) + 40));
            ui.fileDragEnter({cFile.getFullPathName(), aFile.getFullPathName()}, point.x, point.y);
            check(timeline.dropPreview && timeline.dropPreview->track == 3 && timeline.dropPreview->time >= 0, "Explorer file drag shares the default-placement preview");
            ui.filesDropped({cFile.getFullPathName(), aFile.getFullPathName()}, point.x, point.y);
            timeline.pixelsPerSecond = 80; timeline.updateSize(); ui.viewport.setViewPosition(2000, 0); phase = 1;
        }
        else if (phase == 1)
        {
            const auto& clips = ui.engine.project.clips;
            check(clips.size() == 5 && clips[3].length == 3 && clips[3].media->file == cFile && clips[4].media->id == a.media->id && clips[3].track == 3 && clips[4].track == 3
                  && clips[3].start >= 0 && clips[4].start <= clips[3].end() && clips[4].beatmatched
                  && ui.engine.library.size() == 3 && originalKept(), "batch file drop preserves an unknown-tempo source, matches the known source on the chosen lane, and reuses library audio: " + ui.status);
            ui.undo(false); check(ui.engine.project.clips.size() == 3 && ui.engine.library.size() == 3, "one Undo removes the entire file-drop batch while retaining imported library sources"); ui.undo(true);
            ui.viewport.setViewPosition(0, 0); const auto point = ui.getLocalPoint(&timeline, juce::Point<int>(300, ui.timeline.laneTop(1) + 40));
            ui.filesDropped({dFile.getFullPathName()}, point.x, point.y);
            check(!ui.busy && ui.engine.project.clips.size() == 6, "an external file also appears before metadata and decoding finish");
            ui.undo(false); phase = 2;
        }
        else if (phase == 2)
        {
            check(ui.engine.project.clips.size() == 5 && ui.engine.library.size() == 4, "background completion does not resurrect an undone external drop");
            const auto point = ui.getLocalPoint(&timeline, juce::Point<int>(300, ui.timeline.laneTop(1) + 40));
            ui.filesDropped({dFile.getFullPathName(), folder.getChildFile("missing.wav").getFullPathName()}, point.x, point.y); phase = 3;
        }
        else if (phase == 3)
        {
            check(ui.engine.project.clips.size() == 7 && !ui.engine.project.clips.back().media->playbackError.isEmpty() && !ui.engine.playbackReady()
                  && originalKept(), "a missing file remains visibly unavailable without replacing existing audio or allowing incomplete playback");
            ui.undo(false); check(ui.engine.project.clips.size() == 5 && ui.engine.playbackReady(), "one Undo removes an unavailable file-drop batch");
            ui.newSession(); ui.addMedia(0); phase = 4;
        }
        else if (phase == 4)
        {
            check(ui.engine.project.clips.size() == 1, "double-click append still loads and prepares audio through the shared source loader");
            ui.addMedia(0); phase = 5;
        }
        else if (phase == 5)
        {
            const auto& clips = ui.engine.project.clips;
            check(clips.size() == 2 && clips[1].start < clips[0].end() && clips[0].fadeOut == 0 && clips[1].fadeIn == 0 && clips[0].controls.automation[1].active() && clips[1].controls.automation[1].active(),
                  "default append creates an overlapping transition at the first track's tempo");
            const auto before = juce::JSON::toString(projectToJson(ui.engine.project)); ui.menuItemSelected(41, 3);
            check(ui.transitionIndex == 1 && juce::JSON::toString(projectToJson(ui.engine.project)) == before,
                  "Beatmatch menu sets transition length without silently rearranging the mix");
            ui.menuItemSelected(13, 3); phase = 6;
        }
        else if (phase == 6)
        {
            const auto& clips = ui.engine.project.clips;
            check(clips.size() == 2 && clips[1].start < clips[0].end() && clips[0].fadeOut == 0 && clips[1].fadeIn == 0 && clips[0].controls.automation[1].active() && clips[1].controls.automation[1].active(),
                  "Beatmatch menu explicitly rebuilds the requested overlapping transition");
            ui.menuItemSelected(40, 3); ui.menuItemSelected(13, 3); phase = 7;
        }
        else if (phase == 7)
        {
            const auto& clips = ui.engine.project.clips;
            check(ui.transitionIndex == 0 && clips.size() == 2 && clips[1].start >= clips[0].end() - 0.00001 && clips[0].fadeOut == 0 && clips[1].fadeIn == 0,
                  "top-level Beatmatch menu rebuilds Cut transitions through the menu");
            ui.newSession(); auto stale = b.media->catalogueCopy(); stale->file = dFile;
            Clip protectedClip; protectedClip.media = a.media; protectedClip.length = 8; protectedClip.start = 30;
            ui.engine.project.clips = {protectedClip};
            // A new insertion selects the current file; an old catalogue fingerprint is not an existing arrangement.
            cFile.copyFileTo(dFile); ui.engine.library = {stale, a.media}; ui.refresh(); ui.addMedia(0, TimelinePlacement {1, 12.125}); phase = 8;
        }
        else if (phase == 8)
        {
            check(ui.engine.project.clips.size() == 2 && ui.engine.project.clips.back().media->hasPlayback()
                  && ui.engine.project.clips.back().media->playbackError.isEmpty() && ui.engine.project.clips.back().length == 3
                  && ui.engine.project.clips.back().media->manualBpm == 0 && ui.engine.project.clips.back().media->manualKey == -1,
                  "a fresh drop refreshes changed catalogue audio, duration and corrections instead of declaring it unavailable");
            check(ui.engine.project.clips.front().media == a.media && ui.engine.project.clips.front().start == 30
                  && ui.engine.project.clips.front().length == 8 && b.media->duration() == 8 && b.media->manualBpm == 135,
                  "fresh-source adoption never rewrites established clip audio, geometry or the previous snapshot");
            ui.newSession(); ui.engine.library = {a.media->catalogueCopy()}; ui.addMedia(0, TimelinePlacement {0, 1}); ui.newSession();
            ui.audioWorkers.addJob([cancelledLoadFinished] { juce::MessageManager::callAsync([cancelledLoadFinished] { *cancelledLoadFinished = true; }); }); phase = 9;
        }
        else if (phase == 9)
        {
            if (!*cancelledLoadFinished) return;
            check(ui.engine.project.clips.empty() && ui.timelineLoads.empty() && !ui.engine.library[0]->hasPlayback(), "a late audio completion cannot repopulate a new mix or its released playback mappings");
            ui.newSession();
            auto active = existing; active.start = 0; active.length = 8; active.fadeOut = 0;
            ui.engine.project.clips = {active}; ui.engine.project.bpm = 120; ui.engine.project.tempo.setPoint(0, 120);
            ui.engine.library = {a.media, b.media->catalogueCopy()}; ui.engine.position = 1; ui.engine.setPlaying(true); ui.refresh();
            ui.addMedia(1, TimelinePlacement {1, 4});
            check(ui.engine.playing && !ui.busy && ui.playButton.isEnabled() && !ui.timelineLoads.empty(), "lazy live insertion keeps transport, mixer and Pause enabled while decoding in the background");
            phase = 11;
        }
        else if (phase == 11)
        {
            check(liveContinuous && ui.engine.project.clips.back().beatmatched && ui.engine.project.clips.back().elasticSource
                  && !ui.engine.project.hasPendingPlacement() && ui.engine.project.tempo.bpmAt(0) == 120,
                  "real callback audio and playhead continue through lazy decoding and the live matching commit");
            const auto point = ui.getLocalPoint(&timeline, juce::Point<int>(10, ui.timeline.laneTop(2) + 40));
            ui.filesDropped({folder.getChildFile("missing-live.wav").getFullPathName()}, point.x, point.y);
            phase = 12;
        }
        else if (phase == 12)
        {
            check(liveContinuous && ui.engine.playing && !ui.engine.project.clips.back().media->playbackError.isEmpty(),
                  "an incoming decode failure leaves existing live audio and transport running");
            ui.addMedia(0, TimelinePlacement {3, 6}); phase = 13;
        }
        else if (phase == 13)
        {
            check(liveContinuous && !ui.engine.project.hasPendingPlacement() && ui.engine.project.clips.back().beatmatched
                  && ui.engine.project.clips.back().elasticSource,
                  "a failed incoming file does not block the next live insertion or stop established audio");
            ui.newSession(); ui.engine.library = {a.media}; auto active = existing; active.start = 0; active.outroAnchor = 5; active.fadeOut = 0;
            ui.engine.project.clips = {active}; ui.engine.project.bpm = 120; ui.engine.project.tempo.setPoint(0, 120);
            ui.transitionPreset.setSelectedId(1, juce::dontSendNotification);
            ui.addMedia(0, TimelinePlacement {1, 3});
            ui.transitionPreset.setSelectedId(3, juce::dontSendNotification); // A later UI choice must not change an in-flight insertion.
            phase = 14;
        }
        else
        {
            const auto& out = ui.engine.project.clips.front(); const auto& in = ui.engine.project.clips.back();
            check(ui.engine.project.clips.size() == 2 && out.outroAnchor == std::optional<double>(5)
                && std::abs(out.start + out.transitionTime(false) - in.start - in.transitionTime(true)) < 1e-7,
                "native alignment-only drop pairs new IN with the user's existing OUT");
            check(in.fadeIn == 0 && in.fadeOut == 0 && out.fadeOut == 0 && !in.controls.automation[1].active() && !out.controls.automation[1].active(),
                "native drop creates no default transition and retains the preset captured before background preparation");
            poll.stopTimer(); juce::MessageManager::getInstance()->stopDispatchLoop();
        }
    };
    poll.startTimer(20); juce::MessageManager::getInstance()->runDispatchLoop();
}
}
