#include "Model.h"
#include "LibraryCatalogue.h"
#include <set>
namespace mixworks
{
int runImportRecovery(const juce::StringArray& args)
{
    auto argument=[&](const juce::String& option) { const int at=args.indexOf(option); return at>=0 && at+1<args.size() ? args[at+1].unquoted() : juce::String(); };
    const auto manifestPath=argument("--recover-imports"),cataloguePath=argument("--catalogue"),reportPath=argument("--report");
    for (const auto& path:{manifestPath,cataloguePath,reportPath}) if (!juce::File::isAbsolutePath(path)) return 2;
    const juce::File manifest(manifestPath),catalogue(cataloguePath),report(reportPath);
    if (manifest.getSize()>1024*1024 || report.exists() || report.isSymbolicLink() || catalogue.isSymbolicLink()) return 2;
    const auto doc=juce::JSON::parse(manifest);
    if (!doc["missingPaths"].isArray() || doc["missingPaths"].size()>600) return 2;
    auto saved=readLibraryCatalogue(catalogue); if (saved.error.isNotEmpty()) return 2;
    juce::AudioFormatManager formats; formats.registerBasicFormats(); juce::Array<juce::var> outcomes;
    juce::Array<juce::File> additions; std::vector<std::shared_ptr<Media>> snapshots; std::set<juce::String> seen;
    int failures=0,added=0,reused=0,skipped=0; bool stopped=false;
    juce::String saveIssue;
    auto flush=[&]
    {
        if (additions.isEmpty()) return;
        const auto error=rememberLibraryCatalogue(catalogue,additions,snapshots);
        if (error.isNotEmpty()) saveIssue=error; else { additions.clear(); snapshots.clear(); }
    };
    auto publish=[&]
    {
        juce::var value(new juce::DynamicObject()); auto* object=value.getDynamicObject(); object->setProperty("scope","missing-import-recovery");
        object->setProperty("requested",doc["missingPaths"].size()); object->setProperty("added",added); object->setProperty("reusedAnalysis",reused);
        object->setProperty("skippedExisting",skipped); object->setProperty("failures",failures); object->setProperty("stopped",stopped);
        object->setProperty("catalogueError",saveIssue); object->setProperty("files",outcomes);
        report.getParentDirectory().createDirectory(); juce::TemporaryFile temporary(report);
        return temporary.getFile().replaceWithText(juce::JSON::toString(value),false,false,"\n") && temporary.overwriteTargetFileWithTemporary();
    };
    const auto music=juce::File::getSpecialLocation(juce::File::userMusicDirectory);
    for (const auto& value:*doc["missingPaths"].getArray())
    {
        if (report.withFileExtension("stop").existsAsFile()) { stopped=true; break; }
        if (!value.isString() || !juce::File::isAbsolutePath(value.toString())) return 2;
        const juce::File source(value.toString());
        if (!source.isAChildOf(music) || !source.hasFileExtension("mp3;flac;wav;aif;aiff;ogg") || !seen.insert(source.getFullPathName().toLowerCase()).second) return 2;
        const auto existing=std::find_if(saved.media.begin(),saved.media.end(),[&](const auto& item)
            { return item->file==source && item->analysisVersion==analysisRevision && item->sourceModified==source.getLastModificationTime().toMilliseconds() && item->analysisSource.bytes==source.getSize(); });
        juce::var row(new juce::DynamicObject()); auto* record=row.getDynamicObject(); record->setProperty("source",source.getFullPathName());
        if (existing!=saved.media.end()) { ++skipped; record->setProperty("status","existing analysis retained; no decode or analysis"); }
        else
        {
            const auto began=juce::Time::getMillisecondCounterHiRes();
            const auto loaded=readMedia(formats,source,{},AnalysisCacheMode::readWrite,MediaReadPurpose::catalogue);
            record->setProperty("milliseconds",juce::Time::getMillisecondCounterHiRes()-began);
            if (!loaded.media) { ++failures; record->setProperty("status","failed"); record->setProperty("reason",loaded.error); }
            else
            {
                ++added; reused+=loaded.media->analysisFromCache; additions.add(source); snapshots.push_back(loaded.media);
                record->setProperty("status","imported"); record->setProperty("analysisFromCache",loaded.media->analysisFromCache); record->setProperty("decodedFromCache",loaded.media->decodedFromCache);
                record->setProperty("warning",loaded.media->analysisCacheWarning+" "+loaded.media->playbackCacheWarning);
                record->setProperty("frames",loaded.media->frames());
            }
        }
        outcomes.add(row); if (additions.size()>=100) flush(); if (!publish()) return 2;
        if (saveIssue.isNotEmpty()) break;
    }
    flush(); if (!publish()) return 2;
    return failures==0 && saveIssue.isEmpty() && !stopped ? 0 : 1;
}
}
