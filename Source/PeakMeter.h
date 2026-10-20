#pragma once
#include "AlloyLookAndFeel.h"
#include "OutputPeaks.h"

namespace mixworks
{
struct PeakMeterDisplay
{
    static constexpr float floorDb = -48;
    std::array<float, 2> db {floorDb, floorDb}, hold {floorDb, floorDb}, holdSeconds {};
    std::array<float, 2> sparkSeconds {};
    double clock = 0;
    bool clipped = false;
    void advance(std::array<float, 2> peaks, double elapsed, bool animate);
    void resetPeaks();
    void paint(juce::Graphics&, juce::Rectangle<int>, bool sparks) const;
};

// Fixed-cost vector droplets. No bitmap allocation, timers or audio work in paint.
void drawForgeSparks(juce::Graphics&, juce::Point<float>, float spread, double phase, float strength, int seed);
}
