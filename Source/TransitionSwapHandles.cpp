#include "Workstation.h"
#include <cmath>
#include <limits>

namespace mixworks
{
std::vector<Timeline::SwapHandle> Timeline::swapHandles() const
{
    std::vector<SwapHandle> result;
    const auto& project = owner.engine.project; const auto* link = project.transitionFor(owner.selectedTransition);
    if (!link) return result;
    for (int band = 0; band < 4; ++band)
    {
        if (!owner.transitionCurveVisible(band)) continue;
        const auto& setting = link->bands[(size_t) band];
        if (setting && setting->strength == 0) continue;
        const auto times = project.transitionSwapTimes(*link, band); if (!times) continue;
        for (size_t side = 0; side < 2; ++side)
        {
            const auto* clip = project.clipFor(side == 0 ? link->outgoing : link->incoming); if (!clip || laneSize(clip->track) < 30) continue;
            const float x = (float) ((*times)[side] * pixelsPerSecond), y = laneY(clip->track, 54.f + band * 14);
            const float height = std::min(12.f, std::max(4.f, 14.f * laneSize(clip->track) / laneHeight - 2));
            result.push_back({link->incoming, band, side == 1, !setting.has_value(), setting ? setting->mode : TransitionBandMode::swap,
                (*times)[side], {x - 10, y - height * .5f, 20, height}});
        }
    }
    return result;
}
std::optional<Timeline::SwapHandle> Timeline::swapHandleAt(juce::Point<float> point) const
{
    if (owner.busy || owner.pendingTempo > 0 || owner.tempoPreparationPending || owner.engine.project.hasPendingPlacement()) return {};
    std::optional<SwapHandle> result; float nearest = std::numeric_limits<float>::max();
    for (const auto& handle : swapHandles()) if (handle.bounds.expanded(2).contains(point))
    {
        const float distance = point.getDistanceFrom(handle.bounds.getCentre());
        if (distance < nearest) { nearest = distance; result = handle; }
    }
    return result;
}
void Timeline::paintSwapHandles(juce::Graphics& g)
{
    const std::array<juce::Colour, 4> colours {juce::Colour(0xffffad70), juce::Colour(0xffe4d15b), juce::Colour(0xff76c7ef), juce::Colour(0xffd3a3ef)};
    const char* names[] {"V", "B", "M", "H"};
    for (const auto& handle : swapHandles())
    {
        if (!handle.bounds.expanded(3).intersects(g.getClipBounds().toFloat())) continue;
        const auto* link = owner.engine.project.transitionFor(handle.incoming);
        const bool locked = link && (owner.engine.project.clipTransitionLocked(link->outgoing) || owner.engine.project.clipTransitionLocked(link->incoming));
        const auto ink = colours[(size_t) handle.band].withAlpha(locked ? .4f : 1.f);
        const float x = handle.bounds.getCentreX();
        const auto* clip = link ? owner.engine.project.clipFor(handle.incomingSide ? link->incoming : link->outgoing) : nullptr;
        if (clip) { g.setColour(ink.withMultipliedAlpha(.4f)); g.drawVerticalLine((int) std::round(x), laneY(clip->track, 48), laneY(clip->track, laneHeight - 30)); }
        g.setColour(colour::panel); g.fillRoundedRectangle(handle.bounds, 3);
        g.setColour(ink); g.drawRoundedRectangle(handle.bounds, 3, 1.2f);
        g.setFont(juce::FontOptions(9).withStyle("Bold")); g.drawText(names[handle.band], handle.bounds, juce::Justification::centred);
        const float y = handle.bounds.getCentreY();
        g.drawLine(x - 7, y, x - 5, y - 2); g.drawLine(x - 7, y, x - 5, y + 2);
        g.drawLine(x + 7, y, x + 5, y - 2); g.drawLine(x + 7, y, x + 5, y + 2);
    }
}
bool Timeline::beginSwapDrag(const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu()) return false;
    const auto hit = swapHandleAt(event.position); if (!hit) return false;
    const auto* link = owner.engine.project.transitionFor(hit->incoming); if (!link) return true;
    if (owner.engine.project.clipTransitionLocked(link->outgoing) || owner.engine.project.clipTransitionLocked(link->incoming))
    { owner.setStatus("Unlock both participating tracks before dragging their paired swap."); return true; }
    swapDrag = SwapGesture {*hit, event.position.x}; owner.selectTransition(hit->incoming, false);
    owner.setStatus("Drag the swap pair left/right; its gap stays paired. Alt bypasses snapping."); return true;
}
bool Timeline::continueSwapDrag(const juce::MouseEvent& event)
{
    if (!swapDrag) return false;
    if (owner.busy || !event.mouseWasDraggedSinceMouseDown()) return true;
    if (owner.pendingTempo > 0 || owner.tempoPreparationPending || owner.engine.project.hasPendingPlacement())
    { owner.setStatus("Wait for preparation before dragging a paired swap."); return true; }
    if (!moved) { owner.engine.flushMidiControls(); owner.engine.setAutomationRecordMode(AutomationRecordMode::read); }
    const juce::ScopedLock guard(owner.engine.lock);
    const auto& handle = swapDrag->handle; const auto* link = owner.engine.project.transitionFor(handle.incoming);
    if (!link || (link->bassSwap.has_value() && handle.band == 1) != handle.legacy
        || (!handle.legacy && (!link->bands[(size_t) handle.band] || link->bands[(size_t) handle.band]->mode != handle.mode)))
    { owner.setStatus("The swap treatment changed. Release and drag its new handle."); return true; }
    const auto before = owner.engine.project.transitionSwapTimes(*link, handle.band); if (!before) return true;
    const double target = owner.engine.project.snappedTime(handle.time + (event.position.x - swapDrag->x) / pixelsPerSecond,
        owner.engine.project.snap && !event.mods.isAltDown());
    auto candidate = owner.engine.project; const auto changed = candidate.setTransitionSwapTime(handle.incoming, handle.band, handle.incomingSide, target);
    if (changed.failed()) { owner.setStatus(changed.getErrorMessage()); return true; }
    const auto* next = candidate.transitionFor(handle.incoming); const auto after = next ? candidate.transitionSwapTimes(*next, handle.band) : std::nullopt;
    if (!after || std::abs((*after)[handle.incomingSide ? 1 : 0] - (*before)[handle.incomingSide ? 1 : 0]) < 1e-9) return true;
    if (!moved) { owner.remember(); moved = true; }
    owner.engine.project = std::move(candidate); repaint();
    owner.setStatus("Paired swap moved; both sides and their beat gap retained. Undo restores the gesture."); return true;
}
}
