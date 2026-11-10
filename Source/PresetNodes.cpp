#include "PresetNodes.h"
#include <cmath>
#include <set>

namespace mixworks
{
namespace
{
using Links = std::set<juce::String>;
struct NodeSource
{
    const Clip& clip;
    AutomationAddress address;
    TransitionPlayback playback;
    ClipEnvelope fades;
    bool generated = false;
};
NodeSource nodeSource(const Project& project, const Clip& clip, AutomationTarget target, const Links& links)
{
    NodeSource result{clip,{clip.track,target,{},{},clip.id},{},clip.envelope(),false};
    result.fades = {clip.length,0,clip.length,0,0};
    const auto full = project.transitionPlaybackFor(clip);
    if (links.empty() && target == AutomationTarget::volume && (clip.fadeIn > 0 || clip.fadeOut > 0))
    {
        const auto original = clip.envelope();
        if (clip.fadeIn > 0) { result.fades.intro = original.intro; result.fades.fadeIn = clip.fadeIn; }
        if (clip.fadeOut > 0) { result.fades.outro = original.outro; result.fades.fadeOut = clip.fadeOut; }
        result.generated = true;
    }
    for (const auto& link : project.transitionLinks) if (links.count(link.incoming))
    {
        const bool incoming = link.incoming == clip.id, outgoing = link.outgoing == clip.id;
        if (!incoming && !outgoing) continue;
        const size_t side = incoming ? 0 : 1;
        result.playback.pairs[side] = full.pairs[side]; result.playback.filters[side] = full.filters[side];
        const auto originalFades = clip.envelope();
        if (incoming && clip.fadeIn > 0) { result.fades.intro = originalFades.intro; result.fades.fadeIn = clip.fadeIn; }
        else if (outgoing && clip.fadeOut > 0) { result.fades.outro = originalFades.outro; result.fades.fadeOut = clip.fadeOut; }
        const int band = (int) target;
        result.generated |= band >= 0 && band < 4 && full.pairs[side][(size_t) band].strength > 0;
        result.generated |= target == AutomationTarget::volume && (incoming ? clip.fadeIn : clip.fadeOut) > 0;
        result.generated |= target == AutomationTarget::cutoff && full.filters[side].setting.active();
    }
    return result;
}
Links linksFor(const Project& p, const juce::String& id)
{
    Links result; for (const auto& link : p.transitionLinks) if (link.incoming == id || link.outgoing == id) result.insert(link.incoming); return result;
}
void shapeClock(AutomationShape& shape, const Project& project, const AutomationAddress& address, double begin, double end)
{
    shape.clock.points.clear();
    const auto* clip = project.clipFor(address.clipId);
    if (!clip || !clip->tempoMap || end <= begin) return;
    shape.clock.points = clip->tempoMap->localPoints(begin,end);
    for (auto& point : shape.clock.points) point.time /= end-begin;
    const double bpm = shape.clock.points.front().bpm;
    if (std::all_of(shape.clock.points.begin(),shape.clock.points.end(),[&](const auto& point) { return point.bpm == bpm; })) shape.clock.points.clear();
}
// Fresh generated treatments are exact, editable two-endpoint spans. Independent
// entrance/exit fades retain their own endpoints and the intentional hold between.
bool directNodes(const Project& project, const NodeSource& source, AutomationCurve& result)
{
    const int band = (int)source.address.target;
    struct Fade { double begin, end; AutomationShape shape; bool step = false; };
    std::vector<Fade> fades;
    if (band >= 0 && band < 4) for (const auto& pair : source.playback.pairs)
    {
        const auto& envelope = pair[(size_t)band];
        if (envelope.strength <= 0) continue;
        AutomationShape shape; shape.kind = envelope.constantPower ? 1 : 0; shape.strength = envelope.strength;
        shape.bend = envelope.bend >= 1 ? 0 : envelope.bend; shape.rising = envelope.rising;
        fades.push_back({envelope.begin,envelope.end,shape,envelope.instant || envelope.bend >= 1});
    }
    if (band == 0)
    {
        if (source.fades.fadeIn > 0) fades.push_back({source.clip.start+source.fades.intro,source.clip.start+source.fades.intro+source.fades.fadeIn,{}});
        if (source.fades.fadeOut > 0)
        { AutomationShape shape; shape.rising = false; fades.push_back({source.clip.start+source.fades.outro,source.clip.start+source.fades.outro+source.fades.fadeOut,shape}); }
    }
    if (source.address.target == AutomationTarget::cutoff) for (const auto& envelope : source.playback.filters)
    {
        if (!envelope.setting.active()) continue;
        AutomationShape shape; shape.kind = envelope.highPass() ? 2 : 3; shape.strength = envelope.setting.strength;
        shape.rising = !envelope.incoming; shape.upper = std::min(20000.,source.clip.media->sampleRate*.45);
        fades.push_back({envelope.begin,envelope.end,shape});
    }
    std::sort(fades.begin(),fades.end(),[](const auto& a,const auto& b) { return a.begin < b.begin; });
    if (fades.empty()) return false;
    for (size_t i = 1; i < fades.size(); ++i) if (fades[i].begin <= fades[i-1].end) return false;
    const double manual = project.manualValue(source.address);
    result = {}; result.enabled = true;
    for (auto& fade : fades)
    {
        const bool gain = fade.shape.kind < 2;
        const auto value = [&](double u) { const double v = fade.shape.value(u); return (float)(gain ? std::max(-160.,juce::Decibels::gainToDecibels(v,-160.)+manual) : v); };
        double begin = project.automationTime(source.address,fade.begin), end = project.automationTime(source.address,fade.end);
        if (fade.step) begin = std::max(0.,end-1.e-9);
        if (end <= begin) return false;
        shapeClock(fade.shape,project,source.address,fade.begin,fade.end);
        if (!fade.shape.valid()) return false;
        const auto first = value(0), last = value(1);
        result.points.push_back({begin,first,fade.step ? 1. : 0.,false,gain,{},fade.step ? std::nullopt : std::optional<AutomationShape>(fade.shape)});
        result.points.push_back({end,last,0,false,gain});
    }
    return true;
}
juce::Result composeNodes(const Project& p, const NodeSource& source, AutomationCurve& result)
{
    const auto* saved = p.curve(source.address); result = saved ? *saved : AutomationCurve{};
    if (!source.generated) return juce::Result::ok();
    if (saved && !saved->enabled && !saved->points.empty())
        return juce::Result::fail("Enable or clear the existing disabled curve before converting its preset to nodes. Its saved points were kept.");
    const int band = (int) source.address.target; const bool filter = source.address.target == AutomationTarget::cutoff;
    if (filter && saved && saved->active())
        return juce::Result::fail("This track already has manual cutoff automation. Keep it or clear that curve before applying a generated filter sweep. The curve was kept.");
    const auto range = automationRange(source.address.target); const float manual = p.manualValue(source.address);
    const auto curveTime = [&](double t) { return p.automationTime(source.address,t); };
    const auto session = [&](double x)
    {
        // Exact structural endpoints must survive the source/session round trip,
        // including cuts and clip bounds on a ramped tempo map.
        for (double t : {source.clip.start,source.clip.end()}) if (x==curveTime(t)) return t;
        for (const auto& pair : source.playback.pairs) for (const auto& e : pair)
            if (e.strength>0) for (double t : {e.begin,e.end}) if (x==curveTime(t)) return t;
        return p.automationSessionTime(source.address,x);
    };
    if ((!saved || saved->points.empty()) && directNodes(p,source,result)) return juce::Result::ok();
    if (filter)
        return juce::Result::fail("These filter sweeps overlap. Shorten or move their frames so only one sweep controls this track at a time. The curves were kept.");
    if (band < 0 || band >= 4) return juce::Result::fail("This parameter has no generated fade.");
    std::set<double> knots {curveTime(source.clip.start),curveTime(source.clip.end())};
    if (saved) for (const auto& point : saved->points) knots.insert(point.time);
    for (const auto& pair : source.playback.pairs)
    { const auto& e=pair[(size_t)band]; if (e.strength>0) { knots.insert(curveTime(e.begin)); knots.insert(curveTime(e.end)); } }
    if (band==0)
    {
        if (source.fades.fadeIn>0) { knots.insert(curveTime(source.clip.start+source.fades.intro)); knots.insert(curveTime(source.clip.start+source.fades.intro+source.fades.fadeIn)); }
        if (source.fades.fadeOut>0) { knots.insert(curveTime(source.clip.start+source.fades.outro)); knots.insert(curveTime(source.clip.start+source.fades.outro+source.fades.fadeOut)); }
    }
    if (knots.size()>4096) return juce::Result::fail("This track already has too many authored nodes to add these fade endpoints. Remove unused nodes first. The curves were kept.");
    const auto baseAt = [&](double x) { return saved ? saved->interpolatedValue(x,manual) : manual; };
    const auto gain = [](double db) { return db<=-160 ? 0. : juce::Decibels::decibelsToGain(db); };
    const double clipFirst=curveTime(source.clip.start),clipLast=curveTime(source.clip.end());
    const auto value = [&](double x)
    {
        const double t=session(x); double v=gain(baseAt(x));
        if (x>=clipFirst && x<clipLast)
        { v*=source.playback.gainAt(band,t); if (band==0) v*=source.fades.gainAt(t-source.clip.start); }
        return (float)juce::Decibels::gainToDecibels(v,-160.);
    };
    result={}; result.enabled=true;
    for (const double x : knots) result.points.push_back({x,value(x),0,false,true});
    for (size_t i=0;i+1<result.points.size();++i)
    {
        const double a=result.points[i].time,b=result.points[i+1].time,begin=session(a),end=session(b),duration=end-begin;
        if (duration<=0) return juce::Result::fail("The source clock cannot represent these fade endpoints. The curves were kept.");
        AutomationShape composite; composite.kind=4; shapeClock(composite,p,source.address,begin,end);
        AutomationShape base; base.kind=5; base.first=baseAt(a); base.last=baseAt(b);
        if (saved && !saved->points.empty())
        {
            const auto slice=saved->sliced(a,b,manual); const auto& point=slice.points.front();
            base.bend=point.bend; base.exponential=point.exponential; base.gainInterpolation=point.gainSpace;
            if (point.shape) base.terms.push_back(*point.shape);
            composite.endpointStep=point.bend>=1 || (point.shape && point.shape->endpointStep);
        }
        shapeClock(base,p,source.address,begin,end); base.forwardClock=true;
        composite.terms.push_back(std::move(base));
        const auto fade = [&](double firstTime,double lastTime,AutomationShape shape,bool step)
        {
            shape.inputEnd=(lastTime-begin)/duration;
            shape.inputBegin=step ? shape.inputEnd-1 : (firstTime-begin)/duration;
            if (step) shape.kind=7;
            return shape;
        };
        // Every term is evaluated analytically in the interval's project clock.
        // Only actual authored nodes and the two endpoints of each fade are knots.
        if (a>=clipFirst && b<=clipLast)
        {
            for (const auto& pair : source.playback.pairs)
            {
                const auto& e=pair[(size_t)band]; if (e.strength<=0) continue;
                AutomationShape shape; shape.kind=e.constantPower ? 1 : 0; shape.strength=e.strength;
                shape.rising=e.rising; shape.bend=e.bend>=1 ? 0 : e.bend;
                composite.terms.push_back(fade(e.begin,e.end,shape,e.instant || e.bend>=1));
                if ((e.instant || e.bend>=1) && std::abs(end-e.end)<1.e-9) composite.endpointStep=true;
            }
            if (band==0 && (source.fades.fadeIn>0 || source.fades.fadeOut>0))
            {
                AutomationShape clip; clip.kind=6;
                if (source.fades.fadeIn>0) clip.terms.push_back(fade(source.clip.start+source.fades.intro,source.clip.start+source.fades.intro+source.fades.fadeIn,{},false));
                if (source.fades.fadeOut>0)
                { AutomationShape shape; shape.rising=false; clip.terms.push_back(fade(source.clip.start+source.fades.outro,source.clip.start+source.fades.outro+source.fades.fadeOut,shape,false)); }
                composite.terms.push_back(std::move(clip));
            }
        }
        // Clip boundaries and authored cuts can jump without a guard sample.
        if (std::abs(composite.value(composite.position(1))-gain(result.points[i+1].value))>1.e-7) composite.endpointStep=true;
        composite.first=composite.value(composite.position(0)); composite.last=composite.value(composite.position(1));
        if (!composite.valid()) return juce::Result::fail("This compound curve exceeds the supported formula depth. Undo or simplify an earlier compound treatment first. The curves were kept.");
        result.points[i].shape=std::move(composite);
    }

    return juce::Result::ok();
}
juce::Result materialise(Project& project, const Links& links, unsigned targets, const juce::String& standalone = {})
{
    Project candidate = project;
    for (auto& clip : candidate.clips) for (int target = 0; target < trackAutomationCount; ++target) if (targets & (1u << target))
    {
        if (links.empty() && clip.id != standalone) continue;
        const auto* original = project.clipFor(clip.id); const auto source = nodeSource(project,*original,(AutomationTarget) target,links);
        if (!source.generated) continue;
        AutomationCurve curve; const auto valid = composeNodes(project,source,curve); if (valid.failed()) return valid;
        candidate.effectSettings(clip).automation[(size_t) target] = std::move(curve);
        if (target == 0)
        {
            if (source.fades.fadeIn > 0) { clip.fadeIn = 0; clip.fadeInAnchor = clip.sourceElapsed(source.fades.intro); }
            if (source.fades.fadeOut > 0) { clip.fadeOut = 0; clip.fadeOutAnchor = clip.sourceElapsed(source.fades.outro+source.fades.fadeOut); }
        }
    }
    for (auto& link : candidate.transitionLinks) if (links.count(link.incoming))
    {
        for (int band = 0; band < 4; ++band) if ((targets & (1u << band)) && link.bands[(size_t) band] && link.bands[(size_t) band]->generated())
        { link.bands[(size_t) band].reset(); if (band == 1) link.bassSwap.reset(); }
        if (targets & (1u << (int) AutomationTarget::cutoff)) link.filter = {};
    }
    const auto checked = candidate.validateTransitionLinks(); if (checked.failed()) return checked;
    const auto protectedResult = project.validateTransitionLocks(candidate); if (protectedResult.failed()) return protectedResult;
    project = std::move(candidate); return juce::Result::ok();
}
}
juce::Result materialisePresetNodes(Project& project, const juce::String& incoming, unsigned targets)
{
    if (!project.transitionFor(incoming)) return juce::Result::fail("Select a ready transition before creating its preset nodes.");
    return materialise(project,Links{incoming},targets);
}
juce::Result editablePresetNodes(const Project& project, const AutomationAddress& address, AutomationCurve& result)
{
    const auto* clip = project.clipFor(address.clipId);
    if (!clip || (int) address.target >= trackAutomationCount) return juce::Result::fail("No source curve at this address.");
    return composeNodes(project,nodeSource(project,*clip,address.target,linksFor(project,clip->id)),result);
}
juce::Result materialiseEditablePresetNodes(Project& project, const AutomationAddress& address)
{
    if (address.stem >= 0) return juce::Result::ok();
    const auto* clip = project.clipFor(address.clipId);
    if (!clip || (int) address.target >= trackAutomationCount) return juce::Result::ok();
    return materialise(project,linksFor(project,clip->id),1u << (int) address.target,clip->id);
}
juce::Result capturePresetNodes(const Project& project, const juce::String& incoming, TransitionStyle::Nodes& result)
{
    const auto* link = project.transitionFor(incoming); if (!link) return juce::Result::fail("Select a transition to capture its nodes.");
    const auto frame = project.transitionRange(*link); if (frame.getLength() <= 1.e-6) return juce::Result::fail("Select a non-zero transition frame.");
    TransitionStyle::Nodes captured;
    for (int side = 0; side < 2; ++side)
    {
        const auto* clip = project.clipFor(side == 0 ? link->outgoing : incoming); if (!clip) return juce::Result::fail("A preset source was removed.");
        for (int target = 0; target < trackAutomationCount; ++target)
        {
            const AutomationAddress address{clip->track,(AutomationTarget) target,{},{},clip->id}; AutomationCurve view;
            const auto valid = editablePresetNodes(project,address,view); if (valid.failed()) return valid;
            const double begin = project.automationTime(address,frame.getStart()), end = project.automationTime(address,frame.getEnd());
            auto curve = view.sliced(begin,end,project.manualValue(address));
            for (auto& point : curve.points)
            {
                if (point.shape) point.shape->clock.points.clear(); // Portable templates use project-time fractions.
                point.time = juce::jlimit(0.,1.,(project.automationSessionTime(address,begin + point.time) - frame.getStart()) / frame.getLength());
            }
            // Source guard points one ULP apart can map to the same project
            // instant. Keep the later point's right-hand value/interpolation,
            // matching AutomationCurve::interpolatedValue at that instant.
            std::vector<AutomationPoint> distinct;
            for (const auto& point : curve.points)
            {
                if (!distinct.empty() && point.time == distinct.back().time) distinct.back() = point;
                else distinct.push_back(point);
            }
            curve.points = std::move(distinct);
            captured.curves[(size_t) side][(size_t) target] = std::move(curve);
        }
    }
    const auto valid = captured.validate(); if (valid.failed()) return valid; result = std::move(captured); return juce::Result::ok();
}
juce::Result applyOwnedPresetNodes(Project& project, const juce::String& incoming, const TransitionStyle::Nodes& nodes, const juce::String& name)
{
    const auto valid = nodes.validate(); if (valid.failed()) return valid;
    const auto* link = project.transitionFor(incoming);
    if (!link) return juce::Result::fail("Select an aligned pair first.");
    const auto frame = project.transitionRange(*link);
    auto* out = project.clipFor(link->outgoing);
    auto* in = project.clipFor(incoming);
    if (out->effects && out->effects == in->effects)
        return juce::Result::fail("This pair shares one automation curve. Select two independent tracks.");
    const auto owner = juce::Uuid().toString();
    bool inserted = false;
    for (size_t side = 0; side < 2; ++side)
    {
        auto& clip = side == 0 ? *out : *in;
        for (int target = 0; target < trackAutomationCount; ++target)
        {
            const auto& shape = nodes.curves[side][(size_t)target];
            if (!shape.active()) continue;
            const AutomationAddress address{clip.track,(AutomationTarget)target,{},{},clip.id};
            auto& curve = project.effectSettings(clip).automation[(size_t)target];
            if (!curve.enabled && !curve.points.empty())
                return juce::Result::fail("Enable the existing " + juce::String(automationName(address.target)) + " curve before applying this preset.");
            const auto sourceTime = [&](double time) { return project.automationTime(address,time); };
            const double begin = sourceTime(frame.getStart()), end = sourceTime(frame.getEnd());
            const float manual = project.manualValue(address);
            std::vector<AutomationPoint> placed;
            for (auto point : shape.points)
            {
                point.time = sourceTime(frame.getStart() + point.time * frame.getLength());
                point.presetOwner = owner;

                placed.push_back(std::move(point));
            }
            for (size_t i = 0; i+1 < placed.size(); ++i) if (placed[i].shape)
                shapeClock(*placed[i].shape,project,address,project.automationSessionTime(address,placed[i].time),project.automationSessionTime(address,placed[i+1].time));
            constexpr double guard = 1.e-6;
            {
                const float value = curve.valueAt(begin - guard,manual);
                if (curve.active() || value != placed.front().value)
                    placed.insert(placed.begin(),{begin-guard,value,1,false,value < -60.f,owner});
            }
            {
                const float value = curve.valueAt(end + guard,manual);
                if (curve.active() || value != placed.back().value)
                {
                    placed.back().bend = 1;
                    placed.push_back({end+guard,value,0,false,value < -60.f,owner});
                }
            }
            for (const auto& point : placed)
                if (std::any_of(curve.points.begin(),curve.points.end(),[&](const auto& existing)
                    { return std::abs(existing.time-point.time) < 1.e-9; }))
                    return juce::Result::fail("A preset node meets an existing " + juce::String(automationName(address.target))
                        + " node. Move the existing node or adjust the join or length first.");
            if (curve.points.size() + placed.size() > 4096)
                return juce::Result::fail("This preset would exceed the automation node limit.");
            curve.points.insert(curve.points.end(),placed.begin(),placed.end());
            std::sort(curve.points.begin(),curve.points.end(),[](const auto& a,const auto& b) { return a.time < b.time; });
            if (std::adjacent_find(curve.points.begin(),curve.points.end(),[](const auto& a,const auto& b) { return a.time >= b.time; }) != curve.points.end())
                return juce::Result::fail("Preset nodes are too close at this source clock.");
            curve.enabled = true;
            inserted = true;
        }
    }
    if (!inserted) return juce::Result::ok();
    auto& edited = project.transitionLinks[(size_t)(link-project.transitionLinks.data())];
    edited.presetOwner = owner;
    edited.presetName = name;
    edited.alignmentOnly = false;
    return juce::Result::ok();
}
juce::Result applyPresetNodes(Project& project, const juce::String& incoming, const TransitionStyle::Nodes& nodes, unsigned targets)
{
    auto valid = nodes.validate(); if (valid.failed()) return valid;
    const auto* original = project.transitionFor(incoming); if (!original) return juce::Result::fail("Select a transition to apply preset nodes.");
    if (project.hasPendingPlacement() || project.clipTransitionLocked(original->outgoing) || project.clipTransitionLocked(incoming))
        return juce::Result::fail("Wait for preparation and unlock both segments before applying preset nodes.");
    TransitionStyle::Nodes current;
    const auto* originalOut = project.clipFor(original->outgoing); const auto* originalIn = project.clipFor(incoming);
    bool procedural = (targets & 1) && originalOut && originalIn && (originalOut->fadeOut > 0 || originalIn->fadeIn > 0);
    procedural |= (targets & 2) && original->bassSwap.has_value();
    procedural |= (targets & (1u << (int) AutomationTarget::cutoff)) && original->filter.active();
    for (int band = 0; band < 4; ++band) procedural |= (targets & (1u << band)) && original->bands[(size_t) band] && original->bands[(size_t) band]->generated();
    if (!procedural && capturePresetNodes(project,incoming,current).wasOk())
    {
        for (int side = 0; side < 2; ++side) for (int target = 0; target < trackAutomationCount; ++target)
            if (!(targets & (1u << target))) current.curves[(size_t) side][(size_t) target] = nodes.curves[(size_t) side][(size_t) target];
        if (current == nodes) return juce::Result::ok();
    }
    const auto frame = project.transitionRange(*original); if (frame.getLength() <= 1.e-6) return juce::Result::fail("Select a non-zero transition frame.");
    const auto* outSource = project.clipFor(original->outgoing); const auto* inSource = project.clipFor(incoming);
    if (!outSource || !inSource || frame.getStart() < std::max(outSource->start,inSource->start) - 1.e-7
        || frame.getEnd() > std::min(outSource->end(),inSource->end()) + 1.e-7)
        return juce::Result::fail("Keep the preset frame inside both source segments.");
    Project candidate = project;
    // Applying a preset replaces this frame. It must not bake the previous
    // treatment and then add the new one on top of it.
    for (auto& link : candidate.transitionLinks) if (link.incoming == incoming)
    {
        if (targets & 2) candidate.clearTransitionBass(link);
        for (int band = 0; band < 4; ++band) if (targets & (1u << band)) link.bands[(size_t) band].reset();
        if (targets & (1u << (int) AutomationTarget::cutoff)) link.filter = {};
    }
    const auto outgoing = original->outgoing;
    auto* out = candidate.clipFor(outgoing); auto* in = candidate.clipFor(incoming);
    if (targets & 1)
    {
        out->fadeOut = in->fadeIn = 0;
        out->fadeOutAnchor.reset();
        in->fadeInAnchor.reset();
    }
    for (int side = 0; side < 2; ++side)
    {
        auto* clip = candidate.clipFor(side == 0 ? outgoing : incoming);
        for (int target = 0; target < trackAutomationCount; ++target)
        {
            if (!(targets & (1u << target))) continue;
            const AutomationAddress address{clip->track,(AutomationTarget) target,{},{},clip->id};
            auto& curve = candidate.effectSettings(*clip).automation[(size_t) target]; const auto before = curve;
            const auto& source = nodes.curves[(size_t) side][(size_t) target];
            if (!source.enabled && source.points.empty() && before.points.empty()) continue;
            // The validated frame is inside the source. Inverting a descending
            // tempo ramp at its exact start can round a few ULPs below zero;
            // generated nodes must still use the valid source boundary.
            const auto sourceTime = [&](double time) { return std::max(0.,candidate.automationTime(address,time)); };
            const double begin = sourceTime(frame.getStart()), end = sourceTime(frame.getEnd());
            const float manual = candidate.manualValue(address);
            const bool exterior = begin > 0 || clip->sourceElapsed(clip->length) > end
                || (!before.points.empty() && before.points.back().time > end);
            if (exterior && !before.points.empty() && !source.points.empty() && before.enabled != source.enabled)
                return juce::Result::fail("Enable or clear the existing curve before replacing a frame with a different Read state. Exterior points were kept.");
            // Slice the exterior using the ordinary bend-preserving operation.
            // Leave a sub-sample guard that also survives the session JSON's
            // 15 decimal places. Adjacent binary doubles below one second can
            // serialize to the same time, invalidating an otherwise good mix.
            curve.points.clear();
            if (begin > 0 && (!before.points.empty() || source.points.empty() || source.points.front().value != manual))
            {
                const double guard = std::min(std::nextafter(begin,0.),std::max(0.,begin - 1.e-12));
                if (!before.points.empty()) curve.points = before.sliced(0,guard,manual).points;
                if (curve.points.empty()) curve.points.push_back({guard,manual,1});
                else curve.points.back().bend = 1;
            }
            if (!source.points.empty())
                for (const auto& point : source.points)
                {
                    const double time = sourceTime(frame.getStart() + point.time * frame.getLength());
                    // Bulk insertion must retain the template's bends. The
                    // single-node editor splits a preceding bend when adding
                    // a point; using it here would turn a step into a ramp.
                    auto placed = point; placed.time = time;
                    curve.points.push_back(std::move(placed));
                }
            else if (!before.points.empty()) { curve.points.push_back({begin,manual,0}); curve.points.push_back({end,manual,0}); }
            for (size_t i = 0; i+1 < curve.points.size(); ++i) if (curve.points[i].time >= begin && curve.points[i+1].time <= end && curve.points[i].shape)
                shapeClock(*curve.points[i].shape,candidate,address,candidate.automationSessionTime(address,curve.points[i].time),candidate.automationSessionTime(address,curve.points[i+1].time));
            if ((clip->sourceElapsed(clip->length) > end || (!before.points.empty() && before.points.back().time > end))
                && (!before.points.empty() || source.points.empty() || source.points.back().value != manual))
            {
                const double guard = std::max(std::nextafter(end,INFINITY),end + 1.e-12);
                if (!curve.points.empty()) curve.points.back().bend = 1;
                const double last = std::max(clip->sourceElapsed(clip->length),before.points.empty() ? guard : before.points.back().time);
                if (!before.points.empty() && last > guard)
                {
                    auto right = before.sliced(guard,last,manual);
                    for (auto point : right.points) { point.time += guard; curve.points.push_back(point); }
                }
                else curve.points.push_back({guard,manual,0});
            }
            std::sort(curve.points.begin(),curve.points.end(),[](const auto& a,const auto& b) { return a.time < b.time; });
            for (size_t i = 1; i < curve.points.size(); ++i) if (curve.points[i].time <= curve.points[i-1].time)
                return juce::Result::fail("Preset nodes are too close at this source clock. The arrangement was kept.");
            curve.enabled = source.enabled || (before.enabled && !curve.points.empty());
        }
    }
    valid = candidate.validateTransitionLinks(); if (valid.failed()) return valid;
    valid = project.validateTransitionLocks(candidate); if (valid.failed()) return valid;
    project = std::move(candidate); return juce::Result::ok();
}
namespace
{
juce::Result placeTargetNodes(Project& project,const juce::String& incoming,int target,
                             const TransitionBandSettings* band,const TransitionFilterSettings* filter)
{
    const auto* link = project.transitionFor(incoming); if (!link) return juce::Result::fail("Select a ready transition.");
    auto shape = project;
    shape.transitionLinks = {*link}; shape.transitionLinks.front().locked = false;
    shape.transitionLinks.front().bands = {}; shape.transitionLinks.front().filter = {}; shape.transitionLinks.front().bassSwap.reset();
    for (auto& clip : shape.clips) { shape.effectSettings(clip).automation = {}; clip.controls.automation = {}; clip.fadeIn = clip.fadeOut = 0; }
    const auto checked = band ? shape.setTransitionBand(incoming,target,*band) : shape.setTransitionFilter(incoming,*filter);
    if (checked.failed()) return checked;
    const auto compiled = materialisePresetNodes(shape,incoming,1u << target); if (compiled.failed()) return compiled;
    TransitionStyle::Nodes nodes; const auto captured = capturePresetNodes(shape,incoming,nodes); if (captured.failed()) return captured;
    return applyPresetNodes(project,incoming,nodes,1u << target);
}
}
juce::Result placeTransitionBandNodes(Project& project,const juce::String& incoming,int band,const TransitionBandSettings& setting)
{
    if (band < 0 || band > 3) return juce::Result::fail("Unknown preset band.");
    if (setting.mode == TransitionBandMode::manual) return materialisePresetNodes(project,incoming,1u << band);
    return placeTargetNodes(project,incoming,band,&setting,nullptr);
}
juce::Result placeTransitionFilterNodes(Project& project,const juce::String& incoming,const TransitionFilterSettings& setting)
{ return placeTargetNodes(project,incoming,(int) AutomationTarget::cutoff,nullptr,&setting); }
juce::Result TransitionStyle::Nodes::validate() const
{
    for (const auto& side : curves) for (int target = 0; target < trackAutomationCount; ++target)
    {
        const auto& curve = side[(size_t) target]; const auto range = automationRange((AutomationTarget) target); double previous = -1;
        if (curve.points.size() > 4096) return juce::Result::fail("The preset has too many automation nodes.");
        for (const auto& point : curve.points)
        {
            if (!std::isfinite(point.time) || point.time < 0 || point.time > 1 || point.time <= previous
                || !std::isfinite(point.value) || (point.value < (point.gainSpace && target < 4 ? -160 : range.getStart()) || point.value > range.getEnd()) || (point.gainSpace && target >= 4)
                || !std::isfinite(point.bend) || std::abs(point.bend) > 1
                || (point.shape && (!point.shape->valid() || (point.shape->kind>4 || ((point.shape->kind < 2 || point.shape->kind==4) ? !point.gainSpace : target != (int)AutomationTarget::cutoff)))))
                return juce::Result::fail("Invalid ordinary preset node time, value or bend.");
            previous = point.time;
        }
    }
    return juce::Result::ok();
}
bool TransitionStyle::Nodes::operator==(const Nodes& b) const
{
    for (size_t side = 0; side < curves.size(); ++side) for (size_t target = 0; target < curves[side].size(); ++target)
    {
        const auto& a = curves[side][target]; const auto& other = b.curves[side][target];
        if (a.enabled != other.enabled || a.points.size() != other.points.size()) return false;
        for (size_t i = 0; i < a.points.size(); ++i)
            if (std::abs(a.points[i].time - other.points[i].time) > 1.e-12 || a.points[i].value != other.points[i].value || std::abs(a.points[i].bend - other.points[i].bend) > 1.e-12 || a.points[i].exponential != other.points[i].exponential || a.points[i].gainSpace != other.points[i].gainSpace || a.points[i].shape != other.points[i].shape) return false;
    }
    return true;
}
juce::var presetNodesToJson(const TransitionStyle::Nodes& nodes)
{
    juce::Array<juce::var> sides;
    for (const auto& side : nodes.curves)
    {
        juce::Array<juce::var> curves;
        for (const auto& curve : side)
        {
            auto* object = new juce::DynamicObject(); object->setProperty("enabled",curve.enabled); juce::Array<juce::var> points;
            for (const auto& point : curve.points)
            {
                auto* node = new juce::DynamicObject(); node->setProperty("time",point.time); node->setProperty("value",point.value); node->setProperty("bend",point.bend); if (point.exponential) node->setProperty("exponential",true); if (point.gainSpace) node->setProperty("gainSpace",true); if (point.shape) node->setProperty("shape",automationShapeToJson(*point.shape)); points.add(juce::var(node));
            }
            object->setProperty("points",points); curves.add(juce::var(object));
        }
        sides.add(curves);
    }
    return sides;
}
juce::Result readPresetNodes(const juce::var& document, TransitionStyle::Nodes& result)
{
    auto fail = [] { return juce::Result::fail("Invalid preset automation nodes."); };
    auto numeric = [](const juce::var& v) { return v.isInt() || v.isInt64() || v.isDouble(); };
    if (!document.isArray() || document.size() != 2) return fail();
    TransitionStyle::Nodes candidate;
    for (int side = 0; side < 2; ++side)
    {
        const auto& curves = document[side]; if (!curves.isArray() || curves.size() != trackAutomationCount) return fail();
        for (int target = 0; target < trackAutomationCount; ++target)
        {
            const auto& saved = curves[target]; auto& curve = candidate.curves[(size_t) side][(size_t) target];
            if (!saved.isObject() || !saved["enabled"].isBool() || !saved["points"].isArray() || saved["points"].size() > 4096) return fail();
            curve.enabled = (bool) saved["enabled"];
            for (const auto& point : *saved["points"].getArray())
            {
                if (!point.isObject() || !numeric(point["time"]) || !numeric(point["value"]) || !numeric(point["bend"]) || (point.hasProperty("exponential") && !point["exponential"].isBool()) || (point.hasProperty("gainSpace") && !point["gainSpace"].isBool())) return fail();
                curve.points.push_back({(double) point["time"],(float) point["value"],(double) point["bend"],(bool) point["exponential"],(bool) point["gainSpace"]});
                if (point.hasProperty("shape") && !readAutomationShape(point["shape"],curve.points.back().shape)) return fail();
            }
        }
    }
    const auto valid = candidate.validate(); if (valid.failed()) return valid; result = std::move(candidate); return juce::Result::ok();
}
}
