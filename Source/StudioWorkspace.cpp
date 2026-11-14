#include "StudioWorkspace.h"
#include "Workstation.h"
#include "MilkDrop.h"
#include "EnergySections.h"
#include "AutoArrangement.h"
#include <cmath>
#include <juce_cryptography/juce_cryptography.h>

namespace mixworks
{
namespace
{
juce::File runtime()
{
    return juce::File::getSpecialLocation(juce::File::currentExecutableFile).getSiblingFile("original-visualiser-runtime");
}
juce::var json(const juce::File& file)
{
    return file.existsAsFile() && file.getSize() < 1024 * 1024 ? juce::JSON::parse(file) : juce::var();
}
bool validOutput(const juce::var& view)
{
    if (!view.isObject()) return false;
    const auto token = view["token"].toString();
    const auto bounds = view["bounds"];
    if (token.length() != 32 || !token.containsOnly("0123456789abcdef") || bounds.size() != 4
        || (view.hasProperty("showFps") && !view["showFps"].isBool())) return false;
    for (int i = 0; i < bounds.size(); ++i)
    {
        if (!bounds[i].isInt()) return false;
        const int value = (int)bounds[i];
        if (i < 2 ? value < -32768 || value > 32768 : value < 1 || value > 16384) return false;
    }
    return true;
}
bool atPlayhead(const VisualSceneClip& clip, double position)
{
    return clip.start <= position && position < clip.end();
}
std::pair<int, double> timelineRank(const VisualSceneClip& clip, double position)
{
    if (atPlayhead(clip, position)) return {0, -clip.start};
    if (position < clip.start) return {1, clip.start};
    return {2, -clip.end()};
}
const std::map<juce::String, VisualControlSpec> milkSpecs {
    {"trail", {.5, 0, "Trail length"}}, {"zoom", {.5, 0, "Zoom"}}, {"spin", {.5, 0, "Spin"}},
    {"warp", {.5, 0, "Warp"}}, {"sensitivity", {.5, 0, "Beat sensitivity"}}
};
const std::map<juce::String, VisualControlSpec> mediaSpecs {
    {"overlayAmount", {.75, 0, "Amount"}}, {"spriteRoute", {0, 2, "Nested"}},
    {"spriteMotion", {0, 7, "Motion"}}, {"scale", {(.9 / 3.9), 0, "Scale"}},
    {"nestedAmount", {1, 0, "Injection"}}, {"nestedBeat", {0, 0, "Beat push"}}, {"nestedAngle", {.5, 0, "Angle"}},
    {"nestedPosition_x", {.5, 0, "Position X"}}, {"nestedPosition_y", {.5, 0, "Position Y"}},
    {"overlayMode", {0, 6, "Blend mode"}}, {"fill", {0, 2, "Fill"}}, {"flipX", {0, 2, "Flip X"}}, {"flipY", {0, 2, "Flip Y"}},
    {"invert", {0, 2, "Invert"}}, {"contrast", {1./3, 0, "Contrast"}}, {"gamma", {(.85 / 2.85), 0, "Gamma"}},
    {"hue", {.5, 0, "Hue"}}, {"saturation", {.5, 0, "Saturation"}}
};
const std::map<juce::String, VisualControlSpec> metaSpecs {
    {"master", {1, 0, "Master"}}, {"invert", {0, 2, "Invert"}}, {"brightness", {.5, 0, "Brightness"}},
    {"gamma", {(.85 / 2.85), 0, "Gamma"}}, {"contrast", {1./3, 0, "Contrast"}}, {"hue", {.5, 0, "Hue"}},
    {"saturation", {.5, 0, "Saturation"}}, {"vertical", {0, 2, "Mirror vertical"}}, {"horizontal", {0, 2, "Mirror horizontal"}},
    {"limitColors", {0, 2, "Limit colours"}}, {"lowColor_x", {.18, 0, "Low red"}}, {"lowColor_y", {.025, 0, "Low green"}},
    {"lowColor_z", {.035, 0, "Low blue"}}, {"highColor_x", {1, 0, "High red"}}, {"highColor_y", {.59, 0, "High green"}},
    {"highColor_z", {.17, 0, "High blue"}}, {"alpha", {0, 2, "Alpha channel"}}
};
const VisualSceneEntry stockMilk {"studio-milk/prism", "Prism Current"};
const VisualSceneEntry stockBackground {"studio-bg/copper", "Molten Alloy"};
const VisualSceneEntry stockSprite {"studio-sprite/circuit", "Circuit Martini"};

std::vector<VisualSceneEntry> artworkLibrary(const juce::File& install, bool sprite)
{
    const auto folder = install.getChildFile(sprite ? "Sprites" : "Backgrounds");
    auto files = folder.findChildFiles(juce::File::findFiles, false, "*", juce::File::FollowSymlinks::no);
    std::vector<VisualSceneEntry> entries;
    for (const auto& file : files)
        if (file.hasFileExtension("png;jpg;jpeg;webp;bmp;tif;tiff") && file.getFileName().length()<=240)
            entries.push_back({(sprite ? "studio-sprite/" : "studio-bg/")+file.getFileName(),
                               file.getFileNameWithoutExtension().substring(0,96)});
    std::sort(entries.begin(),entries.end(),[](const auto& a,const auto& b) { return a.key<b.key; });
    return entries;
}

class MilkDropPicker final : public juce::Component, private juce::ListBoxModel, private juce::Timer
{
public:
    MilkDropPicker(const std::vector<VisualSceneEntry>& catalogue,
                   std::function<bool(const juce::String&, const juce::String&)> place,
                   std::function<void()> openFile, bool replacing) : placePreset(std::move(place))
    {
        entries.reserve(catalogue.size() + 1);
        entries.emplace_back(stockMilk.key, stockMilk.title);
        for (const auto& preset : catalogue)
            if (isMilkDropKey(preset.key)) entries.emplace_back(preset.key, preset.title);

        for (juce::Component* component : std::array<juce::Component*, 5>{&search, &list, &count, &open, &add})
            addAndMakeVisible(*component);
        search.setTextToShowWhenEmpty("Search MilkDrop presets", colour::muted);
        search.setTitle("Search MilkDrop presets");
        search.onTextChange = [this] { startTimer(100); };
        search.onReturnKey = [this] { if (isTimerRunning()) timerCallback(); placeSelected(list.getSelectedRow()); };
        list.setTitle("MilkDrop presets");
        list.setRowHeight(28);
        count.setColour(juce::Label::textColourId, colour::muted);
        open.onClick = [this, openFile = std::move(openFile)] { close(); openFile(); };
        if (replacing) add.setButtonText("Replace preset");
        add.onClick = [this] { if (isTimerRunning()) timerCallback(); placeSelected(list.getSelectedRow()); };
        setSize(590, 470);
        timerCallback();
    }
    ~MilkDropPicker() override { stopTimer(); list.setModel(nullptr); }
    void resized() override
    {
        auto area = getLocalBounds().reduced(14);
        search.setBounds(area.removeFromTop(30));
        area.removeFromTop(8);
        auto footer = area.removeFromBottom(32);
        add.setBounds(footer.removeFromRight(112));
        footer.removeFromRight(8);
        open.setBounds(footer.removeFromRight(104));
        count.setBounds(footer);
        area.removeFromBottom(8);
        list.setBounds(area);
    }
    void focusSearch() { search.grabKeyboardFocus(); }

private:
    void timerCallback() override
    {
        stopTimer();
        const auto query = search.getText().trim();
        visible.clear();
        for (size_t i = 0; i < entries.size(); ++i)
            if (query.isEmpty() || entries[i].second.containsIgnoreCase(query)) visible.push_back(i);
        count.setText(juce::String((int)visible.size()) + (query.isEmpty() ? " presets" : " matches"), juce::dontSendNotification);
        list.deselectAllRows();
        list.updateContent();
        if (!visible.empty()) list.selectRow(0);
        add.setEnabled(!visible.empty());
    }
    int getNumRows() override { return (int)visible.size(); }
    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (!juce::isPositiveAndBelow(row, getNumRows())) return;
        if (selected) g.fillAll(colour::orange.withAlpha(.22f));
        g.setColour(colour::text);
        g.setFont(14.f);
        g.drawFittedText(entries[visible[(size_t)row]].second, 8, 0, width - 16, height, juce::Justification::centredLeft, 1);
    }
    void selectedRowsChanged(int row) override { add.setEnabled(juce::isPositiveAndBelow(row, getNumRows())); }
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override { placeSelected(row); }
    void returnKeyPressed(int row) override { placeSelected(row); }
    void placeSelected(int row)
    {
        if (!juce::isPositiveAndBelow(row, getNumRows())) return;
        const auto& [key, title] = entries[visible[(size_t)row]];
        if (placePreset(key, title)) close();
    }
    void close()
    {
        if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>()) dialog->exitModalState(0);
    }
    std::vector<std::pair<juce::String, juce::String>> entries;
    std::vector<size_t> visible;
    std::function<bool(const juce::String&, const juce::String&)> placePreset;
    juce::TextEditor search;
    juce::ListBox list {"MilkDrop presets", this};
    juce::Label count;
    juce::TextButton open {"Open file..."}, add {"Add preset"};
};

}

