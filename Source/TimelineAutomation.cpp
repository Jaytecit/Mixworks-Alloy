#include "Workstation.h"
#include "UiMessages.h"
#include "PresetNodes.h"
#include <cmath>
#include <limits>
#include <cstdlib>
#include <cstring>

namespace mixworks
{
namespace
{
template <typename Handle>
bool showCutAmount(const Handle&, const std::vector<Handle>&, int, int, const std::optional<Handle>&);
}
TimelineAccessItem::TimelineAccessItem(bool hasValue) : valueObject(hasValue)
{
    setInterceptsMouseClicks(false, false); setWantsKeyboardFocus(true);
    setHelpText("Enter selects this object. Existing editing, Copy/Cut, Undo and transition shortcuts apply to the selected owner. Selection does not seek or edit audio.");
}
void TimelineAccessItem::activate()
{
    const auto action = choose; // Selection refreshes callbacks; keep the executing closure alive.
    if (isEnabled() && action) action();
}
void TimelineAccessItem::invoke()
{
    if (!isEnabled()) return;
    const juce::Component::SafePointer<TimelineAccessItem> safe(this);
    activate(); if (!safe) return;
    grabKeyboardFocus(); if (!safe) return;
    const auto action = open;
    if (action) action();
}
void TimelineAccessItem::focusGained(FocusChangeType)
{ const juce::Component::SafePointer<TimelineAccessItem> safe(this); activate(); if (safe) repaint(); }
bool TimelineAccessItem::keyPressed(const juce::KeyPress& key)
{
    if (isTimelineContextKey(key) && context) { const auto action = context; action(); return true; }
    if (key.getKeyCode() != juce::KeyPress::returnKey || key.getModifiers().isAnyModifierKeyDown()) return false;
    invoke(); return true;
}
void TimelineAccessItem::paint(juce::Graphics& g)
{
    if (hasKeyboardFocus(false)) { g.setColour(colour::focus); g.drawRect(getLocalBounds().reduced(1), 2); }
}
std::unique_ptr<juce::AccessibilityHandler> TimelineAccessItem::createAccessibilityHandler()
{
    class Value final : public juce::AccessibilityValueInterface
    {
    public:
        explicit Value(TimelineAccessItem& c) : item(c) {}
        bool isReadOnly() const override { return true; }
        double getCurrentValue() const override { return item.value ? item.value() : 0; }
        juce::String getCurrentValueAsString() const override { return item.valueText ? item.valueText() : juce::String(); }
        void setValue(double) override {}
        void setValueAsString(const juce::String&) override {}
        AccessibleValueRange getRange() const override { return {}; }
    private:
        TimelineAccessItem& item;
    };
    class Handler final : public juce::AccessibilityHandler
    {
    public:
        explicit Handler(TimelineAccessItem& c) : AccessibilityHandler(c, juce::AccessibilityRole::listItem,
            juce::AccessibilityActions().addAction(juce::AccessibilityActionType::focus, [&c]
            { const juce::Component::SafePointer<TimelineAccessItem> safe(&c); if (c.isEnabled()) { c.activate(); if (safe) c.grabKeyboardFocus(); } })
                .addAction(juce::AccessibilityActionType::press, [&c] { c.invoke(); }),
            c.valueObject ? Interfaces{std::make_unique<Value>(c)} : Interfaces{}), item(c) {}
        juce::String getTitle() const override { return item.read ? item.read() : juce::String(); }
        juce::AccessibleState getCurrentState() const override
        {
            auto state = AccessibilityHandler::getCurrentState().withSelectable().withAccessibleOffscreen();
            return item.selected && item.selected() ? state.withSelected() : state;
        }
    private:
        TimelineAccessItem& item;
    };
    return std::make_unique<Handler>(*this);
}
std::unique_ptr<juce::AccessibilityHandler> Timeline::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler>(*this, juce::AccessibilityRole::list);
}
void Timeline::refreshAccessibleItems()
{
    juce::StringArray retained;
    int order = 1;
    auto itemFor = [&](const juce::String& key, juce::Rectangle<int> bounds, bool valueObject = false) -> TimelineAccessItem&
    {
        retained.add(key);
        const auto existing = std::find_if(accessibleItems.begin(), accessibleItems.end(), [&](const auto& c) { return c->getComponentID() == key; });
        TimelineAccessItem* pointer = existing == accessibleItems.end() ? nullptr : existing->get();
        if (!pointer)
        {
            auto item = std::make_unique<TimelineAccessItem>(valueObject); item->setComponentID(key); addAndMakeVisible(*item);
            pointer = item.get();
            accessibleItems.push_back(std::move(item));
        }
        auto& item = *pointer;
        item.setBounds(bounds); item.setVisible(!bounds.isEmpty()); item.setEnabled(!owner.busy && !owner.engine.exporting);
        item.setExplicitFocusOrder(order++);
        return item;
    };
    auto reveal = [this](const juce::String& key)
    {
        const auto found = std::find_if(accessibleItems.begin(), accessibleItems.end(), [&](const auto& c) { return c->getComponentID() == key; });
        if (found == accessibleItems.end()) return;
        const auto bounds = (*found)->getBounds(); const auto view = owner.viewport.getViewArea();
        if (view.contains(bounds)) return;
        owner.setFollowPlayhead(false);
        const int x = bounds.getX() < view.getX() || bounds.getWidth() > view.getWidth() ? bounds.getX()
            : bounds.getRight() > view.getRight() ? bounds.getRight() - view.getWidth() : view.getX();
        const int y = bounds.getY() < view.getY() || bounds.getHeight() > view.getHeight() ? bounds.getY()
            : bounds.getBottom() > view.getBottom() ? bounds.getBottom() - view.getHeight() : view.getY();
        owner.viewport.setViewPosition(std::max(0, x - 4), std::max(0, y - 4));
    };
    for (const auto& clip : owner.engine.project.clips)
    {
        const auto id = clip.id, key = "segment:" + id;
        auto& item = itemFor(key, clipBounds(clip).toNearestInt());
        item.setHelpText("Enter selects this segment without seeking. Shift+F10 opens its menu. Ctrl+T opens segment settings.");
        item.read = [this, id]
        {
            const auto* c = owner.engine.project.clipFor(id); if (!c) return juce::String("Removed segment");
            return "Segment " + juce::String((int) (c - owner.engine.project.clips.data()) + 1) + ", " + c->media->libraryTitle()
                + ", mixer lane " + juce::String(c->track + 1) + ", start " + timeText(c->start - owner.engine.project.tempo.origin)
                + ", length " + timeText(c->length) + ", BPM " + wholeBpmText(c->media->bpm()) + ", key " + owner.keyText(*c->media)
                + (owner.engine.project.clipTransitionLocked(id) ? ", locked" : ", unlocked");
        };
        item.selected = [this, id] { return owner.selectedClip == id || owner.editSelection.contains(id); };
        item.choose = [this, id, key, reveal]
        {
            if (owner.busy || owner.engine.exporting) return;
            const auto* c = owner.engine.project.clipFor(id); if (!c) return;
            const int track = c->track;
            selectedLabel.clear(); editingAutomationPoint.reset(); selectedCutAmount.reset(); selectedSection.reset(); selectedHotcue.reset(); hotcueGesture.reset();
            owner.correctionFromLibrary = false; owner.selectedAutomationTarget = -1; owner.selectedClip = id;
            owner.selectTrack(track); reveal(key);
        };
        item.context = [this, id, &item]
        {
            const auto anchor = item.getScreenBounds().withSizeKeepingCentre(1, 1);
            const auto* target = owner.engine.project.clipFor(id);
            if (!target) return;
            const int track = target->track;
            if (owner.selectedClip != id)
            {
                if (owner.editSelection.contains(id)) owner.editSelection.primary = id;
                else owner.clearEditSelection();
                owner.selectedClip = id;
                owner.selectTrack(track);
            }
            owner.showTimelineMenu(owner.engine.position.load(), track, anchor);
        };
    }
    for (const auto& link : owner.engine.project.transitionLinks)
    {
        const auto id = link.incoming, key = "transition:" + id;
        auto& item = itemFor(key, transitionBounds(link).toNearestInt());
        item.setHelpText("Enter selects this pair without seeking. Shift+F10 opens its transition menu. Alt+Left/Right navigates between pairs.");
        item.read = [this, id]
        {
            const auto* pair = owner.engine.project.transitionFor(id); if (!pair) return juce::String("Removed transition");
            const auto* out = owner.engine.project.clipFor(pair->outgoing); const auto* in = owner.engine.project.clipFor(pair->incoming);
            if (!out || !in) return juce::String("Unavailable transition");
            const auto range = owner.engine.project.transitionRange(*pair);
            return "Transition, " + out->media->libraryTitle() + " to " + in->media->libraryTitle()
                + ", start " + timeText(range.getStart() - owner.engine.project.tempo.origin) + ", end " + timeText(range.getEnd() - owner.engine.project.tempo.origin)
                + (pair->locked ? ", locked" : ", unlocked");
        };
        item.selected = [this, id] { return owner.selectedTransition == id; };
        item.choose = [this, id, key, reveal]
        {
            if (owner.busy || owner.engine.exporting || !owner.engine.project.transitionFor(id)) return;
            selectedLabel.clear(); editingAutomationPoint.reset(); selectedCutAmount.reset(); selectedSection.reset(); selectedHotcue.reset(); hotcueGesture.reset(); owner.correctionFromLibrary = false;
            owner.selectTransition(id, false); reveal(key);
        };
        item.context = [this, id, &item] { owner.showTransitionMenu(id, item.getScreenBounds().withSizeKeepingCentre(1, 1)); };
    }
    auto locate = [this](const AutomationAddress& address, double time, int duplicate)
    {
        const auto view = automationViewCurve(address);
        const auto& points = view.points;
        auto point = std::lower_bound(points.begin(), points.end(), time, [](const auto& p, double t) { return p.time < t; });
        while (point != points.end() && point->time == time && duplicate-- > 0) ++point;
        return point != points.end() && point->time == time ? (int) (point - points.begin()) : -1;
    };
    setDescription("Timeline musical objects. Use Editing Previous/Next to navigate every automation node.");
    for (int track = 0; track < trackCount; ++track)
        for (const auto& address : visibleAutomationAddresses(track)) if (laneSize(track) >= 55 && automationNodesVisible(address))
        {
            juce::Array<juce::var> identity {address.track, (int) address.target, address.slotId, address.parameterId, address.clipId};
            if (address.stem >= 0) identity.add(address.stem);
            const auto prefix = "node:" + juce::JSON::toString(juce::var(identity), true) + ":";
            const auto curve = automationViewCurve(address);
            // Keep native children bounded for long recorded takes. Prioritise the
            // selected node, then visible nodes; Editing navigation reaches all others.
            std::vector<int> admitted;
            auto admit = [&](int i)
            { if (i >= 0 && i < (int) curve.points.size() && admitted.size() < 256 && std::find(admitted.begin(), admitted.end(), i) == admitted.end()) admitted.push_back(i); };
            if (editingAutomationPoint && editingAutomationPoint->first == address)
                for (int step : {0, -1, 1}) admit(editingAutomationPoint->second + step);
            const auto visible = owner.viewport.getViewArea();
            for (int i = 0; i < (int) curve.points.size() && admitted.size() < 256; ++i)
            {
                const double x = owner.engine.project.automationSessionTime(address, curve.points[(size_t) i].time) * pixelsPerSecond;
                if (x >= visible.getX() - 8 && x <= visible.getRight() + 8) admit(i);
            }
            for (int i = 0; i < (int) curve.points.size() && admitted.size() < 256; ++i) admit(i);
            std::sort(admitted.begin(), admitted.end());
            if (admitted.size() < curve.points.size()) setDescription("Many automation nodes. Use Editing Previous/Next to reach every node; native navigation includes the selected node and nearby visible points.");
            int duplicate = 0;
            for (int index = 0; index < (int) curve.points.size(); ++index)
            {
                const auto& point = curve.points[(size_t) index];
                duplicate = index > 0 && point.time == curve.points[(size_t) index - 1].time ? duplicate + 1 : 0;
                if (!std::binary_search(admitted.begin(), admitted.end(), index)) continue;
                const double storedTime = point.time;
                juce::int64 timeBits = 0; std::memcpy(&timeBits, &storedTime, sizeof(timeBits));
                const auto key = prefix + juce::String::toHexString(timeBits) + ":" + juce::String(duplicate);
                const double time = owner.engine.project.automationSessionTime(address, storedTime);
                const auto display = point.value;
                auto& item = itemFor(key, {(int) std::round(time * pixelsPerSecond) - 8, (int) std::round(automationY(track, display, (int) address.target)) - 8, 16, 16}, true);
                item.setHelpText("Focus selects this automation node in Editing without changing it. Enter opens the existing guarded time/value editor. Delete removes this node; Undo restores it. Native value is read-only; use the editor to change it.");
                item.read = [this, address, storedTime, duplicate, locate]
                {
                    const int i = locate(address, storedTime, duplicate); if (i < 0) return juce::String("Removed automation node");
                    const auto* clip = owner.engine.project.clipFor(address.clipId);
                    const auto scope = clip ? "Segment " + juce::String((int) (clip - owner.engine.project.clips.data()) + 1) + " / " + clip->media->libraryTitle()
                        : address.target == AutomationTarget::master ? juce::String("Master") : "Mixer lane " + juce::String(address.track + 1);
                    return scope + (address.stem >= 0 ? " / " + juce::String(stemNames[address.stem]) : juce::String()) + " / " + automationName(address.target) + " node, project time "
                        + timeText(owner.engine.project.automationSessionTime(address, storedTime) - owner.engine.project.tempo.origin)
                        + ", source/curve time " + juce::String(storedTime, 6) + (owner.engine.project.automationLocked(address) ? ", locked" : "");
                };
                item.value = [this, address, storedTime, duplicate, locate]
                {
                    const int i = locate(address, storedTime, duplicate); if (i < 0) return 0.;
                    return (double) automationViewCurve(address).points[(size_t) i].value;
                };
                item.valueText = [this, address, value = item.value] { return owner.automationText(address.target, (float) value()); };
                item.selected = [this, address, storedTime, duplicate, locate]
                { return editingAutomationPoint && editingAutomationPoint->first == address && editingAutomationPoint->second == locate(address, storedTime, duplicate); };
                item.choose = [this, address, storedTime, duplicate, key, reveal, locate]
                {
                    if (owner.busy || owner.engine.exporting || !automationNodesVisible(address)) return;
                    const int i = locate(address, storedTime, duplicate); if (i < 0) return;
                    selectedLabel.clear(); owner.correctionFromLibrary = false;
                    owner.selectedStem = address.stem;
    owner.automationClip = address.clipId;
    if (address.clipId.isNotEmpty()) owner.selectedClip = address.clipId;
    owner.selectedAutomationTarget = (int) address.target;
                    if (address.target == AutomationTarget::plugin) owner.pluginLane = address;
                    owner.selectTrack(address.target == AutomationTarget::master ? 0 : address.track);
                    selectAutomationPoint(address, i); owner.refresh(); reveal(key);
                };
                item.context = [this, address, storedTime, duplicate, locate, &item]
                {
                    const int i = locate(address, storedTime, duplicate);
                    if (i >= 0) owner.showAutomationNodeMenu(address, i, owner.engine.project.automationSessionTime(address, storedTime), item.getScreenBounds());
                };
                item.open = [this, address, storedTime, duplicate, locate]
                {
                    const int track = address.target == AutomationTarget::master ? 0 : address.track;
                    const int i = locate(address, storedTime, duplicate);
                    if (i >= 0 && owner.displayedAddress(track) == address && owner.automationEditable(address)) owner.editAutomationPoint(track, i);
                };
            }
        }
    const auto handles = cutAmountHandles(true);
    const auto paintedAmounts = cutAmountHandles();
    const auto amountKey = [](const CutAmountHandle& h)
    {
        juce::Array<juce::var> identity {h.address.track, (int) h.address.target, h.address.slotId, h.address.parameterId, h.address.clipId, h.incoming, h.before, h.legacy};
        juce::int64 bits = 0; std::memcpy(&bits, &h.time, sizeof(bits));
        return "amount:" + juce::JSON::toString(juce::var(identity), true) + ":" + juce::String::toHexString(bits);
    };
    const auto locateAmount = [this, amountKey](const juce::String& key) -> std::optional<CutAmountHandle>
    {
        for (const auto& h : cutAmountHandles(true)) if (amountKey(h) == key) return h;
        return {};
    };
    std::vector<size_t> admittedAmounts;
    auto admitAmount = [&](size_t index)
    {
        const auto& h = handles[index];
        if (admittedAmounts.size() < 256 && std::find(admittedAmounts.begin(), admittedAmounts.end(), index) == admittedAmounts.end()
            && showCutAmount(h, h.time * pixelsPerSecond >= owner.viewport.getViewPositionX()
                && h.time * pixelsPerSecond <= owner.viewport.getViewPositionX() + owner.viewport.getViewWidth() ? paintedAmounts : handles,
                laneSize(h.address.target == AutomationTarget::master ? 0 : h.address.track), owner.automationLane(), selectedCutAmount)) admittedAmounts.push_back(index);
    };
    if (selectedCutAmount) for (size_t i = 0; i < handles.size(); ++i) if (amountKey(handles[i]) == amountKey(*selectedCutAmount)) admitAmount(i);
    const auto amountView = owner.viewport.getViewArea();
    for (size_t i = 0; i < handles.size() && admittedAmounts.size() < 256; ++i)
        if (handles[i].time * pixelsPerSecond >= amountView.getX() && handles[i].time * pixelsPerSecond <= amountView.getRight()) admitAmount(i);
    for (size_t i = 0; i < handles.size() && admittedAmounts.size() < 256; ++i) admitAmount(i);
    std::sort(admittedAmounts.begin(), admittedAmounts.end());
    for (const auto index : admittedAmounts)
    {
        const auto& h = handles[index]; const auto key = amountKey(h);
        auto& item = itemFor(key, h.bounds.toNearestInt(), true);
        item.setHelpText("Hard-cut amount. Focus selects without editing or seeking. Enter opens the existing fixed-time amount editor. Up/Down changes the selected amount; D/Shift+D reaches every visible-time handle, including dense curves. Native values are read-only. Transition locks still apply.");
        item.read = [this, key, locateAmount]
        {
            const auto current = locateAmount(key); if (!current) return juce::String("Removed cut amount");
            const auto* clip = owner.engine.project.clipFor(current->address.clipId);
            const auto scope = clip ? "Segment " + juce::String((int) (clip - owner.engine.project.clips.data()) + 1) + " / " + clip->media->libraryTitle()
                : current->address.target == AutomationTarget::master ? juce::String("Master") : "Mixer lane " + juce::String(current->address.track + 1);
            return scope + " / " + cutAmountText(*current) + ", project time " + timeText(current->time - owner.engine.project.tempo.origin)
                + (owner.engine.project.automationLocked(current->address) ? ", locked" : "");
        };
        item.value = [this, key, locateAmount]
        {
            const auto current = locateAmount(key); if (!current) return 0.;
            return automationUsesGain(current->address.target) ? (1. - current->value) * 100
                : current->address.target == AutomationTarget::cutoff ? (double) current->value : current->value * 100.;
        };
        item.valueText = [this, key, locateAmount] { const auto current = locateAmount(key); return current ? cutAmountText(*current) : juce::String(); };
        item.selected = [this, key, amountKey] { return selectedCutAmount && amountKey(*selectedCutAmount) == key; };
        item.choose = [this, key, locateAmount]
        {
            if (owner.busy || owner.engine.exporting) return;
            const auto current = locateAmount(key); if (!current) return;
            // Keep the source cut and its offset widget in view. Native focus
            // scrolling can reveal the widget while pushing its time offscreen.
            selectedCutAmount = *current; editingAutomationPoint.reset(); selectedLabel.clear(); selectedSection.reset(); selectedHotcue.reset(); hotcueGesture.reset();
            const auto view = owner.viewport.getViewArea(); const int x = Clip::gridPixel(current->time, pixelsPerSecond);
            if (x < view.getX() + 100 || x > view.getRight() - 100)
            { owner.setFollowPlayhead(false); owner.viewport.setViewPosition(std::max(0, x - view.getWidth() / 2), view.getY()); }
            owner.setStatus(cutAmountText(*current) + "");
            refreshAccessibleItems(); repaint();
        };
        item.open = [this, key, locateAmount]
        { const auto current = locateAmount(key); if (current && owner.automationEditable(current->address)) editCutAmount(*current); };
    }
    refreshAccessibleSections(itemFor);
    refreshAccessibleHotcues(itemFor);
    for (const auto& item : accessibleItems)
        if (!retained.contains(item->getComponentID()) && item->hasKeyboardFocus(true)) grabKeyboardFocus();
    accessibleItems.erase(std::remove_if(accessibleItems.begin(), accessibleItems.end(), [&](const auto& item) { return !retained.contains(item->getComponentID()); }), accessibleItems.end());
}
namespace
{
constexpr std::array<const char*, 6> curveNames {"Volume", "Bass", "Mid", "Treble", "Filter", "All"};
constexpr std::array<AutomationTarget, 5> curveTargets {AutomationTarget::volume, AutomationTarget::bass,
    AutomationTarget::mid, AutomationTarget::treble, AutomationTarget::cutoff};
int pairedNodeAt(const AutomationCurve& curve,double time)
{
    int result = -1; double distance = 1.e-7;
    for (int i = 0; i < (int) curve.points.size(); ++i)
        if (const double delta = std::abs(curve.points[(size_t) i].time-time); delta < distance)
        { result = i; distance = delta; if (delta == 0) break; }
    return result;
}
template <typename Handle>
bool showCutAmount(const Handle& handle, const std::vector<Handle>& handles, int height, int requested, const std::optional<Handle>& selected)
{
    if (height >= 96) return true;
    // Short lanes show one parameter at a time, like their automation nodes.
    // D can still cycle every handle; selected paired amounts appear on both sides.
    if (selected && std::any_of(handles.begin(), handles.end(), [&](const auto& h) { return h.sameOwner(*selected); })) requested = (int) selected->address.target;
    const auto first = std::find_if(handles.begin(), handles.end(), [&](const auto& h) { return h.address.track == handle.address.track; });
    const bool present = std::any_of(handles.begin(), handles.end(), [&](const auto& h) { return h.address.track == handle.address.track && (int) h.address.target == requested; });
    return (int) handle.address.target == (present ? requested : first != handles.end() ? (int) first->address.target : -1);
}
}

