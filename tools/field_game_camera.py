#!/usr/bin/env python3
"""Complete actual14 Podunk GameCamera + dynamically created source Shaker."""
from __future__ import annotations
import argparse,hashlib,re,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,SCENE,read,write,decode,stable,sha,require
SCRIPT='Scripts/Main/Camera2D.gd';SHAKER='Scripts/misc/Shaker.gd';PROTOTYPE='Nodes/Ui/Camera.tscn';PLAYER='Scripts/Main/party/Player.gd';IR=ROOT/'content/podunk-game-camera.json';REVIEW=ROOT/'compatibility/reviews/podunk-game-camera-v0410.json';OUT=ROOT/'romfs/data/podunk.encgamecamera'

def extract(native):
 d=read(native);g=read(ROOT/'content/podunk-scene.json');inv=read(ROOT/'compatibility/upstream-inventory.json')['files'];sources=dict(g['sources']);nm={n['path']:n for n in d['nodes']};rs={r['id']:r for r in d['resources']};require(d['source']=='res://'+SCENE and len(nm)==8686 and sha(native)==g['export_sha256'],'GameCamera complete original source required');cam=(ROOT/'upstream/MOTHER-Encore'/SCRIPT).read_text();shake=(ROOT/'upstream/MOTHER-Encore'/SHAKER).read_text();player=(ROOT/'upstream/MOTHER-Encore'/PLAYER).read_text()
 require(re.findall(r'^func ([A-Za-z_][A-Za-z0-9_]*)\(',cam,re.M)==['_ready','_process','_physics_process','_scoping_start','_scoping_stop','_scoping_process','_input','_on_player_pause','move_camera','move_offset','return_camera','return_offset','set_camarea_offset','get_offset_with_camerea_offset','reset','shake_camera','is_shaking','set_current'],'Changed Camera source methods')
 require(re.findall(r'^func ([A-Za-z_][A-Za-z0-9_]*)\(',shake,re.M)==['_init','set_shake_direction','set_shake_magnitude','set_shake_length','set_shake_interval','set_shake_weight','set_shake_side_amplitude','set_shake_diminish','_calculate_shakes_left','_update_shakes_left','_update_shake_reduction','start','pause','is_playing','stop','_physics_process'],'Changed Shaker methods')
 constants={k:float(v)for k,v in re.findall(r'^const ([A-Z_]+) := ([0-9.]+)',cam,re.M)};require(len(constants)==9,'Camera constant roster changed')
 # Every tuning value is reviewed from this immutable source, including currently unused constants.
 patterns={'scope_return':r'return_offset\(([0-9.]+)\)','return_time':r'func return_camera\(time := ([0-9.]+)\)','final_time':r'"_shake_offset", Vector2.ZERO, ([0-9.]+)\)','shake_magnitude':r'func shake_camera\(magnitude := ([0-9.]+)','shake_length':r'func shake_camera\([^\n]*length := ([0-9.]+)','shake_weight':r'func shake_camera\([^\n]*lerp_weight := ([0-9.]+)'}
 values={k:float(re.search(v,cam)[1])for k,v in patterns.items()};require('var _shake_side_amplitude := Vector2.ONE'in shake,'Changed source Shaker amplitude');require('interval := SHAKE_STEP_TIME'in cam and 'direction := Vector2.ONE'in cam and 'diminish := true'in cam,'Camera shake default changed')
 values.update(shake_minimum=float(re.search(r'max\(_shake_magnitude, ([0-9.]+)\)',shake)[1]),shake_small=float(re.search(r'_shake_magnitude <= ([0-9.]+) and',shake)[1]),shake_last=int(re.search(r'if _shakes_left > ([0-9]+):\n\t+new_value = new_offset',shake)[1]),shake_direct=float(re.search(r'if _shake_magnitude <= ([0-9]+):',shake)[1]),shake_side=int(re.search(r'var _shake_side = ([0-9]+)',shake)[1]))
 enum=[v.strip()for v in re.search(r'enum \{([^}]+)\}',player,re.S)[1].split(',')if v.strip()];require('CAMERA'in enum and 'ATTACK'in enum,'Camera source Player enums absent');values['player_camera']=enum.index('CAMERA');values['player_attack']=enum.index('ATTACK')
 for fact in ['if get_parent() == global.get_player():','global.currentCamera = self','_base_offset += move_direction * SCOPE_MOVE_SPEED','for i in [position, _base_offset, _shake_offset]:','yield(get_tree(), "idle_frame")','if tween and tween.is_running(): tween.kill()','if tween: tween.kill()','tween.parallel().tween_property','if _scope_arrows.visible: return']:require(fact in cam,'Unreviewed Camera behavior '+fact)
 for fact in ['while (_dir == old__dir):','_dir = Vector2(round(rand_range(-1, 1)), round(rand_range(-1, 1)))','_timer -= _shake_interval','_shaked_object = null','queue_free()','_shake_magnitude -= _shake_magnitude_reduction']:require(fact in shake,'Unreviewed Shaker behavior '+fact)
 children={p:[]for p in nm}
 for p in nm:
  if p!='.':children[p.rsplit('/',1)[0]if'/'in p else'.'].append(p)
 order=[]
 def visit(p):
  for c in children[p]:visit(c)
  order.append(p)
 visit('.');ordinal={p:i for i,p in enumerate(order)};records=[];lengths=None
 for b in g['pending']:
  if b['script']!=SCRIPT:continue
  p=b['node'];v=decode(nm[p]['properties']);a=p+'/Area2D';av=decode(nm[a]['properties']);sp=a+'/CollisionShape2D';sv=decode(nm[sp]['properties']);shape=rs[sv['shape']['id']];ext=decode(shape['properties'])['extents'];ap=p+'/ArrowsAnim';an=decode(nm[ap]['properties']);local=[]
  for name in ['Come In','Come Out','RESET']:
   x=decode(rs[an['anims/'+name]['id']]['properties']);require(not x['loop']and not any(k.startswith('tracks/')for k in x),'Camera native arrows has unknown tracks');local.append(x['length'])
  if lengths is None:lengths=local
  require(local==lengths and an['playback_process_mode']==1 and an['playback_speed']==1 and an['autoplay']==''and an['blend_times']==[]and an['playback_default_blend_time']==0,'Unreviewed Camera ArrowsAnim')
  require(v['anchor_mode']==1 and v['zoom']==[1,1]and v['process_mode']==0 and not v['smoothing_enabled']and not v['limit_smoothed']and not v['drag_margin_h_enabled']and not v['drag_margin_v_enabled']and v['offset_h']==v['offset_v']==0 and v['rotation']==0 and v['scale']==[1,1]and v['material']is None and not v['use_parent_material']and shape['class']=='RectangleShape2D','Unreviewed native Camera configuration')
  records.append(dict(id=b['stable_id'],ready=b['ready_ordinal'],node=p,parent_id=stable(p.rsplit('/',1)[0]),arrows_id=stable(p+'/ScopeArrows'),animation_id=stable(ap),animation_ready=ordinal[ap],area_id=stable(a),shape_id=stable(sp),position=v['position'],offset=v['offset'],world=decode(nm[p]['world_transform']),zoom=v['zoom'],rotation=v['rotation'],flags=sum(int(x)<<i for i,x in enumerate([v['current'],v['rotating'],v['visible'],v['z_as_relative']])),limits=[v['limit_'+k]for k in ['top','left','right','bottom']],z=v['z_index'],pause=v['pause_mode'],priority=v['process_priority'],physics_interpolation=v['physics_interpolation_mode'],area_position=av['position'],area_scale=av['scale'],shape_position=sv['position'],shape_scale=sv['scale'],shape_extents=ext,area_layer=av['collision_layer'],area_mask=av['collision_mask'],area_flags=sum(int(x)<<i for i,x in enumerate([av['monitoring'],av['monitorable'],sv['disabled'],sv['one_way_collision']]))))
 require(len(records)==14,'GameCamera full scope count differs')
 for p in [SCRIPT,SHAKER,PROTOTYPE,PLAYER,'Scripts/UI/MapScreen/MapArrows.gd','Nodes/Ui/MapScreen/MapArrows.tscn','LICENSE']:sources[p]=inv[p]['sha256']
 for p,h in sources.items():require(h==inv[p]['sha256']==sha(ROOT/'upstream/MOTHER-Encore'/p),'Changed GameCamera source '+p)
 tuning=[constants[k]for k in ['CAM_LIMIT','SCOPE_MOVE_SPEED','SCOPE_VERTICAL_LIMIT','SCOPE_HORIZONTAL_LIMIT','SHAKE_STEP_TIME']]+[values[k]for k in ['scope_return','return_time','final_time','shake_magnitude','shake_length','shake_weight','shake_minimum','shake_small','shake_last','shake_direct','shake_side']]
 write(IR,dict(schema=1,kind='encore.field-game-camera.source-ir',commit=PIN,scene=SCENE,scene_id=stable('.'),source_sha256=sources[SCENE],native_sha256=sha(native),scene_admitted=False,sources=sources,constants=constants,tuning=tuning,player_states=[values['player_camera'],values['player_attack']],animation_lengths=lengths,shake_direction=[1,1],shake_side_amplitude=[1,1],scope_action='ui_scope',records=records,pending=['Actual admitted ancestors incl JumpArea; active player/UI/input/currentCamera source bus','Real viewport/native Camera canvas and source global SceneTree tween ordering, dynamic Shaker factory','MapArrows bridge with actual source visibility/position/input and no duplicated leaf clocks','Player Camera belongs to a separate source scene and is not admitted by these14 records']))

