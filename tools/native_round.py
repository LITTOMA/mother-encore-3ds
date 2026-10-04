#!/usr/bin/env python3
"""Compile strictly typed first-round content into checked ENCRND01 bytes."""
import argparse,hashlib,json,math,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
PIN='7d9246600fffe518408f5830d4848635019005a3'
from extract_battle_round import RULE_NAMES
SECTION_NAMES=['Strings','Skills','EnemyChoices','Rules','Bindings','Texts','Resources','Media','Tracks','Keys','Events','PresentationBindings','Parameters','Victory']
FORMATS=[None,'<10I7i2Ii','<2I','<Id','<16I','<5I','<7I32s','<9I11f','<7I','<6f','<IfI','<2I','<I4f','<16I']
STRIDES=[1]+[struct.calcsize(x)for x in FORMATS[1:]]
HEADER=64+16*len(STRIDES)
V3_NAMES=SECTION_NAMES+['Encounter','Growth','BossShakes']
V3_FORMATS=FORMATS+['<9I','<4I','<5f']
V3_STRIDES=STRIDES+[36,16,20]
V4_FORMATS=FORMATS+['<10I','<4I','<5f']
V4_STRIDES=STRIDES+[40,16,20]
V3_ENCOUNTER_FIELDS=['boss','keep_actor','post_win_script','boss_flash_media','promoted_level','following_level_exp','level_text','learned_skill','learned_text']
ENCOUNTER_FIELDS=V3_ENCOUNTER_FIELDS+['stop_area_music_if_overworld']
PARAMETERS=['TextTiming','TextTagSpeeds','HpDigits','PartyHit','PartyShown','FlyingNumberRandom','DamageGlyphGrid','TargetPointerOffset','TargetGlow','DialogueMargins','DialogueTextLayout','DamageShadow','SmashTiming','SmashOffset','PartyBounceMotion','PlateHitIntensity','RisingNumberSize','ReturnPartyGeometry','ReturnPartyFrames','ReturnPartyTurn','ReturnCamera']
SLOTS=['PartyIdle','PartyPrepare','PartyReturn','PartyGuard','EnemyFlash','EnemyAttack','EnemyHit','EnemyDefeat','PartyHit','TargetPointer','Dialogue','FlyingNumber','RisingNumber','BackgroundDim','PartyHit2','PartyHit3','PartyShow','PartyHide','PartyBounce','PartyShake','PlateQuake','EnemyDodge','PartyDodge','BackgroundUndim','TargetNameBox','DialogueText','TargetNameText','Smash','SmashBackground','PartyGuardPrepare','DialogueCursor','PartyVictory','VictoryBanner','ReturnTop','ReturnBottom','ReturnPlate','ReturnTimeline','PartyJumpToWorld']
SKILL_FIELDS=['id','source','name','description','dialog','action_type','target_type','skill_type','damage_type','traits','power','variance','priority','miss_chance','pp_cost','hp_cost','crit_chance','user_media','hit_media','fail_chance']
BIND_FIELDS=['battle_id','player_participant','enemy_participant','basic_skill','guard_skill','basic_menu','items_menu','guard_menu','locale','enemy_name','enemy_article','enemy_outro','mortal_damage','no_effect','show_intro_outro','win_flag']
VICTORY_FIELDS=['initial_exp','initial_level','initial_bank','initial_cash','initial_earned_cash','reward_exp','reward_cash','reward_item_count','level_cap','next_level_exp','max_exp','exp_text','acknowledgment','currency_policy','earned_cash_flag','enemy_body_id']
class ContentError(ValueError):pass
def require(value,message):
 if not value:raise ContentError(message)
def fields(obj,names,label):require(isinstance(obj,dict)and set(obj)==set(names),'Unknown/missing '+label+' fields')
def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def finite(v):return isinstance(v,(int,float))and not isinstance(v,bool)and math.isfinite(v)
def uint(v):return isinstance(v,int)and not isinstance(v,bool)and 0<=v<=0xffffffff
def vector(v,n):require(isinstance(v,list)and len(v)==n and all(finite(x)for x in v),'Bad vector');return v
def safe_path(name):return isinstance(name,str)and bool(name)and not name.startswith('/')and all(x not in ('','..','.')for x in name.split('/'))and ':'not in name and '\\'not in name

