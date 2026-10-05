#!/usr/bin/env python3
"""Checked scoped source Item definitions, ownership, grant and UID protocol."""
from __future__ import annotations
import argparse,json,re,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,SCENE,stable,read,sha,require
from tools.extract_battle_entry import Extractor
IR=ROOT/'content/native-field-item-definitions.json';REVIEW=ROOT/'reports/field-item-definitions/source-review.json';PACK=ROOT/'romfs/data/podunk.encfielditems'
STATS=['maxhp','maxpp','offense','defense','speed','iq','guts'];FUNCTIONS={'consume':1,'use':2,'equip':3}
def write(p,d):
 p.parent.mkdir(parents=True,exist_ok=True)
 with p.open('w',encoding='utf-8',newline='\n')as f:json.dump(d,f,ensure_ascii=False,indent=2);f.write('\n')
def extract():
 from tools.field_present import load as present
 from tools.field_dropped import load as dropped
 from tools.field_openable_door import load as doors
 from tools.field_payphone import load as phones
 from tools.basement_progression import load as basement
 packs=[present(),dropped(),doors(),phones(),basement()];paths=['content/native-field-present.json','content/native-field-dropped.json','content/podunk-openable-door.json','content/native-field-payphone.json','content/native-basement-progression.json'];ex=Extractor(ROOT)
 for d in packs:
  require(d['commit']==PIN,'Foreign source definition binding')
  for p,h in d['sources'].items():ex.data(p);require(ex.sources[p]==h,'Changed scoped item source '+p)
 item=ex.text('Scripts/global/Item.gd');inv=ex.text('Scripts/global/Inventory.gd');globaldata=ex.text('Scripts/global/globalData.gd');holder=ex.text('Scripts/Main/ItemHolder.gd');dialogue=ex.text('Scripts/UI/DialogueBox.gd');ex.text('LICENSE');ex.text('Translations/TranslatedText/items - sheet.csv')
 require('var item_data: Dictionary = _items.get(item_name, {})'in globaldata and 'if item_data: item_data["id"] = item_name'in globaldata,'Unknown item definition merge/default mechanism')
 require('uid := get_uid(used_uids_tab)'in item and 'randomize()\n\tvar new_uid = randi()\n\twhile(new_uid in used_uids):\n\t\tnew_uid = randi()\n\tused_uids.append(new_uid)'in item,'Unknown Item UID/reseed/generated-ledger sequence')
 require('item.get("doses", 1), int(item.get("uid", Item.get_uid(Item.used_uids_tab)))'in inv,'Unknown eager saved UID fallback')
 default_doses=int(re.search(r'self.doses = get_data\(\).get\("doses", (\d+)\)',item)[1]);step=int(re.search(r'item.doses -= (\d+)',inv)[1]);threshold=int(re.search(r'if item.doses > (\d+):',inv)[1]);require(step==threshold,'Unknown source dose drop boundary')
 require('var item := Item.new(item_name)\n\tvar result := add_item(item)'in inv and '_items.append(item)'in inv and 'item.equipped = false'in inv and 'return globaldata.key_items.add_item_by_name(item_name)'in inv and 'for member in global.party:\n\t\t\tvar inv = member.inv\n\t\t\tif !inv.is_full()'in inv,'Unknown item creation/first available owner order')
 require('global.item = Item.new(item)'in holder and 'global.item = Inventory.add_item_available(item)'in holder,'Unknown holder full transient item allocation')
 capacities=[int(re.search(r'const '+n+r' := (\d+)',inv)[1])for n in ('_MAX_INVENTORY_SIZE','_MAX_STORAGE_SIZE','_MAX_STORAGE_SIZE_GOD')];require('elif _type == InvType.KEY:\n\t\treturn 0'in inv and 'if _type == InvType.STORAGE_GOD:\n\t\treturn false'in inv,'Unknown unlimited inventory source')
 baseline=read(ROOT/'content/native-items.json');dependencies={p:sha(ROOT/p)for p in paths+['content/native-items.json']};names={x['source']for x in baseline['definitions']};bindings=[]
 def bind(kind,obj,scene,node,name,operation,program='',label=''):
  if name:names.add(name)
  bindings.append(dict(kind=kind,object_id=obj,scene=scene,node=node,item=name,operation=operation,program=program,label=label))
 for r in packs[0]['holders']:bind(1,r['id'],SCENE,r['node'],r['item'],1)
 for r in packs[1]['bindings']:bind(2,r['id'],SCENE,r['node'],r['item'],1)
 for r in packs[2]['records']:bind(3,r['id'],SCENE,r['node'],r['key'],2)
 for r in packs[3]['records']:bind(4,r['id'],SCENE,r['node'],r['item'],3)
 b=packs[4];p=b['present'];bind(1,p['stable_id'],p['scene'],p['node'],next(k['source']for k in b['key_items']if k['id']==p['key_id']),1)
 for program in b['programs']:
  for c in program['commands']:
   if c['kind']=='GrantKeyItem':
    name=next(k['source']for k in b['key_items']if k['id']==c['key_id']);source='Data/Dialogue/'+program['identity']+'.yaml';actual=ex.yaml(source);require(actual[c['label']]['item']==name,'Source typed grant mismatch');bind(5,stable(source+'#'+c['label']),SCENE,b['mick']['node'],name,4,program['identity'],c['label'])
 initial=ex.yaml('Data/save_new_game.yaml')
 for n in initial['key_items']:bind(6,stable('Data/save_new_game.yaml/key_items/'+n['item_name']),'','key_items',n['item_name'],5)
 for k in b['key_items']:names.add(k['source'])
 defs=[]
 for name in sorted(names):
  source='Data/Items/'+name+'.yaml';raw=ex.yaml(source);required=set('name sorting_name description article keyitem cost value transform slot HPrecover PPrecover boost'.split());optional=set('actions doses can_use can_consume is_food target_all status_heals reusable'.split());require(required<=set(raw)<=required|optional,'Unknown item field '+name);require(set(raw['boost'])==set(STATS),'Unknown item stat tuning '+name)
  actions=[]
  for a in raw.get('actions',[]):require(set(a)<=set('name function textfail'.split())and a['function']in FUNCTIONS,'Unknown typed action '+name);actions.append(dict(function=FUNCTIONS[a['function']],name=a.get('name',''),textfail=a.get('textfail',''),pending=True))
  require(raw['transform']=='','Unreviewed source transform definition '+name)
  old=next((d for d in baseline['definitions']if d['source']==name),None);key=next((k for k in b['key_items']if k['source']==name),None)
  defs.append(dict(id=stable(source),source=source,item_name=name,name_key=raw['name'],sorting_key=raw['sorting_name'],description_key=raw['description'],article_key=raw['article'],keyitem=raw['keyitem'],doses=raw.get('doses',default_doses),cost=raw['cost'],value=raw['value'],slot=raw['slot'],transform=raw['transform'],heal_hp=raw['HPrecover'],heal_pp=raw['PPrecover'],boost=[raw['boost'][s]for s in STATS],can_use=raw.get('can_use',[]),can_consume=raw.get('can_consume',[]),status_heals=raw.get('status_heals',[]),is_food=raw.get('is_food',False),target_all=raw.get('target_all',False),reusable=raw.get('reusable',False),actions=actions,legacy_domain=1 if old else 2 if key else 0,legacy_id=old['id']if old else key['id']if key else 0))
 lookup={d['item_name']:d['id']for d in defs}
 for binding in bindings:binding['definition']=lookup.get(binding.pop('item'),0)
 d=dict(schema=1,kind='encore.field-item-definitions.source-ir',commit=PIN,definitions=defs,bindings=bindings,policy=dict(capacities=[capacities[0],0,capacities[1],capacities[2]],unbounded_mask=10,default_doses=default_doses,dose_step=step,dose_drop_threshold=threshold,uid_protocol=7),sources=dict(sorted(ex.sources.items())),dependencies=dependencies,semantics=['Actual 17 YAML definitions: literal content and typed pending actions are not replaced with AsthmaSpray','ItemHolder success routes key inventory or first party inventory with space; full inventory creates a transient Item/global.item UID','Every default construction reseeds the shared Godot PCG using source clock expression; generated-only UID ledger persists across scenes; zero is a valid uint32 draw','Saved UID eager fallback uses existing checked LOAD implementation; explicit UID construction does not generate another default','Ownership and transfer retain original UID and doses; source removal clears equipped; party/key lookups exclude storage','Mick key grant is bound to actual woof_key phrase4; flags remain the original programme responsibility'],unverified=['Manual tests not executed','Session/menu/Host integration pending','Emulator and hardware unverified'])
 validate(d);write(IR,d);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=d['sources'],dependencies=dependencies,semantics=d['semantics'],unverified=d['unverified']));return d
