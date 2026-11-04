"""Optional libprojectM 4.1.8 adapter. Runs only in Alloy's isolated GL worker.

PCM is the existing mixed-audio feed (mono, selected from the louder stereo side),
not loopback or a microphone. The upstream renderer uses a wall clock; seeks and
clip changes restart feedback, and this first integration does not promise
deterministic timeline reconstruction or offline export.
"""
import ctypes as ct
import os
from pathlib import Path
import glfw
import numpy as np
from OpenGL import GL as gl
from original_visual_gl import Target

FAILED = ct.CFUNCTYPE(None, ct.c_char_p, ct.c_char_p, ct.c_void_p)


class Library:
    def __init__(self, root):
        path = Path(root).resolve()/'milkdrop'/'projectM-4.dll'
        if not path.is_file():
            raise RuntimeError('MilkDrop support is unavailable: install the optional projectM runtime.')
        self.search = os.add_dll_directory(str(path.parent)) if os.name == 'nt' else None
        self.glew = ct.CDLL(str(path.with_name('glew32.dll')))
        ct.c_ubyte.in_dll(self.glew, 'glewExperimental').value = 1
        self.glew.glewInit.restype, self.glew.glewInit.argtypes = ct.c_uint, []
        if self.glew.glewInit() != 0:
            raise RuntimeError('Could not initialise MilkDrop OpenGL functions.')
        # GLEW's core-profile probing can leave GL_INVALID_ENUM behind.
        for _ in range(8):
            if gl.glGetError() == gl.GL_NO_ERROR: break
        self.dll = ct.CDLL(str(path))
        for name, returns, args in (
            ('create', ct.c_void_p, []), ('destroy', None, [ct.c_void_p]),
            ('set_window_size', None, [ct.c_void_p, ct.c_size_t, ct.c_size_t]),
            ('set_mesh_size', None, [ct.c_void_p, ct.c_size_t, ct.c_size_t]),
            ('set_fps', None, [ct.c_void_p, ct.c_int32]),
            ('set_preset_locked', None, [ct.c_void_p, ct.c_bool]),
            ('set_hard_cut_enabled', None, [ct.c_void_p, ct.c_bool]),
            ('set_beat_sensitivity', None, [ct.c_void_p, ct.c_float]),
            ('set_texture_search_paths', None, [ct.c_void_p, ct.POINTER(ct.c_char_p), ct.c_size_t]),
            ('set_preset_switch_failed_event_callback', None, [ct.c_void_p, FAILED, ct.c_void_p]),
            ('load_preset_file', None, [ct.c_void_p, ct.c_char_p, ct.c_bool]),
            ('pcm_add_float', None, [ct.c_void_p, ct.POINTER(ct.c_float), ct.c_uint, ct.c_int]),
            ('opengl_render_frame', None, [ct.c_void_p]),
        ):
            function = getattr(self.dll, 'projectm_'+name)
            function.restype, function.argtypes = returns, args
            setattr(self, name, function)


class MilkDropScene:
    def __init__(self, library, entry, width, height, key, renderer):
        self.library, self.key = library, key
        self.width, self.height = width, height
        self.frames, self.handle, self.final = 0, None, None
        self.handoff, self.identity, self.clock = None, None, None
        self.error = None
        self.callback = FAILED(self._failed)
        try:
            # projectM 4.1 renders into framebuffer zero. Size the hidden drawable
            # and copy into our owned target before the normal compositor runs.
            glfw.set_window_size(renderer.window, width, height)
            self.final = Target(width, height)
            self.handle = library.create()
            if not self.handle:
                raise RuntimeError('Could not create the MilkDrop renderer.')
            library.set_preset_switch_failed_event_callback(self.handle, self.callback, None)
            library.set_window_size(self.handle, width, height)
            library.set_mesh_size(self.handle, 48, 36)
            library.set_fps(self.handle, 60)
            library.set_preset_locked(self.handle, True)
            library.set_hard_cut_enabled(self.handle, False)
            paths = [os.fsencode(p) for p in entry['texturePaths'] if Path(p).is_dir()]
            library.set_texture_search_paths(self.handle, (ct.c_char_p*len(paths))(*paths), len(paths))
            library.load_preset_file(self.handle, os.fsencode(entry['preset']), False)
            self._check()
        except BaseException:
            self.close()
            raise

    @property
    def byte_size(self):
        # Host target only; upstream allocates its own feedback/blur/noise targets.
        return self.final.byte_size

    def _failed(self, filename, message, context):
        self.error = (message or b'Preset rejected').decode('utf-8', errors='replace')

    def _check(self):
        if self.error:
            raise RuntimeError('MilkDrop preset could not render: '+self.error[:1000])

    def render_pcm(self, actor, pcm):
        identity = (actor['revision'], actor['time'])
        if identity == self.identity:
            return
        samples = np.ascontiguousarray(np.clip(pcm, -1., 1.), dtype=np.float32)
        self.library.set_beat_sensitivity(self.handle, .5+actor['controls']['intensity'])
        self.library.pcm_add_float(self.handle, samples.ctypes.data_as(ct.POINTER(ct.c_float)), len(samples), 1)
        # libprojectM owns GL state during the call. Reset shared state before
        # handing its final pixels to Alloy's normal scene compositor.
        gl.glBindFramebuffer(gl.GL_FRAMEBUFFER, 0)
        gl.glDisable(gl.GL_SCISSOR_TEST)
        self.library.opengl_render_frame(self.handle)
        self._check()
        gl.glBindFramebuffer(gl.GL_READ_FRAMEBUFFER, 0)
        gl.glReadBuffer(gl.GL_BACK)
        gl.glBindFramebuffer(gl.GL_DRAW_FRAMEBUFFER, self.final.fbos[0])
        gl.glBlitFramebuffer(0, 0, self.width, self.height, 0, 0, self.width, self.height,
                             gl.GL_COLOR_BUFFER_BIT, gl.GL_NEAREST)
        gl.glDisable(gl.GL_BLEND)
        gl.glDisable(gl.GL_DEPTH_TEST)
        gl.glDisable(gl.GL_SCISSOR_TEST)
        for unit in range(16): gl.glBindSampler(unit, 0)
        gl.glActiveTexture(gl.GL_TEXTURE0)
        self.identity = identity
        self.frames += 1

    def close(self):
        if self.handle:
            self.library.destroy(self.handle)
            self.handle = None
        if self.final:
            self.final.close()
            self.final = None