StudioWorkspace::StudioWorkspace(SceneLane& l) : lane(l), owner(l.owner)
{
    lane.setStudioWorkspace(this);
    for (juce::Component* item : std::array<juce::Component*, 9>{
         &backgrounds, &presets, &shaders, &sprites, &match, &output, &zoom, &scroll, &lane})
        addAndMakeVisible(*item);
    backgrounds.onClick = [this] { pick(VisualLayer::background, owner.engine.position.load()); };
    presets.onClick = [this] { pick(VisualLayer::milk, owner.engine.position.load()); };
    shaders.onClick = [this] { pick(VisualLayer::shader, owner.engine.position.load()); };
    sprites.onClick = [this] { pick(VisualLayer::sprite, owner.engine.position.load()); };
    output.onClick = [this] { openStudio(); };
    match.onClick = [this] { arrange(); };
    match.setTooltip("Arrange all four visual lanes from musical cues.");
    zoom.setRange(1, 60, .1); zoom.setValue(lane.studioScale); zoom.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    zoom.setSkewFactorFromMidPoint(10); zoom.setTooltip("Visual timeline zoom");
    zoom.onValueChange = [this] { lane.studioScale = zoom.getValue(); lane.repaint(); };
    scroll.setRange(0, 3600, .1); scroll.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    scroll.setTooltip("Scroll the visual timeline and turn off Follow");
    scroll.onValueChange = [this]
    {
        owner.setFollowPlayhead(false);
        lane.studioStart = scroll.getValue();
        lane.repaint();
    };
}
void StudioWorkspace::followPlayhead(double position)
{
    const int width = lane.getWidth() - SceneLane::studioGutter;
    if (width <= 0) return;
    const double x = lane.studioX(position) - SceneLane::studioGutter;
    if (x >= 0. && x < width) return;
    scroll.setValue(std::max(0., position - width / (5. * lane.studioScale)), juce::dontSendNotification);
    lane.studioStart = scroll.getValue();
    lane.repaint();
}
void StudioWorkspace::resized()
{
    auto area = getLocalBounds().reduced(6, 4);
    auto toolbar = area.removeFromTop(28);
    for (auto* button : {&backgrounds, &presets, &shaders, &sprites, &match, &output})
    {
        const auto width = std::max(56, std::min(button == &output ? 150 : 105, getWidth() / 6));
        button->setBounds(toolbar.removeFromLeft(width).reduced(2));
    }
    auto tools = area.removeFromBottom(22);
    zoom.setBounds(tools.removeFromRight(112)); scroll.setBounds(tools);
    lane.setBounds(area);
}
void StudioWorkspace::paint(juce::Graphics& g)
{
    g.fillAll(colour::panel);
}
StudioWorkspace::~StudioWorkspace()
{
    if (studioRoot.isDirectory()) studioRoot.getChildFile("closed").replaceWithText("closed");
}
bool StudioWorkspace::outputShowing() const { return const_cast<juce::ChildProcess&>(studioProcess).isRunning(); }
bool StudioWorkspace::openStudio()
{
    if (studioProcess.isRunning())
    {
        studioRoot.getChildFile("focus").replaceWithText("focus");
        return true;
    }
    const auto python = runtime().getSiblingFile("visual-python/pythonw.exe");
    const auto script = runtime().getChildFile("studio_engine/studio.py");
    if (!python.existsAsFile() || !script.existsAsFile())
    { owner.setStatus("The bundled Visual Studio is unavailable."); return false; }
    studioRoot = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("Alloy-Studio-" + juce::Uuid().toString());
    if (studioRoot.getChildFile("requests").createDirectory().failed()) return false;
    if (!lane.studioAudioShared() && !lane.shareStudioAudio(true, 0))
    { owner.setStatus("Could not start the local Visual Studio audio link."); return false; }
    lastExchange = 0; studioOutput = juce::var();
    pinnedClip.clear(); observedLaneSelection.clear(); lastGesture.clear(); cachedScene.clear(); swapStatus.clear();
    focusedLayer = -1;
    exchange();
    const bool started = studioProcess.start(juce::StringArray {python.getFullPathName(), "-B", script.getFullPathName(),
        "--alloy-session", studioRoot.getFullPathName(), "--projectm", runtime().getChildFile("milkdrop").getFullPathName()}, 0);
    studioConnecting = started;
    owner.setStatus(started ? "Opening Visual Studio..."
                            : "Visual Studio could not start. The timeline was kept.");
    return started;
}
void StudioWorkspace::update()
{
    double end = std::max(60., owner.engine.project.duration());
    for (const auto& c : owner.engine.project.visualScenes) end = std::max(end, c.end());
    if (scroll.getMaximum() != end) scroll.setRange(0, end, .1);
    if (studioProcess.isRunning())
    {
        exchange();
        if (studioConnecting && studioRoot.getChildFile("audio-port").existsAsFile())
        {
            studioConnecting = false;
            owner.setStatus("Visual Studio opened. It follows this mix's visual timeline and master audio.");
        }
    }
    else
    {
        lane.connectStudioAudio(0);
        if (studioConnecting)
        {
            studioConnecting = false;
            owner.setStatus("Visual Studio could not connect. Open Visual Studio again.");
        }
    }
}
const VisualSceneClip* StudioWorkspace::clipFor(const juce::String& id) const
{
    if (id.isEmpty()) return nullptr;
    const auto& clips = owner.engine.project.visualScenes;
    const auto found = std::find_if(clips.begin(), clips.end(), [&](const auto& clip) { return clip.id == id; });
    return found == clips.end() || found->scene.startsWith("original/") ? nullptr : &*found;
}
void StudioWorkspace::focusFromLane(const juce::String& id)
{
    observedLaneSelection = id;
    pinnedClip = clipFor(id) ? id : juce::String();
    focusedLayer = -1;
    lastGesture.clear();
}
std::array<const VisualSceneClip*, 4> StudioWorkspace::targetsAt(double position) const
{
    std::array<const VisualSceneClip*, 4> targets {};
    for (const auto& clip : owner.engine.project.visualScenes)
        if (!clip.scene.startsWith("original/"))
        {
            const auto layer = visualLayer(clip.scene);
            auto& candidate = targets[(size_t)layer];
            if (candidate && (layer == VisualLayer::shader || layer == VisualLayer::milk)
                && atPlayhead(clip, position) && atPlayhead(*candidate, position) && clip.transition == "cut"
                && position < clip.start + (candidate->end() - clip.start) * clip.pivot) continue;
            if (!candidate || timelineRank(clip, position) <= timelineRank(*candidate, position)) candidate = &clip;
        }
    return targets;
}
const VisualSceneClip* StudioWorkspace::focusedClip(double position) const
{
    if (const auto* clip = clipFor(pinnedClip)) return clip;
    const auto targets = targetsAt(position);
    if (focusedLayer >= 0) return targets[(size_t)focusedLayer];
    const VisualSceneClip* fallback = nullptr;
    for (const auto layer : {VisualLayer::shader, VisualLayer::milk, VisualLayer::sprite, VisualLayer::background})
        if (const auto* clip = targets[(size_t)layer])
        {
            if (atPlayhead(*clip, position)) return clip;
            if (!fallback || timelineRank(*clip, position) < timelineRank(*fallback, position)) fallback = clip;
        }
    return fallback;
}
StudioWorkspace::Specs StudioWorkspace::controlsFor(const VisualSceneClip& clip)
{
    if (cachedScene != clip.scene || clip.scene.startsWith("glsl/"))
    {
        cachedScene = clip.scene;
        cachedSpecs = specs(runtime(), clip.scene);
    }
    auto controls = cachedSpecs;
    if (lane.previewFacts["shaderClip"].toString() == clip.id)
        if (const auto* actors = lane.previewFacts["scenes"].getArray())
            if (std::any_of(actors->begin(), actors->end(), [&](const auto& actor)
                    { return actor["clip"].toString() == clip.id && actor["scene"].toString() == clip.scene; }))
                if (const auto* detected = lane.previewFacts["shaderControls"].getDynamicObject())
                    for (const auto& property : detected->getProperties())
                        controls[property.name.toString()] = {(double)property.value[0], (int)property.value[1], property.value[2].toString()};
    return controls;
}
void StudioWorkspace::exchange()
{
    // One isolated mailbox per Alloy instance. Commands are consumed once,
    // bound to a clip ID and a validated parameter, never retried on timeout.
    const auto requests = studioRoot.getChildFile("requests");
    const auto* laneSelection = lane.selectedVisual();
    const auto laneId = laneSelection ? laneSelection->id : juce::String();
    if (laneId != observedLaneSelection)
    {
        observedLaneSelection = laneId;
        if (clipFor(laneId)) focusFromLane(laneId);
    }
    if (!pinnedClip.isEmpty() && !clipFor(pinnedClip)) { pinnedClip.clear(); lastGesture.clear(); }
    const auto audioPort = studioRoot.getChildFile("audio-port").loadFileAsString().trim();
    const int port = audioPort.containsOnly("0123456789") ? audioPort.getIntValue() : 0;
    lane.connectStudioAudio(port >= 1024 && port <= 65535 ? port : 0);
    auto files = requests.findChildFiles(juce::File::findFiles, false, "*.json");
    files.sort();
    for (const auto& file : files)
    {
        const auto request = json(file);
        file.deleteFile();
        if (!request.isObject()) continue;
        const auto command = request["command"].toString();
        if (command == "focus")
        {
            const auto id = request["clip"].toString();
            if (id.isEmpty() || clipFor(id)) { pinnedClip = id; focusedLayer = -1; lastGesture.clear(); }
            continue;
        }
        if (command == "focus-lane")
        {
            const auto layer = (int)request["layer"];
            if (request["layer"].isInt() && layer >= 0 && layer < 4)
            { pinnedClip.clear(); focusedLayer = layer; lastGesture.clear(); }
            continue;
        }
        if (command == "sprite-route")
        {
            const auto position = owner.engine.position.load();
            const auto* sprite = clipFor(pinnedClip);
            if (!sprite || visualLayer(sprite->scene) != VisualLayer::sprite)
                sprite = targetsAt(position)[(size_t)VisualLayer::sprite];
            const auto route = request["route"].toString();
            if (sprite && sprite->id == request["clip"].toString() && sprite->scene == request["scene"].toString()
                && (route == "overlay" || route == "milk" || route == "shader"))
            {
                lastGesture.clear();
                edit(sprite->id, {}, 0., 0, {{"spriteRoute", route == "shader" ? 1. : 0.},
                                             {"spriteInMilkDrop", route == "milk" ? 1. : 0.}});
            }
            continue;
        }
        if (command == "output")
        {
            const auto view = request["view"];
            if (view.isVoid() || view.isUndefined()) studioOutput = juce::var();
            else if (validOutput(view)) studioOutput = view;
            continue;
        }
        if (command == "arrange") { arrange(); continue; }
        if (command == "swap")
        {
            const auto layer = (int)request["layer"];
            if (request["layer"].isInt() && layer >= 0 && layer < 4) swapRandom((VisualLayer)layer);
            continue;
        }
        if (command == "play" || command == "pause" || command == "stop" || command == "home")
        {
            if (owner.busy || owner.engine.exporting) continue;
            if (command == "home") owner.homeButton.onClick();
            else if (command == "stop") owner.stopButton.onClick();
            else if (owner.engine.playing.load() != (command == "play")) owner.playButton.onClick();
            continue;
        }
        const auto id = request["clip"].toString(), name = request["parameter"].toString();
        const auto* clip = focusedClip(owner.engine.position.load());
        if (!clip || clip->id != id || (request.hasProperty("scene") && request["scene"].toString() != clip->scene)) continue;
        if (command == "undo" || command == "redo")
        {
            if (!owner.busy && !owner.engine.exporting && !owner.engine.capturingPerformance()) owner.undo(command == "redo");
            lastGesture.clear(); continue;
        }
        const auto controls = controlsFor(*clip);
        if (command == "reset") { resetControls(*clip, request["parameters"]); continue; }
        const auto value = (double)request["value"];
        const auto action = (int)request["action"];
        if (const auto* batch = request["values"].getDynamicObject())
        {
            std::map<juce::String, double> values;
            bool valid = true;
            for (const auto& property : batch->getProperties())
            {
                const auto parameter = property.name.toString();
                const double number = (double)property.value;
                if (!controls.count(parameter) || !std::isfinite(number) || number < 0 || number > 1) { valid = false; break; }
                values.emplace(parameter, number);
            }
            if (valid && !values.empty())
            {
                const auto gesture = request["gesture"].toString();
                if (gesture != lastGesture) lastGesture.clear();
                edit(id, name, value, 0, values);
                lastGesture = gesture;
            }
            continue;
        }
        if (controls.count(name) && std::isfinite(value) && value >= 0 && value <= 1 && action >= 0 && action <= 2)
        {
            const auto gesture = request["gesture"].toString();
            if (gesture != lastGesture || action != 0) lastGesture.clear();
            edit(id, name, value, action, {}, request["settings"]);
            lastGesture = gesture;
        }
    }
    const auto now = juce::Time::getMillisecondCounterHiRes();
    if (now - lastExchange < 30.) return;
    lastExchange = now;
    auto* state = new juce::DynamicObject();
    state->setProperty("protocol", 1);
    state->setProperty("editable", !owner.busy && !owner.engine.exporting && !owner.engine.capturingPerformance());
    state->setProperty("updated", juce::Time::currentTimeMillis());
    const auto position = owner.engine.position.load();
    state->setProperty("position", position);
    state->setProperty("playing", owner.engine.playing.load());
    state->setProperty("hasVisuals", !owner.engine.project.visualScenes.empty());
    state->setProperty("milkDropLibrary", milkDropLibraryRoot().getFullPathName());
    state->setProperty("bpm", owner.engine.project.gridClock().bpmAt(owner.engine.position.load()));
    state->setProperty("status", lane.previewStatus);
    state->setProperty("swapStatus", swapStatus);
    state->setProperty("emptyPreview", (bool)lane.previewFacts["empty"]);
    if (lane.previewFacts["outputToken"] == studioOutput["token"] && studioOutput.isObject())
    {
        state->setProperty("outputToken", lane.previewFacts["outputToken"]);
        state->setProperty("outputError", lane.previewFacts["outputError"].toString());
        state->setProperty("outputWidth", (int)lane.previewFacts["outputWidth"]);
        state->setProperty("outputHeight", (int)lane.previewFacts["outputHeight"]);
    }
    const auto targets = targetsAt(position);
    const auto* pinned = clipFor(pinnedClip);
    const auto* clip = focusedClip(position);
    juce::Array<juce::var> choices;
    const auto addTarget = [&](const VisualSceneClip* target)
    {
        if (!target) return;
        auto* entry = new juce::DynamicObject();
        entry->setProperty("clip", target->id);
        entry->setProperty("title", target->title.substring(0, 120));
        entry->setProperty("layer", (int)visualLayer(target->scene));
        entry->setProperty("start", target->start);
        choices.add(juce::var(entry));
    };
    for (const auto* target : targets) addTarget(target);
    if (pinned && std::find(targets.begin(), targets.end(), pinned) == targets.end()) addTarget(pinned);
    state->setProperty("targets", juce::var(std::move(choices)));
    state->setProperty("pinned", pinned ? pinned->id : juce::String());
    state->setProperty("focusedLayer", focusedLayer);
    const auto* sprite = pinned && visualLayer(pinned->scene) == VisualLayer::sprite
        ? pinned : targets[(size_t)VisualLayer::sprite];
    if (sprite)
    {
        const auto grid = owner.engine.project.gridClock();
        auto controls = visualControlsAt(*sprite, position - sprite->start + sprite->offset,
                                         grid.beatsAt(position) - grid.beatsAt(sprite->start));
        for (const auto& [name, value] : overridesFor(*sprite, owner.engine.project.visualOverrides))
            controls[name] = value;
        auto* routing = new juce::DynamicObject();
        routing->setProperty("clip", sprite->id);
        routing->setProperty("scene", sprite->scene);
        routing->setProperty("route", controls["spriteInMilkDrop"] >= .5 ? "milk"
                                    : controls["spriteRoute"] >= .5 ? "shader" : "overlay");
        state->setProperty("spriteRouting", juce::var(routing));
    }
    if (clip)
    {
        state->setProperty("clip", clip->id); state->setProperty("title", clip->title);
        state->setProperty("scene", clip->scene); state->setProperty("layer", (int)visualLayer(clip->scene));
        state->setProperty("atPlayhead", atPlayhead(*clip, position));
        const auto controls = controlsFor(*clip);
        const auto grid = owner.engine.project.gridClock();
        auto values = visualControlsAt(*clip, position - clip->start + clip->offset, grid.beatsAt(position) - grid.beatsAt(clip->start));
        const auto overrides = overridesFor(*clip, owner.engine.project.visualOverrides);
        if (const auto* rendered = lane.previewFacts["scenes"].getArray())
            for (const auto& actor : *rendered)
                if ((juce::int64)lane.previewFacts["generation"] == (juce::int64)owner.engine.visualClockRevision.load()
                    && actor["clip"].toString() == clip->id && actor["scene"].toString() == clip->scene
                    && atPlayhead(*clip, position) && std::abs((double)actor["time"] - (position - clip->start + clip->offset)) < .3)
                    if (const auto* effective = actor["audioValues"].getDynamicObject())
                        for (const auto& item : effective->getProperties())
                        {
                            const auto curve = clip->automation.find(item.name.toString());
                            if (curve != clip->automation.end() && curve->second.audioSource == actor["audio"][item.name]["source"].toString()
                                && std::abs(curve->second.audioAmount - (double)actor["audio"][item.name]["amount"]) < 1.e-9)
                                values[item.name.toString()] = (double)item.value;
                        }
        auto* parameters = new juce::DynamicObject();
        for (const auto& [name, spec] : controls)
        {
            auto* item = new juce::DynamicObject(); item->setProperty("label", spec.label);
            item->setProperty("initial", spec.initial);
            item->setProperty("value", overrides.count(name) ? overrides.at(name) : values.count(name) ? values.at(name) : spec.initial);
            item->setProperty("steps", spec.steps);
            item->setProperty("held", overrides.count(name) != 0);
            const auto found = clip->automation.find(name);
            const auto curve = found == clip->automation.end() ? VisualControlCurve{} : found->second;
            item->setProperty("cycle", curve.cycle); item->setProperty("cycleMin", curve.cycleMin);
            item->setProperty("cycleMax", curve.cycleMax); item->setProperty("cycleBars", curve.cycleBars);
            item->setProperty("audioSource", curve.audioSource); item->setProperty("audioAmount", curve.audioAmount);
            parameters->setProperty(name, juce::var(item));
        }
        state->setProperty("parameters", juce::var(parameters));
    }
    juce::TemporaryFile temporary(studioRoot.getChildFile("state.json"));
    if (temporary.getFile().appendText(juce::JSON::toString(juce::var(state))))
        temporary.getFile().replaceFileIn(studioRoot.getChildFile("state.json"));
}

