#!/usr/bin/env python3
"""Compile reviewed battle-entry IR into a separate checked ENCBTL01 resource."""
import argparse,hashlib,json,math,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
STRIDES=[1,60,92,24,24,20,64,16,32,60,16,36]
PARAMETERS=['CanvasSize','SceneDuration','MaskDuration','MaskColor','MaskGrid','MenuDuration','EnemyMoveDuration','EnemyMoveScaleDuration','EnemyMoveScaleDelay','EnemyMoveInitialScale','PlayerJumpDuration','PlayerJumpHeight','CursorDuration','CursorRepeatDelay','CursorRepeatInterval','PlateSize','MenuSpacing','EnemyShakeInterval','EnemyShakeMagnitude','FontMetrics','PartyJumpStart','PartyJumpTarget','PartyJumpScale','PartySquash','PartyShow','EnemyTint','BackdropColor','DigitGrid','PartyNudge','PartyScreenOffset','CursorScale1','CursorScale2','CursorScale3','PartyQuake1','PartyQuake2','PartyQuake3','PartyQuake4','PartyQuake5','MaskOldColor']
class ContentError(ValueError):pass
def require(value,message):
 if not value:raise ContentError(message)
def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def vec(v):
 if not isinstance(v,(list,tuple)):v=[v]
 require(len(v)<=4 and all(isinstance(x,(int,float))and math.isfinite(x)for x in v),'Invalid finite vector')
 return list(v)+[0]*(4-len(v))
def verify_sources(ir):
 from tools.extract_battle_entry import Extractor
 from tools.battle_entry_bindings import verify_ir
 verify_ir(ir,Extractor(ROOT))
 require(ir['schema']==1 and ir['kind']=='encore.native-battle-entry.source-ir','Unknown battle IR')
 for path,sha in ir['sources'].items():
  p=ROOT/'upstream/MOTHER-Encore'/path
  require(digest(p)==sha,'Changed source '+path)
