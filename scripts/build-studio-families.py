"""Materialise the Studio scene library from its authored family profiles."""

import json
from math import cos, sin
from pathlib import Path
import re


STUDIO = Path(__file__).resolve().parents[1] / "visual-studio"
AUTHORING = STUDIO / "authoring"
SCENES = STUDIO / "scenes"
ORIGINALS = {
    "Signal Loom": "aurora",
    "Vector Tunnel": "tunnel",
    "Glass Garden": "garden",
    "Shrouded": "shrouded",
    "Contour Bloom": "contour-bloom",
    "Waxfall": "waxfall",
    "Tidal Culture": "tidal-culture",
}
TREATMENTS = {
    "cut": ("Cut depth", "Silhouette cutout"),
    "relief": ("Relief height", "Brightness relief"),
    "pigment": ("Pigment uptake", "Colour transfer"),
    "stamps": ("Stamp depth", "Repeated impressions"),
    "flow": ("Motion transfer", "Artwork-driven movement"),
    "fragments": ("Fragment lift", "Moving fragments"),
    "relay": ("Delay from light", "Beat relay"),
    "memory": ("Trace strength", "Persistent trace"),
    "shadow": ("Shadow depth", "Projected shadow"),
    "prism": ("Spectrum split", "Colour separation"),
    "filaments": ("Filament pull", "Drawn fibres"),
}
HISTORY = {
    "relay": {"TARGET": "nestedHistory", "WIDTH": 20, "HEIGHT": 12, "FILTER": "nearest", "WRAP": "clamp"},
    "memory": {"TARGET": "nestedHistory", "WIDTH": 384, "HEIGHT": 216, "WRAP": "clamp"},
}


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8"))


def json_text(data):
    return json.dumps(data, indent=2, ensure_ascii=False) + "\n"


def slug(title):
    return re.sub(r"[^a-z0-9]+", "-", title.lower()).strip("-")


def adapt_source(source, family):
    body = source.partition("\n")[2].lstrip()
    for before, after in family["edits"]:
        if before not in body:
            raise ValueError(f"{family['title']} is missing source text {before!r}")
        body = body.replace(before, after)
    body, artwork = re.subn(r"float artwork\(vec2 uv\) \{.*?\n\}",
                            "float artwork(vec2 uv) { return librarySignal(uv); }",
                            body, count=1, flags=re.S)
    if artwork != 1 or "vec4 renderMain()" not in body:
        raise ValueError(f"{family['title']} needs an artwork hook and a renderMain function.")
    body = re.sub(r"\b_uvc\b", "libraryCentered()", body)
    body = re.sub(r"\b_uv\b", "libraryPosition", body)
    return body.replace("vec4 renderMain()", "vec4 renderFamily()", 1).rstrip()


