#!/usr/bin/env python3
"""Reviewed, source-pinned first-round textures and presentation IR.

Only the bounded source tracks below are translated. Procedural tween records
are reviewed source data, never C++ game constants. Upstream remains read-only.
"""
from __future__ import annotations
import argparse, hashlib, json, math, re, subprocess, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor, animation, node, properties, one
from tools.upstream import read_json,write_json,safe_path
from tools.asset_receipts import receipt_path, receipt_entries
from PIL import Image
RECIPE=ROOT/'content/round-assets.json'
OUT=ROOT/'romfs/graphics/battle/round'
REPORT=ROOT/'reports/battle-victory-presentation'
NIL=4294967295

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def validate_source(root,recipe,lock):
    if set(recipe)!={'schema','commit','game_version','licence_review','sources','resources'} or recipe['schema']!=1 or recipe['commit']!=lock['commit'] or recipe['game_version']!=lock['game_version'] or not recipe['licence_review']:
        raise ValueError('Unreviewed round assets schema/source/permission')
    seen=set()
    for path,digest in recipe['sources'].items():
        if sha(safe_path(root,path))!=digest:raise ValueError('Changed round source: '+path)
    for i,r in enumerate(recipe['resources']):
        if set(r)!={'id','name','source','size','grid','output_grid','output'} or r['id']!=i+1 or r['source'] not in recipe['sources'] or r['output'] in seen:raise ValueError('Unknown/duplicate resource')
        safe_path(ROOT/'romfs',r['output']);seen.add(r['output'])
        for field in ['size','grid','output_grid']:
            if len(r[field])!=2 or any(type(x)!=int or x<=0 for x in r[field]):raise ValueError('Invalid resource geometry')
        if any(s%g for s,g in zip(r['size'],r['grid'])) or math.prod(r['grid'])!=math.prod(r['output_grid']):raise ValueError('Invalid atlas mapping')
        if any(s//g*o>1024 for s,g,o in zip(r['size'],r['grid'],r['output_grid'])):raise ValueError('Atlas exceeds GPU extent')

def repack(image,grid,outgrid):
    if image.width%grid[0] or image.height%grid[1] or math.prod(grid)!=math.prod(outgrid):raise ValueError('Invalid atlas mapping')
    w,h=image.width//grid[0],image.height//grid[1]
    output=Image.new('RGBA',(w*outgrid[0],h*outgrid[1]))
    for f in range(math.prod(grid)):
        tile=image.crop((f%grid[0]*w,f//grid[0]*h,(f%grid[0]+1)*w,(f//grid[0]+1)*h))
        output.paste(tile,(f%outgrid[0]*w,f//outgrid[0]*h))
    return output

def compile_assets(root,tex3ds,out=OUT):
    recipe=read_json(RECIPE);validate_source(root,recipe,read_json(ROOT/'upstream.lock'))
    # Extractor checks immutable commit and full reviewed inventory hashes.
    ex=Extractor(ROOT);build=ROOT/'build/round-assets';build.mkdir(parents=True,exist_ok=True);out.mkdir(parents=True,exist_ok=True)
    resources=[]
    for r in recipe['resources']:
        image=Image.open(safe_path(root,r['source'])).convert('RGBA')
        if list(image.size)!=r['size']:raise ValueError('Image dimensions changed')
        converted=repack(image,r['grid'],r['output_grid']);png=build/(r['name']+'.png');converted.save(png)
        target=out/Path(r['output']).name
        subprocess.run([str(tex3ds),'-f','rgba8','-z','none','-o',str(target),str(png)],check=True)
        resources.append(dict(id=r['id'],name=r['name'],path=r['output'],kind=1,width=converted.width,height=converted.height,columns=r['output_grid'][0],rows=r['output_grid'][1],sha256=sha(target)))
    receipt=dict(schema=1,recipe=recipe,resources=resources,outputs={Path(r['path']).name:dict(sha256=r['sha256'],bytes=(out/Path(r['path']).name).stat().st_size) for r in resources},tex3ds_sha256=sha(tex3ds),limits='Lossless repacking; nearest sampling; no GPU/hardware comparison claim')
    write_json(receipt_path(out, ROOT),receipt)
    export_presentation(ex,resources)
    print('Compiled source-pinned round assets and presentation IR')

def verify(root,out=OUT):
    recipe=read_json(RECIPE);validate_source(root,recipe,read_json(ROOT/'upstream.lock'));receipt=read_json(receipt_path(out, ROOT))
    if receipt.get('recipe')!=recipe:raise ValueError('Stale round asset receipt')
    if set(receipt['outputs'])!={Path(r['output']).name for r in recipe['resources']} or {p.name for p in out.iterdir()}!=set(receipt['outputs'])|receipt_entries(out, ROOT):raise ValueError('Missing/unexpected round files')
    for name,r in receipt['outputs'].items():
        p=safe_path(out,name)
        if sha(p)!=r['sha256'] or p.stat().st_size!=r['bytes']:raise ValueError('Changed round output: '+name)
    verify_reviewed_presentation(ROOT)
    print('Verified source-pinned round assets and presentation recipe')

class Presentation:
    def __init__(self,ex,resources):self.ex=ex;self.resources=resources;self.media=[];self.tracks=[];self.keys=[];self.events=[];self.bindings={};self.parameters={}
    def resource(self,name):return next(i for i,r in enumerate(self.resources) if r['name']==name)
    def add(self,name,role,resource=NIL,duration=0,rect=(0,0,0,0),flags=0,color=(1,1,1,1),anchor=(.5,.5)):
        m=dict(id=len(self.media)+1,name=name,role=role,resource=resource,first_track=len(self.tracks),track_count=0,first_event=len(self.events),event_count=0,flags=flags,duration=duration,rect=list(rect),color=list(color),anchor=list(anchor));self.media.append(m);return m
    def track(self,m,prop,times,values,update=0,interp=0,mode=0,eases=None):
        if len(times)!=len(values):raise ValueError('Track geometry mismatch')
        self.tracks.append(dict(media=m['id']-1,property=prop,first=len(self.keys),count=len(times),update=update,interpolation=interp,mode=mode));m['track_count']+=1
        for t,v,e in zip(times,values,eases or [1]*len(times)):
            v=list(v) if isinstance(v,(list,tuple)) else [int(v) if isinstance(v,bool) else v]
            self.keys.append(dict(time=t,ease=e,value=v+[0]*(4-len(v))))
    def event(self,m,t,kind):self.events.append(dict(media=m['id']-1,time=t,kind=kind));m['event_count']+=1
    def anim(self,path,name,role,res=NIL,rect=(0,0,0,0),nodepath='AnimationPlayer',flags=0,anchor=(.5,.5)):
        text=self.ex.text(path)
        rid=int(one(r'^anims/'+re.escape(name)+r' = SubResource\( (\d+) \)',text,name)[1]) if '!' in name else node(text,nodepath)['anims/'+name]['SubResource']
        from tools.round_animation_bindings import checked_visual_source
        if not hasattr(self,'animation_config'):raise ValueError('Unreviewed presentation track: missing checked source bindings')
        text=checked_visual_source(text,rid,path,name,self.animation_config)
        a=animation(text,rid,path,name)
        m=self.add(path+':'+name,role,res,a['length'],rect,flags|(1 if a['loop'] else 0),anchor=anchor)
        config=self.animation_config;mapping=config['properties']
        for tr in a['tracks']:
            k=tr['keys'];p=tr['path']
            if not k['times']:continue
            if tr['type']=='method':
                for t,v in zip(k['times'],k['values']):
                    if v['args'] or v['method'] not in config['methods']:raise ValueError('Unreviewed action method')
                    self.event(m,t,config['methods'][v['method']])
                continue
            if p in config['atlas']:
                if len(k['values'])!=1:raise ValueError('Changing resource binding not reviewed')
                r=self.resources[res]
                selector=config['atlas'][p];expected=r[selector]if selector!='texture'else None
                if selector!='texture' and k['values'][0]!=expected:raise ValueError('Effect atlas mismatch')
                continue
            if p not in mapping:raise ValueError('Unreviewed presentation track '+path+':'+p)
            if p in config['outline'] and k['values']!=config['outline'][p]:raise ValueError('Outline rendering outside scope')
            if str(tr['interp']) not in config['interpolation']:raise ValueError('Unknown source interpolation')
            self.track(m,mapping[p],k['times'],k['values'],k.get('update',0),config['interpolation'][str(tr['interp'])],eases=k['transitions'])
        return m
    def bind(self,name,m):self.bindings[name]=m['id']-1;return m

def build_presentation(ex,resources,recipe=None):
    from tools.round_presentation_recipe import apply
    p=Presentation(ex,resources);context=apply(p,recipe)
    for path,digest in read_json(ex.root/'content/round-assets.json')['sources'].items():ex.data(path)
    report=dict(schema=1,sources=ex.sources,resources=[{k:v for k,v in r.items() if k!='name'} for r in resources],media=p.media,tracks=p.tracks,keys=p.keys,events=p.events,bindings=p.bindings,parameters=p.parameters,skill_media=context['skill_media'])
    return report

def verify_reviewed_presentation(root=ROOT):
    from tools.round_presentation_recipe import read
    resources=read_json(root/'content/asset-receipts/graphics/battle/round/source.json')['resources']
    fresh=build_presentation(Extractor(root),resources,read(root/'content/round-presentation-recipe.json'))
    if fresh!=read_json(root/'reports/battle-victory-presentation/presentation.json'):raise ValueError('Stale reviewed presentation recipe output')
    base=read_json(root/'content/native-round.json')['presentation']
    for key,value in base.items():
        if key=='parameters':
            if any(value[name]!=fresh[key][name]for name in fresh[key]):raise ValueError('Stale presentation parameters in native round IR')
        elif value!=fresh[key]:raise ValueError('Stale presentation section in native round IR: '+key)
    return fresh

def export_presentation(ex,resources):
    report=build_presentation(ex,resources)
    REPORT.mkdir(parents=True,exist_ok=True);write_json(REPORT/'presentation.json',report)
    return report

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('action',choices=['compile','verify','extract']);parser.add_argument('--root',type=Path,default=ROOT/'upstream/MOTHER-Encore');parser.add_argument('--tex3ds',type=Path);args=parser.parse_args()
    try:
        if args.action=='compile':
            if args.tex3ds is None:raise ValueError('Need official tex3ds path')
            compile_assets(args.root,args.tex3ds)
        elif args.action=='extract':export_presentation(Extractor(ROOT),read_json(receipt_path(OUT, ROOT))['resources'])
        else:verify(args.root)
        return 0
    except (ValueError,OSError,KeyError,TypeError,subprocess.SubprocessError) as e:print('ROUND ASSET ERROR:',e,file=sys.stderr);return 1
if __name__=='__main__':raise SystemExit(main())
