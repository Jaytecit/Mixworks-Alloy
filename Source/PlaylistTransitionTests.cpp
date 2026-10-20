#include "Workstation.h"
#include "Beatmatch.h"
#include "EnergySections.h"
#include <cmath>
#include <limits>

namespace mixworks
{
void runPlaylistTransitionTests(const std::function<void(bool, const juce::String&)>& check, const juce::File& report)
{
    auto source = std::make_shared<Media>(); source->name = "Energy sections / test source";
    source->sampleRate = 8000; source->samples.setSize(1, 96 * 8000); source->samples.clear();
    source->analysis.bpm = 120; source->analysis.beatOrigin = 0; source->analysis.downbeatOrigin = 0; source->analysis.tempoConfidence = 1;
    const int bucket = source->frames() / 4000;
    const int buckets = (source->frames() + bucket - 1) / bucket;
    source->peaks.assign((size_t) buckets, .8f); source->waveformRms.assign((size_t) buckets, .2f);
    check(detectEnergySections(*source).empty(), "steady retained energy produces no invented sections");
    for (int i = 0; i < buckets; ++i)
    {
        const double t = (i + .5) * bucket / source->sampleRate;
        source->waveformRms[(size_t) i] = t < 32 || t >= 64 ? .08f : .4f;
    }
    const auto sections = detectEnergySections(*source);
    check(sections.size() == 2 && std::abs(sections[0].sourceSeconds - 32) < .01 && std::abs(sections[1].sourceSeconds - 64) < .01
        && sections[0].contrastDb > 3 && sections[1].contrastDb < -3, "sustained quiet/loud changes produce source-bar section suggestions at known boundaries");
    auto rms = source->waveformRms;
    source->analysis.downbeatOrigin = -1; check(detectEnergySections(*source).empty(), "unknown source bar phase does not manufacture section alignment");
    source->analysis.downbeatOrigin = 48;
    const auto late = detectEnergySections(*source);
    check(late.size() == sections.size() && late[0].sourceSeconds == sections[0].sourceSeconds, "a downbeat anchor later in the source extends section phase backwards");
    source->analysis.downbeatOrigin = 0; source->analysis.variableTempo = true;
    check(detectEnergySections(*source).empty(), "variable-tempo source suggestions remain unavailable until a constant clock is selected");
    source->analysis.variableTempo = false; source->waveformRms[0] = std::numeric_limits<float>::quiet_NaN();
    check(detectEnergySections(*source).empty(), "malformed waveform data cannot contaminate section positions");
    source->waveformRms.assign(rms.size(), 0); check(detectEnergySections(*source).empty(), "silent sources produce no energy sections");
    source->waveformRms.clear(); check(detectEnergySections(*source).empty(), "missing retained RMS does not trigger decoding or guessed sections");
    source->waveformRms = rms;

    {
        Clip out, in; out.media = in.media = source; out.length = in.length = source->duration();
        const auto both = chooseSectionTransition(out, in);
        check(both && both->matchedSections == 2 && std::abs(both->overlapBeats - 128) < 1e-7,
            "section planning prefers a valid two-track section alignment over a shorter one-track choice");
        auto plain = source->catalogueCopy(); plain->samples = source->samples; plain->waveformRms.clear();
        in.media = plain; const auto outgoingOnly = chooseSectionTransition(out, in);
        out.media = plain; in.media = source; const auto incomingOnly = chooseSectionTransition(out, in);
        check(outgoingOnly && outgoingOnly->matchedSections == 1 && incomingOnly && incomingOnly->matchedSections == 1,
            "one usable track supplies a section alignment when the other has no section evidence");
        in.media = plain; const auto fallback = chooseSectionTransition(out, in);
        check(fallback && fallback->matchedSections == 0 && fallback->overlapBeats == 32,
            "missing section evidence uses the minimum eight-bar grid fallback without inventing a section");
        in.length = 8; check(!chooseSectionTransition(out, in), "automatic transitions reject sources shorter than the eight-bar minimum");
        out.media = in.media = source; out.offset = in.offset = 16; out.length = in.length = 80;
        const auto trimmed = chooseSectionTransition(out, in);
        check(trimmed && trimmed->matchedSections == 2 && std::abs(trimmed->overlapBeats - 96) < 1e-7,
            "trimmed sources plan section alignment from their visible source spans");
        source->sampleRate = 2000; out.offset = in.offset = 0; out.length = in.length = source->duration();
        const auto bounded = chooseSectionTransition(out, in);
        check(bounded && bounded->matchedSections == 1 && bounded->overlapBeats == 256,
            "section choices outside the last 64 outgoing bars or requiring over 64 overlap bars cannot win");
        source->sampleRate = 8000;
    }

    Workstation ui(false); ui.stopTimer(); ui.setSize(1440, 940);
    check(ui.selectedTransitionPreset() == TransitionPreset::crossfade && ui.transitionOverlapBeats() == 256
        && ui.transitionLength.getText() == "Sections 8-64 bars", "new playlist additions default to section-aligned Crossfade within 8-64 bars");
    ui.engine.project.followTrackTempo = false; ui.engine.project.bpm = ui.engine.project.tempo.fallback = 120;
    ui.insertTimelineMedia({source, source}, {});
    bool captured = true; for (const auto& clip : ui.engine.project.clips)
        captured = captured && clip.pendingTransitionBeats == 256 && clip.pendingTransitionPreset == TransitionPreset::crossfade && clip.pendingSectionTransition == true;
    check(captured, "each inserted track captures its default treatment before asynchronous preparation");
    ui.transitionIndex = 1; ui.transitionLength.setSelectedId(2, juce::dontSendNotification); ui.transitionPreset.setSelectedId(1, juce::dontSendNotification);
    const auto matched = prepareTempoMap(ui.engine.project, false, 0, 0, 0, {}, TransitionPreset::alignOnly);
    check(matched.error.isEmpty() && matched.project.clips.size() == 2 && matched.project.transitionLinks.size() == 1,
        "production placement applies the captured insertion default despite later changes to toolbar defaults");
    if (matched.error.isNotEmpty() || matched.project.transitionLinks.empty()) return;
    {
        auto candidate = matched.project;
        Clip incoming; incoming.media = source; incoming.length = 4; incoming.pendingPlacement = PendingPlacement::append;
        incoming.pendingSectionTransition = true; incoming.pendingTransitionPreset = TransitionPreset::crossfade;
        candidate.clips.push_back(incoming);
        const auto shortSource = prepareTempoMap(candidate, false, 256, 0, 0, {}, TransitionPreset::crossfade, true);
        check(shortSource.error.isNotEmpty() && juce::JSON::toString(projectToJson(shortSource.project)) == juce::JSON::toString(projectToJson(candidate)),
            "short automatic additions reject transactionally instead of creating an under-eight-bar frame");
        candidate.clips.back().length = source->duration();
        const auto lateSource = prepareTempoMap(candidate, false, 256, 0, 0, candidate.duration() - 1, TransitionPreset::crossfade, true);
        check(lateSource.error.isNotEmpty() && juce::JSON::toString(projectToJson(lateSource.project)) == juce::JSON::toString(projectToJson(candidate)),
            "too-late live additions cannot silently shorten the eight-bar automatic minimum");
    }
    ui.engine.project = matched.project; ui.undoHistory.clear(); ui.redoHistory.clear(); ui.refresh();
    const auto incoming = ui.engine.project.clips[1].id, outgoing = ui.engine.project.clips[0].id;
    const auto link = *ui.engine.project.transitionFor(incoming); const auto range = ui.engine.project.transitionRange(link);
    check(std::abs(range.getLength() - 64) < 1e-6 && std::abs(ui.engine.project.clips[0].fadeOut - 64) < 1e-6
        && std::abs(ui.engine.project.clips[1].fadeIn - 64) < 1e-6, "actual overlap is chosen from both section changes instead of a fixed bar length");
    check(std::abs(ui.engine.project.clips[0].start + ui.engine.project.clips[0].audioTimelineTime(64)
        - ui.engine.project.clips[1].start - ui.engine.project.clips[1].audioTimelineTime(32)) < 1e-6,
        "default placement aligns the outgoing and incoming energy-section changes to the same project bar");
    const auto json = [](const Project& p) { return juce::JSON::toString(projectToJson(p), juce::JSON::FormatOptions().withMaxDecimalPlaces(17)); };
    const auto baseline = json(ui.engine.project);
    const auto rowFor = [&](const juce::String& id) { for (int row = 0; row < ui.playlistTable.getNumRows(); ++row) if (ui.playlistTable.clipAtRow(row)->id == id) return row; return -1; };
    std::unique_ptr<juce::Component> cell(ui.playlistTable.refreshComponentForCell(rowFor(incoming), 6, false, nullptr));
    auto* picker = dynamic_cast<juce::ComboBox*>(cell.get());
    check(picker && picker->isEnabled() && picker->getSelectedId() == 3, "playlist row exposes the current incoming Crossfade in a native dropdown");
    if (!picker) return;
    picker->setSelectedId(1, juce::dontSendNotification); picker->onChange();
    check(ui.undoHistory.size() == 1 && ui.engine.project.clips[0].fadeOut == 0 && ui.engine.project.clips[1].fadeIn == 0
        && ui.engine.project.clips[1].start == matched.project.clips[1].start, "row preset swapping changes only the pair treatment with one Undo");
    ui.undo(false); check(json(ui.engine.project) == baseline, "Undo restores the exact automatically aligned transition");
    ui.undo(true); check(ui.engine.project.clips[1].fadeIn == 0, "Redo reapplies the row transition treatment");
    ui.undo(false);
    TransitionStyle saved; check(ui.engine.project.captureTransitionStyle(incoming, saved).wasOk(), "a playlist transition can use the existing saved-style owner");
    saved.filter.mode = TransitionFilterMode::highToLow; saved.filter.strength = .5;
    ui.engine.project.transitionPresets.push_back({"Half sweep", saved}); ui.refresh();
    cell.release(); cell.reset(ui.playlistTable.refreshComponentForCell(rowFor(incoming), 6, false, picker));
    picker = dynamic_cast<juce::ComboBox*>(cell.get()); picker->setSelectedId(100, juce::dontSendNotification); picker->onChange();
    check(ui.engine.project.transitionFor(incoming)->filter.mode == TransitionFilterMode::highToLow
        && ui.engine.project.clips[1].start == matched.project.clips[1].start, "saved session presets are directly available in each playlist row and retain placement");
    ui.playlistTable.sortOrderChanged(3, false);
    cell.release(); cell.reset(ui.playlistTable.refreshComponentForCell(rowFor(outgoing), 6, false, picker)); picker = dynamic_cast<juce::ComboBox*>(cell.get());
    check(!picker->isEnabled() && picker->getText() == "No incoming transition", "sorting and recycled cells keep the first track's missing incoming relationship explicit");
    cell.release(); cell.reset(ui.playlistTable.refreshComponentForCell(rowFor(incoming), 6, false, picker)); picker = dynamic_cast<juce::ComboBox*>(cell.get());
    picker->setSelectedId(3, juce::dontSendNotification); picker->onChange();
    check(ui.engine.project.transitionFor(incoming)->filter.mode == TransitionFilterMode::none, "recycled sorted row still targets its clip identity when switching back to Crossfade");
    check(ui.engine.project.setTransitionLocked(incoming, true).wasOk(), "transition lock fixture is valid"); ui.refresh();
    cell.release(); cell.reset(ui.playlistTable.refreshComponentForCell(rowFor(incoming), 6, false, picker)); picker = dynamic_cast<juce::ComboBox*>(cell.get());
    const auto locked = json(ui.engine.project); picker->setSelectedId(1, juce::dontSendNotification); picker->onChange();
    check(!picker->isEnabled() && json(ui.engine.project) == locked, "locked transitions disable row changes and preserve both tracks");
    ui.engine.project.setTransitionLocked(incoming, false); ui.refresh(); ui.showBrowserPage(1);
    ui.transitionIndex = 3; ui.transitionLength.setSelectedId(4, juce::dontSendNotification); ui.transitionPreset.setSelectedId(3, juce::dontSendNotification);
    ui.timeline.pixelsPerSecond = 3; ui.timeline.updateSize();
    const bool hints = ui.showEnergySections; const auto beforeToggle = json(ui.engine.project); const auto undoSize = ui.undoHistory.size();
    ui.menuItemSelected(85, 4); check(ui.showEnergySections != hints && json(ui.engine.project) == beforeToggle && ui.undoHistory.size() == undoSize,
        "hiding suggested sections is display-only and does not enter musical Undo"); ui.menuItemSelected(85, 4);
    // An offset/slip/stretch uses the waveform's actual source-to-audio clock.
    Clip clock = ui.engine.project.clips[1]; clock.offset = 8; clock.rate = 1.25; clock.audioSlip = .3;
    const double local = clock.audioTimelineTime(sections[0].sourceSeconds - clock.offset);
    check(std::abs(clock.audioSourceElapsed(local) + clock.offset - sections[0].sourceSeconds) < 1e-7,
        "section overlay uses the waveform clock through source trimming, stretch and audio slip");
    const auto capture = [&](const juce::String& name)
    { auto output = report.getParentDirectory().getChildFile(name).createOutputStream(); juce::PNGImageFormat png;
      return output && png.writeImageToStream(ui.createComponentSnapshot(ui.getLocalBounds()), *output); };
    check(capture("playlist-transitions-1440.png"), "native playlist preset picker and section suggestions are captured at 1440 pixels");
    ui.setSize(1250, 800); check(capture("playlist-transitions-1250.png"), "native playlist actions remain visible at minimum supported window width");
    const auto capturePanel = [&](const juce::String& name)
    { auto output = report.getParentDirectory().getChildFile(name).createOutputStream(); juce::PNGImageFormat png;
      return output && png.writeImageToStream(ui.transitionPanel.createComponentSnapshot(ui.transitionPanel.getLocalBounds()), *output); };
    ui.transitionPanel.setSize(1240, 370); ui.layoutTransitionUI();
    check(!ui.transitionAdvancedOpen && !ui.transitionAdvancedBody.isVisible()
        && ui.transitionBandModes[0].getY() == ui.transitionBandModes[3].getY(),
        "clean wide transition panel has four aligned bands and collapsed advanced tools");
    check(capturePanel("transitions-clean-wide.png"), "capture the cleaned wide transition controls");
    const auto panelBefore = json(ui.engine.project); const auto panelUndo = ui.undoHistory.size();
    ui.transitionAdvanced.onClick();
    ui.transitionPanel.setSize(1240, 580); ui.layoutTransitionUI();
    check(ui.transitionAdvancedBody.isVisible() && ui.rehearsalLeadIn.getParentComponent() == &ui.transitionAdvancedBody
        && json(ui.engine.project) == panelBefore && ui.undoHistory.size() == panelUndo,
        "expanding rehearsal and preset tools retains their handlers without modifying the mix");
    check(capturePanel("transitions-expanded-wide.png"), "capture expanded rehearsal and saved-preset groups");
    ui.transitionAdvanced.onClick(); ui.transitionPanel.setSize(610, 490); ui.layoutTransitionUI();
    check(ui.transitionBandModes[0].getY() < ui.transitionBandModes[2].getY()
        && !ui.transitionPresetForSelection.getBounds().intersects(ui.transitionLock.getBounds())
        && ui.transitionPresetForSelection.getWidth() >= 120,
        "compact transition layout wraps into two band columns and retains readable primary controls");
    check(capturePanel("transitions-clean-compact.png"), "capture compact transition controls without track metadata or instruction paragraphs");
    check(ui.engine.project.clips[0].media->waveformRms == rms && ui.engine.project.clips[0].media->manualBpm == 0,
        "section suggestions and playlist presets retain original automatic analysis and waveform facts");
}
}
