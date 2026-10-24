#include "Workstation.h"

namespace mixworks
{
void Workstation::showControllerConnection(const MidiConnectionStatus& connection)
{
    settingsButton.setButtonText(connection.caption());
    settingsButton.setTitle("Controller connection status");
    settingsButton.setTooltip(connection.description() + "\nClick to open Audio / MIDI. Detection, an open input and received messages are separate states.");
    const auto state = connection.state();
    settingsButton.setColour(juce::TextButton::textColourOffId, state == MidiConnectionStatus::State::receiving ? colour::orange
        : state == MidiConnectionStatus::State::connected ? colour::tracks[2] : colour::muted);
}
void Workstation::refreshControllerConnection()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (now < nextControllerPoll) return;
    nextControllerPoll = now + 500;
    showControllerConnection(engine.midiConnectionStatus());
}
}
