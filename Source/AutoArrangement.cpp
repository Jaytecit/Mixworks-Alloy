#include "AutoArrangement.h"
#include "Beatmatch.h"
#include "EnergySections.h"
#include "PresetNodes.h"
#include <cmath>
#include <limits>
#include <set>

namespace mixworks
{
namespace
{
void ownShortBlend(Project&, Project::TransitionLink&, const juce::String&);
bool shortAutoBlend(AutoTransitionStyle style)
{
    return style == AutoTransitionStyle::balanced || style == AutoTransitionStyle::bold || style == AutoTransitionStyle::varied;
}

// Mean source powers in the outgoing head/tail and incoming head/tail halves
// of the actual placed frame. Treatment selection and soft handover scoring
// consume these same source-clock measurements.
std::optional<std::array<double, 4>> autoTransitionEnergy(const Project& project, const Project::TransitionLink& link)
{
    const auto* outgoing = project.clipFor(link.outgoing); const auto* incoming = project.clipFor(link.incoming);
    if (!outgoing || !incoming || !outgoing->media || !incoming->media) return {};
    const auto range = project.transitionRange(link);
    const double mid = range.getStart() + range.getLength() * .5;
    const auto energy = [&](const Clip& clip, double a, double b)
    { return sourceEnergy(*clip.media, clip.offset + clip.audioSourceElapsed(a - clip.start), clip.offset + clip.audioSourceElapsed(b - clip.start)); };
    const auto outA = energy(*outgoing, range.getStart(), mid), outB = energy(*outgoing, mid, range.getEnd());
    const auto inA = energy(*incoming, range.getStart(), mid), inB = energy(*incoming, mid, range.getEnd());
    if (!outA || !outB || !inA || !inB || std::min(*outA + *outB, *inA + *inB) <= 2e-8) return {};
    return std::array<double, 4> {*outA, *outB, *inA, *inB};
}
std::optional<std::array<double, 4>> autoTransitionActivity(const Project& project, const Project::TransitionLink& link)
{
    const auto* out = project.clipFor(link.outgoing); const auto* in = project.clipFor(link.incoming);
    if (!out || !in || !out->media || !in->media) return {};
    const auto range = project.transitionRange(link); const double mid = (range.getStart() + range.getEnd()) * .5;
    const auto at = [](const Clip& c, double a, double b) { return musicalEnergyAt(c.media->energy,
        c.offset + c.audioSourceElapsed(a - c.start), c.offset + c.audioSourceElapsed(b - c.start)); };
    const auto a = at(*out, range.getStart(), mid), b = at(*out, mid, range.getEnd());
    const auto c = at(*in, range.getStart(), mid), d = at(*in, mid, range.getEnd());
    if (!a || !b || !c || !d) return {};
    return std::array<double, 4>{*a, *b, *c, *d};
}
bool autoCompatible(const Media& a, const Media& b)
{
    return harmonicallyCompatible(a.key(), b.key()) && std::abs(std::log(a.bpm() / b.bpm())) <= std::log(1.08) + 1e-9;
}

std::optional<double> autoBodyLevel(const Clip& clip)
{
    const auto power = sourceEnergy(*clip.media, clip.offset + clip.sourceElapsed(clip.length * .1),
                                    clip.offset + clip.sourceElapsed(clip.length * .9));
    return power && *power > 1.e-8 ? std::optional<double>(10 * std::log10(*power)) : std::nullopt;
}

int finishAutoMix(Project& project, size_t first)
{
    constexpr double targetRmsDb = -18, peakCeilingDb = -6;
    double target = targetRmsDb;
    if (first > 0 && first < project.clips.size())
        if (const auto* join = project.transitionFor(project.clips[first].id))
            if (const auto* tail = project.clipFor(join->outgoing))
                if (const auto body = autoBodyLevel(*tail))
                    target = juce::jlimit(-24., targetRmsDb, *body + tail->gainDb);

    int trimmed = 0;
    std::set<juce::String> generated;
    for (size_t i = first; i < project.clips.size(); ++i)
    {
        auto& clip = project.clips[i];
        double gain = clip.gainDb;
        if (const auto body = autoBodyLevel(clip)) gain = std::min(gain, target - *body);
        // Waveform buckets contain channel sample maxima. Reserve room for
        // summation and for peaks introduced by stretch or EQ.
        const auto& peaks = clip.media->peaks;
        if (!peaks.empty())
        {
            const auto peak = *std::max_element(peaks.begin(), peaks.end());
            if (std::isfinite(peak) && peak > 0) gain = std::min(gain, peakCeilingDb - 20 * std::log10((double) peak));
        }
        if (gain < clip.gainDb)
        {
            clip.gainDb = (float) std::max(-60., gain);
            ++trimmed;
        }
        generated.insert(clip.id);
    }
    for (auto& join : project.transitionLinks)
        if (generated.count(join.incoming)) join.movementLocked = true;
    return trimmed;
}
juce::String curveOwnerAt(const AutomationCurve& curve, double time)
{
    if (curve.points.empty()) return {};
    const auto next=std::upper_bound(curve.points.begin(),curve.points.end(),time,
        [](double t,const auto& p) { return t<p.time; });
    return (next==curve.points.begin() ? *next : *std::prev(next)).presetOwner;
}
// Preset splicing/slicing synthesizes boundary points. Preserve an earlier
// join's ownership when copying its held value, and own new neutral padding.
// Existing unowned exterior points remain authored.
void ownGeneratedPadding(AutomationCurve& curve, const AutomationCurve& before, const juce::String& owner,
                         double begin, double end, float manual)
{
    for (auto& point : curve.points)
    {
        if (point.presetOwner.isNotEmpty()) continue;
        if (point.time>=begin-1.e-9 && point.time<=end+1.e-9) { point.presetOwner=owner; continue; }
        const auto exact=std::find_if(before.points.begin(),before.points.end(),[&](const auto& p) { return p.time==point.time; });
        if (exact!=before.points.end()) { point.presetOwner=exact->presetOwner; continue; }
        const auto inherited=curveOwnerAt(before,point.time);
        if (inherited.isNotEmpty() && point.value==before.interpolatedValue(point.time,manual)) point.presetOwner=inherited;
        else if (!point.shape && point.value==manual) point.presetOwner=owner;
    }
}
// Only fresh arrangements or explicitly selected generated curves may be
// compacted. Never discard authored exterior automation for presentation.
void compactAutoNodes(Project& project, size_t first = 0, std::optional<AutomationTarget> targetOnly = {},
                      const std::set<juce::String>& selected = {})
{
    for (size_t index = first; index < project.clips.size(); ++index)
    {
        auto& clip = project.clips[index];
        if (!selected.empty() && !selected.count(clip.id)) continue;
        // Legacy paired endpoints are owned by their relationship, not freely
        // removable curve padding.
        if (std::any_of(project.transitionLinks.begin(),project.transitionLinks.end(),[&](const auto& link)
            { return link.bassSwap && (link.incoming == clip.id || link.outgoing == clip.id); })) continue;
        const auto playback = project.transitionPlaybackFor(clip);
        const double begin = std::max(0.,clip.effectOffset+clip.sourceElapsed(playback.startCut.value_or(clip.start)-clip.start));
        const double end = clip.effectOffset+clip.sourceElapsed(playback.endCut.value_or(clip.end())-clip.start);
        if (end <= begin) continue;
        auto& controls = project.effectSettings(clip);
        for (int target = 0; target < trackAutomationCount; ++target)
        {
            if (targetOnly && target != (int)*targetOnly) continue;
            auto& curve = controls.automation[(size_t) target]; if (curve.points.empty()) continue;
            const float manual = controls.manualValue((AutomationTarget) target);
            const auto original = curve;
            // Slice just inside hard audible edges. Taking only the inside
            // VALUE can leave an exterior guard's hold/gain-space/shape behind,
            // flattening the fade or making a -160 dB endpoint unsaveable.
            constexpr double edgeTolerance = 1.e-9;
            const double inset=std::min(edgeTolerance,(end-begin)*.25);
            const double insideBegin=begin+inset, insideEnd=end-inset;
            curve=original.sliced(insideBegin,insideEnd,manual);
            for (auto& point : curve.points) point.time+=insideBegin;
            curve.points.front().time=begin; curve.points.back().time=end;
            curve.points.front().presetOwner=curveOwnerAt(original,insideBegin);
            curve.points.back().presetOwner=curveOwnerAt(original,insideEnd);
            auto& points = curve.points;
            for (size_t i = 1; i+1 < points.size();)
            {
                const auto& a = points[i-1]; const auto& b = points[i]; const auto& c = points[i+1];
                const bool flat = !a.shape && a.value == b.value && (b.value == c.value || b.bend >= 1);
                const auto level = [](const AutomationPoint& p, bool gain)
                { return gain ? (p.value <= -160 ? 0. : juce::Decibels::decibelsToGain((double) p.value)) : (double) p.value; };
                const bool straight = !a.shape && !b.shape && a.bend == 0 && b.bend == 0 && a.gainSpace == b.gainSpace
                    && std::abs(level(b,a.gainSpace) - (level(a,a.gainSpace)+(level(c,a.gainSpace)-level(a,a.gainSpace))
                        *(b.time-a.time)/(c.time-a.time))) < 1.e-8;
                if (flat || straight)
                {
                    if (flat) { points[i-1].bend = b.bend; points[i-1].exponential = b.exponential; points[i-1].gainSpace = b.gainSpace; points[i-1].shape = b.shape; }
                    points.erase(points.begin()+(int) i);
                    if (i > 1) --i;
                }
                else ++i;
            }
            while (points.size() > 1 && !points[0].shape && points[0].value == points[1].value) points.erase(points.begin());
            while (points.size() > 1 && !points[points.size()-2].shape && points.back().value == points[points.size()-2].value) points.pop_back();
            if (points.size() == 1 && points.front().value == manual) points.clear();
        }
    }
}
juce::Result styleAutoTransition(Project& project, AutoTransitionStyle choice, const juce::StringArray& history, juce::String& decision)
{
    if (choice == AutoTransitionStyle::current || project.clips.size() < 2) return juce::Result::ok();
    const auto& incoming = project.clips.back();
    const auto* link = project.transitionFor(incoming.id);
    if (!link) return juce::Result::fail("The next blend could not be prepared.");
    const auto range = project.transitionRange(*link);
    const auto clock = project.gridClock();
    const auto firstBeat = clock.beatsAt(range.getStart()), lastBeat = clock.beatsAt(range.getEnd());
    TransitionStyle style;
    if (choice == AutoTransitionStyle::smooth)
    {
        style.volume = {0, 1, 0, 1};
        decision = "Crossfade: gradual full-band blend";
    }
    else if (choice == AutoTransitionStyle::constantPower || shortAutoBlend(choice))
    {
        style.bands[0] = TransitionBandSettings{TransitionBandMode::constantPower, 1, 0, .5};
        decision = shortAutoBlend(choice) ? "Short blend: " + juce::String((lastBeat - firstBeat) / 4, 1) + " bars, curved volume handover"
                                         : "Power blend: curved volume handover";
        if (choice == AutoTransitionStyle::varied)
        {
            if (const auto activity = autoTransitionActivity(project, *link))
            {
                const double change = ((*activity)[2] + (*activity)[3] - (*activity)[0] - (*activity)[1]) * .5;
                const double density = *std::max_element(activity->begin(), activity->end());
                bool recentFilter = false;
                for (int i = std::max(0, history.size() - 2); i < history.size(); ++i)
                    recentFilter |= history[i].contains("filter fade");
                if (density > .5 && std::abs(change) > .1 && !recentFilter)
                    style.filter = {change > 0 ? TransitionFilterMode::lowToHigh : TransitionFilterMode::highToLow, .165};
            }
        }
        else if (choice == AutoTransitionStyle::bold || choice == AutoTransitionStyle::balanced)
            if (const auto power = autoTransitionEnergy(project, *link))
            {
                const double change = 10 * std::log10(((*power)[2] + (*power)[3]) / ((*power)[0] + (*power)[1]));
                const auto shift = [&](size_t side) { return 10 * std::log10(std::max((*power)[side+1],1.e-12) / std::max((*power)[side],1.e-12)); };
                const double outShift = shift(0), inShift = shift(2);
                if (choice == AutoTransitionStyle::balanced && std::min(std::abs(outShift),std::abs(inShift)) >= 4 && outShift * inShift < 0)
                    style.filter = {inShift > 0 ? TransitionFilterMode::lowToHigh : TransitionFilterMode::highToLow, .12};
                else if (choice == AutoTransitionStyle::bold && std::abs(change) >= 1.5)
                    style.filter = {change > 0 ? TransitionFilterMode::lowToHigh : TransitionFilterMode::highToLow, .26};
            }
        if (style.filter.active()) decision += " with filter fade";
    }
    else
    {
        if (lastBeat <= firstBeat + 1.e-7) { decision = "Hard cut: aligned clip edges"; return juce::Result::ok(); }
        const double cut = (clock.beatsAt(alignedSectionCut(project, *link)) - firstBeat) / (lastBeat - firstBeat);
        style.bands[0] = TransitionBandSettings{TransitionBandMode::swap, 1, 0, cut};
        decision = "Hard cut: aligned section or bar boundary";
    }
    const auto applied = project.applyTransitionStyle(incoming.id, style);
    if (applied.wasOk() || !style.filter.active()) return applied;
    style.filter = {};
    decision = decision.upToFirstOccurrenceOf(" with filter fade", false, false);
    return project.applyTransitionStyle(incoming.id, style);
}

// A small, deterministic search over audible source passages. RMS is evidence
// of level continuity, never a claim about vocals or semantic phrase boundaries.
double passageCost(const Clip& out, double outEnd, const Clip& in, double inBegin, double beats,
                   double outGain, double inGain)
{
    const double outSpan = beats * 60 / out.media->bpm(), inSpan = beats * 60 / in.media->bpm();
    double cost = 0, previous = 0; int measured = 0;
    for (int i = 0; i < 8; ++i)
    {
        const double x = (i + .5) / 8.;
        const auto a = sourceEnergy(*out.media, outEnd-outSpan+i*outSpan/8, outEnd-outSpan+(i+1)*outSpan/8);
        const auto b = sourceEnergy(*in.media, inBegin+i*inSpan/8, inBegin+(i+1)*inSpan/8);
        if (!a || !b) continue;
        const double pa = *a * outGain, pb = *b * inGain;
        const double wa = std::pow(std::cos(x*juce::MathConstants<double>::halfPi),2);
        const double level = 10*std::log10(std::max(1.e-12,wa*pa+(1-wa)*pb));
        const double difference = std::abs(10*std::log10(std::max(pa,1.e-12)/std::max(pb,1.e-12)));
        cost += .10*std::max(0.,difference-3) + .30*std::max(0.,-24-level);
        if (measured) cost += .25*std::abs(level-previous);
        previous = level; ++measured;
    }
    return measured ? cost/measured : 0.;
}

// Use the strongest nearby structural evidence and keep the number of full
// placement trials independent of a song's length.
std::vector<EnergySection> handoverSections(const Clip& clip, double span, bool entering)
{
    auto sections = clipEnergySections(clip);
    const double window = 256 * 60 / clip.media->bpm();
    const double edge = clip.offset + (entering ? 0. : span);
    sections.erase(std::remove_if(sections.begin(), sections.end(), [&](const auto& section)
    {
        const double distance = entering ? section.sourceSeconds - edge : edge - section.sourceSeconds;
        return distance < 0 || distance > std::min(span, window);
    }), sections.end());
    std::stable_sort(sections.begin(), sections.end(), [](const auto& a, const auto& b)
    {
        if (a.manual != b.manual) return a.manual;
        return std::abs(a.contrastDb) > std::abs(b.contrastDb);
    });
    if (sections.size() > 4) sections.resize(4);
    return sections;
}

double sectionWeight(const EnergySection& section)
{
    return section.manual ? 1.4 : juce::jlimit(.25, .8, std::abs(section.contrastDb) / 12.);
}

// Do not let the provisional crossfade overwrite authored volume automation.
// Owned points on earlier joins may stay; an unowned held value or segment
// crossing this frame is authored even if its endpoint is outside the frame.
bool authoredInFrame(const Project& original, const Project& placed, const Project::TransitionLink& link, int target)
{
    const auto frame = placed.transitionRange(link);
    for (const auto& id : {link.outgoing,link.incoming})
    {
        const auto* before = original.clipFor(id); const auto* clip = placed.clipFor(id);
        if (!before || !clip) continue;
        const auto& controls = original.effectSettings(*before);
        const auto& curve = controls.automation[(size_t)target];
        if (controls.manualValue((AutomationTarget)target) != 0) return true;
        if (!curve.active()) continue;
        const double a = clip->effectOffset+clip->sourceElapsed(frame.getStart()-clip->start);
        const double b = clip->effectOffset+clip->sourceElapsed(frame.getEnd()-clip->start);
        const auto& first = curve.points.front();
        if (first.time > b && first.presetOwner.isEmpty() && first.value != 0) return true;
        for (size_t i=0; i<curve.points.size(); ++i)
        {
            const auto& p = curve.points[i]; if (p.presetOwner.isNotEmpty()) continue;
            if (p.time >= a-1.e-9 && p.time <= b+1.e-9) return true;
            const double end = i+1<curve.points.size() ? curve.points[i+1].time : INFINITY;
            if (p.time < b && end > a && (p.value != 0 || (i+1<curve.points.size() && curve.points[i+1].value != 0))) return true;
        }
    }
    return false;
}

MatchResult planAutoPair(Project pending, AutoTransitionStyle style, const juce::StringArray& history,
                         bool freezeExisting, bool preserveTail, double& quality, juce::String& decision,
                         bool trimSources,
                         const std::shared_ptr<std::atomic<bool>>& cancelled)
{
    quality = 0; decision.clear();
    const auto& request = pending.clips.back();
    const auto live = freezeExisting ? std::optional<double>(0.) : std::nullopt;
    if (!shortAutoBlend(style) || pending.clips.size()<2)
    {
        auto result = planTempoMap(pending,request.pendingTransitionBeats.value_or(32),request.pendingTransitionPreset.value_or(TransitionPreset::crossfade),request.pendingSectionTransition.value_or(false),128,live);
        if (result.error.isEmpty()) result.error = styleAutoTransition(result.project,style,history,decision).getErrorMessage();
        return result;
    }
    // Match the placement owner's tail: start at the furthest audio end, then
    // follow its established chain. Storage order and nested clip lengths must
    // not make scoring inspect a different outgoing song from actual placement.
    const auto* tail = &*std::max_element(pending.clips.begin(),std::prev(pending.clips.end()),
        [](const auto& a,const auto& b) { return a.end() < b.end(); });
    for (size_t visited=0; visited<pending.clips.size(); ++visited)
    {
        const auto next=std::find_if(pending.transitionLinks.begin(),pending.transitionLinks.end(),
            [&](const auto& link) { return link.outgoing==tail->id; });
        if (next==pending.transitionLinks.end()) break;
        const auto* following=pending.clipFor(next->incoming); if (!following || following->id==request.id) break;
        tail=following;
    }
    const auto& out = *tail; const auto& in = request;
    if (out.effects == in.effects)
    {
        MatchResult failed; failed.error = "Join or unlink split pieces before blending tracks that share one mixer."; return failed;
    }
    const double outSpan = out.sourceElapsed(out.length), inSpan = in.sourceElapsed(in.length);
    const double outBar = 240/out.media->bpm(), inBar = 240/in.media->bpm();
    const auto* preceding = pending.transitionFor(out.id);
    const double protectedEnd = preceding ? pending.transitionRange(*preceding).getEnd() : out.start;
    const double protectedSource = out.sourceElapsed(protectedEnd-out.start);
    std::vector<double> heads{0}, tails{outSpan};
    const bool editable = !out.reversed && !in.reversed && out.loopSpan<=0 && in.loopSpan<=0
        && std::abs(out.audioSlip)<1.e-9 && std::abs(in.audioSlip)<1.e-9;
    const auto outSections = editable ? handoverSections(out,outSpan,false) : std::vector<EnergySection>{};
    const auto inSections = editable ? handoverSections(in,inSpan,true) : std::vector<EnergySection>{};
    if (editable && trimSources && inSpan>=inBar*16)
    {
        for (double bars : {4.,8.}) if (bars*inBar<=inSpan*.20) heads.push_back(bars*inBar);
        const double phase = std::max(0.,in.firstDownbeatTime())*in.rate;
        for (const auto& section : inSections)
        {
            if (!section.manual && section.contrastDb<=0) continue;
            const double cue = section.sourceSeconds-in.offset;
            const double aligned = phase+std::round((cue-phase)/inBar)*inBar;
            if (aligned>1.e-7 && aligned<=std::min(8*inBar,inSpan*.20) && std::abs(cue-aligned)<=inBar/8
                && std::none_of(heads.begin(),heads.end(),[&](double head) { return std::abs(head-aligned)<1.e-7; }))
            { heads.push_back(aligned); break; }
        }
    }
    if (editable && trimSources && !preserveTail && outSpan>=outBar*16)
    {
        // Include an exact source bar edge; a file's end is rarely on a bar.
        const double anchor = out.media->grid().downbeatOrigin;
        const double edge = anchor+std::floor((out.offset+outSpan-anchor)/outBar)*outBar-out.offset;
        for (double bars : {0.,4.,8.})
        {
            const double end = edge-bars*outBar;
            if (end>0 && outSpan-end<=std::min(outSpan*.20,8*outBar)+1.e-7 && end<outSpan-1.e-7) tails.push_back(end);
        }
    }
    const auto gain = [](const Clip& clip)
    {
        const double trim = std::min((double)clip.gainDb,-18.-autoBodyLevel(clip).value_or(-18.));
        return std::pow(10.,trim/10);
    };
    const double outGain = gain(out), inGain = gain(in);
    const bool tonalMatch = autoCompatible(*out.media,*in.media);
    struct Choice { double head,end,beats,cost,evidence; };
    std::vector<Choice> choices;
    for (double head : heads) for (double end : tails)
    {
        const double room = (end-protectedSource)*4/outBar-(preceding ? 4 : 0);
        const double phase = std::max(0.,in.firstDownbeatTime())*in.rate;
        const double lead = phase + std::ceil((head-phase)/inBar-1.e-9)*inBar;
        const double span = (inSpan-lead)*4/inBar-8;
        const double limit = std::min({256.,room,span});
        const double outEnd = out.offset+end, inCue = in.offset+lead;
        const auto consider = [&](double beats,double evidence)
        {
            if (beats<4 || beats>limit+1.e-7 || (beats>32 && !tonalMatch)) return;
            const double cost = passageCost(out,outEnd,in,inCue,beats,outGain,inGain)
                + .035*((outSpan-end)/outBar+head/inBar) + .004*std::max(0.,beats-32.)
                + (beats<32 ? .12 : 0.) - evidence;
            choices.push_back({head,end,beats,cost,evidence});
        };
        consider(std::min(32.,limit),0.);
        consider(16.,0.);
        for (const auto& a : outSections)
        {
            const double remaining = (outEnd-a.sourceSeconds)*4/outBar;
            if (remaining<4) continue;
            // Snap the cue onto the accepted placement grid. The remainder
            // includes any fractional bar at the outgoing file edge.
            consider(remaining,sectionWeight(a)*.65);
            for (const auto& b : inSections)
            {
                const double elapsed = (b.sourceSeconds-inCue)*4/inBar;
                if (elapsed<4) continue;
                const bool complementary = a.contrastDb<0 && b.contrastDb>0;
                consider(remaining+elapsed,sectionWeight(a)+sectionWeight(b)+(complementary ? .3 : 0.));
            }
        }
        for (const auto& section : inSections)
            consider((section.sourceSeconds-inCue)*4/inBar,sectionWeight(section)*.65);
    }
    std::stable_sort(choices.begin(),choices.end(),[](const auto& a,const auto& b) { return a.cost<b.cost; });
    std::set<std::array<double,3>> geometry;
    choices.erase(std::remove_if(choices.begin(),choices.end(),[&](const auto& item)
        { return !geometry.insert({item.head,item.end,item.beats}).second; }),choices.end());
    const auto fallback = std::find_if(choices.begin(),choices.end(),[](const auto& item) { return item.evidence==0; });
    if (choices.size()>6 && fallback!=choices.end() && fallback>=choices.begin()+6)
        choices[5] = *fallback;
    MatchResult best; best.error = "No blend fits without disturbing the preceding join or authored volume automation.";
    double bestCost = INFINITY;
    // Keep a level-only fallback among the six placement trials.
    for (size_t index=0; index<std::min<size_t>(6,choices.size()); ++index)
    {
        if (cancelled && *cancelled) { best.error="Arrangement cancelled. The current mix was kept."; return best; }
        const auto& choice = choices[index]; auto candidate = pending;
        auto* outgoing = candidate.clipFor(out.id);
        if (choice.end<outSpan-1.e-7) *outgoing = outgoing->slice(0,outgoing->localTimeForSource(choice.end));
        auto& incoming = candidate.clips.back();
        if (choice.head>0) incoming = incoming.slice(incoming.localTimeForSource(choice.head),incoming.length-incoming.localTimeForSource(choice.head));
        incoming.start=0; incoming.pendingPlacement=PendingPlacement::append; incoming.pendingAutoTransition=true;
        // Placement overlaps file edges; treatment starts at the incoming
        // musical cue. Include that lead-in so an eight-bar choice gets eight
        // audible bars when the outgoing end is itself on a bar boundary.
        const double placementBeats=choice.beats+std::max(0.,incoming.firstDownbeatTime())*incoming.media->bpm()/60;
        incoming.pendingTransitionBeats=placementBeats; incoming.pendingTransitionPreset=TransitionPreset::crossfade; incoming.pendingSectionTransition=false;
        auto planned = planTempoMap(candidate,placementBeats,TransitionPreset::crossfade,false,128,live);
        if (planned.error.isNotEmpty()) continue;
        const auto* link = planned.project.transitionFor(in.id); if (!link) continue;
        const auto frame = planned.project.transitionRange(*link);
        const auto* placedOut = planned.project.clipFor(out.id); const auto* placedIn = planned.project.clipFor(in.id);
        const auto clock = planned.project.gridClock();
        const double lead = preceding ? 4. : 0.;
        const auto* placedPrevious=planned.project.transitionFor(out.id);
        const double earlierEnd=placedPrevious ? planned.project.transitionRange(*placedPrevious).getEnd() : placedOut->start;
        if (clock.beatsAt(frame.getStart()) < clock.beatsAt(earlierEnd)+lead-1.e-7
            || authoredInFrame(pending,planned.project,*link,0)) continue;
        const double actualBeats = clock.beatsAt(frame.getEnd())-clock.beatsAt(frame.getStart());
        if (actualBeats>256.+1.e-7 || (choice.evidence>0 && std::abs(actualBeats-choice.beats)>1.01)) continue;
        const double actual = passageCost(*placedOut,placedOut->offset+placedOut->audioSourceElapsed(frame.getEnd()-placedOut->start),
            *placedIn,placedIn->offset+placedIn->audioSourceElapsed(frame.getStart()-placedIn->start),actualBeats,outGain,inGain)
            + .035*((outSpan-choice.end)/outBar+choice.head/inBar) + .004*std::max(0.,actualBeats-32.)
            + (choice.beats<32 ? .12 : 0.) - choice.evidence;
        if (actual>=bestCost) continue;
        juce::String reason; const auto applied=styleAutoTransition(planned.project,style,history,reason);
        if (applied.failed()) continue;
        // The general preset editor replaces all parameters in its frame.
        // Automatic placement owns only volume here, plus an optional filter;
        // keep every other source curve exact for the measured EQ pass below.
        const bool keepFilter=reason.contains("filter fade") && !authoredInFrame(pending,planned.project,*planned.project.transitionFor(in.id),(int)AutomationTarget::cutoff);
        for (const auto& id : {out.id,in.id})
        {
            const auto& original=pending.effectSettings(*pending.clipFor(id));
            auto& controls=planned.project.effectSettings(*planned.project.clipFor(id));
            for (int target=1; target<trackAutomationCount; ++target)
                if (target!=(int)AutomationTarget::cutoff || !keepFilter) controls.automation[(size_t)target]=original.automation[(size_t)target];
        }
        if (!keepFilter) reason=reason.replace(" with filter fade","");
        if (choice.evidence>0)
        {
            reason=reason.replace("Short blend:","Section blend:");
            reason += "; section-guided";
        }
        if (choice.head>0 || outSpan-choice.end>1.e-7) reason += "; entry/exit adjusted";
        auto& owned=*std::find_if(planned.project.transitionLinks.begin(),planned.project.transitionLinks.end(),
            [&](const auto& item) { return item.incoming==in.id; });
        // The chosen musical IN/OUT also bounds audible playback. Otherwise
        // neutral exterior automation can expose a short, unfaded file lead.
        owned.muteOutside=true;
        ownShortBlend(planned.project,owned,reason);
        // Preset replacement can synthesize neutral exterior padding. Attribute
        // new points now, before planning the next pair, so that padding cannot
        // be mistaken for authored volume at a short middle song's other end.
        for (const auto& id : {out.id,in.id})
        {
            const auto& original=pending.effectSettings(*pending.clipFor(id)).automation[0];
            const auto& clip=*planned.project.clipFor(id);
            auto& controls=planned.project.effectSettings(clip);
            const double begin=clip.effectOffset+clip.sourceElapsed(frame.getStart()-clip.start);
            const double end=clip.effectOffset+clip.sourceElapsed(frame.getEnd()-clip.start);
            ownGeneratedPadding(controls.automation[0],original,owned.presetOwner,begin,end,controls.gainDb);
        }
        bestCost=actual; best=std::move(planned); decision=reason;
    }
    if (best.error.isEmpty()) quality=std::max(0.,bestCost);
    return best;
}
}
juce::String normalizedAutoGenre(const juce::String& label)
{
    juce::String result;
    for (auto c : label.toLowerCase())
    {
        if (juce::CharacterFunctions::isLetterOrDigit(c)) result += juce::String::charToString(c);
        else if (c == '&') result += " and ";
        else result += ' ';
    }
    auto words = juce::StringArray::fromTokens(result, " ", ""); words.removeEmptyStrings(); result = words.joinIntoString(" ");
    if (result == "hiphop") return "hip hop";
    if (result == "dnb" || result == "d n b" || result == "d and b" || result == "drum and bass" || result == "drum n bass") return "drum bass";
    if (result == "r and b" || result == "r b") return "rnb";
    return result;
}
bool autoArrangementEligible(const Media& m)
{
    return !m.sourceDetailsPending && m.playbackError.isEmpty() && m.analysisVersion == analysisRevision
        && m.grid().hasBar() && m.bpm() >= 20 && m.bpm() <= 400
        && (m.manualBpm > 0 || (!m.analysis.variableTempo && m.analysis.tempoConfidence >= .55))
        // The key evidence margin is not a probability or an admission threshold.
        // Use the analysed/manual key for matching; an unknown key stays unknown.
        && m.key() >= 0 && m.key() < 24 && std::isfinite(m.duration()) && m.duration() > 0 && m.duration() <= autoArrangementMaxSeconds;
}
AutoArrangementResult planTimelineBlends(Project input, const std::shared_ptr<std::atomic<bool>>& cancelled)
{
    AutoArrangementResult result;
    const auto fail = [&](const juce::String& message) { AutoArrangementResult r; r.project=input; r.error=message; return r; };
    if (input.hasLockedTransitions() || input.hasPendingPlacement()) return fail("Unlock transitions and finish source preparation before blending the timeline.");
    if (input.clips.size()<2) return fail("Place at least two songs on the timeline.");
    auto clean=input;
    const bool trimSources = std::none_of(input.transitionLinks.begin(),input.transitionLinks.end(),[](const auto& link)
    {
        return link.muteOutside && (link.presetName == "Short blend" || link.presetName == "Section blend" || link.presetName == "Bass handover");
    });
    for (const auto& link : input.transitionLinks)
        if (link.presetOwner.isNotEmpty())
        {
            const auto removed=clean.removeOwnedTransitionPreset(link.incoming);
            if (removed.failed()) return fail(removed.getErrorMessage());
        }
    auto order=clean.clips;
    std::stable_sort(order.begin(),order.end(),[](const auto& a,const auto& b) { return a.start<b.start; });
    result.project=clean; result.project.clips.clear(); result.project.transitionLinks.clear(); result.project.transitionLinksKnown=true;
    result.project.tempo={}; result.project.tempo.origin=input.tempo.origin; result.project.tempo.fallback=input.bpm;
    for (auto clip : order)
    {
        if (cancelled && *cancelled) return fail("Timeline blending cancelled. The current mix was kept.");
        if (!clip.media || !clip.media->grid().hasBar()) return fail("Each timeline source needs a usable beat and bar grid.");
        clip.length=clip.sourceElapsed(clip.length); clip.rate=1; clip.prepared.reset(); clip.stemPrepared={}; clip.preparedOffset=0;
        clip.tempoMap.reset(); clip.tempoSourceBpm=0; clip.elasticSource.reset(); clip.elasticPlayback=false;
        clip.start=0; clip.pendingPlacement=PendingPlacement::append; clip.pendingAutoTransition=true;
        clip.pendingTransitionBeats=32; clip.pendingTransitionPreset=TransitionPreset::crossfade; clip.pendingSectionTransition=false;
        auto candidate=result.project; candidate.clips.push_back(clip);
        double quality=0; juce::String decision;
        auto planned=planAutoPair(std::move(candidate),AutoTransitionStyle::balanced,result.decisions,false,false,quality,decision,trimSources,cancelled);
        if (planned.error.isNotEmpty()) return fail(planned.error);
        if (decision.isNotEmpty()) result.decisions.add(decision);
        result.project=std::move(planned.project);
    }
    finishAutoMix(result.project,0);
    result.summary="Blended "+juce::String((int)result.project.clips.size())+" timeline tracks in their existing order.";
    return result;
}
MatchResult prepareOrderedTimeline(Project input, int64_t existing)
{
    auto planned=planTimelineBlends(input);
    if (planned.error.isNotEmpty()) return {input,planned.error};
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    applyAutoArrangementBass(planned,AutoTransitionStyle::balanced,0,formats);
    auto prepared=prepareTempoMap(std::move(planned.project),false,32,existing,0,{},TransitionPreset::crossfade,false);
    if (prepared.error.isNotEmpty()) return {input,prepared.error};
    return prepared;
}
AutoArrangementResult planAutoArrangement(const std::vector<std::shared_ptr<Media>>& library, const AutoArrangementOptions& requested,
                                         juce::Random& random, const std::shared_ptr<std::atomic<bool>>& cancelled, const Project* existing)
{
    AutoArrangementResult result;
    auto options = requested;
    const int laneCount = juce::jlimit(2, trackCount, options.laneCount);
    const bool extending = options.extendExisting;
    const size_t prefix = extending && existing ? existing->clips.size() : 0;
    const Clip* tail = nullptr;
    std::set<juce::String> existingIds; std::set<juce::File> existingFiles;
    if (extending)
    {
        if (!existing || existing->clips.empty() || existing->hasLockedTransitions() || existing->hasPendingPlacement()
            || existing->validateTransitionLinks().failed())
        { result.error = "Choose a ready, unlocked arrangement to extend."; return result; }
        for (const auto& c : existing->clips)
        {
            if (!c.media || !c.media->hasPlayback() || c.media->sourceDetailsPending)
            { result.error = "Wait for the existing arrangement's sources before extending it."; return result; }
            existingIds.insert(c.media->id); if (c.media->file != juce::File()) existingFiles.insert(c.media->file);
            if (!tail || c.end() > tail->end()) tail = &c;
        }
        for (size_t i = 0; i < prefix; ++i)
        {
            const auto link = std::find_if(existing->transitionLinks.begin(),existing->transitionLinks.end(),[&](const auto& l) { return l.outgoing == tail->id; });
            if (link == existing->transitionLinks.end()) break;
            tail = existing->clipFor(link->incoming);
        }
        if (!tail || !autoArrangementEligible(*tail->media))
        { result.error = "Check the last song's beat grid, speed and key before extending."; return result; }
        options.startingId = tail->media->id;
        if (options.targetSeconds) *options.targetSeconds += existing->duration();
    }
    if (options.trackCount < 1 || (requested.targetSeconds && *requested.targetSeconds <= 0) || (options.targetSeconds && (!std::isfinite(*options.targetSeconds)
        || *options.targetSeconds <= 0 || *options.targetSeconds > autoArrangementMaxSeconds))
        || !std::isfinite(options.overlapBeats) || options.overlapBeats < 0 || options.overlapBeats > 256
        || options.preset < TransitionPreset::alignOnly || options.preset > TransitionPreset::crossfade
        || options.transitions < AutoTransitionStyle::balanced || options.transitions > AutoTransitionStyle::varied || options.preferredGenre.length() > 2048
        || options.energy < AutoEnergyPolicy::unrestricted || options.energy > AutoEnergyPolicy::windDown)
    { result.error = "Choose a positive track count or a duration up to 180 minutes."; return result; }
    const auto preset = options.transitions == AutoTransitionStyle::current ? options.preset : TransitionPreset::crossfade;
    const bool shortBlend = shortAutoBlend(options.transitions);
    const bool sections = options.transitions == AutoTransitionStyle::current ? options.sectionTransitions : !shortBlend;
    const double overlap = std::min(128., options.overlapBeats);
    auto stopped = [&] { return cancelled && cancelled->load(); };
    std::vector<std::shared_ptr<Media>> pool;
    std::set<juce::String> ids; std::set<juce::File> files;
    if (tail) { pool.push_back(tail->media); ids.insert(tail->media->id); if (tail->media->file != juce::File()) files.insert(tail->media->file); }
    for (const auto& media : library)
        if (media && autoArrangementEligible(*media) && !ids.count(media->id)
            && (media->file == juce::File() || !files.count(media->file))
            && !existingIds.count(media->id) && (media->file == juce::File() || !existingFiles.count(media->file)))
        {
            ids.insert(media->id); if (media->file != juce::File()) files.insert(media->file);
            // Section transitions must fit even when this source is chosen first.
            if (sections && preset != TransitionPreset::alignOnly
                && media->duration() * media->bpm() / 60 < 32) continue;
            pool.push_back(media);
        }
    result.eligibleTracks = (int) pool.size();
    if (stopped()) { result.error = "Auto arrangement cancelled. The current mix was kept."; return result; }
    if (pool.empty()) { result.error = "No ready tracks yet. Check the library's beat grid, speed and key first."; return result; }
    const bool shaped = options.energy != AutoEnergyPolicy::unrestricted;
    const auto preferredGenre = normalizedAutoGenre(options.preferredGenre);
    const auto genreCost = [&](const Media& m)
    { const auto genre = normalizedAutoGenre(m.assets.genre); return preferredGenre.isEmpty() || genre == preferredGenre ? 0. : genre.isEmpty() ? .04 : .12; };
    std::vector<std::optional<double>> levels(pool.size());
    std::vector<std::vector<size_t>> neighbours(pool.size());
    if (shaped)
        for (size_t i = 0; i < pool.size(); ++i)
        {
            if (stopped()) { result.error = "Auto arrangement cancelled. The current mix was kept."; return result; }
            levels[i] = musicalEnergyBody(pool[i]->energy);
            for (size_t j = 0; j < i; ++j) if (autoCompatible(*pool[i], *pool[j]))
            { neighbours[i].push_back(j); neighbours[j].push_back(i); }
        }
    size_t start = (size_t) random.nextInt((int) pool.size());
    if (options.startingId.isNotEmpty())
    {
        const auto found = std::find_if(pool.begin(), pool.end(), [&](const auto& m) { return m->id == options.startingId; });
        if (found == pool.end()) { result.error = "The first track is unavailable or needs its beat grid, speed and key checked."; return result; }
        start = (size_t) std::distance(pool.begin(), found);
    }
    std::vector<size_t> component;
    if (shaped)
    {
        std::vector<bool> visited(pool.size());
        for (size_t seed = 0; seed < pool.size(); ++seed)
        {
            if (options.startingId.isNotEmpty() && seed != start) continue;
            if (visited[seed]) continue;
            std::vector<size_t> group {seed}; visited[seed] = true;
            for (size_t cursor = 0; cursor < group.size(); ++cursor)
                for (const auto next : neighbours[group[cursor]]) if (!visited[next])
                { visited[next] = true; group.push_back(next); }
            if (group.size() > component.size()) component = std::move(group);
        }
    }
    double lowActivity = 0, highActivity = 0, lowTempo = 0, highTempo = 0;
    if (shaped)
    {
        std::vector<double> known;
        lowTempo = highTempo = std::log(pool[component.front()]->bpm());
        for (const auto i : component)
        {
            if (levels[i]) known.push_back(*levels[i]);
            lowTempo = std::min(lowTempo, std::log(pool[i]->bpm())); highTempo = std::max(highTempo, std::log(pool[i]->bpm()));
        }
        std::sort(known.begin(), known.end());
        if (!known.empty()) { lowActivity = known.front(); highActivity = known.back(); }
        if (options.startingId.isEmpty())
        {
            // Automatic starts leave headroom in the largest compatible group.
            const double fraction = options.energy == AutoEnergyPolicy::build ? 0 : options.energy == AutoEnergyPolicy::windDown ? 1 : .5;
            const double targetActivity = lowActivity + fraction * (highActivity - lowActivity), targetTempo = lowTempo + fraction * (highTempo - lowTempo);
            double best = std::numeric_limits<double>::infinity();
            for (const auto i : component)
            {
                const double score = (levels[i] ? std::abs(*levels[i] - targetActivity) / .25 : .75)
                    + .5 * std::abs(std::log(pool[i]->bpm()) - targetTempo) / std::log(1.08) + genreCost(*pool[i]) + random.nextDouble() * .002;
                if (score < best) { best = score; start = i; }
            }
        }
    }
    const double startActivity = levels[start].value_or((lowActivity + highActivity) * .5), startTempo = std::log(pool[start]->bpm());
    const double endActivity = options.energy == AutoEnergyPolicy::build ? std::max(startActivity, highActivity)
        : options.energy == AutoEnergyPolicy::windDown ? std::min(startActivity, lowActivity) : startActivity;
    const double endTempo = options.energy == AutoEnergyPolicy::build ? std::max(startTempo, highTempo)
        : options.energy == AutoEnergyPolicy::windDown ? std::min(startTempo, lowTempo) : startTempo;
    const size_t available = pool.size() - (extending ? 1 : 0);
    const size_t limit = prefix + (options.targetSeconds ? available : std::min(available, (size_t) options.trackCount));
    if (extending) result.project = *existing;
    else { result.project.name = "Auto arrangement"; result.project.bpm = result.project.tempo.fallback = pool[start]->bpm(); }
    std::set<juce::String> used = existingIds;
    int energyUnknown = 0, energyDepartures = 0, tempoDepartures = 0;
    juce::String candidateDecision;
    double candidateQuality=0;
    auto append = [&](const std::shared_ptr<Media>& media)
    {
        auto p = result.project;
        Clip clip; clip.media = media; clip.length = media->duration(); clip.start = p.duration(); clip.hotcuesPending = true;
        clip.track = extending ? ((p.clips.size() == prefix ? tail->track : p.clips.back().track)+1)%laneCount : (int) p.clips.size()%laneCount;
        clip.pendingPlacement = PendingPlacement::append; clip.pendingAutoTransition = true;
        double preferredOverlap = overlap;
        std::optional<double> previousFrameEnd;
        if (shortBlend && !p.clips.empty())
        {
            const auto& out = extending && p.clips.size() == prefix ? *p.clipFor(tail->id) : p.clips.back();
            const auto clock = p.gridClock();
            const auto* previousJoin = p.transitionFor(out.id);
            previousFrameEnd = previousJoin ? p.transitionRange(*previousJoin).getEnd() : out.start;
            const double room = clock.beatsAt(out.end()) - clock.beatsAt(*previousFrameEnd);
            const double incomingBeats = media->duration() * media->bpm() / 60;
            preferredOverlap = overlap == 16 || std::min(room, incomingBeats) < 36 ? 16 : 32;
            // Leave the middle song an audible lead and keep part of the incoming song beyond the fade.
            preferredOverlap = std::min({preferredOverlap, room - (previousJoin ? 4. : 0.), incomingBeats - 4});
            if (preferredOverlap < 4) { MatchResult failed; failed.error = "The two songs have no room for a short blend."; return failed; }
        }
        clip.pendingTransitionPreset = preset; clip.pendingTransitionBeats = preferredOverlap;
        clip.pendingSectionTransition = sections;
        p.clips.push_back(std::move(clip));
        // Both Auto Arrange commands use this pair owner. Explicit styles keep
        // their requested treatment and section policy through the same adapter.
        auto planned = planAutoPair(p,options.transitions,result.decisions,extending,
            extending && result.project.clips.size()==prefix,candidateQuality,candidateDecision,true,cancelled);
        if (!shortBlend && extending && result.project.clips.size() == prefix)
        {
            const auto* previousJoin = existing->transitionFor(tail->id);
            const double protectedEnd = previousJoin ? existing->transitionRange(*previousJoin).getEnd() : tail->start;
            const auto* newJoin = planned.error.isEmpty() ? planned.project.transitionFor(planned.project.clips.back().id) : nullptr;
            if (newJoin && planned.project.transitionRange(*newJoin).getStart() < protectedEnd - 1.e-8)
            {
                const auto clock = existing->gridClock();
                const double room = clock.beatsAt(tail->end()) - clock.beatsAt(protectedEnd) - 4;
                if (room > 0)
                {
                    const double shorter = std::min({32.,preferredOverlap,room});
                    p.clips.back().pendingTransitionBeats = shorter; p.clips.back().pendingSectionTransition = false;
                    planned = planAutoPair(p,options.transitions,result.decisions,true,true,candidateQuality,candidateDecision,true,cancelled);
                    newJoin = planned.error.isEmpty() ? planned.project.transitionFor(planned.project.clips.back().id) : nullptr;
                }
                if (!newJoin || planned.project.transitionRange(*newJoin).getStart() < protectedEnd - 1.e-8)
                    planned.error = "The last song has no unused tail for a new handover after its existing transition.";
            }
        }
        if (extending && planned.error.isEmpty())
        {
            auto& incoming = planned.project.clips.back(); bool free = false;
            const int previousTrack = result.project.clips.size() == prefix ? tail->track : result.project.clips.back().track;
            for (int offset = 1; offset <= laneCount; ++offset)
            {
                incoming.track = (previousTrack + offset) % laneCount;
                if (incoming.track == previousTrack) continue;
                if (std::none_of(planned.project.clips.begin(),std::prev(planned.project.clips.end()),[&](const auto& c)
                    { return c.track == incoming.track && incoming.start < c.end()-1.e-9 && incoming.end() > c.start+1.e-9; }))
                { free = true; break; }
            }
            if (!free) planned.error = "No free lane fits the next handover without overlapping existing audio.";
        }
        return planned;
    };
    if (!extending)
    {
        auto first = append(pool[start]);
        if (first.error.isNotEmpty() || !std::isfinite(first.project.duration()) || first.project.duration() > autoArrangementMaxSeconds)
        { result.error = first.error.isNotEmpty() ? first.error : "The starting track exceeds the three-hour limit."; return result; }
        result.project = std::move(first.project); used.insert(pool[start]->id);
    }
    while (result.project.clips.size() < limit && (!options.targetSeconds || result.project.duration() < *options.targetSeconds))
    {
        const auto& previous = *(extending && result.project.clips.size() == prefix ? tail->media : result.project.clips.back().media);
        const auto previousIndex = (size_t) std::distance(pool.begin(), std::find_if(pool.begin(), pool.end(), [&](const auto& m) { return m->id == previous.id; }));
        const double progress = options.targetSeconds ? std::min(1., (result.project.duration()-(extending ? existing->duration() : 0.)) / *requested.targetSeconds)
            : limit-prefix > 1 ? (double) (result.project.clips.size()-prefix) / (limit-prefix-1) : 1.;
        const double targetActivity = startActivity + progress * (endActivity - startActivity), targetTempo = startTempo + progress * (endTempo - startTempo);
        std::vector<std::pair<double, size_t>> ranked;
        for (size_t index = 0; index < pool.size(); ++index)
        {
            const auto& m = pool[index];
            if (used.count(m->id) || !autoCompatible(previous, *m)) continue;
            const double distance = std::abs(std::log(m->bpm() / previous.bpm()));
            double score = distance + (previous.key() == m->key() ? 0.0 : .01) + genreCost(*m) + random.nextDouble() * .002;
            if (shaped)
            {
                score += levels[index] ? std::abs(*levels[index] - targetActivity) / .25 : .75;
                score += .5 * std::abs(std::log(m->bpm()) - targetTempo) / std::log(1.08);
                if (levels[index] && levels[previousIndex])
                {
                    const double change = *levels[index] - *levels[previousIndex];
                    score += .35 * (options.energy == AutoEnergyPolicy::windDown ? std::max(0., change) : std::max(0., -change)) / .25;
                }
                const bool needsNext = result.project.clips.size() + 1 < limit
                    && (!options.targetSeconds || result.project.duration() + m->duration() < *options.targetSeconds);
                if (needsNext && std::none_of(neighbours[index].begin(), neighbours[index].end(), [&](size_t n) { return !used.count(pool[n]->id); })) score += 2;
            }
            ranked.emplace_back(score, index);
        }
        std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        if (ranked.empty())
        {
            result.stopReason = "No unused tracks match the last track's speed and key.";
            break;
        }
        bool added = false;
        int capRejected = 0, targetRejected = 0;
        juce::String placementFailure;
        double bestScore = std::numeric_limits<double>::infinity();
        std::optional<Project> bestProject;
        size_t bestIndex = 0;
        std::optional<std::array<double, 4>> bestEnergy;
        juce::String bestDecision;
        for (const auto& candidate : ranked)
        {
            if (stopped()) { result.error = "Auto arrangement cancelled. The current mix was kept."; return result; }
            if (candidate.first >= bestScore) break; // Handover costs below are nonnegative.
            auto next = append(pool[candidate.second]);
            const double duration = next.project.duration();
            if (next.error.isNotEmpty() || !std::isfinite(duration) || duration <= result.project.duration() + .001)
            {
                if (placementFailure.isEmpty()) placementFailure = next.error.isNotEmpty() ? next.error : "The next track did not extend the arrangement.";
                continue;
            }
            if (duration > autoArrangementMaxSeconds + 1e-7) { ++capRejected; continue; }
            if (options.targetSeconds && std::abs(duration - *options.targetSeconds) > std::abs(result.project.duration() - *options.targetSeconds))
            { ++targetRejected; continue; }
            double score = candidate.first + .015*candidateQuality;
            std::optional<std::array<double, 4>> energy;
            if (shaped)
            {
                const auto* link = next.project.transitionFor(next.project.clips.back().id);
                energy = link ? autoTransitionActivity(next.project, *link) : std::nullopt;
                if (energy)
                    for (size_t half = 0; half < 2; ++half)
                    {
                        const double change = (*energy)[half + 2] - (*energy)[half];
                        score += .05 * (options.energy == AutoEnergyPolicy::maintain ? std::abs(change)
                            : options.energy == AutoEnergyPolicy::windDown ? std::max(0., change) : std::max(0., -change)) / .25;
                    }
            }
            if (score < bestScore)
            { bestScore = score; bestIndex = candidate.second; bestProject = std::move(next.project); bestEnergy = energy; bestDecision = candidateDecision; }
        }
        if (bestProject)
        {
            if (shaped)
            {
                if (!bestEnergy || !levels[previousIndex] || !levels[bestIndex]) ++energyUnknown;
                else
                {
                    const double change = *levels[bestIndex] - *levels[previousIndex];
                    if (options.energy == AutoEnergyPolicy::windDown ? change >= -.025
                        : options.energy == AutoEnergyPolicy::build ? change <= .025 : std::abs(change) > .1) ++energyDepartures;
                }
                const double change = pool[bestIndex]->bpm() - previous.bpm();
                if (options.energy == AutoEnergyPolicy::windDown ? change > .01 : change < -.01) ++tempoDepartures;
            }
            result.project = std::move(*bestProject); used.insert(pool[bestIndex]->id); added = true;
            if (bestDecision.isNotEmpty()) result.decisions.add(bestDecision);
        }
        if (!added)
        {
            result.stopReason = capRejected > 0 ? "The next track would take the mix over 180 minutes."
                : targetRejected > 0 ? "Another track would move the mix farther from your chosen length."
                : "Could not prepare the next transition: " + placementFailure;
            break;
        }
    }
    if (stopped()) { result.error = "Auto arrangement cancelled. The current mix was kept."; return result; }
    const auto count = juce::String((int) (result.project.clips.size()-prefix));
    compactAutoNodes(result.project,prefix);
    const int trimmed = finishAutoMix(result.project, prefix);
    result.summary = (extending ? "Added: " : "Finished: ") + count + (options.targetSeconds ? " tracks" : " of " + juce::String(options.trackCount) + " requested tracks")
        + " / " + timeText(result.project.duration()) + ".";
    if (options.targetSeconds) result.summary += " Requested " + juce::String(extending ? "additional length: " : "length: ")
        + juce::String(*requested.targetSeconds / 60, 1) + " minutes (retaining song bodies).";
    if ((!options.targetSeconds && result.project.clips.size()-prefix < (size_t) options.trackCount)
        || (options.targetSeconds && result.project.duration() < *options.targetSeconds))
    {
        if (result.stopReason.isEmpty()) result.stopReason = "No more ready tracks in your library.";
        result.summary += "\nStopped: " + result.stopReason;
    }
    result.summary += "\nUndo brings back your previous mix.";
    if (extending) result.summary += " Existing joins and source timing kept; the new handover uses the last song's tail.";
    if (trimmed > 0) result.summary += "\nLevel trims on " + juce::String(trimmed) + " tracks.";
    if (result.project.clips.size() > std::max<size_t>(prefix, 1))
        result.summary += "\nNew pairs are locked for movement.";
    const std::array<const char*, 4> policies {"Unrestricted", "Keep pace", "Build energy", "Wind down"};
    result.summary += "\nEnergy: " + juce::String(policies[(size_t) options.energy]) + ".";
    if (shaped)
    {
        result.summary += " Best available overall progression; plateaus and necessary reversals are allowed."
            "\nUses rhythm, bass activity and dynamics, with BPM guidance. Unmeasured activity stays unknown.";
        result.summary += "\n" + juce::String(options.energy == AutoEnergyPolicy::maintain ? "Activity changes: "
            : options.energy == AutoEnergyPolicy::build ? "Energy plateaus/dips: " : "Energy plateaus/rises: ") + juce::String(energyDepartures)
            + (options.energy == AutoEnergyPolicy::windDown ? "; tempo rises: " : "; tempo dips: ") + juce::String(tempoDepartures)
            + "; energy unavailable: " + juce::String(energyUnknown) + ".";
    }
    if (preferredGenre.isNotEmpty())
    {
        int unknown = 0, other = 0;
        for (const auto& clip : result.project.clips)
        { const auto genre = normalizedAutoGenre(clip.media->assets.genre); if (genre.isEmpty()) ++unknown; else if (genre != preferredGenre) ++other; }
        result.summary += "\nGenre preference: " + preferredGenre + "; other genres: " + juce::String(other) + "; unlabelled: " + juce::String(unknown) + ".";
    }
    return result;
}

juce::String AutoArrangementResult::completionText(AutoTransitionStyle style) const
{
    if (decisions.isEmpty()) return summary;
    std::map<juce::String, int> counts;
    for (const auto& decision : decisions) ++counts[decision.upToFirstOccurrenceOf(":", false, false)];
    juce::StringArray blends;
    for (const auto& [name, uses] : counts) blends.add(name + " (" + juce::String(uses) + ")");
    const auto heading = style == AutoTransitionStyle::varied ? "\nVariety: " : "\nBlends: ";
    const int afterCount = summary.indexOfChar('\n');
    return summary.substring(0, afterCount) + heading + blends.joinIntoString(", ") + "." + summary.substring(afterCount);
}

namespace
{
bool strongHandoverBass(const Clip& clip, double begin, double end, juce::AudioFormatManager& formats,
                        const std::shared_ptr<std::atomic<bool>>& cancelled)
{
    if (!clip.media || clip.reversed || clip.loopSpan > 0) return false;
    const auto& media = *clip.media;
    const double rate = media.sampleRate;
    if (!std::isfinite(rate) || rate < 2000 || rate > 384000) return false;
    const auto sourceTime = [&](double at)
    { return juce::jlimit(0., media.duration(), clip.offset + clip.audioSourceElapsed(at - clip.start)); };
    const auto first = (int64_t) std::llround(sourceTime(begin) * rate);
    const auto last = (int64_t) std::llround(sourceTime(end) * rate);
    if (last - first < rate * .25 || last - first > rate * 8) return false;
    const int frames = (int) (last - first);
    const auto samples = media.audioView();
    juce::AudioBuffer<float> window;
    if (samples.getNumChannels() > 0 && last <= samples.getNumSamples())
    {
        window.setSize(std::min(2, samples.getNumChannels()), frames);
        for (int channel = 0; channel < window.getNumChannels(); ++channel)
            window.copyFrom(channel, 0, samples.getReadPointer(channel) + first, frames);
    }
    else
    {
        if (!media.original || media.file.getSize() != media.original->fileBytes
            || media.file.getLastModificationTime().toMilliseconds() != media.original->modified) return false;
        const std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(media.file));
        if (!reader || reader->sampleRate != rate || last > reader->lengthInSamples || reader->numChannels == 0) return false;
        window.setSize((int) std::min(2u, reader->numChannels), frames);
        if (!reader->read(&window, 0, frames, first, true, true)) return false;
    }
    const auto profile = analyseMusicalEnergy(PcmView(window), rate, cancelled);
    if (profile.windows.empty()) return false;
    const auto& evidence = profile.windows.front();
    double power = 0;
    for (int channel = 0; channel < window.getNumChannels(); ++channel)
    {
        const double rms = window.getRMSLevel(channel, 0, frames);
        power += rms * rms / window.getNumChannels();
    }
    const double lowDb = 10 * std::log10(std::max(1.e-12, power * evidence.bassRatio)) + clip.gainDb;
    // Require both relative weight and audible post-trim level around the handover.
    return evidence.coverage >= .35 && evidence.bassRatio >= .28 && lowDb >= -32;
}

void ownShortBlend(Project& project, Project::TransitionLink& link, const juce::String& decision)
{
    const auto range = project.transitionRange(link);
    link.presetName = decision.upToFirstOccurrenceOf(":", false, false);
    if (link.presetOwner.isEmpty()) link.presetOwner = juce::Uuid().toString();
    const bool bass = link.presetName == "Bass handover";
    const bool filter = decision.contains("filter fade");
    for (const auto& id : {link.outgoing, link.incoming})
    {
        const auto& clip = *project.clipFor(id);
        const double begin = clip.effectOffset+clip.sourceElapsed(range.getStart() - clip.start);
        const double end = clip.effectOffset+clip.sourceElapsed(range.getEnd() - clip.start);
        auto& automation = project.effectSettings(clip).automation;
        // Auto Arrange replaced these parameters within the frame. Mark its
        // endpoints and sub-sample guards so a later preset can replace them.
        for (const auto target : {AutomationTarget::volume, AutomationTarget::bass, AutomationTarget::mid, AutomationTarget::treble, AutomationTarget::cutoff})
        {
            if ((target >= AutomationTarget::bass && target <= AutomationTarget::treble && !bass) || (target == AutomationTarget::cutoff && !filter)) continue;
            for (auto& point : automation[(size_t) target].points)
                if (point.presetOwner.isEmpty() && point.time >= begin - 1.e-9 && point.time <= end + 1.e-9)
                    point.presetOwner = link.presetOwner;
        }
    }
}

juce::Result coordinatedHandover(Project& project, const juce::String& incoming, double beat)
{
    const auto* link=project.transitionFor(incoming); if (!link) return juce::Result::fail("Missing handover.");
    for (int target=1; target<=3; ++target)
        if (authoredInFrame(project,project,*link,target)) return juce::Result::fail("Authored EQ kept.");
    const auto range=project.transitionRange(*link); const auto clock=project.gridClock();
    const double h=(clock.timeAtBeat(beat)-range.getStart())/range.getLength();
    // Four beats of low-end exchange, while both channels are near unity.
    // The upper bands compensate the fader hold. Multiplying a bass swap by
    // two ordinary full-frame fades would instead dig a hole at the crossover.
    const double a=std::max(0.,(clock.timeAtBeat(beat-2)-range.getStart())/range.getLength());
    const double b=std::min(1.,(clock.timeAtBeat(beat+2)-range.getStart())/range.getLength());
    const std::array<juce::String,2> ids{link->outgoing,link->incoming};
    const auto owner=link->presetOwner;
    std::array<std::array<AutomationCurve,4>,2> previous;
    for (size_t side=0; side<ids.size(); ++side) for (size_t target=0; target<4; ++target)
        previous[side][target]=project.effectSettings(*project.clipFor(ids[side])).automation[target];
    std::set<juce::String> generatedBass;
    for (const auto& id : {link->outgoing,link->incoming})
    {
        const auto& curve=project.effectSettings(*project.clipFor(id)).automation[1];
        // Earlier generated joins can leave a neutral held endpoint behind.
        // It carries no audible shaping; non-neutral or shaped authored curves
        // still keep their exterior points exactly.
        const bool neutral=std::all_of(curve.points.begin(),curve.points.end(),[](const auto& p) { return p.value==0 && !p.shape; });
        if (neutral || std::none_of(curve.points.begin(),curve.points.end(),[](const auto& p) { return p.presetOwner.isEmpty(); }))
            generatedBass.insert(id);
    }
    TransitionStyle::Nodes nodes;
    const auto db=[](double value) { return (float)juce::Decibels::gainToDecibels(value,-160.); };
    const std::array<double,5> positions{0,h*.5,h,(h+1)*.5,1};
    for (int side=0; side<2; ++side)
    {
        auto& volume=nodes.curves[(size_t)side][0]; volume.enabled=true;
        AutomationShape shape; shape.kind=1; shape.rising=side==1;
        if (side==0) volume.points={{0,0,0,false,true},{h,0,0,false,true,{},shape},{1,-160,0,false,true}};
        else volume.points={{0,-160,0,false,true,{},shape},{h,0,0,false,true},{1,0,0,false,true}};
        for (int target=1; target<=3; ++target)
        {
            auto& curve=nodes.curves[(size_t)side][(size_t)target]; curve.enabled=true;
            if (target==1)
            {
                // One readable, monotonic bass exchange. The fader hold already
                // supplies headroom here; do not turn EQ back up as a song exits.
                AutomationShape bass; bass.kind=1; bass.rising=side==1;
                bass.strength=1-juce::Decibels::decibelsToGain(-18.);
                const float first=side==0 ? 0.f : -18.f, last=side==0 ? -18.f : 0.f;
                curve.points={{0,first,0,false,true},{a,first,0,false,true,{},bass},{b,last,0,false,true},{1,last,0,false,true}};
                if (a<=1.e-9) curve.points.erase(curve.points.begin());
                if (b>=1-1.e-9) curve.points.pop_back();
                continue;
            }
            for (double x : positions)
            {
                const double fader=side==0 ? std::cos(juce::MathConstants<double>::halfPi*std::max(0.,(x-h)/(1-h)))
                    : std::sin(juce::MathConstants<double>::halfPi*std::min(1.,x/h));
                const double desired=side==0 ? std::cos(juce::MathConstants<double>::halfPi*x) : std::sin(juce::MathConstants<double>::halfPi*x);
                const double limit=side==0 ? 1-h : h;
                const double gain=fader<1.e-7 ? limit : juce::jlimit(0.,1.,desired/fader);
                curve.points.push_back({x,db(gain),0,false,true});
            }
        }
    }
    const auto applied=applyPresetNodes(project,incoming,nodes,0x0f);
    if (applied.wasOk()) for (size_t side=0; side<ids.size(); ++side)
    {
        const auto& clip=*project.clipFor(ids[side]); auto& controls=project.effectSettings(clip);
        const double begin=clip.effectOffset+clip.sourceElapsed(range.getStart()-clip.start);
        const double end=clip.effectOffset+clip.sourceElapsed(range.getEnd()-clip.start);
        for (size_t target=0; target<4; ++target)
            ownGeneratedPadding(controls.automation[target],previous[side][target],owner,begin,end,controls.manualValue((AutomationTarget)target));
    }
    if (applied.wasOk() && !generatedBass.empty())
    {
        // Neutral exterior padding must not restore the entering bass before
        // its cue, or restore the departing bass after the exchange. Extend
        // these generated holds; retain the song's other incoming/outgoing join.
        for (const auto& id : generatedBass)
        {
            const auto& clip=*project.clipFor(id);
            const AutomationAddress address{clip.track,AutomationTarget::bass,{},{},id};
            const bool entering=id==incoming;
            const double edge=project.automationTime(address,range.getStart()+(entering ? a : b)*range.getLength());
            auto& points=project.effectSettings(clip).automation[1].points;
            points.erase(std::remove_if(points.begin(),points.end(),[&](const auto& p)
                { return entering ? p.time<edge : p.time>edge; }),points.end());
        }
        compactAutoNodes(project,0,AutomationTarget::bass,generatedBass);
    }
    return applied;
}
}

