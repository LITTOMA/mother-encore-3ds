#!/usr/bin/env python3
"""Compile bounded, external house interaction content as checked ENCHSE01."""
import argparse,hashlib,json,math,re,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT));sys.path.insert(0,str(ROOT/'tools'))
PIN='7d9246600fffe518408f5830d4848635019005a3'
SECTION_NAMES=['Strings','Doors','Npcs','Segments','Tokens','Interaction','Boundaries','Resources','Clips','Keys','Parameters','Overrides','Profiles','Dialogues','OpenableDoors','StoryTriggers','StoryConditions']
FORMATS=[None,'<4I8f6d4f','<8I11fd5I','<6I','<2I','<3f3d4I2d','<3I4f','<7I32s','<5Id3I','<df2fI','<I4f','<5I','<2I4f2I','<4I','<21I21f5d','<3I4f4I','<3I']
STRIDES=[1]+[struct.calcsize(x)for x in FORMATS[1:]];HEADER=64+16*len(STRIDES)
PARAMETERS=['DialogueRect','DialogueMargins','DialogueClip','DialogueText','DialogueBullet','NameRect','NameMargins','NameLabel','NameSizing','CursorGeometry','CursorRotation','TextTiming','TextTagSpeeds','VoicePitch','DisplayReference','FontMetrics','FadeCuts','FadeShader','NpcInteractionReturn']
CLIP_ROLES=['NpcIdleDown','NpcIdleLeft','NpcIdleRight','NpcIdleUp','NpcTalkDown','NpcTalkLeft','NpcTalkRight','NpcTalkUp','DialogueOpen','DialogueClose','NameOpen','NameClose','Cursor']
DOOR_FIELDS=['id','source_path','start_sound','end_sound','center','extents','destination','direction','fade_in_length','fade_in_opaque','fade_out_length','fade_out_mostly','fade_in_speed','fade_out_speed','color']
NPC_FIELDS=['id','source_path','body_id','primary_resource','shadow_resource','first_segment','segment_count','seen_key','position','interact_center','interact_extents','default_direction','view_center','view_radius','return_delay','flags','profile','dialogue_path','room_actor_index','program_index']
OPENABLE_FIELDS=['id','source_path','player_body_id','nonplayer_body_id','sprite_resource','flag','key','activates_flag','deactivates_flag','blocked_dialogue','locked_dialogue','opened_dialogue','start_sound','end_sound','ram_sound','policy','normal_mask','normal_values','action_mask','action_values','timer_flags','position','trigger_center','trigger_extents','interact_center','interact_extents','collision_center','collision_extents','sprite_position','sprite_offset','ram_direction','ram_required_y','close_delay','action_length','normal_length','ram_strength','ram_duration']
class ContentError(ValueError):pass
def require(v,msg):
 if not v:raise ContentError(msg)
