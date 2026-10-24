#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace mixworks
{
class Workstation;
// Opt-in local development/capture automation; name retained for promo compatibility.
class PromoControl final : private juce::Timer
{
public:
    static juce::Result prepare(const juce::File&);
    PromoControl(Workstation&, juce::File, bool poll = true);
    ~PromoControl() override;
    juce::var execute(const juce::var&);
    juce::var state() const;
private:
    Workstation& owner;
    const juce::File root;
    const juce::String token;
    void timerCallback() override;
    bool contained(const juce::File&) const;
    bool safeDocument(const juce::var&) const;
};
void runPromoControlTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
}
