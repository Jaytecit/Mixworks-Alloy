#include "VerificationMetrics.h"
#if JUCE_WINDOWS
 #include <windows.h>
 #include <psapi.h>
#endif

namespace mixworks
{
VerificationMetrics readVerificationMetrics()
{
    VerificationMetrics result;
   #if JUCE_WINDOWS
    PROCESS_MEMORY_COUNTERS_EX counters {}; counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)))
    {
        result.available = true; result.working = (juce::int64) counters.WorkingSetSize; result.peakWorking = (juce::int64) counters.PeakWorkingSetSize;
        result.committed = (juce::int64) counters.PrivateUsage; result.peakCommitted = (juce::int64) counters.PeakPagefileUsage;
    }
    else result.memoryError = GetLastError();
    FILETIME created {}, exited {}, kernel {}, user {};
    if (GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user))
    {
        const auto milliseconds = [](FILETIME time) { return (((juce::uint64) time.dwHighDateTime << 32) | time.dwLowDateTime) / 10000.; };
        result.cpuAvailable = true; result.kernelMilliseconds = milliseconds(kernel); result.userMilliseconds = milliseconds(user);
    }
    else result.cpuError = GetLastError();
   #endif
    return result;
}
juce::var verificationMetricsJson(const VerificationMetrics& value)
{
    auto* object = new juce::DynamicObject(); object->setProperty("available", value.available); object->setProperty("cpuAvailable", value.cpuAvailable);
    if (value.available)
    {
        object->setProperty("workingSetBytes", value.working); object->setProperty("peakWorkingSetBytes", value.peakWorking);
        object->setProperty("privateCommitBytes", value.committed); object->setProperty("peakPrivateCommitBytes", value.peakCommitted);
    }
    else object->setProperty("memoryError", (juce::int64) value.memoryError);
    if (value.cpuAvailable) { object->setProperty("kernelMilliseconds", value.kernelMilliseconds); object->setProperty("userMilliseconds", value.userMilliseconds); }
    else object->setProperty("cpuError", (juce::int64) value.cpuError);
    return juce::var(object);
}
}