def schema_layout(version):
 require(version in [2,3,4,5],'Round version')
 return {2:(SECTION_NAMES,FORMATS,STRIDES),3:(V3_NAMES,V3_FORMATS,V3_STRIDES),4:(V3_NAMES,V4_FORMATS,V4_STRIDES),5:(V3_NAMES,V4_FORMATS,V4_STRIDES)}[version]

class RoundTables(dict):
 """Keep the checked schema explicit while preserving the table mapping API."""
 def __init__(self,version):
  names,_,_=schema_layout(version);super().__init__((name,[])for name in names);self.version=version

def verify_sources(ir,root=ROOT):
 from battle_round_bindings import load,check_round
 check_round(ir,load(root))
 require(ir['schema']in[2,3,4,5] and ir['kind']=='encore.native-battle-round.source-ir' and ir['commit']==PIN,'Unreviewed round schema/pin')
 review='reports/pillow-battle/source-review.json'if ir['schema']==5 and ir['binding']['battle_id']==3 else('reports/doll-round/source-review.json'if ir['schema']in[3,4,5] else'reports/battle-victory-data/source-review.json')
 report=json.loads((root/review).read_text())
 require(report['schema']==ir['schema'] and report['commit']==PIN and report['sources']==ir['sources'] and report['dependencies']==ir['dependencies'],'Round source review does not match')
 inventory=json.loads((root/'compatibility/upstream-inventory.json').read_text())
 require(inventory['commit']==PIN,'Round inventory pin')
 require(isinstance(ir['sources'],dict)and len(ir['sources'])>=20,'Round sources missing')
 for path,sha in ir['sources'].items():
  require(safe_path(path)and inventory['files'][path]['sha256']==sha and digest(root/'upstream/MOTHER-Encore'/path)==sha,'Changed reviewed source '+path)
 expected={'content/native-battle.json','content/native-opening.json','content/native-round.json','content/pillow-entry.json','content/doll-round.json'}if ir['schema']==5 and ir['binding']['battle_id']==3 else({'content/native-battle.json','content/native-opening.json','content/doll-entry.json','content/native-round.json'}if ir['schema']in[3,4,5] else{'content/native-battle.json','content/native-opening.json'})
 require(set(ir['dependencies'])==expected,'Unknown round dependency')
 for path,sha in ir['dependencies'].items():require(digest(root/path)==sha,'Changed round dependency '+path)

