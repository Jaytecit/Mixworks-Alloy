#include "Model.h"
#include "PresetNodes.h"
#include <cmath>
#include <set>

namespace mixworks
{
namespace
{
bool numeric(const juce::var& x) { return x.isInt() || x.isInt64() || x.isDouble(); }
bool nameValid(const juce::String& name) { return name.isNotEmpty() && name == name.trim() && name.length() <= 128 && !name.containsAnyOf("\r\n\t"); }
}
juce::Result TransitionStyle::validate() const
{
    if (nodes)
    {
        const auto valid = nodes->validate(); if (valid.failed()) return valid;
        if (bassPositions || filter.active() || volume != std::array<double,4>{0,0,1,1}
            || std::any_of(bands.begin(),bands.end(),[](const auto& b) { return b.has_value(); }))
            return juce::Result::fail("A node preset cannot also contain procedural treatments.");
    }
    const auto filterValid = filter.validate(); if (filterValid.failed()) return filterValid;
    for (int i = 0; i < 4; ++i) if (bands[(size_t) i])
    { const auto r = bands[(size_t) i]->validate(i); if (r.failed()) return r; }
    const auto unit = [](double x) { return std::isfinite(x) && x >= 0 && x <= 1; };
    if (!std::all_of(volume.begin(), volume.end(), unit) || volume[0] > volume[1] || volume[2] > volume[3] || volume[3] != 1)
        return juce::Result::fail("This preset requires fades contained in its transition frame.");
    if (bassPositions && (bands[1] || !std::all_of(bassPositions->begin(), bassPositions->end(), unit)
        || (*bassPositions)[0] >= (*bassPositions)[1] || (*bassPositions)[2] >= (*bassPositions)[3]))
        return juce::Result::fail("Invalid preset bass ownership or positions.");
    for (float value : bassValues) if (!std::isfinite(value) || value < -60 || value > 12) return juce::Result::fail("Invalid preset bass value.");
    return juce::Result::ok();
}
juce::Result Project::captureTransitionStyle(const juce::String& incoming, TransitionStyle& result) const
{
    const auto valid = validateTransitionLinks(); if (valid.failed()) return valid;
    const auto* link = transitionFor(incoming); if (!link) return juce::Result::fail("Select a transition first.");
    const auto range = transitionRange(*link); if (range.getLength() <= 1e-6 || hasPendingPlacement()) return juce::Result::fail("Wait for a non-zero, ready transition.");
    const auto* out = clipFor(link->outgoing); const auto* in = clipFor(incoming);
    const auto normal = [&](double time) { const double x = (time - range.getStart()) / range.getLength(); return std::abs(x) < 1e-9 ? 0. : std::abs(x - 1) < 1e-9 ? 1. : x; };
    TransitionStyle style; style.bands = link->bands; style.filter = link->filter;
    const auto a = in->envelope(), b = out->envelope();
    style.volume = {normal(in->start + a.intro), normal(in->start + a.intro + a.fadeIn), normal(out->start + b.outro), normal(out->start + b.outro + b.fadeOut)};
    if (link->bassSwap)
    {
        style.bassPositions.emplace();
        for (size_t side = 0; side < 2; ++side)
        {
            const auto* clip = side == 0 ? out : in; const auto& curve = effectSettings(*clip).automation[1]; style.bassEnabled[side] = curve.enabled;
            for (size_t end = 0; end < 2; ++end)
            {
                const auto i = side * 2 + end; const double source = (*link->bassSwap)[i];
                (*style.bassPositions)[i] = normal(clip->start + clip->localTimeForSource(source));
                const auto point = std::find_if(curve.points.begin(), curve.points.end(), [&](const auto& p) { return std::abs(p.time - source) < 1e-7; });
                if (point == curve.points.end()) return juce::Result::fail("Owned bass point is unavailable.");
                style.bassValues[i] = point->value;
            }
        }
    }
    style.nodes.emplace(); const auto captured = capturePresetNodes(*this,incoming,*style.nodes); if (captured.failed()) return captured;
    style.bands = {}; style.filter = {}; style.bassPositions.reset(); style.volume = {0,0,1,1};
    style.bassValues = TransitionStyle{}.bassValues; style.bassEnabled = TransitionStyle{}.bassEnabled;
    const auto checked = style.validate(); if (checked.failed()) return checked;
    result = style; return juce::Result::ok();
}
juce::Result Project::applyNamedTransitionStyle(const juce::String& incoming, const TransitionStyle& style, const juce::String& name)
{
    const auto* link = transitionFor(incoming);
    if (!link || hasPendingPlacement()) return juce::Result::fail("Select a ready transition first.");
    if (clipTransitionLocked(link->outgoing) || clipTransitionLocked(incoming))
        return juce::Result::fail("Unlock both tracks before applying a preset.");
    const auto* out = clipFor(link->outgoing);
    const auto* in = clipFor(incoming);
    if (!out || !in || !out->media || !in->media) return juce::Result::fail("Select two ready tracks first.");
    const auto range = transitionRange(*link);
    const double join = out->start + out->transitionTime(false);
    const double inMarker = in->start + in->transitionTime(true);
    const double begin = std::max(join,std::max(out->start,in->start));
    const double end = std::min({out->end(),in->end(),begin + range.getLength()});
    if (std::abs(join-inMarker) > 1.e-7 || begin-join > 1.e-7 || end-begin <= 1.e-6)
        return juce::Result::fail("Align OUT and IN inside the shared audio and leave room after the join for the preset.");
    Project candidate = *this;
    if (link->presetOwner.isNotEmpty())
    {
        const auto removed = candidate.removeOwnedTransitionPreset(incoming);
        if (removed.failed()) return removed;
    }
    auto& edited = candidate.transitionLinks[(size_t)(link-transitionLinks.data())];
    edited.frame = std::array<double,2>{out->sourceElapsed(begin-out->start),out->sourceElapsed(end-out->start)};
    const auto applied = candidate.applyTransitionStyle(incoming,style,name);
    if (applied.failed()) return applied;
    const auto locked = validateTransitionLocks(candidate); if (locked.failed()) return locked;
    *this = std::move(candidate);
    return juce::Result::ok();
}
juce::Result Project::applyTransitionStyle(const juce::String& incoming, const TransitionStyle& style, const juce::String& presetName)
{
    auto valid = style.validate(); if (valid.failed()) return valid;
    if (style.nodes) return presetName.isEmpty() ? applyPresetNodes(*this,incoming,*style.nodes)
        : applyOwnedPresetNodes(*this,incoming,*style.nodes,presetName);
    valid = validateTransitionLinks(); if (valid.failed()) return valid;
    const auto* current = transitionFor(incoming); if (!current || hasPendingPlacement()) return juce::Result::fail("Select a ready transition first.");
    if (clipTransitionLocked(current->outgoing) || clipTransitionLocked(incoming)) return juce::Result::fail("Unlock transitions on both participating tracks before applying a treatment.");
    const auto range = transitionRange(*current);
    auto candidate = *this; auto& link = candidate.transitionLinks[(size_t) (current - transitionLinks.data())];
    auto* out = candidate.clipFor(link.outgoing); auto* in = candidate.clipFor(incoming);
    if (range.getLength() <= 1e-6 || range.getStart() < std::max(out->start, in->start) - 1e-7 || range.getEnd() > std::min(out->end(), in->end()) + 1e-7)
        return juce::Result::fail("Keep the transition inside both songs before applying a treatment.");
    // Reapplying an identical treatment must leave surrounding/manual data exact.
    TransitionStyle existing; if (presetName.isEmpty() && captureTransitionStyle(incoming, existing).wasOk() && existing == style) return juce::Result::ok();
    // Compile an isolated template, then replace only the selected frame in
    // the original project. Existing ordinary nodes are not generator input.
    candidate.transitionLinks.erase(std::remove_if(candidate.transitionLinks.begin(),candidate.transitionLinks.end(),
        [&](const auto& other) { return other.incoming != incoming; }),candidate.transitionLinks.end());
    auto& templateLink = candidate.transitionLinks.front();
    templateLink.locked = false; templateLink.bassSwap.reset(); templateLink.bands = style.bands; templateLink.filter = style.filter;
    out = candidate.clipFor(current->outgoing); in = candidate.clipFor(incoming);
    for (auto* clip : {out,in})
    {
        // Active source curves live in effectControls after the first edit or
        // load. Clearing only Clip.controls leaves the previous treatment in
        // this private compiler and multiplies it into the replacement.
        candidate.effectSettings(*clip).automation = {}; clip->controls.automation = {};
        clip->fadeIn = clip->fadeOut = 0;
    }
    const auto time = [&](double fraction) { return range.getStart() + range.getLength() * fraction; };
    in->fadeInAnchor = in->sourceElapsed(time(style.volume[0]) - in->start); in->fadeIn = time(style.volume[1]) - time(style.volume[0]);
    out->fadeOutAnchor = out->sourceElapsed(time(style.volume[2]) - out->start); out->fadeOut = range.getEnd() - time(style.volume[2]);
    if (style.bassPositions)
    {
        std::array<double, 4> sources;
        for (size_t side = 0; side < 2; ++side)
        {
            auto* clip = side == 0 ? out : in; auto& curve = candidate.effectSettings(*clip).automation[1];
            for (size_t end = 0; end < 2; ++end) sources[side * 2 + end] = clip->sourceElapsed(time((*style.bassPositions)[side * 2 + end]) - clip->start);
            const double first = sources[side * 2], last = sources[side * 2 + 1];
            if (std::any_of(curve.points.begin(), curve.points.end(), [&](const auto& point) { return point.time >= first - 1e-7 && point.time <= last + 1e-7; }))
                return juce::Result::fail("Manual or neighbouring bass points overlap this preset's ramp. They were kept.");
            // Enabling a disabled curve with unrelated points would change them.
            if (!curve.points.empty() && curve.enabled != style.bassEnabled[side]) return juce::Result::fail("This preset would change the enabled state of manual bass automation.");
            curve.enabled = style.bassEnabled[side];
            for (size_t end = 0; end < 2; ++end) curve.setPoint(sources[side * 2 + end], style.bassValues[side * 2 + end]);
        }
        templateLink.bassSwap = sources;
    }
    valid = materialisePresetNodes(candidate,incoming); if (valid.failed()) return valid;
    TransitionStyle::Nodes nodes; valid = capturePresetNodes(candidate,incoming,nodes); if (valid.failed()) return valid;
    return presetName.isEmpty() ? applyPresetNodes(*this,incoming,nodes) : applyOwnedPresetNodes(*this,incoming,nodes,presetName);
}
juce::Result Project::storeTransitionAlternative(const juce::String& incoming, int slot)
{
    if (slot < 0 || slot >= 3) return juce::Result::fail("Choose A, B or C.");
    TransitionStyle style; const auto r = captureTransitionStyle(incoming, style); if (r.failed()) return r;
    for (auto& link : transitionLinks) if (link.incoming == incoming) link.alternatives[(size_t) slot] = style;
    return juce::Result::ok();
}
juce::Result Project::putTransitionPreset(const NamedTransitionStyle& preset)
{
    if (!nameValid(preset.name)) return juce::Result::fail("Use a preset name of 1 to 128 characters without line breaks.");
    const auto valid = preset.style.validate(); if (valid.failed()) return valid;
    for (auto& item : transitionPresets) if (item.name == preset.name) { item.style = preset.style; return juce::Result::ok(); }
    transitionPresets.push_back(preset); return juce::Result::ok();
}
juce::var transitionStyleToJson(const TransitionStyle& style)
{
    auto* obj = new juce::DynamicObject(); Project::TransitionLink link; link.bands = style.bands; link.filter = style.filter;
    obj->setProperty("bands", transitionBandsToJson(link));
    obj->setProperty("filter", transitionFilterToJson(style.filter));
    if (style.nodes) obj->setProperty("nodes",presetNodesToJson(*style.nodes));
    juce::Array<juce::var> volume; for (double x : style.volume) volume.add(x); obj->setProperty("volume", volume);
    if (style.bassPositions)
    {
        juce::Array<juce::var> positions, values, enabled;
        for (double x : *style.bassPositions) positions.add(x); for (float x : style.bassValues) values.add((double) x); for (bool x : style.bassEnabled) enabled.add(x);
        obj->setProperty("bassPositions", positions); obj->setProperty("bassValues", values); obj->setProperty("bassEnabled", enabled);
    }
    return juce::var(obj);
}
juce::Result readTransitionStyle(const juce::var& data, TransitionStyle& result)
{
    const auto fail = [] { return juce::Result::fail("Invalid transition treatment."); };
    if (!data.isObject()) return fail();
    TransitionStyle style; Project::TransitionLink link; auto valid = readTransitionBands(data["bands"], link); if (valid.failed()) return valid; style.bands = link.bands;
    if (data.hasProperty("nodes")) { style.nodes.emplace(); valid = readPresetNodes(data["nodes"],*style.nodes); if (valid.failed()) return valid; }
    if (data.hasProperty("filter")) { valid = readTransitionFilter(data["filter"], style.filter); if (valid.failed()) return valid; }
    const auto volume = data["volume"]; if (!volume.isArray() || volume.size() != 4) return fail();
    for (int i = 0; i < 4; ++i) { if (!numeric(volume[i])) return fail(); style.volume[(size_t) i] = (double) volume[i]; }
    if (data.hasProperty("bassPositions"))
    {
        const auto p = data["bassPositions"], v = data["bassValues"], e = data["bassEnabled"];
        if (!p.isArray() || p.size() != 4 || !v.isArray() || v.size() != 4 || !e.isArray() || e.size() != 2) return fail();
        style.bassPositions.emplace();
        for (int i = 0; i < 4; ++i) { if (!numeric(p[i]) || !numeric(v[i])) return fail(); (*style.bassPositions)[(size_t) i] = (double) p[i]; style.bassValues[(size_t) i] = (float) (double) v[i]; }
        for (int i = 0; i < 2; ++i) { if (!e[i].isBool()) return fail(); style.bassEnabled[(size_t) i] = (bool) e[i]; }
    }
    else if (data.hasProperty("bassValues") || data.hasProperty("bassEnabled")) return fail();
    valid = style.validate(); if (valid.failed()) return valid; result = style; return juce::Result::ok();
}
juce::var namedTransitionStylesToJson(const std::vector<NamedTransitionStyle>& presets)
{
    juce::Array<juce::var> rows;
    for (const auto& preset : presets) { auto* obj = new juce::DynamicObject(); obj->setProperty("name", preset.name); obj->setProperty("style", transitionStyleToJson(preset.style)); rows.add(juce::var(obj)); }
    return rows;
}
juce::Result readNamedTransitionStyles(const juce::var& data, std::vector<NamedTransitionStyle>& result)
{
    if (!data.isArray()) return juce::Result::fail("Invalid named transition presets.");
    std::vector<NamedTransitionStyle> parsed; std::set<juce::String> names;
    for (const auto& row : *data.getArray())
    {
        NamedTransitionStyle item; if (!row.isObject() || !row["name"].isString()) return juce::Result::fail("Invalid preset name."); item.name = row["name"].toString();
        if (!nameValid(item.name) || !names.insert(item.name).second) return juce::Result::fail("Invalid or duplicate preset name.");
        const auto r = readTransitionStyle(row["style"], item.style); if (r.failed()) return r; parsed.push_back(item);
    }
    result = std::move(parsed); return juce::Result::ok();
}
juce::Result writeTransitionPreset(const juce::File& file, const NamedTransitionStyle& preset)
{
    Project checked; const auto valid = checked.putTransitionPreset(preset); if (valid.failed()) return valid;
    auto* root = new juce::DynamicObject(); root->setProperty("format", "alloy-transition-preset"); root->setProperty("version", 7); root->setProperty("presets", namedTransitionStylesToJson({preset}));
    const auto text = juce::JSON::toString(juce::var(root), juce::JSON::FormatOptions().withMaxDecimalPlaces(17));
    juce::TemporaryFile temp(file); if (!temp.getFile().replaceWithText(text)) return juce::Result::fail("Cannot write transition preset.");
    if (file.existsAsFile() && !file.copyFileTo(file.getSiblingFile(file.getFileName() + ".bak").getNonexistentSibling())) return juce::Result::fail("Cannot back up the existing preset.");
    return temp.overwriteTargetFileWithTemporary() ? juce::Result::ok() : juce::Result::fail("Cannot finish transition preset export.");
}
juce::Result readTransitionPreset(const juce::File& file, NamedTransitionStyle& result)
{
    // Input expansion is bounded before parsing a standalone preset file.
    if (!file.existsAsFile() || file.getSize() > 1024 * 1024) return juce::Result::fail("Preset is missing or exceeds the 1 MiB input budget.");
    const auto data = juce::JSON::parse(file);
    if (!data.isObject() || data["format"].toString() != "alloy-transition-preset" || !data["version"].isInt() || ((int) data["version"] < 1 || (int) data["version"] > 7)) return juce::Result::fail("Unsupported transition preset file.");
    std::vector<NamedTransitionStyle> presets; const auto valid = readNamedTransitionStyles(data["presets"], presets); if (valid.failed()) return valid;
    if (presets.size() != 1) return juce::Result::fail("A portable preset must contain one named treatment.");
    result = presets.front(); return juce::Result::ok();
}
}
