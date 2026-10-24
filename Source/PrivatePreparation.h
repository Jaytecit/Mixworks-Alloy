#pragma once
#include "Engine.h"
namespace mixworks
{
// Mutable DSP belongs exclusively to this owner. Only verified PCM is shared.
class PrivatePreparation final : public juce::AudioSource
{
public:
    explicit PrivatePreparation(Engine& e):programme(e) {}
    juce::Result prepareTrack(const juce::String& clip);
    juce::Result prepareTransition(const juce::String& incoming);
    juce::Result prepareSourceAudition();
    juce::Result alternative(int slot);
    juce::Result setControl(const AutomationAddress&,float);
    juce::Result apply(bool launch);
    void discard();
    void seek(double seconds);
    double cursor() const { return position.load(); }
    bool ready() const { return renderer!=nullptr; }
    void setLoop(bool value) { loop=value; }
    void setPlaying(bool value) { playing=value; }
    juce::String target() const { return incoming.isEmpty() ? sourceClip : incoming; }
    void prepareToPlay(int,double) override;
    void releaseResources() override {}
    void getNextAudioBlock(const juce::AudioSourceChannelInfo&) override;
    Engine* privateEngine() { return renderer.get(); }
private:
    juce::Result prepare(const juce::String&,bool);
    Engine& programme;
    std::unique_ptr<Engine> renderer;
    juce::String sourceClip,incoming;
    Project baseline;
    juce::AudioBuffer<float> buffer;
    std::atomic<double> position {0};
    double begin=0,end=0,outputRate=44100;
    bool loop=true,playing=true;
    bool sourceAudition=false;
    juce::CriticalSection mutex;
};
}