def lower(ir,root=ROOT,verify_assets=True):
 require(ir['schema']in[2,3,4,5] and ir['commit']==PIN,'Unreviewed round schema/pin')
 fields(ir,['schema','kind','commit','scope','sources','dependencies','skills','enemy_choices','rules','binding','texts','presentation','victory']+(['encounter','growth','boss_shakes']if ir['schema']in[3,4,5] else[]),'round IR')
 pool=bytearray(b'\0');strings={'':0}
 def string(s):
  require(isinstance(s,str)and '\0'not in s and len(s.encode())<=4096,'Invalid string')
  if s not in strings:strings[s]=len(pool);pool.extend(s.encode()+b'\0')
  return strings[s]
 tables=RoundTables(ir['schema'])
 for s in ir['skills']:
  fields(s,SKILL_FIELDS,'skill');tables['Skills'].append([string(s[k])if k=='source'else s[k]for k in SKILL_FIELDS])
 for c in ir['enemy_choices']:
  fields(c,['skill','weight'],'enemy choice');tables['EnemyChoices'].append([c['skill'],c['weight']])
 fields(ir['rules'],RULE_NAMES,'rules');tables['Rules']=[[i+1,ir['rules'][name]]for i,name in enumerate(RULE_NAMES)]
 fields(ir['binding'],BIND_FIELDS,'binding');tables['Bindings']=[[string(ir['binding'][k])if k in ['locale','win_flag']else ir['binding'][k]for k in BIND_FIELDS]]
 fields(ir['victory'],VICTORY_FIELDS,'victory');tables['Victory']=[[string(ir['victory'][k])if k=='earned_cash_flag'else ir['victory'][k]for k in VICTORY_FIELDS]]
 for t in ir['texts']:
  fields(t,['id','role','key','source_text','text'],'text');tables['Texts'].append([t['id'],t['role'],string(t['key']),string(t['source_text']),string(t['text'])])
 p=ir['presentation'];fields(p,['resources','media','tracks','keys','events','bindings','parameters'],'presentation')
 for r in p['resources']:
  fields(r,['id','path','kind','width','height','columns','rows','sha256'],'resource');require(safe_path(r['path']),'Resource path')
  sha=bytes.fromhex(r['sha256']);require(len(sha)==32,'Resource SHA')
  if verify_assets:require(digest(root/'romfs'/r['path'])==r['sha256'],'Changed round resource '+r['path'])
  tables['Resources'].append([r['id'],string(r['path']),r['kind'],r['width'],r['height'],r['columns'],r['rows'],sha])
 for m in p['media']:
  fields(m,['id','name','role','resource','first_track','track_count','first_event','event_count','flags','duration','rect','color','anchor'],'media')
  tables['Media'].append([m['id'],string(m['name']),m['role'],m['resource'],m['first_track'],m['track_count'],m['first_event'],m['event_count'],m['flags'],m['duration'],*vector(m['rect'],4),*vector(m['color'],4),*vector(m['anchor'],2)])
 for t in p['tracks']:
  names=['media','property','first','count','update','interpolation','mode'];fields(t,names,'track');tables['Tracks'].append([t[x]for x in names])
 for k in p['keys']:
  fields(k,['time','ease','value'],'key');tables['Keys'].append([k['time'],k['ease'],*vector(k['value'],4)])
 for e in p['events']:
  fields(e,['media','time','kind'],'event');tables['Events'].append([e['media'],e['time'],e['kind']])
 if p['media']:
  fields(p['bindings'],SLOTS,'presentation bindings');fields(p['parameters'],PARAMETERS,'presentation parameters')
 else:require(not p['bindings']and not p['parameters'],'Media missing')
 tables['PresentationBindings']=[[SLOTS.index(k)+1,v]for k,v in p['bindings'].items()]
 tables['Parameters']=[[PARAMETERS.index(k)+1,*vector(v,4)]for k,v in p['parameters'].items()]
 if ir['schema']in[3,4,5]:
  encounter_fields=ENCOUNTER_FIELDS if ir['schema']in[4,5] else V3_ENCOUNTER_FIELDS
  fields(ir['encounter'],encounter_fields,'encounter');tables['Encounter']=[[string(ir['encounter'][k])if k in['post_win_script','learned_skill']else ir['encounter'][k]for k in encounter_fields]]
  for g in ir['growth']:
   names=['stat','before','after','text'];fields(g,names,'growth');tables['Growth'].append([g[k]for k in names])
  for q in ir['boss_shakes']:
   names=['time','magnitude','length','interval','weight'];fields(q,names,'boss shake');tables['BossShakes'].append([q[k]for k in names])
 tables['Strings']=bytes(pool)
 return tables

