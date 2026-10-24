#include "Workstation.h"
#include "AppWindow.h"
#include "RhythmDecoder.h"

#include "PromoControl.h"
#include <juce_cryptography/juce_cryptography.h>
namespace mixworks { int runSelfTests(const juce::String& commandLine); }
namespace mixworks { int runImportRecovery(const juce::StringArray&); }
namespace
{
int migrateAnalysis(const juce::StringArray& args)
{
    const int inputIndex = args.indexOf("--migrate-analysis"), reportIndex = args.indexOf("--report");
    if (inputIndex < 0 || inputIndex + 1 >= args.size() || reportIndex < 0 || reportIndex + 1 >= args.size()
        || !juce::File::isAbsolutePath(args[inputIndex + 1].unquoted()) || !juce::File::isAbsolutePath(args[reportIndex + 1].unquoted())) return 1;
    const juce::File folder(args[inputIndex + 1].unquoted()), report(args[reportIndex + 1].unquoted());
    if (!folder.isDirectory() || folder.isSymbolicLink() || report.exists() || report.isSymbolicLink()
        || !report.hasFileExtension("json") || report.getFileName().endsWithIgnoreCase(".mixworks-analysis.json")) return 1;
    juce::Array<juce::var> rows;
    for (const auto& entry : juce::RangedDirectoryIterator(folder, false, "*.mixworks-analysis.json", juce::File::findFiles, juce::File::FollowSymlinks::no))
    {
        const auto legacy = entry.getFile(); if (legacy.isSymbolicLink()) continue;
        const auto audio = legacy.getSiblingFile(legacy.getFileName().dropLastCharacters(juce::String(".mixworks-analysis.json").length()));
        if (!audio.hasFileExtension("wav;aif;aiff;flac;ogg;mp3")) continue;
        auto* row = new juce::DynamicObject(); row->setProperty("source", audio.getFullPathName()); row->setProperty("legacy", legacy.getFullPathName());
        row->setProperty("destination", mixworks::analysisRecordFile(audio).getFullPathName());
        row->setProperty("error", mixworks::migrateLegacyAnalysisRecord(audio)); rows.add(juce::var(row));
    }
    if (report.getParentDirectory().createDirectory().failed()) return 1;
    return report.replaceWithText(juce::JSON::toString(rows), false, false, "\n") ? 0 : 1;
}
int prepareSavedLibrary(const juce::StringArray& args)
{
    const auto index = args.indexOf("--prepare-library");
    if (index < 0 || index + 1 >= args.size() || !juce::File::isAbsolutePath(args[index + 1].unquoted())) return 1;
    const juce::File file(args[index + 1].unquoted()); const auto started = juce::Time::getMillisecondCounterHiRes();
    auto saved = mixworks::readLibraryCatalogue(file); if (saved.error.isNotEmpty()) return 1;
    const auto snapshotReadMs = juce::Time::getMillisecondCounterHiRes() - started;
    juce::AudioFormatManager formats; formats.registerBasicFormats(); auto snapshots = saved.media; int unavailable = 0;
    for (const auto& source : saved.files)
    {
        if (std::any_of(snapshots.begin(), snapshots.end(), [&](const auto& m) { return m->file == source; })) continue;
        auto loaded = mixworks::readMedia(formats, source, {}, mixworks::AnalysisCacheMode::readOnly, mixworks::MediaReadPurpose::catalogue);
        if (loaded.media) snapshots.push_back(std::move(loaded.media)); else ++unavailable;
    }
    const auto error = mixworks::rememberLibraryCatalogue(file, saved.files, snapshots);
    const auto reportIndex = args.indexOf("--report");
    if (reportIndex >= 0 && reportIndex + 1 < args.size() && juce::File::isAbsolutePath(args[reportIndex + 1].unquoted()))
    {
        juce::var report(new juce::DynamicObject()); auto* object = report.getDynamicObject(); object->setProperty("tracks", (int) snapshots.size());
        object->setProperty("unavailable", unavailable); object->setProperty("error", error); object->setProperty("elapsedMs", juce::Time::getMillisecondCounterHiRes() - started);
        object->setProperty("snapshotReadMs", snapshotReadMs);
        juce::File(args[reportIndex + 1].unquoted()).replaceWithText(juce::JSON::toString(report));
    }
    return error.isEmpty() ? 0 : 1;
}
int analyseFiles(const juce::StringArray& args)
{
    auto destination = juce::File::getCurrentWorkingDirectory().getChildFile(".local/analysis.json");
    juce::Array<juce::var> rows; juce::AudioFormatManager formats; formats.registerBasicFormats(); int failures = 0;
    for (int i = 0; i < args.size(); ++i)
    {
        if (args[i] == "--analyse") continue;
        if (args[i] == "--report" && i + 1 < args.size()) { destination = juce::File(args[++i].unquoted()); continue; }
        const auto path = args[i].unquoted();
        auto* row = new juce::DynamicObject(); row->setProperty("file", path);
        mixworks::MediaResult loaded;
        if (juce::File::isAbsolutePath(path)) loaded = mixworks::readMedia(formats, juce::File(path), {}, mixworks::AnalysisCacheMode::readOnly);
        else loaded.error = "An absolute audio path is required.";
        if (!loaded.media) { ++failures; row->setProperty("error", loaded.error); }
        else
        {
            const auto& a = loaded.media->analysis;
            row->setProperty("analysisFromCache", loaded.media->analysisFromCache);
            row->setProperty("decodedFromCache", loaded.media->decodedFromCache);
            row->setProperty("bpm", a.bpm); row->setProperty("beatOriginSeconds", a.beatOrigin); row->setProperty("tempoConfidence", a.tempoConfidence); row->setProperty("variableTempo", a.variableTempo); row->setProperty("gridErrorMs", a.gridErrorMs);
            row->setProperty("downbeatOriginSeconds", a.downbeatOrigin); row->setProperty("downbeatConfidence", a.downbeatConfidence);
            row->setProperty("downbeatExaminedSeconds", a.downbeatExaminedSeconds);
            juce::Array<juce::var> windows;
            for (const auto& w : a.beatWindows)
            {
                auto* estimate = new juce::DynamicObject(); estimate->setProperty("sourceStartSeconds", w.sourceStart);
                estimate->setProperty("bpm", w.bpm); estimate->setProperty("beatOriginSeconds", w.origin); estimate->setProperty("confidence", w.confidence);
                windows.add(juce::var(estimate));
            }
            row->setProperty("beatWindows", windows);
            row->setProperty("key", mixworks::keyNoteName(a.key)); row->setProperty("camelot", mixworks::keyCamelot(a.key)); row->setProperty("keyConfidence", a.keyConfidence); row->setProperty("alternativeKey", mixworks::keyNoteName(a.alternativeKey));
            row->setProperty("tuningCents", a.tuningCents); row->setProperty("examinedSeconds", a.examinedSeconds); row->setProperty("durationSeconds", loaded.media->duration());
        }
        rows.add(juce::var(row));
    }
    destination.getParentDirectory().createDirectory();
    return destination.replaceWithText(juce::JSON::toString(rows)) && failures == 0 && !rows.isEmpty() ? 0 : 1;
}
}
class MixworksApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Mixworks Alloy"; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void initialise(const juce::String& commandLine) override
    {
        juce::LookAndFeel::setDefaultLookAndFeel(&look);
        auto args = juce::StringArray::fromTokens(commandLine, true); args.removeEmptyStrings();
        if (args.contains("--rhythm-research")) { setApplicationReturnValue(mixworks::runRhythmResearch(args)); quit(); return; }
        if (args.contains("--self-test")) { setApplicationReturnValue(mixworks::runSelfTests(commandLine)); quit(); return; }
        if (args.contains("--analyse")) { setApplicationReturnValue(analyseFiles(args)); quit(); return; }
        if (args.contains("--recover-imports")) { setApplicationReturnValue(mixworks::runImportRecovery(args)); quit(); return; }
        if (args.contains("--migrate-analysis")) { setApplicationReturnValue(migrateAnalysis(args)); quit(); return; }
        if (args.contains("--prepare-library")) { setApplicationReturnValue(prepareSavedLibrary(args)); quit(); return; }
        if (args.contains("--promo-control") || args.contains("--dev-control"))
        {
            const bool development = args.contains("--dev-control");
            const int at = args.indexOf(development ? "--dev-control" : "--promo-control");
            if (args.size() != 2 || at != 0 || !juce::File::isAbsolutePath(args[1].unquoted())) { setApplicationReturnValue(2); quit(); return; }
            const juce::File root(args[1].unquoted());
            promoLease = std::make_unique<juce::InterProcessLock>("AlloyPromo-" + juce::SHA256(root.getFullPathName().toLowerCase().toUTF8()).toHexString());
            if (!promoLease->enter(0) || mixworks::PromoControl::prepare(root).failed()) { setApplicationReturnValue(2); quit(); return; }
            auto* workstation = new mixworks::Workstation(false);
            window = std::make_unique<mixworks::AppWindow>(workstation);
            window->setName(development ? "Mixworks Alloy - Development" : "Mixworks Alloy - Promo"); window->setVisible(true);
            promo = std::make_unique<mixworks::PromoControl>(*workstation,root);
            const auto error=workstation->engine.startDevice(false);
            if (error.isNotEmpty()) workstation->status="Promo audio device: "+error;
            return;
        }
        window = std::make_unique<mixworks::AppWindow>(new mixworks::Workstation());
        window->setVisible(true);
        if (auto* workstation = dynamic_cast<mixworks::Workstation*>(window->getContentComponent()))
        {
            if (args.size() == 1 && juce::File::isAbsolutePath(args[0].unquoted()) && juce::File(args[0].unquoted()).hasFileExtension("mixworks")) workstation->openProjectFile(juce::File(args[0].unquoted()));
            else { juce::Array<juce::File> files; for (const auto& arg : args) if (juce::File::isAbsolutePath(arg.unquoted())) files.add(juce::File(arg.unquoted())); if (!files.isEmpty()) workstation->importFiles(files); }
        }
    }
    void shutdown() override { promo.reset(); window.reset(); if(promoLease) promoLease->exit(); promoLease.reset(); juce::LookAndFeel::setDefaultLookAndFeel(nullptr); }
    void systemRequestedQuit() override
    {
        if (window) window->closeButtonPressed(); else quit();
    }
private:
    mixworks::AlloyLookAndFeel look;
    std::unique_ptr<mixworks::AppWindow> window;
    std::unique_ptr<mixworks::PromoControl> promo;
    std::unique_ptr<juce::InterProcessLock> promoLease;
};
START_JUCE_APPLICATION(MixworksApplication)
