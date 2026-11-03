#include "Model.h"
#include "EnergySections.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace mixworks
{
juce::Result arrangeVisualScenes(Project& project, const std::vector<VisualSceneEntry>& library, juce::Random& random)
{
    auto pool = automaticVisualScenes(library);
    if (pool.empty()) return juce::Result::fail("No music visual scenes are available. Keep the visual lane or install the scene collection.");
    if (project.clips.empty()) return juce::Result::fail("Add music before matching visual scenes.");
    const auto transitions = project.validateTransitionLinks(); if (transitions.failed()) return transitions;
    struct Song
    {
        juce::Range<double> entry;
        int next=-1;
        bool incoming=false;
        const Project::TransitionLink* transition=nullptr;
        double energy=.5;
        const Clip* clip=nullptr;
        bool energyRise=false;
    };
    std::vector<Song> facts;
    std::map<juce::String,int> indices;
    // Playback starts at zero even when musical grid alignment adds a lead-in.
    constexpr double begin=0;
    double end=0;
    for (const auto& clip : project.clips)
    {
        if (!std::isfinite(clip.start) || !std::isfinite(clip.end()) || clip.start<0 || clip.length<=0 || clip.end()>1.e9
            || !indices.emplace(clip.id,(int)facts.size()).second)
            return juce::Result::fail("A music placement has invalid timing or identity.");
        facts.push_back({{clip.start,clip.start}});
        auto& song=facts.back();
        song.clip=&clip;
        const auto activity=clip.media ? musicalEnergyBody(clip.media->energy) : std::nullopt;
        song.energy=activity.value_or(juce::jlimit(.2,.85,(project.gridClock().bpmAt(clip.start)-80)/100));
        end=std::max(end,clip.end());
    }
    for (const auto& link : project.transitionLinks)
    {
        // The music owner maps source time, preroll, tempo ramps and frame edits.
        const auto phase=project.transitionRange(link);
        if (!std::isfinite(phase.getStart()) || !std::isfinite(phase.getEnd()) || phase.getStart()<0 || phase.getEnd()<phase.getStart())
            return juce::Result::fail("A music transition has invalid visual handover timing.");
        const int out=indices.at(link.outgoing), in=indices.at(link.incoming);
        facts[(size_t)out].next=in; facts[(size_t)in].incoming=true; facts[(size_t)in].entry=phase;
        facts[(size_t)in].transition=&link;
    }
    // A clip's preroll may start before its predecessor. Follow transition
    // ownership, merging independent chains by their actual handover cue.
    std::vector<int> ready;
    for (size_t i=0;i<facts.size();++i) if (!facts[i].incoming) ready.push_back((int)i);
    std::vector<const Song*> songs;
    const auto cue=[](const Song& song) { return (song.entry.getStart()+song.entry.getEnd())*.5; };
    while (!ready.empty())
    {
        const auto next=std::min_element(ready.begin(),ready.end(),[&](int a,int b)
            { const double x=cue(facts[(size_t)a]), y=cue(facts[(size_t)b]); return x==y ? a<b : x<y; });
        const auto& song=facts[(size_t)*next]; ready.erase(next); songs.push_back(&song);
        if (song.next>=0) ready.push_back(song.next);
    }
    if (songs.size()!=facts.size()) return juce::Result::fail("Music transition order could not be resolved.");

    // Generated scenes get at least 100ms of ordered timeline space. If an
    // exceptionally short mix cannot hold every song, share scenes across it.
    constexpr double minimumHold=.1;
    end=std::max(end,begin+minimumHold);
    const auto capacity=std::max((size_t)1,(size_t)std::floor((end-begin)/minimumHold));
    if (songs.size()>capacity)
    {
        std::vector<const Song*> retained;
        for (size_t i=0;i<capacity;++i) retained.push_back(songs[capacity==1 ? 0 : i*(songs.size()-1)/(capacity-1)]);
        songs=std::move(retained);
    }
    std::vector<juce::Range<double>> windows;
    std::vector<double> centres;
    for (size_t i=1;i<songs.size();++i)
    {
        const auto desired=songs[i]->entry;
        const double centre=std::max(i==1 ? begin+minimumHold : centres.back()+minimumHold,
            juce::jlimit(begin+i*minimumHold,end-(songs.size()-i)*minimumHold,cue(*songs[i])));
        centres.push_back(centre);
        // A missing/out-of-order music phase gets a short visual-only blend.
        // Its final limits are fitted below, without changing any music timing.
        windows.push_back(centre<desired.getStart() || centre>desired.getEnd()
            ? juce::Range<double>(centre,centre) : desired);
    }
    // Adjacent handovers share one boundary. Separated music phases retain
    // their exact edges; crowded/nested phases shorten without three layers.
    std::vector<double> boundaries {begin};
    for (size_t i=1;i<windows.size();++i)
        boundaries.push_back(juce::jlimit(centres[i-1],centres[i],(windows[i-1].getEnd()+windows[i].getStart())*.5));
    boundaries.push_back(end);
    for (size_t i=0;i<windows.size();++i)
    {
        windows[i]={std::max(boundaries[i],windows[i].getStart()),std::min(boundaries[i+1],windows[i].getEnd())};
        if (windows[i].isEmpty())
        {
            const double halfBeat=30./project.gridClock().bpmAt(centres[i]);
            windows[i]={std::max(boundaries[i],centres[i]-halfBeat),std::min(boundaries[i+1],centres[i]+halfBeat)};
        }
    }
    // Section markers belong to source time. Map through the same slip/tempo
    // conversion as audible section flags, only inside this song's visual span.
    // Use the interval between musical handover midpoints, then fit adjoining
    // windows. Long music fades must not exclude all of a song's section cues.
    const auto middle=[](juce::Range<double> range) { return (range.getStart()+range.getEnd())*.5; };
    std::vector<Song> sectionEvents;
    std::vector<juce::Range<double>> sectionWindows;
    for (size_t i=0;i<songs.size();++i)
    {
        if (i>0) sectionWindows.push_back(windows[i-1]);
        sectionEvents.push_back(*songs[i]);
        const auto* clip=songs[i]->clip;
        if (!clip || !clip->media || clip->reversed || clip->loopSpan>0) continue;
        double previousCue=i==0 ? begin : middle(windows[i-1]);
        const double limit=i+1==songs.size() ? end : middle(windows[i]);
        for (const auto& section : clipEnergySections(*clip))
        {
            const double at=clip->start+clip->audioTimelineTime(section.sourceSeconds-clip->offset);
            if (!std::isfinite(at) || at<=previousCue+minimumHold || at>=limit-minimumHold
                || at<=clip->start || at>=clip->end()) continue;
            const double beat=60./project.gridClock().bpmAt(at);
            const double sourceBar=4*60./std::max(20.,clip->media->bpm());
            const auto before=sourceEnergy(*clip->media,section.sourceSeconds-sourceBar,section.sourceSeconds);
            const auto after=sourceEnergy(*clip->media,section.sourceSeconds,section.sourceSeconds+sourceBar);
            double contrast=std::isfinite(section.contrastDb) ? section.contrastDb : 0;
            if (before && after) contrast=10*std::log10(std::max(1.e-12,*after)/std::max(1.e-12,*before));
            Song event=*songs[i]; event.transition=nullptr; event.energyRise=contrast>=6;
            event.energy=musicalEnergyAt(clip->media->energy,section.sourceSeconds,section.sourceSeconds+sourceBar)
                .value_or(juce::jlimit(.05,.95,songs[i]->energy+contrast/24.));
            const double duration=beat*(event.energyRise ? .5 : 4.);
            // A rise lands fully on the loud section's first beat, not halfway through it.
            const juce::Range<double> window {std::max(previousCue+minimumHold,at-duration),at};
            sectionWindows.push_back(window); sectionEvents.push_back(event); previousCue=at;
        }
    }
    if (sectionEvents.size()>songs.size())
    {
        std::vector<double> edges {begin};
        for (size_t i=1;i<sectionWindows.size();++i)
            edges.push_back(juce::jlimit(middle(sectionWindows[i-1]),middle(sectionWindows[i]),
                (sectionWindows[i-1].getEnd()+sectionWindows[i].getStart())*.5));
        edges.push_back(end);
        for (size_t i=0;i<sectionWindows.size();++i)
            sectionWindows[i]={std::max(edges[i],sectionWindows[i].getStart()),std::min(edges[i+1],sectionWindows[i].getEnd())};
    }
    songs.clear(); for (const auto& event : sectionEvents) songs.push_back(&event);
    windows=std::move(sectionWindows);
    // Musical energy favours a family without restricting the entire show to it.
    // Balance whole-show usage before repeating an eligible look.
    double mean=0; for (const auto* song : songs) mean+=song->energy; mean/=songs.size();
    const juce::String family=mean<.4 ? "Fluid" : mean>.65 ? "Electric" : "Prismatic";
    if (project.visualImage.isNotEmpty())
    {
        pool.erase(std::remove_if(pool.begin(),pool.end(),[](const auto& entry) { return !entry.imageReady; }),pool.end());
        if (pool.empty()) return juce::Result::fail("No image-integrating scenes are available in this collection.");
    }
    std::map<juce::String,int> sceneUses, transitionUses;
    const bool hasSolidScenes=std::any_of(pool.begin(),pool.end(),[](const auto& e){return e.family!="sprite-particles";});
    std::vector<VisualSceneClip> result;
    for (size_t i = 0; i < songs.size(); ++i)
    {
        size_t selected=0; double best=1.e9;
        for (size_t n=0;n<pool.size();++n)
        {
            // Particle scenes are occasional accents, at most one in five when
            // solid/image choices exist. Manual access to every scene remains.
            if (hasSolidScenes && pool[n].family=="sprite-particles" && i%5!=4) continue;
            const double score=sceneUses[pool[n].key]*2+std::abs(pool[n].energy-songs[i]->energy)+random.nextDouble()*.18
                +(pool[n].style==family ? 0 : .15)
                +(!result.empty() && pool[n].key==result.back().scene && pool.size()>1 ? 2 : 0);
            if (score<best) { best=score; selected=n; }
        }
        const auto& entry = pool[selected];
        ++sceneUses[entry.key];
        VisualSceneClip scene; scene.scene = entry.key; scene.title = entry.title;
        scene.style=entry.style.isEmpty() ? "Varied" : entry.style;
        if (project.visualImage.isNotEmpty()) scene.controls=entry.imageControls;
        scene.start = i==0 ? begin : windows[i-1].getStart();
        scene.length = (i+1==songs.size() ? end : windows[i].getEnd())-scene.start;
        automateVisualScene(scene,entry,songs[i]->energy,random);
        // Discrete choices land on the nearest 16-beat phrase within this block.
        const auto clock=project.gridClock();
        for(auto& [name,curve]:scene.automation) if(curve.stepped)
        {
            std::vector<VisualControlPoint> aligned {curve.points.front()};
            for(size_t n=1;n+1<curve.points.size();++n)
            {
                const auto point=curve.points[n];
                const double beat=std::round(clock.beatsAt(scene.start+point.at*scene.length)/16.)*16.;
                const double at=(clock.timeAtBeat(beat)-scene.start)/scene.length;
                if(at>aligned.back().at && at<1.) aligned.push_back({at,point.value});
            }
            aligned.push_back(curve.points.back());curve.points=std::move(aligned);
        }
        if (i>0)
        {
            const auto* link=songs[i]->transition;
            const auto window=windows[i-1];
            const double beats=project.gridClock().beatsAt(window.getEnd())-project.gridClock().beatsAt(window.getStart());
            const auto swap=link ? project.transitionSwapTimes(*link,1) : std::nullopt;
            if (window.isEmpty()) scene.transition="cut";
            else if (link && link->filter.active()) scene.transition=link->filter.mode==TransitionFilterMode::highToLow ? "sweep" : "sweep-reverse";
            else if (swap) scene.transition=beats<=2 || i%2==0 ? "zoom" : "radial";
            else if (songs[i]->energyRise || beats<=4 || songs[i]->energy-songs[i-1]->energy>.18) scene.transition="zoom";
            else scene.transition=i%2==0 ? "radial" : "dissolve";
            if (!window.isEmpty() && !(link && link->filter.active()))
            {
                const auto preferred=scene.transition;
                double bestTransition=1.e9;
                for (const auto* kind : {"dissolve","zoom","radial","sweep","sweep-reverse"})
                {
                    const double score=transitionUses[kind]+(preferred==kind ? 0 : .35)+random.nextDouble()*.1;
                    if (score<bestTransition) { bestTransition=score; scene.transition=kind; }
                }
            }
            // Extended track handovers need a direct blend, including filtered
            // music transitions. Keep spatial reveals for short section changes.
            if (window.getLength()>4. || beats>8.) scene.transition="dissolve";
            ++transitionUses[scene.transition];
            if (swap && window.getLength()>0)
                scene.pivot=juce::jlimit(.05,.95,(((*swap)[0]+(*swap)[1])*.5-window.getStart())/window.getLength());
        }
        result.push_back(std::move(scene));
    }
    const auto valid = validateVisualScenes(result);
    if (valid.failed()) return juce::Result::fail("The visual handover schedule is invalid: "+valid.getErrorMessage());
    project.visualScenes = std::move(result);
    return juce::Result::ok();
}
}