std::vector<VisualSceneEntry> StudioWorkspace::library(const juce::File& install)
{
    std::vector<VisualSceneEntry> entries;
    const auto data = json(install.getChildFile("studio/scene-library.json"));
    if (const auto* groups = data.getDynamicObject())
        for (const auto& group : groups->getProperties())
            if (const auto* scenes = group.value.getArray())
                for (const auto& scene : *scenes)
                {
                    const auto folder = scene["folder"].toString();
                    if (folder.isEmpty() || !folder.containsOnly("abcdefghijklmnopqrstuvwxyz0123456789-")) continue;
                    VisualSceneEntry entry;
                    entry.key = "studio/" + folder; entry.title = scene["title"].toString(); entry.family = group.name.toString();
                    entries.push_back(std::move(entry));
                }
    return entries;
}

StudioWorkspace::Specs StudioWorkspace::specs(const juce::File& install, const juce::String& key)
{
    switch (visualLayer(key))
    {
        case VisualLayer::background: return {{"backgroundMix", {.38, 0, "Blend amount"}}, {"backgroundNested", {0, 2, "Feed GLSL"}}};
        case VisualLayer::sprite:
        {
            auto controls = mediaSpecs;
            controls.emplace("spriteInMilkDrop", VisualControlSpec{0, 2, "Feed MilkDrop"});
            return controls;
        }
        case VisualLayer::milk:
        {
            auto controls = milkSpecs;
            controls.insert(metaSpecs.begin(), metaSpecs.end());
            return controls;
        }
        case VisualLayer::shader: break;
    }
    auto result = metaSpecs;
    if (!key.startsWith("studio/") && !key.startsWith("glsl/")) return {};
    result.emplace("milkMix", VisualControlSpec{.35, 0, "Original / MilkDrop"});
    const auto path = key.fromFirstOccurrenceOf("/", false, false);
    const auto source = key.startsWith("studio/") ? install.getChildFile("studio/scenes/" + path + "/main.glsl") : juce::File(path);
    const auto descriptor = source.getFileName() == "main.glsl" && source.getSiblingFile("scene.json").existsAsFile()
        ? source.getSiblingFile("scene.json") : source.withFileExtension(".json");
    const auto document = json(descriptor);
    const auto controls = document["CONTROLS"];
    if (const auto* list = controls.getArray())
        for (const auto& control : *list)
        {
            const auto name = control["NAME"].toString(), type = control["TYPE"].toString();
            const auto label = control["LABEL"].isString() ? control["LABEL"].toString() : name;
            if (name.isEmpty() || type == "bang") continue;
            const auto value = type == "color" ? control["DEF_COLOR"] : control["DEFAULT"];
            const auto low = control["MIN"], high = control["MAX"];
            const auto normal = [](double x, double a, double b) { return b > a ? juce::jlimit(0., 1., (x - a) / (b - a)) : 0.; };
            if (value.isArray())
            {
                const juce::StringArray suffixes {"_x", "_y", "_z"};
                for (int i = 0; i < std::min(3, value.size()); ++i)
                    result[name + suffixes[i]] = {normal((double)value[i], low.isArray() ? (double)low[i] : 0., high.isArray() ? (double)high[i] : 1.),
                                                   0, label + " " + suffixes[i].substring(1).toUpperCase()};
            }
            else if (type == "dropdown") result[name] = {normal((double)value, 0., (double)control["LABELS"].size()-1.), control["LABELS"].size(), label};
            else
            {
                const double a = (double)low, b = high.isVoid() ? 1. : (double)high;
                const int steps = type == "toggle" ? 2 : type == "integer" || b <= a ? std::max(1, (int)(b - a) + 1) : 0;
                result[name] = {normal((double)value, a, b), steps, label};
            }
        }
    return result;
}

juce::String StudioWorkspace::overrideKey(const VisualSceneClip& clip, const juce::String& parameter)
{
    const auto layer = visualLayer(clip.scene);
    auto scope = juce::String((int)layer);
    if (layer == VisualLayer::shader && !metaSpecs.count(parameter) && parameter != "milkMix")
    {
        if (clip.scene.startsWith("studio/"))
        {
            if (parameter == "motion") return scope + "/motion";
            static const auto families = []
            {
                std::map<juce::String, juce::String> result;
                for (const auto& entry : library(runtime())) result.emplace(entry.key, entry.family);
                return result;
            }();
            const auto family = families.find(clip.scene);
            if (family != families.end() && family->second != "Studio originals")
            {
                static juce::CriticalSection lock;
                static std::map<juce::String, Specs> declared;
                const juce::ScopedLock guard(lock);
                auto entry = declared.find(clip.scene);
                if (entry == declared.end()) entry = declared.emplace(clip.scene, specs(runtime(), clip.scene)).first;
                if (const auto control = entry->second.find(parameter); control != entry->second.end())
                    return scope + "/family/" + family->second + "/" + control->second.label + "/" + parameter;
            }
        }
        scope += "/" + clip.scene;
    }
    return scope + "/" + parameter;
}

VisualOverrides StudioWorkspace::overridesFor(const VisualSceneClip& clip, const VisualOverrides& overrides)
{
    VisualOverrides result;
    const auto prefix = juce::String((int)visualLayer(clip.scene)) + "/";
    for (auto it = overrides.lower_bound(prefix); it != overrides.end() && it->first.startsWith(prefix); ++it)
    {
        const auto parameter = it->first.fromLastOccurrenceOf("/", false, false);
        if (it->first == overrideKey(clip, parameter)) result.emplace(parameter, it->second);
    }
    return result;
}

void StudioWorkspace::resetControls(const VisualSceneClip& clip, const juce::var& parameters)
{
    if (!parameters.isArray() || owner.busy || owner.engine.exporting || owner.engine.capturingPerformance()) return;
    auto next = owner.engine.project.visualOverrides;
    const auto controls = controlsFor(clip);
    for (const auto& parameter : *parameters.getArray())
        if (parameter.isString() && controls.count(parameter.toString())) next.erase(overrideKey(clip, parameter.toString()));
    if (next == owner.engine.project.visualOverrides) return;
    owner.remember();
    { const juce::ScopedLock guard(owner.engine.lock); owner.engine.project.visualOverrides = std::move(next); }
    lastGesture.clear(); owner.refresh(); lane.repaint();
}

