#!/usr/bin/env python3
"""Pinned normal House exit, using the existing checked ENCFDOR1 executor."""
from __future__ import annotations
import argparse, hashlib, json, re, sys, struct, zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor, node, one, variant, require, PIN
from tools.podunk_scene import stable, sha, read, write
from tools import field_door
SCENE="Maps/podunk/Nintens House.tscn"
NODE="Doors/Podunk"
AREA="Scripts/Main/RoomTypes/AreaRoom.gd"
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
 area=ex.text(AREA);require(one(r'func leave_for\(new_scene\):\n\s*(emit_signal[^\n]+)',area,'Area leave')[1].strip()=='emit_signal("area_left", new_scene.get_region_name() != self.get_region_name())','Area leave behavior changed')
 require(not re.search(r'\[connection signal="area_left"',scene),'House area listeners require native bridge')
 region=node(scene,'.')['_region_name'];rootname=one(r'\[node name="([^"]+)" type="Node2D"\]',scene,'House root name')[1].replace("\\'","'")
 left=one(r'^signal (area_left)$',area,'Area left signal')[1]
 door_signals=[one(r'^signal '+name+r'$',script,'Door '+name)[0].split()[1]for name in ['entered','moved_player','done']]
 change=one(r'^signal (scene_changed)$',ex.text(field_door.GLOBAL),'Global scene changed')[1]
 flaggable=ex.text('Scripts/Main/FlaggableObject.gd');item=ex.text('Scripts/Main/ItemHolder.gd');present=ex.text('Scripts/Main/Present.gd');template=ex.text('Nodes/Overworld/Objects/Present.tscn')
 require('extends ItemHolder' in present and 'extends FlaggableObject' in item,'House source flaggable hierarchy changed')
 require('var reset_when_leaving_area := false' in flaggable and 'reset_when_leaving_region = false' in item,'House source item reset policy changed')
 require(not re.search(r'^reset_when_leaving_area\s*=\s*true',scene+'\n'+template,re.M),'House source reset-area listener needs execution')
 require(one(r'func _on_leave_area\(region_changed: bool\):\n([^\n]+)\n([^\n]+)',flaggable,'Flaggable area branch')[1].strip()=='if reset_when_leaving_area or (region_changed and reset_when_leaving_region):','House source area branch changed')
 require(region==node(ex.text(target),'.')['_region_name'],'House source region-change branch requires actual flag reset consumer')
 body=one(r'\[connection signal="body_entered" from="\." to="\." method="([^"]+)"\]',door,'Door body method')[1];require('func '+body+'(' in script,'House Door body source method missing')
 continuation=dict(region=region,root_name=rootname,area_left=left,door_signals=door_signals,scene_changed=change,area_script=AREA,empty_params=True,area_listeners=[],item_reset_area=False,region_changed=False)
 ir=dict(schema=3,commit=PIN,scene=SCENE,scene_id=stable('.'),source_sha256=ex.sources[SCENE],sources=ex.sources,scene_admitted=False,capabilities=5,body_method=body,continuation=continuation,ground_offset=offset,none=none,records=[record],audio=[dict(id=record['audio'],source=initial,bus=audio['bus'])],targets=[dict(id=record['id'],path=target)])
 write(IR,ir)
 write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=ex.sources,producer_sha256=sha(Path(__file__)),dependencies={name:sha(ROOT/name)for name in ['tools/field_door.py','tools/podunk_scene.py','tools/extract_battle_entry.py']},executor='ENCFDOR1 format3 family001f capability5',scope='Only original normal House Doors/Podunk. ready=0 is single-pack ordering, never a full-scene Ready certificate.',semantics=['Inherited Area2D native defaults monitoring=true/visible=true, explicit layer/mask/monitorable/pause_mode and native RectangleShape2D.', 'All source exports/defaults, source target scene fingerprint and initial AudioStreamPlayer/SFX are retained.', 'Existing FieldDoorRuntime actual onready/body signal/16 deferred scene steps/Fade endpoints execute; uncalled special guest remains rejected.', 'House contact uses its existing real source actor polygon overlap; binary binds exact transformed source rectangle.', 'Destination preparation is required before pause/flag/music/fade; loading this pack does not activate or grant Ready.']))
 return ir

