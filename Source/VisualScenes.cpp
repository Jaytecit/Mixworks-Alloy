#include "VisualScenes.h"
#include <algorithm>
#include <cmath>
#include <set>

namespace mixworks
{
namespace
{
bool originalKey(const juce::String& key)
{
    const auto slug=key.substring(9);
    return key.startsWith("original/") && slug.isNotEmpty() && slug.length()<=64
        && slug.containsOnly("abcdefghijklmnopqrstuvwxyz0123456789-")
        && juce::CharacterFunctions::isLetter(slug[0]);
}
bool validKey(const juce::String& key)
{
    return originalKey(key);
}
bool number(const juce::var& v)
{ return (v.isDouble() || v.isInt() || v.isInt64()) && std::isfinite((double)v); }
}
bool validVisualTransition(const juce::String& value)
{
    return value == "dissolve" || value == "zoom" || value == "radial" || value == "sweep" || value == "cut" || value == "sweep-reverse";
}
juce::var visualControlsToJson(const std::map<juce::String, double>& controls)
{
    auto* object = new juce::DynamicObject();
    for (const auto& [name, value] : controls) object->setProperty(name, value);
    return juce::var(object);
}
void describeVisualScene(VisualSceneEntry& entry)
{
    if (entry.key == "original/prism-bloom")
    { entry.style="Prismatic"; entry.energy=.55; entry.imageReady=true; entry.imageControls={{"imageMix",.35}}; return; }
    if (entry.key=="original/velvet-current" || entry.key=="original/neon-run" || entry.key=="original/ember-mosaic")
    {
        entry.style=entry.key=="original/velvet-current" ? "Fluid" : "Electric";
        entry.energy=entry.key=="original/velvet-current" ? .4 : entry.key=="original/neon-run" ? .8 : .95;
        entry.imageReady=true; entry.imageControls={{"imageMix",.35}}; return;
    }

}
std::vector<VisualSceneEntry> discoverVisualScenes(const juce::File& install)
{
    std::vector<VisualSceneEntry> result;
    if (install.getChildFile("original-scenes.json").existsAsFile())
    {
        const auto manifest=juce::JSON::parse(install.getChildFile("original-scenes.json"));
        std::set<juce::String> seen;
        if ((int)manifest["schema"]==1)
            if (const auto* entries=manifest["scenes"].getArray()) for (const auto& entry : *entries)
            {
                const auto key=entry["key"].toString(), title=entry["title"].toString();
                if (!originalKey(key) || title.isEmpty() || title.length()>96 || !seen.insert(key).second) continue;
                const auto directory=install.getChildFile("scenes").getChildFile(key.fromFirstOccurrenceOf("/",false,false));
                if (!directory.getChildFile("main.glsl").existsAsFile() || !directory.getChildFile("scene.json").existsAsFile()) continue;
                VisualSceneEntry item {key,title};
                item.style=entry["style"].toString(); item.energy=juce::jlimit(0.,1.,(double)entry["energy"]);
                item.imageReady=(bool)entry["imageReady"]; item.imageControls={{"imageMix",.5}};
                const auto descriptor=juce::JSON::parse(directory.getChildFile("scene.json"));
                item.family=descriptor["family"].toString();
                if (const auto* controls=descriptor["controls"].getDynamicObject())
                    for (const auto& prop : controls->getProperties())
                        if (number(prop.value["default"]) && (double)prop.value["min"]==0 && (double)prop.value["max"]==1
                            && (double)prop.value["default"]>=0 && (double)prop.value["default"]<=1)
                            item.controlSpecs[prop.name.toString()]={(double)prop.value["default"],(int)prop.value["steps"]};
                if (item.controlSpecs.empty() || item.controlSpecs.size()>32) continue;
                if (const auto* presets=descriptor["presets"].getArray(); presets && presets->size()<=16)
                {
                    std::set<juce::String> presetIds;
                    for (const auto& preset : *presets)
                    {
                        VisualScenePreset look {preset["id"].toString(),preset["title"].toString()};
                        bool valid=look.id.isNotEmpty() && look.id.length()<=48
                            && look.id.containsOnly("abcdefghijklmnopqrstuvwxyz0123456789-")
                            && look.title.isNotEmpty() && look.title.length()<=64;
                        if (const auto* values=preset["controls"].getDynamicObject())
                            for (const auto& value : values->getProperties())
                            {
                                const auto name=value.name.toString();
                                if (!item.controlSpecs.count(name) || !number(value.value) || (double)value.value<0 || (double)value.value>1) valid=false;
                                else look.controls[name]=(double)value.value;
                            }
                        if (valid && !look.controls.empty() && presetIds.insert(look.id).second) item.presets.push_back(std::move(look));
                    }
                }
                result.push_back(std::move(item));
            }
        return result;
    }
    return result;
}
std::vector<VisualSceneEntry> automaticVisualScenes(const std::vector<VisualSceneEntry>& entries)
{
    std::vector<VisualSceneEntry> result;
    std::set<juce::String> seen;
    for (const auto& entry : entries)
        if (originalKey(entry.key) && seen.insert(entry.key).second) result.push_back(entry);
    return result;
}
VisualSceneFrame visualFrameAt(const std::vector<VisualSceneClip>& clips, double at)
{
    VisualSceneFrame frame;
    for (const auto& c : clips) if (at >= c.start && at < c.end())
    {
        if (!frame.outgoing) frame.outgoing = &c;
        else frame.incoming = &c;
    }
    if (frame.incoming)
        frame.mix = juce::jlimit(0., 1., (at - frame.incoming->start) / (frame.outgoing->end() - frame.incoming->start));
    return frame;
}
juce::Result validateVisualScenes(const std::vector<VisualSceneClip>& clips)
{
    std::set<juce::String> ids;
    double previousStart = -1, previousEnd = -1, earlierEnd = -1;
    for (const auto& c : clips)
    {
        if (c.id.isEmpty() || !ids.insert(c.id).second || !validKey(c.scene) || c.title.isEmpty()
            || !std::isfinite(c.start) || !std::isfinite(c.length) || !std::isfinite(c.offset)
            || c.start < 0 || c.length < .05 || c.offset < 0 || c.offset > 1.0e9 || !std::isfinite(c.end()) || c.end() > 1.0e9
            || c.start < previousStart || c.end() < previousEnd || c.start < earlierEnd
            || !validVisualTransition(c.transition) || !std::isfinite(c.pivot) || c.pivot<0 || c.pivot>1)
            return juce::Result::fail("Scenes must stay in order, with at most two overlapping for a blend.");
        for (const auto& [name,value] : c.controls)
            if (name.isEmpty() || !name.containsOnly("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")
                || juce::CharacterFunctions::isDigit(name[0]) || !std::isfinite(value))
                return juce::Result::fail("Invalid generated visual control.");
        if(c.automation.size()>32) return juce::Result::fail("Too many visual automation controls.");
        for(const auto& [name,curve] : c.automation)
        {
            if(name.isEmpty() || !name.containsOnly("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")
                || juce::CharacterFunctions::isDigit(name[0]) || curve.points.size()<2 || curve.points.size()>64)
                return juce::Result::fail("Invalid visual automation curve.");
            double previous=-1;
            for(const auto& point:curve.points)
            {
                if(!std::isfinite(point.at) || !std::isfinite(point.value) || point.at<0 || point.at>1 || point.at<=previous || point.value<0 || point.value>1)
                    return juce::Result::fail("Visual automation points must be ordered and bounded.");
                previous=point.at;
            }
            if(curve.points.front().at!=0 || curve.points.back().at!=1) return juce::Result::fail("Visual automation must cover its block.");
        }
        earlierEnd = previousEnd; previousStart = c.start; previousEnd = c.end();
    }
    return juce::Result::ok();
}
juce::var visualScenesToJson(const std::vector<VisualSceneClip>& clips)
{
    juce::Array<juce::var> values;
    for (const auto& c : clips)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty("id", c.id); o->setProperty("scene", c.scene); o->setProperty("title", c.title);
        o->setProperty("start", c.start); o->setProperty("length", c.length); o->setProperty("offset", c.offset);
        o->setProperty("transition",c.transition); o->setProperty("style",c.style); o->setProperty("pivot",c.pivot);
        o->setProperty("controls",visualControlsToJson(c.controls));
        auto* automation=new juce::DynamicObject();
        for(const auto& [name,curve]:c.automation)
        {
            auto* value=new juce::DynamicObject(); value->setProperty("stepped",curve.stepped);
            juce::Array<juce::var> points;
            for(const auto& point:curve.points) points.add(juce::var(juce::Array<juce::var>{point.at,point.value}));
            value->setProperty("points",points);automation->setProperty(name,juce::var(value));
        }
        o->setProperty("automation",juce::var(automation)); values.add(juce::var(o));
    }
    return values;
}
juce::Result readVisualScenes(const juce::var& value, std::vector<VisualSceneClip>& destination)
{
    if (!value.isArray()) return juce::Result::fail("Invalid visual scene lane.");
    std::vector<VisualSceneClip> parsed;
    for (const auto& v : *value.getArray())
    {
        if (!v.isObject() || !v["id"].isString() || !v["scene"].isString() || !v["title"].isString()
            || !number(v["start"]) || !number(v["length"]) || !number(v["offset"]))
            return juce::Result::fail("Invalid visual scene block.");
        VisualSceneClip clip {v["id"].toString(), v["scene"].toString(), v["title"].toString(), (double)v["start"], (double)v["length"], (double)v["offset"]};
        if (v.hasProperty("transition"))
        {
            if (!v["transition"].isString() || !v["style"].isString() || !number(v["pivot"]) || !v["controls"].isObject())
                return juce::Result::fail("Invalid visual look or transition.");
            clip.transition=v["transition"].toString(); clip.style=v["style"].toString(); clip.pivot=(double)v["pivot"];
            for (const auto& property : v["controls"].getDynamicObject()->getProperties())
            {
                if (!number(property.value)) return juce::Result::fail("Invalid visual control value.");
                clip.controls.emplace(property.name.toString(),(double)property.value);
            }
        }
        if(v.hasProperty("automation"))
        {
            const auto* object=v["automation"].getDynamicObject();
            if(!object || object->getProperties().size()>32) return juce::Result::fail("Invalid visual automation object.");
            for(const auto& prop:object->getProperties())
            {
                const auto* points=prop.value["points"].getArray();
                if(!prop.value["stepped"].isBool() || !points || points->size()<2 || points->size()>64)
                    return juce::Result::fail("Invalid visual automation points.");
                VisualControlCurve curve;curve.stepped=(bool)prop.value["stepped"];
                for(const auto& point:*points)
                {
                    if(!point.isArray() || point.size()!=2 || !number(point[0]) || !number(point[1]))
                        return juce::Result::fail("Invalid visual automation point.");
                    curve.points.push_back({(double)point[0],(double)point[1]});
                }
                clip.automation.emplace(prop.name.toString(),std::move(curve));
            }
        }
        parsed.push_back(std::move(clip));
    }
    const auto valid = validateVisualScenes(parsed); if (valid.failed()) return valid;
    destination = std::move(parsed); return juce::Result::ok();
}

