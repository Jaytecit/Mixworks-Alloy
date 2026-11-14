import mmap
import os
from pathlib import Path
import sys
from tempfile import TemporaryDirectory
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "visual-studio"))
from timeline import AlloySession, PREVIEW_HEADER, PREVIEW_SEQUENCE, PREVIEW_SLOTS, PREVIEW_STRIDE


class StudioPreviewTests(unittest.TestCase):
    def test_audio_port_is_republished_only_when_it_changes(self):
        with TemporaryDirectory() as temporary:
            folder = Path(temporary)
            link = AlloySession(folder)
            link.audio_port(55237)
            with patch("timeline.os.replace", side_effect=AssertionError("duplicate publication")):
                link.audio_port(55237)
            link.audio_port(0)
            self.assertEqual((folder / "audio-port").read_text(encoding="ascii"), "0")

    @unittest.skipUnless(sys.platform == "win32", "Windows file sharing")
    def test_audio_port_retries_a_locked_destination(self):
        with TemporaryDirectory() as temporary:
            folder = Path(temporary)
            link = AlloySession(folder)
            link.audio_port(55237)
            replace = os.replace
            def replace_after_lock(*args):
                if publish.call_count == 1:
                    raise PermissionError(13, "locked")
                replace(*args)

            with patch("timeline.os.replace", side_effect=replace_after_lock) as publish:
                link.audio_port(60061)
                self.assertEqual(publish.call_count, 2)
            self.assertEqual((folder / "audio-port").read_text(encoding="ascii"), "60061")

    def test_shared_frames_are_ordered_and_decoded_bottom_up(self):
        with TemporaryDirectory() as temporary:
            folder = Path(temporary)
            link = AlloySession(folder)
            self.assertIsNone(link.preview())
            path = folder / "preview.frames"
            with path.open("w+b") as file:
                file.truncate(PREVIEW_SEQUENCE.size + PREVIEW_SLOTS * PREVIEW_STRIDE)
                with mmap.mmap(file.fileno(), 0) as writer:
                    def publish(sequence, width=1, height=2):
                        slot = PREVIEW_SEQUENCE.size + sequence % PREVIEW_SLOTS * PREVIEW_STRIDE
                        PREVIEW_HEADER.pack_into(writer, slot, sequence, width, height)
                        offset = slot + PREVIEW_HEADER.size
                        writer[offset:offset + 8] = bytes([255, 0, 0, 255, 0, 0, 255, 255])
                        PREVIEW_SEQUENCE.pack_into(writer, 0, sequence)
                        return slot

                    try:
                        publish(1)
                        publish(2)
                        self.assertEqual(link.latest_preview_sequence(), 2)
                        image = link.preview(1)
                        self.assertEqual(list(image.getdata()), [(255, 0, 0), (0, 0, 255)])
                        self.assertIsNone(link.preview(1))
                        self.assertEqual(link.preview().size, (1, 2))
                        slot = publish(3)
                        PREVIEW_SEQUENCE.pack_into(writer, slot, 0)
                        self.assertIsNone(link.preview(3))
                        publish(9, 0, 0)
                        self.assertIsNone(link.preview(2))
                        self.assertIsNone(link.preview())
                    finally:
                        link.close_preview()


if __name__ == "__main__":
    unittest.main()
