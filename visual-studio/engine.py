"""Standalone OpenGL renderer with GLSL and projectM backends."""

import ctypes as ct
from math import radians
import os
from pathlib import Path
import re
import time

import glfw
from OpenGL import GL as gl
from PIL import Image, ImageDraw, ImageFont

if __package__:
    from .display import physical_coordinates, place_window
    from .media import MEDIA_FRAGMENT, OUTPUT_FRAGMENT, SPRITE_FRAGMENT, sprite_motion
    from .scene import Control, Scene, identifier, number
else:
    from display import physical_coordinates, place_window
    from media import MEDIA_FRAGMENT, OUTPUT_FRAGMENT, SPRITE_FRAGMENT, sprite_motion
    from scene import Control, Scene, identifier, number


SIZE = (800, 450)
VERTEX = """#version 330 core
void main() {
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    gl_Position = vec4(2.0 * p - 1.0, 0.0, 1.0);
}
"""
BUILTINS = {
    "TIME": "float", "FRAMECOUNT": "float", "RENDERSIZE": "vec2", "PASSINDEX": "int",
    "iTime": "float", "iTimeDelta": "float", "iFrame": "int", "iResolution": "vec3",
    "iMouse": "vec4", "_mouse": "vec4", "_muv": "vec2", "_click": "vec3",
    "syn_Level": "float", "syn_Hits": "float", "syn_OnBeat": "float",
    "syn_BPM": "float", "syn_BPMSin": "float", "alloy_DetectedBPM": "float", "alloy_MixBPM": "float",
    "syn_FinalPass": "sampler2D", "syn_Media": "sampler2D", "syn_MediaType": "float",
    "alloy_MilkDrop": "sampler2D", "alloy_MilkDropActive": "float", "alloy_MilkDropAmount": "float",
    "alloy_Background": "sampler2D", "alloy_BackgroundAmount": "float", "alloy_BackgroundSize": "vec2",
    "iChannel0": "sampler2D",
}
for band in ("Bass", "Mid", "MidHigh", "High"):
    for signal in ("Level", "Hits"):
        BUILTINS[f"syn_{band}{signal}"] = "float"

FLOATS = {gl.GL_FLOAT: 1, gl.GL_FLOAT_VEC2: 2, gl.GL_FLOAT_VEC3: 3, gl.GL_FLOAT_VEC4: 4}
INTEGERS = (gl.GL_INT, gl.GL_BOOL)
FLOAT_SETTERS = (gl.glUniform1f, gl.glUniform2f, gl.glUniform3f, gl.glUniform4f)


def compile_program(fragment):
    shaders = []
    program = gl.glCreateProgram()
    try:
        for stage, source in ((gl.GL_VERTEX_SHADER, VERTEX), (gl.GL_FRAGMENT_SHADER, fragment)):
            shader = gl.glCreateShader(stage)
            shaders.append(shader)
            gl.glShaderSource(shader, source)
            gl.glCompileShader(shader)
            if not gl.glGetShaderiv(shader, gl.GL_COMPILE_STATUS):
                raise ValueError(gl.glGetShaderInfoLog(shader).decode(errors="replace"))
            gl.glAttachShader(program, shader)
        gl.glLinkProgram(program)
        if not gl.glGetProgramiv(program, gl.GL_LINK_STATUS):
            raise ValueError(gl.glGetProgramInfoLog(program).decode(errors="replace"))
        for shader in shaders:
            gl.glDetachShader(program, shader)
        return program
    except Exception:
        gl.glDeleteProgram(program)
        raise
    finally:
        for shader in shaders:
            gl.glDeleteShader(shader)


def source_for(scene, samplers):
    original = scene.source
    code = re.sub(r"/\*.*?\*/|//[^\n]*", lambda m: re.sub(r"[^\n]", " ", m[0]), original, flags=re.S)
    version = re.search(r"(?m)^[ \t]*#version[^\n]*", code)
    header = version[0].strip() if version else "#version 330 core"
    if version:
        original = original[:version.start()] + "\n" * version[0].count("\n") + original[version.end():]
    declared = set(re.findall(r"\buniform\s+\w+\s+(\w+)", code))
    render_main = re.search(r"\bvec4\s+renderMain\s*\(", code)
    main_image = re.search(r"\bvoid\s+mainImage\s*\(", code)
    types = dict(BUILTINS) if render_main or main_image else {}
    for name, control in scene.controls.items():
        if name in BUILTINS or name in samplers:
            raise ValueError(f"{name} is reserved by the renderer.")
        types[name] = {2: "vec2", 3: "vec3"}.get(len(control.default), "float")
    types.update({name: "sampler2D" for name in samplers})
    header += "\n" + "\n".join(f"uniform {kind} {name};" for name, kind in types.items()
                                if name not in declared)
    if render_main:
        header += """
#define _xy (gl_FragCoord.xy)
#define _uv (_xy / RENDERSIZE)
#define _uvc ((_xy - .5 * RENDERSIZE) / RENDERSIZE.y)
const float PI = 3.141592653589793;
out vec4 studioOutput;
"""
        original += """
void main() {
    vec4 scene = renderMain();
    if (alloy_BackgroundAmount > 0.) {
        vec2 flow = .04 * (scene.rg - scene.gb);
        float aspect = (RENDERSIZE.x * alloy_BackgroundSize.y) / (RENDERSIZE.y * alloy_BackgroundSize.x);
        vec2 uv = (_uv + flow - .5) * min(vec2(aspect, 1. / aspect), 1.) + .5;
        vec4 background = texture(alloy_Background, uv);
        float amount = alloy_BackgroundAmount * background.a;
        scene.rgb = mix(background.rgb * amount, scene.rgb, scene.a);
        scene.rgb += (1. - scene.rgb) * background.rgb * amount * scene.a;
        scene.a = max(scene.a, amount);
    }
    studioOutput = scene;
}
"""
    elif main_image:
        header += "\nout vec4 studioOutput;\n"
        original += "\nvoid main() { mainImage(studioOutput, gl_FragCoord.xy); }\n"
    return header + "\n#line 1\n" + original


