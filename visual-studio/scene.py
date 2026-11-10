"""Visual Studio's portable scene and look format."""

from dataclasses import dataclass
from hashlib import sha256
import json
import math
from pathlib import Path
import re


def read(path, limit=1024 * 1024):
    with path.open("rb") as stream:
        data = stream.read(limit + 1)
    if len(data) > limit:
        raise ValueError(f"{path.name} exceeds the {limit // 1024} KB limit.")
    return data


def identifier(value):
    if not isinstance(value, str) or not re.fullmatch(r"[a-zA-Z_]\w*", value, re.ASCII):
        raise ValueError(f"Invalid uniform name {value!r}.")
    return value


def number(value):
    result = float(value)
    if not math.isfinite(result) or abs(result) > 1e9:
        raise ValueError("Control values must be finite numbers within ±1 billion.")
    return result


@dataclass(frozen=True)
class Control:
    name: str
    label: str
    group: str
    description: str
    kind: str
    default: tuple
    low: tuple
    high: tuple
    labels: tuple = ()
    choices: tuple = ()
    mode: str = ""
    rate: float = .01

    def validate(self, value):
        if not isinstance(value, (tuple, list)) or len(value) != len(self.default):
            raise ValueError(f"Invalid shape for {self.label}.")
        values = tuple(number(x) for x in value)
        if any(not a <= x <= b for x, a, b in zip(values, self.low, self.high)):
            raise ValueError(f"{self.label} is outside its allowed range.")
        if self.kind == "dropdown" and values[0] not in self.choices:
            raise ValueError(f"Unknown option for {self.label}.")
        if self.kind == "toggle" and values not in (self.low, self.high):
            raise ValueError(f"Invalid switch value for {self.label}.")
        if self.kind == "integer" and not values[0].is_integer():
            raise ValueError(f"{self.label} requires a whole number.")
        return values

    @classmethod
    def parse(cls, item):
        name = identifier(item["NAME"])
        kind, _, mode = item["TYPE"].lower().partition(" ")
        kind = "slider" if kind == "knob" else kind
        modes = {"slider": ("", "smooth", "speed"), "xy": ("", "smooth", "speed"),
                 "toggle": ("", "smooth"), "color": ("", "smooth"),
                 "dropdown": ("", "smooth"), "bang": ("", "counter", "smooth")}
        if mode not in modes.get(kind, ()):
            raise ValueError(f"Unsupported control type {item['TYPE']} for {name}.")
        length = {"xy": 2, "color": 3}.get(kind, 1)

        def vector(value):
            result = tuple(map(number, value)) if isinstance(value, list) else (number(value),) * length
            if len(result) != length:
                raise ValueError(f"{name} expects {length} control values.")
            return result

        low, high = vector(item.get("MIN", 0)), vector(item.get("MAX", 1))
        labels, choices = (), ()
        if kind == "dropdown":
            labels = tuple(item.get("LABELS", ()))
            choices = tuple(map(number, item.get("VALUES", range(len(labels)))))
            labels = labels or tuple(f"Option {i + 1}" for i in range(len(choices)))
            if (not choices or len(labels) != len(choices) or len(set(choices)) != len(choices)
                    or len(set(labels)) != len(labels)):
                raise ValueError(f"{name} needs unique labels and matching dropdown values.")
            index = number(item.get("DEFAULT", 0))
            if not index.is_integer() or not 0 <= index < len(choices):
                raise ValueError(f"{name} has an invalid default option.")
            default, low, high = (choices[int(index)],), (min(choices),), (max(choices),)
        elif kind == "color":
            default, low, high = vector(item.get("DEF_COLOR", [1, 1, 1])), (0.,) * 3, (1.,) * 3
        else:
            default = low if kind == "bang" else vector(item.get("DEFAULT", item.get("MIN", 0)))
        rate = number(item.get("PARAMS", .01)) if mode == "smooth" else .01
        if not 0 < rate <= 1:
            raise ValueError(f"{name} smoothing must be greater than zero and at most one.")
        control = cls(name, item.get("LABEL", name.replace("_", " ")),
                      item.get("UI_GROUP", "Scene"), item.get("DESCRIPTION", ""),
                      kind, default, low, high, labels, choices, mode, rate)
        control.validate(default)
        return control


