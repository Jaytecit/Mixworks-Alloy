#include "AlloyLookAndFeel.h"
#include <cmath>
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#endif

namespace mixworks
{
bool AlloyLookAndFeel::editingSliderText(const juce::Slider& slider)
{
    for (int i = 0; i < slider.getNumChildComponents(); ++i)
        if (const auto* value = dynamic_cast<const juce::Label*>(slider.getChildComponent(i)); value && value->isBeingEdited()) return true;
    return false;
}

AlloyBeatPulse AlloyLookAndFeel::fourCountPulse(double beats, bool playing, bool animate)
{
    if (!std::isfinite(beats) || beats < 0) return {};
    const double phase = beats - std::floor(beats);
    const int count = (int) std::fmod(std::floor(beats), 4.0);
    // One warm surge per musical beat; the first of four is accented. The
    // opaque outline never disappears, so the effect doesn't flash on/off.
    const float heat = playing && animate ? .24f + .76f * (count == 0 ? 1.f : .72f) * (float) std::exp(-9 * phase) : .36f;
    return {count, heat};
}
bool AlloyLookAndFeel::motionAllowed()
{
#if JUCE_WINDOWS
    // UI-only, cached across paint calls. Re-read so a live Windows preference
    // change takes effect without adding a persistent application setting.
    static double checked = -2000;
    static bool allowed = true;
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (now - checked >= 1000)
    {
        BOOL enabled = TRUE;
        if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0)) allowed = enabled != FALSE;
        checked = now;
    }
    return allowed;
#else
    return true;
