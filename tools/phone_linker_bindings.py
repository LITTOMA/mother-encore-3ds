"""Reviewed Phone Room actor/audio linking data with source and identity checks."""
from __future__ import annotations
import hashlib,json,math,re,wave
from pathlib import Path
from tools.extract_battle_entry import Extractor,require,node,one
IR='content/phone-linker-bindings.json'

def pairs(rows):
 result={}
 for k,v in rows:require(k not in result,'Duplicate phone linker field '+k);result[k]=v
 return result

def read(path):return json.loads(Path(path).read_text(encoding='utf-8'),object_pairs_hook=pairs,parse_constant=lambda v:(_ for _ in ()).throw(ValueError('Nonfinite phone linker JSON')))
def fields(v,expected):require(type(v)is dict and set(v)==set(expected.split()),'Unknown/missing phone linker fields')
def path(v):require(type(v)is str and v and not v.startswith('/') and not any(c in v for c in (':','\\','..','\0')),'Unsafe phone linker path');return v
def integer(v,maximum=2**32-1):require(type(v)is int and 0<=v<=maximum,'Phone linker integer rejected');return v

def load(root,document=None):
 root=Path(root);c=document if document is not None else read(root/IR);ex=Extractor(root)
 fields(c,'schema commit paths sources world house house_presentation house_assets house_receipt dialogue_first_id room_prefix actors carol audio audio_prefix_count sound_roles');require(type(c['schema'])is int and c['schema']==1 and c['commit']==ex.lock['commit'],'Phone linker schema/pin')
 fields(c['room_prefix'],'Resource Program Binding')
 for value in c['room_prefix'].values():integer(value,65535)
 fields(c['paths'],'scene npc_script npc_scene animation bus manager title');fields(c['sound_roles'],'ring hangup')
 require(type(c['audio'])is list and c['audio'] and integer(c['audio_prefix_count'],64)<=len(c['audio']),'Phone linker audio scope')
 expected=set(c['paths'].values())|{p for r in c['audio']for p in (r['source'],r['source']+'.import')};require(set(c['sources'])==expected,'Phone linker source coverage')
 for p,d in c['sources'].items():require(hashlib.sha256(ex.data(path(p))).hexdigest()==d,'Stale phone linker source '+p)
 for k in ('world','house','house_presentation','house_assets','house_receipt'):require((root/path(c[k])).is_file(),'Missing phone linker dependency')
 # Reuse the checked House source identity projection; do not duplicate its facts.
 from tools.house_source_bindings import load as house_load
 house=house_load(root);require(integer(c['dialogue_first_id'])==min(house['phone']['text_ids'].values()),'Phone text namespace differs');world=read(root/c['world']);require(world['commit']==c['commit'],'Phone world identity pin')
 actors={}
 for actor in c['actors']:
  fields(actor,'alias kind reference');require(type(actor['alias'])is str and actor['alias'] and actor['alias']not in actors,'Duplicate phone actor alias')
  if actor['kind']=='none':require(actor['reference']=='','No-actor reference');index=65535 # Command schema sentinel.
  elif actor['kind']=='world':
   rows=[r for r in world['actors']if r['alias']==actor['reference']];require(len(rows)==1,'Unknown phone world actor');index=integer(rows[0]['id'])-1
  elif actor['kind']=='house':
   rows=[r for r in house['npcs']if r['source_path']==actor['reference']];require(len(rows)==1,'Unknown phone House actor');index=rows[0]['room_actor']
  else:raise ValueError('Unknown phone actor reference kind')
  require(index not in actors.values(),'Duplicate phone actor index');actors[actor['alias']]=index
 carol=c['carol'];fields(carol,'role source shared_role shared_source shared_actor_index actor_stable_id actor_index execution_kind native_yaml_index native_animation_index directions states direction sprite_node sprite_prefix sprite_suffix')
 rows=[r for r in house['npcs']if r['source_path']==carol['source']];shared=[r for r in house['npcs']if r['source_path']==carol['shared_source']]
 require(len(rows)==len(shared)==1 and rows[0]['room_actor']==carol['actor_index'] and rows[0]['actor_stable_id']==carol['actor_stable_id'] and shared[0]['room_actor']==carol['shared_actor_index'] and rows[0]['resource']==carol['role'] and shared[0]['resource']==carol['shared_role'],'Phone Carol source/stable identity differs')
 for k in ('shared_actor_index','actor_index','actor_stable_id','execution_kind','native_yaml_index','native_animation_index'):integer(carol[k],65535)
 require(carol['actor_index']==len(world['actors']) and carol['actor_stable_id']==carol['actor_index']+1,'Phone Carol append-only actor prefix')
 scene=ex.text(c['paths']['scene']);node(scene,carol['source']);node(scene,carol['shared_source']);node(ex.text(c['paths']['npc_scene']),carol['sprite_node'])
 npc=ex.text(c['paths']['npc_script']);require('res://'+c['paths']['animation']in npc,'NPC inherited animation source')
 default_direction=[float(v)for v in one(r'if initial_dir == Vector2.ZERO:\s*initial_dir = Vector2\(([^)]+)\)',npc,'NPC inherited initial direction')[1].split(',')]
 require('initial_dir'not in node(scene,carol['source'])and carol['direction']==default_direction,'Phone actor source direction mismatch')
 anim=ex.yaml(c['paths']['animation']);require(len(carol['directions'])==anim['size'][1] and len(set(carol['directions']))==len(carol['directions']),'Phone direction scope')
 motions=set()
 for state in carol['states']:
  fields(state,'motion name');integer(state['motion'],65535);require(state['motion']not in motions and state['name']in anim['animations'],'Unknown/duplicate Carol source motion');motions.add(state['motion'])
 require(len(carol['direction'])==2 and all(type(v)in (int,float) and math.isfinite(v)for v in carol['direction']),'Phone actor direction')
 pcm=set();sources=set();ids=set()
 for r in c['audio']:
  fields(r,'source pcm gain_db identity conversion');path(r['source']);path(r['pcm']);require(r['pcm'].endswith('.pcm')and r['pcm']not in pcm and r['source']not in sources,'Duplicate/invalid Phone audio paths');pcm.add(r['pcm']);sources.add(r['source'])
  require(type(r['gain_db'])in(int,float) and math.isfinite(r['gain_db']) and -120<=r['gain_db']<=0,'Phone audio gain')
  ref=r['identity'];require(type(ref)is dict,'Phone audio identity')
  if ref.get('kind')=='stable':fields(ref,'kind value');require(integer(ref['value'])>0 and ref['value']not in ids,'Duplicate/invalid Phone audio ID');ids.add(ref['value'])
  elif ref.get('kind')=='room':fields(ref,'kind source');require(ref['source']=='res://'+r['source'],'Phone audio Room source binding')
  else:raise ValueError('Unknown Phone audio identity kind')
  if r['conversion']is not None:
   conversion=r['conversion'];fields(conversion,'source_sample_rate output_sample_rate');rate=integer(conversion['output_sample_rate']);require(8000<=rate<=48000,'Phone audio target sample rate')
   with wave.open(str(ex.upstream/r['source']),'rb')as source:require(source.getframerate()==conversion['source_sample_rate'],'Phone audio resample source rate changed')
 require(set(c['sound_roles'].values())<sources,'Phone source sound role missing')
 from tools.phone_presentation_bindings import load as presentation_load
 require(c['sound_roles']==presentation_load(root)['sounds'],'Phone sound source roles differ')
 # Title music binding is checked as a source expression, not only as a hash.
 title=ex.text(c['paths']['title']);title_music=one(r'\.stream != load\("res://([^"\n]+)"\)',title,'Title music source')[1];require(title_music in sources,'Phone audio Title stream binding missing')
 return c,actors

