#include "Workstation.h"
#include "EnergySections.h"
#include "GridCorrectionUI.h"
#include "AppWindow.h"
#include "Beatmatch.h"
#include <cmath>
#include <limits>
#include <set>
#include <cstdlib>

namespace mixworks
{
namespace
{
void label(juce::Label& l, const juce::String& text, float size = 12)
{
    l.setText(text, juce::dontSendNotification); l.setFont(juce::Font(juce::FontOptions(size))); l.setColour(juce::Label::textColourId, colour::muted);
}
void slider(juce::Slider& s, double low, double high, double step, const juce::String& suffix = {})
{
    s.setRange(low, high, step); s.setSliderStyle(juce::Slider::LinearHorizontal); s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 22); s.setTextValueSuffix(suffix);
}
juce::String trackLabel(int i) { return juce::String(i + 1).paddedLeft('0', 2); }
class MidiSettings final : public juce::Component
{
public:
    explicit MidiSettings(Engine& e) : engine(e), audio(e.devices, 0, 0, 0, 2, true, false, true, false)
    {
        addAndMakeVisible(audio); addAndMakeVisible(help); addAndMakeVisible(action); addAndMakeVisible(learn); addAndMakeVisible(cancel); addAndMakeVisible(mappings); addAndMakeVisible(quickSetup); addAndMakeVisible(reload);
        label(help, "FLkey 49: enable its MIDI input above; use Custom Mode for faders / knobs. Keys play the selected track's instrument.");
        help.setJustificationType(juce::Justification::topLeft);
        int id = 1;
        for (auto a : midiActions()) { auto name = midiActionId(a); actions.add(name); action.addItem(name, id++); }
        action.setSelectedId(1); learn.setButtonText("Learn"); cancel.setButtonText("Cancel learn");
        learn.onClick = [this] { engine.beginLearn(actions[action.getSelectedId() - 1]); updateText(); };
        cancel.onClick = [this] { engine.beginLearn({}); updateText(); };
        quickSetup.setButtonText("FLkey guided setup"); quickSetup.onClick = [this] { engine.beginControllerSetup(); updateText(); };
        reload.setButtonText("Reload saved mappings"); reload.setComponentID("reload-midi-mappings");
        reload.setTooltip("Replace active assignments from the saved profile shown below. Does not rewrite the profile, controller or mix.");
        reload.onClick = [this] { engine.reloadMidiMappings(); updateText(); };
        mappings.setMultiLine(true); mappings.setReadOnly(true); updateText(); setSize(740, 650);
    }
    void updateText() { mappings.setText(engine.mappingSummary()); }
    void resized() override
    {
        auto area = getLocalBounds().reduced(18); audio.setBounds(area.removeFromTop(350)); help.setBounds(area.removeFromTop(48));
        auto row = area.removeFromTop(32); action.setBounds(row.removeFromLeft(370).reduced(2)); learn.setBounds(row.removeFromLeft(100).reduced(2)); cancel.setBounds(row.reduced(2));
        auto setup = area.removeFromTop(32); quickSetup.setBounds(setup.removeFromLeft(setup.getWidth() / 2).reduced(2)); reload.setBounds(setup.reduced(2));
        mappings.setBounds(area.reduced(2));
    }
    Engine& engine;
private:
    juce::AudioDeviceSelectorComponent audio;
    juce::Label help;
    juce::ComboBox action;
    juce::StringArray actions;
    juce::TextButton learn, cancel, quickSetup, reload;
    juce::TextEditor mappings;
};
void findPlugins(const juce::File& folder, juce::Array<juce::File>& out)
{
    for (const auto& entry : folder.findChildFiles(juce::File::findFilesAndDirectories, false))
        if (entry.hasFileExtension("vst3")) out.add(entry);
        else if (entry.isDirectory() && !entry.isSymbolicLink()) findPlugins(entry, out);
}
}
ImportProgressView::ImportProgressView()
{
    for (auto* child : std::initializer_list<juce::Component*> { &heading, &file, &stage, &detail, &bar, &cancel }) addAndMakeVisible(child);
    label(heading, "Importing audio", 20); heading.setColour(juce::Label::textColourId, colour::text);
    label(file, {}, 14); file.setColour(juce::Label::textColourId, colour::text);
    label(stage, {}, 14); label(detail, {}, 13);
    bar.setColour(juce::ProgressBar::foregroundColourId, colour::orange);
    bar.setTextToDisplay({}); setAlwaysOnTop(true);
}
void ImportProgressView::paint(juce::Graphics& g)
{
    g.setColour(colour::raised); g.fillRoundedRectangle(getLocalBounds().toFloat(), 9);
    g.setColour(colour::orange); g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1), 9, 2);
}
void ImportProgressView::resized()
{
    auto area = getLocalBounds().reduced(22);
    heading.setBounds(area.removeFromTop(30)); detail.setBounds(area.removeFromTop(26));
    file.setBounds(area.removeFromTop(46)); stage.setBounds(area.removeFromTop(26));
    area.removeFromTop(8); bar.setBounds(area.removeFromTop(20)); area.removeFromTop(12);
    cancel.setBounds(area.removeFromTop(30).removeFromRight(140));
}
void ImportProgressView::begin(int totalFiles)
{
    heading.setText("Importing audio", juce::dontSendNotification); stopMessage = "Stopping after this track. Finished tracks will stay in your library.";
    total = totalFiles; completed = 0; fraction = 0; stopping = false; exporting = false; startedAt = juce::Time::getMillisecondCounterHiRes();
    file.setText("Starting import...", juce::dontSendNotification); stage.setText({}, juce::dontSendNotification);
    cancel.setButtonText("Stop import"); cancel.setEnabled(true); setEnabled(true); setVisible(true); toFront(false); tick();
}
void ImportProgressView::showFile(const juce::String& name, MediaReadStage phase)
{
    file.setText(name, juce::dontSendNotification);
    if (stopping) return;
    const char* description = "";
    switch (phase)
    {
        case MediaReadStage::decode: description = "Decoding audio"; break;
        case MediaReadStage::waveform: description = "Building waveform"; break;
        case MediaReadStage::analyse: description = "Detecting beats and musical key"; break;
        case MediaReadStage::cache: description = "Preparing playback audio"; break;
        case MediaReadStage::lookupAnalysis: description = "Checking saved track analysis"; break;
        case MediaReadStage::savedAnalysis: description = "Using saved beat, key and waveform analysis"; break;
        case MediaReadStage::playlist: description = "Reading local playlist paths"; break;
        case MediaReadStage::savedAudio: description = "Reusing decoded playback audio"; break;
        case MediaReadStage::tempo: description = "Reusing or preparing tempo audio"; break;
    }
    stage.setText(description, juce::dontSendNotification);
}
void ImportProgressView::finishFile() { ++completed; fraction = total > 0 ? (double) completed / total : 0; tick(); }
void ImportProgressView::setCounts(int done, int count) { completed = done; total = count; fraction = total > 0 ? (double) completed / total : 0; tick(); }
void ImportProgressView::tick()
{
    const auto elapsed = (int) ((juce::Time::getMillisecondCounterHiRes() - startedAt) / 1000);
    const auto progress = exporting ? juce::String(renderedFrames / exportPlan.sampleRate, 1) + " / " + juce::String(exportPlan.seconds(), 1) + " seconds rendered"
                                    : juce::String(completed) + " of " + juce::String(total) + " files processed";
    detail.setText(progress + "  |  " + juce::String(elapsed / 60) + ":" + juce::String(elapsed % 60).paddedLeft('0', 2) + " elapsed", juce::dontSendNotification);
}
void ImportProgressView::setExporting(const juce::File& destination, const WavExportPlan& plan)
{
    begin(0); exporting = true; exportPlan = plan; renderedFrames = 0;
    heading.setText("Exporting mix", juce::dontSendNotification); file.setText(destination.getFileName(), juce::dontSendNotification);
    stage.setText(plan.tailSeconds > 0 ? "Stereo 24-bit WAV / including " + juce::String(plan.tailSeconds, 2) + " s effect tail" : "Stereo 24-bit WAV / last-clip cutoff", juce::dontSendNotification);
    cancel.setButtonText("Cancel export"); stopMessage = "Cancelling export. The destination will be kept."; tick();
}
void ImportProgressView::showExportProgress(juce::int64 frames)
{
    renderedFrames = frames; fraction = exportPlan.frames > 0 ? juce::jlimit(0., 1., (double) frames / exportPlan.frames) : 0; tick();
}
void ImportProgressView::requestStop()
{
    stopping = true; cancel.setEnabled(false);
    stage.setText(stopMessage, juce::dontSendNotification);
}
Timeline::Timeline(Workstation& w) : owner(w) { setWantsKeyboardFocus(true); }
int Timeline::laneTop(int track) const { return owner.layout.laneTop(track); }
int Timeline::laneSize(int track) const { return owner.layout.laneSize(track); }
int Timeline::contentLaneCount() const
{
    return trackCount;
}
int Timeline::trackAt(float y) const
{
    for (int i = 0; i < trackCount; ++i) if (y < laneTop(i + 1)) return i;
    return trackCount - 1;
}
float Timeline::laneY(int track, float y) const { return laneTop(track) + y * laneSize(track) / laneHeight; }
int Timeline::dividerAt(float y) const
{
    for (int i = 0; i < trackCount; ++i) if (std::abs(y - (laneTop(i + 1) - 1)) <= 3) return i;
    return -1;
}
float Timeline::automationY(int track, float value, int target) const { target = std::max(0, target < 0 ? owner.automationLane() : target); const auto range = automationRange((AutomationTarget) target); return laneY(track, 10.0f + target * 5.0f + (range.getEnd() - value) / range.getLength() * 70); }
float Timeline::automationDb(int track, float y, AutomationTarget target) const { const auto range = automationRange(target); return range.clipValue(range.getEnd() - ((y - laneTop(track)) * laneHeight / laneSize(track) - 10 - (int) target * 5) / 70 * range.getLength()); }
bool Timeline::hasGeneratedAutomation(const AutomationAddress& address) const
{
    const int band = (int) address.target;
    const auto* clip = owner.engine.project.clipFor(address.clipId);
    if (!clip || band < 0 || band >= 4) return false;
    const auto playback = owner.engine.project.transitionPlaybackFor(*clip);
    return playback.pairs[0][(size_t) band].strength > 0 || playback.pairs[1][(size_t) band].strength > 0;
}
float Timeline::automationDisplayValue(const AutomationAddress& address, double time, float manual, const TransitionPlayback* compiled) const
{
    const int band = (int) address.target;
    const auto* clip = owner.engine.project.clipFor(address.clipId);
    if (!clip || band < 0 || band >= 4) return manual;
    const float gain = compiled ? compiled->gainAt(band, time) : owner.engine.project.transitionPlaybackFor(*clip).gainAt(band, time);
    return automationRange(address.target).clipValue(manual + juce::Decibels::gainToDecibels(gain, -60.f));
}
float Timeline::automationManualValue(const AutomationAddress& address, double time, float displayed) const
{
    return automationRange(address.target).clipValue(displayed - automationDisplayValue(address, time, 0));
}
void Timeline::selectAutomationPoint(const AutomationAddress& address, int point)
{
    const auto* curve = owner.engine.project.curve(address);
    editingAutomationPoint = curve && point >= 0 && point < (int) curve->points.size()
        ? std::optional<std::pair<AutomationAddress, int>>({address, point}) : std::nullopt;
    repaint();
}
juce::String Timeline::automationPointLabel(const AutomationAddress& address, int point) const
{
    if (!editingAutomationPoint || !(editingAutomationPoint->first == address) || editingAutomationPoint->second != point) return {};
    const auto* curve = owner.engine.project.curve(address);
    const int track = address.target == AutomationTarget::master ? 0 : address.track;
    if (!curve || point < 0 || point >= (int) curve->points.size() || owner.automationLane() != (int) address.target
        || !(owner.displayedAddress(track) == address)) return {};
    const auto& node = curve->points[(size_t) point];
    return owner.automationText(address.target, automationDisplayValue(address, owner.engine.project.automationSessionTime(address, node.time), node.value));
}
std::optional<std::pair<AutomationAddress, int>> Timeline::automationPointAt(juce::Point<float> position) const
{
    if (position.y < 0 || position.y >= laneTop(contentLaneCount())) return {};
    const int track = trackAt(position.y);
    std::optional<std::pair<AutomationAddress, int>> found;
    float nearest = 8;
    for (const auto& address : owner.automationAddresses(track))
    {
        if (hasGeneratedAutomation(address) && !owner.transitionCurveVisible((int) address.target)) continue;
        const auto* curve = owner.engine.project.curve(address); if (laneSize(track) < 30 || !curve) continue;
        for (int index = 0; index < (int) curve->points.size(); ++index)
        {
            const auto& point = curve->points[(size_t) index];
            const double time = owner.engine.project.automationSessionTime(address, point.time);
            const float distance = position.getDistanceFrom({(float) (time * pixelsPerSecond), automationY(track, automationDisplayValue(address, time, point.value), (int) address.target)});
            if (distance < nearest) { nearest = distance; found = {{address, index}}; }
        }
    }
    return found;
}
std::optional<std::pair<juce::String, bool>> Timeline::anchorAt(juce::Point<float> position) const
{
    if (owner.busy || owner.tempoPreparationPending || owner.engine.project.hasPendingPlacement()) return {};
    for (auto it = owner.engine.project.clips.rbegin(); it != owner.engine.project.clips.rend(); ++it)
    {
        const auto bounds = clipBounds(*it);
        if (position.y < bounds.getY() || position.y > bounds.getBottom() || !it->media->hasPlayback()) continue;
        for (const bool intro : {true, false})
        {
            const double local = it->transitionTime(intro);
            if (local < 0 || local > it->length) continue;
            if (std::abs(position.x - (it->start + local) * pixelsPerSecond) <= 7) return {{it->id, intro}};
        }
    }
    return {};
}
void Timeline::mouseMove(const juce::MouseEvent& event)
{
    if (event.y >= 0 && dividerAt(event.position.y) >= 0)
    { setTooltip("Drag to resize this lane. Double-click restores its default height."); setMouseCursor(juce::MouseCursor::UpDownResizeCursor); return; }
    if (audioSlipAt(event.position))
    { setTooltip("Slip audio only: drag left/right; automation, clip boundaries and paired transitions stay fixed. Alt bypasses beat snap. Double-click resets. Unlock protected tracks first."); setMouseCursor(juce::MouseCursor::LeftRightResizeCursor); return; }
    if (event.y < 0) { setMouseCursor(juce::MouseCursor::NormalCursor); return; }
    if (const auto swap = swapHandleAt(event.position))
    { setTooltip("Drag this paired swap left/right. Both sides move together, keeping their beat gap. Alt bypasses snapping; unlock protected tracks first."); setMouseCursor(juce::MouseCursor::LeftRightResizeCursor); return; }
    const auto transition = transitionAt(event.position);
    setTooltip(transition ? "Transition frame: drag the header to move, top corners to resize, or OUTGOING / INCOMING to move a song inside it. Alt bypasses snap." : juce::String());
    auto pointer = juce::MouseCursor::NormalCursor;
    if (!owner.busy)
    {
        if (anchorAt(event.position) || labelAt(event.position) >= 0) pointer = juce::MouseCursor::LeftRightResizeCursor;
        else if (automationPointAt(event.position) || transition) pointer = juce::MouseCursor::DraggingHandCursor;
        else for (const auto& clip : owner.engine.project.clips)
            if (clipBounds(clip).contains(event.position)) { pointer = juce::MouseCursor::DraggingHandCursor; break; }
    }
    setMouseCursor(pointer);
}
juce::Rectangle<float> Timeline::clipBounds(const Clip& c) const
{
    return { (float) (c.start * pixelsPerSecond), (float) laneTop(c.track) + std::min(7.f, laneSize(c.track) * .1f), std::max(3.0f, (float) (c.length * pixelsPerSecond)), (float) laneSize(c.track) - 2 * std::min(7.f, laneSize(c.track) * .1f) };
}
void Timeline::updateSize()
{
    const juce::ScopedValueSetter<bool> viewGuard(owner.programmaticTimelineView, true);
    double end = owner.engine.project.audibleEnd();
    if (owner.layout.followPlayhead) end = std::max(end, owner.engine.position.load());
    if (!owner.engine.project.tempo.points.empty()) end = std::max(end, owner.engine.project.tempo.points.back().time);
    if (!owner.engine.project.labels.empty()) end = std::max(end, owner.engine.project.labels.back().time);
    for (const auto& track : owner.engine.project.tracks) for (const auto& curve : track.automation) if (!curve.points.empty()) end = std::max(end, curve.points.back().time);
    if (!owner.engine.project.masterAutomation.points.empty()) end = std::max(end, owner.engine.project.masterAutomation.points.back().time);
    for (const auto& track : owner.engine.project.tracks) for (const auto& lane : track.pluginAutomation) if (!lane.curve.points.empty()) end = std::max(end, lane.curve.points.back().time);
    const double width = std::min((double) std::numeric_limits<int>::max() / 4, std::max(60.0, end + 10) * pixelsPerSecond);
    setSize(std::max(owner.viewport.getWidth(), (int) width), laneTop(contentLaneCount()));
    owner.zoomSlider.setValue(std::log10(pixelsPerSecond), juce::dontSendNotification);
    owner.timelineRuler.repaint(); owner.timelineOverview.repaint();
    repaint();
}
void Timeline::zoomAt(double factor, double timelineX)
{
    const juce::ScopedValueSetter<bool> viewGuard(owner.programmaticTimelineView, true);
    const double time = timelineX / pixelsPerSecond;
    const double screenX = timelineX - owner.viewport.getViewPositionX();
    pixelsPerSecond = juce::jlimit(0.1, 400.0, pixelsPerSecond * factor);
    const int y = owner.viewport.getViewPositionY();
    updateSize();
    owner.viewport.setViewPosition((int) std::round(time * pixelsPerSecond - screenX), y);
    owner.followPlayheadIntoView();
}
void Timeline::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    // Shift retains viewport scrolling for lanes below the visible region.
    if (e.mods.isShiftDown())
    {
        owner.viewport.setViewPosition(owner.viewport.getViewPositionX(), (int) std::round(owner.viewport.getViewPositionY() - wheel.deltaY * 280));
        return;
    }
    if (std::abs(wheel.deltaX) > std::abs(wheel.deltaY))
    { owner.setFollowPlayhead(false); owner.viewport.setViewPosition((int) std::round(owner.viewport.getViewPositionX() - wheel.deltaX * 280), owner.viewport.getViewPositionY()); return; }
    if (wheel.deltaY != 0 && !e.mods.isAnyMouseButtonDown())
        zoomAt(std::exp2(juce::jlimit(-1.0f, 1.0f, wheel.deltaY) * 2), e.position.x);
}
void Timeline::paint(juce::Graphics& g)
{
    g.fillAll(colour::background);
    const auto& p = owner.engine.project;
    auto visible = getLocalArea(&owner.viewport, owner.viewport.getLocalBounds());
    const TempoMap grid = p.followTrackTempo ? p.tempo : TempoMap {p.bpm, {}};
    int stride = 1; double fastest = p.bpm;
    for (const auto& point : grid.points) fastest = std::max(fastest, point.bpm);
    while (60 / fastest * stride * pixelsPerSecond < 8) stride *= 4;
    // Four bars (16 beats in 4/4), on the same integrated map as snapping.
    // This is a counting guide, not a claim to have detected musical phrases.
    const double firstGroup = std::floor(grid.beatsAt(visible.getX() / pixelsPerSecond) / 16) * 16;
    if (60 / fastest * 16 * pixelsPerSecond >= 4)
        for (double beat = firstGroup;; beat += 16)
        {
            const float x = (float) (grid.timeAtBeat(beat) * pixelsPerSecond);
            if (x >= visible.getRight()) break;
            if ((int64_t) std::llround(beat / 16) % 2 == 0)
            { g.setColour(colour::raised.withAlpha(0.24f)); g.fillRect(x, 0.0f,
                (float) (grid.timeAtBeat(beat + 16) * pixelsPerSecond) - x, (float) getHeight()); }
        }
    const auto first = std::max<int64_t>(0, (int64_t) (grid.beatsAt(visible.getX() / pixelsPerSecond) / stride));
    struct BeatGuide { int64_t beat; int x; }; std::vector<BeatGuide> guides;
    for (int64_t n = first;; ++n)
    {
        const auto beat = n * stride; const double time = grid.timeAtBeat((double) beat);
        const int x = Clip::gridPixel(time, pixelsPerSecond); if (x >= visible.getRight()) break;
        guides.push_back({beat, x});
        g.setColour(beat % 16 == 0 ? colour::muted.withAlpha(0.28f) : colour::border.withAlpha(beat % 4 == 0 ? 0.65f : 0.3f));
        g.drawVerticalLine(x, 0, (float) getHeight());

    }
    for (int i = 0; i < trackCount; ++i) { g.setColour(colour::border); g.drawHorizontalLine(laneTop(i + 1) - 1, (float) visible.getX(), (float) visible.getRight()); }
    const double forgeTime = owner.engine.position.load();
    const float forgeX = (float) Clip::gridPixel(forgeTime, pixelsPerSecond);
    const bool forgeMotion = owner.forgeMotionAllowed() && owner.engine.playing && !owner.engine.gridAudition.active()
        && !owner.engine.rehearsalActive();
    const bool anySolo = std::any_of(p.tracks.begin(), p.tracks.end(), [](const auto& t) { return t.solo; });
    auto forgeEligible = [&](const Clip& c)
    {
        if (c.start > forgeTime || c.end() <= forgeTime || c.pendingPlacement != PendingPlacement::none || (!c.prepared && !c.media->hasPlayback())) return false;
        const auto& lane = p.tracks[(size_t) c.track];
        if (lane.mute || (anySolo && !lane.solo)) return false;
        const auto playback = p.transitionPlaybackFor(c);
        return (!playback.startCut || forgeTime >= *playback.startCut) && (!playback.endCut || forgeTime < *playback.endCut);
    };
    const bool forging = forgeMotion && forgeX >= visible.getX() - 100 && forgeX < visible.getRight() + 100
        && std::count_if(p.clips.begin(), p.clips.end(), forgeEligible) >= 2;
    for (const auto& c : p.clips)
    {
        const auto r = clipBounds(c); if (!r.intersects(visible.toFloat())) continue;
        const auto accent = colour::tracks[(size_t) c.track];
        if (laneSize(c.track) < 30) { g.setColour(accent.withAlpha(.7f)); g.fillRect(r.withHeight(std::min(2.f, r.getHeight()))); continue; }
        g.setColour(accent.withAlpha(c.id == owner.selectedClip ? 0.13f : 0.055f)); g.fillRoundedRectangle(r, 5);
        g.setColour(accent.withAlpha(c.id == owner.selectedClip ? 1.0f : 0.6f)); g.drawRoundedRectangle(r.reduced(0.5f), 5, c.id == owner.selectedClip ? 2.0f : 1.0f);
        juce::Graphics::ScopedSaveState save(g); g.reduceClipRegion(r.toNearestInt());
        const int left = std::max(visible.getX(), (int) r.getX()), right = std::min(visible.getRight(), (int) r.getRight());
        g.setFont(juce::FontOptions(12).withStyle("Bold")); g.setColour(colour::text); g.drawText(c.media->name
            + (!c.media->playbackError.isEmpty() ? " [audio unavailable]" : !c.media->hasPlayback() ? (c.media->sourceDetailsPending ? " [reading duration / preparing audio]" : " [preparing audio]") : "")
            + (c.pendingPlacement == PendingPlacement::failed ? " [tempo failed; retry Beatmatch]" : c.pendingPlacement != PendingPlacement::none ? " [matching tempo]" : "")
            + (c.reversed ? " [reverse]" : "") + (c.loopSpan > 0 ? " [loop]" : "") + "  " + owner.keyText(*c.media)
            + (c.media->bpm() > 0 ? "  / " + juce::String(c.media->bpm(), 2) + " BPM / " + (c.media->manualBpm <= 0 && (c.media->analysis.variableTempo || c.media->analysis.tempoConfidence < 0.2) ? "beat grid needs review" : c.firstDownbeatTime() >= 0 ? (c.media->manualBeatOrigin >= 0 ? "4/4" : "4/4 inferred") : "downbeat unknown") : ""), r.withLeft((float) left).withRight((float) right).withTrimmedRight(28).reduced(8, 3).removeFromTop(18), juce::Justification::left);
        const float baseline = r.getBottom() - 5, height = std::max(1.f, r.getHeight() - 32);
        std::vector<WaveformRange> levels; levels.reserve((size_t) std::max(0, right - left));
        float ceiling = 1;
        for (const float peak : c.media->peaks) ceiling = std::max(ceiling, peak);
        for (int x = left; x < right; ++x)
        {
            // Grid lines denote pixel centres (gridPixel rounds to nearest).
            // Sample the same centred interval, rather than drawing a [x,x+1)
            // interval at its left edge, which makes an onset lead its tick.
            const double local = (x - 0.5) / pixelsPerSecond - c.start;
            const auto level = c.waveformRange(local, local + 1 / pixelsPerSecond);
            levels.push_back(level); ceiling = std::max(ceiling, level.peak);
        }
        const int mode = owner.waveformMode.getSelectedId();
        float forgePoolY = baseline;
        juce::Path waveform, body; waveform.startNewSubPath((float) left, baseline); body.startNewSubPath((float) left, baseline);
        for (int x = left; x < right; ++x)
        {
            const auto level = levels[(size_t) (x - left)]; const float rms = level.rms / ceiling;
            // Fixed mean-square energy scale exposes structure without viewport
            // normalization: scrolling or zooming cannot inflate a quiet passage.
            const float contour = mode == 1 && pixelsPerSecond < 80 ? std::min(1.0f, 2.5f * rms * rms) : rms;
            if (x == (int) forgeX) forgePoolY = baseline - (mode == 2 ? level.peak / ceiling : contour) * height * .45f;
            waveform.lineTo(x + .5f, baseline - level.peak * height / ceiling); body.lineTo(x + .5f, baseline - contour * height);
        }
        waveform.lineTo((float) right, baseline); waveform.closeSubPath();
        body.lineTo((float) right, baseline); body.closeSubPath();
        auto drawWaveform = [&](float opacity)
        {
            if (mode != 1 || pixelsPerSecond >= 80)
            {
                g.setColour(accent.withAlpha(opacity * (mode == 2 ? 0.6f : 0.12f))); g.fillPath(waveform);
                g.setColour(accent.withAlpha(opacity * (mode == 2 ? 0.9f : 0.35f))); g.strokePath(waveform, juce::PathStrokeType(0.8f));
            }
            if (mode != 2)
            { g.setColour(accent.withAlpha(opacity * .72f)); g.fillPath(body); g.setColour(accent.withAlpha(opacity)); g.strokePath(body, juce::PathStrokeType(0.8f)); }
        };
        const auto playback = p.transitionPlaybackFor(c);
        if (!playback.startCut && !playback.endCut) drawWaveform(1);
        else
        {
            // Keep excluded source material as a ghost; editing handles and
            // automation remain fully visible and keep their original clocks.
            const double begin = std::max(c.start, playback.startCut.value_or(c.start));
            const double end = std::min(c.end(), playback.endCut.value_or(c.end()));
            auto drawRegion = [&](double from, double to, float opacity)
            {
                if (to <= from) return;
                juce::Graphics::ScopedSaveState active(g); juce::Path audible;
                audible.addRectangle(r.getX() + (float) ((from - c.start) * pixelsPerSecond), r.getY(), (float) ((to - from) * pixelsPerSecond), r.getHeight());
                g.reduceClipRegion(audible); drawWaveform(opacity);
            };
            if (end <= begin) drawWaveform(.14f);
            else { drawRegion(c.start, begin, .14f); drawRegion(begin, end, 1); drawRegion(end, c.end(), .14f); }
        }
        if (owner.showEnergySections) paintEnergySections(g, c, r, pixelsPerSecond);
        if (forging && forgeEligible(c))
        {
            juce::Graphics::ScopedSaveState hotRegion(g);
            g.reduceClipRegion(juce::Rectangle<int>((int) forgeX - 150, (int) r.getY() + 24, 164, std::max(1, (int) r.getHeight() - 24)));
            const float begin = (float) (std::max(c.start, playback.startCut.value_or(c.start)) * pixelsPerSecond);
            const float end = (float) (std::min(c.end(), playback.endCut.value_or(c.end())) * pixelsPerSecond);
            g.reduceClipRegion(juce::Rectangle<int>((int) std::ceil(begin), (int) r.getY(), std::max(0, (int) std::floor(end) - (int) std::ceil(begin)), (int) r.getHeight()));
            juce::ColourGradient molten(colour::moltenOrange.withAlpha(0.f), forgeX - 150, baseline,
                                        colour::moltenCore, forgeX, baseline, false);
            molten.addColour(.7, colour::moltenOrange.withAlpha(.8f));
            g.setGradientFill(molten); g.fillPath(mode == 2 ? waveform : body);
            g.strokePath(mode == 2 ? waveform : body, juce::PathStrokeType(1.4f));
            const float poolY = forgePoolY;
            g.setGradientFill(juce::ColourGradient(colour::moltenCore.withAlpha(.8f), forgeX, poolY,
                                                 colour::moltenOrange.withAlpha(0.f), forgeX + 22, poolY, true));
            g.fillEllipse(forgeX - 22, poolY - 22, 44, 44);
            drawForgeSparks(g, {forgeX, poolY}, 70, owner.peakDisplay.clock, .9f, c.track);
        }
        // The mix clock owns full-height guides. Detected source pulses
        // remain short ticks, including genuine offsets after free edits.
        for (const auto& guide : guides)
        {
            g.setColour(guide.beat % 16 == 0 ? colour::text.withAlpha(0.24f) : colour::text.withAlpha(guide.beat % 4 == 0 ? 0.13f : 0.055f));
            g.drawVerticalLine(guide.x, r.getY() + 23, baseline);
        }
        if (c.media->bpm() > 0)
        {
            const double sourceBeat = 60.0 / c.media->bpm();
            const double beatDuration = c.localTimeForSource(sourceBeat);
            const double firstBeat = c.firstBeatTime();
            double firstSourceBeat = c.audioSourceElapsed(firstBeat);
            const double downbeat = c.firstDownbeatTime();
            const double barSource = downbeat >= 0 ? c.audioSourceElapsed(downbeat) : -1;
            int clipStride = beatDuration * pixelsPerSecond < 12 ? 4 : 1;
            if (clipStride == 4 && downbeat >= 0) firstSourceBeat = barSource;
            int64_t n = std::max<int64_t>(0, (int64_t) std::floor((c.audioSourceElapsed((left - r.getX()) / pixelsPerSecond) - firstSourceBeat) / (sourceBeat * clipStride)));
            for (;; ++n) { const double t = c.audioTimelineTime(firstSourceBeat + (double) n * sourceBeat * clipStride); if (t >= c.length || r.getX() + t * pixelsPerSecond >= right) break;
                if (t >= 0) {
                    const double sourceTime = firstSourceBeat + (double) n * sourceBeat * clipStride;
                    const int inBar = downbeat < 0 ? -1 : (int) ((std::llround((sourceTime - barSource) / sourceBeat) * (c.reversed ? -1 : 1) % 4 + 4) % 4);
                    const int x = Clip::gridPixel(c.beatTimelineTime(sourceTime), pixelsPerSecond);
                    // At overview zoom the shared four-bar grid carries the
                    // structure; avoid a bright fence of individual downbeats.
                    g.setColour(inBar == 0 ? colour::text.withAlpha(0.24f) : accent.withAlpha(0.16f));
                    if (beatDuration * clipStride * pixelsPerSecond >= 6) g.drawVerticalLine(x, baseline - (inBar == 0 ? 8 : 4), baseline);
                    if (sourceBeat * pixelsPerSecond > 34) { g.setFont(juce::FontOptions(10)); g.drawText(inBar < 0 ? "|" : juce::String(inBar + 1), x + 3, (int) baseline - 16, 20, 13, juce::Justification::left); }
                } }
            const double anchor = c.media->downbeatOrigin();
            if (anchor >= 0 && !c.reversed && c.loopSpan <= 0 && anchor >= c.offset)
            {
                const double local = c.audioTimelineTime(anchor - c.offset);
                if (local >= 0 && local < c.length) { const float x = r.getX() + (float) (local * pixelsPerSecond); juce::Path flag;
                    flag.addTriangle(x - 4, baseline, x + 4, baseline, x, baseline - 7); g.setColour(colour::orange); g.fillPath(flag); }
            }
        }
        g.setColour(accent.brighter(0.2f));
        juce::Path envelope;
        const auto fades = c.envelope();
        const double intro = fades.intro, outro = fades.outro;
        envelope.startNewSubPath(r.getX(), r.getBottom() - 5);
        envelope.lineTo(r.getX() + (float) (intro * pixelsPerSecond), baseline);
        envelope.lineTo(r.getX() + (float) ((intro + c.fadeIn) * pixelsPerSecond), r.getY() + 29);
        envelope.lineTo(r.getX() + (float) (outro * pixelsPerSecond), r.getY() + 29);
        envelope.lineTo(r.getX() + (float) (std::min(c.length, outro + c.fadeOut) * pixelsPerSecond), baseline);
        envelope.lineTo(r.getRight(), r.getBottom() - 5); g.strokePath(envelope, juce::PathStrokeType(1.2f));
        for (const bool isIntro : {true, false})
        {
            const float x = r.getX() + (float) (c.transitionTime(isIntro) * pixelsPerSecond);
            g.setColour(colour::orange); g.drawVerticalLine((int) x, r.getY(), baseline);
            juce::Path handle; handle.addTriangle(x - 5, r.getY(), x + 5, r.getY(), x, r.getY() + 7);
            handle.addTriangle(x - 4, baseline, x + 4, baseline, x, baseline - 6); g.fillPath(handle);
            if (c.id == owner.selectedClip) { g.setFont(juce::FontOptions(9)); g.drawText(isIntro ? "IN" : "OUT", (int) x + 5, (int) r.getY() + 20, 26, 12, juce::Justification::left); }
        }
        if (c.id == owner.loopStartClip)
        {
            const float x = r.getX() + (float) (c.localTimeForSource(owner.loopStartSource) * pixelsPerSecond);
            g.setColour(colour::focus); g.drawVerticalLine((int) x, r.getY() + 23, baseline);
            g.setFont(juce::FontOptions(10)); g.drawText("LOOP START", (int) x + 4, (int) baseline - 16, 80, 14, juce::Justification::left);
        }
    }
    {
        for (int track = 0; track < trackCount; ++track)
        for (const auto& address : owner.automationAddresses(track))
        {
            if (laneSize(track) < 30) continue;
            const auto* displayed = p.curve(address); if (!displayed) continue; const auto& curve = *displayed;
            const auto* clip = p.clipFor(address.clipId);
            const double begin = std::max(clip ? clip->start : 0.0, visible.getX() / pixelsPerSecond);
            const double end = std::min(clip ? clip->end() : visible.getRight() / pixelsPerSecond, visible.getRight() / pixelsPerSecond);
            if (end <= begin) continue;
            const float manual = p.manualValue(address); const int target = (int) address.target;
            const bool combined = hasGeneratedAutomation(address);
            if (combined && !owner.transitionCurveVisible(target)) continue;
            const auto playback = clip ? p.transitionPlaybackFor(*clip) : TransitionPlayback {};
            auto y = [&](double time, float value) { return automationY(track, automationDisplayValue(address, time, value, &playback), target); };
            auto valueAt = [&](double time) { return combined ? curve.valueAt(p.automationTime(address, time), manual) : curve.interpolatedValue(p.automationTime(address, time), manual); };
            juce::Path line; line.startNewSubPath((float) (begin * pixelsPerSecond), y(begin, valueAt(begin)));
            if (combined)
            {
                double previous = begin;
                auto append = [&](double time)
                {
                    for (const auto& pair : playback.pairs)
                    {
                        const auto& envelope = pair[(size_t) target];
                        if (envelope.instant && envelope.begin > previous && envelope.begin <= time)
                        {
                            const double before = std::nextafter(envelope.begin, -std::numeric_limits<double>::infinity());
                            const float x = (float) (envelope.begin * pixelsPerSecond);
                            line.lineTo(x, y(before, valueAt(before))); line.lineTo(x, y(envelope.begin, valueAt(envelope.begin)));
                        }
                    }
                    line.lineTo((float) (time * pixelsPerSecond), y(time, valueAt(time))); previous = time;
                };
                for (int x = (int) std::ceil(begin * pixelsPerSecond); x < end * pixelsPerSecond; ++x) append(x / pixelsPerSecond);
                append(end);
            }
            else for (const auto& point : curve.points)
            {
                const double time = p.automationSessionTime(address, point.time);
                if (time >= begin && time <= end) line.lineTo((float) (time * pixelsPerSecond), y(time, point.value));
            }
            line.lineTo((float) (end * pixelsPerSecond), y(end, valueAt(end)));
            const std::array<juce::Colour, 4> bands {juce::Colour(0xffffad70), juce::Colour(0xffe4d15b), juce::Colour(0xff76c7ef), juce::Colour(0xffd3a3ef)};
            const auto accent = target < 4 ? bands[(size_t) target] : juce::Colour::fromHSV(0.08f + target * 0.115f, 0.65f, 0.95f, 1.0f);
            g.setColour(accent.withAlpha(curve.active() || combined ? 0.85f : 0.4f)); g.strokePath(line, juce::PathStrokeType(1.7f));
            for (size_t pointIndex = 0; pointIndex < curve.points.size(); ++pointIndex)
            {
                const auto& point = curve.points[pointIndex];
                const double time = p.automationSessionTime(address, point.time);
                if (time < begin || time > end) continue;
                const float x = (float) (time * pixelsPerSecond), py = y(time, point.value);
                g.fillEllipse(x - 4, py - 4, 8, 8);
                const auto caption = automationPointLabel(address, (int) pointIndex);
                if (caption.isNotEmpty())
                {
                    juce::Graphics::ScopedSaveState valueStyle(g);
                    g.setColour(colour::text); g.setFont(juce::FontOptions(11));
                    g.drawText(caption, (int) x + 6, (int) py - 15, 48, 14, juce::Justification::left);
                }
            }
        }
    }
    if (p.clips.empty() && owner.automationLane() < 0)
    {
        g.setColour(colour::muted); g.setFont(juce::FontOptions(16));
        g.drawText("Your mix starts here", visible.withY(160).withHeight(30), juce::Justification::centred);
        g.setFont(juce::FontOptions(12)); g.drawText("Drag a library track or audio file onto a lane to place it.", visible.withY(195).withHeight(25), juce::Justification::centred);
    }
    for (const auto& marker : p.labels) {
        g.setColour(colour::text.withAlpha(.25f));
        g.drawVerticalLine(Clip::gridPixel(marker.time, pixelsPerSecond), 0, (float) getHeight());
    }
    paintTransitions(g); paintAudioSlipHandles(g);
    const int playhead = Clip::gridPixel(owner.engine.position.load(), pixelsPerSecond);
    if (forging)
    {
        const auto seam = juce::Rectangle<float>((float) playhead - 1, (float) visible.getY(), 2, (float) visible.getHeight());
        AlloyLookAndFeel::drawMoltenMetal(g, seam, true, 1);
    }
    g.setColour(colour::text); g.drawVerticalLine(playhead, 0, (float) getHeight());
    paintDropPreview(g);
}
juce::Rectangle<float> Timeline::transitionBounds(const Project::TransitionLink& link) const
{
    const auto& p = owner.engine.project; const auto* out = p.clipFor(link.outgoing); const auto* in = p.clipFor(link.incoming);
    if (!out || !in || laneSize(out->track) < 55 || laneSize(in->track) < 55) return {};
    const auto range = p.transitionRange(link);
    const float x = (float) (range.getStart() * pixelsPerSecond), right = (float) (range.getEnd() * pixelsPerSecond);
    const float top = laneY(std::min(out->track, in->track), 26);
    return {x, top, std::max(24.f, right - x), (float) laneTop(std::max(out->track, in->track) + 1) - top - 8};
}
juce::Rectangle<float> Timeline::transitionSongBounds(const Project::TransitionLink& link, bool outgoing) const
{
    const auto& p = owner.engine.project; const auto* clip = p.clipFor(outgoing ? link.outgoing : link.incoming);
    const auto* partner = p.clipFor(outgoing ? link.incoming : link.outgoing); if (!clip || !partner) return {};
    const auto box = transitionBounds(link); if (box.isEmpty()) return {};
    const float y = laneY(clip->track, laneHeight - (outgoing && clip->track == partner->track ? 49.f : 29.f));
    return {box.getX() + 6, y, std::max(12.f, std::min(132.f, box.getWidth() - 12)), 18};
}
double Timeline::transitionReference(const Project& p, const std::pair<juce::String, Project::TransitionEdit>& hit) const
{
    const auto link = std::find_if(p.transitionLinks.begin(), p.transitionLinks.end(), [&](const auto& x) { return x.incoming == hit.first; });
    if (link == p.transitionLinks.end()) return 0;
    if (hit.second == Project::TransitionEdit::moveOutgoing) return p.clipFor(link->outgoing)->start;
    if (hit.second == Project::TransitionEdit::moveIncoming) return p.clipFor(link->incoming)->start;
    const auto range = p.transitionRange(*link); return hit.second == Project::TransitionEdit::resizeEnd ? range.getEnd() : range.getStart();
}
std::optional<std::pair<juce::String, Project::TransitionEdit>> Timeline::transitionAt(juce::Point<float> point) const
{
    if (owner.engine.project.hasPendingPlacement()) return {};
    const auto& links = owner.engine.project.transitionLinks;
    for (auto it = links.rbegin(); it != links.rend(); ++it)
    {
        const auto box = transitionBounds(*it); if (box.isEmpty()) continue;
        if (transitionSongBounds(*it, true).contains(point)) return {{it->incoming, Project::TransitionEdit::moveOutgoing}};
        if (transitionSongBounds(*it, false).contains(point)) return {{it->incoming, Project::TransitionEdit::moveIncoming}};
        if (point.y < box.getY() || point.y > box.getY() + 18) continue;
        if (std::abs(point.x - box.getX()) < 5) return {{it->incoming, Project::TransitionEdit::resizeStart}};
        if (std::abs(point.x - box.getRight()) < 5) return {{it->incoming, Project::TransitionEdit::resizeEnd}};
        if (box.withHeight(18).contains(point)) return {{it->incoming, Project::TransitionEdit::moveWindow}};
    }
    return {};
}
void Timeline::paintTransitions(juce::Graphics& g)
{
    const auto& p = owner.engine.project;
    const auto visible = g.getClipBounds();
    const TempoMap clock = p.followTrackTempo ? p.tempo : TempoMap {p.bpm, {}};
    const auto pulse = AlloyLookAndFeel::fourCountPulse(clock.beatsAt(owner.engine.position.load()), owner.engine.playing.load(), AlloyLookAndFeel::motionAllowed());
    for (const auto& link : p.transitionLinks)
    {
        const auto box = transitionBounds(link); if (box.isEmpty() || !box.expanded(4).intersects(visible.toFloat())) continue;
        const bool selected = owner.selectedTransition == link.incoming;
        const auto accent = colour::moltenOrange.interpolatedWith(colour::moltenCore, .1f + pulse.heat * .2f);
        g.setColour(colour::moltenOrange.withAlpha(.025f + pulse.heat * .035f)); g.fillRect(box);
        for (int layer = 3; layer > 0; --layer)
        {
            g.setColour(colour::moltenOrange.withAlpha((selected ? .035f : .018f) + pulse.heat * .018f));
            g.drawRect(box.expanded((float) layer), 1.5f);
        }
        g.setColour(accent.withAlpha(selected ? 1.f : .85f)); g.drawRect(box, selected ? 1.8f : 1.2f);
        const auto header = box.withHeight(18);
        g.setGradientFill(juce::ColourGradient(colour::moltenEdge.darker(.45f), header.getTopLeft(), colour::panel, header.getBottomLeft(), false));
        g.fillRect(header);
        AlloyLookAndFeel::drawMoltenMetal(g, header.withHeight(1.6f), false, pulse.heat);
        g.setColour(colour::text); g.fillRect(box.getX(), box.getY() + 4, 3.f, 10.f); g.fillRect(box.getRight() - 3, box.getY() + 4, 3.f, 10.f);
        const auto range = p.transitionRange(link);
        const double beats = clock.beatsAt(range.getEnd()) - clock.beatsAt(range.getStart());
        auto title = header.reduced(6, 0);
        g.setFont(juce::FontOptions(10).withStyle("Bold"));
        if (box.getWidth() >= 200)
        {
            auto count = title.removeFromLeft(48);
            for (int beat = 0; beat < 4; ++beat)
            {
                const auto cell = count.removeFromLeft(10);
                if (beat == pulse.count)
                {
                    g.setColour(colour::moltenOrange.withAlpha(.15f + pulse.heat * .1f)); g.fillRoundedRectangle(cell.reduced(.5f, 2), 2);
                    g.setColour(colour::moltenCore);
                }
                else g.setColour(colour::orange.withAlpha(.65f));
                g.drawText(juce::String(beat + 1), cell, juce::Justification::centred);
            }
        }
        if (link.locked)
        {
            const auto badge = title.removeFromRight(8).reduced(1, 4);
            g.setColour(juce::Colours::mediumseagreen); g.fillRoundedRectangle(badge, 2);
        }
        g.setColour(colour::text);
        g.drawText((link.locked ? "LOCKED / " : "TRANSITION / ") + juce::String(beats / 4, 1) + " bars", title, juce::Justification::centred);
        if (owner.selectedTransition == link.incoming && range.getLength() > 1e-6)
        {
            const std::array<juce::Colour, 4> colours {juce::Colour(0xffffad70), juce::Colour(0xffe4d15b), juce::Colour(0xff76c7ef), juce::Colour(0xffd3a3ef)};
            for (const auto* clip : {p.clipFor(link.outgoing), p.clipFor(link.incoming)})
            {
                const auto playback = p.transitionPlaybackFor(*clip);
                if (laneSize(clip->track) < 55) continue;
                const AutomationAddress address {clip->track, AutomationTarget::volume, {}, {}, clip->id};
                const int begin = std::max(visible.getX(), (int) std::floor(range.getStart() * pixelsPerSecond));
                const int end = std::min(visible.getRight(), (int) std::ceil(range.getEnd() * pixelsPerSecond));
                for (size_t band = 0; band < 4; ++band) if (owner.transitionCurveVisible((int) band) && link.bands[band] && link.bands[band]->generated() && clip->controls.automation[band].points.empty())
                {
                    juce::Path curve;
                    auto bandAddress = address; bandAddress.target = (AutomationTarget) band;
                    auto y = [&](double time) { return automationY(clip->track, automationDisplayValue(bandAddress, time, p.manualValue(bandAddress), &playback), (int) band); };
                    for (int x = begin; x <= end; ++x)
                    {
                        const double time = x / pixelsPerSecond;
                        if (x == begin) curve.startNewSubPath((float) x, y(time));
                        else
                        {
                            for (const auto& pair : playback.pairs)
                            {
                                const auto& envelope = pair[band];
                                if (envelope.instant && envelope.begin > (x - 1) / pixelsPerSecond && envelope.begin <= time)
                                {
                                    const float edge = (float) (envelope.begin * pixelsPerSecond);
                                    curve.lineTo(edge, y(std::nextafter(envelope.begin, -std::numeric_limits<double>::infinity())));
                                    curve.lineTo(edge, y(envelope.begin));
                                }
                            }
                            curve.lineTo((float) x, y(time));
                        }
                    }
                    g.setColour(colours[band].withAlpha(.88f)); g.strokePath(curve, juce::PathStrokeType(1.7f));
                }
            }
        }
        for (bool outgoing : {true, false})
        {
            const auto handle = transitionSongBounds(link, outgoing);
            g.setColour(colour::panel); g.fillRect(handle); g.setColour(accent); g.drawRect(handle);
            g.drawText(outgoing ? "< OUTGOING >" : "< INCOMING >", handle, juce::Justification::centred);
        }
    }
    paintSwapHandles(g);
}
int Timeline::labelAt(juce::Point<float> position) const
{
    if (position.y < -labelHeight || position.y >= 0) return -1;
    const auto& markers = owner.engine.project.labels;
    for (int i = (int) markers.size() - 1; i >= 0; --i)
    {
        const float x = (float) (markers[(size_t) i].time * pixelsPerSecond);
        const float next = i + 1 < (int) markers.size() ? (float) (markers[(size_t) i + 1].time * pixelsPerSecond) - 4 : x + 190;
        if (position.x >= x - 4 && position.x <= std::max(x + 4, std::min(x + 190, next))) return i;
    }
    return -1;
}
void Timeline::mouseDown(const juce::MouseEvent& e)
{
    if (e.y >= 0 && !e.mods.isPopupMenu() && (resizingLane = dividerAt(e.position.y)) >= 0)
    { resizeStartHeight = laneSize(resizingLane); origin = e.getPosition(); return; }
    if (owner.busy) return;
    dragging.clear(); dragClip.reset(); clipDrag.reset(); anchorDrag.reset(); transitionDrag.reset(); swapDrag.reset(); audioSlipDrag.reset(); draggingLabel.clear(); moved = false;
    editingAutomationPoint.reset();
    if (e.y >= -labelHeight && e.y < 0)
    {
        owner.tempoStrip.selectedTempoPoint = -1; selectedLabel.clear();
        const int index = labelAt(e.position);
        if (index >= 0)
        {
            const auto& marker = owner.engine.project.labels[(size_t) index]; selectedLabel = marker.id;
            if (e.mods.isPopupMenu()) { owner.removeLabelMarker(); return; }
            draggingLabel = marker.id; dragStart = marker.time; origin = e.getPosition(); owner.engine.seek(marker.time);
            owner.setStatus(marker.text + " / " + timeText(marker.time) + " / drag to move, double-click to edit, right-click to remove");
        }
        else owner.engine.seek(std::max(0.0, e.x / pixelsPerSecond));
        owner.refresh(); return;
    }
    selectedLabel.clear();
    owner.tempoStrip.selectedTempoPoint = -1;
    if (beginAudioSlip(e)) return;
    if (beginSwapDrag(e)) return;
    if (!e.mods.isPopupMenu() && !automationPointAt(e.position)) if (const auto hit = transitionAt(e.position))
    {
        transitionDrag = hit; dragStart = transitionReference(owner.engine.project, *hit); origin = e.getPosition();
        owner.selectTransition(hit->first);
        owner.setStatus("Transition: drag the frame, either top corner, or an OUTGOING / INCOMING song handle. Alt bypasses snapping."); return;
    }
    if (!e.mods.isPopupMenu()) if (const auto anchor = anchorAt(e.position))
    {
        owner.selectedClip = anchor->first;
        const auto* clip = owner.currentClip(); if (!clip) return;
        owner.selectTrack(clip->track); dragging = clip->id; draggingIntro = anchor->second;
        anchorDrag = owner.engine.project; origin = e.getPosition();
        dragStart = clip->start + clip->transitionTime(draggingIntro); owner.refresh();
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor); return;
    }
    if (const auto hit = automationPointAt(e.position))
    {
        if (owner.engine.project.automationLocked(hit->first)) { owner.setStatus("Unlock the transition before editing its automation."); return; }
        owner.selectedClip = hit->first.clipId;
        owner.selectedAutomationTarget = (int) hit->first.target;
        if (hit->first.target == AutomationTarget::plugin) owner.pluginLane = hit->first;
        owner.engine.setAutomationRecordMode(AutomationRecordMode::read);
        const int track = trackAt((float) e.y);
        owner.selectTrack(track); const juce::ScopedLock guard(owner.engine.lock);
        auto* displayed = owner.displayedCurve(track); if (!displayed) return; auto& curve = *displayed;
        int point = hit->second;
        if (e.mods.isPopupMenu())
        {
            owner.remember(); owner.engine.project.automationEdited(hit->first, AutomationEdit::removePoint, curve.points[(size_t) point].time); curve.points.erase(curve.points.begin() + point); owner.refresh();
            return;
        }
        selectAutomationPoint(hit->first, point); owner.refresh(); return;
    }
    for (auto it = owner.engine.project.clips.rbegin(); it != owner.engine.project.clips.rend(); ++it)
        if (clipBounds(*it).contains(e.position))
        {
            if (e.mods.isPopupMenu())
            {
                owner.selectedClip = it->id; owner.selectedLibraryId = it->media->id; owner.selectTrack(it->track); owner.refresh();
                owner.showTimelineMenu(e.x / pixelsPerSecond, it->track);
                return;
            }
            owner.selectedAutomationTarget = -1;
            owner.selectedClip = it->id; owner.selectTrack(it->track); dragging = it->id; dragStart = it->start; dragClip = *it; clipDrag = owner.engine.project; origin = e.getPosition(); owner.refresh(); return;
        }
    if (e.mods.isPopupMenu() && e.y >= 0)
    { owner.selectedClip.clear(); owner.showTimelineMenu(e.x / pixelsPerSecond, trackAt((float) e.y)); return; }
    owner.selectedAutomationTarget = -1;
    owner.engine.seek(std::max(0.0, e.x / pixelsPerSecond)); owner.refresh();
}
void Timeline::mouseDrag(const juce::MouseEvent& e)
{
    if (resizingLane >= 0) { owner.resizeLane(resizingLane, resizeStartHeight + e.y - origin.y); return; }
    if (continueAudioSlip(e)) return;
    if (continueSwapDrag(e)) return;
    if (transitionDrag && !owner.busy && e.mouseWasDraggedSinceMouseDown())
    {
        owner.engine.finishAutomation(); const juce::ScopedLock guard(owner.engine.lock);
        auto candidate = owner.engine.project;
        const double wanted = dragStart + (e.x - origin.x) / pixelsPerSecond;
        const auto result = candidate.editTransition(transitionDrag->first, transitionDrag->second,
            wanted - transitionReference(candidate, *transitionDrag), candidate.snap && !e.mods.isAltDown());
        if (result.failed()) { owner.setStatus(result.getErrorMessage()); return; }
        if (std::abs(transitionReference(candidate, *transitionDrag) - transitionReference(owner.engine.project, *transitionDrag)) < 1e-8) return;
        if (!moved) { owner.remember(); moved = true; }
        owner.engine.project = std::move(candidate); updateSize(); owner.tempoStrip.repaint();
        owner.setStatus("Transition adjusted. Undo restores the complete edit."); return;
    }
    if (anchorDrag && !owner.busy && e.mouseWasDraggedSinceMouseDown())
    {
        owner.engine.finishAutomation();
        auto candidate = *anchorDrag;
        const auto result = candidate.moveTransitionAnchor(dragging, draggingIntro, dragStart + (e.x - origin.x) / pixelsPerSecond, candidate.snap && !e.mods.isAltDown());
        if (result.failed()) { owner.setStatus(result.getErrorMessage()); return; }
        const auto selected = std::find_if(candidate.clips.begin(), candidate.clips.end(), [&](const auto& c) { return c.id == dragging; });
        const auto* current = owner.currentClip();
        if (selected == candidate.clips.end() || !current) return;
        const auto& existing = owner.engine.project.clips;
        if (std::equal(candidate.clips.begin(), candidate.clips.end(), existing.begin(), existing.end(), [](const auto& a, const auto& b)
            { return a.start == b.start && a.introAnchor == b.introAnchor && a.outroAnchor == b.outroAnchor; })) return;
        if (!moved) { owner.remember(); moved = true; }
        { const juce::ScopedLock guard(owner.engine.lock); const auto applied = owner.engine.project.applyPlacements(candidate);
          if (applied.failed()) { owner.setStatus(applied.getErrorMessage()); return; } }
        updateSize(); owner.tempoStrip.repaint();
        owner.setStatus(juce::String(draggingIntro ? "Intro" : "Outro") + " aligned / following tracks repositioned"); return;
    }
    if (draggingLabel.isNotEmpty() && !owner.busy)
    {
        if (!e.mouseWasDraggedSinceMouseDown()) return;
        const auto& labels = owner.engine.project.labels;
        const auto found = std::find_if(labels.begin(), labels.end(), [&](const auto& item) { return item.id == draggingLabel; });
        if (found == labels.end()) return;
        auto marker = *found;
        marker.time = std::min(1.0e9, owner.engine.project.snappedTime(dragStart + (e.x - origin.x) / pixelsPerSecond, owner.engine.project.snap && !e.mods.isAltDown()));
        if (marker.time == found->time) return;
        if (std::any_of(labels.begin(), labels.end(), [&](const auto& item) { return item.id != marker.id && item.time == marker.time; })) return;
        if (!moved) { owner.engine.setAutomationRecordMode(AutomationRecordMode::read); owner.remember(); moved = true; }
        { const juce::ScopedLock guard(owner.engine.lock); owner.engine.project.putLabelMarker(marker); }
        owner.setStatus(marker.text + " / " + timeText(marker.time)); repaint(); return;
    }
    if (editingAutomationPoint && owner.automationLane() >= 0 && !owner.busy)
    {
        const auto address = editingAutomationPoint->first; const int draggingPoint = editingAutomationPoint->second;
        const int automationTrack = address.target == AutomationTarget::master ? 0 : address.track;
        if (!e.mouseWasDraggedSinceMouseDown()) return;
        if (!moved) { owner.remember(); moved = true; }
        const juce::ScopedLock guard(owner.engine.lock);
        auto* displayed = owner.engine.project.curve(address); if (!displayed || draggingPoint >= (int) displayed->points.size()) return;
        const double time = owner.engine.project.snappedTime(e.x / pixelsPerSecond, owner.engine.project.snap && !e.mods.isAltDown());
        const auto result = owner.engine.project.moveAutomationPoint(address, draggingPoint, time, automationManualValue(address, time, automationDb(automationTrack, e.position.y, address.target)));
        if (result.failed()) { owner.setStatus(result.getErrorMessage()); return; }
        displayed = owner.engine.project.curve(address); if (!displayed || draggingPoint >= (int) displayed->points.size()) return;
        const auto& point = displayed->points[(size_t) draggingPoint]; owner.setStatus(timeText(point.time) + " / " + owner.automationText(address.target, automationDisplayValue(address, owner.engine.project.automationSessionTime(address, point.time), point.value))); repaint(); return;
    }
    if (dragging.isEmpty() || !dragClip || owner.busy || !e.mouseWasDraggedSinceMouseDown()) return;
    owner.engine.finishAutomation();
    const juce::ScopedLock guard(owner.engine.lock);
    if (auto* c = owner.currentClip())
    {
        // Background source loading may finish during a gesture. Keep its
        // published media/mapping instead of restoring the mouse-down placeholder.
        if (c->media != dragClip->media || c->tempoMap != dragClip->tempoMap || c->awaitingSourceLength != dragClip->awaitingSourceLength)
        { dragClip = *c; clipDrag = owner.engine.project; dragStart = c->start; origin = e.getPosition(); }
        const double start = std::max(0.0, dragStart + (e.x - origin.x) / pixelsPerSecond);
        const int track = trackAt((float) e.y);
        if (e.x == origin.x && c->start == dragStart && c->track == track) return;
        auto candidate = clipDrag.value_or(owner.engine.project);
        if (e.x != origin.x)
        {
            const auto result = candidate.moveClip(dragging, start, candidate.snap && !e.mods.isAltDown(), 6 / pixelsPerSecond);
            if (result.failed()) { owner.setStatus(result.getErrorMessage()); return; }
        }
        candidate.clipFor(dragging)->track = track;
        if (!moved) { owner.remember(); moved = true; }
        const auto applied = owner.engine.project.applyPlacements(candidate);
        if (applied.failed()) { owner.setStatus(applied.getErrorMessage()); return; }
        owner.engine.project.clipFor(dragging)->track = track; owner.engine.selectedTrack = track;
    }
    repaint();
}
void Timeline::mouseUp(const juce::MouseEvent& event)
{
    if (resizingLane >= 0) { resizingLane = -1; mouseMove(event); return; }
    if (moved) owner.refresh();
    dragging.clear(); dragClip.reset(); clipDrag.reset(); anchorDrag.reset(); transitionDrag.reset(); swapDrag.reset(); audioSlipDrag.reset(); draggingLabel.clear();
    mouseMove(event);
}
void Timeline::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (e.y >= 0) if (const int lane = dividerAt(e.position.y); lane >= 0) { owner.resizeLane(lane, owner.layout.defaultLaneHeight); return; }
    if (owner.busy) return;
    if (resetAudioSlip(e.position)) return;
    if (e.y >= -labelHeight && e.y < 0)
    {
        const int index = labelAt(e.position); owner.editLabelMarker(index >= 0 ? owner.engine.project.labels[(size_t) index].id : juce::String(), owner.engine.project.snappedTime(e.x / pixelsPerSecond, owner.engine.project.snap && !e.mods.isAltDown())); return;
    }
    if (const auto hit = automationPointAt(e.position))
    {
        owner.selectedClip = hit->first.clipId;
        owner.selectedAutomationTarget = (int) hit->first.target;
        if (hit->first.target == AutomationTarget::plugin) owner.pluginLane = hit->first;
        const int track = trackAt((float) e.y);
        selectAutomationPoint(hit->first, hit->second);
        owner.editAutomationPoint(track, hit->second); return;
    }
    owner.engine.seek(e.x / pixelsPerSecond);
}

