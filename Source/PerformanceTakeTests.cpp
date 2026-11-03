#include "Workstation.h"
#include "PrivatePreparation.h"
#include "UiMessages.h"
#include <cmath>
#include <filesystem>
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace mixworks
{
namespace
{
class FourChannelDevice final : public juce::AudioIODevice
{
public:
    FourChannelDevice():AudioIODevice("Interface","Routing fixture") {}
    juce::StringArray getOutputChannelNames() override { return {"Audience L","Audience R","Headphones L","Headphones R"}; }
    juce::StringArray getInputChannelNames() override { return {}; }
    juce::Array<double> getAvailableSampleRates() override { return {8000}; }
    juce::Array<int> getAvailableBufferSizes() override { return {128}; }
    int getDefaultBufferSize() override { return 128; }
    juce::String open(const juce::BigInteger&,const juce::BigInteger& output,double,int) override { channels=output; opened=true; return {}; }
    void close() override { opened=false; }
    bool isOpen() override { return opened; }
    void start(juce::AudioIODeviceCallback* c) override { callback=c; if (c) c->audioDeviceAboutToStart(this); }
    void stop() override { if (callback) callback->audioDeviceStopped(); callback=nullptr; }
    bool isPlaying() override { return callback!=nullptr; }
    juce::String getLastError() override { return {}; }
    int getCurrentBufferSizeSamples() override { return 128; }
    double getCurrentSampleRate() override { return 8000; }
    int getCurrentBitDepth() override { return 32; }
    juce::BigInteger getActiveOutputChannels() const override { return channels; }
    juce::BigInteger getActiveInputChannels() const override { return {}; }
    int getOutputLatencyInSamples() override { return 0; }
    int getInputLatencyInSamples() override { return 0; }
    bool opened=false; juce::BigInteger channels {3}; juce::AudioIODeviceCallback* callback=nullptr;
};
class FourChannelType final : public juce::AudioIODeviceType
{
public:
    FourChannelType():AudioIODeviceType("Routing fixture") {}
    void scanForDevices() override {}
    juce::StringArray getDeviceNames(bool=false) const override { return {"Interface"}; }
    int getDefaultDeviceIndex(bool) const override { return 0; }
    int getIndexOfDevice(juce::AudioIODevice*,bool) const override { return 0; }
    bool hasSeparateInputsAndOutputs() const override { return true; }
    juce::AudioIODevice* createDevice(const juce::String&,const juce::String&) override { return new FourChannelDevice; }
};
class PrivateConstant final : public juce::AudioSource
{
public:
    void prepareToPlay(int,double) override {}
    void releaseResources() override {}
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override
    { for (int ch=0;ch<info.buffer->getNumChannels();++ch) for (int n=0;n<info.numSamples;++n) info.buffer->setSample(ch,info.startSample+n,1.f); }
};
}
void runPerformanceTakeParserTests(const std::function<void(bool,const juce::String&)>& check)
{
    PerformanceTake fixture; fixture.name="Parser fixture"; fixture.initial=projectToJson(Project());
    fixture.state=TakeState::complete; fixture.frames=32; fixture.sampleRate=8000;
    TakeEvent event; event.kind=TakeEventKind::transport; event.value=1; fixture.events.push_back(event);
    const auto valid=takeToJson(fixture);
    auto rejected=[&](juce::var value,const juce::String& name)
    {
        PerformanceTake output; output.id="unchanged sentinel"; output.frames=77;
        check(readTake(value,output).failed() && output.id=="unchanged sentinel" && output.frames==77,name);
    };
    PerformanceTake output;
    check(readTake(valid,output).wasOk() && output.frames==32 && output.events.size()==1,"accept a valid current take");
    for (const auto kind : {TakeEventKind::pluginState,TakeEventKind::midi})
    {
        auto value=valid.clone(); auto* item=(*value["events"].getArray())[0].getDynamicObject();
        item->setProperty("kind",(int)kind); item->setProperty("payload",juce::var(new juce::DynamicObject()));
        rejected(value,"reject unsupported replay event kind "+juce::String((int)kind)+" without mutating output");
        auto session=projectToJson(Project()); session.getDynamicObject()->setProperty("performanceTakes",juce::Array<juce::var>{value});
        check(parseProject(juce::JSON::toString(session)).error.isNotEmpty(),"session refuses unsupported replay event kind "+juce::String((int)kind));
    }
    for (const auto kind : {TakeEventKind::control,TakeEventKind::transport,TakeEventKind::seek,TakeEventKind::clipJump,
                           TakeEventKind::mute,TakeEventKind::solo,TakeEventKind::tempo,TakeEventKind::returnAutomation,TakeEventKind::project})
    {
        auto value=valid.clone(); auto* item=(*value["events"].getArray())[0].getDynamicObject();
        item->setProperty("kind",(int)kind); item->setProperty("value",kind==TakeEventKind::tempo ? 120. : 0.);
        item->setProperty("payload",kind==TakeEventKind::project ? fixture.initial : juce::var(true));
        check(readTake(value,output).wasOk() && output.events.size()==1 && output.events[0].kind==kind,
              "retain supported replay event kind "+juce::String((int)kind));
    }
    for (const auto* field : {"frames","frame","sequence"})
    {
        auto value=valid.clone();
        auto* object=juce::String(field)=="frames" ? value.getDynamicObject() : (*value["events"].getArray())[0].getDynamicObject();
        object->setProperty(field,.5);
        rejected(value,"reject fractional "+juce::String(field)+" without mutating output");
        object->setProperty(field,std::numeric_limits<double>::infinity());
        rejected(value,"reject nonfinite "+juce::String(field));
    }
    {
        auto value=valid.clone(); value.getDynamicObject()->setProperty("frames",(juce::int64)(INT64_MAX/2+1));
        rejected(value,"enforce the exact integer frame bound without double rounding");
    }
    auto projectEvent=event; projectEvent.kind=TakeEventKind::project; projectEvent.value=0; projectEvent.payload=fixture.initial;
    fixture.events.clear();
    for (int n=0;n<64;++n) { projectEvent.sequence=n; fixture.events.push_back(projectEvent); }
    check(readTake(takeToJson(fixture),output).wasOk() && output.events.size()==64,"accept exactly 64 structural snapshots");
    projectEvent.sequence=64; fixture.events.push_back(projectEvent);
    rejected(takeToJson(fixture),"reject the 65th structural snapshot");
    for (const bool initial : {true,false})
    {
        auto value=valid.clone();
        if (!initial) (*value["events"].getArray())[0].getDynamicObject()->setProperty("kind",(int)TakeEventKind::project);
        auto setSnapshot=[&](const juce::var& snapshot)
        {
            if (initial) value.getDynamicObject()->setProperty("initial",snapshot);
            else (*value["events"].getArray())[0].getDynamicObject()->setProperty("payload",snapshot);
        };
        const juce::String location=initial ? "initial" : "event";
        setSnapshot(juce::var(new juce::DynamicObject()));
        rejected(value,"reject malformed "+location+" project before hydration");
        auto nested=fixture.initial.clone(); nested.getDynamicObject()->setProperty("performanceTakes",juce::Array<juce::var>{valid});
        setSnapshot(nested); rejected(value,"reject nested takes in "+location+" project");
        auto malformed=fixture.initial.clone(); malformed.getDynamicObject()->setProperty("bpm",-1);
        setSnapshot(malformed); rejected(value,"validate "+location+" project settings before hydration");
        auto session=projectToJson(Project()); session.getDynamicObject()->setProperty("performanceTakes",juce::Array<juce::var>{value});
        check(parseProject(juce::JSON::toString(session)).error.isNotEmpty(),"session parsing rejects malformed "+location+" take snapshot");
    }
    auto interrupted=valid.clone(); interrupted.getDynamicObject()->setProperty("state",(int)TakeState::recording);
    check(readTake(interrupted,output).wasOk() && !output.complete && output.state==TakeState::complete,"valid interrupted take retains its recorded prefix without resuming");
    auto integral=valid.clone(); integral.getDynamicObject()->setProperty("frames",32.);
    (*integral["events"].getArray())[0].getDynamicObject()->setProperty("sequence",0.);
    check(readTake(integral,output).wasOk(),"integral double counters remain valid without truncation");
    {
        auto boundary=valid.clone(); boundary.getDynamicObject()->setProperty("frames",(juce::int64)(INT64_MAX/2));
        (*boundary["events"].getArray())[0].getDynamicObject()->setProperty("frame",(juce::int64)(INT64_MAX/2));
        check(readTake(boundary,output).wasOk() && output.frames==INT64_MAX/2 && output.events[0].frame==output.frames,"exact maximum integer frame remains lossless");
    }
}
void runPerformanceTakeTests(const std::function<void(bool,const juce::String&)>& check,const juce::File& report)
{
    if (juce::JUCEApplicationBase::getCommandLineParameters().contains("--mode-seek-ui-only") || juce::JUCEApplicationBase::getCommandLineParameters().contains("--take-seek-lifecycle-only"))
    {
        auto owned=std::make_unique<Workstation>(false); auto& ui=*owned; ui.stopTimer(); ui.setSize(1250,800); ui.engine.prepareRender(8000,128);
        auto pump=[](int milliseconds=10)
        {
            const auto until=juce::Time::getMillisecondCounterHiRes()+milliseconds;
            do
            {
               #if JUCE_WINDOWS
                MSG message; while (PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
               #endif
                juce::Thread::sleep(1);
            } while (juce::Time::getMillisecondCounterHiRes()<until);
        };
        auto complete=[&](int result)
        {
            auto* modal=juce::Component::getCurrentlyModalComponent(); check(modal!=nullptr,"owned mode/seek popup or dialog exists");
            if (modal) { modal->exitModalState(result); pump(); }
        };
        auto drain=[&]
        {
            pump(180); check(!juce::Component::getCurrentlyModalComponent(),"bounded take-seek worker/modal completion drains");
        };
        auto media=std::make_shared<Media>(); media->sampleRate=8000; media->name="Mode seek fixture";
        media->file=report.getSiblingFile("mode-seek.wav"); media->samples.setSize(2,8000);
        for (int ch=0;ch<2;++ch) for (int n=0;n<8000;++n) media->samples.setSample(ch,n,.25f);
        { juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream=media->file.createOutputStream(); auto writer=wav.createWriterFor(stream,juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(2).withBitsPerSample(24));
          check(writer && writer->writeFromAudioSampleBuffer(media->samples,0,8000),"write one tiny owned mode/seek source"); }
        media->analysis.bpm=120; media->analysis.downbeatOrigin=0; media->analysisSource={audioDigest(media->file),media->file.getSize(),8000,8000,2};
        check(writeAnalysisRecord(media->file,media->analysisRecord()).isEmpty(),"reuse only isolated mode/seek fixture analysis");
        Clip clip; clip.media=media; clip.length=1; Project programme; programme.masterDb=0; programme.clips={clip};
        PerformanceTake a,b; a.name="Seek A"; b.name="Seek B"; a.sampleRate=b.sampleRate=8000; a.frames=b.frames=8000;
        a.state=b.state=TakeState::complete; a.initial=projectToJson(programme); programme.masterDb=-12; b.initial=projectToJson(programme); programme.masterDb=0;
        auto reset=[&]
        {
            juce::ModalComponentManager::getInstance()->cancelAllModalComponents(); pump(150); ui.libraryPreview.stop(); ui.engine.stop();
            ui.engine.exporting=false; ui.busy=false; ui.engine.setWorkMode(WorkMode::arrange); ui.libraryPreview.setPrivateOutput(false);
            ui.engine.project=programme; ui.engine.project.performanceTakes={a,b}; ui.engine.library={media}; ui.engine.prepareClipProcessing(); ui.engine.position=.375;
            ui.undoHistory.clear(); ui.redoHistory.clear(); ui.workspaceDirty=false; ui.refresh();
        };
        auto mode=[&](int result) { ui.workModeButton.onClick(); complete(result); };
        auto pcm=[&]
        {
            float left[32]{},right[32]{}; float* outputs[]{left,right}; ui.engine.audioDeviceIOCallbackWithContext(nullptr,0,outputs,2,32,{});
            return juce::FloatVectorOperations::findMinAndMax(left,32).getEnd();
        };
        auto seek=[&](const juce::String& text,int answer=1,int slot=0)
        {
            ui.showTakeMenu(); complete(300+slot); auto* dialog=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
            check(dialog && dialog->getTextEditor("seconds"),"seek dialog exposes performance-time input");
            if (dialog) dialog->getTextEditor("seconds")->setText(text); complete(answer);
        };
        if (juce::JUCEApplicationBase::getCommandLineParameters().contains("--take-seek-lifecycle-only"))
        {
            reset(); seek("0.1"); drain(); const auto level=pcm();
            check(level>.1f && ui.engine.position==.375 && ui.undoHistory.empty(),"lifecycle baseline reconstructs owned take without programme edit");
            for (int state=0;state<8;++state)
            {
                reset(); seek("0.1");
                if (state==0) ui.engine.stop();
                if (state==1) { ui.engine.setPlaying(true); ui.engine.stop(); }
                if (state==2) { ui.engine.setWorkMode(WorkMode::recordPerform); ui.engine.setWorkMode(WorkMode::arrange); }
                if (state==3) { check(ui.engine.auditionTake(1).wasOk(),"new direct audition is admitted while earlier seek completion is pending"); }
                if (state==4) seek("0.2",0,1); // New intent supersedes the earlier worker even when cancelled.
                if (state==5) { ui.newSession(); }
                if (state==6) { ui.showTakeMenu(); complete(5); }
                if (state==7) { const auto saved=juce::JSON::toString(ui.engine.captureProject()); check(ui.engine.restoreProject(saved).isEmpty(),"replacement document with identical take IDs is admitted"); }
                drain(); const auto output=pcm();
                check(state==3 ? std::abs(output-level*juce::Decibels::decibelsToGain(-12.f))<1e-5f : output==0,
                      "pending seek cannot override later lifecycle intent "+juce::String(state));
            }
            for (const bool button:{false,true})
            {
                reset(); seek("0"); juce::Thread::sleep(35);
                auto* progress=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
                check(progress!=nullptr,"owned seek progress exists before close/cancel");
                if (progress) { if (button) progress->triggerButtonClick("Cancel"); else static_cast<juce::Component*>(progress)->userTriedToCloseWindow(); }
                pump(); drain(); check(pcm()==0,button ? "late Cancel preserves stopped audition" : "late progress-window close cannot install its renderer");
            }
            reset(); ui.engine.prepareRender(16000,128); seek("0.1"); drain();
            check(pcm()==0 && ui.status.contains("recorded sample rate") && ui.undoHistory.empty(),"rate mismatch leaves programme/history intact with actionable feedback");
            ui.engine.prepareRender(8000,128);
            auto missing=a.initial.clone();
            (*missing["media"].getArray())[0].getDynamicObject()->setProperty("file",report.getSiblingFile("missing-owned.wav").getFullPathName());
            for (int failure=0;failure<3;++failure)
            {
                reset();
                if (failure==0) ui.engine.project.performanceTakes[0].initial=missing;
                if (failure==1) { TakeEvent e; e.kind=TakeEventKind::project; e.payload=missing; ui.engine.project.performanceTakes[0].events={e}; }
                if (failure==2) ui.engine.project.performanceTakes[0].initial=juce::var(new juce::DynamicObject());
                const auto before=juce::JSON::toString(ui.engine.captureProject()); ui.setStatus("Owned seek error sentinel"); seek("0.1"); drain();
                check(pcm()==0 && !ui.busy && ui.status!="Owned seek error sentinel" && (failure==2 ? ui.status.isNotEmpty() : ui.status.contains("Required source")) && !ui.status.contains("Auditioning")
                      && ui.engine.position==.375 && ui.undoHistory.empty() && juce::JSON::toString(ui.engine.captureProject())==before,
                      "seek restore error preserves programme/takes/history and reports failure "+juce::String(failure));
            }
            reset(); seek("0.1"); drain(); check(std::abs(pcm()-level)<1e-5f,"valid seek succeeds after lifecycle cancellation and restore failures");
            reset(); seek("0.1"); auto* progress=juce::Component::getCurrentlyModalComponent();
            auto stream=report.getSiblingFile("seek-lifecycle-progress.png").createOutputStream(); juce::PNGImageFormat png;
            check(progress && stream && png.writeImageToStream(progress->createComponentSnapshot(progress->getLocalBounds()),*stream),"capture owned seek lifecycle progress");
            owned.reset(); pump(250);
            check(!juce::Component::getCurrentlyModalComponent(),"destroyed owned workstation drains pending seek safely");
            return;
        }
        reset(); mode(2);
        check(ui.engine.workMode==WorkMode::recordPerform && ui.libraryPreview.usesPrivateOutput() && ui.layout.workMode==1 && ui.workspaceDirty,"mode choice updates engine/private routing/workspace together");
        mode(1); check(ui.engine.workMode==WorkMode::arrange && !ui.libraryPreview.usesPrivateOutput() && ui.layout.workMode==0,"Arrange choice restores audience preview policy");
        reset(); mode(0); check(ui.engine.workMode==WorkMode::arrange && !ui.workspaceDirty,"cancelled mode menu leaves workspace unchanged");
        // The fake interface opens no hardware and keeps the production device alone.
        ui.engine.devices.addAudioDeviceType(std::make_unique<FourChannelType>());
        check(ui.engine.devices.initialise(0,2,nullptr,false,"Interface").isEmpty(),"open fake interface only for mode admission");
        mode(2); check(ui.libraryPreview.select({"Routing fixture","Interface",2}).wasOk(),"configure fake independent preview pair");
        pump(); // Deliver the device-change notification before starting preview.
        PrivateConstant privateAudio;
        check(ui.libraryPreview.playPrepared(privateAudio,"mode-private").wasOk(),"start fake private preview for unchanged/blocked mode choices");
        ui.workspaceDirty=false; mode(2);
        check(ui.libraryPreview.active("mode-private") && !ui.workspaceDirty,"choosing current mode preserves private transport and workspace");
        ui.libraryPreview.playPrepared(privateAudio,"mode-private"); ui.workModeButton.onClick(); ui.engine.exporting=true; complete(1);
        check(ui.engine.workMode==WorkMode::recordPerform && ui.libraryPreview.active("mode-private"),"mode callback rechecks export before stopping private preview"); ui.engine.exporting=false;
        reset(); mode(2); ui.showTakeMenu(); ui.engine.setWorkMode(WorkMode::arrange); complete(50);
        check(!ui.engine.automationArmed[0] && ui.engine.workMode==WorkMode::arrange,"stale exclusion cannot arm Arrange after mode change");
        reset(); mode(2); ui.showTakeMenu(); ui.engine.armAutomation(0,false); complete(50);
        check(!ui.engine.automationArmed[0],"stale exclusion cannot reverse a newer lane eligibility choice");
        reset(); mode(2); ui.showTakeMenu(); ui.engine.armAutomation(-1,false); complete(54);
        check(!ui.engine.masterAutomationArmed,"stale exclusion cannot reverse newer master eligibility");
        reset(); mode(2); ui.engine.setPlaying(true); check(ui.engine.capturingPerformance(),"supported Play starts owned capture for exclusions");
        for (int owner=0;owner<=trackCount;++owner) { ui.showTakeMenu(); complete(50+owner); }
        check(ui.engine.capturingPerformance() && std::none_of(ui.engine.automationArmed.begin(),ui.engine.automationArmed.end(),[](bool v){ return v; }) && !ui.engine.masterAutomationArmed,"all five exclusions apply during active capture without ending it");
        mode(1); check(!ui.engine.capturingPerformance() && ui.engine.project.performanceTakes.size()==3 && ui.engine.project.performanceTakes.back().state==TakeState::complete,"leaving Record/Perform finalises the owned capture and retains takes");
        reset(); seek("0.1"); drain(); const auto level=pcm();
        check(level>.1f && ui.engine.position==.375 && ui.undoHistory.empty(),"seek reconstructs audible take without programme cursor or Undo changes");
        ui.showTakeMenu(); complete(5); check(pcm()==0,"Stop take audition silences the reconstructed take");
        reset(); seek("0.1",0); drain(); check(pcm()==0,"cancelled seek input does not begin audition");
        for (const auto* text:{"garbage","0.1oops","","-1","1"})
        { reset(); seek(text); drain(); check(pcm()==0,"seek rejects invalid or out-of-range input: "+juce::String(text)); }
        reset(); seek("0");
        // No reconstruction samples are needed at zero; give the tiny worker CPU
        // without dispatching its 100 ms completion timer, then press real Cancel.
        juce::Thread::sleep(35); auto* progress=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        check(progress && progress->getButton("Cancel"),"seek reconstruction exposes native Cancel");
        if (progress) progress->triggerButtonClick("Cancel"); pump(); drain();
        check(pcm()==0,"late Cancel cannot install a completed seek renderer");
        for (int state=0;state<5;++state)
        {
            reset(); seek("0.1");
            if (state==0) ui.engine.project.performanceTakes.erase(ui.engine.project.performanceTakes.begin());
            if (state==1) ui.engine.project.performanceTakes[0].frames=4000;
            if (state==2) std::swap(ui.engine.project.performanceTakes[0],ui.engine.project.performanceTakes[1]);
            if (state==3) ui.engine.exporting=true;
            if (state==4) mode(2);
            drain(); ui.engine.exporting=false;
            check(state==2 ? std::abs(pcm()-level)<1e-5f : pcm()==0,"seek completion retains target/admission after state change "+juce::String(state));
        }
        reset(); ui.showTakeMenu(); complete(300); auto* dialog=juce::Component::getCurrentlyModalComponent();
        auto stream=report.getSiblingFile("take-seek-dialog.png").createOutputStream(); juce::PNGImageFormat png;
        check(dialog && stream && png.writeImageToStream(dialog->createComponentSnapshot(dialog->getLocalBounds()),*stream),"capture owned take seek input dialog");
        complete(0); reset(); ui.engine.devices.closeAudioDevice(); return;
    }
    if (juce::JUCEApplicationBase::getCommandLineParameters().contains("--export-target-ui-only")
        || juce::JUCEApplicationBase::getCommandLineParameters().contains("--destination-safety-only")
        || juce::JUCEApplicationBase::getCommandLineParameters().contains("--save-export-workflow-only"))
    {
        Workstation ui(false); ui.stopTimer(); ui.setSize(1250,800); ui.engine.prepareRender(8000,128);
        auto pump=[]
        {
            const auto until=juce::Time::getMillisecondCounterHiRes()+25;
            do
            {
               #if JUCE_WINDOWS
                MSG message; while (PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
               #endif
                juce::Thread::sleep(1);
            } while (juce::Time::getMillisecondCounterHiRes()<until);
        };
        auto idle=[&]
        {
            const auto deadline=juce::Time::getMillisecondCounterHiRes()+5000;
            while (ui.busy && juce::Time::getMillisecondCounterHiRes()<deadline) pump();
            check(!ui.busy,"bounded owned export/tempo work completes");
        };
        auto complete=[&](int result)
        {
            auto* modal=juce::Component::getCurrentlyModalComponent();
            check(modal!=nullptr,"owned export dialog exists before completion");
            if (modal) { modal->exitModalState(result); pump(); }
        };
        std::function<juce::FileBrowserComponent*(juce::Component*)> browser=[&](juce::Component* c)->juce::FileBrowserComponent*
        {
            if (auto* b=dynamic_cast<juce::FileBrowserComponent*>(c)) return b;
            if (c) for (auto* child:c->getChildren()) if (auto* b=browser(child)) return b;
            return nullptr;
        };
        int serial=0;
        auto destination=[&]
        {
            const auto file=report.getSiblingFile("export-target-"+juce::String(++serial)+".wav");
            auto* b=browser(juce::Component::getCurrentlyModalComponent());
            check(b!=nullptr,"real non-native WAV chooser exposes its owned file browser");
            if (b) { b->setRoot(file.getParentDirectory()); b->setFileName(file.getFileName()); }
            return file;
        };
        PerformanceTake a,b; a.name="Export A"; b.name="Export B"; a.initial=b.initial=projectToJson(Project());
        a.sampleRate=b.sampleRate=8000; a.frames=800; b.frames=1600; a.state=b.state=TakeState::complete;
        auto reset=[&]
        {
            idle(); juce::ModalComponentManager::getInstance()->cancelAllModalComponents(); pump(); ui.chooser.reset();
            ui.engine.stop(); ui.busy=false; ui.engine.exporting=false; ui.engine.project=Project(); ui.engine.project.performanceTakes={a,b};
            ui.selectedExportTake=-1; ui.pendingTempo=0; ui.afterTempoPreparation={}; ui.undoHistory.clear(); ui.redoHistory.clear(); ui.refresh();
        };
        auto frames=[&](const juce::File& file)
        {
            idle(); auto reader=std::unique_ptr<juce::AudioFormatReader>(ui.engine.formats.createReaderFor(file));
            return reader ? reader->lengthInSamples : (juce::int64)-1;
        };
        if (juce::JUCEApplicationBase::getCommandLineParameters().contains("--save-export-workflow-only")
            || juce::JUCEApplicationBase::getCommandLineParameters().contains("--destination-safety-only"))
        {
            // One tiny original, two edited placements and real asynchronous UI
            // completions. No detector, device, plugin or long-clock benchmark.
            auto media=std::make_shared<Media>(); media->name="Edited source"; media->sampleRate=8000;
            media->manualBpm=120; media->manualBeatOrigin=0; media->manualKey=0;
            media->file=report.getSiblingFile("workflow-original.wav"); media->samples.setSize(2,1600);
            for (int i=0;i<1600;++i) { media->samples.setSample(0,i,.25f); media->samples.setSample(1,i,-.125f); }
            {
                juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> output=media->file.createOutputStream();
                auto writer=wav.createWriterFor(output,juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(2).withBitsPerSample(24));
                check(writer && writer->writeFromAudioSampleBuffer(media->samples,0,1600),"write bounded original stereo fixture");
            }
            juce::MemoryBlock originalBytes; check(media->file.loadFileAsData(originalBytes),"retain original source bytes for preservation comparison");
            Clip first; first.media=media; first.start=.05; first.offset=.025; first.length=.1;
            Clip second=first; second.id=juce::Uuid().toString(); second.track=1; second.start=.2; second.offset=.1; second.length=.05;
            ui.engine.library={media}; ui.engine.project.clips={first,second}; ui.engine.project.masterDb=0;
            ui.engine.project.name="Save export journey"; ui.dirty=true; ui.refresh();
            auto pick=[&](const juce::String& name)
            {
                auto target=report.getSiblingFile(name); auto* b=browser(juce::Component::getCurrentlyModalComponent());
                check(b!=nullptr,"real save/export chooser exposes the destination field");
                if (b) { b->setRoot(target.getParentDirectory()); b->setFileName(target.getFileName()); }
                return target;
            };
            auto capture=[&](const juce::String& name)
            {
                auto* modal=juce::Component::getCurrentlyModalComponent();
                auto output=report.getSiblingFile(name+".png").createOutputStream(); juce::PNGImageFormat png;
                check(modal && output && png.writeImageToStream(modal->createComponentSnapshot(modal->getLocalBounds()),*output),"capture "+name);
            };
            int savedCallbacks=0;
            if (juce::JUCEApplicationBase::getCommandLineParameters().contains("--destination-safety-only"))
            {
                std::function<juce::Button*(juce::Component*, const juce::String&)> button = [&](juce::Component* c, const juce::String& label)->juce::Button*
                {
                    if (auto* b=dynamic_cast<juce::Button*>(c); b && b->getButtonText()==label) return b;
                    if (c) for (auto* child:c->getChildren()) if (auto* b=button(child,label)) return b;
                    return nullptr;
                };
                auto submit=[&]
                {
                    auto* modal=juce::Component::getCurrentlyModalComponent(); auto* b=browser(modal);
                    auto* action=b ? button(modal,b->getActionVerb()) : nullptr;
                    check(action && action->isEnabled(),"submit through actual file chooser action including overwrite admission");
                    if (action) { action->triggerClick(); pump(); }
                };
                const auto marker=juce::String("previous destination bytes");
                // The typed path does not exist; the extension-corrected path does.
                for (int kind=0;kind<4;++kind)
                {
                    const auto base="normalized-"+juce::String(kind);
                    const auto extension=kind==0 ? ".mixworks" : kind==1 ? ".wav" : kind==2 ? ".m3u8" : ".txt";
                    const auto target=report.getSiblingFile(base+extension);
                    check(target.replaceWithText(marker),"seed existing normalized destination");
                    auto choose=[&]
                    {
                        if (kind==0) ui.chooseSave([&] { ++savedCallbacks; },false);
                        else if (kind==1) ui.chooseExport();
                        else ui.choosePlaylistExport(kind==3);
                        pick(base); submit();
                    };
                    choose(); auto* confirm=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
                    const bool correct=confirm && confirm->getName()=="Replace existing file?";
                    check(correct && target.loadFileAsString()==marker,"final filename is confirmed before replacing output kind "+juce::String(kind));
                    if (!correct) { juce::ModalComponentManager::getInstance()->cancelAllModalComponents(); pump(); ui.chooser.reset(); return; }
                    if (kind==0) capture("normalized-replacement");
                    complete(0);
                    check(target.loadFileAsString()==marker && !target.getSiblingFile(target.getFileName()+".bak").exists()
                        && !ui.choosingSave && !ui.busy && !ui.exportTask && savedCallbacks==0,
                        "cancelled replacement keeps bytes, creates no backup and releases chooser state");
                    choose(); complete(1); if (kind==1) complete(1); idle();
                    check(target.loadFileAsString()!=marker && target.getSiblingFile(target.getFileName()+".bak").loadFileAsString()==marker,
                          "confirmed replacement retains exact previous bytes for output kind "+juce::String(kind));
                    if (kind==0) { check(savedCallbacks==1 && ui.sessionFile==target && !ui.dirty,"confirmed Save As completes once"); savedCallbacks=0; }
                }
                // Same-name overwrites still use JUCE's original confirmation.
                ui.choosePlaylistExport(true); const auto existing=pick("normalized-3.txt"); const auto existingText=existing.loadFileAsString(); submit();
                auto* overwrite=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
                check(overwrite && overwrite->getName()=="File already exists","unchanged filename retains chooser overwrite confirmation");
                complete(0); check(browser(juce::Component::getCurrentlyModalComponent())!=nullptr,"cancel overwrite returns to the chooser");
                complete(0); check(existing.loadFileAsString()==existingText,"cancel chooser preserves existing track list");

                const auto session=ui.sessionFile; ui.dirty=true;
                const auto document=juce::JSON::toString(ui.engine.captureProject());
                const auto missing=report.getSiblingFile("missing-folder").getChildFile("output.wav");
                ui.beginWavExport(missing,{});
                check(!ui.busy && !ui.exportTask && !ui.engine.exporting && !missing.exists()
                    && ui.getProperties()["lastMessageDetails"].toString().contains("existing output folder"),"missing WAV folder reports a useful error and releases export state");
                ui.chooseExport(); pick(media->file.getFileName()); submit(); complete(1); complete(1); idle();
                check(ui.getProperties()["lastMessageDetails"].toString().contains("source audio") && !ui.busy && !ui.engine.exporting,
                      "actual WAV chooser refuses replacing original source after overwrite acceptance");
                // Protect arrangement-only references and real aliases across all writers.
                ui.engine.library.clear();
                check(ui.engine.saveProject(media->file).contains("source audio"),"session writer protects arrangement-only audio");
                const auto folder=report.getSiblingFile("directory.wav"); check(folder.createDirectory().wasOk(),"create isolated directory destination");
                check(ui.engine.exportWav(folder).isNotEmpty() && folder.isDirectory(),"WAV rejects a directory destination intact");
               #if JUCE_WINDOWS
                for (int kind=0;kind<4;++kind)
                {
                    const auto alias=report.getSiblingFile("source-alias"+juce::String(kind)+(kind==0 ? ".mixworks" : kind==1 ? ".wav" : kind==2 ? ".m3u8" : ".txt"));
                    std::error_code error; std::filesystem::create_hard_link(std::filesystem::path(media->file.getFullPathName().toWideCharPointer()),std::filesystem::path(alias.getFullPathName().toWideCharPointer()),error);
                    check(!error,"create disposable source alias");
                    if (!error)
                    {
                        const auto refusal=kind==0 ? ui.engine.saveProject(alias) : kind==1 ? ui.engine.exportWav(alias)
                            : ui.engine.exportPlaylist(alias,kind==2 ? PlaylistExport::m3u : PlaylistExport::text);
                        juce::MemoryBlock bytes;
                        check(refusal.contains("alias") && alias.loadFileAsData(bytes) && bytes==originalBytes,"writer refuses source identity through a hardlink");
                        check(alias.deleteFile(),"remove owned alias so normal fixture retention can remove audio");
                    }
                }
                // An OS-held destination reliably fails at commit without modifying permissions.
                for (int kind=0;kind<3;++kind)
                {
                    const auto target=report.getSiblingFile("locked"+juce::String(kind)+(kind==0 ? ".mixworks" : kind==1 ? ".wav" : ".txt"));
                    check(target.replaceWithText(marker),"seed locked destination");
                    const auto held=CreateFileW(target.getFullPathName().toWideCharPointer(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
                    check(held!=INVALID_HANDLE_VALUE,"hold only the owned destination against replacement");
                    if (held==INVALID_HANDLE_VALUE) continue;
                    ui.getProperties().remove("lastMessageDetails");
                    if (kind==0) { ui.chooseSave([&] { ++savedCallbacks; },false); pick(target.getFileName()); complete(1); }
                    else if (kind==1) { ui.chooseExportEnd(target); complete(1); idle(); }
                    else { ui.choosePlaylistExport(true); pick(target.getFileName()); complete(1); }
                    check(target.loadFileAsString()==marker && ui.dirty && ui.sessionFile==session && savedCallbacks==0
                        && !ui.busy && !ui.engine.exporting && !ui.exportTask && !ui.choosingSave
                        && ui.getProperties()["lastMessageDetails"].toString().contains(kind==1 ? "Cannot commit" : target.getFullPathName()),
                        "failed write reports this failure, preserves destination and unsaved session and releases task ownership");
                    if (kind==1)
                    {
                        showUiDetails(ui,"Message details",ui.getProperties()["lastMessageDetails"].toString()); capture("export-commit-error"); complete(0);
                    }
                    CloseHandle(held);
                    if (kind==0) { ui.chooseSave([&] { ++savedCallbacks; },false); pick(target.getFileName()); complete(1);
                        check(savedCallbacks==1 && !ui.dirty && ui.sessionFile==target,"Save As succeeds after destination becomes writable");
                        ui.sessionFile=session; ui.dirty=true; savedCallbacks=0; }
                    else if (kind==1) { ui.chooseExportEnd(target); complete(1); check(frames(target)==2000,"WAV retry succeeds with the full expected frame count"); }
                    else { ui.choosePlaylistExport(true); pick(target.getFileName()); complete(1); }
                    check(target.loadFileAsString()!=marker && target.getSiblingFile(target.getFileName()+".bak").loadFileAsString()==marker,
                          "retry replaces only the destination and retains its previous contents");
                }
               #endif
                const auto cancelled=report.getSiblingFile("mid-cancel.wav"); check(cancelled.replaceWithText(marker),"seed mid-export cancellation destination");
                juce::String error; auto renderer=ui.engine.createExportRenderer(error); auto cancel=std::make_shared<std::atomic<bool>>(false);
                check(renderer!=nullptr && error.isEmpty(),"prepare bounded cancellation renderer");
                if (renderer)
                {
                    const auto result=renderer->writeWav(cancelled,{},cancel,[&](auto done,const auto&) { if (done>=128) *cancel=true; });
                    check(result.outcome==WavExportOutcome::cancelled && result.writtenFrames==128 && cancelled.loadFileAsString()==marker
                        && !cancelled.getSiblingFile(cancelled.getFileName()+".bak").exists() && !renderer->exporting,"mid-write cancellation preserves destination without a backup or busy owner");
                    *cancel=false; check(renderer->writeWav(cancelled,{},cancel).wasOk(),"same renderer can retry after cancellation");
                }
                juce::MemoryBlock bytes;
                ui.engine.library={media};
                check(media->file.loadFileAsData(bytes) && bytes==originalBytes && ui.dirty && ui.sessionFile==session && ui.undoHistory.empty()
                    && juce::JSON::toString(ui.engine.captureProject())==document,"failure and retry journey keeps original audio, mix, dirty state and Undo intact");
                ui.chooser.reset(); ui.engine.devices.closeAudioDevice(); return;
            }
            ui.chooseSave([&] { ++savedCallbacks; },false); auto cancelled=pick("cancelled.mixworks"); complete(0);
            check(!cancelled.exists() && ui.dirty && ui.sessionFile==juce::File() && savedCallbacks==0 && !ui.choosingSave,
                  "Save As cancellation preserves dirty mix and does not run its continuation");
            ui.chooseSave([&] { ++savedCallbacks; },false); auto session=pick("workflow.mixworks"); capture("save-mix-chooser"); complete(1);
            check(session.existsAsFile() && ui.sessionFile==session && !ui.dirty && savedCallbacks==1 && ui.undoHistory.empty(),
                  "Save As commits chosen session and clears dirty state without adding Undo");
            const auto savedText=session.loadFileAsString();
            ui.engine.project.name="Updated mix"; ui.dirty=true; ui.chooseSave({},true);
            check(!ui.choosingSave && !ui.dirty && session.loadFileAsString()!=savedText && ui.sessionFile==session,
                  "Save reuses the current destination without another chooser");
            ui.chooseSave({},false); auto copy=pick("workflow-copy.mixworks"); complete(1);
            check(copy.existsAsFile() && session.existsAsFile() && ui.sessionFile==copy,"Save As creates a separate mix and keeps the prior file");
            ui.openProjectFile(session); idle();
            check(ui.sessionFile==session && !ui.dirty && ui.engine.project.clips.size()==2
                && ui.engine.project.clips[0].id==first.id && ui.engine.project.clips[1].id==second.id
                && ui.engine.project.clips[0].offset==.025 && ui.engine.project.clips[1].start==.2,
                  "actual asynchronous reopen restores trimmed placements and stable identities");
            ui.chooseExport(); auto wav=pick("edited-mix.wav"); complete(1); capture("mix-export-ending"); complete(1);
            check(frames(wav)==2000,"ordinary mix chooser exports exactly through the final edited placement");
            auto reader=std::unique_ptr<juce::AudioFormatReader>(ui.engine.formats.createReaderFor(wav));
            bool exact=reader && reader->sampleRate==8000 && reader->numChannels==2 && reader->bitsPerSample==24 && reader->lengthInSamples==2000;
            if (exact)
            {
                juce::AudioBuffer<float> pcm(2,2000); exact=reader->read(&pcm,0,2000,0,true,true);
                for (int i=0;i<2000;++i)
                {
                    const bool audible=(i>=400 && i<1200) || i>=1600;
                    exact &= std::abs(pcm.getSample(0,i)-(audible ? .25f : 0.f))<2e-6f
                          && std::abs(pcm.getSample(1,i)-(audible ? -.125f : 0.f))<2e-6f;
                }
            }
            check(exact,"written 24-bit stereo WAV matches independent sample oracle including both silent gaps");
            ui.chooseExport(); auto noWav=pick("cancelled-mix.wav"); complete(1); complete(0); idle();
            check(!noWav.exists() && !ui.exportTask && !ui.engine.exporting,"Cancel at the ending prompt writes no audio and releases export state");
            for (bool text : {false,true})
            {
                ui.choosePlaylistExport(text); auto list=pick(text ? "track-list.txt" : "sources.m3u8");
                capture(text ? "track-list-chooser" : "source-playlist-chooser"); complete(1);
                if (text) check(list.loadFileAsString().contains("Edited source") && list.loadFileAsString().contains("Audio edits remain in the .mixworks session"),
                                "track-list chooser writes titles and explains the separate session edits");
                else
                {
                    const auto parsed=readPlaylist(list);
                    check(parsed.error.isEmpty() && parsed.files.size()==2 && parsed.files[0]==media->file && parsed.files[1]==media->file,
                          "source-playlist chooser preserves repeated original sources instead of claiming rendered edits");
                }
                ui.choosePlaylistExport(text); auto noList=pick(text ? "cancelled-list.txt" : "cancelled-sources.m3u8"); complete(0);
                check(!noList.exists(),"Cancel preserves destination for "+juce::String(text ? "track list" : "source playlist"));
            }
            juce::MemoryBlock afterBytes;
            check(!ui.dirty && ui.undoHistory.empty() && ui.sessionFile==session && media->file.loadFileAsData(afterBytes) && afterBytes==originalBytes,
                  "export journey leaves session identity, dirty state, Undo and original audio intact");
            ui.chooser.reset(); ui.engine.devices.closeAudioDevice(); return;
        }
        reset(); ui.showTakeMenu(); complete(101); auto file=destination(); complete(1);
        check(frames(file)==800 && ui.undoHistory.empty(),"unchanged chooser exports intended take duration without Undo");
        reset(); ui.chooseExport(0); file=destination(); std::swap(ui.engine.project.performanceTakes[0],ui.engine.project.performanceTakes[1]); complete(1);
        check(frames(file)==800,"reordered chooser exports the original stable take");
        reset(); ui.chooseExport(0); file=destination(); ui.engine.project.performanceTakes.erase(ui.engine.project.performanceTakes.begin()); complete(1); idle();
        check(!file.exists() && !ui.exportTask,"removed chooser target cannot export its replacement");
        reset(); ui.chooseExport(0); file=destination(); ui.engine.project.performanceTakes[0].frames=2400; complete(1); idle();
        check(!file.exists(),"changed take invalidates pending export choice");
        reset(); ui.chooseExport(0); file=destination(); ui.selectedExportTake=1; complete(1);
        check(frames(file)==800,"chooser target is independent of mutable export selection");
        reset(); ui.chooseExport(0); file=destination(); complete(0); idle();
        check(!file.exists() && !ui.exportTask && ui.undoHistory.empty(),"cancelled WAV chooser keeps files and history");
        for (int admission=0;admission<2;++admission)
        {
            reset(); ui.chooseExport(0); file=destination();
            if (admission==0) ui.busy=true; else ui.engine.exporting=true;
            complete(1); ui.busy=false; ui.engine.exporting=false; idle();
            check(!file.exists() && !ui.exportTask,"chooser rejects changed "+juce::String(admission==0 ? "busy" : "export")+" admission");
        }
        // The real tempo worker installs the continuation. Hold just that owned
        // continuation until its worker has completed, then vary its target state.
        for (int scenario=0;scenario<3;++scenario)
        {
            reset(); ui.pendingTempo=121; ui.chooseExport(0); auto continuation=std::move(ui.afterTempoPreparation); ui.afterTempoPreparation={};
            check((bool)continuation,"pending tempo installs a deferred export continuation"); idle();
            if (scenario==0) std::swap(ui.engine.project.performanceTakes[0],ui.engine.project.performanceTakes[1]);
            else if (scenario==1) ui.engine.project.performanceTakes.erase(ui.engine.project.performanceTakes.begin());
            else ui.engine.exporting=true;
            if (continuation) continuation();
            if (scenario==0) { file=destination(); complete(1); check(frames(file)==800,"deferred tempo export retains original take across reorder"); }
            else { check(!juce::Component::getCurrentlyModalComponent(),"deferred tempo export rejects removed target or changed admission"); }
            ui.engine.exporting=false;
        }
        reset(); ui.chooseExport(0); file=destination();
        auto* modal=juce::Component::getCurrentlyModalComponent(); auto stream=report.getSiblingFile("export-target-chooser.png").createOutputStream(); juce::PNGImageFormat png;
        check(modal && stream && png.writeImageToStream(modal->createComponentSnapshot(modal->getLocalBounds()),*stream),"capture owned export target chooser");
        complete(0); reset();
        // Remaining bounded take controls: each exclusion owner and chronology
        // removal use the actual popup/modal route, without a physical device.
        ui.engine.setWorkMode(WorkMode::recordPerform);
        for (int owner=0;owner<=trackCount;++owner)
        {
            ui.showTakeMenu(); complete(50+owner);
            check(owner==trackCount ? !ui.engine.masterAutomationArmed : !ui.engine.automationArmed[(size_t)owner],
                  "capture exclusion toggles its intended owner "+juce::String(owner));
        }
        ui.engine.setWorkMode(WorkMode::arrange);
        TakeEvent first,second; first.kind=second.kind=TakeEventKind::control; first.frame=0; second.frame=400; second.sequence=1; second.value=-6;
        ui.engine.project.performanceTakes[0].events={first,second}; ui.editTakeEvent(0,0); complete(2);
        const auto& edited=ui.engine.project.performanceTakes[0];
        check(edited.events.size()==1 && edited.events[0].frame==400 && edited.events[0].sequence==1 && edited.events[0].value==-6 && ui.undoHistory.size()==1,
              "Remove event preserves surviving chronology and makes one Undo step");
        ui.undo(false); check(ui.engine.project.performanceTakes[0].events.size()==2,"Undo restores removed event chronology");
        reset(); return;
    }
    if (juce::JUCEApplicationBase::getCommandLineParameters().contains("--private-popup-only"))
    {
        Workstation ui(false); ui.stopTimer(); ui.setSize(1250,800);
        auto pump=[]
        {
            const auto until=juce::Time::getMillisecondCounterHiRes()+40;
            do
            {
               #if JUCE_WINDOWS
                MSG message; while (PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
               #endif
                juce::Thread::sleep(1);
            } while (juce::Time::getMillisecondCounterHiRes()<until);
        };
        auto complete=[&](int result)
        {
            auto* modal=juce::Component::getCurrentlyModalComponent();
            check(modal!=nullptr,"owned private popup/dialog exists before completion");
            if (modal) { modal->exitModalState(result); pump(); }
        };
        std::vector<std::shared_ptr<Media>> sources;
        for (int i=0;i<3;++i)
        {
            auto media=std::make_shared<Media>(); media->sampleRate=8000; media->name="Private fixture "+juce::String(i+1);
            media->file=report.getSiblingFile("private-"+juce::String(i)+".wav"); media->samples.setSize(2,4000); media->samples.clear();
            { juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream=media->file.createOutputStream();
              auto writer=wav.createWriterFor(stream,juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(2).withBitsPerSample(24));
              check(writer && writer->writeFromAudioSampleBuffer(media->samples,0,4000),"write isolated private fixture "+juce::String(i)); }
            media->analysis.bpm=120; media->analysis.tempoConfidence=1; media->analysis.key=0; media->analysis.keyConfidence=1; media->analysis.downbeatOrigin=0;
            media->analysisSource={audioDigest(media->file),media->file.getSize(),8000,4000,2};
            check(writeAnalysisRecord(media->file,media->analysisRecord()).isEmpty(),"isolated private fixture analysis "+juce::String(i)); sources.push_back(media);
        }
        Clip a,b; a.media=sources[0]; b.media=sources[1]; a.length=b.length=.5; b.track=1;
        ui.engine.library=sources; ui.engine.project.clips={a,b}; ui.engine.prepareRender(8000,128); ui.engine.prepareClipProcessing();
        auto reset=[&]
        {
            juce::ModalComponentManager::getInstance()->cancelAllModalComponents(); pump(); ui.libraryPreview.stop(); ui.privatePreparation.discard();
            ui.busy=false; ui.engine.exporting=false; ui.engine.setWorkMode(WorkMode::recordPerform); ui.engine.selectedTrack=0;
            ui.selectedClip=a.id; ui.selectedLibraryId=sources[2]->id; ui.layout.privateLevel=.5f; ui.libraryPreview.setLevel(.5f);
            ui.undoHistory.clear(); ui.redoHistory.clear(); ui.refresh();
        };
        reset(); const auto programme=juce::JSON::toString(ui.engine.captureProject()); ui.showPrivatePreparationMenu(); complete(2);
        check(ui.privatePreparation.target()==a.id && juce::JSON::toString(ui.engine.captureProject())==programme,"unchanged private popup prepares its target without programme edits");
        reset(); ui.showPrivatePreparationMenu(); ui.selectedClip=b.id; complete(2);
        check(ui.privatePreparation.target()==a.id && ui.selectedClip==b.id,"private popup retains its original clip without retargeting programme selection");
        reset(); ui.showPrivatePreparationMenu(); ui.busy=true; complete(2); ui.busy=false;
        check(!ui.privatePreparation.ready(),"private popup rechecks busy admission");
        reset(); ui.showPrivatePreparationMenu(); ui.engine.exporting=true; complete(2); ui.engine.exporting=false;
        check(!ui.privatePreparation.ready(),"private popup rechecks export admission");
        reset(); ui.showPrivatePreparationMenu(); ui.engine.setWorkMode(WorkMode::arrange); complete(2);
        check(!ui.privatePreparation.ready(),"private popup cannot prepare after leaving Record/Perform");
        reset(); ui.privatePreparation.prepareTrack(a.id); ui.showPrivatePreparationMenu(); ui.privatePreparation.prepareTrack(b.id); complete(22);
        check(ui.privatePreparation.target()==b.id && ui.privatePreparation.ready(),"stale private discard cannot remove a newer preparation");
        reset(); ui.privatePreparation.prepareTrack(a.id); ui.showPrivatePreparationMenu(); complete(6);
        auto* dialog=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        if (dialog) { dialog->getTextEditor("position")->setText(".25"); dialog->getTextEditor("level")->setText(".25"); }
        ui.privatePreparation.prepareTrack(b.id); complete(1);
        check(ui.privatePreparation.target()==b.id && ui.privatePreparation.cursor()==0 && ui.layout.privateLevel==.5f,"stale private transport draft keeps newer preparation and headphone level");
        reset(); ui.privatePreparation.prepareTrack(a.id); ui.showPrivatePreparationMenu(); complete(7);
        dialog=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        if (dialog) { dialog->getComboBoxComponent("parameter")->setSelectedItemIndex(0); dialog->getTextEditor("value")->setText("-6"); }
        ui.privatePreparation.prepareTrack(a.id); complete(1);
        auto* prepared=ui.privatePreparation.privateEngine();
        check(prepared && prepared->project.effectSettings(prepared->project.clips[0]).gainDb==0,"same-source replacement invalidates an older private control draft");
        reset(); ui.privatePreparation.prepareTrack(a.id); ui.showPrivatePreparationMenu(); complete(7);
        dialog=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        if (dialog) { dialog->getComboBoxComponent("parameter")->setSelectedItemIndex(0); dialog->getTextEditor("value")->setText("-6"); }
        complete(1); prepared=ui.privatePreparation.privateEngine();
        check(prepared && prepared->project.effectSettings(prepared->project.clips[0]).gainDb==-6 && ui.engine.project.effectSettings(ui.engine.project.clips[0]).gainDb==0,"unchanged private control draft edits private gain only");
        auto cold=sources[2]; cold->samples.setSize(0,0); cold->cache.reset();
        for (int scenario=0;scenario<4;++scenario)
        {
            reset(); cold->cache.reset(); juce::WaitableEvent started,release;
            ui.workers.addJob([&] { started.signal(); release.wait(4000); });
            check(started.wait(1000),"gate one owned worker for deterministic private completion");
            ui.showPrivatePreparationMenu(); complete(8);
            if (scenario==0) ui.busy=true;
            if (scenario==1) { ui.selectedClip=b.id; ui.showPrivatePreparationMenu(); complete(2); ui.showPrivatePreparationMenu(); complete(22); }
            if (scenario==2) ui.engine.selectedTrack=1;
            release.signal();
            const auto deadline=juce::Time::getMillisecondCounterHiRes()+3000;
            while (ui.workers.getNumJobs()>0 && juce::Time::getMillisecondCounterHiRes()<deadline) pump(); pump();
            check(ui.workers.getNumJobs()==0,"owned private worker finishes without analysis/model run");
            if (scenario==0) { check(!ui.privatePreparation.ready() && !cold->hasPlayback(),"busy private completion cannot publish cache or preparation"); ui.busy=false; }
            if (scenario==1) check(!ui.privatePreparation.ready(),"late library request cannot undo newer prepare then discard");
            if (scenario==2) check(!ui.privatePreparation.ready() && ui.engine.selectedTrack==1,"late library completion cannot retarget a changed programme lane");
            if (scenario==3) check(ui.privatePreparation.target()==cold->id && cold->original && !cold->cache && cold->samples.getNumSamples()==0,
                "unchanged library request completes with verified original-file playback and no PCM copy");
        }
        reset(); ui.privatePreparation.prepareTrack(a.id); ui.showPrivatePreparationMenu();
        auto* modal=juce::Component::getCurrentlyModalComponent(); auto stream=report.getSiblingFile("private-popup.png").createOutputStream(); juce::PNGImageFormat png;
        check(modal && stream && png.writeImageToStream(modal->createComponentSnapshot(modal->getLocalBounds()),*stream),"capture owned private popup"); complete(0);
        ui.privatePreparation.discard(); return;
    }
    if (juce::JUCEApplicationBase::getCommandLineParameters().contains("--performance-take-ui-only"))
    {
        Workstation ui(false); ui.stopTimer(); ui.setSize(1250,800);
        auto pump=[]
        {
            const auto until=juce::Time::getMillisecondCounterHiRes()+40;
            do
            {
               #if JUCE_WINDOWS
                MSG message; while (PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
               #endif
                juce::Thread::sleep(1);
            } while (juce::Time::getMillisecondCounterHiRes()<until);
        };
        auto close=[&] { juce::ModalComponentManager::getInstance()->cancelAllModalComponents(); pump(); };
        auto complete=[&](int result)
        {
            auto* modal=juce::Component::getCurrentlyModalComponent();
            check(modal!=nullptr,"owned native popup/dialog exists before completion");
            if (modal) { modal->exitModalState(result); pump(); }
        };
        auto image=[&](juce::Component* component,const juce::String& name)
        {
            auto stream=report.getSiblingFile(name+".png").createOutputStream(); juce::PNGImageFormat png;
            check(component && stream && png.writeImageToStream(component->createComponentSnapshot(component->getLocalBounds()),*stream),"capture owned "+name);
        };
        PerformanceTake a,b; a.name="Take A"; b.name="Take B"; a.initial=b.initial=projectToJson(Project());
        a.sampleRate=b.sampleRate=8000; a.frames=b.frames=8000; a.state=b.state=TakeState::complete;
        TakeEvent event; event.kind=TakeEventKind::control; event.value=0; a.events={event}; event.value=-6; b.events={event};
        auto reset=[&](std::vector<PerformanceTake> takes)
        {
            close(); ui.engine.project=Project(); ui.engine.project.performanceTakes=std::move(takes);
            ui.undoHistory.clear(); ui.redoHistory.clear(); ui.busy=false; ui.engine.exporting=false; ui.dirty=false; ui.refresh();
        };
        auto undoTarget=[&](std::vector<PerformanceTake> before,std::vector<PerformanceTake> after)
        { reset(std::move(after)); ui.remember(); ui.engine.project.performanceTakes=std::move(before); ui.refresh(); };
        reset({a,b}); ui.showTakeMenu(); image(juce::Component::getCurrentlyModalComponent(),"take-popup"); complete(103);
        check(ui.engine.project.performanceTakes.size()==1 && ui.engine.project.performanceTakes[0].id==b.id,"unchanged popup discards its intended take");
        ui.undo(false); check(ui.engine.project.performanceTakes.size()==2 && ui.engine.project.performanceTakes[0].id==a.id,"Undo restores exactly the discarded take");

        undoTarget({a,b},{b}); ui.showTakeMenu(); ui.undo(false); complete(103);
        check(ui.engine.project.performanceTakes.size()==1 && ui.engine.project.performanceTakes[0].id==b.id && ui.undoHistory.empty(),"removed popup target cannot discard the replacement take or add Undo");
        undoTarget({a,b},{b,a}); ui.showTakeMenu(); ui.undo(false); complete(103);
        check(ui.engine.project.performanceTakes.size()==1 && ui.engine.project.performanceTakes[0].id==b.id,"reordered popup still discards the original stable take");

        undoTarget({a,b},{b}); ui.editTakeEvent(0,-1); ui.undo(false); complete(1);
        check(!juce::Component::getCurrentlyModalComponent() && ui.engine.project.performanceTakes[0].id==b.id,"stale event-number dialog cannot open the replacement take editor"); close();
        undoTarget({a,b},{b,a}); ui.editTakeEvent(0,-1); ui.undo(false); complete(1);
        auto* editor=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        check(editor && editor->getTextEditor("value") && editor->getTextEditorContents("value").getDoubleValue()==0,"reordered event-number dialog retains original take identity");
        if (editor) image(editor,"take-event-editor"); close();

        auto changed=a; changed.events[0].value=-12;
        undoTarget({a},{changed}); ui.editTakeEvent(0,0); ui.undo(false);
        editor=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent()); if (editor) editor->getTextEditor("value")->setText("-3"); complete(1);
        check(ui.engine.project.performanceTakes[0].events[0].value==-12 && ui.undoHistory.empty(),"stale final event editor cannot overwrite a changed chronology");
        reset({a}); ui.editTakeEvent(0,0); editor=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        if (editor) editor->getTextEditor("value")->setText("-3"); complete(1);
        check(ui.engine.project.performanceTakes[0].events[0].value==-3 && ui.undoHistory.size()==1,"unchanged event editor commits one Undo step");
        ui.undo(false); check(ui.engine.project.performanceTakes[0].events[0].value==0,"Undo restores event value");
        reset({a}); ui.editTakeEvent(0,0); complete(0);
        check(ui.engine.project.performanceTakes[0].events[0].value==0 && ui.undoHistory.empty(),"cancelled event editor keeps take and history");
        reset({a,b}); ui.showTakeMenu(); ui.engine.exporting=true; complete(103); ui.engine.exporting=false;
        check(ui.engine.project.performanceTakes.size()==2 && ui.undoHistory.empty(),"popup callback rechecks exclusive export admission");

        // One tiny owned source permits a genuine recording-state transition, with
        // no device, model or production library. Never render an invalid red state.
        reset({a,b}); auto media=std::make_shared<Media>(); media->sampleRate=8000; media->name="Popup fixture";
        media->file=report.getSiblingFile("popup-fixture.wav"); media->samples.setSize(2,8000); media->samples.clear();
        { juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream=media->file.createOutputStream();
          auto writer=wav.createWriterFor(stream,juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(2).withBitsPerSample(24));
          check(writer && writer->writeFromAudioSampleBuffer(media->samples,0,8000),"write tiny owned capture-state source"); }
        media->analysis.bpm=120; media->analysis.downbeatOrigin=0;
        media->analysisSource={audioDigest(media->file),media->file.getSize(),8000,8000,2};
        check(writeAnalysisRecord(media->file,media->analysisRecord()).isEmpty(),"owned capture fixture reuses its isolated analysis");
        Clip clip; clip.media=media; clip.length=1; ui.engine.library={media}; ui.engine.project.clips={clip};
        ui.engine.prepareRender(8000,128); ui.engine.prepareClipProcessing(); ui.engine.setWorkMode(WorkMode::recordPerform);
        ui.showTakeMenu(); ui.engine.setPlaying(true); check(ui.engine.capturingPerformance(),"supported Play starts a take while the earlier popup is pending");
        // A test-only sentinel keeps the deliberately broken red callback in bounds.
        // The supported Play transition and real menu callback remain unchanged.
        auto sentinel=b; sentinel.id=juce::Uuid().toString(); ui.engine.project.performanceTakes.push_back(sentinel);
        const auto captured=ui.engine.project.performanceTakes; complete(103);
        check(ui.engine.project.performanceTakes.size()==captured.size() && ui.engine.project.performanceTakes[0].id==a.id
              && ui.engine.capturingPerformance(),"stale discard cannot erase a take while recording or invalidate active capture index");
        // Restore only this owned fixture before stopping if the red callback broke
        // its vector. Production state is never referenced by these checks.
        ui.engine.project.performanceTakes=captured; ui.engine.performanceProjectPending=false; ui.engine.stop();
        ui.engine.setWorkMode(WorkMode::arrange); ui.engine.recordMasterWav=true; ui.showTakeMenu(); ui.engine.setPlaying(true); complete(3);
        check(ui.engine.recordMasterWav && ui.engine.playing,"master-WAV popup choice rechecks stopped-transport admission");
        ui.engine.stop(); ui.engine.recordMasterWav=false; close();
        return;
    }
    runPerformanceTakeParserTests(check);
    const int rate=8000,block=128,frames=rate*2;
    auto media=std::make_shared<Media>(); media->sampleRate=rate; media->file=report.getSiblingFile("performance.wav"); media->name="Performance fixture";
    media->samples.setSize(2,frames);
    for (int n=0;n<frames;++n) for (int ch=0;ch<2;++ch) media->samples.setSample(ch,n,(float)(.2*std::sin(n*.137)));
    { juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream=media->file.createOutputStream(); auto writer=wav.createWriterFor(stream,juce::AudioFormatWriterOptions().withSampleRate(rate).withNumChannels(2).withBitsPerSample(24)); check(writer && writer->writeFromAudioSampleBuffer(media->samples,0,frames),"write tiny deterministic performance source"); }
    media->analysis.bpm=120; media->analysis.tempoConfidence=1; media->analysis.key=0; media->analysis.keyConfidence=1; media->analysis.downbeatOrigin=0;
    media->analysisSource={audioDigest(media->file),media->file.getSize(),rate,frames,2}; media->peaks.resize(4000,.2f);
    check(writeAnalysisRecord(media->file,media->analysisRecord()).isEmpty(),"isolated performance source reuses analysis on reopen");
    Engine engine({},false); engine.prepareRender(rate,block); engine.project.masterDb=0; engine.library={media};
    Clip clip; clip.media=media; clip.length=2; engine.project.clips={clip}; engine.prepareClipProcessing(); engine.selectedClip=clip.id;
    {
        Engine capture({},false); capture.prepareRender(rate,block); capture.library={media}; capture.project.clips={clip}; capture.prepareClipProcessing();
        const auto started=capture.beginPerformance();
        check(started.wasOk(),"start isolated synthetic-audio capture for the snapshot admission guard: "+started.getErrorMessage());
        if (started.wasOk())
        {
            TakeEvent checkpoint; checkpoint.kind=TakeEventKind::project; checkpoint.payload=capture.project.performanceTakes.back().initial;
            for (int n=0;n<65;++n) capture.appendTakeEvent(checkpoint);
            TakeEvent control; control.kind=TakeEventKind::control; control.value=-6; capture.appendTakeEvent(control);
            const auto& retained=capture.project.performanceTakes.back();
            check(retained.events.size()==65 && retained.events.back().kind==TakeEventKind::control
                && !retained.complete && retained.issue.contains("Structural checkpoint capacity") && capture.capturingPerformance(),
                "all capture routes cap structural snapshots at 64, flag incomplete and continue ordinary events");
            capture.stop();
        }
    }
    const auto original=juce::JSON::toString(engine.captureProject());
    check(engine.setWorkMode(WorkMode::recordPerform).wasOk() && !engine.playing && engine.automationArmed[0] && engine.masterAutomationArmed,"Record/Perform enables all capture owners without starting playback");
    engine.recordMasterWav=true; engine.masterRecordingDestination=report.getSiblingFile("live-master.wav");
    engine.setPlaying(true); check(engine.capturingPerformance(),"Play begins independent editable take");
    std::vector<float> heard;
    auto callback=[&](int count)
    {
        juce::AudioBuffer<float> output(2,block); float* channels[]{output.getWritePointer(0),output.getWritePointer(1)};
        for (int b=0;b<count;++b) { engine.audioDeviceIOCallbackWithContext(nullptr,0,channels,2,block,{}); for (int n=0;n<block;++n) heard.push_back(output.getSample(0,n)); }
    };
    callback(3); engine.setControlValue({0,AutomationTarget::volume},-12); callback(2);
    engine.seek(0); callback(2); engine.setControlValue({0,AutomationTarget::volume},-6); callback(2);
    engine.setPlaying(false); callback(1); engine.setPlaying(true); callback(2); engine.stop();
    check(!engine.capturingPerformance() && engine.project.performanceTakes.size()==1 && engine.project.performanceTakes[0].state==TakeState::complete,"Stop finalises one take before resetting programme");
    check(engine.project.effectSettings(engine.project.clips[0]).gainDb==0 && engine.project.effectSettings(engine.project.clips[0]).automation[0].points.empty(),"recording leaves original arrangement controls independently unchanged");
    const auto take=engine.project.performanceTakes[0];
    check(take.frames==(int64_t)heard.size() && take.events.size()>=6 && take.complete,"monotonic performance clock includes repeated passage and pause silence / "+juce::String(take.frames)+" frames / "+juce::String((int)take.events.size())+" events / "+take.issue);
    bool ordered=true; for (size_t i=1;i<take.events.size();++i) ordered &= take.events[i].frame>=take.events[i-1].frame && take.events[i].sequence>take.events[i-1].sequence;
    check(ordered,"event ties retain deterministic commit order through backwards source seek");
    juce::String error; auto replay=engine.prepareTakeReplay(0,error); check(replay && error.isEmpty(),"prepare independent take replay: "+error);
    double maximum=0;
    if (replay)
    {
        juce::AudioBuffer<float> output(2,block);
        for (int at=0;at<(int)heard.size();at+=block) { replay->renderTake(output,block,at/(double)rate); for (int n=0;n<block;++n) maximum=std::max(maximum,std::abs((double)output.getSample(0,n)-heard[(size_t)at+n])); }
    }
    check(replay && maximum<1.e-6,"captured/replayed fader changes, source jump, repeated passage and pause PCM agree: "+juce::String(maximum,9));
    auto seeked=engine.prepareTakeReplay(0,error); juce::AudioBuffer<float> sought(2,64);
    if (seeked) seeked->renderTake(sought,64,640/(double)rate);
    double seekError=0; if (seeked) for (int n=0;n<64;++n) seekError=std::max(seekError,std::abs((double)sought.getSample(0,n)-heard[(size_t)640+n]));
    check(seeked && seekError<1.e-6,"seeking reconstructs preceding controls and source-jump history before rendering");
    std::unique_ptr<juce::AudioFormatReader> live(engine.formats.createReaderFor(engine.masterRecordingDestination));
    check(live && live->lengthInSamples==(int64_t)heard.size(),"bounded live writer finalises the exact captured frame count");
    Engine reopened({},false); reopened.prepareRender(rate,block); const auto saved=juce::JSON::toString(engine.captureProject());
    check(reopened.restoreProject(saved).isEmpty() && reopened.project.performanceTakes.size()==1,"current session saves/reopens both arrangement and editable chronology");
    auto exportTake=reopened.prepareTakeReplay(0,error);
    if (exportTake) { const auto written=exportTake->writeWav(report.getSiblingFile("take-export.wav"),{}); check(written.wasOk() && written.writtenFrames==take.frames,"offline take WAV uses the same event renderer and duration: "+written.error); }
    else check(false,"prepare reopened take export: "+error);
    PrivatePreparation privateCopy(engine); const auto before=juce::JSON::toString(engine.captureProject());
    check(privateCopy.prepareTrack(clip.id).wasOk(),"private track preparation owns independent renderer");
    check(privateCopy.setControl({0,AutomationTarget::volume,{},{},clip.id},-24).wasOk() && juce::JSON::toString(engine.captureProject())==before,"private fader edit cannot mutate programme, playhead or take");
    privateCopy.prepareToPlay(block,rate); juce::AudioBuffer<float> privatePcm(2,block); privateCopy.getNextAudioBlock({&privatePcm,0,block});
    check(privatePcm.getMagnitude(0,block)>.001f && engine.position==0,"private playback remains independent of stopped programme");
    check(privateCopy.apply(false).wasOk() && engine.project.effectSettings(engine.project.clips[0]).gainDb==-24,"explicit Apply commits the prepared owner");
    engine.project.clips[0].length=1; check(privateCopy.apply(true).failed(),"stale private source geometry rejects Apply/Launch");
    auto malformed=takeToJson(take); malformed.getDynamicObject()->setProperty("initial",engine.captureProject()); PerformanceTake rejected;
    check(readTake(malformed,rejected).failed(),"nested take snapshots cannot recursively expand session parsing");
    {
        Engine cue({},false); cue.prepareRender(rate,block); cue.library={media}; cue.project.clips={clip}; cue.project.masterDb=0; cue.project.launch.timing=LaunchTiming::nextBeat;
        auto* volume=cue.project.curve({0,AutomationTarget::volume,{},{},clip.id}); volume->enabled=true; volume->setPoint(0,0); volume->setPoint(2,-6);
        check(cue.project.putHotcue(clip.id,0,0).wasOk(),"save a real source cue for the ending-boundary performance fixture");
        cue.prepareClipProcessing(); cue.setWorkMode(WorkMode::recordPerform); cue.setPlaying(true); std::vector<float> heardCue;
        auto step=[&](int count) { juce::AudioBuffer<float> pcm(2,block); float* channels[]{pcm.getWritePointer(0),pcm.getWritePointer(1)}; for (int n=0;n<count;++n) { cue.audioDeviceIOCallbackWithContext(nullptr,0,channels,2,block,{}); for (int i=0;i<block;++i) heardCue.push_back(pcm.getSample(0,i)); } };
        step(124); const auto requested=cue.triggerLiveHotcue(clip.id,0); check(requested.wasOk(),"a hotcue can queue during an editable take: "+requested.getErrorMessage()); step(8);
        check(cue.playing && cue.position>2 && cue.liveHotcueStatus(clip.id).active,"a cue at the old arrangement ending extends audible programme playback without an ending gap"); cue.setWorkMode(WorkMode::arrange);
        check(!cue.capturingPerformance() && !cue.playing && cue.position==0,"leaving Record/Perform finalises the take and returns an extended cue cursor to playable arrangement bounds");
        const auto& captured=cue.project.performanceTakes[0]; const auto jump=std::find_if(captured.events.begin(),captured.events.end(),[](const auto& e) { return e.kind==TakeEventKind::clipJump; });
        check(jump!=captured.events.end() && jump->frame==16000,"hotcue chronology records its audible frame boundary rather than its request time");
        auto render=cue.prepareTakeReplay(0,error); double delta=0;
        if (render) { juce::AudioBuffer<float> pcm(2,block); for (int at=0;at<(int)heardCue.size();at+=block) { render->renderTake(pcm,block,at/(double)rate); for (int i=0;i<block;++i) delta=std::max(delta,std::abs((double)pcm.getSample(0,i)-heardCue[(size_t)at+i])); } }
        check(render && delta<1.e-6,"audible hotcue/source repetition and original-track automation replay identically through the old ending: "+error+" / "+juce::String(delta,9));
    }
    {
        Engine launch({},false); launch.prepareRender(rate,block); launch.library={media}; launch.project.clips={clip}; launch.project.masterDb=0; launch.project.launch.timing=LaunchTiming::nextBeat; launch.prepareClipProcessing();
        PrivatePreparation preparation(launch); check(preparation.prepareTrack(clip.id).wasOk() && preparation.setControl({0,AutomationTarget::volume,{},{},clip.id},-18).wasOk(),"prepare an independent private control treatment for explicit launch"); preparation.seek(.1);
        launch.setWorkMode(WorkMode::recordPerform); launch.setPlaying(true); check(preparation.apply(true).wasOk(),"private edits queue explicitly for the next programme beat");
        std::vector<float> heardLaunch; juce::AudioBuffer<float> pcm(2,block); float* channels[]{pcm.getWritePointer(0),pcm.getWritePointer(1)};
        for (int n=0;n<40;++n) { launch.audioDeviceIOCallbackWithContext(nullptr,0,channels,2,block,{}); for (int i=0;i<block;++i) heardLaunch.push_back(pcm.getSample(0,i)); } launch.stop();
        auto render=launch.prepareTakeReplay(0,error); double delta=0;
        if (render) for (int at=0;at<(int)heardLaunch.size();at+=block) { render->renderTake(pcm,block,at/(double)rate); for (int i=0;i<block;++i) delta=std::max(delta,std::abs((double)pcm.getSample(0,i)-heardLaunch[(size_t)at+i])); }
        check(render && delta<1.e-6,"private launch snapshot, cue clock and take events share the audible commit path: "+error+" / "+juce::String(delta,9));
    }
    {
        Engine failure({},false); failure.prepareRender(rate,block); failure.library={media}; failure.project.clips={clip}; failure.project.masterDb=0; failure.prepareClipProcessing();
        failure.setWorkMode(WorkMode::recordPerform); failure.setPlaying(true);
        check(failure.masterRecorder.start(report.getSiblingFile("incomplete-master.wav"),rate,32).wasOk(),"open a tiny bounded writer failure fixture");
        juce::AudioBuffer<float> output(2,block); float* channels[]{output.getWritePointer(0),output.getWritePointer(1)};
        failure.audioDeviceIOCallbackWithContext(nullptr,0,channels,2,block,{});
        check(failure.playing && failure.masterRecorder.failed() && output.getMagnitude(0,block)>.01f,"FIFO refusal reports failure while programme audio and editable capture continue");
        failure.stop(); check(!failure.project.performanceTakes[0].complete && failure.project.performanceTakes[0].issue.contains("Incomplete master WAV"),"failed writer retains partial WAV and marks the take visibly incomplete");
    }
    {
        Engine routing({},false); routing.devices.addAudioDeviceType(std::make_unique<FourChannelType>());
        check(routing.devices.initialise(0,2,nullptr,false,"Interface").isEmpty(),"open only fake four-channel interface");
        routing.prepareRender(rate,block); routing.library={media}; routing.project.clips={clip}; routing.project.masterDb=0; routing.prepareClipProcessing();
        LibraryPreview preview(routing); preview.inventory.addAudioDeviceType(std::make_unique<FourChannelType>()); preview.setPrivateOutput(true); PrivateConstant privateAudio;
        check(preview.playPrepared(privateAudio,"private").failed() && preview.select({"Routing fixture","Interface",0}).failed(),"unconfigured or overlapping private routes cannot use audience channels");
        check(preview.select({"Routing fixture","Interface",2}).wasOk(),"explicit independent stereo pair is configured while stopped");
        preview.setLevel(.25f); check(preview.playPrepared(privateAudio,"private").wasOk(),"private preparation attaches only to channels 3-4");
        routing.setWorkMode(WorkMode::recordPerform); routing.recordMasterWav=true; routing.masterRecordingDestination=report.getSiblingFile("routed-master.wav"); routing.setPlaying(true); routing.monitorMuted=true;
        juce::AudioBuffer<float> output(4,block); float* channels[]{output.getWritePointer(0),output.getWritePointer(1),output.getWritePointer(2),output.getWritePointer(3)};
        routing.audioDeviceIOCallbackWithContext(nullptr,0,channels,4,block,{});
        check(output.getMagnitude(0,0,block)==0 && std::abs(output.getSample(2,20)-.25f)<1.e-7f && std::abs(output.getSample(3,20)-.25f)<1.e-7f,"audience monitor mute and private pair remain independent with no channel leakage");
        routing.stop(); preview.stop(); std::unique_ptr<juce::AudioFormatReader> recorded(routing.formats.createReaderFor(routing.masterRecordingDestination));
        juce::AudioBuffer<float> captured(2,block); const bool read=recorded && recorded->read(&captured,0,block,0,true,true);
        check(read && captured.getMagnitude(0,block)>.19f && captured.getMagnitude(0,block)<.21f,"master WAV taps programme before monitor mute and excludes the louder private signal");
    }
    {
        const auto args=juce::StringArray::fromTokens(juce::JUCEApplicationBase::getCommandLineParameters(),true); const int at=args.indexOf("--plugin");
        check(at>=0 && at+1<args.size(),"finite VST fixture supplied for continuous shared automation");
        if (at>=0 && at+1<args.size())
        {
            Engine fx({},false); fx.prepareRender(rate,block); fx.library={media}; fx.project.masterDb=0; auto source=clip; source.length=.5; fx.project.clips={source}; fx.selectedClip=source.id;
            const auto loaded=fx.addPlugin(0,juce::File(args[at+1].unquoted())); check(loaded.isEmpty(),"load existing real finite-delay VST: "+loaded);
            if (loaded.isEmpty())
            {
                for (auto target:{AutomationTarget::volume,AutomationTarget::bass,AutomationTarget::cutoff}) { auto* curve=fx.project.curve({0,target,{},{},source.id}); curve->enabled=true; curve->setPoint(0,target==AutomationTarget::cutoff ? -.1f : -3.f); curve->setPoint(.8,target==AutomationTarget::cutoff ? -.5f : -9.f); }
                auto parameter=fx.pluginAddress(0,0,0); if (parameter && fx.ensurePluginAutomation(*parameter)) { auto* curve=fx.project.curve(*parameter); curve->enabled=true; curve->setPoint(0,.25f); curve->setPoint(.8,.9f); }
                auto render=[&](int chunk)
                { fx.prepareClipProcessing(); fx.resetPlugins(); juce::AudioBuffer<float> result(2,7200),pcm(2,block); juce::MidiBuffer midi; for (int n=0;n<7200;n+=chunk) { const int count=std::min(chunk,7200-n); fx.render(pcm,count,n/(double)rate,midi); for (int ch=0;ch<2;++ch) result.copyFrom(ch,n,pcm,ch,0,count); } return result; };
                const auto intact=render(block); const auto whole=fx.project;
                const auto oddBlocks=render(113); double blockError=0; for (int n=0;n<7200;++n) blockError=std::max(blockError,std::abs((double)intact.getSample(0,n)-oddBlocks.getSample(0,n)));
                check(blockError<1.e-6,"shared automation and finite tails use the same sample clock across different callback sizes: "+juce::String(blockError,9));
                auto first=source.slice(0,.2),last=source.slice(.2,.3); last.id=juce::Uuid().toString(); fx.project.clips={first,last};
                const auto split=render(block); double splitError=0; for (int n=0;n<7200;++n) splitError=std::max(splitError,std::abs((double)intact.getSample(0,n)-split.getSample(0,n)));
                check(splitError<1.e-6,"shared Volume/EQ/Filter/VST curves preserve split and unsplit PCM including natural tail: "+juce::String(splitError,9));
                auto a=AutomationAddress{0,AutomationTarget::volume,{},{},first.id}; auto b=a; b.clipId=last.id;
                check(fx.project.curve(a)==fx.project.curve(b) && fx.project.automationBounds(b).getEnd()>.5,"all pieces edit one owner with automation extending through its effect tail");
                fx.project.clips[0]=source.slice(0,.1); fx.project.clips[1]=source.slice(.3,.2); fx.project.clips[1].id=last.id;
                const auto gap=render(block); check(gap.getMagnitude(0,1600,600)>.001f,"finite VST state continues through a deleted source gap");
                auto duplicate=last; duplicate.id=juce::Uuid().toString(); const auto copied=fx.cloneClipEffects(duplicate); check(copied.isEmpty() && duplicate.effects!=last.effects,"independent placements clone the original controls and processors");
                auto saved=fx.captureProject(); Engine roundtrip({},false); roundtrip.prepareRender(rate,block); const auto opened=roundtrip.restoreProject(juce::JSON::toString(saved));
                check(opened.isEmpty() && roundtrip.project.clips.size()==2 && effectsIdentity(roundtrip.project.clips[0].effects)==effectsIdentity(source.effects)
                    && roundtrip.project.curve({0,AutomationTarget::volume,{},{},roundtrip.project.clips[0].id})==roundtrip.project.curve({0,AutomationTarget::volume,{},{},roundtrip.project.clips[1].id}),"original owner identity and built-in/VST sharing survive save/reopen: "+opened);
                fx.setWorkMode(WorkMode::recordPerform); fx.setPlaying(true);
                auto* liveParameter=fx.project.clips[0].effects->plugins[0]->processor->getParameters()[0];
                liveParameter->beginChangeGesture(); liveParameter->setValueNotifyingHost(.7f); liveParameter->endChangeGesture();
                std::vector<float> liveFx;
                auto step=[&](int count)
                { juce::AudioBuffer<float> pcm(2,block); float* channels[]{pcm.getWritePointer(0),pcm.getWritePointer(1)}; for (int n=0;n<count;++n) { fx.audioDeviceIOCallbackWithContext(nullptr,0,channels,2,block,{}); for (int i=0;i<block;++i) liveFx.push_back(pcm.getSample(0,i)); } };
                step(4); if (parameter) fx.returnToAutomation(*parameter); step(3);
                const auto pauseBegin=liveFx.size(); fx.setPlaying(false); step(3);
                double pauseTail=0; for (size_t i=pauseBegin;i<liveFx.size();++i) pauseTail=std::max(pauseTail,std::abs((double)liveFx[i]));
                check(pauseTail>1.e-5,"paused performance keeps finite effect tails audible while its source cursor holds");
                fx.setPlaying(true); step(2); fx.stop();
                const auto fxTake=fx.project.performanceTakes.back();
                check(std::any_of(fxTake.events.begin(),fxTake.events.end(),[](const auto& e) { return e.kind==TakeEventKind::control && e.control==(int)AutomationTarget::plugin; })
                    && std::any_of(fxTake.events.begin(),fxTake.events.end(),[](const auto& e) { return e.kind==TakeEventKind::returnAutomation; }),"direct VST editor-style gestures and explicit Return record deterministic punch boundaries");
                auto replayFx=fx.prepareTakeReplay((int)fx.project.performanceTakes.size()-1,error); double fxError=0;
                if (replayFx) { juce::AudioBuffer<float> pcm(2,block); for (int n=0;n<(int)liveFx.size();n+=block) { replayFx->renderTake(pcm,block,n/(double)rate); for (int i=0;i<block;++i) fxError=std::max(fxError,std::abs((double)pcm.getSample(0,i)-liveFx[(size_t)n+i])); } }
                check(replayFx && fxError<1.e-6,"finite VST gesture/Return capture and replay agree with continuous effect state: "+error+" / "+juce::String(fxError,9));
            }
        }
    }
    Workstation ui(false); ui.stopTimer(); ui.setSize(1440,900);
    auto capture=[&](const juce::String& name) { auto image=ui.createComponentSnapshot(ui.getLocalBounds()); juce::PNGImageFormat png; auto stream=report.getSiblingFile(name+".png").createOutputStream(); return stream && png.writeImageToStream(image,*stream); };
    check(capture("arrange-mode"),"capture actual Arrange controls"); ui.engine.setWorkMode(WorkMode::recordPerform); ui.workModeButton.setButtonText("Record/Perform");
    check(capture("record-perform-mode"),"capture actual Record/Perform controls");
    report.getSiblingFile("take-evidence.json").replaceWithText(juce::JSON::toString(takeToJson(take)));
}
}