def encode(tables,commit=PIN):
 require(isinstance(tables,RoundTables),'Round tables require explicit schema')
 version=tables.version;names,formats,strides=schema_layout(version);require(set(tables)==set(names),'Round tables do not match schema')
 data=bytearray(64+16*len(names))
 for i,name in enumerate(names):
  block=tables[name]if i==0 else b''.join(struct.pack(formats[i],*r)for r in tables[name])
  while len(data)%4:data.append(0)
  offset=len(data)if block else 0;struct.pack_into('<HHIII',data,64+i*16,i+1,strides[i],offset,len(block)//strides[i],len(block));data.extend(block)
 struct.pack_into('<8s6I20s12x',data,0,b'ENCRND01',version,len(data),0,len(strides),version,version,bytes.fromhex(commit))
 struct.pack_into('<I',data,16,zlib.crc32(data));return bytes(data)

def parse_pack(blob):
 require(HEADER<=len(blob)<=2*1024*1024,'Round binary size')
 magic,version,size,crc,n,caps,rules,commit=struct.unpack_from('<8s6I20s',blob)
 names,formats,strides=schema_layout(version)
 require(magic==b'ENCRND01'and size==len(blob)and n==len(strides)and caps==rules==version and not any(blob[52:64]),'Round header')
 copy=bytearray(blob);struct.pack_into('<I',copy,16,0);require(zlib.crc32(copy)==crc,'Round CRC')
 tables=RoundTables(version);end=64+16*n
 require(end<=len(blob),'Round header size')
 for i,name in enumerate(names):
  kind,stride,offset,count,amount=struct.unpack_from('<HHIII',blob,64+i*16)
  require(kind==i+1 and stride==strides[i]and amount==count*stride,'Round directory')
  if count:
   require(offset%4==0 and offset>=end and offset+amount<=len(blob)and not any(blob[end:offset]),'Round span')
   block=blob[offset:offset+amount];end=offset+amount
  else:require(offset==amount==0,'Round empty section');block=b''
  tables[name]=block if i==0 else list(struct.iter_unpack(formats[i],block))
 require(end==len(blob),'Round trailing bytes')
 validate(tables);return tables

def validate(t):
 require(isinstance(t,RoundTables),'Round tables require explicit schema')
 version=t.version;names,_,_=schema_layout(version);require(set(t)==set(names),'Round tables do not match schema')
 p=t['Strings'];require(p and len(p)<=65536 and p[0]==p[-1]==0,'Round string pool');p.decode('utf-8')
 def string(o):
  require(uint(o)and o<len(p)and(o==0 or p[o-1]==0),'Round string offset');end=p.find(b'\0',o);require(end>=o and end-o<=4096,'Round string length');return p[o:end].decode()
 limits={'Skills':(1,32),'EnemyChoices':(1,32),'Rules':(len(RULE_NAMES),len(RULE_NAMES)),'Bindings':(1,1),'Texts':(1,256),'Resources':(0,128),'Media':(1,256),'Tracks':(0,2048),'Keys':(0,8192),'Events':(0,1024),'PresentationBindings':(0,len(SLOTS)),'Parameters':(0,len(PARAMETERS)),'Victory':(1,1)}
 for k,(lo,hi)in limits.items():require(lo<=len(t[k])<=hi,'Round '+k+' capacity')
 for k in ['Skills','Texts','Resources','Media']:
  ids=[r[0]for r in t[k]];require(all(x>0 for x in ids)and len(set(ids))==len(ids),'Round duplicate/zero ID')
 for s in t['Skills']:
  require(safe_path(string(s[1]))and all(x<len(t['Texts'])for x in s[2:5]),'Round skill text')
  require(s[5]in[0,4]and s[6]in[0,5]and s[7]<=2 and s[8]<=1 and s[9]&~1==0,'Round skill opcode')
  require(0<=s[10]<=100000 and 0<=s[11]<=10000 and -100<=s[12]<=100 and 0<=s[13]<=100 and s[14]==s[15]==s[19]==0 and 0<=s[16]<=100,'Round skill numeric path')
  require(all(x==0xffffffff or x<len(t['Media'])for x in s[17:19]),'Round skill media')
  require((s[5]==0 and s[6]==0 and s[7]in[1,2]and s[8]==1 and not s[9])or(s[5]==4 and s[6]==5 and s[7]==s[8]==s[10]==s[11]==0),'Round skill supported path')
  require(not(s[9]&1)or(s[4]==0 and s[5]==4),'Round guard path')
 choices=set()
 for c in t['EnemyChoices']:
  require(c[0]<len(t['Skills'])and 1<=c[1]<=100000 and c[0]not in choices,'Round enemy choice');choices.add(c[0])
 rules={}
 for k,v in t['Rules']:require(1<=k<=len(RULE_NAMES)and k not in rules and finite(v)and 0<v<=100000,'Round rule');rules[k]=v
 for name in ['PercentScale','HpTransitionFrames']:require(rules[RULE_NAMES.index(name)+1].is_integer(),'Round integer rule')
 b=t['Bindings'][0];require(b[0]>0 and b[1]==0 and b[2]==1 and b[3]<len(t['Skills'])and b[4]<len(t['Skills'])and len(set(b[5:8]))==3 and all(x>0 for x in b[5:8]),'Round binding')
 require(string(b[8])and all(x<len(t['Texts'])for x in b[9:14])and b[14]<=1,'Round binding text');string(b[15])
 require(t['Skills'][b[3]][7]==1 and t['Skills'][b[4]][9]&1,'Round bound skill role')
 for row in t['Texts']:
  require(row[1]<=(12 if version in[3,4,5] else 9),'Round text role');[string(x)for x in row[2:]]
 require(t['Texts'][0][1:]==(0,0,0,0),'Round empty text')
 v=dict(zip(VICTORY_FIELDS,t['Victory'][0]))
 require(v['enemy_body_id']>0 and 0<v['initial_level']<v['level_cap']<=1000 and v['reward_exp']>0 and v['reward_item_count']==0 and (version in[3,4,5] or v['initial_exp']+v['reward_exp']<v['next_level_exp'])and v['next_level_exp']<=v['max_exp']<=0x7fffffff,'Round unsupported victory progression/reward state')
 require(v['initial_cash']<=0x7fffffff and v['initial_bank']+v['reward_cash']<=0x7fffffff and v['initial_earned_cash']+v['reward_cash']<=0x7fffffff,'Round victory currency overflow')
 require(v['exp_text']<len(t['Texts'])and t['Texts'][v['exp_text']][1]==9 and string(t['Texts'][v['exp_text']][4])and v['acknowledgment']==1 and v['currency_policy']==1 and string(v['earned_cash_flag'])and v['earned_cash_flag']!=b[15],'Round victory binding/policy')
 if version in[3,4,5]:
  require(len(t['Encounter'])==1 and len(t['Growth'])==7 and (version==5 or len(t['BossShakes'])==3),'Round encounter capacity')
  encounter_fields=ENCOUNTER_FIELDS if version in[4,5] else V3_ENCOUNTER_FIELDS
  require(len(t['Encounter'][0])==len(encounter_fields),'Round encounter stride')
  e=dict(zip(encounter_fields,t['Encounter'][0]))
  if version in[4,5]:require(uint(e['stop_area_music_if_overworld'])and e['stop_area_music_if_overworld']<=1,'Round area music policy')
  if version==5:
   require(e['boss']in[0,1]and e['keep_actor']in[0,1]and len(t['BossShakes'])==(3 if e['boss']else 0),'Round conditional encounter capacity')
   require((e['boss_flash_media']<len(t['Media']))if e['boss']else(e['boss_flash_media']==0xffffffff),'Round conditional boss media')
   require(v['initial_exp']<v['next_level_exp']<e['following_level_exp'] and v['initial_exp']+v['reward_exp']<e['following_level_exp'],'Round conditional progression range')
   if not e['boss']:
    require(not any(event[2]>=10 for event in t['Events'])and not any(media[2]==12 for media in t['Media']),'Round nonboss effect rejected')
  else:require(e['boss']==e['keep_actor']==1 and e['boss_flash_media']<len(t['Media'])and v['next_level_exp']<=v['initial_exp']+v['reward_exp']<e['following_level_exp'],'Round legacy encounter policy')
  require(safe_path(string(e['post_win_script']))and e['promoted_level']==v['initial_level']+1<v['level_cap']and v['next_level_exp']<e['following_level_exp']<=v['max_exp']and e['level_text']<len(t['Texts'])and t['Texts'][e['level_text']][1]==10 and string(e['learned_skill'])and e['learned_text']<len(t['Texts'])and t['Texts'][e['learned_text']][1]==12 and not string(b[15]),'Round encounter policy')
  require({g[0]for g in t['Growth']}==set(range(1,8)),'Round growth stats')
  for stat,before,after,text in t['Growth']:require(0<=before<=after<=0x7fffffff and text<len(t['Texts'])and(text==0 if before==after else t['Texts'][text][1]==11),'Round growth')
  prior=-1
  for time,magnitude,length,interval,weight in t['BossShakes']:
   require(all(finite(x)for x in[time,magnitude,length,interval,weight])and time>prior and time>=0 and 1<magnitude<=100 and 0<interval<=length and length/interval<=1000 and 0<weight<=1,'Round boss shake');prior=time
  if e['boss']:
   require(all(event[0]==(dict(t['PresentationBindings'])[SLOTS.index('EnemyDefeat')+1]if event[2]==10 else e['boss_flash_media'])for event in t['Events']if event[2]>=10),'Round boss callback owner')
   flash=t['Media'][e['boss_flash_media']];defeat=t['Media'][dict(t['PresentationBindings'])[SLOTS.index('EnemyDefeat')+1]]
   require(flash[2]==12 and flash[3]==0xffffffff and flash[7]==defeat[7]==1 and t['Events'][flash[6]][2]==11 and t['Events'][defeat[6]][2]==10 and prior<t['Events'][defeat[6]][1]<defeat[9],'Round boss callback contract')
 for r in t['Resources']:
  require(safe_path(string(r[1]))and r[2]in[1,2,3]and 1<=r[3]<=8192 and 1<=r[4]<=8192 and 1<=r[5]<=256 and 1<=r[6]<=256,'Round resource')
  require(r[2]!=1 or(r[3]%r[5]==0 and r[4]%r[6]==0),'Round grid')
 owned_tracks=set();owned_events=set()
 for i,m in enumerate(t['Media']):
  string(m[1]);require(1<=m[2]<=(12 if version in[3,4,5] else 11) and(m[3]==0xffffffff or m[3]<len(t['Resources']))and m[4]+m[5]<=len(t['Tracks'])and m[6]+m[7]<=len(t['Events'])and m[8]&~7==0 and all(finite(v)for v in m[9:])and 0<=m[9]<=120 and (m[9]>0 or (m[5]==m[7]==0 and not(m[8]&1))) and all(0<=v<=1 for v in m[14:20]),'Round media')
  for j in range(m[4],m[4]+m[5]):
   require(t['Tracks'][j][0]==i and j not in owned_tracks,'Round media track ownership');owned_tracks.add(j)
  prior=-1
  for j in range(m[6],m[6]+m[7]):
   require(t['Events'][j][0]==i and j not in owned_events and t['Events'][j][1]>=prior,'Round media event ownership/order');owned_events.add(j);prior=t['Events'][j][1]
 require(len(owned_tracks)==len(t['Tracks'])and len(owned_events)==len(t['Events']),'Round orphan tracks/events')
 used_keys=set()
 for tr in t['Tracks']:
  require(tr[0]<len(t['Media'])and 1<=tr[1]<=(18 if version in[3,4,5] else 17) and tr[3]>0 and tr[2]+tr[3]<=len(t['Keys'])and tr[4]<=1 and tr[5]<=8 and tr[6]<=2,'Round track opcode/span')
  prev=-1
  for j in range(tr[2],tr[2]+tr[3]):
   k=t['Keys'][j];require(j not in used_keys,'Round overlapping keys');used_keys.add(j)
   require(all(finite(x)for x in k)and prev<k[0]<=t['Media'][tr[0]][9]and k[0]>=0 and abs(k[1])<=100,'Round key');prev=k[0]
   if tr[1]==6:
    r=t['Media'][tr[0]][3];require(r<len(t['Resources'])and 0<=k[2]<t['Resources'][r][5]*t['Resources'][r][6]and k[2].is_integer(),'Round frame')
   if tr[1]in[5,10,14]:require(0<=k[2]<=1,'Round alpha')
   if tr[1]==7:require(k[2]in[0,1],'Round visibility')
   if tr[1]==17:require(k[2]==0,'Round unsupported outline')
 require(len(used_keys)==len(t['Keys']),'Round orphan key')
 for e in t['Events']:require(e[0]<len(t['Media'])and finite(e[1])and 0<=e[1]<=t['Media'][e[0]][9]and 1<=e[2]<=(11 if version in[3,4,5] else 9),'Round event opcode/span')
 bindings=set()
 for slot,index in t['PresentationBindings']:require(1<=slot<=len(SLOTS)and slot not in bindings and index<len(t['Media']),'Round presentation binding');bindings.add(slot)
 params=set()
 for row in t['Parameters']:require(1<=row[0]<=len(PARAMETERS)and row[0]not in params and all(finite(x)for x in row[1:]),'Round presentation parameter');params.add(row[0])
 require(len(bindings)==len(SLOTS)and len(params)==len(PARAMETERS),'Round incomplete presentation')
 if params:
  par={PARAMETERS[row[0]-1]:row[1:]for row in t['Parameters']}
  require(all(v>0 for v in par['TextTiming'])and all(v>0 for v in par['TextTagSpeeds'][:3]),'Round text timing parameter')
  radix,frames,_,_=par['HpDigits'];require(radix>=2 and radix.is_integer()and frames==rules[RULE_NAMES.index('HpTransitionFrames')+1],'Round digit parameter')
  require(all(v>0 for v in par['PartyHit'])and all(v>0 for v in par['PartyShown'])and 0<=par['FlyingNumberRandom'][0]<=par['FlyingNumberRandom'][1],'Round motion parameter')
  require(par['DamageGlyphGrid'][0]>=0 and par['DamageGlyphGrid'][0].is_integer()and all(v>0 for v in par['DamageGlyphGrid'][1:])and par['TargetPointerOffset'][0]>0,'Round glyph/pointer parameter')
  require(all(0<=v<=1 for v in par['TargetGlow'])and all(v>=0 for v in par['DialogueMargins'])and all(v>0 for v in par['DialogueTextLayout'][2:]),'Round text/color parameter')
  require(0<par['SmashTiming'][0]<=1 and par['SmashTiming'][1]>0 and all(v>0 for v in par['PartyBounceMotion'])and all(v>0 for v in par['PlateHitIntensity'][:2])and par['PlateHitIntensity'][2]>=0,'Round effect timing parameter')
  geometry=par['ReturnPartyGeometry'];require(geometry[2]>0 and geometry[3]>0,'Round return geometry')
  camera=par['ReturnCamera'];require(0<camera[0]<=120 and camera[1:]==(0,0,0),'Round bounded return camera')
  turn=par['ReturnPartyTurn'];require(sum(v*v for v in turn[:2])==1 and turn[2]>0 and turn[3]==0,'Round return party turn')
  slotmap=dict(t['PresentationBindings'])
  def bound(name):return slotmap[SLOTS.index(name)+1]
  for name,role in [('PartyVictory',1),('VictoryBanner',3),('ReturnTop',10),('ReturnBottom',10),('ReturnPlate',4),('ReturnTimeline',10),('PartyJumpToWorld',11)]:require(t['Media'][bound(name)][2]==role,'Round victory presentation role')
  timeline=bound('ReturnTimeline');jump=bound('PartyJumpToWorld')
  timeline_events=[e[2]for e in t['Events']if e[0]==timeline];jump_events=[e[2]for e in t['Events']if e[0]==jump]
  require(timeline_events==[4,5,6,7,8] and jump_events==[9],'Round victory callback contract')
  require(all(e[0]==(jump if e[2]==9 else timeline)for e in t['Events']if 4<=e[2]<=9),'Round victory callback owner')
  r=t['Media'][jump][3];require(r<len(t['Resources'])and t['Resources'][r][2]==1,'Round return party resource')
  frames=par['ReturnPartyFrames'];require(all(0<=f<t['Resources'][r][5]*t['Resources'][r][6]and f.is_integer()for f in frames[:2])and frames[2:]==(0,0),'Round return party frame')

def stage_files(source,pack_path=Path('data/opening.encround')):
 blob=(source/pack_path).read_bytes();tables=parse_pack(blob);pool=tables['Strings'];result={pack_path:blob}
 for r in tables['Resources']:
  path=pool[r[1]:pool.index(0,r[1])].decode();require(safe_path(path),'Round staged path');full=(source/path).resolve();require(full.is_relative_to(source.resolve()),'Round staged escape');data=full.read_bytes();require(hashlib.sha256(data).digest()==r[7],'Round staged hash');result[Path(path)]=data
 return result

def main():
 p=argparse.ArgumentParser();p.add_argument('action',nargs='?',choices=['compile','verify'],default='compile');p.add_argument('--input',type=Path,default=ROOT/'content/native-round.json');p.add_argument('--out',type=Path,default=ROOT/'romfs/data/opening.encround');a=p.parse_args()
 try:
  ir=json.loads(a.input.read_text());verify_sources(ir);tables=lower(ir);blob=encode(tables,ir['commit']);parse_pack(blob)
  if a.action=='compile':
   a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_bytes(blob);report=ROOT/'reports/battle-victory-data';report.mkdir(parents=True,exist_ok=True);(report/'compile.json').write_text(json.dumps({'schema':2,'format':2,'capabilities':2,'rules':2,'commit':ir['commit'],'ir_sha256':digest(a.input),'bytes':len(blob),'sha256':hashlib.sha256(blob).hexdigest(),'sections':{k:len(v)for k,v in tables.items()}},indent=2)+'\n')
  else:require(a.out.read_bytes()==blob,'Round pack stale or changed')
  print('Battle round content:',len(blob),'bytes; checked independent ENCRND01')
 except (OSError,ValueError,KeyError,TypeError,struct.error,OverflowError)as e:print('ROUND CONTENT ERROR:',e,file=sys.stderr);return 1
 return 0
if __name__=='__main__':raise SystemExit(main())
