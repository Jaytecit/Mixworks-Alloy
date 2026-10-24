#include "Workstation.h"
#include "PresetNodes.h"
#include "Beatmatch.h"
#include <cmath>

namespace mixworks
{
void runPresetNodeTests(const std::function<void(bool,const juce::String&)>& check,const juce::File& report)
{
    auto media = std::make_shared<Media>(); media->name = "Preset node source"; media->sampleRate = 8000; media->manualBpm = 120; media->manualBeatOrigin = 0;
    media->samples.setSize(1,96 * 8000);
    for (int i = 0; i < media->frames(); ++i) media->samples.setSample(0,i,(float) (.2 * std::sin(i * .13)));
    Project legacy; legacy.transitionLinksKnown = true;
    for (int i = 0; i < 3; ++i)
    { Clip c; c.media = media; c.track = i; c.start = i * 32.; c.length = 48;
      c.introAnchor = c.fadeInAnchor = 0; c.outroAnchor = 32; c.fadeOutAnchor = 48; legacy.clips.push_back(c); }
    for (int i = 1; i < 3; ++i)
    { Project::TransitionLink link; link.outgoing = legacy.clips[(size_t) i-1].id; link.incoming = legacy.clips[(size_t) i].id;
      link.bands[1] = TransitionBandSettings{TransitionBandMode::swap,1,0,.5}; legacy.transitionLinks.push_back(link); }
    const auto id = legacy.clips[1].id; const AutomationAddress address{1,AutomationTarget::bass,{},{},id};
    legacy.clips[1].controls.automation[1] = {true,{{0,0},{7.9999,0},{8,0}}};
    // Put the outgoing step one device sample beyond a normal32-sample EQ tick.
    legacy.transitionLinks[1].bands[1]->position += 1. / (16 * media->sampleRate);
    const double cut = legacy.transitionPlaybackFor(legacy.clips[1]).pairs[1][1].begin;
    const double cutSource = legacy.automationTime(address,cut);
    check(legacy.validateTransitionLinks().wasOk() && legacy.transitionRange(legacy.transitionLinks[1]) == juce::Range<double>(64,80),
        "node-preset fixture has real interior frame anchors and valid outgoing swap geometry");
    const auto json = [](const Project& p) { return juce::JSON::toString(projectToJson(p),true,17); };
    {
        auto clean = legacy;
        for (auto& c : clean.clips) c.controls.automation = {};
        for (auto& link : clean.transitionLinks) link.bands = {};
        auto none = clean;
        const auto applied = none.applyTransitionPreset(id,TransitionPreset::alignOnly);
        bool empty = applied.wasOk();
        for (const auto& c : none.clips) for (const auto& curve : c.controls.automation) empty &= curve.points.empty();
        check(empty,"AUDIT: None does not manufacture neutral nodes on untouched sources/parameters");
        auto swapped = clean;
        const auto bass = swapped.applyTransitionPreset(id,TransitionPreset::bassSwap);
        bool unrelated = bass.wasOk();
        for (const auto& c : swapped.clips) for (int t = 0; t < trackAutomationCount; ++t)
            if (t != 1) unrelated &= c.controls.automation[(size_t) t].points.empty();
        check(unrelated,"AUDIT: Bass swap creates nodes only for Bass");
        bool distinct = true;
        for (const auto& c : swapped.clips) for (const auto& curve : c.controls.automation)
            for (size_t n = 1; n < curve.points.size(); ++n)
                distinct &= curve.points[n].time - curve.points[n-1].time > 1.e-10 || curve.points[n].value != curve.points[n-1].value;
        check(distinct,"AUDIT: new preset has no coincident redundant boundary nodes");
        report.getSiblingFile("audit-bass-before.json").replaceWithText(json(swapped));
        Project::BassSwapPairing pairing;
        const auto paired = swapped.pairBassSwap(id,40,pairing);
        check(paired.wasOk(),"AUDIT: RMB pairing accepts the ordinary sharp Bass swap: " + paired.getErrorMessage());
        auto outgoingPair = clean; outgoingPair.applyTransitionPreset(id,TransitionPreset::bassSwap);
        const auto fromOutgoing = outgoingPair.pairBassSwap(clean.clips[0].id,40,pairing);
        check(fromOutgoing.wasOk(),"AUDIT: RMB pairing also works from the outgoing side: " + fromOutgoing.getErrorMessage());
        if (paired.wasOk())
        {
            const auto original = swapped;
            const auto moved = swapped.setTransitionSwapTime(id,1,false,39);
            const auto times = swapped.transitionSwapTimes(*swapped.transitionFor(id),1);
            const auto& curve = swapped.clips[0].controls.automation[1];
            check(moved.wasOk() && times && std::abs((*times)[0]-39) < 1.e-9 && std::abs((*times)[1]-39) < 1.e-9
                && std::any_of(curve.points.begin(),curve.points.end(),[](const auto& p) { return p.time == 32 && p.bend == 1; }),
                "AUDIT: paired sharp swap moves earlier while retaining its opening and sharp bend: " + moved.getErrorMessage());
            swapped = original;
            auto guarded = original; auto& guardedCurve = guarded.clips[0].controls.automation[1];
            guardedCurve.points.insert(guardedCurve.points.begin(),{std::nextafter(32.,0.),-3,1});
            const auto exact = guarded.setTransitionSwapTime(id,1,false,39);
            check(exact.wasOk() && guarded.clips[0].controls.automation[1].points.front().value == -3
                && std::any_of(guarded.clips[0].controls.automation[1].points.begin(),guarded.clips[0].controls.automation[1].points.end(),[](const auto& node) { return node.time == 32 && node.value == 0; }),
                "AUDIT: paired movement resolves exact owned nodes ahead of adjacent exterior guards: " + exact.getErrorMessage());
        }
        auto window = std::make_unique<Workstation>(false); auto& w = *window; w.stopTimer(); w.setSize(1920,1080);
        w.engine.project = clean; w.engine.project.applyTransitionPreset(id,TransitionPreset::bassSwap);
        w.selectedClip = id; w.selectedTransition = id; w.engine.selectedTrack = 1;
        w.selectedAutomationTarget = (int) AutomationTarget::volume; w.refresh();
        const AutomationAddress neutral{1,AutomationTarget::mid,{},{},id};
        check(w.timeline.automationViewCurve(neutral).points.empty(),"AUDIT: selecting an empty curve does not invent endpoint nodes");
        const auto beforeAdd = json(w.engine.project);
        const auto added = w.insertAutomationNode(neutral,38,false);
        const auto* single = w.engine.project.curve(neutral);
        check(added && single && single->points.size() == 1 && single->points.front().time == 6,
            "AUDIT: one explicit Add Node creates exactly one point at the requested source time");
        w.undo(false); check(json(w.engine.project) == beforeAdd,"AUDIT: single-node Add Undo restores the empty curve");
        w.selectedAutomationTarget = (int) AutomationTarget::volume; w.refresh();
        w.timeline.pixelsPerSecond = 40; w.timeline.updateSize(); w.viewport.setViewPosition(1200,0);
        const auto amounts = w.timeline.cutAmountHandles();
        check(std::any_of(amounts.begin(),amounts.end(),[&](const auto& h) { return h.address.clipId == clean.clips[0].id && h.address.target == AutomationTarget::bass; })
            && std::any_of(amounts.begin(),amounts.end(),[&](const auto& h) { return h.address.clipId == id && h.address.target == AutomationTarget::bass; }),
            "AUDIT: selected ordinary swap exposes vertical amount handles on both songs without selecting its curve");
        const auto lower = std::find_if(amounts.begin(),amounts.end(),[&](const auto& h) { return h.address.clipId == clean.clips[0].id && h.address.target == AutomationTarget::bass && h.value == 0; });
        if (lower != amounts.end())
        {
            const auto baseline = json(w.engine.project); const auto undo = w.undoHistory.size();
            const auto changed = w.timeline.setCutAmount(*lower,.5f,true);
            const auto* curve = w.engine.project.curve(lower->address);
            check(changed && curve && std::abs(curve->valueAt(44,0)-juce::Decibels::gainToDecibels(.5f)) < .0001f
                && w.undoHistory.size() == undo+1,"AUDIT: vertical cut amount changes its whole flat plateau without creating a ramp");
            w.undo(false); check(json(w.engine.project) == baseline,"AUDIT: cut amount Undo restores exact nodes");
        }
        for (int width : {1250,1920})
        { w.setSize(width,width == 1250 ? 900 : 1080); w.refresh(); w.timeline.pixelsPerSecond = 40; w.timeline.updateSize(); w.viewport.setViewPosition(1300,0);
          auto out = report.getSiblingFile("audit-swap-" + juce::String(width) + ".png").createOutputStream(); juce::PNGImageFormat png;
          check(out && png.writeImageToStream(w.createComponentSnapshot(w.getLocalBounds()),*out),"AUDIT: capture selected ordinary swap handles / " + juce::String(width)); }
    }
    {
        Project pending;
        for (int i = 0; i < 3; ++i)
        {
            auto source = std::make_shared<Media>(); source->sampleRate = 8000; source->name = "Preset phase " + juce::String(i);
            source->manualBpm = i == 0 ? 120 : i == 1 ? 128 : 124; source->manualBeatOrigin = i == 1 ? 1.213 : .213;
            source->samples.setSize(1,48*8000); source->samples.clear();
            Clip clip; clip.media = source; clip.length = 48; clip.track = i; clip.pendingPlacement = PendingPlacement::append; pending.clips.push_back(clip);
        }
        const auto placed = prepareTempoMap(pending,false,32);
        check(placed.error.isEmpty() && placed.project.clips.size() == 3,"fractional source phase and changing tempo place ordinary preset nodes without composition discontinuities: " + placed.error);
        if (placed.error.isEmpty())
        {
            const auto incoming = placed.project.clips[1].id;
            auto faded = placed.project; const auto applied = faded.applyTransitionPreset(incoming,TransitionPreset::crossfade);
            TransitionStyle before,after; const auto beforeCapture = faded.captureTransitionStyle(incoming,before);
            const auto rebuilt = prepareTempoMap(faded,true,32,0,0,{},TransitionPreset::crossfade);
            const auto captured = rebuilt.project.captureTransitionStyle(incoming,after);
            check(applied.wasOk() && rebuilt.error.isEmpty() && captured.wasOk() && before.nodes && after.nodes
                && before.nodes->curves[0][0].points.size() == after.nodes->curves[0][0].points.size(),
                "Beatmatch rebuild replaces a Crossfade shape instead of compounding its existing nodes: " + rebuilt.error
                + " / before: " + beforeCapture.getErrorMessage() + " / after: " + captured.getErrorMessage()
                + " / counts: " + juce::String(before.nodes ? (int) before.nodes->curves[0][0].points.size() : -1)
                + "/" + juce::String(after.nodes ? (int) after.nodes->curves[0][0].points.size() : -1));
            if (beforeCapture.failed() || captured.failed())
            {
                report.getSiblingFile("rebuild-before.json").replaceWithText(json(faded));
                report.getSiblingFile("rebuild-after.json").replaceWithText(json(rebuilt.project));
            }
            if (before.nodes && after.nodes)
            {
                bool same = true;
                for (int side = 0; side < 2; ++side) for (double fraction : {.1,.25,.5,.75,.9})
                    same &= std::abs(before.nodes->curves[(size_t) side][0].valueAt(fraction,0)-after.nodes->curves[(size_t) side][0].valueAt(fraction,0)) < .003;
                check(same,"repeated Crossfade rebuild retains its measured gain shape across the whole overlap");
            }
        }
    }
    // A late source start can round just below zero when mapped back through
    // the tempo clock. A fresh fade-out must still start at unity, not silence.
    for (double start : {4255.294237201032, 4849.564063734359})
    {
        Project p; p.followTrackTempo = true; p.transitionLinksKnown = true;
        p.tempo.points = {{0,120,false},{4000.213,127.992,true},{6000.113,125.997,true}};
        Clip c; c.media = media; c.start = start; c.length = 48; c.beatmatched = true; c.tempoSourceBpm = 120;
        c.tempoMap = std::make_shared<const TempoMap>(p.tempo); c.rate = c.sourceElapsed(c.length) / c.length;
        c.fadeOut = 12; c.fadeOutAnchor = c.sourceElapsed(36); p.clips.push_back(c);
        const AutomationAddress volume{0,AutomationTarget::volume,{},{},c.id}; AutomationCurve curve;
        const auto convertedFade = editablePresetNodes(p,volume,curve); double error = 0;
        for (int n = 0; n < 2048; ++n)
        {
            const double local = 48.*n/2048.;
            const double actual = juce::Decibels::decibelsToGain((double) curve.valueAt(c.sourceElapsed(local),0),-160.);
            error = std::max(error,std::abs(actual - c.envelope().gainAt(local)));
        }
        check(convertedFade.wasOk() && curve.points.size() < 32 && error < 2.e-6,
            "late fractional source start yields a sparse accurate fade without false silence / " + juce::String(start,9)
            + " / " + convertedFade.getErrorMessage() + " / gain error " + juce::String(error,9));
    }
    {
        auto p = legacy; p.clips.resize(2); p.transitionLinks.resize(1);
        for (auto& c : p.clips) c.controls.automation = {};
        p.transitionLinks[0].bands = {};
        p.clips[0].outroAnchor = 32.107322333828806;
        const auto applied = p.applyTransitionPreset(id,TransitionPreset::bassSwap);
        Project saved; saved.masterAutomation = p.clips[1].controls.automation[1];
        const auto parsed = parseProject(json(saved));
        const double edge = p.automationTime({1,AutomationTarget::bass,{},{},id},p.transitionRange(p.transitionLinks[0]).getStart());
        bool same = applied.wasOk() && parsed.error.isEmpty();
        for (double at : {0.,edge - 1.e-10,edge,edge + 1.e-10,8.,16.})
            same &= saved.masterAutomation.valueAt(at,0) == parsed.project.masterAutomation.valueAt(at,0);
        check(same,"fractional preset boundary guards stay distinct through default session JSON and retain the sharp cut: "
            + applied.getErrorMessage() + " / " + parsed.error);
    }
    const auto baseline = json(legacy); AutomationCurve preview;
    check(editablePresetNodes(legacy,address,preview).wasOk() && json(legacy) == baseline,"legacy preset node preview is read-only and keeps original playback ownership");
    check(std::any_of(preview.points.begin(),preview.points.end(),[&](const auto& p) { return p.time == cutSource && p.value == -60; }),
        "manual incoming curve still exposes the outgoing swap as an ordinary editable node");
    auto converted = legacy; check(materialiseEditablePresetNodes(converted,address).wasOk(),"first ordinary edit can materialise both related preset sides atomically");
    bool identical = true;
    for (double time = 32; time < 80; time += .001)
    {
        const auto& clip = legacy.clips[1]; const double x = legacy.automationTime(address,time);
        const auto expected = std::max(-60.f,clip.controls.automation[1].valueAt(x,0) + juce::Decibels::gainToDecibels(legacy.transitionPlaybackFor(clip).gainAt(1,time),-60.f));
        identical &= std::abs(converted.curve(address)->valueAt(x,0) - expected) < .0021f;
    }
    check(identical && !converted.transitionLinks[0].bands[1] && !converted.transitionLinks[1].bands[1],
        "ordinary nodes preserve both full cuts without leaving a second active Bass generator");
    for (int block : {1,31,173,512})
    {
        auto originalEngine = std::make_unique<Engine>(juce::File(),false), nodeEngine = std::make_unique<Engine>(juce::File(),false);
        originalEngine->sampleRate = nodeEngine->sampleRate = 8000; originalEngine->project = legacy; nodeEngine->project = converted;
        originalEngine->prepareClipProcessing(); nodeEngine->prepareClipProcessing(); juce::MidiBuffer midi; bool same = true;
        for (int sample = 0; sample < 640; sample += block)
        {
            const int count = std::min(block,640-sample); juce::AudioBuffer<float> a(2,count), b(2,count);
            const double at = cut - .04 + sample/8000.;
            originalEngine->render(a,count,at,midi); nodeEngine->render(b,count,at,midi);
            for (int ch = 0; ch < 2; ++ch) for (int n = 0; n < count; ++n) same &= a.getSample(ch,n) == b.getSample(ch,n);
        }
        check(same,"measured ordinary full-cut PCM matches original preset at an off-tick sample boundary / block " + juce::String(block));
    }
    {
        auto p = legacy;
        for (auto& clip : p.clips) clip.controls.automation = {};
        for (auto& link : p.transitionLinks) link.bands = {};
        p.transitionLinks[0].bands[0] = TransitionBandSettings{TransitionBandMode::swap,1,0,.5};
        const auto envelope = p.transitionPlaybackFor(p.clips[0]).pairs[1][0];
        p.tracks[1].mute = p.tracks[2].mute = true;
        auto ordinary = p; const auto compiled = materialisePresetNodes(ordinary,id,1);
        auto original = std::make_unique<Engine>(juce::File(),false), actual = std::make_unique<Engine>(juce::File(),false);
        original->sampleRate = actual->sampleRate = 8000; original->project = p; actual->project = ordinary;
        original->prepareClipProcessing(); actual->prepareClipProcessing(); juce::AudioBuffer<float> a(2,512),b(2,512); juce::MidiBuffer midi;
        original->render(a,512,envelope.end+.02,midi); actual->render(b,512,envelope.end+.02,midi);
        check(compiled.wasOk() && a.getMagnitude(0,512) == 0 && b.getMagnitude(0,512) == 0,
            "measured full Volume swap reaches true silence through ordinary zero-percent nodes, matching the original envelope");
    }
    {
        Workstation editor(false); auto clip = legacy.clips[0]; clip.start = 0;
        clip.introAnchor = clip.fadeInAnchor = 10; clip.outroAnchor = clip.fadeOutAnchor = 48;
        clip.controls.automation = {}; clip.controls.automation[0] = {true,{{5,-6},{20,0}}};
        editor.engine.project.clips = {clip}; editor.engine.library = {media}; editor.selectedClip = clip.id;
        editor.selectedAutomationTarget = 0; editor.setSize(1250,800); editor.refresh(); editor.timeline.pixelsPerSecond = 20; editor.timeline.updateSize();
        const AutomationAddress volume {0,AutomationTarget::volume,{},{},clip.id};
        const auto path = editor.timeline.automationPath(volume,0,48);
        const juce::Point<float> node {100,editor.timeline.automationY(0,-6,0)}; juce::Point<float> nearest;
        path.getNearestPoint(node,nearest);
        check(node.getDistanceFrom(nearest) < 1, "authored volume curve joins a node before the IN marker");
        check(std::abs(editor.timeline.automationManualValue(volume,5,-6)+6) < .0001f,
            "editing a pre-IN node keeps its chosen level instead of compensating for the silent gate");
    }
    for (auto preset : {TransitionPreset::bassSwap,TransitionPreset::crossfade})
    {
        Project p; p.transitionLinksKnown = true; p.clips = {legacy.clips[0],legacy.clips[1]}; p.clips[1].controls.automation[1] = {};
        Project::TransitionLink link; link.outgoing = p.clips[0].id; link.incoming = p.clips[1].id; p.transitionLinks = {link};
        const auto result = p.applyTransitionPreset(p.clips[1].id,preset);
        check(result.wasOk(),"apply preset as ordinary node starting shape / " + juce::String((int) preset) + " / " + result.getErrorMessage());
        const int target = preset == TransitionPreset::bassSwap ? 1 : 0;
        check(!p.transitionLinks[0].bands[(size_t) target] && p.clips[0].controls.automation[(size_t) target].active()
            && p.clips[1].controls.automation[(size_t) target].active() && p.clips[0].fadeOut == 0 && p.clips[1].fadeIn == 0,
            "applied preset plays through regular nodes with no separate band/fade function / " + juce::String((int) preset));
        if (preset == TransitionPreset::crossfade) for (int side = 0; side < 2; ++side)
        {
            const auto count = p.clips[(size_t) side].controls.automation[0].points.size();
            check(count == 2, "default full crossfade has only two endpoint nodes / side " + juce::String(side) + " / nodes " + juce::String((int) count));
        }
        if (preset == TransitionPreset::bassSwap) for (int side = 0; side < 2; ++side)
        {
            const auto& curve = p.clips[(size_t) side].controls.automation[1];
            check(std::any_of(curve.points.begin(),curve.points.end(),[](const auto& point) { return point.bend == 1; }),
                "newly applied full Swap retains its actual step on both sides, including a later exterior guard / " + juce::String(side));
        }
    }
    for (int band = 0; band < 4; ++band)
    {
        auto p = legacy; for (auto& c : p.clips) c.controls.automation = {};
        for (auto& link : p.transitionLinks) link.bands = {};
        p.transitionLinks[0].bands[(size_t) band] = TransitionBandSettings{TransitionBandMode::crossfade,.75,0,.5};
        auto compiled = p; const auto result = materialisePresetNodes(compiled,id,1u << band); double error = 0; size_t most = 0;
        for (int side = 0; side < 2; ++side)
        {
            const auto& clip = p.clips[(size_t) side]; const auto playback = p.transitionPlaybackFor(clip);
            const auto& curve = compiled.clips[(size_t) side].controls.automation[(size_t) band]; most = std::max(most,curve.points.size());
            for (int n = 0; n <= 4096; ++n)
            {
                const double t = clip.start + clip.length*n/4096.;
                const float expected = juce::Decibels::gainToDecibels((float) playback.gainAt(band,t),-60.f);
                error = std::max(error,(double) std::abs(curve.valueAt(clip.sourceElapsed(t-clip.start),0)-expected));
            }
        }
        check(result.wasOk() && most <= 24 && error <= .0021,"sparse Volume/EQ crossfade retains gain accuracy / band " + juce::String(band)
            + " / nodes " + juce::String((int) most) + " / error dB " + juce::String(error,7));
    }
    for (int band = 0; band < 4; ++band) for (int mode = 0; mode < 8; ++mode)
    {
        const TransitionBandSettings setting{(TransitionBandMode) mode,.75,0,.5}; if (setting.validate(band).failed()) continue;
        auto p = legacy; for (auto& c : p.clips) c.controls.automation = {};
        for (auto& link : p.transitionLinks) link.bands = {};
        p.clips[0].controls.automation[4] = {true,{{1,.2f},{4,-.2f}}};
        const auto neighbour = p.transitionLinks[1]; const auto unrelated = p.clips[0].controls.automation[4];
        const auto applied = placeTransitionBandNodes(p,id,band,setting);
        const auto first = json(p); const auto again = placeTransitionBandNodes(p,id,band,setting);
        bool plain = true; for (const auto& link : p.transitionLinks) for (const auto& b : link.bands) plain &= !b;
        check(applied.wasOk() && again.wasOk() && first == json(p) && plain && p.transitionLinks[1] == neighbour
            && p.clips[0].controls.automation[4].points.size() == unrelated.points.size(),
            "every band preset places an idempotent ordinary shape preserving other controls/neighbour / " + juce::String(band) + "/" + juce::String(mode) + " / " + applied.getErrorMessage());
    }
    // Silence uses a gain-space endpoint, including the held regions before
    // and after the authored span. Slicing/splicing must keep it valid on save.
    for (bool rising : {false, true})
    {
        const AutomationCurve curve {true, {{2, rising ? -160.f : 0.f, 0, false, true},
                                            {6, rising ? 0.f : -160.f, 0, false, true}}};
        for (const auto range : {juce::Range<double>(0,1), {0,8}, {8,10}, {3,5}})
        {
            Project session; session.masterAutomation = curve.sliced(range.getStart(),range.getEnd(),0);
            const auto parsed = parseProject(json(session)); bool same = parsed.error.isEmpty();
            for (int i = 0; i <= 16; ++i)
            {
                const double t = range.getLength()*i/16;
                same &= std::abs(session.masterAutomation.valueAt(t,0)-curve.valueAt(t+range.getStart(),0)) < .0001;
            }
            check(same,"gain-space slice retains held silence, curve shape and session validity / "
                + juce::String((int) rising) + " / " + juce::String(range.getStart()) + "-" + juce::String(range.getEnd()));
        }
        for (const auto range : {juce::Range<double>(.5,1), {8,9}, {3,5}, {0,8}})
        {
            Project session; session.masterAutomation = curve;
            const auto closed = session.closeTimeRange(range.getStart(),range.getEnd());
            bool same = closed.wasOk() && parseProject(json(session)).error.isEmpty();
            for (double t : {0., .25, 1., 2., 3., 5., 7., 10.})
                same &= std::abs(session.masterAutomation.valueAt(t,0)-curve.valueAt(t < range.getStart() ? t : t+range.getLength(),0)) < .0001;
            check(same,"closing time preserves held gain-space endpoints and saved values / "
                + juce::String((int) rising) + " / " + juce::String(range.getStart()) + "-" + juce::String(range.getEnd()));
        }
    }
    {
        Project invalid; invalid.masterAutomation = {true,{{0,-160}}};
        check(parseProject(json(invalid)).error.isNotEmpty(),"ordinary below-floor nodes still fail validation without gain-space ownership");
    }
    // Native curved spans retain their shape through ordinary editing and IO.
    for (bool exponential : {false,true})
    {
        AutomationCurve curve{true,{{0,-.8f,-.45,exponential},{16,.7f,0}}};
        auto split = curve; split.setPoint(5,curve.valueAt(5,0));
        const auto slice = curve.sliced(3,11,0); bool exact = true;
        for (int n = 0; n <= 1024; ++n)
        {
            const double t = 16. * n / 1024;
            exact &= std::abs(split.valueAt(t,0)-curve.valueAt(t,0)) < 3.e-7;
            if (t >= 3 && t <= 11) exact &= std::abs(slice.valueAt(t-3,0)-curve.valueAt(t,0)) < 3.e-7;
        }
        check(exact && split.points[0].exponential == exponential && split.points[1].exponential == exponential && slice.points[0].exponential == exponential,
            "ordinary insertion/slicing preserves native curved spans / exponential " + juce::String((int) exponential));
        Project session; session.masterAutomation = curve;
        const auto parsed = parseProject(json(session));
        check(parsed.error.isEmpty() && parsed.project.masterAutomation.points[0].exponential == exponential,
            "session36 retains interpolation kind for ordinary automation");
        session.masterAutomation = curve; const auto closed = session.closeTimeRange(4,8); bool folded = closed.wasOk();
        for (double t : {1.,3.,4.,5.,8.,12.}) folded &= std::abs(session.masterAutomation.valueAt(t,0)-curve.valueAt(t < 4 ? t : t+4,0)) < 3.e-7;
        check(folded,"closing time preserves both curved sides of the splice / exponential " + juce::String((int) exponential));
    }
    // Ordinary filter sweeps must remain practical to edit, not hundreds of
    // straight samples. Keep the existing conversion accuracy as a separate check.
    for (double sampleRate : {8000.,44100.,48000.})
    {
        auto source = std::make_shared<Media>(); source->name = "Sparse filter sweep"; source->sampleRate = sampleRate;
        source->samples.setSize(1,(int) (96 * sampleRate)); source->samples.clear(); source->manualBpm = 120; source->manualBeatOrigin = 0;
        for (double depth : {.3,.35,.65,.75}) for (int mode : {1,2})
        {
            auto p = legacy; for (auto& c : p.clips) { c.controls.automation = {}; c.media = source; }
            for (auto& link : p.transitionLinks) link.bands = {};
            auto reference = p; reference.setTransitionFilter(id,{(TransitionFilterMode) mode,depth});
            const auto placed = placeTransitionFilterNodes(p,id,{(TransitionFilterMode) mode,depth});
            int most = 0; double error = 0;
            for (int side = 0; side < 2; ++side)
            {
                const auto& clip = p.clips[(size_t) side]; const auto& curve = clip.controls.automation[5];
                most = std::max(most,(int) curve.points.size()); const auto playback = reference.transitionPlaybackFor(reference.clips[(size_t) side]);
                const auto& envelope = playback.filters[(size_t) (1-side)];
                for (int n = 0; n <= 4096; ++n)
                {
                    const double time = std::min(std::nextafter(clip.end(),clip.start),clip.start + clip.length*n/4096.);
                    const double amount = envelope.amountAt(time), upper = std::min(20000.,sampleRate*.45), ratio = upper/20;
                    const double hz = envelope.highPass() ? 20*std::pow(ratio,.8*amount) : upper*std::pow(ratio,-.8*amount);
                    const double expected = amount <= 0 ? 0 : (envelope.highPass() ? (hz-20)/19980 : (hz-20000)/19980) * std::min(1.,amount/.02);
                    error = std::max(error,std::abs(curve.valueAt(clip.sourceElapsed(time-clip.start),0)-expected));
                }
            }
            check(placed.wasOk() && most <= 20,"sparse Filter sweep / " + juce::String(sampleRate) + " Hz / mode " + juce::String(mode) + " / depth " + juce::String(depth)
                + " / maximum nodes " + juce::String(most));
            check(error <= .0000101,"sparse Filter retains frequency/wet-entry accuracy / maximum cutoff error " + juce::String(error,9));
            if (sampleRate == 44100 && depth == .35 && mode == 1)
            {
                TransitionStyle style; const auto captured = p.captureTransitionStyle(id,style);
                const auto file = report.getSiblingFile("sparse-filter.alloypreset"); NamedTransitionStyle loaded;
                const auto written = writeTransitionPreset(file,{"Sparse filter",style}), read = readTransitionPreset(file,loaded);
                double rounding = 0;
                if (style.nodes && loaded.style.nodes) for (int side = 0; side < 2; ++side) for (int target = 0; target < trackAutomationCount; ++target)
                {
                    const auto& a = style.nodes->curves[(size_t) side][(size_t) target].points;
                    const auto& b = loaded.style.nodes->curves[(size_t) side][(size_t) target].points;
                    for (size_t n = 0; n < std::min(a.size(),b.size()); ++n) rounding = std::max(rounding,std::abs(a[n].bend-b[n].bend));
                }
                check(captured.wasOk() && written.wasOk() && read.wasOk() && loaded.style == style && rounding < 1.e-12,
                    "portable preset4 retains sparse exponential spans at serialization precision / error " + written.getErrorMessage() + read.getErrorMessage()
                    + " / bend rounding x1e18 " + juce::String(rounding * 1.e18,3));
                auto window = std::make_unique<Workstation>(false); auto& ui = *window; ui.stopTimer(); ui.engine.project = p;
                ui.engine.library = {source}; ui.selectedClip = id; ui.selectedTransition = id; ui.engine.selectedTrack = 1; ui.selectedAutomationTarget = (int) AutomationTarget::cutoff;
                for (int width : {1250,1920})
                {
                    ui.setSize(width,1080); ui.refresh(); ui.timeline.pixelsPerSecond = 28; ui.timeline.updateSize(); ui.viewport.setViewPosition(840,0);
                    auto stream = report.getSiblingFile("sparse-filter-" + juce::String(width) + ".png").createOutputStream(); juce::PNGImageFormat png;
                    check(stream && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()),*stream),"capture sparse ordinary Filter nodes at width " + juce::String(width));
                }
            }
        }
    }
    for (int mode = 1; mode <= 2; ++mode)
    {
        auto p = legacy; for (auto& c : p.clips) c.controls.automation = {};
        for (auto& link : p.transitionLinks) link.bands = {};
        const auto placed = placeTransitionFilterNodes(p,id,{(TransitionFilterMode) mode,.75});
        const auto first = json(p); const auto repeated = placeTransitionFilterNodes(p,id,{(TransitionFilterMode) mode,.75});
        check(placed.wasOk() && repeated.wasOk() && first == json(p) && p.clips[0].controls.automation[5].active()
            && p.clips[1].controls.automation[5].active() && !p.transitionLinks[0].filter.active(),
            "each Filter preset places regular editable cutoff nodes without a second function / " + juce::String(mode) + " / " + placed.getErrorMessage());
    }
    {
        auto p = legacy; p.clips[0].controls.automation[0] = {true,{{0,-12,.6},{48,-3,0}}};
        const auto original = p.clips[0].controls.automation[0];
        const auto placed = placeTransitionBandNodes(p,id,0,{TransitionBandMode::crossfade,.75,0,.5});
        bool exterior = placed.wasOk();
        for (double time = 0; time < 32; time += .031)
            exterior &= std::abs(p.clips[0].controls.automation[0].valueAt(time,0)-original.valueAt(time,0)) < .00001;
        check(exterior,"replacing a frame through a bent manual segment preserves the complete exterior curve: " + placed.getErrorMessage());
        auto disabled = legacy; disabled.clips[0].controls.automation[0] = original; disabled.clips[0].controls.automation[0].enabled = false;
        const auto before = json(disabled);
        check(placeTransitionBandNodes(disabled,id,0,{TransitionBandMode::crossfade,.75,0,.5}).failed() && json(disabled) == before,
            "a preset cannot silently enable a disabled exterior draft when replacing only its frame");
    }
    auto owned = std::make_unique<Workstation>(false); auto& ui = *owned; ui.stopTimer(); ui.setSize(1920,1080);
    ui.engine.project = legacy; ui.engine.library = {media}; ui.selectedClip = id; ui.selectedAutomationTarget = 1; ui.selectTrack(1); ui.refresh();
    ui.timeline.pixelsPerSecond = 40; ui.timeline.updateSize(); ui.viewport.setViewPosition(64 * 40,0);
    const auto view = ui.timeline.automationViewCurve(address);
    check(std::any_of(view.points.begin(),view.points.end(),[&](const auto& point) { return point.time == cutSource && point.value == -60; }),
        "actual selected Bass curve provides outgoing nodes without selecting a separate transition function");
    const auto beforeUiEdit = json(ui.engine.project); const auto history = ui.undoHistory.size();
    check(ui.insertAutomationNode(address,cut+.5,false) && !ui.engine.project.transitionLinks[1].bands[1] && ui.undoHistory.size() == history+1,
        "actual Add node materialises the cut and uses exactly one Undo");
    ui.undo(false); check(json(ui.engine.project) == beforeUiEdit,"Undo restores the complete legacy shape and both sides after ordinary node insertion");
    const auto amounts = ui.timeline.cutAmountHandles();
    const auto amount = std::find_if(amounts.begin(),amounts.end(),[&](const auto& h) { return h.address == address && h.node >= 0 && std::abs(h.time-cut) < 1.e-7 && h.value == 0; });
    check(amount != amounts.end(),"outgoing cut has an ordinary amount handle without selecting the transition frame");
    if (amount != amounts.end())
    {
        check(ui.timeline.setCutAmount(*amount,.1f,true) && !ui.engine.project.transitionLinks[1].bands[1],"ordinary cut-amount edit changes the preset node and removes its hidden generator");
        ui.undo(false); check(json(ui.engine.project) == beforeUiEdit,"Undo amount edit restores exact original arrangement");
    }
    ui.timeline.selectAutomationPoint(address,(int) (std::find_if(view.points.begin(),view.points.end(),[&](const auto& p) { return p.time == cutSource && p.value == -60; })-view.points.begin()));
    const auto mouse = juce::Desktop::getInstance().getMainMouseSource();
    const juce::Point<float> down{(float) (cut * ui.timeline.pixelsPerSecond),ui.timeline.automationY(1,-60,1)}, to = down.translated(40,0);
    auto event = [&](juce::Point<float> at,bool drag)
    { return juce::MouseEvent(mouse,at,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::altModifier),1,0,0,0,0,&ui.timeline,&ui.timeline,juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),1,drag); };
    ui.timeline.mouseDown(event(down,false)); ui.timeline.mouseDrag(event(to,true)); ui.timeline.mouseUp(event(to,true));
    check(!ui.engine.project.transitionLinks[1].bands[1] && ui.undoHistory.size() == history+1,
        "actual node drag edits the swap itself through the normal node route with one Undo");
    ui.undo(false); check(json(ui.engine.project) == beforeUiEdit,"Undo swap-node drag restores both original treatments and manual incoming nodes");
    ui.timeline.mouseDown(event(down,false));
    const auto earlier = down.translated(-40,0);
    ui.timeline.mouseDrag(event(earlier,true)); ui.timeline.mouseUp(event(earlier,true));
    const auto* earlierCurve = ui.engine.project.curve(address);
    const double earlierTime = ui.engine.project.automationTime(address,(double) earlier.x/ui.timeline.pixelsPerSecond);
    check(earlierCurve && std::any_of(earlierCurve->points.begin(),earlierCurve->points.end(),[&](const auto& point)
        { return std::abs(point.time-earlierTime) < 1.e-7 && point.time < cutSource-.999 && point.value == -60; }),
        "ordinary swap node moves a full second earlier without a redundant guard pinning its time");
    ui.undo(false); check(json(ui.engine.project) == beforeUiEdit,"Undo earlier swap drag restores exact previous nodes and timing");
    const auto cutIndex = (int) (std::find_if(view.points.begin(),view.points.end(),[&](const auto& point) { return point.time == cutSource && point.value == -60; })-view.points.begin());
    check(ui.deleteAutomationNode(address,cutIndex) && !ui.engine.project.transitionLinks[1].bands[1]
        && std::none_of(ui.engine.project.curve(address)->points.begin(),ui.engine.project.curve(address)->points.end(),[&](const auto& point) { return point.time == cutSource; }),
        "Delete removes the preset's actual swap node through the ordinary curve owner");
    ui.undo(false); check(json(ui.engine.project) == beforeUiEdit,"Undo Delete restores the original swap and manual source nodes");
    TransitionStyle captured; check(converted.captureTransitionStyle(converted.clips[1].id,captured).wasOk() && captured.nodes.has_value(),
        "saved/custom presets capture ordinary nodes instead of a separate generator description");
    if (captured.nodes)
    {
        const auto portable = report.getSiblingFile("ordinary-nodes.alloypreset"); NamedTransitionStyle loaded;
        check(writeTransitionPreset(portable,{"Ordinary nodes",captured}).wasOk() && (int) juce::JSON::parse(portable)["version"] == 5
            && readTransitionPreset(portable,loaded).wasOk() && loaded.style == captured,
            "portable node presets use format3 so an older generator-only app rejects them instead of loading an empty shape");
        TransitionStyle::Nodes decoded;
        check(readPresetNodes(presetNodesToJson(*captured.nodes),decoded).wasOk() && decoded == *captured.nodes,"ordinary preset-node template round-trips all owners, times, values and bends");
        auto invalid = presetNodesToJson(*captured.nodes); invalid.getArray()->removeLast();
        check(readPresetNodes(invalid,decoded).failed(),"malformed preset side list refuses before replacing a saved node template");
        auto mixed = captured; mixed.bands[1] = TransitionBandSettings{TransitionBandMode::swap};
        check(mixed.validate().failed(),"a saved preset cannot combine ordinary nodes with a second procedural function");
    }
    for (int width : {1250,1920})
    {
        ui.setSize(width,1080); ui.refresh(); ui.timeline.selectAutomationPoint(address,cutIndex);
        auto output = report.getSiblingFile("preset-nodes-" + juce::String(width) + ".png").createOutputStream(); juce::PNGImageFormat png;
        check(output && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()),*output),"capture ordinary preset nodes / " + juce::String(width));
    }
    media->file = report.getSiblingFile("ordinary-preset-source.wav");
    {
        juce::WavAudioFormat wav; std::unique_ptr<juce::OutputStream> stream = media->file.createOutputStream();
        auto writer = wav.createWriterFor(stream,juce::AudioFormatWriterOptions().withSampleRate(8000).withNumChannels(1).withBitsPerSample(24));
        check(writer && writer->writeFromAudioSampleBuffer(media->samples,0,media->frames()),"write owned audio for ordinary-node preset Save/Open");
    }
    converted.storeTransitionAlternative(id,0);
    converted.putTransitionPreset({"Ordinary full cuts",captured});
    auto saver = std::make_unique<Engine>(juce::File(),false); saver->project = converted; saver->library = {media};
    const auto file = report.getSiblingFile("ordinary-preset.mixworks"); const auto saved = saver->saveProject(file);
    auto reopened = std::make_unique<Engine>(juce::File(),false); const auto opened = saved.isEmpty() ? reopened->restoreProject(file.loadFileAsString()) : saved;
    check(saved.isEmpty() && opened.isEmpty() && reopened->project.clips.size() == 3 && !reopened->project.transitionLinks[1].bands[1]
        && reopened->project.transitionPresets.size() == 1 && reopened->project.transitionPresets[0].style == captured,
        "production Save/Open retains ordinary cuts, alternatives and named node templates / " + opened);
    bool savedNodes = opened.isEmpty() && reopened->project.clips.size() == converted.clips.size();
    for (size_t i = 0; savedNodes && i < converted.clips.size(); ++i)
    {
        const auto& a = converted.clips[i].controls.automation[1]; const auto& b = reopened->project.clips[i].controls.automation[1];
        savedNodes &= a.enabled == b.enabled && a.points.size() == b.points.size();
        for (size_t n = 0; savedNodes && n < a.points.size(); ++n) savedNodes &= a.points[n].time == b.points[n].time && a.points[n].value == b.points[n].value && a.points[n].bend == b.points[n].bend;
    }
    check(savedNodes,"production reopen preserves every ordinary source time/value/bend exactly without regenerating a preset");
}
}
