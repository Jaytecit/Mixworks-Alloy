#include "Workstation.h"
#include "Hotcues.h"
#include "Beatmatch.h"
#include <cmath>

namespace mixworks
{
void runLiveHotcueTests(const std::function<void(bool,const juce::String&)>& check, const juce::File& report)
{
    const auto json = [](const Project& project) { return juce::JSON::toString(projectToJson(project),true,17); };
    auto media = std::make_shared<Media>(); media->name = "Target voice"; media->sampleRate = 8000;
    media->manualBpm = 120; media->manualBeatOrigin = 0; media->samples.setSize(1,24*8000);
    media->file = report.getSiblingFile("live-hotcue-source.wav");
    for (int i=0;i<media->frames();++i) media->samples.setSample(0,i,(float)(.14*std::sin(i*.013)+.05*std::cos(i*.037)));
    {
        juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream = media->file.createOutputStream();
        auto writer = wav.createWriterFor(stream,juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(1).withBitsPerSample(24));
        check(writer && writer->writeFromAudioSampleBuffer(media->samples,0,media->frames()),"create owned callback hotcue audio");
    }
    media->analysisSource = {audioDigest(media->file),media->file.getSize(),8000,media->frames(),1};
    media->sourceModified = media->file.getLastModificationTime().toMilliseconds();
    Project base; base.followTrackTempo = false; base.masterDb = 0; base.transitionLinksKnown = true;
    base.launch = {LaunchTiming::nextBeat,16};
    Clip target; target.media = media; target.length = 20; target.controls.pan = -1;
    Clip neighbour = target; neighbour.id = juce::Uuid().toString(); neighbour.controls.pan = 1; neighbour.offset = .125;
    Clip other = neighbour; other.id = juce::Uuid().toString(); other.track = 2; other.offset = .25;
    base.clips = {target,neighbour,other}; const auto id = target.id, second = other.id;
    base.putHotcue(id,0,4.25); base.putHotcue(id,1,6.125); base.putHotcue(second,0,8.25);
    for (double rate : {8000.,44100.,48000.}) for (int block : {1,31,173,512})
    {
        auto engine = std::make_unique<Engine>(juce::File(),false), reference = std::make_unique<Engine>(juce::File(),false);
        engine->project = reference->project = base; engine->sampleRate = reference->sampleRate = rate;
        const double begin = 1.499013; engine->position = begin; engine->setPlaying(true);
        const auto before = json(engine->project); const auto admitted = engine->triggerLiveHotcue(id,0);
        const int edge = (int)std::ceil((1.5-begin)*rate-1.e-6);
        juce::AudioBuffer<float> actual(2,block), expected(2,block), suffix(2,block); juce::MidiBuffer midi;
        bool pcm=true,otherUnchanged=true; int rendered=0; float maximumError=0;
        while (rendered < edge+block*2)
        {
            const double at=engine->position.load(); auto* outputs=actual.getArrayOfWritePointers();
            engine->audioDeviceIOCallbackWithContext(nullptr,0,outputs,2,block,{});
            const int prefix=juce::jlimit(0,block,edge-rendered);
            if (prefix>0) reference->render(expected,prefix,at,midi);
            if (prefix<block)
            {
                const double executed=begin+edge/rate;
                reference->project.clips[0].start=-(4.25-executed);
                reference->render(suffix,block-prefix,at+prefix/rate,midi);
            }
            for(int ch=0;ch<2;++ch) for(int n=0;n<block;++n)
            {
                const auto difference=std::abs(actual.getSample(ch,n)-(n<prefix?expected.getSample(ch,n):suffix.getSample(ch,n-prefix)));
                maximumError=std::max(maximumError,difference); pcm &= difference < 3.e-6f;
            }
            // The target is hard left. Overlapping same-lane and lane3 voices
            // are hard right, so their continuous output is independently tested.
            reference->project=base; reference->render(expected,block,at,midi);
            for(int n=0;n<block;++n) otherUnchanged &= actual.getSample(1,n)==expected.getSample(1,n);
            rendered+=block;
        }
        const auto state=engine->liveHotcueStatus(id);
        check(admitted.wasOk() && state.state==LiveHotcueState::launched && state.active && pcm && otherUnchanged
            && state.actualTime>=1.5-1.e-10 && state.actualTime<1.5+1./rate
            && std::abs(engine->position.load()-(begin+rendered/rate))<1.e-10 && json(engine->project)==before,
            "target-only hotcue PCM, continuous overlapping voices, programme clock/document unchanged within one sample / " + juce::String(rate,0)+" Hz / block "+juce::String(block)
            + " / pcm="+juce::String((int)pcm)+" other="+juce::String((int)otherUnchanged)+" state="+juce::String((int)state.state)+" active="+juce::String((int)state.active)
            + " error="+juce::String(maximumError,9)+" executed="+juce::String(state.actualTime,12)+" position="+juce::String(engine->position.load(),12));
    }
    auto engine=std::make_unique<Engine>(juce::File(),false);
    auto setup=[&]
    {
        engine->stop(); engine->project=base; engine->sampleRate=8000; engine->position=1.49; engine->setPlaying(true);
        return engine->triggerLiveHotcue(id,0).wasOk();
    };
    check(setup() && engine->triggerLiveHotcue(second,0).wasOk() && engine->triggerLiveHotcue(id,1).wasOk()
        && engine->liveHotcueStatus(id).slot==1 && engine->liveHotcueStatus(second).state==LiveHotcueState::pending,
        "different cue replaces only its target queue while another target stays pending");
    check(engine->triggerLiveHotcue(id,1).wasOk() && engine->liveHotcueStatus(id).state==LiveHotcueState::cancelled
        && engine->liveHotcueStatus(second).state==LiveHotcueState::pending,"same pending cue toggles only its target cancellation");
    engine->cancelLiveHotcue(second);
    check(engine->liveHotcueStatus(second).state==LiveHotcueState::cancelled,"explicit target Cancel uses the same queue owner");
    for(int kind=0;kind<11;++kind)
    {
        const bool admitted=setup();
        switch(kind)
        {
            case 0: engine->seek(1.6); break;
            case 1: engine->setPlaying(false); break;
            case 2: engine->project.tempo.origin=.1; break;
            case 3: engine->setLaunchSettings({LaunchTiming::nextBar,8}); break;
            case 4: engine->sampleRate=48000; break;
            case 5: engine->project.clips[0].offset=.1; break;
            case 6: engine->project.putHotcue(id,0,4.5); break;
            case 7: engine->exporting=true; break;
            case 8: engine->controlInputEnabled=false; break;
            case 9: engine->project.clips.erase(engine->project.clips.begin()); break;
            case 10: engine->setAutomationRecordMode(AutomationRecordMode::touch); break;
        }
        engine->serviceLiveHotcues();
        check(admitted && engine->liveHotcueStatus(id).state==LiveHotcueState::cancelled,"queued target cancels stale transport/grid/timing/device/geometry/cue/export/preparation/removal/recording / "+juce::String(kind));
        engine->exporting=false; engine->controlInputEnabled=true; engine->setAutomationRecordMode(AutomationRecordMode::read);
    }
    setup(); engine->project.launch.timing=LaunchTiming::immediate;
    engine->serviceLiveHotcues(); const bool immediate=engine->triggerLiveHotcue(id,0).wasOk();
    juce::AudioBuffer<float> output(2,31); auto* outputs=output.getArrayOfWritePointers();
    engine->audioDeviceIOCallbackWithContext(nullptr,0,outputs,2,31,{});
    check(immediate && engine->liveHotcueStatus(id).actualTime==1.49 && engine->liveHotcueStatus(id).active
        && std::abs(engine->liveClipTime(id,1.49)-4.25)<1.e-10,"explicit Immediate executes at the next audio sample without seeking the programme");
    const auto sourceClock=engine->liveClipTime(id,engine->position); engine->setPlaying(false);
    engine->serviceLiveHotcues(); engine->setPlaying(true);
    check(engine->liveHotcueStatus(id).active && engine->liveClipTime(id,engine->position)==sourceClock,"pause/resume retains the stopped target cursor while pending actions cancel");
    engine->setAutomationRecordMode(AutomationRecordMode::touch);
    check(engine->automationRecordMode==AutomationRecordMode::read && engine->liveClipTime(id,engine->position)==sourceClock,
        "arming cannot silently replace an executed cursor with arrangement audio before discrete event recording exists");
    engine->setLaunchSettings({LaunchTiming::nextBeat,16});
    const bool replacement=engine->triggerLiveHotcue(id,1).wasOk(); engine->project.tempo.origin=.1; engine->serviceLiveHotcues();
    check(replacement && engine->liveHotcueStatus(id).state==LiveHotcueState::cancelled && engine->liveHotcueStatus(id).active
        && engine->liveClipTime(id,engine->position)==sourceClock,"stale replacement queue cancels without losing the previous active source cursor");
    engine->stop(); check(!engine->liveHotcueStatus(id).active,"main Stop clears runtime target cursors");

    for(int kind=0;kind<6;++kind)
    {
        auto scenario=base; scenario.launch.timing=LaunchTiming::immediate;
        auto& c=scenario.clips[0]; c.offset=.5; c.length=16; c.beatmatched=true; c.rate=1.125;
        if(kind==1) { scenario.followTrackTempo=true; scenario.tempo.points={{0,120,false},{5,132,true},{12,108,false}}; }
        if(kind==2) c.reversed=true;
        if(kind==3) { c.loopSpan=2; c.loopPhase=.125; }
        if(kind==4) c.audioSlip=.017;
        const auto prepared=prepareTempoMap(scenario,false,0);
        if(!prepared.error.isEmpty()) { check(false,"prepare real hotcue mapping variant / "+juce::String(kind)+" / "+prepared.error); continue; }
        scenario=prepared.project; auto& mapped=scenario.clips[0];
        if(kind==5)
        {
            mapped.elasticPlayback=true; mapped.prepared.reset(); mapped.elasticSource=std::make_shared<const ElasticSource>(mapped);
            // A split/duplicate may share the original immutable elastic clock.
            // Its ordinary stream must not be rewound by this target's hotcue.
            scenario.clips[1]=mapped; scenario.clips[1].id=neighbour.id; scenario.clips[1].controls.pan=1;
        }
        const double cueSource=mapped.sourceTimeAt(mapped.length*.4);
        scenario.putHotcue(id,0,cueSource);
        const auto destination=hotcueTimelineTime(mapped,0,mapped.start+mapped.length*.1);
        auto actual=std::make_unique<Engine>(juce::File(),false), expected=std::make_unique<Engine>(juce::File(),false), unchanged=std::make_unique<Engine>(juce::File(),false);
        actual->project=expected->project=unchanged->project=scenario; expected->project.clips={mapped};
        actual->sampleRate=expected->sampleRate=unchanged->sampleRate=8000;
        actual->position=mapped.start+mapped.length*.1; actual->setPlaying(true);
        const auto before=json(actual->project); const auto admitted=actual->triggerLiveHotcue(id,0);
        bool pcm=destination.has_value(),continuity=true; juce::AudioBuffer<float> a(2,173),b(2,173),u(2,173);juce::MidiBuffer midi;
        for(int n=0;n<3 && destination;++n)
        {
            const double now=actual->position.load();
            actual->audioDeviceIOCallbackWithContext(nullptr,0,a.getArrayOfWritePointers(),2,173,{});
            expected->render(b,173,*destination+n*173./8000,midi); unchanged->render(u,173,now,midi);
            for(int frame=0;frame<173;++frame)
            { pcm &= std::abs(a.getSample(0,frame)-b.getSample(0,frame))<4.e-6f; continuity &= a.getSample(1,frame)==u.getSample(1,frame); }
        }
        check(admitted.wasOk() && actual->liveHotcueStatus(id).active && pcm && continuity && json(actual->project)==before,
            "real prepared source cursor preserves mapped PCM/other voices for constant/ramped/reverse/arranged-loop/slip/elastic / "+juce::String(kind));
    }

    setup(); engine->triggerLiveHotcue(second,0);
    for(int n=0;n<4;++n) engine->audioDeviceIOCallbackWithContext(nullptr,0,outputs,2,31,{});
    check(engine->liveHotcueStatus(id).active && engine->liveHotcueStatus(second).active
        && engine->liveHotcueStatus(id).actualTime==engine->liveHotcueStatus(second).actualTime
        && engine->liveClipTime(id,engine->position)!=engine->liveClipTime(second,engine->position),"two targets execute at one programme boundary with independent source cursors");
    const auto activeBefore=engine->liveClipTime(id,engine->position);
    const auto file=report.getSiblingFile("live-hotcue-runtime.mixworks"); const auto saved=engine->saveProject(file);
    auto reopened=std::make_unique<Engine>(juce::File(),false); const auto opened=reopened->restoreProject(file.loadFileAsString());
    check(saved.isEmpty() && opened.isEmpty() && !reopened->playing && reopened->liveHotcueStatus(id).state==LiveHotcueState::idle
        && !reopened->hasLiveHotcueVoices() && engine->liveClipTime(id,engine->position)==activeBefore,
        "real Save/Open retains cues/settings while live cursors/queues restore silently absent and saving leaves the running voice alone: "+saved+opened);
    const auto restored=engine->restoreProject(file.loadFileAsString());
    check(restored.isEmpty() && !engine->playing && !engine->hasLiveHotcueVoices(),"opening a document in a previously live engine clears all runtime voice ownership");
    setup(); const bool paused=engine->toggleScheduledPause().wasOk(); juce::AudioBuffer<float> pauseOutput(2,173);
    engine->audioDeviceIOCallbackWithContext(nullptr,0,pauseOutput.getArrayOfWritePointers(),2,173,{});
    check(paused && !engine->playing && engine->scheduledPauseStatus().state==ScheduledPauseState::paused
        && engine->liveHotcueStatus(id).state==LiveHotcueState::cancelled && !engine->liveHotcueStatus(id).active,
        "scheduled pause wins a shared sample boundary; pending hotcue cannot execute after the transport stops");

    setup(); engine->setLaunchSettings({LaunchTiming::immediate,16}); engine->triggerLiveHotcue(id,0);
    engine->audioDeviceIOCallbackWithContext(nullptr,0,outputs,2,31,{});
    auto offline=std::make_unique<Engine>(juce::File(),false);offline->project=base;offline->sampleRate=8000;
    juce::AudioBuffer<float> arrangement(2,31),baseline(2,31);juce::MidiBuffer noMidi;
    engine->render(arrangement,31,engine->position,noMidi);offline->render(baseline,31,engine->position,noMidi);
    bool same=true;for(int ch=0;ch<2;++ch)for(int n=0;n<31;++n)same &= arrangement.getSample(ch,n)==baseline.getSample(ch,n);
    check(same && engine->hasLiveHotcueVoices(),"ordinary offline render remains arrangement-only while a callback voice is live");
    {
        auto retirement=std::make_unique<Engine>(juce::File(),false); retirement->project=base; retirement->sampleRate=8000;
        retirement->position=1.49; retirement->setPlaying(true); const auto references=media.use_count();
        const bool admitted=retirement->triggerLiveHotcue(id,0).wasOk(); const auto captured=media.use_count();
        retirement->stop(); retirement->serviceScheduledPause();
        check(admitted && captured>references && media.use_count()==references && retirement->liveHotcueStatus(id).state==LiveHotcueState::cancelled,
            "message-thread transport service retires cancelled media snapshots while preserving visible cancellation status");
    }

    Workstation ui(false); ui.stopTimer(); ui.engine.project=base; ui.engine.library={media}; ui.selectedClip=id; ui.selectTrack(0);
    ui.engine.project.clips[0].controls.automation[0]={true,{{0,0},{2,0},{3,-6},{10,-6}}};
    ui.engine.position=1.49; ui.engine.setPlaying(true); ui.dirty=false; ui.refresh();
    const auto before=json(ui.engine.project); const auto history=ui.undoHistory.size();
    ui.performance->channelButtons[0].onClick();
    check(ui.performance->channelButtons[0].isEnabled() && ui.performance->channelButtons[0].getButtonText().contains("@")
        && ui.engine.liveHotcueStatus(id).state==LiveHotcueState::pending && ui.engine.position==1.49
        && before==json(ui.engine.project) && history==ui.undoHistory.size() && !ui.dirty,"native live pad exposes target queue/boundary without document/history/global seek");
    for(int width:{1250,1920})
    {
        ui.setSize(width,1080); ui.showToolPage(0); ui.refresh();
        auto stream=report.getSiblingFile("live-hotcue-queued-"+juce::String(width)+".png").createOutputStream(); juce::PNGImageFormat png;
        check(stream && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()),*stream),"capture visible live target cue queue / width "+juce::String(width));
        ui.performanceViewport.setViewPosition(ui.performance->getWidth(),ui.performance->getHeight());
        auto controls=report.getSiblingFile("live-hotcue-queued-controls-"+juce::String(width)+".png").createOutputStream();
        check(controls && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()),*controls),"capture revealed hotcue pads and programme boundary / width "+juce::String(width));
    }
    const auto expected=ui.engine.project.launch; ui.performance->performLaunchMenu(401,expected,id);
    check(ui.engine.liveHotcueStatus(id).state==LiveHotcueState::cancelled && ui.engine.playing && before==json(ui.engine.project),"native timing-menu Cancel cancels only its captured target");
    ui.engine.setLaunchSettings({LaunchTiming::immediate,16});ui.performance->channelButtons[0].onClick();
    ui.engine.audioDeviceIOCallbackWithContext(nullptr,0,outputs,2,31,{});ui.refresh();
    check(!ui.performance->buttons[5].isEnabled() && ui.performance->buttons[5].getTooltip().contains("Stop"),"native Record explains the unrecorded live-cursor restriction instead of replacing programme playback");
    check(std::abs(ui.performance->faders[0].getValue()+6)<1.e-5 && ui.engine.position<2,
        "Controller source-owned fader follows the live source automation while programme time remains before that shape");
    ui.engine.stop();
    ui.engine.project.clips[0].start=10;ui.engine.position=1;ui.engine.setPlaying(true);ui.refresh();
    check(!ui.performance->channelButtons[0].isEnabled() && ui.engine.triggerLiveHotcue(id,0).failed() && ui.engine.position==1,
        "a selected future segment has visibly unavailable live pads and cannot start implicitly or seek the programme");
    ui.engine.stop();
}
}
