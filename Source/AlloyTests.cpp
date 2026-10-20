#include "Workstation.h"
#include <cmath>
#include <limits>

namespace mixworks
{
void runAttractTests(Workstation&, const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runPlaylistTransitionTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runMidiConnectionTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runAutoArrangementTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runAlloyTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    runPlaylistTransitionTests(check, report);
    runMidiConnectionTests(check, report);
    runAutoArrangementTests(check, report);
    {
        Workstation keys(false); keys.stopTimer();
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
        check(keys.keyText(*keys.engine.library[0]) == "Gbm ?", "compact labels retain the existing uncertain-key marker");
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
        check(tooltip.contains("Gb minor") && tooltip.contains("m means minor"), "key-cell hint explains the compact major/minor notation");
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
        Workstation labels(false); labels.stopTimer();
        for (const auto target : {AutomationTarget::volume, AutomationTarget::bass, AutomationTarget::mid, AutomationTarget::treble})
        {
            labels.selectAutomationTarget((int) target);
            check(labels.automationText(target, 0) == "100%" && labels.automationText(target, -60) == "0%"
                && labels.automationText(target, -6.0206f) == "50%" && labels.automationText(target, 6.0206f) == "200%", "automation node levels use percentage gain instead of fixed frequency labels or dB");
        }
        labels.selectAutomationTarget((int) AutomationTarget::lowPass);
        check(labels.automationText(AutomationTarget::lowPass, 20000) == "100%" && labels.automationText(AutomationTarget::lowPass, 20) == "0%", "filter automation node levels show their percentage range");
    }
    const auto root = report.getParentDirectory().getChildFile("alloy-fixture-" + juce::Uuid().toString()); root.createDirectory();
    const auto preference = root.getChildFile("workspace.json"); WorkspaceLayout settings;
    check(trackCount == 4, "the mixer exposes exactly four real lanes");
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
    settings.preferredLibraryWidth = 380; settings.preferredInspectorWidth = 290;
    check(writeWorkspacePreferences(preference, settings).wasOk(), "workspace preferences write an identified local record");
    auto doc = juce::JSON::parse(preference); doc.getDynamicObject()->setProperty("futureSetting", "keep me"); preference.replaceWithText(juce::JSON::toString(doc));
    const auto before = preference.loadFileAsString(); settings.preferredLibraryWidth = 420;
    check(writeWorkspacePreferences(preference, settings).wasOk() && preference.getSiblingFile("workspace.json.bak").loadFileAsString() == before
          && juce::JSON::parse(preference)["futureSetting"].toString() == "keep me", "width changes preserve unknown settings and create a backup");
    const auto modified = preference.getLastModificationTime();
    check(writeWorkspacePreferences(preference, settings).wasOk() && preference.getLastModificationTime() == modified, "unchanged workspace preferences are idempotent");
    WorkspaceLayout reopened; check(readWorkspacePreferences(preference, reopened).wasOk() && reopened.preferredLibraryWidth == 420 && reopened.preferredInspectorWidth == 290, "panel widths reopen independently of a musical session");
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

    Workstation ui(false); ui.stopTimer(); ui.workspacePreference = preference;
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
    ui.showBrowserPage(1); check(ui.playlistTable.table.isVisible() && !ui.libraryTable.table.isVisible() && ui.getMenuBarNames().contains("Beatmatch"), "Playlist tab retains arrangement and beatmatch access");
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
                for (auto* control : std::initializer_list<juce::Component*> {&lane->select, &lane->mute, &lane->solo, &lane->gain, &lane->pan, &lane->lowPass})
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
        for (auto* c : std::initializer_list<juce::Component*> {&ui.homeButton, &ui.playButton, &ui.stopButton, &ui.timeLabel, &ui.tempoSlider, &ui.snapButton, &ui.zoomSlider}) valid = valid && ui.layout.transport.contains(c->getBounds());
        for (auto& c : ui.clipSliders) valid = valid && ui.layout.detail.contains(c.getBounds());
        for (auto* c : {&ui.displayButton, &ui.windowModeButton, &ui.minimiseButton, &ui.closeAppButton})
            valid = valid && c->isVisible() && ui.layout.windowControls.contains(c->getBounds()) && !c->getBounds().intersects(ui.menuBar.getBounds());
        std::vector<juce::Component*> toolbarControls;
        for (auto* c : std::initializer_list<juce::Component*> {&ui.newButton, &ui.openButton, &ui.saveButton, &ui.importButton, &ui.exportButton, &ui.undoButton, &ui.redoButton, &ui.splitButton, &ui.duplicateButton, &ui.removeButton, &ui.beatmatchButton, &ui.waveformMode, &ui.settingsButton})
            if (c->isVisible()) { valid = valid && ui.layout.toolbar.contains(c->getBounds()); toolbarControls.push_back(c); }
        for (size_t i = 0; i < toolbarControls.size(); ++i) for (size_t j = i + 1; j < toolbarControls.size(); ++j)
            valid = valid && !toolbarControls[i]->getBounds().intersects(toolbarControls[j]->getBounds());
        valid = valid && ui.transitionLength.getParentComponent() == &ui.transitionBody && ui.transitionPreset.getParentComponent() == &ui.transitionBody
            && ui.layout.ruler.getY() == ui.layout.overview.getBottom() + 2 && ui.laneAmount.getBottom() <= ui.layout.timeline.getY();
        check(valid && capture(ui, "alloy-" + juce::String(size.x) + ".png"), "native Alloy layout and controls fit at " + juce::String(size.x));
    }
    check(ui.layout.ruler.getY() == ui.layout.overview.getBottom() + 2 && ui.waveformMode.isVisible(), "waveform view keeps the ruler directly beneath the overview");
    ui.transitionLength.setSelectedId(4, juce::sendNotificationSync);
    check(ui.transitionOverlapBeats() == 64, "bars dropdown retains the transition-length operation beside waveform display");
    ui.menuItemSelected(42, 3);
    check(ui.transitionLength.getSelectedId() == 3 && ui.transitionOverlapBeats() == 32, "Beatmatch menu and bars dropdown share the same transition choice");
    {
        const auto before = ui.engine.project;
        const auto ruler = ui.layout.ruler, lanes = ui.layout.timeline;
        const auto selected = ui.engine.project.clips.front().id; ui.selectedClip = selected; ui.engine.selectedTrack = 0;
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
    check(ui.laneAmount.getText() == "4 LANES" && ui.timeline.contentLaneCount() == 4
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
            const std::array<juce::Component*, 6> controls {&strip.select, &strip.mute, &strip.solo, &strip.lowPass, &strip.pan, &strip.gain};
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
    check(ui.timeline.laneSize(0) == WorkspaceLayout::minimumLaneHeight && ui.strips[0]->lowPass.isVisible()
        && ui.strips[0]->pan.isVisible(), "a lane cannot collapse away its controls");
    ui.strips[0]->keyPressed(juce::KeyPress(juce::KeyPress::homeKey));
    check(std::abs(ui.timeline.laneSize(0) - defaultHeight) <= 1, "keyboard Home restores the lane's share of available height");
    ui.saveWorkspacePreferences(); WorkspaceLayout restoredSizes;
    bool restoredLaneGeometry = readWorkspacePreferences(preference, restoredSizes).wasOk();
    restoredSizes.update(ui.getWidth(), ui.getHeight());
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
    ui.timeline.pixelsPerSecond = 40; ui.timeline.updateSize(); ui.viewport.setViewPosition(300, 0);
    const double centreTime = (ui.viewport.getViewPositionX() + ui.viewport.getViewWidth() * 0.5) / ui.timeline.pixelsPerSecond;
    ui.zoomSlider.setValue(std::log10(80.0), juce::sendNotificationSync);
    check(std::abs(ui.timeline.pixelsPerSecond - 80) < 0.001
          && std::abs((ui.viewport.getViewPositionX() + ui.viewport.getViewWidth() * 0.5) / ui.timeline.pixelsPerSecond - centreTime) < 0.02
          && !ui.dirty && ui.undoHistory.size() == laneUndo, "zoom slider magnifies around the visible centre without changing the mix");
    ui.timeline.zoomAt(0.5, ui.viewport.getViewPositionX() + 50);
    check(std::abs(std::pow(10.0, ui.zoomSlider.getValue()) - ui.timeline.pixelsPerSecond) < 0.001, "wheel and menu zoom refresh the same slider");
    ui.menuItemSelected(22, 4);
    check(ui.viewport.getViewPositionX() == 0 && std::abs(std::pow(10.0, ui.zoomSlider.getValue()) - ui.timeline.pixelsPerSecond) < 0.001, "View / Fit whole mix retains a usable reset for the zoom slider");
    ui.resizeLane(0, 2 * ui.layout.defaultLaneHeight);
    check(capture(*ui.strips[0], "alloy-lane-controller.png"), "capture reference-inspired lane controls with LPF, pan, volume, mute and solo");
    ui.strips[0]->lowPass.setValue(1800, juce::sendNotificationSync);
    check(ui.engine.controlSettings(0).lowPassHz == 1800, "lane LPF operates immediately without arming or MIDI mapping");
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
        const float value = i < 6 ? -5.f - i : 1200.f + 100 * i;
        desk.pots[(size_t) i].setValue(value, juce::sendNotificationSync);
        desk.faders[(size_t) i].setValue(i < 4 ? -12 - i : .1 * (i - 3), juce::sendNotificationSync);
        const auto& t = ui.engine.controlSettings(i % 2);
        const auto& lane = ui.engine.controlSettings(i % 4);
        check((i < 6 ? std::abs(t.eqDb[(size_t) (i / 2)] - value) < .001 : t.lowPassHz == value)
            && (i < 4 ? lane.gainDb == -12 - i : std::abs(lane.pan - .1 * (i - 3)) < .001),
            "routed control " + juce::String(i) + " EQ/LPF=" + juce::String(i < 6 ? t.eqDb[(size_t) (i / 2)] : t.lowPassHz) + " fader=" + juce::String(i < 4 ? lane.gainDb : lane.pan));
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
        filter.project.clips[0].controls.lowPassHz = 800; const auto clipWet = render(173);
        double dryEnergy = 0, wetEnergy = 0; for (size_t i = 4800; i < dry.size(); ++i) { dryEnergy += dry[i] * dry[i]; wetEnergy += clipWet[i] * clipWet[i]; }
        check(wetEnergy < dryEnergy * .001, "clip low-pass actually attenuates 8 kHz audio at an 800 Hz cutoff");
        filter.project.clips[0].controls.lowPassHz = 20000; filter.project.tracks[0].lowPassHz = 800; const auto laneWet = render(173);
        double ownerDifference = 0; for (size_t i = 4800; i < dry.size(); ++i) ownerDifference = std::max(ownerDifference, (double) std::abs(laneWet[i] - clipWet[i]));
        check(ownerDifference < 1e-6, "clip and mixer low-pass use the same audio implementation");
        auto& cutoff = filter.project.tracks[0].automation[(size_t) AutomationTarget::lowPass]; cutoff.enabled = true; cutoff.setPoint(0, 20000); cutoff.setPoint(.8, 300);
        const auto odd = render(173), regular = render(512); double partitionDifference = 0;
        for (size_t i = 0; i < odd.size(); ++i) partitionDifference = std::max(partitionDifference, (double) std::abs(odd[i] - regular[i]));
        check(partitionDifference < 1e-6, "low-pass automation is identical across live/export callback partitions");
        filter.project.tracks[1].mute = true; filter.project.tracks[2].solo = true;
        auto filterDocument = projectToJson(filter.project); auto reopenedFilter = parseProject(juce::JSON::toString(filterDocument));
        check(reopenedFilter.error.isEmpty() && reopenedFilter.project.tracks[0].lowPassHz == 800
            && reopenedFilter.project.tracks[1].mute && reopenedFilter.project.tracks[2].solo
            && reopenedFilter.project.tracks[0].automation[(size_t) AutomationTarget::lowPass].active(), "cutoff and its curve survive session parsing: " + reopenedFilter.error);
        for (const auto bad : {juce::var("800"), juce::var(19), juce::var(20001), juce::var()})
        {
            auto invalid = juce::JSON::parse(juce::JSON::toString(filterDocument)); invalid["tracks"].getArray()->getReference(0).getDynamicObject()->setProperty("lowPassHz", bad);
            check(parseProject(juce::JSON::toString(invalid)).error.isNotEmpty(), "new sessions reject malformed or missing filter cutoff");
        }
        filterDocument.getDynamicObject()->setProperty("version", 22); filterDocument["tracks"].getArray()->removeRange(4, trackCount - 4);
        for (auto& lane : *filterDocument["tracks"].getArray()) { lane.getDynamicObject()->removeProperty("lowPassHz"); lane["automation"].getDynamicObject()->removeProperty("low-pass"); }
        for (auto& clip : *filterDocument["clips"].getArray()) { clip["controls"].getDynamicObject()->removeProperty("lowPassHz"); clip["controls"]["automation"].getDynamicObject()->removeProperty("low-pass"); }
        const auto legacy = parseProject(juce::JSON::toString(filterDocument));
        check(legacy.error.isEmpty() && legacy.project.tracks[0].lowPassHz == 20000 && legacy.clips[0].clip.controls.lowPassHz == 20000,
            "existing format-22 sessions load with exact dry filter defaults: " + legacy.error);
        filter.mappings = {{0xb0, 16, 80, {MidiActionKind::deskPot, 6}}, {0x90, 16, 81, {MidiActionKind::cyclePot, 6}}};
        filter.testMidi(juce::MidiMessage::noteOn(16, 81, (juce::uint8) 127)); filter.testMidi(juce::MidiMessage::controllerEvent(16, 80, 64)); filter.flushMidiControls();
        check(filter.deskLanes[6] == 2 && filter.project.tracks[2].lowPassHz > 600 && filter.project.tracks[2].lowPassHz < 700,
            "mapped lane selector followed by a routed pot operates the newly selected lane");
        filter.applyMidiControl({MidiActionKind::bass, 3}, .5);
        check(filter.project.tracks[3].eqDb[0] == -24, "existing fixed-lane MIDI EQ mappings retain their original action");
    }
    const auto history = ui.undoHistory.size(); desk.faders[0].onDragStart(); desk.faders[0].setValue(-8, juce::sendNotificationSync); desk.faders[0].setValue(-6, juce::sendNotificationSync); desk.faders[0].onDragEnd();
    check(ui.undoHistory.size() == history + 1 && ui.engine.controlSettings(0).gainDb == -6, "one performance fader gesture creates one undo transaction");
    ui.undo(false); check(ui.engine.controlSettings(0).gainDb == -12, "performance fader undo restores the mixer owner");
    desk.learn.setToggleState(true, juce::dontSendNotification); desk.pads[12].onClick();
    check(ui.engine.learnAction && *ui.engine.learnAction == MidiAction {MidiActionKind::solo, 0} && !ui.engine.project.tracks[0].solo,
          "MIDI Learn on a performance pad selects its exact action without firing it");
    desk.pots[7].onDragStart(); desk.pots[7].onDragEnd();
    check(ui.engine.learnAction && *ui.engine.learnAction == MidiAction {MidiActionKind::deskPot, 7}, "the eighth pot learns its routed surface identity");
    desk.cancelLearn.onClick(); check(!ui.engine.learnAction && !desk.learn.getToggleState(), "cancel learn clears pending hardware assignment");
    desk.channelButtons[0].onClick(); desk.channelButtons[4].onClick(); desk.channelButtons[8].onClick();
    check(ui.engine.controlSettings(0).eqDb == std::array<float, 3>{} && ui.engine.controlSettings(0).lowPassHz == 20000
        && ui.engine.controlSettings(3).pan == 0 && !ui.engine.masterAutomationArmed, "small pads reset EQ, open filters and centre pans without arming");
    check(desk.buttons[6].getButtonText() == "Spare" && !desk.buttons[6].isEnabled() && !desk.buttons[7].isEnabled(), "two transport pads remain explicitly spare");
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
        Workstation transport(false); transport.stopTimer();
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
    desk.faders[0].setValue(-20, juce::sendNotificationSync); desk.pots[7].setValue(900, juce::sendNotificationSync); desk.master.setValue(-10, juce::sendNotificationSync);
    ui.engine.position = 1.3; ui.engine.advanceAutomation(1.3); desk.buttons[3].onClick();
    check(ui.engine.project.tracks[0].automation[0].active() && ui.engine.project.tracks[1].automation[(size_t) AutomationTarget::lowPass].active()
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
        four.project.tracks[3].pan = .4f; four.project.tracks[3].lowPassHz = 7000;
        const auto savedFour = juce::JSON::toString(four.captureProject()); Engine reopenedFour({}, false); reopenedFour.sampleRate = 8000;
        check(reopenedFour.restoreProject(savedFour).isEmpty() && reopenedFour.project.clips[0].track == 3
            && reopenedFour.project.tracks[3].pan == .4f && reopenedFour.project.tracks[3].lowPassHz == 7000,
            "format 25 saves and reopens four real mixer lanes through the production engine");
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
        && ui.inspectorPage == 0 && ui.transitionTab.getToggleState(), "Transitions tab replaces the controller without taking over the inspector");
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
    check(ui.transitionBody.getWidth() >= 1000 && ui.transitionPanel.dock.getTitle() == "Attach Transitions"
        && capture(ui.transitionPanel, "tool-transitions-detached.png"), "detached Transitions lays out in columns and offers an attach icon");
    ui.performancePanel.dock.onClick();
    const auto expandedTimeline = ui.layout.timeline.getHeight();
    check(ui.performanceWindow && ui.transitionWindow && ui.layout.performance.getHeight() >= 32
        && ui.controllerTab.isVisible() && ui.transitionTab.isVisible(), "both panels detach independently while their workspace tabs remain reachable");
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
    ui.transitionPanel.dock.onClick();
    check(ui.bottomSplitter.isVisible() && !ui.transitionWindow && ui.transitionPanel.getParentComponent() == &ui
        && ui.transitionViewport.getViewedComponent() == transitionControls && ui.toolPage == 1
        && ui.layout.timeline.getHeight() <= expandedTimeline, "the attach icon restores the same transition controls and shared dock without enlarging the lane viewport");
    ui.performancePanel.dock.onClick(); ui.controllerTab.onClick();
    check(ui.performance.get() == same && !ui.performanceWindow && ui.toolPage == 0
        && beforePanels == juce::JSON::toString(projectToJson(ui.engine.project))
        && ui.undoHistory.size() == beforePanelUndo && ui.engine.position == beforePanelPosition,
        "switching, resizing, attaching and native closing preserve mix, undo history, transport and controller identity");
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
    runRecoveryTests(check, report);
}
}