int Timeline::curveToolsReserve(int track) const
{
    // Stable space in every normal-height lane: selection never changes waveform geometry.
    return laneSize(track) >= 96 ? 28 : 0;
}

juce::Rectangle<int> Timeline::clipFooterBounds(const Clip& clip) const
{
    if (curveToolsReserve(clip.track) == 0) return {};
    const auto view = owner.viewport.getViewArea();
    const auto wave = clipBounds(clip);
    const int left = std::max(view.getX(), (int) std::ceil(wave.getX())) + 1;
    const int right = std::min(view.getRight(), (int) std::floor(wave.getRight())) - 1;
    const int y = laneTop(clip.track + 1) - curveToolsReserve(clip.track) - 3;
    const juce::Rectangle<int> area {left, y, std::min(432, right - left), 24};
    // Never expand beyond a short clip or move vertically over its waveform/another lane.
    return area.getWidth() >= 70 && view.contains(area) ? area : juce::Rectangle<int>();
}

juce::Rectangle<int> Timeline::curveToolsBounds() const
{
    const auto* clip = owner.currentClip(); if (!clip) return {};
    auto bounds = clipFooterBounds(*clip);
    if (bounds.getWidth() < 122) return {};
    return bounds.withTrimmedLeft(52);
}

void Timeline::initialiseCurveTools()
{
    curveTools = std::make_unique<ClipCurveTools>();
    addChildComponent(*curveTools);
    curveTools->setName("Segment automation tools");
    curveTools->setInterceptsMouseClicks(false, true);
    const std::array<const char*, 5> labels {"VOL", "BAS", "MID", "TRE", "FLT"};
    for (size_t i = 0; i < curveTools->buttons.size(); ++i)
    {
        auto& button = curveTools->buttons[i];
        curveTools->addAndMakeVisible(button); button.setButtonText(labels[i]);
        button.setTitle(juce::String(curveNames[i]) + " curve");
        button.setWantsKeyboardFocus(true);
        button.ink = i < 4 ? std::array<juce::Colour, 4> {juce::Colour(0xffffad70), juce::Colour(0xffe4d15b), juce::Colour(0xff76c7ef), juce::Colour(0xffd3a3ef)}[i]
            : juce::Colour::fromHSV(.08f + (int) AutomationTarget::cutoff * .115f, .65f, .95f, 1.f);
        button.onClick = [this, i] { chooseCurveTool((int) i); };
    }
    curveTools->addAndMakeVisible(curveTools->fx); curveTools->fx.setWantsKeyboardFocus(true);
    curveTools->fx.setTooltip("Choose a VST3 automation parameter for this segment. This uses the existing effect-control editor.");
    curveTools->fx.onClick = [this] { chooseCurveTool(5); };
    for (auto* button : {&curveTools->hotcues, &curveTools->sections})
    { curveTools->addAndMakeVisible(*button); button->setWantsKeyboardFocus(true); }
    curveTools->hotcues.setTitle("Show hotcue labels");
    curveTools->sections.setTitle("Show section labels");
    curveTools->hotcues.setTooltip("Show or hide this segment's hotcue labels. Cue positions and playback are unchanged.");
    curveTools->sections.setTooltip("Show or hide this segment's section labels. Section positions and playback are unchanged.");
    curveTools->hotcues.onClick = [this] { chooseCurveTool(7); };
    curveTools->sections.onClick = [this] { chooseCurveTool(8); };
    curveTools->addAndMakeVisible(curveTools->saveDownbeat); curveTools->saveDownbeat.setWantsKeyboardFocus(true);
    curveTools->saveDownbeat.onClick = [this] { if (const auto* clip = owner.currentClip()) owner.editSavedDownbeat(clip->id); };
    curveTools->addChildComponent(curveTools->selector); curveTools->selector.setTextWhenNothingSelected("Curve...");
    curveTools->selector.setTooltip("Select this segment's automation curve. Filled dots on the expanded buttons indicate existing automation.");
    curveTools->selector.onChange = [this] { if (curveTools->selector.getSelectedId() > 0) chooseCurveTool(curveTools->selector.getSelectedId() - 1); };
}

