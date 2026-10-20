#include "Model.h"
#include <limits>

namespace mixworks
{
juce::Result RehearsalContext::validate() const
{
    return beforeBars >= 0 && afterBars >= 0 ? juce::Result::ok() : juce::Result::fail("Rehearsal context requires nonnegative whole bars.");
}
juce::var rehearsalContextToJson(const RehearsalContext& context)
{
    auto* obj = new juce::DynamicObject(); obj->setProperty("beforeBars", context.beforeBars); obj->setProperty("afterBars", context.afterBars); return juce::var(obj);
}
juce::Result readRehearsalContext(const juce::var& data, RehearsalContext& result)
{
    const auto count = [](const juce::var& value)
    { return (value.isInt() || value.isInt64()) && (juce::int64) value >= 0 && (juce::int64) value <= std::numeric_limits<int>::max(); };
    if (!data.isObject() || !count(data["beforeBars"]) || !count(data["afterBars"])) return juce::Result::fail("Invalid rehearsal context bar counts.");
    RehearsalContext parsed {(int) data["beforeBars"], (int) data["afterBars"]};
    const auto valid = parsed.validate(); if (valid.failed()) return valid; result = parsed; return juce::Result::ok();
}
juce::Result Project::setRehearsalContext(const RehearsalContext& context)
{
    const auto valid = context.validate(); if (valid.failed()) return valid;
    rehearsalContext = context; return juce::Result::ok();
}
}
