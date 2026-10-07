#!/usr/bin/env python3
"""Source-checked Podunk CutsceneArea lifecycle; existing Room owns programmes."""
from __future__ import annotations
import argparse,hashlib,json,math,re,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import decode,stable,SCENE,PIN,read,write,sha,require
from tools.extract_battle_entry import Extractor
SCRIPT='Scripts/Main/CutsceneArea.gd';AREA='Nodes/Reusables/CutsceneArea.tscn'
IR=ROOT/'content/native-field-cutscene-area.json';REVIEW=ROOT/'reports/field-cutscene-area/source-review.json';PACK=ROOT/'romfs/data/podunk-cutscene-areas.encarea'
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
 require(len(out)==15,'Podunk CutsceneArea binding loss');return out,names,resources,dict(native_export_sha256=sha(native),source_receipt_sha256=sha(receipt),source_files={name:r['sha256'] for name,r in s['files'].items()})


def build(records,nodes,resources,provenance):
 ex=Extractor(ROOT);ex.sources.update(provenance['source_files']);script=ex.text(SCRIPT);scene=ex.text(AREA);ui=ex.text('Scripts/global/uiManager.gd');flags=ex.text('Scripts/global/globalData.gd');player=ex.text('Scripts/Main/party/Player.gd');ex.text('Nodes/Reusables/Player.tscn');ex.text('LICENSE')
 for fact in ['set_process(false)','uiManager.connect("battle_to_ov", self, "_stop_process")','uiManager.is_in_cutscene() or uiManager.is_in_battle() or uiManager.is_pause_menu_active()','body == global.get_player()','body == global.get_player() and !uiManager.is_in_battle()','if dialog != "":','globaldata.check_appear_disappear_flags(appear_flag, disappear_flag)','global.get_player().pause()','uiManager.open_dialogue_box_and_unpause(dialog)']:
  require(fact in script,'Unknown CutsceneArea behavior: '+fact)
 require(script.count('set_process(true)')==2 and script.count('set_process(false)')==5 and not re.search(r'\b(set_flag|change_flag|yield|randi|randf)\b',script),'CutsceneArea branch/side-effect topology changed')
 close=re.search(r'uiManager.close_commands_menu\((true|false), (true|false)\)',script);require(close,'Missing close source arguments')
 paused=re.search(r'func pause\(stop_running := (true|false), start_idle := (true|false), emit_signal := (true|false)\)',player);require(paused,'Missing player pause source defaults')
 close_defaults=re.search(r'func close_commands_menu\(keep_pause := (true|false), remove_bars := (true|false), silent := (true|false), called_from_pause := (true|false)\)',ui);require(close_defaults,'Missing source command close defaults')
 completion=re.search(r'funcref\(global.get_player\(\), "([^"]+)"\)',ui);require(completion and 'func '+completion[1]+'(result := 0):\n\tunpause()'in player,'Unknown dialogue completion source callback')
 require('flag_on = flags.get(appear_flag, false)'in flags and 'flag_on = flag_on and !flags.get(disappear_flag, false)'in flags,'Unsupported flag query topology')
 connections=re.findall(r'\[connection signal="([^"]+)" from="\." to="\." method="([^"]+)"\]',scene);require(len(connections)==2 and dict(connections)=={'body_entered':'_on_Cutscene_Area_body_entered','body_exited':'_on_Cutscene_Area_body_exited'},'Area signal topology')
 bindings=[]
 for a in records:
  path=a['node'];root=decode(a['native']['properties']);child=nodes[path+'/CollisionShape2D'];v=decode(child['properties']);wt=decode(child['world_transform']);shape=resources[v['shape']['id']];props=decode(shape['properties']);over=a['overrides'];dialog=over.get('dialog','');require(type(dialog)is str and dialog,'Actual Podunk area programme absent')
  programme='Data/Dialogue/'+dialog+'.yaml';ex.text(programme)
  require(a['native']['class']=='Area2D' and shape['class']=='RectangleShape2D' and not v['disabled'] and not v['one_way_collision'] and v['rotation']==0 and v['scale']==[1,1] and wt[0][1]==wt[1][0]==0 and wt[0][0]>0 and wt[1][1]>0 and root['space_override']==0 and not root['audio_bus_override'],'CutsceneArea unsupported geometry/space behavior')
  require(root['pause_mode']==0 and root['process_priority']==0,'CutsceneArea unsupported process priority/mode')
  ext=props['extents'];bindings.append(dict(id=a['stable_id'],node=path,ready_ordinal=a['ready_ordinal'],shape_id=stable(child['path']),dialog=dialog,programme=programme,programme_sha256=ex.sources[programme],appear=over.get('appear_flag',''),disappear=over.get('disappear_flag',''),collision_layer=root['collision_layer'],collision_mask=root['collision_mask'],monitoring=root['monitoring'],monitorable=root['monitorable'],centre=wt[2],half_extents=[ext[0]*wt[0][0],ext[1]*wt[1][1]]))
 battle=re.search(r'uiManager\.connect\("([^"]+)", self, "([^"]+)"\)',script);require(battle and 'func '+battle[2]+'(' in script,'Cutscene battle callback missing')
 return dict(schema=2,kind='encore.field-cutscene-area.source-ir',commit=PIN,scene=SCENE,script=SCRIPT,close=[close[i]=='true'for i in [1,2]]+[close_defaults[i]=='true'for i in [3,4]],pause=[paused[i]=='true'for i in [1,2,3]],completion=completion[1],battle_signal=battle[1],battle_method=battle[2],connections=[dict(signal=s,method=m)for s,m in connections],bindings=bindings,sources=dict(sorted(ex.sources.items())),provenance=provenance,semantics=['All15 actual Podunk instances with inherited defaults, inner-to-outer overrides and full-scene postorder Ready ordinals','Ready disables script idle process and connects source uiManager battle_to_ov signal, no eager programme start or flag write','Only exact current global player body enter checks flags and enables process for either result if dialog nonempty','Idle blocked by cutscene/battle/pause keeps process enabled; first unblocked false flag check disables process without polling','Appear read defaults false; disappear read is short-circuited when appear false, preserving source read order','Start admits existing checked Room programme before effects then executes source close command booleans, Player.pause defaults, source dialogue ID and source player completion funcref in order; processing disabled after invocation returns','Exit while in battle deliberately does not disable; battle_to_ov unconditionally disables without rearming overlapping areas','No one-shot flag is written by this component; programme effects control repeated trigger, including mick_bark which does not set its disappear flag','Area layer/mask/monitoring and actual transformed rectangle retained; use original Player body polygon overlap and Godot bilateral OR layer/mask filtering, never replace with a point or investigation ray'],unsupported=['Programme operations belong to existing checked Room/scene actor host; each unresolved source programme rejects before UI/pause mutation','No generic Area2D physics emulation or arbitrary unknown source signal listeners'])

