#!/usr/bin/env python3
"""Actual original six-node Grass factory, source data only."""
from pathlib import Path
import argparse,hashlib,json,re,struct,sys,zlib
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,decode,sha,require
from tools import field_node_recipe as recipe
from tools.player_initialization import CLASSES
SCENE='Nodes/Overworld/Grass/grass.tscn';SCRIPT='Scripts/misc/grass.gd'
IR=ROOT/'content/native-grass.json';REVIEW=ROOT/'reports/grass-native/source-review.json';PACK=ROOT/'romfs/data/grass.encnative';FAMILY=0x454e0071
def stable(p):return int.from_bytes(hashlib.sha256(('recipe:'+SCENE+'#'+p).encode()).digest()[:4],'little')
def extract(directory,engine):
 d=read(directory/'grass-native.json');a=read(directory/'field_node_recipe.json');receipt=read(directory/'source.json');up=ROOT/'upstream/MOTHER-Encore';field=read(ROOT/'content/podunk-scene.json')
 require(d['source']=='res://'+SCENE and a['scene']==SCENE and a['scene_entered']is False and d['godot']['string']=='3.6.2-stable (official)'and len(d['nodes'])==len(a['nodes'])==6,'Grass complete native source scope')
 source={p:record['sha256']for p,record in receipt['files'].items()};source.update({p:sha(up/p)for p in [SCRIPT,'Scripts/misc/grass spawner.gd','Nodes/Overworld/Grass/grass spawner.tscn','Graphics/Objects/Grass/Podunk/1.png']})
 for p,h in source.items():require(sha(up/p)==h,'Grass changed source '+p)
 original=(up/SCRIPT).read_text(encoding='utf8');spawner=(up/'Scripts/misc/grass spawner.gd').read_text(encoding='utf8')
 require('newGrass.global_position = position'in spawner and spawner.index('add_child(newGrass)')<spawner.index('newGrass.set_grass')<spawner.index('currentGrass = newGrass'),'Grass instantiate/source order')
 require('yield(create_tween().tween_property($Sprite, "scale", Vector2.ONE, 0.2).from(Vector2(1, 0.8)), "finished")'in original and 'timer.queue_free()'in original,'Grass coroutine/tree exit source')
 require(re.findall(r'^var\s+(\w+)\s*=\s*(.*)$',original,re.M)==[('objects','[]')],'Grass complete source constructor declarations')
 nm={n['path']:n for n in d['nodes']};rows=[];paths=[n['path']for n in a['nodes']]
 require(paths==['.','CollisionShape2D','Sprite','AnimationPlayer','AnimationTree','Timer'],'Grass typed node roster changed')
 for n in a['nodes']:
  p=n['path'];props=decode(nm[p]['properties']);canvas=n['canvas'];flags=int(canvas);local=world=[[1,0],[0,1],[0,0]];color=selfcolor=[1,1,1,1];cp='';z=light=0
  require(n['class']==nm[p]['class']and not n['groups']and n['pause']==0 and n['priority']==0 and props['script']is None and not n['unique'],'Grass unsupported native/script override')
  if canvas:
   local=recipe.numbers(n['local']);world=recipe.numbers(n['world']);color=recipe.numbers(n['modulate']);selfcolor=recipe.numbers(n['self_modulate']);cp=n['canvas_parent'];z=n['z'];light=n['light_mask'];flags|=sum(int(n[k])<<(i+1)for i,k in enumerate(['visible','top_level','behind','use_parent_material','material','z_relative','y_sort','notify_transform','notify_local_transform']))
   require(not n['material']and not n['top_level'],'Grass source material/native top-level unsupported')
  rows.append(dict(id=stable(p),parent=stable(n['parent'])if n['parent']else 0,owner=stable(n['owner'])if n['owner']else 0,canvas_parent=stable(cp)if cp else 0,class_index=CLASSES.index(n['class']),native_class=n['class'],ready=5 if p=='.'else paths.index(p)-1,pause=0,flags=flags,light_mask=light,script_methods=129 if p=='.'else 0,index=n['index'],priority=0,z=z,local=local,world=world,modulate=color,self_modulate=selfcolor,node=p,name=n['name'],script=SCRIPT if p=='.'else'',script_sha=source[SCRIPT]if p=='.'else'0'*64,groups=[],native_generated=False))
 p={k:decode(v['properties'])for k,v in nm.items()};profile=field['profiles'][0];resources={v['id']:decode(v['properties'])for v in d['resources']if'properties'in v}
 require(p['.']['collision_layer']==profile['collision_layer']and p['.']['collision_mask']==profile['collision_mask']and p['.']['space_override']==0 and not p['.']['audio_bus_override'],'Grass native area/profile binding')
 require(p['CollisionShape2D']['position']==profile['collision_offset']and not p['CollisionShape2D']['one_way_collision']and resources[p['CollisionShape2D']['shape']['id']]['extents']==profile['collision_extents'],'Grass actual rectangle profile')
 require(p['Sprite']['offset']==profile['sprite_offset']and p['Sprite']['hframes']==4 and p['Sprite']['vframes']==1 and not p['Sprite']['region_enabled']and p['Sprite']['normal_map']is None and not p['Sprite']['flip_v'],'Grass Sprite source capability')
 require(p['Timer']['wait_time']==profile['idle_delay']and p['Timer']['one_shot']and not p['Timer']['autostart']and p['Timer']['process_mode']==1,'Grass actual Timer profile')
 require(not p['AnimationTree']['active']and p['AnimationTree']['process_mode']==1 and p['AnimationTree']['anim_player']['value']=='../AnimationPlayer'and not p['AnimationPlayer']['autoplay']and p['AnimationPlayer']['playback_process_mode']==1,'Grass sole native animation clock')
 # Existing admitted profile owns the complete discrete AnimationTree math.
 # Cross-check each clip instead of replacing its frame values with guesses.
 clip_frames={}
 for name,key in [('Idle',0),('Right',1),('Middle',2),('Left',3)]:
  v=resources[p['AnimationPlayer']['anims/'+name]['id']];require(v['tracks/0/type']=='value'and v['tracks/0/path']['value']=='Sprite:frame','Grass clip binding changed');keys=dict(v['tracks/0/keys']['pairs']);require(keys['times']==[0.0]and keys['transitions']==[1.0]and keys['update']==1 and len(keys['values'])==1 and v['length']==1.0 and not v['loop'],'Grass source discrete clip semantics');clip_frames[name]=keys['values'][0];require(clip_frames[name]==profile['frames'][key],'Grass source frame profile changed')
 require(resources[9]['blend_mode']==1 and [resources[9]['blend_point_'+str(i)+'/pos'] for i in range(3)]==profile['blend_points'],'Grass nearest blend source point order')
 connections=[]
 kinds={'_on_Grass_body_entered':1,'_on_Grass_body_exited':2,'_on_Timer_timeout':3,'_on_Grass_tree_exiting':4}
 for raw_connection in receipt['signal_connections']:
  c=dict(re.findall(r'(signal|from|to|method)="([^"]*)"',raw_connection['declaration']))
  require(c['method']in kinds,'Grass unknown source signal callback');connections.append(dict(emitter=stable(c['from']),receiver=stable(c['to']),signal=c['signal'],method=c['method'],kind=kinds[c['method']]))
 require(len(connections)==4,'Grass complete source signals')
 engines={}
 for path,filename in [('scene/animation/scene_tree_tween.cpp','scene_tree_tween.cpp'),('scene/animation/scene_tree_tween.h','scene_tree_tween.h'),('modules/gdscript/gdscript_function.cpp','gdscript_function.cpp'),('scene/main/scene_tree.cpp','scene_tree.cpp')]:
  q=engine/filename;require(q.is_file(),'Reviewed Grass source tween engine absent');engines[path]=dict(sha256=sha(q),url='https://github.com/godotengine/godot/blob/3.6.2-stable/'+path)
 allsources={**source,**{k:v['sha256']for k,v in engines.items()}}
 result=dict(schema=1,format=1,family=FAMILY,capability=1,rules=1,commit=PIN,scene=SCENE,scene_id=stable('.'),source_sha256=source[SCENE],sources=source,engine=engines,records=rows,classes=CLASSES,controls=[],canvas_layers=[],profile=profile,field_ir_sha256=sha(ROOT/'content/podunk-scene.json'),native_sha256=sha(directory/'grass-native.json'),tree_export_sha256=sha(directory/'field_node_recipe.json'),exporter_sha256=sha(ROOT/'tools/godot_exporter/grass_node_recipe.gd'),script=SCRIPT,connections=connections,nodes=[stable(p)for p in paths],monitoring=p['.']['monitoring'],monitorable=p['.']['monitorable'],disabled=p['CollisionShape2D']['disabled'],centered=p['Sprite']['centered'],tween_source='scene/animation/scene_tree_tween.cpp',tween_signal='finished',scene_admitted=False)
 result['sources']=allsources;write(IR,result);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=source,engine=engines,exporter_sha256=result['exporter_sha256'],source_ready_granted=False));return result
