#!/usr/bin/env python3
"""Pinned house NPC, door leaf and original world-dialogue resources.

The content recipe and generated IR remain external to the native executable.
The source fonts are identical to the already reviewed battle font; no system
font or synthetic NPC motion is introduced.
"""
from __future__ import annotations
import argparse, hashlib, json, re, struct, subprocess, sys
from pathlib import Path
import yaml
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.upstream import read_json,write_json,safe_path,git
from tools.asset_receipts import receipt_path, receipt_entries
from tools.extract_battle_entry import Extractor,node,animation,one
RECIPE=ROOT/'content/house-assets.json'
OUT=ROOT/'romfs/graphics/ui/house'
IR=ROOT/'content/native-house-presentation.json'

def f32(value):return struct.unpack('<f',struct.pack('<f',value))[0]
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def validate_source(root,recipe,lock):
    if set(recipe)!={'schema','commit','game_version','licence_review','sources','resources'} or recipe['schema']!=1 or recipe['commit']!=lock['commit'] or recipe['game_version']!=lock['game_version'] or not recipe['licence_review']:raise ValueError('Unreviewed house asset schema/source/license')
    for path,digest in recipe['sources'].items():
        if sha(safe_path(root,path))!=digest:raise ValueError('Changed house source: '+path)
    paths=set();roles=set()
    for i,r in enumerate(recipe['resources']):
        if set(r)!={'id','role','source','size','grid','output'} or r['id']!=i+1 or r['source'] not in recipe['sources'] or r['output'] in paths or r['role'] in roles:raise ValueError('Unknown/duplicate house asset')
        safe_path(ROOT/'romfs',r['output']);paths.add(r['output']);roles.add(r['role'])
        if any(len(r[k])!=2 or any(type(x)!=int or x<=0 for x in r[k]) for k in ['size','grid']) or any(s%g for s,g in zip(r['size'],r['grid'])) or max(r['size'])>1024:raise ValueError('Invalid house asset grid')
    from tools.house_source_bindings import load
    if roles!=set(load(ROOT)['asset_roles']):raise ValueError('Unsupported house asset roles')

