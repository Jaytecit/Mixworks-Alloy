#include "EarTools.h"
#include "ElasticAudio.h"
#include <cmath>

namespace mixworks
{
EarKeyChoice::EarKeyChoice(int suggestion)
{
    const int first = suggestion >= 0 && suggestion < 24 ? suggestion % 12 : 0;
    for (int i = 0; i < 12; ++i) order[(size_t) i] = (first + i) % 12;
    winner = first;
}
int EarKeyChoice::candidate(int sound) const
{
    return stage == 0 ? (sound == 1 ? winner : order[(size_t) juce::jlimit(1, 11, next)]) : winner + (sound == 1 ? 0 : 12);
}
void EarKeyChoice::prefer(int sound)
{
    if (sound != 1 && sound != 2) return;
    if (stage == 0) { winner = candidate(sound); if (++next == 12) stage = 1; }
    else { selected = candidate(sound); stage = 2; }
}
void EarLoopPlayer::source(std::shared_ptr<const Media> value, double a, double b)
{
    const juce::ScopedLock guard(mutex); media = std::move(value); samples.reset();
    sourceRate = media->sampleRate; begin = juce::jlimit(0., media->duration(), a);
    end = juce::jlimit(begin, media->duration(), b); cursor = begin;
}
void EarLoopPlayer::pcm(std::shared_ptr<const juce::AudioBuffer<float>> value, double rate)
{
    const juce::ScopedLock guard(mutex); samples = std::move(value); media.reset(); sourceRate = rate;
    begin = 0; end = samples && rate > 0 ? samples->getNumSamples() / rate : 0; cursor = begin; mode = 0;
}
void EarLoopPlayer::reference(int value, bool chords, int reg, float db)
{
    const juce::ScopedLock guard(mutex); key = juce::jlimit(0, 23, value); chord = chords; octave = juce::jlimit(2, 5, reg);
    referenceGain = juce::Decibels::decibelsToGain(juce::jlimit(-36.f, -6.f, db)); cursor = begin;
}
void EarLoopPlayer::hear(int value) { const juce::ScopedLock guard(mutex); mode = juce::jlimit(0, 2, value); }
void EarLoopPlayer::level(float db) { const juce::ScopedLock guard(mutex); gain = juce::Decibels::decibelsToGain(juce::jlimit(-36.f, -3.f, db)); }
void EarLoopPlayer::prepareToPlay(int, double rate) { const juce::ScopedLock guard(mutex); outputRate = rate; cursor = begin; }
void EarLoopPlayer::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
    OriginalAudio::Realtime realtimeReads;
    info.clearActiveBufferRegion(); const juce::ScopedTryLock guard(mutex);
    if (!guard.isLocked() || end <= begin || outputRate <= 0 || sourceRate <= 0) return;
    if (media && media->original)
    {
        const bool current = media->original->request((int)(cursor * sourceRate),
            (int)std::min(double(media->frames()), (std::min(end, cursor + info.numSamples / outputRate) * sourceRate) + 2));
        const bool restart = media->original->request((int)(begin * sourceRate),
            (int)std::min(double(media->frames()), (std::min(end, begin + info.numSamples / outputRate) * sourceRate) + 2));
        if (!current || !restart) return;
        media->original->request((int)(cursor * sourceRate), (int)std::min(double(media->frames()), std::min(end, cursor + 1.) * sourceRate + 2));
        media->original->request((int)(begin * sourceRate), (int)std::min(double(media->frames()), std::min(end, begin + 1.) * sourceRate + 2));
    }
    const int frames = samples ? samples->getNumSamples() : media && media->hasPlayback() ? media->frames() : 0;
    const int channels = samples ? samples->getNumChannels() : media ? media->channels() : 0;
    if (frames < 1 || channels < 1) return;
    for (int i = 0; i < info.numSamples; ++i)
    {
        if (cursor >= end) cursor = begin + std::fmod(cursor - begin, end - begin);
        const double position = cursor * sourceRate;
        const int first = juce::jlimit(0, frames - 1, (int) position), next = std::min(first + 1, frames - 1);
        const float fraction = (float) (position - std::floor(position));
        const float fade = (float) juce::jlimit(0., 1., std::min(cursor - begin, end - cursor) / .005);
        double tone = 0;
        if (mode != 0)
        {
            const int intervals[] {0, key < 12 ? 4 : 3, 7}; const int count = chord ? 3 : 1;
            for (int n = 0; n < count; ++n)
            {
                const double hz = 440 * std::pow(2., (12 * (octave + 1) + key % 12 + intervals[n] - 69) / 12.);
                const double angle = juce::MathConstants<double>::twoPi * hz * cursor;
                tone += (.75 * std::sin(angle) + .18 * std::sin(angle * 2) + .07 * std::sin(angle * 3)) / count;
            }
        }
        for (int ch = 0; ch < std::min(2, info.buffer->getNumChannels()); ++ch)
        {
            const int channel = std::min(ch, channels - 1);
            const float a = samples ? samples->getSample(channel, first) : media->sample(channel, first);
            const float b = samples ? samples->getSample(channel, next) : media->sample(channel, next);
            const float music = a + fraction * (b - a);
            info.buffer->setSample(ch, info.startSample + i, fade * gain * ((mode != 1 ? music : 0) + (float) tone * referenceGain));
        }
        cursor += 1. / outputRate;
    }
}
juce::Result renderHarmony(Engine& renderer, const juce::String& reference, const juce::String& adjusted,
    juce::Range<double> loop, double pitch, const std::atomic<bool>& cancel, HarmonyAudio& output)
{
    if (reference == adjusted || !renderer.project.clipFor(reference) || !renderer.project.clipFor(adjusted)
        || !std::isfinite(loop.getStart()) || !std::isfinite(loop.getEnd()) || loop.getStart() < 0 || loop.getLength() < .1
        || loop.getLength() > 30 || !std::isfinite(pitch) || std::abs(pitch) > 6.5)
        return juce::Result::fail("Choose two different overlapping segments and a loop of 0.1 to 30 seconds.");
    auto base = renderer.project;
    base.clips.erase(std::remove_if(base.clips.begin(), base.clips.end(), [&](const auto& c) { return c.id != reference && c.id != adjusted; }), base.clips.end());
    for (auto& track : base.tracks) track.solo = false;
    for (auto& c : base.clips)
    {
        if (!c.media->hasPlayback()) return juce::Result::fail("Wait for both source files to finish loading.");
        base.tracks[(size_t) c.track].mute = false;
        c.pitchSemitones = c.id == adjusted ? pitch : c.pitchSemitones;
        c.elasticPlayback = true; c.elasticSource = std::make_shared<const ElasticSource>(c);
    }
    base.transitionLinks.erase(std::remove_if(base.transitionLinks.begin(), base.transitionLinks.end(), [&](const auto& link)
    { return !base.clipFor(link.incoming) || !base.clipFor(link.outgoing); }), base.transitionLinks.end());
    const double rate = renderer.sampleRate.load();
    if (!std::isfinite(rate) || rate < 8000 || rate > 192000) return juce::Result::fail("Unsupported preview sample rate.");
    const int total = (int) std::llround(loop.getLength() * rate), warm = (int) std::min(loop.getStart() * rate, rate * 2);
    renderer.prepareRender(rate, 512); HarmonyAudio result; result.sampleRate = rate; result.pitch = pitch;
    for (int mode = 0; mode < 3; ++mode)
    {
        renderer.project = base;
        if (mode != 2) renderer.project.clips.erase(std::remove_if(renderer.project.clips.begin(), renderer.project.clips.end(),
            [&](const auto& c) { return c.id != (mode == 0 ? reference : adjusted); }), renderer.project.clips.end());
        renderer.resetPlugins(); renderer.prepareClipProcessing(); renderer.playing = true;
        auto pcm = std::make_shared<juce::AudioBuffer<float>>(2, total); pcm->clear();
        juce::AudioBuffer<float> buffer(2, 512); juce::MidiBuffer midi;
        for (int offset = -warm; offset < total;)
        {
            if (cancel) return juce::Result::fail("Listening preparation cancelled.");
            const int count = std::min(512, total - offset);
            renderer.render(buffer, count, loop.getStart() + offset / rate, midi);
            const int skip = std::max(0, -offset), copy = count - skip;
            if (copy > 0) for (int ch = 0; ch < 2; ++ch) pcm->copyFrom(ch, std::max(0, offset), buffer, ch, skip, copy);
            offset += count;
        }
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < total; ++i)
            if (!std::isfinite(pcm->getSample(ch, i))) return juce::Result::fail("Preview returned invalid audio; the arrangement was kept.");
        result.hear[(size_t) mode] = std::move(pcm);
    }
    output = std::move(result); return juce::Result::ok();
}
void matchHarmonyLevel(HarmonyAudio& audio, const HarmonyAudio& original)
{
    for (size_t mode = 0; mode < 3; ++mode)
    {
        auto& buffer = audio.hear[mode]; const auto& reference = original.hear[mode];
        if (!buffer || !reference) continue;
        const auto rms = [](const auto& b) { return std::sqrt((std::pow(b.getRMSLevel(0, 0, b.getNumSamples()), 2)
            + std::pow(b.getRMSLevel(1, 0, b.getNumSamples()), 2)) * .5); };
        const double value = rms(*buffer), target = rms(*reference);
        if (value > 1.e-6 && target > 1.e-6) buffer->applyGain((float) juce::jlimit(.5, 2., target / value));
    }
}
juce::Result commitHarmony(Engine& engine, const juce::String& baseline, const Clip& prepared, const std::function<void()>& remember)
{
    const juce::ScopedLock guard(engine.lock);
    if (engine.exporting || !std::isfinite(prepared.pitchSemitones) || std::abs(prepared.pitchSemitones) > 6.5)
        return juce::Result::fail("Pitch Apply is unavailable during export or has an invalid value.");
    if (juce::JSON::toString(projectToJson(engine.project), true, 17) != baseline)
        return juce::Result::fail("The arrangement changed. Reopen Harmonise by ear to compare its current sound.");
    auto* current = engine.project.clipFor(prepared.id);
    if (!current || current->media->file != prepared.media->file || !(current->media->analysisSource == prepared.media->analysisSource))
        return juce::Result::fail("That source or segment changed. Reopen Harmonise by ear.");
    if (prepared.pitchSemitones != 0 && !prepared.elasticPlayback && (!prepared.prepared || prepared.prepared->pitchSemitones != prepared.pitchSemitones))
        return juce::Result::fail("Pitch audio is not ready. The arrangement was kept.");
    if (current->pitchSemitones == prepared.pitchSemitones) return juce::Result::ok();
    remember();
    current->pitchSemitones = prepared.pitchSemitones; current->prepared = prepared.prepared; current->stemPrepared = prepared.stemPrepared;
    current->elasticSource = prepared.elasticSource; current->preparedOffset = prepared.preparedOffset;
    current->elasticPlayback = prepared.elasticPlayback;
    engine.prepareClipProcessing(); engine.recordProjectCommit(); return juce::Result::ok();
}
EarPreviewControls::EarPreviewControls(Engine& e, LibraryPreview& r, EarLoopPlayer& p) : engine(e), route(r), player(p), mode(e.workMode)
{
    for (auto* c : std::initializer_list<juce::Component*> {&play, &output, &destination, &description, &volume}) addAndMakeVisible(c);
    if (mode == WorkMode::arrange) destination.addItem("Master preview", 1);
    destination.addItem("Private headphones", 2); destination.setSelectedId(route.usesPrivateOutput() ? 2 : 1, juce::dontSendNotification);
    description.setFont(juce::FontOptions(11));
    volume.setRange(-36, -3, .5); volume.setValue(-12, juce::dontSendNotification); volume.setTextValueSuffix(" dB");
    volume.setSliderStyle(juce::Slider::LinearHorizontal); volume.setTextBoxStyle(juce::Slider::TextBoxRight, false, 68, 25);
    volume.setName("Listening preview level"); volume.setTooltip("Listening level only. Arrangement and export gains stay unchanged.");
    volume.onValueChange = [this] { player.level((float) volume.getValue()); };
    destination.onChange = [this] { stop(); description.setText(destination.getSelectedId() == 1 ? "Arrange preview pauses the mix." : route.description(), juce::dontSendNotification); };
    destination.onChange();
    play.onClick = [this]
    {
        if (route.active(id)) { stop(); return; }
        if (engine.workMode != mode) { if (feedback) feedback("Work mode changed. Close and reopen this listening tool."); return; }
        ownsRouting = true; route.setPrivateOutput(destination.getSelectedId() == 2);
        const auto result = route.playPrepared(player, id);
        if (feedback) feedback(result.failed() ? result.getErrorMessage() : destination.getSelectedId() == 1 ? "Looping through master preview; arrangement paused." : "Looping through " + route.description());
    };
    output.onClick = [this]
    {
        stop(); if (engine.playing || engine.capturingPerformance()) { if (feedback) feedback("Stop programme playback/capture before changing the headphones route."); return; }
        destination.setSelectedId(2, juce::sendNotificationSync); ownsRouting = true; route.setPrivateOutput(true);
        route.chooseOutput(*this, [safe = juce::Component::SafePointer<EarPreviewControls>(this)]
        { if (safe) safe->description.setText(safe->route.description(), juce::dontSendNotification); });
    };
}
EarPreviewControls::~EarPreviewControls() { stop(); }
void EarPreviewControls::stop()
{
    if (route.active(id)) route.stop();
    if (ownsRouting && route.idle()) route.setPrivateOutput(engine.workMode == WorkMode::recordPerform);
    ownsRouting = false;
}
void EarPreviewControls::resized()
{
    auto r = getLocalBounds(); auto top = r.removeFromTop(29);
    play.setBounds(top.removeFromLeft(115).reduced(2, 0)); destination.setBounds(top.removeFromLeft(170).reduced(2, 0));
    output.setBounds(top.removeFromLeft(175).reduced(2, 0)); volume.setBounds(top); description.setBounds(r);
}
EarPassage::EarPassage(std::shared_ptr<const Media> value) : media(std::move(value)) { setMouseCursor(juce::MouseCursor::CrosshairCursor); }
void EarPassage::paint(juce::Graphics& g)
{
    g.fillAll(colour::panel); const double duration = media->duration(); if (duration <= 0) return;
    const auto x = [&](double t) { return (float) (t / duration * getWidth()); };
    g.setColour(colour::orange.withAlpha(.2f)); g.fillRect(x(range.getStart()), 0.f, x(range.getLength()), (float) getHeight());
    g.setColour(colour::muted);
    for (int i = 0; i < getWidth(); ++i)
    {
        const float height = media->waveformRange(duration * i / getWidth(), duration * (i + 1) / getWidth()).peak * (getHeight() - 24) * .45f;
        g.drawVerticalLine(i, getHeight() * .5f - height, getHeight() * .5f + height);
    }
    g.setColour(colour::orange); for (auto t : {range.getStart(), range.getEnd()}) g.drawVerticalLine(juce::roundToInt(x(t)), 0.f, (float) getHeight());
    g.setColour(colour::text); g.setFont(juce::FontOptions(12));
    g.drawText("Drag over a musical passage to loop  /  " + juce::String(range.getStart(), 2) + " to " + juce::String(range.getEnd(), 2) + " s", getLocalBounds().withHeight(22).reduced(6, 0), juce::Justification::left);
}
void EarPassage::mouseDown(const juce::MouseEvent& e) { if (e.mods.isLeftButtonDown()) down = juce::jlimit(0., media->duration(), e.position.x / getWidth() * media->duration()); }
void EarPassage::mouseDrag(const juce::MouseEvent& e)
{
    if (!e.mods.isLeftButtonDown()) return;
    const double at = juce::jlimit(0., media->duration(), e.position.x / getWidth() * media->duration());
    range = {std::min(at, down), std::max(at, down)}; repaint();
}
void EarPassage::mouseUp(const juce::MouseEvent&)
{
    if (range.getLength() < .1) range = {std::min(down, std::max(0., media->duration() - 4)), std::min(media->duration(), down + 4)};
    if (onChange) onChange();
}
EarToolWindow::EarToolWindow(juce::Component* content, const juce::String& title) : AlloyPopupWindow(title, colour::background, true)
{
    setUsingNativeTitleBar(false); setContentOwned(content, true); setResizable(false, false); centreWithSize(content->getWidth(), content->getHeight());
}
void EarToolWindow::closeButtonPressed()
{
    if (auto* key = dynamic_cast<KeyEarEditor*>(getContentComponent())) key->close();
    if (auto* harmony = dynamic_cast<HarmonyEarEditor*>(getContentComponent())) harmony->close();
    setVisible(false); exitModalState(0);
}
}