def load():
 d=read(IR);r=read(REVIEW);require(d['commit']==PIN and d['family']==FAMILY and d['format']==d['capability']==d['rules']==1 and r['ir_sha256']==sha(IR),'Grass source review/version rejected')
 require(d['field_ir_sha256']==sha(ROOT/'content/podunk-scene.json')and d['exporter_sha256']==sha(ROOT/'tools/godot_exporter/grass_node_recipe.gd'),'Grass source producer dependencies changed')
 for p,h in r['sources'].items():require(sha(ROOT/'upstream/MOTHER-Encore'/p)==h,'Grass source changed '+p)
 return d
def encode(d):
 recipe.CLASSES=CLASSES;recipe.IR=IR;sub=bytearray(recipe.encode(d));struct.pack_into('<I',sub,28,6)
 b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def f(*v):b.extend(struct.pack('<'+'f'*len(v),*v))
 def t(s):v=s.encode();u(len(v));b.extend(v)
 u(len(sub));b.extend(sub);u(*d['nodes']);t(d['script']);t(d['tween_source']);t(d['tween_signal']);u(d['monitoring'],d['monitorable'],d['disabled'],d['centered']);p=d['profile'];u(p['stable_id'],p['collision_layer'],p['collision_mask']);f(*p['collision_extents'],*p['sprite_offset'],*p['collision_offset'],p['idle_delay'],p['squash'],p['blend_divisor'],p['enter_tween'],p['exit_tween']);u(*p['frames']);u(len(d['connections']))
 for c in d['connections']:u(c['emitter'],c['receiver'],c['kind']);t(c['signal']);t(c['method'])
 struct.pack_into('<8s8I',b,0,b'ENCGRS01',1,128,len(b),zlib.crc32(b[128:]),FAMILY,1,1,d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=bytes.fromhex(sha(IR));return bytes(b)
def stage_files(root):
 b=encode(load());require((Path(root)/'data/grass.encnative').read_bytes()==b,'Grass generated stage bytes changed');return {Path('data/grass.encnative'):b}
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile']);p.add_argument('--directory',type=Path);p.add_argument('--engine',type=Path);a=p.parse_args()
 try:
  d=extract(a.directory,a.engine)if a.action=='extract'else load();PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(encode(d));print('Grass actual factory:',len(d['records']),'native nodes;',len(d['connections']),'source connections; no whole scene admission')
 except(ValueError,KeyError,OSError,TypeError,struct.error)as e:sys.exit('GRASS NATIVE ERROR: '+str(e))
