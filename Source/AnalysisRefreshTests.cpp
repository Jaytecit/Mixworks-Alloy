#include "Workstation.h"
#include "Beatmatch.h"
#include <cmath>
#include <limits>

namespace mixworks
{
void runFocusedSignalTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runFocusedRealProbe(const std::function<void(bool, const juce::String&)>& check, const juce::File& report, const juce::File& manifest)
{
    const auto sources = juce::JSON::parse(manifest);
    check(sources.isArray() && sources.size() > 0 && sources.size() <= 12, "bounded real-source probe manifest");
    if (!sources.isArray() || sources.size() > 12) return;
    juce::AudioFormatManager formats; formats.registerBasicFormats(); juce::Array<juce::var> outcomes;
    for (const auto& entry : *sources.getArray())
    {
        const juce::File file(entry["path"].toString()); const auto old = parseAnalysisRecord(entry["record"]);
        check(old.reusableAutomatic && old.record.has_value(), "source-identity-bound baseline: " + file.getFileName()); if (!old.record) continue;
        const auto began = juce::Time::getMillisecondCounterHiRes();
        const auto result = readMedia(formats, file, {}, AnalysisCacheMode::focused, MediaReadPurpose::catalogue, &*old.record);
        check(result.media && result.error.isEmpty(), "real-source retry completes: " + file.getFileName() + " " + result.error); if (!result.media) continue;
        const auto& m = *result.media; const auto& a = m.analysis;
        check(m.analysisSource == old.record->source && audioDigest(file) == old.record->source.sha256
            && m.manualBpm == old.record->manualBpm && m.manualBeatOrigin == old.record->manualBeatOrigin && m.manualKey == old.record->manualKey,
            "original audio and manual corrections unchanged: " + file.getFileName());
        check(a.focusedTempo != 2 || (!a.variableTempo && a.tempoConfidence > old.record->analysis.tempoConfidence), "real tempo adoption has stronger consistent evidence");
        check(a.focusedKey != 2 || a.keyConfidence > old.record->analysis.keyConfidence, "real key adoption has stronger evidence");
        auto* row = new juce::DynamicObject(); row->setProperty("path", file.getFullPathName());
        row->setProperty("milliseconds", juce::Time::getMillisecondCounterHiRes() - began);
        row->setProperty("before", entry["record"]); row->setProperty("after", analysisRecordToJson(m.analysisRecord())); outcomes.add(juce::var(row));
    }
    check(report.getSiblingFile("real-retry-results.json").replaceWithText(juce::JSON::toString(juce::var(outcomes))), "retain independent real-source numerical outcomes (not ground truth)");
}
void runFreshAnalysisProbe(const std::function<void(bool,const juce::String&)>& check, const juce::File& report, const juce::StringArray& args)
{
    const int index=args.indexOf("--analysis-source");
    const bool valid=index>=0 && index+1<args.size() && juce::File::isAbsolutePath(args[index+1].unquoted());
    check(valid,"one absolute source supplied for isolated baseline analysis"); if(!valid)return;
    const juce::File source(args[index+1].unquoted());
    const bool isolated=analysisStoreRoot().isAChildOf(report.getParentDirectory());
    check(isolated && source.existsAsFile(),"analysis output is isolated and original source exists");if(!isolated || !source.existsAsFile())return;
    const auto digest=audioDigest(source);const auto size=source.getSize();const auto modified=source.getLastModificationTime();
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    const auto loaded=readMedia(formats,source,{},AnalysisCacheMode::refresh,MediaReadPurpose::catalogue);
    check(loaded.media && loaded.error.isEmpty(),"fresh catalogue analysis completes: "+source.getFileName()+" / "+loaded.error);
    check(source.getSize()==size && source.getLastModificationTime()==modified && audioDigest(source)==digest,"original source bytes and modification time remain unchanged");
    if(!loaded.media)return;
    const auto& media=*loaded.media;
    check(!media.analysisFromCache && media.manualBpm==0 && media.manualBeatOrigin<0 && media.manualKey<0,"fresh automatic facts contain no retained analysis or manual overrides");
    juce::var row(new juce::DynamicObject());auto* object=row.getDynamicObject();
    object->setProperty("status","automatic baseline; not independently reviewed ground truth");
    object->setProperty("source",source.getFullPathName());object->setProperty("sourceSha256",digest);
    object->setProperty("durationSeconds",media.duration());object->setProperty("record",analysisRecordToJson(media.analysisRecord()));
    check(report.getSiblingFile("fresh-analysis.json").replaceWithText(juce::JSON::toString(row,true,17)),"retain numerical baseline and source identity without copying original music");
}

void runAnalysisRefreshTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    if (juce::JUCEApplicationBase::getCommandLineParameters().contains("--review-sorting-only"))
    {
        Workstation ui(false); ui.stopTimer(); ui.setSize(1920,900); ui.layout.preferredLibraryWidth=820; ui.resized();
        const juce::StringArray names {"Unknown tempo","Uncertain tempo","Uncertain bar","Detected grid","Manual bar","Manual grid"};
        for (int i=0;i<names.size();++i)
        {
            auto m=std::make_shared<Media>(); m->name=names[i]; m->analysis.bpm=i==0 ? 0 : 120;
            m->analysis.tempoConfidence=i==1 ? .1 : .9; m->analysis.downbeatOrigin=i==2 ? -1 : 0;
            m->analysis.downbeatConfidence=.9; m->manualBeatOrigin=i>=4 ? .125 : -1; m->manualBpm=i==5 ? 120 : 0;
            m->analysis.key=i==5 ? -1 : i; m->analysis.keyConfidence=i==4 ? .01 : .9;
            if (i==1) m->analysis.keyConfidence=.146;
            if (i==4) { m->analysis.key=-1; m->analysis.keyGuess=4; }
            m->manualKey=i==0 ? 3 : -1; ui.engine.library.push_back(m);
        }
        ui.refresh();
        for (double score : {0., .0028, .05, .146, .30, .5499, .55, 1.})
        {
            Media detected; detected.analysis.key=6; detected.analysis.keyConfidence=score;
            check(Workstation::keyEvidenceText(detected)=="Detected", "accepted automatic key is detected independent of stability score " + juce::String(score,4));
        }
        Media tentative; tentative.analysis.keyGuess=6; tentative.analysis.keyConfidence=1;
        check(Workstation::keyEvidenceText(tentative)=="Review", "a tentative fallback stays in review even with a high stored score");
        tentative.analysis.key=6; tentative.analysis.keyConfidence=std::numeric_limits<double>::quiet_NaN();
        check(Workstation::keyEvidenceText(tentative)=="Review", "invalid automatic evidence stays in review");
        tentative.manualKey=3;
        check(Workstation::keyEvidenceText(tentative)=="Manual", "manual choice remains manual regardless of detector evidence");
        check(ui.libraryTable.table.getHeader().getIndexOfColumnId(7, true) < 0
            && ui.libraryTable.table.getHeader().getIndexOfColumnId(9, true) < 0,
            "library removes separate grid/key review columns");
        for (int i=0;i<6;++i)
        {
            const auto& media=*ui.engine.library[(size_t)i];
            check(Workstation::libraryNeedsReview(media)==(i!=3),"row review combines independent committed grid/key facts: "+names[i]);
            check(ui.libraryTable.table.rowName(i).contains("Grid check:") && ui.libraryTable.getCellTooltip(i,1).contains("key"),
                "ordinary row exposes grid/key details through hint and accessibility: "+names[i]);
        }
        const auto geometry=juce::JSON::toString(projectToJson(ui.engine.project));
        ui.libraryTable.table.selectRow(0);
        check(ui.selectedLibraryId==ui.engine.library[0]->id && ui.libraryTable.table.isEnabled(),
            "subdued row remains ordinarily selectable");
        const auto drag=ui.libraryTable.getDragSourceDescription(ui.libraryTable.table.getSelectedRows());
        check(!drag.isVoid(),"subdued row still offers normal timeline drag payload");
        auto pixels=report.getSiblingFile("library-review-rows.png").createOutputStream();
        check(pixels && juce::PNGImageFormat().writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()),*pixels),"capture mixed ready/subdued library rows");
        check(geometry==juce::JSON::toString(projectToJson(ui.engine.project)) && ui.undoHistory.empty(),"review presentation creates no musical edits");

        // Commit through the production source-save entry, with separate playback
        // and catalogue objects and real identity-bound durable storage.
        auto source=std::make_shared<Media>(); source->name="Source save refresh"; source->sampleRate=8000;
        source->file=report.getSiblingFile("library-refresh.wav"); source->samples.setSize(1,8000); source->samples.clear();
        { juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream=source->file.createOutputStream();
          auto writer=wav.createWriterFor(stream,juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(1).withBitsPerSample(24));
          check(writer && writer->writeFromAudioSampleBuffer(source->samples,0,source->frames()),"write one-second source-refresh fixture"); }
        source->analysisSource={audioDigest(source->file),source->file.getSize(),8000,source->frames(),1};
        source->analysis.bpm=120; source->analysis.downbeatOrigin=0; source->analysis.beatOrigin=0;
        source->analysis.tempoConfidence=.9; source->analysis.downbeatConfidence=.9;
        auto catalogue=source->catalogueCopy(); ui.engine.library={catalogue};
        Clip clip; clip.media=source; clip.length=1; ui.engine.project.clips={clip};
        ui.selectedLibraryId=catalogue->id; ui.refresh();
        auto correction=SourceCorrection::from(*source); correction.bpm=128;
        check(ui.applySourceCorrection(source,correction).wasOk() && catalogue->bpm()==128 && catalogue->manualKey==-1
            && Workstation::libraryNeedsReview(*catalogue),"BPM-only save immediately refreshes catalogue, preserving unresolved key");
        ui.search.setText("128.00",false); ui.filterLibrary(); check(ui.visibleMedia.size()==1,"saved BPM immediately participates in library search");
        correction.key=13;
        check(ui.applySourceCorrection(source,correction).wasOk() && catalogue->manualKey==13 && catalogue->bpm()==128
            && !Workstation::libraryNeedsReview(*catalogue),"key-only save preserves BPM and resolves combined row review");
        ui.search.setText(keyNoteName(13),false); ui.filterLibrary(); check(ui.visibleMedia.size()==1,"saved key immediately participates in library search");
        correction.bpm=130; correction.key=3;
        check(ui.applySourceCorrection(source,correction).wasOk() && catalogue->bpm()==130 && catalogue->key()==3,"combined source correction refreshes both fields");
        const auto recordFile=analysisRecordFile(source->file); const auto backup=report.getSiblingFile("source-save-before-failure.json");
        check(recordFile.moveFileTo(backup) && recordFile.createDirectory().wasOk(),"isolate an owned record-path write refusal");
        auto failed=correction; failed.bpm=140; failed.key=8;
        check(ui.applySourceCorrection(source,failed).failed() && catalogue->bpm()==130 && catalogue->key()==3
            && source->bpm()==130 && source->key()==3,"failed durable write rolls both source and catalogue back");
        check(recordFile.deleteFile() && backup.moveFileTo(recordFile),"restore owned source record after refusal");
        ui.search.setText({},false); ui.refresh();
        const auto saved=report.getSiblingFile("library-refresh.mixworks");
        const auto saveError=ui.engine.saveProject(saved); Engine reopened;
        const auto openError=saveError.isEmpty()?reopened.restoreProject(saved.loadFileAsString()):saveError;
        check(saveError.isEmpty() && openError.isEmpty() && reopened.library.size()==1 && reopened.library[0]->bpm()==130
            && reopened.library[0]->key()==3,"source-save library values survive actual source-backed save/reopen: "+openError);
        check(audioDigest(source->file)==source->analysisSource.sha256,"source corrections never modify original PCM");
        return;
    }
    runFocusedSignalTests(check, report);
    Workstation ui(false); ui.stopTimer(); ui.setSize(1440, 940);
    for (int track = 0; track < 4; ++track)
    {
        const auto file = report.getSiblingFile("refresh-" + juce::String(track) + ".wav");
        juce::AudioBuffer<float> pcm(1, 8000 * 12);
        for (int i = 0; i < pcm.getNumSamples(); ++i)
            pcm.setSample(0, i, (float) (0.3 * std::sin(i * (0.08 + track * 0.01)) * std::exp(-(i % 4000) / 400.0)));
        {
            juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
            auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(1).withBitsPerSample(24));
            check(writer && writer->writeFromAudioSampleBuffer(pcm, 0, pcm.getNumSamples()), "write owned re-analysis source");
        }
        auto loaded = readMedia(ui.engine.formats, file, {}, AnalysisCacheMode::readWrite, MediaReadPurpose::catalogue);
        check(loaded.media != nullptr, "production analysis loads refresh source: " + loaded.error); if (!loaded.media) return;
        auto media = loaded.media; media->manualBpm = 120 + track; media->manualBeatOrigin = 0.213; media->manualKey = track;
        auto record = media->analysisRecord(); record.analysis.bpm = 70 + track;
        check(writeAnalysisRecord(file, record).isEmpty(), "persist valid cached sentinel with independent manual corrections");
        auto cached = readMedia(ui.engine.formats, file, {}, AnalysisCacheMode::readWrite, MediaReadPurpose::catalogue);
        check(cached.media && cached.media->analysisFromCache && cached.media->analysis.bpm == 70 + track, "prove the automatic sentinel is reusable before forcing re-analysis");
        if (!cached.media) return; ui.engine.library.push_back(cached.media);
    }
    ui.engine.library[1]->manualKey = -1; ui.engine.library[1]->analysis.key = -1;
    ui.engine.library[2]->analysisVersion = analysisRevision - 1;
    check(ui.analysisTargets(1).size() == 2 && ui.analysisTargets(2).size() == 4, "incomplete scope includes unknown/outdated facts and respects complete manual corrections");
    {
        const auto previous = ui.engine.library[1]->analysisRecord();
        auto retried = readMedia(ui.engine.formats, ui.engine.library[1]->file, {}, AnalysisCacheMode::focused, MediaReadPurpose::catalogue, &previous);
        check(retried.media && retried.media->analysis.focusedKey == 1 && retried.media->analysis.focusedTempo == 0
            && retried.media->manualBpm == previous.manualBpm && retried.media->manualBeatOrigin == previous.manualBeatOrigin
            && retried.media->manualKey == -1 && retried.media->analysis.bpm == previous.analysis.bpm,
            "focused source load protects manual BPM/downbeat and persists an honest unchanged unknown key");
        if (retried.media)
        {
            const auto record = retried.media->analysisRecord(); const auto parsed = readAnalysisRecord(retried.media->file, record.source);
            check(parsed.reusableAutomatic && parsed.record->analysis.focusedKey == 1, "completed focused retry survives source-bound cache parsing");
            int work = 0;
            auto again = readMedia(ui.engine.formats, retried.media->file, [&](MediaReadStage stage) { work += stage == MediaReadStage::analyse || stage == MediaReadStage::decode; }, AnalysisCacheMode::focused, MediaReadPurpose::catalogue, &record);
            check(again.media && again.media->analysisFromCache && work == 0 && !Workstation::canRetryAnalysis(*again.media), "identical focused retry reuses saved results without decoding or rerunning analysis");
            auto invalid = analysisRecordToJson(record); invalid["analysis"]["focusedRetry"].getDynamicObject()->setProperty("key", "invalid");
            const auto damaged = parseAnalysisRecord(invalid, record.source);
            check(damaged.reusableAutomatic && damaged.record->analysis.focusedRevision == 0 && damaged.record->manualBeatOrigin == record.manualBeatOrigin,
                  "malformed optional retry evidence cannot invalidate the existing analysis or manual downbeat");
            auto identity = record.source; identity.sha256 = juce::String::repeatedString("a", 64);
            check(!parseAnalysisRecord(analysisRecordToJson(record), identity).record, "focused attempts cannot leak to changed source identities");
        }
        auto protectedSource = ui.engine.library[0]->catalogueCopy(); protectedSource->manualBpm = 0;
        protectedSource->analysis.bpm = 0; protectedSource->analysis.keyConfidence = 1;
        check(!Workstation::canRetryAnalysis(*protectedSource), "manual downbeat protects its pulse even when the automatic BPM looks weak");
    }
    auto playback = readMedia(ui.engine.formats, ui.engine.library[0]->file);
    check(playback.media != nullptr, "load actual arranged playback"); if (!playback.media) return;
    Clip clip; clip.media = playback.media; clip.length = playback.media->duration(); clip.pendingPlacement = PendingPlacement::append;
    Project input; input.clips = {clip}; const auto prepared = prepareTempoMap(input, false, 0);
    check(prepared.error.isEmpty(), "prepare real playback before re-analysis: " + prepared.error); if (prepared.error.isNotEmpty()) return;
    ui.engine.project = prepared.project; ui.refresh(); ui.menuItemSelected(63, 3);
    check(ui.libraryTable.table.getNumSelectedRows() == 2 && ui.analysisTargets(0).size() == 2, "Select tracks needing analysis makes a native multiple selection");
    juce::SparseSet<int> chosen; chosen.addRange({0, 1}); chosen.addRange({2, 3});
    ui.libraryTable.table.setSelectedRows(chosen, juce::sendNotification);
    const auto selected = ui.analysisTargets(0); ui.libraryTable.sortOrderChanged(1, false); ui.refresh();
    const auto sorted = ui.analysisTargets(0);
    check(selected.size() == 2 && sorted.size() == 2 && std::all_of(sorted.begin(), sorted.end(), [&](const auto& media)
        { return std::any_of(selected.begin(), selected.end(), [&](const auto& old) { return old->id == media->id; }); }), "multiple selection survives sorting and refresh by identity");
    const auto rightClick = [&](int row)
    {
        auto& table = ui.libraryTable.table; const auto point = juce::Point<float> {40, 12};
        const auto event = juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), point,
            juce::ModifierKeys(juce::ModifierKeys::rightButtonModifier), 1, 0, 0, 0, 0,
            &table, &table, juce::Time::getCurrentTime(), point, juce::Time::getCurrentTime(), 1, false);
        ui.libraryTable.cellClicked(row, 1, event);
    };
    const auto rows = ui.libraryTable.table.getSelectedRows(); rightClick(rows[0]);
    juce::Component* menuRow = nullptr;
    std::function<void(juce::Component&)> findMenu = [&](juce::Component& component)
    {
        if (const auto* handler = component.getAccessibilityHandler())
            if (handler->getRole() == juce::AccessibilityRole::menuItem && handler->getTitle() == "Re-analyse selected tracks") menuRow = &component;
        for (auto* child : component.getChildren()) findMenu(*child);
    };
    auto& desktop = juce::Desktop::getInstance();
    for (int i = 0; i < desktop.getNumComponents(); ++i) findMenu(*desktop.getComponent(i));
    bool menuCaptured = false;
    if (menuRow)
    {
        auto* popup = menuRow->getTopLevelComponent(); auto image = report.getSiblingFile("library-batch-menu.png").createOutputStream(); juce::PNGImageFormat format;
        menuCaptured = image && format.writeImageToStream(popup->createComponentSnapshot(popup->getLocalBounds()), *image);
    }
    check(menuCaptured, "capture actual native right-click batch menu");
    check(ui.libraryTable.table.getSelectedRows() == rows && juce::PopupMenu::dismissAllActiveMenus(), "RMB on a selected row preserves the batch and opens the real popup");
    auto menu = ui.libraryTable.beatmatchMenu(rows[0]); bool batch = false, single = false;
    for (juce::PopupMenu::MenuItemIterator i(menu, true); i.next();) { batch |= i.getItem().itemID == 60; single |= i.getItem().itemID == 8600; }
    check(batch && !single, "batch menu exposes selected re-analysis without pretending single-track splitting is batched");
    int other = 0; while (rows.contains(other)) ++other; rightClick(other);
    check(ui.libraryTable.table.getNumSelectedRows() == 1 && ui.libraryTable.table.isRowSelected(other) && juce::PopupMenu::dismissAllActiveMenus(), "RMB outside the selection targets only the clicked source");
    ui.libraryTable.table.selectRowsBasedOnModifierKeys(0, juce::ModifierKeys(), false);
    ui.libraryTable.table.selectRowsBasedOnModifierKeys(2, juce::ModifierKeys(juce::ModifierKeys::ctrlModifier), false);
    check(ui.libraryTable.table.getNumSelectedRows() == 2, "native Ctrl-click adds a non-adjacent source");
    ui.libraryTable.table.selectRowsBasedOnModifierKeys(3, juce::ModifierKeys(juce::ModifierKeys::shiftModifier), false);
    check(ui.libraryTable.table.isRowSelected(2) && ui.libraryTable.table.isRowSelected(3), "native Shift-click selects a contiguous range");
    ui.menuItemSelected(65, 3); check(ui.libraryTable.table.getNumSelectedRows() == 4, "Select all visible tracks selects the native table");
    ui.libraryTable.table.deselectAllRows();
    check(ui.libraryTable.table.keyPressed(juce::KeyPress('a', juce::ModifierKeys::commandModifier, 0))
        && ui.libraryTable.table.getNumSelectedRows() == 4, "native Ctrl+A selects every visible library row");
    ui.search.setText("refresh-1", juce::dontSendNotification); ui.filterLibrary(); ui.menuItemSelected(65, 3);
    check(ui.analysisTargets(0).size() == 1 && ui.analysisTargets(0)[0]->file.getFileName() == "refresh-1.wav", "filtered Select all cannot include hidden library sources");
    ui.search.setText({}, juce::dontSendNotification); ui.filterLibrary();
    auto repeated = ui.engine.project.clips[0]; repeated.id = juce::Uuid().toString(); repeated.start += repeated.length;
    ui.engine.project.clips.push_back(repeated); ui.showBrowserPage(1); ui.refresh();
    juce::SparseSet<int> both; both.addRange({0, 2}); ui.playlistTable.table.setSelectedRows(both, juce::sendNotification);
    check(ui.analysisTargets(0).size() == 1 && ui.playlistTable.table.getNumSelectedRows() == 2, "multiple playlist clips sharing a source are analysed only once");
    ui.engine.project = prepared.project; ui.showBrowserPage(0); ui.refresh(); chosen.clear();
    for (int row = 0; row < (int) ui.visibleMedia.size(); ++row) if (ui.visibleMedia[(size_t) row] == 0 || ui.visibleMedia[(size_t) row] == 2) chosen.addRange({row, row + 1});
    ui.libraryTable.table.setSelectedRows(chosen, juce::sendNotification);
    const auto geometry = juce::JSON::toString(projectToJson(ui.engine.project));
    const auto digest = audioDigest(playback.media->file); const auto pcm = ui.engine.project.clips[0].prepared; const auto untouched = ui.engine.library[3];
    ui.menuItemSelected(60, 3);
    check(ui.busy && ui.importCancellation && ui.importProgress.isVisible(), "native Selected tracks starts background re-analysis and progress");
    auto image = report.getSiblingFile("reanalysis-progress.png").createOutputStream(); juce::PNGImageFormat png;
    check(image && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *image), "capture native multi-selection and cancellable re-analysis progress");
    int phase = 0; const auto began = juce::Time::getMillisecondCounterHiRes(); std::vector<std::shared_ptr<Media>> beforeCancel;
    struct Poll final : juce::Timer { std::function<void()> tick; void timerCallback() override { tick(); } } poll;
    poll.tick = [&]
    {
        if (juce::Time::getMillisecondCounterHiRes() - began > 180000)
        {
            if (ui.importCancellation) *ui.importCancellation = true;
            check(false, "re-analysis deadline"); poll.stopTimer(); juce::MessageManager::getInstance()->stopDispatchLoop(); return;
        }
        if (phase == 0 && !ui.busy)
        {
            check(ui.engine.library[0]->analysis.bpm != 70 && ui.engine.library[2]->analysis.bpm != 72 && !ui.engine.library[0]->analysisFromCache
                  && !ui.engine.library[2]->analysisFromCache && ui.engine.library[3] == untouched, "selected sources receive fresh automatic results; other sources stay untouched");
            check(ui.engine.library[0]->manualBpm == 120 && ui.engine.library[0]->manualKey == 0 && ui.engine.library[0]->manualBeatOrigin == 0.213
                  && ui.engine.project.clips[0].media->analysis.bpm != 70 && ui.engine.project.clips[0].media->manualBpm == 120, "fresh facts reach arranged audio while all manual corrections survive");
            const auto saved = readAnalysisRecord(ui.engine.library[0]->file, ui.engine.library[0]->analysisSource);
            check(saved.record && saved.reusableAutomatic && saved.record->manualBpm == 120 && saved.record->manualKey == 0, "forced analysis persists manual corrections with its fresh detector facts");
            check(juce::JSON::toString(projectToJson(ui.engine.project)) == geometry && ui.engine.project.clips[0].prepared == pcm
                  && audioDigest(playback.media->file) == digest, "re-analysis preserves source bytes, placement, source clock and prepared playback");
            beforeCancel = ui.engine.library; ui.menuItemSelected(62, 3); phase = 1;
        }
        else if (phase == 1 && ui.engine.library[0] != beforeCancel[0]) { ui.importProgress.cancel.onClick(); phase = 2; }
        else if (phase == 2 && !ui.busy)
        {
            int completed = 0; for (size_t i = 0; i < beforeCancel.size(); ++i) completed += ui.engine.library[i] != beforeCancel[i];
            check(completed > 0 && completed < (int) beforeCancel.size() && !ui.importProgress.isVisible(), "Stop analysis retains completed results and skips remaining tracks");
            check(juce::JSON::toString(projectToJson(ui.engine.project)) == geometry, "cancelling analysis also retains the arrangement");
            auto& source = ui.engine.library[1]; source->manualKey = -1; source->analysis.key = -1;
            source->analysis.focusedRevision = source->analysis.focusedKey = source->analysis.focusedTempo = 0;
            check(writeAnalysisRecord(source->file, source->analysisRecord()).isEmpty(), "seed an untried weak automatic key for the native batch retry");
            ui.showBrowserPage(0); ui.refresh(); ui.menuItemSelected(65, 3); beforeCancel = ui.engine.library;
            ui.menuItemSelected(64, 3);
            check(ui.busy && ui.importProgress.isVisible(), "native focused command starts cancellable batch work");
            auto capture = report.getSiblingFile("focused-retry-progress.png").createOutputStream(); juce::PNGImageFormat imageFormat;
            check(capture && imageFormat.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *capture), "capture actual focused retry and selected library rows");
            phase = 3;
        }
        else if (phase == 3 && !ui.busy)
        {
            check(ui.engine.library[1]->analysis.focusedKey == 1 && ui.engine.library[1]->manualBpm == 121
                && ui.engine.library[1]->manualBeatOrigin == .213 && ui.engine.library[0] == beforeCancel[0]
                && ui.engine.library[2] == beforeCancel[2] && ui.engine.library[3] == beforeCancel[3],
                "native focused batch attempts only weak unprotected fields and skips manual sources");
            check(ui.status.contains("need review in Source corrections")
                && juce::JSON::toString(projectToJson(ui.engine.project)) == geometry && ui.engine.project.clips[0].prepared == pcm,
                "focused completion reports unchanged honestly and preserves arranged PCM and clocks");
            const auto completed = ui.engine.library;
            ui.menuItemSelected(64, 3);
            check(!ui.busy && ui.engine.library == completed && ui.status.contains("No tracks need another retry"), "repeating completed batch retry is a no-op with clear feedback");
            poll.stopTimer(); juce::MessageManager::getInstance()->stopDispatchLoop();
        }
    };
    poll.startTimer(20); juce::MessageManager::getInstance()->runDispatchLoop();
}
}