def shader_for(family, index, scene, body, common, treatment, pass_count):
    a, b = scene["profile"]
    angle = .31 * family["id"] + .79 * index
    columns = 3.0 + (family["id"] + index) % 3
    rows = 3.0 + (family["id"] * 2 + index) % 3
    power = .88 + .085 * ((family["id"] + index * 2) % 5)
    r, g, b_ink = family["ink"]
    setup = f"""// F{family['id']:02d}-{index:02d} - {family['title']} / {scene['title']}.
// Generated from Studio's authored family profiles and Nested treatments.
const float LIB_A = {a:.5f};
const float LIB_B = {b:.5f};
const vec2 LIB_AXIS = vec2({cos(angle):.5f}, {sin(angle):.5f});
const vec2 LIB_GRID = vec2({columns:.1f}, {rows:.1f});
const float LIB_POWER = {power:.3f};
const vec3 LIB_INK = vec3({r:.4f}, {g:.4f}, {b_ink:.4f});

vec2 libraryPosition;
vec2 libraryCentered() {{
    return (libraryPosition - .5) * vec2(RENDERSIZE.x / RENDERSIZE.y, 1.0);
}}
"""
    layout = f"""vec2 libraryLayout(vec2 u) {{
    vec2 q = u - .5;
    float t = motion;
    float r = length(q * vec2(1.24, 1.0));
    float a = atan(q.y, q.x + .00001) / (2.0 * PI) + .5;
    return {scene['layout']};
}}"""
    setup_passes = ""
    if scene["treatment"] in HISTORY:
        setup_passes += f"    if (PASSINDEX == {pass_count}) return libraryTick();\n"
        mark = "    LibraryMark art = libraryMark(_uv);"
    else:
        mark = """    LibraryMark art = emptyMark();
    if (syn_MediaType >= .5 && imprint > 0.0) art = libraryMark(_uv);"""
    if pass_count:
        setup_passes += f"    if (PASSINDEX < {pass_count}) return renderFamily();\n"
    renderer = f"""vec4 renderMain() {{
    libraryPosition = _uv;
{setup_passes}{mark}
    libraryPosition = libraryMirror(libraryLayout(_uv) + art.shift);
    vec4 surface = renderFamily();
    float material = .50 + .50 * smoothstep(.09, .68, libraryLuma(surface.rgb));
    vec3 pigment = (.18 + surface.rgb * .97) * art.colour;
    vec3 colour = mix(surface.rgb, pigment, clamp(material * art.tint, 0.0, 1.0));
    return vec4(colour * art.shade + material * art.glow, surface.a);
}}"""
    return "\n\n".join(part.strip() for part in (setup, common, treatment, body, layout, renderer)) + "\n"


def make_siblings(family, common, shaders):
    folder = SCENES / family["anchor"]["folder"]
    anchor_source = (folder / "main.glsl").read_text(encoding="utf-8")
    anchor_spec = read_json(folder / "scene.json")
    body = adapt_source(anchor_source, family)
    assets = {}
    entries = [family["anchor"]]
    for index, scene in enumerate(family["siblings"], start=2):
        mode = scene["treatment"]
        label, treatment = TREATMENTS[mode]
        title = scene["title"]
        name = folder.name + "-" + slug(title)
        spec = dict(anchor_spec, TITLE=f"{family['title']} / {title}")
        spec["CONTROLS"] = [
            dict(control, LABEL=label, DESCRIPTION=scene["description"])
            if control["NAME"] == "imprint" else control for control in anchor_spec["CONTROLS"]
        ]
        original_passes = anchor_spec.get("PASSES", [])
        if mode in HISTORY:
            spec["PASSES"] = [*original_passes, HISTORY[mode]]
            spec["CONTROLS"].append({
                "NAME": "clear_nested", "TYPE": "bang", "LABEL": "Clear Nested history",
                "MIN": 0, "MAX": 1, "UI_GROUP": "Nested",
            })
        target = SCENES / name
        assets[target / "main.glsl"] = shader_for(
            family, index, scene, body, common, shaders[mode], len(original_passes))
        assets[target / "scene.json"] = json_text(spec)
        entries.append({
            "title": title, "folder": name, "treatment": treatment, "description": scene["description"],
        })
    return entries, assets


def main():
    treatment_source = (AUTHORING / "nested-treatments.glsl").read_text(encoding="utf-8")
    common, *sections = treatment_source.split("// @treatment ")
    shaders = dict(section.split("\n", 1) for section in sections)
    originals = [{"title": title, "folder": folder} for title, folder in ORIGINALS.items()
                 if (SCENES / folder / "main.glsl").is_file()]
    catalog = {"Studio originals": originals} if originals else {}
    assets = {}
    for family in read_json(AUTHORING / "families.json"):
        if "kept" in family:
            entries = family["kept"]
        elif "existing" in family:
            entries = [family["anchor"], *family["existing"]]
        else:
            entries, generated = make_siblings(family, common, shaders)
            assets.update(generated)
        if entries:
            catalog[family["title"]] = entries
    assets[STUDIO / "scene-library.json"] = json_text(catalog)
    for path, source in assets.items():
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(source, encoding="utf-8", newline="\n")
    print(f"Materialised {len(catalog) - int(bool(originals))} Studio scene families and {len(assets) - 1} sibling assets.")


if __name__ == "__main__":
    main()
