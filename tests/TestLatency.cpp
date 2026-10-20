#include <juce_audio_utils/juce_audio_utils.h>

// Real VST3 with a reported and physically implemented 64-sample transport
// latency. This is distinct from the zero-host-latency creative delay fixture.
class TestLatency final : public juce::AudioProcessor
{
public:
    TestLatency() : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true))
    {
        addParameter(delayed = new LatencyMode()); delayed->changed = [this] { updateLatency(); }; setLatencySamples(64);
    }
    const juce::String getName() const override { return "Mixworks Test Latency"; }
    void prepareToPlay(double, int) override { history.setSize(2, 64); reset(); updateLatency(); }
    void releaseResources() override {}
    void reset() override { history.clear(); cursor = 0; }
    void processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer&) override
    {
        updateLatency(); if (!delayed->get()) return;
        for (int i = 0; i < audio.getNumSamples(); ++i)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                const auto input = audio.getSample(ch, i); audio.setSample(ch, i, history.getSample(ch, cursor)); history.setSample(ch, cursor, input);
            }
            cursor = (cursor + 1) % 64;
        }
    }
    bool isBusesLayoutSupported(const BusesLayout& b) const override { return b.getMainInputChannelSet() == juce::AudioChannelSet::stereo() && b.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0; }
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override { return new juce::GenericAudioProcessorEditor(*this); }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& state) override { const unsigned char value = delayed->get() ? 1 : 0; state.replaceAll(&value, 1); }
    void setStateInformation(const void* data, int bytes) override
    { if (bytes == 1) { delayed->setValueNotifyingHost(*static_cast<const unsigned char*>(data) != 0 ? 1.f : 0.f); updateLatency(); reset(); } }
private:
    struct LatencyMode final : juce::AudioParameterBool
    {
        LatencyMode() : AudioParameterBool("latency", "Latency enabled", true) {}
        std::function<void()> changed;
        void valueChanged(bool) override { if (changed) changed(); }
    };
    void updateLatency() { const int samples = delayed->get() ? 64 : 0; if (getLatencySamples() != samples) setLatencySamples(samples); }
    LatencyMode* delayed;
    juce::AudioBuffer<float> history;
    int cursor = 0;
};
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TestLatency(); }
