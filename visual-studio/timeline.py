"""The connected Studio's session mailbox and timeline control mapping."""

import json
import mmap
import os
from pathlib import Path
from struct import Struct
import time
from uuid import uuid4

from PIL import Image

from media import MEDIA, META
from scene import Control, Scene


# SceneRenderWorker writes BGRA pixels into versioned slots. Check the
# sequence again after copying to reject a slot overwritten during the read.
PREVIEW_SEQUENCE = Struct("<Q")
PREVIEW_HEADER = Struct("<QII")
PREVIEW_SLOTS = 6
PREVIEW_BYTES = 640 * 360 * 4
PREVIEW_STRIDE = PREVIEW_HEADER.size + PREVIEW_BYTES


def map_preview(path):
    if os.name == "nt":
        # The writer keeps GENERIC_WRITE access while Studio has the file open.
        import ctypes
        import msvcrt
        from ctypes import wintypes

        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        open_file = kernel.CreateFileW
        open_file.argtypes = (wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, wintypes.LPVOID,
                              wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE)
        open_file.restype = wintypes.HANDLE
        generic_read = 0x80000000
        share_read_write_delete = 7
        open_existing = 3
        normal_attributes = 0x80
        handle = open_file(str(path), generic_read, share_read_write_delete, None, open_existing, normal_attributes, None)
        if handle == wintypes.HANDLE(-1).value:
            raise ctypes.WinError(ctypes.get_last_error())
        try:
            fd = msvcrt.open_osfhandle(handle, os.O_RDONLY | os.O_BINARY)
        except OSError:
            kernel.CloseHandle.argtypes = (wintypes.HANDLE,)
            kernel.CloseHandle(handle)
            raise
    else:
        fd = os.open(path, os.O_RDONLY)
    try:
        if os.fstat(fd).st_size == PREVIEW_SEQUENCE.size + PREVIEW_SLOTS * PREVIEW_STRIDE:
            return mmap.mmap(fd, 0, access=mmap.ACCESS_READ)
    finally:
        os.close(fd)


class AlloySession:
    def __init__(self, folder):
        self.folder = Path(folder)
        self.preview_map = None
        self.preview_stamp = 0
        self.published_audio_port = None

    def signalled(self, name, consume=False):
        flag = self.folder / name
        if not flag.is_file():
            return False
        if consume:
            flag.unlink()
        return True

    def state(self):
        state = json.loads((self.folder / "state.json").read_text(encoding="utf-8-sig"))
        if state.get("protocol") != 1 or time.time() * 1000 - state["updated"] > 5000:
            raise ValueError("Alloy is unavailable")
        return state

    def initial_state(self):
        deadline = time.monotonic() + 5
        while not self.signalled("closed"):
            try:
                return self.state()
            except (PermissionError, FileNotFoundError) as error:
                # Alloy can be replacing the snapshot as Studio starts.
                if time.monotonic() >= deadline:
                    raise RuntimeError("Could not read Alloy's timeline. Open Visual Studio again from Alloy.") from error
                time.sleep(.05)
        raise RuntimeError("Alloy closed before Visual Studio connected.")

    def audio_port(self, port):
        if port == self.published_audio_port:
            return
        destination = self.folder / "audio-port"
        temporary = self.folder / "audio-port.tmp"
        temporary.write_text(str(port), encoding="ascii")
        # JUCE may briefly hold the current file open without delete sharing.
        for delay in (.01, .02, .04, None):
            try:
                os.replace(temporary, destination)
                break
            except PermissionError:
                if os.name != "nt" or delay is None:
                    raise
                time.sleep(delay)
        self.published_audio_port = port

    def send(self, **request):
        destination = self.folder / "requests" / f"{time.time_ns():020}-{uuid4().hex}.json"
        temporary = destination.with_suffix(".tmp")
        temporary.write_text(json.dumps(request, allow_nan=False), encoding="utf-8")
        os.replace(temporary, destination)

    def latest_preview_sequence(self):
        if self.preview_map is None:
            try:
                self.preview_map = map_preview(self.folder / "preview.frames")
            except OSError:
                return 0
            if self.preview_map is None:
                return 0
        return PREVIEW_SEQUENCE.unpack_from(self.preview_map)[0]

    def preview(self, sequence=None):
        latest = self.latest_preview_sequence()
        if not latest:
            return None
        sequence = latest if sequence is None else sequence
        if sequence == self.preview_stamp or not max(1, latest - PREVIEW_SLOTS + 1) <= sequence <= latest:
            return None
        offset = PREVIEW_SEQUENCE.size + (sequence % PREVIEW_SLOTS) * PREVIEW_STRIDE
        stamp, width, height = PREVIEW_HEADER.unpack_from(self.preview_map, offset)
        if stamp != sequence or not 0 < width <= 640 or not 0 < height <= 360:
            return None
        start = offset + PREVIEW_HEADER.size
        pixels = self.preview_map[start:start + width * height * 4]
        if PREVIEW_SEQUENCE.unpack_from(self.preview_map, offset)[0] != sequence:
            return None
        self.preview_stamp = sequence
        return Image.frombytes("RGB", (width, height), pixels, "raw", "BGRX", 0, -1)

    def close_preview(self):
        if self.preview_map is not None:
            self.preview_map.close()
            self.preview_map = None


