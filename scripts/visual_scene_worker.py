"""Owned Alloy scene renderer. No audio device, external service or native app control.

One authenticated local connection; bounded length-prefixed JSON/BGRA replies.
All OpenGL and script resources belong to this process's main thread.
"""
import argparse
import base64
import json
import math
from pathlib import Path
import socket
import struct
import sys
import select
import time

PROTOCOL = 6
PIXEL_FORMAT = 'bgra8-bottom-up'
MAX_REQUEST = 65536  # A frame carries at most 2048 float32 samples plus transport facts.


def receive(stream):
    line = stream.readline(MAX_REQUEST + 1)
    if not line:
        return None
    if len(line) > MAX_REQUEST or not line.endswith(b'\n'):
        raise ValueError('Oversized visual frame request')
    return json.loads(line)


def send(connection, metadata, image=b''):
    body = json.dumps(dict(protocol=PROTOCOL, **metadata), allow_nan=False).encode()
    connection.sendall(struct.pack('!II', len(body), len(image)) + body + image)


def decode_frame(pixels, width, height):
    """Artifact/test consumer for the native wire's exact byte/row contract."""
    from PIL import Image
    return Image.frombytes('RGBA', (width, height), pixels, 'raw', 'BGRA', 0, -1).convert('RGB')


def main():
    sys.path.insert(0,str(Path(__file__).resolve().parent))
    parser = argparse.ArgumentParser()
    parser.add_argument('--renderer-root', type=Path, required=True)
    parser.add_argument('--install', type=Path, required=True)
    parser.add_argument('--ready', type=Path, required=True)
    parser.add_argument('--token', required=True)
    parser.add_argument('--width', type=int, default=1280)
    parser.add_argument('--height', type=int, default=720)
    parser.add_argument('--owner-pid', type=int, default=0)
    parser.add_argument('--original', action='store_true')
    parser.add_argument('--milkdrop-library', type=Path)
    parser.add_argument('--sprite-folder', type=Path)
    parser.add_argument('--studio-mailbox', type=Path)
    args = parser.parse_args()
    # Bind locally before loading OpenGL; parent sees readiness or a startup error.
    with socket.socket() as server:
        server.bind(('127.0.0.1', 0))
        server.listen(1)
        server.settimeout(20)
        args.ready.write_text(json.dumps(dict(port=server.getsockname()[1])), encoding='utf-8')
        connection, _ = server.accept()
        with connection:
            connection.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            # A settled paused preview intentionally sends no pixel requests.
            # Keep that owned connection alive until EOF/parent cleanup; the
            # native caller separately bounds response reads and reaps us.
            connection.settimeout(None)
            player = None
            studio_mode = False
            failed_identity = None
            last_error = None
            pending_preload = None
            try:
                with connection.makefile('rb') as stream:
                    while True:
                        if player and studio_mode:
                            player.poll()
                        elif player and player.presenter:
                            player.presenter.poll()
                        if not select.select([connection], [], [], .02)[0]:
                            if player:
                                player.prepare(pending_preload)
                            continue
                        request = receive(stream)
                        if request is None:
                            break
                        if request.get('token') != args.token:
                            raise ValueError('Wrong renderer owner')
                        if request.get('stop'):
                            break
                        requested_studio = "studioLayers" in request
                        pending_preload = request.get('preload')
                        if player and requested_studio != studio_mode:
                            player.close()
                            player = None
                        identity = tuple((a.get('clip'), a.get('scene'), a.get('revision'), json.dumps(a.get('controls', {}), sort_keys=True))
                                         for a in request.get('scenes', []) if isinstance(a, dict)) + (request.get('generation'), request.get('image'), request.get('transition'), json.dumps(request.get('views', []), sort_keys=True))
                        if requested_studio:
                            identity += (json.dumps(request["studioLayers"], sort_keys=True), request.get('studioOutput'))
                        if identity == failed_identity:
                            send(connection, dict(ok=False, error=last_error))
                            continue
                        try:
                            if player is None:
                                if requested_studio:
                                    from studio_scene_player import StudioScenePlayer
                                    player = StudioScenePlayer(args.install, (args.width, args.height), args.milkdrop_library,
                                                               args.studio_mailbox)
                                else:
                                    from original_scene_player import OriginalScenePlayer
                                    player = OriginalScenePlayer(args.renderer_root, args.install, args.width, args.height, args.owner_pid,
                                                                 milkdrop_library=args.milkdrop_library, sprite_folder=args.sprite_folder)
                                studio_mode = requested_studio
                                # Import/context startup can take seconds. Drop the old
                                # request and let Alloy submit its current clock/PCM.
                                send(connection, dict(ok=True, initialised=True))
                                continue
                            metadata, image = player.render(request)
                            failed_identity = None
                            send(connection, metadata, image)
                            # Deliver the current picture before allocating and
                            # compiling the next pipeline on the owned GL context.
                            player.prepare(request.get('preload'))
                        except Exception as error:
                            if player:
                                player.reset()
                            failed_identity, last_error = identity, str(error)
                            send(connection, dict(ok=False, error=last_error))
            finally:
                if player:
                    player.close()


if __name__ == '__main__':
    main()