def export_presentation(root,resources,destination=IR,write=True):
    from tools.house_source_bindings import load
    b=load(ROOT)
    ex=Extractor(ROOT);recipe=read_json(RECIPE);validate_source(root,recipe,read_json(ROOT/'upstream.lock'))
    for path in recipe['sources']:ex.data(path)
    scene=ex.text(b['source_refs']['dialogue_scene']);npc=ex.text(b['source_refs']['npc_scene'])
    # Textual TSCN permits physical newlines in quoted sample label strings.
    scene=re.sub(r'(?m)^(bbcode_text|text) = \"(.*?)\"(?=\n)',lambda m:m[1]+' = '+json.dumps(m[2]),scene,flags=re.S)
    if ex.data(b['source_refs']['main_font'])!=ex.data(b['source_refs']['reviewed_font']):raise ValueError('World and reviewed font resources differ')
    box=node(scene,b['nodes']['dialogue_box']);name=node(scene,b['nodes']['name_box']);clip=node(scene,b['nodes']['clip_box']);hbox=node(scene,b['nodes']['hbox']);label=node(scene,b['nodes']['dialogue_label']);bullet=node(scene,b['nodes']['bullet']);namelabel=node(scene,b['nodes']['name_label']);cursor=node(scene,b['nodes']['dialogue_cursor'])
    font=read_json(ROOT/'content/asset-receipts/graphics/battle/lamp/source.json')['font_metrics']
    abstract=ex.text(b['source_refs']['abstract_script']);globaldata=ex.text(b['source_refs']['global_data'])
    def c(key):return float(one(r'^const '+key+r'\s*:?=\s*([0-9.]+)',abstract,key)[1])
    def margins(n):return [n['patch_margin_'+p] for p in ['left','top','right','bottom']]
    def rect(n):return [n.get('margin_left',0),n.get('margin_top',0),n.get('margin_right',0)-n.get('margin_left',0),n.get('margin_bottom',0)-n.get('margin_top',0)]
    sprite=node(npc,b['nodes']['character_sprite']);shadow=node(npc,b['nodes']['shadow']);house=ex.text(b['source_refs']['house'])
    params=dict(DialogueRect=rect(box),DialogueMargins=margins(box),DialogueClip=[clip['margin_left'],clip['margin_top'],rect(box)[2]-clip['margin_left'],clip['margin_bottom']-clip['margin_top']],DialogueText=[hbox['margin_left']+label['margin_left'],hbox['margin_top'],label['margin_right']-label['margin_left'],label['rect_min_size'][1]],DialogueBullet=[hbox['margin_left'],hbox['margin_top'],bullet['rect_min_size'][0],bullet['rect_min_size'][1]],NameRect=rect(name),NameMargins=margins(name),NameLabel=rect(namelabel),NameSizing=[*b['presentation']['name_sizing'],namelabel['rect_min_size'][0]],CursorGeometry=[*cursor['position'],*b['cursor_frame_size']],CursorRotation=[cursor['rotation'],0,0,0],TextTiming=[json.loads(one(r'const TEXT_SPEEDS := (\[[^\n]+\])',globaldata,'text speed')[1])[0],c('SPEED_UP_FROM_PRESS_A'),c('SPEED_UP_FROM_PRESS_B'),0],TextTagSpeeds=[c('NORMAL_SPEED'),c('FASTER_SPEED'),c('SLOWER_SPEED'),0],VoicePitch=[*b['voice_pitch'],0,0],DisplayReference=b['presentation']['display_reference'],FontMetrics=[font['height'],label['custom_constants/line_separation'],font['ascent'],font['descent']])
    profiles=[];clips=[]
    def add(role,res,duration,loop,interp,keys,profile=None):clips.append(dict(id=len(clips)+1,role=role,profile=profile,resource=res,duration=f32(duration),loop=loop,interpolation=interp,keys=[dict(k,time=f32(k['time'])) for k in keys]))
    # Source npc._update_sprite_and_animations resolves these actors;
    # each owns its profile even when two share the same YAML state machine.
    def add_npc(path,role):
        instance=node(house,path);resource=next(r for r in resources if r['role']==role)
        if b['patterns']['sprite'].format(sprite=instance['sprite']) != next(r['source'] for r in recipe['resources'] if r['role']==role):raise ValueError('NPC sprite/profile binding changed')
        animation_path=instance.get('yaml','res://'+b['source_refs']['default_animation']).removeprefix('res://')
        anims=yaml.safe_load(ex.text(animation_path))
        if set(anims)!={'animations','offset','size','type'} or anims['type']!=0 or anims['size']!=[resource['columns'],resource['rows']] or anims['offset']!=[0,0]:raise ValueError('Changed NPC animation schema')
        directions=len(anims['animations']['Idle']['directions'])
        has_talk='Talk' in anims['animations']
        if directions not in (1,4) or instance.get('idle_animation','Idle')!='Idle' or instance.get('talk_idle_animation','Idle')!='Idle':raise ValueError('Unsupported initial NPC state')
        instance_offset=instance.get('sprite_offset',[0,0])
        offset=[sprite['position'][0]+anims['offset'][0]+instance_offset[0],sprite['position'][1]-int(resource['height']/(resource['rows']*2))+anims['offset'][1]+instance_offset[1]]
        binding=next(n for n in b['npcs']if n['source_path']==path)
        profiles.append(dict(id=binding['profile_id'],role=role,resource=role,sprite_offset=offset,shadow_offset=shadow['position'],directions=directions,flags=1|(0 if instance.get('no_shadow',False) else 2)|(4 if has_talk else 0)))
        for state in b['presentation']['npc_states'][:1]+(b['presentation']['npc_states'][1:]if has_talk else[]):
            spec=anims['animations'][state]
            if spec['type']!=0 or len(spec['directions'])!=directions:raise ValueError('Unsupported NPC animation')
            for direction,track in zip(b['presentation']['directions'],spec['directions']):
                if len(track)<2 or track[0]<0:raise ValueError('Unsupported NPC frame track')
                t=track[0];keys=[]
                for frame,duration in track[1:]:
                    if type(frame)!=int or frame<1 or duration<=0:raise ValueError('Unsupported NPC frame key')
                    keys.append(dict(time=t,ease=1,position=[0,0],frame=frame-1));t+=duration
                add('Npc'+state+direction,role,t,True,1,keys,role)
    for path in b['presentation']['profile_order']:add_npc(path,next(n['resource']for n in b['npcs']if n['source_path']==path))
    for binding in b['presentation']['ui_clips']:
        player=binding['player'];action=binding['clip']
        a=animation(scene,node(scene,player)['anims/'+action]['SubResource'],b['source_refs']['dialogue_scene'],action)
        if len(a['tracks'])!=1 or a['tracks'][0]['type']!='value' or a['tracks'][0]['keys']['update']!=0 or a['tracks'][0]['interp']!=2:raise ValueError('Unsupported world dialogue animation')
        k=a['tracks'][0]['keys'];add(binding['role'],binding['resource'],a['length'],a['loop'],2,[dict(time=t,ease=e,position=v,frame=0) for t,e,v in zip(k['times'],k['transitions'],k['values'])])
    arrow=ex.text(b['source_refs']['arrow_scene'])
    period=1/b['cursor_fps'];frames=b['cursor_frames']
    add('Cursor',b['presentation']['cursor_role'],period*len(frames),True,1,[dict(time=i*period,ease=1,position=[0,0],frame=f)for i,f in enumerate(frames)])
    for path in b['presentation']['profile_tail']:add_npc(path,next(n['resource']for n in b['npcs']if n['source_path']==path))
    result=dict(schema=2,commit=recipe['commit'],sources=ex.sources,resources=resources,profiles=profiles,parameters=params,clips=clips)
    if write:write_json(destination,result)
    return result

