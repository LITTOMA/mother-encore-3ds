#!/usr/bin/env python3
"""Dialogue recipe-specific extraction; reuse audited native camera/arrow codecs.
The extractor accepts the complete official 47-node export only. Static Podunk
records and private paths are never substituted for the dynamic recipe.
"""
from __future__ import annotations
import re,struct,hashlib
from pathlib import Path
from tools.podunk_scene import PIN,read,write,decode,sha,require
ROOT=Path(__file__).resolve().parents[1]
SCENE='Nodes/Ui/DialogueBox.tscn'
SHAKER='Scripts/misc/Shaker.gd';PLAYER='Scripts/Main/party/Player.gd'
IMAGE='Graphics/UI/Inventory/cursor.png';TEXTURE='graphics/ui/map-arrows/cursor.t3x'
ARROWS_IR=ROOT/'content/dialogue-camera-arrows.json'
CAMERA_IR=ROOT/'content/dialogue-game-camera.json'
def stable(path):
 return int.from_bytes(hashlib.sha256(('recipe:'+SCENE+'#'+path).encode()).digest()[:4],'little')
def source_context(native):
 recipe=read(ROOT/'content/dialogue-node-recipe.json');d=read(native)
 require(recipe['commit']==PIN and recipe['scene']==SCENE and len(recipe['records'])==47 and d['source']=='res://'+SCENE and len(d['nodes'])==47 and sha(native)==recipe['native_sha256'],'Dialogue full native recipe mismatch')
 inv=read(ROOT/'compatibility/upstream-inventory.json')['files'];sources=dict(recipe['sources'])
 for path in [SHAKER,PLAYER,'LICENSE','project.godot',IMAGE+'.import']:
  sources[path]=inv[path]['sha256']
 for path,value in sources.items():
  require(value==inv[path]['sha256']==sha(ROOT/'upstream/MOTHER-Encore'/path),'Dialogue visual source changed '+path)
 return dict(sources=sources,export_sha256=sha(native),world={r['node']:r['world'] for r in recipe['records']},parent={r['node']:r['parent'] for r in recipe['records']},pending=[dict(script=r['script'],node=r['node'],stable_id=r['id'],ready_ordinal=r['ready']) for r in recipe['records'] if r['script'] in ['Scripts/Main/Camera2D.gd','Scripts/UI/MapScreen/MapArrows.gd']])

