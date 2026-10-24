#include "Workstation.h"
#include "ElasticAudio.h"
#include "Beatmatch.h"
#include "AutoArrangement.h"
#include <cmath>
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace mixworks
{
void runIndependentTimelineTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    auto ui = std::make_unique<Workstation>(false); auto& w = *ui;
    w.setSize(1250,1000); w.engine.sampleRate = 8000;
    auto media = std::make_shared<Media>(); media->name = "Independent track"; media->sampleRate = 8000;
    media->manualBpm = 120; media->manualBeatOrigin = 0; media->samples.setSize(1,1280000);
    for (int i=0;i<media->frames();++i) media->samples.setSample(0,i,.06f+.02f*std::sin(i*.071f));
    media->file = report.getSiblingFile("independent-source.wav");
    {
        juce::WavAudioFormat wav; auto stream=media->file.createOutputStream();
        std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(),8000,1,24,{},0));
        if (writer) stream.release();
        check(writer && writer->writeFromAudioSampleBuffer(media->samples,0,media->frames()),"write small isolated source");
    }
    auto json=[](const Project& p) { return juce::JSON::toString(projectToJson(p)); };
    w.engine.library={media};
    bool placed=true;
    for (int lane=0;lane<4;++lane)
    {
        w.insertTimelineMedia({media},TimelinePlacement{lane,4});
        auto result=planTempoMap(w.engine.project,128,TransitionPreset::crossfade,true);
        placed &= result.error.isEmpty(); if (result.error.isEmpty()) w.engine.project=std::move(result.project);
    }
    check(placed && w.engine.project.clips.size()==4 && w.engine.project.transitionLinks.empty()
        && std::all_of(w.engine.project.clips.begin(),w.engine.project.clips.end(),[](const auto& c) { return std::abs(c.start-4)<1.e-7 && c.fadeIn==0 && c.fadeOut==0; }),
        "actual insertion and tempo planning allow four independent tracks at the chosen overlapping drop time");
    const auto placedProject=w.engine.project;
    const auto moved=w.engine.project.moveClip(w.engine.project.clips[1].id,12,false);
    check(moved.wasOk() && w.engine.project.clips[0].start==4 && w.engine.project.clips[2].start==4 && w.engine.project.transitionLinks.empty(),
        "moving one newly placed track leaves every other track independent");
    const auto anchored=w.engine.project.moveTransitionAnchor(w.engine.project.clips[0].id,false,30,false);
    check(anchored.wasOk() && w.engine.project.transitionLinks.empty() && w.engine.project.clips[1].start==12,"IN/OUT edits do not create transitions");
    w.engine.project=placedProject; w.selectedClip=w.engine.project.clips[0].id; w.engine.position=80; w.clearEditSelection(); w.splitClip();
    check(w.engine.project.clips.size()==5 && w.engine.project.transitionLinks.empty(),"actual Split leaves both pieces independent");
    Project base; base.followTrackTempo=false; base.masterDb=0;
    for (int lane=0;lane<4;++lane) { Clip c; c.media=media; c.length=160; c.track=lane; base.clips.push_back(c); }
    auto render=[&](Project p,double at,int solo=-1)
    {
        auto e=std::make_unique<Engine>(); e->sampleRate=8000; e->project=std::move(p);
        if (solo>=0) e->project.tracks[(size_t)solo].solo=true;
        juce::AudioBuffer<float> audio(2,173); juce::MidiBuffer midi; e->render(audio,173,at,midi); return audio;
    };
    auto error=[&](const Project& a,const Project& b,double at,int solo=-1)
    {
        const auto x=render(a,at,solo),y=render(b,at,solo); float worst=0;
        for(int c=0;c<2;++c) for(int i=0;i<x.getNumSamples();++i) worst=std::max(worst,std::abs(x.getSample(c,i)-y.getSample(c,i))); return worst;
    };
    w.engine.project=base; w.clearEditSelection(); w.undoHistory.clear(); w.redoHistory.clear();
    w.selectedClip=base.clips[1].id; w.editSelection.primary=w.selectedClip; w.editSelection.clips={base.clips[0].id,base.clips[1].id};
    w.transitionPreset.setSelectedId(1,juce::dontSendNotification); w.refresh(); w.addTransitionButton.onClick();
    check(w.engine.project.transitionLinks.size()==1 && w.undoHistory.size()==1 && w.engine.project.clips[1].start==0,
        "dedicated Add Transition links the selected pair in one Undo without moving audio");
    w.undo(false); check(w.engine.project.transitionLinks.empty(),"Undo removes the explicit transition"); w.undo(true);
    auto linked=w.engine.project;
    bool overlapping=true;
    for (int n=1;n<3;++n) overlapping &= linked.addTransition(base.clips[(size_t)n].id,base.clips[(size_t)n+1].id,8,TransitionPreset::alignOnly).wasOk();
    for (int n=1;n<4;++n)
    {
        const auto id=base.clips[(size_t)n].id; const auto frame=linked.transitionRange(*linked.transitionFor(id));
        overlapping &= linked.editTransition(id,Project::TransitionEdit::resizeStart,-frame.getStart(),false).wasOk();
    }
    check(overlapping && linked.validateTransitionLinks().wasOk() && linked.transitionRange(linked.transitionLinks[0]).getLength()==160,
        "three transition windows span the same entire four-lane overlap, exceeding 64 bars");
    check(error(base,linked,1)<1.e-6f && error(base,linked,159)<1.e-6f,"unmuted transitions retain all four lanes' production PCM");
    const auto id=base.clips[1].id;
    auto narrowed=linked;
    const auto a=narrowed.editTransition(id,Project::TransitionEdit::resizeStart,40,false);
    const auto b=narrowed.editTransition(id,Project::TransitionEdit::resizeEnd,-40,false);
    check(a.wasOk() && b.wasOk() && narrowed.clips[1].start==0 && narrowed.transitionRange(*narrowed.transitionFor(id))==juce::Range<double>(40,120),
        "frame resizing changes only the bounds and permits overlapping neighbour frames");
    const auto before=json(narrowed);
    auto suffix=narrowed; const auto following=suffix.moveClipFollowing(id,5,false);
    check(following.wasOk() && suffix.clips[0].start==0 && suffix.clips[1].start==5 && suffix.clips[2].start==5 && suffix.clips[3].start==5
        && suffix.transitionRange(*suffix.transitionFor(id))==juce::Range<double>(40,120),"following-track control retains its fixed incoming frame and moves the complete suffix");
    check(narrowed.editTransition(id,Project::TransitionEdit::resizeEnd,41,false).failed() && json(narrowed)==before,"audio end is the transactional outer bound");
    auto startBound=narrowed;
    check(startBound.editTransition(id,Project::TransitionEdit::resizeStart,-41,false).wasOk()
        && startBound.transitionRange(*startBound.transitionFor(id)).getStart()==0,"frame clamps at audio start without moving any track");
    w.engine.project=narrowed; w.selectedTransition=w.selectedClip=id; w.clearEditSelection(); w.undoHistory.clear(); w.redoHistory.clear(); w.refresh();
    w.transitionMuteOutside.setToggleState(true,juce::dontSendNotification); w.transitionMuteOutside.onClick();
    const auto muted=w.engine.project;
    check(muted.transitionFor(id)->muteOutside && w.undoHistory.size()==1,"mute-outside toggle is optional and creates one Undo");
    check(render(muted,1,1).getMagnitude(0,173)<1.e-8f && render(muted,140,0).getMagnitude(0,173)<1.e-8f
        && render(narrowed,1,1).getMagnitude(0,173)>.01f && render(narrowed,140,0).getMagnitude(0,173)>.01f,
        "production PCM mutes only the requested incoming head and outgoing tail");
    check(error(narrowed,muted,80)<1.e-6f && error(narrowed,muted,1,2)<1.e-6f,"muting preserves inside audio and other lanes");
    w.undo(false); check(!w.engine.project.transitionFor(id)->muteOutside,"Undo restores exterior audio"); w.undo(true);
    const auto parsed=parseProject(json(w.engine.project));
    report.getSiblingFile("independent-document.json").replaceWithText(json(w.engine.project));
    check(parsed.error.isEmpty() && parsed.project.transitionFor(id) && parsed.project.transitionFor(id)->muteOutside
        && parsed.project.transitionFor(id)->frame==muted.transitionFor(id)->frame,"current saved format round-trips frame bounds and optional muting: "+parsed.error);
    auto locked=narrowed; locked.setTransitionLocked(id,true); const auto frozen=json(locked);
    check(locked.setTransitionMuteOutside(id,true).failed() && locked.editTransition(id,Project::TransitionEdit::resizeEnd,1,false).failed() && json(locked)==frozen,
        "existing transition lock protects both frame and mute edits");
    auto nodes=narrowed; const auto preset=nodes.applyTransitionPreset(id,TransitionPreset::crossfade);
    check(preset.wasOk() && nodes.transitionRange(*nodes.transitionFor(id))==juce::Range<double>(40,120)
        && error(narrowed,nodes,1)<1.e-6f && error(narrowed,nodes,140)<1.e-6f,"existing crossfade control preserves explicit bounds and exterior audio when muting is off");
    const auto arranged=prepareOrderedTimeline(placedProject,0);
    check(arranged.error.isEmpty() && arranged.project.transitionLinks.size()==3 && arranged.project.validateTransitionLinks().wasOk(),"Auto Arrange still creates transitions: "+arranged.error);
    for (int width : {1250,1920})
    {
        w.engine.project=width==1250?narrowed:muted; w.selectedTransition=w.selectedClip=id; w.setSize(width,1000); w.consoleCollapsed=false; w.toolPage=1;
        w.timeline.pixelsPerSecond=3; w.refresh(); w.resized(); w.transitionViewport.setViewPosition(0,0);
        juce::PNGImageFormat png; auto stream=report.getSiblingFile("independent-"+juce::String(width)+".png").createOutputStream();
        check(stream && png.writeImageToStream(w.createComponentSnapshot(w.getLocalBounds()),*stream),"capture Add Transition, mute toggle and four overlapping lanes at "+juce::String(width));
    }
}
void runUnlinkTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    auto ui = std::make_unique<Workstation>(false); auto& w = *ui;
    w.setSize(1250, 900); w.engine.sampleRate = 8000;
    auto media = std::make_shared<Media>(); media->name = "Spoken piece"; media->sampleRate = 8000;
    media->manualBpm = 120; media->manualBeatOrigin = 0;
    media->file = report.getSiblingFile("unlink-source.wav"); media->samples.setSize(1, 160000);
    for (int i = 0; i < media->frames(); ++i) media->samples.setSample(0, i, .12f + .03f * std::sin(i * .071f));
    {
        juce::WavAudioFormat wav; auto stream = media->file.createOutputStream();
        std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(), 8000, 1, 24, {}, 0));
        if (writer) stream.release();
        check(writer && writer->writeFromAudioSampleBuffer(media->samples, 0, media->frames()), "write isolated unlink source");
    }
    auto json = [](const Project& p) { return juce::JSON::toString(projectToJson(p)); };
    Project base; base.followTrackTempo = false; base.transitionLinksKnown = true; base.masterDb = 0;
    Clip backing; backing.media = media; backing.length = 20; backing.track = 0; base.clips.push_back(backing);
    Clip voice; voice.media = media; voice.start = 4; voice.length = 12; voice.track = 1;
    voice.introAnchor = 0; voice.outroAnchor = 12;
    voice.controls.automation[0].enabled = true;
    voice.controls.automation[0].points = {{0,-160,0,false,true},{1,-3,0,false,true},{11,-3,0,false,true},{12,-160,0,false,true}};
    base.clips.push_back(voice);
    w.engine.project = base; w.selectedClip = voice.id; w.engine.position = 8; w.refresh(); w.splitClip();
    w.engine.position = 12; w.splitClip();
    check(w.engine.project.clips.size() == 4 && w.engine.project.transitionLinks.size() == 2, "actual Split creates the three linked spoken pieces");
    const auto split = w.engine.project; const auto middle = split.clips[2].id; const auto last = split.clips[3].id;
    w.selectedClip = middle; w.clearEditSelection(); w.undoHistory.clear(); w.redoHistory.clear(); w.refresh();
    check(w.editingUnlink.isEnabled(), "Unlink control is enabled for the linked middle piece");
    w.editingUnlink.onClick();
    const auto independent = w.engine.project;
    check(independent.transitionLinks.empty() && w.undoHistory.size() == 1 && independent.clips.size() == 4,
        "Unlink button detaches both sides in one Undo without removing audio");
    check(w.editSelection.clips.empty() && !w.editingUnlink.isEnabled(), "unlink clears group selection so the next drag is individual");
    w.undo(false); check(json(w.engine.project) == json(split), "Undo exactly restores original linked split chain");
    w.undo(true); check(json(w.engine.project) == json(independent), "Redo restores independent pieces");
    auto moved = independent;
    check(moved.moveClip(middle, 9, false).wasOk() && moved.clipFor(middle)->start == 9
        && moved.clipFor(voice.id)->start == 4 && moved.clipFor(last)->start == 12 && moved.clipFor(backing.id)->start == 0
        && moved.transitionLinks.empty(), "ordinary movement moves only the unlinked middle piece and cannot silently relink it");
    const auto parsed = parseProject(json(independent));
    check(parsed.error.isEmpty(), "unlinked arrangement validates in the existing saved format");
    auto group = independent;
    check(group.moveClipGroup({voice.id,middle,last}, middle, 10, 1, false).wasOk()
        && group.clipFor(voice.id)->start == 6 && group.clipFor(last)->start == 14 && group.clipFor(backing.id)->start == 0,
        "Ctrl-selected independent pieces can still move as a rigid group over the backing track");
    auto reset = [&] { w.engine.project = split; w.selectedClip = middle; w.clearEditSelection(); w.undoHistory.clear(); w.redoHistory.clear(); w.engine.playing = false; w.refresh(); };
    reset(); w.engine.playing = true; w.performTimelineAction(TimelineAction::unlinkSelection, 0, 1);
    check(json(w.engine.project) == json(split) && w.undoHistory.empty(), "playback rejects unlink without mutation");
    reset(); w.engine.project.transitionLinks[0].locked = true; const auto locked = json(w.engine.project); w.editingUnlink.onClick();
    check(json(w.engine.project) == locked && w.undoHistory.empty(), "locked linked partner rejects the whole unlink");
    reset(); w.setEditRange(8.5, 9); w.performTimelineAction(TimelineAction::unlinkSelection, 0, 1);
    check(json(w.engine.project) == json(split) && w.undoHistory.empty(), "partial audio range refuses whole-piece unlink");
    reset(); w.editSelection.primary = middle; w.editSelection.clips = {voice.id,middle,last}; w.refresh(); w.editingUnlink.onClick();
    check(w.engine.project.transitionLinks.empty() && w.undoHistory.size() == 1, "selected pieces unlink together in one transaction");
    auto render = [&](const Project& project)
    {
        auto engine = std::make_unique<Engine>(); engine->sampleRate = 8000; engine->project = project;
        juce::AudioBuffer<float> result(2,160000), block(2,173); juce::MidiBuffer midi;
        for (int offset = 0; offset < result.getNumSamples(); offset += 173)
        {
            const int count = std::min(173,result.getNumSamples()-offset); engine->render(block,count,offset/8000.,midi);
            for (int ch = 0; ch < 2; ++ch) result.copyFrom(ch,offset,block,ch,0,count);
        }
        return result;
    };
    auto error = [&](const Project& before, const Project& after)
    {
        const auto a=render(before), b=render(after); float difference=0;
        for (int ch=0;ch<2;++ch) for(int i=0;i<a.getNumSamples();++i) difference=std::max(difference,std::abs(a.getSample(ch,i)-b.getSample(ch,i)));
        return difference;
    };
    check(error(split,independent)<1.e-6f, "unlinking split speech with current node fades preserves production rendered PCM");
    Project overlap = base; overlap.clips.clear();
    for (int side=0;side<3;++side)
    {
        Clip c; c.media=media; c.track=side; c.start=side==0?0:side==1?1:4; c.length=8;
        c.introAnchor=side==0?0:(side==1?2.0000625:5.0000625)-c.start;
        c.outroAnchor=(side==0?2.0000625:side==1?5.0000625:c.end())-c.start;
        c.fadeInAnchor=0; c.fadeOutAnchor=(side==0?3.7500625:side==1?6.1250625:c.end())-c.start;
        overlap.clips.push_back(c);
    }
    overlap.transitionLinks={{overlap.clips[0].id,overlap.clips[1].id},{overlap.clips[1].id,overlap.clips[2].id}};
    auto detached=overlap; const auto result=detached.unlinkClips({overlap.clips[1].id});
    check(result.wasOk() && detached.transitionLinks.empty() && error(overlap,detached)<1.e-6f,
        "unlink preserves fractional-sample transition hard cuts without exposing silent heads or tails: "+result.getErrorMessage());
    for (int width : {1250,1920})
    {
        reset(); w.setSize(width,1000); w.consoleCollapsed=false; w.toolPage=2; w.timeline.pixelsPerSecond=35; w.resized();
        w.editingViewport.setViewPosition(0,std::max(0,w.editingRange.getY()-10));
        juce::PNGImageFormat png; auto stream=report.getSiblingFile("unlink-linked-"+juce::String(width)+".png").createOutputStream();
        check(stream && png.writeImageToStream(w.createComponentSnapshot(w.getLocalBounds()),*stream),"capture LINK group identifiers and Unlink control at "+juce::String(width));
    }
}
void runEditSelectionTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    auto window = std::make_unique<Workstation>(false); auto& w = *window;
    w.setSize(1250, 900); w.engine.sampleRate = 8000;
    Project base; base.followTrackTempo = false; base.masterDb = 0; base.transitionLinksKnown = true;
    for (int n = 0; n < 2; ++n)
    {
        auto media = std::make_shared<Media>(); media->name = "Range source " + juce::String(n + 1);
        media->sampleRate = 8000; media->manualBpm = 120; media->manualBeatOrigin = 0;
        media->file = report.getSiblingFile("selection-source-" + juce::String(n) + ".wav");
        media->samples.setSize(1, 160000);
        for (int i = 0; i < media->frames(); ++i) media->samples.setSample(0, i, .25f * (float) std::sin(i * .04 + n));
        juce::WavAudioFormat format; auto stream = media->file.createOutputStream();
        std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.get(), 8000, 1, 24, {}, 0));
        if (writer) { stream.release(); writer->writeFromAudioSampleBuffer(media->samples, 0, media->frames()); }
        writer.reset();
        Clip c; c.media = media; c.track = n; c.length = 20; base.clips.push_back(c);
    }
    auto json = [](const Project& p) { return juce::JSON::toString(projectToJson(p)); };
    auto pump = []
    {
        for (int i = 0; i < 10; ++i)
        {
#if JUCE_WINDOWS
            MSG message;
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
#endif
            juce::Thread::sleep(5);
        }
    };
    auto reset = [&]
    {
        w.clearEditSelection(); w.editClipboard.reset(); w.clipClipboard.reset(); w.engine.project = base;
        w.engine.playing = false; w.engine.position = 3.125; w.engine.setAutomationRecordMode(AutomationRecordMode::read);
        w.selectedClip = base.clips.front().id; w.engine.selectedTrack = 0; w.undoHistory.clear(); w.redoHistory.clear(); w.dirty = false;
        w.editingRange.setToggleState(false, juce::dontSendNotification); w.refresh();
    };
    reset(); const auto initial = json(base);
    w.setEditRange(6, 2);
    check(w.editSelection.range && w.editSelection.range->getStart() == 2 && w.editSelection.range->getEnd() == 6
        && w.engine.position == 3.125 && !w.dirty && w.undoHistory.empty() && json(w.engine.project) == initial,
        "range selection normalises reversed endpoints without project/transport/dirty/history mutation");
    w.performTimelineAction(TimelineAction::copy, 0, 0);
    check(w.editClipboard && w.editClipboard->clips.size() == 1 && w.editClipboard->clips[0].offset == 2
        && w.editClipboard->clips[0].length == 4 && w.editClipboard->clips[0].effects != base.clips[0].effects
        && json(w.engine.project) == initial && w.undoHistory.empty(), "range Copy slices source and clones effect ownership without mutating source/project");
    w.performTimelineAction(TimelineAction::paste, 25, 2);
    const auto* pasted = w.currentClip();
    check(pasted && pasted->start == 25 && pasted->length == 4 && pasted->offset == 2 && pasted->track == 2
        && w.engine.project.clips.size() == 3 && w.undoHistory.size() == 1 && pasted->effects != w.editClipboard->clips[0].effects,
        "range Paste uses destination lane/playhead with separately owned effects and one Undo");
    w.undo(false); check(json(w.engine.project) == initial, "range Paste Undo restores exact project");
    w.undo(true); check(w.engine.project.clips.size() == 3, "range Paste Redo restores pasted audio");
    check(w.engine.position == 29, "range paste advances the cursor to its exact end");
    w.performTimelineAction(TimelineAction::paste, w.engine.position, 2);
    check(w.currentClip() && w.currentClip()->start == 29 && w.currentClip()->end() == 33 && w.engine.position == 33,
        "repeated range paste appends at the previous paste end");
    reset(); w.setEditRange(2, 6); w.performTimelineAction(TimelineAction::copy, 0, 0);
    w.performTimelineAction(TimelineAction::paste, 5, 0);
    check(json(w.engine.project) == initial && w.undoHistory.empty() && w.engine.position == 3.125, "range Paste refuses occupied destination without moving the cursor");

    for (bool reverse : {false, true}) for (bool loop : {false, true})
    {
        reset(); w.engine.project.clips[0].reversed = reverse;
        if (loop) { w.engine.project.clips[0].loopSpan = .73; w.engine.project.clips[0].loopPhase = .13; }
        const auto before = json(w.engine.project);
        juce::MidiBuffer midi; juce::AudioBuffer<float> original(2, 160000), edited(2, 160000);
        auto render = [&](juce::AudioBuffer<float>& target)
        {
            auto engine = std::make_unique<Engine>(); engine->sampleRate = 8000; engine->project = w.engine.project;
            // Isolate the selected source; other-lane independence is checked separately.
            engine->project.clips.erase(engine->project.clips.begin() + 1, engine->project.clips.end());
            juce::AudioBuffer<float> block(2, 173);
            for (int offset = 0; offset < target.getNumSamples(); offset += 173)
            {
                const int count = std::min(173, target.getNumSamples() - offset);
                engine->render(block, count, offset / 8000., midi);
                for (int ch = 0; ch < 2; ++ch) target.copyFrom(ch, offset, block, ch, 0, count);
            }
        };
        render(original); w.setEditRange(2.125, 6.375);
        w.performTimelineAction(TimelineAction::remove, 0, 0);
        check(w.engine.project.clips.size() == 3 && w.engine.project.clipFor(base.clips[1].id)->start == 0 && w.undoHistory.size() == 1,
            "range Delete leaves two selected-source pieces and other lane unchanged / reverse " + juce::String((int) reverse) + " loop " + juce::String((int) loop));
        // The new right piece follows the unchanged other-lane clip in storage.
        auto other = *w.engine.project.clipFor(base.clips[1].id);
        w.engine.project.clips.erase(std::remove_if(w.engine.project.clips.begin(), w.engine.project.clips.end(), [&](const auto& c) { return c.id == other.id; }), w.engine.project.clips.end());
        auto outputEngine = std::make_unique<Engine>(); outputEngine->sampleRate = 8000; outputEngine->project = w.engine.project;
        juce::AudioBuffer<float> block(2, 173);
        for (int offset = 0; offset < edited.getNumSamples(); offset += 173)
        {
            const int count = std::min(173, edited.getNumSamples() - offset); outputEngine->render(block, count, offset / 8000., midi);
            for (int ch = 0; ch < 2; ++ch) edited.copyFrom(ch, offset, block, ch, 0, count);
        }
        float maximum = 0;
        for (int i = 0; i < 160000; ++i)
            maximum = std::max(maximum, std::abs(edited.getSample(0, i) - (i >= 17000 && i < 51000 ? 0.f : original.getSample(0, i))));
        check(maximum < 1.e-5f, "actual PCM range deletion preserves exterior source phase and silence inside gap; max " + juce::String(maximum, 8));
        w.engine.project.clips.push_back(other);
        w.undo(false); check(json(w.engine.project) == before, "range Delete Undo restores exact loop/reverse/source clocks");
    }
    reset(); w.selectAllSegments();
    check(w.editSelection.clips.size() == 2 && json(w.engine.project) == initial && !w.dirty, "Select all selects segment set without document mutation");
    w.performTimelineAction(TimelineAction::copy, 0, 0); w.performTimelineAction(TimelineAction::paste, 24, 2);
    check(w.engine.project.clips.size() == 4 && w.editSelection.clips.size() == 2 && w.engine.project.clips[2].track == 2
        && w.engine.project.clips[3].track == 3 && w.undoHistory.size() == 1, "group Paste retains relative lanes/start offsets and one Undo");
    w.undo(false); check(json(w.engine.project) == initial, "group Paste Undo restores exact arrangement");
    w.performTimelineAction(TimelineAction::paste, 24, 3);
    check(json(w.engine.project) == initial && w.undoHistory.empty(), "group Paste refuses lane overflow without partial paste/history");
    reset(); w.selectAllSegments(); w.performTimelineAction(TimelineAction::cut, 0, 0);
    check(w.engine.project.clips.empty() && w.editClipboard && w.editClipboard->clips.size() == 2 && w.undoHistory.size() == 1,
        "group Cut removes selected segments atomically and keeps copied sources");
    w.undo(false); check(json(w.engine.project) == initial, "group Cut Undo restores source identities/clocks");
    reset(); juce::Component::unfocusAllComponents();
    check(w.keyPressed(juce::KeyPress('A', juce::ModifierKeys::ctrlModifier, 0)) && w.editSelection.clips.size() == 2,
        "Ctrl+A dispatch selects segment group through the actual workstation keyboard owner");
    check(w.keyPressed(juce::KeyPress('C', juce::ModifierKeys::ctrlModifier, 0)) && w.editClipboard && w.editClipboard->clips.size() == 2,
        "Ctrl+C dispatch copies the active group");
    w.engine.position = 24; w.engine.selectedTrack = 2;
    check(w.keyPressed(juce::KeyPress('V', juce::ModifierKeys::ctrlModifier, 0)) && w.engine.project.clips.size() == 4 && w.undoHistory.size() == 1,
        "Ctrl+V dispatch pastes the group to the chosen empty lanes");
    w.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey));
    check(w.engine.project.clips.size() == 2 && w.undoHistory.size() == 2, "keyboard Delete targets the pasted group rather than just its active member");
    reset(); w.setEditRange(2, 6); w.engine.playing = true;
    w.performTimelineAction(TimelineAction::cut, 0, 0);
    check(json(w.engine.project) == initial && w.undoHistory.empty() && !w.editClipboard, "playing range Cut refuses without changing clipboard/document");
    w.engine.playing = false;
    reset(); w.engine.project.clips[1].start = 12;
    w.engine.project.ensureTransitionLinks();
    check(w.engine.project.linkClips(base.clips[0].id, base.clips[1].id).wasOk(), "range transition fixture owns a valid linked pair");
    const auto linked = json(w.engine.project);
    w.setEditRange(2, 6); w.performTimelineAction(TimelineAction::remove, 0, 0);
    check(json(w.engine.project) == linked && w.undoHistory.empty(), "partial range deletion through linked transition ownership refuses atomically");
    w.performTimelineAction(TimelineAction::copy, 0, 0);
    check(w.editClipboard && w.editClipboard->clips.size() == 1 && json(w.engine.project) == linked,
        "range copy within a linked source captures ordinary curves without touching source relationship");
    w.selectAllSegments(); w.performTimelineAction(TimelineAction::copy, 0, 0);
    w.performTimelineAction(TimelineAction::paste, 40, 2);
    check(w.engine.project.clips.size() == 4 && w.engine.project.transitionLinks.size() == 2 && w.undoHistory.size() == 1
        && w.engine.project.validateTransitionLinks().wasOk(), "complete paired group copies its internal transition with fresh identities");
    const auto pairedSave = juce::JSON::toString(projectToJson(w.engine.project));
    check(parseProject(pairedSave).error.isEmpty(), "range/group output passes the real session34 parser");
    auto pairedFile = report.getSiblingFile("selection-pair.mixworks");
    check(pairedFile.replaceWithText(pairedSave) && parseProject(pairedFile.loadFileAsString()).error.isEmpty(),
        "range/group output writes and reads an actual arrangement file");
    w.undo(false); check(json(w.engine.project) == linked, "paired group Undo restores original relationship and clocks");
    w.engine.project.transitionLinks.front().locked = true;
    const auto locked = json(w.engine.project); w.selectAllSegments(); w.performTimelineAction(TimelineAction::cut, 0, 0);
    check(json(w.engine.project) == locked && w.undoHistory.empty(), "group Cut refuses a locked participant without partial removal");
    reset(); w.selectAllSegments(); w.setEditRange(2, 6);
    w.performTimelineAction(TimelineAction::remove, 0, 0);
    check(w.engine.project.clips.size() == 4 && w.undoHistory.size() == 1, "range deletion applies atomically across selected lanes");
    w.undo(false); check(json(w.engine.project) == initial, "multilane range deletion Undo restores both sources");
    reset(); w.setEditRange(2, 6); w.editRangeTime();
    auto* dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
    check(dialog != nullptr, "exact range opens a native accessible time editor");
    if (dialog) { dialog->getTextEditor("begin")->setText("4"); dialog->getTextEditor("end")->setText("7"); dialog->exitModalState(1); }
    pump();
    check(w.editSelection.range && w.editSelection.range->getStart() == 4 && w.editSelection.range->getEnd() == 7
        && json(w.engine.project) == initial && !w.dirty, "exact time editor changes range only");
    w.editRangeTime(); dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
    w.engine.project.masterDb = -2;
    if (dialog) { dialog->getTextEditor("end")->setText("9"); dialog->exitModalState(1); }
    pump();
    check(w.editSelection.range && w.editSelection.range->getEnd() == 7 && w.engine.project.masterDb == -2,
        "stale exact range callback preserves changed document and previous range");
    reset(); w.setEditRange(2, 6); w.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));
    check(!w.editSelection.range && w.engine.position == 3.125 && json(w.engine.project) == initial, "Escape clears selection without seeking or editing");

    reset(); w.timeline.pixelsPerSecond = 20; w.timeline.updateSize();
    const auto mouse = juce::Desktop::getInstance().getMainMouseSource();
    auto event = [&](juce::Point<float> point, int modifiers, bool drag = false)
    { return juce::MouseEvent(mouse, point, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier | modifiers), 1, 0, 0, 0, 0, &w.timeline, &w.timeline, juce::Time::getCurrentTime(), point, juce::Time::getCurrentTime(), 1, drag); };
    auto first = w.timeline.clipBounds(base.clips[0]), second = w.timeline.clipBounds(base.clips[1]);
    const std::vector<juce::String> group {base.clips[0].id, base.clips[1].id};
    auto legacyGroup = base; legacyGroup.transitionLinksKnown = false;
    const auto legacyBefore = json(legacyGroup);
    check(legacyGroup.moveClipGroup(group, group[0], 0, 0, false).wasOk() && json(legacyGroup) == legacyBefore,
        "no-op group move leaves legacy transition metadata unchanged");
    auto groupProject = base;
    groupProject.clips[0].start = 1.13; groupProject.clips[1].start = 2.37;
    groupProject.clips[0].audioSlip = .17; groupProject.clips[0].reversed = true;
    groupProject.clips[1].loopSpan = .73; groupProject.clips[1].loopPhase = .13;
    groupProject.clips[0].controls.automation[0].points = {{0, -12, 0}, {3, 0, 0}};
    groupProject.linkClips(group[0], group[1]);
    auto translated = groupProject;
    check(translated.moveClipGroup(group, group[0], 3.39, 1, true).wasOk()
        && std::abs(translated.clips[0].start - 3.63) < 1.e-9 && std::abs(translated.clips[1].start - 4.87) < 1.e-9
        && translated.clips[0].track == 1 && translated.clips[1].track == 2,
        "group move snaps one displacement, preserving manual phase and relative start/lane offsets");
    bool mappings = true;
    for (size_t i = 0; i < group.size(); ++i)
    {
        const auto& a = groupProject.clips[i]; const auto& b = translated.clips[i];
        mappings = mappings && a.id == b.id && a.media == b.media && a.effects == b.effects && a.prepared == b.prepared;
        for (double t : {0., .25, 3.1, 19.9}) mappings = mappings && std::abs(a.sourceTimeAt(t) - b.sourceTimeAt(t)) < 1.e-9;
    }
    check(mappings && translated.transitionLinks.size() == 1 && translated.validateTransitionLinks().wasOk(),
        "group move retains source IDs, slip/reverse/loop mapping, effects and complete internal links");
    auto file = report.getSiblingFile("moved-group.mixworks");
    check(file.replaceWithText(json(translated)) && parseProject(file.loadFileAsString()).error.isEmpty(), "moved linked group saves and reopens through actual session parser");
    for (int mode = 0; mode < 7; ++mode)
    {
        auto p = groupProject; auto ids = group; double target = 4; int lane = 0;
        if (mode == 0) { p.clips[1].track = 3; lane = 1; }
        if (mode == 1) { p.clips[1].start = 0; target = 0; }
        if (mode == 2) p.transitionLinks.front().locked = true;
        if (mode == 3) ids[1] = "missing";
        if (mode == 4) { Clip c = p.clips[0]; c.id = juce::Uuid().toString(); c.track = 2; p.clips.push_back(c); p.linkClips(group[1], c.id); }
        if (mode == 5) { Clip c = p.clips[0]; c.id = juce::Uuid().toString(); c.start = 22; p.clips.push_back(c); }
        if (mode == 6) p.clips[1].pendingPlacement = PendingPlacement::move;
        const auto before = json(p);
        check(p.moveClipGroup(ids, group[0], target, lane, false).failed() && json(p) == before,
            "group refusal is atomic: lane overflow/start/lock/stale/external link/collision/pending " + juce::String(mode));
    }
    auto clocked = base; clocked.followTrackTempo = true; clocked.tempo.points = {{0, 120, false}};
    for (auto& c : clocked.clips)
    {
        c.beatmatched = true; c.tempoSourceBpm = 120; c.tempoMap = std::make_shared<const TempoMap>(clocked.tempo);
        c.elasticPlayback = true; c.elasticSource = std::make_shared<const ElasticSource>(c);
    }
    auto clockMoved = clocked;
    check(clockMoved.moveClipGroup(group, group[0], 2, 0, false).wasOk()
        && clockMoved.clips[0].elasticSource != clocked.clips[0].elasticSource
        && std::abs(clockMoved.clips[0].elasticSource->progressAt(5) - clocked.clips[0].elasticSource->progressAt(3)) < 1.e-9,
        "constant captured clocks move with an elastic source bound to the new programme time");
    check(parseProject(json(clockMoved)).error.isEmpty(), "moved captured-clock group passes saved geometry validation");
    auto ramp = clocked; ramp.tempo.points = {{0, 100, false}, {20, 140, true}, {60, 140, false}};
    for (auto& c : ramp.clips) c.tempoMap = std::make_shared<const TempoMap>(ramp.tempo);
    auto rampBefore = json(ramp);
    check(ramp.moveClipGroup(group, group[0], 5, 0, false).failed() && json(ramp) == rampBefore,
        "group refuses a destination that changes captured tempo shape");
    auto staleClock = clocked; staleClock.tempo.points = {{0, 90, false}};
    const auto staleClockBefore = json(staleClock);
    check(staleClock.moveClipGroup(group, group[0], 2, 0, false).failed() && json(staleClock) == staleClockBefore,
        "group refuses captured/project clock disagreement that would change audio on reopen");
    // Production rendering, including source curves, reverse, loops and elastic
    // playback. Compare equal local passages after a rigid two-second move.
    for (const auto& source : {groupProject, clocked})
    {
        auto a = std::make_unique<Engine>(), b = std::make_unique<Engine>(); a->sampleRate = b->sampleRate = 8000;
        a->project = source; b->project = source;
        const double at = source.clips[0].start;
        const auto result = b->project.moveClipGroup(group, group[0], at + 2, source.clips[0].track, false);
        juce::AudioBuffer<float> x(2, 173), y(2, 173); juce::MidiBuffer midi; float maximum = 0;
        for (int block = 0; block < 24; ++block)
        {
            const double time = at + 1 + block * 173. / 8000;
            a->render(x, 173, time, midi); b->render(y, 173, time + 2, midi);
            for (int ch = 0; ch < 2; ++ch) for (int n = 0; n < 173; ++n)
                maximum = std::max(maximum, std::abs(x.getSample(ch, n) - y.getSample(ch, n)));
        }
        check(result.wasOk() && maximum < 1.e-5f, "group move production PCM matches local passage / elastic "
            + juce::String((int) source.clips[0].elasticPlayback) + " / maximum " + juce::String(maximum, 8));
    }
    auto beginMove = [&]
    {
        reset(); w.selectAllSegments(); w.timeline.pixelsPerSecond = 20; w.timeline.updateSize();
        w.timeline.mouseDown(event({100, first.getY() + 25}, 0));
        w.timeline.mouseDrag(event({140, first.getY() + 25}, 0, true));
    };
    beginMove();
    check(w.timeline.groupMove && w.timeline.groupMove->preview && json(w.engine.project) == initial
        && w.undoHistory.empty() && !w.dirty && w.engine.position == 3.125,
        "actual group mouse drag previews without document/history/transport changes");
    for (int width : {1250, 1920})
    {
        w.setSize(width, 1000); w.resized();
        juce::PNGImageFormat png; auto output = report.getSiblingFile("group-move-preview-" + juce::String(width) + ".png").createOutputStream();
        check(output && png.writeImageToStream(w.createComponentSnapshot(w.getLocalBounds()), *output), "capture group move preview at " + juce::String(width));
    }
    w.timeline.mouseUp(event({140, first.getY() + 25}, 0));
    check(w.engine.project.clips[0].start == 2 && w.engine.project.clips[1].start == 2 && w.undoHistory.size() == 1
        && w.editSelection.clips == group && w.engine.position == 3.125, "group mouse release commits once, retaining selection and playhead");
    const auto movedGroup = json(w.engine.project);
    w.undo(false); check(json(w.engine.project) == initial, "group move Undo restores exact document");
    w.undo(true); check(json(w.engine.project) == movedGroup, "group move Redo restores exact moved document");
    beginMove(); w.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)); w.timeline.mouseUp(event({140, first.getY() + 25}, 0));
    check(!w.timeline.groupMove && w.editSelection.clips == group && json(w.engine.project) == initial && w.undoHistory.empty(),
        "Escape cancels a group preview and retains selection without creating Undo");
    beginMove(); w.engine.project.masterDb = -2; w.timeline.mouseUp(event({140, first.getY() + 25}, 0));
    check(w.engine.project.masterDb == -2 && w.engine.project.clips[0].start == 0 && w.undoHistory.empty(), "stale group preview preserves intervening control edit");
    beginMove(); w.engine.playing = true; w.timeline.mouseUp(event({140, first.getY() + 25}, 0));
    check(json(w.engine.project) == initial && w.undoHistory.empty(), "playback starting before release refuses group commit");
    beginMove(); w.timeline.mouseDrag(event({100, first.getY() + 25}, 0, true)); w.timeline.mouseUp(event({100, first.getY() + 25}, 0));
    check(json(w.engine.project) == initial && w.undoHistory.empty(), "returning group preview to origin is a no-op");
    reset(); w.timeline.pixelsPerSecond = 20; w.timeline.updateSize();
    first = w.timeline.clipBounds(base.clips[0]); second = w.timeline.clipBounds(base.clips[1]);
    w.timeline.mouseDown(event({20, second.getY() + 25}, juce::ModifierKeys::ctrlModifier));
    check(w.editSelection.clips.size() == 2 && w.selectedClip == base.clips[1].id && json(w.engine.project) == initial, "actual Ctrl-click handler adds segment to selection without seeking");
    w.timeline.mouseDown(event({20, first.getY() + 25}, juce::ModifierKeys::ctrlModifier));
    check(w.editSelection.clips.size() == 1 && w.selectedClip == base.clips[1].id, "Ctrl-click toggles member without losing active remaining owner");
    w.clearEditSelection(); w.selectedClip = base.clips[0].id; w.editingRange.setToggleState(true, juce::dontSendNotification); w.refresh();
    w.timeline.mouseDown(event({40, first.getY() + 25}, 0));
    w.timeline.mouseDrag(event({120, first.getY() + 25}, 0, true)); w.timeline.mouseUp(event({120, first.getY() + 25}, 0));
    check(w.editSelection.range && w.editSelection.range->getStart() == 2 && w.editSelection.range->getEnd() == 6
        && w.engine.position == 3.125 && json(w.engine.project) == initial, "real range drag handlers select time interval without moving source/playhead");
    w.editingRangeSize.setSelectedId(4, juce::dontSendNotification);
    w.timeline.mouseDown(event({40, first.getY() + 25}, 0)); w.timeline.mouseUp(event({40, first.getY() + 25}, 0));
    check(w.editSelection.range && std::abs(w.editSelection.range->getLength() - 16) < 1.e-8, "8-bar range preset uses 32 beats on canonical 4/4 clock");
    for (int width : {1250, 1920})
    {
        w.setSize(width, 1000); w.toolPage = 2; w.resized();
        w.editingViewport.setViewPosition(0, std::max(0, w.editingRange.getY() - 10));
        juce::PNGImageFormat png; auto output = report.getSiblingFile("range-selection-" + juce::String(width) + ".png").createOutputStream();
        check(output && png.writeImageToStream(w.createComponentSnapshot(w.getLocalBounds()), *output), "capture range/group editor at " + juce::String(width));
    }

    reset(); w.setEditRange(2, 6); w.performTimelineAction(TimelineAction::copy, 0, 0);
    const auto clipboardRevision = w.editClipboardRevision;
    const auto clipboardEffects = w.editClipboard->clips[0].effects;
    const auto otherId = base.clips[1].id;
    w.performTimelineAction(TimelineAction::cropSelection, 0, 0);
    check(w.currentClip() && w.currentClip()->start == 2 && w.currentClip()->length == 4 && w.currentClip()->offset == 2
        && w.engine.project.clipFor(otherId)->start == 0 && w.engine.project.clipFor(otherId)->length == 20
        && w.undoHistory.size() == 1 && w.engine.position == 3.125,
        "Crop retains selected source interval and ID, leaving other lane and playhead unchanged with one Undo");
    check(w.editClipboardRevision == clipboardRevision && w.editClipboard->clips[0].effects == clipboardEffects,
        "Crop preserves existing clipboard ownership and revision");
    const auto cropped = json(w.engine.project);
    w.performTimelineAction(TimelineAction::cropSelection, 0, 0);
    check(json(w.engine.project) == cropped && w.undoHistory.size() == 1, "Crop of the already-cropped interval is a no-op");
    w.undo(false); check(json(w.engine.project) == initial, "Crop Undo restores complete original source span");
    w.undo(true); check(json(w.engine.project) == cropped, "Crop Redo restores exact cropped document");
    w.selectedClip = base.clips[0].id; w.setEditRange(2, 6);
    w.performSelectionAction(TimelineAction::repeatSelection, 0, 0, 3);
    check(w.engine.project.clips.size() == 5 && w.undoHistory.size() == 2 && w.editSelection.clips.size() == 3
        && w.engine.project.clips[2].start == 6 && w.engine.project.clips[3].start == 10 && w.engine.project.clips[4].start == 14
        && w.engine.position == 3.125, "Repeat adds three exact adjacent copies in one transaction without seeking");
    std::set<const ClipEffects*> independentEffects;
    std::set<juce::String> repeatedIds;
    for (const auto& c : w.engine.project.clips) if (c.track == 0) { independentEffects.insert(c.effects.get()); repeatedIds.insert(c.id); }
    check(independentEffects.size() == 4 && repeatedIds.size() == 4 && w.editClipboardRevision == clipboardRevision
        && w.editClipboard->clips[0].effects == clipboardEffects, "Repeat uses fresh segment IDs and independent effects without replacing clipboard");
    const auto repeated = json(w.engine.project);
    const auto repeatedFile = report.getSiblingFile("repeat-crop.mixworks");
    check(repeatedFile.replaceWithText(repeated) && parseProject(repeatedFile.loadFileAsString()).error.isEmpty(), "Crop and Repeat write and reopen a real arrangement file");
    w.undo(false); check(json(w.engine.project) == cropped, "one Repeat Undo removes all additional copies");
    w.undo(true); check(json(w.engine.project) == repeated, "Repeat Redo restores all copies exactly");

    for (int mode = 0; mode < 6; ++mode)
    {
        reset(); w.setEditRange(2, 6);
        if (mode == 0) w.engine.playing = true;
        if (mode == 1) { w.engine.project.linkClips(base.clips[0].id, otherId); }
        if (mode == 2) { w.engine.project.linkClips(base.clips[0].id, otherId); w.engine.project.transitionLinks[0].locked = true; }
        if (mode == 3) w.setEditRange(25, 30);
        if (mode == 4) w.engine.project.clips[0].pendingPlacement = PendingPlacement::move;
        if (mode == 5) w.engine.setAutomationRecordMode(AutomationRecordMode::touch);
        const auto before = json(w.engine.project);
        w.performTimelineAction(TimelineAction::cropSelection, 0, 0);
        check(json(w.engine.project) == before && w.undoHistory.empty() && !w.dirty,
            "Crop refusal keeps document/history: playing/linked/locked/empty/pending/record mode " + juce::String(mode));
    }
    reset(); w.engine.project.clips[0].length = 2; w.engine.project.clips[1].start = 4; w.engine.project.clips[1].length = 10;
    w.selectAllSegments(); w.setEditRange(5, 6); w.performTimelineAction(TimelineAction::cropSelection, 0, 0);
    check(w.engine.project.clips.size() == 1 && w.selectedClip == otherId && w.engine.selectedTrack == 1
        && w.currentClip()->start == 5 && w.currentClip()->length == 1 && w.undoHistory.size() == 1,
        "Crop removes selected nonintersecting segments and transfers active ownership to a surviving lane");
    reset(); w.selectAllSegments(); w.engine.project.clips[1].start = 12;
    w.engine.project.linkClips(base.clips[0].id, otherId);
    const auto paired = json(w.engine.project);
    w.performSelectionAction(TimelineAction::repeatSelection, 0, 0, 2);
    check(w.engine.project.clips.size() == 6 && w.engine.project.transitionLinks.size() == 3
        && w.engine.project.clips[2].start == 32 && w.engine.project.clips[3].start == 44 && w.undoHistory.size() == 1
        && parseProject(json(w.engine.project)).error.isEmpty(), "Repeat carries complete internal transition links and relative lane/time offsets");
    w.undo(false); check(json(w.engine.project) == paired, "linked group Repeat Undo restores original relationships");
    for (int mode = 0; mode < 5; ++mode)
    {
        reset(); int count = 2;
        if (mode == 0) w.setEditRange(2, 6); // Original source occupies the first repeat destination.
        if (mode == 1) count = 33;
        if (mode == 2) count = -1;
        if (mode == 3) w.engine.playing = true;
        if (mode == 4) { w.engine.project.linkClips(base.clips[0].id, otherId); w.engine.project.transitionLinks[0].locked = true; }
        const auto before = json(w.engine.project);
        w.performSelectionAction(TimelineAction::repeatSelection, 0, 0, count);
        check(json(w.engine.project) == before && w.undoHistory.empty() && !w.editClipboard,
            "Repeat refuses collision/count/playing/lock without partial copies or clipboard changes " + juce::String(mode));
    }
    reset(); auto lateObstacle = base.clips[0]; lateObstacle.id = juce::Uuid().toString(); lateObstacle.start = 45;
    w.engine.project.clips.push_back(lateObstacle); const auto lateBefore = json(w.engine.project);
    w.performSelectionAction(TimelineAction::repeatSelection, 0, 0, 2);
    check(json(w.engine.project) == lateBefore && w.undoHistory.empty(), "collision in a later repeat rolls back the entire candidate, including earlier clear copies");
    reset(); w.engine.project.clips[0].controls.pluginAutomation.push_back({});
    w.engine.project.clips[0].controls.pluginAutomation.back().slotId = "missing-plugin";
    const auto missingPlugin = json(w.engine.project);
    w.performSelectionAction(TimelineAction::repeatSelection, 0, 0, 2);
    check(json(w.engine.project) == missingPlugin && w.undoHistory.empty(), "Repeat plugin-clone failure rejects atomically");

    for (bool elastic : {false, true}) for (bool reverse : {false, true})
    {
        reset(); auto& c = w.engine.project.clips[0];
        c.reversed = reverse; c.audioSlip = .17; c.loopSpan = .73; c.loopPhase = .13;
        c.controls.automation[0].points = {{0, -12, 0}, {3, -4, 0}, {7, -16, 0}};
        if (elastic)
        {
            w.engine.project.followTrackTempo = true; w.engine.project.tempo.points = {{0, 120, false}};
            c.beatmatched = true; c.tempoSourceBpm = 120; c.tempoMap = std::make_shared<const TempoMap>(w.engine.project.tempo);
            c.elasticPlayback = true; c.elasticSource = std::make_shared<const ElasticSource>(c);
        }
        const auto original = w.engine.project;
        w.setEditRange(2, 6); w.performTimelineAction(TimelineAction::cropSelection, 0, 0);
        const auto afterCrop = w.engine.project;
        w.performSelectionAction(TimelineAction::repeatSelection, 0, 0, 1);
        const bool added = w.engine.project.clips.size() == 3;
        auto a = std::make_unique<Engine>(), b = std::make_unique<Engine>(), d = std::make_unique<Engine>();
        a->sampleRate = b->sampleRate = d->sampleRate = 8000;
        a->project = original; a->project.clips.resize(1);
        b->project = afterCrop; b->project.clips.resize(1);
        d->project = w.engine.project;
        if (added) d->project.clips = {d->project.clips.back()};
        juce::AudioBuffer<float> x(2, 173), y(2, 173), z(2, 173); juce::MidiBuffer midi;
        float cropError = 0, repeatError = 0;
        for (int block = 0; block < 20; ++block)
        {
            const double time = 2.5 + block * 173. / 8000;
            a->render(x, 173, time, midi); b->render(y, 173, time, midi); d->render(z, 173, time + 4, midi);
            for (int ch = 0; ch < 2; ++ch) for (int n = 0; n < 173; ++n)
            { cropError = std::max(cropError, std::abs(x.getSample(ch, n) - y.getSample(ch, n))); repeatError = std::max(repeatError, std::abs(x.getSample(ch, n) - z.getSample(ch, n))); }
        }
        check(added && cropError < 1.e-5f && repeatError < 1.e-5f,
            "Crop/Repeat production PCM preserves slipped loop and source automation / elastic " + juce::String((int) elastic)
            + " reverse " + juce::String((int) reverse) + " errors " + juce::String(cropError, 8) + "/" + juce::String(repeatError, 8));
        check(parseProject(json(w.engine.project)).error.isEmpty(), "Crop/Repeat saved source geometry validates for rendered fixture");
    }
    reset(); w.engine.project.followTrackTempo = true; w.engine.project.tempo.points = {{0, 120, false}, {20, 120, false}, {40, 150, true}};
    auto& tempoClip = w.engine.project.clips[0]; tempoClip.beatmatched = true; tempoClip.tempoSourceBpm = 120;
    tempoClip.tempoMap = std::make_shared<const TempoMap>(w.engine.project.tempo);
    const auto tempoBefore = json(w.engine.project);
    w.performSelectionAction(TimelineAction::repeatSelection, 0, 0, 1);
    check(json(w.engine.project) == tempoBefore && w.undoHistory.empty(), "Repeat refuses destination tempo stretching without moving existing tempo markers");

    reset(); w.performTimelineAction(TimelineAction::repeatSelection, 0, 0);
    dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
    check(dialog && dialog->getTextEditor("copies") && dialog->getButton(0)->isEnabled(), "Repeat opens native count editor with destination preview");
    if (dialog)
    {
        dialog->getTextEditor("copies")->setText("3"); pump();
        juce::PNGImageFormat png; auto output = report.getSiblingFile("repeat-dialog.png").createOutputStream();
        check(output && png.writeImageToStream(dialog->createComponentSnapshot(dialog->getLocalBounds()), *output), "capture Repeat count and destination preview");
        dialog->exitModalState(0);
    }
    pump(); check(json(w.engine.project) == initial && w.undoHistory.empty(), "Repeat dialog Cancel is a no-op");
    w.setEditRange(2, 6); w.performTimelineAction(TimelineAction::repeatSelection, 0, 0);
    dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
    check(dialog && !dialog->getButton(0)->isEnabled(), "Repeat preview disables commit for occupied destination");
    if (dialog)
    {
        juce::PNGImageFormat png; auto output = report.getSiblingFile("repeat-occupied.png").createOutputStream();
        check(output && png.writeImageToStream(dialog->createComponentSnapshot(dialog->getLocalBounds()), *output), "capture occupied Repeat destination explanation");
    }
    if (dialog) dialog->exitModalState(0); pump();
    reset(); w.performTimelineAction(TimelineAction::repeatSelection, 0, 0);
    dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
    w.engine.project.masterDb = -2;
    if (dialog) dialog->exitModalState(1); pump();
    check(w.engine.project.masterDb == -2 && w.engine.project.clips.size() == 2 && w.undoHistory.empty(), "stale Repeat dialog preserves intervening document changes");
    reset(); w.performTimelineAction(TimelineAction::repeatSelection, 0, 0);
    dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
    if (dialog) { dialog->getTextEditor("copies")->setText("2"); dialog->exitModalState(1); }
    pump(); check(w.engine.project.clips.size() == 4 && w.undoHistory.size() == 1, "Repeat native dialog dispatch applies chosen count in one Undo");
    for (int width : {1250, 1920})
    {
        reset(); w.setEditRange(2, 6); w.setSize(width, 1000); w.consoleCollapsed = false; w.toolPage = 2; w.resized();
        w.editingViewport.setViewPosition(0, std::max(0, w.editingRange.getY() - 10));
        juce::PNGImageFormat png; auto output = report.getSiblingFile("repeat-crop-controls-" + juce::String(width) + ".png").createOutputStream();
        check(w.editingPanel.isVisible() && w.editingRepeat.isEnabled() && w.editingCrop.isEnabled() && output
            && png.writeImageToStream(w.createComponentSnapshot(w.getLocalBounds()), *output), "capture Repeat/Crop controls at " + juce::String(width));
    }

    auto replacementFixture = [&]
    {
        reset(); w.selectedClip = otherId; w.setEditRange(2, 6); w.performTimelineAction(TimelineAction::copy, 0, 1);
        w.selectedClip = base.clips[0].id; w.setEditRange(8, 12);
    };
    replacementFixture(); const auto replaceClipboard = w.editClipboardRevision;
    const auto replaceEffects = w.editClipboard->clips[0].effects;
    w.replaceSelectedAudio(true);
    check(w.engine.project.clips.size() == 4 && w.currentClip() && w.currentClip()->start == 8 && w.currentClip()->end() == 12
        && w.currentClip()->offset == 2 && w.currentClip()->media == base.clips[1].media && w.currentClip()->track == 0
        && w.undoHistory.size() == 1 && w.engine.position == 3.125, "Replace swaps the selected interval from clipboard with one Undo and no seek");
    std::set<const ClipEffects*> replacedEffects;
    for (const auto& c : w.engine.project.clips) if (c.track == 0) replacedEffects.insert(c.effects.get());
    check(replacedEffects.size() == 3 && w.currentClip()->effects != replaceEffects && w.editClipboardRevision == replaceClipboard
        && w.editClipboard->clips[0].effects == replaceEffects && w.engine.project.clipFor(otherId)->length == 20,
        "Replace splits destination with independent effects, keeps source lane and clipboard ownership");
    const auto replaced = json(w.engine.project);
    auto replacedFile = report.getSiblingFile("replace-selection.mixworks");
    check(replacedFile.replaceWithText(replaced) && parseProject(replacedFile.loadFileAsString()).error.isEmpty(), "Replace writes and reopens a valid arrangement");
    w.undo(false); check(json(w.engine.project) == initial, "Replace Undo restores original segments and identities exactly");
    w.undo(true); check(json(w.engine.project) == replaced, "Replace Redo restores independent replacement and exterior slices");

    replacementFixture(); w.setEditRange(8, 15); w.replaceSelectedAudio(true);
    check(w.currentClip() && w.currentClip()->end() == 12 && w.engine.project.clips.size() == 4
        && std::none_of(w.engine.project.clips.begin(), w.engine.project.clips.end(), [](const auto& c) { return c.track == 0 && c.start < 15 && c.end() > 12; }),
        "shorter clipboard leaves the rest of the selected destination empty without moving following audio");
    reset(); w.selectedClip = otherId; w.performTimelineAction(TimelineAction::copy, 0, 1);
    w.selectedClip = base.clips[0].id; w.replaceSelectedAudio(true);
    check(w.clipClipboard && w.engine.project.clips.size() == 2 && !w.engine.project.clipFor(base.clips[0].id)
        && w.currentClip()->media == base.clips[1].media && w.currentClip()->start == 0 && w.undoHistory.size() == 1,
        "Replace also accepts ordinary single-segment clipboard and active destination without a range");

    for (int mode = 0; mode < 13; ++mode)
    {
        replacementFixture();
        if (mode == 0) w.setEditRange(8, 10);
        if (mode == 1) w.setEditRange(25, 30);
        if (mode == 2) w.engine.playing = true;
        if (mode == 3) w.engine.setAutomationRecordMode(AutomationRecordMode::touch);
        if (mode == 4) w.engine.project.clips[0].pendingPlacement = PendingPlacement::move;
        if (mode == 5 || mode == 6) { w.engine.project.linkClips(base.clips[0].id, otherId); w.engine.project.transitionLinks[0].locked = mode == 6; }
        if (mode == 7) { auto obstacle = base.clips[0]; obstacle.id = juce::Uuid().toString(); obstacle.start = 9; obstacle.length = 1; w.engine.project.clips.push_back(obstacle); }
        if (mode == 8) { w.editClipboard->clips[0].controls.pluginAutomation.push_back({}); w.editClipboard->clips[0].controls.pluginAutomation.back().slotId = "missing-plugin"; }
        if (mode == 9) { auto extra = w.editClipboard->clips[0]; extra.id = juce::Uuid().toString(); ++extra.track; w.editClipboard->clips.push_back(extra); }
        if (mode == 10) w.editClipboard->clips[0].media.reset();
        if (mode == 11) { w.engine.project.clips[0].controls.pluginAutomation.push_back({}); w.engine.project.clips[0].controls.pluginAutomation.back().slotId = "missing-plugin"; }
        if (mode == 12) w.editClipboard.reset();
        const auto before = json(w.engine.project); const auto scope = w.editSelection; const auto revision = w.editClipboardRevision;
        w.replaceSelectedAudio(true);
        check(json(w.engine.project) == before && w.undoHistory.empty() && !w.dirty && w.editSelection.clips == scope.clips
            && w.editSelection.range == scope.range && w.editClipboardRevision == revision && w.engine.position == 3.125,
            "Replace refuses atomically: short/empty/playing/Read/preparing/linked/locked/collision/plugin/lanes/source/split/clipboard " + juce::String(mode));
    }
    reset(); w.engine.project.clips[1].start = 12; w.engine.project.linkClips(base.clips[0].id, otherId);
    w.selectAllSegments(); w.performTimelineAction(TimelineAction::copy, 0, 0);
    const auto beforePairReplace = json(w.engine.project); w.replaceSelectedAudio(true);
    check(w.engine.project.clips.size() == 2 && w.engine.project.transitionLinks.size() == 1
        && !w.engine.project.clipFor(base.clips[0].id) && !w.engine.project.clipFor(otherId) && w.undoHistory.size() == 1
        && parseProject(json(w.engine.project)).error.isEmpty(), "Replace whole selected linked group carries only fresh internal relationships");
    w.undo(false); check(json(w.engine.project) == beforePairReplace, "Replace linked group Undo restores exact pair");

    replacementFixture(); w.engine.project.tempo.points = {{0, 120, false}, {10, 130, true}};
    w.engine.project.tracks[0].automation[0].points = {{0, -3, 0}, {20, -6, 0}};
    auto withoutClips = [&](Project p) { p.clips.clear(); return json(p); };
    const auto fixedControls = withoutClips(w.engine.project); w.replaceSelectedAudio(true);
    check(w.undoHistory.size() == 1 && withoutClips(w.engine.project) == fixedControls,
        "Replace keeps project tempo and lane/session control data exactly");
    replacementFixture(); w.engine.project.followTrackTempo = true;
    w.engine.project.tempo.points = {{0, 120, false}, {6, 120, false}, {20, 150, true}};
    auto& clockCopy = w.editClipboard->clips[0]; clockCopy.beatmatched = true; clockCopy.tempoSourceBpm = 120;
    clockCopy.tempoMap = std::make_shared<const TempoMap>(w.engine.project.tempo);
    const auto beforeClockReplace = json(w.engine.project); w.replaceSelectedAudio(true);
    check(json(w.engine.project) == beforeClockReplace && w.undoHistory.empty(), "Replace refuses incompatible destination source tempo shape");

    for (bool elastic : {false, true}) for (bool reverse : {false, true})
    {
        reset();
        w.engine.project.followTrackTempo = elastic; if (elastic) w.engine.project.tempo.points = {{0, 120, false}};
        for (auto& c : w.engine.project.clips)
        {
            c.reversed = reverse; c.audioSlip = .17; c.loopSpan = .73; c.loopPhase = .13;
            c.controls.automation[0].points = {{0, -12, 0}, {3, -4, 0}, {17, -16, 0}};
            if (elastic)
            {
                c.beatmatched = true; c.tempoSourceBpm = 120; c.tempoMap = std::make_shared<const TempoMap>(w.engine.project.tempo);
                c.elasticPlayback = true; c.elasticSource = std::make_shared<const ElasticSource>(c);
            }
        }
        const auto original = w.engine.project;
        w.selectedClip = otherId; w.setEditRange(2, 6); w.performTimelineAction(TimelineAction::copy, 0, 1);
        w.selectedClip = base.clips[0].id; w.setEditRange(8, 12); w.replaceSelectedAudio(true);
        auto destination = std::make_unique<Engine>(), otherLane = std::make_unique<Engine>(), copied = std::make_unique<Engine>(), actual = std::make_unique<Engine>();
        destination->sampleRate = otherLane->sampleRate = copied->sampleRate = actual->sampleRate = 8000;
        destination->project = otherLane->project = copied->project = original;
        destination->project.clips = {original.clips[0]}; otherLane->project.clips = copied->project.clips = {original.clips[1]};
        actual->project = w.engine.project;
        juce::AudioBuffer<float> a(2, 128), b(2, 128), c(2, 128), d(2, 128); juce::MidiBuffer midi; float error = 0; int worstFrame = -1;
        for (int frame = 0; frame < 160000; frame += 128)
        {
            const double time = frame / 8000.; const bool inside = frame >= 64000 && frame < 96000;
            destination->render(a, 128, time, midi); otherLane->render(b, 128, time, midi); actual->render(d, 128, time, midi);
            if (inside) copied->render(c, 128, time - 6, midi);
            for (int ch = 0; ch < 2; ++ch) for (int n = 0; n < 128; ++n)
            { const float difference = std::abs(d.getSample(ch,n)-b.getSample(ch,n)-(inside ? c.getSample(ch,n) : a.getSample(ch,n))); if (difference > error) { error = difference; worstFrame = frame+n; } }
        }
        check(w.undoHistory.size() == 1 && error < 1.e-5f, "Replace production PCM equals independent source concatenation plus unchanged other lane / elastic "
            + juce::String((int) elastic) + " reverse " + juce::String((int) reverse) + " maximum " + juce::String(error, 8) + " at " + juce::String(worstFrame));
        const auto parsedReplacement = parseProject(json(w.engine.project));
        check(parsedReplacement.error.isEmpty(), "Replace rendered slipped-loop/automation geometry passes saved validation: " + parsedReplacement.error);
        report.getSiblingFile("replace-pcm-"+juce::String((int)elastic)+"-"+juce::String((int)reverse)+".json").replaceWithText(json(w.engine.project));
    }
    for (int mode = 0; mode < 6; ++mode)
    {
        replacementFixture(); w.performTimelineAction(TimelineAction::replaceSelection, 0, 0);
        dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        check(dialog && dialog->getButton(0)->isEnabled(), "Replace opens native destination preview " + juce::String(mode));
        if (mode == 0 && dialog)
        {
            juce::PNGImageFormat png; auto output = report.getSiblingFile("replace-dialog.png").createOutputStream();
            check(output && png.writeImageToStream(dialog->createComponentSnapshot(dialog->getLocalBounds()), *output), "capture native Replace preview");
        }
        if (mode == 1) w.engine.project.masterDb = -2;
        if (mode == 2) w.setEditRange(9, 13);
        if (mode == 3) { w.performTimelineAction(TimelineAction::copy, 0, 0); }
        if (mode == 4) w.engine.playing = true;
        const auto before = json(w.engine.project);
        if (dialog) dialog->exitModalState(mode == 0 ? 0 : 1); pump();
        check(mode == 5 ? w.undoHistory.size() == 1 && w.engine.project.clips.size() == 4
            : json(w.engine.project) == before && w.undoHistory.empty(), "Replace native Cancel/stale document/selection/clipboard/playback/accept " + juce::String(mode));
    }
    replacementFixture(); w.setEditRange(8, 10); w.performTimelineAction(TimelineAction::replaceSelection, 0, 0);
    dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
    check(dialog && !dialog->getButton(0)->isEnabled(), "Replace preview disables commit when the clipboard will not fit");
    if (dialog)
    {
        juce::PNGImageFormat png; auto output = report.getSiblingFile("replace-refused.png").createOutputStream();
        check(output && png.writeImageToStream(dialog->createComponentSnapshot(dialog->getLocalBounds()), *output), "capture Replace refused destination explanation");
        dialog->exitModalState(0);
    }
    pump();
    for (int width : {1250, 1920})
    {
        replacementFixture(); w.setSize(width, 1000); w.consoleCollapsed = false; w.toolPage = 2; w.resized();
        w.editingViewport.setViewPosition(0, std::max(0, w.editingRange.getY() - 10));
        juce::PNGImageFormat png; auto output = report.getSiblingFile("replace-controls-" + juce::String(width) + ".png").createOutputStream();
        check(w.editingPanel.isVisible() && w.editingReplace.isEnabled() && output
            && png.writeImageToStream(w.createComponentSnapshot(w.getLocalBounds()), *output), "capture Replace controls at " + juce::String(width));
    }
}
}
