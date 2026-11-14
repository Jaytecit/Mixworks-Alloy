"""Alloy's Studio layer renderer. Transport and automation arrive from Alloy."""

import base64
from pathlib import Path

import glfw
import numpy as np
from OpenGL import GL as gl
from PIL import Image

from studio_engine.audio import Audio, AudioFrame, OFFSETS, RATE
from studio_engine.engine import Engine, MilkDrop, Shader, Target, compile_program, upload_texture
from studio_engine.media import MEDIA, META, Media
from studio_engine.scene import Scene
from milkdrop_catalogue import catalogue, validate_source


BLEND = """#version 330 core
uniform sampler2D background, scene, milk;
uniform float hasScene, opacity, milkMix;
uniform vec2 outputSize, backgroundSize;
out vec4 colour;
void main() {
    vec2 uv = gl_FragCoord.xy / outputSize;
    float aspect = (outputSize.x * backgroundSize.y) / (outputSize.y * backgroundSize.x);
    vec4 bg = texture(background, (uv - .5) * min(vec2(aspect, 1. / aspect), 1.) + .5);
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

JOIN = """#version 330 core
uniform sampler2D previousFrame, nextFrame;
uniform float progress, bassHandoff;
uniform vec2 outputSize;
out vec4 colour;
void main() {
    vec2 uv = gl_FragCoord.xy / outputSize;
    float front = uv.x * .7 + uv.y * .3;
    float wipe = smoothstep(front - .12, front + .12, progress * 1.24 - .12);
    float amount = mix(smoothstep(0., 1., progress), wipe, bassHandoff);
    colour = mix(texture(previousFrame, uv), texture(nextFrame, uv), amount);
}
"""

AUDIO_LEVELS = {"bass": "syn_BassLevel", "energy": "syn_Level"}


def effective_actor(actor, audio):
    if not actor:
        return None
    controls = actor["controls"].copy()
    response = {}
    held = actor.get("held", {})
    for name, rule in actor.get("audio", {}).items():
        if name not in held:
            level = audio.uniforms[AUDIO_LEVELS[rule["source"]]][0]
            response[name] = min(1., max(0., controls.get(name, .5) + level * rule["amount"]))
    controls.update(response)
    controls.update(held)
    return {**actor, "controls": controls, "audioValues": response}


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
            steps = (2 if control.kind == "toggle" else max(1, round(high - low) + 1)
                     if control.kind == "integer" or high <= low else len(control.choices))
            if control.kind == "dropdown":
                initial = control.choices.index(default) / max(1, steps - 1)
            else:
                initial = (default - low) / (high - low) if high > low else 0.
            result[key] = [initial, steps, label]
    return result


class StudioScenePlayer:
    presenter = None

    def __init__(self, install, size, library, mailbox=None):
        self.install, self.library = install, library
        self.mailbox = mailbox
        self.output_error = None
        self.milk_entries = {}
        self.engine = Engine(install / "milkdrop", size)
        self.assets = {}
        self.prepared, self.prepare_attempts = {}, {}
        self.clock = TransportAudio()
        self.analysis = Audio()
        self.analysis.mode, self.analysis.live = "Alloy", self.clock
        self.position, self.generation = None, None
        try:
            composite = self.engine.composite
            composite.programs["studio"] = compile_program(BLEND)
            composite.programs["join"] = compile_program(JOIN)
            for name in ("studio", "join", "outgoing", "incoming"):
                composite.targets[name] = Target(size, {"FLOAT": False, "WRAP": "clamp"}, buffers=1)
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

    def return_preview(self, token):
        if self.mailbox:
            try:
                (self.mailbox / f"attach-{token}").touch()
            except OSError:
                pass  # Alloy may have already closed the Studio session.

    def configure_output(self, output):
        token = output["token"] if output else None
        if self.output_error and self.output_error[0] == token:
            return
        self.output_error = None
        assets = [asset for _, asset in (*self.assets.values(), *self.prepared.values())]
        try:
            self.engine.set_output(output, self.return_preview, assets)
        except Exception as error:
            self.output_error = token, str(error)

    def output_facts(self, *, empty=False):
        if self.output_error:
            token, error = self.output_error
            return dict(outputError=error, outputToken=token)
        if self.engine.present_output(empty=empty):
            facts = dict(outputWidth=self.engine.size[0], outputHeight=self.engine.size[1],
                         outputToken=self.engine.output["token"])
            if self.engine.counter:
                facts["outputFps"] = self.engine.counter.value
            return facts
        return {}

    def asset(self, layer, actor, *, outgoing=False):
        slot = f"{layer}:outgoing" if outgoing else layer
        if not actor:
            current = self.assets.pop(slot, None)
            if current:
                self.dispose(current[1])
            return None
        identity = self.identity(actor)
        current = self.assets.get(slot)
        if current and current[0] == identity:
            return current[1]
        other = layer if outgoing else f"{layer}:outgoing"
        alternate = self.assets.get(other)
        prepared = self.prepared.get(layer)
        if alternate and alternate[0] == identity:
            asset = self.assets.pop(other)[1]
        elif prepared and prepared[0] == identity:
            asset = self.prepared.pop(layer)[1]
        else:
            asset = self.create_asset(layer, actor)
        if current:
            self.dispose(current[1])
        self.assets[slot] = identity, asset
        return asset

    def create_asset(self, layer, actor):
        key = actor["scene"]
        if layer in ("background", "sprite"):
            asset = Media()
            bundled = {"studio-bg/copper": "Copper Background.png", "studio-sprite/circuit": "Circuit Sprite.png",
                       "studio-sprite/flare": "Flare Sprite.png"}
            group, name = key.split("/", 1)
            if key in bundled:
                path = self.install / "studio" / bundled[key]
            elif group in ("studio-bg", "studio-sprite"):
                path = self.install / ("Sprites" if layer == "sprite" else "Backgrounds") / name
            else:
                path = Path(name)
            asset.open(path)
            if layer == "background":
                try:
                    asset.studio_texture = upload_texture(asset.image.transpose(Image.Transpose.FLIP_TOP_BOTTOM))
                except Exception:
                    asset.clear()
                    raise
        elif layer == "shader":
            path = self.install / "studio" / "scenes" / key.split("/", 1)[1] if key.startswith("studio/") else Path(key.split("/", 1)[1])
            asset = Shader(Scene.open(path), self.engine.size, self.engine.vao, preview_size=self.engine.preview_size)
        else:
            if key == "studio-milk/prism":
                path = self.install / "studio" / "Prism Current.milk"
            else:
                if key not in self.milk_entries:
                    self.milk_entries = catalogue(self.library)
                entry = self.milk_entries[key]
                validate_source(entry)
                path = entry["preset"]
            asset = MilkDrop(Scene.open(Path(path)), self.engine.size, self.install / "milkdrop",
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
        self.configure_output(request.get("studioOutput"))
        actors = request["studioLayers"]
        blends = request.get("studioBlends", {})
        position, generation = request["position"], request["generation"]
        if not actors:
            if self.assets:
                self.release_active()
            self.analysis.reset()
            self.position, self.generation = position, generation
            return dict(ok=True, empty=True, generation=generation, scenes=[], studio=True,
                        **self.output_facts(empty=True)), b""
        dt = position - self.position if self.position is not None else 0.
        if generation != self.generation or dt < 0 or dt > .35:
            self.reset_timing()
            dt = 0.
        self.generation, self.position = generation, position
        self.clock.pcm = np.frombuffer(base64.b64decode(request["pcm"], validate=True), dtype="<f4")
        self.clock.rate, self.clock.beat, self.clock.bpm = request["rate"], request["beat"], request["bpm"]
        audio = self.analysis.frame(position, dt)
        advance = dt > 0 and request["playing"]
        actors = {layer: effective_actor(actor, audio) for layer, actor in actors.items()}
        for layer in ("shader", "milk"):
            if layer not in blends:
                self.asset(layer, None, outgoing=True)
        outgoing = {layer: effective_actor(blend["outgoing"], audio) for layer, blend in blends.items()}
        # Claim outgoing slots before replacing the main actor. This preserves
        # the outgoing feedback history and promotes the prepared incoming asset.
        for layer, actor in outgoing.items():
            self.asset(layer, actor, outgoing=True)
        assets = {layer: self.asset(layer, actors.get(layer)) for layer in ("shader", "milk", "background", "sprite")}
        composite = self.engine.composite
        if outgoing:
            self.render_layers(actors, assets, audio, dt, advance)
            self.blit("output", "incoming")
        for layer, info in outgoing.items():
            alternate = {**actors, layer: info}
            self.render_layers(alternate, {**assets, layer: self.assets[f"{layer}:outgoing"][1]}, audio, dt, advance,
                               advance_shared=layer)
            self.blit("output", "outgoing")
            # Each additional simultaneous join uses the composited result as
            # its incoming side. Match Mix schedules a single primary lane.
            blend = blends[layer]
            pivot = min(.95, max(.05, blend["pivot"]))
            fraction = blend["fraction"]
            bass = blend["preset"] == "bass-handoff"
            if bass:
                fraction = (.5 * fraction / pivot if fraction < pivot
                            else .5 + .5 * (fraction - pivot) / (1. - pivot))
            composite.draw("join", {"previousFrame": composite.targets["outgoing"].textures[0],
                                    "nextFrame": composite.targets["incoming"].textures[0]},
                           {"progress": (fraction,), "bassHandoff": (float(bass),)})
            self.blit("join", "incoming")
        if outgoing:
            self.blit("incoming", "output")
        if not outgoing:
            self.render_layers(actors, assets, audio, dt, advance)
        output = self.output_facts()
        size, pixels = self.engine.read_preview(gl.GL_BGRA)
        bg_info = actors.get("background", {})
        shader = assets["shader"]
        nested = self.sprite_target(actors, assets)
        facts = dict(ok=True, generation=generation, sequence=request.get("sequence", 0),
                     width=size[0], height=size[1], pixelFormat="bgra8-bottom-up",
                     scenes=list(actors.values()) + list(outgoing.values()), studio=True,
                     transitions={layer: {k: v for k, v in blend.items() if k != "outgoing"}
                                  for layer, blend in blends.items()},
                     spriteInMilkDrop=nested == "milk", spriteInGLSL=nested in ("media", "feedback"),
                     backgroundInShader=bool(shader and bg_info.get("controls", {}).get("backgroundNested", 0.) >= .5), **output)
        if shader:
            facts.update(shaderClip=actors["shader"]["clip"], shaderControls=parameter_specs(shader.controls))
        return facts, pixels

    def blit(self, source, destination):
        targets = self.engine.composite.targets
        width, height = self.engine.size
        gl.glBindFramebuffer(gl.GL_READ_FRAMEBUFFER, targets[source].fbos[0])
        gl.glBindFramebuffer(gl.GL_DRAW_FRAMEBUFFER, targets[destination].fbos[0])
        gl.glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, gl.GL_COLOR_BUFFER_BIT, gl.GL_NEAREST)

    @staticmethod
    def sprite_target(actors, assets):
        if not assets["sprite"]:
            return None
        controls = actors["sprite"]["controls"]
        if controls.get("spriteInMilkDrop", 0.) >= .5:
            return "milk" if assets["milk"] and assets["milk"].nested_route else None
        if controls.get("spriteRoute", 0.) >= .5 and assets["shader"]:
            return assets["shader"].nested_route
        return None

    def render_layers(self, actors, assets, audio, dt, advance, *, advance_shared=None):
        composite = self.engine.composite
        empty = composite.empty
        sprite_info, bg_info, milk_info, shader_info = (actors.get(layer) for layer in ("sprite", "background", "milk", "shader"))
        shader, milk, bg, sprite = (assets[layer] for layer in ("shader", "milk", "background", "sprite"))
        sampler_count = max(3, len(shader.samplers) if shader else 0)
        self.reset_gl(sampler_count)
        background = empty
        if bg:
            background = bg.studio_texture
        sprite_values = values_for(MEDIA, sprite_info["controls"] if sprite_info else {})
        background_in_shader = bool(bg and bg_info["controls"].get("backgroundNested", 0.) >= .5)
        nested = self.sprite_target(actors, assets)
        artwork = composite.media(sprite, sprite_values, shader.media_spec if nested == "media" else {},
                                  sprite_info["time"], audio, nested=bool(nested)) if sprite else empty
        if milk and advance_shared == "shader":
            milk_texture = composite.targets["milk"].textures[0]
        elif milk:
            count = max(1, min(len(audio.pcm), round(dt * RATE)))
            milk.render(AudioFrame(audio.pcm[-count:], audio.uniforms), values_for(milk.controls, milk_info["controls"]),
                        advance and advance_shared in (None, "milk"),
                        (lambda: composite.draw_sprite(artwork)) if nested == "milk" and milk.nested_route else None)
            milk_texture = composite.milk_texture()
            self.reset_gl(sampler_count)
        else:
            milk_texture = 0
        source = milk_texture or empty
        milk_mix = shader_info["controls"].get("milkMix", .35) if shader and milk_texture else 0.
        if shader:
            target = shader.targets["syn_FinalPass"]
            feedback = composite.seed_feedback(target, artwork) if nested == "feedback" else 0
            shader_advance = advance_shared in (None, "shader")
            held = shader_info.get("held", {})
            manual = {name for name, control in shader.controls.items()
                      if name in held or any(f"{name}_{axis}" in held for axis in "xyz"[:len(control.default)])}
            shader.render(shader_info["time"], dt if shader_advance else 0., audio, values_for(shader.controls, shader_info["controls"]),
                          ((0, 0), (0, 0), False), advance and shader_advance, artwork if nested == "media" else empty,
                          float(nested == "media"), feedback, milk_texture=milk_texture, milk_amount=4 * milk_mix * (1 - milk_mix),
                          background=background if background_in_shader else 0,
                          background_amount=bg_info["controls"].get("backgroundMix", .38) if background_in_shader else 0.,
                          background_size=bg.image.size if background_in_shader else (1, 1), manual_controls=manual)
            source = target.textures[target.front]
        opacity = bg_info["controls"].get("backgroundMix", .38) if bg_info else 0.
        fallback_mix = milk_mix if shader else 0.
        result = composite.draw("studio", {"background": empty if background_in_shader else background, "scene": source, "milk": milk_texture or empty},
                                {"hasScene": (float(bool(shader or milk)),), "opacity": (opacity,),
                                 "milkMix": (fallback_mix,), "backgroundSize": bg.image.size if bg else (1, 1)})
        grading = shader_info or milk_info
        meta = values_for(META, grading["controls"] if grading else {})
        composite.draw("output", {"source": result, "media": empty if nested else artwork},
                       {**meta, "overlayMode": sprite_values["overlayMode"]})

    def close(self):
        self.reset()
        self.engine.close()

    def poll(self):
        glfw.poll_events()
        self.engine.refresh_output_counter()
