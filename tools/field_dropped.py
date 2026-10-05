#!/usr/bin/env python3
"""Pinned Podunk DroppedItem source, binary, real tex3ds and lifecycle data."""
from __future__ import annotations
import argparse,json,hashlib,re,math,struct,sys,zlib,subprocess
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.field_present import source_instances
from tools.podunk_scene import decode,stable,SCENE,PIN,read,write,sha,require
from tools.extract_battle_entry import Extractor,node,animation
from tools.asset_receipts import receipt_path
IR=ROOT/'content/native-field-dropped.json';REVIEW=ROOT/'reports/field-dropped/source-review.json';PACK=ROOT/'romfs/data/podunk-dropped.encdrop'
SCRIPT='Scripts/Main/DroppedItem.gd';SCENE_ITEM='Nodes/Overworld/Objects/DroppedItem.tscn'
ENGINE=[dict(url='https://raw.githubusercontent.com/godotengine/godot/3.6.2-stable/scene/animation/easing_equations.h',sha256='ec67f651cb7c723652d2968738633b0d4f3c01fb813437aec812cdec3db6f7be'),dict(url='https://raw.githubusercontent.com/godotengine/godot/3.6.2-stable/scene/animation/scene_tree_tween.cpp',sha256='c0272f420342bf8606602c31ea935d9d05b6093190a77128889ec362483693a4'),dict(url='https://raw.githubusercontent.com/godotengine/godot/3.6.2-stable/scene/animation/tween.cpp',sha256='0d8cb57f288bbbd46a9c0e176756dcf5e51fedcc49a7faef7ca665bdcccea38c')]
def build(records,nodes,resources,provenance):
 ex=Extractor(ROOT);ex.sources.update(provenance['source_files']);script=ex.text(SCRIPT);scene=ex.text(SCENE_ITEM);holder=ex.text('Scripts/Main/ItemHolder.gd');ex.text('Scripts/Main/FlaggableObject.gd');ex.text('Scripts/global/Inventory.gd');ex.text('Scripts/global/Item.gd');ex.text('Scripts/UI/Button Prompt.gd');ex.text('Nodes/Ui/ButtonPrompt.tscn');ex.text('LICENSE')
 for fact in ['yield(get_tree(), "idle_frame")','global.get_player().connect("paused"','yield($Timer,"timeout")','button_prompt_node.force_show()','button_prompt_node.press_button()','button_prompt_node.connect("hide", button_prompt_node, "queue_free")','$interact.remove_child(button_prompt_node)','get_parent().add_child(button_prompt_node)','button_prompt_node.position = position + button_prompt_node.offset','._check_item()','$Tween.start()','yield(tween, "finished")','if reset_when_consumed:','_set_flag_status(false)']:require(fact in script,'Unknown DroppedItem source '+fact)
 require(script.count('yield($Timer,"timeout")')==4 and script.count('queue_free()')==4,'Dropped coroutine source topology')
 timings=[float(x)for x in re.findall(r'\$Timer.start\(([\d.]+)\)',script)];speeds=[float(x)for x in re.findall(r'\$AnimationPlayer.playback_speed = ([\d.]+)',script)];require(len(timings)==4 and len(speeds)==2,'Dropped disappear source stages')
 position=re.search(r'tween_property\(self, "global_position", global.get_player\(\).global_position, ([\d.]+)\)[\s\\]*\.set_trans\(Tween.TRANS_QUART\)\.set_ease\(Tween.EASE_OUT\)\.set_delay\(([\d.]+)\)',script);scale=re.search(r'tween_property\(self, "scale", Vector2\(([\d.-]+), ([\d.-]+)\), ([\d.]+)\)[\s\\]*\.from\(Vector2\(([\d.-]+), ([\d.-]+)\)\)\.set_trans\(Tween.TRANS_QUAD\)\.set_ease\(Tween.EASE_IN\)',script);rotation=re.search(r'interpolate_property\(\$Sprite, "rotation_degrees", ([\d.-]+), ([\d.-]+), ([\d.]+), Tween.TRANS_ELASTIC,Tween.EASE_OUT\)',script);require(position and scale and rotation,'Dropped source Tween capability')
 player=node(scene,'AnimationPlayer');blink=animation(scene,player['anims/blink']['SubResource'],SCENE_ITEM,'blink');require(blink['loop'] and len(blink['tracks'])==1,'Dropped blink topology');track=blink['tracks'][0];require(track['path']=='Sprite:visible'and track['keys']['update']==1 and track['enabled'],'Dropped source blink target')
 from tools.present_sparkles import build as sparkle_build
 sp=sparkle_build(ROOT);ex.sources.update(sp['sources']);sparkles=dict(animation=sp['animation'],resource=sp['resource'],engine=sp['engine_reference'])
 children={p:[]for p in nodes}
 for p in nodes:
  if p!='.':children[p.rsplit('/',1)[0]if'/'in p else'.'].append(p)
 ready=[]
 def visit(p):
  for c in children[p]:visit(c)
  ready.append(p)
 visit('.');ordinal={p:i for i,p in enumerate(ready)}
 def geometry(path):
  n=nodes[path];p=decode(n['properties']);shape=resources[p['shape']['id']];require(shape['class']=='RectangleShape2D','Dropped unsupported shape');v=decode(shape['properties'])['extents'];w=decode(n['world_transform']);require(w[0][1]==w[1][0]==0 and w[0][0]>0 and w[1][1]>0,'Dropped collision transform');return[w[2][0],w[2][1],v[0]*w[0][0],v[1]*w[1][1]]
 bindings=[];items={};assets=[];outer=ex.text(SCENE)
 require(not re.search(r'parent="Objects/Items/(?:Item|Item2|FavFood)(?:/interact)?"',outer),'Dropped child override requires typed source adapter')
 for a in records:
  if a['script']!=SCRIPT:continue
  path=a['node'];o=a['overrides'];item=o.get('item','');require(item,'Dropped static source item absent');data=ex.yaml('Data/Items/'+item+'.yaml');items[item]=dict(key=item,source='Data/Items/'+item+'.yaml',keyitem=bool(data.get('keyitem',False)),doses=data.get('doses',1))
  art='Graphics/Objects/Items/'+item+'.png';size=ex.png_size(art);ex.data(art+'.import');asset=dict(id=stable('source:'+art),source=art,path='graphics/objects/dropped/'+str(stable('source:'+art))+'.t3x',width=size[0],height=size[1]);assets.append(asset)
  wt=decode(a['native']['world_transform']);require(wt[0][1]==wt[1][0]==0 and wt[0][0]>0 and wt[1][1]>0,'Dropped rotated parent requires adapter');visual=decode(nodes[path+'/Sprite']['properties']);spv=decode(nodes[path+'/Sparkles']['properties']);require(visual['centered'] and visual['offset']==[0,0]and visual['position']==[0,0]and visual['rotation']==0 and visual['scale']==[1,1]and visual['hframes']==visual['vframes']==1 and not visual['flip_h']and not visual['flip_v']and not visual['region_enabled'],'Dropped sprite capability');require(spv['centered']and spv['offset']==[0,0]and spv['rotation']==0 and spv['scale']==[1,1]and spv['playing']and spv['speed_scale']==sparkles['animation']['speed_scale'],'Dropped Sparkles lifecycle')
  timer=decode(nodes[path+'/Timer']['properties']);anim=decode(nodes[path+'/AnimationPlayer']['properties']);require(timer['process_mode']==1 and not timer['autostart']and not timer['one_shot']and anim['playback_speed']==player['playback_speed'],'Dropped serialized Timer/AnimationPlayer source');rawflag=o.get('flag','');prompt=node(scene,'interact/ButtonPrompt');dialog=o.get('dialog','')or'ItemDialogue/itemcheck';full=o.get('dialog_full','')or'ItemDialogue/itemfull';empty=o.get('dialog_empty','');ex.text('Data/Dialogue/'+dialog+'.yaml');ex.text('Data/Dialogue/'+full+'.yaml');ex.text('Data/Dialogue/ItemDialogue/presentempty.yaml')
  bindings.append(dict(id=a['stable_id'],node=path,ready_ordinal=a['ready_ordinal'],sparkles_id=stable(path+'/Sparkles'),sparkles_ready=ordinal[path+'/Sparkles'],prompt_id=stable(path+'/interact/ButtonPrompt'),tween_id=stable(path+'/Tween'),timer_id=stable(path+'/Timer'),animation_id=stable(path+'/AnimationPlayer'),tween_ordinal=ordinal[path+'/Tween'],timer_ordinal=ordinal[path+'/Timer'],animation_ordinal=ordinal[path+'/AnimationPlayer'],asset_id=asset['id'],flag=rawflag or nodes['.']['name']+'/'+a['native']['name'],object_flag=o.get('is_object_flag',False)or not rawflag,emit=o.get('emit_flag_updated_signal',False),reset_area=o.get('reset_when_leaving_area',False),reset_consumed=o.get('reset_when_consumed',False),can_pickup=o.get('can_pickup',True),item=item,dialogue=dialog,full=full,empty=empty,position=wt[2],scale=[wt[0][0],wt[1][1]],local_position=decode(a['native']['properties'])['position'],sparkles_offset=spv['position'],prompt_offset=prompt['offset'],collision=geometry(path+'/StaticBody2D/CollisionShape2D'),interaction=geometry(path+'/interact/CollisionShape2D'),collision_layer=decode(nodes[path+'/StaticBody2D']['properties'])['collision_layer'],interaction_layer=decode(nodes[path+'/interact']['properties'])['collision_layer']))
 require(len(bindings)==3,'Dropped Podunk instance loss')
 return dict(schema=1,kind='encore.field-dropped.source-ir',commit=PIN,scene=SCENE,script=SCRIPT,bindings=bindings,items=list(items.values()),assets=assets,sparkles=sparkles,timers=timings,speeds=speeds,initial_speed=player['playback_speed'],blink=dict(length=blink['length'],keys=[dict(time=t,value=bool(v))for t,v in zip(track['keys']['times'],track['keys']['values'])]),collect=dict(position_duration=float(position[1]),position_delay=float(position[2]),scale_duration=float(scale[3]),scale_to=[float(scale[1]),float(scale[2])],scale_from=[float(scale[4]),float(scale[5])],position_trans=3,position_ease=1,scale_trans=4,scale_ease=0),revert=dict(initial=float(rotation[1]),final=float(rotation[2]),duration=float(rotation[3]),transition=6,ease=1),empty_message=re.search(r'uiManager.open_dialogue_box\("(ItemDialogue/presentempty)"\)',holder)[1],sources=dict(sorted(ex.sources.items())),provenance=provenance,engine=ENGINE,semantics=['Three actual Podunk DroppedItems with inherited/outer overrides and source child-before-parent Ready, no static replacement','Child Sparkles Ready consumes the same global RNG before parent collected flag may queue_free; parent hide then one tree idle_frame signal before show','Inherited ItemHolder grant/UID/global.item/flag/dialogue source order; all paths then start legacy Tween; full path creates an unowned UID item context','Collection force-show prompt, conditional free or press/connect hide-to-free/reparent/local position; SceneTreeTween captures player destination once and interpolates source quart-out position and quad-in scale','Disappear coroutine FIFO waiters share one Timer; start5/2/2/3, player pause only pauses Timer; source initial AnimationPlayer speed0 retained; timeout changes speed2/3 and queues deletion','Timer timeout uses negative time_left, repeating Timer rollover before waiter callbacks; SceneTreeTweens advance after idle node processing; engine Tween/easing review references retained','Source exit_tree resets consumed flag only when its source property true; queued deletion remains visible until scene host flush lifecycle'],unsupported=['Real inventory definitions/UID clock/global.item context, source Room dialogue and ButtonPrompt Ready/press/reparent/hide must pass typed live host admission','Runtime factory set_item outside the three serialized Podunk instances remains explicit unsupported; source built-in Error texture editor fallback is not used in game'],unverified=['Manual tests not run','3DS renderer/SDK assets and integrated scene admission not yet verified'])
