#include "Model.h"
#include "Engine.h"
#include "Beatmatch.h"
#include "NeuralRhythm.h"
#include "VerificationMetrics.h"
#include <juce_cryptography/juce_cryptography.h>
#include <limits>
#include <thread>

namespace mixworks
{
void runOriginalPlaybackTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    const auto root = report.getParentDirectory().getChildFile("original-playback"); root.createDirectory();
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    const auto args = juce::JUCEApplicationBase::getCommandLineParameterArray();
    if (args.contains("--seek-continuity-only"))
    {
        constexpr int rate = 8000, frames = 24 * rate, block = 128;
        auto resident = std::make_shared<Media>(); resident->sampleRate = rate; resident->samples.setSize(1,frames);
        resident->manualBpm = 120; resident->manualBeatOrigin = 0;
        for (int i = 0; i < frames; ++i) resident->samples.setSample(0,i,.12f*(float)std::sin(i*.137));
        const auto file = root.getChildFile("seek-source.wav");
        { juce::WavAudioFormat format; std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
          auto writer = format.createWriterFor(stream,juce::AudioFormatWriterOptions().withSampleRate(rate).withNumChannels(1).withBitsPerSample(24));
          check(writer && writer->writeFromAudioSampleBuffer(resident->samples,0,frames),"write bounded seek fixture"); }
        // Read the encoded samples independently; this never warms a live identity.
        { std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
          check(reader && reader->read(&resident->samples,0,frames,0,true,false),"independent seek PCM oracle reads encoded source"); }
        juce::Array<juce::var> observations;
        for (int mode = 0; mode < 6; ++mode)
        {
            Engine live({},false), oracle({},false); live.prepareRender(rate,512); oracle.prepareRender(rate,512);
            live.project.masterDb = oracle.project.masterDb = 0;
            auto streamed = resident->catalogueCopy(); streamed->file = file;
            streamed->analysisSource.frames = frames; streamed->analysisSource.channels = 1;
            streamed->original = std::make_shared<OriginalAudio>(file,rate,1,frames);
            Clip clip; clip.media = streamed; clip.length = 16; clip.rate = mode ? 1.1 : 1.; clip.pitchSemitones = mode ? 2 : 0;
            clip.reversed = mode == 2; clip.loopSpan = mode == 3 ? 1.3 : 0; clip.loopPhase = .7; clip.audioSlip = mode == 3 ? .12 : 0;
            if (mode == 4)
            {
                auto stems = std::make_shared<StemAssets>();
                for (auto& source : stems->sources) source = streamed;
                clip.stems = stems;
            }
            clip.elasticPlayback = mode != 0;
            if (clip.elasticPlayback) clip.elasticSource = std::make_shared<const ElasticSource>(clip);
            live.project.clips = {clip};
            if (mode == 5)
            {
                auto second = clip; second.id = juce::Uuid().toString(); second.track = 1; second.offset = .4; second.pitchSemitones = -2;
                second.elasticSource = std::make_shared<const ElasticSource>(second); live.project.clips.push_back(second);
                auto third = second; third.id = juce::Uuid().toString(); third.track = 2; third.offset = .9;
                third.elasticSource = std::make_shared<const ElasticSource>(third); live.project.clips.push_back(third);
            }
            oracle.project = live.project;
            for (auto& c : oracle.project.clips)
            {
                c.media = resident;
                if (c.stems) { auto stems = std::make_shared<StemAssets>(); for (auto& source : stems->sources) source = resident; c.stems = stems; }
                if (c.elasticPlayback) c.elasticSource = std::make_shared<const ElasticSource>(c);
            }
            live.prepareClipProcessing(); oracle.prepareClipProcessing();
            check(live.seek(1.).wasOk(),"prepare initial playback / scenario " + juce::String(mode)); live.playing = true;
            auto prepared = live.prepareSeek(9.037); const auto windows = prepared.windowsRendered();
            check(live.playing && live.position == 1. && (mode == 0 || windows > 0),"seek preparation retains current clock and warms destination windows / " + juce::String(mode));
            juce::AudioBuffer<float> actual(2,block), expected(2,block); juce::MidiBuffer midi;
            const auto callbacks = live.callbackDiagnostics.snapshot(); const auto waits = live.streamWaitCallbacks.load();
            std::thread callback([&] { live.audioDeviceIOCallbackWithContext(nullptr,0,actual.getArrayOfWritePointers(),2,block,{}); }); callback.join();
            oracle.render(expected,block,1.,midi); float oldError = 0;
            for (int ch=0;ch<2;++ch) for (int i=0;i<block;++i) oldError = std::max(oldError,std::abs(actual.getSample(ch,i)-expected.getSample(ch,i)));
            check(oldError < 2.e-6f && actual.getMagnitude(0,block) > .001f && std::abs(live.position-(1.+block/double(rate))) < 1.e-9,
                "old programme still renders exact PCM before seek commit / " + juce::String(mode));
            check(live.commitSeek(std::move(prepared)).wasOk() && live.playing && live.position == 9.037,"prepared seek publishes destination without stopping / " + juce::String(mode));
            oracle.resetPlugins(); oracle.render(expected,block,9.037,midi);
            std::thread destination([&] { live.audioDeviceIOCallbackWithContext(nullptr,0,actual.getArrayOfWritePointers(),2,block,{}); }); destination.join();
            float error = 0; for (int ch=0;ch<2;++ch) for (int i=0;i<block;++i) error = std::max(error,std::abs(actual.getSample(ch,i)-expected.getSample(ch,i)));
            check(error < 2.e-6f && actual.getMagnitude(0,block) > .001f && std::abs(live.position-(9.037+block/double(rate))) < 1.e-9,
                "first destination callback matches independent PCM and advances exact sample clock / " + juce::String(mode));
            check(live.elasticWindowsRendered() == windows && live.streamWaitCallbacks == waits && live.callbackDiagnostics.snapshot().lockMisses == callbacks.lockMisses,
                "destination callback needs no new stretch windows, buffering callbacks or lock skips / " + juce::String(mode));
            auto* fact = new juce::DynamicObject(); fact->setProperty("scenario",mode); fact->setProperty("maxPcmError",error);
            fact->setProperty("peak",actual.getMagnitude(0,block)); fact->setProperty("preparedWindows",(juce::int64)windows);
            fact->setProperty("position",live.position.load()); fact->setProperty("bufferingCallbacks",(juce::int64)(live.streamWaitCallbacks.load()-waits));
            observations.add(juce::var(fact));
            if (mode <= 1)
            {
                const auto auditionRequest = live.beginTakeAuditionRequest();
                { auto preparedIntent = live.prepareSeek(3.); }
                check(!live.isTakeAuditionRequestCurrent(auditionRequest),"seek intent invalidates earlier take preparation / " + juce::String(mode));
                const auto before = live.position.load(); const auto misses = live.callbackDiagnostics.snapshot().lockMisses;
                check(live.queueSeek(3.2).wasOk() && live.queueSeek(6.2).wasOk() && live.seekPending && live.position == before,
                    "async requests return before publication and retain only latest destination / " + juce::String(mode));
                std::thread stillPlaying([&] { live.audioDeviceIOCallbackWithContext(nullptr,0,actual.getArrayOfWritePointers(),2,block,{}); }); stillPlaying.join();
                oracle.render(expected,block,before,midi); float pendingError = 0;
                for (int ch=0;ch<2;++ch) for (int i=0;i<block;++i) pendingError = std::max(pendingError,std::abs(actual.getSample(ch,i)-expected.getSample(ch,i)));
                check(pendingError < 2.e-6f && actual.getMagnitude(0,block)>.001f && live.callbackDiagnostics.snapshot().lockMisses==misses,
                    "actual old-position callback remains audible while seek worker prepares / " + juce::String(mode));
                const auto deadline = juce::Time::getMillisecondCounterHiRes() + 5000;
                while (live.seekPending && juce::Time::getMillisecondCounterHiRes() < deadline)
                { live.flushMidiControls(); juce::Thread::sleep(2); }
                check(!live.seekPending && live.seekError.isEmpty() && live.position == 6.2 && live.playing,
                    "message-thread completion publishes latest asynchronous destination / " + juce::String(mode));
                live.queueSeek(7.2); live.stop(); const auto stopped = live.position.load();
                for (int i=0;i<10;++i) { live.flushMidiControls(); juce::Thread::sleep(2); }
                check(!live.seekPending && !live.playing && live.position == stopped,"Stop cancels pending asynchronous seek / " + juce::String(mode));
                live.queueSeek(7.2); live.sampleRate = 0;
                check(live.queueSeek(8.2).failed() && !live.seekPending,"invalid clock clears superseded pending seek / " + juce::String(mode));
                live.sampleRate = rate;
            }
            for (int stale = 0; stale < 6; ++stale)
            {
                auto rejected = live.prepareSeek(3.);
                if (stale == 0) live.stop();
                if (stale == 1) live.sampleRate = 16000;
                if (stale == 2) live.project.clips[0].audioSlip += .1;
                if (stale == 3) { auto newer = live.prepareSeek(4.); }
                if (stale == 5) live.beginTakeAuditionRequest();
                const auto position = live.position.load(); const bool playing = live.playing;
                const auto result = stale == 4 ? oracle.commitSeek(std::move(rejected)) : live.commitSeek(std::move(rejected));
                check(result.failed() && live.position == position && live.playing == playing,"stale/foreign prepared seek is transactional / " + juce::String(mode) + "/" + juce::String(stale));
                live.sampleRate = rate;
            }
            live.playing = false;
        }
        Engine failed({},false); failed.prepareRender(rate,512); Clip good; good.media = resident; good.length = 4;
        auto missing = resident->catalogueCopy(); missing->analysisSource.frames = frames; missing->analysisSource.channels = 1;
        missing->original = std::make_shared<OriginalAudio>(root.getChildFile("missing.wav"),rate,1,frames);
        Clip bad = good; bad.id = juce::Uuid().toString(); bad.media = missing; bad.start = 8;
        failed.project.clips = {good,bad}; failed.prepareClipProcessing(); failed.seek(1.); failed.playing = true;
        check(failed.seek(9.).failed() && failed.position == 1. && failed.playing,"unavailable destination keeps current playback and cursor");
        juce::AudioBuffer<float> retained(2,block); failed.audioDeviceIOCallbackWithContext(nullptr,0,retained.getArrayOfWritePointers(),2,block,{});
        check(retained.getMagnitude(0,block)>.001f && failed.position>1.,"failed seek leaves actual old programme output available");
        const auto cursor = failed.position.load();
        check(failed.seek(std::numeric_limits<double>::quiet_NaN()).failed() && failed.position==cursor,"nonfinite seek is refused without a clock change");
        failed.playing = false;
        check(report.getSiblingFile("seek-continuity.json").replaceWithText(juce::JSON::toString(juce::var(observations),true,17)),"retain actual destination PCM/clock/preparation observations");
        return;
    }
    if (args.contains("--transition-boundary-only"))
    {
        const int index = args.indexOf("--boundary-fixture");
        const juce::File input(index >= 0 && index + 1 < args.size() ? args[index + 1] : juce::String());
        const auto file = root.getChildFile("boundary" + input.getFileExtension());
        check(input.existsAsFile() && input.copyFileTo(file), "copy source into owned boundary fixture");
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
        check(reader && reader->lengthInSamples > OriginalAudio::blockFrames, "open boundary fixture");
        if (!reader) return;
        const int length = (int)reader->lengthInSamples, first = (length - 1) / OriginalAudio::blockFrames * OriginalAudio::blockFrames;
        juce::AudioBuffer<float> reference(2, length - first);
        check(reader->read(&reference, 0, length - first, first, true, true), "independent exact-length final source read succeeds");
        auto media = std::make_shared<Media>(); media->file = file; media->sampleRate = reader->sampleRate;
        media->analysisSource.frames = length;
        media->analysisSource.channels = (int)reader->numChannels;
        media->original = std::make_shared<OriginalAudio>(file, reader->sampleRate, (int)reader->numChannels, length);
        // Offline sample access waits for the bounded worker, without a device.
        bool exact = true;
        for (int ch=0; ch<(int)reader->numChannels; ++ch) for (int i=0; i<length-first; ++i)
            exact = exact && media->original->sample(ch, first+i) == reference.getSample(ch,i);
        check(!media->original->failed() && exact, "stream final partial block preserves valid audio instead of failing the source");
        Engine live({}, false); live.prepareRender(reader->sampleRate, 512);
        Clip outgoing; outgoing.media = media; outgoing.length = length / reader->sampleRate;
        auto incomingMedia = std::make_shared<Media>(); incomingMedia->sampleRate = reader->sampleRate;
        incomingMedia->samples.setSize(2, (int)reader->sampleRate * 2);
        for (int ch=0; ch<2; ++ch) for (int i=0; i<incomingMedia->frames(); ++i) incomingMedia->samples.setSample(ch,i,.1f);
        Clip incoming; incoming.media = incomingMedia; incoming.track = 1; incoming.start = outgoing.end() - 1; incoming.length = 2;
        live.project.clips = {outgoing, incoming}; live.prepareClipProcessing();
        const double at = first / reader->sampleRate;
        live.seek(at); live.playing = true;
        juce::AudioBuffer<float> output(2,512);
        const auto deadline = juce::Time::getMillisecondCounterHiRes() + 5000;
        while (live.playing && live.position < outgoing.end() + .1 && juce::Time::getMillisecondCounterHiRes() < deadline)
        { live.audioDeviceIOCallbackWithContext(nullptr,0,output.getArrayOfWritePointers(),2,512,{}); juce::Thread::sleep(1); }
        check(live.playing && !live.streamFailed && live.position > outgoing.end() && output.getMagnitude(0,512) > .01f,
              "actual callback remains audible and advances beyond outgoing EOF into incoming track");
        live.playing = false; return;
    }
    constexpr int rate = 44100, frames = rate * 8 + 37;
    juce::AudioBuffer<float> fixture(2, frames);
    for (int i = 0; i < frames; ++i)
    { fixture.setSample(0,i,.2f * std::sin(float(i) * .031f)); fixture.setSample(1,i,.13f * std::cos(float(i) * .049f)); }
    for (const bool flac : {false, true})
    {
        const auto file = root.getChildFile(flac ? "original.flac" : "original.wav");
        {
            juce::WavAudioFormat wav; juce::FlacAudioFormat compressed;
            auto& format = flac ? static_cast<juce::AudioFormat&>(compressed) : static_cast<juce::AudioFormat&>(wav);
            std::unique_ptr<juce::OutputStream> output = file.createOutputStream();
            auto writer = format.createWriterFor(output, juce::AudioFormatWriterOptions().withSampleRate(rate).withNumChannels(2).withBitsPerSample(24));
            check(writer && writer->writeFromAudioSampleBuffer(fixture,0,frames), "create original " + file.getFileExtension());
        }
        const auto hash = audioDigest(file);
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
        if (!reader) { check(false, "open original reference reader"); continue; }
        juce::AudioBuffer<float> reference(2, frames); reader->read(&reference,0,frames,0,true,true);
        auto stream = std::make_shared<OriginalAudio>(file, rate, 2, frames);
        bool exact = true;
        for (const int first : {0, 32760, frames-11, 65531, 17, 180000})
            for (int i = first; i < std::min(frames, first+75); ++i) for (int ch = 0; ch < 2; ++ch)
                exact = exact && stream->sample(ch,i) == reference.getSample(ch,i);
        check(exact, "original-file seeks preserve stereo samples at block boundaries and EOF " + file.getFileExtension());
        TrackAnalysisRecord record; record.source = {hash, file.getSize(), rate, frames, 2}; record.manualBpm=120; record.manualBeatOrigin=0;
        record.peaks={.2f}; record.waveformRms={.1f};
        int decodes=0;
        auto loaded=readMedia(formats,file,[&](MediaReadStage stage) { if(stage==MediaReadStage::decode) ++decodes; },
                              AnalysisCacheMode::readOnly,MediaReadPurpose::playback,&record);
        check(loaded.media && loaded.media->original && !loaded.media->cache && loaded.media->samples.getNumSamples()==0 && decodes==0,
              "timeline hydration uses original file and small saved facts without decoded copy");
        if (!loaded.media) continue;
        Engine engine({},false); engine.prepareRender(rate,512);
        Clip clip; clip.media=loaded.media; clip.length=6; clip.rate=1.2; clip.pitchSemitones=2; clip.beatmatched=true;
        engine.project.clips={clip};
        check(prepareSavedClips(engine.project).isEmpty() && engine.project.clips[0].elasticPlayback && !engine.project.clips[0].prepared,
              "tempo and pitch preparation retains no full-song output");
        engine.prepareClipProcessing();
        juce::AudioBuffer<float> output(2,512); juce::MidiBuffer midi;
        bool audible=false, finite=true;
        for (int b=0;b<80;++b)
        {
            engine.render(output,512,b*512./rate,midi);
            audible = audible || output.getMagnitude(0,512)>.01f;
            for(int ch=0;ch<2;++ch) for(int i=0;i<512;++i) finite=finite && std::isfinite(output.getSample(ch,i));
        }
        check(audible && finite, "offline original-file tempo/pitch render produces finite audible output");
        engine.seek(3.2); engine.playing=true; float* outputs[]{output.getWritePointer(0),output.getWritePointer(1)};
        const auto deadline=juce::Time::getMillisecondCounterHiRes()+5000;
        while(engine.position.load()==3.2 && juce::Time::getMillisecondCounterHiRes()<deadline)
        { engine.audioDeviceIOCallbackWithContext(nullptr,0,outputs,2,512,{}); juce::Thread::sleep(2); }
        check(engine.position.load()>3.2 && !engine.streamFailed && output.getMagnitude(0,512)>.001f,
              "seek waits for bounded read-ahead then resumes actual audio callback");
        engine.playing=false;
        // Traverse multiple disk blocks at device-sized steps. The independent
        // offline reader has a different identity so it cannot warm live blocks.
        Engine continuous({},false), referenceEngine({},false);
        continuous.prepareRender(rate,512); referenceEngine.prepareRender(rate,512);
        continuous.project.clips={clip}; prepareSavedClips(continuous.project);
        referenceEngine.project=continuous.project;
        auto independent=loaded.media->catalogueCopy();
        independent->original=std::make_shared<OriginalAudio>(file,rate,2,frames);
        referenceEngine.project.clips[0].media=independent; prepareSavedClips(referenceEngine.project);
        continuous.prepareClipProcessing(); referenceEngine.prepareClipProcessing();
        continuous.playing=true; juce::AudioBuffer<float> expectedContinuous(2,512);
        bool continuousExact=true; float maximumDifference=0; int rendered=0, waitsAfterStart=0;
        const auto continuousDeadline=juce::Time::getMillisecondCounterHiRes()+10000;
        while(rendered<400 && juce::Time::getMillisecondCounterHiRes()<continuousDeadline)
        {
            const double before=continuous.position.load();
            continuous.audioDeviceIOCallbackWithContext(nullptr,0,outputs,2,512,{});
            if(continuous.position.load()>before)
            {
                referenceEngine.render(expectedContinuous,512,before,midi);
                for(int ch=0;ch<2;++ch) for(int i=0;i<512;++i)
                {
                    const auto difference=std::abs(output.getSample(ch,i)-expectedContinuous.getSample(ch,i));
                    maximumDifference=std::max(maximumDifference,difference); continuousExact=continuousExact && difference<1.e-6f;
                }
                ++rendered;
            }
            else if(rendered) ++waitsAfterStart;
            juce::Thread::sleep(2);
        }
        check(rendered==400 && continuousExact && waitsAfterStart==0,
              "sustained callback crosses source blocks without dropouts and matches independent offline rendering: blocks="+juce::String(rendered)
                  +" waits="+juce::String(waitsAfterStart)+" maxDifference="+juce::String(maximumDifference,9));
        continuous.playing=false;
        // Compare the real callback and the offline path from identical DSP
        // reset points, including discontinuities and simultaneous tracks.
        for (int scenario=0; scenario<4; ++scenario)
        {
            auto first=clip; first.offset=.2; first.length=5;
            first.reversed=scenario==1;
            if (scenario==2) { first.loopSpan=.7; first.loopPhase=.55; }
            Engine live({},false), offline({},false); live.prepareRender(rate,512); offline.prepareRender(rate,512);
            live.project.clips={first};
            if(scenario==3) { auto second=first; second.id=juce::Uuid().toString(); second.track=1; second.offset=1; second.pitchSemitones=-2; live.project.clips.push_back(second); }
            check(prepareSavedClips(live.project).isEmpty(), "prepare bounded seek/loop/reverse/overlap scenario");
            offline.project=live.project; live.prepareClipProcessing(); offline.prepareClipProcessing();
            const double at=2.37; juce::AudioBuffer<float> expected(2,512), actual(2,512);
            offline.render(expected,512,at,midi); live.seek(at); live.playing=true;
            float* destinations[]{actual.getWritePointer(0),actual.getWritePointer(1)};
            const auto until=juce::Time::getMillisecondCounterHiRes()+5000;
            while(live.position.load()==at && juce::Time::getMillisecondCounterHiRes()<until)
            { live.audioDeviceIOCallbackWithContext(nullptr,0,destinations,2,512,{}); juce::Thread::sleep(1); }
            float difference=0;
            for(int ch=0;ch<2;++ch) for(int i=0;i<512;++i) difference=std::max(difference,std::abs(actual.getSample(ch,i)-expected.getSample(ch,i)));
            check(live.position.load()>at && expected.getMagnitude(0,512)>.001f && difference<1.e-6f,
                  "callback matches offline samples after seek/loop/reverse/overlap " + juce::String(scenario));
            live.playing=false;
        }
        // More retained source objects than pool slots must recycle buffers,
        // not attach a full decoded song to every catalogue/Undo identity.
        std::vector<std::shared_ptr<OriginalAudio>> owners;
        for(int i=0;i<OriginalAudio::poolBlocks+12;++i)
        {
            auto item=std::make_shared<OriginalAudio>(file,rate,2,frames);
            exact=exact && item->sample(0,137)==reference.getSample(0,137); owners.push_back(std::move(item));
        }
        check(exact && owners.front()->sample(1,137)==reference.getSample(1,137),
              "bounded pool evicts and reloads correctly with more source owners than buffer slots");
        check(audioDigest(file)==hash && root.findChildFiles(juce::File::findFiles,true,"*.mwpcm").isEmpty(),
              "original bytes unchanged and playback creates no expanded PCM files");
        auto changed=std::make_shared<OriginalAudio>(file,rate,2,frames);
        (void)changed->sample(0,0);
        file.setLastModificationTime(file.getLastModificationTime()+juce::RelativeTime::seconds(3));
        (void)changed->sample(0,OriginalAudio::blockFrames*3);
        check(changed->failed(), "changed original is rejected at the next read instead of mixing identities");
        auto failedMedia=loaded.media->catalogueCopy(); failedMedia->original=changed;
        Engine refused({},false); refused.prepareRender(rate,512);
        auto failedClip=clip; failedClip.media=failedMedia; failedClip.elasticSource.reset(); refused.project.clips={failedClip}; refused.playing=true;
        check(!refused.requestPlayback(0,512) && refused.streamFailed && !refused.playing && !refused.streamBuffering,
              "unreadable original stops playback explicitly instead of buffering forever");
        auto stems=std::make_shared<StemAssets>(); stems->sources.fill(loaded.media); stems->sources[0]=failedMedia;
        failedClip.media=loaded.media; failedClip.stems=stems; failedClip.stemsEnabled=true;
        refused.project.clips={failedClip}; refused.streamFailed=false; refused.playing=true;
        check(!refused.requestPlayback(0,512) && refused.streamFailed && !refused.playing,
              "unreadable enabled stem stops playback explicitly");

    }
    const auto stats=OriginalAudio::statistics();
    check(stats.bytes==32LL*1024*1024, "shared decoded working set is 32 MiB regardless of source durations");
    check(stats.misses==0, "admitted audio callbacks never substitute missing source samples");
    juce::DynamicObject::Ptr facts=new juce::DynamicObject(); facts->setProperty("poolBytes",stats.bytes);
    facts->setProperty("blockReads",(juce::int64)stats.reads); facts->setProperty("callbackSampleMisses",(juce::int64)stats.misses);
    report.getSiblingFile("original-playback.json").replaceWithText(juce::JSON::toString(juce::var(facts.get())));
}
namespace
{
class ControlledReader final : public juce::AudioFormatReader
{
public:
    ControlledReader(std::unique_ptr<juce::AudioFormatReader> source, int failRead = 0, int corruptRead = 0,
                     std::shared_ptr<std::atomic<bool>> cancellation = {})
        : AudioFormatReader(nullptr, "controlled real WAV"), inner(std::move(source)), fail(failRead), corrupt(corruptRead), cancelled(std::move(cancellation))
    {
        sampleRate = inner->sampleRate; lengthInSamples = inner->lengthInSamples; numChannels = inner->numChannels;
        bitsPerSample = 32; usesFloatingPointData = true;
    }
    bool readSamples(int* const* destination, int destinationChannels, int startOffset, juce::int64 start, int count) override
    {
        ++reads; if (reads == fail) return false;
        juce::AudioBuffer<float> decoded(destinationChannels, count);
        if (!inner->read(&decoded, 0, count, start, true, true)) return false;
        for (int channel = 0; channel < destinationChannels; ++channel) if (destination[channel])
            std::copy_n(decoded.getReadPointer(channel), count, reinterpret_cast<float*>(destination[channel]) + startOffset);
        if (reads == corrupt && destination[0]) reinterpret_cast<float*>(destination[0])[startOffset] = std::numeric_limits<float>::quiet_NaN();
        if (cancelled && reads == 2) *cancelled = true;
        return true;
    }
    int reads = 0;
private:
    std::unique_ptr<juce::AudioFormatReader> inner;
    int fail, corrupt;
    std::shared_ptr<std::atomic<bool>> cancelled;
};

}
void runSourceStreamingTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report, const juce::File& largeSource)
{
    const auto root = report.getParentDirectory().getChildFile("streaming-" + juce::Uuid().toString()); root.createDirectory();
    const auto temporary = root.getChildFile("owned-temporary"); temporary.createDirectory();
    const auto source = root.getChildFile("actual-stereo.wav"); juce::AudioBuffer<float> generated(2, 100001);
    for (int i = 0; i < generated.getNumSamples(); ++i)
    {
        generated.setSample(0, i, .25f * std::sin(i * .00731f)); generated.setSample(1, i, -.125f * std::cos(i * .00479f));
    }
    {
        juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> output = source.createOutputStream();
        auto writer = wav.createWriterFor(output, juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(2).withBitsPerSample(24));
        check(writer && writer->writeFromAudioSampleBuffer(generated, 0, generated.getNumSamples()), "create real stereo WAV across several mapped-decode chunks");
    }
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    auto open = [&] { return std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(source)); };
    auto original = open(); check(original != nullptr, "ordinary production WAV reader opens the boundary fixture"); if (!original) return;
    juce::AudioBuffer<float> reference(2, generated.getNumSamples()); const auto readReference = original->read(&reference, 0, reference.getNumSamples(), 0, true, true);
    juce::String error; auto mapped = AudioCache::decode(*original, 2, reference.getNumSamples(), error, {}, temporary);
    bool exact = readReference && mapped && mapped->mapping && mapped->mapping->getSize() == (size_t) reference.getNumSamples() * 2 * sizeof(float);
    if (mapped) for (int channel = 0; channel < 2; ++channel) for (int i = 0; i < reference.getNumSamples(); ++i)
        exact = exact && mapped->channelData(channel)[i] == reference.getSample(channel, i);
    check(exact, "actual streaming decode preserves every stereo sample across chunk boundaries and the final partial chunk: " + error);
    const auto mappedFile = mapped ? mapped->file : juce::File(); mapped.reset();
    check(mappedFile != juce::File() && !mappedFile.exists(), "released session mapping removes only its owned decoded file");
    for (int kind = 0; kind < 3; ++kind)
    {
        auto cancel = kind == 2 ? std::make_shared<std::atomic<bool>>(false) : nullptr;
        ControlledReader reader(open(), kind == 0 ? 2 : 0, kind == 1 ? 2 : 0, cancel);
        auto failed = AudioCache::decode(reader, 2, reference.getNumSamples(), error, cancel, temporary);
        check(!failed && reader.reads == 2 && error.isNotEmpty() && temporary.findChildFiles(juce::File::findFiles, false).isEmpty(),
              kind == 0 ? "mid-stream decoder failure cleans partial owned PCM" : kind == 1 ? "mid-stream non-finite data rejects and cleans partial owned PCM" : "mid-stream cancellation cleans partial owned PCM without finishing the source");
    }
    auto cancel = std::make_shared<std::atomic<bool>>(true);
    auto cancelled = AudioCache::decode(*original, 2, reference.getNumSamples(), error, cancel, temporary);
    check(!cancelled && error.containsIgnoreCase("cancel") && temporary.findChildFiles(juce::File::findFiles, false).isEmpty(), "pre-cancelled mapping allocates no source-sized output");
    auto invalid = AudioCache::decode(*original, 3, reference.getNumSamples(), error, {}, temporary);
    check(!invalid && error.isNotEmpty() && temporary.findChildFiles(juce::File::findFiles, false).isEmpty(), "unsupported dimensions reject before creating decoded files");
    const auto obstruction = root.getChildFile("keep-existing-file"); obstruction.replaceWithText("kept output obstruction");
    invalid = AudioCache::decode(*original, 2, reference.getNumSamples(), error, {}, obstruction);
    check(!invalid && obstruction.loadFileAsString() == "kept output obstruction", "blocked decode folder preserves the existing unrelated file");

    const auto manifestFile = largeSource.getSiblingFile("owned-90-minute-manifest.json");
    const auto manifest = manifestFile.getSize() <= 256 * 1024 ? juce::JSON::parse(manifestFile) : juce::var();
    check(largeSource.existsAsFile() && manifest.isObject() && (juce::int64) manifest["frames"] == 238140000
          && (int) manifest["sampleRate"] == 44100 && (int) manifest["channels"] == 2 && largeSource.getSize() == 1428840044,
          "explicit owned 90-minute capacity fixture and dimensions are available");
    if (!largeSource.existsAsFile() || !manifest.isObject()) return;
    const auto sourceHash = juce::SHA256(largeSource).toHexString();
    check(sourceHash == manifest["sha256"].toString(), "owned long source matches its immutable creation digest");
    if (sourceHash != manifest["sha256"].toString()) return;
    const auto before = readVerificationMetrics(); const auto began = juce::Time::getMillisecondCounterHiRes();
    auto loaded = readMedia(formats, largeSource, {}, AnalysisCacheMode::readOnly);
    const auto elapsed = juce::Time::getMillisecondCounterHiRes() - began; const auto after = readVerificationMetrics();
    check(loaded.media && loaded.media->original && !loaded.media->cache && loaded.media->samples.getNumSamples() == 0 && loaded.media->frames() == 238140000
          && loaded.media->channels() == 2 && loaded.media->duration() == 5400, "production importer accepts 90-minute stereo audio without retaining a whole decoded RAM buffer: " + loaded.error);
    auto* measurements = new juce::DynamicObject(); measurements->setProperty("source", largeSource.getFullPathName()); measurements->setProperty("sourceSha256", sourceHash);
    measurements->setProperty("importMilliseconds", elapsed); measurements->setProperty("before", verificationMetricsJson(before)); measurements->setProperty("after", verificationMetricsJson(after));
    // Counter definitions: https://learn.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-process_memory_counters_ex
    if (loaded.media)
    {
        const auto& media = *loaded.media; bool pcmMatches = true;
        for (int i = 0; i < media.frames(); i += 44100 * 29)
        {
            const bool impulse = i % 44100 == 0;
            pcmMatches = pcmMatches && media.sample(0,i) == (impulse ? .25f : 0) && media.sample(1,i) == (impulse ? -.125f : 0);
        }
        check(pcmMatches, "bounded seeks across the actual 90-minute original match its independently encoded impulses");
        check(media.playbackCacheWarning.isEmpty() && media.analysis.rhythmIssue.isNotEmpty(), "large recording plays without temporary PCM while the bounded analysis limit is explicit");
        check(writeAnalysisRecord(largeSource, media.analysisRecord()).isEmpty(), "long-source waveform and unavailable-analysis facts have a small durable record");
        auto warmLong=readMedia(formats,largeSource,{},AnalysisCacheMode::readOnly);
        check(warmLong.media && warmLong.media->analysisFromCache && warmLong.media->original
            && warmLong.media->peaks==media.peaks && warmLong.media->analysis.rhythmMethod=="bounded-scan",
            "reopening a long recording reuses its waveform without repeating the scan");
        measurements->setProperty("frames", media.frames()); measurements->setProperty("mappedBytes", (juce::int64) 0);
        measurements->setProperty("retainedSampleBufferFrames", media.samples.getNumSamples()); measurements->setProperty("playbackWarning", media.playbackCacheWarning);
        measurements->setProperty("rhythmIssue", media.analysis.rhythmIssue);
        loaded.media.reset();
        check(largeSource.existsAsFile(), "releasing original-file playback preserves the original recording");
    }
   #if JUCE_WINDOWS
    check(before.available && after.available && after.peakCommitted - before.peakCommitted < 1024LL * 1024 * 1024,
          "Windows peak private-commit growth stays below the old 1 GiB allocation ceiling while decoding 1.905 GB of float PCM");
   #endif
    auto preCancelled = readMedia(formats, largeSource, {}, AnalysisCacheMode::readOnly, MediaReadPurpose::playback, nullptr, cancel);
    check(!preCancelled.media && preCancelled.error.containsIgnoreCase("cancel"), "production cancellation rejects a long import before decoding it");
    const auto cancelledAnalysis = analyseMusic(reference, 8000, cancel);
    check(cancelledAnalysis.rhythmIssue.containsIgnoreCase("cancel") && cancelledAnalysis.tempoConfidence == 0 && cancelledAnalysis.downbeatOrigin < 0,
          "cancelled analysis preserves an explicit unavailable result instead of a partially confident grid");
    RhythmActivations stale; stale.beatLogits = {99};
    const auto cancelledNeural = analyseNeuralRhythm(reference, 8000, &stale, cancel);
    check(cancelledNeural.error.containsIgnoreCase("cancel") && stale.beatLogits.empty(), "pre-cancelled neural work clears previous research output before acquiring the model");
    juce::AudioBuffer<float> tooShort(1, 1); tooShort.clear();
    const auto shortNeural = analyseNeuralRhythm(tooShort, 44100);
    check(shortNeural.error.isNotEmpty() && shortNeural.beats.empty(), "unsupported short reflection input fails explicitly instead of indexing outside PCM");
    juce::AudioBuffer<float> longAnalysis(1, 8000 * 600); longAnalysis.clear();
    for (int i = 0; i < longAnalysis.getNumSamples(); i += 4000) longAnalysis.setSample(0, i, .25f);
    auto cancelOwner = std::make_shared<std::atomic<bool>>(false), cancelWaiter = std::make_shared<std::atomic<bool>>(false);
    juce::WaitableEvent enteredOwner, enteredWaiter, ownerDone, waiterDone; NeuralRhythm ownerResult, waiterResult;
    std::thread owner([&] { enteredOwner.signal(); ownerResult = analyseNeuralRhythm(longAnalysis, 8000, nullptr, cancelOwner); ownerDone.signal(); });
    const bool ownerStarted = enteredOwner.wait(1000); juce::Thread::sleep(20);
    std::thread waiter([&] { enteredWaiter.signal(); waiterResult = analyseNeuralRhythm(longAnalysis, 8000, nullptr, cancelWaiter); waiterDone.signal(); });
    const bool waiterStarted = enteredWaiter.wait(1000); juce::Thread::sleep(20); *cancelWaiter = true;
    const bool waiterStopped = waiterDone.wait(2000); *cancelOwner = true; const bool ownerStopped = ownerDone.wait(2000);
    owner.join(); waiter.join();
    check(ownerStarted && waiterStarted && waiterStopped && waiterResult.error.containsIgnoreCase("cancel"), "queued neural analysis can cancel while another source owns the shared model");
    check(ownerStopped && ownerResult.error.containsIgnoreCase("cancel"), "active long neural conversion/feature/inference work cooperatively releases its model and buffers");
    check(juce::SHA256(largeSource).toHexString() == sourceHash, "all long-source success and cancellation paths preserve the original encoded audio");
    report.getSiblingFile("source-streaming-metrics.json").replaceWithText(juce::JSON::toString(juce::var(measurements), true));
}
}
