#include "Workstation.h"
#include "PresetNodes.h"
#include <cmath>

namespace mixworks
{
void Workstation::addSelectedTransition(const juce::String& outgoing, const juce::String& incoming)
{
    if (!canInsertArrangement() || editSelection.range) { setStatus("Stop playback and select whole tracks before adding a transition."); return; }
    engine.flushMidiControls();
    const juce::ScopedLock guard(engine.lock);
    auto candidate = engine.project;
    const auto result = candidate.addTransition(outgoing,incoming,transitionOverlapBeats(),selectedTransitionPreset());
    if (result.failed()) { setStatus(result.getErrorMessage()); return; }
    remember(); engine.project = std::move(candidate); selectedTransition = selectedClip = incoming;
    transitionNodeOptionsFor.clear(); showToolPage(1); refresh();
    setStatus("Transition added. Drag either bound anywhere inside both tracks. Outside audio stays audible unless muted.");
}
void Workstation::chooseAddTransition()
{
    if (!canInsertArrangement() || editSelection.range) { setStatus("Stop playback and select whole tracks before adding a transition."); return; }
    auto ids = editSelection.clips;
    if (ids.empty() && currentClip()) ids.push_back(selectedClip);
    if (ids.empty() || ids.size() > 2) { setStatus("Select one track, or Ctrl-select two overlapping tracks, then Add Transition."); return; }
    const auto ordered = [&](const juce::String& a, const juce::String& b)
    {
        const auto* first = engine.project.clipFor(a); const auto* second = engine.project.clipFor(b);
        return first && second && (first->start < second->start || (first->start == second->start && first->track < second->track));
    };
    if (ids.size() == 2)
    { if (!ordered(ids[0],ids[1])) std::swap(ids[0],ids[1]); addSelectedTransition(ids[0],ids[1]); return; }
    const auto* selected = engine.project.clipFor(ids.front()); if (!selected) return;
    std::vector<std::pair<juce::String,juce::String>> pairs; juce::PopupMenu menu;
    for (const auto& clip : engine.project.clips)
    {
        if (clip.id == selected->id || std::min(clip.end(),selected->end()) <= std::max(clip.start,selected->start)) continue;
        const bool first = ordered(selected->id,clip.id);
        const auto out = first ? selected->id : clip.id, in = first ? clip.id : selected->id;
        if (engine.project.clipTransitionLocked(out) || engine.project.clipTransitionLocked(in)
            || std::any_of(engine.project.transitionLinks.begin(),engine.project.transitionLinks.end(),[&](const auto& l) { return l.outgoing == out || l.incoming == in; })) continue;
        pairs.emplace_back(out,in);
        menu.addItem((int)pairs.size(),(first ? "To: " : "From: ") + clip.media->libraryTitle() + " / lane " + juce::String(clip.track+1));
    }
    if (pairs.empty()) { setStatus("Overlap this track with a free partner, or unlink its existing transition first."); return; }
    if (pairs.size() == 1) { addSelectedTransition(pairs[0].first,pairs[0].second); return; }
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&addTransitionButton),
        [safe=juce::Component::SafePointer<Workstation>(this),pairs](int choice)
        { if (safe && choice > 0 && choice <= (int)pairs.size()) safe->addSelectedTransition(pairs[(size_t)choice-1].first,pairs[(size_t)choice-1].second); });
}
void Workstation::initialiseTransitionUI()
{
    addAndMakeVisible(transitionPanel); transitionViewport.setViewedComponent(&transitionBody, false);
    transitionViewport.setScrollBarsShown(true, true); transitionViewport.setScrollBarThickness(12);
    transitionViewport.onResize = [this] { layoutTransitionUI(); };
    addAndMakeVisible(transitionTab); transitionTab.getProperties().set("alloyTab", true);
    transitionTab.onClick = [this]
    {
        if (!engine.project.transitionFor(selectedTransition))
        {
            engine.project.ensureTransitionLinks();
            const auto link = std::find_if(engine.project.transitionLinks.begin(), engine.project.transitionLinks.end(), [&](const auto& item)
            { return item.incoming == selectedClip || item.outgoing == selectedClip; });
            if (link != engine.project.transitionLinks.end()) selectedTransition = link->incoming;
        }
        toggleConsole(1); refresh();
    };
    for (auto* control : std::initializer_list<juce::Component*> {&transitionLengthLabel,
        &transitionPresetLabel, &selectedTransitionLength, &transitionPresetForSelection, &transitionLock, &previousTransition, &nextTransition}) transitionBody.addAndMakeVisible(control);
    transitionDefaultsLabel.setText("Defaults", juce::dontSendNotification);
    transitionBody.addAndMakeVisible(transitionDefaultsLabel);
    transitionBody.addAndMakeVisible(addTransitionButton); transitionBody.addAndMakeVisible(transitionMuteOutside);
    addTransitionButton.setTooltip("Link two overlapping tracks without moving them. Ctrl-select two, or select one and choose its partner. Ordinary placement stays independent.");
    addTransitionButton.onClick = [this] { chooseAddTransition(); };
    transitionMuteOutside.setTooltip("For this pair, silence the incoming track before the frame and the outgoing track after it. Off keeps exterior audio and its automation. Saved with the mix; one Undo.");
    transitionMuteOutside.onClick = [this]
    {
        if (!canInsertArrangement()) { refresh(); return; }
        const juce::ScopedLock guard(engine.lock); auto candidate = engine.project;
        const auto result = candidate.setTransitionMuteOutside(selectedTransition,transitionMuteOutside.getToggleState());
        if (result.failed()) { setStatus(result.getErrorMessage()); refresh(); return; }
        remember(); engine.project = std::move(candidate); refresh();
    };
    const std::array<const char*, 4> names {"Volume", "Bass", "Mid", "Treble"};
    const std::array<const char*, 7> modes {"None", "Crossfade", "Fade in", "Fade out", "Swap", "Manual", "Bass fade handover"};
    const std::array<juce::Colour, 4> colours {juce::Colour(0xffffad70), juce::Colour(0xffe4d15b), juce::Colour(0xff76c7ef), juce::Colour(0xffd3a3ef)};
    for (size_t band = 0; band < 4; ++band)
    {
        auto& label = transitionBandLabels[band]; label.setText(names[band], juce::dontSendNotification); label.setColour(juce::Label::textColourId, colours[band]); transitionBody.addAndMakeVisible(label);
        auto& show = transitionCurveShow[band]; auto& only = transitionCurveOnly[band];
        for (auto* button : {&show, &only}) { transitionBody.addAndMakeVisible(button); button->getProperties().set("alloyTab", true); }
        show.setButtonText("Show"); only.setButtonText("Only"); show.setClickingTogglesState(true);
        show.setTitle(juce::String(names[band]) + " curve visibility"); only.setTitle("Isolate " + juce::String(names[band]) + " curve");
        show.setTooltip("Show this band's automation curve. This affects the display only; audio and automation continue unchanged.");
        only.setTooltip("Display only this band's automation curve. Click again to restore the other visible curves. Audio is unchanged.");
        show.onClick = [this, band]
        {
            transitionCurveShown[band] = transitionCurveShow[band].getToggleState();
            if (!transitionCurveShown[band] && transitionCurveOnlyBand == (int) band) transitionCurveOnlyBand = -1;
            refreshTransitionUI(); timeline.repaint();
        };
        only.onClick = [this, band]
        {
            transitionCurveOnlyBand = transitionCurveOnlyBand == (int) band ? -1 : (int) band;
            if (transitionCurveOnlyBand >= 0) transitionCurveShown[band] = true;
            refreshTransitionUI(); timeline.repaint();
        };
        auto& mode = transitionBandModes[band]; auto& strength = transitionBandStrengths[band]; auto& gap = transitionBandGaps[band]; auto& position = transitionBandPositions[band];
        for (auto* combo : {&mode, &strength, &gap, &position}) transitionBody.addAndMakeVisible(combo);
        mode.setTitle(juce::String(names[band]) + " transition mode");
        for (int id = 0; id < (band == 1 ? 7 : 6); ++id) mode.addItem(modes[(size_t) id], id + 1);
        if (band == 0) mode.addItem("Constant power", (int) TransitionBandMode::constantPower + 1);
        strength.setTitle(juce::String(names[band]) + " transition strength");
        for (int amount : {25, 50, 75, 100}) strength.addItem(juce::String(amount) + "% strength", amount);
        gap.setTitle(juce::String(names[band]) + " swap gap");
        int gapId = 1; for (double beats : {0., .5, 1., 2., 4., 8.}) gap.addItem("Gap " + juce::String(beats, beats == .5 ? 1 : 0) + " beats", gapId++);
        position.setTitle(juce::String(names[band]) + " swap position");
        for (int amount : {25, 50, 75}) position.addItem("Swap at " + juce::String(amount) + "%", amount);
        mode.setTextWhenNothingSelected("Existing / custom");
        mode.setTooltip("Place this preset as ordinary nodes inside the selected frame, replacing that band's shape. Then drag/add/delete nodes normally. Manual keeps the current shape for editing.");
        strength.setTooltip("Place the chosen shape again at this strength, replacing its nodes in this frame. 100% reaches a full cut.");
        gap.setTooltip("Beat-based separation between the outgoing cut and incoming entry. Gaps must fit inside the frame.");
        position.setTooltip("Position of this band's swap inside the transition, measured through the mix beat map.");
        for (auto* combo : {&mode, &strength, &gap, &position}) combo->onChange = [this, band] { if (!refreshing) changeSelectedTransitionBand((int) band); };
    }
    const juce::String shortcuts = "Alt + Left/Right: navigate. Ctrl + Alt + Left/Right: move frame. Shift + Alt + Left/Right: resize end. Alt + L: lock/unlock.";
    previousTransition.setTooltip(shortcuts); nextTransition.setTooltip(shortcuts);
    transitionBody.addAndMakeVisible(transitionAdvanced);
    transitionBody.addChildComponent(transitionAdvancedBody);
    transitionAdvanced.onClick = [this] { transitionAdvancedOpen = !transitionAdvancedOpen; layoutTransitionUI(); };
    transitionLengthLabel.setText("Length", juce::dontSendNotification);
    transitionPresetLabel.setText("Preset", juce::dontSendNotification);
    selectedTransitionLength.setTitle("Selected transition length");
    selectedTransitionLength.setTooltip("Resize only the selected frame, keeping its start and both songs fixed. One Undo restores it.");
    for (int bars : {4, 8, 16, 32}) selectedTransitionLength.addItem(juce::String(bars) + " bars", bars);
    selectedTransitionLength.setTextWhenNothingSelected("Select a transition");
    selectedTransitionLength.setEditableText(true);
    selectedTransitionLength.onChange = [this]
    { if (!refreshing) { const double bars = selectedTransitionLength.getText().getDoubleValue(); if (bars > 0) changeSelectedTransition(bars, {}); } };
    transitionPresetForSelection.setTitle("Selected transition preset");
    transitionPresetForSelection.setTextWhenNothingSelected("Apply preset...");
    transitionPresetForSelection.addItem("None", 1);
    transitionPresetForSelection.addItem("Bass swap", 2); transitionPresetForSelection.addItem("Crossfade", 3);
    transitionPresetForSelection.setTooltip("Apply only to this pair. Other segments and the tempo map stay in place. Manual bass conflicts are reported.");
    transitionPresetForSelection.onChange = [this]
    { if (!refreshing && transitionPresetForSelection.getSelectedId() > 0) changeSelectedTransition({}, (TransitionPreset) (transitionPresetForSelection.getSelectedId() - 1)); };
    transitionLock.onClick = [this] { changeSelectedTransition({}, {}, true); };
    transitionFilterLabel.setText("Filter", juce::dontSendNotification);
    for (auto* component : std::initializer_list<juce::Component*> {&transitionFilterLabel, &transitionFilterMode, &transitionFilterDepth}) transitionBody.addAndMakeVisible(component);
    transitionFilterMode.setTitle("Transition filter mode");
    transitionFilterMode.addItem("None", 1); transitionFilterMode.addItem("High to low", 2); transitionFilterMode.addItem("Low to high", 3);
    transitionFilterDepth.setTitle("Transition filter depth");
    for (int amount : {25, 50, 75, 100}) transitionFilterDepth.addItem(juce::String(amount) + "% depth", amount);
    transitionFilterMode.setTooltip("High to low closes the outgoing low-pass while opening the incoming high-pass. Low to high reverses those filters. Both follow this frame.");
    transitionFilterDepth.setTooltip("Place the chosen sweep again as ordinary Filter nodes, replacing cutoff points in this frame. Other parameters stay in place. None clears this frame's cutoff shape.");
    for (auto* combo : {&transitionFilterMode, &transitionFilterDepth}) combo->onChange = [this] { if (!refreshing) changeSelectedTransitionFilter(); };
    previousTransition.onClick = [this] { navigateTransition(-1); };
    nextTransition.onClick = [this] { navigateTransition(1); };
    transitionLock.setTooltip("Protect this transition and its participating segments from edits. Click again to unlock before changing it.");
    transitionAdvanced.setTooltip("Show A/B/C comparisons and saved transition presets.");
    initialiseTransitionRehearsalUI();
}
void Workstation::layoutTransitionUI()
{
    const int width = std::max(520, transitionViewport.getWidth() - transitionViewport.getScrollBarThickness());
    const int columns = width >= 900 ? 4 : 2;
    const int bandRows = 4 / columns;
    const int coreHeight = 36 + 28 + 12 + 28 + 16 + bandRows * 140 + 12 + 28 + 12 + 16;
    const int advancedHeight = width >= 700 ? 220 : 350;
    transitionBody.setSize(width, std::max(coreHeight + (transitionAdvancedOpen ? advancedHeight : 0), transitionViewport.getHeight()));
    auto area = transitionBody.getLocalBounds().reduced(10, 8);
    auto row = area.removeFromTop(28);
    addTransitionButton.setBounds(row.removeFromLeft(150)); row.removeFromLeft(12);
    transitionMuteOutside.setBounds(row.removeFromLeft(230)); area.removeFromTop(8);
    row = area.removeFromTop(28);
    auto navigation = row.removeFromRight(160);
    previousTransition.setBounds(navigation.removeFromLeft(80).withTrimmedRight(3)); nextTransition.setBounds(navigation);
    transitionDefaultsLabel.setBounds(row.removeFromLeft(76));
    transitionPreset.setBounds(row.removeFromLeft(150).withTrimmedRight(6));
    transitionLength.setBounds(row.removeFromLeft(150)); area.removeFromTop(12);
    row = area.removeFromTop(28);
    transitionLengthLabel.setBounds(row.removeFromLeft(42)); selectedTransitionLength.setBounds(row.removeFromLeft(106)); row.removeFromLeft(12);
    transitionLock.setBounds(row.removeFromRight(100)); row.removeFromRight(12);
    transitionPresetLabel.setBounds(row.removeFromLeft(44)); transitionPresetForSelection.setBounds(row);
    area.removeFromTop(16);
    const int bandWidth = (area.getWidth() - (columns - 1) * 12) / columns;
    for (size_t band = 0; band < 4; ++band)
    {
        auto cell = juce::Rectangle<int>(area.getX() + (int) (band % columns) * (bandWidth + 12), area.getY() + (int) (band / columns) * 140, bandWidth, 132);
        transitionBandLabels[band].setBounds(cell.removeFromTop(18));
        transitionBandModes[band].setBounds(cell.removeFromTop(26)); cell.removeFromTop(4);
        row = cell.removeFromTop(24); transitionCurveShow[band].setBounds(row.removeFromLeft(row.getWidth() / 2).withTrimmedRight(3)); transitionCurveOnly[band].setBounds(row); cell.removeFromTop(4);
        row = cell.removeFromTop(24); transitionBandStrengths[band].setBounds(row.removeFromLeft(row.getWidth() / 2).withTrimmedRight(3)); transitionBandGaps[band].setBounds(row); cell.removeFromTop(4);
        transitionBandPositions[band].setBounds(cell.removeFromTop(26));
    }
    area.removeFromTop(bandRows * 140 + 12);
    row = area.removeFromTop(28);
    transitionAdvanced.setBounds(row.removeFromRight(162)); row.removeFromRight(12);
    transitionFilterLabel.setBounds(row.removeFromLeft(42));
    transitionFilterDepth.setBounds(row.removeFromRight(110)); row.removeFromRight(6); transitionFilterMode.setBounds(row);
    area.removeFromTop(16);
    transitionAdvanced.setButtonText(transitionAdvancedOpen ? "Hide rehearsal & presets" : "Rehearsal & presets...");
    transitionAdvanced.setToggleState(transitionAdvancedOpen, juce::dontSendNotification);
    transitionAdvancedBody.setVisible(transitionAdvancedOpen);
    transitionAdvancedBody.setBounds(area.withHeight(advancedHeight));
    auto advanced = transitionAdvancedBody.getLocalBounds(); layoutTransitionRehearsalUI(advanced);
}

