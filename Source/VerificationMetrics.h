#pragma once
#include <juce_core/juce_core.h>

namespace mixworks
{
struct VerificationMetrics
{
    bool available = false, cpuAvailable = false;
    juce::int64 working = 0, peakWorking = 0, committed = 0, peakCommitted = 0;
    double kernelMilliseconds = 0, userMilliseconds = 0;
    unsigned long memoryError = 0, cpuError = 0;
};
VerificationMetrics readVerificationMetrics();
juce::var verificationMetricsJson(const VerificationMetrics&);
}