void applyAutoArrangementBass(AutoArrangementResult& result, AutoTransitionStyle style, size_t firstNewClip,
                              juce::AudioFormatManager& formats, const std::shared_ptr<std::atomic<bool>>& cancelled)
{
    if (!shortAutoBlend(style) || result.error.isNotEmpty()) return;
    auto& project = result.project;
    const auto clock = project.gridClock();
    const auto first = std::max<size_t>(1, firstNewClip);
    for (size_t i = first; i < project.clips.size(); ++i)
    {
        if (cancelled && *cancelled) return;
        const auto incoming = project.clips[i].id;
        const auto* link = project.transitionFor(incoming);
        if (!link) continue;
        const auto* out = project.clipFor(link->outgoing);
        const auto range = project.transitionRange(*link);
        const double begin = clock.beatsAt(range.getStart()), end = clock.beatsAt(range.getEnd());
        double beat = 4 * std::round((begin + end) / 8);
        if (!out || beat < begin + 2 || beat > end - 2) continue;
        if (firstNewClip > 0 && i == first && project.effectSettings(*out).automation[(size_t) AutomationTarget::bass].active())
        {
            const auto& bass = project.effectSettings(*out).automation[(size_t) AutomationTarget::bass];
            const auto tail = out->sourceElapsed(range.getStart() - out->start);
            if (std::any_of(bass.points.begin(), bass.points.end(), [&](const auto& point) { return point.time >= tail - 1.e-9; })) continue;
        }
        // Prefer a structural change close to the level-matched handover.
        // Keep the exchange inside the frame, with useful audio on both sides.
        const auto& in = project.clips[i];
        const bool sections = !out->reversed && !in.reversed && out->loopSpan<=0 && in.loopSpan<=0;
        const auto outSections = sections ? handoverSections(*out,out->sourceElapsed(out->length),false) : std::vector<EnergySection>{};
        const auto inSections = sections ? handoverSections(in,in.sourceElapsed(in.length),true) : std::vector<EnergySection>{};
        const auto evidence = [&](const Clip& clip, const auto& boundaries, double at)
        {
            const double source = clip.offset+clip.audioSourceElapsed(at-clip.start);
            const double halfBeat = 30/clip.media->bpm();
            double weight = 0;
            for (const auto& section : boundaries)
                if (std::abs(source-section.sourceSeconds)<=halfBeat) weight=std::max(weight,sectionWeight(section));
            return weight;
        };
        double best=INFINITY;
        const double margin = std::max(2.,(end-begin)*.2);
        for (double candidate=4*std::ceil((begin+margin)/4); candidate<=end-margin; candidate+=4)
        {
            const double fraction = (candidate-begin)/(end-begin);
            const double at=clock.timeAtBeat(candidate);
            const auto level=[&](const Clip& c) { return sourceEnergy(*c.media,c.offset+c.audioSourceElapsed(at-1-c.start),c.offset+c.audioSourceElapsed(at+1-c.start)); };
            const auto a=level(*out), b=level(in); if (!a || !b) continue;
            const double difference=std::abs(10*std::log10(std::max(*a,1.e-12)/std::max(*b,1.e-12))+out->gainDb-in.gainDb)
                + 2*std::abs(fraction-.5) - evidence(*out,outSections,at) - evidence(in,inSections,at);
            if (difference<best) { best=difference; beat=candidate; }
        }
        const double at = clock.timeAtBeat(beat);
        const double measureStart = std::max(at - 2., clock.timeAtBeat(beat - 2));
        const double measureEnd = std::min(at + 2., clock.timeAtBeat(beat + 2));
        if (!strongHandoverBass(*out, measureStart, measureEnd, formats, cancelled)
            || !strongHandoverBass(project.clips[i], measureStart, measureEnd, formats, cancelled)) continue;
        if (coordinatedHandover(project,incoming,beat).wasOk() && i-first<(size_t)result.decisions.size())
        {
            result.decisions.set((int)(i-first),result.decisions[(int)(i-first)].replace("Short blend:","Bass handover:")
                .replace("Section blend:","Bass handover:")
                .replace("curved volume handover","coordinated volume and EQ"));
            // A middle song participates again on the next iteration. Its
            // generated entrance must already be owned when shaping its exit.
            const auto owned=std::find_if(project.transitionLinks.begin(),project.transitionLinks.end(),
                [&](const auto& item) { return item.incoming==incoming; });
            if (owned!=project.transitionLinks.end()) ownShortBlend(project,*owned,result.decisions[(int)(i-first)]);
        }
    }
    // Keep exterior/authored nodes intact. Library planning already compacted
    // its fresh curves; the timeline command may contain hand-edited controls.
    for (size_t i = first; i < project.clips.size() && i - first < (size_t) result.decisions.size(); ++i)
    {
        const auto& id = project.clips[i].id;
        const auto link = std::find_if(project.transitionLinks.begin(), project.transitionLinks.end(),
                                      [&](const auto& item) { return item.incoming == id; });
        if (link != project.transitionLinks.end()) ownShortBlend(project, *link, result.decisions[(int) (i - first)]);
    }
}
}
