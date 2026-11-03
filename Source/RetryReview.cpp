#include "RetryReview.h"
#include "UiMessages.h"

namespace mixworks
{
namespace
{
juce::CriticalSection reviewMutex;
bool matches(const juce::var& row, const Media& media)
{ return row.isObject() && juce::File(row["path"].toString()) == media.file && row["sha256"].toString() == media.analysisSource.sha256; }
bool sameRow(const juce::var& a, const juce::var& b)
{ return a["path"] == b["path"] && a["sha256"] == b["sha256"]; }
juce::var loadReview()
{
    const auto file = retryReviewFile();
    if (!file.existsAsFile() || file.getSize() > 8 * 1024 * 1024) return {};
    auto doc = juce::JSON::parse(file);
    return doc["format"].toString() == "mixworks-retry-review" && (int) doc["version"] == 1 ? doc : juce::var();
}
juce::Result writeReview(const juce::var& doc)
{
    const auto file = retryReviewFile();
    if (file.getParentDirectory().createDirectory().failed()) return juce::Result::fail("Could not save the retry list.");
    juce::TemporaryFile temporary(file);
    const auto text = juce::JSON::toString(doc);
    if (text.getNumBytesAsUTF8() > 8 * 1024 * 1024) return juce::Result::fail("The retry list is full.");
    if (!temporary.getFile().replaceWithText(text, false, false, "\n") || !temporary.overwriteTargetFileWithTemporary())
        return juce::Result::fail("Could not save the retry list.");
    return juce::Result::ok();
}
juce::String reviewFields(const Media& media)
{
    const auto& a = media.analysis;
    const bool tempo = media.manualBpm > 0 || (std::isfinite(a.bpm) && a.bpm > 0 && !a.variableTempo && std::isfinite(a.tempoConfidence) && a.tempoConfidence >= .2 && a.rhythmIssue.isEmpty());
    const bool bar = media.manualBeatOrigin >= 0 || (std::isfinite(a.downbeatOrigin) && a.downbeatOrigin >= 0);
    const bool key = media.manualKey >= 0 || (a.key >= 0 && a.key < 24 && std::isfinite(a.keyConfidence) && a.keyConfidence <= 1 && a.keyConfidence >= keyReviewConfidence);
    if ((!tempo || !bar) && !key) return "BPM/grid and key need review";
    if (!tempo || !bar) return "BPM/grid needs review";
    return !key ? "Key needs review" : juce::String();
}
}
juce::File retryReviewFile() { return analysisStoreRoot().getSiblingFile("FocusedRetryReview-v1.json"); }
juce::String retryIssueText(const Media& media, const juce::var& stored)
{
    if (matches(stored, media))
    {
        if ((bool) stored["readError"]) return "File could not be read";
        if ((bool) stored["saveError"]) return "Results could not be saved";
    }
    const bool attempted = matches(stored, media) || (media.analysis.focusedRevision > 0 && (media.analysis.focusedTempo > 0 || media.analysis.focusedKey > 0));
    return attempted ? reviewFields(media) : juce::String();
}
juce::var retryReviewRow(const Media& before, const Media* after, const juce::String& error)
{
    const auto& value = after ? *after : before;
    juce::var row(new juce::DynamicObject()); auto* object = row.getDynamicObject();
    object->setProperty("path", value.file.getFullPathName()); object->setProperty("sha256", value.analysisSource.sha256);
    object->setProperty("name", value.name.isNotEmpty() ? value.name : value.file.getFileName());
    object->setProperty("readError", after == nullptr); object->setProperty("saveError", after && value.analysisCacheWarning.isNotEmpty());
    object->setProperty("details", after ? value.analysisCacheWarning : error);
    object->setProperty("checkedUtc", juce::Time::getCurrentTime().toISO8601(true));
    object->setProperty("issue", retryIssueText(value, row));
    const auto& a = value.analysis;
    const auto bpm = value.manualBpm > 0 ? "manual BPM kept" : value.manualBeatOrigin >= 0 ? "manual grid kept"
        : !needsFocusedTempo(before.analysis) ? "BPM kept" : a.focusedTempo == 2 ? "BPM improved" : "BPM unchanged";
    const auto key = value.manualKey >= 0 ? "manual key kept" : !needsFocusedKey(before.analysis) ? "key kept"
        : a.focusedKey == 2 ? "key improved" : "key unchanged";
    const auto issue = row["issue"].toString();
    object->setProperty("result", (bool) row["readError"] || (bool) row["saveError"] ? issue
        : (issue.contains("BPM/grid") ? juce::String("BPM/grid needs review") : juce::String(bpm)) + "; "
            + (issue.containsIgnoreCase("key") ? juce::String("key needs review") : juce::String(key)));
    return row;
}
void publishRetryResult(juce::Component& owner, const juce::var& row)
{
    auto& properties = owner.getProperties(); auto current = properties["focusedRetryRows"];
    auto rows = current.isArray() ? *current.getArray() : juce::Array<juce::var>();
    for (int i = rows.size(); --i >= 0;) if (sameRow(rows[i], row)) rows.remove(i);
    rows.add(row); properties.set("focusedRetryRows", rows); properties.set("focusedLastResult", row);
}
juce::String retryResultText(const juce::var& row)
{ return row.isObject() ? "Last retry: " + shortUiMessage(row["name"].toString(), 10, 60) + " - " + row["result"].toString() + "." : juce::String(); }
juce::Result saveRetryReview(const juce::var& row)
{
    const juce::ScopedLock guard(reviewMutex); auto doc = loadReview();
    if (!doc.isObject() && retryReviewFile().existsAsFile()) return juce::Result::fail("The saved retry list could not be opened.");
    if (!doc.isObject()) { doc = juce::var(new juce::DynamicObject()); doc.getDynamicObject()->setProperty("format", "mixworks-retry-review"); doc.getDynamicObject()->setProperty("version", 1); }
    auto rows = doc["rows"].isArray() ? *doc["rows"].getArray() : juce::Array<juce::var>();
    for (int i = rows.size(); --i >= 0;) if (sameRow(rows[i], row)) rows.remove(i);
    rows.add(row); doc.getDynamicObject()->setProperty("rows", rows); doc.getDynamicObject()->setProperty("last", row);
    return writeReview(doc);
}
juce::String lastRetryResult(const juce::var& live)
{
    if (live.isObject()) return retryResultText(live);
    const juce::ScopedLock guard(reviewMutex); return retryResultText(loadReview()["last"]);
}
std::vector<RetryIssue> retryIssues(const std::vector<std::shared_ptr<Media>>& sources, const juce::var& live)
{
    const juce::ScopedLock guard(reviewMutex); auto doc = loadReview();
    auto rows = doc["rows"].isArray() ? *doc["rows"].getArray() : juce::Array<juce::var>();
    if (live.isArray()) for (const auto& row : *live.getArray())
    {
        const bool exists = std::any_of(rows.begin(), rows.end(), [&](const auto& item) { return sameRow(item, row); });
        if ((bool) row["volatile"] || !exists)
        { for (int i = rows.size(); --i >= 0;) if (sameRow(rows[i], row)) rows.remove(i); rows.add(row); }
    }
    std::vector<RetryIssue> result;
    for (const auto& media : sources)
    {
        if (!media || std::any_of(result.begin(), result.end(), [&](const auto& previous)
            { return previous.media->file == media->file && previous.media->analysisSource == media->analysisSource; })) continue;
        juce::var stored; for (const auto& row : rows) if (matches(row, *media)) stored = row;
        const auto issue = retryIssueText(*media, stored); if (issue.isNotEmpty()) result.push_back({media, issue});
    }
    return result;
}
void clearRetryError(const Media& media, bool readable, juce::Component* owner)
{
    const auto flag = readable ? "readError" : "saveError";
    if (owner)
    {
        auto live = owner->getProperties()["focusedRetryRows"];
        if (live.isArray()) for (auto& row : *live.getArray())
            if (matches(row, media)) row.getDynamicObject()->setProperty(flag, false);
    }
    const juce::ScopedLock guard(reviewMutex); auto doc = loadReview(); if (!doc["rows"].isArray()) return;
    bool changed = false;
    for (auto& row : *doc["rows"].getArray()) if (matches(row, media) && (bool) row[flag])
    { row.getDynamicObject()->setProperty(flag, false); changed = true; }
    if (changed) writeReview(doc);
}
RetryFailurePanel::RetryFailurePanel(const std::vector<RetryIssue>& rows)
{
    setSize(720, 220); for (auto* c : std::initializer_list<juce::Component*> {&heading, &instruction, &issue, &choices, &open}) addAndMakeVisible(c);
    heading.setText("SOURCE CORRECTIONS / RETRY FAILURES", juce::dontSendNotification);
    instruction.setText("Choose a track. Unreadable files must be found or imported again.", juce::dontSendNotification);
    for (size_t i = 0; i < rows.size(); ++i) choices.addItem(shortUiMessage(rows[i].media->name, 10, 65) + " - " + rows[i].reason, (int) i + 1);
    choices.setTextWhenNothingSelected("Choose a failed track");
    choices.onChange = [this, rows]
    { const int i = choices.getSelectedItemIndex(); if (i >= 0) { issue.setText(rows[(size_t)i].reason + ".", juce::dontSendNotification); open.setEnabled(rows[(size_t)i].media->duration() > 0); } };
    open.setEnabled(false); open.onClick = [this, rows] { const int i = choices.getSelectedItemIndex(); if (i >= 0 && onOpen) onOpen(rows[(size_t)i].media->id); };
}
void RetryFailurePanel::paint(juce::Graphics& g) { g.fillAll(juce::Colour(0xff15191d)); }
void RetryFailurePanel::resized()
{
    auto r = getLocalBounds().reduced(18); heading.setBounds(r.removeFromTop(28)); instruction.setBounds(r.removeFromTop(32));
    choices.setBounds(r.removeFromTop(30)); issue.setBounds(r.removeFromTop(32)); open.setBounds(r.removeFromBottom(30).removeFromRight(170));
}
}
