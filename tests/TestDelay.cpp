#include <juce_audio_utils/juce_audio_utils.h>

// Actual test-only VST3: three known stereo echoes, no dry signal by default.
// The optional non-realtime delay makes cooperative export cancellation observable.
class TestDelay final : public juce::AudioProcessor
{
public:
    TestDelay() : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true))
    {
        addParameter(mix = new juce::AudioParameterFloat("mix", "Wet mix", juce::NormalisableRange<float>(0, 1), 1));
        addParameter(slow = new juce::AudioParameterBool("slowExport", "Slow export fixture", false));
    }
    const juce::String getName() const override { return "Mixworks Test Delay"; }
    void prepareToPlay(double sampleRate, int) override
    { rate = sampleRate; delay = std::max(1, (int) std::llround(sampleRate * .125)); history.setSize(2, delay * 3 + 1); reset(); }
    void releaseResources() override {}
    void reset() override { history.clear(); cursor = 0; }
    void processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer&) override
    {
        if (isNonRealtime() && slow->get()) juce::Thread::sleep(1);
        const int size = history.getNumSamples(); if (size == 0) return;
        const float wet = mix->get();
        for (int frame = 0; frame < audio.getNumSamples(); ++frame)
        {
            for (int channel = 0; channel < 2; ++channel)
            {
                const float input = audio.getSample(channel, frame);
                const float echoes = history.getSample(channel, (cursor + size - delay) % size) * .6f
                    + history.getSample(channel, (cursor + size - delay * 2) % size) * .3f
                    + history.getSample(channel, (cursor + size - delay * 3) % size) * .1f;
                history.setSample(channel, cursor, input); audio.setSample(channel, frame, input * (1 - wet) + echoes * wet);
            }
            cursor = (cursor + 1) % size;
        }
    }
    bool isBusesLayoutSupported(const BusesLayout& layout) const override
    { return layout.getMainInputChannelSet() == juce::AudioChannelSet::stereo() && layout.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return rate > 0 ? delay * 3 / rate : 0; }
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override { return new juce::GenericAudioProcessorEditor(*this); }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& state) override
    { const float values[] {mix->get(), slow->get() ? 1.f : 0.f}; state.replaceAll(values, sizeof(values)); }
    void setStateInformation(const void* data, int bytes) override
    {
        if (bytes != 2 * sizeof(float)) return; float values[2]; std::memcpy(values, data, sizeof(values));
        if (std::isfinite(values[0]) && std::isfinite(values[1])) { *mix = juce::jlimit(0.f, 1.f, values[0]); *slow = values[1] >= .5f; }
    }
private:
    juce::AudioParameterFloat* mix;
    juce::AudioParameterBool* slow;
    juce::AudioBuffer<float> history;
    int delay = 0, cursor = 0;
    double rate = 0;
};
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TestDelay(); }