void Timeline::refreshCurveTools()
{
    const auto bounds = curveToolsBounds();
    curveTools->setVisible(!bounds.isEmpty()); if (bounds.isEmpty()) return;
    curveTools->setBounds(bounds);
    const auto* clip = owner.currentClip(); if (!clip) return;
    const bool available = !owner.busy && !owner.engine.exporting;
    auto row = curveTools->getLocalBounds().reduced(0, 2);
    constexpr int buttonWidth = 42;
    const bool showSave = row.getWidth() >= 112;
    curveTools->saveDownbeat.setVisible(showSave);
    if (showSave) curveTools->saveDownbeat.setBounds(row.removeFromRight(buttonWidth).withTrimmedRight(2));
    const bool showMarkers = row.getWidth() >= 154;
    curveTools->hotcues.setVisible(showMarkers); curveTools->sections.setVisible(showMarkers);
    if (showMarkers)
    {
        curveTools->sections.setBounds(row.removeFromRight(buttonWidth).withTrimmedRight(2));
        curveTools->hotcues.setBounds(row.removeFromRight(buttonWidth).withTrimmedRight(2));
    }
    curveTools->hotcues.setEnabled(available); curveTools->sections.setEnabled(available);
    const bool hotcuesShown = visibleHotcueClips.count(clip->id) != 0;
    const bool sectionsShown = owner.showEnergySections && visibleSectionClips.count(clip->id) != 0;
    curveTools->hotcues.setToggleState(hotcuesShown, juce::dontSendNotification);
    curveTools->sections.setToggleState(sectionsShown, juce::dontSendNotification);
    const bool compact = row.getWidth() < 6 * buttonWidth;
    const auto selectorBounds = row;
    curveTools->saveDownbeat.setTitle("Save downbeat to track / " + clip->media->libraryTitle());
    curveTools->saveDownbeat.setTooltip("Save beat one from an exact time, the playhead or IN marker. Beatmatch applies the correction to the mix.");
    curveTools->saveDownbeat.setEnabled(available && !clip->media->sourceDetailsPending && clip->media->hasPlayback()
        && owner.pendingTempo <= 0 && !owner.tempoPreparationPending && !owner.engine.project.clipTransitionLocked(clip->id)
        && owner.engine.automationRecordMode == AutomationRecordMode::read && !owner.engine.recordingAutomation());
    const int width = buttonWidth;
    curveTools->selector.clear(juce::dontSendNotification);
    int selected = 0;
    for (size_t i = 0; i < curveTools->buttons.size(); ++i)
    {
        auto& button = curveTools->buttons[i];
        const AutomationAddress address {clip->track, curveTargets[i], {}, {}, clip->id};
        const auto* curve = owner.engine.project.curve(address);
        button.hasCurve = hasGeneratedAutomation(address) || (curve && !curve->points.empty());
        const bool active = owner.automationLane() == (int) address.target && owner.displayedAddress(clip->track) == address;
        if (active) selected = (int) i + 1;
        button.setToggleState(active, juce::dontSendNotification);
        button.setTooltip(juce::String(curveNames[i]) + (button.hasCurve ? " / existing automation" : " / no saved nodes")
            + ". Edit this track's automation across its full span, including transition preset nodes. Filled dot: automation exists."
            + (owner.engine.project.automationLocked(address) ? " Locked: viewing is available; unlock to edit." : ""));
        button.setEnabled(available); button.setVisible(!compact);
        button.setBounds(row.removeFromLeft(std::max(0, width)).withTrimmedRight(2));
        curveTools->selector.addItem(juce::String(curveNames[i]) + (button.hasCurve ? " (curve)" : ""), (int) i + 1);
    }
    curveTools->selector.addItem("FX parameter...", 6);
    curveTools->selector.addItem("Save downbeat to track...",7);
    curveTools->selector.addItem(hotcuesShown ? "Hide hotcue labels" : "Show hotcue labels",8);
    curveTools->selector.addItem(sectionsShown ? "Hide section labels" : "Show section labels",9);
    curveTools->fx.setVisible(!compact); curveTools->fx.setEnabled(available); curveTools->fx.setBounds(row.removeFromLeft(buttonWidth).withTrimmedRight(2));
    curveTools->selector.setVisible(compact); curveTools->selector.setEnabled(available);
    curveTools->selector.setBounds(selectorBounds);
    if (owner.automationLane() == (int) AutomationTarget::plugin && owner.pluginLane.clipId == clip->id) selected = 6;
    curveTools->selector.setSelectedId(selected, juce::dontSendNotification);
    curveTools->repaint();
}

