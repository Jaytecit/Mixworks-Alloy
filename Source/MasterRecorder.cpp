#include "MasterRecorder.h"
#include <cmath>
namespace mixworks
{
namespace
{
class RecordingStream final : public juce::OutputStream
{
public:
    RecordingStream(std::unique_ptr<juce::FileOutputStream> f,std::atomic<bool>& error):file(std::move(f)),failed(error) {}
    ~RecordingStream() override { flush(); }
    juce::int64 getPosition() override { return file->getPosition(); }
    bool setPosition(juce::int64 position) override { const bool ok=file->setPosition(position); if (!ok) failed=true; return ok; }
    bool write(const void* data,size_t bytes) override { const bool ok=file->write(data,bytes); if (!ok) failed=true; return ok; }
    void flush() override { file->flush(); if (file->getStatus().failed()) failed=true; }
private:
    std::unique_ptr<juce::FileOutputStream> file; std::atomic<bool>& failed;
};
}
MasterRecorder::MasterRecorder() = default;
MasterRecorder::~MasterRecorder() { finish(); }
juce::Result MasterRecorder::start(const juce::File& destination,double rate,int fifoFrames)
{
    if (active() || destination.exists() || destination.isSymbolicLink() || !std::isfinite(rate) || rate<8000 || fifoFrames<4 || fifoFrames>131072)
        return juce::Result::fail("Choose a new WAV filename while stopped; existing files are kept.");
    if (destination.getParentDirectory().createDirectory().failed()) return juce::Result::fail("Cannot create the master recording folder.");
    auto fileStream=destination.createOutputStream(); if (!fileStream || fileStream->getStatus().failed()) return juce::Result::fail("Cannot open the master WAV.");
    overflow=false; std::unique_ptr<juce::OutputStream> stream=std::make_unique<RecordingStream>(std::move(fileStream),overflow);
    juce::WavAudioFormat wav;
    auto raw=wav.createWriterFor(stream,juce::AudioFormatWriterOptions().withSampleRate(rate).withNumChannels(2).withBitsPerSample(24));
    if (!raw) return juce::Result::fail("Cannot create the master WAV writer.");
    if (overflow) return juce::Result::fail("Cannot write the master WAV header. The partial file was kept: "+destination.getFullPathName());
    file=destination; acceptedFrames=0; thread.startThread(juce::Thread::Priority::low);
    writer=std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(raw.release(),thread,fifoFrames);
    return juce::Result::ok();
}
void MasterRecorder::push(const juce::AudioBuffer<float>& audio,int frames)
{
    if (writer && !overflow) { if (!writer->write(audio.getArrayOfReadPointers(),frames)) overflow=true; else acceptedFrames+=frames; }
}
juce::String MasterRecorder::finish()
{
    writer.reset(); thread.stopThread(-1);
    // ThreadedWriter reports FIFO failure synchronously. Re-open the final file
    // to catch a truncated/disk-failed stream too, before claiming completion.
    if (file==juce::File()) return {};
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    return overflow || !reader || reader->lengthInSamples!=acceptedFrames ? "Incomplete master WAV (FIFO/disk write failure). Usable partial audio was kept: "+file.getFullPathName() : juce::String();
}
}
