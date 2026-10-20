#include "AppWindow.h"
#include "Workstation.h"
#include <juce_cryptography/juce_cryptography.h>
#include <cmath>

namespace mixworks
{
void runWindowTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    const auto* primary = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
    check(primary != nullptr, "native display inventory is available"); if (!primary) return;
    const auto fullBounds = primary->logicalBounds.getSmallestIntegerContainer();
    auto* facts = new juce::DynamicObject(); facts->setProperty("monitorWidth", primary->physicalBounds.getWidth()); facts->setProperty("monitorHeight", primary->physicalBounds.getHeight());
    facts->setProperty("logicalWidth", fullBounds.getWidth()); facts->setProperty("logicalHeight", fullBounds.getHeight()); facts->setProperty("scale", primary->scale);
    juce::Array<juce::var> displayFacts;
    for (const auto& display : juce::Desktop::getInstance().getDisplays().displays)
    {
        auto* info = new juce::DynamicObject(); info->setProperty("logicalBounds", display.logicalBounds.toString()); info->setProperty("scale", display.scale); displayFacts.add(juce::var(info));
    }
    facts->setProperty("displays", displayFacts);
    report.getSiblingFile("display-facts.json").replaceWithText(juce::JSON::toString(juce::var(facts)));
    AppWindow window(new juce::Component());
    check(window.windowMode() == WindowMode::fullscreen && window.getBounds() == fullBounds
          && window.getContentComponent()->getBounds() == window.getLocalBounds(),
          "fullscreen fills the full monitor, including taskbar space, without a caption or content border");
    check(window.performDisplayAction(AppWindow::windowed) && window.windowMode() == WindowMode::windowed
          && primary->userBounds.getLargestIntegerWithin().contains(window.getBounds()) && window.getTitleBarHeight() == 27,
          "display menu restores a fitting, resizable window with controls");
    const auto restored = window.getBounds();
    window.toggleFullscreen(); window.toggleFullscreen();
    check(window.windowMode() == WindowMode::windowed && window.getBounds() == restored,
          "repeated fullscreen toggles preserve the user's windowed size and position");
    check(window.performDisplayAction(AppWindow::maximised) && window.windowMode() == WindowMode::maximised,
          "maximised mode is distinct from full-monitor fullscreen");
    window.performDisplayAction(AppWindow::fullscreen);
    check(window.getBounds() == fullBounds, "switching from maximised to fullscreen fills the entire monitor again");
    const auto menu = window.displayMenu(); bool ticked = false, windowOption = false;
    for (juce::PopupMenu::MenuItemIterator it(menu, true); it.next();)
    { ticked |= it.getItem().itemID == AppWindow::fullscreen && it.getItem().isTicked; windowOption |= it.getItem().itemID == AppWindow::windowed; }
    check(ticked && windowOption, "display menu advertises named modes and ticks the actual selected mode");
    check(!window.performDisplayAction(9999) && window.getBounds() == fullBounds, "unknown display actions preserve the current monitor and mode");
    std::vector<juce::Rectangle<int>> monitors;
    for (const auto& display : juce::Desktop::getInstance().getDisplays().displays) monitors.push_back(display.logicalBounds.getSmallestIntegerContainer());
    const auto span = AppWindow::spanningBounds(monitors);
    check(AppWindow::spanningBounds({{0, 0, 1920, 1080}, {-1920, 64, 1920, 1080}}) == juce::Rectangle<int>(-1920, 64, 3840, 1016), "offset horizontal monitors span their shared visible band without losing menus");
    check(AppWindow::spanningBounds({{64, -1080, 1920, 1080}, {0, 0, 1920, 1080}}) == juce::Rectangle<int>(64, -1080, 1856, 2160), "offset stacked monitors span their shared visible columns");
    check(AppWindow::spanningBounds({{0, 0, 1920, 1080}, {2000, 0, 1920, 1080}}).isEmpty() && AppWindow::spanningBounds({{0, 0, 1920, 1080}}).isEmpty(), "monitor gaps and a single monitor cannot create an invisible workspace region");
    if (!span.isEmpty())
    {
        check(window.performDisplayAction(AppWindow::spanDisplays) && window.windowMode() == WindowMode::spanning
              && window.getBounds() == span
              && window.getContentComponent()->getBounds() == window.getLocalBounds(), "span all monitors exposes the full workspace across the actual desktop");
        window.toggleFullscreen(); check(window.windowMode() == WindowMode::windowed, "F11 escapes spanning into the saved resizable window");
    }
    else check(!window.performDisplayAction(AppWindow::spanDisplays) && window.getBounds() == fullBounds, "spanning requires a usable shared display area; detached windows remain available");
    window.performDisplayAction(AppWindow::fullscreen);
    if (primary->userBounds.getWidth() < 2576 || primary->userBounds.getHeight() < 1456)
        check(!window.performDisplayAction(AppWindow::size2560) && window.getBounds() == fullBounds, "a window preset too large for the monitor is rejected without changing mode");
    window.setMinimised(true); window.setMinimised(false);
    check(window.windowMode() == WindowMode::fullscreen && window.getBounds() == fullBounds, "minimising and restoring retain the selected full-monitor mode");
    window.performDisplayAction(AppWindow::windowed);
    if (primary->userBounds.getWidth() >= 1460 && primary->userBounds.getHeight() >= 830)
        check(window.performDisplayAction(AppWindow::size1440) && window.getWidth() == 1440 && window.getHeight() == 810,
              "16:9 window preset applies its advertised size");
    window.setVisible(false);
    auto* ui = new Workstation(false); ui->stopTimer();
    AppWindow workstation(ui); workstation.performDisplayAction(AppWindow::windowed);
    const auto before = workstation.windowMode(); ui->keyPressed(juce::KeyPress(juce::KeyPress::F11Key));
    check(before == WindowMode::windowed && workstation.windowMode() == WindowMode::fullscreen && workstation.getBounds() == fullBounds,
          "F11 dispatches through the same window owner as the display menu");
    check(workstation.keyPressed(juce::KeyPress(juce::KeyPress::F11Key)) && workstation.windowMode() == WindowMode::windowed,
          "F11 also works when the recreated native window itself has keyboard focus");
    workstation.keyPressed(juce::KeyPress(juce::KeyPress::F11Key));
    ui->windowModeButton.onClick();
    check(workstation.windowMode() == WindowMode::windowed && ui->windowModeButton.getButtonText() == "Fullscreen", "visible mode button restores windowed mode and advertises its next action");
    ui->menuItemSelected(AppWindow::fullscreen, 4);
    check(workstation.getBounds() == fullBounds && ui->windowModeButton.getButtonText() == "Windowed", "View / Display dispatch restores true fullscreen and updates its visible control");
    const auto view = ui->getMenuForIndex(4, {}); bool viewFullscreen = false;
    for (juce::PopupMenu::MenuItemIterator it(view, true); it.next();) viewFullscreen |= it.getItem().itemID == AppWindow::fullscreen && it.getItem().isTicked;
    check(viewFullscreen, "View menu and header Display menu expose the same current fullscreen selection");
    ui->setBusy(true);
    check(ui->displayButton.isEnabled() && ui->windowModeButton.isEnabled() && ui->minimiseButton.isEnabled() && ui->closeAppButton.isEnabled(),
          "window controls remain available while source work is busy"); ui->setBusy(false);
    const auto image = ui->createComponentSnapshot(ui->getLocalBounds()); auto stream = report.getSiblingFile("full-monitor.png").createOutputStream(); juce::PNGImageFormat png;
    check(stream && png.writeImageToStream(image, *stream), "capture the production controls at full-monitor size");

