#include <juce_audio_utils/juce_audio_utils.h>
class TestGain final : public juce::AudioProcessor
{
public:
    TestGain() : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true))
    { addParameter(gain = new juce::AudioParameterFloat("gain", "Gain", juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f)); }
    const juce::String getName() const override { return "Mixworks Test Gain"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>& b, juce::MidiBuffer&) override { b.applyGain(gain->get()); }
    bool isBusesLayoutSupported(const BusesLayout& layout) const override { return layout.getMainInputChannelSet() == juce::AudioChannelSet::stereo() && layout.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
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
    void getStateInformation(juce::MemoryBlock& state) override { const float value = gain->get(); state.replaceAll(&value, sizeof(value)); }
    void setStateInformation(const void* data, int size) override { if (size == sizeof(float)) { float value; std::memcpy(&value, data, sizeof(value)); *gain = value; } }
private:
    juce::AudioParameterFloat* gain;
};
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TestGain(); }