std::map<juce::String,double> visualControlsAt(const VisualSceneClip& scene,double localTime)
{
    auto values=scene.controls;
    const double at=juce::jlimit(0.,1.,(localTime-scene.offset)/std::max(.05,scene.length));
    for(const auto& [name,curve]:scene.automation)
    {
        if(curve.points.empty()) continue;
        double value=curve.points.back().value;
        for(size_t i=1;i<curve.points.size();++i) if(at<curve.points[i].at)
        {
            const auto a=curve.points[i-1],b=curve.points[i];
            double u=juce::jlimit(0.,1.,(at-a.at)/(b.at-a.at)); u=u*u*(3.-2.*u);
            value=curve.stepped?a.value:a.value+(b.value-a.value)*u;break;
        }
        values[name]=value;
    }
    return values;
}
double visualMotionTime(const VisualSceneClip& scene,double localTime)
{
    const auto found=scene.automation.find("motion");
    const double duration=std::max(.05,scene.length),elapsed=juce::jlimit(0.,duration,localTime-scene.offset);
    if(found==scene.automation.end())
    { const auto value=scene.controls.find("motion");return localTime*(.25+.75*(value==scene.controls.end()?.5:value->second)); }
    const auto& curve=found->second;double integrated=0.;
    for(size_t i=1;i<curve.points.size();++i)
    {
        const auto a=curve.points[i-1],b=curve.points[i];const double span=(b.at-a.at)*duration;
        const double u=juce::jlimit(0.,1.,(elapsed-a.at*duration)/span);
        integrated+=span*(a.value*u+(curve.stepped?0.:(b.value-a.value)*(u*u*u-.5*u*u*u*u)));
    }
    return scene.offset*(.25+.75*curve.points.front().value)+elapsed*.25+integrated*.75;
}
void automateVisualScene(VisualSceneClip& scene,const VisualSceneEntry& entry,double energy,juce::Random& random)
{
    scene.automation.clear();
    for(const auto& [name,spec]:entry.controlSpecs)
    {
        scene.controls[name]=spec.initial;
        // Image choice is fixed for each scene; the pool advances on project phrases.
        if(name=="imageIndex") { scene.controls[name]=random.nextDouble();continue; }
        VisualControlCurve curve;curve.stepped=spec.steps>1;
        for(int i=0;i<5;++i)
        {
            double value=spec.initial+(random.nextDouble()-.5)*.32;
            if(name=="intensity" || name=="motion" || name=="pulse") value=.25+energy*.5+(random.nextDouble()-.5)*.18;
            if(name=="imageRotation") value=.5+(random.nextDouble()-.5)*.12;
            if(name=="symmetry") value=(random.nextInt(4)==0?.33:0.);
            value=juce::jlimit(.05,.95,value);
            if(curve.stepped)value=std::round(value*(spec.steps-1))/(spec.steps-1);
            curve.points.push_back({i*.25,value});
        }
        scene.automation.emplace(name,std::move(curve));
    }
}
}
