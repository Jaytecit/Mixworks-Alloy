#include "AutoArrangement.h"
#include "Beatmatch.h"
#include "Engine.h"

namespace mixworks
{
// Bounded, read-only real-mix comparison. Only each selected pair is hydrated;
// the complete saved mix is planned from its retained analysis records.
void runAutoPairComparison(const std::function<void(bool,const juce::String&)>& check,
                          const juce::File& report,const juce::File& manifest)
{
    const auto input=juce::JSON::parse(manifest);
    const auto parsed=parseProject(juce::File(input["session"].toString()).loadFileAsString());
    check(parsed.error.isEmpty(),"read saved comparison mix: "+parsed.error); if (parsed.error.isNotEmpty()) return;
    std::map<juce::String,TrackAnalysisRecord> records;
    std::map<juce::String,std::shared_ptr<Media>> sources;
    for (const auto& entry : *input["sources"].getArray())
    {
        const auto cached=parseAnalysisRecord(juce::JSON::parse(juce::File(entry["record"].toString())));
        check(cached.record.has_value(),"read retained analysis "+entry["id"].toString()); if (!cached.record) return;
        const auto& r=*cached.record; auto m=std::make_shared<Media>(); m->id=entry["id"].toString();
        m->file=juce::File(entry["path"].toString()); m->name=entry["title"].toString();
        m->sampleRate=r.source.sampleRate; m->analysisSource=r.source; m->analysis=r.analysis;
        m->manualBpm=r.manualBpm; m->manualBeatOrigin=r.manualBeatOrigin; m->manualKey=r.manualKey;
        m->peaks=r.peaks; m->waveformRms=r.waveformRms; m->energy=r.energy; m->analysisVersion=r.revision;
        sources[m->id]=m; records[m->id]=r;
    }
    Project saved=parsed.project; auto clock=std::make_shared<const TempoMap>(saved.tempo);
    for (const auto& reference : parsed.clips)
    {
        auto clip=reference.clip; clip.media=sources[reference.mediaId];
        if (!clip.media) { check(false,"saved source has retained analysis"); return; }
        if (clip.beatmatched) clip.tempoMap=clock;
        saved.clips.push_back(std::move(clip));
    }
    const auto before=juce::JSON::toString(projectToJson(saved));
    auto timeline=planTimelineBlends(saved);
    check(timeline.error.isEmpty() && timeline.project.clips.size()==saved.clips.size(),"shared timeline planner completes saved mix: "+timeline.error);
    check(before==juce::JSON::toString(projectToJson(saved)),"comparison leaves saved input exact");
    if (timeline.error.isNotEmpty()) return;
    std::vector<std::shared_ptr<Media>> library;
    for (const auto& c : saved.clips) library.push_back(c.media);
    AutoArrangementOptions options; options.startingId=library.front()->id; options.trackCount=(int)library.size();
    juce::Random random(548); auto arranged=planAutoArrangement(library,options,random);
    check(arranged.error.isEmpty() && arranged.project.clips.size()==library.size(),"shared library planner completes the saved track selection: "+arranged.error+arranged.stopReason);
    for (const auto& item : std::array<std::pair<const char*,Project*>,2>{{{"timeline-planned",&timeline.project},{"library-planned",&arranged.project}}})
    {
        const auto json=juce::JSON::toString(projectToJson(*item.second),true,17);
        const auto reopened=parseProject(json);
        check(reopened.error.isEmpty(),juce::String(item.first)+" round-trips current session format: "+reopened.error);
        check(report.getSiblingFile(juce::String(item.first)+".mixworks").replaceWithText(json),"retain planned comparison mix");
    }
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    juce::Array<juce::var> variants;
    for (const auto& selected : *input["joins"].getArray())
    {
        const int index=(int)selected-1; if (index<0 || index>=(int)saved.transitionLinks.size()) continue;
        const auto oldLink=saved.transitionLinks[(size_t)index];
        std::map<juce::String,std::shared_ptr<Media>> audio;
        for (const auto& id : {oldLink.outgoing,oldLink.incoming})
        {
            const auto& meta=*saved.clipFor(id)->media;
            const auto loaded=readMedia(formats,meta.file,{},AnalysisCacheMode::readOnly,MediaReadPurpose::playback,&records[meta.id]);
            const bool valid=loaded.media && loaded.media->analysisSource==records[meta.id].source;
            check(valid,"comparison audio matches saved source identity: "+meta.name+" "+loaded.error); if (!valid) return;
            loaded.media->id=meta.id; loaded.media->name=meta.name; audio[id]=loaded.media;
        }
        for (const auto& choice : std::array<std::pair<const char*,const Project*>,2>{{{"saved",&saved},{"shared",&timeline.project}}})
        {
            Project pair=*choice.second;
            const auto* selectedLink=pair.transitionFor(oldLink.incoming); if (!selectedLink) { check(false,"selected join survives planning"); return; }
            const auto link=*selectedLink;
            pair.clips.erase(std::remove_if(pair.clips.begin(),pair.clips.end(),[&](const auto& c) { return c.id!=link.outgoing && c.id!=link.incoming; }),pair.clips.end());
            pair.transitionLinks={link};
            // Preserve every tempo value/time while detaching ownership from
            // omitted songs, so the two-song audition is a reopenable session.
            pair.tempo.detachMissingTracks(pair.clips); pair.visualScenes.clear();
            for (auto& c : pair.clips) c.media=audio[c.id];
            juce::String decision=link.presetName;
            if (juce::String(choice.first)=="shared")
            {
                AutoArrangementResult refined; refined.project=pair; refined.decisions.add(timeline.decisions[index]);
                applyAutoArrangementBass(refined,AutoTransitionStyle::balanced,0,formats);
                pair=std::move(refined.project); decision=refined.decisions[0];
                if (decision.startsWith("Bass handover:"))
                    check(std::all_of(pair.clips.begin(),pair.clips.end(),[&](const auto& clip)
                        { return pair.effectSettings(clip).automation[1].points.size()==2; }),
                        "saved-mix bass exchange has two endpoints per side, including nonzero incoming cues");
                const auto repeated=planTimelineBlends(pair);
                check(repeated.error.isEmpty() && repeated.project.transitionLinks.size()==1,
                    "the generated real pair accepts a second Blend timeline tracks command: "+repeated.error);
            }
            const auto ready=prepareSavedClips(pair);
            check(ready.isEmpty(),"prepare saved clocks for comparison: "+ready); if (ready.isNotEmpty()) return;
            const auto frame=pair.transitionRange(pair.transitionLinks.front()); const auto grid=pair.gridClock();
            const double begin=std::max(pair.playbackStart(),grid.timeAtBeat(grid.beatsAt(frame.getStart())-8));
            const double end=std::min(pair.duration(),grid.timeAtBeat(grid.beatsAt(frame.getEnd())+8));
            auto engine=std::make_unique<Engine>(juce::File(),false); engine->project=pair;
            if (juce::String(choice.first)=="shared")
            {
                const auto& entering=*pair.clipFor(link.incoming); const auto& leaving=*pair.clipFor(link.outgoing);
                const double lead=frame.getStart()-entering.start;
                if (lead>.01 && entering.track!=leaving.track)
                {
                    // Independent incoming-only PCM catches an unfaded file
                    // lead that curve-point/count checks cannot establish.
                    const bool muted=engine->project.tracks[(size_t)leaving.track].mute;
                    engine->project.tracks[(size_t)leaving.track].mute=true;
                    juce::String probeError; auto probe=engine->createExportRenderer(probeError);
                    engine->project.tracks[(size_t)leaving.track].mute=muted;
                    bool silent=false,audible=false;
                    if (probe)
                    {
                        juce::AudioBuffer<float> pcm(2,64); juce::MidiBuffer events;
                        const int count=std::min(64,(int)(lead*.25*probe->sampleRate.load()));
                        probe->render(pcm,count,entering.start+lead*.5,events);
                        silent=pcm.getMagnitude(0,count)==0;
                        for (double fraction : {.25,.5,.75})
                        { probe->render(pcm,64,frame.getStart()+frame.getLength()*fraction,events); audible|=pcm.getMagnitude(0,64)>1.e-6f; }
                    }
                    check(silent && audible,"real incoming PCM is silent before its cue and audible within its planned fade: "+probeError);
                }
            }
            juce::String error; auto renderer=engine->createExportRenderer(error);
            check(renderer!=nullptr,"create comparison renderer: "+error); if (!renderer) return;
            const double rate=renderer->sampleRate.load(); const int block=renderer->preparedBlockSize();
            const int frames=(int)std::llround((end-begin)*rate);
            const auto name="join-"+juce::String(index+1)+"-"+choice.first;
            const auto file=report.getSiblingFile(name+".wav"); juce::WavAudioFormat wav;
            std::unique_ptr<juce::OutputStream> stream=file.createOutputStream();
            auto writer=wav.createWriterFor(stream,juce::AudioFormatWriterOptions().withSampleRate(rate).withNumChannels(2).withBitsPerSample(24));
            juce::AudioBuffer<float> buffer(2,block); juce::MidiBuffer midi; bool ok=writer!=nullptr; float peak=0;
            for (int offset=0; ok && offset<frames; offset+=block)
            {
                const int count=std::min(block,frames-offset); midi.clear(); renderer->render(buffer,count,begin+offset/rate,midi);
                for (int side=0; side<2; ++side) peak=std::max(peak,buffer.getMagnitude(side,0,count));
                ok=writer->writeFromAudioSampleBuffer(buffer,0,count);
            }
            writer.reset(); check(ok && peak>.01 && peak<1,"render audible comparison with peak headroom: "+name);
            auto row=std::make_unique<juce::DynamicObject>();
            row->setProperty("name",name); row->setProperty("join",index+1); row->setProperty("variant",choice.first);
            row->setProperty("decision",decision); row->setProperty("frames",frames); row->setProperty("sampleRate",rate);
            row->setProperty("peak",peak); row->setProperty("sha256",audioDigest(file));
            row->setProperty("fadeStart",frame.getStart()-begin); row->setProperty("fadeEnd",frame.getEnd()-begin);
            row->setProperty("bars",(grid.beatsAt(frame.getEnd())-grid.beatsAt(frame.getStart()))/4);
            row->setProperty("outEnd",pair.clipFor(link.outgoing)->offset+pair.clipFor(link.outgoing)->sourceElapsed(pair.clipFor(link.outgoing)->length));
            row->setProperty("inOffset",pair.clipFor(link.incoming)->offset);
            variants.add(juce::var(row.release()));
            if (juce::String(choice.first)=="shared")
            {
                check(parseProject(juce::JSON::toString(projectToJson(pair),true,17)).error.isEmpty(),"two-track preview reopens without missing tempo owners");
                check(report.getSiblingFile(name+".mixworks").replaceWithText(juce::JSON::toString(projectToJson(pair),true,17)),"retain hydrated two-track preview");
            }
        }
    }
    auto evidence=std::make_unique<juce::DynamicObject>(); evidence->setProperty("version",JUCE_APPLICATION_VERSION_STRING);
    evidence->setProperty("variants",variants); evidence->setProperty("timelineDecisions",juce::var(timeline.decisions));
    evidence->setProperty("libraryDecisions",juce::var(arranged.decisions));
    check(report.getSiblingFile("pair-comparison.json").replaceWithText(juce::JSON::toString(juce::var(evidence.release()))),"retain measured comparison identities and placement");
}
}
