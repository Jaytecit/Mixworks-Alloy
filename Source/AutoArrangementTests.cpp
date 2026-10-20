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
    check(plan.project.transitionLinks.size() == 2 && plan.project.clips[1].fadeIn > 0 && plan.project.clips[0].fadeOut > 0
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
            && rising.project.transitionLinks[0].filter.mode == TransitionFilterMode::lowToHigh
            && rising.project.transitionLinks[0].bands[1] && rising.project.transitionLinks[0].bands[1]->mode == TransitionBandMode::swap,
            "Balanced uses a gentle opening filter and bass handover for measured energy increase");
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
        check(falling.error.isEmpty() && falling.project.transitionLinks.size() == 1 && falling.project.transitionLinks[0].filter.mode == TransitionFilterMode::highToLow,
            "measured energy decrease chooses the other paired filter direction");
        styled.startingId = quiet->id; styled.transitions = AutoTransitionStyle::bold;
        auto bold = planAutoArrangement({quiet, loud}, styled, random);
        check(bold.error.isEmpty() && bold.project.transitionLinks.size() == 1 && bold.project.transitionLinks[0].filter.strength == .65
            && bold.project.transitionLinks[0].bands[1] && bold.project.transitionLinks[0].bands[1]->mode == TransitionBandMode::swap && bold.project.transitionLinks[0].bands[2],
            "Bold uses stronger filters, tighter bass handover and extra tone fades");
        check(noDefaultFade(bold.project), "Bold has no inherited full-band volume crossfade beneath its selected band/filter treatment");
        styled.transitions = AutoTransitionStyle::smooth;
        auto smooth = planAutoArrangement({quiet, loud}, styled, random);
        check(smooth.error.isEmpty() && smooth.project.transitionLinks.size() == 1 && !smooth.project.transitionLinks[0].filter.active() && !smooth.project.transitionLinks[0].bands[1]
            && smooth.project.clips[1].fadeIn > 0 && smooth.project.clips[0].fadeOut > 0,
            "Smooth keeps simple volume blends despite available energy evidence");
        styled.transitions = AutoTransitionStyle::balanced; waveform(*loud, .1f);
        auto steady = planAutoArrangement({quiet, loud}, styled, random);
        check(steady.error.isEmpty() && steady.project.transitionLinks.size() == 1 && !steady.project.transitionLinks[0].filter.active() && steady.project.transitionLinks[0].bands[1],
            "steady similar energy uses bass handover without an unnecessary filter sweep");
        loud->waveformRms.pop_back();
        auto missingEnergy = planAutoArrangement({quiet, loud}, styled, random);
        check(missingEnergy.error.isEmpty() && missingEnergy.project.transitionLinks.size() == 1 && !missingEnergy.project.transitionLinks[0].filter.active()
            && !missingEnergy.project.transitionLinks[0].bands[1] && !sourceEnergy(*loud, 0, 10)
            && missingEnergy.project.clips[0].fadeOut > 0 && missingEnergy.project.clips[1].fadeIn > 0,
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
                && selected.project.transitionLinks[0].bands[0]
                && !selected.project.transitionLinks[0].bands[1] && !selected.project.transitionLinks[0].filter.active(),
                "explicit volume style replaces default fades and adds no automatic bass or filter treatment");
            const auto& p = selected.project; const auto& link = p.transitionLinks[0]; const auto range = p.transitionRange(link);
            const auto out = p.transitionPlaybackFor(p.clips[0]), in = p.transitionPlaybackFor(p.clips[1]);
            if (choice == AutoTransitionStyle::constantPower)
            {
                bool power = true;
                for (int i = 0; i <= 20; ++i)
                {
                    const double at = range.getStart() + range.getLength() * i / 20.;
                    const auto aGain = out.gainAt(0, at), bGain = in.gainAt(0, at);
                    power &= std::abs(aGain * aGain + bGain * bGain - 1) < 1e-6;
                }
                check(power, "constant-power envelopes preserve summed power throughout the handover for equally loud unrelated sources");
            }
            else
            {
                const double first = range.getStart() + range.getLength() * .25, last = range.getStart() + range.getLength() * .75;
                check(out.gainAt(0, first) == 1 && in.gainAt(0, first) == 0 && out.gainAt(0, last) == 0 && in.gainAt(0, last) == 1
                    && out.pairs[1][0].end - out.pairs[1][0].begin < .005,
                    "hard cuts keep only one song audible away from the short midpoint click-prevention ramp");
            }
            const auto document = parseProject(juce::JSON::toString(projectToJson(p)));
            check(document.error.isEmpty() && document.project.transitionLinks[0].bands == link.bands,
                "chosen volume style persists through session serialization");
        }
        styled.transitions = AutoTransitionStyle::balanced; styled.energy = AutoEnergyPolicy::maintain;
        waveform(*quiet, .2f); waveform(*loud, .1f); waveform(*third, .3f);
        const auto maintained = planAutoArrangement({quiet, loud, third}, styled, random);
        check(maintained.error.isEmpty() && maintained.project.clips.size() == 2 && maintained.project.clips[1].media == third,
            "Keep pace skips a quieter near-BPM candidate for a track whose overlap energy does not drop");
        styled.energy = AutoEnergyPolicy::build;
        auto slight = source("Slight increase"); waveform(*slight, .21f);
        const auto built = planAutoArrangement({quiet, slight, third}, styled, random);
        check(built.error.isEmpty() && built.project.clips.size() == 2 && built.project.clips[1].media == third,
            "Build energy requires at least one dB in each overlap half instead of merely any increase");
        auto slower = source("Louder but slower", 118); waveform(*slower, .4f);
        const auto slowerRejected = planAutoArrangement({quiet, slower}, styled, random);
        check(slowerRejected.project.clips.size() == 1 && slowerRejected.stopReason.contains("energy choice"),
            "Build energy does not trade a louder recording for a BPM drop");
        styled.energy = AutoEnergyPolicy::windDown;
        const auto winding = planAutoArrangement({quiet, loud, third}, styled, random);
        check(winding.project.clips.size() == 2 && winding.project.clips[1].media == loud,
            "Wind down chooses a lower-energy handover without a tempo rise");
        styled.energy = AutoEnergyPolicy::maintain; loud->waveformRms.clear();
        const auto unavailable = planAutoArrangement({quiet, loud}, styled, random);
        check(unavailable.project.clips.size() == 1 && unavailable.stopReason.contains("energy unavailable: 1"),
            "an energy-constrained mix stops explicitly when candidate energy is unknown");
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
        if (within) { const auto window = bounded.project.transitionRange(bounded.project.transitionLinks[0]); within = bounded.project.tempo.beatsAt(window.getEnd()) - bounded.project.tempo.beatsAt(window.getStart()) <= 128 + 1e-7; }
        check(within, "section hints outside 32 bars cannot widen actual aligned Auto Arrange overlaps");
        auto invalid = styled; invalid.transitions = static_cast<AutoTransitionStyle>(99);
        check(planAutoArrangement({sectionOut, sectionIn}, invalid, random).error.isNotEmpty(), "unknown blend choices reject at the options boundary");
    }
    options.startingId = a->id; options.trackCount = 2; options.sectionTransitions = false; options.overlapBeats = 16; options.preset = TransitionPreset::bassSwap;
    options.transitions = AutoTransitionStyle::current;
    plan = planAutoArrangement({a, b}, options, random);
    check(plan.error.isEmpty() && plan.project.transitionLinks.size() == 1 && plan.project.transitionLinks[0].bands[1] && plan.project.transitionLinks[0].bands[1]->mode == TransitionBandMode::swap,
        "auto arrangement reuses selected fixed-length Bass swap rather than imposing Crossfade");

    a->samples.setSize(1, a->frames()); a->samples.clear(); b->samples.setSize(1, b->frames()); b->samples.clear();
    for (auto* m : {a.get(), b.get()})
    { const int bucket = std::max(1, m->frames() / 4000); m->waveformRms.assign((size_t) ((m->frames() + (int64_t) bucket - 1) / bucket), m == a.get() ? .1f : .3f); }
    Workstation ui(false); ui.stopTimer(); ui.setSize(1440, 940); ui.engine.library = {a, b};
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
        && dialog->getComboBoxComponent("mode")->getNumItems() == 2 && dialog->getComboBoxComponent("transitions")->getNumItems() == 6
        && dialog->getComboBoxComponent("energy")->getNumItems() == 4,
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
    ui.transitionIndex = 1; ui.transitionPreset.setSelectedId(2, juce::dontSendNotification);
    ui.autoArrangeButton.onClick();
    dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
    if (dialog)
    {
        dialog->getComboBoxComponent("start")->setSelectedItemIndex(1, juce::dontSendNotification);
        dialog->getTextEditor("amount")->setText("2"); dialog->exitModalState(1); pumpAutoMessages(30);
    }
    const double deadline = juce::Time::getMillisecondCounterHiRes() + 20000;
    while (ui.busy && juce::Time::getMillisecondCounterHiRes() < deadline) pumpAutoMessages(20);
    check(!ui.busy && ui.engine.project.clips.size() == 2 && ui.undoHistory.size() == 1 && ui.engine.playbackReady(),
        "production Auto Arrange prepares audio in the background and commits one playable Undo step");
    check(ui.attractState.enabled && ui.attractState.motion == AttractMotion::animated && ui.layout.forgeEffects && ui.forgeMotionAllowed(),
        "successful Auto Arrange activates full attract lighting and animated forge effects");
    check(ui.engine.project.transitionLinks.size() == 1 && ui.engine.project.transitionLinks[0].filter.active()
        && ui.engine.project.transitionLinks[0].bands[1].has_value(), "background audio preparation preserves the chosen automatic filter and bass treatment");
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
            && ui.engine.project.transitionLinks[0].bands[0]
            && ui.engine.project.transitionLinks[0].bands[0]->mode == (style == 4 ? TransitionBandMode::constantPower : TransitionBandMode::swap)
            && ui.engine.project.clips[0].fadeOut == 0 && ui.engine.project.clips[1].fadeIn == 0 && ui.status.contains("Keep pace"),
            "native Auto Arrange dialog passes selected energy and volume style through background preparation into the playable mix");
        dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        if (dialog) { dialog->exitModalState(0); pumpAutoMessages(30); }
    }
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
        check(ui.layout.toolbar.contains(ui.autoArrangeButton.getBounds()) && ui.zoomSlider.getRight() <= ui.autoArrangeButton.getX()
            && ui.autoArrangeButton.getRight() < ui.settingsButton.getX() && ui.autoArrangeButton.isVisible(),
            "Auto Arrange fits between zoom and Audio / MIDI at " + juce::String(width));
        auto image = ui.createComponentSnapshot(ui.getLocalBounds()); juce::PNGImageFormat png;
        auto out = report.getSiblingFile("auto-arrange-" + juce::String(width) + ".png").createOutputStream();
        check(out && png.writeImageToStream(image, *out), "Auto Arrange native toolbar capture at " + juce::String(width));
    }
}
void runAutoArrangementCatalogueProbe(const std::function<void(bool, const juce::String&)>& check, const juce::File& report,
                                     const juce::File& catalogueFile, const juce::String& start)
{
    const auto library = readLibraryCatalogue(catalogueFile);
    check(library.error.isEmpty() && !library.media.empty(), "read the supplied catalogue through its production parser without decoding sources");
    if (library.error.isNotEmpty() || library.media.empty()) return;
    AutoArrangementOptions options; options.startingId = start; options.trackCount = 10;
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
}
}
