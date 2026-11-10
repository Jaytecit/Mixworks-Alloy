#include "StudioWorkspace.h"
#include "Workstation.h"
#include "MilkDrop.h"
#include "EnergySections.h"
#include "AutoArrangement.h"
#include <cmath>

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
const VisualSceneEntry stockFlare {"studio-sprite/flare", "Copper Flare"};

}

StudioWorkspace::StudioWorkspace(SceneLane& l) : lane(l), owner(l.owner)
{
    lane.setStudioWorkspace(this);
    for (juce::Component* item : std::array<juce::Component*, 9>{
         &backgrounds, &presets, &shaders, &sprites, &automatic, &output, &zoom, &scroll, &lane})
        addAndMakeVisible(*item);
    backgrounds.onClick = [this] { pick(VisualLayer::background, owner.engine.position.load()); };
    presets.onClick = [this] { pick(VisualLayer::milk, owner.engine.position.load()); };
    shaders.onClick = [this] { pick(VisualLayer::shader, owner.engine.position.load()); };
    sprites.onClick = [this] { pick(VisualLayer::sprite, owner.engine.position.load()); };
    automatic.onClick = [this] { arrange(); };
    output.onClick = [this] { openStudio(); };
    zoom.setRange(1, 60, .1); zoom.setValue(lane.studioScale); zoom.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    zoom.setSkewFactorFromMidPoint(10); zoom.setTooltip("Visual timeline zoom");
    zoom.onValueChange = [this] { lane.studioScale = zoom.getValue(); lane.repaint(); };
    scroll.setRange(0, 3600, .1); scroll.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    scroll.setTooltip("Scroll visual timeline independently of the music tracks");
    scroll.onValueChange = [this] { lane.studioStart = scroll.getValue(); lane.repaint(); };
}
void StudioWorkspace::resized()
{
    auto area = getLocalBounds().reduced(6, 4);
    auto toolbar = area.removeFromTop(28);
    for (auto* button : {&backgrounds, &presets, &shaders, &sprites, &automatic, &output})
    {
        const auto width = std::max(56, std::min(button == &automatic || button == &output ? 150 : 105, getWidth() / 7));
        button->setBounds(toolbar.removeFromLeft(width).reduced(2));
    }
    auto tools = area.removeFromBottom(22);
    zoom.setBounds(tools.removeFromRight(112)); scroll.setBounds(tools);
    previewArea = area.removeFromRight(juce::jlimit(180, 480, area.getWidth() * 2 / 5)).withTrimmedLeft(6);
    lane.setBounds(area);
}
void StudioWorkspace::paint(juce::Graphics& g)
{
    g.fillAll(colour::panel);
    g.setColour(juce::Colours::black); g.fillRect(previewArea);
    if (lane.preview.isValid())
    {
        g.drawImageWithin(lane.preview, previewArea.getX(), previewArea.getY(), previewArea.getWidth(), previewArea.getHeight(), juce::RectanglePlacement::centred);
        lane.recordPreviewPaint();
    }
    else
    {
        g.setColour(colour::muted); g.setFont(12.f);
        g.drawFittedText(lane.previewStatus.isEmpty() ? "Add visual clips to preview" : lane.previewStatus, previewArea.reduced(8), juce::Justification::centred, 2);
    }
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
    lastExchange = 0; sharedPreview = {}; exchange();
    const bool started = studioProcess.start(juce::StringArray {python.getFullPathName(), "-B", script.getFullPathName(),
        "--alloy-session", studioRoot.getFullPathName()}, 0);
    owner.setStatus(started ? "Visual Studio opened. Select a visual clip to edit its controls."
                            : "Visual Studio could not start. The timeline was kept.");
    return started;
}
void StudioWorkspace::update()
{
    double end = std::max(60., owner.engine.project.duration());
    for (const auto& c : owner.engine.project.visualScenes) end = std::max(end, c.end());
    if (scroll.getMaximum() != end) scroll.setRange(0, end, .1);
    if (studioProcess.isRunning()) exchange();
    repaint(previewArea);
}
void StudioWorkspace::exchange()
{
    // One isolated mailbox per Alloy instance. Commands are consumed once,
    // bound to a clip ID and a validated parameter, never retried on timeout.
    const auto requests = studioRoot.getChildFile("requests");
    auto files = requests.findChildFiles(juce::File::findFiles, false, "*.json");
    files.sort();
    for (const auto& file : files)
    {
        const auto request = json(file);
        file.deleteFile();
        if (!request.isObject()) continue;
        const auto id = request["clip"].toString(), name = request["parameter"].toString();
        const auto* clip = lane.selectedVisual();
        if (!clip || clip->id != id) continue;
        const auto command = request["command"].toString();
        if (command == "undo" || command == "redo")
        {
            if (!owner.busy && !owner.engine.exporting && !owner.engine.capturingPerformance()) owner.undo(command == "redo");
            lastGesture.clear(); continue;
        }
        auto controls = specs(runtime(), clip->scene);
        if (lane.previewFacts["shaderClip"].toString() == id)
            if (const auto* detected = lane.previewFacts["shaderControls"].getDynamicObject())
                for (const auto& property : detected->getProperties())
                    controls[property.name.toString()] = {(double)property.value[0], (int)property.value[1], property.value[2].toString()};
        const auto value = (double)request["value"];
        const auto action = (int)request["action"];
        if (controls.count(name) && std::isfinite(value) && value >= 0 && value <= 1 && action >= 0 && action <= 2)
        {
            const auto gesture = request["gesture"].toString();
            if (gesture != lastGesture || action != 0) lastGesture.clear();
            edit(id, name, value, action);
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
    state->setProperty("position", owner.engine.position.load());
    state->setProperty("bpm", owner.engine.project.gridClock().bpmAt(owner.engine.position.load()));
    state->setProperty("status", lane.previewStatus);
    if (const auto* clip = lane.selectedVisual())
    {
        state->setProperty("clip", clip->id); state->setProperty("title", clip->title);
        auto controls = specs(runtime(), clip->scene);
        if (lane.previewFacts["shaderClip"].toString() == clip->id)
            if (const auto* detected = lane.previewFacts["shaderControls"].getDynamicObject())
                for (const auto& property : detected->getProperties())
                    controls[property.name.toString()] = {(double)property.value[0], (int)property.value[1], property.value[2].toString()};
        const auto position = owner.engine.position.load(); const auto grid = owner.engine.project.gridClock();
        const auto values = visualControlsAt(*clip, position - clip->start + clip->offset, grid.beatsAt(position) - grid.beatsAt(clip->start));
        auto* parameters = new juce::DynamicObject();
        for (const auto& [name, spec] : controls)
        {
            auto* item = new juce::DynamicObject(); item->setProperty("label", spec.label);
            item->setProperty("value", values.count(name) ? values.at(name) : spec.initial);
            item->setProperty("steps", spec.steps);
            const auto curve = clip->automation.find(name);
            item->setProperty("cycle", curve != clip->automation.end() && curve->second.cycle);
            parameters->setProperty(name, juce::var(item));
        }
        state->setProperty("parameters", juce::var(parameters));
    }
    if (lane.preview.isValid() && lane.preview != sharedPreview)
    {
        juce::TemporaryFile temporary(studioRoot.getChildFile("preview.jpg"));
        { juce::FileOutputStream stream(temporary.getFile());
          if (stream.openedOk()) { juce::JPEGImageFormat format; format.setQuality(.92f); format.writeImageToStream(lane.preview, stream); } }
        if (temporary.overwriteTargetFileWithTemporary()) sharedPreview = lane.preview;
    }
    juce::TemporaryFile temporary(studioRoot.getChildFile("state.json"));
    temporary.getFile().replaceWithText(juce::JSON::toString(juce::var(state)));
    temporary.overwriteTargetFileWithTemporary();
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
        case VisualLayer::background: return {{"backgroundMix", {.38, 0, "Blend amount"}}};
        case VisualLayer::sprite: return mediaSpecs;
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
            else result[name] = {normal((double)value, (double)low, high.isVoid() ? 1. : (double)high), type == "toggle" ? 2 : 0, label};
        }
    return result;
}

void StudioWorkspace::edit(const juce::String& id, const juce::String& name, double value, int action)
{
    if (owner.busy || owner.engine.exporting || owner.engine.capturingPerformance()) return;
    const auto position = owner.engine.position.load();
    auto clips = owner.engine.project.visualScenes;
    auto selected = std::find_if(clips.begin(), clips.end(), [&](const auto& clip) { return clip.id == id; });
    if (selected == clips.end()) return;
    lane.studioParameter = name;
    if (action == 0) selected->controls[name] = value;
    else if (action == 1)
    {
        auto& curve = selected->automation[name];
        curve.cycle = value > .5;
        if (!curve.cycle && curve.points.empty())
        {
            const auto grid = owner.engine.project.gridClock();
            selected->controls[name] = visualCycleAt(grid.beatsAt(position) - grid.beatsAt(selected->start));
            selected->automation.erase(name);
        }
    }
    else
    {
        if (position < selected->start || position > selected->end())
        { owner.setStatus("Move the playhead inside the selected visual clip to add a point."); return; }
        auto& curve = selected->automation[name];
        const auto initial = value;
        if (curve.points.empty()) curve.points = {{0, initial}, {1, initial}};
        const auto fraction = juce::jlimit(0., 1., (position - selected->start) / selected->length);
        auto point = std::lower_bound(curve.points.begin(), curve.points.end(), fraction, [](const auto& p, double at) { return p.at < at; });
        if (point != curve.points.end() && std::abs(point->at - fraction) < .00001) point->value = value;
        else curve.points.insert(point, {fraction, value});
        curve.cycle = false;
    }
    if (validateVisualScenes(clips).failed()) return;
    if (action != 0 || lastGesture.isEmpty()) owner.remember();
    { const juce::ScopedLock guard(owner.engine.lock); owner.engine.project.visualScenes = std::move(clips); }
    owner.refresh(); lane.repaint();
}
void StudioWorkspace::editPoint(double time, double value, bool remove)
{
    auto clips = owner.engine.project.visualScenes;
    auto clip = std::find_if(clips.begin(), clips.end(), [&](const auto& c) { return c.id == lane.selection; });
    if (clip == clips.end() || lane.studioParameter.isEmpty() || owner.busy || owner.engine.exporting || owner.engine.capturingPerformance()) return;
    const auto found = clip->automation.find(lane.studioParameter);
    if (found == clip->automation.end() || found->second.cycle || found->second.points.empty()) return;
    const auto fraction = juce::jlimit(0., 1., (time - clip->start) / clip->length);
    auto& points = found->second.points;
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

void StudioWorkspace::addFile(VisualLayer layer, double time)
{
    const auto extensions = layer == VisualLayer::shader ? "*.glsl;*.frag;*.fs"
        : layer == VisualLayer::milk ? "*.milk" : "*.png;*.jpg;*.jpeg;*.webp;*.bmp;*.tiff";
    chooser = std::make_unique<juce::FileChooser>("Choose a local visual asset", juce::File(), extensions);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe = juce::Component::SafePointer<StudioWorkspace>(this), layer, time](const juce::FileChooser& picker)
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
            safe->lane.placeStudio(key, file.getFileNameWithoutExtension(), time);
        });
}

