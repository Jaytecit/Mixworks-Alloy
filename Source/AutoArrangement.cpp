#include "AutoArrangement.h"
#include "Beatmatch.h"
#include "EnergySections.h"
#include <cmath>
#include <set>

namespace mixworks
{
namespace
{
// Mean source powers in the outgoing head/tail and incoming head/tail halves
// of the actual placed frame. Both treatment selection and energy admission
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
bool meetsAutoEnergy(const std::array<double, 4>& power, AutoEnergyPolicy policy)
{
    const double threshold = policy == AutoEnergyPolicy::build ? std::pow(10., .1) : policy == AutoEnergyPolicy::windDown ? std::pow(10., -.1) : 1.;
    for (size_t half = 0; half < 2; ++half)
    {
        const double out = power[half], in = power[half + 2];
        if (policy == AutoEnergyPolicy::windDown ? in > out * threshold : in + 1e-12 < out * threshold) return false;
    }
    return true;
}
juce::Result styleAutoTransition(Project& project, AutoTransitionStyle choice)
{
    if (choice == AutoTransitionStyle::current || project.clips.size() < 2) return juce::Result::ok();
    const auto& incoming = project.clips.back();
    const auto* link = project.transitionFor(incoming.id);
    if (!link) return juce::Result::fail("The next blend could not be prepared.");
    TransitionStyle style; style.volume = {0, 1, 0, 1};
    if (choice == AutoTransitionStyle::hardCut || choice == AutoTransitionStyle::constantPower)
    {
        style.volume = {0, 0, 1, 1};
        style.bands[0] = TransitionBandSettings {choice == AutoTransitionStyle::hardCut ? TransitionBandMode::swap : TransitionBandMode::constantPower, 1, 0, .5};
        return project.applyTransitionStyle(incoming.id, style);
    }
    if (choice != AutoTransitionStyle::smooth)
    {
        // RMS indicates measured source energy, never perceived loudness,
        // vocals, instruments or named song sections.
        if (const auto power = autoTransitionEnergy(project, *link))
        {
            // The chosen treatment replaces placement's provisional crossfade.
            // Keep a plain volume fade only for Smooth or unknown energy.
            style.volume = {0, 0, 1, 1};
            const bool bold = choice == AutoTransitionStyle::bold;
            const double threshold = bold ? 1.5 : 3.;
            const auto db = [](double a, double b) { return 10 * std::log10(std::max(a, 1e-10) / std::max(b, 1e-10)); };
            const double change = db((*power)[2] + (*power)[3], (*power)[0] + (*power)[1]);
            const double rise = db((*power)[3], (*power)[2]), fall = db((*power)[1], (*power)[0]);
            // Hand the low end over once; tighter ramps suit a busier, bolder mix.
            style.bands[1] = TransitionBandSettings {TransitionBandMode::swap, 1, 0, .5};
            if (change >= threshold || rise >= threshold)
                style.filter = {TransitionFilterMode::lowToHigh, bold ? .65 : .35};
            else if (change <= -threshold || fall <= -threshold)
                style.filter = {TransitionFilterMode::highToLow, bold ? .65 : .35};
            if (bold)
            {
                style.bands[2] = TransitionBandSettings {TransitionBandMode::crossfade, .3, 0, .5};
                style.bands[3] = TransitionBandSettings {TransitionBandMode::crossfade, .25, 0, .5};
            }
        }
    }
    return project.applyTransitionStyle(incoming.id, style);
}
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
AutoArrangementResult planAutoArrangement(const std::vector<std::shared_ptr<Media>>& library, const AutoArrangementOptions& options,
                                         juce::Random& random, const std::shared_ptr<std::atomic<bool>>& cancelled)
{
    AutoArrangementResult result;
    if (options.trackCount < 1 || (options.targetSeconds && (!std::isfinite(*options.targetSeconds)
        || *options.targetSeconds <= 0 || *options.targetSeconds > autoArrangementMaxSeconds))
        || !std::isfinite(options.overlapBeats) || options.overlapBeats < 0 || options.overlapBeats > 256
        || options.preset < TransitionPreset::alignOnly || options.preset > TransitionPreset::crossfade
        || options.transitions < AutoTransitionStyle::balanced || options.transitions > AutoTransitionStyle::hardCut
        || options.energy < AutoEnergyPolicy::unrestricted || options.energy > AutoEnergyPolicy::windDown)
    { result.error = "Choose a positive track count or a duration up to 180 minutes."; return result; }
    const auto preset = options.transitions == AutoTransitionStyle::current ? options.preset : TransitionPreset::crossfade;
    const bool sections = options.transitions == AutoTransitionStyle::current ? options.sectionTransitions : true;
    const double overlap = std::min(128., options.overlapBeats);
    auto stopped = [&] { return cancelled && cancelled->load(); };
    std::vector<std::shared_ptr<Media>> pool;
    std::set<juce::String> ids; std::set<juce::File> files;
    for (const auto& media : library)
        if (media && autoArrangementEligible(*media) && !ids.count(media->id)
            && (media->file == juce::File() || !files.count(media->file)))
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
    size_t start = (size_t) random.nextInt((int) pool.size());
    if (options.startingId.isNotEmpty())
    {
        const auto found = std::find_if(pool.begin(), pool.end(), [&](const auto& m) { return m->id == options.startingId; });
        if (found == pool.end()) { result.error = "The first track is unavailable or needs its beat grid, speed and key checked."; return result; }
        start = (size_t) std::distance(pool.begin(), found);
    }
    const size_t limit = options.targetSeconds ? pool.size() : std::min(pool.size(), (size_t) options.trackCount);
    result.project.name = "Auto arrangement";
    result.project.bpm = result.project.tempo.fallback = pool[start]->bpm();
    std::set<juce::String> used;
    auto append = [&](const std::shared_ptr<Media>& media)
    {
        auto p = result.project;
        Clip clip; clip.media = media; clip.length = media->duration(); clip.start = p.duration();
        clip.track = (int) p.clips.size() % trackCount; clip.pendingPlacement = PendingPlacement::append;
        clip.pendingTransitionPreset = preset; clip.pendingTransitionBeats = overlap;
        clip.pendingSectionTransition = sections;
        p.clips.push_back(std::move(clip));
        auto planned = planTempoMap(std::move(p), overlap, preset, sections, 128);
        if (planned.error.isEmpty()) planned.error = styleAutoTransition(planned.project, options.transitions).getErrorMessage();
        return planned;
    };
    auto first = append(pool[start]);
    if (first.error.isNotEmpty() || !std::isfinite(first.project.duration()) || first.project.duration() > autoArrangementMaxSeconds)
    { result.error = first.error.isNotEmpty() ? first.error : "The starting track exceeds the three-hour limit."; return result; }
    result.project = std::move(first.project); used.insert(pool[start]->id);
    while (result.project.clips.size() < limit && (!options.targetSeconds || result.project.duration() < *options.targetSeconds))
    {
        const auto& previous = *result.project.clips.back().media;
        std::vector<std::pair<double, std::shared_ptr<Media>>> ranked;
        for (const auto& m : pool)
        {
            if (used.count(m->id) || !harmonicallyCompatible(previous.key(), m->key())) continue;
            const double distance = std::abs(std::log(m->bpm() / previous.bpm()));
            if (distance > std::log(1.08) + 1e-9) continue;
            ranked.emplace_back(distance + (previous.key() == m->key() ? 0.0 : .01) + random.nextDouble() * .002, m);
        }
        std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        if (ranked.empty())
        {
            result.stopReason = "No unused tracks match the last track's speed and key.";
            break;
        }
        bool added = false;
        int capRejected = 0, targetRejected = 0, energyRejected = 0, energyUnknown = 0;
        for (const auto& candidate : ranked)
        {
            if (stopped()) { result.error = "Auto arrangement cancelled. The current mix was kept."; return result; }
            if (((options.energy == AutoEnergyPolicy::maintain || options.energy == AutoEnergyPolicy::build) && candidate.second->bpm() < previous.bpm())
                || (options.energy == AutoEnergyPolicy::windDown && candidate.second->bpm() > previous.bpm()))
            { ++energyRejected; continue; }
            auto next = append(candidate.second);
            const double duration = next.project.duration();
            if (next.error.isNotEmpty() || !std::isfinite(duration) || duration <= result.project.duration() + .001)
            { continue; }
            if (options.energy != AutoEnergyPolicy::unrestricted)
            {
                const auto* link = next.project.transitionFor(next.project.clips.back().id);
                const auto energy = link ? autoTransitionEnergy(next.project, *link) : std::nullopt;
                if (!energy) { ++energyUnknown; continue; }
                if (!meetsAutoEnergy(*energy, options.energy)) { ++energyRejected; continue; }
            }
            if (duration > autoArrangementMaxSeconds + 1e-7) { ++capRejected; continue; }
            if (options.targetSeconds && std::abs(duration - *options.targetSeconds) > std::abs(result.project.duration() - *options.targetSeconds))
            { ++targetRejected; continue; }
            result.project = std::move(next.project); used.insert(candidate.second->id); added = true; break;
        }
        if (!added)
        {
            result.stopReason = capRejected > 0 ? "The next track would take the mix over 180 minutes."
                : targetRejected > 0 ? "Another track would move the mix farther from your chosen length."
                : energyRejected || energyUnknown ? "No remaining match fits your energy choice. Rejected by measured energy or tempo: " + juce::String(energyRejected)
                    + "; energy unavailable: " + juce::String(energyUnknown) + "."
                : "The next matching tracks need more room for a blend.";
            break;
        }
    }
    if (stopped()) { result.error = "Auto arrangement cancelled. The current mix was kept."; return result; }
    const auto count = juce::String((int) result.project.clips.size());
    result.summary = "Finished: " + count + (options.targetSeconds ? " tracks" : " of " + juce::String(options.trackCount) + " requested tracks")
        + " / " + timeText(result.project.duration()) + ".";
    if (options.targetSeconds) result.summary += " Requested length: " + juce::String(*options.targetSeconds / 60, 1) + " minutes (whole tracks).";
    if ((!options.targetSeconds && result.project.clips.size() < (size_t) options.trackCount)
        || (options.targetSeconds && result.project.duration() < *options.targetSeconds))
    {
        if (result.stopReason.isEmpty()) result.stopReason = "No more ready tracks in your library.";
        result.summary += "\nStopped: " + result.stopReason;
    }
    result.summary += "\nUndo brings back your previous mix.";
    const std::array<const char*, 4> policies {"Unrestricted", "Keep pace (no measured energy or BPM drop)", "Build energy (at least +1 dB; no BPM drop)", "Wind down (at least -1 dB; no BPM rise)"};
    result.summary += "\nEnergy: " + juce::String(policies[(size_t) options.energy]) + ". Uses source RMS in both halves of each handover, not a perceived-loudness rating.";
    return result;
}
}
