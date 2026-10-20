#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "NeuralRhythm.h"
#include "ContentDigest.h"
#include "ThirdParty/BeatThis/MelSpectrogram.h"
#include "ThirdParty/BeatThis/Postprocessor.h"
#include <juce_cryptography/juce_cryptography.h>
#include <onnxruntime_cxx_api.h>
#include <onnxruntime_run_options_config_keys.h>
#include <CDSPResampler.h>
#include <mutex>
#include <chrono>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace mixworks
{
namespace
{
constexpr double modelRate = 22050;
constexpr const char* modelSha256 = "c5c1466e08abdb03fdeb50668a06f244b787d564c212490482231a9cfbe9ccbd";
struct Model
{
    Ort::Env environment {ORT_LOGGING_LEVEL_WARNING, "Mixworks rhythm"};
    Ort::Session session {nullptr};
    std::timed_mutex mutex;
    Model()
    {
        const auto file = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getSiblingFile("beat-this-final0.onnx");
        juce::FileInputStream stream(file);
        juce::BufferedInputStream buffered(stream, 256 * 1024);
        if (!stream.openedOk() || contentDigest(buffered) != modelSha256 || stream.getStatus().failed())
            throw std::runtime_error("The local rhythm model is missing or does not match this release.");
        Ort::SessionOptions options;
        options.SetIntraOpNumThreads(3); options.SetInterOpNumThreads(1);
        // Retain the runtime's default arena and memory-pattern reuse across
        // equal-shaped context windows; model arithmetic and coverage stay fixed.
        options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        session = Ort::Session(environment, file.getFullPathName().toWideCharPointer(), options);
    }
};
std::vector<float> modelAudio(const PcmView& audio, double rate, const std::shared_ptr<std::atomic<bool>>& cancelled)
{
    const size_t wanted = (size_t) std::ceil(audio.getNumSamples() * modelRate / rate);
    // Bound the actual PCM + mel representation before allocating either.
    const double scratch = wanted * 4.0 + (wanted / 441.0 + 1) * (128 * 4 + sizeof(std::vector<float>)) * 2;
    if (scratch > 512.0 * 1024 * 1024) throw std::runtime_error("Rhythm analysis exceeds its 512 MiB signal/feature budget.");
    std::vector<float> result; result.reserve(wanted);
    double aa = 0, bb = 0, ab = 0;
    if (audio.getNumChannels() >= 2)
        for (int i = 0; i < audio.getNumSamples(); i += std::max(1, audio.getNumSamples() / 4096))
        { const double a = audio.getSample(0, i), b = audio.getSample(1, i); aa += a * a; bb += b * b; ab += a * b; }
    const bool antiPhase = aa * bb > 0 && ab / std::sqrt(aa * bb) < -0.2;
    r8b::CDSPResampler24 resampler(rate, modelRate, 4096);
    std::array<double, 4096> input {};
    int position = 0;
    while (result.size() < wanted)
    {
        if (cancelled && *cancelled) throw std::runtime_error("Rhythm analysis cancelled.");
        const int count = position < audio.getNumSamples() ? std::min(4096, audio.getNumSamples() - position) : 4096;
        std::fill(input.begin(), input.end(), 0);
        if (position < audio.getNumSamples()) for (int i = 0; i < count; ++i)
        {
            if (antiPhase) input[(size_t) i] = audio.getSample(aa >= bb ? 0 : 1, position + i);
            else for (int ch = 0; ch < audio.getNumChannels(); ++ch) input[(size_t) i] += audio.getSample(ch, position + i) / audio.getNumChannels();
        }
        double* output = nullptr; const int produced = resampler.process(input.data(), count, output);
        for (int i = 0; i < produced && result.size() < wanted; ++i) result.push_back((float) output[i]);
        position += count;
    }
    return result;
}
}
NeuralRhythm analyseNeuralRhythm(const PcmView& audio, double rate, RhythmActivations* research, const std::shared_ptr<std::atomic<bool>>& cancelled)
{
    NeuralRhythm result;
    if (research) *research = {};
    if (cancelled && *cancelled) { result.error = "Rhythm analysis cancelled."; return result; }
    if (!std::isfinite(rate) || rate < 8000 || audio.getNumChannels() < 1 || audio.getNumSamples() < 1)
    { result.error = "Unsupported rhythm input dimensions or sample rate."; return result; }
    try
    {
        static Model model;
        std::unique_lock<std::timed_mutex> lock(model.mutex, std::defer_lock);
        while (!lock.try_lock_for(std::chrono::milliseconds(10)))
            if (cancelled && *cancelled) throw std::runtime_error("Rhythm analysis cancelled while waiting for the model.");
        if (cancelled && *cancelled) throw std::runtime_error("Rhythm analysis cancelled.");
        const auto pcm = modelAudio(audio, rate, cancelled);
        MelSpectrogram transform; const auto spectrogram = transform.compute(pcm, [cancelled] { return cancelled && cancelled->load(); });
        const int frames = (int) spectrogram.size();
        std::vector<float> beats((size_t) frames, -1000), bars((size_t) frames, -1000);
        // Published 30-second context, 6-frame borders, keep-first overlap.
        // Reuse one tensor; never retain a second whole-piece chunk collection.
        constexpr int length = 1500, border = 6, stride = length - 2 * border;
        std::vector<int> starts;
        for (int start = -border; start < frames - border; start += stride) starts.push_back(start);
        if (frames > stride) starts.back() = frames - (length - border);
        auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        for (auto iterator = starts.rbegin(); iterator != starts.rend(); ++iterator)
        {
            if (cancelled && *cancelled) throw std::runtime_error("Rhythm analysis cancelled.");
            const int start = *iterator, count = std::min(length, frames - start + border);
            std::vector<float> tensor((size_t) count * 128, 0);
            for (int frame = 0; frame < count; ++frame)
                if (start + frame >= 0 && start + frame < frames)
                    std::copy_n(spectrogram[(size_t) (start + frame)].data(), 128, tensor.data() + (size_t) frame * 128);
            const std::array<int64_t, 3> dimensions {1, count, 128};
            auto input = Ort::Value::CreateTensor<float>(memory, tensor.data(), tensor.size(), dimensions.data(), dimensions.size());
            const char* inputs[] {"input_spectrogram"}; const char* outputs[] {"beat", "downbeat"};
            Ort::RunOptions runOptions;
            // Reuse scratch buffers throughout this source, then return unused
            // arena regions before the application begins mix preparation.
            if (std::next(iterator) == starts.rend())
                runOptions.AddConfigEntry(kOrtRunOptionsConfigEnableMemoryArenaShrinkage, "cpu:0");
            auto values = model.session.Run(runOptions, inputs, &input, 1, outputs, 2);
            for (size_t output = 0; output < 2; ++output)
            {
                if (!values[output].IsTensor() || values[output].GetTensorTypeAndShapeInfo().GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT
                    || values[output].GetTensorTypeAndShapeInfo().GetShape() != std::vector<int64_t>{1, count})
                    throw std::runtime_error("The rhythm model returned an unexpected tensor.");
                const auto* data = values[output].GetTensorData<float>(); auto& destination = output == 0 ? beats : bars;
                for (int frame = border; frame < count - border; ++frame)
                    if (start + frame >= 0 && start + frame < frames)
                    {
                        if (!std::isfinite(data[frame])) throw std::runtime_error("The rhythm model returned a non-finite prediction.");
                        destination[(size_t) (start + frame)] = data[frame];
                    }
            }
        }
        if (cancelled && *cancelled) throw std::runtime_error("Rhythm analysis cancelled.");
        Postprocessor post; const auto positions = post.process(beats, bars);
        for (auto time : positions.first)
        {
            result.beats.push_back(time);
            result.beatConfidence.push_back(1.0f / (1 + std::exp(-beats[(size_t) std::round(time * 50)])));
        }
        for (auto time : positions.second)
        {
            result.downbeats.push_back(time);
            const int centre = juce::jlimit(0, frames - 1, (int) std::round(time * 50));
            float logit = -1000;
            for (int frame = std::max(0, centre - 3); frame <= std::min(frames - 1, centre + 3); ++frame) logit = std::max(logit, bars[(size_t) frame]);
            result.downbeatConfidence.push_back(1.0f / (1 + std::exp(-logit)));
        }
        if (research) { research->beatLogits = std::move(beats); research->downbeatLogits = std::move(bars); }
    }
    catch (const std::exception& error) { result.error = juce::String("Rhythm analysis unavailable: ") + error.what(); }
    return result;
}
}
