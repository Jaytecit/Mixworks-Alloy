#include "Workstation.h"
#include "Beatmatch.h"
#include <juce_cryptography/juce_cryptography.h>
#include <cmath>

namespace mixworks
{
void runCacheSettingsTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    const auto folder = report.getParentDirectory().getChildFile("cache-settings-" + juce::Uuid().toString());
    folder.createDirectory();
    const auto profile = folder.getChildFile("location.json"), oldRoot = folder.getChildFile("retained"), newRoot = folder.getChildFile("expanded");
    auto settings = PcmStore::readSettings(profile, oldRoot);
    check(settings.error.isEmpty() && settings.root == oldRoot && settings.budget == 128LL * 1024 * 1024 * 1024,
          "missing cache profile preserves the default location with a 128 GiB budget");
    juce::DynamicObject::Ptr object = new juce::DynamicObject();
    object->setProperty("format", "mixworks-audio-cache-location"); object->setProperty("version", 1);
    object->setProperty("root", newRoot.getFullPathName()); object->setProperty("retainedRoot", oldRoot.getFullPathName());
    object->setProperty("budgetGiB", 128);
    const auto saveProfile = [&] { return profile.replaceWithText(juce::JSON::toString(juce::var(object.get()))); };
    const bool saved = saveProfile(); settings = PcmStore::readSettings(profile, oldRoot);
    check(saved && settings.error.isEmpty() && settings.root == newRoot && settings.retainedRoot == oldRoot
          && settings.budget == 128LL * 1024 * 1024 * 1024 && !oldRoot.exists() && !newRoot.exists(),
          "valid location settings select the expanded cache without changing any existing data");
    bool invalid = true;
    for (const auto& field : {juce::String("root"), juce::String("retainedRoot"), juce::String("budgetGiB"), juce::String("version")})
    {
        const auto previous = object->getProperty(field);
        object->setProperty(field, "invalid"); invalid &= saveProfile();
        const auto bytes = profile.loadFileAsString();
        invalid &= PcmStore::readSettings(profile, oldRoot).error.isNotEmpty() && profile.loadFileAsString() == bytes;
        object->setProperty(field, previous);
    }
    check(invalid && !oldRoot.exists() && !newRoot.exists(), "invalid paths and numeric settings are rejected without modifying caches or settings");
    check(PcmStore::shared().directory() != PcmStore::defaultRoot()
          && PcmStore::shared().limitBytes() == PcmStore::defaultBudgetBytes,
          "explicit isolated test roots do not use the production cache profile");
    PcmStore oldStore(oldRoot, 1024 * 1024);
    juce::AudioBuffer<float> audio(2, 1024); audio.clear(); audio.setSample(0, 123, 0.375f); audio.setSample(1, 321, -0.25f);
    const auto first = juce::String::repeatedString("a", 64), late = juce::String::repeatedString("b", 64), fresh = juce::String::repeatedString("c", 64);
    juce::String warning;
    auto seed = oldStore.write(first, 48000, audio, warning);
    check(seed && warning.isEmpty(), "small existing PCM fixture uses the unchanged cache format");
    if (!seed) return;
    seed.reset();
    const auto originalHash = juce::SHA256(oldStore.entryFile(first)).toHexString();
    PcmStore expanded(settings.root, settings.budget, settings.retainedRoot);
    const bool copied = oldStore.entryFile(first).copyFileTo(expanded.entryFile(first));
    auto reused = expanded.read(first, 48000, 2, 1024);
    check(copied && reused && reused->file == expanded.entryFile(first)
          && reused->channelData(0)[123] == 0.375f && reused->channelData(1)[321] == -0.25f
          && juce::SHA256(oldStore.entryFile(first)).toHexString() == originalHash,
          "copied audio is reused exactly from the expanded cache while the original is preserved");
    auto lateSeed = oldStore.write(late, 48000, audio, warning); lateSeed.reset();
    const auto lateHash = juce::SHA256(oldStore.entryFile(late)).toHexString();
    auto lateRead = expanded.read(late, 48000, 2, 1024);
    auto lateWrite = expanded.write(late, 48000, audio, warning);
    check(lateRead && lateWrite && lateRead->file == oldStore.entryFile(late) && lateWrite->file == oldStore.entryFile(late)
          && !expanded.entryFile(late).exists() && juce::SHA256(oldStore.entryFile(late)).toHexString() == lateHash,
          "entries completed by the previous scan are reused read-only without redecoding or rewriting");
    auto added = expanded.write(fresh, 48000, audio, warning);
    check(added && warning.isEmpty() && added->file == expanded.entryFile(fresh) && !oldStore.entryFile(fresh).exists(),
          "new audio writes go exclusively to the expanded cache");
    const auto marker = oldRoot.getChildFile(".mixworks-cache");
    check(marker.replaceWithText("unidentified cache") && !expanded.read(late, 48000, 2, 1024)
          && juce::SHA256(oldStore.entryFile(late)).toHexString() == lateHash,
          "retained-cache ownership is checked before reuse and rejected files remain untouched");
}
void runOriginalPlaybackTests(const std::function<void(bool,const juce::String&)>&,const juce::File&);
void runPlaybackCacheTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    // The former persistent-PCM workflows were removed: playback and tempo
    // changes now read originals through a shared bounded pool.
    runOriginalPlaybackTests(check, report);
}
}
