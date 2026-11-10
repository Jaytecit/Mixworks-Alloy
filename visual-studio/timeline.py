"""Standalone Visual Studio surface connected to one Alloy timeline.

The native app owns rendering and persistence. Atomic mailboxes carry bounded,
clip-bound normalized edits; this surface never opens an audio device.
"""
import argparse
import json
import os
from pathlib import Path
import time
import tkinter as tk
from tkinter import ttk
from uuid import uuid4
from PIL import Image, ImageTk
from media import MEDIA, META

BG, PANEL, CARD, TEXT, MUTED, COPPER = '#121619', '#1c2327', '#273136', '#e5e3de', '#a0acb0', '#dfaa80'
VERSION = '0.1.18'


class TimelineStudio:
    def __init__(self, root, folder):
        self.root, self.folder = root, Path(folder)
        self.state, self.rows, self.photo = {}, {}, None
        self.identity, self.signature, self.preview_stamp = None, None, 0
        self.gesture, self.dragging, self.pending = '', set(), {}
        self.output = None
        self.closed = False
        self.last_contact = time.monotonic()
        root.title(f'Mixworks Visual Studio  {VERSION}')
        root.geometry('1220x800')
        root.minsize(1000, 650)
        root.configure(bg=BG)
        root.protocol('WM_DELETE_WINDOW', self.close)
        style = ttk.Style(root)
        style.theme_use('clam')
        style.configure('.', background=PANEL, foreground=TEXT, font=('Segoe UI', 10))
        style.configure('TButton', background=CARD, padding=(12, 7), borderwidth=0)
        style.map('TButton', background=[('pressed', '#35434a'), ('active', '#35434a'), ('focus', CARD)], foreground=[('!disabled', TEXT), ('disabled', MUTED)])
        style.configure('TCheckbutton', background=CARD)
        style.configure('Horizontal.TScale', background=CARD, troughcolor=BG)
        header = tk.Frame(root, bg=BG)
        header.pack(fill='x', padx=22, pady=18)
        tk.Label(header, text='MIXWORKS / VISUAL STUDIO', fg=COPPER, bg=BG,
                 font=('Segoe UI', 13, 'bold')).pack(side='left')
        ttk.Button(header, text='Clean output', command=self.clean_output).pack(side='right')
        ttk.Button(header, text='Redo', command=lambda: self.send_command('redo')).pack(side='right', padx=5)
        ttk.Button(header, text='Undo', command=lambda: self.send_command('undo')).pack(side='right')
        root.bind('<Control-z>', lambda _: self.send_command('undo'))
        root.bind('<Control-y>', lambda _: self.send_command('redo'))
        self.subtitle = tk.Label(root, text='Connected to the Mixworks visuals timeline', fg=MUTED, bg=BG,
                                 font=('Segoe UI', 10), anchor='w')
        self.subtitle.pack(fill='x', padx=22, pady=(0, 14))
        body = tk.Frame(root, bg=BG)
        body.pack(fill='both', expand=True, padx=22)
        stage = tk.Frame(body, bg=PANEL)
        stage.pack(side='left', fill='both', expand=True, padx=(0, 14))
        self.title = tk.Label(stage, text='Select a visual clip in Mixworks', fg=TEXT, bg=PANEL,
                              font=('Segoe UI', 18, 'bold'), anchor='w')
        self.title.pack(fill='x', padx=16, pady=16)
        self.preview = tk.Canvas(stage, bg='#090d0f', highlightthickness=0)
        self.preview.pack(fill='both', expand=True, padx=12, pady=(0, 12))
        self.preview.bind('<Configure>', lambda _: self.present())
        tk.Label(stage, text='Move and trim clips in Mixworks. Shape their look here.', fg=MUTED,
                 bg=PANEL, font=('Segoe UI', 10)).pack(pady=(0, 14))
        side = tk.Frame(body, width=445, bg=PANEL)
        side.pack(side='right', fill='y')
        side.pack_propagate(False)
        tk.Label(side, text='SELECTED CLIP / CONTROLS', fg=COPPER, bg=PANEL,
                 font=('Segoe UI', 10, 'bold')).pack(anchor='w', padx=14, pady=(16, 6))
        tk.Label(side, text='0 = original scene     1 = MilkDrop\n128 bars sweeps up, then back. + Point keys the playhead.',
                 justify='left', fg=MUTED, bg=PANEL, font=('Segoe UI', 9)).pack(anchor='w', padx=14, pady=(0, 12))
        self.scroll = tk.Canvas(side, bg=PANEL, highlightthickness=0)
        scrollbar = ttk.Scrollbar(side, orient='vertical', command=self.scroll.yview)
        scrollbar.pack(side='right', fill='y')
        self.scroll.pack(fill='both', expand=True)
        self.scroll.configure(yscrollcommand=scrollbar.set)
        self.controls = tk.Frame(self.scroll, bg=PANEL)
        self.control_window = self.scroll.create_window(0, 0, window=self.controls, anchor='nw')
        self.controls.bind('<Configure>', lambda _: self.scroll.configure(scrollregion=self.scroll.bbox('all')))
        self.scroll.bind('<Configure>', lambda e: self.scroll.itemconfigure(self.control_window, width=e.width))
        root.bind('<MouseWheel>', lambda e: self.scroll.yview_scroll(-int(e.delta / 120), 'units'))
        self.status = tk.Label(root, text='Waiting for Mixworks…', fg=MUTED, bg=BG, anchor='w',
                               font=('Segoe UI', 9))
        self.status.pack(fill='x', padx=22, pady=14)
        root.after(20, self.tick)

    def send_command(self, command):
        if not self.identity:
            return
        destination = self.folder / 'requests' / f'{time.time_ns():020}-{uuid4().hex}.json'
        temporary = destination.with_suffix('.tmp')
        temporary.write_text(json.dumps(dict(command=command, clip=self.identity)), encoding='utf-8')
        os.replace(temporary, destination)

    def send(self, name, value, action=0):
        if not self.identity:
            return
        if action == 0:
            self.pending[name] = (self.identity, float(value), self.gesture or uuid4().hex)
        else:
            self.publish(name, self.identity, float(value), action, uuid4().hex)

    def publish(self, name, clip, value, action, gesture):
        request = dict(clip=clip, parameter=name, value=value, action=action, gesture=gesture)
        destination = self.folder / 'requests' / f'{time.time_ns():020}-{uuid4().hex}.json'
        temporary = destination.with_suffix('.tmp')
        temporary.write_text(json.dumps(request), encoding='utf-8')
        os.replace(temporary, destination)

    def press(self, name):
        self.dragging.add(name)
        self.gesture = uuid4().hex

    def release(self, name):
        self.dragging.discard(name)
        self.gesture = ''

    def build(self, parameters):
        for child in self.controls.winfo_children():
            child.destroy()
        self.rows.clear()
        # The blend remains prominent even for shaders with many parameters.
        for name in sorted(parameters, key=lambda n: (n != 'milkMix', parameters[n]['label'])):
            spec = parameters[name]
            card = tk.Frame(self.controls, bg=CARD)
            card.pack(fill='x', padx=10, pady=4)
            heading = tk.Frame(card, bg=CARD)
            heading.pack(fill='x', padx=10, pady=(8, 2))
            tk.Label(heading, text=spec['label'], fg=COPPER if name == 'milkMix' else TEXT,
                     bg=CARD, font=('Segoe UI', 10, 'bold')).pack(side='left')
            value_label = tk.Label(heading, fg=MUTED, bg=CARD, font=('Segoe UI', 9))
            value_label.pack(side='right')
            variable, cycle = tk.DoubleVar(value=spec['value']), tk.BooleanVar(value=spec['cycle'])
            slider = ttk.Scale(card, variable=variable, from_=0, to=1,
                               command=lambda v, n=name, steps=spec['steps']: self.changed(n, v, steps))
            slider.pack(fill='x', padx=10)
            slider.bind('<ButtonPress-1>', lambda _, n=name: self.press(n))
            slider.bind('<ButtonRelease-1>', lambda _, n=name: self.release(n))
            actions = tk.Frame(card, bg=CARD)
            actions.pack(fill='x', padx=8, pady=(2, 6))
            ttk.Checkbutton(actions, text='128 bars', variable=cycle,
                            command=lambda n=name, c=cycle: self.send(n, float(c.get()), 1)).pack(side='left')
            ttk.Button(actions, text='+ Point', command=lambda n=name, v=variable: self.send(n, v.get(), 2)).pack(side='right')
            self.rows[name] = variable, cycle, slider, value_label
        self.scroll.yview_moveto(0)

    @staticmethod
    def display_value(name, value):
        if name == 'milkMix':
            return f'{round(100 * (1 - value))}% original / {round(100 * value)}% MilkDrop'
        control = MEDIA.get(name) or META.get(name)
        if control and len(control.default) == 1:
            if control.kind == 'dropdown':
                return control.labels[round(value * (len(control.labels) - 1))]
            if control.kind == 'toggle':
                return 'On' if value >= .5 else 'Off'
            return f'{control.low[0] + value * (control.high[0] - control.low[0]):.3g}'
        return f'{value:.3f}'

    def changed(self, name, value, steps):
        value = float(value)
        if steps > 1:
            value = round(value * (steps - 1)) / (steps - 1)
        self.rows[name][3].configure(text=self.display_value(name, value))
        self.send(name, value)

    def clean_output(self):
        if self.output is None or not self.output.winfo_exists():
            self.output = tk.Toplevel(self.root, bg='black')
            self.output.title('Mixworks Visual Studio / Output')
            self.output.geometry('960x540')
            self.output_canvas = tk.Canvas(self.output, bg='black', highlightthickness=0)
            self.output_canvas.pack(fill='both', expand=True)
            self.output_canvas.bind('<Configure>', lambda _: self.present())
            self.output.bind('<F11>', lambda _: self.output.attributes('-fullscreen', not self.output.attributes('-fullscreen')))
            self.output.bind('<Escape>', lambda _: self.output.attributes('-fullscreen', False))
        else:
            self.output.lift()
        self.present()

    def present(self):
        if not getattr(self, 'frame', None):
            return
        targets = [(self.preview, 'photo')]
        if self.output is not None and self.output.winfo_exists():
            targets.append((self.output_canvas, 'output_photo'))
        for canvas, attribute in targets:
            w, h = max(1, canvas.winfo_width()), max(1, canvas.winfo_height())
            factor = min(w / self.frame.width, h / self.frame.height)
            photo = ImageTk.PhotoImage(self.frame.resize((max(1, round(self.frame.width * factor)),
                                                         max(1, round(self.frame.height * factor))), Image.Resampling.BILINEAR))
            setattr(self, attribute, photo)
            canvas.delete('frame')
            canvas.create_image(w / 2, h / 2, image=photo, tags='frame')

    def tick(self):
        if self.closed:
            return
        try:
            if (self.folder / 'closed').exists():
                self.close()
                return
            focus = self.folder / 'focus'
            if focus.exists():
                focus.unlink()
                self.root.deiconify()
                self.root.lift()
            for name, (clip, value, gesture) in tuple(self.pending.items()):
                self.publish(name, clip, value, 0, gesture)
            self.pending.clear()
            state = json.loads((self.folder / 'state.json').read_text(encoding='utf-8-sig'))
            if state.get('protocol') != 1:
                raise ValueError('Unsupported Mixworks connection')
            if time.time() * 1000 - state['updated'] > 5000:
                self.status.configure(text='Mixworks is unavailable. Reopen Studio from the timeline.')
                self.root.after(200, self.tick)
                return
            self.last_contact = time.monotonic()
            parameters = state.get('parameters', {})
            identity = state.get('clip')
            signature = identity, tuple((n, p['label'], p['steps']) for n, p in parameters.items())
            if signature != self.signature:
                self.identity, self.signature = identity, signature
                self.dragging.clear()
                self.build(parameters)
            for name, spec in parameters.items():
                variable, cycle, slider, label = self.rows[name]
                if name not in self.dragging:
                    variable.set(spec['value'])
                    label.configure(text=self.display_value(name, spec['value']))
                cycle.set(spec['cycle'])
                slider.configure(state='disabled' if spec['cycle'] or not state.get('editable', True) else 'normal')
            self.title.configure(text=state.get('title', 'Select a visual clip in Mixworks'))
            self.status.configure(text=f"{state.get('position', 0):.2f} s  /  {state.get('bpm', 0):.1f} BPM  /  {state.get('status', '')}")
            self.state = state
            image = self.folder / 'preview.jpg'
            if image.is_file() and image.stat().st_mtime_ns != self.preview_stamp:
                with Image.open(image) as frame:
                    self.frame = frame.copy()
                self.preview_stamp = image.stat().st_mtime_ns
                self.present()
        except (OSError, ValueError, KeyError):
            # Native atomic swaps can briefly deny reads on Windows. Keep the
            # current frame and status unless the connection remains unavailable.
            if time.monotonic() - self.last_contact > 5:
                self.status.configure(text='Waiting for Mixworks...')
        self.root.after(33, self.tick)

    def close(self):
        self.closed = True
        self.root.destroy()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--alloy-session', type=Path, required=True)
    args = parser.parse_args()
    root = tk.Tk()
    TimelineStudio(root, args.alloy_session)
    root.mainloop()
    return 0
