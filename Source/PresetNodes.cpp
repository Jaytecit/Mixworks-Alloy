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
juce::Result composeNodes(const Project& p, const NodeSource& source, AutomationCurve& result)
{
    const auto* saved = p.curve(source.address); result = saved ? *saved : AutomationCurve{};
    if (!source.generated) return juce::Result::ok();
    if (saved && !saved->enabled && !saved->points.empty())
        return juce::Result::fail("Enable or clear the existing disabled curve before converting its preset to nodes. Its saved points were kept.");
    const int band = (int) source.address.target; const bool filter = source.address.target == AutomationTarget::cutoff;
    if (filter && saved && saved->active())
        return juce::Result::fail("This legacy filter also has manual cutoff automation. Keep it or clear that curve before converting the filter preset.");
    const auto range = automationRange(source.address.target); const float manual = p.manualValue(source.address);
    const auto session = [&](double x) { return p.automationSessionTime(source.address,x); };
    const auto curveTime = [&](double t) { return p.automationTime(source.address,t); };
    // Fresh ordinary volume fades are linear in audible gain, not in dB.
    // Keep that native interpolation instead of baking its logarithm into
    // dozens of short dB segments (which also looked like hard-cut handles).
    if (source.address.target == AutomationTarget::volume && (!saved || saved->points.empty())
        && (source.fades.fadeIn > 0 || source.fades.fadeOut > 0)
        && source.playback.pairs[0][0].strength == 0 && source.playback.pairs[1][0].strength == 0)
    {
        std::set<double> times {0.,curveTime(source.clip.end())};
        for (double local : {source.fades.intro,source.fades.intro+source.fades.fadeIn,source.fades.outro,source.fades.outro+source.fades.fadeOut})
            if (local > 0 && local < source.clip.length) times.insert(curveTime(source.clip.start+local));
        if (source.fades.fadeIn > 0 && source.fades.fadeOut > 0)
        {
            const double crossing = (source.fades.fadeIn*(source.fades.outro+source.fades.fadeOut)
                + source.fades.fadeOut*source.fades.intro)/(source.fades.fadeIn+source.fades.fadeOut);
            if (crossing > 0 && crossing < source.clip.length) times.insert(curveTime(source.clip.start+crossing));
        }
        if (source.clip.tempoMap) for (const auto& point : source.clip.tempoMap->points)
            if (point.time > source.clip.start && point.time < source.clip.end()) times.insert(curveTime(point.time));
        const auto gain = [&](double x)
        {
            // Evaluate in clip-local time. The source/session round trip at a
            // late clip's exact start can land just before it, inventing a
            // silence-to-unity discontinuity that subdivision cannot fit.
            double local = source.clip.localTimeForSource(x);
            if (x == 0 || std::abs(local) < 1.e-9) local = 0;
            if (std::abs(local-source.clip.length) < 1.e-9) local = std::nextafter(source.clip.length,0.);
            return (double) source.fades.gainAt(local) * juce::Decibels::decibelsToGain((double) manual);
        };
        const auto db = [](double g) { return (float) juce::Decibels::gainToDecibels(g,-160.); };
        result = {}; result.enabled = true;
        result.points.push_back({*times.begin(),db(gain(*times.begin())),0,false,true});
        bool valid = true;
        std::function<void(double,double,double,double,int)> span = [&](double a,double av,double b,double bv,int depth)
        {
            const auto deviation = [&](double bend, bool exponential)
            {
                double error = 0;
                for (int i = 1; i < 16; ++i) error = std::max(error,std::abs(gain(a+(b-a)*i/16.)
                    -(av+(bv-av)*automationProgress(i/16.,bend,exponential))));
                return error;
            };
            double bend = 0; bool exponential = false; double error = deviation(0,false);
            // A tempo-shaped fade usually fits one native bent gain span.
            // Fit it before subdividing into many short straight segments.
            if (error > 2.e-6 && av != bv)
            {
                const double fraction = (gain((a+b)*.5)-av)/(bv-av);
                if (fraction > 0 && fraction < 1) for (bool exp : {false,true})
                {
                    const double fitted = juce::jlimit(-.999999,.999999,std::log((1-fraction)/fraction)/(exp ? 4 : 8));
                    const double candidateError = deviation(fitted,exp);
                    if (candidateError < error) { error = candidateError; bend = fitted; exponential = exp; }
                }
            }
            if (error > 2.e-6 && depth < 24 && result.points.size() < 4095)
            { const double mid = (a+b)*.5, mv = gain(mid); span(a,av,mid,mv,depth+1); span(mid,mv,b,bv,depth+1); return; }
            if (error > 2.e-6 || result.points.size() >= 4096) { valid = false; return; }
            result.points.back().bend = bend; result.points.back().exponential = exponential;
            result.points.push_back({b,db(bv),0,false,true});
        };
        for (auto at = times.begin(); at != times.end(); ++at)
            if (const auto next = std::next(at); next != times.end()) span(*at,gain(*at),*next,gain(*next),0);
        if (!valid) return juce::Result::fail("Cannot represent this volume fade accurately. The arrangement was kept.");
        // Endpoint holding already provides the flat portions outside a fade.
        while (result.points.size() > 1 && result.points[0].value == result.points[1].value) result.points.erase(result.points.begin());
        while (result.points.size() > 1 && result.points.back().value == result.points[result.points.size()-2].value) result.points.pop_back();
        return juce::Result::ok();
    }
    auto value = [&](double x)
    {
        const double mapped = session(x);
        double t = std::abs(mapped-source.clip.end()) < 1.e-9 ? std::nextafter(source.clip.end(),source.clip.start)
            : std::abs(mapped-source.clip.start) < 1.e-9 ? source.clip.start : mapped;
        if (band >= 0 && band < 4) for (const auto& pair : source.playback.pairs)
        {
            const auto& envelope = pair[(size_t) band];
            const double edge = envelope.instant ? envelope.begin : envelope.end;
            if (envelope.strength > 0 && (envelope.instant || envelope.bend >= 1) && std::abs(t-edge) < 1.e-9) t = edge;
        }
        const float base = saved ? saved->valueAt(x,manual) : manual;
        if (t < source.clip.start || t > source.clip.end()) return base;
        if (filter)
        {
            const TransitionFilterEnvelope* active = nullptr;
            for (const auto& envelope : source.playback.filters) if (envelope.setting.active() && envelope.amountAt(t) > 0)
            { if (active) return std::numeric_limits<float>::quiet_NaN(); active = &envelope; }
            if (!active) return base;
            const double amount = active->amountAt(t), upper = std::min(20000.,source.clip.media->sampleRate * .45), ratio = upper / 20.;
            const double hz = active->highPass() ? 20 * std::pow(ratio,.8 * amount) : upper * std::pow(ratio,-.8 * amount);
            const float cutoff = active->highPass() ? (float) ((hz - 20) / 19980) : lowPassToCutoff((float) hz);
            // Match the legacy wet entrance continuously from Off, including
            // source rates whose Nyquist limit is below the cutoff range.
            return range.clipValue(cutoff * (float) std::min(1.,amount / .02));
        }
        double gain = source.playback.gainAt(band,t);
        if (source.address.target == AutomationTarget::volume) gain *= source.fades.gainAt(t - source.clip.start);
        return range.clipValue(base + juce::Decibels::gainToDecibels((float) gain,-60.f));
    };
    std::set<double> knots{0.,curveTime(source.clip.end())}; std::set<double> steps,guards;
    if (saved) for (size_t i = 0; i < saved->points.size(); ++i)
    { knots.insert(saved->points[i].time); if (i > 0 && saved->points[i-1].bend >= 1) steps.insert(saved->points[i].time); }
    for (const auto& pair : source.playback.pairs) if (band >= 0 && band < 4)
    {
        const auto& envelope = pair[(size_t) band]; if (envelope.strength <= 0) continue;
        knots.insert(curveTime(envelope.begin)); knots.insert(curveTime(envelope.end));
        if (envelope.instant) steps.insert(curveTime(envelope.begin));
        else if (envelope.bend >= 1) steps.insert(curveTime(envelope.end));
    }
    if (source.address.target == AutomationTarget::volume)
        for (double local : {source.fades.intro,source.fades.intro + source.fades.fadeIn,source.fades.outro,source.fades.outro + source.fades.fadeOut})
            if (local >= 0 && local <= source.clip.length) knots.insert(curveTime(source.clip.start + local));
    if (filter) for (const auto& envelope : source.playback.filters) if (envelope.setting.active())
    {
        knots.insert(curveTime(envelope.begin)); knots.insert(curveTime(envelope.end));
        // The legacy wet entrance changes formula at amount .02. Keep that
        // boundary explicit so a single exponential span covers the real sweep.
        if (envelope.setting.strength > .02)
        {
            const double wet = .02 / envelope.setting.strength;
            knots.insert(curveTime(envelope.begin + (envelope.end - envelope.begin) * (envelope.incoming ? 1 - wet : wet)));
        }
    }
    for (double edge : steps) if (edge > 0)
    { const double guard = std::max(0.,edge - 1 / source.clip.media->sampleRate); knots.insert(guard); guards.insert(guard); }
    result = {}; result.enabled = true; bool valid = true; juce::String diagnostic;
    const float tolerance = filter ? .00001f : .002f;
    std::function<void(double,float,double,float,int)> subdivide = [&](double a,float av,double b,float bv,int depth)
    {
        if (!valid) return;
        if (!std::isfinite(av) || !std::isfinite(bv) || result.points.size() >= 4096) { valid = false; return; }
        std::array<float,15> samples;
        for (size_t n = 0; n < samples.size(); ++n)
        { samples[n] = value(a + (b-a) * (n+1) / 16.); if (!std::isfinite(samples[n])) { valid = false; return; } }
        const auto deviation = [&](double bend, bool exponential)
        {
            float error = 0;
            for (size_t n = 0; n < samples.size(); ++n)
                error = std::max(error,std::abs(samples[n] - (float) (av + (bv-av) * automationProgress((n+1)/16.,bend,exponential))));
            return error;
        };
        double bend = 0; bool exponential = false; float error = deviation(0,false);
        // Fit the existing bend before subdividing. Frequency-domain exponentials
        // also have a native span, rather than being baked into dozens of points.
        if (error > tolerance * .9f && bv != av)
        {
            const double fraction = (samples[7] - (double) av) / (bv-av);
            if (fraction > 0 && fraction < 1)
            {
                const double odds = std::log((1-fraction)/fraction);
                for (bool exp : {false,true})
                {
                    if (exp && !filter) continue;
                    const double trial = juce::jlimit(-1.,std::nextafter(1.,0.),odds / (exp ? 4 : 8));
                    const float nextError = deviation(trial,exp);
                    if (nextError < error) { error = nextError; bend = trial; exponential = exp; }
                }
            }
        }
        if (error > tolerance * .9f && depth < 28 && b-a > 1.e-10)
        { const double mid = (a+b)*.5; const float mv = value(mid); subdivide(a,av,mid,mv,depth+1); subdivide(mid,mv,b,bv,depth+1); return; }
        if (error > tolerance)
        {
            diagnostic = "source " + juce::String(a,12) + ".." + juce::String(b,12) + ", values " + juce::String(av,8) + "/" + juce::String(bv,8) + ", error " + juce::String(error,8);
            valid = false; return;
        }
        result.points.back().bend = bend; result.points.back().exponential = exponential;
        result.points.push_back({b,bv,0});
    };
    if (!knots.empty()) result.points.push_back({*knots.begin(),value(*knots.begin()),0});
    for (auto i = knots.begin(); i != knots.end(); ++i)
    {
        const auto next = std::next(i); if (next == knots.end()) break;
        if (steps.count(*next)) { result.points.back().bend = 1; result.points.push_back({*next,value(*next),0}); }
        else subdivide(*i,value(*i),*next,value(*next),0);
    }
    if (!valid)
    {
        juce::Logger::writeToLog("Preset composition refused: " + source.clip.media->name + " / " + automationName(source.address.target)
            + " / " + juce::String((int) result.points.size()) + " nodes / " + diagnostic);
        return juce::Result::fail("Cannot create " + juce::String(automationName(source.address.target)) + " preset nodes for " + source.clip.media->name
            + ". The arrangement was kept. " + diagnostic);
    }
    // A flat approach needs no extra point one sample before its step. Keeping
    // that redundant guard would prevent dragging the cut earlier normally.
    for (size_t i = 1; i+1 < result.points.size();)
    {
        const auto& point = result.points[i];
        const bool authored = saved && std::any_of(saved->points.begin(),saved->points.end(),[&](const auto& p) { return p.time == point.time; });
        if (guards.count(point.time) && !authored && point.bend == 1 && result.points[i-1].value == point.value)
        { result.points[i-1].bend = 1; result.points.erase(result.points.begin()+(int) i); }
        else ++i;
    }
    // Generated flat padding adds no shape: the ordinary curve already holds
    // its endpoints. Preserve authored points, including deliberately flat ones.
    const auto authored = [&](const AutomationPoint& point)
    { return saved && std::any_of(saved->points.begin(),saved->points.end(),[&](const auto& original) { return original.time == point.time; }); };
    for (size_t i = 1; i+1 < result.points.size();)
        if (!authored(result.points[i]) && result.points[i-1].value == result.points[i].value && result.points[i].value == result.points[i+1].value)
            result.points.erase(result.points.begin()+(int) i);
        else ++i;
    while (result.points.size() > 1 && !authored(result.points.front()) && result.points[0].value == result.points[1].value)
        result.points.erase(result.points.begin());
    while (result.points.size() > 1 && !authored(result.points.back()) && result.points.back().value == result.points[result.points.size()-2].value)
        result.points.pop_back();
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
                point.time = juce::jlimit(0.,1.,(project.automationSessionTime(address,begin + point.time) - frame.getStart()) / frame.getLength());
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
                    curve.points.push_back({time,point.value,point.bend,point.exponential,point.gainSpace});
                }
            else if (!before.points.empty()) { curve.points.push_back({begin,manual,0}); curve.points.push_back({end,manual,0}); }
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
    for (auto& clip : shape.clips) { clip.controls.automation = {}; clip.fadeIn = clip.fadeOut = 0; }
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
                || !std::isfinite(point.bend) || std::abs(point.bend) > 1)
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
            if (std::abs(a.points[i].time - other.points[i].time) > 1.e-12 || a.points[i].value != other.points[i].value || std::abs(a.points[i].bend - other.points[i].bend) > 1.e-12 || a.points[i].exponential != other.points[i].exponential || a.points[i].gainSpace != other.points[i].gainSpace) return false;
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
                auto* node = new juce::DynamicObject(); node->setProperty("time",point.time); node->setProperty("value",point.value); node->setProperty("bend",point.bend); if (point.exponential) node->setProperty("exponential",true); if (point.gainSpace) node->setProperty("gainSpace",true); points.add(juce::var(node));
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
            }
        }
    }
    const auto valid = candidate.validate(); if (valid.failed()) return valid; result = std::move(candidate); return juce::Result::ok();
}
}
