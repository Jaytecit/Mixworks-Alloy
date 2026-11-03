#include "EarTools.h"

namespace mixworks
{
namespace
{
void earSlider(juce::Slider& slider, const juce::String& name, double low, double high, double step)
{
    slider.setName(name); slider.setRange(low, std::max(low + step, high), step);
    slider.setSliderStyle(juce::Slider::LinearHorizontal); slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 95, 26);
}
}
KeyEarEditor::KeyEarEditor(Engine& engine, LibraryPreview& route, std::shared_ptr<const Media> media, double at,
    std::function<juce::Result(int)> saveAction)
    : preview(engine, route, player), passage(std::move(media)), choice(passage.media->key()), saveKey(std::move(saveAction))
{
    setSize(900, 640);
    for (auto* c : std::initializer_list<juce::Component*> {&preview, &passage, &heading, &instructions, &result, &status,
        &sound1, &sound2, &prefer1, &prefer2, &unsure, &other, &useHome, &back, &restart, &previous, &next, &save, &cancel,
        &details, &hear, &registerChoice, &begin, &end, &referenceLevel}) addAndMakeVisible(c);
    heading.setText("KEY BY EAR  /  " + passage.media->name, juce::dontSendNotification);
    heading.setFont(juce::FontOptions(17)); instructions.setFont(juce::FontOptions(13));
    result.setFont(juce::FontOptions(14)); status.setFont(juce::FontOptions(12));
    earSlider(begin, "Loop start in source seconds", 0, passage.media->duration() - .1, .001); begin.setTextValueSuffix(" s start");
    earSlider(end, "Loop end in source seconds", .1, passage.media->duration(), .001); end.setTextValueSuffix(" s end");
    earSlider(referenceLevel, "Reference sound level", -36, -6, .5); referenceLevel.setValue(-18, juce::dontSendNotification); referenceLevel.setTextValueSuffix(" dB reference");
    hear.addItemList({"Track", "Reference", "Together"}, 1); hear.setSelectedId(3, juce::dontSendNotification);
    registerChoice.addItemList({"Low reference", "Middle reference", "High reference"}, 1); registerChoice.setSelectedId(2, juce::dontSendNotification);
    begin.onValueChange = end.onValueChange = [this] { if (!updating) setPassage(begin.getValue(), end.getValue()); };
    passage.onChange = [this] { setPassage(passage.range.getStart(), passage.range.getEnd()); };
    hear.onChange = [this] { player.hear(hear.getSelectedId() - 1); };
    registerChoice.onChange = referenceLevel.onValueChange = [this] { audition(sound); };
    preview.feedback = [this](const auto& text) { status.setText(text, juce::dontSendNotification); };
    sound1.onClick = [this] { audition(1); }; sound2.onClick = [this] { audition(2); };
    prefer1.onClick = [this] { prefer(1); }; prefer2.onClick = [this] { prefer(2); };
    unsure.onClick = [this]
    {
        if (choice.selected >= 0) { history.push_back(choice); choice.selected = -1; choice.stage = 1; refresh(); }
        status.setText("No choice recorded. Try another passage, adjust the reference level, or leave the key unconfirmed.", juce::dontSendNotification);
    };
    other.onClick = [this]
    {
        history.push_back(choice);
        if (choice.stage == 0) { do { if (++choice.next >= 12) choice.next = 1; } while (choice.candidate(2) == choice.winner); }
        else { choice.stage = 0; choice.next = 1; choice.selected = -1; }
        refresh(); audition(2);
    };
    useHome.onClick = [this]
    {
        if (choice.stage != 0) return;
        history.push_back(choice); choice.winner = choice.candidate(sound); choice.stage = 1; choice.selected = -1; refresh(); audition(1);
    };
    useHome.setTooltip("Keep the reference tone you are hearing as the home sound and move on to chord comparison. This is your listening choice.");
    back.onClick = [this] { if (!history.empty()) { choice = history.back(); history.pop_back(); refresh(); audition(1); } };
    restart.onClick = [this] { history.push_back(choice); choice = EarKeyChoice(); refresh(); audition(1); };
    details.onClick = [this] { refresh(); };
    previous.onClick = [this] { const double length = passage.range.getLength(); const double start = std::max(0., passage.range.getStart() - length); setPassage(start, start + length); };
    next.onClick = [this] { const double length = passage.range.getLength(); const double start = std::min(std::max(0., passage.media->duration() - length), passage.range.getStart() + length); setPassage(start, start + length); };
    cancel.onClick = [this] { close(); if (auto* window = findParentComponentOfClass<juce::DialogWindow>()) window->closeButtonPressed(); };
    save.onClick = [this]
    {
        if (choice.selected < 0) return;
        preview.stop(); const auto saved = saveKey(choice.selected);
        if (saved.failed()) { status.setText(saved.getErrorMessage(), juce::dontSendNotification); return; }
        status.setText("Source key saved. Pitch, BPM and arrangement timing were kept.", juce::dontSendNotification);
        if (auto* window = findParentComponentOfClass<juce::DialogWindow>()) window->closeButtonPressed();
    };
    save.setTooltip("Save only this source's manually chosen key. Timing corrections and segment pitch stay unchanged.");
    sound1.setTooltip("Audition the first reference sound over the same loop. Play / Stop controls the preview.");
    sound2.setTooltip("Audition the second reference sound over the same loop.");
    setPassage(at, at + 8); refresh(); audition(1);
}
KeyEarEditor::~KeyEarEditor() { close(); }
void KeyEarEditor::close() { preview.stop(); }
void KeyEarEditor::setPassage(double a, double b)
{
    const double duration = passage.media->duration(), minimum = std::min(.1, duration);
    a = juce::jlimit(0., std::max(0., duration - minimum), a); b = juce::jlimit(a + minimum, duration, b);
    updating = true; begin.setValue(a, juce::dontSendNotification); end.setValue(b, juce::dontSendNotification); updating = false;
    passage.range = {a, b}; passage.repaint(); player.source(passage.media, a, b);
}
void KeyEarEditor::audition(int selected)
{
    sound = selected; player.reference(choice.candidate(sound), choice.stage != 0, registerChoice.getSelectedId() + 1, (float) referenceLevel.getValue());
    player.hear(hear.getSelectedId() - 1);
    sound1.setToggleState(sound == 1, juce::dontSendNotification); sound2.setToggleState(sound == 2, juce::dontSendNotification);
}
void KeyEarEditor::prefer(int selected)
{
    history.push_back(choice); choice.prefer(selected); refresh(); audition(choice.stage == 2 && choice.selected >= 12 ? 2 : 1);
}
void KeyEarEditor::refresh()
{
    instructions.setText(choice.stage == 0 ? "Find the home sound. Compare Sound 1 and Sound 2: which feels more settled against the music?"
        : choice.stage == 1 ? "Compare two chord sounds. Which fits the track's overall character? Check more than one passage."
        : "Your listening choice is ready. Check another passage before saving; Unsure is a valid outcome.", juce::dontSendNotification);
    const auto name = [this](int which)
    { return details.getToggleState() ? (choice.stage == 0 ? keyCompactName(choice.candidate(which) % 12) : keyNoteName(choice.candidate(which))) : "Sound " + juce::String(which); };
    sound1.setButtonText(name(1)); sound2.setButtonText(name(2));
    result.setText(choice.selected < 0 ? "No key chosen yet. A reference fitting one passage does not prove the song's key."
        : "Your choice: " + keyNoteName(choice.selected) + " / " + keyCamelot(choice.selected) + "  /  manual listening choice", juce::dontSendNotification);
    save.setEnabled(choice.selected >= 0); back.setEnabled(!history.empty()); useHome.setEnabled(choice.stage == 0);
}
void KeyEarEditor::paint(juce::Graphics& g) { g.fillAll(colour::background); }
void KeyEarEditor::resized()
{
    auto r = getLocalBounds().reduced(16); heading.setBounds(r.removeFromTop(30)); instructions.setBounds(r.removeFromTop(42));
    passage.setBounds(r.removeFromTop(130)); begin.setBounds(r.removeFromTop(29)); end.setBounds(r.removeFromTop(29));
    auto passages = r.removeFromTop(29); previous.setBounds(passages.removeFromLeft(160).reduced(2, 0)); next.setBounds(passages.removeFromLeft(160).reduced(2, 0)); details.setBounds(passages);
    r.removeFromTop(12); auto sounds = r.removeFromTop(34);
    sound1.setBounds(sounds.removeFromLeft(150).reduced(2, 0)); sound2.setBounds(sounds.removeFromLeft(150).reduced(2, 0));
    hear.setBounds(sounds.removeFromLeft(125).reduced(2, 0)); registerChoice.setBounds(sounds.removeFromLeft(155).reduced(2, 0)); referenceLevel.setBounds(sounds);
    r.removeFromTop(5); auto choices = r.removeFromTop(32);
    for (auto* b : {&prefer1, &prefer2, &useHome, &unsure, &other, &back, &restart}) b->setBounds(choices.removeFromLeft(120).reduced(2, 0));
    result.setBounds(r.removeFromTop(40)); preview.setBounds(r.removeFromTop(53)); status.setBounds(r.removeFromTop(39));
    auto footer = r.removeFromBottom(32); cancel.setBounds(footer.removeFromRight(110)); footer.removeFromRight(8); save.setBounds(footer.removeFromRight(180));
}
}
