#include "Workstation.h"
#include "Beatmatch.h"
#include <cmath>

namespace mixworks
{
void runTempoBrowserTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    auto near = [](double a, double b) { return std::abs(a - b) < 1.0e-6; };
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
        targets &= p != ready.project.tempo.points.end() && near(p->time, c.start + c.transitionTime(true)) && near(p->bpm, c.media->bpm());
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
            check(chain.error.isEmpty() && target != chain.project.tempo.points.end() && near(target->bpm, incoming.media->bpm())
                && near(chain.project.tempo.bpmAt(chain.project.duration() + 1), incoming.media->bpm()),
                "each later live addition reaches its own BPM instead of retaining track two's tempo: " + chain.error);
            bool preserved = chain.error.isEmpty();
            for (double time = 0; time <= before.duration(); time += .137)
                preserved &= near(before.tempo.bpmAt(time), chain.project.tempo.bpmAt(time)) && near(before.tempo.beatsAt(time), chain.project.tempo.beatsAt(time));
            for (size_t n = 0; n < before.clips.size(); ++n)
                preserved &= chain.project.clips[n].prepared == before.clips[n].prepared && chain.project.clips[n].elasticSource == before.clips[n].elasticSource
                    && near(chain.project.clips[n].start, before.clips[n].start) && near(chain.project.clips[n].end(), before.clips[n].end());
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
        check(overlap <= 32 + 1.0e-7 && overlap > 28 && near(std::remainder(ready.project.tempo.beatsAt(c.start + c.firstDownbeatTime()), 4), 0),
              "single-target tempo ramps align incoming bars within one bar of the requested overlap");
    }
    auto edit = ready.project; edit.tempo.movePoint(1, edit.tempo.points[1].time, 126.25);
    auto adjusted = prepareTempoMap(edit, false, 32);
    check(adjusted.error.isEmpty() && adjusted.project.tempo.points.size() == 3 && near(adjusted.project.tempo.points[1].bpm, 126.25)
          && adjusted.project.tempo.points[1].clipId == ready.project.tempo.points[1].clipId, "editing an owned tempo target retains its identity without adding another marker");
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
            preserved &= near(next.project.tempo.bpmAt(time), manualReady.project.tempo.bpmAt(time))
                && near(next.project.tempo.beatsAt(time), manualReady.project.tempo.beatsAt(time));
        check(preserved && near(next.project.tempo.bpmAt(next.project.duration() + 1), source.media->bpm()),
            "a new live target preserves caller-selected future tempo markers before its own ramp");
    }
    auto live = ready.project; auto incoming = input.clips[0]; incoming.id = juce::Uuid().toString(); incoming.effects = makeClipEffects(); live.clips.push_back(incoming);
    auto joined = prepareTempoMap(live, false, 32, 0, 0, 2.0);
    bool unchanged = joined.error.isEmpty() && std::count_if(joined.project.tempo.points.begin(), joined.project.tempo.points.end(), [](const auto& point) { return point.clipId.isNotEmpty(); }) == 4;
    for (double time : {0.0, 2.0, 10.0, 35.0, 80.0, 160.0})
        if (time <= ready.project.duration()) unchanged &= near(joined.project.tempo.bpmAt(time), ready.project.tempo.bpmAt(time)) && near(joined.project.tempo.beatsAt(time), ready.project.tempo.beatsAt(time));
    check(unchanged && joined.project.clips[0].prepared == ready.project.clips[0].prepared, "live insertion preserves the tempo clock through all established voices and retains their PCM");
    auto shortMix = input; shortMix.clips.resize(2); for (auto& c : shortMix.clips) c.length = 2;
    auto coincident = prepareTempoMap(shortMix, false, 32);
    check(coincident.error.isEmpty() && coincident.project.tempo.points.size() == 2 && coincident.project.tempo.points[0].time == 0
          && coincident.project.tempo.points[1].time == 0 && parseProject(juce::JSON::toString(projectToJson(coincident.project))).error.isEmpty(), "short coincident tracks retain both tempo-marker identities");

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
    ui.engine.project = coincident.project; ui.tempoStrip.selectedTempoPoint = -1; ui.refresh();
    check(!ui.tempoStrip.tempoValueBounds(0).intersects(ui.tempoStrip.tempoValueBounds(1)) && !ui.tempoStrip.tempoValueBounds(0).isEmpty(), "coincident tempo targets have separately clickable BPM labels");
    ui.engine.project = ready.project; ui.refresh();
    auto menu = ui.timelineMenu(6, 0); bool addTempo = false;
    for (juce::PopupMenu::MenuItemIterator i(menu); i.next();) addTempo |= i.getItem().itemID == (int) TimelineAction::tempo && i.getItem().isEnabled;
    check(addTempo, "RMB exposes Add Tempo Marker independently of clip selection");
    ui.engine.position = 6; const bool handled = ui.keyPressed(juce::KeyPress('P'));
    auto* dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
    check(handled && dialog && near(dialog->getTextEditorContents("time").getDoubleValue(), ui.engine.project.snappedTime(6, true)), "P opens the tempo-marker editor at the playhead");
    if (dialog) dialog->exitModalState(0);
    ui.timeline.pixelsPerSecond = 10; ui.showBrowserPage(1); ui.refresh();
    auto output = report.getSiblingFile("tempo-browser.png").createOutputStream(); juce::PNGImageFormat png;
    check(output && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *output), "capture track tempo targets and sortable artwork columns");
}
}
