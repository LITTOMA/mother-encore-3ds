#!/usr/bin/env python3
"""Checked actual DialogueBox visual nodes; no whole-scene admission."""
from __future__ import annotations
import argparse,copy,hashlib,re,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,decode,sha,require
from tools import field_camera_arrows as arrows,field_game_camera as camera
from tools.field_dialogue_visual_source import extract_arrows,extract_camera,source_context,stable,SCENE,ARROWS_IR,CAMERA_IR
IR=ROOT/'content/native-field-dialogue-visual.json';REVIEW=ROOT/'reports/field-dialogue-visual/source-review.json';OUT=ROOT/'romfs/data/podunk-dialogue-visual.encdvisual'
CURSOR='Scripts/UI/cursor.gd'
def extract(native):
 extract_arrows(native);extract_camera(native);context=source_context(native);recipe=read(ROOT/'content/dialogue-node-recipe.json');d=read(native);nm={n['path']:n for n in d['nodes']};resources={r['id']:r for r in d['resources']};records={r['node']:r for r in recipe['records']};states={s['source'][6:]:s for s in d['scene_states']}
 source=(ROOT/'upstream/MOTHER-Encore'/CURSOR).read_text(encoding='utf8');require(all("play_sfx('"+name+"')"in source for name in ['cursor1','cursor2','back']),'Cursor source sound binding changed')
 flags=['on','consume_input_events','_exact_dir_input','loop_around','skip_empty_labels','skip_hidden_items','highlight','move_sfx','select_sfx','cancel_sfx','cancel_on','reset_on_show','dont_kill_tween']
 defaults={k:re.search(r'export var '+k+r' := (true|false)',source)[1]=='true' for k in flags}
 size=re.search(r'export var _cursor_size := Vector2\(([^)]+)\)',source);size=[float(v)for v in size[1].split(',')]
 length=float(re.search(r'const TWEEN_LENGTH := ([0-9.]+)',source)[1]);timer=decode(nm['Dialoguebox/Arrow/Timer']['properties'])['wait_time'];require('$Timer.start()'in source,'Cursor default Timer start changed');parts=[];cursors=[]
 for path in ['Dialoguebox/Arrow','Dialoguebox/Cursor_Down']:
  props={};props.update(decode(next(n for n in states['Nodes/Ui/arrow.tscn']['nodes']if n['path']=='.')['properties']));props.update(decode(next(n for n in states[SCENE]['nodes']if n['path']=='./'+path)['properties']));v=decode(nm[path]['properties']);policy={k:props.get(k,defaults[k])for k in flags};require(not policy['highlight'] and not policy['loop_around'] and not policy['consume_input_events'] and not policy['_exact_dir_input'] and not policy['select_sfx'] and not policy['cancel_sfx'] and not policy['cancel_on'] and not policy['dont_kill_tween'],'Dialogue Cursor contains unreviewed policy')
  frame=decode(resources[v['frames']['id']]['properties'])['animations'][0];frame=dict(frame['pairs']);require(frame['name']=='Idle'and frame['loop']and len(frame['frames'])==4,'Dialogue cursor frame schema differs')
  atlas=[];original=(ROOT/'upstream/MOTHER-Encore/Nodes/Ui/arrow.tscn').read_text(encoding='utf8');regions={int(i):[float(v)for v in re.search(r'region = Rect2\( ([^)]*) \)',body)[1].split(',')]for i,body in re.findall(r'^\[sub_resource type="AtlasTexture" id=(\d+)\]\n(.*?)(?=^\[|\Z)',original,re.M|re.S)}
  for ref in frame['frames']:
   q=resources[ref['id']];require(q['class']=='AtlasTexture'and q['path'].startswith('res://Nodes/Ui/arrow.tscn::'),'Cursor atlas source differs');region=regions[int(q['path'].split('::')[1])];require(decode(q['size'])==region[2:],'Cursor native atlas extent differs');atlas.append(region)
  require(atlas==read(ARROWS_IR)['frames'],'Dialogue cursor atlas differs from checked GPU source')
  a=decode(nm[path+'/AnimationPlayer']['properties']);clips=[]
  for role,name in enumerate(['Point','UnPoint','RESET'],1):
   vclip=decode(resources[a['anims/'+name]['id']]['properties']);tracks=[];i=0
   while 'tracks/'+str(i)+'/type' in vclip:
    pre='tracks/'+str(i)+'/';keys=dict(vclip[pre+'keys']['pairs']);node,prop=vclip[pre+'path']['value'].split(':');require(node=='.'and prop in ['offset','playing','frame']and vclip[pre+'type']=='value'and vclip[pre+'interp']==1 and vclip[pre+'enabled']and vclip[pre+'loop_wrap'],'Dialogue cursor animation changed');value=lambda v:v if type(v)is list else [float(v),0];tracks.append(dict(target=0,property={'frame':1,'offset':2,'playing':3}[prop],update=keys['update'],keys=[dict(time=t,transition=e,value=value(x))for t,e,x in zip(keys['times'],keys['transitions'],keys['values'])]));i+=1
   clips.append(dict(role=role,name=name,length=vclip['length'],loop=vclip['loop'],tracks=tracks))
  child=decode(nm[path+'/Sprite']['properties']);require(child['material']is None and not child['use_parent_material']and resources[child['texture']['id']]['path']=='res://Graphics/UI/Inventory/cursor.png','Cursor Sprite texture/material differs')
  cursors.append(dict(id=stable(path),sprite=stable(path+'/Sprite'),player=stable(path+'/AnimationPlayer'),timer=stable(path+'/Timer'),menu=props.get('menu_parent_path',dict(value=''))['value'],offset=props['cursor_offset'],size=size,flags=sum(int(policy[k])<<i for i,k in enumerate(flags)),position=v['position'],rotation=v['rotation'],scale=v['scale'],drawing_offset=v['offset'],frame=props.get('frame',0),fps=frame['speed']*v['speed_scale'],visible=v['visible'],playing=v['playing'],centered=v['centered'],clips=clips,static_sprite=dict(position=child['position'],offset=child['offset'],scale=child['scale'],rotation=child['rotation'],flags=sum(int(child[k])<<i for i,k in enumerate(['visible','centered','flip_h','flip_v'])))))
 allowed=['AnimatedSprite','Sprite','AnimationPlayer','Camera2D','Area2D','CollisionShape2D','Node2D']
 for r in recipe['records']:
  if r['native_class']not in allowed or r['node'] in ['AnimationPlayer','NameAnim']:continue
  parts.append({k:r[k]for k in ['id','parent','node','native_class','script','ready','pause','priority']})
 require(len(parts)==20 and len(cursors)==2,'Dialogue visuals exact20 scope differs')
 sources=context['sources'];sources[CURSOR]=sha(ROOT/'upstream/MOTHER-Encore'/CURSOR)
 write(IR,dict(schema=1,kind='encore.field-dialogue-visual.source-ir',commit=PIN,scene=SCENE,scene_id=recipe['scene_id'],source_sha256=recipe['source_sha256'],recipe_sha256=sha(ROOT/'content/dialogue-node-recipe.json'),native_sha256=sha(native),sources=sources,records=parts,cursors=cursors,tween_length=length,timer_length=timer,transition=re.search(r'set_trans\(Tween\.([A-Z_]+)\)',source)[1],ease=re.search(r'set_ease\(Tween\.([A-Z_]+)\)',source)[1],actions=['ui_accept','ui_cancel','ui_toggle'],sounds=['cursor1','cursor2','back'],signals=['activated','moved','failed_move','selected','failed_select','cancel','frame_changed','animation_finished','visibility_changed','item_rect_changed','locale_changed'],scene_admitted=False,pending=['Actual global viewport/canvas/physics/signal/Timer/tween owners required per actual factory ObjectID','DialogueBox/AbstractDialogueBox phrase execution remains separate owner','Highlight/frames-null and general parent-changing Cursor calls are not used by this recipe and reject before mutation']))
 write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),native_sha256=sha(native),recipe_sha256=sha(ROOT/'content/dialogue-node-recipe.json'),camera_ir_sha256=sha(CAMERA_IR),arrows_ir_sha256=sha(ARROWS_IR),producer_sha256=sha(Path(__file__)),source_producer_sha256=sha(ROOT/'tools/field_dialogue_visual_source.py'),borrowed_assets_sha256=sha(arrows.ASSETS),scope='20 actual DialogueBox visual native bodies; source Cursor2/GameCamera1/MapArrows1; no sceneReady grant',sources=sources))
