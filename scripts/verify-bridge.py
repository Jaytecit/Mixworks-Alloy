"""Run the focused native bridge suite and a real isolated-app smoke scenario."""
import argparse
from array import array
import json
import math
from pathlib import Path
import time
import uuid
import wave

from alloy_bridge import WORKSPACE, BridgeError, Client, initialise, launch
from test_artifacts import safe_root
from verification import bundle_identity, run_checks, write_json


def make_audio(path, frequency):
    rate = 24000
    samples = array('h')
    for i in range(rate*24):
        t = i/rate
        pulse = math.exp(-35*(t % .5))
        sample = int(1600*pulse*math.sin(2*math.pi*frequency*t))
        samples.extend((sample, sample))
    with wave.open(str(path), 'wb') as output:
        output.setparams((2, 2, rate, 0, 'NONE', 'not compressed'))
        output.writeframes(samples.tobytes())


class Smoke:
    def run(self, exe, flags, folder, environment, log, startup, deadline):
        rows, process, client = [], None, None
        def check(ok, label):
            if not ok:
                raise AssertionError(label)
            rows.append('PASS '+label)
        def remaining():
            seconds = deadline-time.monotonic()
            if seconds <= 0:
                raise TimeoutError('Interactive smoke deadline exceeded')
            return seconds
        def call(command, **args):
            reply = client.call(command, timeout=min(15, remaining()), **args)
            if command in ('seek', 'timeline_cue', 'jump_bars'):
                reply['state'] = client.ready(min(30, remaining()))
                check(not reply['state'].get('seekError'), 'prepared seek completes without error: '+command)
            return reply
        try:
            root = initialise(folder/'w')
            process = launch(root, exe, environment=environment, log=log, startup=startup)
            client = Client(root)
            state = call('status')['state']
            check(state['pid'] == process.pid and state['protocolVersion'] == 1, 'live process and protocol identity')
            check(state['clips'] == [] and state['library'] == [], 'fresh isolated session is empty')
            check(all(command in state['capabilities'] for command in ('save', 'open', 'close', 'control')), 'capabilities advertised')
            for index, frequency in enumerate((220, 330)):
                source = root/f'audio/fixture-{index}.wav'
                make_audio(source, frequency)
                call('import', path=str(source))
                client.ready(min(90, remaining()))
                call('correct_source', index=index, bpm=120, downbeat=0)
                call('add', index=index)
                state = client.ready(min(90, remaining()))
            check(len(state['library']) == 2 and len(state['clips']) == 2, 'real imports and timeline additions')
            # Ordinary Add deliberately leaves tracks independent. Declare the
            # transition in this saved fixture before testing its treatment.
            fixture = Path(call('save', name='independent-tracks')['path'])
            document = json.loads(fixture.read_text(encoding='utf-8-sig'))
            document['clips'][1]['start'] = 12
            # The controlled preset fixture needs aligned OUT/IN anchors, not
            # detector-dependent defaults from the imported source's sections.
            document['clips'][0]['outroAnchor'] = 12
            document['clips'][0]['fadeOutAnchor'] = 12
            document['clips'][1]['introAnchor'] = 0
            document['clips'][1]['fadeInAnchor'] = 0
            document['clips'][1]['hotcues'] = [dict(id='bridge-live-cue', sourceSeconds=18, manual=True)]+[None]*8
            document['transitionLinks'] = [dict(outgoing=document['clips'][0]['id'],
                incoming=document['clips'][1]['id'], frame=[12, 24], locked=False, movementLocked=False, muteOutside=False,
                filter=dict(mode='none', strength=0), alternatives=[None, None, None])]
            write_json(root/'sessions/linked-fixture.mixworks', document)
            call('open', name='linked-fixture')
            linked = client.ready(min(90, remaining()))
            check(abs(linked['clips'][1]['start']-12)<.001, 'explicit linked fixture opened')
            call('view', start=0, seconds=40, monitorMuted=True)
            call('select_transition', index=1)
            call('seek', seconds=13)
            call('play', enabled=True)
            state = call('transition_preset', index=2)['state']
            check(bool(state['selectedTransition']) and state['playing'] and 13 <= state['position'] < 15,
                  'actual Arrange preset publication keeps programme playback and clock running')
            state = call('timeline_cue', slot=0)['state']
            check(state['playing'] and 30 <= state['position'] < 31, 'actual running cue jumps the whole mix to the incoming track passage')
            call('stop')
            call('work_mode', name='record-perform')
            state = client.ready(min(15, remaining()))
            check(state['workMode']=='record-perform', 'actual mode menu selects Record/Perform')
            call('seek', seconds=13)
            call('play', enabled=True)
            state = call('transition_preset', index=3)['state']
            check(not state['busy'] and not state.get('transitionQuestion') and 'updated' in state['status'],
                  'replacing the owned preset succeeds without an authored-curve confirmation')
            state = client.ready(min(15, remaining()))
            check(state['playing'] and state['capturingTake'] and 13 <= state['position'] < 15,
                  'actual Record/Perform preset publication preserves its running editable take and clock')
            state = call('timeline_cue', slot=0)['state']
            check(state['playing'] and state['capturingTake'] and 30 <= state['position'] < 31,
                  'actual cue seek preserves active editable take capture')
            call('screenshot', name='live-transition-and-cue')
            state = call('stop')['state']
            check(not state['capturingTake'] and state['takeCount']>=1, 'actual Stop finalises the cue/transition take')
            replaced = Path(call('save', name='crossfade-replacement')['path'])
            replacement = json.loads(replaced.read_text(encoding='utf-8-sig'))
            # Finalising returns the arrangement to the pre-take programme.
            # The performed replacement belongs to the take's project event.
            snapshots = [event['payload'] for event in replacement['performanceTakes'][-1]['events']
                         if isinstance(event.get('payload'),dict) and event['payload'].get('clips')]
            check(bool(snapshots), 'finalised take retains its actual performed transition snapshot')
            curves = [clip['controls']['automation'] for clip in snapshots[-1]['clips'][:2]]
            write_json(root/'evidence/replacement-curves.json', dict(preset='Crossfade', scope='performed-take', curves=curves))
            check(all(abs(point['value'])<1e-6 for side, source_range in enumerate(((12,24),(0,12)))
                      for point in curves[side]['bass']['points'] if source_range[0] < point['time'] < source_range[1]),
                  'replacement Crossfade actually clears the previous bass swap inside its frame')
            call('work_mode', name='arrange')
            state = client.ready(min(15, remaining()))
            check(state['workMode']=='arrange', 'actual mode menu returns to Arrange after finalising')
            state = call('timeline_cue', slot=0)['state']
            check(not state['playing'] and abs(state['position']-30)<.001, 'stopped actual cue jumps without starting audio')
            call('seek', seconds=0)
            call('panel', name='controller')
            state = call('control', lane=0, parameter='pan', value=.25)['state']
            check(abs(state['lanes'][0]['pan']-.25)<.001, 'state reflects actual lane control')
            state = call('undo')['state']
            check(abs(state['lanes'][0]['pan'])<.001, 'real Undo restores prior pan')
            call('control', lane=0, parameter='cutoff', value=-.4)
            try:
                call('close')
                raise AssertionError('dirty close was incorrectly accepted')
            except BridgeError as error:
                check('Save' in str(error), 'dirty session blocks graceful close')
            saved = call('save', name='before')
            check(Path(saved['path']).is_file(), 'real session saved')
            try:
                call('save', name='before')
                raise AssertionError('session overwrite was incorrectly accepted')
            except BridgeError as error:
                check('preserved' in str(error), 'existing session cannot be overwritten')
            call('control', lane=0, parameter='pan', value=.5)
            call('save', name='after')
            call('open', name='before')
            state = client.ready(min(90, remaining()))
            check(abs(state['lanes'][0]['pan'])<.001 and abs(state['lanes'][0]['cutoff']+.4)<.001,
                  'saved control state survives real reopen')
            for lane in (0, 1):
                call('control', lane=lane, parameter='volume', value=-60)
            call('seek', seconds=1)
            started_audio = call('play', enabled=True)['state']['audioCallbacks']
            time.sleep(.25)
            state = call('status')['state']
            check(state['playing'] and state['position']>1, 'real muted transport advances')
            audio = state['audioCallbacks']
            check(audio['callbacks'] > started_audio['callbacks'] and audio['totalMs'] >= started_audio['totalMs']
                  and all(audio[key] >= started_audio[key] for key in ('overBudget', 'lockMisses', 'oversized', 'exportSilence'))
                  and audio['peakMs'] >= 0, 'read-only callback diagnostics advance with actual device playback')
            call('stop')
            state = call('view', start=0, seconds=40, monitorMuted=True)['state']
            check(state['monitorMuted'], 'actual monitor mute silences isolated audition verification')
            preview_cursor = state['position']
            source_id = state['library'][0]['id']
            call('preview', index=0, fraction=.5)
            until = time.monotonic()+min(15, remaining())
            while time.monotonic()<until:
                state = call('status')['state']
                if state['preview']['sourceId'] == source_id and not state['preview']['loading']:
                    break
                time.sleep(.05)
            check(state['preview']['sourceId']==source_id and state['preview']['duration']>0
                  and .45 < state['preview']['fraction'] < .8, 'real owned-source audition starts inside decoded file')
            call('preview', index=0, fraction=.2)
            state = call('status')['state']
            check(state['preview']['sourceId']==source_id and .15 < state['preview']['fraction'] < .4
                  and state['position']==preview_cursor, 'same-source seek updates audition while programme cursor stays fixed')
            state = call('preview', stop=True)['state']
            check(not state['preview']['sourceId'], 'actual preview Stop returns to idle')
            image = Path(call('screenshot', name='bridge-smoke')['path'])
            check(image.read_bytes().startswith(b'\x89PNG\r\n\x1a\n'), 'real component screenshot saved')
            # Exercise the inbox directly: no client-side archiving can mask a native regression.
            ident = uuid.uuid4().hex
            request = root/f'control/requests/{ident}.json'
            reply = root/f'control/responses/{ident}.json'
            payload = dict(id=ident, token='wrong-token', timestamp=time.time()*1000, command='status')
            write_json(request, payload)
            until = time.monotonic()+min(5, remaining())
            while not reply.exists() and time.monotonic()<until:
                time.sleep(.03)
            result = json.loads(reply.read_text())
            check(result['ok'] is False and result['id'] == ident, 'file-protocol authentication rejected')
            original = reply.read_bytes()
            payload['token'] = client.token
            write_json(request, payload)
            call('status')
            time.sleep(.15)
            check(reply.read_bytes() == original, 'replay cannot replace cached rejection or reexecute')
            check(not list((root/'control/requests').glob('*.json')), 'server archives completed and replayed requests')
            check(bool(list((root/'control/history').glob('*.json'))), 'request audit history retained')
            call('save', name='smoke-final')
            check(not call('status')['state']['dirty'], 'saved session reports clean state')
            call('close')
            check(process.wait(timeout=min(10, remaining())) == 0, 'graceful close exits the owned app')
            process = launch(root, exe, environment=environment, log=log, startup=startup)
            state = call('status')['state']
            check(state['pid'] == process.pid and not state['clips'], 'same frozen workspace restarts with explicit session loading')
            call('open', name='smoke-final')
            state = client.ready(min(90, remaining()))
            check(len(state['clips']) == 2 and abs(state['lanes'][0]['cutoff']+.4)<.001,
                  'saved isolated session survives process restart')
            call('close')
            check(process.wait(timeout=min(10, remaining())) == 0, 'restarted app closes cleanly')
        except Exception as error:
            rows.append(f'FAIL {type(error).__name__}: {error}')
            print(rows[-1], file=log, flush=True)
        finally:
            if process is not None and process.poll() is None and client is not None:
                try:
                    final = client.call('status', timeout=2)['state']
                    if not final['busy'] and not final['dirty'] and not final['playing']:
                        client.call('close', timeout=2)
                        process.wait(timeout=3)
                except Exception:
                    pass  # Keep the original failure; fallback owns only this child.
            if process is not None and process.poll() is None:
                # Test failure cleanup is restricted to the child this runner just created.
                process.terminate()
                process.wait(timeout=10)
                rows.append('FAIL owned app required termination; graceful completion not established')
            failures = sum(row.startswith('FAIL ') for row in rows)
            (folder/'checks.txt').write_text('\n'.join(rows+[f'{len(rows)} checks; {failures} failures'])+'\n', encoding='utf-8')
        return int(bool(failures))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output = safe_root(args.output, WORKSPACE)
    args.output.mkdir(parents=True, exist_ok=False)
    identity = bundle_identity(args.exe)
    results = []
    results.append(dict(name='bridge-native', **run_checks(args.exe.resolve(), ['--promo-control-only'], args.output/'native')))
    results.append(dict(name='bridge-live', **run_checks(args.exe.resolve(), ['--dev-control'], args.output/'live',
                                                        driver=Smoke(), timeout=240)))
    errors = [] if bundle_identity(args.exe) == identity else ['Bundle changed during verification']
    failed = bool(errors or any(r['exit'] or r.get('artifactRetention', {}).get('errors') for r in results))
    write_json(args.output/'summary.json', dict(format='alloy-bridge-verification', schemaVersion=1,
               scope='bridge-integration', **identity, suites=results, errors=errors, passed=not failed))
    for result in results:
        print(f"{result['name']}: {result['summary']}; exit {result['exit']}")
    return int(failed)


if __name__ == '__main__':
    raise SystemExit(main())
