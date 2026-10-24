#include "LaunchClock.h"
#include <cmath>
#include <limits>

namespace mixworks
{
juce::Result LaunchSettings::validate() const
{
    if ((int) timing < 0 || (int) timing > 3 || (phraseBars != 8 && phraseBars != 16 && phraseBars != 32))
        return juce::Result::fail("Choose Immediate, Next beat, Next bar or Next phrase, with an 8, 16 or 32 bar phrase.");
    return juce::Result::ok();
}
const char* launchTimingName(LaunchTiming value)
{
    switch (value)
    {
        case LaunchTiming::immediate: return "Immediate";
        case LaunchTiming::nextBeat: return "Next beat";
        case LaunchTiming::nextBar: return "Next bar";
        case LaunchTiming::nextPhrase: return "Next phrase";
    }
    return "Unavailable";
}
const char* launchTimingShortName(LaunchTiming value)
{
    switch (value)
    {
        case LaunchTiming::immediate: return "now";
        case LaunchTiming::nextBeat: return "beat";
        case LaunchTiming::nextBar: return "bar";
        case LaunchTiming::nextPhrase: return "phrase";
    }
    return "?";
}
std::optional<LaunchBoundary> nextLaunchBoundary(const TempoMap& clock, const LaunchSettings& settings, double now)
{
    if (settings.validate().failed() || !std::isfinite(now) || now < 0 || now > 1.e9) return {};
    if (settings.timing == LaunchTiming::immediate) return LaunchBoundary{now, 0};
    if (!std::isfinite(clock.fallback) || clock.fallback < 20 || clock.fallback > 400
        || !std::isfinite(clock.origin) || clock.origin < 0 || clock.origin > 600) return {};
    double previous = 0;
    for (const auto& point : clock.points)
    {
        if (!std::isfinite(point.time) || point.time < previous || point.time > 1.e9
            || !std::isfinite(point.bpm) || point.bpm < 20 || point.bpm > 400) return {};
        previous = point.time;
    }
    if (!clock.points.empty() && clock.points.front().time != 0) return {};
    const double step = settings.timing == LaunchTiming::nextBeat ? 1. : settings.timing == LaunchTiming::nextBar ? 4. : settings.phraseBars * 4.;
    const double beat = clock.beatsAt(now);
    if (!std::isfinite(beat)) return {};
    double target = (std::floor(beat / step) + 1) * step;
    double time = clock.timeAtBeat(target);
    // Inversion can put an exactly reached edge a few ulps after now. Treat
    // only floating-point roundoff as that edge, never a nearby human press.
    const double tolerance = 32 * std::numeric_limits<double>::epsilon() * std::max(1., std::abs(now));
    if (time <= now + tolerance) { target += step; time = clock.timeAtBeat(target); }
    if (!std::isfinite(time) || time <= now || time > 1.e9) return {};
    return LaunchBoundary{time, target};
}
}
