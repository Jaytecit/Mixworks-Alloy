"""Bounded audio sources for file playback and the local Alloy master feed."""

from collections import deque
from dataclasses import dataclass
from pathlib import Path
from queue import Empty, Full, Queue
import socket
from struct import Struct
from threading import Event, Thread
import time

import numpy as np


BLOCK = 1024
STUDIO_PORT = 49731
PACKET = Struct("<4sIII")
CLOCK = Struct("<fff")
SIGNAL_TIMEOUT = .45


@dataclass(frozen=True)
class AudioFile:
    path: Path
    frames: int
    channels: int
    rate: int

    @classmethod
    def open(cls, path):
        import soundfile

        if path.suffix.lower() == ".mixworks":
            raise ValueError("Open the mix session in Alloy and select Alloy live in Visual Studio.")
        with soundfile.SoundFile(path) as source:
            if source.frames < 1 or source.channels not in (1, 2) or source.samplerate <= 0:
                raise ValueError("Choose a non-empty mono or stereo audio file.")
            return cls(path.resolve(), source.frames, source.channels, source.samplerate)


class FilePlayback:
    def __init__(self, track, position, sounddevice):
        self.track = track
        self.start_frame = self.cursor = position
        self.ready = Queue(maxsize=16)
        self.history = deque(maxlen=128)
        self.cancelled, self.primed, self.decoded = Event(), Event(), Event()
        self.error = None
        self.stream = None
        self.sounddevice = sounddevice
        self.worker = Thread(target=self.decode, name="Studio mix decoder", daemon=True)

    def decode(self):
        import soundfile

        try:
            with soundfile.SoundFile(self.track.path) as source:
                if (source.frames, source.channels, source.samplerate) != (
                        self.track.frames, self.track.channels, self.track.rate):
                    raise ValueError("The audio file changed. Open it again.")
                source.seek(self.start_frame)
                remaining = self.track.frames - self.start_frame
                while remaining and not self.cancelled.is_set():
                    block = source.read(min(BLOCK, remaining), dtype="float32", always_2d=True)
                    if not len(block):
                        raise ValueError("The audio file ended before its reported duration.")
                    while not self.cancelled.is_set():
                        try:
                            self.ready.put(block, timeout=.05)
                            if self.ready.qsize() >= 4:
                                self.primed.set()
                            break
                        except Full:
                            continue
                    remaining -= len(block)
        except Exception as error:
            self.error = str(error)
        finally:
            self.decoded.set()
            self.primed.set()

    def start(self):
        self.worker.start()
        try:
            if not self.primed.wait(2.):
                raise ValueError("Audio did not read in time. Check that the file is available and try again.")
            if self.error and self.ready.empty():
                raise ValueError(self.error)
            self.stream = self.sounddevice.OutputStream(samplerate=self.track.rate, channels=self.track.channels,
                                                        dtype="float32", blocksize=BLOCK, callback=self.submit)
            self.stream.start()
        except Exception:
            self.close()
            raise

    def submit(self, out, frames, timing, status):
        # Decoder I/O runs in the producer. The device callback takes a ready
        # block immediately, or emits silence while the next block arrives.
        if frames != BLOCK:
            out.fill(0)
            self.error = "The output device returned an unexpected audio buffer size."
            raise self.sounddevice.CallbackAbort()
        try:
            block = self.ready.get_nowait()
        except Empty:
            out.fill(0)
            if self.decoded.is_set() and self.ready.empty():
                raise self.sounddevice.CallbackStop()
            return
        count = len(block)
        out[:count] = block
        out[count:] = 0
        self.history.append((timing.outputBufferDacTime, self.cursor, block))
        self.cursor += count
        if self.cursor == self.track.frames:
            raise self.sounddevice.CallbackStop()

    def audible_cursor(self):
        now = self.stream.time
        for dac_time, start, block in reversed(self.history.copy()):
            if dac_time <= now:
                return start + min(len(block), round((now - dac_time) * self.track.rate))
        return self.start_frame

    def samples(self, offsets):
        wanted = self.stream.time + offsets
        result = np.zeros(len(offsets), dtype=np.float32)
        for dac_time, _, block in reversed(self.history.copy()):
            end = dac_time + len(block) / self.track.rate
            if end <= wanted[0]:
                break
            if dac_time > wanted[-1]:
                continue
            start, stop = np.searchsorted(wanted, (dac_time, end))
            positions = (wanted[start:stop] - dac_time) * self.track.rate
            result[start:stop] = np.interp(positions, np.arange(len(block)), block.mean(axis=1))
        return result

    def close(self, abort=True):
        self.cancelled.set()
        try:
            if self.stream:
                try:
                    if abort:
                        self.stream.abort()
                finally:
                    self.stream.close()
                    self.stream = None
        finally:
            self.worker.join(.15)


