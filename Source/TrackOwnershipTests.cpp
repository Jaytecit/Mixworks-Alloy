#include "Workstation.h"
#include "Beatmatch.h"
#include "AutoArrangement.h"
#include <cmath>

namespace mixworks
{
void runTrackOwnershipTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report, const juce::File& plugin)
{
    if (juce::JUCEApplicationBase::getCommandLineParameters().contains("--split-effects-only"))
    {
        constexpr int sr = 8000, frames = 4000;
        auto media = std::make_shared<Media>(); media->sampleRate = sr; media->name = "One track, shared echo";
        media->manualBpm = 120; media->manualBeatOrigin = 0; media->manualKey = 4;
        media->file = report.getSiblingFile("split-source.wav"); media->samples.setSize(2, frames); media->samples.clear();
        for (int sample : {120, 1760, 3360})
        { media->samples.setSample(0, sample, .5f); media->samples.setSample(1, sample, -.25f); }
        {
            juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream = media->file.createOutputStream();
            auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(sr).withNumChannels(2).withBitsPerSample(24));
            check(writer && writer->writeFromAudioSampleBuffer(media->samples, 0, frames), "create a small isolated source for shared-effects checks");
        }
        auto uiOwner=std::make_unique<Workstation>(false); auto& ui=*uiOwner; ui.stopTimer(); ui.setSize(1600, 900); ui.engine.sampleRate = sr;
        ui.engine.project.masterDb = 0; ui.engine.project.snap = false; ui.engine.library = {media};
        Clip clip; clip.media = media; clip.length = .5; ui.engine.project.clips = {clip}; ui.selectedClip = clip.id; ui.selectTrack(0);
        const auto loaded = ui.engine.addPlugin(0, plugin);
        check(loaded.isEmpty(), "load the real finite echo VST3: " + loaded); if (loaded.isNotEmpty()) return;
        auto render = [&](Engine& engine, int count = 7200)
        {
            engine.prepareClipProcessing(); engine.resetPlugins();
            juce::AudioBuffer<float> result(2, count), block(2, 113); juce::MidiBuffer midi;
            for (int at = 0; at < count; at += 113)
            {
                const int n = std::min(113, count - at); engine.render(block, n, at / (double)sr, midi);
                for (int ch = 0; ch < 2; ++ch) result.copyFrom(ch, at, block, ch, 0, n);
            }
            return result;
        };
        const auto beforeDuplicate=ui.engine.project;
        const auto originalEffects=ui.engine.project.clips[0].effects;
        ui.duplicateClip();
        const auto duplicateId=ui.selectedClip;const auto* duplicate=ui.engine.project.clipFor(duplicateId);
        bool copiedState=duplicate && duplicate->effects!=originalEffects && duplicate->effects->plugins.size()==1;
        if(copiedState)
        {
            const auto& a=*originalEffects->plugins[0];const auto& b=*duplicate->effects->plugins[0];
            copiedState=a.id!=b.id && a.processor.get()!=b.processor.get()
                && a.processor->getParameters().size()==b.processor->getParameters().size();
            for(int i=0;copiedState && i<a.processor->getParameters().size();++i)
                copiedState=std::abs(a.processor->getParameters()[i]->getValue()-b.processor->getParameters()[i]->getValue())<1.e-6f;
        }
        check(copiedState && ui.undoHistory.size()==1 && duplicate->start==.5,
              "actual Duplicate clones the real native echo state into a distinct processor and slot with one Undo");
        ui.undo(false);const bool duplicateUndone=ui.engine.project.clips.size()==1 && ui.engine.project.clips[0].effects==originalEffects;
        ui.undo(true);
        check(duplicateUndone && ui.engine.project.clipFor(duplicateId) && ui.engine.project.clipFor(duplicateId)->effects!=originalEffects,
              "native-effect Duplicate Undo/Redo preserves independent processor ownership");
        ui.engine.project=beforeDuplicate;ui.selectedClip=clip.id;ui.undoHistory.clear();ui.redoHistory.clear();ui.engine.position=0;
        const auto intact = render(ui.engine);
        check(std::abs(ui.engine.project.audibleEnd() - .875) < 1.e-8 && std::abs(intact.getSample(0, 6360) - .05f) < 1.e-6f,
              "reported echo duration extends playback automatically and retains the last echo after source end");
        ui.engine.position = .18; ui.splitClip(); ui.engine.position = .34; ui.splitClip();
        check(ui.engine.project.clips.size() == 3 && ui.engine.project.clips[0].effects == ui.engine.project.clips[1].effects
              && ui.engine.project.clips[1].effects == ui.engine.project.clips[2].effects,
              "the actual Split action retains one insert chain across all pieces");
        const auto split = render(ui.engine); float error = 0;
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < intact.getNumSamples(); ++i) error = std::max(error, std::abs(intact.getSample(ch,i) - split.getSample(ch,i)));
        check(error < 1.e-6f, "split and unsplit echo output match sample-for-sample across boundaries and the final tail: " + juce::String(error, 9));
        const auto group = ui.engine.project;
        ui.selectedClip = group.clips[1].id; const auto address = ui.engine.pluginAddress(0, 0, 0);
        check(address && ui.engine.ensurePluginAutomation(*address), "effect automation can be created from a middle piece"); if (!address) return;
        auto* parameter = ui.engine.pluginChain(0)[0]->processor->getParameters()[0];
        const float manual = ui.engine.project.manualValue(*address);
        ui.remember();
        parameter->setValue(manual < .5f ? .95f : .05f);
        ui.undo(false);
        check(std::abs(parameter->getValue() - manual) < 1.e-6f && ui.engine.project.curve(*address) && !ui.engine.project.curve(*address)->active(),
              "normal Undo restores the real shared VST parameter's inactive-lane manual value");
        auto* curve = ui.engine.project.curve(*address); curve->enabled = true; curve->setPoint(0, .25f); curve->setPoint(.7, 1);
        auto firstAddress = *address; firstAddress.clipId = group.clips[0].id;
        auto lastAddress = *address; lastAddress.clipId = group.clips[2].id;
        check(ui.engine.project.curve(firstAddress) == curve && ui.engine.project.curve(lastAddress) == curve
              && std::abs(ui.engine.project.automationTime(lastAddress, .42) - .42) < 1.e-8,
              "all pieces edit one curve on the original track clock, including nodes beyond the final piece");
        const auto automated = ui.engine.project;
        ui.selectedClip = group.clips[0].id;
        ui.editSelection.clips = {group.clips[0].id, group.clips[1].id, group.clips[2].id}; ui.editSelection.primary = ui.selectedClip;
        ui.performTimelineAction(TimelineAction::removePiece, .25, 0);
        check(ui.engine.project.clips.size() == 2 && ui.engine.project.clipFor(group.clips[0].id) && ui.engine.project.clipFor(group.clips[2].id),
              "Delete this piece removes only the middle piece even with a whole-track selection");
        ui.selectedClip = group.clips[0].id;
        ui.engine.project.curve(firstAddress)->enabled = false; ui.engine.pluginChain(0)[0]->processor->getParameters()[0]->setValue(1);
        const auto gap = render(ui.engine);
        check(std::abs(gap.getSample(0, 2120) - .15f) < 1.e-6f && std::abs(gap.getSample(0, 6360) - .05f) < 1.e-6f,
              "echo continues through the deleted gap and beyond the last surviving piece");
        ui.engine.project = automated;
        ui.engine.project.tempo.points.push_back({0, 120, false, group.clips[0].id});
        ui.selectedClip = group.clips[0].id; ui.clearEditSelection(); ui.removeClip();
        check(ui.engine.project.clips.size() == 2 && std::abs(ui.engine.project.automationTime(lastAddress, .42) - .42) < 1.e-8
              && ui.engine.project.curve(lastAddress)->points.size() == 2,
              "deleting the first piece preserves the shared effect curve and original clock");
        check(ui.engine.project.tempo.points.size() == 1 && ui.engine.project.tempo.points[0].clipId == group.clips[1].id,
              "deleting one piece retains its original track's automatic tempo marker on a survivor");
        ui.undo(false);
        check(ui.engine.project.clips.size() == 3 && ui.engine.project.clips[0].effects == ui.engine.project.clips[2].effects,
              "Undo restores the deleted piece with shared effect ownership");
        ui.engine.project = automated; ui.selectedClip = group.clips[1].id; ui.selectedAutomationTarget = -1;
        ui.editSelection.clips = {group.clips[0].id, group.clips[1].id, group.clips[2].id}; ui.editSelection.primary = ui.selectedClip;
        ui.timeline.pixelsPerSecond = 1200; ui.refresh();
        const auto mouse = juce::Desktop::getInstance().getMainMouseSource(); const juce::Point<float> point {300, ui.timeline.laneY(0,40)};
        const auto event = juce::MouseEvent(mouse, point, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1, 0, 0, 0, 0,
            &ui.timeline, &ui.timeline, juce::Time::getCurrentTime(), point, juce::Time::getCurrentTime(), 1, false);
        ui.timeline.mouseDown(event); ui.timeline.mouseUp(event);
        check(!ui.editSelection.multiple() && ui.selectedClip == group.clips[1].id, "a plain click selects one split piece from an existing group");
        ui.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey));
        check(ui.engine.project.clips.size() == 2 && !ui.engine.project.clipFor(group.clips[1].id), "Delete key after that click removes only the chosen piece");
        ui.engine.project = automated; ui.selectedClip = group.clips[1].id; ui.clearEditSelection();
        const auto second = ui.engine.addPlugin(0, plugin);
        check(second.isEmpty() && ui.engine.project.clips[0].effects->plugins.size() == 2 && ui.engine.project.clips[2].effects->plugins.size() == 2,
              "adding an effect after splitting applies it to every piece");
        ui.engine.removePlugin(0, 1);
        Clip other; other.media = media; other.start = .1; other.length = .3; other.track = 1;
        ui.engine.project.clips.push_back(other);
        check(other.effects != group.clips[0].effects && other.effects->plugins.empty(), "a separately placed copy of the same song keeps independent effects");
        const auto saved = report.getSiblingFile("shared-effects.mixworks"); const auto saveError = ui.engine.saveProject(saved);
        PreparedProject prepared; prepared.parsed = parseProject(saved.loadFileAsString()); prepared.library = {media};
        for (auto reference : prepared.parsed.clips) { reference.clip.media = media; prepared.parsed.project.clips.push_back(std::move(reference.clip)); }
        auto reopenedOwner=std::make_unique<Engine>(); auto& reopened=*reopenedOwner; reopened.sampleRate = sr;
        const auto reopenedError = prepared.parsed.error.isNotEmpty() ? prepared.parsed.error : reopened.commitProject(std::move(prepared));
        check(saveError.isEmpty() && reopenedError.isEmpty() && reopened.project.clips.size() == 4
              && reopened.project.clips[0].effects == reopened.project.clips[2].effects && reopened.project.clips[3].effects != reopened.project.clips[0].effects
              && reopened.project.curve(firstAddress) && reopened.project.curve(firstAddress)->points.size() == 2,
              "save/reopen restores shared processors, continuous automation and separate placements: " + saveError + reopenedError);
        juce::String exportError; auto exported = ui.engine.createExportRenderer(exportError);
        check(exported && exportError.isEmpty() && exported->project.clips[0].effects == exported->project.clips[2].effects
              && exported->project.clips[0].effects != ui.engine.project.clips[0].effects,
              "export clones one independent processor per original track while retaining split ownership");
        if (exported)
        {
            const auto live = render(ui.engine), offline = render(*exported); float difference = 0;
            for (int i = 0; i < live.getNumSamples(); ++i) difference = std::max(difference, std::abs(live.getSample(0,i) - offline.getSample(0,i)));
            check(difference < 1.e-6f, "live and export render the same shared effect automation and tails");
        }
        WavExportPlan plan; const auto planned = ui.engine.planWavExport(report.getSiblingFile("tail-plan.wav"), {true}, plan);
        check(planned.wasOk() && std::abs(plan.tailSeconds - .375) < 1.e-8, "WAV export includes the natural echo tail without a manual tail setting");
        {
            auto laneOwner=std::make_unique<Engine>(); auto& lane=*laneOwner; lane.sampleRate = sr; Clip dry; dry.media = media; dry.length = .5; lane.project.clips = {dry};
            const auto laneError = lane.addPlugin(0, plugin); WavExportPlan lanePlan;
            check(laneError.isEmpty() && lane.planWavExport(report.getSiblingFile("lane-tail-plan.wav"), {true}, lanePlan).wasOk()
                  && std::abs(lanePlan.tailSeconds - .375) < 1.e-8, "whole-lane effects also receive their reported natural ending");
            lane.project.effectTailSeconds = 1;
            check(lane.planWavExport(report.getSiblingFile("minimum-tail-plan.wav"), {true}, lanePlan).wasOk()
                  && std::abs(lanePlan.tailSeconds - 1.375) < 1.e-8, "a manual effect ending still allows downstream lane echoes to finish");
        }
        ui.engine.project.clips.resize(3); ui.selectedClip = group.clips[1].id;
        ui.performTimelineAction(TimelineAction::removePiece, .25, 0); ui.selectedClip = group.clips[0].id;
        ui.pluginLane = firstAddress; ui.selectedAutomationTarget = (int)AutomationTarget::plugin;
        check(ui.insertAutomationNode(firstAddress, .25, false) && ui.insertAutomationNode(firstAddress, .7, false),
              "the native node editor permits effect nodes in a deleted gap and in the ending tail");
        auto capture = [&](const juce::String& name, juce::Component& component)
        {
            auto stream = report.getSiblingFile(name + ".png").createOutputStream(); juce::PNGImageFormat png;
            return stream && png.writeImageToStream(component.createComponentSnapshot(component.getLocalBounds()), *stream);
        };
        ui.refresh(); ui.timeline.pixelsPerSecond = 800; ui.timeline.updateSize();
        check(capture("shared-effects", ui), "capture continuous effect automation across a deleted piece");
        ui.showShortcuts(true); ui.refresh(); check(capture("shortcuts", ui), "capture the revised shortcuts panel");
        ui.showAutoArrangement(); auto* dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        check(dialog && dialog->getComboBoxComponent("action")->getSelectedItemIndex() == 1 && !dialog->getComboBoxComponent("start")->isEnabled()
              && dialog->getButton(0)->getButtonText() == "Extend mix", "Auto Arrange opens with Extend current arrangement selected for an existing mix");
        if (dialog) { check(capture("auto-arrange", *dialog), "capture the actual Auto Arrange panel and orange border"); dialog->exitModalState(0); }
        juce::Timer::callAfterDelay(20, [] { juce::MessageManager::getInstance()->stopDispatchLoop(); }); juce::MessageManager::getInstance()->runDispatchLoop();
        juce::Image menu(juce::Image::RGB, 300, 100, true);
        { juce::Graphics graphics(menu); ui.look.drawPopupMenuBackground(graphics, 300, 100); }
        auto menuStream = report.getSiblingFile("menu-border.png").createOutputStream(); juce::PNGImageFormat png;
        check(menuStream && png.writeImageToStream(menu, *menuStream), "capture the shared menu frame styling");
        {
            PluginWindow editor(*ui.engine.pluginChain(0)[0]);
            check(capture("plugin-border", editor), "capture the native effect editor frame above its content");
        }
        return;
    }
    auto near = [](double a, double b) { return std::abs(a - b) < 1.0e-6; };
    auto media = std::make_shared<Media>(); media->sampleRate = 8000; media->manualBpm = 120; media->manualBeatOrigin = 0.213;
    media->name = "Track ownership"; media->file = report.getSiblingFile("ownership.wav");
    media->samples.setSize(2, 8000 * 120);
    for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < media->frames(); ++i) media->samples.setSample(ch, i, 0.25f);
    {
        juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream = media->file.createOutputStream();
        auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(2).withBitsPerSample(24));
        check(writer && writer->writeFromAudioSampleBuffer(media->samples, 0, media->frames()), "ownership tests use a real source file");
    }
    Clip a; a.media = media; a.length = 120; a.pendingPlacement = PendingPlacement::append;
    Clip b = a; b.id = juce::Uuid().toString(); b.effects = makeClipEffects(); b.track = 1;
    Project p; p.clips = {a, b};
    auto ready = prepareTempoMap(p, false, 32);
    check(ready.error.isEmpty(), "prepare default eight-bar arrangement: " + ready.error);
    if (ready.error.isNotEmpty()) return;
    const auto& first = ready.project.clips[0]; const auto& next = ready.project.clips[1];
    check(ready.project.tempo.points.size() == 2 && near(ready.project.tempo.points.back().time, next.start + next.transitionTime(true))
          && ready.project.tempo.points.back().bpm == next.media->bpm(), "adding two tracks creates exactly two tempo markers, with the incoming target at IN");
    check(near(std::remainder(ready.project.tempo.beatsAt(first.start + first.firstDownbeatTime()), 4), 0)
          && near(std::remainder(ready.project.tempo.beatsAt(next.start + next.firstDownbeatTime()), 4), 0)
          && near(ready.project.tempo.beatsAt(first.end()) - ready.project.tempo.beatsAt(next.start), 32),
          "automatic insertion aligns both bar phases and retains eight bars when source spans permit it");
    check(first.fadeIn == 0 && first.fadeOut == 0 && next.fadeIn == 0 && next.fadeOut == 0,
          "insertion never introduces volume fades");
    check(near(first.start + first.transitionTime(false), next.start + next.transitionTime(true)), "default IN and OUT handles coincide at the overlap start");
    const auto& outgoingBass = first.controls.automation[1]; const auto& incomingBass = next.controls.automation[1];
    const auto stepped = [](const AutomationCurve& curve) { return curve.active() && std::any_of(curve.points.begin(),curve.points.end(),[](const auto& point) { return point.bend == 1; }); };
    check(stepped(outgoingBass) && stepped(incomingBass) && !ready.project.transitionLinks[0].bands[1],
          "default transition swaps bass vertically using the ordinary source automation curve");
    Clip third = b; third.id = juce::Uuid().toString(); third.effects = makeClipEffects(); third.pendingPlacement = PendingPlacement::append;
    auto chain = ready.project; chain.clips.push_back(third); auto three = prepareTempoMap(chain, false, 32);
    check(three.error.isEmpty() && stepped(three.project.clips[1].controls.automation[1])
          && std::count_if(three.project.clips[1].controls.automation[1].points.begin(),three.project.clips[1].controls.automation[1].points.end(),[](const auto& point) { return point.bend == 1; }) >= 2,
          "the middle track keeps both its incoming and outgoing bass swaps");
    auto cutMix = prepareTempoMap(three.project, true, 0);
    bool preserved = cutMix.error.isEmpty() && cutMix.project.clips.size() == three.project.clips.size();
    for (size_t i = 0; preserved && i < three.project.clips.size(); ++i)
        preserved &= cutMix.project.clips[i].controls.automation[1].points.size() == three.project.clips[i].controls.automation[1].points.size();
    check(preserved,"rebuilding alignment without overlap preserves ordinary source-owned bass nodes");
    auto staged = ready.project; staged.clips[1].start += 11; staged.clips[1].pendingPlacement = PendingPlacement::drop;
    staged.clips[1].moveTo(173.123456);
    const auto explicitlyMoved = prepareTempoMap(staged, false, 32);
    bool noGenerator = explicitlyMoved.error.isEmpty();
    for (const auto& link : explicitlyMoved.project.transitionLinks) for (const auto& band : link.bands) noGenerator &= !band || !band->generated();
    check(noGenerator && near(explicitlyMoved.project.clips[1].start,173.123456) && explicitlyMoved.project.clips[1].controls.automation[1].active(),
          "a pending disjoint move keeps its exact position and source-owned nodes without a generated external handoff: " + explicitlyMoved.error);
    auto free = staged.clips[1]; free.moveTo(4.123456);
    check(near(free.start, 4.123456), "freehand moves preserve an exact requested time");
    auto snapped = next; snapped.moveTo(ready.project.snappedClipStart(snapped, 33.721, true));
    const double beat = ready.project.tempo.beatsAt(snapped.start + snapped.firstBeatTime());
    check(near(beat, std::round(beat)), "beat snapping aligns the source pulse rather than its pickup edge");
    bool pixels = true;
    for (double zoom : {3.7, 7.3, 10.0, 12.7, 27.01, 81.3, 200.0}) for (int n = 0; n < 170; ++n)
    {
        const double source = snapped.sourceElapsed(snapped.firstBeatTime()) + n * 0.5;
        const double time = snapped.beatTimelineTime(source);
        pixels &= Clip::gridPixel(time, zoom) == Clip::gridPixel(ready.project.tempo.timeAtBeat(std::round(ready.project.tempo.beatsAt(time))), zoom);
    }
    check(pixels, "track and background beats rasterize to identical pixels across seven fractional zoom scales");

    auto uiOwner=std::make_unique<Workstation>(false); auto& ui=*uiOwner; ui.stopTimer(); ui.setSize(1440, 940); ui.engine.project.masterDb = 0;
    a.pendingPlacement = PendingPlacement::none; a.start = 2; a.length = 12; a.effects = makeClipEffects(); a.controls = {};
    ui.engine.project.followTrackTempo = false; ui.engine.project.clips = {a}; ui.engine.library = {media}; ui.selectedClip = a.id;
    ui.engine.project.snap = false; ui.refresh();
    ui.performTimelineAction(TimelineAction::volume, 6.123, 0);
    auto address = AutomationAddress {0, AutomationTarget::volume, {}, {}, a.id};
    auto* curve = ui.engine.project.curve(address);
    check(curve && curve->points.size() == 1 && near(curve->points.front().time, 4.123) && ui.engine.project.tracks[0].automation[0].points.empty(),
          "RMB creates automation on its audio track in source coordinates");
    if (!curve || curve->points.empty()) return;
    curve->points.front().value = -12;
    auto& voice = ui.engine.project.clips[0]; voice.moveTo(9.327); voice.track = 2; ui.engine.selectedTrack = 2;
    check(near(ui.engine.project.automationSessionTime(address, curve->points.front().time), 13.45)
          && near(ui.engine.automationValue(address, 13.45), -12) && voice.controls.automation[0].active(),
          "automation follows exact free movement and lane changes");
    ui.engine.project.followTrackTempo = true; ui.engine.project.tempo.setPoint(0, 120); ui.engine.project.tempo.setPoint(15, 160);
    voice.beatmatched = true; voice.tempoSourceBpm = 120;
    voice.retime(7.913, std::make_shared<const TempoMap>(ui.engine.project.tempo));
    check(near(ui.engine.project.automationTime(address, ui.engine.project.automationSessionTime(address, 4.123)), 4.123),
          "automation retains its source position through a changing tempo map");
    ui.engine.project.followTrackTempo = false; ui.engine.project.tempo = {}; voice = a; voice.controls.automation[0].enabled = true;
    voice.controls.automation[0].setPoint(0, -12); voice.start = 0; voice.track = 0;
    Clip neighbour = a; neighbour.id = juce::Uuid().toString(); neighbour.start = 0; neighbour.effects = makeClipEffects(); neighbour.controls = {};
    ui.engine.project.clips.push_back(neighbour);
    juce::AudioBuffer<float> output(2, 512); juce::MidiBuffer midi;
    ui.engine.render(output, 512, 1, midi);
    check(near(output.getSample(0, 100), 0.25 * (1 + juce::Decibels::decibelsToGain(-12.0f))),
          "overlapping tracks on one lane retain independent volume automation in actual PCM");
    auto legacy = ui.engine.captureProject(); legacy.getDynamicObject()->setProperty("version", 10); legacy["tracks"].getArray()->removeRange(4, trackCount - 4);
    auto migratedOwner=std::make_unique<Engine>(); auto& migrated=*migratedOwner; const auto migrationError = migrated.restoreProject(juce::JSON::toString(legacy));
    check(migrationError.isEmpty(), "older sessions still reopen before ownership migration: " + migrationError);

    auto recorderOwner=std::make_unique<Engine>(); auto& recorder=*recorderOwner; recorder.project.clips = {a}; recorder.selectedClip = a.id;
    recorder.armAutomation(0, true); recorder.setAutomationRecordMode(AutomationRecordMode::touch); recorder.setPlaying(true); recorder.position = 6.5;
    recorder.setControlValue({0, AutomationTarget::volume}, -9); recorder.position = 6.7; recorder.finishAutomation();
    const auto& recorded = recorder.project.clips[0].controls.automation[0];
    check(recorded.active() && recorder.project.tracks[0].automation[0].points.empty()
          && std::any_of(recorded.points.begin(), recorded.points.end(), [&](const auto& point) { return near(point.time, 4.5) && point.value == -9; }),
          "Touch recording binds control changes to source positions on the selected audio track");
    recorder.project.clips[0].moveTo(10.125);
    check(near(recorder.project.automationSessionTime(address, 4.5), 14.625), "recorded automation follows a later move");
    if (plugin == juce::File()) return;
    ui.engine.project.clips.resize(1); ui.engine.project.clips[0] = a; ui.engine.project.clips[0].start = 0;
    ui.selectedClip = a.id; ui.engine.selectedTrack = 0;
    const auto error = ui.engine.addPlugin(0, plugin);
    check(error.isEmpty() && ui.engine.plugins[0].empty() && ui.engine.project.clips[0].effects->plugins.size() == 1,
          "a VST insert belongs to the selected audio track: " + error);
    if (error.isNotEmpty()) return;
    auto parameter = ui.engine.pluginAddress(0, 0, 0); check(parameter && parameter->clipId == a.id, "VST parameter identity includes its audio track owner");
    if (!parameter) return;
    ui.engine.ensurePluginAutomation(*parameter); auto* lane = ui.engine.project.curve(*parameter); lane->enabled = true; lane->setPoint(0, 0.5f);
    ui.engine.render(output, 512, 1, midi);
    check(near(output.getSample(0, 100), 0.125), "track-owned VST automation changes actual PCM");
    lane->enabled = false; ui.engine.pluginChain(0)[0]->processor->getParameters()[0]->setValue(0.25f);
    ui.engine.render(output, 512, 1, midi);
    check(near(output.getSample(0, 100), 0.0625), "disabled automation preserves manual VST parameter edits");
    lane->enabled = true;
    ui.engine.project.clips.push_back(neighbour); ui.engine.resetPlugins(); ui.engine.render(output, 512, 1, midi);
    check(near(output.getSample(0, 100), 0.375), "a track's VST does not process an overlapping neighbour on the same lane");
    ui.engine.project.clips.resize(1); ui.engine.selectedTrack = 0; ui.timeline.pixelsPerSecond = 30; ui.refresh();
    const auto mouse = juce::Desktop::getInstance().getMainMouseSource();
    auto event = [&](juce::Point<float> position, juce::Point<float> down, bool drag)
    { return juce::MouseEvent(mouse, position, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1, 0, 0, 0, 0, &ui.timeline, &ui.timeline, juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), 1, drag); };
    // Use the waveform body, clear of the footer and the horizontal VST line.
    const juce::Point<float> from {120, ui.timeline.laneY(0, 40)}, to {159, ui.timeline.laneY(3, 40)};
    const auto attached = ui.engine.project.clips[0].effects;
    ui.timeline.mouseDown(event(from, from, false)); ui.timeline.mouseDrag(event(to, from, true)); ui.timeline.mouseUp(event(to, from, true));
    check(near(ui.engine.project.clips[0].start, 1.3) && ui.engine.project.clips[0].track == 3 && ui.engine.project.clips[0].effects == attached,
          "native freehand dragging carries the effect chain and its markers to another lane");
    ui.undo(false); check(ui.engine.project.clips[0].start == 0 && ui.engine.project.clips[0].track == 0 && ui.engine.project.clips[0].effects == attached,
          "Undo restores track placement with its effect instance intact");
    ui.undo(true); check(near(ui.engine.project.clips[0].start, 1.3) && ui.engine.project.clips[0].track == 3, "Redo restores the moved track and effects together");
    auto& effected = ui.engine.project.clips[0]; const auto effects = effected.effects; effected.moveTo(7.123); effected.track = 3;
    ui.engine.project.clips.resize(1); ui.engine.selectedTrack = 3; ui.engine.resetPlugins(); ui.engine.render(output, 512, 8.123, midi);
    check(effected.effects == effects && ui.engine.pluginChain(3).size() == 1 && near(output.getSample(0, 100), 0.125),
          "VST chain, state and automation follow free movement onto another lane");
    ui.engine.resetPlugins();
    ui.engine.render(output, 512, std::ceil(7.123 * 44100) / 44100, midi);
    check(near(output.getSample(0, 0), 0.125), "a moved track applies its source-zero VST automation from the first audible sample");
    auto stale = *parameter; stale.clipId = "missing";
    check(!ui.engine.ensurePluginAutomation(stale), "a deleted automation owner cannot fall back to a mixer-lane effect");
    auto saved = juce::JSON::toString(ui.engine.captureProject()); auto reopenedOwner=std::make_unique<Engine>(); auto& reopened=*reopenedOwner;
    const auto reopenError = reopened.restoreProject(saved);
    reopened.project.masterDb = 0; reopened.render(output, 512, 8.123, midi);
    check(reopenError.isEmpty() && reopened.project.clips.size() == 1 && reopened.project.clips[0].effects->plugins.size() == 1 && near(output.getSample(0, 100), 0.125),
          "save/reopen preserves track-owned VST state, automation and PCM after moving lanes: " + reopenError);
    ui.performTimelineAction(TimelineAction::copy, 8, 3); ui.performTimelineAction(TimelineAction::paste, 30.123, 1);
    check(ui.engine.project.clips.size() == 2 && ui.engine.project.clips[0].effects != ui.engine.project.clips[1].effects
          && ui.engine.project.clips[0].effects->plugins[0]->id != ui.engine.project.clips[1].effects->plugins[0]->id,
          "copy/paste clones VST instances and rebinds their automation instead of sharing mutable DSP");
    ui.engine.render(output, 512, 31.123, midi);
    check(near(output.getSample(0, 100), 0.125), "copied VST automation drives its independently hosted effect");
    auto malformed = juce::JSON::parse(saved); malformed["clips"][0]["controls"]["pluginAutomation"][0].getDynamicObject()->setProperty("slotId", "missing");
    check(parseProject(juce::JSON::toString(malformed)).error.isNotEmpty(), "orphaned clip VST automation is rejected before loading plugins");
    auto oldOwner=std::make_unique<Engine>(); auto& old=*oldOwner; old.project.masterDb = 0; old.project.followTrackTempo = false; old.project.clips = {neighbour};
    old.project.tracks[0].automation[0].enabled = true; old.project.tracks[0].automation[0].setPoint(0, -6);
    const auto oldError = old.addPlugin(0, plugin);
    if (oldError.isEmpty()) old.plugins[0][0]->processor->getParameters()[0]->setValue(0.5f);
    auto oldSave = old.captureProject(); oldSave.getDynamicObject()->setProperty("version", 10); oldSave["tracks"].getArray()->removeRange(4, trackCount - 4); auto upgradedOwner=std::make_unique<Engine>(); auto& upgraded=*upgradedOwner;
    const auto upgradeError = upgraded.restoreProject(juce::JSON::toString(oldSave));
    bool migratedOwnership = oldError.isEmpty() && upgradeError.isEmpty() && upgraded.plugins[0].empty() && upgraded.project.clips.size() == 1 && upgraded.project.clips[0].effects->plugins.size() == 1;
    if (migratedOwnership)
    {
        upgraded.project.clips[0].moveTo(4.321); upgraded.project.clips[0].track = 2; upgraded.render(output, 512, 5.321, midi);
        migratedOwnership = near(output.getSample(0, 100), 0.125 * juce::Decibels::decibelsToGain(-6.0f));
    }
    check(migratedOwnership, "v10 lane inserts and automation migrate to audio tracks and follow later moves: " + upgradeError);
    ui.engine.project.clips.resize(1); ui.selectedClip = a.id; ui.engine.project.snap = false;
    const auto exported = report.getSiblingFile("owned-effects.wav");
    const auto exportError = ui.engine.exportWav(exported);
    std::unique_ptr<juce::AudioFormatReader> reader(ui.engine.formats.createReaderFor(exported));
    juce::AudioBuffer<float> exportedPcm(2, 256);
    if (reader) reader->read(&exportedPcm, 0, 256, (juce::int64) std::llround(8.123 * reader->sampleRate), true, true);
    check(exportError.isEmpty() && reader && near(exportedPcm.getSample(0, 100), 0.125), "offline export contains the moved track's automated VST PCM");
    ui.engine.selectedTrack = 3; ui.showInspectorPage(1); ui.timeline.pixelsPerSecond = 27.01; ui.refresh();
    auto image = report.getSiblingFile("owned-effects.png").createOutputStream(); juce::PNGImageFormat png;
    check(image && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *image), "capture the native track-owned automation and Effects rack");
    ui.selectedAutomationTarget = (int) AutomationTarget::plugin; ui.pluginLane = *parameter;
    ui.removeClip();
    check(ui.engine.project.clips.empty() && ui.displayedAutomationName() == "Choose a VST parameter", "deleting a track safely clears the displayed effect-automation owner");
    ui.undo(false);
    check(ui.engine.project.clips.size() == 1 && ui.engine.project.clips[0].effects->plugins.size() == 1, "Undo restores a deleted track with its effect chain retained");
    const juce::File instrument("C:/Program Files/Common Files/VST3/Vital.vst3");
    if (instrument.exists())
    {
        auto synthOwner=std::make_unique<Engine>(); auto& synth=*synthOwner; synth.project.clips = {a}; synth.project.clips[0].effects = makeClipEffects(); synth.selectedClip = a.id;
        const auto instrumentError = synth.addPlugin(0, instrument);
        juce::MidiBuffer notes; notes.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0); float peak = 0;
        if (instrumentError.isEmpty()) for (int block = 0; block < 32; ++block)
        { synth.render(output, 512, -1, notes); peak = std::max(peak, output.getMagnitude(0, 512)); notes.clear(); }
        check(instrumentError.isEmpty() && synth.plugins[0].empty() && peak > 0.0001f,
              "track-owned Vital still receives keyboard MIDI and produces audio with the transport stopped: " + instrumentError);
    }
}
}
