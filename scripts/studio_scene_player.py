"""Alloy's Studio layer renderer. Transport and automation arrive from Alloy."""

import base64
from pathlib import Path

import numpy as np
from OpenGL import GL as gl
from PIL import Image, ImageOps

from studio_engine.audio import Audio, AudioFrame, OFFSETS, RATE
from studio_engine.engine import Engine, MilkDrop, Shader, Target, compile_program, upload_texture
from studio_engine.media import MEDIA, META, Media
from studio_engine.scene import Scene
from milkdrop_catalogue import catalogue, validate_source


BLEND = """#version 330 core
uniform sampler2D background, scene, milk;
uniform float hasScene, opacity, milkMix;
uniform vec2 outputSize;
out vec4 colour;
void main() {
    vec2 uv = gl_FragCoord.xy / outputSize;
    vec4 bg = texture(background, uv);
    vec4 visual = texture(scene, uv);
    vec3 base = mix(bg.rgb * mix(opacity, 1., hasScene), visual.rgb, visual.a * hasScene);
    base = mix(base, 1. - (1. - base) * (1. - bg.rgb), bg.a * opacity * hasScene);
    if (milkMix > 0.) {
        vec2 drift = .04 * (1. - milkMix) * (visual.rg - visual.gb);
        vec3 current = texture(milk, clamp(uv + drift, 0., 1.)).rgb;
        base = mix(base, current, milkMix);
    }
    colour = vec4(base, max(bg.a, visual.a * hasScene));
}
"""


class TransportAudio:
    def __init__(self):
        self.pcm = np.zeros(len(OFFSETS), dtype=np.float32)
        self.rate = RATE
        self.beat, self.bpm = 0., 120.

    def clock(self):
        return self.bpm, self.bpm, self.beat % 1

    def samples(self, offsets):
        if self.rate == RATE:
            return self.pcm
        return np.interp(len(self.pcm) + offsets * self.rate, np.arange(len(self.pcm)), self.pcm, left=0)


def values_for(controls, overrides):
    result = {}
    for name, control in controls.items():
        axes = ("_x", "_y", "_z") if len(control.default) > 1 else ("",)
        parts = []
        for index, (default, low, high) in enumerate(zip(control.default, control.low, control.high)):
            key = name + axes[index]
            if key not in overrides or control.kind == "bang":
                parts.append(default)
                continue
            value = min(1., max(0., float(overrides[key])))
            if control.kind == "dropdown":
                parts.append(control.choices[round(value * (len(control.choices) - 1))])
            elif control.kind == "toggle":
                parts.append(high if value >= .5 else low)
            elif control.kind == "integer":
                parts.append(round(low + value * (high - low)))
            else:
                parts.append(low + value * (high - low))
        result[name] = tuple(parts)
    return result


def parameter_specs(controls):
    result = {}
    for name, control in controls.items():
        if control.kind == "bang":
            continue
        axes = ("x", "y", "z") if len(control.default) > 1 else ("",)
        for axis, default, low, high in zip(axes, control.default, control.low, control.high):
            key = f"{name}_{axis}" if axis else name
            label = f"{control.label} {axis.upper()}" if axis else control.label
            steps = 2 if control.kind == "toggle" else len(control.choices)
            if control.kind == "dropdown":
                initial = control.choices.index(default) / max(1, steps - 1)
            else:
                initial = (default - low) / (high - low) if high > low else 0.
            result[key] = [initial, steps, label]
    return result


