#include "Workstation.h"
#include <cmath>
#include <algorithm>
#include <charconv>
#include <juce_cryptography/juce_cryptography.h>
namespace mixworks
{
namespace
{
int takeIndex(const Project& project,const juce::String& id)
{
    const auto found=std::find_if(project.performanceTakes.begin(),project.performanceTakes.end(),[&](const auto& take) { return take.id==id; });
    return found==project.performanceTakes.end() ? -1 : (int)(found-project.performanceTakes.begin());
}
juce::String takeFingerprint(const PerformanceTake& take)
{
    // Dialogs can outlive Undo or another edit of the same take ID. Keep a digest
    // rather than retaining another full set of structural project snapshots.
    const auto json=juce::JSON::toString(takeToJson(take)); return juce::SHA256(json.toUTF8()).toHexString();
}
const juce::Identifier privateRequestKey("alloyPrivatePreparationRequest");
juce::String privateRequest(juce::Component& owner) { return owner.getProperties()[privateRequestKey].toString(); }
juce::String beginPrivateRequest(juce::Component& owner)
{
    // UI-only cancellation identity; not a saved project or engine format field.
    const auto id=juce::Uuid().toString(); owner.getProperties().set(privateRequestKey,id); return id;
}
juce::String privateFingerprint(PrivatePreparation& preparation)
{
    const auto* renderer=preparation.privateEngine(); if (!renderer) return "empty";
    const auto json=juce::JSON::toString(projectToJson(renderer->project));
    return juce::String::toHexString((juce::int64)(uintptr_t)renderer)+":"+preparation.target()+":"+juce::SHA256(json.toUTF8()).toHexString();
}
class TakeSeekPreparation final : public juce::Thread, private juce::Timer
{
public:
    TakeSeekPreparation(std::unique_ptr<Engine> e,double seconds,juce::Component* parent,std::function<void(std::unique_ptr<Engine>)> result)
        : Thread("Take seek preparation"),renderer(std::move(e)),target((int64_t)std::llround(seconds*renderer->sampleRate.load())),done(std::move(result)),owner(parent),dialog("Reconstructing performance history",{},juce::MessageBoxIconType::NoIcon,parent)
    {
        dialog.addProgressBarComponent(progress); dialog.addButton("Cancel",0,juce::KeyPress(juce::KeyPress::escapeKey));
    }
    ~TakeSeekPreparation() override { stopTimer(); signalThreadShouldExit(); stopThread(-1); }
    void launch()
    {
        dialog.enterModalState(true);
        if (!startThread(juce::Thread::Priority::low)) { dialog.exitModalState(0); if (done) done(nullptr); delete this; return; }
        startTimer(100);
    }
    void run() override
    {
        const int block=renderer->preparedBlockSize(); juce::AudioBuffer<float> pcm(2,block);
        for (int64_t frame=0;frame<target && !threadShouldExit();)
        { const int count=(int)std::min<int64_t>(block,target-frame); renderer->renderTake(pcm,count,frame/renderer->sampleRate.load()); frame+=count; completed=frame; }
    }
private:
    void timerCallback() override
    {
        // Observe dismissal before closing the dialog ourselves. Window close,
        // Escape and Cancel are cancellation even if the worker already finished.
        if (!owner || !dialog.isCurrentlyModal(false)) { cancelled=true; signalThreadShouldExit(); }
        progress=target>0 ? completed.load()/(double)target : 1;
        if (isThreadRunning()) return; // Join/reclaim only after cooperative exit.
        stopTimer(); stopThread(-1); dialog.exitModalState(1); dialog.setVisible(false);
        if (done) { if (cancelled) done(nullptr); else done(std::move(renderer)); }
        delete this;
    }
    std::unique_ptr<Engine> renderer; int64_t target; std::function<void(std::unique_ptr<Engine>)> done;
    juce::Component::SafePointer<juce::Component> owner; double progress=0; juce::AlertWindow dialog;
    std::atomic<int64_t> completed {0}; bool cancelled=false;
};
}
void Workstation::initialiseWorkModes()
{
    addAndMakeVisible(workModeButton); addAndMakeVisible(takeButton);
    workModeButton.setComponentID("work-mode"); takeButton.setComponentID("performance-takes");
    workModeButton.setTooltip("Arrange: free editing, playback and offline WAV. Record/Perform: Play starts an independent editable take; headphones and master WAV are optional.");
    engine.setWorkMode(layout.workMode==1 ? WorkMode::recordPerform : WorkMode::arrange);
    libraryPreview.setPrivateOutput(engine.workMode==WorkMode::recordPerform);
    libraryPreview.restoreOutputPreference({layout.privateDeviceType,layout.privateDeviceName,layout.privateFirstChannel}); libraryPreview.setLevel(layout.privateLevel);
    engine.onPerformanceBegin=[this] { remember(); };
    engine.onPerformanceFinalised=[this] { editors.clear(); dirty=true; setStatus("Performance take finalised. Save retains it; Takes / preview offers audition, event editing and WAV export."); };
    workModeButton.setButtonText(engine.workMode==WorkMode::arrange ? "Arrange" : "Record/Perform");
    workModeButton.onClick=[this]
    {
        juce::PopupMenu menu; menu.addItem(1,"Arrange",!busy && !engine.exporting,engine.workMode==WorkMode::arrange);
        menu.addItem(2,"Record/Perform",!busy && !engine.exporting,engine.workMode==WorkMode::recordPerform);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&workModeButton),[safe=juce::Component::SafePointer<Workstation>(this)](int result)
        {
            if (!safe || result<1 || result>2 || safe->busy || safe->engine.exporting) return;
            const auto mode=result==1 ? WorkMode::arrange : WorkMode::recordPerform;
            if (mode==safe->engine.workMode) return;
            const auto changed=safe->engine.setWorkMode(mode); if (changed.failed()) { safe->setStatus(changed.getErrorMessage()); return; }
            safe->libraryPreview.stop(); safe->engine.cancelPreparedLaunch();
            beginPrivateRequest(*safe);
            safe->libraryPreview.setPrivateOutput(mode==WorkMode::recordPerform); safe->layout.workMode=result-1; safe->workspaceDirty=true; safe->workspaceChangedAt=juce::Time::getMillisecondCounterHiRes();
            safe->workModeButton.setButtonText(result==1 ? "Arrange" : "Record/Perform"); safe->refresh(); safe->setStatus(safe->engine.performanceStatus());
        });
    };
    takeButton.onClick=[this] { showTakeMenu(); };
}
void Workstation::showTakeMenu()
{
    if (busy || engine.exporting) return;
    juce::PopupMenu menu; menu.addSectionHeader(engine.performanceStatus());
    if (engine.capturingPerformance()) menu.addItem(1,"Stop and finalise take");
    menu.addItem(2,"Private preparation / headphones...",engine.workMode==WorkMode::recordPerform);
    menu.addItem(3,"Record master WAV...",!engine.playing && !engine.capturingPerformance(),engine.recordMasterWav);
    menu.addItem(4,"Return selected control to automation",engine.capturingPerformance() || engine.recordingAutomation());
    menu.addItem(5,"Stop take audition");
    menu.addItem(6,"Open targeted-analysis evidence folder");
    juce::PopupMenu exclusions;
    for (int lane=0;lane<trackCount;++lane) exclusions.addItem(50+lane,"Capture lane "+juce::String(lane+1),true,engine.automationArmed[(size_t)lane]);
    exclusions.addItem(54,"Capture master",true,engine.masterAutomationArmed); menu.addSubMenu("Advanced capture exclusions",exclusions,engine.workMode==WorkMode::recordPerform);
    menu.addSeparator();
    std::vector<juce::String> takeIds;
    const auto openedMode=engine.workMode;
    const auto armedAtOpen=engine.automationArmed;
    const bool masterArmedAtOpen=engine.masterAutomationArmed;
    for (int i=0;i<(int)engine.project.performanceTakes.size();++i)
    {
        const auto& take=engine.project.performanceTakes[(size_t)i]; juce::PopupMenu submenu;
        takeIds.push_back(take.id);
        submenu.addItem(100+i*4,"Audition from beginning",!engine.playing && !engine.capturingPerformance());
        submenu.addItem(300+i,"Seek / audition at performance time...",!engine.playing && !engine.capturingPerformance());
        submenu.addItem(101+i*4,"Export take WAV...",!engine.capturingPerformance());
        submenu.addItem(102+i*4,"Edit / remove chronological event...",!engine.capturingPerformance() && !take.events.empty());
        submenu.addItem(103+i*4,"Discard this take",!engine.capturingPerformance());
        menu.addSubMenu(take.name+" / "+timeText(take.frames/take.sampleRate)+(take.complete ? "" : " / incomplete"),submenu);
    }
    menu.addSeparator(); menu.addItem(-1,"Mapped/direct VST parameters are captured. Plugin programs, live instruments and nondeterministic effects may need the master WAV.",false);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&takeButton),[safe=juce::Component::SafePointer<Workstation>(this),takeIds=std::move(takeIds),openedMode,armedAtOpen,masterArmedAtOpen](int result)
    {
        if (!safe || result==0 || safe->busy || safe->engine.exporting) return;
        auto target=[&](int slot)
        {
            if (slot<0 || slot>=(int)takeIds.size()) return -1;
            const int index=takeIndex(safe->engine.project,takeIds[(size_t)slot]);
            if (index<0) safe->setStatus("That take is no longer available. Reopen Takes / preview.");
            return index;
        };
        if (result==1) { safe->engine.stop(); safe->refresh(); }
        else if (result==2) safe->showPrivatePreparationMenu();
        else if (result==3)
        {
            if (safe->engine.playing || safe->engine.capturingPerformance()) return;
            if (safe->engine.recordMasterWav) { safe->engine.recordMasterWav=false; safe->setStatus("Master WAV off. Editable performance capture remains enabled."); return; }
            safe->chooser=std::make_unique<juce::FileChooser>("New live master WAV (existing files are kept)",juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Performance.wav"),"*.wav",false);
            safe->chooser->launchAsync(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles,[safe](const auto& chooser)
            { if (!safe || safe->busy || safe->engine.exporting || safe->engine.playing || safe->engine.capturingPerformance() || chooser.getResult()==juce::File()) return; const auto file=chooser.getResult().withFileExtension("wav"); if (file.exists()) { safe->setStatus("Choose a new recording filename; existing files were kept."); return; } safe->engine.masterRecordingDestination=file; safe->engine.recordMasterWav=true; safe->setStatus("Next take also records master WAV: "+file.getFullPathName()); });
        }
        else if (result==4) { safe->engine.returnToAutomation(safe->displayedAddress(safe->engine.selectedTrack)); safe->refresh(); }
        else if (result==5) safe->engine.endTakeAudition();
        else if (result==6)
        {
            const auto folder=safe->catalogueFile.getSiblingFile("AnalysisRetries"); if (folder.isDirectory()) folder.revealToUser(); else safe->setStatus("No targeted-analysis evidence has been written yet.");
        }
        else if (result>=50 && result<=54)
        {
            const int lane=result==54 ? -1 : result-50;
            const bool previous=lane<0 ? masterArmedAtOpen : armedAtOpen[(size_t)lane];
            const bool current=lane<0 ? safe->engine.masterAutomationArmed : safe->engine.automationArmed[(size_t)lane];
            if (openedMode!=WorkMode::recordPerform || safe->engine.workMode!=openedMode || current!=previous)
            { safe->setStatus("Capture eligibility changed. Reopen Takes / preview."); return; }
            safe->engine.armAutomation(lane,!previous); safe->setStatus("Advanced capture eligibility changed. Excluded gestures are not part of the editable take.");
        }
        else if (result>=300)
        {
            if (safe->engine.playing || safe->engine.capturingPerformance() || safe->engine.workMode!=openedMode) return;
            const int index=target(result-300); if (index<0) return;
            const auto id=safe->engine.project.performanceTakes[(size_t)index].id;
            const auto fingerprint=takeFingerprint(safe->engine.project.performanceTakes[(size_t)index]);
            const auto request=safe->engine.beginTakeAuditionRequest();
            auto* dialog=new juce::AlertWindow("Seek performance take","Choose where to listen in this recorded performance. Preparation can be cancelled.",juce::MessageBoxIconType::NoIcon,safe.getComponent());
            dialog->addTextEditor("seconds","0","Performance seconds"); dialog->addButton("Prepare",1); dialog->addButton("Cancel",0);
            dialog->enterModalState(true,juce::ModalCallbackFunction::create([safe,dialog,id,fingerprint,openedMode,request](int result)
            {
                if (!safe || result!=1 || safe->busy || safe->engine.exporting || safe->engine.playing || safe->engine.capturingPerformance() || safe->engine.workMode!=openedMode) return;
                if (!safe->engine.isTakeAuditionRequestCurrent(request)) return;
                const int index=takeIndex(safe->engine.project,id);
                if (index<0 || takeFingerprint(safe->engine.project.performanceTakes[(size_t)index])!=fingerprint)
                { safe->setStatus("That take changed. Reopen its seek dialog."); return; }
                const auto& take=safe->engine.project.performanceTakes[(size_t)index]; const auto text=dialog->getTextEditorContents("seconds").trim(); double seconds=0;
                const char* begin=text.toRawUTF8(); const auto parsed=std::from_chars(begin,begin+text.getNumBytesAsUTF8(),seconds);
                if (take.id!=id || parsed.ec!=std::errc() || parsed.ptr!=begin+text.getNumBytesAsUTF8() || !std::isfinite(seconds) || seconds<0 || seconds>=take.frames/take.sampleRate) { safe->setStatus("Choose a performance time inside the saved take."); return; }
                juce::String error; auto renderer=safe->engine.prepareTakeReplay(index,error); if (!renderer) { safe->setStatus(error); return; }
                auto* preparation=new TakeSeekPreparation(std::move(renderer),seconds,safe.getComponent(),[safe,id,fingerprint,openedMode,request](std::unique_ptr<Engine> ready)
                {
                    if (!safe || !ready || safe->busy || safe->engine.exporting || safe->engine.playing || safe->engine.capturingPerformance() || safe->engine.workMode!=openedMode) return;
                    if (!safe->engine.isTakeAuditionRequestCurrent(request)) return;
                    const int current=takeIndex(safe->engine.project,id);
                    if (current<0 || takeFingerprint(safe->engine.project.performanceTakes[(size_t)current])!=fingerprint)
                    { safe->setStatus("That take changed during preparation. Reopen its seek dialog."); return; }
                    safe->libraryPreview.stop(); const auto result=safe->engine.installTakeAudition(std::move(ready)); safe->setStatus(result.failed() ? result.getErrorMessage() : "Auditioning reconstructed take history. The source arrangement cursor is unchanged.");
                }); preparation->launch();
            }),true);
        }
        else if (result>=100)
        {
            if (safe->engine.capturingPerformance()) return;
            const int take=target((result-100)/4),action=(result-100)%4;
            if (take<0 || (action==0 && safe->engine.playing)) return;
            if (action==0) { safe->libraryPreview.stop(); const auto reply=safe->engine.auditionTake(take); safe->setStatus(reply.failed() ? reply.getErrorMessage() : "Auditioning saved take through master. Stop ends audition."); }
            else if (action==1) safe->chooseExport(take);
            else if (action==2) safe->editTakeEvent(take,-1);
            else { safe->remember(); safe->engine.endTakeAudition(); safe->engine.project.performanceTakes.erase(safe->engine.project.performanceTakes.begin()+take); safe->refresh(); safe->setStatus("Take discarded. Undo restores it; any recorded WAV remains on disk."); }
        }
    });
}
void Workstation::editTakeEvent(int take,int index)
{
    if (busy || engine.exporting || take<0 || take>=(int)engine.project.performanceTakes.size() || engine.capturingPerformance()) return;
    const auto& value=engine.project.performanceTakes[(size_t)take];
    const auto id=value.id,fingerprint=takeFingerprint(value);
    if (index<0)
    {
        auto* dialog=new juce::AlertWindow("Edit performance event",value.name+" has "+juce::String((int)value.events.size())+" events. Choose the event number to edit.",juce::MessageBoxIconType::NoIcon,this);
        dialog->addTextEditor("event","1","Event number"); dialog->addButton("Edit",1); dialog->addButton("Cancel",0);
        dialog->enterModalState(true,juce::ModalCallbackFunction::create([safe=juce::Component::SafePointer<Workstation>(this),dialog,id,fingerprint](int answer)
        {
            if (!safe || answer!=1 || safe->busy || safe->engine.exporting || safe->engine.capturingPerformance()) return;
            const int current=takeIndex(safe->engine.project,id);
            if (current<0 || takeFingerprint(safe->engine.project.performanceTakes[(size_t)current])!=fingerprint)
            { safe->setStatus("That take changed. Reopen its event editor."); return; }
            safe->editTakeEvent(current,dialog->getTextEditorContents("event").getIntValue()-1);
        }),true); return;
    }
    if (index>=(int)value.events.size()) { setStatus("Choose an existing event number."); return; }
    const auto original=value.events[(size_t)index];
    const char* kinds[]{"Control","Play / pause","Programme seek","Hotcue launch","Mute","Solo","Tempo","Return to automation","Programme edit","Plugin state","MIDI input"};
    juce::String owner=original.lane<0 ? "Master" : "Lane "+juce::String(original.lane+1);
    auto snapshot=value.initial; for (const auto& e:value.events) if (e.frame<=original.frame && e.kind==TakeEventKind::project) snapshot=e.payload;
    if (const auto* clips=snapshot["clips"].getArray()) for (const auto& c:*clips) if (c["id"].toString()==original.owner || c["trackOwnerId"].toString()==original.owner)
        if (const auto* media=snapshot["media"].getArray()) for (const auto& m:*media) if (m["id"]==c["mediaId"]) owner=m["name"].toString()+" / "+owner;
    const auto control=original.kind==TakeEventKind::control || original.kind==TakeEventKind::returnAutomation ? " / "+juce::String(automationName((AutomationTarget)original.control)) : juce::String();
    auto* dialog=new juce::AlertWindow("Edit performance event",juce::String("Event ")+juce::String(index+1)+" / "+kinds[(int)original.kind]+" / "+owner+control+". Enter time in seconds from the performance start.",juce::MessageBoxIconType::NoIcon,this);
    dialog->addTextEditor("time",juce::String(original.frame/value.sampleRate,6),"Performance seconds"); dialog->addTextEditor("value",juce::String(original.value,6),"Control / destination value");
    dialog->addButton("Apply",1); dialog->addButton("Remove event",2); dialog->addButton("Cancel",0);
    dialog->enterModalState(true,juce::ModalCallbackFunction::create([safe=juce::Component::SafePointer<Workstation>(this),dialog,index,id,fingerprint](int answer)
    {
        if (!safe || answer==0 || safe->busy || safe->engine.exporting || safe->engine.capturingPerformance()) return;
        const int take=takeIndex(safe->engine.project,id);
        if (take<0 || takeFingerprint(safe->engine.project.performanceTakes[(size_t)take])!=fingerprint)
        { safe->setStatus("That take changed. Reopen its event editor."); return; }
        auto candidate=safe->engine.project.performanceTakes[(size_t)take]; if (index>=(int)candidate.events.size()) return;
        if (answer==2) candidate.events.erase(candidate.events.begin()+index);
        else
        {
            const double time=dialog->getTextEditorContents("time").getDoubleValue(),value=dialog->getTextEditorContents("value").getDoubleValue();
            if (!std::isfinite(time) || time<0 || time>candidate.frames/candidate.sampleRate || !std::isfinite(value)) { safe->setStatus("Keep event time inside the take and enter a finite value."); return; }
            auto& event=candidate.events[(size_t)index]; event.frame=(int64_t)std::llround(time*candidate.sampleRate); event.value=value;
            std::stable_sort(candidate.events.begin(),candidate.events.end(),[](const auto& a,const auto& b) { return a.frame<b.frame; });
            for (size_t i=0;i<candidate.events.size();++i) candidate.events[i].sequence=(int64_t)i;
        }
        PerformanceTake validated; const auto valid=readTake(takeToJson(candidate),validated); if (valid.failed()) { safe->setStatus(valid.getErrorMessage()); return; }
        safe->remember(); safe->engine.endTakeAudition(); safe->engine.project.performanceTakes[(size_t)take]=std::move(candidate); safe->refresh(); safe->setStatus("Performance event edited. The original arrangement and recorded WAV were kept.");
    }),true);
}
void Workstation::showPrivatePreparationMenu()
{
    if (busy || engine.exporting || engine.workMode!=WorkMode::recordPerform) return;
    const auto clipId=selectedClip,transitionId=selectedTransition,libraryId=selectedLibraryId;
    const int lane=engine.selectedTrack.load();
    const auto request=privateRequest(*this),fingerprint=privateFingerprint(privatePreparation);
    juce::PopupMenu menu; menu.addSectionHeader("Private / "+libraryPreview.description());
    menu.addItem(1,"Choose headphones route...",!engine.playing && !engine.capturingPerformance());
    menu.addItem(2,"Prepare selected track / segment",selectedClip.isNotEmpty());
    menu.addItem(3,"Prepare selected complete transition",selectedTransition.isNotEmpty());
    menu.addItem(4,"Play / test headphones",privatePreparation.ready()); menu.addItem(5,"Stop private playback");
    menu.addItem(6,"Private seek / level / loop...",privatePreparation.ready());
    menu.addItem(7,"Edit private track / transition controls and VST parameters...",privatePreparation.ready());
    menu.addItem(8,"Prepare selected library song for lane "+juce::String(engine.selectedTrack.load()+1),selectedLibraryId.isNotEmpty());
    for (int i=0;i<3;++i) menu.addItem(10+i,"Hear private alternative "+juce::String::charToString((juce::juce_wchar)('A'+i)),privatePreparation.ready());
    menu.addItem(20,"Apply private edits to programme now",privatePreparation.ready());
    menu.addItem(21,"Launch private edits at "+juce::String(launchTimingName(engine.project.launch.timing)),privatePreparation.ready() && engine.playing);
    menu.addItem(22,"Discard private edits",privatePreparation.ready());
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&takeButton),[safe=juce::Component::SafePointer<Workstation>(this),clipId,transitionId,libraryId,lane,request,fingerprint](int result)
    {
        if (!safe || result==0) return;
        if (safe->busy || safe->engine.exporting || safe->engine.workMode!=WorkMode::recordPerform) return;
        if (privateRequest(*safe)!=request || privateFingerprint(safe->privatePreparation)!=fingerprint)
        { safe->setStatus("Private preparation changed. Reopen its menu."); return; }
        auto done=[&](const juce::Result& reply)
        {
            auto* renderer=safe->privatePreparation.privateEngine(); const auto* clip=renderer ? renderer->project.clipFor(safe->privatePreparation.target()) : nullptr;
            const auto target=clip ? clip->media->libraryTitle()+" / lane "+juce::String(clip->track+1) : "selected transition";
            safe->setStatus(reply.failed() ? reply.getErrorMessage() : "Private preparation / "+target+". Apply now or Launch at "+juce::String(launchTimingName(safe->engine.project.launch.timing))+".");
        };
        if (result==1)
        {
            if (safe->engine.playing || safe->engine.capturingPerformance()) return;
            safe->libraryPreview.chooseOutput(*safe,[safe]
        { if (!safe) return; const auto route=safe->libraryPreview.selectedOutput(); safe->layout.privateDeviceType=route.type; safe->layout.privateDeviceName=route.name; safe->layout.privateFirstChannel=route.firstChannel; safe->workspaceDirty=true; safe->workspaceChangedAt=juce::Time::getMillisecondCounterHiRes(); });
        }
        else if (result==2 || result==3)
        {
            const auto id=result==2 ? clipId : transitionId;
            if (!safe->engine.project.clipFor(id)) { safe->setStatus("That private preparation target is no longer available."); return; }
            beginPrivateRequest(*safe); safe->libraryPreview.stop(); done(result==2 ? safe->privatePreparation.prepareTrack(id) : safe->privatePreparation.prepareTransition(id));
        }
        else if (result==4) done(safe->libraryPreview.playPrepared(safe->privatePreparation,"private-preparation"));
        else if (result==5) safe->libraryPreview.stop();
        else if (result==8)
        {
            if (safe->engine.selectedTrack.load()!=lane) { safe->setStatus("The destination lane changed. Reopen private preparation."); return; }
            const auto found=std::find_if(safe->engine.library.begin(),safe->engine.library.end(),[&](const auto& m) { return m->id==libraryId; });
            if (found==safe->engine.library.end()) return; const auto media=*found; safe->libraryPreview.stop();
            const auto pending=beginPrivateRequest(*safe);
            if (media->hasPlayback()) { done(safe->privatePreparation.prepareTrack(media->id)); return; }
            safe->setStatus("Preparing only the selected library song for private editing; existing analysis is reused.");
            const auto record=media->analysisRecord(); safe->workers.addJob([safe,media,record,pending,fingerprint,lane]
            {
                juce::AudioFormatManager formats; formats.registerBasicFormats(); const auto loaded=readMedia(formats,media->file,{},AnalysisCacheMode::readOnly,MediaReadPurpose::playback,&record);
                juce::MessageManager::callAsync([safe,media,loaded,pending,fingerprint,lane]
                {
                    if (!safe || safe->busy || safe->engine.exporting || safe->engine.workMode!=WorkMode::recordPerform
                        || safe->selectedLibraryId!=media->id || safe->engine.selectedTrack.load()!=lane
                        || privateRequest(*safe)!=pending || privateFingerprint(safe->privatePreparation)!=fingerprint) return;
                    if (std::none_of(safe->engine.library.begin(),safe->engine.library.end(),[&](const auto& entry) { return entry==media; })) return;
                    if (!loaded.media) { safe->setStatus(loaded.error); return; }
                    if (!(loaded.media->analysisSource==media->analysisSource)) { safe->setStatus("Source identity changed. Refresh this library entry before private preparation."); return; }
                    { const juce::ScopedLock guard(safe->engine.lock); media->original=std::move(loaded.media->original); media->cache.reset(); media->samples.setSize(0,0); }
                    const auto prepared=safe->privatePreparation.prepareTrack(media->id); safe->setStatus(prepared.failed() ? prepared.getErrorMessage() : "Library song prepared privately. Seek/edit, then Apply/Launch to the named lane.");
                });
            });
        }
        else if (result>=10 && result<=12) { beginPrivateRequest(*safe); safe->libraryPreview.stop(); done(safe->privatePreparation.alternative(result-10)); }
        else if (result==20 || result==21) { if (!safe->engine.capturingPerformance()) safe->remember(); done(safe->privatePreparation.apply(result==21)); safe->refresh(); }
        else if (result==22) { beginPrivateRequest(*safe); safe->libraryPreview.stop(); safe->privatePreparation.discard(); safe->setStatus("Private preparation discarded."); }
        else if (result==6 || result==7)
        {
            auto choices=std::make_shared<std::vector<AutomationAddress>>();
            auto* dialog=new juce::AlertWindow(result==6 ? "Private transport" : "Private track controls","Private edits do not change the audience programme. Use Apply/Launch when ready.",juce::MessageBoxIconType::NoIcon,safe.getComponent());
            if (result==6) { dialog->addTextEditor("position",juce::String(safe->privatePreparation.cursor(),3),"Private source/timeline seconds"); dialog->addTextEditor("level",juce::String(safe->layout.privateLevel,2),"Headphones level (0-1)"); dialog->addComboBox("loop",{"Loop","One pass"},"Private transport"); }
            else
            {
                juce::StringArray names;
                if (auto* renderer=safe->privatePreparation.privateEngine()) for (const auto& clip:renderer->project.clips)
                {
                    for (int target=0;target<trackAutomationCount;++target) { choices->push_back({clip.track,(AutomationTarget)target,{},{},clip.id}); names.add(clip.media->name+" / "+automationName((AutomationTarget)target)); }
                    for (const auto& slot:clip.effects->plugins) for (const auto& [id,p]:slot->parameters) if (p->isAutomatable())
                    { choices->push_back({clip.track,AutomationTarget::plugin,slot->id,id,clip.id}); names.add(clip.media->name+" / "+slot->description.name+" / "+p->getName(128)); }
                }
                dialog->addComboBox("parameter",names,"Private owner / control"); dialog->addTextEditor("value","0","dB / pan/filter (-1..1) / VST (0..1)");
            }
            dialog->addButton("Apply privately",1); dialog->addButton("Cancel",0);
            dialog->enterModalState(true,juce::ModalCallbackFunction::create([safe,dialog,result,choices,request,fingerprint](int answer)
            {
                if (!safe || answer!=1 || safe->busy || safe->engine.exporting || safe->engine.workMode!=WorkMode::recordPerform) return;
                if (privateRequest(*safe)!=request || privateFingerprint(safe->privatePreparation)!=fingerprint)
                { safe->setStatus("Private preparation changed. Reopen its editor."); return; }
                beginPrivateRequest(*safe);
                if (result==6) { safe->privatePreparation.seek(dialog->getTextEditorContents("position").getDoubleValue()); safe->privatePreparation.setLoop(dialog->getComboBoxComponent("loop")->getSelectedId()!=2); const float level=juce::jlimit(0.f,1.f,(float)dialog->getTextEditorContents("level").getDoubleValue()); safe->libraryPreview.setLevel(level); safe->layout.privateLevel=level; safe->workspaceDirty=true; safe->workspaceChangedAt=juce::Time::getMillisecondCounterHiRes(); }
                else
                { const int index=dialog->getComboBoxComponent("parameter")->getSelectedItemIndex(); if (index>=0 && index<(int)choices->size()) { const auto reply=safe->privatePreparation.setControl((*choices)[(size_t)index],(float)dialog->getTextEditorContents("value").getDoubleValue()); safe->setStatus(reply.failed() ? reply.getErrorMessage() : "Private control changed. Programme remains unchanged until Apply/Launch."); } }
            }),true);
        }
    });
}
}
