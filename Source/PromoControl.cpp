#include "PromoControl.h"
#include "Workstation.h"
#include "AppWindow.h"
#include "GridCorrection.h"
#include "EnergySections.h"
#include <cmath>
#if JUCE_WINDOWS
#include <process.h>
#else
#include <unistd.h>
#endif

namespace mixworks
{
namespace
{
int processId()
{
   #if JUCE_WINDOWS
    return _getpid();
   #else
    return (int)getpid();
   #endif
}
juce::var object() { return juce::var(new juce::DynamicObject()); }
void put(juce::var& o, const char* key, const juce::var& v) { o.getDynamicObject()->setProperty(key, v); }
bool number(const juce::var& v, double low, double high)
{ return (v.isInt() || v.isInt64() || v.isDouble()) && std::isfinite((double) v) && (double) v >= low && (double) v <= high; }
bool plainName(const juce::String& s)
{ return s.isNotEmpty() && s.length() <= 96 && s.containsOnly("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_"); }
bool noLinks(juce::File f)
{
    for (;;) { if (f.isSymbolicLink()) return false; const auto parent = f.getParentDirectory(); if (parent == f) return true; f = parent; }
}
bool atomicJson(const juce::File& file, const juce::var& value)
{
    juce::TemporaryFile temporary(file);
    return temporary.getFile().replaceWithText(juce::JSON::toString(value), false, false, "\n") && temporary.overwriteTargetFileWithTemporary();
}
}
juce::Result PromoControl::prepare(const juce::File& root)
{
    if (!root.isDirectory() || !noLinks(root) || root.getChildFile(".alloy-promo").loadFileAsString().trim() != "alloy-promo-v1")
        return juce::Result::fail("Promo mode requires an existing owned folder with .alloy-promo = alloy-promo-v1, without links.");
    const auto key = root.getChildFile("control/token.txt");
    if (!noLinks(key) || key.getSize() > 128 || key.loadFileAsString().trim().length() < 32)
        return juce::Result::fail("Promo mode requires a private control token of 32-128 characters.");
    for (const auto* name : {"data", "control/requests", "control/responses", "control/history", "audio", "sessions", "evidence"})
    {
        const auto folder = root.getChildFile(name);
        if (!noLinks(folder) || folder.createDirectory().failed()) return juce::Result::fail("Cannot create a link-free promo folder.");
    }
    Engine::useTestMidiProfile(root.getChildFile("data/midi-mappings.json"));
    PcmStore::shared(root.getChildFile("data/AudioCache-v1"));
    analysisStoreRoot(root.getChildFile("data/Analysis-v1"));
    return juce::Result::ok();
}
PromoControl::PromoControl(Workstation& w, juce::File directory, bool poll)
    : owner(w), root(std::move(directory)), token(root.getChildFile("control/token.txt").loadFileAsString().trim())
{
    // Workstation(false) never loads or writes production workspace/library/recovery.
    owner.musicFolder = root.getChildFile("audio");
    owner.musicPreference = root.getChildFile("data/library-scan.json");
    owner.workspacePreference = root.getChildFile("data/workspace.json");
    owner.catalogueFile = root.getChildFile("data/library.json");
    owner.layout.saveFolder = root.getChildFile("sessions").getFullPathName();
    owner.hints.setHintsEnabled(false);
    owner.setStatus("LOCAL BRIDGE / isolated session / MIDI input disabled");
    if (poll) startTimer(40);
}
PromoControl::~PromoControl() { stopTimer(); }
bool PromoControl::contained(const juce::File& file) const { return file.isAChildOf(root) && noLinks(file); }
bool PromoControl::safeDocument(const juce::var& v) const
{
    if (v.isString()) { const auto s = v.toString(); return !juce::File::isAbsolutePath(s) || contained(juce::File(s)); }
    if (const auto* values = v.getArray()) for (const auto& item : *values) if (!safeDocument(item)) return false;
    if (const auto* o = v.getDynamicObject()) for (const auto& item : o->getProperties()) if (!safeDocument(item.value)) return false;
    return true;
}
juce::var PromoControl::state() const
{
    const juce::ScopedLock guard(owner.engine.lock);
    auto o = object(); put(o, "version", JUCE_APPLICATION_VERSION_STRING); put(o, "promo", true);
    put(o,"protocolVersion",1); put(o,"root",root.getFullPathName()); put(o,"pid",processId()); put(o,"dirty",owner.dirty);
    juce::Array<juce::var> capabilities;
    for (const auto* name : {"status","import","add","correct_source","transition_preset","play","stop","seek","view","panel","select_clip","select_transition","control","save","open","screenshot","undo","close","timing","audio_probe","jump_bars","move_clip","transition_edit","insertion_preset","preview","work_mode","timeline_cue","transition_confirm"}) capabilities.add(name);
    put(o,"capabilities",capabilities);
    put(o,"insertionPreset",owner.transitionPreset.getSelectedId()); put(o,"sectionInsertion",owner.transitionIndex==5);
    put(o, "playing", owner.engine.playing.load()); put(o, "position", owner.engine.position.load());
    put(o,"workMode",owner.engine.workMode==WorkMode::arrange ? "arrange" : "record-perform");
    put(o,"capturingTake",owner.engine.capturingPerformance());put(o,"takeCount",(int)owner.engine.project.performanceTakes.size());
    if(auto* question=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());question && question->getName()=="Replace transition curves?")
    {auto q=object();put(q,"incoming",owner.selectedTransition);juce::StringArray text;for(auto* child:question->getChildren())if(auto* label=dynamic_cast<juce::Label*>(child))text.add(label->getText());put(q,"message",text.joinIntoString("\n"));put(o,"transitionQuestion",q);}
    put(o,"seekPending",owner.engine.seekPending.load()); put(o,"seekError",owner.engine.seekError);
    put(o, "ready", owner.engine.playbackReady()); put(o, "busy", owner.busy || owner.engine.seekPending || !owner.timelineLoads.empty() || owner.tempoPreparationPending || owner.engine.exporting || juce::Component::getCurrentlyModalComponent()!=nullptr);
    put(o, "status", owner.status); put(o, "selectedClip", owner.selectedClip); put(o, "selectedTransition", owner.selectedTransition);
    put(o,"saveFolder",owner.defaultSaveFolder().getFullPathName());
    put(o, "width", owner.getWidth()); put(o, "height", owner.getHeight()); put(o, "peak", owner.engine.masterPeak.load());
    put(o, "timestamp", (double) juce::Time::currentTimeMillis());
    put(o,"monitorMuted",owner.engine.monitorMuted.load()); put(o,"audibleEnd",owner.engine.project.audibleEnd());
    auto preview=object();put(preview,"sourceId",owner.libraryPreview.sourceId());put(preview,"loading",owner.libraryPreview.isLoading());
    put(preview,"fraction",owner.libraryPreview.positionFraction());put(preview,"duration",owner.libraryPreview.lengthSeconds());put(preview,"privateOutput",owner.libraryPreview.usesPrivateOutput());put(o,"preview",preview);
    const auto streaming=OriginalAudio::statistics(); auto stream=object();
    put(stream,"poolBytes",(juce::int64)streaming.bytes); put(stream,"blockReads",(juce::int64)streaming.reads);
    put(stream,"callbackSampleMisses",(juce::int64)streaming.misses); put(stream,"buffering",owner.engine.streamBuffering.load());
    put(stream,"waitCallbacks",(juce::int64)owner.engine.streamWaitCallbacks.load()); put(o,"originalAudio",stream);
    const auto callbacks=owner.engine.callbackDiagnostics.snapshot(); auto audio=object();
    put(audio,"callbacks",(juce::int64)callbacks.callbacks); put(audio,"overBudget",(juce::int64)callbacks.overBudget);
    put(audio,"lockMisses",(juce::int64)callbacks.lockMisses); put(audio,"oversized",(juce::int64)callbacks.oversized);
    put(audio,"exportSilence",(juce::int64)callbacks.exportSilence);
    const auto ticksPerMs=juce::Time::getHighResolutionTicksPerSecond()/1000.;
    put(audio,"totalMs",callbacks.totalTicks/ticksPerMs); put(audio,"peakMs",callbacks.peakTicks/ticksPerMs);
    put(o,"audioCallbacks",audio);
    if(owner.sceneLane)
    {
        const auto& lane=*owner.sceneLane; const auto& timing=lane.getProperties();
        auto visual=object(); put(visual,"validFrame",lane.preview.isValid() || (bool)lane.previewFacts["presented"]); put(visual,"status",lane.previewStatus);
        put(visual,"frame",lane.previewFacts.clone()); put(visual,"paintedFrames",timing.getWithDefault("visualPaintCount",0));
        put(visual,"lastPaintMs",timing.getWithDefault("visualPaintTime",0.)); put(visual,"maxGapMs",timing.getWithDefault("visualPaintMaxGap",0.));
        put(visual,"gapHistogramCeilMs",timing["visualGapHistogramCeilMs"].clone()); put(o,"visuals",visual);
        if (lane.previewFacts["presentation"].toString()=="shared-gpu-texture")
        {
            put(visual,"paintedFrames",lane.previewFacts["presentedFrames"]);
            put(visual,"maxGapMs",lane.previewFacts["maxGapMs"]);
            put(visual,"gapHistogramCeilMs",lane.previewFacts["gapHistogramCeilMs"].clone());
            put(visual,"measurement","GPU window swaps; not monitor scanout");
        }
    }
    juce::Array<juce::var> media, clips;
    for (const auto& m : owner.engine.library) { auto v = object(); put(v,"id",m->id); put(v,"name",m->name); put(v,"bpm",m->bpm()); media.add(v); }
    for (const auto& c : owner.engine.project.clips) { auto v = object(); put(v,"id",c.id); put(v,"name",c.media->name); put(v,"start",c.start); put(v,"end",c.end()); put(v,"lane",c.track); clips.add(v); }
    juce::Array<juce::var> lanes;
    for (const auto& track : owner.engine.project.tracks) { auto v=object(); put(v,"volume",track.manualValue(AutomationTarget::volume)); put(v,"pan",track.manualValue(AutomationTarget::pan)); put(v,"cutoff",track.manualValue(AutomationTarget::cutoff)); lanes.add(v); }
    put(o,"lanes",lanes); put(o,"library",media); put(o,"clips",clips); return o;
}
juce::var PromoControl::execute(const juce::var& request)
{
    auto result = object(); const auto id = request["id"].toString(), command = request["command"].toString();
    put(result,"id",id); put(result,"ok",false);
    auto fail = [&](const juce::String& text) { put(result,"error",text); return result; };
    if (!request.isObject() || !plainName(id) || token.length() < 32 || request["token"].toString() != token) return fail("Unauthorised or invalid request.");
    if (!number(request["timestamp"], juce::Time::currentTimeMillis() - 60000.0, juce::Time::currentTimeMillis() + 5000.0)) return fail("Expired request.");
    if (command == "status") { put(result,"ok",true); put(result,"state",state()); return result; }
    if(command=="transition_confirm")
    {
        auto* question=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        if(!request["replace"].isBool() || !question || question->getName()!="Replace transition curves?"
            || owner.selectedTransition.isEmpty() || request["incoming"].toString()!=owner.selectedTransition)
            return fail("Inspect the matching transition replacement question before answering it.");
        question->exitModalState((bool)request["replace"] ? 1:0);
        put(result,"ok",true);put(result,"state",state());return result;
    }
    if (owner.busy || !owner.timelineLoads.empty() || owner.tempoPreparationPending || owner.engine.exporting || juce::Component::getCurrentlyModalComponent()) return fail("App is busy; inspect status and retry with a new request ID.");
    const auto path = request["path"].toString();
    if (command == "import")
    {
        if (!juce::File::isAbsolutePath(path)) return fail("Expected an absolute demo audio path.");
        const juce::File file(path);
        if (!contained(file) || !file.isAChildOf(root.getChildFile("audio")) || !file.existsAsFile() || !file.hasFileExtension("wav;flac;aif;aiff;mp3;ogg")) return fail("Audio must be an existing file inside promo/audio.");
        owner.importFiles({file});
    }
    else if (command == "add")
    {
        if (!request["index"].isInt() || !number(request["index"],0,(double) owner.engine.library.size()-1)) return fail("Invalid library index.");
        owner.addMedia((int) request["index"]);
    }
    else if (command == "correct_source")
    {
        if (!request["index"].isInt() || !number(request["index"],0,(double)owner.engine.library.size()-1)
            || !number(request["bpm"],40,300) || !number(request["downbeat"],0,10800)) return fail("Invalid source correction.");
        const auto source=owner.engine.library[(size_t)(int)request["index"]];
        const auto correction=owner.engine.correctSource(source,{(double)request["bpm"],(double)request["downbeat"],-1});
        if (correction.failed()) return fail(correction.getErrorMessage());
    }
    else if (command == "transition_preset")
    {
        if (owner.selectedTransition.isEmpty() || !request["index"].isInt() || !number(request["index"],1,3)) return fail("Select a transition and built-in preset 1-3.");
        owner.transitionPresetForSelection.setSelectedId((int)request["index"],juce::sendNotificationSync);
    }
    else if (command == "insertion_preset")
    {
        if (!request["index"].isInt() || !number(request["index"],1,3)) return fail("Invalid insertion preset.");
        owner.transitionPreset.setSelectedId((int)request["index"],juce::sendNotificationSync);
    }
    else if (command == "work_mode")
    {
        const auto name=request["name"].toString();
        if(name!="arrange" && name!="record-perform")return fail("Choose arrange or record-perform.");
        if(owner.engine.playing || owner.engine.capturingPerformance())return fail("Stop and finalise the owned take before changing work mode.");
        // Select the real mode menu, including its route, take and workspace side effects.
        owner.workModeButton.onClick();
        auto* menu=juce::Component::getCurrentlyModalComponent();
        if(!menu)return fail("Work-mode menu did not open; inspect the actual state.");
        menu->exitModalState(name=="arrange" ? 1:2);
    }
    else if(command=="timeline_cue")
    {
        if(!request["slot"].isInt() || !number(request["slot"],0,8))return fail("Choose timeline cue slot 0-8.");
        owner.locateHotcue((int)request["slot"]);
    }
    else if (command == "play")
    {
        if (!request["enabled"].isBool() || ((bool) request["enabled"] && (!owner.engine.playbackReady() || owner.engine.project.clips.empty()))) return fail("Playback is not ready or enabled is missing.");
        owner.engine.setPlaying((bool) request["enabled"]);
    }
    else if (command == "stop") owner.engine.stop();
    else if (command == "seek")
    {
        if (!number(request["seconds"],0,owner.engine.project.audibleEnd())) return fail("Seek is outside the mix.");
        const auto seek = owner.engine.seek((double) request["seconds"]); if (seek.failed()) return fail(seek.getErrorMessage());
    }
    else if (command == "view")
    {
        if (!number(request["start"],0,10800) || !number(request["seconds"],2,10800)) return fail("Invalid timeline view.");
        if(request.hasProperty("monitorMuted") && !request["monitorMuted"].isBool())return fail("Monitor mute must be an explicit boolean.");
        if(request["monitorMuted"].isBool())
        {owner.monitorMute.setToggleState((bool)request["monitorMuted"],juce::dontSendNotification);owner.monitorMute.onClick();}
        owner.setFollowPlayhead(false);
        const juce::ScopedValueSetter<bool> guard(owner.programmaticTimelineView,true);
        owner.timeline.zoomAt((owner.viewport.getViewWidth() / (double) request["seconds"]) / owner.timeline.pixelsPerSecond,0);
        owner.viewport.setViewPosition((int) ((double) request["start"] * owner.timeline.pixelsPerSecond),0);
    }
    else if (command == "panel")
    {
        const auto name = request["name"].toString();
        if (name != "controller" && name != "transitions" && name != "editing") return fail("Unknown panel.");
        owner.showToolPage(name == "controller" ? 0 : name == "transitions" ? 1 : 2);
    }
    else if (command == "select_clip" || command == "select_transition")
    {
        if (!request["index"].isInt() || !number(request["index"],0,(double) owner.engine.project.clips.size()-1)) return fail("Invalid clip index.");
        const auto& c = owner.engine.project.clips[(size_t)(int)request["index"]];
        if (command == "select_transition") owner.selectTransition(c.id,true);
        else { owner.selectedClip = c.id; owner.selectTrack(c.track); }
    }
    else if (command == "timing")
    {
        if (!request["index"].isInt() || !number(request["index"],0,(double)owner.engine.project.clips.size()-1)) return fail("Invalid clip index.");
        const auto& clip = owner.engine.project.clips[(size_t)(int)request["index"]];
        if (clip.reversed || clip.loopSpan > 0 || !number(request["source_seconds"],clip.offset,clip.media->duration())) return fail("Timing probe needs a forward source position.");
        const double source = (double)request["source_seconds"], at = clip.start + clip.audioTimelineTime(source - clip.offset);
        const auto clock = owner.engine.project.gridClock(); auto data = object();
        put(data,"sourceSeconds",source); put(data,"projectSeconds",at); put(data,"projectBeat",clock.beatsAt(at));
        put(data,"cursorSourceSeconds",clip.offset + clip.audioSourceElapsed(owner.engine.position.load()-clip.start));
        put(data,"bpm",clip.media->bpm()); put(data,"beatOrigin",clip.media->beatOrigin()); put(data,"downbeatOrigin",clip.media->downbeatOrigin());
        put(data,"clipStartBeat",clock.beatsAt(clip.start)); put(data,"pixelsPerSecond",owner.timeline.pixelsPerSecond);
        put(data,"waveformPixel",Clip::gridPixel(at,owner.timeline.pixelsPerSecond));
        juce::Array<juce::var> sections, transitions;
        for (const auto& section : detectEnergySections(*clip.media))
        { auto item=object(); put(item,"sourceSeconds",section.sourceSeconds); put(item,"projectSeconds",clip.start+clip.audioTimelineTime(section.sourceSeconds-clip.offset)); sections.add(item); }
        for (const auto& link : owner.engine.project.transitionLinks) if (link.incoming==clip.id || link.outgoing==clip.id)
        {
            const auto range=owner.engine.project.transitionRange(link); auto item=object();
            put(item,"incoming",link.incoming); put(item,"begin",range.getStart()); put(item,"end",range.getEnd());
            if (const auto swap=owner.engine.project.transitionSwapTimes(link,1)) { put(item,"outgoingBassSwap",(*swap)[0]); put(item,"incomingBassSwap",(*swap)[1]); }
            transitions.add(item);
        }
        put(data,"sections",sections); put(data,"transitions",transitions); put(result,"timing",data);
    }
    else if (command == "audio_probe")
    {
        if (owner.engine.playing || !request["frames"].isInt() || !number(request["frames"],1,8192)
            || !number(request["seconds"],0,owner.engine.project.audibleEnd())
            || (!request["lane"].isVoid() && (!request["lane"].isInt() || !number(request["lane"],0,trackCount-1)))) return fail("Audio probe needs stopped transport, a valid time and 1-8192 frames.");
        juce::String error; auto renderer=owner.engine.createExportRenderer(error); if (!renderer) return fail(error);
        if (!request["lane"].isVoid()) for (int i=0;i<trackCount;++i)
        { renderer->project.tracks[(size_t)i].mute=i!=(int)request["lane"]; renderer->project.tracks[(size_t)i].solo=false; }
        const int frames=(int)request["frames"], block=std::max(1,std::min(512,renderer->preparedBlockSize()));
        const double rate=renderer->sampleRate.load(), at=(double)request["seconds"];
        juce::AudioBuffer<float> buffer(2,block); juce::MidiBuffer midi; juce::Array<juce::var> left,right;
        for (int offset=0;offset<frames;offset+=block)
        {
            const int count=std::min(block,frames-offset); midi.clear(); renderer->render(buffer,count,at+offset/rate,midi);
            for (int i=0;i<count;++i) { left.add(buffer.getSample(0,i)); right.add(buffer.getSample(1,i)); }
        }
        auto data=object(); put(data,"seconds",at); put(data,"sampleRate",rate); put(data,"frames",frames); put(data,"left",left); put(data,"right",right);
        put(result,"audio",data);
    }
    else if (command == "jump_bars")
    {
        const int bars=(int)request["bars"], direction=(int)request["direction"];
        if (!request["bars"].isInt() || !juce::Array<int>{4,8,16,32}.contains(bars) || !request["direction"].isInt() || (direction!=-1 && direction!=1)) return fail("Invalid bar jump.");
        owner.jumpBars(bars,direction);
    }
    else if (command == "move_clip" || command == "transition_edit")
    {
        if (owner.engine.playing || !request["snap"].isBool()) return fail("Geometry probe needs stopped transport and explicit snap.");
        auto candidate=owner.engine.project; juce::Result changed=juce::Result::fail("Invalid geometry command.");
        if (command=="move_clip")
        {
            if (!request["index"].isInt() || !number(request["index"],0,(double)candidate.clips.size()-1) || !number(request["seconds"],0,10800)
                || (request["mode"]!="body" && request["mode"]!="following")) return fail("Invalid clip movement.");
            const auto id=candidate.clips[(size_t)(int)request["index"]].id;
            changed=request["mode"]=="body" ? candidate.moveClip(id,(double)request["seconds"],(bool)request["snap"])
                : candidate.moveClipFollowing(id,(double)request["seconds"],(bool)request["snap"]);
        }
        else
        {
            const juce::StringArray actions{"window","start","end","outgoing","incoming"}; const int action=actions.indexOf(request["action"].toString());
            if (action<0 || !number(request["delta"],-3600,3600) || !candidate.transitionFor(owner.selectedTransition)) return fail("Invalid transition movement.");
            const std::array<Project::TransitionEdit,5> edits{Project::TransitionEdit::moveWindow,Project::TransitionEdit::resizeStart,Project::TransitionEdit::resizeEnd,Project::TransitionEdit::moveOutgoing,Project::TransitionEdit::moveIncoming};
            changed=candidate.editTransition(owner.selectedTransition,edits[(size_t)action],(double)request["delta"],(bool)request["snap"]);
        }
        if (changed.failed()) return fail(changed.getErrorMessage());
        if (juce::JSON::toString(projectToJson(candidate))!=juce::JSON::toString(projectToJson(owner.engine.project)))
        { owner.engine.finishAutomation(); owner.remember(); const juce::ScopedLock guard(owner.engine.lock); owner.engine.project=std::move(candidate); }
    }
    else if(command=="preview")
    {
        if(request["stop"].isBool() && (bool)request["stop"])owner.libraryPreview.stop();
        else
        {
            if(!request["index"].isInt() || !number(request["index"],0,(double)owner.engine.library.size()-1) || !number(request["fraction"],0,1))return fail("Preview needs a library index and fraction from 0 to 1, or stop:true.");
            const auto& media=owner.engine.library[(size_t)(int)request["index"]];
            if(!media || !contained(media->file) || !media->file.isAChildOf(root.getChildFile("audio")))return fail("Preview requires an owned audio fixture.");
            owner.previewLibraryTrack(media->id,(double)request["fraction"]);
        }
    }
    else if (command == "control")
    {
        if (!request["lane"].isInt() || !number(request["lane"],0,trackCount-1)) return fail("Invalid lane.");
        const auto name = request["parameter"].toString();
        const auto target = name == "volume" ? AutomationTarget::volume : name == "pan" ? AutomationTarget::pan : AutomationTarget::cutoff;
        if (name != "volume" && name != "pan" && name != "cutoff") return fail("Unsupported control.");
        const auto range = automationRange(target);
        if (!number(request["value"],range.getStart(),range.getEnd())) return fail("Control value outside range.");
        owner.remember();
        if (!owner.engine.setControlValue({(int)request["lane"],target,{}, {}, {}},(float)request["value"])) return fail("Control rejected.");
    }
    else if (command == "save" || command == "open" || command == "screenshot")
    {
        const auto name = request["name"].toString(); if (!plainName(name)) return fail("Use a simple output name.");
        const auto file = root.getChildFile(command == "screenshot" ? "evidence" : "sessions").getChildFile(name + (command == "screenshot" ? ".png" : ".mixworks"));
        if (!contained(file)) return fail("Unsafe output path.");
        if (command == "save")
        {
            if (file.exists()) return fail("Choose a new name; saved demo sessions are preserved.");
            const auto error = owner.engine.saveProject(file); if (error.isNotEmpty()) return fail(error);
            owner.sessionFile = file; owner.dirty = false;
        }
        else if (command == "open")
        {
            if (owner.dirty) return fail("Save the current demo session before replacing it.");
            if (!file.existsAsFile() || file.getSize() > 32*1024*1024 || !safeDocument(juce::JSON::parse(file))) return fail("Session is missing, too large or references files outside the promo folder.");
            owner.openProjectFile(file);
        }
        else
        {
            if (file.exists()) return fail("Choose a new screenshot name.");
            juce::FileOutputStream output(file); juce::PNGImageFormat png;
            if (!output.openedOk() || !png.writeImageToStream(owner.createComponentSnapshot(owner.getLocalBounds()),output)) return fail("Screenshot failed.");
        }
        put(result,"path",file.getFullPathName());
    }
    else if (command == "undo") owner.undo(false);
    else if (command == "close")
    {
        if (owner.dirty) return fail("Save the isolated session before closing it.");
        owner.engine.stop();
        juce::MessageManager::callAsync([] { if (auto* app=juce::JUCEApplicationBase::getInstance()) app->quit(); });
    }
    else return fail("Unknown command.");
    owner.refresh(); put(result,"ok",true); put(result,"state",state()); return result;
}
void PromoControl::timerCallback()
{
    const auto requests = root.getChildFile("control/requests"), responses = root.getChildFile("control/responses"), history=root.getChildFile("control/history");
    if (!contained(requests) || !contained(responses) || !contained(history)) { stopTimer(); return; }
    auto archive=[&](const juce::File& file) { const auto destination=history.getChildFile(file.getFileName()).getNonexistentSibling(); if(contained(destination)) file.moveFileTo(destination); };
    for (const auto& file : requests.findChildFiles(juce::File::findFiles,false,"*.json"))
    {
        const auto id = file.getFileNameWithoutExtension(); if (!plainName(id) || !contained(file)) continue;
        const auto reply = responses.getChildFile(id+".json"); if (!contained(reply)) continue;
        if (reply.exists()) { archive(file); break; }
        if (file.getSize() > 65536) { auto error=object(); put(error,"id",id); put(error,"ok",false); put(error,"error","Request exceeds 64 KiB."); if(atomicJson(reply,error)) archive(file); break; }
        auto request=juce::JSON::parse(file);
        if (request["id"].toString()!=id) { auto error=object(); put(error,"id",id); put(error,"ok",false); put(error,"error","Invalid JSON or mismatched request ID."); if(atomicJson(reply,error)) archive(file); break; }
        // A durable claim prevents duplicate execution after a crash. Uncertain claims require a new ID and state inspection.
        const auto claim=responses.getChildFile(id+".claimed"); if (!contained(claim) || claim.exists()) continue;
        if (!claim.replaceWithText("claimed\n")) continue;
        if (atomicJson(reply,execute(request))) archive(file);
        break; // Bound message-thread work; preserve UI/audio responsiveness.
    }
}
void runPromoControlTests(const std::function<void(bool,const juce::String&)>& check,const juce::File& output)
{
    const auto root=output.getParentDirectory().getChildFile("promo-fixture"); root.createDirectory();
    root.getChildFile("control").createDirectory(); root.getChildFile("control/token.txt").replaceWithText(juce::String::repeatedString("a",64));
    auto w=std::make_unique<Workstation>(false); PromoControl bridge(*w,root,false);
    int id=0; auto call=[&](const char* command,const char* key=nullptr,juce::var value={}) { auto q=object(); put(q,"id","test-"+juce::String(++id)); put(q,"token",juce::String::repeatedString("a",64)); put(q,"timestamp",(double)juce::Time::currentTimeMillis()); put(q,"command",command); if(key)put(q,key,value); return bridge.execute(q); };
    check((bool)call("status")["ok"],"promo status returns actual state");
    check(juce::File(call("status")["state"]["saveFolder"].toString()).isAChildOf(root),"normal save/export choosers default inside the isolated bridge root");
    {
        std::function<SceneLane*(juce::Component&)> findLane = [&](juce::Component& component)->SceneLane*
        {
            if (auto* lane = dynamic_cast<SceneLane*>(&component)) return lane;
            for (auto* child : component.getChildren()) if (auto* lane = findLane(*child)) return lane;
            return nullptr;
        };
        auto* diagnosticLane = findLane(*w);
        check(diagnosticLane!=nullptr,"visual diagnostics use the actual scene component");
        auto gaps=object(); put(gaps,"17",2);
        if(diagnosticLane) diagnosticLane->getProperties().set("visualGapHistogramCeilMs",gaps);
        const auto snapshot=call("status")["state"]["visuals"];
        put(gaps,"17",3);
        check(snapshot["gapHistogramCeilMs"]["17"]==juce::var(2) && snapshot["paintedFrames"].isInt(),
            "read-only visual diagnostics copy cumulative timing facts instead of sharing mutable state");
    }
    check((int)call("status")["state"]["protocolVersion"]==1,"bridge advertises protocol version");
    check(call("status")["state"]["seekPending"].isBool(),"bridge exposes actual pending seek state");
    check(call("status")["state"]["seekError"].isString(),"bridge exposes the last seek preparation result");
    check(call("status")["state"]["capabilities"].getArray()->contains("close"),"bridge advertises graceful close");
    check(!(bool)bridge.execute(object())["ok"],"promo rejects unauthenticated request");
    check(!(bool)call("shell")["ok"],"promo does not execute arbitrary commands");
    check(!(bool)call("play","enabled",true)["ok"],"promo rejects playback of empty mix");
    check(!(bool)call("seek","seconds",-1)["ok"],"promo rejects negative seek");
    check(!(bool)call("seek","seconds",100)["ok"],"promo rejects seek past end");
    check(!(bool)call("import","path",output.getFullPathName())["ok"],"promo rejects external imports");
    check(!(bool)call("import","path","relative.wav")["ok"],"promo rejects relative imports");
    check(!(bool)call("save","name","../escape")["ok"],"promo rejects output traversal");
    check(!(bool)call("add","index",-1)["ok"],"promo rejects absent library index");
    check(!(bool)call("panel","name","unknown")["ok"],"promo rejects unknown panels");
    check((bool)call("panel","name","controller")["ok"],"promo selects real controller panel");
    check((bool)call("stop")["ok"],"promo transport stop is idempotent");
    check(!(bool)call("timing","index",0)["ok"],"bridge timing rejects absent clip and missing source time");
    check(!(bool)call("audio_probe","frames",8193)["ok"],"bridge bounds offline PCM inspection");
    check(!(bool)call("jump_bars","bars",3)["ok"],"bridge bar jump rejects unsupported steps");
    check(!(bool)call("move_clip","seconds",10)["ok"],"bridge movement requires explicit target and snapping");
    check(!(bool)call("transition_edit","action","window")["ok"],"bridge transition edits require valid selected ownership");
    check(!(bool)call("insertion_preset","index",4)["ok"],"bridge insertion preset rejects unknown choices");
    const auto insertion=call("insertion_preset","index",2);
    check((bool)insertion["ok"] && (int)insertion["state"]["insertionPreset"]==2 && (bool)insertion["state"]["sectionInsertion"],
          "bridge sets the real new-track Bass swap picker while retaining automatic sections");
    auto source=std::make_shared<Media>(); source->sampleRate=8000; source->samples.setSize(1,96000); source->samples.clear();
    source->samples.setSample(0,4000,.9f); source->manualBpm=120; source->manualBeatOrigin=.5;
    Clip clip; clip.media=source; clip.length=12; w->engine.project.clips={clip}; w->refresh();
    auto query=[&](const char* command,std::initializer_list<std::pair<const char*,juce::var>> fields)
    { auto q=object();put(q,"id","test-"+juce::String(++id));put(q,"token",juce::String::repeatedString("a",64));put(q,"timestamp",(double)juce::Time::currentTimeMillis());put(q,"command",command);for(const auto& x:fields)put(q,x.first,x.second);return bridge.execute(q); };
    const auto muted=query("view",{{"start",0},{"seconds",40},{"monitorMuted",true}});
    check((bool)muted["ok"] && (bool)muted["state"]["monitorMuted"],"bridge uses the actual monitor-mute control in its isolated app");
    check(!(bool)query("view",{{"start",0},{"seconds",40},{"monitorMuted",1}})["ok"],"bridge monitor mute rejects ambiguous numeric values");
    check(!(bool)query("preview",{{"index",-1},{"fraction",.5}})["ok"],"bridge preview rejects missing or invalid source ownership");
    const auto stopped=query("preview",{{"stop",true}});
    check((bool)stopped["ok"] && stopped["state"]["preview"]["sourceId"].toString().isEmpty(),"bridge preview Stop is explicit and exposes actual idle state");
    const auto timed=query("timing",{{"index",0},{"source_seconds",.5}});
    check((bool)timed["ok"] && std::abs((double)timed["timing"]["projectSeconds"]-.5)<1e-9,"bridge projects an actual source event without editing it");
    const auto probed=query("audio_probe",{{"seconds",.49},{"frames",1024}});
    const auto* audio=probed["audio"]["left"].getArray(); float peak=0;
    if(audio)for(const auto& sample:*audio)peak=std::max(peak,(float)sample);
    check((bool)probed["ok"] && audio && audio->size()==1024 && peak>.2f && w->engine.position==0,
          "bridge PCM probe uses a separate actual export renderer and retains live position");
    const auto jumped=query("jump_bars",{{"bars",4},{"direction",1}});
    check((bool)jumped["ok"] && w->engine.position==8,"bridge bar jump invokes the native navigation owner");
    check(!(bool)query("timeline_cue",{{"slot",9}})["ok"] && w->engine.position==8,"bridge cue rejects invalid slots without seeking");
    w->engine.project.putHotcue(clip.id,0,.5);w->selectedClip=clip.id;w->selectTrack(0);
    const auto cue=query("timeline_cue",{{"slot",0}});
    check((bool)cue["ok"] && w->engine.position==.5 && !w->engine.playing,"bridge timeline cue invokes the actual whole-mix navigation owner");
    check(!(bool)query("work_mode",{{"name","invalid"}})["ok"],"bridge work mode rejects unknown choices");
    check(!(bool)query("transition_confirm",{{"replace",true},{"incoming",clip.id}})["ok"],"bridge cannot confirm an absent or unrelated dialog");
    w->engine.playing=true;
    check(!(bool)query("work_mode",{{"name","record-perform"}})["ok"] && w->engine.workMode==WorkMode::arrange,"bridge cannot switch mode beneath running programme audio");w->engine.playing=false;
}
}