def validate(d):
 require(d['schema']==1 and d['kind']=='encore.field-dropped.source-ir'and d['commit']==PIN and d['scene']==SCENE and len(d['bindings'])==3 and len(d['timers'])==4 and len(d['speeds'])==2,'Dropped schema/capability')
 ids=set();items={x['key']for x in d['items']};assets={x['id']for x in d['assets']}
 for b in d['bindings']:
  require(b['id']==stable(b['node'])and b['id']not in ids and b['sparkles_id']==stable(b['node']+'/Sparkles')and b['sparkles_ready']<b['ready_ordinal']and b['asset_id']in assets and b['item']in items,'Dropped source identity');ids.add(b['id'])
  for k in ['position','scale','local_position','sparkles_offset','prompt_offset','collision','interaction']:require(all(type(v)in(int,float)and math.isfinite(v)and abs(v)<=1e6 for v in b[k]),'Dropped finite geometry')
 return d
def load():
 d=validate(read(IR));r=read(REVIEW);require(r['commit']==PIN and r['ir_sha256']==sha(IR)and r['sources']==d['sources'],'Dropped source review')
 inventory=read(ROOT/'compatibility/upstream-inventory.json')['files']
 for p,h in d['sources'].items():require(sha(ROOT/'upstream/MOTHER-Encore'/p)==h==inventory[p]['sha256'],'Changed Dropped source '+p)
 return d
