"""Checked source selectors and stable bindings for the original House slice."""
import copy,csv,hashlib,io,json,math,re
from pathlib import Path
from tools.extract_battle_entry import ROOT,PIN,one,node,properties,animation,require
IR=Path(__file__).resolve().parents[1]/'content/house-source-bindings.json'
SOURCE_KEYS='house door_scene door_script fade_scene npc_scene npc_script player_scene dialogue_scene dialogue_script abstract_script global_data text_tools arrow_scene openable_scene openable_script area_scene fade_shader main_font reviewed_font default_animation new_game podunk_text cutscene_text reusable_text menu_text project scene_transition fade_script party_script party_object character_sprite_script cutscene_area_script flag_landmarks_script flag_landmarks_scene'.split()
NODE_KEYS='dialogue_box name_box clip_box hbox dialogue_label bullet name_label dialogue_cursor character_sprite shadow doors door_shape door_target npc_area npc_shape npc_view player_ray openable_timer openable_sprite area_shape doll_parent'.split()
def fields(value,keys,label):require(type(value)is dict and set(value)==set(keys),'Unknown/missing House '+label)
def finite(value):return type(value)in(int,float)and math.isfinite(value)
def uint(value,maximum=0xffffffff,zero=False):return type(value)is int and int(not zero)<=value<=maximum
def vector(value,size):return type(value)is list and len(value)==size and all(finite(v)for v in value)
def safe(value):return type(value)is str and value and ':'not in value and '\\'not in value and all(ord(c)>=32 and ord(c)!=127 for c in value)and all(p not in('','.','..')for p in value.split('/'))
def unique(pairs):
 result={}
 for key,value in pairs:require(key not in result,'Duplicate House binding field');result[key]=value
 return result
def read(path):return json.loads(Path(path).read_text(encoding='utf-8'),object_pairs_hook=unique)
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def clean(text):return re.sub(r'(?m)^(bbcode_text|text) = \"(.*?)\"(?=\n)',lambda m:m[1]+' = '+json.dumps(m[2]),text,flags=re.S)
def sn(text,path):return node(text.replace('Rect2(','Color('),path)
def clip_reference(text,path,name):
 bodies=[]
 for match in re.finditer(r'^\[node ([^\n]+)\]\n(.*?)(?=^\[|\Z)',text,re.M|re.S):
  attrs=dict(re.findall(r'(\w+)="([^"]*)"',match[1]));actual='.'if'parent'not in attrs else attrs['name']if attrs['parent']=='.'else attrs['parent']+'/'+attrs['name']
  if actual==path:bodies.append(match[2])
 require(len(bodies)==1,'Missing/ambiguous House animation player')
 return int(one(r'^(?:"anims/'+re.escape(name)+r'"|anims/'+re.escape(name)+r')\s*=\s*SubResource\( (\d+) \)',bodies[0],'House source animation reference')[1])

