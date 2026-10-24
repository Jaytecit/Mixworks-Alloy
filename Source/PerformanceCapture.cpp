#include "Engine.h"
#include "EffectTail.h"
#include <algorithm>

namespace mixworks
{
namespace
{
const juce::String eventLimitIssue="Editable event capacity reached. Keep the optional master WAV; programme playback continues.";
juce::var withoutTakes(juce::var value)
{
    value=value.clone(); if (auto* object=value.getDynamicObject()) object->removeProperty("performanceTakes"); return value;
}
AutomationAddress eventAddress(const Project& project,const TakeEvent& event)
{
    AutomationAddress address {event.lane,(AutomationTarget)event.control,event.slot,event.parameter}; address.stem=event.stem;
    if (event.owner.isNotEmpty()) for (const auto& c:project.clips)
        if (effectsIdentity(c.effects)==event.owner || c.id==event.owner) { address.clipId=c.id; break; }
    return address;
}
}
Engine::ParameterCapture::ParameterCapture(Engine& e,juce::AudioProcessorParameter& p,TakeEvent v):engine(e),parameter(p),event(std::move(v)) { parameter.addListener(this); }
Engine::ProcessorCapture::ProcessorCapture(juce::AudioProcessor& p):processor(p) { p.addListener(this); }
Engine::ProcessorCapture::~ProcessorCapture() { processor.removeListener(this); }
void Engine::ProcessorCapture::audioProcessorChanged(juce::AudioProcessor*,const ChangeDetails& details)
{ if (details.programChanged || details.nonParameterStateChanged) changed=true; }
Engine::ParameterCapture::~ParameterCapture() { parameter.removeListener(this); }
void Engine::ParameterCapture::parameterValueChanged(int,float value)
    {
        const juce::ScopedTryLock guard(engine.lock);
        if (!guard.isLocked()) { missed=true; return; }
        if (!engine.capturingPerformance() || engine.renderingPlugin || engine.applyingPerformanceEvent) return;
        auto current=event; current.value=value; current.sourcePosition=std::max(0.,engine.position.load());
        const auto address=eventAddress(engine.project,current);
        if (engine.ensurePluginAutomation(address))
        {
            auto found=std::find_if(engine.activeWrites.begin(),engine.activeWrites.end(),[&](const auto& w) { return w.address==address; });
            if (found==engine.activeWrites.end()) { ActiveWrite write; write.address=address; write.original=*engine.project.curve(address); write.manualBefore=engine.project.manualValue(address); engine.activeWrites.push_back(std::move(write)); found=std::prev(engine.activeWrites.end()); }
            found->value=value; found->lastInput=found->lastTime=engine.performanceFrames.load()/engine.sampleRate.load();
            engine.project.setManualValue(address,value); engine.project.curve(address)->enabled=false;
        }
        engine.appendTakeEvent(std::move(current));
    }
TakeState Engine::takeState() const
{
    return activeTake>=0 && activeTake<(int)project.performanceTakes.size() ? project.performanceTakes[(size_t)activeTake].state : TakeState::idle;
}
bool Engine::capturingPerformance() const { return activeTake>=0 && performanceOriginal!=nullptr; }
juce::Result Engine::setWorkMode(WorkMode mode)
{
    const juce::ScopedLock guard(lock);
    if (exporting) return juce::Result::fail("Finish export before changing mode.");
    if (mode==workMode) return juce::Result::ok();
    finalisePerformance(); endTakeAudition(); finishAutomation(); workMode=mode; performanceError.clear();
    automationRecordMode=mode==WorkMode::recordPerform ? AutomationRecordMode::write : AutomationRecordMode::read;
    automationArmed.fill(mode==WorkMode::recordPerform); masterAutomationArmed=mode==WorkMode::recordPerform;
    return juce::Result::ok();
}
void Engine::attachPerformanceParameters()
{
    detachPerformanceParameters();
    auto chain=[&](int lane,const juce::String& owner,const auto& plugins)
    {
        for (const auto& slot:plugins) processorCaptures.push_back(std::make_unique<ProcessorCapture>(*slot->processor));
        for (const auto& slot:plugins) for (const auto& [id,p]:slot->parameters)
        {
            TakeEvent event; event.lane=lane; event.owner=owner; event.slot=slot->id; event.parameter=id; event.control=(int)AutomationTarget::plugin;
            ensurePluginAutomation(eventAddress(project,event));
            parameterCaptures.push_back(std::make_unique<ParameterCapture>(*this,*p,std::move(event)));
        }
    };
    for (int lane=0;lane<trackCount;++lane) chain(lane,{},plugins[(size_t)lane]);
    for (const auto& c:project.clips) if (project.effectOwner(c)==&c) chain(c.track,effectsIdentity(c.effects),c.effects->plugins);
}
void Engine::detachPerformanceParameters() { parameterCaptures.clear(); processorCaptures.clear(); }
juce::Result Engine::beginPerformance()
{
    const juce::ScopedLock guard(lock);
    if (capturingPerformance()) return juce::Result::ok();
    if (project.clips.empty() || !playbackReady() || exporting || rehearsalActive()) return juce::Result::fail("Prepare programme audio before starting a take.");
    if (project.performanceTakes.size()>=32) return juce::Result::fail("Save or discard a take before recording another (32-take limit).");
    juce::String preparationError; auto original=createExportRenderer(preparationError); if (!original) return juce::Result::fail(preparationError);
    if (onPerformanceBegin) onPerformanceBegin();
    PerformanceTake take; take.initial=withoutTakes(captureProject()); take.initialPosition=position.load(); take.sampleRate=sampleRate.load();
    take.name="Take "+juce::String((int)project.performanceTakes.size()+1); take.events.reserve(PerformanceTake::maximumEvents); take.state=TakeState::recording;
    if (recordMasterWav)
    {
        const auto valid=validateWriteDestination(masterRecordingDestination); if (valid.failed()) return valid;
        const auto result=masterRecorder.start(masterRecordingDestination,sampleRate.load()); if (result.failed()) return result;
        take.masterWav=masterRecordingDestination.getFullPathName();
    }
    performanceOriginal=std::make_unique<Project>(std::move(original->project)); performanceOriginal->performanceTakes.clear(); originalEngine=std::move(original);
    project.performanceTakes.push_back(std::move(take)); activeTake=(int)project.performanceTakes.size()-1; performanceFrames=0; takeSequence=0;
    resetPlugins(); performanceActive=true; performanceEnded=false; performanceCallbackMissed=false; attachPerformanceParameters(); return juce::Result::ok();
}
void Engine::appendTakeEvent(TakeEvent event)
{
    if (!capturingPerformance() || applyingPerformanceEvent) return;
    auto& take=project.performanceTakes[(size_t)activeTake];
    if (take.events.size()==PerformanceTake::maximumEvents) { take.complete=false; take.issue=eventLimitIssue; return; }
    event.frame=std::max(event.frame,performanceFrames.load()); event.sequence=takeSequence++;
    // Events at an audible callback boundary can precede the next message-thread
    // gesture. Do not reorder ties by source position after a seek.
    if (!take.events.empty()) event.frame=std::max(event.frame,take.events.back().frame);
    take.events.push_back(std::move(event));
}
void Engine::recordProjectCommit()
{
    const juce::ScopedLock guard(lock);
    if (!capturingPerformance() || !performanceProjectPending) return;
    if (!playbackReady()) return; // Admit asynchronous edits only when they can become audible.
    performanceProjectPending=false;
    auto& take=project.performanceTakes[(size_t)activeTake];
    if (std::count_if(take.events.begin(),take.events.end(),[](const auto& event) { return event.kind==TakeEventKind::project; })>=64)
    { take.complete=false; take.issue="Structural checkpoint capacity reached. Keep the optional master WAV; original arrangement remains protected."; return; }
    TakeEvent event; event.kind=TakeEventKind::project; event.sourcePosition=position.load(); event.payload=withoutTakes(captureProject(true));
    auto before=take.initial;
    for (auto it=take.events.rbegin();it!=take.events.rend();++it) if (it->kind==TakeEventKind::project) { before=it->payload; break; }
    auto topology=[](const juce::var& document)
    {
        std::map<juce::String,juce::String> result;
        for (const auto* field:{"tracks","clips"}) if (const auto* owners=document[field].getArray()) for (int i=0;i<owners->size();++i)
        {
            const auto& owner=owners->getReference(i); if (owner["effectOwner"].toString().isNotEmpty()) continue;
            const auto key=owner.hasProperty("trackOwnerId") ? owner["trackOwnerId"].toString() : "lane-"+juce::String(i);
            juce::String chain; if (const auto* slots=owner["plugins"].getArray()) for (const auto& slot:*slots) chain+=slot["id"].toString()+":"+slot["bypass"].toString()+";";
            result[key]=chain;
        }
        return result;
    };
    const auto oldTopology=topology(before),newTopology=topology(event.payload);
    if (std::any_of(oldTopology.begin(),oldTopology.end(),[&](const auto& owner) { const auto found=newTopology.find(owner.first); return found!=newTopology.end() && found->second!=owner.second; }))
    { take.complete=false; take.issue="An effect chain/bypass topology changed. The snapshot is retained, but the master WAV is required for an exact audible record."; }
    if (before["bpm"]!=event.payload["bpm"] || before["followTrackTempo"]!=event.payload["followTrackTempo"] || juce::JSON::toString(before["tempo"])!=juce::JSON::toString(event.payload["tempo"]))
    { TakeEvent tempo; tempo.kind=TakeEventKind::tempo; tempo.sourcePosition=position.load(); tempo.value=project.gridClock().bpmAt(position.load()); appendTakeEvent(std::move(tempo)); }
    appendTakeEvent(std::move(event)); attachPerformanceParameters();
}
void Engine::returnToAutomation(const AutomationAddress& input)
{
    const juce::ScopedLock guard(lock); auto address=input;
    if (address.clipId.isEmpty() && address.target!=AutomationTarget::master && controlClip(address.track)) address.clipId=selectedClip;
    auto* curve=project.curve(address); if (!curve) return;
    const auto writing=std::find_if(activeWrites.begin(),activeWrites.end(),[&](const auto& item) { return item.address==address; });
    const float returnValue=writing!=activeWrites.end() ? writing->manualBefore : project.manualValue(address);
    const bool returnEnabled=writing!=activeWrites.end() ? writing->original.enabled : !curve->points.empty();
    if (capturingPerformance())
    {
        TakeEvent event; event.kind=TakeEventKind::returnAutomation; event.lane=address.track; event.control=(int)address.target;
        event.slot=address.slotId; event.parameter=address.parameterId; event.stem=address.stem;
        if (const auto* c=project.clipFor(address.clipId)) event.owner=effectsIdentity(c->effects);
        event.sourcePosition=position.load(); event.value=returnValue; event.payload=returnEnabled; appendTakeEvent(std::move(event));
        if (const auto* original=performanceOriginal->curve(address)) *curve=*original;
    }
    for (auto it=activeWrites.begin();it!=activeWrites.end();)
        if (it->address==address) { if (capturingPerformance()) *curve=it->original; else finishWrite(*it,position.load(),true); it=activeWrites.erase(it); } else ++it;
    curve->enabled=returnEnabled && !curve->points.empty(); project.setManualValue(address,returnValue);
    if (auto* parameter=parameterFor(address)) { const juce::ScopedValueSetter<bool> applying(applyingPerformanceEvent,true); parameter->setValueNotifyingHost(returnValue); }
}
void Engine::restoreInitialPluginStates(const juce::var& snapshot)
{
    const juce::ScopedValueSetter<bool> applying(applyingPerformanceEvent,true);
    auto chain=[](const juce::var& values,auto& slots)
    {
        if (const auto* array=values.getArray()) for (const auto& value:*array) for (auto& slot:slots)
            if (slot->id==value["id"].toString())
            { juce::MemoryBlock state; if (state.fromBase64Encoding(value["state"].toString())) slot->processor->setStateInformation(state.getData(),(int)state.getSize()); }
    };
    for (int lane=0;lane<trackCount;++lane) chain(snapshot["tracks"][lane]["plugins"],plugins[(size_t)lane]);
    if (const auto* clips=snapshot["clips"].getArray()) for (const auto& value:*clips)
        if (auto* c=project.clipFor(value["id"].toString())) chain(value["plugins"],c->effects->plugins);
}
void Engine::finalisePerformance()
{
    const juce::ScopedLock guard(lock); if (!capturingPerformance()) return;
    performanceActive=false; recordProjectCommit();
    for (const auto& listener:processorCaptures) if (listener->changed.load()) { project.performanceTakes[(size_t)activeTake].complete=false; project.performanceTakes[(size_t)activeTake].issue="A plugin program/opaque state changed. Keep the optional master WAV for the exact audible performance."; }
    for (const auto& listener:parameterCaptures) if (listener->missed.load()) { project.performanceTakes[(size_t)activeTake].complete=false; project.performanceTakes[(size_t)activeTake].issue="A plugin gesture was not captured. Keep the master WAV."; }
    detachPerformanceParameters(); activeWrites.clear(); automationTake=false;
    auto& take=project.performanceTakes[(size_t)activeTake]; take.state=TakeState::finalising; take.frames=performanceFrames.load();
    const auto writerIssue=masterRecorder.finish();
    if (!take.complete && take.issue.isEmpty()) take.issue="The event limit was reached; the editable chronology is incomplete. Keep the master WAV.";
    if (writerIssue.isNotEmpty()) { take.complete=false; take.issue=writerIssue; }
    take.state=TakeState::complete;
    if (onPerformanceFinalised) onPerformanceFinalised(); // Close editors before retiring programme processors.
    const auto initial=take.initial;
    auto takes=std::move(project.performanceTakes);
    project=std::move(*performanceOriginal); performanceOriginal.reset(); project.performanceTakes=std::move(takes); activeTake=-1;
    if (originalEngine) { plugins.swap(originalEngine->plugins); originalEngine.reset(); }
    restoreInitialPluginStates(initial); performanceProjectPending=false; playing=false;
    if (position.load()>=effectAudibleEnd(*this)) position=project.playbackStart(); deviceClockValid=false;
}
void Engine::servicePerformance()
{
    const juce::ScopedLock guard(lock);
    if (queuedProgrammeCommitted) { queuedProgramme.reset(); queuedProgrammeCommitted=false; if (capturingPerformance()) attachPerformanceParameters(); }
    if (!capturingPerformance()) return;
    for (const auto& listener:processorCaptures) if (listener->changed.exchange(false))
    { auto& take=project.performanceTakes[(size_t)activeTake]; take.complete=false; take.issue="A plugin program/opaque state changed. Keep the optional master WAV for the exact audible performance."; }
    if (automationRecordMode==AutomationRecordMode::touch)
    {
        std::vector<AutomationAddress> released;
        for (const auto& write:activeWrites) if (performanceFrames.load()/sampleRate.load()>=write.lastInput+.5) released.push_back(write.address);
        for (const auto& address:released) returnToAutomation(address);
    }
    for (const auto& listener:parameterCaptures) if (listener->missed.exchange(false))
    { auto& take=project.performanceTakes[(size_t)activeTake]; take.complete=false; take.issue="A plugin gesture could not be captured; use the optional master WAV for an exact audible record."; }
    auto& take=project.performanceTakes[(size_t)activeTake]; take.frames=performanceFrames.load();
    if (performanceCallbackMissed.exchange(false)) { take.complete=false; take.issue="An audio callback could not be captured; the take/WAV is incomplete."; }
    if (performanceEnded.exchange(false)) { finalisePerformance(); return; }
    if (sampleRate.load()!=take.sampleRate) { take.complete=false; take.issue="The audio device/rate changed during the take."; finalisePerformance(); }
}
juce::String Engine::performanceStatus() const
{
    if (performanceError.isNotEmpty()) return "Take could not start: "+performanceError;
    if (capturingPerformance()) return juce::String(playing ? "Recording editable take / " : "Take paused / ")+timeText(performanceFrames.load()/sampleRate.load())
        +(masterRecorder.failed() ? " / WAV incomplete" : masterRecorder.active() ? " / master WAV" : "")
        +(project.performanceTakes[(size_t)activeTake].complete ? "" : " / editable capture incomplete: "+project.performanceTakes[(size_t)activeTake].issue);
    if (!project.performanceTakes.empty()) { const auto& t=project.performanceTakes.back(); return t.name+" / "+juce::String((int)t.events.size())+" events / "+(t.complete ? "complete" : t.issue); }
    return workMode==WorkMode::arrange ? "Arrange / master preview" : "Record/Perform / private preview; headphones optional";
}
std::unique_ptr<Engine> Engine::prepareTakeReplay(int index,juce::String& error)
{
    const juce::ScopedLock guard(lock);
    if (index<0 || index>=(int)project.performanceTakes.size()) { error="Select a saved take."; return {}; }
    const auto& take=project.performanceTakes[(size_t)index];
    auto renderer=std::make_unique<Engine>(juce::File(),false); renderer->offlineRenderer=true; renderer->sampleRate=take.sampleRate;
    renderer->blockSize=blockSize; renderer->prepareBuffers(blockSize); renderer->takeBuffer.setSize(2,blockSize);
    error=renderer->restoreProject(juce::JSON::toString(take.initial)); if (error.isNotEmpty()) return {};
    renderer->replayTake=take; renderer->replaySource=take.initialPosition; renderer->replayRunning=true;
    renderer->deviceClockAnchor=take.initialPosition; renderer->deviceClockFrames=0;
    renderer->replayProjects.resize(take.events.size());
    std::map<juce::String,std::shared_ptr<ClipEffects>> owners;
    for (auto& c:renderer->project.clips) { renderer->project.effectSettings(c); owners.emplace(effectsIdentity(c.effects),c.effects); }
    for (size_t i=0;i<take.events.size();++i) if (take.events[i].kind==TakeEventKind::project)
    {
        auto changed=std::make_unique<Engine>(juce::File(),false); changed->sampleRate=take.sampleRate; changed->blockSize=blockSize; changed->prepareBuffers(blockSize);
        error=changed->restoreProject(juce::JSON::toString(take.events[i].payload)); if (error.isNotEmpty()) return {};
        // Carry each original processor through structural edits. New placements
        // own new instances; unchanged originals retain their running tails.
        for (auto& c:changed->project.clips)
        { c.effects=owners.try_emplace(effectsIdentity(c.effects),c.effects).first->second; changed->project.effectSettings(c); }
        renderer->replayProjects[i]=std::move(changed);
    }
    for (const auto& changed:renderer->replayProjects) if (changed) for (const auto& c:changed->project.clips)
    { renderer->clipProcessors.try_emplace(c.id); renderer->clipProcessors.try_emplace(effectsIdentity(c.effects)); }
    for (const auto& event:take.events) if (event.kind==TakeEventKind::clipJump) renderer->replayShifts.try_emplace(event.owner,0.);
    for (const auto& event:take.events) if (event.kind==TakeEventKind::control && event.control==(int)AutomationTarget::plugin)
    {
        renderer->ensurePluginAutomation(eventAddress(renderer->project,event));
        for (const auto& changed:renderer->replayProjects) if (changed) changed->ensurePluginAutomation(eventAddress(changed->project,event));
    }
    renderer->resetPlugins(); return renderer;
}
void Engine::applyTakeEvent(const TakeEvent& event)
{
    const juce::ScopedValueSetter<bool> applying(applyingPerformanceEvent,true);
    auto address=eventAddress(project,event);
    switch (event.kind)
    {
        case TakeEventKind::control:
            setControlValueAt(address,(float)event.value,event.sourcePosition,false); break;
        case TakeEventKind::returnAutomation:
        {
            // Return restores the snapshot's curve, not a later latch state.
            // Parsed clips are references until hydration: use the prepared
            // immutable curve saved alongside the replay before any mutation.
            if (auto* curve=project.curve(address)) curve->enabled=(bool)event.payload && !curve->points.empty();
            project.setManualValue(address,(float)event.value); if (auto* parameter=parameterFor(address)) parameter->setValue((float)event.value);
            break;
        }
        case TakeEventKind::transport: replayRunning=event.value!=0; break;
        case TakeEventKind::seek: replaySource=event.value; deviceClockAnchor=event.value; deviceClockFrames=0; for (auto& shift:replayShifts) shift.second=0; resetPlugins(); break;
        case TakeEventKind::clipJump: replayShifts[event.owner]=event.value; break;
        case TakeEventKind::mute: if (event.lane>=0) project.tracks[(size_t)event.lane].mute=event.value!=0; break;
        case TakeEventKind::solo: if (event.lane>=0) project.tracks[(size_t)event.lane].solo=event.value!=0; break;
        case TakeEventKind::tempo: project.bpm=event.value; project.tempo.fallback=event.value; break;
        case TakeEventKind::project:
            if (replayEvent<replayProjects.size() && replayProjects[replayEvent])
            { std::swap(project,replayProjects[replayEvent]->project); }
            break;
        default: break;
    }
}
void Engine::renderTake(juce::AudioBuffer<float>& output,int frames,double seconds)
{
    if (!replayTake) { juce::MidiBuffer midi; render(output,frames,seconds,midi); return; }
    const auto wanted=(int64_t)std::llround(seconds*sampleRate.load());
    if (wanted!=replayFrame)
    {
        // Reconstruct DSP history by rendering from the beginning, including
        // previous loops/tails. This path runs on the preparation/export worker.
        if (wanted<replayFrame) { output.clear(0,frames); return; }
        while (replayFrame<wanted)
        { const int count=(int)std::min<int64_t>(blockSize,wanted-replayFrame); renderTake(takeBuffer,count,replayFrame/sampleRate.load()); }
    }
    output.clear(0,frames); int offset=0;
    while (offset<frames)
    {
        while (replayEvent<replayTake->events.size() && replayTake->events[replayEvent].frame<=replayFrame)
        { applyTakeEvent(replayTake->events[replayEvent]); ++replayEvent; }
        int count=frames-offset;
        if (replayEvent<replayTake->events.size()) count=(int)std::min<int64_t>(count,replayTake->events[replayEvent].frame-replayFrame);
        count=(int)std::min<int64_t>(count,std::max<int64_t>(0,replayTake->frames-replayFrame));
        if (count<=0) break;
        juce::MidiBuffer midi;
        // The same renderer owns control application and continuous DSP for live
        // audition and offline output. Clip shifts are the committed cue clocks.
        render(mixBuffer,count,replaySource,midi);
        for (int ch=0;ch<2;++ch) output.copyFrom(ch,offset,mixBuffer,ch,0,count);
        if (replayRunning) { deviceClockFrames+=count; replaySource=deviceClockAnchor+deviceClockFrames/sampleRate.load(); }
        replayFrame+=count; offset+=count;
    }
}
juce::Result Engine::auditionTake(int index)
{
    if (playing || capturingPerformance()) return juce::Result::fail("Stop programme playback before auditioning a take.");
    juce::String error; auto renderer=prepareTakeReplay(index,error); if (!renderer) return juce::Result::fail(error);
    return installTakeAudition(std::move(renderer));
}
juce::Result Engine::installTakeAudition(std::unique_ptr<Engine> renderer)
{
    const juce::ScopedLock guard(lock);
    if (playing || capturingPerformance() || !renderer || !renderer->replayingTake()) return juce::Result::fail("Stop programme playback before auditioning a prepared take.");
    if (renderer->sampleRate.load()!=sampleRate.load()) return juce::Result::fail("Select the take's recorded sample rate in Audio / MIDI before live audition. Offline take export keeps its recorded rate.");
    takeAudition=std::move(renderer); return juce::Result::ok();
}
void Engine::endTakeAudition() { takeAudition.reset(); }
void Engine::prepareRender(double rate,int size)
{
    const juce::ScopedLock guard(lock); sampleRate=rate; blockSize=size; prepareBuffers(size); midiCollector.reset(rate);
    eachPlugin([&](auto& slot) { slot.processor->releaseResources(); slot.processor->setRateAndBufferSizeDetails(rate,size); slot.processor->prepareToPlay(rate,size); }); resetPlugins();
}
bool Engine::startPrivatePreview(juce::AudioSource& audio,int pair)
{
    const juce::ScopedLock guard(lock); const auto* device=devices.getCurrentAudioDevice();
    if (!device || pair<2 || !device->getActiveOutputChannels()[pair] || !device->getActiveOutputChannels()[pair+1]) return false;
    privatePreviewSource=&audio; privatePreviewFirstChannel=pair; return true;
}
void Engine::detachPrivatePreview(juce::AudioSource& audio)
{
    const juce::ScopedLock guard(lock); if (privatePreviewSource==&audio) { privatePreviewSource=nullptr; privatePreviewFirstChannel=-1; }
}
juce::Result Engine::queueProgrammeProject(Project candidate,juce::var snapshot,double boundary,juce::String launchClip,double launchShift)
{
    const juce::ScopedLock guard(lock);
    if (!playing || exporting || queuedProgramme || !std::isfinite(boundary) || boundary<position.load()) return juce::Result::fail("Programme launch is busy or the timing boundary is stale.");
    for (const auto& c:candidate.clips) { clipProcessors.try_emplace(c.id); clipProcessors.try_emplace(effectsIdentity(c.effects)); }
    queuedProgramme=std::make_unique<Project>(std::move(candidate)); queuedProgrammeBoundary=boundary; queuedProgrammeCommitted=false;
    queuedProgrammeEvent={}; queuedProgrammeEvent.kind=TakeEventKind::project; queuedProgrammeEvent.payload=std::move(snapshot);
    queuedProgrammeEvent.owner=std::move(launchClip); queuedProgrammeEvent.value=launchShift;
    if (queuedProgrammeEvent.owner.isNotEmpty()) replayShifts.try_emplace(queuedProgrammeEvent.owner,0.);
    return juce::Result::ok();
}
void Engine::cancelPreparedLaunch() { const juce::ScopedLock guard(lock); queuedProgramme.reset(); queuedProgrammeCommitted=false; }
}