#endif
}
juce::Colour AlloyLookAndFeel::metalHeat(float amount)
{
    amount = std::isfinite(amount) ? juce::jlimit(0.f, 1.f, amount) : 0;
    if (amount < .45f) return colour::moltenEdge.interpolatedWith(colour::orange, amount / .45f);
    if (amount < .82f) return colour::orange.interpolatedWith(colour::moltenOrange, (amount - .45f) / .37f);
    return colour::moltenOrange.interpolatedWith(colour::moltenCore, (amount - .82f) / .18f);
}
void AlloyLookAndFeel::drawMoltenMetal(juce::Graphics& g, juce::Rectangle<float> bounds, bool vertical, float heat)
{
    if (bounds.isEmpty()) return;
    heat = juce::jlimit(0.f, 1.f, heat);
    const float radius = std::min(4.f, std::min(bounds.getWidth(), bounds.getHeight()) * .5f);
    const auto hot = metalHeat(heat);
    for (int layer = 3; layer > 0; --layer)
    {
        g.setColour(hot.withAlpha(heat * heat * .04f * (4 - layer)));
        g.drawRoundedRectangle(bounds.expanded((float) layer), radius + layer, 1.5f);
    }
    const auto first = bounds.getTopLeft();
    const auto last = vertical ? bounds.getTopRight() : bounds.getBottomLeft();
    juce::ColourGradient metal(colour::moltenEdge, first, colour::moltenEdge.darker(.4f), last, false);
    metal.addColour(.22, hot.darker(.3f));
    metal.addColour(.43, hot);
    metal.addColour(.51, hot.interpolatedWith(colour::moltenCore, heat * .6f));
    metal.addColour(.63, hot.darker(.1f));
    metal.addColour(.85, colour::moltenEdge.brighter(.15f));
    g.setGradientFill(metal); g.fillRoundedRectangle(bounds, radius);
    {
        juce::Graphics::ScopedSaveState save(g);
        juce::Path clip; clip.addRoundedRectangle(bounds, radius); g.reduceClipRegion(clip);
        const float length = vertical ? bounds.getHeight() : bounds.getWidth();
        const float cross = vertical ? bounds.getWidth() : bounds.getHeight();
        juce::Path seam;
        const auto visible = bounds.getIntersection(g.getClipBounds().toFloat());
        const float begin = vertical ? visible.getY() - bounds.getY() : visible.getX() - bounds.getX();
        const float end = std::min(length, vertical ? visible.getBottom() - bounds.getY() : visible.getRight() - bounds.getX());
        for (float along = std::max(0.f, begin); along <= end; along += 2)
        {
            const float offset = cross * (.48f + .12f * std::sin(along * .075f) + .05f * std::sin(along * .23f));
            const auto point = vertical ? juce::Point<float>(bounds.getX() + offset, bounds.getY() + along) : juce::Point<float>(bounds.getX() + along, bounds.getY() + offset);
            if (seam.isEmpty()) seam.startNewSubPath(point); else seam.lineTo(point);
        }
        g.setColour(hot.interpolatedWith(colour::moltenCore, heat).withAlpha(.3f + heat * .5f)); g.strokePath(seam, juce::PathStrokeType(.8f));
    }
    g.setColour(hot.withAlpha(.7f)); g.drawRoundedRectangle(bounds.reduced(.35f), radius, .7f);
}
void AlloyLookAndFeel::drawScrollbar(juce::Graphics& g, juce::ScrollBar& bar, int x, int y, int width, int height,
                                    bool vertical, int thumbStart, int thumbSize, bool hover, bool down)
{
    if (!(bool) bar.getProperties()["alloyMolten"] || !bar.isEnabled())
    { juce::LookAndFeel_V4::drawScrollbar(g, bar, x, y, width, height, vertical, thumbStart, thumbSize, hover, down); return; }
    const auto track = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height).reduced(1);
    g.setColour(colour::background.darker(.4f)); g.fillRoundedRectangle(track, 4);
    g.setColour(colour::moltenEdge.withAlpha(.3f)); g.drawRoundedRectangle(track.reduced(.5f), 4, .7f);
    if (thumbSize <= 0) return;
    const auto thumb = vertical ? juce::Rectangle<float>(x + 3.f, thumbStart + 1.f, std::max(1.f, width - 6.f), std::max(1.f, thumbSize - 2.f))
                                : juce::Rectangle<float>(thumbStart + 1.f, y + 3.f, std::max(1.f, thumbSize - 2.f), std::max(1.f, height - 6.f));
    drawMoltenMetal(g, thumb, vertical, down ? 1.f : hover ? .85f : .4f);
}
bool AlloyLookAndFeel::styleNativeTitleBar(juce::Component& window)
{
#if JUCE_WINDOWS
    if (auto* peer = window.getPeer())
    {
        const auto handle = (HWND) peer->getNativeHandle();
        const BOOL dark = TRUE;
        // Keep Windows dragging, snapping, resizing and caption buttons.
        // The dark-mode attribute works on Windows 10; explicit colors on 11.
        const auto darkResult = DwmSetWindowAttribute(handle, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
        const COLORREF caption = RGB(colour::background.getRed(), colour::background.getGreen(), colour::background.getBlue());
        const COLORREF text = RGB(colour::text.getRed(), colour::text.getGreen(), colour::text.getBlue());
        const auto colorResult = DwmSetWindowAttribute(handle, DWMWA_CAPTION_COLOR, &caption, sizeof(caption));
        DwmSetWindowAttribute(handle, DWMWA_TEXT_COLOR, &text, sizeof(text));
        return SUCCEEDED(darkResult) || SUCCEEDED(colorResult);
    }
#else
    juce::ignoreUnused(window);
#endif
    return false;
}
namespace
{
juce::Point<float> radial(juce::Point<float> centre, float radius, float angle)
{ return {centre.x + radius * std::sin(angle), centre.y - radius * std::cos(angle)}; }
juce::Path hexagon(juce::Point<float> centre, float radius, float angle = 0)
{
    juce::Path path;
    for (int i = 0; i < 6; ++i)
    {
        auto p = radial(centre, radius, angle + i * juce::MathConstants<float>::twoPi / 6);
        if (i == 0) path.startNewSubPath(p); else path.lineTo(p);
    }
    path.closeSubPath(); return path.createPathWithRoundedCorners(radius * 0.17f);
}
void arc(juce::Graphics& g, juce::Point<float> c, float r, float from, float to, float thickness)
{
    if (std::abs(from - to) < 0.0001f) return;
    juce::Path p; p.addCentredArc(c.x, c.y, r, r, 0, std::min(from, to), std::max(from, to), true);
    g.strokePath(p, juce::PathStrokeType(thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
}

AlloyLookAndFeel::AlloyLookAndFeel()
{
    setDefaultSansSerifTypefaceName("Segoe UI");
    setUsingNativeAlertWindows(false);
    setColour(juce::AlertWindow::backgroundColourId, colour::panel);
    setColour(juce::AlertWindow::textColourId, colour::text);
    setColour(juce::AlertWindow::outlineColourId, colour::border);
    setColour(juce::TooltipWindow::backgroundColourId, colour::raised);
    setColour(juce::TooltipWindow::textColourId, colour::text);
    setColour(juce::TooltipWindow::outlineColourId, colour::border);
    setColour(juce::DirectoryContentsDisplayComponent::textColourId, colour::text);
    setColour(juce::DirectoryContentsDisplayComponent::highlightColourId, colour::raised);
    setColour(juce::DirectoryContentsDisplayComponent::highlightedTextColourId, colour::orange);
    setColour(juce::FileChooserDialogBox::titleTextColourId, colour::text);
    setColour(juce::ResizableWindow::backgroundColourId, colour::background);
    setColour(juce::TextButton::buttonColourId, colour::raised);
    setColour(juce::TextButton::buttonOnColourId, colour::orange.withMultipliedBrightness(0.38f));
    setColour(juce::TextButton::textColourOffId, colour::text); setColour(juce::TextButton::textColourOnId, colour::text);
    setColour(juce::Label::textColourId, colour::text);
    setColour(juce::ToggleButton::textColourId, colour::text); setColour(juce::ToggleButton::tickColourId, colour::orange);
    setColour(juce::Slider::thumbColourId, colour::orange); setColour(juce::Slider::trackColourId, colour::orange);
    setColour(juce::Slider::backgroundColourId, colour::border);
    setColour(juce::Slider::textBoxTextColourId, colour::text);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxHighlightColourId, colour::orange.withAlpha(0.3f));
    setColour(juce::TextEditor::backgroundColourId, colour::background); setColour(juce::TextEditor::outlineColourId, colour::border);
    setColour(juce::TextEditor::focusedOutlineColourId, colour::focus); setColour(juce::TextEditor::textColourId, colour::text);
    setColour(juce::TextEditor::highlightColourId, colour::orange.withAlpha(0.3f)); setColour(juce::CaretComponent::caretColourId, colour::orange);
    setColour(juce::ComboBox::backgroundColourId, colour::raised); setColour(juce::ComboBox::outlineColourId, colour::border);
    setColour(juce::ComboBox::textColourId, colour::text); setColour(juce::ComboBox::arrowColourId, colour::muted);
    setColour(juce::ListBox::backgroundColourId, colour::panel); setColour(juce::ListBox::textColourId, colour::text);
    setColour(juce::TableHeaderComponent::backgroundColourId, colour::raised); setColour(juce::TableHeaderComponent::textColourId, colour::muted);
    setColour(juce::TableHeaderComponent::outlineColourId, colour::border);
    setColour(juce::ScrollBar::thumbColourId, colour::muted.withAlpha(0.45f)); setColour(juce::ScrollBar::trackColourId, colour::background);
    setColour(juce::PopupMenu::backgroundColourId, colour::panel); setColour(juce::PopupMenu::textColourId, colour::text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, colour::raised); setColour(juce::PopupMenu::highlightedTextColourId, colour::orange);
    setColour(juce::PopupMenu::headerTextColourId, colour::muted);
}
juce::Font AlloyLookAndFeel::getPopupMenuFont() { return juce::Font(juce::FontOptions(13.f)); }
juce::Font AlloyLookAndFeel::getMenuBarFont(juce::MenuBarComponent&, int, const juce::String&) { return getPopupMenuFont(); }
juce::Font AlloyLookAndFeel::getAlertWindowTitleFont() { return juce::Font(juce::FontOptions(16.f, juce::Font::bold)); }
juce::Font AlloyLookAndFeel::getAlertWindowMessageFont() { return juce::Font(juce::FontOptions(13.f)); }
juce::Font AlloyLookAndFeel::getAlertWindowFont() { return juce::Font(juce::FontOptions(12.f)); }
void AlloyLookAndFeel::drawAlertBox(juce::Graphics& g, juce::AlertWindow& alert, const juce::Rectangle<int>&, juce::TextLayout& text)
{
    g.fillAll(colour::panel);
    g.setColour(colour::border); g.drawRect(alert.getLocalBounds());
    g.setColour(colour::text);
    text.draw(g, juce::Rectangle<float>(20, 30, (float) alert.getWidth() - 40, text.getHeight()));
}
void AlloyLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    g.fillAll(colour::panel); g.setColour(colour::border); g.drawRect(0, 0, width, height);
}
void AlloyLookAndFeel::drawFileBrowserRow(juce::Graphics& g, int width, int height, const juce::File&, const juce::String& name,
    juce::Image* icon, const juce::String& size, const juce::String& date, bool directory, bool selected, int,
    juce::DirectoryContentsDisplayComponent&)
{
    if (selected) { g.setColour(colour::raised); g.fillRect(0, 0, width, height); }
    if (icon && icon->isValid()) g.drawImageWithin(*icon, 6, 2, 18, height - 4, juce::RectanglePlacement::centred);
    const bool details = width >= 450;
    const int right = details ? width - 185 : width - 8;
    g.setColour(selected ? colour::orange : colour::text); g.setFont(juce::FontOptions(12));
    g.drawText(name, 30, 0, std::max(0, right - 30), height, juce::Justification::centredLeft, true);
    if (details)
    {
        g.setColour(colour::muted); g.setFont(juce::FontOptions(11));
        if (!directory) g.drawText(size, width - 180, 0, 70, height, juce::Justification::centredRight, true);
        g.drawText(date, width - 102, 0, 94, height, juce::Justification::centredRight, true);
    }
}
juce::Font AlloyLookAndFeel::getTextButtonFont(juce::TextButton&, int height)
{ return juce::Font(juce::FontOptions(juce::jlimit(11.0f, 13.0f, height * 0.47f))); }
juce::Font AlloyLookAndFeel::getComboBoxFont(juce::ComboBox&)
{ return juce::Font(juce::FontOptions(12.0f)); }
void AlloyLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour& base, bool hover, bool down)
{
    const auto bounds = b.getLocalBounds().toFloat().reduced(0.5f);
    const bool tab = (bool) b.getProperties()["alloyTab"];
    auto fill = tab ? colour::panel : base;
    if (hover && b.isEnabled()) fill = fill.brighter(0.08f);
    if (down) fill = fill.darker(0.12f);
    if (!b.isEnabled()) fill = colour::panel;
    g.setGradientFill(juce::ColourGradient(fill.brighter(0.035f), 0, 0, fill, 0, (float) b.getHeight(), false));
    g.fillRoundedRectangle(bounds, 4);
    if ((bool) b.getProperties()["alloyPad"])
    {
        g.setColour((b.getToggleState() || down ? b.findColour(juce::TextButton::buttonOnColourId).brighter(0.5f) : colour::muted).withAlpha(b.isEnabled() ? 0.8f : 0.25f));
        g.fillRoundedRectangle(bounds.withTrimmedTop(bounds.getHeight() - 2).reduced(5, 0), 1);
    }
    if (!tab) { g.setColour(b.hasKeyboardFocus(true) ? colour::focus : colour::border); g.drawRoundedRectangle(bounds, 4, 1); }
    const bool molten = (tab && b.getToggleState()) || ((bool) b.getProperties()["alloyMolten"]
        && (hover || down || b.getToggleState() || (bool) b.getProperties()["alloyActive"]));
    if (molten && b.isEnabled()) drawMoltenMetal(g, bounds.withTop(bounds.getBottom() - 2.5f).reduced(tab ? 6.f : 5.f, 0), false, down ? 1.f : .65f);
    if (tab && b.hasKeyboardFocus(true)) { g.setColour(colour::focus); g.drawRoundedRectangle(bounds.reduced(1), 3, 1); }
}
void AlloyLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<float>(0.5f, 0.5f, (float) width - 1, (float) height - 1);
    g.setColour(colour::raised); g.fillRoundedRectangle(bounds, 4);
    g.setColour(box.hasKeyboardFocus(true) ? colour::focus : colour::border); g.drawRoundedRectangle(bounds, 4, 1);
    juce::Path arrow; arrow.startNewSubPath((float) width - 17, height * 0.45f); arrow.lineTo((float) width - 13, height * 0.60f); arrow.lineTo((float) width - 9, height * 0.45f);
    g.setColour(colour::muted); g.strokePath(arrow, juce::PathStrokeType(1.5f));
}
void AlloyLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float value, float start, float end, juce::Slider& slider)
{
    const auto centre = juce::Point<float>((float) x + width * 0.5f, (float) y + height * 0.5f);
    const float radius = std::max(2.0f, std::min(width, height) * 0.5f - 5.0f), angle = start + value * (end - start);
    const float alpha = slider.isEnabled() ? 1.0f : 0.4f;
    const float heat = slider.isEnabled() ? value : 0;
    const auto hot = metalHeat(heat).withAlpha(alpha);
    g.setColour(colour::border); arc(g, centre, radius, start, end, 2);
    for (int i = 0; i <= 24; ++i)
    {
        const float a = start + (end - start) * i / 24;
        g.setColour(colour::muted.withAlpha((i % 6 == 0 ? 0.85f : 0.42f) * alpha));
        g.drawLine({radial(centre, radius + 2, a), radial(centre, radius + (i % 6 == 0 ? 5.0f : 3.5f), a)}, 0.8f);
    }
    const float neutral = slider.getMinimum() < 0 && slider.getMaximum() > 0 ? (float) slider.valueToProportionOfLength(0) : 0;
    if (slider.isEnabled() && (slider.hasKeyboardFocus(true) || slider.isMouseOver(true)))
    { g.setColour(colour::moltenOrange.withAlpha(.2f)); arc(g, centre, radius, start + neutral * (end - start), angle, 6); }
    g.setColour(hot); arc(g, centre, radius, start + neutral * (end - start), angle, 2.4f);
    if (heat > .7f) { g.setColour(hot.withAlpha((heat - .7f) * .35f)); arc(g, centre, radius, start + neutral * (end - start), angle, 5); }
    const float capRadius = radius - 5;
    const auto cap = hexagon(centre, capRadius, angle);
    g.setColour(juce::Colours::black.withAlpha(0.35f)); g.fillPath(cap, juce::AffineTransform::translation(0, 2));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xfff3f4f3).withAlpha(alpha), centre.x - capRadius, centre.y - capRadius,
                                        juce::Colour(0xff40494f).withAlpha(alpha), centre.x + capRadius, centre.y + capRadius, false));
    g.fillPath(cap);
    const auto face = hexagon(centre, capRadius - 3, angle);
    auto metal = juce::ColourGradient(juce::Colour(0xffbfc5c7).withAlpha(alpha), centre.x - capRadius, centre.y - capRadius,
                                     juce::Colour(0xff636d74).withAlpha(alpha), centre.x + capRadius, centre.y + capRadius, false);
    metal.addColour(0.34, juce::Colour(0xffe0e3e3).withAlpha(alpha)); metal.addColour(0.55, juce::Colour(0xff9ca5a9).withAlpha(alpha));
    g.setGradientFill(metal); g.fillPath(face);
    // Keep the machined hex cap; the value heats its metal instead of adding a
    // second control skin. Fixed ticks and keyboard focus remain legible.
    g.setColour(hot.withAlpha(.10f + heat * .34f)); g.fillPath(face);
    {
        juce::Graphics::ScopedSaveState save(g); g.reduceClipRegion(face);
        for (float r = 2; r < capRadius + 2; r += 1.15f)
        { g.setColour(juce::Colours::white.withAlpha(0.055f * alpha)); g.drawEllipse(centre.x - r, centre.y - r, 2 * r, 2 * r, 0.4f); }
    }
    g.setColour(juce::Colour(0xffe6e9eb).withAlpha(0.65f * alpha)); g.strokePath(cap, juce::PathStrokeType(1));
    g.setColour(hot); g.strokePath(hexagon(centre, capRadius - 2, angle), juce::PathStrokeType(1 + heat));
    g.setColour(colour::background.withAlpha(alpha));
    juce::Path notch; notch.startNewSubPath(radial(centre, capRadius * 0.48f, angle)); notch.lineTo(radial(centre, capRadius * 0.77f, angle));
    g.strokePath(notch, juce::PathStrokeType(std::max(2.0f, capRadius * 0.08f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(hot); g.strokePath(notch, juce::PathStrokeType(std::max(1.f, capRadius * .04f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (slider.hasKeyboardFocus(true)) { g.setColour(colour::focus); g.drawEllipse(centre.x - radius - 5, centre.y - radius - 5, 2 * radius + 10, 2 * radius + 10, 1.2f); }
}
void AlloyLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float pos, float min, float max, juce::Slider::SliderStyle style, juce::Slider& slider)
{
    const float heat = slider.isEnabled() ? (float) slider.valueToProportionOfLength(slider.getValue()) : 0;
    if (style == juce::Slider::LinearBarVertical || style == juce::Slider::LinearBar)
    {
        g.setColour(colour::background); g.fillRoundedRectangle((float) x, (float) y, (float) width, (float) height, 3);
        g.setColour(slider.hasKeyboardFocus(true) ? colour::focus : metalHeat(heat));
        g.fillRect((float) x, (float) y + height - 2, (float) width, 2.0f); return;
    }
    if (style == juce::Slider::LinearVertical)
    {
        const float mid = x + width * 0.5f;
        g.setColour(colour::background); g.fillRoundedRectangle(mid - 3, (float) y, 6, (float) height, 3);
        g.setColour(colour::border);
        for (int i = 0; i <= 8; ++i)
        {
            const float tick = y + height * i / 8.0f;
            g.drawLine(mid - 12, tick, mid - 7, tick); g.drawLine(mid + 7, tick, mid + 12, tick);
        }
        g.setColour(metalHeat(heat).withAlpha(slider.isEnabled() ? 0.7f : 0.2f));
        g.fillRect(mid - 1, pos, 2.0f, std::max(0.0f, y + height - pos));
        const juce::Rectangle<float> cap(mid - 12, pos - 6, 24, 12);
        if (slider.isEnabled()) drawMoltenMetal(g, cap, false, heat);
        else { g.setGradientFill(juce::ColourGradient(colour::text, mid, pos - 6, colour::muted.darker(0.5f), mid, pos + 6, false)); g.fillRoundedRectangle(cap, 2); }
        g.setColour(slider.hasKeyboardFocus(true) ? colour::focus : colour::border); g.drawRoundedRectangle(cap, 2, 1);
        g.setColour(colour::background); g.drawLine(mid - 8, pos, mid + 8, pos, 1);
        return;
    }
    if (style != juce::Slider::LinearHorizontal)
    { juce::LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, pos, min, max, style, slider); return; }
    const float mid = y + height * 0.5f;
    g.setColour(colour::background); g.fillRoundedRectangle((float) x, mid - 2, (float) width, 4, 2);
    g.setColour(metalHeat(heat).withAlpha(slider.isEnabled() ? 0.75f : 0.25f)); g.fillRoundedRectangle((float) x, mid - 1, std::max(0.0f, pos - x), 2, 1);
    const auto thumb = juce::Rectangle<float>(pos - 5, mid - 6, 10, 12);
    if (slider.isEnabled()) drawMoltenMetal(g, thumb, true, heat);
    else { g.setGradientFill(juce::ColourGradient(colour::text, pos, mid - 6, colour::muted.darker(0.3f), pos, mid + 6, false)); g.fillRoundedRectangle(thumb, 2); }
    g.setColour(slider.hasKeyboardFocus(true) ? colour::focus : colour::orange); g.drawRoundedRectangle(thumb, 2, 1);
}
void AlloyLookAndFeel::drawEmblem(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    const auto c = bounds.getCentre(); const float r = std::min(bounds.getWidth(), bounds.getHeight()) * 0.48f;
    auto cap = hexagon(c, r); g.setGradientFill(juce::ColourGradient(colour::text, bounds.getTopLeft(), colour::muted.darker(0.4f), bounds.getBottomRight(), false)); g.fillPath(cap);
    juce::Path wave; wave.startNewSubPath(c.x - r * 0.72f, c.y); wave.lineTo(c.x - r * 0.38f, c.y);
    wave.lineTo(c.x - r * 0.2f, c.y - r * 0.25f); wave.lineTo(c.x, c.y + r * 0.38f);
    wave.lineTo(c.x + r * 0.2f, c.y - r * 0.45f); wave.lineTo(c.x + r * 0.38f, c.y); wave.lineTo(c.x + r * 0.72f, c.y);
    g.setColour(colour::background); g.strokePath(wave, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(colour::orange); g.strokePath(wave, juce::PathStrokeType(1.2f));
}
void AlloyDial::configure(const juce::String& name, double defaultValue)
{
    configured = true; resetValue = defaultValue; setName(name); setTitle(name); setWantsKeyboardFocus(true);
    setSliderStyle(juce::Slider::RotaryVerticalDrag); setTextBoxStyle(juce::Slider::TextBoxBelow, false, 76, 20);
    setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    setDoubleClickReturnValue(true, resetValue); setSliderSnapsToMousePosition(false); setMouseDragSensitivity(240);
}
void AlloyDial::mouseDown(const juce::MouseEvent& event)
{
    // Decide sensitivity once per gesture so toggling Shift cannot jump a value.
    if (configured) setMouseDragSensitivity(event.mods.isShiftDown() ? 2400 : 240); juce::Slider::mouseDown(event);
}
void AlloyDial::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (!configured || hasKeyboardFocus(true)) juce::Slider::mouseWheelMove(event, wheel);
    else juce::Component::mouseWheelMove(event, wheel);
}
bool AlloyDial::keyPressed(const juce::KeyPress& key)
{
    if (!isEnabled()) return false;
    if (configured && key == juce::KeyPress::homeKey) { juce::Slider::ScopedDragNotification gesture(*this); setValue(resetValue, juce::sendNotificationSync); return true; }
    if (key == juce::KeyPress::upKey || key == juce::KeyPress::rightKey || key == juce::KeyPress::downKey || key == juce::KeyPress::leftKey)
    { juce::Slider::ScopedDragNotification gesture(*this); return juce::Slider::keyPressed(key); }
    return juce::Slider::keyPressed(key);
}
}
