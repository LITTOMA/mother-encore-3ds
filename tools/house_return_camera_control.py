#!/usr/bin/env python3
"""Actual House RoomShaker script and instance closure, independent ENCHSHK1."""
import argparse, hashlib, json, re, struct, sys, zlib, urllib.request
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
IR=ROOT/'content/native-house-return-camera-control.json'
REVIEW=ROOT/'reports/house-return-camera-control/source-review.json'
PACK=ROOT/'romfs/data/house-return-camera-control.encshaker'
PIN='7d9246600fffe518408f5830d4848635019005a3'
SCRIPT='Scripts/Main/roomshaker.gd'
SCENE='Maps/podunk/Nintens House.tscn'
PROTOTYPE='Nodes/Reusables/roomshaker.tscn'
METHODS=['_ready','delayed_start','start_shake','stop_shake','vibrate','_on_Timer_timeout']
ENGINE='3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'
ENGINE_RECEIPT=ROOT/'reports/global-child-ready/engine-source.json'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def require(v,m):
 if not v:raise ValueError(m)
def write(p,d):p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes((json.dumps(d,indent=2,ensure_ascii=False)+'\n').encode())
def engine_input_source():
 # Same bounded urllib + verified build/vendor cache convention as the existing
 # official packaging bootstrap. Source receipts are reviewed repository data;
 # no developer-private paths/materials or IR-as-source fallback are used.
 receipt=json.loads(ENGINE_RECEIPT.read_text(encoding='utf-8'))
 require(receipt['schema']==1 and receipt['engine_commit']==ENGINE,'Unreviewed engine InputDefault receipt')
 rows=[r for r in receipt['files']if r['path']=='main/input_default.cpp']
 require(len(rows)==1,'Missing/ambiguous reviewed engine InputDefault source')
 row=rows[0];url='https://raw.githubusercontent.com/godotengine/godot/'+ENGINE+'/main/input_default.cpp'
 require(row['url']==url and isinstance(row['bytes'],int) and 0<row['bytes']<=1024*1024 and re.fullmatch(r'[0-9a-f]{64}',row['sha256']),'Unreviewed official InputDefault URL/size/SHA')
 path=ROOT/'build/vendor/godot-sources'/ENGINE/'main/input_default.cpp'
 if path.exists():
  require(path.is_file() and path.stat().st_size==row['bytes'],'Cached InputDefault source size/type changed')
  raw=path.read_bytes()
 else:
  request=urllib.request.Request(url,headers={'User-Agent':'encore-native-build/0.1'})
  with urllib.request.urlopen(request,timeout=60)as response:
   require(response.geturl()==url,'Official InputDefault source redirected outside the pinned URL')
   raw=response.read(row['bytes']+1)
  require(len(raw)==row['bytes']and hashlib.sha256(raw).hexdigest()==row['sha256'],'Official InputDefault source size/SHA mismatch')
  path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(raw)
 require(len(raw)==row['bytes']and hashlib.sha256(raw).hexdigest()==row['sha256'],'Cached reviewed InputDefault source SHA mismatch')
 return raw.decode('utf-8'),row

