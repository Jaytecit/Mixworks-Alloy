"""Measure production-rendered rhythm fixtures; requires NumPy, SciPy and SoundFile.

This is a timing diagnostic, not a musical downbeat labeller or listening test.
"""
import argparse, json
from pathlib import Path
import numpy as np
from scipy import signal,ndimage
import soundfile as sf

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('output_directory', type=Path, help='Folder from --self-test --real-tracks-only')
root=parser.parse_args().output_directory
meta=json.loads((root/'measurements-input.json').read_text())
hz=1000
def envelope(path):
    y,rate=sf.read(path,dtype='float32',always_2d=True)
    y=signal.resample_poly(y.mean(axis=1),1,round(rate/2000))
    actual=rate/round(rate/2000)
    y=signal.sosfiltfilt(signal.butter(3,[40,180],btype='band',fs=actual,output='sos'),y)
    # Smooth several bass cycles; individual waveform cycles are not extra beats.
    e=np.sqrt(np.maximum(0,ndimage.gaussian_filter1d(y*y,actual*.012)))
    return np.interp(np.arange(int(len(y)/actual*hz))/hz,np.arange(len(y))/actual,e)

def stats(x):
    return {'count':len(x),'medianMs':float(np.median(x)),'p95AbsoluteMs':float(np.quantile(np.abs(x),.95)),
            'maxAbsoluteMs':float(np.max(np.abs(x)))} if len(x) else {'count':0}

source=[envelope(root/s['decodedWav']) for s in meta['sources']]
result=[]
for scenario in meta['scenarios']:
    rows=[]
    for clip in scenario['clips']:
        track=clip['track'];src=source[track];dst=envelope(root/clip['wav'])
        s=np.array([b['source'] for b in clip['predictedBeats']]); t=np.array([b['local'] for b in clip['predictedBeats']])
        shifts=[];correlations=[];at=[];examined=0
        for center in t[2:-2]:
            # Match the complete bass-envelope shape under the expected time warp,
            # independently of peak picking. Search both earlier and later output.
            times=center+np.arange(-90,151)/hz
            template=np.interp(np.interp(times,t,s)*hz,np.arange(len(src)),src)
            if np.std(template)<.006 or np.max(template)<.025:continue
            segment=np.interp((center+np.arange(-140,201)/hz)*hz,np.arange(len(dst)),dst)
            n=len(template);template-=template.mean()
            dot=signal.correlate(segment,template,mode='valid',method='direct')
            sums=np.convolve(segment,np.ones(n),mode='valid')
            sq=np.convolve(segment**2,np.ones(n),mode='valid')-sums*sums/n
            corr=dot/np.sqrt(np.maximum(sq,1e-16)*np.sum(template**2))
            best=int(np.argmax(corr));examined+=1
            if corr[best]<.95 or best in (0,len(corr)-1):continue
            shifts.append(best-50);correlations.append(float(corr[best]));at.append(float(center))
        row={'track':track,'examinedDistinctEnvelopes':examined,'highCorrelationMatches':len(shifts),
             'displacementMs':stats(np.array(shifts)), 'medianCorrelation':float(np.median(correlations)) if correlations else None,
             'localTimes':at,'shiftMs':shifts}
        rows.append(row);print(scenario['name'],json.dumps({k:v for k,v in row.items() if k not in ('localTimes','shiftMs')}),flush=True)
    result.append({'name':scenario['name'],'tracks':rows})
(root/'envelope-transfer.json').write_text(json.dumps({'method':'Zero-phase 40-180 Hz energy envelope, Gaussian smoothing 12 ms, time-warped 241 ms source templates, +/-50 ms lag search, normalized correlation >=0.95; weak/ambiguous envelopes excluded with explicit counts. This measures stretch timing, not musical downbeat accuracy.',
    'sourceSha256':[s['sha256'] for s in meta['sources']], 'applicationVersion':meta['applicationVersion'], 'scenarios':result},indent=2,allow_nan=False))
