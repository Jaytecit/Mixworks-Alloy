#include "TutorialMusic.h"
#include <juce_cryptography/juce_cryptography.h>

namespace mixworks
{
namespace
{
constexpr int rate=44100, frames=32*rate, blockSize=frames/4000;
constexpr const char* names[]{"01 Copper Steps.wav","02 Blue Hours.wav","03 Green Spark.wav"};
double tone(double midi,double time) {return std::sin(juce::MathConstants<double>::twoPi*440*std::pow(2.,(midi-69)/12)*time);}
float sample(int song,int frame,int channel)
{
    const double time=(double)frame/rate,beat=time*2,within=std::fmod(time,.5),eighth=std::fmod(time,.25);
    const int bar=(int)(beat/4),step=(int)(beat*2)%16,chord=(bar/4)%4;
    constexpr int roots[]{36,33,29,31}, thirds[]{4,3,4,4};
    // A deterministic per-sample noise source: no shared RNG or recordings.
    uint32_t seed=(uint32_t)frame*747796405u+(uint32_t)song*2891336453u+277803737u;
    seed=((seed>>((seed>>28)+4))^seed)*277803737u;seed=(seed>>22)^seed;
    const double noise=(double)(seed&65535)/32767.5-1;
    const double kick=.36*std::exp(-within*19)*std::sin(juce::MathConstants<double>::twoPi*(45*within+100./35*(1-std::exp(-35*within))));
    const double snare=((int)beat%2==1 ? .11*std::exp(-within*25)*(noise+.4*tone(50,within)):0);
    const double hat=(song==1 && step%2 ? .025:.04)*noise*std::exp(-eighth*85);
    const bool bassNote=(step+(song==1 ? 1:0))%4!=3;
    const double bass=bassNote ? .17*(1-std::exp(-eighth*450))*std::exp(-eighth*12)
        *(tone(roots[chord]+(song==2 && step%4==2 ? 12:0),time)+.18*tone(roots[chord]+12,time)):0;
    const double swell=juce::jlimit(0.,1.,(beat-8)/8)*.026*(.85+.15*std::sin(time*1.3));
    const double pad=swell*(tone(roots[chord]+24,time+channel*.0003)+tone(roots[chord]+24+thirds[chord],time)+tone(roots[chord]+31,time-channel*.0002));
    constexpr int melody[]{72,76,79,74,81,79,76,74,72,79,76,74,79,81,76,72};
    const int note=melody[(step+song*3+(bar%4)*2)%16];
    const double hook=bar>=4 && (step+song)%3!=2 ? .055*(1-std::exp(-eighth*500))*std::exp(-eighth*(song==1 ? 13:20))
        *(tone(note,time)+.15*tone(note+12,time))*(channel==song%2 ? 1:.75):0;
    const double fade=std::min({1.,time/.004,(32-time)/.04});
    return (float)juce::jlimit(-.9,.9,fade*(kick+snare+hat+bass+pad+hook));
}
bool stopped(const std::shared_ptr<std::atomic<bool>>& flag) {return flag && flag->load();}
struct Stage
{
    juce::File folder;juce::Array<juce::File> created;
    ~Stage() {for(const auto& file:created)file.deleteFile();if(folder!=juce::File())folder.deleteFile();}
};
}
TutorialMusicResult createTutorialMusic(const juce::File& destination,const std::shared_ptr<std::atomic<bool>>& cancelled)
{
    TutorialMusicResult result;
    auto fail=[&](juce::String error) {result.files.clear();result.error=std::move(error);return result;};
    if(stopped(cancelled))return fail("Demo preparation cancelled. Your music was kept.");
    if(destination.isSymbolicLink() || destination.getParentDirectory().isSymbolicLink())return fail("The demo folder is a link. Choose your own music instead.");
    const auto manifest=destination.getChildFile("original-demo.json");
    if(destination.exists())
    {
        if(manifest.isSymbolicLink() || manifest.getSize()>16384)return fail("The existing demo manifest cannot be used. Its files were kept.");
        const auto saved=juce::JSON::parse(manifest);
        if(manifest.isSymbolicLink() || saved["format"].toString()!="mixworks-original-demo-v1" || saved["hashes"].size()!=3)
            return fail("The existing demo folder is not recognised. Its files were kept; import your own music instead.");
        for(int song=0;song<3;++song)
        {
            const auto file=destination.getChildFile(names[song]);
            if(file.isSymbolicLink() || !file.existsAsFile() || audioDigest(file)!=saved["hashes"][song].toString())
                return fail("A demo file changed or is missing. Existing files were kept; import your own music instead.");
            if(stopped(cancelled))return fail("Demo preparation cancelled. Your music was kept.");
            result.files.add(file);
        }
        return result; // Reuse leaves reviewed grids/corrections untouched.
    }
    const auto parent=destination.getParentDirectory().createDirectory();if(parent.failed())return fail(parent.getErrorMessage());
    Stage stage{destination.getSiblingFile(".tutorial-"+juce::Uuid().toString()),{}};
    if(stage.folder.exists() || stage.folder.createDirectory().failed())return fail("Cannot create the optional demo files. Check workspace folder access.");
    juce::Array<juce::var> hashes;std::array<TrackAnalysisRecord,3> records;
    for(int song=0;song<3;++song)
    {
        const auto file=stage.folder.getChildFile(names[song]);stage.created.add(file);
        juce::WavAudioFormat wav;std::unique_ptr<juce::OutputStream> stream=file.createOutputStream();
        if(!stream)return fail("Cannot write the optional demo audio.");
        auto writer=wav.createWriterFor(stream,juce::AudioFormatWriterOptions().withSampleRate(rate).withNumChannels(2).withBitsPerSample(16)
            .withMetadata(juce::WavAudioFormat::riffInfoArtist,"Mixworks original tutorial music")
            .withMetadata(juce::WavAudioFormat::riffInfoComment,"Original synthetic demonstration. 120 BPM, C major, 4/4, 16 bars. No imported recordings."));
        if(!writer)return fail("Cannot create the optional demo WAV.");
        auto& record=records[(size_t)song];juce::AudioBuffer<float> buffer(2,blockSize);
        for(int offset=0;offset<frames;offset+=blockSize)
        {
            if(stopped(cancelled))return fail("Demo preparation cancelled. Your music was kept.");
            const int count=std::min(blockSize,frames-offset);float peak=0;double squared=0;
            for(int i=0;i<count;++i)for(int channel=0;channel<2;++channel)
            {const float value=sample(song,offset+i,channel);buffer.setSample(channel,i,value);peak=std::max(peak,std::abs(value));squared+=value*value;}
            record.peaks.push_back(peak);record.waveformRms.push_back((float)std::sqrt(squared/(count*2)));
            if(!writer->writeFromAudioSampleBuffer(buffer,0,count))return fail("Demo audio could not be written completely.");
        }
        writer.reset();juce::AudioFormatManager formats;formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> verified(formats.createReaderFor(file));
        if(!verified || verified->lengthInSamples!=frames || verified->sampleRate!=rate || verified->numChannels!=2)
            return fail("The newly created demo WAV is incomplete. Existing music was kept.");
        verified.reset();record.source={audioDigest(file),file.getSize(),rate,frames,2};
        if(record.source.sha256.isEmpty())return fail("Cannot verify the newly created demo audio.");
        hashes.add(record.source.sha256);
        // These are authored source facts, stored as manual corrections rather
        // than claimed detector confidence or human listening acceptance.
        record.manualBpm=120;record.manualBeatOrigin=0;record.manualKey=0;
    }
    if(stopped(cancelled))return fail("Demo preparation cancelled. Your music was kept.");
    juce::var info(new juce::DynamicObject());info.getDynamicObject()->setProperty("format","mixworks-original-demo-v1");
    info.getDynamicObject()->setProperty("hashes",hashes);info.getDynamicObject()->setProperty("description","Three original locally synthesised tutorial tracks; application Personal Beta Licence applies.");
    const auto stagedManifest=stage.folder.getChildFile("original-demo.json");stage.created.add(stagedManifest);
    if(!stagedManifest.replaceWithText(juce::JSON::toString(info)) || destination.exists() || !stage.folder.moveFileTo(destination))return fail("Cannot finish the optional demo folder. Existing files were kept.");
    stage.created.clear();stage.folder={};
    for(int song=0;song<3;++song)
    {
        const auto file=destination.getChildFile(names[song]);
        const auto error=writeAnalysisRecord(file,records[(size_t)song]);
        if(error.isNotEmpty())return fail("Demo files are ready, but their authored grid could not be saved: "+error);
        if(!readAnalysisRecord(file,records[(size_t)song].source).reusableAutomatic)
            return fail("Demo files are ready, but their authored grid could not be verified. Import your own music instead.");
        result.files.add(file);
    }
    return result;
}
}