void StudioWorkspace::pick(VisualLayer layer, double time)
{
    if (owner.busy || owner.engine.exporting || owner.engine.capturingPerformance()) return;
    juce::PopupMenu menu;
    menu.addItem(1, "Open file...");
    std::vector<VisualSceneEntry> entries;
    if (layer == VisualLayer::shader) entries = library(runtime());
    else if (layer == VisualLayer::milk)
    {
        entries = discoverMilkDropScenes(milkDropLibraryRoot());
        entries.insert(entries.begin(), stockMilk);
    }
    else if (layer == VisualLayer::background) entries.push_back(stockBackground);
    else entries = {stockSprite, stockFlare};
    std::map<juce::String, juce::PopupMenu> groups;
    for (size_t i = 0; i < entries.size(); ++i)
    {
        const auto& entry = entries[i];
        auto& group = entry.family.isNotEmpty() ? groups[entry.family] : menu;
        group.addItem((int)i + 2, entry.title);
    }
    for (auto& [name, group] : groups) menu.addSubMenu(name, group);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&lane),
        [safe = juce::Component::SafePointer<StudioWorkspace>(this), layer, time, entries = std::move(entries)](int choice)
        {
            if (!safe || choice == 0) return;
            if (choice == 1) safe->addFile(layer, time);
            else if (juce::isPositiveAndBelow(choice - 2, (int)entries.size()))
            {
                const auto& entry = entries[(size_t)choice - 2];
                safe->lane.placeStudio(entry.key, entry.title, time);
            }
        });
}

