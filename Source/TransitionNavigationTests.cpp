#include "Workstation.h"
#include <cmath>
#include <thread>
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace mixworks
{
void runZoomHandleTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    Workstation ui(false); ui.stopTimer(); ui.setSize(1250,900);
    Project base; base.followTrackTempo = false; base.bpm = base.tempo.fallback = 120; base.snap = false;
    auto media = std::make_shared<Media>(); media->sampleRate = 8000; media->samples.setSize(1,48*8000); media->samples.clear(); media->manualBpm = 120; media->name = "Zoomed transition anchors";
    Clip clip; clip.media = media; clip.length = 48; clip.introAnchor = 4; clip.outroAnchor = 32;
    base.clips.push_back(clip); base.transitionLinksKnown = true;
    ui.engine.project = base; ui.selectedClip = clip.id; ui.timeline.pixelsPerSecond = 500; ui.refresh(); ui.viewport.setViewPosition(8000,0);
    const auto json = [&] { return juce::JSON::toString(projectToJson(ui.engine.project)); };
    const auto before = json();
    const auto in = ui.timeline.offscreenAnchorBounds(clip,true), out = ui.timeline.offscreenAnchorBounds(clip,false);
    check(!in.isEmpty() && !out.isEmpty() && in.getRight() < out.getX(),"zoomed view offers separate left IN and right OUT proxy tabs");
    check(json() == before && ui.undoHistory.empty(),"zooming and displaying off-screen handles do not change real anchors");
    const auto mouse = juce::Desktop::getInstance().getMainMouseSource();
    auto event = [&](juce::Point<float> at,juce::Point<float> down,bool drag)
    { return juce::MouseEvent(mouse,at,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::altModifier),1,0,0,0,0,&ui.timeline,&ui.timeline,juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),1,drag); };
    for (bool intro : {true,false})
    {
        ui.engine.project = base; ui.selectedClip = clip.id; ui.undoHistory.clear(); ui.redoHistory.clear(); ui.refresh(); ui.viewport.setViewPosition(8000,0); ui.engine.position = 11.125;
        const auto tab = ui.timeline.offscreenAnchorBounds(clip,intro); const auto down = tab.getCentre(), to = down.translated(intro ? 50.f : -50.f,0);
        const auto hit = ui.timeline.anchorAt(down); check(hit && hit->first == clip.id && hit->second == intro,"proxy hit targets the correct real IN/OUT anchor");
        ui.timeline.mouseDown(event(down,down,false)); check(json() == before,"grabbing the proxy does not jump its actual source anchor");
        ui.timeline.mouseDrag(event(to,down,true)); ui.timeline.mouseUp(event(to,down,true));
        const auto& changed = ui.engine.project.clips.front();
        check(std::abs(changed.transitionTime(intro)-(clip.transitionTime(intro)+(intro ? .1 : -.1))) < 1.e-7 && ui.undoHistory.size() == 1 && ui.engine.position == 11.125,
            "proxy drag adjusts actual anchor by pointer delta with one Undo and no seek");
        ui.undo(false); check(json() == before,"Undo restores the complete off-screen anchor edit");
    }
    ui.engine.project = base; ui.refresh(); ui.viewport.setViewPosition(0,0);
    const auto sameIn = ui.timeline.offscreenAnchorBounds(clip,true), sameOut = ui.timeline.offscreenAnchorBounds(clip,false);
    check(!sameIn.isEmpty() && !sameOut.isEmpty() && !sameIn.intersects(sameOut),"two anchors off the same edge keep distinct reachable tabs");
    ui.viewport.setViewPosition(8000,0);
    auto capture = report.getSiblingFile("zoomed-in-out-handles.png").createOutputStream();
    check(capture && juce::PNGImageFormat().writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()),*capture),"capture visible zoomed IN/OUT edge tabs");
}
void runTransitionNavigationTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    if (juce::JUCEApplicationBase::getCommandLineParameters().contains("--lane-automation-only"))
    {
        auto media = std::make_shared<Media>();
        media->sampleRate = 8000; media->manualBpm = 120;
        media->samples.setSize(1, 8000 * 48);
        for (int i = 0; i < media->frames(); ++i) media->samples.setSample(0, i, .08f * std::sin((float) i * .031f));
        Clip first; first.media = media; first.length = 48;
        Clip second = first; second.id = juce::Uuid().toString(); second.effects = makeClipEffects(); second.start = 32;
        Project base; base.masterDb = 0; base.clips = {first, second}; base.transitionLinksKnown = true;
        const AutomationAddress lane {0, AutomationTarget::volume};
        const AutomationAddress outgoing {0, AutomationTarget::volume, {}, {}, first.id};
        const AutomationAddress incoming {0, AutomationTarget::volume, {}, {}, second.id};
        *base.curve(outgoing) = {true, {{0, 0}, {32, 0}, {48, -60}}};
        *base.curve(incoming) = {true, {{0, -60}, {16, 0}, {48, 0}}};

        auto levelAt = [](Engine& engine, double time)
        {
            juce::AudioBuffer<float> audio(2, 128); juce::MidiBuffer midi;
            engine.render(audio, 128, time, midi);
            return audio.getRMSLevel(0, 0, 128);
        };
        auto render = [&](const Project& project)
        {
            auto engine = std::make_unique<Engine>();
            engine->sampleRate = 8000; engine->project = project;
            return levelAt(*engine, 40);
        };
        auto oldMixer = base;
        oldMixer.tracks[0].automation[0] = {true, {{0, -60}, {80, -60}}};
        check(!oldMixer.curve(lane) && render(base) > .001f && std::abs(render(oldMixer) - render(base)) < 1.e-7f,
            "stored mixer curves cannot play over the tracks' transition curves");
        oldMixer.tracks[0].gainDb = -6;
        check(std::abs(render(oldMixer) / render(base) - juce::Decibels::decibelsToGain(-6.f)) < 1.e-5f,
            "manual mixer trim still controls both tracks on the lane");

        auto writer = std::make_unique<Engine>();
        writer->sampleRate = 8000; writer->project = base; writer->armAutomation(0, true);
        writer->setAutomationRecordMode(AutomationRecordMode::touch);
        writer->selectedClip = first.id; writer->position = 38; writer->setPlaying(true);
        const auto beforeTouch = levelAt(*writer, 38);
        check(writer->setControlValue(lane, -12)
            && writer->project.curve(outgoing)->valueAt(38, 0) == -12
            && writer->project.curve(incoming)->points.size() == 3
            && writer->project.tracks[0].automation[0].points.empty() && writer->project.tracks[0].gainDb == 0,
            "Touch writes to the selected playing track's existing curve during an overlap");
        check(std::abs(levelAt(*writer, 38) - beforeTouch) > .0001f, "Touch changes the live mix while writing to the track curve");
        writer->position = 39; writer->advanceAutomation(39);
        check(!writer->recordingAutomation()
            && std::abs(writer->project.curve(outgoing)->valueAt(38.55, 0) - base.curve(outgoing)->valueAt(38.55, 0)) < .0001f,
            "Touch returns to the existing transition curve when the control is released");

        writer->setAutomationRecordMode(AutomationRecordMode::write);
        writer->selectedClip.clear(); writer->position = 42;
        const auto beforeLatch = levelAt(*writer, 42);
        check(writer->setControlValue(lane, -9) && writer->project.curve(incoming)->valueAt(10, 0) == -9,
            "Latch chooses the newest playing track when no playing track is selected");
        check(std::abs(levelAt(*writer, 42) - beforeLatch) > .0001f, "Latch changes the live mix while writing to the track curve");
        writer->position = 50; writer->advanceAutomation(50);
        check(writer->project.curve(incoming)->valueAt(18, 0) == -9 && writer->recordingAutomation(),
            "Latch keeps writing to that same track after the frame ends");
        writer->position = 82; writer->advanceAutomation(82);
        check(!writer->recordingAutomation() && writer->project.curve(incoming)->points.back().time == 48
            && !writer->setControlValue(lane, -24) && writer->project.tracks[0].gainDb == 0,
            "Latch stops at the track end and recording over empty space creates no mixer curve");

        writer->setPlaying(false); writer->setAutomationRecordMode(AutomationRecordMode::read);
        writer->selectedClip = second.id;
        check(writer->setControlValue(lane, -6) && writer->project.tracks[0].gainDb == -6
            && writer->project.curve(incoming)->active(), "Read-mode mixer controls stay manual while the track curve remains active");
        return;
    }
    if (juce::JUCEApplicationBase::getCommandLineParameters().contains("--alignment-guide-only"))
    {
        Workstation ui(false); ui.stopTimer(); ui.setSize(1250,800);
        auto media=std::make_shared<Media>(); media->sampleRate=8000; media->samples.setSize(1,8000*32); media->samples.clear();
        media->name="Alignment preview"; media->manualBpm=120;
        Clip clip; clip.media=media; clip.length=32; ui.engine.project.clips={clip};
        ui.engine.project.followTrackTempo=false; ui.engine.project.bpm=120; ui.engine.project.snap=true;
        ui.timeline.pixelsPerSecond=30; ui.refresh(); ui.engine.position=7.25; ui.engine.playing=true;
        const auto before=juce::JSON::toString(projectToJson(ui.engine.project));
        const auto mouse=juce::Desktop::getInstance().getMainMouseSource();
        const auto event=[&](float x,int flags,int clicks=1)
        {
            const juce::Point<float> at{x,8};
            return juce::MouseEvent(mouse,at,juce::ModifierKeys(flags),1,0,0,0,0,&ui.timelineRuler,&ui.timelineRuler,
                juce::Time::getCurrentTime(),at,juce::Time::getCurrentTime(),clicks,false);
        };
        const int left=juce::ModifierKeys::leftButtonModifier;
        auto down=event(123,left|juce::ModifierKeys::shiftModifier);
        ui.timelineRuler.mouseDown(down); ui.timelineRuler.mouseUp(down);
        check(ui.timelineRuler.alignmentGuide && *ui.timelineRuler.alignmentGuide==ui.engine.project.snappedTime(4.1,true),
            "Shift-click places an independent alignment guide using the project Snap setting");
        const float handle=(float)(*ui.timelineRuler.alignmentGuide*30);
        ui.timelineRuler.mouseDown(event(handle,left));
        auto free=event(157,left|juce::ModifierKeys::altModifier);
        ui.timelineRuler.mouseDrag(free); ui.timelineRuler.mouseUp(free);
        check(std::abs(*ui.timelineRuler.alignmentGuide-157./30)<1.e-8,"Alt-drag freely positions the guide between beats");
        ui.timelineRuler.mouseDoubleClick(event(157,left,2));
        check(ui.engine.playing && ui.engine.position==7.25 && before==juce::JSON::toString(projectToJson(ui.engine.project))
            && ui.undoHistory.empty(),"guide placement/drag/double-click neither seeks nor edits audio or history during playback");
        ui.timelineRuler.guideButton.setToggleState(false,juce::dontSendNotification); ui.timelineRuler.guideButton.onClick();
        check(!ui.timelineRuler.alignmentGuide && ui.engine.playing && ui.engine.position==7.25,"Guide toggle hides the view-only reference without changing playback");
        ui.timelineRuler.guideButton.setToggleState(true,juce::dontSendNotification); ui.timelineRuler.guideButton.onClick();
        check(ui.timelineRuler.alignmentGuide.has_value(),"Guide toggle restores a visible alignment reference");
        auto pixels=report.getSiblingFile("alignment-guide-playing.png").createOutputStream();
        check(pixels && juce::PNGImageFormat().writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()),*pixels),"capture distinct cyan guide and playback cursor");
        ui.engine.playing=false; return;
    }
    if (juce::JUCEApplicationBase::getCommandLineParameters().contains("--live-transition-only"))
    {
        // Bounded functional fixture: no device, detector, worker, timing or load test.
        Workstation ui(false); ui.stopTimer(); ui.setSize(1250,800); ui.engine.sampleRate = 8000;
        Project base; base.followTrackTempo = false; base.bpm = base.tempo.fallback = 120; base.masterDb = 0; base.transitionLinksKnown = true;
        for (int lane = 0; lane < 3; ++lane)
        {
            auto media = std::make_shared<Media>(); media->sampleRate = 8000; media->manualBpm = 120; media->manualBeatOrigin = 0;
            media->manualKey = 0; media->name = "Live transition " + juce::String(lane + 1); media->file = report.getSiblingFile("live-transition-" + juce::String(lane) + ".wav");
            media->samples.setSize(1,16*8000); media->peaks.assign(256,.15f); media->waveformRms.assign(256,.1f);
            for (int i = 0; i < media->frames(); ++i) media->samples.setSample(0,i,.15f*std::sin((float)i*(.031f+lane*.013f)));
            { juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream = media->file.createOutputStream();
              auto writer = wav.createWriterFor(stream,juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(1).withBitsPerSample(24));
              check(writer && writer->writeFromAudioSampleBuffer(media->samples,0,media->frames()),"write tiny owned live-transition source " + juce::String(lane)); }
            media->analysisSource={audioDigest(media->file),media->file.getSize(),8000,media->frames(),1};
            check(writeAnalysisRecord(media->file,media->analysisRecord()).isEmpty(),"seed owned live source analysis without detector work");
            Clip clip; clip.media = media; clip.track = lane; clip.start = lane == 1 ? 8 : 0; clip.length = 16; base.clips.push_back(clip);
        }
        const auto a = base.clips[0].id, b = base.clips[1].id;
        const auto json = [](const Project& p) { return juce::JSON::toString(projectToJson(p),juce::JSON::FormatOptions().withMaxDecimalPlaces(17)); };
        const auto hasVolumeNodes = [](const Project& p, size_t clip) { return !p.effectSettings(p.clips[clip]).automation[0].points.empty(); };
        auto reset = [&](bool playing, int preset)
        {
            ui.engine.finalisePerformance(); ui.engine.setWorkMode(WorkMode::arrange);
            ui.engine.setPlaying(false); ui.engine.project = base; ui.undoHistory.clear(); ui.redoHistory.clear();
            ui.selectedTransition.clear(); ui.selectedClip = b; ui.editSelection.clips = {a,b}; ui.editSelection.primary = b;
            ui.transitionIndex = 2; ui.transitionPreset.setSelectedId(preset,juce::dontSendNotification);
            ui.engine.seek(9); ui.engine.setPlaying(playing); ui.refresh();
        };
        auto capture = [&](const juce::String& name)
        { auto stream = report.getSiblingFile(name).createOutputStream(); check(stream && juce::PNGImageFormat().writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()),*stream),"capture actual native transition UI: " + name); };
        reset(true,1);
        check(!ui.canInsertArrangement(),"original song-insertion guard reproduces the playback exclusion (kept for song insertion)");
        check(ui.addTransitionButton.isEnabled(),"native Add Transition is enabled during normal prepared playback");
        const auto context = ui.timelineMenu(9,1);
        bool contextEnabled = false;
        for (juce::PopupMenu::MenuItemIterator item(context); item.next();)
            if (item.getItem().itemID == (int)TimelineAction::addTransition) contextEnabled = item.getItem().isEnabled;
        check(contextEnabled,"timeline context menu also enables Add Transition during playback");
        const double before = ui.engine.position.load(); ui.addTransitionButton.onClick();
        const auto* link = ui.engine.project.transitionFor(b);
        check(link && ui.engine.playing && ui.engine.position == before && ui.undoHistory.size() == 1,"native Add callback links the selected pair with one Undo and no transport jump: " + ui.status);
        if (!link) { capture("live-transition-rejected.png"); ui.engine.setPlaying(false); return; }
        check(link->outgoing == a && ui.engine.project.transitionRange(*link) == juce::Range<double>(8,16),"transition names the chosen pair and actual 8-second overlap");
        check(ui.engine.project.clips[2].id == base.clips[2].id && !hasVolumeNodes(ui.engine.project,2)
            && ui.engine.project.clips[0].start == 0 && ui.engine.project.clips[1].start == 8,"adding the pair preserves unrelated lane and track positions");
        juce::AudioBuffer<float> callback(2,128);
        ui.engine.audioDeviceIOCallbackWithContext(nullptr,0,callback.getArrayOfWritePointers(),2,128,{});
        check(ui.engine.playing && std::abs(ui.engine.position-(before+128./8000))<1.e-8 && callback.getMagnitude(0,128)>.001f,
            "actual device callback renders nonzero audio and continues the pre-edit sample clock");
        const double livePosition = ui.engine.position.load();
        ui.selectTransition(b,true); ui.navigateTransition(1);
        check(ui.engine.playing && ui.engine.position == livePosition,"focused selection and Next preserve playback position while running");
        ui.transitionPresetForSelection.setSelectedId(3,juce::dontSendNotification); ui.transitionPresetForSelection.onChange();
        check(ui.transitionBandModes[0].getSelectedId()!=(int)TransitionBandMode::none+1
            && ui.selectedAutomationTarget==0 && ui.transitionCurveShown[0],
            "Crossfade refreshes its visible volume summary and reveals the affected curves");
        const auto applied = ui.engine.project;
        const bool shaped = hasVolumeNodes(applied,0) && hasVolumeNodes(applied,1);
        check(shaped && ui.undoHistory.size() == 2 && ui.engine.position == livePosition && ui.engine.playing,"native Apply Crossfade creates audible ordinary nodes without seeking: " + ui.status);
        auto render = [&](const Project& p, int lane, double at)
        {
            Engine engine; engine.sampleRate = 8000; engine.project = p;
            for (int i = 0; i < trackCount; ++i) engine.project.tracks[(size_t)i].mute = lane != i;
            juce::AudioBuffer<float> audio(2,128); juce::MidiBuffer midi; engine.render(audio,128,at,midi); return audio;
        };
        auto relative = [&](int lane,double at) { const auto raw = render(base,lane,at), faded = render(applied,lane,at); return faded.getRMSLevel(0,0,128)/std::max(.00001f,raw.getRMSLevel(0,0,128)); };
        check(relative(0,9)>relative(0,15)+.4f && relative(1,15)>relative(1,9)+.4f,"rendered PCM independently confirms outgoing fades down and incoming fades up");
        const auto otherBefore = render(base,2,10), otherAfter = render(applied,2,10); float otherError = 0;
        for (int ch=0;ch<2;++ch) for(int i=0;i<128;++i) otherError=std::max(otherError,std::abs(otherBefore.getSample(ch,i)-otherAfter.getSample(ch,i)));
        check(otherError<1.e-7f,"unrelated lane PCM is unchanged by live preset placement");
        // Six ordinary callback blocks with view changes between them: functional
        // PCM/clock evidence, without a device, stress loop or timing benchmark.
        Engine oracle; oracle.sampleRate=8000; oracle.project=applied;
        const auto beforeView=json(ui.engine.project); const auto diagnostics=ui.engine.callbackDiagnostics.snapshot();
        bool stable=true; double expectedPosition=ui.engine.position.load();
        for(int step=0;step<6;++step)
        {
            ui.timeline.zoomAt(step%2==0 ? 2 : .5,ui.viewport.getViewPositionX()+60);
            ui.viewport.setViewPosition(step%2==0 ? 40 : 0,0);
            juce::AudioBuffer<float> actual(2,128), expected(2,128); juce::MidiBuffer midi;
            oracle.render(expected,128,expectedPosition,midi);
            float* outputs[]{actual.getWritePointer(0),actual.getWritePointer(1)};
            ui.engine.audioDeviceIOCallbackWithContext(nullptr,0,outputs,2,128,{});
            for(int ch=0;ch<2;++ch)for(int i=0;i<128;++i)stable &= std::abs(actual.getSample(ch,i)-expected.getSample(ch,i))<1.e-6f;
            expectedPosition+=128./8000;
            stable &= std::abs(ui.engine.position.load()-expectedPosition)<1.e-8;
        }
        check(stable && json(ui.engine.project)==beforeView && ui.engine.callbackDiagnostics.snapshot().lockMisses==diagnostics.lockMisses,
            "zoom/scroll preserve continuous sample-clock and captured callback PCM against independent renderer");
        ui.engine.seek(livePosition);
        capture("live-transition-playing.png");
        ui.undo(false); const bool presetUndone = !hasVolumeNodes(ui.engine.project,0); ui.undo(true);
        check(presetUndone && json(ui.engine.project)==json(applied) && ui.engine.playing && ui.engine.position==livePosition,"live preset Undo/Redo restores the exact project without changing transport");
        ui.undo(false); ui.undo(false); check(json(ui.engine.project)==json(base),"second Undo removes the newly added transition completely");
        ui.undo(true); ui.undo(true); ui.engine.setPlaying(false);
        ui.selectTransition(b,true); check(ui.engine.position==8,"explicit focused transition navigation still seeks while stopped");
        const auto file=report.getSiblingFile("live-transition.mixworks"); const auto save=ui.engine.saveProject(file);
        Engine reopened; const auto opened=save.isEmpty()?reopened.restoreProject(file.loadFileAsString()):save;
        const auto* reopenedLink = reopened.project.transitionFor(b);
        check(save.isEmpty() && opened.isEmpty() && reopenedLink && reopenedLink->presetName == "Crossfade"
            && reopenedLink->presetOwner == applied.transitionFor(b)->presetOwner,
            "save and actual source-backed reopen retain the placed preset and its ownership: " + opened);
        if(opened.isEmpty())
        {
            const auto restored=render(reopened.project,0,10), expected=render(applied,0,10); float error=0;
            for(int ch=0;ch<2;++ch) for(int i=0;i<128;++i) error=std::max(error,std::abs(restored.getSample(ch,i)-expected.getSample(ch,i)));
            check(error<2.e-6f,"reopened transition reproduces its rendered source PCM");
            juce::String errorText; auto exporter=reopened.createExportRenderer(errorText);
            check(exporter && errorText.isEmpty(),"normal offline export renderer admits the saved live transition: " + errorText);
            if(exporter) { exporter->sampleRate=8000; juce::AudioBuffer<float> audio(2,128); juce::MidiBuffer midi; exporter->render(audio,128,10,midi); check(audio.getMagnitude(0,128)>.001f,"offline export path renders the transition audio"); }
        }
        for(bool playing:{false,true})
        {
            reset(playing,3); ui.performTimelineAction(TimelineAction::addTransition,9,1);
            check(ui.engine.project.transitionFor(b) && hasVolumeNodes(ui.engine.project,0) && hasVolumeNodes(ui.engine.project,1)
                && ui.engine.playing==playing && ui.engine.position==9,"default Add Crossfade has the same result stopped/playing: " + juce::String((int)playing) + " / " + ui.status);
        }
        for(int condition=0;condition<5;++condition)
        {
            reset(true,1);
            if(condition==0)ui.busy=true; if(condition==1)ui.engine.exporting=true;
            if(condition==2)ui.pendingTempo=121; if(condition==3)ui.tempoPreparationPending=true;
            if(condition==4)ui.engine.automationRecordMode=AutomationRecordMode::write;
            const auto original=json(ui.engine.project); ui.refresh(); const bool disabled=!ui.addTransitionButton.isEnabled();
            ui.addSelectedTransition(a,b);
            check(disabled && json(ui.engine.project)==original && ui.undoHistory.empty() && ui.engine.playing && ui.engine.position==9,"live Add rechecks busy/export/preparation/write guard " + juce::String(condition));
            ui.busy=false;ui.engine.exporting=false;ui.pendingTempo=0;ui.tempoPreparationPending=false;ui.engine.automationRecordMode=AutomationRecordMode::read;
        }
        reset(true,1); ui.engine.project.clips.erase(ui.engine.project.clips.begin()); const auto removed=json(ui.engine.project);
        ui.addSelectedTransition(a,b); check(json(ui.engine.project)==removed && ui.undoHistory.empty() && ui.engine.playing,"late removed-target admission rejects without editing another lane or transport");
        reset(true,3);
        ui.engine.project.effectSettings(ui.engine.project.clips[0]).automation[0]={true,{{0,-3},{9,-6},{15,-9}}};
        ui.engine.project.tracks[0].automation[0]={true,{{0,-6},{24,-6}}}; ui.refresh();
        const auto manual=ui.engine.project; const auto manualJson=json(manual); const auto manualHistory=ui.undoHistory.size();
        const auto pump=[] {
            #if JUCE_WINDOWS
            MSG message; while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
            #endif
        };
        ui.addTransitionButton.onClick();
        auto* question=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        const auto* named = ui.engine.project.transitionFor(b);
        check(!question && named && named->presetName == "Crossfade" && ui.undoHistory.size() == manualHistory+1
            && ui.engine.playing && ui.engine.position==9,"Add places an owned Crossfade beside existing manual nodes while playback continues");
        if (question) { question->exitModalState(0); pump(); }
        const auto oldExterior=render(manual,0,2),newExterior=render(ui.engine.project,0,2); float exteriorError=0;
        for(int ch=0;ch<2;++ch)for(int i=0;i<128;++i)exteriorError=std::max(exteriorError,std::abs(oldExterior.getSample(ch,i)-newExterior.getSample(ch,i)));
        check(exteriorError<1.e-6f && ui.engine.project.tracks[0].automation[0].points.size()==manual.tracks[0].automation[0].points.size()
            && ui.engine.project.tracks[0].automation[0].points[0].value==-6,"replacement preserves exterior source PCM and independent mixer-lane automation");
        ui.undo(false); check(json(ui.engine.project)==manualJson,"Undo restores the exact manual-node project after preset placement");
        reset(true,1); ui.addTransitionButton.onClick();
        ui.engine.project.effectSettings(ui.engine.project.clips[0]).automation[1]={true,{{0,-3},{9,-6},{15,-9}}};
        ui.engine.project.effectSettings(ui.engine.project.clips[0]).automation[0]={true,{{0,-2},{10,-4},{16,-8}}};
        ui.transitionNodeOptionsFor.clear(); ui.refresh();
        const auto manualBands=json(ui.engine.project);
        const auto chooseBand=[&](int band,TransitionBandMode mode)
        { ui.transitionBandModes[(size_t)band].setSelectedId((int)mode+1,juce::dontSendNotification); ui.changeSelectedTransitionBand(band); };
        chooseBand(1,TransitionBandMode::crossfade);
        question=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        check(question && question->getDescription().containsIgnoreCase("bass") && json(ui.engine.project)==manualBands,
            "individual band replacement names the changed treatment before overwriting manual nodes");
        if(question) { question->exitModalState(0); pump(); }
        check(json(ui.engine.project)==manualBands && ui.transitionBandModes[1].getSelectedId()==(int)TransitionBandMode::manual+1,
            "band cancellation keeps manual nodes and restores the actual summary");
        chooseBand(1,TransitionBandMode::crossfade); question=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        if(question) { question->exitModalState(1); pump(); }
        check(ui.engine.playing && ui.engine.position==9 && json(ui.engine.project)!=manualBands,
            "confirmed individual-band replacement preserves programme playback");
        const auto bandHistory=ui.undoHistory.size();
        ui.transitionBandStrengths[1].setSelectedId(50,juce::dontSendNotification); ui.changeSelectedTransitionBand(1);
        check(!juce::Component::getCurrentlyModalComponent() && ui.undoHistory.size()==bandHistory+1,
            "adjusting the same unchanged generated treatment does not repeatedly interrupt with questions");
        const auto beforeOtherBand=json(ui.engine.project); chooseBand(0,TransitionBandMode::crossfade);
        question=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        check(question && json(ui.engine.project)==beforeOtherBand,"a previously accepted bass edit does not authorise replacing manual volume nodes");
        if(question) { question->exitModalState(0); pump(); }
        check(ui.insertAutomationNode({0,AutomationTarget::bass,{},{},a},10.5,false),"insert a real manual node after generating a band treatment");
        ui.selectTransition(b,false); const auto manuallyEdited=json(ui.engine.project); chooseBand(1,TransitionBandMode::swap);
        question=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        check(question && json(ui.engine.project)==manuallyEdited,"intervening manual edits invalidate direct treatment adjustment");
        if(question) { question->exitModalState(0); pump(); }
        reset(true,1); ui.addTransitionButton.onClick();
        ui.engine.project.effectSettings(ui.engine.project.clips[0]).automation[(size_t)AutomationTarget::cutoff]={true,{{8,-.2f},{12,.3f},{16,.6f}}};
        ui.transitionNodeOptionsFor.clear(); ui.refresh(); const auto manualFilter=json(ui.engine.project);
        check(ui.transitionFilterMode.getText()=="Existing / custom","manual filter nodes are described as existing rather than None");
        ui.transitionFilterMode.setSelectedId((int)TransitionFilterMode::highToLow+1,juce::dontSendNotification); ui.changeSelectedTransitionFilter();
        question=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        check(question && json(ui.engine.project)==manualFilter,"filter replacement protects manual cutoff nodes with the same range question / " + ui.status);
        ui.engine.exporting=true;
        if(question) { question->exitModalState(1); pump(); }
        check(json(ui.engine.project)==manualFilter && ui.status.contains("unavailable"),"export starting during the question refuses a stale filter publication");
        ui.engine.exporting=false;
        for(bool playing:{false,true})
        {
            reset(playing,3); const auto mode=ui.engine.setWorkMode(WorkMode::recordPerform); ui.refresh();
            check(mode.wasOk() && ui.addTransitionButton.isEnabled(),"Record/Perform default recording mode admits Add Transition / "+juce::String((int)playing));
            if(playing)
            {
                check(ui.engine.beginPerformance().wasOk(),"start actual editable take before live transition placement");
                TakeEvent marker; marker.kind=TakeEventKind::control; marker.lane=2; marker.control=(int)AutomationTarget::volume;
                marker.value=0; marker.sourcePosition=ui.engine.position; ui.engine.appendTakeEvent(std::move(marker));
            }
            const auto at=ui.engine.position.load(); ui.addTransitionButton.onClick();
            check(ui.engine.project.transitionFor(b) && hasVolumeNodes(ui.engine.project,0) && ui.engine.position==at && ui.engine.playing==playing,
                "native Record/Perform Add creates audible preset without stopping/reseeking / "+juce::String((int)playing)+" / "+ui.status);
            if(playing)
            {
                const auto& take=ui.engine.project.performanceTakes[0];
                check(take.events.size()>=2 && take.events[0].kind==TakeEventKind::control
                    && std::any_of(take.events.begin(),take.events.end(),[](const auto& e){return e.kind==TakeEventKind::project;}),
                    "live publication preserves existing take chronology and records the transition checkpoint");
                juce::AudioBuffer<float> heard(2,128); float* output[]{heard.getWritePointer(0),heard.getWritePointer(1)};
                ui.engine.audioDeviceIOCallbackWithContext(nullptr,0,output,2,128,{});
                const auto undoAt=ui.engine.position.load();
                ui.engine.setControlValue({2,AutomationTarget::volume},-3); ui.undo(false);
                check(ui.engine.capturingPerformance() && ui.engine.playing && ui.engine.position==undoAt && !ui.engine.project.transitionFor(b)
                    && ui.engine.project.tracks[2].gainDb==-3,"live Undo returns the structural edit while preserving clock, active take and later fader movement");
                const auto eventCount=ui.engine.project.performanceTakes[0].events.size(); ui.undo(false);
                check(ui.engine.capturingPerformance() && ui.engine.project.performanceTakes.size()==1
                    && ui.engine.project.performanceTakes[0].events.size()==eventCount && ui.status.contains("No live structural"),
                    "live Undo cannot fall through to a pre-take snapshot or erase active chronology");
                juce::AudioBuffer<float> undone(2,128); float* undoneOutput[]{undone.getWritePointer(0),undone.getWritePointer(1)};
                ui.engine.audioDeviceIOCallbackWithContext(nullptr,0,undoneOutput,2,128,{});
                Engine plain; plain.sampleRate=8000; plain.project=base; plain.project.tracks[2].gainDb=-3;
                juce::AudioBuffer<float> expectedUndo(2,128); juce::MidiBuffer midi; plain.render(expectedUndo,128,undoAt,midi); float undoError=0;
                for(int ch=0;ch<2;++ch)for(int i=0;i<128;++i)undoError=std::max(undoError,std::abs(undone.getSample(ch,i)-expectedUndo.getSample(ch,i)));
                check(undoError<1.e-6f,"live Undo produces the independent original-source PCM with the subsequent fader value");
                const auto redoAt=ui.engine.position.load(); ui.undo(true);
                check(ui.engine.project.transitionFor(b) && ui.engine.position==redoAt && ui.engine.playing && ui.engine.project.tracks[2].gainDb==-3,
                    "live Redo reapplies the treatment at current time without rewinding the performance");
                juce::AudioBuffer<float> redone(2,128); float* redoneOutput[]{redone.getWritePointer(0),redone.getWritePointer(1)};
                ui.engine.audioDeviceIOCallbackWithContext(nullptr,0,redoneOutput,2,128,{});
                ui.engine.finalisePerformance(); juce::String replayError; auto replay=ui.engine.prepareTakeReplay(0,replayError);
                check(replay && replayError.isEmpty(),"editable take admits transition replay: "+replayError);
                if(replay)
                { juce::AudioBuffer<float> audio(2,128); replay->renderTake(audio,128,0); float difference=0;
                  for(int ch=0;ch<2;++ch)for(int i=0;i<128;++i)difference=std::max(difference,std::abs(audio.getSample(ch,i)-heard.getSample(ch,i)));
                  check(difference<2.e-6f && heard.getMagnitude(0,128)>.001f,"recorded transition replay matches actual callback PCM");
                  replay->renderTake(audio,128,128./8000); float undoReplay=0;
                  for(int ch=0;ch<2;++ch)for(int i=0;i<128;++i)undoReplay=std::max(undoReplay,std::abs(audio.getSample(ch,i)-undone.getSample(ch,i)));
                  replay->renderTake(audio,128,256./8000); float redoReplay=0;
                  for(int ch=0;ch<2;++ch)for(int i=0;i<128;++i)redoReplay=std::max(redoReplay,std::abs(audio.getSample(ch,i)-redone.getSample(ch,i)));
                  check(undoReplay<2.e-6f && redoReplay<2.e-6f,"take replay preserves earlier sound then performs live Undo and Redo at their recorded times"); }
            }
        }
        auto oldTreatment=base,cleanCrossfade=base;
        const bool compileOld=oldTreatment.addTransition(a,b,0,TransitionPreset::bassSwap).wasOk()
            && cleanCrossfade.addTransition(a,b,0,TransitionPreset::crossfade).wasOk();
        oldTreatment.effectSettings(oldTreatment.clips[0]).automation[0].setPoint(10,-6);
        oldTreatment.effectSettings(oldTreatment.clips[0]).automation[0].enabled=true;
        const auto replaceResult=oldTreatment.applyTransitionPreset(b,TransitionPreset::crossfade);const bool replaced=replaceResult.wasOk();float replaceError=0;
        for(int lane:{0,1})for(double time:{10.,14.})
        {const auto wanted=render(cleanCrossfade,lane,time),heard=render(oldTreatment,lane,time);
            for(int ch=0;ch<2;++ch)for(int i=0;i<128;++i)replaceError=std::max(replaceError,std::abs(wanted.getSample(ch,i)-heard.getSample(ch,i)));}
        check(compileOld && replaced && replaceError<1.e-7f,
            "whole-preset replacement removes prior active bass/manual-volume curves and matches a fresh Crossfade's actual PCM / "+replaceResult.getErrorMessage()+" / error "+juce::String(replaceError,9));
        const auto once=oldTreatment;oldTreatment.applyTransitionPreset(b,TransitionPreset::crossfade);float repeatError=0;
        for(int lane:{0,1}) {const auto wanted=render(once,lane,14),heard=render(oldTreatment,lane,14);
            for(int ch=0;ch<2;++ch)for(int i=0;i<128;++i)repeatError=std::max(repeatError,std::abs(wanted.getSample(ch,i)-heard.getSample(ch,i)));}
        check(repeatError<1.e-7f,"reapplying the whole Crossfade cannot multiply its audible fade a second time");
        reset(true,1);ui.engine.project.clips[2].start=12;
        const auto c=ui.engine.project.clips[2].id;
        const bool chained=ui.engine.project.addTransition(a,b,0,TransitionPreset::crossfade).wasOk()
            && ui.engine.project.addTransition(b,c,0,TransitionPreset::crossfade).wasOk();
        check(chained,"prepare two real transition frames sharing the middle track and a four-second overlap");
        ui.selectTransition(b,false);const auto sharedBefore=ui.engine.project;
        ui.changeSelectedTransition({},TransitionPreset::bassSwap);question=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        const auto* bass = ui.engine.project.transitionFor(b);
        check(!question && bass && bass->presetName == "Bass swap" && ui.engine.playing && ui.engine.position==9,
            "applying Bass swap beside an overlapping pair preserves the programme clock");
        if (question) { question->exitModalState(0); pump(); }
        const auto outsideBefore=render(sharedBefore,1,18),outsideAfter=render(ui.engine.project,1,18);
        const auto partnerBefore=render(sharedBefore,2,14),partnerAfter=render(ui.engine.project,2,14);float sharedError=0;
        for(int ch=0;ch<2;++ch)for(int i=0;i<128;++i)sharedError=std::max({sharedError,std::abs(outsideBefore.getSample(ch,i)-outsideAfter.getSample(ch,i)),std::abs(partnerBefore.getSample(ch,i)-partnerAfter.getSample(ch,i))});
        check(chained && sharedError<1.e-7f && ui.engine.playing && ui.engine.position==9 && ui.engine.project.transitionFor(c),
            "applying a preset preserves exterior middle-track PCM and the other partner's actual audio");
        reset(true,1);
        const AutomationAddress nodeAddress{0,AutomationTarget::volume,{},{},a};
        ui.timeline.selectAutomationCurve(nodeAddress); ui.refresh();
        const auto at=ui.engine.position.load(); const auto misses=ui.engine.callbackDiagnostics.snapshot().lockMisses;
        juce::AudioBuffer<float> blocked(2,128); blocked.clear();
        {
            const juce::ScopedLock deliberatelyHeld(ui.engine.lock);
            std::thread callback([&] { ui.engine.audioDeviceIOCallbackWithContext(nullptr,0,blocked.getArrayOfWritePointers(),2,128,{}); }); callback.join();
        }
        check(blocked.getMagnitude(0,128)==0 && ui.engine.position==at && ui.engine.callbackDiagnostics.snapshot().lockMisses==misses+1,
            "bounded contention control captures actual silent output and held programme clock, not just a counter");
        struct RefreshCallback final : juce::ComponentListener
        {
            std::function<void()> call;
            void componentEnablementChanged(juce::Component& c) override { if(c.isEnabled() && call)call(); }
        } refreshCallback;
        int refreshCallbacks=0; bool refreshAudio=true; juce::Array<juce::var> callbackFacts;
        auto* controlFact=new juce::DynamicObject(); controlFact->setProperty("kind","held-lock-control");
        controlFact->setProperty("peak",blocked.getMagnitude(0,128)); controlFact->setProperty("positionBefore",at);
        controlFact->setProperty("positionAfter",ui.engine.position.load()); callbackFacts.add(juce::var(controlFact));
        refreshCallback.call=[&]
        {
            ++refreshCallbacks;
            const auto position=ui.engine.position.load(); const auto beforeMisses=ui.engine.callbackDiagnostics.snapshot().lockMisses;
            std::thread callback([&]
            {
                Engine oracle; oracle.sampleRate=8000; oracle.project=ui.engine.project;
                juce::AudioBuffer<float> expected(2,128),actual(2,128); juce::MidiBuffer midi;
                oracle.render(expected,128,position,midi);
                ui.engine.audioDeviceIOCallbackWithContext(nullptr,0,actual.getArrayOfWritePointers(),2,128,{});
                float difference=0; for(int ch=0;ch<2;++ch)for(int i=0;i<128;++i)
                    difference=std::max(difference,std::abs(expected.getSample(ch,i)-actual.getSample(ch,i)));
                refreshAudio &= actual.getMagnitude(0,128)>.001f && difference<1.e-6f
                    && std::abs(ui.engine.position-(position+128./8000))<1.e-8 && ui.engine.callbackDiagnostics.snapshot().lockMisses==beforeMisses;
                auto* fact=new juce::DynamicObject(); fact->setProperty("kind",refreshCallbacks==1 ? "insert-refresh" : "delete-refresh");
                fact->setProperty("peak",actual.getMagnitude(0,128)); fact->setProperty("maxPcmError",difference);
                fact->setProperty("positionBefore",position); fact->setProperty("positionAfter",ui.engine.position.load());
                fact->setProperty("lockMissesBefore",(juce::int64)beforeMisses); fact->setProperty("lockMissesAfter",(juce::int64)ui.engine.callbackDiagnostics.snapshot().lockMisses);
                callbackFacts.add(juce::var(fact));
            }); callback.join();
        };
        ui.automationAdd.addComponentListener(&refreshCallback);
        ui.automationAdd.setEnabled(false);
        check(ui.insertAutomationNode(nodeAddress,10.5,false),"insert an actual source node through the ordinary editor during playback");
        check(refreshCallbacks==1 && refreshAudio,"node insertion refresh permits real callback PCM and uninterrupted sample-clock progress");
        ui.automationAdd.setEnabled(false);
        check(ui.deleteAutomationNode(nodeAddress,0),"delete the source node through the ordinary editor during playback");
        check(refreshCallbacks==2 && refreshAudio,"node deletion refresh permits real callback PCM and uninterrupted sample-clock progress");
        ui.automationAdd.removeComponentListener(&refreshCallback);
        check(report.getSiblingFile("automation-refresh-callbacks.json").replaceWithText(juce::JSON::toString(juce::var(callbackFacts),true,17)),
            "retain independent callback PCM error, peak, clock and contention observations");
        ui.engine.finalisePerformance(); ui.engine.setPlaying(false); return;
    }
    const auto json = [](const Project& p) { return juce::JSON::toString(projectToJson(p), juce::JSON::FormatOptions().withMaxDecimalPlaces(17)); };
    Project base; base.followTrackTempo = false; base.bpm = base.tempo.fallback = 120;
    for (int n = 0; n < 3; ++n)
    {
        auto media = std::make_shared<Media>(); media->sampleRate = 8000; media->samples.setSize(1, 12 * 8000); media->samples.clear();
        media->name = "Navigation source " + juce::String(n + 1); media->manualBpm = 120; media->manualBeatOrigin = 0;
        media->manualKey = n == 0 ? 11 : n == 1 ? 6 : -1;
        Clip clip; clip.media = media; clip.start = n * 8; clip.track = n; clip.length = 12;
        clip.introAnchor = clip.fadeInAnchor = 0; clip.outroAnchor = 8; clip.fadeOutAnchor = 9; clip.fadeOut = 1;
        base.clips.push_back(clip);
    }
    base.transitionLinksKnown = true;
    base.transitionLinks = {{base.clips[0].id, base.clips[1].id}, {base.clips[1].id, base.clips[2].id}};
    Workstation ui(false); ui.stopTimer(); ui.setSize(1440, 940); ui.engine.project = base;
    const int alt = juce::ModifierKeys::altModifier, ctrl = juce::ModifierKeys::ctrlModifier, shift = juce::ModifierKeys::shiftModifier;
    const auto key = [&](int code, int mods) { return ui.keyPressed(juce::KeyPress(code, juce::ModifierKeys(mods), 0)); };
    check(base.validateTransitionLinks().wasOk(), "keyboard transition fixture has interior editable frames");
    const auto id = base.clips[1].id; ui.selectTransition(id);
    check(ui.selectedTransitionLength.getTooltip().contains("Wheel +1") && ui.selectedTransitionLength.getTooltip().contains("same mode"), "inspector derives source key movement using the existing wheel facts");
    ui.engine.project.clips[0].media->manualKey = 4; ui.engine.project.clips[1].media->manualKey = 11; ui.refresh();
    check(ui.selectedTransitionLength.getTooltip().contains("Wheel +1"), "wheel movement wraps from twelve to one without reporting eleven steps");
    ui.engine.project.clips[0].media->manualKey = 11;
    ui.engine.project.clips[1].media->manualKey = 20; ui.refresh();
    check(ui.selectedTransitionLength.getTooltip().contains("Relative major / minor"), "same wheel with another mode is described as relative keys");
    ui.engine.project.clips[1].media->manualKey = -1; ui.engine.project.clips[1].media->analysis.key = 6;
    ui.engine.project.clips[1].media->analysis.keyConfidence = .3; ui.refresh();
    check(ui.selectedTransitionLength.getTooltip().contains("In: " + ui.keyText(*ui.engine.project.clips[1].media)) && !ui.selectedTransitionLength.getTooltip().contains("?") && ui.selectedTransitionLength.getTooltip().contains("Movement unknown"), "uncertain key stays visibly uncertain without a derived movement");
    ui.engine.project.clips[1].media->analysis.keyConfidence = .55; ui.refresh();
    check(ui.selectedTransitionLength.getTooltip().contains("Wheel +1") && !ui.selectedTransitionLength.getTooltip().contains("?"), "confirmed key and key formatting share the same owning confidence boundary");
    ui.engine.project.clips[1].media->analysis.key = -1; ui.refresh();
    check(ui.selectedTransitionLength.getTooltip().contains("Unknown") && ui.selectedTransitionLength.getTooltip().contains("Movement unknown"), "unavailable key is retained as unknown");
    ui.engine.project.clips[1].media->manualKey = 6; ui.engine.project.clips[1].media->analysis.keyConfidence = 0; ui.refresh();
    const auto initial = json(ui.engine.project); const auto undos = ui.undoHistory.size();
    check(key(juce::KeyPress::rightKey, alt) && ui.selectedTransition == base.clips[2].id && ui.undoHistory.size() == undos, "Alt Right navigates by transition identity without Undo");
    check(key(juce::KeyPress::rightKey, alt) && ui.selectedTransition == base.clips[2].id, "keyboard Next remains bounded at the last pair");
    check(key(juce::KeyPress::leftKey, alt) && ui.selectedTransition == id && json(ui.engine.project) == initial, "Alt Left restores the previous selection without editing the session");
    auto emptyFrame = base; emptyFrame.clips[0].fadeOutAnchor = 8; emptyFrame.clips[0].fadeOut = 0;
    ui.engine.project = emptyFrame; ui.selectTransition(id); const auto emptyBefore = json(ui.engine.project);
    key(juce::KeyPress::leftKey, shift | alt);
    check(json(ui.engine.project) == emptyBefore && ui.undoHistory.size() == undos, "keyboard resize before the frame start rejects without an Undo");
    ui.engine.project = base; ui.selectTransition(id);
    ui.busy = true;
    check(!key(juce::KeyPress::rightKey, ctrl | alt) && json(ui.engine.project) == initial, "busy workflow leaves keyboard geometry untouched"); ui.busy = false;
    ui.engine.position = 6.125; const auto before = ui.engine.project.transitionRange(*ui.engine.project.transitionFor(id));
    check(key(juce::KeyPress::rightKey, ctrl | alt), "Ctrl Alt Right handles frame movement");
    const auto moved = ui.engine.project.transitionRange(*ui.engine.project.transitionFor(id));
    check(std::abs(moved.getStart() - before.getStart() - .5) < 1e-7 && std::abs(moved.getLength() - before.getLength()) < 1e-7
        && ui.undoHistory.size() == undos + 1 && ui.engine.position == 6.125, "one-beat keyboard move keeps frame length, transport and one Undo");
    check(ui.engine.project.clips[0].start == base.clips[0].start && ui.engine.project.clips[1].start == base.clips[1].start + .5
        && ui.engine.project.clips[2].start == base.clips[2].start + .5 && ui.engine.project.clips[0].effects == base.clips[0].effects
        && ui.engine.project.clips[1].introAnchor == base.clips[1].introAnchor,
        "keyboard frame edit carries the aligned incoming chain and retains source IN and effect ownership");
    const auto edited = json(ui.engine.project); ui.undo(false); const bool undone = json(ui.engine.project) == initial; ui.undo(true);
    check(undone && json(ui.engine.project) == edited, "keyboard frame movement round-trips through native Undo/Redo");
    check(key(juce::KeyPress::leftKey, shift | alt), "Shift Alt Left handles end resize");
    const auto resized = ui.engine.project.transitionRange(*ui.engine.project.transitionFor(id));
    check(std::abs(resized.getStart() - moved.getStart()) < 1e-7 && std::abs(resized.getEnd() - moved.getEnd() + .5) < 1e-7, "keyboard resize keeps start fixed and changes the end by a beat");
    key('L', alt); const auto locked = json(ui.engine.project); const auto lockedUndo = ui.undoHistory.size();
    check(ui.engine.project.transitionFor(id)->locked && !ui.transitionFilterMode.isEnabled(), "Alt L locks the pair and refreshes protected controls");
    key(juce::KeyPress::leftKey, ctrl | alt); key(juce::KeyPress::rightKey, shift | alt);
    check(json(ui.engine.project) == locked && ui.undoHistory.size() == lockedUndo, "keyboard edits cannot bypass a transition lock");
    key(juce::KeyPress::rightKey, alt); key(juce::KeyPress::rightKey, ctrl | alt);
    check(json(ui.engine.project) == locked && ui.undoHistory.size() == lockedUndo, "keyboard geometry on an unlocked neighbour cannot change a protected track");
    key(juce::KeyPress::leftKey, alt);
    key('L', alt); check(!ui.engine.project.transitionFor(id)->locked, "Alt L unlocks the pair");
    ui.pendingTempo = 121; const auto pending = json(ui.engine.project); const auto pendingUndo = ui.undoHistory.size();
    key(juce::KeyPress::rightKey, ctrl | alt);
    check(json(ui.engine.project) == pending && ui.undoHistory.size() == pendingUndo, "pending tempo preparation rejects keyboard geometry without history"); ui.pendingTempo = 0;
    check(!key(juce::KeyPress::rightKey, ctrl | alt | shift) && !key(juce::KeyPress::rightKey, 0), "conflicting modifiers and ordinary arrows are left to focused controls");
    ui.engine.project = base; ui.engine.project.followTrackTempo = true; ui.engine.project.tempo.points = {{0, 100, true}, {28, 160, true}}; ui.refresh();
    const auto clock = ui.engine.project.tempo; const auto rampBefore = ui.engine.project.transitionRange(*ui.engine.project.transitionFor(id));
    key(juce::KeyPress::rightKey, ctrl | alt); const auto rampAfter = ui.engine.project.transitionRange(*ui.engine.project.transitionFor(id));
    check(std::abs(clock.beatsAt(rampAfter.getStart()) - clock.beatsAt(rampBefore.getStart()) - 1) < 1e-7, "keyboard movement integrates one beat through a tempo ramp");
    // Canvas editing must not invoke the explicit transition navigation seek.
    const auto mouse = juce::Desktop::getInstance().getMainMouseSource();
    auto event = [&](juce::Point<float> position, juce::Point<float> origin, bool drag)
    { return juce::MouseEvent(mouse, position, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1, 0, 0, 0, 0, &ui.timeline, &ui.timeline, juce::Time::getCurrentTime(), origin, juce::Time::getCurrentTime(), 1, drag); };
    for (bool playing : {false, true}) for (bool alreadySelected : {false, true})
        for (auto edit : {Project::TransitionEdit::resizeStart, Project::TransitionEdit::resizeEnd, Project::TransitionEdit::moveWindow})
        {
            ui.engine.setPlaying(false); ui.engine.project = base; ui.engine.project.snap = false;
            ui.selectedTransition = alreadySelected ? id : base.clips[2].id; ui.selectedClip = id;
            ui.undoHistory.clear(); ui.redoHistory.clear(); ui.timeline.pixelsPerSecond = 30; ui.refresh();
            ui.viewport.setViewPosition(16, 0); ui.engine.seek(6.125); ui.engine.setPlaying(playing);
            const auto view = ui.viewport.getViewPosition(); const auto box = ui.timeline.transitionBounds(base.transitionLinks[0]);
            const juce::Point<float> from {edit == Project::TransitionEdit::resizeStart ? box.getX() : edit == Project::TransitionEdit::resizeEnd ? box.getRight() : box.getCentreX(), box.getY() + 8};
            const auto hit = ui.timeline.transitionAt(from);
            check(hit && hit->first == id && hit->second == edit, "transport fixture hits the actual transition width/header control");
            const auto original = json(ui.engine.project);
            ui.timeline.mouseDown(event(from, from, false));
            check(ui.selectedTransition == id && ui.engine.position == 6.125 && ui.engine.playing == playing && ui.viewport.getViewPosition() == view
                && ui.undoHistory.empty() && json(ui.engine.project) == original,
                "grabbing a transition width/header preserves playhead, playback, view and history: " + juce::String((int) edit) + "/" + juce::String((int) playing) + "/" + juce::String((int) alreadySelected));
            const auto to = from.translated(edit == Project::TransitionEdit::resizeEnd ? -15.f : 15.f, 0);
            ui.timeline.mouseDrag(event(to, from, true)); ui.timeline.mouseUp(event(to, from, true));
            const auto range = ui.engine.project.transitionRange(*ui.engine.project.transitionFor(id));
            check(std::abs(range.getStart() - (edit == Project::TransitionEdit::resizeEnd ? 8 : 8.5)) < 1e-7
                && std::abs(range.getEnd() - (edit == Project::TransitionEdit::resizeStart ? 10 : edit == Project::TransitionEdit::resizeEnd ? 9.5 : 10.5)) < 1e-7
                && ui.engine.position == 6.125 && ui.engine.playing == playing && ui.undoHistory.size() == 1,
                "native width/header drag changes only its geometry with one Undo and no transport jump");
            juce::AudioBuffer<float> output(2, 256);
            ui.engine.audioDeviceIOCallbackWithContext(nullptr, 0, output.getArrayOfWritePointers(), 2, 256, {});
            check(std::abs(ui.engine.position - (6.125 + (playing ? 256 / ui.engine.sampleRate.load() : 0))) < 1e-7,
                "next device callback continues from the pre-grab playback clock");
            ui.engine.setPlaying(false); const auto editedGeometry = json(ui.engine.project);
            ui.undo(false); const bool restored = json(ui.engine.project) == original; ui.undo(true);
            check(restored && json(ui.engine.project) == editedGeometry, "transition handle editing retains complete Undo/Redo");
        }
    ui.engine.setPlaying(false); ui.engine.project = base; ui.timeline.pixelsPerSecond = 30; ui.selectTransition(id, true);
    check(ui.engine.position == 8, "explicit transition navigation still seeks to its start");
    const auto beforeEditor = json(ui.engine.project);
    const auto page = ui.toolPage;
    check(ui.timeline.keyPressed(juce::KeyPress(juce::KeyPress::F10Key, juce::ModifierKeys::shiftModifier, 0))
        && juce::PopupMenu::dismissAllActiveMenus() && ui.toolPage == page && ui.engine.position == 8
        && json(ui.engine.project) == beforeEditor, "Shift+F10 opens the selected frame menu without opening its dock or moving the playhead");
    ui.openTransitionEditor(id);
    check(ui.transitionPanel.isVisible() && ui.engine.position == 8 && json(ui.engine.project) == beforeEditor,
        "opening the selected pair editor preserves the playhead and document");
    auto image = report.getSiblingFile("transition-width-transport.png").createOutputStream(); juce::PNGImageFormat png;
    check(image && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *image), "capture transition width controls after transport-preserving editing");
    ui.engine.project = base; ui.selectTransition(id); ui.setSize(1000, 620);
    check(ui.selectedTransitionLength.getWidth() >= 100 && ui.transitionPresetForSelection.getWidth() >= 120,
        "compact transition panel keeps primary edits reachable without repeated track information");
    check(ui.previousTransition.getTooltip().contains("Ctrl + Alt"), "transition keyboard help remains available on navigation tooltips");

}
}
