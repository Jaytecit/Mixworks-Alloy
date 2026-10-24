#include "Workstation.h"
#include "AutoArrangement.h"
#include "Beatmatch.h"
#include "LibraryCatalogue.h"
#include "EnergySections.h"
#include <cmath>
#include <limits>
#include <set>
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace mixworks
{
namespace
{
float nodeValue(const Project& p,int side,int target,double fraction)
{
    if (p.transitionLinks.empty() || p.clips.size() < 2) return std::numeric_limits<float>::quiet_NaN();
    const auto& link = p.transitionLinks.front(); const auto* clip = p.clipFor(side == 0 ? link.outgoing : link.incoming);
    const auto range = p.transitionRange(link); const AutomationAddress address{clip->track,(AutomationTarget) target,{},{},clip->id};
    return clip->controls.automation[(size_t) target].valueAt(p.automationTime(address,range.getStart()+range.getLength()*fraction),p.manualValue(address));
}
bool nodeFade(const Project& p)
{ return nodeValue(p,0,0,.25) > nodeValue(p,0,0,.75) && nodeValue(p,1,0,.25) < nodeValue(p,1,0,.75); }
bool nodeSwap(const Project& p)
{ return nodeValue(p,0,1,.1) == 0 && nodeValue(p,0,1,.9) <= -60 && nodeValue(p,1,1,.1) <= -60 && nodeValue(p,1,1,.9) == 0; }
void activityProfile(Media& m, double onsets, double bass = 1, double ratio = .2)
{
    m.energy = {}; m.energy.revision = musicalEnergyRevision; m.energy.duration = m.duration();
    for (double t = 0; t < m.duration(); t += 8)
        m.energy.windows.push_back({t, std::min(t + 8, m.duration()), 1, onsets, bass, ratio, 6, 8});
}
void pumpAutoMessages(int milliseconds)
{
    const auto until = juce::Time::getMillisecondCounterHiRes() + milliseconds;
    do
    {
#if JUCE_WINDOWS
        MSG message;
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        { TranslateMessage(&message); DispatchMessageW(&message); }
#endif
        juce::Thread::sleep(1);
    } while (juce::Time::getMillisecondCounterHiRes() < until);
}
}
void runAutoArrangementTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    auto source = [](juce::String name, double bpm = 120, int key = 0, double seconds = 96)
    {
        auto m = std::make_shared<Media>(); m->name = name; m->sampleRate = 8000;
        m->analysisSource.frames = (int) (seconds * m->sampleRate); m->analysisSource.channels = 1;
        m->analysis.bpm = bpm; m->analysis.tempoConfidence = 1; m->analysis.downbeatOrigin = 0;
        m->analysis.key = key; m->analysis.keyConfidence = 1;
        return m;
    };
    auto a = source("Alpha"), b = source("Bravo", 122), c = source("Charlie", 124);
    auto wrongKey = source("Incompatible key", 120, 1), fast = source("Too fast", 160), unknown = source("Unknown key"); unknown->analysis.key = -1;
    AutoArrangementOptions options; options.startingId = a->id; options.trackCount = 20;
    juce::Random random(548);
    auto weakA = a->catalogueCopy(), weakB = b->catalogueCopy();
    weakA->analysis.keyConfidence = .018; weakB->analysis.keyConfidence = .027;
    const auto weakPlan = planAutoArrangement({weakA, weakB}, options, random);
    check(weakPlan.error.isEmpty() && weakPlan.project.clips.size() == 2,
        "realistic key evidence margins remain eligible for harmonic matching instead of being treated as probabilities");
    auto plan = planAutoArrangement({a, wrongKey, unknown, fast, c, b, a}, options, random);
    check(plan.error.isEmpty() && plan.project.clips.size() == 3 && plan.project.clips.front().media == a
        && plan.project.clips[1].media == b && plan.project.clips[2].media == c,
        "auto arrangement preserves a chosen start, ranks nearby BPM, rejects incompatible keys and avoids duplicates");
    check(plan.project.transitionLinks.size() == 2 && nodeFade(plan.project)
        && plan.project.validateTransitionLinks().wasOk(), "auto arrangement uses production section Crossfade and valid transition ownership");
    bool aligned = true;
    for (const auto& clip : plan.project.clips)
        aligned &= clip.beatmatched && std::abs(std::remainder(plan.project.tempo.beatsAt(clip.start + clip.firstDownbeatTime()), 4.0)) < 1e-6
            && !clip.prepared && !clip.media->hasPlayback();
    check(aligned, "planning aligns source bars with the production tempo map without decoding or preparing audio");
    // A relative key uses the existing Camelot owner rather than a second key interpretation.
    auto relative = source("Relative minor", 120, 21);
    auto compatible = planAutoArrangement({a, relative}, options, random);
    check(harmonicallyCompatible(a->key(), relative->key()) && compatible.project.clips.size() == 2,
        "auto arrangement admits relative major/minor through the existing harmonic matcher");
    options.trackCount = 2; plan = planAutoArrangement({a, b, c}, options, random);
    check(plan.project.clips.size() == 2, "auto arrangement honours a requested track count");
    {
        auto tagged = source("Preferred tag", 121), unlabelled = source("Unlabelled"), other = source("Other genre");
        tagged->assets.genre = "HOUSE"; other->assets.genre = "Techno";
        auto genreOptions = options; genreOptions.preferredGenre = " house "; genreOptions.transitions = AutoTransitionStyle::smooth;
        const auto preferred = planAutoArrangement({a, other, unlabelled, tagged}, genreOptions, random);
        check(preferred.project.clips.size() == 2 && preferred.project.clips[1].media == tagged && preferred.project.clips.front().media == a,
            "genre is a normalized soft preference that preserves the explicit first song");
        genreOptions.trackCount = 3;
        const auto fallback = planAutoArrangement({a, other, unlabelled}, genreOptions, random);
        check(fallback.project.clips.size() == 3 && fallback.summary.contains("unlabelled: 2") && fallback.summary.contains("other genres: 1"),
            "unavailable preferred genre never truncates a feasible mix and reports unknown/other-tag use");
        check(normalizedAutoGenre("Hip-Hop") == normalizedAutoGenre("hiphop")
            && normalizedAutoGenre("D&B") == normalizedAutoGenre("dnb")
            && normalizedAutoGenre("Trip-hop") != normalizedAutoGenre("Hip-hop"), "genre aliases are explicit and do not use unsafe substring matches");
        auto unavailableTag = source("Wrong tempo tagged", 180); unavailableTag->assets.genre = "House";
        genreOptions.trackCount = 2;
        const auto guarded = planAutoArrangement({a, unavailableTag, unlabelled}, genreOptions, random);
        check(guarded.project.clips.size() == 2 && guarded.project.clips[1].media == unlabelled,
            "genre cannot override the adjacent tempo feasibility guard");
        activityProfile(*a, 2); activityProfile(*tagged, .2); activityProfile(*other, 6);
        genreOptions.energy = AutoEnergyPolicy::build;
        const auto energyFirst = planAutoArrangement({a, tagged, other}, genreOptions, random);
        check(energyFirst.project.clips.size() == 2 && energyFirst.project.clips[1].media == other,
            "a substantially better musical progression supersedes a genre match");
    }
    auto shortageOptions = options; shortageOptions.trackCount = 10;
    const auto shortage = planAutoArrangement({a, wrongKey}, shortageOptions, random);
    check(shortage.summary.contains("1 of 10") && shortage.summary.containsIgnoreCase("no unused tracks"),
        "an early stop explicitly reports requested and placed counts and why no next match exists");
    options.targetSeconds = 176; plan = planAutoArrangement({a, b, c}, options, random);
    check(plan.error.isEmpty() && plan.project.clips.size() == 2 && std::abs(plan.project.duration() - 176) < 3,
        "duration targeting accounts for overlap and tempo rather than summing source lengths");
    options.targetSeconds = 10; plan = planAutoArrangement({a, b}, options, random);
    check(plan.project.clips.size() == 1, "a target shorter than the chosen start retains that whole starting track");
    options.targetSeconds = autoArrangementMaxSeconds + 1;
    check(planAutoArrangement({a}, options, random).error.isNotEmpty(), "duration above three hours rejects without a candidate");
    options.targetSeconds = std::numeric_limits<double>::quiet_NaN();
    check(planAutoArrangement({a}, options, random).error.isNotEmpty(), "nonfinite duration rejects at the request boundary");
    options.targetSeconds.reset(); options.trackCount = 0;
    check(planAutoArrangement({a}, options, random).error.isNotEmpty(), "zero track count rejects at the request boundary");
    options.trackCount = 20;
    auto longA = source("Long A", 120, 0, 6000), longB = source("Long B", 120, 0, 6000);
    options.startingId = longA->id; plan = planAutoArrangement({longA, longB}, options, random);
    check(plan.error.isEmpty() && plan.project.clips.size() == 1 && plan.project.duration() <= autoArrangementMaxSeconds,
        "track-count mode also enforces the three-hour arranged duration cap");
    options.startingId = "missing";
    check(planAutoArrangement({a}, options, random).error.isNotEmpty(), "an unavailable explicit start never silently becomes random");
    options.startingId.clear(); std::set<juce::String> starts;
    for (int i = 0; i < 18; ++i) starts.insert(planAutoArrangement({a, b, c}, options, random).project.clips.front().media->id);
    check(starts.size() == 3, "random start can select each eligible library track");
    auto bad = source("Bad BPM"); bad->analysis.bpm = std::numeric_limits<double>::quiet_NaN();
    auto variable = source("Variable tempo"); variable->analysis.variableTempo = true;
    auto noBar = source("No downbeat"); noBar->analysis.downbeatOrigin = -1;
    auto old = source("Old analysis"); old->analysisVersion = analysisRevision - 1;
    check(planAutoArrangement({nullptr, bad, variable, noBar, old, unknown}, options, random).error.isNotEmpty(),
        "unknown key, stale analysis, invalid BPM, variable tempo and missing bar phase cannot manufacture eligible sources");
    auto cancelled = std::make_shared<std::atomic<bool>>(true);
    check(planAutoArrangement({a, b}, options, random, cancelled).error.containsIgnoreCase("cancel"), "planning honours cancellation");
    {
        const auto waveform = [](Media& m, float rms)
        { const int bucket = std::max(1, m.frames() / 4000); m.waveformRms.assign((size_t) ((m.frames() + (int64_t) bucket - 1) / bucket), rms); };
        auto quiet = source("Quiet", 120, 0, 192), loud = source("Loud", 120, 0, 192);
        quiet->file = report.getSiblingFile("quiet-fixture.wav"); loud->file = report.getSiblingFile("loud-fixture.wav");
        waveform(*quiet, .1f); waveform(*loud, .3f);
        AutoArrangementOptions styled; styled.startingId = quiet->id; styled.trackCount = 2;
        auto rising = planAutoArrangement({quiet, loud}, styled, random);
        check(rising.error.isEmpty() && rising.project.transitionLinks.size() == 1
            && nodeValue(rising.project,0,5,.5) > 0 && nodeValue(rising.project,1,5,.5) < 0 && nodeSwap(rising.project)
            && !rising.project.transitionLinks[0].filter.active() && !rising.project.transitionLinks[0].bands[1],
            "Balanced uses a gentle opening filter and bass handover for measured energy increase");
        bool sparse = rising.error.isEmpty();
        for (const auto& clip : rising.project.clips) for (int target = 0; target < trackAutomationCount; ++target)
            sparse &= clip.controls.automation[(size_t) target].points.size() <= (target == (int) AutomationTarget::cutoff ? 20u : 48u);
        check(sparse,"actual Auto Arrange keeps Filter and all other generated parameters sparse");
        const auto noDefaultFade = [](const Project& project)
        {
            return std::all_of(project.transitionLinks.begin(), project.transitionLinks.end(), [&](const auto& link)
            {
                const auto* out = project.clipFor(link.outgoing); const auto* in = project.clipFor(link.incoming);
                const auto range = project.transitionRange(link); const double mid = range.getStart() + range.getLength() * .5;
                return out && in && out->fadeOut == 0 && in->fadeIn == 0
                    && out->envelope().gainAt(mid - out->start) == 1 && in->envelope().gainAt(mid - in->start) == 1;
            });
        };
        check(noDefaultFade(rising.project), "Balanced replaces placement's default volume crossfade instead of layering bass/filter treatment over it");
        styled.startingId = loud->id;
        auto falling = planAutoArrangement({quiet, loud}, styled, random);
        check(falling.error.isEmpty() && falling.project.transitionLinks.size() == 1 && nodeValue(falling.project,0,5,.5) < 0 && nodeValue(falling.project,1,5,.5) > 0,
            "measured energy decrease chooses the other paired filter direction");
        styled.startingId = quiet->id; styled.transitions = AutoTransitionStyle::bold;
        auto bold = planAutoArrangement({quiet, loud}, styled, random);
        check(bold.error.isEmpty() && bold.project.transitionLinks.size() == 1 && nodeValue(bold.project,0,5,.5) > nodeValue(rising.project,0,5,.5)
            && nodeSwap(bold.project) && nodeValue(bold.project,0,2,.75) < nodeValue(bold.project,0,2,.25),
            "Bold uses stronger filters, tighter bass handover and extra tone fades");
        check(noDefaultFade(bold.project), "Bold has no inherited full-band volume crossfade beneath its selected band/filter treatment");
        styled.transitions = AutoTransitionStyle::smooth;
        auto smooth = planAutoArrangement({quiet, loud}, styled, random);
        check(smooth.error.isEmpty() && smooth.project.transitionLinks.size() == 1 && !smooth.project.transitionLinks[0].filter.active() && !smooth.project.transitionLinks[0].bands[1]
            && nodeFade(smooth.project),
            "Smooth keeps simple volume blends despite available energy evidence");
        styled.transitions = AutoTransitionStyle::balanced; waveform(*loud, .1f);
        auto steady = planAutoArrangement({quiet, loud}, styled, random);
        check(steady.error.isEmpty() && steady.project.transitionLinks.size() == 1 && nodeValue(steady.project,0,5,.5) == 0 && nodeValue(steady.project,1,5,.5) == 0 && nodeSwap(steady.project),
            "steady similar energy uses bass handover without an unnecessary filter sweep");
        loud->waveformRms.pop_back();
        auto missingEnergy = planAutoArrangement({quiet, loud}, styled, random);
        check(missingEnergy.error.isEmpty() && missingEnergy.project.transitionLinks.size() == 1 && !missingEnergy.project.transitionLinks[0].filter.active()
            && !missingEnergy.project.transitionLinks[0].bands[1] && !sourceEnergy(*loud, 0, 10)
            && nodeFade(missingEnergy.project),
            "incomplete waveform stays unknown and falls back to plain fade");
        waveform(*loud, 0);
        check(!planAutoArrangement({quiet, loud}, styled, random).project.transitionLinks[0].filter.active(),
            "silence cannot manufacture energy evidence for effect selection");
        waveform(*loud, .1f); loud->waveformRms[0] = std::numeric_limits<float>::quiet_NaN();
        check(!planAutoArrangement({quiet, loud}, styled, random).project.transitionLinks[0].bands[1],
            "nonfinite values in the examined source window retain the safe fade fallback");
        const auto saved = parseProject(juce::JSON::toString(projectToJson(bold.project)));
        check(saved.error.isEmpty() && saved.project.transitionLinks.size() == 1 && saved.project.transitionLinks[0].filter == bold.project.transitionLinks[0].filter
            && saved.project.transitionLinks[0].bands == bold.project.transitionLinks[0].bands,
            "automatic treatments persist through the existing session format without recomputation");
        check(saved.error.isEmpty() && saved.clips.size() == 2 && saved.clips[0].clip.fadeOut == 0 && saved.clips[1].clip.fadeIn == 0
            && saved.clips[0].clip.fadeOutAnchor == bold.project.clips[0].fadeOutAnchor
            && saved.clips[1].clip.fadeInAnchor == bold.project.clips[1].fadeInAnchor,
            "session parsing preserves replacement fade values and source anchors without restoring the default crossfade");
        waveform(*loud, .3f);
        auto third = source("Third energy track"); waveform(*third, .2f);
        styled.trackCount = 3;
        const auto chain = planAutoArrangement({quiet, loud, third}, styled, random);
        check(chain.error.isEmpty() && chain.project.transitionLinks.size() == 2 && noDefaultFade(chain.project),
            "appending a third track preserves replacement treatment on both sides of the middle track");
        styled.trackCount = 2;
        for (auto choice : {AutoTransitionStyle::constantPower, AutoTransitionStyle::hardCut})
        {
            styled.transitions = choice;
            const auto selected = planAutoArrangement({quiet, loud}, styled, random);
            check(selected.error.isEmpty() && selected.project.transitionLinks.size() == 1 && noDefaultFade(selected.project)
                && selected.project.clips[0].controls.automation[0].active() && !selected.project.transitionLinks[0].bands[0]
                && !selected.project.transitionLinks[0].bands[1] && !selected.project.transitionLinks[0].filter.active(),
                "explicit volume style replaces default fades and adds no automatic bass or filter treatment: " + selected.error + " / " + selected.stopReason);
            if (selected.project.transitionLinks.empty()) continue;
            const auto& p = selected.project; const auto& link = p.transitionLinks[0]; const auto range = p.transitionRange(link);
            const auto gain = [&](int side,double fraction) { const auto db = nodeValue(p,side,0,fraction); return db <= -60 ? 0.f : juce::Decibels::decibelsToGain(db); };
            if (choice == AutoTransitionStyle::constantPower)
            {
                bool power = true;
                for (int i = 0; i <= 20; ++i)
                {
                    const auto aGain = gain(0,i/20.), bGain = gain(1,i/20.);
                    // The bounded dB template approximation is within .002dB.
                    power &= std::abs(aGain * aGain + bGain * bGain - 1) < .0005;
                }
                check(power, "constant-power envelopes preserve summed power throughout the handover for equally loud unrelated sources");
            }
            else
            {
                check(gain(0,.25) == 1 && gain(1,.25) == 0 && gain(0,.75) == 0 && gain(1,.75) == 1,
                    "hard cuts keep only one song audible away from the short midpoint click-prevention ramp");
            }
            const auto document = parseProject(juce::JSON::toString(projectToJson(p)));
            check(document.error.isEmpty() && document.project.transitionLinks[0].bands == link.bands,
                "chosen volume style persists through session serialization");
        }
        styled.transitions = AutoTransitionStyle::balanced; styled.energy = AutoEnergyPolicy::maintain;
        waveform(*quiet, .2f); waveform(*loud, .1f); waveform(*third, .3f);
        activityProfile(*quiet, 2); activityProfile(*loud, 1); activityProfile(*third, 3);
        const auto maintained = planAutoArrangement({quiet, loud, third}, styled, random);
        check(maintained.error.isEmpty() && maintained.project.clips.size() == 2 && maintained.project.clips[1].media == third,
            "Keep pace prefers avoiding a substantial energy drop when a stronger compatible option exists");
        styled.energy = AutoEnergyPolicy::build;
        auto slight = source("Slight increase"); waveform(*slight, .21f);
        activityProfile(*slight, 2.2);
        styled.trackCount = 3;
        const auto built = planAutoArrangement({quiet, slight, third}, styled, random);
        check(built.error.isEmpty() && built.project.clips.size() == 3 && built.project.clips[1].media == slight && built.project.clips[2].media == third,
            "Build energy follows gradual musical activity rather than requiring a recording-level increase");
        styled.trackCount = 2;
        auto slower = source("Louder but slower", 118); waveform(*slower, .4f);
        activityProfile(*slower, 4);
        const auto slowerRejected = planAutoArrangement({quiet, slower}, styled, random);
        check(slowerRejected.project.clips.size() == 2 && slowerRejected.stopReason.isEmpty() && slowerRejected.summary.contains("tempo dips: 1"),
            "Build energy allows and reports a compatible tempo dip instead of truncating a feasible mix");
        styled.energy = AutoEnergyPolicy::windDown;
        const auto winding = planAutoArrangement({quiet, loud, third}, styled, random);
        check(winding.project.clips.size() == 2 && winding.project.clips[1].media == loud,
            "Wind down chooses a lower-energy handover without a tempo rise");
        styled.energy = AutoEnergyPolicy::maintain; loud->waveformRms.clear(); loud->energy = {};
        const auto unavailable = planAutoArrangement({quiet, loud}, styled, random);
        check(unavailable.project.clips.size() == 2 && unavailable.summary.contains("energy unavailable: 1"),
            "missing energy evidence remains usable and is reported without inventing an energy measurement");
        std::vector<std::shared_ptr<Media>> plateau;
        for (int i = 0; i < 10; ++i) { auto m = source("Plateau " + juce::String(i)); waveform(*m, .2f); activityProfile(*m, 2); plateau.push_back(m); }
        styled.energy = AutoEnergyPolicy::build; styled.startingId = plateau.front()->id; styled.trackCount = 10;
        const auto flat = planAutoArrangement(plateau, styled, random);
        check(flat.error.isEmpty() && flat.project.clips.size() == 10 && flat.stopReason.isEmpty() && flat.summary.contains("plateaus/dips: 9"),
            "ten similarly mastered tracks complete a Build mix with honest plateau reporting");
        styled.startingId.clear(); styled.trackCount = 3;
        const auto automatic = planAutoArrangement({third, quiet, slight}, styled, random);
        check(automatic.project.clips.size() == 3 && automatic.project.clips.front().media == quiet && automatic.project.clips.back().media == third,
            "automatic Build start leaves headroom and orders the available progression");
        styled.energy = AutoEnergyPolicy::windDown;
        const auto automaticDown = planAutoArrangement({quiet, third, slight}, styled, random);
        check(automaticDown.project.clips.size() == 3 && automaticDown.project.clips.front().media == third && automaticDown.project.clips.back().media == quiet,
            "automatic Wind down start leaves room for a gradual descent");
        styled.startingId = quiet->id; styled.trackCount = 2;
        for (const auto direction : {AutoEnergyPolicy::unrestricted, AutoEnergyPolicy::maintain, AutoEnergyPolicy::build, AutoEnergyPolicy::windDown})
            for (const auto style : {AutoTransitionStyle::balanced, AutoTransitionStyle::smooth, AutoTransitionStyle::bold,
                                    AutoTransitionStyle::current, AutoTransitionStyle::constantPower, AutoTransitionStyle::hardCut, AutoTransitionStyle::varied})
            {
                auto combination = styled; combination.energy = direction; combination.transitions = style; combination.trackCount = 10;
                combination.startingId = plateau.front()->id;
                const auto full = planAutoArrangement(plateau, combination, random);
                check(full.error.isEmpty() && full.project.clips.size() == 10 && full.project.validateTransitionLinks().wasOk(),
                    "all 28 direction/blend combinations complete a plateau library with valid transition ownership: "
                    + juce::String((int) direction) + "/" + juce::String((int) style));
                combination.startingId = quiet->id; combination.trackCount = 2;
                const auto unknownLevel = planAutoArrangement({quiet, loud}, combination, random);
                check(unknownLevel.error.isEmpty() && unknownLevel.project.clips.size() == 2,
                    "all direction/blend combinations tolerate missing energy: " + juce::String((int) direction) + "/" + juce::String((int) style));
            }
        for (const auto direction : {AutoEnergyPolicy::maintain, AutoEnergyPolicy::build, AutoEnergyPolicy::windDown})
        {
            auto alignment = styled; alignment.energy = direction; alignment.transitions = AutoTransitionStyle::current;
            alignment.preset = TransitionPreset::alignOnly; alignment.sectionTransitions = false; alignment.overlapBeats = 0;
            const auto alignedMix = planAutoArrangement({quiet, third}, alignment, random);
            check(alignedMix.project.clips.size() == 2 && alignedMix.error.isEmpty(),
                "Use my settings with alignment-only and no handover measurements remains feasible for direction " + juce::String((int) direction));
            auto timed = styled; timed.energy = direction; timed.targetSeconds = 300;
            const auto timedMix = planAutoArrangement({quiet, slight, third}, timed, random);
            check(timedMix.error.isEmpty() && timedMix.project.clips.size() > 1 && timedMix.project.duration() <= autoArrangementMaxSeconds,
                "duration mode uses soft progression and retains its hard duration guards for direction " + juce::String((int) direction));
        }
        auto deadEnd = source("Dead end", 120, 21), bridge = source("Route bridge", 120, 0), destination = source("Destination", 120, 7);
        waveform(*deadEnd, .21f); waveform(*bridge, .22f); waveform(*destination, .3f);
        styled.energy = AutoEnergyPolicy::build; styled.trackCount = 4;
        const auto onward = planAutoArrangement({quiet, deadEnd, bridge, destination}, styled, random);
        check(onward.project.clips.size() >= 3, "soft progression avoids an immediate harmonic dead end when an onward route exists");
        auto disconnected = source("Isolated low level", 200, 1); waveform(*disconnected, .01f);
        activityProfile(*disconnected, .1);
        styled.startingId.clear(); styled.trackCount = 3;
        const auto connected = planAutoArrangement({disconnected, quiet, slight, third}, styled, random);
        check(connected.project.clips.size() == 3 && connected.project.clips.front().media == quiet,
            "automatic progression starts in the largest compatible group rather than an isolated energy extreme");
        styled.startingId = quiet->id; styled.trackCount = 2;
        styled.energy = AutoEnergyPolicy::unrestricted;
        check(planAutoArrangement({quiet, loud}, styled, random).project.clips.size() == 2,
            "Unrestricted remains compatible with missing energy evidence");
        auto invalidEnergy = styled; invalidEnergy.energy = static_cast<AutoEnergyPolicy>(99);
        check(planAutoArrangement({quiet, third}, invalidEnergy, random).error.isNotEmpty(), "invalid energy choices reject at the options boundary");
        auto sectionOut = source("Section out", 120, 0, 256), sectionIn = source("Section in", 120, 0, 256);
        waveform(*sectionOut, .1f); waveform(*sectionIn, .1f);
        const auto step = [](Media& m, double seconds)
        { const int bucket = std::max(1, m.frames() / 4000); for (size_t i = 0; i < m.waveformRms.size(); ++i) if (i * bucket / m.sampleRate >= seconds) m.waveformRms[i] = .6f; };
        step(*sectionOut, 160); step(*sectionIn, 32); styled.startingId = sectionOut->id;
        const auto bounded = planAutoArrangement({sectionOut, sectionIn}, styled, random);
        bool within = bounded.error.isEmpty() && bounded.project.clips.size() == 2 && bounded.project.transitionLinks.size() == 1;
        if (within)
        {
            const auto& out = bounded.project.clips[0]; const auto& in = bounded.project.clips[1];
            const auto window = bounded.project.transitionRange(bounded.project.transitionLinks[0]);
            const double outChange = out.start + out.audioTimelineTime(160), inChange = in.start + in.audioTimelineTime(32);
            within = std::abs(outChange - inChange) < 1e-7 && window.contains(outChange)
                && bounded.project.tempo.beatsAt(window.getEnd()) - bounded.project.tempo.beatsAt(window.getStart()) > 128;
        }
        check(within, "Auto Arrange extends a preferred 32-bar limit to retain a genuine shared section change");
        auto invalid = styled; invalid.transitions = static_cast<AutoTransitionStyle>(99);
        check(planAutoArrangement({sectionOut, sectionIn}, invalid, random).error.isNotEmpty(), "unknown blend choices reject at the options boundary");
        std::vector<std::shared_ptr<Media>> variedSources;
        for (int i = 0; i < 6; ++i)
        {
            auto track = source("Varied " + juce::String(i), 120 * std::pow(1.06, i), 0, 192);
            track->file = report.getSiblingFile("varied-" + juce::String(i) + ".wav");
            waveform(*track, .2f); activityProfile(*track, i % 3 == 0 ? 1 : 6); variedSources.push_back(track);
        }
        auto variedOptions = styled; variedOptions.startingId = variedSources.front()->id; variedOptions.trackCount = 6;
        variedOptions.transitions = AutoTransitionStyle::varied; variedOptions.energy = AutoEnergyPolicy::unrestricted;
        const auto variedMix = planAutoArrangement(variedSources, variedOptions, random);
        int filters = 0, bassJoins = 0; bool cooldown = true;
        for (int i = 0; i < variedMix.decisions.size(); ++i)
        {
            if (variedMix.decisions[i].startsWith("Bass handover:")) ++bassJoins;
            if (!variedMix.decisions[i].startsWith("Filter handover:")) continue;
            ++filters;
            for (int j = std::max(0, i - 2); j < i; ++j) cooldown &= !variedMix.decisions[j].startsWith("Filter handover:");
        }
        check(variedMix.error.isEmpty() && variedMix.project.clips.size() == 6 && variedMix.decisions.size() == 5
            && filters >= 1 && bassJoins >= 1 && cooldown && noDefaultFade(variedMix.project),
            "DJ set varies evidence-supported bass/filter treatments with a two-join filter cooldown and no inherited volume fade");
        const auto restoredVariety = parseProject(juce::JSON::toString(projectToJson(variedMix.project)));
        auto restoredProject = restoredVariety.project;
        for (const auto& reference : restoredVariety.clips)
        {
            auto clip = reference.clip;
            const auto media = std::find_if(variedSources.begin(), variedSources.end(), [&](const auto& m) { return m->id == reference.mediaId; });
            if (media != variedSources.end()) clip.media = *media;
            restoredProject.clips.push_back(std::move(clip));
        }
        check(restoredVariety.error.isEmpty() && restoredProject.clips.size() == 6 && restoredProject.validateTransitionLinks().wasOk(),
            "varied transitions remain ordinary editable session nodes with valid ownership: " + restoredVariety.error);
    }
    options.startingId = a->id; options.trackCount = 2; options.sectionTransitions = false; options.overlapBeats = 16; options.preset = TransitionPreset::bassSwap;
    options.transitions = AutoTransitionStyle::current;
    plan = planAutoArrangement({a, b}, options, random);
    check(plan.error.isEmpty() && plan.project.transitionLinks.size() == 1 && nodeSwap(plan.project),
        "auto arrangement reuses selected fixed-length Bass swap rather than imposing Crossfade");

    a->samples.setSize(1, a->frames()); a->samples.clear(); b->samples.setSize(1, b->frames()); b->samples.clear();
    activityProfile(*a, 2); activityProfile(*b, 4); a->assets.genre = "House"; b->assets.genre = "Techno";
    for (auto* m : {a.get(), b.get()})
    { const int bucket = std::max(1, m->frames() / 4000); m->waveformRms.assign((size_t) ((m->frames() + (int64_t) bucket - 1) / bucket), m == a.get() ? .1f : .3f); }
    {
        AutoArrangementOptions compact; compact.startingId = a->id; compact.trackCount = 2;
        const auto generated = planAutoArrangement({a,b},compact,random);
        bool neutral = generated.error.isEmpty(), tidy = neutral;
        for (const auto& clip : generated.project.clips)
        {
            neutral &= clip.controls.automation[0].points.empty();
            const auto& bass = clip.controls.automation[1]; tidy &= bass.points.size() <= 2;
            for (size_t i = 1; i < bass.points.size(); ++i) tidy &= bass.points[i].time-bass.points[i-1].time > 1.e-8;
        }
        check(neutral && tidy,"Balanced Auto Arrange omits neutral Volume and keeps each fresh bass handover to two distinct nodes");
        compact.transitions = AutoTransitionStyle::smooth;
        auto sameBpm = source("Same tempo fade"); sameBpm->samples.setSize(1,sameBpm->frames()); sameBpm->samples.clear();
        const auto smooth = planAutoArrangement({a,sameBpm},compact,random);
        check(smooth.error.isEmpty() && smooth.project.clips.size() == 2
            && smooth.project.clips[0].controls.automation[0].points.size() == 2 && smooth.project.clips[1].controls.automation[0].points.size() == 2,
            "simple automatic crossfades use two endpoints per source without stacked exterior guards");
    }
    {
        c->samples.setSize(1,c->frames()); c->samples.clear();
        auto d = source("Extension fourth",126); d->samples.setSize(1,d->frames()); d->samples.clear();
        for (const auto& m : {a,b,c,d}) m->file = report.getSiblingFile(m->name+".wav");
        AutoArrangementOptions initial; initial.startingId = a->id; initial.trackCount = 2;
        auto prefix = planAutoArrangement({a,b},initial,random).project;
        prefix.name = "Kept arrangement"; prefix.masterAutomation.points = {{0,-3},{200,-2}};
        prefix.tracks[0].automation[0].points = {{0,-5},{200,-4}};
        const auto original = juce::JSON::toString(projectToJson(prefix));
        AutoArrangementOptions extend; extend.extendExisting = true; extend.trackCount = 2;
        const auto added = planAutoArrangement({a,b,c,d,a},extend,random,{},&prefix);
        bool kept = added.error.isEmpty() && added.project.clips.size() == prefix.clips.size()+2;
        if (kept)
        {
            kept &= added.project.name == prefix.name && added.project.transitionLinks.front() == prefix.transitionLinks.front()
                && juce::JSON::toString(projectToJson(added.project)["clips"][0]) == juce::JSON::toString(projectToJson(prefix)["clips"][0])
                && juce::JSON::toString(projectToJson(added.project)["tracks"]) == juce::JSON::toString(projectToJson(prefix)["tracks"])
                && juce::JSON::toString(projectToJson(added.project)["masterAutomation"]) == juce::JSON::toString(projectToJson(prefix)["masterAutomation"]);
            for (size_t i = 0; i < prefix.clips.size(); ++i)
                kept &= added.project.clips[i].start == prefix.clips[i].start && added.project.clips[i].length == prefix.clips[i].length
                    && added.project.clips[i].tempoMap == prefix.clips[i].tempoMap && added.project.clips[i].prepared == prefix.clips[i].prepared;
            for (double t = 0; t <= prefix.duration(); t += .137)
                kept &= std::abs(added.project.tempo.beatsAt(t)-prefix.tempo.beatsAt(t)) < 1.e-8;
            const auto* join = added.project.transitionFor(added.project.clips[prefix.clips.size()].id);
            kept &= join && added.project.transitionRange(*join).getStart() >= prefix.transitionRange(prefix.transitionLinks.back()).getEnd()-1.e-8;
        }
        check(kept && original == juce::JSON::toString(projectToJson(prefix)),
            "extension adds requested unused songs, preserves existing joins/clocks/controls and shortens only the new join to fit the unused tail: " + added.error);
        std::set<juce::String> unique;
        for (const auto& clip : added.project.clips) unique.insert(clip.media->id);
        check(unique.size() == added.project.clips.size() && added.summary.contains("Added: 2 of 2"),"extension excludes existing songs and duplicate library entries, reporting additional count");
        auto prepared = added.project; const auto preparation = prepareAppendedClips(prepared,prefix.clips.size(),0);
        const auto parsedExtension = parseProject(juce::JSON::toString(projectToJson(prepared)));
        check(preparation.isEmpty() && prepared.clips[0].prepared == prefix.clips[0].prepared && parsedExtension.error.isEmpty(),
            "extension prepares only appended audio and validates the saved result: " + preparation + parsedExtension.error);
        const auto exhausted = planAutoArrangement({a,b},extend,random,{},&prefix);
        check(exhausted.error.isEmpty() && juce::JSON::toString(projectToJson(exhausted.project)) == original && exhausted.summary.contains("Added: 0"),
            "extension with no unused songs preserves the complete current document");
        auto locked = prefix; locked.transitionLinks[0].locked = true;
        check(planAutoArrangement({a,b,c},extend,random,{},&locked).error.isNotEmpty(),"extension refuses locked existing transitions");
        auto cancelled = std::make_shared<std::atomic<bool>>(true);
        check(planAutoArrangement({a,b,c},extend,random,cancelled,&prefix).error.isNotEmpty(),"extension cancellation leaves the input arrangement unchanged");
    }
    auto ownedStackFixture_ui = std::make_unique<Workstation>(false); auto& ui = *ownedStackFixture_ui; ui.stopTimer(); ui.setSize(1440, 940); ui.engine.library = {a, b};
    Clip oldClip; oldClip.media = a; oldClip.length = a->duration(); ui.engine.project.clips.push_back(oldClip);
    const auto before = juce::JSON::toString(projectToJson(ui.engine.project));
    ui.refresh();
    auto capture = [&](juce::Component& component, const juce::String& name)
    {
        juce::PNGImageFormat png; auto out = report.getSiblingFile(name + ".png").createOutputStream();
        return out && png.writeImageToStream(component.createComponentSnapshot(component.getLocalBounds()), *out);
    };
    {
        juce::AlertWindow standalone("Mix saved", "You're ready to go.", juce::MessageBoxIconType::InfoIcon);
        standalone.addButton("OK", 0);
        check(standalone.findColour(juce::AlertWindow::backgroundColourId) == colour::panel
            && standalone.findColour(juce::AlertWindow::textColourId) == colour::text
            && standalone.getLookAndFeel().getAlertWindowMessageFont().getHeight() == 13,
            "standalone alerts resolve Alloy colours and body type through the application-owned theme");
        check(capture(standalone, "alloy-alert"), "capture globally themed standalone alert");
    }
    ui.chooseImport(); pumpAutoMessages(50);
    auto* picker = dynamic_cast<juce::FileChooserDialogBox*>(juce::Component::getCurrentlyModalComponent());
    check(picker && picker->getLookAndFeel().findColour(juce::ListBox::backgroundColourId) == colour::panel,
        "audio picker uses the shared Alloy panel theme");
    if (picker) { check(capture(*picker, "alloy-file-picker"), "capture themed audio picker"); picker->exitModalState(0); pumpAutoMessages(30); }
    auto menu = ui.getMenuForIndex(0, "File");
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&ui), [](int) {}); pumpAutoMessages(30);
    if (auto* popup = juce::Component::getCurrentlyModalComponent())
        check(capture(*popup, "alloy-menu"), "capture a real File menu with shared panel and text styling");
    juce::PopupMenu::dismissAllActiveMenus(); pumpAutoMessages(30);
    ui.autoArrangeButton.onClick();
    auto* dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
    check(dialog && dialog->getComboBoxComponent("start")->getNumItems() == 3
        && dialog->getComboBoxComponent("mode")->getNumItems() == 2 && dialog->getComboBoxComponent("transitions")->getNumItems() == 7
        && dialog->getComboBoxComponent("energy")->getNumItems() == 4 && dialog->getComboBoxComponent("genre")->getNumItems() == 3,
        "highlighted Auto Arrange opens a native start/count/duration dialog");
    if (dialog)
    {
        dialog->getComboBoxComponent("mode")->setSelectedItemIndex(1, juce::sendNotificationSync);
        check(dialog->getTextEditorContents("amount") == "60", "duration selection offers minutes through the native dialog");
        auto image = dialog->createComponentSnapshot(dialog->getLocalBounds());
        juce::PNGImageFormat png; auto out = report.getSiblingFile("auto-arrange-dialog.png").createOutputStream();
        check(out && png.writeImageToStream(image, *out), "native Auto Arrange dialog captured for visual verification");
        dialog->exitModalState(0); pumpAutoMessages(30);
    }
    check(before == juce::JSON::toString(projectToJson(ui.engine.project)) && ui.undoHistory.empty(), "dialog cancellation leaves the arrangement and Undo unchanged");
    ui.autoArrangeButton.onClick(); dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
    if (dialog)
    {
        dialog->getComboBoxComponent("action")->setSelectedItemIndex(1,juce::sendNotificationSync);
        check(!dialog->getComboBoxComponent("start")->isEnabled() && dialog->getButton(0)->getButtonText() == "Extend mix",
            "native extension choice disables a new starting song and previews additional-count semantics");
        check(capture(*dialog,"auto-arrange-extend"),"capture native Extend current arrangement option");
        dialog->exitModalState(0); pumpAutoMessages(30);
    }
    ui.transitionIndex = 1; ui.transitionPreset.setSelectedId(2, juce::dontSendNotification);
    ui.autoArrangeButton.onClick();
    dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
    if (dialog)
    {
        dialog->getComboBoxComponent("start")->setSelectedItemIndex(1, juce::dontSendNotification);
        dialog->getComboBoxComponent("energy")->setSelectedItemIndex(2, juce::dontSendNotification);
        dialog->getComboBoxComponent("genre")->setSelectedItemIndex(1, juce::dontSendNotification);
        dialog->getTextEditor("amount")->setText("2"); dialog->exitModalState(1); pumpAutoMessages(30);
    }
    const double deadline = juce::Time::getMillisecondCounterHiRes() + 20000;
    while (ui.busy && juce::Time::getMillisecondCounterHiRes() < deadline) pumpAutoMessages(20);
    check(!ui.busy && ui.engine.project.clips.size() == 2 && ui.undoHistory.size() == 1 && ui.engine.playbackReady(),
        "production Auto Arrange prepares audio in the background and commits one playable Undo step");
    check(ui.attractState.enabled && ui.attractState.motion == AttractMotion::animated && ui.layout.forgeEffects && ui.forgeMotionAllowed(),
        "successful Auto Arrange activates full attract lighting and animated forge effects");
    check(ui.engine.project.transitionLinks.size() == 1 && nodeValue(ui.engine.project,0,5,.5) != 0
        && nodeSwap(ui.engine.project), "background audio preparation preserves the chosen automatic filter and bass treatment");
    check(ui.engine.project.clips[0].fadeOut == 0 && ui.engine.project.clips[1].fadeIn == 0,
        "production Auto Arrange commits the chosen treatment without the default volume crossfade");
    dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
    check(dialog && dialog->getName() == "Auto Arrange complete", "completed auto arrangement displays a native completion report");
    check(ui.status.contains("2 of 2 requested tracks"), "a pending initial mode notification cannot overwrite the caller's entered track count");
    if (dialog)
    {
        juce::PNGImageFormat png; auto out = report.getSiblingFile("auto-arrange-complete.png").createOutputStream();
        check(out && png.writeImageToStream(dialog->createComponentSnapshot(dialog->getLocalBounds()), *out), "capture the complete native arrangement report");
        dialog->exitModalState(0); pumpAutoMessages(30);
    }
    const auto generated = juce::JSON::toString(projectToJson(ui.engine.project));
    ui.undo(false); check(juce::JSON::toString(projectToJson(ui.engine.project)) == before, "Undo restores the complete pre-auto arrangement");
    ui.undo(true); check(juce::JSON::toString(projectToJson(ui.engine.project)) == generated, "Redo restores the prepared auto arrangement");
    const auto history = ui.undoHistory.size();
    ui.startAutoArrangement(options); if (ui.importCancellation) *ui.importCancellation = true;
    check(ui.busy && ui.importProgress.isVisible() && ui.autoArrangeButton.getButtonText() == "Arranging...",
        "background auto arrangement exposes a visible working indicator until completion");
    while (ui.busy && juce::Time::getMillisecondCounterHiRes() < deadline + 20000) pumpAutoMessages(20);
    check(!ui.busy && ui.undoHistory.size() == history && generated == juce::JSON::toString(projectToJson(ui.engine.project)),
        "worker cancellation preserves the current mix and Undo history");
    auto missing = source("Missing source"); missing->file = report.getSiblingFile("missing-auto-source.wav");
    ui.engine.library = {missing}; options.startingId = missing->id;
    ui.startAutoArrangement(options);
    while (ui.busy && juce::Time::getMillisecondCounterHiRes() < deadline + 30000) pumpAutoMessages(20);
    check(!ui.busy && generated == juce::JSON::toString(projectToJson(ui.engine.project)) && ui.undoHistory.size() == history,
        "missing chosen audio rejects transactionally without losing the previous mix");
    dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
    check(dialog && dialog->getName() == "Auto Arrange stopped", "source preparation failure displays a visible stopped report");
    if (dialog) { dialog->exitModalState(0); pumpAutoMessages(30); }
    ui.engine.library = {a, b}; ui.refresh();
    ui.autoArrangeButton.onClick();
    dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
    if (dialog)
    {
        dialog->getComboBoxComponent("mode")->setSelectedItemIndex(1, juce::sendNotificationSync);
        dialog->getTextEditor("amount")->setText("181"); dialog->exitModalState(1); pumpAutoMessages(30);
    }
    check(!ui.busy && generated == juce::JSON::toString(projectToJson(ui.engine.project)) && ui.undoHistory.size() == history,
        "native duration input rejects over 180 minutes without changing the mix");
    ui.engine.playing = true; ui.startAutoArrangement(options);
    check(!ui.busy && ui.engine.project.clips.size() == 2, "auto arrangement cannot replace a playing mix"); ui.engine.playing = false;
    for (const int style : {4, 5})
    {
        ui.engine.library = {a, b}; ui.refresh(); ui.autoArrangeButton.onClick();
        dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        if (dialog)
        {
            dialog->getComboBoxComponent("start")->setSelectedItemIndex(1, juce::dontSendNotification);
            dialog->getComboBoxComponent("transitions")->setSelectedItemIndex(style, juce::dontSendNotification);
            dialog->getComboBoxComponent("energy")->setSelectedItemIndex(1, juce::dontSendNotification);
            dialog->getTextEditor("amount")->setText("2"); dialog->exitModalState(1); pumpAutoMessages(30);
        }
        const auto until = juce::Time::getMillisecondCounterHiRes() + 20000;
        while (ui.busy && juce::Time::getMillisecondCounterHiRes() < until) pumpAutoMessages(20);
        check(!ui.busy && ui.engine.playbackReady() && ui.engine.project.clips.size() == 2 && ui.engine.project.transitionLinks.size() == 1
            && ui.engine.project.clips[0].controls.automation[0].active() && !ui.engine.project.transitionLinks[0].bands[0]
            && ui.engine.project.clips[0].fadeOut == 0 && ui.engine.project.clips[1].fadeIn == 0 && ui.status.contains("Keep pace"),
            "native Auto Arrange dialog passes selected energy and volume style through background preparation into the playable mix");
        dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        if (dialog) { dialog->exitModalState(0); pumpAutoMessages(30); }
    }
    ui.engine.library = {a, b}; ui.refresh(); ui.autoArrangeButton.onClick();
    dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
    if (dialog)
    {
        dialog->getComboBoxComponent("start")->setSelectedItemIndex(1, juce::dontSendNotification);
        dialog->getComboBoxComponent("transitions")->setSelectedItemIndex(6, juce::dontSendNotification);
        dialog->getComboBoxComponent("energy")->setSelectedItemIndex(2, juce::dontSendNotification);
        dialog->getComboBoxComponent("genre")->setText("Trance", juce::dontSendNotification);
        dialog->getTextEditor("amount")->setText("2"); dialog->exitModalState(1); pumpAutoMessages(30);
    }
    const auto variedDeadline = juce::Time::getMillisecondCounterHiRes() + 20000;
    while (ui.busy && juce::Time::getMillisecondCounterHiRes() < variedDeadline) pumpAutoMessages(20);
    check(!ui.busy && ui.engine.playbackReady() && ui.engine.project.clips.size() == 2 && ui.status.contains("Variety:")
        && ui.status.contains("Bass handover") && ui.status.contains("Genre preference: trance") && ui.status.contains("other genres: 2"),
        "native DJ set/genre choices pass through preparation and report a playable technique and genre fallback");
    dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
    if (dialog) { check(capture(*dialog, "auto-arrange-varied-complete"), "capture native varied/genre completion report"); dialog->exitModalState(0); pumpAutoMessages(30); }
    a->energy = {}; b->energy = {};
    const double oldManualBpm = a->manualBpm, oldManualDownbeat = a->manualBeatOrigin; const int oldManualKey = a->manualKey;
    auto upgradeOptions = options; upgradeOptions.startingId = a->id; upgradeOptions.trackCount = 2;
    upgradeOptions.energy = AutoEnergyPolicy::build; upgradeOptions.transitions = AutoTransitionStyle::varied;
    const auto beforeUpgradeHistory = ui.undoHistory.size(); ui.startAutoArrangement(upgradeOptions);
    const auto energyDeadline = juce::Time::getMillisecondCounterHiRes() + 20000;
    while (ui.busy && juce::Time::getMillisecondCounterHiRes() < energyDeadline) pumpAutoMessages(20);
    check(!ui.busy && ui.engine.playbackReady() && ui.engine.project.clips.size() == 2 && a->energy.complete() && b->energy.complete()
        && ui.undoHistory.size() == beforeUpgradeHistory + 1 && a->manualBpm == oldManualBpm && a->manualBeatOrigin == oldManualDownbeat && a->manualKey == oldManualKey
        && ui.status.contains("energy unavailable: 1"), "native Auto Arrange backfills missing profiles asynchronously, preserves corrections and keeps silent activity unknown");
    dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
    if (dialog) { dialog->exitModalState(0); pumpAutoMessages(30); }
    ui.engine.library = {a}; ui.startAutoArrangement(shortageOptions);
    const auto shortageDeadline = juce::Time::getMillisecondCounterHiRes() + 20000;
    while (ui.busy && juce::Time::getMillisecondCounterHiRes() < shortageDeadline) pumpAutoMessages(20);
    dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
    check(!ui.busy && dialog && dialog->getName() == "Auto Arrange complete" && ui.status.contains("1 of 10")
        && ui.status.containsIgnoreCase("no more ready tracks"), "a single-track shortfall visibly reports finished, count and reason rather than appearing to run silently");
    if (dialog)
    {
        juce::PNGImageFormat png; auto out = report.getSiblingFile("auto-arrange-shortfall.png").createOutputStream();
        check(out && png.writeImageToStream(dialog->createComponentSnapshot(dialog->getLocalBounds()), *out), "capture the native single-track shortfall report");
        dialog->exitModalState(0); pumpAutoMessages(30);
    }
    ui.undo(false); ui.engine.library = {a, b}; ui.refresh();
    for (int width : {1250, 1440, 1920, 2560})
    {
        ui.setSize(width, 940);
        check(ui.layout.toolbar.contains(ui.autoArrangeButton.getBounds()) && ui.gridDimmer.getRight() <= ui.autoArrangeButton.getX()
            && ui.autoArrangeButton.getRight() < ui.settingsButton.getX() && ui.autoArrangeButton.isVisible(),
            "Auto Arrange fits between grid dimmer and Audio / MIDI at " + juce::String(width));
        auto image = ui.createComponentSnapshot(ui.getLocalBounds()); juce::PNGImageFormat png;
        auto out = report.getSiblingFile("auto-arrange-" + juce::String(width) + ".png").createOutputStream();
        check(out && png.writeImageToStream(image, *out), "Auto Arrange native toolbar capture at " + juce::String(width));
    }
}
void runAutoArrangementCatalogueProbe(const std::function<void(bool, const juce::String&)>& check, const juce::File& report,
                                     const juce::File& catalogueFile, const juce::String& start, int energySamples, int requestedTracks)
{
    const auto library = readLibraryCatalogue(catalogueFile);
    check(library.error.isEmpty() && !library.media.empty(), "read the supplied catalogue through its production parser without decoding sources");
    if (library.error.isNotEmpty() || library.media.empty()) return;
    juce::Array<juce::var> measured; juce::AudioFormatManager formats; formats.registerBasicFormats();
    std::vector<std::shared_ptr<Media>> available;
    for (const auto& media : library.media) if (autoArrangementEligible(*media) && media->file.existsAsFile()) available.push_back(media);
    energySamples = std::min(energySamples, (int) available.size());
    for (int i = 0; i < energySamples; ++i)
    {
        const auto& media = available[(size_t) i * available.size() / energySamples]; const auto record = media->analysisRecord();
        const auto began = juce::Time::getMillisecondCounterHiRes();
        const auto read = readMedia(formats, media->file, {}, AnalysisCacheMode::readOnly, MediaReadPurpose::energyAnalysis, &record);
        const bool same = read.media && read.media->analysisSource == media->analysisSource
            && read.media->manualBpm == media->manualBpm && read.media->manualBeatOrigin == media->manualBeatOrigin && read.media->manualKey == media->manualKey
            && juce::JSON::toString(analysisRecordToJson(read.media->analysisRecord())["analysis"]) == juce::JSON::toString(analysisRecordToJson(record)["analysis"]);
        check(same && read.media->energy.complete() && read.media->analysisFromCache,
            "real source supplementary analysis reuses exact timing/corrections read-only: " + media->name + " / " + read.error);
        if (!same || !read.media->energy.complete()) continue;
        media->energy = read.media->energy;
        auto* row = new juce::DynamicObject(); row->setProperty("name", media->name); row->setProperty("sourceSha256", media->analysisSource.sha256);
        row->setProperty("milliseconds", juce::Time::getMillisecondCounterHiRes() - began); row->setProperty("profile", musicalEnergyToJson(media->energy));
        measured.add(juce::var(row));
    }
    if (energySamples) check(report.getSiblingFile("real-musical-energy.json").replaceWithText(juce::JSON::toString(measured)), "retain real-source activity measurements without retaining decoded audio");
    AutoArrangementOptions options; options.startingId = start; options.trackCount = requestedTracks > 0 ? requestedTracks : 10;
    juce::Random random(549);
    const auto result = planAutoArrangement(library.media, options, random);
    auto evidence = std::make_unique<juce::DynamicObject>();
    evidence->setProperty("catalogue", catalogueFile.getFullPathName()); evidence->setProperty("libraryTracks", (int) library.media.size());
    evidence->setProperty("eligibleTracks", (int) std::count_if(library.media.begin(), library.media.end(), [](const auto& m) { return autoArrangementEligible(*m); }));
    evidence->setProperty("error", result.error); evidence->setProperty("summary", result.summary);
    juce::Array<juce::var> tracks;
    for (const auto& clip : result.project.clips)
    {
        auto row = std::make_unique<juce::DynamicObject>(); row->setProperty("id", clip.media->id); row->setProperty("name", clip.media->name);
        row->setProperty("bpm", clip.media->bpm()); row->setProperty("key", clip.media->key()); row->setProperty("keyEvidenceScore", clip.media->analysis.keyConfidence);
        row->setProperty("start", clip.start); row->setProperty("length", clip.length); tracks.add(juce::var(row.release()));
    }
    evidence->setProperty("tracks", tracks);
    report.getSiblingFile("auto-catalogue-probe.json").replaceWithText(juce::JSON::toString(juce::var(evidence.release())));
    check(result.error.isEmpty(), "production auto planner accepts the supplied library and explicit starting track: " + result.error);
    check(result.project.clips.size() > 1, "supplied library can produce a multi-track arrangement: " + result.summary.replaceCharacters("\r\n", "  "));
    if (requestedTracks > 0)
    {
        check((int) result.project.clips.size() == requestedTracks, "focused catalogue planning completes the requested count: " + result.summary.replaceCharacters("\r\n", "  "));
        const auto links = result.project.validateTransitionLinks();
        const auto parsed = parseProject(juce::JSON::toString(projectToJson(result.project)));
        check(links.wasOk() && parsed.error.isEmpty(),
            "focused catalogue plan has valid links and saved geometry without decoding audio: " + links.getErrorMessage() + " / " + parsed.error);
        report.getSiblingFile("auto-catalogue-project.json").replaceWithText(juce::JSON::toString(projectToJson(result.project)));
        return;
    }
    juce::Array<juce::var> combinations;
    for (int direction = 0; direction < 4; ++direction)
        for (int style = 0; style < 7; ++style)
        {
            auto selected = options; selected.energy = (AutoEnergyPolicy) direction; selected.transitions = (AutoTransitionStyle) style;
            juce::Random seeded(549);
            const auto began = juce::Time::getMillisecondCounterHiRes();
            const auto planned = planAutoArrangement(library.media, selected, seeded);
            auto item = std::make_unique<juce::DynamicObject>();
            item->setProperty("direction", direction); item->setProperty("style", style);
            item->setProperty("milliseconds", juce::Time::getMillisecondCounterHiRes() - began);
            item->setProperty("summary", planned.summary); item->setProperty("error", planned.error);
            item->setProperty("decisions", planned.decisions.joinIntoString("\n"));
            juce::Array<juce::var> sequence;
            for (const auto& clip : planned.project.clips)
            {
                auto row = std::make_unique<juce::DynamicObject>(); row->setProperty("id", clip.media->id);
                row->setProperty("name", clip.media->name); row->setProperty("bpm", clip.media->bpm());
                row->setProperty("key", clip.media->key()); sequence.add(juce::var(row.release()));
            }
            item->setProperty("tracks", sequence); combinations.add(juce::var(item.release()));
            check(planned.error.isEmpty() && planned.project.clips.size() > 1 && planned.project.validateTransitionLinks().wasOk(),
                "catalogue direction/blend " + juce::String(direction) + "/" + juce::String(style) + ": " + planned.summary.replaceCharacters("\r\n", "  "));
        }
    check(report.getSiblingFile("auto-catalogue-combinations.json").replaceWithText(juce::JSON::toString(combinations)),
        "retain actual catalogue sequences and timing for all 28 direction/blend combinations");
}
}
