#include "Workstation.h"
#include "Hotcues.h"
#include "Beatmatch.h"
#include <cmath>
#include <limits>
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace mixworks
{
void runHotcueTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    const auto json = [](const Project& p) { return juce::JSON::toString(projectToJson(p), true, 17); };
    auto source = std::make_shared<Media>(); source->name = "Hotcue source"; source->sampleRate = 8000;
    source->manualBpm = 120; source->manualBeatOrigin = 0; source->analysis.downbeatOrigin = 0;
    source->file = report.getSiblingFile("hotcue-source.wav"); source->samples.setSize(1, 96 * 8000);
    for (int n = 0; n < source->frames(); ++n)
    { const double t = n / source->sampleRate; source->samples.setSample(0, n, (float) ((t < 32 || t >= 64 ? .1 : .5) * std::sin(n * .2356194490192345))); }
    source->prepareWaveformBody(); source->peaks.assign(source->waveformRms.size(), .5f);
    {
        juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream = source->file.createOutputStream();
        auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(1).withBitsPerSample(24));
        check(writer && writer->writeFromAudioSampleBuffer(source->samples, 0, source->frames()), "write actual owned source for hotcue Save/Open");
    }
    source->analysisSource = {audioDigest(source->file), source->file.getSize(), source->sampleRate, source->frames(), source->channels()};
    source->sourceModified = source->file.getLastModificationTime().toMilliseconds();
    Project base; base.transitionLinksKnown = true;
    Clip clip; clip.media = source; clip.start = 4; clip.length = 95; clip.audioSlip = .017; base.clips = {clip};
    const auto id = clip.id;
    check(!base.clips[0].hotcues && parseProject(json(base)).error.isEmpty(), "uninitialised cue set stays absent through a valid session32 document");
    check(base.seedHotcues(id).wasOk() && hotcueFor(base.clips[0], 0) && hotcueFor(base.clips[0], 1) && hotcueFor(base.clips[0], 2)
        && !hotcueFor(base.clips[0], 3) && hotcueFor(base.clips[0], 0)->sourceSeconds == 0
        && hotcueFor(base.clips[0], 1)->sourceSeconds == 32 && hotcueFor(base.clips[0], 2)->sourceSeconds == 64,
        "explicit first initialisation seeds source start and two retained section hints, leaving remaining slots empty");
    const auto defaults = base.clips[0].hotcues;
    auto edited = base; const auto originalIdentity = hotcueFor(edited.clips[0], 1)->id;
    check(edited.putHotcue(id, 1, 33.125).wasOk() && hotcueFor(edited.clips[0], 1)->id == originalIdentity && hotcueFor(edited.clips[0], 1)->manual,
        "manual move preserves cue identity and belongs only to that instance");
    check(edited.removeHotcue(id, 0).wasOk() && edited.clips[0].hotcues && !hotcueFor(edited.clips[0], 0), "cleared default slot remains an explicit tombstone");
    const auto savedCues = edited.clips[0].hotcues; const auto rms = source->waveformRms; source->waveformRms.assign(rms.size(), .2f);
    const auto retained = prepareTempoMap(edited, false, 0, 0, 0, {}, TransitionPreset::alignOnly);
    check(retained.error.isEmpty() && retained.project.clips[0].hotcues == savedCues, "new automatic analysis/normal preparation cannot reseed a deleted or manually moved cue");
    source->waveformRms = rms;
    auto copied = edited.clips[0]; copied.id = juce::Uuid().toString(); (*copied.hotcues)[1]->sourceSeconds = 44;
    const auto sliced = edited.clips[0].slice(20, 40);
    check(hotcueFor(edited.clips[0], 1)->sourceSeconds == 33.125 && sliced.hotcues == edited.clips[0].hotcues
        && copied.hotcues != edited.clips[0].hotcues, "copy/split snapshot independent cue values and retain trimmed-out records");
    for (int mode = 0; mode < 5; ++mode)
    {
        auto mapped = base.clips[0]; mapped.offset = 8; mapped.start = 11; mapped.length = 40; mapped.rate = 1.2; mapped.audioSlip = mode % 2 ? -.123 : .123;
        if (mode) { mapped.beatmatched = true; mapped.tempoMap = std::make_shared<const TempoMap>(TempoMap{120, {{0, 120, false}, {25, 132, true}, {90, 124, true}}, .217}); mapped.tempoSourceBpm = 120; mapped.rate = mapped.sourceElapsed(mapped.length) / mapped.length; }
        mapped.reversed = mode == 2 || mode == 4;
        if (mode >= 3) { mapped.loopSpan = 8; mapped.loopPhase = 2.5; }
        (*mapped.hotcues)[1]->sourceSeconds = mapped.offset + (mode >= 3 ? 3.25 : 17.3);
        const auto position = hotcueTimelineTime(mapped, 1, mapped.start + 12);
        check(position && *position >= mapped.start && *position < mapped.end()
            && std::abs(mapped.sourceTimeAt(*position - mapped.start) - hotcueFor(mapped, 1)->sourceSeconds) < 1.e-7,
            "cue maps to its actual source through trim/slip/rate/tempo/reverse/loop mode " + juce::String(mode));
        if (position)
        {
            const double before = *position; const auto cues = mapped.hotcues; auto translated = mapped;
            translated.start += 20;
            if (translated.tempoMap)
            {
                auto shifted = std::make_shared<TempoMap>(*translated.tempoMap); shifted->origin += 20;
                for (auto& point : shifted->points) point.time += 20;
                translated.tempoMap = std::move(shifted);
            }
            const auto shifted = hotcueTimelineTime(translated, 1, before + 20 - .001);
            check(shifted && std::abs(*shifted - before - 20) < 1.e-7 && translated.hotcues == cues,
                "translating the complete source clock shifts the cue occurrence by the same time " + juce::String(mode));
            mapped.moveTo(mapped.start + 20);
            const auto after = hotcueTimelineTime(mapped, 1, mapped.start + 12);
            check(after && mapped.hotcues == cues && std::abs(mapped.sourceTimeAt(*after - mapped.start) - hotcueFor(mapped, 1)->sourceSeconds) < 1.e-7,
                "ordinary movement retimes against the global clock while preserving cue source position/identity " + juce::String(mode));
        }
    }
    auto loop = base.clips[0]; loop.loopSpan = 8; loop.loopPhase = 1; (*loop.hotcues)[1]->sourceSeconds = 4;
    const auto first = hotcueTimelineTime(loop, 1), next = hotcueTimelineTime(loop, 1, 20);
    check(first && next && *next > *first && std::abs(loop.sourceTimeAt(*next - loop.start) - 4) < 1.e-7, "loop locator chooses the next valid source occurrence relative to programme cursor");
    auto trimmed = base.clips[0]; trimmed.offset = 40; trimmed.length = 30;
    check(!hotcueTimelineTime(trimmed, 0) && !hotcueTimelineTime(trimmed, 1) && hotcueTimelineTime(trimmed, 2)
        && trimmed.hotcues == defaults, "trimmed-out slots stay saved while unavailable for locate");
    const auto document = parseProject(json(edited));
    check(document.error.isEmpty() && document.clips[0].clip.hotcues == savedCues, "session32 round-trips slot tombstones, identities, source positions and manual state");
    auto old = projectToJson(edited); old.getDynamicObject()->setProperty("version", 31);
    (*old["clips"].getArray())[0].getDynamicObject()->removeProperty("hotcues");
    const auto legacy = parseProject(juce::JSON::toString(old));
    check(legacy.error.isEmpty() && !legacy.clips[0].clip.hotcues && !legacy.clips[0].clip.hotcuesPending, "opening session31 never silently initialises cues or new-instance intent");
    {
        auto older = std::make_unique<Engine>(juce::File(), false); const auto openedOld = older->restoreProject(juce::JSON::toString(old, true, 17));
        check(openedOld.isEmpty() && older->project.clips.size() == 1 && !older->project.clips[0].hotcues,
            "real session31 Open hydrates source without adding cue data: " + openedOld);
        auto added = std::make_unique<Workstation>(false); added->stopTimer(); added->insertTimelineMedia({source}, {});
        const auto pendingId = added->engine.project.clips[0].id;
        check(added->engine.project.clips[0].hotcuesPending && !added->engine.project.clips[0].hotcues,
            "actual new Library insertion carries explicit one-time cue initialisation intent");
        auto ready = prepareTempoMap(added->engine.project, false, 0, 0, 0, {}, TransitionPreset::alignOnly);
        const auto repeated = prepareTempoMap(added->engine.project, false, 0, 0, 0, {}, TransitionPreset::alignOnly);
        check(ready.error.isEmpty() && hotcueFor(ready.project.clips[0], 2) && !ready.project.clips[0].hotcuesPending
            && ready.project.clips[0].hotcues == repeated.project.clips[0].hotcues && ready.project.clips[0].id == pendingId,
            "new-source preparation seeds once with deterministic identities through replayed pending intent");
        if (ready.error.isEmpty())
        {
            const auto seeded = ready.project.clips[0].hotcues; added->engine.project = std::move(ready.project);
            added->undo(false); added->undo(true);
            check(added->engine.project.clips.size() == 1 && added->engine.project.clips[0].hotcues == seeded,
                "new insertion Undo/Redo retains the exact initialised slot snapshot");
        }
    }
    for (int failure = 0; failure < 6; ++failure)
    {
        auto bad = juce::JSON::parse(json(base)); auto c = (*bad["clips"].getArray())[0]; auto* slots = c["hotcues"].getArray();
        if (failure == 0) c.getDynamicObject()->removeProperty("hotcues");
        if (failure == 1) slots->removeLast();
        if (failure == 2) (*slots)[1].getDynamicObject()->setProperty("id", (*slots)[0]["id"]);
        if (failure == 3) (*slots)[0].getDynamicObject()->setProperty("sourceSeconds", -1);
        if (failure == 4) (*slots)[0].getDynamicObject()->setProperty("manual", "yes");
        if (failure == 5) (*slots)[0].getDynamicObject()->setProperty("id", juce::String());
        check(parseProject(juce::JSON::toString(bad)).error.isNotEmpty(), "malformed source-cue record rejects before mutation " + juce::String(failure));
    }
    const auto unchanged = json(edited);
    check(edited.putHotcue(id, 3, -1).failed() && edited.putHotcue(id, 3, std::numeric_limits<double>::quiet_NaN()).failed()
        && edited.putHotcue(id, 9, 10).failed() && json(edited) == unchanged, "invalid cue edits reject atomically");
    auto locked = edited; auto partner = clip; partner.id = juce::Uuid().toString(); partner.start = 80; partner.track = 1;
    locked.clips.push_back(partner); locked.transitionLinks = {{id, partner.id}}; locked.transitionLinks[0].locked = true;
    const auto protectedBefore = json(locked);
    check(locked.putHotcue(id, 1, 34).failed() && locked.removeHotcue(id, 1).failed() && locked.seedHotcues(id).failed()
        && json(locked) == protectedBefore, "transition lock protects cue changes, clearing and reseeding");
    auto bypass = locked; (*bypass.clips[0].hotcues)[1]->sourceSeconds += 1;
    check(locked.validateTransitionLocks(bypass).failed(), "whole-project candidate validation includes protected cue data");

    auto owned = std::make_unique<Workstation>(false); auto& ui = *owned; ui.stopTimer(); ui.setSize(1920, 1080);
    ui.engine.project = base; ui.engine.library = {source}; ui.selectedClip = id; ui.selectTrack(0); ui.timeline.pixelsPerSecond = 10; ui.refresh();
    ui.addToDesktop(0); ui.setVisible(true); ui.engine.position = 10; ui.dirty = false;
    auto& deck = *ui.performance;
    const auto beforeLocate = json(ui.engine.project); const auto history = ui.undoHistory.size();
    check(ui.timeline.hotcueFlags().empty(), "hotcue labels start hidden without removing cue data");
    ui.timeline.chooseCurveTool(7);
    check(!ui.timeline.hotcueFlags().empty() && ui.timeline.sectionFlags().empty()
        && json(ui.engine.project) == beforeLocate && ui.undoHistory.size() == history, "HC footer toggle reveals hotcues independently without changing data or history");
    ui.timeline.chooseCurveTool(7);
    check(ui.timeline.hotcueFlags().empty(), "HC footer toggle hides hotcue hit targets");
    deck.channelButtons[1].onClick();
    check(!ui.timeline.hotcueFlags().empty(), "explicit hotcue locate reveals its labels even after hiding them");
    check(!ui.engine.playing && std::abs(ui.engine.position.load() - 36.017) < 1.e-7 && json(ui.engine.project) == beforeLocate
        && ui.undoHistory.size() == history && !ui.dirty && deck.channelButtons[1].getToggleState(), "HC2 pad locates its own song/source while stopped, selects the matching label, and preserves document/history");
    ui.engine.playing = true; deck.refresh(true); const double at = ui.engine.position;
    deck.channelButtons[0].onClick();
    check(deck.channelButtons[0].isEnabled() && ui.engine.position == at && json(ui.engine.project) == beforeLocate
        && ui.status.containsIgnoreCase("queued"),
        "playing cue press queues its explicit target without seeking the programme or changing the document"); ui.engine.playing = false;
    ui.engine.project.clips[0] = trimmed; ui.refresh();
    check(!deck.channelButtons[0].isEnabled() && deck.channelButtons[2].isEnabled() && ui.engine.project.clips[0].hotcues == defaults,
        "Controller keeps out-of-range numbered slots visible and disables only their locate action");
    bool editOutside = false, resets = false;
    const auto cueMenu = deck.controlsMenu();
    for (juce::PopupMenu::MenuItemIterator it(cueMenu, true); it.next();)
    { if (it.getItem().itemID == 100) editOutside = it.getItem().isEnabled; if (it.getItem().itemID == 1) resets = it.getItem().isEnabled; }
    check(editOutside && resets && midiActionId({MidiActionKind::resetEq, 0}) == "reset-eq-1"
        && midiActionId({MidiActionKind::resetCutoff, 0}) == "reset-filter-1", "cue editor menu reaches disabled slots while original reset commands/MIDI identities remain available");
    ui.engine.project = base; ui.refresh(); ui.undoHistory.clear(); ui.redoHistory.clear();
    auto pump = []
    {
        const auto until = juce::Time::getMillisecondCounterHiRes() + 40;
        do {
#if JUCE_WINDOWS
            MSG m; while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageW(&m); }
#endif
            juce::Thread::sleep(1);
        } while (juce::Time::getMillisecondCounterHiRes() < until);
    };
    auto edit = [&](int slot, const juce::String& text, int answer)
    {
        deck.performControlsMenu(100 + slot, id); pump(); auto* dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        check(dialog && dialog->getName() == "Hotcue HC" + juce::String(slot + 1), "actual Controller menu opens the precise owned cue editor");
        if (dialog) { if (text.isNotEmpty()) dialog->getTextEditor("source")->setText(text); dialog->exitModalState(answer); pump(); }
    };
    edit(3, "24.125", 0); check(!hotcueFor(ui.engine.project.clips[0], 3) && ui.undoHistory.empty(), "Cancel adding a cue preserves empty slot/history");
    edit(3, "24.125", 1);
    check(hotcueFor(ui.engine.project.clips[0], 3) && hotcueFor(ui.engine.project.clips[0], 3)->sourceSeconds == 24.125 && ui.undoHistory.size() == 1,
        "actual exact Apply creates one source-owned HC4 with one Undo");
    const auto exact = json(ui.engine.project); edit(3, "", 1);
    check(json(ui.engine.project) == exact && ui.undoHistory.size() == 1, "unchanged cue text does not quantise saved precision or add Undo");
    edit(3, "bad", 1); check(json(ui.engine.project) == exact, "invalid precise cue entry preserves saved source position");
    ui.editHotcue(id, 3); pump(); auto* stale = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
    ui.engine.project.labels.push_back({juce::Uuid().toString(), 2, "Changed while cue editor open"}); const auto changed = json(ui.engine.project);
    if (stale) { stale->getTextEditor("source")->setText("28"); stale->exitModalState(1); pump(); }
    check(stale && json(ui.engine.project) == changed, "stale cue editor cannot overwrite intervening project changes");
    edit(3, "", 2); check(!hotcueFor(ui.engine.project.clips[0], 3) && ui.engine.project.clips.size() == 1, "Clear slot keeps the song/other cues and leaves a persistent tombstone");
    ui.undo(false); check(hotcueFor(ui.engine.project.clips[0], 3), "Undo restores the exact cleared cue");
    const auto beforeDefaults = ui.engine.project.clips[0].hotcues; edit(3, "", 3);
    check(ui.engine.project.clips[0].hotcues == defaults, "explicit Use section defaults replaces only this segment's nine slots");
    ui.undo(false); check(ui.engine.project.clips[0].hotcues == beforeDefaults, "Undo restores manual/deleted slots after explicit reseeding");
    ui.timeline.pixelsPerSecond = 10; ui.refresh();
    auto flags = ui.timeline.hotcueFlags(); const auto flag = std::find_if(flags.begin(), flags.end(), [](const auto& f) { return f.slot == 1; });
    check(flag != flags.end() && ui.timeline.hotcueAt(flag->bounds.getCentre()).has_value(), "actual HC2 timeline glyph exposes its own mouse hit region");
    if (flag != flags.end())
    {
        const auto mouse = juce::Desktop::getInstance().getMainMouseSource(); const auto point = flag->bounds.getCentre(); const auto dragHistory = ui.undoHistory.size();
        auto event = [&](juce::Point<float> p, bool drag)
        { return juce::MouseEvent(mouse, p, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::altModifier), 1,0,0,0,0, &ui.timeline,&ui.timeline,juce::Time::getCurrentTime(),point,juce::Time::getCurrentTime(),1,drag); };
        ui.timeline.mouseDown(event(point, false)); ui.timeline.mouseDrag(event(point.translated(10,0), true)); ui.timeline.mouseDrag(event(point.translated(20,0), true)); ui.timeline.mouseUp(event(point.translated(20,0), true));
        check(std::abs(hotcueFor(ui.engine.project.clips[0], 1)->sourceSeconds - 34) < 1.e-7 && ui.undoHistory.size() == dragHistory + 1,
            "real free cue drag uses source clock and exactly one Undo across updates");
    }
    ui.timeline.pixelsPerSecond = 40; ui.viewport.setViewPosition(0, 0); ui.setSize(1250, 940); ui.refresh();
    TimelineAccessItem* native = nullptr;
    for (auto& item : ui.timeline.accessibleItems) if (item->getComponentID().startsWith("hotcue:") && item->read().startsWith("HC2 /")) native = item.get();
    const auto nativeBefore = json(ui.engine.project); const auto nativeHistory = ui.undoHistory.size(); const auto nativeAt = ui.engine.position.load();
    const juce::Component::SafePointer<TimelineAccessItem> alive(native);
    auto* handler = native ? native->getAccessibilityHandler() : nullptr; auto* value = handler ? handler->getValueInterface() : nullptr;
    auto* parent = ui.timeline.getAccessibilityHandler(); const auto children = parent->getChildren();
    const bool discovered = handler && handler->getParent() == parent && std::find(children.begin(), children.end(), handler) != children.end();
    if (handler) handler->getActions().invoke(juce::AccessibilityActionType::focus);
    if (value && alive) { value->setValue(60); value->setValueAsString("60"); }
    check(alive && discovered && value && value->isReadOnly() && value->getCurrentValueAsString().contains("source seconds")
        && ui.timeline.selectedHotcue && ui.timeline.selectedHotcue->slot == 1 && ui.viewport.getViewPositionX() > 0
        && json(ui.engine.project) == nativeBefore && ui.undoHistory.size() == nativeHistory && ui.engine.position == nativeAt,
        "native cue is discoverable/read-only; offscreen focus reveals source without retiring identity, seeking or writing history");
    const auto segmentCount = ui.engine.project.clips.size(); ui.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey));
    check(!hotcueFor(ui.engine.project.clips[0], 1) && ui.engine.project.clips.size() == segmentCount, "Delete with selected cue clears only its slot even through the top-level keyboard route");
    ui.undo(false);
    const auto session = report.getSiblingFile("hotcues.mixworks"); auto reopened = std::make_unique<Engine>(juce::File(), false);
    const auto saved = ui.engine.saveProject(session), opened = reopened->restoreProject(session.loadFileAsString());
    check(saved.isEmpty() && opened.isEmpty() && reopened->project.clips[0].hotcues == ui.engine.project.clips[0].hotcues,
        "actual Save/Open retains hotcue identities/source positions/default/manual/tombstone data: " + saved + opened);
    auto dry = std::make_unique<Engine>(juce::File(), false); dry->project = base; dry->sampleRate = ui.engine.sampleRate = 8000;
    juce::AudioBuffer<float> a(2, 256), b(2, 256); juce::MidiBuffer midi;
    for (double t : {12., 45., 75.})
    {
        dry->render(a, 256, t, midi); ui.engine.render(b, 256, t, midi); double error = 0;
        for (int ch = 0; ch < 2; ++ch) for (int n = 0; n < 256; ++n) error = std::max(error, (double) std::abs(a.getSample(ch,n) - b.getSample(ch,n)));
        check(error == 0 && a.getRMSLevel(0,0,256) > .001, "cue annotation/navigation leaves production programme PCM unchanged at " + juce::String(t));
    }
    ui.timeline.pixelsPerSecond = 10; ui.viewport.setViewPosition(0,0);
    for (int width : {1250, 1920})
    {
        ui.setSize(width,1080); ui.refresh(); auto stream = report.getSiblingFile("hotcues-" + juce::String(width) + ".png").createOutputStream(); juce::PNGImageFormat png;
        check(stream && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *stream), "capture actual HC pads/colours/source flags at width " + juce::String(width));
    }
}
}
