#include "Model.h"
#include <cmath>
namespace mixworks
{
void runTargetedAnalysisTests(const std::function<void(bool,const juce::String&)>& check,const juce::File& report)
{
    MusicAnalysis previous; previous.bpm=123; previous.beatOrigin=.1; previous.downbeatOrigin=.1;
    previous.tempoConfidence=.9; previous.key=0; previous.keyConfidence=.4;
    check(needsFocusedKey(previous),"library-review confidence .40 is eligible for targeted key retry");
    previous.keyConfidence=keyReviewConfidence; check(!needsFocusedKey(previous),"accepted .55 key is protected"); previous.keyConfidence=.4;
    MusicAnalysis candidate=previous; candidate.bpm=0; candidate.rhythmIssue="Model unavailable"; candidate.key=7; candidate.keyConfidence=.8;
    const auto merged=mergeAnalysisRetry(previous,candidate,true,true);
    check(merged.bpm==previous.bpm && merged.beatOrigin==previous.beatOrigin && merged.downbeatOrigin==previous.downbeatOrigin
        && merged.tempoConfidence==previous.tempoConfidence,"failed tempo leaves accepted pulse/downbeat unchanged");
    check(merged.key==7 && merged.keyConfidence==.8 && merged.focusedKey==2,"failed tempo cannot discard independently improved key");
    check(merged.retryEvidence["tempoDecision"].toString().contains("Model unavailable") && merged.retryEvidence["keyDecision"].toString().contains("improved"),"per-field reasons and before/candidate/result evidence retained");
    check(!needsFocusedKey(merged),"same targeted method is not repeated indefinitely");
    previous.rhythmIssue="Model unavailable"; previous.tempoConfidence=0;
    const auto partial=mergeAnalysisRetry(previous,candidate,true,true);
    TrackAnalysisRecord record; record.source={juce::String::repeatedString("a",64),40000,8000,8000,1};
    record.analysis=partial; record.peaks.resize(4000); record.manualBeatOrigin=.25; record.manualBpm=122; record.manualKey=5;
    const auto document=analysisRecordToJson(record); const auto parsed=parseAnalysisRecord(document,record.source);
    check(parsed.reusableAutomatic && parsed.record && parsed.record->analysis.key==7 && parsed.record->analysis.rhythmIssue.isNotEmpty(),"partial key success survives cache parsing alongside unresolved tempo");
    check(parsed.record && parsed.record->manualBpm==122 && parsed.record->manualBeatOrigin==.25 && parsed.record->manualKey==5,"manual fields survive partial retry persistence");
    check(report.getSiblingFile("retry-evidence.json").replaceWithText(juce::JSON::toString(document)),"retain understandable numerical retry evidence");
    juce::AudioBuffer<float> silent(1,8000*2); silent.clear();
    MusicAnalysis weak; weak.key=-1; weak.tempoConfidence=1; weak.bpm=120; weak.downbeatOrigin=0;
    auto stopped=std::make_shared<std::atomic<bool>>(true);
    const auto cancelled=retryMusicAnalysis(PcmView(silent),8000,weak,false,true,stopped);
    check(cancelled.focusedRevision==0 && cancelled.key==-1,"cancellation never commits a partial attempted-method marker");
    const auto quiet=retryMusicAnalysis(PcmView(silent),8000,weak,false,true);
    check(quiet.bpm==120 && quiet.focusedKey==1 && quiet.retryEvidence["keyDecision"].toString().contains("review required"),"key-only retry abstains on silence and preserves accepted BPM without invoking a rhythm model");
    juce::AudioBuffer<float> rhythm(1,8000*8); rhythm.clear();
    for (int i=0;i<rhythm.getNumSamples();++i) rhythm.setSample(0,i,(float)(std::sin(i*.09)*std::exp(-(i%4000)/80.0)*.5));
    weak.bpm=0; weak.tempoConfidence=0; weak.rhythmIssue="Model unavailable";
    const auto compared=retryMusicAnalysis(PcmView(rhythm),8000,weak,true,false);
    check(compared.retryEvidence["passages"].isArray() && compared.retryEvidence["passages"].size()==1 && compared.retryEvidence["passages"][0]["soundTouchBpm"].isDouble(),"existing SoundTouch is evaluated on a bounded physical-pulse retry; raw estimate and agreement classification retained");
}
}