LibraryTable::LibraryTable(Workstation& w, bool playlist) : owner(w), isPlaylist(playlist)
{
    table.setModel(this); table.setMultipleSelectionEnabled(true); table.setRowHeight(playlist ? 28 : 44); table.setHeaderHeight(26); table.setColour(juce::ListBox::backgroundColourId, colour::panel);
    auto& h = table.getHeader(); h.addColumn(playlist ? "TRACK" : "TITLE", 1, playlist ? 80 : 112, 80, 1000, juce::TableHeaderComponent::defaultFlags);
    h.addColumn(playlist ? "LANE" : "TIME", 2, playlist ? 50 : 62, 50, 160, playlist ? juce::TableHeaderComponent::defaultFlags : juce::TableHeaderComponent::notSortable);
    h.addColumn(playlist ? "START" : "KEY", 3, playlist ? 82 : 68, 60, 220, juce::TableHeaderComponent::defaultFlags);
    h.addColumn(playlist ? "LENGTH" : "BPM", 4, playlist ? 84 : 62, 60, 180, juce::TableHeaderComponent::defaultFlags);
    h.addColumn("ART", 5, playlist ? 36 : 44, 32, 120);
    if (playlist) h.addColumn("TRANSITION IN", 6, 130, 130, 300, juce::TableHeaderComponent::notSortable);
    if (!playlist) { h.moveColumn(2, 4); h.moveColumn(5, 0); }
    else { h.moveColumn(6, 1); h.moveColumn(5, 5); }
    table.setHeaderHeight(26); h.setStretchToFitActive(false);
}
int LibraryTable::getNumRows() { return isPlaylist ? (int) playlistRows.size() : (int) owner.visibleMedia.size(); }
Clip* LibraryTable::clipAtRow(int row) const
{
    return row >= 0 && row < (int) playlistRows.size() ? owner.engine.project.clipFor(playlistRows[(size_t) row]) : nullptr;
}
void LibraryTable::rebuildPlaylist()
{
    juce::StringArray selectedIds; const auto oldRows = table.getSelectedRows();
    for (int i = 0; i < oldRows.size(); ++i) if (oldRows[i] >= 0 && oldRows[i] < (int) playlistRows.size()) selectedIds.add(playlistRows[(size_t) oldRows[i]]);
    if (selectedIds.size() <= 1 || !selectedIds.contains(owner.selectedClip)) { selectedIds.clear(); selectedIds.add(owner.selectedClip); }
    playlistRows.clear(); for (const auto& clip : owner.engine.project.clips) playlistRows.push_back(clip.id);
    if (playlistSortColumn != 0) std::stable_sort(playlistRows.begin(), playlistRows.end(), [&](const auto& a, const auto& b)
    {
        const auto& first = *owner.engine.project.clipFor(a); const auto& second = *owner.engine.project.clipFor(b);
        int order = 0;
        if (playlistSortColumn == 5)
        {
            const auto& x = first.media->assets.artworkGroup; const auto& y = second.media->assets.artworkGroup;
            if (x.isEmpty() != y.isEmpty()) return !x.isEmpty();
            order = x.compare(y);
            if (order == 0) return first.media->libraryTitle().compareNatural(second.media->libraryTitle()) < 0;
        }
        else if (playlistSortColumn == 1) order = first.media->name.compareNatural(second.media->name);
        else
        {
            const double x = playlistSortColumn == 2 ? first.track : playlistSortColumn == 3 ? first.start : first.length;
            const double y = playlistSortColumn == 2 ? second.track : playlistSortColumn == 3 ? second.start : second.length;
            order = x < y ? -1 : x > y ? 1 : 0;
        }
        return playlistSortForward ? order < 0 : order > 0;
    });
    table.updateContent();
    juce::SparseSet<int> selection;
    for (int row = 0; row < (int) playlistRows.size(); ++row) if (selectedIds.contains(playlistRows[(size_t) row])) selection.addRange({row, row + 1});
    table.setSelectedRows(selection, juce::dontSendNotification);
}
void LibraryTable::paintRowBackground(juce::Graphics& g, int row, int w, int h, bool selected)
{
    if (isPlaylist) if (const auto* clip = clipAtRow(row)) selected = clip->id == owner.selectedClip;
    g.fillAll(selected ? colour::orange.withAlpha(0.13f) : (row % 2 ? colour::panel : colour::panel.brighter(0.025f)));
    if (selected) { g.setColour(colour::orange); g.fillRect(0, 0, 3, h); }
    juce::ignoreUnused(w);
}
void LibraryTable::paintCell(juce::Graphics& g, int row, int col, int w, int h, bool)
{
    if (row < 0 || row >= getNumRows()) return;
    juce::String text;
    if (col == 5)
    {
        const auto* clip = isPlaylist ? clipAtRow(row) : nullptr; if (isPlaylist && !clip) return;
        const auto& m = isPlaylist ? clip->media : owner.engine.library[(size_t) owner.visibleMedia[(size_t) row]];
        const int size = std::min(h - 6, w - 6); const juce::Rectangle<int> cover((w - size) / 2, (h - size) / 2, size, size);
        g.setColour(colour::raised); g.fillRoundedRectangle(cover.toFloat(), 3);
        if (m->assets.cover.isValid()) g.drawImageWithin(m->assets.cover, cover.getX(), cover.getY(), cover.getWidth(), cover.getHeight(), juce::RectanglePlacement::centred);
        else { g.setColour(colour::muted); g.setFont(juce::FontOptions(9)); g.drawText("MW", cover, juce::Justification::centred); }
        return;
    }
    if (isPlaylist)
    {
        const auto* clip = clipAtRow(row); if (!clip) return; const auto& c = *clip;
        if (col == 1) text = c.media->name + (!c.media->playbackError.isEmpty() ? " [unavailable]" : !c.media->hasPlayback() ? " [preparing]" : c.beatmatched ? "  [sync]" : ""); else if (col == 2) text = trackLabel(c.track); else if (col == 3) text = timeText(c.start); else text = c.awaitingSourceLength ? "Pending" : timeText(c.length);
    }
    else
    {
        const auto& m = owner.engine.library[(size_t) owner.visibleMedia[(size_t) row]];
        if (col == 1)
        {
            const int x = 10;
            g.setColour(colour::text); g.setFont(juce::FontOptions(12)); g.drawText(m->libraryTitle(), x, 4, std::max(0, w - x - 5), 18, juce::Justification::centredLeft, true);
            g.setColour(colour::muted); g.setFont(juce::FontOptions(11));
            g.drawText(m->assets.artist.isNotEmpty() ? m->assets.artist : m->file.getFileName(), x, 23, std::max(0, w - x - 5), 15, juce::Justification::centredLeft, true); return;
        }
        else if (col == 2) text = timeText(m->duration()).dropLastCharacters(4); else if (col == 3) text = owner.keyText(*m); else text = m->bpm() > 0 ? juce::String(m->bpm(), 2) + (m->analysis.variableTempo && m->manualBpm == 0 ? " ~" : m->manualBpm == 0 && m->analysis.tempoConfidence < 0.2 ? " ?" : "") : "Unknown";
        if (col == 3 && m->key() >= 0)
        {
            auto* selected = owner.currentClip();
            if (selected && harmonicallyCompatible(selected->media->key(), m->key())) { g.setColour(colour::orange.withAlpha(0.12f)); g.fillRoundedRectangle(3, 3, (float) w - 6, (float) h - 6, 3); }
        }
    }
    g.setColour(col == 1 ? colour::text : colour::muted); g.setFont(juce::FontOptions(12)); g.drawText(text, 10, 0, w - 15, h, juce::Justification::centredLeft, true);
}
void LibraryTable::cellDoubleClicked(int row, int, const juce::MouseEvent&)
{
    if (!isPlaylist && row >= 0 && row < getNumRows()) owner.addMedia(owner.visibleMedia[(size_t) row]);
    else if (isPlaylist) { if (const auto* clip = clipAtRow(row)) owner.engine.seek(clip->start); }
}
juce::String LibraryTable::getCellTooltip(int row, int column)
{
    if (row < 0 || row >= getNumRows()) return {};
    const auto* clip = isPlaylist ? clipAtRow(row) : nullptr; if (isPlaylist && !clip) return {};
    const auto& media = isPlaylist ? clip->media : owner.engine.library[(size_t) owner.visibleMedia[(size_t) row]];
    if (!isPlaylist && column == 3)
        return owner.keyText(*media) + " / " + keyNoteName(media->key())
            + ". m means minor; no m means major."
            + (media->manualKey >= 0 ? " Manual key override." : media->key() < 0 || media->key() > 23 ? " No key found." : media->keyConfirmed() ? "" : " ? means uncertain, not a confirmed key.");
    return media->file.getFullPathName() + (media->assets.album.isNotEmpty() ? "\nAlbum: " + media->assets.album : "")
        + (media->assets.cover.isValid() ? "\nArtwork: " + juce::String(media->assets.audioThumbnail ? "audio thumbnail / " : "") + media->assets.coverSource.getFullPathName() : "\nNo local cover artwork found.");
}
void LibraryTable::selectedRowsChanged(int row)
{
    if (isPlaylist && clipAtRow(row)) { const auto& c = *clipAtRow(row); owner.timeline.selectedLabel.clear(); owner.selectedClip = c.id; owner.selectTrack(c.track); owner.refresh(); }
    else if (!isPlaylist) owner.selectedLibraryId = row >= 0 && row < getNumRows() ? owner.engine.library[(size_t) owner.visibleMedia[(size_t) row]]->id : juce::String();
}
void LibraryTable::sortOrderChanged(int column, bool forwards)
{
    if (isPlaylist) { playlistSortColumn = column; playlistSortForward = forwards; rebuildPlaylist(); table.repaint(); return; }
    owner.librarySortColumn = column; owner.librarySortForward = forwards; owner.filterLibrary(); table.updateContent(); table.repaint();
}
PluginWindow::PluginWindow(PluginSlot& slot) : juce::DocumentWindow(slot.description.name, colour::panel, juce::DocumentWindow::closeButton), slotId(slot.id)
{
    setUsingNativeTitleBar(true); auto* editor = slot.processor->createEditorAndMakeActive();
    if (editor == nullptr) editor = new juce::GenericAudioProcessorEditor(*slot.processor);
    setContentOwned(editor, true); setResizable(editor->isResizable(), false); centreWithSize(editor->getWidth(), editor->getHeight()); setVisible(true);
    AlloyLookAndFeel::styleNativeTitleBar(*this);
}
PluginList::PluginList(Workstation& w) : owner(w) { list.setModel(this); list.setRowHeight(36); list.setColour(juce::ListBox::backgroundColourId, colour::background); }
int PluginList::getNumRows() { return (int) owner.engine.pluginChain(owner.engine.selectedTrack.load()).size(); }
void PluginList::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= getNumRows()) return;
    const auto& p = owner.engine.pluginChain(owner.engine.selectedTrack.load())[(size_t) row];
    g.fillAll(selected ? colour::raised : colour::background); g.setColour(p->bypass ? colour::muted : colour::orange); g.fillEllipse(10, 14, 7, 7);
    const int latency = p->unsupportedLatency();
    g.setColour(latency > 0 ? juce::Colours::orangered : colour::text); g.setFont(juce::FontOptions(12)); g.drawText(juce::String(row + 1) + "  " + p->description.name + (p->bypass ? " [bypassed]" : latency > 0 ? " [latency " + juce::String(latency) + " / blocked]" : ""), 27, 0, w - 32, h, juce::Justification::left);
}
void PluginList::listBoxItemDoubleClicked(int row, const juce::MouseEvent&) { owner.showEditor(row); }
juce::String PluginList::getTooltipForRow(int row)
{
    const auto& chain = owner.engine.pluginChain(owner.engine.selectedTrack.load());
    if (row < 0 || row >= (int) chain.size()) return {};
    const auto& slot = *chain[(size_t) row]; const auto issue = slot.latencyError();
    return issue.isNotEmpty() ? issue : slot.description.name + (slot.bypass ? " / bypassed" : "");
}
TrackStrip::TrackStrip(Workstation& w, int i) : owner(w), index(i)
{
    setWantsKeyboardFocus(true); setTitle("Resize lane " + juce::String(i + 1));
    setTooltip("Drag the bottom edge to resize this lane; double-click resets. With the edge focused, Up/Down resize, Home resets and End doubles the height.");
    for (auto* c : std::initializer_list<juce::Component*> { &select, &mute, &solo, &lowPass, &gain, &pan }) addAndMakeVisible(c);
    select.setButtonText(trackLabel(i) + "  AUDIO " + juce::String(i + 1)); select.onClick = [this] { owner.selectTrack(index); };
    mute.setClickingTogglesState(true); solo.setClickingTogglesState(true);
    slider(gain, -60, 12, 0.1, " dB"); slider(pan, -1, 1, 0.01); gain.setDoubleClickReturnValue(true, 0); pan.setDoubleClickReturnValue(true, 0);
    pan.configure("Track " + juce::String(i + 1) + " pan");
    pan.textFromValueFunction = [](double value) { return std::abs(value) < 0.005 ? juce::String("C") : juce::String(std::abs(value) * 100, 0) + (value < 0 ? " L" : " R"); };
    pan.valueFromTextFunction = [](const juce::String& text) { const auto v = text.trim().toUpperCase(); return v == "C" ? 0.0 : juce::jlimit(-1.0, 1.0, v.getDoubleValue() / 100 * (v.contains("L") ? -1 : 1)); };
    lowPass.configure("Lane " + juce::String(i + 1) + " low-pass cutoff", 20000);
    lowPass.setRange(20, 20000, 1); lowPass.setSkewFactorFromMidPoint(1000); lowPass.setDoubleClickReturnValue(true, 20000);
    lowPass.setTextValueSuffix(" Hz"); lowPass.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 65, 18);
    lowPass.setTooltip("Low-pass cutoff. Fully open at 20 kHz bypasses the filter. Drag or touch; no arm or MIDI mapping needed.");
    lowPass.onDragStart = [this] { controlGesture = true; if (!owner.engine.canWriteAutomation(index)) owner.remember(); };
    lowPass.onDragEnd = pan.onDragEnd = gain.onDragEnd = [this] { controlGesture = false; };
    lowPass.onValueChange = [this] { if (!owner.refreshing) { if (!controlGesture && !owner.engine.canWriteAutomation(index)) owner.remember(); owner.engine.setControlValue({index, AutomationTarget::lowPass}, (float) lowPass.getValue()); owner.dirty = true; } };
    pan.updateText(); gain.setName("Track " + juce::String(i + 1) + " volume");
    gain.onDragStart = pan.onDragStart = [this] { controlGesture = true; if (!owner.engine.canWriteAutomation(index)) owner.remember(); };
    gain.onValueChange = [this] { if (!owner.refreshing) { if (!controlGesture && !owner.engine.canWriteAutomation(index)) owner.remember(); const juce::ScopedLock guard(owner.engine.lock); owner.engine.setControlValue({index, AutomationTarget::volume}, (float) gain.getValue()); owner.dirty = true; } };
    pan.onValueChange = [this] { if (!owner.refreshing) { if (!controlGesture && !owner.engine.canWriteAutomation(index)) owner.remember(); const juce::ScopedLock guard(owner.engine.lock); owner.engine.setControlValue({index, AutomationTarget::pan}, (float) pan.getValue()); owner.dirty = true; } };
    mute.onClick = [this] { owner.remember(); const juce::ScopedLock guard(owner.engine.lock); owner.engine.project.tracks[(size_t) index].mute = mute.getToggleState(); };
    solo.onClick = [this] { owner.remember(); const juce::ScopedLock guard(owner.engine.lock); owner.engine.project.tracks[(size_t) index].solo = solo.getToggleState(); };
}
void TrackStrip::refresh()
{
    const auto& t = owner.engine.controlSettings(index);
    const auto& mixer = owner.engine.project.tracks[(size_t) index];
    if (!gain.isMouseButtonDown() && !AlloyLookAndFeel::editingSliderText(gain)) gain.setValue(t.gainDb, juce::dontSendNotification);
    if (!pan.isMouseButtonDown() && !AlloyLookAndFeel::editingSliderText(pan)) pan.setValue(t.pan, juce::dontSendNotification);
    gain.setTooltip("Lane volume. A direct move takes control; Record captures a performance.");
    if (!lowPass.isMouseButtonDown() && !AlloyLookAndFeel::editingSliderText(lowPass)) lowPass.setValue(t.lowPassHz, juce::dontSendNotification);
    const bool unlocked = !owner.engine.controlClip(index) || !owner.engine.project.clipTransitionLocked(owner.engine.controlClip(index)->id);
    for (auto* control : {static_cast<juce::Slider*>(&lowPass), static_cast<juce::Slider*>(&pan), &gain}) control->setEnabled(!owner.busy && unlocked);
    select.setButtonText(trackLabel(index) + "  " + (owner.engine.controlClip(index) ? owner.engine.controlClip(index)->media->name : mixer.name) + (std::any_of(t.automation.begin(), t.automation.end(), [](const auto& curve) { return curve.active(); }) || std::any_of(t.pluginAutomation.begin(), t.pluginAutomation.end(), [](const auto& lane) { return lane.curve.active(); }) ? " [A]" : ""));
    mute.setToggleState(mixer.mute, juce::dontSendNotification); solo.setToggleState(mixer.solo, juce::dontSendNotification);
    select.setColour(juce::TextButton::buttonColourId, owner.engine.selectedTrack == index ? colour::orange.withAlpha(0.23f) : colour::raised); repaint();

}
void TrackStrip::paint(juce::Graphics& g)
{
    g.fillAll(colour::panel); g.setColour(colour::border); g.drawHorizontalLine(getHeight() - 1, 0, (float) getWidth());
    if (getHeight() < 30) { g.setColour(colour::tracks[(size_t) index]); g.fillRect(7, getHeight() / 2, getWidth() - 14, 1); return; }
    g.setColour(colour::tracks[(size_t) index]); g.fillRoundedRectangle(1, 5, 2, (float) getHeight() - 13, 1);
    if (getHeight() >= 124)
    {
        g.setColour(colour::muted); g.setFont(juce::FontOptions(10));
        g.drawText("LPF " + (lowPass.getValue() >= 1000 ? juce::String(lowPass.getValue() / 1000, 1) + "k" : juce::String(lowPass.getValue(), 0)), 7, 50, 65, 12, juce::Justification::centred);
        g.drawText("PAN " + pan.getTextFromValue(pan.getValue()), 78, 50, getWidth() - 85, 12, juce::Justification::centred);
    }
    auto level = juce::jlimit(0.0f, 1.0f, owner.engine.meters[(size_t) index].load());
    g.setColour(colour::background); g.fillRect(6, getHeight() - 6, getWidth() - 12, 3);
    AlloyLookAndFeel::drawMoltenMetal(g, {6.f, getHeight() - 6.f, (getWidth() - 12) * level, 3}, false, level);
    if (level >= 1) { g.setColour(colour::error); g.fillRect(getWidth() - 8, getHeight() - 7, 3, 5); }
    if (owner.forgeMotionAllowed() && level >= .89f
        && std::fmod(owner.peakDisplay.clock + index * .31, .73) < .22)
        drawForgeSparks(g, {6 + (getWidth() - 12) * level, getHeight() - 6.f}, 18, owner.peakDisplay.clock, .8f, index);
}
void TrackStrip::resized()
{
    const int h = getHeight(), half = (getWidth() - 21) / 2;
    for (auto* c : std::initializer_list<juce::Component*> {&select, &mute, &solo, &lowPass, &pan, &gain}) c->setVisible(h >= 30);
    if (h < 30) return;
    select.setBounds(7, 4, getWidth() - 14, 20);
    mute.setBounds(7, 28, half, 20); solo.setBounds(14 + half, 28, half, 20);
    gain.setBounds(7, h - 28, getWidth() - 14, 20); gain.setTextBoxStyle(juce::Slider::TextBoxRight, false, 57, 18);
    const bool dials = h >= 100;
    const int dialTop = h >= 124 ? 63 : 50;
    lowPass.setBounds(7, dialTop, half, std::max(20, h - 31 - dialTop));
    pan.setBounds(14 + half, dialTop, half, std::max(20, h - 31 - dialTop));
    for (auto* dial : {&lowPass, &pan})
    {
        dial->setVisible(dials);
        dial->setTextBoxStyle(h >= 164 ? juce::Slider::TextBoxBelow : juce::Slider::NoTextBox, false, half, 18);
    }
    mute.setVisible(h >= 76); solo.setVisible(h >= 76); gain.setVisible(h >= 76);

}

