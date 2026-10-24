#include "Workstation.h"
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
