#!/usr/bin/env python3
"""Native House continuation adoption; never replays the original cold LOAD.

Source SAVE/LOAD names are checked, but this explicit native migration avoids
serialized Item UID fallback and preserves the already played random stream.
"""
import argparse, hashlib, re, struct, sys, zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,read,write,sha,require,stable
from tools.extract_battle_entry import Extractor
from tools.global_load import load as cold_load,IR as COLD
from tools.field_character_load import derive as chars_derive,IR as CHARS
from tools.global_item_definitions import load as global_defs,IR as DEFS
from tools.native_session import verify_recipe,IR as SESSION
IR=ROOT/'content/native-house-global-bridge.json'
REVIEW=ROOT/'reports/house-global-bridge/source-review.json'
PACK=ROOT/'romfs/data/house.encglobalbridge'
FAMILY=0x454e0060
KEYS=['posX','scene','runsound','shadoweffect','playtime','favoritefood','playername','textspeed','menuflavor','buttonprompts','earned_cash','description','cash','bank','keys','rareDrops','encountered','object_flags','seen_dialogue_flags']

def derive():
 ex=Extractor(ROOT);cold=cold_load();chars=chars_derive();defs=global_defs();session=read(SESSION);verify_recipe(session)
 require(chars==read(CHARS),'Character source resource stale')
 for p,h in cold['sources'].items():ex.data(p);require(ex.sources[p]==h,'Changed source '+p)
 source=ex.text('Scripts/Main/Status.gd');pm=ex.text('Scripts/global/PartyMember.gd');inventory=ex.text('Scripts/global/Inventory.gd');glob=ex.text('Scripts/global/global.gd')
 require(source.startswith('extends Node\nclass_name Status\n'),'Status is not native Node')
 require('var battle_turns := 0\nvar times_afflicted := 1\nvar passive_heal_probability : int'in source,'Unknown Status defaults')
 require('ailment = status\n\tvar status_dictionary: Dictionary = get_data()\n\tif get_data()["healing"].get("passive_heal", false):\n\t\tpassive_heal_probability = status_dictionary["healing"].get("heal_prob", 25)'in source,'Unknown Status constructor')
 require('return globaldata.get_ailment_data(ailment)'in source,'Unknown Status cache getter')
 require('item.get("doses", 1), int(item.get("uid", Item.get_uid(Item.used_uids_tab)))'in inventory,'Unknown serialized UID fallback')
 require('"exp": _exp'in pm and '"status": status_to_array()'in pm and '"inventory": _inventory.to_array()'in pm,'Unknown serialized character fields')
 require('"ninten": globaldata.characters.ninten.to_dict()'in glob,'Unknown SAVE character binding')
 require(len(session['defaults']['characters'])==1 and session['defaults']['party']==['ninten'],'Unknown native continuation roster')
 leader=session['defaults']['party'][0];row=next(x for x in chars['rows']if x['name']==leader)
 assignments=[]
 for role,key in enumerate(KEYS,1):
  found=[(i,a)for i,a in enumerate(cold['assignments'])if a['key']==key];require(len(found)==1,'Missing source LOAD mapping '+key)
  i,a=found[0];assignments.append(dict(role=role,index=i,member=a['member'],key=key,kind=a['kind']))
 statuses=[]
 for policy in session['status_policies']:
  p='Data/StatusAilments/'+policy['id']+'.yaml';raw=ex.yaml(p);healing=raw['healing'];passive=healing.get('passive_heal',False)
  require(passive==policy['passive_healing'],'Status session policy differs')
  statuses.append(dict(id=policy['id'],source=p,source_sha256=ex.sources[p],passive=passive,priority=raw.get('priority',0),probability=healing.get('heal_prob',25)if passive else 0))
 roster=read(ROOT/'content/field-global-registry.json');project=ex.text('project.godot')
 continuation=[]
 for role,name in enumerate(('globaldata','global','uiManager'),1):
  rows=[a for a in roster['autoloads']if a['name']==name];require(len(rows)==1,'Missing continuation autoload '+name);a=rows[0]
  require(a['kind']==2 and name+'="*res://'+a['path']+'"'in project,'Changed continuation project declaration '+name)
  for path,key in ((a['path'],'source_sha256'),(a['script'],'script_sha256')):
   ex.data(path);require(ex.sources[path]==a[key],'Changed continuation source '+path)
  continuation.append(dict(role=role,**a))
 require([x['ordinal']for x in continuation]==sorted(x['ordinal']for x in continuation),'Changed relative continuation constructor order')
 house_source='Maps/podunk/Nintens House.tscn';ex.data(house_source)
 ctor=read(ROOT/'content/native-global-data-constructor.json')
 roles={x['name']:x for x in ctor['declarations']}
 for a in assignments:a['member_kind']=roles[a['member']]['kind'];a['adapter']=roles[a['member']]['adapter']
 return dict(schema=2,format=1,capability=2,rules=1,family=FAMILY,commit=PIN,owner=cold['owner'],scene_id=stable('native-house-global-bridge'),sources=ex.sources,
  dependencies=dict(character=sha(CHARS),load=sha(COLD),definitions=sha(DEFS),session=sha(SESSION)),leader=dict(name=leader,declaration=row['id']),assignments=assignments,continuation_autoloads=continuation,continuation_scene=house_source,namespace_source=roster['scene'],
  status=dict(script='Scripts/Main/Status.gd',native='Node',stable_id=stable('Scripts/Main/Status.gd:continuation'),getter='get_ailment_data',ailment='ailment',turns='battle_turns',times='times_afflicted',probability='passive_heal_probability',healing_key='healing',passive_key='passive_heal',probability_key='heal_prob',times_default=1,turns_default=0,statuses=statuses),
  adaptation=dict(uid='Adopt existing native session UID and explicit doses without invoking Item.new defaults or init_from_serialized. Allocate a real source Reference in the target registry only.',rng='Actual target source constructors and cache Directory loops complete without source Ready or cold LOAD. No GodStorage or inactive-character Item constructors are required for this continuation. Existing actual target Item collisions are still rejected; adoption neither reads nor advances the played SourceRandom and preserves the live UID exclusion ledger.',playtime='The source integer counter receives complete elapsed seconds; the native fractional remainder stays owned by the bridge and is available for save/export.',scope='Current validated singleton House state, same actual Ninten Character and KEY/STORAGE constructor References. Explicit capability2 prepares NORMAL Inventory from checked source defaults, before any cold LOAD; source fields/status/Items/party adopt live House state without granting source Ready, GodStorage, cold-LOAD completion or arbitrary party/state admission. Inactive characters remain real constructor bodies, never cold-default gameplay substitutes.'),ready_admitted=False)

