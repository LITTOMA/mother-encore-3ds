#!/usr/bin/env python3
"""Actual Podunk DoorNPC: ordered knock/timer and checked Room dialogue dispatch."""
from __future__ import annotations
import argparse,json,math,re,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import decode,stable,SCENE,PIN,read,write,sha,require
from tools.extract_battle_entry import Extractor
SCRIPT='Scripts/Main/door_npc.gd';AREA='Nodes/Reusables/door_npc.tscn'
IR=ROOT/'content/native-field-door-npc.json';REVIEW=ROOT/'reports/field-door-npc/source-review.json';PACK=ROOT/'romfs/data/podunk-door-npc.encdoor'
ENGINE_URL='https://raw.githubusercontent.com/godotengine/godot/3.6.2-stable/scene/main/scene_tree.cpp';ENGINE_SHA='a1ac9f2da5a1046d1c468fd463de4907f535f124952b137d69fb39ef8b14b3b4'
def source_instances(native,receipt,upstream):
 d=read(native);s=read(receipt);require(d['source']=='res://'+SCENE and d['schema']==1 and d['native_compatible'] is False,'Incomplete CutsceneArea native scene')
 require(s['commit']==PIN and s['scene']==SCENE,'Unreviewed CutsceneArea native receipt')
 inventory=read(ROOT/'compatibility/upstream-inventory.json')['files']
 for name,record in s['files'].items():require(sha(upstream/name)==record['sha256']==inventory[name]['sha256'],'Changed NPC closure '+name)
 names={n['path']:n for n in d['nodes']};require(len(names)==8686,'Incomplete Podunk enemy scene');require(decode(names['Objects']['world_transform'])==[[1,0],[0,1],[0,0]],'NPC Objects transform requires a source adapter')
 resources={r['id']:r for r in d['resources']};states={s['source'][6:]:s for s in d['scene_states']};roots=[]
 def visit_instances(path,filename):
  roots.append((path,filename))
  for n in states[filename]['nodes']:
   if n['instance'] is not None:
    local=n['path'][2:] if n['path'].startswith('./') else n['path'];target=local if path=='.' else path+'/'+local
    visit_instances(target,resources[n['instance']['id']]['path'][6:])
 visit_instances('.',SCENE);bindings={};overrides={}
 for root,filename in sorted(roots,key=lambda r:r[0].count('/') if r[0]!='.' else -1,reverse=True):
  for n in states[filename]['nodes']:
   local=n['path'][2:] if n['path'].startswith('./') else n['path'];path=root if local=='.' else local if root=='.' else root+'/'+local
   if path in names:overrides.setdefault(path,{}).update(decode(n['properties']))
  for a in s['script_attachments']:
   if a['source']!=filename:continue
   name=re.search(r'\bname="([^"]+)"',a['node_declaration'])[1];parent=re.search(r'\bparent="([^"]+)"',a['node_declaration'])
   local='.' if parent is None else name if parent[1]=='.' else parent[1]+'/'+name;path=root if local=='.' else local if root=='.' else root+'/'+local
   require(path in names,'Unresolved NPC binding '+path);bindings[path]=a['script']
 children={p:[] for p in names}
 for path in names:
  if path!='.':children[path.rsplit('/',1)[0] if '/' in path else '.'].append(path)
 ready=[]
 def visit(path):
  for child in children[path]:visit(child)
  ready.append(path)
 visit('.');ordinal={p:i for i,p in enumerate(ready)};out=[]
 for path in sorted(bindings,key=lambda p:ordinal[p]):
  if bindings[path]!=SCRIPT:continue
  out.append(dict(stable_id=stable(path),node=path,ready_ordinal=ordinal[path],script=bindings[path],overrides=overrides[path],native=names[path],children=[names[p] for p in children[path]]))
 require(len(out)==1,'Podunk DoorNPC binding loss');return out,names,resources,dict(native_export_sha256=sha(native),source_receipt_sha256=sha(receipt),source_files={name:r['sha256'] for name,r in s['files'].items()})


