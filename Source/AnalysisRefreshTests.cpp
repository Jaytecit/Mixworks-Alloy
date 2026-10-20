#include "Workstation.h"
#include "Beatmatch.h"
#include <cmath>

namespace mixworks
{
void runAnalysisRefreshTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
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
            poll.stopTimer(); juce::MessageManager::getInstance()->stopDispatchLoop();
        }
    };
    poll.startTimer(20); juce::MessageManager::getInstance()->runDispatchLoop();
}
}
