#include "Workstation.h"
#include "Beatmatch.h"
#include "AutoArrangement.h"
#include "PresetNodes.h"
#include <cmath>
#include <limits>
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#undef near
#endif

namespace mixworks
{
juce::var legacyLowPassFixture(juce::var, int);
void runTransitionBandTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    Project base; base.followTrackTempo = false; base.bpm = base.tempo.fallback = 120;
    for (int i = 0; i < 3; ++i)
    {
        auto media = std::make_shared<Media>(); media->file = report.getSiblingFile("band-source-" + juce::String(i) + ".wav");
        media->sampleRate = 8000; media->manualBpm = 120; media->manualBeatOrigin = 0; media->name = "Band source " + juce::String(i + 1);
        media->samples.setSize(1, 48 * 8000);
        for (int frame = 0; frame < media->frames(); ++frame)
            media->samples.setSample(0, frame, (float) (.1 * (std::sin(frame * 2 * juce::MathConstants<double>::pi * 40 / 8000)
                + std::sin(frame * 2 * juce::MathConstants<double>::pi * 1000 / 8000) + std::sin(frame * 2 * juce::MathConstants<double>::pi * 3000 / 8000))));
        Clip clip; clip.media = media; clip.track = i; clip.start = i * 32; clip.length = 48;
        clip.introAnchor = 0; clip.outroAnchor = 32; clip.fadeInAnchor = 0; clip.fadeOutAnchor = 48;
        base.clips.push_back(clip);
    }
    base.transitionLinksKnown = true;
    base.transitionLinks = {{base.clips[0].id, base.clips[1].id}, {base.clips[1].id, base.clips[2].id}};
    const auto id = base.clips[1].id;
    const auto stableJson = [](const Project& p) { return juce::JSON::toString(projectToJson(p), juce::JSON::FormatOptions().withMaxDecimalPlaces(17)); };
    const auto near = [](double a, double b) { return std::abs(a - b) < 1e-6; };
    auto presetMatrix = [&]
    {
        std::vector<std::pair<juce::String, Project>> fixtures;
        for (int preset = 0; preset < 3; ++preset)
        {
            auto project = base; project.applyTransitionPreset(id, (TransitionPreset) preset);
            fixtures.push_back({"builtin-" + juce::String(preset), project});
            TransitionStyle style; const auto captured = project.captureTransitionStyle(id, style);
            check(captured.wasOk(), "matrix captures builtin " + juce::String(preset));
            NamedTransitionStyle named {"Matrix " + juce::String(preset), style};
            auto namedProject = base;
            check(namedProject.putTransitionPreset(named).wasOk() && namedProject.applyTransitionStyle(id, namedProject.transitionPresets[0].style).wasOk(), "matrix applies named preset");
            fixtures.push_back({"named-" + juce::String(preset), namedProject});
            const auto file = report.getSiblingFile("matrix-" + juce::String(preset) + ".alloypreset");
            NamedTransitionStyle loaded;
            check(writeTransitionPreset(file, named).wasOk() && readTransitionPreset(file, loaded).wasOk(), "matrix exports and imports portable preset");
            auto portable = base; portable.applyTransitionStyle(id, loaded.style);
            fixtures.push_back({"portable-" + juce::String(preset), portable});
            for (int slot = 0; slot < 3; ++slot)
            {
                auto alternative = project;
                check(alternative.storeTransitionAlternative(id, slot).wasOk(), "matrix stores alternative " + juce::String(slot));
                alternative.applyTransitionPreset(id, TransitionPreset::alignOnly);
                const auto saved = alternative.transitionFor(id)->alternatives[(size_t) slot];
                check(saved && alternative.applyTransitionStyle(id, *saved).wasOk(), "matrix recalls alternative " + juce::String(slot));
                fixtures.push_back({"alternative-" + juce::String(preset) + "-" + juce::String(slot), alternative});
            }
        }
        for (int band = 0; band < 4; ++band)
            for (int mode = 0; mode < 8; ++mode)
            {
                const TransitionBandSettings setting {(TransitionBandMode) mode, .7, 0, .5};
                if (setting.validate(band).failed()) continue;
                auto project = base; project.applyTransitionPreset(id, TransitionPreset::alignOnly);
                if (mode == (int) TransitionBandMode::manual)
                    for (int side = 0; side < 2; ++side)
                    { auto& curve = project.clips[(size_t) side].controls.automation[(size_t) band]; curve.enabled = true; curve.setPoint(0, -3); curve.setPoint(40 - project.clips[(size_t) side].start, -9); }
                check(placeTransitionBandNodes(project, id, band, setting).wasOk(), "matrix places independent ordinary band preset " + juce::String(band) + "/" + juce::String(mode));
                fixtures.push_back({"custom-band-" + juce::String(band) + "-mode-" + juce::String(mode), project});
            }
        auto library = std::vector<std::shared_ptr<Media>> {base.clips[0].media->catalogueCopy(), base.clips[1].media->catalogueCopy()};
        for (int i = 0; i < 2; ++i)
        {
            library[(size_t) i]->samples.makeCopyOf(base.clips[(size_t) i].media->samples);
            library[(size_t) i]->manualKey = 0; library[(size_t) i]->analysisVersion = analysisRevision;
            const int bucket = std::max(1, library[(size_t) i]->frames() / 4000);
            library[(size_t) i]->waveformRms.assign((size_t) ((library[(size_t) i]->frames() + bucket - 1) / bucket), i == 0 ? .1f : .2f);
        }
        for (int style = 0; style < 6; ++style)
            for (int preset = 0; preset < (style == (int) AutoTransitionStyle::current ? 3 : 1); ++preset)
            {
                AutoArrangementOptions options; options.startingId = library[0]->id; options.trackCount = 2; options.overlapBeats = 32;
                options.sectionTransitions = false; options.transitions = (AutoTransitionStyle) style; options.preset = style == (int) AutoTransitionStyle::current ? (TransitionPreset) preset : TransitionPreset::crossfade;
                juce::Random random(42); auto generated = planAutoArrangement(library, options, random);
                check(generated.error.isEmpty() && generated.project.clips.size() == 2, "matrix Auto Arrange producer " + juce::String(style) + "/" + juce::String(preset) + ": " + generated.error);
                if (generated.project.clips.size() == 2) fixtures.push_back({"auto-" + juce::String(style) + "-" + juce::String(preset), generated.project});
            }
        juce::String matrix = "preset\tside\tparameter\tcoverage\n";
        juce::String amountMatrix = "preset\tside\tparameter\tamount coverage\n";
        auto ownedUi = std::make_unique<Workstation>(false); auto& ui = *ownedUi; ui.stopTimer(); ui.setSize(1440, 940); ui.timeline.pixelsPerSecond = 40;
        const auto mouse = juce::Desktop::getInstance().getMainMouseSource();
        auto event = [&](juce::Point<float> position, juce::Point<float> down, bool drag)
        { return juce::MouseEvent(mouse, position, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1, 0, 0, 0, 0, &ui.timeline, &ui.timeline, juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), 1, drag); };
        for (const auto& [name, fixture] : fixtures)
        {
            const auto incoming = fixture.transitionLinks[0].incoming;
            const auto range = fixture.transitionRange(fixture.transitionLinks[0]);
            for (int side = 0; side < 2; ++side) for (int band = 0; band < 4; ++band)
            {
                ui.engine.project = fixture; ui.engine.project.snap = false; ui.undoHistory.clear(); ui.redoHistory.clear();
                ui.selectedTransition = incoming; ui.transitionCurveOnlyBand = band; ui.transitionCurveShown = {true, true, true, true};
                const auto clip = ui.engine.project.clips[(size_t) side];
                const AutomationAddress address {clip.track, (AutomationTarget) band, {}, {}, clip.id};
                const auto prefix = name + "/" + juce::String(side) + "/" + juce::String(band);
                ui.timeline.selectAutomationCurve(address); ui.refresh();
                ui.viewport.setViewPosition(std::max(0, (int) (range.getStart() * 40) - 80), 0);
                const auto amounts = ui.timeline.cutAmountHandles();
                const auto amount = std::find_if(amounts.begin(), amounts.end(), [&](const auto& h) { return h.address == address; });
                if (amount == amounts.end()) amountMatrix += prefix.replaceCharacter('/', '\t') + "\tN/A: no hard cut produced\n";
                else
                {
                    const auto h = *amount; const auto original = stableJson(ui.engine.project);
                    const auto down = h.bounds.getCentre();
                    ui.timeline.mouseDown(event(down, down, false)); ui.timeline.mouseDrag(event(down.translated(37, -24), down, true)); ui.timeline.mouseUp(event(down.translated(37, -24), down, true));
                    const auto edited = stableJson(ui.engine.project);
                    check(edited != original && ui.undoHistory.size() == 1, prefix + " P50 actual preset amount gesture");
                    const auto now = ui.timeline.cutAmountHandles();
                    check(std::any_of(now.begin(), now.end(), [&](const auto& n) { return n.sameOwner(h) && n.time == h.time && n.value != h.value; }), prefix + " P50 cut time stays fixed");
                    ui.undo(false); check(stableJson(ui.engine.project) == original, prefix + " P50 exact Undo"); ui.undo(true);
                    check(stableJson(ui.engine.project) == edited, prefix + " P50 exact Redo");
                    TransitionStyle changed; ui.engine.project.captureTransitionStyle(incoming, changed);
                    check(ui.engine.project.applyTransitionStyle(incoming, changed).wasOk() && stableJson(ui.engine.project) == edited, prefix + " P50 reapply retains edited amount");
                    const auto parsed = parseProject(edited);
                    check(parsed.error.isEmpty() && parsed.project.transitionLinks == ui.engine.project.transitionLinks, prefix + " P50 paired settings serialize");
                    ui.engine.project.transitionLinks[0].locked = true; const auto locked = stableJson(ui.engine.project);
                    const auto current = ui.timeline.cutAmountHandles();
                    const auto same = std::find_if(current.begin(), current.end(), [&](const auto& n) { return n.sameOwner(h); });
                    check(same != current.end() && !ui.timeline.setCutAmount(*same, .5f, true) && stableJson(ui.engine.project) == locked, prefix + " P50 locked amount rejects atomically");
                    amountMatrix += prefix.replaceCharacter('/', '\t') + "\tamount drag/fixed time/Undo/Redo/lock/serialization/reapply checked\n";
                }
                // Keep the established P06 matrix independent of the new gesture's edits.
                ui.engine.project = fixture; ui.engine.project.snap = false; ui.undoHistory.clear(); ui.redoHistory.clear();
                ui.timeline.selectedCutAmount.reset(); ui.timeline.editingAutomationPoint.reset(); ui.selectedAutomationTarget = -1;
                const auto initial = ui.engine.project.curve(address);
                if (!ui.timeline.hasGeneratedAutomation(address) && initial->points.empty())
                { matrix += prefix.replaceCharacter('/', '\t') + "\tN/A: no curve produced\n"; continue; }
                ui.selectedAutomationTarget = -1; ui.refresh();
                const auto before = stableJson(ui.engine.project); const auto history = ui.undoHistory.size();
                const double time = range.getStart() + range.getLength() * .23;
                const float value = initial->valueAt(ui.engine.project.automationTime(address, time), ui.engine.project.manualValue(address));
                const juce::Point<float> line {(float) (time * 40), ui.timeline.automationY(clip.track, ui.timeline.automationDisplayValue(address, time, value), band)};
                const auto hit = ui.timeline.automationCurveAt(line);
                check(hit && *hit == address, prefix + " line hit resolves exact owner");
                ui.timeline.mouseDown(event(line, line, false)); ui.timeline.mouseUp(event(line, line, false));
                check(ui.timeline.automationNodesVisible(address) && !ui.timeline.automationViewCurve(address).points.empty(), prefix + " real line click reveals adjustment nodes");
                check(stableJson(ui.engine.project) == before && ui.undoHistory.size() == history, prefix + " selection is display-only");
                // Pick the loudest exposed handle so a downward edit is audible.
                const auto viewCurve = ui.timeline.automationViewCurve(address); int chosen = 0; float loudest = -100;
                for (int i = 0; i < (int) viewCurve.points.size(); ++i)
                {
                    const auto& node = viewCurve.points[(size_t) i];
                    const auto at = ui.engine.project.automationSessionTime(address, node.time);
                    const auto db = ui.timeline.automationDisplayValue(address, at, node.value);
                    if (db > loudest) { loudest = db; chosen = i; }
                }
                if (viewCurve.points.empty()) continue;
                const auto at = ui.engine.project.automationSessionTime(address, viewCurve.points[(size_t) chosen].time);
                const juce::Point<float> node {(float) (at * 40), ui.timeline.automationY(clip.track, loudest, band)};
                const auto down = event(node, node, false), drag = event(node.translated(0, 5), node, true);
                ui.timeline.mouseDown(down); ui.timeline.mouseDrag(drag); ui.timeline.mouseUp(drag);
                check(stableJson(ui.engine.project) != before && ui.undoHistory.size() == history + 1, prefix + " node drag saves one Undo transaction");
                const auto edited = stableJson(ui.engine.project);
                const auto reopened = parseProject(edited);
                const auto parsedClip = std::find_if(reopened.clips.begin(), reopened.clips.end(), [&](const auto& item) { return item.clip.id == address.clipId; });
                bool samePoints = parsedClip != reopened.clips.end();
                if (samePoints)
                {
                    const auto& points = parsedClip->clip.controls.automation[(size_t) band].points;
                    const auto& savedPoints = ui.engine.project.curve(address)->points;
                    samePoints = points.size() == savedPoints.size();
                    for (size_t i = 0; samePoints && i < points.size(); ++i) samePoints = near(points[i].time, savedPoints[i].time) && near(points[i].value, savedPoints[i].value);
                }
                check(reopened.error.isEmpty() && samePoints, prefix + " edited nodes survive session serialization: " + reopened.error);
                ui.undo(false); check(stableJson(ui.engine.project) == before, prefix + " Undo restores original generated/manual treatment");
                ui.undo(true); check(stableJson(ui.engine.project) == edited, prefix + " Redo restores edit");
                ui.engine.project = fixture; ui.engine.project.snap = false; ui.timeline.selectAutomationCurve(address);
                ui.addTimelineAutomation((AutomationTarget) band, time, clip.track);
                check(ui.timeline.editingAutomationPoint && ui.timeline.editingAutomationPoint->first == address && ui.timeline.automationNodesVisible(address), prefix + " marker inserts into selected curve");
                const auto saved = stableJson(ui.engine.project); const auto undo = ui.undoHistory.size();
                ui.engine.project.transitionLinks[0].locked = true;
                const auto locked = stableJson(ui.engine.project);
                ui.addTimelineAutomation((AutomationTarget) band, time + .5, clip.track);
                check(stableJson(ui.engine.project) == locked && ui.undoHistory.size() == undo, prefix + " lock rejects marker edit");
                ui.engine.project.transitionLinks[0].locked = false;
                TransitionStyle same; ui.engine.project.captureTransitionStyle(incoming, same);
                const auto regenerated = ui.engine.project.applyTransitionStyle(incoming, same);
                check(regenerated.wasOk() && stableJson(ui.engine.project) == saved, prefix + " reapplying treatment retains manual nodes");
                matrix += prefix.replaceCharacter('/', '\t') + "\tselection/reveal/drag/insert/lock/Undo/Redo/reopen/reapply checked\n";
            }
            matrix += name + "\tboth\tFilter\t" + (fixture.transitionLinks[0].filter.active() ? "inspector-owned paired filter; direct curve nodes remain open" : "N/A: no paired filter produced") + "\n";
        }
        check(report.getSiblingFile("transition-preset-matrix.tsv").replaceWithText(matrix), "write complete transition preset and parameter coverage matrix");
        check(report.getSiblingFile("p50-cut-amount-matrix.tsv").replaceWithText(amountMatrix), "write P50 every-preset/route/both-side amount matrix with absent cuts N/A");
    };
    presetMatrix();
    const auto samePoints = [&](const auto& a, const auto& b)
    { return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [&](const auto& x, const auto& y) { return near(x.time, y.time) && near(x.value, y.value) && near(x.bend, y.bend); }); };
    auto cutAmountTests = [&]
    {
        auto owned = std::make_unique<Workstation>(false); auto& ui = *owned; ui.stopTimer(); ui.setSize(1440, 940);
        ui.timeline.pixelsPerSecond = 40;
        const auto source = juce::Desktop::getInstance().getMainMouseSource();
        auto event = [&](juce::Point<float> p, juce::Point<float> down, bool drag)
        { return juce::MouseEvent(source, p, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::altModifier), 1, 0, 0, 0, 0, &ui.timeline, &ui.timeline, juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), 1, drag); };
        auto reset = [&]
        {
            ui.engine.project = base; ui.engine.project.snap = true; ui.selectedTransition = id; ui.selectedClip = id;
            ui.sessionFile = {}; ui.dirty = false;
            ui.selectedAutomationTarget = -1; ui.transitionCurveOnlyBand = -1; ui.transitionCurveShown = {true, true, true, true};
            ui.undoHistory.clear(); ui.redoHistory.clear(); ui.engine.position = 17.25; ui.refresh();
            ui.viewport.setViewPosition(1200, 0);
        };
        auto pcm = [](Project project)
        {
            for (int lane = 0; lane < trackCount; ++lane) project.tracks[(size_t) lane].mute = lane != 0;
            auto renderer = std::make_unique<Engine>(); renderer->sampleRate = 8000; renderer->project = std::move(project);
            juce::AudioBuffer<float> samples(2, 2048), block(2, 512); juce::MidiBuffer midi;
            for (int n = 0; n < 4; ++n)
            {
                renderer->render(block, 512, 42 + n * 512 / 8000., midi);
                for (int ch = 0; ch < 2; ++ch) samples.copyFrom(ch, n * 512, block, ch, 0, 512);
            }
            return samples;
        };
        for (int band = 0; band < 4; ++band) for (int side = 0; side < 2; ++side)
        {
            reset(); ui.engine.project.setTransitionBand(id, band, {TransitionBandMode::swap, .75, 2, .5}); ui.refresh();
            const auto handles = ui.timeline.cutAmountHandles();
            const auto found = std::find_if(handles.begin(), handles.end(), [&](const auto& h) { return (int) h.address.target == band && h.before == (side == 0); });
            const auto prefix = "P50 generated " + juce::String(band) + "/" + juce::String(side);
            check(found != handles.end(), prefix + " exposes amount separately from timing"); if (found == handles.end()) continue;
            const auto h = *found; const auto before = stableJson(ui.engine.project); const auto times = *ui.engine.project.transitionSwapTimes(*ui.engine.project.transitionFor(id), band);
            const auto down = h.bounds.getCentre();
            check(ui.timeline.cutAmountAt(down).has_value() && !ui.timeline.swapHandleAt(down), prefix + " has a distinct hit target");
            ui.timeline.mouseDown(event(down, down, false)); ui.timeline.mouseUp(event(down, down, false));
            check(stableJson(ui.engine.project) == before && ui.undoHistory.empty() && ui.engine.position == 17.25, prefix + " selection is view-only");
            ui.timeline.mouseDown(event(down, down, false));
            ui.timeline.mouseDrag(event(down.translated(90, -24), down, true));
            ui.timeline.mouseDrag(event(down.translated(-80, -48), down, true));
            ui.timeline.mouseUp(event(down.translated(-80, -48), down, true));
            const auto* link = ui.engine.project.transitionFor(id);
            const auto afterTimes = *ui.engine.project.transitionSwapTimes(*link, band);
            const auto* out = ui.engine.project.clipFor(link->outgoing); const auto* in = ui.engine.project.clipFor(link->incoming);
            check(near(link->bands[(size_t) band]->strength, .35) && times == afterTimes && ui.undoHistory.size() == 1
                && out->start == base.clips[0].start && in->start == base.clips[1].start && ui.engine.position == 17.25,
                prefix + " vertical multi-event drag changes paired depth only, ignoring X/Alt/Snap with one Undo");
            const auto a = ui.engine.project.transitionPlaybackFor(*out).pairs[1][(size_t) band];
            const auto b = ui.engine.project.transitionPlaybackFor(*in).pairs[0][(size_t) band];
            check(near(a.gainAt(a.end + .01), .65) && near(b.gainAt(b.begin - .01), .65), prefix + " both rendered envelopes retain 65% gain");
            const auto rendered = pcm(ui.engine.project);
            auto reference = std::make_unique<Project>(ui.engine.project); reference->transitionLinks[0].bands[(size_t) band].reset();
            if (band == 0) reference->clips[0].controls.gainDb = juce::Decibels::gainToDecibels(.65f);
            else reference->clips[0].controls.eqDb[(size_t) band - 1] = juce::Decibels::gainToDecibels(.65f);
            const auto expected = pcm(*reference); float difference = 0;
            for (int ch = 0; ch < 2; ++ch) for (int frame = 512; frame < 2048; ++frame) difference = std::max(difference, std::abs(rendered.getSample(ch, frame) - expected.getSample(ch, frame)));
            check(difference < 1e-5, prefix + " actual PCM agrees with the same manual remaining-gain setting");
            const auto edited = stableJson(ui.engine.project); ui.undo(false); check(stableJson(ui.engine.project) == before, prefix + " Undo restores exact treatment"); ui.undo(true);
            check(stableJson(ui.engine.project) == edited, prefix + " Redo restores amount");
            const auto parsed = parseProject(edited); check(parsed.error.isEmpty() && parsed.project.transitionLinks == ui.engine.project.transitionLinks, prefix + " amount and paired timing persist");
            auto fresh = ui.timeline.cutAmountHandles().front(); ui.timeline.setCutAmount(fresh, 1, true);
            fresh = ui.timeline.cutAmountHandles().front();
            check(fresh.value == 1 && ui.timeline.setCutAmount(fresh, 0, true), prefix + " zero-depth handle remains available to restore full cut");
            const auto full = ui.timeline.cutAmountHandles().front(); const auto fullBefore = stableJson(ui.engine.project);
            ui.engine.exporting = true; check(!ui.timeline.setCutAmount(full, .3f, true), prefix + " export blocks amount"); ui.engine.exporting = false;
            ui.busy = true; check(!ui.timeline.setCutAmount(full, .3f, true), prefix + " busy blocks amount"); ui.busy = false;
            ui.pendingTempo = 130; check(!ui.timeline.setCutAmount(full, .3f, true), prefix + " pending tempo blocks amount"); ui.pendingTempo = 0;
            ui.engine.project.transitionLinks[1].locked = true; check(!ui.timeline.setCutAmount(full, .3f, true), prefix + " neighbouring ownership lock blocks amount"); ui.engine.project.transitionLinks[1].locked = false;
            ui.engine.project.transitionLinks[0].locked = true; check(!ui.timeline.setCutAmount(full, .3f, true), prefix + " pair lock blocks amount"); ui.engine.project.transitionLinks[0].locked = false;
            check(stableJson(ui.engine.project) == fullBefore, prefix + " rejected edits preserve project");
            ui.timeline.selectedCutAmount = full; ui.timeline.keyPressed(juce::KeyPress(juce::KeyPress::upKey));
            check(near(ui.engine.project.transitionFor(id)->bands[(size_t) band]->strength, .99), prefix + " keyboard gives precise 1% remaining-gain increments");
            const auto nodelete = stableJson(ui.engine.project);
            check(ui.timeline.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey)) && stableJson(ui.engine.project) == nodelete, prefix + " amount Delete cannot delete its segment");
            ui.timeline.selectedCutAmount.reset(); check(ui.keyPressed(juce::KeyPress('D')) && ui.timeline.selectedCutAmount.has_value(), prefix + " D reaches amount selection without a mouse");
        }
        for (const auto target : {AutomationTarget::volume, AutomationTarget::bass, AutomationTarget::mid, AutomationTarget::treble, AutomationTarget::cutoff, AutomationTarget::pan, AutomationTarget::master})
            {
                reset(); ui.selectedTransition.clear(); ui.viewport.setViewPosition(0, 0);
                if (target != AutomationTarget::master)
                {
                    ui.engine.project.transitionLinks.clear();
                    auto& clip = ui.engine.project.clips[0]; clip.start = 3; clip.offset = 2; clip.rate = 1.25; clip.length = 32;
                }
                const AutomationAddress address {target == AutomationTarget::master ? -1 : 0, target, {}, {}, target != AutomationTarget::master ? base.clips[0].id : juce::String()};
                auto* curve = ui.engine.project.curve(address); curve->enabled = true;
                const bool gain = ui.timeline.automationUsesGain(target);
                curve->setPoint(8, gain ? 0 : -.5f); curve->setPoint(12, gain ? -60 : .5f); curve->points[0].bend = 1;
                ui.timeline.selectAutomationCurve(address); ui.refresh();
                const auto handles = ui.timeline.cutAmountHandles(); const auto prefix = "P50 manual " + juce::String((int) target);
                check(handles.size() == 2, prefix + " exposes both fixed step endpoints"); if (handles.size() != 2) continue;
                const auto h = handles.back(); const auto before = *ui.engine.project.curve(address);
                const auto down = h.bounds.getCentre(); const auto destination = down.translated(100, -30);
                ui.timeline.mouseDown(event(down, down, false)); ui.timeline.mouseDrag(event(destination, down, true)); ui.timeline.mouseUp(event(destination, down, true));
                const auto* after = ui.engine.project.curve(address);
                check(after->points.size() == 2 && after->points[0].time == before.points[0].time && after->points[1].time == before.points[1].time
                    && after->points[0].value == before.points[0].value && after->points[0].bend == 1 && after->points[1].value != before.points[1].value && ui.undoHistory.size() == 1,
                    prefix + " drag edits its endpoint value with every time, other endpoint and step shape fixed");
                const auto currentHandles = ui.timeline.cutAmountHandles();
                check(std::any_of(currentHandles.begin(), currentHandles.end(), [&](const auto& now) { return now.sameOwner(h); }), prefix + " exact project cut time stays fixed through the captured source clock");
                if (target == AutomationTarget::cutoff) check(ui.timeline.cutAmountText(*ui.timeline.selectedCutAmount).contains("HP") && !ui.timeline.cutAmountText(*ui.timeline.selectedCutAmount).contains("cut "), prefix + " Filter readout uses physical cutoff, not gain attenuation");
                const auto saved = stableJson(ui.engine.project); const auto parsed = parseProject(saved);
                check(parsed.error.isEmpty() && samePoints(address.clipId.isEmpty() ? parsed.project.curve(address)->points : parsed.clips[0].clip.controls.automation[(size_t) target].points, after->points), prefix + " source-owned values/times survive serialization");
                const auto changed = *after; ui.undo(false); check(samePoints(ui.engine.project.curve(address)->points, before.points), prefix + " Undo restores manual endpoints"); ui.undo(true);
                check(samePoints(ui.engine.project.curve(address)->points, changed.points), prefix + " Redo restores manual amount");
            }
        reset();
        {
            auto& link = ui.engine.project.transitionLinks[0]; const auto range = ui.engine.project.transitionRange(link);
            link.bands[1].reset(); link.bassSwap = initialiseTransition(*ui.engine.project.clipFor(link.outgoing), *ui.engine.project.clipFor(link.incoming), range.getLength(), TransitionPreset::bassSwap, range);
            ui.refresh(); const auto times = *link.bassSwap; const auto handles = ui.timeline.cutAmountHandles();
            check(handles.size() == 2 && handles.front().legacy, "P50 legacy paired bass exposes its actual endpoint owner");
            if (!handles.empty())
            {
                check(ui.timeline.setCutAmount(handles.front(), .4f, true) && *ui.engine.project.transitionFor(id)->bassSwap == times, "P50 legacy amount keeps every paired source time and metadata");
                const auto& pair = *ui.engine.project.transitionFor(id);
                for (const auto& clipId : {pair.outgoing, pair.incoming})
                {
                    const auto* clip = ui.engine.project.clipFor(clipId); const auto& points = clip->controls.automation[1].points;
                    check(std::any_of(points.begin(), points.end(), [&](const auto& point) { return near(juce::Decibels::decibelsToGain(point.value), .4); }), "P50 legacy shared depth changes both tracks");
                }
                const auto* incoming = ui.engine.project.clipFor(id); const auto& curve = incoming->controls.automation[1];
                const double beforeCut = (*pair.bassSwap)[2] * .25;
                check(near(juce::Decibels::decibelsToGain(curve.interpolatedValue(beforeCut, 0)), .4), "P50 legacy incoming amount retains a flat low plateau before the cut");
                const auto updated = ui.timeline.cutAmountHandles();
                if (updated.size() == 2) ui.timeline.setCutAmount(updated.back(), .7f, false);
                check(near(juce::Decibels::decibelsToGain(ui.engine.project.clipFor(id)->controls.automation[1].interpolatedValue(beforeCut, 0)), .7), "P50 repeated legacy amount events retain the flat hold rather than introducing a ramp");
            }
        }
        reset(); ui.selectedTransition.clear(); ui.viewport.setViewPosition(0, 0);
        ui.selectedClip = base.clips[0].id; ui.selectTrack(0);
        ui.currentClip()->effects = makeClipEffects(); // The test plugin must not mutate the shared base fixture.
        const auto plugin = juce::File::getCurrentWorkingDirectory().getChildFile("build/MixworksTestGain_artefacts/Release/VST3/Mixworks Test Gain.vst3");
        const auto pluginError = ui.engine.addPlugin(0, plugin); check(pluginError.isEmpty(), "P50 FX actual plugin loads: " + pluginError);
        if (pluginError.isEmpty())
        {
            const auto& slot = ui.currentClip()->effects->plugins.front(); const auto parameter = slot->parameters.begin();
            const AutomationAddress address {0, AutomationTarget::plugin, slot->id, parameter->first, ui.selectedClip};
            auto* curve = ui.materialiseAutomation(ui.engine.project, address); curve->enabled = true; curve->setPoint(8, .2f); curve->setPoint(12, .8f); curve->points[0].bend = 1;
            ui.timeline.selectAutomationCurve(address); ui.undoHistory.clear(); ui.redoHistory.clear(); ui.refresh();
            const auto handles = ui.timeline.cutAmountHandles(); check(handles.size() == 2, "P50 FX step exposes normalised parameter handles");
            if (!handles.empty())
            {
                const auto h = handles.back(); check(ui.timeline.setCutAmount(h, .55f, true) && ui.engine.project.curve(address)->points[1].time == 12
                    && near(ui.engine.project.curve(address)->points[1].value, .55), "P50 FX amount retains parameter identity and cut time");
                const auto file = report.getSiblingFile("p50-fx.mixworks"); const auto result = ui.engine.saveProject(file); const auto parsed = parseProject(file.loadFileAsString());
                const auto found = std::find_if(parsed.clips.begin(), parsed.clips.end(), [&](const auto& item) { return item.clip.id == address.clipId; });
                check(result.isEmpty() && parsed.error.isEmpty() && found != parsed.clips.end() && !found->clip.controls.pluginAutomation.empty()
                    && samePoints(found->clip.controls.pluginAutomation.front().curve.points, ui.engine.project.curve(address)->points), "P50 native Save retains FX stepped amount");
            }
        }
        // Retained legacy paired-depth editor coverage; new preset nodes have
        // independent endpoint amounts, exercised in PresetNodeTests.
        reset(); ui.engine.project.setTransitionBand(id, 1, {TransitionBandMode::swap, 1, 0, .5}); ui.refresh();
        auto h = ui.timeline.cutAmountHandles().front();
        auto pump = []
        {
            const auto end = juce::Time::getMillisecondCounterHiRes() + 40;
            while (juce::Time::getMillisecondCounterHiRes() < end)
            {
#if JUCE_WINDOWS
                MSG message; while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
#endif
                juce::Thread::sleep(1);
            }
        };
        ui.timeline.editCutAmount(h); pump();
        auto* dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        check(dialog != nullptr, "P50 Enter/exact amount opens native editor");
        const auto beforeDialog = stableJson(ui.engine.project);
        if (dialog) { dialog->getTextEditor("amount")->setText("37.5"); dialog->exitModalState(0); pump(); }
        check(stableJson(ui.engine.project) == beforeDialog && ui.undoHistory.empty(), "P50 Cancel preserves treatment/history");
        ui.timeline.editCutAmount(h); pump(); dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        if (dialog) { dialog->getTextEditor("amount")->setText("37.5"); dialog->exitModalState(1); pump(); }
        check(near(ui.engine.project.transitionFor(id)->bands[1]->strength, .375) && ui.undoHistory.size() == 1, "P50 exact entry uses cut depth, 100 means full attenuation");
        h = ui.timeline.cutAmountHandles().front(); ui.timeline.editCutAmount(h); pump(); dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        ui.engine.project.masterDb = -7; const auto stale = stableJson(ui.engine.project);
        if (dialog) { dialog->getTextEditor("amount")->setText("99"); dialog->exitModalState(1); pump(); }
        check(stableJson(ui.engine.project) == stale && ui.undoHistory.size() == 1, "P50 stale exact-entry callback preserves newer document");
        reset();
        ui.editingSelect.onClick(); pump();
        auto* selectionMenu = juce::Component::getCurrentlyModalComponent();
        check(selectionMenu != nullptr, "P12 explicit Select opens a native segment chooser");
        const auto selectionDocument = stableJson(ui.engine.project); const auto selectionPosition = ui.engine.position.load();
        if (selectionMenu) { selectionMenu->keyPressed(juce::KeyPress(juce::KeyPress::downKey)); selectionMenu->keyPressed(juce::KeyPress(juce::KeyPress::returnKey)); pump(); }
        check(ui.selectedClip == base.clips[0].id && stableJson(ui.engine.project) == selectionDocument && ui.engine.position == selectionPosition && ui.undoHistory.empty(), "P12 native selection changes only the segment/lane owner");
        ui.selectedClip = id; ui.selectTrack(1);
        const auto editingBefore = stableJson(ui.engine.project); const auto count = ui.engine.project.clips.size();
        ui.editingCopy.onClick();
        check(ui.clipClipboard && stableJson(ui.engine.project) == editingBefore && ui.undoHistory.empty(), "P12 explicit Copy uses the shared clipboard without changing the segment");
        ui.engine.position = 56; ui.selectTrack(3); ui.editingPaste.onClick();
        const auto* pasted = ui.currentClip();
        check(pasted && ui.engine.project.clips.size() == count + 1 && pasted->track == 3 && pasted->start == 56
            && pasted->media == base.clips[1].media && pasted->effects != base.clips[1].effects && ui.undoHistory.size() == 1,
            "P12 explicit Paste uses playhead/lane with shared source and independent effect ownership");
        bool retained = true; for (const auto& clip : base.clips) retained &= ui.engine.project.clipFor(clip.id)->start == clip.start;
        check(retained, "P12 ordinary Paste leaves earlier and later segment positions fixed");
        ui.undo(false); check(stableJson(ui.engine.project) == editingBefore, "P12 Paste Undo restores the exact arrangement");
        ui.selectedClip = id; ui.selectTrack(1); ui.editingCut.onClick();
        retained = true; for (const auto& clip : base.clips) if (clip.id != id) retained &= ui.engine.project.clipFor(clip.id)->start == clip.start;
        check(!ui.engine.project.clipFor(id) && ui.engine.project.clips.size() == count - 1 && ui.clipClipboard && retained, "P12 ordinary Cut removes only its segment and leaves a gap");
        ui.undo(false); check(stableJson(ui.engine.project) == editingBefore, "P12 Cut Undo restores source clocks, linked treatments and geometry");
        ui.selectedClip = id; ui.selectTrack(1); ui.engine.project.transitionLinks[0].locked = true; const auto lockedEditing = stableJson(ui.engine.project); ui.editingCut.onClick();
        check(stableJson(ui.engine.project) == lockedEditing && !ui.editingCut.isEnabled(), "P12 locked segment rejects Cut through the shared command owner");
        ui.engine.project.transitionLinks[0].locked = false; ui.refresh();
        ui.loopButton.onClick(); pump(); dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        check(dialog != nullptr, "P12 explicit Loop opens the existing range/repetition editor");
        ui.engine.project.masterDb = -6; const auto changedWhileLoop = stableJson(ui.engine.project);
        if (dialog) { dialog->exitModalState(1); pump(); }
        check(stableJson(ui.engine.project) == changedWhileLoop, "P12 stale Loop entry cannot overwrite newer document state");
        const auto savedFile = report.getSiblingFile("a05-current.mixworks");
        check(ui.saveSessionFile(savedFile), "A05 establish an owned current document using the transactional writer");
        const auto originalSaved = savedFile.loadFileAsString(); ui.engine.project.masterDb = -8; ui.dirty = true;
        const auto saveHistory = ui.undoHistory.size(); ui.saveButton.onClick();
        check(savedFile.loadFileAsString() != originalSaved && !ui.dirty && !ui.choosingSave && ui.sessionFile == savedFile && ui.undoHistory.size() == saveHistory,
            "A05 Save button updates its current file without opening a chooser or adding Undo");
        ui.engine.project.masterDb = -9; ui.dirty = true; const auto keptBytes = savedFile.loadFileAsString();
        ui.keyPressed(juce::KeyPress('S', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0)); pump();
        auto* savePicker = juce::Component::getCurrentlyModalComponent();
        check(savePicker != nullptr && ui.choosingSave, "A05 Ctrl+Shift+S opens explicit Save As");
        if (savePicker) { savePicker->exitModalState(0); pump(); }
        check(ui.sessionFile == savedFile && ui.dirty && savedFile.loadFileAsString() == keptBytes && !ui.choosingSave, "A05 cancelling Save As keeps current path, dirty state and file bytes");
        ui.menuItemSelected(89, 0); pump(); savePicker = juce::Component::getCurrentlyModalComponent();
        std::function<juce::FileBrowserComponent*(juce::Component*)> browser = [&](juce::Component* component) -> juce::FileBrowserComponent*
        {
            if (auto* found = dynamic_cast<juce::FileBrowserComponent*>(component)) return found;
            if (component) for (auto* child : component->getChildren()) if (auto* found = browser(child)) return found;
            return nullptr;
        };
        const auto newFile = report.getSiblingFile("a05-save-as.mixworks"); auto* files = browser(savePicker);
        if (files) { files->setRoot(newFile.getParentDirectory()); files->setFileName(newFile.getFileName()); pump(); }
        if (savePicker) { savePicker->exitModalState(files ? 1 : 0); pump(); }
        check(files && newFile.existsAsFile() && ui.sessionFile == newFile && !ui.dirty && savedFile.loadFileAsString() == keptBytes,
            "A05 real Save As picker creates a new current document and leaves the original file unchanged");
        ui.engine.project.masterDb = -10; ui.dirty = true; ui.keyPressed(juce::KeyPress('S', juce::ModifierKeys::ctrlModifier, 0));
        check(!ui.dirty && parseProject(newFile.loadFileAsString()).project.masterDb == -10 && ui.saveButton.getTooltip().contains(newFile.getFullPathName()), "A05 Ctrl+S and full-path tooltip follow the newly saved document");
        reset(); ui.setSize(1000, 620); ui.timeline.pixelsPerSecond = 24;
        for (int band = 0; band < 4; ++band) ui.engine.project.setTransitionBand(id, band, {TransitionBandMode::swap, .75, 0, .5});
        ui.refresh(); ui.viewport.setViewPosition(820, 0);
        for (int band = 0; band < 4; ++band)
        {
            const auto handles = ui.timeline.cutAmountHandles();
            const auto found = std::find_if(handles.begin(), handles.end(), [&](const auto& h) { return (int) h.address.target == band && h.before; });
            check(found != handles.end(), "P50 compact all-band treatment retains parameter " + juce::String(band)); if (found == handles.end()) continue;
            // Keyboard selection reveals its parameter; hidden chips cannot intercept it.
            for (int i = 0; i < (int) handles.size() && (!ui.timeline.selectedCutAmount || !ui.timeline.selectedCutAmount->sameOwner(*found)); ++i) ui.timeline.keyPressed(juce::KeyPress('D'));
            const auto down = found->bounds.getCentre(); const auto hit = ui.timeline.cutAmountAt(down);
            check(hit && hit->sameOwner(*found), "P50 compact selected amount has an unambiguous mouse target " + juce::String(band));
            ui.timeline.mouseDown(event(down, down, false)); ui.timeline.mouseDrag(event(down.translated(0, -12), down, true)); ui.timeline.mouseUp(event(down, down, true));
            check(near(ui.engine.project.transitionFor(id)->bands[(size_t) band]->strength, .65), "P50 compact keyboard-to-mouse journey edits the intended cut " + juce::String(band));
        }
        reset(); ui.engine.project.applyTransitionPreset(id, TransitionPreset::bassSwap);
        ui.engine.project.setTransitionBand(id, 1, {TransitionBandMode::swap, .375, 0, .5}); ui.selectTransition(id, false); ui.showToolPage(2);
        // Inspect actual candidate-native pixels at both layouts.
        for (const int width : {1000, 1920})
        {
            ui.setSize(width, width == 1000 ? 620 : 1080); ui.timeline.pixelsPerSecond = 24; ui.refresh(); ui.viewport.setViewPosition(820, 0);
            const auto visibleAmounts = ui.timeline.cutAmountHandles();
            check(visibleAmounts.size() == 2 && std::all_of(visibleAmounts.begin(), visibleAmounts.end(), [&](const auto& amount) { return ui.viewport.getViewArea().toFloat().contains(amount.bounds); }), "P50 compact/wide amount handles remain inside the actual viewport");
            if (!visibleAmounts.empty())
            {
                const auto at = visibleAmounts.front().bounds.getCentre();
                ui.timeline.mouseDown(event(at, at, false)); ui.timeline.mouseUp(event(at, at, false));
            }
            ui.editingViewport.setViewPosition(0, width == 1000 ? 200 : 0);
            check(ui.editingSelect.isVisible() && ui.editingCut.isVisible() && ui.editingCopy.isVisible() && ui.editingPaste.isVisible(), "P12 explicit editing actions are available in the scrolling console");
            juce::PNGImageFormat png; auto stream = report.getSiblingFile("p50-cut-amount-" + juce::String(width) + ".png").createOutputStream();
            check(stream && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *stream), "P50 capture native amount controls " + juce::String(width));
        }
    };
    cutAmountTests();
    {
        auto ownedPreview = std::make_unique<Workstation>(false); auto& preview = *ownedPreview; preview.stopTimer(); preview.setSize(1440, 940);
        preview.engine.project = base; preview.engine.project.applyTransitionPreset(id, TransitionPreset::alignOnly); preview.timeline.pixelsPerSecond = 40;
        for (const auto address : {AutomationAddress {0, AutomationTarget::volume, {}, {}, base.clips[0].id}, AutomationAddress {-1, AutomationTarget::master}})
        {
            auto* curve = preview.engine.project.curve(address); curve->enabled = true; curve->setPoint(0, -60); curve->setPoint(16, 0);
            const juce::Point<float> midpoint {320, preview.timeline.automationY(0, -30, (int) address.target)};
            juce::Point<float> nearest; preview.timeline.automationPath(address, 0, 16).getNearestPoint(midpoint, nearest);
            check(midpoint.getDistanceFrom(nearest) < 1, "percentage preview follows the actual dB ramp midpoint for " + juce::String(automationName(address.target)));
        }
    }
    {
        auto ownedSnap = std::make_unique<Workstation>(false); auto& snap = *ownedSnap; snap.stopTimer(); snap.setSize(1440, 940); snap.timeline.pixelsPerSecond = 40;
        const auto mouse = juce::Desktop::getInstance().getMainMouseSource();
        auto event = [&](juce::Point<float> p, juce::Point<float> down, bool drag)
        { return juce::MouseEvent(mouse, p, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1, 0, 0, 0, 0, &snap.timeline, &snap.timeline, juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), 1, drag); };
        for (int band = 0; band < 4; ++band)
        {
            snap.engine.project = base; snap.engine.project.snap = false; snap.selectedTransition.clear(); snap.transitionCurveOnlyBand = -1;
            const AutomationAddress address {0, (AutomationTarget) band, {}, {}, base.clips[0].id};
            auto* curve = snap.engine.project.curve(address); curve->enabled = true; curve->setPoint(12, -6);
            snap.timeline.selectAutomationCurve(address);
            const juce::Point<float> down {480, snap.timeline.automationY(0, -6, band)};
            const juce::Point<float> above {480, snap.timeline.automationY(0, snap.timeline.automationNodeMaximum(address.target), band) - 40};
            snap.timeline.mouseDown(event(down, down, false)); snap.timeline.mouseDrag(event(above, down, true)); snap.timeline.mouseDrag(event(above, down, true)); snap.timeline.mouseUp(event(above, down, true));
            check(snap.engine.project.curve(address)->points[0].value == 0, "gain node " + juce::String(band) + " stops at 100 percent for the entire first drag");
            const juce::Point<float> unity {480, snap.timeline.automationY(0, 0, band)};
            snap.timeline.mouseDown(event(unity, unity, false)); snap.timeline.mouseDrag(event(above, unity, true)); snap.timeline.mouseUp(event(above, unity, true));
            check(near(juce::Decibels::decibelsToGain(snap.engine.project.curve(address)->points[0].value), 1.3), "gain node " + juce::String(band) + " second drag stops at 130 percent");
            const auto bottom = snap.timeline.clipBounds(snap.engine.project.clips[0]).getBottom() - 5;
            check(near(snap.timeline.automationY(0, -60, band), bottom) && near(snap.timeline.automationDb(0, bottom, address.target), -60), "gain node zero percent is at the waveform bottom");
        }
    }
    {
        Workstation fade(false); fade.stopTimer(); fade.setSize(1440, 940);
        fade.engine.project = base;
        check(fade.engine.project.applyTransitionPreset(id, TransitionPreset::crossfade).wasOk(), "P06 reproduce default crossfade producer");
        fade.selectTransition(id); fade.timeline.pixelsPerSecond = 40; fade.refresh();
        const AutomationAddress address {1, AutomationTarget::volume, {}, {}, id};
        const double time = 40;
        const float db = fade.engine.project.curve(address)->valueAt(fade.engine.project.automationTime(address,time),0);
        const juce::Point<float> point {(float) (time * 40), fade.timeline.automationY(1, db, 0)};
        const auto hit = fade.timeline.automationCurveAt(point);
        check(hit && *hit == address, "P06 default crossfade line resolves its incoming volume owner");
        fade.timeline.selectAutomationCurve(address);
        const juce::Point<float> end {48 * 40.f, fade.timeline.automationY(1, 0, 0)};
        check(fade.timeline.automationPointAt(end).has_value(), "P06 selecting default crossfade reveals endpoint adjustment nodes");
        check(fade.timeline.automationY(0, 12, 0) > fade.timeline.transitionBounds(fade.engine.project.transitionLinks[0]).getY() + 24,
            "P06 top volume node clears title and transition header");
        const AutomationAddress lane {1, AutomationTarget::volume};
        const auto sourceNodes = fade.engine.project.curve(address)->points.size();
        fade.performTimelineAction(TimelineAction::volume, time + .5, 1);
        check(!fade.engine.project.curve(lane) && fade.engine.project.curve(address)->points.size() == sourceNodes + 1,
            "P06 marker insertion uses the same track curve as the preset");
        check(fade.timeline.automationNodesVisible(address), "P06 inserted marker remains selected and visible");
        for (int width : {1000, 1920})
        {
            fade.engine.project = base; fade.engine.project.applyTransitionPreset(id, TransitionPreset::crossfade);
            fade.setSize(width, width == 1000 ? 620 : 1080); fade.timeline.pixelsPerSecond = width == 1000 ? 12 : 30;
            fade.selectTransition(id); fade.timeline.selectAutomationCurve(address); fade.timeline.selectAutomationPoint(address, 1);
            fade.viewport.setViewPosition((int) (32 * fade.timeline.pixelsPerSecond) - 24, 0); fade.refresh();
            auto stream = report.getSiblingFile("default-fade-nodes-" + juce::String(width) + ".png").createOutputStream(); juce::PNGImageFormat png;
            check(stream && png.writeImageToStream(fade.createComponentSnapshot(fade.getLocalBounds()), *stream), "capture default crossfade nodes below header at " + juce::String(width));
        }
    }
    for (int band = 1; band < 4; ++band)
    {
        auto sharp = base; sharp.setTransitionBand(id, band, {TransitionBandMode::swap, 1, 0, .50013});
        const auto outgoing = sharp.transitionPlaybackFor(sharp.clips[0]).pairs[1][(size_t) band];
        const auto incoming = sharp.transitionPlaybackFor(sharp.clips[1]).pairs[0][(size_t) band];
        check(outgoing.instant && outgoing.begin == outgoing.end && incoming.begin == incoming.end
            && outgoing.gainAt(std::nextafter(outgoing.begin, -INFINITY)) == 1 && outgoing.gainAt(outgoing.begin) == 0
            && incoming.gainAt(std::nextafter(incoming.begin, -INFINITY)) == 0 && incoming.gainAt(incoming.begin) == 1,
            "EQ swap " + juce::String(band) + " is a true vertical step without a ramp");
        Engine actual, reference; actual.sampleRate = reference.sampleRate = 8000;
        actual.project = reference.project = sharp;
        reference.project.transitionLinks[0].bands[(size_t) band].reset();
        reference.project.clips[1].controls.eqDb[(size_t) band - 1] = -60;
        const auto edge = (int64_t) std::ceil(outgoing.begin * 8000 - 1e-9);
        const double start = (edge - 37) / 8000.;
        juce::AudioBuffer<float> whole(2, 128), first(2, 37), last(2, 91); juce::MidiBuffer midi;
        actual.render(whole, 128, start, midi); reference.render(first, 37, start, midi);
        reference.project.clips[0].controls.eqDb[(size_t) band - 1] = -60;
        reference.project.clips[1].controls.eqDb[(size_t) band - 1] = 0;
        reference.render(last, 91, edge / 8000., midi);
        bool exact = true;
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 128; ++i)
            exact &= std::abs(whole.getSample(ch, i) - (i < 37 ? first.getSample(ch, i) : last.getSample(ch, i - 37))) < 1e-5;
        check(exact, "production EQ swap " + juce::String(band) + " changes at its exact sample inside a 32-sample processing block");
    }
    {
        Workstation marker(false); marker.stopTimer(); marker.setSize(1920, 1080);
        marker.engine.project = base; marker.engine.project.snap = false;
        marker.engine.project.setTransitionBand(id, 1, {TransitionBandMode::swap, 1, 0, .5});
        marker.selectedTransition = id;
        marker.selectedClip = base.clips[0].id; marker.selectAutomationTarget(1); marker.timeline.pixelsPerSecond = 12; marker.refresh();
        const AutomationAddress address {0, AutomationTarget::bass, {}, {}, base.clips[0].id};
        marker.performTimelineAction(TimelineAction::bass, 8, 0);
        const auto oneNode = stableJson(marker.engine.project);
        marker.performTimelineAction(TimelineAction::bass, 12, 0);
        const auto* curve = marker.engine.project.curve(address);
        check(curve && marker.automationAddresses(0).size() == 1 && !marker.engine.project.transitionFor(id)->bands[1] && marker.selectedAutomationTarget == 1,
            "adding bass markers materialises the existing swap on the same ordinary source curve");
        check(!marker.timeline.hasGeneratedAutomation(address) && curve->valueAt(35,0) == 0 && curve->valueAt(41,0) == -60,
            "the editable bass line retains the complete cut without a second generator");
        const auto point = marker.timeline.editingAutomationPoint;
        const juce::Point<float> hit {144, marker.timeline.automationY(0, 0, 1)};
        check(point && point->second == 2 && marker.timeline.automationPointAt(hit)
            && marker.timeline.automationPointLabel(address, 2) == "100%", "new markers select the same displayed bass curve with a matching hit target and caption");
        const auto withNodes = stableJson(marker.engine.project); marker.undo(false);
        check(stableJson(marker.engine.project) == oneNode, "Undo removes only the second bass node and retains the materialised swap");
        marker.undo(true); check(stableJson(marker.engine.project) == withNodes, "Redo restores the node and existing swap exactly");
        const auto reopened = parseProject(withNodes);
        check(reopened.error.isEmpty() && !reopened.project.transitionFor(id)->bands[1]
            && samePoints(reopened.clips[0].clip.controls.automation[1].points,marker.engine.project.curve(address)->points), "ordinary bass cut and added nodes retain exact saved values and ownership");
        const auto image = marker.createComponentSnapshot(marker.getLocalBounds());
        auto out = report.getSiblingFile("bass-swap-with-added-nodes.png").createOutputStream(); juce::PNGImageFormat png;
        check(out && png.writeImageToStream(image, *out), "capture the vertical bass swap and its added nodes on one line");
        marker.engine.project.setTransitionBand(id, 1, {TransitionBandMode::swap, .5, 0, .5});
        const float manual = marker.timeline.automationManualValue(address, 41, -12);
        check(std::abs(marker.timeline.automationDisplayValue(address, 41, manual) + 12) < 1e-5,
            "drag and precise entry invert the generated contribution so a node edits the displayed effective bass value");
    }
    check(base.validateTransitionLinks().wasOk(), "independent-band fixture has valid source-owned frames");
    Project project = base;
    project.clips[0].controls.automation[2].enabled = true; project.clips[0].controls.automation[2].setPoint(4, -3);
    const auto manual = project.clips[0].controls.automation[2].points;
    const auto nextLink = project.transitionLinks[1];
    for (int band = 0; band < 4; ++band)
    {
        const auto result = project.setTransitionBand(id, band, {band == 1 ? TransitionBandMode::swapFade : TransitionBandMode::crossfade, .75, band == 1 ? 1.0 : 0.0, .5});
        check(result.wasOk(), "apply independent band " + juce::String(band) + ": " + result.getErrorMessage());
    }
    check(project.transitionLinks[1] == nextLink && project.clips[0].controls.automation[2].points.size() == manual.size()
        && project.clips[0].controls.automation[2].points[0].value == manual[0].value, "composed bands preserve the next pair and manual EQ");
    bool stable = true; for (size_t i = 0; i < 3; ++i) stable &= project.clips[i].start == base.clips[i].start && project.clips[i].length == base.clips[i].length
        && project.clips[i].prepared == base.clips[i].prepared && project.clips[i].tempoMap == base.clips[i].tempoMap;
    check(stable && near(project.transitionRange(project.transitionLinks[0]).getEnd(), 48), "band commands retain placements, clocks, PCM and frame geometry");
    for (const auto mode : {TransitionBandMode::crossfade, TransitionBandMode::fadeIn, TransitionBandMode::fadeOut, TransitionBandMode::swap})
    {
        auto trial = base; check(trial.setTransitionBand(id, 0, {mode, 1, mode == TransitionBandMode::swap ? 2.0 : 0.0, .5}).wasOk(), "volume mode is accepted: " + juce::String((int) mode));
        const auto out = trial.transitionPlaybackFor(trial.clips[0]), in = trial.transitionPlaybackFor(trial.clips[1]);
        const bool fadesOut = mode != TransitionBandMode::fadeIn, fadesIn = mode != TransitionBandMode::fadeOut;
        check(near(out.gainAt(0, 48), fadesOut ? 0 : 1) && near(in.gainAt(0, 32), fadesIn ? 0 : 1), "mode treats only its specified side: " + juce::String((int) mode));
        if (mode == TransitionBandMode::swap) check(out.gainAt(0, 40) == 0 && in.gainAt(0, 40) == 0, "positive swap gap cuts both sides at the crossing");
    }
    for (double strength : {.25, .5, .75, 1.})
    {
        auto trial = base; trial.setTransitionBand(id, 2, {TransitionBandMode::fadeOut, strength, 0, .5});
        const auto state = trial.transitionPlaybackFor(trial.clips[0]);
        check(near(state.gainAt(2, 48), 1 - strength) && state.gainAt(1, 48) == 1, "strength changes only the selected band's attenuation depth");
    }
    const auto untouched = stableJson(project);
    check(project.setTransitionBand(id, 1, {TransitionBandMode::swap, 1, 32, .25}).failed() && stableJson(project) == untouched, "an impossible gap is rejected atomically");
    check(project.setTransitionBand(id, 3, {TransitionBandMode::swapFade, 1, 0, .5}).failed()
        && project.setTransitionBand(id, 0, {TransitionBandMode::crossfade, std::numeric_limits<double>::quiet_NaN(), 0, .5}).failed(), "invalid modes and nonfinite strengths are rejected");
    check(project.setTransitionLength(id, 4).wasOk() && project.validateTransitionLinks().wasOk(), "resizing reprojects every generated band through the existing geometry owner");
    auto ramp = project; ramp.followTrackTempo = true; ramp.tempo.points = {{0, 100, true}, {60, 160, true}};
    check(ramp.setTransitionBand(id, 0, {TransitionBandMode::swap, 1, 2, .5}).wasOk(), "beat gap works with a changing tempo map");
    const auto outgoing = ramp.transitionPlaybackFor(ramp.clips[0]).pairs[1][0], incoming = ramp.transitionPlaybackFor(ramp.clips[1]).pairs[0][0];
    check(near(ramp.tempo.beatsAt((incoming.begin + incoming.end) / 2) - ramp.tempo.beatsAt((outgoing.begin + outgoing.end) / 2), 2), "swap separation follows integrated mix beats rather than a fixed seconds estimate");
    const auto parsed = parseProject(stableJson(project));
    check(parsed.error.isEmpty() && parsed.project.transitionLinks == project.transitionLinks, "format 16 round-trips all independent modes, strengths, gaps and positions");
    for (const auto& bad : {juce::var("bad"), juce::var(true), juce::var(-1), juce::var(2)})
    {
        auto value = projectToJson(project); auto& band = value["transitionLinks"][0]["bands"][0];
        band.getDynamicObject()->setProperty("strength", bad);
        check(parseProject(juce::JSON::toString(value)).error.isNotEmpty(), "session rejects malformed band strength");
    }
    auto legacy = projectToJson(base); legacy.getDynamicObject()->setProperty("version", 15); legacy["tracks"].getArray()->removeRange(4, trackCount - 4);
    check(parseProject(juce::JSON::toString(legacy)).error.isEmpty(), "format 15 opens without manufacturing band settings");
    auto gesture = project; check(gesture.editTransition(id, Project::TransitionEdit::resizeEnd, -1, false).wasOk(), "a composed frame can resize");
    project.setTransitionBand(id, 3, {TransitionBandMode::fadeIn, .25, 0, .5});
    check(project.applyPlacements(gesture).wasOk() && project.transitionLinks[0].bands[3]->strength == .25, "geometry commits retain live band settings instead of their mouse-down snapshot");
    check(project.setTransitionLocked(id, true).wasOk(), "composed transition locks");
    const auto locked = stableJson(project);
    check(project.setTransitionBand(id, 1, {}).failed() && stableJson(project) == locked, "locks enforce band protection in the model");
    auto tampered = project; tampered.transitionLinks[0].bands[3]->strength = .75;
    check(project.validateTransitionLocks(tampered).failed(), "indirect commits cannot replace locked settings");
    check(project.setTransitionBand(base.clips[2].id, 2, {TransitionBandMode::crossfade, .5, 0, .5}).failed(), "a neighbouring pair cannot edit bands on a track protected by a lock");
    tampered = project; tampered.transitionLinks[1].bands[2] = TransitionBandSettings {TransitionBandMode::crossfade, .5, 0, .5};
    check(project.validateTransitionLocks(tampered).failed(), "indirect commits also protect neighbour band settings on a locked track");
    project.setTransitionLocked(id, false);
    check(project.applyTransitionPreset(id, TransitionPreset::alignOnly).wasOk()
        && std::none_of(project.transitionLinks[0].bands.begin(), project.transitionLinks[0].bands.end(), [](const auto& b) { return b.has_value(); }), "whole-pair preset resets the composed treatment intentionally");
    auto oldBass = base;
    oldBass.transitionLinks[0].bassSwap = initialiseTransition(oldBass.clips[0], oldBass.clips[1], 16, TransitionPreset::bassSwap, oldBass.transitionRange(oldBass.transitionLinks[0]));
    check(oldBass.transitionLinks[0].bassSwap.has_value(), "construct an explicitly historical bass-pair fixture");
    check(oldBass.setTransitionBand(id, 0, {TransitionBandMode::crossfade, .75, 0, .5}).wasOk()
        && oldBass.transitionLinks[0].bassSwap.has_value(), "independent Volume leaves an existing paired bass handoff intact");
    auto contradictory = oldBass; contradictory.transitionLinks[0].bands[1] = TransitionBandSettings {};
    check(parseProject(stableJson(contradictory)).error.isNotEmpty(), "a session cannot claim both an old bass handoff and a new None setting");
    check(oldBass.setTransitionBand(id, 1, {TransitionBandMode::swap, 1, 2, .5}).wasOk() && !oldBass.transitionLinks[0].bassSwap
        && oldBass.clips[0].controls.automation[1].points.empty() && oldBass.clips[1].controls.automation[1].points.empty(), "explicit Bass replacement removes old owned points to avoid double processing");
    auto manualBand = base; manualBand.clips[0].controls.automation[2].enabled = true; manualBand.clips[0].controls.automation[2].setPoint(36, -7);
    manualBand.setTransitionBand(id, 2, {TransitionBandMode::fadeOut, .5, 0, .5});
    check(manualBand.setTransitionBand(id, 2, {TransitionBandMode::manual, .5, 0, .5}).wasOk()
        && manualBand.transitionPlaybackFor(manualBand.clips[0]).gainAt(2, 47) == 1 && manualBand.clips[0].controls.automation[2].points[0].value == -7,
        "Manual removes the generated treatment while retaining ordinary editable automation");
    // Exercise the real mixer rather than only the curve evaluator.
    const auto render = [&](Project p, double at, int solo = 0)
    {
        p.masterDb = 0; p.tracks[0].mute = solo != 0; p.tracks[1].mute = solo != 1; p.tracks[2].mute = true;
        Engine engine; engine.project = std::move(p); engine.sampleRate = 8000;
        juce::AudioBuffer<float> pcm(2, 512); juce::MidiBuffer midi; double energy = 0;
        for (int n = 0; n < 8; ++n) { engine.render(pcm, 512, at + n * 512. / 8000, midi);
            if (n >= 4) for (int i = 0; i < 512; ++i) energy += pcm.getSample(0, i) * pcm.getSample(0, i); }
        return energy;
    };
    const double dry = render(base, 43);
    auto powerBlend = base;
    check(powerBlend.setTransitionBand(id, 0, {TransitionBandMode::constantPower, 1, 0, .5}).wasOk(), "constant-power mode is available through the shared Volume owner");
    const auto energy = render(powerBlend, 43, 0) + render(powerBlend, 43, 1);
    check(dry > 1 && std::abs(energy / dry - 1) < .01,
        "production mixer retains summed isolated-source power through a constant-power blend without an inherited linear fade");
    const auto savedPower = parseProject(stableJson(powerBlend));
    check(savedPower.error.isEmpty() && savedPower.project.transitionLinks[0].bands == powerBlend.transitionLinks[0].bands,
        "session format 26 retains the constant-power mode without flattening it to a linear crossfade");
    auto oldFormat = legacyLowPassFixture(projectToJson(base), 25);
    check(parseProject(juce::JSON::toString(oldFormat)).error.isEmpty(), "existing format-25 sessions remain readable without rewriting analysis");
    check(powerBlend.setTransitionBand(id, 1, {TransitionBandMode::constantPower, 1, 0, .5}).failed(), "constant power rejects unsupported EQ-band use");
    for (int band = 0; band < 4; ++band)
    {
        auto wet = base; wet.setTransitionBand(id, band, {TransitionBandMode::swap, 1, 0, .5});
        check(render(wet, 43) < dry * .97, "production mixer audibly attenuates selected band " + juce::String(band));
    }
    auto gap = base; gap.setTransitionBand(id, 0, {TransitionBandMode::swap, 1, 4, .5});
    gap.clips[0].controls.gainDb = gap.clips[1].controls.gainDb = 6;
    check(render(gap, 39.6) < 1e-10 && dry > 1, "real mixer produces silence inside a full-strength volume gap");
    auto cut = base; cut.setTransitionBand(id, 0, {TransitionBandMode::swap, 1, 0, .5});
    check(render(cut, 36, 0) > 1 && render(cut, 36, 1) < 1e-10 && render(cut, 43, 0) < 1e-10 && render(cut, 43, 1) > 1,
        "production hard-cut playback switches source at the midpoint instead of running both through the full overlap");
    Workstation ui(false); ui.stopTimer(); ui.setSize(1440, 940); ui.engine.project = base; ui.selectTransition(id); ui.engine.position = 5;
    ui.transitionBandModes[0].setSelectedId(2, juce::sendNotificationSync);
    ui.transitionBandModes[1].setSelectedId(5, juce::sendNotificationSync);
    ui.transitionBandGaps[1].setSelectedId(3, juce::sendNotificationSync);
    ui.transitionBandStrengths[0].setSelectedId(75, juce::sendNotificationSync);
    check(ui.undoHistory.size() == 4 && ui.engine.position == 5 && !ui.busy && !ui.engine.project.transitionLinks[0].bands[1]
        && !ui.engine.project.transitionLinks[0].bands[0] && ui.engine.project.clips[0].controls.automation[0].active()
        && ui.engine.project.clips[0].controls.automation[1].active(), "native preset options place ordinary nodes with one Undo each without seeking or preparing audio");
    const auto viewState = stableJson(ui.engine.project); const auto viewUndo = ui.undoHistory.size(); const auto viewDirty = ui.dirty;
    juce::AudioBuffer<float> audibleBefore(2, 256), audibleAfter(2, 256), uninterrupted(2, 256); juce::MidiBuffer noMidi;
    Engine reference; reference.project = ui.engine.project; reference.sampleRate = ui.engine.sampleRate = 8000;
    reference.render(uninterrupted, 256, 39.6, noMidi); ui.engine.render(audibleBefore, 256, 39.6, noMidi);
    ui.timeline.pixelsPerSecond = 18; ui.refresh(); ui.viewport.setViewPosition(500, 0);
    const auto allCurvesImage = ui.createComponentSnapshot(ui.viewport.getBounds());
    ui.transitionCurveShow[0].setToggleState(false, juce::dontSendNotification); ui.transitionCurveShow[0].onClick();
    check(!ui.transitionCurveVisible(0) && ui.transitionCurveVisible(1), "Show curve hides Volume independently of Bass");
    ui.transitionCurveOnly[1].onClick();
    check(!ui.transitionCurveVisible(0) && ui.transitionCurveVisible(1) && !ui.transitionCurveVisible(2) && !ui.transitionCurveVisible(3),
          "Only curve isolates Bass without an audio-solo command");
    const auto isolatedImage = ui.createComponentSnapshot(ui.viewport.getBounds()); int changedPixels = 0;
    for (int y = 0; y < allCurvesImage.getHeight(); ++y) for (int x = 0; x < allCurvesImage.getWidth(); ++x)
        changedPixels += allCurvesImage.getPixelAt(x, y) != isolatedImage.getPixelAt(x, y);
    check(changedPixels > 20, "native timeline rendering actually hides the other generated curves");
    ui.transitionCurveOnly[2].onClick();
    check(ui.transitionCurveOnly[2].getToggleState() && !ui.transitionCurveOnly[1].getToggleState() && ui.transitionCurveVisible(2) && !ui.transitionCurveVisible(1),
          "isolating Mid replaces Bass isolation with one visible control state");
    ui.transitionCurveOnly[2].onClick();
    check(!ui.transitionCurveVisible(0) && ui.transitionCurveVisible(1) && ui.transitionCurveVisible(2) && ui.transitionCurveVisible(3),
          "ending isolation restores the other visibility choices");
    ui.transitionCurveOnly[0].onClick();
    check(ui.transitionCurveShown[0] && ui.transitionCurveVisible(0), "isolating a hidden curve makes that curve visible");
    ui.transitionCurveShow[0].setToggleState(false, juce::dontSendNotification); ui.transitionCurveShow[0].onClick();
    check(ui.transitionCurveOnlyBand == -1 && ui.transitionCurveVisible(1), "hiding the isolated curve returns to the remaining visible curves");
    ui.transitionCurveShow[0].setToggleState(true, juce::dontSendNotification); ui.transitionCurveShow[0].onClick();
    reference.render(uninterrupted, 256, 39.632, noMidi); ui.engine.render(audibleAfter, 256, 39.632, noMidi); bool identicalAudio = true;
    for (int ch = 0; ch < 2; ++ch) for (int f = 0; f < 256; ++f) identicalAudio &= uninterrupted.getSample(ch, f) == audibleAfter.getSample(ch, f);
    check(identicalAudio && stableJson(ui.engine.project) == viewState && ui.undoHistory.size() == viewUndo && ui.dirty == viewDirty && ui.engine.position == 5,
          "curve visibility and isolation retain exact renderer PCM, session, Undo, dirty state and playhead");
    const auto beforeUndo = stableJson(ui.engine.project); ui.undo(false); ui.undo(true);
    check(stableJson(ui.engine.project) == beforeUndo, "Undo and Redo retain the complete composed transition");
    ui.transitionLock.onClick(); check(!ui.transitionBandModes[0].isEnabled() && !ui.transitionBandGaps[1].isEnabled(), "native lock disables all band modes and settings");
    check(ui.transitionCurveShow[0].isEnabled() && ui.transitionCurveOnly[1].isEnabled(), "locked transitions still allow display-only curve controls");
    ui.transitionLock.onClick(); ui.selectTransition(base.clips[2].id);
    check(ui.transitionBandModes[0].getSelectedId() == 1, "Next pair starts with None and does not inherit another pair's preset options");
    ui.selectTransition(id); ui.timeline.pixelsPerSecond = 18; ui.viewport.setViewPosition(500, 0); ui.refresh();
    auto stream = report.getSiblingFile("independent-transition-bands.png").createOutputStream(); juce::PNGImageFormat png;
    check(stream && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *stream), "capture composed controls and shared-renderer curve preview");
    ui.setSize(1000, 620); ui.transitionViewport.setViewPosition(0, ui.transitionBandModes[3].getY());
    check(ui.transitionCurveShow[3].getWidth() > 65 && ui.transitionCurveOnly[3].getWidth() > 65 && ui.transitionCurveShow[3].getBottom() < ui.transitionBandPositions[3].getBottom(),
          "narrow inspector retains usable Show and Only buttons alongside all High settings");
    check(ui.transitionViewport.getViewedComponent()->getHeight() > ui.transitionViewport.getHeight()
        && ui.transitionViewport.getViewArea().intersects(ui.transitionBandModes[3].getBounds()), "small windows retain a scrollable path to High controls");
    {
        Workstation nodes(false); nodes.stopTimer(); nodes.setSize(1440, 940);
        nodes.engine.project = base; nodes.selectTransition(id); nodes.timeline.pixelsPerSecond = 40;
        const auto* link = nodes.engine.project.transitionFor(id);
        const auto* clip = nodes.engine.project.clipFor(id);
        const int nodeTrack = clip->track;
        const double time = (nodes.engine.project.transitionRange(*link).getStart() + nodes.engine.project.transitionRange(*link).getEnd()) * .5;
        const AutomationAddress bass {clip->track, AutomationTarget::bass, {}, {}, id};
        const AutomationAddress mid {clip->track, AutomationTarget::mid, {}, {}, id};
        nodes.engine.project.curve(bass)->setPoint(nodes.engine.project.automationTime(bass, time), -12);
        nodes.engine.project.curve(mid)->setPoint(nodes.engine.project.automationTime(mid, time), -12);
        nodes.transitionCurveOnly[2].onClick(); nodes.refresh();
        nodes.timeline.selectAutomationCurve(mid); nodes.timeline.selectAutomationPoint(mid, 0);
        const juce::Point<float> point {(float) (time * nodes.timeline.pixelsPerSecond), nodes.timeline.automationY(clip->track, -12, 2)};
        const auto isolated = nodes.timeline.automationPointAt(point);
        check(isolated && isolated->first == mid, "P06 isolation targets the selected manual curve when Bass and Mid nodes coincide");
        const auto larger = nodes.timeline.automationPointAt(point.translated(11, 0));
        check(larger && larger->first == mid, "P06 manual nodes admit an eleven-pixel mouse target");
        nodes.selectedAutomationTarget = -1;
        const auto tools = nodes.timeline.curveToolsBounds();
        check(tools.isEmpty() || tools.getY() >= nodes.timeline.clipBounds(*clip).getBottom(), "curve tools stay below the waveform or hide when the selected clip is outside the view");
        check(!nodes.timeline.automationPointAt(point), "unselected automation nodes stay hidden and cannot steal clicks");
        const auto curveClick = point.translated(-80, 0);
        const auto mouse = juce::Desktop::getInstance().getMainMouseSource();
        const juce::MouseEvent click(mouse, curveClick, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1, 0, 0, 0, 0,
            &nodes.timeline, &nodes.timeline, juce::Time::getCurrentTime(), curveClick, juce::Time::getCurrentTime(), 1, false);
        nodes.timeline.mouseDown(click); nodes.timeline.mouseUp(click);
        check(nodes.automationLane() == (int) AutomationTarget::mid && nodes.timeline.automationPointAt(point),
            "clicking the automation line selects its owner and reveals its editable nodes");
        nodes.selectedAutomationTarget = 2;
        nodes.transitionCurveOnly[2].onClick();
        const auto serialised = stableJson(nodes.engine.project); const auto history = nodes.undoHistory.size(); const bool dirty = nodes.dirty;
        for (const auto& address : {bass, mid})
        {
            nodes.timeline.selectAutomationCurve(address);
            check(nodes.timeline.automationNodesVisible(address) && !nodes.timeline.automationNodesVisible(address == bass ? mid : bass)
                && nodes.transitionCurveOnlyBand == -1, "line selection reveals only its own nodes while retaining other visible curves");
        }
        check(stableJson(nodes.engine.project) == serialised && nodes.undoHistory.size() == history && nodes.dirty == dirty,
            "line selection retains the exact musical session, Undo and dirty state");
        nodes.engine.project.curve(bass)->setPoint(nodes.engine.project.automationTime(bass, time + 2), -12);
        check(!nodes.timeline.automationPointAt(point.translated(80, 0)), "P06 isolated-out manual nodes cannot steal clicks");
        nodes.timeline.selectAutomationPoint(mid, 0);
        nodes.viewport.setViewPosition((int) point.x - 200, nodes.timeline.laneTop(clip->track)); nodes.refresh();
        auto caption = nodes.timeline.automationCaptionBounds();
        check(!caption.isEmpty() && !caption.intersects(juce::Rectangle<float> {point.x - 10, point.y - 10, 20, 20})
            && nodes.viewport.getViewArea().toFloat().contains(caption), "P06 caption fits the viewport without obscuring the selected node");
        nodes.engine.project.curve(mid)->setPoint(nodes.engine.project.automationTime(mid, time + .02), -12);
        nodes.timeline.keyPressed(juce::KeyPress(juce::KeyPress::rightKey));
        check(nodes.timeline.editingAutomationPoint && nodes.timeline.editingAutomationPoint->second == 1,
            "P06 Right selects a crowded adjacent node without a mouse target");
        nodes.timeline.keyPressed(juce::KeyPress(juce::KeyPress::leftKey));
        check(nodes.timeline.editingAutomationPoint && nodes.timeline.editingAutomationPoint->second == 0,
            "P06 Left returns to the previous node");
        const auto beforeDelete = stableJson(nodes.engine.project); const auto deleteHistory = nodes.undoHistory.size();
        check(nodes.timeline.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey)) && nodes.engine.project.curve(mid)->points.size() == 1
            && nodes.undoHistory.size() == deleteHistory + 1, "P06 keyboard Delete removes only the selected node with one Undo");
        nodes.undo(false); check(stableJson(nodes.engine.project) == beforeDelete, "P06 Undo restores the removed node and complete transition");
        nodes.undo(true); nodes.undo(false);
        nodes.engine.project.transitionLinks[0].locked = true; nodes.refresh();
        nodes.timeline.selectAutomationCurve(bass); nodes.timeline.selectAutomationPoint(bass, 0);
        const auto lockedJson = stableJson(nodes.engine.project); const auto lockedUndo = nodes.undoHistory.size();
        check(nodes.timeline.automationNodesVisible(bass), "a locked transition permits display-only line selection");
        nodes.timeline.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey));
        check(stableJson(nodes.engine.project) == lockedJson && nodes.undoHistory.size() == lockedUndo,
            "P06 locked node deletion rejects without an Undo entry");
        nodes.engine.project.transitionLinks[0].locked = false; nodes.engine.exporting = true;
        nodes.timeline.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey));
        check(nodes.engine.project.curve(bass)->points.size() == 2 && nodes.undoHistory.size() == lockedUndo,
            "P06 export blocks keyboard node mutation");
        nodes.engine.exporting = false;
        for (int width : {1000, 1920})
        {
            nodes.setSize(width, width == 1000 ? 620 : 1080); nodes.refresh();
            nodes.viewport.setViewPosition((int) point.x - 35, nodes.timeline.laneTop(nodeTrack));
            const juce::Point<float> linePoint {point.x, nodes.timeline.automationY(nodeTrack, -12, 2)};
            check(nodes.timeline.automationCurveAt(linePoint) && !nodes.timeline.curveToolsBounds().toFloat().contains(linePoint),
                "direct line selection stays clear of the lower tools in scrolled view " + juce::String(width));
            nodes.timeline.selectAutomationPoint(bass, 0);
            auto image = nodes.createComponentSnapshot(nodes.getLocalBounds());
            auto captureStream = report.getSiblingFile("p06-local-curves-" + juce::String(width) + ".png").createOutputStream();
            check(captureStream && png.writeImageToStream(image, *captureStream), "capture compact bar count and revealed nodes above lower tools at " + juce::String(width));
        }
        nodes.selectedAutomationTarget = -1; nodes.timeline.editingAutomationPoint.reset();
        check(!nodes.timeline.automationPointAt(point), "ending line selection hides the nodes again");
        nodes.selectedTransition.clear(); nodes.refresh();
        nodes.selectedClip.clear(); nodes.refresh();
        check(!nodes.timeline.curveTools->isVisible(), "clearing clip selection hides the lower tools after transition deselection");
    }
}
}
