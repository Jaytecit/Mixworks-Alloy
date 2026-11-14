#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <vector>
#include <map>

namespace mixworks
{
enum class VisualSceneSource { originals=1, milkdrop=2, both=3 };
VisualSceneSource visualSceneSourceFromId(int);
juce::String visualSceneSourceName(VisualSceneSource);
struct VisualControlPoint { double at=0, value=0; };
struct VisualControlCurve
{
    bool stepped = false;
    std::vector<VisualControlPoint> points;
    bool cycle = false;
    double cycleMin = 0., cycleMax = 1., cycleBars = 64.;
    juce::String audioSource;
    double audioAmount = .25;
};
struct VisualControlSpec { double initial=.5; int steps=0; juce::String label; };
using VisualOverrides = std::map<juce::String, double>;
enum class VisualLayer { background, milk, shader, sprite };
VisualLayer visualLayer(const juce::String& key);
double visualCycleAt(double elapsedBeats, const VisualControlCurve&);
bool validVisualAudioSource(const juce::String&);
const std::array<std::pair<juce::String, juce::String>, 3>& studioTransitions();
struct VisualScenePreset
{
    juce::String id, title;
    std::map<juce::String,double> controls;
};
struct VisualSceneClip
{
    juce::String id = juce::Uuid().toString(), scene, title;
    double start = 0, length = 16, offset = 0;
    juce::String transition = "feedback", style;
    double pivot = .5;
    std::map<juce::String, double> controls;
    std::map<juce::String, VisualControlCurve> automation;
    double end() const { return start + length; }
};
struct VisualSceneEntry
{
    juce::String key, title, style;
    double energy = .5;
    bool imageReady = false;
    std::map<juce::String, double> imageControls;
    std::map<juce::String, VisualControlSpec> controlSpecs;
    std::vector<VisualScenePreset> presets;
    juce::String family, motif;
    juce::String referenceImage;
};
std::map<juce::String,double> visualControlsAt(const VisualSceneClip&, double localTime, double elapsedBeats = 0);
double visualMotionTime(const VisualSceneClip&, double localTime);
void automateVisualScene(VisualSceneClip&, const VisualSceneEntry&, double energy, juce::Random&);
void describeVisualScene(VisualSceneEntry&);
bool validVisualTransition(const juce::String&);
juce::var visualControlsToJson(const std::map<juce::String, double>&);
struct VisualSceneFrame
{
    const VisualSceneClip* outgoing = nullptr;
    const VisualSceneClip* incoming = nullptr;
    double mix = 0;
    const VisualSceneClip* primary() const { return incoming ? incoming : outgoing; }
};
std::vector<VisualSceneEntry> discoverVisualScenes(const juce::File& install);
std::vector<VisualSceneEntry> automaticVisualScenes(const std::vector<VisualSceneEntry>&, VisualSceneSource = VisualSceneSource::originals);
VisualSceneFrame visualFrameAt(const std::vector<VisualSceneClip>&, double seconds);
juce::Result validateVisualScenes(const std::vector<VisualSceneClip>&);
juce::var visualScenesToJson(const std::vector<VisualSceneClip>&);
juce::Result readVisualScenes(const juce::var&, std::vector<VisualSceneClip>&);
juce::var visualOverridesToJson(const VisualOverrides&);
juce::Result readVisualOverrides(const juce::var&, VisualOverrides&);
struct Project;
juce::Result arrangeVisualScenes(Project&, const std::vector<VisualSceneEntry>&, juce::Random&, VisualSceneSource = VisualSceneSource::originals);
}