def validate(d):
 require(d['schema']==2 and d['kind']=='encore.field-cutscene-area.source-ir'and d['commit']==PIN and d['script']==SCRIPT and d['scene']==SCENE,'CutsceneArea source version')
 require(len(d['bindings'])==15 and len(d['close'])==4 and len(d['pause'])==3 and all(type(x)is bool for x in d['close']+d['pause']),'CutsceneArea source capabilities')
 require(d['battle_signal']=='battle_to_ov'and d['battle_method']=='_stop_process'and d['completion']=='on_dialogue_done'and d['connections']==[dict(signal='body_entered',method='_on_Cutscene_Area_body_entered'),dict(signal='body_exited',method='_on_Cutscene_Area_body_exited')],'CutsceneArea lifecycle source receipt')
 ids=set();last=-1
 for b in d['bindings']:
  require(b['id']==stable(b['node'])and b['shape_id']==stable(b['node']+'/CollisionShape2D')and b['id']not in ids and b['ready_ordinal']>last,'CutsceneArea stable source identity/order');ids.add(b['id']);last=b['ready_ordinal']
  require(b['programme']=='Data/Dialogue/'+b['dialog']+'.yaml'and b['programme_sha256']==d['sources'][b['programme']],'CutsceneArea source programme closure')
  require(all(type(b[k])is str and '\x00'not in b[k] for k in ['node','dialog','programme','appear','disappear']),'CutsceneArea source strings')
  require(type(b['monitoring'])is bool and type(b['monitorable'])is bool and 0<=b['collision_layer']<=0xffffffff and 0<=b['collision_mask']<=0xffffffff,'CutsceneArea collision policy')
  require(all(type(v)in(int,float)and math.isfinite(v)and abs(v)<=1e6 for v in b['centre']+b['half_extents'])and all(v>0 for v in b['half_extents']),'CutsceneArea finite geometry')
 return d

def load():
 d=validate(read(IR));r=read(REVIEW);require(r['commit']==PIN and r['ir_sha256']==sha(IR)and r['sources']==d['sources'],'CutsceneArea semantic review stale');inv=read(ROOT/'compatibility/upstream-inventory.json')['files']
 for p,h in d['sources'].items():require(sha(ROOT/'upstream/MOTHER-Encore'/p)==h==inv[p]['sha256'],'Changed CutsceneArea source '+p)
 return d

def encode(d):
 validate(d);b=bytearray(64)
 def u(*x):b.extend(struct.pack('<'+'I'*len(x),*x))
 def text(x):v=x.encode('utf-8');u(len(v));b.extend(v)
 text(d['scene']);text(d['script']);u(*[int(x)for x in d['close']+d['pause']]);text(d['completion']);text(d['battle_signal']);text(d['battle_method']);u(len(d['connections']))
 for role,c in enumerate(d['connections'],1):u(role);text(c['signal']);text(c['method'])
 u(len(d['bindings']))
 for a in d['bindings']:
  u(a['id'],a['ready_ordinal'],a['shape_id'],a['collision_layer'],a['collision_mask'],int(a['monitoring'])|int(a['monitorable'])<<1)
  for k in ['node','dialog','programme','appear','disappear']:text(a[k])
  b.extend(bytes.fromhex(a['programme_sha256']));b.extend(struct.pack('<4f',*(a['centre']+a['half_extents'])))
 u(len(d['sources']))
 for p,h in d['sources'].items():text(p);b.extend(bytes.fromhex(h))
 struct.pack_into('<8s6I20s12x',b,0,b'ENCCSA01',2,len(b),0,2,1,len(d['bindings']),bytes.fromhex(PIN));struct.pack_into('<I',b,16,zlib.crc32(b)&0xffffffff);return bytes(b)

def stage_files(source):
 b=encode(load());p=Path('data/podunk-cutscene-areas.encarea');require((Path(source)/p).read_bytes()==b,'Stale CutsceneArea binary');return{p:b}

def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile']);p.add_argument('--native',type=Path);p.add_argument('--source',type=Path);a=p.parse_args()
 if a.action=='extract':
  require(a.native and a.source,'Actual complete Podunk source exports required');d=validate(build(*source_instances(a.native,a.source,ROOT/'upstream/MOTHER-Encore')));write(IR,d);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=d['sources'],semantics=d['semantics'],unsupported=d['unsupported']));print('CutsceneArea source:15 actual instances /15 source programme references');return
 b=encode(load());PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(b);print('CutsceneArea checked resource:',len(b),'bytes')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,OSError,TypeError,struct.error)as e:sys.exit('FIELD CUTSCENE AREA ERROR: '+str(e))