void Timeline::chooseCurveTool(int index)
{
    const auto* clip = owner.currentClip();
    if (!clip || owner.busy || owner.engine.exporting || index < 0 || index > 8) return;
    const auto id = clip->id; const int track = clip->track;
    if (index == 7 || index == 8)
    {
        auto& visible = index == 7 ? visibleHotcueClips : visibleSectionClips;
        const bool shown = visible.count(id) && (index == 7 || owner.showEnergySections);
        if (shown) visible.erase(id); else visible.insert(id);
        if (index == 8 && !shown) owner.showEnergySections = true;
        if (index == 7 && shown) { selectedHotcue.reset(); hotcueGesture.reset(); }
        if (index == 8 && shown) { finishSectionGesture(); selectedSection.reset(); }
        refreshCurveTools(); refreshAccessibleItems(); repaint(); return;
    }
    if (index == 6) { owner.editSavedDownbeat(id); refreshCurveTools(); return; }
    if (index == 5) { owner.selectTrack(track); owner.chooseAutomationPlugin(); return; }
    const AutomationAddress address {track, curveTargets[(size_t) index], {}, {}, id};
    const auto title = clip->media->libraryTitle();
    // Reveal the requested curve even when the inspector previously hid or isolated another band.
    owner.transitionCurveOnlyBand = -1;
    if (index < 4) owner.transitionCurveShown[(size_t) index] = true;
    selectAutomationCurve(address);
    owner.setStatus(juce::String(curveNames[(size_t) index]) + " automation / " + title);
}

void Timeline::selectAutomationCurve(const AutomationAddress& address)
{
    owner.showEditingPage(2);
    owner.clearEditSelection();
    selectedSection.reset(); selectedHotcue.reset(); hotcueGesture.reset();
    selectedCutAmount.reset();
    editingAutomationPoint.reset();
    owner.selectedStem = address.stem;
    owner.automationClip = address.clipId;
    if (address.clipId.isNotEmpty()) owner.selectedClip = address.clipId;
    owner.selectedAutomationTarget = (int) address.target;
    if (address.target == AutomationTarget::plugin) owner.pluginLane = address;
    owner.selectTrack(address.target == AutomationTarget::master ? 0 : address.track);
    refreshCurveTools();
}

bool Timeline::automationNodesVisible(const AutomationAddress& address) const
{
    const int track = address.target == AutomationTarget::master ? 0 : address.track;
    return automationVisible(address) && owner.automationLane() == (int) address.target
        && owner.engine.selectedTrack == track && owner.displayedAddress(track) == address;
}

std::vector<AutomationAddress> Timeline::visibleAutomationAddresses(int track) const
{
    auto addresses = owner.automationAddresses(track);
    for (const auto& clip : owner.engine.project.clips) if (clip.track == track)
        for (int band = 0; band < 4; ++band)
        {
            const AutomationAddress address {track, (AutomationTarget) band, {}, {}, clip.id};
            if (hasGeneratedAutomation(address) && std::find(addresses.begin(), addresses.end(), address) == addresses.end()) addresses.push_back(address);
        }
    if (owner.automationLane() >= 0 && owner.engine.selectedTrack == track)
    {
        const auto selected = owner.displayedAddress(track);
        if (std::find(addresses.begin(), addresses.end(), selected) == addresses.end()) addresses.push_back(selected);
    }
    // One visible line per original track, even when another split piece is selected.
    for (size_t i = 0; i < addresses.size(); ++i) if (addresses[i].stem<0 && addresses[i].clipId.isNotEmpty() && !hasGeneratedAutomation(addresses[i]))
        for (size_t j = i + 1; j < addresses.size();)
        {
            const auto* a = owner.engine.project.clipFor(addresses[i].clipId);
            const auto* b = owner.engine.project.clipFor(addresses[j].clipId);
            if (a && b && a->effects == b->effects && addresses[i].target==addresses[j].target && addresses[j].stem<0
                && !hasGeneratedAutomation(addresses[j]) && addresses[i].slotId == addresses[j].slotId && addresses[i].parameterId == addresses[j].parameterId)
            {
                if (addresses[j] == owner.displayedAddress(track)) addresses[i] = addresses[j];
                addresses.erase(addresses.begin() + (ptrdiff_t)j);
            }
            else ++j;
        }
    return addresses;
}

