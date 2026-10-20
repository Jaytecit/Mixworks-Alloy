#include "Workstation.h"
#include <juce_cryptography/juce_cryptography.h>
#include <cmath>
#include <limits>

namespace mixworks
{
void runFusionWorkflowTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    const auto folder = report.getParentDirectory().getChildFile("fusion-workflows-" + juce::Uuid().toString()); folder.createDirectory();
    const auto first = folder.getChildFile("01. source.wav"), second = folder.getChildFile(juce::String::fromUTF8("02. caf\xc3\xa9 #100%.wav"));
    bool written = false;
    {
        juce::AudioBuffer<float> pcm(2, 44100); for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 44100; ++i) pcm.setSample(ch, i, 0.25f);
        juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream = first.createOutputStream();
        auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(44100).withNumChannels(2).withBitsPerSample(24));
        written = writer && writer->writeFromAudioSampleBuffer(pcm, 0, 44100);
    }
    check(written && first.copyFileTo(second), "playlist tests use owned real WAV fixtures, including Unicode, spaces, hash and percent paths");
    Engine engine;
    auto a = readMedia(engine.formats, first), b = readMedia(engine.formats, second);
    check(a.media && b.media, "playlist sources load through the production importer"); if (!a.media || !b.media) return;
    a.media->manualBpm = 120; a.media->manualKey = 17; a.media->name = "First source"; b.media->name = "Second source";
    engine.library = {a.media, b.media}; engine.project.followTrackTempo = false;
    Clip ca; ca.media = a.media; ca.length = 0.5; ca.offset = 0.2; ca.reversed = true;
    Clip cb; cb.media = b.media; cb.length = 0.75; cb.start = 10;
    auto repeat = ca; repeat.id = juce::Uuid().toString(); repeat.start = 20;
    engine.project.clips = {cb, ca, repeat};
    LabelMarker intro; intro.time = 0; intro.text = "Intro"; LabelMarker drop; drop.time = 12.25; drop.text = juce::String::fromUTF8("Drop / caf\xc3\xa9");
    check(engine.project.putLabelMarker(drop).wasOk() && engine.project.putLabelMarker(intro).wasOk() && engine.project.labels[0].id == intro.id,
          "labels retain identity and sort at their model owner");
    auto invalid = drop; invalid.time = std::numeric_limits<double>::infinity();
    bool rejects = engine.project.putLabelMarker(invalid).failed(); invalid = drop; invalid.text = "  "; rejects &= engine.project.putLabelMarker(invalid).failed();
    invalid.text = "A\nB"; rejects &= engine.project.putLabelMarker(invalid).failed(); invalid.text = juce::String::repeatedString("x", 257); rejects &= engine.project.putLabelMarker(invalid).failed();
    invalid = drop; invalid.time = 0; rejects &= engine.project.putLabelMarker(invalid).failed();
    check(rejects && engine.project.labels.size() == 2 && engine.project.labels[1].time == drop.time, "invalid label names, non-finite times and time collisions preserve existing markers");
    const double audioDuration = engine.project.duration();
    auto far = drop; far.id = juce::Uuid().toString(); far.time = 500; far.text = "Later note"; engine.project.putLabelMarker(far);
    check(engine.project.duration() == audioDuration, "labels beyond the audio do not add silent export duration"); engine.project.labels.pop_back();
    const auto session = folder.getChildFile("labels.mixworks");
    check(engine.saveProject(session).isEmpty() && engine.restoreProject(session.loadFileAsString()).isEmpty()
          && engine.project.labels.size() == 2 && engine.project.labels[1].id == drop.id && engine.project.labels[1].text == drop.text && engine.project.labels[1].time == drop.time,
          "production save/reopen retains label identity, Unicode and precise time with audio clips");
    const auto saved = session.loadFileAsString(); auto old = juce::JSON::parse(saved); old.getDynamicObject()->setProperty("version", 6); old["tracks"].getArray()->removeRange(4, trackCount - 4); old.getDynamicObject()->removeProperty("labels");
    check(parseProject(juce::JSON::toString(old)).error.isEmpty() && parseProject(juce::JSON::toString(old)).project.labels.empty(), "version 6 sessions migrate with no invented labels");
    bool malformed = true;
    for (int mode = 0; mode < 8; ++mode)
    {
        auto doc = juce::JSON::parse(saved); auto* labels = doc.getDynamicObject()->getProperty("labels").getArray();
        if (mode == 0) doc.getDynamicObject()->removeProperty("labels");
        if (mode == 1) labels->getReference(0).getDynamicObject()->setProperty("time", "0");
        if (mode == 2) labels->getReference(0).getDynamicObject()->setProperty("time", -1);
        if (mode == 3) labels->getReference(1).getDynamicObject()->setProperty("id", intro.id);
        if (mode == 4) labels->getReference(1).getDynamicObject()->setProperty("time", 0);
        if (mode == 5) labels->getReference(0).getDynamicObject()->setProperty("time", 50);
        if (mode == 6) labels->getReference(1).getDynamicObject()->setProperty("text", "bad\nname");
        if (mode == 7) labels->getReference(1).getDynamicObject()->setProperty("text", juce::String::repeatedString("x", 257));
        malformed &= engine.restoreProject(juce::JSON::toString(doc)).isNotEmpty() && engine.project.labels.size() == 2 && engine.project.labels[1].id == drop.id && engine.project.clips.size() == 3;
    }
    check(malformed, "malformed label collections, types, times, identities and names fail transactionally without replacing the active session");
    const auto m3u = folder.getChildFile("sources.m3u8"), text = folder.getChildFile("tracklist.txt");
    check(engine.exportPlaylist(m3u, PlaylistExport::m3u).isEmpty(), "production M3U8 export writes source playlist");
    auto parsed = readPlaylist(m3u);
    check(parsed.error.isEmpty() && parsed.files.size() == 3 && parsed.files[0] == first && parsed.files[1] == second && parsed.files[2] == first
          && m3u.loadFileAsString().contains("#EXTINF:1,First source"), "M3U round trip uses chronological clip order, retains repeated sources and reports source duration rather than edited duration");
    check(engine.exportPlaylist(text, PlaylistExport::text).isEmpty() && text.loadFileAsString().contains(drop.text)
          && text.loadFileAsString().contains(timeText(12.25)) && text.loadFileAsString().contains("4A") && text.loadFileAsString().contains("Unknown"),
          "text tracklist includes precise mix times, label names, note/Camelot keys and explicit unknowns");
    const auto original = m3u.loadFileAsString();
    engine.project.name = "Changed name";
    check(engine.exportPlaylist(m3u, PlaylistExport::m3u).isEmpty() && engine.exportPlaylist(m3u, PlaylistExport::m3u).isEmpty()
          && m3u.getSiblingFile(m3u.getFileName() + ".bak").loadFileAsString() == original && folder.findChildFiles(juce::File::findFiles, false, "sources*.bak").size() == 2,
          "repeated playlist exports preserve the first backup and allocate a fresh backup sibling");
    const auto protectedAudio = folder.getChildFile("source-with-text-extension.txt"); first.copyFileTo(protectedAudio);
    const auto sourceHash = juce::SHA256(protectedAudio).toHexString(); engine.library[0]->file = protectedAudio;
    check(engine.exportPlaylist(protectedAudio, PlaylistExport::text).contains("Cannot replace source audio") && juce::SHA256(protectedAudio).toHexString() == sourceHash
          && engine.exportPlaylist(session, PlaylistExport::text).isNotEmpty(), "playlist export cannot overwrite imported source audio or use a session extension");
    engine.library[0]->file = first;
    const auto relative = folder.getChildFile("relative.m3u");
    relative.replaceWithText(juce::String::charToString(0xfeff) + "#EXTM3U\r\n#EXTINF:1,Untrusted title\r\n01. source.wav\r\n" + second.getFileName() + "\r\n01. source.wav\r\n");
    parsed = readPlaylist(relative);
    check(parsed.error.isEmpty() && parsed.files.size() == 3 && parsed.files[1] == second, "UTF-8 BOM, CRLF, comments and relative paths resolve against the playlist folder without URL-decoding literal percent/hash filenames");
    bool rejected = true;
    for (const auto& body : juce::StringArray {"01. source.wav\nmissing.wav", "https://example.invalid/a.wav", "file:///C:/audio.wav", "#EXTM3U\n#EXT-X-VERSION:3\n01. source.wav", "relative.m3u", "C:relative.wav", "#EXTM3U\n#empty"})
    { relative.replaceWithText(body); const auto result = readPlaylist(relative); rejected &= result.error.isNotEmpty() && result.files.isEmpty(); }
    check(rejected, "missing, URL, HLS, nested, drive-relative and empty playlists fail without a partial file selection");
    const char badBytes[] {'a', 0, 'b'}; relative.replaceWithData(badBytes, sizeof(badBytes));
    check(readPlaylist(relative).error.isNotEmpty(), "embedded NUL input cannot truncate a playlist path silently");
    const unsigned char badUtf8[] {0xc3, 0x28}; relative.replaceWithData(badUtf8, sizeof(badUtf8));
    check(readPlaylist(relative).error.isNotEmpty(), "invalid UTF-8 is rejected instead of silently changing a source path");
    { auto stream = relative.createOutputStream(); if (stream) { stream->setPosition(8 * 1024 * 1024); stream->writeByte('\n'); } }
    check(readPlaylist(relative).error.isNotEmpty(), "playlist text allocation is bounded before parsing");
    check(engine.exportPlaylist(folder, PlaylistExport::m3u).isNotEmpty(), "invalid export destinations report failure");

    Workstation ui(false); ui.stopTimer(); ui.engine.project = engine.project; ui.engine.library = engine.library; ui.refresh();
    auto& ruler = ui.timelineRuler;
    auto& timeline = ui.timeline; timeline.pixelsPerSecond = 40; ui.refresh();
    const auto source = juce::Desktop::getInstance().getMainMouseSource();
    auto event = [&](juce::Point<float> pos, juce::Point<float> down, bool dragged, int flags = juce::ModifierKeys::leftButtonModifier)
    { return juce::MouseEvent(source, pos, juce::ModifierKeys(flags), 1, 0, 0, 0, 0, &ruler, &ruler, juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), 1, dragged); };
    const juce::Point<float> start {490, 38.f}, finish {611, start.y};
    ruler.mouseDown(event(start, start, false)); ruler.mouseUp(event(start, start, false));
    check(timeline.selectedLabel == drop.id && std::abs(ui.engine.position - 12.25) < 0.000001, "native label click selects its stable identity and seeks to its session time");
    const auto beforeUndo = ui.undoHistory.size(); ruler.mouseDown(event(start, start, false)); ruler.mouseDrag(event(finish, start, true)); ruler.mouseUp(event(finish, start, true));
    check(ui.engine.project.labels[1].id == drop.id && std::abs(ui.engine.project.labels[1].time - 15.5) < 0.000001 && ui.undoHistory.size() == beforeUndo + 1,
          "native label drag snaps to the musical grid and records one undo action");
    ui.undo(false); check(ui.engine.project.labels[1].time == 12.25, "Undo restores the label time"); ui.undo(true);
    check(ui.engine.project.labels[1].time == 15.5, "Redo restores the dragged label");
    const juce::Point<float> snapped {620, start.y}, free {631, start.y};
    ruler.mouseDown(event(snapped, snapped, false)); ruler.mouseDrag(event(free, snapped, true, juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::altModifier)); ruler.mouseUp(event(free, snapped, true));
    check(std::abs(ui.engine.project.labels[1].time - 15.775) < 0.000001, "Alt-drag bypasses label snapping without jumping at the initial pointer offset");
    ruler.mouseDown(event(free, free, false, juce::ModifierKeys::rightButtonModifier));
    check(ui.engine.project.labels.size() == 1 && ui.engine.project.clips.size() == 3, "right-click removes the label without deleting the previously selected clip"); ui.undo(false);
    ui.engine.seek(0); ui.keyPressed(juce::KeyPress(']'));
    check(ui.engine.position > 15 && timeline.selectedLabel == drop.id, "next-label keyboard navigation seeks to the next marker");
    ui.keyPressed(juce::KeyPress('[')); check(ui.engine.position == 0 && timeline.selectedLabel == intro.id, "previous-label navigation reaches the zero marker");
    ui.engine.seek(999); ui.navigateLabel(true); check(ui.engine.position == 999, "navigation at the last label keeps the current playhead");
    ui.selectedClip = ui.engine.project.clips[0].id; ui.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey));
    check(ui.engine.project.labels.size() == 1 && ui.engine.project.clips.size() == 3, "Delete targets the selected label while keeping audio clips intact"); ui.undo(false);
    auto durationBefore = ui.engine.project.duration(); ui.engine.project.putLabelMarker(far); ui.refresh();
    check(timeline.getWidth() > 20000 && ui.engine.project.duration() == durationBefore, "timeline includes distant labels while audio duration remains owned by clips");
    ui.engine.project.labels.pop_back(); ui.refresh();
    for (const auto size : {juce::Point<int>{1440, 940}, juce::Point<int>{1250, 800}})
    {
        ui.setSize(size.x, size.y); ui.engine.seek(3); ui.viewport.setViewPosition(0, 0); ui.layoutTrackHeaders();
        const auto image = ui.createComponentSnapshot(ui.getLocalBounds()); auto stream = report.getSiblingFile("fusion-labels-" + juce::String(size.x) + ".png").createOutputStream();
        if (stream) { stream->setPosition(0); stream->truncate(); juce::PNGImageFormat format; format.writeImageToStream(image, *stream); }
        check(image.isValid() && Timeline::rulerHeight == Timeline::timeRulerHeight + Timeline::labelHeight && ui.tempoStrip.getY() == ui.viewport.getBottom() && ui.getLocalPoint(ui.strips[0].get(), juce::Point<int>{0, 0}).y == ui.viewport.getY(), "native label/tempo lanes render with aligned track controls at " + juce::String(size.x));
    }
    // Exercise the actual modal Apply/Cancel callbacks and background M3U import on JUCE's message queue.
    ui.engine.project.labels.clear(); ui.undoHistory.clear(); ui.redoHistory.clear();
    ui.menuItemSelected(32, 2);
    auto* dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
    check(dialog != nullptr, "Mix / Label marker opens the native name/time editor");
    if (!dialog) return;
    dialog->getTextEditor("name")->setText("Precise native label"); dialog->getTextEditor("time")->setText("7.123456"); dialog->exitModalState(1);
    struct Poll final : juce::Timer { std::function<void()> tick; void timerCallback() override { tick(); } } poll;
    int phase = 0; const auto deadline = juce::Time::getMillisecondCounterHiRes() + 15000;
    poll.tick = [&]
    {
        if (juce::Time::getMillisecondCounterHiRes() > deadline) { check(false, "label dialog/import completes within deadline"); poll.stopTimer(); juce::MessageManager::getInstance()->stopDispatchLoop(); return; }
        if (ui.busy) return;
        if (phase == 0)
        {
            if (ui.engine.project.labels.empty()) return;
            check(ui.engine.project.labels.size() == 1 && ui.engine.project.labels[0].text == "Precise native label" && std::abs(ui.engine.project.labels[0].time - 7.123456) < 1.0e-9, "native dialog Apply commits the precise label without beat snapping");
            ui.editLabelMarker(ui.engine.project.labels[0].id, 0);
            if (auto* edit = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent())) { edit->getTextEditor("name")->setText("Renamed label"); edit->exitModalState(1); phase = 1; } else phase = 3;
        }
        else if (phase == 1)
        {
            if (ui.engine.project.labels[0].text != "Renamed label") return;
            check(ui.undoHistory.size() == 2, "renaming through the native dialog is one undoable edit");
            ui.editLabelMarker({}, 9);
            if (auto* cancel = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent())) { cancel->exitModalState(0); phase = 2; } else phase = 3;
        }
        else if (phase == 2)
        {
            check(ui.engine.project.labels.size() == 1 && ui.undoHistory.size() == 2, "Cancel leaves label state and undo history unchanged");
            const auto staged = folder.getChildFile("ui-import.wav"); first.copyFileTo(staged);
            relative.replaceWithText("#EXTM3U\nui-import.wav\nui-import.wav\n");
            ui.filesDropped({relative.getFullPathName()}, 0, 0); phase = 3;
        }
        else if (phase == 3)
        {
            check(ui.engine.library.size() == 3 && ui.engine.library.back()->file == folder.getChildFile("ui-import.wav") && ui.engine.library.back()->name != "Untrusted title"
                  && ui.engine.project.clips.size() == 3 && ui.engine.project.labels.size() == 1, "M3U file drop traverses the real worker importer, deduplicates library audio and preserves the current mix/labels");
            first.copyFileTo(folder.getChildFile("cancelled-import.wav")); relative.replaceWithText("cancelled-import.wav\n");
            ui.importFiles({relative}); ui.importProgress.cancel.onClick(); phase = 4;
        }
        else
        {
            check(ui.engine.library.size() == 3 && ui.engine.project.clips.size() == 3 && ui.engine.project.labels[0].text == "Renamed label"
                  && ui.status.contains("cancelled"), "cancelling playlist preflight preserves the library, mix and labels before any audio import");
            poll.stopTimer(); juce::MessageManager::getInstance()->stopDispatchLoop();
        }
    };
    poll.startTimer(20); juce::MessageManager::getInstance()->runDispatchLoop();
}
}
