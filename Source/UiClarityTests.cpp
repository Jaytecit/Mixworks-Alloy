#include "Workstation.h"
#include "GridCorrectionUI.h"
#include "UiMessages.h"
#include "RetryReview.h"
#include <limits>
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace mixworks
{
void runUiClarityTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    const auto pump = []
    {
       #if JUCE_WINDOWS
        MSG m; while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageW(&m); }
       #endif
    };
    const auto capture = [&](juce::Component& c, const juce::String& name)
    {
        auto stream = report.getSiblingFile(name + ".png").createOutputStream(); juce::PNGImageFormat png;
        check(stream && png.writeImageToStream(c.createComponentSnapshot(c.getLocalBounds()), *stream), "capture actual UI / " + name);
    };
    const auto childNamed = [](juce::Component& parent, const juce::String& name) -> juce::Component*
    { for (auto* c : parent.getChildren()) if (c->getName() == name) return c; return nullptr; };
    check(retryReviewFile().isAChildOf(report.getParentDirectory()), "retry review persistence is isolated inside the owned test root");
    if (!retryReviewFile().isAChildOf(report.getParentDirectory())) return;

    juce::TextButton hint("Apply"); hint.setTooltip("Move by 0.01 BPM. Keep listening. Internal diagnostics should not be appended.");
    check(AlloyHints::textFor(hint) == "Move by 0.01 BPM. Keep listening.", "hover help keeps two sentences without splitting decimal values");
    hint.setEnabled(false); hint.getProperties().set("mixworksHintDisabled", "Stop playback first.");
    check(AlloyHints::textFor(hint) == "Stop playback first.", "disabled help shows its specific recovery action without generic filler");
    hint.setEnabled(true); hint.setTooltip(juce::String::repeatedString("word ", 100));
    check(AlloyHints::textFor(hint).length() <= 240, "dynamic hover help has a bounded footprint");
    hint.getProperties().set("mixworksExternalEditor", true);
    check(AlloyHints::textFor(hint).isEmpty(), "third-party editor help stays outside the app-owned hint policy");

    auto ui = std::make_unique<Workstation>(false); ui->stopTimer(); ui->setSize(1250, 800);
    Media unknown; unknown.analysis.keyConfidence = 1;
    check(Workstation::keyEvidenceText(unknown) == "Unknown" && Workstation::keyEvidenceHint(unknown).contains("Key not found"), "missing keys give a clear correction action");
    unknown.analysis.key = 4; unknown.analysis.keyConfidence = std::numeric_limits<double>::infinity();
    check(Workstation::keyEvidenceText(unknown) == "Review", "invalid evidence never becomes a found-key label");
    unknown.manualKey = 6;
    check(Workstation::keyEvidenceText(unknown) == "Manual" && Workstation::keyEvidenceHint(unknown).contains("Your chosen key"), "manual key is plainly labelled");
    const int modals = juce::Component::getNumCurrentlyModalComponents();
    const auto diagnostic = "Could not save this item. Choose another folder. " + juce::String::repeatedString("Diagnostic detail. ", 20);
    ui->fail(diagnostic);
    check(juce::Component::getNumCurrentlyModalComponents() == modals && ui->status == "Could not save this item. Choose another folder."
        && ui->getProperties()["lastMessageDetails"].toString() == diagnostic, "routine failure is nonblocking, concise and retains its full details");
    auto tools = ui->getMenuForIndex(5, {}); int detailsId = 0;
    for (juce::PopupMenu::MenuItemIterator it(tools, true); it.next();) if (it.getItem().text == "Last message details...") detailsId = it.getItem().itemID;
    check(detailsId != 0, "full message details are available on request through Tools");
    if (detailsId)
    {
        ui->menuItemSelected(detailsId, 5);
        auto* details = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        auto* text = details ? dynamic_cast<juce::TextEditor*>(details->getCustomComponent(0)) : nullptr;
        check(text && text->isReadOnly() && text->isMultiLine() && text->getText() == diagnostic, "requested details use a scrolling copyable view");
        if (details) { capture(*details, "message-details"); details->exitModalState(0); pump(); }
    }

    const auto fixture = [&](const juce::String& name)
    {
        auto m = std::make_shared<Media>(); m->name = name; m->sampleRate = 8000; m->samples.setSize(1, 16000); m->samples.clear();
        m->file = report.getSiblingFile(name + ".wav");
        { juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> out = m->file.createOutputStream();
          auto writer = wav.createWriterFor(out, juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(1).withBitsPerSample(24));
          check(writer && writer->writeFromAudioSampleBuffer(m->samples, 0, m->frames()), "write tiny owned silent source / " + name); }
        m->analysisSource = {audioDigest(m->file), m->file.getSize(), 8000, 16000, 1};
        m->analysis.bpm = 120; m->analysis.beatOrigin = .1; m->analysis.downbeatOrigin = .1; m->analysis.tempoConfidence = .9;
        m->manualBpm = 120; m->manualBeatOrigin = .1; m->prepareWaveformBody();
        check(writeAnalysisRecord(m->file, m->analysisRecord()).isEmpty(), "save owned baseline without running a rhythm detector / " + name);
        return m;
    };
    auto a = fixture("Silent source"), b = fixture("Other source");
    const auto originalHash = audioDigest(a->file);
    auto missing = a->catalogueCopy(); missing->id = juce::Uuid().toString(); missing->name = "Missing source"; missing->file = report.getSiblingFile("missing.wav");
    auto zero = missing->catalogueCopy(); zero->id = juce::Uuid().toString(); zero->name = "No duration"; zero->file = report.getSiblingFile("no-duration.wav"); zero->analysisSource.frames = 0;
    auto attemptOwner = a->catalogueCopy(); auto& attempted = *attemptOwner; attempted.analysis.focusedRevision = focusedAnalysisRevision; attempted.analysis.focusedKey = 1;
    check(!Workstation::canRetryAnalysis(attempted) && retryIssueText(attempted) == "Key needs review", "an unchanged exhausted retry remains a failure for manual review");
    attempted.analysis.key = 0; attempted.analysis.keyConfidence = .9; attempted.manualBpm = 0; attempted.manualBeatOrigin = -1; attempted.analysis.tempoConfidence = 0;
    check(retryIssueText(attempted) == "BPM/grid needs review", "successful key retry does not hide unresolved tempo");
    attempted.manualBpm = 120; attempted.manualBeatOrigin = .1; attempted.manualKey = 0;
    check(retryIssueText(attempted).isEmpty(), "manual timing and key resolve remaining review fields");
    auto earlierPass = a->catalogueCopy(); earlierPass->manualBpm = 0; earlierPass->manualBeatOrigin = -1;
    earlierPass->analysis.focusedRevision = focusedAnalysisRevision; earlierPass->analysis.focusedTempo = 2;
    auto keyPass = earlierPass->catalogueCopy(); keyPass->analysis.key = 0; keyPass->analysis.keyConfidence = .9; keyPass->analysis.focusedKey = 2;
    check(retryReviewRow(*earlierPass, keyPass.get(), {})["result"].toString() == "BPM kept; key improved",
        "a key-only retry does not claim a BPM improvement from an earlier pass");
    auto readFailure = retryReviewRow(*a, nullptr, "Reader failed");
    auto changeOwner = a->catalogueCopy(); auto& changed = *changeOwner; changed.analysisSource.sha256 = juce::String::repeatedString("a", 64);
    check(retryIssueText(*a, readFailure) == "File could not be read" && retryIssueText(changed, readFailure).isEmpty(), "read failures are bound to the original source identity");
    auto saveOwner = a->catalogueCopy(); auto& saveFailed = *saveOwner; saveFailed.manualKey = 0; saveFailed.analysisCacheWarning = "Disk full";
    auto saveRow = retryReviewRow(*a, &saveFailed, {}); saveRow.getDynamicObject()->setProperty("volatile", true);
    publishRetryResult(*ui, saveRow);
    const auto savedReview = saveRetryReview(saveRow); const auto saveIssues = retryIssues({a}, ui->getProperties()["focusedRetryRows"]);
    check(savedReview.wasOk() && saveIssues.size() == 1 && saveIssues[0].reason == "Results could not be saved", "save failures survive review persistence and live publication");
    clearRetryError(saveFailed, false, ui.get());
    check(retryIssueText(saveFailed).isEmpty() && retryIssues({saveOwner}, ui->getProperties()["focusedRetryRows"]).empty(), "successful manual save clears stored and volatile save errors");

    ui->engine.library = {a, b, missing, zero}; ui->catalogueFile = report.getSiblingFile("catalogue.json");
    Clip clip; clip.media = a; clip.start = 2; clip.length = 2; ui->engine.project.clips = {clip}; ui->refresh(); ui->remember();
    ui->hints.displayTip({600, 300}, ui->libraryTable.getCellTooltip(0, 7));
    capture(ui->hints, "short-key-help"); ui->hints.hideTip();
    const auto geometry = juce::JSON::toString(projectToJson(ui->engine.project));
    const auto pointer = ui->engine.project.clips[0].media;
    ui->reanalyseTracks(0, true, {a, b, missing, zero});
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + 30000;
    while (ui->busy && juce::Time::getMillisecondCounterHiRes() < deadline) { pump(); juce::Thread::sleep(5); }
    pump();
    check(!ui->busy && ui->status.contains("4 tracks need review"), "actual focused batch reports all remaining failures without a modal");
    if (ui->busy) return;
    check(ui->engine.library[0]->analysis.focusedKey == 1 && !Workstation::canRetryAnalysis(*ui->engine.library[0]), "tiny key-only retry finishes honestly without rerunning rhythm analysis");
    check(ui->getProperties()["focusedLastResult"]["name"].toString() == "No duration"
        && ui->importProgress.getProperties()["focusedLastResult"].toString().contains("File could not be read"), "last analysed track publishes its own result to progress and persistent feedback");
    check(lastRetryResult().contains("No duration") && retryIssues(ui->engine.library).size() == 4, "last result and failures reload independently of transient progress");
    check(juce::JSON::toString(projectToJson(ui->engine.project)) == geometry && ui->engine.project.clips[0].media == pointer
        && a->manualBpm == 120 && a->manualBeatOrigin == .1 && a->manualKey == -1 && audioDigest(a->file) == originalHash, "retry feedback preserves arrangement geometry, audio ownership, manual facts and original audio");
    capture(ui->importProgress, "retry-last-result");
    ui->importProgress.setVisible(false);

    ui->editAnalysis(a->id);
    auto* grid = dynamic_cast<GridCorrectionEditor*>(ui->gridWindow ? ui->gridWindow->getContentComponent() : nullptr);
    check(grid != nullptr, "source corrections open with retry review available"); if (!grid) return;
    auto* picker = dynamic_cast<juce::ComboBox*>(childNamed(*grid, "Retry failures"));
    check(picker && picker->getNumItems() == 4, "corrections dropdown exposes every unresolved result and read failure");
    capture(*grid, "retry-failures-grid");
    if (picker)
    {
        pump(); picker->showPopup(); juce::Component* menu = nullptr;
        std::function<void(juce::Component&)> find = [&](juce::Component& c)
        {
            if (auto* h = c.getAccessibilityHandler(); h && h->getRole() == juce::AccessibilityRole::menuItem && h->getTitle().contains("Other source")) menu = c.getTopLevelComponent();
            for (auto* child : c.getChildren()) find(*child);
        };
        auto& desktop = juce::Desktop::getInstance(); for (int i = 0; i < desktop.getNumComponents(); ++i) find(*desktop.getComponent(i));
        check(menu != nullptr, "native failure menu exposes named tracks and their reasons");
        if (menu) capture(*menu, "retry-failures-dropdown");
        juce::PopupMenu::dismissAllActiveMenus(); pump();
    }
    auto* bpm = dynamic_cast<juce::TextEditor*>(childNamed(*grid, "BPM override"));
    if (bpm && picker)
    {
        bpm->setText("121"); picker->setSelectedItemIndex(1, juce::dontSendNotification); picker->onChange();
        auto* decision = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        check(decision && decision->getName() == "Unsaved corrections", "switching a dirty source asks a short save/discard/stay question");
        if (decision) { capture(*decision, "unsaved-corrections"); decision->exitModalState(0); pump(); }
        check(ui->gridWindow->getContentComponent() == grid && bpm->getText() == "121", "Stay here retains the draft and source");
        bpm->setText("120"); picker->setSelectedItemIndex(1, juce::dontSendNotification); picker->onChange(); pump();
        auto* switched = dynamic_cast<GridCorrectionEditor*>(ui->gridWindow->getContentComponent());
        bool named = false; for (auto* c : switched ? switched->getChildren() : juce::Array<juce::Component*>())
            if (auto* label = dynamic_cast<juce::Label*>(c)) named |= label->getText().contains("SOURCE BEAT GRID  /  Other source");
        check(switched && named, "failure selection opens the requested source by identity");
    }
    ui->gridWindow->closeButtonPressed(); ui->editAnalysis(zero->id);
    auto* home = dynamic_cast<RetryFailurePanel*>(ui->gridWindow->getContentComponent());
    check(home != nullptr, "failures remain accessible when the selected file has no usable duration");
    if (home)
    {
        capture(*home, "retry-failures-home");
        home->onOpen(b->id); ui->gridWindow->closeButtonPressed(); ui->editAnalysis(a->id);
        auto target = juce::Component::SafePointer<juce::Component>(ui->gridWindow->getContentComponent()); pump();
        check(target && ui->gridWindow->getContentComponent() == target.getComponent(), "late failure selection cannot retarget a newer review window");
    }
    ui->gridWindow->closeButtonPressed(); pump();
    check(!ui->gridWindow->isCurrentlyModal(), "closing failure review releases modal input");
    const auto resolved = SourceCorrection {120, .1, 0};
    check(ui->applySourceCorrection(a, resolved, false).wasOk() && retryIssues(ui->engine.library).size() == 3,
        "saving manual corrections removes that source from the failure list without retiming the mix");
    check(juce::Component::getNumCurrentlyModalComponents() == modals, "batch completion and review leave no hidden blocking popup");
    const auto reviewText = retryReviewFile().loadFileAsString();
    check(retryReviewFile().replaceWithText("damaged list") && saveRetryReview(readFailure).failed()
        && retryReviewFile().loadFileAsString() == "damaged list", "a damaged review file is preserved instead of overwritten");
    retryReviewFile().replaceWithText(reviewText);
}
}
