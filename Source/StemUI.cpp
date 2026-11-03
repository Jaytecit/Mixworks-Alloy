#include "Workstation.h"
#include "StemJobs.h"
#include "Beatmatch.h"
#include <cstdlib>
#include <cmath>

namespace mixworks
{
void Timeline::paintStems(juce::Graphics& g, const Clip& c)
{
    if (!c.stems) return;
    const auto r = clipBounds(c); const auto view = owner.viewport.getViewArea().toFloat();
    const float left = std::max(r.getX(), view.getX());
    const auto badge = juce::Rectangle<float>(left + 4, r.getY() + 3, 112, 18).getIntersection(r);
    g.setColour(colour::panel); g.fillRect(badge); g.setColour(colour::orange); g.setFont(juce::FontOptions(11));
    const bool solo = c.stemsEnabled && std::any_of(c.stemControls.begin(), c.stemControls.end(), [](const auto& control) { return control.solo; });
    g.drawText(juce::String(c.stemsExpanded ? "- Stems" : "+ Stems") + (c.stemsEnabled ? (solo ? " / SOLO" : "") : " / original"), badge, juce::Justification::centredLeft);
    if (!c.stemsExpanded || r.getHeight() < 100) return;
    auto area = r.withTrimmedTop(43).withTrimmedBottom(4);
    const float height = area.getHeight() / stemCount;
    for (int s = 0; s < stemCount; ++s)
    {
        const auto row = area.withY(area.getY() + s * height).withHeight(height);
        const auto& controls = c.stemControls[(size_t) s];
        g.setColour(colour::panel.withAlpha(.95f)); g.fillRect(row);
        const auto accent = colour::tracks[(size_t) s];
        g.setColour(accent.withAlpha(controls.mute ? .15f : .4f));
        const auto& media = *c.stems->sources[(size_t) s];
        for (int x = (int) std::max(row.getX(), view.getX()); x < std::min(row.getRight(), view.getRight()); ++x)
        {
            const double a = c.sourceTimeAt((x - .5) / pixelsPerSecond - c.start), b = c.sourceTimeAt((x + .5) / pixelsPerSecond - c.start);
            const auto level = media.waveformRange(std::min(a, b), std::max(a, b));
            const float amplitude = std::min(1.f, level.peak) * (height - 4) * .5f;
            g.drawVerticalLine(x, row.getCentreY() - amplitude, row.getCentreY() + amplitude);
        }
        const auto label = row.withX(left + 4).withWidth(std::min(112.f, row.getRight() - left - 4));
        g.setColour(colour::panel.withAlpha(.9f)); g.fillRect(label);
        g.setColour(controls.mute ? colour::muted : accent);
        g.drawText(juce::String(stemNames[s]) + (controls.mute ? " M" : controls.solo ? " S" : "") + " " + juce::String(controls.gainDb, 0) + "dB", label, juce::Justification::centredLeft);
        const auto button = juce::Rectangle<float>(left + 118, row.getY() + 2, 42, std::min(22.f, height - 4)).getIntersection(row);
        const bool active = c.stemsEnabled && controls.solo;
        g.setColour(active ? colour::orange : colour::panel); g.fillRoundedRectangle(button, 3);
        g.setColour(active ? colour::panel : colour::text); g.drawRoundedRectangle(button, 3, 1);
        g.drawText("Solo", button, juce::Justification::centred);
    }
}
bool Timeline::stemMouseDown(const juce::MouseEvent& e)
{
    for (auto it = owner.engine.project.clips.rbegin(); it != owner.engine.project.clips.rend(); ++it)
    {
        const auto& c = *it; if (!c.stems) continue;
        const auto r = clipBounds(c); if (!r.contains(e.position)) continue;
        const float left = std::max(r.getX(), (float) owner.viewport.getViewPositionX());
        if (e.x > left + 160 || e.y < r.getY() + 3) return false;
        if (c.stemsExpanded && r.getHeight() >= 100 && e.y >= r.getY() + 43 && e.y < r.getBottom() - 4)
        {
            const float height = (r.getHeight() - 47) / stemCount;
            const int stem = juce::jlimit(0, stemCount - 1, (int) ((e.y - r.getY() - 43) / height));
            const auto button = juce::Rectangle<float>(left + 118, r.getY() + 43 + stem * height + 2, 42, std::min(22.f, height - 4)).getIntersection(r);
            if (button.contains(e.position))
            {
                if (e.mods.isPopupMenu()) owner.showStemMenu(c.id, stem, e.x / pixelsPerSecond);
                else owner.toggleStemSolo(c.id, stem);
                return true;
            }
        }
        if (e.x > left + 116 || automationPointAt(e.position)) return false;
        if (e.y < r.getY() + 21)
        {
            if (e.mods.isPopupMenu()) owner.showStemMenu(c.id, -1, e.x / pixelsPerSecond);
            else { owner.remember(); owner.engine.project.clipFor(c.id)->stemsExpanded = !c.stemsExpanded; owner.dirty = true; owner.refresh(); }
            return true;
        }
        if (e.y < r.getY() + 43 || !c.stemsExpanded || r.getHeight() < 100) return false;
        const int stem = juce::jlimit(0, stemCount - 1, (int) ((e.y - r.getY() - 43) / ((r.getHeight() - 47) / stemCount)));
        owner.showStemMenu(c.id, stem, e.x / pixelsPerSecond); return true;
    }
    return false;
}
namespace
{
class StemPanel final : public juce::Component, private juce::Timer
{
public:
    StemPanel(std::shared_ptr<StemJob> active, juce::String sourceName, std::function<void(const juce::File&)> use) : job(std::move(active))
    {
        title.setText(sourceName, juce::dontSendNotification);
        title.setFont(juce::FontOptions(18));
        detail.setText("Vocals, drums, bass and other - source-aligned WAVs", juce::dontSendNotification);
        detail.setFont(juce::FontOptions(13));
        message.setJustificationType(juce::Justification::centredLeft);
        message.setFont(juce::FontOptions(14));
        progress = std::make_unique<juce::ProgressBar>(fraction);
        for (auto* c : std::initializer_list<juce::Component*> {&title, &detail, &message, &cancelButton, &openButton, &useButton, progress.get()}) addAndMakeVisible(c);
        cancelButton.onClick = [this] { job->cancel(); };
        openButton.onClick = [this] { const auto state = job->state(); if (state.ready) state.directory.revealToUser(); };
        useButton.onClick = [this, use] { const auto state = job->state(); if (state.ready) use(state.directory); };
        setSize(560, 230); timerCallback(); startTimerHz(5);
    }
    void resized() override
    {
        auto area = getLocalBounds().reduced(20);
        title.setBounds(area.removeFromTop(28)); detail.setBounds(area.removeFromTop(24)); area.removeFromTop(8);
        message.setBounds(area.removeFromTop(62)); progress->setBounds(area.removeFromTop(22)); area.removeFromTop(16);
        cancelButton.setBounds(area.removeFromLeft(100)); useButton.setBounds(area.removeFromLeft(180)); openButton.setBounds(area.removeFromRight(160));
    }
private:
    void timerCallback() override
    {
        const auto state = job->state(); fraction = state.progress;
        message.setText(state.message + (state.error.isEmpty() ? "" : "\n" + state.error), juce::dontSendNotification);
        cancelButton.setEnabled(state.running); openButton.setEnabled(state.ready); useButton.setEnabled(state.ready);
    }
    std::shared_ptr<StemJob> job;
    juce::Label title, detail, message;
    juce::TextButton cancelButton {"Cancel"}, openButton {"Open stem folder"}, useButton {"Use on timeline"};
    double fraction = 0;
    std::unique_ptr<juce::ProgressBar> progress;
};
}
void Workstation::splitStems(const juce::String& sourceId, bool cpu)
{
    if (!stemJob || !stemJob->state().running)
    {
        const auto found = std::find_if(engine.library.begin(), engine.library.end(), [&](const auto& m) { return m->id == sourceId; });
        if (found == engine.library.end()) { fail("Source is no longer in the library."); return; }
        stemJob = std::make_shared<StemJob>((*found)->file, StemJob::defaultRoot(), StemJob::defaultRuntime(), cpu ? "cpu" : "auto");
        stemSourceName = (*found)->name; stemSourceId = sourceId; stemJob->begin();
    }
    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Split track"; options.dialogBackgroundColour = colour::panel;
    options.content.setOwned(new StemPanel(stemJob, stemSourceName, [safe = juce::Component::SafePointer<Workstation>(this), id = stemSourceId](const juce::File& directory) { if (safe) safe->attachStems(id, directory); }));
    options.componentToCentreAround = this; options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true; options.resizable = false; options.launchAsync();
}
void Workstation::attachStems(const juce::String& sourceId, const juce::File& directory)
{
    if (!canAutoArrange()) { setStatus("Pause playback, unlock transitions and wait for preparation before using stems."); return; }
    auto candidate = engine.project;
    const auto found = std::find_if(candidate.clips.begin(), candidate.clips.end(), [&](const auto& c) { return c.media->id == sourceId; });
    if (found == candidate.clips.end()) { setStatus("Place this song on the timeline, then choose Use on timeline."); return; }
    const auto parent = found->media; setBusy(true); setStatus("Loading source-aligned stems...");
    workers.addJob([safe = juce::Component::SafePointer<Workstation>(this), candidate = std::move(candidate), parent, sourceId, directory]() mutable
    {
        std::shared_ptr<const StemAssets> assets;
        juce::String error = loadStemAssets(*parent, directory, {}, assets).getErrorMessage();
        if (error.isEmpty())
        {
            for (auto& c : candidate.clips) if (c.media->id == sourceId)
            {
                c.stems = assets; c.stemDirectory = directory; c.stemManifestHash = assets->manifestHash;
                c.stemsEnabled = c.stemsExpanded = true; c.stemPrepared = {};
            }
            error = prepareSavedClips(candidate);
        }
        juce::MessageManager::callAsync([safe, candidate = std::move(candidate), error]() mutable
        {
            if (!safe) return;
            safe->setBusy(false);
            if (error.isNotEmpty()) { safe->fail(error); return; }
            safe->remember();
            { const juce::ScopedLock guard(safe->engine.lock); safe->engine.project = std::move(candidate); safe->engine.resetPlugins(); }
            safe->dirty = true; safe->refresh(); safe->setStatus("Stems ready. Solo isolates a stem on its lane; other lanes keep playing. Click its label for level, mute or automation.");
        });
    });
}
void Workstation::toggleStemSolo(const juce::String& id, int stem)
{
    const juce::ScopedLock guard(engine.lock);
    auto* clip = engine.project.clipFor(id);
    if (!clip || !clip->stems || stem < 0 || stem >= stemCount || busy || engine.exporting) return;
    if (clip->pendingPlacement != PendingPlacement::none) { setStatus("Wait for this segment's audio preparation before changing stem Solo."); return; }
    if (engine.project.clipTransitionLocked(id)) { setStatus("Unlock this segment's transition before changing stem Solo."); return; }
    const bool enable = !clip->stemsEnabled || !clip->stemControls[(size_t) stem].solo;
    if (enable) for (const auto& other : engine.project.clips)
        if (other.track == clip->track && other.id != id && engine.project.clipTransitionLocked(other.id)
            && std::any_of(other.stemControls.begin(), other.stemControls.end(), [](const auto& control) { return control.solo; }))
        { setStatus("Unlock the other soloed segment's transition before switching stem Solo."); return; }
    remember();
    if (enable)
    {
        for (auto& other : engine.project.clips) if (other.track == clip->track)
            for (auto& control : other.stemControls) control.solo = false;
        clip->stemsEnabled = true;
        clip->stemControls[(size_t) stem].mute = false;
    }
    clip->stemControls[(size_t) stem].solo = enable;
    selectedClip = id; selectTrack(clip->track);
    // Keep continuous processor histories: a local Solo must not reset other
    // lanes, shared split returns or master effects during playback.
    dirty = true; refresh();
    setStatus(enable ? "Solo: " + juce::String(stemNames[stem]) + " on lane " + juce::String(clip->track + 1) + ". Other lanes unchanged. Lane Mute/Solo, levels and effects still apply."
                     : "Stem Solo off on lane " + juce::String(clip->track + 1) + ". Other lanes unchanged.");
}
bool Workstation::restoreTimelineStems()
{
    if (pendingStemDiscovery.empty()) return false;
    std::vector<std::pair<juce::String, std::shared_ptr<Media>>> pending;
    for (const auto& id : pendingStemDiscovery)
        if (const auto* clip = engine.project.clipFor(id); clip && !clip->stems && clip->stemDirectory == juce::File() && clip->media->hasPlayback())
            pending.emplace_back(id,clip->media);
    pendingStemDiscovery.clear();
    if (pending.empty() || !StemJob::defaultRoot().isDirectory()) return false;
    setBusy(true); setStatus("Checking for previously separated stems...");
    workers.addJob([safe = juce::Component::SafePointer<Workstation>(this), pending = std::move(pending)]
    {
        std::map<std::shared_ptr<Media>,std::shared_ptr<const StemAssets>> assets;
        juce::StringArray warnings;
        for (const auto& entry : pending) if (!assets.count(entry.second))
        {
            std::shared_ptr<const StemAssets> stems;
            const auto result = loadCachedStemAssets(*entry.second,StemJob::defaultRoot(),stems);
            assets.emplace(entry.second,std::move(stems));
            if (result.failed()) warnings.add(entry.second->libraryTitle() + ": " + result.getErrorMessage());
        }
        juce::MessageManager::callAsync([safe,pending,assets = std::move(assets),warnings]() mutable
        {
            if (!safe) return;
            int restored = 0;
            {
                const juce::ScopedLock guard(safe->engine.lock);
                for (const auto& entry : pending)
                {
                    auto* clip = safe->engine.project.clipFor(entry.first);
                    const auto found = assets.find(entry.second);
                    if (!clip || clip->media != entry.second || clip->stems || clip->stemDirectory != juce::File()
                        || found == assets.end() || !found->second) continue;
                    clip->stems = found->second; clip->stemDirectory = clip->stems->directory; clip->stemManifestHash = clip->stems->manifestHash;
                    clip->stemsEnabled = clip->stemsExpanded = true; ++restored;
                }
            }
            safe->setBusy(false); safe->dirty |= restored > 0; safe->refresh();
            if (restored || !warnings.isEmpty())
            {
                auto action = std::move(safe->afterTimelineAudio);
                const auto notice = juce::String(restored) + " segment(s) restored previous stems."
                    + (warnings.isEmpty() ? "" : " Unavailable stems kept original audio: " + warnings.joinIntoString(" / "));
                safe->afterTimelineAudio = [safe,action = std::move(action),notice]
                { if (action) action(); if (safe) safe->setStatus(safe->status + " " + notice); };
            }
            safe->finishTimelineAudio();
        });
    });
    return true;
}
void Workstation::showStemMenu(const juce::String& id, int stem, double time)
{
    const auto* c = engine.project.clipFor(id); if (!c || !c->stems) return;
    juce::PopupMenu menu;
    menu.addItem(1, c->stemsExpanded ? "Collapse stems" : "Expand stems");
    menu.addItem(2, c->stemsEnabled ? "Play original track" : "Play stem mix", !engine.project.clipTransitionLocked(id));
    const bool editable = !busy && !engine.exporting && !engine.project.clipTransitionLocked(id);
    if (stem >= 0 && stem < stemCount)
    {
        const auto& controls = c->stemControls[(size_t) stem];
        menu.addSectionHeader(stemNames[stem]);
        menu.addItem(3, "Mute", editable, controls.mute); menu.addItem(4, "Solo this stem on this lane", editable, c->stemsEnabled && controls.solo);
        menu.addItem(5, "Level / pan...", editable);
        menu.addItem(6, "Edit volume automation", editable); menu.addItem(7, "Edit pan automation", editable);
        menu.addItem(8, "Add volume node here...", editable);
    }
    else for (int s = 0; s < stemCount; ++s) menu.addItem(20 + s, juce::String(stemNames[s]) + " / controls...");
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&timeline),
        [safe = juce::Component::SafePointer<Workstation>(this), id, stem, time](int action)
    {
        if (!safe || !action || safe->busy || safe->engine.exporting) return;
        auto* clip = safe->engine.project.clipFor(id); if (!clip || !clip->stems) return;
        if (action >= 20) { safe->showStemMenu(id, action - 20, time); return; }
        if (action != 1 && safe->engine.project.clipTransitionLocked(id)) return;
        if (action == 4) { safe->toggleStemSolo(id, stem); return; }
        safe->selectedClip = id; safe->selectTrack(clip->track);
        if (action >= 6 && action <= 8)
        {
            const AutomationAddress address {clip->track, action == 7 ? AutomationTarget::pan : AutomationTarget::volume, {}, {}, id, stem};
            safe->timeline.selectAutomationCurve(address); safe->showToolPage(2); safe->refresh();
            if (action == 8) safe->editAutomationPoint(clip->track, -1, time);
            return;
        }
        if (action == 5)
        {
            auto* dialog = new juce::AlertWindow(juce::String(stemNames[stem]) + " / mix", "Set level and stereo balance. Existing curves remain available through Read.", juce::MessageBoxIconType::NoIcon, safe);
            dialog->addTextEditor("gain", juce::String(clip->stemControls[(size_t) stem].gainDb, 2), "Level dB (-60 to +12)");
            dialog->addTextEditor("pan", juce::String(clip->stemControls[(size_t) stem].pan, 2), "Pan (-1 left to +1 right)");
            dialog->addButton("Apply", 1, juce::KeyPress(juce::KeyPress::returnKey)); dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
            dialog->enterModalState(true, juce::ModalCallbackFunction::create([safe, id, stem, dialog](int answer)
            {
                if (!safe || answer != 1 || safe->busy || safe->engine.exporting || safe->engine.project.clipTransitionLocked(id)) return;
                auto* c = safe->engine.project.clipFor(id); if (!c || !c->stems) return;
                auto number = [](juce::String text, double& value) { auto raw = text.trim().toStdString(); char* end = nullptr; value = std::strtod(raw.c_str(), &end); return !raw.empty() && end == raw.c_str() + raw.size() && std::isfinite(value); };
                double gain = 0, pan = 0;
                if (!number(dialog->getTextEditorContents("gain"), gain) || !number(dialog->getTextEditorContents("pan"), pan) || gain < -60 || gain > 12 || pan < -1 || pan > 1) { safe->setStatus("Enter a valid stem level and pan."); return; }
                const juce::ScopedLock guard(safe->engine.lock); safe->remember();
                auto& settings = c->stemControls[(size_t) stem]; settings.gainDb = (float) gain; settings.pan = (float) pan;
                settings.automation[0].enabled = settings.automation[4].enabled = false; safe->dirty = true; safe->refresh();
            }), true);
            return;
        }
        const juce::ScopedLock guard(safe->engine.lock); safe->remember();
        if (action == 1) clip->stemsExpanded = !clip->stemsExpanded;
        if (action == 2) clip->stemsEnabled = !clip->stemsEnabled;
        if (action == 3) clip->stemControls[(size_t) stem].mute = !clip->stemControls[(size_t) stem].mute;
        safe->dirty = true; safe->refresh();
    });
}
}
