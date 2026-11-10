"""Focused pass-graph safety and independent audio-controller behaviour."""
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest

import numpy as np

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from original_visual_gl import AudioController, pass_descriptors, SCENE_KEYS
from original_scene_player import audio_levels, actor, kick_impact, transport_reset_required


class OriginalVisualTests(unittest.TestCase):
    def test_kick_impacts_follow_audio_timestamps_across_cadences(self):
        kicks=[[.5,1.],[.877,.94],[1.5,1.]]
        self.assertEqual(kick_impact(1.05,[]),0.)
        self.assertEqual(kick_impact(.49,kicks),0.)
        self.assertLess(kick_impact(.55,kicks),.15)
        self.assertAlmostEqual(kick_impact(.587,kicks),1.)
        self.assertAlmostEqual(kick_impact(.964,kicks),.94)
        self.assertEqual(kick_impact(1.21,kicks),0.)
        for fps in (30,60):
            times=np.arange(2*fps)/fps
            impacts=[kick_impact(at,kicks) for at in times]
            starts=sum(value>0 and (i==0 or impacts[i-1]==0) for i,value in enumerate(impacts))
            self.assertEqual(starts,3)
        self.assertLess(kick_impact(.72,kicks),kick_impact(.65,kicks))
        self.assertEqual(kick_impact(.82,kicks),0.)

    def test_kick_input_is_bounded(self):
        for bad in (None,[[0,1]]*4,[[0]],[[float('nan'),1]],[[0,1.1]],[[-1,1]]):
            with self.subTest(bad=bad),self.assertRaises(ValueError):
                kick_impact(1.,bad)

    def test_slow_forward_cadence_keeps_state_while_real_seeks_reset(self):
        self.assertFalse(transport_reset_required(None,4,100.))
        for at in (100.,100.5,102.,130.):
            self.assertFalse(transport_reset_required((4,100.),4,at))
        self.assertTrue(transport_reset_required((4,100.),5,130.))
        self.assertTrue(transport_reset_required((4,100.),5,100.))
        self.assertTrue(transport_reset_required((4,100.),4,99.))

    def test_preset_validation_rejects_invalid_controls_and_duplicate_names(self):
        from visual_catalogue import catalogue
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder); scene=root/'scenes/example'; scene.mkdir(parents=True)
            (root/'original-scenes.json').write_text(json.dumps(dict(schema=1,scenes=[dict(key='original/example')])))
            valid=dict(controls=dict(bassDrive=dict(min=0,max=1,default=.5)),
                presets=[dict(id='silk',title='Silk',controls=dict(bassDrive=.3))])
            def parse(value):
                (scene/'scene.json').write_text(json.dumps(value));return catalogue(root)
            self.assertEqual(parse(valid)['original/example']['presets'][0]['controls']['bassDrive'],.3)
            for values in ({'unknown':.5},{'bassDrive':True},{'bassDrive':float('nan')},{'bassDrive':1.1}):
                invalid=copy.deepcopy(valid);invalid['presets'][0]['controls']=values
                with self.subTest(values=values),self.assertRaises(ValueError):parse(invalid)
            invalid=copy.deepcopy(valid);invalid['presets']*=2
            with self.assertRaises(ValueError):parse(invalid)

    def test_momentum_changes_release_without_reversing_integrated_motion(self):
        quiet=dict(energy=0.,bass=0.,mid=0.,treble=0.); loud=dict.fromkeys(quiet,1.)
        results=[]
        for momentum in (0.,1.):
            c=AudioController();c.update(quiet,0,True,momentum)
            hit=c.update(loud,1/60,False,momentum)
            release=c.update(quiet,.1,False,momentum)
            self.assertGreater(release['bassImpulse'],hit['bassImpulse'])
            self.assertEqual(release,c.update(quiet,0,False,momentum))
            results.append(release)
        self.assertGreater(results[1]['bassHit'],results[0]['bassHit'])

    def test_handoff_conversions_do_not_reinterpret_state_channels(self):
        from visual_handoff import adapter,PARTICLE,ATLAS,FLOW,REACTION,UV
        self.assertEqual(adapter(PARTICLE,{PARTICLE,ATLAS}),'exact')
        self.assertEqual(adapter(FLOW,{PARTICLE,ATLAS}),'image-to-flow')
        self.assertEqual(adapter(REACTION,{FLOW,UV}),'image-to-growth')
        self.assertEqual(adapter(PARTICLE,{REACTION}),'image-to-particles')
        self.assertEqual(adapter(ATLAS,{FLOW}),'image-to-pigment')
        self.assertIsNone(adapter(UV,{PARTICLE}))
        self.assertIsNone(adapter('unknown-state',{}))

    def test_handoff_phase_endpoints_and_pivot(self):
        from visual_handoff import transition_phase
        for pivot in (0.,.1,.5,.9,1.):
            values=[transition_phase(i/100,pivot) for i in range(101)]
            self.assertEqual(values[0],0.);self.assertEqual(values[-1],1.)
            self.assertEqual(values,sorted(values))
            if 0<pivot<1:self.assertAlmostEqual(transition_phase(pivot,pivot),.5)

    def test_particle_display_is_bounded_and_local(self):
        valid=dict(passSchema=2,passes=[dict(id='finish',shader='main.glsl',vertex='vertex.glsl',
                                           instances=1344,scale=1,format='rgba8')])
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            def parse(d):
                (root/'scene.json').write_text(json.dumps(d))
                return pass_descriptors(root)
            self.assertEqual(parse(valid)[0]['instances'],1344)
            for key,value in [('instances',0),('instances',4097),('instances',True),
                              ('vertex','../escape.glsl'),('history',True),('state',True)]:
                d=copy.deepcopy(valid);d['passes'][0][key]=value
                with self.subTest(key=key,value=value),self.assertRaises(ValueError):parse(d)
            d=copy.deepcopy(valid);del d['passes'][0]['vertex']
            with self.assertRaises(ValueError):parse(d)

    def test_catalogue_and_actor_identities(self):
        root=Path(__file__).resolve().parents[1]/'visuals/original'
        catalogue=json.loads((root/'original-scenes.json').read_text())
        self.assertEqual(set(e['key'] for e in catalogue['scenes']),set(SCENE_KEYS))
        for key in SCENE_KEYS:
            self.assertEqual(actor(dict(clip='x',scene=key,revision='r',time=0))['scene'],key)
            passes=pass_descriptors(root/'scenes'/key.split('/')[1])
            self.assertEqual(passes[-1]['format'],'rgba8')
        for key in ('original/../neon-run','original/missing'):
            with self.assertRaises(ValueError): actor(dict(clip='x',scene=key,revision='r',time=0))

    def test_loud_audio_preserves_dynamics(self):
        x=np.arange(2048)/44100
        low,_=audio_levels(.4*np.sin(2*np.pi*80*x),44100)
        high,_=audio_levels(.8*np.sin(2*np.pi*80*x),44100)
        self.assertGreater(high['bass']-low['bass'],.05)
        self.assertGreater(high['energy']-low['energy'],.05)
        self.assertLess(high['bass'],1.)

    def test_band_separation(self):
        x=np.arange(2048)/44100
        for frequency,band in ((80,'bass'),(1000,'mid'),(6000,'treble')):
            levels,_=audio_levels(.1*np.sin(2*np.pi*frequency*x),44100)
            self.assertGreater(levels[band],.2)
            self.assertTrue(all(levels[other]<.03 for other in ('bass','mid','treble') if other!=band))

    def test_smooth_attack_release_onset_and_reset(self):
        quiet=dict(energy=0.,bass=0.,mid=0.,treble=0.)
        loud=dict.fromkeys(quiet,1.)
        c=AudioController(); c.update(quiet,0,True)
        attack=c.update(loud,.05,False)
        self.assertTrue(0.<attack['bass']<1.)
        self.assertEqual(attack['onset'],1.)
        released=c.update(quiet,.05,False)
        self.assertTrue(0.<released['bass']<attack['bass'])
        self.assertGreater(released['bass'],released['treble'])
        self.assertEqual(c.update(loud,0,False)['bass'],released['bass'])
        self.assertEqual(c.update(quiet,0,True),dict(quiet,onset=0.,bassHit=0.,bassTime=0.,midHit=0.,trebleHit=0.,midTime=0.,trebleTime=0.,bassImpulse=0.,midImpulse=0.,trebleImpulse=0.))

    def test_new_band_clocks_are_selective_pause_safe_and_reset(self):
        quiet=dict(energy=.2,bass=0.,mid=0.,treble=0.)
        for band,other in (('mid','treble'),('treble','mid')):
            c=AudioController();c.update(quiet,0,True)
            hit=c.update(dict(quiet,**{band:.8}),.03,False)
            self.assertGreater(hit[band+'Hit'],.8)
            self.assertGreater(hit[band+'Time'],0.)
            self.assertEqual(hit[other+'Hit'],0.)
            self.assertEqual(hit[other+'Time'],0.)
            self.assertEqual(c.update(quiet,0,False),hit)
            reset=c.update(quiet,0,True)
            self.assertEqual(reset[band+'Time'],0.)
            self.assertEqual(reset[band+'Hit'],0.)

    def test_bass_transients_are_independent_and_pause_safe(self):
        quiet=dict(energy=.2,bass=0.,mid=.2,treble=.2)
        c=AudioController();c.update(quiet,0,True)
        high=c.update(dict(quiet,treble=.8),.03,False)
        self.assertEqual(high['bassHit'],0.)
        hit=c.update(dict(quiet,bass=.8),.03,False)
        self.assertGreater(hit['bassHit'],.8)
        self.assertGreater(hit['bassTime'],0.)
        self.assertEqual(c.update(quiet,0,False),hit)

    def test_modest_music_attacks_are_visible_without_noise_pumping(self):
        steady=dict(energy=.3,bass=.3,mid=.3,treble=.3)
        for band in ('bass','mid','treble'):
            c=AudioController();c.update(steady,0,True)
            hit=c.update(dict(steady,**{band:.34}),1/60,False)
            self.assertGreater(hit[band+'Hit'],.3)
            phase=hit[band+'Impulse']
            for _ in range(60):released=c.update(steady,1/60,False)
            self.assertLess(released[band+'Hit'],.001)
            self.assertGreater(released[band+'Impulse'],phase)
            self.assertEqual(c.update(steady,0,False),released)
            self.assertEqual(c.update(steady,0,True)[band+'Impulse'],0.)
            c=AudioController();quiet=dict.fromkeys(steady,0.)
            c.update(quiet,0,True)
            self.assertEqual(c.update(dict(quiet,**{band:.005}),1/60,False)[band+'Hit'],0.)

    def test_descriptor_rejects_unbounded_or_ambiguous_graph(self):
        valid=dict(passSchema=1,passes=[dict(id='light',shader='main.glsl',scale=1,format='rgba8')])
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            def parse(d):
                (root/'scene.json').write_text(json.dumps(d))
                return pass_descriptors(root)
            self.assertEqual(len(parse(valid)),1)
            variants=[]
            for key,value in [('shader','../outside.glsl'),('scale',8),('scale',True),
                              ('inputs',['future']),('history',1),('format','rgba32f')]:
                d=copy.deepcopy(valid); d['passes'][0][key]=value; variants.append(d)
            d=copy.deepcopy(valid); d['passes']*=5; variants.append(d)
            d=copy.deepcopy(valid); d['passes']*=2; variants.append(d)
            for d in variants:
                with self.subTest(d=d),self.assertRaises(ValueError): parse(d)

    def test_explicit_state_graph(self):
        state=dict(id='state',shader='state.glsl',size=[384,216],format='rgba32f',
                   state=True,semantic='test-signed-v1',history=True,
                   inputs=[dict(pass_='state',frame='previous')])
        state['inputs']=[{'pass':'state','frame':'previous'}]
        valid=dict(passSchema=2,passes=[state,dict(id='finish',shader='main.glsl',scale=1,
                   format='rgba8',inputs=[{'pass':'state','frame':'current'}])])
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            def parse(d):
                (root/'scene.json').write_text(json.dumps(d));return pass_descriptors(root)
            self.assertEqual(parse(valid)[0]['format'],'rgba32f')
            variants=[]
            for key,value in [('history',False),('size',[513,216]),('size',[True,216]),
                              ('semantic',''),('format','rgba8'),('filter','mipmap'),
                              ('inputs',[{'pass':'state','frame':'current'}]),
                              ('inputs',[{'pass':'absent','frame':'previous'}]),
                              ('inputs',[{'pass':'state','frame':'future'}]),('inputs',['state'])]:
                d=copy.deepcopy(valid);d['passes'][0][key]=value;variants.append(d)
            d=copy.deepcopy(valid);d['passes'][1]['inputs'][0]['frame']='previous';variants.append(d)
            for d in variants:
                with self.subTest(d=d),self.assertRaises(ValueError):parse(d)


if __name__=='__main__': unittest.main()