def texture_settings(spec):
    wraps = {"repeat": gl.GL_REPEAT, "clamp": gl.GL_CLAMP_TO_EDGE, "mirror": gl.GL_MIRRORED_REPEAT}
    filters = {"linear": gl.GL_LINEAR, "nearest": gl.GL_NEAREST,
               "mipmap": gl.GL_LINEAR_MIPMAP_LINEAR}
    wrap, mode = wraps[spec.get("WRAP", "repeat")], filters[spec.get("FILTER", "linear")]
    for axis in (gl.GL_TEXTURE_WRAP_S, gl.GL_TEXTURE_WRAP_T):
        gl.glTexParameteri(gl.GL_TEXTURE_2D, axis, wrap)
    gl.glTexParameteri(gl.GL_TEXTURE_2D, gl.GL_TEXTURE_MIN_FILTER, mode)
    gl.glTexParameteri(gl.GL_TEXTURE_2D, gl.GL_TEXTURE_MAG_FILTER,
                       gl.GL_NEAREST if mode == gl.GL_NEAREST else gl.GL_LINEAR)


def image_texture(path=None):
    if path:
        with Image.open(path) as image:
            if image.width * image.height > 32_000_000:
                raise ValueError("Image exceeds 32 megapixels.")
            image.thumbnail((2048, 2048))
            image = image.convert("RGBA").transpose(Image.Transpose.FLIP_TOP_BOTTOM)
    else:
        image = Image.new("RGBA", (1, 1))
    return upload_texture(image)


def upload_texture(image, texture=0):
    new = not texture
    texture = texture or int(gl.glGenTextures(1))
    try:
        gl.glBindTexture(gl.GL_TEXTURE_2D, texture)
        texture_settings({"WRAP": "clamp"})
        gl.glTexImage2D(gl.GL_TEXTURE_2D, 0, gl.GL_RGBA8, *image.size, 0, gl.GL_RGBA,
                        gl.GL_UNSIGNED_BYTE, image.tobytes())
        return texture
    except Exception:
        if new:
            gl.glDeleteTextures([texture])
        raise


class Target:
    def __init__(self, size, spec, buffers=2):
        self.size, self.textures, self.fbos, self.front = size, [], [], 0
        self.spec = spec
        self.mipmap = spec.get("FILTER") == "mipmap"
        try:
            for _ in range(buffers):
                self.textures.append(int(gl.glGenTextures(1)))
                self.fbos.append(int(gl.glGenFramebuffers(1)))
                gl.glBindTexture(gl.GL_TEXTURE_2D, self.textures[-1])
                texture_settings(spec)
                fmt = gl.GL_RGBA32F if spec.get("FLOAT", True) else gl.GL_RGBA8
                gl.glTexImage2D(gl.GL_TEXTURE_2D, 0, fmt, *size, 0, gl.GL_RGBA, gl.GL_FLOAT, None)
                if self.mipmap:
                    gl.glGenerateMipmap(gl.GL_TEXTURE_2D)
                gl.glBindFramebuffer(gl.GL_FRAMEBUFFER, self.fbos[-1])
                gl.glFramebufferTexture2D(gl.GL_FRAMEBUFFER, gl.GL_COLOR_ATTACHMENT0,
                                          gl.GL_TEXTURE_2D, self.textures[-1], 0)
                if gl.glCheckFramebufferStatus(gl.GL_FRAMEBUFFER) != gl.GL_FRAMEBUFFER_COMPLETE:
                    raise ValueError("Could not allocate the scene's render targets.")
            self.clear()
        except Exception:
            self.close()
            raise

    def clear(self):
        for fbo in self.fbos:
            gl.glBindFramebuffer(gl.GL_FRAMEBUFFER, fbo)
            gl.glClearColor(0, 0, 0, 0)
            gl.glClear(gl.GL_COLOR_BUFFER_BIT)
        if self.mipmap:
            for texture in self.textures:
                gl.glBindTexture(gl.GL_TEXTURE_2D, texture)
                gl.glGenerateMipmap(gl.GL_TEXTURE_2D)
        self.front = 0

    def close(self):
        if self.textures:
            gl.glDeleteTextures(self.textures)
            gl.glDeleteFramebuffers(len(self.fbos), self.fbos)

    def resized(self, size, preserve):
        target = Target(size, self.spec, len(self.fbos))
        if preserve:
            try:
                for source, destination in zip(self.fbos, target.fbos):
                    gl.glBindFramebuffer(gl.GL_READ_FRAMEBUFFER, source)
                    gl.glBindFramebuffer(gl.GL_DRAW_FRAMEBUFFER, destination)
                    gl.glBlitFramebuffer(0, 0, *self.size, 0, 0, *size, gl.GL_COLOR_BUFFER_BIT, gl.GL_LINEAR)
                target.front = self.front
            except Exception:
                target.close()
                raise
        return target


