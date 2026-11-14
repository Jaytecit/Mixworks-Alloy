"""Mixworks Visual Studio, an independent scene player and control surface."""

import argparse
from concurrent.futures import Future
from dataclasses import replace
import json
import math
import os
from pathlib import Path
import random
import sys
from threading import Thread
import time
import tkinter as tk
from tkinter import colorchooser, filedialog, messagebox, ttk
from uuid import uuid4

from PIL import Image, ImageTk

# Alloy's private Python uses an isolated search path.
sys.path.insert(0, str(Path(__file__).resolve().parent))
from audio import Audio
from audio_sources import STUDIO_PORT
from deck import BG, BORDER, CARD, COPPER, LANE_LABELS, MUTED, PANEL, TEXT, TRACKS, LiveDeck
from display import output_bounds
from engine import Engine, MilkDrop, Shader
from media import MEDIA, META, NESTED, Media
from midi import FADERS, Midi
from scene import Scene, load_look, save_look
from timeline import AlloySession, decode, encode, linked_controls, wire_names


ROOT = Path(__file__).resolve().parent
if not (ROOT / "scene-library.json").is_file():
    ROOT = ROOT.parent / "studio"
# The worker's catalogue lives beside studio_engine in Alloy, and in scripts in a source checkout.
sys.path.extend(map(str, (ROOT.parent, ROOT.parent / "scripts")))
from milkdrop_catalogue import catalogue as milkdrop_catalogue

VERSION = "0.1.34"
WELL = "#121619"
FAMILIES = {
    family: {scene["title"]: scene["folder"] for scene in scenes}
    for family, scenes in json.loads((ROOT / "scene-library.json").read_text(encoding="utf-8")).items()
}
BUNDLED_BY_PATH = {
    ROOT / "scenes" / folder / "main.glsl": (family, title)
    for family, scenes in FAMILIES.items()
    for title, folder in scenes.items()
}
STANDALONE_MILKDROP = Path(os.environ["LOCALAPPDATA"]) / "Mixworks" / "MilkDrop-v1" if "LOCALAPPDATA" in os.environ else None


class LiveShuffle:
    def __init__(self):
        self.index_revision, self.presets = None, ()

    def milkdrop_presets(self, library):
        def stamp(path):
            try:
                return path.stat().st_mtime_ns
            except FileNotFoundError:
                return 0

        revision = None if library is None else (
            library, stamp(library), tuple((path, stamp(path)) for path in sorted((library / "linked").glob("*.json"))))
        if revision != self.index_revision:
            entries = milkdrop_catalogue(library)
            self.presets = tuple((Path(entry["preset"]), key.removeprefix("milkdrop/"),
                                  entry["title"].removeprefix("MilkDrop: "), entry["texturePaths"])
                                 for key, entry in entries.items())
            self.index_revision = revision
        return self.presets

    def choose(self, milk, library, current):
        if milk:
            presets = self.milkdrop_presets(library)
            candidates = random.sample(presets, min(8, len(presets)))
            builtin = ROOT / "Prism Current.milk"
            candidates.append((builtin if builtin.is_file() else ROOT / "authoring" / builtin.name,
                               None, None, ()))
        else:
            scenes = [path for path in BUNDLED_BY_PATH if path != current.path]
            candidates = [(path, None, None, ()) for path in random.sample(scenes, min(8, len(scenes)))]
        for path, identity, title, textures in candidates:
            if path == current.path or identity == current.identity:
                continue
            try:
                scene = Scene.open(path)
                if scene.identity != current.identity and (identity is None or scene.identity == identity):
                    return scene, textures, title
            except (OSError, ValueError):
                continue
        if milk:
            raise ValueError("No other playable MilkDrop preset found. Link or rescan a MilkDrop folder in Alloy.")
        raise ValueError("No other bundled scene could open.")


