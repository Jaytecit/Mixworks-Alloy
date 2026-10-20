#include "Engine.h"

namespace mixworks
{
juce::Result Engine::reloadMidiMappings()
{
    const auto fail = [this](const juce::String& reason)
    { const juce::ScopedLock guard(midiLock); midiProfileError = reason; return juce::Result::fail(reason); };
    if (exporting || automationTake) return fail("Finish export or Record before reloading mappings.");
    juce::FileInputStream stream(mappingsFile);
    if (!stream.openedOk()) return fail("Cannot read " + mappingsFile.getFullPathName());
    juce::var data; const auto parsed = juce::JSON::parse(stream.readEntireStreamAsString(), data);
    if (parsed.failed()) return fail(parsed.getErrorMessage());
    const auto* entries = data.getArray();
    if (!entries) return fail("The profile must contain an array of MIDI assignments.");
    std::vector<MidiBinding> loaded; std::set<int> messages;
    for (int index = 0; index < entries->size(); ++index)
    {
        const auto& value = entries->getReference(index);
        const auto invalid = [&] { return fail("Invalid or conflicting MIDI assignment " + juce::String(index + 1) + ". Active mappings were retained."); };
        if (!value.isObject() || !value["status"].isInt() || !value["channel"].isInt() || !value["number"].isInt() || !value["action"].isString()) return invalid();
        const int status = value["status"], channel = value["channel"], number = value["number"];
        const auto action = parseMidiAction(value["action"].toString());
        if ((status != 0xb0 && status != 0x90) || channel < 1 || channel > 16 || number < 0 || number > 127 || !action.hasValue()) return invalid();
        if (!messages.insert(status * 65536 + channel * 256 + number).second) return invalid();
        loaded.push_back({status, channel, number, *action});
    }
    const juce::ScopedLock guard(midiLock);
    mappings = std::move(loaded); pendingControls.clear(); pressedMidiButtons.clear(); mappingsDirty = false;
    learnAction.reset(); learnSteps.clear(); learnStep = 0; learnedControls.clear(); replaceControllerProfile = false; midiProfileError.clear();
    return juce::Result::ok();
}
MidiConnectionStatus::State MidiConnectionStatus::state() const
{
    if (ports.empty()) return State::absent;
    if (listening && std::any_of(ports.begin(), ports.end(), [](const auto& p) { return p.open && p.recent; })) return State::receiving;
    if (listening && std::any_of(ports.begin(), ports.end(), [](const auto& p) { return p.open; })) return State::connected;
    return State::detected;
}
juce::String MidiConnectionStatus::caption() const
{
    const auto current = state();
    if (current == State::absent) return "No MIDI controller";
    const auto found = std::find_if(ports.begin(), ports.end(), [current](const auto& p) { return p.open && (current != State::receiving || p.recent); });
    const auto name = (found != ports.end() ? found->name : ports.front().name).substring(0, 24);
    return name + (current == State::receiving ? " / receiving" : current == State::connected ? " / connected" : " / detected");
}
juce::String MidiConnectionStatus::description() const
{
    juce::String text;
    if (ports.empty()) text = "No MIDI input is currently detected. Connect the controller, then open Audio / MIDI.\n";
    for (const auto& port : ports)
        text += port.name + ": " + (!port.open ? "detected, input disabled" : !listening ? "input open, Alloy is not listening"
            : port.recent ? "receiving MIDI" : port.received ? "connected, input received earlier" : "connected, no messages received yet") + "\n";
    text += juce::String(mappings) + " active mappings\nProfile: " + profile.getFullPathName();
    if (profileError.isNotEmpty()) text += "\nProfile error: " + profileError;
    return text;
}
MidiConnectionStatus Engine::midiConnectionStatus()
{
    const auto available = juce::MidiInput::getAvailableDevices();
    MidiConnectionStatus status; status.listening = midiCallbackRegistered; status.profile = mappingsFile;
    const double now = juce::Time::getMillisecondCounterHiRes();
    const juce::ScopedLock guard(midiLock); status.mappings = (int) mappings.size(); status.profileError = midiProfileError;
    for (const auto& port : available)
    {
        const auto found = midiInputActivity.find(port.identifier);
        const bool received = found != midiInputActivity.end();
        const double age = received ? now - found->second : -1;
        status.ports.push_back({port.identifier, port.name, devices.isMidiInputDeviceEnabled(port.identifier), received, received && age >= 0 && age < 1500});
    }
    for (auto it = midiInputActivity.begin(); it != midiInputActivity.end();)
        if (std::none_of(available.begin(), available.end(), [&](const auto& port) { return port.identifier == it->first; })) it = midiInputActivity.erase(it);
        else ++it;
    return status;
}
}