def load():
 d=read(IR);r=read(REVIEW);require(r['producer_sha256']==sha(Path(__file__))and r['source_producer_sha256']==sha(ROOT/'tools/field_dialogue_visual_source.py')and r['borrowed_assets_sha256']==sha(arrows.ASSETS),'Dialogue visual producer/asset receipt mismatch');arrows.assets(arrows.load(),None);require(d['commit']==PIN and d['schema']==1 and not d['scene_admitted']and r['ir_sha256']==sha(IR)and r['recipe_sha256']==sha(ROOT/'content/dialogue-node-recipe.json')and r['camera_ir_sha256']==sha(CAMERA_IR)and r['arrows_ir_sha256']==sha(ARROWS_IR),'Dialogue visual source review mismatch')
 inv=read(ROOT/'compatibility/upstream-inventory.json')['files']
 for p,h in d['sources'].items():require(h==inv[p]['sha256']==sha(ROOT/'upstream/MOTHER-Encore'/p),'Changed visual source '+p)
 recipe=read(ROOT/'content/dialogue-node-recipe.json');records={v['id']:v for v in recipe['records']}
 for ir in [read(CAMERA_IR),read(ARROWS_IR)]:
  for v in ir['records']:require(v['id']in records and v['parent_id']==records[v['id']]['parent']and v['ready']==records[v['id']]['ready'],'Dialogue camera/arrows actual parent/Ready mismatch')
 return d
