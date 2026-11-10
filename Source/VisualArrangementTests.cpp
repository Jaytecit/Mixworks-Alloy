#include "Workstation.h"
#include "AutoArrangement.h"
#include "MilkDrop.h"
#include <cmath>
#include <map>
#include <set>
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace mixworks
{
void runVisualArrangementTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    const auto pump = []
    {
       #if JUCE_WINDOWS
        MSG m; while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageW(&m); }
       #endif
        juce::Thread::sleep(2);
    };
    const auto source = [](const juce::String& name, bool audio)
    {
        auto m = std::make_shared<Media>(); m->name = name; m->sampleRate = 8000;
        m->analysisSource.frames = 320000; m->analysisSource.channels = 1;
        m->analysis.bpm = 120; m->analysis.tempoConfidence = 1; m->analysis.downbeatOrigin = 0;
        m->analysis.key = 0; m->analysis.keyConfidence = 1;
        if (audio)
        {
            m->samples.setSize(1,320000);
            for (int i=0;i<320000;++i) m->samples.setSample(0,i,.08f*(float)std::sin(i*juce::MathConstants<double>::twoPi*220/8000));
        }
        return m;
    };
    std::vector<VisualSceneEntry> pool {{"original/prism-bloom","Alien Cavern"},{"original/beats-in-4d","Beamers"},
        {"welcome","Welcome"},{"release/raw_video.synScene","Raw Video"}};
    check(automaticVisualScenes(pool).size()==2,"automatic collection excludes media-only/built-in presentation scenes while keeping manual availability");
    auto a=source("Alpha",false), b=source("Bravo",false), c=source("Charlie",false);
    AutoArrangementOptions options; options.startingId=a->id; options.trackCount=3; options.overlapBeats=8; options.sectionTransitions=false; options.transitions=AutoTransitionStyle::smooth;
    juce::Random random(312);
    auto plan=planAutoArrangement({a,b,c},options,random);
    check(plan.error.isEmpty() && plan.project.clips.size()==3,"real music planner supplies a three-song visual fixture");
    if (plan.project.clips.size()!=3) return;
    {
        std::vector<VisualSceneEntry> choices;
        for(int i=0;i<24;++i)
        {
            VisualSceneEntry entry {"original/variety-probe-"+juce::String(i),"Variety "+juce::String(i)};
            entry.energy=i==0 ? .4 : .95; entry.style="Prismatic";
            entry.family="family-"+juce::String(i); entry.motif=entry.family;
            choices.push_back(entry);
        }
        std::set<juce::String> openings; bool valid=true; juce::Random seedStream(8136571);
        for(int seed=1;seed<=32;++seed)
        {
            auto show=plan.project; juce::Random seeded(seedStream.nextInt64());
            valid &= arrangeVisualScenes(show,choices,seeded).wasOk();
            if(!show.visualScenes.empty()) openings.insert(show.visualScenes.front().scene);
        }
        check(valid && openings.size()>=8,"fresh random seeds explore the collection instead of always opening with the closest-energy scene: "+juce::String((int)openings.size())+" distinct openings");
        Project longShow;
        for(int i=0;i<48;++i) {Clip clip;clip.media=a;clip.start=i*40.;clip.length=40.;longShow.clips.push_back(clip);}
        juce::Random seeded(718); const auto made=arrangeVisualScenes(longShow,choices,seeded);
        std::set<juce::String> firstPass,secondPass;
        for(size_t i=0;i<longShow.visualScenes.size();++i)
            (i<24 ? firstPass : secondPass).insert(longShow.visualScenes[i].scene);
        check(made.wasOk() && longShow.visualScenes.size()==48 && firstPass.size()==24 && secondPass.size()==24,
            "a long mix uses every eligible scene before repeating, then reshuffles a complete second pass");
        auto repeat=plan.project, same=plan.project;juce::Random first(919),second(919);
        arrangeVisualScenes(repeat,choices,first);arrangeVisualScenes(same,choices,second);
        bool reproducible=repeat.visualScenes.size()==same.visualScenes.size();
        for(size_t i=0;reproducible && i<repeat.visualScenes.size();++i) reproducible &= repeat.visualScenes[i].scene==same.visualScenes[i].scene;
        check(reproducible,"an explicit seed still reproduces scene choices for diagnostics");
    }
    {
        auto collection=discoverVisualScenes(juce::File::getSpecialLocation(juce::File::currentExecutableFile).getSiblingFile("original-visualiser-runtime"));
        VisualSceneEntry imported {"milkdrop/"+juce::String::repeatedString("a",64),"MilkDrop fixture"};
        imported.family="milkdrop"; imported.style="MilkDrop"; imported.motif="milkdrop"; imported.controlSpecs["intensity"]={.5,0};
        collection.push_back(imported);
        check(automaticVisualScenes(collection).size()==205 && automaticVisualScenes(collection,VisualSceneSource::milkdrop).size()==1
            && automaticVisualScenes(collection,VisualSceneSource::both).size()==206,"source filters select Originals, MilkDrop or Both from the same catalogue");
        for(const auto selected : {VisualSceneSource::originals,VisualSceneSource::milkdrop,VisualSceneSource::both})
        {
            auto show=plan.project; show.visualImage="selected-image.png";
            const auto music=juce::JSON::toString(projectToJson(show)["clips"]);
            const auto made=arrangeVisualScenes(show,collection,random,selected);
            const auto milk=std::count_if(show.visualScenes.begin(),show.visualScenes.end(),[](const auto& s){return isMilkDropKey(s.scene);});
            check(made.wasOk() && show.visualScenes.size()==3
                && (selected==VisualSceneSource::originals ? milk==0 : selected==VisualSceneSource::milkdrop ? milk==3 : milk==1),
                visualSceneSourceName(selected)+" generates the requested engines even with an image selected");
            bool controls=true,transitions=true;
            for(size_t i=0;i<show.visualScenes.size();++i)
            {
                const auto& scene=show.visualScenes[i];
                if(isMilkDropKey(scene.scene)) controls &= scene.controls.size()==1 && scene.controls.count("intensity") && scene.automation.size()==1;
                if(i && (isMilkDropKey(scene.scene) || isMilkDropKey(show.visualScenes[i-1].scene)))
                    transitions &= scene.transition=="feedback";
            }
            check(controls && transitions && validateVisualScenes(show.visualScenes).wasOk(),visualSceneSourceName(selected)+" uses supported controls and engine handovers");
            std::vector<VisualSceneClip> reopened;
            check(readVisualScenes(visualScenesToJson(show.visualScenes),reopened).wasOk()
                && juce::JSON::toString(visualScenesToJson(reopened))==juce::JSON::toString(visualScenesToJson(show.visualScenes))
                && music==juce::JSON::toString(projectToJson(show)["clips"]),visualSceneSourceName(selected)+" round-trips its visual schedule without changing music");
        }
        auto unchanged=plan.project;
        const auto before=juce::JSON::toString(projectToJson(unchanged));
        collection.pop_back();
        check(arrangeVisualScenes(unchanged,collection,random,VisualSceneSource::milkdrop).failed()
            && arrangeVisualScenes(unchanged,collection,random,VisualSceneSource::both).failed()
            && before==juce::JSON::toString(projectToJson(unchanged)),"missing MilkDrop imports preserve the complete current arrangement for both explicit choices");
        check(arrangeVisualScenes(unchanged,{imported},random,VisualSceneSource::both).failed(),"Both refuses a collection missing original scenes");
    }
    auto before=projectToJson(plan.project); before.getDynamicObject()->removeProperty("visualScenes");
    const auto generated=arrangeVisualScenes(plan.project,pool,random);
    check(generated.wasOk() && plan.project.visualScenes.size()==3,"one editable scene is generated per arranged song");
    if (generated.failed()) return;
    auto after=projectToJson(plan.project); after.getDynamicObject()->removeProperty("visualScenes");
    check(juce::JSON::toString(before)==juce::JSON::toString(after),"matching visuals does not change music order, clocks, fades, controls or source facts");
    bool exact=true, different=true;
    for (size_t i=1;i<plan.project.clips.size();++i)
    {
        const auto* link=plan.project.transitionFor(plan.project.clips[i].id);
        if (!link) { exact=false; continue; }
        const auto range=plan.project.transitionRange(*link);
        exact &= plan.project.visualScenes[i].start==range.getStart() && plan.project.visualScenes[i-1].end()==range.getEnd();
        different &= plan.project.visualScenes[i].scene!=plan.project.visualScenes[i-1].scene;
        const auto frame=visualFrameAt(plan.project.visualScenes,(range.getStart()+range.getEnd())*.5);
        exact &= frame.outgoing && frame.incoming && std::abs(frame.mix-.5)<1.e-8;
    }
    check(exact,"generative scene phase uses the actual music transition owner's window and midpoint");
    check(different,"shuffled scene choices do not repeat on adjacent songs");
    {
        Project leadIn; Clip music; music.media=a; music.start=1.716633359549756; music.length=20;
        leadIn.clips={music};
        const auto beforeMusic=juce::JSON::toString(projectToJson(leadIn)["clips"]);
        const auto result=arrangeVisualScenes(leadIn,pool,random);
        check(result.wasOk() && leadIn.visualScenes.front().start==0
            && leadIn.visualScenes.back().end()==music.end() && visualFrameAt(leadIn.visualScenes,0).primary()
            && beforeMusic==juce::JSON::toString(projectToJson(leadIn)["clips"]),
            "grid-aligned music lead-in has visuals from Play at zero without changing music placement");
    }
    {
        auto show=plan.project; show.visualImage=report.getSiblingFile("brand.png").getFullPathName();
        auto collection=discoverVisualScenes(juce::File::getSpecialLocation(juce::File::currentExecutableFile).getSiblingFile("original-visualiser-runtime"));
        const auto made=arrangeVisualScenes(show,collection,random);
        bool curated=made.wasOk(), looksSaved=false;
        for (const auto& c : show.visualScenes)
        {
            const auto entry=std::find_if(collection.begin(),collection.end(),[&](const auto& e) { return e.key==c.scene; });
            curated &= entry!=collection.end() && entry->imageReady;
        }
        // These planner-only music fixtures intentionally have no file paths.
        // Persist their generated visual document without inventing audio references.
        Project visualDocument; visualDocument.visualScenes=show.visualScenes; visualDocument.visualImage=show.visualImage;
        auto reopened=parseProject(juce::JSON::toString(projectToJson(visualDocument),true,17));
        if (reopened.error.isEmpty()) looksSaved=juce::JSON::toString(visualScenesToJson(reopened.project.visualScenes))==juce::JSON::toString(visualScenesToJson(show.visualScenes));
        check(curated,"image-aware automatic show chooses explicitly curated image-compatible shader looks across styles");
        check(looksSaved && reopened.project.visualImage==show.visualImage,"automatic looks, transitions, pivot and shared image survive project save/reopen");
        const auto incoming=show.transitionLinks.front().incoming;
        const auto applied=show.applyTransitionPreset(incoming,TransitionPreset::bassSwap);
        const auto swap=show.transitionSwapTimes(*show.transitionFor(incoming),1);
        const auto remade=arrangeVisualScenes(show,collection,random);
        const auto& visual=show.visualScenes[1];
        const double length=show.visualScenes[0].end()-visual.start;
        check(applied.wasOk() && swap && remade.wasOk() && length>0
            && std::abs(visual.pivot-juce::jlimit(.05,.95,(((*swap)[0]+(*swap)[1])*.5-visual.start)/length))<1.e-8,
            "automatic visual midpoint follows the bass band's actual paired swap times rather than the volume band");
        show.transitionLinks.front().filter.mode=TransitionFilterMode::highToLow;
        check(arrangeVisualScenes(show,collection,random).wasOk() && show.visualScenes[1].transition=="feedback","filtered music handover retains a generative visual transition");
        show.transitionLinks.front().filter.mode=TransitionFilterMode::lowToHigh;
        check(arrangeVisualScenes(show,collection,random).wasOk() && show.visualScenes[1].transition=="feedback","opposite filter direction also retains generative visual feedback");
        auto invalidLook=show.visualScenes; invalidLook.front().controls["bad-name"]=.5;
        check(validateVisualScenes(invalidLook).failed(),"malformed generated control identity is rejected by the visual owner");
        invalidLook=show.visualScenes; invalidLook.front().transition="unknown";
        check(validateVisualScenes(invalidLook).failed(),"unknown visual transition is rejected before publication");
        check(arrangeVisualScenes(show,pool,random).failed(),"an image show refuses a collection without verified image-integrating scenes");
    }
    const auto laneBefore=juce::JSON::toString(visualScenesToJson(plan.project.visualScenes));
    check(arrangeVisualScenes(plan.project,{},random).failed() && laneBefore==juce::JSON::toString(visualScenesToJson(plan.project.visualScenes)),"missing scene collection preserves the existing visual lane");
    auto invalid=plan.project; invalid.transitionLinks[0].frame=std::array<double,2>{-2,-1};
    check(arrangeVisualScenes(invalid,pool,random).failed() && laneBefore==juce::JSON::toString(visualScenesToJson(invalid.visualScenes)),"malformed music phase fails before replacing any scenes");
    Project visualOnly; visualOnly.visualScenes=plan.project.visualScenes;
    auto parsed=parseProject(juce::JSON::toString(projectToJson(visualOnly)));
    check(parsed.error.isEmpty() && parsed.project.visualScenes.size()==3,"current format saves and reopens crossfading scene blocks");
    const auto range=plan.project.transitionRange(plan.project.transitionLinks.front());
    auto start=visualFrameAt(plan.project.visualScenes,range.getStart());
    auto end=visualFrameAt(plan.project.visualScenes,range.getEnd());
    check(start.incoming && start.mix==0 && end.primary()==&plan.project.visualScenes[1] && !end.incoming,"fade endpoints retain the outgoing picture then finish on the incoming scene");

    const auto crowded=[&](bool preroll)
    {
        Project p; p.transitionLinksKnown=true;
        Clip x,y,z; x.media=a; y.media=b; z.media=c;
        x.track=0; y.track=1; z.track=2;
        x.start=0; x.length=40; y.start=10; y.length=40; z.start=preroll ? 5 : 20; z.length=40;
        Project::TransitionLink xy,yz; xy.outgoing=x.id; xy.incoming=y.id; xy.frame=std::array<double,2>{10,35};
        yz.outgoing=y.id; yz.incoming=z.id; yz.frame=std::array<double,2>{10,35};
        p.clips={x,y,z}; p.transitionLinks={xy,yz}; return p;
    };
    for(bool preroll : {false,true})
    {
        auto p=crowded(preroll);
        check(p.validateTransitionLinks().wasOk(),"crowded handover fixture has valid music transition ownership");
        const auto music=juce::JSON::toString(projectToJson(p)["clips"]);
        const auto result=arrangeVisualScenes(p,pool,random);
        check(result.wasOk() && p.visualScenes.size()==3 && validateVisualScenes(p.visualScenes).wasOk(),
            preroll ? "incoming music preroll before its predecessor still produces an ordered visual lane"
                    : "overlapping music handover phases generate bounded two-scene blends instead of stopping Auto Arrange");
        check(music==juce::JSON::toString(projectToJson(p)["clips"]),"fitting crowded visual handovers leaves the music untouched");
        check(result.wasOk() && p.visualScenes[0].end()==27.5 && p.visualScenes[1].start==10
            && p.visualScenes[1].end()==45 && p.visualScenes[2].start==27.5,
            "crowded phases share a fitted boundary while preserving their exterior musical edges");
        bool bounded=result.wasOk();
        for(double time=0;time<p.duration() && bounded;time+=.037)
        {
            const auto frame=visualFrameAt(p.visualScenes,time);
            const auto active=std::count_if(p.visualScenes.begin(),p.visualScenes.end(),[&](const auto& s){ return time>=s.start && time<s.end(); });
            bounded &= active>=1 && active<=2 && frame.primary() && std::isfinite(frame.mix) && frame.mix>=0 && frame.mix<=1;
        }
        check(bounded,"crowded visuals cover the music continuously with no more than two active scenes");
    }
    auto reversed=crowded(true); reversed.transitionLinks.back().frame=std::array<double,2>{0,5};
    check(reversed.validateTransitionLinks().wasOk() && arrangeVisualScenes(reversed,pool,random).wasOk()
        && validateVisualScenes(reversed.visualScenes).wasOk(),"nested or backwards-in-time phase cues remain an ordered visual sequence");
    Project simultaneous;
    for(int i=0;i<3;++i)
    {
        Clip clip; clip.media=a; clip.length=.05; simultaneous.clips.push_back(clip);
    }
    check(arrangeVisualScenes(simultaneous,pool,random).wasOk() && simultaneous.visualScenes.size()==1,
        "coincident sub-frame music placements share a scene instead of aborting the music arrangement");

    auto owned=std::make_unique<Workstation>(false); auto& ui=*owned; ui.stopTimer(); ui.setSize(1920,1080);
    ui.engine.project=plan.project; ui.engine.project.visualScenes.clear(); ui.engine.library={source("One",true),source("Two",true),source("Three",true)};
    const auto musicBefore=juce::JSON::toString(projectToJson(ui.engine.project)["clips"]);
    ui.sceneLane->showBrowser();
    juce::DocumentWindow* browser = nullptr;
    juce::TextButton* match = nullptr;
    for (int i = 0; i < juce::TopLevelWindow::getNumTopLevelWindows() && !match; ++i)
        if (auto* window = dynamic_cast<juce::DocumentWindow*>(juce::TopLevelWindow::getTopLevelWindow(i)))
            if (auto* content = window->getContentComponent())
                if (auto* button = dynamic_cast<juce::TextButton*>(content->findChildWithID("visual-match-mix")))
                { browser = window; match = button; }
    check(match != nullptr,"visual browser exposes the Match mix action");
    if (match)
    {
        match->onClick();
        check(!match->isEnabled() && match->getButtonText() == "Matching visuals..."
            && ui.engine.project.visualScenes.empty(),"Match mix shows a working notification before arranging scenes");
        auto* content = browser->getContentComponent();
        auto image = report.getSiblingFile("match-mix-working.png").createOutputStream();
        check(image && juce::PNGImageFormat().writeImageToStream(content->createComponentSnapshot(content->getLocalBounds()),*image),
            "capture Match mix working notification in the visual browser");
        const auto end = juce::Time::getMillisecondCounterHiRes()+5000;
        while (!match->isEnabled() && juce::Time::getMillisecondCounterHiRes()<end) pump();
        check(match->isEnabled() && match->getButtonText() == "Match mix" && ui.status.startsWith("Visual show:"),
            "Match mix restores the button and reports the visual show when ready");
        browser->closeButtonPressed();
    }
    else ui.sceneLane->matchMix();
    check(ui.engine.project.visualScenes.size()==3 && ui.undoHistory.size()==1,"Match mix populates an existing arrangement through the real command with one Undo");
    check(musicBefore==juce::JSON::toString(projectToJson(ui.engine.project)["clips"]),"Match mix command leaves existing music intact");
    ui.undo(false); check(ui.engine.project.visualScenes.empty(),"Undo restores the visual lane before Match mix"); ui.undo(true);
    ui.showAutoArrangement();
    auto* dialog=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
    check(dialog && dialog->getComboBoxComponent("visuals") && dialog->getComboBoxComponent("visuals")->getSelectedItemIndex()==1,"Auto Arrange defaults to keeping hand-placed visuals when the lane is populated");
    if (dialog)
    {
        auto* sourceChoice=dialog->getComboBoxComponent("visualSources");
        check(sourceChoice && sourceChoice->getNumItems()==3 && sourceChoice->getSelectedId()==1 && !sourceChoice->isEnabled(),"Auto Arrange offers all sources, defaulting to Originals and disabled with Keep visual lane");
        dialog->getComboBoxComponent("visuals")->setSelectedItemIndex(0,juce::sendNotificationSync);
        check(sourceChoice && sourceChoice->isEnabled(),"creating a visual show enables the source selector");
        check(sourceChoice && sourceChoice->getBottom()+8<=dialog->getButton(0)->getY(),"all visual choices remain clear of action buttons on the actual display");
        auto output=report.getSiblingFile("auto-visuals-options.png").createOutputStream(); juce::PNGImageFormat png;
        check(output && png.writeImageToStream(dialog->createComponentSnapshot(dialog->getLocalBounds()),*output),"capture actual Auto Arrange visual choices");
        dialog->exitModalState(0); pump();
    }
    AutoArrangementOptions make; make.startingId=ui.engine.library[0]->id; make.trackCount=2; make.overlapBeats=8;
    make.sectionTransitions=false; make.transitions=AutoTransitionStyle::smooth; make.createVisualScenes=true;
    {
        const auto before=juce::JSON::toString(projectToJson(ui.engine.project));
        auto unavailable=make; unavailable.visualSceneSource=VisualSceneSource::milkdrop;
        ui.startAutoArrangement(unavailable);
        check(!ui.busy && before==juce::JSON::toString(projectToJson(ui.engine.project)) && ui.status.contains("Import a MilkDrop"),
            "native Auto Arrange refuses absent imports before preparing or replacing music");
    }
    const auto wait=[&]
    {
        const auto deadline=juce::Time::getMillisecondCounterHiRes()+30000;
        while(ui.busy && juce::Time::getMillisecondCounterHiRes()<deadline) pump();
        return !ui.busy;
    };
    // Exercise the actual dialog and its Make mix callback, rather than supplying
    // createVisualScenes directly to the worker as the checks below do.
    ui.engine.project=Project{};
    ui.autoArrangeButton.onClick();
    dialog=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
    check(dialog && dialog->getComboBoxComponent("visuals")->getSelectedItemIndex()==0,
        "right-panel Auto Arrange defaults to creating visuals for an empty visual lane");
    if (dialog)
    {
        dialog->getTextEditor("amount")->setText("2");
        dialog->getComboBoxComponent("transitions")->setSelectedItemIndex(1,juce::sendNotificationSync);
        dialog->getButton(0)->triggerClick();
        const auto deadline=juce::Time::getMillisecondCounterHiRes()+30000;
        do { pump(); } while ((juce::Component::getCurrentlyModalComponent() || ui.busy)
            && juce::Time::getMillisecondCounterHiRes()<deadline);
        check(!ui.busy && ui.engine.project.clips.size()==2 && ui.engine.project.visualScenes.size()==2,
            "pressing Make mix in the real Auto Arrange dialog publishes music and visual scenes together");
        if (ui.busy) return;
    }
    ui.startAutoArrangement(make);
    check(wait() && ui.engine.project.clips.size()==2 && ui.engine.project.visualScenes.size()==2,"production Auto Arrange prepares audio and publishes matching visual scenes together");
    if (ui.busy) return;
    const auto completed=juce::JSON::toString(projectToJson(ui.engine.project));
    const auto history=ui.undoHistory.size();
    ui.startAutoArrangement(make); if(ui.importCancellation) *ui.importCancellation=true;
    check(wait() && ui.undoHistory.size()==history && completed==juce::JSON::toString(projectToJson(ui.engine.project)),"cancelling automatic music/visual generation preserves the whole current arrangement");
    make.createVisualScenes=false;
    const auto kept=juce::JSON::toString(visualScenesToJson(ui.engine.project.visualScenes));
    ui.startAutoArrangement(make);
    check(wait() && kept==juce::JSON::toString(visualScenesToJson(ui.engine.project.visualScenes)),"Keep visual lane publishes new music without changing any existing visual block");
    make.createVisualScenes=true; make.extendExisting=true; make.trackCount=1;
    ui.startAutoArrangement(make);
    check(wait() && ui.engine.project.clips.size()==3 && ui.engine.project.visualScenes.size()==3,"extending music can explicitly regenerate matching scenes across the completed mix");
    const auto& p=ui.engine.project;
    bool joins=p.visualScenes.size()==p.clips.size();
    for(size_t i=1;i<p.clips.size() && joins;++i)
    {
        const auto* link=p.transitionFor(p.clips[i].id); if(!link) {joins=false;break;}
        const auto phase=p.transitionRange(*link);
        joins &= p.visualScenes[i].start==phase.getStart() && p.visualScenes[i-1].end()==phase.getEnd();
    }
    check(joins,"published visual blend edges still match final prepared music, including extension joins");
    ui.refresh();
    auto output=report.getSiblingFile("auto-visuals-lane.png").createOutputStream(); juce::PNGImageFormat png;
    check(output && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()),*output),"capture the arranged music and blended visual lane");
    ui.engine.project=crowded(true);
    for(size_t i=0;i<ui.engine.project.clips.size();++i) ui.engine.project.clips[i].media=ui.engine.library[i];
    ui.sceneLane->matchMix();
    check(ui.engine.project.visualScenes.size()==3 && validateVisualScenes(ui.engine.project.visualScenes).wasOk(),
        "real Match mix command fills the visual lane for crowded music with earlier incoming preroll");
    ui.timeline.pixelsPerSecond=16; ui.refresh(); ui.viewport.setViewPosition(0,0);
    auto crowdedOutput=report.getSiblingFile("crowded-visuals-lane.png").createOutputStream();
    check(crowdedOutput && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()),*crowdedOutput),
        "capture crowded music with its fitted automatic visual lane");
}
}
