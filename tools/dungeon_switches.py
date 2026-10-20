"""Source-owned dungeon switch graph candidate; no live scene admission.

Captures flags, control edges, camera policy and source timing parameters.
It does not execute GDScript or admit animation/audio/camera consumers.
"""
from __future__ import annotations
import argparse,hashlib,json,math,posixpath,re,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor,PIN,require
from tools.godot_text import instantiate,NodePathValue,Vec2
from tools.podunk_field import Sources,pack_container,unpack_container
from tools.dungeon_keys import fields,path,function,ordered
RECIPE=ROOT/'content/catacombs-switches.json'
MAGIC=b'ENCDSW01';STRIDES=(1,48,64,48,8);LIMIT=65536
SELECTORS='scene lever plate gate wall switch_script list_script abstract_script flag_script object_script two_state_script gate_script wall_script scene_script area_script globaldata_script'

def real(v):
 require(type(v)in(int,float)and math.isfinite(v)and abs(v)<=1000000,'Dungeon switch finite real')
 return struct.unpack('<f',struct.pack('<f',v))[0]
def read(file=RECIPE):
 def unique(rows):
  out={}
  for k,v in rows:require(k not in out,'Dungeon switch duplicate JSON field');out[k]=v
  return out
 def invalid(value):raise ValueError('Dungeon switch nonfinite JSON')
 p=json.loads(Path(file).read_text(encoding='utf8'),object_pairs_hook=unique,parse_constant=invalid)
 fields(p,'schema kind commit scene_id scope paths sources nodes','switch review')
 require(type(p['schema'])is int and p['schema']==1 and p['kind']=='encore.dungeon-switches.reviewed-source'and p['commit']==PIN,'Dungeon switch schema/pin')
 require(type(p['scene_id'])is int and 0<p['scene_id']<65536 and type(p['scope'])is str and 0<len(p['scope'])<=2048,'Dungeon switch scene/scope')
 fields(p['paths'],SELECTORS,'switch selectors');selected=[path(v)for v in p['paths'].values()]
 require(len(set(selected))==len(selected)and type(p['sources'])is dict and set(p['sources'])==set(selected),'Dungeon switch source coverage')
 ex=Extractor(ROOT)
 for name,digest in p['sources'].items():require(type(digest)is str and re.fullmatch('[0-9a-f]{64}',digest)and hashlib.sha256(ex.data(name)).hexdigest()==digest,'Dungeon switch source changed: '+name)
 require(type(p['nodes'])is list and 0<len(p['nodes'])<=128,'Dungeon switch node count')
 ids=set();paths=set()
 for n in p['nodes']:
  fields(n,'id path kind','switch node');path(n['path'])
  require(type(n['id'])is int and 0<n['id']<2**32 and n['id']not in ids and n['path']not in paths and n['kind']in('lever','plate','gate','wall'),'Dungeon switch node identity')
  ids.add(n['id']);paths.add(n['path'])
 return p,ex

