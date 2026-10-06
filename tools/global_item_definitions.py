#!/usr/bin/env python3
"""All literal upstream Items for Item.new/GodStorage; gameplay actions pending."""
from __future__ import annotations
import argparse,csv,json,re,struct,sys,zlib,math
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,stable,read,sha,require
from tools.extract_battle_entry import Extractor
from tools.field_item_definitions import STATS,write
IR=ROOT/'content/native-global-item-definitions.json';REVIEW=ROOT/'reports/global-item-definitions/source-review.json';PACK=ROOT/'romfs/data/global.encfielditems'
EXTRA=['battle_action','basic_skill','repairable','repairiq','passive_skills','traits','enable_skill','dialog','map_for','affinity_multipliers','use_at_shop_dialog']
FN={'consume':1,'use':2,'equip':3,'transform':4}
def metadata(raw):
 out=[]
 for field,name in enumerate(EXTRA,1):
  if name not in raw:continue
  v=raw[name]
  if name=='battle_action':require(type(v)is dict and set(v)<=set('skill dialog target_type reusable'.split()),'Unknown battle action metadata')
  elif name in ('basic_skill','enable_skill','dialog','map_for','use_at_shop_dialog'):require(type(v)is str and v,'Unknown item text metadata')
  elif name=='repairable':require(type(v)is bool,'Unknown repair policy')
  elif name=='repairiq':require(type(v)is dict and all(type(k)is str and type(n)is int and n>=0 for k,n in v.items()),'Unknown repair requirements')
  elif name in ('passive_skills','traits'):require(type(v)is list and all(type(n)is str and n for n in v),'Unknown item trait metadata')
  elif name=='affinity_multipliers':require(type(v)is dict and all(type(k)is str and type(n)in(int,float)and math.isfinite(n)and n>=0 for k,n in v.items()),'Unknown item affinity metadata')
  vals=list(v.items())if type(v)is dict else list(enumerate(v))if type(v)is list else [('',v)]
  for key,value in vals:
   kind=3 if type(value)is bool else 2 if type(value)is int else 4 if type(value)is float else 1 if type(value)is str else 0
   require(kind,'Unknown metadata leaf')
   if name=='battle_action':require(kind==({'skill':1,'dialog':1,'target_type':2,'reusable':3}[key]),'Unknown battle metadata shape')
   if name=='affinity_multipliers':kind=4
   out.append(dict(field=field,key=str(key),kind=kind,text=value if kind==1 else '',integer=int(value)if kind in(2,3)else 0,real=float(value)if kind==4 else 0.0))
 return out