AutomationCurve Timeline::automationViewCurve(const AutomationAddress& address) const
{
    if (hasGeneratedAutomation(address))
    {
        AutomationCurve composed;
        if (editablePresetNodes(owner.engine.project,address,composed).wasOk()) return composed;
        return {}; // Failed legacy conversion is reported by the edit owner; never expose incorrectly indexed nodes.
    }
    const auto* saved = owner.engine.project.curve(address);
    return saved ? *saved : AutomationCurve {};
}

juce::Path Timeline::automationPath(const AutomationAddress& address, double begin, double end) const
{
    juce::Path line;
    const auto& p = owner.engine.project; const auto* curve = p.curve(address);
    const auto view = (!curve || address.target == AutomationTarget::cutoff && hasGeneratedAutomation(address)) ? automationViewCurve(address) : AutomationCurve {};
    if (address.target == AutomationTarget::cutoff && hasGeneratedAutomation(address)) curve = &view;
    if (!curve && owner.automationPluginParameter(address)) curve = &view;
    if (!curve || end <= begin) return line;
    const int track = address.target == AutomationTarget::master ? 0 : address.track, target = (int) address.target;
    const auto* clip = p.clipFor(address.clipId);
    const auto playback = clip ? p.transitionPlaybackFor(*clip) : TransitionPlayback {};
    const bool combined = hasGeneratedAutomation(address);
    const auto* parameter = owner.automationPluginParameter(address);
    const float manual = parameter && !p.curve(address) ? parameter->getValue() : p.manualValue(address);
    auto valueAt = [&](double time) { return combined ? curve->valueAt(p.automationTime(address, time), manual) : curve->interpolatedValue(p.automationTime(address, time), manual); };
    auto y = [&](double time, float value) { return automationY(track, automationDisplayValue(address, time, value, &playback), target); };
    line.startNewSubPath((float) (begin * pixelsPerSecond), y(begin, valueAt(begin)));
    if (combined || automationUsesGain(address.target) || std::any_of(curve->points.begin(), curve->points.end(), [](const auto& point) { return point.bend != 0; }))
    {
        double previous = begin;
        auto append = [&](double time)
        {
            if (combined) for (const auto& pair : playback.pairs)
            {
                const auto& envelope = pair[(size_t) target];
                const double jump = envelope.instant ? envelope.begin : envelope.end;
                if ((envelope.instant || envelope.bend >= 1) && jump > previous && jump <= time)
                {
                    const double before = std::nextafter(jump, -std::numeric_limits<double>::infinity());
                    const float x = (float) (jump * pixelsPerSecond);
                    line.lineTo(x, y(before, valueAt(before))); line.lineTo(x, y(jump, valueAt(jump)));
                }
            }
            for (size_t i = 1; i < curve->points.size(); ++i) if (curve->points[i - 1].bend >= 1 || (curve->points[i-1].shape && curve->points[i-1].shape->endpointStep))
            {
                const double jump = p.automationSessionTime(address, curve->points[i].time);
                if (jump > previous && jump <= time)
                {
                    const float x = (float) (jump * pixelsPerSecond);
                    line.lineTo(x, y(jump, curve->points[i - 1].value)); line.lineTo(x, y(jump, curve->points[i].value));
                }
            }
            line.lineTo((float) (time * pixelsPerSecond), y(time, valueAt(time))); previous = time;
        };
        for (int x = (int) std::ceil(begin * pixelsPerSecond); x < end * pixelsPerSecond; ++x) append(x / pixelsPerSecond);
        append(end);
    }
    else for (const auto& point : curve->points)
    {
        const double time = p.automationSessionTime(address, point.time);
        if (time >= begin && time <= end) line.lineTo((float) (time * pixelsPerSecond), y(time, point.value));
    }
    line.lineTo((float) (end * pixelsPerSecond), y(end, valueAt(end))); return line;
}

void Timeline::beginCurveBend(const AutomationAddress& address, juce::Point<float> position)
{
    const auto& project = owner.engine.project;
    if (project.automationLocked(address)) { owner.setStatus("Unlock the transition before bending its curve."); return; }
    const auto preview = automationViewCurve(address); const auto* curve = &preview; if (curve->points.empty()) return;
    const double time = project.automationTime(address, position.x / pixelsPerSecond);
    const auto next = std::upper_bound(curve->points.begin(), curve->points.end(), time, [](double t, const auto& point) { return t < point.time; });
    if (next != curve->points.begin() && next != curve->points.end() && next->value != std::prev(next)->value)
    {
        const auto previous = std::prev(next);
        curveBendDrag = CurveBendGesture {address, (int) (previous - curve->points.begin()), {}, 0, previous->bend, position.y, next->value > previous->value};
    }
    else if (const auto* clip = project.clipFor(address.clipId); clip && (int) address.target < 4)
    {
        const auto playback = project.transitionPlaybackFor(*clip); const int band = (int) address.target;
        for (const auto& link : project.transitionLinks)
        {
            const int side = link.incoming == clip->id ? 0 : link.outgoing == clip->id ? 1 : -1;
            if (side < 0 || !link.bands[(size_t) band]) continue;
            const auto& envelope = playback.pairs[(size_t) side][(size_t) band];
            const double session = position.x / pixelsPerSecond;
            if (envelope.strength > 0 && !envelope.instant && session >= envelope.begin && session < envelope.end)
                curveBendDrag = CurveBendGesture {address, -1, link.incoming, side, envelope.bend, position.y, side == 0};
        }
    }
    if (curveBendDrag) owner.setStatus("Drag up/down to bend this segment; its end nodes stay fixed. Pull a rising curve down fully for a snap at the second node.");
}

bool Timeline::continueCurveBend(const juce::MouseEvent& event)
{
    if (!curveBendDrag) return false;
    if (owner.busy || owner.engine.exporting || !event.mouseWasDraggedSinceMouseDown()) return true;
    const auto gesture = *curveBendDrag;
    if (!owner.automationEditable(gesture.address)) return true;
    const double bend = juce::jlimit(-1., 1., gesture.bend + (event.position.y - gesture.y) / (gesture.rising ? 80. : -80.));
    auto candidate = owner.automationEditSnapshot();
    if (gesture.segment >= 0)
    {
        auto* curve = owner.materialiseAutomation(candidate,gesture.address);
        if (!curve || gesture.segment + 1 >= (int) curve->points.size() || curve->points[(size_t) gesture.segment].bend == bend) return true;
        curve->enabled = true; curve->points[(size_t) gesture.segment].bend = bend; candidate.automationEdited(gesture.address);
    }
    else
    {
        const auto* found = candidate.transitionFor(gesture.incoming); if (!found) return true;
        auto& link = candidate.transitionLinks[(size_t) (found - candidate.transitionLinks.data())];
        auto& setting = link.bands[(size_t) gesture.address.target]; if (!setting || setting->bend[(size_t) gesture.side] == bend) return true;
        setting->bend[(size_t) gesture.side] = bend;
        const auto valid = candidate.validateTransitionBands(link); if (valid.failed()) { owner.setStatus(valid.getErrorMessage()); return true; }
    }
    const auto protection = owner.engine.project.validateTransitionLocks(candidate);
    if (protection.failed()) { owner.setStatus(protection.getErrorMessage()); return true; }
    if (!owner.publishAutomationEdit(std::move(candidate),gesture.address,!moved)) return true;
    moved=true;
    owner.setStatus(bend >= 1 ? "Hard cut / snap at the second node" : "Curve bend " + juce::String(bend, 2) + " / end nodes fixed");
    repaint(); return true;
}

std::optional<AutomationAddress> Timeline::automationCurveAt(juce::Point<float> point) const
{
    if (point.y < 0 || point.y >= laneTop(contentLaneCount()) || transitionAt(point)) return {};
    const int track = trackAt(point.y); if (laneSize(track) < 30) return {};
    auto addresses = visibleAutomationAddresses(track);
    const auto& p = owner.engine.project;
    float nearest = 6; std::optional<AutomationAddress> result;
    for (const auto& address : addresses)
    {
        if (!automationVisible(address)) continue;
        if (!p.curve(address) && !owner.automationPluginParameter(address)) continue;
        const auto bounds = p.automationBounds(address);
        double begin = std::max(bounds.getStart(), (point.x - 8) / pixelsPerSecond);
        double end = std::min(bounds.getEnd(), (point.x + 8) / pixelsPerSecond);
        const auto path = automationPath(address, begin, end); if (path.isEmpty()) continue;
        juce::Point<float> onLine; path.getNearestPoint(point, onLine);
        const float distance = point.getDistanceFrom(onLine) - (automationNodesVisible(address) ? 1.f : 0.f);
        if (distance < nearest) { nearest = distance; result = address; }
    }
    return result;
}

