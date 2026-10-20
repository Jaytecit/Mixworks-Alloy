#include "Workstation.h"
#include "Beatmatch.h"
#include <cmath>
#include <limits>

namespace mixworks
{
void runTransitionBandTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runTransitionFilterTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runTransitionNavigationTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runRehearsalContextTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runAudioSlipTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runTransitionCutTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runTransitionSwapTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runBarNavigationTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runTransitionRehearsalTests(const std::function<void(bool, const juce::String&)>&, const juce::File&);
void runTimelineEditingTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
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
        check(local.applyTransitionPreset(first.incoming, TransitionPreset::bassSwap).wasOk() && local.transitionFor(first.incoming)->bassSwap.has_value(),
            "selected Bass swap creates explicit paired source endpoints");
        check(local.clips[0].fadeOut == 0 && local.clips[1].fadeIn == 0 && local.clips[0].controls.automation[2].points[0].value == -3,
            "local Bass swap retains the bass-only contract and unrelated manual mid automation");
        check(local.applyTransitionPreset(first.incoming, TransitionPreset::crossfade).wasOk() && !local.transitionFor(first.incoming)->bassSwap
            && local.clips[0].controls.automation[1].points.empty() && local.clips[1].controls.automation[1].points.empty(),
            "switching Bass swap to Crossfade removes only its owned bass points without leaving an incoming mute");
        check(local.clips[0].fadeOut > 0 && local.clips[1].fadeIn > 0 && local.transitionLinks[1] == neighbour,
            "local Crossfade sets both fades and preserves the other relationship");
        check(local.applyTransitionPreset(first.incoming, TransitionPreset::alignOnly).wasOk() && local.clips[0].fadeOut == 0 && local.clips[1].fadeIn == 0
            && std::abs(local.transitionRange(*local.transitionFor(first.incoming)).getEnd() - range.getEnd()) < 1e-7,
            "None clears pair effects while keeping the frame length and alignment");
        const auto frozen = juce::JSON::toString(projectToJson(local));
        check(local.setTransitionLength(first.incoming, 100000).failed() && juce::JSON::toString(projectToJson(local)) == frozen,
            "an unavailable selected length rejects atomically instead of moving or trimming songs");
        check(local.setTransitionLength(first.incoming, std::numeric_limits<double>::quiet_NaN()).failed(), "nonfinite selected lengths reject");
        auto manual = local; manual.clips[1].controls.automation[1].setPoint(manual.clips[1].sourceElapsed(begin - manual.clips[1].start + 1), -4);
        const auto manualBefore = juce::JSON::toString(projectToJson(manual));
        check(manual.applyTransitionPreset(first.incoming, TransitionPreset::bassSwap).failed() && juce::JSON::toString(projectToJson(manual)) == manualBefore,
            "local Bass swap reports a manual-point conflict and preserves the whole project");
        check(local.setTransitionLocked(first.incoming, true).wasOk(), "transition lock is owned by the relationship");
        const auto locked = juce::JSON::toString(projectToJson(local));
        check(local.editTransition(first.incoming, Project::TransitionEdit::moveWindow, -1, false).failed()
            && local.setTransitionLength(first.incoming, 8).failed() && local.applyTransitionPreset(first.incoming, TransitionPreset::crossfade).failed()
            && locked == juce::JSON::toString(projectToJson(local)), "locked local edits all reject without a partial mutation");
        check(local.moveClip(first.incoming, local.clips[1].start + 1, false).failed()
            && local.moveTransitionAnchor(first.outgoing, false, begin - 1, false).failed(), "locked tracks reject direct and linked marker movement");
        check(local.editTransition(neighbour.incoming, Project::TransitionEdit::moveOutgoing, 1, false).failed(),
            "an adjacent transition cannot move the shared locked song");
        auto automated = local; automated.clips[1].controls.automation[2].setPoint(1, -7);
        check(local.validateTransitionLocks(automated).failed(), "candidate commits cannot change a locked participant through neighbouring automation");
        auto tempo = local; tempo.tempo.points[0].bpm += 1;
        check(local.validateTransitionLocks(tempo).failed(), "protected transition rejects a changed global tempo map");
        const auto rebuilt = prepareTempoMap(local, true, 16, 0);
        check(rebuilt.error.isNotEmpty() && juce::JSON::toString(projectToJson(rebuilt.project)) == locked, "global Beatmatch cannot erase transition locks");
        const auto parsed = parseProject(locked); auto loaded = parsed.project;
        for (const auto& reference : parsed.clips) loaded.clips.push_back(reference.clip);
        check(parsed.error.isEmpty() && loaded.transitionFor(first.incoming) && loaded.transitionFor(first.incoming)->locked,
            "format 15 round-trips relationship locks and frame effects");
        auto bad = projectToJson(local); (*bad.getProperty("transitionLinks", {}).getArray())[0].getDynamicObject()->setProperty("locked", "yes");
        check(parseProject(juce::JSON::toString(bad)).error.isNotEmpty(), "malformed transition lock rejects instead of becoming unlocked");
        auto legacy = projectToJson(local); legacy.getDynamicObject()->setProperty("version", 14); legacy["tracks"].getArray()->removeRange(4, trackCount - 4);
        for (auto& item : *legacy.getProperty("transitionLinks", {}).getArray()) item.getDynamicObject()->removeProperty("locked");
        const auto older = parseProject(juce::JSON::toString(legacy));
        check(older.error.isEmpty() && !older.project.hasLockedTransitions(), "format 14 loads without inventing locks");
        check(local.setTransitionLocked(first.incoming, false).wasOk() && local.setTransitionLength(first.incoming, 4).wasOk(), "explicit unlock restores local editing");
    }
    {
        const auto swapped = prepareTempoMap(input, true, 16, 0, 0, {}, TransitionPreset::bassSwap);
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
        const auto swapped = prepareTempoMap(input, true, 16, 0, 0, {}, TransitionPreset::bassSwap);
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
        check(faded.error.isEmpty() && faded.project.clips[0].fadeOut > 0 && faded.project.clips[1].fadeIn > 0
            && !faded.project.clips[0].controls.automation[1].active(), "crossfade preset creates bounded fades without a bass swap");
    }
    auto insertion = input; for (auto& c : insertion.clips) c.pendingPlacement = PendingPlacement::append;
    const auto inserted = prepareTempoMap(insertion, false, 32);
    const auto& firstInserted = inserted.project.clips.front();
    check(inserted.error.isEmpty() && firstInserted.start >= 0 && firstInserted.offset == input.clips.front().offset
          && std::abs(firstInserted.length * firstInserted.rate - 48) < 1.0e-7
          && std::abs(std::remainder(inserted.project.tempo.beatsAt(firstInserted.start + firstInserted.firstDownbeatTime()), 4)) < 1.0e-7,
          "automatic first insertion retains its complete pickup and aligns the musical bar to the timeline");
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
          "later append placement uses full track duration regardless of alignment markers");
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
        if (snap) retained &= std::abs(std::remainder(moved.tempo.beatsAt(clip.start + clip.firstBeatTime()), 1)) < 1.0e-7;
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

    Workstation ui(false); ui.stopTimer(); ui.setSize(1440, 940); ui.engine.project = original;
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
        const auto preset = prepareTempoMap(input, true, 16, 0, 0, {}, TransitionPreset::bassSwap);
        for (const auto edit : {Project::TransitionEdit::moveWindow, Project::TransitionEdit::resizeStart, Project::TransitionEdit::resizeEnd,
             Project::TransitionEdit::moveOutgoing, Project::TransitionEdit::moveIncoming})
        {
            ui.engine.project = preset.project; ui.engine.project.snap = false; ui.selectAutomationTarget(-1); ui.refresh();
            const auto edge = ui.engine.project.transitionLinks[0]; const auto box = ui.timeline.transitionBounds(edge);
            auto point = box.withHeight(18).getCentre();
            if (edit == Project::TransitionEdit::resizeStart) point.x = box.getX() + 1;
            if (edit == Project::TransitionEdit::resizeEnd) point.x = box.getRight() - 1;
            if (edit == Project::TransitionEdit::moveOutgoing || edit == Project::TransitionEdit::moveIncoming)
                point = ui.timeline.transitionSongBounds(edge, edit == Project::TransitionEdit::moveOutgoing).getCentre();
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
        auto preset = prepareTempoMap(input, true, 16, 0, 0, {}, TransitionPreset::bassSwap);
        ui.engine.project = preset.project; ui.engine.project.snap = false; ui.selectedClip = preset.project.clips[1].id;
        ui.selectAutomationTarget((int) AutomationTarget::bass); ui.refresh();
        const auto& link = ui.engine.project.transitionLinks[0]; const auto* in = ui.engine.project.clipFor(link.incoming);
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
        const juce::Point<float> body {(float) ((middle.start + middle.length * 0.4) * 12), (float) ui.timeline.laneTop(1) + 105};
        const juce::Point<float> target {body.x + 29, body.y};
        const auto undos = ui.undoHistory.size();
        ui.timeline.mouseDown(event(body, body, false));
        ui.engine.project.masterDb = -7; ui.engine.project.clips[0].gainDb = -5; // A live control change during a gesture.
        ui.timeline.mouseDrag(event(target, body, true)); ui.timeline.mouseUp(event(target, body, true));
        check(linked(ui.engine.project) && ui.engine.project.clips[0].start > manualPair.clips[0].start
              && ui.engine.project.clips[2].start > manualPair.clips[2].start && ui.engine.project.masterDb == -7 && ui.engine.project.clips[0].gainDb == -5
              && ui.undoHistory.size() == undos + 1 && ui.engine.project.clips[1].introAnchor == manualPair.clips[1].introAnchor
              && (!snap || std::abs(std::remainder(ui.engine.project.tempo.beatsAt(ui.engine.project.clips[1].start + ui.engine.project.clips[1].firstBeatTime()), 1)) < 1.0e-7),
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
        const juce::Point<float> body {180, (float) ui.timeline.laneTop(0) + 100};
        ui.timeline.mouseMove(event(body, body, false));
        check(ui.timeline.getMouseCursor() == juce::MouseCursor(juce::MouseCursor::DraggingHandCursor), "clip bodies advertise movement while automation remains overlaid");
        ui.timeline.mouseDown(event(body, body, false)); ui.timeline.mouseDrag(event({body.x + 12, body.y}, body, true)); ui.timeline.mouseUp(event({body.x + 12, body.y}, body, true));
        check(ui.engine.project.clips[0].start > original.clips[0].start && ui.automationLane() == -1, "ordinary clip movement remains available after editing an automation marker");
        ui.selectAutomationTarget(-1);
    }
    {
        Workstation live(false); live.stopTimer(); live.engine.project = original;
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
    ui.performTimelineAction(TimelineAction::copy, 8, 0); ui.performTimelineAction(TimelineAction::paste, 5, 3);
    check(ui.engine.project.clips.size() == 4 && ui.engine.project.clips.back().id != a.id && ui.engine.project.clips.back().track == 3
          && ui.engine.project.clips.back().media == a.media, "context copy/paste creates an independently editable clip on the chosen lane");
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
        check(ui.engine.project.transitionFor(id)->bassSwap.has_value() && ui.undoHistory.size() == 2, "native preset selector applies only the selected pair with one Undo");
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
    ui.engine.project = changed; ui.selectedClip = changed.clips[0].id; ui.timeline.pixelsPerSecond = 10; ui.refresh();
    const auto snapshot = ui.createComponentSnapshot(ui.getLocalBounds()); auto out = report.getSiblingFile("alignment-markers.png").createOutputStream(); juce::PNGImageFormat png;
    check(out && png.writeImageToStream(snapshot, *out), "capture production UI with linked alignment markers and repositioned tracks");
}
void runTimelineEditingSuite(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    // Keep suite dispatch outside fixture frames: each eight-lane Project/Engine
    // belongs to one case group, not the stack of every nested group.
    runAudioSlipTests(check, report);
    runTransitionCutTests(check, report);
    runTransitionBandTests(check, report);
    runTransitionFilterTests(check, report);
    runTransitionNavigationTests(check, report);
    runRehearsalContextTests(check, report);
    runTransitionSwapTests(check, report);
    runBarNavigationTests(check, report);
    runTimelineEditingTests(check, report);
    // This last native workflow runs and stops JUCE's process-wide dispatch loop.
    runTransitionRehearsalTests(check, report);
}
}