class Shader:
    def __init__(self, scene, size, vao, *, preview_size=None):
        self.vao, self.program = vao, 0
        self.targets, self.images, self.controls, self.uniforms = {}, {}, dict(scene.controls), {}
        self.state, self.events, self.frame = {}, set(), 0
        self.media_spec = scene.media
        if (self.media_spec.get("WRAP", "repeat") not in ("repeat", "clamp", "mirror")
                or self.media_spec.get("FILTER", "linear") not in ("linear", "nearest", "mipmap")):
            raise ValueError("Unsupported media wrap or filter.")
        names = set(BUILTINS) | self.controls.keys() | scene.images.keys()
        if set(scene.images) & (set(BUILTINS) | self.controls.keys()):
            raise ValueError("An image uniform conflicts with a control or renderer uniform.")
        outputs = {}
        budget = 0
        normal_width, normal_height = preview_size or size
        for spec in scene.passes:
            name = identifier(spec["TARGET"])
            width, height = spec.get("WIDTH", normal_width), spec.get("HEIGHT", normal_height)
            if name in names or name in outputs:
                raise ValueError(f"Duplicate pass uniform {name}.")
            if type(width) is not int or type(height) is not int or not (1 <= width <= 1920 and 1 <= height <= 1080):
                raise ValueError(f"Invalid dimensions for pass {name}.")
            budget += width * height * (32 if spec.get("FLOAT", True) else 8)
            outputs[name] = ((width, height), spec)
        if budget > 96 * 1024 * 1024:
            raise ValueError("Intermediate passes exceed the 96 MB preview buffer budget.")
        outputs["syn_FinalPass"] = (size, {"FLOAT": False, "WRAP": "clamp"})
        try:
            self.program = compile_program(source_for(scene, (outputs.keys() - BUILTINS.keys()) | scene.images.keys()))
            for index in range(gl.glGetProgramiv(self.program, gl.GL_ACTIVE_UNIFORMS)):
                name, count, kind = gl.glGetActiveUniform(self.program, index)
                name = name.decode()
                if count != 1 or "[" in name:
                    raise ValueError(f"Uniform arrays are unsupported ({name}).")
                self.uniforms[name] = (gl.glGetUniformLocation(self.program, name), kind)
            self.samplers = []
            available = set(outputs) | scene.images.keys() | {"syn_Media", "iChannel0", "alloy_MilkDrop", "alloy_Background"}
            for name, (location, kind) in self.uniforms.items():
                if kind == gl.GL_SAMPLER_2D:
                    if name not in available:
                        raise ValueError(f"Bind sampler {name} in the scene's IMAGES list.")
                    self.samplers.append(name)
                elif name not in BUILTINS and name not in self.controls:
                    self.controls[name] = self.infer_control(name, location, kind)
                elif name in self.controls:
                    control = self.controls[name]
                    if FLOATS.get(kind, int(kind in INTEGERS)) != len(control.default):
                        raise ValueError(f"GLSL uniform type does not match control {name}.")
                    if kind in INTEGERS and control.kind != "toggle":
                        raise ValueError(f"Declare {name} as float or use a toggle for int/bool.")
            if len(self.samplers) > gl.glGetIntegerv(gl.GL_MAX_TEXTURE_IMAGE_UNITS):
                raise ValueError("The scene needs more texture units than this GPU provides.")
            self.nested_route = (
                "media" if {"syn_Media", "iChannel0"}.intersection(self.samplers)
                else "feedback" if "syn_FinalPass" in self.samplers else None
            )
            for name, (dimensions, spec) in outputs.items():
                self.targets[name] = Target(dimensions, spec)
            for name, path in scene.images.items():
                self.images[name] = image_texture(path)
        except Exception:
            self.close()
            raise

    def infer_control(self, name, location, kind):
        size = FLOATS.get(kind, int(kind in INTEGERS))
        if not size:
            raise ValueError(f"Unsupported type for uniform {name}.")
        out = (ct.c_int * size)() if kind in INTEGERS else (ct.c_float * size)()
        get = gl.glGetUniformiv if kind in INTEGERS else gl.glGetUniformfv
        get(self.program, location, out)
        default = tuple(number(x) for x in out)
        low = tuple(min(0., x - abs(x)) for x in default)
        high = tuple(max(1., 2 * abs(x)) for x in default)
        form = "toggle" if kind == gl.GL_BOOL else "integer" if kind == gl.GL_INT else "slider"
        return Control(name, name.replace("_", " "), "Shader uniforms",
                       "Range inferred from the GLSL initializer. Add a JSON sidecar for custom ranges.",
                       form, default, low, high)

    def render(self, elapsed, dt, audio, values, mouse, advance, media, media_type, feedback, *, milk_texture=0, milk_amount=1.,
               background=0, background_amount=0., background_size=(1, 1), manual_controls=()):
        gl.glBindVertexArray(self.vao)
        gl.glUseProgram(self.program)
        coords, click, down = mouse
        common = {"TIME": (elapsed,), "iTime": (elapsed,), "iTimeDelta": (dt,),
                  "iFrame": (self.frame,), "FRAMECOUNT": (self.frame,),
                  "syn_MediaType": (media_type,), "alloy_MilkDropActive": (float(bool(milk_texture)),),
                  "alloy_MilkDropAmount": (milk_amount,), "alloy_BackgroundAmount": (background_amount,),
                  "alloy_BackgroundSize": background_size, "_muv": coords,
                  "_click": (float(down), 0., 0.), **audio.uniforms}
        for name, control in self.controls.items():
            value = values[name]
            if control.kind == "bang":
                if control.mode == "counter":
                    value = (self.state.get(name, (0.,))[0] + int(name in self.events),)
                elif control.mode == "smooth" and name not in self.events:
                    previous = self.state.get(name, control.default)[0]
                    decay = (1 - control.rate) ** (dt * 60)
                    value = (control.low[0] + (previous - control.low[0]) * decay,)
                else:
                    value = control.high if name in self.events else control.low
            elif control.mode == "speed":
                value = tuple(a + b * dt for a, b in zip(self.state.get(name, (0.,) * len(value)), value))
            elif control.mode == "smooth" and name not in manual_controls:
                factor = 1 - (1 - control.rate) ** (dt * 60) if advance else 1
                value = tuple(a + (b - a) * factor for a, b in zip(self.state.get(name, control.default), value))
            self.state[name] = common[name] = value
        for index, (name, target) in enumerate(self.targets.items()):
            width, height = target.size
            gl.glBindFramebuffer(gl.GL_FRAMEBUFFER, target.fbos[1 - target.front])
            gl.glViewport(0, 0, width, height)
            click_sign = 1 if down else -1
            pointer = (coords[0] * width, coords[1] * height,
                       click_sign * click[0] * width, click_sign * click[1] * height)
            common.update(RENDERSIZE=(width, height), iResolution=(width, height, 1),
                          PASSINDEX=(index,), iMouse=pointer, _mouse=pointer)
            for key, value in common.items():
                if key in self.uniforms:
                    location, kind = self.uniforms[key]
                    if kind in INTEGERS:
                        gl.glUniform1i(location, int(value[0]))
                    elif FLOATS.get(kind) == len(value):
                        FLOAT_SETTERS[len(value) - 1](location, *value)
            for unit, sampler in enumerate(self.samplers):
                if sampler == "syn_FinalPass" and feedback:
                    texture = feedback
                elif sampler in self.targets:
                    source = self.targets[sampler]
                    texture = source.textures[source.front]
                elif sampler == "alloy_MilkDrop":
                    texture = milk_texture or media
                elif sampler == "alloy_Background":
                    texture = background or media
                else:
                    texture = self.images.get(sampler, media)
                gl.glActiveTexture(gl.GL_TEXTURE0 + unit)
                gl.glBindTexture(gl.GL_TEXTURE_2D, texture)
                gl.glUniform1i(self.uniforms[sampler][0], unit)
            gl.glDrawArrays(gl.GL_TRIANGLES, 0, 3)
            target.front = 1 - target.front
            if target.mipmap:
                gl.glBindTexture(gl.GL_TEXTURE_2D, target.textures[target.front])
                gl.glGenerateMipmap(gl.GL_TEXTURE_2D)
        self.events.clear()
        self.frame += int(advance)

    def reset(self):
        self.frame = 0
        self.state.clear()
        self.events.clear()
        for target in self.targets.values():
            target.clear()

    def close(self):
        for target in self.targets.values():
            target.close()
        if self.images:
            gl.glDeleteTextures(list(self.images.values()))
        if self.program:
            gl.glDeleteProgram(self.program)


