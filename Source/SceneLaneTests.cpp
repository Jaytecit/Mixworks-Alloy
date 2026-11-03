#include "Workstation.h"
#include "AppWindow.h"
#include "AutoArrangement.h"
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
void runOriginalVisualTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    auto owned=std::make_unique<Workstation>(false); auto& desk=*owned;
    desk.stopTimer(); desk.setSize(1600,900);
    auto& lane=*desk.sceneLane; auto& engine=desk.engine;
    check(lane.catalogue().size()==123 && lane.catalogue()[0].key=="original/prism-bloom", "catalogue contains 123 active scenes including the preserved retained selection");
    if(lane.catalogue().empty()) return;
    check(lane.place("original/prism-bloom",0,2), "original scene places on the actual timeline with the chosen preset");
    check(!lane.place("release/alien_cavern.synScene",20), "original catalogue rejects unavailable external scenes");
    if(engine.project.visualScenes.empty()) return;
    check(engine.project.visualScenes.front().controls["bassDrive"]==.9,"new scene receives the library's chosen Percussive look");
    const auto first=engine.project.visualScenes.front();
    check(lane.change(first.id,1,12,.5), "original scene supports move trim and offset");
    desk.undo(false); check(engine.project.visualScenes.front().start==0, "original placement edits undo");
    check(std::all_of(lane.catalogue().begin(),lane.catalogue().end(),[](const auto& e){return e.presets.size()==6
        && e.controlSpecs.count("viewPattern") && e.controlSpecs.count("materialMode");}),"every scene offers six named presets and both method selectors");
    const auto& look=lane.catalogue().front().presets.back();
    check(lane.applyLook(first.id,look.controls,true,look.title)
        && engine.project.visualScenes.front().controls.at("viewPattern")==.75,"named preset changes the actual timeline controls");
    check(lane.applyLook(first.id,{{"materialMode",1.}},false,"Ribbons")
        && engine.project.visualScenes.front().controls.at("viewPattern")==.75,"image method combines with the preset view");
    const auto beforeInvalid=juce::JSON::toString(visualScenesToJson(engine.project.visualScenes));
    check(!lane.applyLook(first.id,{{"unknown",.5}},false,"Invalid")
        && !lane.applyLook(first.id,{{"bassDrive",2.}},false,"Invalid")
        && juce::JSON::toString(visualScenesToJson(engine.project.visualScenes))==beforeInvalid,"invalid look changes leave the scene intact");
    desk.undo(false);check(engine.project.visualScenes.front().controls.at("materialMode")==1./3.,"method edit Undo restores the previous preset material");
    const auto saved=report.getSiblingFile("original-visual.mixworks");
    check(desk.saveSessionFile(saved),"applied scene preset saves through the native session workflow");
    auto restored=parseProject(saved.loadFileAsString());
    check(restored.error.isEmpty() && restored.project.visualScenes.size()==1
        && restored.project.visualScenes[0].scene=="original/prism-bloom"
        && restored.project.visualScenes[0].controls==engine.project.visualScenes.front().controls, "scene identity and applied preset survive disk save and parse");
    const auto automatic=automaticVisualScenes(lane.catalogue());
    check(automatic.size()==123 && std::all_of(automatic.begin(),automatic.end(),[](const auto& entry){return entry.imageReady && !entry.controlSpecs.empty();}),
        "all original scenes are available to automatic image shows");
    // Actual automatic arrangements: long filter handovers remain direct blends,
    // and particle entries cannot dominate the show.
    std::vector<std::shared_ptr<Media>> visualMusic;
    for(int i=0;i<12;++i) {
        auto m=std::make_shared<Media>();m->name="Visual fixture "+juce::String(i);
        m->sampleRate=8000;m->analysisSource.frames=480000;m->analysisSource.channels=1;
        m->analysis.bpm=120;m->analysis.tempoConfidence=1;m->analysis.downbeatOrigin=0;
        m->analysis.key=0;m->analysis.keyConfidence=1;visualMusic.push_back(m);
    }
    AutoArrangementOptions options;options.startingId=visualMusic[0]->id;options.trackCount=12;
    options.overlapBeats=32;options.sectionTransitions=false;options.transitions=AutoTransitionStyle::smooth;
    juce::Random scheduleRandom(923);auto planned=planAutoArrangement(visualMusic,options,scheduleRandom);
    for(auto& link:planned.project.transitionLinks)link.filter.mode=TransitionFilterMode::highToLow;
    const auto scheduledVisuals=arrangeVisualScenes(planned.project,automatic,scheduleRandom);
    bool longBlends=true;int particles=0;
    for(size_t i=0;i<planned.project.visualScenes.size();++i) {
        const auto& clip=planned.project.visualScenes[i];
        auto entry=std::find_if(automatic.begin(),automatic.end(),[&](const auto& e){return e.key==clip.scene;});
        if(entry!=automatic.end() && entry->family=="sprite-particles")++particles;
        if(i && planned.project.visualScenes[i-1].end()-clip.start>4.)longBlends &= clip.transition=="dissolve";
    }
    check(planned.error.isEmpty() && scheduledVisuals.wasOk() && planned.project.visualScenes.size()==12,
        "twelve-track music fixture generates a complete visual schedule");
    check(longBlends && planned.project.visualScenes.size()>1 && planned.project.visualScenes[0].end()-planned.project.visualScenes[1].start>4.,
        "long actual music/filter overlaps use direct image blends");
    check(particles<=2,"automatic show uses at most one particle scene in five");
    std::vector<VisualSceneClip> automated;juce::Random visualRandom(919);
    for(const auto& entry:automatic)
    {
        VisualSceneClip clip;clip.scene=entry.key;clip.title=entry.title;clip.start=automated.size()*8.;clip.length=8.;
        automateVisualScene(clip,entry,.7,visualRandom);automated.push_back(std::move(clip));
    }
    std::vector<VisualSceneClip> reloaded;
    check(validateVisualScenes(automated).wasOk() && readVisualScenes(visualScenesToJson(automated),reloaded).wasOk()
        && reloaded.size()==123 && juce::JSON::toString(visualScenesToJson(reloaded))==juce::JSON::toString(visualScenesToJson(automated)),
        "all 123 saved scene choices retain bounded automation");
    VisualSceneClip curveTest;curveTest.length=10.;curveTest.automation["motion"]={false,{{0,0},{1,1}}};
    check(std::abs(visualControlsAt(curveTest,5)["motion"]-.5)<1.e-9 && std::abs(visualMotionTime(curveTest,10)-6.25)<1.e-9
        && visualMotionTime(curveTest,4)<visualMotionTime(curveTest,5),"smooth control and integrated speed remain continuous and seekable");
    curveTest.automation["mode"]={true,{{0,0},{.5,1},{1,0}}};
    check(visualControlsAt(curveTest,4.99)["mode"]==0 && visualControlsAt(curveTest,5)["mode"]==1,
        "discrete control switches exactly at its saved boundary");
    automated.front().automation["motion"].points[1].value=2.;
    check(validateVisualScenes(automated).failed(),"out-of-range saved automation is rejected");
    Project show;Clip showAudio;showAudio.length=80.;show.clips={showAudio};
    const auto arranged=arrangeVisualScenes(show,automatic,visualRandom);
    bool phraseAligned=arranged.wasOk() && !show.visualScenes.empty();
    for(const auto& visual:show.visualScenes)
        for(const auto& [name,curve]:visual.automation) if(curve.stepped)
            for(size_t p=1;p+1<curve.points.size();++p)
            {
                const double phrase=show.gridClock().beatsAt(visual.start+curve.points[p].at*visual.length)/16.;
                phraseAligned &= std::abs(phrase-std::round(phrase))<1.e-8;
            }
    check(phraseAligned && !show.visualScenes.front().automation.empty() && show.visualScenes.back().end()==80.,
        "Match mix covers the whole arrangement and aligns discrete automation to musical phrases");
    auto media=std::make_shared<Media>(); media->sampleRate=44100; media->name="Original visual authored tone";
    media->samples.setSize(2,44100*12);
    for(int ch=0;ch<2;++ch) for(int i=0;i<media->samples.getNumSamples();++i)
        media->samples.setSample(ch,i,.18f*(float)std::sin(i*juce::MathConstants<double>::twoPi*80/44100));
    Clip audio; audio.media=media; audio.length=12; engine.project.clips={audio};
    engine.sampleRate=44100; engine.position=0; engine.playing=true; engine.monitorMuted=true;
    desk.getProperties().set("visualDiagnosticCapture",true);
    lane.showBrowser();
    if(auto* mode=dynamic_cast<juce::ComboBox*>(lane.browser->getContentComponent()->findChildWithID("visual-library-view")))
        mode->setSelectedId(3,juce::sendNotificationSync);
    juce::AudioBuffer<float> output(2,512); float* channels[]{output.getWritePointer(0),output.getWritePointer(1)};
    const auto tick=[&] {
        engine.audioDeviceIOCallbackWithContext(nullptr,0,channels,2,512,{}); lane.timerCallback();
       #if JUCE_WINDOWS
        MSG message; while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
       #endif
        juce::Thread::sleep(15);
    };
    const auto until=[&](const std::function<bool()>& predicate,int ms) {
        const auto end=juce::Time::getMillisecondCounterHiRes()+ms;
        while(!predicate() && juce::Time::getMillisecondCounterHiRes()<end) tick();
        return predicate();
    };
    check(until([&]{return lane.preview.isValid() && (bool)lane.previewFacts["original"]
        && (double)lane.previewFacts["scenes"][0]["rms"]>.01;},20000), "original GPU scene receives actual mixed PCM: "+lane.previewStatus);
    if(lane.preview.isValid())
    {
        check(lane.preview.getWidth()==1280 && lane.preview.getHeight()==720, "native preview receives native 720p scene pixels");
        engine.setPlaying(false); const auto position=engine.position.load();
        for(int i=0;i<12;++i) tick();
        check(engine.position.load()==position, "pause freezes the original scene transport");
        const auto write=[&](const juce::Image& image,const juce::String& name) {
            auto stream=report.getSiblingFile(name).createOutputStream(); juce::PNGImageFormat format;
            check(stream && format.writeImageToStream(image,*stream),"capture "+name);
        };
        write(lane.preview,"original-native-scene.png");
        write(desk.createComponentSnapshot(desk.getLocalBounds()),"original-native-timeline.png");
        write(lane.browser->getContentComponent()->createComponentSnapshot(lane.browser->getContentComponent()->getLocalBounds()),"original-native-browser.png");
        engine.seek(3.25);
        check(until([&]{return lane.preview.isValid() && std::abs((double)lane.previewFacts["position"]-3.25)<.01;},5000), "original preview follows a paused seek");
        report.getSiblingFile("original-native-facts.json").replaceWithText(juce::JSON::toString(lane.previewFacts));
        for (const auto& entry : lane.catalogue())
        {
            if (entry.key!="original/beats-in-4d" && entry.key!="original/pulse-gallery" && entry.key!="original/mosaic-kiln" && entry.key!="original/firefly-assembly-colour-separation") continue;
            lane.selection=engine.project.visualScenes.front().id;
            lane.removeSelected();
            check(lane.place(entry.key,0,entry.key=="original/mosaic-kiln" ? 1 : 0),entry.title+" places on the native timeline (Glass image for Mosaic Kiln)");
            const auto sceneSaved=report.getSiblingFile(entry.key.fromFirstOccurrenceOf("/",false,false)+".mixworks");
            // The playback fixture is memory-only; persist the visual lane without
            // pretending that its authored tone has a real source-file reference.
            auto visualDocument=engine.project; visualDocument.clips.clear();
            sceneSaved.replaceWithText(juce::JSON::toString(projectToJson(visualDocument)));
            const auto parsed=parseProject(sceneSaved.loadFileAsString());
            check(parsed.error.isEmpty() && parsed.project.visualScenes.size()==1 && parsed.project.visualScenes[0].scene==entry.key,
                entry.title+" saves and parses with its own identity: "+parsed.error);
            engine.seek(0); engine.setPlaying(true);
            check(until([&]{return lane.preview.isValid() && lane.previewFacts["scenes"][0]["scene"].toString()==entry.key
                && (double)lane.previewFacts["scenes"][0]["rms"]>.01;},15000),entry.title+" renders actual mixed PCM: "+lane.previewStatus);
            engine.setPlaying(false);
            for(int i=0;i<6;++i) tick();
            write(lane.browser->getContentComponent()->createComponentSnapshot(lane.browser->getContentComponent()->getLocalBounds()),
                entry.key.fromFirstOccurrenceOf("/",false,false)+"-native-browser.png");
        }
    }
    lane.browser->setVisible(false); lane.timerCallback(); engine.playing=false;
}

}
