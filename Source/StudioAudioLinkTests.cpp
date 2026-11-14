#include "Workstation.h"
#include <cmath>

namespace mixworks
{
void runStudioAudioLinkTests(const std::function<void(bool, const juce::String&)>& check,
                             const juce::File& report, const juce::StringArray& args)
{
    Workstation ui(false);
    auto& engine = ui.engine;
    auto& lane = *ui.sceneLane;
    auto menuItem = [&ui](bool ticked)
    {
        const auto menu = ui.getMenuForIndex(4, "View");
        for (juce::PopupMenu::MenuItemIterator it(menu); it.next();)
        {
            const auto& item = it.getItem();
            if (item.text == "Share master audio with Visual Studio") return item.isEnabled && item.isTicked == ticked;
        }
        return false;
    };
    check(menuItem(false) && !engine.studioSpectrum.enabled, "An isolated workstation stays off the shared audio port until requested");
    juce::DatagramSocket receiver;
    const bool bound = receiver.bindToPort(0, "127.0.0.1");
    check(bound && lane.shareStudioAudio(true, receiver.getBoundPort()) && menuItem(true),
          "Enable the actual Alloy audio link on an isolated loopback port");
    juce::DatagramSocket connected;
    const bool connectedBound = connected.bindToPort(0, "127.0.0.1");
    if (connectedBound) lane.connectStudioAudio(connected.getBoundPort());
    auto tone = std::make_shared<Media>();
    tone->sampleRate = 48000;
    tone->analysis.bpm = 128.5;
    tone->samples.setSize(1, 48000);
    for (int i = 0; i < 48000; ++i) tone->samples.setSample(0, i, .65f * (float) std::sin(i * .009));
    Clip clip; clip.media = tone; clip.length = 1.; engine.project.clips = {clip};
    engine.sampleRate = 48000; engine.project.masterDb = -6; engine.project.bpm = 123;
    engine.project.followTrackTempo = false;
    engine.monitorMuted = false; engine.setPlaying(true);
    constexpr int frames = 512;
    std::array<float, frames> left {}, right {};
    float* channels[] {left.data(), right.data()};
    auto render = [&] { engine.audioDeviceIOCallbackWithContext(nullptr, 0, channels, 2, frames, {}); };
    render();
    std::array<char, 28 + 1024 * sizeof(float)> packet {};
    const auto size = receiver.waitUntilReady(true, 1000) == 1 ? receiver.read(packet.data(), (int) packet.size(), false) : -1;
    juce::MemoryInputStream data(packet.data(), (size_t) std::max(0, size), false);
    char magic[4] {}; data.read(magic, 4);
    const auto sequence = data.readInt(), rate = data.readInt(), count = data.readInt();
    const auto detected = data.readFloat(), mix = data.readFloat(), phase = data.readFloat();
    check(detected == 128.5f && mix == 123.f
          && std::abs(phase - frames / 48000. * 123. / 60.) < .0001,
          "The packet separates detected track BPM, arranged tempo and the current mix beat phase");
    bool match = rate == 48000 && count == frames && size == 28 + frames * (int) sizeof(float);
    for (int i = 0; i < frames && match; ++i) match &= data.readFloat() == left[(size_t) i];
    check(sequence == 0 && std::string(magic, 4) == "MWV2" && match
          && juce::FloatVectorOperations::findMinAndMax(left.data(), frames).getLength() > .1f,
          "Loopback packet contains the exact post-master PCM produced by the live callback");
    std::array<char, 28 + 1024 * sizeof(float)> connectedPacket {};
    const int connectedSize = connectedBound && connected.waitUntilReady(true, 1000) == 1
        ? connected.read(connectedPacket.data(), (int) connectedPacket.size(), false) : -1;
    check(size > 0 && connectedSize == size && std::equal(packet.begin(), packet.begin() + size, connectedPacket.begin()),
          "The full Studio's private loopback port receives the same master PCM and clock");
    lane.connectStudioAudio(0);
    render();
    check(receiver.waitUntilReady(true, 1000) == 1 && receiver.read(packet.data(), (int)packet.size(), false) > 0
          && connected.waitUntilReady(true, 60) == 0,
          "Disconnecting linked Studio retains the standalone feed");
    check(!engine.visualSpectrum.enabled, "Standalone sharing needs no internal visual renderer");
    lane.shareStudioAudio(false);
    render();
    check(!engine.studioSpectrum.enabled && menuItem(false) && receiver.waitUntilReady(true, 60) == 0,
          "Disabling sharing stops packets while the mix keeps running");
    const int at = args.indexOf("--studio-test-port");
    if (at >= 0 && at + 1 < args.size())
    {
        const int port = args[at + 1].getIntValue();
        const bool started = port > 0 && port <= 65535 && lane.shareStudioAudio(true, port);
        check(started, "Enable link for the isolated Studio process");
        if (started)
            for (int i = 0; i < 52; ++i) { render(); juce::Thread::sleep(10); }
        lane.shareStudioAudio(false);
    }
    auto shot = report.getSiblingFile("alloy-version.png").createOutputStream();
    ui.setSize(1200, 750);
    check(shot && juce::PNGImageFormat().writeImageToStream(ui.createComponentSnapshot({0, 0, 1200, 85}), *shot),
          "Capture the actual Alloy title area");
}
}
