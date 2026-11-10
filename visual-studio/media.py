"""Shared output controls, media controls and a local video transport."""

from math import cos, sin, pi
from pathlib import Path
import time

from PIL import Image

if __package__:
    from .scene import Control
else:
    from scene import Control


def slider(name, label, group, default, low, high):
    return Control(name, label, group, "", "slider", (default,), (low,), (high,))


def switch(name, label, group):
    return Control(name, label, group, "", "toggle", (0.,), (0.,), (1.,))


def color(name, label, default):
    return Control(name, label, "Limit colours", "", "color", default, (0.,) * 3, (1.,) * 3)


META = {control.name: control for control in (
    slider("master", "Master", "Master", 1., 0., 1.),
    switch("invert", "Invert", "Colour"),
    slider("brightness", "Brightness", "Colour", 1., 0., 2.),
    slider("gamma", "Gamma", "Colour", 1., .15, 3.),
    slider("contrast", "Contrast", "Colour", 1., 0., 3.),
    slider("hue", "Hue", "Colour", 0., -180., 180.),
    slider("saturation", "Saturation", "Colour", 0., -1., 1.),
    switch("vertical", "Vertical mirror", "Mirror"),
    switch("horizontal", "Horizontal mirror", "Mirror"),
    switch("limitColors", "Limit colours", "Limit colours"),
    color("lowColor", "Low colour", (.18, .025, .035)),
    color("highColor", "High colour", (1., .59, .17)),
    switch("alpha", "Alpha channel", "Output"),
)}

MOTIONS = ("Still", "Drift", "Orbit", "Spin", "Pendulum", "Bounce", "Beat Pulse")

NESTED = (
    slider("nestedAmount", "Injection", "Nested sprite", 1., 0., 1.),
    slider("nestedBeat", "Beat push", "Nested sprite", 0., 0., 1.),
    slider("nestedAngle", "Angle", "Nested sprite", 0., -180., 180.),
    Control("nestedPosition", "Position", "Nested sprite", "", "xy", (0., 0.), (-.5, -.5), (.5, .5)),
)
MEDIA = {control.name: control for control in (
    Control("spriteRoute", "Render as", "Sprite", "", "dropdown", (0.,), (0.,), (1.,),
            ("Overlay", "Nested"), (0., 1.)),
    Control("spriteMotion", "Sprite motion", "Sprite", "", "dropdown", (0.,), (0.,), (6.,),
            MOTIONS, tuple(map(float, range(len(MOTIONS))))),
    slider("overlayAmount", "Sprite amount", "Sprite", .75, 0., 1.),
    Control("overlayMode", "Overlay blend", "Sprite", "", "dropdown", (0.,), (0.,), (5.,),
            ("Normal", "Multiply", "Screen", "Add", "Difference", "Overlay"), tuple(map(float, range(6)))),
    *NESTED,
    slider("scale", "Media scale", "Transform", 1., .1, 4.),
    switch("fill", "Fill frame", "Transform"),
    switch("flipY", "Vertical flip", "Transform"),
    switch("flipX", "Horizontal flip", "Transform"),
    switch("invert", "Invert media", "Colour"),
    slider("contrast", "Media contrast", "Colour", 1., 0., 3.),
    slider("gamma", "Media gamma", "Colour", 1., .15, 3.),
    slider("hue", "Media hue", "Colour", 0., -180., 180.),
    slider("saturation", "Media saturation", "Colour", 0., -1., 1.),
    slider("speed", "Playback speed", "Transport", 1., .25, 4.),
)}