void StudioWorkspace::edit(const juce::String& id, const juce::String& name, double value, int action,
                           const std::map<juce::String, double>& values, const juce::var& settings)
{
    if (owner.busy || owner.engine.exporting || owner.engine.capturingPerformance()) return;
    const auto* clip = clipFor(id);
    if (!clip) return;
    if (action == 0)
    {
        auto next = owner.engine.project.visualOverrides;
        if (values.empty()) next[overrideKey(*clip, name)] = value;
        else for (const auto& [parameter, number] : values) next[overrideKey(*clip, parameter)] = number;
        if (next.size() > 4096 || next == owner.engine.project.visualOverrides) return;
        if (lastGesture.isEmpty()) owner.remember();
        { const juce::ScopedLock guard(owner.engine.lock); owner.engine.project.visualOverrides = std::move(next); }
        owner.refresh(); lane.repaint(); return;
    }
    const auto position = owner.engine.position.load();
    auto clips = owner.engine.project.visualScenes;
    auto selected = std::find_if(clips.begin(), clips.end(), [&](const auto& clip) { return clip.id == id; });
    if (selected == clips.end()) return;
    const auto specs = controlsFor(*selected);
    const auto spec = specs.find(name);
    if (spec == specs.end()) return;
    if (action == 1)
    {
        if (spec->second.steps != 0) return;
        auto& curve = selected->automation[name];
        curve.cycle = value > .5;
        if (settings.isObject())
        {
            const auto scalar = [&](const char* field, double& target)
            {
                if (!settings.hasProperty(field)) return true;
                const auto input = settings[field];
                if (!input.isDouble() && !input.isInt() && !input.isInt64()) return false;
                target = (double)input; return true;
            };
            if (!scalar("cycleMin", curve.cycleMin) || !scalar("cycleMax", curve.cycleMax)
                || !scalar("cycleBars", curve.cycleBars) || !scalar("audioAmount", curve.audioAmount)) return;
            if (settings.hasProperty("audioSource"))
            {
                if (!settings["audioSource"].isString()) return;
                curve.audioSource = settings["audioSource"].toString();
            }
        }
        if (curve.audioSource.isNotEmpty()) selected->controls.try_emplace(name, spec->second.initial);
    }
    else
    {
        if (position < selected->start || position > selected->end())
        { owner.setStatus("Move the playhead inside the selected visual clip to add a point."); return; }
        auto& curve = selected->automation[name];
        curve.stepped = spec->second.steps > 1;
        const auto base = selected->controls.find(name);
        const auto initial = base == selected->controls.end() ? spec->second.initial : base->second;
        if (curve.points.empty()) curve.points = {{0, initial}, {1, initial}};
        const auto fraction = juce::jlimit(0., 1., (position - selected->start) / selected->length);
        auto point = std::lower_bound(curve.points.begin(), curve.points.end(), fraction, [](const auto& p, double at) { return p.at < at; });
        if (point != curve.points.end() && std::abs(point->at - fraction) < .00001) point->value = value;
        else curve.points.insert(point, {fraction, value});
    }
    if (validateVisualScenes(clips).failed()) return;
    if (lane.selection == id) lane.studioParameter = name;
    owner.remember();
    { const juce::ScopedLock guard(owner.engine.lock); owner.engine.project.visualScenes = std::move(clips); }
    owner.refresh(); lane.repaint();
}
void StudioWorkspace::editPoint(double time, double value, bool remove)
{
    auto clips = owner.engine.project.visualScenes;
    auto clip = std::find_if(clips.begin(), clips.end(), [&](const auto& c) { return c.id == lane.selection; });
    if (clip == clips.end() || lane.studioParameter.isEmpty() || owner.busy || owner.engine.exporting || owner.engine.capturingPerformance()) return;
    const auto controls = controlsFor(*clip);
    const auto spec = controls.find(lane.studioParameter);
    if (spec == controls.end()) return;
    const auto fraction = juce::jlimit(0., 1., (time - clip->start) / clip->length);
    auto& curve = clip->automation[lane.studioParameter];
    auto& points = curve.points;
    if (points.empty())
    {
        if (remove) return;
        const auto base = clip->controls.find(lane.studioParameter);
        const auto initial = base != clip->controls.end() ? base->second : spec->second.initial;
        points = {{0, initial}, {1, initial}};
        curve.stepped = spec->second.steps > 1;
    }
    const auto closest = std::min_element(points.begin(), points.end(), [&](const auto& a, const auto& b)
        { return std::abs(a.at - fraction) < std::abs(b.at - fraction); });
    if (remove)
    {
        if (closest == points.begin() || closest == points.end() - 1) return;
        points.erase(closest);
    }
    else if (std::abs(closest->at - fraction) < 6 / (lane.studioScale * clip->length)) closest->value = value;
    else points.insert(std::lower_bound(points.begin(), points.end(), fraction, [](const auto& a, double b) { return a.at < b; }), {fraction, value});
    if (validateVisualScenes(clips).failed()) return;
    owner.remember();
    { const juce::ScopedLock guard(owner.engine.lock); owner.engine.project.visualScenes = std::move(clips); }
    owner.refresh(); lane.repaint();
}

void StudioWorkspace::swapRandom(VisualLayer layer)
{
    if (owner.busy || owner.engine.exporting || owner.engine.capturingPerformance()) return;
    const auto position = owner.engine.position.load();
    const auto* target = targetsAt(position)[(size_t)layer];
    const auto* active = target && atPlayhead(*target, position) ? target : nullptr;
    std::vector<VisualSceneEntry> choices;
    switch (layer)
    {
        case VisualLayer::background: choices = artworkLibrary(runtime(), false); break;
        case VisualLayer::sprite: choices = artworkLibrary(runtime(), true); break;
        case VisualLayer::shader: choices = library(runtime()); break;
        case VisualLayer::milk:
            choices.push_back(stockMilk);
            for (const auto& entry : lane.catalogue()) if (isMilkDropKey(entry.key)) choices.push_back(entry);
            break;
    }
    // Local files already on the timeline are available alongside the library.
    for (const auto& clip : owner.engine.project.visualScenes)
        if (visualLayer(clip.scene) == layer && (clip.scene.startsWith("glsl/") || clip.scene.startsWith("sprite/")
             || clip.scene.startsWith("background/")) && juce::File(clip.scene.fromFirstOccurrenceOf("/", false, false)).exists())
            choices.push_back({clip.scene, clip.title});
    std::set<juce::String> seen;
    choices.erase(std::remove_if(choices.begin(), choices.end(), [&](const auto& entry)
        { return (active && entry.key == active->scene) || !seen.insert(entry.key).second; }), choices.end());
    if (choices.empty()) { swapStatus = "No other item is available in this lane."; owner.setStatus(swapStatus); return; }
    const auto& choice = choices[(size_t)juce::Random::getSystemRandom().nextInt((int)choices.size())];
    if (active)
    {
        auto next = owner.engine.project.visualScenes;
        auto& replacement = next[(size_t)(active - owner.engine.project.visualScenes.data())];
        replacement.scene = choice.key; replacement.title = choice.title; replacement.offset = 0.;
        const auto nextSpecs = specs(runtime(), choice.key);
        const auto previousSpecs = controlsFor(*active);
        const auto compatible = [&](const auto& item)
        {
            return nextSpecs.count(item.first) && previousSpecs.count(item.first)
                && overrideKey(*active, item.first) == overrideKey(replacement, item.first);
        };
        for (auto it = replacement.controls.begin(); it != replacement.controls.end();)
            it = compatible(*it) ? std::next(it) : replacement.controls.erase(it);
        for (auto it = replacement.automation.begin(); it != replacement.automation.end();)
            it = compatible(*it) ? std::next(it) : replacement.automation.erase(it);
        if (validateVisualScenes(next).failed()) return;
        owner.remember();
        { const juce::ScopedLock guard(owner.engine.lock); owner.engine.project.visualScenes = std::move(next); }
        owner.refresh(); lane.repaint();
    }
    else if (!lane.placeStudio(choice.key, choice.title, position, 0., false)) return;
    swapStatus = "Swapped in " + choice.title;
    owner.setStatus(swapStatus); lastGesture.clear();
}

bool StudioWorkspace::applyVisualChoice(const juce::String& key, const juce::String& title, double time,
                                        const juce::String& replacement)
{
    if (replacement.isEmpty()) return lane.placeStudio(key, title, time);
    return lane.replaceScene(replacement, VisualSceneEntry{key, title});
}

void StudioWorkspace::addFile(VisualLayer layer, double time, const juce::String& replacement)
{
    const auto extensions = layer == VisualLayer::shader ? "*.glsl;*.frag;*.fs"
        : layer == VisualLayer::milk ? "*.milk" : "*.png;*.jpg;*.jpeg;*.webp;*.bmp;*.tiff";
    chooser = std::make_unique<juce::FileChooser>("Choose a local visual asset", juce::File(), extensions);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe = juce::Component::SafePointer<StudioWorkspace>(this), layer, time, replacement](const juce::FileChooser& picker)
        {
            const auto file = picker.getResult();
            if (!safe || !file.existsAsFile()) return;
            juce::String key;
            if (layer == VisualLayer::milk)
            {
                const auto imported = importMilkDropPreset(file, milkDropLibraryRoot(), key);
                if (imported.failed()) { safe->owner.setStatus(imported.getErrorMessage()); return; }
            }
            else
                key = juce::String(layer == VisualLayer::shader ? "glsl/" : layer == VisualLayer::background ? "background/" : "sprite/") + file.getFullPathName();
            safe->applyVisualChoice(key, file.getFileNameWithoutExtension(), time, replacement);
        });
}

void StudioWorkspace::pick(VisualLayer layer, double time, const juce::String& replacement)
{
    if (owner.busy || owner.engine.exporting || owner.engine.capturingPerformance()) return;
    if (layer == VisualLayer::milk)
    {
        const auto safe = juce::Component::SafePointer<StudioWorkspace>(this);
        auto* content = new MilkDropPicker(lane.catalogue(),
            [safe, time, replacement](const auto& key, const auto& title) { return safe && safe->applyVisualChoice(key, title, time, replacement); },
            [safe, time, replacement] { if (safe) safe->addFile(VisualLayer::milk, time, replacement); },
            replacement.isNotEmpty());
        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned(content);
        options.componentToCentreAround = this;
        options.dialogTitle = replacement.isEmpty() ? "MilkDrop presets" : "Replace with MilkDrop";
        options.dialogBackgroundColour = colour::panel;
        options.useNativeTitleBar = true;
        options.escapeKeyTriggersCloseButton = true;
        options.launchAsync();
        content->focusSearch();
        return;
    }
    juce::PopupMenu menu;
    menu.addItem(1, "Open file...");
    std::vector<VisualSceneEntry> entries;
    if (layer == VisualLayer::shader) entries = library(runtime());
    else if (layer == VisualLayer::background) entries = artworkLibrary(runtime(),false);
    else entries = artworkLibrary(runtime(),true);
    std::map<juce::String, juce::PopupMenu> groups;
    for (size_t i = 0; i < entries.size(); ++i)
    {
        const auto& entry = entries[i];
        auto& group = entry.family.isNotEmpty() ? groups[entry.family] : menu;
        group.addItem((int)i + 2, entry.title);
    }
    for (auto& [name, group] : groups) menu.addSubMenu(name, group);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&lane),
        [safe = juce::Component::SafePointer<StudioWorkspace>(this), layer, time, replacement, entries = std::move(entries)](int choice)
        {
            if (!safe || choice == 0) return;
            if (choice == 1) safe->addFile(layer, time, replacement);
            else if (juce::isPositiveAndBelow(choice - 2, (int)entries.size()))
            {
                const auto& entry = entries[(size_t)choice - 2];
                safe->applyVisualChoice(entry.key, entry.title, time, replacement);
            }
        });
}

