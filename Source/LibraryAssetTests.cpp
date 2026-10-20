#include "Workstation.h"
#include <cmath>

namespace mixworks
{
void runLibraryAssetTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report, const juce::File& encoder, const juce::File& reopen)
{
    juce::File index = reopen; juce::Array<juce::File> sources; juce::File session; bool snapshotStartupExpected = true;
    if (reopen == juce::File())
    {
        const auto root = report.getParentDirectory().getChildFile("library-assets-" + juce::Uuid().toString()); root.createDirectory();
        const auto album = root.getChildFile("album"); album.createDirectory(); root.getChildFile("empty").createDirectory();
        const auto wave = album.getChildFile("Fixture.wav");
        {
            juce::AudioBuffer<float> audio(2, 22050);
            for (int i = 0; i < audio.getNumSamples(); ++i) for (int ch = 0; ch < 2; ++ch) audio.setSample(ch, i, (float) (0.2 * std::sin(i * 440.0 * juce::MathConstants<double>::twoPi / 22050)));
            juce::WavAudioFormat format; std::unique_ptr<juce::OutputStream> output = wave.createOutputStream();
            auto writer = format.createWriterFor(output, juce::AudioFormatWriterOptions().withSampleRate(22050).withNumChannels(2).withBitsPerSample(24));
            check(writer && writer->writeFromAudioSampleBuffer(audio, 0, audio.getNumSamples()), "library workflows create an owned real audio fixture");
        }
        auto image = [](const juce::File& file, juce::Colour colour)
        {
            juce::Image art(juce::Image::RGB, 240, 160, true, juce::SoftwareImageType()); juce::Graphics g(art); g.fillAll(colour);
            g.setColour(juce::Colours::white.withAlpha(0.7f)); g.drawEllipse(45, 5, 150, 150, 7); g.fillEllipse(110, 70, 20, 20);
            juce::PNGImageFormat png; auto output = file.createOutputStream(); if (output) { output->setPosition(0); output->truncate(); } return output && png.writeImageToStream(art, *output);
        };
        const auto common = album.getChildFile("cover.png"), specific = album.getChildFile("Fixture.png");
        check(image(common, juce::Colours::darkblue) && image(specific, juce::Colours::darkorange), "cover workflows create distinguishable album and track images");
        auto assets = readLibraryAssets(wave);
        check(assets.cover.isValid() && assets.coverSource == specific && !assets.audioThumbnail && assets.cover.getWidth() <= 64 && assets.cover.getHeight() <= 64,
              "track-named local artwork precedes album artwork and is bounded to a 64-pixel thumbnail");
        specific.replaceWithText("not a PNG"); assets = readLibraryAssets(wave);
        check(assets.cover.isValid() && assets.coverSource == common, "malformed track artwork falls through to a valid album cover without blocking audio");
        image(specific, juce::Colours::darkorange);
        const auto missing = root.getChildFile("empty/Unknown.wav"); wave.copyFileTo(missing);
        check(!readLibraryAssets(missing).cover.isValid(), "missing local artwork remains absent for the neutral library placeholder");
        const auto alternate = album.getChildFile("Second.wav"); wave.copyFileTo(alternate); sources = {wave, alternate, missing};
        if (encoder.existsAsFile())
        {
            const auto tagged = album.getChildFile("Tagged.mp3");
            juce::StringArray arguments {encoder.getFullPathName(), "-v", "error", "-y", "-i", wave.getFullPathName(), "-i", specific.getFullPathName(), "-map", "0:a", "-map", "1:v", "-c:a", "libmp3lame", "-ar", "44100", "-b:a", "128k", "-write_xing", "0", "-c:v", "png", "-id3v2_version", "3", "-metadata", "title=Library Fixture Title", "-metadata", "artist=Library Fixture Artist", "-metadata", "album=Library Fixture Album", "-metadata:s:v", "title=Album cover", "-metadata:s:v", "comment=Cover (front)", "-disposition:v", "attached_pic", tagged.getFullPathName()};
            juce::ChildProcess encoding; const bool started = encoding.start(arguments), finished = started && encoding.waitForProcessToFinish(15000);
            if (started && !finished) encoding.kill();
            check(finished && encoding.getExitCode() == 0, "the existing local encoder produces a real MP3 with embedded ID3 cover/title/artist/album");
            const auto embedded = readLibraryAssets(tagged);
            const auto pixel = embedded.cover.getPixelAt(8, 8);
            check(embedded.audioThumbnail && embedded.cover.isValid() && embedded.coverSource == tagged && pixel.getRed() > pixel.getBlue(),
                  "Windows extracts embedded MP3 cover artwork before the conflicting folder cover; pixel=" + pixel.toString());
            check(embedded.title == "Library Fixture Title" && embedded.artist.contains("Library Fixture Artist") && embedded.album == "Library Fixture Album",
                  "real Windows audio metadata supplies title, artist and album");
            sources.add(tagged);
        }
        index = root.getChildFile("library.json");
        check(rememberLibraryCatalogue(index, {wave}).isEmpty() && readLibraryCatalogue(index).files == juce::Array<juce::File> {wave}, "an identified catalogue retains an imported absolute source path");
        auto document = juce::JSON::parse(index); document.getDynamicObject()->setProperty("extension", "keep me"); index.replaceWithText(juce::JSON::toString(document));
        const auto backup = index.getSiblingFile("library.json.bak"); backup.replaceWithText("existing backup");
        check(rememberLibraryCatalogue(index, sources).isEmpty() && readLibraryCatalogue(index).files.size() == sources.size() && juce::JSON::parse(index)["extension"].toString() == "keep me"
              && backup.loadFileAsString() == "existing backup", "catalogue updates merge tracks, retain unknown fields and preserve existing backups");
        const auto before = index.loadFileAsString(); check(rememberLibraryCatalogue(index, sources).isEmpty() && index.loadFileAsString() == before, "repeated catalogue registration is idempotent");
        const auto offline = root.getChildFile("offline.wav"); rememberLibraryCatalogue(index, {offline}); rememberLibraryCatalogue(index, sources);
        check(readLibraryCatalogue(index).files.contains(offline), "unavailable tracks are retained in the catalogue for a later reconnect");
        const auto foreign = root.getChildFile("foreign.json"); foreign.replaceWithText("{\"format\":\"mixworks-library\",\"version\":2,\"files\":[]}"); const auto foreignText = foreign.loadFileAsString();
        check(readLibraryCatalogue(foreign).error.isNotEmpty() && rememberLibraryCatalogue(foreign, sources).isNotEmpty() && foreign.loadFileAsString() == foreignText, "newer catalogue versions are reported and preserved");
        foreign.replaceWithText("{broken"); check(rememberLibraryCatalogue(foreign, sources).isNotEmpty() && foreign.loadFileAsString() == "{broken", "malformed catalogues cannot be overwritten by registration");
        foreign.replaceWithText("{\"format\":\"mixworks-library\",\"version\":1,\"files\":[\"relative.wav\"]}");
        check(readLibraryCatalogue(foreign).error.isNotEmpty(), "catalogue path parsing rejects relative paths rather than interpreting another working directory");
        foreign.replaceWithText("{\"format\":\"mixworks-library\",\"version\":1,\"files\":[\"C:/fixture\\u0000suffix.wav\"]}");
        check(readLibraryCatalogue(foreign).error.isNotEmpty(), "escaped NUL paths are rejected before JSON conversion can lose their suffix");
        juce::AudioFormatManager formats; formats.registerBasicFormats(); auto loaded = readMedia(formats, wave, {}, AnalysisCacheMode::readWrite, MediaReadPurpose::catalogue);
        check(loaded.media && loaded.media->assets.cover.isValid() && !loaded.media->hasPlayback(), "the production catalogue loader supplies artwork without pinning playback PCM");
        std::vector<std::shared_ptr<Media>> snapshots;
        bool analysed = true; juce::String analysisErrors; for (const auto& source : sources) { auto item = readMedia(formats, source); analysed &= item.media != nullptr; if (item.media) snapshots.push_back(item.media->catalogueCopy()); if (item.error.isNotEmpty()) analysisErrors += item.error + " "; }
        check(analysed, "every remembered fixture has persistent production analysis before testing a fresh process: " + analysisErrors);
        check(rememberLibraryCatalogue(index, sources, snapshots).isEmpty() && readLibraryCatalogue(index).media.size() == snapshots.size(), "one catalogue snapshot retains all track facts, artwork and waveforms");
        const auto snapshotText = index.loadFileAsString();
        check(rememberLibraryCatalogue(index, sources, snapshots).isEmpty() && index.loadFileAsString() == snapshotText, "saving identical library snapshots is idempotent");
        const auto restored = readLibraryCatalogue(index);
        check(!restored.media.empty() && !restored.media[0]->assets.artworkGroup.isEmpty() && restored.media[0]->assets.artworkGroup == snapshots[0]->assets.artworkGroup,
              "cached covers reconstruct the same normalized artwork grouping as imported sources: " + snapshots[0]->assets.artworkGroup + " / " + (restored.media.empty() ? juce::String("missing") : restored.media[0]->assets.artworkGroup));
        check(!restored.media.empty() && restored.media[0]->peaks == snapshots[0]->peaks && restored.media[0]->waveformRms == snapshots[0]->waveformRms
              && !restored.media[0]->hasPlayback(), "saved peak and RMS display data restore without loading playback audio");
        auto stale = juce::JSON::parse(snapshotText);
        auto& staleEntry = stale["snapshots"].getArray()->getReference(0);
        staleEntry["record"].getDynamicObject()->setProperty("analysisRevision", analysisRevision - 1);
        staleEntry["record"]["manual"].getDynamicObject()->setProperty("bpm", 123.4567);
        const auto staleFile = root.getChildFile("stale-detector.json"); staleFile.replaceWithText(juce::JSON::toString(stale));
        const auto staleRead = readLibraryCatalogue(staleFile);
        auto staleMedia = std::find_if(staleRead.media.begin(), staleRead.media.end(), [&](const auto& m) { return m->file.getFullPathName() == staleEntry["path"].toString(); });
        check(staleMedia != staleRead.media.end() && (*staleMedia)->analysisVersion == analysisRevision - 1 && (*staleMedia)->manualBpm == 123.4567
            && !(*staleMedia)->peaks.empty() && !parseAnalysisRecord(analysisRecordToJson((*staleMedia)->analysisRecord())).reusableAutomatic,
            "old detector snapshots retain immediate waveforms and manual corrections without being relabelled as current analysis");
        if (staleMedia != staleRead.media.end())
        {
            const auto record = (*staleMedia)->analysisRecord();
            const auto existingSidecar = readMedia(formats, (*staleMedia)->file, {}, AnalysisCacheMode::readOnly, MediaReadPurpose::catalogue, &record);
            check(existingSidecar.media && existingSidecar.media->manualBpm == 123.4567, "caller-selected library corrections survive refresh with an older on-disk correction");
            // The owned fixture sidecar is disconnected, matching a read-only media folder whose overrides live only in the library.
            const auto sidecar = analysisRecordFile((*staleMedia)->file), disconnected = sidecar.getSiblingFile(sidecar.getFileName() + ".stale-test");
            const bool moved = sidecar.moveFileTo(disconnected);
            auto refreshed = readMedia(formats, (*staleMedia)->file, {}, AnalysisCacheMode::readOnly, MediaReadPurpose::catalogue, &record);
            check(moved && refreshed.media && !refreshed.media->analysisFromCache && refreshed.media->analysisVersion == analysisRevision
                && refreshed.media->manualBpm == 123.4567, "stale library analysis refreshes before reuse while preserving snapshot-only manual BPM");
            check(disconnected.moveFileTo(sidecar), "restore the owned analysis fixture after the stale-library check");
        }
        auto damaged = juce::JSON::parse(snapshotText); auto& entry = damaged.getDynamicObject()->getProperty("snapshots").getArray()->getReference(0);
        entry["record"].getDynamicObject()->setProperty("peaksF32", "AAAA");
        const auto damagedFile = root.getChildFile("damaged-snapshot.json"); damagedFile.replaceWithText(juce::JSON::toString(damaged));
        auto recovered = readLibraryCatalogue(damagedFile);
        check(recovered.error.isEmpty() && recovered.files.size() == sources.size() + 1 && recovered.media.size() + 1 == snapshots.size(), "a malformed compact waveform retains all source paths and other snapshots for background recovery");
        entry.getDynamicObject()->setProperty("coverPng", juce::Base64::toBase64("bad PNG", 7));
        entry.getDynamicObject()->setProperty("record", analysisRecordToJson(snapshots[0]->analysisRecord(), true));
        damagedFile.replaceWithText(juce::JSON::toString(damaged)); recovered = readLibraryCatalogue(damagedFile);
        check(recovered.error.isEmpty() && recovered.media.size() + 1 == snapshots.size(), "invalid thumbnail bytes are rejected before image decoding without losing remembered sources");
        Engine mix; if (loaded.media) mix.library = {loaded.media}; session = root.getChildFile("other.mixworks"); check(mix.saveProject(session).isEmpty(), "the library test creates an owned session for merge verification");
        juce::StringArray childArgs {juce::File::getSpecialLocation(juce::File::currentExecutableFile).getFullPathName(), "--self-test", "--library-assets-only", "--playback-cache-root", PcmStore::shared().directory().getFullPathName(), "--catalogue-reopen", index.getFullPathName(), "--report", root.getChildFile("fresh-library.txt").getFullPathName()};
        const auto disconnected = root.getChildFile("disconnected-album");
        check(album.moveFileTo(disconnected), "disconnect owned source audio and artwork before the fresh-process snapshot check");
        juce::ChildProcess child; const bool childStarted = child.start(childArgs), childFinished = childStarted && child.waitForProcessToFinish(15000);
        if (childStarted && !childFinished) child.kill();
        check(childFinished && child.getExitCode() == 0, "a fresh app process restores the persistent catalogue, analysis and artwork without opening a session");
        check(disconnected.moveFileTo(album), "restore the owned source fixture after the disconnected-library check");
        const auto legacyIndex = root.getChildFile("legacy-library.json"); rememberLibraryCatalogue(legacyIndex, sources);
        childArgs.set(childArgs.indexOf("--catalogue-reopen") + 1, legacyIndex.getFullPathName());
        childArgs.set(childArgs.indexOf("--report") + 1, root.getChildFile("legacy-library.txt").getFullPathName());
        juce::ChildProcess legacyChild; const bool legacyStarted = legacyChild.start(childArgs), legacyFinished = legacyStarted && legacyChild.waitForProcessToFinish(15000);
        if (legacyStarted && !legacyFinished) legacyChild.kill();
        check(legacyFinished && legacyChild.getExitCode() == 0 && readLibraryCatalogue(legacyIndex).media.size() == snapshots.size(), "an old path-only library upgrades through native background restoration and persists its snapshot");
    }
    else
    {
        auto saved = readLibraryCatalogue(index); snapshotStartupExpected = !saved.media.empty();
        if (snapshotStartupExpected) { for (const auto& source : saved.media) sources.add(source->file); }
        else for (const auto& source : saved.files) if (source.existsAsFile()) sources.add(source);
    }
    Workstation ui(false); ui.stopTimer(); ui.catalogueFile = index; ui.catalogueRestorePending = true;
    ui.tickLibraryCatalogue();
    auto startupGate = std::make_shared<std::atomic<bool>>(!snapshotStartupExpected);
    if (snapshotStartupExpected) ui.workers.addJob([startupGate] { while (!*startupGate) juce::Thread::sleep(1); });
    struct Poll final : juce::Timer { std::function<void()> tick; void timerCallback() override { tick(); } } poll;
    int phase = 0; const auto deadline = juce::Time::getMillisecondCounterHiRes() + 20000;
    poll.tick = [&]
    {
        if (juce::Time::getMillisecondCounterHiRes() > deadline) { *startupGate = true; check(false, "library worker workflows complete before the deadline"); poll.stopTimer(); juce::MessageManager::getInstance()->stopDispatchLoop(); return; }
        if (!*startupGate)
        {
            if (ui.engine.library.size() != (size_t) sources.size()) return;
            check(!ui.busy && !ui.catalogueRestorePending && !ui.catalogueRefreshQueue.empty(), "the entire saved library is usable while background source checks are blocked");
            *startupGate = true;
        }
        ui.tickLibraryCatalogue(); if (ui.busy || ui.catalogueRestorePending || ui.catalogueWritePending || ui.catalogueRefreshPending || !ui.timelineLoads.empty()) return;
        if (phase == 0)
        {
            check(ui.engine.library.size() == (size_t) sources.size() && ui.engine.project.clips.empty() && !ui.dirty
                  && std::all_of(ui.engine.library.begin(), ui.engine.library.end(), [](const auto& media) { return !media->hasPlayback() && media->analysisFromCache; }),
                  "native startup restores the remembered library with saved analysis and lightweight media without making a mix dirty");
            check(!ui.engine.library.empty() && ui.engine.library[0]->assets.cover.isValid() && ui.libraryTable.table.getRowHeight() == 44 && ui.playlistTable.table.getRowHeight() == 28,
                  "restored artwork appears in the two-line library while the playlist retains its compact rows");
            if (reopen != juce::File()) { poll.stopTimer(); juce::MessageManager::getInstance()->stopDispatchLoop(); return; }
            ui.refresh();
            for (int width : {1440, 1250})
            {
                ui.setSize(width, width == 1440 ? 940 : 800); ui.refresh(); auto snapshot = ui.createComponentSnapshot(ui.getLocalBounds());
                juce::PNGImageFormat png; auto output = report.getSiblingFile("library-covers-" + juce::String(width) + ".png").createOutputStream();
                if (output) { output->setPosition(0); output->truncate(); }
                check(output && png.writeImageToStream(snapshot, *output), "native cover library renders at " + juce::String(width) + " pixels");
            }
            if (encoder.existsAsFile())
            {
                ui.search.setText("Library Fixture Artist", false); ui.filterLibrary();
                check(ui.visibleMedia.size() == 1 && ui.libraryTable.getCellTooltip(0, 1).contains("audio thumbnail"), "artist search finds real tagged tracks and the tooltip identifies artwork provenance");
                ui.search.clear(); ui.filterLibrary();
            }
            ui.addMedia(0); ++phase; return;
        }
        if (phase == 1)
        {
            check(ui.engine.project.clips.size() == 1 && ui.engine.library[0]->hasPlayback() && ui.engine.library[0]->assets.cover.isValid(), "adding a catalogue track hydrates playback and retains artwork");
            ui.newSession(); check(ui.engine.project.clips.empty() && ui.engine.library.size() == (size_t) sources.size() && !ui.engine.library[0]->hasPlayback(), "New mix retains the permanent library while releasing unused playback mappings");
            ui.openProjectFile(session); ++phase; return;
        }
        if (phase == 2)
        {
            check(ui.engine.library.size() == (size_t) sources.size() && ui.engine.project.clips.empty(), "opening another session merges its sources with the remembered library without adding clips");
            ui.writeLibraryCatalogue(); ++phase; return;
        }
        check(readLibraryCatalogue(index).files.size() == sources.size() + 1, "native persistence keeps all remembered sources plus the unavailable track"); poll.stopTimer(); juce::MessageManager::getInstance()->stopDispatchLoop();
    };
    poll.startTimer(30); juce::MessageManager::getInstance()->runDispatchLoop();
}
}