def extract_arrows(native):
 SCRIPT='Scripts/UI/MapScreen/MapArrows.gd';PROTOTYPE='Nodes/Ui/MapScreen/MapArrows.tscn';d=read(native);g=source_context(native);sources=dict(g['sources']);inv=read(ROOT/'compatibility/upstream-inventory.json')['files'];nm={n['path']:n for n in d['nodes']};rs={r['id']:r for r in d['resources']};require(d['source']=='res://'+SCENE and len(nm)==47 and sha(native)==g['export_sha256'],'MapArrows full source identity differs');text=(ROOT/'upstream/MOTHER-Encore'/SCRIPT).read_text();methods=re.findall(r'^func ([A-Za-z_][A-Za-z0-9_]*)\(',text,re.M);require(methods==['_ready','show','hide','_refresh_show_hide','handle_input_events','point_directions','unpoint_directions','point_dir_sum','set_offset','set_bounds','_refresh_visibility','set_arrow_visible','_on_anim_finished'],'MapArrows source methods changed');directions=re.findall(r'Vector2\.(UP|DOWN|LEFT|RIGHT): \$(arrow[A-Z])',text);require(directions==[('UP','arrowU'),('DOWN','arrowD'),('LEFT','arrowL'),('RIGHT','arrowR')],'MapArrows dictionary order changed')
 for f in ['_show_arrows = visible','_refresh_show_hide(true)','_refresh_show_hide(visible)','_refresh_show_hide(!visible)','play(anim_name, -1, 0, true)','get_just_pressed_directions()','get_just_released_directions()','anim_player.assigned_animation != "Point"','anim_player.assigned_animation != "UnPoint"','Vector2(sign(dir_sum.x), 0), Vector2(0, sign(dir_sum.y))','if arrow != other_arrow and other_arrow.visible and other_arrow.playing:','yield(other_arrow, "frame_changed")','arrow.frame = other_arrow.frame']:require(f in text,'MapArrows changed '+f)
 prototype=(ROOT/'upstream/MOTHER-Encore'/PROTOTYPE).read_text();atlas={}
 for aid,body in re.findall(r'^\[sub_resource type="AtlasTexture" id=(\d+)\]\n(.*?)(?=^\[|\Z)',prototype,re.M|re.S):
  require(re.findall(r'^([a-z_]+) =',body,re.M)==['atlas','region']and 'atlas = ExtResource( 3 )'in body,'MapArrows original atlas unknown property');region=re.search(r'region = Rect2\( ([^)]*) \)',body);require(region,'MapArrows original region missing');atlas['res://'+PROTOTYPE+'::'+aid]=[float(x)for x in region[1].split(',')]
 require(len(atlas)==3 and '[ext_resource path="res://'+IMAGE+'" type="Texture" id=3]'in prototype,'MapArrows original atlas source binding differs')
 states={s['source'][6:]:s for s in d['scene_states']};roots=[]
 def instances(root,f):
  roots.append((root,f))
  for n in states[f]['nodes']:
   if n['instance']:
    local=n['path'].removeprefix('./');instances(local if root=='.'else root+'/'+local,rs[n['instance']['id']]['path'][6:])
 instances('.',SCENE);overrides={}
 for root,f in sorted(roots,key=lambda v:v[0].count('/')if v[0]!='.'else-1,reverse=True):
  for n in states[f]['nodes']:
   path=n['path'].removeprefix('./');path=root if path=='.'else path if root=='.'else root+'/'+path
   if path in nm:overrides.setdefault(path,{}).update(decode(n['properties']))
 children={p:[]for p in nm}
 for p in nm:
  if p!='.':children[p.rsplit('/',1)[0]if'/'in p else'.'].append(p)
 ready=[]
 def visit(p):
  for c in children[p]:visit(c)
  ready.append(p)
 visit('.');ordinal={p:i for i,p in enumerate(ready)};profiles=[];sprites=[];records=[];frames=None;rootclips=['Come In','Come Out','RESET'];arrowclips=['Point','UnPoint','RESET'];property_role={'frame':1,'offset':2,'playing':3,'position':4,'visible':5};property_kind={'frame':'int','offset':'vector','playing':'bool','position':'vector','visible':'bool'}
 def player(path,target_root,kind,targets):
  a=decode(nm[path]['properties']);require(a['playback_process_mode']==1 and a['playback_speed']>0 and a['autoplay']==''and a['blend_times']==[]and a['playback_default_blend_time']==0,'MapArrows unknown AnimationPlayer state');out=[]
  for role,name in enumerate(rootclips if kind==0 else arrowclips,1):
   v=decode(rs[a['anims/'+name]['id']]['properties']);tracks=[];i=0
   while'tracks/'+str(i)+'/type'in v:
    pre='tracks/'+str(i)+'/';key=dict(v[pre+'keys']['pairs']);ref=v[pre+'path']['value'];node,prop=ref.split(':');require(prop in property_role and node in targets and v[pre+'type']=='value'and v[pre+'enabled']and v[pre+'interp']==1 and v[pre+'loop_wrap']and key['update']in[0,1],'MapArrows unsupported native track '+ref);values=key['values'];typed=[]
    for value in values:
     if property_kind[prop]=='vector':require(type(value)is list and len(value)==2,'MapArrows expected Vector2');typed.append(value)
     elif property_kind[prop]=='bool':require(type(value)is bool,'MapArrows expected bool');typed.append([float(value),0])
     else:require(type(value)is int,'MapArrows expected int');typed.append([float(value),0])
    require(len(typed)==len(key['times'])==len(key['transitions']),'MapArrows native keys differ');tracks.append(dict(target=targets[node],property=property_role[prop],update=key['update'],keys=[dict(time=t,transition=c,value=value)for t,c,value in zip(key['times'],key['transitions'],typed)]));i+=1
   out.append(dict(role=role,name=name,length=v['length'],loop=v['loop'],tracks=tracks))
  if out not in profiles:profiles.append(out)
  return dict(id=stable(path),ready=ordinal[path],target_root=target_root,kind=kind,profile=profiles.index(out),speed=a['playback_speed'],pause=a['pause_mode'],priority=a['process_priority'])
 players=[]
 for b in g['pending']:
  if b['script']!=SCRIPT:continue
  p=b['node'];v=decode(nm[p]['properties']);require(v['rotation']==0 and v['scale']==[1,1]and v['material']is None and not v['use_parent_material'],'MapArrows root material/transform unsupported');arrow_ids=[]
  for index,(_,part)in enumerate(directions):
   path=p+'/'+part;s=decode(nm[path]['properties']);require(s['animation']=='Idle'and not s['flip_h']and not s['flip_v']and s['scale']==[1,1]and s['material']is None and not s['use_parent_material'],'MapArrows source sprite unsupported');a=dict(decode(rs[s['frames']['id']]['properties'])['animations'][0]['pairs']);require(a['name']=='Idle'and len(a['frames'])==4 and a['loop'],'MapArrows SpriteFrames differs');f=[]
   for ref in a['frames']:
    q=rs[ref['id']];require(q['class']=='AtlasTexture'and q['path']in atlas and decode(q['size'])==atlas[q['path']][2:],'MapArrows native/original atlas binding differs');f.append(atlas[q['path']])
   if frames is None:frames=f
   require(frames==f,'MapArrows source frame roster differs');color=lambda q:[q[k]for k in ['r','g','b','a']];sid=stable(path);arrow_ids.append(sid);sprites.append(dict(id=sid,root_id=b['stable_id'],ready=ordinal[path],direction=index,frame=overrides[path].get('frame',0),flags=sum(int(x)<<i for i,x in enumerate([s['visible'],s['playing'],s['centered']])),position=s['position'],offset=s['offset'],scale=s['scale'],rotation=s['rotation'],speed_scale=s['speed_scale'],animation_speed=a['speed'],pause=s['pause_mode'],priority=s['process_priority'],modulate=color(s['modulate']),self_modulate=color(s['self_modulate'])));players.append(player(path+'/AnimationPlayer',sid,1,{'.':0}))
  players.append(player(p+'/AnimationPlayer',b['stable_id'],0,{'.':0,**{part:i+1 for i,(_,part)in enumerate(directions)}}));color=lambda q:[q[k]for k in ['r','g','b','a']];records.append(dict(id=b['stable_id'],ready=b['ready_ordinal'],parent_id=g['parent'][p],node=p,arrows=arrow_ids,position=v['position'],visible=v['visible'],z=v['z_index'],z_relative=v['z_as_relative'],pause=v['pause_mode'],priority=v['process_priority'],modulate=color(v['modulate']),self_modulate=color(v['self_modulate'])))
 require(len(records)==1 and len(sprites)==4 and len(players)==5 and len(profiles)==2,'Incomplete MapArrows full source scope');image=(ROOT/'upstream/MOTHER-Encore'/IMAGE).read_bytes();width,height=struct.unpack('>II',image[16:24]);require('flags/filter=false'in(ROOT/'upstream/MOTHER-Encore'/(IMAGE+'.import')).read_text(),'MapArrows source filtering changed');require('2d/snapping/use_gpu_pixel_snap=true'in(ROOT/'upstream/MOTHER-Encore/project.godot').read_text(),'MapArrows pixel snap source changed')
 for p in [SCRIPT,PROTOTYPE,IMAGE,IMAGE+'.import','LICENSE','project.godot']:sources[p]=inv[p]['sha256']
 for p,h in sources.items():require(h==inv[p]['sha256']==sha(ROOT/'upstream/MOTHER-Encore'/p),'MapArrows changed source '+p)
 write(ARROWS_IR,dict(schema=1,kind='encore.field-camera-arrows.source-ir',commit=PIN,scene=SCENE,scene_id=stable('.'),source_sha256=sources[SCENE],scene_admitted=False,native_sha256=sha(native),sources=sources,program=dict(source='platform/ctr/shaders/field_camera_arrows.v.pica',path='shaders/field-camera-arrows.shbin',source_sha256=sha(ROOT/'platform/ctr/shaders/field_camera_arrows.v.pica')),records=records,sprites=sprites,players=players,profiles=profiles,frames=frames,asset=dict(source=IMAGE,path=TEXTURE,width=width,height=height),pixel_snap=True,directions=[[0,-1],[0,1],[-1,0],[1,0]],pending=['Actual liveGameCamera, source parent/material/Canvas and OS update_pending/process gates','Real synchronous AnimationPlayer/AnimatedSprite signals and ordered one-shot frame_changed resync','No generic input aliases: controlsManager pressed/released direction arrays must preserve source dictionary directions','Dynamic DialogueBox Camera and other scene classes independently pending']))