def compile_assets(root,tex3ds,out=OUT):
    recipe=read_json(RECIPE);validate_source(root,recipe,read_json(ROOT/'upstream.lock'))
    if git(root,'rev-parse','HEAD')!=recipe['commit'] or git(root,'status','--porcelain'):raise ValueError('House assets need pristine pinned source')
    out.mkdir(parents=True,exist_ok=True);resources=[]
    for r in recipe['resources']:
        src=safe_path(root,r['source']);target=out/Path(r['output']).name
        with Image.open(src) as image:
            if image.format!='PNG' or list(image.size)!=r['size']:raise ValueError('Changed image size/codec')
        subprocess.run([str(tex3ds),'-f','rgba8','-z','none','-o',str(target),str(src)],check=True)
        resources.append(dict(id=r['id'],role=r['role'],path=r['output'],kind=1,width=r['size'][0],height=r['size'][1],columns=r['grid'][0],rows=r['grid'][1],sha256=sha(target)))
    write_json(receipt_path(out, ROOT),dict(schema=1,recipe=recipe,resources=resources,tex3ds_sha256=sha(tex3ds),outputs={Path(r['path']).name:dict(sha256=r['sha256'],bytes=(out/Path(r['path']).name).stat().st_size) for r in resources}))
    export_presentation(root,resources)

def verify(root,out=OUT):
    recipe=read_json(RECIPE);validate_source(root,recipe,read_json(ROOT/'upstream.lock'));receipt=read_json(receipt_path(out, ROOT))
    if receipt.get('recipe')!=recipe or set(receipt['outputs'])!={Path(r['output']).name for r in recipe['resources']} or {p.name for p in out.iterdir()}!=set(receipt['outputs'])|receipt_entries(out, ROOT):raise ValueError('Stale/missing/unexpected house assets')
    for name,r in receipt['outputs'].items():
        p=safe_path(out,name)
        if sha(p)!=r['sha256'] or p.stat().st_size!=r['bytes']:raise ValueError('Changed house output: '+name)

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('action',choices=['compile','verify','extract']);ap.add_argument('--root',type=Path,default=ROOT/'upstream/MOTHER-Encore');ap.add_argument('--tex3ds',type=Path);a=ap.parse_args()
    try:
        if a.action=='compile':
            if not a.tex3ds:raise ValueError('Need tex3ds')
            compile_assets(a.root,a.tex3ds)
        elif a.action=='extract':export_presentation(a.root,read_json(receipt_path(OUT, ROOT))['resources'])
        else:verify(a.root)
        print('House assets '+a.action+' complete');return 0
    except (ValueError,OSError,KeyError,TypeError,subprocess.SubprocessError) as e:print('HOUSE ASSET ERROR:',e,file=sys.stderr);return 1
if __name__=='__main__':raise SystemExit(main())