def sprite_motion(index, elapsed, audio):
    """Return screen offset, rotation in radians and scale multiplier."""
    motion = MOTIONS[int(index)]
    # Native playback supplies the absolute grid beat, so seeks, clip entries
    # and tempo ramps keep the sprite in phase with the audible track.
    bpm = audio.get("alloy_MixBPM", (0.,))[0] or audio.get("syn_BPM", (120.,))[0] or 120.
    beat = audio.get("alloy_BeatTime", (elapsed * bpm / 60.,))[0]
    if motion == "Drift":
        return (.16 * sin(2 * pi * beat / 16), .11 * sin(2 * pi * beat / 12)), 0., .85
    if motion == "Orbit":
        return (.23 * cos(2 * pi * beat / 8), .20 * sin(2 * pi * beat / 8)), 0., .75
    if motion == "Spin":
        return (0., 0.), 2 * pi * beat / 8, .85
    if motion == "Pendulum":
        swing = sin(2 * pi * beat / 4)
        return (.16 * swing, -.06 + .06 * swing ** 2), -.52 * swing, .82
    if motion == "Bounce":
        return (.14 * sin(2 * pi * beat / 8), -.18 + .36 * abs(sin(pi * beat))), 0., .78
    if motion == "Beat Pulse":
        pulse = max(audio["syn_BPMSin"][0], audio["syn_OnBeat"][0])
        return (0., 0.), 0., .8 + .28 * pulse ** 3 + .16 * audio["syn_BassLevel"][0]
    return (0., 0.), 0., 1.


GRADE = """
float luma(vec3 c) { return dot(c, vec3(.2126, .7152, .0722)); }
vec3 grade(vec3 c, float inv, float brightness, float gamma, float contrast, float hue, float saturation) {
    c = mix(c, 1. - c, inv);
    c = (c - .5) * contrast + .5;
    c = pow(max(c * brightness, vec3(0.)), vec3(1. / gamma));
    float a = radians(hue);
    vec3 axis = vec3(.577350269);
    c = c * cos(a) + cross(axis, c) * sin(a) + axis * dot(axis, c) * (1. - cos(a));
    return max(mix(vec3(luma(c)), c, 1. + saturation), vec3(0.));
}
"""

MEDIA_FRAGMENT = """#version 330 core
uniform sampler2D source;
uniform vec2 outputSize, sourceSize, offset;
uniform float scale, angle, amount, fill, flipX, flipY, invert, contrast, gamma, hue, saturation;
out vec4 color;
""" + GRADE + """
void main() {
    vec2 p = (gl_FragCoord.xy / outputSize - .5 - offset) / scale;
    vec2 screenAspect = vec2(outputSize.x / outputSize.y, 1.);
    float c = cos(angle), s = sin(angle);
    p = mat2(c, -s, s, c) * (p * screenAspect) / screenAspect;
    float aspect = (outputSize.x / outputSize.y) / (sourceSize.x / sourceSize.y);
    vec2 fit = vec2(max(1., aspect), max(1., 1. / aspect));
    vec2 cover = vec2(min(1., aspect), min(1., 1. / aspect));
    p *= mix(fit, cover, fill);
    p *= vec2(1. - 2. * flipX, 1. - 2. * flipY);
    vec2 uv = p + .5;
    color = vec4(0.);
    if (all(greaterThanEqual(uv, vec2(0.))) && all(lessThanEqual(uv, vec2(1.)))) {
        vec4 image = texture(source, uv);
        color = vec4(grade(image.rgb, invert, 1., gamma, contrast, hue, saturation), image.a * amount);
    }
}
"""

SPRITE_FRAGMENT = """#version 330 core
uniform sampler2D source;
uniform vec2 outputSize;
out vec4 color;
void main() { color = texture(source, gl_FragCoord.xy / outputSize); }
"""