class Studio:
    def __init__(self, root, engine, alloy_port=STUDIO_PORT, link=None):
        self.root, self.engine = root, engine
        self.link, self.timeline_mode = link, False
        self.audio, self.media = Audio(alloy_port), Media()
        self.live_status = None
        self.midi = Midi(ROOT / "alloy-flkey49.json")
        self.midi_device = None
        self.section = tk.StringVar(value="Scene")
        self.values = {name: self.defaults(spec) for name, spec in (("Meta", META), ("Media", MEDIA))}
        self.history = {name: [[], []] for name in ("Scene", "Meta", "Media")}
        self.locks = {name: set() for name in self.history}
        self.last_edit = {}
        self.bank = 0
        self.live_lane = None
        self.wheel_targets = [None, None]
        self.wheel_assignments = {}
        self.running, self.closed = True, False
        self.show_fps = tk.BooleanVar(value=False)
        self.elapsed, self.clock = 0., time.monotonic()
        self.dirty, self.scene_dirty, self.sync = True, True, False
        self.frame, self.photo, self.rect = None, None, (0, 0, 1, 1)
        self.scene_textures, self.indexed_title = (), None
        self.mouse = ((0., 0.), (0., 0.), False)
        self.meters = {}
        self.widgets = []
        self.output = None
        self.shuffle = LiveShuffle()
        self.shuffle_request = self.shuffle_job = None
        self.control_build = None
        self.setup()
        self.initialise_preview()
        if self.audio.mode != "Alloy live":
            self.listen_to_alloy()
        self.root.after(10, self.tick)

    def initialise_preview(self):
        initial = FAMILIES.get("Studio originals", {}).get("Signal Loom")
        if initial is None:
            initial = next((folder for group in FAMILIES.values() for folder in group.values()), None)
        if initial is not None:
            self.open_scene(ROOT / "scenes" / initial)
            return
        fallback = ROOT / "scenes/aurora"
        if fallback.is_dir():
            self.open_scene(fallback)
        else:
            preset = ROOT / "Prism Current.milk"
            self.open_scene(preset if preset.is_file() else ROOT / "authoring" / preset.name)

    def listen_to_alloy(self):
        try:
            self.audio.set_mode("Alloy live")
        except OSError:
            self.audio.set_mode("Muted")
            self.notice("Alloy audio port is in use. Select Alloy live to retry.")
        self.audio_mode.set(self.audio.mode)
        self.publish_audio_port()

    def publish_audio_port(self):
        if self.link:
            self.link.audio_port(self.audio.live.socket.getsockname()[1] if self.audio.live else 0)

    @staticmethod
    def defaults(controls):
        return {name: control.default for name, control in controls.items()}

    def controls(self, section):
        return self.engine.controls if section == "Scene" else META if section == "Meta" else MEDIA

    def settings(self, section):
        return self.engine.values if section == "Scene" else self.values[section]

    def label(self, parent, text="", *, fg=TEXT, bg=PANEL, size=10, weight="normal", **kwargs):
        return tk.Label(parent, text=text, fg=fg, bg=bg, font=("Segoe UI", size, weight), **kwargs)

    def button(self, parent, text, command, *, accent=False, compact=False):
        style = "Accent.TButton" if accent else "Tool.TButton" if compact else "TButton"
        return ttk.Button(parent, text=text, command=command, style=style, width=len(text) + 1 if compact else -9)

    def setup(self):
        root = self.root
        root.title(f"Mixworks Visual Studio  {VERSION}")
        root.geometry("1280x840")
        root.minsize(1080, 710)
        root.configure(bg=BG)
        root.protocol("WM_DELETE_WINDOW", self.close)
        root.bind("<Control-o>", lambda _: self.pick_scene())
        root.bind("<Control-r>", lambda _: self.reload())
        root.bind("<Control-z>", lambda _: self.undo())
        root.bind("<Control-y>", lambda _: self.undo(redo=True))
        root.bind("<Control-s>", lambda _: self.look(save=True))
        root.bind("<F11>", lambda _: self.clean_output())
        root.bind("<Escape>", lambda _: self.return_preview())
        root.bind("<space>", self.space)
        style = ttk.Style(root)
        style.theme_use("clam")
        root.option_add("*TCombobox*Listbox.background", CARD)
        root.option_add("*TCombobox*Listbox.foreground", TEXT)
        root.option_add("*TCombobox*Listbox.selectBackground", COPPER)
        root.option_add("*TCombobox*Listbox.selectForeground", BG)
        style.configure(".", font=("Segoe UI", 10), background=PANEL, foreground=TEXT)
        style.configure("TButton", padding=(10, 6), background=CARD, borderwidth=0)
        style.map("TButton", background=[("active", BORDER), ("disabled", PANEL)])
        style.configure("Tool.TButton", padding=(3, 4), background=WELL)
        style.configure("Accent.TButton", foreground=BG, background=COPPER)
        style.map("Accent.TButton", background=[("active", "#edbd98")])
        style.configure("TCheckbutton", background=CARD, foreground=TEXT)
        style.map("TCheckbutton", background=[("active", CARD)])
        style.configure("TCombobox", fieldbackground=WELL, selectbackground=WELL, selectforeground=TEXT,
                        borderwidth=0, padding=4, bordercolor=BORDER, arrowcolor=MUTED)
        style.map("TCombobox", fieldbackground=[("readonly", WELL)], foreground=[("readonly", TEXT)])
        style.configure("TSpinbox", fieldbackground=WELL, foreground=TEXT, bordercolor=BORDER,
                        arrowcolor=MUTED, selectbackground=CARD, selectforeground=TEXT)
        style.configure("Horizontal.TScale", background=COPPER, troughcolor=WELL,
                        bordercolor=WELL, lightcolor=COPPER, darkcolor=COPPER, sliderlength=15)
        style.configure("Vertical.TScrollbar", background=BORDER, troughcolor=PANEL,
                        bordercolor=PANEL, lightcolor=PANEL, darkcolor=PANEL, arrowcolor=MUTED,
                        arrowsize=11)

        header = tk.Frame(root, bg=BG)
        header.pack(fill="x", padx=20, pady=(14, 13))
        logo = tk.Frame(header, bg=BG)
        logo.pack(side="left")
        self.label(logo, "MIXWORKS / VISUAL STUDIO", fg=COPPER, bg=BG,
                   size=11, weight="bold").pack(anchor="w")
        self.subtitle = self.label(logo, f"Standalone scene player   ·   {VERSION}", bg=BG, fg=MUTED, size=9)
        self.subtitle.pack(anchor="w")
        self.midi_input = self.button(header, "MIDI Input Off", self.toggle_midi_input)
        self.midi_input.configure(width=14)
        self.midi_input.pack(side="right", padx=(7, 0))
        self.scene_buttons = []
        for text, command in reversed((("Open scene…", self.pick_scene), ("Reload", self.reload),
                                       ("MIDI", self.midi_dialog))):
            button = self.button(header, text, command, accent=text == "Open scene…")
            button.pack(side="right", padx=(7, 0))
            if text != "MIDI":
                self.scene_buttons.append(button)
        self.scene_picker = ttk.Combobox(header, width=22, state="readonly")
        self.scene_picker.set("Studio scenes")
        self.scene_picker.pack(side="right", padx=5)
        self.scene_picker.bind("<<ComboboxSelected>>", self.pick_bundled_scene)
        self.family_picker = ttk.Combobox(header, values=tuple(FAMILIES), width=18, state="readonly")
        self.family_picker.set("Scene family")
        self.family_picker.pack(side="right", padx=5)
        self.family_picker.bind("<<ComboboxSelected>>", self.pick_family)

        body = tk.Frame(root, bg=BG)
        body.pack(fill="both", expand=True, padx=20)
        stage = tk.Frame(body, bg=PANEL)
        stage.pack(side="left", fill="both", expand=True, padx=(0, 12))
        side = tk.Frame(body, width=385, bg=PANEL)
        side.pack(side="right", fill="y")
        side.pack_propagate(False)

        scene_bar = tk.Frame(stage, bg=PANEL)
        scene_bar.pack(fill="x", padx=14, pady=10)
        self.scene_title = self.label(scene_bar, "Preview", size=18, weight="bold")
        self.engine_label = self.label(scene_bar, fg=COPPER, size=9)
        self.engine_label.pack(side="right")
        ttk.Checkbutton(scene_bar, text="FPS counter", variable=self.show_fps,
                        command=self.toggle_fps).pack(side="right", padx=(0, 12))
        self.scene_title.pack(side="left")
        self.live_tools = tk.Frame(stage, bg=PANEL)
        self.live_tools.pack(fill="x", padx=12, pady=(0, 7))
        self.shuffle_buttons = []
        for text, milk in (("Random scene", False), ("Random MilkDrop", True)):
            button = self.button(self.live_tools, text, lambda milk=milk: self.request_shuffle(milk))
            button.pack(side="left", padx=(0, 6))
            self.shuffle_buttons.append(button)
        self.swap_tools = tk.Frame(stage, bg=PANEL)
        self.swap_buttons = []
        if self.link:
            for layer, title in enumerate(LANE_LABELS):
                button = self.button(self.swap_tools, f"Swap {title}", lambda target=layer: self.swap_lane(target), compact=True)
                button.pack(side="left", padx=(0, 5))
                self.swap_buttons.append(button)
            self.swap_tools.pack(fill="x", padx=12, pady=(0, 7))
        self.preview = tk.Canvas(stage, bg=WELL, bd=0, highlightthickness=0)
        self.preview.pack(fill="both", expand=True, padx=12, pady=(0, 10))
        self.preview.bind("<Configure>", lambda _: self.present())
        self.preview.bind("<Motion>", lambda event: self.pointer(event))
        self.preview.bind("<ButtonPress-1>", lambda event: self.pointer(event, True))
        self.preview.bind("<ButtonRelease-1>", lambda event: self.pointer(event, False))

        transport = tk.Frame(stage, bg=PANEL)
        transport.pack(fill="x", padx=12, pady=(0, 8))
        self.play_button = self.button(transport, "Pause", self.toggle, accent=True)
        self.play_button.pack(side="left")
        self.button(transport, "Restart", self.restart).pack(side="left", padx=6)
        if self.link:
            self.button(transport, "Match Mix", lambda: self.link.send(command="arrange")).pack(side="left")
        self.output_button = self.button(transport, "Fullscreen output", self.clean_output)
        self.output_button.pack(side="right")
        self.button(transport, "Save frame…", self.capture).pack(side="right")
        self.timer = self.label(transport, "00:00", fg=MUTED)
        self.timer.pack(side="right", padx=9)

        sound = tk.Frame(stage, bg=PANEL)
        sound.pack(fill="x", padx=12, pady=(0, 12))
        self.audio_mode = ttk.Combobox(sound, values=("Demo pulse", "Muted", "Audio file", "Alloy live"),
                                       width=12, state="readonly")
        self.audio_mode.set(self.audio.mode)
        self.audio_mode.pack(side="left")
        self.audio_mode.bind("<<ComboboxSelected>>", lambda _: self.change_audio())
        self.audio_file_button = self.button(sound, "Open audio…", self.pick_audio)
        self.audio_file_button.pack(side="left", padx=6)
        self.label(sound, "BPM", fg=MUTED, size=9).pack(side="left", padx=(5, 2))
        bpm = ttk.Spinbox(sound, from_=40, to=240, increment=1, width=5)
        self.bpm_box = bpm
        bpm.set("124")
        bpm.pack(side="left")
        bpm.configure(command=lambda: self.set_bpm(bpm))
        bpm.bind("<Return>", lambda _: self.set_bpm(bpm))
        self.live_indicator = self.label(sound, fg=TRACKS[2], size=8, weight="bold")
        self.live_indicator.pack(side="left", padx=8)
        for name, title in reversed((("Level", "LEVEL"), ("BassLevel", "BASS"), ("HighLevel", "HIGH"))):
            box = tk.Frame(sound, bg=PANEL)
            box.pack(side="right", padx=5)
            self.label(box, title, fg=MUTED, size=8).pack()
            self.meters[name] = tk.Canvas(box, width=42, height=4, bg=WELL, highlightthickness=0)
            self.meters[name].pack(pady=(2, 0))

        tabs = tk.Frame(side, bg=PANEL)
        tabs.pack(fill="x", padx=9, pady=9)
        self.tabs = {}
        for name in self.history:
            self.tabs[name] = self.button(tabs, name, lambda section=name: self.select(section))
            self.tabs[name].pack(side="left", fill="x", expand=True, padx=3)
        self.actions = tk.Frame(side, bg=PANEL)
        self.actions.pack(fill="x", padx=12)
        self.media_actions = tk.Frame(side, bg=PANEL)
        self.scroll = tk.Canvas(side, bg=PANEL, highlightthickness=0, bd=0)
        scrollbar = ttk.Scrollbar(side, orient="vertical", command=self.scroll.yview)
        scrollbar.pack(side="right", fill="y", padx=(0, 4))
        self.scroll.pack(fill="both", expand=True, padx=(12, 4), pady=(9, 3))
        self.scroll.configure(yscrollcommand=scrollbar.set)
        self.cards = tk.Frame(self.scroll, bg=PANEL)
        self.scroll_id = self.scroll.create_window((0, 0), window=self.cards, anchor="nw")
        self.scroll.bind("<Configure>", lambda e: self.scroll.itemconfigure(self.scroll_id, width=e.width))
        self.cards.bind("<Configure>", lambda _: self.scroll.configure(scrollregion=self.scroll.bbox("all")))
        root.bind_all("<MouseWheel>", self.wheel_scroll, add="+")

        self.deck = LiveDeck(root, self)
        self.deck.pack(fill="x", padx=20, pady=(9, 0))
        footer = tk.Frame(root, bg=BG)
        footer.pack(fill="x", padx=20, pady=7)
        self.status = self.label(footer, fg=MUTED, bg=BG, size=9, anchor="w")
        self.status.pack(side="left", fill="x", expand=True)
        self.midi_status = self.label(footer, "No MIDI device selected", fg=COPPER, bg=BG, size=9)
        self.midi_status.pack(side="right", padx=(8, 0))

    def notice(self, text):
        self.status.configure(text=text)

    def error(self, title, exception):
        messagebox.showerror(title, str(exception)[:3200], parent=self.root)

    def pick_scene(self):
        name = filedialog.askopenfilename(parent=self.root, title="Open a scene or MilkDrop preset",
                                          filetypes=[("Scene and preset files", "*.glsl *.frag *.fs *.milk"),
                                                     ("All files", "*.*")])
        if name:
            self.open_scene(Path(name))

    def reload(self):
        self.open_scene(self.engine.scene.path, texture_paths=self.scene_textures, title=self.indexed_title)

    def pick_family(self, _event):
        scenes = FAMILIES[self.family_picker.get()]
        self.open_scene(ROOT / "scenes" / next(iter(scenes.values())))

    def pick_bundled_scene(self, _event):
        scenes = FAMILIES[self.family_picker.get()]
        self.open_scene(ROOT / "scenes" / scenes[self.scene_picker.get()])

    def sync_scene_pickers(self, path):
        family, title = BUNDLED_BY_PATH.get(path, ("Scene family", "Studio scenes"))
        self.family_picker.set(family)
        self.scene_picker.configure(values=tuple(FAMILIES.get(family, ())))
        self.scene_picker.set(title)

    def open_scene(self, path, *, texture_paths=(), title=None):
        self.shuffle_request = None
        try:
            scene = Scene.open(path)
            self.load_scene(scene, texture_paths=texture_paths, title=title)
        except Exception as error:
            if self.engine.scene is None:
                raise
            self.sync_scene_pickers(self.engine.scene.path)
            self.error("Scene could not open", error)
            return False
        return True

    def load_scene(self, scene, *, texture_paths=(), title=None):
        if title:
            scene = replace(scene, title=title)
        self.engine.load(scene, texture_paths=texture_paths)
        self.scene_textures, self.indexed_title = texture_paths, title
        self.sync_scene_pickers(scene.path)
        self.history["Scene"] = [[], []]
        self.locks["Scene"].clear()
        self.last_edit.pop("Scene", None)
        self.scene_title.configure(text=scene.title[:54])
        milk = not isinstance(self.engine.backend, Shader)
        self.engine_label.configure(text="MILKDROP  /  PROJECTM" if milk else "GLSL  /  OPENGL")
        self.elapsed, self.clock = 0., time.monotonic()
        self.scene_dirty = self.dirty = True
        self.select_lane(self.live_lane)
        self.notice(f"{scene.path.name}  ·  {len(self.engine.controls)} scene controls")
        self.midi_label()

    def milkdrop_library(self):
        return STANDALONE_MILKDROP

    def request_shuffle(self, milk):
        if self.timeline_mode or self.engine is None:
            return
        self.shuffle_request = milk, self.milkdrop_library(), self.engine.scene
        self.notice("Preparing a random MilkDrop preset…" if milk else "Preparing a random scene…")

    def poll_shuffle(self):
        if self.shuffle_job is not None:
            request, job = self.shuffle_job
            if not job.done():
                return
            self.shuffle_job = None
            if request is self.shuffle_request:
                self.shuffle_request = None
                try:
                    scene, textures, title = job.result()
                    self.load_scene(scene, texture_paths=textures, title=title)
                except Exception as error:
                    self.notice(f"Random visual could not open · {error}")
                return
        if self.shuffle_request is not None:
            request, job = self.shuffle_request, Future()
            self.shuffle_job = request, job

            def prepare():
                try:
                    job.set_result(self.shuffle.choose(*request))
                except Exception as error:
                    job.set_exception(error)

            Thread(target=prepare, name="Studio live shuffle", daemon=True).start()

    def select(self, section):
        self.section.set(section)
        self.build_controls()
        self.scroll.yview_moveto(0)

    def wheel_scroll(self, event):
        under = event.widget
        while under:
            if under == self.scroll:
                self.scroll.yview_scroll(-int(event.delta / 120), "units")
                return
            under = getattr(under, "master", None)

    def build_controls(self):
        for _ in self.build_control_rows():
            pass

    def build_control_rows(self):
        section = self.section.get()
        for name, button in self.tabs.items():
            button.configure(style="Accent.TButton" if name == section else "TButton")
        self.widgets.clear()
        for panel in (self.cards, self.actions, self.media_actions):
            for child in panel.winfo_children():
                child.destroy()
                yield True
        self.media_actions.pack_forget()
        self.scrub = None
        self.scrubbing = False
        locked = "*" in self.locks[section]
        for text, command in ((("Unlock" if locked else "Lock"), lambda: self.lock(section, "*")),
                              ("Undo", self.undo), ("Reset" if self.timeline_mode else "Default", lambda: self.change_group(section)),
                              ("Random", lambda: self.change_group(section, randomize=True))):
            self.button(self.actions, text, command, compact=True).pack(side="left", padx=(0, 4))
        if section == "Scene" and not self.timeline_mode:
            for text, save in (("Load", False), ("Save", True)):
                self.button(self.actions, text, lambda flag=save: self.look(flag), compact=True).pack(side="right", padx=(4, 0))
        if section == "Media" and not self.timeline_mode:
            self.media_actions.pack(fill="x", padx=12, pady=(8, 0), before=self.scroll)
            self.build_media()
        groups = {}
        for control in self.controls(section).values():
            groups.setdefault(control.group, []).append(control)
        if not groups:
            empty = "No clip in this lane." if self.timeline_mode and not self.selected[0] else "No controls in this tab."
            self.label(self.cards, empty, fg=MUTED).pack(pady=20)
        for index, (group, controls) in enumerate(groups.items()):
            card = tk.Frame(self.cards, bg=CARD)
            card.pack(fill="x", pady=(0, 8))
            row = tk.Frame(card, bg=CARD)
            row.pack(fill="x", padx=10, pady=(6, 1))
            self.label(row, group.upper(), fg=TRACKS[index % len(TRACKS)], bg=CARD,
                       size=9, weight="bold").pack(side="left")
            for text, action in reversed((("Unlock" if group in self.locks[section] else "Lock",
                                           lambda g=group: self.lock(section, g)),
                                          ("Reset", lambda g=group: self.change_group(section, g)),
                                          ("Random", lambda g=group: self.change_group(section, g, True)))):
                button = self.button(row, text, action, compact=True)
                button.pack(side="right", padx=(2, 0))
            for control in controls:
                self.control_widget(card, section, control)
                yield True
            if section == "Media" and group == "Sprite" and not self.timeline_mode:
                hint = self.label(card, bg=CARD, fg=MUTED, size=9, anchor="w", justify="left", wraplength=315)
                hint.pack(fill="x", padx=10, pady=(1, 6), anchor="w")
                self.widgets.append(lambda label=hint: label.configure(text=self.sprite_hint()))
                self.widgets[-1]()
            if section == "Media" and group == "Nested sprite":
                self.label(card, "Applies when the sprite is routed into MilkDrop or GLSL.", fg=MUTED, bg=CARD,
                           size=9, anchor="w").pack(fill="x", padx=10, pady=(1, 6))
        self.deck.refresh()

    def control_widget(self, card, section, ctrl):
        name = ctrl.name
        row = tk.Frame(card, bg=CARD)
        row.pack(fill="x", padx=10, pady=(1, 4))
        enabled = not self.locked(section, ctrl)

        def get():
            return self.settings(section)[name]

        if ctrl.kind == "bang":
            self.button(row, ctrl.label, lambda: self.set_control(section, name, None)).pack(side="left")
            return
        if ctrl.kind == "toggle":
            var = tk.BooleanVar()
            widget = ttk.Checkbutton(row, text=ctrl.label, variable=var,
                                     command=lambda: self.set_control(section, name, ctrl.high if var.get() else ctrl.low))
            widget.pack(side="left")
            if not enabled:
                widget.state(["disabled"])
            self.widgets.append(lambda: var.set(get() == ctrl.high))
            self.widgets[-1]()
            return
        if ctrl.kind == "dropdown":
            self.label(row, ctrl.label, bg=CARD).pack(side="left")
            box = ttk.Combobox(row, values=ctrl.labels, width=15, state="readonly" if enabled else "disabled")
            box.pack(side="right")
            box.bind("<<ComboboxSelected>>", lambda _: self.set_control(section, name, (ctrl.choices[box.current()],)))
            def sync_choice():
                box.set(ctrl.labels[ctrl.choices.index(get()[0])])
                if section == "Media" and name == "overlayMode":
                    nested = (self.state.get("spriteRouting", {}).get("route") in ("milk", "shader")
                              if self.timeline_mode else self.settings(section)["spriteRoute"][0])
                    active = enabled and not nested
                    box.configure(state="readonly" if active else "disabled")

            self.widgets.append(sync_choice)
            self.widgets[-1]()
            return
        if ctrl.kind == "color":
            self.label(row, ctrl.label, bg=CARD).pack(side="left")
            swatch = tk.Button(row, width=6, bd=0, cursor="hand2")

            def choose():
                result, _ = colorchooser.askcolor(color=hex_color(), parent=self.root)
                if result:
                    self.set_control(section, name, tuple(channel / 255 for channel in result))

            def hex_color():
                return "#%02x%02x%02x" % tuple(round(x * 255) for x in get())

            swatch.configure(command=choose, state="normal" if enabled else "disabled")
            swatch.pack(side="right", pady=3)
            self.widgets.append(lambda: swatch.configure(bg=hex_color(), activebackground=hex_color()))
            self.widgets[-1]()
            return

        size = len(ctrl.default)
        self.label(row, ctrl.label, bg=CARD, weight="bold" if size > 1 else "normal").pack(anchor="w")
        first_widget = len(self.widgets)
        if size == 2:
            xy = tk.Canvas(row, height=75, bg=WELL, highlightthickness=0)
            xy.pack(fill="x", pady=(3, 2))

            def pad_draw():
                width, height = max(1, xy.winfo_width()), max(1, xy.winfo_height())
                frac = [(v - a) / (b - a) if a != b else .5 for v, a, b in zip(get(), ctrl.low, ctrl.high)]
                x, y = 6 + frac[0] * (width - 12), 6 + (1. - frac[1]) * (height - 12)
                xy.delete("all")
                xy.create_line(width / 2, 0, width / 2, height, fill=BORDER)
                xy.create_line(0, height / 2, width, height / 2, fill=BORDER)
                xy.create_oval(x - 5, y - 5, x + 5, y + 5, fill=COPPER, outline=TEXT)

            def pad_move(event):
                coords = ((event.x - 6) / max(1, xy.winfo_width() - 12),
                          1 - (event.y - 6) / max(1, xy.winfo_height() - 12))
                self.set_control(section, name, tuple(a + (b - a) * min(1., max(0., v))
                                                       for a, b, v in zip(ctrl.low, ctrl.high, coords)))

            xy.bind("<Configure>", lambda _: pad_draw())
            if enabled:
                xy.bind("<Button-1>", pad_move)
                xy.bind("<B1-Motion>", pad_move)
            self.widgets.append(pad_draw)
        axes = ("X", "Y") if size == 2 else ("R", "G", "B", "A") if size > 1 else ("",)
        for index, axis in enumerate(axes[:size]):
            line = tk.Frame(row, bg=CARD)
            line.pack(fill="x")
            if axis:
                self.label(line, axis, fg=MUTED, bg=CARD, width=2, size=9).pack(side="left")
            field = tk.Entry(line, width=8, bd=0, bg=WELL, fg=TEXT, insertbackground=TEXT, justify="right")
            field.pack(side="right", padx=(8, 0), ipady=2)
            scale = ttk.Scale(line, from_=ctrl.low[index], to=ctrl.high[index],
                              command=lambda value, i=index: self.slider_value(section, ctrl, i, value))
            scale.pack(fill="x", expand=True)
            if not enabled or ctrl.low[index] == ctrl.high[index]:
                scale.state(["disabled"])
                field.configure(state="readonly", readonlybackground=WELL)
            field.bind("<Return>", lambda event, i=index: self.entry_value(section, ctrl, i, event.widget))

            def sync_line(widget=scale, entry=field, i=index):
                value = get()[i]
                widget.set(value)
                if self.timeline_mode and entry.focus_get() == entry:
                    return
                if entry.cget("state") == "readonly":
                    entry.configure(state="normal")
                    restore = True
                else:
                    restore = False
                entry.delete(0, "end")
                entry.insert(0, f"{value:.3g}" if abs(value) >= 100 else f"{value:.2f}")
                if restore:
                    entry.configure(state="readonly")

            self.widgets.append(sync_line)
        if section == self.pot_section() and size == 2:
            paired = self.wheel_targets == [(name, 0), (name, 1)]
            self.button(row, "Wheels assigned · X / Y" if paired else "Assign both wheels",
                        lambda: self.assign_wheels(name)).pack(anchor="e", pady=(4, 0))
        self.sync_widgets(first_widget)

    def sync_widgets(self, start=0):
        self.sync = True
        try:
            for callback in self.widgets[start:]:
                callback()
        finally:
            self.sync = False

    def slider_value(self, section, control, index, raw):
        if not self.sync:
            value = list(self.settings(section)[control.name])
            scalar = float(raw)
            value[index] = float(round(scalar)) if control.kind == "integer" else scalar
            self.set_control(section, control.name, value)

    def entry_value(self, section, control, index, field):
        try:
            self.slider_value(section, control, index, float(field.get()))
        except ValueError as error:
            self.notice(str(error))
            self.sync_widgets()
        return "break"

    def locked(self, section, control):
        return "*" in self.locks[section] or control.group in self.locks[section]

    def lock(self, section, group):
        if group in self.locks[section]:
            self.locks[section].remove(group)
        else:
            self.locks[section].add(group)
        self.build_controls()

    def remember(self, section, token=None):
        now = time.monotonic()
        recent, last = self.last_edit.get(section, (None, 0))
        back, forward = self.history[section]
        if token is None or token != recent or now - last > .45:
            back.append(dict(self.settings(section)))
            del back[:-40]
        forward.clear()
        self.last_edit[section] = token, now

    def changed(self, section, sync=True):
        self.dirty = True
        self.scene_dirty |= section == "Scene" or (
            section == "Media" and bool(self.values["Media"]["spriteRoute"][0] and self.engine.backend.nested_route))
        if sync and self.section.get() == section:
            self.sync_widgets()
        self.deck.refresh()

    def set_control(self, section, name, values):
        ctrl = self.controls(section)[name]
        if self.locked(section, ctrl):
            return
        if ctrl.kind == "bang":
            self.engine.backend.events.add(name)
        else:
            values = ctrl.validate(values)
            if self.settings(section)[name] == values:
                return
            self.remember(section, name)
            self.settings(section)[name] = values
        self.changed(section)

    def change_group(self, section, group=None, randomize=False):
        controls = [c for c in self.controls(section).values()
                    if c.kind != "bang" and (group is None or c.group == group) and not self.locked(section, c)]
        if not controls:
            return
        self.remember(section)
        for ctrl in controls:
            self.settings(section)[ctrl.name] = self.group_value(ctrl, randomize)
        self.changed(section)

    @staticmethod
    def group_value(control, randomize):
        if not randomize:
            return control.default
        if control.kind == "dropdown":
            return (random.choice(control.choices),)
        if control.kind == "toggle":
            return random.choice((control.low, control.high))
        if control.kind == "integer":
            return (float(random.randint(math.ceil(control.low[0]), math.floor(control.high[0]))),)
        return tuple(random.uniform(low, high) for low, high in zip(control.low, control.high))

    def undo(self, redo=False):
        section = self.section.get()
        back, forward = self.history[section]
        source, target = (forward, back) if redo else (back, forward)
        if source and "*" not in self.locks[section]:
            target.append(dict(self.settings(section)))
            self.settings(section).update({name: value for name, value in source.pop().items()
                                           if not self.locked(section, self.controls(section)[name])})
            self.last_edit.pop(section, None)
            self.changed(section)

    def look(self, save=False):
        if save:
            name = filedialog.asksaveasfilename(parent=self.root, title="Save scene look", defaultextension=".look.json",
                                                initialfile=f"{self.engine.scene.path.stem}.look.json",
                                                filetypes=[("Scene look", "*.look.json")])
        else:
            name = filedialog.askopenfilename(parent=self.root, title="Load scene look",
                                              filetypes=[("Scene look", "*.look.json"), ("JSON", "*.json")])
        if name:
            try:
                if save:
                    save_look(Path(name), self.engine.scene, self.engine.controls, self.engine.values)
                else:
                    values = load_look(Path(name), self.engine.scene, self.engine.controls)
                    self.remember("Scene")
                    self.engine.values.update({key: value for key, value in values.items()
                                               if not self.locked("Scene", self.engine.controls[key])})
                    self.changed("Scene")
                self.notice(f"{'Saved' if save else 'Loaded'} {Path(name).name}")
            except Exception as error:
                self.error("Scene look", error)

    def sprite_hint(self):
        if not self.values["Media"]["spriteRoute"][0]:
            return "Overlay composites the sprite over the finished scene."
        if self.engine.backend.nested_route == "media":
            return "Nested lets this scene use the sprite as a material or simulation input."
        if self.engine.backend.nested_route == "feedback":
            return "Nested feeds the sprite into the scene’s feedback."
        if isinstance(self.engine.backend, Shader):
            return "This shader needs a media or feedback sampler for Nested."
        return "Nested needs Studio’s patched projectM runtime."

    def build_media(self):
        panel = self.media_actions
        top = tk.Frame(panel, bg=PANEL)
        top.pack(fill="x")
        self.button(top, "Open media…", self.pick_media).pack(side="left")
        self.button(top, "Clear", self.clear_media).pack(side="right")
        title = self.media.path.name if self.media.path else "No image or video selected"
        self.label(panel, title[:48], fg=MUTED, size=9, anchor="w").pack(fill="x", pady=4)
        if self.media.capture:
            transport = tk.Frame(panel, bg=PANEL)
            transport.pack(fill="x")
            self.media_play = self.button(transport, "Pause" if self.media.playing else "Play", self.toggle_media)
            self.media_play.pack(side="left")
            for label, action in (("Stop", self.stop_media),
                                  ("◀", lambda: self.seek_media(self.media.position - 1 / self.media.fps)),
                                  ("▶", lambda: self.seek_media(self.media.position + 1 / self.media.fps))):
                self.button(transport, label, action, compact=True).pack(side="left", padx=3)
            loop = tk.BooleanVar(value=self.media.loop)
            ttk.Checkbutton(transport, text="Loop", variable=loop,
                            command=lambda: setattr(self.media, "loop", loop.get())).pack(side="right")
            self.scrub = ttk.Scale(panel, from_=0, to=self.media.duration)
            self.scrub.set(self.media.position)
            self.scrub.pack(fill="x", pady=(4, 0))
            self.scrub.bind("<ButtonPress-1>", lambda _: setattr(self, "scrubbing", True))
            self.scrub.bind("<ButtonRelease-1>", lambda _: self.finish_scrub())

    def finish_scrub(self):
        self.scrubbing = False
        self.seek_media(self.scrub.get())

    def pick_media(self):
        name = filedialog.askopenfilename(parent=self.root, title="Open an image or video",
                                          filetypes=[("Media", "*.png *.jpg *.jpeg *.webp *.bmp *.mp4 *.mov *.mkv *.webm *.avi *.m4v"),
                                                     ("All files", "*.*")])
        if name:
            try:
                self.media.open(Path(name))
                self.build_controls()
                self.changed("Media")
            except Exception as error:
                self.error("Media could not open", error)

    def clear_media(self):
        self.media.clear()
        self.build_controls()
        self.changed("Media")

    def seek_media(self, position):
        try:
            self.media.seek(position)
            self.changed("Media", sync=False)
        except ValueError as error:
            self.notice(str(error))

    def toggle_media(self):
        try:
            self.media.toggle()
        except ValueError as error:
            self.notice(str(error))
        self.media_play.configure(text="Pause" if self.media.playing else "Play")

    def stop_media(self):
        self.media.playing = False
        self.seek_media(0.)
        self.media_play.configure(text="Play")

    def set_bpm(self, widget):
        if self.audio.live:
            return
        try:
            self.audio.bpm = min(240., max(40., float(widget.get())))
        except ValueError:
            pass
        widget.set(str(round(self.audio.bpm)))

    def pick_audio(self):
        name = filedialog.askopenfilename(parent=self.root, title="Open audio for playback",
                                          filetypes=[("Audio", "*.wav *.flac *.ogg *.mp3 *.aiff"), ("All files", "*.*")])
        if name:
            try:
                self.audio.open(Path(name))
                self.publish_audio_port()
                self.set_running(False)
                self.restart()
                self.notice(f"{Path(name).name}  ·  Press Play to start audio")
            except Exception as error:
                self.error("Audio could not open", error)
        self.audio_mode.set(self.audio.mode)

    def change_audio(self):
        mode = self.audio_mode.get()
        if mode == "Audio file" and self.audio.track is None:
            self.pick_audio()
            return
        try:
            self.audio.set_mode(mode)
        except OSError as error:
            self.audio_mode.set(self.audio.mode)
            self.error("Alloy live audio", f"Could not open the local Alloy audio feed. "
                       f"Close any other Studio window using Alloy live and retry.\n\n{error}")
            return
        self.publish_audio_port()
        if mode == "Audio file":
            self.set_running(False)
        self.restart()
        if mode == "Alloy live":
            self.set_running(True)
            self.live_status = None
        else:
            self.notice(f"{mode} selected")

    def update_live_audio(self):
        self.audio.poll_live()
        live = self.audio.live
        if live is None:
            if self.live_status is not None:
                self.live_indicator.configure(text="")
                self.bpm_box.state(["!readonly"])
                self.bpm_box.set(str(round(self.audio.bpm)))
                self.live_status = None
            return
        rate = live.rate if live.active else 0
        detected, mix, _ = live.clock()
        current = rate, round(detected, 2), round(mix, 1)
        if current != self.live_status:
            self.live_status = current
            self.live_indicator.configure(text="LIVE" if rate else "WAITING", fg=TRACKS[2] if rate else MUTED)
            self.bpm_box.state(["readonly"])
            self.bpm_box.set(f"{detected or mix:g}" if detected or mix else "—")
            if rate:
                tempo = (f"  ·  Track {detected:g} BPM" if detected else "") + (f"  ·  Mix {mix:.1f} BPM" if mix else "")
                self.notice(f"Receiving Alloy master audio · {rate / 1000:g} kHz{tempo}")
            else:
                self.notice("Waiting for Alloy · View → Share master audio with Visual Studio")

    def set_running(self, running):
        if running == self.running:
            return
        try:
            if running:
                self.audio.play()
            else:
                self.audio.stop()
            self.running = running
            self.clock = time.monotonic()
            self.play_button.configure(text="Pause" if running else "Play")
            self.deck.refresh()
        except Exception as error:
            self.error("Audio output", error)

    def toggle(self):
        self.set_running(not self.running)

    def space(self, event):
        if not isinstance(event.widget, (tk.Button, tk.Entry, ttk.Entry, ttk.Combobox, ttk.Button, ttk.Checkbutton)):
            self.toggle()
            return "break"

    def restart(self):
        try:
            self.audio.stop(rewind=True)
            if self.running:
                self.audio.play()
            self.engine.backend.reset()
            self.elapsed, self.clock = 0., time.monotonic()
            self.scene_dirty = self.dirty = True
        except Exception as error:
            self.error("Restart failed", error)

    def assign_wheels(self, name):
        self.wheel_targets = [(name, 0), (name, 1)]
        self.wheels_changed()
        self.notice(f"Pitch → {self.controls(self.pot_section())[name].label} X   ·   Mod → Y")

    def assign_wheel(self, slot, target):
        self.wheel_targets[slot] = target
        self.wheels_changed()

    def wheels_changed(self):
        self.wheel_assignments[self.wheel_key()] = tuple(self.wheel_targets)
        self.build_controls()
        self.midi_label()

    def wheel_key(self):
        return self.engine.scene.identity, self.pot_section()

    def midi_label(self):
        active = bool(self.midi.name)
        label = (f"{self.midi.name}  ·  Bank {self.bank + 1}" if active
                 else self.midi_device[1] if self.midi_device else "No MIDI device selected")
        self.midi_status.configure(text=label)
        self.midi_input.configure(text=f"MIDI Input {'On' if active else 'Off'}",
                                  style="Accent.TButton" if active else "TButton")

    def toggle_midi_input(self):
        if self.midi.name:
            self.midi.close()
            self.midi_label()
        elif self.midi_device and self.midi.ports().get(self.midi_device[0]) == self.midi_device[1]:
            try:
                self.midi.open(*self.midi_device)
            except OSError as error:
                self.error("Could not enable MIDI input", error)
            self.midi_label()
        else:
            self.midi_dialog()

    def midi_dialog(self):
        popup = tk.Toplevel(self.root, bg=PANEL)
        popup.title("MIDI input · Alloy FLkey 49")
        popup.transient(self.root)
        popup.resizable(False, False)
        content = tk.Frame(popup, bg=PANEL, padx=16, pady=14)
        content.pack(fill="both")
        self.label(content, "FLkey 49  /  Alloy Custom Modes", fg=COPPER, weight="bold").pack(anchor="w")
        self.label(content, "Pots adjust scene controls. Top pads select banks.\nFaders adjust Meta and Media. Fader 9 sets Master.\nPitch and modulation follow the assignments in the controller.",
                   fg=MUTED, justify="left").pack(anchor="w", pady=(5, 9))
        ports = self.midi.ports()
        names = {f"{name}  [{index + 1}]": (index, name) for index, name in ports.items()}
        combo = ttk.Combobox(content, values=tuple(names), state="readonly", width=35)
        combo.pack(fill="x")
        primary = next((label for label in names if "flkey" in label.lower()
                        and "daw" not in label.lower() and "midiin2" not in label.lower()), None)
        selected = next((label for label, device in names.items() if device == self.midi_device), None)
        combo.set(selected or primary or next(iter(names), "No MIDI inputs found"))

        def connect():
            if combo.get() not in names:
                return
            try:
                device = names[combo.get()]
                self.midi.open(*device)
                self.midi_device = device
                self.midi_label()
                popup.destroy()
            except OSError as error:
                self.midi_label()
                self.error("Could not connect MIDI", error)

        def disconnect():
            self.midi.close()
            self.midi_device = None
            self.midi_label()
            popup.destroy()

        buttons = tk.Frame(content, bg=PANEL)
        buttons.pack(fill="x", pady=(9, 0))
        self.button(buttons, "Connect", connect, accent=True).pack(side="left")
        self.button(buttons, "Disconnect", disconnect).pack(side="left", padx=6)

    def midi_event(self, action, position):
        if action in ("wheel-x", "wheel-y"):
            target = self.wheel_targets[int(action == "wheel-y")]
            if target:
                name, axis = target
                section = self.pot_section()
                self.set_position(section, self.controls(section)[name], axis, position)
        elif action == "play":
            self.set_running(True)
        elif action == "stop":
            self.set_running(False)
            self.restart()
        elif action == "home":
            self.restart()
        elif action in ("undo", "redo"):
            self.undo(redo=action == "redo")
        elif action == "snap":
            self.change_group(self.pot_section(), randomize=True)
        elif action == "record-take":
            self.capture()
        elif action.startswith("desk-lane-"):
            self.select_bank(int(action.rsplit("-", 1)[1]) - 1)
        elif action.startswith("desk-pot-"):
            index = int(action.rsplit("-", 1)[1]) - 1
            targets = self.bank_pots()
            if 0 <= index < len(targets):
                section, ctrl, axis = targets[index]
                self.set_position(section, ctrl, axis, position)
        elif action.startswith(("mute-", "solo-")):
            index = int(action.rsplit("-", 1)[1]) - 1 + (4 if action.startswith("solo-") else 0)
            self.press_pad(index)
        else:
            for prefix, offset, reset in (("volume-", 0, False), ("pan-", 4, False), ("master", 8, False),
                                           ("reset-eq-", 0, True), ("reset-filter-", 4, True), ("reset-pans", 8, True)):
                if action == prefix or prefix.endswith("-") and action.startswith(prefix):
                    index = offset if action == prefix else offset + int(action.rsplit("-", 1)[1]) - 1
                    if 0 <= index < len(FADERS):
                        section, name = FADERS[index]
                        ctrl = self.controls(section).get(name)
                        if ctrl is None:
                            break
                        if reset:
                            self.reset_control(section, ctrl)
                        else:
                            self.set_position(section, ctrl, 0, position)
                    break

    def set_position(self, section, control, axis, position):
        values = list(self.settings(section)[control.name])
        low, high = control.low[axis], control.high[axis]
        value = min(high, low + (high - low) * position)
        values[axis] = (control.choices[round(position * (len(control.choices) - 1))]
                        if control.kind == "dropdown" else round(value) if control.kind == "integer" else value)
        self.set_control(section, control.name, values)

    def reset_control(self, section, control, axis=None):
        values = list(self.settings(section)[control.name])
        if axis is None:
            values = control.default
        else:
            values[axis] = control.default[axis]
        self.set_control(section, control.name, values)

    def select_bank(self, bank):
        if 0 <= bank < self.pot_bank_count():
            self.bank = bank
            self.deck.refresh()
            self.midi_label()

    def pot_section(self):
        return "Media" if self.live_lane == 3 else "Scene"

    def live_scene_lane(self):
        return 1 if isinstance(self.engine.backend, MilkDrop) else 2

    def select_lane(self, layer):
        if layer not in (None, 3, self.live_scene_lane()):
            layer = None
        self.live_lane = layer
        section = self.pot_section()
        controls = self.controls(section)
        assigned = self.wheel_assignments.get(self.wheel_key())
        if assigned is None:
            xy = next((control.name for control in controls.values() if control.kind == "xy"), None)
            assigned = ((xy, 0), (xy, 1)) if xy else (None, None)
        self.wheel_targets = [target if target and target[0] in controls
                              and target[1] < len(controls[target[0]].default) else None for target in assigned]
        self.bank = 0
        self.select(section)
        self.midi_label()

    def route_sprite(self, route):
        if route == "overlay" or route == self.live_sprite_destination():
            self.set_control("Media", "spriteRoute", (float(route != "overlay"),))

    def live_sprite_destination(self):
        if not self.engine.backend.nested_route:
            return None
        return "milk" if self.live_scene_lane() == 1 else "shader"

    def deck_selection(self):
        scene_lane = self.live_scene_lane()
        layer = scene_lane if self.live_lane is None else self.live_lane
        destination = self.live_sprite_destination()
        route = destination if self.values["Media"]["spriteRoute"][0] else "overlay"
        return layer, (scene_lane, 3), route, ("overlay", destination), self.live_lane is None

    def pad_controls(self):
        return [ctrl for ctrl in self.controls(self.pot_section()).values() if ctrl.kind in ("toggle", "bang")]

    def press_pad(self, index):
        controls = self.pad_controls()
        if 0 <= index < len(controls):
            ctrl = controls[index]
            section = self.pot_section()
            self.set_control(section, ctrl.name, ctrl.low if self.settings(section)[ctrl.name] == ctrl.high else ctrl.high)

    def pot_controls(self):
        return [(ctrl, axis) for ctrl in self.controls(self.pot_section()).values()
                if ctrl.kind not in ("toggle", "bang") for axis in range(len(ctrl.default))]

    def has_nested_bank(self):
        backend = self.engine.backend
        return self.pot_section() == "Scene" and isinstance(backend, MilkDrop) and bool(backend.nested_route)

    def pot_bank_count(self):
        return 2 if self.has_nested_bank() else min(8, (len(self.pot_controls()) + 7) // 8)

    def bank_pots(self):
        if self.bank == 1 and self.has_nested_bank():
            return [("Media", ctrl, axis) for ctrl in NESTED for axis in range(len(ctrl.default))]
        start = self.bank * 8
        return [(self.pot_section(), ctrl, axis) for ctrl, axis in self.pot_controls()[start:start + 8]]

    def pointer(self, event, pressed=None):
        x, y, width, height = self.rect
        point = (min(1., max(0., (event.x - x) / width)),
                 min(1., max(0., 1 - (event.y - y) / height)))
        _, last, down = self.mouse
        self.mouse = (point, point if pressed else last, down if pressed is None else pressed)

    def present(self):
        if self.output:
            self.preview.delete("frame")
            width, height = self.output["bounds"][2:]
            self.preview.create_text(self.preview.winfo_width() // 2, self.preview.winfo_height() // 2,
                text=f"Fullscreen output · {width} × {height}\nEsc or F11 to return", justify="center",
                fill=MUTED, font=("Segoe UI", 11), tags="frame")
            return
        if not self.frame:
            return
        frame = self.frame
        if not self.timeline_mode and self.values["Meta"]["alpha"][0]:
            if not hasattr(self, "checker") or self.checker.size != frame.size:
                self.checker = Image.new("RGBA", frame.size)
                self.checker.putdata([(36, 45, 58, 255) if (x // 16 + y // 16) % 2 else (24, 30, 38, 255)
                                      for y in range(frame.height) for x in range(frame.width)])
            frame = Image.alpha_composite(self.checker, frame)
        self.rect = self.present_frame(self.preview, frame, "photo")

    def present_frame(self, canvas, frame, attribute):
        width, height = canvas.winfo_width(), canvas.winfo_height()
        ratio = min(width / frame.width, height / frame.height)
        size = max(1, round(frame.width * ratio)), max(1, round(frame.height * ratio))
        x, y = (width - size[0]) // 2, (height - size[1]) // 2
        photo = ImageTk.PhotoImage(frame if size == frame.size else frame.resize(size, Image.Resampling.BILINEAR))
        items = canvas.find_withtag("frame")
        if items and canvas.type(items[0]) == "image":
            canvas.itemconfigure(items[0], image=photo)
            canvas.coords(items[0], x, y)
        else:
            canvas.delete("frame")
            canvas.create_image(x, y, anchor="nw", image=photo, tags="frame")
        setattr(self, attribute, photo)
        return x, y, *size

    def clean_output(self):
        if self.output:
            self.return_preview()
            return
        try:
            self.change_output({"token": uuid4().hex, "bounds": output_bounds(self.root),
                                "showFps": self.show_fps.get()})
        except (OSError, RuntimeError) as error:
            self.error("Fullscreen output unavailable", error)

    def return_preview(self, token=None):
        if self.output and (token is None or token == self.output["token"]):
            self.change_output(None)

    def toggle_fps(self):
        if self.output:
            self.change_output({**self.output, "showFps": self.show_fps.get()})

    def change_output(self, output):
        if self.link:
            self.link.send(command="output", view=output)
        self.output = output
        self.output_button.configure(text="Return preview" if output else "Fullscreen output")
        self.preview.delete("frame")
        self.dirty = self.scene_dirty = True
        self.present()

    def capture(self):
        if self.timeline_mode and self.output:
            if self.state.get("emptyPreview"):
                self.frame = None
            elif (image := self.link.preview()) is not None:
                self.frame = image
        if self.frame:
            name = filedialog.asksaveasfilename(parent=self.root, title="Save preview frame", defaultextension=".png",
                                                filetypes=[("PNG", "*.png")])
            if name:
                try:
                    self.frame.save(name)
                    self.notice(f"Saved {Path(name).name}")
                except OSError as error:
                    self.error("Frame could not save", error)

    def tick(self):
        if self.closed:
            return
        started = time.monotonic()
        try:
            for action, position in self.midi.poll():
                self.midi_event(action, position)
            self.update_live_audio()
            revision = self.media.revision
            self.media.tick(started, self.values["Media"]["speed"][0])
            if self.scrub and self.media.playing and not self.scrubbing:
                self.scrub.set(self.media.position)
            if self.scrub:
                self.media_play.configure(text="Pause" if self.media.playing else "Play")
            if revision != self.media.revision:
                self.changed("Media", sync=False)
            if self.running or self.dirty:
                dt = min(.1, started - self.clock) if self.running else 0.
                self.elapsed += dt
                audio = self.audio.frame(self.elapsed, dt)
                try:
                    self.engine.set_output(self.output, self.return_preview)
                except Exception as error:
                    self.return_preview()
                    self.error("Fullscreen output unavailable", error)
                self.frame = self.engine.render(self.elapsed, dt, audio, self.values["Meta"], self.values["Media"],
                                                self.media, self.mouse, self.running, self.running or self.scene_dirty)
                self.present()
                minutes, seconds = divmod(int(self.elapsed), 60)
                self.timer.configure(text=f"{minutes:02}:{seconds:02}")
                for name, meter in self.meters.items():
                    meter.delete("level")
                    meter.create_rectangle(0, 0, 42 * audio.uniforms[f"syn_{name}"][0], 4,
                                           fill=COPPER, width=0, tags="level")
                self.dirty = self.scene_dirty = False
                if self.running and self.audio.mode == "Audio file" and self.audio.track is not None:
                    if self.audio.playback is None and self.audio.cursor >= self.audio.track.frames:
                        self.set_running(False)
                        self.notice("Audio playback finished")
            else:
                self.engine.refresh_output_counter()
            self.poll_shuffle()
        except Exception as error:
            self.set_running(False)
            self.media.playing = False
            self.dirty = self.scene_dirty = False
            self.notice(f"Preview error · {error}")
        self.clock = started
        delay = max(5, round(33 - (time.monotonic() - started) * 1000))
        self.root.after(delay, self.tick)

    def close(self):
        self.closed = True
        try:
            self.audio.close()
            self.media.clear()
            self.midi.close()
            if self.engine:
                self.engine.close()
        finally:
            self.root.destroy()


class ConnectedStudio(Studio):
    def __init__(self, root, runtime, folder):
        self.runtime = runtime
        self.live_running = True
        self.selected = (None, "")
        self.signature = None
        self.editable = False
        self.state = {}
        self.last_contact = time.monotonic()
        self.linked = {section: {} for section in ("Scene", "Meta", "Media")}
        self.linked_values = {section: {} for section in self.linked}
        self.pending = {}
        self.outbox = None
        self.gesture = None
        self.target_choices = ()
        self.awaited_focus = None
        self.swap_status = ""
        self.swap_notice_until = 0.
        self.preview_deadline = None
        super().__init__(root, None, alloy_port=0, link=AlloySession(folder))
        self.root.after(8, self.tick_preview)

    def initialise_preview(self):
        self.accept_state(self.link.initial_state())

    def build_controls(self):
        if self.control_build is not None:
            self.control_build.close()
            self.control_build = None
        super().build_controls()

    def milkdrop_library(self):
        library = self.state.get("milkDropLibrary")
        return Path(library) if library else None

    def controls(self, section):
        return self.linked[section] if self.timeline_mode else super().controls(section)

    def settings(self, section):
        return self.linked_values[section] if self.timeline_mode else super().settings(section)

    def has_nested_bank(self):
        return not self.timeline_mode and super().has_nested_bank()

    def pot_section(self):
        if self.timeline_mode:
            return "Media" if self.state.get("layer") == 3 else "Scene"
        return super().pot_section()

    def wheel_key(self):
        return ("Alloy", self.selected[1], self.pot_section()) if self.timeline_mode else super().wheel_key()

    def pick_bundled_scene(self, event):
        if not self.timeline_mode:
            return super().pick_bundled_scene(event)
        index = self.scene_picker.current()
        if 0 <= index < len(self.target_choices):
            clip = self.target_choices[index][0]
            if clip is not None:
                self.request_focus("pinned", clip, command="focus", clip=clip)

    def select_lane(self, layer):
        if not self.timeline_mode:
            return super().select_lane(layer)
        if layer is None:
            self.request_focus("pinned", "", command="focus", clip="")
        else:
            self.request_focus("focusedLayer", layer, command="focus-lane", layer=layer)

    def request_focus(self, key, value, **request):
        try:
            self.flush_values()
            self.link.send(**request)
            self.awaited_focus = key, value, time.monotonic() + .6
            self.editable = False
            self.build_controls()
            self.deck.refresh_lane_controls()
        except OSError as error:
            self.error("Could not select Alloy visual", error)

    def route_sprite(self, route):
        if not self.timeline_mode:
            return super().route_sprite(route)
        routing = self.state.get("spriteRouting")
        if not routing or self.awaited_focus or not self.state["editable"]:
            return
        try:
            self.flush_values()
            self.link.send(command="sprite-route", clip=routing["clip"], scene=routing["scene"], route=route)
        except OSError as error:
            self.error("Could not route sprite", error)

    def deck_selection(self):
        if not self.timeline_mode:
            return super().deck_selection()
        layer = self.state.get("focusedLayer", -1)
        if layer < 0:
            layer = self.state.get("layer", -1)
        if self.awaited_focus and self.awaited_focus[0] == "focusedLayer":
            layer = self.awaited_focus[1]
        routing = self.state.get("spriteRouting")
        available = ("overlay", "milk", "shader") if routing and self.state["editable"] and not self.awaited_focus else ()
        following = not self.awaited_focus and self.state.get("focusedLayer", -1) < 0 and not self.state.get("pinned")
        return layer, range(len(LANE_LABELS)), routing["route"] if routing else None, available, following

    def sync_timeline_picker(self, state):
        lane = state.get("focusedLayer", -1)
        choices = [(None, f"Follow {LANE_LABELS[lane]}")] if lane >= 0 else []
        choices.append(("", "Follow playhead"))
        for target in state.get("targets", ()):
            minutes, seconds = divmod(int(target["start"]), 60)
            label = f"{minutes}:{seconds:02} · {LANE_LABELS[target['layer']]} · {target['title']}"
            choices.append((target["clip"], label))
        if tuple(choices) != self.target_choices:
            self.target_choices = tuple(choices)
            self.scene_picker.configure(values=tuple(label for _, label in choices))
        desired = (self.awaited_focus[1] if self.awaited_focus and self.awaited_focus[0] == "pinned"
                   else None if lane >= 0 else state.get("pinned", ""))
        selected = next((i for i, (clip, _) in enumerate(choices) if clip == desired), 0)
        if self.scene_picker.current() != selected:
            self.scene_picker.current(selected)

    def set_timeline_mode(self, timeline):
        if self.control_build is not None:
            self.control_build.close()
            self.control_build = None
        self.frame = None
        self.preview.delete("frame")
        if timeline:
            self.shuffle_request = None
            if self.engine:
                self.engine.set_output(None)
            self.live_running = self.running
            if self.audio.mode != "Alloy live":
                self.listen_to_alloy()
        self.timeline_mode = timeline
        self.selected = (None, "")
        self.signature = None
        self.awaited_focus = None
        self.target_choices = ()
        self.pending.clear()
        self.outbox = None
        self.subtitle.configure(text=f"{'Alloy timeline' if timeline else 'Alloy live'}   ·   {VERSION}")
        for button in (*self.scene_buttons, *self.shuffle_buttons, self.audio_file_button):
            button.state(["disabled" if timeline else "!disabled"])
        self.audio_mode.configure(state="disabled" if timeline else "readonly")
        if timeline:
            self.live_tools.pack_forget()
            self.bpm_box.state(["readonly"])
            self.family_picker.pack_forget()
            self.scene_picker.configure(width=32)
            self.link.preview_stamp = 0
            self.preview_deadline = None
            return
        self.live_tools.pack(fill="x", padx=12, pady=(0, 7), before=self.swap_tools)
        self.family_picker.pack(side="right", padx=5)
        self.scene_picker.configure(width=22)
        self.running, self.clock = self.live_running, time.monotonic()
        if self.engine is None:
            self.engine = Engine(self.runtime)
            super().initialise_preview()
        else:
            self.sync_scene_pickers(self.engine.scene.path)
            self.scene_title.configure(text=self.engine.scene.title[:54])
            self.engine_label.configure(text="GLSL  /  OPENGL" if isinstance(self.engine.backend, Shader) else "MILKDROP  /  PROJECTM")
            self.select_lane(self.live_lane)
            self.dirty = True
        self.play_button.configure(text="Pause" if self.running else "Play")
        self.present()

    def accept_state(self, state):
        self.state = state
        self.last_contact = time.monotonic()
        for button in self.swap_buttons:
            button.state(["!disabled" if state["editable"] else "disabled"])
        timeline = bool(state["hasVisuals"])
        if timeline != self.timeline_mode or (not timeline and self.engine is None):
            self.set_timeline_mode(timeline)
        if not timeline:
            return
        if self.awaited_focus:
            key, requested, deadline = self.awaited_focus
            confirmed = state.get(key) == requested and (requested != "" or state.get("focusedLayer", -1) < 0)
            if confirmed or time.monotonic() >= deadline:
                self.awaited_focus = None
        self.sync_timeline_picker(state)
        parameters = state.get("parameters", {})
        identity = state.get("clip"), state.get("scene", "")
        signature = identity, tuple((name, p["label"], p["steps"]) for name, p in parameters.items())
        editable = bool(state["editable"]) and bool(identity[0]) and self.awaited_focus is None
        rebuild = signature != self.signature or editable != self.editable
        if rebuild and self.control_build is not None:
            self.control_build.close()
            self.control_build = None
        if signature != self.signature:
            changed_clip = self.selected != identity
            self.selected, self.signature = identity, signature
            self.linked = linked_controls(state, ROOT)
            self.pending.clear()
            self.outbox = None
            self.gesture = None
            section = self.pot_section()
            if changed_clip:
                self.bank = 0
                self.section.set(section)
            else:
                self.bank = min(self.bank, max(0, self.pot_bank_count() - 1))
            controls = self.controls(section)
            assigned = self.wheel_assignments.get(self.wheel_key(), (None, None))
            self.wheel_targets = [target if target and target[0] in controls and target[1] < len(controls[target[0]].default)
                                  else None for target in assigned]
        self.editable = editable
        now = time.monotonic()
        for key, (value, expires) in tuple(self.pending.items()):
            if now > expires or abs(parameters[key]["value"] - value) < 1e-7:
                self.pending.pop(key)
        old_values = self.linked_values
        self.linked_values = {
            section: {control.name: decode(control, tuple(self.pending.get(key, (parameters[key]["value"], 0))[0]
                                                          for key in wire_names(control)))
                      for control in controls.values()}
            for section, controls in self.linked.items()
        }
        layer = state.get("focusedLayer", -1)
        empty = f"No {LANE_LABELS[layer]} clip" if layer >= 0 else "No controllable visual clips"
        self.scene_title.configure(text=state.get("title", empty)[:54])
        self.engine_label.configure(text="OUTSIDE PLAYHEAD" if identity[0] and state.get("atPlayhead") is False else "ALLOY TIMELINE")
        running = bool(state["playing"])
        self.running = running
        self.play_button.configure(text="Pause" if running else "Play")
        if rebuild:
            self.control_build = self.build_control_rows()
        else:
            if self.control_build is None:
                self.sync_widgets()
            if old_values != self.linked_values:
                self.deck.refresh()
        self.deck.refresh_lane_controls()

    def advance_control_build(self):
        if self.control_build is None:
            return
        deadline = time.monotonic() + .006
        while time.monotonic() < deadline:
            if next(self.control_build, False) is False:
                self.control_build = None
                return

    def control_widget(self, card, section, control):
        super().control_widget(card, section, control)
        if not self.timeline_mode or not self.editable:
            return
        for axis, key in enumerate(wire_names(control)):
            spec = self.state["parameters"][key]
            row = tk.Frame(card, bg=CARD)
            row.pack(fill="x", padx=10, pady=(0, 5))
            if len(control.default) > 1:
                self.label(row, spec["label"], bg=CARD, fg=MUTED, size=8).pack(side="left")
            reset = self.button(row, "Reset", lambda i=axis: self.reset_control(section, control, i), compact=True)
            reset.pack(side="right", padx=(4, 0))
            self.button(row, "Automation", lambda name=key: self.automation_dialog(name), compact=True).pack(side="right")
            auto = tk.BooleanVar(value=spec["cycle"])
            if not spec["steps"]:
                ttk.Checkbutton(row, text="Auto", variable=auto,
                                command=lambda name=key, flag=auto: self.edit_automation(name, flag.get())).pack(side="right", padx=4)

            def refresh(name=key, flag=auto, button=reset):
                latest = self.state["parameters"][name]
                if flag.get() != latest["cycle"]:
                    flag.set(latest["cycle"])
                button.state(["!disabled" if latest["held"] else "disabled"])

            self.widgets.append(refresh)
            refresh()

    def automation_dialog(self, key):
        spec = self.state["parameters"][key]
        target = self.selected
        window = tk.Toplevel(self.root, bg=PANEL)
        window.title(f"{spec['label']} automation")
        window.transient(self.root)
        fields = {}
        response = {"Off": "", "Bass": "bass", "Overall energy": "energy"}
        source = tk.StringVar(value=next(label for label, value in response.items() if value == spec["audioSource"]))
        message = self.label(window, fg=COPPER, wraplength=280)
        if not spec["steps"]:
            for row, (label, name, scale) in enumerate((("Min (%)", "cycleMin", 100), ("Max (%)", "cycleMax", 100),
                                                        ("Cycle length (bars)", "cycleBars", 1), ("Response depth (%)", "audioAmount", 100))):
                self.label(window, label).grid(row=row, column=0, padx=12, pady=4, sticky="w")
                field = ttk.Entry(window, width=12)
                field.insert(0, f"{spec[name] * scale:g}")
                field.grid(row=row, column=1, padx=12, sticky="ew")
                fields[name] = field, scale
            self.label(window, "Audio response").grid(row=4, column=0, padx=12, pady=4, sticky="w")
            ttk.Combobox(window, values=tuple(response), textvariable=source, state="readonly", width=13).grid(row=4, column=1, padx=12)

            def apply():
                if target != self.selected or not self.editable:
                    message.configure(text="The timeline target changed. Reopen automation on the current clip.")
                    return
                try:
                    settings = {name: float(field.get()) / scale for name, (field, scale) in fields.items()}
                    if not (0 <= settings["cycleMin"] <= settings["cycleMax"] <= 1
                            and 1 <= settings["cycleBars"] <= 1024 and 0 <= settings["audioAmount"] <= 1):
                        raise ValueError
                except ValueError:
                    message.configure(text="Use 0–100% for range and depth, with Min ≤ Max, and 1–1024 bars.")
                    return
                self.edit_automation(key, self.state["parameters"][key]["cycle"], **settings, audioSource=response[source.get()])
                window.destroy()

            self.button(window, "Apply", apply, accent=True).grid(row=6, column=1, padx=12, pady=8, sticky="e")

        def point():
            if target == self.selected and self.editable:
                self.flush_values()
                value = self.pending.get(key, (self.state["parameters"][key]["value"], 0))[0]
                self.send_edit(parameter=key, action=2, value=value)
                window.destroy()

        self.button(window, "Point at playhead", point).grid(row=6, column=0, padx=12, pady=8, sticky="w")
        message.grid(row=7, column=0, columnspan=2, padx=12, pady=(0, 6))

    def edit_automation(self, key, cycle, **settings):
        self.flush_values()
        self.send_edit(parameter=key, action=1, value=float(cycle), settings=settings)

    def swap_lane(self, layer):
        self.flush_values()
        self.link.send(command="swap", layer=layer)

    def send_edit(self, **request):
        clip, scene = self.selected
        self.link.send(clip=clip, scene=scene, **request)

    def locked(self, section, control):
        if self.timeline_mode and not self.editable:
            return True
        return super().locked(section, control)

    def publish_values(self, batch, gesture):
        if self.outbox and self.outbox[:2] != (self.selected, gesture):
            self.flush_values()
        if self.outbox:
            self.outbox[2].update(batch)
        else:
            self.outbox = self.selected, gesture, batch.copy()
        deadline = time.monotonic() + .35
        self.pending.update({key: (value, deadline) for key, value in batch.items()})

    def flush_values(self):
        if self.outbox:
            (clip, scene), gesture, batch = self.outbox
            self.outbox = None
            self.link.send(clip=clip, scene=scene, values=batch, parameter=next(iter(batch)), gesture=gesture)

    def set_control(self, section, name, values):
        if not self.timeline_mode:
            return super().set_control(section, name, values)
        control = self.controls(section)[name]
        if self.locked(section, control) or self.settings(section)[name] == tuple(values):
            return
        now = time.monotonic()
        previous = self.gesture
        token = previous[2] if previous and previous[0] == name and now - previous[1] < .45 else uuid4().hex
        self.gesture = name, now, token
        old = encode(control, self.settings(section)[name])
        self.publish_values({key: value for key, value in encode(control, values).items() if value != old[key]}, token)
        self.settings(section)[name] = tuple(values)
        self.sync_widgets()
        self.deck.refresh()

    def change_group(self, section, group=None, randomize=False):
        if not self.timeline_mode:
            return super().change_group(section, group, randomize)
        batch = {}
        for control in self.controls(section).values():
            if (group is None or control.group == group) and not self.locked(section, control):
                batch.update(encode(control, self.group_value(control, randomize)))
        if batch and randomize:
            self.publish_values(batch, uuid4().hex)
            self.gesture = None
        elif batch:
            self.reset_parameters(list(batch))

    def reset_control(self, section, control, axis=None):
        if not self.timeline_mode:
            return super().reset_control(section, control, axis)
        names = wire_names(control)
        self.reset_parameters(list(names) if axis is None else [names[axis]])

    def reset_parameters(self, names):
        if self.editable:
            self.flush_values()
            for name in names:
                self.pending.pop(name, None)
            self.gesture = None
            self.send_edit(command="reset", parameters=names)

    def undo(self, redo=False):
        if not self.timeline_mode:
            return super().undo(redo)
        if self.editable:
            self.flush_values()
            self.pending.clear()
            self.gesture = None
            self.send_edit(command="redo" if redo else "undo")

    def pick_scene(self):
        if not self.timeline_mode:
            super().pick_scene()

    def reload(self):
        if not self.timeline_mode:
            super().reload()

    def look(self, save=False):
        if not self.timeline_mode:
            super().look(save)

    def set_running(self, running):
        if self.timeline_mode:
            self.link.send(command="play" if running else "pause")
        else:
            super().set_running(running)

    def restart(self):
        if self.timeline_mode:
            self.link.send(command="home")
        else:
            super().restart()

    def midi_event(self, action, position):
        if self.timeline_mode and action == "stop":
            self.link.send(command="stop")
        else:
            super().midi_event(action, position)

    def tick_preview(self):
        if self.closed:
            return
        started = time.monotonic()
        poll_ms = 8
        if self.timeline_mode and not self.output:
            if self.state.get("emptyPreview"):
                self.preview_deadline = None
                if self.frame is not None:
                    self.frame = None
                    self.preview.delete("frame")
            else:
                latest = self.link.latest_preview_sequence()
                previous = self.link.preview_stamp
                period = 1 / 30
                if not self.running or not previous:
                    self.preview_deadline = None
                    next_frame = latest
                elif self.preview_deadline is None:
                    next_frame = previous
                    if latest - previous >= 2:
                        self.preview_deadline = started
                elif started >= self.preview_deadline:
                    elapsed_slots = int((started - self.preview_deadline) / period) + 1
                    self.preview_deadline += elapsed_slots * period
                    # Retain roughly two frames of headroom. Recover from
                    # delayed UI work without waiting to fill the buffer again.
                    next_frame = min(latest, max(previous + 1, latest - 3))
                else:
                    next_frame = previous
                image = self.link.preview(next_frame)
                if image is None and next_frame > previous and latest > next_frame:
                    # A seek or transport change leaves a hole in the ring.
                    image = self.link.preview(latest)
                if self.preview_deadline is not None:
                    poll_ms = max(2, min(8, round((self.preview_deadline - started) * 1000)))
                if image is not None:
                    self.frame = image
                    self.present()
        else:
            self.preview_deadline = None
        self.advance_control_build()
        delay = max(1, round(poll_ms - (time.monotonic() - started) * 1000))
        self.root.after(delay, self.tick_preview)

    def tick(self):
        if self.closed:
            return
        started = time.monotonic()
        try:
            if self.link.signalled("closed"):
                self.close()
                return
            if self.link.signalled("focus", consume=True):
                self.root.deiconify()
                self.root.lift()
                self.root.focus_force()
            if self.output and self.link.signalled(f"attach-{self.output['token']}", consume=True):
                self.return_preview()
            self.flush_values()
            self.accept_state(self.link.state())
            if self.timeline_mode:
                if self.output and self.state.get("outputToken") == self.output["token"] and self.state.get("outputError"):
                    self.return_preview()
                    self.error("Fullscreen output unavailable", self.state["outputError"])
                for action, position in self.midi.poll():
                    self.midi_event(action, position)
                self.update_live_audio()
                minutes, seconds = divmod(int(self.state["position"]), 60)
                self.timer.configure(text=f"{minutes:02}:{seconds:02}")
                swap_status = self.state.get("swapStatus")
                if swap_status != self.swap_status:
                    self.swap_status = swap_status
                    self.swap_notice_until = time.monotonic() + 3.
                self.notice(self.swap_status if time.monotonic() < self.swap_notice_until else self.state.get("status") or "Alloy timeline")
                delay = max(5, round(33 - (time.monotonic() - started) * 1000))
                self.root.after(delay, self.tick)
                return
        except (OSError, ValueError, KeyError):
            # Windows can briefly deny reads during an atomic file swap.
            if time.monotonic() - self.last_contact > 5:
                self.notice("Waiting for Alloy. Reopen Studio from Alloy if the connection has closed.")
                self.editable = False
        if not self.timeline_mode and self.engine is not None:
            super().tick()
        else:
            self.root.after(33, self.tick)

    def close(self):
        try:
            if self.editable and not self.link.signalled("closed"):
                self.flush_values()
        finally:
            try:
                super().close()
            finally:
                self.link.close_preview()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("scene", nargs="?", type=Path)
    parser.add_argument("--projectm", type=Path, default=ROOT.parent / ".local" / "visual-studio-runtime")
    parser.add_argument("--alloy-session", type=Path)
    parser.add_argument("--alloy-port", type=int, default=STUDIO_PORT)
    args = parser.parse_args()
    root = tk.Tk()
    engine = None
    try:
        if args.alloy_session:
            studio = ConnectedStudio(root, args.projectm.resolve(), args.alloy_session)
        else:
            engine = Engine(args.projectm.resolve())
            studio = Studio(root, engine, alloy_port=args.alloy_port)
            if args.scene:
                studio.open_scene(args.scene)
    except Exception as error:
        if engine:
            engine.close()
        messagebox.showerror("Visual Studio could not start", str(error), parent=root)
        root.destroy()
        return 1
    root.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
