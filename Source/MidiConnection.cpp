#include "Engine.h"
#include <juce_cryptography/juce_cryptography.h>

namespace mixworks
{
namespace
{
bool validTitle(const juce::String& title)
{
    if (title.length() > 128) return false;
    for (auto c : title) if (c < 32 || c == 127) return false;
    return true;
}
bool validReference(const juce::String& value)
{
    return value.isEmpty() || (value.length() <= 128 && value.endsWith(".json") && value != ".json"
        && value.containsOnly("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_. ")
        && !value.contains("..") && !value.startsWith("."));
}
struct ParsedMidiProfile { std::vector<MidiBinding> bindings; juce::String title, reference; };
juce::Result parseMidiProfile(const juce::var& data, ParsedMidiProfile& result, bool internal)
{
    if (data.isObject() && !data.isArray())
    {
        if (data["format"].toString() != "mixworks-midi-profile" || !data["version"].isInt() || (int) data["version"] != 1
            || !data["title"].isString() || !validTitle(data["title"].toString())) return juce::Result::fail("Invalid MIDI profile title or version.");
        result.title = data["title"].toString().trim();
        if (internal && data.hasProperty("savedProfile"))
        {
            if (!data["savedProfile"].isString() || !validReference(data["savedProfile"].toString())) return juce::Result::fail("Invalid saved-profile reference.");
            result.reference = data["savedProfile"].toString();
        }
    }
    const auto* entries = data.isArray() ? data.getArray() : data["mappings"].getArray();
    if (!entries || entries->size() > 4096) return juce::Result::fail("The profile must contain at most 4096 MIDI assignments.");
    std::set<int> messages; std::set<juce::String> actions;
    for (int index = 0; index < entries->size(); ++index)
    {
        const auto& value = entries->getReference(index);
        const auto invalid = [&] { return juce::Result::fail("Invalid or conflicting MIDI assignment " + juce::String(index + 1) + "."); };
        if (!value.isObject() || !value["status"].isInt() || !value["channel"].isInt() || !value["number"].isInt() || !value["action"].isString()) return invalid();
        const int status = value["status"], channel = value["channel"], number = value["number"];
        const auto action = parseMidiAction(value["action"].toString());
        if ((status != 0xb0 && status != 0x90) || channel < 1 || channel > 16 || number < 0 || number > 127 || !action.hasValue()) return invalid();
        if (!messages.insert(status * 65536 + channel * 256 + number).second || !actions.insert(midiActionId(*action)).second) return invalid();
        result.bindings.push_back({status, channel, number, *action});
    }
    return juce::Result::ok();
}
juce::var profileDocument(const std::vector<MidiBinding>& bindings, const juce::String& title, const juce::String& reference)
{
    juce::Array<juce::var> values;
    for (const auto& b : bindings)
    {
        auto* entry = new juce::DynamicObject(); entry->setProperty("status", b.status); entry->setProperty("channel", b.channel);
        entry->setProperty("number", b.number); entry->setProperty("action", midiActionId(b.action)); values.add(juce::var(entry));
    }
    if (title.isEmpty() && reference.isEmpty()) return juce::var(values);
    auto* document = new juce::DynamicObject(); document->setProperty("format", "mixworks-midi-profile"); document->setProperty("version", 1);
    document->setProperty("title", title); document->setProperty("mappings", values);
    if (reference.isNotEmpty()) document->setProperty("savedProfile", reference);
    return juce::var(document);
}
}
juce::Result Engine::midiProfileFailure(const juce::String& error)
{
    const juce::ScopedLock guard(midiLock); midiProfileError = error; return juce::Result::fail(error);
}
juce::File Engine::midiProfilesDirectory() const { return mappingsFile.getSiblingFile("MIDI Profiles"); }
juce::var Engine::midiProfileDocument(const juce::String& title, const juce::String& reference) { return profileDocument(mappings, title, reference); }
juce::String Engine::midiMappingName() { const juce::ScopedLock guard(midiLock); return midiProfileTitle; }
juce::File Engine::selectedMidiProfile()
{
    const juce::ScopedLock guard(midiLock);
    return midiNamedProfile.isEmpty() ? juce::File() : midiProfilesDirectory().getChildFile(midiNamedProfile);
}
juce::Array<juce::File> Engine::midiProfileFiles() const
{
    if (midiProfilesDirectory().isSymbolicLink()) return {};
    auto files = midiProfilesDirectory().findChildFiles(juce::File::findFiles, false, "*.json");
    files.removeIf([](const auto& f) { return f.isSymbolicLink() || f.getSize() > 1024 * 1024; }); files.sort(); return files;
}
void Engine::applyMidiProfile(std::vector<MidiBinding> loaded, const juce::String& title, const juce::String& reference)
{
    mappings = std::move(loaded); midiProfileTitle = title; midiNamedProfile = reference;
    pendingControls.clear(); pressedMidiButtons.clear(); mappingsDirty = false; gainSnapResetPending = true;
    learnAction.reset(); learnSteps.clear(); learnStep = 0; learnedControls.clear(); replaceControllerProfile = false; midiProfileError.clear();
    midiLearnMode = MidiLearnMode::off; midiLearnFeedback.clear();
}
juce::Result Engine::loadMidiProfileFile(const juce::File& file, bool activate, bool named)
{
    if (exporting || automationTake) return midiProfileFailure("Finish export or Record before loading mappings.");
    if (!file.existsAsFile() || file.getSize() > 1024 * 1024) return midiProfileFailure("Cannot read a MIDI profile of at most 1 MiB: " + file.getFullPathName());
    juce::FileInputStream stream(file);
    if (!stream.openedOk()) return midiProfileFailure("Cannot read " + file.getFullPathName());
    juce::MemoryBlock bytes; stream.readIntoMemoryBlock(bytes, 1024 * 1024 + 1);
    if (stream.getStatus().failed() || bytes.getSize() > 1024 * 1024) return midiProfileFailure("Could not read the complete MIDI profile.");
    const auto evidence = "Last profile read: " + file.getFullPathName() + " / Read " + juce::String((int64_t) bytes.getSize())
        + " bytes / SHA-256 " + juce::SHA256(bytes).toHexString();
    juce::var data; const auto json = juce::JSON::parse(juce::String::createStringFromData(bytes.getData(), (int) bytes.getSize()), data);
    ParsedMidiProfile parsed;
    const auto result = json.failed() ? json : parseMidiProfile(data, parsed, !activate);
    const juce::ScopedLock guard(midiLock); midiProfileReadEvidence = evidence;
    if (result.failed()) return midiProfileFailure(result.getErrorMessage() + " Active mappings were retained.");
    if (activate)
    {
        parsed.reference = named ? file.getFileName() : juce::String();
        const auto error = writeMidiProfileFile(mappingsFile, juce::JSON::toString(profileDocument(parsed.bindings, parsed.title, parsed.reference), false));
        if (error.isNotEmpty()) return midiProfileFailure(error + " Active mappings were retained.");
    }
    applyMidiProfile(std::move(parsed.bindings), parsed.title, parsed.reference); return juce::Result::ok();
}
juce::Result Engine::reloadMidiMappings() { return loadMidiProfileFile(mappingsFile, false); }
juce::Result Engine::openMidiProfile(const juce::File& file)
{
    if (midiProfilesDirectory().isSymbolicLink() || file.getParentDirectory() != midiProfilesDirectory() || !validReference(file.getFileName()) || file.isSymbolicLink())
        return midiProfileFailure("Choose a saved profile from the profile list.");
    return loadMidiProfileFile(file, true, true);
}
juce::Result Engine::importMidiProfile(const juce::File& file) { return loadMidiProfileFile(file, true); }
juce::Result Engine::renameMidiMapping(const juce::String& name)
{
    const juce::ScopedLock guard(midiLock);
    if (exporting || automationTake) return midiProfileFailure("Finish export or Record before naming mappings.");
    if (!validTitle(name)) return midiProfileFailure("Use a name of at most 128 characters without control characters.");
    const auto title = name.trim();
    const auto error = writeMidiProfileFile(mappingsFile, juce::JSON::toString(midiProfileDocument(title, midiNamedProfile), false));
    if (error.isNotEmpty()) return midiProfileFailure(error);
    midiProfileTitle = title; midiProfileError.clear(); return juce::Result::ok();
}
juce::Result Engine::saveMidiProfile(const juce::String& name, bool saveAs)
{
    const juce::ScopedLock guard(midiLock);
    if (exporting || automationTake) return midiProfileFailure("Finish export or Record before saving a profile.");
    const auto title = saveAs ? name.trim() : midiProfileTitle;
    if (!validTitle(title) || title.isEmpty()) return midiProfileFailure("Enter a profile name of 1–128 characters without control characters.");
    if (!saveAs && midiNamedProfile.isEmpty()) return midiProfileFailure("Use Save as to create a named profile first.");
    auto reference = saveAs ? juce::Uuid().toString() + ".json" : midiNamedProfile;
    const auto folder = midiProfilesDirectory();
    if (folder.isSymbolicLink() || folder.createDirectory().failed()) return midiProfileFailure("Cannot create the MIDI Profiles folder.");
    // Save as never overwrites a different profile, even when its display name matches.
    for (const auto& existing : midiProfileFiles())
        if (existing.getFileName() != reference && juce::JSON::parse(existing.loadFileAsString())["title"].toString().equalsIgnoreCase(title)) return midiProfileFailure("A saved profile already uses that name. Choose another name or open it and use Save.");
    const auto target = folder.getChildFile(reference);
    if (target.isSymbolicLink()) return midiProfileFailure("A saved profile cannot be a symbolic link.");
    const auto error = writeMidiProfileFile(target, juce::JSON::toString(midiProfileDocument(title, {}), false));
    if (error.isNotEmpty()) return midiProfileFailure(error);
    // Link persistence can fail independently. The valid named copy remains available to open.
    const auto activeError = writeMidiProfileFile(mappingsFile, juce::JSON::toString(midiProfileDocument(title, reference), false));
    if (activeError.isNotEmpty()) return midiProfileFailure(activeError + " The named copy was saved; active identity was retained.");
    midiProfileTitle = title; midiNamedProfile = reference; midiProfileError.clear(); return juce::Result::ok();
}
juce::Result Engine::exportMidiProfile(const juce::File& file)
{
    const juce::ScopedLock guard(midiLock);
    if (exporting || automationTake) return midiProfileFailure("Finish export or Record before exporting a profile.");
    if (!file.hasFileExtension("json")) return midiProfileFailure("Choose a destination with the .json extension.");
    if (file == mappingsFile || file.isAChildOf(midiProfilesDirectory()) || file.isSymbolicLink())
        return midiProfileFailure("Export to a separate JSON file outside the app's profile folder.");
    const auto error = writeMidiProfileFile(file, juce::JSON::toString(midiProfileDocument(midiProfileTitle, {}), false));
    if (error.isNotEmpty()) return midiProfileFailure(error);
    midiProfileError.clear(); return juce::Result::ok();
}
juce::Result Engine::deleteMidiProfile()
{
    const juce::ScopedLock guard(midiLock);
    if (exporting || automationTake) return midiProfileFailure("Finish export or Record before deleting a profile.");
    const auto selected = selectedMidiProfile();
    if (selected == juce::File() || !selected.existsAsFile() || selected.isSymbolicLink() || midiProfilesDirectory().isSymbolicLink())
        return midiProfileFailure("The selected saved profile is unavailable. Active mappings were retained.");
    const auto backup = selected.getSiblingFile(selected.getFileName() + ".deleted.bak").getNonexistentSibling();
    if (!selected.moveFileTo(backup)) return midiProfileFailure("Could not archive the saved profile. Active mappings were retained.");
    const auto error = writeMidiProfileFile(mappingsFile, juce::JSON::toString(midiProfileDocument(midiProfileTitle, {}), false));
    if (error.isNotEmpty())
    {
        const bool restored = backup.moveFileTo(selected);
        return midiProfileFailure(error + (restored ? " The saved profile was restored." : " Restore the saved profile from " + backup.getFullPathName()));
    }
    midiNamedProfile.clear(); midiProfileError.clear(); return juce::Result::ok();
}
juce::String Engine::midiProfilePersistenceState()
{
    const auto selected = selectedMidiProfile();
    if (selected == juce::File()) return "Named profile: none. Completed mappings autosave to the startup snapshot above; use Save as for a named copy.";
    const auto path = "Named profile: " + selected.getFullPathName();
    if (!selected.existsAsFile()) return path + " (missing; active assignments retained)";
    if (selected.getSize() > 1024 * 1024 || selected.isSymbolicLink() || midiProfilesDirectory().isSymbolicLink()) return path + " (unreadable or oversized; active assignments retained)";
    ParsedMidiProfile parsed; const auto result = parseMidiProfile(juce::JSON::parse(selected.loadFileAsString()), parsed, false);
    const bool same = result.wasOk() && juce::JSON::toString(profileDocument(parsed.bindings, parsed.title, {}), false)
        == juce::JSON::toString(midiProfileDocument(midiProfileTitle, {}), false);
    return path + (same ? " (matches active assignments)" : " (differs from active assignments; Save replaces this copy, Open reloads it)");
}
std::vector<MidiReconnectPolicy::Request> MidiReconnectPolicy::update(const std::vector<MidiPortConnection>& available)
{
    std::vector<Request> requests;
    for (auto& previous : known)
        if (previous.present && std::none_of(available.begin(), available.end(), [&](const auto& p) { return p.id == previous.id; }))
        {
            if (previous.wanted) requests.push_back({previous.id, false});
            previous.present = false;
        }
    for (const auto& port : available)
    {
        auto match = std::find_if(known.begin(), known.end(), [&](const auto& p) { return p.id == port.id; });
        if (match == known.end())
        {
            // Windows can change an identifier. Only reconnect by a unique exact name.
            auto candidate = known.end(); int remembered = 0;
            for (auto it = known.begin(); it != known.end(); ++it) if (!it->present && it->name == port.name) { candidate = it; ++remembered; }
            const auto presentNames = std::count_if(available.begin(), available.end(), [&](const auto& p) { return p.name == port.name; });
            if (remembered == 1 && presentNames == 1) { match = candidate; match->id = port.id; }
            else
            {
                const bool primary = port.name.containsIgnoreCase("FLkey") && !port.name.containsIgnoreCase("DAW") && !port.name.startsWithIgnoreCase("MIDIIN2");
                known.push_back({port.id, port.name, port.open || primary, false}); match = std::prev(known.end());
            }
        }
        if (match->present)
        {
            if (match->wanted && !port.open) requests.push_back({port.id, false});
            match->wanted = port.open; // Includes explicit disables in Audio / MIDI.
        }
        else if (match->wanted && !port.open) requests.push_back({port.id, true});
        match->present = true;
    }
    return requests;
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
    if (current == State::absent) return "Audio / MIDI";
    const auto found = std::find_if(ports.begin(), ports.end(), [current](const auto& p) { return p.open && (current != State::receiving || p.recent); });
    const auto fullName = found != ports.end() ? found->name : ports.front().name;
    const auto name = fullName.length() > 12 ? fullName.substring(0, 11) + "…" : fullName;
    const auto others = ports.size() > 1 ? " +" + juce::String((int) ports.size() - 1) : juce::String();
    return name + others + (current == State::receiving ? " / receiving" : current == State::connected ? " / connected" : " / detected");
}
juce::String MidiConnectionStatus::description() const
{
    juce::String text;
    if (ports.empty()) text = "No MIDI input is currently detected. Connect the controller, then open Audio / MIDI.\n";
    for (const auto& port : ports)
        text += port.name + ": " + (!port.open ? "detected, input disabled" : !listening ? "input open, Alloy is not listening"
            : port.recent ? "receiving MIDI" : port.received ? "connected, input received earlier" : "connected, no messages received yet") + "\n";
    text += "Mapping: " + mappingTitle + "\n" + juce::String(mappings) + " active mappings\nProfile: " + profile.getFullPathName();
    if (profileError.isNotEmpty()) text += "\nProfile error: " + profileError;
    return text;
}
MidiConnectionStatus Engine::midiConnectionStatus()
{
    const auto available = juce::MidiInput::getAvailableDevices();
    if (midiCallbackRegistered)
    {
        std::vector<MidiPortConnection> observed;
        for (const auto& port : available) observed.push_back({port.identifier, port.name, devices.isMidiInputDeviceEnabled(port.identifier)});
        for (const auto& request : midiReconnect.update(observed))
        {
            devices.setMidiInputDeviceEnabled(request.id, request.enabled);
            if (!request.enabled)
            {
                { const juce::ScopedLock guard(midiLock); midiInputActivity.erase(request.id); pressedMidiButtons.clear(); pendingControls.clear(); }
                // A removed input cannot deliver held-note releases. Queue local MIDI,
                // through the existing instrument path; never send controller output.
                for (int channel = 1; channel <= 16; ++channel)
                {
                    auto off = juce::MidiMessage::allNotesOff(channel);
                    off.setTimeStamp(juce::Time::getMillisecondCounterHiRes() / 1000.0); midiCollector.addMessageToQueue(off);
                }
            }
        }
    }
    MidiConnectionStatus status; status.listening = midiCallbackRegistered; status.profile = mappingsFile;
    const double now = juce::Time::getMillisecondCounterHiRes();
    const juce::ScopedLock guard(midiLock); status.mappingTitle = activeMappingTitle(); status.mappings = (int) mappings.size(); status.profileError = midiProfileError;
    for (const auto& port : available)
    {
        const bool open = devices.isMidiInputDeviceEnabled(port.identifier);
        if (!open) midiInputActivity.erase(port.identifier);
        const auto found = midiInputActivity.find(port.identifier);
        const bool received = found != midiInputActivity.end();
        const double age = received ? now - found->second : -1;
        status.ports.push_back({port.identifier, port.name, open, received, received && age >= 0 && age < 1500});
    }
    for (auto it = midiInputActivity.begin(); it != midiInputActivity.end();)
        if (std::none_of(available.begin(), available.end(), [&](const auto& port) { return port.identifier == it->first; })) it = midiInputActivity.erase(it);
        else ++it;
    return status;
}
}