OUTPUT_FRAGMENT = """#version 330 core
uniform sampler2D source, media;
uniform vec2 outputSize;
uniform float master, invert, brightness, gamma, contrast, hue, saturation;
uniform float vertical, horizontal, limitColors, alpha, overlayMode;
uniform vec3 lowColor, highColor;
out vec4 color;
""" + GRADE + """
void main() {
    vec2 uv = gl_FragCoord.xy / outputSize;
    vec2 sceneUV = mix(uv, min(uv, 1. - uv), vec2(horizontal, vertical));
    vec4 scene = texture(source, sceneUV);
    vec4 art = texture(media, uv);
    vec3 mixed = art.rgb;
    if (overlayMode == 1.) mixed = scene.rgb * art.rgb;
    if (overlayMode == 2.) mixed = 1. - (1. - scene.rgb) * (1. - art.rgb);
    if (overlayMode == 3.) mixed = scene.rgb + art.rgb;
    if (overlayMode == 4.) mixed = abs(scene.rgb - art.rgb);
    if (overlayMode == 5.) mixed = mix(2. * scene.rgb * art.rgb,
        1. - 2. * (1. - scene.rgb) * (1. - art.rgb), step(.5, scene.rgb));
    float weight = art.a;
    vec3 graded = grade(mix(scene.rgb, mixed, weight), invert, brightness, gamma, contrast, hue, saturation);
    graded = mix(graded, mix(lowColor, highColor, clamp(luma(graded), 0., 1.)), limitColors);
    color = vec4(graded * master, mix(1., scene.a + (1. - scene.a) * weight, alpha));
}
"""


class Media:
    def __init__(self):
        self.path, self.image, self.capture = None, None, None
        self.fps, self.frames, self.index = 0., 0, 0
        self.position, self.clock = 0., time.monotonic()
        self.playing, self.loop = False, True
        self.revision = 0

    @property
    def duration(self):
        return self.frames / self.fps if self.capture else 0.

    @staticmethod
    def video_image(frame):
        import cv2

        height, width = frame.shape[:2]
        scale = min(1., 1600 / max(width, height))
        if scale < 1.:
            frame = cv2.resize(frame, (round(width * scale), round(height * scale)), interpolation=cv2.INTER_AREA)
        return Image.fromarray(cv2.cvtColor(frame, cv2.COLOR_BGR2RGBA))

    def open(self, path):
        capture, fps, count = None, 0., 0
        if path.suffix.lower() in (".mp4", ".mov", ".mkv", ".webm", ".avi", ".m4v"):
            import cv2

            capture = cv2.VideoCapture(str(path))
            try:
                fps, count = capture.get(cv2.CAP_PROP_FPS), int(capture.get(cv2.CAP_PROP_FRAME_COUNT))
                ok, frame = capture.read()
                if not ok or not 0 < fps <= 240 or count < 1:
                    raise ValueError("Could not decode a seekable video from this file.")
                image = self.video_image(frame)
            except Exception:
                capture.release()
                raise
        else:
            with Image.open(path) as original:
                if original.width * original.height > 32_000_000:
                    raise ValueError("Image exceeds 32 megapixels.")
                original.thumbnail((2048, 2048))
                image = original.convert("RGBA")
        self.clear()
        self.path, self.image, self.capture = Path(path), image, capture
        self.fps, self.frames = fps, count

    def clear(self):
        if self.capture:
            self.capture.release()
        self.path, self.image, self.capture = None, None, None
        self.playing = False
        self.position, self.index = 0., 0
        self.revision += 1

    def seek(self, position):
        if not self.capture:
            return
        import cv2

        position = min(self.duration, max(0., position))
        index = min(self.frames - 1, int(position * self.fps + 1e-6))
        if index != self.index:
            if index != self.index + 1:
                self.capture.set(cv2.CAP_PROP_POS_FRAMES, index)
            ok, frame = self.capture.read()
            if not ok:
                self.playing = False
                raise ValueError("The selected video frame could not be decoded.")
            self.image = self.video_image(frame)
            self.index = index
            self.revision += 1
        self.position = position

    def toggle(self):
        if self.capture:
            if not self.playing and self.position >= self.duration:
                self.seek(0.)
            self.playing = not self.playing
            self.clock = time.monotonic()

    def tick(self, now, speed):
        dt = min(.25, max(0., now - self.clock))
        self.clock = now
        if self.playing:
            position = self.position + dt * speed
            if position >= self.duration:
                if self.loop:
                    position %= self.duration
                else:
                    self.playing = False
            self.seek(position)
