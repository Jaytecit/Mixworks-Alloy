#include "AuftaktNetwork.h"
#include <juce_cryptography/juce_cryptography.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace mixworks
{
namespace
{
constexpr int units = 25, gates = 4 * units;
constexpr const char* carrierHash = "01fbd5a2c5d958ff12d9169de72f6f3e6540dde7af97efedfbc94231817eb917";
std::vector<float> tensor(const juce::var& value, size_t count)
{
    if (!value.isArray()) throw std::runtime_error("Invalid recovered tensor carrier.");
    juce::String encoded;
    for (const auto& part : *value.getArray())
    {
        if (!part.isString()) throw std::runtime_error("Invalid recovered tensor chunk.");
        encoded += part.toString();
    }
    juce::MemoryOutputStream bytes;
    if (!juce::Base64::convertFromBase64(bytes, encoded) || bytes.getDataSize() != count * sizeof(float))
        throw std::runtime_error("Invalid recovered tensor dimensions.");
    std::vector<float> result(count);
    for (size_t i = 0; i < count; ++i)
    {
        const uint32_t bits = juce::ByteOrder::littleEndianInt((const char*) bytes.getData() + i * sizeof(float));
        std::memcpy(&result[i], &bits, sizeof(float));
        if (!std::isfinite(result[i])) throw std::runtime_error("Non-finite recovered tensor.");
    }
    return result;
}
float sigmoid(float x) { return x >= 0 ? 1 / (1 + std::exp(-x)) : std::exp(x) / (1 + std::exp(x)); }
struct Direction { std::vector<float> weights, recurrent, bias; };
struct Layer { Direction forward, backward; int inputs = 0; };
struct Network
{
    std::array<Layer, 3> layers;
    std::vector<float> dense, bias;
    explicit Network(const juce::var& document)
    {
        const auto parameters = document["trainable_params"];
        for (size_t i = 0; i < layers.size(); ++i)
        {
            auto& layer = layers[i]; layer.inputs = i == 0 ? 320 : 50;
            const auto row = parameters[juce::Identifier(i == 0 ? "bidirectional" : "bidirectional_" + juce::String((int) i))];
            auto load = [&](const juce::String& prefix, Direction& direction)
            {
                direction.weights = tensor(row[juce::Identifier(prefix + "_weights")], (size_t) layer.inputs * gates);
                direction.recurrent = tensor(row[juce::Identifier(prefix + "_recurrent_weights")], units * gates);
                direction.bias = tensor(row[juce::Identifier(prefix + "_bias")], gates);
            };
            load("forward", layer.forward); load("backward", layer.backward);
        }
        dense = tensor(parameters["time_distributed"]["weights"], 50 * 3);
        bias = tensor(parameters["time_distributed"]["bias"], 3);
    }
    std::vector<std::array<float, 3>> predict(const std::vector<float>& features) const
    {
        const size_t frames = features.size() / 320;
        const std::vector<float>* input = &features;
        std::vector<float> previous, output;
        for (const auto& layer : layers)
        {
            output.assign(frames * 50, 0);
            auto direction = [&](const Direction& weights, bool reverse)
            {
                std::array<float, units> hidden {}, cell {};
                for (size_t step = 0; step < frames; ++step)
                {
                    const size_t frame = reverse ? frames - step - 1 : step;
                    std::array<float, gates> z; std::copy(weights.bias.begin(), weights.bias.end(), z.begin());
                    for (int i = 0; i < layer.inputs; ++i)
                    {
                        const float x = (*input)[frame * layer.inputs + i];
                        const auto* w = weights.weights.data() + i * gates;
                        for (int g = 0; g < gates; ++g) z[(size_t) g] += x * w[g];
                    }
                    for (int i = 0; i < units; ++i)
                    {
                        const auto* w = weights.recurrent.data() + i * gates;
                        for (int g = 0; g < gates; ++g) z[(size_t) g] += hidden[(size_t) i] * w[g];
                    }
                    for (size_t i = 0; i < units; ++i)
                    {
                        cell[i] = sigmoid(z[i + units]) * cell[i] + sigmoid(z[i]) * std::tanh(z[i + 2 * units]);
                        hidden[i] = sigmoid(z[i + 3 * units]) * std::tanh(cell[i]);
                        output[frame * 50 + (reverse ? units : 0) + i] = hidden[i];
                    }
                }
            };
            direction(layer.forward, false); direction(layer.backward, true);
            previous.swap(output); input = &previous;
        }
        std::vector<std::array<float, 3>> result(frames);
        for (size_t frame = 0; frame < frames; ++frame)
        {
            auto& z = result[frame]; std::copy(bias.begin(), bias.end(), z.begin());
            for (int i = 0; i < 50; ++i) for (size_t k = 0; k < 3; ++k) z[k] += (*input)[frame * 50 + i] * dense[(size_t) i * 3 + k];
            const float maximum = *std::max_element(z.begin(), z.end());
            float sum = 0; for (auto& v : z) { v = std::exp(v - maximum); sum += v; }
            for (auto& v : z) v /= sum;
        }
        return result;
    }
};
}
AuftaktObservations evaluateAuftaktNetwork(const AuftaktFeatures& features, const juce::File& model, size_t maximumBytes)
{
    AuftaktObservations result;
    try
    {
        if (features.error.isNotEmpty() || features.values.size() % features.dimensions != 0
            || !std::all_of(features.values.begin(), features.values.end(), [](float v) { return std::isfinite(v); }))
            throw std::runtime_error("Invalid AUFTAKT network feature matrix.");
        // Existing feature rows + both 50-wide intermediate matrices + output,
        // parsed fixed carrier and weights. Account before duration-sized allocation.
        if (features.values.size() * (double) sizeof(float) + features.frames() * (100 + 3) * sizeof(float) + 8 * 1024 * 1024 > maximumBytes)
            throw std::runtime_error("AUFTAKT network exceeds the requested memory budget.");
        if (model.getSize() != 605824) throw std::runtime_error("The selected recovered AUFTAKT model is missing or has the wrong size.");
        juce::MemoryBlock carrier;
        if (!model.loadFileAsData(carrier) || juce::SHA256(carrier).toHexString() != carrierHash)
            throw std::runtime_error("The selected recovered AUFTAKT model has the wrong digest.");
        const auto document = juce::JSON::parse(carrier.toString());
        Network network(document);
        const auto tests = document["tests"];
        if (!tests.isArray() || tests.size() != 1) throw std::runtime_error("Missing packaged AUFTAKT model test.");
        const auto expected = tensor(tests[0]["outputs"][0]["values"], 32 * 3);
        const auto measured = network.predict(tensor(tests[0]["inputs"][0]["values"], 32 * 320));
        result.packagedTestMaximumError = 0;
        for (size_t i = 0; i < expected.size(); ++i)
            result.packagedTestMaximumError = std::max(result.packagedTestMaximumError, (double) std::abs(expected[i] - measured[i / 3][i % 3]));
        if (result.packagedTestMaximumError > 0.0001) throw std::runtime_error("Recovered AUFTAKT network failed its packaged numerical test.");
        result.probabilities = network.predict(features.values);
    }
    catch (const std::exception& error) { result.probabilities.clear(); result.error = error.what(); }
    return result;
}
}