def lower(ir,assets,room=None):
 from tools.extract_battle_entry import Extractor
 from tools.battle_entry_bindings import compiler
 entry_bindings=compiler(ir,assets,Extractor(ROOT));plate_binding=entry_bindings['plate'];depths=entry_bindings['compiler']['plate_depths'];menu_depths=entry_bindings['compiler']['menu_depths']
 pool=bytearray(b'\0');strings={'':0}
 def string(v):
  require(isinstance(v,str)and '\0'not in v,'Invalid string')
  if v not in strings:strings[v]=len(pool);pool.extend(v.encode()+b'\0')
  return strings[v]
 resources=[];names={}
 for r in assets['resources']:
  path=r['output'];names[r['name']]=len(resources)
  require(digest(ROOT/'romfs'/path)==assets['outputs'][Path(path).name]['sha256'],'Changed battle asset '+path)
  w,h=r['width'],r['height'];cols,rows=r['grid']
  resources.append([r['id']+1,string(path),1 if r['kind']=='texture'else 2,w,h,cols,rows,bytes.fromhex(digest(ROOT/'romfs'/path))])
 # Existing reviewed room textures are shared by reference, not recopied or named in C++.
 from native_content import parse_pack
 if 'binding' in ir:
  binding=ir['binding'];require(set(binding)=={'stable_id','player_instance','enemy_instance','world_resources'},'Unknown external battle binding')
  scene={'player_instance_index':binding['player_instance']};battle={'stable_id':binding['stable_id'],'actor_instance_index':binding['enemy_instance']}
  require(set(binding['world_resources'])=={'world_player','world_enemy'},'Unknown world transition resources')
  world_resources=binding['world_resources']
 else:
  rv=parse_pack(room);rs=rv['sections'];scene=rs['Scene'][0];battle=rs['Battle'][0];world_resources={}
  for name,instance in [('world_player',scene['player_instance_index']),('world_enemy',battle['actor_instance_index'])]:
   profile=rs['ActorProfile'][rs['ActorInstance'][instance]['profile_index']];r=rs['Resource'][profile['primary_resource']]
   world_resources[name]=dict(r,path=rv['strings'][r['path_string']])
 for name in ['world_player','world_enemy']:
  r=world_resources[name];path=r['path'];require(digest(ROOT/'romfs'/path)==r['sha256'],'Changed world transition resource '+path);names[name]=len(resources)
  resources.append([len(resources)+1,string(path),1,r['width'],r['height'],r['columns'],r['rows'],bytes.fromhex(r['sha256'])])
 presentation=ir['presentation'];entry=ir['entry'];derived=presentation['derived_layout'];anims={a['name']:a for a in ir['animations']}
 layouts=[];roles={};tracks=[];keys=[];events=[];menus=[]
 def layout(name,kind,role,resource,rect,color=(1,1,1,1),frame=0,text='',visible=True,centered=False,depth=0,margins=(0,0,0,0),binding=0):
  index=len(layouts);roles[name]=index
  require(resource is None or resource in names,'Unknown layout resource alias')
  layouts.append([index+1,kind,role,names[resource]if resource is not None else 0,frame,string(text),int(visible)|(int(centered)<<1),binding,vec(rect),vec(color),depth,list(margins)]);return index
 def track(target,prop,clock,times,values,eases=None,update=0):
  require(len(times)==len(values)>0,'Invalid track')
  first=len(keys);eases=eases or[1]*len(times)
  for t,v,e in zip(times,values,eases):keys.append([t,e,vec(v)])
  tracks.append([roles[target],prop,clock,first,len(times),update])
 def source_track(anim,path):
  matches=[t for t in anims[anim]['tracks']if t.get('path')==path];require(len(matches)==1,'Missing/ambiguous source track '+path);return matches[0]['keys']
 def apply_track(target,prop,clock,anim,path,transform=lambda x:x):
  k=source_track(anim,path);track(target,prop,clock,k['times'],[transform(x)for x in k['values']],k['transitions'],k['update'])
 viewport=[ir['viewport']['width'],ir['viewport']['height']]
 layout('mask',1,15,'transition',[0,0,*viewport],depth=1000)
 apply_track('mask',6,1,'overlay.Start','Sprite:frame')
 for name,role in [('top',11),('bottom',12)]:
  node=presentation['scene_nodes'][name];height=node.get('rect_min_size',[0,node.get('margin_bottom',0)-node['margin_top']])[1]
  layout(name,2,role,None,[0,node['margin_top'],node['margin_right'],height],node['color'],depth=15)
  apply_track(name,1,0,'scene.transitionIn',name+':rect_position')
 enemy=ir['enemy'];et=presentation['enemy_transition'];pt=presentation['party_transition']
 er=resources[names['world_enemy']];ew,eh=er[3]/er[5],er[4]/er[6]
 # The actor's final frame is supplied at the dynamic entry boundary; position too.
 layout('enemy_transition',1,1,'world_enemy',[*enemy['sprite_center'],ew,eh],centered=True,depth=10)
 track('enemy_transition',1,3,[0,et['duration']],[[0,0],enemy['sprite_center']],[0.5,1])
 track('enemy_transition',8,3,[0,et['tint_seconds']],[[1,1,1,1],et['tint_to']])
 layout('enemy',1,3,'enemy',[*enemy['sprite_center'],*enemy['sprite_size']],visible=False,centered=True,depth=11)
 for prop,path in [(4,'.:rect_scale'),(9,'.:material:shader_param/flash_color'),(10,'.:material:shader_param/flash_modifier')]:apply_track('enemy',prop,5,'enemy.appear',path)
 pr=resources[names['world_player']];pw,ph=pr[3]/pr[5],pr[4]/pr[6];crouch=pt['crouch_frame_coords'][1]*pr[5]+pt['crouch_frame_coords'][0]
 layout('party_transition',1,2,'world_player',[0,0,pw,ph],frame=crouch,centered=True,depth=30)
 squash=pt['squash_segment_seconds'];track('party_transition',4,0,[0,squash,squash*2],[[1,1],pt['squash_scale'],[1,1]],[0.25,0.25,1])
 jumpframe=pt['jump_frame_coords'][1]*pr[5]+pt['jump_frame_coords'][0]
 track('party_transition',6,4,[0],[jumpframe],update=1)
 # Dynamic jump positions are evaluated from the source formula in shared execution.
 layout('party',1,4,'party',[*derived['party_hidden_position'],*derived['party_sprite_size']],depth=20)
 # BattleParticipant parents the portrait below PartyInfoPlate and PlayerInfo.
 # Its inherited entry translation precedes its own later show_in tween.
 apply_track('party',3,0,'scene.transitionIn','PlayerInfo:rect_position:y',lambda v:derived['party_hidden_position'][1]+v-viewport[1])
 track('party',2,6,[0],[derived['party_shown_position'][0]],update=1)
 track('party',3,6,[0,presentation['party_sprite']['show_tween_seconds']],[derived['party_hidden_position'][1],derived['party_shown_position'][1]],[1,1])
 # One plate for the current scoped fresh-game party. All child positions are source/natively resolved.
 px,py=derived['plate_position'];plate_nodes=presentation['plate_nodes'];ps=derived['plate_size'];m=plate_nodes['.']
 content=plate_nodes[plate_binding['content']];counter_node=plate_nodes[plate_binding['counter']]
 content_pos=[content['margin_left'],content['margin_top']];content_size=[ps[i]+content['margin_'+side]-content_pos[i]for i,side in enumerate(['right','bottom'])]
 counter_pos=[content_size[0]*counter_node['anchor_left']+counter_node['margin_left'],counter_node.get('margin_top',0)]
 layout('plate_bg',5,5,plate_binding['background_asset'],[px+content_pos[0],py+content_pos[1],*content_size],depth=depths[0],margins=entry_bindings['compiler']['background_margins'])
 layout('plate',3,5,plate_binding['frame_asset'],[px,py,*ps],depth=depths[1],margins=[m['patch_margin_'+side]for side in ['left','top','right','bottom']])
 for stat_binding,label_asset in zip(plate_binding['stats'],plate_binding['label_assets']):
  what=stat_binding['stat'];node=plate_nodes[stat_binding['label']]
  layout(what+'_label',1,5,label_asset,[px+node['margin_left'],py+node['margin_top'],node['margin_right']-node['margin_left'],node['margin_bottom']-node['margin_top']],depth=depths[2])
  node=plate_nodes[stat_binding['counter']];counter_resource=resources[names[plate_binding['counter_asset']]]
  layout(what+'_counter',1,5,plate_binding['counter_asset'],[px+content_pos[0]+counter_pos[0]+node['margin_left'],py+content_pos[1]+counter_pos[1]+node['margin_top'],counter_resource[3],counter_resource[4]],depth=depths[2])
  stat=ir['party']['effective_stats'][what];digits_resource=resources[names[plate_binding['digits_asset']]]
  for digit in stat_binding['digits']:
   node=plate_nodes[digit['node']];value=(stat//(10**digit['power']))%10
   layout(digit['label'],1,stat_binding['role'],plate_binding['digits_asset'],[px+content_pos[0]+counter_pos[0]+node['position'][0],py+content_pos[1]+counter_pos[1]+node['position'][1],digits_resource[3]/digits_resource[5],digits_resource[4]/digits_resource[6]],frame=value*digits_resource[5],visible=not(digit['binding']==0 and value==0),depth=depths[3],binding=digit['binding'])
 name=ir['party']['initial_save_data']['name'];name_node=plate_nodes[plate_binding['name']]
 name_size=[ps[0]*name_node['anchor_right']+name_node.get('margin_right',0)-name_node['margin_left'],name_node['margin_bottom']-name_node['margin_top']]
 layout('party_name',4,6,plate_binding['name_asset'],[px+name_node['margin_left'],py+name_node['margin_top'],*name_size],name_node['custom_colors/font_color'],text=name,depth=depths[4])
 for name in list(roles):
  if layouts[roles[name]][2]in[5,6,7,8]:
   base=layouts[roles[name]][8][1]
   apply_track(name,3,0,'scene.transitionIn','PlayerInfo:rect_position:y',lambda v,b=base:b+v-viewport[1])
 for i,action in enumerate(ir['menu']['actions']):
  binding=entry_bindings['menu'][i];alias=binding['asset_alias'];x,y=derived['command_icon_positions'][i];idx=layout('menu_'+action['id'],1,9,alias,[x,y,presentation['scene_nodes'][binding['node']]['margin_right']-presentation['scene_nodes'][binding['node']].get('margin_left',0),presentation['scene_nodes'][binding['node']]['margin_bottom']],depth=menu_depths[1],binding=i)
  menus.append([i+1,string(presentation['translations_en'][action['label_key']]),idx,1])
  apply_track('menu_'+action['id'],3,2,'actions.transitionIn',binding['node']+':margin_top',lambda v,b=y:b+v)
 cursor=ir['menu']['cursor_size'];center=derived['cursor_centers'][0];color=presentation['scene_nodes']['ActionMenuBox/Arrow/ActionCursor']['color']
 layout('cursor',2,10,None,[center[0]-cursor[0]/2,center[1]-cursor[1]/2,*cursor],color,visible=False,depth=menu_depths[0])
 apply_track('cursor',1,2,'actions.transitionIn','ActionMenuBox/Arrow:position',lambda v:[v[0]-cursor[0]/2,v[1]+presentation['scene_nodes']['ActionMenuBox']['margin_top']-cursor[1]/2])
 box=presentation['scene_nodes']['TargetNameBox'];bw=box['margin_right']-box['margin_left'];bh=-box['margin_top']
 layout('target_box',3,13,'box',[box['margin_left'],box['margin_top'],bw,bh],depth=menu_depths[2],margins=[box['patch_margin_'+s]for s in['left','top','right','bottom']])
 apply_track('target_box',3,2,'actions.transitionIn','TargetNameBox:rect_position:y')
 label=presentation['scene_nodes']['TargetNameBox/Label'];layout('target_text',4,14,'font',[box['margin_left']+label['margin_left'],label['margin_top']+box['margin_top'],label['margin_right']-label['margin_left'],label['margin_bottom']-label['margin_top']],text=presentation['translations_en'][ir['menu']['actions'][0]['label_key']],depth=menu_depths[3])
 apply_track('target_text',3,2,'actions.transitionIn','TargetNameBox:rect_position:y',lambda v:v+label['margin_top'])
 method_k=source_track('scene.transitionIn','.')
 event_ids={'_enemy_to_position':1,'_jump_to_battle':2,'_show_action_menu':3,'_show_enemy_sprites':4,'_remove_enemy_transitions':5}
 for time,method in zip(method_k['times'],method_k['values']):events.append([time,event_ids[method['method']],0,0])
 # _jump_to_battle creates nested SceneTreeTimers; the state machine preserves these waits, not a flattened timestamp.
 params={k:[0,0,0,0]for k in PARAMETERS}
 def param(k,v):params[k]=vec(v)
 for k,v in [('CanvasSize',viewport),('SceneDuration',entry['scene_duration']),('MaskDuration',entry['overlay_duration']),('MaskColor',[v/255 for v in entry['neutral_color_rgba8']]),('MaskGrid',entry['mask_grid']),('MenuDuration',entry['action_reveal_duration']),('EnemyMoveDuration',et['duration']),('PlayerJumpDuration',pt['jump_duration']),('PlayerJumpHeight',pt['jump_apex_offset']),('CursorDuration',ir['menu']['cursor_tween_seconds']),('CursorRepeatDelay',ir['menu']['cursor_repeat_delay_seconds']),('CursorRepeatInterval',ir['menu']['cursor_repeat_delay_seconds']),('PlateSize',ps),('MenuSpacing',presentation['scene_nodes']['ActionMenuBox/ActionIcons']['custom_constants/separation']),('EnemyShakeInterval',et['shake_frequency']),('EnemyShakeMagnitude',et['shake_range']),('FontMetrics',[assets['font_metrics']['ascent'],assets['font_metrics']['descent'],assets['font_metrics']['height']]),('PartyJumpStart',[pt['initial_jump_wait'],pt['per_member_wait'],pt['jump_up_seconds'],pt['jump_down_seconds']]),('PartyJumpTarget',[pt['jump_target_x'],pt['jump_target_y'],pt['jump_apex_offset'],pt['jump_height_threshold']]),('PartyJumpScale',[*pt['scale_target'],pt['scale_delay'],pt['scale_duration']]),('PartySquash',[*pt['squash_scale'],squash,0]),('PartyShow',[presentation['party_sprite']['show_tween_seconds'],presentation['party_sprite']['distance_to_shown']]),('EnemyTint',[*et['tint_to'][:3],et['tint_seconds']]),('BackdropColor',[0,0,0,1]),('DigitGrid',[digits_resource[5],digits_resource[3]/digits_resource[5],digits_resource[4]/digits_resource[6]])]:param(k,v)
 nudge=pt['center_nudge'];param('PartyNudge',[nudge['center_x'],nudge['distance_lt'],nudge['offset'],nudge['duration']]);param('PartyScreenOffset',pt['screen_offset'])
 cursor_steps=ir['menu']['cursor_scale_steps']
 param('CursorScale1',cursor_steps[0]['to']+cursor_steps[0]['from'])
 param('CursorScale2',cursor_steps[1]['to']+[cursor_steps[1]['duration'],0])
 param('CursorScale3',cursor_steps[2]['to']+[cursor_steps[2]['duration'],cursor_steps[2]['from'][1]])
 for i,step in enumerate(pt['arrival_quake_steps']):param('PartyQuake'+str(i+1),[step['from'],step['to'],step['duration'],step['ease']])
 import re
 source=(ROOT/'upstream/MOTHER-Encore/Nodes/Ui/Battle/Battle Transition.tscn').read_text()
 matches=re.findall(r'shader_param/OLDCOLOR = Color\( ([^)]*) \)',source)
 require(len(matches)==1,'Missing source transition old color');param('MaskOldColor',[float(x)for x in matches[0].split(',')])
 participants=[]
 for i,(data,name,kind)in enumerate([(dict(ir['party']['effective_stats'],level=ir['party']['level'],exp=ir['party']['exp'],cash=0),ir['party']['initial_save_data']['name'],1),(ir['enemy']['data'],presentation['translations_en'][ir['enemy']['data']['name']],2)]):
  participants.append([i+1,string(name),kind,0]+[data[k]for k in ['level','hp','maxhp','pp','maxpp','offense','defense','speed','iq','guts','exp','cash']])
 metadata=[battle['stable_id'],string(ir['enemy']['id']),ir['audio']['encounter_audio_id'],names['transition'],scene['player_instance_index'],battle['actor_instance_index'],int(entry['can_run'])|(int(ir['menu']['wrap_around'])<<1),0]
 backgrounds=[];version=ir.get('binary_version',1)
 require(version in (1,2),'Unknown battle binary version')
 for layer in ir['background']['layers']:
  p=layer['properties'];supported={'shader','texture','texture_stretch','opacity','screen_size','move','ping_pong_speed','oscillation_amplitude','oscillation_frequency','oscillation_speed','osc_amp_ping_pong','osc_trans_ping_pong','compression_amplitude','compression_frequency','compression_speed','comp_amp_ping_pong','comp_trans_ping_pong','interlaced_amplitude','interlaced_frequency','interlaced_speed','inter_amp_ping_pong','inter_trans_ping_pong','palette_shifting_speed','palette','palette_shifting','palette_anim_frame_count','barrel','effect','effect_scale','barrelxy','blending'}
  require(set(p)<=supported and p['shader']=='[DEFAULT]' and p['texture_stretch']=='STRETCH_TILE','Unknown background shader field/profile')
  zero_fields=['osc_amp_ping_pong','osc_trans_ping_pong','comp_amp_ping_pong','comp_trans_ping_pong','interlaced_amplitude','interlaced_frequency','interlaced_speed','inter_amp_ping_pong','inter_trans_ping_pong']
  require(all(p.get(k,[0,0])==[0,0] for k in zero_fields) and p.get('blending',0)==0,'Unimplemented background shader branch')
  if version==1:
   require(all(p.get(k,[0,0])==[0,0] for k in ['move','ping_pong_speed','compression_amplitude','compression_frequency','compression_speed']) and not p.get('palette_shifting',False),'Background requires extended schema')
  row=[names['background'],int(p['barrel'])|2,*ir['background']['native_layer_size'],p['opacity'],p['effect'],p['effect_scale'],*p['barrelxy'],*p['oscillation_amplitude'],*p['oscillation_frequency'],*p['oscillation_speed']]
  if version==2:
   palette=p.get('palette_shifting',False);row[1]|=4 if palette else 0
   fixed=ir['background'].get('compatibility_policy',{}).get('fixed_palette_row',0xffffffff)
   if fixed!=0xffffffff:
    require(palette and not p.get('palette_anim_frame_count',0),'Fixed row only for undefined source divisor');row[1]|=8
   row.extend([*p.get('move',[0,0]),*p.get('ping_pong_speed',[0,0]),*p.get('compression_amplitude',[0,0]),*p.get('compression_frequency',[0,0]),*p.get('compression_speed',[0,0]),names['background-palette'] if palette else 0xffffffff,p.get('palette_shifting_speed',0) if palette else 0,int(p.get('palette_anim_frame_count',0)) if palette else 0,fixed])
  backgrounds.append(row)
 glyphs=[[g['codepoint'],names['font'],g['u'],g['v'],g['width'],g['height'],g['advance'],g['offset_x'],g['offset_y']]for g in assets['glyphs']]
 adapter=json.loads((ROOT/'content/display-adapter.json').read_text())
 require(adapter['schema']==1 and adapter['reference_canvas']==viewport,'Display adapter source viewport mismatch')
 for layout_row in layouts:
  role=layout_row[2];anchor=adapter['role_anchors'][str(role)]
  require(len(anchor)==2 and all(0<=a<=1 for a in anchor),'Invalid display anchor')
  if role in adapter['expand_width_roles']:layout_row[6]|=4
  layout_row.append(anchor)
 return {'pool':bytes(pool),'resources':resources,'layouts':layouts,'tracks':tracks,'keys':keys,'parameters':[[i+1,*params[k]]for i,k in enumerate(PARAMETERS)],'participants':participants,'menus':menus,'metadata':[metadata],'backgrounds':backgrounds,'events':events,'glyphs':glyphs,'roles':roles,**({'version':version} if version!=1 else {})}

def encode(t,commit):
 sections=[t['pool'],b''.join(struct.pack('<7I32s',*r)for r in t['resources']),b''.join(struct.pack('<8I9f4I2f',*r[:8],*r[8],*r[9],r[10],*r[11],*r[12])for r in t['layouts']),b''.join(struct.pack('<6I',*r)for r in t['tracks']),b''.join(struct.pack('<6f',r[0],r[1],*r[2])for r in t['keys']),b''.join(struct.pack('<I4f',*r)for r in t['parameters']),b''.join(struct.pack('<4I12i',*r)for r in t['participants']),b''.join(struct.pack('<4I',*r)for r in t['menus']),b''.join(struct.pack('<8I',*r)for r in t['metadata']),b''.join(struct.pack('<2I23fIf2I' if t.get('version',1)==2 else '<2I13f',*r)for r in t['backgrounds']),b''.join(struct.pack('<f3I',*r)for r in t['events']),b''.join(struct.pack('<6I3f',*r)for r in t['glyphs'])]
 data=bytearray(256)
 strides=list(STRIDES);version=t.get('version',1);require(version in (1,2),'Unknown output version')
 if version==2:strides[9]=116
 for i,(block,stride)in enumerate(zip(sections,strides)):
  while len(data)%4:data.append(0)
  off=len(data)if block else 0;require(len(block)%stride==0,'Bad encoded stride');struct.pack_into('<HHIII',data,64+i*16,i+1,stride,off,len(block)//stride,len(block));data.extend(block)
 struct.pack_into('<8s6I20s12x',data,0,b'ENCBTL01',version,len(data),0,12,1,1,bytes.fromhex(commit))
 struct.pack_into('<I',data,16,zlib.crc32(data))
 return bytes(data)
def parse_sections(blob):
 require(256<=len(blob)<=1024*1024,'Battle binary size')
 magic,version,size,crc,n,caps,rules,commit=struct.unpack_from('<8s6I20s',blob)
 require(magic==b'ENCBTL01' and version in (1,2) and size==len(blob) and n==12 and caps==1 and rules==1 and not any(blob[52:64]),'Battle binary header')
 copy=bytearray(blob);struct.pack_into('<I',copy,16,0);require(zlib.crc32(copy)==crc,'Battle binary CRC')
 sections=[];end=256;strides=list(STRIDES)
 if version==2:strides[9]=116
 for i,stride in enumerate(strides):
  kind,got,offset,count,amount=struct.unpack_from('<HHIII',blob,64+i*16)
  require(kind==i+1 and got==stride and amount==count*stride,'Battle binary directory')
  if not count:require(offset==amount==0,'Battle empty section');sections.append(b'');continue
  require(offset%4==0 and offset>=end and offset+amount<=len(blob) and not any(blob[end:offset]),'Battle section span')
  sections.append(blob[offset:offset+amount]);end=offset+amount
 require(end==len(blob),'Battle trailing data')
 pool=sections[0];require(pool and pool[0]==0 and pool[-1]==0,'Battle pool bounds');pool.decode('utf-8')
 return sections

def stage_files(source,pack_path=Path('data/opening.encbattle')):
 pack_path=Path(pack_path);require(not pack_path.is_absolute() and '..' not in pack_path.parts,'Unsafe battle pack path')
 blob=(source/pack_path).read_bytes();sections=parse_sections(blob);pool=sections[0]
 files={pack_path:blob}
 for offset in range(0,len(sections[1]),60):
  row=struct.unpack_from('<7I32s',sections[1],offset);ref=row[1]
  require(ref<len(pool) and (ref==0 or pool[ref-1]==0),'Battle resource string offset')
  name=pool[ref:pool.index(0,ref)].decode('utf-8');path=Path(name)
  require(name and not path.is_absolute() and all(p not in('','..','.')for p in name.split('/')) and ':'not in name and '\\'not in name,'Battle resource path')
  resolved=(source/path).resolve();require(resolved.is_relative_to(source.resolve()),'Battle resource escaped root')
  data=resolved.read_bytes();require(hashlib.sha256(data).digest()==row[7],'Battle resource hash '+name);files[path]=data
 return files

def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['compile','verify'],nargs='?',default='compile');p.add_argument('--input',type=Path,default=ROOT/'content/native-battle.json');p.add_argument('--out',type=Path,default=ROOT/'romfs/data/opening.encbattle');a=p.parse_args()
 try:
  ir=json.loads(a.input.read_text());verify_sources(ir);assets=json.loads((ROOT/ir['presentation']['asset_receipt_path']).read_text());tables=lower(ir,assets,(ROOT/'romfs/data/opening.encroom').read_bytes());blob=encode(tables,ir['commit']);parse_sections(blob)
  if a.action=='compile':a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_bytes(blob);(ROOT/'build/battle-content-manifest.json').write_text(json.dumps({'schema':1,'ir_sha256':digest(a.input),'bytes':len(blob),'sha256':hashlib.sha256(blob).hexdigest(),'roles':tables['roles'],'sections':{k:len(v)for k,v in tables.items()if k not in ('roles','version')}},indent=2)+'\n')
  else:require(a.out.read_bytes()==blob,'Battle pack stale or changed')
  print('Battle content:',len(blob),'bytes; external data only')
  return 0
 except (ValueError,KeyError,TypeError,OSError,struct.error)as e:print('BATTLE CONTENT ERROR:',e,file=sys.stderr);return 1
if __name__=='__main__':raise SystemExit(main())
