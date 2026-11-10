#include "Workstation.h"
#include "AppWindow.h"
#include "AutoArrangement.h"
#include "MilkDrop.h"
#include "VisualKick.h"
#include <juce_cryptography/juce_cryptography.h>
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
void runMilkDropTests(const std::function<void(bool,const juce::String&)>& check,const juce::File& report,const juce::File& preset)
{
    check(preset.existsAsFile(),"explicit local MilkDrop fixture exists");
    if(!preset.existsAsFile()) return;
    const auto library=milkDropLibraryRoot();
    juce::String key;
    const auto imported=importMilkDropPreset(preset,library,key);
    check(imported.wasOk() && isMilkDropKey(key),"native importer stores actual preset under its content identity: "+imported.getErrorMessage());
    if(imported.failed()) return;
    juce::String duplicate;
    check(importMilkDropPreset(preset,library,duplicate).wasOk() && duplicate==key && discoverMilkDropScenes(library).size()==1,
        "reimport deduplicates without replacing original content");
    check(!isMilkDropKey("milkdrop/../preset") && !isMilkDropKey("milkdrop/"+juce::String::repeatedString("g",64)),"imported scene identities reject traversal and invalid hashes");
    const auto broken=report.getSiblingFile("invalid.milk"); broken.replaceWithText("This is not a MilkDrop preset.");
    check(importMilkDropPreset(broken,library,duplicate).failed() && discoverMilkDropScenes(library).size()==1,"invalid preset import preserves the library");
    broken.deleteFile();
    const auto collection=report.getSiblingFile("linked-collection"), linkedLibrary=report.getSiblingFile("linked-library");
    const auto nested=collection.getChildFile("nested"); nested.createDirectory();
    for(int i=0;i<140;++i) nested.getChildFile(juce::String(i)+".milk").replaceWithText("[preset00]\nfDecay=.98\n// unique "+juce::String(i));
    nested.getChildFile("duplicate.milk").replaceWithText("[preset00]\nfDecay=.98\n// unique 0");
    nested.getChildFile("invalid.milk").replaceWithText("Invalid MilkDrop preset");
    const auto sourceBefore=nested.getChildFile("0.milk").loadFileAsString();
    int indexed=0,skipped=0;
    check(linkMilkDropFolder(collection,linkedLibrary,indexed,skipped).wasOk() && indexed==140 && skipped==2
        && discoverMilkDropScenes(linkedLibrary).size()==140,"recursive linked index exceeds 128 and deduplicates content while skipping invalid presets");
    check(linkedLibrary.findChildFiles(juce::File::findFiles,true,"*.milk").isEmpty()
        && nested.getChildFile("0.milk").loadFileAsString()==sourceBefore,"linking writes only metadata and leaves source presets untouched");
    nested.getChildFile("139.milk").deleteFile();
    check(linkMilkDropFolder(collection,linkedLibrary,indexed,skipped).wasOk() && discoverMilkDropScenes(linkedLibrary).size()==139,
        "rescanning the same folder replaces its index and removes deleted entries");
    check(linkMilkDropFolder(collection,linkedLibrary,indexed,skipped,[]{return true;}).failed()
        && discoverMilkDropScenes(linkedLibrary).size()==139,"cancelled indexing preserves the previous committed collection");
    // A real fixture remains external to the live library for the async UI link.
    const auto realCollection=report.getSiblingFile("real-linked-collection"); realCollection.createDirectory();
    const auto realPreset=realCollection.getChildFile(preset.getFileName());
    preset.copyFileTo(realPreset);
    realPreset.replaceWithText(realPreset.loadFileAsString()+"\n// isolated linked fixture\n");
    const auto fixtureText=realPreset.loadFileAsString();
    auto owned=std::make_unique<Workstation>(false); auto& desk=*owned;
    desk.stopTimer(); desk.setSize(1600,900);
    auto& lane=*desk.sceneLane; auto& engine=desk.engine;
    check(lane.catalogue().size()==206 && automaticVisualScenes(lane.catalogue()).size()==205,
        "import adds one optional scene while all 205 originals remain automatic");
    check(lane.importMilkDrop(preset) && lane.catalogue().size()==206,"scene-browser import path refreshes the optional library without duplicates");
    check(lane.place(key,0),"imported preset places on the native visual timeline");
    if(engine.project.visualScenes.empty()) return;
    const auto saved=report.getSiblingFile("milkdrop.mixworks");
    check(desk.saveSessionFile(saved),"imported timeline saves with native session workflow");
    const auto parsed=parseProject(saved.loadFileAsString());
    check(parsed.error.isEmpty() && parsed.project.visualScenes.size()==1 && parsed.project.visualScenes[0].scene==key,
        "imported preset identity survives disk save and parse");
    // Two separate sources are mixed by the real engine callback. No audio device
    // or production arrangement is needed, and monitoring stays muted.
    engine.project.clips.clear();
    for(int index=0;index<2;++index)
    {
        auto media=std::make_shared<Media>(); media->sampleRate=44100; media->name="MilkDrop source "+juce::String(index);
        media->analysisSource.frames=44100*20; media->analysisSource.channels=2;
        media->analysis.bpm=120; media->analysis.tempoConfidence=1; media->analysis.downbeatOrigin=0;
        media->analysis.key=0; media->analysis.keyConfidence=1;
        media->samples.setSize(2,44100*20);
        for(int ch=0;ch<2;++ch) for(int i=0;i<media->samples.getNumSamples();++i)
            media->samples.setSample(ch,i,.18f*(float)std::sin(i*juce::MathConstants<double>::twoPi*(index ? 330 : 80)/44100));
        Clip audio; audio.media=media; audio.length=20; audio.track=index; engine.project.clips.push_back(audio);
    }
    engine.sampleRate=44100; engine.position=0; engine.playing=true; engine.monitorMuted=true;
    desk.getProperties().set("visualPreviewWidth",640); desk.getProperties().set("visualDiagnosticCapture",true);
    lane.showBrowser();
    auto* libraryView=dynamic_cast<juce::ComboBox*>(lane.browser->getContentComponent()->findChildWithID("visual-library-source"));
    check(libraryView && libraryView->getSelectedId()==1,"scene browser opens in the separate Originals collection");
    if(libraryView) libraryView->setSelectedId(2,juce::sendNotificationSync);
    if(auto* search=dynamic_cast<juce::TextEditor*>(lane.browser->getContentComponent()->findChildWithID("visual-scene-search")))
        search->setText("MilkDrop",true);
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
    engine.setPlaying(false);
    if(auto* mode=dynamic_cast<juce::ComboBox*>(lane.browser->getContentComponent()->findChildWithID("visual-library-view")))
        mode->setSelectedId(1,juce::sendNotificationSync);
    check(lane.linkMilkDrop(realCollection) && until([&]{return !lane.milkDropIndexer;},10000)
        && lane.catalogue().size()==207 && desk.status.startsWith("Linked 1"),"asynchronous browser folder link publishes an external preset: "+desk.status);
    const auto linkedEntry=std::find_if(lane.catalogue().begin(),lane.catalogue().end(),[&](const auto& e){return isMilkDropKey(e.key) && e.key!=key;});
    if(linkedEntry!=lane.catalogue().end()) { key=linkedEntry->key; engine.project.visualScenes.front().scene=key; }
    const auto thumb=library.getChildFile("previews").getChildFile(key.fromFirstOccurrenceOf("/",false,false)+"-v1.jpg");
    check(until([&]{return thumb.existsAsFile();},25000) && juce::ImageFileFormat::loadFrom(thumb).isValid(),
        "MilkDrop gallery generates a real cached preview without changing transport or the visual timeline");
    check(!engine.playing && engine.project.visualScenes.size()==1 && realPreset.loadFileAsString()==fixtureText,
        "library previews preserve paused mix, placement and linked source bytes");
    { auto stream=report.getSiblingFile("milkdrop-linked-gallery.png").createOutputStream(); juce::PNGImageFormat png;
      check(stream && png.writeImageToStream(lane.browser->getContentComponent()->createComponentSnapshot(lane.browser->getContentComponent()->getLocalBounds()),*stream),"capture separate MilkDrop gallery with generated thumbnail"); }
    if(auto* mode=dynamic_cast<juce::ComboBox*>(lane.browser->getContentComponent()->findChildWithID("visual-library-view")))
        mode->setSelectedId(2,juce::sendNotificationSync);
    for(int i=0;i<4;++i) tick();
    { auto stream=report.getSiblingFile("milkdrop-linked-selected.png").createOutputStream(); juce::PNGImageFormat png;
      check(stream && png.writeImageToStream(lane.browser->getContentComponent()->createComponentSnapshot(lane.browser->getContentComponent()->getLocalBounds()),*stream),"capture selected MilkDrop preset preview"); }
    if(libraryView) libraryView->setSelectedId(1,juce::sendNotificationSync);
    { auto stream=report.getSiblingFile("milkdrop-originals-separated.png").createOutputStream(); juce::PNGImageFormat png;
      check(stream && png.writeImageToStream(lane.browser->getContentComponent()->createComponentSnapshot(lane.browser->getContentComponent()->getLocalBounds()),*stream),"capture Originals collection independently of MilkDrop"); }
    if(libraryView) libraryView->setSelectedId(2,juce::sendNotificationSync);
    if(auto* mode=dynamic_cast<juce::ComboBox*>(lane.browser->getContentComponent()->findChildWithID("visual-library-view")))
        mode->setSelectedId(3,juce::sendNotificationSync);
    engine.setPlaying(true);
    check(until([&]{return lane.preview.isValid() && lane.previewFacts["scenes"][0]["scene"].toString()==key
        && (int)lane.previewFacts["scenes"][0]["frames"]>20 && (double)lane.previewFacts["scenes"][0]["rms"]>.01;},20000),
        "projectM renders actual two-source mixed PCM: "+lane.previewStatus);
    if(lane.preview.isValid())
    {
        check(lane.previewFacts["scenes"][0]["renderer"].toString()=="projectM-4.1.8","native renderer reports real projectM delivery");
        engine.setPlaying(false); for(int i=0;i<12;++i) tick();
        const auto frozen=lane.preview;
        const auto sequence=(juce::int64)lane.previewFacts["sequence"];
        for(int i=0;i<12;++i) tick();
        check((juce::int64)lane.previewFacts["sequence"]==sequence,"paused imported scene stops rendering");
        auto stream=report.getSiblingFile("milkdrop-native-scene.png").createOutputStream(); juce::PNGImageFormat format;
        check(stream && format.writeImageToStream(frozen,*stream),"capture actual imported native scene pixels");
        stream.reset(); stream=report.getSiblingFile("milkdrop-native-browser.png").createOutputStream();
        check(stream && format.writeImageToStream(lane.browser->getContentComponent()->createComponentSnapshot(lane.browser->getContentComponent()->getLocalBounds()),*stream),"capture import control and native scene browser");
        engine.seek(3.25);
        check(until([&]{return lane.preview.isValid() && std::abs((double)lane.previewFacts["position"]-3.25)<.01;},5000),"paused seek restarts imported feedback at the new transport position");
        report.getSiblingFile("milkdrop-native-facts.json").replaceWithText(juce::JSON::toString(lane.previewFacts));
        lane.selection=engine.project.visualScenes.front().id; lane.removeSelected();
        check(lane.place("original/prism-bloom",0),"original scene still places after imported rendering");
        engine.seek(0); engine.setPlaying(true);
        check(until([&]{return lane.preview.isValid() && lane.previewFacts["scenes"][0]["scene"].toString()=="original/prism-bloom";},10000),
            "original GPU renderer still works after projectM without a playback restart");
    }
    engine.setPlaying(false); lane.browser->setVisible(false); lane.timerCallback();
    engine.library.clear(); for(const auto& clip:engine.project.clips) engine.library.push_back(clip.media);
    engine.project=Project{}; desk.refresh();
    desk.showAutoArrangement();
    auto* dialog=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
    check(dialog && dialog->getComboBoxComponent("visualSources"),"real Auto Arrange exposes the scene-source selector with imports installed");
    if(dialog)
    {
        dialog->getTextEditor("amount")->setText("2");
        dialog->getComboBoxComponent("transitions")->setSelectedItemIndex(1,juce::sendNotificationSync);
        dialog->getComboBoxComponent("visualSources")->setSelectedId(3,juce::sendNotificationSync);
        auto stream=report.getSiblingFile("both-auto-options.png").createOutputStream(); juce::PNGImageFormat png;
        check(stream && png.writeImageToStream(dialog->createComponentSnapshot(dialog->getLocalBounds()),*stream),"capture actual Auto Arrange with Both selected");
        dialog->getButton(0)->triggerClick();
        const auto end=juce::Time::getMillisecondCounterHiRes()+60000;
        do { tick(); } while((desk.busy || juce::Component::getCurrentlyModalComponent()) && juce::Time::getMillisecondCounterHiRes()<end);
        const auto importedCount=std::count_if(engine.project.visualScenes.begin(),engine.project.visualScenes.end(),[](const auto& s){return isMilkDropKey(s.scene);});
        check(!desk.busy && engine.project.clips.size()==2 && engine.project.visualScenes.size()==2 && importedCount==1,
            "real Make mix callback builds one original and one imported scene using Both: "+desk.status);
        if(!desk.busy && engine.project.visualScenes.size()==2)
        {
            desk.refresh(); auto picture=report.getSiblingFile("both-native-timeline.png").createOutputStream();
            check(picture && png.writeImageToStream(desk.createComponentSnapshot(desk.getLocalBounds()),*picture),"capture automatically arranged music and mixed-engine visual timeline");
            const auto arranged=engine.project;
            desk.undo(false); check(engine.project.visualScenes.empty(),"Undo restores the arrangement before automatic mixed-engine publication");
            desk.undo(true); check(engine.project.visualScenes.size()==2,"Redo restores the automatically generated mixed-engine visual lane");
            engine.project.visualScenes.clear();
            desk.getProperties().set("visualSceneSource",2); lane.matchMix();
            check(engine.project.visualScenes.size()==2 && std::all_of(engine.project.visualScenes.begin(),engine.project.visualScenes.end(),[](const auto& s){return isMilkDropKey(s.scene);}),
                "visual Match mix uses MilkDrop-only selection on the existing music");
            engine.project.visualScenes.clear();
            desk.getProperties().set("visualSceneSource",3); lane.matchMix();
            check(engine.project.visualScenes.size()==2 && std::count_if(engine.project.visualScenes.begin(),engine.project.visualScenes.end(),[](const auto& s){return isMilkDropKey(s.scene);})==1,
                "visual Match mix uses Both on the same existing music");
            lane.showBrowser();
            auto* sources=dynamic_cast<juce::ComboBox*>(lane.browser->getContentComponent()->findChildWithID("visual-scene-sources"));
            lane.timerCallback();
            check(sources && sources->getSelectedId()==3,"visual browser shares the current Both selection with Auto Arrange");
            auto view=report.getSiblingFile("both-native-browser.png").createOutputStream();
            check(view && png.writeImageToStream(lane.browser->getContentComponent()->createComponentSnapshot(lane.browser->getContentComponent()->getLocalBounds()),*view),"capture shared scene-source choice in the visual browser");
            lane.browser->setVisible(false); lane.timerCallback();
            desk.getProperties().set("visualSceneSource",1);
        }
    }
    // Use one external preset/JPG directly. Do not rescan a large installed
    // collection as a side effect of routine tests, especially during generation.
    engine.setPlaying(false);
    lane.showBrowser();
    if(libraryView) libraryView->setSelectedId(2,juce::sendNotificationSync);
    if(auto* mode=dynamic_cast<juce::ComboBox*>(lane.browser->getContentComponent()->findChildWithID("visual-library-view")))
        mode->setSelectedId(1,juce::sendNotificationSync);
    const auto external=preset.getSiblingFile("!!!---flexi + amandio c - organic.milk");
    const auto externalImage=external.withFileExtension(".jpg");
    if(external.existsAsFile() && externalImage.existsAsFile())
    {
        auto* entry=new juce::DynamicObject(); entry->setProperty("sha256",juce::SHA256(external).toHexString());
        entry->setProperty("source",external.getFullPathName()); entry->setProperty("title",external.getFileNameWithoutExtension());
        entry->setProperty("preview",externalImage.getFullPathName());
        auto* manifest=new juce::DynamicObject(); manifest->setProperty("schema",2);
        manifest->setProperty("root",external.getParentDirectory().getFullPathName());
        manifest->setProperty("entries",juce::Array<juce::var>{juce::var(entry)});
        manifest->setProperty("texturePaths",juce::Array<juce::var>{external.getParentDirectory().getFullPathName()});
        library.getChildFile("linked/external-jpg-fixture.json").replaceWithText(juce::JSON::toString(juce::var(manifest)));
        lane.scenes.erase(std::remove_if(lane.scenes.begin(),lane.scenes.end(),[](const auto& e){return isMilkDropKey(e.key);}),lane.scenes.end());
        const auto entries=discoverMilkDropScenes(library); lane.scenes.insert(lane.scenes.end(),entries.begin(),entries.end());
    }
    const auto existingImage=std::find_if(lane.catalogue().begin(),lane.catalogue().end(),[](const auto& e){return isMilkDropKey(e.key) && e.referenceImage.isNotEmpty();});
    check(existingImage!=lane.catalogue().end() && juce::File(existingImage->referenceImage).existsAsFile(),"linked collection exposes existing JPG preview paths");
    if(existingImage!=lane.catalogue().end())
    {
        if(auto* search=dynamic_cast<juce::TextEditor*>(lane.browser->getContentComponent()->findChildWithID("visual-scene-search")))
            search->setText(existingImage->title,true);
        for(int i=0;i<4;++i) tick();
        auto stream=report.getSiblingFile("milkdrop-existing-jpg-gallery.png").createOutputStream(); juce::PNGImageFormat png;
        check(stream && png.writeImageToStream(lane.browser->getContentComponent()->createComponentSnapshot(lane.browser->getContentComponent()->getLocalBounds()),*stream),"capture actual collection using its existing JPG images");
        stream.reset();
        if(auto* mode=dynamic_cast<juce::ComboBox*>(lane.browser->getContentComponent()->findChildWithID("visual-library-view")))
            mode->setSelectedId(2,juce::sendNotificationSync);
        stream=report.getSiblingFile("milkdrop-existing-jpg-selected.png").createOutputStream();
        check(stream && png.writeImageToStream(lane.browser->getContentComponent()->createComponentSnapshot(lane.browser->getContentComponent()->getLocalBounds()),*stream),"capture selected preset with its original JPG");
        check(!library.getChildFile("previews").getChildFile(existingImage->key.fromFirstOccurrenceOf("/",false,false)+"-v1.jpg").existsAsFile(),"existing JPG preview is read directly without generating or copying an image");
    }
    lane.browser->setVisible(false); lane.timerCallback();
}

