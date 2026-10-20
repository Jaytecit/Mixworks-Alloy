#include "Workstation.h"
#include <charconv>
#include <cmath>

namespace mixworks
{
juce::Result Workstation::applyEffectTail(double seconds)
{
    if (busy) return juce::Result::fail("Wait for the current operation before changing the tail window.");
    const juce::ScopedLock guard(engine.lock);
    auto candidate = engine.project; const auto valid = candidate.setEffectTailSeconds(seconds);
    if (valid.failed() || engine.project.effectTailSeconds == seconds) return valid;
    engine.setPlaying(false); engine.finishAutomation(); remember(); engine.project.effectTailSeconds = seconds;
    engine.resetPlugins(); timeline.updateSize(); refresh();
    setStatus(seconds > 0 ? "Effect ending: " + juce::String(seconds, 3) + " seconds. Export can include it or stop at the last clip." : "Effect tails stop at the last clip.");
    return juce::Result::ok();
}
void Workstation::configureEffectTail(std::function<void()> after)
{
    if (busy) return;
    auto* dialog = new juce::AlertWindow("Effect ending", "Let effects ring out after the music stops. Use 0 to stop with the last clip.", juce::MessageBoxIconType::NoIcon, this);
    dialog->addTextEditor("seconds", juce::String(engine.project.effectTailSeconds, 6), "Seconds");
    dialog->addButton("Apply", 1, juce::KeyPress(juce::KeyPress::returnKey)); dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    dialog->enterModalState(true, juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<Workstation>(this), dialog, after](int answer)
    {
        if (!safe) return;
        if (answer == 1)
        {
            const auto text = dialog->getTextEditorContents("seconds").trim(); double seconds = 0;
            const char* begin = text.toRawUTF8(); const auto parsed = std::from_chars(begin, begin + text.getNumBytesAsUTF8(), seconds);
            const auto valid = parsed.ec == std::errc() && parsed.ptr == begin + text.getNumBytesAsUTF8() && std::isfinite(seconds)
                ? safe->applyEffectTail(seconds) : juce::Result::fail("Enter a finite number of seconds, such as 0, 0.5 or 4.");
            if (valid.failed()) { safe->fail(valid.getErrorMessage()); return; }
        }
        if (after) juce::MessageManager::callAsync(after);
    }), true);
}
void Workstation::chooseExportEnd(const juce::File& file)
{
    if (busy) return;
    const double tail = engine.project.effectTailSeconds;
    auto* dialog = new juce::AlertWindow("Export mix", "Choose where the audio ends.\nEffects can ring for " + juce::String(tail, 3) + " seconds.", juce::MessageBoxIconType::NoIcon, this);
    dialog->addButton("Last clip", 1); if (tail > 0) dialog->addButton("Include tail", 2);
    dialog->addButton("Set tail...", 3); dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    dialog->enterModalState(true, juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<Workstation>(this), file](int answer)
    {
        if (!safe || safe->busy) return;
        if (answer == 1 || answer == 2) safe->beginWavExport(file, {answer == 2});
        else if (answer == 3) safe->configureEffectTail([safe, file] { if (safe) safe->chooseExportEnd(file); });
    }), true);
}
void Workstation::beginWavExport(const juce::File& file, WavExportOptions options)
{
    if (busy || exportTask) return;
    auto task = std::make_shared<ExportTask>(); task->destination = file; task->options = options;
    const auto valid = engine.planWavExport(file, options, task->plan); if (valid.failed()) { fail(valid.getErrorMessage()); return; }
    engine.setPlaying(false); engine.finishAutomation(); setBusy(true);
    if (engine.exporting.exchange(true)) { setBusy(false); fail("An export is already running."); return; }
    engine.silenceNotes();
    juce::String error; task->renderer = engine.createExportRenderer(error, task->cancelled);
    if (!task->renderer) { engine.exporting = false; setBusy(false); fail(error); return; }
    exportTask = task; importProgress.setExporting(file, task->plan); resized();
    setStatus("Rendering " + juce::String(task->plan.seconds(), 2) + " seconds to WAV...");
    audioWorkers.addJob([task, safe = juce::Component::SafePointer<Workstation>(this)]() mutable
    {
        const auto result = task->renderer->writeWav(task->destination, task->options, task->cancelled,
            [task](auto completed, const auto&) { task->completed = completed; });
        const std::weak_ptr<ExportTask> weak = task;
        // The UI owns the renderer. Drop this worker's reference before a completion
        // can release the UI owner, so native processors are destroyed on main.
        task.reset();
        juce::MessageManager::callAsync([safe, weak, result] { if (safe) safe->finishWavExport(weak, result); });
    });
}
void Workstation::finishWavExport(const std::weak_ptr<ExportTask>& weak, const WavExportResult& result)
{
    const auto task = weak.lock(); if (!task || task != exportTask) return;
    engine.silenceNotes(); engine.exporting = false; exportTask.reset(); importProgress.setVisible(false); setBusy(false); refresh();
    if (result.wasOk())
    {
        if (result.peak > 1) engine.masterClipped = true;
        setStatus("Exported " + juce::String(result.plan.seconds(), 2) + " seconds: " + task->destination.getFullPathName());
    }
    else if (result.outcome == WavExportOutcome::cancelled) setStatus(result.error);
    else fail(result.error);
}
}
