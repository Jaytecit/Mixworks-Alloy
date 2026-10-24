#include "Engine.h"
#include "Beatmatch.h"
#include "ThirdParty/BeatThis/MelSpectrogram.h"
#include <juce_cryptography/juce_cryptography.h>
#include <cmath>

namespace mixworks
{
namespace
{
constexpr double rate = 22050;
juce::AudioBuffer<float> copyPreparedPcm(const PreparedAudio& audio)
{
    juce::AudioBuffer<float> result(audio.channels(), audio.frames());
    for (int ch = 0; ch < audio.channels(); ++ch) result.copyFrom(ch, 0, audio.channelData(ch), audio.frames());
    return result;
}
juce::AudioBuffer<float> rhythm(double bpm, double phase, double seconds = 24)
{
    juce::AudioBuffer<float> result(2, (int) (seconds * rate)); result.clear();
    for (double beat = phase; beat < seconds - 0.1; beat += 60 / bpm)
        for (int i = 0; i < (int) (rate * 0.09); ++i)
        {
            double t = i / rate;
            float value = (float) (0.7 * std::sin(2 * juce::MathConstants<double>::pi * (75 * t - 200 * t * t)) * std::exp(-t * 40));
            const int pos = (int) std::round(beat * rate) + i;
            if (pos < result.getNumSamples()) { result.addSample(0, pos, value); result.addSample(1, pos, value); }
        }
    return result;
}
juce::AudioBuffer<float> progression(int key, double tuning = 0, double harmonics = 0.18, double targetRate = rate)
{
    juce::AudioBuffer<float> audio(2, (int) (12 * targetRate)); audio.clear();
    const int roots[] {0, 5, 7, 0}; const double lengths[] {4, 2, 2, 4};
    double start = 0;
    for (int chord = 0; chord < 4; ++chord)
    {
        int third = key < 12 || chord == 2 ? 4 : 3;
        const int notes[] {48 + key % 12 + roots[chord], 60 + key % 12 + roots[chord], 60 + key % 12 + roots[chord] + third, 67 + key % 12 + roots[chord]};
        for (int i = 0; i < (int) (lengths[chord] * targetRate); ++i)
        {
            const double t = i / targetRate, envelope = std::min(1.0, t / 0.03) * std::min(1.0, (lengths[chord] - t) / 0.04);
            double value = 0;
            for (int n = 0; n < 4; ++n)
            {
                const double frequency = 440 * std::pow(2.0, (notes[n] - 69 + tuning / 100) / 12.0);
                value += (n == 0 ? 0.18 : 0.1) * (std::sin(2 * juce::MathConstants<double>::pi * frequency * t) + harmonics * std::sin(4 * juce::MathConstants<double>::pi * frequency * t));
            }
            audio.setSample(0, (int) (start * targetRate) + i, (float) (value * envelope));
            audio.setSample(1, (int) (start * targetRate) + i, (float) (value * envelope));
        }
        start += lengths[chord];
    }
    return audio;
}
double frequency(const juce::AudioBuffer<float>& samples)
{
    int crossings = 0; int start = (int) rate / 2, end = std::min(samples.getNumSamples() - 1, (int) (rate * 2.5));
    for (int i = start; i < end; ++i) if (samples.getSample(0, i) <= 0 && samples.getSample(0, i + 1) > 0) ++crossings;
    return crossings * rate / (end - start);
}
}
void runFocusedSignalTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    MusicAnalysis previous; previous.bpm = 120; previous.beatOrigin = .213; previous.tempoConfidence = .9;
    previous.downbeatOrigin = 2.213; previous.downbeatConfidence = .7; previous.downbeatMethod = "signal-structure";
    auto chord = progression(0);
    MusicAnalysis evidence;
    const auto key = retryMusicAnalysis(chord, rate, previous, false, true, {}, &evidence);
    check(key.key == 0 && key.focusedKey == 2 && key.bpm == previous.bpm && key.beatOrigin == previous.beatOrigin
        && key.downbeatOrigin == previous.downbeatOrigin, "focused harmonic pass recovers authored C major and preserves the complete pulse/bar clock; candidate=" + juce::String(evidence.key) + " score=" + juce::String(evidence.keyConfidence, 4));
    auto wrong = previous; wrong.key = 6; wrong.keyConfidence = .05;
    const auto corrected = retryMusicAnalysis(chord, rate, wrong, false, true);
    check(corrected.key == 0 && corrected.focusedKey == 2, "focused harmonic evidence can replace a weak wrong key; score=" + juce::String(corrected.keyConfidence, 4));
    int acceptedKeys = 0;
    for (int tonic = 0; tonic < 24; ++tonic)
    {
        auto music = progression(tonic);
        const auto retry = retryMusicAnalysis(music, rate, previous, false, true);
        acceptedKeys += retry.focusedKey == 2;
        check(retry.key < 0 || retry.key == tonic, "focused harmonic pass agrees with authored " + keyNoteName(tonic) + " or abstains");
    }
    check(acceptedKeys >= 20, "focused harmonic recovery covers at least twenty authored major/minor keys: " + juce::String(acceptedKeys));
    auto antiPhase = progression(17, 20, .7, 8000); antiPhase.applyGain(.1f); antiPhase.applyGain(1, 0, antiPhase.getNumSamples(), -1);
    const auto anti = retryMusicAnalysis(antiPhase, 8000, previous, false, true);
    const auto normalChord = analyseMusic(chord, rate), normalAnti = analyseMusic(antiPhase, 8000);
    const auto values = [](const MusicAnalysis& a)
    {
        auto* value = new juce::DynamicObject(); value->setProperty("key", a.key); value->setProperty("keyConfidence", a.keyConfidence);
        value->setProperty("tuningCents", a.tuningCents); value->setProperty("bpm", a.bpm); value->setProperty("tempoConfidence", a.tempoConfidence);
        return juce::var(value);
    };
    auto* comparison = new juce::DynamicObject(); comparison->setProperty("cleanC_normal", values(normalChord)); comparison->setProperty("cleanC_focused", values(key));
    comparison->setProperty("quietDetunedAntiphaseFminor8k_normal", values(normalAnti)); comparison->setProperty("quietDetunedAntiphaseFminor8k_focused", values(anti));
    report.getSiblingFile("normal-focused-comparison.json").replaceWithText(juce::JSON::toString(juce::var(comparison)));
    check(anti.key == 17 && std::abs(anti.tuningCents - 20) < 5, "focused key keeps detuned quiet antiphase harmony at 8 kHz; key=" + juce::String(anti.key) + " tuning=" + juce::String(anti.tuningCents, 3));
    auto strong = previous; strong.key = 6; strong.keyConfidence = .8;
    const auto kept = retryMusicAnalysis(chord, rate, strong, true, true);
    check(kept.key == 6 && kept.bpm == 120 && kept.focusedRevision == 0, "strong automatic results are not silently replaced by a focused retry");
    auto cancel = std::make_shared<std::atomic<bool>>(true);
    const auto cancelled = retryMusicAnalysis(chord, rate, previous, false, true, cancel);
    check(cancelled.key == -1 && cancelled.focusedRevision == 0, "cancelled focused work neither changes results nor records a completed attempt");
    juce::AudioBuffer<float> silence(1, (int) (rate * 12)); silence.clear();
    const auto silent = retryMusicAnalysis(silence, rate, previous, false, true);
    check(silent.key == -1 && silent.focusedKey == 1 && !needsFocusedKey(silent), "silence stays unknown and an identical completed retry is skipped");
    for (int i = 0; i < silence.getNumSamples(); ++i) silence.setSample(0, i, (float) (.3 * std::sin(2 * juce::MathConstants<double>::pi * 440 * i / rate)));
    const auto tone = retryMusicAnalysis(silence, rate, previous, false, true);
    check(tone.key == -1 && tone.focusedKey == 1, "a pitched tone is not promoted to a musical key by harmonic filtering");
    juce::Random random(8123);
    for (int i = 0; i < silence.getNumSamples(); ++i) silence.setSample(0, i, random.nextFloat() - .5f);
    check(retryMusicAnalysis(silence, rate, previous, false, true).key == -1, "broadband noise stays unknown after harmonic filtering");
    auto beat = rhythm(128, .173, 36); MusicAnalysis missing; missing.key = 7; missing.keyConfidence = .8;
    const auto timing = retryMusicAnalysis(beat, rate, missing, true, false);
    check(std::abs(timing.bpm - 128) < .02 && timing.focusedTempo == 2 && !timing.variableTempo
        && timing.key == 7 && timing.keyConfidence == .8, "focused pulse retry recovers authored constant tempo without changing the key; bpm=" + juce::String(timing.bpm, 4));
    // Musical material lies between every first-pass sample on this long source.
    // A different sampling schedule, not a rerun of the same input windows, must recover it.
    juce::AudioBuffer<float> sparse(1, 8000 * 360); sparse.clear();
    const auto lowRateChord = progression(0, 0, .18, 8000);
    for (int n = 0; n < 8; ++n) for (int i = 0; i < 12 * 8000; ++i)
        sparse.setSample(0, (int) ((42.5 * n + 21) * 8000) + i, lowRateChord.getSample(0, i));
    const auto normal = analyseMusic(sparse, 8000);
    const auto recovered = retryMusicAnalysis(sparse, 8000, normal, false, true, {}, &evidence);
    check(normal.key == -1 && recovered.key == 0 && recovered.focusedKey == 2,
          "shifted windows recover authored harmony missed by the normal sampling budget; candidate=" + juce::String(evidence.key) + " score=" + juce::String(evidence.keyConfidence, 4));
}
void runMusicTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    // Frozen outputs of the pre-optimisation dense producer, generated before
    // changing its representation. Exercise reflection and mixed signal levels.
    const char* melGoldens[] {
        "3be0c6b1e8934283f1dbcaaef088105abaa9be4671ccee647f73c45054606f78",
        "cad14a681d5400db582efc461acb5613741289f7af2d74b7a3d26551b7977084",
        "127c91bbe292865b8bae75dfa4c0a85f2137fe5097b2fb3ae818e4a273988385",
        "6bed8d9065ae71a682506f34cd2fdfbeec162216549289d7bbacb4ade75c180e"
    };
    for (int fixture = 0; fixture < 4; ++fixture)
    {
        const int size = fixture < 2 ? 1025 : fixture == 2 ? 4099 : 22051;
        std::vector<float> audio((size_t) size, 0); uint32_t state = 0x12345678u;
        for (int i = 0; i < size; ++i)
        {
            state ^= state << 13; state ^= state >> 17; state ^= state << 5;
            if (fixture == 1) audio[(size_t) i] = i % 257 == 0 ? 0.75f : 0;
            if (fixture >= 2) audio[(size_t) i] = (float) ((int) (state & 65535u) - 32768) / 32768.0f * (fixture == 3 && i % 2 ? 0.000001f : 1.0f);
        }
        MelSpectrogram mel; const auto values = mel.compute(audio); juce::MemoryOutputStream bytes;
        for (const auto& row : values) bytes.write(row.data(), row.size() * sizeof(float));
        check(juce::SHA256(bytes.getData(), bytes.getDataSize()).toHexString() == melGoldens[fixture],
              "sparse mel input is bit-identical to the frozen dense producer " + juce::String(fixture));
    }
    for (double bpm : {80.0, 100.0, 120.0, 128.0, 150.0, 174.0})
    {
        const auto audio = rhythm(bpm, 0.213); auto analysis = analyseMusic(audio, rate);
        check(std::abs(analysis.bpm - bpm) < 0.5, "BPM " + juce::String(bpm, 0) + " detected " + juce::String(analysis.bpm, 2) + " confidence " + juce::String(analysis.tempoConfidence, 2));
        const double period = 60 / bpm;
        double error = std::abs(analysis.beatOrigin - 0.213); error = std::min(error, std::abs(error - period));
        check(error < 0.035, "beat phase within 35 ms at " + juce::String(bpm, 0) + " BPM (" + juce::String(error * 1000, 1) + " ms)");
    }
    for (int key = 0; key < 24; ++key)
    {
        auto music = progression(key); auto analysis = analyseMusic(music, rate);
        check(analysis.key == key, "tonal progression " + keyNoteName(key) + " -> " + keyNoteName(analysis.key) + " / " + keyCamelot(analysis.key));
    }
    auto antiPhase = progression(17); antiPhase.applyGain(1, 0, antiPhase.getNumSamples(), -1);
    check(analyseMusic(antiPhase, rate).key == 17, "anti-phase stereo remains analysable");
    for (double tuning : {-32.0, 27.0})
    {
        auto music = progression(17, tuning, 0.7); auto percussion = rhythm(126, 0.213, 12);
        juce::Random noise(8017);
        for (int i = 0; i < music.getNumSamples(); ++i)
            for (int ch = 0; ch < 2; ++ch) music.addSample(ch, i, percussion.getSample(ch, i) * 0.6f + (noise.nextFloat() - 0.5f) * 0.04f);
        const auto a = analyseMusic(music, rate);
        check(a.key == 17 && std::abs(a.tuningCents - tuning) < 5, "detuned harmonic F minor with drums/noise: " + keyNoteName(a.key) + ", tuning " + juce::String(a.tuningCents, 1));
    }
    juce::AudioBuffer<float> noiseAudio(2, (int) (rate * 10)); juce::Random noise(4901);
    for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < noiseAudio.getNumSamples(); ++i) noiseAudio.setSample(ch, i, (noise.nextFloat() - 0.5f) * 0.3f);
    check(analyseMusic(noiseAudio, rate).key == -1, "broadband noise does not receive a musical key");
    auto longRhythm = rhythm(127.3749, 0.213, 210); const auto longGrid = analyseMusic(longRhythm, rate);
    double drift = std::abs(std::remainder(longGrid.beatOrigin + 420 * 60 / longGrid.bpm - (0.213 + 420 * 60 / 127.3749), 60 / 127.3749));
    check(std::abs(longGrid.bpm - 127.3749) < 0.001 && drift < 0.004 && !longGrid.variableTempo,
          "long fractional-tempo grid drift under 4 ms: " + juce::String(longGrid.bpm, 4) + " BPM, " + juce::String(drift * 1000, 2) + " ms");
    const auto longFile = report.getSiblingFile("long-analysis.wav");
    {
        juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream = longFile.createOutputStream();
        auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(rate).withNumChannels(2).withBitsPerSample(24));
        check(writer && writer->writeFromAudioSampleBuffer(longRhythm, 0, longRhythm.getNumSamples()), "write long source for nine-window analysis cache round trip");
    }
    juce::AudioFormatManager longFormats; longFormats.registerBasicFormats();
    auto longCold = readMedia(longFormats, longFile);
    auto longWarm = readMedia(longFormats, longFile);
    check(longCold.media && longWarm.media && !longCold.media->analysisFromCache && longWarm.media->analysisFromCache
          && longWarm.media->analysis.beatWindows.size() == maxAnalysisWindows
          && longWarm.media->analysis.examinedSeconds <= 180.0001
          && std::abs(longWarm.media->analysis.bpm - longCold.media->analysis.bpm) < 1.0e-9,
          "production cache reuses all nine distributed analysis windows within the same 180-second budget");
    auto sparse = rhythm(123.45, 0.213, 90);
    // A silent intro and breakdown must not create a different tempo or reset source phase.
    for (int ch = 0; ch < 2; ++ch) { sparse.clear(ch, 0, (int) (rate * 8)); sparse.clear(ch, (int) (rate * 35), (int) (rate * 12)); }
    const auto sparseGrid = analyseMusic(sparse, rate);
    check(std::abs(sparseGrid.bpm - 123.45) < 0.02 && !sparseGrid.variableTempo && std::abs(std::remainder(sparseGrid.beatOrigin - 0.213, 60 / 123.45)) < 0.025,
          "silent intro/breakdown retain source beat phase: " + juce::String(sparseGrid.bpm, 4));
    juce::AudioBuffer<float> shifted(2, (int) (rate * 150)); shifted.clear();
    for (int section = 0; section < 3; ++section)
    {
        auto part = rhythm(120, section == 1 ? 0.463 : 0.213, 50);
        for (int ch = 0; ch < 2; ++ch) shifted.copyFrom(ch, (int) (section * 50 * rate), part, ch, 0, part.getNumSamples());
    }
    check(analyseMusic(shifted, rate).variableTempo, "equal local BPM with a shifted middle grid requires manual review");
    juce::AudioBuffer<float> silence(2, (int) (rate * 8)); silence.clear(); auto unknown = analyseMusic(silence, rate);
    check(unknown.key == -1 && unknown.bpm == 0, "silence stays unknown rather than receiving fabricated metadata");
    juce::AudioBuffer<float> changing(2, (int) (rate * 90)); changing.clear();
    for (int section = 0; section < 3; ++section)
    {
        auto part = rhythm(section == 0 ? 90 : section == 1 ? 120 : 160, 0.213, 30);
        for (int ch = 0; ch < 2; ++ch) changing.copyFrom(ch, (int) (section * 30 * rate), part, ch, 0, part.getNumSamples());
    }
    check(analyseMusic(changing, rate).variableTempo, "different source-window tempos are flagged for manual review");
    check(keyCamelot(0) == "8B" && keyCamelot(21) == "8A" && keyCamelot(17) == "4A" && keyNoteName(17) == "F minor", "note/Camelot reference mappings");
    check(harmonicallyCompatible(0, 21) && harmonicallyCompatible(0, 7) && !harmonicallyCompatible(0, -1), "Camelot neighbour, relative-mode and unknown handling");
    auto tone = std::make_shared<Media>(); tone->sampleRate = rate; tone->samples.setSize(2, (int) (rate * 4));
    for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < tone->samples.getNumSamples(); ++i) tone->samples.setSample(ch, i, (float) (0.3 * std::sin(2 * juce::MathConstants<double>::pi * 440 * i / rate)));
    check(analyseMusic(tone->samples, rate).key == -1, "a single pitch is not labelled as a musical key");
    auto harmonicTone = tone->samples;
    for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < harmonicTone.getNumSamples(); ++i)
        for (int h = 2; h <= 7; ++h) harmonicTone.addSample(ch, i, (float) (0.3 / h * std::sin(2 * juce::MathConstants<double>::pi * 440 * h * i / rate)));
    check(analyseMusic(harmonicTone, rate).key == -1, "one harmonic-rich pitch is not mistaken for a major key");
    for (double speed : {0.8, 1.25})
    {
        auto stretched = stretchAudio(*tone, speed, maxPreparedBytes);
        check(stretched.error.isEmpty() && stretched.audio && std::abs(stretched.audio->frames() - tone->samples.getNumSamples() / speed) <= 1, "pitch-preserving stretch duration at " + juce::String(speed, 2) + ": " + stretched.error);
        if (stretched.audio) check(std::abs(frequency(copyPreparedPcm(*stretched.audio)) - 440) < 2, "key lock retains 440 Hz at tempo ratio " + juce::String(speed, 2));
    }
    check(stretchAudio(*tone, 0.8, 16).error.isNotEmpty(), "stretch expansion budget checked before allocation");
    Project project; project.masterDb = 0; auto first = std::make_shared<Media>(); first->sampleRate = rate; first->samples = rhythm(128, 0.213); first->analysis = analyseMusic(first->samples, rate);
    first->manualBpm = 128; first->manualBeatOrigin = 0.213; first->file = report.getSiblingFile("beatmatch-source.wav");
    { juce::WavAudioFormat format; std::unique_ptr<juce::OutputStream> stream = first->file.createOutputStream(); auto writer = format.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(rate).withNumChannels(2).withBitsPerSample(24)); if (writer) writer->writeFromAudioSampleBuffer(first->samples, 0, first->samples.getNumSamples()); }
    for (int i = 0; i < 2; ++i) { Clip clip; clip.media = first; clip.track = i; clip.length = first->duration(); project.clips.push_back(clip); }
    auto matched = prepareBeatmatch(project, 120, true, 32);
    check(matched.error.isEmpty() && matched.matched == 2 && matched.project.clips[0].prepared == matched.project.clips[1].prepared, "beatmatch prepares and shares one source/tempo cache");
    if (matched.error.isEmpty())
    {
        const auto& prepared = matched.project.clips[0].prepared;
        auto actual = analyseMusic(copyPreparedPcm(*prepared), rate);
        check(std::abs(actual.bpm - 120) < 0.5, "prepared beatmatching PCM has the target 120 BPM tempo");
        double actualPhaseError = std::remainder(actual.beatOrigin - first->beatOrigin() / prepared->rate, 0.5);
        check(std::abs(actualPhaseError) < 0.04, "prepared PCM beat phase stays within 40 ms of the aligned grid (" + juce::String(actualPhaseError * 1000, 1) + " ms); signal "
              + juce::String(actual.signalBpm, 4) + " / " + juce::String(actual.signalTempoConfidence, 3) + ", review " + juce::String((int) actual.signalGridNeedsReview)
              + ", neural " + juce::String(actual.neuralBpm, 4) + ", correction " + juce::String(actual.neuralPhaseCorrectionMs, 2));
        bool aligned = true;
        for (const auto& c : matched.project.clips) { double phase = (c.start + (first->beatOrigin() - c.offset) / c.rate) * 120 / 60; aligned = aligned && std::abs(phase - std::round(phase)) < 1.0e-6; }
        check(aligned, "incoming and outgoing beat phases align on the project grid");
        auto before = matched.project.clips[0]; double splitTime = 5;
        check(std::abs(before.sourceTimeAt(splitTime) - (before.offset + splitTime * 120 / 128)) < 1.0e-8, "trim/split uses source time after stretching");
        Engine engine; engine.project = matched.project; engine.library = { first };
        auto saved = juce::JSON::toString(engine.captureProject());
        check(engine.restoreProject(saved).isEmpty() && engine.project.clips[0].prepared && engine.project.clips[0].media->manualBpm == 128, "beatmatching, key display and manual overrides survive save/reopen");
        auto corrupt = juce::JSON::parse(saved); corrupt["clips"].getArray()->getReference(0).getDynamicObject()->setProperty("rate", -2);
        check(engine.restoreProject(juce::JSON::toString(corrupt)).isNotEmpty(), "malformed stretch rates cannot replace a valid session");
        auto wav = report.getSiblingFile("beatmatched-render.wav");
        check(engine.exportWav(wav).isEmpty(), "beatmatched PCM exports through production mixer");
    }
    Engine controller; auto originalMappings = controller.mappings; controller.mappings.clear(); controller.mappings.push_back({0xb0, 8, 71, {MidiActionKind::volume, 0}}); controller.beginControllerSetup();
    check(controller.mappingSummary().contains("Pot 1") && controller.mappingSummary().contains("volume-1  <-  CC 71"), "current guided setup starts at paired pots and preserves the existing fader 1 assignment");
    controller.testMidi(juce::MidiMessage::controllerEvent(8, 71, 127));
    check(controller.mappingSummary().contains("Pot 1") && controller.mappingSummary().contains("Already assigned to volume-1"), "repeated events from an assigned control explain the conflict without filling the next wizard step");
    controller.testMidi(juce::MidiMessage::controllerEvent(8, 72, 64));
    check(controller.mappingSummary().contains("Pot 2") && controller.mappingSummary().contains("desk-pot-1  <-  CC 72"), "current guided setup advances after a distinct control");
    controller.beginLearn({}); controller.mappings = originalMappings;
}
}
