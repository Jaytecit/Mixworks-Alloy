"""Alloy's pot, pad and fader layout for Studio's live mappings."""

import math
import tkinter as tk

from midi import FADERS


# From AlloyLookAndFeel.h.
BG, PANEL, CARD, BORDER = "#15191d", "#20262b", "#2a3239", "#3b454e"
TEXT, MUTED, COPPER, FOCUS = "#e7ebee", "#abb5be", "#d49a72", "#d5e9f5"
TRACKS = (COPPER, "#80b2cf", "#9eb8a1", "#b5a3c5", "#c8b57a", "#7fbab7", "#cc949c", "#9baad1")
FADER_LABELS = ("Bright", "Gamma", "Contr.", "Hue", "Satur.", "Sprite", "Scale", "Speed", "Master")


def axis_label(control, axis):
    size = len(control.default)
    return control.label + (" " + ("XY" if size == 2 else "RGBA")[axis] if size > 1 else "")


class Slot(tk.Frame):
    def __init__(self, parent, studio, fader, accent, wheel=None, caption=None):
        super().__init__(parent, bg=PANEL)
        self.studio, self.fader, self.accent, self.wheel = studio, fader, accent, wheel
        self.caption = caption
        self.target, self.drag, self.view = None, None, None
        self.label = studio.label(self, "—", fg=MUTED, size=7 if caption else 8, padx=0)
        self.label.place(relwidth=1, height=31)
        if not caption:
            self.label.bind("<Configure>", lambda e: self.label.configure(wraplength=max(1, e.width - 2)))
        self.canvas = tk.Canvas(self, bg=PANEL, bd=0, highlightthickness=1,
                                highlightbackground=PANEL, highlightcolor=FOCUS, takefocus=False)
        self.canvas.place(y=31, relwidth=1, relheight=1, height=-31)
        self.canvas.bind("<Configure>", lambda _: self.paint())
        self.canvas.bind("<ButtonPress-1>", self.press)
        self.canvas.bind("<B1-Motion>", self.move)
        self.canvas.bind("<Double-Button-1>", lambda _: self.reset())
        self.canvas.bind("<ButtonRelease-1>", self.release)
        for key, direction in (("Left", -1), ("Down", -1), ("Right", 1), ("Up", 1)):
            self.canvas.bind(f"<{key}>", lambda _, d=direction: self.nudge(d))
        self.canvas.bind("<Home>", lambda _: self.reset())

    def refresh(self, target):
        if target != self.target:
            self.target, self.drag = target, None
        label, value, position, enabled = "—", "", 0., False
        if target:
            section, control, axis = target
            label = self.caption or axis_label(control, axis)
            actual = self.studio.settings(section)[control.name][axis]
            position = self.position(actual)
            enabled = not self.studio.locked(section, control) and control.low[axis] < control.high[axis]
            value = (control.labels[control.choices.index(actual)] if control.kind == "dropdown"
                     else f"{actual:.3g}")
        if self.wheel is not None:
            label = ("Pitch wheel", "Mod wheel")[self.wheel]
        view = label, value, position, enabled
        if view != self.view:
            self.view = view
            self.label.configure(text=label, fg=self.accent if enabled else MUTED)
            self.canvas.configure(takefocus=enabled, cursor=("sb_v_double_arrow" if enabled else ""))
            self.paint()

    def position(self, value):
        _, control, axis = self.target
        if control.kind == "dropdown":
            return control.choices.index(value) / max(1, len(control.choices) - 1)
        low, high = control.low[axis], control.high[axis]
        return (value - low) / (high - low) if high > low else 0.

    def set(self, position):
        self.studio.set_position(*self.target, min(1., max(0., position)))

    def reset(self):
        if self.target and self.view[-1]:
            section, control, axis = self.target
            values = list(self.studio.settings(section)[control.name])
            values[axis] = control.default[axis]
            self.studio.set_control(section, control.name, values)
        self.drag = None
        return "break"

    def press(self, event):
        if self.target and self.view[-1]:
            self.canvas.focus_set()
            self.drag = event.y, self.view[2]
            if self.fader:
                self.move(event)

    def move(self, event):
        if self.drag is not None and self.view[-1]:
            y, position = self.drag
            if self.fader:
                position = 1. - (event.y - 9) / max(1, self.canvas.winfo_height() - 31)
            else:
                position += (y - event.y) / 105.
            self.set(position)

    def release(self, _):
        if self.wheel == 0 and self.drag is not None and self.view[-1]:
            self.set(.5)
        self.drag = None

    def nudge(self, direction):
        if self.target and self.view[-1]:
            _, control, axis = self.target
            increment = (1. / (len(control.choices) - 1) if control.kind == "dropdown"
                         else 1. / (control.high[axis] - control.low[axis]) if control.kind == "integer" else .02)
            self.set(self.view[2] + direction * increment)
        return "break"

    def paint(self):
        if self.view is None:
            return
        _, value, position, enabled = self.view
        canvas = self.canvas
        canvas.delete("all")
        w, h = canvas.winfo_width(), canvas.winfo_height()
        if w < 8 or h < 20:
            return
        x = w / 2
        accent = self.accent if enabled else BORDER
        if self.fader:
            top, bottom = 9, h - 22
            y = bottom - position * (bottom - top)
            if self.wheel is None:
                canvas.create_line(x, top, x, bottom, fill=BG, width=5)
                canvas.create_line(x, y, x, bottom, fill=accent, width=2)
                canvas.create_rectangle(x - 10, y - 5, x + 10, y + 5, fill=CARD, outline=BORDER)
                canvas.create_line(x - 7, y, x + 7, y, fill=accent)
            else:
                canvas.create_rectangle(x - 16, top - 5, x + 16, bottom + 5, fill=BG, outline=BORDER)
                for offset in range(-12, 13, 4):
                    if top - 2 <= y + offset <= bottom + 2:
                        canvas.create_line(x - 11, y + offset, x + 11, y + offset,
                                           fill=accent if offset == 0 else BORDER, width=2 if offset == 0 else 1)
        else:
            r = min(18., (w - 6) / 2, (h - 25) / 2)
            y = (h - 22) / 2
            ring = (x-r, y-r, x+r, y+r)
            canvas.create_arc(*ring, start=225, extent=-270, outline=BORDER, width=3, style="arc")
            canvas.create_arc(*ring, start=225, extent=-270 * position, outline=accent, width=3, style="arc")
            canvas.create_oval(x-r+4, y-r+4, x+r-4, y+r-4, fill=CARD, outline=BORDER)
            theta = math.radians(225 - 270 * position)
            canvas.create_line(x + 4 * math.cos(theta), y - 4 * math.sin(theta),
                               x + (r - 7) * math.cos(theta), y - (r - 7) * math.sin(theta),
                               fill=accent, width=2)
        canvas.create_text(x, h - (9 if self.fader else 11), text=value, width=max(1, w - 2),
                           fill=TEXT, font=("Segoe UI", 8 if self.fader else 7))


