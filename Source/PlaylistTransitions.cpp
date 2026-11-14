#include "Workstation.h"
#include "EnergySections.h"
#include "PresetNodes.h"
#include <cmath>

namespace mixworks
{
namespace
{
class PreviewArtwork final : public juce::Button
{
public:
    explicit PreviewArtwork(LibraryTable& t) : Button("Preview track in headphones"), model(t) {}
    void paintButton(juce::Graphics& g, bool over, bool) override
    {
        model.paintCell(g,row,5,getWidth(),getHeight(),false);
        if (over || hasKeyboardFocus(true)) { g.setColour(colour::orange); g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(2),3,1); }
    }
    void mouseDown(const juce::MouseEvent& event) override
    { if (event.mods.isPopupMenu()) model.cellClicked(row,5,event); else Button::mouseDown(event); }
    LibraryTable& model; int row = -1;
};
}
juce::Component* LibraryTable::refreshComponentForCell(int row, int column, bool selectedRow, juce::Component* previous)
{
    if (!isPlaylist && column == 5)
    {
        auto* button = dynamic_cast<PreviewArtwork*>(previous);
        if (!button) { delete previous; button = new PreviewArtwork(*this); }
        button->row = row;
        const auto media = row >= 0 && row < getNumRows() ? owner.engine.library[(size_t)owner.visibleMedia[(size_t)row]] : nullptr;
        button->setEnabled(media != nullptr);
        button->setTitle("Preview in headphones: " + (media ? media->libraryTitle() : juce::String()));
        button->setTooltip(getCellTooltip(row,column));
        button->onClick = [safe = juce::Component::SafePointer<Workstation>(&owner), id = media ? media->id : juce::String()]
        { if (safe) safe->previewLibraryTrack(id); };
        return button;
    }
    if (!isPlaylist || column != 6) return accessibleCell(row, column, selectedRow, previous);
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
    if (link && link->alignmentOnly) selected = 1;
    else if (link && link->presetName.isNotEmpty())
    {
        if (link->presetName == "Bass swap") selected = 2;
        else if (link->presetName == "Crossfade") selected = 3;
        else for (size_t i = 0; i < choices.size(); ++i)
            if (link->presetName == choices[i].name) { selected = 100 + (int)i; break; }
    }
    else if (link)
    {
        const auto& project = owner.engine.project;
        const auto* outgoing = project.clipFor(link->outgoing);
        const auto* incoming = project.clipFor(link->incoming);
        TransitionStyle::Nodes captured;
        if (outgoing && incoming && !project.hasPendingPlacement()
            && capturePresetNodes(project, link->incoming, captured).wasOk())
        {
            style.nodes = std::move(captured);
            for (size_t i = 0; i < choices.size(); ++i)
                if (style == choices[i].style) { selected = 100 + (int) i; break; }
        }
        if (!selected && style.nodes)
        {
            // Derive the label from the actual ordinary shape. There is no
            // hidden preset identity that survives an edit to its nodes.
            Project pair;
            pair.bpm = project.bpm;
            pair.tempo = project.tempo;
            pair.followTrackTempo = project.followTrackTempo;
            pair.clips = {*outgoing, *incoming};
            pair.transitionLinks = {*link};
            for (auto& side : pair.clips)
            {
                const auto* original = project.clipFor(side.id);
                side.controls = project.effectSettings(*original);
                side.controls.automation = {};
                const auto* effectOwner = project.effectOwner(*original);
                if (effectOwner && effectOwner != original)
                    side.effectOffset = project.automationTime({side.track, AutomationTarget::volume, {}, {}, side.id}, side.start);
            }
            auto& reference = pair.transitionLinks.front();
            reference.bands = {}; reference.filter = {}; reference.bassSwap.reset();
            for (int preset = 0; preset < 3 && !selected; ++preset)
            {
                Project expected = pair;
                TransitionStyle::Nodes shape;
                if (expected.applyTransitionPreset(clip->id,(TransitionPreset) preset).wasOk()
                    && capturePresetNodes(expected,clip->id,shape).wasOk())
                {
                    auto actual = *style.nodes, wanted = std::move(shape);
                    for (int side = 0; side < 2; ++side) for (int target = 0; target < trackAutomationCount; ++target)
                    {
                        const auto* source = owner.engine.project.clipFor(side == 0 ? link->outgoing : link->incoming);
                        const float manual = owner.engine.project.manualValue({source->track,(AutomationTarget) target,{},{},source->id});
                        for (auto* nodes : {&actual,&wanted})
                        {
                            auto& curve = nodes->curves[(size_t) side][(size_t) target];
                            if (curve.enabled && std::all_of(curve.points.begin(),curve.points.end(),[&](const auto& point) { return point.value == manual; })) curve = {};
                        }
                    }
                    if (actual == wanted) selected = preset + 1;
                }
            }
        }
    }
    picker->setSelectedId(selected, juce::dontSendNotification);
    if (!selected) picker->setText(link ? (link->presetName.isNotEmpty() ? link->presetName : "Custom / edited") : "No incoming transition", juce::dontSendNotification);
    if (link && owner.engine.project.transitionRange(*link).getLength() <= 1.e-6)
    {
        picker->setSelectedId(0, juce::dontSendNotification); picker->setText("No overlap", juce::dontSendNotification); picker->setEnabled(false);
        picker->setTooltip("This pair has no overlapping frame. Set a transition length in Transitions before applying a treatment. Track positions and sound are unchanged by this display.");
    }
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
        else if (choice >= 100 && choice - 100 < (int) choices.size())
        {
            const auto& preset = choices[(size_t)(choice-100)];
            safe->applySelectedStyle(preset.style,preset.name);
        }
    };
    return picker;
}
}