@dataclass(frozen=True)
class Scene:
    path: Path
    title: str
    identity: str
    source: str
    controls: dict
    passes: tuple
    images: dict
    media: dict

    @classmethod
    def open(cls, path):
        path = path.resolve()
        if path.is_dir():
            path = path / "main.glsl"
        if path.suffix.lower() == ".milk":
            data = read(path)
            if not re.search(rb"(?im)^\s*\[preset\d*\]", data):
                raise ValueError("The file has no MilkDrop preset header.")
            return cls(path, path.stem, sha256(data).hexdigest(), "", {}, (), {}, {})
        if path.suffix.lower() not in (".glsl", ".frag", ".fs"):
            raise ValueError("Choose a GLSL fragment, scene folder or .milk preset.")
        scene_config = path.parent / "scene.json"
        folder_config, folder_meta = b"{}", {}
        if scene_config.is_file():
            try:
                folder_config = read(scene_config, 256 * 1024)
                folder_meta = json.loads(folder_config.decode("utf-8-sig"))
            except ValueError:
                if path.name == "main.glsl":
                    raise
        if (isinstance(folder_meta, dict) and isinstance(folder_meta.get("key"), str)
                and folder_meta["key"].startswith("original/")
                and folder_meta.get("passSchema") in (1, 2)):
            if path.name == "main.glsl" or any(
                isinstance(item, dict) and path.name in (item.get("shader"), item.get("vertex"))
                for item in folder_meta.get("passes", [])
            ):
                title = folder_meta.get("title", path.parent.name)
                raise ValueError(f"{path.name} belongs to {title}, an Alloy multipass scene.\n\n"
                                 "Open it from Alloy’s Visuals / scenes library. In Studio, choose "
                                 "a Studio scene or a standalone GLSL fragment.")
        sidecar = path.with_suffix(".json")
        if path.name == "main.glsl" and scene_config.is_file():
            config, meta = folder_config, folder_meta
        else:
            config = read(sidecar, 256 * 1024) if sidecar.is_file() else b"{}"
            meta = json.loads(config.decode("utf-8-sig"))
        data = read(path)
        unsupported = [key for key in ("SCRIPT", "CUBEMAPS", "HARD_TRANSITIONS", "SMOOTH_TRANSITIONS")
                       if meta.get(key)]
        if path.name == "main.glsl" and (path.parent / "script.js").exists():
            unsupported.append("script.js")
        if unsupported:
            raise ValueError("Unsupported scene features: " + ", ".join(unsupported))
        controls = {}
        for item in meta.get("CONTROLS", []):
            control = Control.parse(item)
            if control.name in controls:
                raise ValueError(f"Duplicate control {control.name}.")
            controls[control.name] = control
        passes = tuple(meta.get("PASSES", []))
        images = {}
        for item in meta.get("IMAGES", []):
            name = identifier(item["NAME"])
            image = (path.parent / item["PATH"]).resolve()
            if name in images or not image.is_relative_to(path.parent):
                raise ValueError(f"Invalid image binding {name}.")
            images[name] = image
        if len(passes) > 8 or len(images) > 12 or len(controls) > 256:
            raise ValueError("Scene limit is 8 intermediate passes, 12 images and 256 controls.")
        media = meta.get("MEDIA", [])
        return cls(path, meta.get("TITLE", path.parent.stem if path.name == "main.glsl" else path.stem),
                   sha256(data + b"\0" + config).hexdigest(), data.decode("utf-8-sig"),
                   controls, passes, images, media[0] if media else {})


def save_look(path, scene, controls, values):
    look = {"format": 1, "scene": scene.identity, "title": scene.title,
            "controls": {key: value for key, value in values.items() if controls[key].kind != "bang"}}
    with path.open("w", encoding="utf-8") as stream:
        json.dump(look, stream, indent=2)
        stream.write("\n")


def load_look(path, scene, controls):
    look = json.loads(read(path, 128 * 1024).decode("utf-8-sig"))
    if look.get("format") != 1 or look.get("scene") != scene.identity:
        raise ValueError("This look belongs to a different scene or revision.")
    values = {name: control.default for name, control in controls.items()}
    for name, value in look["controls"].items():
        if controls[name].kind != "bang":
            values[name] = controls[name].validate(value)
    return values