class LiveDeck(tk.Frame):
    def __init__(self, parent, studio):
        super().__init__(parent, bg=PANEL, highlightbackground=BORDER, highlightthickness=1, height=184)
        self.studio = studio
        tk.Frame(self, bg=COPPER).place(x=12, relwidth=1, width=-24, height=1)
        wheels = self.cluster(.012, .106)
        left = self.cluster(.128, .392)
        middle = self.cluster(.534, .302)
        right = self.cluster(.849, .141)
        studio.label(wheels, "WHEELS", fg=COPPER, size=9, weight="bold",
                     anchor="w").place(relwidth=1, height=19)
        self.bank_label = studio.label(left, fg=COPPER, size=9, weight="bold", anchor="w")
        self.bank_label.place(relwidth=1, height=19)
        studio.label(middle, "META / MEDIA  ·  FADERS", fg=COPPER, size=9,
                     weight="bold", anchor="w").place(relwidth=1, height=19)
        studio.label(right, "TRANSPORT", fg=COPPER, size=9,
                     weight="bold", anchor="w").place(relwidth=1, height=19)

        self.wheels, self.assignments = [], []
        self.wheel_menu = None
        for i in range(2):
            wheel = Slot(wheels, studio, True, TRACKS[i], wheel=i)
            self.cell(wheel, i, 2, 21, 113)
            self.wheels.append(wheel)
            button = self.button(wheels, "Assign…", lambda slot=i: self.choose_wheel(slot))
            self.cell(button, i, 2, 136, 33)
            self.assignments.append(button)

        self.pots, self.banks, self.pads, self.faders = [], [], [], []
        for i in range(8):
            pot = Slot(left, studio, False, TRACKS[i])
            self.cell(pot, i, 8, 21, 91)
            self.pots.append(pot)
            bank = self.button(left, f"Bank {i + 1}", lambda slot=i: studio.select_bank(slot))
            self.cell(bank, i, 8, 114, 24)
            self.banks.append(bank)
            pad = self.button(left, "—", lambda slot=i: studio.press_pad(slot))
            self.cell(pad, i, 8, 141, 29)
            self.pads.append(pad)
        for i in range(9):
            fader = Slot(middle, studio, True, TRACKS[i % 8] if i < 8 else COPPER, caption=FADER_LABELS[i])
            self.cell(fader, i, 9, 21, 119)
            self.faders.append(fader)
            reset = self.button(middle, "Reset", fader.reset, size=7)
            self.cell(reset, i, 9, 143, 26)
        for i, (text, action) in enumerate((
            ("Home", studio.restart), ("MIDI", studio.midi_dialog), ("Undo", studio.undo),
            ("Redo", lambda: studio.undo(redo=True)), ("Pause", studio.toggle),
            ("Stop", lambda: studio.midi_event("stop", 1.)), ("Frame", studio.capture),
            ("Rand.", lambda: studio.change_group("Scene", randomize=True))
        )):
            button = self.button(right, text, action, size=7)
            self.cell(button, i % 4, 4, 66 + 38 * (i // 4), 34)
            if i == 4:
                self.play = button

    def cluster(self, start, width):
        frame = tk.Frame(self, bg=PANEL)
        frame.place(relx=start, y=5, relwidth=width, relheight=1, height=-6)
        return frame

    @staticmethod
    def cell(widget, slot, count, y, height):
        widget.place(relx=slot / count, x=1, y=y, relwidth=1 / count, width=-3, height=height)

    @staticmethod
    def button(parent, text, command, size=8):
        widget = tk.Button(parent, text=text, command=command, bg=CARD, fg=TEXT,
                           activebackground=BORDER, activeforeground=TEXT, disabledforeground=MUTED,
                           bd=0, highlightbackground=BORDER, highlightcolor=FOCUS,
                           highlightthickness=1, font=("Segoe UI", size), padx=0, pady=0)
        widget.bind("<Configure>", lambda e: widget.configure(wraplength=max(1, e.width - 2)))
        return widget

    def choose_wheel(self, slot):
        if self.wheel_menu:
            self.wheel_menu.destroy()
        self.wheel_menu = menu = tk.Menu(self, tearoff=False, bg=CARD, fg=TEXT,
                                         activebackground=COPPER, activeforeground=BG)
        menu.add_command(label="Unassigned", command=lambda: self.studio.assign_wheel(slot, None))
        groups = {}
        for control, axis in self.studio.pot_controls():
            if control.group not in groups:
                group = tk.Menu(menu, tearoff=False, bg=CARD, fg=TEXT,
                                activebackground=COPPER, activeforeground=BG)
                groups[control.group] = group
                menu.add_cascade(label=control.group, menu=group)
            groups[control.group].add_command(label=axis_label(control, axis),
                                              command=lambda target=(control.name, axis):
                                              self.studio.assign_wheel(slot, target))
        button = self.assignments[slot]
        try:
            menu.tk_popup(button.winfo_rootx(), button.winfo_rooty() + button.winfo_height())
        finally:
            menu.grab_release()

    def refresh(self):
        studio = self.studio
        pots, pads = studio.bank_pots(), studio.pad_controls()
        scene_pots = bool(studio.pot_controls())
        bank_count = studio.pot_bank_count()
        bank_text = f"   ·   BANK {studio.bank + 1} OF {bank_count}" if bank_count else ""
        bank_title = "NESTED SPRITE POTS" if studio.bank == 1 and studio.has_nested_bank() else "SCENE POTS / PADS"
        self.bank_label.configure(text=bank_title + bank_text)
        for slot, (wheel, button) in enumerate(zip(self.wheels, self.assignments)):
            target = studio.wheel_targets[slot]
            if target:
                name, axis = target
                control = studio.engine.controls[name]
                wheel.refresh(("Scene", control, axis))
                button.configure(text=axis_label(control, axis))
            else:
                wheel.refresh(None)
                button.configure(text="Assign…")
            button.configure(state="normal" if scene_pots else "disabled")
        for i, (pot, bank, pad) in enumerate(zip(self.pots, self.banks, self.pads)):
            pot.refresh(pots[i] if i < len(pots) else None)
            bank.configure(state="normal" if i < bank_count else "disabled",
                           bg=COPPER if i == studio.bank and pots else CARD,
                           fg=BG if i == studio.bank and pots else TEXT)
            control = pads[i] if i < len(pads) else None
            active = control and control.kind == "toggle" and studio.engine.values[control.name] == control.high
            pad.configure(text=control.label if control else "—",
                          state="normal" if control and not studio.locked("Scene", control) else "disabled",
                          bg=TRACKS[i] if active else CARD, fg=BG if active else TEXT)
        for fader, (section, name) in zip(self.faders, FADERS):
            fader.refresh((section, studio.controls(section)[name], 0))
        self.play.configure(text="Pause" if studio.running else "Play",
                            bg=COPPER if studio.running else CARD, fg=BG if studio.running else TEXT)