bool Timeline::automationVisible(const AutomationAddress& address) const
{
    const int target = (int) address.target;
    if (target != owner.automationLane()) return false;
    if (address.target == AutomationTarget::plugin) return address == owner.pluginLane;
    const auto selected = owner.displayedAddress(owner.engine.selectedTrack);
    if (address.clipId.isEmpty()) return address.target == AutomationTarget::master;
    if (selected.clipId.isEmpty()) return false;
    if (address.stem >= 0) return owner.selectedStem == address.stem && owner.selectedClip == address.clipId;
    if (target < 4 && hasGeneratedAutomation(address) && !owner.transitionCurveVisible(target)) return false;
    const auto* link = owner.engine.project.transitionFor(owner.selectedTransition);
    if (address.clipId != selected.clipId && (!link || (address.clipId != link->incoming && address.clipId != link->outgoing))) return false;
    if (link && (address.clipId == link->incoming || address.clipId == link->outgoing))
    {
        if (owner.transitionCurveOnlyBand >= 0 && target != owner.transitionCurveOnlyBand) return false;
        if (target < 4 && !owner.transitionCurveVisible(target)) return false;
    }
    return true;
}

juce::Rectangle<float> Timeline::automationCaptionBounds() const
{
    if (!editingAutomationPoint) return {};
    const auto& [address, index] = *editingAutomationPoint;
    const auto curve = automationViewCurve(address);
    if (index < 0 || index >= (int) curve.points.size() || automationPointLabel(address, index).isEmpty() || !automationVisible(address)) return {};
    const int track = address.target == AutomationTarget::master ? 0 : address.track;
    const auto& point = curve.points[(size_t) index];
    const double time = owner.engine.project.automationSessionTime(address, point.time);
    const juce::Point<float> node {(float) (time * pixelsPerSecond), automationY(track, point.value, (int) address.target)};
    const auto view = owner.viewport.getViewArea().toFloat();
    if (!view.contains(node)) return {};
    const auto area = view.getIntersection({view.getX(), (float) laneTop(track), view.getWidth(), (float) (laneSize(track) - curveToolsReserve(track))}).reduced(3);
    if (area.getWidth() < 47 || area.getHeight() < 26) return {};
    // One late-painted opaque caption, offset from all editable dots and handles.
    std::vector<juce::Rectangle<float>> obstacles;
    for (const auto& other : visibleAutomationAddresses(track)) if (automationNodesVisible(other))
        for (const auto& p : automationViewCurve(other).points)
        {
            const auto t = owner.engine.project.automationSessionTime(other, p.time);
            obstacles.push_back({(float) (t * pixelsPerSecond) - 10, automationY(track, p.value, (int) other.target) - 10, 20, 20});
        }
    for (const auto& handle : swapHandles()) obstacles.push_back(handle.bounds.expanded(2));
    for (const auto& handle : cutAmountHandles()) obstacles.push_back(handle.bounds.expanded(2));
    for (const auto& link : owner.engine.project.transitionLinks)
    {
        obstacles.push_back(transitionBounds(link).withHeight(18));
        obstacles.push_back(transitionSongBounds(link, true)); obstacles.push_back(transitionSongBounds(link, false));
    }
    for (float dy : {-30.f, 12.f, -56.f, 38.f}) for (float dx : {12.f, -56.f})
    {
        auto candidate = juce::Rectangle<float>(node.x + dx, node.y + dy, 44, 22).constrainedWithin(area);
        if (std::none_of(obstacles.begin(), obstacles.end(), [&](const auto& r) { return candidate.intersects(r); })) return candidate;
    }
    // Dense nodes: use free space anywhere in the visible lane before conceding.
    for (float y = area.getY(); y + 22 <= area.getBottom(); y += 24)
        for (float x = area.getX(); x + 44 <= area.getRight(); x += 28)
        {
            const juce::Rectangle<float> candidate {x, y, 44, 22};
            if (std::none_of(obstacles.begin(), obstacles.end(), [&](const auto& r) { return candidate.intersects(r); })) return candidate;
        }
    return {}; // Keep every node target exposed if the entire lane is occupied; status retains its value.
}

void Timeline::paintAutomationCaption(juce::Graphics& g)
{
    const auto box = automationCaptionBounds(); if (box.isEmpty()) return;
    const auto& [address, point] = *editingAutomationPoint;
    g.setColour(colour::panel); g.fillRoundedRectangle(box, 3);
    g.setColour(colour::orange); g.drawRoundedRectangle(box, 3, 1);
    g.setColour(colour::text); g.setFont(juce::FontOptions(11).withStyle("Bold"));
    const auto curve = automationViewCurve(address);
    const float value = curve.points[(size_t) point].value;
    const auto text = address.target == AutomationTarget::cutoff
        ? juce::String(juce::roundToInt(value * 100)) + "%" : owner.automationText(address.target, value);
    g.drawText(text, box.reduced(3, 0), juce::Justification::centred);
}

void Timeline::paintAutomationNodes(juce::Graphics& g)
{
    const auto& project = owner.engine.project;
    const std::array<juce::Colour, 4> colours {juce::Colour(0xffffad70), juce::Colour(0xffe4d15b), juce::Colour(0xff76c7ef), juce::Colour(0xffd3a3ef)};
    for (int track = 0; track < trackCount; ++track)
        for (const auto& address : visibleAutomationAddresses(track)) if (laneSize(track) >= 55 && automationNodesVisible(address))
        {
            const auto curve = automationViewCurve(address);
            const auto type = (int) address.target;
            const auto ink = type < 4 ? colours[(size_t) type] : juce::Colour::fromHSV(.08f + type * .115f, .65f, .95f, 1.f);
            for (int index = 0; index < (int) curve.points.size(); ++index)
            {
                const auto& point = curve.points[(size_t) index];
                const auto time = project.automationSessionTime(address, point.time);
                const juce::Point<float> centre {(float) (time * pixelsPerSecond), automationY(track, point.value, type)};
                if (!g.getClipBounds().toFloat().expanded(8).contains(centre)) continue;
                const bool selected = automationPointLabel(address, index).isNotEmpty();
                g.setColour(colour::panel); g.fillEllipse(centre.x - 6, centre.y - 6, 12, 12);
                g.setColour(ink); g.fillEllipse(centre.x - 4, centre.y - 4, 8, 8);
                if (selected) { g.setColour(colour::text); g.drawEllipse(centre.x - 7, centre.y - 7, 14, 14, 1.2f); }
            }
        }
}

bool Timeline::keyPressed(const juce::KeyPress& key)
{
    if (isTimelineContextKey(key))
    {
        const auto anchor = owner.timelineRuler.getScreenBounds().withWidth(1);
        const auto time = owner.engine.position.load();
        if (editingAutomationPoint) owner.showAutomationNodeMenu(editingAutomationPoint->first, editingAutomationPoint->second, time, anchor);
        else if (!owner.editSelection.multiple() && owner.selectedTransition.isNotEmpty()) owner.showTransitionMenu(owner.selectedTransition, anchor);
        else owner.showTimelineMenu(time, owner.engine.selectedTrack, anchor);
        return true;
    }
    if (hotcueKeyPressed(key)) return true;
    if (sectionKeyPressed(key)) return true;
    if (key.getKeyCode() == 'D' && !key.getModifiers().isCtrlDown() && !key.getModifiers().isAltDown() && !owner.busy)
    {
        const auto handles = cutAmountHandles(); if (handles.empty()) return false;
        const auto found = selectedCutAmount ? std::find_if(handles.begin(), handles.end(), [&](const auto& h) { return h.sameOwner(*selectedCutAmount); }) : handles.end();
        const int step = key.getModifiers().isShiftDown() ? -1 : 1;
        const int index = found == handles.end() ? (step > 0 ? 0 : (int) handles.size() - 1) : ((int) (found - handles.begin()) + step + (int) handles.size()) % (int) handles.size();
        selectedCutAmount = handles[(size_t) index]; editingAutomationPoint.reset(); selectedSection.reset(); selectedHotcue.reset(); hotcueGesture.reset(); grabKeyboardFocus();
        owner.setStatus(cutAmountText(*selectedCutAmount) + ""); refreshAccessibleItems(); repaint(); return true;
    }
    if (selectedCutAmount && !owner.busy)
    {
        if (key.getKeyCode() == juce::KeyPress::deleteKey) { owner.setStatus("Amount handle kept. Use Transitions for paired treatments, or select a manual node to delete it."); return true; }
        const auto handles = cutAmountHandles();
        const auto found = std::find_if(handles.begin(), handles.end(), [&](const auto& h) { return h.sameOwner(*selectedCutAmount); });
        if (found == handles.end()) selectedCutAmount.reset();
        else if (key.getKeyCode() == juce::KeyPress::escapeKey) { selectedCutAmount.reset(); refreshAccessibleItems(); repaint(); return true; }
        else if (key.getKeyCode() == juce::KeyPress::returnKey) { editCutAmount(*found); return true; }
        else if (!key.getModifiers().isCtrlDown() && !key.getModifiers().isAltDown() && (key.getKeyCode() == juce::KeyPress::upKey || key.getKeyCode() == juce::KeyPress::downKey))
        {
            const auto h = *found;
            const float delta = (key.getKeyCode() == juce::KeyPress::upKey ? 1.f : -1.f) * (key.getModifiers().isShiftDown() ? .1f : .01f);
            setCutAmount(h, h.value + delta, true); owner.refresh(); return true;
        }
    }
    if (owner.busy || key.getModifiers().isAnyModifierKeyDown()) return false;
    const int code = key.getKeyCode();
    if (editingAutomationPoint)
    {
        const auto& address = editingAutomationPoint->first;
        const int track = address.target == AutomationTarget::master ? 0 : address.track;
        if (owner.automationLane() != (int) address.target || !(owner.displayedAddress(track) == address) || !automationVisible(address)) editingAutomationPoint.reset();
    }
    if (code == juce::KeyPress::leftKey || code == juce::KeyPress::rightKey)
    {
        if (owner.automationLane() < 0) return false;
        const auto address = owner.displayedAddress(owner.engine.selectedTrack);
        const auto curve = automationViewCurve(address); if (curve.points.empty() || !automationVisible(address)) return false;
        int index = code == juce::KeyPress::leftKey ? (int) curve.points.size() - 1 : 0;
        if (editingAutomationPoint && editingAutomationPoint->first == address)
            index = juce::jlimit(0, (int) curve.points.size() - 1, editingAutomationPoint->second + (code == juce::KeyPress::leftKey ? -1 : 1));
        selectAutomationPoint(address, index);
        owner.setStatus(juce::String(automationName(address.target)) + " " + automationPointLabel(address, index) + "");
        return true;
    }
    if (code == juce::KeyPress::returnKey && editingAutomationPoint)
    {
        if (!owner.engine.exporting) owner.editAutomationPoint(owner.engine.selectedTrack, editingAutomationPoint->second);
        return true;
    }
    if (code == juce::KeyPress::deleteKey && editingAutomationPoint)
    {
        const auto [address, index] = *editingAutomationPoint;
        owner.deleteAutomationNode(address, index); return true;
    }
    return false;
}

