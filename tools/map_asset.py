#!/usr/bin/env python3
"""Lossless, fingerprinted opening-map BACKGROUND asset pipeline, not scene import."""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.upstream import read_json,write_json,safe_path,git
REVIEW=ROOT/'compatibility/reviews/house-background-v0410.json'
TILES=ROOT/'build/m2/house-background'

def sha(path: Path)->str:return hashlib.sha256(path.read_bytes()).hexdigest()

def validate_source(root: Path,recipe: dict,lock: dict)->None:
    if recipe.get('schema')!=1 or recipe.get('game_version')!='0.4.1.0' or recipe.get('tile_size')!=256 or recipe.get('size')!=[624,1104]:
        raise ValueError('Unreviewed map asset schema/version/layout')
    if recipe.get('commit')!=lock.get('commit') or lock.get('game_version')!=recipe['game_version']:
        raise ValueError('Map asset recipe differs from pinned source')
    if not recipe.get('licence_review'):raise ValueError('Missing asset permission review')
    for key in ('scene','texture'):
        path=safe_path(root,recipe[key])
        if sha(path)!=recipe[key+'_sha256']:raise ValueError('Changed source requires semantic/asset review: '+recipe[key])

def layout(recipe: dict)->list[dict]:
    width,height=recipe['size'];step=recipe['tile_size'];result=[]
    for y in range(0,height,step):
        for x in range(0,width,step):
            result.append({'index':len(result),'x':x,'y':y,'width':min(step,width-x),'height':min(step,height-y)})
    return result

def prepare(root: Path,out: Path)->None:
    recipe=read_json(REVIEW);validate_source(root,recipe,read_json(ROOT/'upstream.lock'))
    if git(root,'rev-parse','HEAD')!=recipe['commit'] or git(root,'status','--porcelain'):
        raise ValueError('Asset preparation requires pristine pinned upstream')
    from PIL import Image
    with Image.open(safe_path(root,recipe['texture'])) as source:
        if source.size!=tuple(recipe['size']) or source.format!='PNG':raise ValueError('Unexpected source image dimensions/codec')
        image=source.convert('RGBA');out.mkdir(parents=True,exist_ok=True);tiles=layout(recipe)
        rebuilt=Image.new('RGBA',image.size)
        for tile in tiles:
            x,y,w,h=(tile[k] for k in ('x','y','width','height'))
            fragment=image.crop((x,y,x+w,y+h));name=f"tile-{tile['index']:02d}.png"
            fragment.save(out/name);tile['png']=name;tile['sha256']=sha(out/name)
            rebuilt.paste(fragment,(x,y))
        if rebuilt.tobytes()!=image.tobytes():raise ValueError('Lossless tile reconstruction mismatch')
    write_json(out/'tiles.json',{'schema':1,'recipe_sha256':sha(REVIEW),'source_texture_sha256':recipe['texture_sha256'],
        'pixel_reconstruction':'identical RGBA pixels','tiles':tiles})
    print(f'Prepared {len(tiles)} lossless tiles from original opening-area background; no gameplay export.')

def compile_tiles(root: Path,tiles_root: Path,tex3ds: Path)->None:
    recipe=read_json(REVIEW);validate_source(root,recipe,read_json(ROOT/'upstream.lock'))
    manifest=read_json(tiles_root/'tiles.json')
    if manifest.get('schema')!=1 or manifest.get('recipe_sha256')!=sha(REVIEW) or manifest.get('source_texture_sha256')!=recipe['texture_sha256']:
        raise ValueError('Stale/unsupported tile manifest; regenerate from source')
    tiles=manifest.get('tiles',[]);expected=layout(recipe)
    if len(tiles)!=len(expected):raise ValueError('Incomplete map tiles')
    for tile,wanted in zip(tiles,expected):
        if any(tile.get(k)!=v for k,v in wanted.items()):raise ValueError('Tile geometry/order mismatch')
        if tile.get('png')!=f"tile-{wanted['index']:02d}.png":raise ValueError('Unsupported tile file identity')
        if sha(safe_path(tiles_root,tile['png']))!=tile.get('sha256'):raise ValueError('Tile bytes changed; regenerate from source')
    out=ROOT/'romfs/map-preview';out.mkdir(parents=True,exist_ok=True);outputs={}
    for tile in tiles:
        target=out/f"house-{tile['index']:02d}.t3x"
        # This packaged tex3ds documents -b but rejects that short option.
        # Its default border is already none; do not change the pixel geometry.
        subprocess.run([str(tex3ds),'-f','rgba8','-z','none','-o',str(target),str(tiles_root/tile['png'])],check=True)
        outputs[target.name]={'sha256':sha(target),'bytes':target.stat().st_size}
    receipt={'schema':1,'commit':recipe['commit'],'game_version':recipe['game_version'],'recipe':recipe,
        'pixel_format':'RGBA8, no lossy compression, nearest sampling in viewer','tiles':tiles,'outputs':outputs,
        'tex3ds_sha256':sha(tex3ds),'scope':'static opening map background preview; not a playable map'}
    write_json(out/'source.json',receipt);write_json(ROOT/'reports/m2-map-asset-build.json',receipt)
    print('Compiled 15 real texture tiles for 3DS; background preview only, no scene compatibility approval.')

def verify(root: Path,out: Path)->None:
    if not out.exists():raise ValueError("Missing required opening-map assets; run make map-assets")
    receipt=read_json(out/'source.json');recipe=read_json(REVIEW)
    validate_source(root,recipe,read_json(ROOT/'upstream.lock'))
    if receipt.get('schema')!=1 or receipt.get('recipe')!=recipe:raise ValueError('Unreviewed/stale map build receipt')
    outputs=receipt.get('outputs',{})
    if set(outputs)!={f'house-{i:02d}.t3x' for i in range(15)}:raise ValueError('Incomplete map build receipt')
    if {p.name for p in out.iterdir()}!=set(outputs)|{'source.json'}:raise ValueError('Unexpected map preview files require review')
    for name,record in outputs.items():
        path=safe_path(out,name)
        if sha(path)!=record.get('sha256') or path.stat().st_size!=record.get('bytes'):raise ValueError('Compiled map asset differs from receipt: '+name)
    print('Verified pinned map background tiles before embedding in RomFS.')

def main()->int:
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('action',choices=['prepare','compile','verify'])
    ap.add_argument('--root',type=Path,default=ROOT/'upstream/MOTHER-Encore');ap.add_argument('--tiles',type=Path,default=TILES)
    ap.add_argument('--tex3ds',type=Path,default=Path('/opt/devkitpro/tools/bin/tex3ds'));args=ap.parse_args()
    try:
        if args.action=='prepare':prepare(args.root,args.tiles)
        elif args.action=='compile':compile_tiles(args.root,args.tiles,args.tex3ds)
        else:verify(args.root,ROOT/'romfs/map-preview')
        return 0
    except (OSError,ValueError,KeyError,ImportError,subprocess.CalledProcessError) as error:
        print(f'MAP ASSET ERROR: {error}',file=sys.stderr);return 1
if __name__=='__main__':raise SystemExit(main())
