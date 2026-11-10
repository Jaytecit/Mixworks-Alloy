"""Input-only WinMM MIDI connection for the Alloy FLkey layout."""

import ctypes as ct
from ctypes import wintypes as wt
import json
from queue import Empty, Queue, Full


CALLBACK = ct.WINFUNCTYPE(None, wt.HANDLE, wt.UINT, ct.c_size_t, ct.c_size_t, ct.c_size_t)
FADERS = (("Meta", "brightness"), ("Meta", "gamma"), ("Meta", "contrast"),
          ("Meta", "hue"), ("Meta", "saturation"), ("Media", "overlayAmount"),
          ("Media", "scale"), ("Media", "speed"), ("Meta", "master"))


class DeviceCaps(ct.Structure):
    _fields_ = [("manufacturer", wt.WORD), ("product", wt.WORD), ("version", wt.UINT),
                ("name", wt.WCHAR * 32), ("support", wt.DWORD)]


class Midi:
    def __init__(self, profile):
        self.events = Queue(maxsize=512)
        self.handle, self.name = wt.HANDLE(), ""
        self.pressed = set()
        self.set_profile(profile)
        self.callback = CALLBACK(self.receive)
        self.winmm = ct.WinDLL("winmm")
        signatures = {
            "midiInGetNumDevs": [],
            "midiInGetDevCapsW": [ct.c_size_t, ct.POINTER(DeviceCaps), wt.UINT],
            "midiInOpen": [ct.POINTER(wt.HANDLE), wt.UINT, CALLBACK, ct.c_size_t, wt.DWORD],
            "midiInStart": [wt.HANDLE], "midiInStop": [wt.HANDLE],
            "midiInReset": [wt.HANDLE], "midiInClose": [wt.HANDLE],
            "midiInGetErrorTextW": [wt.UINT, wt.LPWSTR, wt.UINT],
        }
        for name, args in signatures.items():
            function = getattr(self.winmm, name)
            function.restype, function.argtypes = wt.UINT, args

    def set_profile(self, profile):
        with profile.open(encoding="utf-8-sig") as source:
            records = json.load(source)
        mappings = {}
        for row in records:
            status, channel, key = row["status"], row["channel"], row["number"]
            if status not in (0x90, 0xB0) or not 1 <= channel <= 16 or not 0 <= key <= 127:
                raise ValueError("Invalid MIDI mapping in the Alloy profile.")
            mappings[(status | (channel - 1), key)] = row["action"]
        if not mappings:
            raise ValueError("The Alloy profile has no supported mappings.")
        self.mappings = mappings
        self.pressed.clear()

    def ports(self):
        devices = {}
        for index in range(self.winmm.midiInGetNumDevs()):
            caps = DeviceCaps()
            if not self.winmm.midiInGetDevCapsW(index, ct.byref(caps), ct.sizeof(caps)):
                devices[index] = caps.name
        return devices

    def check(self, result):
        if result:
            message = ct.create_unicode_buffer(256)
            self.winmm.midiInGetErrorTextW(result, message, len(message))
            raise OSError(message.value or f"MIDI input error {result}.")

    def open(self, index, name):
        self.close()
        self.check(self.winmm.midiInOpen(ct.byref(self.handle), index, self.callback, 0, 0x30000))
        try:
            self.check(self.winmm.midiInStart(self.handle))
        except Exception:
            self.close()
            raise
        self.name = name

    def receive(self, handle, kind, instance, packet, timestamp):
        if kind == 0x3C3:  # MIM_DATA
            try:
                self.events.put_nowait((packet & 255, (packet >> 8) & 127, (packet >> 16) & 127))
            except Full:
                pass

    def route(self, status, key, value):
        if status in (0xFA, 0xFB, 0xFC):
            return ("stop" if status == 0xFC else "play", 1.)
        if status & 0xF0 == 0xE0:
            bend = (value << 7 | key) - 8192
            return "wheel-x", .5 + .5 * bend / (8192 if bend < 0 else 8191)
        if status & 0xF0 == 0x80:
            status, value = status + 0x10, 0
        message = (status, key)
        action = self.mappings.get(message, "")
        if not action and status & 0xF0 == 0xB0 and key == 1:
            return "wheel-y", value / 127.
        if not action:
            return None
        if action.startswith(("desk-pot-", "volume-", "pan-")) or action == "master":
            return action, value / 127.
        if not value:
            self.pressed.discard(message)
        elif message not in self.pressed:
            self.pressed.add(message)
            return action, 1.
        return None

    def poll(self):
        for _ in range(128):
            try:
                event = self.events.get_nowait()
            except Empty:
                break
            routed = self.route(*event)
            if routed:
                yield routed

    def close(self):
        if self.handle:
            self.winmm.midiInStop(self.handle)
            self.winmm.midiInReset(self.handle)
            self.winmm.midiInClose(self.handle)
        self.handle, self.name = wt.HANDLE(), ""
        self.pressed.clear()
        while not self.events.empty():
            self.events.get_nowait()
