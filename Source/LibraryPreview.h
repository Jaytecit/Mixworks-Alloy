#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

namespace mixworks
{
class Engine;
// Independent streaming transport. Arrange previews use the master device;
// private previews require an explicit headphone route. Neither enters export.
class LibraryPreview final : public juce::Component, private juce::ChangeListener, private juce::Timer
{
public:
    struct Output { juce::String type, name; int firstChannel = -1; };
    explicit LibraryPreview(Engine&);
    ~LibraryPreview() override;
    std::vector<Output> outputs();
    juce::Result select(Output);
    void toggle(const juce::File&, const juce::String& sourceId);
    void stop();
    void chooseOutput(juce::Component& parent, std::function<void()> after = {});
    juce::Result playPrepared(juce::AudioSource&,const juce::String& id);
    void setLevel(float value);
    Output selectedOutput() const { return output; }
    void restoreOutputPreference(Output value) { stop(); output=std::move(value); }
    bool active(const juce::String& id) const { return id.isNotEmpty() && id == source; }
    bool configured() const { return output.name.isNotEmpty(); }
    void setPrivateOutput(bool enabled) { stop(); privateOutput = enabled; }
    bool usesPrivateOutput() const { return privateOutput; }
    juce::String description() const;
    std::function<void(const juce::String&)> onChange;
    // Exposed for isolated driver fixtures; does not initialise a real device.
    juce::AudioDeviceManager inventory;
private:
    bool allowed(const Output&) const;
    void notify(const juce::String&);
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    Engine& engine;
    juce::AudioDeviceManager& main;
    Output output;
    juce::TimeSliceThread readAhead {"Library headphone read-ahead"};
    juce::ThreadPool loader {1};
    juce::AudioTransportSource transport;
    juce::AudioSourcePlayer player;
    std::unique_ptr<juce::AudioFormatReaderSource> reader;
    std::unique_ptr<juce::AudioIODevice> device;
    juce::String source;
    uint64_t generation = 0;
    uint64_t masterRequest = 0;
    bool loading = false;
    bool privateOutput = false;
    juce::AudioSource* preparedSource = nullptr;
    float privateLevel = .5f;
};
}
