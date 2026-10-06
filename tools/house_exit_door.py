#!/usr/bin/env python3
"""Pinned normal House exit, using the existing checked ENCFDOR1 executor."""
from __future__ import annotations
import argparse, hashlib, json, re, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor, node, one, variant, require, PIN
from tools.podunk_scene import stable, sha, read, write
from tools import field_door
SCENE="Maps/podunk/Nintens House.tscn"
NODE="Doors/Podunk"
IR=ROOT/'content/native-house-exit-door.json'
REVIEW=ROOT/'reports/house-exit-door/source-review.json'
PACK=Path('data/house.encdoor')
KEYS='targetX targetY dir sound end_sound transit_in_anim transit_out_anim transit_in_color transit_out_color fade_in_speed fade_out_speed fadeout_music_on_scene_change fadeout_music_length targetScene _target_scene_params set_respawn set_crumbs unpause_player show_player_after_warp flag_set set_flag_state'.split()
def extract():
 ex=Extractor(ROOT);scene=ex.text(SCENE);door=ex.text('Nodes/Overworld/Door.tscn');script=ex.text(field_door.SCRIPT)
 for p in [field_door.TRANS,field_door.GLOBAL,field_door.FADE,'Nodes/Ui/effects/Fade.tscn']:ex.data(p)
 # The source root and parent have no transform overrides; do not absorb a new affine branch.
 for path in ['.','Doors']:
  props=node(scene,path)
  require(not any(k in props for k in ['position','rotation','rotation_degrees','scale','transform']), 'House exit ancestor transform requires review')
 v={}
 for k in KEYS:
  raw=one(r'var '+re.escape(k)+r'\s*:?=\s*([^\n#]+)',script,k)[1].strip()
  aliases={'Vector2.ZERO':[0,0],'Color.black':[0,0,0,1]}
  v[k]=aliases[raw] if raw in aliases else variant(raw)
 none=v['sound']
 instance=node(scene,NODE)
 require(set(instance)<=set(KEYS)|{'position','scale'},'Unknown House exit source override')
 v.update({k:value for k,value in instance.items() if k in KEYS})
 require(v['_target_scene_params']==[],'House exit init_params unsupported')
 rootprops=node(door,'.');shape=node(door,'CollisionShape2D');marker=node(door,'Position2D');audio=node(door,'AudioStreamPlayer')
 require(set(rootprops)=={'pause_mode','collision_layer','collision_mask','monitorable','script'} and rootprops['script']=={'ExtResource':1},'Door native root changed')
 require(set(shape)=={'position','shape'} and shape['shape']=={'SubResource':1} and set(marker)=={'position'},'Door shape/marker changed')
 extents=variant(one(r'\[sub_resource type="RectangleShape2D" id=1\]\nextents = ([^\n]+)',door,'Door rectangle')[1])
 require('[connection signal="body_entered" from="." to="." method="_on_Door_body_entered"]' in door,'Door actual connection changed')
 pos=instance['position'];scale=instance['scale'];transform=[[scale[0],0],[0,scale[1]],pos]
 markerworld=[pos[i]+scale[i]*marker['position'][i] for i in range(2)]
 initial=one(r'\[ext_resource path="res://([^\"]+)" type="AudioStream" id=2\]',door,'Door initial audio')[1]
 target='Maps/'+v['targetScene']+'.tscn'
 sound=lambda k:'Audio/Sound effects/'+v[k] if v[k] and v[k]!='None' else v[k]
 for p in [target,initial,sound('sound'),sound('end_sound')]:
  if p and p!='None':
   ex.data(p)
   if p+'.import' in ex.inventory['files']:ex.data(p+'.import')
 flags=sum(int(x)<<i for i,x in enumerate([True,rootprops['monitorable'],True,v['set_respawn'],v['set_crumbs'],v['unpause_player'],v['show_player_after_warp'],v['set_flag_state'],v['fadeout_music_on_scene_change']]))
 record=dict(id=stable(NODE),ready=0,node=NODE,marker=stable(NODE+'/Position2D'),audio=stable(NODE+'/AudioStreamPlayer'),shape=stable(NODE+'/CollisionShape2D'),layer=rootprops['collision_layer'],mask=rootprops['collision_mask'],flags=flags,pause_mode=rootprops['pause_mode'],target_name=v['targetScene'],target_path=target,sound=sound('sound'),end_sound=sound('end_sound'),in_anim=v['transit_in_anim'],out_anim=v['transit_out_anim'],flag=v['flag_set'],target=[v['targetX'],v['targetY']],direction=v['dir'],speeds=[v['fade_in_speed'],v['fade_out_speed']],music_fade=v['fadeout_music_length'],in_color=v['transit_in_color'],out_color=v['transit_out_color'],body_transform=transform,marker_transform=[transform[0],transform[1],markerworld],shape_offset=shape['position'],extents=extents)
 offset=float(one(r'targetY - ([0-9]+)',script,'Door ground origin')[1]);require('Vector2(0, '+str(int(offset))+')' in script,'Door ground origins differ')
 ir=dict(schema=1,commit=PIN,scene=SCENE,scene_id=stable('.'),source_sha256=ex.sources[SCENE],sources=ex.sources,scene_admitted=False,capabilities=3,ground_offset=offset,none=none,records=[record],audio=[dict(id=record['audio'],source=initial,bus=audio['bus'])],targets=[dict(id=record['id'],path=target)])
 write(IR,ir)
 write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=ex.sources,producer_sha256=sha(Path(__file__)),dependencies={name:sha(ROOT/name)for name in ['tools/field_door.py','tools/podunk_scene.py','tools/extract_battle_entry.py']},executor='ENCFDOR1 format1 family001f capability3',scope='Only original normal House Doors/Podunk. ready=0 is single-pack ordering, never a full-scene Ready certificate.',semantics=['Inherited Area2D native defaults monitoring=true/visible=true, explicit layer/mask/monitorable/pause_mode and native RectangleShape2D.', 'All source exports/defaults, source target scene fingerprint and initial AudioStreamPlayer/SFX are retained.', 'Existing FieldDoorRuntime actual onready/body signal/16 deferred scene steps/Fade endpoints execute; uncalled special guest remains rejected.', 'House contact uses its existing real source actor polygon overlap; binary binds exact transformed source rectangle.', 'Destination preparation is required before pause/flag/music/fade; loading this pack does not activate or grant Ready.']))
 return ir