def load(root=ROOT):
 ir=read(root/IR.relative_to(ROOT));rv=read(root/REVIEW.relative_to(ROOT));inv=read(root/'compatibility/upstream-inventory.json')['files']
 require(set(ir)==set('schema commit scene scene_id source_sha256 sources scene_admitted capabilities ground_offset none records audio targets continuation body_method'.split()),'House exit unknown metadata')
 require(ir['schema']==3 and ir['commit']==PIN and ir['scene']==SCENE and ir['scene_admitted'] is False and ir['capabilities']==5 and len(ir['records'])==1 and ir['records'][0]['node']==NODE,'House exit scope rejected')
 require(rv['commit']==PIN and rv['ir_sha256']==sha(root/IR.relative_to(ROOT)) and rv['sources']==ir['sources'] and rv['producer_sha256']==sha(root/'tools/house_exit_door.py'),'House exit review rejected')
 require(set(rv['dependencies'])=={'tools/field_door.py','tools/podunk_scene.py','tools/extract_battle_entry.py'},'House exit producer dependency closure rejected')
 for p,h in rv['dependencies'].items():require(sha(root/p)==h,'House exit producer changed '+p)
 for p,h in ir['sources'].items():require(h==inv[p]['sha256']==sha(root/'upstream/MOTHER-Encore'/p),'House exit source changed '+p)
 return ir

def binary(ir):
 strings=[];lookup={};blob=bytearray();formats=dict(field_door.FORMATS);formats[8]='10I';rows={k:[]for k in formats}
 def string(v):
  if v not in lookup:
   lookup[v]=len(strings);raw=v.encode();strings.append((len(blob),len(raw)));blob.extend(raw+b'\0')
  return lookup[v]
 scene=string(ir['scene'])
 for n in ir['records']:
  ints=[n[k]for k in ['id','ready']]+[string(n['node'])]+[n[k]for k in ['marker','audio','shape','layer','mask','flags','pause_mode']]+[string(n[k])for k in ['target_name','target_path','sound','end_sound','in_anim','out_anim','flag']]
  values=n['target']+n['direction']+n['speeds']+[n['music_fade']]+n['in_color']+n['out_color']+field_door.flat(n['body_transform'])+field_door.flat(n['marker_transform'])+n['shape_offset']+n['extents'];rows[3].append((*ints,*values))
 rows[4]=[(string(p),bytes.fromhex(h))for p,h in ir['sources'].items()];rows[5]=[(*[string(p)for p in [field_door.SCRIPT,field_door.TRANS,field_door.GLOBAL,field_door.FADE,ir['none']]],ir['ground_offset'])]
 rows[6]=[(n['id'],string(n['source']),string(n['bus']),bytes.fromhex(ir['sources'][n['source']]))for n in ir['audio']];rows[7]=[(n['id'],string(n['path']),bytes.fromhex(ir['sources'][n['path']]))for n in ir['targets']]
 c=ir['continuation'];require(c['empty_params']is True and c['area_listeners']==[] and len(c['door_signals'])==3,'House continuation unsupported closure')
 rows[8]=[(*[string(v)for v in [c['region'],c['root_name'],c['area_left'],*c['door_signals'],c['scene_changed'],c['area_script']]],1,0)];rows[9]=[(string(ir['body_method']),)];rows[1]=strings;rows[2]=[(v,)for v in blob]
 begin=128+24*len(formats);out=bytearray(begin);dirs=[]
 for k,fmt in formats.items():
  raw=b''.join(struct.pack('<'+fmt,*r)for r in rows[k]);dirs.append((k,len(rows[k]),struct.calcsize('<'+fmt),len(out),len(raw),0));out.extend(raw)
 struct.pack_into('<8s8I',out,0,b'ENCFDOR1',3,128,len(out),len(formats),zlib.crc32(out[begin:]),0x454e001f,5,ir['scene_id']);out[40:60]=bytes.fromhex(PIN);out[60:92]=bytes.fromhex(ir['source_sha256']);out[92:124]=hashlib.sha256(IR.read_bytes()).digest();struct.pack_into('<I',out,124,scene)
 for i,row in enumerate(dirs):struct.pack_into('<6I',out,128+24*i,*row)
 return bytes(out)

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
