#include "Workstation.h"
#include "PrivatePreparation.h"
#include <cmath>
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
void runPerformanceTakeTests(const std::function<void(bool,const juce::String&)>& check,const juce::File& report)
{
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
