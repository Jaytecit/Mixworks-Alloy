"""Physical monitor bounds for Studio's borderless GPU output."""

from contextlib import contextmanager
import ctypes as ct
from ctypes import wintypes as win
import os


if os.name == "nt":
    class MonitorInfo(ct.Structure):
        _fields_ = [("size", win.DWORD), ("bounds", win.RECT),
                    ("work", win.RECT), ("flags", win.DWORD)]

    MonitorCallback = ct.WINFUNCTYPE(win.BOOL, win.HMONITOR, win.HDC, ct.POINTER(win.RECT), win.LPARAM)
    user = ct.WinDLL("user32", use_last_error=True)
    for name, result, args in (
        ("SetThreadDpiAwarenessContext", ct.c_void_p, [ct.c_void_p]),
        ("MonitorFromWindow", win.HMONITOR, [win.HWND, win.DWORD]),
        ("EnumDisplayMonitors", win.BOOL, [win.HDC, ct.POINTER(win.RECT), MonitorCallback, win.LPARAM]),
        ("GetMonitorInfoW", win.BOOL, [win.HMONITOR, ct.POINTER(MonitorInfo)]),
        ("SetWindowPos", win.BOOL, [win.HWND, win.HWND, ct.c_int, ct.c_int, ct.c_int, ct.c_int, win.UINT]),
    ):
        function = getattr(user, name)
        function.restype, function.argtypes = result, args


@contextmanager
def physical_coordinates():
    if os.name != "nt":
        yield
        return
    previous = user.SetThreadDpiAwarenessContext(ct.c_void_p(-4))  # Per-monitor v2
    try:
        yield
    finally:
        if previous:
            user.SetThreadDpiAwarenessContext(previous)


def output_bounds(root):
    if os.name != "nt":
        return 0, 0, root.winfo_screenwidth(), root.winfo_screenheight()
    monitors = []

    @MonitorCallback
    def collect(monitor, _dc, _rect, _data):
        info = MonitorInfo(size=ct.sizeof(MonitorInfo))
        if user.GetMonitorInfoW(monitor, ct.byref(info)):
            rect = info.bounds
            monitors.append((monitor, bool(info.flags & 1),
                             (rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top)))
        return True

    with physical_coordinates():
        current = user.MonitorFromWindow(root.winfo_id(), 2)  # nearest monitor
        if not user.EnumDisplayMonitors(None, None, collect, 0) or not monitors:
            raise RuntimeError("Windows could not find a display for fullscreen output.")
    _, _, bounds = min(monitors, key=lambda display: (display[0] == current, not display[1], display[2]))
    return bounds


def place_window(hwnd, bounds):
    with physical_coordinates():
        if not user.SetWindowPos(hwnd, None, *bounds, 0x0010 | 0x0200):  # no activation or owner z-order change
            raise ct.WinError(ct.get_last_error())