FAILED = ct.CFUNCTYPE(None, ct.c_char_p, ct.c_char_p, ct.c_void_p)
FEEDBACK_INPUT = ct.CFUNCTYPE(None, ct.c_void_p)


class MilkDrop:
    def __init__(self, scene, size, runtime, *, texture_paths=()):
        self.size, self.handle, self.error = size, None, None
        self.path = scene.path
        self.controls = {"sensitivity": Control("sensitivity", "Beat sensitivity", "Audio", "",
                                                "slider", (1.,), (0.,), (2.,))}
        if not runtime or not (runtime / "projectM-4.dll").is_file():
            raise ValueError("MilkDrop needs projectM-4.dll and glew32.dll. See the Studio README.")
        self.search = os.add_dll_directory(str(runtime))
        try:
            glew = ct.CDLL(str(runtime / "glew32.dll"))
            ct.c_ubyte.in_dll(glew, "glewExperimental").value = 1
            if glew.glewInit():
                raise ValueError("Could not initialise projectM's OpenGL functions.")
            for _ in range(8):
                if gl.glGetError() == gl.GL_NO_ERROR:
                    break
            self.dll = ct.CDLL(str(runtime / "projectM-4.dll"))
            signatures = {
                "create": (ct.c_void_p, []), "destroy": (None, [ct.c_void_p]),
                "set_window_size": (None, [ct.c_void_p, ct.c_size_t, ct.c_size_t]),
                "set_fps": (None, [ct.c_void_p, ct.c_int32]),
                "set_preset_locked": (None, [ct.c_void_p, ct.c_bool]),
                "set_hard_cut_enabled": (None, [ct.c_void_p, ct.c_bool]),
                "set_texture_search_paths": (None, [ct.c_void_p, ct.POINTER(ct.c_char_p), ct.c_size_t]),
                "set_preset_switch_failed_event_callback": (None, [ct.c_void_p, FAILED, ct.c_void_p]),
                "load_preset_file": (None, [ct.c_void_p, ct.c_char_p, ct.c_bool]),
                "set_beat_sensitivity": (None, [ct.c_void_p, ct.c_float]),
                "pcm_add_float": (None, [ct.c_void_p, ct.POINTER(ct.c_float), ct.c_uint, ct.c_int]),
                "opengl_render_frame": (None, [ct.c_void_p]),
            }
            for name, (result, args) in signatures.items():
                function = getattr(self.dll, f"projectm_{name}")
                function.restype, function.argtypes = result, args
                setattr(self, name, function)
            self.render_with_feedback = getattr(self.dll, "projectm_opengl_render_frame_with_feedback", None)
            self.nested_route = "feedback" if self.render_with_feedback else None
            if self.render_with_feedback:
                self.render_with_feedback.restype = None
                self.render_with_feedback.argtypes = [ct.c_void_p, FEEDBACK_INPUT, ct.c_void_p]
            self.render_with_controls = getattr(self.dll, "projectm_opengl_render_frame_with_controls", None)
            if self.render_with_controls:
                self.render_with_controls.restype = None
                self.render_with_controls.argtypes = [ct.c_void_p, FEEDBACK_INPUT, ct.c_void_p, *([ct.c_float] * 4)]
                feedback = ("trail", "Trail length"), ("zoom", "Zoom"), ("spin", "Spin"), ("warp", "Warp")
                self.controls = {**{name: Control(name, label, "Feedback", "", "slider", (0.,), (-1.,), (1.,))
                                    for name, label in feedback}, **self.controls}
            self.handle = self.create()
            if not self.handle:
                raise ValueError("Could not create projectM.")
            self.callback = FAILED(self.failed)
            self.set_preset_switch_failed_event_callback(self.handle, self.callback, None)
            self.set_window_size(self.handle, *size)
            self.set_fps(self.handle, 30)
            self.set_preset_locked(self.handle, True)
            self.set_hard_cut_enabled(self.handle, False)
            paths = [scene.path.parent, *map(Path, texture_paths)]
            for root in (scene.path.parent, *list(scene.path.parents)[1:4]):
                if (root / "Textures").is_dir():
                    paths.append(root / "Textures")
            encoded = list(map(os.fsencode, dict.fromkeys(paths)))
            self.set_texture_search_paths(self.handle, (ct.c_char_p * len(encoded))(*encoded), len(encoded))
            self.reset()
        except Exception:
            self.close()
            raise

    def failed(self, filename, message, context):
        self.error = (message or b"Preset rejected").decode(errors="replace")

    def check(self):
        if self.error:
            raise ValueError(self.error)

    def reset(self):
        self.error = None
        gl.glBindFramebuffer(gl.GL_FRAMEBUFFER, 0)
        self.load_preset_file(self.handle, os.fsencode(self.path), False)
        self.check()

    def render(self, audio, values, advance, inject):
        self.set_beat_sensitivity(self.handle, values["sensitivity"][0])
        if advance:
            samples = audio.pcm
            self.pcm_add_float(self.handle, samples.ctypes.data_as(ct.POINTER(ct.c_float)), len(samples), 1)
        gl.glBindFramebuffer(gl.GL_FRAMEBUFFER, 0)
        failure = []
        callback = FEEDBACK_INPUT()
        if inject:
            @FEEDBACK_INPUT
            def callback(_):
                try:
                    inject()
                except Exception as error:
                    failure.append(error)

        if self.render_with_controls:
            trail = values["trail"][0]
            self.render_with_controls(self.handle, callback, None, trail * (.12 if trail < 0. else .06),
                                      1. + .06 * values["zoom"][0], .06 * values["spin"][0], 1.5 * values["warp"][0])
        elif inject:
            self.render_with_feedback(self.handle, callback, None)
        else:
            self.opengl_render_frame(self.handle)
        if failure:
            raise failure[0]
        self.check()

    def close(self):
        if self.handle:
            self.destroy(self.handle)
        self.search.close()


