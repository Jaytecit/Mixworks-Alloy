#include "Workstation.h"
#include "PresetNodes.h"
#include "Beatmatch.h"
#include <cmath>
#include <thread>

namespace mixworks
{
void runPresetNodeTests(const std::function<void(bool,const juce::String&)>& check,const juce::File& report)
{
    if (juce::JUCEApplicationBase::getCommandLineParameters().contains("--composite-fades-only"))
    {
        auto source=std::make_shared<Media>(); source->name="Composite fade fixture"; source->sampleRate=8000; source->manualBpm=120; source->manualBeatOrigin=0; source->manualKey=0;
        source->samples.setSize(1,36*8000);
        for (int n=0;n<source->frames();++n) source->samples.setSample(0,n,.1f*(float)std::sin(n*.17));
        source->file=report.getSiblingFile("composite-source.wav");
        { juce::WavAudioFormat format; std::unique_ptr<juce::OutputStream> stream=source->file.createOutputStream();
          auto writer=format.createWriterFor(stream,juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(1).withBitsPerSample(24));
          check(writer && writer->writeFromAudioSampleBuffer(source->samples,0,source->frames()),"write owned composite source"); }
        const int bucket=std::max(1,source->frames()/4000);
        for (int first=0;first<source->frames();first+=bucket) source->peaks.push_back(source->samples.getMagnitude(0,first,std::min(bucket,source->frames()-first)));
        source->prepareWaveformBody(); source->analysisSource={audioDigest(source->file),source->file.getSize(),8000,source->frames(),1};
        check(writeAnalysisRecord(source->file,source->analysisRecord()).isEmpty(),"seed composite source without detector work");
        const auto json=[](const Project& p) { return juce::JSON::toString(projectToJson(p),true,17); };
        const auto gain=[](double db) { return db<=-160 ? 0. : juce::Decibels::decibelsToGain(db); };
        Project gestureProject;
        for (bool ramp : {false,true}) for (int mode=0;mode<6;++mode)
        {
            Project p; p.transitionLinksKnown=true; p.followTrackTempo=ramp;
            if (ramp) p.tempo.points={{0,120},{20,150,true},{44,100,true}};
            for (int n=0;n<3;++n)
            { Clip c; c.media=source; c.track=n; c.start=n*8.; c.length=24;
              if (ramp) { c.tempoMap=std::make_shared<TempoMap>(p.tempo); c.tempoSourceBpm=120; } p.clips.push_back(c); }
            for (int n=1;n<3;++n)
            { Project::TransitionLink link; link.outgoing=p.clips[(size_t)n-1].id; link.incoming=p.clips[(size_t)n].id;
              link.frame=std::array<double,2>{p.clips[(size_t)n-1].sourceElapsed(n*8.-p.clips[(size_t)n-1].start),p.clips[(size_t)n-1].sourceElapsed(n*8.+16-p.clips[(size_t)n-1].start)};
              p.transitionLinks.push_back(link); }
            const auto id=p.clips[1].id; const int target=mode==2 ? 2 : mode==3 ? 1 : 0;
            const AutomationAddress address{1,(AutomationTarget)target,{},{},id};
            if (mode!=1) for (auto& link : p.transitionLinks)
            { TransitionBandSettings setting; setting.mode=mode==3 ? TransitionBandMode::swap : mode==0 || mode==5 ? TransitionBandMode::constantPower : TransitionBandMode::crossfade;
              setting.strength=mode==2 ? .45 : 1.; setting.bend=mode==4 || mode==3 ? std::array<double,2>{} : std::array<double,2>{-.25,.35}; link.bands[(size_t)target]=setting; }
            if (mode==1) { p.clips[1].fadeIn=18; p.clips[1].fadeOut=18; p.clips[1].fadeInAnchor=0; p.clips[1].fadeOutAnchor=p.clips[1].sourceElapsed(6); }
            if (mode==0 || mode==2 || mode==3 || mode==5)
            {
                auto& c=p.effectSettings(p.clips[1]).automation[(size_t)target]; c={true,{{0,-3,-.2},{10,-8,.3},{26,-4}}};
                if (mode==5) { c={true,{{0,-12,0,false,true},{26,-3,0,false,true}}}; c.points[0].shape=AutomationShape{}; c.points[0].shape->kind=1; }
            }
            const auto before=p; const auto converted=materialiseEditablePresetNodes(p,address); const auto* curve=p.curve(address);
            const auto label=" / case "+juce::String(mode)+" / ramp "+juce::String((int)ramp);
            check(converted.wasOk(),"compose without adaptive fitting"+label+" / "+converted.getErrorMessage());
            if (converted.failed() || !curve || curve->points.size()<2) continue;
            std::set<double> expected;
            const auto& original=before.clips[1]; const auto clock=before.transitionPlaybackFor(original);
            expected.insert(before.automationTime(address,original.start)); expected.insert(before.automationTime(address,original.end()));
            if (const auto* old=before.curve(address)) for (const auto& point : old->points) expected.insert(point.time);
            for (const auto& pair : clock.pairs) if (pair[(size_t)target].strength>0)
            { expected.insert(before.automationTime(address,pair[(size_t)target].begin)); expected.insert(before.automationTime(address,pair[(size_t)target].end)); }
            if (mode==1) for (double t : {8.,26.,14.,32.}) expected.insert(before.automationTime(address,t));
            bool sparse=curve->points.size()==expected.size();
            for (const auto& point : curve->points) sparse &= expected.count(point.time)>0;
            check(sparse,"only authored nodes and real fade endpoints exist"+label+" / nodes "+juce::String((int)curve->points.size()));
            double error=0;
            for (int n=0;n<=1024;++n)
            {
                const double t=original.start+original.length*n/1024.,x=before.automationTime(address,t);
                const auto* manual=before.curve(address); double v=gain(manual ? manual->valueAt(x,before.manualValue(address)) : before.manualValue(address));
                if (t<original.end()) { v*=clock.gainAt(target,t); if (target==0) v*=original.envelope().gainAt(t-original.start); }
                error=std::max(error,std::abs(gain(curve->valueAt(x,0))-v));
            }
            check(error<3.e-6,"analytical composite matches independent control values"+label+" / gain error "+juce::String(error,9));
            const double a=before.automationTime(address,18),b=before.automationTime(address,22);
            auto inserted=*curve; inserted.setPoint(a,curve->valueAt(a,0)); const auto slice=curve->sliced(a,b,0); bool same=true;
            for (int n=0;n<=128;++n)
            { const double x=a+(b-a)*n/128.; same &= std::abs(gain(inserted.valueAt(x,0))-gain(curve->valueAt(x,0)))<3.e-6;
              same &= std::abs(gain(slice.valueAt(x-a,0))-gain(curve->valueAt(x,0)))<3.e-6; }
            check(same,"composite insertion and slicing retain interiors"+label);
            const auto parsed=parseProject(json(p)); check(parsed.error.isEmpty(),"session53 retains composite formulas"+label+" / "+parsed.error);
            TransitionStyle::Nodes nodes,decoded; const auto capture=capturePresetNodes(p,id,nodes); const auto read=readPresetNodes(presetNodesToJson(nodes),decoded);
            check(capture.wasOk() && read.wasOk() && nodes==decoded,"portable composite formula round-trip"+label+" / "+capture.getErrorMessage()+read.getErrorMessage());
            if (target==0)
            {
                Engine originalEngine,convertedEngine; originalEngine.project=before; convertedEngine.project=p; originalEngine.sampleRate=convertedEngine.sampleRate=8000;
                originalEngine.prepareClipProcessing(); convertedEngine.prepareClipProcessing(); double pcmError=0; juce::MidiBuffer midi;
                for (double time : {12.,18.,20.,25.})
                { juce::AudioBuffer<float> aPcm(2,128),bPcm(2,128); originalEngine.render(aPcm,128,time,midi); convertedEngine.render(bPcm,128,time,midi);
                  for (int ch=0;ch<2;++ch) for (int n=0;n<128;++n) pcmError=std::max(pcmError,(double)std::abs(aPcm.getSample(ch,n)-bPcm.getSample(ch,n))); }
                check(pcmError<3.e-6,"rendered composite volume PCM matches original layered fades"+label+" / error "+juce::String(pcmError,9));
            }
            if (!ramp && mode==4)
            {
                const double left=gain(curve->valueAt(8,0)),right=gain(curve->valueAt(16,0)),middle=gain(curve->valueAt(12,0));
                check(std::abs(left-right)<1.e-6 && middle>left+.05,"equal endpoints retain an analytical overlap crest");
                gestureProject=p;
            }
        }
        if (!gestureProject.clips.empty())
        {
            Workstation ui(false); ui.stopTimer(); ui.setSize(1600,950); ui.engine.project=gestureProject; ui.engine.sampleRate=8000;
            const auto id=gestureProject.clips[1].id; const AutomationAddress address{1,AutomationTarget::volume,{},{},id};
            ui.selectedClip=id; ui.engine.selectedTrack=1; ui.timeline.selectAutomationCurve(address); ui.refresh();
            ui.engine.prepareClipProcessing(); ui.engine.seek(20); ui.engine.setPlaying(true); ui.engine.monitorMuted=false;
            juce::Array<juce::var> facts; int callbacks=0; bool healthy=true,blocked=false; juce::String action;
            const auto probe=[&]
            {
                const double at=ui.engine.position.load(); const auto misses=ui.engine.callbackDiagnostics.snapshot().lockMisses;
                auto* fact=new juce::DynamicObject();
                std::thread callback([&]
                { Engine oracle; oracle.sampleRate=8000; oracle.project=ui.engine.project; oracle.prepareClipProcessing(); juce::MidiBuffer midi;
                  juce::AudioBuffer<float> actual(2,128),expected(2,128); oracle.render(expected,128,at,midi);
                  ui.engine.audioDeviceIOCallbackWithContext(nullptr,0,actual.getArrayOfWritePointers(),2,128,{});
                  double error=0; for (int ch=0;ch<2;++ch) for (int n=0;n<128;++n) error=std::max(error,(double)std::abs(actual.getSample(ch,n)-expected.getSample(ch,n)));
                  const auto after=ui.engine.callbackDiagnostics.snapshot().lockMisses;
                  if (action=="held-lock-control") blocked=actual.getMagnitude(0,128)==0 && ui.engine.position==at && after==misses+1;
                  else healthy &= actual.getMagnitude(0,128)>.001 && error<3.e-6 && std::abs(ui.engine.position-(at+128./8000))<1.e-8 && after==misses;
                  fact->setProperty("action",action); fact->setProperty("peak",actual.getMagnitude(0,128)); fact->setProperty("pcmError",error);
                  fact->setProperty("positionBefore",at); fact->setProperty("positionAfter",ui.engine.position.load()); fact->setProperty("lockMissesBefore",(juce::int64)misses); fact->setProperty("lockMissesAfter",(juce::int64)after);
                }); callback.join(); facts.add(juce::var(fact)); ++callbacks;
            };
            action="held-lock-control"; { const juce::ScopedLock guard(ui.engine.lock); probe(); }
            check(blocked,"negative control observes real silence and held clock under the callback lock");
            const auto normal=ui.statusAccess.read; bool probing=false;
            ui.statusAccess.read=[&] { if (probing) probe(); return normal ? normal() : juce::String(); };
            const auto mouse=juce::Desktop::getInstance().getMainMouseSource();
            const auto event=[&](float x,float y) { return juce::MouseEvent(mouse,{x,y},juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,&ui.timeline,&ui.timeline,juce::Time::getCurrentTime(),{x,y-20},juce::Time::getCurrentTime(),1,true); };
            action="bend-status"; probing=true; const int beforeBend=callbacks;
            ui.timeline.curveBendDrag=Timeline::CurveBendGesture{address,1,{},0,0,250,true}; ui.timeline.moved=false;
            ui.timeline.continueCurveBend(event(800,270)); probing=false; ui.timeline.curveBendDrag.reset();
            check(callbacks>beforeBend && healthy,"bend preparation/publication/status permits actual callback PCM and advancing clock");
            action="endpoint-status"; probing=true; const int beforeDrag=callbacks;
            ui.timeline.editingAutomationPoint=std::make_pair(address,2); ui.timeline.moved=false; ui.timeline.pixelsPerSecond=40;
            ui.timeline.mouseDrag(event(24.5f*40,ui.timeline.automationY(1,-6,(int)AutomationTarget::volume))); probing=false;
            check(callbacks>beforeDrag && healthy,"endpoint drag/status permits actual callback PCM and advancing clock");
            action="insert-status"; probing=true; const int beforeInsert=callbacks;
            const bool inserted=ui.insertAutomationNode(address,19.5,false); probing=false;
            check(inserted && callbacks>beforeInsert && healthy,"composite node insertion permits callback PCM during UI notification");
            action="delete-status"; probing=true; const int beforeDelete=callbacks;
            const bool deleted=ui.deleteAutomationNode(address,0); probing=false;
            check(deleted && callbacks>beforeDelete && healthy,"composite node deletion permits callback PCM during UI notification");
            ui.statusAccess.read=normal; ui.engine.setPlaying(false);
            check(report.getSiblingFile("composite-callbacks.json").replaceWithText(juce::JSON::toString(juce::var(facts),true,17)),"retain PCM, clock and lock observations at actual editor boundaries");
            const auto file=report.getSiblingFile("composite.mixworks"); const auto save=ui.engine.saveProject(file); Engine reopened;
            const auto open=save.isEmpty() ? reopened.restoreProject(file.loadFileAsString()) : save;
            check(open.isEmpty(),"actual edited composite saves and reopens from owned audio / "+open);
            juce::String error; auto exporter=ui.engine.createExportRenderer(error); bool same=exporter && error.isEmpty() && open.isEmpty();
            if (same) { exporter->sampleRate=reopened.sampleRate=8000; juce::AudioBuffer<float> a(2,128),b(2,128); juce::MidiBuffer midi;
              exporter->render(a,128,20,midi); reopened.render(b,128,20,midi);
              for (int ch=0;ch<2;++ch) for (int n=0;n<128;++n) same &= std::abs(a.getSample(ch,n)-b.getSample(ch,n))<3.e-6; }
            check(same,"normal export and actual reopened composite PCM agree");
            ui.timeline.updateSize(); ui.viewport.setViewPosition(12*40,0);
            auto stream=report.getSiblingFile("composite-ui.png").createOutputStream();
            check(stream && juce::PNGImageFormat().writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()),*stream),"capture composite curve and candidate version");
            auto conflict=gestureProject; auto& filter=conflict.effectSettings(conflict.clips[1]).automation[5]; filter={true,{{0,.2f},{24,.5f}}};
            const auto set=conflict.setTransitionFilter(id,{TransitionFilterMode::highToLow,.5}); const auto before=json(conflict);
            const auto refused=materialiseEditablePresetNodes(conflict,{1,AutomationTarget::cutoff,{},{},id});
            check(set.wasOk() && refused.failed() && before==json(conflict) && refused.getErrorMessage().contains("manual"),"incompatible manual/filter composition explains refusal and preserves curves");
        }
        AutomationShape bad; bad.kind=4; bad.terms={AutomationShape{}}; std::optional<AutomationShape> result;
        check(!readAutomationShape(automationShapeToJson(bad),result),"malformed composite formula is refused");
        return;
    }
    if (juce::JUCEApplicationBase::getCommandLineParameters().contains("--shaped-fades-only"))
    {
        auto source = std::make_shared<Media>(); source->name = "Direct fade fixture"; source->sampleRate = 8000;
        source->manualBpm = 120; source->manualBeatOrigin = 0; source->samples.setSize(1,24*8000);
        for (int i = 0; i < source->frames(); ++i) source->samples.setSample(0,i,.1f*(float)std::sin(i*.17));
        source->file = report.getSiblingFile("shaped-fade-source.wav");
        { juce::WavAudioFormat format; std::unique_ptr<juce::OutputStream> stream = source->file.createOutputStream();
          const auto options = juce::AudioFormatWriterOptions{}.withSampleRate(8000).withNumChannels(1).withBitsPerSample(24);
          auto writer = format.createWriterFor(stream,options);
          check(writer && writer->writeFromAudioSampleBuffer(source->samples,0,source->frames()),"write an owned source for real save/reopen"); }
        const int bucket = std::max(1,source->frames()/4000);
        for (int first = 0; first < source->frames(); first += bucket) source->peaks.push_back(source->samples.getMagnitude(0,first,std::min(bucket,source->frames()-first)));
        source->prepareWaveformBody();
        source->analysisSource = {audioDigest(source->file),source->file.getSize(),8000,source->frames(),1};
        check(writeAnalysisRecord(source->file,source->analysisRecord()).isEmpty(),"seed owned source metadata without detector work");
        const auto json = [](const Project& p) { return juce::JSON::toString(projectToJson(p),true,17); };
        for (bool ramp : {false,true}) for (int treatment = 0; treatment < 7; ++treatment)
        {
            Project p; p.transitionLinksKnown = true; p.followTrackTempo = ramp;
            if (ramp) p.tempo.points = {{0,120},{12,150,true},{24,100,true}};
            for (int side = 0; side < 2; ++side)
            { Clip c; c.media = source; c.track = side; c.start = side*8.; c.length = 16;
              if (ramp) { c.tempoMap = std::make_shared<TempoMap>(p.tempo); c.tempoSourceBpm = 120; }
              p.clips.push_back(c); }
            const auto id = p.clips[1].id;
            Project::TransitionLink link; link.outgoing = p.clips[0].id; link.incoming = id;
            link.frame = std::array<double,2>{p.clips[0].sourceElapsed(8),p.clips[0].sourceElapsed(16)};
            p.transitionLinks.push_back(link);
            int target = treatment >= 5 ? 5 : treatment == 4 ? 1 : treatment == 3 ? 2 : 0;
            juce::Result set = juce::Result::ok();
            if (treatment >= 5) set = p.setTransitionFilter(id,{treatment == 5 ? TransitionFilterMode::highToLow : TransitionFilterMode::lowToHigh,.65});
            else if (treatment == 0)
            { p.clips[0].fadeOutAnchor = p.clips[0].sourceElapsed(8); p.clips[0].fadeOut = p.clips[1].fadeIn = 8; }
            else
            { TransitionBandSettings setting; setting.mode = treatment == 1 ? TransitionBandMode::constantPower : treatment == 4 ? TransitionBandMode::swapFade : TransitionBandMode::crossfade;
              setting.strength = treatment == 3 ? .2 : 1; setting.bend = {-.35,.42}; set = p.setTransitionBand(id,target,setting); }
            const auto before = p;
            const auto compiled = materialisePresetNodes(p,id,1u<<target);
            bool sparse = set.wasOk() && compiled.wasOk(), values = sparse, slicing = sparse;
            for (int side = 0; side < 2; ++side)
            {
                const auto& c = p.clips[(size_t)side]; const auto& curve = p.effectSettings(c).automation[(size_t)target];
                sparse &= curve.points.size() == 2;
                if (curve.points.size() != 2) continue;
                const auto& old = before.clips[(size_t)side]; const auto playback = before.transitionPlaybackFor(old);
                auto split = curve; const double mid = (curve.points.front().time+curve.points.back().time)*.5;
                split.setPoint(mid,curve.valueAt(mid,0)); const auto slice = curve.sliced(curve.points.front().time,mid,0);
                for (int n = 0; n <= 257; ++n)
                {
                    const double time = 8+8.*n/257; const double x = c.sourceElapsed(time-c.start);
                    const auto& envelope = playback.filters[(size_t)(1-side)]; const double amount = envelope.amountAt(time);
                    const double hz = envelope.highPass() ? 20*std::pow(180.,.8*amount) : 3600*std::pow(180.,-.8*amount);
                    const double expected = target == 5 ? (envelope.highPass() ? (hz-20)/19980 : (hz-20000)/19980)*std::min(1.,amount/.02)
                        : playback.gainAt(target,time)*(target == 0 ? old.envelope().gainAt(time-old.start) : 1);
                    const double heard = target == 5 ? curve.valueAt(x,0) : juce::Decibels::decibelsToGain((double)curve.valueAt(x,0),-160.);
                    values &= std::abs(heard-expected) < 2.e-6;
                    slicing &= std::abs(split.valueAt(x,0)-curve.valueAt(x,0)) < 3.e-5;
                    if (x >= curve.points.front().time && x <= mid) slicing &= std::abs(slice.valueAt(x-curve.points.front().time,0)-curve.valueAt(x,0)) < 3.e-5;
                }
            }
            const auto label = " / treatment " + juce::String(treatment) + " / tempo ramp " + juce::String((int)ramp);
            check(sparse,"generated fade contains exactly two endpoints"+label+" / "+set.getErrorMessage()+compiled.getErrorMessage());
            check(values,"direct curve matches independent gain/frequency formula"+label);
            check(slicing,"insertion and slicing preserve direct shape"+label);
            if (sparse)
            {
                auto held = p; auto& curve = held.effectSettings(held.clips[0]).automation[(size_t)target];
                curve.points.front().bend = 1;
                const double first = curve.points.front().time, last = (first+curve.points.back().time)*.5;
                const auto original = curve; curve = curve.sliced(first,last,0);
                bool exact = true;
                for (int n = 0; n <= 16; ++n) exact &= curve.valueAt((last-first)*n/16,0) == original.valueAt(first+(last-first)*n/16,0);
                check(exact && parseProject(json(held)).error.isEmpty(),"hard-cut edit slices and saves its held native shape"+label);
            }
            const auto parsed = parseProject(json(p));
            check(parsed.error.isEmpty(),"session52 reads native shapes"+label+" / "+parsed.error);
            TransitionStyle::Nodes portable; const auto captured = capturePresetNodes(p,id,portable); TransitionStyle::Nodes decoded;
            const auto decodedResult = readPresetNodes(presetNodesToJson(portable),decoded);
            check(captured.wasOk() && decodedResult.wasOk() && portable == decoded,"portable preset retains native shapes"+label+" / "+captured.getErrorMessage()+decodedResult.getErrorMessage());
            if (captured.wasOk() && decodedResult.wasOk() && !(portable == decoded))
                report.getSiblingFile("portable-shape-difference-"+juce::String((int)ramp)+"-"+juce::String(treatment)+".json").replaceWithText(juce::JSON::toString(presetNodesToJson(portable),true,17)+"\n"+juce::JSON::toString(presetNodesToJson(decoded),true,17));
            auto reapplied = p;
            for (auto& c : reapplied.clips) reapplied.effectSettings(c).automation[(size_t)target] = {};
            const auto applied = captured.wasOk() ? applyPresetNodes(reapplied,id,portable,1u<<target) : juce::Result::fail("capture failed");
            bool same = applied.wasOk();
            for (size_t side = 0; same && side < p.clips.size(); ++side)
                for (int n = 0; n <= 257; ++n)
                { const double x = p.clips[side].sourceElapsed(8+8.*n/257-p.clips[side].start);
                  same &= std::abs(p.effectSettings(p.clips[side]).automation[(size_t)target].valueAt(x,0)
                      -reapplied.effectSettings(reapplied.clips[side]).automation[(size_t)target].valueAt(x,0)) < 3.e-5; }
            check(same,"preset placement retains direct curvature under the receiving tempo map"+label+" / "+applied.getErrorMessage());
            if (sparse && values && !ramp && treatment == 1)
            {
                Workstation ui(false); ui.stopTimer(); ui.setSize(1600,950); ui.engine.project = p;
                ui.selectedClip = id; ui.selectedTransition = id; ui.engine.selectedTrack = 1; ui.selectedAutomationTarget = 0;
                ui.timeline.pixelsPerSecond = 45; ui.refresh(); ui.timeline.updateSize();
                ui.remember(); auto* curve = ui.engine.project.curve({1,AutomationTarget::volume,{},{},id});
                curve->movePoint(1,curve->points.back().time+.5,0); const auto edited = json(ui.engine.project);
                ui.undo(false); const bool undone = json(ui.engine.project) == json(p); ui.undo(true);
                check(undone && json(ui.engine.project) == edited,"endpoint edit Undo/Redo preserves exact shape and project");
                ui.undo(false);
                const auto file = report.getSiblingFile("direct-fades.mixworks"); const auto saved = ui.engine.saveProject(file);
                Engine reopened; const auto opened = saved.isEmpty() ? reopened.restoreProject(file.loadFileAsString()) : saved;
                check(opened.isEmpty() && reopened.project.clips.size() == 2,"actual source-backed save/reopen retains two-node fades / "+opened);
                juce::String error; auto exporter = ui.engine.createExportRenderer(error); bool pcm = exporter && error.isEmpty();
                if (exporter)
                { exporter->sampleRate = 8000; reopened.sampleRate = 8000; juce::MidiBuffer midi;
                  for (double time : {8.01,10.,12.,15.9}) { juce::AudioBuffer<float> a(2,128),b(2,128); exporter->render(a,128,time,midi); reopened.render(b,128,time,midi);
                    for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 128; ++i) pcm &= std::abs(a.getSample(ch,i)-b.getSample(ch,i)) < 2.e-6; } }
                check(pcm,"export renderer and reopened source PCM agree through the shaped fade");
                auto stream = report.getSiblingFile("shaped-fades-ui.png").createOutputStream(); juce::PNGImageFormat png;
                check(stream && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()),*stream),"capture actual two-endpoint curve and app version");
            }
        }
        AutomationShape shape; auto invalid = automationShapeToJson(shape); invalid.getDynamicObject()->setProperty("from",2.);
        std::optional<AutomationShape> parsed; check(!readAutomationShape(invalid,parsed),"malformed shape is refused without mutation");
        return;
    }
    auto media = std::make_shared<Media>(); media->name = "Preset node source"; media->sampleRate = 8000; media->manualBpm = 120; media->manualBeatOrigin = 0;
    media->samples.setSize(1,96 * 8000);
    for (int i = 0; i < media->frames(); ++i) media->samples.setSample(0,i,(float) (.2 * std::sin(i * .13)));
    Project legacy; legacy.transitionLinksKnown = true;
    for (int i = 0; i < 3; ++i)
    { Clip c; c.media = media; c.track = i; c.start = i * 32.; c.length = 48;
      c.introAnchor = c.fadeInAnchor = 0; c.outroAnchor = 32; c.fadeOutAnchor = 48; legacy.clips.push_back(c); }
    for (int i = 1; i < 3; ++i)
    { Project::TransitionLink link; link.outgoing = legacy.clips[(size_t) i-1].id; link.incoming = legacy.clips[(size_t) i].id;
      link.bands[1] = TransitionBandSettings{TransitionBandMode::swap,1,0,.5}; legacy.transitionLinks.push_back(link); }
    const auto id = legacy.clips[1].id; const AutomationAddress address{1,AutomationTarget::bass,{},{},id};
    legacy.effectSettings(legacy.clips[1]).automation[1] = {true,{{0,0},{7.9999,0},{8,0}}};
    // Put the outgoing step one device sample beyond a normal32-sample EQ tick.
    legacy.transitionLinks[1].bands[1]->position += 1. / (16 * media->sampleRate);
    const double cut = legacy.transitionPlaybackFor(legacy.clips[1]).pairs[1][1].begin;
    const double cutSource = legacy.automationTime(address,cut);
    check(legacy.validateTransitionLinks().wasOk() && legacy.transitionRange(legacy.transitionLinks[1]) == juce::Range<double>(64,80),
        "node-preset fixture has real interior frame anchors and valid outgoing swap geometry");
    const auto json = [](const Project& p) { return juce::JSON::toString(projectToJson(p),true,17); };
    {
        Project pending;
        pending.clips = {legacy.clips[0],legacy.clips[1]};
        pending.clips[1].pendingPlacement = PendingPlacement::append;
        const auto placed = prepareTempoMap(pending,false,16,0,0,{},TransitionPreset::crossfade);
        const auto* join = placed.project.transitionFor(id);
        bool clean = placed.error.isEmpty() && join && join->alignmentOnly;
        for (const auto& clip : placed.project.clips)
        {
            clean &= clip.fadeIn == 0 && clip.fadeOut == 0;
            for (const auto& curve : placed.project.effectSettings(clip).automation) clean &= curve.points.empty();
        }
        check(clean,"appending at OUT aligns IN without applying the selected Crossfade: " + placed.error);
        if (clean)
        {
            Workstation ui(false); ui.stopTimer(); ui.setSize(1600,950);
            ui.engine.project = placed.project;
            auto& project = ui.engine.project;
            project.snap = false;
            const auto outId = project.clips[0].id;
            const AutomationAddress outgoing{0,AutomationTarget::volume,{},{},outId};
            const AutomationAddress incoming{1,AutomationTarget::volume,{},{},id};
            const auto marker = project.clips[0].start + project.clips[0].transitionTime(false);
            const double manualTime = project.automationTime(outgoing,marker+3);
            project.effectSettings(project.clips[0]).automation[0] = {true,{{manualTime,-5,.25}}};
            ui.selectTransition(id,false);
            ui.changeSelectedTransition({},TransitionPreset::crossfade);
            const auto fadeAt = [&](const AutomationAddress& address,double time,float value)
            {
                const auto* curve = project.curve(address);
                return curve && std::any_of(curve->points.begin(),curve->points.end(),[&](const auto& point)
                    { return point.presetOwner.isNotEmpty() && point.value == value
                        && std::abs(project.automationSessionTime(address,point.time)-time) < 1.e-8; });
            };
            check(project.transitionFor(id)->presetName == "Crossfade" && fadeAt(outgoing,marker,0) && fadeAt(incoming,marker,-160),
                "explicit Crossfade shows its name and puts both fade start nodes on OUT/IN");
            ui.timeline.pixelsPerSecond = 45; ui.timeline.updateSize();
            ui.viewport.setViewPosition((int)(marker*45)-200,0);
            const auto drag = [&](bool edge,double delta)
            {
                const auto bounds = ui.timeline.transitionBounds(*project.transitionFor(id));
                auto start = bounds.withHeight(18).getCentre();
                if (edge) start.x = bounds.getRight()-1;
                const auto mouse = juce::Desktop::getInstance().getMainMouseSource();
                const auto event = [&](juce::Point<float> position,bool dragging)
                { return juce::MouseEvent(mouse,position,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,&ui.timeline,&ui.timeline,
                    juce::Time::getCurrentTime(),start,juce::Time::getCurrentTime(),1,dragging); };
                ui.timeline.mouseDown(event(start,false));
                const auto end = start.translated((float)(delta*45),0);
                ui.timeline.mouseDrag(event(end,true));
                ui.timeline.mouseUp(event(end,true));
            };
            drag(false,2);
            const auto moved = project.transitionRange(*project.transitionFor(id));
            check(fadeAt(outgoing,marker+2,0) && fadeAt(incoming,marker+2,-160)
                && project.clips[0].start + project.clips[0].transitionTime(false) == marker
                && project.clips[1].start + project.clips[1].transitionTime(true) == marker,
                "native frame drag moves both fade starts and keeps the aligned markers fixed");
            drag(true,2);
            const auto resized = project.transitionRange(*project.transitionFor(id));
            check(std::abs(resized.getEnd()-moved.getEnd()-2) < 1.e-8 && resized.getStart() == moved.getStart()
                && fadeAt(outgoing,resized.getEnd(),-160) && fadeAt(incoming,resized.getEnd(),0),
                "native right-edge drag changes both fade ends with their starts fixed");
            auto image = report.getSiblingFile("owned-crossfade.png").createOutputStream(); juce::PNGImageFormat png;
            check(image && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()),*image),"capture the visible paired fade and its duration handle");
            const double addedTime = project.automationTime(incoming,marker+5);
            project.effectSettings(project.clips[1]).automation[0].setPoint(addedTime,-8);
            const auto beforeRemoval = json(project);
            ui.removeSelectedTransition();
            const auto* after = project.curve(outgoing);
            const auto* added = project.curve(incoming);
            check(project.transitionFor(id)->alignmentOnly && after && after->points.size() == 1 && after->points.front().time == manualTime
                && after->points.front().value == -5 && after->points.front().bend == .25
                && added && added->points.size() == 1 && added->points.front().time == addedTime && added->points.front().value == -8,
                "Remove Preset keeps manual nodes added before and after application, including inside the fade");
            ui.undo(false);
            check(json(project) == beforeRemoval,"Undo restores the exact paired fade and its ownership");

            for (bool deleteIncoming : {false,true})
            {
                project = placed.project;
                project.followTrackTempo = false;
                const auto deletedId = deleteIncoming ? id : outId;
                const auto survivorId = deleteIncoming ? outId : id;
                const auto applied = project.applyTransitionPreset(id,TransitionPreset::bassSwap,true);
                const auto range = project.transitionRange(*project.transitionFor(id));
                auto* survivor = project.clipFor(survivorId);
                const int survivorLane = survivor->track;
                const AutomationAddress bass{survivorLane,AutomationTarget::bass,{},{},survivorId};
                const double manualIn = project.automationTime(bass,range.getStart()+1);
                const double manualOutside = project.automationTime(bass,range.getEnd()+1);
                auto& controls = project.effectSettings(*survivor);
                controls.automation[(size_t)AutomationTarget::cutoff] = {true,{{manualIn,.75f}}};
                controls.automation[(size_t)AutomationTarget::pan] = {true,{{manualIn,.25f},{manualOutside,0}}};
                const auto baseline = placed.project;
                const auto render = [&](Project mix,double time)
                {
                    Engine renderer; renderer.sampleRate = 8000; renderer.project = std::move(mix);
                    for (int lane = 0; lane < trackCount; ++lane) renderer.project.tracks[(size_t)lane].mute = lane != survivorLane;
                    juce::AudioBuffer<float> audio(2,256); juce::MidiBuffer midi;
                    renderer.render(audio,audio.getNumSamples(),time,midi);
                    return audio;
                };
                const auto beforeDelete = json(project);
                ui.clearEditSelection();
                ui.selectedClip = deletedId; ui.selectTrack(project.clipFor(deletedId)->track); ui.refresh();
                ui.removeClip();
                survivor = project.clipFor(survivorId);
                check(applied.wasOk() && survivor && !project.clipFor(deletedId) && project.transitionLinks.empty(),
                    "native Delete removes the selected track and breaks its Bass swap: " + applied.getErrorMessage() + " / " + ui.status);
                if (!survivor || project.clipFor(deletedId)) continue;
                const auto& remaining = project.effectSettings(*survivor).automation;
                check(remaining[(size_t)AutomationTarget::bass].points.empty() && remaining[(size_t)AutomationTarget::cutoff].points.empty()
                    && remaining[(size_t)AutomationTarget::pan].points.size() == 1
                    && remaining[(size_t)AutomationTarget::pan].points.front().time == manualOutside,
                    "Delete removes preset guards and manual nodes in the former frame and keeps the exterior node");
                float error = 0;
                for (double time : {range.getStart()+1,range.getEnd()-1})
                {
                    const auto expected = render(baseline,time), actual = render(project,time);
                    for (int i = 0; i < actual.getNumSamples(); ++i)
                        error = std::max(error,std::abs(actual.getSample(0,i)-expected.getSample(0,i)));
                }
                check(error < 1.e-6f,"the surviving track renders like the untreated source on both sides of the old bass handover");
                ui.undo(false);
                check(json(project) == beforeDelete,"Delete Undo restores the tracks, preset and manual nodes exactly");
                ui.undo(true);

                auto replacement = std::make_shared<Media>();
                replacement->name = "Clean replacement"; replacement->sampleRate = media->sampleRate;
                replacement->manualBpm = 120; replacement->manualBeatOrigin = 0;
                replacement->samples.makeCopyOf(media->samples);
                ui.engine.library = {replacement};
                ui.transitionPreset.setSelectedId(2,juce::dontSendNotification);
                ui.engine.seek(0);
                ui.engine.setPlaying(true);
                ui.addMedia(0,deleteIncoming ? std::nullopt : std::optional<TimelinePlacement>(TimelinePlacement{0,0}));
                ui.engine.setPlaying(false);
                const auto* addedClip = project.clipFor(ui.selectedClip);
                bool defaultCurves = addedClip && addedClip->media == replacement && !project.hasPendingPlacement();
                if (defaultCurves) for (const auto& curve : project.effectSettings(*addedClip).automation) defaultCurves &= curve.points.empty();
                check(defaultCurves && std::all_of(project.transitionLinks.begin(),project.transitionLinks.end(),
                    [](const auto& link) { return link.alignmentOnly && link.presetOwner.isEmpty(); }),
                    "native Add places the replacement cleanly even with Bass swap selected for new transitions: " + ui.status);
                if (deleteIncoming && defaultCurves)
                {
                    ui.timeline.pixelsPerSecond = 24; ui.timeline.updateSize();
                    ui.viewport.setViewPosition((int)(range.getStart()*24)-100,0);
                    auto snapshot = report.getSiblingFile("delete-replace-clean.png").createOutputStream();
                    check(snapshot && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()),*snapshot),
                        "capture the clean replacement on the timeline after Delete");
                }
            }
            auto automatic = placed.project;
            const auto applied = automatic.applyTransitionPreset(id,TransitionPreset::bassSwap);
            automatic.clearDeletedTrackTransitions({outId});
            const auto& automaticBass = automatic.effectSettings(*automatic.clipFor(id)).automation[(size_t)AutomationTarget::bass];
            check(applied.wasOk() && !automaticBass.active() && automatic.transitionLinks.empty(),
                "Delete also clears the unlabelled Bass swap produced by automatic arrangement");
        }
    }
    {
        auto clean = legacy;
        for (auto& c : clean.clips) clean.effectSettings(c).automation = {};
        for (auto& link : clean.transitionLinks) link.bands = {};
        auto none = clean;
        const auto applied = none.applyTransitionPreset(id,TransitionPreset::alignOnly);
        bool empty = applied.wasOk();
        for (const auto& c : none.clips) for (const auto& curve : none.effectSettings(c).automation) empty &= curve.points.empty();
        check(empty,"AUDIT: None does not manufacture neutral nodes on untouched sources/parameters");
        auto swapped = clean;
        const auto bass = swapped.applyTransitionPreset(id,TransitionPreset::bassSwap);
        bool unrelated = bass.wasOk();
        for (const auto& c : swapped.clips) for (int t = 0; t < trackAutomationCount; ++t)
            if (t != 1) unrelated &= swapped.effectSettings(c).automation[(size_t) t].points.empty();
        check(unrelated,"AUDIT: Bass swap creates nodes only for Bass");
        bool distinct = true;
        for (const auto& c : swapped.clips) for (const auto& curve : swapped.effectSettings(c).automation)
            for (size_t n = 1; n < curve.points.size(); ++n)
                distinct &= curve.points[n].time - curve.points[n-1].time > 1.e-10 || curve.points[n].value != curve.points[n-1].value;
        check(distinct,"AUDIT: new preset has no coincident redundant boundary nodes");
        report.getSiblingFile("audit-bass-before.json").replaceWithText(json(swapped));
        Project::BassSwapPairing pairing;
        const auto paired = swapped.pairBassSwap(id,40,pairing);
        check(paired.wasOk(),"AUDIT: RMB pairing accepts the ordinary sharp Bass swap: " + paired.getErrorMessage());
        auto outgoingPair = clean; outgoingPair.applyTransitionPreset(id,TransitionPreset::bassSwap);
        const auto fromOutgoing = outgoingPair.pairBassSwap(clean.clips[0].id,40,pairing);
        check(fromOutgoing.wasOk(),"AUDIT: RMB pairing also works from the outgoing side: " + fromOutgoing.getErrorMessage());
        if (paired.wasOk())
        {
            const auto original = swapped;
            const auto moved = swapped.setTransitionSwapTime(id,1,false,39);
            const auto times = swapped.transitionSwapTimes(*swapped.transitionFor(id),1);
            const auto& curve = swapped.effectSettings(swapped.clips[0]).automation[1];
            check(moved.wasOk() && times && std::abs((*times)[0]-39) < 1.e-9 && std::abs((*times)[1]-39) < 1.e-9
                && std::any_of(curve.points.begin(),curve.points.end(),[](const auto& p) { return p.time == 32 && p.bend == 1; }),
                "AUDIT: paired sharp swap moves earlier while retaining its opening and sharp bend: " + moved.getErrorMessage());
            swapped = original;
            auto guarded = original; auto& guardedCurve = guarded.effectSettings(guarded.clips[0]).automation[1];
            guardedCurve.points.insert(guardedCurve.points.begin(),{std::nextafter(32.,0.),-3,1});
            const auto exact = guarded.setTransitionSwapTime(id,1,false,39);
            check(exact.wasOk() && guarded.effectSettings(guarded.clips[0]).automation[1].points.front().value == -3
                && std::any_of(guarded.effectSettings(guarded.clips[0]).automation[1].points.begin(),guarded.effectSettings(guarded.clips[0]).automation[1].points.end(),[](const auto& node) { return node.time == 32 && node.value == 0; }),
                "AUDIT: paired movement resolves exact owned nodes ahead of adjacent exterior guards: " + exact.getErrorMessage());
        }
        auto window = std::make_unique<Workstation>(false); auto& w = *window; w.stopTimer(); w.setSize(1920,1080);
        w.engine.project = clean; w.engine.project.applyTransitionPreset(id,TransitionPreset::bassSwap);
        w.selectedClip = id; w.selectedTransition = id; w.engine.selectedTrack = 1;
        w.selectedAutomationTarget = (int) AutomationTarget::volume; w.refresh();
        const AutomationAddress neutral{1,AutomationTarget::mid,{},{},id};
        check(w.timeline.automationViewCurve(neutral).points.empty(),"AUDIT: selecting an empty curve does not invent endpoint nodes");
        const auto beforeAdd = json(w.engine.project);
        const auto added = w.insertAutomationNode(neutral,38,false);
        const auto* single = w.engine.project.curve(neutral);
        check(added && single && single->points.size() == 1 && single->points.front().time == 6,
            "AUDIT: one explicit Add Node creates exactly one point at the requested source time");
        w.undo(false); check(json(w.engine.project) == beforeAdd,"AUDIT: single-node Add Undo restores the empty curve");
        w.selectedAutomationTarget = (int) AutomationTarget::volume; w.transitionCurveShown[1]=true; w.refresh();
        w.timeline.pixelsPerSecond = 40; w.timeline.updateSize(); w.viewport.setViewPosition(1200,0);
        const auto amounts = w.timeline.cutAmountHandles();
        check(std::any_of(amounts.begin(),amounts.end(),[&](const auto& h) { return h.address.clipId == clean.clips[0].id && h.address.target == AutomationTarget::bass; })
            && std::any_of(amounts.begin(),amounts.end(),[&](const auto& h) { return h.address.clipId == id && h.address.target == AutomationTarget::bass; }),
            "AUDIT: selected ordinary swap exposes vertical amount handles on both songs without selecting its curve");
        const auto lower = std::find_if(amounts.begin(),amounts.end(),[&](const auto& h) { return h.address.clipId == clean.clips[0].id && h.address.target == AutomationTarget::bass && h.value == 0; });
        if (lower != amounts.end())
        {
            const auto baseline = json(w.engine.project); const auto undo = w.undoHistory.size();
            const auto changed = w.timeline.setCutAmount(*lower,.5f,true);
            const auto* curve = w.engine.project.curve(lower->address);
            check(changed && curve && std::abs(curve->valueAt(44,0)-juce::Decibels::gainToDecibels(.5f)) < .0001f
                && w.undoHistory.size() == undo+1,"AUDIT: vertical cut amount changes its whole flat plateau without creating a ramp");
            w.undo(false); check(json(w.engine.project) == baseline,"AUDIT: cut amount Undo restores exact nodes");
        }
        for (int width : {1250,1920})
        { w.setSize(width,width == 1250 ? 900 : 1080); w.refresh(); w.timeline.pixelsPerSecond = 40; w.timeline.updateSize(); w.viewport.setViewPosition(1300,0);
          auto out = report.getSiblingFile("audit-swap-" + juce::String(width) + ".png").createOutputStream(); juce::PNGImageFormat png;
          check(out && png.writeImageToStream(w.createComponentSnapshot(w.getLocalBounds()),*out),"AUDIT: capture selected ordinary swap handles / " + juce::String(width)); }
    }
    {
        Project pending;
        for (int i = 0; i < 3; ++i)
        {
            auto source = std::make_shared<Media>(); source->sampleRate = 8000; source->name = "Preset phase " + juce::String(i);
            source->manualBpm = i == 0 ? 120 : i == 1 ? 128 : 124; source->manualBeatOrigin = i == 1 ? 1.213 : .213;
            source->samples.setSize(1,48*8000); source->samples.clear();
            Clip clip; clip.media = source; clip.length = 48; clip.track = i; clip.pendingAutoTransition=true; clip.pendingPlacement = PendingPlacement::append; pending.clips.push_back(clip);
        }
        const auto placed = prepareTempoMap(pending,false,32);
        check(placed.error.isEmpty() && placed.project.clips.size() == 3,"fractional source phase and changing tempo place ordinary preset nodes without composition discontinuities: " + placed.error);
        if (placed.error.isEmpty())
        {
            const auto incoming = placed.project.clips[1].id;
            auto faded = placed.project;
            check(faded.transitionFor(incoming)!=nullptr,"explicit automatic placement establishes the overlapping transition before testing its tempo-map rebuild");
            const auto applied = faded.applyTransitionPreset(incoming,TransitionPreset::crossfade);
            TransitionStyle before,after; const auto beforeCapture = faded.captureTransitionStyle(incoming,before);
            const auto rebuilt = prepareTempoMap(faded,true,32,0,0,{},TransitionPreset::crossfade);
            const auto captured = rebuilt.project.captureTransitionStyle(incoming,after);
            check(applied.wasOk() && rebuilt.error.isEmpty() && captured.wasOk() && before.nodes && after.nodes
                && before.nodes->curves[0][0].points.size() == after.nodes->curves[0][0].points.size(),
                "Beatmatch rebuild replaces a Crossfade shape instead of compounding its existing nodes: " + rebuilt.error
                + " / before: " + beforeCapture.getErrorMessage() + " / after: " + captured.getErrorMessage()
                + " / counts: " + juce::String(before.nodes ? (int) before.nodes->curves[0][0].points.size() : -1)
                + "/" + juce::String(after.nodes ? (int) after.nodes->curves[0][0].points.size() : -1));
            if (beforeCapture.failed() || captured.failed())
            {
                report.getSiblingFile("rebuild-before.json").replaceWithText(json(faded));
                report.getSiblingFile("rebuild-after.json").replaceWithText(json(rebuilt.project));
            }
            if (before.nodes && after.nodes)
            {
                bool same = true;
                for (int side = 0; side < 2; ++side) for (double fraction : {.1,.25,.5,.75,.9})
                    same &= std::abs(before.nodes->curves[(size_t) side][0].valueAt(fraction,0)-after.nodes->curves[(size_t) side][0].valueAt(fraction,0)) < .003;
                check(same,"repeated Crossfade rebuild retains its measured gain shape across the whole overlap");
            }
        }
    }
    // A late source start can round just below zero when mapped back through
    // the tempo clock. A fresh fade-out must still start at unity, not silence.
    for (double start : {4255.294237201032, 4849.564063734359})
    {
        Project p; p.followTrackTempo = true; p.transitionLinksKnown = true;
        p.tempo.points = {{0,120,false},{4000.213,127.992,true},{6000.113,125.997,true}};
        Clip c; c.media = media; c.start = start; c.length = 48; c.beatmatched = true; c.tempoSourceBpm = 120;
        c.tempoMap = std::make_shared<const TempoMap>(p.tempo); c.rate = c.sourceElapsed(c.length) / c.length;
        c.fadeOut = 12; c.fadeOutAnchor = c.sourceElapsed(36); p.clips.push_back(c);
        const AutomationAddress volume{0,AutomationTarget::volume,{},{},c.id}; AutomationCurve curve;
        const auto convertedFade = editablePresetNodes(p,volume,curve); double error = 0;
        for (int n = 0; n < 2048; ++n)
        {
            const double local = 48.*n/2048.;
            const double actual = juce::Decibels::decibelsToGain((double) curve.valueAt(c.sourceElapsed(local),0),-160.);
            error = std::max(error,std::abs(actual - c.envelope().gainAt(local)));
        }
        check(convertedFade.wasOk() && curve.points.size() < 32 && error < 2.e-6,
            "late fractional source start yields a sparse accurate fade without false silence / " + juce::String(start,9)
            + " / " + convertedFade.getErrorMessage() + " / gain error " + juce::String(error,9));
    }
    {
        auto p = legacy; p.clips.resize(2); p.transitionLinks.resize(1);
        for (auto& c : p.clips) p.effectSettings(c).automation = {};
        p.transitionLinks[0].bands = {};
        p.clips[0].outroAnchor = 32.107322333828806;
        const auto applied = p.applyTransitionPreset(id,TransitionPreset::bassSwap);
        Project saved; saved.masterAutomation = p.effectSettings(p.clips[1]).automation[1];
        const auto parsed = parseProject(json(saved));
        const double edge = p.automationTime({1,AutomationTarget::bass,{},{},id},p.transitionRange(p.transitionLinks[0]).getStart());
        bool same = applied.wasOk() && parsed.error.isEmpty();
        for (double at : {0.,edge - 1.e-10,edge,edge + 1.e-10,8.,16.})
            same &= saved.masterAutomation.valueAt(at,0) == parsed.project.masterAutomation.valueAt(at,0);
        check(same,"fractional preset boundary guards stay distinct through default session JSON and retain the sharp cut: "
            + applied.getErrorMessage() + " / " + parsed.error);
    }
    const auto baseline = json(legacy); AutomationCurve preview;
    check(editablePresetNodes(legacy,address,preview).wasOk() && json(legacy) == baseline,"legacy preset node preview is read-only and keeps original playback ownership");
    check(std::any_of(preview.points.begin(),preview.points.end(),[&](const auto& p) { return p.time == cutSource && p.value == -60; }),
        "manual incoming curve still exposes the outgoing swap as an ordinary editable node");
    auto converted = legacy; check(materialiseEditablePresetNodes(converted,address).wasOk(),"first ordinary edit can materialise both related preset sides atomically");
    bool identical = true;
    for (double time = 32; time < 80; time += .001)
    {
        const auto& clip = legacy.clips[1]; const double x = legacy.automationTime(address,time);
        const auto expected = std::max(-60.f,legacy.effectSettings(clip).automation[1].valueAt(x,0) + juce::Decibels::gainToDecibels(legacy.transitionPlaybackFor(clip).gainAt(1,time),-60.f));
        identical &= std::abs(converted.curve(address)->valueAt(x,0) - expected) < .0021f;
    }
    check(identical && !converted.transitionLinks[0].bands[1] && !converted.transitionLinks[1].bands[1],
        "ordinary nodes preserve both full cuts without leaving a second active Bass generator");
    for (int block : {1,31,173,512})
    {
        auto originalEngine = std::make_unique<Engine>(juce::File(),false), nodeEngine = std::make_unique<Engine>(juce::File(),false);
        originalEngine->sampleRate = nodeEngine->sampleRate = 8000; originalEngine->project = legacy; nodeEngine->project = converted;
        originalEngine->prepareClipProcessing(); nodeEngine->prepareClipProcessing(); juce::MidiBuffer midi; bool same = true;float maximumError=0;
        for (int sample = 0; sample < 640; sample += block)
        {
            const int count = std::min(block,640-sample); juce::AudioBuffer<float> a(2,count), b(2,count);
            const double at = cut - .04 + sample/8000.;
            originalEngine->render(a,count,at,midi); nodeEngine->render(b,count,at,midi);
            for (int ch = 0; ch < 2; ++ch) for (int n = 0; n < count; ++n) {maximumError=std::max(maximumError,std::abs(a.getSample(ch,n)-b.getSample(ch,n)));same &= a.getSample(ch,n) == b.getSample(ch,n);}
        }
        check(same,"measured ordinary full-cut PCM matches original preset at an off-tick sample boundary / block " + juce::String(block)+" / max error "+juce::String(maximumError,10));
    }
    {
        auto p = legacy;
        for (auto& clip : p.clips) p.effectSettings(clip).automation = {};
        for (auto& link : p.transitionLinks) link.bands = {};
        p.transitionLinks[0].bands[0] = TransitionBandSettings{TransitionBandMode::swap,1,0,.5};
        const auto envelope = p.transitionPlaybackFor(p.clips[0]).pairs[1][0];
        p.tracks[1].mute = p.tracks[2].mute = true;
        auto ordinary = p; const auto compiled = materialisePresetNodes(ordinary,id,1);
        auto original = std::make_unique<Engine>(juce::File(),false), actual = std::make_unique<Engine>(juce::File(),false);
        original->sampleRate = actual->sampleRate = 8000; original->project = p; actual->project = ordinary;
        original->prepareClipProcessing(); actual->prepareClipProcessing(); juce::AudioBuffer<float> a(2,512),b(2,512); juce::MidiBuffer midi;
        original->render(a,512,envelope.end+.02,midi); actual->render(b,512,envelope.end+.02,midi);
        check(compiled.wasOk() && a.getMagnitude(0,512) == 0 && b.getMagnitude(0,512) == 0,
            "measured full Volume swap reaches true silence through ordinary zero-percent nodes, matching the original envelope");
    }
    {
        Workstation editor(false); auto clip = legacy.clips[0]; clip.start = 0;
        clip.introAnchor = clip.fadeInAnchor = 10; clip.outroAnchor = clip.fadeOutAnchor = 48;
        clip.controls.automation = {}; clip.controls.automation[0] = {true,{{5,-6},{20,0}}};
        editor.engine.project.clips = {clip}; editor.engine.library = {media}; editor.selectedClip = clip.id;
        editor.selectedAutomationTarget = 0; editor.setSize(1250,800); editor.refresh(); editor.timeline.pixelsPerSecond = 20; editor.timeline.updateSize();
        const AutomationAddress volume {0,AutomationTarget::volume,{},{},clip.id};
        const auto path = editor.timeline.automationPath(volume,0,48);
        const juce::Point<float> node {100,editor.timeline.automationY(0,-6,0)}; juce::Point<float> nearest;
        path.getNearestPoint(node,nearest);
        check(node.getDistanceFrom(nearest) < 1, "authored volume curve joins a node before the IN marker");
        check(std::abs(editor.timeline.automationManualValue(volume,5,-6)+6) < .0001f,
            "editing a pre-IN node keeps its chosen level instead of compensating for the silent gate");
    }
    for (auto preset : {TransitionPreset::bassSwap,TransitionPreset::crossfade})
    {
        Project p; p.transitionLinksKnown = true; p.clips = {legacy.clips[0],legacy.clips[1]}; p.effectSettings(p.clips[1]).automation[1] = {};
        Project::TransitionLink link; link.outgoing = p.clips[0].id; link.incoming = p.clips[1].id; p.transitionLinks = {link};
        const auto result = p.applyTransitionPreset(p.clips[1].id,preset);
        check(result.wasOk(),"apply preset as ordinary node starting shape / " + juce::String((int) preset) + " / " + result.getErrorMessage());
        const int target = preset == TransitionPreset::bassSwap ? 1 : 0;
        check(!p.transitionLinks[0].bands[(size_t) target] && p.effectSettings(p.clips[0]).automation[(size_t) target].active()
            && p.effectSettings(p.clips[1]).automation[(size_t) target].active() && p.clips[0].fadeOut == 0 && p.clips[1].fadeIn == 0,
            "applied preset plays through regular nodes with no separate band/fade function / " + juce::String((int) preset));
        if (preset == TransitionPreset::crossfade) for (int side = 0; side < 2; ++side)
        {
            const auto count = p.effectSettings(p.clips[(size_t) side]).automation[0].points.size();
            check(count == 2, "default full crossfade has only two endpoint nodes / side " + juce::String(side) + " / nodes " + juce::String((int) count));
        }
        if (preset == TransitionPreset::bassSwap) for (int side = 0; side < 2; ++side)
        {
            const auto& curve = p.effectSettings(p.clips[(size_t) side]).automation[1];
            check(std::any_of(curve.points.begin(),curve.points.end(),[](const auto& point) { return point.bend == 1; }),
                "newly applied full Swap retains its actual step on both sides, including a later exterior guard / " + juce::String(side));
        }
    }
    for (int band = 0; band < 4; ++band)
    {
        auto p = legacy; for (auto& c : p.clips) p.effectSettings(c).automation = {};
        for (auto& link : p.transitionLinks) link.bands = {};
        p.transitionLinks[0].bands[(size_t) band] = TransitionBandSettings{TransitionBandMode::crossfade,.75,0,.5};
        auto compiled = p; const auto result = materialisePresetNodes(compiled,id,1u << band); double error = 0; size_t most = 0;
        for (int side = 0; side < 2; ++side)
        {
            const auto& clip = p.clips[(size_t) side]; const auto playback = p.transitionPlaybackFor(clip);
            const auto& curve = compiled.effectSettings(compiled.clips[(size_t) side]).automation[(size_t) band]; most = std::max(most,curve.points.size());
            for (int n = 0; n <= 4096; ++n)
            {
                const double t = clip.start + clip.length*n/4096.;
                const float expected = juce::Decibels::gainToDecibels((float) playback.gainAt(band,t),-60.f);
                error = std::max(error,(double) std::abs(curve.valueAt(clip.sourceElapsed(t-clip.start),0)-expected));
            }
        }
        check(result.wasOk() && most <= 24 && error <= .0021,"sparse Volume/EQ crossfade retains gain accuracy / band " + juce::String(band)
            + " / nodes " + juce::String((int) most) + " / error dB " + juce::String(error,7));
    }
    for (int band = 0; band < 4; ++band) for (int mode = 0; mode < 8; ++mode)
    {
        const TransitionBandSettings setting{(TransitionBandMode) mode,.75,0,.5}; if (setting.validate(band).failed()) continue;
        auto p = legacy; for (auto& c : p.clips) p.effectSettings(c).automation = {};
        for (auto& link : p.transitionLinks) link.bands = {};
        p.effectSettings(p.clips[0]).automation[4] = {true,{{1,.2f},{4,-.2f}}};
        const auto neighbour = p.transitionLinks[1]; const auto unrelated = p.effectSettings(p.clips[0]).automation[4];
        const auto applied = placeTransitionBandNodes(p,id,band,setting);
        const auto first = json(p); const auto again = placeTransitionBandNodes(p,id,band,setting);
        bool plain = true; for (const auto& link : p.transitionLinks) for (const auto& b : link.bands) plain &= !b;
        check(applied.wasOk() && again.wasOk() && first == json(p) && plain && p.transitionLinks[1] == neighbour
            && p.effectSettings(p.clips[0]).automation[4].points.size() == unrelated.points.size(),
            "every band preset places an idempotent ordinary shape preserving other controls/neighbour / " + juce::String(band) + "/" + juce::String(mode) + " / " + applied.getErrorMessage());
    }
    // Silence uses a gain-space endpoint, including the held regions before
    // and after the authored span. Slicing/splicing must keep it valid on save.
    for (bool rising : {false, true})
    {
        const AutomationCurve curve {true, {{2, rising ? -160.f : 0.f, 0, false, true},
                                            {6, rising ? 0.f : -160.f, 0, false, true}}};
        for (const auto range : {juce::Range<double>(0,1), {0,8}, {8,10}, {3,5}})
        {
            Project session; session.masterAutomation = curve.sliced(range.getStart(),range.getEnd(),0);
            const auto parsed = parseProject(json(session)); bool same = parsed.error.isEmpty();
            for (int i = 0; i <= 16; ++i)
            {
                const double t = range.getLength()*i/16;
                same &= std::abs(session.masterAutomation.valueAt(t,0)-curve.valueAt(t+range.getStart(),0)) < .0001;
            }
            check(same,"gain-space slice retains held silence, curve shape and session validity / "
                + juce::String((int) rising) + " / " + juce::String(range.getStart()) + "-" + juce::String(range.getEnd()));
        }
        for (const auto range : {juce::Range<double>(.5,1), {8,9}, {3,5}, {0,8}})
        {
            Project session; session.masterAutomation = curve;
            const auto closed = session.closeTimeRange(range.getStart(),range.getEnd());
            bool same = closed.wasOk() && parseProject(json(session)).error.isEmpty();
            for (double t : {0., .25, 1., 2., 3., 5., 7., 10.})
                same &= std::abs(session.masterAutomation.valueAt(t,0)-curve.valueAt(t < range.getStart() ? t : t+range.getLength(),0)) < .0001;
            check(same,"closing time preserves held gain-space endpoints and saved values / "
                + juce::String((int) rising) + " / " + juce::String(range.getStart()) + "-" + juce::String(range.getEnd()));
        }
    }
    {
        Project invalid; invalid.masterAutomation = {true,{{0,-160}}};
        check(parseProject(json(invalid)).error.isNotEmpty(),"ordinary below-floor nodes still fail validation without gain-space ownership");
    }
    // Native curved spans retain their shape through ordinary editing and IO.
    for (bool exponential : {false,true})
    {
        AutomationCurve curve{true,{{0,-.8f,-.45,exponential},{16,.7f,0}}};
        auto split = curve; split.setPoint(5,curve.valueAt(5,0));
        const auto slice = curve.sliced(3,11,0); bool exact = true;
        for (int n = 0; n <= 1024; ++n)
        {
            const double t = 16. * n / 1024;
            exact &= std::abs(split.valueAt(t,0)-curve.valueAt(t,0)) < 3.e-7;
            if (t >= 3 && t <= 11) exact &= std::abs(slice.valueAt(t-3,0)-curve.valueAt(t,0)) < 3.e-7;
        }
        check(exact && split.points[0].exponential == exponential && split.points[1].exponential == exponential && slice.points[0].exponential == exponential,
            "ordinary insertion/slicing preserves native curved spans / exponential " + juce::String((int) exponential));
        Project session; session.masterAutomation = curve;
        const auto parsed = parseProject(json(session));
        check(parsed.error.isEmpty() && parsed.project.masterAutomation.points[0].exponential == exponential,
            "session36 retains interpolation kind for ordinary automation");
        session.masterAutomation = curve; const auto closed = session.closeTimeRange(4,8); bool folded = closed.wasOk();
        for (double t : {1.,3.,4.,5.,8.,12.}) folded &= std::abs(session.masterAutomation.valueAt(t,0)-curve.valueAt(t < 4 ? t : t+4,0)) < 3.e-7;
        check(folded,"closing time preserves both curved sides of the splice / exponential " + juce::String((int) exponential));
    }
    // Ordinary filter sweeps must remain practical to edit, not hundreds of
    // straight samples. Keep the existing conversion accuracy as a separate check.
    for (double sampleRate : {8000.,44100.,48000.})
    {
        auto source = std::make_shared<Media>(); source->name = "Sparse filter sweep"; source->sampleRate = sampleRate;
        source->samples.setSize(1,(int) (96 * sampleRate)); source->samples.clear(); source->manualBpm = 120; source->manualBeatOrigin = 0;
        for (double depth : {.3,.35,.65,.75}) for (int mode : {1,2})
        {
            auto p = legacy; for (auto& c : p.clips) { p.effectSettings(c).automation = {}; c.media = source; }
            for (auto& link : p.transitionLinks) link.bands = {};
            auto reference = p; reference.setTransitionFilter(id,{(TransitionFilterMode) mode,depth});
            const auto placed = placeTransitionFilterNodes(p,id,{(TransitionFilterMode) mode,depth});
            int most = 0; double error = 0;
            for (int side = 0; side < 2; ++side)
            {
                const auto& clip = p.clips[(size_t) side]; const auto& curve = p.effectSettings(clip).automation[5];
                most = std::max(most,(int) curve.points.size()); const auto playback = reference.transitionPlaybackFor(reference.clips[(size_t) side]);
                const auto& envelope = playback.filters[(size_t) (1-side)];
                for (int n = 0; n <= 4096; ++n)
                {
                    const double time = std::min(std::nextafter(clip.end(),clip.start),clip.start + clip.length*n/4096.);
                    const double amount = envelope.amountAt(time), upper = std::min(20000.,sampleRate*.45), ratio = upper/20;
                    const double hz = envelope.highPass() ? 20*std::pow(ratio,.8*amount) : upper*std::pow(ratio,-.8*amount);
                    const double expected = amount <= 0 ? 0 : (envelope.highPass() ? (hz-20)/19980 : (hz-20000)/19980) * std::min(1.,amount/.02);
                    error = std::max(error,std::abs(curve.valueAt(clip.sourceElapsed(time-clip.start),0)-expected));
                }
            }
            check(placed.wasOk() && most <= 20,"sparse Filter sweep / " + juce::String(sampleRate) + " Hz / mode " + juce::String(mode) + " / depth " + juce::String(depth)
                + " / maximum nodes " + juce::String(most));
            check(error <= .0000101,"sparse Filter retains frequency/wet-entry accuracy / maximum cutoff error " + juce::String(error,9));
            if (sampleRate == 44100 && depth == .35 && mode == 1)
            {
                TransitionStyle style; const auto captured = p.captureTransitionStyle(id,style);
                const auto file = report.getSiblingFile("sparse-filter.alloypreset"); NamedTransitionStyle loaded;
                const auto written = writeTransitionPreset(file,{"Sparse filter",style}), read = readTransitionPreset(file,loaded);
                double rounding = 0;
                if (style.nodes && loaded.style.nodes) for (int side = 0; side < 2; ++side) for (int target = 0; target < trackAutomationCount; ++target)
                {
                    const auto& a = style.nodes->curves[(size_t) side][(size_t) target].points;
                    const auto& b = loaded.style.nodes->curves[(size_t) side][(size_t) target].points;
                    for (size_t n = 0; n < std::min(a.size(),b.size()); ++n) rounding = std::max(rounding,std::abs(a[n].bend-b[n].bend));
                }
                check(captured.wasOk() && written.wasOk() && read.wasOk() && loaded.style == style && rounding < 1.e-12,
                    "portable preset4 retains sparse exponential spans at serialization precision / error " + written.getErrorMessage() + read.getErrorMessage()
                    + " / bend rounding x1e18 " + juce::String(rounding * 1.e18,3));
                auto window = std::make_unique<Workstation>(false); auto& ui = *window; ui.stopTimer(); ui.engine.project = p;
                ui.engine.library = {source}; ui.selectedClip = id; ui.selectedTransition = id; ui.engine.selectedTrack = 1; ui.selectedAutomationTarget = (int) AutomationTarget::cutoff;
                for (int width : {1250,1920})
                {
                    ui.setSize(width,1080); ui.refresh(); ui.timeline.pixelsPerSecond = 28; ui.timeline.updateSize(); ui.viewport.setViewPosition(840,0);
                    auto stream = report.getSiblingFile("sparse-filter-" + juce::String(width) + ".png").createOutputStream(); juce::PNGImageFormat png;
                    check(stream && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()),*stream),"capture sparse ordinary Filter nodes at width " + juce::String(width));
                }
            }
        }
    }
    for (int mode = 1; mode <= 2; ++mode)
    {
        auto p = legacy; for (auto& c : p.clips) p.effectSettings(c).automation = {};
        for (auto& link : p.transitionLinks) link.bands = {};
        const auto placed = placeTransitionFilterNodes(p,id,{(TransitionFilterMode) mode,.75});
        const auto first = json(p); const auto repeated = placeTransitionFilterNodes(p,id,{(TransitionFilterMode) mode,.75});
        check(placed.wasOk() && repeated.wasOk() && first == json(p) && p.effectSettings(p.clips[0]).automation[5].active()
            && p.effectSettings(p.clips[1]).automation[5].active() && !p.transitionLinks[0].filter.active(),
            "each Filter preset places regular editable cutoff nodes without a second function / " + juce::String(mode) + " / " + placed.getErrorMessage());
    }
    {
        auto p = legacy; p.effectSettings(p.clips[0]).automation[0] = {true,{{0,-12,.6},{48,-3,0}}};
        const auto original = p.effectSettings(p.clips[0]).automation[0];
        const auto placed = placeTransitionBandNodes(p,id,0,{TransitionBandMode::crossfade,.75,0,.5});
        bool exterior = placed.wasOk();
        for (double time = 0; time < 32; time += .031)
            exterior &= std::abs(p.effectSettings(p.clips[0]).automation[0].valueAt(time,0)-original.valueAt(time,0)) < .00001;
        check(exterior,"replacing a frame through a bent manual segment preserves the complete exterior curve: " + placed.getErrorMessage());
        auto disabled = legacy; disabled.effectSettings(disabled.clips[0]).automation[0] = original; disabled.effectSettings(disabled.clips[0]).automation[0].enabled = false;
        const auto before = json(disabled);
        check(placeTransitionBandNodes(disabled,id,0,{TransitionBandMode::crossfade,.75,0,.5}).failed() && json(disabled) == before,
            "a preset cannot silently enable a disabled exterior draft when replacing only its frame");
    }
    auto owned = std::make_unique<Workstation>(false); auto& ui = *owned; ui.stopTimer(); ui.setSize(1920,1080);
    ui.engine.project = legacy; ui.engine.library = {media}; ui.selectedClip = id; ui.selectedAutomationTarget = 1; ui.selectTrack(1); ui.refresh();
    ui.timeline.pixelsPerSecond = 40; ui.timeline.updateSize(); ui.viewport.setViewPosition(64 * 40,0);
    const auto view = ui.timeline.automationViewCurve(address);
    check(std::any_of(view.points.begin(),view.points.end(),[&](const auto& point) { return point.time == cutSource && point.value == -60; }),
        "actual selected Bass curve provides outgoing nodes without selecting a separate transition function");
    const auto beforeUiEdit = json(ui.engine.project); const auto history = ui.undoHistory.size();
    check(ui.insertAutomationNode(address,cut+.5,false) && !ui.engine.project.transitionLinks[1].bands[1] && ui.undoHistory.size() == history+1,
        "actual Add node materialises the cut and uses exactly one Undo");
    ui.undo(false); check(json(ui.engine.project) == beforeUiEdit,"Undo restores the complete legacy shape and both sides after ordinary node insertion");
    const auto amounts = ui.timeline.cutAmountHandles();
    const auto amount = std::find_if(amounts.begin(),amounts.end(),[&](const auto& h) { return h.address == address && h.node >= 0 && std::abs(h.time-cut) < 1.e-7 && h.value == 0; });
    check(amount != amounts.end(),"outgoing cut has an ordinary amount handle without selecting the transition frame");
    if (amount != amounts.end())
    {
        check(ui.timeline.setCutAmount(*amount,.1f,true) && !ui.engine.project.transitionLinks[1].bands[1],"ordinary cut-amount edit changes the preset node and removes its hidden generator");
        ui.undo(false); check(json(ui.engine.project) == beforeUiEdit,"Undo amount edit restores exact original arrangement");
    }
    ui.timeline.selectAutomationPoint(address,(int) (std::find_if(view.points.begin(),view.points.end(),[&](const auto& p) { return p.time == cutSource && p.value == -60; })-view.points.begin()));
    const auto mouse = juce::Desktop::getInstance().getMainMouseSource();
    const juce::Point<float> down{(float) (cut * ui.timeline.pixelsPerSecond),ui.timeline.automationY(1,-60,1)}, to = down.translated(40,0);
    auto event = [&](juce::Point<float> at,bool drag)
    { return juce::MouseEvent(mouse,at,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::altModifier),1,0,0,0,0,&ui.timeline,&ui.timeline,juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),1,drag); };
    ui.timeline.mouseDown(event(down,false)); ui.timeline.mouseDrag(event(to,true)); ui.timeline.mouseUp(event(to,true));
    check(!ui.engine.project.transitionLinks[1].bands[1] && ui.undoHistory.size() == history+1,
        "actual node drag edits the swap itself through the normal node route with one Undo");
    ui.undo(false); check(json(ui.engine.project) == beforeUiEdit,"Undo swap-node drag restores both original treatments and manual incoming nodes");
    ui.timeline.mouseDown(event(down,false));
    const auto earlier = down.translated(-40,0);
    ui.timeline.mouseDrag(event(earlier,true)); ui.timeline.mouseUp(event(earlier,true));
    const auto* earlierCurve = ui.engine.project.curve(address);
    const double earlierTime = ui.engine.project.automationTime(address,(double) earlier.x/ui.timeline.pixelsPerSecond);
    check(earlierCurve && std::any_of(earlierCurve->points.begin(),earlierCurve->points.end(),[&](const auto& point)
        { return std::abs(point.time-earlierTime) < 1.e-7 && point.time < cutSource-.999 && point.value == -60; }),
        "ordinary swap node moves a full second earlier without a redundant guard pinning its time");
    ui.undo(false); check(json(ui.engine.project) == beforeUiEdit,"Undo earlier swap drag restores exact previous nodes and timing");
    const auto cutIndex = (int) (std::find_if(view.points.begin(),view.points.end(),[&](const auto& point) { return point.time == cutSource && point.value == -60; })-view.points.begin());
    check(ui.deleteAutomationNode(address,cutIndex) && !ui.engine.project.transitionLinks[1].bands[1]
        && std::none_of(ui.engine.project.curve(address)->points.begin(),ui.engine.project.curve(address)->points.end(),[&](const auto& point) { return point.time == cutSource; }),
        "Delete removes the preset's actual swap node through the ordinary curve owner");
    ui.undo(false); check(json(ui.engine.project) == beforeUiEdit,"Undo Delete restores the original swap and manual source nodes");
    TransitionStyle captured; check(converted.captureTransitionStyle(converted.clips[1].id,captured).wasOk() && captured.nodes.has_value(),
        "saved/custom presets capture ordinary nodes instead of a separate generator description");
    if (captured.nodes)
    {
        const auto portable = report.getSiblingFile("ordinary-nodes.alloypreset"); NamedTransitionStyle loaded;
        check(writeTransitionPreset(portable,{"Ordinary nodes",captured}).wasOk() && (int) juce::JSON::parse(portable)["version"] == 7
            && readTransitionPreset(portable,loaded).wasOk() && loaded.style == captured,
            "portable node presets use format3 so an older generator-only app rejects them instead of loading an empty shape");
        TransitionStyle::Nodes decoded;
        check(readPresetNodes(presetNodesToJson(*captured.nodes),decoded).wasOk() && decoded == *captured.nodes,"ordinary preset-node template round-trips all owners, times, values and bends");
        auto invalid = presetNodesToJson(*captured.nodes); invalid.getArray()->removeLast();
        check(readPresetNodes(invalid,decoded).failed(),"malformed preset side list refuses before replacing a saved node template");
        auto mixed = captured; mixed.bands[1] = TransitionBandSettings{TransitionBandMode::swap};
        check(mixed.validate().failed(),"a saved preset cannot combine ordinary nodes with a second procedural function");
    }
    for (int width : {1250,1920})
    {
        ui.setSize(width,1080); ui.refresh(); ui.timeline.selectAutomationPoint(address,cutIndex);
        auto output = report.getSiblingFile("preset-nodes-" + juce::String(width) + ".png").createOutputStream(); juce::PNGImageFormat png;
        check(output && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()),*output),"capture ordinary preset nodes / " + juce::String(width));
    }
    media->file = report.getSiblingFile("ordinary-preset-source.wav");
    {
        juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream = media->file.createOutputStream();
        auto writer = wav.createWriterFor(stream,juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(1).withBitsPerSample(24));
        check(writer && writer->writeFromAudioSampleBuffer(media->samples,0,media->frames()),"write owned audio for ordinary-node preset Save/Open");
    }
    converted.storeTransitionAlternative(id,0);
    converted.putTransitionPreset({"Ordinary full cuts",captured});
    auto saver = std::make_unique<Engine>(juce::File(),false); saver->project = converted; saver->library = {media};
    const auto file = report.getSiblingFile("ordinary-preset.mixworks"); const auto saved = saver->saveProject(file);
    auto reopened = std::make_unique<Engine>(juce::File(),false); const auto opened = saved.isEmpty() ? reopened->restoreProject(file.loadFileAsString()) : saved;
    check(saved.isEmpty() && opened.isEmpty() && reopened->project.clips.size() == 3 && !reopened->project.transitionLinks[1].bands[1]
        && reopened->project.transitionPresets.size() == 1 && reopened->project.transitionPresets[0].style == captured,
        "production Save/Open retains ordinary cuts, alternatives and named node templates / " + opened);
    bool savedNodes = opened.isEmpty() && reopened->project.clips.size() == converted.clips.size();
    for (size_t i = 0; savedNodes && i < converted.clips.size(); ++i)
    {
        const auto& a = converted.effectSettings(converted.clips[i]).automation[1]; const auto& b = reopened->project.effectSettings(reopened->project.clips[i]).automation[1];
        savedNodes &= a.enabled == b.enabled && a.points.size() == b.points.size();
        for (size_t n = 0; savedNodes && n < a.points.size(); ++n) savedNodes &= a.points[n].time == b.points[n].time && a.points[n].value == b.points[n].value && a.points[n].bend == b.points[n].bend;
    }
    check(savedNodes,"production reopen preserves every ordinary source time/value/bend exactly without regenerating a preset");
}
}
