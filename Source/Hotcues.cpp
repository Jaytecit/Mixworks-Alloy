#include "Hotcues.h"
#include "EnergySections.h"
#include <cmath>

namespace mixworks
{
const HotcueMarker* hotcueFor(const Clip& clip, int slot)
{
    if (!clip.hotcues || slot < 0 || slot >= 9 || !(*clip.hotcues)[(size_t) slot]) return nullptr;
    return &*(*clip.hotcues)[(size_t) slot];
}
void initialiseHotcues(Clip& clip)
{
    if (!clip.media || clip.media->duration() <= 0 || !std::isfinite(clip.media->duration())) return;
    clip.hotcues.emplace();
    auto add = [&](int slot, double source)
    { (*clip.hotcues)[(size_t) slot] = HotcueMarker{"seed:" + juce::String::toHexString(clip.id.hashCode64()) + ":" + juce::String(slot), source, false}; };
    add(0, 0); int slot = 1;
    for (const auto& section : clipEnergySections(clip))
        if (slot < 9 && section.sourceSeconds > 0 && section.sourceSeconds < clip.media->duration()) add(slot++, section.sourceSeconds);
    clip.hotcuesPending = false;
}
std::optional<double> hotcueTimelineTime(const Clip& clip, int slot, double near)
{
    const auto* cue = hotcueFor(clip, slot);
    if (!cue || !clip.media || !std::isfinite(cue->sourceSeconds) || cue->sourceSeconds < 0 || cue->sourceSeconds >= clip.media->duration()) return {};
    double elapsed = clip.reversed ? clip.sourceSpan() - (cue->sourceSeconds - clip.offset) : cue->sourceSeconds - clip.offset;
    auto mapped = [&](double source) -> std::optional<double>
    {
        const double local = clip.audioTimelineTime(source);
        if (!std::isfinite(local) || local < -1.e-8 || local >= clip.length
            || std::abs(clip.sourceTimeAt(std::max(0., local)) - cue->sourceSeconds) > 1.e-7) return {};
        return clip.start + std::max(0., local);
    };
    if (clip.loopSpan <= 0) return mapped(elapsed);
    const double within = cue->sourceSeconds - clip.offset;
    if (within < 0 || within >= clip.loopSpan) return {};
    elapsed -= clip.loopPhase;
    const double low = clip.audioSourceElapsed(0);
    const double first = elapsed + std::ceil((low - elapsed) / clip.loopSpan - 1.e-9) * clip.loopSpan;
    if (near >= clip.start && near < clip.end())
    {
        const double wanted = clip.audioSourceElapsed(near - clip.start);
        const auto next = mapped(elapsed + std::ceil((wanted - elapsed) / clip.loopSpan - 1.e-9) * clip.loopSpan);
        if (next) return next;
    }
    return mapped(first);
}
juce::Colour hotcueColour(int slot)
{
    static constexpr std::array<juce::uint32, 9> colours {0xfff4b06a,0xff82c9ed,0xffa9d68c,0xffd3a5ef,0xfff18e9e,0xff73d4c2,0xffdfd779,0xff9caff4,0xffe3aece};
    return juce::Colour(colours[(size_t) juce::jlimit(0, 8, slot)]);
}
namespace
{
juce::Result editableCue(const Project& project, const Clip* clip, int slot)
{
    if (!clip || !clip->media || slot < 0 || slot >= 9) return juce::Result::fail("Select a segment and HC1-HC9 slot.");
    if (!clip->media->hasPlayback() || clip->media->sourceDetailsPending || project.hasPendingPlacement()) return juce::Result::fail("Wait for this segment's audio before editing hotcues.");
    if (project.clipTransitionLocked(clip->id)) return juce::Result::fail("Unlock the transition before editing its segment's hotcues.");
    return juce::Result::ok();
}
}
juce::Result Project::putHotcue(const juce::String& id, int slot, double source)
{
    auto* clip = clipFor(id); const auto ready = editableCue(*this, clip, slot); if (ready.failed()) return ready;
    if (!std::isfinite(source) || source < 0 || source >= clip->media->duration()) return juce::Result::fail("Keep the hotcue inside the source file. Trimmed-out cues can remain saved.");
    const auto* old = hotcueFor(*clip, slot); if (old && std::abs(old->sourceSeconds - source) < 1.e-9) return juce::Result::ok();
    const auto identity = old ? old->id : juce::Uuid().toString();
    if (!clip->hotcues) clip->hotcues.emplace();
    (*clip->hotcues)[(size_t) slot] = HotcueMarker{identity, source, true}; clip->hotcuesPending = false;
    return juce::Result::ok();
}
juce::Result Project::removeHotcue(const juce::String& id, int slot)
{
    auto* clip = clipFor(id); const auto ready = editableCue(*this, clip, slot); if (ready.failed()) return ready;
    if (clip->hotcues) (*clip->hotcues)[(size_t) slot].reset();
    clip->hotcuesPending = false; return juce::Result::ok();
}
juce::Result Project::seedHotcues(const juce::String& id)
{
    auto* clip = clipFor(id); const auto ready = editableCue(*this, clip, 0); if (ready.failed()) return ready;
    initialiseHotcues(*clip); return juce::Result::ok();
}
}