def build(records,nodes,resources,provenance,engine):
 ex=Extractor(ROOT);ex.sources.update(provenance['source_files']);script=ex.text(SCRIPT);scene=ex.text(AREA);ui=ex.text('Scripts/global/uiManager.gd');player=ex.text('Scripts/Main/party/Player.gd');party=ex.text('Scripts/Main/party/party_object.gd');flags=ex.text('Scripts/global/globalData.gd');ex.text('LICENSE')
 require(sha(engine)==ENGINE_SHA,'Unreviewed actual SceneTree timer source');es=engine.read_text()
 require('if (time_left < 0)'in es and 'ClassDB::bind_method(D_METHOD("create_timer", "time_sec", "pause_mode_process"), &SceneTree::create_timer, DEFVAL(true));'in es and 'break on last, so if new timers were added during list traversal, ignore them.'in es,'Unknown source global timer semantics')
 for fact in ['_all_dialog.push_front(["", dialog])','set_process(false)','uiManager.is_in_cutscene() or uiManager.is_in_battle() or uiManager.is_pause_menu_active()','global.get_player().try_to_turn(self)','global.get_player().pause()','uiManager.set_cutscene(true)','uiManager.toggle_black_bars(true)','$AudioStreamPlayer.play()','_get_right_dialog(true)','uiManager.set_cutscene(false)','body != global.get_player()','get_path(), flag, j, cur_dialog','globaldata.seen_dialogue_flags[last_dialog_hash] = true','!globaldata.seen_dialogue_flags.get(dialog_hash, false) or j == _all_dialog[i].size() - 1']:require(fact in script,'Unknown DoorNPC source '+fact)
 require('func try_to_turn(target:Node2D)'in party and 'flag_on = flags.get(appear_flag, false)'in flags and 'flag_on = flag_on and !flags.get(disappear_flag, false)'in flags,'Unknown source turn/flags')
 pause=re.search(r'func pause\(stop_running := (true|false), start_idle := (true|false), emit_signal := (true|false)\)',player);require(pause,'Door source pause defaults')
 duration=float(re.search(r'yield\(get_tree\(\).create_timer\(([0-9.]+)\),"timeout"\)',script)[1]);completion=re.search(r'funcref\(global.get_player\(\), "([^"]+)"\)',ui)[1]
 turn={k:re.search(r'"'+k+r'": (true|false)',script)[1]=='true'for k in ['x','y']};connections=[dict(role=i+1,signal=s,method=m)for i,(s,m)in enumerate(re.findall(r'\[connection signal="([^"]+)" from="\." to="\." method="([^"]+)"\]',scene))]
 audio_path=re.search(r'\[ext_resource path="res://([^"]+)" type="AudioStream"',scene)[1];ex.data(audio_path);ex.data(audio_path+'.import');bus=re.search(r'^bus = "([^"]+)"$',scene,re.M)[1]
 bindings=[]
 for a in records:
  path=a['node'];o=a['overrides'];n=a['native'];p=decode(n['properties']);wt=decode(n['world_transform']);dialog=o.get('dialog','');groups=o.get('_all_dialog',[]);audio=nodes[path+'/AudioStreamPlayer'];av=decode(audio['properties']);shape=nodes[path+'/CollisionShape2D'];sv=decode(shape['properties']);st=decode(shape['world_transform']);r=resources[sv['shape']['id']];q=decode(r['properties'])
  require(n['class']=='Area2D'and p['pause_mode']==0 and p['process_priority']==0 and p['space_override']==0 and not p['audio_bus_override'],'DoorNPC source Area capability');require(r['class']=='RectangleShape2D'and not sv['disabled']and not sv['one_way_collision']and st[0][1]==st[1][0]==0 and st[0][0]>0 and st[1][1]>0,'DoorNPC actual transformed rectangle capability')
  require(len(a['children'])==2 and not av['autoplay']and not av['stream_paused']and av['mix_target']==0 and audio['class']=='AudioStreamPlayer','DoorNPC actual child lifecycle')
  programmes=[]
  for name in sorted(set([dialog]+[v for group in groups for v in group[1:]])-set([''])):
   src='Data/Dialogue/'+name+'.yaml';ex.text(src);programmes.append(dict(dialog=name,path=src,sha256=ex.sources[src]))
  child_ready={c['path']:a['ready_ordinal']-len(a['children'])+i for i,c in enumerate(a['children'])}
  bindings.append(dict(id=a['stable_id'],node=path,ready_ordinal=a['ready_ordinal'],shape_id=stable(shape['path']),audio_id=stable(audio['path']),audio_ready=child_ready[audio['path']],dialog=dialog,groups=groups,programmes=programmes,appear=o.get('appear_flag',''),disappear=o.get('disappear_flag',''),layer=p['collision_layer'],mask=p['collision_mask'],flags=int(p['monitoring'])|int(p['monitorable'])<<1,position=wt[2],centre=st[2],half=[q['extents'][0]*st[0][0],q['extents'][1]*st[1][1]],audio=dict(path=audio_path,bus=bus,volume_db=av['volume_db'],pitch=av['pitch_scale'])))
 return dict(schema=1,kind='encore.field-door-npc.source-ir',commit=PIN,scene=SCENE,script=SCRIPT,pause=[pause[i]=='true'for i in [1,2,3]],turn=[turn['x'],turn['y']],completion=completion,timer=dict(seconds=duration,process_pause=True,ignore_time_scale=False,strict_negative=True,source_global_fifo=True),connections=connections,bindings=bindings,sources=dict(sorted(ex.sources.items())),provenance=provenance,engine=dict(url=ENGINE_URL,sha256=ENGINE_SHA),semantics=['One actual source DoorNPC root and real transformed body collider layer4096 mask0; bilateral OR Player filtering, not inspection ray','Ready conditionally prepends default dialogue row then disables process; actual runtime absolute get_path supplied by SceneTree host forms persistent seen keys','Only actual global player enter with nonempty selected dialogue enables idle process; both flag-check branches do so; no body-exit connection cancels processing','UI cutscene/battle/pause blocks but retains processing; first unblocked false flags disables, true starts source coroutine then disables immediately','Start ordered actual Player.try_to_turn(y=true,x=false), pause(source defaults), cutscene=true, blackbars=true, child AudioStreamPlayer.play then SceneTree create_timer(1)','Actual source audio bus is SFX; disabled-project native export fallback Master is not substituted for the original serialized bus','SceneTreeTimer pause_process default true, scaled idle clock, strict time_left<0, global insertion FIFO, new timers during traversal wait until next frame; do not replace with actor paused Timer','On timeout select dialogue anew using then-current flags/seen keys; last matching row overrides prior rows, first unseen or final entry, mark chosen hash then existing Room.open_dialogue_box_and_unpause(actual player completion)','After invoking asynchronous open_room_and_unpause, immediately set_cutscene(false); existing Room own yielded Ready later restores actual UI cutscene state; do not invent synchronous completion','Multiple source coroutine waits are independent global tokens; owner freeing invalidates callback instance but SceneTree timer retains global lifetime','Unknown programme/turn/player pause/blackbar/audio/timer endpoints reject complete plan before first mutation; no generic VM or fake playback record'],unsupported=['Existing Room programme must admit question playeremote, Adult/Kid/Female/Shy source voices and cleardialog before actual trigger','Real source child AudioStreamPlayer WAV/bus routing, global SceneTree timer order and actual scene player/blackbars are mandatory typed Host dependencies','Scene exit/free distinction and source runtime NodePath identity must come from actual tree owner; no hardcoded /root name'])