def load(root=ROOT,recipe=None):
 root=Path(root);b=copy.deepcopy(read(IR)if recipe is None else recipe)
 fields(b,'schema kind commit sources source_refs nodes same_scene_doors npcs presentation door openable npc text triggers phone pillow source_facts asset_roles patterns'.split(),'binding root')
 require(type(b['schema'])is int and b['schema']==1 and b['kind']=='encore.house-source-bindings'and b['commit']==PIN,'Unsupported House binding schema/pin')
 inventory=read(root/'compatibility/upstream-inventory.json');lock=read(root/'upstream.lock')
 require(inventory['commit']==lock['commit']==PIN and type(b['sources'])is dict and 32<=len(b['sources'])<=512,'House binding source coverage')
 for path,digest in b['sources'].items():require(safe(path)and path in inventory['files']and inventory['files'][path]['sha256']==digest and sha(root/'upstream/MOTHER-Encore'/path)==digest,'Changed House binding source '+str(path))
 fields(b['source_refs'],SOURCE_KEYS,'source selectors');fields(b['nodes'],NODE_KEYS,'node selectors')
 for value in b['source_refs'].values():require(value in b['sources'],'Unknown House source selector')
 for value in b['nodes'].values():require(safe(value),'Invalid House node selector')
 def source(key):return clean((root/'upstream/MOTHER-Encore'/b['source_refs'][key]).read_text(encoding='utf-8'))
 scene=source('house');dialogue=source('dialogue_scene');npc=source('npc_scene');ods=source('openable_scene')
 root_name=one(r'^\[node name="([^"]+)" type="Node2D"\]',scene,'House root')[1].replace("\\'","'");b['scene_root']=root_name
 for key in NODE_KEYS[:8]:sn(dialogue,b['nodes'][key])
 for key in ['character_sprite','shadow','npc_area','npc_shape','npc_view']:sn(npc,b['nodes'][key])
 for key in ['doors','doll_parent']:sn(scene,b['nodes'][key])
 for key in ['door_shape','door_target']:sn(source('door_scene'),b['nodes'][key])
 for key in ['openable_timer','openable_sprite']:sn(ods,b['nodes'][key])
 sn(source('player_scene'),b['nodes']['player_ray']);shape=sn(source('area_scene'),b['nodes']['area_shape'])['shape']['SubResource'];b['area_shape_id']=shape
 require(type(b['asset_roles'])is list and b['asset_roles']and len(set(b['asset_roles']))==len(b['asset_roles'])and all(safe(role)for role in b['asset_roles']),'Invalid House asset role coverage')
 assets=read(root/'content/house-assets.json');resource={r['role']:r for r in assets['resources']}
 require(set(resource)==set(b['asset_roles'])and assets['commit']==PIN,'House source asset role mismatch')
 fields(b['patterns'],['sprite','dialogue','voice','door_sound'],'source patterns')
 for key,argument in [('sprite','sprite'),('dialogue','dialogue'),('voice','sound'),('door_sound','sound')]:
  require(type(b['patterns'][key])is str and b['patterns'][key].count('{'+argument+'}')==1 and safe(b['patterns'][key].replace('{'+argument+'}','checked')),'Invalid House source path pattern')
 room=read(root/'content/native-opening.json');strings=room['strings'];sections=room['sections']
 def body(path):
  rows=[r['body_id']for r in sections['BodyRule']if strings[r['source_path_string']]==path]
  require(len(rows)==1,'Missing/ambiguous House source body '+path);return rows[0]
 def rows(value,keys,label,maximum):
  require(type(value)is list and 0<len(value)<=maximum,'House '+label+' count');ids=set();paths=set()
  for row in value:
   fields(row,keys,label);require(uint(row['id'])and row['id']not in ids and safe(row['source_path'])and row['source_path']not in paths,'Duplicate/invalid House '+label+' identity');ids.add(row['id']);paths.add(row['source_path']);sn(scene,row['source_path'])
 rows(b['same_scene_doors'],['id','source_path'],'warp bindings',32)
 require(len(b['same_scene_doors'])==8,'House reviewed warp coverage')
 for row in b['same_scene_doors']:
  door=sn(scene,row['source_path']);require(row['source_path'].startswith(b['nodes']['doors']+'/')and not door.get('targetScene')and not door.get('flag_set'),'Unsupported House warp source binding')
  sn(scene,row['source_path']+'/'+b['nodes']['door_target'])
 rows(b['npcs'],'id profile_id source_path resource shadow_resource body_id room_actor actor_stable_id actor_name'.split(),'NPC bindings',16)
 require(len(b['npcs'])==4 and len({r['profile_id']for r in b['npcs']})==4,'House reviewed NPC/profile coverage')
 for row in b['npcs']:
  require(uint(row['body_id'])and uint(row['profile_id'])and row['resource']in resource and row['shadow_resource']in set(resource)|{''}and uint(row['room_actor'],len(sections['ActorInstance'])-1,True)and uint(row['actor_stable_id'])and safe(row['actor_name']),'Unknown House NPC/profile/actor')
  instance=sn(scene,row['source_path']);require(resource[row['resource']]['source']==b['patterns']['sprite'].format(sprite=instance['sprite'])and body(row['source_path'])==row['body_id'],'House NPC sprite/body source mismatch')
  actor=sections['ActorInstance'][row['room_actor']]
  require(actor['stable_id']==row['actor_stable_id']and strings[actor['display_name_string']]==row['actor_name']==row['source_path'].split('/')[-1],'House preserved Room actor identity mismatch')
  require(bool(row['shadow_resource'])==(not instance.get('no_shadow',False)),'House NPC shadow source mismatch')
 fields(b['presentation'],'profile_order profile_tail ui_clips cursor_role cursor_animation npc_states directions name_sizing display_reference'.split(),'presentation')
 p=b['presentation'];require(type(p['profile_order'])is list and type(p['profile_tail'])is list and p['profile_order']+p['profile_tail']==[r['source_path']for r in b['npcs']]and p['cursor_role']in resource and p['npc_states']==['Idle','Talk']and p['directions']==['Down','Left','Right','Up'],'Unsupported House profile/state bindings')
 require(vector(p['name_sizing'],3)and p['name_sizing'][0]>=0 and 0<p['name_sizing'][1]<=120 and p['name_sizing'][2]==4,'Unsupported House name tween lowering')
 script=source('dialogue_script');padding=float(one(r'var new_size = _name_label.rect_size.x \+ ([0-9.]+)',script,'House name padding')[1]);duration=float(one(r'Vector2\(new_size, [0-9.]+\), ([0-9.]+)\)',script,'House name duration')[1])
 require(p['name_sizing'][:2]==[padding,duration]and '.set_trans(Tween.TRANS_QUART).set_ease(Tween.EASE_OUT)'in script,'House name sizing source mismatch')
 require(vector(p['display_reference'],4)and all(0<v<=8192 for v in p['display_reference'][:2])and all(0<=v<=1 for v in p['display_reference'][2:]),'Invalid House display adapter')
 dimensions=[int(one(r'^window/size/'+axis+r'=(\d+)$',source('project'),'House source viewport')[1])for axis in ['width','height']]
 require(p['display_reference'][:2]==dimensions,'House source viewport mismatch')
 require(type(p['ui_clips'])is list and len(p['ui_clips'])==4,'House UI clip coverage');seen=set()
 for row in p['ui_clips']:
  fields(row,['player','clip','role','resource'],'UI clip');require(safe(row['player'])and row['role']in {'DialogueOpen','DialogueClose','NameOpen','NameClose'}and row['role']not in seen and row['resource']in resource and row['clip']in {'Open','Close'},'Unknown/duplicate House UI animation binding')
  player=sn(dialogue,row['player']);require('anims/'+row['clip']in player,'Missing House UI source clip');seen.add(row['role'])
 arrow=source('arrow_scene');frames=sn(arrow,'.')['frames']['SubResource'];body_text=one(r'^\[sub_resource type="SpriteFrames" id='+str(frames)+r'\]\n(.*?)(?=^\[|\Z)',arrow,'House cursor frames',re.M|re.S)[1]
 animations=properties(body_text)['animations'];require(len(animations)==1 and animations[0]['name']==p['cursor_animation']and animations[0]['loop']and finite(animations[0]['speed'])and animations[0]['speed']>0,'Unsupported House cursor animation')
 cursor_frames=[];frame_size=None
 for ref in animations[0]['frames']:
  atlas=one(r'^\[sub_resource type="AtlasTexture" id='+str(ref['SubResource'])+r'\]\n(.*?)(?=^\[|\Z)',arrow,'House cursor atlas',re.M|re.S)[1];props=properties(atlas.replace('Rect2(','Color('));region=props['region'];size=region[2:]
  require(vector(region,4)and region[1]==0 and size[0]==size[1]>0 and region[0]%size[0]==0 and(frame_size is None or frame_size==size),'Unsupported House cursor atlas geometry')
  frame_size=size;cursor_frames.append(int(region[0]/size[0]));rid=props['atlas']['ExtResource']
  path=one(r'^\[ext_resource path="res://([^"]+)" type="Texture" id='+str(rid)+r'\]',arrow,'House cursor texture')[1];require(path==resource[p['cursor_role']]['source'],'House cursor texture role mismatch')
 b['cursor_frame_size']=frame_size;b['cursor_frames']=cursor_frames;b['cursor_fps']=animations[0]['speed']
 pitch=one(r'set_pitch_scale\(rand_range\(([0-9.]+), ([0-9.]+)\)\)',source('abstract_script'),'House voice pitch');b['voice_pitch']=[float(pitch[1]),float(pitch[2])]
 fields(b['door'],'destination_offset shape_id fade_player fade_in fade_out fade_callback color fade_value_track fade_callback_track fade_value_property fade_callback_node'.split(),'door adapter');d=b['door']
 require(vector(d['destination_offset'],2)and uint(d['shape_id'])and safe(d['fade_player'])and d['fade_in']=='Fade In'and d['fade_out']=='Fade Out'and re.fullmatch('[A-Za-z_][A-Za-z_0-9]*',d['fade_callback'])and vector(d['color'],4)and all(0<=v<=1 for v in d['color']),'Invalid House door/fade binding')
 destination=one(r'_new_pos.global_position - Vector2\(([0-9.]+), ([0-9.]+)\)',source('door_script'),'House source destination');require(d['destination_offset']==[float(destination[1]),float(destination[2])],'House warp destination source mismatch')
 require(sn(source('door_scene'),b['nodes']['door_shape'])['shape']['SubResource']==d['shape_id'],'House door collision shape source mismatch')
 require(type(d['fade_value_property'])is str and type(d['fade_callback_node'])is str,'Invalid House fade property/node')
 fade=source('fade_scene')
 for name in ['fade_in','fade_out']:b[name+'_resource']=clip_reference(fade,d['fade_player'],d[name])
 clip=animation(fade,b['fade_out_resource'],b['source_refs']['fade_scene'],d['fade_out'])
 require(uint(d['fade_callback_track'],len(clip['tracks'])-1,True)and uint(d['fade_value_track'],len(clip['tracks'])-1,True),'Invalid House fade track selectors')
 require(clip['tracks'][d['fade_callback_track']]['type']=='method'and clip['tracks'][d['fade_callback_track']]['keys']['values'][0]['method']==d['fade_callback']and clip['tracks'][d['fade_callback_track']]['path']==d['fade_callback_node']and clip['tracks'][d['fade_value_track']]['type']=='value'and clip['tracks'][d['fade_value_track']]['path']==d['fade_value_property'],'House fade source callback/value binding mismatch')
 fields(b['openable'],'rows shapes shape_nodes clips track_components base_y blocked_dialogue_id ram body_nodes expected_masks'.split(),'openable door bindings');od=b['openable']
 rows(od['rows'],'id source_path player_body_id nonplayer_body_id resource'.split(),'openable identities',16);require(len(od['rows'])==4,'House openable source coverage')
 fields(od['body_nodes'],['player','nonplayer'],'openable body nodes');fields(od['shapes'],['trigger','interact','collision','nonplayer'],'openable shapes');fields(od['shape_nodes'],['trigger','interact','collision','nonplayer'],'openable shape nodes')
 for key,value in od['shapes'].items():require(uint(value)and safe(od['shape_nodes'][key])and sn(ods,od['shape_nodes'][key])['shape']['SubResource']==value,'House openable source shape mismatch')
 fields(od['clips'],['Action','Normal'],'openable clips');require(all(uint(v)for v in od['clips'].values()),'Invalid House openable clip reference')
 require(type(od['track_components'])is dict and all(type(v)is int for v in od['track_components'].values()),'Invalid House openable component type')
 require(od['track_components']=={b['nodes']['openable_sprite']+':visible':1,od['shape_nodes']['collision']+':disabled':2,od['shape_nodes']['nonplayer']+':disabled':4},'House collision/visibility component schema')
 fields(od['expected_masks'],['Action','Normal'],'openable expected source masks')
 require(all(type(v)is list and len(v)==2 and all(uint(n,7,True)for n in v)for v in od['expected_masks'].values()),'Invalid House source mask type')
 for name,rid in od['clips'].items():
  clip=animation(ods,rid,b['source_refs']['openable_scene'],name)
  require(all(t['path']in od['track_components']and t['type']=='value'and t['keys']['times']==[0.]and len(t['keys']['values'])==1 and type(t['keys']['values'][0])is bool for t in clip['tracks']),'Unknown House openable animation track')
  mask=values=0
  for track in clip['tracks']:
   component=od['track_components'][track['path']];mask|=component
   if track['keys']['values'][0]:values|=component
  require(od['expected_masks'][name]==[mask,values],'House openable source masks mismatch')
 for row in od['rows']:
  instance=sn(scene,row['source_path']);require(uint(row['player_body_id'])and uint(row['nonplayer_body_id'])and row['resource']in resource and row['player_body_id']==body(row['source_path']+'/'+od['body_nodes']['player'])and row['nonplayer_body_id']==body(row['source_path']+'/'+od['body_nodes']['nonplayer']),'House openable stable body identity mismatch')
  rid=instance['sprite']['ExtResource'];path=one(r'^\[ext_resource path="res://([^"]+)" type="Texture" id='+str(rid)+r'\]',scene,'House door texture source')[1];require(path==resource[row['resource']]['source'],'House door sprite role source mismatch')
 require(finite(od['base_y'])and uint(od['blocked_dialogue_id']),'Invalid House openable geometry/identity')
 offset=float(one(r'door_offset.y \+ ([0-9.]+)',source('openable_script'),'House openable source offset')[1]);require(od['base_y']==offset,'House openable source offset mismatch')
 fields(od['ram'],['sound','direction','required_y','strength','duration'],'ram binding');ram=od['ram']
 shake=one(r'shake_camera\(([0-9.]+), ([0-9.]+), Vector2\(([0-9.]+),([0-9.]+)\)\)',source('openable_script'),'House ram source')
 require(finite(ram['strength'])and finite(ram['duration'])and finite(ram['required_y'])and vector(ram['direction'],2)and ram['direction']==[float(shake[3]),float(shake[4])]and ram['strength']==float(shake[1])and ram['duration']==float(shake[2])and ram['required_y']==float(one(r'get_direction\(\).y == (-?[0-9.]+)',source('openable_script'),'House ram condition')[1])and ram['sound']in b['sources'],'House ram source rule mismatch')
 fields(b['npc'],'shape_id default_direction primary mimmie doll melody_program mimmie_seen_dialogue initial_party primary_phrase primary_segments'.split(),'NPC source bindings');n=b['npc'];require(uint(n['shape_id'])and sn(npc,b['nodes']['npc_shape'])['shape']['SubResource']==n['shape_id']and vector(n['default_direction'],2),'House NPC interact shape/direction')
 direction=one(r'initial_dir = Vector2\((-?[0-9.]+),(-?[0-9.]+)\)',source('npc_script'),'House NPC direction');require(n['default_direction']==[float(direction[1]),float(direction[2])],'House source default direction mismatch')
 for key in ['primary','mimmie','doll']:require(n[key]in {r['source_path']for r in b['npcs']},'Missing House NPC semantic binding')
 import yaml
 require(yaml.safe_load(source('new_game'))['party']==[n['initial_party']],'House singleton party source mismatch')
 require(type(n['primary_phrase'])is str and uint(n['primary_segments'],64),'Invalid House primary phrase binding')
 primary=sn(scene,n['primary']);path=b['patterns']['dialogue'].format(dialogue=primary['dialog']);doc=yaml.safe_load((root/'upstream/MOTHER-Encore'/path).read_text(encoding='utf-8'))
 require(n['primary_phrase']in doc and set(doc[n['primary_phrase']])=={'name','sound','text'},'House primary literal source phrase mismatch')
 table={r['key']:r['en']for r in csv.DictReader(io.StringIO(source('podunk_text')))}
 require(table[doc[n['primary_phrase']]['text']].count('[WAIT@]')+1==n['primary_segments'],'House primary source segment count mismatch')
 fields(b['text'],'rows post_win melody guard default after repeat default_parts'.split(),'text bindings');require(type(b['text']['rows'])is list and len(b['text']['rows'])==10,'House literal text scope')
 ids={od['blocked_dialogue_id']}
 for row in b['text']['rows']:
  fields(row,['id','path','label','parts','table'],'literal text identity');require(uint(row['id'])and row['id']not in ids and row['path']in b['sources']and type(row['label'])is str and uint(row['parts'],64)and row['table']in b['source_refs'],'Unknown/duplicate House literal text binding');ids.add(row['id'])
  doc=yaml.safe_load((root/'upstream/MOTHER-Encore'/row['path']).read_text(encoding='utf-8'));require(row['label']in doc and 'text'in doc[row['label']],'Missing House literal source phrase')
  translations={r['key']:r['en']for r in csv.DictReader(io.StringIO(source(row['table'])))};text=translations[doc[row['label']]['text']];require(text.startswith('[@]')and len(text[3:].split('[WAIT@]'))==row['parts'],'House literal source segment mismatch')
 for key,path in b['text'].items():
  if key not in('rows','default_parts'):require(path in b['sources'],'Unknown House dialogue source selector')
 require(uint(b['text']['default_parts'],64),'Invalid House default literal segment count')
 default=yaml.safe_load((root/'upstream/MOTHER-Encore'/b['text']['default']).read_text(encoding='utf-8'))
 require(table[default['0']['text']].count('[WAIT@]')+1==b['text']['default_parts'],'House default source segment mismatch')
 rows(b['triggers'],['id','source_path','disposition'],'trigger bindings',16);require(len(b['triggers'])==4,'House initial trigger source coverage')
 for row in b['triggers']:require(type(row['disposition'])is int and row['disposition']in(1,2)and 'dialog'in sn(scene,row['source_path']),'Unsupported House trigger source')
 fields(b['phone'],['npc_source','actor','text_ids','triggers'],'Phone House links');fields(b['pillow'],['npc_source','tutorial','opened','text_ids','triggers'],'Pillow House links')
 for key in ['phone','pillow']:
  require(b[key]['npc_source']in {r['source_path']for r in b['npcs']}and type(b[key]['text_ids'])is dict and b[key]['text_ids'],'Missing House appended text/NPC binding')
  for identity,ident in b[key]['text_ids'].items():require(type(identity)is str and identity and uint(ident)and ident not in ids,'Duplicate/invalid House appended text identity');ids.add(ident)
 from tools import link_phone_content as phone,link_pillow_content as pillow
 from tools.extract_battle_entry import Extractor
 stage,_=phone.load_stage(root);dad,_=phone.load_dad(root);expected={**phone.text_ids(stage),**phone.dad_text_ids(stage,dad)}
 require(b['phone']['text_ids']==expected and b['phone']['actor']==next(r['room_actor']for r in b['npcs']if r['source_path']==b['phone']['npc_source']),'House/Room Phone stable identity mismatch')
 source_ex=Extractor(root);pillow_rows,_=pillow.texts(source_ex,pillow.load_documents(source_ex));require(b['pillow']['text_ids']=={r['identity']:r['id']for r in pillow_rows},'House/Room Pillow stable identity mismatch')
 programs={strings[r['source_path_string']]for r in sections['Program']}
 require(b['pillow']['tutorial']in programs and n['melody_program']in programs,'Missing House source programme link')
 require(b['pillow']['opened']in {row[1]for row in sn(scene,b['pillow']['npc_source']).get('_all_dialog',[])}and b['patterns']['dialogue'].format(dialogue=b['pillow']['opened'])in b['sources'],'Missing House Pillow literal override source')
 require(type(b['phone']['triggers'])is list and len(b['phone']['triggers'])==2 and type(b['pillow']['triggers'])is list and len(b['pillow']['triggers'])==2,'Invalid House append trigger scope')
 require(len({row['source_path']for row in b['phone']['triggers']})==2 and len(set(b['pillow']['triggers']))==2,'Duplicate House append trigger binding')
 for row in b['phone']['triggers']:
  fields(row,['source_path','program'],'Phone trigger');require(safe(row['source_path'])and row['program']in programs and sn(scene,row['source_path'])['dialog']==row['program'],'House Phone source trigger programme mismatch')
 for path in b['pillow']['triggers']:require(safe(path)and sn(scene,path)['dialog']in programs,'House Pillow trigger programme mismatch')
 require(type(b['source_facts'])is dict and set(b['source_facts'])=={b['source_refs'][key]for key in ['door_script','openable_script','npc_script','dialogue_script']},'House semantic source coverage')
 for path,facts in b['source_facts'].items():
  require(type(facts)is list and facts and all(type(f)is str and f for f in facts),'Invalid House semantic facts');text=(root/'upstream/MOTHER-Encore'/path).read_text(encoding='utf-8');require(all(f in text for f in facts),'Changed House source semantic fact')
 return b

def check_house(ir,presentation,root=ROOT):
 from tools import extract_house,house_assets
 require(Path(root)==house_assets.ROOT,'House extractor must match compiler checkout')
 require(extract_house.build(root)==ir,'House IR differs from checked source bindings')
 if presentation is not None:
  receipt=read(root/'romfs/house-preview/source.json')
  expected=house_assets.export_presentation(root/'upstream/MOTHER-Encore',receipt['resources'],write=False)
  require(expected==presentation,'House presentation differs from checked source bindings')
