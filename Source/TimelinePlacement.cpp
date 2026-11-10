#include "Workstation.h"
#include "Beatmatch.h"
#include <cmath>

namespace mixworks
{
namespace
{
bool audioFiles(const juce::StringArray& paths)
{
    return !paths.isEmpty() && std::all_of(paths.begin(), paths.end(), [](const auto& path)
    { return juce::File::isAbsolutePath(path) && juce::File(path).hasFileExtension("wav;aif;aiff;flac;ogg;mp3"); });
}
juce::StringArray draggedSources(const juce::var& description)
{
    juce::StringArray ids;
    if (description.isString()) ids.add(description.toString());
    else if (const auto* values = description.getArray()) for (const auto& value : *values)
    {
        if (!value.isString() || value.toString().isEmpty() || ids.contains(value.toString())) return {};
        ids.add(value.toString());
    }
    return ids;
}
}
juce::var LibraryTable::getDragSourceDescription(const juce::SparseSet<int>& rows)
{
    if (isPlaylist || owner.busy || rows.isEmpty()) return {};
    juce::Array<juce::var> ids;
    // SparseSet rows are ascending display indices, not selection-click order
    // or underlying catalogue order. Freeze identities at drag start.
    for (int i = 0; i < rows.size(); ++i)
    {
        const int row = rows[i]; if (row < 0 || row >= (int)owner.visibleMedia.size()) return {};
        ids.add(owner.engine.library[(size_t)owner.visibleMedia[(size_t)row]]->id);
    }
    return ids.size() == 1 ? ids[0] : juce::var(ids);
}
bool Timeline::isInterestedInDragSource(const SourceDetails& details)
{
    const auto ids = draggedSources(details.description);
    return !owner.busy && details.sourceComponent.get() == &owner.libraryTable.table && !ids.isEmpty()
        && std::all_of(ids.begin(),ids.end(),[&](const auto& id)
        { return std::any_of(owner.engine.library.begin(),owner.engine.library.end(),[&](const auto& media) { return media->id == id; }); });
}
std::optional<TimelinePlacement> Timeline::placementAt(juce::Point<int> position) const
{
    // JUCE supplies content coordinates, including both viewport scroll offsets.
    if (owner.busy || !owner.viewport.getViewArea().contains(position) || position.x < 0
        || position.y < 0 || position.y >= laneTop(contentLaneCount()) || !std::isfinite(pixelsPerSecond) || pixelsPerSecond <= 0) return {};
    const auto& p = owner.engine.project;
    const double time = p.snappedTime(position.x / pixelsPerSecond, p.snap);
    return TimelinePlacement {trackAt((float) position.y), time};
}
void Timeline::previewDrop(juce::Point<int> position) { dropPreview = placementAt(position); repaint(); }
void Timeline::clearDropPreview() { dropPreview.reset(); repaint(); }
void Timeline::itemDragEnter(const SourceDetails& details) { itemDragMove(details); }
void Timeline::itemDragMove(const SourceDetails& details)
{
    if (isInterestedInDragSource(details)) previewDrop(details.localPosition); else clearDropPreview();
}
void Timeline::itemDragExit(const SourceDetails&) { clearDropPreview(); }
void Timeline::itemDropped(const SourceDetails& details)
{
    clearDropPreview();
    if (!isInterestedInDragSource(details)) return;
    const auto placement = placementAt(details.localPosition); if (!placement) return;
    std::vector<Workstation::TimelineSource> sources;
    for (const auto& id : draggedSources(details.description))
    {
        const auto found = std::find_if(owner.engine.library.begin(),owner.engine.library.end(),[&](const auto& media) { return media->id == id; });
        if (found == owner.engine.library.end()) return;
        sources.push_back({(*found)->file,*found});
    }
    owner.loadTimelineSources(std::move(sources),placement);
}
void Timeline::paintDropPreview(juce::Graphics& g)
{
    if (!dropPreview) return;
    const auto visible = owner.viewport.getViewArea();
    const int x = (int) std::round(dropPreview->time * pixelsPerSecond), y = laneTop(dropPreview->track);
    const auto lane = juce::Rectangle<int>(visible.getX(), y, visible.getWidth(), laneSize(dropPreview->track));
    g.setColour(colour::orange.withAlpha(0.12f)); g.fillRect(lane);
    g.setColour(colour::orange); g.fillRect(x, y + 2, 2, std::max(1, laneSize(dropPreview->track) - 4));
    const int width = std::min(240, visible.getWidth());
    auto badge = juce::Rectangle<int>(juce::jlimit(visible.getX(), visible.getRight() - width, x + 8), std::max(y + 7, visible.getY() + 4), width, 26);
    g.setColour(colour::panel); g.fillRect(badge); g.setColour(colour::orange); g.drawRect(badge);
    g.setFont(juce::FontOptions(12));
    g.drawText(owner.engine.project.tracks[(size_t) dropPreview->track].name + " / " + juce::String(dropPreview->time, 3) + " s", badge.reduced(7, 0), juce::Justification::centredLeft);
}
void Workstation::fileDragEnter(const juce::StringArray& paths, int x, int y) { fileDragMove(paths, x, y); }
void Workstation::fileDragMove(const juce::StringArray& paths, int x, int y)
{
    if (audioFiles(paths)) timeline.previewDrop(timeline.getLocalPoint(this, juce::Point<int>(x, y))); else timeline.clearDropPreview();
}
void Workstation::fileDragExit(const juce::StringArray&) { timeline.clearDropPreview(); }
void Workstation::filesDropped(const juce::StringArray& paths, int x, int y)
{
    timeline.clearDropPreview();
    const auto placement = timeline.placementAt(timeline.getLocalPoint(this, juce::Point<int>(x, y)));
    juce::Array<juce::File> files;
    for (const auto& path : paths) if (juce::File::isAbsolutePath(path)) files.addIfNotAlreadyThere(juce::File(path));
    if (!placement || !audioFiles(paths)) { importFiles(files); return; }
    std::vector<TimelineSource> sources;
    for (const auto& file : files)
    {
        const auto found = std::find_if(engine.library.begin(), engine.library.end(), [&](const auto& media) { return media->file == file; });
        sources.push_back({file, found == engine.library.end() ? nullptr : *found});
    }
    loadTimelineSources(std::move(sources), placement);
}
void Workstation::addMedia(int index, std::optional<TimelinePlacement> placement)
{
    if (busy || index < 0 || index >= (int) engine.library.size()) return;
    const auto source = engine.library[(size_t) index];
    loadTimelineSources({{source->file, source}}, placement);
}
void Workstation::loadTimelineSources(std::vector<TimelineSource> sources, std::optional<TimelinePlacement> placement)
{
    if (busy || sources.empty() || (placement && (placement->track < 0 || placement->track >= trackCount || !std::isfinite(placement->time) || placement->time < 0))) return;
    std::vector<std::shared_ptr<Media>> media;
    for (const auto& source : sources)
    {
        auto item = source.existing;
        if (!item) { item = std::make_shared<Media>(); item->file = source.file; item->name = source.file.getFileNameWithoutExtension(); item->sourceDetailsPending = true; }
        else if (!item->hasPlayback() && !timelineLoads.count(item->id))
        {
            // A fresh insertion owns a pending source check, not the catalogue's old audio identity.
            // Detach first so adoption cannot change established clips or their Undo snapshots.
            item = item->catalogueCopy(); item->sourceDetailsPending = true;
        }
        media.push_back(std::move(item));
    }
    const bool loading = std::any_of(media.begin(), media.end(), [](const auto& m) { return !m->hasPlayback(); });
    insertTimelineMedia(media, placement);
    for (const auto& item : media) if (!item->hasPlayback()) prepareTimelineAudio(item);
    if (!loading) finishTimelineAudio();
}

void Workstation::insertTimelineMedia(const std::vector<std::shared_ptr<Media>>& loaded, std::optional<TimelinePlacement> placement)
{
    if (loaded.empty()) return;
    const size_t first = engine.project.clips.size();
    remember();
    {
        const juce::ScopedLock guard(engine.lock);
        engine.project.ensureTransitionLinks();
        double time = placement ? placement->time : first > 0 ? engine.project.clips.back().end() : 0;
        const auto preset = selectedTransitionPreset();
        juce::String predecessor; bool pendingOrder = false;
        for (const auto& media : loaded)
        {
            const auto found = std::find_if(engine.library.begin(), engine.library.end(), [&](const auto& item) { return item->id == media->id; });
            if (found == engine.library.end()) engine.library.push_back(media); else *found = media;
            Clip clip; clip.media = media; clip.length = media->duration() > 0 ? media->duration() : 8; clip.start = time;
            clip.pendingPlacement = placement ? PendingPlacement::drop : PendingPlacement::append;
            clip.pendingTransitionPreset = preset;
            clip.hotcuesPending = true;
            clip.awaitingSourceLength = media->sourceDetailsPending;
            if (pendingOrder) clip.pendingPredecessor = predecessor;
            pendingOrder = pendingOrder || media->sourceDetailsPending; predecessor = clip.id;
            clip.track = placement ? placement->track : (int) engine.project.clips.size() % trackCount;
            pendingStemDiscovery.insert(clip.id);
            time = clip.end(); selectedClip = clip.id; engine.project.clips.push_back(std::move(clip));
        }
    }
    selectTrack(engine.project.clips.back().track); refresh();
    setStatus("Track added. Preparing independent audio and beat alignment...");
}

void Workstation::prepareTimelineAudio(const std::shared_ptr<Media>& source)
{
    if (source->hasPlayback() || timelineLoads.count(source->id)) return;
    stopMusicScan(); const auto cancelled = std::make_shared<std::atomic<bool>>(false); timelineLoads[source->id] = cancelled;
    { const juce::ScopedLock guard(engine.lock); source->playbackError.clear(); }
    setStatus("Preparing audio in the background. You can keep arranging tracks.");
    const auto file = source->file; const auto expected = source->analysisSource; const auto displayRecord = source->analysisRecord();
    auto safe = juce::Component::SafePointer<Workstation>(this);
    audioWorkers.addJob([safe, source, file, expected, displayRecord, cancelled]
    {
        if (*cancelled) return;
        juce::AudioFormatManager formats; formats.registerBasicFormats(); auto result = readMedia(formats, file, {}, AnalysisCacheMode::readWrite, MediaReadPurpose::playback, expected.sha256.isEmpty() ? nullptr : &displayRecord, cancelled);
        if (result.media && source->sessionMetadataPending && source->manualBeatOrigin > result.media->duration())
        { result.media.reset(); result.error = "Saved beat marker exceeds source audio: " + file.getFullPathName(); }
        const bool sameSource = result.media && !expected.sha256.isEmpty() && result.media->analysisSource == expected;
        juce::MessageManager::callAsync([safe, source, cancelled, sameSource, result = std::move(result)]() mutable
        {
            if (!safe || *cancelled) return;
            const auto job = safe->timelineLoads.find(source->id); if (job == safe->timelineLoads.end() || job->second != cancelled) return;
            safe->timelineLoads.erase(job);
            if (result.media)
            {
                auto media = result.media;
                if (sameSource || source->analysisSource.sha256.isEmpty()) media->id = source->id;
                if (sameSource || source->sessionMetadataPending) { media->name = source->name; media->manualBpm = source->manualBpm; media->manualBeatOrigin = source->manualBeatOrigin; media->manualKey = source->manualKey; }
                if (safe->selectedLibraryId == source->id) safe->selectedLibraryId = media->id;
                const juce::ScopedLock guard(safe->engine.lock);
                for (auto& item : safe->engine.library)
                    if (item == source) { item = media; safe->markCatalogueChanged(media); }
                auto replace = [&](Project& project)
                {
                    std::map<juce::String, std::pair<double, double>> ends;
                    for (auto& clip : project.clips)
                    {
                        const auto oldEnd = clip.end();
                        if (auto previous = ends.find(clip.pendingPredecessor); previous != ends.end())
                        { if (clip.start == previous->second.first) clip.start = previous->second.second; clip.pendingPredecessor.clear(); }
                        if (clip.media == source)
                        { clip.media = media; if (clip.awaitingSourceLength) { clip.length = media->duration(); clip.awaitingSourceLength = false; } }
                        if (oldEnd != clip.end()) ends[clip.id] = {oldEnd, clip.end()};
                    }
                };
                replace(safe->engine.project); for (auto& project : safe->undoHistory) replace(project); for (auto& project : safe->redoHistory) replace(project);
                if (safe->tempoEditBaseline) replace(*safe->tempoEditBaseline);
            }
            else { const juce::ScopedLock guard(safe->engine.lock); source->playbackError = result.error; }
            safe->refresh(); safe->finishTimelineAudio();
        });
    });
}
void Workstation::finishTimelineAudio()
{
    if (busy || !timelineLoads.empty()) return;
    if (restoreTimelineStems()) return;
    if (engine.playing && engine.project.hasPendingPlacement())
    {
        // Geometry only: preserve the transport, plugins and established voices.
        {
            const juce::ScopedLock guard(engine.lock);
            Project ready = engine.project;
            ready.clips.erase(std::remove_if(ready.clips.begin(), ready.clips.end(), [](const auto& clip)
                { return !clip.prepared && !clip.media->hasPlayback(); }), ready.clips.end());
            auto result = prepareTempoMap(std::move(ready), false, selectedTransitionPreset() == TransitionPreset::alignOnly ? 0 : transitionOverlapBeats(), 0, 0, engine.position.load(), selectedTransitionPreset(), transitionIndex == 5);
            if (result.error.isNotEmpty())
            {
                for (auto& clip : engine.project.clips) if (clip.pendingPlacement != PendingPlacement::none) clip.pendingPlacement = PendingPlacement::failed;
                setStatus("Incoming audio unavailable: " + result.error); return;
            }
            for (auto& clip : engine.project.clips)
            {
                const auto found = std::find_if(result.project.clips.begin(), result.project.clips.end(), [&](const auto& item) { return item.id == clip.id; });
                if (found != result.project.clips.end()) clip = std::move(*found);
                else if (clip.pendingPlacement != PendingPlacement::none) clip.pendingPlacement = PendingPlacement::failed;
            }
            engine.project.tempo = std::move(result.project.tempo); engine.project.bpm = result.project.bpm;
            engine.project.followTrackTempo = result.project.followTrackTempo;
            engine.project.transitionLinks = std::move(result.project.transitionLinks); engine.project.transitionLinksKnown = result.project.transitionLinksKnown;
        }
        dirty = true; refresh();
        auto action = std::move(afterTimelineAudio); afterTimelineAudio = {};
        setStatus(engine.sourceAudioReady() ? "Track ready / synced to the running mix tempo." : "Live mix continues. An incoming source is unavailable; remove or reimport its segment.");
        if (action && engine.playbackReady()) action();
        return;
    }
    if (!engine.sourceAudioReady())
    {
        afterTimelineAudio = {};
        for (const auto& clip : engine.project.clips) if (!clip.media->hasPlayback())
        {
            setStatus("Audio unavailable: " + clip.media->file.getFileName() + ". "
                      + (clip.media->playbackError.isNotEmpty() ? clip.media->playbackError : "Source preparation did not complete."));
            break;
        }
        return;
    }
    if (engine.project.hasPendingPlacement())
    {
        afterTempoPreparation = std::move(afterTimelineAudio); afterTimelineAudio = {};
        prepareTempo(engine.project, false, false); return;
    }
    auto action = std::move(afterTimelineAudio); afterTimelineAudio = {};
    setStatus("Audio ready."); if (action) action();
}
void Workstation::cancelTimelineAudio()
{
    for (const auto& job : timelineLoads) *job.second = true;
    timelineLoads.clear(); afterTimelineAudio = {};
}
}