def extract(file=RECIPE):
 p,ex=read(file);s=p['paths'];sources=Sources(ROOT);nodes=instantiate(sources.doc,s['scene']);by_path={n.path:n for n in nodes};doc=sources.doc(s['scene'])
 texts={k:ex.text(v)for k,v in s.items()if k.endswith('_script')}
 require(by_path['.'].script==s['scene_script'],'Dungeon switch root script binding')
 inheritance={'switch_script':'ObjectsListSwitch','list_script':'AbstractSwitch','abstract_script':'FlaggableObject','flag_script':'Sprite','object_script':'Sprite','two_state_script':'ControlledObject','gate_script':'ControlledTwoStatesObject','wall_script':'ControlledTwoStatesObject'}
 for k,parent in inheritance.items():require(re.search(r'^extends '+re.escape(parent)+r'\s*$',texts[k],re.M),'Dungeon switch source inheritance')
 ordered(function(texts['abstract_script'],'_try_operate_manually'),['!_is_disabled','!global.get_player().is_paused()','_is_ready_to_interact','_operate_manually()'],'manual gates')
 ordered(function(texts['abstract_script'],'_operate_manually'),['global.currentScene.can_switch','_audio_player.playing = true','has_animation("Hit")','emit_signal("switch_hit")','_do_switch_action()','audioManager.play_sfx'],'manual dispatch')
 denied=re.findall(r'audioManager.play_sfx\(load\("res://([^"\n]+)"\), "([^"\n]+)"\)',function(texts['abstract_script'],'_operate_manually'));require(len(denied)==1,'Dungeon switch denial sound binding');denied,channel=denied[0];path(denied);path(channel);ex.data(denied)
 ready=re.findall(r'create_timer\(([0-9.]+)\)',function(texts['abstract_script'],'_ready'));require(len(ready)==1,'Dungeon switch readiness timer');ready=real(float(ready[0]))
 ordered(function(texts['switch_script'],'turn_on_off'),['_set_flag_status(value)','_on_state_changed(value)','_update_controlled_objects()'],'switch action')
 ordered(function(texts['switch_script'],'_on_state_changed'),['if state:','play("TurnOn")','if _is_one_way:','_is_disabled = true','play("TurnOff")'],'switch one-way state')
 require('state = (state != switch.is_on())'in function(texts['two_state_script'],'_get_state')and'return _update_order * (1 if _get_state() else -1)'in function(texts['two_state_script'],'get_update_order'),'Dungeon switch XOR/update order')
 ordered(function(texts['two_state_script'],'update_state'),['_get_state()','"Stay" if silent else ""','yield(_state_anim_player, "animation_finished")','emit_signal("state_changed", is_on, silent)'],'controlled continuation')
 ordered(function(texts['list_script'],'_update_controlled_objects'),['if _pause_when_changed:','global.get_player().pause()','uiManager.set_cutscene(true)','sort_custom','_control_object(i)','_wait_and_move_camera(objects_to_focus, actions_to_wait)','if _focus_objects_when_changed:','yield(get_tree(), "idle_frame")','camera.return_camera(_camera_delay)','global.get_player().unpause()','uiManager.set_cutscene(false)'],'control continuation')
 require('global.currentScene.name + "/" + name'in function(texts['flag_script'],'_get_flag_status')and'reset_when_leaving_area or (region_changed and reset_when_leaving_region)'in function(texts['flag_script'],'_on_leave_area'),'Dungeon switch flag/reset identity')
 callback=function(texts['scene_script'],'_on_main_switch_hit');ordered(callback,['audioManager.music_fadeout(0)','global.get_player().pause()','uiManager.set_cutscene(true)'],'root switch callback')
 fade=re.findall(r'music_fadeout\(([0-9.]+)\)',callback);require(len(fade)==1,'Dungeon switch music fade');fade=real(float(fade[0]))
 callback=function(texts['scene_script'],'_on_wall_door_state_changed');ordered(callback,['if state and !silent:','create_timer(0.5)','uiManager.set_cutscene(false)','global.get_player().unpause()','$Music/MusicArea.play_music()'],'root wall callback')
 post=re.findall(r'create_timer\(([0-9.]+)\)',callback);require(len(post)==1,'Dungeon switch wall callback delay');post=real(float(post[0]))
 main=[c for c in doc.connections if c['method']=='_on_main_switch_hit'];wall=[c for c in doc.connections if c['method']=='_on_wall_door_state_changed']
 require(len(main)==len(wall)==1 and main[0]==dict(signal='switch_hit',**{'from':main[0]['from']},to='.',method='_on_main_switch_hit')and wall[0]==dict(signal='state_changed',**{'from':wall[0]['from']},to='.',method='_on_wall_door_state_changed'),'Dungeon switch root signal bindings')
 camera=re.findall(r'export \(float\) var _camera_delay := ([0-9.]+)',texts['list_script']);require(len(camera)==1,'Dungeon switch camera default');camera=real(float(camera[0]))
 actual=[n for n in nodes if n.scene in [s[k]for k in('lever','plate','gate','wall')]]
 require([n.path for n in actual]==[n['path']for n in p['nodes']],'Dungeon switch node order/coverage')
 ids={n['path']:n['id']for n in p['nodes']};switches=[];objects=[];edges=[];scene_name=doc.nodes[0].attrs['name']
 for n,row in zip(actual,p['nodes']):
  kind=row['kind'];require(n.scene==s[kind]and n.script==s['switch_script'if kind in('lever','plate')else kind+'_script'],'Dungeon switch node kind/script')
  allowed='script position '+('flag is_object_flag emit_flag_updated_signal reset_when_leaving_area reset_when_leaving_region _is_one_way _pause_when_changed _focus_objects_when_changed _camera_delay _controlled_objects'if kind in('lever','plate')else'_initially_on _is_synced_with_room _update_order _move_cam_when_changed _move_cam_offset')
  require(set(n.props)<=set(allowed.split()),'Dungeon switch unknown effective override');pos=n.props.get('position',Vec2(0,0));require(type(pos)is Vec2,'Dungeon switch position')
  if kind in('lever','plate'):
   names='is_object_flag emit_flag_updated_signal reset_when_leaving_area reset_when_leaving_region _is_one_way _pause_when_changed _focus_objects_when_changed'.split();bits=0
   for bit,name in enumerate(names):
    v=n.props.get(name,False);require(type(v)is bool,'Dungeon switch boolean override');bits|=int(v)<<bit
   flag=n.props.get('flag','');require(flag is None or type(flag)is str,'Dungeon switch flag type')
   if not flag:flag=scene_name+'/'+n.name;bits|=1
   path(flag);delay=real(n.props.get('_camera_delay',camera));require(0<delay<=60,'Dungeon switch camera delay')
   refs=n.props.get('_controlled_objects');require(type(refs)is list and refs and all(type(v)is NodePathValue for v in refs),'Dungeon switch control paths')
   first=len(edges)
   for ref in refs:
    target=posixpath.normpath(posixpath.join(n.path,ref.path));path(target)
    require(target in ids and any(v['path']==target and v['kind']in('gate','wall')for v in p['nodes']),'Dungeon switch missing/wrong-kind target');edges.append([row['id'],ids[target]])
   audio=by_path[n.path+'/AudioStreamPlayer'];owner=audio.overrides.get('stream',audio.resolve);sound=owner.ext_path(audio.props['stream'],'AudioStream');ex.data(sound)
   switches.append(dict(id=row['id'],path=n.path,flag=flag,bits=bits,camera=delay,x=real(pos.x),y=real(pos.y),first=first,count=len(edges)-first,audio=sound,kind=1 if kind=='lever'else 2))
  else:
   bits=0
   for bit,name in enumerate(('_initially_on','_move_cam_when_changed')):
    value=n.props.get(name,False);require(type(value)is bool,'Dungeon object boolean override');bits|=int(value)<<bit
   require(n.props.get('_is_synced_with_room',False)is False,'Dungeon synced switches are not admitted');order=n.props.get('_update_order',0);require(type(order)is int and -32767<=order<=32767,'Dungeon object update order')
   offset=n.props.get('_move_cam_offset',Vec2(0,0));require(type(offset)is Vec2,'Dungeon object camera offset')
   objects.append(dict(id=row['id'],path=n.path,kind=1 if kind=='gate'else 2,order=order,bits=bits,x=real(pos.x),y=real(pos.y),offset_x=real(offset.x),offset_y=real(offset.y)))
 region=by_path['.'].props.get('_region_name');path(region)
 data=dict(scene=p['scene_id'],region=region,ready=ready,main=ids.get(main[0]['from'],0),wall=ids.get(wall[0]['from'],0),post_delay=post,fade=fade,denied=denied,channel=channel,switches=switches,objects=objects,edges=edges)
 encode(data)
 return data,dict(commit=PIN,scope=p['scope'],whole_scripts_approved=False,sources=dict(sorted({**sources.digests(),**ex.sources}.items())))

