#include "LibraryPreview.h"
#include "AlloyLookAndFeel.h"
#include "AppWindow.h"
#include "Engine.h"

namespace mixworks
{
namespace
{
void primePrivatePlayer(juce::AudioSourcePlayer& player,juce::AudioIODevice& device,float level)
{
    struct Silence final : juce::AudioSource
    {
        void prepareToPlay(int,double) override {}
        void releaseResources() override {}
        void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override { info.clearActiveBufferRegion(); }
    } silence;
    // AudioSourcePlayer's first ramp starts at unity. Prime only owned silence
    // before the device starts and before attaching music/preparation, so the
    // first audible sample already uses the selected private level.
    player.setGain(level); player.setSource(&silence); player.audioDeviceAboutToStart(&device);
    juce::AudioBuffer<float> scratch(2,1); float* outputs[]{scratch.getWritePointer(0),scratch.getWritePointer(1)};
    player.audioDeviceIOCallbackWithContext(nullptr,0,outputs,2,1,{}); player.setSource(nullptr);
}
}
LibraryPreview::LibraryPreview(Engine& e) : engine(e), main(e.devices)
{
    main.addChangeListener(this); transport.addChangeListener(this);
    transport.setGain(.5f);
    // Prime the transport's previous gain while silent, so even its first
    // audible sample uses the headphone level rather than a unity fade-down.
    juce::AudioBuffer<float> silence(2,1); transport.getNextAudioBlock({&silence,0,1});
}
LibraryPreview::~LibraryPreview()
{
    onChange = {}; main.removeChangeListener(this); transport.removeChangeListener(this);
    stop(); loader.removeAllJobs(true,-1); readAhead.stopThread(-1);
}
bool LibraryPreview::allowed(const Output& candidate) const
{
    auto* live = main.getCurrentAudioDevice();
    // Compare names across driver modes too. Physical routing behind differently
    // named drivers is selected by the user; we never choose a default device.
    if (candidate.firstChannel>=2) return live && candidate.type==main.getCurrentAudioDeviceType() && candidate.name==live->getName()
        && candidate.firstChannel+1<live->getOutputChannelNames().size();
    return candidate.firstChannel==-1 && candidate.name.isNotEmpty() && (!live || !candidate.name.equalsIgnoreCase(live->getName()));
}
std::vector<LibraryPreview::Output> LibraryPreview::outputs()
{
    std::vector<Output> result;
    if (auto* live=main.getCurrentAudioDevice()) for (int pair=2;pair+1<live->getOutputChannelNames().size();pair+=2)
        result.push_back({main.getCurrentAudioDeviceType(),live->getName(),pair});
    for (auto* type : inventory.getAvailableDeviceTypes())
    {
        type->scanForDevices();
        for (const auto& name : type->getDeviceNames(false))
            if (allowed({type->getTypeName(),name})) result.push_back({type->getTypeName(),name});
    }
    return result;
}
juce::Result LibraryPreview::select(Output next)
{
    stop();
    if (!allowed(next)) return juce::Result::fail("Choose a separate headphones device; the live output cannot be used for previews.");
    const auto choices = outputs();
    if (std::none_of(choices.begin(),choices.end(),[&](const auto& c) { return c.type == next.type && c.name == next.name && c.firstChannel==next.firstChannel; }))
        return juce::Result::fail("That preview output is no longer available. Choose another headphones device.");
    if (next.firstChannel>=2)
    {
        if (engine.playing || engine.capturingPerformance()) return juce::Result::fail("Configure headphone channels while stopped.");
        auto setup=main.getAudioDeviceSetup(); setup.useDefaultOutputChannels=false; setup.outputChannels.setRange(0,2,true); setup.outputChannels.setRange(next.firstChannel,2,true);
        const auto error=main.setAudioDeviceSetup(setup,true); if (error.isNotEmpty()) return juce::Result::fail(error);
    }
    output = std::move(next); notify("Preview output: " + description());
    return juce::Result::ok();
}
juce::String LibraryPreview::description() const
{ return configured() ? output.name + (output.firstChannel>=2 ? " / channels "+juce::String(output.firstChannel+1)+"-"+juce::String(output.firstChannel+2) : " (" + output.type + ")") : "Choose headphones device or independent stereo pair"; }
void LibraryPreview::notify(const juce::String& message) { if (onChange) onChange(message); }
void LibraryPreview::stop()
{
    stopTimer(); engine.detachLibraryPreview(transport);
    engine.detachPrivatePreview(transport);
    if (preparedSource) { engine.detachLibraryPreview(*preparedSource); engine.detachPrivatePreview(*preparedSource); }
    ++generation; loading = false;
    if (device) { device->stop(); device->close(); device.reset(); }
    // The callback is detached: setSource clears transport playback directly.
    // stop() would wait for a final callback that can no longer arrive.
    player.setSource(nullptr); preparedSource=nullptr; transport.releaseResources(); transport.setSource(nullptr); reader.reset();
    const bool hadSource = source.isNotEmpty(); source.clear();
    if (hadSource) notify("Library preview stopped.");
}
void LibraryPreview::toggle(const juce::File& file, const juce::String& id)
{
    if (active(id)) { stop(); return; }
    stop();
    if (privateOutput && (!configured() || !allowed(output))) { notify("Choose a separate headphones output before previewing."); return; }
    if (!privateOutput) masterRequest = engine.requestLibraryPreview();
    source = id; loading = true; const auto request = generation;
    notify(privateOutput ? "Loading headphone preview..." : "Loading master preview...");
    // Header/codec opening stays off the message thread and outside Engine.lock.
    loader.addJob([safe = juce::Component::SafePointer<LibraryPreview>(this),file,id,request]
    {
        juce::AudioFormatManager manager; manager.registerBasicFormats();
        auto opened = std::make_shared<std::unique_ptr<juce::AudioFormatReader>>(manager.createReaderFor(file));
        juce::MessageManager::callAsync([safe,opened,id,request]
        {
            if (!safe || safe->generation != request) return;
            if (!*opened || (*opened)->lengthInSamples <= 0 || (*opened)->sampleRate <= 0)
            { safe->stop(); safe->notify("Cannot read this file for preview."); return; }
            if (safe->privateOutput && !safe->allowed(safe->output))
            { safe->stop(); safe->notify("Preview stopped: the chosen device is now the live output."); return; }
            // Rescan and open only the exact selection; no default/fallback path.
            if (safe->privateOutput && safe->output.firstChannel<0) for (auto* type : safe->inventory.getAvailableDeviceTypes())
                if (type->getTypeName() == safe->output.type)
                {
                    type->scanForDevices();
                    if (type->getDeviceNames(false).contains(safe->output.name))
                        safe->device.reset(type->createDevice(safe->output.name,{}));
                    break;
                }
            juce::String error = safe->privateOutput && safe->output.firstChannel<0 ? "The selected headphones device is unavailable." : "";
            if (safe->device)
            {
                juce::BigInteger channels; channels.setRange(0,std::min(2,safe->device->getOutputChannelNames().size()),true);
                if (!channels.isZero()) error = safe->device->open({},channels,0,0);
                if (error.isEmpty() && (!safe->device->isOpen() || safe->device->getName() != safe->output.name))
                    error = "The driver did not open the selected headphones device.";
            }
            if (error.isNotEmpty()) { safe->stop(); safe->notify("Preview unavailable: " + error); return; }
            const auto rate = (*opened)->sampleRate;
            safe->reader = std::make_unique<juce::AudioFormatReaderSource>(opened->release(),true);
            safe->readAhead.startThread(juce::Thread::Priority::low);
            safe->transport.setGain(1.f);
            // Prime the gain while there is no source, including when switching
            // between master and private audition. Do not consume source audio.
            juce::AudioBuffer<float> silence(2,1);
            safe->transport.getNextAudioBlock({&silence,0,1});
            safe->transport.setSource(safe->reader.get(),32768,&safe->readAhead,rate);
            if (safe->privateOutput)
            {
                if (safe->output.firstChannel>=2)
                {
                    safe->transport.prepareToPlay(safe->engine.preparedBlockSize(),safe->engine.sampleRate.load());
                    if (!safe->engine.startPrivatePreview(safe->transport,safe->output.firstChannel)) { safe->stop(); safe->notify("Selected private channels are unavailable; programme output was kept."); return; }
                }
                else { primePrivatePlayer(safe->player,*safe->device,safe->privateLevel); safe->player.setSource(&safe->transport); safe->device->start(&safe->player); }
            }
            else if (!safe->engine.startLibraryPreview(safe->transport,safe->masterRequest))
            { safe->stop(); safe->notify("Preview cancelled: the transport or master output changed, or an export is active."); return; }
            safe->loading = false; safe->transport.start(); safe->startTimerHz(20);
            safe->notify(safe->privateOutput ? "Headphone preview: " + safe->description() : "Library preview through master output. Arrangement paused.");
        });
    });
}
void LibraryPreview::timerCallback()
{
    if (device && (!device->isPlaying() || device->getLastError().isNotEmpty())) { stop(); notify("Headphone output failed. Private playback stopped; audience routing was kept."); return; }
    if (preparedSource)
    {
        if (!privateOutput && !engine.hasLibraryPreview(*preparedSource)) stop();
        return;
    }
    if (!loading && ((!privateOutput && !engine.hasLibraryPreview(transport)) || !transport.isPlaying())) stop();
}
void LibraryPreview::changeListenerCallback(juce::ChangeBroadcaster* changed)
{
    if (changed == &main)
    {
        const bool wasActive = source.isNotEmpty(); stop();
        if (wasActive) notify("Preview stopped because the main audio configuration changed.");
    }
    else if (!preparedSource && !loading && source.isNotEmpty() && !transport.isPlaying()) stop();
}
void LibraryPreview::chooseOutput(juce::Component& parent, std::function<void()> after)
{
    stop(); const auto choices = outputs();
    auto* dialog = new juce::AlertWindow("Library preview output",
        "Choose a separate headphones output while playback is stopped. Private listening never falls back to the audience speakers.",juce::MessageBoxIconType::NoIcon,&parent);
    juce::StringArray names;
    for (const auto& choice : choices) names.add(choice.name + (choice.firstChannel>=2 ? " / channels "+juce::String(choice.firstChannel+1)+"-"+juce::String(choice.firstChannel+2) : " (" + choice.type + ")"));
    dialog->addComboBox("output",names,"Headphones output");
    auto* box = dialog->getComboBoxComponent("output"); box->setSelectedId(0,juce::dontSendNotification);
    box->setTextWhenNothingSelected(choices.empty() ? "No separate output available" : "Select headphones device");
    for (size_t i = 0; i < choices.size(); ++i)
        if (choices[i].name == output.name && choices[i].type == output.type && choices[i].firstChannel==output.firstChannel) box->setSelectedItemIndex((int)i,juce::dontSendNotification);
    dialog->addButton("Use output",1); dialog->addButton("Cancel",0,juce::KeyPress(juce::KeyPress::escapeKey));
    dialog->getButton(0)->setEnabled(box->getSelectedId() > 0);
    box->onChange = [dialog,box] { dialog->getButton(0)->setEnabled(box->getSelectedId() > 0); };
    dialog->enterModalState(true,juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<LibraryPreview>(this),dialog,choices,after](int result)
    {
        if (!safe || result != 1) return;
        const int i = dialog->getComboBoxComponent("output")->getSelectedItemIndex();
        if (i < 0 || i >= (int)choices.size()) return;
        const auto selected = safe->select(choices[(size_t)i]);
        if (selected.failed()) safe->notify(selected.getErrorMessage()); else if (after) after();
    }),true);
}
void LibraryPreview::setLevel(float value)
{
    privateLevel=juce::jlimit(0.f,1.f,value); player.setGain(privateLevel); engine.privatePreviewGain=privateLevel;
}
juce::Result LibraryPreview::playPrepared(juce::AudioSource& audio,const juce::String& id)
{
    stop();
    if (!privateOutput)
    {
        if (engine.workMode == WorkMode::recordPerform) return juce::Result::fail("Choose a private headphones route in Record/Perform.");
        const auto request = engine.requestLibraryPreview();
        audio.prepareToPlay(engine.preparedBlockSize(), engine.sampleRate.load());
        if (!engine.startLibraryPreview(audio, request)) return juce::Result::fail("Preview unavailable: transport or export state changed.");
        preparedSource = &audio; source = id; startTimerHz(20); return juce::Result::ok();
    }
    if (!privateOutput || !configured() || !allowed(output)) return juce::Result::fail("Configure an explicit private headphone route before private playback.");
    preparedSource=&audio; source=id;
    if (output.firstChannel>=2)
    {
        audio.prepareToPlay(engine.preparedBlockSize(),engine.sampleRate.load());
        if (engine.startPrivatePreview(audio,output.firstChannel)) { startTimerHz(20); return juce::Result::ok(); }
        stop(); return juce::Result::fail("The selected private channels are unavailable. Audience output was kept.");
    }
    for (auto* type:inventory.getAvailableDeviceTypes()) if (type->getTypeName()==output.type)
    { type->scanForDevices(); if (type->getDeviceNames(false).contains(output.name)) device.reset(type->createDevice(output.name,{})); break; }
    if (!device) { stop(); return juce::Result::fail("The selected headphones device is unavailable."); }
    juce::BigInteger channels; channels.setRange(0,2,true); const auto error=device->open({},channels,0,0);
    if (error.isNotEmpty() || !device->isOpen()) { stop(); return juce::Result::fail("Cannot open the selected headphones device: "+error); }
    primePrivatePlayer(player,*device,privateLevel); player.setSource(&audio); device->start(&player); startTimerHz(20); return juce::Result::ok();
}
}