juce::Result StudioWorkspace::arrangeProject(Project& project, VisualSceneSource sources)
{
    return arrangeProject(project, sources, nullptr);
}
juce::Result StudioWorkspace::arrangeProject(Project& project, VisualSceneSource sources, juce::Random* choices)
{
    const auto install = runtime();
    const auto available = library(install);
    if (available.empty() && sources != VisualSceneSource::milkdrop)
        return juce::Result::fail("The Studio scene library could not be loaded.");
    if (project.clips.empty()) return juce::Result::fail("Add music to the mix before auto arranging visuals.");
    const auto grid = project.gridClock();
    const auto end = project.duration();
    struct Join { double cut, start, end; juce::String preset; };
    if (!std::isfinite(end) || end < .05) return juce::Result::fail("The mix is too short to arrange visuals.");
    std::vector<Join> primaryCues;
    std::vector<double> backgroundCues, milkCues, spriteCues;
    const bool anySolo = std::any_of(project.tracks.begin(), project.tracks.end(), [](const auto& t) { return t.solo; });
    const auto audible = [&](const Clip& clip)
    {
        const auto& track = project.tracks[(size_t)clip.track];
        return clip.media && !track.mute && (!anySolo || track.solo);
    };
    std::map<juce::String, Join> arrivals;
    std::map<juce::String, double> departures;
    for (const auto& link : project.transitionLinks)
    {
        const auto* outgoing = project.clipFor(link.outgoing);
        const auto* incoming = project.clipFor(link.incoming);
        if (!outgoing || !incoming || !audible(*outgoing) || !audible(*incoming)) continue;
        const auto range = project.transitionRange(link);
        const auto cut = alignedSectionCut(project, link);
        Join cue {cut, cut, cut, "cut"};
        if (!link.alignmentOnly && link.bassSwap && range.getLength() >= .1)
        {
            const auto& bass = *link.bassSwap;
            cue.cut = range.clipValue(incoming->start + incoming->localTimeForSource((bass[2] + bass[3]) * .5));
            const auto beat = grid.beatsAt(cue.cut);
            cue.start = std::max(range.getStart(), grid.timeAtBeat(beat - 2.));
            cue.end = std::min(range.getEnd(), grid.timeAtBeat(beat + 2.));
            cue.preset = "bass-handoff";
        }
        else if (!link.alignmentOnly && grid.beatsAt(range.getEnd()) - grid.beatsAt(range.getStart()) >= 8.)
            cue = {cut, range.getStart(), range.getEnd(), "dissolve"};
        arrivals.emplace(link.incoming, cue);
        departures.emplace(link.outgoing, cue.cut);
    }

    struct Passage { const Clip* clip; double begin, end; };
    std::vector<Passage> passages;
    for (const auto& clip : project.clips)
    {
        if (!audible(clip)) continue;
        const auto playback = project.transitionPlaybackFor(clip);
        double begin = std::max(clip.start, playback.startCut.value_or(clip.start));
        double finish = std::min(clip.end(), playback.endCut.value_or(clip.end()));
        if (const auto in = arrivals.find(clip.id); in != arrivals.end()) begin = std::max(begin, in->second.cut);
        if (const auto out = departures.find(clip.id); out != departures.end()) finish = std::min(finish, out->second);
        if (finish > begin) passages.push_back({&clip, begin, finish});
    }
    std::stable_sort(passages.begin(), passages.end(), [](const auto& a, const auto& b) { return a.begin < b.begin; });
    for (size_t i = 1; i < passages.size(); ++i)
        passages[i - 1].end = std::min(passages[i - 1].end, passages[i].begin);
    std::optional<double> backgroundEnergy;
    std::optional<std::array<double, 2>> groove;
    const Passage* previous = nullptr;
    for (const auto& passage : passages)
    {
        const auto& clip = *passage.clip;
        const auto begin = passage.begin, finish = passage.end;
        if (finish <= begin) continue;
        const bool forward = clip.loopSpan <= 0. && !clip.reversed;
        const bool continuousSource = previous && forward && !previous->clip->reversed && previous->clip->loopSpan <= 0.
            && previous->clip->media->id == clip.media->id && std::abs(previous->end - begin) < .05
            && std::abs(previous->clip->sourceTimeAt(begin - previous->clip->start) - clip.sourceTimeAt(begin - clip.start)) < .05;
        if (const auto in = arrivals.find(clip.id); in != arrivals.end()) primaryCues.push_back(in->second);
        else if (previous && !continuousSource) primaryCues.push_back({begin, begin, begin, "cut"});
        previous = &passage;
        if (!forward) continue;

        const auto timelineAt = [&](double source) { return clip.start + clip.audioTimelineTime(source - clip.offset); };
        const auto placeOnBar = [&](double time)
        {
            const auto downbeat = grid.snap(time, true, 4.);
            return downbeat > begin && downbeat < finish ? downbeat : time;
        };
        const double sourceBegin = clip.sourceTimeAt(begin - clip.start), sourceEnd = clip.sourceTimeAt(finish - clip.start);
        const auto& profile = clip.media->energy;
        const bool measured = profile.complete();
        const auto bassActivity = [&](double low, double high)
        {
            double sum = 0.;
            for (const auto& w : profile.windows)
            {
                if (w.start >= high) break;
                sum += w.bass() * std::max(0., std::min(high, w.end) - std::max(low, w.start));
            }
            return sum / (high - low);
        };
        for (const auto& section : clipEnergySections(clip))
        {
            const auto time = timelineAt(section.sourceSeconds);
            if (!std::isfinite(time) || time <= begin || time >= finish
                || (!section.manual && std::abs(section.contrastDb) < 4.)) continue;
            const auto cut = section.manual ? time : placeOnBar(time);
            const bool recede = section.contrastDb <= -4.;
            const auto blendStart = recede ? std::max(begin, grid.timeAtBeat(grid.beatsAt(cut) - 2.)) : cut;
            primaryCues.push_back({cut, blendStart, cut, recede ? "dissolve" : "cut"});

            // A rise needs separate bass evidence before it can replace a sprite.
            const auto source = section.sourceSeconds;
            if (measured && section.contrastDb >= 6. && source - 8. >= std::max(0., sourceBegin)
                && source + 8. <= std::min(profile.duration, sourceEnd))
            {
                const auto before = bassActivity(source - 8., source);
                const auto after = bassActivity(source, source + 8.);
                if (before <= .25 && after >= .45 && after - before >= .25) spriteCues.push_back(cut);
            }
        }
        if (clip.hotcues)
            for (const auto& hit : *clip.hotcues)
                if (hit && hit->manual)
                {
                    const auto time = timelineAt(hit->sourceSeconds);
                    if (time > begin && time < finish) spriteCues.push_back(time);
                }
        if (!measured) continue;

        // Compare two consecutive activity windows against the sound that chose
        // the current look. A single fill or a window boundary cannot change it.
        const auto sustained = [](double a, double b, double baseline, double threshold)
        {
            return std::min(a, b) >= baseline + threshold || std::max(a, b) <= baseline - threshold;
        };
        for (size_t i = 1; i < profile.windows.size(); ++i)
        {
            const auto& a = profile.windows[i - 1];
            const auto& b = profile.windows[i];
            if (std::min(a.end, sourceEnd) - std::max(a.start, sourceBegin) < 6.
                || std::min(b.end, sourceEnd) - std::max(b.start, sourceBegin) < 6.) continue;
            const auto cut = placeOnBar(std::max(begin, timelineAt(a.start)));
            const auto energy = (a.score() + b.score()) * .5;
            const std::array<double, 2> density {(a.onsetsPerSecond + b.onsetsPerSecond) * .5,
                                                 (a.bassOnsetsPerSecond + b.bassOnsetsPerSecond) * .5};
            if (!backgroundEnergy) backgroundEnergy = energy;
            else if (sustained(a.score(), b.score(), *backgroundEnergy, .20))
            {
                backgroundCues.push_back(cut);
                backgroundEnergy = energy;
            }
            if (!groove) groove = density;
            else if (sustained(a.onsetsPerSecond, b.onsetsPerSecond, (*groove)[0], 1.5)
                || sustained(a.bassOnsetsPerSecond, b.bassOnsetsPerSecond, (*groove)[1], .8))
            {
                milkCues.push_back(cut);
                groove = density;
            }
        }
    }
    const auto edgesFor = [end](std::vector<double> cues)
    {
        std::sort(cues.begin(), cues.end());
        std::vector<double> edges {0.};
        for (const auto cue : cues)
            if (cue > edges.back() + .05 && cue < end - .05) edges.push_back(cue);
        edges.push_back(end);
        return edges;
    };
    std::vector<double> primaryTimes;
    for (const auto& cue : primaryCues) primaryTimes.push_back(cue.cut);
    const auto cuts = edgesFor(std::move(primaryTimes));
    const auto backgroundCuts = edgesFor(std::move(backgroundCues));
    const auto spriteCuts = edgesFor(std::move(spriteCues));
    if (sources == VisualSceneSource::milkdrop)
        for (const auto& cue : primaryCues) milkCues.push_back(cue.cut);
    const auto milkCuts = edgesFor(std::move(milkCues));
    std::vector<VisualSceneEntry> milk = discoverMilkDropScenes(milkDropLibraryRoot());
    milk.insert(milk.begin(), stockMilk);
    juce::Random fresh;
    auto& random = choices ? *choices : fresh;
    const auto shuffle = [&](auto& entries)
    {
        for (int i = (int)entries.size() - 1; i > 0; --i)
            std::swap(entries[(size_t)i], entries[(size_t)random.nextInt(i + 1)]);
    };
    shuffle(milk);
    // Deal one shuffled scene from each shuffled family per round. This keeps
    // a large family from dominating, with no repeated scene until exhaustion.
    std::map<juce::String, std::vector<VisualSceneEntry>> grouped;
    for (const auto& entry : available) grouped[entry.family].push_back(entry);
    for (auto& [name, entries] : grouped) shuffle(entries);
    std::vector<VisualSceneEntry> shaders;
    while (shaders.size() < available.size())
    {
        std::vector<juce::String> order;
        for (const auto& [name, entries] : grouped) if (!entries.empty()) order.push_back(name);
        shuffle(order);
        if (order.size() > 1 && !shaders.empty() && order.front() == shaders.back().family)
            std::swap(order.front(), order.back());
        for (const auto& family : order)
        {
            auto& entries = grouped.at(family);
            shaders.push_back(entries.back()); entries.pop_back();
        }
    }
    auto backgrounds = artworkLibrary(install,false);
    auto sprites = artworkLibrary(install,true);
    shuffle(backgrounds);
    shuffle(sprites);
    std::vector<VisualSceneClip> clips;
    const auto add = [&](const VisualSceneEntry& entry, double start, double finish) -> VisualSceneClip&
    {
        VisualSceneClip clip;
        clip.scene = entry.key; clip.title = entry.title; clip.start = start;
        clip.length = finish - start; clip.transition = "cut";
        clips.push_back(std::move(clip)); return clips.back();
    };
    const auto joinPrimary = [&](const auto& edges, size_t i, VisualSceneClip& outgoing, VisualSceneClip& incoming)
    {
        const auto join = std::find_if(primaryCues.begin(), primaryCues.end(), [&](const auto& item)
        { return item.preset != "cut" && std::abs(item.cut - edges[i]) < .05; });
        if (join == primaryCues.end()) return;
        const auto start = std::max(join->start, (edges[i - 1] + edges[i]) * .5);
        const auto finish = std::min(join->end, (edges[i] + edges[i + 1]) * .5);
        if (finish - start < .05) return;
        const auto incomingEnd = incoming.end();
        outgoing.length = finish - outgoing.start;
        incoming.start = start; incoming.length = incomingEnd - start;
        incoming.transition = join->preset;
        incoming.pivot = juce::jlimit(0., 1., (join->cut - start) / (finish - start));
    };
    if (sources != VisualSceneSource::milkdrop && !backgrounds.empty())
    {
        std::map<juce::String,juce::String> hashes;
        const auto folder = install.getChildFile("Backgrounds");
        const auto hashOf = [&](const VisualSceneEntry& image) -> const juce::String&
        {
            auto [hash, inserted] = hashes.try_emplace(image.key);
            if (inserted)
                hash->second = juce::SHA256(folder.getChildFile(image.key.fromFirstOccurrenceOf("/",false,false))).toHexString();
            return hash->second;
        };
        juce::String previous;
        size_t next = 0;
        for (size_t i = 0; i + 1 < backgroundCuts.size(); ++i)
        {
            size_t checked = 0;
            while (checked < backgrounds.size() && hashOf(backgrounds[next]) == previous)
            {
                next = (next + 1) % backgrounds.size();
                ++checked;
            }
            if (checked == backgrounds.size())
                clips.back().length = backgroundCuts[i+1]-clips.back().start;
            else
            {
                const auto& image = backgrounds[next];
                add(image,backgroundCuts[i],backgroundCuts[i+1]).controls={{"backgroundMix",.38},{"backgroundNested",1.}};
                previous = hashOf(image);
                next = (next + 1) % backgrounds.size();
            }
        }
    }
    if (sources != VisualSceneSource::originals && milk.size() == 1)
        add(milk.front(), 0., end);
    else if (sources != VisualSceneSource::originals)
        for (size_t i = 0; i + 1 < milkCuts.size(); ++i)
        {
            add(milk[i % milk.size()], milkCuts[i], milkCuts[i+1]);
            if (i > 0 && sources == VisualSceneSource::milkdrop)
                joinPrimary(milkCuts, i, clips[clips.size() - 2], clips.back());
        }
    if (sources != VisualSceneSource::milkdrop)
        for (size_t i = 0; i + 1 < cuts.size(); ++i)
        {
            add(shaders[i % shaders.size()], cuts[i], cuts[i+1]).controls["milkMix"] = .2 + .25 * random.nextDouble();
            if (i > 0) joinPrimary(cuts, i, clips[clips.size() - 2], clips.back());
        }
    if (!sprites.empty())
    {
        const bool nestInGlsl = sources == VisualSceneSource::originals;
        const auto edges = sprites.size() == 1 ? std::vector<double>{0., end} : spriteCuts;
        for (size_t i = 0; i + 1 < edges.size(); ++i)
            add(sprites[i % sprites.size()], edges[i], edges[i+1]).controls = {
                {"overlayAmount", .52}, {"spriteMotion", (double)(i%6+1)/6.},
                {"spriteRoute", nestInGlsl ? 1. : 0.}, {"spriteInMilkDrop", nestInGlsl ? 0. : 1.}};
    }
    std::stable_sort(clips.begin(), clips.end(), [](const auto& a, const auto& b) { return a.start < b.start; });
    const auto result = validateVisualScenes(clips);
    if (result.wasOk()) project.visualScenes = std::move(clips);
    return result;
}