    bool closed = false;
    ui->dirty = false; ui->requestClose([&] { closed = true; });
    check(closed && !ui->unsavedPromptOpen, "a clean mix closes immediately without an unnecessary prompt");
    closed = false; ui->dirty = true;
    ui->requestClose([&] { closed = true; });
    const auto closeRoot = report.getSiblingFile("close-fixture-" + juce::Uuid().toString()); closeRoot.createDirectory();
    const auto saveFile = closeRoot.getChildFile("close-saved.mixworks");
    const auto blockedFile = closeRoot.getChildFile("blocked.mixworks"); blockedFile.createDirectory();
    struct Poll final : juce::Timer { std::function<void()> tick; void timerCallback() override { tick(); } } poll;
    int phase = 0; const auto deadline = juce::Time::getMillisecondCounterHiRes() + 15000;
    poll.tick = [&]
    {
        auto finish = [&] { poll.stopTimer(); juce::MessageManager::getInstance()->stopDispatchLoop(); };
        if (juce::Time::getMillisecondCounterHiRes() > deadline) { check(false, "save-on-close dialogs complete within the test deadline"); finish(); return; }
        auto* alert = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        if (phase == 0 && alert)
        {
            check(alert->getButton("Save") && alert->getButton("Discard") && alert->getButton("Cancel"), "dirty mix offers Save, Discard and Cancel");
            const auto count = juce::ModalComponentManager::getInstance()->getNumModalComponents();
            ui->requestClose([&] { closed = true; });
            check(juce::ModalComponentManager::getInstance()->getNumModalComponents() == count, "repeated exit requests do not stack unsaved prompts");
            alert->triggerButtonClick("Cancel"); ++phase;
        }
        else if (phase == 1 && !ui->unsavedPromptOpen)
        {
            check(!closed && ui->dirty, "Cancel keeps the unsaved mix open");
            ui->requestClose([&] { closed = true; }); ++phase;
        }
        else if (phase == 2 && alert) { alert->triggerButtonClick("Discard"); ++phase; }
        else if (phase == 3 && closed)
        {
            check(ui->dirty && !saveFile.exists(), "Discard permits exit without writing or clearing the unsaved mix");
            closed = false; ui->sessionFile = saveFile; ui->engine.project.masterDb = -7;
            ui->requestClose([&] { closed = true; }); ++phase;
        }
        else if (phase == 4 && alert) { alert->triggerButtonClick("Save"); ++phase; }
        else if (phase == 5 && closed)
        {
            Engine reopened; const auto error = reopened.restoreProject(saveFile.loadFileAsString());
            check(!ui->dirty && error.isEmpty() && reopened.project.masterDb == -7, "Save writes and reopens the edited mix before allowing exit");
            closed = false; ui->dirty = true; ui->sessionFile = blockedFile;
            ui->requestClose([&] { closed = true; }); ++phase;
        }
        else if (phase == 6 && alert) { alert->triggerButtonClick("Save"); ++phase; }
        else if (phase == 7 && alert && alert->getName() == "Mixworks")
        {
            check(!closed && ui->dirty && blockedFile.isDirectory(), "failed save keeps the mix open and unsaved, preserving the blocking path");
            alert->exitModalState(0); ++phase;
        }
        else if (phase == 8 && !alert)
        {
            ui->setBusy(true); ui->requestClose([&] { closed = true; }); ++phase;
        }
        else if (phase == 9 && alert) { alert->triggerButtonClick("Save"); ++phase; }
        else if (phase == 10 && !ui->unsavedPromptOpen)
        {
            check(!closed && ui->dirty && ui->status.contains("current task"), "Save during active work keeps the mix open with a useful explanation");
            ui->requestClose([&] { closed = true; }); ++phase;
        }
        else if (phase == 11 && alert) { alert->triggerButtonClick("Discard"); ++phase; }
        else if (phase == 12 && closed)
        {
            check(ui->busy, "Discard still permits exit while a worker is active"); ui->setBusy(false); finish();
        }
    };
    poll.startTimer(20); juce::MessageManager::getInstance()->runDispatchLoop();
}
// Interactive acceptance: an external operator drives the real OS pickers.
// The fixture owns all paths and uses no audio device or user preferences.
void runNativePickerTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    const auto folder = report.getParentDirectory().getChildFile("owned-native-picker");
    const bool fresh = !folder.exists() && folder.createDirectory().wasOk();
    check(fresh, "native picker acceptance owns a fresh isolated fixture directory");
    if (!fresh) return;
    const auto source = folder.getChildFile(juce::String::fromUTF8("Owned picker – étude.wav"));
    const auto session = folder.getChildFile(juce::String::fromUTF8("Owned session – étude.mixworks"));
    const auto output = folder.getChildFile(juce::String::fromUTF8("Owned export – étude.wav"));
    {
        juce::AudioBuffer<float> audio(2, 32000);
        for (int i = 0; i < audio.getNumSamples(); ++i) for (int ch = 0; ch < 2; ++ch)
            audio.setSample(ch, i, .1f * (float) std::sin(i * 440. * juce::MathConstants<double>::twoPi / 8000));
        juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream = source.createOutputStream();
        auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(2).withBitsPerSample(24));
        const bool written = writer && writer->writeFromAudioSampleBuffer(audio, 0, audio.getNumSamples());
        check(written, "create the owned Unicode and space-containing picker source"); if (!written) return;
    }
    const auto sourceHash = juce::SHA256(source).toHexString();
    auto* ui = new Workstation(false); AppWindow window(ui); window.performDisplayAction(AppWindow::windowed);
    window.setName("Mixworks owned native picker acceptance"); window.setVisible(true);
    ui->engine.project.name = "Native picker acceptance"; ui->dirty = true;
    auto state = [&](const juce::String& phase)
    {
        auto* data = new juce::DynamicObject(); data->setProperty("phase", phase);
        data->setProperty("source", source.getFullPathName()); data->setProperty("session", session.getFullPathName()); data->setProperty("output", output.getFullPathName());
        report.getSiblingFile("picker-state.json").replaceWithText(juce::JSON::toString(juce::var(data)));
    };
    struct Poll final : juce::Timer { std::function<void()> tick; void timerCallback() override { tick(); } } poll;
    int phase = 0; const auto deadline = juce::Time::getMillisecondCounterHiRes() + 900000;
    poll.tick = [&]
    {
        auto finish = [&] { ui->chooser.reset(); poll.stopTimer(); juce::MessageManager::getInstance()->stopDispatchLoop(); };
        if (juce::Time::getMillisecondCounterHiRes() > deadline)
        { check(false, "interactive native pickers complete within 15 minutes"); state("timeout"); finish(); return; }
        if (phase == 0)
        { state("cancel-save"); ui->chooseSave(); phase = 1; }
        else if (phase == 1 && !ui->choosingSave)
        {
            check(ui->dirty && ui->engine.project.name == "Native picker acceptance" && !session.exists(), "cancelling the actual native Save picker preserves the unsaved session");
            state("import"); ui->chooseImport(); phase = 2;
        }
        else if (phase == 2 && !ui->busy && ui->engine.library.size() == 1)
        {
            const auto media = ui->engine.library[0];
            const bool admitted = media->file == source && media->frames() == 32000 && media->analysisSource.sha256 == sourceHash;
            check(admitted, "actual native Import picker catalogues the exact Unicode source through production import");
            if (!admitted) { finish(); return; }
            ui->engine.project.followTrackTempo = false; state("timeline-loading"); ui->addMedia(0); phase = 20;
        }
        else if (phase == 20 && !ui->busy && ui->timelineLoads.empty() && ui->engine.project.clips.size() == 1 && ui->engine.playbackReady())
        {
            check(ui->engine.project.clips[0].media->hasPlayback(), "normal timeline insertion hydrates the catalogued source before native file operations");
            state("save"); ui->chooseSave(); phase = 3;
        }
        else if (phase == 3 && !ui->choosingSave && ui->sessionFile == session && !ui->dirty)
        {
            check(session.existsAsFile(), "actual native Save picker writes the exact Unicode session path");
            ui->engine.project.name = "Transient reopen sentinel"; state("open"); ui->chooseOpen(); phase = 4;
        }
        else if (phase == 4 && !ui->busy && ui->engine.project.name == "Native picker acceptance")
        {
            check(ui->sessionFile == session && ui->engine.project.clips.size() == 1 && ui->engine.project.clips[0].media->file == source,
                  "actual native Open picker restores source identity and arrangement");
            state("export"); ui->chooseExport(); phase = 5;
        }
        else if (phase == 5 && !ui->busy && output.existsAsFile())
        {
            auto reader = std::unique_ptr<juce::AudioFormatReader>(ui->engine.formats.createReaderFor(output));
            check(reader && reader->numChannels == 2 && reader->bitsPerSample == 24 && reader->lengthInSamples == (juce::int64) std::llround(ui->engine.sampleRate.load() * 4),
                  "actual native Export picker and ending choice produce a complete four-second stereo PCM24 WAV");
            check(juce::SHA256(source).toHexString() == sourceHash, "native file workflows preserve the source bytes");
            auto image = report.getSiblingFile("native-picker-complete.png").createOutputStream(); juce::PNGImageFormat png;
            check(image && png.writeImageToStream(ui->createComponentSnapshot(ui->getLocalBounds()), *image), "capture the completed native file workflow");
            state("complete"); finish();
        }
    };
    poll.startTimer(100); juce::MessageManager::getInstance()->runDispatchLoop();
}

}