def encode(d):
 validate(d);b=bytearray(64)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def f(*v):b.extend(struct.pack('<'+'f'*len(v),*v))
 def t(s):v=s.encode();u(len(v));b.extend(v)
 t(d['scene']);t(d['script']);t(d['empty_message']);f(*d['timers'],*d['speeds'],d['initial_speed']);c=d['collect'];f(c['position_duration'],c['position_delay'],c['scale_duration'],*c['scale_from'],*c['scale_to']);u(c['position_trans'],c['position_ease'],c['scale_trans'],c['scale_ease']);r=d['revert'];f(r['initial'],r['final'],r['duration']);u(r['transition'],r['ease']);f(d['blink']['length']);u(len(d['blink']['keys']))
 for k in d['blink']['keys']:f(k['time']);u(int(k['value']))
 sp=d['sparkles'];a=sp['animation'];resource=sp['resource'];from tools.present_sparkles import verify_receipt
 verify_receipt(ROOT);raw=(ROOT/'romfs'/resource['output']).read_bytes();t(resource['output']);u(*resource['size'],len(raw),zlib.crc32(raw)&0xffffffff);f(a['speed'],a['speed_scale'],*a['random_range']);u(len(a['frames']))
 for v in a['frames']:u(*v)
 u(len(d['items']))
 for i in d['items']:t(i['key']);t(i['source']);u(int(i['keyitem']),i['doses'])
 u(len(d['assets']));r=read(receipt_path(ROOT/'romfs/graphics/objects/dropped',ROOT));require(r['commit']==PIN and r['recipe_sha256']==sha(IR),'Dropped genuine tex3ds receipt')
 for a in d['assets']:
  raw=(ROOT/'romfs'/a['path']).read_bytes();require(r['outputs'][a['path']]['sha256']==hashlib.sha256(raw).hexdigest(),'Dropped texture receipt');u(a['id'],a['width'],a['height'],len(raw),zlib.crc32(raw)&0xffffffff);t(a['source']);t(a['path'])
 u(len(d['bindings']))
 for h in d['bindings']:
  flags=sum(int(h[k])<<i for i,k in enumerate(['object_flag','emit','reset_area','reset_consumed','can_pickup']));u(h['id'],h['ready_ordinal'],h['sparkles_id'],h['sparkles_ready'],h['prompt_id'],h['tween_id'],h['timer_id'],h['animation_id'],h['tween_ordinal'],h['timer_ordinal'],h['animation_ordinal'],h['asset_id'],flags,h['collision_layer'],h['interaction_layer'])
  for k in ['node','flag','item','dialogue','full','empty']:t(h[k])
  for k in ['position','scale','local_position','sparkles_offset','prompt_offset','collision','interaction']:f(*h[k])
 u(len(d['sources']))
 for p,h in d['sources'].items():t(p);b.extend(bytes.fromhex(h))
 struct.pack_into('<8s6I20s12x',b,0,b'ENCDRP01',1,len(b),0,1,1,len(d['bindings']),bytes.fromhex(PIN));struct.pack_into('<I',b,16,zlib.crc32(b)&0xffffffff);return bytes(b)
