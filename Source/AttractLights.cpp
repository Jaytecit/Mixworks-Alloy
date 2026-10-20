#include "AttractLights.h"
#include <cmath>

namespace mixworks
{
namespace
{
float fraction(double value) { return (float) (value - std::floor(value)); }
float intensity(int pattern, double seconds, float x, float y, int ordinal, int count)
{
    const float p = fraction(seconds * .16);
    const float rank = (ordinal + .5f) / std::max(1, count);
    const auto beam = [](float d, float width) { return std::exp(-d * d * width); };
    switch (pattern)
    {
        case 0: return beam(x - (1 - std::abs(2 * p - 1)), 28); // Scanner.
        case 1: { float d = std::abs(rank - p); d = std::min(d, 1 - d); return beam(d, 150); }
        case 2: return beam(std::hypot(x - .5f, y - .5f) - p, 65); // Expanding ring.
        case 3: return std::pow(.5f + .5f * std::sin((float) seconds * 1.3f + (ordinal % 8) * .8f), 3.f);
        case 4: return beam(y - p, 22); // Vertical cascade, including fader banks.
        default: return .3f + .7f * (.5f + .5f * std::sin((float) seconds * .65f));
    }
}
juce::Colour ink(int pattern)
{
    const std::array<juce::Colour, 6> colours {{colour::moltenOrange, colour::tracks[1], colour::orange,
        colour::tracks[3], colour::tracks[2], colour::moltenCore}};
    return colours[(size_t) pattern];
}
}
AttractLights::AttractLights(juce::Component& owner, const AttractState& shared) : host(owner), state(shared)
{
    setInterceptsMouseClicks(false, false); setWantsKeyboardFocus(false); setAccessible(false); setOpaque(false);
}
void AttractLights::syncMode()
{
    setVisible(state.enabled); if (state.enabled) toFront(false); repaint();
}
AttractLights::Light AttractLights::lightAt(double seconds, float x, float y, int ordinal, int count, bool animate)
{
    if (!animate) return {.42f, 0, colour::orange, 5};
    seconds = std::isfinite(seconds) ? std::fmod(std::max(0., seconds), 288.) : 0;
    static constexpr int order[] {0, 1, 4, 3, 2, 5};
    const int step = (int) (seconds / 8), cycle = step / 6;
    const int a = order[(step + cycle * 2) % 6], b = order[(step + 1 + ((step + 1) / 6) * 2) % 6];
    const float blend = juce::jlimit(0.f, 1.f, ((float) std::fmod(seconds, 8.) - 6.8f) / 1.2f);
    const float heat = intensity(a, seconds, x, y, ordinal, count) * (1 - blend) + intensity(b, seconds, x, y, ordinal, count) * blend;
    return {juce::jlimit(0.f, 1.f, .06f + .94f * heat), fraction(seconds * .24 + ordinal * .073), ink(a).interpolatedWith(ink(b), blend), a};
}
void AttractLights::collect(juce::Component& node, std::vector<Target>& targets)
{
    if (&node == this || (&node != &host && !node.isVisible())) return;
    if (&node != &host && (bool) node.getProperties()["alloyAttractOwned"]) return;
    auto bounds = host.getLocalArea(&node, node.getLocalBounds()).toFloat();
    for (auto* parent = node.getParentComponent(); parent && &node != &host; parent = parent->getParentComponent())
    {
        bounds = bounds.getIntersection(host.getLocalArea(parent, parent->getLocalBounds()).toFloat());
        if (parent == &host) break;
    }
    if (bounds.isEmpty()) return;
    const auto tag = node.getProperties()["alloyAttractSurface"].toString();
    if (tag == "keys") { targets.push_back({bounds, Kind::keys}); return; }
    if (dynamic_cast<juce::Button*>(&node))
    { targets.push_back({bounds, (bool) node.getProperties()["alloyPad"] ? Kind::pad : Kind::button}); return; }
    if (auto* slider = dynamic_cast<juce::Slider*>(&node))
    {
        auto body = host.getLocalArea(slider, slider->getLookAndFeel().getSliderLayout(*slider).sliderBounds).toFloat().getIntersection(bounds);
        targets.push_back({body, slider->isRotary() ? Kind::dial : Kind::linear, slider->isVertical()}); return;
    }
    if (dynamic_cast<juce::ComboBox*>(&node)) { targets.push_back({bounds, Kind::button}); return; }
    if (auto* scrollbar = dynamic_cast<juce::ScrollBar*>(&node)) { targets.push_back({bounds, Kind::linear, scrollbar->isVertical()}); return; }
    if (dynamic_cast<juce::TableListBox*>(&node) || tag == "bank") { targets.push_back({bounds, Kind::bank}); return; }
    for (auto* child : node.getChildren()) collect(*child, targets);
}
void AttractLights::illuminate(juce::Graphics& g, const Target& target, const Light& light)
{
    auto r = target.bounds.reduced(1); if (r.isEmpty()) return;
    const float heat = light.heat; const auto c = light.colour;
    juce::Graphics::ScopedSaveState save(g);
    if (target.kind == Kind::dial)
    {
        const float radius = std::max(2.f, std::min(r.getWidth(), r.getHeight()) * .44f);
        for (int led = 0; led < 18; ++led)
        {
            const float phase = led / 18.f, angle = phase * juce::MathConstants<float>::twoPi;
            float d = std::abs(phase - light.phase); d = std::min(d, 1 - d);
            const float level = heat * (.18f + .82f * std::exp(-d * d * 65));
            auto dot = juce::Rectangle<float>(4, 4).withCentre(r.getCentre() + juce::Point<float>(std::sin(angle) * radius, -std::cos(angle) * radius));
            g.setColour(c.withAlpha(level * .12f)); g.fillEllipse(dot.expanded(3));
            g.setColour(c.withAlpha(level * .9f)); g.fillEllipse(dot);
        }
        return;
    }
    if (target.kind == Kind::linear)
    {
        for (int led = 0; led < 14; ++led)
        {
            const float phase = led / 13.f, d = std::abs(phase - light.phase);
            const float level = heat * (.08f + .92f * std::exp(-d * d * 32));
            const auto point = target.vertical ? juce::Point<float>(r.getRight() - 2, r.getBottom() - phase * r.getHeight())
                : juce::Point<float>(r.getX() + phase * r.getWidth(), r.getY() + 2);
            const auto dot = juce::Rectangle<float>(3, 3).withCentre(point);
            g.setColour(c.withAlpha(level * .12f)); g.fillEllipse(dot.expanded(2)); g.setColour(c.withAlpha(level)); g.fillEllipse(dot);
        }
        return;
    }
    for (int layer = 3; layer > 0; --layer)
    { g.setColour(c.withAlpha(heat * .025f * (4 - layer))); g.drawRoundedRectangle(r.expanded((float) layer), 4.f + layer, 2); }
    g.setColour(c.withAlpha(heat * .62f)); g.drawRoundedRectangle(r, 4, 1.2f);
    if (target.kind == Kind::button || target.kind == Kind::pad)
    {
        g.setColour(c.withAlpha(heat * (target.kind == Kind::pad ? .16f : .055f))); g.fillRoundedRectangle(r, 4);
        const float x = r.getX() + light.phase * r.getWidth();
        g.setGradientFill(juce::ColourGradient(c.withAlpha(0.f), x - 16, r.getY(), c.withAlpha(heat * .85f), x, r.getY(), false));
        g.fillRect(r.withHeight(2));
    }
    else
    {
        // Moving edge lights keep waveforms, real meters, values and tab state legible.
        const int leds = target.kind == Kind::keys ? 24 : 12;
        for (int led = 0; led < leds; ++led)
        {
            const float phase = (led + .5f) / leds, d = std::abs(phase - light.phase);
            const float level = heat * (.12f + .88f * std::exp(-d * d * 45));
            const float x = r.getX() + phase * r.getWidth();
            g.setColour(c.withAlpha(level)); g.fillRoundedRectangle(x - 2, r.getBottom() - 3, 4, 2, 1);
        }
    }
}
void AttractLights::paint(juce::Graphics& g)
{
    if (!state.enabled) return;
    std::vector<Target> targets;
    for (const auto& panel : panels) targets.push_back({panel.getIntersection(getLocalBounds().toFloat()), Kind::panel});
    collect(host, targets);
    const double seconds = (juce::Time::getMillisecondCounterHiRes() - state.startedAtMs) * .001;
    const bool animate = state.allowsMotion();
    for (size_t index = 0; index < targets.size(); ++index)
    {
        const auto centre = targets[index].bounds.getCentre();
        illuminate(g, targets[index], lightAt(seconds, centre.x / std::max(1, getWidth()), centre.y / std::max(1, getHeight()), (int) index, (int) targets.size(), animate));
    }
}
}
