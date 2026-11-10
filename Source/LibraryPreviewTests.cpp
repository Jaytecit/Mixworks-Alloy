#include "Workstation.h"
#include "EarTools.h"
#include "TutorialMusic.h"
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
void pumpPreview(int ms)
{
    const auto end = juce::Time::getMillisecondCounterHiRes()+ms;
    do {
#if JUCE_WINDOWS
        MSG message; while (PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
#endif
        juce::Thread::sleep(1);
    } while (juce::Time::getMillisecondCounterHiRes() < end);
}
class PreviewDevice final : public juce::AudioIODevice
{
public:
    PreviewDevice(juce::String name, bool fail) : AudioIODevice(name,"Preview fixture"), failing(fail) {}
    juce::StringArray getOutputChannelNames() override { return {"Left","Right"}; }
    juce::StringArray getInputChannelNames() override { return {}; }
    juce::Array<double> getAvailableSampleRates() override { return {48000}; }
    juce::Array<int> getAvailableBufferSizes() override { return {256}; }
    int getDefaultBufferSize() override { return 256; }
    juce::String open(const juce::BigInteger&,const juce::BigInteger&,double,int) override { opened = !failing; return failing ? "Fixture refused" : ""; }
    void close() override { opened = false; }
    bool isOpen() override { return opened; }
    void start(juce::AudioIODeviceCallback* next) override { callback = next; if (callback) callback->audioDeviceAboutToStart(this); }
    void stop() override { if (callback) callback->audioDeviceStopped(); callback = nullptr; }
    bool isPlaying() override { return callback != nullptr; }
    juce::String getLastError() override { return {}; }
    int getCurrentBufferSizeSamples() override { return 256; }
    double getCurrentSampleRate() override { return 48000; }
    int getCurrentBitDepth() override { return 32; }
    juce::BigInteger getActiveOutputChannels() const override { return juce::BigInteger(3); }
    juce::BigInteger getActiveInputChannels() const override { return {}; }
    int getOutputLatencyInSamples() override { return 256; }
    int getInputLatencyInSamples() override { return 0; }
    bool opened = false, failing = false; juce::AudioIODeviceCallback* callback = nullptr;
};
class PreviewType final : public juce::AudioIODeviceType
{
public:
    PreviewType() : AudioIODeviceType("Preview fixture") {}
    void scanForDevices() override {}
    juce::StringArray getDeviceNames(bool = false) const override { return names; }
    int getDefaultDeviceIndex(bool) const override { return 0; }
    int getIndexOfDevice(juce::AudioIODevice* d,bool) const override { return names.indexOf(d->getName()); }
    bool hasSeparateInputsAndOutputs() const override { return true; }
    juce::AudioIODevice* createDevice(const juce::String& name,const juce::String&) override
    { ++created; last = new PreviewDevice(name,fail); return last; }
    juce::StringArray names {"Live speakers","Headphones"}; bool fail = false; int created = 0; PreviewDevice* last = nullptr;
};
}
void runLibraryPreviewTests(const std::function<void(bool,const juce::String&)>& check,const juce::File& report)
{
    Workstation ui(false); ui.stopTimer(); ui.setSize(1250,900);
    auto* mainType = new PreviewType; ui.engine.devices.addAudioDeviceType(std::unique_ptr<PreviewType>(mainType));
    check(ui.engine.devices.initialise(0,2,nullptr,false,"Live speakers").isEmpty(),"open only isolated main fixture"); pumpPreview(20);
    auto* cueType = new PreviewType; ui.libraryPreview.inventory.addAudioDeviceType(std::unique_ptr<PreviewType>(cueType));
    const auto file = report.getSiblingFile("preview-tone.wav");
    juce::AudioBuffer<float> tone(1,48000); for (int n=0;n<tone.getNumSamples();++n) tone.setSample(0,n,.25f);
    { juce::WavAudioFormat wav; std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(file.createOutputStream().release(),48000,1,16,{},0));
      check(writer && writer->writeFromAudioSampleBuffer(tone,0,tone.getNumSamples()),"create short preview fixture"); }
    auto media = std::make_shared<Media>(); media->file = file; media->name = "Library preview test";
    if(juce::JUCEApplicationBase::getCommandLineParameters().contains("--tutorial-demo-only"))
    {
        ui.workspacePreference=report.getSiblingFile("tutorial-preferences.json");
        ui.musicFolder=report.getSiblingFile("empty-music");ui.musicPreference=report.getSiblingFile("tutorial-scan.json");ui.catalogueFile=report.getSiblingFile("tutorial-library.json");
        const auto original=juce::JSON::toString(projectToJson(ui.engine.project));ui.showTutorial();
        const auto find=[](juce::Component& root,const juce::String& id)
        {std::function<juce::Component*(juce::Component&)> walk=[&](juce::Component& c)->juce::Component* {if(c.getComponentID()==id)return &c;for(auto* child:c.getChildren())if(auto* result=walk(*child))return result;return nullptr;};return walk(root);};
        auto* demo=ui.tutorialWindow ? dynamic_cast<juce::Button*>(find(*ui.tutorialWindow,"tutorial-demo")):nullptr;
        check(demo && demo->isVisible() && demo->isEnabled(),"first tutorial step offers optional original music alongside importing your own");if(!demo)return;
        auto settle=[&] {const auto end=juce::Time::getMillisecondCounter()+15000;while((ui.busy || !ui.timelineLoads.empty() || ui.tempoPreparationPending || ui.pendingTempo>0) && juce::Time::getMillisecondCounter()<end) {pumpPreview(10);ui.timerCallback();}pumpPreview(40);};
        ui.engine.playing=true;demo->onClick();check(!ui.busy && ui.engine.library.empty() && ui.engine.playing,"demo action refuses during playback without changing the running mix");ui.engine.playing=false;
        demo->onClick();demo->onClick();settle();
        check(!ui.busy && ui.engine.library.empty() && ui.status.containsIgnoreCase("cancel"),"demo preparation cancels through its actual button and can be retried");
        demo->onClick();check(ui.busy,"demo creation runs asynchronously through the existing single worker");settle();
        check(!ui.busy && ui.engine.library.size()==3,"actual optional-demo button generates and imports three real tracks: "+ui.status);
        if(ui.engine.library.size()!=3)return;
        check(original==juce::JSON::toString(projectToJson(ui.engine.project)) && ui.undoHistory.empty(),"optional demos populate only the library and preserve the existing arrangement/history");
        std::vector<juce::String> identities;juce::AudioFormatManager formats;formats.registerBasicFormats();
        for(const auto& source:ui.engine.library)
        {
            identities.push_back(audioDigest(source->file));std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(source->file));
            check(reader && reader->sampleRate==44100 && reader->numChannels==2 && reader->lengthInSamples==32*44100,"original demo is an actual 32-second stereo44.1kHz WAV / "+source->name);
            const auto cached=readAnalysisRecord(source->file,source->analysisSource);
            check(cached.reusableAutomatic && source->manualBpm==120 && source->manualBeatOrigin==0 && source->manualKey==0 && !Workstation::libraryNeedsReview(*source),"authored demo grid/key survives real import without claiming automatic detector confidence");
            check(reader && reader->metadataValues.getValue(juce::WavAudioFormat::riffInfoComment,{}).contains("No imported recordings"),"demo audio carries original-source provenance");
            bool finite=true;float peak=0;double squared=0;int measured=0;juce::AudioBuffer<float> audio(2,512);
            if(reader)for(int64_t offset=0;offset<reader->lengthInSamples;offset+=512)
            {const int count=(int)std::min<int64_t>(512,reader->lengthInSamples-offset);reader->read(&audio,0,count,offset,true,true);
                for(int ch=0;ch<2;++ch)for(int i=0;i<count;++i) {const float x=audio.getSample(ch,i);finite&=std::isfinite(x);peak=std::max(peak,std::abs(x));squared+=x*x;++measured;}}
            check(finite && peak>.1f && peak<.91f && measured>0 && std::sqrt(squared/std::max(1,measured))>.025,"independently decoded original demo has finite, audible, unclipped PCM / peak "+juce::String(peak,6));
        }
        const auto folder=ui.workspacePreference.getSiblingFile("TutorialAudio-v1");auto token=std::make_shared<std::atomic<bool>>(false);
        const auto reused=createTutorialMusic(folder,token);bool same=reused.files.size()==3 && reused.error.isEmpty();
        for(int i=0;same && i<3;++i)same=audioDigest(reused.files[i])==identities[(size_t)i];
        check(same,"reusing the optional music keeps all original demo audio byte-for-byte and creates no duplicates");
        const auto source=ui.engine.library[0];auto correction=source->analysisRecord();correction.manualBeatOrigin=.025;
        const auto changed=writeAnalysisRecord(source->file,correction);createTutorialMusic(folder,token);
        const auto preserved=readAnalysisRecord(source->file,source->analysisSource);
        check(changed.isEmpty() && preserved.record && preserved.record->manualBeatOrigin==.025,"demo reuse preserves later owner-reviewed grid corrections");
        const auto unknown=report.getSiblingFile("unrecognised-demo");unknown.createDirectory();unknown.getChildFile("keep.txt").replaceWithText("keep");
        const auto refused=createTutorialMusic(unknown,token);
        check(refused.files.isEmpty() && refused.error.isNotEmpty() && unknown.getChildFile("keep.txt").loadFileAsString()=="keep","unknown existing demo folder is refused without adopting, overwriting or removing files");
        ui.previewLibraryTrack(source->id,.65);pumpPreview(180);juce::AudioBuffer<float> heard(2,256);float previewPeak=0;
        for(int n=0;n<8;++n) {ui.engine.audioDeviceIOCallbackWithContext(nullptr,0,heard.getArrayOfWritePointers(),2,256,{});previewPeak=std::max(previewPeak,heard.getMagnitude(0,256));}
        check(ui.libraryPreview.active(source->id) && previewPeak>.001f,"optional tutorial track auditions a later passage through the actual isolated master callback");ui.libraryPreview.stop();
        auto capture=report.getSiblingFile("tutorial-original-music.png").createOutputStream();
        check(capture && juce::PNGImageFormat().writeImageToStream(ui.tutorialWindow->createComponentSnapshot(ui.tutorialWindow->getLocalBounds()),*capture),"capture readable first-use guide with original-demo action");capture.reset();
        ui.addMedia(0);settle();check(!ui.busy && ui.engine.project.clips.size()==1,"new user can place a generated track through the ordinary Add workflow");
        if(!ui.engine.project.clips.empty())
        {
            juce::String error;auto renderer=ui.engine.createExportRenderer(error);float peak=0;
            if(renderer)for(double time:{2.,12.,26.}) {juce::AudioBuffer<float> output(2,256);juce::MidiBuffer midi;renderer->render(output,256,time,midi);peak=std::max(peak,output.getMagnitude(0,256));}
            check(renderer && peak>.001f,"placed optional music produces actual export-renderer audio at authored source passages: "+error);
            const auto session=report.getSiblingFile("tutorial-first-mix.mixworks");const auto saved=ui.engine.saveProject(session);Engine reopened(juce::File(),false);
            const auto opened=saved.isEmpty() ? reopened.restoreProject(session.loadFileAsString()):saved;
            check(saved.isEmpty() && opened.isEmpty() && reopened.project.clips.size()==1,"first demo mix saves and reopens with its real generated source: "+opened);
        }
        ui.tutorialWindow->closeButtonPressed();return;
    }
    if(juce::JUCEApplicationBase::getCommandLineParameters().contains("--library-row-preview-only"))
    {
        // JUCE creates native accessibility handlers only when a window peer exists.
        // Keep the fixture window hidden; no production window or audio device is used.
        ui.addToDesktop(0);
        const auto seekFile=report.getSiblingFile("seek-sections.wav");juce::AudioBuffer<float> sections(1,8*48000);
        for(int i=0;i<sections.getNumSamples();++i)sections.setSample(0,i,i<4*48000 ? .12f:.36f);
        {juce::WavAudioFormat wav;std::unique_ptr<juce::OutputStream> stream=seekFile.createOutputStream();auto writer=wav.createWriterFor(stream,juce::AudioFormatWriterOptions().withSampleRate(48000).withNumChannels(1).withBitsPerSample(16));
            check(writer && writer->writeFromAudioSampleBuffer(sections,0,sections.getNumSamples()),"write owned source with independently known early/late audio sections");}
        auto first=std::make_shared<Media>();first->file=seekFile;first->name="Alpha seek preview";first->sampleRate=48000;first->samples.makeCopyOf(sections);
        first->peaks.assign(64,.12f);for(size_t i=32;i<64;++i)first->peaks[i]=.36f;
        media->name="Beta other preview";media->sampleRate=48000;media->samples.makeCopyOf(tone);
        ui.engine.project.masterDb=0;ui.engine.library={first,media};ui.layout.preferredLibraryWidth=520;ui.resized();ui.filterLibrary();ui.libraryTable.table.getHeader().setColumnWidth(1,220);ui.refresh();
        const auto find=[](juce::Component& parent,const juce::String& id)
        {std::function<juce::Component*(juce::Component&)> walk=[&](juce::Component& c)->juce::Component* {if(c.getComponentID()==id)return &c;for(auto* child:c.getChildren())if(auto* match=walk(*child))return match;return nullptr;};return walk(parent);};
        const auto rowFor=[&](const juce::String& id) {for(size_t n=0;n<ui.visibleMedia.size();++n)if(ui.engine.library[(size_t)ui.visibleMedia[n]]->id==id)return (int)n;return -1;};
        auto* seek=dynamic_cast<juce::Slider*>(find(ui.libraryTable.table,"library-seek-"+first->id));
        auto* accessibility=seek ? seek->getAccessibilityHandler():nullptr;
        check(seek && seek->isVisible() && accessibility && accessibility->getRole()==juce::AccessibilityRole::slider,
            "actual title cell exposes a visible seekable preview slider with native accessibility");if(!seek)return;
        check(seek->getTooltip().contains("pauses the arrangement") && seek->getDescription().contains("Left/Right"),"preview strip explains its audible consequence and keyboard seeking");
        check(Workstation::libraryNeedsReview(*first) && seek->isEnabled(),"review styling leaves the track preview fully usable");
        const auto original=juce::JSON::toString(projectToJson(ui.engine.project));const auto history=ui.undoHistory.size();ui.engine.position=12.25;ui.engine.playing=true;
        const auto mouse=juce::Desktop::getInstance().getMainMouseSource();
        const auto event=[&](juce::Slider& slider,float fraction,juce::Point<float> down,bool dragged)
        {return juce::MouseEvent(mouse,{fraction*slider.getWidth(),slider.getHeight()*.5f},juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,&slider,&slider,juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),1,dragged);};
        const juce::Point<float> down{seek->getWidth()*.25f,seek->getHeight()*.5f};
        seek->mouseDown(event(*seek,.25f,down,false));seek->mouseUp(event(*seek,.25f,down,false));pumpPreview(180);
        check(ui.libraryPreview.active(first->id) && std::abs(ui.libraryPreview.positionFraction()-.25)<.02 && !ui.engine.playing && ui.engine.position==12.25,
            "actual row seek click starts inside the file and pauses only the master arrangement without rewinding");
        const auto heard=[&] {juce::AudioBuffer<float> audio(2,256);for(int n=0;n<8;++n)ui.engine.audioDeviceIOCallbackWithContext(nullptr,0,audio.getArrayOfWritePointers(),2,256,{});return audio;};
        auto audio=heard();check(std::abs(audio.getMagnitude(0,256)-.12f)<.001f,"early seek produces actual decoded early-section PCM through the programme callback");
        const juce::Point<float> later{seek->getWidth()*.75f,seek->getHeight()*.5f};
        seek->mouseDown(event(*seek,.75f,later,false));seek->mouseDrag(event(*seek,.75f,later,true));seek->mouseUp(event(*seek,.75f,later,true));pumpPreview(100);
        audio=heard();check(ui.libraryPreview.active(first->id) && std::abs(audio.getMagnitude(0,256)-.36f)<.001f && mainType->created==1,
            "same-source mouse seek reaches the later audio section without toggling off or reopening the device");
        seek->mouseDoubleClick(event(*seek,.75f,later,false));
        check(ui.engine.project.clips.empty() && ui.libraryPreview.active(first->id),"preview-strip double-click does not add the track to the timeline");
        pumpPreview(60);const double beforeKey=ui.libraryPreview.positionFraction();seek->keyPressed(juce::KeyPress(juce::KeyPress::leftKey));pumpPreview(80);
        check(std::abs(ui.libraryPreview.positionFraction()-(beforeKey-5./8))<.002,"focused preview Left seeks five seconds through the real source transport");
        check(juce::JSON::toString(projectToJson(ui.engine.project))==original && ui.undoHistory.size()==history && ui.engine.position==12.25 && ui.engine.project.clips.empty(),
            "seeking leaves the arrangement, saved data, Undo, programme cursor and timeline insertion untouched");
        juce::AudioBuffer<float> offline(2,256);juce::MidiBuffer midi;ui.engine.render(offline,256,0,midi);check(offline.getMagnitude(0,256)==0,"offline mix renderer excludes source audition");
        auto capture=report.getSiblingFile("library-seek-wide.png").createOutputStream();check(capture && juce::PNGImageFormat().writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()),*capture),"capture actual title, waveform strip, position and preview state");capture.reset();
        // Recycle a visible title cell while a gesture is active, as sorting can.
        const juce::Point<float> gestureStart{seek->getWidth()*.4f,seek->getHeight()*.5f};seek->mouseDown(event(*seek,.4f,gestureStart,false));pumpPreview(50);
        seek->focusLost(juce::Component::focusChangedDirectly);const double interrupted=ui.libraryPreview.positionFraction();
        seek->mouseDrag(event(*seek,.9f,gestureStart,true));pumpPreview(70);
        check(std::abs(ui.libraryPreview.positionFraction()-interrupted)<.002 && std::abs(seek->getValue()-interrupted)<.002,
            "focus loss cancels a stale drag while the preview cursor continues following the actual source");
        seek->mouseUp(event(*seek,.9f,gestureStart,true));seek->mouseDown(event(*seek,.4f,gestureStart,false));pumpPreview(50);
        auto* cell=ui.libraryTable.table.getCellComponent(1,rowFor(first->id));
        auto* recycled=ui.libraryTable.accessibleCell(rowFor(media->id),1,false,cell);check(recycled==cell,"native table reuses the title cell for another source identity");
        seek->mouseDrag(event(*seek,.8f,gestureStart,true));seek->mouseUp(event(*seek,.8f,gestureStart,true));pumpPreview(70);
        check(ui.libraryPreview.active(first->id) && !ui.libraryPreview.active(media->id),"recycled/sorted rows cancel stale seeking instead of auditioning another track");
        ui.libraryTable.accessibleCell(rowFor(first->id),1,false,recycled);ui.libraryPreview.stop();ui.previewLibraryTrack(first->id,.1);ui.previewLibraryTrack(first->id,.8);pumpPreview(130);
        check(ui.libraryPreview.active(first->id) && std::abs(ui.libraryPreview.positionFraction()-.8)<.002,"latest requested position wins while codec opening is asynchronous");
        seek->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));check(ui.libraryPreview.idle(),"preview Escape stops the active source");
        seek->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));check(ui.libraryPreview.idle(),"Escape on an idle preview cannot start playback");
        ui.previewLibraryTrack(media->id,.2);pumpPreview(100);audio=heard();check(ui.libraryPreview.active(media->id) && !ui.libraryPreview.active(first->id) && std::abs(audio.getMagnitude(0,256)-.25f)<.001f,"switching rows delivers the selected file's independent PCM");
        ui.previewLibraryTrack(first->id,2.);pumpPreview(90);heard();pumpPreview(80);check(ui.libraryPreview.idle(),"end seeking clamps safely and finishes cleanly at actual source EOF");
        auto missing=std::make_shared<Media>();missing->name="Missing preview";missing->file=report.getSiblingFile("not-present.wav");ui.engine.library.push_back(missing);ui.previewLibraryTrack(missing->id,.5);pumpPreview(100);
        check(ui.libraryPreview.idle() && ui.status.contains("Cannot read") && ui.engine.devices.getCurrentAudioDevice()->isOpen(),"missing-file preview explains failure and leaves the live device open");
        ui.libraryPreview.setPrivateOutput(true);check(ui.libraryPreview.select({"Preview fixture","Headphones"}).wasOk(),"select only the isolated private headphone fixture");ui.engine.playing=true;
        ui.previewLibraryTrack(first->id,.75);pumpPreview(120);float privatePeak=0;
        if(cueType->last && cueType->last->callback) {juce::AudioBuffer<float> cue(2,256);for(int n=0;n<8;++n)cueType->last->callback->audioDeviceIOCallbackWithContext(nullptr,0,cue.getArrayOfWritePointers(),2,256,{});privatePeak=cue.getMagnitude(0,256);}
        check(ui.engine.playing && ui.engine.position==12.25 && privatePeak>.17f && privatePeak<.19f,"private seek delivers the chosen section while programme transport remains playing at its own clock");
        ui.libraryPreview.stop();ui.engine.playing=false;ui.layout.preferredLibraryWidth=333;ui.resized();ui.libraryTable.table.getHeader().setColumnWidth(1,90);ui.refresh();
        capture=report.getSiblingFile("library-seek-compact.png").createOutputStream();check(capture && juce::PNGImageFormat().writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()),*capture),"capture compact title and usable preview strip");
        check(juce::JSON::toString(projectToJson(ui.engine.project))==original && ui.undoHistory.size()==history,"all preview starts/seeks/switches/failures preserve saved music and history");
        return;
    }
    ui.engine.library = {media}; ui.filterLibrary(); ui.refresh();
    const auto project = juce::JSON::toString(projectToJson(ui.engine.project)); const auto history = ui.undoHistory.size();
    ui.engine.position = 12.25; ui.engine.playing = true;
    ui.previewLibraryTrack(media->id);
    pumpPreview(180);
    juce::AudioBuffer<float> masterPreview(2,256);
    for (int block=0;block<8;++block)
        ui.engine.audioDeviceIOCallbackWithContext(nullptr,0,masterPreview.getArrayOfWritePointers(),2,256,{});
    check(masterPreview.getMagnitude(0,256) > .17f && masterPreview.getMagnitude(0,256) < .18f
        && cueType->created == 0 && !ui.engine.playing && ui.engine.position == 12.25,
        "default library button renders through master at master gain, pausing arrangement without rewinding or opening headphones");
    check(juce::JSON::toString(projectToJson(ui.engine.project)) == project && ui.undoHistory.size() == history,
        "master preview leaves saved arrangement and Undo untouched");
    ui.engine.monitorMuted = true;
    ui.engine.audioDeviceIOCallbackWithContext(nullptr,0,masterPreview.getArrayOfWritePointers(),2,256,{});
    check(masterPreview.getMagnitude(0,256) == 0,"master preview respects monitor mute"); ui.engine.monitorMuted = false;
    juce::MidiBuffer midi; ui.engine.render(masterPreview,256,0,midi);
    check(masterPreview.getMagnitude(0,256) == 0,"offline mix renderer excludes library preview");
    // Exercise the real scan-status layout at library-scale counts, without
    // running a library scan or writing production preferences.
    ui.musicFailures = 579; ui.musicProblems = 2; ui.stopMusicScan(true);
    auto masterCapture = report.getSiblingFile("library-preview-master.png").createOutputStream();
    check(masterCapture && juce::PNGImageFormat().writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()),*masterCapture),"capture default master preview"); masterCapture.reset();
    ui.engine.setPlaying(true); pumpPreview(70);
    check(!ui.libraryPreview.active(media->id) && ui.engine.playing,"programme Play ends master preview");
    auto earSource = std::make_shared<Media>(); earSource->sampleRate = 48000; earSource->samples.makeCopyOf(tone);
    EarLoopPlayer ear; ear.source(earSource, 0, 1); ear.hear(0); ear.level(-12);
    const double earPosition = ui.engine.position.load();
    check(ui.libraryPreview.playPrepared(ear,"ear-master-fixture").wasOk() && ui.engine.hasLibraryPreview(ear),
          "prepared by-ear loop uses the existing isolated fake master output");
    for (int block=0;block<8;++block) ui.engine.audioDeviceIOCallbackWithContext(nullptr,0,masterPreview.getArrayOfWritePointers(),2,256,{});
    check(masterPreview.getMagnitude(0,256) > .03f && masterPreview.getMagnitude(0,256) < .05f
        && !ui.engine.playing && ui.engine.position == earPosition && cueType->created == 0,
          "prepared master loop is audible at listening/master gain without moving the programme cursor or opening headphones");
    ui.libraryPreview.stop();
    ui.engine.audioDeviceIOCallbackWithContext(nullptr,0,masterPreview.getArrayOfWritePointers(),2,256,{});
    check(!ui.engine.hasLibraryPreview(ear) && masterPreview.getMagnitude(0,256) == 0,
          "prepared master callback is detached before the listening owner can be destroyed");
    ui.previewLibraryTrack(media->id); ui.engine.stop(); pumpPreview(80);
    check(!ui.libraryPreview.active(media->id),"programme Stop cancels a pending master preview reader");
    ui.engine.position = 12.25; ui.engine.playing = true;
    ui.libraryPreview.setPrivateOutput(true);
    ui.libraryPreview.toggle(file,media->id); pumpPreview(20);
    check(!ui.libraryPreview.active(media->id) && cueType->created == 0,"unconfigured previews never open a default output");
    const auto outputs = ui.libraryPreview.outputs();
    check(outputs.size() == 1 && outputs[0].name == "Headphones" && ui.libraryPreview.select({"Preview fixture","Live speakers"}).failed(),"live output excluded and explicitly rejected");
    check(ui.libraryPreview.select({"Preview fixture","Headphones"}).wasOk() && cueType->created == 0,"selecting headphones alone opens no device");
    ui.previewLibraryTrack(media->id);
    for (int n=0;n<100 && (!cueType->last || !cueType->last->callback);++n) pumpPreview(10);
    float peak = 0;
    if (cueType->last && cueType->last->callback)
    {
        juce::AudioBuffer<float> cue(2,256);
        for (int block=0;block<8;++block) { cue.clear(); cueType->last->callback->audioDeviceIOCallbackWithContext(nullptr,0,cue.getArrayOfWritePointers(),2,256,{}); peak = std::max(peak,cue.getMagnitude(0,256)); }
    }
    check(peak > .1f && peak < .14f && ui.libraryPreview.active(media->id),"independent preview callback renders file at half level: " + juce::String(peak,8) + " / " + ui.status);
    juce::AudioBuffer<float> master(2,256); master.clear();
    ui.engine.audioDeviceIOCallbackWithContext(nullptr,0,master.getArrayOfWritePointers(),2,256,{});
    check(master.getMagnitude(0,256) == 0 && ui.engine.playing && ui.engine.position > 12.25 && ui.engine.position < 12.26
        && juce::JSON::toString(projectToJson(ui.engine.project)) == project && ui.undoHistory.size() == history,
        "preview stays out of live master callback and preserves arrangement, Undo and transport");
    auto capture = report.getSiblingFile("library-preview-playing.png").createOutputStream();
    check(capture && juce::PNGImageFormat().writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()),*capture),"capture artwork stop overlay during headphone preview"); capture.reset();
    ui.previewLibraryTrack(media->id);
    check(!ui.libraryPreview.active(media->id),"same artwork stops preview");
    ui.engine.workMode = WorkMode::recordPerform;
    check(ui.libraryPreview.playPrepared(ear,"ear-private-fixture").wasOk(), "prepared by-ear loop opens only the explicitly selected fake headphones route");
    juce::AudioBuffer<float> earCue(2,256); float earPeak = 0;
    if (cueType->last && cueType->last->callback) for (int block=0;block<8;++block)
    { cueType->last->callback->audioDeviceIOCallbackWithContext(nullptr,0,earCue.getArrayOfWritePointers(),2,256,{}); earPeak = std::max(earPeak,earCue.getMagnitude(0,256)); }
    ui.engine.audioDeviceIOCallbackWithContext(nullptr,0,master.getArrayOfWritePointers(),2,256,{});
    check(earPeak > .01f && earPeak < .04f && master.getMagnitude(0,256) == 0 && ui.engine.playing,
          "prepared private listening remains audible only on headphones while programme transport continues");
    ui.libraryPreview.stop(); ui.engine.workMode = WorkMode::arrange;
    ui.previewLibraryTrack(media->id); ui.libraryPreview.stop(); pumpPreview(40);
    check(!ui.libraryPreview.active(media->id),"stopped pending reader cannot restart playback");
    cueType->names = {"Live speakers"}; const int before = cueType->created;
    ui.previewLibraryTrack(media->id); pumpPreview(80);
    check(!ui.libraryPreview.active(media->id) && cueType->created == before && ui.engine.devices.getCurrentAudioDevice()->getName() == "Live speakers","missing headphone device never falls back to live output");
    cueType->names.add("Headphones"); cueType->fail = true; ui.previewLibraryTrack(media->id); pumpPreview(80);
    check(!ui.libraryPreview.active(media->id) && ui.engine.devices.getCurrentAudioDevice()->isOpen(),"preview device failure leaves live device open");
    cueType->fail = false; ui.previewLibraryTrack(media->id); pumpPreview(80);
    ui.engine.devices.sendChangeMessage(); pumpPreview(30);
    check(!ui.libraryPreview.active(media->id),"main audio reconfiguration stops headphone preview");
    ui.libraryPreview.chooseOutput(ui); pumpPreview(20);
    if (auto* dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent()))
    {
        check(dialog->getComboBoxComponent("output")->getNumItems() == 1,"native output chooser lists only separate fixture headphones");
        auto image = report.getSiblingFile("library-preview-output.png").createOutputStream();
        check(image && juce::PNGImageFormat().writeImageToStream(dialog->createComponentSnapshot(dialog->getLocalBounds()),*image),"capture separate output chooser");
        dialog->exitModalState(0); pumpPreview(20);
    }
    else check(false,"preview output chooser opens");
    ui.engine.playing = false;
}
}