def fields(obj,names,label):require(isinstance(obj,dict)and set(obj)==set(names),'Unknown/missing '+label+' fields')
def digest(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def finite(v):return isinstance(v,(int,float))and not isinstance(v,bool)and math.isfinite(v)
def vector(v,n):require(isinstance(v,list)and len(v)==n and all(finite(x)for x in v),'Invalid vector');return v
def safe_path(s):return isinstance(s,str)and bool(s)and not s.startswith('/')and ':'not in s and '\\'not in s and all(x not in ('','..','.')for x in s.split('/'))
def verify_sources(ir,presentation=None,root=ROOT):
 require(ir['schema']in(4,5,6,7) and ir['kind']=='encore.native-house.source-ir'and ir['commit']==PIN,'Unreviewed house schema/pin')
 report=json.loads((root/'reports/house-data/source-review.json').read_text());require(report['commit']==PIN and report['sources']==ir['sources']and report['dependencies']==ir['dependencies'],'House review mismatch')
 inventory=json.loads((root/'compatibility/upstream-inventory.json').read_text());require(inventory['commit']==PIN,'House inventory pin')
 sources=dict(ir['sources'])
 if presentation is not None:
  require(presentation['commit']==PIN,'Presentation pin');sources.update(presentation['sources'])
 for path,sha in sources.items():require(safe_path(path)and inventory['files'][path]['sha256']==sha and digest(root/'upstream/MOTHER-Encore'/path)==sha,'Changed source '+path)
 require(set(ir['dependencies'])=={'content/native-opening.json'},'Unknown house dependency')
 for path,sha in ir['dependencies'].items():require(digest(root/path)==sha,'Changed house dependency '+path)
 from tools.house_source_bindings import check_house
 check_house(ir,presentation,root)

def lower(ir,presentation=None,root=ROOT,verify_assets=True):
 fields(ir,['schema','kind','commit','scope','sources','dependencies','fade_parameters','npc_parameters','doors','npcs','segments','interaction','boundaries','overrides','dialogues','openable_doors','story_triggers','story_conditions'],'house IR');require(ir['schema']in(4,5,6,7) and ir['commit']==PIN,'Unreviewed house IR')
 room=json.loads((root/'content/native-opening.json').read_text());require(room['upstream_commit']==ir['commit']and room['rules']==room['capabilities']and room['rules']in(4,5,6,7),'House room rules/pin')
 actor_count=len(room['sections']['ActorInstance']);program_count=len(room['sections']['Program']);dialogue_ids={d['id']for d in ir['dialogues']}
 for n in ir['npcs']:require(n.get('room_actor_index')==0xffffffff or isinstance(n.get('room_actor_index'),int)and 0<=n['room_actor_index']<actor_count,'House room actor reference')
 for n in ir['npcs']:
  require(n.get('program_index')==0xffffffff or isinstance(n.get('program_index'),int)and 0<=n['program_index']<program_count,'House NPC program reference')
  if n['program_index']!=0xffffffff:require(n['dialogue_path']=='Data/Dialogue/'+room['strings'][room['sections']['Program'][n['program_index']]['source_path_string']]+'.yaml','House NPC program path mismatch')
 for o in ir['overrides']:
  require(o.get('dialogue_index')==0xffffffff or isinstance(o.get('dialogue_index'),int)and 0<=o['dialogue_index']<len(ir['dialogues']),'House override dialogue reference')
  if o['dialogue_index']!=0xffffffff:require(ir['dialogues'][o['dialogue_index']]['source_path']=='Data/Dialogue/'+o['dialogue']+'.yaml','House override dialogue path mismatch')
 for d in ir['story_triggers']:require(d.get('disposition')!=2 or isinstance(d.get('program_index'),int)and 0<=d['program_index']<program_count,'House room program reference')
 for c in room['sections']['Command']:
  if c['opcode']==32:require(c['target_index']in dialogue_ids,'Room stable dialogue reference missing from House')
 if presentation is None:presentation=json.loads((root/'content/native-house-presentation.json').read_text())
 fields(presentation,['schema','commit','sources','resources','clips','parameters','profiles'],'house presentation');require(presentation['schema']==2 and presentation['commit']==PIN,'Presentation schema')
 pool=bytearray(b'\0');strings={'':0}
 def string(s):
  require(isinstance(s,str)and '\0'not in s and len(s.encode())<=4096,'Invalid string')
  if s not in strings:strings[s]=len(pool);pool.extend(s.encode()+b'\0')
  return strings[s]
 t={name:[]for name in SECTION_NAMES};resources={}
 for r in presentation['resources']:
  fields(r,['id','role','path','kind','width','height','columns','rows','sha256'],'resource');require(r['role']not in resources,'Duplicate resource role');resources[r['role']]=len(t['Resources']);require(safe_path(r['path']),'Resource path');sha=bytes.fromhex(r['sha256']);require(len(sha)==32,'Resource SHA')
  if verify_assets:require(digest(root/'romfs'/r['path'])==r['sha256'],'Changed resource '+r['path'])
  t['Resources'].append([r['id'],string(r['path']),r['kind'],r['width'],r['height'],r['columns'],r['rows'],sha])
 for d in ir['doors']:
  fields(d,DOOR_FIELDS,'door');t['Doors'].append([d['id'],string(d['source_path']),string(d['start_sound']),string(d['end_sound']),*vector(d['center'],2),*vector(d['extents'],2),*vector(d['destination'],2),*vector(d['direction'],2),*[d[x]for x in DOOR_FIELDS[8:14]],*vector(d['color'],4)])
 profiles={}
 for q in presentation['profiles']:
  fields(q,['id','role','resource','sprite_offset','shadow_offset','directions','flags'],'profile');require(q['role']not in profiles,'Duplicate profile role');profiles[q['role']]=len(t['Profiles']);t['Profiles'].append([q['id'],resources[q['resource']],*vector(q['sprite_offset'],2),*vector(q['shadow_offset'],2),q['directions'],q['flags']])
 for n in ir['npcs']:
  fields(n,NPC_FIELDS,'npc');t['Npcs'].append([n['id'],string(n['source_path']),n['body_id'],resources[n['primary_resource']],resources[n['shadow_resource']]if n['shadow_resource']is not None else 0xffffffff,n['first_segment'],n['segment_count'],string(n['seen_key']),*[v for key in NPC_FIELDS[8:13]for v in vector(n[key],2)],n['view_radius'],n['return_delay'],n['flags'],profiles[n['profile']],string(n['dialogue_path']),n['room_actor_index'],n['program_index']])
 for s in ir['segments']:
  fields(s,['id','speaker','voice','tokens','flags'],'segment');first=len(t['Tokens'])
  for tok in s['tokens']:fields(tok,['kind','text'],'token');t['Tokens'].append([tok['kind'],string(tok['text'])])
  t['Segments'].append([s['id'],string(s['speaker']),string(s['voice']),first,len(s['tokens']),s['flags']])
 q=ir['interaction'];fields(q,['ray_origin','ray_length','text_seconds','accept_multiplier','cancel_multiplier','collision_mask','bullet_string','word_separator','max_player_name_length','voice_pitch_min','voice_pitch_max'],'interaction');t['Interaction']=[[*vector(q['ray_origin'],2),q['ray_length'],q['text_seconds'],q['accept_multiplier'],q['cancel_multiplier'],q['collision_mask'],string(q['bullet_string']),string(q['word_separator']),q['max_player_name_length'],q['voice_pitch_min'],q['voice_pitch_max']]]
 for b in ir['boundaries']:fields(b,['id','source_path','kind','center','extents'],'boundary');t['Boundaries'].append([b['id'],string(b['source_path']),b['kind'],*vector(b['center'],2),*vector(b['extents'],2)])
 for c in presentation['clips']:
  fields(c,['id','role','resource','duration','loop','interpolation','keys','profile'],'clip');require(c['role']in CLIP_ROLES,'Unknown clip role');first=len(t['Keys'])
  for k in c['keys']:fields(k,['time','ease','position','frame'],'key');t['Keys'].append([k['time'],k['ease'],*vector(k['position'],2),k['frame']])
  t['Clips'].append([c['id'],CLIP_ROLES.index(c['role'])+1,resources[c['resource']]if c['resource']is not None else 0xffffffff,first,len(c['keys']),c['duration'],int(c['loop']),c['interpolation'],profiles[c['profile']]if c['profile']is not None else 0xffffffff])
 fields(presentation['parameters'],PARAMETERS[:16],'presentation parameters');fields(ir['fade_parameters'],PARAMETERS[16:18],'fade parameters');fields(ir['npc_parameters'],PARAMETERS[18:],'NPC parameters');parameters={**presentation['parameters'],**ir['fade_parameters'],**ir['npc_parameters']};t['Parameters']=[[i+1,*vector(parameters[name],4)]for i,name in enumerate(PARAMETERS)]
 for o in ir['overrides']:fields(o,['npc','flag','dialogue','dialogue_index','seen_key'],'override');t['Overrides'].append([o['npc'],string(o['flag']),string(o['dialogue']),o['dialogue_index'],string(o['seen_key'])])
 for d in ir['dialogues']:
  fields(d,['id','source_path','first_segment','segment_count'],'dialogue');t['Dialogues'].append([d['id'],string(d['source_path']),d['first_segment'],d['segment_count']])
 for d in ir['openable_doors']:
  fields(d,OPENABLE_FIELDS,'openable door');row=[]
  for name in OPENABLE_FIELDS[:21]:
   row.append(resources[d[name]]if name=='sprite_resource'else string(d[name])if name in ['source_path','flag','key','activates_flag','deactivates_flag','locked_dialogue','opened_dialogue','start_sound','end_sound','ram_sound']else d[name])
  row.extend(v for name in OPENABLE_FIELDS[21:31]for v in vector(d[name],2));row.extend(d[name]for name in OPENABLE_FIELDS[31:]);t['OpenableDoors'].append(row)
 for d in ir['story_triggers']:
  fields(d,['id','source_path','dialogue','center','extents','first_condition','condition_count','disposition','program_index'],'story trigger');t['StoryTriggers'].append([d['id'],string(d['source_path']),string(d['dialogue']),*vector(d['center'],2),*vector(d['extents'],2),d['first_condition'],d['condition_count'],d['disposition'],d['program_index']])
 for d in ir['story_conditions']:
  fields(d,['kind','flag','value'],'story condition');t['StoryConditions'].append([d['kind'],string(d['flag']),d['value']])
 t['Strings']=bytes(pool);return t

def encode(t,commit=PIN,version=4):
 require(version in(4,5,6,7),'House version')
 data=bytearray(HEADER)
 for i,name in enumerate(SECTION_NAMES):
  block=t[name]if i==0 else b''.join(struct.pack(FORMATS[i],*r)for r in t[name])
  if block:
   while len(data)%4:data.append(0)
  off=len(data)if block else 0;struct.pack_into('<HHIII',data,64+i*16,i+1,STRIDES[i],off,len(block)//STRIDES[i],len(block));data.extend(block)
 struct.pack_into('<8s6I20s12x',data,0,b'ENCHSE01',version,len(data),0,len(STRIDES),version,version,bytes.fromhex(commit));struct.pack_into('<I',data,16,zlib.crc32(data));return bytes(data)
def parse_pack(blob):
 require(HEADER<=len(blob)<=1024*1024,'House size');magic,version,size,crc,n,caps,rules,commit=struct.unpack_from('<8s6I20s',blob)
 require(magic==b'ENCHSE01'and version==caps==rules and version in(4,5,6,7) and size==len(blob)and n==len(STRIDES)and not any(blob[52:64]),'House header');copy=bytearray(blob);struct.pack_into('<I',copy,16,0);require(zlib.crc32(copy)==crc,'House CRC');t={};end=HEADER
 for i,name in enumerate(SECTION_NAMES):
  kind,stride,off,count,amount=struct.unpack_from('<HHIII',blob,64+i*16);require(kind==i+1 and stride==STRIDES[i]and amount==count*stride,'House directory')
  if count:require(off%4==0 and off>=end and off+amount<=len(blob)and not any(blob[end:off]),'House span');block=blob[off:off+amount];end=off+amount
  else:require(off==amount==0,'House empty section');block=b''
  t[name]=block if i==0 else list(struct.iter_unpack(FORMATS[i],block))
 require(end==len(blob),'House trailing bytes');validate(t,version);return t

def validate(t,version=4):
 require(version in(4,5,6,7),'House validation version')
 p=t['Strings'];require(p and len(p)<=65536 and p[0]==p[-1]==0,'House strings');p.decode('utf-8')
 def string(o):require(isinstance(o,int)and 0<=o<len(p)and(o==0 or p[o-1]==0),'House string offset');end=p.find(b'\0',o);require(0<=end-o<=4096,'House string length');return p[o:end].decode()
 limits={'Doors':(1,32),'Npcs':(1,16),'Segments':(1,256 if version>=7 else 128 if version>=6 else 64),'Tokens':(1,256),'Interaction':(1,1),'Boundaries':(0,64),'Resources':(1,64),'Clips':(5,256),'Keys':(1,2048),'Parameters':(len(PARAMETERS),len(PARAMETERS)),'Overrides':(0,128),'Profiles':(1,16),'Dialogues':(1,128 if version>=7 else 64),'OpenableDoors':(1,32),'StoryTriggers':(1,64),'StoryConditions':(1,128)}
 for k,(lo,hi)in limits.items():require(lo<=len(t[k])<=hi,'House '+k+' capacity')
 for k in ['Doors','Npcs','Segments','Boundaries','Resources','Clips','Profiles','Dialogues','OpenableDoors','StoryTriggers']:
  ids=[x[0]for x in t[k]];require(all(x>0 for x in ids)and len(ids)==len(set(ids)),'House IDs')
 for d in t['Doors']:
  require(safe_path(string(d[1]))and all(not string(x)or safe_path(string(x))for x in d[2:4]),'House door path');require(all(finite(x)for x in d[4:])and all(0<x<=10000 for x in d[6:8])and d[10]*d[10]+d[11]*d[11]==1,'House door geometry');require(0<d[13]<=d[12]<=120 and 0<d[15]<=d[14]<=120 and 0<d[16]<=100 and 0<d[17]<=100 and all(0<=x<=1 for x in d[18:]),'House fade')
 actor_maps=[n[23]for n in t['Npcs']if n[23]!=0xffffffff];require(len(actor_maps)==len(set(actor_maps)),'House duplicate actor binding')
 owned_segments=set();owned_tokens=set()
 for n in t['Npcs']:
  require(safe_path(string(n[1]))and n[2]>0 and n[3]<len(t['Resources'])and (n[4]==0xffffffff or n[4]<len(t['Resources']))and n[6]>=0 and n[5]+n[6]<=len(t['Segments'])and bool(string(n[7]))==bool(n[6]or n[24]!=0xffffffff)and (n[6]>0 or n[5]==0)and n[21]<len(t['Profiles'])and safe_path(string(n[22]))and t['Profiles'][n[21]][1]==n[3]and bool(t['Profiles'][n[21]][7]&2)==(n[4]!=0xffffffff)and all(finite(x)for x in n[8:20])and all(x>0 for x in n[12:14])and n[14]*n[14]+n[15]*n[15]==1 and n[18]>0 and 0<n[19]<=120 and n[20]&~15==0 and (n[23]==0xffffffff or 0<=n[23]<64)and(n[24]==0xffffffff or 0<=n[24]<1024)and(n[24]==0xffffffff or n[6]==0),'House NPC')
  for j in range(n[5],n[5]+n[6]):require(j not in owned_segments,'House segment ownership');owned_segments.add(j);require(t['Segments'][j][5]==(5 if j==n[5]+n[6]-1 else 3),'House segment termination')
 for d in t['Dialogues']:
  require(safe_path(string(d[1]))and d[3]>0 and d[2]+d[3]<=len(t['Segments']),'House dialogue')
  for j in range(d[2],d[2]+d[3]):require(j not in owned_segments,'House dialogue ownership');owned_segments.add(j);require(t['Segments'][j][5]==(5 if j==d[2]+d[3]-1 else 3),'House dialogue termination')
 for s in t['Segments']:
  require(isinstance(string(s[1]),str)and (not string(s[2])or safe_path(string(s[2])))and s[4]>0 and s[3]+s[4]<=len(t['Tokens'])and s[5]in[3,5],'House segment')
  for j in range(s[3],s[3]+s[4]):require(j not in owned_tokens,'House token ownership');owned_tokens.add(j)
 require(len(owned_segments)==len(t['Segments'])and len(owned_tokens)==len(t['Tokens']),'House orphan text')
 for tok in t['Tokens']:
  require(tok[0]in(range(1,10)if version>=6 else range(1,8)if version>=5 else[1,2]),'House token opcode')
  value=string(tok[1])
  if tok[0]==1:require(bool(value),'House empty literal')
  elif tok[0]==3:require(len(value)==6 and all(c in'0123456789abcdefABCDEF'for c in value),'House hint color')
  elif tok[0]==8:require(len(value)<=8 and re.fullmatch(r'[0-9]+(?:\.[0-9]+)?',value)is not None and finite(float(value))and 0<float(value)<=3600,'House source delay')
  else:require(tok[1]==0,'House unused token payload')
 q=t['Interaction'][0];require(all(finite(x)for x in q[:6])and 0<q[2]<=1024 and all(0<x<=10000 for x in q[3:6])and q[6]>0 and bool(string(q[7]))and bool(string(q[8]))and 0<q[9]<=128 and all(finite(x)for x in q[10:])and 0<q[10]<=q[11]<=10,'House interaction')
 for b in t['Boundaries']:require(safe_path(string(b[1]))and b[2]in[1,2]and all(finite(x)for x in b[3:])and all(x>0 for x in b[5:]),'House boundary')
 for r in t['Resources']:require(safe_path(string(r[1]))and r[2]in[1,2,3]and all(0<x<=8192 for x in r[3:5])and all(0<x<=256 for x in r[5:7])and(r[2]!=1 or(r[3]%r[5]==0 and r[4]%r[6]==0)),'House resource')
 roles=set();owned_keys=set()
 for c in t['Clips']:
  require(1<=c[1]<=len(CLIP_ROLES)and (c[8],c[1])not in roles and(c[2]==0xffffffff or c[2]<len(t['Resources']))and c[4]>0 and c[3]+c[4]<=len(t['Keys'])and finite(c[5])and 0<c[5]<=120 and c[6]<=1 and c[7]<=2,'House clip');roles.add((c[8],c[1]));prev=-1
  require((c[8]<len(t['Profiles']))if c[1]<=8 else c[8]==0xffffffff,'House clip profile')
  if c[1]<=8 or c[1]==13:require(c[2]<len(t['Resources'])and c[7]==1,'House frame clip')
  else:require(c[7]in[0,2],'House position clip')
  for j in range(c[3],c[3]+c[4]):
   k=t['Keys'][j];require(j not in owned_keys and all(finite(x)for x in k[:4])and 0<=k[0]<=c[5]and k[0]>prev and abs(k[1])<=100,'House key');owned_keys.add(j);prev=k[0]
   if c[1]<=8 or c[1]==13:require(k[4]<t['Resources'][c[2]][5]*t['Resources'][c[2]][6],'House frame')
 require(len(owned_keys)==len(t['Keys']),'House orphan key')
 expected={(0xffffffff,role)for role in range(9,14)}
 for i,q in enumerate(t['Profiles']):
  require(q[1]<len(t['Resources'])and all(finite(x)for x in q[2:6])and q[6]in[1,4]and q[7]&~7==0,'House profile')
  expected.update((i,role)for role in range(1,1+q[6]));
  if q[7]&4:expected.update((i,role)for role in range(5,5+q[6]))
 require(roles==expected,'House profile clip coverage')
 for c in t['Clips']:
  if c[8]!=0xffffffff:require(c[2]==t['Profiles'][c[8]][1],'House profile resource')
 params=set()
 for k,*v in t['Parameters']:require(1<=k<=len(PARAMETERS)and k not in params and all(finite(x)for x in v),'House parameter');params.add(k)
 param={PARAMETERS[r[0]-1]:r[1:]for r in t['Parameters']};f=param['FadeShader'];require(f[0]>0 and f[1]>0 and f[2:]==(0,0),'House fade shader');require(all(0<=x<=1 for x in param['FadeCuts']),'House fade cuts')
 for name in ['DialogueRect','DialogueClip','DialogueText','DialogueBullet','NameRect','NameLabel','CursorGeometry']:require(all(0<x<=8192 for x in param[name][2:]),'House layout size')
 for name in ['DialogueMargins','NameMargins']:require(all(0<=x<=8192 for x in param[name]),'House margins')
 sizing=param['NameSizing'];require(sizing[0]>=0 and 0<sizing[1]<=120 and 0<sizing[2]<=100 and sizing[3]>=0,'House name sizing')
 metrics=param['FontMetrics'];require(0<metrics[0]<=256 and all(0<=x<=256 for x in metrics[1:]),'House font metrics')
 display=param['DisplayReference'];require(all(0<x<=8192 for x in display[:2])and all(0<=x<=1 for x in display[2:]),'House display reference')
 for name in ['TextTiming','TextTagSpeeds']:require(all(x>0 for x in param[name][:3])and param[name][3]==0,'House text parameters')
 delay=param['NpcInteractionReturn'];require(0<delay[0]<=120 and delay[1:]==(0,0,0),'House NPC interaction return delay')
 seen=set()
 for o in t['Overrides']:require(o[0]<len(t['Npcs'])and bool(string(o[1]))and safe_path(string(o[2]))and(o[0],o[1])not in seen and(o[3]==0xffffffff or 0<=o[3]<len(t['Dialogues']))and bool(string(o[4])),'House override');seen.add((o[0],o[1]))

 for d in t['OpenableDoors']:
  require(safe_path(string(d[1]))and d[2]>0 and d[3]>0 and d[2]!=d[3]and d[4]<len(t['Resources']),'House openable binding')
  for i in [5,6,7,8]:string(d[i])
  require(d[9]<len(t['Dialogues'])and all(safe_path(string(d[i]))for i in [10,11,14])and all(not string(d[i])or safe_path(string(d[i]))for i in [12,13]),'House openable strings')
  require(d[15]&~15==0 and 0<d[16]<=7 and d[17]&~d[16]==0 and 0<d[18]<=7 and d[19]&~d[18]==0 and d[20]==1,'House openable policy/state')
  require(all(finite(x)for x in d[21:])and all(0<x<=10000 for j in [25,29,33]for x in d[j:j+2])and -1<=d[41]<=1 and all(0<x<=120 for x in d[42:45])and 0<d[45]<=100 and 0<d[46]<=120,'House openable geometry/timing')
 owned_conditions=set()
 for d in t['StoryTriggers']:
  require(safe_path(string(d[1]))and safe_path(string(d[2]))and all(finite(x)for x in d[3:7])and all(0<x<=10000 for x in d[5:7])and d[8]>0 and d[7]+d[8]<=len(t['StoryConditions'])and (d[9]==1 and d[10]==0xffffffff or d[9]==2 and 0<=d[10]<1024),'House story trigger')
  for j in range(d[7],d[7]+d[8]):require(j not in owned_conditions,'House story condition ownership');owned_conditions.add(j)
 require(len(owned_conditions)==len(t['StoryConditions']),'House orphan story condition')
 for c in t['StoryConditions']:require(c[0]in[1,2]and bool(string(c[1]))and c[2]in[0,1],'House story condition')

def stage_files(source):
 blob=(source/'data/opening.enchouse').read_bytes();t=parse_pack(blob);p=t['Strings'];out={Path('data/opening.enchouse'):blob}
 for r in t['Resources']:
  path=p[r[1]:p.index(0,r[1])].decode();require(safe_path(path),'House staged path');full=(source/path).resolve();require(full.is_relative_to(source.resolve()),'House staged escape');data=full.read_bytes();require(hashlib.sha256(data).digest()==r[7],'House staged hash');out[Path(path)]=data
 return out
def main():
 p=argparse.ArgumentParser();p.add_argument('action',nargs='?',choices=['compile','verify'],default='compile');p.add_argument('--input',type=Path,default=ROOT/'content/native-house.json');p.add_argument('--presentation',type=Path,default=ROOT/'content/native-house-presentation.json');p.add_argument('--out',type=Path,default=ROOT/'romfs/data/opening.enchouse');a=p.parse_args()
 try:
  ir=json.loads(a.input.read_text());pres=json.loads(a.presentation.read_text());verify_sources(ir,pres);t=lower(ir,pres);blob=encode(t,ir['commit'],ir['schema']);parse_pack(blob)
  if a.action=='compile':
   a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_bytes(blob);report=ROOT/'reports/house-data';report.mkdir(parents=True,exist_ok=True);(report/'compile.json').write_text(json.dumps(dict(schema=2,commit=ir['commit'],ir_sha256=digest(a.input),presentation_sha256=digest(a.presentation),bytes=len(blob),sha256=hashlib.sha256(blob).hexdigest(),sections={k:len(v)for k,v in t.items()}),indent=2)+'\n')
  else:require(a.out.read_bytes()==blob,'House pack stale or changed')
  print('House content:',len(blob),'bytes; checked independent ENCHSE01')
 except(OSError,ValueError,KeyError,TypeError,struct.error,OverflowError)as e:print('HOUSE CONTENT ERROR:',e,file=sys.stderr);return 1
 return 0
if __name__=='__main__':raise SystemExit(main())