def wire_names(control):
    if len(control.default) == 1:
        return (control.name,)
    return tuple(control.name + "_" + axis for axis in "xyz"[:len(control.default)])


def decode(control, values):
    if control.kind == "dropdown":
        return (control.choices[round(values[0] * (len(control.choices) - 1))],)
    if control.kind == "toggle":
        return control.high if values[0] >= .5 else control.low
    if control.kind == "integer":
        return (float(round(control.low[0] + values[0] * (control.high[0] - control.low[0]))),)
    return tuple(low + value * (high - low) for low, high, value in zip(control.low, control.high, values))


def encode(control, values):
    values = control.validate(values)
    if control.kind == "dropdown":
        fractions = (control.choices.index(values[0]) / max(1, len(control.choices) - 1),)
    else:
        fractions = tuple((value - low) / (high - low) if high > low else 0.
                          for low, high, value in zip(control.low, control.high, values))
    return dict(zip(wire_names(control), fractions))


def linked_controls(state, assets):
    """Expose only controls accepted by the current clip's Alloy renderer."""
    parameters = state.get("parameters", {})
    sections = {section: {} for section in ("Scene", "Meta", "Media")}
    remaining = set(parameters)

    def include(section, candidates):
        for control in candidates.values():
            names = wire_names(control)
            if control.kind == "bang" or not remaining.issuperset(names):
                continue
            if control.kind == "dropdown" and parameters[names[0]]["steps"] != len(control.choices):
                continue
            sections[section][control.name] = control
            remaining.difference_update(names)

    key = state.get("scene", "")
    try:
        if key.startswith("studio/"):
            include("Scene", Scene.open(assets / "scenes" / key.removeprefix("studio/")).controls)
        elif key.startswith("glsl/"):
            include("Scene", Scene.open(Path(key.removeprefix("glsl/"))).controls)
    except (OSError, ValueError, KeyError):
        # An external shader may only expose uniforms reported by the renderer.
        pass
    if state.get("layer") == 3:
        remaining.difference_update(("spriteRoute", "spriteInMilkDrop"))
        include("Media", MEDIA)
    else:
        include("Meta", META)
    for name, spec in parameters.items():
        if name not in remaining:
            continue
        steps = spec["steps"]
        default = spec.get("initial", .5)
        kind = "slider"
        if steps == 2:
            kind, default = "toggle", float(default >= .5)
        elif steps > 2:
            kind, default = "integer", float(round(default * (steps - 1)))
        high = float(steps - 1) if steps > 2 else 1.
        section = "Media" if state.get("layer") == 3 else "Scene"
        sections[section][name] = Control(name, spec["label"], section, "", kind, (default,), (0.,), (high,))
    return sections
