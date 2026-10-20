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
    ui.showInspectorPage(0); ui.setSize(1920, 1080);
    check(project == juce::JSON::toString(projectToJson(ui.engine.project)) && ui.undoHistory.size() == undo && ui.redoHistory.size() == redo
        && ui.dirty == dirty && ui.workspaceDirty == workspaceDirty && ui.status == status && ui.engine.position == position && ui.engine.playing == playing
        && ui.engine.automationRecordMode == record && ui.engine.automationArmed == armed && ui.engine.learnAction.hasValue() == learn,
          "light sequences, resizing and detached panels preserve the mix, transport, Undo, record and learn state");
    ui.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));
    check(!ui.attractState.enabled && !ui.attractLights.isVisible() && differences(normal, snapshot(ui)) == 0,
          "Escape restores the original native appearance exactly");
    const juce::KeyPress shortcut('A', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0);
    check(ui.keyPressed(shortcut) && ui.attractState.enabled, "Ctrl+Shift+A enables attract lighting");
    ui.busy = true; const bool exited = ui.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)); ui.busy = false;
    check(exited && !ui.attractState.enabled, "Escape exits attract lighting even while a background operation is busy");
    ui.menuItemSelected(77, 4); ui.menuItemSelected(77, 4);
    check(!ui.attractState.enabled, "View menu toggles attract lighting off again without persisting it");
    ui.menuItemSelected(80, 4); ui.menuItemSelected(77, 4);
    const auto steadyFrame = snapshot(ui); ui.attractState.startedAtMs -= 20000;
    check(!ui.attractState.allowsMotion() && differences(steadyFrame, snapshot(ui)) == 0, "Steady choice stops native lighting motion even when explicitly enabled");
    ui.menuItemSelected(77, 4); ui.menuItemSelected(78, 4);
    check(ui.attractState.motion == AttractMotion::system && ui.attractState.allowsMotion() == AlloyLookAndFeel::motionAllowed(),
          "Follow Windows restores the current reduced-motion preference");
    ui.setVisible(wasVisible);
}
}
