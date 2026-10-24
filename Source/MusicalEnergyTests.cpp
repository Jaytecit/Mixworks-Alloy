#include "MusicalEnergy.h"
#include "AutoArrangement.h"
#include "Engine.h"
#include "Beatmatch.h"
#include <cmath>
#include <limits>

namespace mixworks
{
void runMusicalEnergyTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    const auto fixture = [](double rate, double hits, bool bass, double gain = .2, int channels = 1)
    {
        juce::AudioBuffer<float> pcm(channels, (int) (32 * rate));
        for (int i = 0; i < pcm.getNumSamples(); ++i)
        {
            const double t = i / rate, phase = std::fmod(t, 1 / hits);
            const double pulse = std::exp(-phase * 45);
            const double value = gain * pulse * (std::sin(2 * juce::MathConstants<double>::pi * 1700 * t)
                + (bass ? 2 * std::sin(2 * juce::MathConstants<double>::pi * 65 * t) : 0));
            for (int ch = 0; ch < channels; ++ch) pcm.setSample(ch, i, (float) (ch ? -value : value));
        }
        return pcm;
    };
    auto sparse = fixture(8000, 1, false), busy = fixture(8000, 6, false), bass = fixture(8000, 6, true);
    auto quietBusy = fixture(8000, 6, true, .02), stereo = fixture(8000, 6, true, .2, 2);
    const auto a = analyseMusicalEnergy(PcmView(sparse), 8000), b = analyseMusicalEnergy(PcmView(busy), 8000), c = analyseMusicalEnergy(PcmView(bass), 8000);
    const auto quiet = analyseMusicalEnergy(PcmView(quietBusy), 8000), anti = analyseMusicalEnergy(PcmView(stereo), 8000);
    const auto score = [](const auto& p) { return musicalEnergyBody(p).value_or(-1); };
    check(a.complete() && b.complete() && c.complete() && a.windows.size() == 4, "bounded complete musical profiles cover the source in local activity windows");
    check(score(b) > score(a) + .1, "busy percussion has more musical drive than sparse percussion at the same tempo");
    check(score(c) > score(b) + .02 && c.windows[1].bass() > b.windows[1].bass(), "rhythmic bass contributes activity beyond high percussion alone");
    check(std::abs(score(quiet) - score(c)) < .005, "a 20dB recording gain change preserves musical activity instead of changing its rank");
    check(std::abs(score(anti) - score(c)) < .005, "opposite-polarity stereo preserves evidence by combining channel powers");
    auto equalRms = busy; equalRms.applyGain(sparse.getRMSLevel(0, 0, sparse.getNumSamples()) / busy.getRMSLevel(0, 0, busy.getNumSamples()));
    const auto equal = analyseMusicalEnergy(PcmView(equalRms), 8000);
    check(std::abs(equalRms.getRMSLevel(0, 0, equalRms.getNumSamples()) - sparse.getRMSLevel(0, 0, sparse.getNumSamples())) < 1e-6
        && score(equal) > score(a) + .1, "equal-RMS authored rhythms retain different musical drive");
    auto compressed = bass;
    for (int i = 0; i < compressed.getNumSamples(); ++i) { const float x = compressed.getSample(0, i); compressed.setSample(0, i, x / (.02f + std::abs(x))); }
    const auto limited = analyseMusicalEnergy(PcmView(compressed), 8000);
    check(limited.complete() && limited.windows[1].crestDb < c.windows[1].crestDb,
        "saturation reduces measured transient crest rather than receiving credit for its louder mastering");
    for (const double rate : {44100., 48000.})
    {
        const auto pcm = fixture(rate, 6, true); const auto profile = analyseMusicalEnergy(PcmView(pcm), rate);
        check(profile.complete() && std::abs(score(profile) - score(c)) < .1, "musical activity is stable across supported sample rates: " + juce::String(rate));
    }
    juce::AudioBuffer<float> pad(1, 256000), silence(1, 256000); silence.clear();
    for (int i = 0; i < pad.getNumSamples(); ++i) pad.setSample(0, i, (float) (.7 * std::sin(2 * juce::MathConstants<double>::pi * 65 * i / 8000)));
    const auto drone = analyseMusicalEnergy(PcmView(pad), 8000), silent = analyseMusicalEnergy(PcmView(silence), 8000);
    check(score(quiet) > score(drone) + .15 && drone.windows[1].bass() < .05, "a quiet driving track outranks a loud stationary bass drone");
    check(silent.complete() && !musicalEnergyBody(silent), "measured silence is complete but supplies no invented musical-energy evidence");
    auto dc = silence;
    for (int i = 0; i < dc.getNumSamples(); ++i) dc.setSample(0, i, .3f);
    check(musicalEnergyBody(analyseMusicalEnergy(PcmView(dc), 8000)).value_or(0) < .05,
        "a DC offset cannot masquerade as rhythmic musical drive");
    auto cancelled = std::make_shared<std::atomic<bool>>(true);
    check(!analyseMusicalEnergy(PcmView(bass), 8000, cancelled).complete(), "cancelled extraction cannot publish a partial profile");
    bass.setSample(0, 4000, std::numeric_limits<float>::quiet_NaN());
    check(!analyseMusicalEnergy(PcmView(bass), 8000).complete(), "nonfinite audio cannot manufacture musical features");
    const auto json = musicalEnergyToJson(c);
    check(parseMusicalEnergy(json, 32).complete() && score(parseMusicalEnergy(json, 32)) == score(c), "local feature profiles round-trip without score drift");
    check(!parseMusicalEnergy(json, 31).complete(), "a source-duration mismatch rejects supplementary evidence");
    auto future = juce::JSON::parse(juce::JSON::toString(json)); future.getDynamicObject()->setProperty("revision", musicalEnergyRevision + 1);
    check(!parseMusicalEnergy(future, 32).complete(), "future supplementary versions remain unknown");
    auto malformed = juce::JSON::parse(juce::JSON::toString(json)); malformed.getProperty("windows", {})[1].getArray()->set(2, -1.);
    check(!parseMusicalEnergy(malformed, 32).complete(), "malformed profile coverage is rejected");
    auto sections = silence; sections.copyFrom(0, 128000, busy, 0, 128000, 128000);
    const auto local = analyseMusicalEnergy(PcmView(sections), 8000);
    check(!musicalEnergyAt(local, 0, 8) && musicalEnergyAt(local, 16, 24).value_or(0) > .1,
        "local profile preserves a quiet intro and a busy body instead of assigning one level to the whole source");
    const auto media = [](const juce::String& name, const MusicalEnergyProfile& profile, float rms)
    {
        auto m = std::make_shared<Media>(); m->name = name; m->sampleRate = 8000; m->analysisSource.frames = 256000;
        m->analysisSource.channels = 1; m->analysis.bpm = 120; m->analysis.tempoConfidence = 1;
        m->analysis.downbeatOrigin = 0; m->analysis.key = 0; m->energy = profile;
        m->waveformRms.assign(4000, rms); return m;
    };
    auto startingPad = media("Loud sustained pad", drone, .7f), sparseSong = media("Sparse percussion", a, .3f), driving = media("Quiet driving song", quiet, .02f);
    startingPad->samples = pad; sparseSong->samples = sparse; driving->samples = quietBusy;
    AutoArrangementOptions options; options.startingId = startingPad->id; options.trackCount = 2;
    options.energy = AutoEnergyPolicy::build; options.transitions = AutoTransitionStyle::smooth;
    juce::Random seeded(904);
    const auto musical = planAutoArrangement({startingPad, sparseSong, driving}, options, seeded);
    check(musical.error.isEmpty() && musical.project.clips.size() == 2 && musical.project.clips.back().media == driving,
        "production arranger chooses quieter rhythmic drive over louder sparse material");
    driving->waveformRms.assign(4000, .9f); juce::Random sameSeed(904);
    const auto mastered = planAutoArrangement({startingPad, sparseSong, driving}, options, sameSeed);
    check(mastered.project.clips.size() == 2 && mastered.project.clips.back().media == driving,
        "changing only recording RMS does not alter musical-energy ordering");
    options.transitions = AutoTransitionStyle::varied; juce::Random variedSeed(904);
    auto varied = planAutoArrangement({startingPad, sparseSong, driving}, options, variedSeed);
    auto prepared = prepareTempoMap(std::move(varied.project), false, options.overlapBeats, 0, 0, {}, options.preset, true);
    check(prepared.error.isEmpty() && prepared.project.clips.size() == 2 && !varied.decisions.isEmpty(),
        "real authored PCM prepares a varied handover through the production tempo/preset path: " + prepared.error);
    if (prepared.error.isEmpty() && prepared.project.clips.size() == 2)
    {
        auto engine = std::make_unique<Engine>(); engine->project = std::move(prepared.project); engine->prepareClipProcessing();
        const auto range = engine->project.transitionRange(engine->project.transitionLinks.front());
        juce::AudioBuffer<float> rendered(2, 512); juce::MidiBuffer midi;
        engine->render(rendered, 512, (range.getStart() + range.getEnd()) * .5, midi);
        bool finite = rendered.getMagnitude(0, 512) > 1e-6;
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 512; ++i) finite &= std::isfinite(rendered.getSample(ch, i));
        check(finite, "selected varied treatment renders finite non-silent audio from the actual authored sources");
    }
    auto* result = new juce::DynamicObject(); result->setProperty("sparse", musicalEnergyToJson(a)); result->setProperty("busy", musicalEnergyToJson(b));
    result->setProperty("bass", json); result->setProperty("quiet", musicalEnergyToJson(quiet)); result->setProperty("drone", musicalEnergyToJson(drone));
    result->setProperty("equalRms", musicalEnergyToJson(equal)); result->setProperty("limited", musicalEnergyToJson(limited));
    check(report.getSiblingFile("musical-energy-measurements.json").replaceWithText(juce::JSON::toString(juce::var(result))), "retain measured musical descriptors for independent inspection");
}
}