def validate(d):
 require(set(d)==set('schema kind commit definitions bindings policy sources dependencies semantics unverified'.split())and d['schema']==1 and d['kind']=='encore.field-item-definitions.source-ir'and d['commit']==PIN,'Unknown item definition IR')
 require(set(d['policy'])==set('capacities unbounded_mask default_doses dose_step dose_drop_threshold uid_protocol'.split())and len(d['policy']['capacities'])==4 and all(type(v)is int and 0<=v<=100000 for v in d['policy']['capacities'])and d['policy']['unbounded_mask']==10 and d['policy']['uid_protocol']==7 and 0<d['policy']['default_doses']<=65535 and 0<d['policy']['dose_step']==d['policy']['dose_drop_threshold']<=65535,'Unknown item ownership/UID protocol')
 ids=set();names=set()
 fields=set('id source item_name name_key sorting_key description_key article_key keyitem doses cost value slot transform heal_hp heal_pp boost can_use can_consume status_heals is_food target_all reusable actions legacy_domain legacy_id'.split())
 for r in d['definitions']:
  require(set(r)==fields and r['id']==stable(r['source'])and r['id']not in ids and r['item_name']not in names,'Unknown/duplicate item definition');ids.add(r['id']);names.add(r['item_name']);require(r['source']=='Data/Items/'+r['item_name']+'.yaml'and all(type(r[k])is bool for k in ('keyitem','is_food','target_all','reusable'))and type(r['doses'])is int and 0<r['doses']<=65535 and all(type(r[k])is int and 0<=r[k]<=2147483647 for k in ('cost','value','heal_hp','heal_pp'))and len(r['boost'])==7 and all(type(v)is int and -2147483648<=v<=2147483647 for v in r['boost'])and r['legacy_domain']in range(3)and (r['legacy_id']>0 if r['legacy_domain']else r['legacy_id']==0),'Invalid item content fields')
  require(all(type(r[k])is str and r[k] and len(r[k].encode())<=8192 for k in ('source','item_name','name_key','sorting_key','description_key','article_key'))and type(r['slot'])is str and r['transform']=='','Invalid item source text/unsupported transform')
  for k in ('can_use','can_consume','status_heals'):require(type(r[k])is list and len(r[k])<=256 and all(type(v)is str and v for v in r[k])and len(set(r[k]))==len(r[k]),'Invalid item typed list')
  for a in r['actions']:require(set(a)=={'function','name','textfail','pending'}and a['function']in FUNCTIONS.values()and a['pending']is True and type(a['name'])is str and type(a['textfail'])is str,'Unknown item action implementation')
 seen=set()
 for b in d['bindings']:
  require(set(b)==set('kind object_id scene node definition operation program label'.split())and type(b['kind'])is int and b['kind']in range(1,7)and type(b['object_id'])is int and 0<b['object_id']<=0xffffffff and (b['kind'],b['object_id'],b['scene'])not in seen and (b['definition']in ids or b['definition']==0 and b['kind']==3)and b['operation']==(1 if b['kind']<3 else b['kind']-1),'Invalid scoped item binding');seen.add((b['kind'],b['object_id'],b['scene']))
  require(type(b['node'])is str and b['node'] and type(b['scene'])is str and bool(b['scene'])==(b['kind']!=6)and all(type(b[k])is str for k in ('program','label')),'Invalid item source node/scene')
  require(bool(b['program'])==bool(b['label'])==(b['kind']==5),'Invalid typed grant phrase binding')
  if b['kind']==5:require(next(x for x in d['definitions']if x['id']==b['definition'])['keyitem'],'Ordinary item not reviewed as typed key grant')
 require(len(d['definitions'])==17 and [sum(b['kind']==k for b in d['bindings'])for k in range(1,7)]==[17,3,10,4,1,1],'Incomplete scoped item mechanism coverage');return d
