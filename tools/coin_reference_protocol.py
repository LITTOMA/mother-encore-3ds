"""Strict grammar for the retained original Coin/factory observation protocol."""
import math,re,struct
from tools.extract_battle_entry import require
from tools.dungeon_keys import fields
ENGINE='3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8'

def integer(v,maximum=0xffffffff):require(type(v)is int and 0<=v<=maximum,'Coin reference integer')
def boolean(v):require(type(v)is bool,'Coin reference boolean')
def text(v):require(type(v)is str and 0<len(v)<=512 and all(32<=ord(c)<127 for c in v),'Coin reference text')
def vector(v):
 require(type(v)is list and len(v)==2,'Coin reference vector')
 for b in v:integer(b);require(math.isfinite(struct.unpack('<f',struct.pack('<I',b))[0]),'Coin reference nonfinite vector')
def scalar(v):
 require(type(v)is str and re.fullmatch('[0-9a-f]{16}',v),'Coin reference binary64');result=struct.unpack('<d',bytes.fromhex(v))[0];require(math.isfinite(result),'Coin reference nonfinite scalar');return result

def state(v,animations=None):
 require(type(v)is dict,'Coin reference state object');fields(v,'position speed direction air_time start_air_time grounded physics body_disabled pickup_disabled offset sprite_frame sparkle_frame glow animation playing time queued','Coin reference state')
 for key in ['position','direction','offset']:vector(v[key])
 for key in ['speed','air_time','start_air_time','glow']:scalar(v[key])
 for key in ['grounded','physics','body_disabled','pickup_disabled','playing','queued']:boolean(v[key])
 for key in ['sprite_frame','sparkle_frame','time']:integer(v[key])
 require(math.isfinite(struct.unpack('<f',struct.pack('<I',v['time']))[0]),'Coin reference animation clock');text(v['animation'])
 if animations is not None:require(v['animation']in animations,'Coin reference unmapped animation')

def groups(rows):
 require(type(rows)is list and 0<len(rows)<=65536,'Coin reference row count');result=[]
 for row in rows:
  require(type(row)is dict and 'case'in row and 'frame'in row,'Coin reference row identity');integer(row['case'],255);integer(row['frame'],10000)
  if not result or row['case']!=result[-1][0]['case']:
   require(row['case']==len(result)and row['frame']==0,'Coin reference case ordering');result.append([])
  require(row['frame']==len(result[-1]),'Coin reference frame ordering');result[-1].append(row)
 return result

