#include "Workstation.h"
#include "GridCorrectionUI.h"
#include <cmath>
#include <limits>
#include <thread>
#include <chrono>
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace mixworks
{
void runAudioSettingsUiTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runAudioUiTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    auto owned = std::make_unique<Engine>(juce::File(), false); auto& engine = *owned;
    PerformanceDeck deck(engine); deck.setSize(1250, 260);
    engine.project.tracks[0].gainDb = -9; engine.project.tracks[1].mute = true;
    engine.project.tracks[2].solo = true; engine.project.masterDb = -3;
    int updates = 0;
    deck.cueSelected = [&](int slot)
    {
        ++updates;
        // Synchronously deliver a real callback from a different thread while
        // the UI is applying its snapshot. No device, timing stress or busy loop.
        std::thread callback([&]
        {
            float left[128] {}, right[128] {}; float* channels[] {left, right};
            engine.audioDeviceIOCallbackWithContext(nullptr, 0, channels, 2, 128, {});
        });
        callback.join(); return slot == 2;
    };
    const auto before = engine.callbackDiagnostics.snapshot();
    deck.refresh(true);
    const auto after = engine.callbackDiagnostics.snapshot();
    check(updates == 9 && after.callbacks - before.callbacks == 9, "live desk applies all cue states while native audio callbacks execute");
    check(after.lockMisses == before.lockMisses && after.oversized == before.oversized, "UI notification callbacks do not hold the audio engine lock or silence blocks");
    check(deck.faders[0].getValue() == -9 && deck.master.getValue() == -3 && deck.pads[9].getToggleState()
        && deck.pads[14].getToggleState() && deck.channelButtons[2].getToggleState(), "snapshot retains gain, master, mute, solo and cue selection");
    deck.setVisible(false); engine.project.tracks[0].gainDb = -12; updates = 0;
    deck.refresh(true, true);
    check(updates == 0 && deck.faders[0].getValue() == -9, "hidden periodic desk refresh does no UI or engine work");
    deck.refresh(false);
    check(deck.faders[0].getValue() == -12 && !deck.faders[0].isEnabled(), "explicit refresh still updates a hidden desk and preserves availability guards");
    juce::WaitableEvent acquired, release;
    std::thread owner([&] { const juce::ScopedLock guard(engine.lock); acquired.signal(); release.wait(2000); });
    acquired.wait(2000); updates = 0; const auto started = juce::Time::getMillisecondCounterHiRes();
    deck.refresh(true); const auto elapsed = juce::Time::getMillisecondCounterHiRes() - started;
    release.signal(); owner.join();
    check(updates == 0 && elapsed < 1000, "busy engine skips a display refresh instead of blocking the message thread");
    deck.cueSelected = {}; runAudioSettingsUiTests(check, report);
}
void runAttractTests(Workstation&, const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runPlaylistTransitionTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runMidiConnectionTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runAutoArrangementTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
// Build genuinely old filter fields for compatibility tests, rather than just changing a version number.
juce::var legacyLowPassFixture(juce::var document, int version)
{
    auto convert = [](juce::var controls)
    {
        auto* object = controls.getDynamicObject();
        object->setProperty("lowPassHz", 20000 + (double) controls["filterCutoff"] * 19980); object->removeProperty("filterCutoff");
        auto curve = controls["automation"]["cutoff"];
        for (auto& point : *curve["points"].getArray()) point.getDynamicObject()->setProperty("value", 20000 + (double) point["value"] * 19980);
        auto* lanes = controls["automation"].getDynamicObject(); lanes->setProperty("low-pass", curve); lanes->removeProperty("cutoff");
    };
    for (auto& lane : *document["tracks"].getArray()) convert(lane);
    for (auto& clip : *document["clips"].getArray()) convert(clip["controls"]);
    document.getDynamicObject()->setProperty("version", version); return document;
}
// Isolated, paced production callbacks alongside actual software UI rendering.
// No driver is opened; wake lateness is a probe scheduling observation, not an xrun.
void runAlloyTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    {
        auto ownedController = std::make_unique<Engine>(juce::File(), false);
        auto& controller = *ownedController;
        const std::array<MidiActionKind, 5> kinds {MidiActionKind::volume, MidiActionKind::bass, MidiActionKind::mid, MidiActionKind::treble, MidiActionKind::master};
        for (int i = 0; i < 5; ++i)
        {
            const AutomationAddress address {i == 4 ? -1 : 0, i == 4 ? AutomationTarget::master : (AutomationTarget) i};
            controller.project.setManualValue(address, -6);
            controller.applyMidiControl({kinds[(size_t) i], 0}, 1);
            check(controller.project.manualValue(address) == 0, "controller gain " + juce::String(i) + " first upward sweep stops at exactly 100 percent");
            controller.applyMidiControl({kinds[(size_t) i], 0}, 1);
            check(controller.project.manualValue(address) == 0, "controller gain " + juce::String(i) + " continuous messages cannot pass the unity stop");
        }
        juce::Thread::sleep(400);
        for (int i = 0; i < 5; ++i)
        {
            const AutomationAddress address {i == 4 ? -1 : 0, i == 4 ? AutomationTarget::master : (AutomationTarget) i};
            controller.applyMidiControl({kinds[(size_t) i], 0}, 1);
            check(std::abs(juce::Decibels::decibelsToGain(controller.project.manualValue(address)) - 1.3f) < 1.e-5,
                "controller gain " + juce::String(i) + " second gesture can reach the 130 percent ceiling");
        }
        controller.mappings = {{0xb0, 8, 71, {MidiActionKind::volume, 1}}};
        controller.project.tracks[1].gainDb = -9;
        controller.testMidi(juce::MidiMessage::controllerEvent(8, 71, 0));
        controller.testMidi(juce::MidiMessage::controllerEvent(8, 71, 127));
        controller.testMidi(juce::MidiMessage::controllerEvent(8, 71, 127)); controller.flushMidiControls();
        check(controller.project.tracks[1].gainDb == 0, "queued physical MIDI messages retain the first-sweep unity stop while transport is stopped");
        controller.project.tracks[1].gainDb = -6; controller.armAutomation(1, true);
        controller.setAutomationRecordMode(AutomationRecordMode::touch); controller.playing = true; controller.position = 1;
        controller.applyMidiControl({MidiActionKind::volume, 1}, 1);
        check(controller.project.tracks[1].automation[0].enabled && controller.project.tracks[1].automation[0].interpolatedValue(1, -6) == 0,
            "Touch records the snapped controller value without an overshoot point");
        controller.exporting = true; const auto before = controller.project.tracks[1].gainDb;
        check(!controller.applyMidiControl({MidiActionKind::volume, 1}, 0) && controller.project.tracks[1].gainDb == before,
            "export protection still rejects controller gain mutation");
        controller.exporting = false; controller.playing = false; controller.finishAutomation();
    }
    {
        auto ownedGenres = std::make_unique<Workstation>(false); auto& genres = *ownedGenres; genres.stopTimer(); genres.setSize(1920, 1080);
        for (const auto& name : {"Techno", "House", "", "house"})
        {
            auto media = std::make_shared<Media>(); media->name = "Genre " + juce::String((int) genres.engine.library.size());
            media->assets.genre = name; media->assets.genreRead = true; genres.engine.library.push_back(media);
        }
        const auto selected = genres.engine.library[0]->id; genres.selectedLibraryId = selected; genres.refresh();
        auto& header = genres.libraryTable.table.getHeader();
        check(header.getColumnName(8) == "GENRE", "library exposes the sortable Genre column");
        header.setSortColumnId(8, true); genres.libraryTable.sortOrderChanged(8, true);
        check(genres.visibleMedia == std::vector<int>({1, 3, 0, 2}) && genres.selectedLibraryId == selected,
            "genre sorts naturally ignoring case, keeps blanks last and preserves source selection");
        header.setSortColumnId(8, false); genres.libraryTable.sortOrderChanged(8, false);
        check(genres.visibleMedia == std::vector<int>({0, 1, 3, 2}), "descending genre sort keeps blanks last");
        genres.search.setText("house", juce::dontSendNotification); genres.filterLibrary();
        check(genres.visibleMedia.size() == 2, "genre tags participate in library search");
        genres.search.setText({}, juce::dontSendNotification); genres.filterLibrary();
        check(genres.libraryTable.getCellTooltip(3, 8).contains("No genre") && !genres.playlistTable.table.getHeader().getColumnName(8).isNotEmpty(),
            "missing genre has a clear hint and Playlist column identities stay independent");
        genres.showBrowserPage(0); genres.layout.preferredLibraryWidth = 900; genres.resized(); genres.refresh();
        auto stream = report.getSiblingFile("genre-library.png").createOutputStream(); juce::PNGImageFormat png;
        check(stream && png.writeImageToStream(genres.createComponentSnapshot(genres.getLocalBounds()), *stream), "capture native sortable Genre column");
    }
    auto auditCommandProbe = [&]
    {
        auto ownedStackFixture_ui = std::make_unique<Workstation>(false); auto& ui = *ownedStackFixture_ui; ui.stopTimer(); ui.setSize(1440, 940);
        auto command = [&](int page, const juce::String& text)
        {
            auto menu = ui.getMenuForIndex(page, {});
            for (juce::PopupMenu::MenuItemIterator it(menu, true); it.next();)
                if (it.getItem().text == text) return it.getItem().itemID;
            return 0;
        };
        juce::StringArray ids; bool unique = true;
        for (int page = 0; page < ui.getMenuBarNames().size(); ++page)
        {
            auto menu = ui.getMenuForIndex(page, {});
            for (juce::PopupMenu::MenuItemIterator it(menu, true); it.next();)
                if (const int id = it.getItem().itemID; id > 0)
                { const auto key = juce::String(id); unique &= !ids.contains(key); ids.add(key); }
        }
        check(unique, "audit: top-level and nested menu commands have unique identities");
        const int quit = command(0, "Exit Mixworks (Alt+F4)");
        auto a = std::make_shared<Media>(); a->name = "Library song A"; a->sampleRate = 8000; a->manualBpm = 120;
        a->samples.setSize(1, 8000 * 12); a->samples.clear();
        auto b = a->catalogueCopy(); b->id = juce::Uuid().toString(); b->name = "Playlist song B"; b->samples.makeCopyOf(a->samples);
        ui.engine.library = {a, b}; Clip clip; clip.media = b; clip.track = 0; clip.length = 12;
        for (int state = 0; state < 5; ++state)
        {
            ui.engine.project = {}; if (state != 0) ui.engine.project.clips = {clip};
            if (state == 4)
            {
                auto next = clip; next.id = juce::Uuid().toString(); next.start = 6; next.track = 1;
                ui.engine.project.clips.push_back(next); ui.engine.project.linkClips(clip.id, next.id);
                ui.engine.project.setTransitionLocked(next.id, true);
                check(ui.engine.project.clipTransitionLocked(clip.id), "audit: locked command fixture has an actual locked transition");
            }
            ui.dirty = state == 2; ui.engine.playing = state == 3; ui.refresh();
            const auto before = juce::JSON::toString(projectToJson(ui.engine.project)); const auto history = ui.undoHistory.size();
            const bool dirty = ui.dirty;
            for (int length = 0; length < ui.transitionLength.getNumItems(); ++length)
            {
                const int id = command(3, ui.transitionLength.getItemText(length));
                // Never execute the known colliding Quit in a red test process.
                if (id > 0 && id != quit) ui.menuItemSelected(id, 3);
                check(id > 0 && id != quit && ui.transitionIndex == length && ui.transitionLength.getSelectedId() == length + 1
                    && ui.dirty == dirty && ui.engine.playing == (state == 3) && ui.undoHistory.size() == history
                    && juce::JSON::toString(projectToJson(ui.engine.project)) == before,
                    "audit: transition default only / state " + juce::String(state) + " / " + ui.transitionLength.getItemText(length));
            }
        }
        ui.engine.playing = false; ui.engine.project = {}; ui.engine.project.clips = {clip}; ui.refresh();
        const auto original = juce::JSON::toString(projectToJson(ui.engine.project)); const auto history = ui.undoHistory.size();
        auto selectLibrary = [&]
        {
            ui.showBrowserPage(0);
            const auto row = std::find(ui.visibleMedia.begin(), ui.visibleMedia.end(), 0) - ui.visibleMedia.begin();
            ui.libraryTable.selectedRowsChanged((int) row);
        };
        auto opened = [&](const juce::String& name, const juce::String& route, const juce::String& capture = {})
        {
            bool found = false;
            if (ui.gridWindow && ui.gridWindow->isVisible())
            {
                std::function<void(juce::Component&)> visit = [&](juce::Component& c)
                {
                    if (auto* label = dynamic_cast<juce::Label*>(&c)) found |= label->getText() == "SOURCE BEAT GRID  /  " + name;
                    for (auto* child : c.getChildren()) visit(*child);
                };
                visit(*ui.gridWindow->getContentComponent());
                if (capture.isNotEmpty())
                {
                    auto stream = report.getSiblingFile(capture).createOutputStream(); juce::PNGImageFormat png;
                    check(stream && png.writeImageToStream(ui.gridWindow->getContentComponent()->createComponentSnapshot(ui.gridWindow->getContentComponent()->getLocalBounds()), *stream), "audit: capture " + route);
                }
                ui.gridWindow->closeButtonPressed();
            }
            check(found, "audit: correction identifies " + name + " via " + route);
        };
        selectLibrary(); ui.showBrowserPage(1); ui.playlistTable.selectedRowsChanged(0);
        ui.menuItemSelected(command(3, "Track BPM / key..."), 3); opened(b->name, "Playlist menu", "audit-playlist-source.png");
        selectLibrary(); ui.trackInfoButton.onClick(); opened(a->name, "library BPM/Key button");
        selectLibrary(); ui.showBrowserPage(1); ui.playlistTable.selectedRowsChanged(0);
        ui.keyPressed(juce::KeyPress('W', juce::ModifierKeys::ctrlModifier, 0)); opened(b->name, "Ctrl+W");
        selectLibrary();
        const auto point = juce::Point<float> {60, (float) ui.timeline.laneTop(clip.track) + 70};
        const auto input = juce::Desktop::getInstance().getMainMouseSource();
        const auto click = juce::MouseEvent(input, point, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),
            1, 0, 0, 0, 0, &ui.timeline, &ui.timeline, juce::Time::getCurrentTime(), point, juce::Time::getCurrentTime(), 1, false);
        ui.timeline.mouseDown(click); ui.timeline.mouseUp(click);
        ui.menuItemSelected(command(3, "Track BPM / key..."), 3); opened(b->name, "timeline click with Library still visible");
        ui.libraryTable.cellClicked(0, 1, click);
        ui.menuItemSelected(command(3, "Track BPM / key..."), 3); opened(a->name, "clicking the previously selected library row again");
        selectLibrary(); ui.selectedClip = clip.id;
        ui.performTimelineAction(TimelineAction::tweaks, 1, clip.track); opened(b->name, "explicit clip context");
        selectLibrary(); ui.menuItemSelected(command(3, "Track BPM / key..."), 3); opened(a->name, "Library menu");
        ui.showBrowserPage(1); ui.selectedClip = "removed-clip";
        ui.menuItemSelected(command(3, "Track BPM / key..."), 3);
        check(!ui.gridWindow || !ui.gridWindow->isVisible(), "audit: missing Playlist target never falls back to the hidden library song");
        if (ui.gridWindow && ui.gridWindow->isVisible()) ui.gridWindow->closeButtonPressed();
        check(juce::JSON::toString(projectToJson(ui.engine.project)) == original && ui.undoHistory.size() == history
            && a->manualBpm == 120 && b->manualBpm == 120, "audit: target review and Cancel preserve source facts, arrangement and Undo");
        const int sections = command(3, ui.transitionLength.getItemText(5)); const int previous = ui.transitionIndex;
        ui.menuItemSelected(sections, 0);
        check(ui.transitionIndex == previous, "audit: a transition-length command rejects an unrelated menu context");
        ui.busy = true; ui.menuItemSelected(command(3, ui.transitionLength.getItemText(0)), 3);
        ui.trackInfoButton.onClick();
        check(ui.transitionIndex == previous && (!ui.gridWindow || !ui.gridWindow->isVisible()), "audit: busy command/correction guards preserve the default and source facts");
        ui.busy = false;
        if (juce::JUCEApplicationBase::getCommandLineParameters().contains("--audit-interactive"))
        {
            // Owned self-test workspace: no audio device, production preferences, scan or recovery.
            ui.selectedClip = clip.id; selectLibrary(); ui.dirty = true; ui.refresh();
            struct ProbeWindow final : juce::DocumentWindow
            {
                ProbeWindow() : DocumentWindow("Mixworks A01 isolated verification", colour::background, closeButton) {}
                void closeButtonPressed() override { juce::MessageManager::getInstance()->stopDispatchLoop(); }
            } host;
            host.setUsingNativeTitleBar(true); host.setContentNonOwned(&ui, false); host.centreWithSize(1440, 940); host.setVisible(true);
            ui.closeAppButton.onClick = [] { juce::MessageManager::getInstance()->stopDispatchLoop(); };
            juce::Timer::callAfterDelay(240000, [] { juce::MessageManager::getInstance()->stopDispatchLoop(); });
            juce::MessageManager::getInstance()->runDispatchLoop();
            auto stream = report.getSiblingFile("audit-interactive-final.png").createOutputStream(); juce::PNGImageFormat png;
            check(stream && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *stream), "audit: retain isolated native workflow capture");
            host.clearContentComponent();
        }
    };
    auditCommandProbe();
    if (juce::JUCEApplicationBase::getCommandLineParameters().contains("--audit-commands-only")) return;
    auto cutoffRoutingProbe = [&]
    {
        auto owned = std::make_unique<Workstation>(false); auto& desk = *owned; desk.stopTimer(); desk.setSize(1920, 1080);
        auto& engine = desk.engine; engine.sampleRate = 48000; engine.position = 2.1; engine.playing = true; engine.project.masterDb = 0;
        auto media = std::make_shared<Media>(); media->sampleRate = 48000; media->name = "Cutoff routing tone"; media->samples.setSize(2, 96000);
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 96000; ++i) media->samples.setSample(ch, i, .2f * (float) std::sin(i * juce::MathConstants<double>::twoPi * 8000 / 48000));
        Clip old; old.media = media; old.length = .5; old.track = 0;
        old.controls.automation[(size_t) AutomationTarget::cutoff] = {true, {{0, 0}, {.5, 0}}};
        Clip audible; audible.media = media; audible.start = 2; audible.length = 1.5; audible.track = 0;
        engine.project.clips = {old, audible}; engine.library = {media}; desk.selectedClip = engine.selectedClip = old.id; desk.refresh();
        const auto originalClip = juce::JSON::toString(projectToJson(engine.project)["clips"][0]);
        auto level = [&]
        {
            engine.resetPlugins(); juce::AudioBuffer<float> output(2, 512); juce::MidiBuffer midi; double square = 0; int samples = 0;
            for (int block = 0; block < 24; ++block)
            {
                engine.render(output, 512, 2.1 + block * 512. / 48000, midi);
                if (block >= 16) for (int i = 0; i < 512; ++i) { const double v = output.getSample(0, i); square += v * v; ++samples; }
            }
            return std::sqrt(square / samples);
        };
        const double dry = level(); check(dry > .1, "cutoff routing probe has audible later audio with an earlier same-lane clip selected");
        auto reset = [&]
        {
            engine.setAutomationRecordMode(AutomationRecordMode::read);
            engine.project.clips = {old, audible}; engine.project.tracks[0].filterCutoff = 0; engine.project.tracks[0].automation[(size_t) AutomationTarget::cutoff] = {};
            desk.selectedClip = engine.selectedClip = old.id; engine.position = 2.1; desk.refresh();
        };
        for (int input = 0; input < 3; ++input)
        {
            reset();
            if (input == 0) desk.strips[0]->cutoff.setValue(1, juce::sendNotificationSync);
            else if (input == 1) desk.performance->pots[6].setValue(1, juce::sendNotificationSync);
            else
            {
                engine.mappings = {{0xb0, 16, 80, {MidiActionKind::deskPot, 6}}};
                engine.testMidi(juce::MidiMessage::controllerEvent(16, 80, 64)); engine.flushMidiControls();
                engine.testMidi(juce::MidiMessage::controllerEvent(16, 80, 127)); engine.flushMidiControls();
            }
            check(engine.project.tracks[0].filterCutoff == 1 && juce::JSON::toString(projectToJson(engine.project)["clips"][0]) == originalClip,
                "live cutoff input " + juce::String(input) + " targets the lane while preserving the old selected clip and its curve");
            check(level() < dry * .05, "live cutoff input " + juce::String(input) + " audibly attenuates the later same-lane clip");
            desk.refresh(); desk.performance->refresh(true);
            check(desk.strips[0]->cutoff.getValue() == 1 && desk.performance->pots[6].getValue() == 1,
                "both cutoff displays read the same lane owner for input " + juce::String(input));
        }
        reset();
        check(engine.setControlValue({0, AutomationTarget::cutoff, {}, {}, audible.id}, -.8f)
            && engine.project.clips[1].controls.filterCutoff == -.8f && engine.project.tracks[0].filterCutoff == 0,
            "an explicit clip cutoff address remains source-owned and independent of live lane controls");
        desk.selectedClip = engine.selectedClip = audible.id; desk.refresh();
        const auto beforeReset = juce::JSON::toString(projectToJson(engine.project));
        desk.menuItemSelected(88, 1);
        check(engine.project.clips[1].controls.filterCutoff == 0 && engine.project.tracks[0].filterCutoff == 0,
            "Edit offers an explicit clip-only cutoff reset for retained legacy settings");
        desk.undo(false); check(juce::JSON::toString(projectToJson(engine.project)) == beforeReset, "clip-only cutoff reset is fully undoable");
        reset(); engine.setAutomationRecordMode(AutomationRecordMode::touch); engine.armAutomation(0, true);
        desk.performance->pots[6].setValue(.8, juce::sendNotificationSync); engine.finishAutomation();
        check(engine.project.tracks[0].automation[(size_t) AutomationTarget::cutoff].active()
            && juce::JSON::toString(projectToJson(engine.project)["clips"][0]) == originalClip,
            "recording a live cutoff writes lane automation even when an ended clip remains selected");
        engine.setAutomationRecordMode(AutomationRecordMode::read); engine.playing = false;
    };
    cutoffRoutingProbe();
    if (juce::JUCEApplicationBase::getCommandLineParameters().contains("--cutoff-route-only")) return;
    auto runPlaybackBudgetProbe = [&]
    {
    AudioCallbackDiagnostics known; known.record(10, 20); known.record(30, 20); known.record(20, 20);
    const auto measured = known.snapshot();
    check(measured.callbacks == 3 && measured.overBudget == 1 && measured.peakTicks == 30 && measured.totalTicks == 60,
        "callback diagnostics distinguish work over budget from an exact-budget block");
    auto desk = std::make_unique<Workstation>(false); desk->stopTimer();
    auto& engine = desk->engine; engine.sampleRate = 48000;
    auto media = std::make_shared<Media>(); media->name = "P10 synthetic overlap"; media->sampleRate = 48000;
    media->manualBpm = 120; media->manualBeatOrigin = 0; media->samples.setSize(2, 48000 * 8);
    for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < media->frames(); ++i)
        media->samples.setSample(ch, i, .08f * (float) std::sin(i * (.04 + .01 * ch)));
    media->peaks.assign(4000, .08f); media->prepareWaveformBody();
    engine.project.mastering.saturation = true; engine.project.mastering.saturationDriveDb = 2;
    for (int lane = 0; lane < trackCount; ++lane)
    {
        Clip clip; clip.media = media; clip.start = 0; clip.length = 8; clip.track = lane;
        clip.introAnchor = clip.fadeInAnchor = 0; clip.outroAnchor = 6; clip.fadeOutAnchor = 8;
        clip.controls.eqDb = {2, -1, 1}; clip.controls.filterCutoff = -.1f;
        engine.project.clips.push_back(clip);
    }
    engine.prepareClipProcessing(); desk->timeline.pixelsPerSecond = 130; desk->setSize(1920, 1080); desk->refresh();
    juce::AudioBuffer<float> output(2, 16384); auto* const* channels = output.getArrayOfWritePointers();
    engine.setPlaying(true); const auto clock = engine.position.load();
    {
        const juce::ScopedLock hold(engine.lock);
        std::thread skipped([&] { engine.audioDeviceIOCallbackWithContext(nullptr, 0, channels, 2, 128, {}); }); skipped.join();
    }
    check(engine.callbackDiagnostics.snapshot().lockMisses == 1 && engine.position == clock && output.getMagnitude(0, 128) == 0,
        "contended callback records its silent skip without blocking or advancing playback");
    engine.audioDeviceIOCallbackWithContext(nullptr, 0, channels, 2, 16384, {});
    engine.exporting = true; engine.audioDeviceIOCallbackWithContext(nullptr, 0, channels, 2, 128, {}); engine.exporting = false;
    const auto exits = engine.callbackDiagnostics.snapshot();
    check(exits.callbacks == 3 && exits.oversized == 1 && exits.exportSilence == 1 && engine.position == clock,
        "callback size rejection and intentional export silence have distinct lifetime counters");
    engine.playing = false;
    juce::MidiBuffer midi; engine.render(output, 128, 0, midi);
    check(engine.callbackDiagnostics.snapshot().callbacks == 3, "offline render never contributes to device callback diagnostics");
    desk->settingsButton.onClick();
    auto* health = dynamic_cast<juce::Label*>(desk->settingsWindow->getContentComponent()->findChildWithID("audio-callback-health"));
    check(health && health->getText().contains("Lock skips: 1") && health->getText().contains("size skips: 1")
        && health->getTooltip().contains("not driver underrun"), "Audio / MIDI exposes counters with their measurement limits");
    {
        auto image = desk->settingsWindow->getContentComponent()->createComponentSnapshot(desk->settingsWindow->getContentComponent()->getLocalBounds());
        auto stream = report.getSiblingFile("p10-audio-health.png").createOutputStream(); juce::PNGImageFormat png;
        check(stream && png.writeImageToStream(image, *stream), "capture native callback-health settings");
    }
    desk->settingsWindow.reset();
    juce::Array<juce::var> scenarios;
    using Clock = std::chrono::steady_clock;
    const auto quantile = [](std::vector<double> values, double q)
    { std::sort(values.begin(), values.end()); return values.empty() ? 0.0 : values[(size_t) ((values.size() - 1) * q)]; };
    std::vector<float> reference;
    for (int load = 0; load < 2; ++load) for (int frames : {128, 512}) for (int mode = 0; mode < 3; ++mode)
    {
        desk->layout.forgeEffects = mode != 0; desk->layout.forgeMotion = mode == 2 ? 1 : 2;
        desk->attractState.enabled = mode != 0;
        desk->attractState.motion = mode == 2 ? AttractMotion::animated : AttractMotion::steady;
        desk->attractLights.syncMode(); desk->performance->setForgeEffects(desk->forgeMotionAllowed());
        engine.seek(0); engine.setPlaying(true);
        engine.audioDeviceIOCallbackWithContext(nullptr, 0, channels, 2, frames, {});
        if (mode == 0) reference.assign(output.getReadPointer(0), output.getReadPointer(0) + frames);
        else check(std::equal(reference.begin(), reference.end(), output.getReadPointer(0)), "visual mode preserves exact callback PCM");
        const auto before = engine.callbackDiagnostics.snapshot();
        std::atomic<bool> done {false};
        std::thread background;
        if (load) background = std::thread([&] { volatile double result = 0; while (!done.load()) result = std::sin(result + .1); });
        const int blocks = (int) std::ceil(1.2 * 48000 / frames);
        std::vector<double> work((size_t) blocks), late((size_t) blocks), paints;
        std::thread audio([&]
        {
            const auto start = Clock::now();
            for (int block = 0; block < blocks; ++block)
            {
                const auto target = start + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>((double) block * frames / 48000));
                std::this_thread::sleep_until(target); const auto began = Clock::now();
                late[(size_t) block] = std::chrono::duration<double, std::milli>(began - target).count();
                engine.audioDeviceIOCallbackWithContext(nullptr, 0, channels, 2, frames, {});
                work[(size_t) block] = std::chrono::duration<double, std::milli>(Clock::now() - began).count();
            }
            done = true;
        });
        auto nextPaint = Clock::now();
        while (!done.load())
        {
            const auto began = Clock::now(); desk->advancePeakMeter();
            const auto image = desk->createComponentSnapshot(desk->getLocalBounds()); juce::ignoreUnused(image);
            paints.push_back(std::chrono::duration<double, std::milli>(Clock::now() - began).count());
            nextPaint += std::chrono::milliseconds(40); std::this_thread::sleep_until(nextPaint);
        }
        audio.join(); if (background.joinable()) background.join(); engine.playing = false;
        const auto after = engine.callbackDiagnostics.snapshot(); auto* row = new juce::DynamicObject();
        row->setProperty("mode", mode == 0 ? "off" : mode == 1 ? "steady" : "animated"); row->setProperty("busyWorkers", load);
        row->setProperty("sampleRate", 48000); row->setProperty("frames", frames); row->setProperty("blockBudgetMs", 1000.0 * frames / 48000);
        row->setProperty("callbacks", (juce::int64) (after.callbacks - before.callbacks));
        row->setProperty("overBudget", (juce::int64) (after.overBudget - before.overBudget));
        row->setProperty("lockSkips", (juce::int64) (after.lockMisses - before.lockMisses));
        row->setProperty("workP95Ms", quantile(work, .95)); row->setProperty("workMaxMs", quantile(work, 1));
        row->setProperty("wakeLateP95Ms", quantile(late, .95)); row->setProperty("wakeLateMaxMs", quantile(late, 1));
        row->setProperty("paintP95Ms", quantile(paints, .95)); row->setProperty("paintMaxMs", quantile(paints, 1));
        scenarios.add(juce::var(row));
        check(after.callbacks - before.callbacks == (uint64_t) blocks && after.lockMisses == before.lockMisses
            && after.oversized == before.oversized && output.getMagnitude(0, frames) > .001f,
            "paced overlap callback and native paint probe completes: " + juce::String(load) + "/" + juce::String(frames) + "/" + juce::String(mode));
    }
    check(report.getSiblingFile("p10-playback-budget.json").replaceWithText(juce::JSON::toString(juce::var(scenarios))),
        "retain matched playback/paint timings without treating scheduler lateness as driver xruns");
    desk->attractState.enabled = false; desk->attractLights.syncMode(); desk->workspaceDirty = false;
    };
    // Deliberate CPU saturation and repeated full-window rendering are opt-in.
    // Routine interaction checks must not disrupt the user's other applications.
    if (juce::SystemStats::getEnvironmentVariable("MIXWORKS_TEST_STRESS", "0") == "1")
        runPlaybackBudgetProbe();
    runPlaylistTransitionTests(check, report);
    runMidiConnectionTests(check, report);
    runAutoArrangementTests(check, report);
    {
        auto energy = [](double frequency, float setting)
        {
            juce::AudioBuffer<float> signal(2, 24000);
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < signal.getNumSamples(); ++i)
                signal.setSample(ch, i, .2f * (float) std::sin(juce::MathConstants<double>::twoPi * frequency * i / 48000));
            CutoffFilterProcessor filter; filter.process(signal, 0, signal.getNumSamples(), 48000, setting);
            double sum = 0; for (int i = 4800; i < signal.getNumSamples(); ++i) sum += signal.getSample(0, i) * signal.getSample(0, i);
            return sum;
        };
        const double highDry = energy(8000, 0), lowDry = energy(80, 0);
        check(highDry > 380 && lowDry > 380, "DJ cutoff centre is unfiltered at low and high frequencies");
        check(energy(8000, -.96f) < highDry * .001, "DJ cutoff left rejects high frequencies");
        check(energy(80, .04f) < lowDry * .001 && energy(8000, .04f) > highDry * .9,
            "DJ cutoff right rejects bass and retains treble");
        auto centre = std::make_unique<Workstation>(false); centre->stopTimer();
        check(centre->strips[0]->cutoff.getMinimum() == -1 && centre->strips[0]->cutoff.getMaximum() == 1
            && centre->strips[0]->cutoff.getValue() == 0, "native cutoff dial has a neutral centre and two filter directions");
        auto& dial = centre->strips[0]->cutoff;
        check(dial.valueToProportionOfLength(0) == .5 && std::abs(dial.proportionOfLengthToValue(.25) - cutoffFromPosition(-.5)) < 1e-6
            && dial.snapValue(cutoffFromPosition(.01), juce::Slider::absoluteDrag) == 0, "native cutoff uses logarithmic halves and a centre detent");
        float parsed = 0;
        check(parseCutoffText("LP 800 Hz", parsed) && std::abs(parsed - lowPassToCutoff(800)) < 1e-6
            && parseCutoffText("HP 1.2k", parsed) && parsed > 0 && parseCutoffText("Off", parsed) && parsed == 0
            && !parseCutoffText("HP garbage", parsed) && !parseCutoffText("LP 20001", parsed), "cutoff text accepts explicit filter side/frequency and rejects invalid input");
        centre->engine.setControlValue({0, AutomationTarget::cutoff}, .4f); centre->refresh();
        dial.keyPressed(juce::KeyPress(juce::KeyPress::homeKey));
        check(centre->engine.project.tracks[0].filterCutoff == 0, "Home resets the production cutoff owner to exact neutral");
        for (int cc : {0, 63, 64, 127})
        {
            centre->engine.applyMidiControl({MidiActionKind::cutoff, 0}, cc / 127.f);
            check(centre->engine.project.tracks[0].filterCutoff == (cc == 0 ? -1 : cc == 127 ? 1 : 0), "MIDI cutoff endpoint/neutral CC " + juce::String(cc));
        }
        centre->setSize(1440, 940);
        for (const auto value : {0.f, lowPassToCutoff(800), .04f})
        {
            centre->engine.setControlValue({0, AutomationTarget::cutoff}, value); centre->refresh();
            auto stream = report.getSiblingFile(value == 0 ? "cutoff-centre.png" : value < 0 ? "cutoff-lp.png" : "cutoff-hp.png").createOutputStream(); juce::PNGImageFormat png;
            check(stream && png.writeImageToStream(centre->performance->createComponentSnapshot(centre->performance->getLocalBounds()), *stream), "capture actual cutoff desk direction " + cutoffText(value));
        }
    }
    {
        auto ownedStackFixture_keys = std::make_unique<Workstation>(false); auto& keys = *ownedStackFixture_keys; keys.stopTimer();
        const juce::StringArray tonics {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
        bool compact = true;
        for (int key = 0; key < 24; ++key)
            compact &= keyCompactName(key) == tonics[key % 12] + (key >= 12 ? "m" : "")
                && keyNoteName(key) == tonics[key % 12] + (key >= 12 ? " minor" : " major");
        check(compact && keyCompactName(18, false) == "F#m" && keyCompactName(6, false) == "F#",
            "all 24 compact key labels preserve tonic and major/minor identity; descriptive report names remain unchanged");
        check(keyCompactName(-1) == "Unknown" && keyCompactName(24) == "Unknown", "compact keys preserve unknown and invalid identities");
        for (const int key : {18, -1, 6, 0, 24})
        {
            auto media = std::make_shared<Media>(); media->name = "Key fixture " + juce::String(key);
            media->analysis.key = key; media->analysis.keyConfidence = .8;
            keys.engine.library.push_back(media);
        }
        keys.engine.project.keyDisplay = 0;
        const auto originalAnalysis = keys.engine.library[0]->analysis;
        check(keys.keyText(*keys.engine.library[0]) == "Gbm" && keys.keyText(*keys.engine.library[2]) == "Gb",
            "library and timeline shared display uses compact keys in note mode");
        keys.engine.library[0]->analysis.keyConfidence = .1;
        check(keys.keyText(*keys.engine.library[0]) == "Gbm", "compact labels keep uncertainty in the adjacent confidence column");
        keys.engine.library[0]->manualKey = 6;
        check(keys.keyText(*keys.engine.library[0]) == "Gb", "manual key overrides retain precedence without changing analysis");
        keys.engine.library[0]->manualKey = -1; keys.engine.library[0]->analysis = originalAnalysis;
        keys.search.setText("gbm", false); keys.filterLibrary();
        check(keys.visibleMedia == std::vector<int>{0}, "case-insensitive library search finds the displayed compact minor key");
        keys.search.setText("Gb minor", false); keys.filterLibrary();
        check(keys.visibleMedia == std::vector<int>{0}, "descriptive key search remains available");
        keys.search.setText({}, false); keys.librarySortColumn = 3; keys.librarySortForward = true; keys.filterLibrary();
        check(keys.visibleMedia == std::vector<int>({3, 2, 0, 1, 4}), "note sorting preserves pitch identity and keeps unknown keys last");
        const auto tooltip = keys.libraryTable.getCellTooltip(2, 3);
        check(tooltip.contains("Gb minor") && tooltip.contains("Check it by ear"), "key-cell hint gives a short listening action");
        keys.keyDisplay.setSelectedId(2, juce::sendNotificationSync);
        check(keys.visibleMedia == std::vector<int>({2, 3, 0, 1, 4}) && keys.keyText(*keys.engine.library[0]) == "11A",
            "switching to Camelot immediately re-sorts the key column using its displayed wheel identity");
        keys.keyDisplay.setSelectedId(3, juce::sendNotificationSync);
        check(keys.keyText(*keys.engine.library[0]) == "11A / Gbm", "combined key mode uses compact notes alongside unchanged Camelot values");
        keys.keyDisplay.setSelectedId(1, juce::sendNotificationSync);
        keys.librarySortForward = false; keys.filterLibrary();
        check(keys.visibleMedia == std::vector<int>({0, 2, 3, 1, 4}) && keys.engine.library[0]->analysis.key == 18
            && keys.engine.library[0]->manualKey == -1 && keys.engine.library[0]->analysis.keyConfidence == .8,
            "descending key order keeps unknowns last and display changes preserve detector evidence and overrides");
        keys.setSize(1440, 940); keys.refresh();
        for (int mode : {1, 3})
        {
            keys.keyDisplay.setSelectedId(mode, juce::sendNotificationSync);
            auto out = report.getSiblingFile(mode == 1 ? "keys-compact.png" : "keys-combined.png").createOutputStream();
            juce::PNGImageFormat png;
            check(out && png.writeImageToStream(keys.libraryTable.table.createComponentSnapshot(keys.libraryTable.table.getLocalBounds()), *out),
                "capture compact-key library labels in note and combined modes");
        }
    }
    {
        auto evidenceOwner = std::make_unique<Workstation>(false); auto& evidence = *evidenceOwner; evidence.stopTimer(); evidence.setSize(1920, 1000);
        {
            const double bpms[] {128,129,126,140,0}; const int keys[] {0,7,21,6,-1};
            for (int i = 0; i < 5; ++i)
            { auto m = std::make_shared<Media>(); m->name = juce::String::charToString((juce::juce_wchar) ('A'+i));
              m->manualBpm = bpms[i]; m->manualKey = keys[i]; evidence.engine.library.push_back(m); }
            evidence.filterLibrary(); evidence.libraryTable.table.selectRow(0); evidence.libraryTable.selectedRowsChanged(0);
            const auto source = evidence.selectedLibraryId;
            evidence.libraryTable.sortOrderChanged(4,true);
            check(evidence.visibleMedia == std::vector<int>({0,1,2,3,4}) && evidence.selectedLibraryId == source,
                "AUDIT: BPM header uses highlighted library source as nearest-distance baseline and retains selection");
            evidence.libraryTable.sortOrderChanged(4,false);
            check(evidence.visibleMedia == std::vector<int>({0,3,2,1,4}),"AUDIT: reverse BPM similarity pins baseline and keeps unknowns last");
            evidence.libraryTable.sortOrderChanged(3,true);
            check(evidence.visibleMedia == std::vector<int>({0,1,2,3,4}),"AUDIT: key similarity ranks adjacent/relative Camelot keys before distant keys");
            evidence.libraryTable.table.selectRow(2); evidence.libraryTable.selectedRowsChanged(2); evidence.libraryTable.sortOrderChanged(4,true);
            check(evidence.visibleMedia.front() == 2 && evidence.librarySortReferenceId == evidence.engine.library[2]->id,
                "AUDIT: next header click captures the newly highlighted reference rather than the timeline selection");
            evidence.engine.library.clear(); evidence.selectedLibraryId.clear(); evidence.librarySortReferenceId.clear(); evidence.librarySortColumn = 0; evidence.filterLibrary();
            evidence.resizeSidePanel(true,350); const int width = evidence.layout.library.getWidth(), before = evidence.viewport.getViewWidth();
            evidence.leftSplitter.keyPressed(juce::KeyPress(juce::KeyPress::endKey));
            check(evidence.layout.libraryCollapsed && evidence.viewport.getViewWidth() == before + width && !evidence.libraryTable.table.isVisible()
                && !evidence.search.isVisible() && evidence.leftSplitter.isVisible(),"AUDIT: left collapse reclaims width and hides library controls while preserving its restore grip");
            const auto preferences = report.getSiblingFile("audit-workspace.json"); WorkspaceLayout loaded;
            check(writeWorkspacePreferences(preferences,evidence.layout).wasOk() && readWorkspacePreferences(preferences,loaded).wasOk()
                && loaded.libraryCollapsed && loaded.preferredLibraryWidth == 350,"AUDIT: library collapsed state and expanded width survive preferences roundtrip");
            evidence.rightSplitter.keyPressed(juce::KeyPress(juce::KeyPress::endKey));
            for (int size : {1250,1920})
            {
                evidence.setSize(size,size == 1250 ? 900 : 1080);
                check(evidence.layout.library.getWidth() == 0 && evidence.layout.master.getWidth() == 0
                    && evidence.leftSplitter.isVisible() && evidence.rightSplitter.isVisible(),"AUDIT: both panels can collapse at compact/wide sizes");
                auto out = report.getSiblingFile("audit-collapsed-" + juce::String(size) + ".png").createOutputStream(); juce::PNGImageFormat png;
                check(out && png.writeImageToStream(evidence.createComponentSnapshot(evidence.getLocalBounds()),*out),"AUDIT: capture both collapsed panels / " + juce::String(size));
            }
            evidence.setSize(1920,1080); evidence.rightSplitter.keyPressed(juce::KeyPress(juce::KeyPress::returnKey));
            evidence.leftSplitter.keyPressed(juce::KeyPress(juce::KeyPress::returnKey));
            check(!evidence.layout.libraryCollapsed && evidence.layout.library.getWidth() == width && evidence.libraryTable.table.isVisible(),
                "AUDIT: Enter restores exact left width and active browser controls");
            auto legacy = juce::JSON::parse(preferences); legacy.getDynamicObject()->removeProperty("libraryCollapsed"); preferences.replaceWithText(juce::JSON::toString(legacy));
            check(readWorkspacePreferences(preferences,loaded).wasOk() && !loaded.libraryCollapsed,"AUDIT: older preferences default library to expanded");
            legacy.getDynamicObject()->setProperty("libraryCollapsed",1); preferences.replaceWithText(juce::JSON::toString(legacy));
            check(readWorkspacePreferences(preferences,loaded).failed(),"AUDIT: malformed library-collapse preference rejects atomically");
            check(evidence.transitionPreset.getItemText(0).contains("NONE") && evidence.selectedTransitionPreset() == TransitionPreset::crossfade,
                "AUDIT: new-track picker exposes NONE and preserves existing default");
        }
        const std::vector<double> scores {0, .049999, .05, .149999, .15, .299999, .30, 1,
            std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -.1, 1.01};
        const int expected[] {1, 1, 2, 2, 3, 3, 4, 4, 0, 0, 0, 0};
        for (size_t n = 0; n < scores.size(); ++n)
        {
            Media media; media.analysis.key = 18; media.analysis.keyConfidence = scores[n];
            check(Workstation::keyEvidenceLevel(media) == expected[n], "key evidence handles score boundary / invalid case " + juce::String((int) n));
            check(!Workstation::keyEvidenceHint(media).contains("%") && !Workstation::keyEvidenceHint(media).contains("evidence score"), "key help uses a plain status instead of raw scores " + juce::String((int) n));
        }
        Media missing; missing.analysis.keyConfidence = 1;
        check(Workstation::keyEvidenceText(missing) == "Unknown", "a strong stored score cannot invent a missing key");
        missing.analysis.key = 24;
        check(Workstation::keyEvidenceLevel(missing) == 0, "out-of-range key remains explicitly unknown");
        missing.manualKey = 6;
        check(Workstation::keyEvidenceText(missing) == "Manual" && Workstation::keyEvidenceHint(missing).contains("Your chosen key"), "manual override has a plain source label");
        missing.manualKey = -1; missing.analysis.key = 6; missing.analysis.keyConfidence = std::numeric_limits<double>::quiet_NaN();
        check(Workstation::keyEvidenceText(missing) == "Review" && !Workstation::keyEvidenceHint(missing).contains("nan"), "invalid score asks for review without technical detail");
        for (int n = 0; n < 5; ++n)
        {
            auto media = std::make_shared<Media>(); media->name = "Evidence " + juce::String(n); media->analysis.key = n == 0 ? -1 : 18;
            media->analysis.keyConfidence = n == 2 ? .55 : n == 3 ? .8 : .1;
            if (n == 4) media->manualKey = 6;
            evidence.engine.library.push_back(media);
        }
        const int originalKeyMode = evidence.engine.project.keyDisplay;
        const auto before = juce::JSON::toString(projectToJson(evidence.engine.project));
        evidence.refresh(); evidence.libraryTable.table.selectRow(2);
        const auto selected = evidence.selectedLibraryId;
        evidence.libraryTable.sortOrderChanged(7, false);
        check(evidence.visibleMedia == std::vector<int>({3, 2, 1, 0, 4}) && evidence.selectedLibraryId == selected
            && evidence.libraryTable.table.getSelectedRow() == 1, "descending confidence sorting preserves row identity and keeps manual keys unranked last");
        evidence.libraryTable.sortOrderChanged(7, true);
        check(evidence.visibleMedia == std::vector<int>({1, 2, 3, 0, 4}) && evidence.libraryTable.table.getSelectedRow() == 1,
            "ascending confidence order matches numbered levels without ranking manual correction as excellent");
        auto& header = evidence.libraryTable.table.getHeader();
        check(header.getIndexOfColumnId(7, true) == header.getIndexOfColumnId(2, true) + 1, "confidence follows Time in the requested library order");
        check(evidence.libraryTable.getCellTooltip(0, 7).contains("Suggested key") && evidence.libraryTable.getCellTooltip(1, 7).contains("Key found")
            && evidence.libraryTable.getCellTooltip(4, 3).contains("Your chosen key"), "key cells give short review guidance and manual origin");
        for (int mode : {0, 1, 2})
        {
            evidence.engine.project.keyDisplay = mode;
            check(!evidence.keyText(*evidence.engine.library[1]).contains("?") && evidence.keyText(*evidence.engine.library[0]) == "Unknown",
                "all tracklist/timeline key modes remove the suffix and retain explicit Unknown " + juce::String(mode));
        }
        evidence.engine.project.keyDisplay = originalKeyMode;
        check(before == juce::JSON::toString(projectToJson(evidence.engine.project)) && evidence.undoHistory.empty() && !evidence.dirty
            && evidence.engine.library[1]->analysis.keyConfidence == .1 && evidence.engine.library[4]->manualKey == 6,
            "confidence display and sorting preserve musical state, Undo, source score and overrides");
        evidence.engine.project.keyDisplay = 0;
        for (int width : {1440, 1920})
        {
            evidence.layout.preferredLibraryWidth = width == 1920 ? 560 : WorkspaceLayout::defaultLibraryWidth;
            evidence.setSize(width, 1000); evidence.resized(); evidence.refresh();
            auto out = report.getSiblingFile("key-confidence-" + juce::String(width) + ".png").createOutputStream(); juce::PNGImageFormat png;
            check(out && png.writeImageToStream(evidence.createComponentSnapshot(evidence.getLocalBounds()), *out), "capture actual key evidence workflow at width " + juce::String(width));
        }
    }
    {
        auto ownedStackFixture_labels = std::make_unique<Workstation>(false); auto& labels = *ownedStackFixture_labels; labels.stopTimer();
        for (const auto target : {AutomationTarget::volume, AutomationTarget::bass, AutomationTarget::mid, AutomationTarget::treble})
        {
            labels.selectAutomationTarget((int) target);
            check(labels.automationText(target, 0) == "100%" && labels.automationText(target, -60) == "0%"
                && labels.automationText(target, -6.0206f) == "50%" && labels.automationText(target, 6.0206f) == "200%", "automation node levels use percentage gain instead of fixed frequency labels or dB");
        }
        labels.selectAutomationTarget((int) AutomationTarget::cutoff);
        check(labels.automationText(AutomationTarget::cutoff, 0) == "Off" && labels.automationText(AutomationTarget::cutoff, -1) == "LP 20"
            && labels.automationText(AutomationTarget::cutoff, 1) == "HP 20.0k", "cutoff node captions distinguish LP, neutral and HP");
    }
    const auto root = report.getParentDirectory().getChildFile("alloy-fixture-" + juce::Uuid().toString()); root.createDirectory();
    const auto preference = root.getChildFile("workspace.json"); WorkspaceLayout settings;
    check(trackCount == 4, "the mixer exposes exactly four real lanes");
    check(!settings.followPlayhead, "Follow playhead defaults off for an existing workspace");
    const auto evenLanes = [](const WorkspaceLayout& layout)
    {
        int low = layout.laneSize(0), high = low;
        for (int lane = 1; lane < trackCount; ++lane)
        { low = std::min(low, layout.laneSize(lane)); high = std::max(high, layout.laneSize(lane)); }
        return high - low <= 1 && layout.laneTop(trackCount) == layout.timeline.getHeight() - WorkspaceLayout::timelineScrollbarThickness;
    };
    {
        WorkspaceLayout fitted; fitted.preferredTimelineHeight = 800; fitted.update(1920, 1440, false);
        fitted.resizeLane(3, 450); const auto custom = fitted.laneScale;
        fitted.update(1800, 1440, false);
        check(fitted.laneScale == custom && fitted.laneSize(3) > fitted.laneSize(0),
            "width-only layout refresh retains an individually resized lane");
        bool even = true;
        for (const int height : {600, 1001, 445, 800, 414, 1005, 600})
        {
            fitted.preferredTimelineHeight = height; fitted.update(1920, 1440, false);
            even &= evenLanes(fitted);
        }
        check(even, "overall timeline expansion and compression replace unequal saved shares with four equal lanes without accumulated rounding");
        fitted.resizeLane(3, 250); fitted.preferredTimelineHeight = 0;
        fitted.update(1920, 1080, false); const bool compressed = evenLanes(fitted);
        fitted.update(1920, 1440, false);
        check(compressed && evenLanes(fitted), "window height changes resize all four lanes evenly");
    }
    {
        WorkspaceLayout fitted; fitted.preferredTimelineHeight = 1; fitted.update(1250, 800, true);
        check(fitted.timeline.getHeight() >= 4 * 100 + WorkspaceLayout::timelineScrollbarThickness,
            "a docked timeline cannot shrink below four fully controlled lanes and its horizontal scrollbar");
    }
    {
        StereoPeakFeed feed; juce::AudioBuffer<float> audio(2, 64); audio.clear();
        audio.setSample(0, 17, -.8f); audio.setSample(1, 33, .2f); feed.push(audio, 64);
        audio.clear(); audio.setSample(0, 8, .1f); audio.setSample(1, 12, -.6f); feed.push(audio, 64);
        const auto peaks = feed.take();
        check(peaks[0] == .8f && peaks[1] == .6f && feed.take() == std::array<float, 2>{0, 0},
            "stereo peak feed preserves independent transient maxima across callbacks and consumes them once");
        PeakMeterDisplay display; display.advance({1.1f, .25f}, .04, false);
        check(display.clipped && display.db[0] > 0 && std::abs(display.db[1] + 12.0412f) < .001f && display.sparkSeconds[0] == 0,
            "meter reports sample dBFS per channel and clipping independently of optional motion");
        const auto held = display.hold; display.advance({0, 0}, .25, true);
        check(display.hold == held && display.db[1] < held[1] && display.db[1] > PeakMeterDisplay::floorDb,
            "peak hold survives a quiet interval while the displayed level falls");
        display.resetPeaks(); check(!display.clipped && display.hold == display.db, "peak reset clears clipping and held peaks without changing measured levels");
        for (int i = 0; i < 40; ++i) display.advance({0, 0}, .25, false);
        check(display.db == std::array<float, 2>{-48, -48} && display.hold == display.db, "stopped output falls to silence without retaining stale meter levels");
        display.advance({std::numeric_limits<float>::quiet_NaN(), -1}, std::numeric_limits<double>::infinity(), true);
        check(display.db == std::array<float, 2>{-48, -48}, "nonfinite display inputs cannot contaminate meter state");
        const auto cold = AlloyLookAndFeel::metalHeat(0), warm = AlloyLookAndFeel::metalHeat(.6f), hot = AlloyLookAndFeel::metalHeat(1);
        check(cold.getPerceivedBrightness() < warm.getPerceivedBrightness() && warm.getPerceivedBrightness() < hot.getPerceivedBrightness(),
            "shared metal progresses from copper to orange to a pale molten core as controls approach maximum");
    }
    check(readWorkspacePreferences(preference, settings).wasOk() && !preference.exists(), "missing workspace preferences leave defaults without writing");
   #if JUCE_WINDOWS
    std::unique_ptr<juce::AudioIODeviceType> asio(juce::AudioIODeviceType::createAudioIODeviceType_ASIO());
    if (asio) asio->scanForDevices();
    check(asio && asio->getTypeName() == "ASIO", "Windows build includes the native ASIO backend; installed drivers: " + (asio ? asio->getDeviceNames().joinIntoString(", ") : juce::String("unavailable")));
   #endif
    settings.followPlayhead = true;
    settings.preferredLibraryWidth = 380; settings.preferredInspectorWidth = 290;
    check(writeWorkspacePreferences(preference, settings).wasOk(), "workspace preferences write an identified local record");
    auto doc = juce::JSON::parse(preference); doc.getDynamicObject()->setProperty("futureSetting", "keep me"); preference.replaceWithText(juce::JSON::toString(doc));
    const auto before = preference.loadFileAsString(); settings.preferredLibraryWidth = 420;
    check(writeWorkspacePreferences(preference, settings).wasOk() && preference.getSiblingFile("workspace.json.previous").loadFileAsString() == before
          && juce::JSON::parse(preference)["futureSetting"].toString() == "keep me", "width changes preserve unknown settings and create a backup");
    const auto modified = preference.getLastModificationTime();
    check(writeWorkspacePreferences(preference, settings).wasOk() && preference.getLastModificationTime() == modified, "unchanged workspace preferences are idempotent");
    WorkspaceLayout reopened; check(readWorkspacePreferences(preference, reopened).wasOk() && reopened.preferredLibraryWidth == 420 && reopened.preferredInspectorWidth == 290 && reopened.followPlayhead, "panel widths and Follow playhead reopen independently of a musical session");
    for (const auto& text : {juce::String("broken json"), juce::String("{\"format\":\"mixworks-workspace\",\"version\":1,\"libraryWidth\":320,\"inspectorWidth\":270,\"monitorMuted\":\"yes\"}"), juce::String("{\"format\":\"mixworks-workspace\",\"version\":2,\"libraryWidth\":320,\"inspectorWidth\":270}"), juce::String("{\"format\":\"mixworks-workspace\",\"version\":1,\"libraryWidth\":\"320\",\"inspectorWidth\":270}")})
    {
        const auto invalid = root.getChildFile("invalid.json"); invalid.replaceWithText(text); WorkspaceLayout defaults;
        check(readWorkspacePreferences(invalid, defaults).failed() && writeWorkspacePreferences(invalid, settings).failed() && invalid.loadFileAsString() == text
              && defaults.preferredLibraryWidth == 320, "malformed, newer or wrongly typed workspace data is retained");
    }
    for (const auto& sizes : {juce::String("[1,1,1]"), juce::String("[1,1,1,3]"), juce::String("[1,1,1,\"large\"]")})
    {
        const auto invalid = root.getChildFile("invalid-lanes.json"); auto bad = juce::JSON::parse(preference);
        bad.getDynamicObject()->setProperty("laneScales", juce::JSON::parse(sizes)); invalid.replaceWithText(juce::JSON::toString(bad));
        const auto kept = invalid.loadFileAsString(); WorkspaceLayout untouched;
        check(readWorkspacePreferences(invalid, untouched).failed() && writeWorkspacePreferences(invalid, settings).failed()
              && untouched.laneScale == WorkspaceLayout().laneScale && invalid.loadFileAsString() == kept,
              "malformed lane size arrays are retained without changing workspace defaults");
    }
    {
        auto old = juce::JSON::parse(preference);
        for (int i = 0; i < 4; ++i) old["laneScales"].getArray()->add(.5);
        old.getDynamicObject()->removeProperty("timelineHeight"); old.getDynamicObject()->setProperty("visibleLanes", 8);
        const auto file = root.getChildFile("eight-lane-workspace.json"); file.replaceWithText(juce::JSON::toString(old)); WorkspaceLayout migrated;
        check(readWorkspacePreferences(file, migrated).wasOk() && migrated.laneScale == WorkspaceLayout().laneScale
            && migrated.visibleLaneCount == 4 && migrated.preferredTimelineHeight == 0,
            "old eight-lane workspace sizes safely reopen as four fully visible lanes");
        migrated.laneScale[3] = .5; migrated.preferredTimelineHeight = 645;
        check(writeWorkspacePreferences(file, migrated).wasOk() && readWorkspacePreferences(file, reopened).wasOk()
            && reopened.visibleLaneCount == 4 && reopened.laneScale[3] == .5 && reopened.preferredTimelineHeight == 645
            && juce::JSON::parse(file)["futureSetting"].toString() == "keep me",
            "four lane weights and timeline height persist with unknown properties and a backup");
        for (const auto value : {juce::var(-1), juce::var(16385), juce::var("tall")})
        {
            auto bad = juce::JSON::parse(file); bad.getDynamicObject()->setProperty("timelineHeight", value);
            const auto invalid = root.getChildFile("invalid-height.json"); invalid.replaceWithText(juce::JSON::toString(bad));
            check(readWorkspacePreferences(invalid, reopened).failed(), "malformed undocked timeline heights are rejected");
        }
    }
    const auto huge = root.getChildFile("oversized.json"); huge.replaceWithText(juce::String::repeatedString(" ", 16385));
    check(readWorkspacePreferences(huge, reopened).failed() && writeWorkspacePreferences(huge, settings).failed() && huge.getSize() == 16385, "oversized workspace data is kept without unbounded reads");

    settings.monitorMuted = true;
    settings.forgeEffects = true;
    check(writeWorkspacePreferences(preference, settings).wasOk() && readWorkspacePreferences(preference, reopened).wasOk() && reopened.monitorMuted,
          "monitor mute persists outside musical sessions");
    check(reopened.forgeEffects, "optional forge effects persist in workspace settings independently of sessions");
    {
        auto bad = juce::JSON::parse(preference); bad.getDynamicObject()->setProperty("forgeEffects", "yes");
        const auto invalid = root.getChildFile("invalid-forge.json"); invalid.replaceWithText(juce::JSON::toString(bad));
        WorkspaceLayout untouched; const auto beforeBad = invalid.loadFileAsString();
        check(readWorkspacePreferences(invalid, untouched).failed() && writeWorkspacePreferences(invalid, settings).failed()
            && invalid.loadFileAsString() == beforeBad && !untouched.forgeEffects, "malformed visual preferences are preserved and rejected before changing settings");
    }
    {
        Engine monitor; auto tone = std::make_shared<Media>(); tone->sampleRate = 8000; tone->samples.setSize(1, 16000);
        for (int i = 0; i < 16000; ++i) tone->samples.setSample(0, i, .2f * (float) std::sin(i * .07));
        Clip c; c.media = tone; c.length = 2; monitor.project.clips = {c}; monitor.sampleRate = 8000;
        monitor.project.masterDb = -9; monitor.project.masterAutomation.enabled = true;
        monitor.project.masterAutomation.setPoint(0, -6); monitor.project.masterAutomation.setPoint(2, -3);
        const auto musicalState = juce::JSON::toString(projectToJson(monitor.project));
        float left[256] {}, right[256] {}; float* outputs[] {left, right};
        monitor.setPlaying(true); monitor.audioDeviceIOCallbackWithContext(nullptr, 0, outputs, 2, 256, {});
        check(monitor.outputPeaks.take()[0] > .01f, "device callback publishes actual pre-mute output peaks");
        check(juce::FloatVectorOperations::findMinAndMax(left, 256).getLength() > .01f, "unmuted device callback delivers the mix");
        const auto position = monitor.position.load(); monitor.monitorMuted = true;
        std::fill_n(left, 256, 1.0f); std::fill_n(right, 256, 1.0f);
        monitor.audioDeviceIOCallbackWithContext(nullptr, 0, outputs, 2, 256, {});
        check(juce::FloatVectorOperations::findMinAndMax(left, 256) == juce::Range<float>()
              && juce::FloatVectorOperations::findMinAndMax(right, 256) == juce::Range<float>()
              && monitor.position > position && monitor.masterPeak > .01f,
              "Mute silences both device channels while playback and pre-mute meters continue");
        check(monitor.outputPeaks.take()[1] > .01f, "monitor mute retains both stereo output measurements");
        juce::AudioBuffer<float> reference(2, 256), muted(2, 256); juce::MidiBuffer midi;
        monitor.monitorMuted = false; monitor.render(reference, 256, .5, midi);
        monitor.monitorMuted = true; monitor.render(muted, 256, .5, midi);
        bool same = reference.getMagnitude(0, 256) > .01f;
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 256; ++i) same &= reference.getSample(ch, i) == muted.getSample(ch, i);
        check(same && juce::JSON::toString(projectToJson(monitor.project)) == musicalState,
              "monitor mute preserves exact offline PCM, master gain and automation");
        check(monitor.outputPeaks.take() == std::array<float, 2>{0, 0}, "offline renders never publish peaks to the live device display");
        monitor.gridAudition.start(tone, {}, .25, true, true, -12); monitor.playing = false;
        monitor.audioDeviceIOCallbackWithContext(nullptr, 0, outputs, 2, 256, {});
        check(juce::FloatVectorOperations::findMinAndMax(left, 256) == juce::Range<float>(), "monitor mute also silences source-grid audition");
        monitor.gridAudition.stop(); monitor.monitorMuted = false; monitor.playing = true;
        monitor.audioDeviceIOCallbackWithContext(nullptr, 0, outputs, 2, 256, {});
        check(juce::FloatVectorOperations::findMinAndMax(left, 256).getLength() > .01f, "unmuting resumes audible output without resetting the mix");
    }

    auto ownedStackFixture_ui = std::make_unique<Workstation>(false); auto& ui = *ownedStackFixture_ui; ui.stopTimer(); ui.workspacePreference = preference;
    {
        const int columns[] {5, 1, 3, 4, 2, 7, 8}; bool order = true;
        for (int i = 0; i < 7; ++i) order &= ui.libraryTable.table.getHeader().getColumnIdOfIndex(i, true) == columns[i];
        check(order, "default library order matches Art / Title / Key / BPM / Time / Conf / Genre");
        const auto before = juce::JSON::toString(projectToJson(ui.engine.project));
        const auto undo = ui.undoHistory.size(); const auto position = ui.engine.position.load(); const bool dirty = ui.dirty;
        ui.shortcutsTab.onClick();
        check(ui.showingShortcuts && ui.shortcutSearch.isVisible() && !ui.pluginList.list.isVisible() && ui.masterSlider.isVisible()
            && ui.shortcutResults.getText().contains("Paste at playhead") && !ui.shortcutResults.getText().contains("Lock / unlock transition"),
            "Shortcuts tab shares VST area and opens everyday commands with master output retained");
        ui.allShortcuts.onClick();
        check(ui.showAllShortcuts && ui.shortcutResults.getText().contains("Lock / unlock transition"), "All exposes contextual commands");
        ui.allShortcuts.onClick(); ui.shortcutSearch.setText("LOCK transition", false); ui.refreshShortcuts();
        check(!ui.showAllShortcuts && ui.shortcutResults.getText().contains("Alt + L") && !ui.shortcutResults.getText().contains("Play / pause"),
            "search covers all shortcuts, case-insensitive and independent of everyday filter");
        ui.shortcutSearch.setText("ctrl+v", false); ui.refreshShortcuts();
        check(ui.shortcutResults.getText().contains("Paste at playhead"), "search accepts key chords without spaces");
        ui.shortcutSearch.setText("zzzz-no-command", false); ui.refreshShortcuts();
        check(ui.shortcutResults.getText().contains("No matching shortcuts"), "search has explicit empty result state");
        ui.shortcutSearch.clear(); ui.refreshShortcuts();
        for (auto size : {juce::Point<int>(1250, 720), juce::Point<int>(1920, 1080)})
        {
            ui.setSize(size.x, size.y);
            check(ui.layout.detail.contains(ui.shortcutResults.getBounds()) && ui.shortcutResults.getHeight() > 100
                && ui.layout.detail.contains(ui.shortcutSearch.getBounds()) && !ui.shortcutResults.getBounds().intersects(ui.shortcutSearch.getBounds()),
                "shortcut controls fit compact and wide inspector " + juce::String(size.x));
            auto output = report.getSiblingFile("shortcuts-" + juce::String(size.x) + ".png").createOutputStream();
            if (output) { juce::PNGImageFormat png; png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *output); }
        }
        ui.layout.inspectorCollapsed = true; ui.resized();
        check(!ui.shortcutResults.isVisible() && !ui.shortcutsTab.isVisible(), "collapsing inspector hides shortcut controls");
        ui.layout.inspectorCollapsed = false; ui.effectsTab.onClick();
        check(ui.pluginList.list.isVisible() && !ui.shortcutResults.isVisible(), "VST tab restores existing plugin controls");
        ui.showInspectorPage(2); ui.showShortcuts(true); ui.effectsTab.onClick();
        check(ui.piano.isVisible() && ui.inspectorPage == 2, "switching shortcut tab preserves MIDI keyboard mode");
        check(before == juce::JSON::toString(projectToJson(ui.engine.project)) && ui.undoHistory.size() == undo
            && ui.engine.position == position && ui.dirty == dirty, "shortcut browsing leaves musical state, playhead and history unchanged");
        ui.showInspectorPage(1); ui.setSize(1440, 940);
    }

    {
        const auto state = juce::JSON::toString(projectToJson(ui.engine.project));
        const auto history = ui.undoHistory.size(); const bool wasDirty = ui.dirty;
        const auto motion = ui.layout.forgeMotion; const auto attract = ui.attractState.motion;
        check(ui.layout.popupHints && ui.hints.hintsEnabled(), "popup hints default on, including legacy workspace preferences");
        ui.menuItemSelected(86, 4);
        check(!ui.layout.popupHints && !ui.hints.hintsEnabled() && ui.hints.getTipFor(ui.playButton).isEmpty(), "View Popup hints turns off the host immediately");
        ui.saveWorkspacePreferences(); WorkspaceLayout restored;
        check(readWorkspacePreferences(preference, restored).wasOk() && !restored.popupHints, "popup hints off persists independently of a musical session");
        const auto badHint = root.getChildFile("bad-hint.json"); auto bad = juce::JSON::parse(preference);
        bad.getDynamicObject()->setProperty("popupHints", "false"); badHint.replaceWithText(juce::JSON::toString(bad));
        const auto retained = badHint.loadFileAsString();
        check(readWorkspacePreferences(badHint, restored).failed() && writeWorkspacePreferences(badHint, ui.layout).failed()
            && badHint.loadFileAsString() == retained && !restored.popupHints, "wrongly typed hint preference is retained and does not partially apply");
        bad.getDynamicObject()->removeProperty("popupHints"); badHint.replaceWithText(juce::JSON::toString(bad));
        check(readWorkspacePreferences(badHint, restored).wasOk() && restored.popupHints, "legacy workspace without hint field defaults on");
        ui.menuItemSelected(86, 4); ui.saveWorkspacePreferences();
        check(ui.hints.hintsEnabled() && readWorkspacePreferences(preference, restored).wasOk() && restored.popupHints,
            "View Popup hints turns on again and reopens enabled");
        check(juce::JSON::toString(projectToJson(ui.engine.project)) == state && ui.undoHistory.size() == history && ui.dirty == wasDirty
            && ui.layout.forgeMotion == motion && ui.attractState.motion == attract, "hints preserve musical state, Undo and reduced-motion choices");
        juce::Component valueChild; ui.masterSlider.addChildComponent(valueChild);
        check(AlloyHints::textFor(valueChild).contains("Master output gain"), "slider text/value children inherit their control's explanation");
        ui.masterSlider.removeChildComponent(&valueChild);
        ui.splitButton.setEnabled(false);
        ui.splitButton.getProperties().set("mixworksHintDisabled", "Select an unlocked clip first.");
        check(AlloyHints::textFor(ui.splitButton).contains("Select an unlocked clip first"), "disabled controls retain their explanation and actionable unavailable state");
        ui.splitButton.getProperties().remove("mixworksHintDisabled");
        juce::Component foreign; foreign.getProperties().set("mixworksExternalEditor", true); foreign.addChildComponent(valueChild);
        valueChild.getProperties().set("mixworksHint", "Must remain private");
        check(AlloyHints::textFor(valueChild).isEmpty(), "third-party editor boundary excludes app hints, including descendants");
        foreign.removeChildComponent(&valueChild);
        ui.detachToolPanel(0, true);
        check(AlloyHints::textFor(ui.performance->pots[0]).isNotEmpty(), "same hint resolver covers detached live desk controls");
        ui.detachToolPanel(0, false);
    }
    check(ui.bottomSplitter.isVisible() && ui.bottomSplitter.isEnabled(), "the timeline stretch grip remains available while both panels are docked");
    const auto cleanUndo = ui.undoHistory.size(); ui.dirty = false;
    ui.monitorMute.setToggleState(true, juce::dontSendNotification); ui.monitorMute.onClick();
    ui.saveWorkspacePreferences();
    check(ui.engine.monitorMuted && !ui.dirty && ui.undoHistory.size() == cleanUndo && readWorkspacePreferences(preference, reopened).wasOk() && reopened.monitorMuted,
          "native Mute checkbox persists monitoring without dirtying the mix or adding Undo");
    ui.monitorMute.setToggleState(false, juce::dontSendNotification); ui.monitorMute.onClick();
    const char* titles[] {"Deep Current", "Submerge", "Glass Horizons", "Fading Light", "Aurora Motion", "Parallel Skies", "Velvet Torque", "Wide Awake"};
    for (int track = 0; track < 8; ++track)
    {
        auto media = std::make_shared<Media>(); media->name = titles[track]; media->file = root.getChildFile(media->name + ".wav"); media->sampleRate = 8000;
        media->manualBpm = 120; media->manualBeatOrigin = 0; media->manualKey = track % 12; media->samples.setSize(1, 8000 * 24);
        media->assets.artist = "Alloy interface test fixture";
        media->assets.cover = juce::Image(juce::Image::RGB, 48, 48, true, juce::SoftwareImageType());
        { juce::Graphics g(media->assets.cover); g.fillAll(colour::tracks[(size_t) track % 4].darker(0.8f)); g.setColour(colour::tracks[(size_t) track % 4]); g.drawEllipse(9, 9, 30, 30, 2); g.drawLine(3, (float) (10 + track * 3), 45, 40.0f - track * 2, 2); }
        const int bucket = std::max(1, media->frames() / 4000);
        media->peaks.resize((size_t) ((media->frames() + bucket - 1) / bucket));
        for (int frame = 0; frame < media->frames(); ++frame)
        {
            const double t = frame / 8000.0, pulse = std::exp(-std::fmod(t, 0.5) * (18 + track));
            const float value = (float) ((0.18 + 0.62 * pulse) * std::sin(t * (370 + track * 73)) * (0.7 + 0.3 * std::sin(t * 1.7)));
            media->samples.setSample(0, frame, value); auto& peak = media->peaks[(size_t) frame / bucket]; peak = std::max(peak, std::abs(value));
        }
        media->prepareWaveformBody();
        ui.engine.library.push_back(media);
        if (track < 4) { Clip clip; clip.media = media; clip.track = track; clip.start = track * 2; clip.length = 24; clip.fadeIn = 3; clip.fadeOut = 4; ui.engine.project.clips.push_back(clip); }
    }
    ui.engine.project.tempo.setPoint(0, 120); ui.engine.project.tempo.setPoint(8, 128); ui.engine.project.tempo.setPoint(16, 124);
    ui.selectedClip = ui.engine.project.clips.front().id; ui.timeline.pixelsPerSecond = 20; ui.engine.seek(12); ui.refresh();
    const auto source = juce::Desktop::getInstance().getMainMouseSource();
    auto event = [&](juce::Component& component, juce::Point<float> position, juce::Point<float> down, bool dragged, bool fine = false)
    { return juce::MouseEvent(source, position, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier | (fine ? juce::ModifierKeys::shiftModifier : 0)), 1, 0, 0, 0, 0, &component, &component, juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), 1, dragged); };
    auto& pan = ui.strips[0]->pan;
    auto drag = [&](bool fine)
    {
        pan.setValue(0, juce::sendNotificationSync); const juce::Point<float> start {35, 25}, end {35, 1};
        pan.mouseDown(event(pan, start, start, false, fine)); pan.mouseDrag(event(pan, end, start, true, fine)); pan.mouseUp(event(pan, end, start, true, fine)); return pan.getValue();
    };
    const double coarse = drag(false), fine = drag(true);
    check(coarse > fine && fine > 0 && std::abs(coarse / fine - 10) < 1, "hexagonal pan dial supports relative vertical drag and Shift fine adjustment");
    check(std::abs(ui.engine.controlSettings(0).pan - fine) < 0.001, "dial gestures update the existing mixer value");
    const auto undoCount = ui.undoHistory.size(); pan.keyPressed(juce::KeyPress(juce::KeyPress::rightKey));
    check(ui.undoHistory.size() == undoCount + 1 && pan.getValue() > fine, "keyboard dial adjustment creates an undoable gesture");
    ui.undo(false); check(std::abs(ui.engine.controlSettings(0).pan - fine) < 0.001, "undo restores the dial's prior mixer value");
    pan.keyPressed(juce::KeyPress(juce::KeyPress::homeKey)); check(pan.getValue() == 0 && ui.engine.controlSettings(0).pan == 0, "Home restores the pan center");
    ui.resizeLane(0, 2 * ui.layout.defaultLaneHeight);
    pan.showTextBox(); juce::TextEditor* editor = nullptr;
    for (int i = 0; i < pan.getNumChildComponents(); ++i) if (auto* label = dynamic_cast<juce::Label*>(pan.getChildComponent(i))) if (label->getCurrentTextEditor()) editor = label->getCurrentTextEditor();
    if (editor) { editor->setText("25 L"); ui.refresh(); check(editor->getText() == "25 L", "native refresh preserves in-progress dial text"); pan.hideTextBox(false); }
    check(editor != nullptr && std::abs(ui.engine.controlSettings(0).pan + 0.25) < 0.001, "exact pan text commits through the existing control path");
    ui.resizeLane(0, ui.layout.defaultLaneHeight);
    ui.engine.controlSettings(0).pan = 0.4f; ui.refresh(); check(std::abs(pan.getValue() - 0.4) < 0.001, "external mixer changes refresh the same hexagonal dial");
    ui.addToDesktop(0); // JUCE creates accessibility handlers only for a native peer.
    check(pan.getAccessibilityHandler() && pan.getAccessibilityHandler()->getValueInterface(), "hexagonal dial retains native accessible value support");
    ui.removeFromDesktop();
    ui.showBrowserPage(1); check(ui.playlistTable.table.isVisible() && !ui.libraryTable.table.isVisible() && ui.getMenuBarNames().contains("Beat grids"), "Playlist tab retains arrangement and beatmatch access");
    ui.showBrowserPage(0); check(ui.libraryTable.table.isVisible() && ui.search.isVisible() && !ui.playlistTable.table.isVisible(), "Library tab restores search and cover catalogue");

    const auto sessionUndo = ui.undoHistory.size(); ui.dirty = false;
    ui.leftSplitter.keyPressed(juce::KeyPress(juce::KeyPress::rightKey)); const auto leftWidth = ui.layout.library.getWidth();
    ui.rightSplitter.keyPressed(juce::KeyPress(juce::KeyPress::leftKey));
    check(leftWidth == 330 && ui.layout.library.getWidth() == 330 && ui.layout.master.getWidth() == 280 && !ui.dirty && ui.undoHistory.size() == sessionUndo,
          "independent panel resizing preserves the opposite width and musical undo/dirty state");
    auto& split = ui.leftSplitter; const juce::Point<float> begin {5, 150}, moved {45, 150};
    split.mouseDown(event(split, begin, begin, false)); split.mouseDrag(event(split, moved, begin, true)); split.mouseUp(event(split, moved, begin, true));
    check(ui.layout.library.getWidth() == 370 && ui.layout.master.getWidth() == 280, "native divider pointer drag resizes its panel by the horizontal distance");
    ui.saveWorkspacePreferences(); WorkspaceLayout saved;
    check(readWorkspacePreferences(preference, saved).wasOk() && saved.preferredLibraryWidth == 370 && saved.preferredInspectorWidth == 280, "native panel gestures persist both preferred widths");
    ui.leftSplitter.keyPressed(juce::KeyPress(juce::KeyPress::homeKey)); ui.rightSplitter.keyPressed(juce::KeyPress(juce::KeyPress::homeKey));
    check(ui.layout.library.getWidth() == 320 && ui.layout.master.getWidth() == 270, "panel reset restores default widths");
    for (auto* table : {&ui.libraryTable.table, &ui.playlistTable.table})
    {
        auto& header = table->getHeader(); header.setColumnWidth(1, 217); header.setColumnWidth(3, 113); header.moveColumn(3, 0);
        ui.resizeSidePanel(true, 440); ui.resizeSidePanel(false, 310); ui.resizeSidePanel(true, 290); ui.resized();
        check(header.getColumnWidth(1) == 217 && header.getColumnWidth(3) == 113 && header.getColumnIdOfIndex(0, true) == 3,
              table == &ui.libraryTable.table ? "library column widths and order survive splitter movement" : "playlist column widths and order survive splitter movement");
    }
    ui.resizeSidePanel(true, 320); ui.resizeSidePanel(false, 270);

    auto capture = [&](juce::Component& component, const juce::String& name, float scale = 1)
    {
        const auto snapshot = component.createComponentSnapshot(component.getLocalBounds(), true, scale);
        auto out = report.getSiblingFile(name).createOutputStream(); if (out) { out->setPosition(0); out->truncate(); }
        juce::PNGImageFormat png; return out && png.writeImageToStream(snapshot, *out);
    };
    auto ownerLabels = [&]
    {
        const auto selected = ui.selectedClip;
        const auto document = juce::JSON::toString(projectToJson(ui.engine.project));
        const auto history = ui.undoHistory.size(); const auto dirty = ui.dirty;
        const auto mode = ui.engine.automationRecordMode;
        const auto lanes = ui.engine.deskLanes;
        const auto action = midiActionId({MidiActionKind::deskPot, 0});
        ui.engine.deskLanes[0] = 0; ui.engine.deskLanes[6] = 0; ui.refresh();
        auto& strip = *ui.strips[0]; auto& desk = *ui.performance;
        const auto expected = "Segment 1 / " + ui.engine.project.clips.front().media->libraryTitle() + " / lane 1";
        for (auto* control : std::initializer_list<juce::Slider*> {&strip.gain, &strip.pan, &desk.faders[0], &desk.faders[4], &desk.pots[0]})
            check(control->getName().contains(expected) && control->getTooltip().contains(expected),
                "owner label follows the selected segment for " + control->getName());
        check(strip.cutoff.getName().contains("Mixer lane 1") && desk.pots[6].getName().contains("Mixer lane 1")
            && !strip.cutoff.getName().contains("Segment") && desk.master.getName().contains("Master"),
            "cutoff and master retain their actual owners while a segment is selected");
        const auto once = strip.gain.getTooltip(); ui.refresh();
        check(strip.gain.getTooltip() == once && once.contains("Record captures a performance"),
            "owner refresh preserves existing help without accumulating prefixes");
        ui.addToDesktop(0);
        auto* tableHandler = ui.libraryTable.table.getAccessibilityHandler();
        auto* tableInterface = tableHandler ? tableHandler->getTableInterface() : nullptr;
        auto* row = tableInterface ? tableInterface->getRowHandler(0) : nullptr;
        check(row && row->getTitle().contains("Deep Current") && row->getTitle().contains("BPM 120.00")
            && row->getTitle().contains("key ") && row->getTitle().contains("artist Alloy interface test fixture")
            && row->getTitle().contains("key check Manual"), "native Library row announces actual song, artist, BPM, key and review status");
        ui.showBrowserPage(1); ui.refresh();
        tableHandler = ui.playlistTable.table.getAccessibilityHandler();
        tableInterface = tableHandler ? tableHandler->getTableInterface() : nullptr;
        row = tableInterface ? tableInterface->getRowHandler(0) : nullptr;
        check(row && row->getTitle().contains("Segment 1") && row->getTitle().contains("Deep Current")
            && row->getTitle().contains("mixer lane 1") && row->getTitle().contains("start ") && row->getTitle().contains("length "),
            "native Playlist row announces segment identity, musical metadata and placement");
        ui.showBrowserPage(0); ui.refresh();
        for (auto* control : std::initializer_list<juce::Slider*> {&strip.gain, &strip.pan, &desk.faders[0], &desk.pots[0]})
            check(control->getAccessibilityHandler() && control->getAccessibilityHandler()->getTitle().contains(expected),
                "native accessible control title exposes selected segment owner");
        ui.hints.displayTip({720, 470}, AlloyHints::textFor(strip.gain));
        check(capture(ui.hints, "owner-segment-hint.png"), "capture actual segment-owner popup"); ui.hints.hideTip();
        ui.selectedClip.clear(); ui.refresh();
        for (auto* control : std::initializer_list<juce::Slider*> {&strip.gain, &strip.pan, &desk.faders[0], &desk.faders[4], &desk.pots[0]})
            check(control->getName().contains("Mixer lane 1") && !control->getTooltip().contains("Segment 1 /"),
                "clearing selection exposes mixer-lane owner for " + control->getName());
        ui.hints.displayTip({720, 470}, AlloyHints::textFor(strip.gain));
        check(capture(ui.hints, "owner-lane-hint.png"), "capture actual mixer-lane-owner popup"); ui.hints.hideTip();
        ui.removeFromDesktop(); ui.selectedClip = selected; ui.engine.deskLanes = lanes; ui.refresh();
        check(juce::JSON::toString(projectToJson(ui.engine.project)) == document && ui.undoHistory.size() == history
            && ui.dirty == dirty && ui.engine.automationRecordMode == mode && midiActionId({MidiActionKind::deskPot, 0}) == action,
            "owner descriptions and selection preserve document, Undo, dirty, recording and MIDI identity");
    };
    ownerLabels();
    auto accessibleContent = [&]
    {
        auto owned = std::make_unique<Workstation>(false); auto& access = *owned; access.stopTimer(); access.setSize(1250, 800);
        access.engine.library = ui.engine.library;
        access.engine.project.clips = ui.engine.project.clips;
        access.selectedClip = access.engine.project.clips.front().id; access.refresh(); access.addToDesktop(0);
        const auto document = juce::JSON::toString(projectToJson(access.engine.project)); const auto history = access.undoHistory.size();
        juce::Array<juce::var> observed;
        auto inspect = [&](LibraryTable& model, bool playlist)
        {
            auto* handler = model.table.getAccessibilityHandler();
            auto* table = handler ? handler->getTableInterface() : nullptr;
            check(table && table->getNumRows() == model.getNumRows(), "native table interface reports current filtered row count");
            if (!table) return;
            for (int col = 0; col < model.table.getHeader().getNumColumns(true); ++col)
            {
                const int id = model.table.getHeader().getColumnIdOfIndex(col, true);
                if (playlist && id == 6) continue; // The existing interactive transition picker keeps its own native role.
                const auto* cell = table->getCellHandler(0, col);
                const auto rowSpan = cell ? table->getRowSpan(*cell) : juce::Optional<juce::AccessibilityTableInterface::Span>();
                const auto colSpan = cell ? table->getColumnSpan(*cell) : juce::Optional<juce::AccessibilityTableInterface::Span>();
                check(cell && cell->getRole() == juce::AccessibilityRole::cell && cell->getTitle().isNotEmpty()
                    && cell->getCellInterface() && cell->getCellInterface()->getTableHandler() == handler
                    && rowSpan && rowSpan->begin == 0 && rowSpan->num == 1 && colSpan && colSpan->begin == col && colSpan->num == 1,
                    "native musical cell has a name, cell role and correct table spans after reorder/filter");
                if (cell)
                {
                    bool self = true, children = true; cell->getComponent().getInterceptsMouseClicks(self, children);
                    check(!self && !children, "named cell preserves existing row mouse, double-click and drag routing");
                    auto* item = new juce::DynamicObject(); item->setProperty("table", playlist ? "Playlist" : "Library");
                    item->setProperty("columnId", id); item->setProperty("title", cell->getTitle()); item->setProperty("help", cell->getHelp());
                    observed.add(juce::var(item));
                }
            }
        };
        inspect(access.libraryTable, false);
        auto& header = access.libraryTable.table.getHeader(); header.moveColumn(4, 0); header.setColumnVisible(8, false);
        access.search.setText("Deep Current", juce::dontSendNotification); access.filterLibrary(); access.libraryTable.table.updateContent();
        inspect(access.libraryTable, false);
        auto* table = access.libraryTable.table.getAccessibilityHandler()->getTableInterface();
        const auto* row = table->getRowHandler(0);
        check(access.libraryTable.getNumRows() == 1 && row && row->getTitle().contains("Deep Current"),
            "recycled native row and cells follow the filtered song identity");
        check(row && row->getActions().invoke(juce::AccessibilityActionType::focus)
            && access.selectedLibraryId == ui.engine.project.clips.front().media->id,
            "native row focus retains the existing source-selection action");
        access.search.setText({}, juce::dontSendNotification); access.libraryTable.sortOrderChanged(1, false);
        inspect(access.libraryTable, false);
        table = access.libraryTable.table.getAccessibilityHandler()->getTableInterface(); row = table->getRowHandler(0);
        check(row && row->getTitle().contains("Wide Awake"), "recycled native row follows descending sort without stale song metadata");
        access.showBrowserPage(1); access.refresh(); inspect(access.playlistTable, true);
        auto* focus = juce::Component::getCurrentlyFocusedComponent();
        const auto* feedback = access.statusAccess.getAccessibilityHandler();
        access.sessionFile = root.getChildFile("accessible.mixworks"); access.dirty = true;
        access.setStatus("Copied segment."); access.setStatus("Paste cancelled.");
        check(feedback && feedback->getRole() == juce::AccessibilityRole::staticText && feedback->getTitle().contains("Paste cancelled.")
            && feedback->getTitle().contains("accessible.mixworks") && feedback->getTitle().contains("unsaved changes")
            && feedback->getTitle().contains("Selected segment 1") && feedback->getTitle().contains("Deep Current")
            && juce::Component::getCurrentlyFocusedComponent() == focus, "native footer queries read latest feedback, document, dirty and selected owner without taking focus");
        access.dirty = false; access.selectedClip.clear();
        check(feedback && feedback->getTitle().contains(", saved.") && !feedback->getTitle().contains("Selected segment")
            && feedback->getTitle().contains("Mixer lane"), "footer queries immediately follow save and deselection even between coalesced notifications");
        check(access.statusAccess.getBounds() == access.layout.status && !access.statusAccess.getWantsKeyboardFocus(),
            "native feedback occupies the painted footer without an extra tab stop");
        check(capture(access, "accessible-content-1250.png"), "capture named table and native feedback at compact size");
        access.setSize(1920, 1080); check(capture(access, "accessible-content-1920.png"), "capture accessible content at wide size");
        report.getSiblingFile("native-cell-metadata.json").replaceWithText(juce::JSON::toString(juce::var(observed)));
        check(juce::JSON::toString(projectToJson(access.engine.project)) == document && access.undoHistory.size() == history,
            "metadata, native selection and footer feedback preserve musical document and Undo");
        access.removeFromDesktop();
    };
    accessibleContent();
    auto accessibleTimeline = [&]
    {
        auto owned = std::make_unique<Workstation>(false); auto& access = *owned; access.stopTimer(); access.setSize(1250, 800);
        access.engine.library = ui.engine.library;
        access.engine.project.clips = {ui.engine.project.clips[0], ui.engine.project.clips[1]};
        for (auto& clip : access.engine.project.clips) clip.effects = makeClipEffects();
        access.engine.project.clips[1].start = 8;
        initialiseTransition(access.engine.project.clips[0], access.engine.project.clips[1], 16, TransitionPreset::crossfade);
        const auto first = access.engine.project.clips[0].id, second = access.engine.project.clips[1].id;
        check(access.engine.project.linkClips(first, second).wasOk()
            && std::abs(access.engine.project.transitionRange(*access.engine.project.transitionFor(second)).getLength() - 16) < 1.e-8,
            "accessible transition fixture has a genuine sixteen-second shared frame");
        access.engine.seek(7); access.refresh(); access.addToDesktop(0); access.setVisible(true);
        auto find = [&](const juce::String& id) { return dynamic_cast<TimelineAccessItem*>(access.timeline.findChildWithID(id)); };
        auto* segment = find("segment:" + first); auto* pair = find("transition:" + second);
        auto* handler = segment ? segment->getAccessibilityHandler() : nullptr;
        const auto musicalChildren = access.timeline.getAccessibilityHandler()->getChildren();
        check(handler && pair && std::find(musicalChildren.begin(), musicalChildren.end(), handler) != musicalChildren.end()
            && std::find(musicalChildren.begin(), musicalChildren.end(), pair->getAccessibilityHandler()) != musicalChildren.end(),
            "native segment and transition are discoverable through the actual canvas tree");
        check(access.timeline.getAccessibilityHandler() && access.timeline.getAccessibilityHandler()->getRole() == juce::AccessibilityRole::list
            && handler && handler->getRole() == juce::AccessibilityRole::listItem && handler->getTitle().contains("Segment 1")
            && handler->getTitle().contains("Deep Current") && handler->getTitle().contains("mixer lane 1")
            && handler->getTitle().contains("BPM 120.00"), "native timeline exposes named segment objects and their actual musical owners");
        const auto document = juce::JSON::toString(projectToJson(access.engine.project)); const auto history = access.undoHistory.size(); const bool dirty = access.dirty;
        const auto mode = access.engine.automationRecordMode; const auto position = access.engine.position.load();
        check(handler && handler->getActions().invoke(juce::AccessibilityActionType::focus)
            && access.selectedClip == first && access.toolPage == 2 && handler->getCurrentState().isSelected(),
            "native segment focus selects its actual owner and existing Editing console");
        access.refresh();
        check(find("segment:" + first) == segment && juce::Component::getCurrentlyFocusedComponent() == segment,
            "musical-object identity and keyboard focus survive ordinary refresh");
        check(segment && access.timeline.getComponentAt(segment->getBounds().getCentre()) == &access.timeline,
            "accessible segment objects preserve existing canvas mouse and drag hit testing");
        auto* pairHandler = pair ? pair->getAccessibilityHandler() : nullptr;
        check(pairHandler && pairHandler->getTitle().contains("Deep Current to Submerge")
            && pairHandler->getActions().invoke(juce::AccessibilityActionType::press) && access.selectedTransition == second
            && access.selectedClip == second && access.toolPage == 1 && access.engine.position == position
            && juce::Component::getCurrentlyFocusedComponent() == pair,
            "native transition action selects the pair and Transitions console without seeking");
        access.engine.exporting = true; if (segment) segment->activate(); access.engine.exporting = false;
        check(access.selectedClip == second, "stale enabled accessibility action revalidates export guard at execution");
        access.busy = true; if (segment) segment->activate(); access.busy = false;
        check(access.selectedClip == second, "native object action revalidates busy guard");
        check(juce::JSON::toString(projectToJson(access.engine.project)) == document && access.undoHistory.size() == history
            && access.dirty == dirty && access.engine.position == position && access.engine.automationRecordMode == mode,
            "native segment/transition selection preserves musical state, playhead, Undo, dirty and recording mode");
        access.engine.project.setTransitionLocked(second, true); access.refresh();
        segment = find("segment:" + first); if (segment) segment->activate();
        const auto lockedDocument = juce::JSON::toString(projectToJson(access.engine.project));
        access.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey));
        check(segment && segment->getAccessibilityHandler()->getTitle().contains("locked")
            && juce::JSON::toString(projectToJson(access.engine.project)) == lockedDocument && access.undoHistory.size() == history,
            "accessible selection exposes locks and retains the canonical Delete protection");
        access.engine.project.setTransitionLocked(second, false); access.refresh();
        if (segment) segment->keyPressed(juce::KeyPress(juce::KeyPress::returnKey));
        const auto beforeDelete = juce::JSON::toString(projectToJson(access.engine.project));
        access.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey));
        check(!find("segment:" + first) && !find("transition:" + second) && access.engine.project.clips.size() == 1
            && access.undoHistory.size() == history + 1, "existing Delete removes only the chosen unlocked segment and retires its native objects");
        access.undo(false);
        check(find("segment:" + first) && find("transition:" + second)
            && juce::JSON::toString(projectToJson(access.engine.project)) == beforeDelete, "canonical Undo restores musical data and its native objects");
        access.timeline.pixelsPerSecond = 80; access.refresh(); access.viewport.setViewPosition(1000, 0);
        segment = find("segment:" + first); if (segment) segment->getAccessibilityHandler()->getActions().invoke(juce::AccessibilityActionType::focus);
        check(segment && access.viewport.getViewPositionX() < 100 && access.engine.position == position,
            "native focus reveals an offscreen segment without changing the playback clock");
        check(capture(access, "accessible-timeline-1250.png"), "capture native keyboard focus on a segment at compact size");
        access.setSize(1920, 1080); access.timeline.pixelsPerSecond = 30; access.refresh();
        pair = find("transition:" + second); if (pair) pair->getAccessibilityHandler()->getActions().invoke(juce::AccessibilityActionType::focus);
        check(capture(access, "accessible-timeline-1920.png"), "capture native keyboard focus on the transition frame at wide size");
        access.removeFromDesktop();
    };
    accessibleTimeline();
    auto accessibleMarkersAndNodes = [&]
    {
        auto owned = std::make_unique<Workstation>(false); auto& access = *owned; access.stopTimer(); access.setSize(1250, 800);
        auto pump = []
        {
            const auto until = juce::Time::getMillisecondCounterHiRes() + 40;
            do
            {
               #if JUCE_WINDOWS
                MSG message; while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
               #endif
                juce::Thread::sleep(1);
            } while (juce::Time::getMillisecondCounterHiRes() < until);
        };
        auto media = std::make_shared<Media>(); media->name = "Native node song"; media->sampleRate = 8000; media->manualBpm = 120;
        media->samples.setSize(1, 8000 * 40); media->samples.clear(); media->file = root.getChildFile("native-node.wav");
        Clip first; first.media = media; first.start = 4; first.length = 16;
        Clip second = first; second.id = juce::Uuid().toString(); second.start = 12; second.track = 1; second.effects = makeClipEffects();
        initialiseTransition(first, second, 8, TransitionPreset::crossfade);
        // Native identity tests exercise ordinary manual nodes independently
        // of legacy fade composition (covered by PresetNodeTests).
        first.fadeOut = second.fadeIn = 0;
        first.fadeOutAnchor = first.sourceElapsed(first.length); second.fadeInAnchor = second.sourceElapsed(0);
        first.controls.automation[0] = {true, {{0, -6}, {4, -12}, {8, -3}, {16, -6}}};
        access.engine.library = {media}; access.engine.project.clips = {first, second}; access.engine.project.linkClips(first.id, second.id);
        LabelMarker marker; marker.time = 30; marker.text = "Bridge label"; access.engine.project.putLabelMarker(marker);
        const AutomationAddress address {0, AutomationTarget::volume, {}, {}, first.id};
        access.selectedClip = first.id; access.selectedAutomationTarget = 0; access.engine.seek(3); access.refresh(); access.addToDesktop(0); access.setVisible(true);
        auto nodeAt = [&](double storedTime, int duplicate = 0) -> TimelineAccessItem*
        {
            const auto suffix = "source/curve time " + juce::String(storedTime, 6);
            for (auto* child : access.timeline.getChildren()) if (auto* item = dynamic_cast<TimelineAccessItem*>(child))
                if (item->valueObject && item->read && item->read().contains(suffix) && duplicate-- == 0) return item;
            return nullptr;
        };
        auto json = [&] { return juce::JSON::toString(projectToJson(access.engine.project)); };
        auto* node = nodeAt(4); auto* handler = node ? node->getAccessibilityHandler() : nullptr;
        auto* value = handler ? handler->getValueInterface() : nullptr;
        const auto initial = json(); const auto history = access.undoHistory.size(); const auto position = access.engine.position.load();
        check(handler && handler->getTitle().contains("Segment 1 / Native node song / Volume")
            && value && value->isReadOnly() && std::abs(value->getCurrentValue() + 12) < 1.e-6 && value->getCurrentValueAsString().isNotEmpty(),
            "native automation node exposes the actual owner and read-only displayed value");
        if (value) { value->setValue(-60); value->setValueAsString("0"); }
        if (handler) handler->getActions().invoke(juce::AccessibilityActionType::focus);
        check(json() == initial && access.undoHistory.size() == history && access.engine.position == position
            && access.automationNode.getText().contains("Node 2/4"), "native node selection/value queries preserve document, history and clock");
        const auto key = node ? node->getComponentID() : juce::String();
        access.engine.project.curve(address)->setPoint(2, -9); access.refresh();
        check(nodeAt(4) == node && access.timeline.findChildWithID(key) == node, "inserting an earlier point preserves the actual native node identity");
        if (node) node->activate();
        check(access.automationNode.getText().contains("Node 3/5"), "native action resolves curve time after indices change");
        const auto beforeEdit = json();
        if (node) node->invoke();
        auto* dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        check(dialog && dialog->getName() == "Automation node" && dialog->getDescription().contains("Segment 1 / Native node song / Volume")
            && std::abs(dialog->getTextEditorContents("time").getDoubleValue() - 8) < 1.e-8
            && capture(*dialog, "accessible-node-editor.png"), "native node opens the existing exact editor with mapped project time");
        if (dialog) { dialog->exitModalState(0); pump(); }
        check(json() == beforeEdit && access.undoHistory.size() == history, "Cancel native node editor preserves the complete document");
        if (node) node->invoke(); dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        if (dialog) { dialog->getTextEditor("value")->setText("50"); dialog->exitModalState(1); pump(); }
        check(dialog && std::abs(access.engine.project.curve(address)->points[2].value - juce::Decibels::gainToDecibels(.5f)) < 1.e-5
            && access.undoHistory.size() == history + 1 && nodeAt(4) == node, "native exact value edit uses one existing Undo and retains time-bound identity");
        access.undo(false); check(json() == beforeEdit, "Undo restores native node edit exactly");
        node = nodeAt(4); if (node) node->activate();
        check(access.timeline.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey)) && !nodeAt(4)
            && access.engine.project.clips.size() == 2, "native selected-node Delete removes its node and keeps both segments");
        access.undo(false); check(json() == beforeEdit && nodeAt(4), "Undo recreates the same owner/time native node");
        access.engine.project.setTransitionLocked(second.id, true); access.refresh(); node = nodeAt(4);
        const auto locked = json(); if (node) node->invoke();
        check(node && node->read().contains("locked") && !juce::Component::getCurrentlyModalComponent() && json() == locked,
            "native node exact editor respects existing transition locks");
        access.engine.project.setTransitionLocked(second.id, false);
        access.engine.project.curve(address)->points = {{0, 0}, {4, 0}, {4, -12}, {16, -12}}; access.refresh();
        auto* beforeStep = nodeAt(4, 0); auto* afterStep = nodeAt(4, 1);
        check(beforeStep && afterStep && beforeStep != afterStep && beforeStep->getComponentID() != afterStep->getComponentID()
            && std::abs(beforeStep->value()) < 1.e-6 && std::abs(afterStep->value() + 12) < 1.e-6,
            "equal-time hard-cut endpoints have distinct native identities and correct values");
        access.timeline.pixelsPerSecond = 80; access.refresh(); access.viewport.setViewPosition(1200, 0); node = nodeAt(4, 1);
        auto children = access.timeline.getAccessibilityHandler()->getChildren(); handler = node ? node->getAccessibilityHandler() : nullptr;
        check(handler && handler->getParent() == access.timeline.getAccessibilityHandler()
            && std::find(children.begin(), children.end(), handler) != children.end(), "offscreen native node remains discoverable in its actual accessibility tree");
        if (handler) handler->getActions().invoke(juce::AccessibilityActionType::focus);
        check(access.viewport.getViewPositionX() < 650 && access.engine.position == position, "offscreen node focus reveals its actual position without seeking");
        check(capture(access, "accessible-node-1250.png"), "capture native node focus and Editing at compact size");
        auto* labelItem = dynamic_cast<TimelineAccessItem*>(access.timelineRuler.findChildWithID("label:" + marker.id));
        children = access.timelineRuler.getAccessibilityHandler()->getChildren(); handler = labelItem ? labelItem->getAccessibilityHandler() : nullptr;
        check(handler && handler->getTitle().contains("Bridge label") && handler->getParent() == access.timelineRuler.getAccessibilityHandler()
            && std::find(children.begin(), children.end(), handler) != children.end(),
            "offscreen named label remains discoverable on the actual ruler");
        if (handler) handler->getActions().invoke(juce::AccessibilityActionType::focus);
        const auto labelBefore = json(); const auto labelHistory = access.undoHistory.size();
        check(access.timeline.selectedLabel == marker.id && access.engine.position == position && labelItem
            && access.timelineRuler.getComponentAt(labelItem->getBounds().getCentre()) == &access.timelineRuler,
            "native label selection reveals it without seeking and preserves ruler pointer input");
        if (labelItem) labelItem->invoke(); dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        check(dialog && dialog->getName() == "Label marker" && dialog->getTextEditorContents("name") == marker.text
            && capture(*dialog, "accessible-label-editor.png"), "native label opens its existing named exact editor");
        if (dialog) { dialog->exitModalState(0); pump(); }
        check(json() == labelBefore && access.undoHistory.size() == labelHistory, "Cancel native label editor preserves the complete document");
        if (labelItem) labelItem->invoke(); dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        if (dialog) { dialog->getTextEditor("name")->setText("Bridge updated"); dialog->getTextEditor("time")->setText("32"); dialog->exitModalState(1); pump(); }
        check(dialog && labelItem && labelItem->read().contains("Bridge updated")
            && access.timelineRuler.findChildWithID("label:" + marker.id) == labelItem && access.undoHistory.size() == labelHistory + 1,
            "native label name/time edit keeps stable ID and uses one existing Undo");
        access.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey));
        check(!access.timelineRuler.findChildWithID("label:" + marker.id) && access.engine.project.clips.size() == 2,
            "Delete on native label selection retires that marker and keeps segments");
        access.undo(false); access.undo(false); check(json() == labelBefore, "Undo restores label removal and name/time edit exactly");
        access.setSize(1920, 1080); access.timeline.pixelsPerSecond = 30; access.refresh();
        labelItem = dynamic_cast<TimelineAccessItem*>(access.timelineRuler.findChildWithID("label:" + marker.id));
        if (labelItem) labelItem->getAccessibilityHandler()->getActions().invoke(juce::AccessibilityActionType::focus);
        check(capture(access, "accessible-label-1920.png"), "capture native label focus at wide size");
        auto& dense = *access.engine.project.curve(address); dense.points.clear();
        for (int i = 0; i < 1024; ++i) dense.setPoint(i / 64., -9);
        access.selectedClip = first.id; access.selectedAutomationTarget = 0; access.refresh();
        const auto denseBefore = json(); const auto denseHistory = access.undoHistory.size();
        access.timeline.selectAutomationPoint(address, 900);
        int nativeNodes = 0; for (auto* child : access.timeline.getChildren())
            if (auto* item = dynamic_cast<TimelineAccessItem*>(child); item && item->valueObject) ++nativeNodes;
        check(nativeNodes <= 512 && nodeAt(900 / 64.) && access.timeline.getDescription().contains("Editing Previous/Next"),
            "dense recorded curve bounds native children while retaining the selected distant node and navigation help");
        access.automationNext.onClick();
        check(nodeAt(901 / 64.) && access.automationNode.getText().contains("Node 902/1024")
            && json() == denseBefore && access.undoHistory.size() == denseHistory,
            "existing Editing navigation exposes every dense-curve node without editing data or history");
        access.removeFromDesktop();
    };
    accessibleMarkersAndNodes();
    auto accessibleTempoAndAmounts = [&]
    {
        auto owned = std::make_unique<Workstation>(false); auto& access = *owned; access.stopTimer(); access.setSize(1250, 900);
        auto pump = []
        {
            const auto until = juce::Time::getMillisecondCounterHiRes() + 40;
            do
            {
               #if JUCE_WINDOWS
                MSG message; while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
               #endif
                juce::Thread::sleep(1);
            } while (juce::Time::getMillisecondCounterHiRes() < until);
        };
        auto capture = [&](juce::Component& c, const juce::String& name)
        { auto out = report.getSiblingFile(name).createOutputStream(); juce::PNGImageFormat png; return out && png.writeImageToStream(c.createComponentSnapshot(c.getLocalBounds()), *out); };
        auto json = [&] { return juce::JSON::toString(projectToJson(access.engine.project), true, 17); };
        auto findItem = [](juce::Component& parent, const juce::String& prefix, const juce::String& text) -> TimelineAccessItem*
        {
            for (auto* c : parent.getChildren()) if (auto* item = dynamic_cast<TimelineAccessItem*>(c); item && item->getComponentID().startsWith(prefix) && item->read && item->read().contains(text)) return item;
            return nullptr;
        };
        auto media = std::make_shared<Media>(); media->name = "Native amount song"; media->sampleRate = 8000; media->manualBpm = 120;
        media->file = root.getChildFile("native-amount.wav"); media->samples.setSize(1, 8000 * 32); media->samples.clear();
        Clip first; first.media = media; first.start = 4; first.length = 16;
        first.controls.automation[0] = {true, {{0, -3, 1}, {4, -12}, {8, -2}}};
        Clip second = first; second.id = juce::Uuid().toString(); second.start = 12; second.track = 1; second.effects = makeClipEffects();
        initialiseTransition(first, second, 8, TransitionPreset::crossfade);
        first.fadeOut = second.fadeIn = 0;
        first.fadeOutAnchor = first.sourceElapsed(first.length); second.fadeInAnchor = second.sourceElapsed(0);
        access.engine.library = {media}; access.engine.project.clips = {first, second}; access.engine.project.linkClips(first.id, second.id);
        access.engine.project.tempo = {120, {{0, 120, false}, {12, 128, true, first.id}, {30, 132, false}}};
        access.selectedClip = first.id; access.selectedAutomationTarget = 0; access.timeline.pixelsPerSecond = 40;
        access.engine.position = 3; access.refresh(); access.addToDesktop(0); access.setVisible(true);
        const auto before = json(); const auto history = access.undoHistory.size();
        auto* tempo = findItem(access.tempoStrip, "tempo:", "128.0000 BPM");
        auto* parent = access.tempoStrip.getAccessibilityHandler(); auto children = parent->getChildren();
        check(tempo && parent->getRole() == juce::AccessibilityRole::list && tempo->getAccessibilityHandler()->getParent() == parent
            && std::find(children.begin(), children.end(), tempo->getAccessibilityHandler()) != children.end(), "native tempo marker is discoverable in the actual strip tree with the correct parent");
        const auto tempoKey = tempo ? tempo->getComponentID() : juce::String();
        if (tempo) tempo->getAccessibilityHandler()->getActions().invoke(juce::AccessibilityActionType::focus);
        auto* value = tempo ? tempo->getAccessibilityHandler()->getValueInterface() : nullptr;
        if (value) { value->setValue(200); value->setValueAsString("200"); }
        check(value && value->isReadOnly() && value->getCurrentValue() == 128 && value->getCurrentValueAsString().contains("BPM")
            && access.tempoStrip.selectedTempoPoint == 1 && access.engine.position == 3 && json() == before && access.undoHistory.size() == history,
            "native tempo focus selects without seeking/Undo; BPM value setters cannot mutate the document");
        if (tempo) tempo->invoke(); pump(); auto* dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        check(dialog && dialog->getName() == "Tempo marker" && dialog->getTextEditorContents("bpm").getDoubleValue() == 128
            && capture(*dialog, "accessible-tempo-editor.png"), "native tempo Enter opens the real BPM/time editor");
        if (dialog) { dialog->exitModalState(0); pump(); }
        check(json() == before && access.undoHistory.size() == history, "Cancel native tempo entry preserves the complete mix");
        if (tempo) tempo->invoke(); pump(); dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        access.engine.project.tempo.setPoint(6, 126); access.refresh(); const auto inserted = json();
        if (dialog) { dialog->getTextEditor("bpm")->setText("160"); dialog->exitModalState(1); pump(); }
        check(dialog && json() == inserted && access.undoHistory.size() == history && access.status.contains("project changed")
            && access.tempoStrip.findChildWithID(tempoKey) == tempo, "tempo identity survives earlier insertion; stale Apply rejects instead of editing a shifted index");
        access.engine.project.tempo.points[2].bpm = 129; access.refresh();
        check(tempo && access.tempoStrip.findChildWithID(tempoKey) == tempo && tempo->value() == 129, "segment-owned tempo marker keeps native identity and latest BPM after value edits");
        access.viewport.setViewPosition(0, 0); access.selectedClip = first.id; access.selectTrack(0); access.selectedAutomationTarget = 0; access.selectedTransition.clear(); access.refresh();
        auto* amount = findItem(access.timeline, "amount:", "after cut");
        parent = access.timeline.getAccessibilityHandler(); children = parent->getChildren();
        check(amount && amount->getAccessibilityHandler()->getParent() == parent && std::find(children.begin(), children.end(), amount->getAccessibilityHandler()) != children.end(),
            "visible manual cut amount is discoverable in the actual timeline tree");
        const auto amountKey = amount ? amount->getComponentID() : juce::String(); const auto beforeAmount = json();
        const juce::Component::SafePointer<TimelineAccessItem> stableAmount(amount);
        const int pageBeforeAmountFocus = access.toolPage, heightBeforeAmountFocus = access.timeline.laneSize(0);
        capture(access, "amount-focus-before.png");
        const auto stateText = [&]
        { return "clip=" + access.selectedClip + " target=" + juce::String(access.selectedAutomationTarget) + " lane=" + juce::String(access.engine.selectedTrack.load())
            + " view=" + access.viewport.getViewArea().toString(); };
        const auto stateBeforeFocus = stateText();
        if (amount) amount->getAccessibilityHandler()->getActions().invoke(juce::AccessibilityActionType::focus);
        amount = stableAmount.getComponent();
        auto* amountHandler = amount ? amount->getAccessibilityHandler() : nullptr;
        report.getSiblingFile("amount-focus-diagnostic.txt").replaceWithText("alive=" + juce::String((int) (amount != nullptr))
            + " handler=" + juce::String((int) (amountHandler != nullptr)) + " panel=" + juce::String(access.toolPage) + "/" + juce::String(pageBeforeAmountFocus)
            + " lane=" + juce::String(access.timeline.laneSize(0)) + "/" + juce::String(heightBeforeAmountFocus)
            + " showing=" + juce::String((int) (amount && amount->isShowing())) + " accessible=" + juce::String((int) (amount && amount->isAccessible()))
            + " key=" + amountKey + "\n" + (amount && amount->read ? amount->read() : juce::String("retired"))
            + "\nbefore " + stateBeforeFocus + "\nafter " + stateText());
        capture(access, "amount-focus-after.png");
        check(amountHandler && access.timeline.findChildWithID(amountKey) == amount && access.toolPage == pageBeforeAmountFocus
            && access.timeline.laneSize(0) == heightBeforeAmountFocus, "native amount focus preserves its live handler, panel and visible lane geometry");
        value = amountHandler ? amountHandler->getValueInterface() : nullptr;
        if (value) { value->setValue(10); value->setValueAsString("10"); }
        check(value && value->isReadOnly() && value->getCurrentValueAsString().contains("cut") && json() == beforeAmount
            && access.engine.position == 3 && access.undoHistory.size() == history, "native amount reports cut depth read-only and focus preserves sound/time/history");
        auto& curve = access.engine.project.clips[0].controls.automation[0]; curve.setPoint(1, -3); access.refresh();
        check(amount && access.timeline.findChildWithID(amountKey) == amount, "manual amount native identity survives an earlier automation-node insertion");
        if (amount) amount->invoke(); pump(); dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        check(dialog && dialog->getName() == "Hard-cut amount" && capture(*dialog, "accessible-amount-editor.png"), "native cut amount opens the original fixed-time precise editor");
        if (dialog) { dialog->getTextEditor("amount")->setText("25"); dialog->exitModalState(1); pump(); }
        check(amount && std::abs(amount->value() - 25) < .001 && access.undoHistory.size() == history + 1
            && access.engine.project.clips[0].controls.automation[0].points[2].time == 4, "native amount Apply uses one Undo and edits the bound endpoint without moving cut time");
        access.undo(false); check(access.engine.project.clips[0].controls.automation[0].points[2].value == -12, "native amount Undo restores the original endpoint");
        access.timeline.pixelsPerSecond = 20; access.viewport.setViewPosition(0, 0);
        access.selectedTransition = second.id; access.engine.project.setTransitionBand(second.id, 0, {TransitionBandMode::swap, .8}); access.refresh();
        auto* paired = findItem(access.timeline, "amount:", "paired amount");
        check(paired && std::abs(paired->value() - 80) < .001, "native generated paired amount reports the actual shared cut depth");
        access.engine.project.transitionLinks[0].locked = true; access.refresh(); const auto locked = json();
        if (paired) paired->invoke(); pump();
        check(paired && paired->read().contains("locked") && !juce::Component::getCurrentlyModalComponent() && json() == locked,
            "locked native amount remains readable and rejects the precise editor without mutation");
        tempo = findItem(access.tempoStrip, "tempo:", "129.0000 BPM"); if (tempo) tempo->invoke(); pump();
        check(tempo && !juce::Component::getCurrentlyModalComponent() && json() == locked, "protected tempo native entry also retains existing lock admission");
        for (int width : {1250, 1920})
        { access.setSize(width, 1080); access.refresh(); if (paired) paired->grabKeyboardFocus(); check(capture(access, "accessible-amount-" + juce::String(width) + ".png"), "capture native amount focus at width " + juce::String(width)); }
        access.engine.project.transitionLinks[0].locked = false; access.engine.project.tempo.points.clear();
        for (int i = 0; i < 1024; ++i) access.engine.project.tempo.points.push_back({(double) i, 120. + i % 7, false});
        access.tempoStrip.selectedTempoPoint = 900; access.refresh(); const auto dense = json();
        access.tempoStrip.keyPressed(juce::KeyPress(juce::KeyPress::rightKey));
        check(access.tempoStrip.getNumChildComponents() <= 256 && access.tempoStrip.selectedTempoPoint == 901
            && json() == dense && access.engine.position == 3 && access.viewport.getViewPositionX() > 0,
            "dense native tempo children stay bounded while arrows reveal and select every marker without seeking or editing");
        check(capture(access, "accessible-tempo-dense.png"), "capture offscreen tempo selection and actual focus");
        access.removeFromDesktop();
    };
    accessibleTempoAndAmounts();
    auto namedWorkspaceLayouts = [&]
    {
        auto owned = std::make_unique<Workstation>(false); auto& desk = *owned; desk.stopTimer(); desk.setSize(1920, 1080);
        const auto folder = root.getChildFile("named-layouts"); folder.createDirectory(); desk.workspacePreference = folder.getChildFile("workspace.json");
        auto pump = []
        {
            const auto until = juce::Time::getMillisecondCounterHiRes() + 40;
            do
            {
               #if JUCE_WINDOWS
                MSG message; while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
               #endif
                juce::Thread::sleep(1);
            } while (juce::Time::getMillisecondCounterHiRes() < until);
        };
        auto capture = [&](juce::Component& c, const juce::String& name)
        { auto out = report.getSiblingFile(name).createOutputStream(); juce::PNGImageFormat png; return out && png.writeImageToStream(c.createComponentSnapshot(c.getLocalBounds()), *out); };
        auto media = std::make_shared<Media>(); media->name = "Workspace musical state"; media->sampleRate = 8000; media->manualBpm = 120;
        media->file = folder.getChildFile("fixture.wav"); media->samples.setSize(1, 8000 * 24); media->samples.clear();
        Clip clip; clip.media = media; clip.length = 24; clip.controls.pan = .3f;
        desk.engine.project.clips = {clip}; desk.engine.library = {media}; desk.selectedClip = clip.id; desk.selectTrack(0);
        desk.engine.mappings = {{0xb0, 1, 7, {MidiActionKind::volume, 2}}};
        desk.engine.monitorMuted = desk.layout.monitorMuted = true; desk.engine.position = 2.5; desk.engine.playing = true;
        desk.dirty = false; desk.undoHistory.clear(); desk.redoHistory.clear();
        const auto song = juce::JSON::toString(projectToJson(desk.engine.project), true, 17);
        const auto unchanged = [&]
        {
            return song == juce::JSON::toString(projectToJson(desk.engine.project), true, 17) && !desk.dirty && desk.undoHistory.empty() && desk.redoHistory.empty()
                && desk.engine.playing && desk.engine.position == 2.5 && desk.engine.monitorMuted && desk.layout.monitorMuted
                && desk.selectedClip == clip.id && desk.engine.selectedTrack == 0 && desk.engine.mappings.size() == 1
                && desk.engine.mappings[0].number == 7 && desk.engine.mappings[0].action == MidiAction {MidiActionKind::volume, 2};
        };
        desk.addToDesktop(0); desk.setVisible(true); desk.layout.consoleOrder = {2, 0, 1}; desk.layout.inspectorCollapsed = true;
        desk.layout.gridBrightness = .28; desk.layout.followPlayhead = false; desk.timeline.pixelsPerSecond = 55;
        desk.showToolPage(2); desk.refresh(); desk.viewport.setViewPosition(240, 0);
        const auto preset = desk.captureNamedWorkspace("Editing room"); const auto file = folder.getChildFile("editing.alloyworkspace");
        check(writeNamedWorkspace(file, preset).wasOk(), "save an identified named visual workspace through shared preference encoding");
        const auto bytes = file.loadFileAsString(); NamedWorkspace loaded;
        check(readNamedWorkspace(file, loaded).wasOk() && loaded.name == "Editing room" && loaded.layout.consoleOrder == preset.layout.consoleOrder
            && loaded.zoom == 55 && loaded.view == preset.view && loaded.layout.inspectorCollapsed && !juce::JSON::parse(bytes).hasProperty("monitorMuted"),
            "named layout round-trips visual state/order/zoom/view and excludes the audio mute setting");
        check(writeNamedWorkspace(file, preset).failed() && file.loadFileAsString() == bytes, "named layout save never overwrites an existing file");
        desk.layout.consoleOrder = {0, 1, 2}; desk.layout.inspectorCollapsed = false; desk.timeline.pixelsPerSecond = 10; desk.showToolPage(0); desk.refresh();
        check(desk.restoreNamedWorkspace(file).wasOk() && desk.layout.consoleOrder == preset.layout.consoleOrder && desk.toolPage == 2 && desk.layout.inspectorCollapsed
            && desk.editingTab.getX() < desk.controllerTab.getX() && desk.timeline.pixelsPerSecond == 55 && unchanged(),
            "restore named layout reorders consoles and visual controls while preserving audio, MIDI, selection, live transport and musical history");
        desk.saveWorkspacePreferences(); WorkspaceLayout reopened;
        check(readWorkspacePreferences(desk.workspacePreference, reopened).wasOk() && reopened.consoleOrder == preset.layout.consoleOrder,
            "console order survives the existing local workspace preference reload");
        auto bad = juce::JSON::parse(bytes); bad.getDynamicObject()->setProperty("namedLayoutVersion", 99);
        const auto invalid = folder.getChildFile("future.alloyworkspace"); invalid.replaceWithText(juce::JSON::toString(bad));
        const auto beforeOrder = desk.layout.consoleOrder; loaded.name = "Sentinel";
        check(readNamedWorkspace(invalid, loaded).failed() && loaded.name == "Sentinel" && desk.restoreNamedWorkspace(invalid).failed()
            && desk.layout.consoleOrder == beforeOrder && unchanged(), "future named-layout data rejects atomically and retains the usable current workspace");
        for (const auto& wrong : {juce::var(juce::Array<juce::var> {0, 0, 2}), juce::var(juce::Array<juce::var> {0, 1}), juce::var(juce::Array<juce::var> {0, 1, 8})})
        { bad = juce::JSON::parse(bytes); bad.getDynamicObject()->setProperty("consoleOrder", wrong); invalid.replaceWithText(juce::JSON::toString(bad)); check(desk.restoreNamedWorkspace(invalid).failed() && unchanged(), "invalid console order cannot retarget a console or change musical data"); }
        desk.engine.exporting = true; check(desk.restoreNamedWorkspace(file).failed(), "export defers workspace restoration"); desk.engine.exporting = false;
        auto detached = preset; detached.name = "Detached desk"; detached.layout.inspectorCollapsed = false;
        detached.detached = {true, true, true}; for (auto& bounds : detached.windows) bounds = {50000, 50000, 900, 420};
        const auto detachedFile = folder.getChildFile("detached.alloyworkspace");
        check(writeNamedWorkspace(detachedFile, detached).wasOk() && desk.restoreNamedWorkspace(detachedFile).wasOk()
            && desk.performanceWindow && desk.transitionWindow && desk.editingWindow && unchanged(), "named workspace restores all three independent console windows without programme/controller mutations");
        bool reachable = true;
        for (const auto* window : {desk.performanceWindow.get(), desk.transitionWindow.get(), desk.editingWindow.get()})
        { const auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect(window->getBounds()); reachable &= display && display->userBounds.getLargestIntegerWithin().contains(window->getBounds()); }
        check(reachable, "saved offscreen console windows are constrained to a currently available display");
        check(desk.restoreNamedWorkspace(file).wasOk() && !desk.performanceWindow && !desk.transitionWindow && !desk.editingWindow && unchanged(),
            "a docked named layout returns all windows to their canonical console owners");
        desk.menuItemSelected(92, 4); pump(); auto* dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        check(dialog && dialog->getName() == "Workspace layouts" && capture(*dialog, "workspace-layout-manager.png"), "View menu opens the native named-layout manager");
        check(dialog && !dialog->getButton(1)->isEnabled() && dialog->getComboBoxComponent("layouts")->getNumItems() == 0,
            "empty named-layout manager explains the empty list and disables Restore without a destination");
        if (dialog) { dialog->getTextEditor("name")->setText("Native saved view"); dialog->exitModalState(1); pump(); }
        auto saved = folder.getChildFile("workspace-layouts").findChildFiles(juce::File::findFiles, false, "*.alloyworkspace");
        check(saved.size() == 1 && unchanged(), "native Save current creates one owned named layout without musical Undo");
        desk.layout.consoleOrder = {0, 1, 2}; desk.resized(); desk.menuItemSelected(92, 4); pump(); dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        if (dialog) { dialog->getComboBoxComponent("layouts")->setSelectedItemIndex(0); dialog->exitModalState(2); pump(); }
        check(saved.size() == 1 && desk.layout.consoleOrder == preset.layout.consoleOrder && unchanged(), "native Restore uses the selected saved file and preserves programme/MIDI state");
        desk.menuItemSelected(92, 4); pump(); dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        if (dialog) { dialog->exitModalState(0); pump(); }
        check(unchanged() && saved.size() == 1, "Close named-layout manager leaves saved and musical state unchanged");
        std::array<int, 3> order {0, 1, 2};
        do
        {
            desk.menuItemSelected(93, 4); pump(); dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
            if (dialog) { for (int i = 0; i < 3; ++i) dialog->getComboBoxComponent("position" + juce::String(i))->setSelectedItemIndex(order[(size_t) i]); dialog->exitModalState(1); pump(); }
            const std::array<juce::TextButton*, 3> buttons {&desk.controllerTab, &desk.transitionTab, &desk.editingTab};
            bool correct = desk.layout.consoleOrder == order;
            for (int i = 0; i < 3; ++i) { buttons[(size_t) order[(size_t) i]]->onClick(); correct &= desk.toolPage == order[(size_t) i]; if (i > 0) correct &= buttons[(size_t) order[(size_t) i - 1]]->getX() < buttons[(size_t) order[(size_t) i]]->getX(); }
            check(correct && unchanged(), "every native console permutation keeps the displayed order and canonical button commands");
        } while (std::next_permutation(order.begin(), order.end()));
        desk.layout.consoleOrder = {2, 1, 0}; desk.toolPage = 2; desk.layout.inspectorCollapsed = false;
        for (int width : {1250, 1920})
        { desk.setSize(width, 1080); desk.refresh(); check(capture(desk, "named-layout-" + juce::String(width) + ".png"), "capture reordered named workspace at width " + juce::String(width)); }
        desk.engine.setPlaying(false); desk.removeFromDesktop();
    };
    namedWorkspaceLayouts();
    auto editingConsole = [&]
    {
        auto owned = std::make_unique<Workstation>(false); auto& edit = *owned; edit.stopTimer();
        edit.workspacePreference = root.getChildFile("editing-workspace.json");
        auto pump = [](int ms)
        {
            const auto until = juce::Time::getMillisecondCounterHiRes() + ms;
            do
            {
               #if JUCE_WINDOWS
                MSG message; while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
               #endif
                juce::Thread::sleep(1);
            } while (juce::Time::getMillisecondCounterHiRes() < until);
        };
        auto media = std::make_shared<Media>(); media->name = "Editing console fixture";
        media->file = root.getChildFile("editing-fixture.wav"); media->sampleRate = 8000; media->samples.setSize(2, 8000 * 40); media->samples.clear(); media->manualBpm = 120; media->manualBeatOrigin = 0;
        Clip first; first.media = media; first.length = 16; first.track = 0;
        Clip second = first; second.id = juce::Uuid().toString(); second.start = 12; second.track = 1; second.gainDb = -3;
        edit.engine.library = {media}; edit.engine.project.clips = {first, second}; edit.engine.project.snap = false;
        edit.refresh(); edit.playlistTable.table.selectRow(0); edit.editingTab.onClick();
        check(edit.toolPage == 2 && edit.editingPanel.isVisible() && !edit.performancePanel.isVisible()
            && edit.clipSliders[0].getParentComponent() == &edit.editingBody && edit.pluginList.list.isVisible(), "Editing selects a third lower console while the right VST chain remains visible");
        check(edit.currentClip() && edit.currentClip()->id == first.id && edit.clipSliders[5].getValue() == 0, "playlist selection populates the relocated controls");
        for (int field = 0; field < 6; ++field)
        {
            const auto before = juce::JSON::toString(projectToJson(edit.engine.project));
            const double values[] {2, 1, 10, 1.25, 2.5, -6};
            auto& control = edit.clipSliders[(size_t) field]; control.onDragStart(); control.setValue(values[field], juce::sendNotificationSync); control.onDragEnd();
            const auto* c = edit.currentClip(); const double actual[] {c->start, c->offset, c->length, c->fadeIn, c->fadeOut, c->gainDb};
            check(std::abs(actual[field] - values[field]) < .001, "relocated clip field commits through its existing callback: " + juce::String(field));
            edit.undo(false);
            check(juce::JSON::toString(projectToJson(edit.engine.project)) == before, "Undo restores relocated field " + juce::String(field));
        }
        edit.reverseButton.setToggleState(true, juce::dontSendNotification); edit.reverseButton.onClick();
        check(edit.currentClip()->reversed, "relocated Reverse updates the selected clip"); edit.undo(false);
        edit.loopButton.onClick(); auto* dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        check(dialog && dialog->getName() == "Loop section", "relocated Loop opens its existing native section dialog");
        if (dialog) { dialog->exitModalState(1); pump(40); }
        check(edit.currentClip()->loopSpan == 2 && edit.currentClip()->length == 8, "Loop commits the selected four-beat section four times"); edit.undo(false);
        edit.playlistTable.table.selectRow(1);
        check(edit.currentClip()->id == second.id && edit.clipSliders[5].getValue() == -3 && edit.engine.selectedTrack == 1, "changing selection refreshes clip values and right-side lane ownership");
        edit.engine.project.linkClips(first.id, second.id); edit.engine.project.transitionLinks[0].locked = true; edit.refresh();
        bool disabled = !edit.reverseButton.isEnabled() && !edit.loopButton.isEnabled(); for (auto& c : edit.clipSliders) disabled &= !c.isEnabled();
        check(disabled, "transition locks disable all relocated editing controls");
        edit.engine.project.transitionLinks.clear(); edit.selectedClip.clear(); edit.refresh();
        check(!edit.clipSliders[0].isEnabled() && edit.clipDetail.getText().contains("Select a segment"), "empty selection remains explicit and disables Editing");
        edit.selectedClip = first.id; edit.selectTrack(0);
        const auto musical = juce::JSON::toString(projectToJson(edit.engine.project)); const auto history = edit.undoHistory.size();
        for (auto size : {juce::Point<int>{1250, 720}, juce::Point<int>{1250, 800}, juce::Point<int>{1920, 1080}})
        {
            edit.setSize(size.x, size.y); edit.resizeTimeline(0); edit.showToolPage(2);
            bool contained = true; for (auto& c : edit.clipSliders) contained &= edit.editingBody.getLocalBounds().contains(c.getBounds());
            check(contained && edit.editingBody.getLocalBounds().contains(edit.loopButton.getBounds()) && capture(edit, "editing-docked-" + juce::String(size.x) + ".png"), "Editing retains all controls with scrolling at width " + juce::String(size.x));
            edit.resizeTimeline(edit.layout.minimumTimelineHeight);
            bool visible = true;
            for (auto* control : edit.editingBody.getChildren())
                if (control->isVisible()) visible &= edit.editingViewport.getViewArea().contains(control->getBounds());
            check(visible && !edit.editingViewport.getVerticalScrollBar().isVisible()
                && !edit.editingViewport.getHorizontalScrollBar().isVisible()
                && musical == juce::JSON::toString(projectToJson(edit.engine.project)) && history == edit.undoHistory.size()
                && capture(edit, "editing-expanded-" + juce::String(size.x) + "x" + juce::String(size.y) + ".png"),
                "fully expanded Editing shows every control without scrolling or musical changes at " + juce::String(size.x) + "x" + juce::String(size.y));
        }
        edit.editSavedDownbeat(first.id); pump(40);
        auto* downbeatDialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        check(downbeatDialog && capture(*downbeatDialog, "save-downbeat-plain.png"), "capture the native Save downbeat dialog");
        if (downbeatDialog) { downbeatDialog->exitModalState(0); pump(40); }
        edit.editingPanel.dock.onClick();
        check(edit.editingWindow && edit.editingWindow->getContentComponent() == &edit.editingPanel && edit.clipSliders[0].getParentComponent() == &edit.editingBody, "detaching Editing moves the original controls into a native window");
        for (int width : {640, 1440})
        {
            edit.editingWindow->setSize(width, 400); edit.editingViewport.setViewPosition(0, 0);
            check(capture(*edit.editingWindow, "editing-detached-" + juce::String(width) + ".png") && edit.editingBody.getWidth() >= 600, "detached Editing resizes and captures at " + juce::String(width));
        }
        edit.editingWindow->setBounds(100, 100, 900, 420); edit.trackEditingWorkspace(); edit.saveWorkspacePreferences();
        edit.clipSliders[5].onDragStart(); edit.clipSliders[5].setValue(-9, juce::sendNotificationSync);
        check(edit.currentClip()->gainDb == -9, "detached gain uses the same selected clip and edit callback"); edit.undo(false);
        edit.setBusy(true); check(!edit.clipSliders[0].isEnabled() && !edit.loopButton.isEnabled(), "busy state disables Editing in its detached native window"); edit.setBusy(false); edit.refresh();
        WorkspaceLayout saved; check(readWorkspacePreferences(edit.workspacePreference, saved).wasOk() && saved.editingDetached && saved.editingWindowBounds == juce::Rectangle<int>(100, 100, 900, 420), "Editing detach, size and position persist in workspace preferences");
        edit.editingTab.onClick(); check(edit.editingWindow != nullptr, "detached tab focuses the existing Editing window");
        edit.editingWindow->closeButtonPressed(); pump(40);
        check(!edit.editingWindow && edit.editingPanel.getParentComponent() == &edit && edit.toolPage == 2, "native close asynchronously docks the original Editing panel");
        edit.layout = saved; edit.layout.editingWindowBounds.setPosition(-60000, -60000); edit.restoreEditingWorkspace();
        const auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect(edit.editingWindow->getBounds());
        check(display && display->userBounds.getLargestIntegerWithin().intersects(edit.editingWindow->getBounds()), "restored off-screen Editing bounds are constrained to an available display");
        edit.detachToolPanel(2, false); edit.editingTab.onClick(); edit.saveWorkspacePreferences();
        check(readWorkspacePreferences(edit.workspacePreference, saved).wasOk() && !saved.editingDetached && saved.toolPage == 2, "docked Editing tab is restored independently of musical session data");
        edit.layout.toolPage = 0; edit.restoreEditingWorkspace(); edit.layout = saved; edit.restoreEditingWorkspace();
        check(edit.toolPage == 2 && edit.editingPanel.isVisible(), "workspace restoration selects the saved docked Editing console");
        edit.keyboardButton.setToggleState(true, juce::dontSendNotification); edit.keyboardButton.onClick();
        check(edit.piano.isVisible() && edit.pluginList.list.isVisible() && edit.instrumentSetupButton.isVisible() && capture(edit, "editing-keyboard-vst.png"), "Keyboard and device setup remain reachable alongside the right VST chain");
        edit.instrumentSetupButton.onClick();
        check(edit.settingsWindow && edit.settingsWindow->getContentComponent() && edit.settingsWindow->getName() == "Audio, MIDI and FLkey 49", "relocated setup shortcut opens the existing Audio / MIDI panel"); edit.settingsWindow.reset();
        edit.showInspectorPage(0); check(edit.toolPage == 2 && edit.piano.isVisible(), "legacy clip-settings action opens Editing without hiding the keyboard");
        check(juce::JSON::toString(projectToJson(edit.engine.project)) == musical && edit.undoHistory.size() == history, "panel selection, docking, resizing and keyboard visibility preserve musical state and Undo");
        for (const auto& property : {juce::Identifier("toolPage"), juce::Identifier("editingDetached"), juce::Identifier("editingWindowBounds")})
        {
            auto doc = juce::JSON::parse(edit.workspacePreference); doc.getDynamicObject()->setProperty(property, "invalid");
            const auto invalid = root.getChildFile("invalid-editing-" + property.toString() + ".json"); invalid.replaceWithText(juce::JSON::toString(doc));
            WorkspaceLayout untouched; const auto original = invalid.loadFileAsString();
            check(readWorkspacePreferences(invalid, untouched).failed() && untouched.toolPage == 0 && !untouched.editingDetached
                && writeWorkspacePreferences(invalid, saved).failed() && original == invalid.loadFileAsString(), "malformed Editing preference is rejected atomically and retained: " + property.toString());
        }
        for (const auto& bounds : {juce::Array<juce::var>{0, 0, 100, 400}, juce::Array<juce::var>{0, 0, 900, -1}, juce::Array<juce::var>{0, 0, "900", 400}, juce::Array<juce::var>{0, 0, 900}})
        {
            auto doc = juce::JSON::parse(edit.workspacePreference); doc.getDynamicObject()->setProperty("editingWindowBounds", juce::var(bounds));
            const auto invalid = root.getChildFile("invalid-editing-size.json"); invalid.replaceWithText(juce::JSON::toString(doc)); WorkspaceLayout untouched;
            check(readWorkspacePreferences(invalid, untouched).failed() && !untouched.editingDetached, "invalid Editing bounds dimensions/types/count reject without partial restoration");
        }
        auto legacy = juce::JSON::parse(edit.workspacePreference);
        for (const char* property : {"toolPage", "editingDetached", "editingWindowBounds"}) legacy.getDynamicObject()->removeProperty(property);
        const auto old = root.getChildFile("legacy-editing-workspace.json"); old.replaceWithText(juce::JSON::toString(legacy));
        check(readWorkspacePreferences(old, saved).wasOk() && !saved.editingDetached && saved.toolPage == 0 && saved.editingWindowBounds.isEmpty(), "legacy workspaces retain a usable docked default");
    };
    editingConsole();
    {
        juce::Array<juce::var> controls; juce::StringArray missing;
        std::function<void(juce::Component&)> inventory = [&](juce::Component& component)
        {
            const bool control = dynamic_cast<juce::Button*>(&component) || dynamic_cast<juce::Slider*>(&component)
                || dynamic_cast<juce::ComboBox*>(&component) || dynamic_cast<juce::TextEditor*>(&component);
            if (control)
            {
                auto name = component.getTitle().isNotEmpty() ? component.getTitle() : component.getName();
                if (auto* button = dynamic_cast<juce::Button*>(&component)) name = button->getButtonText();
                const auto text = AlloyHints::textFor(component); auto* row = new juce::DynamicObject();
                row->setProperty("name", name); row->setProperty("hint", text); row->setProperty("enabled", component.isEnabled()); controls.add(juce::var(row));
                if (text.isEmpty()) missing.add(name);
            }
            for (auto* child : component.getChildren()) inventory(*child);
        };
        inventory(ui);
        ui.settings(); inventory(*ui.settingsWindow->getContentComponent()); ui.settingsWindow.reset();
        ui.masterPanelButton->onClick(); inventory(*ui.masterWindow->getContentComponent()); ui.masterWindow->closeButtonPressed();
        ui.editAnalysis(ui.selectedLibraryId);
        if (ui.gridWindow) { inventory(*ui.gridWindow->getContentComponent()); ui.gridWindow->closeButtonPressed(); }
        report.getSiblingFile("hint-inventory.json").replaceWithText(juce::JSON::toString(controls));
        check(missing.isEmpty(), "native main/docked/settings/master/source-grid control hint inventory has no gaps: " + missing.joinIntoString(", "));
        const juce::Rectangle<int> screen {0, 0, 1440, 940};
        for (auto point : {juce::Point<int>{1, 1}, {1439, 1}, {1, 939}, {1439, 939}, {720, 470}})
        {
            const auto bounds = ui.look.getTooltipBounds(AlloyHints::textFor(ui.masterSlider), point, screen);
            check(screen.contains(bounds) && !bounds.intersects(juce::Rectangle<int>(point.x - 16, point.y - 16, 32, 32)),
                "native popup stays on screen and clear of its hovered node at centre and monitor edges");
        }
        auto* focus = juce::Component::getCurrentlyFocusedComponent();
        ui.hints.displayTip({720, 470}, AlloyHints::textFor(ui.masterSlider));
        check(ui.hints.isVisible() && !ui.hints.getWantsKeyboardFocus() && juce::Component::getCurrentlyFocusedComponent() == focus
            && capture(ui.hints, "popup-hint.png"), "actual native hint displays with orange styling without taking keyboard focus");
        ui.menuItemSelected(86, 4);
        check(!ui.hints.isVisible(), "turning Popup hints off dismisses an already visible popup");
        ui.menuItemSelected(86, 4);
        const bool splitEnabled = ui.splitButton.isEnabled(); ui.splitButton.setEnabled(false);
        ui.splitButton.getProperties().set("mixworksHintDisabled", "Select an unlocked clip before splitting.");
        ui.hints.displayTip({720, 470}, AlloyHints::textFor(ui.splitButton));
        check(capture(ui.hints, "popup-hint-disabled.png"), "capture disabled-state explanation in the actual hint window");
        ui.hints.hideTip();
        ui.splitButton.setEnabled(splitEnabled); ui.splitButton.getProperties().remove("mixworksHintDisabled");
        check(capture(ui, "hints-workspace-1440.png"), "capture hint-enabled workspace with controls unobstructed");
    }
    auto hoverProbe = [&]
    {
        // No pointer movement or forced activation: the user may be working in another app.
        ui.addToDesktop(0); ui.setVisible(true);
        const bool foreground = juce::Process::isForegroundProcess();
        auto* facts = new juce::DynamicObject(); facts->setProperty("foreground", foreground);
        facts->setProperty("scope", "Native rendering, focus, background guard and menu resolution; foreground OS hover requires normal app use.");
        report.getSiblingFile("hover-facts.json").replaceWithText(juce::JSON::toString(juce::var(facts)));
        if (!foreground)
            check(ui.hints.getTipFor(ui.playButton).isEmpty(), "native background app suppresses automatic popup help");
        auto* focus = juce::Component::getCurrentlyFocusedComponent();
        ui.hints.displayTip({720, 470}, AlloyHints::textFor(ui.playButton));
        check(ui.hints.isVisible() && juce::Component::getCurrentlyFocusedComponent() == focus
            && capture(ui.hints, "popup-native.png"), "native popup renders the control explanation without moving keyboard focus");
        ui.menuItemSelected(86, 4);
        check(!ui.hints.isVisible() && ui.hints.getTipFor(ui.playButton).isEmpty(), "disabling hints immediately dismisses native help and rejects new hover queries");
        ui.menuItemSelected(86, 4);
        auto menu = ui.getMenuForIndex(4, "View"); menu.setLookAndFeel(&ui.look);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&ui.displayButton), [](int) {});
        juce::Component* hintRow = nullptr;
        std::function<void(juce::Component&)> findRow = [&](juce::Component& component)
        {
            if (auto* handler = component.getAccessibilityHandler())
                if (handler->getRole() == juce::AccessibilityRole::menuItem && handler->getTitle() == "Popup hints") hintRow = &component;
            for (auto* child : component.getChildren()) findRow(*child);
        };
        auto& desktop = juce::Desktop::getInstance();
        for (int i = 0; i < desktop.getNumComponents(); ++i) findRow(*desktop.getComponent(i));
        check(hintRow && AlloyHints::textFor(*hintRow).contains("Show or hide popup help"), "native menu rows expose explanatory hints through accessibility");
        if (hintRow)
        {
            ui.hints.displayTip(hintRow->getScreenBounds().getCentre(), AlloyHints::textFor(*hintRow));
            check(capture(ui.hints, "popup-menu-native.png") && capture(*hintRow->getTopLevelComponent(), "hints-view-menu.png"),
                "native View menu and its saved Popup hints switch render with explanatory help");
        }
        juce::PopupMenu::dismissAllActiveMenus(); ui.hints.hideTip(); ui.removeFromDesktop();
    };
    if (juce::JUCEApplicationBase::getCommandLineParameters().contains("--hint-probe")) { hoverProbe(); ui.workspaceDirty = false; return; }
    {
        auto ownedStackFixture_nav = std::make_unique<Workstation>(false); auto& nav = *ownedStackFixture_nav; nav.stopTimer(); nav.workspacePreference = root.getChildFile("follow-workspace.json");
        nav.engine.project = ui.engine.project;
        nav.engine.project.labels.push_back({juce::Uuid().toString(), 10800, "Long mix end"});
        nav.timeline.pixelsPerSecond = 40; nav.refresh();
        const auto state = juce::JSON::toString(projectToJson(nav.engine.project));
        const auto selected = nav.selectedClip; const auto undo = nav.undoHistory.size();
        auto visible = [&] { const int x = Clip::gridPixel(nav.engine.position.load(), nav.timeline.pixelsPerSecond); return x >= nav.viewport.getViewPositionX() && x < nav.viewport.getViewPositionX() + nav.viewport.getViewWidth(); };
        const juce::Point<float> point {123, 10};
        nav.viewport.setViewPosition(400, 0);
        nav.timelineRuler.mouseDown(event(nav.timelineRuler, point, point, false));
        const double expected = 523. / 40;
        check(!nav.engine.playing && std::abs(nav.engine.position.load() - expected) < 1e-9, "ruler single-click retains exact seeking through a scrolled viewport");
        nav.timelineRuler.mouseDoubleClick(event(nav.timelineRuler, point, point, false));
        check(nav.engine.playing && std::abs(nav.engine.position.load() - expected) < 1e-9, "ruler double-click starts playback at its visible time");
        nav.timelineRuler.mouseDoubleClick(event(nav.timelineRuler, point, point, false));
        check(nav.engine.playing, "double-click during playback remains a start action rather than toggling pause");
        nav.engine.setPlaying(false); nav.setBusy(true);
        nav.timelineRuler.mouseDoubleClick(event(nav.timelineRuler, {210, 10}, point, false));
        check(!nav.engine.playing && nav.engine.position.load() == expected, "busy ruler double-click leaves transport unchanged");
        nav.setBusy(false); nav.engine.exporting = true;
        nav.timelineRuler.mouseDown(event(nav.timelineRuler, {210, 10}, point, false));
        nav.timelineRuler.mouseDoubleClick(event(nav.timelineRuler, {210, 10}, point, false));
        check(!nav.engine.playing && nav.engine.position.load() == expected, "export blocks ruler seeking and double-click playback"); nav.engine.exporting = false;
        nav.engine.project.clips.front().pendingPlacement = PendingPlacement::drop;
        nav.timelineRuler.mouseDoubleClick(event(nav.timelineRuler, point, point, false));
        check(!nav.engine.playing, "ruler start retains the engine's pending-audio playback guard");
        nav.engine.project.clips = ui.engine.project.clips;
        check(!nav.layout.followPlayhead && !nav.followButton.getToggleState(), "Follow starts off and the visible toggle agrees");
        nav.engine.seek(7200); nav.timerCallback();
        check(nav.viewport.getViewPositionX() == 400, "disabled Follow permits inspecting an earlier window while seeking");
        nav.followButton.setToggleState(true, juce::dontSendNotification); nav.followButton.onClick();
        check(nav.layout.followPlayhead && visible(), "enabling Follow immediately reveals an offscreen playhead");
        const int firstView = nav.viewport.getViewPositionX();
        const juce::Point<float> edge {1, 10};
        nav.timelineRuler.mouseDown(event(nav.timelineRuler, edge, edge, false));
        nav.timelineRuler.mouseUp(event(nav.timelineRuler, edge, edge, false));
        const auto secondDown = juce::MouseEvent(source, edge, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1, 0, 0, 0, 0,
            &nav.timelineRuler, &nav.timelineRuler, juce::Time::getCurrentTime(), edge, juce::Time::getCurrentTime(), 2, false);
        nav.timelineRuler.mouseDown(secondDown); nav.timelineRuler.mouseDoubleClick(secondDown); nav.timelineRuler.mouseUp(secondDown);
        check(nav.engine.playing && nav.viewport.getViewPositionX() == firstView
            && std::abs(nav.engine.position.load() - (firstView + 1.) / nav.timeline.pixelsPerSecond) < 1e-9,
            "a complete ruler double-click near the view edge preserves its clicked time with Follow enabled");
        nav.engine.setPlaying(false);
        const int held = nav.viewport.getViewPositionX(); nav.timerCallback();
        check(nav.viewport.getViewPositionX() == held, "Follow holds the viewport while the playhead is already visible");
        nav.engine.setPlaying(true); nav.engine.position = 9000; nav.timerCallback();
        check(visible() && nav.layout.followPlayhead, "playing Follow pans a three-hour timeline without disabling itself");
        nav.engine.setPlaying(false); nav.engine.seek(200); nav.timerCallback();
        check(visible() && nav.layout.followPlayhead, "backward seek while paused follows the canonical transport position");
        nav.engine.project.tempo.setPoint(0, 120); nav.engine.project.tempo.setPoint(300, 150);
        nav.engine.seek(nav.engine.project.tempo.timeAtBeat(1600)); nav.timerCallback();
        check(visible(), "Follow uses seconds from the shared clock across tempo changes");
        nav.engine.project.tempo = ui.engine.project.tempo;
        nav.timeline.zoomAt(2, nav.viewport.getViewPositionX() + 20);
        check(nav.layout.followPlayhead && visible(), "zoom retains Follow and brings its playhead into the magnified view");
        nav.menuItemSelected(22, 4);
        check(nav.layout.followPlayhead && visible(), "Fit whole mix preserves Follow rather than treating its zoom as manual panning");
        nav.timeline.zoomAt(400, 0);
        for (const auto size : {juce::Point<int>{1250, 800}, juce::Point<int>{1920, 1080}})
        {
            nav.setSize(size.x, size.y); nav.timerCallback();
            check(visible() && nav.layout.followPlayhead && nav.followButton.isVisible()
                && nav.followButton.getWidth() >= 100 && nav.followButton.getBottom() <= nav.viewport.getY()
                && capture(nav, "follow-playhead-" + juce::String(size.x) + ".png"), "Follow remains readable and visible through resize at " + juce::String(size.x));
        }
        nav.performancePanel.dock.onClick(); nav.transitionPanel.dock.onClick();
        check(nav.layout.followPlayhead && visible(), "detaching the lower consoles retains Follow and its position");
        nav.performancePanel.dock.onClick(); nav.transitionPanel.dock.onClick();
        nav.saveWorkspacePreferences(); WorkspaceLayout saved;
        check(readWorkspacePreferences(nav.workspacePreference, saved).wasOk() && saved.followPlayhead, "Follow is saved in workspace preferences");
        nav.engine.seek(12000); nav.timerCallback();
        check(visible() && nav.layout.followPlayhead, "explicit seek past the current content extends only the timeline's view bounds");
        nav.viewport.getHorizontalScrollBar().setCurrentRangeStart(nav.viewport.getViewPositionX() + 80, juce::sendNotificationSync);
        check(!nav.layout.followPlayhead && !nav.followButton.getToggleState(), "manual scrollbar panning turns Follow off and updates the toggle");
        nav.setFollowPlayhead(true); nav.timelineOverview.keyPressed(juce::KeyPress(juce::KeyPress::leftKey));
        check(!nav.layout.followPlayhead, "keyboard overview panning disables Follow");
        nav.setFollowPlayhead(true); const auto preview = nav.timelineOverview.previewBounds(); const auto grab = preview.getCentre();
        nav.timelineOverview.mouseDown(event(nav.timelineOverview, grab, grab, false));
        check(!nav.layout.followPlayhead, "manual overview dragging disables Follow even at a view boundary");
        nav.setFollowPlayhead(true); juce::MouseWheelDetails wheel; wheel.deltaX = .3f;
        nav.timeline.mouseWheelMove(event(nav.timeline, point, point, false), wheel);
        check(!nav.layout.followPlayhead, "horizontal wheel panning disables Follow");
        nav.saveWorkspacePreferences();
        check(readWorkspacePreferences(nav.workspacePreference, saved).wasOk() && !saved.followPlayhead,
            "manual panning's Follow-off state persists");
        check(!nav.dirty && nav.undoHistory.size() == undo && nav.selectedClip == selected
            && juce::JSON::toString(projectToJson(nav.engine.project)) == state, "transport navigation and Follow preserve selection, musical state and Undo");
        const auto malformed = root.getChildFile("invalid-follow.json"); auto followDoc = juce::JSON::parse(nav.workspacePreference);
        followDoc.getDynamicObject()->setProperty("followPlayhead", "yes"); malformed.replaceWithText(juce::JSON::toString(followDoc));
        check(readWorkspacePreferences(malformed, saved).failed() && writeWorkspacePreferences(malformed, saved).failed(), "wrongly typed Follow preferences are retained without replacement");
    }
    {
        AlloyLookAndFeel look;
        for (const int width : {240, 800})
        {
            juce::Image image(juce::Image::RGB, width, 180, true);
            { juce::Graphics g(image); look.drawPopupMenuBackground(g, width, 180); }
            check(image.getPixelAt(0, 80) == colour::moltenOrange && image.getPixelAt(width - 1, 80) == colour::moltenOrange
                && image.getPixelAt(8, 8) == colour::panel, "popup menus draw orange edges around the unchanged graphite surface");
            { juce::Graphics g(image);
            look.drawPopupMenuItem(g, {2, 4, width - 4, 28}, false, true, true, false, false, "Open mix...", "Ctrl+O", nullptr, nullptr);
            look.drawPopupMenuItem(g, {2, 32, width - 4, 28}, false, true, false, true, false, "Follow playhead", {}, nullptr, nullptr);
            look.drawPopupMenuItem(g, {2, 60, width - 4, 28}, false, false, false, false, false, "Export mix as WAV...", {}, nullptr, nullptr);
            look.drawPopupMenuItem(g, {2, 88, width - 4, 28}, false, true, false, false, true, "Display", {}, nullptr, nullptr); }
            auto out = report.getSiblingFile("orange-menu-" + juce::String(width) + ".png").createOutputStream(); juce::PNGImageFormat png;
            check(out && png.writeImageToStream(image, *out), "capture compact and wide popup edge rendering");
            juce::AlertWindow alert("Mixworks", "Your mix is ready. Existing analysis and edits are kept.", juce::MessageBoxIconType::InfoIcon);
            alert.setLookAndFeel(&look); alert.addButton("OK", 1); alert.setSize(width + 120, 180);
            const auto rendered = alert.createComponentSnapshot(alert.getLocalBounds());
            check(rendered.getPixelAt(0, 80) == colour::moltenOrange && capture(alert, "orange-alert-" + juce::String(width) + ".png"), "real Alloy alerts keep orange edges and readable content");
            alert.setLookAndFeel(nullptr);
        }
        juce::FileBrowserComponent browser(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, root, nullptr, nullptr);
        juce::FileChooserDialogBox picker("Open mix", "Choose a saved mix", browser, false, colour::panel);
        picker.setLookAndFeel(&look);
        for (const int width : {420, 900})
        {
            picker.setSize(width, 480); const auto rendered = picker.createComponentSnapshot(picker.getLocalBounds());
            check(rendered.getPixelAt(0, 200) == colour::moltenOrange && capture(picker, "orange-picker-" + juce::String(width) + ".png"), "app file pickers retain their orange edge at compact and wide sizes");
        }
        picker.setLookAndFeel(nullptr);
    }
    for (const auto size : {juce::Point<int>{1440, 940}, juce::Point<int>{1250, 800}, juce::Point<int>{1920, 1080}, juce::Point<int>{2560, 1440}})
    {
        ui.setSize(size.x, size.y); ui.refresh();
        const auto retainedMix = juce::JSON::toString(projectToJson(ui.engine.project));
        const auto retainedUndo = ui.undoHistory.size();
        bool fittedControls = true;
        for (const int requested : {1, 100000})
        {
            ui.resizeLane(3, 100000);
            ui.resizeTimeline(requested);
            fittedControls &= ui.bottomSplitter.isVisible() && ui.bottomSplitter.isEnabled()
                && evenLanes(ui.layout)
                && !ui.viewport.getVerticalScrollBar().isVisible() && ui.viewport.getViewPositionY() == 0
                && ui.timeline.laneTop(trackCount) <= ui.viewport.getViewHeight();
            for (auto& lane : ui.strips)
                for (auto* control : std::initializer_list<juce::Component*> {&lane->select, &lane->mute, &lane->solo, &lane->gain, &lane->pan, &lane->cutoff})
                    fittedControls &= control->isVisible() && lane->getLocalBounds().contains(control->getBounds())
                        && ui.trackHeaders.getLocalBounds().contains(lane->getBounds());
            capture(ui, "four-lanes-docked-" + juce::String(size.x) + (requested == 1 ? "-minimum.png" : "-maximum.png"));
        }
        check(fittedControls && retainedMix == juce::JSON::toString(projectToJson(ui.engine.project)) && retainedUndo == ui.undoHistory.size(),
            "docked grip limits preserve four complete lanes without scrolling or musical edits at " + juce::String(size.x));
        ui.resizeTimeline(0);
        bool valid = ui.layout.library.getBottom() >= ui.layout.tempo.getBottom() && ui.layout.tempo.getY() == ui.layout.timeline.getBottom()
            && ui.layout.library.getRight() <= ui.layout.headers.getX()
            && ui.layout.timeline.getRight() + 8 == ui.layout.peakPanel.getX() && ui.layout.peakPanel.getRight() <= ui.layout.master.getX()
            && ui.layout.peakPanel.getY() == ui.layout.overview.getY() && ui.layout.peakPanel.getBottom() == ui.layout.tempo.getBottom()
            && (ui.layout.headers.getWidth() + 4 + ui.layout.timeline.getWidth()) >= std::min(WorkspaceLayout::minimumCentreWidth, size.x - 44 - 480) - WorkspaceLayout::peakPanelAllocation;
        for (auto* c : std::initializer_list<juce::Component*> {&ui.homeButton, &ui.playButton, &ui.stopButton, &ui.timeLabel, &ui.tempoSlider, &ui.snapButton, &ui.gridDimmer}) valid = valid && ui.layout.transport.contains(c->getBounds());
        for (auto& c : ui.clipSliders) valid = valid && c.getParentComponent() == &ui.editingBody && ui.editingBody.getLocalBounds().contains(c.getBounds());
        for (auto* c : {&ui.displayButton, &ui.windowModeButton, &ui.minimiseButton, &ui.closeAppButton})
            valid = valid && c->isVisible() && ui.layout.windowControls.contains(c->getBounds()) && !c->getBounds().intersects(ui.menuBar.getBounds());
        std::vector<juce::Component*> toolbarControls;
        for (auto* c : std::initializer_list<juce::Component*> {&ui.newButton, &ui.openButton, &ui.saveButton, &ui.importButton, &ui.exportButton, &ui.undoButton, &ui.redoButton, &ui.splitButton, &ui.duplicateButton, &ui.removeButton, &ui.beatmatchButton, &ui.waveformMode, &ui.settingsButton})
            if (c->isVisible()) { valid = valid && ui.layout.toolbar.contains(c->getBounds()); toolbarControls.push_back(c); }
        for (size_t i = 0; i < toolbarControls.size(); ++i) for (size_t j = i + 1; j < toolbarControls.size(); ++j)
            valid = valid && !toolbarControls[i]->getBounds().intersects(toolbarControls[j]->getBounds());
        valid = valid && ui.transitionLength.getParentComponent() == &ui.transitionBody && ui.transitionPreset.getParentComponent() == &ui.transitionBody
            && ui.layout.ruler.getY() == ui.layout.overview.getBottom() + 2 && ui.followButton.getBottom() <= ui.layout.timeline.getY();
        check(valid && capture(ui, "alloy-" + juce::String(size.x) + ".png"), "native Alloy layout and controls fit at " + juce::String(size.x));
    }
    check(ui.layout.ruler.getY() == ui.layout.overview.getBottom() + 2 && ui.waveformMode.isVisible(), "waveform view keeps the ruler directly beneath the overview");
    ui.transitionLength.setSelectedId(4, juce::sendNotificationSync);
    check(ui.transitionOverlapBeats() == 64, "bars dropdown retains the transition-length operation beside waveform display");
    auto lengthMenu = ui.getMenuForIndex(3, "Beatmatch");
    for (juce::PopupMenu::MenuItemIterator it(lengthMenu); it.next();)
        if (it.getItem().text == ui.transitionLength.getItemText(2)) ui.menuItemSelected(it.getItem().itemID, 3);
    check(ui.transitionLength.getSelectedId() == 3 && ui.transitionOverlapBeats() == 32, "Beatmatch menu and bars dropdown share the same transition choice");
    {
        const auto before = ui.engine.project;
        const auto ruler = ui.layout.ruler, lanes = ui.layout.timeline;
        const auto selected = ui.engine.project.clips.front().id; ui.selectedClip = selected; ui.engine.selectedTrack = 0;
        for (auto& clip : ui.engine.project.clips) clip.fadeIn = clip.fadeOut = 0;
        for (auto& link : ui.engine.project.transitionLinks) link.bands[0].reset();
        auto& curve = ui.engine.project.clips.front().controls.automation[0]; curve.enabled = true; curve.setPoint(0, -12); curve.setPoint(2, -6);
        ui.selectAutomationTarget(0);
        check(ui.layout.ruler == ruler && ui.layout.timeline == lanes && capture(ui, "alloy-automation.png"), "selecting an automation curve never adds a toolbar or shifts the timeline");
        auto find = [](const juce::PopupMenu& menu, TimelineAction action)
        {
            for (juce::PopupMenu::MenuItemIterator it(menu, true); it.next();) if (it.getItem().itemID == (int) action) return std::pair<bool, bool> {it.getItem().isEnabled, it.getItem().isTicked};
            return std::pair<bool, bool> {false, false};
        };
        const auto mixMenu = ui.getMenuForIndex(2, "Mix"), context = ui.timelineMenu(1, 0);
        check(find(mixMenu, TimelineAction::curveRead) == std::pair<bool, bool>{true, true}
            && find(context, TimelineAction::curveClear).first && find(context, TimelineAction::curvePoint).first,
            "Mix and timeline menus expose the same enabled curve actions and current Read state");
        ui.menuItemSelected((int) TimelineAction::curveRead, 2);
        check(!ui.engine.project.clips.front().controls.automation[0].enabled && ui.engine.project.clips.front().controls.automation[0].points.size() == 2,
            "Mix menu toggles Read without discarding automation points");
        ui.undo(false); check(ui.engine.project.clips.front().controls.automation[0].active(), "menu Read change is undoable");
        ui.performTimelineAction(TimelineAction::curveClear, 1, 0);
        check(ui.engine.project.clips.front().controls.automation[0].points.empty(), "context-menu Clear targets the selected clip curve");
        ui.undo(false); check(ui.engine.project.clips.front().controls.automation[0].points.size() == 2, "menu Clear restores all curve points with Undo");
        ui.menuItemSelected((int) TimelineAction::recordTouch, 2); ui.menuItemSelected((int) TimelineAction::armAutomation, 2);
        check(ui.engine.automationRecordMode == AutomationRecordMode::touch && ui.engine.automationArmed[0]
            && ui.layout.ruler == ruler && ui.layout.timeline == lanes, "advanced recording options remain in menus without revealing or reserving a strip");
        ui.menuItemSelected((int) TimelineAction::recordLatch, 2);
        check(ui.engine.automationRecordMode == AutomationRecordMode::write && find(ui.automationMenu(0), TimelineAction::recordLatch).second, "recording menu follows latch state");
        ui.menuItemSelected((int) TimelineAction::recordOff, 2); ui.engine.armAutomation(0, false);
        ui.selectAutomationTarget((int) AutomationTarget::plugin);
        check(find(ui.automationMenu(0), TimelineAction::choosePlugin).first && ui.layout.timeline == lanes, "VST target selection stays in the menu without adding UI");
        ui.selectAutomationTarget(0); ui.setBusy(true);
        const auto kept = juce::JSON::toString(projectToJson(ui.engine.project)); ui.performTimelineAction(TimelineAction::curveClear, 1, 0);
        check(!find(ui.automationMenu(0), TimelineAction::curveClear).first && juce::JSON::toString(projectToJson(ui.engine.project)) == kept, "busy menus and handlers cannot clear a curve");
        ui.setBusy(false); ui.engine.project = before; ui.selectedClip = selected; ui.selectAutomationTarget(-1);
        check(!find(ui.automationMenu(0), TimelineAction::curveRead).first && ui.layout.ruler == ruler, "without a selected curve, curve commands are disabled and layout remains fixed");
    }
    ui.setSize(1440, 940); ui.resizeSidePanel(true, 1000000);
    check((ui.layout.headers.getWidth() + 4 + ui.layout.timeline.getWidth()) == 620 && ui.layout.master.getWidth() == 270 && capture(ui, "alloy-wide-library.png"), "expanding library protects minimum timeline space and inspector width");
    const int preferred = ui.layout.preferredLibraryWidth; ui.setSize(1250, 800); ui.setSize(1440, 940);
    check(ui.layout.library.getWidth() == preferred, "window shrinking and growing restores preferred panel width");
    ui.leftSplitter.keyPressed(juce::KeyPress(juce::KeyPress::homeKey));
    check(capture(ui, "alloy-150.png", 1.5f) && capture(ui, "alloy-200.png", 2), "native Alloy vector controls render at 150 and 200 percent scale");
    ui.dirty = false; const auto laneUndo = ui.undoHistory.size(); const auto mix = juce::JSON::toString(projectToJson(ui.engine.project));
    check(ui.masterPanelButton && ui.followButton.isVisible() && ui.timeline.contentLaneCount() == 4
        && !ui.dirty && ui.undoHistory.size() == laneUndo && juce::JSON::toString(projectToJson(ui.engine.project)) == mix,
        "the fixed four-lane workspace has no extra-lane selector or hidden lanes");
    const auto laneState = juce::JSON::toString(projectToJson(ui.engine.project));
    const auto resizeUndo = ui.undoHistory.size(); ui.dirty = false;
    const auto rulerBounds = ui.timelineRuler.getBounds();
    check(!ui.viewport.getVerticalScrollBar().isVisible() && !ui.viewport.getHorizontalScrollBar().isVisible()
          && ui.timeline.getHeight() <= ui.viewport.getViewHeight(), "four default lanes fit without timeline scrollbars");
    const int defaultHeight = ui.layout.defaultLaneHeight;
    bool compactControls = true;
    for (const int height : {-1000, 6, 30, 75, 100, 132, 264, 100000})
    {
        ui.resizeLane(0, height);
        for (int lane = 0; lane < trackCount; ++lane)
        {
            auto& strip = *ui.strips[(size_t) lane];
            const std::array<juce::Component*, 6> controls {&strip.select, &strip.mute, &strip.solo, &strip.cutoff, &strip.pan, &strip.gain};
            compactControls &= strip.getHeight() >= WorkspaceLayout::minimumLaneHeight;
            for (size_t i = 0; i < controls.size(); ++i)
            {
                compactControls &= controls[i]->isVisible() && strip.getLocalBounds().contains(controls[i]->getBounds());
                for (size_t j = i + 1; j < controls.size(); ++j)
                    compactControls &= !controls[i]->getBounds().intersects(controls[j]->getBounds());
            }
        }
        compactControls &= !ui.viewport.getVerticalScrollBar().isVisible()
            && ui.timeline.laneTop(trackCount) <= ui.viewport.getViewHeight();
    }
    check(compactControls, "lane resizing redistributes bounded space with all four complete control sets visible and no scrolling");
    ui.resizeLane(0, defaultHeight);
    const juce::Point<float> edge {80, (float) ui.timeline.laneTop(1) - 1}, taller {80, edge.y + defaultHeight};
    ui.timeline.mouseDown(event(ui.timeline, edge, edge, false)); ui.timeline.mouseDrag(event(ui.timeline, taller, edge, true)); ui.timeline.mouseUp(event(ui.timeline, taller, edge, true));
    check(ui.timeline.laneSize(0) > defaultHeight && ui.timeline.laneSize(1) >= WorkspaceLayout::minimumLaneHeight
        && !ui.viewport.getVerticalScrollBar().isVisible(), "native lane boundary drag shares the existing area instead of adding vertical overflow");
    ui.viewport.setViewPosition(0, 70);
    check(ui.viewport.getViewPositionY() == 0 && ui.timelineRuler.getBounds().getY() == rulerBounds.getY()
        && ui.timelineRuler.getParentComponent() == &ui && ui.timelineOverview.getParentComponent() == &ui,
        "vertical scrolling cannot hide lanes; ruler and overview remain fixed");
    ui.resizeLane(0, -1000);
    check(ui.timeline.laneSize(0) == WorkspaceLayout::minimumLaneHeight && ui.strips[0]->cutoff.isVisible()
        && ui.strips[0]->pan.isVisible(), "a lane cannot collapse away its controls");
    ui.strips[0]->keyPressed(juce::KeyPress(juce::KeyPress::homeKey));
    check(std::abs(ui.timeline.laneSize(0) - defaultHeight) <= 1, "keyboard Home restores the lane's share of available height");
    ui.saveWorkspacePreferences(); WorkspaceLayout restoredSizes;
    bool restoredLaneGeometry = readWorkspacePreferences(preference, restoredSizes).wasOk();
    restoredSizes.update(ui.getWidth(), ui.getHeight(), !ui.consoleCollapsed);
    for (int lane = 0; lane < trackCount; ++lane)
        restoredLaneGeometry &= std::abs(restoredSizes.laneSize(lane) - ui.layout.laneSize(lane)) <= 1;
    check(restoredLaneGeometry, "saved lane sizing reopens within one pixel after JSON numeric rounding");
    check(capture(ui, "alloy-resized-lanes.png"), "capture the bounded four-lane layout");
    ui.layout.laneScale.fill(1); ui.resized(); ui.viewport.setViewPosition(0, 0);
    ui.timeline.pixelsPerSecond = 80; ui.timeline.updateSize();
    const auto preview = ui.timelineOverview.previewBounds();
    const juce::Point<float> grab {preview.getCentreX(), preview.getCentreY()}, slide {grab.x + 70, grab.y};
    ui.timelineOverview.mouseDown(event(ui.timelineOverview, grab, grab, false)); ui.timelineOverview.mouseDrag(event(ui.timelineOverview, slide, grab, true)); ui.timelineOverview.mouseUp(event(ui.timelineOverview, slide, grab, true));
    check(ui.viewport.getViewPositionX() > 0 && ui.timelineOverview.previewBounds().getX() > 0,
          "dragging the overview preview pans the actual timeline");
    ui.timelineOverview.keyPressed(juce::KeyPress(juce::KeyPress::endKey));
    check(ui.viewport.getViewPositionX() == ui.timeline.getWidth() - ui.viewport.getViewWidth(), "overview End reaches the final visible window");
    ui.timelineOverview.keyPressed(juce::KeyPress(juce::KeyPress::homeKey));
    check(ui.viewport.getViewPositionX() == 0 && !ui.viewport.getVerticalScrollBar().isVisible() && !ui.dirty
          && ui.undoHistory.size() == resizeUndo && juce::JSON::toString(projectToJson(ui.engine.project)) == laneState,
          "reset lane sizes hide scrolling and preview navigation preserves the entire musical state");
    check(capture(ui, "alloy-navigator.png"), "capture the fixed ruler and draggable overview");
    auto overviewAndInspector = [&]
    {
        const auto originalSize = ui.getLocalBounds();
        const auto document = juce::JSON::toString(projectToJson(ui.engine.project)); const auto history = ui.undoHistory.size(); const bool dirty = ui.dirty;
        for (int edge : {-1, 1})
        {
            ui.setFollowPlayhead(true);
            ui.timeline.pixelsPerSecond = 40; ui.timeline.updateSize(); ui.viewport.setViewPosition(600, 0);
            const double start = ui.viewport.getViewPositionX() / 40., end = (ui.viewport.getViewPositionX() + ui.viewport.getViewWidth()) / 40.;
            const auto preview = ui.timelineOverview.previewBounds();
            const juce::Point<float> down {edge < 0 ? preview.getX() : preview.getRight(), preview.getCentreY()};
            const auto to = down.translated(edge < 0 ? 4.f : -4.f, 0);
            ui.timelineOverview.mouseDown(event(ui.timelineOverview, down, down, false));
            ui.timelineOverview.mouseDrag(event(ui.timelineOverview, to, down, true)); ui.timelineOverview.mouseUp(event(ui.timelineOverview, to, down, true));
            const double nextStart = ui.viewport.getViewPositionX() / ui.timeline.pixelsPerSecond;
            const double nextEnd = nextStart + ui.viewport.getViewWidth() / ui.timeline.pixelsPerSecond;
            check(ui.timeline.pixelsPerSecond > 40 && std::abs((edge < 0 ? nextEnd - end : nextStart - start)) <= 1 / ui.timeline.pixelsPerSecond && !ui.layout.followPlayhead,
                "P53 actual " + juce::String(edge) + " edge gesture zooms with its opposite time anchored and Follow off");
        }
        ui.timeline.pixelsPerSecond = 800; ui.timeline.updateSize(); ui.viewport.setViewPosition(6000, 0);
        const auto narrow = ui.timelineOverview.previewBounds(); const auto down = narrow.getCentre(); const auto to = down.translated(12, 0);
        ui.timelineOverview.mouseDown(event(ui.timelineOverview, down, down, false)); ui.timelineOverview.mouseDrag(event(ui.timelineOverview, to, down, true)); ui.timelineOverview.mouseUp(event(ui.timelineOverview, to, down, true));
        check(ui.timeline.pixelsPerSecond == 800 && ui.viewport.getViewPositionX() != 6000, "P53 narrow viewbox body remains a pan target between distinct edges");
        ui.timeline.pixelsPerSecond = 40; ui.timeline.updateSize(); ui.viewport.setViewPosition(600, 0);
        double start = ui.viewport.getViewPositionX() / 40., end = (ui.viewport.getViewPositionX() + ui.viewport.getViewWidth()) / 40.;
        ui.timelineOverview.keyPressed(juce::KeyPress(juce::KeyPress::leftKey, juce::ModifierKeys::shiftModifier, 0));
        check(ui.timeline.pixelsPerSecond < 40 && std::abs((ui.viewport.getViewPositionX() + ui.viewport.getViewWidth()) / ui.timeline.pixelsPerSecond - end) <= 1 / ui.timeline.pixelsPerSecond, "P53 Shift+Left resizes the left edge with fixed right time");
        start = ui.viewport.getViewPositionX() / ui.timeline.pixelsPerSecond;
        ui.timelineOverview.keyPressed(juce::KeyPress(juce::KeyPress::rightKey, juce::ModifierKeys::ctrlModifier, 0));
        check(std::abs(ui.viewport.getViewPositionX() / ui.timeline.pixelsPerSecond - start) <= 1 / ui.timeline.pixelsPerSecond, "P53 Ctrl+Right resizes the right edge with fixed left time");
        ui.setSize(1920, 1080); ui.resizeSidePanel(false, 330); const int expandedWidth = ui.layout.master.getWidth(), timelineWidth = ui.viewport.getViewWidth();
        const juce::Point<float> dividerDown {5, 20}, dividerTo {5 + (float) expandedWidth, 20};
        ui.rightSplitter.mouseDown(event(ui.rightSplitter, dividerDown, dividerDown, false));
        ui.rightSplitter.mouseDrag(event(ui.rightSplitter, dividerTo, dividerDown, true)); ui.rightSplitter.mouseUp(event(ui.rightSplitter, dividerTo, dividerDown, true));
        check(ui.layout.inspectorCollapsed && ui.layout.master.getWidth() == 0 && ui.viewport.getViewWidth() == timelineWidth + expandedWidth
            && !ui.masterLabel.isVisible() && !ui.monitorMute.isVisible() && !ui.pluginList.list.isVisible() && !ui.instrumentSetupButton.isVisible()
            && !ui.piano.isVisible() && ui.masterSlider.isVisible() && ui.rightSplitter.isVisible(), "P56 whole right inspector collapses; centre reclaims its width and master/restore grip remain");
        const auto file = report.getSiblingFile("p56-workspace.json"); WorkspaceLayout loaded;
        check(writeWorkspacePreferences(file, ui.layout).wasOk() && readWorkspacePreferences(file, loaded).wasOk() && loaded.inspectorCollapsed && loaded.preferredInspectorWidth == 330,
            "P56 collapsed preference and previous expanded width survive reopening");
        auto legacy = juce::JSON::parse(file); legacy.getDynamicObject()->removeProperty("inspectorCollapsed"); file.replaceWithText(juce::JSON::toString(legacy));
        check(readWorkspacePreferences(file, loaded).wasOk() && !loaded.inspectorCollapsed, "P56 older workspace without collapsed state opens expanded");
        legacy.getDynamicObject()->setProperty("inspectorCollapsed", "yes"); file.replaceWithText(juce::JSON::toString(legacy)); const auto malformed = file.loadFileAsString();
        check(readWorkspacePreferences(file, loaded).failed() && writeWorkspacePreferences(file, ui.layout).failed() && file.loadFileAsString() == malformed, "P56 malformed collapsed preference is preserved and rejected");
        for (int width : {1000, 1920})
        {
            ui.setSize(width, width == 1000 ? 620 : 1080); ui.resized();
            check(ui.layout.master.getWidth() == 0 && ui.rightSplitter.getBounds().getRight() <= width && ui.masterPanelButton->isVisible(), "P56 compact/wide resize retains zero inspector width and master access");
            check(capture(ui, "p53-p56-collapsed-" + juce::String(width) + ".png"), "capture overview edge grips and collapsed inspector " + juce::String(width));
        }
        ui.setSize(1920, 1080); ui.rightSplitter.keyPressed(juce::KeyPress(juce::KeyPress::returnKey));
        check(!ui.layout.inspectorCollapsed && ui.layout.master.getWidth() == expandedWidth && ui.pluginList.list.isVisible(), "P56 Enter restores exact previous expanded width and hidden controls");
        ui.rightSplitter.keyPressed(juce::KeyPress(juce::KeyPress::endKey));
        check(ui.layout.inspectorCollapsed, "P56 keyboard End collapses the entire inspector");
        ui.rightSplitter.mouseDoubleClick(event(ui.rightSplitter, {5, 20}, {5, 20}, false));
        check(!ui.layout.inspectorCollapsed && ui.layout.master.getWidth() == expandedWidth, "P56 double-click restores collapsed inspector");
        check(document == juce::JSON::toString(projectToJson(ui.engine.project)) && ui.undoHistory.size() == history && ui.dirty == dirty, "P53/P56 view changes preserve musical document, dirty flag and Undo");
        ui.setSize(originalSize.getWidth(), originalSize.getHeight()); ui.resizeSidePanel(false, WorkspaceLayout::defaultInspectorWidth);
    };
    overviewAndInspector();
    ui.timeline.pixelsPerSecond = 40; ui.timeline.updateSize(); ui.viewport.setViewPosition(300, 0);
    const auto gridMix = juce::JSON::toString(projectToJson(ui.engine.project));
    const auto gridHistory = ui.undoHistory.size(); const auto gridDirty = ui.dirty;
    const auto gridView = ui.viewport.getViewPositionX();
    ui.gridDimmer.setValue(0.2, juce::sendNotificationSync);
    check(ui.timeline.pixelsPerSecond == 40 && ui.viewport.getViewPositionX() == gridView && ui.layout.gridBrightness == 0.2
        && ui.gridDimmer.getTitle() == "Grid brightness" && ui.workspaceDirty, "grid dimmer changes only the saved background brightness");
    ui.timeline.zoomAt(2, ui.viewport.getViewPositionX() + 50);
    check(ui.timeline.pixelsPerSecond == 80 && ui.gridDimmer.getValue() == 0.2, "timeline zoom remains independent of the dimmer");
    ui.menuItemSelected(22, 4);
    check(ui.viewport.getViewPositionX() == 0 && ui.gridDimmer.getValue() == 0.2
        && juce::JSON::toString(projectToJson(ui.engine.project)) == gridMix && ui.undoHistory.size() == gridHistory && ui.dirty == gridDirty,
        "Fit whole mix retains dimmer brightness and all musical state");
    ui.saveWorkspacePreferences(); WorkspaceLayout gridSaved;
    check(readWorkspacePreferences(preference, gridSaved).wasOk() && gridSaved.gridBrightness == 0.2, "grid brightness reopens from workspace preferences");
    for (const auto bad : juce::StringArray {"bright", "-0.1", "1.1"})
    {
        auto gridDoc = juce::JSON::parse(preference); gridDoc.getDynamicObject()->setProperty("gridBrightness", bad == "bright" ? juce::var(bad) : juce::var(bad.getDoubleValue()));
        const auto invalid = root.getChildFile("bad-grid.json"); const auto bytes = juce::JSON::toString(gridDoc); invalid.replaceWithText(bytes);
        WorkspaceLayout unchanged;
        check(readWorkspacePreferences(invalid, unchanged).failed() && writeWorkspacePreferences(invalid, ui.layout).failed()
            && unchanged.gridBrightness == 0.45 && invalid.loadFileAsString() == bytes, "invalid grid brightness is rejected without changing preferences");
    }
    {
        auto ownedStackFixture_grid = std::make_unique<Workstation>(false); auto& grid = *ownedStackFixture_grid; grid.stopTimer(); grid.setSize(1920, 1080); grid.engine.project.bpm = 120; grid.engine.project.followTrackTempo = false;
        grid.timeline.pixelsPerSecond = 40; grid.timeline.updateSize(); grid.setFollowPlayhead(true);
        const auto dim = grid.timeline.createComponentSnapshot(grid.timeline.getLocalBounds().withWidth(400));
        grid.gridDimmer.setValue(1, juce::sendNotificationSync);
        const auto bright = grid.timeline.createComponentSnapshot(grid.timeline.getLocalBounds().withWidth(400));
        grid.gridDimmer.setValue(0, juce::sendNotificationSync);
        const auto off = grid.timeline.createComponentSnapshot(grid.timeline.getLocalBounds().withWidth(400));
        check(bright.getPixelAt(80, 15) != dim.getPixelAt(80, 15) && dim.getPixelAt(80, 15) != off.getPixelAt(80, 15)
            && bright.getPixelAt(20, 15) != bright.getPixelAt(80, 15) && off.getPixelAt(80, 15) == off.getPixelAt(81, 15),
            "native grid pixels establish subtle/default, bright and off values with distinct beat/bar guides");
        check(grid.layout.followPlayhead && grid.followButton.getToggleState(), "dimming retains Follow and its visible toggle");
    }
    ui.resizeLane(0, 2 * ui.layout.defaultLaneHeight);
    check(capture(*ui.strips[0], "alloy-lane-controller.png"), "capture reference-inspired lane controls with LPF, pan, volume, mute and solo");
    ui.strips[0]->cutoff.setValue(lowPassToCutoff(1800), juce::sendNotificationSync);
    check(ui.engine.project.tracks[0].filterCutoff == lowPassToCutoff(1800), "lane LPF operates immediately without arming or MIDI mapping");
    ui.resizeLane(0, ui.layout.defaultLaneHeight);
    ui.workspaceDirty = false;
    // Exercise the actual dock, shared Engine ownership and recording callbacks.
    ui.selectAutomationTarget(-1); ui.setSize(1920, 1080); ui.selectedClip.clear(); ui.engine.selectedClip.clear(); ui.refresh();
    auto& desk = *ui.performance;
    ui.selectedClip = ui.engine.project.clips.front().id; ui.engine.selectedClip = ui.selectedClip; ui.refresh(); desk.pots[0].setValue(-17, juce::sendNotificationSync); desk.refresh(true);
    check(ui.engine.project.clips.front().controls.eqDb[0] == -17 && desk.pots[0].getValue() == -17, "performance display and input resolve the same selected-clip control owner");
    ui.selectedClip.clear(); ui.engine.selectedClip.clear(); ui.refresh();
    check(ui.layout.performance.getBottom() == ui.layout.status.getY() - 4 && ui.layout.performance.getY() >= ui.layout.tempo.getBottom()
          && ui.performancePanel.getBounds() == ui.layout.performance.withTrimmedTop(32) && desk.getWidth() <= ui.performanceViewport.getWidth(),
          "live desk fills the lower centre at 1080p without overlapping tempo, inspector or status");
    for (int i = 0; i < 8; ++i)
    {
        const float value = i < 6 ? -5.f - i : (i == 6 ? -.5f : .5f);
        desk.pots[(size_t) i].setValue(value, juce::sendNotificationSync);
        desk.faders[(size_t) i].setValue(i < 4 ? -12 - i : .1 * (i - 3), juce::sendNotificationSync);
        const auto& t = ui.engine.controlSettings(i % 2);
        const auto& lane = ui.engine.controlSettings(i % 4);
        check((i < 6 ? std::abs(t.eqDb[(size_t) (i / 2)] - value) < .001 : t.filterCutoff == value)
            && (i < 4 ? lane.gainDb == -12 - i : std::abs(lane.pan - .1 * (i - 3)) < .001),
            "routed control " + juce::String(i) + " EQ/LPF=" + juce::String(i < 6 ? t.eqDb[(size_t) (i / 2)] : t.filterCutoff) + " fader=" + juce::String(i < 4 ? lane.gainDb : lane.pan));
    }
    desk.master.setValue(-7, juce::sendNotificationSync);
    check(ui.engine.project.masterDb == -7, "master fader updates its production owner without arming");
    bool routing = true;
    for (int i = 0; i < 8; ++i)
    {
        const int partner = ui.engine.deskLanes[(size_t) (i ^ 1)], original = ui.engine.deskLanes[(size_t) i];
        std::set<int> visited;
        for (int step = 0; step < trackCount - 1; ++step) { desk.pads[(size_t) i].onClick(); visited.insert(ui.engine.deskLanes[(size_t) i]); }
        routing &= visited.size() == trackCount - 1 && !visited.count(partner) && ui.engine.deskLanes[(size_t) i] == original;
    }
    check(routing, "each top pad cycles all three eligible lanes without changing or duplicating its partner");
    auto& readCurve = ui.engine.project.tracks[0].automation[1]; readCurve.enabled = true; readCurve.setPoint(0, -30); readCurve.setPoint(2, -20);
    desk.pots[0].setValue(-11, juce::sendNotificationSync);
    check(!readCurve.enabled && readCurve.points.size() == 2 && ui.engine.automationValue({0, AutomationTarget::bass}, 1) == -11,
          "an unarmed direct move is audible immediately and retains the previous Read curve points");
    ui.undo(false); check(ui.engine.project.tracks[0].automation[1].active(), "Undo restores the previous Read curve as well as its manual value");
    {
        Engine filter({}, false); auto media = std::make_shared<Media>(); media->sampleRate = 48000; media->samples.setSize(2, 48000); media->file = root.getChildFile("filter-fixture.wav"); media->name = "Filter fixture";
        for (int i = 0; i < 48000; ++i) for (int ch = 0; ch < 2; ++ch) media->samples.setSample(ch, i, .2f * (float) std::sin(i * juce::MathConstants<double>::twoPi * 8000 / 48000));
        Clip c; c.media = media; c.length = 1; filter.project.clips = {c}; filter.sampleRate = 48000; filter.project.masterDb = 0;
        auto render = [&](int block)
        {
            filter.resetPlugins(); juce::MidiBuffer midi; juce::AudioBuffer<float> buffer(2, block); std::vector<float> samples;
            for (int offset = 0; offset < 48000; offset += block)
            {
                const int count = std::min(block, 48000 - offset); filter.render(buffer, count, offset / 48000., midi);
                for (int i = 0; i < count; ++i) samples.push_back(buffer.getSample(0, i));
            }
            return samples;
        };
        const auto dry = render(173); bool bypass = true;
        for (int i = 0; i < 48000; ++i) bypass &= std::abs(dry[(size_t) i] - media->samples.getSample(0, i)) < 1e-6;
        check(bypass, "fully open clip and lane low-pass filters preserve the dry PCM");
        filter.project.clips[0].controls.filterCutoff = lowPassToCutoff(800); const auto clipWet = render(173);
        double dryEnergy = 0, wetEnergy = 0; for (size_t i = 4800; i < dry.size(); ++i) { dryEnergy += dry[i] * dry[i]; wetEnergy += clipWet[i] * clipWet[i]; }
        check(wetEnergy < dryEnergy * .001, "clip low-pass actually attenuates 8 kHz audio at an 800 Hz cutoff");
        filter.project.clips[0].controls.filterCutoff = 0; filter.project.tracks[0].filterCutoff = lowPassToCutoff(800); const auto laneWet = render(173);
        double ownerDifference = 0; for (size_t i = 4800; i < dry.size(); ++i) ownerDifference = std::max(ownerDifference, (double) std::abs(laneWet[i] - clipWet[i]));
        check(ownerDifference < 1e-6, "clip and mixer low-pass use the same audio implementation");
        filter.project.tracks[0].filterCutoff = 0; filter.project.clips[0].controls.filterCutoff = .04f;
        const auto clipHigh = render(173);
        filter.project.clips[0].controls.filterCutoff = 0; filter.project.tracks[0].filterCutoff = .04f;
        const auto laneHigh = render(173); double highOwnerDifference = 0, highEnergy = 0;
        for (size_t i = 4800; i < dry.size(); ++i)
        { highOwnerDifference = std::max(highOwnerDifference, (double) std::abs(clipHigh[i] - laneHigh[i])); highEnergy += laneHigh[i] * laneHigh[i]; }
        check(highOwnerDifference < 1e-6 && highEnergy > dryEnergy * .9, "clip and lane high-pass share the production path and retain 8 kHz audio");
        filter.project.tracks[0].filterCutoff = lowPassToCutoff(800);
        auto& cutoff = filter.project.tracks[0].automation[(size_t) AutomationTarget::cutoff]; cutoff.enabled = true; cutoff.setPoint(0, 0); cutoff.setPoint(.8, lowPassToCutoff(300));
        const auto odd = render(173), regular = render(512); double partitionDifference = 0;
        for (size_t i = 0; i < odd.size(); ++i) partitionDifference = std::max(partitionDifference, (double) std::abs(odd[i] - regular[i]));
        check(partitionDifference < 1e-6, "low-pass automation is identical across live/export callback partitions");
        filter.project.tracks[1].mute = true; filter.project.tracks[2].solo = true;
        auto filterDocument = projectToJson(filter.project); auto reopenedFilter = parseProject(juce::JSON::toString(filterDocument));
        check(reopenedFilter.error.isEmpty() && reopenedFilter.project.tracks[0].filterCutoff == lowPassToCutoff(800)
            && reopenedFilter.project.tracks[1].mute && reopenedFilter.project.tracks[2].solo
            && reopenedFilter.project.tracks[0].automation[(size_t) AutomationTarget::cutoff].active(), "cutoff and its curve survive session parsing: " + reopenedFilter.error);
        auto highDocument = filterDocument.clone(); highDocument["tracks"].getArray()->getReference(0).getDynamicObject()->setProperty("filterCutoff", .25);
        highDocument["clips"].getArray()->getReference(0)["controls"].getDynamicObject()->setProperty("filterCutoff", .5);
        const auto reopenedHigh = parseProject(juce::JSON::toString(highDocument));
        check(reopenedHigh.error.isEmpty() && reopenedHigh.project.tracks[0].filterCutoff == .25f
            && reopenedHigh.clips[0].clip.controls.filterCutoff == .5f, "format-27 preserves HP independently on clip and lane owners");
        for (const auto bad : {juce::var("0.5"), juce::var(-1.01), juce::var(1.01), juce::var()})
        {
            auto invalid = juce::JSON::parse(juce::JSON::toString(filterDocument)); invalid["tracks"].getArray()->getReference(0).getDynamicObject()->setProperty("filterCutoff", bad);
            check(parseProject(juce::JSON::toString(invalid)).error.isNotEmpty(), "new sessions reject malformed or missing filter cutoff");
        }
        const auto oldFilter = parseProject(juce::JSON::toString(legacyLowPassFixture(filterDocument.clone(), 26)));
        check(oldFilter.error.isEmpty() && std::abs(oldFilter.project.tracks[0].filterCutoff - lowPassToCutoff(800)) < 1e-6
            && std::abs(oldFilter.project.tracks[0].automation[(size_t) AutomationTarget::cutoff].interpolatedValue(.4, 0) - lowPassToCutoff(10150)) < 1e-6,
            "format-26 low-pass values and linear Hz ramps migrate without changing their frequency trajectory: " + oldFilter.error);
        auto invalidLegacy = legacyLowPassFixture(filterDocument.clone(), 26);
        invalidLegacy["tracks"].getArray()->getReference(0).getDynamicObject()->setProperty("lowPassHz", 19);
        check(parseProject(juce::JSON::toString(invalidLegacy)).error.isNotEmpty(), "legacy cutoff validation rejects invalid Hz before migration");
        check(parseMidiAction("low-pass-1") == juce::Optional<MidiAction>(MidiAction {MidiActionKind::cutoff, 0})
            && midiActionId({MidiActionKind::cutoff, 0}) == "cutoff-1", "old low-pass MIDI profiles load as current cutoff actions");
        cutoff.setPoint(.8, .04f); const auto acrossOdd = render(173), acrossRegular = render(512);
        double acrossDifference = 0; for (size_t i = 0; i < acrossOdd.size(); ++i) acrossDifference = std::max(acrossDifference, (double) std::abs(acrossOdd[i] - acrossRegular[i]));
        check(acrossDifference < 1e-6, "automation crossing neutral into HP is identical for live/export block partitions");
        filterDocument.getDynamicObject()->setProperty("version", 22); filterDocument["tracks"].getArray()->removeRange(4, trackCount - 4);
        for (auto& lane : *filterDocument["tracks"].getArray()) { lane.getDynamicObject()->removeProperty("filterCutoff"); lane["automation"].getDynamicObject()->removeProperty("cutoff"); }
        for (auto& clip : *filterDocument["clips"].getArray()) { clip["controls"].getDynamicObject()->removeProperty("filterCutoff"); clip["controls"]["automation"].getDynamicObject()->removeProperty("cutoff"); }
        const auto legacy = parseProject(juce::JSON::toString(filterDocument));
        check(legacy.error.isEmpty() && legacy.project.tracks[0].filterCutoff == 0 && legacy.clips[0].clip.controls.filterCutoff == 0,
            "existing format-22 sessions load with exact dry filter defaults: " + legacy.error);
        filter.mappings = {{0xb0, 16, 80, {MidiActionKind::deskPot, 6}}, {0x90, 16, 81, {MidiActionKind::cyclePot, 6}}};
        filter.testMidi(juce::MidiMessage::noteOn(16, 81, (juce::uint8) 127)); filter.testMidi(juce::MidiMessage::controllerEvent(16, 80, 96)); filter.flushMidiControls();
        check(filter.deskLanes[6] == 2 && filter.project.tracks[2].filterCutoff > 0 && filter.project.tracks[2].filterCutoff < .1f,
            "mapped lane selector followed by a routed pot operates the newly selected lane");
        filter.applyMidiControl({MidiActionKind::bass, 3}, .5);
        check(filter.project.tracks[3].eqDb[0] == -24, "existing fixed-lane MIDI EQ mappings retain their original action");
    }
    const auto history = ui.undoHistory.size(); desk.faders[0].onDragStart(); desk.faders[0].setValue(-8, juce::sendNotificationSync); desk.faders[0].setValue(-6, juce::sendNotificationSync); desk.faders[0].onDragEnd();
    check(ui.undoHistory.size() == history + 1 && ui.engine.controlSettings(0).gainDb == -6, "one performance fader gesture creates one undo transaction");
    ui.undo(false); check(ui.engine.controlSettings(0).gainDb == -12, "performance fader undo restores the mixer owner");
    desk.learn.setToggleState(true, juce::dontSendNotification); desk.learn.onClick(); desk.pads[12].onClick();
    check(ui.engine.learnAction && *ui.engine.learnAction == MidiAction {MidiActionKind::solo, 0} && !ui.engine.project.tracks[0].solo,
          "MIDI Learn on a performance pad selects its exact action without firing it");
    desk.pots[7].onDragStart(); desk.pots[7].onDragEnd();
    check(ui.engine.learnAction && *ui.engine.learnAction == MidiAction {MidiActionKind::deskPot, 7}, "the eighth pot learns its routed surface identity");
    desk.cancelLearn.onClick(); check(!ui.engine.learnAction && !desk.learn.getToggleState(), "cancel learn clears pending hardware assignment");
    desk.triggerReset(0); desk.triggerReset(4); desk.triggerReset(8);
    check(ui.engine.controlSettings(0).eqDb == std::array<float, 3>{} && ui.engine.controlSettings(0).filterCutoff == 0
        && ui.engine.controlSettings(3).pan == 0 && !ui.engine.masterAutomationArmed, "retained reset commands reset EQ, open filters and centre pans without arming");
    check(desk.buttons[6].getButtonText() == "Pause @" && !desk.buttons[6].isEnabled() && desk.buttons[7].isEnabled()
        && desk.buttons[7].getTitle().contains("Next bar"), "scheduled pause requires playing programme and its timing remains explicitly available while stopped");
    for (const auto a : midiActions()) check(parseMidiAction(midiActionId(a)) == juce::Optional<MidiAction>(a), "performance MIDI action round-trips: " + midiActionId(a));
    const auto midiProfile = root.getChildFile("controller-mappings.json");
    {
        Engine controller(midiProfile); controller.beginLearn("solo-1");
        controller.testMidi(juce::MidiMessage::noteOn(16, 118, (juce::uint8) 127)); controller.flushMidiControls();
        controller.testMidi(juce::MidiMessage::noteOn(16, 118, (juce::uint8) 100)); controller.flushMidiControls();
        check(!controller.project.tracks[0].solo && midiProfile.existsAsFile(), "learning saves a button without firing its repeated held messages");
        controller.testMidi(juce::MidiMessage::noteOff(16, 118)); controller.testMidi(juce::MidiMessage::noteOn(16, 118, (juce::uint8) 127)); controller.flushMidiControls();
        check(controller.project.tracks[0].solo, "the first new physical press after learning operates the mapped action");
        controller.beginLearn("record-take"); controller.testMidi(juce::MidiMessage::controllerEvent(16, 117, 127)); controller.flushMidiControls();
        Engine reopenedController(midiProfile);
        check(reopenedController.mappings.size() == 2 && midiProfile.getSiblingFile("controller-mappings.json.bak").existsAsFile(), "new assignments retain existing mappings, create a backup and reopen through production parsing");
        controller.mappings.push_back({0xb0, 12, 70, {MidiActionKind::volume, 2}}); controller.beginLearn("solo-2");
        controller.testMidi(juce::MidiMessage::controllerEvent(12, 70, 0)); controller.flushMidiControls();
        check(controller.learnAction && controller.project.tracks[2].gainDb == 0, "a zero/release message during button learning cannot operate an existing fader mapping");
        controller.beginLearn("volume-2"); controller.testMidi(juce::MidiMessage::controllerEvent(12, 69, 0)); controller.flushMidiControls();
        check(!controller.learnAction && controller.project.tracks[1].gainDb == 0, "continuous controls can learn their valid zero endpoint without changing audio");
        controller.mappings.push_back({0xb0, 12, 71, {MidiActionKind::pan, 0}}); controller.beginControllerSetup(true);
        controller.testMidi(juce::MidiMessage::controllerEvent(12, 71, 127)); controller.flushMidiControls();
        controller.testMidi(juce::MidiMessage::controllerEvent(12, 71, 100)); controller.flushMidiControls();
        check(controller.mappingSummary().contains("Pot 2") && !std::any_of(controller.mappings.begin(), controller.mappings.end(), [](const auto& b) { return b.action == MidiAction {MidiActionKind::pan, 0}; }), "live desk setup deliberately replaces an old control assignment and ignores repeated movement");
        controller.skipControllerStep(); check(controller.mappingSummary().contains("Pot 3"), "skipping an unavailable hardware control advances without assigning it"); controller.beginLearn({});
    }
    {
        auto ownedStackFixture_transport = std::make_unique<Workstation>(false); auto& transport = *ownedStackFixture_transport; transport.stopTimer();
        transport.engine.mappings = {{0xb0, 16, 118, {MidiActionKind::snap}},
            {0xb0, 16, 76, {MidiActionKind::undo}}, {0xb0, 16, 77, {MidiActionKind::redo}},
            {0xb0, 8, 40, {MidiActionKind::mute, 0}}, {0x90, 8, 41, {MidiActionKind::solo, 1}}};
        transport.engine.project.masterDb = -7;
        transport.engine.project.tracks[2].pan = .37f;
        transport.engine.project.tracks[2].automation[0].setPoint(3, -9);
        const auto initial = juce::JSON::toString(projectToJson(transport.engine.project));
        const bool initialSnap = transport.engine.project.snap;
        auto queuePress = [&](int number)
        {
            transport.engine.testMidi(juce::MidiMessage::controllerEvent(16, number, 127));
            transport.engine.testMidi(juce::MidiMessage::controllerEvent(16, number, 100));
            transport.engine.testMidi(juce::MidiMessage::controllerEvent(16, number, 0));
        };
        auto press = [&](int number) { queuePress(number); transport.engine.flushMidiControls(); };
        press(118);
        check(transport.engine.project.snap != initialSnap && transport.undoHistory.size() == 1 && transport.dirty
            && transport.snapButton.getToggleState() == transport.engine.project.snap,
            "mapped Score Log/Snap press creates one Undo step; held messages and release add none");
        press(76);
        check(juce::JSON::toString(projectToJson(transport.engine.project)) == initial && transport.redoHistory.size() == 1,
            "mapped Undo restores the entire project preceding MIDI Snap");
        press(77);
        check(transport.engine.project.snap != initialSnap && transport.undoHistory.size() == 1 && transport.redoHistory.empty(),
            "mapped Redo reapplies MIDI Snap");
        press(76); press(118);
        check(transport.redoHistory.empty() && transport.undoHistory.size() == 1,
            "a new MIDI Snap edit clears the previous Redo branch");
        transport.undo(false); transport.undoHistory.clear(); transport.redoHistory.clear(); transport.dirty = false;
        queuePress(118); queuePress(118); transport.engine.flushMidiControls();
        check(transport.engine.project.snap == initialSnap && transport.undoHistory.size() == 2,
            "two Snap presses queued before UI dispatch keep separate Undo steps");
        transport.undo(false);
        check(transport.engine.project.snap != initialSnap, "Undo steps back through queued Snap edits individually");
        transport.undo(false); transport.redoHistory.clear(); transport.dirty = false;
        transport.setBusy(true); press(118); transport.setBusy(false);
        transport.engine.exporting = true; press(118); transport.engine.exporting = false;
        check(juce::JSON::toString(projectToJson(transport.engine.project)) == initial && transport.undoHistory.empty() && !transport.dirty,
            "busy/export guards reject MIDI Snap without a project change or empty Undo entry");
        transport.performance->buttons[4].onClick();
        check(transport.engine.project.snap != initialSnap && transport.undoHistory.size() == 1,
            "desk Snap uses the same single Undo transaction");
        transport.undo(false);
        transport.snapButton.setToggleState(!initialSnap, juce::dontSendNotification); transport.snapButton.onClick();
        check(transport.engine.project.snap != initialSnap && transport.undoHistory.size() == 1,
            "toolbar/menu Snap uses the same single Undo transaction");
        transport.undo(false);
        transport.engine.testMidi(juce::MidiMessage::controllerEvent(8, 40, 127)); transport.engine.flushMidiControls();
        check(transport.engine.project.tracks[0].mute && transport.undoHistory.size() == 1,
            "mapped mute captures its project switch before mutation");
        transport.undo(false);
        transport.engine.testMidi(juce::MidiMessage::noteOn(8, 41, (juce::uint8)127)); transport.engine.flushMidiControls();
        check(transport.engine.project.tracks[1].solo && transport.undoHistory.size() == 1,
            "mapped note solo shares the project-switch Undo boundary");
        transport.undo(false);
        check(juce::JSON::toString(projectToJson(transport.engine.project)) == initial,
            "Undo of MIDI switches preserves every other project field");
        const auto switchHistory = transport.undoHistory.size();
        transport.engine.applyMidiControl({MidiActionKind::snap}, 0);
        transport.engine.applyMidiControl({MidiActionKind::mute, trackCount}, 1);
        check(transport.undoHistory.size() == switchHistory && juce::JSON::toString(projectToJson(transport.engine.project)) == initial,
            "zero-value and invalid-lane switches do not create Undo entries");
        transport.performance->pads[8].onClick();
        check(transport.engine.project.tracks[0].mute && transport.undoHistory.size() == switchHistory + 1,
            "mouse mute retains exactly one Undo transaction after sharing the engine boundary");
    }
    ui.engine.mappings = {{0xb0, 16, 119, {MidiActionKind::solo, 0}}, {0x90, 16, 120, {MidiActionKind::nextTrack}}};
    auto cc = [&](int value) { ui.engine.testMidi(juce::MidiMessage::controllerEvent(16, 119, value)); ui.engine.flushMidiControls(); };
    cc(127); cc(100); check(ui.engine.project.tracks[0].solo, "held/repeated CC button messages toggle solo only once");
    cc(0); cc(127); check(!ui.engine.project.tracks[0].solo, "CC release rearms the next solo press"); cc(0);
    ui.engine.selectedTrack = 0;
    ui.engine.testMidi(juce::MidiMessage::noteOn(16, 120, (juce::uint8) 127)); ui.engine.testMidi(juce::MidiMessage::noteOn(16, 120, (juce::uint8) 100)); ui.engine.flushMidiControls();
    check(ui.engine.selectedTrack == 1, "mapped note button is edge-triggered while held");
    ui.engine.testMidi(juce::MidiMessage::noteOff(16, 120)); ui.engine.testMidi(juce::MidiMessage::noteOn(16, 120, (juce::uint8) 127)); ui.engine.flushMidiControls();
    check(ui.engine.selectedTrack == 2, "mapped note release rearms hardware navigation");
    desk.buttons[5].onClick();
    check(ui.engine.automationRecordMode == AutomationRecordMode::touch && ui.engine.masterAutomationArmed
          && std::all_of(ui.engine.automationArmed.begin(), ui.engine.automationArmed.end(), [](bool a) { return a; }), "Record take arms all lanes and master in Touch mode");
    ui.engine.setPlaying(true); ui.engine.position = 1;
    desk.faders[0].setValue(-20, juce::sendNotificationSync); desk.pots[7].setValue(lowPassToCutoff(900), juce::sendNotificationSync); desk.master.setValue(-10, juce::sendNotificationSync);
    ui.engine.position = 1.3; ui.engine.advanceAutomation(1.3); desk.buttons[3].onClick();
    check(ui.engine.project.tracks[0].automation[0].active() && ui.engine.project.tracks[1].automation[(size_t) AutomationTarget::cutoff].active()
          && ui.engine.project.masterAutomation.active() && ui.engine.automationRecordMode == AutomationRecordMode::read && !ui.engine.playing,
          "performing fader, low-pass and master moves records distinct curves; Stop finishes the take and returns to Read");
    for (const auto& media : ui.engine.library)
    {
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream = media->file.createOutputStream();
        auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(media->sampleRate).withNumChannels(1).withBitsPerSample(24));
        if (writer) writer->writeFromAudioSampleBuffer(media->samples, 0, media->samples.getNumSamples());
    }
    const auto takeFile = root.getChildFile("performance-take.mixworks"); Engine take;
    check(ui.engine.saveProject(takeFile).isEmpty() && take.restoreProject(takeFile.loadFileAsString()).isEmpty()
          && take.project.tracks[0].automation[0].active() && take.project.masterAutomation.active(), "a live desk take survives production session save and reopen");
    {
        Engine four({}, false); four.sampleRate = 8000; four.project.masterDb = 0;
        Clip c; c.track = 3; c.media = ui.engine.library.back(); c.length = 1; four.library = ui.engine.library; four.project.clips = {c};
        four.project.tracks[3].pan = .4f; four.project.tracks[3].filterCutoff = lowPassToCutoff(7000);
        const auto savedFour = juce::JSON::toString(four.captureProject()); Engine reopenedFour({}, false); reopenedFour.sampleRate = 8000;
        check(reopenedFour.restoreProject(savedFour).isEmpty() && reopenedFour.project.clips[0].track == 3
            && reopenedFour.project.tracks[3].pan == .4f && reopenedFour.project.tracks[3].filterCutoff == lowPassToCutoff(7000),
            "format 27 saves and reopens four real mixer lanes through the production engine");
        juce::AudioBuffer<float> pcm(2, 512); juce::MidiBuffer midi; reopenedFour.render(pcm, 512, .1, midi);
        check(pcm.getMagnitude(0, 0, 512) > .001f && pcm.getMagnitude(1, 0, 512) > .001f, "lane 4 produces audible stereo PCM");
        reopenedFour.project.tracks[3].mute = true; reopenedFour.render(pcm, 512, .1, midi);
        check(pcm.getMagnitude(0, 0, 512) == 0 && pcm.getMagnitude(1, 0, 512) == 0, "lane 4 mute reaches the production audio path");
        auto legacy = four.captureProject(); legacy.getDynamicObject()->setProperty("version", 24);
        const auto neutralDocument = projectToJson(Project());
        for (int lane = 4; lane < 8; ++lane)
        {
            auto neutral = (*neutralDocument["tracks"].getArray())[0].clone();
            neutral.getDynamicObject()->setProperty("name", "Audio " + juce::String(lane + 1));
            legacy["tracks"].getArray()->add(neutral);
        }
        legacy = legacyLowPassFixture(legacy, 24);
        const auto legacyText = juce::JSON::toString(legacy);
        check(reopenedFour.restoreProject(legacyText).isEmpty() && reopenedFour.project.clips[0].track == 3,
            "format 24 opens without losing work when all four extra lanes are unused");
        const auto retained = juce::JSON::toString(reopenedFour.captureProject());
        for (const auto property : {"gainDb", "name", "unknownWork"})
        {
            auto used = juce::JSON::parse(legacyText);
            used["tracks"].getArray()->getReference(4).getDynamicObject()->setProperty(property, juce::String("keep this"));
            check(reopenedFour.restoreProject(juce::JSON::toString(used)).contains("lanes 5-8")
                && juce::JSON::toString(reopenedFour.captureProject()) == retained,
                "used or unknown extra-lane state rejects migration transactionally: " + juce::String(property));
        }
        auto usedAudio = juce::JSON::parse(legacyText); usedAudio["clips"].getArray()->getReference(0).getDynamicObject()->setProperty("track", 7);
        check(reopenedFour.restoreProject(juce::JSON::toString(usedAudio)).contains("lanes 5-8")
            && juce::JSON::toString(reopenedFour.captureProject()) == retained, "audio on an extra lane is never silently discarded or remapped");
        auto malformed = juce::JSON::parse(legacyText); malformed["tracks"].getArray()->removeLast();
        check(reopenedFour.restoreProject(juce::JSON::toString(malformed)).isNotEmpty(), "incomplete old eight-lane sessions are rejected");
        check(!parseMidiAction("volume-5") && !parseMidiAction("volume-8"), "MIDI cannot address nonexistent mixer lanes");
    }
    const auto performedWav = root.getChildFile("performed.wav"), reopenedWav = root.getChildFile("reopened.wav");
    ui.engine.monitorMuted = true; // Device mute must never leak into the production export.
    const auto performedError = ui.engine.exportWav(performedWav), reopenedError = take.exportWav(reopenedWav);
    auto performed = std::unique_ptr<juce::AudioFormatReader>(ui.engine.formats.createReaderFor(performedWav));
    auto reopenedAudio = std::unique_ptr<juce::AudioFormatReader>(take.formats.createReaderFor(reopenedWav));
    bool sameTake = performedError.isEmpty() && reopenedError.isEmpty() && performed && reopenedAudio && performed->lengthInSamples == reopenedAudio->lengthInSamples;
    double maximumDifference = 0;
    if (sameTake)
    {
        juce::AudioBuffer<float> a(2, 512), b(2, 512);
        for (juce::int64 offset = 0; offset < performed->lengthInSamples; offset += 512)
        {
            const int frames = (int) std::min<juce::int64>(512, performed->lengthInSamples - offset);
            sameTake &= performed->read(&a, 0, frames, offset, true, true) && reopenedAudio->read(&b, 0, frames, offset, true, true);
            for (int ch = 0; ch < 2; ++ch) for (int f = 0; f < frames; ++f) maximumDifference = std::max(maximumDifference, (double) std::abs(a.getSample(ch, f) - b.getSample(ch, f)));
        }
    }
    check(sameTake && maximumDifference < 0.00001, "the saved performance renders the same level/EQ/master automation through production WAV export; maximum PCM difference=" + juce::String(maximumDifference, 8));
    ui.performancePanel.dock.onClick(); auto* same = ui.performance.get();
    check(ui.performanceWindow && ui.performanceWindow->getContentComponent() == &ui.performancePanel
          && ui.transitionPanel.isVisible() && ui.toolPage == 1, "detaching moves the actual live desk into a native second window and reveals Transitions in the shared space");
    ui.performanceWindow->setSize(1600, 480);
    check(desk.getWidth() == ui.performanceViewport.getWidth() && desk.getHeight() >= 276 && capture(desk, "alloy-live-desk.png"), "detached live desk responds to native window resizing: " + juce::String(desk.getWidth()) + "/" + juce::String(ui.performanceViewport.getWidth()));
    desk.master.setValue(-9, juce::sendNotificationSync); desk.refresh(true);
    check(ui.engine.project.masterDb == -9, "detached controls still update the same Engine");
    ui.performancePanel.dock.onClick();
    check(!ui.performanceWindow && ui.performance.get() == same && ui.performancePanel.getParentComponent() == &ui,
          "docking restores the same control instance without destroying the engine or rebuilding a mix");
    ui.setSize(1000, 620);
    check(ui.viewport.getViewHeight() >= ui.layout.defaultLaneHeight
        && ui.performanceViewport.getVerticalScrollBar().isVisible(), "short windows retain a complete timeline lane and let the controller scroll vertically: " + juce::String(ui.viewport.getViewHeight()) + "/" + juce::String(ui.performanceViewport.getHeight()));
    ui.setSize(1920, 1080);
    const auto beforePanels = juce::JSON::toString(projectToJson(ui.engine.project));
    const auto beforePanelUndo = ui.undoHistory.size(); const double beforePanelPosition = ui.engine.position;
    ui.transitionTab.onClick();
    check(ui.toolPage == 1 && ui.transitionPanel.isVisible() && !ui.performancePanel.isVisible()
        && ui.inspectorPage == 1 && ui.transitionTab.getToggleState(), "Transitions tab replaces the controller without taking over the VST inspector");
    check(ui.performancePanel.dock.getButtonText().isEmpty() && ui.transitionPanel.dock.getButtonText().isEmpty()
        && ui.transitionPanel.dock.getTitle() == "Detach Transitions", "both panels use icon-only dock buttons with accessible action names");
    for (const auto size : {juce::Point<int>{1250, 800}, juce::Point<int>{1920, 1080}})
    {
        ui.setSize(size.x, size.y); ui.transitionViewport.setViewPosition(0, 0);
        check(ui.layout.performance.withTrimmedTop(32) == ui.transitionPanel.getBounds()
            && ui.transitionPanel.dock.getRight() <= ui.transitionPanel.getWidth()
            && ui.transitionBody.getLocalBounds().contains(ui.transitionBandPositions[3].getBounds())
            && ui.transitionBody.getLocalBounds().contains(ui.exportTransitionPreset.getBounds())
            && capture(ui, "tool-transitions-" + juce::String(size.x) + ".png"), "transition panel retains all controls and independent inspector at " + juce::String(size.x));
    }
    ui.transitionPanel.dock.onClick(); auto* transitionControls = ui.transitionViewport.getViewedComponent();
    check(ui.transitionWindow && ui.transitionWindow->getContentComponent() == &ui.transitionPanel
        && ui.toolPage == 0 && ui.performancePanel.isVisible(), "detaching Transitions reveals the docked controller");
    ui.transitionWindow->setSize(1100, 620);
    {
        bool edges = true;
        for (const int width : {640, 1100})
        {
            ui.transitionWindow->setSize(width, 620);
            const auto image = ui.transitionWindow->createComponentSnapshot(ui.transitionWindow->getLocalBounds());
            edges &= image.getPixelAt(0, 200) == colour::moltenOrange
                && capture(*ui.transitionWindow, "orange-detached-" + juce::String(width) + ".png");
        }
        check(edges, "detached panel client edges remain orange at compact and wide native window sizes");
    }
    check(ui.transitionBody.getWidth() >= 1000 && ui.transitionPanel.dock.getTitle() == "Attach Transitions"
        && capture(ui.transitionPanel, "tool-transitions-detached.png"), "detached Transitions lays out in columns and offers an attach icon");
    ui.detachToolPanel(2, true);
    ui.performancePanel.dock.onClick();
    const auto expandedTimeline = ui.layout.timeline.getHeight();
    check(ui.performanceWindow && ui.transitionWindow && ui.layout.performance.getHeight() >= 32
        && ui.editingWindow && ui.controllerTab.isVisible() && ui.transitionTab.isVisible() && ui.editingTab.isVisible(), "all three panels detach independently while their workspace tabs remain reachable");
    check(ui.bottomSplitter.isVisible() && ui.layout.timeline.getHeight() == ui.layout.maximumTimelineHeight, "both undocked panels expose the bottom grip and fill the available timeline height");
    auto& grip = ui.bottomSplitter; const int fullHeight = ui.layout.timeline.getHeight();
    juce::Point<float> gripDown {40, 4}, gripUp {40, -116};
    grip.mouseDown(event(grip, gripDown, gripDown, false)); grip.mouseDrag(event(grip, gripUp, gripDown, true)); grip.mouseUp(event(grip, gripUp, gripDown, true));
    check(ui.layout.timeline.getHeight() == fullHeight - 120 && ui.layout.preferredTimelineHeight == fullHeight - 120
        && grip.getY() == ui.tempoStrip.getBottom() && evenLanes(ui.layout), "dragging the bottom grip shrinks all four lanes evenly and keeps the tempo footer attached");
    grip.keyPressed(juce::KeyPress(juce::KeyPress::downKey));
    check(ui.layout.timeline.getHeight() == fullHeight - 110 && evenLanes(ui.layout), "timeline grip keyboard resizing distributes height evenly through the geometry owner");
    ui.saveWorkspacePreferences(); WorkspaceLayout heightPreference;
    check(readWorkspacePreferences(preference, heightPreference).wasOk() && heightPreference.preferredTimelineHeight == fullHeight - 110,
        "bottom-drag height persists outside musical session state");
    ui.setSize(1920, 1080); ui.setSize(1250, 800);
    check(ui.layout.timeline.getHeight() <= ui.layout.maximumTimelineHeight && ui.layout.performance.getHeight() >= 32, "a preferred timeline height clamps in a smaller window while keeping docking tabs reachable");
    ui.setSize(1920, 1080); grip.keyPressed(juce::KeyPress(juce::KeyPress::homeKey));
    check(ui.timeline.getHeight() <= ui.viewport.getViewHeight() && capture(ui, "alloy-four-lanes-undocked.png"), "four complete lanes fill the expanded undocked timeline without vertical scrolling");
    check(ui.layout.preferredTimelineHeight == 0 && ui.layout.timeline.getHeight() == ui.layout.maximumTimelineHeight
        && beforePanels == juce::JSON::toString(projectToJson(ui.engine.project)) && ui.undoHistory.size() == beforePanelUndo,
        "reset fills available height and resizing preserves arrangement, automation and Undo");
    ui.controllerTab.onClick(); ui.transitionTab.onClick();
    check(ui.performanceWindow && ui.transitionWindow, "tabs focus detached panels without rebuilding or attaching them");
    ui.detachToolPanel(2, false);
    ui.transitionPanel.dock.onClick();
    check(ui.bottomSplitter.isVisible() && !ui.transitionWindow && ui.transitionPanel.getParentComponent() == &ui
        && ui.transitionViewport.getViewedComponent() == transitionControls && ui.toolPage == 1
        && ui.layout.timeline.getHeight() <= expandedTimeline, "the attach icon restores the same transition controls and shared dock without enlarging the lane viewport");
    ui.performancePanel.dock.onClick(); ui.controllerTab.onClick();
    check(ui.performance.get() == same && !ui.performanceWindow && ui.toolPage == 0
        && beforePanels == juce::JSON::toString(projectToJson(ui.engine.project))
        && ui.undoHistory.size() == beforePanelUndo && ui.engine.position == beforePanelPosition,
        "switching, resizing, attaching and native closing preserve mix, undo history, transport and controller identity");
    ui.showToolPage(0); // An active tab now collapses; explicitly open the panel for this capture.
    check(capture(ui, "tool-controller-1920.png") && capture(ui.performancePanel.dock, "tool-dock-200.png", 2), "capture the controller dock and vector hexagonal attach/detach icon");
    ui.setBusy(true); const auto beforeBusy = ui.engine.project.masterDb; ui.engine.applyMidiControl({MidiActionKind::master}, 0);
    check(!desk.faders[0].isEnabled() && !desk.pads[14].isEnabled() && ui.engine.project.masterDb == beforeBusy, "busy state disables docked, detached and hardware performance actions"); ui.setBusy(false);
    const auto beatOne = AlloyLookAndFeel::fourCountPulse(8, true, true), beatTwo = AlloyLookAndFeel::fourCountPulse(9, true, true);
    check(beatOne.count == 0 && beatTwo.count == 1 && AlloyLookAndFeel::fourCountPulse(11.99, true, true).count == 3
        && AlloyLookAndFeel::fourCountPulse(12, true, true).count == 0 && beatOne.heat > beatTwo.heat,
        "transition pulses count four mix beats, repeat at the bar and accent beat one");
    check(AlloyLookAndFeel::fourCountPulse(8.8, true, true).heat < beatOne.heat
        && AlloyLookAndFeel::fourCountPulse(8, false, true).heat == AlloyLookAndFeel::fourCountPulse(8.8, false, true).heat
        && AlloyLookAndFeel::fourCountPulse(8, true, false).heat == AlloyLookAndFeel::fourCountPulse(8.8, true, false).heat,
        "the pulse decays smoothly while paused and reduced-motion presentation stay steady");
    check(AlloyLookAndFeel::fourCountPulse(std::numeric_limits<double>::quiet_NaN(), true, true).count == 0
        && std::isfinite(AlloyLookAndFeel::fourCountPulse(std::numeric_limits<double>::infinity(), true, true).heat),
        "invalid presentation clock values retain a finite steady appearance");
    TempoMap ramp {120, {{0, 120, true}, {20, 160, false}}}; bool countsFollowMap = true;
    for (int beat = 0; beat < 32; ++beat)
    {
        const double time = ramp.timeAtBeat(beat + .1);
        countsFollowMap &= AlloyLookAndFeel::fourCountPulse(ramp.beatsAt(time), true, true).count == beat % 4;
    }
    check(countsFollowMap, "four-count animation follows the existing integrated ramp clock without accumulating a second timer");

    // Actual vector/native captures cover all four counts plus small and scaled
    // windows. The same source-owned frame and locked state remain interactive.
    ui.engine.beginLearn({});
    ui.engine.project = Project {}; ui.engine.project.tempo.setPoint(0, 120);
    for (int i = 0; i < 4; ++i)
    {
        Clip clip; clip.media = ui.engine.library[(size_t) i]; clip.start = i * 12; clip.length = 24; clip.track = i;
        clip.introAnchor = clip.fadeInAnchor = 0; clip.outroAnchor = 12; clip.fadeOutAnchor = 24;
        ui.engine.project.clips.push_back(clip);
    }
    const auto id = ui.engine.project.clips[1].id;
    ui.engine.project.transitionLinksKnown = true;
    ui.engine.project.transitionLinks = {{ui.engine.project.clips[0].id, id}, {id, ui.engine.project.clips[2].id}};
    ui.engine.project.setTransitionBand(id, 0, {TransitionBandMode::crossfade, .75, 0, .5});
    ui.engine.project.setTransitionBand(id, 1, {TransitionBandMode::swapFade, .8, 1, .5});
    ui.engine.project.setTransitionLocked(ui.engine.project.clips[2].id, true);
    ui.selectedAutomationTarget = -1; ui.engine.automationRecordMode = AutomationRecordMode::read;
    ui.engine.automationArmed.fill(false); ui.engine.masterAutomationArmed = false; ui.tempoStrip.selectedTempoPoint = 0;
    ui.timeline.pixelsPerSecond = 40; ui.setSize(1920, 1080); ui.selectTransition(id); ui.viewport.setViewPosition(320, 0);
    const auto visualState = juce::JSON::toString(projectToJson(ui.engine.project)); const auto visualUndo = ui.undoHistory.size();
    ui.engine.gridAudition.stop(); ui.engine.endTransitionRehearsal();
    const auto forgeDirty = ui.dirty;
    ui.menuItemSelected(81, 4); ui.menuItemSelected(83, 4); ui.saveWorkspacePreferences();
    check(ui.layout.forgeEffects && ui.dirty == forgeDirty && ui.undoHistory.size() == visualUndo
        && readWorkspacePreferences(preference, reopened).wasOk() && reopened.forgeEffects && reopened.forgeMotion == 1,
        "View forge toggle is discoverable, saved and independent of musical Undo and dirty state");
    ui.engine.seek(13); ui.engine.setPlaying(true); ui.peakDisplay.advance({.96f, .5f}, .04, true); ui.refresh();
    ui.timeLabel.setText(timeText(ui.engine.position), juce::dontSendNotification);
    const auto forged = ui.timeline.createComponentSnapshot(ui.timeline.getLocalBounds());
    ui.menuItemSelected(84, 4);
    const auto steady = ui.timeline.createComponentSnapshot(ui.timeline.getLocalBounds());
    const int seamX = Clip::gridPixel(13, ui.timeline.pixelsPerSecond) - 2;
    check(!ui.forgeMotionAllowed() && forged.getPixelAt(seamX, 5) != steady.getPixelAt(seamX, 5),
        "native Animated forge visibly changes the overlapping playhead seam; Steady removes it without changing Windows preferences");
    ui.menuItemSelected(83, 4);
    check(capture(ui, "forge-overlap-1920.png"), "capture native forge playhead melting the overlapping waveform paths with stereo peak hold");
    ui.peakDisplay.advance({1.12f, .9f}, .04, true); ui.advancePeakMeter();
    check(ui.peakReset.getButtonText() == "CLIP / reset" && capture(ui, "forge-clip-1920.png"), "output clipping stays explicit alongside the optional hot-metal treatment");
    ui.peakReset.onClick();
    check(!ui.peakDisplay.clipped && !ui.engine.masterClipped, "native peak reset clears display and engine clipping history");
    ui.setSize(1250, 800); ui.refresh();
    check(capture(ui, "forge-overlap-1250.png"), "forge panel remains readable at the minimum workspace width");
    ui.setSize(1920, 1080); ui.refresh();
    ui.menuItemSelected(81, 4);
    check(!ui.layout.forgeEffects && ui.peakDisplay.sparkSeconds == std::array<float, 2>{0, 0}
        && juce::JSON::toString(projectToJson(ui.engine.project)) == visualState, "disabling forge removes particles immediately without changing waveform or arrangement data");
    for (int beat = 0; beat < 4; ++beat)
    {
        ui.engine.position = (24 + beat) * .5; ui.engine.playing = true; ui.refresh();
        check(capture(ui, "molten-count-" + juce::String(beat + 1) + ".png"), "capture native orange transition pulse count " + juce::String(beat + 1));
    }
    ui.engine.playing = false; ui.refresh();
    check(capture(ui, "molten-paused-1920.png"), "capture steady paused frame, molten timeline rail and selected controls");
    for (const auto size : {juce::Point<int> {1440, 940}, juce::Point<int> {1250, 800}})
    {
        ui.setSize(size.x, size.y); ui.refresh(); ui.viewport.setViewPosition(360, 0);
        check(capture(ui, "molten-" + juce::String(size.x) + ".png"), "molten accents retain the native narrow layout at " + juce::String(size.x));
    }
    check(capture(ui, "molten-200.png", 2), "molten rail and frame remain vector-rendered at 200 percent");
    ui.addToDesktop(0);
    ui.viewport.getHorizontalScrollBar().setCurrentRangeStart(450, juce::sendNotificationSync);
    check(ui.viewport.getViewPositionX() == 450 && ui.viewport.getHorizontalScrollBar().getAccessibilityHandler() != nullptr,
        "styled timeline scrollbar retains native range changes and accessibility");
    ui.removeFromDesktop();
    check(juce::JSON::toString(projectToJson(ui.engine.project)) == visualState && ui.undoHistory.size() == visualUndo
        && ui.engine.project.transitionFor(ui.engine.project.clips[2].id)->locked, "visual pulse and material captures preserve session geometry, lock state and Undo history");
    runAttractTests(ui, check, report);
    ui.workspaceDirty = false;
    ui.detachToolPanel(1, true); ui.transitionWindow->closeButtonPressed();
    juce::MessageManager::callAsync([&]
    { check(ui.bottomSplitter.isVisible() && !ui.transitionWindow && ui.transitionPanel.getParentComponent() == &ui && ui.toolPage == 1,
        "native close returns before docking the same Transitions panel through the existing message loop"); });
    hoverProbe();
    runRecoveryTests(check, report);
}
}

