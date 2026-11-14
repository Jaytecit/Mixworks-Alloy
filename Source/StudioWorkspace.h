#pragma once
#include "SceneLane.h"

namespace mixworks
{
class StudioWorkspace final : public juce::Component
{
public:
    explicit StudioWorkspace(SceneLane&);
    ~StudioWorkspace() override;
    bool openStudio();
    void resized() override;
    void paint(juce::Graphics&) override;
    void update();
    void followPlayhead(double position);
    void pick(VisualLayer, double time, const juce::String& replacement = {});
    void editPoint(double time, double value, bool remove);
    static std::vector<VisualSceneEntry> library(const juce::File& install);
    static juce::String overrideKey(const VisualSceneClip&, const juce::String& parameter);
    static VisualOverrides overridesFor(const VisualSceneClip&, const VisualOverrides&);
    static juce::Result arrangeProject(Project&, VisualSceneSource = VisualSceneSource::both);
    static juce::Result arrangeProject(Project&, VisualSceneSource, juce::Random* choices);
    static void runChecks(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    bool arrange();
    bool outputShowing() const;
    const juce::File& sessionFolder() const { return studioRoot; }
    const juce::var& outputView() const { return studioOutput; }
    void focusFromLane(const juce::String& id);
private:
    friend class SceneLane;
    using Specs = std::map<juce::String, VisualControlSpec>;
    static Specs specs(const juce::File& install, const juce::String& key);
    Specs controlsFor(const VisualSceneClip&);
    const VisualSceneClip* clipFor(const juce::String&) const;
    std::array<const VisualSceneClip*, 4> targetsAt(double position) const;
    const VisualSceneClip* focusedClip(double position) const;
    void edit(const juce::String& id, const juce::String& name, double value, int action,
              const std::map<juce::String, double>& values = {}, const juce::var& settings = {});
    void resetControls(const VisualSceneClip&, const juce::var& parameters);
    void swapRandom(VisualLayer);
    void addFile(VisualLayer, double time, const juce::String& replacement);
    bool applyVisualChoice(const juce::String& key, const juce::String& title, double time, const juce::String& replacement);
    bool publishArrangement();
    void exchange();
    SceneLane& lane;
    Workstation& owner;
    juce::TextButton backgrounds {"+ Background"}, presets {"+ MilkDrop"}, shaders {"+ GLSL"}, sprites {"+ Sprite"};
    juce::TextButton output {"Open Visual Studio"};
    juce::TextButton match {"Match Mix"};
    juce::Slider zoom, scroll;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::ChildProcess studioProcess;
    juce::File studioRoot;
    juce::var studioOutput;
    double lastExchange = 0;
    bool studioConnecting = false;
    juce::String lastGesture;
    juce::String swapStatus;
    juce::String pinnedClip, observedLaneSelection, cachedScene;
    int focusedLayer = -1;
    Specs cachedSpecs;
};
}
