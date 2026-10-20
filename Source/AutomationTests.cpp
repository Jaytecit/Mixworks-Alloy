#include "Engine.h"
#include <cmath>
#include <limits>

namespace mixworks
{
void runAutomationTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    Engine engine; engine.project.masterDb = 0;
    auto media = std::make_shared<Media>(); media->name = "09. Zebra automation fixture"; media->sampleRate = 44100;
    media->samples.setSize(2, 88200); media->samples.clear();
    for (int ch = 0; ch < 2; ++ch) juce::FloatVectorOperations::fill(media->samples.getWritePointer(ch), 0.25f, 88200);
    Clip clip; clip.media = media; clip.length = 2; engine.project.clips = {clip}; engine.library = {media};
    auto& track = engine.project.tracks[0]; auto& volume = track.automation[0];
    volume.enabled = true; volume.setPoint(0, -12); volume.setPoint(1, 0);
    juce::AudioBuffer<float> output(2, 512); juce::MidiBuffer midi;
    engine.render(output, 512, 0.5, midi); bool correct = true;
    for (int i = 0; i < 512; ++i)
    {
        const float expected = (float) (0.25 * std::pow(10.0, (-12 + 12 * (0.5 + i / 44100.0)) / 20));
        correct = correct && std::abs(output.getSample(0, i) - expected) < 1.0e-6 && output.getSample(0, i) == output.getSample(1, i);
    }
    check(correct, "volume automation produces the expected dB ramp at every rendered sample, on both channels");
    check(volume.valueAt(1.5, -3) == 0 && volume.valueAt(-1, -3) == -3, "automation holds its final value and stopped instrument audition uses manual gain");
    volume.enabled = false; track.gainDb = -3; engine.render(output, 512, 0.5, midi);
    check(std::abs(output.getSample(0, 20) - 0.25 * std::pow(10.0, -3.0 / 20)) < 1.0e-6 && volume.points.size() == 2,
          "Read off restores manual volume without losing points");
    volume.enabled = true; track.mute = true; engine.render(output, 512, 0.5, midi);
    check(output.getMagnitude(0, 512) == 0, "mute overrides automated volume");
    track.mute = false; engine.project.tracks[1].solo = true; engine.render(output, 512, 0.5, midi);
    check(output.getMagnitude(0, 512) == 0, "solo on another track excludes an automated track"); engine.project.tracks[1].solo = false;

    AutomationCurve edited; edited.setPoint(1, -6); edited.setPoint(0, 0); edited.setPoint(1, -12);
    check(edited.points.size() == 2 && edited.points[0].time == 0 && edited.points[1].value == -12, "point insertion sorts times and replaces a point at the same time");
    edited.movePoint(0, 2, -9);
    check(edited.points[0].time < edited.points[1].time && edited.setPoint(std::numeric_limits<double>::quiet_NaN(), 0) == -1,
          "point dragging preserves strict ordering and rejects non-finite edits");