juce::Result StudioWorkspace::arrangeProject(Project& project, VisualSceneSource sources)
{
    return arrangeProject(project, sources, nullptr);
}
juce::Result StudioWorkspace::arrangeProject(Project& project, VisualSceneSource sources, juce::Random* choices)
{
    const auto available = library(runtime());
    if (available.empty()) return juce::Result::fail("The Studio scene library could not be loaded.");
    if (project.clips.empty()) return juce::Result::fail("Add music to the mix before auto arranging visuals.");
    const auto grid = project.gridClock();
    const auto end = project.duration();
    std::vector<double> cuts {0.};
    for (const auto& clip : project.clips)
    {
        const auto* link = project.transitionFor(clip.id);
        auto time = project.transitionPlaybackFor(clip).startCut.value_or(clip.start);
        if (link) time = alignedSectionCut(project, *link);
        else time = grid.snap(time, true, 4.);
        if (time > cuts.back() + .05 && time < end - .05) cuts.push_back(time);
    }
    if (end < .05) return juce::Result::fail("The mix is too short to arrange visuals.");
    cuts.push_back(end);
    std::vector<double> sectionCuts;
    for (const auto& clip : project.clips)
        if (clip.media && !clip.reversed && clip.loopSpan == 0)
            for (const auto& section : clipEnergySections(clip))
            {
                const auto time = grid.snap(clip.start + clip.audioTimelineTime(section.sourceSeconds - clip.offset), true, 4.);
                if (time <= clip.start || time >= clip.end() || time >= end) continue;
                const auto next = std::upper_bound(cuts.begin(), cuts.end(), time);
                if (next != cuts.end() && next != cuts.begin()
                    && grid.beatsAt(time) - grid.beatsAt(*(next - 1)) >= 64.
                    && grid.beatsAt(*next) - grid.beatsAt(time) >= 64.)
                    sectionCuts.push_back(time);
            }
    std::sort(sectionCuts.begin(), sectionCuts.end());
    for (const double time : sectionCuts)
    {
        const auto next = std::upper_bound(cuts.begin(), cuts.end(), time);
        if (grid.beatsAt(time) - grid.beatsAt(*(next - 1)) >= 64.
            && grid.beatsAt(*next) - grid.beatsAt(time) >= 64.)
            cuts.insert(next, time);
    }
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
    const auto spritePhase = random.nextInt(2);
    std::vector<VisualSceneClip> clips;
    const double lastBeat = grid.beatsAt(end);
    // Each layer has a distinct musical cadence. Shared show endpoints do not
    // force the interiors into repeated four-layer scene changes.
    const auto schedule = [&](double stride, double phase)
    {
        std::vector<double> edges {0.};
        for (double beat = phase; beat < lastBeat; beat += stride)
        {
            const auto time = grid.timeAtBeat(beat);
            if (time > edges.back() + .05 && time < end - .05) edges.push_back(time);
        }
        edges.push_back(end); return edges;
    };
    // Keep long passages varied even when source section markers are sparse.
    for (const auto time : schedule(128., 128.))
        if (time > .05 && time < end - .05
            && std::all_of(cuts.begin(), cuts.end(), [&](double cut) { return std::abs(grid.beatsAt(cut) - grid.beatsAt(time)) >= 16.; }))
            cuts.insert(std::upper_bound(cuts.begin(), cuts.end(), time), time);
    const auto backgroundCuts = schedule(192., 192.);
    const auto milkCuts = schedule(64., 24.);
    const auto add = [&](const VisualSceneEntry& entry, double start, double finish) -> VisualSceneClip&
    {
        VisualSceneClip clip;
        clip.scene = entry.key; clip.title = entry.title; clip.start = start;
        clip.length = finish - start; clip.transition = "cut";
        clips.push_back(std::move(clip)); return clips.back();
    };
    for (size_t i = 0; i + 1 < backgroundCuts.size(); ++i)
        add(stockBackground, backgroundCuts[i], backgroundCuts[i+1]).controls["backgroundMix"] = .22 + .12 * (i % 3);
    if (sources != VisualSceneSource::originals)
        for (size_t i = 0; i + 1 < milkCuts.size(); ++i)
            add(milk[i % milk.size()], milkCuts[i], milkCuts[i+1]);
    if (sources != VisualSceneSource::milkdrop)
        for (size_t i = 0; i + 1 < cuts.size(); ++i)
            add(shaders[i % shaders.size()], cuts[i], cuts[i+1]).controls["milkMix"] = .2 + .25 * random.nextDouble();
    // Eight-bar artwork accents, with eight bars of breathing room; a single
    // long scene can change character without replacing its other three layers.
    size_t accent = 0;
    for (double beat = 8.; beat < lastBeat; beat += 64., ++accent)
    {
        const auto start = grid.timeAtBeat(beat), finish = std::min(end, grid.timeAtBeat(beat + 32.));
        if (finish <= start + .05) continue;
        auto& clip = add((accent + spritePhase) % 2 == 0 ? stockSprite : stockFlare, start, finish);
        clip.controls["overlayAmount"] = .52;
        clip.controls["spriteMotion"] = (double)((accent % 5) + 1) / 6.;
        clip.controls["spriteRoute"] = sources == VisualSceneSource::milkdrop ? 0. : 1.;
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
        check(std::abs(mix.gridClock().beatsAt(cut) / 4 - std::round(mix.gridClock().beatsAt(cut) / 4)) < 1.e-6
              && valueAt(*before, AutomationTarget::volume, cut - .020) > -1 && valueAt(*before, AutomationTarget::volume, cut + .020) <= -60
              && valueAt(*after, AutomationTarget::volume, cut - .020) <= -60 && valueAt(*after, AutomationTarget::volume, cut + .020) > -1,
              "default join switches audible songs on a bar with only the anti-click ramp");
        const auto visuals = arrangeProject(mix);
        std::set<double> interior[4];
        for (const auto& c : mix.visualScenes)
            if (c.start > .05) interior[(int)visualLayer(c.scene)].insert(c.start);
        check(visuals.wasOk() && !interior[1].empty() && !interior[2].empty() && !interior[3].empty()
              && interior[1] != interior[2] && interior[2] != interior[3] && interior[0] != interior[1],
              "auto arrangement gives the four layers independent change schedules");
        check(interior[2].count(cut) != 0, "GLSL scene changes retain the aligned music handover");
        for (const auto source : {VisualSceneSource::originals, VisualSceneSource::milkdrop})
        {
            auto subset = mix; const auto result = arrangeProject(subset, source);
            check(result.wasOk() && std::none_of(subset.visualScenes.begin(), subset.visualScenes.end(), [&](const auto& c)
                { return visualLayer(c.scene) == (source == VisualSceneSource::originals ? VisualLayer::milk : VisualLayer::shader); }),
                "single-engine arrangements respect the selected source");
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
    Project selectionShow;
    Clip selectionClip; selectionClip.media = first; selectionClip.length = 2000.; selectionShow.clips.push_back(selectionClip);
    juce::Random selectionSeed(731);
    const auto selectionResult = arrangeProject(selectionShow, VisualSceneSource::both, &selectionSeed);
    std::set<juce::String> chosenScenes, chosenFamilies;
    size_t shaderCount = 0;
    for (const auto& clip : selectionShow.visualScenes)
        if (visualLayer(clip.scene) == VisualLayer::shader)
        {
            ++shaderCount; chosenScenes.insert(clip.scene);
            const auto entry = std::find_if(scenePool.begin(), scenePool.end(), [&](const auto& e) { return e.key == clip.scene; });
            if (entry != scenePool.end()) chosenFamilies.insert(entry->family);
        }
    check(selectionResult.wasOk() && shaderCount >= 26 && chosenScenes.size() == shaderCount && chosenFamilies.size() >= 25,
          "a long arrangement spans at least 25 families without repeating a scene");
    const bool added = lane.placeStudio(stockBackground.key, stockBackground.title, 0.)
        && lane.placeStudio(stockMilk.key, stockMilk.title, 0.)
        && lane.placeStudio(stockSprite.key, stockSprite.title, 0.)
        && lane.placeStudio("studio/fault-ledger", "Fault Ledger", 0.);
    check(added && desk.engine.project.visualScenes.size() == 4, "four visual layers share one time range");
    if (!added) return;
    studio.update();
    check(specs(runtime(), "studio/fault-ledger").count("motion") != 0, "Studio receives selected GLSL control specifications");
    if (specs(runtime(), "studio/fault-ledger").count("motion") != 0)
    {
        studio.edit(lane.selection, "motion", 1., 1);
        const auto* clip = lane.selectedVisual();
        check(clip && clip->automation.count("motion") && clip->automation.at("motion").cycle,
              "128-bar control updates the selected clip");
        if (clip && clip->automation.count("motion"))
        {
            std::vector<VisualSceneClip> restored;
            const auto decoded = readVisualScenes(juce::JSON::parse(juce::JSON::toString(visualScenesToJson(desk.engine.project.visualScenes))), restored);
            check(decoded.wasOk() && restored.size() == 4 && restored.back().automation.count("motion") && restored.back().automation.at("motion").cycle,
                  "all four layers and the cycle persist through session JSON");
            check(visualControlsAt(*clip, 0, 0).at("motion") == 0 && visualControlsAt(*clip, 1, 512).at("motion") == 1
                  && visualControlsAt(*clip, 2, 768).at("motion") == .5 && visualControlsAt(*clip, 3, 1024).at("motion") == 0,
                  "128 bars up and 128 bars down use elapsed grid beats");
        }
        studio.edit(lane.selection, "motion", 0., 1);
        studio.edit(lane.selection, "motion", .2, 2);
        const auto* clipWithEnds = lane.selectedVisual();
        if (clipWithEnds) studio.editPoint(clipWithEnds->start + clipWithEnds->length * .5, .8, false);
        const auto* keyed = lane.selectedVisual();
        check(keyed && keyed->automation.count("motion") && keyed->automation.at("motion").points.size() == 3
              && std::abs(visualControlsAt(*keyed, keyed->length * .5).at("motion") - .8) < 1.e-7,
              "manual visual keyframes drive values inside the selected clip");
    }
    studio.studioRoot = report.getSiblingFile("studio-link");
    studio.studioRoot.getChildFile("requests").createDirectory();
    studio.exchange();
    const auto state = json(studio.studioRoot.getChildFile("state.json"));
    check(state["clip"].toString() == lane.selection && state["parameters"]["milkMix"].isObject(),
          "standalone mailbox publishes clip identity and original / MilkDrop control");
    const auto send = [&](const juce::String& clipId, double value, const juce::String& name)
    {
        auto* request = new juce::DynamicObject();
        request->setProperty("clip", clipId); request->setProperty("parameter", name);
        request->setProperty("value", value); request->setProperty("action", 0);
        studio.studioRoot.getChildFile("requests/edit.json").replaceWithText(juce::JSON::toString(juce::var(request)));
        studio.exchange();
    };
    send("stale-clip", .17, "milkMix");
    check(lane.selectedVisual()->controls.count("milkMix") == 0, "stale Studio edits cannot modify another clip");
    send(lane.selection, .17, "milkMix");
    check(lane.selectedVisual()->controls.at("milkMix") == .17, "standalone edits update the actual timeline clip");
    send(lane.selection, 2., "milkMix");
    check(lane.selectedVisual()->controls.at("milkMix") == .17, "out-of-range Studio values are rejected");
    desk.undo(false);
    check(lane.selectedVisual() && lane.selectedVisual()->controls.count("milkMix") == 0, "standalone control edits participate in Alloy Undo");
    desk.detachToolPanel(3, true);
    check(desk.studioWindow && desk.studioPanel->getParentComponent() != &desk, "Studio detaches into an independent panel");
    const auto deadline = juce::Time::getMillisecondCounter() + 14000;
    while (!lane.preview.isValid() && juce::Time::getMillisecondCounter() < deadline)
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
    desk.detachToolPanel(3, false);
    check(desk.studioPanel->getParentComponent() == &desk, "Studio reattaches to the original tab");
    check(save(desk.createComponentSnapshot(desk.getLocalBounds()), "alloy-version.png"),
          "capture the docked workspace and visible Alloy version");
}
}