def derive(source):
 tree=ROOT/'content/native-house-node-tree.json';t=json.loads(tree.read_text(encoding='utf-8'))
 inv=json.loads((ROOT/'compatibility/upstream-inventory.json').read_text(encoding='utf-8'))
 require(t['commit']==inv['commit']==PIN and t['scene']==SCENE,'Source identity differs')
 text=(source/SCRIPT).read_text(encoding='utf-8'); scene=(source/SCENE).read_text(encoding='utf-8'); proto=(source/PROTOTYPE).read_text(encoding='utf-8')
 require(re.findall(r'^func (\w+)\(',text,re.M)==METHODS,'RoomShaker source method roster changed')
 facts=['timer.wait_time = wait_time','audioPlayer.stream = load("res://Audio/Sound effects/" + sound)','if auto_start:','yield(get_tree().create_timer(time), "timeout")','if timer.time_left == 0:','timer.stop()','global.currentCamera.shake_camera(magnitude, length, direction)','audioPlayer.play()','global.start_joy_vibration(0, 0.6, 0.8, length)','timer.wait_time = rand_range(wait_time - wait_margin, wait_time + wait_margin)','if !uiManager.is_in_battle() and !uiManager.is_game_over() and global.get_player().get_state() != global.get_player().CAMERA:']
 require(all(x in text for x in facts),'RoomShaker reviewed source control flow changed')
 numbers={k:float(v)for k,v in re.findall(r'^export \(float\) var (\w+) = ([0-9.]+)',text,re.M)}
 require(set(numbers)=={'wait_time','wait_margin','magnitude','length'},'Unknown shaker exported float')
 start=scene.index('[node name="Room Shaker"');end=scene.find('[node ',start+1);override=scene[start:end if end>=0 else len(scene)]
 direction=re.search(r'^direction = Vector2\( ([^,]+), ([^)]+) \)',override,re.M)
 require(direction is not None and len(override.strip().splitlines())==2,'Unknown RoomShaker instance override')
 sound='Audio/Sound effects/'+re.search(r'^export \(String\) var sound = "([^"]+)"',text,re.M)[1]
 require('export (bool) var auto_start = false' in text and 'export (Vector2) var direction = Vector2.ONE' in text,'Unknown source constructor')
 require('[node name="Timer" type="Timer" parent="."]\n\n' in proto and 'bus = "SFX"' in proto and '[connection signal="timeout" from="Timer" to="." method="_on_Timer_timeout"]' in proto,'Unknown child properties/connection')
 rows=[r for r in t['records']if r['script']==SCRIPT]
 require(len(rows)==1 and not any(t['classes'][r['class_index']]=='Camera2D'for r in t['records']),'House fixed camera/control scope changed')
 root=rows[0];children=[r for r in t['records']if r['parent']==root['id']]
 require([t['classes'][r['class_index']]for r in children]==['Timer','AudioStreamPlayer'] and root['script_methods']==1,'Actual RoomShaker subtree differs')
 paths=[SCENE,PROTOTYPE,SCRIPT,sound,'Scripts/global/global.gd','Scripts/global/uiManager.gd','Scripts/Main/party/Player.gd']
 sources={p:sha(source/p)for p in paths}
 for p,h in sources.items():require(inv['files'][p]['sha256']==h and (p not in t['sources']or t['sources'][p]==h),'Actual source fingerprint differs: '+p)
 require(root['script_sha']==sources[SCRIPT],'Actual script Tree fingerprint differs')
 player=(source/paths[-1]).read_text(encoding='utf-8');require('return _state' in player,'Player get_state changed')
 states=[x.strip()for x in re.search(r'enum \{([^}]+)\}',player,re.S)[1].split(',')if x.strip()]
 global_source=(source/paths[-3]).read_text(encoding='utf-8'); ui=(source/paths[-2]).read_text(encoding='utf-8')
 require('if globaldata.rumble:\n\t\tInput.start_joy_vibration(device_id, weak_magnitude, strong_magnitude, duration)'in global_source and 'func is_game_over() -> bool:\n\treturn _game_over'in ui,'Actual global/UI guard changed')
 engine,engine_row=engine_input_source();start=engine.index('void InputDefault::start_joy_vibration(');end=engine.index('void InputDefault::stop_joy_vibration(',start);input_start=engine[start:end]
 require(all(x in input_start for x in ['p_weak_magnitude < 0.f','p_strong_magnitude > 1.f','OS::get_singleton()->get_ticks_usec()','joy_vibration[p_device] = vibration;']) and 'joypad' not in input_start,'InputDefault request/device-independent semantics changed')
 initial=re.search(r'^var (_game_over) := (false|true)$',ui,re.M)
 require(initial is not None,'Actual UI game-over constructor declaration changed')
 return dict(schema=2,engine_input_commit=ENGINE,engine_input_source_sha256=engine_row['sha256'],engine_input_start_sha256=hashlib.sha256(input_start.encode()).hexdigest(),ui_game_over_member=initial[1],ui_game_over_initial=initial[2]=='true',ui_source=paths[-2],ui_game_over_getter_sha256=hashlib.sha256(re.search(r'^func is_game_over\([^\n]*\n(?:[^\n]*\n)*?(?=^func |\Z)',ui,re.M).group().encode()).hexdigest(),kind='encore.house-return-camera-control.source-ir',commit=PIN,scene=SCENE,scene_id=t['scene_id'],source_sha256=t['source_sha256'],tree_ir_sha256=sha(tree),producer_sha256=sha(__file__),sources=sources,script=SCRIPT,prototype=PROTOTYPE,sound=sound,bus=re.search(r'^bus = "([^"]+)"',proto,re.M)[1],records=[dict(id=r['id'],ready=r['ready'],node=r['node'],native_class=t['classes'][r['class_index']])for r in [root]+children],auto_start=False,**numbers,direction=[float(direction[1]),float(direction[2])],delay=float(re.search(r'func delayed_start\(time = ([0-9.]+)\)',text)[1]),joy=[float(v.strip())for v in re.search(r'global.start_joy_vibration\(([^)]+), length\)',text)[1].split(',')],player_camera=states.index('CAMERA'),player_state_member='_state',methods=METHODS,method_sha256={m:hashlib.sha256(re.search(r'^func '+m+r'\([^\n]*\n(?:[^\n]*\n)*?(?=^func |\Z)',text,re.M).group().encode()).hexdigest()for m in METHODS},semantics=['One actual House RoomShaker Control, no fixed House Camera2D node','Native repeating Timer remains in the sole actual internal traversal; strict time_left==0 start gate','SceneTreeTimer default process_pause=true; multiple delayed_start waiters survive stop_shake','Timeout battle/game-over/actual Player CAMERA guards short circuit in source order','Camera shake, audio play, global rumble gate/Input call precede shared rand_range and f32 Timer setter','CurrentCamera receiver must be a concrete source camera; unknown receivers reject','No whole House Ready approval, second VM, private timer or copied RNG'],unverified=['Manual negative cases not run','ARM integration and hardware run not performed'])