def extract():
 ex=Extractor(ROOT);item=ex.text('Scripts/global/Item.gd');inv=ex.text('Scripts/global/Inventory.gd');gd=ex.text('Scripts/global/globalData.gd');ex.text('LICENSE')
 require('self.doses = get_data().get("doses", 1)'in item and 'uid := get_uid(used_uids_tab)'in item,'Changed actual Item constructor')
 require('for item in globaldata.get_all_items():'in inv and 'var inv_item = Item.new(item)'in inv and '_items.append(inv_item)'in inv and 'sort_auto()'in inv and 'return _items.keys()'in gd,'Unknown GodStorage construction')
 require('elif item_data.get("battle_action", {}).get("skill"):'in inv and 'return tr(a.get_data()["sorting_name"]) < tr(b.get_data()["sorting_name"])'in inv,'Unknown sort programme')
 scores={k:(int(a)-int(b))*int(re.search(r'var STEP := (\d+)',inv)[1])for k,a,b in re.findall(r'var SCORE_CAT_(\w+)\s*:= \((\d+) - (\d+)\)\s*\* STEP',inv)}
 coeff={k.lower():int(v)for k,v in re.findall(r'Character\.(\w+): (\d+)',inv)};require(set(coeff)==set(STATS),'Unknown boost coefficients')
 slots=json.loads(re.search(r'const SLOTS := (\[[^\n]+)',inv)[1]);slotstep=int(re.search(r'var STEP_SLOT := (\d+)',inv)[1]);csvpath='Translations/TranslatedText/items - sheet.csv';csvtext=ex.text(csvpath);translations={r['key']:r for r in csv.DictReader(csvtext.splitlines())}
 files=sorted((ex.upstream/'Data/Items').rglob('*.yaml')) if hasattr(ex,'upstream')else sorted((ROOT/'upstream/MOTHER-Encore/Data/Items').rglob('*.yaml'))
 defs=[];required=set('name sorting_name description article keyitem cost value transform slot HPrecover PPrecover boost'.split());optional=set('actions doses can_use can_consume is_food target_all status_heals reusable'.split())|set(EXTRA)
 for path in files:
  source=path.relative_to(ROOT/'upstream/MOTHER-Encore').as_posix();raw=ex.yaml(source);require(required<=set(raw)<=required|optional,'Unknown item field '+source);require(set(raw['boost'])==set(STATS),'Unknown item boost '+source)
  actions=[]
  for a in raw.get('actions',[]):require(set(a)<=set('name function textfail'.split())and a['function']in FN,'Unknown action');actions.append(dict(function=FN[a['function']],name=a.get('name',''),textfail=a.get('textfail',''),pending=True))
  boost=sum(raw['boost'][s]*coeff[s]for s in STATS);fn=actions[0]['function']if actions else 0
  if raw['keyitem']:score=scores['KEY']
  elif raw.get('battle_action',{}).get('skill'):score=scores['BATTLE_ITEMS']
  elif raw.get('status_heals'):score=scores['STATUS_HEALS']
  elif any(a['function']==3 for a in actions):require(raw['slot']in slots,'Unknown equipment slot');score=scores['UNEQUIPPED']+(len(slots)-slots.index(raw['slot']))*slotstep+boost
  elif boost>0:score=scores['BOOSTS']+boost
  elif raw['PPrecover']>0:score=scores['PP_RECOVER']+raw['PPrecover']
  elif raw['HPrecover']>0:score=scores['HP_RECOVER']+raw['HPrecover']
  else:score=scores['CONSUMABLE'if fn==1 else 'USABLE'if fn==2 else 'OTHERS']
  tr=translations.get(raw['sorting_name'],{});sorttext={loc:tr.get(loc)or raw['sorting_name']for loc in('en','zh_CN')}
  defs.append(dict(id=stable(source),source=source,item_name=source[len('Data/Items/'):-5],name_key=raw['name'],sorting_key=raw['sorting_name'],description_key=raw['description'],article_key=raw['article'],keyitem=raw['keyitem'],doses=raw.get('doses',1),cost=raw['cost'],value=raw['value'],slot=raw['slot'],transform=raw['transform'],heal_hp=raw['HPrecover'],heal_pp=raw['PPrecover'],boost=[raw['boost'][s]for s in STATS],can_use=raw.get('can_use',[]),can_consume=raw.get('can_consume',[]),status_heals=raw.get('status_heals',[]),is_food=raw.get('is_food',False),target_all=raw.get('target_all',False),reusable=raw.get('reusable',False),actions=actions,legacy_domain=0,legacy_id=0,pending_metadata=metadata(raw),unequipped_sort_score=score,sorting_translations=sorttext))
 capacities=[int(re.search(r'const '+n+r' := (\d+)',inv)[1])for n in('_MAX_INVENTORY_SIZE','_MAX_STORAGE_SIZE','_MAX_STORAGE_SIZE_GOD')]
 d=dict(schema=1,kind='encore.global-item-definitions.source-ir',commit=PIN,definitions=defs,policy=dict(capacities=[capacities[0],0,capacities[1],capacities[2]],unbounded_mask=10,default_doses=1,dose_step=int(re.search(r'item.doses -= (\d+)',inv)[1]),dose_drop_threshold=int(re.search(r'if item.doses > (\d+):',inv)[1]),uid_protocol=7),sources=dict(sorted(ex.sources.items())),sort_policy=dict(scores=scores,coefficients=coeff,slots=slots,slot_step=slotstep),constructor_sources=['Scripts/global/Item.gd','Scripts/global/Inventory.gd','Scripts/global/globalData.gd'],god_storage=dict(id=stable('Scripts/global/globalData.gd#god_storage'),member=re.search(r'var (god_storage): Inventory',gd)[1],role=re.search(r'enum InvType \{([^}]+)',inv)[1].replace(' ','').split(',').index('STORAGE_GOD'),type_field=re.search(r'var (_type): int',inv)[1],items_field=re.search(r'var (_items) := \[\]',inv)[1]),scope='All literal definitions and complete Item.new/GodStorage construction only; optional gameplay metadata and actions remain pending; Directory insertion order must be supplied by the actual cache owner')
 validate(d);write(IR,d);write(REVIEW,dict(engine=dict(path='modules/gdscript/gdscript.cpp',tag='3.6.2-stable',sha256='4eef4fbaf0fa4a62c7a40b58b61d685a7fe3e425d67cc529c58f96bdccaf2561',native_reference_before_initializer=True,lines=[96,121,140,170]),schema=1,commit=PIN,ir_sha256=sha(IR),sources=d['sources'],scope=d['scope']));return d

