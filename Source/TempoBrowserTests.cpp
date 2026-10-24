#include "Workstation.h"
#include "Beatmatch.h"
#include <cmath>
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace mixworks
{
void runTempoBrowserTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    auto closeEnough = [](double a, double b) { return std::abs(a - b) < 1.0e-6; };
    std::vector<std::shared_ptr<Media>> media;
    Project input;
    for (int n = 0; n < 3; ++n)
    {
        auto m = std::make_shared<Media>(); m->sampleRate = 8000; m->manualBpm = 120 + n * 8; m->manualBeatOrigin = 0;
        m->name = n == 0 ? "Z from red" : n == 1 ? "Blue" : "A from red"; m->file = report.getSiblingFile("tempo-browser-" + juce::String(n) + ".wav");
        m->samples.setSize(1, 8000 * 48); m->samples.clear();
        {
            juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream = m->file.createOutputStream();
            auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(1).withBitsPerSample(24));
            check(writer && writer->writeFromAudioSampleBuffer(m->samples, 0, m->frames()), "write owned tempo/browser source " + juce::String(n));
        }
        m->assets.cover = juce::Image(juce::Image::RGB, 64, 64, false);
        { juce::Graphics g(m->assets.cover); g.fillAll(n == 1 ? juce::Colours::blue : juce::Colours::red); }
        m->assets.coverSource = m->file; m->assets.audioThumbnail = true; m->assets.prepareArtworkGrouping();
        media.push_back(m);
        Clip c; c.media = m; c.length = 48; c.track = n; c.pendingPlacement = PendingPlacement::append; input.clips.push_back(c);
    }
    auto single = input; single.clips.resize(1); auto first = prepareTempoMap(single, false, 32);
    check(first.error.isEmpty() && first.project.tempo.points.size() == 1 && first.project.tempo.points[0].time == 0
          && first.project.tempo.points[0].bpm == 120 && first.project.tempo.points[0].clipId == input.clips[0].id, "one track creates only its native-tempo marker at zero");
    auto ready = prepareTempoMap(input, false, 32);
    check(ready.error.isEmpty(), "prepare three owned tempo targets: " + ready.error); if (ready.error.isNotEmpty()) return;
    bool targets = ready.project.tempo.points.size() == 3;
    for (const auto& c : ready.project.clips)
    {
        const auto p = std::find_if(ready.project.tempo.points.begin(), ready.project.tempo.points.end(), [&](const auto& t) { return t.clipId == c.id; });
        targets &= p != ready.project.tempo.points.end() && closeEnough(p->time, c.start + c.transitionTime(true)) && closeEnough(p->bpm, c.media->bpm());
    }
    check(targets, "three source additions create exactly three adjustable native targets at their IN handles");
    {
        auto seed = input; seed.clips.resize(2);
        auto chain = prepareTempoMap(seed, false, 32);
        for (int source : {2, 0})
        {
            const auto before = chain.project;
            auto incoming = input.clips[(size_t) source]; incoming.id = juce::Uuid().toString(); incoming.effects = makeClipEffects();
            chain.project.clips.push_back(incoming);
            chain = prepareTempoMap(chain.project, false, 32, 0, 0, 2.0);
            const auto target = std::find_if(chain.project.tempo.points.begin(), chain.project.tempo.points.end(), [&](const auto& point) { return point.clipId == incoming.id; });
            check(chain.error.isEmpty() && target != chain.project.tempo.points.end() && closeEnough(target->bpm, incoming.media->bpm())
                && closeEnough(chain.project.tempo.bpmAt(chain.project.duration() + 1), incoming.media->bpm()),
                "each later live addition reaches its own BPM instead of retaining track two's tempo: " + chain.error);
            bool preserved = chain.error.isEmpty();
            for (double time = 0; time <= before.duration(); time += .137)
                preserved &= closeEnough(before.tempo.bpmAt(time), chain.project.tempo.bpmAt(time)) && closeEnough(before.tempo.beatsAt(time), chain.project.tempo.beatsAt(time));
            for (size_t n = 0; n < before.clips.size(); ++n)
                preserved &= chain.project.clips[n].prepared == before.clips[n].prepared && chain.project.clips[n].elasticSource == before.clips[n].elasticSource
                    && closeEnough(chain.project.clips[n].start, before.clips[n].start) && closeEnough(chain.project.clips[n].end(), before.clips[n].end());
            check(preserved, "later live tempo ramps preserve every established voice, placement and running beat through its end");
            bool aligned = chain.error.isEmpty();
            for (const auto& clip : chain.project.clips)
                for (double beat = clip.sourceElapsed(clip.firstBeatTime()); beat < clip.sourceElapsed(clip.length); beat += 60 / clip.media->bpm())
                    aligned &= std::abs(std::remainder(chain.project.tempo.beatsAt(clip.beatTimelineTime(beat)), 1.0)) < 1.0e-6;
            check(aligned, "every source pulse stays on the shared grid across successive ascending and descending live tempo ramps");
            check(parseProject(juce::JSON::toString(projectToJson(chain.project))).error.isEmpty(),
                "live guard boundaries and source-owned future targets preserve the session contract");
        }
    }
    for (size_t n = 1; n < ready.project.clips.size(); ++n)
    {
        const auto& c = ready.project.clips[n];
        const double overlap = ready.project.tempo.beatsAt(ready.project.clips[n - 1].end()) - ready.project.tempo.beatsAt(c.start);
        check(overlap <= 32 + 1.0e-7 && overlap > 28 && closeEnough(std::remainder(ready.project.tempo.beatsAt(c.start + c.firstDownbeatTime()), 4), 0),
              "single-target tempo ramps align incoming bars within one bar of the requested overlap");
    }
    auto edit = ready.project; edit.tempo.movePoint(1, edit.tempo.points[1].time, 126.25);
    auto adjusted = prepareTempoMap(edit, false, 32);
    check(adjusted.error.isEmpty() && adjusted.project.tempo.points.size() == 3 && closeEnough(adjusted.project.tempo.points[1].bpm, 126.25)
          && adjusted.project.tempo.points[1].clipId == ready.project.tempo.points[1].clipId, "editing an owned tempo target retains its identity without adding another marker");
    const auto partial = prepareTempoMap(adjusted.project, true, 32, 0, 1);
    check(partial.error.isEmpty() && partial.project.tempo.points == adjusted.project.tempo.points
        && closeEnough(partial.project.clips[1].start + partial.project.clips[1].transitionTime(true), partial.project.tempo.points[1].time),
        "partial rebuild retains existing edited targets and their established placement semantics");
    Engine engine; engine.project = ready.project; engine.library = media;
    const auto saved = juce::JSON::toString(engine.captureProject()); Engine reopened;
    const auto error = reopened.restoreProject(saved);
    check(error.isEmpty() && reopened.project.tempo.points == ready.project.tempo.points, "save/reopen retains owned tempo targets: " + error);
    auto invalid = juce::JSON::parse(saved); invalid["tempo"][1].getDynamicObject()->setProperty("clipId", "missing");
    check(parseProject(juce::JSON::toString(invalid)).error.isNotEmpty(), "unknown tempo owners are rejected before media loading");
    auto manual = ready.project; manual.tempo.setPoint(manual.duration() + 20, 142);
    auto manualReady = prepareTempoMap(manual, false, 32);
    check(manualReady.error.isEmpty() && manualReady.project.tempo.points.size() == 4 && manualReady.project.tempo.points.back().clipId.isEmpty(), "additional independent tempo markers coexist with the track defaults");
    {
        auto source = input.clips[0]; source.id = juce::Uuid().toString(); source.effects = makeClipEffects();
        auto candidate = manualReady.project; candidate.clips.push_back(source);
        const auto next = prepareTempoMap(candidate, false, 32, 0, 0, 2.0);
        bool preserved = next.error.isEmpty();
        for (double time = 0; time <= manualReady.project.tempo.points.back().time; time += .137)
            preserved &= closeEnough(next.project.tempo.bpmAt(time), manualReady.project.tempo.bpmAt(time))
                && closeEnough(next.project.tempo.beatsAt(time), manualReady.project.tempo.beatsAt(time));
        check(preserved && next.project.tempo.points == manualReady.project.tempo.points,
            "caller-selected future markers leave too little room for a live ramp, so the existing clock is retained");
    }
    auto live = ready.project; auto incoming = input.clips[0]; incoming.id = juce::Uuid().toString(); incoming.effects = makeClipEffects(); live.clips.push_back(incoming);
    auto joined = prepareTempoMap(live, false, 32, 0, 0, 2.0);
    bool unchanged = joined.error.isEmpty() && std::count_if(joined.project.tempo.points.begin(), joined.project.tempo.points.end(), [](const auto& point) { return point.clipId.isNotEmpty(); }) == 4;
    for (double time : {0.0, 2.0, 10.0, 35.0, 80.0, 160.0})
        if (time <= ready.project.duration()) unchanged &= closeEnough(joined.project.tempo.bpmAt(time), ready.project.tempo.bpmAt(time)) && closeEnough(joined.project.tempo.beatsAt(time), ready.project.tempo.beatsAt(time));
    check(unchanged && joined.project.clips[0].prepared == ready.project.clips[0].prepared, "live insertion preserves the tempo clock through all established voices and retains their PCM");
    auto shortMix = input; shortMix.clips.resize(2); for (auto& c : shortMix.clips) c.length = 2;
    auto coincident = prepareTempoMap(shortMix, false, 32);
    check(coincident.error.isEmpty() && coincident.project.tempo.points.size() == 1 && closeEnough(coincident.project.tempo.bpmAt(2), 120)
          && parseProject(juce::JSON::toString(projectToJson(coincident.project),true,17)).error.isEmpty(),
          "short coincident sources keep the established tempo instead of adding an abrupt target: " + coincident.error);
    for (bool rebuild : {false, true}) for (double targetBpm : {128., 112.})
    {
        auto close = input; close.clips.resize(2); close.clips[0].length = 20;
        close.clips[1].media = media[1]->catalogueCopy(); close.clips[1].media->samples = media[1]->samples;
        close.clips[1].media->manualBpm = targetBpm;
        const auto result = prepareTempoMap(close, rebuild, 32);
        bool spaced = result.error.isEmpty() && result.project.tempo.points.size() == 2;
        juce::String detail = result.error;
        if (spaced)
        {
            const auto& clock = result.project.tempo; const auto& clip = result.project.clips[1];
            const auto& marker = clock.points[1];
            detail += " target beat " + juce::String(clock.beatsAt(marker.time), 9) + " start beat " + juce::String(clock.beatsAt(clip.start), 9)
                + " source span " + juce::String(clip.length * clip.rate, 9);
            spaced &= closeEnough(clock.beatsAt(marker.time) - clock.beatsAt(clock.points[0].time), 64)
                && closeEnough(marker.bpm, targetBpm) && marker.time > clip.start + clip.transitionTime(true)
                && marker.time <= clip.end() && closeEnough(clock.beatsAt(clip.start), 8) && closeEnough(clip.length * clip.rate, 48);
            double pulseError = 0;
            for (double beat = clip.sourceElapsed(clip.firstBeatTime()); beat < clip.sourceElapsed(clip.length); beat += 60 / targetBpm)
                pulseError = std::max(pulseError, std::abs(std::remainder(clock.beatsAt(clip.beatTimelineTime(beat)), 1.)));
            spaced &= pulseError < 1e-6;
            const auto again = prepareTempoMap(result.project, false, 32);
            const auto parsed = parseProject(juce::JSON::toString(projectToJson(result.project), true, 17));
            report.getSiblingFile("spacing-" + juce::String((int) targetBpm) + "-" + juce::String((int) rebuild) + ".json")
                .replaceWithText(juce::JSON::toString(projectToJson(result.project), false, 17));
            spaced &= again.error.isEmpty() && again.project.tempo.points == clock.points
                && parsed.error.isEmpty();
            detail += " pulse error " + juce::String(pulseError, 9) + " repeat " + again.error + " parse " + parsed.error;
        }
        check(spaced, "close automatic target gets a 16-bar ramp without moving source beats or changing saved/reprepared geometry: "
            + juce::String(targetBpm) + " rebuild " + juce::String((int) rebuild) + " / " + detail);
    }
    {
        auto cut = shortMix; const auto cutResult = prepareTempoMap(cut, true, 0);
        check(cutResult.error.isEmpty() && cutResult.project.tempo.points.size() == 2
            && !cutResult.project.tempo.points.back().ramp && closeEnough(cutResult.project.tempo.points.back().bpm, 128),
            "deliberate Cut keeps its immediate BPM change even with very short sources");
        auto shortLive = first.project; auto added = input.clips[1]; added.length = 8; shortLive.clips.push_back(added);
        const auto result = prepareTempoMap(shortLive, false, 32, 0, 0, 2.);
        bool sameClock = result.error.isEmpty();
        for (double time = 0; time <= result.project.duration(); time += .137)
            sameClock &= closeEnough(result.project.tempo.bpmAt(time), first.project.tempo.bpmAt(time))
                && closeEnough(result.project.tempo.beatsAt(time), first.project.tempo.beatsAt(time));
        check(sameClock
            && result.project.clips[0].prepared == first.project.clips[0].prepared,
            "short live addition keeps the running clock and PCM when a 16-bar future ramp cannot fit");
        auto blocked = first.project; blocked.tempo.setPoint(22, 124); blocked.clips.push_back(input.clips[1]);
        blocked.clips.back().pendingPlacement = PendingPlacement::drop; blocked.clips.back().start = 4;
        const auto result2 = prepareTempoMap(blocked, false, 32);
        check(result2.error.isEmpty() && result2.project.tempo.points == blocked.tempo.points,
            "automatic drop does not squeeze a target against or cross an existing manual marker");
        blocked.clips.back().media = media[0];
        const auto same = prepareTempoMap(blocked, false, 32);
        check(same.error.isEmpty() && same.project.tempo.points == blocked.tempo.points,
            "same-BPM drop inside an existing ramp does not compress the remaining BPM change");
    }

    Workstation ui(false); ui.stopTimer(); ui.setSize(1440, 940); ui.engine.project = ready.project; ui.engine.library = media;
    auto unknown = media[0]->catalogueCopy(); unknown->id = juce::Uuid().toString(); unknown->assets = {}; unknown->name = "No artwork"; ui.engine.library.push_back(unknown);
    ui.refresh(); ui.libraryTable.sortOrderChanged(5, true);
    int redA = -1, redB = -1;
    for (int n = 0; n < (int) ui.visibleMedia.size(); ++n) { if (ui.engine.library[(size_t) ui.visibleMedia[(size_t) n]] == media[0]) redA = n; if (ui.engine.library[(size_t) ui.visibleMedia[(size_t) n]] == media[2]) redB = n; }
    check(std::abs(redA - redB) == 1 && ui.engine.library[(size_t) ui.visibleMedia.back()] == unknown && media[0]->assets.artworkGroup == media[2]->assets.artworkGroup,
          "thumbnail sorting groups matching embedded artwork from different files and leaves missing art last");
    ui.libraryTable.sortOrderChanged(5, false);
    check(ui.engine.library[(size_t) ui.visibleMedia.back()] == unknown, "reverse thumbnail sorting still keeps unknown artwork separate");
    const auto before = juce::JSON::toString(projectToJson(ui.engine.project)); ui.playlistTable.sortOrderChanged(5, true);
    const auto selected = ui.playlistTable.clipAtRow(0)->id; ui.playlistTable.selectedRowsChanged(0);
    check(before == juce::JSON::toString(projectToJson(ui.engine.project)) && ui.selectedClip == selected,
          "playlist artwork sorting changes presentation only and row selection follows clip identity");
    ui.playlistTable.sortOrderChanged(5, false);
    check(ui.playlistTable.clipAtRow(ui.playlistTable.table.getSelectedRow())->id == selected, "keyboard row selection follows the selected clip after reverse artwork sorting");
    ui.playlistTable.sortOrderChanged(3, true);
    check(ui.playlistTable.clipAtRow(0)->id == ready.project.clips[0].id && before == juce::JSON::toString(projectToJson(ui.engine.project)), "START sorting restores chronological presentation without moving tracks");
    ui.engine.project = coincident.project;
    ui.engine.project.tempo.setTrackPoint(ui.engine.project.clips[1].id, 0, 128, true);
    ui.tempoStrip.selectedTempoPoint = -1; ui.refresh();
    check(!ui.tempoStrip.tempoValueBounds(0).intersects(ui.tempoStrip.tempoValueBounds(1)) && !ui.tempoStrip.tempoValueBounds(0).isEmpty(), "coincident tempo targets have separately clickable BPM labels");
    ui.engine.project = ready.project; ui.refresh();
    auto menu = ui.timelineMenu(6, 0); bool addTempo = false;
    for (juce::PopupMenu::MenuItemIterator i(menu); i.next();) addTempo |= i.getItem().itemID == (int) TimelineAction::tempo && i.getItem().isEnabled;
    check(addTempo, "RMB exposes Add Tempo Marker independently of clip selection");
    ui.engine.position = 6; const bool handled = ui.keyPressed(juce::KeyPress('P'));
    auto* dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
    check(handled && dialog && closeEnough(dialog->getTextEditorContents("time").getDoubleValue(), ui.engine.project.snappedTime(6, true)), "P opens the tempo-marker editor at the playhead");
    if (dialog) dialog->exitModalState(0);
    const auto pump = [](int milliseconds)
    {
        const auto end = juce::Time::getMillisecondCounterHiRes()+milliseconds;
        do {
#if JUCE_WINDOWS
            MSG message; while (PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
#endif
            juce::Thread::sleep(1);
        } while (juce::Time::getMillisecondCounterHiRes() < end);
    };
    pump(20);
    ui.engine.project = first.project; ui.selectedClip = first.project.clips.front().id; ui.clearEditSelection(); ui.refresh();
    const auto one = juce::JSON::toString(projectToJson(ui.engine.project));
    ui.removeClip();
    check(ui.engine.project.clips.empty() && ui.engine.project.tempo.points.empty(),"deleting the only track clears its automatic tempo marker instead of turning it into a manual marker");
    ui.undo(false); check(juce::JSON::toString(projectToJson(ui.engine.project)) == one,"Undo restores deleted source and automatic tempo ownership together");
    ui.engine.project = ready.project; const auto removedId = ready.project.clips.back().id;
    ui.engine.project.tempo.setPoint(200,126,false); ui.selectedClip = removedId; ui.refresh(); ui.removeClip();
    const auto deadline = juce::Time::getMillisecondCounterHiRes()+20000;
    while (ui.busy && juce::Time::getMillisecondCounterHiRes() < deadline) pump(10);
    check(!ui.busy && !ui.engine.project.clipFor(removedId)
        && std::none_of(ui.engine.project.tempo.points.begin(),ui.engine.project.tempo.points.end(),[&](const auto& point) { return point.clipId == removedId; })
        && std::any_of(ui.engine.project.tempo.points.begin(),ui.engine.project.tempo.points.end(),[](const auto& point) { return point.clipId.isEmpty() && point.time == 200 && point.bpm == 126; })
        && ui.engine.playbackReady() && parseProject(juce::JSON::toString(projectToJson(ui.engine.project))).error.isEmpty(),
        "deleting an owned target re-prepares remaining sources and preserves explicit manual markers");
    ui.engine.project = ready.project; ui.selectedClip = ready.project.clips.front().id; ui.editSelection.primary = ui.selectedClip;
    for (const auto& clip : ready.project.clips) ui.editSelection.clips.push_back(clip.id);
    ui.removeClip();
    check(ui.engine.project.clips.empty() && ui.engine.project.tempo.points.empty(),"group deletion clears all removed tracks' automatic tempo markers");
    ui.engine.project = ready.project; ui.prepareTempo(ui.engine.project,false,false); ui.newSession();
    ui.workers.removeAllJobs(false,-1); pump(30);
    check(!ui.busy && ui.engine.project.clips.empty() && ui.engine.project.tempo.points.empty() && ui.pendingTempo == 0 && !ui.tempoPreparationPending
        && ui.undoHistory.empty(),"New Mix clears tempo state and rejects a late preparation result from the previous document");
    ui.engine.project = ready.project;
    ui.timeline.pixelsPerSecond = 10; ui.showBrowserPage(1); ui.refresh();
    auto output = report.getSiblingFile("tempo-browser.png").createOutputStream(); juce::PNGImageFormat png;
    check(output && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *output), "capture track tempo targets and sortable artwork columns");
}
}