def load(root=ROOT):
 ir=read(root/IR.relative_to(ROOT));rv=read(root/REVIEW.relative_to(ROOT));inv=read(root/'compatibility/upstream-inventory.json')['files']
 require(set(ir)==set('schema commit scene scene_id source_sha256 sources scene_admitted capabilities ground_offset none records audio targets'.split()),'House exit unknown metadata')
 require(ir['schema']==1 and ir['commit']==PIN and ir['scene']==SCENE and ir['scene_admitted'] is False and ir['capabilities']==3 and len(ir['records'])==1 and ir['records'][0]['node']==NODE,'House exit scope rejected')
 require(rv['commit']==PIN and rv['ir_sha256']==sha(root/IR.relative_to(ROOT)) and rv['sources']==ir['sources'] and rv['producer_sha256']==sha(root/'tools/house_exit_door.py'),'House exit review rejected')
 require(set(rv['dependencies'])=={'tools/field_door.py','tools/podunk_scene.py','tools/extract_battle_entry.py'},'House exit producer dependency closure rejected')
 for p,h in rv['dependencies'].items():require(sha(root/p)==h,'House exit producer changed '+p)
 for p,h in ir['sources'].items():require(h==inv[p]['sha256']==sha(root/'upstream/MOTHER-Encore'/p),'House exit source changed '+p)
 return ir

def binary(ir):
 # Reuse the authoring encoder, including the IR fingerprint, without modifying it.
 previous=field_door.IR
 try:
  field_door.IR=IR
  return field_door.pack(ir)
 finally:field_door.IR=previous

def stage_files(source_root):
 raw=(Path(source_root)/PACK).read_bytes();require(raw==binary(load()),'House exit staged binary differs');return {PACK:raw}

def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);a=p.parse_args()
 if a.action=='extract':extract();return
 raw=binary(load());out=ROOT/'romfs'/PACK
 if a.action=='verify':require(out.read_bytes()==raw,'House exit binary differs')
 else:out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(raw)
 print('House source exit:',len(raw),'bytes; existing Door executor; scene-admitted=false')
if __name__=='__main__':
 try:main()
 except (OSError,ValueError,KeyError,TypeError) as e:sys.exit('HOUSE EXIT DOOR ERROR: '+str(e))