void TrackStrip::mouseMove(const juce::MouseEvent& e) { setMouseCursor(e.y >= getHeight() - 5 ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor); }
void TrackStrip::mouseDown(const juce::MouseEvent& e) { resizingLane = e.y >= getHeight() - 5; resizeStartHeight = getHeight(); if (resizingLane) grabKeyboardFocus(); }
void TrackStrip::mouseDrag(const juce::MouseEvent& e) { if (resizingLane) owner.resizeLane(index, resizeStartHeight + e.getDistanceFromDragStartY()); }
void TrackStrip::mouseDoubleClick(const juce::MouseEvent&) { owner.resizeLane(index, owner.layout.defaultLaneHeight); }

bool TrackStrip::keyPressed(const juce::KeyPress& key)
{
    const int code = key.getKeyCode();
    if (code == juce::KeyPress::upKey || code == juce::KeyPress::downKey)
    { owner.resizeLane(index, getHeight() + (code == juce::KeyPress::upKey ? -1 : 1) * (key.getModifiers().isShiftDown() ? 1 : 8)); return true; }
    if (code == juce::KeyPress::homeKey || code == juce::KeyPress::endKey)
    { owner.resizeLane(index, owner.layout.defaultLaneHeight * (code == juce::KeyPress::homeKey ? 1 : 2)); return true; }
    return false;
}