class AlloyLive:
    def __init__(self, port=STUDIO_PORT):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            if hasattr(socket, "SO_EXCLUSIVEADDRUSE"):
                self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 256 * 1024)
            self.socket.bind(("127.0.0.1", port))
            self.socket.setblocking(False)
        except OSError:
            self.socket.close()
            raise
        self.last_packet = float("-inf")
        self.sender = self.sequence = None
        self.rate = 0
        self.detected_bpm = self.mix_bpm = self.beat_phase = 0.
        self.buffer = np.zeros(0, dtype=np.float32)

    @property
    def active(self):
        return time.monotonic() - self.last_packet < SIGNAL_TIMEOUT

    def poll(self, duration):
        for _ in range(128):
            try:
                packet, sender = self.socket.recvfrom(PACKET.size + CLOCK.size + BLOCK * 4 + 1)
            except BlockingIOError:
                break
            except OSError as error:
                if getattr(error, "winerror", None) == 10040:  # Oversized datagram on Windows.
                    continue
                raise
            if len(packet) < PACKET.size or (self.active and self.sender != sender):
                continue
            magic, sequence, rate, count = PACKET.unpack_from(packet)
            header = PACKET.size + (CLOCK.size if magic == b"MWV2" else 0)
            if (magic not in (b"MWV1", b"MWV2") or not 4000 <= rate <= 384000 or not 1 <= count <= BLOCK
                    or len(packet) != header + count * 4):
                continue
            detected, mix, phase = CLOCK.unpack_from(packet, PACKET.size) if magic == b"MWV2" else (0., 0., 0.)
            if not (0. <= detected <= 655.35 and 0. <= mix <= 655.35 and 0. <= phase < 1.):
                continue
            contiguous = self.sequence is not None and sequence == ((self.sequence + 1) & 0xFFFFFFFF)
            if not self.active or self.rate != rate or not contiguous:
                self.buffer = np.zeros(int(np.ceil(duration * rate)) + 1, dtype=np.float32)
            pcm = np.frombuffer(packet, dtype="<f4", offset=header)
            count = min(count, len(self.buffer))
            if count < len(self.buffer):
                self.buffer[:-count] = self.buffer[count:]
            self.buffer[-count:] = np.nan_to_num(pcm[-count:], nan=0., posinf=0., neginf=0.)
            self.last_packet, self.sender, self.sequence, self.rate = time.monotonic(), sender, sequence, rate
            self.detected_bpm, self.mix_bpm, self.beat_phase = detected, mix, phase

    def clock(self):
        if not self.active:
            return 0., 0., 0.
        elapsed = min(.08, time.monotonic() - self.last_packet)
        return self.detected_bpm, self.mix_bpm, (self.beat_phase + elapsed * self.mix_bpm / 60.) % 1.

    def samples(self, offsets):
        if not self.active:
            return np.zeros(len(offsets), dtype=np.float32)
        return np.interp(offsets * self.rate, np.arange(-len(self.buffer), 0), self.buffer)

    def close(self):
        self.socket.close()
