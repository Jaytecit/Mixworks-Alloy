#include "Workstation.h"
#include "EarTools.h"
#include "GridCorrectionUI.h"

namespace mixworks
{
void Workstation::showKeyByEar(const std::shared_ptr<Media>& media, std::shared_ptr<const Media> playback, GridCorrectionEditor* parent)
{
    if (busy || engine.exporting || !playback || !parent || !(playback->analysisSource == media->analysisSource)) return;
    if (earWindow && earWindow->isVisible()) { earWindow->toFront(true); return; }
    libraryPreview.stop();
    auto* editor = new KeyEarEditor(engine, libraryPreview, std::move(playback), 0,
        [safe = juce::Component::SafePointer<Workstation>(this), media, grid = juce::Component::SafePointer<GridCorrectionEditor>(parent)](int key)
        {
            if (!safe || !grid) return juce::Result::fail("The source review was closed.");
            auto correction = SourceCorrection::from(*media); correction.key = key;
            const auto saved = safe->applySourceCorrection(media, correction, false);
            if (saved.wasOk()) grid->setReviewedKey(key);
            return saved;
        });
    earWindow = std::make_unique<EarToolWindow>(editor, "Key by ear"); earWindow->setLookAndFeel(&look);
    earWindow->setVisible(true); earWindow->enterModalState(true, nullptr, false);
}
void Workstation::showHarmoniseByEar()
{
    if (busy || engine.exporting || pendingTempo > 0 || tempoPreparationPending || !timelineLoads.empty())
    { setStatus("Wait for source/tempo preparation before comparing harmony."); return; }
    juce::String targetId;
    {
        const juce::ScopedLock guard(engine.lock);
        const auto* target = engine.project.clipFor(selectedClip);
        if (!target) target = engine.project.clipFor(selectedTransition);
        if (!target) { setStatus("Select the segment whose pitch you want to adjust."); return; }
        if (engine.project.clips.size() < 2) { setStatus("Add a second overlapping segment to compare harmony."); return; }
        targetId = target->id;
    }
    if (earWindow && earWindow->isVisible()) { earWindow->toFront(true); return; }
    libraryPreview.stop();
    auto* editor = new HarmonyEarEditor(engine, libraryPreview, audioWorkers, targetId,
        [safe = juce::Component::SafePointer<Workstation>(this)](const juce::String& baseline, const Clip& prepared)
        {
            if (!safe || safe->busy || !safe->timelineLoads.empty() || safe->pendingTempo > 0 || safe->tempoPreparationPending)
                return juce::Result::fail("The arrangement is busy. Wait before applying pitch.");
            const auto result = commitHarmony(safe->engine, baseline, prepared, [safe] { safe->remember(); });
            if (result.wasOk()) { safe->refresh(); safe->setStatus("Pitch applied to one segment. Timing and source key kept; Undo restores the pitch."); }
            return result;
        });
    earWindow = std::make_unique<EarToolWindow>(editor, "Harmonise by ear"); earWindow->setLookAndFeel(&look);
    earWindow->setVisible(true); earWindow->enterModalState(true, nullptr, false);
}
}
