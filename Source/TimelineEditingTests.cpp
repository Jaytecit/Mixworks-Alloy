#include "Workstation.h"
#include "Beatmatch.h"
#include <cmath>
#include <limits>
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace mixworks
{
void runPrerollTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runTransitionBandTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runTransitionFilterTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runTransitionNavigationTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runRehearsalContextTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runAudioSlipTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runTransitionCutTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runTransitionSwapTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runBarNavigationTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runTransitionRehearsalTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runSectionFlagTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runSourceSectionTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runPresetNodeTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runArrangementInsertionTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runHotcueTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runLaunchTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runLiveHotcueTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runEditSelectionTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runTimelineEditingTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    // Keep historical endpoint-pair editing covered independently of new sharp presets.
    auto legacyBassFixture = [](const Project& source)
    {
        auto result = prepareTempoMap(source, true, 16, 0, 0, {}, TransitionPreset::bassSwap);
        if (result.error.isEmpty()) for (auto& clip : result.project.clips) { clip.controls.automation[1] = {}; clip.automaticBass = false; }
        if (result.error.isEmpty()) for (auto& link : result.project.transitionLinks)
        {
            link.bands[1].reset();
            auto* out = result.project.clipFor(link.outgoing); auto* in = result.project.clipFor(link.incoming);
            const auto range = result.project.transitionRange(link);
            link.bassSwap = initialiseTransition(*out, *in, range.getLength(), TransitionPreset::bassSwap, range);
        }
        return result;
    };

    Project input;
    for (int n = 0; n < 3; ++n)
    {
        auto media = std::make_shared<Media>(); media->sampleRate = 8000; media->manualBpm = n == 0 ? 120 : n == 1 ? 128 : 124; media->manualBeatOrigin = n == 1 ? 1.213 : 0.213;
        media->name = "Anchor fixture " + juce::String(n + 1); media->file = report.getSiblingFile("anchor-" + juce::String(n) + ".wav");
        media->samples.setSize(1, 8000 * 48);
        for (int i = 0; i < media->frames(); ++i) media->samples.setSample(0, i, 0.3f * (float) std::sin(i * 0.04));
        Clip clip; clip.media = media; clip.length = 48; clip.track = n; input.clips.push_back(clip);
    }
    auto ready = prepareTempoMap(input, true, 16, 0, 0, {}, TransitionPreset::alignOnly);
    check(ready.error.isEmpty(), "three actual PCM tracks prepare their two tempo transitions: " + ready.error);
    if (ready.error.isNotEmpty()) return;
    const auto original = ready.project;
    const auto controlBase = prepareTempoMap(input, true, 32, 0, 0, {}, TransitionPreset::alignOnly).project;
    {
        auto local = controlBase; const auto first = local.transitionLinks[0], neighbour = local.transitionLinks[1];
        const auto before = local; const auto begin = local.transitionRange(first).getStart();
        check(local.setTransitionLength(first.incoming, 4).wasOk(), "selected pair length edits without rebuilding the arrangement");
        const auto range = local.transitionRange(*local.transitionFor(first.incoming));
        check(std::abs(range.getStart() - begin) < 1e-7 && std::abs(local.tempo.beatsAt(range.getEnd()) - local.tempo.beatsAt(begin) - 16) < 1e-7,
            "selected length measures four bars through the actual changing tempo map and keeps the left boundary");
        bool placementKept = local.transitionLinks[1] == neighbour;
        for (size_t i = 0; i < local.clips.size(); ++i) placementKept &= local.clips[i].start == before.clips[i].start
            && local.clips[i].prepared == before.clips[i].prepared && local.clips[i].tempoMap == before.clips[i].tempoMap;
        check(placementKept && local.clips[2].fadeIn == before.clips[2].fadeIn, "local length preserves every placement, prepared PCM, clock identity and neighbouring pair");
        local.clips[0].controls.automation[2].enabled = true; local.clips[0].controls.automation[2].setPoint(2, -3);
        const auto bassPlaced = local.applyTransitionPreset(first.incoming, TransitionPreset::bassSwap);
        check(bassPlaced.wasOk() && !local.transitionFor(first.incoming)->bands[1]
            && std::any_of(local.clips[0].controls.automation[1].points.begin(),local.clips[0].controls.automation[1].points.end(),[](const auto& point) { return point.bend == 1; }),
            "selected Bass swap creates a vertical ordinary-node handoff: " + bassPlaced.getErrorMessage());
        check(local.clips[0].fadeOut == 0 && local.clips[1].fadeIn == 0 && local.clips[0].controls.automation[2].points[0].value == -3,
            "local Bass swap retains the bass-only contract and unrelated manual mid automation");
        check(local.applyTransitionPreset(first.incoming, TransitionPreset::crossfade).wasOk() && !local.transitionFor(first.incoming)->bassSwap
            && local.clips[1].controls.automation[1].valueAt(local.clips[1].sourceElapsed((range.getStart()+range.getEnd())*.5-local.clips[1].start),0) == 0,
            "switching Bass swap to Crossfade replaces the interior bass nodes without leaving an incoming mute");
        check(local.clips[0].controls.automation[0].active() && local.clips[1].controls.automation[0].active() && local.transitionLinks[1] == neighbour,
            "local Crossfade sets both ordinary fades and preserves the other relationship");
        check(local.applyTransitionPreset(first.incoming, TransitionPreset::alignOnly).wasOk() && local.clips[0].fadeOut == 0 && local.clips[1].fadeIn == 0
            && std::abs(local.transitionRange(*local.transitionFor(first.incoming)).getEnd() - range.getEnd()) < 1e-7,
            "None clears pair effects while keeping the frame length and alignment");
        const auto frozen = juce::JSON::toString(projectToJson(local));
        check(local.setTransitionLength(first.incoming, 100000).failed() && juce::JSON::toString(projectToJson(local)) == frozen,
            "an unavailable selected length rejects atomically instead of moving or trimming songs");
        check(local.setTransitionLength(first.incoming, std::numeric_limits<double>::quiet_NaN()).failed(), "nonfinite selected lengths reject");
        auto manual = local; manual.clips[1].controls.automation[1].setPoint(manual.clips[1].sourceElapsed(begin - manual.clips[1].start + 1), -4);
        check(manual.applyTransitionPreset(first.incoming, TransitionPreset::bassSwap).wasOk() && !manual.transitionFor(first.incoming)->bands[1],
            "explicit local Bass swap replaces the frame's manual shape with ordinary nodes");
        check(local.setTransitionLocked(first.incoming, true).wasOk(), "transition lock is owned by the relationship");
        const auto locked = juce::JSON::toString(projectToJson(local),true,17);
        check(local.editTransition(first.incoming, Project::TransitionEdit::moveWindow, -1, false).failed()
            && local.setTransitionLength(first.incoming, 8).failed() && local.applyTransitionPreset(first.incoming, TransitionPreset::crossfade).failed()
            && locked == juce::JSON::toString(projectToJson(local),true,17), "locked local edits all reject without a partial mutation");
        check(local.moveClip(first.incoming, local.clips[1].start + 1, false).failed()
            && local.moveTransitionAnchor(first.outgoing, false, begin - 1, false).failed(), "locked tracks reject direct and linked marker movement");
        check(local.editTransition(neighbour.incoming, Project::TransitionEdit::moveOutgoing, 1, false).failed(),
            "an adjacent transition cannot move the shared locked song");
        auto automated = local; automated.clips[1].controls.automation[2].setPoint(1, -7);
        check(local.validateTransitionLocks(automated).failed(), "candidate commits cannot change a locked participant through neighbouring automation");
        auto tempo = local; tempo.tempo.points[0].bpm += 1;
        check(local.validateTransitionLocks(tempo).failed(), "protected transition rejects a changed global tempo map");
        const auto rebuilt = prepareTempoMap(local, true, 16, 0);
        check(rebuilt.error.isNotEmpty() && juce::JSON::toString(projectToJson(rebuilt.project),true,17) == locked, "global Beatmatch cannot erase transition locks");
        const auto parsed = parseProject(locked); auto loaded = parsed.project;
        for (const auto& reference : parsed.clips) loaded.clips.push_back(reference.clip);
        check(parsed.error.isEmpty() && loaded.transitionFor(first.incoming) && loaded.transitionFor(first.incoming)->locked,
            "relationship locks and ordinary frame effects round-trip with production precision: " + parsed.error);
        auto bad = projectToJson(local); (*bad.getProperty("transitionLinks", {}).getArray())[0].getDynamicObject()->setProperty("locked", "yes");
        check(parseProject(juce::JSON::toString(bad)).error.isNotEmpty(), "malformed transition lock rejects instead of becoming unlocked");
        auto legacy = projectToJson(local); legacy.getDynamicObject()->setProperty("version", 14); legacy["tracks"].getArray()->removeRange(4, trackCount - 4);
        for (auto& item : *legacy.getProperty("transitionLinks", {}).getArray()) item.getDynamicObject()->removeProperty("locked");
        const auto older = parseProject(juce::JSON::toString(legacy,true,17));
        check(older.error.isEmpty() && !older.project.hasLockedTransitions(), "format 14 loads without inventing locks: " + older.error);
        check(local.setTransitionLocked(first.incoming, false).wasOk() && local.setTransitionLength(first.incoming, 4).wasOk(), "explicit unlock restores local editing");
    }
    {
        const auto swapped = legacyBassFixture(input);
        for (const auto edit : {Project::TransitionEdit::moveWindow, Project::TransitionEdit::resizeStart, Project::TransitionEdit::resizeEnd,
             Project::TransitionEdit::moveIncoming, Project::TransitionEdit::moveOutgoing})
        {
            auto boxed = swapped.project; const auto edge = boxed.transitionLinks[0];
            const auto before = boxed.transitionRange(edge); const double outStart = boxed.clips[0].start, inStart = boxed.clips[1].start;
            const auto changedBox = boxed.editTransition(edge.incoming, edit, edit == Project::TransitionEdit::moveOutgoing ? 1 : -1, false);
            const auto after = boxed.transitionRange(boxed.transitionLinks[0]);
            check(changedBox.wasOk() && boxed.validateTransitionLinks().wasOk(),
                "transition box action " + juce::String((int) edit) + " retains valid paired bass ownership: " + changedBox.getErrorMessage());
            if (changedBox.failed()) continue;
            const bool song = edit == Project::TransitionEdit::moveIncoming || edit == Project::TransitionEdit::moveOutgoing;
            check(song ? std::abs(before.getStart() - after.getStart()) < 1e-7 && std::abs(before.getEnd() - after.getEnd()) < 1e-7
                : edit == Project::TransitionEdit::moveWindow ? std::abs(after.getStart() - before.getStart() + 1) < 1e-7 && std::abs(after.getEnd() - before.getEnd() + 1) < 1e-7
                : edit == Project::TransitionEdit::resizeStart ? std::abs(after.getEnd() - before.getEnd()) < 1e-7
                : std::abs(after.getStart() - before.getStart()) < 1e-7,
                "frame, edge and song controls move independently (action " + juce::String((int) edit) + ")");
            if (song) check(edit == Project::TransitionEdit::moveIncoming ? boxed.clips[0].start == outStart && boxed.clips[1].start < inStart
                : boxed.clips[0].start > outStart && std::abs(boxed.clips[1].start - inStart) < 1e-7,
                "song movement changes the selected song while retaining its partner's placement");
            check(boxed.clips[1].envelope().gainAt(after.getStart() - boxed.clips[1].start - .001) == 0
                && boxed.clips[0].envelope().gainAt(after.getEnd() - boxed.clips[0].start + .001) == 0,
                "the transition boundaries mute incoming preroll and outgoing tail");
            const auto saved = parseProject(juce::JSON::toString(projectToJson(boxed)));
            check(saved.error.isEmpty(), "transition frame, song cues and paired bass positions persist: " + saved.error);
        }
        auto rejected = swapped.project; const auto snapshot = juce::JSON::toString(projectToJson(rejected));
        check(rejected.editTransition(rejected.transitionLinks[0].incoming, Project::TransitionEdit::resizeEnd, -1000, false).failed()
            && snapshot == juce::JSON::toString(projectToJson(rejected)), "an inverted transition box rejects the entire edit atomically");
    }
    {
        const auto swapped = legacyBassFixture(input);
        check(swapped.error.isEmpty() && swapped.project.transitionLinks.size() == 2 && swapped.project.transitionLinks[0].bassSwap.has_value(),
            "bass-swap preset owns both source-clock endpoint pairs");
        if (swapped.error.isEmpty() && !swapped.project.transitionLinks.empty() && swapped.project.transitionLinks[0].bassSwap)
        {
            auto paired = swapped.project; const auto link = paired.transitionLinks[0];
            auto endpointTime = [](const Project& p, size_t edge, int side, int endpoint)
            { const auto& relationship = p.transitionLinks[edge]; const auto* clip = p.clipFor(side == 0 ? relationship.outgoing : relationship.incoming);
              return clip->start + clip->localTimeForSource((*relationship.bassSwap)[(size_t) side * 2 + endpoint]); };
            check(std::abs(endpointTime(paired, 0, 0, 0) - endpointTime(paired, 0, 1, 0)) < 1e-7
                && std::abs(endpointTime(paired, 0, 0, 1) - endpointTime(paired, 0, 1, 1)) < 1e-7,
                "unequal source tempos share the exact bass handoff interval");
            const auto& clock = paired.tempo;
            check(std::abs(std::remainder((clock.beatsAt(endpointTime(paired, 0, 0, 0)) + clock.beatsAt(endpointTime(paired, 0, 0, 1))) / 2, 4)) < 1e-7,
                "bass preset selects a shared bar boundary inside the overlap, including a changing tempo map");
            for (int side : {0, 1})
            {
                const auto id = side == 0 ? link.outgoing : link.incoming; const auto* clip = paired.clipFor(id);
                const auto& points = clip->controls.automation[1].points; const double source = (*paired.transitionLinks[0].bassSwap)[(size_t) side * 2];
                const int point = (int) std::distance(points.begin(), std::find_if(points.begin(), points.end(), [&](const auto& p) { return std::abs(p.time - source) < 1e-7; }));
                const auto result = paired.moveAutomationPoint({clip->track, AutomationTarget::bass, {}, {}, id}, point, endpointTime(paired, 0, side, 0) + .02, side == 0 ? -1.f : -59.f);
                check(result.wasOk() && std::abs(endpointTime(paired, 0, 0, 0) - endpointTime(paired, 0, 1, 0)) < 1e-7,
                    "editing " + juce::String(side == 0 ? "outgoing" : "incoming") + " bass time moves its paired endpoint");
            }
            const auto restored = parseProject(juce::JSON::toString(projectToJson(paired)));
            check(restored.error.isEmpty() && restored.project.transitionLinks.size() == 2 && restored.project.transitionLinks[0].bassSwap.has_value(),
                "session round trip retains paired bass ownership: " + restored.error);
            auto malformed = projectToJson(paired); malformed["transitionLinks"][0]["bassSwap"].getArray()->set(0, -1);
            check(parseProject(juce::JSON::toString(malformed)).error.isNotEmpty(), "malformed paired bass coordinates reject atomically");
            const auto moved = paired.moveClip(link.incoming, paired.clipFor(link.incoming)->start + 2, false);
            check(moved.wasOk() && std::abs(endpointTime(paired, 0, 0, 0) - endpointTime(paired, 0, 1, 0)) < 1e-7,
                "whole-track movement retains paired bass timing: " + moved.getErrorMessage());
            auto gesture = paired; gesture.moveClip(link.incoming, gesture.clipFor(link.incoming)->start + 1, false);
            auto& live = paired.clipFor(link.outgoing)->controls.automation[1];
            live.setPoint((*paired.transitionLinks[0].bassSwap)[0], -7); live.setPoint(.2, -3);
            const auto applied = paired.applyPlacements(gesture);
            const auto& retained = paired.clipFor(link.outgoing)->controls.automation[1];
            check(applied.wasOk() && std::abs(retained.valueAt((*paired.transitionLinks[0].bassSwap)[0], 0) + 7) < 1e-5
                && std::abs(retained.valueAt(.2, 0) + 3) < 1e-5, "geometry commit preserves concurrent paired values and unrelated live EQ edits");
            const auto* edited = paired.clipFor(link.incoming); const AutomationAddress address {edited->track, AutomationTarget::bass, {}, {}, edited->id};
            paired.automationEdited(address);
            check(paired.transitionLinks[0].bassSwap.has_value(), "ordinary EQ edits preserve explicit bass pairing");
            paired.automationEdited(address, AutomationEdit::removePoint, (*paired.transitionLinks[0].bassSwap)[2]);
            check(!paired.transitionLinks[0].bassSwap && paired.transitionLinks[1].bassSwap.has_value(),
                "deleting a paired endpoint detaches only its owner and preserves the other transition");
        }
        auto plain = input; for (auto& clip : plain.clips) { clip.pendingPlacement = PendingPlacement::append; clip.pendingTransitionPreset = TransitionPreset::alignOnly; }
        plain.clips[0].outroAnchor = 31;
        plain.clips[0].pendingPlacement = PendingPlacement::none;
        const auto aligned = prepareTempoMap(plain, false, 32, 0, 0, {}, TransitionPreset::crossfade);
        check(aligned.error.isEmpty() && aligned.project.clips[0].outroAnchor == plain.clips[0].outroAnchor
            && std::abs(aligned.project.clips[0].start + aligned.project.clips[0].transitionTime(false)
                - aligned.project.clips[1].start - aligned.project.clips[1].transitionTime(true)) < 1e-7,
            "insertion intent retains chosen alignment-only preset and pairs new IN with existing OUT");
        check(aligned.error.isEmpty() && std::all_of(aligned.project.clips.begin(), aligned.project.clips.end(), [](const auto& c)
            { return c.fadeIn == 0 && c.fadeOut == 0 && !c.controls.automation[1].active(); }),
            "alignment-only insertion creates no EQ or volume transition");
        const auto pastOut = prepareTempoMap(plain, false, 32, 0, 0, 35.0, TransitionPreset::alignOnly);
        check(pastOut.error.isNotEmpty() && plain.clips[0].outroAnchor == std::optional<double>(31),
            "a live insertion cannot silently claim an IN/OUT pair when the chosen OUT is behind playback");
        const auto faded = prepareTempoMap(input, true, 16, 0, 0, {}, TransitionPreset::crossfade);
        check(faded.error.isEmpty() && faded.project.clips[0].controls.automation[0].active() && faded.project.clips[1].controls.automation[0].active()
            && !faded.project.clips[0].controls.automation[1].active(), "crossfade preset creates bounded fades without a bass swap");
    }
    auto insertion = input; for (auto& c : insertion.clips) c.pendingPlacement = PendingPlacement::append;
    const auto inserted = prepareTempoMap(insertion, false, 32);
    const auto& firstInserted = inserted.project.clips.front();
    check(inserted.error.isEmpty() && firstInserted.start >= 0 && firstInserted.offset == input.clips.front().offset
          && std::abs(firstInserted.length * firstInserted.rate - 48) < 1.0e-7
          && std::abs(std::remainder(inserted.project.tempo.beatsAt(firstInserted.start + firstInserted.firstDownbeatTime()), 4)) < 1.0e-7,
          "automatic first insertion retains its complete pickup and aligns the musical bar to the timeline: " + inserted.error);
    check(std::all_of(original.clips.begin(), original.clips.end(), [](const auto& c) { return c.fadeIn == 0 && c.fadeOut == 0; }),
          "automatic transitions never add volume fades");
    auto close = [](double a, double b) { return std::abs(a - b) < 1.0e-7; };
    auto alignment = original;
    const auto& outgoing = original.clips[0]; const auto& next = original.clips[1];
    alignment.moveTransitionAnchor(outgoing.id, false, outgoing.start + outgoing.transitionTime(false) - 2, false);
    check(alignment.clips[1].start < next.start
          && close(alignment.clips[0].start + alignment.clips[0].transitionTime(false), alignment.clips[1].start + alignment.clips[1].transitionTime(true)),
          "OUT moves the next track so its IN stays aligned");
    alignment = original;
    alignment.moveTransitionAnchor(next.id, true, next.start + 4, false);
    check(alignment.clips[1].start < next.start
          && close(alignment.clips[0].start + alignment.clips[0].transitionTime(false), alignment.clips[1].start + alignment.clips[1].transitionTime(true)),
          "IN selects later source audio and moves its track under the preceding OUT");
    auto linked = [&](const Project& project)
    {
        auto ordered = project.clips;
        std::stable_sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) { return a.start < b.start; });
        for (size_t i = 1; i < ordered.size(); ++i)
            if (!close(ordered[i - 1].start + ordered[i - 1].transitionTime(false), ordered[i].start + ordered[i].transitionTime(true))) return false;
        return true;
    };
    auto preserved = [&](const Project& project)
    {
        auto normalized = project; normalized.snap = original.snap;
        for (size_t i = 0; i < normalized.clips.size(); ++i)
        {
            auto& n = normalized.clips[i]; const auto& before = original.clips[i];
            const auto oldFade = before.envelope(), newFade = n.envelope();
            if (!close(n.length * n.rate, before.length * before.rate)
                || !close(n.sourceElapsed(newFade.intro + n.fadeIn) - n.sourceElapsed(newFade.intro), before.sourceElapsed(oldFade.intro + before.fadeIn) - before.sourceElapsed(oldFade.intro))
                || !close(n.sourceElapsed(newFade.outro + n.fadeOut) - n.sourceElapsed(newFade.outro), before.sourceElapsed(oldFade.outro + before.fadeOut) - before.sourceElapsed(oldFade.outro))) return false;
            n.start = before.start; n.length = before.length; n.rate = before.rate;
            n.fadeIn = before.fadeIn; n.fadeOut = before.fadeOut; n.elasticPlayback = before.elasticPlayback;
            n.introAnchor = before.introAnchor; n.outroAnchor = before.outroAnchor;
        }
        return juce::JSON::toString(projectToJson(normalized)) == juce::JSON::toString(projectToJson(original));
    };
    const auto& a = original.clips.front(); const auto& incoming = original.clips[1];
    auto stationary = original;
    check(stationary.moveTransitionAnchor(incoming.id, true, incoming.start + incoming.transitionTime(true) + 0.01, true).wasOk()
          && juce::JSON::toString(projectToJson(stationary)) == juce::JSON::toString(projectToJson(original))
          && stationary.clips[1].prepared == incoming.prepared, "a snapped no-op retains omitted anchors, placement and prepared PCM");
    auto firstIntro = original;
    check(firstIntro.moveTransitionAnchor(a.id, true, a.start + 4, false).wasOk()
          && close(firstIntro.clips[0].transitionTime(true), 4) && firstIntro.clips[0].start == a.start && preserved(firstIntro),
          "first unpaired IN can move into its track without cutting its audio");
    auto uncertain = original; uncertain.clips[1].beatmatched = false;
    check(uncertain.moveTransitionAnchor(a.id, false, a.start + 8, true).wasOk()
          && !uncertain.clips[1].beatmatched && linked(uncertain), "uncertain neighbors still move into alignment without acquiring a matched claim");
    const double oldOut = a.start + a.transitionTime(false), wanted = oldOut - 2;
    auto changed = original;
    check(changed.moveTransitionAnchor(a.id, false, wanted, false).wasOk() && preserved(changed) && linked(changed)
          && changed.clips[0].start == a.start && changed.clips[1].start < incoming.start && changed.clips[2].start < original.clips[2].start,
          "OUT repositions both following tracks while preserving sources, fades, tempo and other settings");
    changed = original;
    const double wantedIntroSource = incoming.sourceElapsed(4);
    check(changed.moveTransitionAnchor(incoming.id, true, incoming.start + 4, false).wasOk()
          && close(changed.clips[1].introAnchor.value_or(-1), wantedIntroSource) && changed.clips[1].start < incoming.start
          && linked(changed) && preserved(changed) && changed.clips[0].start == a.start,
          "IN selects later source material and slides its track under the fixed previous OUT, carrying later tracks");
    auto unordered = original; std::swap(unordered.clips[0], unordered.clips[2]);
    check(unordered.moveTransitionAnchor(a.id, false, wanted, false).wasOk() && linked(unordered)
          && unordered.clips[1].start < incoming.start && unordered.clips[2].start == a.start,
          "alignment follows timeline adjacency even when playlist insertion order differs");
    auto unlinked = original; unlinked.clips[1].introAnchor = unlinked.clips[1].sourceElapsed(1);
    check(unlinked.moveTransitionAnchor(a.id, false, wanted, false).wasOk() && linked(unlinked),
          "an existing unaligned IN is brought to the preceding OUT when its handle is moved");
    auto finalOut = original;
    check(finalOut.moveTransitionAnchor(original.clips[2].id, false, original.clips[2].start + 2, false).wasOk()
          && finalOut.clips[2].start == original.clips[2].start && preserved(finalOut), "last unpaired OUT moves without cutting audio or shifting earlier tracks");
    auto beforeZero = original; const auto zeroSnapshot = juce::JSON::toString(projectToJson(beforeZero));
    check(beforeZero.moveTransitionAnchor(incoming.id, true, incoming.end(), false).failed()
          && zeroSnapshot == juce::JSON::toString(projectToJson(beforeZero)), "a linked IN requiring a negative track start rejects the entire edit");
    Engine beforeMarkers, afterMarkers; beforeMarkers.project = afterMarkers.project = changed;
    beforeMarkers.project.transitionLinks.clear(); afterMarkers.project.transitionLinks.clear(); // Unpaired cue markers do not define cutoffs.
    for (auto& clip : beforeMarkers.project.clips) { clip.introAnchor.reset(); clip.outroAnchor.reset(); }
    beforeMarkers.sampleRate = afterMarkers.sampleRate = 8000;
    bool identicalMarkers = true; juce::MidiBuffer markerMidi;
    for (const auto& clip : changed.clips) for (double local : {0.01, 2.0, 6.0, clip.length - 0.1})
    {
        juce::AudioBuffer<float> before(2, 256), after(2, 256);
        beforeMarkers.render(before, 256, clip.start + local, markerMidi);
        afterMarkers.render(after, 256, clip.start + local, markerMidi);
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 256; ++i)
            identicalMarkers &= before.getSample(ch, i) == after.getSample(ch, i);
    }
    check(identicalMarkers, "unpaired IN/OUT cues leave production mixer PCM unchanged when markers are cleared");
    check(changed.clips[0].prepared == original.clips[0].prepared && changed.clips[1].elasticPlayback && changed.clips[1].elasticSource,
          "handle edits retain stationary prepared PCM and use immediate elastic playback for repositioned tracks");
    auto append = original;
    auto extra = input.clips[0]; extra.id = juce::Uuid().toString(); extra.track = 3; extra.pendingPlacement = PendingPlacement::append;
    append.clips.push_back(extra);
    auto markedAppend = append;
    check(markedAppend.moveTransitionAnchor(a.id, false, a.start + 2, false).failed()
          && markedAppend.clips[0].outroAnchor == a.outroAnchor, "pending placement rejects marker edits without partial changes");
    markedAppend.clips[2].outroAnchor = markedAppend.clips[2].sourceElapsed(2);
    const auto normalAppend = prepareTempoMap(append, false, 16), movedAppend = prepareTempoMap(markedAppend, false, 16);
    check(normalAppend.error.isEmpty() && movedAppend.error.isEmpty()
          && close(normalAppend.project.clips.back().start, movedAppend.project.clips.back().start)
          && normalAppend.project.tempo.points == movedAppend.project.tempo.points,
          "later append placement uses full track duration regardless of alignment markers: " + normalAppend.error + " / " + movedAppend.error);
    auto snapped = original;
    snapped.moveTransitionAnchor(a.id, false, a.start + a.transitionTime(false) - 1.37, true);
    check(preserved(snapped) && linked(snapped) && std::abs(std::remainder(snapped.tempo.beatsAt(snapped.clips[0].start + snapped.clips[0].transitionTime(false)), 1)) < 1.0e-7,
          "OUT snaps to the shared timeline beat and carries the linked IN while preserving fades and tempo settings");
    auto manualPair = original;
    manualPair.moveTransitionAnchor(a.id, false, a.start + 20.137, false);
    manualPair.moveTransitionAnchor(incoming.id, true, manualPair.clips[1].start + 2.173, false);
    for (int selected = 0; selected < 3; ++selected) for (bool snap : {false, true})
    {
        auto moved = manualPair; const auto id = moved.clips[(size_t) selected].id;
        const double requested = moved.clips[(size_t) selected].start + 3.137;
        const auto result = moved.moveClip(id, requested, snap);
        bool retained = result.wasOk() && linked(moved) && moved.tempo.points == manualPair.tempo.points;
        for (size_t i = 0; i < moved.clips.size(); ++i)
            retained &= moved.clips[i].introAnchor == manualPair.clips[i].introAnchor && moved.clips[i].outroAnchor == manualPair.clips[i].outroAnchor
                && close(moved.clips[i].length * moved.clips[i].rate, manualPair.clips[i].length * manualPair.clips[i].rate);
        const auto& clip = moved.clips[(size_t) selected];
        if (snap) retained &= std::abs(std::remainder(moved.tempo.beatsAt(clip.start)
            - manualPair.tempo.beatsAt(manualPair.clips[(size_t) selected].start), 1)) < 1.0e-7;
        else retained &= close(clip.start, requested);
        check(retained, "moving linked track " + juce::String(selected) + (snap ? " with Snap" : " freely") + " retains every manual source cue, joins both transitions and moves the partners: " + result.getErrorMessage());
    }
    auto crossed = manualPair; std::swap(crossed.clips[0], crossed.clips[2]);
    check(crossed.moveClip(incoming.id, crossed.clipFor(incoming.id)->start + 5, false).wasOk()
          && close(crossed.clipFor(a.id)->start + crossed.clipFor(a.id)->transitionTime(false), crossed.clipFor(incoming.id)->start + crossed.clipFor(incoming.id)->transitionTime(true)),
          "transition identities survive reordered clip storage while moving their linked group");
    auto legacyPair = manualPair; legacyPair.transitionLinks.clear(); legacyPair.transitionLinksKnown = true;
    legacyPair.clips[1].moveTo(legacyPair.clips[1].start + 0.057);
    check(legacyPair.moveClip(incoming.id, legacyPair.clips[1].start + 3, true).wasOk() && linked(legacyPair)
          && legacyPair.clips[0].outroAnchor == manualPair.clips[0].outroAnchor && legacyPair.clips[1].introAnchor == manualPair.clips[1].introAnchor,
          "moving a legacy manually marked pair joins a previous offset without discarding either chosen source cue");
    auto magnetic = manualPair;
    const double partnerPulse = incoming.media->bpm() > 0 ? manualPair.clips[1].start + manualPair.clips[1].firstBeatTime() + 8 * 60 / incoming.media->bpm() : 30;
    const double magneticRequest = partnerPulse - manualPair.clips[0].firstBeatTime() + 0.001;
    check(magnetic.moveClip(a.id, magneticRequest, true, 0.2).wasOk() && linked(magnetic)
          && std::abs(std::remainder(magnetic.tempo.beatsAt(magnetic.clips[0].start + magnetic.clips[0].firstBeatTime()), 1)) < 1.0e-7,
          "a moving partner cannot attract the group's Snap target away from the shared beat grid");
    auto negative = manualPair; const auto negativeBefore = juce::JSON::toString(projectToJson(negative));
    check(negative.moveClip(incoming.id, 0, false).failed() && juce::JSON::toString(projectToJson(negative)) == negativeBefore,
          "a linked move requiring a partner before zero rejects atomically");
    auto linksJson = projectToJson(manualPair);
    check(parseProject(juce::JSON::toString(linksJson)).project.transitionLinks == manualPair.transitionLinks, "saved sessions retain explicit transition partners");
    linksJson["transitionLinks"][0].getDynamicObject()->setProperty("incoming", "missing");
    check(parseProject(juce::JSON::toString(linksJson)).error.isNotEmpty(), "missing transition owners are rejected before media loading");
    auto cycle = projectToJson(manualPair); auto* cycleLink = new juce::DynamicObject();
    cycleLink->setProperty("outgoing", manualPair.clips.back().id); cycleLink->setProperty("incoming", manualPair.clips.front().id);
    cycle["transitionLinks"].getArray()->add(juce::var(cycleLink));
    check(parseProject(juce::JSON::toString(cycle)).error.isNotEmpty(), "cyclic transition relationships are rejected before playback preparation");
    auto branch = projectToJson(manualPair); branch["transitionLinks"][1].getDynamicObject()->setProperty("outgoing", manualPair.clips.front().id);
    check(parseProject(juce::JSON::toString(branch)).error.isNotEmpty(), "one OUT cannot silently acquire two incoming track owners");
    auto invalid = changed; const auto beforeInvalid = juce::JSON::toString(projectToJson(invalid));
    check(invalid.moveTransitionAnchor(a.id, true, std::numeric_limits<double>::quiet_NaN(), false).failed()
          && invalid.moveTransitionAnchor("missing", true, 4, false).failed()
          && beforeInvalid == juce::JSON::toString(projectToJson(invalid)), "invalid marker edits leave the project intact");
    auto retempo = changed; retempo.tempo.points.back().bpm = 132;
    auto rebuilt = prepareTempoMap(retempo, false, 16);
    check(rebuilt.error.isEmpty() && rebuilt.project.clips[1].introAnchor == changed.clips[1].introAnchor
          && rebuilt.project.clips[0].outroAnchor == changed.clips[0].outroAnchor,
          "tempo edits retain independent source alignment markers");
    auto serial = projectToJson(changed); const auto parsed = parseProject(juce::JSON::toString(serial));
    check(parsed.error.isEmpty() && close(parsed.clips[1].clip.introAnchor.value_or(-1), changed.clips[1].introAnchor.value_or(-2))
          && close(parsed.clips[0].clip.outroAnchor.value_or(-1), changed.clips[0].outroAnchor.value_or(-2)), "sessions retain exact source alignment markers: " + parsed.error);
    auto bad = juce::JSON::parse(juce::JSON::toString(serial)); bad["clips"][0].getDynamicObject()->setProperty("introAnchor", "invalid");
    check(parseProject(juce::JSON::toString(bad)).error.isNotEmpty(), "malformed alignment markers are rejected before source loading");
    bad = juce::JSON::parse(juce::JSON::toString(serial)); bad["clips"][0].getDynamicObject()->setProperty("fadeInAnchor", "invalid");
    check(parseProject(juce::JSON::toString(bad)).error.isNotEmpty(), "malformed independent fade positions are rejected");
    serial.getDynamicObject()->setProperty("version", 9); serial["tracks"].getArray()->removeRange(4, trackCount - 4);
    check(parseProject(juce::JSON::toString(serial)).error.isEmpty(), "version 9 source markers remain readable with alignment-only semantics");
    serial.getDynamicObject()->setProperty("version", 8); serial["tracks"].getArray()->removeRange(4, trackCount - 4);
    check(!parseProject(juce::JSON::toString(serial)).clips[0].clip.outroAnchor, "older sessions retain clip-edge fades");
    auto fading = changed.clips[1]; fading.fadeIn = 4; fading.fadeOut = 3;
    fading.controls = {}; const double cut = fading.envelope().intro + fading.fadeIn * 0.35;
    auto left = fading.slice(0, cut), right = fading.slice(cut, fading.length - cut);
    bool fadePreserved = true;
    for (double t = 0; t < fading.length; t += 0.003)
        fadePreserved &= std::abs(fading.envelope().gainAt(t) - (t < cut ? left.envelope().gainAt(t) : right.envelope().gainAt(t - cut))) < 1.0e-6;
    check(fadePreserved && right.fadeInAnchor && *right.fadeInAnchor < 0, "splitting inside a transition preserves the entire audible fade, including a fade beginning before the right segment");
    auto sliced = changed; sliced.clips = {left, right}; sliced.clips[1].id = juce::Uuid().toString(); sliced.tempo.detachMissingTracks(sliced.clips);
    sliced.transitionLinks.clear(); sliced.transitionLinksKnown = true; // Isolated split-fade fixture, without the other original tracks.
    check(parseProject(juce::JSON::toString(projectToJson(sliced))).error.isEmpty(), "sliced fades and off-segment cue positions survive session validation");
    auto splitParsed = parseProject(juce::JSON::toString(projectToJson(sliced)));
    bool restoredFades = splitParsed.error.isEmpty();
    for (size_t i = 0; restoredFades && i < sliced.clips.size(); ++i)
    {
        auto restored = splitParsed.clips[i].clip; restored.media = sliced.clips[i].media;
        restored.tempoMap = sliced.clips[i].tempoMap; restored.tempoSourceBpm = sliced.clips[i].tempoSourceBpm;
        for (double t = 0; t < restored.length; t += 0.01)
            restoredFades &= std::abs(restored.envelope().gainAt(t) - sliced.clips[i].envelope().gainAt(t)) < 1.0e-6;
    }
    check(restoredFades, "session round trip preserves split fade gain independently of alignment markers");
    Engine wholeAudio, splitAudio; wholeAudio.sampleRate = splitAudio.sampleRate = 8000;
    wholeAudio.project = changed; splitAudio.project = sliced; wholeAudio.project.clips = {fading};
    wholeAudio.project.transitionLinks.clear(); // Both isolated render fixtures have no external partners.
    wholeAudio.project.masterDb = splitAudio.project.masterDb = 0;
    bool samePcm = true; juce::MidiBuffer midi;
    for (int block = 0; block < 5; ++block)
    {
        juce::AudioBuffer<float> whole(2, 512), parts(2, 512);
        const double time = fading.start + cut - 0.12 + block * 512 / 8000.0;
        wholeAudio.render(whole, 512, time, midi); splitAudio.render(parts, 512, time, midi);
        for (int i = 0; i < 512; ++i) samePcm &= std::abs(whole.getSample(0, i) - parts.getSample(0, i)) < 1.0e-5;
    }
    check(samePcm, "production mixer PCM remains unchanged when splitting through an anchored fade");

    auto ownedUi = std::make_unique<Workstation>(false); auto& ui = *ownedUi;
    ui.stopTimer(); ui.setSize(1440, 940); ui.engine.project = original;
    for (const auto& clip : original.clips) ui.engine.library.push_back(clip.media);
    ui.timeline.pixelsPerSecond = 12; ui.selectedClip = a.id; ui.refresh();
    {
        ui.engine.project = manualPair; ui.engine.project.snap = false; ui.refresh();
        auto expected = ui.engine.project; expected.snap = true;
        ui.snapButton.setToggleState(true, juce::sendNotificationSync);
        check(juce::JSON::toString(projectToJson(ui.engine.project)) == juce::JSON::toString(projectToJson(expected)),
              "enabling Snap through its native callback changes no clip, source cue, link or tempo geometry");
        ui.engine.project = original; ui.refresh();
    }
    const auto mouse = juce::Desktop::getInstance().getMainMouseSource();
    auto event = [&](juce::Point<float> position, juce::Point<float> down, bool drag, int modifiers = 0)
    { return juce::MouseEvent(mouse, position, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier | modifiers), 1, 0, 0, 0, 0, &ui.timeline, &ui.timeline, juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), 1, drag); };
    {
        const auto preset = legacyBassFixture(input);
        for (const auto edit : {Project::TransitionEdit::moveWindow, Project::TransitionEdit::resizeStart, Project::TransitionEdit::resizeEnd,
             Project::TransitionEdit::moveOutgoing, Project::TransitionEdit::moveIncoming})
        {
            ui.engine.project = preset.project; ui.engine.project.snap = false; ui.selectAutomationTarget(-1); ui.refresh();
            const auto edge = ui.engine.project.transitionLinks[0];
            // Fixture visibility is explicit: editing no longer navigates/scrolls.
            ui.viewport.setViewPosition(std::max(0, (int) (ui.engine.project.transitionRange(edge).getStart() * ui.timeline.pixelsPerSecond) - 80), 0);
            const auto box = ui.timeline.transitionBounds(edge);
            auto point = box.withHeight(18).getCentre();
            if (edit == Project::TransitionEdit::resizeStart) point.x = box.getX() + 1;
            if (edit == Project::TransitionEdit::resizeEnd) point.x = box.getRight() - 1;
            if (edit == Project::TransitionEdit::moveOutgoing || edit == Project::TransitionEdit::moveIncoming)
            {
                const auto handle = ui.timeline.transitionSongBounds(edge, edit == Project::TransitionEdit::moveOutgoing);
                check(!handle.isEmpty(), "native song-handle fixture brings its footer into view before the gesture");
                point = handle.getCentre();
            }
            const auto hit = ui.timeline.transitionAt(point);
            check(hit && hit->second == edit, "native transition control resolves its own action " + juce::String((int) edit));
            const auto before = ui.timeline.transitionReference(ui.engine.project, {edge.incoming, edit});
            const auto count = ui.undoHistory.size(); const auto destination = point.translated(edit == Project::TransitionEdit::moveOutgoing ? 12.f : -12.f, 0);
            ui.timeline.mouseDown(event(point, point, false));
            ui.engine.project.masterDb = -7; // Preserve live edits while dragging.
            ui.timeline.mouseDrag(event(destination, point, true)); ui.timeline.mouseDrag(event(destination, point, true)); ui.timeline.mouseUp(event(destination, point, true));
            const auto after = ui.timeline.transitionReference(ui.engine.project, {edge.incoming, edit});
            check(std::abs(after - before) > .9 && ui.undoHistory.size() == count + 1 && ui.engine.project.masterDb == -7,
                "native transition drag changes timing once and preserves live controls: " + ui.status);
            ui.undo(false); const auto undone = ui.timeline.transitionReference(ui.engine.project, {edge.incoming, edit}); ui.undo(true);
            check(std::abs(undone - before) < 1e-7 && std::abs(ui.timeline.transitionReference(ui.engine.project, {edge.incoming, edit}) - after) < 1e-7,
                "Undo and Redo restore the complete transition box gesture");
        }
        ui.engine.project = preset.project; ui.timeline.pixelsPerSecond = 40; ui.refresh();
        ui.viewport.setViewPosition((int) (ui.engine.project.transitionRange(ui.engine.project.transitionLinks[0]).getStart() * 40) - 80, 0);
        auto stream = report.getSiblingFile("transition-box.png").createOutputStream(); juce::PNGImageFormat png;
        check(stream && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *stream), "capture native transition frame and independent song handles");
        ui.timeline.pixelsPerSecond = 12; ui.viewport.setViewPosition(0, 0); ui.engine.project = original; ui.refresh();
    }
    {
        auto preset = legacyBassFixture(input);
        ui.engine.project = preset.project; ui.engine.project.snap = false; ui.selectedClip = preset.project.clips[1].id;
        ui.selectAutomationTarget((int) AutomationTarget::bass); ui.refresh();
        const auto& link = ui.engine.project.transitionLinks[0]; const auto* in = ui.engine.project.clipFor(link.incoming);
        ui.timeline.selectAutomationCurve({in->track, AutomationTarget::bass, {}, {}, in->id});
        const double at = in->start + in->localTimeForSource((*link.bassSwap)[2]);
        const juce::Point<float> point {(float) (at * 12), ui.timeline.automationY(in->track, -60, (int) AutomationTarget::bass)};
        const juce::Point<float> target {point.x + 2, point.y}; const auto undos = ui.undoHistory.size();
        ui.timeline.mouseDown(event(point, point, false)); ui.timeline.mouseDrag(event(target, point, true)); ui.timeline.mouseUp(event(target, point, true));
        const auto& moved = ui.engine.project.transitionLinks[0]; const auto* out = ui.engine.project.clipFor(moved.outgoing); in = ui.engine.project.clipFor(moved.incoming);
        check(ui.undoHistory.size() == undos + 1 && std::abs(out->start + out->localTimeForSource((*moved.bassSwap)[0])
            - in->start - in->localTimeForSource((*moved.bassSwap)[2])) < 1e-7,
            "native bass point gesture moves both partners in one undoable edit");
        ui.undo(false); ui.undo(true);
        check(ui.engine.project.validateTransitionLinks().wasOk(), "Undo and Redo retain bass pairing ownership");
        juce::PNGImageFormat png; auto stream = report.getSiblingFile("transition-presets.png").createOutputStream();
        check(stream && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *stream), "capture native transition dropdown and paired bass curves");
        ui.engine.project = original; ui.selectAutomationTarget(-1); ui.selectedClip = a.id; ui.refresh();
    }
    for (bool snap : {false, true})
    {
        ui.engine.project = manualPair; ui.engine.project.snap = snap; ui.selectedClip = incoming.id; ui.refresh();
        const auto& middle = ui.engine.project.clips[1];
        const juce::Point<float> body {(float) ((middle.start + middle.length * 0.4) * 12), ui.timeline.clipBounds(middle).getY() + 24};
        const juce::Point<float> target {body.x + 29, body.y};
        const auto undos = ui.undoHistory.size();
        ui.timeline.mouseDown(event(body, body, false));
        ui.engine.project.masterDb = -7; ui.engine.project.clips[0].gainDb = -5; // A live control change during a gesture.
        ui.timeline.mouseDrag(event(target, body, true)); ui.timeline.mouseUp(event(target, body, true));
        check(linked(ui.engine.project) && ui.engine.project.clips[0].start > manualPair.clips[0].start
              && ui.engine.project.clips[2].start > manualPair.clips[2].start && ui.engine.project.masterDb == -7 && ui.engine.project.clips[0].gainDb == -5
              && ui.undoHistory.size() == undos + 1 && ui.engine.project.clips[1].introAnchor == manualPair.clips[1].introAnchor
              && (!snap || std::abs(std::remainder(ui.engine.project.tempo.beatsAt(ui.engine.project.clips[1].start)
                  - manualPair.tempo.beatsAt(manualPair.clips[1].start), 1)) < 1.0e-7),
              juce::String(snap ? "snapped" : "free") + " native middle-track drag carries both partners, retains manual cues and live controls, and records one undo");
        ui.undo(false); const bool undoLinked = linked(ui.engine.project) && close(ui.engine.project.clips[0].start, manualPair.clips[0].start);
        ui.undo(true);
        check(undoLinked && linked(ui.engine.project) && ui.engine.project.clips[0].start > manualPair.clips[0].start,
              "Undo/Redo restores the complete linked native track movement");
    }
    ui.engine.project = original; ui.selectedClip = a.id; ui.refresh();
    juce::Point<float> down {(float) (oldOut * 12), (float) ui.timeline.laneTop(0) + 75}, to {down.x - 24, down.y};
    const auto undoCount = ui.undoHistory.size();
    ui.timeline.mouseDown(event(down, down, false)); ui.timeline.mouseDrag(event(to, down, true)); ui.timeline.mouseUp(event(to, down, true));
    check(preserved(ui.engine.project) && ui.engine.project.clips[0].outroAnchor >= 0 && ui.undoHistory.size() == undoCount + 1
          && !ui.busy && !ui.tempoPreparationPending, "native orange-bar gesture makes one undo entry without an analysis/preparation job");
    ui.undo(false); check(ui.engine.project.clips[0].prepared == original.clips[0].prepared && ui.engine.project.clips[0].outroAnchor == original.clips[0].outroAnchor, "Undo restores original marker and prepared PCM");
    ui.undo(true); check(ui.engine.project.clips[0].outroAnchor >= 0 && preserved(ui.engine.project), "Redo restores the aligned tracks");
    for (const bool alt : {false, true})
    {
        ui.engine.project = original; ui.selectedClip = a.id; ui.engine.project.snap = alt; ui.refresh();
        to = {down.x - 19, down.y}; const int modifiers = alt ? juce::ModifierKeys::altModifier : 0;
        ui.timeline.mouseDown(event(down, down, false)); ui.timeline.mouseDrag(event(to, down, true, modifiers)); ui.timeline.mouseUp(event(to, down, true, modifiers));
        check(close(ui.engine.project.clips[0].outroAnchor.value_or(-1), a.sourceElapsed(a.transitionTime(false) - 19 / 12.0)) && preserved(ui.engine.project) && ui.engine.project.snap == alt,
              alt ? "Alt bypasses anchor snap without changing the Snap toggle" : "Snap off permits an exact free marker position without changing parameters");
    }
    ui.engine.project = original; ui.engine.project.snap = false; ui.selectedClip = incoming.id; ui.refresh();
    ui.engine.setAutomationRecordMode(AutomationRecordMode::touch);
    const juce::Point<float> introDown {(float) ((incoming.start + incoming.transitionTime(true)) * 12), (float) ui.timeline.laneTop(incoming.track) + 7};
    const juce::Point<float> introTo {introDown.x + 48, introDown.y};
    const double wantedNativeIntroSource = incoming.sourceElapsed(incoming.transitionTime(true) + 4);
    ui.timeline.mouseDown(event(introDown, introDown, false)); ui.timeline.mouseDrag(event(introTo, introDown, true)); ui.timeline.mouseUp(event(introTo, introDown, true));
    check(close(ui.engine.project.clips[1].introAnchor.value_or(-1), wantedNativeIntroSource) && linked(ui.engine.project) && preserved(ui.engine.project)
          && ui.engine.automationRecordMode == AutomationRecordMode::touch,
          "native IN handle slides its track into alignment and preserves automation recording mode");
    ui.engine.setAutomationRecordMode(AutomationRecordMode::read);
    ui.engine.project = original; ui.selectedClip = a.id; ui.refresh();
    {
        auto edgeProject = original; edgeProject.clips = {original.clips[0]};
        edgeProject.transitionLinks.clear(); edgeProject.transitionLinksKnown = true;
        edgeProject.clips[0].fadeOut = 0; edgeProject.clips[0].outroAnchor.reset();
        ui.engine.project = edgeProject; ui.refresh();
        const juce::Point<float> edge {(float) (edgeProject.clips[0].end() * 12), (float) ui.timeline.laneTop(0) + 7};
        const juce::Point<float> earlier {edge.x - 24, edge.y};
        ui.timeline.mouseMove(event(edge, edge, false));
        check(ui.timeline.getMouseCursor() == juce::MouseCursor(juce::MouseCursor::LeftRightResizeCursor), "hovering an edge anchor advertises horizontal dragging");
        ui.timeline.mouseDown(event(edge, edge, false)); ui.timeline.mouseDrag(event(earlier, edge, true)); ui.timeline.mouseUp(event(earlier, edge, true));
        check(ui.engine.project.clips[0].outroAnchor.has_value(), "OUT handle at the exact clip edge is draggable, including its top triangle");
        ui.engine.project = original; ui.selectedClip = a.id; ui.refresh();
        ui.performTimelineAction(TimelineAction::bass, 8, 0);
        check(ui.automationLane() == -1, "RMB automation insertion preserves clip editing and waveform visibility");
        const juce::Point<float> marker {96, ui.timeline.automationY(0, 0, (int) AutomationTarget::bass)};
        const juce::Point<float> movedMarker {108, marker.y + 10};
        const auto reveal = marker.translated(-12, 0);
        ui.timeline.mouseDown(event(reveal, reveal, false)); ui.timeline.mouseUp(event(reveal, reveal, false));
        ui.timeline.mouseMove(event(marker, marker, false));
        check(ui.timeline.getMouseCursor() == juce::MouseCursor(juce::MouseCursor::DraggingHandCursor), "automation overlays advertise draggable points");
        ui.timeline.mouseDown(event(marker, marker, false)); ui.timeline.mouseDrag(event(movedMarker, marker, true)); ui.timeline.mouseUp(event(movedMarker, marker, true));
        const auto& bass = ui.engine.project.clips[0].controls.automation[(size_t) AutomationTarget::bass];
        check(std::any_of(bass.points.begin(), bass.points.end(), [&](const auto& p) { return close(ui.engine.project.clips[0].start + ui.engine.project.clips[0].localTimeForSource(p.time), ui.engine.project.snappedTime(9, true)) && p.value < 0; }), "an RMB-created marker drags in time and value directly over the waveform");
        ui.performTimelineAction(TimelineAction::volume, 10, 0);
        check(ui.automationLane() == (int) AutomationTarget::bass, "RMB insertion preserves the previously selected curve controls too");
        ui.engine.setAutomationRecordMode(AutomationRecordMode::touch);
        ui.performTimelineAction(TimelineAction::treble, 12, 0);
        check(ui.engine.automationRecordMode == AutomationRecordMode::touch, "RMB marker insertion retains the selected automation recording mode");
        ui.engine.setAutomationRecordMode(AutomationRecordMode::read);
        auto overlay = report.getSiblingFile("automation-overlay.png").createOutputStream(); juce::PNGImageFormat overlayPng;
        check(overlay && overlayPng.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *overlay), "capture editable automation overlays with their waveforms retained");
        const juce::Point<float> body {180, ui.timeline.clipBounds(ui.engine.project.clips[0]).getY() + 24};
        ui.timeline.mouseMove(event(body, body, false));
        check(ui.timeline.getMouseCursor() == juce::MouseCursor(juce::MouseCursor::DraggingHandCursor), "clip bodies advertise movement while automation remains overlaid");
        ui.timeline.mouseDown(event(body, body, false)); ui.timeline.mouseDrag(event({body.x + 12, body.y}, body, true)); ui.timeline.mouseUp(event({body.x + 12, body.y}, body, true));
        check(ui.engine.project.clips[0].start > original.clips[0].start && ui.automationLane() == -1, "ordinary clip movement remains available after editing an automation marker");
        ui.selectAutomationTarget(-1);
    }
    {
        auto ownedLive = std::make_unique<Workstation>(false); auto& live = *ownedLive;
        live.stopTimer(); live.engine.project = original;
        live.transitionIndex = 3; live.transitionLength.setSelectedId(4, juce::dontSendNotification); // Short-source fixture uses an explicit fixed frame.
        for (const auto& clip : original.clips) live.engine.library.push_back(clip.media);
        live.engine.position = 2; live.engine.setPlaying(true);
        const auto prepared = live.engine.project.clips[0].prepared;
        const auto map = live.engine.project.tempo;
        auto stableClock = [&]
        {
            for (double time = 0; time <= original.duration(); time += .137)
                if (!close(live.engine.project.tempo.bpmAt(time), map.bpmAt(time)) || !close(live.engine.project.tempo.beatsAt(time), map.beatsAt(time))) return false;
            return true;
        };
        live.addMedia(1);
        check(live.engine.playing && live.engine.position == 2 && !live.busy
              && !live.engine.project.hasPendingPlacement() && live.engine.project.clips[0].prepared == prepared
              && stableClock(),
              "adding ready audio while playing joins the running tempo without stopping, seeking, preparing or replacing audible PCM");
        Engine reference; reference.project = original; reference.position = 2; reference.setPlaying(true);
        bool identical = true;
        for (int block = 0; block < 24; ++block)
        {
            juce::AudioBuffer<float> actual(2, 256), expected(2, 256);
            live.engine.audioDeviceIOCallbackWithContext(nullptr, 0, actual.getArrayOfWritePointers(), 2, 256, {});
            reference.audioDeviceIOCallbackWithContext(nullptr, 0, expected.getArrayOfWritePointers(), 2, 256, {});
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 256; ++i) identical &= actual.getSample(ch, i) == expected.getSample(ch, i);
            identical &= actual.getMagnitude(0, 256) > 0.01f && live.engine.playing && live.engine.position == reference.position;
        }
        check(identical, "audible callback PCM and the advancing playhead are sample-identical to uninterrupted playback after a live add");
        live.addMedia(1, TimelinePlacement {1, 0});
        check(live.engine.playing && live.engine.project.clips.back().start > live.engine.position
              && live.engine.project.clips[0].prepared == prepared && stableClock(),
              "a live drop behind the playhead joins on an upcoming grid position without changing already audible PCM or tempo");
        live.cancelTimelineAudio();
    }
    ui.engine.project = original; ui.selectedClip = a.id; ui.refresh();
    auto menu = ui.timelineMenu(8, 0); bool copyFound = false, fxDisabled = false, loopEndDisabled = false;
    for (juce::PopupMenu::MenuItemIterator i(menu); i.next();)
    { copyFound |= i.getItem().itemID == (int) TimelineAction::copy && i.getItem().isEnabled; fxDisabled |= i.getItem().itemID == (int) TimelineAction::effect && !i.getItem().isEnabled; loopEndDisabled |= i.getItem().itemID == (int) TimelineAction::loopEnd && !i.getItem().isEnabled; }
    check(copyFound && fxDisabled && loopEndDisabled, "RMB menu exposes editing and disables effects/loop end until their prerequisites exist");
    ui.performTimelineAction(TimelineAction::copy, 8, 0);
    ui.clipClipboard->length += .137; // Exercise a non-grid end with Snap still enabled.
    ui.performTimelineAction(TimelineAction::paste, 5, 3);
    check(ui.engine.project.clips.size() == 4 && ui.engine.project.clips.back().id != a.id && ui.engine.project.clips.back().track == 3
          && ui.engine.project.clips.back().media == a.media, "context copy/paste creates an independently editable clip on the chosen lane");
    const auto firstPasteEnd = ui.currentClip()->end();
    check(ui.engine.position == firstPasteEnd, "single paste advances cursor to the actual pasted segment end");
    ui.performTimelineAction(TimelineAction::paste, ui.engine.position, 3);
    check(std::abs(ui.currentClip()->start - firstPasteEnd) < 1.e-8 && ui.engine.position == ui.currentClip()->end(),
        "repeated segment paste uses the previous paste end");
    ui.undo(false); ui.selectedClip = ui.engine.project.clips.back().id;
    ui.performTimelineAction(TimelineAction::cut, 5, 3); check(ui.engine.project.clips.size() == 3 && ui.clipClipboard.has_value(), "Cut retains an internal clip clipboard and removes the selected instance");
    ui.undo(false); check(ui.engine.project.clips.size() == 4, "Cut is undoable");
    ui.engine.project = changed; ui.selectedClip = fading.id; ui.engine.project.snap = false;
    ui.performTimelineAction(TimelineAction::split, fading.start + cut, fading.track);
    check(ui.engine.project.clips.size() == 4 && ui.engine.project.clips[1].id == fading.id && ui.engine.project.clips[2].media == fading.media
          && ui.engine.project.clips[3].id == changed.clips[2].id, "context Split keeps both source segments adjacent in playlist order");
    ui.engine.project = original; ui.selectedClip = a.id;
    for (const auto action : {TimelineAction::volume, TimelineAction::treble, TimelineAction::mid, TimelineAction::bass})
        ui.performTimelineAction(action, 8.12, 0);
    bool automation = true;
    for (int band = 0; band < 4; ++band)
    { const auto& curve = ui.engine.project.clips[0].controls.automation[(size_t) band]; automation = automation && curve.active() && std::any_of(curve.points.begin(), curve.points.end(), [&](const auto& p) { return close(ui.engine.project.clips[0].start + ui.engine.project.clips[0].localTimeForSource(p.time), ui.engine.project.snappedTime(8.12, true)); }); }
    check(automation, "volume and all three EQ context actions add functional snapped automation points");
    {
        const auto before = ui.automationAddresses(0).size();
        for (const auto action : {TimelineAction::volume, TimelineAction::treble, TimelineAction::mid, TimelineAction::bass})
        { ui.performTimelineAction(action, 9.12, 0); ui.performTimelineAction(action, 9.12, 0); }
        bool reused = ui.automationAddresses(0).size() == before;
        for (int band = 0; band < 4; ++band)
            reused &= ui.engine.project.clips[0].controls.automation[(size_t) band].points.size() == 2
                && ui.engine.project.tracks[0].automation[(size_t) band].points.empty();
        check(reused, "repeating each timeline marker action adds nodes to one source-owned curve and reuses a node at the same time");
    }
    ui.selectAutomationTarget(-1); ui.selectedClip = a.id;
    ui.engine.project.snap = false;
    ui.performTimelineAction(TimelineAction::loopStart, a.start + 4, 0); ui.performTimelineAction(TimelineAction::loopEnd, a.start + 6, 0);
    check(close(ui.engine.project.clips[0].loopSpan, a.sourceElapsed(6) - a.sourceElapsed(4)) && close(ui.engine.project.clips[0].length * ui.engine.project.clips[0].rate, 2 * (a.sourceElapsed(6) - a.sourceElapsed(4)))
          && ui.loopStartClip.isEmpty(), "loop markers select and repeat the exact source range twice");
    ui.undo(false); check(ui.engine.project.clips[0].loopSpan == 0 && ui.engine.project.clips[0].length == a.length, "Undo restores the track replaced by the marked loop");
    {
        ui.engine.project = controlBase; ui.selectedClip = controlBase.clips[1].id; ui.undoHistory.clear(); ui.redoHistory.clear();
        const auto id = controlBase.transitionLinks[0].incoming;
        ui.selectTransition(id); ui.engine.position = 3;
        check(ui.toolPage == 1 && ui.transitionPanel.isVisible() && ui.selectedTransitionLength.isVisible(), "selecting a frame opens the lower selected-transition panel");
        ui.selectedTransitionLength.setSelectedId(4, juce::sendNotificationSync);
        check(ui.undoHistory.size() == 1 && ui.engine.position == 3 && !ui.busy, "native length selector makes one Undo without seeking or audio preparation");
        const auto edited = juce::JSON::toString(projectToJson(ui.engine.project));
        ui.undo(false); check(juce::JSON::toString(projectToJson(ui.engine.project)) == juce::JSON::toString(projectToJson(controlBase)), "Undo restores the complete local length edit");
        ui.undo(true); check(juce::JSON::toString(projectToJson(ui.engine.project)) == edited, "Redo restores the local frame and fades");
        ui.transitionPresetForSelection.setSelectedId(2, juce::sendNotificationSync);
        check(!ui.engine.project.transitionFor(id)->bands[1] && ui.engine.project.clipFor(id)->controls.automation[1].active()
            && ui.undoHistory.size() == 2, "native preset selector places ordinary nodes only for the selected pair with one Undo: " + ui.status);
        ui.transitionLock.onClick();
        check(ui.engine.project.transitionFor(id)->locked && !ui.selectedTransitionLength.isEnabled() && !ui.transitionPresetForSelection.isEnabled(),
            "native lock control disables local edits and retains an accessible unlock command");
        auto screenshot = report.getSiblingFile("selected-transition-locked.png").createOutputStream(); juce::PNGImageFormat format;
        check(screenshot && format.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *screenshot), "capture selected-transition inspector and locked frame");
        const auto frozen = juce::JSON::toString(projectToJson(ui.engine.project));
        ui.selectedClip = id; ui.editClip(0, ui.currentClip()->start + 1); ui.removeClip(); ui.engine.position = ui.currentClip()->start + 2; ui.splitClip();
        ui.addTimelineAutomation(AutomationTarget::mid, ui.engine.position, ui.currentClip()->track);
        ui.syncMix(true);
        check(frozen == juce::JSON::toString(projectToJson(ui.engine.project)) && !ui.busy, "production clip, split, remove, automation and Beatmatch handlers respect locks");
        check(!ui.engine.setControlValue({ui.currentClip()->track, AutomationTarget::bass, {}, {}, id}, -8), "controller and mixer input cannot change a locked track's EQ");
        ui.navigateTransition(1); check(ui.selectedTransition == controlBase.transitionLinks[1].incoming, "Next selects a relationship by identity");
        ui.navigateTransition(-1); ui.transitionLock.onClick(); check(!ui.engine.project.hasLockedTransitions(), "Previous and Unlock return to the protected relationship");
        ui.engine.project = controlBase; ui.selectedTransition.clear(); ui.selectedClip = controlBase.clips[0].id; ui.refresh();
    }
    {
        ui.setSize(1920, 1080); ui.engine.project = controlBase;
        ui.selectedClip = controlBase.clips[1].id; ui.selectedTransition = controlBase.transitionLinks[0].incoming;
        ui.timeline.pixelsPerSecond = 32; ui.viewport.setViewPosition(0, 0); ui.refresh();
        auto& timeline = ui.timeline;
        const auto clipId = ui.selectedClip; const int track = ui.currentClip()->track;
        ui.viewport.setViewPosition((int) (ui.currentClip()->start * timeline.pixelsPerSecond), 0);
        const auto document = juce::JSON::toString(projectToJson(ui.engine.project));
        const auto toolsUndoCount = ui.undoHistory.size(), toolsRedoCount = ui.redoHistory.size(); const bool toolsDirty = ui.dirty;
        ui.engine.position = ui.currentClip()->start + 1; const double position = ui.engine.position;
        ui.engine.setAutomationRecordMode(AutomationRecordMode::touch);
        const std::array<AutomationTarget, 5> targets {AutomationTarget::volume, AutomationTarget::bass, AutomationTarget::mid, AutomationTarget::treble, AutomationTarget::cutoff};
        for (size_t i = 0; i < targets.size(); ++i)
        {
            ui.transitionCurveOnlyBand = 3; ui.transitionCurveShown.fill(false);
            timeline.curveTools->buttons[i].onClick();
            const AutomationAddress address {track, targets[i], {}, {}, clipId};
            check(ui.displayedAddress(track) == address && timeline.automationVisible(address) && timeline.curveTools->buttons[i].getToggleState(),
                "below-waveform button selects and reveals its exact clip parameter " + juce::String((int) i));
            check(timeline.curveTools->buttons[i].getWantsKeyboardFocus() && timeline.curveTools->buttons[i].getTitle().isNotEmpty(),
                "curve button has native keyboard focus and an accessible parameter name " + juce::String((int) i));
        }
        check(document == juce::JSON::toString(projectToJson(ui.engine.project)) && toolsUndoCount == ui.undoHistory.size()
            && toolsRedoCount == ui.redoHistory.size() && toolsDirty == ui.dirty && ui.engine.position == position
            && ui.engine.automationRecordMode == AutomationRecordMode::touch,
            "all five curve selectors preserve saved data, Undo/Redo, dirty state, playhead and recording mode");
        timeline.curveTools->fx.onClick();
        check(document == juce::JSON::toString(projectToJson(ui.engine.project)) && ui.status.contains("VST3"),
            "FX selector explains a missing plugin without adding automation or changing audio");
        auto verifyGeometry = [&](const juce::String& name)
        {
            const auto tools = timeline.curveToolsBounds(); const auto wave = timeline.clipBounds(*ui.currentClip());
            check(!tools.isEmpty() && tools.getY() >= wave.getBottom() + 2 && tools.getBottom() <= timeline.laneTop(track + 1) - 4
                && !tools.toFloat().intersects(timeline.clipBounds(ui.engine.project.clips[2]))
                && !timeline.transitionAt(tools.toFloat().getCentre()) && !timeline.anchorAt(tools.toFloat().getCentre()),
                name + " keeps tools below their waveform, inside their lane and outside other edit handles");
            check(ui.viewport.getViewArea().contains(tools) && timeline.curveTools->getBounds() == tools,
                name + " attaches the native controls to the visible part of the selected clip");
        };
        verifyGeometry("wide scrolled clip");
        const auto firstLink = ui.engine.project.transitionLinks[0], secondLink = ui.engine.project.transitionLinks[1];
        const auto inHandle = timeline.transitionSongBounds(firstLink, false), outHandle = timeline.transitionSongBounds(secondLink, true);
        check(inHandle.getWidth() == 22 && outHandle.getWidth() == 22 && inHandle.getY() > timeline.clipBounds(*ui.currentClip()).getBottom()
            && !inHandle.intersects(outHandle) && !inHandle.intersects(timeline.curveToolsBounds().toFloat())
            && !outHandle.intersects(timeline.curveToolsBounds().toFloat()), "I and O replace the large song panels with separate lower-toolbar handles");
        timeline.mouseMove(event(inHandle.getCentre(), inHandle.getCentre(), false));
        check(timeline.getTooltip().contains("Incoming song") && timeline.getTooltip().contains("fixed transition"), "I handle explains its existing song-movement action");
        timeline.mouseMove(event(outHandle.getCentre(), outHandle.getCentre(), false));
        check(timeline.getTooltip().contains("Outgoing song"), "O handle explains the outgoing song action");
        check(timeline.curveTools->buttons[0].isVisible() && !timeline.curveTools->selector.isVisible(), "wide clip displays all five labelled buttons and FX");
        ui.engine.project.clipFor(clipId)->fadeIn = 2; // Authored generated-fade fixture.
        timeline.curveTools->buttons[0].onClick();
        check(timeline.curveTools->buttons[0].hasCurve && !timeline.curveTools->buttons[4].hasCurve,
            "presence indicators distinguish a generated volume fade from a filter with no saved nodes");
        const auto waveBefore = timeline.clipBounds(*ui.currentClip());
        ui.selectedClip = ui.engine.project.clips[0].id; ui.refresh();
        check(timeline.clipBounds(*ui.engine.project.clipFor(clipId)) == waveBefore, "selecting another clip never moves or resizes existing waveforms");
        ui.selectedClip = clipId; ui.engine.project.setTransitionLocked(ui.selectedTransition, true); ui.refresh();
        const auto locked = juce::JSON::toString(projectToJson(ui.engine.project));
        timeline.curveTools->buttons[1].onClick();
        check(timeline.curveTools->buttons[1].isEnabled() && timeline.curveTools->buttons[1].getTooltip().contains("Locked")
            && ui.displayedAddress(track).clipId == clipId && locked == juce::JSON::toString(projectToJson(ui.engine.project)),
            "locked clips allow read-only curve selection while retaining lock ownership");
        ui.engine.project.setTransitionLocked(ui.selectedTransition, false);
        ui.engine.setAutomationRecordMode(AutomationRecordMode::read);
        ui.engine.exporting = true; ui.refresh();
        check(!timeline.curveTools->buttons[0].isEnabled() && !timeline.curveTools->fx.isEnabled(), "export disables curve tool actions");
        ui.engine.exporting = false; ui.refresh();
        ui.engine.project.clips[1].controls.automation[1].setPoint(2, -6);
        timeline.curveTools->buttons[1].onClick();
        check(timeline.keyPressed(juce::KeyPress(juce::KeyPress::rightKey)) && timeline.editingAutomationPoint
            && timeline.editingAutomationPoint->first.clipId == clipId,
            "existing keyboard node navigation follows the strip's selected clip curve");
        auto captureTools = [&](const char* name)
        {
            auto output = report.getSiblingFile(name).createOutputStream(); juce::PNGImageFormat image;
            check(output && image.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *output), juce::String("capture ") + name);
        };
        captureTools("curve-tools-wide.png");
        timeline.pixelsPerSecond = 4; ui.refresh(); ui.viewport.setViewPosition((int) (ui.currentClip()->start * 4), 0);
        verifyGeometry("compact clip");
        check(timeline.curveTools->selector.isVisible() && !timeline.curveTools->buttons[0].isVisible(), "narrow clips collapse to one native parameter selector");
        timeline.curveTools->selector.setSelectedId(5, juce::sendNotificationSync);
        check(ui.displayedAddress(track) == AutomationAddress {track, AutomationTarget::cutoff, {}, {}, clipId}, "compact selector uses the same source-owned Filter selection path");
        captureTools("curve-tools-compact.png");
        timeline.pixelsPerSecond = .1; ui.refresh(); ui.viewport.setViewPosition(0, 0);
        check(!timeline.curveTools->isVisible(), "clips too short for a readable selector never spill tools outside their bounds");
        ui.setSize(1440, 940); timeline.pixelsPerSecond = 32; ui.refresh();
        ui.viewport.setViewPosition((int) (ui.currentClip()->start * 32), 0);
        verifyGeometry("1440-pixel layout"); captureTools("curve-tools-1440.png");
        ui.selectedClip.clear(); ui.refresh(); check(!timeline.curveTools->isVisible(), "clearing the selected clip removes its tools");
        ui.transitionCurveShown.fill(true); ui.transitionCurveOnlyBand = -1; ui.selectedAutomationTarget = -1;
        ui.viewport.setViewPosition(0, 0);
    }
    {
        auto pump = []
        {
#if JUCE_WINDOWS
            for (int n = 0; n < 30; ++n)
            {
                MSG message; while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
                juce::Thread::sleep(1);
            }
#endif
        };
        ui.engine.project = original; ui.selectedClip = a.id; ui.selectedTransition.clear();
        ui.selectedAutomationTarget = -1; ui.undoHistory.clear(); ui.redoHistory.clear(); ui.refresh();
        auto context = ui.timelineMenu(8, 0); bool filter = false;
        for (juce::PopupMenu::MenuItemIterator i(context); i.next();) filter |= i.getItem().text.contains("Add Filter");
        check(filter, "P55 Filter has direct context insertion parity with Volume and EQ");
        const auto plugin = juce::File::getCurrentWorkingDirectory().getChildFile("build/MixworksTestGain_artefacts/Release/VST3/Mixworks Test Gain.vst3");
        const auto error = ui.engine.addPlugin(0, plugin); check(error.isEmpty(), "P55 native FX fixture loads: " + error);
        if (error.isEmpty())
        {
            ui.undoHistory.clear(); ui.redoHistory.clear(); ui.dirty = false; ui.selectTrack(0);
            ui.engine.setAutomationRecordMode(AutomationRecordMode::touch);
            const auto before = juce::JSON::toString(projectToJson(ui.engine.project));
            ui.chooseAutomationPlugin(); pump();
            auto* dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
            check(dialog != nullptr, "P55 native FX chooser opens");
            if (dialog) { dialog->getComboBoxComponent("parameter")->setSelectedItemIndex(0, juce::dontSendNotification); dialog->exitModalState(1); pump(); }
            check(before == juce::JSON::toString(projectToJson(ui.engine.project)) && ui.undoHistory.empty() && !ui.dirty,
                "P55 showing an FX parameter is view-only without allocating a saved curve or Undo");
            check(ui.engine.automationRecordMode == AutomationRecordMode::touch, "P55 FX browsing preserves the active recording mode");
            const auto address = ui.pluginLane; ui.engine.position = a.start + 4;
            juce::AudioBuffer<float> pcmBefore(2, 256), pcmAfter(2, 256); juce::MidiBuffer fxMidi;
            ui.engine.render(pcmBefore, 256, a.start + 4, fxMidi);
            ui.automationAdd.onClick();
            ui.engine.render(pcmAfter, 256, a.start + 4, fxMidi);
            bool identical = true; for (int channel = 0; channel < 2; ++channel) for (int frame = 0; frame < 256; ++frame)
                identical &= pcmBefore.getSample(channel, frame) == pcmAfter.getSample(channel, frame);
            check(identical && pcmBefore.getMagnitude(0, 256) > .0001f, "P55 neutral first FX node preserves actual rendered PCM");
            auto* saved = ui.engine.project.curve(address);
            check(saved && saved->points.size() == 1 && ui.undoHistory.size() == 1 && ui.engine.project.tracks[0].pluginAutomation.empty(),
                "P55 first FX edit materialises the exact segment parameter once with one Undo");
            check(ui.timeline.editingAutomationPoint && ui.timeline.editingAutomationPoint->first == address
                && ui.automationParameter.getText().contains("FX"), "P55 FX target and selected node stay visible in Editing");
            const auto edited = juce::JSON::toString(projectToJson(ui.engine.project));
            ui.undo(false); check(!ui.engine.project.curve(address), "P55 Undo removes first-edit FX storage without removing its plugin");
            ui.undo(true); check(edited == juce::JSON::toString(projectToJson(ui.engine.project)), "P55 Redo restores the exact FX edit");
            ui.timeline.selectAutomationCurve(address); ui.engine.position = a.start + 6;
            const auto history = ui.undoHistory.size(); ui.performTimelineAction(TimelineAction::effect, ui.engine.position, 0);
            check(ui.engine.project.curve(address) && ui.engine.project.curve(address)->points.size() == 2 && ui.undoHistory.size() == history + 1
                && !juce::Component::getCurrentlyModalComponent(), "P55 Add FX here reuses the selected insert and parameter without another picker");
            ui.engine.armAutomation(0, true); ui.engine.setAutomationRecordMode(AutomationRecordMode::touch); ui.engine.setPlaying(true);
            ui.engine.setControlValue(address, .6f);
            check(ui.engine.recordingAutomation(), "P55 fixture starts an actual armed Touch take");
            const auto writing = juce::JSON::toString(projectToJson(ui.engine.project));
            ui.chooseAutomationPlugin(); pump(); dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
            if (dialog) { dialog->getComboBoxComponent("parameter")->setSelectedItemIndex(0, juce::dontSendNotification); dialog->exitModalState(1); pump(); }
            check(ui.engine.recordingAutomation() && ui.engine.automationRecordMode == AutomationRecordMode::touch
                && writing == juce::JSON::toString(projectToJson(ui.engine.project)), "P55 FX Show preserves the actual in-progress take and captured nodes");
            check(!ui.insertAutomationNode(address, a.start + 7) && !ui.deleteAutomationNode(address, 1)
                && ui.engine.recordingAutomation() && writing == juce::JSON::toString(projectToJson(ui.engine.project)), "P55 node edits require explicit take completion and never discard active writes");
            ui.engine.setAutomationRecordMode(AutomationRecordMode::read); ui.engine.setPlaying(false); ui.engine.armAutomation(0, false);
            const auto fxFile = report.getSiblingFile("p55-fx.mixworks"); const auto saveError = ui.engine.saveProject(fxFile);
            const auto parsed = parseProject(fxFile.loadFileAsString());
            const auto found = std::find_if(parsed.clips.begin(), parsed.clips.end(), [&](const auto& ref) { return ref.clip.id == address.clipId; });
            bool retained = found != parsed.clips.end();
            if (retained)
            {
                const auto& lanes = found->clip.controls.pluginAutomation;
                const auto lane = std::find_if(lanes.begin(), lanes.end(), [&](const auto& item) { return item.slotId == address.slotId && item.parameterId == address.parameterId; });
                retained = lane != lanes.end() && lane->curve.points.size() == ui.engine.project.curve(address)->points.size()
                    && lanes.size() == ui.currentClip()->controls.pluginAutomation.size();
                if (retained) for (size_t i = 0; i < lane->curve.points.size(); ++i)
                {
                    const auto& actual = lane->curve.points[i]; const auto& expected = ui.engine.project.curve(address)->points[i];
                    retained &= actual.time == expected.time && actual.value == expected.value && actual.bend == expected.bend;
                }
            }
            check(saveError.isEmpty() && parsed.error.isEmpty() && retained, "P55 first-edit FX nodes retain parameter identity and exact points through native Save and parse: " + saveError + " / " + parsed.error);

        }
        ui.engine.setAutomationRecordMode(AutomationRecordMode::read);
        ui.engine.project = original; ui.selectedClip = a.id; ui.selectedAutomationTarget = -1; ui.refresh();
        ui.addTimelineAutomation(AutomationTarget::bass, 8, 0);
        const AutomationAddress address {0, AutomationTarget::bass, {}, {}, a.id};
        ui.timeline.selectAutomationCurve(address); ui.refresh();
        const auto before = juce::JSON::toString(projectToJson(ui.engine.project)); const auto history = ui.undoHistory.size();
        const auto* curve = ui.engine.project.curve(address); int index = -1;
        for (int i = 0; i < (int) curve->points.size(); ++i) if (std::abs(ui.engine.project.automationSessionTime(address, curve->points[(size_t) i].time) - ui.engine.project.snappedTime(8, true)) < 1.e-7) index = i;
        if (index >= 0)
        {
            const auto& node = curve->points[(size_t) index]; const double t = ui.engine.project.automationSessionTime(address, node.time);
            const juce::Point<float> where {(float) (t * ui.timeline.pixelsPerSecond), ui.timeline.automationY(0, ui.timeline.automationDisplayValue(address, t, node.value), (int) address.target)};
            ui.timeline.mouseDown(event(where, where, false, juce::ModifierKeys::rightButtonModifier));
            check(before == juce::JSON::toString(projectToJson(ui.engine.project)) && ui.undoHistory.size() == history,
                "P55 right-click on a saved node opens choices without deleting or mutating it");
            juce::PopupMenu::dismissAllActiveMenus(); pump();
        }
        else check(false, "P55 right-click fixture finds its inserted node");
        ui.engine.project = original; ui.selectedClip = a.id; ui.selectedAutomationTarget = -1; ui.refresh();
        ui.showToolPage(2); ui.engine.position = a.start + 8.12;
        for (int scope = 0; scope < 3; ++scope)
        {
            const AutomationAddress owner {scope == 2 ? -1 : 0, scope == 2 ? AutomationTarget::master : AutomationTarget::volume, {}, {}, scope == 0 ? a.id : juce::String()};
            ui.timeline.selectAutomationCurve(owner); ui.refresh();
            const auto document = juce::JSON::toString(projectToJson(ui.engine.project)); const auto history = ui.undoHistory.size(); const auto position = ui.engine.position.load();
            int choice = -1;
            for (int i = 0; i < (int) ui.automationOwnerChoices.size(); ++i)
            { const auto& candidate = ui.automationOwnerChoices[(size_t) i]; if (candidate == owner) choice = i; }
            check(choice >= 0, "P55 persistent editor lists owner " + juce::String(scope));
            if (choice >= 0) ui.automationOwner.setSelectedItemIndex(choice, juce::sendNotificationSync);
            check(ui.displayedAddress(0) == owner && document == juce::JSON::toString(projectToJson(ui.engine.project))
                && ui.undoHistory.size() == history && ui.engine.position == position, "P55 owner browsing is view-only for scope " + juce::String(scope));
            for (const auto target : {AutomationTarget::volume, AutomationTarget::bass, AutomationTarget::mid, AutomationTarget::treble, AutomationTarget::pan, AutomationTarget::cutoff})
            {
                if (scope == 2 && target != AutomationTarget::volume) continue;
                auto address = owner; if (scope != 2) address.target = target;
                int parameter = -1;
                for (int i = 0; i < (int) ui.automationParameterChoices.size(); ++i) if (ui.automationParameterChoices[(size_t) i] == address) parameter = i;
                check(parameter >= 0, "P55 editor lists parameter " + juce::String((int) target) + " / scope " + juce::String(scope));
                if (parameter < 0) continue;
                ui.automationParameter.setSelectedItemIndex(parameter, juce::sendNotificationSync);
                const auto before = juce::JSON::toString(projectToJson(ui.engine.project)); const auto count = ui.undoHistory.size();
                ui.automationAdd.onClick();
                const auto* saved = ui.engine.project.curve(address);
                check(saved && saved->active() && ui.undoHistory.size() == count + 1 && ui.timeline.editingAutomationPoint
                    && ui.timeline.editingAutomationPoint->first == address && ui.automationNode.getTooltip().contains("project bar"),
                    "P55 Add at playhead edits one explicit curve with project feedback / " + juce::String(scope) + ":" + juce::String((int) target));
                ui.automationAdd.onClick(); check(ui.undoHistory.size() == count + 1, "P55 repeated Add selects its existing node without another Undo");
                const auto inserted = juce::JSON::toString(projectToJson(ui.engine.project));
                ui.automationNext.onClick(); ui.automationPrevious.onClick();
                check(inserted == juce::JSON::toString(projectToJson(ui.engine.project)) && ui.engine.position == position, "P55 previous/next navigation never seeks or changes sound");
                ui.undo(false); check(before == juce::JSON::toString(projectToJson(ui.engine.project)), "P55 explicit owner insertion Undo restores complete document");
            }
        }
        ui.engine.project = original; ui.engine.project.snap = false; ui.selectedClip = a.id;
        const AutomationAddress volume {0, AutomationTarget::volume, {}, {}, a.id}; ui.timeline.selectAutomationCurve(volume); ui.engine.position = a.start + 8;
        ui.insertAutomationNode(volume,a.start,false); ui.insertAutomationNode(volume,a.end(),false);
        ui.automationAdd.onClick(); const auto beforeDialog = juce::JSON::toString(projectToJson(ui.engine.project));
        const auto dialogHistory = ui.undoHistory.size(); ui.engine.setAutomationRecordMode(AutomationRecordMode::touch);
        ui.automationEdit.onClick(); pump();
        auto* dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        check(dialog && ui.engine.automationRecordMode == AutomationRecordMode::touch, "P55 opening exact entry preserves recording mode");
        if (dialog) { dialog->getTextEditor("value")->setText("42"); dialog->exitModalState(0); pump(); }
        check(beforeDialog == juce::JSON::toString(projectToJson(ui.engine.project)) && ui.undoHistory.size() == dialogHistory
            && ui.engine.automationRecordMode == AutomationRecordMode::touch, "P55 exact editor Cancel preserves document, history and mode");
        ui.automationEdit.onClick(); pump(); dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        if (dialog) { dialog->getTextEditor("time")->setText(juce::String(a.start + 8.25 - ui.engine.project.tempo.origin, 6)); dialog->getTextEditor("value")->setText("42"); dialog->exitModalState(1); pump(); }
        const auto* saved = ui.engine.project.curve(volume);
        check(saved && ui.undoHistory.size() == dialogHistory + 1 && saved->points.size() == 3 && std::abs(saved->points[1].value - juce::Decibels::gainToDecibels(.42f)) < .0001
            && std::abs(ui.engine.project.automationSessionTime(volume, saved->points[1].time) - a.start - 8.25) < 1.e-6,
            "P55 native exact Apply changes time and gain through one Undo");
        ui.automationRead.onClick(); check(!ui.engine.project.curve(volume)->enabled && !ui.automationRead.getToggleState(), "P55 disabled curve retains nodes and exposes manual override");
        ui.automationRead.onClick(); check(ui.engine.project.curve(volume)->enabled && ui.automationRead.getToggleState(), "P55 Return to Read restores curve playback without replacing nodes");
        const auto beforeDelete = juce::JSON::toString(projectToJson(ui.engine.project)); const auto deleteHistory = ui.undoHistory.size();
        ui.automationDelete.onClick(); check(ui.engine.project.curve(volume)->points.size() == 2 && ui.undoHistory.size() == deleteHistory + 1, "P55 visible Delete removes only the selected manual node");
        ui.undo(false); check(beforeDelete == juce::JSON::toString(projectToJson(ui.engine.project)), "P55 visible node deletion has exact Undo");
        ui.timeline.selectAutomationCurve(volume); ui.timeline.selectAutomationPoint(volume, 1);
        const auto clipCount = ui.engine.project.clips.size();
        check(ui.editingPanel.onKey(juce::KeyPress(juce::KeyPress::deleteKey)) && ui.engine.project.clips.size() == clipCount
            && ui.engine.project.curve(volume)->points.size() == 2, "P55 Delete from the Editing console targets the selected node and keeps its segment");
        ui.undo(false);

        ui.timeline.selectAutomationCurve(volume);
        const auto noNode = juce::JSON::toString(projectToJson(ui.engine.project));
        check(ui.editingPanel.onKey(juce::KeyPress(juce::KeyPress::deleteKey)) && noNode == juce::JSON::toString(projectToJson(ui.engine.project)),
            "P55 Delete with a curve but no selected node keeps the entire segment");
        ui.timeline.selectAutomationCurve(volume); ui.timeline.selectAutomationPoint(volume, 1);
        ui.automationEdit.onClick(); pump(); dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        ui.engine.project.masterDb -= 1; const auto changed = juce::JSON::toString(projectToJson(ui.engine.project));
        if (dialog) { dialog->getTextEditor("value")->setText("30"); dialog->exitModalState(1); pump(); }
        check(changed == juce::JSON::toString(projectToJson(ui.engine.project)), "P55 stale exact dialog cannot overwrite newer project edits");
        for (int guard = 0; guard < 3; ++guard)
        {
            ui.timeline.selectAutomationCurve(volume); ui.timeline.selectAutomationPoint(volume, 1); ui.automationEdit.onClick(); pump();
            dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
            if (guard == 0) ui.busy = true;
            else if (guard == 1) ui.engine.exporting = true;
            else ui.engine.project.setTransitionLocked(original.transitionLinks[0].incoming, true);
            const auto guarded = juce::JSON::toString(projectToJson(ui.engine.project)); const auto count = ui.undoHistory.size();
            if (dialog) { dialog->getTextEditor("value")->setText("30"); dialog->exitModalState(1); pump(); }
            check(!ui.insertAutomationNode(volume, a.start + 10) && guarded == juce::JSON::toString(projectToJson(ui.engine.project)) && count == ui.undoHistory.size(),
                "P55 admission and modal Apply revalidate busy/export/lock " + juce::String(guard));
            ui.busy = false; ui.engine.exporting = false; ui.engine.project.setTransitionLocked(original.transitionLinks[0].incoming, false);
        }
        ui.timeline.selectAutomationCurve(volume); ui.timeline.selectAutomationPoint(volume, 1); ui.setVisible(true); ui.showToolPage(2);
        for (const auto size : {juce::Point<int> {1250, 800}, juce::Point<int> {1920, 1080}})
        {
            ui.setSize(size.x, size.y); ui.editingViewport.setViewPosition(0, 0); ui.refresh();
            auto output = report.getSiblingFile("automation-editor-" + juce::String(size.x) + ".png").createOutputStream(); juce::PNGImageFormat image;
            const bool captured = output && image.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *output);
            check(ui.automationAdd.isVisible() && ui.editingPanel.isVisible() && ui.editingBody.getLocalBounds().contains(ui.automationDelete.getBounds())
                && captured, "P55 capture persistent automation editor at " + juce::String(size.x));
        }
        ui.detachToolPanel(2, true); ui.editingWindow->setSize(700, 420); ui.refresh();
        auto output = report.getSiblingFile("automation-editor-detached.png").createOutputStream(); juce::PNGImageFormat image;
        check(output && image.writeImageToStream(ui.editingWindow->createComponentSnapshot(ui.editingWindow->getLocalBounds()), *output), "P55 capture detached automation editor with original controls");
        ui.detachToolPanel(2, false);
        ui.engine.project = original; ui.selectedClip = a.id; ui.selectedAutomationTarget = -1; ui.refresh();
    }
    {
        // Empty curves stay empty; explicitly authored points use the visible source clock.
        for (int variant = 0; variant < 3; ++variant)
        {
            const bool mapped = variant == 1;
            ui.engine.project = {}; ui.engine.project.snap = false;
            ui.engine.project.tempo.setPoint(0, 110); ui.engine.project.tempo.setPoint(20, 140);
            auto endpointClip = input.clips[0]; endpointClip.start = variant == 2 ? 4 : 5.137; endpointClip.offset = 3.1;
            endpointClip.length = variant == 2 ? 10 : 9.731; endpointClip.rate = 1.25; endpointClip.audioSlip = .37;
            endpointClip.fadeIn = endpointClip.fadeOut = 0; endpointClip.controls = {};
            endpointClip.controls.gainDb = -3; endpointClip.controls.eqDb = {-2, -4, -6};
            endpointClip.controls.pan = .25f; endpointClip.controls.filterCutoff = .2f;
            if (mapped) { endpointClip.tempoMap = std::make_shared<const TempoMap>(ui.engine.project.tempo); endpointClip.tempoSourceBpm = 120; }
            ui.engine.project.clips = {endpointClip}; ui.selectedClip = endpointClip.id; ui.selectedTransition.clear();
            ui.undoHistory.clear(); ui.redoHistory.clear(); ui.dirty = false;
            ui.timeline.pixelsPerSecond = 32; ui.viewport.setViewPosition(0, 0); ui.refresh();
            for (int parameter = 0; parameter < trackAutomationCount; ++parameter)
            {
                const AutomationAddress address {0, (AutomationTarget) parameter, {}, {}, endpointClip.id};
                const auto unchanged = juce::JSON::toString(projectToJson(ui.engine.project)); const auto history = ui.undoHistory.size();
                ui.timeline.selectAutomationCurve(address);
                const auto view = ui.timeline.automationViewCurve(address); const float neutral = ui.engine.project.manualValue(address);
                check(view.points.empty(),
                    "new parameter " + juce::String(parameter) + (mapped ? " mapped" : variant == 2 ? " integral edges" : " trimmed/rate/slipped") + " has no invented endpoint nodes");
                check(std::abs(ui.timeline.automationDisplayValue(address, endpointClip.start, neutral) - neutral) < 1.e-5f
                    && std::abs(ui.timeline.automationDisplayValue(address, endpointClip.end(), neutral) - neutral) < 1.e-5f,
                    "empty curves display the manual parameter value at both inclusive UI edges");
                check(unchanged == juce::JSON::toString(projectToJson(ui.engine.project)) && history == ui.undoHistory.size(),
                    "selecting an empty curve leaves the document and Undo unchanged");
                ui.addTimelineAutomation(address.target, endpointClip.start + 4.16, 0);
                const auto* savedCurve = ui.engine.project.curve(address);
                check(savedCurve && savedCurve->points.size() == 1 && savedCurve->points.front().value == neutral
                    && close(ui.engine.project.automationSessionTime(address,savedCurve->points.front().time),endpointClip.start + 4.16)
                    && ui.undoHistory.size() == history + 1,
                    "first marker saves only the requested source-owned point as one Undo");
                const auto saved = juce::JSON::toString(projectToJson(ui.engine.project)); const auto reopened = parseProject(saved);
                check(reopened.error.isEmpty() && reopened.clips.size() == 1
                    && reopened.clips[0].clip.controls.automation[(size_t) parameter].points.size() == 1
                    && close(reopened.clips[0].clip.controls.automation[(size_t) parameter].points.back().time, savedCurve->points.back().time),
                    "the requested node survives saved-project parsing without changing session format");
                ui.undo(false); check(unchanged == juce::JSON::toString(projectToJson(ui.engine.project)), "Undo removes the complete first-marker initialisation");
                ui.undo(true); check(saved == juce::JSON::toString(projectToJson(ui.engine.project)), "Redo restores only the requested marker exactly");
            }
            const AutomationAddress volume {0, AutomationTarget::volume, {}, {}, endpointClip.id};
            *ui.engine.project.curve(volume) = {}; ui.timeline.selectAutomationCurve(volume);
            ui.insertAutomationNode(volume,endpointClip.start,false); ui.insertAutomationNode(volume,endpointClip.end(),false); ui.refresh();
            const auto beforeDrag = juce::JSON::toString(projectToJson(ui.engine.project)); const auto history = ui.undoHistory.size();
            const juce::Point<float> start {(float) (endpointClip.start * ui.timeline.pixelsPerSecond), ui.timeline.automationY(0, -3, 0)};
            const auto destination = start.translated(0, 8);
            const auto edgeHit = ui.timeline.automationPointAt(start);
            check(edgeHit && edgeHit->first == volume && edgeHit->second == 0, "new start handle is selectable at the exact clip boundary");
            ui.timeline.mouseDown(event(start, start, false)); ui.timeline.mouseDrag(event(destination, start, true)); ui.timeline.mouseUp(event(destination, start, true));
            check(ui.engine.project.curve(volume)->points.size() == 2 && ui.engine.project.curve(volume)->points.front().value < -3
                && ui.undoHistory.size() == history + 1, "dragging an explicitly authored start node preserves both endpoints through the real node gesture / " + ui.status
                    + " / points " + juce::String((int) ui.engine.project.curve(volume)->points.size()) + " / Undo " + juce::String((int) (ui.undoHistory.size() - history)));
            ui.undo(false); check(beforeDrag == juce::JSON::toString(projectToJson(ui.engine.project)), "Undo of an endpoint drag restores both authored nodes");
            ui.timeline.selectAutomationCurve(volume);
            const juce::Point<float> finish {(float) (endpointClip.end() * ui.timeline.pixelsPerSecond), ui.timeline.automationY(0, -3, 0)};
            const auto finishDestination = finish.translated(0, 8);
            ui.timeline.mouseDown(event(finish, finish, false)); ui.timeline.mouseDrag(event(finishDestination, finish, true)); ui.timeline.mouseUp(event(finishDestination, finish, true));
            const auto* endCurve = ui.engine.project.curve(volume);
            check(endCurve && endCurve->points.size() == 2 && endCurve->points.back().value < -3
                && close(ui.engine.project.automationSessionTime(volume, endCurve->points.back().time), endpointClip.end())
                && ui.undoHistory.size() == history + 1,
                "vertical end-handle drag retains the exact fractional clip end and never reads the silent exterior");
            ui.undo(false); check(beforeDrag == juce::JSON::toString(projectToJson(ui.engine.project)), "Undo of the end-handle drag restores both authored nodes");
            if (variant == 0)
            {
                ui.timeline.selectAutomationCurve(volume); ui.refresh();
                auto output = report.getSiblingFile("new-curve-endpoints.png").createOutputStream(); juce::PNGImageFormat image;
                check(output && image.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *output), "capture two explicitly authored endpoint nodes below the title and above its toolbar");
            }
            // Existing curves are not padded, reset or rewritten merely by viewing them.
            auto* existing = ui.engine.project.curve(volume); existing->points.clear(); existing->setPoint(2, -7);
            const auto legacy = juce::JSON::toString(projectToJson(ui.engine.project)); ui.timeline.selectAutomationCurve(volume);
            check(ui.timeline.automationViewCurve(volume).points.size() == 1 && legacy == juce::JSON::toString(projectToJson(ui.engine.project)),
                "existing one-node curves retain their original shape and saved points");
        }
        ui.selectedAutomationTarget = -1;
    }
    {
        AutomationCurve base; base.enabled = true; base.setPoint(0, -60); base.setPoint(10, 0);
        check(base.interpolatedValue(5, 0) == -30, "unbent automation preserves the existing decibel interpolation");
        for (double bend : {-1., -.4, .4, .9, 1.})
        {
            auto shaped = base; shaped.points[0].bend = bend;
            const auto slice = shaped.sliced(2, 8, 0);
            bool retained = true;
            for (int n = 0; n <= 60; ++n) retained &= std::abs(slice.interpolatedValue(n * .1, 0) - shaped.interpolatedValue(2 + n * .1, 0)) < .0001f;
            check(retained, "slicing a bent or hard-cut segment preserves every sampled value: " + juce::String(bend));
            auto insertedNode = shaped; insertedNode.setPoint(4, shaped.interpolatedValue(4, 0));
            bool same = true; for (int n = 0; n <= 100; ++n) same &= std::abs(insertedNode.interpolatedValue(n * .1, 0) - shaped.interpolatedValue(n * .1, 0)) < .0001f;
            check(same, "adding a node on a bent segment preserves its sound on both sides");
            auto entire = shaped.sliced(0, 10, 0);
            check(std::abs(entire.interpolatedValue(9.999, 0) - shaped.interpolatedValue(9.999, 0)) < .0001f && entire.interpolatedValue(10, 0) == 0,
                "a slice ending at the second node preserves the hard-cut boundary");
        }
        Clip clip; clip.media = input.clips[0].media; clip.length = 12; clip.fadeIn = clip.fadeOut = 0;
        clip.controls.automation[1] = base;
        ui.engine.project = {}; ui.engine.project.followTrackTempo = false; ui.engine.project.snap = false;
        ui.engine.project.clips = {clip}; ui.selectedClip = clip.id; ui.selectedTransition.clear(); ui.timeline.pixelsPerSecond = 32;
        ui.undoHistory.clear(); ui.redoHistory.clear(); ui.viewport.setViewPosition(0, 0); ui.refresh();
        const AutomationAddress address {0, AutomationTarget::bass, {}, {}, clip.id};
        ui.timeline.selectAutomationCurve(address);
        auto onCurve = [&]
        {
            const float value = ui.engine.project.curve(address)->interpolatedValue(5, 0);
            return juce::Point<float> {160, ui.timeline.automationY(0, value, 1)};
        };
        auto dragBend = [&](float delta)
        {
            const auto curveDown = onCurve(); const auto to = curveDown.translated(0, delta);
            ui.timeline.mouseDown(event(curveDown, curveDown, false)); ui.timeline.mouseDrag(event(to, curveDown, true)); ui.timeline.mouseUp(event(to, curveDown, true));
        };
        const auto unchanged = juce::JSON::toString(projectToJson(ui.engine.project));
        const auto curveDown = onCurve(); ui.timeline.mouseDown(event(curveDown, curveDown, false)); ui.timeline.mouseUp(event(curveDown, curveDown, false));
        check(unchanged == juce::JSON::toString(projectToJson(ui.engine.project)) && ui.undoHistory.empty(), "clicking a curve selects it without writing a bend or Undo");
        dragBend(-30);
        auto* bent = ui.engine.project.curve(address);
        check(bent->points[0].bend < 0 && bent->interpolatedValue(5, 0) > -30 && ui.undoHistory.size() == 1
            && bent->points[0].time == 0 && bent->points[0].value == -60 && bent->points[1].time == 10 && bent->points[1].value == 0,
            "native upward line dragging brings bass in earlier with fixed end nodes and one Undo");
        auto captureBend = [&](const juce::String& name) { auto output = report.getSiblingFile(name).createOutputStream(); juce::PNGImageFormat png; check(output && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *output), "capture " + name); };
        captureBend("curve-bend-gradual.png");
        ui.undo(false); check(ui.engine.project.curve(address)->points[0].bend == 0, "Undo restores the original curve bend");
        ui.undo(true); check(ui.engine.project.curve(address)->points[0].bend < 0, "Redo restores the edited curve bend");
        dragBend(200); bent = ui.engine.project.curve(address);
        check(bent->points[0].bend == 1 && bent->interpolatedValue(9.999999, 0) == -60 && bent->interpolatedValue(10, 0) == 0
            && ui.engine.automationValue(address, 9.9) == -60 && ui.engine.automationValue(address, 10) == 0,
            "native downward dragging reaches a true held-off to on step at node two in engine evaluation");
        const auto path = ui.timeline.automationPath(address, 0, 12);
        juce::Point<float> nearest; path.getNearestPoint({320, ui.timeline.automationY(0, -6, 1)}, nearest);
        check(std::abs(nearest.x - 320) < .01, "the hard-cut path draws a vertical edge at the exact second node");
        captureBend("curve-bend-hard-cut.png");
        juce::AudioBuffer<float> pcm(2, 512); juce::MidiBuffer midi;
        ui.engine.project.masterDb = 0;
        auto renderedLevel = [&](double time)
        {
            double level = 0;
            for (int block = 0; block < 24; ++block)
            {
                ui.engine.render(pcm, 512, time + block * 512 / ui.engine.sampleRate, midi);
                if (block >= 16) level += pcm.getRMSLevel(0, 0, 512);
            }
            return level / 8;
        };
        const auto off = renderedLevel(9), on = renderedLevel(10.2);
        check(on > .01f && off < on * .2f, "rendered low-frequency PCM is suppressed before the bass snap and restored after it");
        const auto saved = projectToJson(ui.engine.project); const auto restored = parseProject(juce::JSON::toString(saved));
        check(restored.error.isEmpty() && restored.clips[0].clip.controls.automation[1].points[0].bend == 1,
            "session 29 round trip preserves hard-cut curvature");
        auto malformedBend = juce::JSON::parse(juce::JSON::toString(saved)); malformedBend["clips"][0]["controls"]["automation"]["bass"]["points"][0].getDynamicObject()->setProperty("bend", 2.);
        check(parseProject(juce::JSON::toString(malformedBend)).error.isNotEmpty(), "invalid saved curvature is rejected");
        dragBend(-40); check(ui.engine.project.curve(address)->points[0].bend < 1, "a hard-cut segment can be dragged back into a gradual curve");
        const auto beforeExport = juce::JSON::toString(projectToJson(ui.engine.project)); ui.engine.exporting = true; dragBend(30); ui.engine.exporting = false;
        check(beforeExport == juce::JSON::toString(projectToJson(ui.engine.project)), "export protection blocks curve bending");
        auto legacy = juce::JSON::parse(unchanged); legacy.getDynamicObject()->setProperty("version", 28);
        const auto oldSession = parseProject(juce::JSON::toString(legacy));
        check(oldSession.error.isEmpty() && oldSession.clips[0].clip.controls.automation[1].interpolatedValue(5, 0) == -30,
            "version 28 sessions without bend retain their original interpolation");
        // Generated envelopes retain independent incoming/outgoing curvature.
        auto generated = prepareTempoMap(input, true, 16, 0, 0, {}, TransitionPreset::crossfade);
        check(generated.error.isEmpty(), "prepare a generated transition for bend editing");
        ui.engine.project = generated.project; auto& link = ui.engine.project.transitionLinks[0];
        TransitionBandSettings setting {TransitionBandMode::crossfade};
        check(ui.engine.project.setTransitionBand(link.incoming, 1, setting).wasOk(), "create a generated bass fade for bending");
        const auto generatedIncoming = ui.engine.project.transitionLinks[0].incoming;
        auto* in = ui.engine.project.clipFor(generatedIncoming); ui.selectedClip = generatedIncoming; ui.selectedTransition.clear();
        const AutomationAddress generatedAddress {in->track, AutomationTarget::bass, {}, {}, generatedIncoming};
        const auto range = ui.engine.project.transitionRange(ui.engine.project.transitionLinks[0]);
        ui.timeline.selectAutomationCurve(generatedAddress); ui.refresh();
        const auto initialEnvelope = ui.engine.project.transitionPlaybackFor(*in).pairs[0][1];
        const double middle = (range.getStart() + range.getEnd()) / 2; const float initialGain = initialEnvelope.gainAt(middle);
        const juce::Point<float> generatedDown {(float) (middle * ui.timeline.pixelsPerSecond), ui.timeline.automationY(in->track, juce::Decibels::gainToDecibels(initialGain), 1)};
        ui.timeline.mouseDown(event(generatedDown, generatedDown, false));
        ui.timeline.beginCurveBend(generatedAddress,generatedDown);
        ui.timeline.mouseDrag(event(generatedDown.translated(0, 90), generatedDown, true)); ui.timeline.mouseUp(event(generatedDown.translated(0, 90), generatedDown, true));
        in = ui.engine.project.clipFor(generatedIncoming);
        const auto& nodes = in->controls.automation[1];
        check(!ui.engine.project.transitionLinks[0].bands[1] && nodes.active()
            && std::any_of(nodes.points.begin(),nodes.points.end(),[](const auto& point) { return point.bend == 1; }),
            "legacy incoming bass bend materialises the editable ordinary shape without retaining a separate generator");
        const auto reopenedGenerated = parseProject(juce::JSON::toString(projectToJson(ui.engine.project),true,17));
        check(reopenedGenerated.error.isEmpty() && !reopenedGenerated.project.transitionLinks[0].bands[1]
            && std::any_of(reopenedGenerated.clips[1].clip.controls.automation[1].points.begin(),reopenedGenerated.clips[1].clip.controls.automation[1].points.end(),[](const auto& point) { return point.bend == 1; }),
            "ordinary converted bends survive save/reopen: " + reopenedGenerated.error);
        ui.engine.project.transitionLinks[0].locked = true;
        const auto protectedState = juce::JSON::toString(projectToJson(ui.engine.project));
        ui.timeline.beginCurveBend(generatedAddress, generatedDown); ui.timeline.mouseDrag(event(generatedDown.translated(0, -80), generatedDown, true)); ui.timeline.mouseUp(event(generatedDown.translated(0, -80), generatedDown, true));
        check(protectedState == juce::JSON::toString(projectToJson(ui.engine.project)), "locked transitions reject curvature edits");
        ui.selectedAutomationTarget = -1;
    }
    ui.engine.project = changed; ui.selectedClip = changed.clips[0].id; ui.timeline.pixelsPerSecond = 10; ui.refresh();
    const auto snapshot = ui.createComponentSnapshot(ui.getLocalBounds()); auto out = report.getSiblingFile("alignment-markers.png").createOutputStream(); juce::PNGImageFormat png;
    check(out && png.writeImageToStream(snapshot, *out), "capture production UI with linked alignment markers and repositioned tracks");
}
void runRippleTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    const auto json = [](const Project& p) { return juce::JSON::toString(projectToJson(p), true, 17); };
    auto media = std::make_shared<Media>(); media->sampleRate = 8000; media->manualBpm = 120; media->manualBeatOrigin = .213;
    media->name = "Ripple fixture"; media->file = report.getSiblingFile("ripple-source.wav"); media->samples.setSize(1, 8000 * 24);
    for (int i = 0; i < media->frames(); ++i) media->samples.setSample(0, i, .12f + .08f * std::sin((float) (i * .031)));
    {
        juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream = media->file.createOutputStream();
        auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(1).withBitsPerSample(24));
        check(writer && writer->writeFromAudioSampleBuffer(media->samples, 0, media->frames()), "write owned ripple source for actual Save/Open");
    }
    Project base; base.transitionLinksKnown = true;
    base.tempo = {120, {{0, 120, false}, {8, 150, true}, {14, 132, true}, {20, 128, false}}, 1};
    auto clock = std::make_shared<const TempoMap>(base.tempo);
    const std::array<double, 5> starts {0, 4, 8, 12, 8}, lengths {3, 2, 8, 8, 2};
    for (int n = 0; n < 5; ++n)
    {
        Clip c; c.media = media; c.start = starts[(size_t) n]; c.length = lengths[(size_t) n]; c.track = n % trackCount;
        c.beatmatched = true; c.tempoSourceBpm = 120; c.tempoMap = clock; c.rate = c.sourceElapsed(c.length) / c.length;
        c.controls.automation[0] = {true, {{0, -7, .4}, {3, -3}, {8, -9}}};
        c.controls.pan = .05f * n; c.audioSlip = .017;
        base.clips.push_back(c);
    }
    base.clips[2].outroAnchor = base.clips[2].sourceElapsed(4); base.clips[3].introAnchor = 0;
    auto prepared = std::make_shared<PreparedAudio>(); prepared->samples.makeCopyOf(media->samples); prepared->clipLocal = true;
    base.clips[2].prepared = prepared;
    base.transitionLinks = {{base.clips[2].id, base.clips[3].id}};
    check(base.validateTransitionLinks().wasOk(), "ripple fixture contains a genuine later four-second linked frame");
    const auto removeId = base.clips[1].id;
    base.labels = {{juce::Uuid().toString(), 2, "Before"}, {juce::Uuid().toString(), 4, "Removed"}, {juce::Uuid().toString(), 6, "Seam"}, {juce::Uuid().toString(), 15, "Later"}};
    base.masterAutomation = {true, {{0, -3, -.5}, {4, -6, 1}, {4.25, -4, .3}, {10, -8, 1}, {14, -2, 1}, {14.25, -5}, {19, -4}}};
    base.tracks[1].automation[0] = {true, {{0, -2, -.6}, {12, -10, .4}, {20, -3}}};
    base.tracks[2].pluginAutomation.push_back({"fixture-slot", "fixture-param", "Fixture FX", .4f, {false, {{0, .2f, .6}, {9, .7f}, {19, .3f}}}});
    auto ripple = base;
    auto result = ripple.closeTimeRange(4, 6, removeId);
    check(result.wasOk(), "global ripple removes an isolated segment through a changing tempo ramp: " + result.getErrorMessage());
    if (result.failed()) return;
    check(ripple.clips.size() == 4 && !ripple.clipFor(removeId) && ripple.clips[1].start == 6 && ripple.clips.back().start == 6,
        "all later lanes move by elapsed time including simultaneous segments, independent of playlist/lane order");
    bool ownership = true;
    for (const auto& c : ripple.clips)
    {
        const auto& old = *base.clipFor(c.id);
        ownership &= c.effects == old.effects && c.prepared == old.prepared && c.length == old.length && c.rate == old.rate
            && c.offset == old.offset && c.audioSlip == old.audioSlip && c.controls.automation[0].points[1].time == old.controls.automation[0].points[1].time;
        for (int n = 0; n <= 16; ++n)
            ownership &= std::abs(c.sourceElapsed(c.length * n / 16) - old.sourceElapsed(old.length * n / 16)) < 1.e-8;
    }
    check(ownership, "ripple preserves source clocks, manual phase/slip, source-owned curves, effects and prepared payload ownership");
    const auto beforeRange = base.transitionRange(base.transitionLinks[0]), afterRange = ripple.transitionRange(ripple.transitionLinks[0]);
    check(ripple.transitionLinks == base.transitionLinks && afterRange.getStart() == beforeRange.getStart() - 2 && afterRange.getEnd() == beforeRange.getEnd() - 2,
        "a complete later linked pair keeps its identities, treatment and exact relative frame geometry");
    check(ripple.labels.size() == 3 && ripple.labels[0].time == 2 && ripple.labels[1].time == 4 && ripple.labels[2].time == 13
        && ripple.labels[2].id == base.labels[3].id && ripple.tempo.origin == 1, "labels use half-open deletion, preserve IDs and move with the timeline; earlier origin stays fixed");
    double curveError = 0, tempoError = 0;
    for (double t = 0; t < 20; t += .017)
    {
        if (t >= 4 && t < 6) continue;
        const double shifted = t < 4 ? t : t - 2;
        tempoError = std::max(tempoError, std::abs(base.tempo.bpmAt(t) - ripple.tempo.bpmAt(shifted)));
        const std::array<const AutomationCurve*, 3> old {&base.masterAutomation, &base.tracks[1].automation[0], &base.tracks[2].pluginAutomation[0].curve};
        const std::array<const AutomationCurve*, 3> now {&ripple.masterAutomation, &ripple.tracks[1].automation[0], &ripple.tracks[2].pluginAutomation[0].curve};
        for (int n = 0; n < 3; ++n) curveError = std::max(curveError, (double) std::abs(old[(size_t) n]->interpolatedValue(t, 0) - now[(size_t) n]->interpolatedValue(shifted, 0)));
    }
    check(curveError < 2.e-6 && tempoError < 1.e-8, "tempo ramps and disabled/enabled master/lane/FX bends and hard steps retain their values on both sides: " + juce::String(curveError, 8));
    bool seams = true;
    for (double t : {std::nextafter(4.0, 0.0), 6.0, std::nextafter(14.0, 0.0), 14.0, 14.25, 20.0})
        seams &= std::abs(base.masterAutomation.interpolatedValue(t, 0) - ripple.masterAutomation.interpolatedValue(t < 4 ? t : t - 2, 0)) < 2.e-6;
    check(seams, "splice preserves exact seam and existing step boundaries including the adjacent representable left limit");
    // The model-only FX lane has no actual plugin. Remove it before document/UI
    // admission; real plugin ownership is exercised by the full native gate.
    base.tracks[2].pluginAutomation.clear(); ripple.tracks[2].pluginAutomation.clear();
    const auto parsed = parseProject(json(ripple));
    check(parsed.error.isEmpty(), "ripple document validates all captured clocks and linked geometry in the unchanged saved format: " + parsed.error);
    auto dry = std::make_unique<Engine>(juce::File(), false); auto wet = std::make_unique<Engine>(juce::File(), false);
    dry->sampleRate = wet->sampleRate = 8000; dry->project = base; wet->project = ripple;
    juce::AudioBuffer<float> a(2, 128), b(2, 128); juce::MidiBuffer midi;
    for (double time : {2.0, 8.5, 12.5, 15.0})
    {
        dry->resetPlugins(); wet->resetPlugins(); dry->render(a, 128, time, midi); wet->render(b, 128, time < 4 ? time : time - 2, midi);
        double difference = 0;
        for (int ch = 0; ch < 2; ++ch) for (int n = 0; n < 128; ++n) difference = std::max(difference, (double) std::abs(a.getSample(ch, n) - b.getSample(ch, n)));
        check(difference < 2.e-6 && a.getRMSLevel(0, 0, 128) > .001, "production PCM preserves earlier and shifted pair audio/automation at " + juce::String(time) + ": " + juce::String(difference, 8));
    }
    auto gap = base; gap.clips.erase(gap.clips.begin() + 1); const auto gapBefore = json(gap);
    check(gap.closeTimeRange(4, 6).wasOk() && json(gap) == json(ripple), "closing the same empty range has exactly the same global transaction as ripple deletion");
    auto head = base; head.clips.erase(head.clips.begin(), head.clips.begin() + 2);
    check(head.closeTimeRange(0, 8).wasOk() && head.clips[0].start == 0 && head.tempo.origin == 0 && parseProject(json(head)).error.isEmpty(),
        "closing initial silence clamps the musical origin and preserves a pair beginning at zero");
    for (int failure = 0; failure < 12; ++failure)
    {
        auto rejected = base; double begin = 4, end = 6; juce::String id = removeId;
        if (failure == 0) rejected.clips.back().start = 5; // other-lane overlap
        if (failure == 1) { rejected.transitionLinks[0].locked = true; }
        if (failure == 2) { rejected.transitionLinks.push_back({removeId, rejected.clips[2].id}); }
        if (failure == 3) rejected.clips.back().pendingPlacement = PendingPlacement::move;
        if (failure == 4) rejected.clips.back().elasticPlayback = true;
        if (failure == 5) { begin = std::numeric_limits<double>::quiet_NaN(); }
        if (failure == 6) { begin = 3; }
        if (failure == 7) { id = "stale-id"; }
        if (failure == 8) rejected.transitionLinks.push_back({rejected.clips[0].id, rejected.clips[2].id});
        if (failure == 9) { auto stale = std::make_shared<TempoMap>(*clock); stale->points[1].bpm = 151; rejected.clips[2].tempoMap = stale; }
        if (failure == 10) rejected.clips.back().media = std::make_shared<Media>();
        if (failure == 11) end = begin;
        const auto before = json(rejected); const auto denied = rejected.closeTimeRange(begin, end, id);
        check(denied.failed() && json(rejected) == before, "global ripple rejects atomically: overlap/lock/pair/preparation/elastic/invalid/stale case " + juce::String(failure));
    }
    auto owned = std::make_unique<Workstation>(false); auto& ui = *owned; ui.stopTimer(); ui.setSize(1920, 1080);
    ui.engine.project = base; ui.engine.library = {media}; ui.selectedClip = removeId; ui.selectTrack(base.clips[1].track); ui.engine.position = 10; ui.refresh();
    ui.clipClipboard = base.clips[0]; const auto clipboardId = ui.clipClipboard->id;
    ui.editingRipple.onClick();
    check(json(ui.engine.project) == json(ripple) && ui.undoHistory.size() == 1 && ui.engine.position == 8 && ui.clipClipboard->id == clipboardId,
        "Editing ripple uses the canonical transaction, folds the playhead, preserves clipboard and creates exactly one Undo");
    ui.undo(false); check(json(ui.engine.project) == json(base), "Undo restores complete ripple source/effects/automation/tempo/labels/pairs");
    ui.undo(true); check(json(ui.engine.project) == json(ripple), "Redo restores complete ripple document");
    ui.engine.project = base; ui.undoHistory.clear(); ui.selectedClip = removeId;
    ui.menuItemSelected(90, 3); check(json(ui.engine.project) == json(base), "ripple top-level command rejects an unrelated menu owner");
    ui.menuItemSelected(90, 1); check(json(ui.engine.project) == json(ripple) && ui.undoHistory.size() == 1, "Edit menu shares the canonical ripple transaction");
    ui.engine.project = base; ui.undoHistory.clear(); ui.selectedClip = removeId;
    ui.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey, juce::ModifierKeys(juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier), 0));
    check(json(ui.engine.project) == json(ripple) && ui.undoHistory.size() == 1, "explicit Ctrl+Shift+Delete shares ripple ownership while ordinary Delete keeps its gap");
    ui.engine.project = base; ui.undoHistory.clear(); ui.redoHistory.clear(); ui.selectedClip = removeId; ui.engine.automationRecordMode = AutomationRecordMode::touch;
    ui.editingRipple.onClick(); check(json(ui.engine.project) == json(base) && ui.undoHistory.empty(), "recording mode rejects ripple before history or document mutation");
    ui.engine.automationRecordMode = AutomationRecordMode::read; ui.performTimelineAction(TimelineAction::cut, 4, 1);
    check(ui.engine.project.clips[1].start == 8 && ui.clipClipboard->id == removeId, "ordinary Cut still leaves its gap and keeps later segment positions");
    ui.selectedClip = base.clips[2].id; ui.engine.position = 5; ui.refresh(); ui.editingCloseGap.onClick();
    check(ui.engine.project.clips[1].start == 3 && ui.engine.position == 3, "Close gap before segment finds the global earlier end and clamps a playhead inside the removed silence");
    const auto session = report.getSiblingFile("ripple.mixworks"); auto reopened = std::make_unique<Engine>(juce::File(), false);
    const auto saveError = ui.engine.saveProject(session), openError = reopened->restoreProject(session.loadFileAsString());
    check(saveError.isEmpty() && openError.isEmpty() && reopened->project.clips.size() == ui.engine.project.clips.size()
        && reopened->project.transitionLinks == ui.engine.project.transitionLinks, "actual Save/Open restores globally closed time and intact pairs: " + saveError + openError);
    const auto exported = report.getSiblingFile("ripple-export.wav");
    const auto exportError = reopened->exportWav(exported); juce::AudioFormatManager formats; formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(exported));
    check(exportError.isEmpty() && reader && std::abs(reader->lengthInSamples / reader->sampleRate - reopened->project.audibleEnd()) < .001,
        "production export of the reopened closed-time arrangement has the expected audible length: " + exportError);
    ui.selectedClip = base.clips[2].id; ui.selectTrack(base.clips[2].track); ui.showInspectorPage(0); ui.refresh();
    for (const int width : {1250, 1920})
    {
        ui.setSize(width, 1080); ui.refresh(); ui.editingViewport.setViewPosition(0, 120);
        auto stream = report.getSiblingFile("ripple-editing-" + juce::String(width) + ".png").createOutputStream(); juce::PNGImageFormat png;
        check(stream && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *stream), "capture explicit ripple and gap commands at width " + juce::String(width));
    }
}
void runTimelineEditingSuite(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    // Keep suite dispatch outside fixture frames: each eight-lane Project/Engine
    // belongs to one case group, not the stack of every nested group.
    runRippleTests(check, report);
    runSectionFlagTests(check, report);
    runSourceSectionTests(check, report);
    runPresetNodeTests(check, report);
    runEditSelectionTests(check, report);
    runHotcueTests(check, report);
    runLaunchTests(check, report);
    runLiveHotcueTests(check, report);
    runArrangementInsertionTests(check, report);
    runAudioSlipTests(check, report);
    runTransitionCutTests(check, report);
    runTransitionBandTests(check, report);
    runTransitionFilterTests(check, report);
    runTransitionNavigationTests(check, report);
    runRehearsalContextTests(check, report);
    runTransitionSwapTests(check, report);
    runBarNavigationTests(check, report);
    runTimelineEditingTests(check, report);
    runPrerollTests(check, report);
    // This last native workflow runs and stops JUCE's process-wide dispatch loop.
    runTransitionRehearsalTests(check, report);
}
}
