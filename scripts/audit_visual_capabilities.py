"""Read-only source/descriptor inventory for a selected local Synesthesia install.

Output is private research metadata, not a runnable port. Lexical references are
candidate evidence, not proof of dataflow, execution or visual equivalence.
"""
import argparse, collections, hashlib, json, re
from pathlib import Path

COMMENT = re.compile(r'"(?:\\.|[^"\\])*"|/\*[\s\S]*?\*/|//[^\n]*')

def uncomment(text):
    return COMMENT.sub(lambda m: m[0] if m[0].startswith('"') else re.sub(r'[^\n]', ' ', m[0]), text)

def inspect_scene(path):
    descriptor=json.loads(uncomment((path/'scene.json').read_text(encoding='utf-8-sig')))
    files={f.name:f.read_text(encoding='utf-8-sig') for f in path.iterdir() if f.suffix in ('.glsl','.js')}
    shader=uncomment(files.get('main.glsl',''));js=uncomment(files.get('script.js',''));both=shader+'\n'+js
    references={}
    for name,source in files.items():
        for number,line in enumerate(uncomment(source).splitlines(),1):
            if re.search(r'\b(?:syn_\w+|_\w*Media\w*|PASSINDEX|uniforms\.|inputs\.)|texture(?:Lod|2D|Size)?\s*\(|texelFetch\s*\(',line):
                references.setdefault(name,[]).append(dict(line=number,text=line.strip()[:600]))
    return dict(key=path.name,path=str(path),title=descriptor.get('TITLE',path.stem),
        tags=descriptor.get('TAGS',[]),description=descriptor.get('DESCRIPTION',''),
        passes=descriptor.get('PASSES') or [],controls=descriptor.get('CONTROLS') or [],
        hardTransitions=descriptor.get('HARD_TRANSITIONS') or [],smoothTransitions=descriptor.get('SMOOTH_TRANSITIONS') or [],
        images=descriptor.get('IMAGES') or [],cubemaps=descriptor.get('CUBEMAPS') or [],media=descriptor.get('MEDIA'),script=bool(js),
        functions=re.findall(r'\b(?:float|vec[234]|mat[234]|void|bool|int)\s+(\w+)\s*\([^;{}]*\)\s*\{',shader),
        signals=sorted(set(re.findall(r'\bsyn_\w+',both))),
        textureInputs=sorted(set(re.findall(r'\b(?:texture|texture2D|textureLod|textureSize|texelFetch)\s*\(\s*(\w+)',shader))),
        includes=re.findall(r'^\s*#include\s+[<"]([^>"]+)',shader,re.M),references=references,
        hashes={str(f.relative_to(path)):hashlib.sha256(f.read_bytes()).hexdigest() for f in path.rglob('*') if f.is_file()})

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--install',type=Path,required=True);parser.add_argument('--out',type=Path,required=True);args=parser.parse_args()
    args.out.mkdir(parents=True,exist_ok=False)
    paths=sorted(args.install.glob('release/*.synScene'))+sorted(args.install.glob('default/*.synScene'))+sorted(args.install.glob('new_scene/*.synScene'))
    if (args.install/'new_scene/scene.json').is_file():paths.append(args.install/'new_scene')
    rows=[];errors=[]
    for path in paths:
        try:rows.append(inspect_scene(path))
        except Exception as error:errors.append(dict(path=str(path),error=str(error)))
    summary=dict(scenes=len(rows),errors=errors,scripts=sum(r['script'] for r in rows),
        multipass=sum(bool(r['passes']) for r in rows),
        intermediateTargetCounts=dict(collections.Counter(len(r['passes']) for r in rows)),
        renderStageCounts=dict(collections.Counter(len(r['passes'])+1 for r in rows)),
        smoothTransitions=sum(bool(r['smoothTransitions']) for r in rows),hardTransitions=sum(bool(r['hardTransitions']) for r in rows),
        signals=dict(collections.Counter(s for r in rows for s in r['signals'])),
        controlTypes=dict(collections.Counter(c.get('TYPE') for r in rows for c in r['controls'])))
    (args.out/'inventory.json').write_text(json.dumps(dict(root=str(args.install),scenes=rows,errors=errors),indent=2),encoding='utf-8')
    (args.out/'summary.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in summary.items() if k!='signals'}))
    raise SystemExit(bool(errors))

if __name__=='__main__':main()