def validate(d):
 require(d['schema']==1 and d['kind']=='encore.field-door-npc.source-ir'and d['commit']==PIN and d['scene']==SCENE and d['script']==SCRIPT,'DoorNPC source version')
 require(len(d['bindings'])==1 and len(d['pause'])==3 and len(d['turn'])==2 and all(type(x)is bool for x in d['pause']+d['turn'])and d['completion']=='on_dialogue_done','DoorNPC typed policy')
 require(d['engine']==dict(url=ENGINE_URL,sha256=ENGINE_SHA)and d['timer']==dict(seconds=1.0,process_pause=True,ignore_time_scale=False,strict_negative=True,source_global_fifo=True),'DoorNPC actual timer source')
 require(d['connections']==[dict(role=1,signal='body_entered',method='_on_Door_NPC_body_entered')],'DoorNPC signal source topology')
 for b in d['bindings']:
  require(b['id']==stable(b['node'])and b['shape_id']==stable(b['node']+'/CollisionShape2D')and b['audio_id']==stable(b['node']+'/AudioStreamPlayer')and b['audio_ready']<b['ready_ordinal'],'DoorNPC source identities')
  require(type(b['flags'])is int and 0<=b['flags']<=3 and all(type(b[k])is int and 0<=b[k]<=0xffffffff for k in ['layer','mask']),'DoorNPC body policy')
  require(all(len(b[k])==2 for k in ['position','centre','half'])and all(type(v)in(int,float)and math.isfinite(v)and abs(v)<=1e6 for k in ['position','centre','half']for v in b[k])and all(v>0 for v in b['half']),'DoorNPC source finite geometry')
  programmes={p['dialog']:p for p in b['programmes']};require(len(programmes)==len(b['programmes'])and b['dialog']in programmes,'DoorNPC programme topology')
  for g in b['groups']:require(type(g)is list and len(g)>=2 and all(type(x)is str for x in g)and all(x in programmes for x in g[1:]),'DoorNPC source dialogue row')
  for p in programmes.values():require(p['path']=='Data/Dialogue/'+p['dialog']+'.yaml'and p['sha256']==d['sources'][p['path']],'DoorNPC programme source hash')
  a=b['audio'];require(a['path']in d['sources']and a['path']+'.import'in d['sources']and a['bus']=='SFX'and math.isfinite(a['volume_db'])and -96<=a['volume_db']<=24 and math.isfinite(a['pitch'])and 0<a['pitch']<=16,'DoorNPC actual audio source binding')
 return d