def pack(d):
 b=bytearray(bytes.fromhex(d['commit'])+struct.pack('<I',d['scene_id'])+bytes.fromhex(d['source_sha256'])+bytes.fromhex(d['tree_ir_sha256'])+bytes.fromhex(sha(IR)))
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def f(*v):b.extend(struct.pack('<'+'d'*len(v),*v))
 def s(v):x=v.encode();u(len(x));b.extend(x)
 for r in d['records']:u(r['id'],r['ready']);s(r['node']);s(r['native_class'])
 u(int(d['auto_start']),d['player_camera']);f(d['wait_time'],d['wait_margin'],d['magnitude'],d['length'],d['delay'],*d['direction'],*d['joy'])
 for x in ['script','prototype','sound','bus','player_state_member']:s(d[x])
 s(d['ui_source']);s(d['ui_game_over_member']);u(int(d['ui_game_over_initial']));b.extend(bytes.fromhex(d['ui_game_over_getter_sha256']));b.extend(bytes.fromhex(d['engine_input_source_sha256']));b.extend(bytes.fromhex(d['engine_input_start_sha256']));b.extend(bytes.fromhex(d['engine_input_commit']))
 u(len(d['sources']))
 for p,h in sorted(d['sources'].items()):s(p);b.extend(bytes.fromhex(h))
 u(len(d['methods']))
 for m in d['methods']:s(m);b.extend(bytes.fromhex(d['method_sha256'][m]))
 return struct.pack('<8s6I',b'ENCHSHK1',2,32+len(b),zlib.crc32(b),0x48525331,2,1)+b
def bundle_context(d):return dict(scene=d['scene'],scene_id=d['scene_id'],source_sha256=d['source_sha256'])
def stage_files(root):
 d=json.loads(IR.read_text(encoding='utf-8'));b=pack(d);p=Path('data/house-return-camera-control.encshaker');require((Path(root)/p).read_bytes()==b,'Staged RoomShaker pack differs');return{p:b}
def main():
 a=argparse.ArgumentParser();a.add_argument('action',choices=['extract','compile']);a.add_argument('--source',type=Path,default=ROOT/'upstream/MOTHER-Encore');args=a.parse_args();d=derive(args.source)
 if args.action=='extract':write(IR,d)
 else:require(json.loads(IR.read_text(encoding='utf-8'))==d,'Actual reviewed IR is stale')
 write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),producer_sha256=sha(__file__),sources=d['sources'],method_sha256=d['method_sha256'],engine_input=dict(commit=d['engine_input_commit'],source='main/input_default.cpp',sha256=d['engine_input_source_sha256'],start_sha256=d['engine_input_start_sha256'],semantics='Validated magnitude guard then device-independent parameter/timestamp request write; no hardware call in source method'),ui_game_over=dict(source=d['ui_source'],member=d['ui_game_over_member'],initial=d['ui_game_over_initial'],getter_sha256=d['ui_game_over_getter_sha256'],unsupported_mutations=['game_over','_on_game_over_done']),semantics=d['semantics'],unverified=d['unverified'],grants_house_ready=False))
 b=pack(d);PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(b);print('Actual House RoomShaker: 1 source Control, 2 native children, '+str(len(b))+' bytes; no Ready approval')
if __name__=='__main__':
 try:main()
 except(ValueError,OSError,KeyError,TypeError,AttributeError,struct.error)as e:sys.exit('HOUSE ROOM SHAKER: '+str(e))