def load():
 d=read(IR);require(d==derive(),'House bridge source/dependencies changed');r=read(REVIEW)
 require(r['ir_sha256']==sha(IR) and r['sources']==d['sources'] and r['dependencies']==d['dependencies'] and r['ready_admitted']is False,'House bridge review stale');return d

def encode(d):
 b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def t(v):raw=v.encode();u(len(raw));b.extend(raw)
 for k in ('character','load','definitions','session'):b.extend(bytes.fromhex(d['dependencies'][k]))
 t(d['leader']['name']);u(d['leader']['declaration']);u(len(d['assignments']))
 for a in d['assignments']:u(a['role'],a['index'],a['kind'],a['member_kind'],a['adapter']);t(a['member']);t(a['key'])
 s=d['status'];u(s['stable_id'],s['times_default'],s['turns_default'])
 for k in ('script','native','getter','ailment','turns','times','probability','healing_key','passive_key','probability_key'):t(s[k])
 u(len(s['statuses']))
 for x in s['statuses']:t(x['id']);t(x['source']);b.extend(bytes.fromhex(x['source_sha256']));u(x['passive']);b.extend(struct.pack('<ii',x['priority'],x['probability']))
 u(len(d['sources']))
 for p,h in d['sources'].items():t(p);b.extend(bytes.fromhex(h))
 if d['capability']==2:
  t(d['continuation_scene']);t(d['namespace_source']);u(len(d['continuation_autoloads']))
  for a in d['continuation_autoloads']:
   u(a['role'],a['id'],a['kind'],a['ordinal'])
   for k in ('name','path','native_class','script'):t(a[k])
   b.extend(bytes.fromhex(a['source_sha256']));b.extend(bytes.fromhex(a['script_sha256']))
 struct.pack_into('<8s8I' ,b,0,b'ENCHGB01',1,128,len(b),zlib.crc32(b[128:]),FAMILY,d['capability'],1,d['scene_id']);b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['sources'][d['owner']]);b[92:124]=bytes.fromhex(sha(IR));return bytes(b)

def stage_files(root):
 d=load();relative=Path('data/house.encglobalbridge');raw=(Path(root)/relative).read_bytes();require(raw==encode(d),'Stale staged House bridge');return {relative:raw}

def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);a=p.parse_args()
 if a.action=='extract':d=derive();write(IR,d);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=d['sources'],dependencies=d['dependencies'],scope=d['adaptation'],ready_admitted=False));return
 d=load();raw=encode(d)
 if a.action=='compile':PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(raw)
 else:require(PACK.read_bytes()==raw,'Stale House bridge binary')
 print('Native House continuation:',len(raw),'bytes; no cold LOAD or UID/RNG replay')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,TypeError,OSError,struct.error)as e:sys.exit('HOUSE GLOBAL BRIDGE ERROR: '+str(e))
