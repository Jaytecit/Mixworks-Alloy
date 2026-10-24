#include "Workstation.h"
#include <cmath>
#include <limits>
#include <set>

namespace mixworks
{
void runAttractTests(Workstation& ui, const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    bool finite = true; std::set<int> patterns;
    for (int frame = 0; frame < 2880; ++frame)
    {
        auto light = AttractLights::lightAt(frame * .1, .37f, .68f, frame % 80, 80, true);
        finite &= std::isfinite(light.heat) && light.heat >= 0 && light.heat <= 1 && light.phase >= 0 && light.phase < 1;
        patterns.insert(light.pattern);
    }
    check(finite && patterns.size() == 6, "attract sequence varies through six bounded light patterns across its complete cycle");
    const auto steady = AttractLights::lightAt(0, 0, 0, 0, 1, false);
    const auto later = AttractLights::lightAt(200, 1, 1, 80, 81, false);
    check(steady.heat == later.heat && steady.phase == later.phase && steady.colour == later.colour,
          "reduced-motion lighting is steady across time and control positions");
    check(std::isfinite(AttractLights::lightAt(std::numeric_limits<double>::infinity(), .5f, .5f, 0, 0, true).heat)
        && std::isfinite(AttractLights::lightAt(-1, .5f, .5f, 0, 1, true).heat), "invalid or early clocks retain finite lighting");
    check(!ui.attractState.enabled && !ui.attractLights.isVisible(), "attract mode starts off without retained preferences");
    const auto project = juce::JSON::toString(projectToJson(ui.engine.project));
    const auto undo = ui.undoHistory.size(), redo = ui.redoHistory.size(); const auto dirty = ui.dirty, workspaceDirty = ui.workspaceDirty;
    const auto status = ui.status; const auto position = ui.engine.position.load(); const auto playing = ui.engine.playing.load();
    const auto record = ui.engine.automationRecordMode; const auto armed = ui.engine.automationArmed;
    const auto learn = ui.engine.learnAction.hasValue();
    const int inspector = ui.inspectorPage;
    auto snapshot = [&](juce::Component& component) { return component.createComponentSnapshot(component.getLocalBounds()); };
    auto save = [&](juce::Component& component, const juce::String& name, float scale = 1)
    {
        const auto image = component.createComponentSnapshot(component.getLocalBounds(), true, scale);
        auto output = report.getSiblingFile(name).createOutputStream(); if (output) { output->setPosition(0); output->truncate(); }
        juce::PNGImageFormat png; return output && png.writeImageToStream(image, *output);
    };
    auto differences = [](const juce::Image& a, const juce::Image& b)
    {
        int count = 0; if (a.getBounds() != b.getBounds()) return -1;
        for (int y = 0; y < a.getHeight(); ++y) for (int x = 0; x < a.getWidth(); ++x) count += a.getPixelAt(x, y) != b.getPixelAt(x, y);
        return count;
    };
    // Escape can leave one antialiased curve-edge pixel differing by 3/255.
    // Keep this tolerance local to appearance restoration; state and PCM stay exact.
    auto restoredAppearance = [](const juce::Image& a, const juce::Image& b)
    {
        if (!a.isValid() || !b.isValid() || a.getBounds() != b.getBounds()) return false;
        int changed = 0;
        for (int y = 0; y < a.getHeight(); ++y) for (int x = 0; x < a.getWidth(); ++x)
        {
            const auto left = a.getPixelAt(x, y), right = b.getPixelAt(x, y);
            if (left == right) continue;
            if (++changed > 1 || left.getAlpha() != right.getAlpha()
                || std::abs(int(left.getRed()) - int(right.getRed())) > 3
                || std::abs(int(left.getGreen()) - int(right.getGreen())) > 3
                || std::abs(int(left.getBlue()) - int(right.getBlue())) > 3) return false;
        }
        return true;
    };
    {
        juce::Image reference(juce::Image::ARGB, 2, 2, true);
        reference.clear(reference.getBounds(), juce::Colour(135, 127, 72));
        auto changed = reference.createCopy();
        check(restoredAppearance(reference, changed), "appearance comparison accepts identical pixels");
        changed.setPixelAt(0, 0, juce::Colour(132, 124, 71));
        check(restoredAppearance(reference, changed) && restoredAppearance(changed, reference),
              "appearance comparison accepts the recorded single-pixel variation in either direction");
        changed.setPixelAt(1, 0, juce::Colour(132, 124, 71));
        check(!restoredAppearance(reference, changed), "appearance comparison rejects two changed pixels");
        changed = reference.createCopy(); changed.setPixelAt(0, 0, juce::Colour(131, 127, 72));
        check(!restoredAppearance(reference, changed), "appearance comparison rejects a channel difference above three");
        changed = reference.createCopy(); changed.setPixelAt(0, 0, juce::Colour(135, 127, 72).withAlpha(juce::uint8(254)));
        check(!restoredAppearance(reference, changed), "appearance comparison rejects an alpha change");
        check(!restoredAppearance(reference, juce::Image(juce::Image::ARGB, 1, 2, true))
            && !restoredAppearance(juce::Image(), juce::Image()), "appearance comparison rejects missing or resized captures");
    }
    const bool wasVisible = ui.isVisible(); ui.setVisible(true);
    ui.setSize(1920, 1080); ui.showToolPage(0);
    juce::MidiBuffer midi; juce::AudioBuffer<float> before(2, 256), after(2, 256);
    ui.engine.resetPlugins(); ui.engine.render(before, 256, 5, midi);
    const auto normal = snapshot(ui); // The render updates real peak meters before either visual baseline.
    ui.menuItemSelected(77, 4);
    check(ui.attractState.enabled && ui.attractLights.isVisible(), "View menu enables the shared attract lighting overlay");
    ui.engine.resetPlugins(); ui.engine.render(after, 256, 5, midi);
    bool identical = before.getMagnitude(0, 256) > .001f;
    for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 256; ++i) identical &= before.getSample(ch, i) == after.getSample(ch, i);
    check(identical, "enabled attract lighting preserves exact non-silent rendered PCM");
    const auto point = ui.homeButton.getBounds().getCentre();
    check(ui.getComponentAt(point) == &ui.homeButton && !ui.attractLights.getWantsKeyboardFocus() && !ui.attractLights.isAccessible(),
          "light overlays pass pointer input through and add no keyboard or accessibility focus targets");
    check(differences(normal, snapshot(ui)) > 1000, "enabled lighting visibly changes the native interface");
    check(save(ui, "attract-native.png"), "capture native lighting with the current Windows motion preference");
    ui.menuItemSelected(79, 4);
    check(ui.attractState.allowsMotion() && ui.attractState.motion == AttractMotion::animated,
          "explicit Animated choice enables motion without changing the Windows preference");
    ui.attractState.startedAtMs = juce::Time::getMillisecondCounterHiRes() - 3000; const auto first = snapshot(ui);
    ui.attractState.startedAtMs = juce::Time::getMillisecondCounterHiRes() - 4000;
    check(differences(first, snapshot(ui)) > 1000, "successive native animated frames advance the scanner and dial/fader lights");
    for (int scene : {0, 1, 2, 3, 4, 5})
    {
        ui.attractState.startedAtMs = juce::Time::getMillisecondCounterHiRes() - (scene * 8. + 3) * 1000;
        check(save(ui, "attract-scene-" + juce::String(scene) + ".png"), "capture native animated attract scene " + juce::String(scene));
    }
    check(ui.workspaceDirty == workspaceDirty, "attract lighting alone does not change saved workspace preferences");
    ui.showToolPage(1); check(save(ui, "attract-transitions.png"), "attract lighting covers the selected Transitions panel");
    ui.detachToolPanel(1, true); ui.transitionWindow->setSize(1100, 620);
    ui.detachToolPanel(0, true);
    check(save(ui.transitionPanel, "attract-transitions-detached.png") && save(ui.performancePanel, "attract-controller-detached.png"),
          "both independently detached panels retain their shared-clock light show");
    check(ui.performancePanel.getComponentAt(ui.performancePanel.dock.getBounds().getCentre()) == &ui.performancePanel.dock,
          "detached overlay leaves the hexagonal attach control interactive");
    ui.detachToolPanel(0, false); ui.detachToolPanel(1, false); ui.showToolPage(0);
    ui.setSize(1250, 800); check(save(ui, "attract-1250.png"), "attract lighting follows the narrow native layout");
    ui.showInspectorPage(2); check(save(ui, "attract-keys-200.png", 2), "keyboard edge lights and controls remain vector-rendered at 200 percent");
    ui.showInspectorPage(inspector); ui.showToolPage(0); ui.setSize(1920, 1080);
    check(project == juce::JSON::toString(projectToJson(ui.engine.project)) && ui.undoHistory.size() == undo && ui.redoHistory.size() == redo
        && ui.dirty == dirty && ui.status == status && ui.engine.position == position && ui.engine.playing == playing
        && ui.engine.automationRecordMode == record && ui.engine.automationArmed == armed && ui.engine.learnAction.hasValue() == learn,
          "light sequences, resizing and detached panels preserve the mix, transport, Undo, record and learn state");
    ui.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));
    const auto restored = snapshot(ui); const int restoredDifferences = differences(normal, restored);
    if (restoredDifferences != 0)
    {
        for (const auto& capture : {std::pair<juce::String, juce::Image>{"escape-before.png", normal}, {"escape-after.png", restored}})
        { auto output = report.getSiblingFile(capture.first).createOutputStream(); juce::PNGImageFormat png; if (output) png.writeImageToStream(capture.second, *output); }
    }
    check(!ui.attractState.enabled && !ui.attractLights.isVisible() && restoredAppearance(normal, restored),
          "Escape restores the original native appearance within one pixel at RGB tolerance three");
    const juce::KeyPress shortcut('A', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0);
    check(ui.keyPressed(shortcut) && ui.attractState.enabled, "Ctrl+Shift+A enables attract lighting");
    ui.busy = true; const bool exited = ui.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)); ui.busy = false;
    check(exited && !ui.attractState.enabled, "Escape exits attract lighting even while a background operation is busy");
    ui.menuItemSelected(77, 4); ui.menuItemSelected(77, 4);
    check(!ui.attractState.enabled, "View menu toggles attract lighting off again without persisting it");
    ui.menuItemSelected(80, 4); ui.menuItemSelected(77, 4);
    // Assert the native lighting surface itself. The host also paints waveform
    // curves, whose rasterisation is independent of the attract clock.
    const auto steadyHost = snapshot(ui), steadyFrame = snapshot(ui.attractLights);
    ui.attractState.startedAtMs -= 20000;
    const auto steadyHostLater = snapshot(ui), steadyLater = snapshot(ui.attractLights);
    const int steadyDifferences = differences(steadyFrame, steadyLater), hostDifferences = differences(steadyHost, steadyHostLater);
    bool hasLighting = false;
    for (int y = 0; y < steadyFrame.getHeight() && !hasLighting; ++y)
        for (int x = 0; x < steadyFrame.getWidth() && !hasLighting; ++x) hasLighting = steadyFrame.getPixelAt(x, y).getAlpha() != 0;
    {
        auto* facts = new juce::DynamicObject(); facts->setProperty("lightingPixelsChanged", steadyDifferences);
        facts->setProperty("hostPixelsChanged", hostDifferences); facts->setProperty("lightingHasInk", hasLighting);
        report.getSiblingFile("steady-render-comparison.json").replaceWithText(juce::JSON::toString(juce::var(facts)));
    }
    if (steadyDifferences != 0 || hostDifferences != 0)
    {
        for (bool laterFrame : {false, true})
        {
            auto output = report.getSiblingFile(laterFrame ? "steady-later-failure.png" : "steady-before-failure.png").createOutputStream();
            juce::PNGImageFormat png; if (output) png.writeImageToStream(laterFrame ? steadyHostLater : steadyHost, *output);
            auto lighting = report.getSiblingFile(laterFrame ? "steady-lighting-later.png" : "steady-lighting-before.png").createOutputStream();
            if (lighting) png.writeImageToStream(laterFrame ? steadyLater : steadyFrame, *lighting);
        }
    }
    check(!ui.attractState.allowsMotion() && hasLighting && steadyDifferences == 0,
          "Steady choice stops native lighting motion even when explicitly enabled (" + juce::String(steadyDifferences) + " lighting pixels; " + juce::String(hostDifferences) + " host pixels changed)");
    ui.menuItemSelected(77, 4); ui.menuItemSelected(78, 4);
    check(ui.attractState.motion == AttractMotion::system && ui.attractState.allowsMotion() == AlloyLookAndFeel::motionAllowed(),
          "Follow Windows restores the current reduced-motion preference");
    ui.setVisible(wasVisible);
}
}
