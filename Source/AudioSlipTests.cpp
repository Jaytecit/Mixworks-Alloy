#include "Workstation.h"
#include "ElasticAudio.h"
#include "Beatmatch.h"
#include <cmath>
#include <limits>
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace mixworks
{
void runAudioSlipTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    const auto root = report.getParentDirectory().getChildFile("audio-slip-" + juce::Uuid().toString()); root.createDirectory();
    auto media = std::make_shared<Media>(); media->sampleRate = 8000; media->manualBpm = 120; media->manualBeatOrigin = .213;
    media->name = "Audio slip fixture"; media->file = root.getChildFile("source.wav"); media->samples.setSize(1, 32000);
    for (int i = 0; i < 32000; ++i) media->samples.setSample(0, i, .12f + .05f * std::sin((float) (i * .031)) + (i > 4000 && i < 4100 ? .4f : 0.f));
    {
        juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream = media->file.createOutputStream();
        auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(1).withBitsPerSample(24));
        check(writer && writer->writeFromAudioSampleBuffer(media->samples, 0, 32000), "write owned nonzero-edge source for production slip Save/Open/export");
    }
    Clip clip; clip.media = media; clip.length = 4; clip.introAnchor = 0; clip.outroAnchor = 2;
    Project base; base.bpm = 120; base.tempo.setPoint(0, 120); base.clips.push_back(clip);
    Clip partner = clip; partner.id = juce::Uuid().toString(); partner.start = 2; partner.track = 1;
    base.clips.push_back(partner); base.transitionLinksKnown = true; base.transitionLinks = {{clip.id, partner.id}};
    base.clips[0].controls.automation[0] = {true, {{0, -9}, {1, -3}, {4, -12}}};
    base.clips[0].fadeIn = .2; base.clips[0].fadeOut = .4;
    if(juce::JUCEApplicationBase::getCommandLineParameters().contains("--pair-movement-only"))
    {
        Project fixture; fixture.followTrackTempo=false; fixture.bpm=fixture.tempo.fallback=120; fixture.snapDivision=1; fixture.masterDb=0; fixture.transitionLinksKnown=true;
        for(int n=0;n<4;++n)
        {
            Clip c; c.media=media; c.track=n; c.start=.17+n*2; c.length=4; c.introAnchor=.043; c.outroAnchor=2.043; c.effects=makeClipEffects(); fixture.clips.push_back(c);
        }
        for(int n=1;n<4;++n) { Project::TransitionLink link{fixture.clips[(size_t)n-1].id,fixture.clips[(size_t)n].id}; link.frame=std::array<double,2>{2.043,4}; fixture.transitionLinks.push_back(link); }
        fixture.updateLaneHandles(); const auto a=fixture.clips[0].id,b=fixture.clips[1].id,c=fixture.clips[2].id,d=fixture.clips[3].id;
        const auto json=[](const Project& p) {return juce::JSON::toString(projectToJson(p),true,17);};
        const auto join=[](const Project& p,const juce::String& id,bool intro) {const auto* c=p.clipFor(id);return c->start+c->transitionTime(intro);};
        check(fixture.validateTransitionLinks().wasOk(),"authored four-track fixture has deliberately off-grid IN/OUT joins");
        auto out=fixture; const auto outMove=out.moveTransitionAnchor(a,false,2.64,true);
        check(outMove.wasOk() && out.clips[0].start==fixture.clips[0].start && std::abs(join(out,a,false)-2.5)<1.e-8
            && std::abs(join(out,b,true)-2.5)<1.e-8 && out.clips[1].start!=fixture.clips[1].start && out.clips[2].start==fixture.clips[2].start,
            "OUT snaps to timeline beat and moves only next audio while its own track stays put / "+outMove.getErrorMessage());
        auto in=fixture; const double fixedJoin=join(in,a,false); const auto inMove=in.moveTransitionAnchor(b,true,3,true);
        check(inMove.wasOk() && in.clips[0].start==fixture.clips[0].start && join(in,a,false)==fixedJoin
            && std::abs(join(in,b,true)-fixedJoin)<1.e-8 && in.clips[1].start!=fixture.clips[1].start && in.clips[2].start==fixture.clips[2].start,
            "IN changes the incoming source beneath fixed OUT without moving earlier or unrelated audio / "+inMove.getErrorMessage());
        check(std::abs(in.clips[1].sourceTimeAt(fixedJoin-in.clips[1].start)-.83)<1.e-7,"IN drag selects the snapped source point requested by the pointer");
        check(out.clips[0].manualTransitionArea && in.clips[0].manualTransitionArea,
            "manual IN and OUT both mark the outgoing tail without changing the placement of the outgoing audio");
        for(bool snap:{false,true})
        {
            auto p=fixture; const auto result=p.moveTransitionAnchor(a,false,2.647,snap);
            check(result.wasOk() && std::abs(join(p,a,false)-(snap?2.5:2.647))<1.e-8,"free adjustment then Snap uses fixed timeline lattice, never prior marker phase / "+juce::String((int)snap));
        }
        auto freeThenSnap=fixture; freeThenSnap.moveTransitionAnchor(a,false,2.647,false); freeThenSnap.moveTransitionAnchor(a,false,2.78,true);
        check(std::abs(join(freeThenSnap,a,false)-3)<1.e-8,"switching Snap back on after fine OUT alignment does not establish a new snap origin");
        auto independently=fixture; const auto moved=independently.moveClip(b,3.37,false);
        check(moved.wasOk() && independently.clips[1].start==3.37 && independently.clips[0].start==fixture.clips[0].start
            && independently.clips[2].start==fixture.clips[2].start && std::abs(join(independently,a,false)-join(independently,b,true))<1.e-8,
            "unlocked header moves one track and keeps paired markers aligned without reflowing the chain");
        for(int locks=1;locks<=3;++locks)
        {
            auto p=fixture; for(int n=0;n<locks;++n)p.setTransitionMovementLocked(p.transitionLinks[(size_t)n].incoming,true);
            const auto result=p.moveClip(b,3.37,false); bool correct=result.wasOk();
            for(size_t n=0;n<4;++n)correct &= std::abs(p.clips[n].start-(fixture.clips[n].start+(n<=(size_t)locks ? 1.2:0)))<1.e-8;
            check(correct,"only transitively locked pairs move together / "+juce::String(locks)+" / "+result.getErrorMessage());
        }
        auto locked=fixture; locked.setTransitionMovementLocked(b,true); const auto frozen=json(locked);
        check(locked.moveTransitionAnchor(a,false,2.5,true).failed() && json(locked)==frozen,"pair movement lock explains and prevents independent OUT alignment until unlocked");
        auto invalid=fixture; invalid.setTransitionMovementLocked(b,true); invalid.setTransitionMovementLocked(c,true);
        const auto beforeInvalid=json(invalid); check(invalid.moveClip(b,.01,false).failed() && json(invalid)==beforeInvalid,"complete locked group cannot be moved before project zero, atomically");
        for(int division:{1,2,4})
        {
            auto p=fixture; p.snapDivision=division;
            check(std::abs(p.snappedMoveTime(.213,2.647,true)-p.gridClock().snap(2.647,true,p.snapStep()))<1.e-10,
                "move snapping ignores a deliberately off-grid original reference / "+juce::String(division));
            const double placed=p.snappedClipStart(p.clips[1],3.37,true,1);
            check(std::abs(std::remainder(p.gridClock().beatsAt(placed+p.clips[1].firstBeatTime()),p.snapStep()))<1.e-8,
                "track pulse destination uses only project grid despite a closer off-grid neighbouring pulse / "+juce::String(division));
        }
        auto append=fixture; append.clips.resize(1); append.transitionLinks.clear();
        const auto lone = append.moveTransitionAnchor(a, false, 2.75, false);
        check(lone.wasOk() && append.clips[0].manualTransitionArea && std::abs(join(append,a,false)-2.75)<1.e-8,
            "manual OUT marks an editable tail before an incoming clip exists");
        const auto loneTrack = append;
        Clip added=fixture.clips[1]; added.start=4.17; added.introAnchor.reset(); added.outroAnchor.reset(); added.pendingPlacement=PendingPlacement::append; added.pendingAutoTransition=false; append.clips.push_back(added);
        const auto planned=planTempoMap(append,0,TransitionPreset::alignOnly,false);
        check(planned.error.isEmpty() && std::abs(join(planned.project,a,false)-join(planned.project,b,true))<1.e-7 && planned.project.transitionFor(b)
            && planned.project.clips[0].manualTransitionArea,
            "ordinary next-track append aligns IN to existing last OUT and retains explicit pair identity / "+planned.error);
        auto drop=append; drop.tempo.setPoint(0,120);
        auto incoming=std::make_shared<Media>();
        incoming->sampleRate=media->sampleRate; incoming->manualBpm=118; incoming->manualBeatOrigin=.213; incoming->samples=media->samples;
        drop.clips[1].media=incoming; drop.clips[1].pendingPlacement=PendingPlacement::drop; drop.clips[1].start=2.9;
        const auto pairedDrop=planTempoMap(drop,0,TransitionPreset::alignOnly,false);
        const auto* dropLink=pairedDrop.project.transitionFor(b);
        check(pairedDrop.error.isEmpty() && dropLink && !dropLink->movementLocked && pairedDrop.project.clips[1].track==1
            && std::abs(join(pairedDrop.project,a,false)-join(pairedDrop.project,b,true))<1.e-7
            && std::abs(join(pairedDrop.project,a,false)-join(drop,a,false))<1.e-7
            && pairedDrop.project.tempo.bpmAt(3)==120 && pairedDrop.project.clips[1].fadeIn==0
            && std::any_of(pairedDrop.project.tempo.points.begin(),pairedDrop.project.tempo.points.end(),
                [&](const auto& point) { return point.clipId==b && point.bpm==120; }),
            "short alignment-only drop keeps the existing tempo, with an editable marker and aligned IN/OUT / "+pairedDrop.error);
        if (dropLink)
        {
            auto movedDrop=pairedDrop.project;
            const auto unlockedMove=movedDrop.moveClip(b,movedDrop.clips[1].start+.5,false);
            check(unlockedMove.wasOk() && movedDrop.clips[0].start==drop.clips[0].start
                && std::abs(join(movedDrop,a,false)-join(movedDrop,b,true))<1.e-7,
                "different-BPM drop can move independently while paired markers stay together / "+unlockedMove.getErrorMessage());
            movedDrop.setTransitionMovementLocked(b,true);
            const auto lockedMove=movedDrop.moveClip(b,movedDrop.clips[1].start+.5,false);
            check(lockedMove.wasOk() && std::abs(movedDrop.clips[0].start-drop.clips[0].start-.5)<1.e-7
                && std::abs(join(movedDrop,a,false)-join(movedDrop,b,true))<1.e-7,
                "locking the different-BPM drop moves both tracks and preserves their join / "+lockedMove.getErrorMessage());
        }
        drop.clips[1].start=5;
        const auto separateDrop=planTempoMap(drop,0,TransitionPreset::crossfade,false);
        check(separateDrop.error.isEmpty() && !separateDrop.project.transitionFor(b),
            "a drop outside the marked track remains independent / "+separateDrop.error);
        auto ramped=fixture; ramped.clips.resize(2); ramped.transitionLinks.resize(1);
        ramped.followTrackTempo=true; ramped.tempo.points={{0,120,false},{10,132,true}};
        const auto clock=std::make_shared<const TempoMap>(ramped.tempo);
        for(auto& track:ramped.clips) { track.beatmatched=true; track.retime(track.start,clock); }
        auto& first=ramped.clips[0]; auto& next=ramped.clips[1];
        first.outroAnchor=first.sourceElapsed(2.5-first.start);
        next.introAnchor=next.sourceElapsed(2.5-next.start);
        ramped.transitionLinks[0].frame=std::array<double,2>{*first.outroAnchor,first.sourceElapsed(3.5-first.start)};
        auto rigid=ramped; const auto rigidBefore=json(rigid);
        check(rigid.moveClipGroup({a,b},b,next.start+.5,next.track,false).failed() && json(rigid)==rigidBefore,
            "explicit multi-selection keeps its rigid clock contract across a ramp");
        auto musical=ramped; musical.setTransitionMovementLocked(b,true);
        const auto movedPair=musical.moveClip(b,next.start+.5,false);
        check(movedPair.wasOk() && musical.tempo.points==ramped.tempo.points
            && musical.clips[0].start>first.start && std::abs(musical.clips[1].start-next.start-.5)<1.e-8
            && std::abs(musical.clips[1].length-next.length)>1.e-4 && std::abs(musical.clips[1].sourceSpan()-next.sourceSpan())<1.e-8
            && std::abs(join(musical,a,false)-join(musical,b,true))<1.e-8,
            "single-header locked pair retimes both tracks, keeps source spans and joins, and leaves tempo points fixed / "+movedPair.getErrorMessage());
        auto earlierClock=ramped; earlierClock.tempo.setPoint(7,128.4,true); earlierClock.tempo.setPoint(10,144,true);
        const auto earlierBefore=json(earlierClock);
        check(earlierClock.moveClip(b,7.5,false).failed() && json(earlierClock)==earlierBefore,
            "moving a captured playback clock into a different future ramp cannot create unreopenable geometry");
        auto agreesLocally = ramped;
        agreesLocally.tempo.setPoint(100, 144, false);
        const auto localEdit = agreesLocally.moveTransitionAnchor(b, true, 2.9, false);
        check(localEdit.wasOk() && std::abs(join(agreesLocally,b,true) - join(ramped,a,false)) < 1.e-7,
            "a captured map that agrees at the new placement permits an unlocked IN drag / " + localEdit.getErrorMessage());
        const auto pcm=[](const Project& p,double at) {Engine e;e.sampleRate=8000;e.project=p;juce::AudioBuffer<float> audio(2,64);juce::MidiBuffer midi;e.render(audio,64,at,midi);return audio;};
        auto treated=fixture; treated.applyTransitionPreset(b,TransitionPreset::crossfade); treated.transitionLinks[0].muteOutside=true;
        auto kept=treated; const auto removed=kept.removeTransition(b,false); float keepError=0;
        for(double at:{1.,2.3,3.5,4.5}) {const auto x=pcm(treated,at),y=pcm(kept,at);for(int ch=0;ch<2;++ch)for(int i=0;i<64;++i)keepError=std::max(keepError,std::abs(x.getSample(ch,i)-y.getSample(ch,i)));}
        check(removed.wasOk() && !kept.transitionFor(b) && kept.transitionFor(c) && kept.transitionFor(d) && keepError<2.e-6f,"remove just one pair/frame and keep actual heard PCM, including exterior cuts / "+removed.getErrorMessage());
        auto cleared=treated; const auto clear=cleared.removeTransition(b,true); const auto clearAudio=pcm(cleared,3),plainAudio=pcm(fixture,3);float clearError=0;
        for(int ch=0;ch<2;++ch)for(int i=0;i<64;++i)clearError=std::max(clearError,std::abs(clearAudio.getSample(ch,i)-plainAudio.getSample(ch,i)));
        check(clear.wasOk() && clearError<2.e-6f && cleared.transitionFor(c) && cleared.transitionFor(d),"clear treatment removes this pair's frame automation without deleting neighbouring frames / "+clear.getErrorMessage());
        Workstation ui(false);ui.stopTimer();ui.setSize(1440,940);ui.engine.sampleRate=8000;ui.timeline.pixelsPerSecond=80;ui.layout.followPlayhead=false;
        const auto reset=[&](const Project& p) {ui.engine.setPlaying(false);ui.engine.project=p;ui.selectedClip=ui.selectedTransition=b;ui.clearEditSelection();ui.undoHistory.clear();ui.redoHistory.clear();ui.refresh();ui.viewport.setViewPosition(0,0);};
        const auto mouse=juce::Desktop::getInstance().getMainMouseSource();
        const auto event=[&](juce::Point<float> at,juce::Point<float> down,bool drag,bool free)
        {return juce::MouseEvent(mouse,at,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier|(free?juce::ModifierKeys::altModifier:0)),1,0,0,0,0,&ui.timeline,&ui.timeline,juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),1,drag);};
        for(bool intro:{false,true})
        {
            reset(fixture);const auto id=intro?b:a;const auto* placed=ui.engine.project.clipFor(id);const auto bounds=ui.timeline.clipBounds(*placed);
            juce::Point<float> down;bool hittable=false;
            for(float y:{bounds.getY()+28,bounds.getCentreY(),bounds.getBottom()-14})
            { const juce::Point<float> point{(float)(join(ui.engine.project,id,intro)*80),y};const auto hit=ui.timeline.anchorAt(point);
                if(hit && hit->first==id && hit->second==intro && !ui.timeline.transitionAt(point) && !ui.timeline.automationPointAt(point)) {down=point;hittable=true;break;} }
            check(hittable,"ordinary waveform IN/OUT marker has a reachable gesture target / "+juce::String((int)intro));
            if(hittable)
            {
                const juce::Point<float> to{(float)((intro?3.:2.64)*80),down.y};
                ui.timeline.mouseDown(event(down,down,false,false));
                check(ui.timeline.anchorDrag.has_value() && !ui.timeline.rangeDragAnchor,"marker mouse-down reaches alignment instead of waveform range selection");
                ui.timeline.mouseDrag(event(to,down,true,false));ui.timeline.mouseUp(event(to,down,true,false));
                const auto& actual=ui.engine.project;
                check(actual.clips[0].start==fixture.clips[0].start && (intro ? std::abs(join(actual,b,true)-fixedJoin)<1.e-7 : std::abs(join(actual,a,false)-2.5)<1.e-7)
                    && actual.clips[1].start!=fixture.clips[1].start && actual.clips[2].start==fixture.clips[2].start && ui.undoHistory.size()==1,
                    "actual IN/OUT mouse handlers implement stationary OUT owner and fixed-join IN / "+juce::String((int)intro)+" / "+ui.status);
                ui.undo(false);check(json(ui.engine.project)==json(fixture),"marker gesture Undo restores complete pre-drag pair geometry");
            }
        }
        for(bool free:{false,true})
        {
            reset(fixture);const AutomationAddress address{0,AutomationTarget::volume,{},{},a};
            ui.engine.project.effectSettings(ui.engine.project.clips[0]).automation[0]={true,{{0,-9},{1,-6},{4,-9}}};
            ui.timeline.selectAutomationCurve(address);ui.refresh();
            const juce::Point<float> down{(float)(1.17*80),ui.timeline.automationY(0,ui.timeline.automationDisplayValue(address,1.17,-6),0)},to{(float)(1.71*80),down.y};
            ui.timeline.mouseDown(event(down,down,false,free));ui.timeline.mouseDrag(event(to,down,true,free));ui.timeline.mouseUp(event(to,down,true,free));
            const auto* curve=ui.engine.project.curve(address);const double at=curve && curve->points.size()>1 ? ui.engine.project.automationSessionTime(address,curve->points[1].time):-1;
            check(std::abs(at-(free?1.71:1.5))<1.e-5,"actual source-node drag uses project grid despite off-grid clip/source clocks; Alt is free / "+juce::String((int)free));
        }
        reset(fixture); const auto beforeUi=json(ui.engine.project);
        auto* pairButton=ui.timeline.movementLockButtons[b].get();
        check(pairButton && pairButton->isVisible() && pairButton->getButtonText()=="Unlocked" && pairButton->getTitle().contains("Pair movement lock"),"LINK replaced by an accessible actual pair-specific movement button");
        if(pairButton)pairButton->onClick();
        check(ui.engine.project.transitionFor(b)->movementLocked && !ui.engine.project.transitionFor(c)->movementLocked && ui.undoHistory.size()==1,"actual pair button changes only its own movement lock in one Undo");
        ui.undo(false);check(json(ui.engine.project)==beforeUi,"Undo restores exact pair movement state");ui.undo(true);
        check(ui.engine.project.transitionFor(b)->movementLocked,"Redo restores the pair movement lock");
        media->analysisSource={audioDigest(media->file),media->file.getSize(),8000,media->frames(),1};check(writeAnalysisRecord(media->file,media->analysisRecord()).isEmpty(),"seed owned marker fixture analysis without detection");
        const auto file=report.getSiblingFile("pair-movement.mixworks");const auto saved=ui.engine.saveProject(file);Engine reopened;const auto opened=saved.isEmpty()?reopened.restoreProject(file.loadFileAsString()):saved;
        check(saved.isEmpty() && opened.isEmpty() && reopened.project.transitionFor(b) && reopened.project.transitionFor(c)
            && reopened.project.transitionFor(b)->movementLocked && !reopened.project.transitionFor(c)->movementLocked,"actual source-backed save/reopen retains pair movement locks / "+opened);

        const auto captureTimeline = [&](const juce::String& name)
        {
            auto stream = report.getSiblingFile(name).createOutputStream();
            return stream && juce::PNGImageFormat().writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *stream);
        };
        reset(loneTrack);
        check(captureTimeline("manual-lone-out.png"), "capture the marked OUT tail before adding the next track");
        reset(out);
        check(captureTimeline("manual-paired-out.png"), "capture the marked transition area across a prepared pair");
        const auto markedFile = report.getSiblingFile("manual-area.mixworks");
        const auto markedSaved = ui.engine.saveProject(markedFile);
        auto restored = std::make_unique<Engine>();
        const auto markedOpened = markedSaved.isEmpty() ? restored->restoreProject(markedFile.loadFileAsString()) : markedSaved;
        check(markedOpened.isEmpty() && restored->project.clips[0].manualTransitionArea
            && std::abs(join(restored->project, a, false) - join(out, a, false)) < 1.e-8,
            "source-backed save and reopen preserves the manual transition area and its anchor / " + markedOpened);

        auto stale = fixture; stale.clips.resize(2); stale.transitionLinks.resize(1); stale.followTrackTempo = true;
        stale.tempo.setPoint(0, 120); auto captured = stale.tempo; captured.setPoint(0, 118);
        auto& outgoing = stale.clips[0]; auto& incomingClip = stale.clips[1];
        outgoing.beatmatched = incomingClip.beatmatched = true;
        outgoing.retime(outgoing.start, std::make_shared<const TempoMap>(stale.tempo));
        incomingClip.retime(incomingClip.start, std::make_shared<const TempoMap>(captured));
        outgoing.outroAnchor = outgoing.sourceElapsed(incomingClip.start - outgoing.start);
        incomingClip.introAnchor = 0.; stale.transitionLinks[0].frame = std::array<double, 2>{*outgoing.outroAnchor, 4.};
        const auto staleBefore = json(stale);
        const double joinTime = incomingClip.start;
        const auto refused = stale.moveTransitionAnchor(b, true, joinTime + .25, false);
        check(refused.failed() && refused.getErrorMessage().contains("Sync playback clocks") && json(stale) == staleBefore,
            "a mismatched captured playback clock refuses an unlocked IN edit with a safe recovery step");
        reset(stale);
        const auto staleFile = report.getSiblingFile("stale-clock.mixworks");
        check(ui.engine.saveProject(staleFile).contains("Sync playback clocks") && !staleFile.exists(),
            "Save refuses an unreadable clock mismatch before creating a file");
        const auto oldFrame = stale.transitionRange(*stale.transitionFor(b));
        ui.syncTransitionPlaybackClocks(b);
        const auto& synced = ui.engine.project;
        const auto syncedFrame = synced.transitionRange(*synced.transitionFor(b));
        check(!synced.transitionHasStaleClock(b) && ui.undoHistory.size()==1
            && synced.clips[1].start == incomingClip.start && synced.clips[1].length == incomingClip.length
            && std::abs(syncedFrame.getStart()-oldFrame.getStart()) < 1.e-7 && std::abs(syncedFrame.getEnd()-oldFrame.getEnd()) < 1.e-7
            && std::abs(join(synced,b,true)-joinTime) < 1.e-7
            && std::abs(synced.clips[1].sourceTimeAt(1.) - incomingClip.sourceTimeAt(1.)) < 1.e-7,
            "Sync playback clocks keeps placements and the join in one Undo");
        ui.undo(false);
        check(ui.engine.project.transitionHasStaleClock(b) && json(ui.engine.project) == staleBefore,
            "Undo restores the original captured timing after a clock sync");
        ui.undo(true);
        const auto syncedSaved = ui.engine.saveProject(staleFile);
        const auto recovered = syncedSaved.isEmpty() ? restored->restoreProject(staleFile.loadFileAsString()) : syncedSaved;
        if (recovered.isEmpty())
        {
            const auto recoveredEdit = restored->project.moveTransitionAnchor(b, true, joinTime + .25, false);
            check(recoveredEdit.wasOk() && std::abs(join(restored->project,a,false) - joinTime) < 1.e-7
                && std::abs(join(restored->project,b,true) - joinTime) < 1.e-7,
                "synced clocks survive Save and Open and an unlocked IN edit keeps the join / " + recoveredEdit.getErrorMessage());
        }
        else check(false, "open the saved stale-clock fixture / " + recovered);

        for (const double zoom : {.1, 80., WorkspaceLayout::maximumTimelineZoom})
        {
            reset(fixture); ui.timeline.pixelsPerSecond = zoom; ui.timeline.updateSize();
            ui.viewport.setViewPosition(std::max(0, (int) std::round(fixedJoin * zoom - 90)), 0);
            auto* button = ui.timeline.movementLockButtons[b].get();
            const auto bounds = ui.timeline.clipBounds(fixture.clips[1]);
            const auto tab = ui.timeline.offscreenAnchorBounds(fixture.clips[1], true);
            const auto handle = tab.isEmpty() ? juce::Point<float>((float)(fixedJoin * zoom), bounds.getY() + 3) : tab.getCentre();
            const auto hit = ui.timeline.anchorAt(handle, true);
            const bool separate = button && button->isVisible() && !button->getBounds().toFloat().contains(handle)
                && hit && hit->first == b && hit->second;
            if (button && button->isVisible()) button->onClick();
            check(separate && ui.engine.project.transitionFor(b)->movementLocked,
                "pair lock and IN each have an independent target at zoom " + juce::String(zoom));
            if (button && button->isVisible()) button->onClick();
            check(!ui.engine.project.transitionFor(b)->movementLocked, "the visible toggle unlocks the same pair at zoom " + juce::String(zoom));
        }
        ui.timeline.pixelsPerSecond = 80.; reset(out);
        const auto completeMix = json(ui.engine.project);
        for (const int count : {2, 3, 4})
        {
            ui.audioLanes.setSelectedId(count, juce::sendNotificationSync);
            check(ui.timeline.contentLaneCount() == count && ui.strips[(size_t) (count-1)]->isVisible()
                && (count == trackCount || !ui.strips[(size_t) count]->isVisible()) && json(ui.engine.project) == completeMix,
                "visible audio lane selector preserves every placement at " + juce::String(count));
        }
        ui.audioLanes.setSelectedId(2, juce::sendNotificationSync);
        check(captureTimeline("manual-two-lanes.png"), "capture the two-lane timeline while higher-lane tracks remain in the mix");
        const auto countBefore = ui.engine.project.clips.size();
        ui.insertTimelineMedia({media}, TimelinePlacement{2, 10.});
        check(ui.engine.project.clips.size() == countBefore, "a hidden lane rejects new placement");
        ui.insertTimelineMedia({media}, TimelinePlacement{1, 10.});
        check(ui.engine.project.clips.size() == countBefore + 1 && ui.engine.project.clips.back().track == 1,
            "the last visible audio lane accepts normal placement");
        ui.audioLanes.setSelectedId(4, juce::sendNotificationSync);
        reset(treated);ui.selectTransition(b,false);ui.showToolPage(1);ui.refresh();ui.removeTransitionButton.onClick();
        auto* question=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        check(question && question->getName()=="Remove transition?" && ui.removeTransitionButton.isEnabled(),"visible Remove transition opens explicit keep-mix/clear-treatment choices");
        const auto pump=[] {
            #if JUCE_WINDOWS
            MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
            #endif
        };
        if(question) {auto capture=report.getSiblingFile("remove-transition-question.png").createOutputStream();check(capture && juce::PNGImageFormat().writeImageToStream(question->createComponentSnapshot(question->getLocalBounds()),*capture),"capture real remove-transition choices");question->exitModalState(0);pump();}
        check(json(ui.engine.project)==json(treated) && ui.undoHistory.empty(),"Cancel removal keeps the complete treatment and pair");
        ui.removeTransitionButton.onClick();question=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());if(question){question->exitModalState(1);pump();}
        check(!ui.engine.project.transitionFor(b) && ui.engine.project.transitionFor(c) && ui.undoHistory.size()==1,"confirmed visible removal removes only the selected pair");
        ui.undo(false);check(json(ui.engine.project)==json(treated),"Undo visible removal restores complete paired treatment");
        ui.selectTransition(b,false);ui.showToolPage(1);ui.refresh();auto capture=report.getSiblingFile("pair-lock-controls.png").createOutputStream();
        check(capture && juce::PNGImageFormat().writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()),*capture),"capture actual pair buttons, IN/OUT markers, movement lock and Remove controls");
        return;
    }
    if (juce::JUCEApplicationBase::getCommandLineParameters().contains("--edge-trim-only"))
    {
        media->analysisSource={audioDigest(media->file),media->file.getSize(),8000,media->frames(),1};
        check(writeAnalysisRecord(media->file,media->analysisRecord()).isEmpty(),"seed only owned trim fixture analysis to avoid detector work on reopen");
        Workstation ui(false); ui.stopTimer(); ui.setSize(1440,940); ui.engine.sampleRate=8000;
        ui.timeline.pixelsPerSecond=160; ui.layout.followPlayhead=false; base.followTrackTempo=false; base.tempo.points.clear(); base.snap=false; base.masterDb=0;
        base.clips[0].fadeIn=base.clips[0].fadeOut=0; base.clips[0].controls={}; base.clips[1].effects=makeClipEffects();
        const auto json=[](const Project& p) { return juce::JSON::toString(projectToJson(p),true,17); };
        auto reset=[&] { ui.engine.setPlaying(false); ui.engine.project=base; ui.engine.position=1.5;
            ui.selectedClip=clip.id; ui.selectedTransition.clear(); ui.undoHistory.clear(); ui.redoHistory.clear(); ui.refresh(); ui.viewport.setViewPosition(0,0); };
        const auto mouse=juce::Desktop::getInstance().getMainMouseSource();
        auto event=[&](juce::Point<float> at,juce::Point<float> down,bool dragging,bool free)
        { return juce::MouseEvent(mouse,at,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier|(free?juce::ModifierKeys::altModifier:0)),
            1,0,0,0,0,&ui.timeline,&ui.timeline,juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),1,dragging); };
        auto drag=[&](bool left,double delta,bool free)
        { const auto* c=ui.engine.project.clipFor(clip.id); const auto down=ui.timeline.edgeTrimBounds(*c,left).getCentre();
          const auto at=down.translated((float)(delta*ui.timeline.pixelsPerSecond),0);
          ui.timeline.mouseDown(event(down,down,false,free)); ui.timeline.mouseDrag(event(at,down,true,free)); ui.timeline.mouseUp(event(at,down,true,free)); };
        auto render=[&](Project p,double at)
        { Engine engine; engine.sampleRate=8000; p.tracks[1].mute=true; engine.project=std::move(p);
          juce::AudioBuffer<float> pcm(2,64); juce::MidiBuffer midi; engine.render(pcm,64,at,midi); return pcm; };
        reset(); const auto original=json(ui.engine.project); const auto frame=base.transitionRange(base.transitionLinks[0]);
        const double sourceBefore=base.clips[0].sourceTimeAt(1.5);
        drag(true,1,true); drag(false,-1,true);
        const auto& trimmed=*ui.engine.project.clipFor(clip.id);
        check(trimmed.start==1 && trimmed.end()==3 && trimmed.offset==1,"ordinary left/right edge drags trim the retained source range");
        check(ui.engine.project.clips[1].start==2 && trimmed.sourceTimeAt(1.5-trimmed.start)==sourceBefore
            && ui.engine.position==1.5,"edge trims preserve other placement, source alignment and playhead");
        check(ui.engine.project.transitionRange(ui.engine.project.transitionLinks[0])==frame
            && trimmed.start+trimmed.transitionTime(false)==2,"transition frame and OUT marker stay independent of audio edges");
        check(ui.status.contains("excluded audio") && ui.engine.project.validateTransitionLinks().wasOk(),"excluded transition audio has an explicit consequence and remains a valid project");
        const auto before=render(ui.engine.project,.5), after=render(ui.engine.project,3.5), retained=render(ui.engine.project,1.5), reference=render(base,1.5);
        float error=0; for(int ch=0;ch<2;++ch)for(int i=0;i<64;++i)error=std::max(error,std::abs(retained.getSample(ch,i)-reference.getSample(ch,i)));
        check(before.getMagnitude(0,64)==0 && after.getMagnitude(0,64)==0 && retained.getMagnitude(0,64)>.01 && error<1.e-6f,
            "rendered PCM excludes both trimmed intervals and preserves the retained sound");
        const auto trimmedDocument=json(ui.engine.project); ui.undo(false); ui.undo(false);
        check(json(ui.engine.project)==original,"Undo restores both edge gestures exactly"); ui.undo(true); ui.undo(true);
        check(json(ui.engine.project)==trimmedDocument,"Redo restores both trims exactly");
        auto pixels=report.getSiblingFile("edge-trim-conflict.png").createOutputStream();
        check(pixels && juce::PNGImageFormat().writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()),*pixels),"capture distinct trim edges with retained transition markers");
        const auto file=report.getSiblingFile("edge-trim.mixworks"); const auto save=ui.engine.saveProject(file); Engine reopened;
        const auto opened=save.isEmpty()?reopened.restoreProject(file.loadFileAsString()):save;
        check(save.isEmpty() && opened.isEmpty() && reopened.project.clips[0].start==1 && reopened.project.clips[0].end()==3
            && reopened.project.transitionRange(reopened.project.transitionLinks[0])==frame,"actual save/reopen retains trim and independent frame: "+opened);
        if(opened.isEmpty())
        { juce::String exportError; auto exporter=reopened.createExportRenderer(exportError);
          check(exporter && exportError.isEmpty(),"trimmed source admits actual offline export renderer");
          if(exporter) { const auto pcm=render(exporter->project,3.5); check(pcm.getMagnitude(0,64)==0,"export path cannot restore excluded tail audio"); }
          ui.engine.project=reopened.project; ui.refresh(); drag(true,-1,true); drag(false,1,true);
          check(ui.engine.project.clips[0].start==0 && ui.engine.project.clips[0].end()==4 && ui.engine.project.clips[0].offset==0,
            "extending both edges after reopen restores the available original audio"); }
        reset(); ui.engine.project.snap=true; drag(true,.73,false);
        check(ui.engine.project.clips[0].start==.5,"ordinary edge drag snaps to the project beat");
        reset(); ui.engine.project.snap=true; drag(true,.73,true);
        check(std::abs(ui.engine.project.clips[0].start-.73125)<.01,"Alt edge drag bypasses beat snapping");
        reset(); ui.engine.project.setTransitionLocked(partner.id,true); const auto locked=json(ui.engine.project); drag(false,-1,true);
        check(json(ui.engine.project)==locked && ui.undoHistory.empty() && ui.status.contains("Unlock"),"locked trim explains refusal without edits or history");
        for(const auto mode:{"raw","reverse","loop","clip-local","elastic"})
        {
            auto p=base; auto& c=p.clips[0]; c.introAnchor=0; const juce::String name(mode);
            if(name=="reverse")c.reversed=true; if(name=="loop")c.loopSpan=2;
            if(name=="clip-local") { auto prepared=std::make_shared<PreparedAudio>(); prepared->clipLocal=true; prepared->samples.makeCopyOf(media->samples); c.prepared=prepared; }
            if(name=="elastic")
            { p.followTrackTempo=true; p.tempo.setPoint(0,120); c.beatmatched=c.elasticPlayback=true; c.tempoSourceBpm=120;
              c.tempoMap=std::make_shared<const TempoMap>(p.tempo); c.elasticSource=std::make_shared<const ElasticSource>(c); }
            const auto expected=render(p,2.75);
            const auto originalFrame=p.transitionRange(p.transitionLinks[0]);
            check(p.trimClip(clip.id,2.5,3).wasOk() && p.validateTransitionLinks().wasOk()
                && p.transitionRange(p.transitionLinks[0])==originalFrame,"trim beyond OUT retains off-segment markers / "+name);
            const auto kept=render(p,2.75), excluded=render(p,2.25); float difference=0;
            for(int ch=0;ch<2;++ch)for(int i=0;i<64;++i)difference=std::max(difference,std::abs(kept.getSample(ch,i)-expected.getSample(ch,i)));
            check(difference<1.e-5f && kept.getMagnitude(0,64)>.001f && excluded.getMagnitude(0,64)==0,
                "trim preserves retained PCM and excludes source head / "+name+" / error "+juce::String(difference,8));
            const auto parsed=parseProject(juce::JSON::toString(projectToJson(p),true,17));
            check(parsed.error.isEmpty(),"signed off-segment transition frame parses / "+name+" / "+parsed.error);
        }
        auto shared=base; shared.clips[1].effects=shared.clips[0].effects;
        const auto expectedShared=render(shared,2.75);
        check(shared.trimClip(clip.id,2.5,3).wasOk(),"trim one shared piece across its partner's start");
        const auto actualShared=render(shared,2.75); float sharedError=0;
        for(int ch=0;ch<2;++ch)for(int i=0;i<64;++i)sharedError=std::max(sharedError,std::abs(expectedShared.getSample(ch,i)-actualShared.getSample(ch,i)));
        check(sharedError<1.e-6f && shared.clips[0].routeStart()==0,"left trim preserves shared-return lane ownership across overlapping pieces");
        const auto sharedParsed=parseProject(juce::JSON::toString(projectToJson(shared),true,17));
        check(sharedParsed.error.isEmpty() && sharedParsed.clips[0].clip.routeStart()==0,"shared routing clock survives session parsing");
        check(audioDigest(media->file)==media->analysisSource.sha256,"original fixture hash is unchanged after every trim gesture");
        return;
    }
    const auto json = [](const Project& p) { return juce::JSON::toString(projectToJson(p)); };
    auto changed = base; auto expected = base; expected.clips[0].audioSlip = .65;
    check(changed.setClipAudioSlip(clip.id, .65).wasOk() && json(changed) == json(expected)
        && changed.transitionRange(changed.transitionLinks[0]) == base.transitionRange(base.transitionLinks[0]),
        "slipping changes only this clip's audio clock, retaining all automation, fade/IN/OUT anchors, partner and transition geometry");
    check(changed.clips[0].sourceElapsed(1) == base.clips[0].sourceElapsed(1)
        && std::abs(changed.clips[0].sourceTimeAt(1) - .35) < 1.e-9
        && std::abs(changed.clips[0].firstDownbeatTime() - .863) < 1.e-8,
        "audio source and detected downbeat move while the automation clock stays fixed");
    auto invalid = changed; const auto valid = json(invalid);
    check(invalid.setClipAudioSlip(clip.id, std::numeric_limits<double>::quiet_NaN()).failed()
        && invalid.setClipAudioSlip("missing", 0).failed() && json(invalid) == valid, "invalid audio slip is failure-atomic");
    invalid.setTransitionLocked(partner.id, true); const auto locked = json(invalid);
    check(invalid.setClipAudioSlip(clip.id, .2).failed() && invalid.setClipAudioSlip(partner.id, .2).failed() && json(invalid) == locked,
        "transition protection covers either participating audio slip without changing the pair");
    for (const auto mode : {"raw", "reverse", "loop", "prepared", "clip-local", "elastic"})
    {
        Clip c = clip; c.introAnchor.reset(); c.outroAnchor.reset(); c.controls = {}; c.fadeIn = c.fadeOut = 0;
        const juce::String name(mode);
        if (name == "reverse") c.reversed = true;
        if (name == "loop") c.loopSpan = 1.5;
        if (name == "prepared" || name == "clip-local")
        {
            auto audio = std::make_shared<PreparedAudio>(); audio->rate = 1; audio->clipLocal = name == "clip-local";
            audio->samples.makeCopyOf(media->samples); c.prepared = audio;
        }
        if (name == "elastic")
        {
            c.beatmatched = c.elasticPlayback = true; c.tempoSourceBpm = 120;
            c.tempoMap = std::make_shared<const TempoMap>(base.tempo); c.elasticSource = std::make_shared<const ElasticSource>(c);
        }
        Engine reference, slipped; reference.sampleRate = slipped.sampleRate = 8000;
        reference.project.clips = {c}; c.audioSlip = .25; slipped.project.clips = {c};
        juce::AudioBuffer<float> a(2, 256), b(2, 256); juce::MidiBuffer midi;
        reference.render(a, 256, 1, midi); slipped.render(b, 256, 1.25, midi);
        double difference = 0; for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 256; ++i) difference = std::max(difference, (double) std::abs(a.getSample(ch, i) - b.getSample(ch, i)));
        check(difference < 1.e-6 && a.getRMSLevel(0, 0, 256) > .01, "slipped " + name + " audio reads the same payload at its new time: " + juce::String(difference, 8));
        auto sliced = c.slice(.875, 2); slipped.project.clips = {sliced}; slipped.render(b, 256, 1.25, midi);
        difference = 0; for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 256; ++i) difference = std::max(difference, (double) std::abs(a.getSample(ch, i) - b.getSample(ch, i)));
        check(difference < 1.e-6, "splitting retains the slipped " + name + " sound and captured payload");
        if (name == "raw" || name == "clip-local" || name == "elastic")
        {
            slipped.project.clips = {c}; slipped.render(b, 256, 0, midi);
            check(b.getRMSLevel(0, 0, 256) < .00001 && std::isfinite(b.getRMSLevel(0, 0, 256)),
                "slipped " + name + " source head pads with silence instead of holding its nonzero first sample");
        }
    }
    {
        Clip plain = clip; plain.controls = {}; plain.fadeIn = plain.fadeOut = 0; plain.audioSlip = .5;
        Clip automated = plain; automated.controls.automation[0] = {true, {{0, -20}, {.8, -20}, {.81, 0}, {4, 0}}};
        Engine control, moving; control.sampleRate = moving.sampleRate = 8000; control.project.clips = {plain}; moving.project.clips = {automated};
        juce::AudioBuffer<float> a(2, 64), b(2, 64); juce::MidiBuffer midi; control.render(a, 64, 1, midi); moving.render(b, 64, 1, midi);
        double difference = 0; for (int i = 0; i < 64; ++i) difference = std::max(difference, (double) std::abs(a.getSample(0, i) - b.getSample(0, i)));
        check(difference < 1.e-6 && a.getRMSLevel(0, 0, 64) > .01, "audible automation stays at mix time instead of following the slipped audio clock");
    }
    auto forwardMoveTests = [&]
    {
        Project movement; movement.followTrackTempo = false; movement.bpm = 120; movement.snap = false;
        for (int i = 0; i < 4; ++i)
        {
            Clip c; c.media = media; c.track = i; c.start = i == 3 ? 10 : i * 2; c.length = 4;
            c.introAnchor = .5; c.outroAnchor = 2.5; c.fadeInAnchor = .5; c.fadeOutAnchor = 2.5; c.fadeIn = c.fadeOut = 1.5;
            movement.clips.push_back(c);
        }
        movement.transitionLinksKnown = true;
        for (int i = 0; i < 2; ++i)
        {
            auto& out = movement.clips[(size_t) i]; auto& in = movement.clips[(size_t) i + 1];
            movement.linkClips(out.id, in.id);
            auto& a = out.controls.automation[1]; auto& b = in.controls.automation[1]; a.enabled = b.enabled = true;
            a.setPoint(2.75, 0); a.setPoint(3.25, -60); b.setPoint(.75, -60); b.setPoint(1.25, 0);
            movement.transitionLinks[(size_t) i].bassSwap = std::array<double, 4>{2.75, 3.25, .75, 1.25};
        }
        const auto id = movement.clips[1].id;
        const auto incoming = movement.transitionRange(movement.transitionLinks[0]);
        const auto outgoing = movement.transitionRange(movement.transitionLinks[1]);
        const auto earlier = juce::JSON::toString(projectToJson(movement)["clips"][0]);
        for (double delta : {-.2, .2})
        {
            auto moved = movement; const auto result = moved.moveClipFollowing(id, 2 + delta, false);
            check(result.wasOk() && moved.validateTransitionLinks().wasOk(), "forward move retains paired transition ownership: " + result.getErrorMessage());
            if (result.failed()) continue;
            check(std::abs(moved.clips[1].start - 2 - delta) < 1e-8 && std::abs(moved.clips[2].start - 4 - delta) < 1e-8
                && std::abs(moved.clips[3].start - 10 - delta) < 1e-8, "selected box, linked successor and unlinked later track move together");
            check(juce::JSON::toString(projectToJson(moved)["clips"][0]) == earlier && moved.transitionRange(moved.transitionLinks[0]) == incoming,
                "the entire earlier clip and incoming transition stay at their exact original positions");
            const auto after = moved.transitionRange(moved.transitionLinks[1]);
            check(std::abs(after.getStart() - outgoing.getStart() - delta) < 1e-8 && std::abs(after.getEnd() - outgoing.getEnd() - delta) < 1e-8
                && moved.transitionLinks[1] == movement.transitionLinks[1], "later transition window moves intact with its source-owned bass treatment");
            const auto& pair = *moved.transitionLinks[0].bassSwap;
            check(std::abs(moved.clips[1].start + moved.clips[1].localTimeForSource(pair[2]) - 2.75) < 1e-8
                && std::abs(moved.clips[1].start + moved.clips[1].localTimeForSource(pair[3]) - 3.25) < 1e-8,
                "incoming bass endpoints stay fixed while the selected audio and box move");
            const auto parsed = parseProject(json(moved));
            check(parsed.error.isEmpty() && parsed.clips.size() == 4 && std::abs(parsed.clips[1].clip.start - 2 - delta) < 1e-8,
                "forward geometry and pinned anchors survive the existing session format");
        }
        for (double target : {-1., 3., std::numeric_limits<double>::quiet_NaN()})
        {
            auto rejected = movement; const auto before = json(rejected);
            check(rejected.moveClipFollowing(id, target, false).failed() && json(rejected) == before, "invalid/out-of-frame forward movement rejects atomically");
        }
        for (const auto& lockId : {movement.clips[1].id, movement.clips[2].id})
        {
            auto locked = movement; locked.setTransitionLocked(lockId, true); const auto before = json(locked);
            check(locked.moveClipFollowing(id, 2.2, false).failed() && json(locked) == before, "incoming and later transition locks protect the whole forward edit");
        }
        auto pending = movement; pending.clips[3].pendingPlacement = PendingPlacement::append; const auto beforePending = json(pending);
        check(pending.moveClipFollowing(id, 2.2, false).failed() && json(pending) == beforePending, "pending later tracks cannot be partly moved");
        auto snapped = movement; const auto aligned = snapped.moveClipFollowing(id, 2.26, true);
        check(aligned.wasOk() && std::abs(snapped.clips[1].start - 2.5) < 1e-6
            && std::abs(snapped.clips[2].start - 4.5) < 1e-6
            && std::abs(snapped.clips[1].firstBeatTime() - movement.clips[1].firstBeatTime()) < 1e-6,
            "forward Snap moves by whole beats and retains both manually placed source phases");
        auto mapped = movement; mapped.followTrackTempo = true; mapped.tempo = TempoMap {120, {{0, 120, false}, {8, 126, false}}};
        const auto map = std::make_shared<const TempoMap>(mapped.tempo);
        for (auto& c : mapped.clips) { c.beatmatched = true; c.tempoSourceBpm = 120; c.tempoMap = map; c.length = c.localTimeForSource(4); c.rate = 4 / c.length; }
        const auto beforeMapped = mapped;
        const auto mappedResult = mapped.moveClipFollowing(id, 2.2, false);
        check(mappedResult.wasOk() && std::abs(mapped.tempo.beatsAt(mapped.clips[3].start) - mapped.tempo.beatsAt(beforeMapped.clips[3].start) - .4) < 1e-7,
            "an unlinked later track carries musical displacement through a different project tempo");
        auto dry = std::make_unique<Engine>(juce::File(), false); auto wet = std::make_unique<Engine>(juce::File(), false);
        dry->sampleRate = wet->sampleRate = 8000; dry->project = wet->project = movement;
        check(wet->project.moveClipFollowing(id, 2.2, false).wasOk(), "prepare a forward render comparison");
        juce::AudioBuffer<float> a(2, 64), b(2, 64); juce::MidiBuffer midi;
        dry->render(a, 64, 1, midi); wet->render(b, 64, 1, midi); double difference = 0;
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 64; ++i) difference = std::max(difference, (double) std::abs(a.getSample(ch, i) - b.getSample(ch, i)));
        check(difference < 1e-7 && a.getRMSLevel(0, 0, 64) > .001, "audio before the pinned incoming transition remains sample-identical");
        dry->resetPlugins(); wet->resetPlugins(); dry->render(a, 64, 5, midi); wet->render(b, 64, 5.2, midi); difference = 0;
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 64; ++i) difference = std::max(difference, (double) std::abs(a.getSample(ch, i) - b.getSample(ch, i)));
        check(difference < 1e-5 && a.getRMSLevel(0, 0, 64) > .001, "later transition audio and treatment translate together through production rendering");
        auto owned = std::make_unique<Workstation>(false); auto& desk = *owned; desk.stopTimer(); desk.setSize(1920, 1080);
        desk.engine.project = movement; desk.engine.library = {media}; desk.timeline.pixelsPerSecond = 100; desk.refresh();
        const auto mouse = juce::Desktop::getInstance().getMainMouseSource();
        auto event = [&](juce::Point<float> p, juce::Point<float> down, bool drag)
        { return juce::MouseEvent(mouse, p, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::altModifier), 1, 0, 0, 0, 0, &desk.timeline, &desk.timeline, juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), 1, drag); };
        const auto down = desk.timeline.audioSlipBounds(desk.engine.project.clips[1]).getCentre();
        desk.timeline.mouseDown(event(down, down, false)); desk.timeline.mouseDrag(event(down.translated(20, 0), down, true));
        desk.engine.project.masterDb = -7;
        desk.timeline.mouseDrag(event(down.translated(32, 0), down, true)); desk.timeline.mouseDrag(event(down.translated(32, 0), down, true));
        desk.timeline.mouseUp(event(down.translated(32, 0), down, true));
        check(std::abs(desk.engine.project.clips[1].start - 2.32) < 1e-7 && std::abs(desk.engine.project.clips[3].start - 10.32) < 1e-7
            && desk.undoHistory.size() == 1 && desk.engine.project.masterDb == -7 && desk.engine.project.clips[1].audioSlip == 0,
            "default native handle drag moves the boxes repeatedly with one Undo and retains live master changes");
        desk.undo(false); check(json(desk.engine.project) == json(movement), "Undo restores the complete forward handle gesture"); desk.undo(true);
        check(std::abs(desk.engine.project.clips[2].start - 4.32) < 1e-7, "Redo restores the translated following arrangement");
        auto stream = report.getSiblingFile("forward-move-pinned.png").createOutputStream(); juce::PNGImageFormat png;
        check(stream && png.writeImageToStream(desk.createComponentSnapshot(desk.getLocalBounds()), *stream), "capture native forward movement with the fixed incoming transition");
    };
    forwardMoveTests();

    Workstation ui(false); ui.stopTimer(); ui.engine.project = base; ui.engine.library = {media}; ui.setSize(1440, 940);
    ui.timeline.pixelsPerSecond = 100; ui.refresh(); ui.engine.position = 1.125; ui.engine.playing = true;
    const auto mouse = juce::Desktop::getInstance().getMainMouseSource();
    auto event = [&](juce::Point<float> point, juce::Point<float> down, bool drag, int modifiers = 0)
    { return juce::MouseEvent(mouse, point, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::shiftModifier | modifiers), 1, 0, 0, 0, 0, &ui.timeline, &ui.timeline, juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), 1, drag); };
    const auto firstHandle = ui.timeline.audioSlipBounds(ui.engine.project.clips[0]), secondHandle = ui.timeline.audioSlipBounds(ui.engine.project.clips[1]);
    check(!firstHandle.isEmpty() && !secondHandle.isEmpty() && ui.timeline.audioSlipAt(firstHandle.getCentre()) == clip.id
        && ui.timeline.audioSlipAt(secondHandle.getCentre()) == partner.id, "every timeline clip has its own independently hit-testable move / Shift-slip handle");
    const auto down = firstHandle.getCentre();
    ui.timeline.mouseDown(event(down, down, false)); ui.timeline.mouseUp(event(down, down, false));
    check(ui.undoHistory.empty() && ui.engine.position == 1.125, "clicking the slip icon selects its track without moving audio, seeking or creating Undo");
    ui.timeline.mouseDown(event(down, down, false));
    ui.timeline.mouseDrag(event(down.translated(40, 0), down, true, juce::ModifierKeys::altModifier));
    ui.engine.project.masterDb = -8;
    ui.timeline.mouseDrag(event(down.translated(65, 0), down, true, juce::ModifierKeys::altModifier));
    ui.timeline.mouseUp(event(down.translated(65, 0), down, true));
    expected.masterDb = -8;
    check(json(ui.engine.project) == json(expected) && ui.undoHistory.size() == 1 && ui.engine.position == 1.125 && ui.engine.playing,
        "native free slip gesture changes only its audio, retains concurrent edits/live transport and creates one Undo");
    const auto slippedState = json(ui.engine.project); ui.undo(false); ui.undo(true);
    check(json(ui.engine.project) == slippedState, "audio slip round-trips through actual Undo and Redo");
    ui.timeline.mouseDoubleClick(event(down, down, false));
    check(ui.engine.project.clips[0].audioSlip == 0 && ui.engine.position == 1.125 && ui.undoHistory.size() == 2,
        "double-click resets only audio slip with one Undo and no seek");
    ui.undo(false);
    ui.timeline.mouseDown(event(down, down, false)); ui.timeline.mouseDrag(event(down.translated(17, 0), down, true)); ui.timeline.mouseUp(event(down, down, true));
    const auto& snappedClip = ui.engine.project.clips[0];
    check(std::abs(std::remainder(snappedClip.start + snappedClip.firstBeatTime(), .5)) < 1.e-8, "slip snapping aligns an actual musical pulse to the mix beat");
    const double beforeLeft = ui.engine.project.clips[0].audioSlip;
    ui.timeline.mouseDown(event(down, down, false)); ui.timeline.mouseDrag(event(down.translated(-237, 0), down, true, juce::ModifierKeys::altModifier)); ui.timeline.mouseUp(event(down, down, true));
    check(std::abs(ui.engine.project.clips[0].audioSlip - beforeLeft + 2.37) < 1.e-8, "first-track audio slips left past mix zero without moving clip boundaries");
    check(std::abs(base.tempo.snapWithPreroll(-.76, true) + 1) < 1.e-8 && base.tempo.snapWithPreroll(-.76, false) == -.76,
        "signed slip snapping extends the first mix tempo while free edits retain their exact offset");
    ui.setBusy(true); const auto busyState = json(ui.engine.project); const auto busyUndo = ui.undoHistory.size();
    ui.timeline.mouseDown(event(down, down, false)); ui.timeline.mouseDrag(event(down.translated(20, 0), down, true)); ui.timeline.mouseUp(event(down, down, true));
    check(json(ui.engine.project) == busyState && ui.undoHistory.size() == busyUndo, "busy state rejects the audio slip gesture atomically"); ui.setBusy(false);
    ui.engine.playing = false; const auto session = root.getChildFile("slipped.mixworks"); Engine reopened;
    const auto saveError = ui.engine.saveProject(session), openError = reopened.restoreProject(session.loadFileAsString());
    check(saveError.isEmpty() && openError.isEmpty() && reopened.project.clips[0].audioSlip == ui.engine.project.clips[0].audioSlip,
        "production Save/Open retains audio slip: " + saveError + openError);
    const auto aFile = root.getChildFile("slipped.wav"), bFile = root.getChildFile("reopened.wav");
    const auto aError = ui.engine.exportWav(aFile), bError = reopened.exportWav(bFile);
    auto left = std::unique_ptr<juce::AudioFormatReader>(ui.engine.formats.createReaderFor(aFile));
    auto right = std::unique_ptr<juce::AudioFormatReader>(ui.engine.formats.createReaderFor(bFile));
    bool sameRender = aError.isEmpty() && bError.isEmpty() && left && right && left->lengthInSamples == right->lengthInSamples;
    double renderDifference = 0;
    if (sameRender)
    {
        juce::AudioBuffer<float> a(2, 512), b(2, 512);
        for (juce::int64 offset = 0; offset < left->lengthInSamples; offset += 512)
        {
            const int frames = (int) std::min<juce::int64>(512, left->lengthInSamples - offset);
            sameRender &= left->read(&a, 0, frames, offset, true, true) && right->read(&b, 0, frames, offset, true, true);
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < frames; ++i) renderDifference = std::max(renderDifference, (double) std::abs(a.getSample(ch, i) - b.getSample(ch, i)));
        }
    }
    check(sameRender && renderDifference <= 4. / 8388608, "slipped mix survives production Save/Open/export within four PCM24 steps: " + juce::String(renderDifference, 9) + aError + bError);
    auto document = projectToJson(ui.engine.project); auto* row = document["clips"].getArray()->getReference(0).getDynamicObject(); row->setProperty("audioSlip", "bad");
    check(parseProject(juce::JSON::toString(document)).error.isNotEmpty(), "session boundary rejects a malformed audio slip");
    row->removeProperty("audioSlip"); document.getDynamicObject()->setProperty("version", 21); document["tracks"].getArray()->removeRange(4, trackCount - 4);
    const auto old = parseProject(juce::JSON::toString(document));
    check(old.error.isEmpty() && old.clips[0].clip.audioSlip == 0, "format 21 sessions retain their original unslipped playback");
    auto stream = report.getSiblingFile("audio-slip-handles.png").createOutputStream(); juce::PNGImageFormat png;
    check(stream && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *stream), "capture native per-track slip icons and fixed transition/automation layout");
}
}