def validate(d):
 require(set(d)==set('schema kind commit definitions policy sources sort_policy god_storage constructor_sources scope'.split()),'Unknown global Items IR field')
 require(d['schema']==1 and d['kind']=='encore.global-item-definitions.source-ir'and d['commit']==PIN,'Unknown global definition IR');ids=set();names=set()
 for r in d['definitions']:
  require(set(r)==set('id source item_name name_key sorting_key description_key article_key keyitem doses cost value slot transform heal_hp heal_pp boost can_use can_consume status_heals is_food target_all reusable actions legacy_domain legacy_id pending_metadata unequipped_sort_score sorting_translations'.split()),'Unknown global definition field')
  require(r['source']=='Data/Items/'+r['item_name']+'.yaml'and r['id']==stable(r['source'])and r['id']not in ids and r['item_name']not in names,'Duplicate/foreign item identity');ids.add(r['id']);names.add(r['item_name'])
  require(all(type(r[k])is bool for k in('keyitem','is_food','target_all','reusable'))and type(r['doses'])is int and 0<r['doses']<=65535 and all(type(r[k])is int and 0<=r[k]<=2147483647 for k in('cost','value','heal_hp','heal_pp'))and len(r['boost'])==7 and all(type(v)is int and -2147483648<=v<=2147483647 for v in r['boost']),'Invalid source item literal')
  require(not r['transform']or r['transform']in {x['item_name']for x in d['definitions']},'Unknown transform target')
  require(all(set(a)==set('function name textfail pending'.split())and a['function']in FN.values()and a['pending']is True and type(a['name'])is str and type(a['textfail'])is str for a in r['actions']),'Unknown executable action')
  require(r['legacy_domain']==r['legacy_id']==0 and type(r['unequipped_sort_score'])is int and -2147483648<=r['unequipped_sort_score']<=2147483647 and set(r['sorting_translations'])=={'en','zh_CN'}and all(type(t)is str and t for t in r['sorting_translations'].values()),'Invalid global constructor/sort domain')
  for v in r['pending_metadata']:
   require(set(v)==set('field key kind text integer real'.split())and type(v['field'])is int and 1<=v['field']<=len(EXTRA)and type(v['kind'])is int and 1<=v['kind']<=4 and type(v['key'])is str and type(v['text'])is str and type(v['integer'])is int and -2147483648<=v['integer']<=2147483647 and type(v['real'])in(int,float)and math.isfinite(v['real']),'Unknown pending source metadata')
 require(d['constructor_sources']==['Scripts/global/Item.gd','Scripts/global/Inventory.gd','Scripts/global/globalData.gd']and all(p in d['sources']for p in d['constructor_sources']),'Unknown constructor binding')
 require(set(d['god_storage'])==set('id member role type_field items_field'.split())and d['god_storage']['id']==stable('Scripts/global/globalData.gd#'+d['god_storage']['member'])and d['god_storage']['role']==3,'Invalid GodStorage identity')
 require(set(d['policy'])==set('capacities unbounded_mask default_doses dose_step dose_drop_threshold uid_protocol'.split())and len(d['policy']['capacities'])==4 and d['policy']['unbounded_mask']==10 and d['policy']['uid_protocol']==7,'Unknown global Items construction policy')
 require(len(ids)==93,'Incomplete reviewed all-items coverage');return d

def load():
 d=validate(read(IR));r=read(REVIEW);require(r['commit']==PIN and r['ir_sha256']==sha(IR)and r['sources']==d['sources'],'Global item review stale');ex=Extractor(ROOT)
 for p,h in d['sources'].items():ex.data(p);require(ex.sources[p]==h,'Changed source '+p)
 return d

def encode(d):
 validate(d);out=bytearray(64)
 def u(*v):out.extend(struct.pack('<'+'I'*len(v),*v))
 def i(*v):out.extend(struct.pack('<'+'i'*len(v),*v))
 def s(v):b=v.encode();u(len(b));out.extend(b)
 p=d['policy'];u(*p['capacities'],*(p[k]for k in('unbounded_mask','default_doses','dose_step','dose_drop_threshold','uid_protocol')));u(*([0]*6));g=d['god_storage'];u(g['id'],g['role']);s(g['member']);s(g['type_field']);s(g['items_field']);u(len(d['constructor_sources']));
 for source in d['constructor_sources']:s(source)
 for r in d['definitions']:
  u(r['id'],r['doses'],int(r['keyitem'])|int(r['is_food'])<<1|int(r['target_all'])<<2|int(r['reusable'])<<3,0,0);i(*(r[k]for k in('cost','value','heal_hp','heal_pp')),*r['boost'])
  for k in('source','item_name','name_key','sorting_key','description_key','article_key','slot','transform'):s(r[k])
  for k in('can_use','can_consume','status_heals'):
   u(len(r[k]))
   for v in r[k]:s(v)
  u(len(r['actions']))
  for a in r['actions']:u(a['function'],1);s(a['name']);s(a['textfail'])
  i(r['unequipped_sort_score']);u(len(r['sorting_translations']))
  for locale,text in r['sorting_translations'].items():s(locale);s(text)
  u(len(r['pending_metadata']))
  for v in r['pending_metadata']:u(v['field'],v['kind']);s(v['key']);s(v['text']);i(v['integer']);out.extend(struct.pack('<d',v['real']))
 u(len(d['sources']))
 for p,h in d['sources'].items():s(p);out.extend(bytes.fromhex(h))
 struct.pack_into('<8s6I20sIII',out,0,b'ENCFIT01',3,len(out),0,3,1,len(d['definitions']),bytes.fromhex(PIN),0,len(d['sources']),0);struct.pack_into('<I',out,16,zlib.crc32(out)&0xffffffff);return bytes(out)

def stage_files(source):
 raw=encode(load());p=Path('data/global.encfielditems');require((Path(source)/p).read_bytes()==raw,'Global Items binary stale');return {p:raw}
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile']);a=p.parse_args()
 if a.action=='extract':print('All source definitions',len(extract()['definitions']))
 else:PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(encode(load()));print('All source definitions binary',PACK.stat().st_size)