bool StudioWorkspace::arrange()
{
    if (owner.busy || owner.engine.exporting || owner.engine.capturingPerformance()) return false;
    if (!owner.engine.project.visualScenes.empty())
    {
        juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::QuestionIcon, "Auto arrange visuals",
            "Replace the four visual lanes with a new arrangement? Undo can restore this arrangement.",
            "Replace", "Cancel", this,
            juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<StudioWorkspace>(this)](int answer)
            { if (safe && answer != 0) safe->publishArrangement(); }));
        return true;
    }
    return publishArrangement();
}
bool StudioWorkspace::publishArrangement()
{
    if (owner.busy || owner.engine.exporting || owner.engine.capturingPerformance()) return false;
    auto project = owner.engine.project;
    const auto result = arrangeProject(project);
    if (result.failed()) { owner.setStatus(result.getErrorMessage()); return false; }
    owner.remember();
    { const juce::ScopedLock guard(owner.engine.lock); owner.engine.project.visualScenes = std::move(project.visualScenes); }
    owner.refresh(); owner.setStatus("Visual Studio arranged " + juce::String((int)owner.engine.project.visualScenes.size()) + " clips.");
    return true;
}
void StudioWorkspace::runChecks(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    Workstation desk(false);
    desk.stopTimer();
    desk.setSize(1440, 940);
    desk.studioTab.onClick();
    auto& studio = *desk.studioWorkspace;
    auto& lane = *desk.sceneLane;
    check(desk.studioPanel->isVisible() && lane.getParentComponent() == &studio && desk.layout.visuals.isEmpty(),
          "Studio has a dedicated tab and music has no embedded visual lane");
    const auto source = [](const juce::String& name)
    {
        auto media = std::make_shared<Media>();
        media->name = name; media->sampleRate = 8000;
        media->analysisSource.frames = 96 * 8000; media->analysisSource.channels = 1;
        media->analysis.bpm = 120; media->analysis.tempoConfidence = 1;
        media->analysis.downbeatOrigin = 0;
        media->analysis.key = 0; media->analysis.keyConfidence = 1;
        return media;
    };
    const auto first = source("First"), second = source("Second");
    AutoArrangementOptions options;
    options.startingId = first->id; options.trackCount = 2;
    juce::Random random(718);
    auto arranged = planAutoArrangement({first, second}, options, random);
    check(arranged.error.isEmpty() && arranged.project.transitionLinks.size() == 1, "automatic music planner makes a valid join");
    if (arranged.project.transitionLinks.size() == 1)
    {
        auto& mix = arranged.project;
        const auto& link = mix.transitionLinks.front();
        const auto cut = alignedSectionCut(mix, link);
        const auto* before = mix.clipFor(link.outgoing);
        const auto* after = mix.clipFor(link.incoming);
        const auto valueAt = [&](const Clip& clip, AutomationTarget parameter, double time)
        {
            const AutomationAddress address{clip.track, parameter, {}, {}, clip.id};
            return mix.effectSettings(clip).automation[(size_t)parameter].valueAt(mix.automationTime(address, time), mix.manualValue(address));
        };
        const auto span = mix.transitionRange(link);
        const auto midpoint = span.getStart() + span.getLength() * .5;
        check(std::abs(mix.gridClock().beatsAt(cut) / 4 - std::round(mix.gridClock().beatsAt(cut) / 4)) < 1.e-6
              && valueAt(*before, AutomationTarget::volume, midpoint) > -60
              && valueAt(*after, AutomationTarget::volume, midpoint) > -60,
              "the visual change stays on a bar while the default music join blends both tracks");
        const auto visuals = arrangeProject(mix);
        std::set<double> interior[4];
        for (const auto& c : mix.visualScenes)
            if (c.start > .05) interior[(int)visualLayer(c.scene)].insert(c.start);
        check(visuals.wasOk() && interior[0].empty() && interior[1].empty() && interior[2].size() == 1 && interior[3].empty(),
              "a handover without other evidence changes GLSL only");
        check(std::any_of(mix.visualScenes.begin(), mix.visualScenes.end(), [&](const auto& clip)
        {
            return visualLayer(clip.scene) == VisualLayer::shader && clip.start > .05 && clip.start <= cut && clip.end() > cut
                && (clip.transition == "dissolve" || clip.transition == "bass-handoff");
        }), "Match Mix makes an overlapping visual preset at the music handover");
        for (const auto source : {VisualSceneSource::originals, VisualSceneSource::milkdrop})
        {
            auto subset = mix; const auto result = arrangeProject(subset, source);
            check(result.wasOk() && std::none_of(subset.visualScenes.begin(), subset.visualScenes.end(), [&](const auto& c)
                { return visualLayer(c.scene) == (source == VisualSceneSource::originals ? VisualLayer::milk : VisualLayer::shader); }),
                "single-engine arrangements respect the selected source");
            double covered = 0.;
            bool routed = true;
            const bool glsl = source == VisualSceneSource::originals;
            for (const auto& clip : subset.visualScenes)
                if (visualLayer(clip.scene) == VisualLayer::sprite)
                {
                    const auto shader = clip.controls.find("spriteRoute");
                    const auto milk = clip.controls.find("spriteInMilkDrop");
                    routed &= std::abs(clip.start - covered) < 1.e-7
                        && shader != clip.controls.end() && shader->second == (glsl ? 1. : 0.)
                        && milk != clip.controls.end() && milk->second == (glsl ? 0. : 1.);
                    covered = clip.end();
                }
            check(result.wasOk() && routed && covered > 0. && std::abs(covered - subset.duration()) < 1.e-7,
                  glsl ? "GLSL-only arrangements cover the mix with sprites routed to GLSL"
                       : "MilkDrop-only arrangements cover the mix with sprites routed to MilkDrop");
        }
    }
    const auto scenePool = library(runtime());
    std::vector<juce::String> firstChoices;
    for (int run = 0; run < 4; ++run)
    {
        auto mix = arranged.project;
        juce::Random seeded(910 + run);
        arrangeProject(mix, VisualSceneSource::both, &seeded);
        for (const auto& c : mix.visualScenes)
            if (visualLayer(c.scene) == VisualLayer::shader) { firstChoices.push_back(c.scene); break; }
    }
    std::set<juce::String> distinct(firstChoices.begin(), firstChoices.end());
    check(distinct.size() > 1 && scenePool.size() >= 200, "repeated arrangements vary their starting scene across the full Studio library");
    const auto cueTrack = source("Cue fixture");
    cueTrack->analysisSource.frames = 384 * 8000;
    cueTrack->energy = {musicalEnergyRevision, 384., {}};
    for (double at = 0.; at < 384.; at += 8.)
        cueTrack->energy.windows.push_back({at, at + 8., .8, 2., .5, .25, 5., 8.});
    Project selectionShow;
    Clip selectionClip; selectionClip.media = cueTrack; selectionClip.length = cueTrack->duration();
    selectionShow.clips.push_back(selectionClip);
    juce::Random selectionSeed(731);
    const auto selectionResult = arrangeProject(selectionShow, VisualSceneSource::both, &selectionSeed);
    const auto startsFor = [](const Project& show, VisualLayer layer)
    {
        std::vector<double> starts;
        for (const auto& clip : show.visualScenes)
            if (visualLayer(clip.scene) == layer) starts.push_back(clip.start);
        return starts;
    };
    check(selectionResult.wasOk() && selectionShow.visualScenes.size() == 4
          && std::all_of(selectionShow.visualScenes.begin(), selectionShow.visualScenes.end(), [&](const auto& visual)
             { return visual.start == 0. && visual.end() == selectionShow.duration(); }),
          "all four lanes hold for a steady 192-bar track");
    const auto fixture = report.getSiblingFile("groove-fixture.milk");
    juce::String fixtureKey;
    const bool linkedPreset = fixture.replaceWithText("[preset00]\nfDecay=0.96\nwave_a=0.7\n")
        && importMilkDropPreset(fixture, milkDropLibraryRoot(), fixtureKey).wasOk();
    check(linkedPreset, "install a disposable alternate MilkDrop preset");
    auto cueShow = selectionShow;
    cueShow.clips.front().sectionMarkers = std::vector<SourceSectionMarker>{{juce::Uuid().toString(), 48., 0., true, "Section"}};
    cueShow.clips.front().hotcues.emplace();
    cueShow.clips.front().hotcues->at(0) = HotcueMarker{juce::Uuid().toString(), 113.25, true};
    cueShow.clips.front().hotcues->at(1) = HotcueMarker{juce::Uuid().toString(), 128., false};
    VisualSceneClip heldShader;
    heldShader.scene = "studio/aurora";
    cueShow.visualOverrides = {{overrideKey(heldShader, "milkMix"), .17}};
    for (auto& window : cueTrack->energy.windows)
    {
        if (window.start == 32. || window.start >= 80.) window.bassOnsetsPerSecond = 1.5;
        if (window.start >= 160.) window.onsetsPerSecond = 5.;
    }
    const auto cueResult = arrangeProject(cueShow, VisualSceneSource::both, &selectionSeed);
    check(cueResult.wasOk() && startsFor(cueShow, VisualLayer::shader) == std::vector<double>{0., 48.}
          && startsFor(cueShow, VisualLayer::milk) == std::vector<double>{0., 80., 160.}
          && startsFor(cueShow, VisualLayer::sprite) == std::vector<double>{0., 113.25}
          && startsFor(cueShow, VisualLayer::background) == std::vector<double>{0., 160.},
          "section, sustained groove, marked hit and sustained energy trigger their respective lanes");
    check(std::count_if(cueShow.visualScenes.begin(), cueShow.visualScenes.end(), [&](const auto& visual)
          {
              const auto values = overridesFor(visual, cueShow.visualOverrides);
              return visualLayer(visual.scene) == VisualLayer::shader && values.count("milkMix") && values.at("milkMix") == .17;
          }) == 2, "a held control overrides both GLSL scenes after a generated cue");
    auto bassShow = selectionShow;
    bassShow.clips.front().sectionMarkers = std::vector<SourceSectionMarker>{
        {juce::Uuid().toString(), 64., 8., false, {}}, {juce::Uuid().toString(), 216., 8., false, {}}};
    for (auto& window : cueTrack->energy.windows)
    {
        if (window.start == 200. || window.start == 208.) { window.onsetsPerSecond = 2.; window.bassOnsetsPerSecond = .5; }
        if (window.start >= 216.) { window.bassRatio = .81; window.bassOnsetsPerSecond = 3.; }
    }
    check(arrangeProject(bassShow, VisualSceneSource::both, &selectionSeed).wasOk()
          && startsFor(bassShow, VisualLayer::shader) == std::vector<double>{0., 64., 216.}
          && startsFor(bassShow, VisualLayer::sprite) == std::vector<double>{0., 216.},
          "a strong section rise changes the sprite only when bass activity also returns");
    const VisualSceneEntry fixtureShader {"studio/lantern-arcade", "Long Arcade Sweep"};
    const bool added = lane.placeStudio(stockBackground.key, stockBackground.title, 0.)
        && lane.placeStudio(stockMilk.key, stockMilk.title, 0.)
        && lane.placeStudio(stockSprite.key, stockSprite.title, 0.)
        && lane.placeStudio(fixtureShader.key, fixtureShader.title, 0.);
    check(added && desk.engine.project.visualScenes.size() == 4, "four visual layers share one time range");
    if (!added) return;
    studio.update();
    check(specs(runtime(), fixtureShader.key).count("motion") != 0, "Studio receives selected GLSL control specifications");
    if (specs(runtime(), fixtureShader.key).count("motion") != 0)
    {
        studio.edit(lane.selection, "motion", 1., 1);
        const auto* clip = lane.selectedVisual();
        check(clip && clip->automation.count("motion") && clip->automation.at("motion").cycle,
              "Auto enables the selected continuous control");
        if (clip && clip->automation.count("motion"))
        {
            std::vector<VisualSceneClip> restored;
            const auto decoded = readVisualScenes(juce::JSON::parse(juce::JSON::toString(visualScenesToJson(desk.engine.project.visualScenes))), restored);
            check(decoded.wasOk() && restored.size() == 4 && restored.back().automation.count("motion") && restored.back().automation.at("motion").cycle,
                  "all four layers and the cycle persist through session JSON");
            check(visualControlsAt(*clip, 0, 0).at("motion") == 0 && visualControlsAt(*clip, 1, 128).at("motion") == 1
                  && visualControlsAt(*clip, 2, 192).at("motion") == .5 && visualControlsAt(*clip, 3, 256).at("motion") == 0,
                  "the default cycle completes in 64 musical bars");
        }
        studio.edit(lane.selection, "motion", 0., 1);
        studio.edit(lane.selection, "motion", .2, 2);
        const auto* clipWithEnds = lane.selectedVisual();
        if (clipWithEnds) studio.editPoint(clipWithEnds->start + clipWithEnds->length * .5, .8, false);
        const auto* keyed = lane.selectedVisual();
        check(keyed && keyed->automation.count("motion") && keyed->automation.at("motion").points.size() == 3
              && std::abs(visualControlsAt(*keyed, keyed->length * .5).at("motion") - .8) < 1.e-7,
              "manual visual keyframes drive values inside the selected clip");
        auto settings = juce::var(new juce::DynamicObject());
        settings.getDynamicObject()->setProperty("cycleMin", .2); settings.getDynamicObject()->setProperty("cycleMax", .6);
        settings.getDynamicObject()->setProperty("cycleBars", 8.); settings.getDynamicObject()->setProperty("audioSource", "bass");
        studio.edit(lane.selection, "motion", 1., 1, {}, settings);
        const auto* cycling = lane.selectedVisual();
        check(cycling && std::abs(visualControlsAt(*cycling, 2., 16.).at("motion") - .6) < 1.e-7
              && cycling->automation.at("motion").points.size() == 3 && cycling->automation.at("motion").audioSource == "bass",
              "custom Auto range and audio response preserve drawn keyframes");
        studio.edit(lane.selection, "motion", .41, 0);
        auto sibling = *lane.selectedVisual(), different = sibling;
        sibling.scene = "studio/lantern-arcade-courtyard-flood"; different.scene = "studio/cable-exchange-crossbar-handoff";
        studio.edit(lane.selection, "imprint", .75, 0);
        studio.edit(lane.selection, "response", .72, 0);
        const auto restored = parseProject(juce::JSON::toString(projectToJson(desk.engine.project)));
        const auto shared = overridesFor(different, restored.project.visualOverrides);
        check(restored.error.isEmpty() && shared.count("motion") && shared.at("motion") == .41 && !shared.count("imprint"),
              "reopened mix holds shared Motion across GLSL families and scopes Imprint to its family");
        const auto family = overridesFor(sibling, restored.project.visualOverrides);
        check(family.count("response") && family.at("response") == .72 && !family.count("imprint"),
              "siblings share controls with matching meaning and keep different imprint treatments separate");
        studio.resetControls(*lane.selectedVisual(), juce::var(juce::Array<juce::var>{"motion", "imprint", "response"}));
        check(desk.engine.project.visualOverrides.empty() && lane.selectedVisual()->automation.at("motion").cycle,
              "Reset releases a manual override back to its current automation");
    }
    studio.studioRoot = report.getSiblingFile("studio-link");
    studio.studioRoot.getChildFile("requests").createDirectory();
    studio.exchange();
    const auto state = json(studio.studioRoot.getChildFile("state.json"));
    check((bool)state["hasVisuals"] && lane.selectedVisual() && state["clip"].toString() == lane.selection && state["scene"].toString() == lane.selectedVisual()->scene
          && state["parameters"]["milkMix"].isObject(),
          "full Studio receives timeline ownership, clip identity, scene and original / MilkDrop control");
    const auto send = [&](const juce::String& clipId, double value, const juce::String& name)
    {
        auto* request = new juce::DynamicObject();
        request->setProperty("clip", clipId); request->setProperty("parameter", name);
        request->setProperty("value", value); request->setProperty("action", 0);
        studio.studioRoot.getChildFile("requests/edit.json").replaceWithText(juce::JSON::toString(juce::var(request)));
        studio.exchange();
    };
    const auto heldValues = [&] { return overridesFor(*lane.selectedVisual(), desk.engine.project.visualOverrides); };
    send("stale-clip", .17, "milkMix");
    check(heldValues().count("milkMix") == 0, "stale Studio edits cannot modify another clip");
    send(lane.selection, .17, "milkMix");
    check(heldValues().count("milkMix") && heldValues().at("milkMix") == .17, "the controller holds its manual value over the timeline");
    send(lane.selection, 2., "milkMix");
    check(heldValues().count("milkMix") && heldValues().at("milkMix") == .17, "out-of-range Studio values are rejected");
    desk.undo(false);
    check(lane.selectedVisual() && heldValues().count("milkMix") == 0, "controller overrides participate in Alloy Undo");
    if (const auto* selected = lane.selectedVisual())
    {
        auto* request = new juce::DynamicObject();
        request->setProperty("clip", selected->id); request->setProperty("scene", selected->scene);
        request->setProperty("parameter", "milkMix"); request->setProperty("gesture", "full-studio-batch");
        const auto batch = visualControlsToJson({{"milkMix", .7}, {"brightness", .25}});
        request->setProperty("values", batch);
        studio.studioRoot.getChildFile("requests/batch.json").replaceWithText(juce::JSON::toString(juce::var(request)));
        studio.exchange();
        const auto changed = heldValues();
        check(changed.count("milkMix") && changed.at("milkMix") == .7 && changed.count("brightness") && changed.at("brightness") == .25,
              "a full Studio control group holds as a single edit");
        desk.undo(false);
        check(lane.selectedVisual() && heldValues().count("milkMix") == 0,
              "one Alloy Undo restores the full Studio control group");
    }
    const auto selectedId = lane.selection;
    lane.selection.clear();
    desk.selectedClip = "audio-only-selection";
    const auto focus = [&](const juce::String& id)
    {
        auto* request = new juce::DynamicObject();
        request->setProperty("command", "focus"); request->setProperty("clip", id);
        studio.studioRoot.getChildFile("requests/focus.json").replaceWithText(juce::JSON::toString(juce::var(request)));
        studio.lastExchange = 0;
        studio.exchange();
        return json(studio.studioRoot.getChildFile("state.json"));
    };
    const auto following = focus({});
    check(following["clip"].toString() == selectedId && following["targets"].size() == 4 && (bool)following["atPlayhead"],
          "Studio follows the active GLSL clip and lists all four layers when Alloy has no visual selection");
    const auto backgroundId = following["targets"][0]["clip"].toString();
    const auto pinned = focus(backgroundId);
    check(pinned["clip"].toString() == backgroundId && pinned["parameters"]["backgroundMix"].isObject()
          && desk.selectedClip == "audio-only-selection",
          "Studio can pin the background controls while Alloy retains its audio selection");
    send(selectedId, .44, "milkMix");
    check(studio.clipFor(selectedId) && desk.engine.project.visualOverrides.empty(),
          "an edit from the previous Studio focus cannot affect the newly focused clip");
    check(focus({})["clip"].toString() == selectedId && studio.focusedClip(1000.) != nullptr,
          "Follow playhead restores the active clip and keeps a control target after the last visual");
    const auto studioRequest = [&](juce::DynamicObject::Ptr request)
    {
        studio.studioRoot.getChildFile("requests/deck.json").replaceWithText(juce::JSON::toString(juce::var(request.get())));
        studio.lastExchange = 0;
        studio.exchange();
        return json(studio.studioRoot.getChildFile("state.json"));
    };
    for (int layer = 0; layer < 4; ++layer)
    {
        juce::DynamicObject::Ptr request = new juce::DynamicObject();
        request->setProperty("command", "focus-lane");
        request->setProperty("layer", layer);
        const auto selected = studioRequest(request);
        check((int)selected["focusedLayer"] == layer && (int)selected["layer"] == layer
              && selected["pinned"].toString().isEmpty() && selected["parameters"].isObject(),
              "controller selects visual lane " + juce::String(layer + 1));
    }
    const auto route = [&](const juce::String& target)
    {
        const auto sprite = following["spriteRouting"];
        juce::DynamicObject::Ptr request = new juce::DynamicObject();
        request->setProperty("command", "sprite-route");
        request->setProperty("clip", sprite["clip"]); request->setProperty("scene", sprite["scene"]);
        request->setProperty("route", target);
        return studioRequest(request);
    };
    const auto milkRoute = route("milk"), shaderRoute = route("shader"), overlayRoute = route("overlay");
    check(milkRoute["spriteRouting"]["route"].toString() == "milk"
          && shaderRoute["spriteRouting"]["route"].toString() == "shader"
          && overlayRoute["spriteRouting"]["route"].toString() == "overlay",
          "sprite routing applies exclusive destinations through the controller mailbox");
    desk.undo(false);
    check(focus({})["spriteRouting"]["route"].toString() == "shader", "Undo restores the previous sprite destination in one step");
    desk.undo(false); desk.undo(false);
    const auto savedPosition = desk.engine.position.load();
    const auto savedFollow = desk.layout.followPlayhead;
    studio.setBounds(0, 0, 640, 230);
    desk.setFollowPlayhead(false);
    desk.engine.position = 50.;
    desk.followPlayheadIntoView();
    check(lane.studioStart == 0., "visual timeline keeps its scroll with Follow off");
    desk.setFollowPlayhead(true);
    check(lane.studioStart > 0. && lane.studioX(50.) < lane.getWidth(),
          "enabling audio Follow brings the visual playhead into view");
    desk.engine.position = savedPosition;
    desk.followPlayheadIntoView();
    check(lane.studioStart == 0., "visual Follow also brings a backwards seek into view");
    desk.setFollowPlayhead(savedFollow);
    desk.selectedClip.clear();
    lane.selection = selectedId;
    desk.detachToolPanel(3, true);
    check(desk.studioWindow && desk.studioPanel->getParentComponent() != &desk, "Studio detaches into an independent panel");
    check(studio.openStudio(), "open full Studio to request its timeline renderer");
    const auto deadline = juce::Time::getMillisecondCounter() + 14000;
    while ((!lane.preview.isValid() || studio.studioConnecting) && juce::Time::getMillisecondCounter() < deadline)
    {
        lane.timerCallback();
        juce::Thread::sleep(30);
    }
    check(lane.preview.isValid() && (bool)lane.previewFacts["studio"] && lane.previewFacts["scenes"].size() == 4,
          "the integrated GPU renderer displays all four actors: " + lane.previewStatus);
    const auto hybrid = lane.preview;
    if (hybrid.isValid())
    {
        studio.edit(lane.selection, "milkMix", 0., 0);
        const auto until = juce::Time::getMillisecondCounter() + 3000;
        while (juce::Time::getMillisecondCounter() < until && lane.preview == hybrid)
        {
            lane.timerCallback();
            juce::Thread::sleep(30);
        }
        int changed = 0;
        if (lane.preview.isValid())
            for (int y = 0; y < hybrid.getHeight(); y += 4)
                for (int x = 0; x < hybrid.getWidth(); x += 4)
                    if (hybrid.getPixelAt(x, y) != lane.preview.getPixelAt(x, y)) ++changed;
        check(changed > 300, "the Original / MilkDrop changes the pixels of the integrated GLSL scene");
        studio.edit(lane.selection, "milkMix", .65, 0);
    }
    const auto save = [&](const juce::Image& image, const juce::String& name)
    {
        if (!image.isValid()) return false;
        juce::FileOutputStream file(report.getSiblingFile(name));
        return file.openedOk() && juce::PNGImageFormat().writeImageToStream(image, file);
    };
    check(save(desk.studioPanel->createComponentSnapshot(desk.studioPanel->getLocalBounds()), "studio-panel.png")
          && save(hybrid, "studio-hybrid.png"), "capture the detached panel and its rendered frame");
    check(studio.outputShowing() && !studio.studioConnecting, "full Studio stayed open and acknowledged the connection");
    desk.detachToolPanel(3, false);
    check(desk.studioPanel->getParentComponent() == &desk, "Studio reattaches to the original tab");
    check(save(desk.createComponentSnapshot(desk.getLocalBounds()), "alloy-version.png"),
          "capture the docked workspace and visible Alloy version");
    {
        const auto snapshot = desk.engine.project;
        const auto position = desk.engine.position.load();
        desk.engine.project = cueShow;
        desk.engine.position = 114.;
        desk.refresh();
        lane.studioStart = 0.; lane.studioScale = 3.;
        check(save(desk.studioPanel->createComponentSnapshot(desk.studioPanel->getLocalBounds()), "studio-cues.png"),
              "capture the four cue-driven Studio lanes");
        desk.engine.project = snapshot;
        desk.engine.position = position;
        desk.refresh();
    }

    if (arranged.project.transitionLinks.size() == 1)
    {
        desk.engine.project = arranged.project;
        const auto initialAudio = juce::JSON::toString(projectToJson(desk.engine.project)["transitionLinks"]);
        const auto visual = std::find_if(desk.engine.project.visualScenes.begin(), desk.engine.project.visualScenes.end(),
            [](const auto& clip) { return visualLayer(clip.scene) == VisualLayer::shader && clip.transition != "cut"; });
        if (visual != desk.engine.project.visualScenes.end())
        {
            const auto incoming = *visual;
            const auto outgoing = *lane.previousStudioClip(incoming);
            const auto time = (incoming.start + outgoing.end()) * .5;
            desk.engine.position = time;
            const auto until = juce::Time::getMillisecondCounter() + 8000;
            do { lane.timerCallback(); juce::Thread::sleep(30); }
            while (!lane.previewFacts["transitions"]["shader"].isObject() && juce::Time::getMillisecondCounter() < until);
            check(lane.previewFacts["transitions"]["shader"].isObject() && save(lane.preview, "studio-join.png"),
                  "connected renderer shows both GLSL actors at the musical handover: " + lane.previewStatus);
            lane.studioScale = 20.; lane.studioStart = std::max(0., incoming.start - 3.);
            const juce::Point<float> handle((float)lane.studioX(incoming.start), lane.bounds(incoming).getBottom() - 6.f);
            const auto moved = handle.translated(5.f, 0.f);
            const auto now = juce::Time::getCurrentTime();
            const auto event = [&](juce::Point<float> point, bool dragged)
            {
                return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), point,
                    juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::altModifier),
                    1, 0, 0, 0, 0, &lane, &lane, now, handle, now, 1, dragged);
            };
            lane.mouseDown(event(handle, false)); lane.mouseDrag(event(moved, true)); lane.mouseUp(event(moved, true));
            check(std::abs(studio.clipFor(incoming.id)->start - incoming.start - .25) < 1.e-6
                  && juce::JSON::toString(projectToJson(desk.engine.project)["transitionLinks"]) == initialAudio,
                  "dragging the visual transition start leaves the audio join unchanged");
            check(lane.editStudioTransition(outgoing.id, incoming.id, "cut", incoming.start + .25, outgoing.end(), true)
                  && studio.clipFor(incoming.id)->transition == "cut", "visual preset can be edited in the same window");
        }
    }
    {
        const auto snapshot = desk.engine.project;
        const auto catalogue = lane.scenes;
        const juce::String alternateMilk = "milkdrop/" + juce::String::repeatedString("a", 64);
        lane.scenes = {{alternateMilk, "Test MilkDrop alternate"}};
        desk.engine.position = 1.;
        std::vector<VisualSceneClip> clips;
        for (const auto& entry : {stockBackground, stockMilk, fixtureShader, stockSprite})
        {
            VisualSceneClip clip; clip.scene = entry.key; clip.title = entry.title; clip.length = 8.;
            clips.push_back(clip);
        }
        desk.engine.project.visualScenes = clips;
        desk.engine.project.visualOverrides.clear();
        studio.edit(clips[2].id, "milkMix", .6, 0);
        for (const auto layer : {VisualLayer::background, VisualLayer::milk, VisualLayer::shader, VisualLayer::sprite})
        {
            studio.swapRandom(layer);
            const auto& beforeSwap = clips[(size_t)layer];
            const auto* afterSwap = studio.clipFor(beforeSwap.id);
            check(afterSwap && afterSwap->scene != beforeSwap.scene && visualLayer(afterSwap->scene) == layer
                  && afterSwap->start == beforeSwap.start && afterSwap->end() == beforeSwap.end(),
                  "random swap replaces the active item in lane " + juce::String((int)layer + 1));
        }
        const auto* swapped = studio.clipFor(clips[2].id);
        check(swapped && overridesFor(*swapped, desk.engine.project.visualOverrides).count("milkMix"),
              "random scene replacement keeps a compatible manual override");
        lane.scenes.clear();
        studio.swapRandom(VisualLayer::milk); // Restore the bundled preset, then exhaust the pool.
        studio.swapRandom(VisualLayer::milk);
        check(studio.swapStatus == "No other item is available in this lane.", "single-item MilkDrop reports when no alternate is available");
        lane.scenes = catalogue;
        desk.engine.project = snapshot;
    }

    VisualSceneClip active, future;
    active.scene = "studio/aurora"; active.title = "Signal Loom"; active.length = 8.; active.offset = 1.;
    active.controls["milkMix"] = .4; active.automation["milkMix"] = {false, {{0., .4}, {1., .6}}};
    future.scene = stockMilk.key; future.title = stockMilk.title; future.start = 7.; future.length = 8.;
    future.transition = "feedback"; future.pivot = .35;
    desk.engine.project.visualScenes = {active, future};
    const auto before = juce::JSON::toString(visualScenesToJson(desk.engine.project.visualScenes));
    check(studio.applyVisualChoice("studio/tunnel", "Vector Tunnel", 0., future.id)
          && studio.applyVisualChoice(stockMilk.key, stockMilk.title, 0., active.id),
          "Studio choices replace a future MilkDrop with GLSL and an active GLSL with MilkDrop");
    const auto* replacedActive = studio.clipFor(active.id);
    const auto* replacedFuture = studio.clipFor(future.id);
    check(replacedActive && replacedFuture && replacedActive->start == active.start && replacedActive->length == active.length
          && replacedActive->offset == 0. && replacedActive->automation.empty() && replacedActive->controls.empty()
          && replacedFuture->start == future.start && replacedFuture->length == future.length
          && replacedFuture->transition == future.transition && replacedFuture->pivot == future.pivot,
          "replacement keeps the selected clip's geometry and join metadata while resetting its scene settings");
    desk.undo(false); desk.undo(false);
    check(juce::JSON::toString(visualScenesToJson(desk.engine.project.visualScenes)) == before,
          "Undo restores both visual identities and the original animation");

    Project show;
    for (int i = 0; i < 44; ++i)
    {
        Clip clip; clip.media = first; clip.start = i * 8.; clip.length = 8.;
        show.clips.push_back(std::move(clip));
    }
    juce::Random showSeed(1044);
    const auto planned = arrangeProject(show, VisualSceneSource::both, &showSeed);
    const auto schedule = juce::JSON::toString(visualScenesToJson(show.visualScenes));
    const auto reopened = readVisualScenes(juce::JSON::parse(schedule), show.visualScenes);
    std::vector<const VisualSceneClip*> glsl;
    std::set<juce::String> showScenes;
    double covered = 0.;
    bool continuous = true;
    for (const auto& clip : show.visualScenes)
        if (visualLayer(clip.scene) == VisualLayer::shader)
        {
            continuous &= clip.start == covered;
            covered = clip.end();
            glsl.push_back(&clip); showScenes.insert(clip.scene);
        }
    check(planned.wasOk() && reopened.wasOk() && schedule == juce::JSON::toString(visualScenesToJson(show.visualScenes))
          && continuous && covered == show.duration() && glsl.size() == 44 && showScenes.size() == 44,
          "a reopened 44-track show uses distinct GLSL scenes and covers every track through the end");
    for (const auto layer : {VisualLayer::background,VisualLayer::sprite})
    {
        const bool sprite = layer==VisualLayer::sprite;
        const auto assets=artworkLibrary(runtime(),sprite);
        std::set<juce::String> used;
        juce::String previous;
        const auto folder=runtime().getChildFile(sprite ? "Sprites" : "Backgrounds");
        double edge=0;
        bool valid=true;
        for (const auto& clip : show.visualScenes) if (visualLayer(clip.scene)==layer)
        {
            const auto control=clip.controls.find(sprite ? "spriteInMilkDrop" : "backgroundNested");
            const auto identity = sprite ? clip.scene : juce::SHA256(folder.getChildFile(clip.scene.fromFirstOccurrenceOf("/",false,false))).toHexString();
            valid &= std::abs(clip.start-edge)<1.e-7 && (sprite || previous!=identity)
                && control!=clip.controls.end() && control->second==1
                && std::any_of(assets.begin(),assets.end(),[&](const auto& item) { return item.key==clip.scene; });
            edge=clip.end(); previous=identity; used.insert(clip.scene);
        }
        check(valid && used.size()==1 && std::abs(edge-show.duration())<1.e-7,
            sprite ? "one nested MilkDrop sprite holds across all track handovers" : "one nested GLSL background holds across all track handovers");
    }
    if (glsl.size() == 44)
    {
        desk.engine.project = show;
        for (const int ordinal : {0, 21, 43})
        {
            const auto& expected = *glsl[(size_t)ordinal];
            desk.engine.position = expected.start + 1.;
            const auto until = juce::Time::getMillisecondCounter() + 5000;
            do
            {
                lane.timerCallback();
                juce::Thread::sleep(30);
            } while (lane.previewFacts["shaderClip"].toString() != expected.id && juce::Time::getMillisecondCounter() < until);
            check(lane.previewFacts["shaderClip"].toString() == expected.id
                  && (bool)lane.previewFacts["spriteInMilkDrop"] && (bool)lane.previewFacts["backgroundInShader"]
                  && save(lane.preview, "show-track-" + juce::String(ordinal + 1) + ".png"),
                  "the reopened show renders track " + juce::String(ordinal + 1) + ": " + lane.previewStatus);
        }
    }
    {
        Project selection;
        auto firstBlock=active, secondBlock=future;
        firstBlock.start=3.; secondBlock.start=7.;
        selection.visualScenes={firstBlock,secondBlock};
        desk.engine.project=selection; desk.refresh();
        lane.studioStart=0; lane.studioScale=12;
        const auto mouse=[&](juce::Point<float> point,juce::Point<float> origin,bool moved)
        {
            const auto now=juce::Time::getCurrentTime();
            return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),point,
                juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::altModifier),
                1,0,0,0,0,&lane,&lane,now,origin,now,1,moved);
        };
        const juce::Point<float> origin{115.f,lane.bounds(secondBlock).getY()+1.f};
        const juce::Point<float> endBox{245.f,lane.bounds(firstBlock).getBottom()-1.f};
        lane.mouseDown(mouse(origin,origin,false)); lane.mouseDrag(mouse(endBox,origin,true)); lane.mouseUp(mouse(endBox,origin,true));
        check(lane.selectedClips.size()==2,"marquee selects clips on two visual lanes");
        const auto centre=lane.bounds(firstBlock).getCentre();
        const auto target=centre.translated(24.f,0.f);
        lane.mouseDown(mouse(centre,centre,false)); lane.mouseDrag(mouse(target,centre,true)); lane.mouseUp(mouse(target,centre,true));
        const auto& moved=desk.engine.project.visualScenes;
        check(moved.size()==2 && moved[0].start==5. && moved[1].start==9.,"drag moves the selected visual clips together");
        lane.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey));
        check(desk.engine.project.visualScenes.empty(),"Delete removes every selected visual clip");
        desk.undo(false); desk.undo(false);
        check(juce::JSON::toString(visualScenesToJson(desk.engine.project.visualScenes))
            ==juce::JSON::toString(visualScenesToJson(selection.visualScenes)),"one Undo per group change restores both visual clips exactly");
    }
    studio.studioRoot.getChildFile("closed").replaceWithText("closed");
    check(studio.studioProcess.waitForProcessToFinish(5000), "owned Studio closes normally after the timeline preview");
}
}
