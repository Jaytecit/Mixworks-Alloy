#include "Workstation.h"
#include <cmath>

namespace mixworks
{
juce::Component* LibraryTable::refreshComponentForCell(int row, int column, bool, juce::Component* previous)
{
    if (!isPlaylist || column != 6) { delete previous; return nullptr; }
    auto* picker = dynamic_cast<juce::ComboBox*>(previous);
    if (!picker) { delete previous; picker = new juce::ComboBox(); }
    picker->onChange = nullptr;
    picker->clear(juce::dontSendNotification);
    picker->addItem("None (keep alignment)", 1);
    picker->addItem("Bass swap", 2);
    picker->addItem("Crossfade", 3);
    const auto choices = owner.engine.project.transitionPresets;
    if (!choices.empty()) picker->addSectionHeading("Saved session presets");
    for (size_t i = 0; i < choices.size(); ++i) picker->addItem(choices[i].name, 100 + (int) i);
    const auto* clip = clipAtRow(row);
    const auto* link = clip ? owner.engine.project.transitionFor(clip->id) : nullptr;
    const bool editable = link && !owner.busy && !owner.engine.exporting && owner.pendingTempo <= 0
        && !owner.tempoPreparationPending && !owner.engine.project.hasPendingPlacement()
        && !owner.engine.project.clipTransitionLocked(link->outgoing) && !owner.engine.project.clipTransitionLocked(link->incoming);
    picker->setEnabled(editable);
    picker->setTitle("Transition into " + (clip ? clip->media->libraryTitle() : juce::String("track")));
    picker->setTooltip("Changes the transition INTO this track. Keeps the frame length and track positions; Undo restores the previous treatment. Unlock both tracks to edit.");
    int selected = 0;
    TransitionStyle style;
    if (link && owner.engine.project.captureTransitionStyle(clip->id, style).wasOk())
    {
        for (size_t i = 0; i < choices.size(); ++i) if (style == choices[i].style) { selected = 100 + (int) i; break; }
        if (!selected && !style.bassPositions && !style.filter.active()
            && std::none_of(style.bands.begin(), style.bands.end(), [](const auto& b) { return b.has_value(); }))
        {
            const auto matches = [&](std::array<double, 4> values)
            { for (size_t i = 0; i < 4; ++i) if (std::abs(style.volume[i] - values[i]) > 1e-7) return false; return true; };
            if (matches({0, 1, 0, 1})) selected = 3;
            else if (matches({0, 0, 1, 1})) selected = 1;
        }
    }
    picker->setSelectedId(selected, juce::dontSendNotification);
    if (!selected) picker->setText(link ? "Custom / edited" : "No incoming transition", juce::dontSendNotification);
    if (!clip) return picker;
    const auto incoming = clip->id;
    const auto safe = juce::Component::SafePointer<Workstation>(&owner);
    picker->onChange = [safe, picker, incoming, choices]
    {
        const int choice = picker->getSelectedId();
        if (!safe || !picker->isEnabled() || safe->engine.exporting || !safe->engine.project.transitionFor(incoming)) return;
        if (safe->selectedTransition != incoming) safe->engine.endTransitionRehearsal();
        safe->selectedTransition = incoming; safe->selectedClip = incoming;
        if (choice >= 1 && choice <= 3) safe->changeSelectedTransition({}, (TransitionPreset) (choice - 1));
        else if (choice >= 100 && choice - 100 < (int) choices.size()) safe->applySelectedStyle(choices[(size_t) (choice - 100)].style);
    };
    return picker;
}
}