class StudioScenePlayer:
    presenter = None

    def __init__(self, install, size, library):
        self.install, self.library = install, library
        self.milk_entries = {}
        self.engine = Engine(install / "milkdrop", size)
        self.size = size
        self.assets = {}
        self.prepared, self.prepare_attempts = {}, {}
        self.clock = TransportAudio()
        self.analysis = Audio()
        self.analysis.mode, self.analysis.live = "Alloy", self.clock
        self.position, self.generation = None, None
        try:
            composite = self.engine.composite
            composite.programs["studio"] = compile_program(BLEND)
            composite.targets["studio"] = Target(size, {"FLOAT": False, "WRAP": "clamp"}, buffers=1)
        except Exception:
            self.engine.close()
            raise

    def prepare(self, upcoming):
        upcoming = upcoming or {}
        # Bound GPU ownership to the current and next actor on each of four
        # layers. Prepare only one resource after each delivered frame.
        for layer in list(self.prepared):
            if layer not in upcoming or self.prepared[layer][0] != self.identity(upcoming[layer]):
                self.dispose(self.prepared.pop(layer)[1])
        for layer in list(self.prepare_attempts):
            if layer not in upcoming or self.prepare_attempts[layer] != self.identity(upcoming[layer]):
                del self.prepare_attempts[layer]
        for layer, actor in upcoming.items():
            if layer not in ("background", "sprite", "shader", "milk"):
                continue
            identity = self.identity(actor)
            if (self.assets.get(layer, (None,))[0] == identity or layer in self.prepared
                    or self.prepare_attempts.get(layer) == identity):
                continue
            self.prepare_attempts[layer] = identity
            try:
                self.prepared[layer] = identity, self.create_asset(layer, actor)
            except Exception:
                # A speculative failure belongs to its future actor. The current
                # output remains usable; a real activation still reports failure.
                pass
            return

    @staticmethod
    def identity(actor):
        return actor["clip"], actor["scene"], actor["revision"]

    @staticmethod
    def dispose(asset):
        if isinstance(asset, Media):
            texture = getattr(asset, "studio_texture", 0)
            if texture:
                gl.glDeleteTextures([texture])
            asset.clear()
        else:
            asset.close()

    def reset_timing(self):
        for _, asset in self.assets.values():
            if not isinstance(asset, Media):
                asset.reset()
        self.analysis.reset()
        self.position = None

    def release_active(self):
        for _, asset in self.assets.values():
            self.dispose(asset)
        self.assets.clear()

    def reset(self):
        self.release_active()
        for _, asset in self.prepared.values():
            self.dispose(asset)
        self.prepared.clear()
        self.prepare_attempts.clear()
        self.analysis.reset()
        self.position = None

    def asset(self, layer, actor):
        if not actor:
            current = self.assets.pop(layer, None)
            if current:
                self.dispose(current[1])
            return None
        identity = self.identity(actor)
        current = self.assets.get(layer)
        if current and current[0] == identity:
            return current[1]
        prepared = self.prepared.get(layer)
        if prepared and prepared[0] == identity:
            asset = self.prepared.pop(layer)[1]
        else:
            asset = self.create_asset(layer, actor)
        if current:
            self.dispose(current[1])
        self.assets[layer] = identity, asset
        return asset

    def create_asset(self, layer, actor):
        key = actor["scene"]
        if layer in ("background", "sprite"):
            asset = Media()
            bundled = {"studio-bg/copper": "Copper Background.png", "studio-sprite/circuit": "Circuit Sprite.png",
                       "studio-sprite/flare": "Flare Sprite.png"}
            path = self.install / "studio" / bundled[key] if key in bundled else Path(key.split("/", 1)[1])
            asset.open(path)
            if layer == "background":
                try:
                    crop = ImageOps.fit(asset.image, self.size, method=Image.Resampling.BILINEAR)
                    asset.studio_texture = upload_texture(crop.transpose(Image.Transpose.FLIP_TOP_BOTTOM))
                except Exception:
                    asset.clear()
                    raise
        elif layer == "shader":
            path = self.install / "studio" / "scenes" / key.split("/", 1)[1] if key.startswith("studio/") else Path(key.split("/", 1)[1])
            asset = Shader(Scene.open(path), self.size, self.engine.vao)
        else:
            if key == "studio-milk/prism":
                path = self.install / "studio" / "Prism Current.milk"
            else:
                if key not in self.milk_entries:
                    self.milk_entries = catalogue(self.library)
                entry = self.milk_entries[key]
                validate_source(entry)
                path = entry["preset"]
            asset = MilkDrop(Scene.open(Path(path)), self.size, self.install / "milkdrop",
                             texture_paths=entry["texturePaths"] if key.startswith("milkdrop/") else ())
        return asset

    @staticmethod
    def reset_gl(samplers):
        for flag in (gl.GL_BLEND, gl.GL_DEPTH_TEST, gl.GL_SCISSOR_TEST, gl.GL_CULL_FACE):
            gl.glDisable(flag)
        gl.glColorMask(True, True, True, True)
        gl.glPixelStorei(gl.GL_PACK_ALIGNMENT, 1)
        for unit in range(samplers):
            gl.glBindSampler(unit, 0)

    def render(self, request):
        actors = request["studioLayers"]
        position, generation = request["position"], request["generation"]
        if not actors:
            if self.assets:
                self.release_active()
            self.analysis.reset()
            self.position, self.generation = position, generation
            return dict(ok=True, empty=True, generation=generation, scenes=[], studio=True), b""
        dt = position - self.position if self.position is not None else 0.
        if generation != self.generation or dt < 0 or dt > .35:
            self.reset_timing()
            dt = 0.
        self.generation, self.position = generation, position
        self.clock.pcm = np.frombuffer(base64.b64decode(request["pcm"], validate=True), dtype="<f4")
        self.clock.rate, self.clock.beat, self.clock.bpm = request["rate"], request["beat"], request["bpm"]
        audio = self.analysis.frame(position, dt)
        advance = dt > 0 and request["playing"]
        composite = self.engine.composite
        empty = composite.empty
        sprite_info, bg_info, milk_info, shader_info = (actors.get(layer) for layer in ("sprite", "background", "milk", "shader"))
        shader = self.asset("shader", shader_info)
        milk = self.asset("milk", milk_info)
        sampler_count = max(3, len(shader.samplers) if shader else 0)
        self.reset_gl(sampler_count)
        bg = self.asset("background", bg_info)
        background = empty
        if bg:
            background = bg.studio_texture
        sprite = self.asset("sprite", sprite_info)
        sprite_values = values_for(MEDIA, sprite_info["controls"] if sprite_info else {})
        nested = None
        if sprite and sprite_values["spriteRoute"][0]:
            nested = shader.nested_route if shader else "milk" if milk and milk.nested_route else None
        artwork = composite.media(sprite, sprite_values, shader.media_spec if nested == "media" else {},
                                  sprite_info["time"], audio, nested=bool(nested)) if sprite else empty
        if milk:
            count = max(1, min(len(audio.pcm), round(dt * RATE)))
            milk.render(AudioFrame(audio.pcm[-count:], audio.uniforms), values_for(milk.controls, milk_info["controls"]),
                        advance, (lambda: composite.draw_sprite(artwork)) if nested == "milk" else None)
            milk_texture = composite.milk_texture()
            self.reset_gl(sampler_count)
        else:
            milk_texture = 0
        source = milk_texture or empty
        milk_mix = shader_info["controls"].get("milkMix", .35) if shader and milk_texture else 0.
        if shader:
            target = shader.targets["syn_FinalPass"]
            feedback = composite.seed_feedback(target, artwork) if nested == "feedback" else 0
            shader.render(shader_info["time"], dt, audio, values_for(shader.controls, shader_info["controls"]),
                          ((0, 0), (0, 0), False), advance, artwork if nested == "media" else empty,
                          float(nested == "media"), feedback, milk_texture=milk_texture, milk_amount=4 * milk_mix * (1 - milk_mix))
            source = target.textures[target.front]
        opacity = bg_info["controls"].get("backgroundMix", .38) if bg_info else 0.
        fallback_mix = milk_mix if shader else 0.
        result = composite.draw("studio", {"background": background, "scene": source, "milk": milk_texture or empty},
                                {"hasScene": (float(bool(shader or milk)),), "opacity": (opacity,), "milkMix": (fallback_mix,)})
        grading = shader_info or milk_info
        meta = values_for(META, grading["controls"] if grading else {})
        composite.draw("output", {"source": result, "media": empty if nested else artwork},
                       {**meta, "overlayMode": sprite_values["overlayMode"]})
        gl.glReadBuffer(gl.GL_COLOR_ATTACHMENT0)
        pixels = gl.glReadPixels(0, 0, *self.size, gl.GL_BGRA, gl.GL_UNSIGNED_BYTE)
        facts = dict(ok=True, generation=generation, sequence=request.get("sequence", 0),
                     width=self.size[0], height=self.size[1], pixelFormat="bgra8-bottom-up",
                     scenes=list(actors.values()), studio=True)
        if shader:
            facts.update(shaderClip=shader_info["clip"], shaderControls=parameter_specs(shader.controls))
        return facts, pixels

    def close(self):
        self.reset()
        self.engine.close()