def audio(root,room,document=None):
 root=Path(root);c,_=load(root,document);ex=Extractor(root);ids={room['strings'][r['path_string']]:r['stable_id']for r in room['sections']['Resource']};room_sources={identity:source for source,identity in ids.items()};assets=[];seen=set()
 for source in c['audio']:
  ref=source['identity'];identity=ref['value']if ref['kind']=='stable'else ids.get(ref['source']);require(type(identity)is int and identity>0 and identity not in seen,'Unknown/duplicate linked audio identity');seen.add(identity)
  require(identity not in room_sources or room_sources[identity]=='res://'+source['source'],'Linked audio identity differs from actual Room source')
  p=source['source'];ex.data(p);ex.data(p+'.import');r=dict(stable_id=identity,source_path='res://'+p,source_sha256=ex.sources[p],import_sha256=ex.sources[p+'.import'],pcm_path=source['pcm'],gain_db=source['gain_db'])
  if source['conversion']is not None:r['output_sample_rate']=source['conversion']['output_sample_rate']
  assets.append(r)
 return dict(schema=1,upstream_commit=c['commit'],bus_source=c['paths']['bus'],bus_sha256=c['sources'][c['paths']['bus']],manager_source=c['paths']['manager'],manager_sha256=c['sources'][c['paths']['manager']],assets=assets)

def verify_audio(value,root):
 root=Path(root);require(value==audio(root,read(root/'content/native-opening.json')),'Stale/unreviewed linked AudioIR')

def first_text(root):
 c=read(Path(root)/IR);require(type(c.get('schema'))is int and c['schema']==1,'Phone linker schema');return integer(c['dialogue_first_id'])
