#include "RhythmDecoder.h"
#include "NeuralRhythm.h"
#include "Model.h"
#include "AuftaktNetwork.h"
#include <juce_cryptography/juce_cryptography.h>
#include <cmath>
#include <limits>

namespace mixworks
{
int runRhythmResearch(const juce::StringArray& args)
{
    const auto inputIndex = args.indexOf("--rhythm-research"), reportIndex = args.indexOf("--report"), modelIndex = args.indexOf("--auftakt-model");
    const bool recovered = modelIndex >= 0, featureValues = args.contains("--feature-values"), automaticControl = args.contains("--automatic-control");
    if (args.size() != (recovered ? 6 : 4) + (featureValues ? 1 : 0) + (automaticControl ? 1 : 0) || inputIndex < 0 || reportIndex < 0 || inputIndex + 1 >= args.size() || reportIndex + 1 >= args.size()
        || (recovered && (modelIndex + 1 >= args.size() || !juce::File::isAbsolutePath(args[modelIndex + 1].unquoted())))
        || (featureValues && !recovered)
        || !juce::File::isAbsolutePath(args[inputIndex + 1].unquoted()) || !juce::File::isAbsolutePath(args[reportIndex + 1].unquoted())) return 1;
    const juce::File source(args[inputIndex + 1].unquoted()), report(args[reportIndex + 1].unquoted());
    if (!source.existsAsFile() || report.exists() || report.isSymbolicLink() || !report.hasFileExtension("json")) return 1;
    auto digest = [](const juce::File& file) { juce::FileInputStream stream(file); return stream.openedOk() ? juce::SHA256(stream).toHexString() : juce::String(); };
    const auto before = digest(source);
    const auto started = juce::Time::getMillisecondCounterHiRes();
    juce::var document(new juce::DynamicObject()); auto& row = *document.getDynamicObject();
    row.setProperty("format", "alloy-rhythm-research"); row.setProperty("version", 3);
    row.setProperty("decoder", recovered ? "alloy-auftakt-sequence-reconstruction-4" : "alloy-state-sequence-research-1"); row.setProperty("source", source.getFullPathName()); row.setProperty("sourceSha256", before);
    row.setProperty("status", "experimental");
    row.setProperty("scope", recovered
        ? "Recovered AUFTAKT frontend/filterbanks, three bidirectional LSTMs, state and transition construction. Alloy finite-file support/padding, tie and floating-point equivalence remain unverified. Not admitted to automatic analysis or sessions."
        : "Independent decoder using Beat This activations. AUFTAKT topology, features and weights are not reproduced. Not admitted to automatic analysis or sessions.");
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(source));
    juce::String error; RhythmActivations activations; DecodedRhythm decoded; AuftaktFeatures features; AuftaktObservations observations;
    if (!reader || before.isEmpty()) error = "Cannot read source audio.";
    else if (!std::isfinite(reader->sampleRate) || reader->sampleRate < 8000 || reader->numChannels < 1 || reader->numChannels > 2
             || reader->lengthInSamples < 2 || reader->lengthInSamples > std::numeric_limits<int>::max()
             || (uint64_t) reader->lengthInSamples > (uint64_t) maxDecodedBytes / sizeof(float) / reader->numChannels)
        error = "Source PCM exceeds the existing decoding budget or has an invalid format.";
    else
    {
        juce::AudioBuffer<float> pcm((int) reader->numChannels, (int) reader->lengthInSamples);
        if (!reader->read(&pcm, 0, pcm.getNumSamples(), 0, true, true)) error = "Incomplete source decoding.";
        else
        {
            row.setProperty("sampleRate", reader->sampleRate); row.setProperty("frames", (juce::int64) reader->lengthInSamples);
            row.setProperty("channels", (int) reader->numChannels);
            if (automaticControl)
            {
                RhythmDiagnostics diagnostics; const auto automatic = analyseMusic(pcm, reader->sampleRate, {}, &diagnostics);
                auto* control = new juce::DynamicObject();
                control->setProperty("bpm", automatic.bpm); control->setProperty("beatOrigin", automatic.beatOrigin);
                control->setProperty("tempoConfidence", automatic.tempoConfidence); control->setProperty("variableTempo", automatic.variableTempo);
                control->setProperty("downbeatOrigin", automatic.downbeatOrigin); control->setProperty("downbeatMethod", automatic.downbeatMethod);
                control->setProperty("downbeatConfidence", automatic.downbeatConfidence); control->setProperty("gridErrorMs", automatic.gridErrorMs);
                control->setProperty("rhythmMethod", automatic.rhythmMethod); control->setProperty("rhythmIssue", automatic.rhythmIssue);
                juce::Array<juce::var> windows, physical;
                for (const auto& evidence : diagnostics.localFits)
                {
                    auto* entry = new juce::DynamicObject();
                    entry->setProperty("start", evidence.sourceStart); entry->setProperty("end", evidence.sourceEnd);
                    entry->setProperty("examinedPhysicalSeconds", evidence.examinedPhysicalSeconds); entry->setProperty("physicalEvents", evidence.physicalEvents);
                    entry->setProperty("hasPhysicalFit", evidence.hasPhysicalFit); entry->setProperty("conflict", evidence.conflict);
                    entry->setProperty("physicalBpm", evidence.hasPhysicalFit ? juce::var(evidence.physicalBpm) : juce::var());
                    entry->setProperty("physicalOrigin", evidence.hasPhysicalFit ? juce::var(evidence.physicalOrigin) : juce::var());
                    entry->setProperty("physicalAgreement", evidence.hasPhysicalFit ? juce::var(evidence.physicalAgreement) : juce::var());
                    entry->setProperty("physicalCoverage", evidence.hasPhysicalFit ? juce::var(evidence.physicalCoverage) : juce::var());
                    entry->setProperty("physicalInliers", evidence.hasPhysicalFit ? juce::var(evidence.physicalInliers) : juce::var());
                    entry->setProperty("globalPhysicalSupport", evidence.globalPhysicalSupport ? juce::var(*evidence.globalPhysicalSupport) : juce::var());
                    entry->setProperty("neuralAgreement", evidence.hasPhysicalFit ? juce::var(evidence.neuralAgreement) : juce::var());
                    entry->setProperty("neuralCoverage", evidence.hasPhysicalFit ? juce::var(evidence.neuralCoverage) : juce::var());
                    windows.add(juce::var(entry));
                }
                for (const auto& event : diagnostics.physicalTransients)
                { juce::Array<juce::var> point; for (double value : event) point.add(value); physical.add(point); }
                control->setProperty("localFits", windows); control->setProperty("physicalTransients", physical);
                control->setProperty("scope", "Automatic source-grid facts and independent physical observations; no vendor or musical ground truth implied.");
                row.setProperty("automaticControl", juce::var(control));
            }
            if (recovered)
            {
                // Full JSON expansion, including JUCE's UTF-16 output string,
                // must fit a 256 MiB report budget before retaining feature rows.
                const size_t featureBudget = featureValues ? 12 * 1024 * 1024 : 256 * 1024 * 1024;
                features = extractAuftaktFeatures(pcm, reader->sampleRate, featureBudget);
                error = features.error;
                if (error.isEmpty())
                {
                    observations = evaluateAuftaktNetwork(features, juce::File(args[modelIndex + 1].unquoted()));
                    error = observations.error; activations.framesPerSecond = 100;
                    activations.classProbabilities = observations.probabilities;
                    activations.signalPresent = features.signalPresent;
                }
            }
            else error = analyseNeuralRhythm(pcm, reader->sampleRate, &activations).error;
            if (error.isEmpty()) { RhythmDecoderSettings settings; settings.recoveredAuftaktTopology = recovered; decoded = decodeRhythm(activations, settings); error = decoded.error; }
        }
    }
    if (digest(source) != before) error = "Source changed during the probe; these results must not be compared.";
    const auto executable = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    row.setProperty("executableSha256", digest(executable));
    row.setProperty("modelSha256", digest(recovered ? juce::File(args[modelIndex + 1].unquoted()) : executable.getSiblingFile("beat-this-final0.onnx")));
    row.setProperty("observationEngine", recovered ? "auftakt-4.2.2-recovered" : "beat-this-final0");
    if (recovered)
    {
        if (error.isEmpty() && row.getProperty("modelSha256").toString() != "01fbd5a2c5d958ff12d9169de72f6f3e6540dde7af97efedfbc94231817eb917")
            error = "Recovered model changed during the probe.";
        row.setProperty("packagedNetworkTestMaximumError", observations.packagedTestMaximumError);
        row.setProperty("featureFrames", (juce::int64) features.frames()); row.setProperty("featureDimensions", features.dimensions);
        row.setProperty("featureHopSamples", features.hop); row.setProperty("featureSampleRate", features.sampleRate);
        row.setProperty("featurePadding", "centred source frame zero; zero extension at file boundaries; vendor finalization equivalence unverified");
        row.setProperty("spectralSignalPresent", features.signalPresent);
        row.setProperty("supportPolicy", "Full source duration, including quiet boundaries; Alloy suppresses the grid only for entirely zero spectral features");
        row.setProperty("tempoProposals", 50); row.setProperty("tempoTransitionCoefficient", 180); row.setProperty("sameMeterBarTransition", 0.95);
        juce::Array<juce::var> probabilities, values;
        for (const auto& p : observations.probabilities) { juce::Array<juce::var> entry; for (auto v : p) entry.add(v); probabilities.add(entry); }
        row.setProperty("classProbabilities", probabilities);
        row.setProperty("nonbeatObservationDivisor", 39);
        if (featureValues) { for (auto v : features.values) values.add(v); row.setProperty("featureValues", values); }
    }
    row.setProperty("error", error); row.setProperty("elapsedMs", juce::Time::getMillisecondCounterHiRes() - started);
    row.setProperty("activationFramesPerSecond", activations.framesPerSecond);
    juce::Array<juce::var> beats, scores, beatLogits, downbeatLogits;
    for (const auto& event : decoded.beats)
    {
        juce::var item(new juce::DynamicObject()); auto* object = item.getDynamicObject();
        object->setProperty("seconds", event.seconds); object->setProperty("position", event.position); object->setProperty("meter", event.meter);
        object->setProperty("beatActivation", event.beatActivation); object->setProperty("downbeatActivation", event.downbeatActivation); beats.add(item);
    }
    for (const auto& score : decoded.meterScores)
    {
        juce::var item(new juce::DynamicObject()); item.getDynamicObject()->setProperty("meter", score.first);
        item.getDynamicObject()->setProperty("logPathScore", score.second); scores.add(item);
    }
    for (auto logit : activations.beatLogits) beatLogits.add(logit);
    for (auto logit : activations.downbeatLogits) downbeatLogits.add(logit);
    row.setProperty("beats", beats); row.setProperty("meterScores", scores); row.setProperty("beatLogits", beatLogits); row.setProperty("downbeatLogits", downbeatLogits);
    row.setProperty("minimumBpm", 60); row.setProperty("maximumBpm", 240);
    if (!recovered) row.setProperty("tempoChangeCost", 20);
    row.setProperty("meterScoresScope", recovered ? "ending-state meter; decoded path may change meter at bar boundaries" : "fixed-meter path hypotheses");
    row.setProperty("traceBudgetBytes", (juce::int64) RhythmDecoderSettings().maximumTraceBytes);
    return report.getParentDirectory().createDirectory().wasOk() && report.replaceWithText(juce::JSON::toString(document)) && error.isEmpty() ? 0 : 1;
}
}