def assets(tex):
 d=load()
 def convert(a):
  out=ROOT/'romfs'/a['path'];out.parent.mkdir(parents=True,exist_ok=True);subprocess.run([str(tex),'-f','rgba8','-z','none','-o',str(out),str(ROOT/'upstream/MOTHER-Encore'/a['source'])],check=True);return a['path'],dict(bytes=out.stat().st_size,sha256=sha(out))
 with ThreadPoolExecutor(max_workers=4)as pool:outputs=dict(pool.map(convert,d['assets']))
 write(receipt_path(ROOT/'romfs/graphics/objects/dropped',ROOT),dict(schema=1,commit=PIN,recipe_sha256=sha(IR),tex3ds_sha256=sha(tex),outputs=outputs))
def stage_files(source):
 d=load();b=encode(d);require((Path(source)/'data/podunk-dropped.encdrop').read_bytes()==b,'Stale Dropped binary');out={Path('data/podunk-dropped.encdrop'):b}
 for a in d['assets']:
  p=Path(a['path']);raw=(Path(source)/p).read_bytes();require(raw==(ROOT/'romfs'/p).read_bytes(),'Stale Dropped texture');out[p]=raw
 return out
def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','assets']);p.add_argument('--native',type=Path);p.add_argument('--source',type=Path);p.add_argument('--tex3ds',type=Path);a=p.parse_args()
 if a.action=='extract':
  require(a.native and a.source,'Actual full Podunk export/receipt required');args=source_instances(a.native,a.source,ROOT/'upstream/MOTHER-Encore');d=validate(build(*args));write(IR,d);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=d['sources'],engine=d['engine'],semantics=d['semantics'],unsupported=d['unsupported']));print('Dropped source extraction: three original Podunk instances');return
 if a.action=='assets':require(a.tex3ds and a.tex3ds.is_file(),'Real tex3ds required');assets(a.tex3ds);return
 b=encode(load());PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(b);print('Dropped checked resource:',len(b),'bytes')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,OSError,TypeError,struct.error)as e:sys.exit('FIELD DROPPED ERROR: '+str(e))