def load():
 d=read(IR);r=read(REVIEW);inv=read(ROOT/'compatibility/upstream-inventory.json')['files'];require(d['schema']==1 and d['commit']==PIN and d['scene']==SCENE and not d['scene_admitted']and r['commit']==PIN and r['ir_sha256']==sha(IR),'GameCamera reviewed IR differs')
 for p,h in d['sources'].items():require(h==inv[p]['sha256']==sha(ROOT/'upstream/MOTHER-Encore'/p),'Changed GameCamera proof '+p)
 return d

def encode(d):
 b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def i(*v):b.extend(struct.pack('<'+'i'*len(v),*v))
 def f(*v):b.extend(struct.pack('<'+'f'*len(v),*v))
 def t(s):v=s.encode();u(len(v));b.extend(v)
 t(d['scene']);t(SCRIPT);t(SHAKER);t(d['scope_action']);u(*d['player_states']);f(*d['shake_direction'],*d['shake_side_amplitude']);b.extend(struct.pack('<16d',*d['tuning']));f(*d['animation_lengths'])
 for r in d['records']:
  u(*[r[k]for k in ['id','ready','parent_id','arrows_id','animation_id','animation_ready','area_id','shape_id','flags','pause','physics_interpolation']]);i(r['z'],r['priority'],*r['limits']);f(*r['position'],*r['offset'],*[v for row in r['world']for v in row],*r['zoom'],r['rotation'],*r['area_position'],*r['area_scale'],*r['shape_position'],*r['shape_scale'],*r['shape_extents']);u(r['area_layer'],r['area_mask'],r['area_flags']);t(r['node'])
 u(len(d['sources']))
 for p,h in d['sources'].items():t(p);b.extend(bytes.fromhex(h))
 struct.pack_into('<8s8I',b,0,b'ENCFGCM1',1,128,len(b),0,0x454e0031,3,len(d['records']),d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['source_sha256']);b[92:124]=hashlib.sha256(IR.read_bytes()).digest();struct.pack_into('<I',b,20,zlib.crc32(b[128:]));return bytes(b)

def stage_files(source):
 raw=encode(load());require((Path(source)/'data/podunk.encgamecamera').read_bytes()==raw,'StagedGameCamera differs');return {Path('data/podunk.encgamecamera'):raw}

def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);p.add_argument('--native',type=Path);a=p.parse_args()
 if a.action=='extract':require(a.native,'Explicit complete native source required');extract(a.native);return
 d=load();raw=encode(d)
 if a.action=='verify':require(OUT.read_bytes()==raw,'GameCamera stale output')
 else:OUT.parent.mkdir(parents=True,exist_ok=True);OUT.write_bytes(raw)
 print('GameCamera source:',len(d['records']),'actual cameras;',len(raw),'bytes; scene_admitted=False')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,TypeError,OSError,struct.error)as e:sys.exit('FIELD GAME CAMERA ERROR: '+str(e))
