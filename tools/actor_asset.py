#!/usr/bin/env python3
"""Fingerprint and compile the reviewed Ninten regular-animation texture."""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.upstream import read_json,write_json,safe_path,git
from tools.asset_receipts import receipt_path, receipt_entries
REVIEW=ROOT/'compatibility/reviews/ninten-sprite-v0410.json'
OUT=ROOT/'romfs/graphics/actors'

def sha(path: Path)->str:return hashlib.sha256(path.read_bytes()).hexdigest()

def validate_source(root: Path,recipe: dict,lock: dict)->None:
    if (recipe.get('schema')!=1 or recipe.get('game_version')!='0.4.1.0' or
        recipe.get('size')!=[310,580] or recipe.get('grid')!=[10,20] or
        recipe.get('sprite_offset')!=[0,-4] or recipe.get('centered') is not True):
        raise ValueError('Unreviewed actor sprite schema/version/layout')
    if recipe.get('commit')!=lock.get('commit') or lock.get('game_version')!=recipe['game_version']:
        raise ValueError('Actor sprite differs from pinned source')
    if recipe.get('lamp_layout')!={'size':[68,22],'grid':[4,1],'idle_frame':0,'root':[496,390],'child_offset':[0,9],'auto_offset':[0,-11],'shadow':False}:
        raise ValueError('Unreviewed lamp layout')
    if not recipe.get('licence_review'):raise ValueError('Missing asset permission review')
    for key in ('scene','texture','lamp_texture','lamp_yaml','npc_script','character_sprite_script','emote_texture','emote_scene','shadow_texture','actor_scene'):
        if sha(safe_path(root,recipe[key]))!=recipe[key+'_sha256']:
            raise ValueError('Changed source requires semantic/asset review: '+recipe[key])

def compile_sprite(root: Path,tex3ds: Path,out: Path=OUT)->None:
    recipe=read_json(REVIEW);validate_source(root,recipe,read_json(ROOT/'upstream.lock'))
    if git(root,'rev-parse','HEAD')!=recipe['commit'] or git(root,'status','--porcelain'):
        raise ValueError('Actor asset preparation requires pristine pinned upstream')
    from PIL import Image
    source=safe_path(root,recipe['texture'])
    with Image.open(source) as image:
        if image.size!=tuple(recipe['size']) or image.format!='PNG':raise ValueError('Unexpected actor dimensions/codec')
    out.mkdir(parents=True,exist_ok=True)
    target=out/'ninten-main.t3x'
    subprocess.run([str(tex3ds),'-f','rgba8','-z','none','-o',str(target),str(source)],check=True)
    lamp_source=safe_path(root,recipe['lamp_texture'])
    with Image.open(lamp_source) as image:
        if image.size!=(68,22) or image.format!='PNG':raise ValueError('Unexpected lamp dimensions/codec')
    lamp_target=out/'lamp.t3x'
    subprocess.run([str(tex3ds),'-f','rgba8','-z','none','-o',str(lamp_target),str(lamp_source)],check=True)
    extra_targets=[]
    for key,name,size in [('emote_texture','emotes.t3x',(372,384)),('shadow_texture','shadow.t3x',(15,5))]:
        source=safe_path(root,recipe[key]);target_extra=out/name
        with Image.open(source) as image:
            if image.size!=size or image.format!='PNG':raise ValueError('Unexpected '+key+' dimensions/codec')
        subprocess.run([str(tex3ds),'-f','rgba8','-z','none','-o',str(target_extra),str(source)],check=True)
        extra_targets.append(target_extra)
    receipt={'schema':1,'recipe':recipe,'pixel_format':'RGBA8; no lossy compression; nearest sampling',
             'tex3ds_sha256':sha(tex3ds),'outputs':{p.name:{'sha256':sha(p),'bytes':p.stat().st_size} for p in (target,lamp_target,*extra_targets)},
             'scope':'Regular Ninten sprite only; no AnimationTree, blink, special animation or shader compatibility claim'}
    write_json(receipt_path(out, ROOT),receipt)
    write_json(ROOT/'reports/m3-actor-asset-build.json',receipt)
    print('Compiled pinned Ninten regular-animation texture.')

def verify(root: Path,out: Path=OUT)->None:
    if not out.exists():raise ValueError('Missing required actor assets; run make actor-assets')
    recipe=read_json(REVIEW);validate_source(root,recipe,read_json(ROOT/'upstream.lock'))
    receipt=read_json(receipt_path(out, ROOT))
    if receipt.get('schema')!=1 or receipt.get('recipe')!=recipe:raise ValueError('Stale actor asset receipt')
    outputs=receipt.get('outputs',{})
    if set(outputs)!={'ninten-main.t3x','lamp.t3x','emotes.t3x','shadow.t3x'} or {p.name for p in out.iterdir()}!=set(outputs)|receipt_entries(out, ROOT):
        raise ValueError('Unexpected or missing actor asset files')
    for name,record in outputs.items():
        path=safe_path(out,name)
        if sha(path)!=record.get('sha256') or path.stat().st_size!=record.get('bytes'):
            raise ValueError('Compiled actor asset differs from receipt: '+name)
    print('Verified pinned Ninten texture before embedding in RomFS.')

def main()->int:
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('action',choices=['compile','verify'])
    ap.add_argument('--root',type=Path,default=ROOT/'upstream/MOTHER-Encore')
    ap.add_argument('--tex3ds',type=Path,default=Path('/opt/devkitpro/tools/bin/tex3ds'));args=ap.parse_args()
    try:
        if args.action=='compile':compile_sprite(args.root,args.tex3ds)
        else:verify(args.root)
        return 0
    except (OSError,ValueError,KeyError,ImportError,subprocess.CalledProcessError) as error:
        print(f'ACTOR ASSET ERROR: {error}',file=sys.stderr);return 1
if __name__=='__main__':raise SystemExit(main())