def load():
 d=validate(read(IR));r=read(REVIEW);require(r['commit']==PIN and r['ir_sha256']==sha(IR)and r['sources']==d['sources']and r['engine']==d['engine'],'DoorNPC semantic review stale');inv=read(ROOT/'compatibility/upstream-inventory.json')['files']
 for p,h in d['sources'].items():require(sha(ROOT/'upstream/MOTHER-Encore'/p)==h==inv[p]['sha256'],'Changed DoorNPC source '+p)
 return d

def encode(d):
 validate(d);b=bytearray(64)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def text(v):raw=v.encode();u(len(raw));b.extend(raw)
 text(d['scene']);text(d['script']);u(*map(int,d['pause']+d['turn']));text(d['completion']);t=d['timer'];b.extend(struct.pack('<f',t['seconds']));u(int(t['process_pause']),int(t['ignore_time_scale']),int(t['strict_negative']),int(t['source_global_fifo']));u(len(d['connections']))
 for c in d['connections']:u(c['role']);text(c['signal']);text(c['method'])
 u(len(d['bindings']))
 for a in d['bindings']:
  u(a['id'],a['ready_ordinal'],a['shape_id'],a['audio_id'],a['audio_ready'],a['layer'],a['mask'],a['flags']);text(a['node']);text(a['dialog']);text(a['appear']);text(a['disappear']);b.extend(struct.pack('<6f',*(a['position']+a['centre']+a['half'])));text(a['audio']['path']);text(a['audio']['bus']);b.extend(struct.pack('<2f',a['audio']['volume_db'],a['audio']['pitch']));u(len(a['groups']))
  for g in a['groups']:
   u(len(g))
   for v in g:text(v)
  u(len(a['programmes']))
  for p in a['programmes']:text(p['dialog']);text(p['path']);b.extend(bytes.fromhex(p['sha256']))
 u(len(d['sources']))
 for p,h in d['sources'].items():text(p);b.extend(bytes.fromhex(h))
 struct.pack_into('<8s6I20s12x',b,0,b'ENCDOR01',1,len(b),0,1,1,len(d['bindings']),bytes.fromhex(PIN));struct.pack_into('<I',b,16,zlib.crc32(b)&0xffffffff);return bytes(b)

def stage_files(source):
 b=encode(load());p=Path('data/podunk-door-npc.encdoor');require((Path(source)/p).read_bytes()==b,'Stale DoorNPC pack');return{p:b}

def audio_bindings():
 return[dict(source_path='res://'+b['audio']['path'],source_sha256=load()['sources'][b['audio']['path']],source_bus=b['audio']['bus'])for b in load()['bindings']]

def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile']);p.add_argument('--native',type=Path);p.add_argument('--source',type=Path);p.add_argument('--engine',type=Path);a=p.parse_args()
 if a.action=='extract':
  require(a.native and a.source and a.engine,'Actual complete Podunk and engine source exports required');d=validate(build(*source_instances(a.native,a.source,ROOT/'upstream/MOTHER-Encore'),a.engine));write(IR,d);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=d['sources'],engine=d['engine'],semantics=d['semantics'],unsupported=d['unsupported']));print('DoorNPC source:1 real root / source AudioStreamPlayer /1 exact Room programme');return
 b=encode(load());PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(b);print('DoorNPC checked binary:',len(b),'bytes')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,OSError,TypeError,struct.error)as e:sys.exit('FIELD DOOR NPC ERROR: '+str(e))