    track.automation = {}; track.gainDb = 0;
    for (int band = 0; band < 3; ++band)
    {
        const double frequency = band == 0 ? 30.0 : band == 1 ? 1000.0 : 16000.0;
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 88200; ++i) media->samples.setSample(ch, i, (float) (0.2 * std::sin(2 * juce::MathConstants<double>::pi * frequency * i / 44100)));
        auto energy = [&](bool automate)
        {
            track.automation = {}; auto& curve = track.automation[(size_t) band + 1]; curve.enabled = automate;
            curve.setPoint(0, -18); curve.setPoint(0.75, -18); curve.setPoint(1.25, 0); engine.resetPlugins();
            std::array<double, 2> sums {};
            for (int offset = 0; offset < 88200; offset += 512)
            {
                const int count = std::min(512, 88200 - offset); engine.render(output, count, offset / 44100.0, midi);
                for (int i = 0; i < count; ++i)
                {
                    const double time = (offset + i) / 44100.0; const double power = std::pow(output.getSample(0, i), 2);
                    if (time >= 0.25 && time < 0.5) sums[0] += power;
                    if (time >= 1.5 && time < 1.75) sums[1] += power;
                    correct = correct && std::isfinite(power);
                }
            }
            return sums;
        };
        const auto dry = energy(false), wet = energy(true);
        const double earlyDb = 10 * std::log10(wet[0] / dry[0]), lateDb = 10 * std::log10(wet[1] / dry[1]);
        check(correct && std::abs(earlyDb + 18) < 1.5 && std::abs(lateDb) < 0.1,
              "EQ automation band " + juce::String(band + 1) + " moves measured response from " + juce::String(earlyDb, 2) + " to " + juce::String(lateDb, 2) + " dB");
    }
    // Use all curves together, including EQ changes between irregular callback boundaries.
    track.automation = {};
    for (int lane = 0; lane < 4; ++lane) { auto& curve = track.automation[(size_t) lane]; curve.enabled = true; curve.setPoint(0, -12); curve.setPoint(0.7, -3); curve.setPoint(1.3, -9); }
    auto trace = [&](int size)
    {
        std::vector<float> result; result.reserve(44100); engine.resetPlugins();
        for (int offset = 0; offset < 44100; offset += size)
        {
            const int count = std::min(size, 44100 - offset); engine.render(output, count, offset / 44100.0, midi);
            for (int i = 0; i < count; ++i) result.push_back(output.getSample(0, i));
        }
        return result;
    };
    const auto live = trace(173), regular = trace(512); double maxError = 0;
    for (size_t i = 0; i < live.size(); ++i) maxError = std::max(maxError, (double) std::abs(live[i] - regular[i]));
    check(maxError < 2.0e-6, "automated EQ and volume are callback-size independent; max PCM error " + juce::String(maxError, 9));
    const auto exported = report.getSiblingFile("automation-render.wav"); check(engine.exportWav(exported).isEmpty(), "automated volume and EQ export through the production WAV renderer");
    std::unique_ptr<juce::AudioFormatReader> reader(engine.formats.createReaderFor(exported)); correct = reader && reader->lengthInSamples == 88200;
    if (reader)
    {
        juce::AudioBuffer<float> wave(2, 44100); reader->read(&wave, 0, 44100, 0, true, true);
        for (int i = 0; i < 44100; ++i) correct = correct && std::abs(wave.getSample(0, i) - live[(size_t) i]) < 2.0e-6;
    }
    check(correct, "exported automation PCM matches live audio across different block sizes");

    media->file = report.getSiblingFile("automation-source.wav");
    {
        juce::WavAudioFormat format; std::unique_ptr<juce::OutputStream> stream = media->file.createOutputStream();
        auto writer = format.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(44100).withNumChannels(2).withBitsPerSample(24));
        if (writer) writer->writeFromAudioSampleBuffer(media->samples, 0, 88200);
    }
    track.automation[2].enabled = false;
    const auto saved = juce::JSON::toString(engine.captureProject());
    check(engine.restoreProject(saved).isEmpty() && engine.project.tracks[0].automation[1].points.size() == 3
          && !engine.project.tracks[0].automation[2].enabled && engine.project.tracks[0].automation[2].points[1].value == -3,
          "session reopening retains every automation point and per-curve Read state");
    auto damaged = juce::JSON::parse(saved); auto& points = *damaged["tracks"].getArray()->getReference(0)["automation"]["volume"]["points"].getArray();
    points.getReference(1).getDynamicObject()->setProperty("time", 0);
    check(engine.restoreProject(juce::JSON::toString(damaged)).isNotEmpty() && engine.project.tracks[0].automation[0].points.size() == 3,
          "duplicate automation times fail transactionally without replacing the session");
    for (auto invalid : {juce::var(-1), juce::var("0.5"), juce::var(1.0e10)})
    {
        auto bad = juce::JSON::parse(saved); auto& point = bad["tracks"].getArray()->getReference(0)["automation"]["volume"]["points"].getArray()->getReference(1);
        point.getDynamicObject()->setProperty("time", invalid);
        check(parseProject(juce::JSON::toString(bad)).error.isNotEmpty(), "invalid automation time rejected: " + invalid.toString());
    }
    auto bad = juce::JSON::parse(saved); bad["tracks"].getArray()->getReference(0)["automation"]["bass"]["points"].getArray()->getReference(0).getDynamicObject()->setProperty("value", 13);
    check(parseProject(juce::JSON::toString(bad)).error.isNotEmpty(), "out-of-range automation gain rejected before audio loading");
    auto legacy = juce::JSON::parse(saved); legacy.getDynamicObject()->setProperty("version", 3); legacy["tracks"].getArray()->removeRange(4, trackCount - 4);
    for (auto& t : *legacy["tracks"].getArray()) t.getDynamicObject()->removeProperty("automation");
    check(engine.restoreProject(juce::JSON::toString(legacy)).isEmpty() && engine.project.tracks[0].automation[0].points.empty(), "version 3 session migrates with manual controls and no automation");

    media->name = "01. 2 One 2 - Blade Runnerz";
    check(media->libraryTitle() == "2 One 2 - Blade Runnerz" && media->name.startsWith("01."), "library title hides only the leading track index and retains the original name");
    media->name = "1999 - Prince"; correct = media->libraryTitle() == media->name;
    media->name = "2 One 2"; correct = correct && media->libraryTitle() == media->name;
    media->name = "007 Zebra"; correct = correct && media->libraryTitle() == "Zebra";
    check(correct, "title display preserves years and number-led titles while hiding padded track indexes");

    // A small real-file UI fixture, separate from the user's sessions and source files.
    media->name = "09. Zebra automation fixture"; engine.library = {media};
    auto unused = readMedia(engine.formats, media->file); unused.media->name = "02. Alpha unused fixture"; engine.library.push_back(unused.media);
    engine.project.clips[0].media = media; engine.project.clips[0].length = 16; engine.project.clips[0].loopSpan = 2;
    for (int lane = 0; lane < 4; ++lane)
    {
        auto& curve = engine.project.tracks[0].automation[(size_t) lane]; curve.enabled = true;
        curve.setPoint(0, 0); curve.setPoint(4, -12); curve.setPoint(8, 0);
    }
    report.getSiblingFile("automation-demo.mixworks").replaceWithText(juce::JSON::toString(engine.captureProject()));
}
}
