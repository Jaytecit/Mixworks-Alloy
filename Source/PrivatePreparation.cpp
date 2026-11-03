#include "PrivatePreparation.h"
#include "ElasticAudio.h"
namespace mixworks
{
juce::Result PrivatePreparation::prepareTrack(const juce::String& id) { return prepare(id,false); }
juce::Result PrivatePreparation::prepareTransition(const juce::String& id) { return prepare(id,true); }
juce::Result PrivatePreparation::prepare(const juce::String& id,bool transition)
{
    juce::String error; auto copy=programme.createExportRenderer(error); if (!copy) return juce::Result::fail(error);
    baseline=copy->project;
    const auto* clip=copy->project.clipFor(id);
    if (!clip && !transition)
    {
        const auto found=std::find_if(copy->library.begin(),copy->library.end(),[&](const auto& m) { return m->id==id; });
        if (found!=copy->library.end() && (*found)->hasPlayback())
        { Clip item; item.id=id; item.media=*found; item.track=programme.selectedTrack; item.length=item.media->duration(); copy->project.clips.push_back(item); clip=copy->project.clipFor(id); }
    }
    if (!clip) return juce::Result::fail("Prepare this library source's audio before private editing.");
    const auto* link=transition ? copy->project.transitionFor(id) : nullptr;
    if (transition && !link) return juce::Result::fail("Select a complete transition to prepare privately.");
    sourceClip=id; incoming=transition ? id : juce::String(); sourceAudition=false;
    auto bounds=transition ? copy->project.transitionRange(*link) : juce::Range<double>(clip->start,clip->end());
    const auto outgoing=link ? link->outgoing : juce::String();
    copy->project.clips.erase(std::remove_if(copy->project.clips.begin(),copy->project.clips.end(),[&](const auto& c) { return c.id!=id && c.id!=outgoing; }),copy->project.clips.end());
    copy->project.transitionLinks.erase(std::remove_if(copy->project.transitionLinks.begin(),copy->project.transitionLinks.end(),[&](const auto& l) { return !transition || l.incoming!=id; }),copy->project.transitionLinks.end());
    const double context=copy->project.gridClock().timeAtBeat(copy->project.gridClock().beatsAt(bounds.getStart())-4);
    const juce::ScopedLock guard(mutex); renderer=std::move(copy); begin=std::max(0.,context); end=std::min(renderer->project.audibleEnd(),bounds.getEnd()+2);
    position=begin; playing=true; buffer.setSize(2,renderer->preparedBlockSize()); return juce::Result::ok();
}
juce::Result PrivatePreparation::alternative(int slot)
{
    const juce::ScopedLock guard(mutex);
    if (!renderer || incoming.isEmpty() || slot<0 || slot>2) return juce::Result::fail("Prepare a transition first.");
    const auto* link=baseline.transitionFor(incoming);
    if (!link || !link->alternatives[(size_t)slot]) return juce::Result::fail("That private A/B/C alternative is empty.");
    const auto result=renderer->project.applyTransitionStyle(incoming,*link->alternatives[(size_t)slot]);
    if (result.wasOk()) { renderer->prepareClipProcessing(); renderer->resetPlugins(); position=begin; }
    return result;
}
juce::Result PrivatePreparation::prepareSourceAudition()
{
    const juce::ScopedLock guard(mutex); renderer=std::make_unique<Engine>(juce::File(),false); renderer->prepareRender(programme.sampleRate.load(),programme.preparedBlockSize());
    sourceAudition=true; sourceClip.clear(); incoming.clear(); buffer.setSize(2,programme.preparedBlockSize()); return juce::Result::ok();
}
juce::Result PrivatePreparation::setControl(const AutomationAddress& address,float value)
{
    const juce::ScopedLock guard(mutex);
    if (renderer && address.target==AutomationTarget::plugin) renderer->ensurePluginAutomation(address);
    if (!renderer || !renderer->setControlValue(address,value)) return juce::Result::fail("That private control is unavailable.");
    return juce::Result::ok();
}
void PrivatePreparation::seek(double seconds) { const juce::ScopedLock guard(mutex); if (renderer) { position=juce::jlimit(begin,end,seconds); renderer->resetPlugins(); } }
void PrivatePreparation::discard() { const juce::ScopedLock guard(mutex); renderer.reset(); sourceClip.clear(); incoming.clear(); }
void PrivatePreparation::prepareToPlay(int size,double rate)
{
    const juce::ScopedLock guard(mutex); outputRate=rate; buffer.setSize(2,size);
    if (renderer) renderer->prepareRender(rate,size);
}
void PrivatePreparation::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
    OriginalAudio::Realtime realtimeReads;
    info.clearActiveBufferRegion(); const juce::ScopedTryLock guard(mutex);
    if (!guard.isLocked() || !renderer || !playing) return;
    if (sourceAudition)
    {
        const juce::ScopedTryLock renderGuard(renderer->lock); if (!renderGuard.isLocked()) return;
        int done=0; while (done<info.numSamples) { const int count=std::min(info.numSamples-done,buffer.getNumSamples()); renderer->gridAudition.render(buffer,count,outputRate); for (int ch=0;ch<std::min(2,info.buffer->getNumChannels());++ch) info.buffer->copyFrom(ch,info.startSample+done,buffer,ch,0,count); done+=count; } return;
    }
    int done=0;
    while (done<info.numSamples)
    {
        if (position>=end) { if (!loop) { playing=false; break; } position=begin; renderer->resetPlugins(); }
        const int count=std::min({info.numSamples-done,renderer->preparedBlockSize(),(int)std::max(1.,std::ceil((end-position)*outputRate))});
        if (!renderer->requestPlayback(position.load(), count)) return;
        juce::MidiBuffer midi; renderer->playing=true; renderer->position=position.load(); renderer->render(buffer,count,position,midi);
        for (int ch=0;ch<std::min(2,info.buffer->getNumChannels());++ch) info.buffer->copyFrom(ch,info.startSample+done,buffer,ch,0,count);
        position=position+count/outputRate; done+=count;
    }
}
juce::Result PrivatePreparation::apply(bool launch)
{
    const juce::ScopedLock guard(programme.lock); const juce::ScopedLock privateGuard(mutex);
    if (!renderer) return juce::Result::fail("Prepare a private track or transition first.");
    const auto* original=baseline.clipFor(sourceClip); const auto* current=programme.project.clipFor(sourceClip);
    if (!original && !current && incoming.isEmpty())
    {
        const auto* prepared=renderer->project.clipFor(sourceClip); if (!prepared) return juce::Result::fail("The private library source is unavailable.");
        const auto found=std::find_if(programme.library.begin(),programme.library.end(),[&](const auto& m) { return m->id==sourceClip && m->analysisSource==prepared->media->analysisSource && m->file==prepared->media->file; });
        if (found==programme.library.end()) return juce::Result::fail("The library source changed. Prepare it again.");
        const auto boundary=programme.playing && launch ? nextLaunchBoundary(programme.project.gridClock(),programme.project.launch,programme.position.load()) : std::optional<LaunchBoundary>(LaunchBoundary{programme.position.load(),0});
        if (!boundary) return juce::Result::fail("No valid programme launch boundary is available.");
        auto candidate=programme.project; auto added=*prepared; added.id=juce::Uuid().toString();
        const auto settings=renderer->project.effectSettings(*prepared); added.controls=settings;
        const auto cloned=programme.cloneClipEffects(added); if (cloned.isNotEmpty()) return juce::Result::fail(cloned);
        added.offset=prepared->sourceTimeAt(std::max(0.,position.load()-prepared->start)); added.length=std::max(0.,(added.media->duration()-added.offset)/added.rate); added.effectOffset=0; added.start=boundary->time;
        if (added.length<1./added.media->sampleRate) return juce::Result::fail("The private source cursor is at the ending. Seek earlier before launching.");
        candidate.clips.push_back(added); candidate.effectSettings(candidate.clips.back())=added.controls;
        std::swap(programme.project,candidate); auto snapshot=programme.captureProject(true); std::swap(programme.project,candidate); snapshot.getDynamicObject()->removeProperty("performanceTakes");
        if (launch && programme.playing) return programme.queueProgrammeProject(std::move(candidate),snapshot,boundary->time);
        programme.project=std::move(candidate); programme.prepareClipProcessing(); programme.performanceProjectPending=true; programme.recordProjectCommit(); return juce::Result::ok();
    }
    if (!original || !current || original->media!=current->media || original->start!=current->start || original->offset!=current->offset || original->length!=current->length || effectsIdentity(original->effects)!=effectsIdentity(current->effects))
        return juce::Result::fail("The programme target changed. Prepare it again before Apply/Launch.");
    Project candidate=programme.project;
    if (!incoming.isEmpty())
    {
        TransitionStyle style; auto result=renderer->project.captureTransitionStyle(incoming,style); if (result.failed()) return result;
        result=candidate.applyTransitionStyle(incoming,style); if (result.failed()) return result;
    }
    else
    {
        auto* target=candidate.clipFor(sourceClip); const auto* prepared=renderer->project.clipFor(sourceClip);
        candidate.effectSettings(*target)=renderer->project.effectSettings(*prepared);
    }
    for (const auto& prepared:renderer->project.clips)
    {
        const auto* before=baseline.clipFor(prepared.id); auto* target=candidate.clipFor(prepared.id);
        if (!before || !target || before->media!=target->media || before->start!=target->start || before->offset!=target->offset || before->length!=target->length || effectsIdentity(before->effects)!=effectsIdentity(target->effects))
            return juce::Result::fail("A private transition participant changed. Prepare it again before Apply/Launch.");
        candidate.effectSettings(*target)=renderer->project.effectSettings(prepared);
    }
    std::swap(programme.project,candidate); auto snapshot=programme.captureProject(true); std::swap(programme.project,candidate);
    snapshot.getDynamicObject()->removeProperty("performanceTakes");
    if (!launch || !programme.playing)
    {
        programme.project=std::move(candidate); programme.prepareClipProcessing(); programme.performanceProjectPending=true; programme.recordProjectCommit();
        return juce::Result::ok();
    }
    const auto boundary=nextLaunchBoundary(programme.project.gridClock(),programme.project.launch,programme.position.load());
    if (!boundary) return juce::Result::fail("No valid programme launch boundary is available.");
    return programme.queueProgrammeProject(std::move(candidate),snapshot,boundary->time,incoming.isEmpty() ? sourceClip : juce::String(),incoming.isEmpty() ? position.load()-boundary->time : 0);
}
}