Workstation::Workstation(bool openAudioDevice) : timeline(*this), timelineRuler(*this, false), timelineOverview(*this, true), tempoStrip(*this), libraryTable(*this, false), playlistTable(*this, true), pluginList(*this), piano(engine.keyboard, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel(&look); setWantsKeyboardFocus(true);
    for (auto* c : std::initializer_list<juce::Component*> {&libraryTab, &playlistTab, &leftSplitter, &rightSplitter, &bottomSplitter}) addAndMakeVisible(c);
    for (auto* b : {&libraryTab, &playlistTab, &clipTab, &effectsTab, &midiTab}) b->getProperties().set("alloyTab", true);
    libraryTab.onClick = [this] { showBrowserPage(0); }; playlistTab.onClick = [this] { showBrowserPage(1); };
    leftSplitter.onValueChange = [this] { resizeSidePanel(true, (int) leftSplitter.getValue()); };
    bottomSplitter.reset = [this] { resizeTimeline(0); };
    bottomSplitter.onValueChange = [this] { resizeTimeline((int) bottomSplitter.getValue()); };
    rightSplitter.onValueChange = [this] { resizeSidePanel(false, (int) rightSplitter.getValue()); };
    for (auto* child : std::initializer_list<juce::Component*> {&menuBar, &displayButton, &windowModeButton, &minimiseButton, &closeAppButton, &trackHeaders, &clipTab, &effectsTab, &midiTab, &clipDetail, &sidebarHelp}) addAndMakeVisible(child);
    displayButton.setTooltip("Window mode, 16:9 window size and monitor. Also available under View / Display.");
    windowModeButton.setTooltip("Switch between full-monitor fullscreen and your previous window size (F11).");
    minimiseButton.setTitle("Minimise"); minimiseButton.setTooltip("Minimise Mixworks");
    closeAppButton.setTitle("Close Mixworks"); closeAppButton.setTooltip("Close Mixworks (Alt+F4)");
    displayButton.onClick = [this]
    {
        if (auto* window = findParentComponentOfClass<AppWindow>())
            window->displayMenu().showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&displayButton),
                [safe = juce::Component::SafePointer<AppWindow>(window)](int action) { if (safe) safe->performDisplayAction(action); });
    };
    windowModeButton.onClick = [this] { toggleWindowMode(); };
    minimiseButton.onClick = [this] { if (auto* window = findParentComponentOfClass<AppWindow>()) window->setMinimised(true); };
    closeAppButton.onClick = [this] { if (auto* window = findParentComponentOfClass<AppWindow>()) window->closeButtonPressed(); };
    label(clipDetail, {}, 13); clipDetail.setColour(juce::Label::textColourId, colour::text); clipDetail.setJustificationType(juce::Justification::topLeft);
    label(sidebarHelp, {}, 12); sidebarHelp.setJustificationType(juce::Justification::topLeft);
    initialiseMasteringUI();
    initialiseTransitionUI();
    initialisePerformance();
    clipTab.onClick = [this] { showInspectorPage(0); }; effectsTab.onClick = [this] { showInspectorPage(1); }; midiTab.onClick = [this] { showInspectorPage(2); };
    for (auto* c : std::initializer_list<juce::Component*> { &newButton, &openButton, &saveButton, &importButton, &exportButton, &settingsButton, &playButton, &stopButton, &homeButton, &undoButton, &redoButton, &splitButton, &duplicateButton, &removeButton, &zoomSlider, &snapButton, &tempoSlider, &masterSlider, &monitorMute, &timeLabel, &inspectorLabel, &pluginLabel, &masterLabel, &libraryTable.table, &playlistTable.table, &viewport, &timelineRuler, &timelineOverview, &tempoStrip, &search, &pluginList.list, &addPluginButton, &editPluginButton, &bypassPluginButton, &removePluginButton, &pluginControlsButton, &piano, &midiLabel, &trackInfoButton, &keyDisplay, &laneAmount, &beatmatchButton }) addAndMakeVisible(c);
    viewport.setViewedComponent(&timeline, false); viewport.setScrollBarsShown(false, false, true, true);
    viewport.setScrollBarThickness(WorkspaceLayout::timelineScrollbarThickness);
    viewport.getHorizontalScrollBar().getProperties().set("alloyMolten", true);
    viewport.getVerticalScrollBar().getProperties().set("alloyMolten", true);
    masterSlider.getProperties().set("alloyMolten", true); zoomSlider.getProperties().set("alloyMolten", true);
    playButton.getProperties().set("alloyMolten", true);
    addAndMakeVisible(waveformMode);
    waveformMode.addItem("Waveform: energy", 1); waveformMode.addItem("Waveform: peaks", 2); waveformMode.addItem("Waveform: detail", 3); waveformMode.setSelectedId(1, juce::dontSendNotification);
    waveformMode.setTooltip("Energy reveals quiet sections, builds and drops; close zoom also resolves transients. Detail always adds a peak outline; Peaks shows amplitude only. Strong grid lines mark four bars (16 beats in 4/4). Display changes never affect audio.");
    waveformMode.onChange = [this] { timeline.repaint(); };
    for (int i = 0; i < trackCount; ++i) { strips[(size_t) i] = std::make_unique<TrackStrip>(*this, i); trackHeaders.addAndMakeVisible(*strips[(size_t) i]); }
    label(inspectorLabel, "CLIP INSPECTOR"); label(pluginLabel, "VST3 INSERTS / AUDIO 1"); label(masterLabel, "MASTER"); label(timeLabel, "00:00.000", 21); timeLabel.setColour(juce::Label::textColourId, colour::orange);
    slider(tempoSlider, 20, 400, 0.01, " BPM"); tempoSlider.setSliderStyle(juce::Slider::LinearBarVertical); tempoSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 96, 28);
    tempoSlider.setVelocityBasedMode(false); tempoSlider.setSliderSnapsToMousePosition(false); tempoSlider.setMouseDragSensitivity(3800); tempoSlider.setValue(120, juce::dontSendNotification);
    tempoSlider.setTooltip("Live BPM at the playhead during playback. When stopped, edits the selected marker. Drag up/down or double-click to type; during playback, adds a tempo target at the playhead.");
    keyDisplay.addItem("Note", 1); keyDisplay.addItem("Camelot", 2); keyDisplay.addItem("Note + Camelot", 3); keyDisplay.setSelectedId(3, juce::dontSendNotification);
    keyDisplay.setTooltip("Key labels: Gb means Gb major; Gbm means Gb minor. Camelot uses the numbered harmonic wheel. ? means uncertain; Unknown means no key found.");
    keyDisplay.onChange = [this] { if (!refreshing) { engine.project.keyDisplay = keyDisplay.getSelectedId() - 1; dirty = true; refresh(); timeline.repaint(); } };
    trackInfoButton.onClick = [this] { editAnalysis(); };
    const juce::StringArray transitions {"Cut", "4 bars", "8 bars", "16 bars", "32 bars", "Sections 8-64 bars"};
    for (int i = 0; i < transitions.size(); ++i) transitionLength.addItem(transitions[i], i + 1);
    transitionLength.setSelectedId(transitionIndex + 1, juce::dontSendNotification);
    transitionLength.setTitle("New-track transition length");
    transitionLength.setTooltip("Sections chooses an 8-64 bar overlap from retained energy changes, preferably on both tracks. Hints are approximate; missing hints use a grid-aligned fallback. Fixed lengths remain available.");
    transitionLength.onChange = [this] { transitionIndex = transitionLength.getSelectedId() - 1; };
    transitionPreset.addItem("Align IN / OUT", 1); transitionPreset.addItem("Bass swap", 2); transitionPreset.addItem("Crossfade", 3);
    transitionPreset.setSelectedId(3, juce::dontSendNotification); transitionPreset.setTitle("New-track transition preset");
    transitionPreset.setTooltip("Preset for new tracks and Beatmatch. Uses existing markers or suitable beat/bar positions inside both sources.");
    transitionBody.addAndMakeVisible(transitionPreset); transitionBody.addAndMakeVisible(transitionLength);
    beatmatchButton.setTooltip("Apply the selected transition preset and length while rebuilding beat-aligned placement.");
    beatmatchButton.onClick = [this] { syncMix(true); };
    addAndMakeVisible(followButton); followButton.setClickingTogglesState(true);
    followButton.setTitle("Follow playhead");
    followButton.setTooltip("Keep the playhead in view during playback and seeking. Pan the timeline manually to turn Follow off. Saved for this workspace.");
    followButton.onClick = [this] { setFollowPlayhead(followButton.getToggleState()); };
    laneAmount.setText("4 LANES", juce::dontSendNotification);
    laneAmount.setJustificationType(juce::Justification::centred);
    laneAmount.setTooltip("All four mixer lanes stay visible. Drag the timeline's bottom grip to resize them together.");
    slider(masterSlider, -60, 12, 0.1, " dB"); masterSlider.setValue(-3, juce::dontSendNotification); masterSlider.setTooltip("Master output gain; lower this if the peak meter turns red.");
    masterSlider.setName("Master output gain"); masterSlider.setDoubleClickReturnValue(true, 0);
    addAndMakeVisible(peakReset);
    peakReset.setTooltip("Stereo sample peaks after master processing, before monitor mute. Values and lines hold for 1.2 seconds. Click to reset held peaks and clipping. Sparks are optional in View.");
    peakReset.onClick = [this] { peakDisplay.resetPeaks(); engine.masterClipped = false; peakReset.setButtonText("PEAK"); repaint(); };
    search.setTextToShowWhenEmpty("Search library...", colour::muted); search.onTextChange = [this] { filterLibrary(); libraryTable.table.updateContent(); libraryTable.table.repaint(); };
    const std::array<juce::String, 6> names { "Start", "Source in", "Length", "Fade in", "Fade out", "Clip gain" };
    for (int i = 0; i < 6; ++i)
    {
        addAndMakeVisible(clipSliders[(size_t) i]); addAndMakeVisible(clipLabels[(size_t) i]); label(clipLabels[(size_t) i], names[(size_t) i]);
        slider(clipSliders[(size_t) i], i == 5 ? -60 : 0, i == 5 ? 12 : 3600, i == 5 ? 0.1 : 0.001, i == 5 ? " dB" : " s");
        clipSliders[(size_t) i].setName(names[(size_t) i]);
        const char* tips[] {"Exact timeline start; turn Snap off for free numerical placement.", "Trim the source beginning without deleting the original audio. IN/OUT handles align transitions instead.", "Exact playback duration of this segment.", "Optional manual volume fade in. Adding a track does not create one.", "Optional manual volume fade out. Adding a track does not create one.", "Input gain before this clip's effects; useful for gain staging into a VST."};
        clipSliders[(size_t) i].setTooltip(tips[i]);
        if (i == 5) clipSliders[(size_t) i].configure("Clip gain");
        clipSliders[(size_t) i].onDragStart = [this] { remember(); };
        clipSliders[(size_t) i].onValueChange = [this, i] { if (!refreshing) editClip(i, clipSliders[(size_t) i].getValue()); };
        clipSliders[(size_t) i].onDragEnd = [this, i] { if (i == 1 || i == 2) scheduleTempoPreparation(); };
    }
    newButton.onClick = [this] { guardUnsaved([this] { newSession(); }); };
    openButton.onClick = [this] { guardUnsaved([this] { chooseOpen(); }); };
    saveButton.onClick = [this] { chooseSave(); }; importButton.onClick = [this] { chooseImport(); }; exportButton.onClick = [this] { chooseExport(); }; settingsButton.onClick = [this] { settings(); };
    playButton.setColour(juce::TextButton::buttonColourId, colour::orange.darker(0.6f));
    playButton.onClick = [this] { engine.flushMidiControls(); engine.setPlaying(!engine.playing); refresh(); };
    stopButton.onClick = [this] { engine.flushMidiControls(); engine.stop(); refresh(); }; homeButton.onClick = [this] { engine.flushMidiControls(); engine.seek(0); };
    undoButton.onClick = [this] { undo(false); }; redoButton.onClick = [this] { undo(true); }; splitButton.onClick = [this] { splitClip(); }; duplicateButton.onClick = [this] { duplicateClip(); }; removeButton.onClick = [this] { removeClip(); };
    tempoSlider.onValueChange = [this] { if (!refreshing) { pendingTempo = tempoSlider.getValue(); tempoChangedAt = juce::Time::getMillisecondCounterHiRes(); } };
    masterSlider.onValueChange = [this] { if (!refreshing) { const juce::ScopedLock guard(engine.lock); engine.setControlValue({-1, AutomationTarget::master}, (float) masterSlider.getValue()); dirty = true; } };
    monitorMute.setTooltip("Silence speakers and headphones. Playback, meters, master automation and WAV export continue unchanged.");
    monitorMute.onClick = [this]
    {
        layout.monitorMuted = monitorMute.getToggleState(); engine.monitorMuted = layout.monitorMuted;
        workspaceDirty = true; workspaceChangedAt = juce::Time::getMillisecondCounterHiRes();
    };
    masterSlider.onDragStart = [this] { if (!engine.canWriteAutomation(-1)) remember(); };
    snapButton.onClick = [this] { performAction({MidiActionKind::snap}); };
    zoomSlider.setRange(-1, std::log10(400.0));
    zoomSlider.setSliderStyle(juce::Slider::LinearHorizontal); zoomSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    zoomSlider.setValue(std::log10(timeline.pixelsPerSecond), juce::dontSendNotification);
    zoomSlider.setTitle("Timeline zoom"); zoomSlider.setTooltip("Drag to zoom around the centre of the timeline. Wheel over the timeline to zoom at the cursor. View / Fit whole mix fits the arrangement.");
    zoomSlider.onValueChange = [this] { timeline.zoomAt(std::pow(10.0, zoomSlider.getValue()) / timeline.pixelsPerSecond, viewport.getViewPositionX() + viewport.getViewWidth() * 0.5); };
    addAndMakeVisible(autoArrangeButton);
    autoArrangeButton.setColour(juce::TextButton::buttonColourId, colour::orange.darker(.45f));
    autoArrangeButton.getProperties().set("alloyMolten", true); autoArrangeButton.getProperties().set("alloyActive", true);
    autoArrangeButton.setTooltip("Build a new mix by key and similar BPM. Choose a starting track and track count or length, up to 3 hours. Undo restores the previous arrangement.");
    autoArrangeButton.onClick = [this] { showAutoArrangement(); };
    addPluginButton.onClick = [this] { choosePlugin(); };
    editPluginButton.onClick = [this] { showEditor(pluginList.list.getSelectedRow()); };
    pluginControlsButton.onClick = [this] { configurePluginControls(); };
    bypassPluginButton.onClick = [this]
    {
        auto& chain = engine.pluginChain(engine.selectedTrack.load()); const int i = pluginList.list.getSelectedRow();
        if (i < 0 || i >= (int) chain.size()) return;
        const juce::ScopedLock guard(engine.lock); chain[(size_t) i]->bypass = !chain[(size_t) i]->bypass; dirty = true; pluginList.list.repaint();
    };
    removePluginButton.onClick = [this]
    {
        auto& chain = engine.pluginChain(engine.selectedTrack.load()); const int i = pluginList.list.getSelectedRow();
        if (i < 0 || i >= (int) chain.size()) return;
        auto id = chain[(size_t) i]->id;
        editors.erase(std::remove_if(editors.begin(), editors.end(), [&](const auto& e) { return e->slotId == id; }), editors.end());
        const juce::ScopedLock guard(engine.lock);
        engine.removePlugin(engine.selectedTrack, i); dirty = true; refresh();
    };
    engine.onControl = [this](bool changed) { dirty = dirty || changed; if (!busy) refresh(); };
    addAndMakeVisible(controllerConnection); controllerConnection.onClick = [this] { settings(); }; refreshControllerConnection();
    engine.onProjectSwitchBegin = [this] { remember(); };
    engine.onAutomationWriteBegin = [this] { remember(); };
    engine.onMidiChanged = [this]
    {
        midiLabel.setText("MIDI: " + engine.lastMidiText(), juce::dontSendNotification);
        if (settingsWindow) if (auto* content = dynamic_cast<MidiSettings*>(settingsWindow->getContentComponent())) content->updateText();
    };
    if (openAudioDevice) { auto deviceError = engine.startDevice(); if (deviceError.isNotEmpty()) status = "Audio device: " + deviceError; }
    addChildComponent(importProgress);
    importProgress.cancel.onClick = [this]
    {
        if (exportTask) { *exportTask->cancelled = true; importProgress.requestStop(); }
        else if (importCancellation) { *importCancellation = true; importProgress.requestStop(); }
    };
    addAndMakeVisible(reverseButton); addAndMakeVisible(loopButton);
    reverseButton.onClick = [this]
    {
        if (busy || !currentClip()) return;
        if (engine.project.clipTransitionLocked(selectedClip)) { setStatus("Unlock the transition before reversing its track."); refresh(); return; }
        remember(); const juce::ScopedLock guard(engine.lock);
        scheduleTempoPreparation(); currentClip()->reversed = reverseButton.getToggleState(); engine.resetPlugins(); refresh();
    };
    loopButton.onClick = [this] { configureLoop(); };
    libraryTable.table.getHeader().setSortColumnId(1, true);
    if (openAudioDevice)
    {
        const auto preference = readScanPreference(musicPreference); musicScanEnabled = preference.enabled; if (preference.error.isNotEmpty()) status = preference.error; catalogueFile = libraryCatalogueFile(); catalogueRestorePending = true;
        workspacePreference = PcmStore::defaultRoot().getParentDirectory().getChildFile("workspace.json");
        const auto loaded = readWorkspacePreferences(workspacePreference, layout); if (loaded.failed()) status = loaded.getErrorMessage();
        recoveryRoot = workspacePreference.getParentDirectory().getChildFile("recovery");
        juce::String recoveryError; recoveryStore = RecoveryStore::create(recoveryRoot, recoveryError);
        if (!recoveryStore) status = "Recovery unavailable: " + recoveryError;
        nextRecovery = juce::Time::getMillisecondCounterHiRes() + 30000;
    }
    followButton.setToggleState(layout.followPlayhead, juce::dontSendNotification);
    engine.monitorMuted = layout.monitorMuted; monitorMute.setToggleState(layout.monitorMuted, juce::dontSendNotification);
    if (performance) performance->setForgeEffects(forgeMotionAllowed());
    viewport.onViewChanged = [this]
    {
        const int x = viewport.getViewPositionX();
        if (x != lastTimelineViewX && !programmaticTimelineView) setFollowPlayhead(false);
        lastTimelineViewX = x;
        layoutTrackHeaders(); timelineRuler.repaint(); timelineOverview.repaint();
    };
    nextMusicScan = juce::Time::getMillisecondCounterHiRes() + 2000;
    initialiseAttract();
    setSize(1440, 940); showBrowserPage(0); showInspectorPage(0); refresh(); tickLibraryCatalogue(); startTimerHz(25);
}
Workstation::~Workstation()
{
    masterWindow.reset();
    if (performanceWindow) performanceWindow->clearContentComponent();
    if (transitionWindow) transitionWindow->clearContentComponent();
    transitionWindow.reset(); transitionViewport.setViewedComponent(nullptr, false);
    performanceWindow.reset(); performanceViewport.setViewedComponent(nullptr, false);
    gridWindow.reset();
    stopTimer(); engine.onControl = {}; engine.onMidiChanged = {}; engine.onAutomationWriteBegin = {}; engine.onPerformanceAction = {};
    saveWorkspacePreferences();
    if (importCancellation) *importCancellation = true;
    *sourceReadsCancelled = true;
    if (exportTask) *exportTask->cancelled = true;
    stopMusicScan();
    cancelTimelineAudio();
    audioWorkers.removeAllJobs(true, -1);
    exportTask.reset(); engine.exporting = false;
    workers.removeAllJobs(true, -1);
    recoveryWorkers.removeAllJobs(true, -1);
    if (recoveryStore) { const auto result = recoveryStore->discard(); if (result.failed()) juce::Logger::writeToLog(result.getErrorMessage()); }
    if (catalogueFile != juce::File())
    {
        juce::Array<juce::File> sources; for (const auto& media : engine.library) sources.add(media->file);
        const auto error = rememberLibraryCatalogue(catalogueFile, sources, engine.library); if (error.isNotEmpty()) juce::Logger::writeToLog(error);
    }
    settingsWindow.reset();  editors.clear(); viewport.setViewedComponent(nullptr, false); setLookAndFeel(nullptr);
}
void Workstation::timerCallback()
{
    refreshControllerConnection();
    advancePeakMeter();
    if (workspaceDirty && juce::Time::getMillisecondCounterHiRes() - workspaceChangedAt > 700) saveWorkspacePreferences();
    tickLibraryCatalogue(); tickMusicScan(); tickRecovery();
    layoutTrackHeaders();
    engine.flushMidiControls();
    engine.advanceAutomation(engine.position);
    if (engine.latencyBlocked && !busy)
    {
        const auto error = engine.pluginLatencyError();
        if (error.isNotEmpty()) { setStatus(error); pluginList.list.repaint(); } else engine.latencyBlocked = false;
    }
    if (performance) { performance->setForgeEffects(forgeMotionAllowed()); performance->refresh(!busy && !engine.exporting); }
    if (importProgress.isVisible()) { if (exportTask) importProgress.showExportProgress(exportTask->completed.load()); else importProgress.tick(); }
    if (!busy && timelineLoads.empty() && (engine.project.hasPendingPlacement() || afterTimelineAudio)) finishTimelineAudio();
    if (!busy && pendingTempo > 0 && !tempoSlider.isMouseButtonDown() && !AlloyLookAndFeel::editingSliderText(tempoSlider) && juce::Time::getMillisecondCounterHiRes() - tempoChangedAt > 450) syncMix(false);
    if (!busy && tempoPreparationPending && !juce::ModifierKeys::getCurrentModifiersRealtime().isAnyMouseButtonDown() && juce::Time::getMillisecondCounterHiRes() - tempoChangedAt > 450)
    { tempoPreparationPending = false; prepareTempo(engine.project, false, false); }
    if (pendingTempo <= 0 && !tempoSlider.isMouseButtonDown() && !AlloyLookAndFeel::editingSliderText(tempoSlider))
    {
        tempoSlider.setValue(displayedTempo(), juce::dontSendNotification);
    }
    followPlayheadIntoView();
    timeLabel.setText(timeText(engine.position), juce::dontSendNotification); playButton.setButtonText(engine.playing ? "Pause" : "Play");
    playButton.getProperties().set("alloyActive", engine.playing.load()); playButton.repaint();
    timeline.repaint(); timelineRuler.repaint(); timelineOverview.repaint(); tempoStrip.repaint(); for (auto& s : strips) s->repaint(); refreshAttractLights(); repaint();
}
void Workstation::advancePeakMeter()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    peakDisplay.advance(engine.outputPeaks.take(), peakDisplayTime == 0 ? .04 : (now - peakDisplayTime) / 1000,
                        forgeMotionAllowed());
    peakDisplayTime = now;
    peakReset.setButtonText(peakDisplay.clipped ? "CLIP / reset" : "PEAK");
    peakReset.setColour(juce::TextButton::textColourOffId, peakDisplay.clipped ? colour::error : colour::orange);
}
bool Workstation::forgeMotionAllowed() const
{
    return layout.forgeEffects && (layout.forgeMotion == 1 || (layout.forgeMotion == 0 && AlloyLookAndFeel::motionAllowed()));
}
Clip* Workstation::currentClip() { for (auto& c : engine.project.clips) if (c.id == selectedClip) return &c; return nullptr; }
void Workstation::setStatus(const juce::String& s) { status = s; repaint(); }
void Workstation::setBusy(bool value)
{
    if (value) { stopMusicScan(); engine.endTransitionRehearsal(); }
    busy = value;
    engine.controlInputEnabled = !value;
    if (performance) performance->refresh(!busy && !engine.exporting);
    for (int i = 0; i < getNumChildComponents(); ++i) getChildComponent(i)->setEnabled(!busy);
    for (auto* control : {&displayButton, &windowModeButton, &minimiseButton, &closeAppButton}) control->setEnabled(true);
    if (masterWindow && masterWindow->getContentComponent()) masterWindow->getContentComponent()->setEnabled(!busy);
    if (settingsWindow && settingsWindow->getContentComponent()) settingsWindow->getContentComponent()->setEnabled(!busy);
    for (auto& editor : editors) editor->setEnabled(!busy);
}
void Workstation::fail(const juce::String& error) { setStatus(error); juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Mixworks", error); }
void Workstation::remember()
{
    engine.endTransitionRehearsal();
    const juce::ScopedLock guard(engine.lock); undoHistory.push_back(engine.project); if (undoHistory.size() > 100) undoHistory.erase(undoHistory.begin()); redoHistory.clear(); dirty = true;
}
void Workstation::undo(bool redo)
{
    if (busy) return;
    engine.endTransitionRehearsal();
    afterTimelineAudio = {};
    pendingTempo = 0; tempoPreparationPending = false; tempoEditBaseline.reset(); tempoStrip.selectedTempoPoint = -1;
    engine.setAutomationRecordMode(AutomationRecordMode::read);
    auto& source = redo ? redoHistory : undoHistory; auto& dest = redo ? undoHistory : redoHistory; if (source.empty()) return;
    timeline.selectAutomationPoint({}, -1);
    const juce::ScopedLock guard(engine.lock);
    // Plugin insert edits are outside arrangement undo; keep their current control ownership too.
    auto currentTracks = engine.project.tracks;
    std::map<juce::String, std::array<PluginControl, 8>> currentClipControls;
    for (const auto& clip : engine.project.clips) currentClipControls[clip.id] = clip.controls.pluginControls;
    dest.push_back(engine.project); engine.project = std::move(source.back()); source.pop_back();
    for (int i = 0; i < trackCount; ++i)
    {
        auto& settings = engine.project.tracks[(size_t) i]; settings.pluginControls = currentTracks[(size_t) i].pluginControls;
        settings.pluginAutomation.erase(std::remove_if(settings.pluginAutomation.begin(), settings.pluginAutomation.end(), [&](const auto& lane)
        { return std::none_of(engine.plugins[(size_t) i].begin(), engine.plugins[(size_t) i].end(), [&](const auto& slot) { return slot->id == lane.slotId; }); }), settings.pluginAutomation.end());
    }
    for (auto& clip : engine.project.clips)
    {
        if (auto current = currentClipControls.find(clip.id); current != currentClipControls.end()) clip.controls.pluginControls = current->second;
        clip.controls.pluginAutomation.erase(std::remove_if(clip.controls.pluginAutomation.begin(), clip.controls.pluginAutomation.end(), [&](const auto& lane)
        { return std::none_of(clip.effects->plugins.begin(), clip.effects->plugins.end(), [&](const auto& slot) { return slot->id == lane.slotId; }); }), clip.controls.pluginAutomation.end());
    }
    engine.restoreManualPluginValues(); engine.tempo = engine.project.bpm; dirty = true; refresh();
    if (engine.project.hasPendingPlacement()) finishTimelineAudio();
}
void Workstation::filterLibrary()
{
    const auto selectedId = selectedLibraryId;
    juce::StringArray selectedIds; const auto oldRows = libraryTable.table.getSelectedRows();
    for (int i = 0; i < oldRows.size(); ++i) if (oldRows[i] >= 0 && oldRows[i] < (int) visibleMedia.size())
    {
        const int source = visibleMedia[(size_t) oldRows[i]];
        if (source >= 0 && source < (int) engine.library.size()) selectedIds.add(engine.library[(size_t) source]->id);
    }
    if (selectedIds.size() <= 1 || !selectedIds.contains(selectedId)) { selectedIds.clear(); if (selectedId.isNotEmpty()) selectedIds.add(selectedId); }
    visibleMedia.clear();
    for (int i = 0; i < (int) engine.library.size(); ++i)
    {
        const auto& m = *engine.library[(size_t) i];
        if (m.name.containsIgnoreCase(search.getText()) || m.assets.artist.containsIgnoreCase(search.getText()) || m.assets.album.containsIgnoreCase(search.getText()) || m.file.getFileName().containsIgnoreCase(search.getText()) || keyNoteName(m.key()).containsIgnoreCase(search.getText()) || keyCompactName(m.key()).containsIgnoreCase(search.getText()) || keyCamelot(m.key()).containsIgnoreCase(search.getText()) || (m.bpm() > 0 && juce::String(m.bpm(), 2).containsIgnoreCase(search.getText()))) visibleMedia.push_back(i);
    }
    if (librarySortColumn == 1 || librarySortColumn == 3 || librarySortColumn == 4 || librarySortColumn == 5)
        std::stable_sort(visibleMedia.begin(), visibleMedia.end(), [this](int a, int b)
        {
            const auto& first = *engine.library[(size_t) a]; const auto& second = *engine.library[(size_t) b];
            if (librarySortColumn == 5)
            {
                const auto& x = first.assets.artworkGroup; const auto& y = second.assets.artworkGroup;
                if (x.isEmpty() != y.isEmpty()) return !x.isEmpty();
                const int group = x.compare(y); if (group != 0) return librarySortForward ? group < 0 : group > 0;
                return first.libraryTitle().compareNatural(second.libraryTitle()) < 0;
            }
            if (librarySortColumn == 1) { const int order = first.libraryTitle().compareNatural(second.libraryTitle()); return librarySortForward ? order < 0 : order > 0; }
            double x = librarySortColumn == 4 ? first.bpm() : first.key() < 0 || first.key() > 23 ? -1 : engine.project.keyDisplay == 0 ? first.key() : keyWheelNumber(first.key()) * 2 + (first.key() < 12 ? 1 : 0);
            double y = librarySortColumn == 4 ? second.bpm() : second.key() < 0 || second.key() > 23 ? -1 : engine.project.keyDisplay == 0 ? second.key() : keyWheelNumber(second.key()) * 2 + (second.key() < 12 ? 1 : 0);
            bool unknownX = librarySortColumn == 4 ? x <= 0 : x < 0, unknownY = librarySortColumn == 4 ? y <= 0 : y < 0;
            if (unknownX != unknownY) return !unknownX;
            return librarySortForward ? x < y : x > y;
        });
    juce::SparseSet<int> rows;
    for (int i = 0; i < (int) visibleMedia.size(); ++i) if (selectedIds.contains(engine.library[(size_t) visibleMedia[(size_t) i]]->id)) rows.addRange({i, i + 1});
    libraryTable.table.setSelectedRows(rows, juce::dontSendNotification);
}
void Workstation::refresh()
{
    engine.prepareClipProcessing();
    if (std::none_of(engine.project.labels.begin(), engine.project.labels.end(), [&](const auto& item) { return item.id == timeline.selectedLabel; })) timeline.selectedLabel.clear();
    refreshing = true; refreshTransitionUI(); filterLibrary(); playlistTable.rebuildPlaylist(); libraryTable.table.updateContent(); pluginList.list.updateContent();
    libraryTable.table.repaint(); playlistTable.table.repaint(); pluginList.list.repaint();
    tempoSlider.setValue(pendingTempo > 0 ? pendingTempo : displayedTempo(), juce::dontSendNotification); masterSlider.setValue(engine.project.masterDb, juce::dontSendNotification); snapButton.setToggleState(engine.project.snap, juce::dontSendNotification);
    keyDisplay.setSelectedId(engine.project.keyDisplay + 1, juce::dontSendNotification);
    for (auto& strip : strips) strip->refresh();
    pluginLabel.setText("VST3 INSERTS / " + (engine.controlClip(engine.selectedTrack) ? engine.controlClip(engine.selectedTrack)->media->name : "Mixer " + engine.project.tracks[(size_t) engine.selectedTrack.load()].name).toUpperCase(), juce::dontSendNotification);
    const auto* c = currentClip(); for (auto& s : clipSliders) s.setEnabled(c != nullptr && (!c->media->sourceDetailsPending || &s == &clipSliders[0] || &s == &clipSliders[5]));
    midiLabel.setText("MIDI / " + engine.project.tracks[(size_t) engine.selectedTrack.load()].name + "\n" + engine.lastMidiText(), juce::dontSendNotification);
    clipDetail.setText(c ? c->media->libraryTitle() + "\n" + juce::String(c->media->bpm(), 2) + " BPM / " + keyText(*c->media) : "Select a clip in the playlist or timeline to view its properties.", juce::dontSendNotification);
    if (c)
    {
        const double values[] { c->start, c->offset, c->length, c->fadeIn, c->fadeOut, c->gainDb };
        for (int i = 0; i < 6; ++i)
        {
            if (i < 5 && (!c->media->sourceDetailsPending || i == 0)) clipSliders[(size_t) i].setRange(0, i == 0 ? std::max(3600.0, c->end() + 60) : i == 1 ? c->media->duration() : c->maxLength(), 0.001);
            if (!AlloyLookAndFeel::editingSliderText(clipSliders[(size_t) i])) clipSliders[(size_t) i].setValue(values[i], juce::dontSendNotification);
        }
    }
    const bool clipLocked = c && engine.project.clipTransitionLocked(c->id);
    for (auto& slider : clipSliders) if (clipLocked) slider.setEnabled(false);
    tempoSlider.setEnabled(!engine.project.hasLockedTransitions());
    beatmatchButton.setEnabled(!engine.project.clips.empty() && !engine.project.hasLockedTransitions());
    splitButton.setEnabled(c != nullptr && !clipLocked && !c->media->sourceDetailsPending); duplicateButton.setEnabled(c != nullptr); removeButton.setEnabled(c != nullptr && !clipLocked);
    reverseButton.setEnabled(c != nullptr && !clipLocked && !c->media->sourceDetailsPending); loopButton.setEnabled(c != nullptr && !clipLocked && !c->media->sourceDetailsPending); reverseButton.setToggleState(c && c->reversed, juce::dontSendNotification);
    undoButton.setEnabled(!undoHistory.empty()); redoButton.setEnabled(!redoHistory.empty());
    playButton.setButtonText(engine.playing ? "Pause" : "Play"); playButton.setEnabled(!busy && (engine.playing || engine.playbackReady()));
    playButton.getProperties().set("alloyActive", engine.playing.load()); refreshing = false;
    if (performance) performance->refresh(!busy && !engine.exporting);
    timeline.updateSize();
    followPlayheadIntoView();
    repaint();
}
void Workstation::selectTrack(int track)
{
    engine.flushMidiControls();
    if (track != engine.selectedTrack)
    {
        engine.silenceNotes(); engine.selectedTrack = track;
    }
    refresh();
}
void Workstation::chooseImport()
{
    chooser = std::make_unique<juce::FileChooser>("Import audio", juce::File::getSpecialLocation(juce::File::userMusicDirectory), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3", false);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems, [safe = juce::Component::SafePointer<Workstation>(this)](const auto& f) { if (safe) safe->importFiles(f.getResults()); });
}
void Workstation::importFiles(const juce::Array<juce::File>& files)
{
    if (busy || files.isEmpty()) return;
    if (std::any_of(files.begin(), files.end(), [](const auto& file) { return file.hasFileExtension("m3u;m3u8"); }))
    {
        setBusy(true); setStatus("Reading playlist paths before importing audio...");
        auto cancelled = std::make_shared<std::atomic<bool>>(false); importCancellation = cancelled;
        importProgress.begin(files.size());
        auto safe = juce::Component::SafePointer<Workstation>(this);
        workers.addJob([safe, files, cancelled]
        {
            juce::Array<juce::File> sources; juce::String error;
            for (const auto& file : files)
            {
                if (*cancelled) break;
                juce::MessageManager::callAsync([safe, cancelled, name = file.getFileName()] { if (safe && safe->importCancellation == cancelled) safe->importProgress.showFile(name, MediaReadStage::playlist); });
                if (file.hasFileExtension("m3u;m3u8"))
                {
                    const auto parsed = readPlaylist(file, [cancelled] { return cancelled->load(); });
                    if (parsed.error.isNotEmpty()) { error = parsed.error; break; }
                    sources.addArray(parsed.files);
                }
                else sources.add(file);
                juce::MessageManager::callAsync([safe, cancelled] { if (safe && safe->importCancellation == cancelled) safe->importProgress.finishFile(); });
            }
            juce::MessageManager::callAsync([safe, cancelled, sources, error]
            {
                if (!safe || safe->importCancellation != cancelled) return;
                safe->importProgress.setVisible(false); safe->importCancellation.reset(); safe->setBusy(false);
                if (*cancelled) { safe->setStatus("Playlist import cancelled. The current library and mix were kept."); return; }
                if (error.isNotEmpty()) { safe->fail(error + "\nNo files from this selection were imported."); return; }
                safe->importFiles(sources);
            });
        });
        return;
    }
    setBusy(true); setStatus("Decoding and analysing BPM, beat phase and musical key...");
    // Decode off the audio and message threads; commit each completed owned sample buffer on the message thread.
    auto safe = juce::Component::SafePointer<Workstation>(this);
    juce::Array<juce::File> pending;
    for (const auto& file : files)
    {
        bool duplicate = false; for (const auto& existing : engine.library) if (existing->file == file && existing->playbackError.isEmpty()) duplicate = true;
        if (!duplicate && !pending.contains(file)) pending.add(file);
    }
    if (pending.isEmpty()) { setBusy(false); setStatus("These tracks are already in your library."); return; }
    auto cancelled = std::make_shared<std::atomic<bool>>(false); importCancellation = cancelled;
    importProgress.begin(pending.size());
    workers.addJob([safe, pending, cancelled]() mutable
    {
        juce::AudioFormatManager formats; formats.registerBasicFormats();
        juce::StringArray failures; int imported = 0, processed = 0, reused = 0, unsavedRecords = 0, decodedReused = 0;
        for (const auto& f : pending)
        {
            if (*cancelled) break;
            auto r = readMedia(formats, f, [safe, cancelled, name = f.getFileName()](MediaReadStage phase)
            {
                juce::MessageManager::callAsync([safe, cancelled, name, phase]
                {
                    if (safe && safe->importCancellation == cancelled) safe->importProgress.showFile(name, phase);
                });
            }, AnalysisCacheMode::readWrite, MediaReadPurpose::catalogue, nullptr, cancelled);
            if (r.media) { ++imported; if (r.media->analysisFromCache) ++reused; if (r.media->decodedFromCache) ++decodedReused; if (r.media->analysisCacheWarning.isNotEmpty() || r.media->playbackCacheWarning.isNotEmpty()) ++unsavedRecords; }
            else failures.add(r.error);
            ++processed;
            juce::MessageManager::callAsync([safe, cancelled, media = std::move(r.media)]() mutable
            {
                if (!safe || safe->importCancellation != cancelled) return;
                if (media)
                {
                    const auto existing = std::find_if(safe->engine.library.begin(), safe->engine.library.end(), [&](const auto& m) { return m->file == media->file; });
                    if (existing == safe->engine.library.end()) safe->engine.library.push_back(std::move(media));
                    else if (!(*existing)->playbackError.isEmpty()) *existing = std::move(media); // Explicit reimport; existing clips retain their original source identity.
                    safe->dirty = true; safe->markCatalogueChanged();
                }
                safe->filterLibrary(); safe->libraryTable.table.updateContent(); safe->libraryTable.table.repaint();
                safe->importProgress.finishFile();
            });
        }
        auto errors = failures.isEmpty() ? juce::String() : juce::String(failures.size()) + " file(s) could not be imported:\n" + juce::StringArray(failures.begin(), std::min(6, failures.size())).joinIntoString("\n");
        if (failures.size() > 6) errors += "\nAnd " + juce::String(failures.size() - 6) + " more. Import smaller selections to inspect individual failures.";
        const bool stopped = processed < pending.size();
        juce::MessageManager::callAsync([safe, cancelled, imported, reused, decodedReused, unsavedRecords, stopped, errors]() mutable
        {
            if (!safe || safe->importCancellation != cancelled) return;
            safe->importProgress.setVisible(false); safe->importCancellation.reset();
            safe->setBusy(false); safe->refresh(); safe->setStatus(juce::String(stopped ? "Import stopped. " : "Import complete. ") + "Added " + juce::String(imported) + " tracks; reused " + juce::String(reused) + " analyses / " + juce::String(decodedReused) + " decoded audio caches. " + (unsavedRecords > 0 ? juce::String(unsavedRecords) + " cache warnings; see BPM / Key." : "Double-click a track to add it to the timeline."));
            if (errors.isNotEmpty()) safe->fail(errors);
        });
    });
}
void Workstation::editClip(int field, double value)
{
    const juce::ScopedLock guard(engine.lock); auto* c = currentClip(); if (!c || busy) return;
    if (engine.project.clipTransitionLocked(c->id)) { setStatus("Unlock the transition before editing its track."); refresh(); return; }
    if (c->media->sourceDetailsPending && field != 0 && field != 5) return;
    if (field == 1 || field == 2) scheduleTempoPreparation();
    if (field == 0)
    {
        const auto result = engine.project.moveClip(c->id, value, engine.project.snap);
        if (result.failed()) { setStatus(result.getErrorMessage()); return; }
        dirty = true; refresh(); return;
    }
    if (field == 1) { c->offset = juce::jlimit(0.0, std::max(0.0, c->media->duration() - (c->loopSpan > 0 ? c->loopSpan : 0.001 * c->rate)), value); if (c->loopSpan == 0) c->length = std::min(c->length, (c->media->duration() - c->offset) / c->rate); }
    if (field == 2) *c = c->slice(0, juce::jlimit(std::min(0.001, c->maxLength()), c->maxLength(), value));
    if (field == 3) c->fadeIn = std::min(value, c->length);
    if (field == 4) c->fadeOut = std::min(value, c->length);
    if (field == 5) c->gainDb = (float) value;
    dirty = true; playlistTable.table.repaint(); timeline.repaint();
}
void Workstation::splitClip()
{
    auto* c = currentClip(); const double at = engine.position; if (!c || c->media->sourceDetailsPending || at <= c->start || at >= c->end() || busy) return;
    if (engine.project.clipTransitionLocked(c->id)) { setStatus("Unlock the transition before splitting its track."); return; }
    const juce::ScopedLock guard(engine.lock); const Clip original = *c;
    const double leftLength = at - c->start; Clip right = original.slice(leftLength, original.length - leftLength); right.id = juce::Uuid().toString();
    if (auto error = engine.cloneClipEffects(right); error.isNotEmpty()) { fail(error); return; }
    auto candidate = engine.project; candidate.ensureTransitionLinks();
    const auto index = (size_t) (c - engine.project.clips.data());
    candidate.clips[index] = original.slice(0, leftLength);
    candidate.clips.insert(candidate.clips.begin() + (ptrdiff_t) index + 1, right);
    for (auto& link : candidate.transitionLinks)
    {
        const bool outOwned = link.outgoing == original.id, inOwned = link.incoming == original.id;
        if (link.outgoing == original.id && original.transitionTime(false) >= leftLength) link.outgoing = right.id;
        if (link.incoming == original.id && original.transitionTime(true) >= leftLength) link.incoming = right.id;
        for (int side = 0; side < 2 && link.bassSwap; ++side) if (side == 0 ? outOwned : inOwned)
        {
            const bool shifted = (side == 0 ? link.outgoing : link.incoming) == right.id;
            const double sourceBegin = shifted ? original.sourceElapsed(leftLength) : 0;
            const double sourceEnd = shifted ? original.sourceElapsed(original.length) : original.sourceElapsed(leftLength);
            auto& p = *link.bassSwap;
            if (p[(size_t) side * 2] < sourceBegin || p[(size_t) side * 2 + 1] > sourceEnd) link.bassSwap.reset();
            else { p[(size_t) side * 2] -= sourceBegin; p[(size_t) side * 2 + 1] -= sourceBegin; }
        }
    }
    if (original.transitionTime(true) < leftLength && original.transitionTime(false) >= leftLength)
    {
        auto* left = candidate.clipFor(original.id); auto* next = candidate.clipFor(right.id);
        left->outroAnchor = left->sourceElapsed(left->length); next->introAnchor = 0;
        const auto result = candidate.linkClips(left->id, next->id);
        if (result.failed()) { setStatus(result.getErrorMessage()); return; }
    }
    const auto valid = candidate.validateTransitionLinks(); if (valid.failed()) { setStatus(valid.getErrorMessage()); return; }
    remember(); engine.project = std::move(candidate); selectedClip = right.id;
    refresh();
}
void Workstation::duplicateClip()
{
    const auto* selected = currentClip(); if (!selected || busy) return;
    if (engine.project.hasLockedTransitions()) { setStatus("Unlock transitions before duplicating tracks with new tempo markers."); return; }
    if (pendingTempo > 0 || tempoPreparationPending || engine.project.hasPendingPlacement() || selected->media->sourceDetailsPending)
    { setStatus("Wait for preparation before duplicating this track."); return; }
    const juce::ScopedLock guard(engine.lock); const Clip original = *selected;
    Clip copy = original; copy.id = juce::Uuid().toString();
    if (const auto error = engine.cloneClipEffects(copy); error.isNotEmpty()) { setStatus("Duplicate kept the current mix: " + error); return; }
    auto candidate = engine.project;
    copy.moveTo(candidate.snappedClipStart(copy, original.end(), candidate.snap));
    if (candidate.followTrackTempo && !candidate.tempo.points.empty())
    {
        const double at = copy.start + copy.transitionTime(true);
        const auto next = std::upper_bound(candidate.tempo.points.begin(), candidate.tempo.points.end(), at, [](double t, const auto& point) { return t < point.time; });
        candidate.tempo.setTrackPoint(copy.id, at, candidate.tempo.bpmAt(at), next == candidate.tempo.points.end() || next->ramp);
    }
    candidate.clips.push_back(copy);
    const auto valid = candidate.validateTransitionLinks(); if (valid.failed()) { setStatus(valid.getErrorMessage()); return; }
    remember(); engine.project = std::move(candidate); selectedClip = copy.id; refresh();
}

void Workstation::removeClip()
{
    if (!currentClip() || busy) return;
    if (engine.project.clipTransitionLocked(selectedClip)) { setStatus("Unlock the transition before removing its track."); return; }
    remember(); const juce::ScopedLock guard(engine.lock);
    engine.project.detachTransitionLinks(selectedClip);
    auto& clips = engine.project.clips; clips.erase(std::remove_if(clips.begin(), clips.end(), [this](const Clip& c) { return c.id == selectedClip; }), clips.end()); selectedClip.clear(); engine.project.tempo.detachMissingTracks(engine.project.clips); refresh();
}
void Workstation::guardUnsaved(std::function<void()> action)
{
    if (unsavedPromptOpen || choosingSave) return;
    engine.flushMidiControls();
    if (!dirty && pendingTempo <= 0 && !busy) { action(); return; }
    unsavedPromptOpen = true;
    juce::AlertWindow::showYesNoCancelBox(juce::MessageBoxIconType::QuestionIcon, "Unsaved mix", "Save changes to this mix before continuing?",
        "Save", "Discard", "Cancel", this, juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<Workstation>(this), action](int result)
        {
            if (!safe) return;
            safe->unsavedPromptOpen = false;
            if (result == 2) action();
            else if (result == 1) { safe->engine.setPlaying(false); safe->chooseSave(action, true); }
        }));
}
void Workstation::requestClose(std::function<void()> close) { guardUnsaved([this, close = std::move(close)] { clearRecovery(); close(); }); }
void Workstation::newSession()
{
    timeline.selectAutomationPoint({}, -1);
    clearRecovery();
    cancelTimelineAudio();
    stopMusicScan(); musicAttempts.clear();
    engine.setAutomationRecordMode(AutomationRecordMode::read); engine.automationArmed.fill(false); engine.masterAutomationArmed = false;
    engine.stop(); editors.clear(); const juce::ScopedLock guard(engine.lock);
    
    engine.masterClipped = false; engine.masterPeak = engine.masterInputPeak = 0;
    engine.project = Project(); engine.tempo = 120; for (auto& p : engine.plugins) p.clear(); undoHistory.clear(); redoHistory.clear(); selectedClip.clear(); sessionFile = {}; dirty = false; refresh();
    for (auto& media : engine.library) media = media->catalogueCopy();
    tempoStrip.selectedTempoPoint = -1; pendingTempo = 0; tempoPreparationPending = false; tempoEditBaseline.reset(); afterTempoPreparation = {};
}
juce::String Workstation::keyText(const Media& media) const
{
    const int key = media.key(); if (key < 0 || key > 23) return "Unknown";
    auto text = engine.project.keyDisplay == 0 ? keyCompactName(key) : engine.project.keyDisplay == 1 ? keyCamelot(key) : keyCamelot(key) + " / " + keyCompactName(key);
    if (!media.keyConfirmed()) text += " ?";
    return text;
}
void Workstation::syncMix(bool align)
{
    if (busy) return;
    if (engine.project.hasLockedTransitions()) { pendingTempo = 0; setStatus("Unlock transitions before changing tempo or rebuilding the mix."); refresh(); return; }
    auto candidate = engine.project;
    if (!align && pendingTempo > 0)
    {
        const double target = pendingTempo; pendingTempo = 0;
        if (candidate.tempo.points.empty()) candidate.tempo.setPoint(0, candidate.bpm);
        const int selected = engine.playing ? -1 : tempoStrip.selectedTempoPoint;
        if (selected >= 0 && selected < (int) candidate.tempo.points.size()) candidate.tempo.movePoint(selected, candidate.tempo.points[(size_t) selected].time, target);
        else tempoStrip.selectedTempoPoint = candidate.tempo.setPoint(engine.position, target);
    }
    pendingTempo = 0; tempoPreparationPending = false;
    prepareTempo(std::move(candidate), align);
}
void Workstation::scheduleTempoPreparation()
{
    if (engine.project.followTrackTempo && !engine.project.tempo.points.empty()) { if (!tempoEditBaseline) tempoEditBaseline = engine.project; tempoPreparationPending = true; tempoChangedAt = juce::Time::getMillisecondCounterHiRes(); }
}
bool Workstation::ensureTempoReady(std::function<void()> action)
{
    if (engine.project.hasPendingPlacement())
    {
        afterTimelineAudio = std::move(action); finishTimelineAudio(); return false;
    }
    if (!engine.playbackReady())
    {
        if (!timelineLoads.empty()) { afterTimelineAudio = std::move(action); setStatus("Waiting for background audio preparation..."); }
        else setStatus("A source is unavailable. Reimport it or remove its clip before continuing.");
        return false;
    }
    if (pendingTempo <= 0 && !tempoPreparationPending) return true;
    afterTempoPreparation = std::move(action);
    if (pendingTempo > 0) syncMix(false);
    else { tempoPreparationPending = false; prepareTempo(engine.project, false, false); }
    return false;
}
void Workstation::prepareTempo(Project source, bool align, bool addUndo, size_t alignFrom)
{
    if (busy) return;
    if (engine.project.hasLockedTransitions()) { pendingTempo = 0; tempoPreparationPending = false; tempoEditBaseline.reset(); setStatus("Unlock transitions before changing tempo or rebuilding the mix."); refresh(); return; }
    if (!engine.sourceAudioReady())
    {
        ensureTempoReady([safe = juce::Component::SafePointer<Workstation>(this), align, addUndo, alignFrom]
        { if (safe) safe->prepareTempo(safe->engine.project, align, addUndo, alignFrom); }); return;
    }
    engine.flushMidiControls(); engine.setAutomationRecordMode(AutomationRecordMode::read);
    const bool resume = engine.playing; engine.setPlaying(false);
    TempoMap previous = engine.project.followTrackTempo ? engine.project.tempo : TempoMap {engine.project.bpm, {}};
    const double positionBeat = previous.beatsAt(engine.position);
    auto previousMap = std::make_shared<const TempoMap>(previous);
    auto rollback = std::move(tempoEditBaseline); tempoEditBaseline.reset(); tempoPreparationPending = false;
    for (auto& clip : source.clips) if (clip.beatmatched && !clip.tempoMap) clip.tempoMap = previousMap;
    std::set<const PreparedAudio*> seen; int64_t used = 0;
    auto count = [&](const Project& p) { for (const auto& c : p.clips) if (c.prepared && seen.insert(c.prepared.get()).second) used += c.prepared->residentBytes(); };
    count(engine.project); if (rollback) count(*rollback); for (const auto& p : undoHistory) count(p); for (const auto& p : redoHistory) count(p);
    const double beats = selectedTransitionPreset() == TransitionPreset::alignOnly ? 0 : transitionOverlapBeats();
    const auto preset = selectedTransitionPreset();
    const bool sectionTransitions = transitionIndex == 5;
    setBusy(true); setStatus("Preparing track tempo, transitions and beat grid...");
    workers.addJob([safe = juce::Component::SafePointer<Workstation>(this), source = std::move(source), rollback = std::move(rollback), align, addUndo, alignFrom, beats, used, resume, positionBeat, preset, sectionTransitions]() mutable
    {
        auto result = prepareTempoMap(std::move(source), align, beats, used, alignFrom, {}, preset, sectionTransitions);
        juce::MessageManager::callAsync([safe, result = std::move(result), rollback = std::move(rollback), align, addUndo, resume, positionBeat]() mutable
        {
            if (!safe) return; safe->setBusy(false);
            if (result.error.isNotEmpty())
            {
                const juce::ScopedLock guard(safe->engine.lock);
                if (rollback) safe->engine.project = std::move(*rollback);
                for (auto& clip : safe->engine.project.clips) if (clip.pendingPlacement != PendingPlacement::none) clip.pendingPlacement = PendingPlacement::failed;
                safe->afterTempoPreparation = {}; safe->engine.setPlaying(resume); safe->refresh(); safe->fail(result.error); return;
            }
            if (addUndo) safe->remember();
            { const juce::ScopedLock guard(safe->engine.lock); safe->engine.project = std::move(result.project); safe->engine.tempo = safe->engine.project.tempo.bpmAt(0);
                safe->engine.position = align ? 0.0 : safe->engine.project.tempo.timeAtBeat(positionBeat); safe->engine.resetPlugins(); }
            safe->engine.setPlaying(resume); safe->dirty = true; safe->refresh();
            safe->setStatus("Track tempo and beat grid ready. Click a tempo number to edit BPM; drag a marker horizontally to move it. " + juce::String(result.skipped) + " uncertain clips left unsynced.");
            auto action = std::move(safe->afterTempoPreparation); safe->afterTempoPreparation = {}; if (action) action();
        });
    });
}
void Workstation::setDownbeatAnchor(const juce::String& clipId, double localTime)
{
    if (engine.project.clipTransitionLocked(clipId)) { setStatus("Unlock the transition before changing its source grid."); return; }
    if (busy) return;
    std::shared_ptr<Media> media; double source = 0;
    for (const auto& clip : engine.project.clips) if (clip.id == clipId)
    { media = clip.media; source = clip.sourceTimeAt(juce::jlimit(0.0, clip.length, localTime)); break; }
    if (!media) return;
    auto correction = SourceCorrection::from(*media); correction.downbeat = source;
    const auto applied = applySourceCorrection(media, correction);
    if (applied.failed()) { fail(applied.getErrorMessage()); return; }
    setStatus("Downbeat anchor set at source " + timeText(source) + ". Beatmatch rebuilds the transitions using this anchor.");
}
juce::Result Workstation::applySourceCorrection(const std::shared_ptr<Media>& media, const SourceCorrection& correction)
{
    if (busy || engine.exporting || !timelineLoads.empty() || pendingTempo > 0 || tempoPreparationPending)
        return juce::Result::fail("Wait for source or tempo preparation before applying a correction.");
    const auto result = engine.correctSource(media, correction); if (result.failed()) return result;
    saveAnalysisCorrection(media);
    setStatus("Source correction applied. Existing placement and audio are retained; Beatmatch explicitly applies the new grid.");
    return juce::Result::ok();
}
void Workstation::saveAnalysisCorrection(const std::shared_ptr<Media>& media)
{
    markCatalogueChanged();
    const auto record = media->analysisRecord(); const auto file = media->file;
    if (file.existsAsFile()) workers.addJob([safe = juce::Component::SafePointer<Workstation>(this), media, file, record]
    {
        const auto warning = writeAnalysisRecord(file, record);
        juce::MessageManager::callAsync([safe, media, warning] { if (safe) { media->analysisCacheWarning = warning; if (warning.isNotEmpty()) safe->setStatus(warning); } });
    });
    dirty = true; refresh();
}
void Workstation::editAnalysis()
{
    if (gridWindow && gridWindow->isVisible()) { gridWindow->toFront(true); return; }
    if (busy || !timelineLoads.empty() || pendingTempo > 0 || tempoPreparationPending)
    { setStatus("Wait for audio and tempo preparation before opening source grid correction."); return; }
    std::shared_ptr<Media> media;
    for (const auto& item : engine.library) if (item->id == selectedLibraryId) { media = item; break; }
    if (!media) if (auto* c = currentClip()) media = c->media;
    if (!media) { setStatus("Select a library track or a timeline clip first."); return; }
    const auto& a = media->analysis;
    juce::String details = media->name + "\nDetected " + juce::String(a.bpm, 2) + " BPM (evidence score " + juce::String(a.tempoConfidence * 100, 0) + "/100). Beat phase " + juce::String(a.beatOrigin, 3) + " s.";
    details += media->manualBeatOrigin >= 0 ? "\nManual downbeat anchor: " + juce::String(media->manualBeatOrigin, 4) + " s (4/4; may be anywhere in the source)."
        : media->downbeatOrigin() >= 0 ? "\nInferred downbeat anchor: " + juce::String(a.downbeatOrigin, 4) + " s (4/4; review its bar phase)."
        : "\nDownbeat unknown. Right-click a waveform at beat one to set it; automatic sync aligns pulses only.";
    details += "\nKey: " + keyNoteName(a.key) + " / " + keyCamelot(a.key) + " (evidence score " + juce::String(a.keyConfidence * 100, 0) + "/100; not a probability). Alternate: " + keyNoteName(a.alternativeKey) + " / " + keyCamelot(a.alternativeKey);
    details += "\nTuning " + juce::String(a.tuningCents, 1) + " cents; examined " + juce::String(a.examinedSeconds, 1) + " of " + juce::String(media->duration(), 1) + " seconds.";
    if (a.bpm > 0) details += " Grid deviation " + juce::String(a.gridErrorMs, 1) + " ms RMS.";
    details += a.variableTempo ? "\nTempo varies or windows disagree: review the grid before syncing." : "\nConstant-tempo grid estimate. Half/double BPM ambiguities can be corrected below.";
    details += "\nLeave overrides blank to use detection. Confidence is a heuristic, not a calibrated probability.";
    details += "\nAnalysis record: " + analysisRecordFile(media->file).getFullPathName();
    details += media->analysisCacheWarning.isNotEmpty() ? "\n" + media->analysisCacheWarning : media->analysisFromCache ? "\nReused saved analysis for unchanged audio." : "\nAnalysis is saved in Mixworks' local application data folder.";
    if (media->playbackError.isNotEmpty()) details += "\nAudio loading: " + media->playbackError;
    details += "\nPlayback cache: " + PcmStore::defaultRoot().getFullPathName();
    if (media->playbackCacheWarning.isNotEmpty()) details += "\n" + media->playbackCacheWarning;
    if (!std::isfinite(media->duration()) || media->duration() <= 0) { setStatus("Wait until this source's duration is known."); return; }
    engine.setPlaying(false);
    auto* editor = new GridCorrectionEditor(engine, media, details,
        [safe = juce::Component::SafePointer<Workstation>(this), media](const SourceCorrection& correction)
        { return safe ? safe->applySourceCorrection(media, correction) : juce::Result::fail("The mix was closed."); });
    gridWindow = std::make_unique<GridCorrectionWindow>(editor); gridWindow->setLookAndFeel(&look);
    gridWindow->setVisible(true); gridWindow->enterModalState(true, nullptr, false);
    setStatus("Source grid audition: mix paused; corrections apply without rearranging existing audio.");
    if (!media->hasPlayback())
    {
        const auto record = media->analysisRecord(); const auto file = media->file;
        audioWorkers.addJob([safe = juce::Component::SafePointer<GridCorrectionEditor>(editor), record, file, cancelled = sourceReadsCancelled]
        {
            juce::AudioFormatManager formats; formats.registerBasicFormats();
            auto result = readMedia(formats, file, {}, AnalysisCacheMode::readOnly, MediaReadPurpose::playback, &record, cancelled);
            if (result.media && !(result.media->analysisSource == record.source))
            { result.media.reset(); result.error = "The source changed. Reimport it before editing its grid."; }
            juce::MessageManager::callAsync([safe, media = std::move(result.media), error = result.error]
            { if (safe) safe->setPlaybackMedia(media, error); });
        });
    }
}
bool Workstation::saveSessionFile(const juce::File& file)
{
    const auto error = engine.saveProject(file);
    if (error.isNotEmpty()) { fail(error); return false; }
    clearRecovery(); sessionFile = file; dirty = false; setStatus("Saved " + file.getFullPathName()); return true;
}
void Workstation::chooseSave(std::function<void()> onSaved, bool useSessionFile)
{
    if (choosingSave) return;
    if (busy) { setStatus("Wait for the current task to finish, then save or close again. Your mix is still open."); return; }
    if (!ensureTempoReady([safe = juce::Component::SafePointer<Workstation>(this), onSaved, useSessionFile] { if (safe) safe->chooseSave(onSaved, useSessionFile); })) return;
    if (useSessionFile && sessionFile != juce::File()) { if (saveSessionFile(sessionFile) && onSaved) onSaved(); return; }
    choosingSave = true;
    chooser = std::make_unique<juce::FileChooser>("Save mix", sessionFile.existsAsFile() ? sessionFile : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Session.mixworks"), "*.mixworks", false);
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting, [safe = juce::Component::SafePointer<Workstation>(this), onSaved](const auto& f)
    {
        if (!safe) return;
        safe->choosingSave = false;
        if (f.getResult() != juce::File() && safe->saveSessionFile(f.getResult().withFileExtension("mixworks")) && onSaved)
            juce::MessageManager::callAsync([safe, onSaved] { if (safe) onSaved(); });
    });
}
void Workstation::chooseOpen()
{
    chooser = std::make_unique<juce::FileChooser>("Open mix", sessionFile, "*.mixworks", false);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe = juce::Component::SafePointer<Workstation>(this)](const auto& f)
    {
        if (!safe || f.getResult() == juce::File()) return;
        safe->openProjectFile(f.getResult());
    });
}
void Workstation::openProjectFile(const juce::File& file) { openProjectDocument({}, file); }
void Workstation::openProjectDocument(juce::String document, const juce::File& file, bool recovered,
                                      std::shared_ptr<RecoveryStore> recovery, juce::String fingerprint)
{
    if (busy) return;
    setBusy(true); engine.playing = false; setStatus("Opening session and library...");
    auto cancelled = std::make_shared<std::atomic<bool>>(false); importCancellation = cancelled;
    importProgress.begin(0); importProgress.setOpeningSession();
    auto safe = juce::Component::SafePointer<Workstation>(this);
    workers.addJob([safe, file, cancelled, document = std::move(document), recovered, recovery, fingerprint]
    {
        if (!safe) return;
        auto staged = safe->engine.prepareProject(document.isEmpty() ? file.loadFileAsString() : document, [safe, cancelled](int done, int total, const juce::String& name, MediaReadStage phase)
        {
            juce::MessageManager::callAsync([safe, cancelled, done, total, name, phase]
            {
                if (!safe || safe->importCancellation != cancelled) return;
                safe->importProgress.setCounts(done, total); safe->importProgress.showFile(name, phase);
            });
        }, cancelled);
        juce::MessageManager::callAsync([safe, file, cancelled, recovered, recovery, fingerprint, staged = std::make_shared<PreparedProject>(std::move(staged))]() mutable
        {
            if (!safe || safe->importCancellation != cancelled) return;
            safe->importProgress.setVisible(false); safe->importCancellation.reset();
            if (*cancelled) staged->error = "Session opening cancelled. The current session was kept.";
            // Editors must be destroyed before their processors can be replaced.
            if (staged->error.isEmpty()) { safe->editors.clear();  }
            auto previousLibrary = safe->engine.library;
            auto error = safe->engine.commitProject(std::move(*staged));
            safe->setBusy(false);
            if (error.isNotEmpty()) { safe->refresh(); safe->fail(error); }
            else
            {
                safe->clearRecovery();
                safe->cancelTimelineAudio();
                for (const auto& media : previousLibrary)
                    if (std::none_of(safe->engine.library.begin(), safe->engine.library.end(), [&](const auto& source) { return source->file == media->file; })) safe->engine.library.push_back(media->catalogueCopy());
                safe->markCatalogueChanged();
                safe->timeline.selectAutomationPoint({}, -1);
                safe->sessionFile = file; safe->musicAttempts.clear(); safe->undoHistory.clear(); safe->redoHistory.clear(); safe->selectedClip.clear(); safe->tempoStrip.selectedTempoPoint = -1; safe->pendingTempo = 0; safe->tempoPreparationPending = false; safe->dirty = recovered; safe->refresh();
                int reused = 0, decoded = 0, prepared = 0, unsaved = 0;
                for (const auto& media : safe->engine.library) { if (media->analysisFromCache) ++reused; if (media->decodedFromCache) ++decoded; if (media->analysisCacheWarning.isNotEmpty() || media->playbackCacheWarning.isNotEmpty()) ++unsaved; }
                for (const auto& clip : safe->engine.project.clips) if (clip.prepared) { if (clip.prepared->fromDiskCache) ++prepared; if (clip.prepared->cacheWarning.isNotEmpty()) ++unsaved; }
                safe->setStatus("Opened " + file.getFileName() + " / reused " + juce::String(reused) + " analyses / " + juce::String(decoded) + " decoded / " + juce::String(prepared) + " tempo caches" + (unsaved > 0 ? " / " + juce::String(unsaved) + " cache warnings" : ""));
                if (recovered)
                {
                    safe->setStatus("Recovered unsaved mix. Save the session to keep these changes.");
                    // Keep the leased source snapshot until the recovered document has
                    // been saved or durably snapshotted under this window's ownership.
                    safe->restoredRecovery = recovery; safe->restoredRecoveryFingerprint = fingerprint;
                    safe->nextRecovery = 0;
                }

            }
        });
    });
}
void Workstation::chooseExport()
{
    if (busy) return;
    if (!ensureTempoReady([safe = juce::Component::SafePointer<Workstation>(this)] { if (safe) safe->chooseExport(); })) return;
    chooser = std::make_unique<juce::FileChooser>("Export stereo 24-bit WAV", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Mix.wav"), "*.wav", false);
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting, [safe = juce::Component::SafePointer<Workstation>(this)](const auto& f)
    {
        if (!safe || f.getResult() == juce::File()) return;
        safe->chooseExportEnd(f.getResult().withFileExtension("wav"));
    });
}
void Workstation::choosePlugin()
{
    if (busy) return;
    juce::Array<juce::File> files; findPlugins(juce::File("C:\\Program Files\\Common Files\\VST3"), files);
    files.sort(); juce::PopupMenu menu; for (int i = 0; i < files.size(); ++i) menu.addItem(i + 1, files[i].getFileNameWithoutExtension());
    menu.addSeparator(); menu.addItem(files.size() + 1, "Choose another VST3...");
    const auto effectOwner = selectedClip;
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&addPluginButton), [safe = juce::Component::SafePointer<Workstation>(this), files, effectOwner](int id)
    {
        if (!safe || id <= 0 || safe->selectedClip != effectOwner) return;
        auto load = [safe, effectOwner](const juce::File& file)
        {
            if (!safe || file == juce::File() || safe->selectedClip != effectOwner) return; safe->setStatus("Loading " + file.getFileName() + "...");
            const int track = safe->engine.selectedTrack; auto error = safe->engine.addPlugin(track, file);
            if (error.isNotEmpty()) safe->fail(error); else { safe->dirty = true; safe->refresh(); const auto latency = safe->engine.pluginLatencyError(); safe->setStatus(latency.isNotEmpty() ? latency : "Loaded " + file.getFileName()); safe->showEditor((int) safe->engine.pluginChain(track).size() - 1); }
        };
        if (id <= files.size()) load(files[id - 1]);
        else
        {
            safe->chooser = std::make_unique<juce::FileChooser>("Choose an effect", juce::File("C:\\Program Files\\Common Files\\VST3"), "*.vst3", false);
            safe->chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectDirectories, [load](const auto& f) { load(f.getResult()); });
        }
    });
}
void Workstation::configureLoop()
{
    auto* clip = currentClip(); if (busy || !clip) return;
    if (engine.project.clipTransitionLocked(clip->id)) { setStatus("Unlock the transition before looping its track."); return; }
    const auto id = clip->id;
    auto* dialog = new juce::AlertWindow("Loop section", "Repeat from the clip's start point.", juce::MessageBoxIconType::NoIcon, this);
    dialog->addComboBox("beats", {"1 beat", "2 beats", "4 beats", "8 beats", "16 beats", "32 beats"}, "Section length"); dialog->getComboBoxComponent("beats")->setSelectedId(3);
    dialog->addComboBox("repeats", {"2 times", "4 times", "8 times", "16 times", "32 times"}, "Repeat section"); dialog->getComboBoxComponent("repeats")->setSelectedId(2);
    dialog->addButton("Set loop", 1, juce::KeyPress(juce::KeyPress::returnKey)); if (clip->loopSpan > 0) dialog->addButton("Remove loop", 2);
    dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    dialog->enterModalState(true, juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<Workstation>(this), dialog, id](int answer)
    {
        if (!safe || answer == 0 || safe->engine.project.clipTransitionLocked(id) || !safe->currentClip() || safe->currentClip()->id != id) return;
        auto* selected = safe->currentClip();
        const double sourceBpm = selected->media->bpm() > 0 ? selected->media->bpm() : safe->engine.project.bpm / selected->rate;
        const double span = std::pow(2.0, dialog->getComboBoxComponent("beats")->getSelectedId() - 1) * 60 / sourceBpm;
        if (answer == 1 && selected->offset + span > selected->media->duration()) { safe->fail("The selected section exceeds the source audio. Move Source in earlier or choose fewer beats."); return; }
        safe->remember(); const juce::ScopedLock guard(safe->engine.lock); safe->scheduleTempoPreparation();
        const double sourceLength = answer == 2 ? selected->loopSpan : span * std::pow(2.0, dialog->getComboBoxComponent("repeats")->getSelectedId());
        const double duration = selected->localTimeForSource(sourceLength);
        if (answer == 2) { selected->loopSpan = selected->loopPhase = 0; }
        else { selected->loopSpan = span; selected->loopPhase = 0; }
        selected->length = duration; selected->rate = sourceLength / duration;
        selected->introAnchor.reset(); selected->outroAnchor.reset();
        selected->fadeInAnchor.reset(); selected->fadeOutAnchor.reset();
        selected->fadeIn = std::min(selected->fadeIn, selected->length); selected->fadeOut = std::min(selected->fadeOut, selected->length);
        safe->engine.resetPlugins(); safe->refresh();
    }), true);
}
void Workstation::configurePluginControls()
{
    if (busy) return;
    const int track = engine.selectedTrack.load();
    const auto& chain = engine.pluginChain(track);
    int insert = pluginList.list.getSelectedRow();
    if (insert < 0 && !chain.empty()) insert = 0;
    if (insert < 0 || insert >= (int) chain.size()) { setStatus("Add a VST3 to this track first."); return; }
    const auto slotId = chain[(size_t) insert]->id;
    auto* dialog = new juce::AlertWindow("Plugin MIDI controls", "Choose a control for " + chain[(size_t) insert]->description.name + ". Save your mix to keep these choices.\n\n" + engine.pluginControlSummary(track), juce::MessageBoxIconType::NoIcon, this);
    juce::StringArray controls; for (int i = 0; i < 8; ++i) controls.add("Control " + juce::String(i + 1));
    juce::StringArray parameters { "Unassigned" }; std::vector<int> indices { -1 };
    const auto& available = chain[(size_t) insert]->processor->getParameters();
    for (int i = 0; i < available.size(); ++i)
        if (available[i]->isAutomatable()) { parameters.add(available[i]->getName(128) + " [" + juce::String(i + 1) + "]"); indices.push_back(i); }
    dialog->addComboBox("control", controls, "MIDI control"); dialog->addComboBox("parameter", parameters, "Plugin parameter");
    dialog->getComboBoxComponent("parameter")->setSelectedItemIndex(-1, juce::dontSendNotification);
    dialog->getComboBoxComponent("parameter")->setTextWhenNothingSelected("Choose a parameter");
    dialog->addButton("Assign", 1, juce::KeyPress(juce::KeyPress::returnKey)); dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    dialog->enterModalState(true, juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<Workstation>(this), dialog, track, insert, slotId, indices](int answer)
    {
        if (!safe || answer != 1) return;
        const auto& current = safe->engine.pluginChain(track);
        if (insert >= (int) current.size() || current[(size_t) insert]->id != slotId) return;
        const int choice = dialog->getComboBoxComponent("parameter")->getSelectedItemIndex();
        if (choice < 0 || choice >= (int) indices.size()) { safe->setStatus("No parameter selected; assignment kept."); return; }
        auto error = safe->engine.assignPluginControl(track, dialog->getComboBoxComponent("control")->getSelectedItemIndex(), insert, indices[(size_t) choice]);
        if (error.isNotEmpty()) safe->fail(error); else { safe->dirty = true; safe->setStatus("Plugin MIDI control assigned. Save the session to keep this choice."); }
    }), true);
}
void Workstation::showEditor(int index)
{
    if (busy) return; auto& chain = engine.pluginChain(engine.selectedTrack.load()); if (index < 0 || index >= (int) chain.size()) return;
    auto& slot = *chain[(size_t) index]; for (auto& editor : editors) if (editor->slotId == slot.id) { editor->setVisible(true); editor->toFront(true); return; }
    auto window = std::make_unique<PluginWindow>(slot);
    if (auto* c = currentClip()) window->effectsOwner = c->effects;
    editors.push_back(std::move(window));
}
void Workstation::settings()
{
    if (busy) return;
    if (settingsWindow) { settingsWindow->setVisible(true); settingsWindow->toFront(true); return; }
    auto* window = new AlloyPopupWindow<juce::DialogWindow>("Audio, MIDI and FLkey 49", colour::panel, true);
    settingsWindow.reset(window); window->setUsingNativeTitleBar(true);
    window->setContentOwned(new MidiSettings(engine), true); window->setResizable(false, false);
    window->centreAroundComponent(this, window->getWidth(), window->getHeight());
    window->setVisible(true); window->toFront(true);
    AlloyLookAndFeel::styleNativeTitleBar(*settingsWindow);
}
bool Workstation::keyPressed(const juce::KeyPress& k)
{
    if (k.getKeyCode() == juce::KeyPress::F11Key) { toggleWindowMode(); return true; }
    if (k.getKeyCode() == 'A' && k.getModifiers().isCtrlDown() && k.getModifiers().isShiftDown()) { setAttractMode(!attractState.enabled); return true; }
    if (k.getKeyCode() == juce::KeyPress::escapeKey && attractState.enabled) { setAttractMode(false); return true; }
    if (busy || dynamic_cast<juce::TextEditor*>(juce::Component::getCurrentlyFocusedComponent())) return false;
    if (transitionKeyPressed(k)) return true;
    if (k.getKeyCode() == juce::KeyPress::escapeKey && loopStartClip.isNotEmpty()) { loopStartClip.clear(); timeline.repaint(); setStatus("Loop start cancelled."); return true; }
    if (k.getKeyCode() == juce::KeyPress::spaceKey) { playButton.triggerClick(); return true; }
    if (k.getKeyCode() == juce::KeyPress::deleteKey) { if (timeline.selectedLabel.isNotEmpty()) removeLabelMarker(); else removeClip(); return true; }
    if (k.getModifiers().isCtrlDown())
    {
        if (k.getKeyCode() == 'X' || k.getKeyCode() == 'C' || k.getKeyCode() == 'V' || k.getKeyCode() == 'W' || k.getKeyCode() == 'T')
        {
            const auto action = k.getKeyCode() == 'X' ? TimelineAction::cut : k.getKeyCode() == 'C' ? TimelineAction::copy : k.getKeyCode() == 'V' ? TimelineAction::paste : k.getKeyCode() == 'W' ? TimelineAction::tweaks : TimelineAction::settings;
            performTimelineAction(action, engine.position.load(), engine.selectedTrack.load()); return true;
        }
        if (k.getTextCharacter() == 's' || k.getKeyCode() == 'S') { chooseSave(); return true; }
        if (k.getKeyCode() == 'Z') { undo(k.getModifiers().isShiftDown()); return true; }
        if (k.getKeyCode() == 'D') { duplicateClip(); return true; }
        if (k.getKeyCode() == 'O') { guardUnsaved([this] { chooseOpen(); }); return true; }
    }
    if (k.getKeyCode() == 'S') { splitClip(); return true; }
    if (!k.getModifiers().isCtrlDown() && !k.getModifiers().isAltDown())
    {
        const juce::String keys("VTMBE90"); const int index = keys.indexOfChar((juce::juce_wchar) k.getKeyCode());
        const TimelineAction actions[] {TimelineAction::volume, TimelineAction::treble, TimelineAction::mid, TimelineAction::bass, TimelineAction::effect, TimelineAction::loopStart, TimelineAction::loopEnd};
        if (index >= 0) { performTimelineAction(actions[index], engine.position.load(), engine.selectedTrack.load()); return true; }
        if (k.getKeyCode() == 'P') { performTimelineAction(TimelineAction::tempo, engine.position.load(), engine.selectedTrack.load()); return true; }
        if (k.getKeyCode() == 'L') { editLabelMarker({}, engine.position); return true; }
        if (k.getKeyCode() == '[') { navigateLabel(false); return true; }
        if (k.getKeyCode() == ']') { navigateLabel(true); return true; }
    }
    return false;
}
}
