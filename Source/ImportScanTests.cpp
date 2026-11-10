#include "MusicScanner.h"
#include "Workstation.h"
#include <cmath>

namespace mixworks
{
void runImportScanTests(const std::function<void(bool,const juce::String&)>& check, const juce::File& report, const juce::StringArray& args)
{
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    const auto inputs = report.getSiblingFile("inputs");
    const auto manifest = juce::JSON::parse(inputs.getChildFile("fixtures.json"));
    if (auto* fixtures = manifest.getArray()) for (const auto& fixture : *fixtures)
    {
        const auto file = inputs.getChildFile(fixture["file"].toString());
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
        bool decoded = reader && reader->lengthInSamples > 0; double energy = 0;
        juce::AudioBuffer<float> block(2,4096);
        if (reader) for (juce::int64 pos=0;decoded && pos<reader->lengthInSamples;pos+=4096)
        {
            const int n = (int) std::min<juce::int64>(4096,reader->lengthInSamples-pos);
            decoded = reader->read(&block,0,n,pos,true,true);
            for (int c=0;c<2;++c) for (int i=0;i<n;++i) { const auto s=block.getSample(c,i); decoded &= std::isfinite(s); energy += s*s; }
        }
        check(decoded && energy > 0, "complete finite audio read: " + file.getFileName());
        if (fixture.hasProperty("frames")) check(reader && reader->lengthInSamples == (juce::int64)fixture["frames"],"exact packet length: " + file.getFileName());
    }
    else check(false,"generated import fixtures are present");
    const int sourceIndex = args.indexOf("--source");
    if (sourceIndex>=0 && sourceIndex+1<args.size())
    {
        const juce::File file(args[sourceIndex+1].unquoted());
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
        bool ok = reader && reader->lengthInSamples == 16160256;
        juce::int64 lastRead = -1;
        juce::AudioBuffer<float> block(2,4096);
        if (reader) for (juce::int64 pos=0;ok && pos<reader->lengthInSamples;pos+=4096)
        {
            lastRead = pos;
            ok = reader->read(&block,0,(int)std::min<juce::int64>(4096,reader->lengthInSamples-pos),pos,true,true);
        }
        check(ok,"previously failing real MP3 reads all 14028 frames; no analysis/cache writes / frames="
            + juce::String(reader ? reader->lengthInSamples : -1) + " / read=" + juce::String(lastRead) + " / file=" + file.getFullPathName());
    }
    const auto scanRoot = report.getSiblingFile("broken-scan"); scanRoot.createDirectory();
    const auto bad = scanRoot.getChildFile("unreadable.wav"); bad.replaceWithText("not an audio file");
    MusicScan scan(scanRoot,{},{}); const auto failed = scan.step();
    const auto text = failed.report.loadFileAsString();
    check(failed.file == bad && !failed.loaded.media && failed.reportError.isEmpty() && text.contains("FAILED")
          && text.contains(bad.getFullPathName()) && text.contains(failed.loaded.error),"failed imports retain the exact filename and decoder reason");
    MusicScan unchanged(scanRoot,{},{{bad.getFullPathName().toLowerCase(),failed.stamp}});
    check(unchanged.step().file == juce::File(),"automatic passes avoid retrying an unchanged failed file continuously");
    MusicScan retry(scanRoot,{},{});
    check(retry.step().file == bad,"explicit retry can read a previously failed source again");
    auto media=std::make_shared<Media>(); media->playbackCacheWarning="Fixture cache write failure";
    const auto warningReport=report.getSiblingFile("warning-report.txt");
    check(writeMusicScanIssue(warningReport,bad,{media,{}}).wasOk()
        && warningReport.loadFileAsString().contains("CACHE WARNING\n" + bad.getFullPathName()),"successful audio with a cache warning is reported separately from an import failure");
}

void runLibraryStartupTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    const auto root = report.getSiblingFile("startup-fixture");
    const auto music = root.getChildFile("Music"), album = music.getChildFile("Album");
    const auto legacy = root.getChildFile("library.json"), preference = root.getChildFile("library-scan.json");
    const auto saved = album.getChildFile("Saved.wav"), cached = album.getChildFile("Previously analysed.wav");
    const auto fresh = music.getChildFile("New.wav"), late = album.getChildFile("Added while open.wav");
    const auto outside = root.getChildFile("Outside.wav");
    album.createDirectory();
    const auto writeTone = [](const juce::File& file, double hz, int length = 6000)
    {
        juce::AudioBuffer<float> audio(1, length);
        for (int i = 0; i < length; ++i) audio.setSample(0, i, float(.2 * std::sin(i * hz * juce::MathConstants<double>::twoPi / 24000.0)));
        juce::WavAudioFormat format;
        if (file.exists() && !file.deleteFile()) return false;
        std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
        auto writer = format.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(24000).withNumChannels(1).withBitsPerSample(16));
        return writer && writer->writeFromAudioSampleBuffer(audio, 0, length);
    };
    const bool audioReady = writeTone(saved, 330) && writeTone(cached, 440) && writeTone(fresh, 550) && writeTone(outside, 660);
    check(audioReady, "create four owned audio sources inside and outside the watched subtree");
    if (!audioReady) return;
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    auto initial = readMedia(formats, saved, {}, AnalysisCacheMode::readWrite, MediaReadPurpose::catalogue).media;
    auto prior = readMedia(formats, cached, {}, AnalysisCacheMode::readWrite, MediaReadPurpose::catalogue).media;
    check(initial && prior && analysisRecordFile(cached).existsAsFile(), "seed independent per-track analysis for the remembered and rediscovered tracks");
    if (!initial || !prior) return;
    initial->manualBpm = 133;
    check(rememberLibraryCatalogue(legacy, {saved}, {initial}).isEmpty(), "seed a legacy catalogue with an existing manual tempo correction");
    const auto legacyBefore = legacy.loadFileAsString();
    const auto sourceStamp = musicFileStamp(saved);
    check(sourceStamp.matches(ScanKnown(*initial)), "source and analysis file stamps match a verified snapshot");
    const auto makeWorkstation = [&](bool configure)
    {
        auto ui = std::make_unique<Workstation>(false);
        ui->stopTimer();
        ui->musicPreference = preference; ui->musicScanEnabled = true;
        if (configure) ui->selectMusicScanFolder(music);
        else ui->musicFolder = ui->musicFolderPreference = readScanPreference(preference).folder;
        ui->catalogueFile = legacy; ui->catalogueRestorePending = true; ui->nextMusicScan = 0;
        return ui;
    };
    auto ui = makeWorkstation(true);
    check(readScanPreference(preference).folder == music, "chosen scan folder survives preference reopen");
    const auto mediaFor = [&](const juce::File& path) -> std::shared_ptr<Media>
    {
        const auto found = std::find_if(ui->engine.library.begin(), ui->engine.library.end(), [&](const auto& row) { return row->file == path; });
        return found == ui->engine.library.end() ? nullptr : *found;
    };
    struct Poll final : juce::Timer { std::function<void()> run; void timerCallback() override { run(); } } poll;
    int phase = 0;
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + 25000;
    poll.run = [&]
    {
        if (juce::Time::getMillisecondCounterHiRes() > deadline)
        {
            check(false, "startup workflow reaches completion, phase " + juce::String(phase) + " / " + ui->musicScanNotice.getText());
            poll.stopTimer(); juce::MessageManager::getInstance()->stopDispatchLoop(); return;
        }
        ui->tickLibraryCatalogue(); ui->tickMusicScan(); ui->refreshMusicScanStatus();
        const bool idle = !ui->catalogueOpening && !ui->catalogueDirty && !ui->catalogueWritePending && !ui->catalogueRefreshPending
            && ui->catalogueRefreshQueue.empty() && !ui->musicScan && !ui->musicScanJobPending;
        if (phase == 0 && idle && ui->musicScanSummary == "Scan complete")
        {
            check(ui->musicChecked == 3 && ui->musicAdded == 2 && ui->musicRefreshed == 0 && ui->musicSaved == 2
                && ui->musicReused == 1 && ui->musicAnalysed == 1 && !ui->dirty && !mediaFor(outside),
                "startup checks the chosen subtree, reuses one analysis, processes one new track and saves both without dirtying the mix");
            auto migrated = mediaFor(saved);
            check(migrated && migrated->id == initial->id && migrated->manualBpm == 133
                && legacy.loadFileAsString() == legacyBefore && libraryStoreFile(legacy).existsAsFile(),
                "migration retains saved identity and manual correction and preserves the original catalogue");
            check(ui->musicFolderWatch && ui->musicFolderWatch->available() && writeTone(saved, 710, 6500),
                "a real watched folder receives a changed source");
            ++phase;
        }
        else if (phase == 1 && idle && ui->musicRefreshed == 1 && ui->musicSaved == 1)
        {
            auto replaced = mediaFor(saved);
            check(replaced && replaced->analysisSource.sha256 != initial->analysisSource.sha256
                && ui->musicUnchanged == 2 && ui->musicAdded == 0, "watch-triggered pass refreshes just the edited source");
            check(writeTone(late, 770), "create a track for the interrupted scan");
            ++phase;
        }
        else if (phase == 2 && ui->musicAdded == 1 && mediaFor(late))
        {
            ui->stopMusicScan();
            ++phase;
        }
        else if (phase == 3 && idle && ui->musicScanSummary == "Scan stopped")
        {
            check(ui->musicSaved == 1 && !ui->dirty, "an interrupted scan commits the track already added");
            ui = makeWorkstation(false);
            ++phase;
        }
        else if (phase == 4 && idle && ui->musicScanSummary == "Scan complete")
        {
            check(ui->engine.library.size() == 4 && mediaFor(late) && !ui->dirty && ui->musicAdded == 0
                && ui->musicRefreshed == 0 && ui->musicAnalysed == 0 && ui->musicReused == 0,
                "fresh workstation restores all four tracks with no duplicate reads, processing or catalogue writes");
            ui->refresh();
            juce::PNGImageFormat png;
            auto output = report.getSiblingFile("startup-library.png").createOutputStream();
            check(output && png.writeImageToStream(ui->createComponentSnapshot(ui->getLocalBounds()), *output), "capture actual Library scan and saved rows");
            poll.stopTimer(); juce::MessageManager::getInstance()->stopDispatchLoop();
        }
    };
    poll.startTimerHz(25);
    juce::MessageManager::getInstance()->runDispatchLoop();
}
}
