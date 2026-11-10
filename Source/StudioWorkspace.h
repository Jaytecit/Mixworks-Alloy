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
    void pick(VisualLayer, double time);
    void editPoint(double time, double value, bool remove);
    static std::vector<VisualSceneEntry> library(const juce::File& install);
    static juce::Result arrangeProject(Project&, VisualSceneSource = VisualSceneSource::both);
    static juce::Result arrangeProject(Project&, VisualSceneSource, juce::Random* choices);
    static void runChecks(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    bool arrange();
    bool outputShowing() const;
private:
    using Specs = std::map<juce::String, VisualControlSpec>;
    static Specs specs(const juce::File& install, const juce::String& key);
    void edit(const juce::String& id, const juce::String& name, double value, int action);
    void addFile(VisualLayer, double time);
    bool publishArrangement();
    void exchange();
    SceneLane& lane;
    Workstation& owner;
    juce::TextButton backgrounds {"+ Background"}, presets {"+ MilkDrop"}, shaders {"+ GLSL"}, sprites {"+ Sprite"};
    juce::TextButton automatic {"Auto arrange visuals"}, output {"Open Visual Studio"};
    juce::Slider zoom, scroll;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::ChildProcess studioProcess;
    juce::File studioRoot;
    juce::Image sharedPreview;
    juce::Rectangle<int> previewArea;
    double lastExchange = 0;
    juce::String lastGesture;
};
}
