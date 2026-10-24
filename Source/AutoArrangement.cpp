#include "AutoArrangement.h"
#include "Beatmatch.h"
#include "EnergySections.h"
#include <cmath>
#include <limits>
#include <set>

namespace mixworks
{
namespace
{
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
// Only for a newly generated arrangement: there are no user-authored nodes to
// preserve. Omit inaudible frame guards and redundant flat/straight padding.
void compactAutoNodes(Project& project, size_t first = 0)
{
    for (size_t index = first; index < project.clips.size(); ++index)
    {
        auto& clip = project.clips[index];
        // Legacy paired endpoints are owned by their relationship, not freely
        // removable curve padding.
        if (std::any_of(project.transitionLinks.begin(),project.transitionLinks.end(),[&](const auto& link)
            { return link.bassSwap && (link.incoming == clip.id || link.outgoing == clip.id); })) continue;
        const auto playback = project.transitionPlaybackFor(clip);
        const double begin = std::max(0.,clip.sourceElapsed(playback.startCut.value_or(clip.start)-clip.start));
        const double end = clip.sourceElapsed(playback.endCut.value_or(clip.end())-clip.start);
        if (end <= begin) continue;
        for (int target = 0; target < trackAutomationCount; ++target)
        {
            auto& curve = clip.controls.automation[(size_t) target]; if (curve.points.empty()) continue;
            const float manual = clip.controls.manualValue((AutomationTarget) target);
            const auto original = curve;
            curve = original.sliced(begin,end,manual);
            // At hard audible edges use the inside value, not a private
            // exterior guard a few rounding units outside the frame.
            constexpr double edgeTolerance = 1.e-9;
            curve.points.front().value = original.interpolatedValue(begin + std::min(edgeTolerance,(end-begin)*.25),manual);
            curve.points.back().value = original.interpolatedValue(end - std::min(edgeTolerance,(end-begin)*.25),manual);
            for (size_t i = 1; i+1 < curve.points.size();)
                if (curve.points[i].time < edgeTolerance || curve.points[i].time > end-begin-edgeTolerance)
                    curve.points.erase(curve.points.begin()+(int) i);
                else ++i;
            for (auto& point : curve.points) point.time += begin;
            auto& points = curve.points;
            for (size_t i = 1; i+1 < points.size();)
            {
                const auto& a = points[i-1]; const auto& b = points[i]; const auto& c = points[i+1];
                const bool flat = a.value == b.value && (b.value == c.value || b.bend >= 1);
                const auto level = [](const AutomationPoint& p, bool gain)
                { return gain ? (p.value <= -160 ? 0. : juce::Decibels::decibelsToGain((double) p.value)) : (double) p.value; };
                const bool straight = a.bend == 0 && b.bend == 0 && a.gainSpace == b.gainSpace
                    && std::abs(level(b,a.gainSpace) - (level(a,a.gainSpace)+(level(c,a.gainSpace)-level(a,a.gainSpace))
                        *(b.time-a.time)/(c.time-a.time))) < 1.e-8;
                if (flat || straight)
                {
                    if (flat) { points[i-1].bend = b.bend; points[i-1].exponential = b.exponential; points[i-1].gainSpace = b.gainSpace; }
                    points.erase(points.begin()+(int) i);
                    if (i > 1) --i;
                }
                else ++i;
            }
            while (points.size() > 1 && points[0].value == points[1].value) points.erase(points.begin());
            while (points.size() > 1 && points.back().value == points[points.size()-2].value) points.pop_back();
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
    TransitionStyle style; style.volume = {0, 1, 0, 1};
    if (choice == AutoTransitionStyle::varied)
    {
        const auto activity = autoTransitionActivity(project, *link);
        decision = "Phrase blend: musical activity unavailable; conservative fade";
        if (activity)
        {
            const double change = ((*activity)[2] + (*activity)[3] - (*activity)[0] - (*activity)[1]) * .5;
            const double density = (*std::max_element(activity->begin(), activity->end()));
            bool recentFilter = false;
            for (int i = std::max(0, history.size() - 2); i < history.size(); ++i) recentFilter |= history[i].startsWith("Filter handover:");
            const auto* outgoing = project.clipFor(link->outgoing);
            const auto range = project.transitionRange(*link);
            const auto bassAt = [&](const Clip& clip)
            {
                const double a = clip.offset + clip.audioSourceElapsed(range.getStart() - clip.start);
                const double b = clip.offset + clip.audioSourceElapsed(range.getEnd() - clip.start);
                double value = 0, weight = 0;
                for (const auto& w : clip.media->energy.windows)
                { const double span = std::max(0., std::min(b, w.end) - std::max(a, w.start)); value += span * w.bass(); weight += span; }
                return weight > 0 ? value / weight : 0.;
            };
            const bool bassPressure = outgoing && bassAt(*outgoing) > .08 && bassAt(incoming) > .08;
            decision = "Phrase blend: restrained compatible join";
            if (bassPressure)
            {
                style.volume = {0, 0, 1, 1};
                style.bands[1] = TransitionBandSettings{TransitionBandMode::swap, 1, 0, sectionSwapPosition(project, *link)};
                decision = "Bass handover: rhythmic low-end activity on both sources";
            }
            if (density > .5 && std::abs(change) > .1 && !recentFilter)
            {
                style.filter = {change > 0 ? TransitionFilterMode::lowToHigh : TransitionFilterMode::highToLow, .3};
                decision = "Filter handover: activity change; restrained depth and two-join cooldown";
            }
            const auto clock = project.gridClock();
            if (!bassPressure && !style.filter.active() && clock.beatsAt(range.getEnd()) - clock.beatsAt(range.getStart()) <= 32.001)
                decision = "Quick phrase blend: short feasible handover without competing rhythmic bass";
        }
        return project.applyTransitionStyle(incoming.id, style);
    }
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
            style.bands[1] = TransitionBandSettings {TransitionBandMode::swap, 1, 0, sectionSwapPosition(project, *link)};
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
MatchResult prepareOrderedTimeline(Project input, int64_t existing)
{
    if (input.hasLockedTransitions() || input.hasPendingPlacement()) return {input, "Unlock transitions and finish source preparation before mixing the timeline."};
    if (input.clips.size() < 2) return {input, "Place at least two songs on the timeline."};
    auto order = input.clips;
    std::stable_sort(order.begin(), order.end(), [](const auto& a, const auto& b) { return a.start < b.start; });
    Project result = input; result.clips.clear(); result.transitionLinks.clear(); result.transitionLinksKnown = true;
    result.tempo = {}; result.tempo.origin = input.tempo.origin; result.tempo.fallback = input.bpm;
    juce::StringArray history;
    for (auto clip : order)
    {
        if (!clip.media || !clip.media->hasPlayback()) return {input, "A timeline source is unavailable."};
        // Captured geometry determines the retained source span. Rebuild placement
        // only; source-owned analysis, review choices and stem controls survive.
        clip.length *= clip.rate; clip.rate = 1; clip.prepared.reset(); clip.stemPrepared = {};
        clip.tempoMap.reset(); clip.tempoSourceBpm = 0; clip.elasticSource.reset(); clip.elasticPlayback = false;
        clip.start = 0; clip.pendingPlacement = PendingPlacement::append; clip.pendingAutoTransition = true;
        auto candidate = result; candidate.clips.push_back(clip);
        auto planned = planTempoMap(candidate, 128, TransitionPreset::crossfade, true);
        bool musical = planned.error.isEmpty();
        if (!musical)
        {
            // A bounded blend also works for short songs and uncertain timing.
            const double bpm = clip.media->bpm() > 0 ? clip.media->bpm() : result.bpm;
            const double span = std::min(clip.length, result.clips.empty() ? clip.length : result.clips.back().length);
            const double beats = std::min(32., std::max(.01, span * bpm / 120.));
            planned = planTempoMap(candidate, beats, TransitionPreset::crossfade, false);
        }
        if (planned.error.isNotEmpty()) return {input, planned.error};
        if (musical && planned.project.clips.size() > 1)
        {
            auto styled = planned.project; juce::String decision;
            if (styleAutoTransition(styled, AutoTransitionStyle::balanced, history, decision).wasOk())
            { planned.project = std::move(styled); history.add(decision); }
        }
        result = std::move(planned.project);
    }
    auto prepared = prepareTempoMap(std::move(result), false, 32, existing, 0, {}, TransitionPreset::crossfade, false);
    if (prepared.error.isNotEmpty()) return {input, prepared.error};
    return prepared;
}
AutoArrangementResult planAutoArrangement(const std::vector<std::shared_ptr<Media>>& library, const AutoArrangementOptions& requested,
                                         juce::Random& random, const std::shared_ptr<std::atomic<bool>>& cancelled, const Project* existing)
{
    AutoArrangementResult result;
    auto options = requested;
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
    const bool sections = options.transitions == AutoTransitionStyle::current ? options.sectionTransitions : true;
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
    auto append = [&](const std::shared_ptr<Media>& media)
    {
        auto p = result.project;
        Clip clip; clip.media = media; clip.length = media->duration(); clip.start = p.duration(); clip.hotcuesPending = true;
        clip.track = extending ? ((p.clips.size() == prefix ? tail->track : p.clips.back().track)+1)%trackCount : (int) p.clips.size()%trackCount;
        clip.pendingPlacement = PendingPlacement::append; clip.pendingAutoTransition = true;
        double preferredOverlap = overlap;
        if (options.transitions == AutoTransitionStyle::varied && !p.clips.empty())
        {
            const auto out = musicalEnergyBody(p.clips.back().media->energy), in = musicalEnergyBody(media->energy);
            if (out && in && *in > *out + .15) preferredOverlap = std::min(32., overlap);
        }
        clip.pendingTransitionPreset = preset; clip.pendingTransitionBeats = preferredOverlap;
        clip.pendingSectionTransition = sections;
        p.clips.push_back(std::move(clip));
        // Guard all established source clocks. New tempo targets may begin
        // only after their audio; placement can still overlap the last song.
        auto planned = planTempoMap(p, preferredOverlap, preset, sections, 128, extending ? std::optional<double>(0.) : std::nullopt);
        if (extending && result.project.clips.size() == prefix)
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
                    planned = planTempoMap(p,shorter,preset,false,128,0.);
                    newJoin = planned.error.isEmpty() ? planned.project.transitionFor(planned.project.clips.back().id) : nullptr;
                }
                if (!newJoin || planned.project.transitionRange(*newJoin).getStart() < protectedEnd - 1.e-8)
                    planned.error = "The last song has no unused tail for a new handover after its existing transition.";
            }
        }
        if (extending && planned.error.isEmpty())
        {
            auto& incoming = planned.project.clips.back(); bool free = false;
            for (int offset = 1; offset < trackCount; ++offset)
            {
                const int lane = (result.project.clips.size() == prefix ? tail->track : result.project.clips.back().track) + offset;
                incoming.track = lane % trackCount;
                if (std::none_of(planned.project.clips.begin(),std::prev(planned.project.clips.end()),[&](const auto& c)
                    { return c.track == incoming.track && incoming.start < c.end()-1.e-9 && incoming.end() > c.start+1.e-9; }))
                { free = true; break; }
            }
            if (!free) planned.error = "No free lane fits the next handover without overlapping existing audio.";
        }
        candidateDecision.clear();
        if (planned.error.isEmpty()) planned.error = styleAutoTransition(planned.project, options.transitions, result.decisions, candidateDecision).getErrorMessage();
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
            double score = candidate.first;
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
    result.summary = (extending ? "Added: " : "Finished: ") + count + (options.targetSeconds ? " tracks" : " of " + juce::String(options.trackCount) + " requested tracks")
        + " / " + timeText(result.project.duration()) + ".";
    if (options.targetSeconds) result.summary += " Requested " + juce::String(extending ? "additional length: " : "length: ")
        + juce::String(*requested.targetSeconds / 60, 1) + " minutes (whole tracks).";
    if ((!options.targetSeconds && result.project.clips.size()-prefix < (size_t) options.trackCount)
        || (options.targetSeconds && result.project.duration() < *options.targetSeconds))
    {
        if (result.stopReason.isEmpty()) result.stopReason = "No more ready tracks in your library.";
        result.summary += "\nStopped: " + result.stopReason;
    }
    result.summary += "\nUndo brings back your previous mix.";
    if (extending) result.summary += " Existing joins and source timing kept; the new handover uses the last song's tail.";
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
    if (!result.decisions.isEmpty())
    {
        std::map<juce::String, int> counts;
        for (const auto& decision : result.decisions) ++counts[decision.upToFirstOccurrenceOf(":", false, false)];
        result.summary += "\nVariety: ";
        for (const auto& item : counts) result.summary += item.first + " " + juce::String(item.second) + "; ";
    }
    return result;
}
}
