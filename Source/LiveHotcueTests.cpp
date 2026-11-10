#include "Workstation.h"
#include "Hotcues.h"
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
    auto timelineJourney=[&]
    {
        check(writeAnalysisRecord(media->file,media->analysisRecord()).isEmpty(),"seed owned cue fixture analysis without detector work");
        Workstation ui(false); ui.stopTimer(); ui.setSize(1440,940); ui.engine.sampleRate=8000; ui.engine.project=base; ui.engine.library={media}; ui.selectedClip=id; ui.selectTrack(0);
        auto pump=[] {
            #if JUCE_WINDOWS
            MSG message; while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
            #endif
        };
        const auto original=json(ui.engine.project);
        for(bool playing:{false,true})
        {
            ui.engine.seek(1.49); ui.engine.setPlaying(playing); ui.refresh();
            ui.performance->channelButtons[0].onClick();
            check(std::abs(ui.engine.position.load()-4.25)<1.e-9 && ui.engine.playing==playing && json(ui.engine.project)==original,
                "native cue pad jumps whole programme immediately with transport state kept / "+juce::String((int)playing));
            check(!ui.engine.hasLiveHotcueVoices() && ui.performance->channelButtons[0].getTooltip().contains("whole mix"),"cue action cannot silently relaunch only one voice");
        }
        ui.engine.seek(1.1); const auto keyboard=ui.keyPressed(juce::KeyPress('1',juce::ModifierKeys(juce::ModifierKeys::altModifier),0));
        check(keyboard && ui.engine.position==4.25 && ui.engine.playing,"Alt+1 uses the same running timeline jump");
        Engine oracle; oracle.sampleRate=8000; oracle.project=base; juce::AudioBuffer<float> actual(2,128),expected(2,128); juce::MidiBuffer midi;
        oracle.render(expected,128,4.25,midi); float* outputs[]{actual.getWritePointer(0),actual.getWritePointer(1)};
        ui.engine.audioDeviceIOCallbackWithContext(nullptr,0,outputs,2,128,{}); float difference=0;
        for(int ch=0;ch<2;++ch)for(int i=0;i<128;++i)difference=std::max(difference,std::abs(expected.getSample(ch,i)-actual.getSample(ch,i)));
        check(difference<1.e-6f && actual.getMagnitude(0,128)>.001f,"all-lane callback PCM follows the jumped programme position");
        const auto parsedAction=parseMidiAction("timeline-cue-1");
        check(parsedAction && parsedAction->kind==MidiActionKind::timelineCue && parsedAction->index==0,"timeline cue is a durable typed MIDI action");
        ui.engine.setMidiLearnEnabled(true); ui.performance->channelButtons[0].onClick();
        ui.engine.testMidi(juce::MidiMessage::controllerEvent(1,77,127)); ui.engine.flushMidiControls(); ui.engine.setMidiLearnEnabled(false);
        ui.engine.seek(1.25); ui.engine.testMidi(juce::MidiMessage::controllerEvent(1,77,0)); ui.engine.flushMidiControls();
        ui.engine.testMidi(juce::MidiMessage::controllerEvent(1,77,127)); ui.engine.flushMidiControls(); pump();
        check(ui.engine.position==4.25 && ui.engine.playing,"virtually learned MIDI cue uses the same programme jump without a physical port");
        ui.engine.project.clips[0].start=10; ui.engine.seek(1); ui.refresh();
        check(ui.performance->channelButtons[0].isEnabled(),"future timeline cues remain available while another passage plays");
        ui.locateHotcue(0); check(ui.engine.position==14.25 && ui.engine.playing,"future cue jumps programme and other lanes to its actual placement");
        ui.engine.setPlaying(false); ui.engine.project=base; ui.engine.project.clips[0].offset=10; ui.engine.project.clips[0].length=3; ui.engine.seek(1); ui.refresh();
        const auto trimmed=json(ui.engine.project); ui.locateHotcue(0);
        check(ui.engine.position==1 && json(ui.engine.project)==trimmed && ui.status.contains("outside"),"trimmed-out cue explains its unavailable source range without restoring audio");
        ui.engine.project=base; ui.engine.setWorkMode(WorkMode::recordPerform); ui.engine.seek(1); ui.engine.setPlaying(true); ui.refresh();
        check(ui.engine.capturingPerformance(),"Record/Perform starts an actual take for timeline cue acceptance");
        ui.engine.audioDeviceIOCallbackWithContext(nullptr,0,outputs,2,64,{}); ui.locateHotcue(0);
        check(std::any_of(ui.engine.project.performanceTakes[0].events.begin(),ui.engine.project.performanceTakes[0].events.end(),[](const auto& e){return e.kind==TakeEventKind::seek && e.value==4.25;}),"global cue records a programme seek in editable chronology");
        ui.engine.audioDeviceIOCallbackWithContext(nullptr,0,outputs,2,128,{}); ui.engine.finalisePerformance(); juce::String error; auto replay=ui.engine.prepareTakeReplay(0,error);
        check(replay && error.isEmpty(),"timeline-seek take admits replay: "+error);
        if(replay) { juce::AudioBuffer<float> heard(2,128); replay->renderTake(heard,128,64./8000); float errorPcm=0;
            for(int ch=0;ch<2;++ch)for(int i=0;i<128;++i)errorPcm=std::max(errorPcm,std::abs(heard.getSample(ch,i)-actual.getSample(ch,i)));
            check(errorPcm<2.e-6f,"take replay reproduces cue-jumped callback PCM"); }
        auto image=report.getSiblingFile("timeline-cues.png").createOutputStream();
        check(image && juce::PNGImageFormat().writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()),*image),"capture native timeline cue controls");
    };

    if(juce::JUCEApplicationBase::getCommandLineParameters().contains("--timeline-cues-only")) { timelineJourney(); return; }
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

    timelineJourney();

}
}