class Composite:
    def __init__(self, size, vao):
        self.size, self.vao = size, vao
        self.programs, self.targets = {}, {}
        self.empty, self.raw_media, self.revision, self.media_key = 0, 0, None, None
        try:
            self.empty = image_texture()
            for name, fragment in (("media", MEDIA_FRAGMENT), ("output", OUTPUT_FRAGMENT),
                                   ("sprite", SPRITE_FRAGMENT)):
                self.programs[name] = compile_program(fragment)
            for name in ("media", "milk", "output", "feedback"):
                self.targets[name] = Target(size, {"FLOAT": False, "WRAP": "clamp"}, buffers=1)
        except Exception:
            self.close()
            raise

    def draw(self, name, textures, values):
        program = self.programs[name]
        target = self.targets[name]
        gl.glBindFramebuffer(gl.GL_FRAMEBUFFER, target.fbos[0])
        gl.glBindVertexArray(self.vao)
        gl.glViewport(0, 0, *self.size)
        gl.glUseProgram(program)
        for unit, (key, texture) in enumerate(textures.items()):
            gl.glActiveTexture(gl.GL_TEXTURE0 + unit)
            gl.glBindSampler(unit, 0)
            gl.glBindTexture(gl.GL_TEXTURE_2D, texture)
            gl.glUniform1i(gl.glGetUniformLocation(program, key), unit)
        for key, value in values.items():
            FLOAT_SETTERS[len(value) - 1](gl.glGetUniformLocation(program, key), *value)
        gl.glUniform2f(gl.glGetUniformLocation(program, "outputSize"), *self.size)
        gl.glDrawArrays(gl.GL_TRIANGLES, 0, 3)
        return target.textures[0]

    def media(self, media, values, spec, elapsed, audio, *, nested=False):
        amount = values["overlayAmount"][0] * (values["nestedAmount"][0] if nested else 1.)
        if not media or not media.image or not amount:
            return self.empty
        revision = (id(media), media.revision)
        image = media.image
        if not self.raw_media or revision != self.revision:
            self.raw_media = upload_texture(image.transpose(Image.Transpose.FLIP_TOP_BOTTOM), self.raw_media)
            self.revision = revision
        offset, angle, scale = sprite_motion(values["spriteMotion"][0], elapsed, audio.uniforms)
        if nested:
            x, y = values["nestedPosition"]
            offset = (offset[0] + x, offset[1] + y)
            angle += radians(values["nestedAngle"][0])
            u = audio.uniforms
            beat = max(u["syn_OnBeat"][0], u["syn_BPMSin"][0] ** 3 * u["syn_BassLevel"][0])
            scale *= 1. + .65 * values["nestedBeat"][0] * beat
        transform = {name: values[name] for name in ("fill", "flipX", "flipY", "invert",
                                                     "contrast", "gamma", "hue", "saturation")}
        transform.update(offset=offset, angle=(angle,), scale=(values["scale"][0] * scale,),
                         amount=(amount,), sourceSize=image.size)
        key = (revision, transform, dict(spec))
        if key == self.media_key:
            return self.targets["media"].textures[0]
        texture = self.draw("media", {"source": self.raw_media}, transform)
        gl.glBindTexture(gl.GL_TEXTURE_2D, texture)
        texture_settings(spec)
        if spec.get("FILTER") == "mipmap":
            gl.glGenerateMipmap(gl.GL_TEXTURE_2D)
        self.media_key = key
        return texture

    def draw_sprite(self, texture):
        # The caller owns the bound framebuffer and viewport.
        program = self.programs["sprite"]
        gl.glBindVertexArray(self.vao)
        gl.glUseProgram(program)
        gl.glActiveTexture(gl.GL_TEXTURE0)
        gl.glBindSampler(0, 0)
        gl.glBindTexture(gl.GL_TEXTURE_2D, texture)
        gl.glUniform1i(gl.glGetUniformLocation(program, "source"), 0)
        gl.glUniform2f(gl.glGetUniformLocation(program, "outputSize"), *self.size)
        gl.glEnable(gl.GL_BLEND)
        gl.glBlendEquation(gl.GL_FUNC_ADD)
        gl.glBlendFuncSeparate(gl.GL_SRC_ALPHA, gl.GL_ONE_MINUS_SRC_ALPHA,
                               gl.GL_ONE, gl.GL_ONE_MINUS_SRC_ALPHA)
        try:
            gl.glDrawArrays(gl.GL_TRIANGLES, 0, 3)
        finally:
            gl.glDisable(gl.GL_BLEND)

    def seed_feedback(self, final, sprite):
        target = self.targets["feedback"]
        width, height = self.size
        gl.glBindFramebuffer(gl.GL_READ_FRAMEBUFFER, final.fbos[final.front])
        gl.glBindFramebuffer(gl.GL_DRAW_FRAMEBUFFER, target.fbos[0])
        gl.glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, gl.GL_COLOR_BUFFER_BIT, gl.GL_NEAREST)
        gl.glViewport(0, 0, width, height)
        self.draw_sprite(sprite)
        return target.textures[0]

    def milk_texture(self):
        gl.glBindFramebuffer(gl.GL_READ_FRAMEBUFFER, 0)
        gl.glReadBuffer(gl.GL_BACK)
        texture = self.targets["milk"].textures[0]
        gl.glActiveTexture(gl.GL_TEXTURE0)
        gl.glBindTexture(gl.GL_TEXTURE_2D, texture)
        gl.glCopyTexSubImage2D(gl.GL_TEXTURE_2D, 0, 0, 0, 0, 0, *self.size)
        return texture

    def close(self):
        for target in self.targets.values():
            target.close()
        for program in self.programs.values():
            gl.glDeleteProgram(program)
        textures = [texture for texture in (self.empty, self.raw_media) if texture]
        if textures:
            gl.glDeleteTextures(textures)