def validate(doc,config=None,animations=None):
 require(type(doc)is dict,'Coin reference root');fields(doc,'schema godot directions rows contacts spawners','Coin reference root');require(type(doc['schema'])is int and doc['schema']==4,'Coin reference protocol version')
 engine=doc['godot'];require(type(engine)is dict and all(type(engine.get(k))is int for k in ['major','minor','patch'])and(engine['major'],engine['minor'],engine['patch'])==(3,6,2)and engine.get('hash')==ENGINE,'Coin reference engine')
 require(type(doc['directions'])is list and 0<len(doc['directions'])<=4096,'Coin reference direction count')
 for v in doc['directions']:
  vector(v);x,y=struct.unpack('<2f',struct.pack('<2I',*v));require(abs(x*x+y*y-1)<1e-5,'Coin reference direction normalization')
 actor_groups=groups(doc['rows'])
 for rows in actor_groups:
  for n,row in enumerate(rows):
   boolean(row.get('alive'));expected='case frame alive cash cash_opens collect_audio drop_audio physics_frame idle_frame'.split()
   if row['alive']:expected.append('state')
   if n==len(rows)-1:expected+=['rng_tail','peek_after_ready','duplicate_collect_called']
   fields(row,' '.join(expected),'Coin reference actor row');integer(row['cash'],0x7fffffffffffffff);integer(row['cash_opens']);require(row['physics_frame']==row['frame']and row['idle_frame']==row['frame']and type(row['physics_frame'])is int and type(row['idle_frame'])is int,'Coin reference separate clock identity')
   if row['alive']:state(row['state'],animations)
   require(type(row['drop_audio'])is list and type(row['collect_audio'])is list and len(row['drop_audio'])<=4096 and len(row['collect_audio'])<=4096,'Coin reference audio queue')
   for frame in row['drop_audio']:integer(frame,row['frame'])
   for event in row['collect_audio']:
    require(type(event)is list and len(event)==3,'Coin reference collect voice');integer(event[0],row['frame']);text(event[1]);text(event[2])
   if n==len(rows)-1:
    integer(row['rng_tail']);boolean(row['duplicate_collect_called'])
    if row['peek_after_ready']is not None:integer(row['peek_after_ready'])
 require(type(doc['contacts'])is list and len(doc['contacts'])<=65536,'Coin reference contacts')
 previous=(-1,-1)
 for contact in doc['contacts']:
  require(type(contact)is dict and contact.get('kind')in ['body','area'],'Coin reference contact kind');fields(contact,'case frame kind identity'+(' layer'if contact['kind']=='area'else''),'Coin reference contact');integer(contact['case'],len(actor_groups)-1);integer(contact['frame'],len(actor_groups[contact['case']])-1);require((contact['case'],contact['frame'])>=previous,'Coin reference contact ordering');previous=(contact['case'],contact['frame'])
  require(type(contact['identity'])is int and contact['identity']in([1,2]if contact['kind']=='body'else[3]),'Coin reference contact identity')
  if contact['kind']=='area':integer(contact['layer'],(1<<20)-1)
 factory_groups=groups(doc['spawners'])
 for rows in factory_groups:
  previous=[]
  for n,row in enumerate(rows):
   fields(row,'case frame timer spawning remaining coins cash cash_opens collect_calls live'+(' rng_tail'if n==len(rows)-1 else''),'Coin reference factory row');require(scalar(row['timer'])>=0,'Coin reference timer');boolean(row['spawning'])
   for k in ['remaining','cash_opens','collect_calls']:integer(row[k])
   integer(row['cash'],0x7fffffffffffffff);require(type(row['coins'])is list and len(row['coins'])<=4096 and row['coins'][:len(previous)]==previous,'Coin reference Ready prefix');previous=row['coins']
   for ordinal,birth in enumerate(row['coins']):
    require(type(birth)is dict,'Coin reference Ready object');fields(birth,'ordinal physics idle parent position speed direction air_time sparkle','Coin reference Ready');require(type(birth['ordinal'])is int and birth['ordinal']==ordinal,'Coin reference Ready ordinal');integer(birth['physics'],row['frame']);integer(birth['idle'],row['frame']);text(birth['parent']);vector(birth['position']);vector(birth['direction']);scalar(birth['speed']);scalar(birth['air_time']);integer(birth['sparkle'])
   require(type(row['live'])is list and len(row['live'])==len(row['coins']),'Coin reference live child count')
   for ordinal,child in enumerate(row['live']):
    require(type(child)is dict,'Coin reference child');fields(child,'ordinal state','Coin reference child');require(type(child['ordinal'])is int and child['ordinal']==ordinal,'Coin reference child order');state(child['state'],animations)
   if n==len(rows)-1:integer(row['rng_tail'])
 if config is not None:
  require(type(config)is dict and len(actor_groups)==len(config['cases'])and len(factory_groups)==len(config['spawner_cases'])and len(doc['directions'])==config['direction_count'],'Coin reference input/output coverage')
  require(all(len(g)==config['frames']+1 for g in actor_groups)and all(len(g)==c['frames']+1 for g,c in zip(factory_groups,config['spawner_cases'])),'Coin reference input/output frame counts')
  require(all(b['parent']==config['factory_parent']for row in doc['spawners']for b in row['coins']),'Coin reference source parent')
 return doc