def encode(d):
 b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def f(*v):b.extend(struct.pack('<'+'f'*len(v),*v))
 def text(s):v=s.encode();u(len(v));b.extend(v)
 b.extend(bytes.fromhex(d['recipe_sha256']));f(d['tween_length'],d['timer_length']);text(d['transition']);text(d['ease'])
 for seq in ['actions','sounds','signals']:
  u(len(d[seq]));[text(s)for s in d[seq]]
 for n in d['records']:u(n['id'],n['parent'],n['ready'],n['pause']);b.extend(struct.pack('<i',n['priority']));text(n['node']);text(n['native_class']);text(n['script'])
 u(len(d['cursors']))
 for c in d['cursors']:
  u(c['id'],c['sprite'],c['player'],c['timer'],c['flags'],c['frame'],int(c['visible']),int(c['playing']),int(c['centered']));text(c['menu']);f(*c['offset'],*c['size'],*c['position'],c['rotation'],*c['scale'],*c['drawing_offset'],c['fps']);s=c['static_sprite'];u(s['flags']);f(*s['position'],*s['offset'],*s['scale'],s['rotation']);u(len(c['clips']))
  for clip in c['clips']:
   u(clip['role']);text(clip['name']);f(clip['length']);u(int(clip['loop']),len(clip['tracks']))
   for tr in clip['tracks']:
    u(tr['property'],tr['update'],len(tr['keys']))
    for k in tr['keys']:f(k['time'],k['transition'],*k['value'])
 old_a,old_c=arrows.IR,camera.IR
 try:
  arrows.IR=ARROWS_IR;camera.IR=CAMERA_IR
  raw_a=arrows.encode(read(ARROWS_IR),read(arrows.ASSETS));raw_c=camera.encode(read(CAMERA_IR))
 finally:arrows.IR, camera.IR=old_a,old_c
 for raw in [raw_a,raw_c]:u(len(raw));b.extend(raw)
 u(len(d['sources']))
 for p,h in d['sources'].items():text(p);b.extend(bytes.fromhex(h))
 struct.pack_into('<8s8I',b,0,b'ENCFDVS1',1,128,len(b),zlib.crc32(b[128:]),0x454e0046,1,len(d['records']),d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=bytes.fromhex(sha(IR));return bytes(b)
def stage_files(source):
 raw=encode(load());require((Path(source)/'data/podunk-dialogue-visual.encdvisual').read_bytes()==raw,'Staged dialogue visuals differ');return {Path('data/podunk-dialogue-visual.encdvisual'):raw}
def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);p.add_argument('--native',type=Path);a=p.parse_args()
 if a.action=='extract':require(a.native,'Explicit complete official native export required');extract(a.native);return
 raw=encode(load())
 if a.action=='verify':require(OUT.read_bytes()==raw,'Dialogue visuals stale')
 else:OUT.parent.mkdir(parents=True,exist_ok=True);OUT.write_bytes(raw)
 print('Dialogue visual resource:',len(raw),'bytes; 20 native nodes; complete scene still requires independent owners')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,TypeError,OSError,struct.error)as e:sys.exit('FIELD DIALOGUE VISUAL ERROR: '+str(e))
