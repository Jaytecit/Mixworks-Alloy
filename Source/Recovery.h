#pragma once
#include "Model.h"

namespace mixworks
{
struct RecoveryCandidate
{
    juce::File folder, sessionFile;
    juce::String document, fingerprint, error, timestamp;
    bool previous = false;
};
// One lifetime lease per window. No PID guesses and no shared autosave filename.
// Acquire/release on the message thread; one worker may write while leased.
class RecoveryStore
{
public:
    static std::unique_ptr<RecoveryStore> create(const juce::File& root, juce::String& error);
    static std::unique_ptr<RecoveryStore> claim(const juce::File& folder, juce::String& error);
    static std::vector<RecoveryCandidate> scan(const juce::File& root);
    ~RecoveryStore();
    RecoveryCandidate read() const;
    juce::Result write(const juce::String& document, const juce::File& sessionFile);
    juce::Result discard(const juce::String& expectedFingerprint = {});
    juce::File directory() const { return folder; }
private:
    explicit RecoveryStore(juce::File);
    juce::File folder;
    juce::String id;
    juce::InterProcessLock lease;
    bool held = false;
};
void runRecoveryTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
}