void Workstation::changeSelectedTransitionFilter()
{
    if (busy) return;
    if (pendingTempo > 0 || tempoPreparationPending) { setStatus("Wait for tempo preparation before editing filters."); refresh(); return; }
    engine.flushMidiControls(); engine.finishAutomation();
    const juce::ScopedLock guard(engine.lock);
    const auto* link = engine.project.transitionFor(selectedTransition); if (!link) return;
    auto setting = transitionFilterNodeOptions;
    if (transitionFilterMode.getSelectedId() > 0) setting.mode = (TransitionFilterMode) (transitionFilterMode.getSelectedId() - 1);
    if (transitionFilterDepth.getSelectedId() > 0) setting.strength = transitionFilterDepth.getSelectedId() / 100.;
    auto candidate = engine.project; const auto result = placeTransitionFilterNodes(candidate,selectedTransition,setting);
    if (result.failed()) { setStatus(result.getErrorMessage()); refresh(); return; }
    transitionFilterNodeOptions = setting;
    if (juce::JSON::toString(projectToJson(candidate)) != juce::JSON::toString(projectToJson(engine.project)))
    {
        remember(); engine.project = std::move(candidate);
        setStatus("Filter applied.");
    }
    selectedClip = selectedTransition; selectedAutomationTarget = (int) AutomationTarget::cutoff;
    refresh();
}
void Workstation::changeSelectedTransitionBand(int band)
{
    if (busy || band < 0 || band >= 4) return;
    if (pendingTempo > 0 || tempoPreparationPending) { setStatus("Wait for tempo preparation before editing transition bands."); refresh(); return; }
    engine.flushMidiControls(); engine.finishAutomation();
    const juce::ScopedLock guard(engine.lock);
    const auto* link = engine.project.transitionFor(selectedTransition); if (!link) return;
    const size_t index = (size_t) band;
    auto setting = transitionNodeOptions[index];
    if (transitionBandModes[index].getSelectedId() > 0) setting.mode = (TransitionBandMode) (transitionBandModes[index].getSelectedId() - 1);
    if (transitionBandStrengths[index].getSelectedId() > 0) setting.strength = transitionBandStrengths[index].getSelectedId() / 100.;
    const std::array<double, 6> gaps {0, .5, 1, 2, 4, 8}; const int gap = transitionBandGaps[index].getSelectedId();
    if (gap > 0 && gap <= (int) gaps.size()) setting.gapBeats = gaps[(size_t) gap - 1];
    if (transitionBandPositions[index].getSelectedId() > 0) setting.position = transitionBandPositions[index].getSelectedId() / 100.;
    auto candidate = engine.project;
    const auto result = placeTransitionBandNodes(candidate,selectedTransition,band,setting);
    if (result.failed()) { setStatus(result.getErrorMessage()); refresh(); return; }
    transitionNodeOptions[index] = setting;
    if (juce::JSON::toString(projectToJson(candidate)) != juce::JSON::toString(projectToJson(engine.project)))
    {
        remember(); engine.project = std::move(candidate);
        setStatus("Preset applied.");
    }
    selectedClip = selectedTransition; selectedAutomationTarget = band; selectedStem = -1;
    transitionCurveShown[index] = true; if (transitionCurveOnlyBand >= 0) transitionCurveOnlyBand = band;
    selectTrack(engine.project.clipFor(selectedClip)->track);
    refresh();
}
void Workstation::selectTransition(const juce::String& incoming, bool focus)
{
    const auto* link = engine.project.transitionFor(incoming); if (!link) return;
    if (selectedTransition != incoming) engine.endTransitionRehearsal();
    selectedTransition = incoming; selectedClip = incoming; showToolPage(1);
    if (focus)
    {
        const auto range = engine.project.transitionRange(*link);
        engine.seek(range.getStart());
        viewport.setViewPosition(std::max(0, (int) ((range.getStart() - 2) * timeline.pixelsPerSecond)),
            std::max(0, timeline.laneTop(std::min(engine.project.clipFor(link->outgoing)->track, engine.project.clipFor(incoming)->track))));
    }
    refresh();
}
void Workstation::navigateTransition(int direction)
{
    engine.project.ensureTransitionLinks();
    std::vector<const Project::TransitionLink*> links;
    for (const auto& link : engine.project.transitionLinks) links.push_back(&link);
    std::stable_sort(links.begin(), links.end(), [&](const auto* a, const auto* b)
    { return engine.project.transitionRange(*a).getStart() < engine.project.transitionRange(*b).getStart(); });
    if (links.empty()) return;
    const auto current = std::find_if(links.begin(), links.end(), [&](const auto* link) { return link->incoming == selectedTransition; });
    const int index = current == links.end() ? (direction > 0 ? 0 : (int) links.size() - 1)
        : juce::jlimit(0, (int) links.size() - 1, (int) std::distance(links.begin(), current) + direction);
    selectTransition(links[(size_t) index]->incoming, true);
}
void Workstation::changeSelectedTransition(std::optional<double> bars, std::optional<TransitionPreset> preset, bool toggleLock)
{
    if (busy) return;
    if (pendingTempo > 0 || tempoPreparationPending) { setStatus("Wait for tempo preparation to finish before editing or locking a transition."); refresh(); return; }
    engine.flushMidiControls(); engine.finishAutomation();
    const juce::ScopedLock guard(engine.lock);
    const auto* link = engine.project.transitionFor(selectedTransition); if (!link) { refresh(); return; }
    auto candidate = engine.project;
    const auto result = toggleLock ? candidate.setTransitionLocked(selectedTransition, !link->locked)
        : bars ? candidate.setTransitionLength(selectedTransition, *bars) : preset ? candidate.applyTransitionPreset(selectedTransition, *preset)
        : juce::Result::fail("Choose a transition edit.");
    if (result.failed()) { setStatus(result.getErrorMessage()); refresh(); return; }
    if (juce::JSON::toString(projectToJson(candidate)) == juce::JSON::toString(projectToJson(engine.project))) { refresh(); return; }
    remember(); engine.project = std::move(candidate);
    setStatus(toggleLock ? "Transition " + juce::String(engine.project.transitionFor(selectedTransition)->locked ? "locked. Unlock it to edit its segments or tempo." : "unlocked.")
        : "Selected transition updated. Other segment positions and the tempo map were kept.");
    if (preset && *preset == TransitionPreset::bassSwap)
    {
        selectedClip = selectedTransition; selectedAutomationTarget = 1; selectedStem = -1;
        transitionCurveShown[1] = true; if (transitionCurveOnlyBand >= 0) transitionCurveOnlyBand = 1;
        transitionNodeOptionsFor.clear(); selectTrack(engine.project.clipFor(selectedClip)->track);
    }
    refresh();
}
void Workstation::refreshTransitionUI()
{
    auto* link = engine.project.transitionFor(selectedTransition);
    if (!link || (selectedClip != link->incoming && selectedClip != link->outgoing)) { selectedTransition.clear(); link = nullptr; }
    const bool protectedTracks = link && (engine.project.clipTransitionLocked(link->outgoing) || engine.project.clipTransitionLocked(link->incoming));
    const bool editable = link && !protectedTracks && !busy && !engine.project.hasPendingPlacement();
    addTransitionButton.setEnabled(canInsertArrangement() && currentClip() && !editSelection.range);
    transitionMuteOutside.setEnabled(editable && canInsertArrangement());
    transitionMuteOutside.setToggleState(link && link->muteOutside,juce::dontSendNotification);
    refreshTransitionRehearsalUI(editable);
    selectedTransitionLength.setEnabled(editable); transitionPresetForSelection.setEnabled(editable);
    if (transitionNodeOptionsFor != selectedTransition)
    {
        transitionNodeOptionsFor = selectedTransition;
        transitionFilterNodeOptions = link ? link->filter : TransitionFilterSettings{};
        for (size_t band = 0; band < 4; ++band) transitionNodeOptions[band] = link ? link->bands[band].value_or(TransitionBandSettings{}) : TransitionBandSettings{};
        if (link) for (size_t band = 0; band < 4; ++band) if (!link->bands[band])
        {
            const auto* out = engine.project.clipFor(link->outgoing); const auto* in = engine.project.clipFor(link->incoming);
            if (!out || !in) continue;
            auto& setting = transitionNodeOptions[band];
            if (!out->controls.automation[band].points.empty() || !in->controls.automation[band].points.empty()) setting.mode = TransitionBandMode::manual;
            if (const auto times = engine.project.transitionSwapTimes(*link,(int)band))
            {
                const auto frame = engine.project.transitionRange(*link); const auto clock = engine.project.gridClock();
                const double first = clock.beatsAt(frame.getStart()), length = clock.beatsAt(frame.getEnd())-first;
                if (length > 0)
                {
                    setting.mode = TransitionBandMode::swap;
                    if (band == 1 && !link->bassSwap)
                    {
                        const auto& points = out->controls.automation[band].points;
                        bool sharp = false;
                        for (size_t i = 1; i < points.size(); ++i)
                        {
                            const double t = out->start+out->localTimeForSource(points[i].time);
                            if (t > frame.getStart()+1.e-8 && t < frame.getEnd()-1.e-8 && points[i-1].value != points[i].value && points[i-1].bend >= 1) sharp = true;
                        }
                        if (!sharp) setting.mode = TransitionBandMode::swapFade;
                    }
                    setting.position = ((clock.beatsAt((*times)[0])+clock.beatsAt((*times)[1]))*.5-first)/length;
                    setting.gapBeats = std::max(0.,clock.beatsAt((*times)[1])-clock.beatsAt((*times)[0]));
                    float cut = 0; for (const auto& node : out->controls.automation[band].points)
                    { const double t = out->start+out->localTimeForSource(node.time); if (t >= frame.getStart()-1.e-8 && t <= frame.getEnd()+1.e-8) cut = std::min(cut,node.value); }
                    setting.strength = cut <= (band == 0 ? -160.f : -60.f) ? 1. : 1.-juce::Decibels::decibelsToGain((double)cut);
                }
            }
        }
    }
    const auto filter = transitionFilterNodeOptions;
    transitionFilterMode.setSelectedId((int) filter.mode + 1, juce::dontSendNotification); transitionFilterMode.setEnabled(editable);
    transitionFilterDepth.setSelectedId(0, juce::dontSendNotification);
    transitionFilterDepth.setText(juce::String(filter.strength * 100, 0) + "% depth", juce::dontSendNotification);
    for (int amount : {25, 50, 75, 100}) if (std::abs(filter.strength * 100 - amount) < 1e-7) transitionFilterDepth.setSelectedId(amount, juce::dontSendNotification);
    transitionFilterDepth.setEnabled(editable && filter.mode != TransitionFilterMode::none);
    transitionLock.setEnabled(link && !busy && !engine.project.hasPendingPlacement());
    transitionLock.setButtonText(link && link->locked ? "Unlock" : "Lock");
    transitionLock.setToggleState(link && link->locked, juce::dontSendNotification);
    previousTransition.setEnabled(!engine.project.transitionLinks.empty()); nextTransition.setEnabled(!engine.project.transitionLinks.empty());
    transitionPresetForSelection.setSelectedId(0, juce::dontSendNotification);
    selectedTransitionLength.setSelectedId(0, juce::dontSendNotification);
    for (size_t band = 0; band < 4; ++band)
    {
        const auto setting = transitionNodeOptions[band];
        transitionCurveShow[band].setToggleState(transitionCurveShown[band], juce::dontSendNotification);
        transitionCurveOnly[band].setToggleState(transitionCurveOnlyBand == (int) band, juce::dontSendNotification);
        transitionCurveShow[band].setEnabled(link && !busy); transitionCurveOnly[band].setEnabled(link && !busy);
        transitionBandModes[band].setSelectedId(link ? (int) setting.mode + 1 : 0, juce::dontSendNotification);
        transitionBandModes[band].setEnabled(editable);
        transitionBandStrengths[band].setSelectedId(0, juce::dontSendNotification);
        transitionBandStrengths[band].setText(juce::String(setting.strength * 100, 0) + "% strength", juce::dontSendNotification);
        for (int amount : {25, 50, 75, 100}) if (std::abs(setting.strength * 100 - amount) < 1e-7) transitionBandStrengths[band].setSelectedId(amount, juce::dontSendNotification);
        transitionBandStrengths[band].setEnabled(editable && setting.generated());
        transitionBandGaps[band].setSelectedId(0, juce::dontSendNotification);
        transitionBandGaps[band].setText("Gap " + juce::String(setting.gapBeats, 1) + " beats", juce::dontSendNotification);
        int id = 1; for (double gap : {0., .5, 1., 2., 4., 8.}) { if (std::abs(gap - setting.gapBeats) < 1e-7) transitionBandGaps[band].setSelectedId(id, juce::dontSendNotification); ++id; }
        transitionBandGaps[band].setEnabled(editable && setting.swapping());
        transitionBandPositions[band].setSelectedId(0, juce::dontSendNotification);
        transitionBandPositions[band].setText("Swap at " + juce::String(setting.position * 100, 0) + "%", juce::dontSendNotification);
        for (int amount : {25, 50, 75}) if (std::abs(setting.position * 100 - amount) < 1e-7) transitionBandPositions[band].setSelectedId(amount, juce::dontSendNotification);
        transitionBandPositions[band].setEnabled(editable && setting.swapping());
    }
    selectedTransitionLength.setTooltip(link ? transitionKeySummary(*link) : "Select a transition frame or use Previous / Next.");
    if (link)
    {
        const auto range = engine.project.transitionRange(*link);
        const auto map = engine.project.gridClock();
        const double bars = (map.beatsAt(range.getEnd()) - map.beatsAt(range.getStart())) / 4;
        for (int count : {4, 8, 16, 32}) if (std::abs(bars - count) < 1e-6) selectedTransitionLength.setSelectedId(count, juce::dontSendNotification);
        if (selectedTransitionLength.getSelectedId() == 0) selectedTransitionLength.setText(juce::String(bars, 2) + " bars (custom)", juce::dontSendNotification);
    }
}
}
