#include "PerformanceTake.h"
#include "Model.h"
#include <cmath>
namespace mixworks
{
juce::var takeToJson(const PerformanceTake& take)
{
    juce::var result(new juce::DynamicObject()); auto* d=result.getDynamicObject();
    d->setProperty("id",take.id); d->setProperty("name",take.name); d->setProperty("initial",take.initial);
    d->setProperty("sampleRate",take.sampleRate); d->setProperty("initialPosition",take.initialPosition);
    d->setProperty("frames",(juce::int64)take.frames); d->setProperty("complete",take.complete); d->setProperty("issue",take.issue); d->setProperty("masterWav",take.masterWav);
    d->setProperty("state",(int)take.state);
    juce::Array<juce::var> events;
    for (const auto& event : take.events)
    {
        juce::var item(new juce::DynamicObject()); auto* e=item.getDynamicObject();
        e->setProperty("frame",(juce::int64)event.frame); e->setProperty("sequence",(juce::int64)event.sequence);
        e->setProperty("kind",(int)event.kind); e->setProperty("lane",event.lane); e->setProperty("control",event.control); e->setProperty("stem",event.stem);
        e->setProperty("owner",event.owner); e->setProperty("slot",event.slot); e->setProperty("parameter",event.parameter);
        e->setProperty("value",event.value); e->setProperty("sourcePosition",event.sourcePosition); e->setProperty("payload",event.payload); events.add(item);
    }
    d->setProperty("events",events); return result;
}
juce::Result readTake(const juce::var& value, PerformanceTake& out)
{
    auto fail=[] { return juce::Result::fail("Invalid performance take; the current session was kept."); };
    auto number=[](const juce::var& x,double lo,double hi) { return (x.isInt()||x.isInt64()||x.isDouble()) && std::isfinite((double)x) && (double)x>=lo && (double)x<=hi; };
    if (!value.isObject() || !value["initial"].isObject() || value["initial"].hasProperty("performanceTakes")
        || !value["id"].isString() || value["id"].toString().isEmpty() || value["id"].toString().length()>512
        || !value["name"].isString() || value["name"].toString().length()>1024 || !value["issue"].isString() || value["issue"].toString().length()>4096
        || !value["masterWav"].isString() || !value["complete"].isBool() || !value["state"].isInt() || !number(value["state"],0,(int)TakeState::complete) || !number(value["sampleRate"],8000,768000)
        || !number(value["initialPosition"],0,1.e9) || !number(value["frames"],0,INT64_MAX/2)
        || !value["events"].isArray() || value["events"].size()>(int)PerformanceTake::maximumEvents) return fail();
    PerformanceTake take; take.id=value["id"].toString(); take.name=value["name"].toString(); take.initial=value["initial"];
    take.sampleRate=(double)value["sampleRate"]; take.initialPosition=(double)value["initialPosition"]; take.frames=(int64_t)value["frames"];
    take.complete=(bool)value["complete"]; take.issue=value["issue"].toString(); take.masterWav=value["masterWav"].toString(); take.state=TakeState::complete;
    if ((int)value["state"]!=(int)TakeState::complete) { take.complete=false; take.issue="Recovered interrupted take: the recorded prefix is retained. Recording was not resumed automatically."; }
    int64_t lastFrame=-1,lastSequence=-1;
    for (const auto& item : *value["events"].getArray())
    {
        if (!item.isObject() || !number(item["frame"],0,(double)take.frames) || !number(item["sequence"],0,1.e12)
            || !item["kind"].isInt() || !number(item["kind"],0,(int)TakeEventKind::midi)
            || !item["lane"].isInt() || !number(item["lane"],-1,3) || !item["control"].isInt() || !number(item["control"],0,7)
            || !item["stem"].isInt() || !number(item["stem"],-1,3) || !number(item["value"],-1.e9,1.e9) || !number(item["sourcePosition"],0,1.e9)) return fail();
        for (const char* key : {"owner","slot","parameter"}) if (!item[key].isString() || item[key].toString().length()>4096) return fail();
        TakeEvent e; e.frame=(int64_t)item["frame"]; e.sequence=(int64_t)item["sequence"];
        if (e.frame<lastFrame || e.sequence<=lastSequence) return fail(); lastFrame=e.frame; lastSequence=e.sequence;
        e.kind=(TakeEventKind)(int)item["kind"]; e.lane=(int)item["lane"]; e.control=(int)item["control"]; e.stem=(int)item["stem"];
        e.owner=item["owner"].toString(); e.slot=item["slot"].toString(); e.parameter=item["parameter"].toString(); e.value=(double)item["value"];
        e.sourcePosition=(double)item["sourcePosition"]; e.payload=item["payload"];
        if ((e.kind==TakeEventKind::control || e.kind==TakeEventKind::returnAutomation) && !automationRange((AutomationTarget)e.control).contains((float)e.value)
            && (float)e.value!=automationRange((AutomationTarget)e.control).getEnd()) return fail();
        if ((e.kind==TakeEventKind::transport || e.kind==TakeEventKind::mute || e.kind==TakeEventKind::solo) && e.value!=0 && e.value!=1) return fail();
        if (e.kind==TakeEventKind::seek && e.value<0) return fail();
        if (e.kind==TakeEventKind::tempo && (e.value<20 || e.value>400)) return fail();
        if ((e.kind==TakeEventKind::project || e.kind==TakeEventKind::pluginState) && !e.payload.isObject()) return fail();
        take.events.push_back(std::move(e));
    }
    out=std::move(take); return juce::Result::ok();
}
}
