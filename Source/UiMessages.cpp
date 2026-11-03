#include "UiMessages.h"

namespace mixworks
{
juce::String shortUiMessage(const juce::String& input, int words, int characters)
{
    const auto parts = juce::StringArray::fromTokens(input.replaceCharacters("\r\n\t", "   "), false);
    juce::String text = parts.joinIntoString(" ").trim();
    int sentences = 0, end = text.length();
    for (int i = 0; i < text.length(); ++i)
        if ((text[i] == '.' || text[i] == '?' || text[i] == '!') && (i + 1 == text.length() || juce::CharacterFunctions::isWhitespace(text[i + 1])))
            if (++sentences == 2) { end = i + 1; break; }
    text = text.substring(0, end);
    const auto tokens = juce::StringArray::fromTokens(text, false);
    if (tokens.size() > words) text = tokens.joinIntoString(" ", 0, words) + "...";
    if (text.length() > characters)
    {
        text = text.substring(0, characters - 3); const int space = text.lastIndexOfChar(' ');
        if (space > characters / 2) text = text.substring(0, space);
        text += "...";
    }
    return text;
}
void showUiDetails(juce::Component& parent, const juce::String& title, const juce::String& details)
{
    class DetailsWindow final : public juce::AlertWindow
    {
    public:
        DetailsWindow(juce::Component& owner, const juce::String& heading, const juce::String& contents)
            : AlertWindow(heading, {}, juce::MessageBoxIconType::NoIcon, &owner)
        {
            text.setMultiLine(true); text.setReadOnly(true); text.setScrollbarsShown(true); text.setSize(560, 260);
            text.setText(contents, false); text.setCaretPosition(0); text.setTooltip("Read or copy the full details.");
            addCustomComponent(&text); addButton("Close", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        }
    private:
        juce::TextEditor text;
    };
    auto* dialog = new DetailsWindow(parent, title, details);
    dialog->enterModalState(true, nullptr, true);
}
}