static double fixtureKick(double age, double amplitude, double start = 180., double end = 53.)
{
    if (age < 0. || age > .36) return 0.;
    const double phase = end * age + (start - end) * .015 * (1. - std::exp(-age / .015));
    return amplitude * std::min(age / .0025, 1.) * std::exp(-age / .061)
        * std::sin(juce::MathConstants<double>::twoPi * phase);
}

void runVisualKickTests(const std::function<void(bool, const juce::String&)>& check)
{
    constexpr double rate = 48000.;
    constexpr double turn = 6.283185307179586;
    const auto pluck = [&](double age, double hz, double amplitude, double harmonic = .36, double decay = .10)
    {
        if (age < 0. || age > .4) return 0.;
        const double phase = turn * hz * age;
        return amplitude * std::min(age / .0025, 1.) * std::exp(-age / decay)
            * (std::sin(phase) + harmonic * std::exp(-age / .029) * std::sin(2. * phase));
    };
    const std::array<double,3> labels {.5,.877,1.5}; // Missing kick at 1 s; the middle attack is off-grid.
    const auto mix = [&](double time, bool bass)
    {
        return fixtureKick(time-labels[0], .66) + fixtureKick(time-labels[1], .42) + fixtureKick(time-labels[2], .60, 148., 49.)
            + (bass ? .15 * std::sin(turn * 58. * time) : 0.);
    };
    const auto sample = [&](const auto& signal, int drainEvery)
    {
        SpectrumFeed feed {true};
        feed.enabled = true;
        VisualKickDetector detector;
        juce::AudioBuffer<float> audio(2, 240);
        std::array<float,SpectrumFeed::capacity> left {}, right {};
        std::array<double,SpectrumFeed::capacity> rates {};
        std::array<SpectrumFeed::Clock,SpectrumFeed::capacity> clocks {};
        std::vector<double> events;
        const auto drain = [&]
        {
            const auto count = feed.read(left.data(),right.data(),rates.data(),feed.capacity,clocks.data());
            for (int i=0;i<count;++i)
            {
                detector.push(left[i],right[i],clocks[i].position,rates[i],clocks[i].revision);
                const auto event = detector.events().back();
                if (event >= 0. && (events.empty() || event != events.back())) events.push_back(event);
            }
        };
        for (int block=0;block<400;++block)
        {
            const SpectrumFeed::Clock clock {block * audio.getNumSamples() / rate, 4};
            for (int i=0;i<audio.getNumSamples();++i)
            {
                const auto value = (float)signal(clock.position+i/rate);
                audio.setSample(0,i,value);
                audio.setSample(1,i,-value); // Stereo energy also works through polarity differences.
            }
            feed.push(audio,audio.getNumSamples(),rate,&clock,audio.getNumSamples());
            if ((block+1)%drainEvery==0) drain();
        }
        drain();
        return events;
    };
    const auto matches = [&](const std::vector<double>& events)
    {
        return events.size()==labels.size()
            && std::equal(events.begin(),events.end(),labels.begin(),[](double event, double label){return std::abs(event-label)<.016;});
    };
    const auto isolated = sample([&](double time){return mix(time,false);},3);
    const auto combined = sample([&](double time){return mix(time,true);},3);
    check(matches(isolated),"one event per kick including syncopation and a missing grid kick");
    check(matches(combined),"one event per kick underneath sustained bass");
    check(combined==sample([&](double time){return mix(time,true);},19),
          "timestamped detector is invariant to 15 ms and 95 ms worker drains");
    check(sample([](double){return 0.;},3).empty(),"silence never confirms a kick");
    check(sample([&](double t){return t<.24 ? 0. : .25*std::sin(turn*65.*t);},3).empty(),
          "a sustained bass note turning on never confirms a kick");
    check(sample([&](double t){return pluck(t-.25,65.,.68)+pluck(t-.877,52.,.62)+pluck(t-1.42,89.,.59);},3).empty(),
          "harmonic bass plucks never confirm a kick");
    check(sample([&](double t)
    {
        return .14*std::sin(turn*58.*t) + pluck(t-.23,65.,.53,.30) + pluck(t-.62,88.,.48,.36,.11)
            + pluck(t-.44,220.,.46,.45,.038);
    },3).empty(),"plucks and percussion over a bass bed leave the kick path idle");
    check(sample([&](double t)
    {
        const double age=t-1.42;
        const double hats=age<0. ? 0. : .25*std::exp(-age/.020)*(std::sin(turn*4110.*age)+std::sin(turn*6087.*age));
        return pluck(t-.25,220.,.56)+pluck(t-.877,182.,.65)+hats;
    },3).empty(),"snare and hat fixtures never confirm a kick");

    VisualKickDetector detector;
    for (int i=0;i<38400;++i)
    {
        const double time=i/rate;
        const float value=(float)fixtureKick(time-.5,.7);
        detector.push(value,value,time,rate,4);
    }
    check(detector.events().back()>=0.,"fixture reaches a confirmed event before discontinuity");
    detector.push(.3f,.3f,.5,rate,4);
    check(detector.events().back()<0.,"backward sample clock clears previous loop events");
    for (int i=0;i<5760;++i)
    {
        const double time=i/rate;
        const float value=(float)fixtureKick(time,.8);
        detector.push(value,value,3.+time,rate,5);
    }
    check(detector.revision()==5 && detector.events().back()<0.,"seek revision primes a hot landing without replaying a kick");
    detector.push(0.f,0.f,-1.,rate,5);
    check(detector.events().back()<0.,"untimed monitor samples invalidate programme kick state");

    SpectrumFeed feed {true};
    feed.enabled=true;
    juce::AudioBuffer<float> buffer(2,4);
    buffer.clear();
    const SpectrumFeed::Clock start {7.,8};
    feed.push(buffer,4,rate,&start,2);
    std::array<float,4> l {}, r {};
    std::array<double,4> rates {};
    std::array<SpectrumFeed::Clock,4> times {};
    check(feed.read(l.data(),r.data(),rates.data(),4,times.data())==4
        && times[0].position==7. && times[0].revision==8 && times[1].position==7.+1./rate
        && times[2].position<0. && times[3].position<0.,
        "partial programme block stamps only the audible transport frames");
}

void runOriginalVisualTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    if (juce::JUCEApplicationBase::getCommandLineParameters().contains("--visual-kick-only"))
    {
        auto owned=std::make_unique<Workstation>(false);
        auto& desk=*owned; auto& engine=desk.engine; auto& lane=*desk.sceneLane;
        desk.stopTimer(); desk.setSize(1440,900);
        desk.getProperties().set("visualDiagnosticCapture",true);
        desk.getProperties().set("visualPreviewWidth",640);
        auto media=std::make_shared<Media>(); media->sampleRate=48000; media->name="Visual kick fixture";
        media->samples.setSize(2,3*48000);
        constexpr double attack=1.377;
        for (int i=0;i<media->samples.getNumSamples();++i)
        {
            const double time=i/media->sampleRate;
            const float value=(float)(.15*std::sin(juce::MathConstants<double>::twoPi*58.*time)
                                      +fixtureKick(time-attack,.72));
            for (int ch=0;ch<2;++ch) media->samples.setSample(ch,i,value);
        }
        Clip audio; audio.media=media; audio.length=3.; engine.project.clips={audio};
        engine.sampleRate=48000; engine.monitorMuted=true;
        check(lane.place("original/opal-reactor",0,0),"place Reactor on the actual visual lane");
        lane.showBrowser();
        if (auto* mode=dynamic_cast<juce::ComboBox*>(lane.browser->getContentComponent()->findChildWithID("visual-library-view")))
            mode->setSelectedId(3,juce::sendNotificationSync);
        juce::AudioBuffer<float> output(2,512);
        float* channels[]{output.getWritePointer(0),output.getWritePointer(1)};
        const auto tick=[&]
        {
            engine.audioDeviceIOCallbackWithContext(nullptr,0,channels,2,512,{});
            lane.timerCallback();
           #if JUCE_WINDOWS
            MSG message; while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
           #endif
            juce::Thread::sleep(15);
        };
        const auto until=[&](const auto& predicate,int ms)
        {
            const auto deadline=juce::Time::getMillisecondCounterHiRes()+ms;
            while(!predicate() && juce::Time::getMillisecondCounterHiRes()<deadline) tick();
            return predicate();
        };
        if (until([&]{return lane.preview.isValid();},15000))
        {
            const auto capture=[&](const juce::String& name)
            {
                auto stream=report.getSiblingFile(name+".png").createOutputStream();
                return stream && juce::PNGImageFormat().writeImageToStream(lane.preview,*stream);
            };
            engine.playing=true;
            double bassImpact=0.;
            const bool bassDelivered=until([&]
            {
                bassImpact=std::max(bassImpact,(double)lane.previewFacts["kickImpact"]);
                return (double)lane.previewFacts["position"]>.85;
            },3500);
            check(bassDelivered && bassImpact==0. && (double)lane.previewFacts["scenes"][0]["rms"]>.02
                && capture("bass-only"),"native programme bass reaches Reactor without a kick impact");
            const bool delivered=until([&]{return (double)lane.previewFacts["kickImpact"]>.65;},2600);
            const double time=(double)lane.previewFacts["position"];
            check(delivered && time>attack && time<attack+.16 && capture("confirmed-kick"),
                  "native audio clock delivers a large response to the syncopated kick at "+juce::String(time,3));
            engine.setPlaying(false); engine.seek(.5);
            check(until([&]{return std::abs((double)lane.previewFacts["position"]-.5)<1.e-6
                && (double)lane.previewFacts["kickImpact"]==0.;},5000),"native seek clears the previous kick pose");
        }
        else check(false,"native visual renderer startup: "+lane.previewStatus);
        engine.playing=false; lane.browser->setVisible(false); lane.timerCallback();
        return;
    }
    if(juce::JUCEApplicationBase::getCommandLineParameters().contains("--scene-replacement-only"))
    {
        Workstation desk(false); desk.stopTimer(); desk.setSize(1440,940); auto& lane=*desk.sceneLane;
        check(lane.place("original/prism-bloom",0,0),"place actual original scene before in-place replacement");
        if(desk.engine.project.visualScenes.empty())return;
        auto original=desk.engine.project.visualScenes[0]; original.start=1; original.length=12; original.offset=3;
        original.controls["bassDrive"]=.3; original.automation["bassDrive"]={false,{{0,.1},{1,.9}}}; desk.engine.project.visualScenes[0]=original;
        lane.selection=original.id; desk.engine.position=6.25; desk.engine.playing=true;
        const auto imagePath=report.getSiblingFile("replacement-shared-image.png");
        {juce::Image pixels(juce::Image::ARGB,8,8,true); pixels.clear(pixels.getBounds(),juce::Colours::blue); auto stream=imagePath.createOutputStream();
         check(stream && juce::PNGImageFormat().writeImageToStream(pixels,*stream),"write one owned shared image for replacement preservation");}
        desk.engine.project.visualImage=imagePath.getFullPathName();
        const auto history=desk.undoHistory.size();
        check(lane.replaceScene(original.id,"original/opal-reactor",0),"replace currently active scene while programme transport runs");
        const auto changed=desk.engine.project.visualScenes[0];
        check(changed.id==original.id && changed.start==original.start && changed.length==original.length && changed.scene=="original/opal-reactor"
            && changed.offset==0 && changed.automation.empty() && desk.engine.project.visualImage==imagePath.getFullPathName(),"replacement keeps placement/identity and resets scene-specific offset and animation");
        check(desk.engine.playing && desk.engine.position==6.25 && desk.undoHistory.size()==history+1,"scene replacement preserves programme transport with one Undo");
        desk.undo(false); check(desk.engine.project.visualScenes[0].scene==original.scene && desk.engine.project.visualScenes[0].offset==3,"Undo restores original scene and its local settings"); desk.undo(true);
        desk.engine.playing=false; const auto file=report.getSiblingFile("scene-replacement.mixworks");
        const auto saved=desk.engine.saveProject(file); Engine reopened; const auto opened=saved.isEmpty()?reopened.restoreProject(file.loadFileAsString()):saved;
        check(saved.isEmpty() && opened.isEmpty() && reopened.project.visualScenes[0].scene==changed.scene && reopened.project.visualScenes[0].length==12,"actual save/reopen retains the scene replacement: "+opened);
        const auto before=juce::JSON::toString(projectToJson(desk.engine.project));
        check(!lane.replaceScene("removed-scene",changed.scene,0) && !lane.replaceScene(original.id,"missing/scene",0)
            && juce::JSON::toString(projectToJson(desk.engine.project))==before,"removed target and unavailable replacement are failure-atomic");
        const auto fixture=report.getSiblingFile("replacement-fixture.milk"); check(fixture.replaceWithText("[preset00]\nfDecay=.98\nfGammaAdj=1.5\n"),"write one owned MilkDrop fixture without any collection scan");
        check(lane.importMilkDrop(fixture),"import one explicit MilkDrop replacement fixture");
        const auto entry=std::find_if(lane.catalogue().begin(),lane.catalogue().end(),[](const auto& e){return isMilkDropKey(e.key);});
        if(entry!=lane.catalogue().end())
        { check(lane.replaceScene(original.id,entry->key,-1) && desk.engine.project.visualScenes[0].start==1,"Original to MilkDrop replacement keeps timing");
          check(lane.replaceScene(original.id,"original/prism-bloom",1),"MilkDrop to Original replacement accepts a chosen original look"); }
        lane.selection=original.id; lane.showBrowser(true);
        check(lane.browser && !lane.browser->isUsingNativeTitleBar(),"replacement control panel uses app-owned dark window chrome");
        auto* browser=lane.browser->getContentComponent();
        auto* view=dynamic_cast<juce::ComboBox*>(browser->findChildWithID("visual-library-view"));
        auto* list=dynamic_cast<juce::ListBox*>(browser->findChildWithID("visual-scene-list"));
        auto* look=dynamic_cast<juce::ComboBox*>(browser->findChildWithID("visual-scene-preset"));
        check(view && view->getSelectedId()==3,"replacement opens the actual live mix rather than a reference image");
        if(list && look && view)
        {
            const juce::MouseEvent click(juce::Desktop::getInstance().getMainMouseSource(),{},juce::ModifierKeys::leftButtonModifier,
                0,0,0,0,0,list,list,juce::Time(),{},juce::Time(),1,false);
            list->selectRow(1); list->getModel()->listBoxItemClicked(1,click);
            look->setSelectedId(2,juce::sendNotificationSync);
            check(view->getSelectedId()==3,"choosing a replacement and look preserves the live renderer route");
            lane.showBrowser(false); list->getModel()->listBoxItemClicked(1,click);
            check(view->getSelectedId()==2,"ordinary add browsing still offers the selected reference preview");
            lane.selection=original.id; lane.showBrowser(true);
        }
        auto image=report.getSiblingFile("replace-scene-panel.png").createOutputStream();
        check(lane.browser && image && juce::PNGImageFormat().writeImageToStream(lane.browser->createComponentSnapshot(lane.browser->getLocalBounds()),*image),"capture actual replacement panel and chrome");
        if(lane.browser)lane.browser->setVisible(false); return;
    }
    auto owned=std::make_unique<Workstation>(false); auto& desk=*owned;
    desk.stopTimer(); desk.setSize(1600,900);
    auto& lane=*desk.sceneLane; auto& engine=desk.engine;
    check(lane.catalogue().size()==205 && lane.catalogue()[0].key=="original/prism-bloom", "catalogue contains 205 active scenes including Inkstone");
    if(lane.catalogue().empty()) return;
    check(lane.place("original/prism-bloom",0,2), "original scene places on the actual timeline with the chosen preset");
    check(!lane.place("release/alien_cavern.synScene",20), "original catalogue rejects unavailable external scenes");
    if(engine.project.visualScenes.empty()) return;
    check(engine.project.visualScenes.front().controls["bassDrive"]==.9,"new scene receives the library's chosen Percussive look");
    const auto first=engine.project.visualScenes.front();
    check(lane.change(first.id,1,12,.5), "original scene supports move trim and offset");
    desk.undo(false); check(engine.project.visualScenes.front().start==0, "original placement edits undo");
    check(std::all_of(lane.catalogue().begin(),lane.catalogue().end(),[](const auto& e){
        if(e.key=="original/jumping-techno-tunnel") return e.presets.size()==1 && !e.imageReady;
        if(e.key=="original/inkstone")
            return e.imageReady && e.presets.size()==3
                && e.presets[0].controls==std::map<juce::String,double>{{"feedback",0.}}
                && e.presets[1].controls==std::map<juce::String,double>{{"feedback",.85}}
                && e.presets[2].controls==std::map<juce::String,double>{{"feedback",.85},{"pulse",0.}};
        return e.presets.size()==(e.controlSpecs.count("reliefDepth")?11:6) && e.controlSpecs.count("viewPattern") && e.controlSpecs.count("materialMode");
    }),"image scenes offer their authored looks; supported surfaces add five independent material looks and the tunnel retains its native look");
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
    const auto beforePalette=engine.project.visualScenes.front().controls;
    check(lane.applyLook(first.id,{{"paletteStyle",2./7.},{"paletteAmount",1.}},false,"Acid lime")
        && engine.project.visualScenes.front().controls.at("paletteStyle")==2./7.
        && engine.project.visualScenes.front().controls.at("viewPattern")==beforePalette.at("viewPattern")
        && engine.project.visualScenes.front().controls.at("materialMode")==beforePalette.at("materialMode"),
        "independent palette selection retains the scene's view and image methods");
    desk.undo(false);check(engine.project.visualScenes.front().controls==beforePalette,"palette selection supports exact Undo");
    lane.applyLook(first.id,{{"paletteStyle",1./7.},{"paletteAmount",1.}},false,"Electric neon");
    const auto saved=report.getSiblingFile("original-visual.mixworks");
    check(desk.saveSessionFile(saved),"applied scene preset saves through the native session workflow");
    auto restored=parseProject(saved.loadFileAsString());
    check(restored.error.isEmpty() && restored.project.visualScenes.size()==1
        && restored.project.visualScenes[0].scene=="original/prism-bloom"
        && restored.project.visualScenes[0].controls==engine.project.visualScenes.front().controls, "scene identity and applied preset survive disk save and parse");
    const auto automatic=automaticVisualScenes(lane.catalogue());
    check(automatic.size()==205 && std::all_of(automatic.begin(),automatic.end(),[](const auto& entry){
        return !entry.controlSpecs.empty() && (entry.imageReady || entry.key=="original/jumping-techno-tunnel");
    }),"all original scenes, including the procedural tunnel, are available to automatic shows");
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
    bool longBlends=true, familyVariety=true, knownControls=true;int particles=0;
    juce::String lastFamily;
    for(size_t i=0;i<planned.project.visualScenes.size();++i) {
        const auto& clip=planned.project.visualScenes[i];
        auto entry=std::find_if(automatic.begin(),automatic.end(),[&](const auto& e){return e.key==clip.scene;});
        if(entry!=automatic.end() && entry->family=="sprite-particles")++particles;
        if(entry!=automatic.end()) { if(i && entry->family==lastFamily) familyVariety=false; lastFamily=entry->family; }
        if(entry!=automatic.end()) for(const auto& [name,value]:clip.controls) { juce::ignoreUnused(value); knownControls &= entry->controlSpecs.count(name)!=0; }
        if(i && planned.project.visualScenes[i-1].end()-clip.start>4.)longBlends &= clip.transition=="feedback";
    }
    check(planned.error.isEmpty() && scheduledVisuals.wasOk() && planned.project.visualScenes.size()==12,
        "twelve-track music fixture generates a complete visual schedule");
    check(longBlends && planned.project.visualScenes.size()>1 && planned.project.visualScenes[0].end()-planned.project.visualScenes[1].start>4.,
        "long actual music/filter overlaps retain generative visual feedback");
    check(particles<=2,"automatic show uses at most one particle scene in five");
    check(familyVariety,"automatic show spaces neighbouring visual families when alternatives exist");
    check(knownControls,"automatic image shows send only controls declared by the selected generator");
    {
        int materials=0;
        for(const auto& entry:automatic)
        {
            if(!entry.controlSpecs.count("reliefDepth"))continue;
            ++materials;
            check(entry.controlSpecs.count("meltWarp") && entry.controlSpecs.count("displacementColour") && entry.presets.size()==11,
                entry.title+" exposes independent relief, warp and colour controls and named looks");
            VisualSceneClip generated;juce::Random methodRandom(924);automateVisualScene(generated,entry,.7,methodRandom);
            check(!generated.automation.count("reliefDepth") && !generated.automation.count("meltWarp") && !generated.automation.count("displacementColour")
                && generated.controls["meltWarp"]==0 && generated.controls["displacementColour"]==0
                && (entry.key=="original/molten-contours" || generated.controls["reliefDepth"]==0),
                entry.title+" keeps optional material methods off in automatic shows");
        }
        check(materials==33,"three independent sprite methods are available in 33 Original scenes");
    }
    {
        const auto entry=std::find_if(automatic.begin(),automatic.end(),[](const auto& e){return e.key=="original/molten-contours";});
        check(entry!=automatic.end() && entry->controlSpecs.count("reliefDepth") && entry->controlSpecs.count("meltWarp") && entry->controlSpecs.count("displacementColour"),
            "Molten Contours exposes three independent scene material methods");
        if(entry!=automatic.end())
        {
            VisualSceneClip material;material.scene=entry->key;material.title=entry->title;material.length=8;
            material.controls={{"reliefDepth",0.},{"meltWarp",0.},{"displacementColour",0.}};
            desk.engine.project.visualScenes={material};desk.refresh();
            check(lane.applyLook(material.id,{{"reliefDepth",.8}},false,"Relief depth")
                && desk.engine.project.visualScenes[0].controls["meltWarp"]==0 && desk.engine.project.visualScenes[0].controls["displacementColour"]==0,
                "enabling relief alone leaves warp and displacement colour off");
            check(lane.applyLook(material.id,{{"meltWarp",.7}},false,"Domain warping")
                && desk.engine.project.visualScenes[0].controls["reliefDepth"]==.8 && desk.engine.project.visualScenes[0].controls["displacementColour"]==0,
                "domain warping is independent and preserves the selected relief amount");
            check(lane.applyLook(material.id,{{"displacementColour",.9}},false,"Displacement colour")
                && desk.engine.project.visualScenes[0].controls["reliefDepth"]==.8 && desk.engine.project.visualScenes[0].controls["meltWarp"]==.7,
                "displacement colour combines without changing the other methods");
            check(lane.applyLook(material.id,{{"reliefDepth",0.}},false,"Relief off")
                && desk.engine.project.visualScenes[0].controls["meltWarp"]==.7 && desk.engine.project.visualScenes[0].controls["displacementColour"]==.9,
                "relief can be disabled while warp and colour remain enabled");
            desk.undo(false);check(desk.engine.project.visualScenes[0].controls["reliefDepth"]==.8,"Undo restores only the changed scene material method");
            juce::Random materialRandom(920);VisualSceneClip generated;automateVisualScene(generated,*entry,.8,materialRandom);
            check(!generated.automation.count("reliefDepth") && !generated.automation.count("meltWarp") && !generated.automation.count("displacementColour")
                && generated.controls["meltWarp"]==0 && generated.controls["displacementColour"]==0,
                "Auto Arrange keeps disabled material methods off rather than automating them into a compulsory trio");
        }
    }
    std::vector<VisualSceneClip> automated;juce::Random visualRandom(919);
    for(const auto& entry:automatic)
    {
        VisualSceneClip clip;clip.scene=entry.key;clip.title=entry.title;clip.start=automated.size()*8.;clip.length=8.;
        automateVisualScene(clip,entry,.7,visualRandom);automated.push_back(std::move(clip));
    }
    std::vector<VisualSceneClip> reloaded;
    check(validateVisualScenes(automated).wasOk() && readVisualScenes(visualScenesToJson(automated),reloaded).wasOk()
        && reloaded.size()==205 && juce::JSON::toString(visualScenesToJson(reloaded))==juce::JSON::toString(visualScenesToJson(automated)),
        "all 205 saved scene choices retain bounded automation");
    check(std::all_of(automated.begin(),automated.end(),[](const auto& clip){return !clip.automation.count("paletteStyle") && !clip.automation.count("paletteAmount");}),
        "automatic arrangement retains each scene's authored palette choice");
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
            if (entry.key!="original/beats-in-4d" && entry.key!="original/pulse-gallery" && entry.key!="original/mosaic-kiln" && entry.key!="original/firefly-assembly-colour-separation"
                && entry.key!="original/prism-bloom" && entry.key!="original/liquid-torus" && entry.key!="original/orbit-theatre" && entry.key!="original/jumping-techno-tunnel"
                && entry.key!="original/iridescent-wake" && entry.key!="original/ember-advection" && entry.key!="original/plasma-orchard"
                && entry.key!="original/tidal-lichen" && entry.key!="original/chromatic-torso" && entry.key!="original/resonant-iris"
                && entry.key!="original/enamel-scales" && entry.key!="original/afterglow-atlas" && entry.key!="original/raster-roulette"
                && entry.key!="original/terrain-prism" && entry.key!="original/hexa-resonator" && entry.key!="original/folded-scaffold") continue;
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
            write(lane.preview,entry.key.fromFirstOccurrenceOf("/",false,false)+"-native-scene.png");
            write(lane.browser->getContentComponent()->createComponentSnapshot(lane.browser->getContentComponent()->getLocalBounds()),
                entry.key.fromFirstOccurrenceOf("/",false,false)+"-native-browser.png");
        }
    }
    lane.browser->setVisible(false); lane.timerCallback(); engine.playing=false;
}

}
