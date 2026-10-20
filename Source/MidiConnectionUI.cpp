#include "Workstation.h"

namespace mixworks
{
void Workstation::showControllerConnection(const MidiConnectionStatus& connection)
{
    controllerConnection.setButtonText(connection.caption());
    controllerConnection.setTitle("Controller connection status");
    controllerConnection.setTooltip(connection.description() + "\nClick to open Audio / MIDI. Detection, an open input and received messages are separate states.");
    const auto state = connection.state();
    controllerConnection.setColour(juce::TextButton::textColourOffId, state == MidiConnectionStatus::State::receiving ? colour::orange
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
