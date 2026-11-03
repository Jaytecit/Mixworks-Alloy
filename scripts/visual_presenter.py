"""GPU-only presentation into Alloy-owned native preview/output windows.

The renderer keeps texture ownership. Each child has a shared GL context and
its own framebuffer; fences order texture reads and subsequent writes without
reading pixels back to Python. Windows parents must belong to the launching
Alloy process. No desktop/global display mode is changed.
"""
import ctypes
from ctypes import wintypes
import math
import os
import time
import glfw
from OpenGL import GL as gl


class Presenter:
    def __init__(self, renderer, owner_pid):
        if os.name != 'nt':
            raise RuntimeError('Native Alloy presentation requires Windows')
        self.renderer, self.owner_pid = renderer, owner_pid
        self.windows = {}
        self.count = 0
        self.previous = None
        self.clock_identity = None
        self.histogram = {}
        self.maximum_gap = 0.
        self.user = ctypes.WinDLL('user32', use_last_error=True)
        declarations = {
            'IsWindow': ([wintypes.HWND], wintypes.BOOL),
            'GetWindowThreadProcessId': ([wintypes.HWND, ctypes.POINTER(wintypes.DWORD)], wintypes.DWORD),
            'SetParent': ([wintypes.HWND, wintypes.HWND], wintypes.HWND),
            'SetWindowLongPtrW': ([wintypes.HWND, ctypes.c_int, ctypes.c_ssize_t], ctypes.c_ssize_t),
            'SetWindowPos': ([wintypes.HWND, wintypes.HWND, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, wintypes.UINT], wintypes.BOOL),
            'PostMessageW': ([wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM], wintypes.BOOL),
            'GetWindowDpiAwarenessContext': ([wintypes.HWND], ctypes.c_void_p),
            'SetThreadDpiAwarenessContext': ([ctypes.c_void_p], ctypes.c_void_p),
        }
        for name, (args, result) in declarations.items():
            function = getattr(self.user, name); function.argtypes = args; function.restype = result

    def validate(self, descriptions):
        if not isinstance(descriptions, list) or len(descriptions) > 2:
            raise ValueError('At most two native visual views are supported')
        result = {}
        for item in descriptions:
            if not isinstance(item, dict) or item.get('id') not in ('preview', 'output') or item['id'] in result:
                raise ValueError('Invalid native visual view identity')
            parent = item.get('parent')
            pid = wintypes.DWORD()
            if type(parent) is not int or parent <= 0 or not self.user.IsWindow(parent):
                raise ValueError('Visual parent window is unavailable')
            self.user.GetWindowThreadProcessId(parent, ctypes.byref(pid))
            if pid.value != self.owner_pid:
                raise ValueError('Visual parent does not belong to Alloy')
            bounds = [item.get(k) for k in ('x', 'y', 'width', 'height')]
            if any(type(v) is not int for v in bounds) or not all(0 <= v <= 32768 for v in bounds[:2]) or not all(1 <= v <= 16384 for v in bounds[2:]):
                raise ValueError('Invalid native visual view bounds')
            result[item['id']] = dict(parent=parent, bounds=bounds)
        return result

    def key(self, parent, key):
        self.user.PostMessageW(parent, 0x0100, key, 0)  # WM_KEYDOWN/UP to this owned window only.
        self.user.PostMessageW(parent, 0x0101, key, 0)

    def create(self, name, description):
        parent = description['parent']
        # Match the native parent's DPI context before creating/reparenting.
        previous_dpi = self.user.SetThreadDpiAwarenessContext(self.user.GetWindowDpiAwarenessContext(parent))
        window = None
        try:
            glfw.window_hint(glfw.VISIBLE, False)
            glfw.window_hint(glfw.DECORATED, False)
            glfw.window_hint(glfw.FOCUSED, False)
            window = glfw.create_window(64, 64, 'Alloy GPU '+name, None, self.renderer.window)
            if not window:
                raise RuntimeError('Could not create the shared visual output context')
            hwnd = glfw.get_win32_window(window)
            self.user.SetWindowLongPtrW(hwnd, -16, 0x40000000 | 0x04000000 | 0x02000000)  # CHILD/CLIPSIBLINGS/CLIPCHILDREN
            ctypes.set_last_error(0)
            self.user.SetParent(hwnd, parent)
            if ctypes.get_last_error():
                raise ctypes.WinError(ctypes.get_last_error())
            glfw.make_context_current(window)
            glfw.swap_interval(0)  # One producer clock; do not block twice for two views.
            fbo = int(gl.glGenFramebuffers(1))
            entry = dict(window=window, hwnd=hwnd, parent=parent, fbo=fbo, bounds=None, click=0.)
            def keyboard(w, key, scan, action, mods):
                if action == glfw.PRESS and name == 'output' and key in (glfw.KEY_ESCAPE, glfw.KEY_F11):
                    self.key(parent, 0x1b if key == glfw.KEY_ESCAPE else 0x7a)
            def mouse(w, button, action, mods):
                if name == 'output' and button == glfw.MOUSE_BUTTON_LEFT and action == glfw.PRESS:
                    now = time.monotonic()
                    if now-entry['click'] < .35:
                        self.key(parent, 0x7a); entry['click'] = 0.
                    else:
                        entry['click'] = now
            glfw.set_key_callback(window, keyboard)
            glfw.set_mouse_button_callback(window, mouse)
            glfw.set_window_close_callback(window, lambda w: self.user.PostMessageW(parent, 0x0010, 0, 0))
            if name == 'output':
                glfw.set_input_mode(window, glfw.CURSOR, glfw.CURSOR_HIDDEN)
            self.windows[name] = entry
            window = None
        finally:
            if window:
                glfw.destroy_window(window)
            self.user.SetThreadDpiAwarenessContext(previous_dpi)
            glfw.make_context_current(self.renderer.window)

    def remove(self, name):
        entry = self.windows.pop(name)
        glfw.make_context_current(entry['window'])
        gl.glDeleteFramebuffers(1, [entry['fbo']])
        glfw.destroy_window(entry['window'])
        glfw.make_context_current(self.renderer.window)

    def configure(self, descriptions):
        wanted = self.validate(descriptions)
        for name in list(self.windows):
            if name not in wanted or self.windows[name]['parent'] != wanted[name]['parent']:
                self.remove(name)
        for name, description in wanted.items():
            if name not in self.windows:
                self.create(name, description)
            entry = self.windows[name]
            if entry['bounds'] != description['bounds']:
                if not self.user.SetWindowPos(entry['hwnd'], None, *description['bounds'], 0x0010 | 0x0040 | 0x0020):
                    raise ctypes.WinError()  # NOACTIVATE/SHOWWINDOW/FRAMECHANGED
                entry['bounds'] = description['bounds']
        glfw.poll_events()

    def present(self, texture, width, height, generation, playing):
        ready = gl.glFenceSync(gl.GL_SYNC_GPU_COMMANDS_COMPLETE, 0)
        gl.glFlush()
        reads = []
        try:
            for entry in self.windows.values():
                glfw.make_context_current(entry['window'])
                # PyOpenGL's generated constant is incorrect on this Windows
                # runtime. GL requires the full unsigned 64-bit all-ones value.
                gl.glWaitSync(ready, 0, 0xffffffffffffffff)
                gl.glBindFramebuffer(gl.GL_DRAW_FRAMEBUFFER, 0)
                gl.glClearColor(0., 0., 0., 1.)
                gl.glClear(gl.GL_COLOR_BUFFER_BIT)
                if texture:
                    gl.glBindFramebuffer(gl.GL_READ_FRAMEBUFFER, entry['fbo'])
                    gl.glFramebufferTexture2D(gl.GL_READ_FRAMEBUFFER, gl.GL_COLOR_ATTACHMENT0, gl.GL_TEXTURE_2D, texture, 0)
                    if gl.glCheckFramebufferStatus(gl.GL_READ_FRAMEBUFFER) != gl.GL_FRAMEBUFFER_COMPLETE:
                        raise RuntimeError('Visual presentation framebuffer is incomplete')
                    w, h = glfw.get_framebuffer_size(entry['window'])
                    scale = min(w/width, h/height)
                    fw, fh = round(width*scale), round(height*scale)
                    x, y = (w-fw)//2, (h-fh)//2
                    gl.glBlitFramebuffer(0, 0, width, height, x, y, x+fw, y+fh, gl.GL_COLOR_BUFFER_BIT, gl.GL_LINEAR)
                reads.append(gl.glFenceSync(gl.GL_SYNC_GPU_COMMANDS_COMPLETE, 0))
                gl.glFlush()
                glfw.swap_buffers(entry['window'])
        finally:
            glfw.make_context_current(self.renderer.window)
            for fence in reads:
                gl.glWaitSync(fence, 0, 0xffffffffffffffff)
                gl.glDeleteSync(fence)
            gl.glDeleteSync(ready)
        now = time.perf_counter()
        identity = (generation, playing)
        if identity != self.clock_identity:
            self.previous = None; self.clock_identity = identity
        if self.windows and texture:
            if self.previous is not None and playing:
                gap = (now-self.previous)*1000
                key = str(math.ceil(gap))
                self.histogram[key] = self.histogram.get(key, 0)+1
                self.maximum_gap = max(self.maximum_gap, gap)
            self.previous = now; self.count += 1
        else:
            self.previous = None
        return dict(presented=bool(self.windows and texture), presentation='shared-gpu-texture',
                    presentedFrames=self.count, maxGapMs=self.maximum_gap, gapHistogramCeilMs=self.histogram.copy(),
                    views=list(self.windows), pixelTransferBytes=0)

    def poll(self):
        glfw.poll_events()

    def close(self):
        for name in list(self.windows):
            self.remove(name)
