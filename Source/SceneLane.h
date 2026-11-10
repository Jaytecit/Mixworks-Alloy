#pragma once
#include "VisualScenes.h"
#include "StudioAudioLink.h"
#include <optional>
#include <juce_gui_extra/juce_gui_extra.h>

namespace mixworks
{
class Workstation;
class SceneBrowser;
class SceneRenderWorker;
class MilkDropIndexWorker;
class StudioWorkspace;
class SceneLane final : public juce::Component, public juce::DragAndDropTarget, public juce::SettableTooltipClient, private juce::Timer
{
public:
    static constexpr int height = 82;
    explicit SceneLane(Workstation&);
    ~SceneLane() override;
    juce::TextButton browserButton {"Visuals / scenes"};
    void showBrowser(bool replaceSelection = false);
    bool shareStudioAudio(bool enabled, int receiverPort = StudioAudioLink::port);
    bool studioAudioShared() const { return studioAudio != nullptr; }
    bool replaceScene(const juce::String& id, const juce::String& scene, int preset = -1);
    void matchMix();
    bool setImage(const juce::String& path);
    bool importMilkDrop(const juce::File& source);
    bool linkMilkDrop(const juce::File& source);
    bool place(const juce::String& scene, double start, int preset = -1);
    bool placeStudio(const juce::String& key, const juce::String& title, double start, double length = 0);
    const VisualSceneClip* selectedVisual() const;
    void setStudioWorkspace(StudioWorkspace*);
    double studioScale = 12, studioStart = 0;
    juce::String studioParameter;
    bool applyLook(const juce::String& id, const std::map<juce::String,double>& values, bool replace, const juce::String& title);
    bool change(const juce::String& id, double start, double length, double offset, bool remember = true);
    void removeSelected();
    void duplicateSelected();
    bool hasSelectedScene() const;
    const std::vector<VisualSceneEntry>& catalogue() const { return scenes; }
    juce::String selection;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    bool isInterestedInDragSource(const SourceDetails&) override;
    void itemDropped(const SourceDetails&) override;
    void itemDragMove(const SourceDetails&) override;
    void itemDragExit(const SourceDetails&) override;
    juce::Image preview;
    juce::var previewFacts;
    juce::String previewStatus;
    void recordPreviewPaint(); // Deduplicated delivery facts for the embedded Studio as well as output.
    Workstation& owner;
private:
    friend class StudioWorkspace;
    friend void runOriginalVisualTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
    friend void runMilkDropTests(const std::function<void(bool, const juce::String&)>&, const juce::File&, const juce::File&);
    std::vector<VisualSceneEntry> scenes;
    std::unique_ptr<juce::DocumentWindow> browser;
    std::unique_ptr<SceneRenderWorker> worker;
    std::unique_ptr<StudioAudioLink> studioAudio;
    std::unique_ptr<MilkDropIndexWorker> milkDropIndexer;
    SceneBrowser* browserContent = nullptr;
    StudioWorkspace* studioWorkspace = nullptr;
    std::optional<VisualSceneClip> dragOriginal;
    int dragEdge = 0;
    double dragX = 0, dropTime = -1;
    bool dragRemembered = false;
    juce::Rectangle<float> bounds(const VisualSceneClip&) const;
    double at(float x, bool snap) const;
    double studioX(double seconds) const;
    void paintStudio(juce::Graphics&);
    const VisualSceneClip* hit(juce::Point<float>) const;
    void timerCallback() override;
};
}