std::vector<Timeline::CutAmountHandle> Timeline::cutAmountHandles(bool includeOffscreen) const
{
    std::vector<CutAmountHandle> result;
    const auto& p = owner.engine.project;
    const auto view = owner.viewport.getViewArea().toFloat();
    auto add = [&](CutAmountHandle h, float y)
    {
        const int track = h.address.target == AutomationTarget::master ? 0 : h.address.track;
        if (laneSize(track) < 55 || (!includeOffscreen && (h.time * pixelsPerSecond < view.getX() || h.time * pixelsPerSecond > view.getRight()))) return;
        const float bottom = (float) laneTop(track + 1) - curveToolsReserve(track) - 20;
        const float line = (float) (h.time * pixelsPerSecond), top = (float) laneTop(track) + 26;
        constexpr float labelWidth = 44;
        float x = line + (h.before ? -labelWidth - 14 : 14);
        // Flip sides at a scrolled edge, preserving the node and timing hit targets.
        if (x < view.getX() + 2) x = line + 14;
        if (x + labelWidth > view.getRight() - 2) x = line - labelWidth - 14;
        h.bounds = {x, juce::jlimit(top, bottom, y - 7), labelWidth, 14};
        const float initialY = h.bounds.getY();
        if (includeOffscreen && (line < view.getX() || line > view.getRight())) { result.push_back(h); return; }
        for (const float offset : {0.f, 16.f, -16.f, 32.f, -32.f, 48.f, -48.f})
        {
            auto placed = h.bounds.withY(juce::jlimit(top, bottom, initialY + offset));
            if (std::none_of(result.begin(), result.end(), [&](const auto& other) { return placed.expanded(1).intersects(other.bounds); })) { h.bounds = placed; break; }
        }
        result.push_back(h);
    };
    if (const auto* link = p.transitionFor(owner.selectedTransition))
        for (int band = 0; band < 4; ++band) if (owner.transitionCurveVisible(band))
            for (int side = 0; side < 2; ++side)
            {
                const auto* clip = p.clipFor(side == 0 ? link->outgoing : link->incoming); if (!clip) continue;
                CutAmountHandle h; h.address = {clip->track, (AutomationTarget) band, {}, {}, clip->id}; h.incoming = link->incoming; h.before = side == 0;
                if (const auto& setting = link->bands[(size_t) band]; setting && setting->generated())
                {
                    if ((setting->mode == TransitionBandMode::fadeIn && side == 0) || (setting->mode == TransitionBandMode::fadeOut && side == 1)) continue;
                    if (setting->mode != TransitionBandMode::swap && setting->bend[side == 1 ? 0 : 1] < 1) continue;
                    const auto playback = p.transitionPlaybackFor(*clip);
                    const auto envelope = playback.pairs[side == 1 ? 0 : 1][(size_t) band];
                    h.time = envelope.instant ? envelope.begin : envelope.end; h.value = 1 - (float) setting->strength;
                }
                else if (band == 1 && link->bassSwap)
                {
                    const auto& curve = owner.engine.project.effectSettings(*clip).automation[1]; if (!curve.enabled) continue;
                    const auto& positions = *link->bassSwap;
                    const int first = pairedNodeAt(curve,positions[(size_t) side*2]), last = pairedNodeAt(curve,positions[(size_t) side*2+1]);
                    if (first < 0 || last < 0) continue;
                    const int low = curve.points[(size_t) first].value <= curve.points[(size_t) last].value ? first : last;
                    h.legacy = true; h.node = low;
                    h.time = clip->start + clip->localTimeForSource(positions[(size_t) side * 2 + 1]);
                    const float db = curve.points[(size_t) low].value; h.value = db <= -60 ? 0 : juce::Decibels::decibelsToGain(db);
                }
                else continue;
                add(h, laneY(clip->track, 54.f + band * 16));
            }
    for (int track = 0; track < trackCount; ++track)
        for (const auto& address : visibleAutomationAddresses(track))
        {
            const auto* selectedPair = p.transitionFor(owner.selectedTransition);
            const int band = (int) address.target;
            const bool pairCurve = selectedPair && band >= 0 && band < 4 && owner.transitionCurveVisible(band)
                && (address.clipId == selectedPair->outgoing || address.clipId == selectedPair->incoming);
            if (!automationNodesVisible(address) && !pairCurve) continue;
            const auto preview = automationViewCurve(address); const auto* curve = &preview; if (!curve->enabled) continue;
            for (int i = 1; i < (int) curve->points.size(); ++i)
            {
                const auto& a = curve->points[(size_t) i - 1]; const auto& b = curve->points[(size_t) i];
                // Existing anti-click endpoint pairs and a fully bent segment are hard cuts.
                const double time = p.automationSessionTime(address, b.time);
                const double begin = p.automationSessionTime(address, a.time);
                if (pairCurve && selectedPair->bands[(size_t) band] && selectedPair->bands[(size_t) band]->generated()
                    && p.transitionRange(*selectedPair).contains(time)) continue;
                if (a.value == b.value || time - begin <= 1.e-10 || (a.bend < 1 && time - begin > .005)) continue;
                const bool paired = std::any_of(result.begin(), result.end(), [&](const auto& h) { return h.legacy && h.address == address && std::abs(h.time - time) < 1e-6; });
                if (paired) continue;
                for (int side = 0; side < 2; ++side)
                {
                    CutAmountHandle h; h.address = address; h.segment = i - 1; h.node = i - 1 + side; h.before = side == 0; h.time = time;
                    const float value = curve->points[(size_t) h.node].value;
                    h.value = automationUsesGain(address.target) ? (value <= -60 ? 0 : juce::Decibels::decibelsToGain(value)) : value;
                    add(h, automationY(track, value, (int) address.target));
                }
            }
        }
    return result;
}

std::optional<Timeline::CutAmountHandle> Timeline::cutAmountAt(juce::Point<float> point) const
{
    if (point.y < 0) return {};
    const auto handles = cutAmountHandles();
    for (const auto& h : handles) if (showCutAmount(h, handles, laneSize(h.address.target == AutomationTarget::master ? 0 : h.address.track), owner.automationLane(), selectedCutAmount) && h.bounds.contains(point)) return h;
    return {};
}

juce::String Timeline::cutAmountText(const CutAmountHandle& h) const
{
    const auto name = h.address.target == AutomationTarget::treble ? juce::String("Treble") : juce::String(automationName(h.address.target));
    const auto value = automationUsesGain(h.address.target) ? "cut " + juce::String((1 - h.value) * 100, 1) + "%"
        : owner.automationText(h.address.target, h.value);
    return name + " " + value + (h.incoming.isNotEmpty() ? " / paired amount" : h.before ? " / before cut" : " / after cut");
}