def _encode(data):
 strings=bytearray();offsets={}
 def text(v):
  if v not in offsets:offsets[v]=(len(strings),len(v));strings.extend(v.encode('ascii'))
  return offsets[v]
 policy=struct.pack('<2If2I2fI4I',*text(data['region']),real(data['ready']),data['main'],data['wall'],real(data['post_delay']),real(data['fade']),0,*text(data['denied']),*text(data['channel']))
 switches=bytearray();objects=bytearray()
 for n in data['switches']:switches.extend(struct.pack('<6I3f7I',n['id'],*text(n['path']),*text(n['flag']),n['bits'],real(n['camera']),real(n['x']),real(n['y']),n['first'],n['count'],*text(n['audio']),n['kind'],0,0))
 for n in data['objects']:objects.extend(struct.pack('<4IiI4f2I',n['id'],*text(n['path']),n['kind'],n['order'],n['bits'],real(n['x']),real(n['y']),real(n['offset_x']),real(n['offset_y']),0,0))
 edges=b''.join(struct.pack('<2I',*e)for e in data['edges'])
 return pack_container(MAGIC,data['scene'],[strings,policy,switches,objects,edges],STRIDES,LIMIT)

def parse(blob):
 scene,sections=unpack_container(blob,MAGIC,STRIDES,LIMIT)
 require(0<scene<65536 and len(sections[1])==48 and 0<len(sections[2])//64<=128 and 0<len(sections[3])//48<=128 and 0<len(sections[4])//8<=1024,'Dungeon switch section counts')
 strings,policy,raw_switches,raw_objects,raw_edges=sections
 def text(offset,length):
  require(length>0 and offset<=len(strings)and length<=len(strings)-offset,'Dungeon switch text span')
  try:value=strings[offset:offset+length].decode('ascii')
  except UnicodeDecodeError as error:raise ValueError('Dungeon switch text encoding')from error
  return path(value)
 a=struct.unpack('<2If2I2fI4I',policy);require(0<a[2]<=60 and 0<a[5]<=60 and math.isfinite(a[6])and abs(a[6])<=1000000 and a[7]==0,'Dungeon switch policy')
 result=dict(scene=scene,region=text(*a[:2]),ready=a[2],main=a[3],wall=a[4],post_delay=a[5],fade=a[6],denied=text(*a[8:10]),channel=text(*a[10:12]),switches=[],objects=[],edges=list(map(list,struct.iter_unpack('<2I',raw_edges))))
 ids=set();paths=set();flags=set();cursor=0
 for at in range(0,len(raw_switches),64):
  n=struct.unpack_from('<6I3f7I',raw_switches,at);p=text(*n[1:3]);flag=text(*n[3:5]);sound=text(*n[11:13])
  require(n[0]>0 and n[0]not in ids and p not in paths and flag not in flags and n[5]<=127 and 0<n[6]<=60 and all(math.isfinite(v)and abs(v)<=1000000 for v in n[7:9])and n[9]==cursor and 0<n[10]<=len(result['edges'])-cursor and n[13]in(1,2)and n[14:]==(0,0),'Dungeon switch record')
  ids.add(n[0]);paths.add(p);flags.add(flag);cursor+=n[10]
  result['switches'].append(dict(id=n[0],path=p,flag=flag,bits=n[5],camera=n[6],x=n[7],y=n[8],first=n[9],count=n[10],audio=sound,kind=n[13]))
 require(cursor==len(result['edges']),'Dungeon switch unowned edges')
 for at in range(0,len(raw_objects),48):
  n=struct.unpack_from('<4IiI4f2I',raw_objects,at);p=text(*n[1:3])
  require(n[0]>0 and n[0]not in ids and p not in paths and n[3]in(1,2)and -32767<=n[4]<=32767 and n[5]<=3 and all(math.isfinite(v)and abs(v)<=1000000 for v in n[6:10])and n[10:]==(0,0),'Dungeon controlled object record')
  ids.add(n[0]);paths.add(p);result['objects'].append(dict(id=n[0],path=p,kind=n[3],order=n[4],bits=n[5],x=n[6],y=n[7],offset_x=n[8],offset_y=n[9]))
 object_ids={n['id']for n in result['objects']};require(len(set(map(tuple,result['edges'])))==len(result['edges'])and{e[1]for e in result['edges']}==object_ids,'Dungeon switch duplicate/missing object edge')
 for n in result['switches']:require(all(e[0]==n['id']and e[1]in object_ids for e in result['edges'][n['first']:n['first']+n['count']]),'Dungeon switch edge owner/kind')
 main=next((n for n in result['switches']if n['id']==result['main']),None);wall=next((n for n in result['objects']if n['id']==result['wall']),None)
 require(main is not None and main['bits']&16 and wall is not None and wall['kind']==2 and [result['main'],result['wall']]in result['edges'],'Dungeon switch root callback identities')
 require(_encode(result)==blob,'Dungeon switch noncanonical/unused data')
 return result

def encode(data):
 fields(data,'scene region ready main wall post_delay fade denied channel switches objects edges','encoded switch graph');path(data['region']);path(data['denied']);path(data['channel'])
 require(type(data['scene'])is int and 0<data['scene']<65536 and all(type(data[k])is int and 0<data[k]<2**32 for k in('main','wall')),'Dungeon switch encoded policy identity')
 for group,keys in(('switches','id path flag bits camera x y first count audio kind'),('objects','id path kind order bits x y offset_x offset_y')):
  require(type(data[group])is list and 0<len(data[group])<=128,'Dungeon switch encoded records')
  for n in data[group]:
   fields(n,keys,'encoded '+group);path(n['path'])
   integers=('id','bits','first','count','kind')if group=='switches'else('id','kind','bits','order')
   require(all(type(n[k])is int and(-32767<=n[k]<=32767 if k=='order'else 0<=n[k]<2**32)for k in integers),'Dungeon switch encoded integer')
   if group=='switches':path(n['flag']);path(n['audio'])
 require(type(data['edges'])is list and 0<len(data['edges'])<=1024 and all(type(e)is list and len(e)==2 and all(type(v)is int and 0<v<2**32 for v in e)for e in data['edges']),'Dungeon switch encoded edges')
 blob=_encode(data);parse(blob);return blob

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--recipe',type=Path,default=RECIPE);p.add_argument('--out',type=Path,required=True);args=p.parse_args();out=args.out.resolve()
 require(out.is_relative_to((ROOT/'build').resolve())and out.suffix=='.encdsw','Dungeon switch candidate must stay under build')
 data,proof=extract(args.recipe);blob=encode(data);out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(blob)
 proof.update(data=data,bytes=len(blob),sha256=hashlib.sha256(blob).hexdigest(),converter_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),review_sha256=hashlib.sha256(args.recipe.read_bytes()).hexdigest())
 out.with_suffix('.source.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Switch graph candidate:',len(blob),'bytes;',len(data['switches']),'switches,',len(data['objects']),'objects; no live scene admission')
if __name__=='__main__':main()
