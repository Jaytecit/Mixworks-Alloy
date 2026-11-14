"""Browse Studio GLSL scenes with Windows desktop audio and optional artwork."""

import argparse
from collections import deque
from contextlib import contextmanager
from datetime import datetime
from hashlib import sha256
import json
from pathlib import Path
import re
import sys
from tempfile import NamedTemporaryFile
import time
import tkinter as tk
from tkinter import filedialog, messagebox, ttk
from uuid import uuid4

import numpy as np
from PIL import Image, ImageTk

from audio import Audio
from engine import Engine
from media import MEDIA, META, Media
from scene import Scene


ROOT = Path(__file__).resolve().parent
DEFAULT_SCENES = ROOT / "scenes"
COMPARISON_SCENES = ROOT / "scene-viewer-scenes"
LOCAL = ROOT.parent / ".local"
VIEWER_DEPS = LOCAL / "scene-viewer-deps"
if VIEWER_DEPS.is_dir():
    sys.path.insert(0, str(VIEWER_DEPS))


class DesktopAudio:
    def __init__(self):
        import pyaudiowpatch as wasapi

        self.port = wasapi.PyAudio()
        self.stream = None
        self.history = deque(maxlen=8)
        try:
            device = self.port.get_default_wasapi_loopback()
            self.name = device["name"].removesuffix(" [Loopback]")
            self.rate = int(device["defaultSampleRate"])
            self.channels = int(device["maxInputChannels"])

            def receive(data, _count, _timing, _flags):
                self.history.append((time.monotonic(), data))
                return None, wasapi.paContinue

            self.stream = self.port.open(format=wasapi.paFloat32, channels=self.channels,
                                         rate=self.rate, input=True, input_device_index=device["index"],
                                         frames_per_buffer=1024, stream_callback=receive)
        except Exception:
            self.close()
            raise

    def clock(self):
        return 0., 0., 0.

    def samples(self, offsets):
        history = self.history.copy()
        if not history or time.monotonic() - history[-1][0] > .3:
            return np.zeros(len(offsets), dtype=np.float32)
        frames = np.frombuffer(b"".join(data for _, data in history), dtype=np.float32).reshape(-1, self.channels)
        frames = frames[-int(-offsets[0] * self.rate) - 2:]
        channel = int(np.argmax(np.mean(frames * frames, axis=0)))
        return np.interp(len(frames) + offsets * self.rate, np.arange(len(frames)), frames[:, channel], left=0.)

    def close(self):
        try:
            if self.stream:
                self.stream.close()
        finally:
            self.port.terminate()


def catalogue(folder):
    scenes = []
    for path in folder.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in (".glsl", ".frag", ".fs"):
            continue
        if path.name != "main.glsl" and (path.parent / "main.glsl").is_file():
            continue
        try:
            title = Scene.open(path).title
        except (OSError, ValueError, KeyError, TypeError):
            title = path.parent.name if path.name == "main.glsl" else path.stem
        scenes.append((title, path))
    if folder == DEFAULT_SCENES:
        scenes.extend(catalogue(COMPARISON_SCENES))
    return sorted(scenes, key=lambda item: (item[0].casefold(), item[1]))


def write_json(path, data):
    write_bytes(path, (json.dumps(data, ensure_ascii=False, indent=2) + "\n").encode("utf-8"))


