#include "EarTools.h"
#include "Beatmatch.h"
#include <set>

namespace mixworks
{
namespace
{
Project earSnapshot(Engine& engine)
{
    const juce::ScopedLock guard(engine.lock); return engine.project;
}
juce::String earFingerprint(Engine& engine)
{
    const juce::ScopedLock guard(engine.lock); return juce::JSON::toString(projectToJson(engine.project), true, 17);
}
}
HarmonyEarEditor::HarmonyEarEditor(Engine& e, LibraryPreview& route, juce::ThreadPool& pool, const juce::String& clipId,
    std::function<juce::Result(const juce::String&, const Clip&)> commit)
    : engine(e), workers(pool), preview(e, route, player), baseline(earSnapshot(e)),
      fingerprint(juce::JSON::toString(projectToJson(baseline), true, 17)), apply(std::move(commit))
{
    setSize(920, 640);
    for (auto* c : std::initializer_list<juce::Component*> {&preview, &heading, &instructions, &result, &status, &referenceLabel, &adjustedLabel, &loopLabel,
        &reference, &adjusted, &hear, &begin, &end, &pitch, &cents, &fine, &lower, &higher, &originalButton, &keptButton, &trialButton, &keep, &prepare, &applyButton, &cancel}) addAndMakeVisible(c);
    heading.setText("HARMONISE BY EAR", juce::dontSendNotification); heading.setFont(juce::FontOptions(18));
    instructions.setText("Keep the reference fixed. Compare pitch choices over the same overlap; tempo and beat alignment stay fixed.", juce::dontSendNotification);
    instructions.setFont(juce::FontOptions(13)); result.setFont(juce::FontOptions(14));
    referenceLabel.setText("Reference / stays unchanged", juce::dontSendNotification);
    adjustedLabel.setText("Segment to adjust", juce::dontSendNotification);
    loopLabel.setText("Loop overlap in timeline seconds  /  0.1 to 30 seconds", juce::dontSendNotification);
    int targetIndex = -1, referenceIndex = -1;
    for (size_t i = 0; i < baseline.clips.size(); ++i)
    {
        const auto& clip = baseline.clips[i]; const auto text = clip.media->name + " / lane " + juce::String(clip.track + 1) + " / " + juce::String(clip.start, 2) + " s";
        reference.addItem(text, (int) i + 1); adjusted.addItem(text, (int) i + 1); if (clip.id == clipId) targetIndex = (int) i;
    }
    if (const auto* link = baseline.transitionFor(clipId))
        for (size_t i = 0; i < baseline.clips.size(); ++i) if (baseline.clips[i].id == link->outgoing) referenceIndex = (int) i;
    if (referenceIndex < 0 && targetIndex >= 0) for (size_t i = 0; i < baseline.clips.size(); ++i)
        if ((int) i != targetIndex && std::min(baseline.clips[i].end(), baseline.clips[(size_t) targetIndex].end())
            - std::max(baseline.clips[i].start, baseline.clips[(size_t) targetIndex].start) >= .1) { referenceIndex = (int) i; break; }
    adjusted.setSelectedId(targetIndex + 1, juce::dontSendNotification); reference.setSelectedId(referenceIndex + 1, juce::dontSendNotification);
    for (auto* slider : {&begin, &end, &pitch, &cents})
    { slider->setSliderStyle(juce::Slider::LinearHorizontal); slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 105, 27); }
    begin.setName("Overlap start in timeline seconds"); end.setName("Overlap end in timeline seconds");
    begin.setTextValueSuffix(" s start"); end.setTextValueSuffix(" s end");
    pitch.setName("Trial pitch in musical steps"); pitch.setRange(-6, 6, 1); pitch.setTextValueSuffix(" steps");
    cents.setName("Fine pitch tuning in cents"); cents.setRange(-50, 50, 5); cents.setTextValueSuffix(" cents"); cents.setVisible(false);
    cents.setTooltip("Small pitch adjustments: 100 cents is one musical step. Tempo stays unchanged.");
    hear.addItemList({"Reference", "Adjusted", "Together"}, 1); hear.setSelectedId(3, juce::dontSendNotification);
    hear.onChange = [this] { listen(version); };
    reference.onChange = adjusted.onChange = [this] { changedPair(); };
    begin.onValueChange = end.onValueChange = [this]
    {
        if (updating) return;
        if (cancellation) *cancellation = true; ++generation; pending = false; preview.stop(); original.reset(); kept.reset(); trial.reset();
        status.setText("Loop changed. Prepare loop to compare the new passage.", juce::dontSendNotification); refresh();
    };
    pitch.onValueChange = cents.onValueChange = [this] { if (!updating && original) request(); };
    lower.onClick = [this] { pitch.setValue(std::max(-6., pitch.getValue() - 1), juce::sendNotificationSync); };
    higher.onClick = [this] { pitch.setValue(std::min(6., pitch.getValue() + 1), juce::sendNotificationSync); };
    fine.onClick = [this] { cents.setVisible(fine.getToggleState()); resized(); };
    prepare.onClick = [this] { request(); };
    originalButton.onClick = [this] { listen(0); }; keptButton.onClick = [this] { listen(1); }; trialButton.onClick = [this] { listen(2); };
    keep.onClick = [this]
    { if (auto audio = selectedAudio()) { kept = audio; keptPitch = audio->pitch; listen(1); status.setText("Kept for comparison. The arrangement is unchanged until Apply.", juce::dontSendNotification); } };
    applyButton.onClick = [this] { applyPitch(); };
    cancel.onClick = [this] { close(); if (auto* window = findParentComponentOfClass<juce::DialogWindow>()) window->closeButtonPressed(); };
    preview.feedback = [this](const auto& text) { status.setText(text, juce::dontSendNotification); };
    applyButton.setTooltip("Prepare and apply only this segment's chosen pitch. Placement, BPM, source key and other occurrences stay unchanged. One arrangement Undo restores it.");
    changedPair();
}
HarmonyEarEditor::~HarmonyEarEditor() { close(); }
void HarmonyEarEditor::close() { closed = true; if (cancellation) *cancellation = true; preview.stop(); }
std::shared_ptr<HarmonyAudio> HarmonyEarEditor::selectedAudio() const { return version == 0 ? original : version == 1 ? kept : trial; }
void HarmonyEarEditor::changedPair()
{
    if (cancellation) *cancellation = true; ++generation; pending = false; preview.stop(); original.reset(); kept.reset(); trial.reset();
    const int a = reference.getSelectedItemIndex(), b = adjusted.getSelectedItemIndex();
    if (a < 0 || b < 0 || a == b) { status.setText("Choose two different segments that overlap in the arrangement.", juce::dontSendNotification); refresh(); return; }
    const auto& r = baseline.clips[(size_t) a]; const auto& t = baseline.clips[(size_t) b];
    const double start = std::max(r.start, t.start), finish = std::min(r.end(), t.end());
    if (finish - start < .1) { status.setText("These segments do not overlap. Choose another reference or arrange an overlap first.", juce::dontSendNotification); refresh(); return; }
    updating = true; begin.setRange(start, finish - .1, .001); end.setRange(start + .1, finish, .001);
    begin.setValue(start, juce::dontSendNotification); end.setValue(std::min(finish, start + 8), juce::dontSendNotification);
    const double coarse = juce::jlimit(-6., 6., std::round(t.pitchSemitones));
    pitch.setValue(coarse, juce::dontSendNotification); cents.setValue((t.pitchSemitones - coarse) * 100, juce::dontSendNotification); updating = false;
    keptPitch = t.pitchSemitones; status.setText("Prepare a short overlap, then Play loop and compare Original / Kept / Trial. No key labels are required.", juce::dontSendNotification); refresh();
}
void HarmonyEarEditor::request()
{
    if (closed || applying) return;
    const int a = reference.getSelectedItemIndex(), b = adjusted.getSelectedItemIndex();
    if (a < 0 || b < 0 || a == b) return;
    const auto& r = baseline.clips[(size_t) a]; const auto& t = baseline.clips[(size_t) b];
    const juce::Range<double> loop(begin.getValue(), end.getValue());
    if (loop.getLength() < .1 || loop.getLength() > 30 || loop.getStart() < std::max(r.start, t.start) - .001
        || loop.getEnd() > std::min(r.end(), t.end()) + .001)
    { status.setText("Keep the loop inside both segments' overlap, from 0.1 to 30 seconds.", juce::dontSendNotification); return; }
    if (earFingerprint(engine) != fingerprint)
    { preview.stop(); status.setText("The arrangement changed. Close and reopen this comparison.", juce::dontSendNotification); return; }
    juce::String error; auto renderer = engine.createExportRenderer(error);
    if (!renderer) { status.setText(error, juce::dontSendNotification); return; }
    if (juce::JSON::toString(projectToJson(renderer->project), true, 17) != fingerprint)
    { status.setText("The arrangement changed while opening its audio. Reopen this comparison.", juce::dontSendNotification); return; }
    if (cancellation) *cancellation = true;
    cancellation = std::make_shared<std::atomic<bool>>(false); const auto token = cancellation; const auto requestId = ++generation;
    preview.stop(); pending = true; trial.reset(); status.setText("Preparing a bounded listening loop. The arrangement stays unchanged.", juce::dontSendNotification); refresh();
    const auto oldOriginal = original, oldKept = kept; const double desired = trialPitch(), initialPitch = keptPitch;
    workers.addJob([safe = juce::Component::SafePointer<HarmonyEarEditor>(this), copy = std::shared_ptr<Engine>(renderer.release()), token,
        requestId, referenceId = r.id, targetId = t.id, loop, desired, initialPitch, oldOriginal, oldKept]
    {
        auto base = oldOriginal; auto remembered = oldKept; auto changed = std::make_shared<HarmonyAudio>(); juce::Result result = juce::Result::ok();
        if (!base) { base = std::make_shared<HarmonyAudio>(); result = renderHarmony(*copy, referenceId, targetId, loop, 0, *token, *base); }
        if (result.wasOk() && !*token)
        {
            if (desired == 0) changed = base;
            else { result = renderHarmony(*copy, referenceId, targetId, loop, desired, *token, *changed); if (result.wasOk()) matchHarmonyLevel(*changed, *base); }
        }
        if (result.wasOk() && !remembered && !*token)
        {
            if (initialPitch == desired) remembered = changed;
            else if (initialPitch == 0) remembered = base;
            else { remembered = std::make_shared<HarmonyAudio>(); result = renderHarmony(*copy, referenceId, targetId, loop, initialPitch, *token, *remembered); if (result.wasOk()) matchHarmonyLevel(*remembered, *base); }
        }
        if (*token) return;
        juce::MessageManager::callAsync([safe, token, requestId, result, base, remembered, changed]
        {
            if (!safe || safe->closed || *token || safe->generation != requestId) return;
            safe->pending = false;
            if (result.failed()) { safe->status.setText(result.getErrorMessage(), juce::dontSendNotification); safe->refresh(); return; }
            safe->original = base; safe->kept = remembered; safe->trial = changed; safe->listen(2);
            safe->status.setText("Ready. Compare the same loop at matched listening levels; check vocals and another passage before Apply.", juce::dontSendNotification);
        });
    });
}
void HarmonyEarEditor::listen(int selection)
{
    version = selection;
    if (auto audio = selectedAudio()) player.pcm(audio->hear[(size_t) (hear.getSelectedId() - 1)], audio->sampleRate);
    refresh();
}
void HarmonyEarEditor::refresh()
{
    auto selected = selectedAudio();
    result.setText("Trial: " + juce::String(trialPitch(), 2) + " steps    |    Kept: " + juce::String(keptPitch, 2)
        + "    |    Hearing: " + (selected ? juce::String(selected->pitch, 2) + " steps" : juce::String("prepare loop"))
        + "    |    Source key labels stay unchanged.", juce::dontSendNotification);
    preview.play.setEnabled(selected != nullptr && !pending && !applying);
    for (auto* b : {&originalButton, &keptButton, &trialButton}) b->setToggleState(false, juce::dontSendNotification);
    (version == 0 ? originalButton : version == 1 ? keptButton : trialButton).setToggleState(true, juce::dontSendNotification);
    originalButton.setEnabled(original != nullptr && !applying); keptButton.setEnabled(kept != nullptr && !applying); trialButton.setEnabled(trial != nullptr && !applying);
    keep.setEnabled(selected != nullptr && !pending && !applying); applyButton.setEnabled(selected != nullptr && !pending && !applying);
    prepare.setEnabled(!applying); reference.setEnabled(!applying); adjusted.setEnabled(!applying);
    for (auto* c : std::initializer_list<juce::Component*> {&pitch, &cents, &lower, &higher}) c->setEnabled(!applying && !pending);
    begin.setEnabled(!applying); end.setEnabled(!applying);
}
void HarmonyEarEditor::applyPitch()
{
    if (applying || pending || closed || !selectedAudio()) return;
    const int index = adjusted.getSelectedItemIndex(); if (index < 0) return;
    Project project; project.clips = {baseline.clips[(size_t) index]}; project.clips[0].pitchSemitones = selectedAudio()->pitch;
    if (cancellation) *cancellation = true;
    cancellation = std::make_shared<std::atomic<bool>>(false); const auto token = cancellation; const auto requestId = ++generation;
    preview.stop(); applying = true; status.setText("Preparing segment pitch for playback and export. Cancel keeps the arrangement unchanged.", juce::dontSendNotification); refresh();
    int64_t resident = 0; std::set<const PreparedAudio*> seen;
    for (const auto& c : baseline.clips)
    {
        if (c.prepared && seen.insert(c.prepared.get()).second) resident += c.prepared->residentBytes();
        for (const auto& s : c.stemPrepared) if (s && seen.insert(s.get()).second) resident += s->residentBytes();
    }
    workers.addJob([safe = juce::Component::SafePointer<HarmonyEarEditor>(this), token, requestId, project = std::move(project), resident]() mutable
    {
        if (*token) return;
        const auto error = prepareAppendedClips(project, 0, resident); if (*token) return;
        juce::MessageManager::callAsync([safe, token, requestId, error, prepared = std::move(project.clips[0])]
        {
            if (!safe || safe->closed || *token || safe->generation != requestId) return;
            safe->applying = false;
            const auto result = error.isEmpty() ? safe->apply(safe->fingerprint, prepared) : juce::Result::fail(error);
            if (result.failed()) { safe->status.setText(result.getErrorMessage(), juce::dontSendNotification); safe->refresh(); return; }
            safe->status.setText("Pitch applied to this segment. Undo restores it.", juce::dontSendNotification);
            if (auto* window = safe->findParentComponentOfClass<juce::DialogWindow>()) window->closeButtonPressed();
        });
    });
}
void HarmonyEarEditor::paint(juce::Graphics& g) { g.fillAll(colour::background); }
void HarmonyEarEditor::resized()
{
    auto r = getLocalBounds().reduced(16); heading.setBounds(r.removeFromTop(32)); instructions.setBounds(r.removeFromTop(40));
    referenceLabel.setBounds(r.removeFromTop(22)); reference.setBounds(r.removeFromTop(29)); adjustedLabel.setBounds(r.removeFromTop(22)); adjusted.setBounds(r.removeFromTop(29));
    loopLabel.setBounds(r.removeFromTop(26)); begin.setBounds(r.removeFromTop(29)); end.setBounds(r.removeFromTop(29));
    auto prep = r.removeFromTop(30); prepare.setBounds(prep.removeFromLeft(150).reduced(2, 0)); hear.setBounds(prep.removeFromLeft(170).reduced(2, 0)); fine.setBounds(prep);
    r.removeFromTop(10); auto adjustment = r.removeFromTop(32); lower.setBounds(adjustment.removeFromLeft(100).reduced(2, 0)); higher.setBounds(adjustment.removeFromRight(100).reduced(2, 0)); pitch.setBounds(adjustment);
    if (fine.getToggleState()) cents.setBounds(r.removeFromTop(28));
    auto comparisons = r.removeFromTop(32); for (auto* b : {&originalButton, &keptButton, &trialButton, &keep}) b->setBounds(comparisons.removeFromLeft(175).reduced(2, 0));
    result.setBounds(r.removeFromTop(38)); preview.setBounds(r.removeFromTop(53)); status.setBounds(r.removeFromTop(48));
    auto footer = r.removeFromBottom(32); cancel.setBounds(footer.removeFromRight(110)); footer.removeFromRight(8); applyButton.setBounds(footer.removeFromRight(210));
}
}
