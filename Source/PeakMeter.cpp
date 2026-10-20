#include "PeakMeter.h"
#include <cmath>

namespace mixworks
{
void PeakMeterDisplay::advance(std::array<float, 2> peaks, double elapsed, bool animate)
{
    const float dt = (float) juce::jlimit(0., .25, std::isfinite(elapsed) ? elapsed : 0.);
    clock += dt;
    for (size_t ch = 0; ch < db.size(); ++ch)
    {
        const float level = std::isfinite(peaks[ch]) ? std::max(0.f, peaks[ch]) : 0;
        const float measured = juce::Decibels::gainToDecibels(level, floorDb);
        db[ch] = std::max(measured, db[ch] - 28 * dt);
        if (measured >= hold[ch]) { hold[ch] = measured; holdSeconds[ch] = 1.2f; }
        else { holdSeconds[ch] = std::max(0.f, holdSeconds[ch] - dt); if (holdSeconds[ch] <= 0) hold[ch] = std::max(db[ch], hold[ch] - 12 * dt); }
        if (level >= 1) clipped = true;
        // Sparse bursts near full scale; clipping remains a separate red status.
        const bool burst = measured >= -1 && std::fmod(clock + ch * .31, .73) < .16;
        sparkSeconds[ch] = animate ? burst ? .38f : std::max(0.f, sparkSeconds[ch] - dt) : 0;
    }
}
void PeakMeterDisplay::resetPeaks()
{ clipped = false; hold = db; holdSeconds = {}; sparkSeconds = {}; }

void drawForgeSparks(juce::Graphics& g, juce::Point<float> origin, float spread, double phase, float strength, int seed)
{
    for (int i = 0; i < 18; ++i)
    {
        const float life = (float) std::fmod(phase * (1.1 + (i % 4) * .11) + i * .173 + seed * .07, 1.);
        const float side = i % 2 == 0 ? 1.f : -1.f;
        const float reach = spread * (.3f + (i % 7) * .1f);
        const auto p = origin + juce::Point<float>(side * life * reach, -life * reach * (1 + (i % 3) * .25f) + life * life * reach * .9f);
        const float alpha = strength * (1 - life) * (1 - life);
        g.setColour(colour::moltenOrange.withAlpha(alpha * .3f)); g.fillEllipse(p.x - 2, p.y - 2, 4, 4);
        g.setColour(colour::moltenCore.withAlpha(alpha));
        g.drawLine(p.x, p.y, p.x - side * (1 + life * 3), p.y + 1 + life * 2, .8f);
    }
}

void PeakMeterDisplay::paint(juce::Graphics& g, juce::Rectangle<int> bounds, bool sparks) const
{
    const auto panel = bounds.toFloat().reduced(.5f);
    g.setGradientFill(juce::ColourGradient(colour::raised, panel.getTopLeft(), colour::panel.darker(.15f), panel.getBottomRight(), false));
    g.fillRoundedRectangle(panel, 4); g.setColour(colour::border); g.drawRoundedRectangle(panel, 4, 1);
    // Header button and gain dial are native controls laid out by the workspace.
    const float top = panel.getY() + 76, bottom = panel.getBottom() - (bounds.getHeight() >= 240 ? 124.f : 30.f);
    const float height = std::max(1.f, bottom - top);
    const float centre = panel.getCentreX(), railWidth = 18;
    for (int ch = 0; ch < 2; ++ch)
    {
        const float x = centre + (ch == 0 ? -40.f : 22.f);
        const auto label = juce::Rectangle<float>(x - 11, panel.getY() + 36, railWidth + 22, 16);
        g.setColour(colour::muted); g.setFont(juce::FontOptions(11)); g.drawText(ch == 0 ? "L" : "R", label, juce::Justification::centred);
        g.setColour(hold[(size_t) ch] >= 0 ? colour::error : colour::text); g.setFont(juce::FontOptions(15));
        g.drawText(hold[(size_t) ch] <= floorDb ? "-inf" : juce::String(hold[(size_t) ch], 1), label.translated(0, 17).withHeight(19), juce::Justification::centred);
    }
    if (bottom - top < 14) return; // Collapsed timelines retain channel readouts; gain stays in the inspector.
    auto yAt = [&](float value) { return top + height * (1 - juce::jlimit(0.f, 1.f, (value - floorDb) / -floorDb)); };
    g.setFont(juce::FontOptions(10));
    const int stride = height < 90 ? 48 : height < 160 ? 24 : height < 300 ? 12 : 6;
    for (int value = 0; value >= (int) floorDb; value -= stride)
    {
        const float y = yAt((float) value);
        g.setColour(colour::muted); g.drawText(juce::String(value), juce::Rectangle<float>(centre - 16, y - 7, 32, 14), juce::Justification::centred);
        g.setColour(colour::border); g.drawLine(centre - 23, y, centre - 17, y); g.drawLine(centre + 17, y, centre + 23, y);
    }
    for (int ch = 0; ch < 2; ++ch)
    {
        const float x = centre + (ch == 0 ? -40.f : 22.f);
        const juce::Rectangle<float> rail(x, top, railWidth, height);
        g.setColour(colour::background.darker(.6f)); g.fillRoundedRectangle(rail.expanded(3), 2);
        g.setColour(colour::border); g.drawRoundedRectangle(rail.expanded(3), 2, 1);
        const int segments = juce::jlimit(1, 120, (int) (height / 6));
        for (int i = 0; i < segments; ++i)
        {
            const float fraction = (i + .5f) / segments;
            const auto cell = juce::Rectangle<float>(x, bottom - (i + 1) * height / segments + 1, railWidth, std::max(1.f, height / segments - 2));
            if (db[(size_t) ch] > floorDb && fraction <= (db[(size_t) ch] - floorDb) / -floorDb)
                AlloyLookAndFeel::drawMoltenMetal(g, cell, false, (db[(size_t) ch] - floorDb) / -floorDb);
            else { g.setColour(colour::moltenEdge.darker(.6f)); g.fillRect(cell); }
        }
        if (hold[(size_t) ch] > floorDb)
        {
            const auto pin = juce::Rectangle<float>(x - 2, yAt(hold[(size_t) ch]) - 1, railWidth + 4, 2);
            g.setColour(colour::moltenCore); g.fillRect(pin);
        }
        if (sparks && sparkSeconds[(size_t) ch] > 0)
            drawForgeSparks(g, {x + railWidth * .5f, yAt(db[(size_t) ch])}, 22, clock, sparkSeconds[(size_t) ch] / .38f, ch);
    }
    g.setColour(colour::border); g.drawHorizontalLine((int) bottom + 24, panel.getX() + 7, panel.getRight() - 7);
    g.setColour(colour::muted); g.setFont(juce::FontOptions(9));
    g.drawText("dBFS", juce::Rectangle<float>(centre - 20, bottom + 10, 40, 10), juce::Justification::centred);
}
}
