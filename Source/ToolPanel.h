#pragma once
#include "AttractLights.h"

namespace mixworks
{
class PanelViewport final : public juce::Viewport
{
public:
    std::function<void()> onResize;
    void resized() override { juce::Viewport::resized(); if (onResize) onResize(); }
};
class PanelDockButton final : public juce::Button
{
public:
    explicit PanelDockButton(const juce::String& panel) : Button(panel), panelName(panel) { setButtonText({}); setDetached(false); }
    void setDetached(bool value)
    {
        detached = value;
        const auto action = juce::String(detached ? "Attach " : "Detach ") + panelName;
        setName(action); setTitle(action); setTooltip(action); repaint();
    }
    void paintButton(juce::Graphics& g, bool hover, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced(2);
        juce::Path hex; hex.addPolygon(r.getCentre(), 6, std::min(r.getWidth(), r.getHeight()) * .5f);
        hex = hex.createPathWithRoundedCorners(2);
        g.setColour(down ? colour::orange.withAlpha(.4f) : hover ? colour::raised.brighter(.2f) : colour::raised); g.fillPath(hex);
        g.setColour(hasKeyboardFocus(true) ? colour::focus : hover ? colour::orange : colour::border); g.strokePath(hex, juce::PathStrokeType(1.2f));
        g.setColour(isEnabled() ? colour::text : colour::muted.withAlpha(.5f));
        const auto c = r.getCentre();
        g.drawRoundedRectangle(c.x - 6, c.y - 3, 9, 9, 1, 1.3f);
        juce::Path arrow;
        if (detached)
        { arrow.startNewSubPath(c.x + 6, c.y - 6); arrow.lineTo(c.x, c.y); arrow.startNewSubPath(c.x, c.y - 4); arrow.lineTo(c.x, c.y); arrow.lineTo(c.x + 4, c.y); }
        else
        { arrow.startNewSubPath(c.x, c.y); arrow.lineTo(c.x + 6, c.y - 6); arrow.startNewSubPath(c.x + 1, c.y - 6); arrow.lineTo(c.x + 6, c.y - 6); arrow.lineTo(c.x + 6, c.y - 1); }
        g.strokePath(arrow, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
private:
    juce::String panelName;
    bool detached = false;
};

// A single content instance follows its frame between the workspace and native window.
class ToolPanel final : public juce::Component
{
public:
    ToolPanel(juce::Component& body, const juce::String& name) : dock(name), content(body)
    {
        getProperties().set("alloyAttractOwned", true);
        title.setText(name, juce::dontSendNotification); title.setFont(juce::FontOptions(13).withStyle("Bold"));
        addAndMakeVisible(title); addAndMakeVisible(dock); addAndMakeVisible(content);
    }
    std::function<bool(const juce::KeyPress&)> onKey;
    bool keyPressed(const juce::KeyPress& key) override { return onKey && onKey(key); }
    void setAttractState(const AttractState& state)
    {
        lights = std::make_unique<AttractLights>(*this, state); addChildComponent(*lights); resized(); lights->syncMode();
    }
    void syncAttract() { if (lights) lights->syncMode(); }
    void refreshAttract() { if (lights && isShowing()) lights->repaint(); }
    void resized() override
    {
        auto area = getLocalBounds().reduced(6, 0); auto header = area.removeFromTop(32);
        dock.setBounds(header.removeFromRight(32)); title.setBounds(header.reduced(6, 0)); content.setBounds(area);
        if (lights) { lights->setBounds(getLocalBounds()); lights->toFront(false); }
    }
    void paint(juce::Graphics& g) override
    { g.fillAll(colour::panel); g.setColour(colour::border); g.drawHorizontalLine(31, 6, (float) getWidth() - 6); }
    PanelDockButton dock;
private:
    juce::Label title;
    juce::Component& content;
    std::unique_ptr<AttractLights> lights;
};
}
