#pragma once
#include "Model.h"

namespace mixworks
{
struct RetryIssue
{
    std::shared_ptr<Media> media;
    juce::String reason;
};
juce::File retryReviewFile();
juce::var retryReviewRow(const Media& before, const Media* after, const juce::String& error);
juce::String retryResultText(const juce::var&);
juce::Result saveRetryReview(const juce::var&);
juce::String retryIssueText(const Media&, const juce::var& stored = {});
std::vector<RetryIssue> retryIssues(const std::vector<std::shared_ptr<Media>>&, const juce::var& live = {});
void clearRetryError(const Media&, bool readable, juce::Component* owner = nullptr);
juce::String lastRetryResult(const juce::var& live = {});
void publishRetryResult(juce::Component&, const juce::var&);

class RetryFailurePanel final : public juce::Component
{
public:
    explicit RetryFailurePanel(const std::vector<RetryIssue>&);
    std::function<void(juce::String)> onOpen;
    void resized() override;
    void paint(juce::Graphics&) override;
private:
    juce::Label heading, instruction, issue;
    juce::ComboBox choices;
    juce::TextButton open {"Open corrections"};
};
}
