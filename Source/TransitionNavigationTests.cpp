#include "Workstation.h"
#include <cmath>

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
    const auto json=[](const Project& p) { return juce::JSON::toString(projectToJson(p),juce::JSON::FormatOptions().withMaxDecimalPlaces(17)); };
    Project base; base.followTrackTempo=false; base.transitionLinksKnown=true; base.bpm=base.tempo.fallback=120; base.masterDb=0;
    auto media=std::make_shared<Media>(); media->sampleRate=8000; media->manualBpm=120; media->manualBeatOrigin=0;
    media->name="Lane curve preview"; media->file=report.getSiblingFile("lane-curve-source.wav");
    media->samples.setSize(1,8000*64); media->peaks.assign(256,.1f); media->waveformRms.assign(256,.06f);
    for(int i=0;i<media->frames();++i)media->samples.setSample(0,i,.08f*std::sin((float)i*.031f));
    { juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream=media->file.createOutputStream();
      auto writer=wav.createWriterFor(stream,juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(1).withBitsPerSample(24));
      check(writer && writer->writeFromAudioSampleBuffer(media->samples,0,media->frames()),"write bounded original source for actual save/reopen and take replay"); }
    Clip first; first.media=media; first.length=48; base.clips.push_back(first);
    const AutomationAddress lane{0,AutomationTarget::volume};
    auto times=[](const AutomationCurve& curve) { std::vector<double> result; for(const auto& p:curve.points)result.push_back(p.time);return result; };
    base.updateLaneHandles();
    check(times(*base.curve(lane))==std::vector<double>({0,16,32,48}),"long track receives start / eight bars / last eight bars / end handles");
    bool neutral=true; for(const auto& curve:base.tracks[0].automation)for(const auto& p:curve.points)neutral=neutral && p.value==0;
    check(neutral && base.tracks[1].automation[0].points.empty(),"all six parameters begin neutral without nodes on empty lanes");
    const auto stable=json(base); base.updateLaneHandles(); check(json(base)==stable,"repeated refresh is idempotent");
    auto shorter=base; shorter.clips[0].length=8; shorter.updateLaneHandles();
    check(times(*shorter.curve(lane))==std::vector<double>({0,4,8}),"short tracks merge inner handles at the beat midpoint");
    auto overlap=base; Clip second=first; second.id=juce::Uuid().toString(); second.effects=makeClipEffects(); second.start=32; overlap.clips.push_back(second); overlap.updateLaneHandles();
    check(times(*overlap.curve(lane))==std::vector<double>({0,16,32,48,64,80}),"same-lane overlap shares one curve and merges coincident handles");
    auto moved=base; moved.clips[0].start=5; moved.updateLaneHandles();
    check(times(*moved.curve(lane))==std::vector<double>({5,21,37,53}),"untouched handles follow a move without leaving old nodes");
    moved.clips.clear(); moved.updateLaneHandles(); check(moved.curve(lane)->points.empty(),"deleting the last untouched track removes only its scaffolding");
    auto faster=base; faster.bpm=faster.tempo.fallback=240; faster.updateLaneHandles();
    check(times(*faster.curve(lane))==std::vector<double>({0,8,40,48}),"eight-bar handles use the current project tempo");
    auto split=base; split.clips[0].length=24; auto right=first; right.id=juce::Uuid().toString(); right.start=24; right.offset=24; right.length=24; split.clips.push_back(right);split.updateLaneHandles();
    check(times(*split.curve(lane))==std::vector<double>({0,12,24,36,48}),"split pieces share a single boundary and receive short-piece midpoint handles");
    auto authored=base; check(authored.moveAutomationPoint(lane,1,17,-9).wasOk(),"existing neutral handle can be edited directly");
    authored.curve(lane)->enabled=true; const auto authoredPoints=times(*authored.curve(lane));
    authored.clips[0].start=9; authored.bpm=90; authored.updateLaneHandles();
    check(times(*authored.curve(lane))==authoredPoints && authored.curve(lane)->points[1].value==-9,"authored lane nodes stay at project positions after move and tempo changes");
    authored.automationEdited(lane,AutomationEdit::removePoint); authored.curve(lane)->points.erase(authored.curve(lane)->points.begin()+1); authored.updateLaneHandles();
    check(authored.curve(lane)->points.size()==3,"deleted authored handle does not respawn");
    const auto saved=parseProject(json(authored)); check(saved.error.isEmpty() && saved.project.laneHandles.at(0).seeds.at(first.id).empty(),"session 46 retains authored ownership independently of point contents: "+saved.error);
    auto preroll=base; const auto shifted=preroll.setTimelineOrigin(4); preroll.updateLaneHandles();
    check(shifted.wasOk() && times(*preroll.curve(lane))==std::vector<double>({4,20,36,52}) && parseProject(json(preroll)).error.isEmpty(),"pre-roll rebases both default handles and their saved ownership");
    for(bool exponential:{false,true})for(bool gain:{false,true})
    {
        auto shaped=base; auto& curve=*shaped.curve(lane); curve={true,{{0,-30,.45,exponential,gain},{96,0}}};
        const auto original=curve; shaped.updateLaneHandles(); double error=0;
        for(int i=0;i<=192;++i)error=std::max(error,std::abs((double)curve.interpolatedValue(i*.5,0)-original.interpolatedValue(i*.5,0)));
        shaped.clips[0].start=7; shaped.updateLaneHandles();
        for(int i=0;i<=192;++i)error=std::max(error,std::abs((double)curve.interpolatedValue(i*.5,0)-original.interpolatedValue(i*.5,0)));
        shaped.clips.clear(); shaped.updateLaneHandles();
        check(error<.00005 && curve.points.size()==2 && std::abs(curve.points[0].bend-.45)<1.e-8,"generated subdivisions preserve bent "+juce::String(exponential?"exponential":"rational")+" / "+(gain?"gain":"dB")+" interpolation through add/move/delete");
    }
    auto render=[](Project p,double at)
    {
        Engine engine; engine.sampleRate=8000; engine.project=std::move(p); juce::AudioBuffer<float> audio(2,128); juce::MidiBuffer midi; engine.render(audio,128,at,midi); return audio;
    };
    auto error=[](const juce::AudioBuffer<float>& a,const juce::AudioBuffer<float>& b)
    { float result=0;for(int c=0;c<2;++c)for(int i=0;i<128;++i)result=std::max(result,std::abs(a.getSample(c,i)-b.getSample(c,i)));return result; };
    auto faded=overlap; faded.automationEdited(lane); auto& shared=*faded.curve(lane); shared={true,{{0,-6},{80,-6}}};
    faded.effectSettings(faded.clips[0]).automation[0]={true,{{0,0},{32,0},{48,-60}}};
    faded.effectSettings(faded.clips[1]).automation[0]={true,{{0,-60},{16,0},{48,0}}};
    auto perTrackOnly=faded; perTrackOnly.curve(lane)->enabled=false;
    const auto wet=render(faded,40), dry=render(perTrackOnly,40);
    check(std::abs(wet.getRMSLevel(0,0,128)/dry.getRMSLevel(0,0,128)-juce::Decibels::decibelsToGain(-6.f))<.0001,
        "one lane gain multiplies both overlapping tracks while separate transition fades remain active");
    check(faded.effectSettings(faded.clips[0]).automation[0].valueAt(1,0)>faded.effectSettings(faded.clips[0]).automation[0].valueAt(47,0)
        && faded.effectSettings(faded.clips[1]).automation[0].valueAt(1,0)<faded.effectSettings(faded.clips[1]).automation[0].valueAt(47,0),"per-track fades keep opposite directions on a shared lane");
    auto other=faded; other.clips[0].track=1; other.clips.erase(other.clips.begin()+1); auto otherBaseline=other; otherBaseline.tracks[0].automation={};
    check(error(render(other,20),render(otherBaseline,20))<1.e-7,"lane curve leaves other-lane PCM unchanged");
    Workstation ui(false);ui.stopTimer();ui.setSize(1440,900);ui.engine.sampleRate=8000;ui.engine.project=overlap;ui.selectedClip=second.id;ui.refresh();
    ui.timeline.chooseCurveTool(0);
    check(ui.displayedAddress(0)==lane && ui.selectedClip==second.id && ui.timeline.automationNodesVisible(lane),"normal Volume tool selects the lane and keeps track selection");
    bool visibleOnly=true;for(const auto& address:ui.timeline.visibleAutomationAddresses(0))if(ui.timeline.automationVisible(address))visibleOnly=visibleOnly && address==lane;
    check(visibleOnly,"ordinary editor shows only the selected lane parameter");
    const auto initial=json(ui.engine.project);ui.engine.position=12.25;ui.engine.playing=true;
    const auto mouse=juce::Desktop::getInstance().getMainMouseSource();ui.timeline.pixelsPerSecond=12;ui.refresh();
    const juce::Point<float> down{16*12,ui.timeline.automationY(0,0,0)},to{16*12,ui.timeline.automationY(0,-12,0)};
    auto event=[&](juce::Point<float> at,bool drag)
    {return juce::MouseEvent(mouse,at,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::altModifier),1,0,0,0,0,&ui.timeline,&ui.timeline,juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),1,drag);};
    ui.timeline.mouseDown(event(down,false));ui.timeline.mouseDrag(event(to,true));ui.timeline.mouseUp(event(to,true));
    check(ui.engine.project.curve(lane)->active() && ui.engine.project.curve(lane)->points[1].value<-6 && ui.undoHistory.size()==1
        && ui.engine.position==12.25 && ui.engine.playing,"native node drag activates the shared lane curve with one Undo and no live seek");
    const auto edited=json(ui.engine.project);ui.undo(false);check(json(ui.engine.project)==initial,"Undo restores untouched default-handle ownership");
    ui.undo(true);check(json(ui.engine.project)==edited,"Redo restores the edited shared curve exactly");
    ui.timeline.selectAutomationCurve({0,AutomationTarget::volume,{},{},second.id});check(ui.displayedAddress(0).clipId==second.id,"explicit Track fades scope still exposes per-track automation");
    ui.timeline.chooseCurveTool(0);ui.engine.playing=false;
    ui.engine.setControlValue(lane,-8);ui.refresh();
    check(ui.engine.project.tracks[0].gainDb==-8 && ui.engine.project.effectSettings(ui.engine.project.clips[1]).gainDb==0,"lane fader controls all tracks even when a track is selected");
    ui.performance->refresh(true);
    check(std::abs(ui.performance->faders[0].getValue()+8)<1.e-6 && ui.performance->faders[0].getTitle().contains("Mixer lane 1"),
        "performance fader display and owner agree with lane input while a track is selected");
    for(int target=0;target<trackAutomationCount;++target)
    {
        const AutomationAddress address{0,(AutomationTarget)target};const float value=target<4?-3.f:.25f;
        const auto old=ui.engine.project.effectSettings(ui.engine.project.clips[1]).manualValue((AutomationTarget)target);
        check(ui.engine.setControlValue(address,value) && ui.engine.project.manualValue(address)==value
            && ui.engine.project.effectSettings(ui.engine.project.clips[1]).manualValue((AutomationTarget)target)==old,
            "builtin control remains lane-owned with a selected track / "+juce::String(target));
    }
    ui.engine.setControlValue({0,AutomationTarget::cutoff,{},{},second.id},-.2f);
    check(ui.engine.project.effectSettings(ui.engine.project.clips[1]).filterCutoff==-.2f && ui.engine.project.tracks[0].filterCutoff==.25f,
        "explicit per-track cutoff remains separate from ordinary lane cutoff");
    ui.engine.applyMidiControl({MidiActionKind::pan,0},.75f);
    check(ui.engine.project.tracks[0].pan==.5f && ui.engine.project.effectSettings(ui.engine.project.clips[1]).pan==0,"MIDI pan retains lane ownership with a selected track");
    ui.engine.returnToAutomation(lane);check(ui.engine.project.curve(lane)->enabled,"Return to automation re-enables the shared lane curve");
    const auto path=report.getSiblingFile("shared-lane.mixworks");const auto saveError=ui.engine.saveProject(path);
    Engine reopened;const auto openError=saveError.isEmpty()?reopened.restoreProject(path.loadFileAsString()):saveError;
    check(openError.isEmpty(),"save and source-backed reopen the shared lane curve: "+openError);
    if(openError.isEmpty())
    {
        check(error(render(ui.engine.project,20),render(reopened.project,20))<2.e-6,"reopened shared automation reproduces the same PCM");
        juce::String exportError;auto exporter=reopened.createExportRenderer(exportError);check(exporter && exportError.isEmpty(),"offline export admits lane automation and per-track controls: "+exportError);
    }
    ui.showToolPage(2);ui.timeline.selectAutomationCurve(lane);ui.refresh();
    auto pixels=report.getSiblingFile("shared-lane-automation.png").createOutputStream();
    check(pixels && juce::PNGImageFormat().writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()),*pixels),"capture actual shared lane automation editor");
    auto touch=std::make_unique<Engine>();touch->sampleRate=8000;touch->project=overlap;touch->selectedClip=second.id;
    touch->armAutomation(0,true);touch->setAutomationRecordMode(AutomationRecordMode::touch);touch->setPlaying(true);touch->position=10;
    touch->setControlValue({0,AutomationTarget::cutoff},.4f);touch->setPlaying(false);touch->project.updateLaneHandles();
    check(touch->project.tracks[0].automation[5].active() && touch->project.tracks[0].automation[5].valueAt(10,0)==.4f
        && touch->project.effectSettings(touch->project.clips[1]).automation[5].points.empty(),
        "Touch writes project-time lane cutoff while a track is selected, without recreating neutral handles");
    Project::TransitionLink locked;locked.outgoing=first.id;locked.incoming=second.id;locked.locked=true;touch->project.transitionLinks={locked};
    touch->project.curve(lane)->enabled=false;const auto lockedBefore=json(touch->project);
    const bool blocked=!touch->setControlValue(lane,-9);touch->returnToAutomation(lane);
    check(blocked && json(touch->project)==lockedBefore,"locked transition protects shared lane input and Return to automation");
    Engine capture;capture.sampleRate=8000;capture.project=overlap;capture.selectedClip=second.id;capture.prepareRender(8000,128);capture.prepareClipProcessing();
    check(capture.setWorkMode(WorkMode::recordPerform).wasOk(),"prepare bounded Record/Perform ownership check");capture.setPlaying(true);
    check(capture.capturingPerformance(),"start source-backed take without a physical audio device");
    if(capture.capturingPerformance())
    {
        capture.setControlValue(lane,-12);juce::AudioBuffer<float> heard(2,128);capture.audioDeviceIOCallbackWithContext(nullptr,0,heard.getArrayOfWritePointers(),2,128,{});capture.stop();
        const auto& events=capture.project.performanceTakes.back().events;
        check(std::any_of(events.begin(),events.end(),[](const auto& e){return e.kind==TakeEventKind::control && e.control==0 && e.owner.isEmpty() && e.lane==0 && e.value==-12;}),"captured lane fader event has no hidden selected-track owner");
        juce::String replayError;auto replay=capture.prepareTakeReplay(0,replayError);
        juce::AudioBuffer<float> replayed(2,128);if(replay)replay->renderTake(replayed,128,0);
        check(replay && error(heard,replayed)<1.e-6,"captured and replayed lane-controlled PCM agree: "+replayError);
    }

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
            Clip clip; clip.media = media; clip.track = lane; clip.start = lane == 1 ? 8 : 0; clip.length = 16; base.clips.push_back(clip);
        }
        base.updateLaneHandles();
        const auto a = base.clips[0].id, b = base.clips[1].id;
        const auto json = [](const Project& p) { return juce::JSON::toString(projectToJson(p),juce::JSON::FormatOptions().withMaxDecimalPlaces(17)); };
        const auto hasVolumeNodes = [](const Project& p, size_t clip) { return !p.effectSettings(p.clips[clip]).automation[0].points.empty(); };
        auto reset = [&](bool playing, int preset)
        {
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
        capture("live-transition-playing.png");
        ui.undo(false); const bool presetUndone = !hasVolumeNodes(ui.engine.project,0); ui.undo(true);
        check(presetUndone && json(ui.engine.project)==json(applied) && ui.engine.playing && ui.engine.position==livePosition,"live preset Undo/Redo restores the exact project without changing transport");
        ui.undo(false); ui.undo(false); check(json(ui.engine.project)==json(base),"second Undo removes the newly added transition completely");
        ui.undo(true); ui.undo(true); ui.engine.setPlaying(false);
        ui.selectTransition(b,true); check(ui.engine.position==8,"explicit focused transition navigation still seeks while stopped");
        const auto file=report.getSiblingFile("live-transition.mixworks"); const auto save=ui.engine.saveProject(file);
        Engine reopened; const auto opened=save.isEmpty()?reopened.restoreProject(file.loadFileAsString()):save;
        check(save.isEmpty() && opened.isEmpty() && reopened.project.transitionFor(b),"save and actual source-backed reopen retain the placed transition: " + opened);
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
        ui.engine.setPlaying(false); return;
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
    auto image = report.getSiblingFile("transition-width-transport.png").createOutputStream(); juce::PNGImageFormat png;
    check(image && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *image), "capture transition width controls after transport-preserving editing");
    ui.engine.project = base; ui.selectTransition(id); ui.setSize(1000, 620);
    check(ui.selectedTransitionLength.getWidth() >= 100 && ui.transitionPresetForSelection.getWidth() >= 120,
        "compact transition panel keeps primary edits reachable without repeated track information");
    check(ui.previousTransition.getTooltip().contains("Ctrl + Alt"), "transition keyboard help remains available on navigation tooltips");

}
}
