"""Local separation measurements. Retains named research audio for later listening.

Predeclared acceptance: exact source rate/channels/frames and all finite; summed
stem lag <= 1 sample at start/middle/end; real-music residual RMS <= 0.15 of input.
These are alignment/recombination bounds, not a substitute for listening or SDR
against independently recorded reference stems. No source audio is uploaded.
"""
import argparse
import json
from pathlib import Path
import subprocess
import time

import numpy as np
import psutil
import soundfile as sf
from stem_worker import digest, write_json, NAMES


def measure(folder):
    original, rate = sf.read(folder/'input.wav', dtype='float32', always_2d=True)
    output = folder/'assets'
    manifest = json.loads((output/'manifest.json').read_text())
    summed = np.zeros_like(original)
    energy = {}
    for name in NAMES:
        audio, sr = sf.read(output/(name+'.wav'), dtype='float32', always_2d=True)
        assert sr == rate and audio.shape == original.shape and np.isfinite(audio).all()
        summed += audio
        energy[name] = float(np.sqrt(np.mean(audio.astype('float64')**2)))
    residual = original - summed
    rms = float(np.sqrt(np.mean(original.astype('float64')**2)))
    error = float(np.sqrt(np.mean(residual.astype('float64')**2)))
    lags = []
    if rms > 1e-7:
        for start in (0, max(0,len(original)//2-rate), max(0,len(original)-2*rate)):
            # Pick the energetic channel: mono averaging could erase antiphase audio.
            ch = int(np.argmax(np.mean(original[start:start+rate]**2,axis=0)))
            a = original[start:start+rate,ch].astype('float64')
            b = summed[start:start+rate,ch].astype('float64')
            scores = [(lag, float(np.dot(a[64:-64], b[64+lag:len(b)-64+lag]))) for lag in range(-32,33)]
            lags.append(max(scores,key=lambda x:x[1])[0])
    ratio = error/max(rms, 1e-12)
    return dict(durationSeconds=len(original)/rate, frames=len(original), sampleRate=rate,
                channels=original.shape[1], residualRatio=ratio,
                residualDb=20*np.log10(max(ratio,1e-12)), lagsSamples=lags, stemRms=energy,
                finiteExactClock=True, alignmentPass=all(abs(x)<=1 for x in lags),
                residualPass=ratio<=.15 if rms>1e-7 else error<1e-7,
                residualRms=error, outputBytes=sum(p.stat().st_size for p in output.glob('*.wav')),
                inferenceSeconds=manifest['elapsedSeconds'], device=manifest['device'],
                peakVramBytes=manifest['peakVramBytes'])


def run_case(root, name, audio, rate, config, device='auto'):
    folder = root/name; folder.mkdir()
    sf.write(folder/'input.wav',audio,rate,subtype='FLOAT')
    write_json(folder/'job.json',dict(runtime=str(config.resolve()), cache=str((root/'cache').resolve()),
                                    sourceSha256=digest(folder/'input.wav'), device=device))
    settings = json.loads(config.read_text())
    started=time.monotonic(); peak=0
    with (folder/'worker.log').open('w') as log:
        child=subprocess.Popen([settings['python'],settings['worker'],'--job',str((folder/'job.json').resolve())],stdout=log,stderr=log)
        process=psutil.Process(child.pid)
        while child.poll() is None:
            try:
                peak=max(peak,process.memory_info().rss + sum(p.memory_info().rss for p in process.children(recursive=True)))
            except psutil.Error: pass
            time.sleep(.2)
    result=dict(exit=child.returncode, wallSeconds=time.monotonic()-started, peakRssBytes=peak)
    if child.returncode==0: result.update(measure(folder))
    else: result['status']=json.loads((folder/'status.json').read_text())
    write_json(folder/'measurement.json',result)
    print(name,json.dumps(result),flush=True)
    return result


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--config',required=True,type=Path);p.add_argument('--output',required=True,type=Path)
    p.add_argument('--source',action='append',default=[],type=Path)
    args=p.parse_args();args.output.mkdir(exist_ok=False)
    write_json(args.output/'bounds.json',dict(exactClock=True,maxLagSamples=1,maxResidualRatio=.15,
               retention='Research inputs/stems retained for subsequent alignment inspection and user listening. Originals unchanged.'))
    results={}
    for i,source in enumerate(args.source):
        before=digest(source); audio,rate=sf.read(source,dtype='float32',always_2d=True)
        # First source is the long/full track; remaining sources use a middle excerpt.
        sample=audio if i==0 else audio[rate*60:rate*105]
        name='real-'+str(i)
        results[name]=run_case(args.output,name,sample,rate,args.config)
        results[name].update(source=str(source),sourceSha256=before)
        assert digest(source)==before
        write_json(args.output/'summary.json',results)
        if i==1:
            results['cpu']=run_case(args.output,'cpu',sample[:rate*12],rate,args.config,'cpu')
        if i==2:
            results['quiet-real']=run_case(args.output,'quiet-real',sample*.001,rate,args.config)
    rate=48000; length=rate*24; t=np.arange(length)/rate
    pulse=np.zeros(length,dtype='float32')
    for sec in (1.0,7.8,11.7,20.0,23.5):pulse[round(sec*rate)]=.7
    results['impulse48']=run_case(args.output,'impulse48',np.stack([pulse,pulse],axis=1),rate,args.config)
    tone=(.15*np.sin(2*np.pi*80*t)+.05*np.sin(2*np.pi*440*t)).astype('float32')
    results['antiphase48']=run_case(args.output,'antiphase48',np.stack([tone,-tone],axis=1),rate,args.config)
    results['silence-mono']=run_case(args.output,'silence-mono',np.zeros((rate*11,1),dtype='float32'),rate,args.config)
    write_json(args.output/'summary.json',results)


if __name__=='__main__':main()