HYBRID_FRAGMENT = """#version 330 core
uniform sampler2D scene, milk;
uniform float amount;
uniform vec2 outputSize;
out vec4 colour;
void main() {
    vec2 uv = gl_FragCoord.xy / outputSize;
    colour = mix(texture(scene, uv), texture(milk, uv), amount);
}
"""


class OutputCounter:
    def __init__(self):
        self.start = time.monotonic()
        self.frames = self.value = self.texture = 0
        self.fbo = int(gl.glGenFramebuffers(1))
        self.label = None

    def sample(self):
        now = time.monotonic()
        elapsed = now - self.start
        if elapsed < .5:
            return False
        previous = self.value
        self.value = round(self.frames / elapsed)
        self.start, self.frames = now, 0
        return self.value != previous

    def draw(self, height):
        font_size = max(14, round(height / 60))
        label = self.value, font_size
        if self.label != label:
            try:
                font = ImageFont.truetype("segoeui.ttf", font_size)
            except OSError:
                font = ImageFont.load_default()
            text = f"{self.value} FPS"
            left, top, right, bottom = font.getbbox(text)
            padding = max(5, font_size // 3)
            size = right - left + 2 * padding, bottom - top + 2 * padding
            image = Image.new("RGBA", size, (17, 22, 25, 255))
            ImageDraw.Draw(image).text((padding - left, padding - top), text, font=font, fill=(245, 246, 247, 255))
            self.texture = upload_texture(image.transpose(Image.Transpose.FLIP_TOP_BOTTOM), self.texture)
            self.label, self.size = label, size
        gl.glBindFramebuffer(gl.GL_READ_FRAMEBUFFER, self.fbo)
        gl.glFramebufferTexture2D(gl.GL_READ_FRAMEBUFFER, gl.GL_COLOR_ATTACHMENT0, gl.GL_TEXTURE_2D, self.texture, 0)
        width, label_height = self.size
        margin = max(8, font_size // 2)
        y = height - margin - label_height
        gl.glBlitFramebuffer(0, 0, width, label_height, margin, y, margin + width, y + label_height,
                             gl.GL_COLOR_BUFFER_BIT, gl.GL_NEAREST)

    def close(self):
        if self.texture:
            gl.glDeleteTextures([self.texture])
        gl.glDeleteFramebuffers(1, [self.fbo])


class Engine:
    def __init__(self, runtime=None, size=SIZE):
        self.window, self.backend, self.scene, self.vao, self.composite = None, None, None, 0, None
        self.runtime, self.size, self.values = runtime, size, {}
        self.preview_size, self.preview_target = size, None
        self.output, self.return_output, self.output_dismissed = None, None, False
        self.counter, self.output_empty = None, True
        self.companion = None
        try:
            with physical_coordinates():
                if not glfw.init():
                    raise RuntimeError("Could not start GLFW.")
                glfw.window_hint(glfw.VISIBLE, False)
                glfw.window_hint(glfw.DECORATED, False)
                glfw.window_hint(glfw.FOCUS_ON_SHOW, False)
                glfw.window_hint(glfw.CONTEXT_VERSION_MAJOR, 3)
                glfw.window_hint(glfw.CONTEXT_VERSION_MINOR, 3)
                glfw.window_hint(glfw.OPENGL_PROFILE, glfw.OPENGL_CORE_PROFILE)
                self.window = glfw.create_window(*size, "Mixworks Visual Studio Output", None, None)
            if not self.window:
                raise RuntimeError("Visual Studio needs OpenGL 3.3.")
            glfw.make_context_current(self.window)
            glfw.swap_interval(0)
            glfw.set_window_close_callback(self.window, lambda _: self.dismiss_output())
            glfw.set_key_callback(self.window, self.output_key)
            glfw.set_input_mode(self.window, glfw.CURSOR, glfw.CURSOR_HIDDEN)
            self.vao = int(gl.glGenVertexArrays(1))
            self.composite = Composite(size, self.vao)
            self.composite.programs["hybrid"] = compile_program(HYBRID_FRAGMENT)
            self.composite.targets["hybrid"] = Target(size, {"FLOAT": False, "WRAP": "clamp"}, buffers=1)
        except Exception:
            self.close()
            raise

    @property
    def controls(self):
        controls = dict(self.backend.controls)
        if isinstance(self.backend, Shader):
            controls["milkMix"] = Control.parse({"NAME": "milkMix", "LABEL": "Original / MilkDrop",
                "TYPE": "slider", "UI_GROUP": "MilkDrop", "DEFAULT": 0, "MIN": 0, "MAX": 1})
        return controls

    def load(self, scene, *, texture_paths=()):
        candidate = (MilkDrop(scene, self.size, self.runtime, texture_paths=texture_paths)
                     if scene.path.suffix.lower() == ".milk"
                     else Shader(scene, self.size, self.vao, preview_size=self.preview_size))
        if self.companion:
            self.companion.close()
            self.companion = None
        if self.backend:
            self.backend.close()
        self.backend, self.scene = candidate, scene
        self.reset_controls()

    def reset_controls(self):
        self.values = {name: control.default for name, control in self.controls.items()}

    def resize(self, size, assets):
        if size == self.size:
            return
        assets = (self.backend, self.companion, *assets)
        targets = [(self.composite.targets, name, False) for name in self.composite.targets]
        targets.extend((asset.targets, "syn_FinalPass", True) for asset in assets if isinstance(asset, Shader))
        gl.glDisable(gl.GL_SCISSOR_TEST)
        gl.glColorMask(True, True, True, True)
        replacements = []
        try:
            for group, name, preserve in targets:
                replacements.append((group, name, group[name].resized(size, preserve)))
        except Exception:
            for _, _, target in replacements:
                target.close()
            raise
        for group, name, target in replacements:
            group[name].close()
            group[name] = target
        for asset in assets:
            if isinstance(asset, MilkDrop):
                asset.set_window_size(asset.handle, *size)
                asset.size = size
        self.size = self.composite.size = size
        self.composite.media_key = None

    def set_output(self, output, return_output=None, assets=()):
        if output == self.output:
            return
        if output is None:
            glfw.hide_window(self.window)
            self.resize(self.preview_size, assets)
            with physical_coordinates():
                glfw.set_window_size(self.window, *self.preview_size)
        elif not self.output or output["bounds"] != self.output["bounds"]:
            bounds = output["bounds"]
            size = tuple(bounds[2:])
            with physical_coordinates():
                if os.name == "nt":
                    hwnd = glfw.get_win32_window(self.window)
                    place_window(hwnd, bounds)
                    # A cross-monitor DPI change can apply an intermediate rectangle.
                    if glfw.get_framebuffer_size(self.window) != size:
                        place_window(hwnd, bounds)
                else:
                    glfw.set_window_pos(self.window, *bounds[:2])
                    glfw.set_window_size(self.window, *size)
                if glfw.get_framebuffer_size(self.window) != size:
                    raise RuntimeError("The output window could not match the display's pixel dimensions.")
            self.resize(size, assets)
        if output and output.get("showFps"):
            if self.counter is None:
                self.counter = OutputCounter()
        elif self.counter:
            self.counter.close()
            self.counter = None
        self.output, self.return_output, self.output_dismissed = output, return_output, False

    def output_key(self, _window, key, _scan, action, _modifiers):
        if action == glfw.PRESS and key in (glfw.KEY_ESCAPE, glfw.KEY_F11):
            self.dismiss_output()

    def dismiss_output(self):
        glfw.set_window_should_close(self.window, False)
        if self.output and not self.output_dismissed:
            self.output_dismissed = True
            glfw.hide_window(self.window)
            self.return_output(self.output["token"])

    def present_output(self, *, empty=False):
        if not self.output or self.output_dismissed:
            return False
        self.output_empty = empty
        if self.counter:
            self.counter.sample()
        self.draw_output()
        if self.counter:
            self.counter.frames += 1
        return True

    def refresh_output_counter(self):
        if self.counter and not self.output_dismissed and self.counter.sample():
            self.draw_output()

    def draw_output(self):
        target = self.composite.targets["output"]
        gl.glBindFramebuffer(gl.GL_DRAW_FRAMEBUFFER, 0)
        gl.glDisable(gl.GL_SCISSOR_TEST)
        if self.output_empty:
            gl.glClearColor(0., 0., 0., 1.)
            gl.glClear(gl.GL_COLOR_BUFFER_BIT)
        else:
            gl.glBindFramebuffer(gl.GL_READ_FRAMEBUFFER, target.fbos[0])
            gl.glBlitFramebuffer(0, 0, *self.size, 0, 0, *self.size, gl.GL_COLOR_BUFFER_BIT, gl.GL_NEAREST)
        if self.counter:
            self.counter.draw(self.size[1])
        if not glfw.get_window_attrib(self.window, glfw.VISIBLE):
            glfw.show_window(self.window)
        glfw.swap_buffers(self.window)

    def read_preview(self, pixel_format):
        target = self.composite.targets["output"]
        gl.glBindFramebuffer(gl.GL_READ_FRAMEBUFFER, target.fbos[0])
        size = self.size
        if size != self.preview_size:
            scale = min(a / b for a, b in zip(self.preview_size, size))
            dimensions = tuple(max(1, round(side * scale)) for side in size)
            if self.preview_target is None or self.preview_target.size != dimensions:
                preview = Target(dimensions, {"FLOAT": False, "WRAP": "clamp"}, buffers=1)
                if self.preview_target:
                    self.preview_target.close()
                self.preview_target = preview
            gl.glBindFramebuffer(gl.GL_READ_FRAMEBUFFER, target.fbos[0])
            gl.glBindFramebuffer(gl.GL_DRAW_FRAMEBUFFER, self.preview_target.fbos[0])
            gl.glBlitFramebuffer(0, 0, *size, 0, 0, *dimensions, gl.GL_COLOR_BUFFER_BIT, gl.GL_LINEAR)
            gl.glBindFramebuffer(gl.GL_READ_FRAMEBUFFER, self.preview_target.fbos[0])
            size = dimensions
        gl.glReadBuffer(gl.GL_COLOR_ATTACHMENT0)
        return size, gl.glReadPixels(0, 0, *size, pixel_format, gl.GL_UNSIGNED_BYTE)

    def render(self, elapsed, dt, audio, meta, media_values, media=None,
               mouse=((0., 0.), (0., 0.), False), advance=True, update_scene=True):
        # Tk dispatches Win32 messages for this thread, including the hidden
        # GL window. Polling here can take mouse messages from a native drag.
        if os.name != "nt":
            glfw.poll_events()
        for flag in (gl.GL_BLEND, gl.GL_DEPTH_TEST, gl.GL_SCISSOR_TEST, gl.GL_CULL_FACE):
            gl.glDisable(flag)
        gl.glColorMask(True, True, True, True)
        gl.glPixelStorei(gl.GL_PACK_ALIGNMENT, 1)
        shader = isinstance(self.backend, Shader)
        nested = bool(media_values["spriteRoute"][0])
        route = self.backend.nested_route if nested else None
        spec = self.backend.media_spec if shader and route == "media" else {}
        empty = self.composite.empty
        sprite = (self.composite.media(media, media_values, spec, elapsed, audio, nested=nested)
                  if not nested or route else empty)
        milk_mix = self.values.get("milkMix", (0.,))[0] if shader else 0.
        milk_texture = 0
        if milk_mix:
            if self.companion is None:
                preset = Path(__file__).resolve().parent / "authoring" / "Prism Current.milk"
                if not preset.is_file():
                    preset = Path(__file__).resolve().parent.parent / "studio" / "Prism Current.milk"
                self.companion = MilkDrop(Scene.open(preset), self.size, self.runtime)
            self.companion.render(audio, {n: c.default for n, c in self.companion.controls.items()}, advance, None)
            milk_texture = self.composite.milk_texture()
            for flag in (gl.GL_BLEND, gl.GL_DEPTH_TEST, gl.GL_SCISSOR_TEST, gl.GL_CULL_FACE):
                gl.glDisable(flag)
        if shader:
            final = self.backend.targets["syn_FinalPass"]
            for unit in range(len(self.backend.samplers)):
                gl.glBindSampler(unit, 0)
            if update_scene:
                media_type = float(2 if media.capture else 1) if route == "media" and sprite != empty else 0.
                feedback = self.composite.seed_feedback(final, sprite) if route == "feedback" and sprite != empty else 0
                self.backend.render(elapsed, dt, audio, self.values, mouse, advance,
                                    sprite if media_type else empty, media_type, feedback,
                                    milk_texture=milk_texture, milk_amount=4 * milk_mix * (1 - milk_mix))
            scene_texture = final.textures[final.front]
        else:
            if update_scene:
                inject = (lambda: self.composite.draw_sprite(sprite)) if route and sprite != empty else None
                self.backend.render(audio, self.values, advance, inject)
                self.composite.milk_texture()
                for flag in (gl.GL_BLEND, gl.GL_DEPTH_TEST, gl.GL_SCISSOR_TEST, gl.GL_CULL_FACE):
                    gl.glDisable(flag)
            scene_texture = self.composite.targets["milk"].textures[0]
        if milk_texture:
            scene_texture = self.composite.draw("hybrid", {"scene": scene_texture, "milk": milk_texture}, {"amount": (milk_mix,)})
        self.composite.draw("output", {"source": scene_texture, "media": empty if nested else sprite},
                            {**meta, "overlayMode": media_values["overlayMode"]})
        self.present_output()
        size, pixels = self.read_preview(gl.GL_RGBA)
        return Image.frombytes("RGBA", size, pixels).transpose(Image.Transpose.FLIP_TOP_BOTTOM)

    def close(self):
        if self.companion:
            self.companion.close()
        if self.backend:
            self.backend.close()
        if self.composite:
            self.composite.close()
        if self.preview_target:
            self.preview_target.close()
        if self.counter:
            self.counter.close()
        if self.vao:
            gl.glDeleteVertexArrays(1, [self.vao])
        if self.window:
            glfw.destroy_window(self.window)
        glfw.terminate()
