#include "ElasticAudio.h"
#include <cmath>
#include <limits>

namespace mixworks
{
ElasticSource::ElasticSource(const Clip& value) : clip(value)
{
    clip.elasticSource.reset(); clip.prepared.reset();
}
double ElasticSource::progressAt(double time) const
{
    return clip.sourceElapsed(time - clip.start);
}
double ElasticSource::timeAtProgress(double source) const
{
    return clip.start + clip.localTimeForSource(source);
}
float ElasticSource::sample(int channel, double source, int stem) const
{
    if (clip.loopSpan > 0)
    {
        const double unwrapped = clip.loopPhase+source;
        source = std::fmod(unwrapped, clip.loopSpan);
        if (source < 0) source += clip.loopSpan;
        const double tolerance = 32*std::numeric_limits<double>::epsilon()*std::max(1.,std::abs(unwrapped));
        if (source <= tolerance || clip.loopSpan-source <= tolerance) source = 0;
    }
    const auto& media = stem >= 0 && clip.stems ? *clip.stems->sources[(size_t) stem] : *clip.media;
    double frame = (clip.offset + (clip.reversed ? clip.sourceSpan() - source : source)) * media.sampleRate - (clip.reversed ? 1 : 0);
    if (frame < 0 && frame > -64*std::numeric_limits<double>::epsilon()*std::max(1.,std::abs(source)*media.sampleRate)) frame = 0;
    if (frame < 0 || frame >= media.frames()) return 0;
    const int first = (int) frame;
    const auto* data = media.channelData(channel);
    return data[first] + (float) (frame - first) * (data[std::min(first + 1, media.frames() - 1)] - data[first]);
}
ElasticRenderer::Window& ElasticRenderer::window(const std::shared_ptr<const ElasticSource>& source, int64_t index, int stem)
{
    auto found = std::find_if(streams.begin(), streams.end(), [&](const auto& item) { return item->source == source && item->stem == stem; });
    if (found == streams.end())
    {
        // Retain every stream active in this or the previous callback. An
        // arbitrary voice cap would restart overlapping clips every block.
        found = std::find_if(streams.begin(), streams.end(), [&](const auto& item) { return item->age + 1 < clock; });
        if (found == streams.end()) { streams.push_back(std::make_unique<Stream>()); found = streams.end() - 1; }
        (*found)->stem = stem; (*found)->source = source; (*found)->nextWindow = std::numeric_limits<int64_t>::min();
        for (auto& cached : (*found)->windows) cached.index = std::numeric_limits<int64_t>::min();
    }
    auto& stream = **found; stream.age = clock;
    for (auto& cached : stream.windows) if (cached.index == index) return cached;
    auto& result = stream.windows[(size_t) ((index % (int64_t) stream.windows.size() + (int64_t) stream.windows.size()) % (int64_t) stream.windows.size())];
    auto& processor = stream.processor;
    const auto& media = *source->clip.media; const double sr = media.sampleRate;
    const int hop = (int) std::llround(sr * windowSeconds), padding = (int) std::llround(sr * 0.1);
    const int channels = media.channels(), wanted = hop + 1;
    result.pcm.setSize(channels, wanted, false, false, true); result.pcm.clear();
    int skip = 0, written = 0;
    if (index != stream.nextWindow)
    {
        // Only seeks/changed placements restart the processor. Normal playback
        // carries phase through cache boundaries; independently stretched tiles
        // would cancel sustained tones during their crossfades.
        for (auto& cached : stream.windows) cached.index = std::numeric_limits<int64_t>::min();
        processor.clear(); processor.setSampleRate((unsigned) std::llround(sr)); processor.setChannels((unsigned) channels);
        processor.setPitch(1); processor.setRate(1);
        processor.setSetting(SETTING_USE_QUICKSEEK, 0); processor.setSetting(SETTING_USE_AA_FILTER, 1);
        processor.setSetting(SETTING_SEQUENCE_MS, 40); processor.setSetting(SETTING_SEEKWINDOW_MS, 15); processor.setSetting(SETTING_OVERLAP_MS, 8);
        stream.sourceOrigin = source->progressAt((index * hop - padding) / sr); stream.inputFrames = 0; skip = padding;
    }
    else
    {
        for (int ch = 0; ch < channels; ++ch) result.pcm.setSample(ch, 0, stream.lastSample[(size_t) ch]);
        written = 1;
    }
    while (written < wanted)
    {
        const int count = (int) processor.receiveSamples(output.data(), (unsigned) std::min(128, skip + wanted - written));
        if (count > 0)
        {
            const int discard = std::min(skip, count); skip -= discard;
            for (int ch = 0; ch < channels; ++ch) for (int i = discard; i < count; ++i)
                result.pcm.setSample(ch, written + i - discard, output[(size_t) i * channels + ch]);
            written += count - discard;
        }
        else
        {
            const double a = stream.sourceOrigin + stream.inputFrames / sr, b = stream.sourceOrigin + (stream.inputFrames + 128) / sr;
            const double tempo = 128 / ((source->timeAtProgress(b) - source->timeAtProgress(a)) * sr);
            // Translating an unchanged source must not toggle SoundTouch's
            // unity bypass solely because of subtraction roundoff.
            processor.setTempo(std::abs(tempo-1.) < 1.e-10 ? 1. : tempo);
            for (int i = 0; i < 128; ++i) for (int ch = 0; ch < channels; ++ch)
                input[(size_t) i * channels + ch] = source->sample(ch, a + i / sr, stem);
            processor.putSamples(input.data(), 128); stream.inputFrames += 128;
        }
    }
    for (int ch = 0; ch < channels; ++ch) stream.lastSample[(size_t) ch] = result.pcm.getSample(ch, hop);
    result.index = index; stream.nextWindow = index + 1; ++rendered;
    return result;
}
void ElasticRenderer::reset()
{
    for (auto& stream : streams)
    {
        stream->source.reset(); stream->nextWindow = std::numeric_limits<int64_t>::min(); stream->age = 0; stream->processor.clear();
        for (auto& cached : stream->windows) cached.index = std::numeric_limits<int64_t>::min();
    }
}
void ElasticRenderer::prepare(const Clip& clip, double at, double outputRate, int frames)
{
    juce::AudioBuffer<float> scratch(2,std::max(1,frames)); scratch.clear();
    beginBlock(); add(scratch,std::max(1,frames),at,outputRate,clip);
    // Warm the processor and allocate every ring slot before callback admission.
    // Subsequent window rotation keeps these dimensions and storage.
    const int wanted=(int)std::llround(clip.media->sampleRate*windowSeconds)+1;
    for(auto& stream:streams) for(auto& cached:stream->windows)
        cached.pcm.setSize(clip.media->channels(),wanted,false,false,true);
}
void ElasticRenderer::add(juce::AudioBuffer<float>& buffer, int frames, double at, double outputRate, const Clip& clip, int stem)
{
    if (!clip.elasticSource || !clip.media->hasPlayback()) return;
    const double sr = clip.media->sampleRate;
    const int hop = (int) std::llround(sr * windowSeconds);
    const float gain = juce::Decibels::decibelsToGain(clip.gainDb);
    const auto envelope = clip.envelope();
    int64_t currentIndex = std::numeric_limits<int64_t>::min(); Window* current = nullptr;
    for (int i = 0; i < frames; ++i)
    {
        const double time = at + i / outputRate, local = time - clip.start;
        if (time < clip.start || time >= clip.end()) continue;
        const double frame = (time - clip.audioSlip) * sr; const auto index = (int64_t) std::floor(frame / hop);
        if (index != currentIndex)
        {
            current = &window(clip.elasticSource, index, stem); currentIndex = index;
        }
        const double pos = frame - index * hop; const int first = (int) pos; const float fraction = (float) (pos - first);
        const float amplitude = gain * envelope.gainAt(local);
        for (int ch = 0; ch < 2; ++ch)
        {
            const int channel = std::min(ch, clip.media->channels() - 1);
            const auto* data = current->pcm.getReadPointer(channel);
            const float value = data[first] + fraction * (data[first + 1] - data[first]);
            float level = 1;
            if (stem >= 0)
            {
                const auto& settings = clip.stemControls[(size_t) stem];
                const auto sourceTime = clip.sourceElapsed(local);
                const float db = settings.valueAt(AutomationTarget::volume, sourceTime), pan = settings.valueAt(AutomationTarget::pan, sourceTime);
                level = (db <= -60 ? 0.f : juce::Decibels::decibelsToGain(db)) * std::min(1.f, ch == 0 ? 1 - pan : 1 + pan);
            }
            buffer.addSample(ch, i, value * amplitude * level);
        }
    }
}
}