def load():
 d=validate(read(IR));r=read(REVIEW);require(r['ir_sha256']==sha(IR)and r['commit']==PIN and r['sources']==d['sources']and r['dependencies']==d['dependencies'],'Item semantic review stale');ex=Extractor(ROOT)
 for p,h in d['sources'].items():ex.data(p);require(ex.sources[p]==h,'Changed source item '+p)
 for p,h in d['dependencies'].items():require(sha(ROOT/p)==h,'Changed source item binding '+p)
 return d
def encode(d):
 validate(d);out=bytearray(64)
 def u(*v):out.extend(struct.pack('<'+'I'*len(v),*v))
 def signed(*v):out.extend(struct.pack('<'+'i'*len(v),*v))
 def s(v):b=v.encode();u(len(b));out.extend(b)
 p=d['policy'];u(*p['capacities'],*(p[k]for k in ('unbounded_mask','default_doses','dose_step','dose_drop_threshold','uid_protocol')))
 u(*(sum(b['kind']==k for b in d['bindings'])for k in range(1,7)))
 for r in d['definitions']:
  u(r['id'],r['doses'],int(r['keyitem'])|int(r['is_food'])<<1|int(r['target_all'])<<2|int(r['reusable'])<<3,r['legacy_domain'],r['legacy_id']);signed(*(r[k]for k in ('cost','value','heal_hp','heal_pp')),*r['boost'])
  for k in ('source','item_name','name_key','sorting_key','description_key','article_key','slot','transform'):s(r[k])
  for k in ('can_use','can_consume','status_heals'):
   u(len(r[k]))
   for value in r[k]:s(value)
  u(len(r['actions']))
  for a in r['actions']:u(a['function'],int(a['pending']));s(a['name']);s(a['textfail'])
 for b in d['bindings']:
  u(*(b[k]for k in ('kind','object_id','definition','operation')))
  for k in ('scene','node','program','label'):s(b[k])
 u(len(d['sources']))
 for p,h in d['sources'].items():s(p);out.extend(bytes.fromhex(h))
 struct.pack_into('<8s6I20sIII',out,0,b'ENCFIT01',2,len(out),0,2,1,len(d['definitions']),bytes.fromhex(PIN),len(d['bindings']),len(d['sources']),0);struct.pack_into('<I',out,16,zlib.crc32(out)&0xffffffff);return bytes(out)
def compile_pack():d=load();raw=encode(d);PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(raw);return raw
def stage_files(source):
 raw=encode(load());require((Path(source)/'data/podunk.encfielditems').read_bytes()==raw,'Stale staged item definitions');return{Path('data/podunk.encfielditems'):raw}
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile']);a=p.parse_args()
 try:
  if a.action=='extract':d=extract();print(len(d['definitions']),'actual source item definitions;',len(d['bindings']),'source bindings')
  else:print('Field item definitions:',len(compile_pack()),'bytes')
 except(ValueError,KeyError,TypeError,OSError,struct.error)as e:sys.exit('FIELD ITEM DEFINITIONS ERROR: '+str(e))
