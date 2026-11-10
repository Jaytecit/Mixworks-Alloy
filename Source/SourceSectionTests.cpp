#include "Workstation.h"
#include "SourceSectionStore.h"
#include "EnergySections.h"
#include "Hotcues.h"
#include "DownbeatReferences.h"
#include "LibraryCatalogue.h"
#include <cmath>
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace mixworks
{
void runSourceSectionTests(const std::function<void(bool,const juce::String&)>& check,const juce::File& report)
{
    auto media = std::make_shared<Media>(); media->name = "Reusable section source"; media->sampleRate = 8000;
    media->manualBpm = 120; media->manualBeatOrigin = .25; media->manualKey = 7; media->file = report.getSiblingFile("reusable-sections.wav");
    media->samples.setSize(1,96 * 8000);
    for (int n = 0; n < media->frames(); ++n) media->samples.setSample(0,n,(float) (.12 * std::sin(n * .187)));
    media->prepareWaveformBody();
    const int bucket = std::max(1,media->frames() / 4000);
    for (int first = 0; first < media->frames(); first += bucket)
        media->peaks.push_back(media->waveformRange(first / media->sampleRate,std::min(first + bucket,media->frames()) / media->sampleRate).peak);
    media->analysis.bpm = 120; media->analysis.downbeatOrigin = .125; media->analysis.downbeatMethod = "signal-accent";
    {
        juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream = media->file.createOutputStream();
        auto writer = wav.createWriterFor(stream,juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(1).withBitsPerSample(24));
        check(writer && writer->writeFromAudioSampleBuffer(media->samples,0,media->frames()),"write owned source for reusable S/downbeat persistence");
    }
    media->analysisSource = {audioDigest(media->file),media->file.getSize(),8000,media->frames(),1}; media->sourceModified = media->file.getLastModificationTime().toMilliseconds();
    check(parseAnalysisRecord(analysisRecordToJson(media->analysisRecord())).validAutomatic,"source-section fixture has a complete valid automatic analysis snapshot");
    SourceSections original = std::vector<SourceSectionMarker>{{"section-a",24,0,true},{"section-b",56,0,true}};
    check(writeSourceSectionRecord(media->file,media->analysisSource,media->sourceModified,{},original).isEmpty()
        && readSourceSectionRecord(media->analysisSource).markers == original,"track section record round-trips stable source identities and positions");
    media->sectionTemplate = original; const auto sourceFile = sourceSectionRecordFile(media->analysisSource); const auto savedText = sourceFile.loadFileAsString();
    auto legacySections = juce::JSON::parse(savedText); legacySections.getDynamicObject()->setProperty("version",1);
    for (auto& item : *legacySections["sections"].getArray()) item.getDynamicObject()->removeProperty("label");
    sourceFile.replaceWithText(juce::JSON::toString(legacySections));
    check(readSourceSectionRecord(media->analysisSource).markers == original,"version1 source sections without labels remain reusable");
    sourceFile.replaceWithText(savedText,false,false,"\n");
    auto changed = original; (*changed)[0].sourceSeconds = 25;
    check(writeSourceSectionRecord(media->file,media->analysisSource,media->sourceModified,{},changed).isNotEmpty() && sourceFile.loadFileAsString() == savedText,
        "stale expected template cannot overwrite another saved source edit");
    auto invalid = *original; invalid[1].sourceSeconds = 20;
    check(writeSourceSectionRecord(media->file,media->analysisSource,media->sourceModified,original,invalid).isNotEmpty() && sourceFile.loadFileAsString() == savedText,
        "unordered source sections reject before replacing saved track data");
    auto unknown = juce::JSON::parse(savedText); unknown.getDynamicObject()->setProperty("version",99); sourceFile.replaceWithText(juce::JSON::toString(unknown));
    check(readSourceSectionRecord(media->analysisSource).error.isNotEmpty()
        && writeSourceSectionRecord(media->file,media->analysisSource,media->sourceModified,original,changed).isNotEmpty(),"newer source section storage is kept and never silently downgraded");
    sourceFile.replaceWithText(savedText,false,false,"\n");
    auto otherIdentity = media->analysisSource; otherIdentity.sha256 = juce::String::repeatedString("a",64);
    check(!readSourceSectionRecord(otherIdentity).exists,"different audio content does not inherit another track's section defaults");
    auto uiOwned = std::make_unique<Workstation>(false); auto& ui = *uiOwned; ui.stopTimer(); ui.setSize(1920,1080);
    Clip clip; clip.media = media; clip.length = 96; initialiseHotcues(clip);
    ui.engine.project.clips = {clip}; ui.engine.project.transitionLinksKnown = true; ui.engine.library = {media}; ui.selectedClip = clip.id; ui.selectTrack(0); ui.showEnergySections = true; ui.refresh();
    const auto cues = clip.hotcues; const auto sourceDigest = audioDigest(media->file); const auto rms = media->waveformRms;
    ui.timeline.chooseCurveTool(8);
    ui.remember(); ui.engine.project.clips[0].gainDb = -2; // Older unrelated history must not clear a pre-existing template.
    const auto flags = ui.timeline.sectionFlags(true); check(flags.size() == 2,"new placed segment consumes track section defaults without materialising an instance edit");
    if (flags.size() != 2) return;
    check(ui.timeline.setSectionFlag(flags[0],25,true) && media->sectionTemplate == changed && readSourceSectionRecord(media->analysisSource).markers == changed
        && ui.engine.project.clips[0].hotcues == cues,"S edit replaces saved track boundary while existing HC points remain independent");
    ui.undo(false); check(media->sectionTemplate == original && readSourceSectionRecord(media->analysisSource).markers == original && !ui.engine.project.clips[0].sectionMarkers,
        "Undo restores exact prior track defaults and unmaterialised instance state");
    ui.undo(false); check(media->sectionTemplate == original && readSourceSectionRecord(media->analysisSource).markers == original,
        "older unrelated Undo preserves pre-existing saved source section defaults");
    ui.undo(true); ui.undo(true); check(media->sectionTemplate == changed,"Redo restores source defaults through the canonical history owner");
    {
        const auto beforeDrag = juce::JSON::toString(projectToJson(ui.engine.project),true,17); const auto beforeDisk = sourceFile.loadFileAsString(); const auto beforeHistory = ui.undoHistory.size();
        const auto flag = ui.timeline.sectionFlags(true)[0]; const auto point = flag.bounds.getCentre(); const auto mouse = juce::Desktop::getInstance().getMainMouseSource();
        auto event = [&](juce::Point<float> at,bool drag)
        { return juce::MouseEvent(mouse,at,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::altModifier),1,0,0,0,0,&ui.timeline,&ui.timeline,juce::Time::getCurrentTime(),point,juce::Time::getCurrentTime(),1,drag); };
        ui.timeline.mouseDown(event(point,false)); ui.timeline.mouseDrag(event(point.translated(12,0),true));
        check(sourceFile.loadFileAsString() == beforeDisk,"S drag previews avoid repeated source-file writes before mouse-up");
        ui.timeline.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));
        check(juce::JSON::toString(projectToJson(ui.engine.project),true,17) == beforeDrag && ui.undoHistory.size() == beforeHistory && sourceFile.loadFileAsString() == beforeDisk,
            "Escape cancels S drag and restores arrangement, source defaults and Undo baseline");
        ui.timeline.mouseDown(event(point,false)); ui.timeline.mouseDrag(event(point.translated(12,0),true)); ui.timeline.mouseUp(event(point.translated(12,0),true));
        check(media->sectionTemplate == ui.engine.project.clips[0].sectionMarkers && media->sectionTemplate != changed
            && readSourceSectionRecord(media->analysisSource).markers == media->sectionTemplate && ui.undoHistory.size() == beforeHistory + 1,
            "actual S mouse-up commits one source save and one arrangement Undo");
        ui.undo(false); check(media->sectionTemplate == changed,"Undo completed S drag restores the reusable track template");
    }
    auto future = clip; future.id = juce::Uuid().toString(); future.sectionMarkers.reset();
    check(clipEnergySections(future)[0].sourceSeconds == 25 && media->catalogueCopy()->sectionTemplate == changed,
        "future instance and lightweight library snapshot reuse the updated section template");
    ui.timeline.resetSectionFlags(clip.id);
    check(!media->sectionTemplate && !readSourceSectionRecord(media->analysisSource).markers && readSourceSectionRecord(media->analysisSource).exists
        && ui.engine.project.clips[0].hotcues == cues,"Use suggestions saves an explicit reset without deleting storage or changing hotcues");
    ui.undo(false); check(media->sectionTemplate == changed,"Undo reset restores saved section defaults");
    const auto beforeFailure = juce::JSON::toString(projectToJson(ui.engine.project),true,17); const auto history = ui.undoHistory.size();
    sourceFile.replaceWithText("unidentified section data");
    check(!ui.timeline.setSectionFlag(ui.timeline.sectionFlags(true)[0],26,true)
        && ui.undoHistory.size() == history && juce::JSON::toString(projectToJson(ui.engine.project),true,17) == beforeFailure
        && sourceFile.loadFileAsString() == "unidentified section data","failed durable S save keeps instance/history and unidentified storage unchanged");
    sourceFile.replaceWithText(juce::JSON::toString(juce::JSON::parse(savedText),false,17));
    // Restore the actual current template through the owned fixture, not by retrying a mutation.
    auto restored = juce::JSON::parse(savedText); restored.getDynamicObject()->setProperty("sections",sourceSectionsToJson(changed)); sourceFile.replaceWithText(juce::JSON::toString(restored,false,17));
    auto pump = []
    {
        const auto until = juce::Time::getMillisecondCounterHiRes() + 40;
        do {
#if JUCE_WINDOWS
            MSG message; while (PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
#endif
            juce::Thread::sleep(1);
        } while (juce::Time::getMillisecondCounterHiRes() < until);
    };
    ui.addToDesktop(0); ui.setVisible(true); ui.refresh();
    {
        const auto baseline = juce::JSON::toString(projectToJson(ui.engine.project),true,17); const auto history = ui.undoHistory.size();
        const auto menu = ui.timelineMenu(40.12,0); bool enabled = false;
        for (juce::PopupMenu::MenuItemIterator i(menu, true); i.next();)
            if (i.getItem().itemID == (int) TimelineAction::sectionBoundary) enabled = i.getItem().isEnabled;
        check(enabled,"segment context menu exposes Add Section Boundary Here");
        auto openBoundary = [&]()
        {
            ui.performTimelineAction(TimelineAction::sectionBoundary,40.12,0); pump();
            return dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        };
        auto* boundary = openBoundary();
        check(boundary && boundary->getTextEditor("label") && boundary->getTextEditor("time"),"section menu action opens native label and grid-position editor");
        if (boundary) { boundary->getTextEditor("label")->setText("Cancelled"); boundary->exitModalState(0); pump(); }
        check(juce::JSON::toString(projectToJson(ui.engine.project),true,17) == baseline && ui.undoHistory.size() == history && media->sectionTemplate == changed,
            "cancelled section creation leaves arrangement, track defaults and history unchanged");
        const double snapped = ui.engine.project.snappedTime(40.12,true);
        boundary = openBoundary();
        if (boundary)
        {
            boundary->getTextEditor("label")->setText(juce::String::fromUTF8("Chorus / Refrain"));
            auto image = report.getSiblingFile("add-section-dialog.png").createOutputStream(); juce::PNGImageFormat png;
            check(image && png.writeImageToStream(boundary->createComponentSnapshot(boundary->getLocalBounds()),*image),"capture actual Add section boundary editor");
            boundary->exitModalState(1); pump();
        }
        const auto labelled = ui.engine.project.clips[0].sectionMarkers;
        check(labelled && labelled->size() == 3 && (*labelled)[1].sourceSeconds == snapped && (*labelled)[1].label == "Chorus / Refrain"
            && (*labelled)[1].manual && (*labelled)[0].id == (*changed)[0].id && media->sectionTemplate == labelled
            && readSourceSectionRecord(media->analysisSource).markers == labelled && ui.undoHistory.size() == history + 1,
            "create at clicked grid point saves a sorted labelled source boundary with stable neighbours and one Undo");
        if (labelled && labelled->size() == 3)
        {
            const auto beforeDuplicate = juce::JSON::toString(projectToJson(ui.engine.project),true,17);
            ui.performTimelineAction(TimelineAction::sectionBoundary,snapped,0);
            check(!juce::Component::getCurrentlyModalComponent() && juce::JSON::toString(projectToJson(ui.engine.project),true,17) == beforeDuplicate,
                "duplicate grid boundary rejects without another marker or dialog");
            ui.undo(false); check(media->sectionTemplate == changed && readSourceSectionRecord(media->analysisSource).markers == changed,"Undo creation restores previous reusable track defaults");
            ui.undo(true); check(media->sectionTemplate == labelled,"Redo creation restores labelled source boundary");
            auto flag = ui.timeline.sectionFlags(true)[1];
            check(ui.timeline.setSectionFlag(flag,snapped + .123456789,true),"move newly labelled S flag through durable edit path");
            flag = ui.timeline.sectionFlags(true)[1]; ui.timeline.editSectionFlag(flag); pump();
            boundary = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
            if (boundary) { boundary->getTextEditor("label")->setText("Drop"); boundary->exitModalState(1); pump(); }
            const auto renamed = ui.engine.project.clips[0].sectionMarkers;
            check(renamed && (*renamed)[1].sourceSeconds == snapped + .123456789 && (*renamed)[1].id == (*labelled)[1].id && (*renamed)[1].label == "Drop",
                "label-only edit preserves exact moved S position and identity despite rounded time display");
            const auto session = report.getSiblingFile("labelled-sections.mixworks"); auto reopened = std::make_unique<Engine>(juce::File(),false);
            const auto saved = ui.engine.saveProject(session), opened = reopened->restoreProject(session.loadFileAsString());
            check(saved.isEmpty() && opened.isEmpty() && reopened->project.clips.size() == 1 && reopened->project.clips[0].sectionMarkers == renamed,
                "actual session Save/Open preserves manually added label and moved S position");
            juce::AudioFormatManager formats; formats.registerBasicFormats();
            const auto fresh = readMedia(formats,media->file,{},AnalysisCacheMode::readOnly,MediaReadPurpose::catalogue);
            check(fresh.media && fresh.media->sectionTemplate == renamed,"fresh source import reuses saved label and moved S position independently of the arrangement");
            for (int width : {1250,1920})
            {
                ui.setSize(width,1080); ui.refresh(); auto stream = report.getSiblingFile("labelled-sections-" + juce::String(width) + ".png").createOutputStream(); juce::PNGImageFormat png;
                check(stream && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()),*stream),"capture labelled section strip at width " + juce::String(width));
            }
            // Restore the pre-existing fixture through real Undo so the remaining source/downbeat checks keep their original baseline.
            ui.undo(false); ui.undo(false); ui.undo(false);
            check(media->sectionTemplate == changed,"Undo rename, move and creation restores the original reusable sections");
        }
    }
    check(ui.timeline.curveTools->saveDownbeat.isVisible() && ui.timeline.curveTools->saveDownbeat.isEnabled(),"segment automation toolbar exposes the actual Save beat1 button");
    ui.timeline.curveTools->saveDownbeat.onClick(); pump(); auto* dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
    check(dialog && dialog->getTextEditor("source"),"actual automation-toolbar button opens the source downbeat editor");
    if (dialog) { dialog->getTextEditor("source")->setText("1.125"); dialog->exitModalState(1); pump(); }
    const auto stored = readAnalysisRecord(media->file,media->analysisSource);
    check(media->manualBeatOrigin == 1.125 && media->manualBpm == 120 && media->manualKey == 7 && stored.record && stored.record->manualBeatOrigin == 1.125
        && media->sectionTemplate == changed && ui.engine.project.clips[0].hotcues == cues,
        "Save beat1 overwrites the source downbeat durably while preserving BPM/key, S defaults and HC points");
    ui.timeline.curveTools->saveDownbeat.onClick(); pump(); dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
    if (dialog) { dialog->getTextEditor("source")->setText("2.5"); dialog->exitModalState(0); pump(); }
    check(media->manualBeatOrigin == 1.125,"Cancel downbeat editor keeps the stored track anchor");
    const auto references = downbeatReferenceRoot().findChildFiles(juce::File::findFiles,false,media->analysisSource.sha256 + "-*.json");
    check(references.size() == 1,"one successful downbeat choice creates one independent dated comparison; Cancel creates none");
    const auto referenceText = references.isEmpty() ? juce::String() : references[0].loadFileAsString(); const auto reference = juce::JSON::parse(referenceText);
    check(reference["format"].toString() == "mixworks-downbeat-reference" && (double) reference["analysis"]["downbeatOrigin"] == .125
        && (double) reference["manual"]["beatOrigin"] == 1.125 && (double) reference["previousManualAnchor"] == .25
        && reference["source"]["sha256"].toString() == media->analysisSource.sha256 && reference["recordedUtc"].isString(),
        "comparison preserves detected anchor, chosen/previous anchors, detector revision, date and verified audio identity");
    ui.engine.project.clips[0].offset = 2; ui.engine.project.clips[0].rate = 1.25; ui.engine.project.clips[0].length = 70;
    ui.engine.project.clips[0].introAnchor = 3.123456789;
    const auto& mapped = ui.engine.project.clips[0]; const double inSource = mapped.sourceTimeAt(mapped.transitionTime(true));
    ui.editSavedDownbeat(clip.id); pump(); dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
    if (dialog)
    {
        dialog->getButton(2)->triggerClick(); pump();
        check(dialog == juce::Component::getCurrentlyModalComponent() && dialog->getTextEditorContents("source") == juce::String(inSource,6)
            && media->manualBeatOrigin == 1.125,"Use IN reviews the mapped source position without closing or saving");
        auto image = report.getSiblingFile("saved-downbeat-dialog.png").createOutputStream(); juce::PNGImageFormat png;
        check(image && png.writeImageToStream(dialog->createComponentSnapshot(dialog->getLocalBounds()),*image),"capture actual downbeat review with automatic comparison and explicit IN choice");
        dialog->exitModalState(1); pump();
    }
    check(media->manualBeatOrigin == inSource,"explicit IN save preserves full source precision through trim and playback rate");
    ui.engine.position = 8.234567891;
    const double playheadSource = ui.engine.project.clips[0].sourceTimeAt(ui.engine.position.load() - mapped.start);
    ui.editSavedDownbeat(clip.id); pump(); dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
    if (dialog) { dialog->getButton(1)->triggerClick(); pump(); dialog->exitModalState(1); pump(); }
    check(media->manualBeatOrigin == playheadSource && ui.engine.position.load() == 8.234567891,"explicit playhead save maps exact source seconds without seeking");
    ui.editSavedDownbeat(clip.id); pump(); dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
    if (dialog) { dialog->getTextEditor("source")->setText("999"); dialog->exitModalState(1); pump(); }
    check(media->manualBeatOrigin == playheadSource,"out-of-source downbeat rejects without replacing the saved choice");
    ui.editSavedDownbeat(clip.id); pump(); dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
    if (dialog) { ui.engine.project.clips[0].start += 1; dialog->getTextEditor("source")->setText("2"); dialog->exitModalState(1); pump(); }
    check(media->manualBeatOrigin == playheadSource,"stale downbeat editor refuses after arrangement geometry changes");
    const auto referenceRoot = downbeatReferenceRoot(), retainedReferences = referenceRoot.getSiblingFile("downbeat-reference-failure-control");
    const bool movedReferences = !retainedReferences.exists() && referenceRoot.moveFileTo(retainedReferences);
    check(movedReferences && referenceRoot.create().wasOk(),"construct owned unavailable reference-storage control");
    if (movedReferences)
    {
        auto correction = SourceCorrection::from(*media); correction.downbeat = 2;
        check(ui.applySourceCorrection(media,correction).failed() && media->manualBeatOrigin == playheadSource
            && readAnalysisRecord(media->file,media->analysisSource).record->manualBeatOrigin == playheadSource,
            "unavailable comparison storage rejects source overwrite and restores the prior in-memory correction");
        check(referenceRoot.deleteFile() && retainedReferences.moveFileTo(referenceRoot),"restore owned reference-storage control without deleting observations");
    }
    media->analysis.downbeatOrigin = .625;
    auto reviewed = SourceCorrection::from(*media); reviewed.downbeat = 1.125;
    check(ui.applySourceCorrection(media,reviewed).wasOk() && !references.isEmpty() && references[0].loadFileAsString() == referenceText,
        "a later detector result and corrected choice append evidence without overwriting the original comparison");
    const auto catalogue = report.getSiblingFile("source-sections-catalogue.json");
    check(rememberLibraryCatalogue(catalogue,{media->file},{media}).isEmpty(),"save source section defaults in the lightweight catalogue");
    const auto loadedCatalogue = readLibraryCatalogue(catalogue);
    check(!loadedCatalogue.media.empty() && loadedCatalogue.media[0]->sectionTemplate == changed,"catalogue reopen retains reusable section defaults");
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    const auto reimported = readMedia(formats,media->file,{},AnalysisCacheMode::readOnly,MediaReadPurpose::catalogue);
    check(reimported.media && reimported.media->sectionTemplate == changed && reimported.media->manualBeatOrigin == 1.125,
        "actual fresh source read reuses durable section defaults and corrected downbeat: " + reimported.error);
    check(audioDigest(media->file) == sourceDigest && media->waveformRms == rms,"S/downbeat persistence never rewrites original music or retained waveform analysis");
    for (int width : {1250,1920})
    {
        ui.setSize(width,1080); ui.refresh(); auto stream = report.getSiblingFile("source-sections-" + juce::String(width) + ".png").createOutputStream(); juce::PNGImageFormat png;
        check(stream && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()),*stream),"capture reusable S/default and downbeat toolbar at width " + juce::String(width));
    }
}
}