void Timeline::paintCutAmounts(juce::Graphics& g)
{
    const auto handles = cutAmountHandles();
    for (const auto& h : handles)
    {
        if (!showCutAmount(h, handles, laneSize(h.address.target == AutomationTarget::master ? 0 : h.address.track), owner.automationLane(), selectedCutAmount)) continue;
        const bool active = selectedCutAmount && h.sameOwner(*selectedCutAmount);
        const auto ink = owner.engine.project.automationLocked(h.address) ? colour::muted : active ? colour::orange
            : juce::Colour::fromHSV(.08f + (int) h.address.target * .115f, .45f, .95f, 1.f);
        g.setColour(colour::panel); g.fillRoundedRectangle(h.bounds, 3);
        g.setColour(ink); g.drawRoundedRectangle(h.bounds, 3, active ? 1.5f : .8f);
        const float x = h.bounds.getX() + 6, y = h.bounds.getCentreY();
        g.drawLine(x, y - 4, x, y + 4); g.drawLine(x, y - 4, x - 2, y - 2); g.drawLine(x, y - 4, x + 2, y - 2);
        g.drawLine(x, y + 4, x - 2, y + 2); g.drawLine(x, y + 4, x + 2, y + 2);
        g.setFont(juce::FontOptions(9).withStyle("Bold"));
        const auto label = juce::String(juce::roundToInt(h.value * 100)) + "%";
        g.drawText(label, h.bounds.withTrimmedLeft(11).reduced(2, 0), juce::Justification::centred);
    }
}

bool Timeline::beginCutAmount(const juce::MouseEvent& event)
{
    const auto hit = cutAmountAt(event.position); if (!hit) return false;
    selectedCutAmount = *hit; editingAutomationPoint.reset(); grabKeyboardFocus();
    owner.setStatus(cutAmountText(*hit) + "");
    if (event.mods.isPopupMenu()) editCutAmount(*hit);
    else cutAmountDrag = CutAmountGesture {*hit, event.position.y, hit->value};
    repaint(); return true;
}

bool Timeline::setCutAmount(const CutAmountHandle& h, float value, bool newUndo)
{
    if (!std::isfinite(value) || !owner.automationEditable(h.address)) return false;
    const auto handles = cutAmountHandles();
    const auto found = std::find_if(handles.begin(), handles.end(), [&](const auto& now) { return now.sameOwner(h) && now.value == h.value; });
    if (found == handles.end()) { owner.setStatus("The cut changed. Select its current amount handle."); return false; }
    const auto range = automationUsesGain(h.address.target) ? juce::Range<float>(0, h.incoming.isEmpty() ? 1.3f : 1.f) : automationRange(h.address.target);
    value = range.clipValue(value); if (std::abs(value - h.value) < 1.e-7f) return false;
    auto candidate = owner.automationEditSnapshot();
    const auto plateau = [](AutomationCurve& curve,int node,bool before,float next)
    {
        const float previous = curve.points[(size_t) node].value;
        int first = node, last = node;
        if (before) while (first > 0 && curve.points[(size_t) first-1].value == previous) --first;
        else while (last+1 < (int) curve.points.size() && curve.points[(size_t) last+1].value == previous) ++last;
        for (int i = first; i <= last; ++i) curve.points[(size_t) i].value = next;
    };
    if (h.incoming.isNotEmpty())
    {
        const auto* current = candidate.transitionFor(h.incoming); if (!current) return false;
        auto* link = &candidate.transitionLinks[(size_t) (current - candidate.transitionLinks.data())];
        for (const auto& id : {link->outgoing, link->incoming})
        {
            const auto* clip = candidate.clipFor(id);
            if (!clip || !owner.automationEditable({clip->track, h.address.target, {}, {}, id})) return false;
        }
        if (h.legacy)
        {
            if (!link->bassSwap) return false;
            for (int side = 0; side < 2; ++side)
            {
                const auto* clip = candidate.clipFor(side == 0 ? link->outgoing : link->incoming);
                const AutomationAddress address {clip->track, AutomationTarget::bass, {}, {}, clip->id};
                auto* curve = candidate.curve(address); if (!curve || !curve->enabled) return false;
                const std::array<int,2> nodes {pairedNodeAt(*curve,(*link->bassSwap)[(size_t) side*2]),pairedNodeAt(*curve,(*link->bassSwap)[(size_t) side*2+1])};
                if (nodes[0] < 0 || nodes[1] < 0) return false;
                const int low = curve->points[(size_t) nodes[0]].value <= curve->points[(size_t) nodes[1]].value ? 0 : 1;
                const float db = juce::Decibels::gainToDecibels(value, -60.f);
                plateau(*curve,nodes[(size_t) low],low == 0,db); candidate.automationEdited(address);
            }
        }
        else
        {
            auto& setting = link->bands[(size_t) h.address.target]; if (!setting) return false;
            setting->strength = 1 - (double) value;
        }
    }
    else
    {
        auto* curve = owner.materialiseAutomation(candidate,h.address); if (!curve || h.node < 0 || h.node >= (int) curve->points.size()) return false;
        const float next = automationUsesGain(h.address.target) ? juce::Decibels::gainToDecibels(value, -60.f) : value;
        // A cut amount owns the contiguous flat side of its step. Keep its
        // plateau flat instead of tilting the following/preceding segment.
        plateau(*curve,h.node,h.before,next);
        candidate.automationEdited(h.address);
    }
    const auto valid = candidate.validateTransitionLinks(); if (valid.failed()) { owner.setStatus(valid.getErrorMessage()); return false; }
    const auto protection = owner.engine.project.validateTransitionLocks(candidate); if (protection.failed()) { owner.setStatus(protection.getErrorMessage()); return false; }
    if (!owner.publishAutomationEdit(std::move(candidate),h.address,newUndo)) return false;
    selectedCutAmount = h; selectedCutAmount->value = value;
    owner.setStatus(cutAmountText(*selectedCutAmount) + ""); repaint(); return true;
}

bool Timeline::continueCutAmount(const juce::MouseEvent& event)
{
    if (!cutAmountDrag) return false;
    if (!event.mouseWasDraggedSinceMouseDown()) return true;
    auto h = cutAmountDrag->handle; h.value = cutAmountDrag->expected;
    const float wanted = cutAmountDrag->handle.value + (cutAmountDrag->y - event.position.y) / 120.f;
    if (setCutAmount(h, wanted, !moved))
    {
        moved = true;
        const auto handles = cutAmountHandles();
        const auto found = std::find_if(handles.begin(), handles.end(), [&](const auto& now) { return now.sameOwner(h); });
        if (found != handles.end()) cutAmountDrag->expected = found->value;
    }
    return true;
}

void Timeline::editCutAmount(const CutAmountHandle& h)
{
    if (!owner.automationEditable(h.address)) return;
    const bool gain = automationUsesGain(h.address.target), filter = h.address.target == AutomationTarget::cutoff;
    const auto before = juce::JSON::toString(projectToJson(owner.engine.project));
    auto* dialog = new juce::AlertWindow("Hard-cut amount", cutAmountText(h) + "\nCut time stays fixed. Paired amount changes both tracks.", juce::MessageBoxIconType::NoIcon, &owner);
    dialog->addTextEditor("amount", filter ? cutoffText(h.value) : juce::String(gain ? (1 - h.value) * 100 : h.value * 100, 3),
        filter ? "LP 20-20000 Hz / Off / HP 20-20000 Hz" : gain ? (h.incoming.isEmpty() ? "Cut depth -30 to 100% (100 = silence; negative = boost)" : "Cut depth 0 to 100% (100 = full attenuation)") : "Parameter value (%)");
    dialog->addButton("Apply", 1, juce::KeyPress(juce::KeyPress::returnKey)); dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    dialog->enterModalState(true, juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<Timeline>(this), dialog, h, gain, filter, before](int result)
    {
        if (!safe || result != 1) return;
        if (before != juce::JSON::toString(projectToJson(safe->owner.engine.project))) { safe->owner.setStatus("The project changed while amount entry was open. Select the handle again."); return; }
        float value = 0;
        if (filter) { if (!parseCutoffText(dialog->getTextEditorContents("amount"), value)) { safe->owner.setStatus("Enter LP or HP with 20-20000 Hz, or Off. Amount kept."); return; } }
        else
        {
            const auto raw = dialog->getTextEditorContents("amount").trim().toStdString(); char* end = nullptr;
            const double number = std::strtod(raw.c_str(), &end);
            const auto range = gain ? juce::Range<double>(h.incoming.isEmpty() ? -30 : 0, 100) : juce::Range<double>(automationRange(h.address.target).getStart() * 100, automationRange(h.address.target).getEnd() * 100);
            if (raw.empty() || end != raw.c_str() + raw.size() || !std::isfinite(number) || number < range.getStart() || number > range.getEnd()) { safe->owner.setStatus("Enter an amount inside the displayed range. Amount kept."); return; }
            value = (float) (gain ? 1 - number / 100 : number / 100);
        }
        safe->setCutAmount(h, value, true); safe->owner.refresh();
    }), true);
}
}
