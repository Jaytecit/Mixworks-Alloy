#pragma once
#include "Model.h"

namespace mixworks
{
struct LaunchBoundary { double time = 0, beat = 0; };
// Project bar 1 owns beat zero. Edit Snap and section hints do not alter this clock.
std::optional<LaunchBoundary> nextLaunchBoundary(const TempoMap&, const LaunchSettings&, double now);
const char* launchTimingName(LaunchTiming);
const char* launchTimingShortName(LaunchTiming);
}