def write_bytes(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    with NamedTemporaryFile(dir=path.parent, mode="wb", delete=False) as output:
        temporary = Path(output.name)
        try:
            output.write(data)
            output.close()
            temporary.replace(path)
        finally:
            output.close()
            temporary.unlink(missing_ok=True)


def active_alloy_catalogue():
    output = ROOT.parent / "out"
    launcher = output / "Launch.cmd"
    if not launcher.exists():
        return None
    match = re.search(r'(?im)^\s*start\s+""\s+"%~dp0releases\\([a-z0-9.-]+)\\Mixworks\.exe"\s*$',
                      launcher.read_text(encoding="utf-8-sig"))
    if not match or match[1] in (".", ".."):
        raise ValueError("Could not identify the local Alloy release from out/Launch.cmd.")
    release = output / "releases" / match[1]
    index = release / "original-visualiser-runtime/studio/scene-library.json"
    if index.resolve() != index or not index.is_file() or not (release / "Mixworks.exe").is_file():
        raise ValueError("The local Alloy release has no usable GLSL catalogue.")
    return index


def without_scenes(catalogue, folders):
    return {family: remaining for family, entries in catalogue.items()
            if (remaining := [entry for entry in entries if entry["folder"] not in folders])}


@contextmanager
def catalogue_lock(folder):
    if folder != DEFAULT_SCENES or sys.platform != "win32":
        yield
        return
    import msvcrt

    LOCAL.mkdir(parents=True, exist_ok=True)
    with (LOCAL / "scene-catalogue.lock").open("a+b") as lock:
        lock.seek(0)
        msvcrt.locking(lock.fileno(), msvcrt.LK_NBLCK, 1)
        try:
            yield
        finally:
            lock.seek(0)
            msvcrt.locking(lock.fileno(), msvcrt.LK_UNLCK, 1)


def archive_scenes(paths, folder):
    with catalogue_lock(folder):
        return move_to_recovery(paths, folder)


def move_to_recovery(paths, folder):
    stamp = f"{datetime.now():%Y%m%d-%H%M%S}-{uuid4().hex[:8]}"
    archive = (LOCAL / "removed-scenes" if folder.is_relative_to(ROOT)
               else folder.with_name(f"{folder.name} - Removed Scenes")) / stamp
    moves = {}
    index = ROOT / "scene-library.json"
    removed_folders = set()
    for path in paths:
        source_root = COMPARISON_SCENES if folder == DEFAULT_SCENES and path.is_relative_to(COMPARISON_SCENES) else folder
        if not path.is_relative_to(source_root) or path == source_root or path.resolve(strict=True) != path:
            raise ValueError(f"Scene path is outside its library or is a link: {path}")
        parent = path.parent
        # A scene folder owns its assets. Preserve the library root and any
        # folders that contain additional, independently browsable scenes.
        if path.name == "main.glsl" and parent != source_root and not any(
            nested.parent != parent for _, nested in catalogue(parent)
        ):
            items = [parent]
        else:
            descriptor = parent / "scene.json"
            if path.name != "main.glsl" or not descriptor.is_file():
                descriptor = path.with_suffix(".json")
            items = [path]
            if descriptor.is_file():
                if descriptor.resolve(strict=True) != descriptor:
                    raise ValueError(f"Scene descriptor is a link: {descriptor}")
                items.append(descriptor)
        for item in items:
            section = source_root.name if folder == DEFAULT_SCENES else "scenes"
            moves[item] = archive / section / item.relative_to(source_root)
        if folder == DEFAULT_SCENES and path.name == "main.glsl" and parent.parent == DEFAULT_SCENES:
            removed_folders.add(parent.name)

    updates = {}
    if removed_folders and index.is_file():
        original = index.read_bytes()
        before = json.loads(original)
        after = without_scenes(before, removed_folders)
        updates[index] = (original, after, "scene-library.json")
        profiles = ROOT / "authoring/families.json"
        if profiles.is_file():
            original_profiles = profiles.read_bytes()
            authoring = json.loads(original_profiles)
            for profile in authoring:
                family = profile["title"]
                if before.get(family) != after.get(family):
                    profile["kept"] = after.get(family, [])
            updates[profiles] = (original_profiles, authoring, "families.json")
        installed = active_alloy_catalogue()
        if installed is not None:
            original_installed = installed.read_bytes()
            updates[installed] = (original_installed,
                                  without_scenes(json.loads(original_installed), removed_folders),
                                  "alloy-scene-library.json")
    if archive.resolve() != archive or any(target.resolve() != target for target in moves.values()):
        raise ValueError(f"Recovery folder contains a link: {archive}")
    moved, updated = [], []
    try:
        for original, _, name in updates.values():
            archive.mkdir(parents=True, exist_ok=True)
            (archive / name).write_bytes(original)
        for source, target in moves.items():
            target.parent.mkdir(parents=True, exist_ok=True)
            source.rename(target)
            moved.append((source, target))
        for path, (_, data, _) in updates.items():
            write_json(path, data)
            updated.append(path)
    except (OSError, ValueError, KeyError, TypeError) as error:
        rollback_errors = []
        for path in reversed(updated):
            try:
                write_bytes(path, updates[path][0])
            except OSError:
                rollback_errors.append(str(path))
        for source, target in reversed(moved):
            try:
                target.rename(source)
            except OSError:
                rollback_errors.append(str(target))
        if rollback_errors:
            raise OSError(f"{error}\n\nSome changes could not be rolled back. Recovery files are in {archive}.") from error
        raise
    return archive


class SceneViewer:
    def __init__(self, root, folder, *, audio_enabled=True):
        self.root, self.folder = root, folder
        self.scenes = []
        self.index = None
        library_key = sha256(str(folder).casefold().encode("utf-8")).hexdigest()[:20]
        self.mark_file = LOCAL / "scene-viewer-marks" / f"{library_key}.json"
        self.marks = set()
        self.scene_ready = False
        self.photo = None
        self.elapsed, self.last_frame = 0., time.monotonic()
        self.frame_id = None
        self.engine = Engine(size=(640, 360))
        self.analysis = Audio()
        self.analysis.mode = "Desktop"
        self.media = Media()
        self.meta = {name: control.default for name, control in META.items()}
        self.media_values = {name: control.default for name, control in MEDIA.items()}
        self.media_values.update(spriteRoute=(1.,), overlayAmount=(1.,), nestedAmount=(1.,))

        root.title("Mixworks Scene Viewer")
        root.geometry("1140x720")
        root.minsize(770, 500)
        root.configure(bg="#171b22")
        root.protocol("WM_DELETE_WINDOW", self.close)
        style = ttk.Style(root)
        style.theme_use("clam")
        style.configure(".", background="#222832", foreground="#edf1f6", font=("Segoe UI", 10))
        style.configure("TButton", padding=(9, 5), background="#323b48", borderwidth=0)
        style.map("TButton", background=[("active", "#49586a"), ("disabled", "#222832")])
        style.configure("ViewerTitle.TLabel", foreground="#f1be94", font=("Segoe UI", 16, "bold"))

        toolbar = ttk.Frame(root, padding=(12, 9))
        toolbar.pack(fill="x")
        self.back = ttk.Button(toolbar, text="← Back", command=lambda: self.step(-1))
        self.forward = ttk.Button(toolbar, text="Forward →", command=lambda: self.step(1))
        self.back.pack(side="left")
        self.forward.pack(side="left", padx=(6, 15))
        self.title = ttk.Label(toolbar, style="ViewerTitle.TLabel")
        self.title.pack(side="left")
        ttk.Button(toolbar, text="Reload scenes", command=self.reload).pack(side="right")

        content = ttk.Panedwindow(root, orient="horizontal")
        content.pack(fill="both", expand=True, padx=12, pady=(0, 8))
        library = ttk.Frame(content)
        display = ttk.Frame(content)
        content.add(library, weight=0)
        content.add(display, weight=1)
        mark_controls = ttk.Frame(library)
        mark_controls.pack(side="bottom", fill="x", pady=(6, 0))
        self.mark_button = ttk.Button(mark_controls, text="Mark", command=self.toggle_mark)
        self.mark_button.pack(side="left", padx=(0, 5))
        self.remove_button = ttk.Button(mark_controls, text="Remove marked", command=self.remove_marked)
        self.remove_button.pack(side="left")
        self.mark_count = ttk.Label(mark_controls)
        self.mark_count.pack(side="right", padx=6)
        self.listbox = tk.Listbox(library, width=36, bg="#222832", fg="#e2e7ef",
                                  selectbackground="#c78f67", selectforeground="#15191e",
                                  activestyle="none", exportselection=False, borderwidth=0,
                                  highlightthickness=1, highlightbackground="#3a4453", font=("Segoe UI", 10))
        scrollbar = ttk.Scrollbar(library, command=self.listbox.yview)
        self.listbox.configure(yscrollcommand=scrollbar.set)
        scrollbar.pack(side="right", fill="y")
        self.listbox.pack(fill="both", expand=True)
        self.listbox.bind("<<ListboxSelect>>", self.choose)
        self.preview = tk.Canvas(display, bg="#0c0e13", highlightthickness=0)
        self.preview.pack(fill="both", expand=True)
        self.preview_image = self.preview.create_image(0, 0)

        actions = ttk.Frame(display, padding=(8, 8))
        actions.pack(fill="x")
        ttk.Button(actions, text="Choose Nested sprite…", command=self.pick_sprite).pack(side="left")
        self.clear_sprite = ttk.Button(actions, text="Clear", command=self.remove_sprite)
        self.clear_sprite.pack(side="left", padx=6)
        self.sprite_status = ttk.Label(actions, text="No sprite")
        self.sprite_status.pack(side="left", padx=4)
        ttk.Button(actions, text="Reconnect audio", command=self.connect_audio).pack(side="right")
        self.audio_level = ttk.Progressbar(actions, maximum=1., length=80)
        self.audio_level.pack(side="right", padx=8)

        footer = ttk.Frame(root, padding=(12, 0, 12, 8))
        footer.pack(fill="x")
        self.status = ttk.Label(footer)
        self.status.pack(side="left", fill="x", expand=True)
        self.audio_status = ttk.Label(footer)
        self.audio_status.pack(side="right", padx=(8, 0))
        root.bind("<Left>", lambda _: self.step(-1))
        root.bind("<Right>", lambda _: self.step(1))
        root.bind("<Control-m>", lambda _: self.toggle_mark())
        self.remove_sprite()
        self.reload()
        if audio_enabled:
            self.connect_audio()
        else:
            self.audio_status.configure(text="Desktop audio off")
        self.tick()

    def reload(self):
        previous_index = self.index or 0
        selected = self.scenes[self.index][1] if self.index is not None else None
        self.scenes = catalogue(self.folder)
        self.index = None
        try:
            self.marks = self.load_marks()
        except (OSError, ValueError, TypeError) as error:
            messagebox.showerror("Could not read scene marks", str(error), parent=self.root)
        self.listbox.delete(0, "end")
        for i in range(len(self.scenes)):
            self.listbox.insert("end", self.list_label(i))
        for button in (self.back, self.forward):
            button.state(["!disabled" if self.scenes else "disabled"])
        if self.scenes:
            fallback = min(previous_index, len(self.scenes) - 1)
            self.show(next((i for i, (_, path) in enumerate(self.scenes) if path == selected), fallback))
        else:
            self.scene_ready = False
            self.preview.itemconfigure(self.preview_image, image="")
            self.title.configure(text="No scenes")
            self.status.configure(text=f"No GLSL scenes found in {self.folder}")
        self.update_mark_controls()

    def list_label(self, index):
        title, path = self.scenes[index]
        if path.is_relative_to(COMPARISON_SCENES):
            variant, separator, version = title.rpartition(" — ")
            if separator:
                title = f"{version} · {variant.rsplit(' / ', 1)[-1]}"
        return f"● {title}" if path in self.marks else title

    def load_marks(self):
        try:
            saved = json.loads(self.mark_file.read_text(encoding="utf-8"))
        except FileNotFoundError:
            return set()
        if not isinstance(saved, list) or not all(isinstance(path, str) for path in saved):
            raise ValueError("The saved marks file must contain a list of scene paths.")
        return {Path(path) for path in saved}

    def update_mark_controls(self):
        current_marked = self.index is not None and self.scenes[self.index][1] in self.marks
        count = sum(path in self.marks for _, path in self.scenes)
        self.mark_button.configure(text="Unmark" if current_marked else "Mark")
        self.mark_button.state(["!disabled" if self.index is not None else "disabled"])
        self.remove_button.state(["!disabled" if count else "disabled"])
        self.mark_count.configure(text=f"{count} marked")

    def toggle_mark(self):
        if self.index is None:
            return "break"
        path = self.scenes[self.index][1]
        try:
            marks = self.load_marks() ^ {path}
            write_json(self.mark_file, sorted(str(mark) for mark in marks))
        except (OSError, ValueError, TypeError) as error:
            messagebox.showerror("Could not save scene marks", str(error), parent=self.root)
            return "break"
        self.marks = marks
        first_visible = self.listbox.yview()[0]
        self.listbox.delete(0, "end")
        for i in range(len(self.scenes)):
            self.listbox.insert("end", self.list_label(i))
        self.listbox.selection_set(self.index)
        self.listbox.yview_moveto(first_visible)
        self.update_mark_controls()
        return "break"

    def remove_marked(self):
        try:
            marks = self.load_marks()
        except (OSError, ValueError, TypeError) as error:
            messagebox.showerror("Could not read scene marks", str(error), parent=self.root)
            return
        marked = [(title, path) for title, path in self.scenes if path in marks]
        if not marked:
            return
        names = "\n".join(title for title, _ in marked[:8])
        if len(marked) > 8:
            names += f"\n… and {len(marked) - 8} more"
        alloy_note = "\nThe removed scenes will also leave Alloy’s GLSL picker." if self.folder == DEFAULT_SCENES else ""
        if not messagebox.askyesno(
            "Remove marked scenes",
            f"Move {len(marked)} marked scene(s) into a recovery folder?{alloy_note}\n\n{names}",
            parent=self.root,
        ):
            return
        paths = [path for _, path in marked]
        try:
            archive = archive_scenes(paths, self.folder)
        except (OSError, ValueError, KeyError, TypeError) as error:
            self.reload()
            messagebox.showerror("Could not remove marked scenes", str(error), parent=self.root)
            return
        try:
            write_json(self.mark_file, sorted(str(mark) for mark in self.load_marks().difference(paths)))
        except (OSError, ValueError, TypeError) as error:
            messagebox.showwarning("Could not clear saved marks", str(error), parent=self.root)
        self.reload()
        messagebox.showinfo("Marked scenes removed", f"Moved {len(paths)} scene(s) to:\n{archive}", parent=self.root)

    def choose(self, _event):
        selection = self.listbox.curselection()
        if selection and selection[0] != self.index:
            self.show(selection[0])

    def step(self, delta):
        if self.index is not None:
            self.show((self.index + delta) % len(self.scenes))
        return "break"

    def show(self, index):
        self.index = index
        title, path = self.scenes[index]
        self.listbox.selection_clear(0, "end")
        self.listbox.selection_set(index)
        self.listbox.see(index)
        self.title.configure(text=title)
        self.update_mark_controls()
        try:
            self.engine.load(Scene.open(path))
            self.scene_ready = True
            self.elapsed = 0.
            location = path.relative_to(self.folder) if path.is_relative_to(self.folder) else path.relative_to(ROOT)
            self.status.configure(text=f"{index + 1} of {len(self.scenes)}   ·   {location}")
            self.update_sprite_status()
        except (OSError, ValueError, RuntimeError, KeyError, TypeError) as error:
            self.render_error(error)

    def render_error(self, error):
        self.scene_ready = False
        self.preview.itemconfigure(self.preview_image, image="")
        self.status.configure(text=f"Scene could not render · {str(error).splitlines()[0]}")

    def pick_sprite(self):
        name = filedialog.askopenfilename(parent=self.root, title="Choose a Nested sprite",
                                          filetypes=[("Images", "*.png *.webp *.jpg *.jpeg *.bmp *.tif *.tiff")])
        if name:
            try:
                self.media.open(Path(name))
                self.update_sprite_status()
            except (OSError, ValueError, RuntimeError) as error:
                self.sprite_status.configure(text=str(error))

    def remove_sprite(self):
        self.media.clear()
        self.update_sprite_status()

    def update_sprite_status(self):
        name = self.media.path.name if self.media.path else "No sprite"
        if self.media.path and self.scene_ready and not self.engine.backend.nested_route:
            name += " · scene has no Nested input"
        self.sprite_status.configure(text=name)
        self.clear_sprite.state(["!disabled" if self.media.path else "disabled"])

    def connect_audio(self):
        self.analysis.close()
        self.analysis.live = None
        self.analysis.reset()
        try:
            self.analysis.live = DesktopAudio()
            self.audio_status.configure(text=f"Desktop audio · {self.analysis.live.name}")
        except (ImportError, OSError, RuntimeError, LookupError, ValueError) as error:
            self.audio_status.configure(text=f"Desktop audio unavailable · {error}")

    def tick(self):
        now = time.monotonic()
        dt = min(.1, max(.001, now - self.last_frame))
        self.last_frame = now
        self.elapsed += dt
        audio = self.analysis.frame(now, dt)
        self.audio_level["value"] = audio.uniforms["syn_Level"][0]
        if self.scene_ready:
            try:
                frame = self.engine.render(self.elapsed, dt, audio, self.meta, self.media_values, self.media)
                width, height = self.preview.winfo_width(), self.preview.winfo_height()
                if width > 1 and height > 1:
                    frame.thumbnail((width, height), Image.Resampling.BILINEAR)
                    ratio = min(width / frame.width, height / frame.height)
                    if ratio > 1.01:
                        frame = frame.resize((round(frame.width * ratio), round(frame.height * ratio)),
                                             Image.Resampling.BILINEAR)
                    self.photo = ImageTk.PhotoImage(frame, master=self.root)
                    self.preview.coords(self.preview_image, width // 2, height // 2)
                    self.preview.itemconfigure(self.preview_image, image=self.photo)
            except (OSError, ValueError, RuntimeError) as error:
                self.render_error(error)
        self.frame_id = self.root.after(max(1, round((.04 - (time.monotonic() - now)) * 1000)), self.tick)

    def close(self):
        if self.frame_id is not None:
            self.root.after_cancel(self.frame_id)
        try:
            self.analysis.close()
        finally:
            self.media.clear()
            self.engine.close()
            self.root.destroy()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("scenes", nargs="?", type=Path, default=DEFAULT_SCENES)
    parser.add_argument("--no-audio", action="store_true", help="Open with desktop audio capture off")
    args = parser.parse_args()
    root = tk.Tk()
    SceneViewer(root, args.scenes.resolve(), audio_enabled=not args.no_audio)
    root.mainloop()


if __name__ == "__main__":
    main()