def extract_camera(native):
 SCRIPT='Scripts/Main/Camera2D.gd';PROTOTYPE='Nodes/Ui/Camera.tscn';d=read(native);g=source_context(native);inv=read(ROOT/'compatibility/upstream-inventory.json')['files'];sources=dict(g['sources']);nm={n['path']:n for n in d['nodes']};rs={r['id']:r for r in d['resources']};require(d['source']=='res://'+SCENE and len(nm)==47 and sha(native)==g['export_sha256'],'GameCamera complete original source required');cam=(ROOT/'upstream/MOTHER-Encore'/SCRIPT).read_text();shake=(ROOT/'upstream/MOTHER-Encore'/SHAKER).read_text();player=(ROOT/'upstream/MOTHER-Encore'/PLAYER).read_text()
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
  records.append(dict(id=b['stable_id'],ready=b['ready_ordinal'],node=p,parent_id=g['parent'][p],arrows_id=stable(p+'/ScopeArrows'),animation_id=stable(ap),animation_ready=ordinal[ap],area_id=stable(a),shape_id=stable(sp),position=v['position'],offset=v['offset'],world=g['world'][p],zoom=v['zoom'],rotation=v['rotation'],flags=sum(int(x)<<i for i,x in enumerate([v['current'],v['rotating'],v['visible'],v['z_as_relative']])),limits=[v['limit_'+k]for k in ['top','left','right','bottom']],z=v['z_index'],pause=v['pause_mode'],priority=v['process_priority'],physics_interpolation=v['physics_interpolation_mode'],area_position=av['position'],area_scale=av['scale'],shape_position=sv['position'],shape_scale=sv['scale'],shape_extents=ext,area_layer=av['collision_layer'],area_mask=av['collision_mask'],area_flags=sum(int(x)<<i for i,x in enumerate([av['monitoring'],av['monitorable'],sv['disabled'],sv['one_way_collision']]))))
 require(len(records)==1,'GameCamera full scope count differs')
 for p in [SCRIPT,SHAKER,PROTOTYPE,PLAYER,'Scripts/UI/MapScreen/MapArrows.gd','Nodes/Ui/MapScreen/MapArrows.tscn','LICENSE']:sources[p]=inv[p]['sha256']
 for p,h in sources.items():require(h==inv[p]['sha256']==sha(ROOT/'upstream/MOTHER-Encore'/p),'Changed GameCamera source '+p)
 tuning=[constants[k]for k in ['CAM_LIMIT','SCOPE_MOVE_SPEED','SCOPE_VERTICAL_LIMIT','SCOPE_HORIZONTAL_LIMIT','SHAKE_STEP_TIME']]+[values[k]for k in ['scope_return','return_time','final_time','shake_magnitude','shake_length','shake_weight','shake_minimum','shake_small','shake_last','shake_direct','shake_side']]
 write(CAMERA_IR,dict(schema=1,kind='encore.field-game-camera.source-ir',commit=PIN,scene=SCENE,scene_id=stable('.'),source_sha256=sources[SCENE],native_sha256=sha(native),scene_admitted=False,sources=sources,constants=constants,tuning=tuning,player_states=[values['player_camera'],values['player_attack']],animation_lengths=lengths,shake_direction=[1,1],shake_side_amplitude=[1,1],scope_action='ui_scope',records=records,pending=['Actual admitted ancestors incl JumpArea; active player/UI/input/currentCamera source bus','Real viewport/native Camera canvas and source global SceneTree tween ordering, dynamic Shaker factory','MapArrows bridge with actual source visibility/position/input and no duplicated leaf clocks','Player Camera belongs to a separate source scene and is not admitted by this DialogueBox camera record']))
