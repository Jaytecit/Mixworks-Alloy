"""Bounded GPU checks for scene-local sprite geometry; no audio device or live app."""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--runtime',type=Path,required=True)
    ap.add_argument('--assets',type=Path,required=True)
    ap.add_argument('--out',type=Path,required=True)
    ap.add_argument('--image',type=Path)
    args=ap.parse_args();args.out.mkdir(parents=True,exist_ok=False)
    if os.name=='nt':ctypes.windll.kernel32.SetPriorityClass(ctypes.windll.kernel32.GetCurrentProcess(),0x4000)
    sys.path.insert(0,str(Path(__file__).resolve().parent))
    import numpy as np
    from PIL import Image,ImageDraw
    from OpenGL import GL as gl
    from original_scene_player import OriginalScenePlayer
    from visual_media import ALPHA_NOISE_MAX
    checks=[];metrics={};w,h=640,360
    def check(ok,label):
        checks.append(dict(ok=bool(ok),label=label))
        if not ok:raise AssertionError(label)
    fixture=args.out/'cutout.png'
    im=Image.new('RGBA',(320,180),(255,0,0,ALPHA_NOISE_MAX));draw=ImageDraw.Draw(im)
    draw.rounded_rectangle((45,35,275,145),radius=20,fill=(20,180,230,255))
    draw.ellipse((105,55,215,135),fill=(240,170,40,255));im.save(fixture)
    source=(args.image or fixture).resolve();before=hashlib.sha256(source.read_bytes()).hexdigest()
    library=args.out/'library';preset=b'[preset00]\nfDecay=.965\nzoom=1.045\nrot=.055\nwave_mode=7\nwave_a=0\n'
    key=hashlib.sha256(preset).hexdigest();folder=library/key;folder.mkdir(parents=True)
    (folder/'preset.milk').write_bytes(preset);(folder/'import.json').write_text(json.dumps(dict(schema=1,sha256=key,title='Nested routing probe',texturePaths=[])))
    p=OriginalScenePlayer(args.runtime,args.assets,w,h,milkdrop_library=library);p.pool=[];p.cutouts=[str(source)]
    def actor(scene,t,clip='a',controls=None):return dict(clip=clip,scene=scene,revision='nested',time=t,controls=controls or {})
    def render(t,actors,**kw):return p.render(dict(protocol=6,position=t,bpm=120,rate=44100,generation=1,capture=True,
        scenes=actors,**dict(dict(beat=t*2,mix=0.,transition='feedback',transitionSeconds=2.,image=''),**kw)))
    def picture(raw,name):Image.frombytes('RGBA',(w,h),raw,'raw','BGRA',0,-1).convert('RGB').save(args.out/(name+'.png'))
    try:
        outputs={}
        for label,values in [('flat',(0,0,0)),('relief',(.85,0,0)),('warp',(0,.85,0)),('colour',(0,0,.9)),('combined',(.85,.85,.9))]:
            p.reset();controls=dict(zip(('reliefDepth','meltWarp','displacementColour'),values))
            controls.update(materialMotion=0.,materialMode=0.)
            for f in range(3):facts,raw=render(2+f/30,[actor('original/molten-contours',2+f/30,controls=controls)])
            check('a' in facts['cutoutSelection'],label+' uses a scene-local geometry source')
            outputs[label]=np.frombuffer(raw,np.uint8).copy();picture(raw,label)
            check(p.render(dict(protocol=6,position=2+2/30,beat=(2+2/30)*2,bpm=120,rate=44100,generation=1,capture=True,image='',mix=0.,transition='feedback',transitionSeconds=2.,scenes=[actor('original/molten-contours',2+2/30,controls=controls)]))[1]==raw,label+' holds exact pixels on pause')
        for label in ('relief','warp','colour','combined'):
            check(np.abs(outputs[label].astype(int)-outputs['flat'].astype(int)).mean()>1.,label+' independently changes the rendered material')
        check(len({hashlib.sha256(v.tobytes()).hexdigest() for v in outputs.values()})==5,'individual methods and combined treatment produce five distinct outputs')
        # A normal viewer hides these bytes. Geometry, normals and palettes must
        # remain invariant to them too, using the actual authored source.
        clean=args.out/'clear-noise-reference.png'
        pixels=np.array(Image.open(source).convert('RGBA'))
        pixels[pixels[:,:,3]<=ALPHA_NOISE_MAX]=0
        Image.fromarray(pixels).save(clean)
        p.cutouts=[]
        for slug in ('molten-contours','chromatic-torso','hexa-resonator'):
            images=[]
            for image in (source,clean):
                p.reset()
                for f in range(3):facts,raw=render(2+f/30,[actor('original/'+slug,2+f/30)],image=str(image))
                images.append(raw)
            check(images[0]==images[1],slug+' geometry and colour ignore near-transparent RGB noise exactly')
            picture(images[0],slug+'-source')
        p.cutouts=[str(source)]
        # Changing the chosen sprite at a held frame replaces captured pigment
        # immediately without restarting or advancing the particle simulation.
        p.reset();particle='original/aurora-couriers'
        facts,before_sprite=render(0,[actor(particle,0)])
        position_before=p.instances['a'].simulation_time
        replacement=args.out/'other-cutout.png'
        im=Image.new('RGBA',(320,180),(0,0,0,0))
        ImageDraw.Draw(im).rectangle((10,25,150,155),fill=(50,245,70,255));im.save(replacement)
        facts,after_sprite=render(0,[actor(particle,0)],image=str(replacement))
        check(before_sprite!=after_sprite and p.instances['a'].simulation_time==position_before
              and facts['cutoutSelection']['a'][0][0]==str(replacement),'paused sprite selection refreshes captured pigment without moving particles')
        p.reset();facts,reference=render(0,[actor('original/prism-datastream',0)])
        check(not facts['cutoutSelection'] and not p.cutout_textures,'flat signal effects neither choose nor retain a sprite texture')
        p.reset();facts,with_image=render(0,[actor('original/prism-datastream',0)],image=str(source))
        check(with_image==reference,'an explicit cutout does not become a flat scene overlay')
        milk='milkdrop/'+key
        p.cutouts=[];p.reset()
        _,clean_milk=render(0,[actor(milk,0)])
        p.reset();p.cutouts=[str(source),str(replacement.resolve())]
        facts,first=render(0,[actor(milk,0)],beat=128)
        check(facts['scenes'][0]['spriteFeedback'] and facts['milkdropSpriteSelection']['a'][0][0]==str(source),
              'MilkDrop renders the automatic sprite in its native feedback pass')
        check(first!=clean_milk,'nested input changes the first preset frame')
        picture(first,'milkdrop-nested')
        facts,_=render(1/30,[actor(milk,1/30)],beat=254)
        check(abs(facts['milkdropSpriteSelection']['a'][1]-.5)<1e-6,'sprite rotation blends through the last bar of the phrase')
        facts,_=render(2/30,[actor(milk,2/30)],beat=256)
        check(facts['milkdropSpriteSelection']['a'][0][0]==str(replacement.resolve()),'automatic selection advances after 64 bars')
        facts,pinned=render(3/30,[actor(milk,3/30)],beat=256,image=str(source))
        check(facts['milkdropSpriteSelection']['a'][0]==(str(source),str(source)),'pinned sprite overrides the automatic phrase')
        check(render(3/30,[actor(milk,3/30)],beat=256,image=str(source))[1]==pinned,'MilkDrop pause keeps exact sprite feedback pixels')
        p.cutouts=[]
        facts,trace=render(4/30,[actor(milk,4/30)],beat=256)
        check(not facts['scenes'][0]['spriteFeedback'] and trace!=pinned and trace!=clean_milk,
              'preset continues warping injected material after sprite input stops')
        picture(trace,'milkdrop-warped-after-removal')
        p.cutouts=[str(source)];p.reset()
        facts,raw=render(0,[actor(milk,0)])
        facts,raw=render(1/30,[actor('milkdrop/'+key,1/30),actor('original/prism-datastream',0,'b')])
        current=p.instances['a'].final
        gl.glBindFramebuffer(gl.GL_FRAMEBUFFER,current.fbos[current.front])
        check(raw==bytes(gl.glReadPixels(0,0,w,h,gl.GL_BGRA,gl.GL_UNSIGNED_BYTE)),
              'transition starts from the current outgoing frame')
        for f in range(1,6):facts,raw=render((f+1)/30,[actor('milkdrop/'+key,(f+1)/30),actor('original/prism-datastream',f/30,'b')],mix=f/5)
        check(not facts['cutoutSelection'] and set(facts['milkdropSpriteSelection'])=={'a'},
              'MilkDrop-to-flat join feeds the sprite to the compatible outgoing actor only')
        picture(raw,'milkdrop-to-flat')
        p.reset();facts,raw=render(0,[actor('original/molten-contours',0)])
        for f in range(4):facts,raw=render((f+1)/30,[actor('original/molten-contours',(f+1)/30),actor('original/prism-datastream',f/30,'b')],mix=f/3)
        check(set(facts['cutoutSelection'])=={'a'},'a geometry-to-flat handover keeps artwork inside its outgoing scene only')
        facts,raw=render(5/30,[actor('original/prism-datastream',4/30,'b')])
        check(not facts['cutoutSelection'] and not p.cutout_textures,'the outgoing geometry source retires without becoming an incoming sprite')
        check(gl.glGetError()==gl.GL_NO_ERROR,'nested methods and mixed-engine routes finish without GL errors')
        check(hashlib.sha256(source.read_bytes()).hexdigest()==before,'original sprite file remains byte-identical')
        metrics['gpu']=facts['gpu'];metrics['sourceSha256']=before
    finally:
        p.close()
        (args.out/'report.json').write_text(json.dumps(dict(checks=checks,metrics=metrics),indent=2))
        (args.out/'checks.txt').write_text('\n'.join(('PASS ' if c['ok'] else 'FAIL ')+c['label'] for c in checks)+f'\n{len(checks)} checks; {sum(not c["ok"] for c in checks)} failures\n')
    print(f'{len(checks)} checks; 0 failures')


if __name__=='__main__':main()
