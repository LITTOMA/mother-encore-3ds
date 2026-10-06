#!/usr/bin/env python3
"""Source-scoped Ninten/KEY/STORAGE inventory execution and save policy.

This is an inventory projection of ailment metadata, not admission of their
battle/overworld behaviours. Existing source UID consumers remain authoritative.
"""
from __future__ import annotations
import argparse,csv,io,json,math,re,struct,subprocess,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,stable,read,sha,require,write
from tools.extract_battle_entry import Extractor
from tools.field_item_definitions import load as definitions,STATS
IR=ROOT/'content/native-field-inventory.json'
REVIEW=ROOT/'reports/field-inventory/source-review.json'
PACK=ROOT/'romfs/data/podunk.encinventory'
ASSET=ROOT/'content/field-inventory-assets.json'
RECEIPT=ROOT/'content/asset-receipts/graphics/ui/field-inventory/source.json'

def extract():
 d=definitions();ex=Extractor(ROOT)
 for p,h in d['sources'].items():ex.data(p);require(ex.sources[p]==h,'Changed shared item source '+p)
 scripts={p:ex.text(p)for p in ('Scripts/global/Item.gd','Scripts/global/Inventory.gd','Scripts/global/Character.gd','Scripts/global/PartyMember.gd','Scripts/Main/Status.gd','Scripts/global/global.gd','Scripts/global/globalData.gd','Nodes/Ui/Inventory/ActionSelect.gd','Nodes/Ui/Inventory/TargetCharaSelect.gd','Scripts/UI/Inventory/InventoryUI.gd','Nodes/Ui/Inventory/InventoryUI.tscn','Scripts/global/text_tools.gd')}
 inv=scripts['Scripts/global/Inventory.gd'];pm=scripts['Scripts/global/PartyMember.gd'];char=scripts['Scripts/global/Character.gd'];target=scripts['Nodes/Ui/Inventory/TargetCharaSelect.gd'];action=scripts['Nodes/Ui/Inventory/ActionSelect.gd']
 checks=[(inv,'receiver.set_hp(receiver.get_hp() + int(item_data["HPrecover"]))'),(inv,'receiver.set_pp(receiver.get_pp() + int(item_data["PPrecover"]))'),(inv,'receiver.remove_status(status)'),(inv,'receiver.apply_boosts(item_data["boost"], performed_actions)'),(inv,'if !is_reusable:\n\t\treduce_or_drop_item(item)'),(pm,'_permanent_boosts[stat_name] = _permanent_boosts.get(stat_name, 0) + boosts[stat_name]'),(pm,'if !(is_unconscious() and stat == HP and new_value > 0):'),(pm,'elif !is_unconscious() and _hp == 0:'),(char,'HP: _hp = int(clamp(new_value, 0, get_max_hp()))'),(char,'PP: _pp = int(clamp(new_value, 0, get_max_pp()))'),(target,'yield(get_tree().create_timer(0.5), "timeout")'),(action,'emit_signal("exit_with_item", _current_item)'),(inv,'item.equipped = false'),(inv,'get_items()[item2_idx] = item1')]
 for text,snippet in checks:require(snippet in text,'Changed inventory source execution '+snippet)
 require(inv.index('receiver.remove_status(status)')<inv.index('receiver.apply_boosts(item_data["boost"]')<inv.index('if !is_reusable:'),'Changed cure/boost/dose source order')
 require('"stat_mods"' in char and 'combined_effect[stat] = effects[i].get(stat, 0)'in char and 'can_receive = !item_data.get("is_food", false)'in char,'Unknown status inventory projection')
 slots=json.loads(re.search(r'^const SLOTS := (\[[^\n]+\])',inv,re.M)[1]);cap=int(re.search(r'^const LEVEL_CAP := (\d+)',pm,re.M)[1]);table=re.search(r'^\tNINTEN: \{(.*?)^\t\},',pm,re.M|re.S)[1]
 targets={k.lower():json.loads(v)for k,v in re.findall(r'\b([A-Z]+):\s*(\[[^\]]+\])',table)};require(list(targets)==STATS,'Changed stat ordering')
 levels=[]
 for level in range(1,cap+1):
  row=[]
  for stat in STATS:
   a=targets[stat];t=min(level//10,len(a)-2);row.append(int(a[t]+(a[t+1]-a[t])*(level-t*10)/10.0))
  levels.append(row)
 policies=[];allstatuses=[]
 for path in sorted((ROOT/'upstream/MOTHER-Encore/Data/StatusAilments').glob('*.yaml')):
  name=path.stem;src='Data/StatusAilments/'+path.name;raw=ex.yaml(src);picked={}
  for case,effects in raw.get('effects_by_char',{}).items():
   require(case in ('any','party_member','field_party_member','enemy','boss','boss_layer_1','boss_layer_2'),'Unknown status selection case')
   if case in ('any','party_member','field_party_member'):picked.update(effects)
  mods=picked.get('stat_mods',{});require(not mods,'Unimplemented Ninten field stat multiplier')
  blocked=picked.get('cant_receive_item');blocks=[]
  if blocked:
   require(set(blocked)=={'type','message'},'Unknown field receive policy');types=blocked['type']if isinstance(blocked['type'],list)else[blocked['type']]
   require(all(t in ('food','status_heals','HPrecover','PPrecover')for t in types),'Unknown field receive item selector');blocks=[dict(type=t,message=blocked['message'])for t in types]
  heal=raw.get('healing',{});messages=raw.get('messages',{})
  allstatuses.append(dict(id=name,source=src,priority=raw.get('priority',0),persistent=heal.get('persistent',False),passive=heal.get('passive_heal',False),exclusive=raw.get('exclusive_status',False),unconscious=name==re.search(r'const AILMENT_UNCONSCIOUS := "([^"]+)"',scripts['Scripts/Main/Status.gd'])[1],heal_message=messages.get('heal_overworld',''),fail_message=messages.get('heal_overworld_fail',''),receive_blocks=blocks))
 for item in d['definitions']:
  raw=ex.yaml(item['source']);boost_order=[STATS.index(s)for s in raw['boost']];require(sorted(boost_order)==list(range(len(STATS))),'Unknown item boost keys')
  require(not raw.get('transform','')and not any(k in raw for k in ('dialog','map_for','target_unconscious','battle_action','passive_skills','stat_mods')),'Unknown field item execution '+item['item_name'])
  for sts in item['status_heals']:require(any(s['id']==sts for s in allstatuses),'Unreviewed status cure')
  policies.append(dict(definition=item['id'],source=item['source'],source_sha=ex.sources[item['source']],boost_order=boost_order,consume_allowed='can_consume'not in raw or 'ninten'in raw['can_consume'],use_allowed='can_use'not in raw or 'ninten'in raw['can_use'],actions=[dict(function=a['function'],name=a.get('name',''),fail=a.get('textfail','ACTION_RESULT_FAIL_ANY'))for a in raw.get('actions',[])]))
 save=ex.yaml('Data/save_new_game.yaml');require(save['party']==['ninten']and not save['ninten']['status']and not save.get('storage',[]),'Changed scoped initial owner state')
 owners=[dict(id=stable('Scripts/global/globalData.gd#'+n),role=r,name=n)for n,r in [('ninten',0),('key_items',1),('storage',2)]]
 initials=[]
 for name,rows in [('key_items',save['key_items']),('storage',save.get('storage',[])),('ninten',save['ninten']['inventory'])]:
  for row in rows:
   require(set(row)<={'item_name','equipped','doses','uid'}and'uid'not in row,'Unreviewed explicit initial UID')
   source='Data/Items/'+row['item_name']+'.yaml';definition=next(v for v in d['definitions']if v['source']==source)
   initials.append(dict(owner=next(o['id']for o in owners if o['name']==name),definition=definition['id'],equipped=row.get('equipped',False),doses=row.get('doses',1)))
 refresh=int(re.search(r'elif !is_unconscious\(\) and _hp == 0:\n\t\tset_hp\((\d+)\)',pm)[1]);delay=float(re.search(r'create_timer\((\d+(?:\.\d+)?)\)',target[target.index('func _process_messages'):])[1])
 labels=['INVENTORY_ACTION_EQUIP','INVENTORY_ACTION_UNEQUIP','INVENTORY_ACTION_SORT','INVENTORY_ACTION_DROP','INVENTORY_ACTION_TARGET','INVENTORY_ACTION_TARGET_ALL','INVENTORY_ACTION_USE_TARGET','INVENTORY_ACTION_SORT_HOW','INVENTORY_ACTION_SORT_MANUAL','INVENTORY_ACTION_SORT_AUTO','INVENTORY_ACTION_EQUIP_CONFIRM','INVENTORY_ACTION_DROP_CONFIRM','ACTION_RESULT_HP_UP','ACTION_RESULT_HP_MAX','ACTION_RESULT_PP_UP','ACTION_RESULT_PP_MAX','ACTION_RESULT_STAT_UP','ACTION_RESULT_FAIL_ANY']
 for item in d['definitions']:labels.extend([item['name_key'],item['sorting_key']])
 for p in policies:
  for a in p['actions']:labels.extend(v for v in (a['name'],a['fail'])if v)
 for sts in allstatuses:labels.extend(v for v in (sts['heal_message'],sts['fail_message'])if v);labels.extend(b['message']for b in sts['receive_blocks'])
 translations={}
 for src in ('Translations/TranslatedText/menus - sheet.csv','Translations/TranslatedText/items - sheet.csv'):
  for row in csv.DictReader(io.StringIO(ex.text(src))):
   if row['key'] in labels:translations[row['key']]=dict(key=row['key'],en=row['en'],zh=row['zh_CN'])
 sorting={v['sorting_key']for v in d['definitions']}
 for k in set(labels)-translations.keys():
  require(k in sorting,'Unmapped inventory source translation '+k);translations[k]=dict(key=k,en=k,zh=k)
 sounds=dict(eat='Audio/Sound effects/EB/eat.wav',equip='Audio/Sound effects/EB/equip.wav',clear='')
 # Source audioManager resolves equip through its concrete source binding.
 from tools.item_use import load as presentation
 geometry=presentation(ROOT);dependencies={'content/native-field-item-definitions.json':sha(ROOT/'content/native-field-item-definitions.json'),'content/native-item-use.json':sha(ROOT/'content/native-item-use.json')}
 for p,h in geometry['sources'].items():ex.data(p);require(ex.sources[p]==h,'Changed shared UI geometry source')
 audio=ex.text('Scripts/global/audioManager.gd');match=re.search(r'"equip"\s*:\s*(?:preload|load)\("res://([^\"]+)"\)',audio)
 require(match,'Unknown equip sound binding');sounds['equip']=match[1]
 sounds['clear']=re.search(r'"clear"\s*:\s*(?:preload|load)\("res://([^\"]+)"\)',audio)[1]
 for sound in sounds.values():ex.data(sound);ex.text(sound+'.import')
 highlight=ex.text('Nodes/Ui/HighlightLabel.tscn');ex.text('Nodes/Ui/HighlightLabel.gd');require('position = Vector2( -6, 6 )'in highlight and 'region_rect = Rect2( 2, 19, 8, 7 )'in highlight,'Unknown source equipped indicator')
 texture='Graphics/UI/Inventory/modifiers.png';ex.data(texture);texture_import=ex.text(texture+'.import');ex.text('Shaders/MenuFlavors.tres')
 require('res://Shaders/MenuFlavors.tres'in highlight and all(v in texture_import for v in ('flags/filter=false','flags/mipmaps=false','flags/repeat=0','process/premult_alpha=false')),'Changed equipped glyph material/filter source')
 weights=[int(re.search(r'Character\.'+k.upper()+r': (\d+)',inv)[1])for k in STATS]
 step=int(re.search(r'var STEP := (\d+)',inv)[1]);slot_step=int(re.search(r'var STEP_SLOT := (\d+)',inv)[1]);sort_scores=[]
 for k in ('HP_RECOVER','PP_RECOVER','STATUS_HEALS','CONSUMABLE','BATTLE_ITEMS','USABLE','OTHERS','BOOSTS','UNEQUIPPED','EQUIPPED','KEY'):
  m=re.search(r'var SCORE_CAT_'+k+r'\s*:= \((\d+) - (\d+)\)',inv);sort_scores.append((int(m[1])-int(m[2]))*step)
 goods_rows=int(re.search(r'const NB_ROWS_WITH_DESC := (\d+)',scripts['Scripts/UI/Inventory/InventoryUI.gd'])[1])
 result=dict(schema=1,kind='encore.field-inventory.source-ir',commit=PIN,owners=owners,slots=slots,stats=STATS,levels=levels,policies=policies,statuses=allstatuses,initial=initials,initial_level=save['ninten']['level'],initial_hp=save['ninten']['hp'],initial_pp=save['ninten']['pp'],initial_cash=save['cash'],refresh_hp=refresh,message_delay=delay,sounds=sounds,feedback=['ACTION_RESULT_HP_UP','ACTION_RESULT_HP_MAX','ACTION_RESULT_PP_UP','ACTION_RESULT_PP_MAX','ACTION_RESULT_STAT_UP','ACTION_RESULT_FAIL_ANY'],action_labels=['INVENTORY_ACTION_EQUIP','INVENTORY_ACTION_UNEQUIP','INVENTORY_ACTION_SORT','INVENTORY_ACTION_DROP'],texts=list(translations.values()),layouts=geometry['layouts'],parameters=geometry['parameters'],sources=dict(sorted(ex.sources.items())),dependencies=dependencies,semantics=['Ninten NORMAL, KEY and STORAGE are an independent owned domain; inactive PartyMember and god_storage remain unsupported','Normal/key/store actual relative LOAD order: key_items,storage,ninten; every saved item eagerly generates an unused fallback UID through the shared source clock/PCG ledger','Stable definition identifies source YAML; UID including zero remains source object identity; no RNG or generated UID ledger is saved','HP then PP then status heals then YAML-ordered permanent boosts then dose/drop; source remove_status HP0 refresh is retained','All actual ailment files have an inventory-only projection; no ailment battle/overworld execution is admitted by this resource','SourceUse emits exit_with_item; PhoneCard menu use does not spend a dose','Shop and item-definition consumers commit to this owning state only after re-deriving exact typed before/after mutations; callbacks alone do not admit rules'],unverified=['Manual cases not executed','Full scene/Main/Session bridge remains parent integration','Emulator/hardware performance unverified'])
 result.update(sort_policy=dict(weights=weights,slot_step=slot_step,scores=sort_scores),description_rows=goods_rows,equipped=dict(source=texture,region=[2,19,8,7],position=[-6,6],output='graphics/ui/field-inventory/equipped.t3x'))
 validate(result);write(IR,result);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=result['sources'],dependencies=dependencies,semantics=result['semantics'],unverified=result['unverified']));return result

def validate(d):
 require(set(d)==set('schema kind commit owners slots stats levels policies statuses initial initial_level initial_hp initial_pp initial_cash refresh_hp message_delay sounds feedback action_labels texts layouts parameters sources dependencies semantics unverified sort_policy description_rows equipped'.split()),'Unknown field inventory IR metadata')
 require(d['schema']==1 and d['kind']=='encore.field-inventory.source-ir'and d['commit']==PIN,'Unknown field inventory IR')
 require(len(d['owners'])==3 and [o['role']for o in d['owners']]==[0,1,2]and len({o['id']for o in d['owners']})==3,'Unknown scoped inventory owners')
 for o in d['owners']:require(o['id']==stable('Scripts/global/globalData.gd#'+o['name'])and o['name']in ('ninten','key_items','storage'),'Unknown source owner')
 require(d['stats']==STATS and len(set(d['slots']))==4 and d['levels']and all(len(r)==7 and all(type(v)is int and 0<=v<=2147483647 for v in r)for r in d['levels']),'Unknown stat/slot source policy')
 ids=set()
 for p in d['policies']:
  require(set(p)==set('definition source source_sha boost_order consume_allowed use_allowed actions'.split())and all(set(a)=={'function','name','fail'}for a in p['actions']),'Unknown inventory definition action metadata')
  require(p['definition']not in ids and p['definition']==stable(p['source'])and p['source_sha']==d['sources'][p['source']]and sorted(p['boost_order'])==list(range(7))and type(p['consume_allowed'])is bool and type(p['use_allowed'])is bool,'Unknown definition execution policy');ids.add(p['definition'])
  require(all(a['function']in ('consume','equip','use')for a in p['actions']),'Unknown inventory executable action')
 require(len(ids)==19 and len(d['statuses'])==15 and len({s['id']for s in d['statuses']})==15,'Incomplete reviewed scoped definitions/status projection')
 require(type(d['refresh_hp'])is int and 0<d['refresh_hp']<=2147483647 and math.isfinite(d['message_delay'])and 0<d['message_delay']<=60,'Invalid source refresh/message timer')
 for s in d['statuses']:require(set(s)==set('id source priority persistent passive exclusive unconscious heal_message fail_message receive_blocks'.split())and all(type(s[k])is bool for k in ('persistent','passive','exclusive','unconscious'))and type(s['priority'])is int and len(s['receive_blocks'])<=16 and all(set(b)=={'type','message'}and b['type']in ('food','status_heals','HPrecover','PPrecover')for b in s['receive_blocks']),'Unknown inventory status profile')
 require(set(d['sort_policy'])=={'weights','slot_step','scores'}and len(d['sort_policy']['weights'])==7 and len(d['sort_policy']['scores'])==11 and all(type(v)is int and 0<=v<=2147483647 for v in d['sort_policy']['weights']+d['sort_policy']['scores'])and type(d['sort_policy']['slot_step'])is int and 0<d['sort_policy']['slot_step']<=2147483647 and type(d['description_rows'])is int and 0<d['description_rows']<=1024,'Unknown inventory sort/grid policy')
 require(set(d['equipped'])=={'source','region','position','output'}and len(d['equipped']['region'])==4 and len(d['equipped']['position'])==2,'Unknown equipped glyph source fields')
 require(sum(s['unconscious']for s in d['statuses'])==1,'Unknown HP0 refresh identity');return d

def load():
 d=validate(read(IR));r=read(REVIEW);require(r['ir_sha256']==sha(IR)and r['sources']==d['sources']and r['dependencies']==d['dependencies']and r['commit']==PIN,'Stale field inventory semantic review');ex=Extractor(ROOT)
 for p,h in d['sources'].items():ex.data(p);require(ex.sources[p]==h,'Changed inventory source '+p)
 for p,h in d['dependencies'].items():require(sha(ROOT/p)==h,'Changed inventory binding '+p)
 return d

def encode(d):
 validate(d);out=bytearray(96)
 def u(*v):out.extend(struct.pack('<'+'I'*len(v),*v))
 def i(*v):out.extend(struct.pack('<'+'i'*len(v),*v))
 def q(v):out.extend(struct.pack('<q',v))
 def f(*v):out.extend(struct.pack('<'+'f'*len(v),*v))
 def s(v):b=v.encode();u(len(b));out.extend(b)
 u(len(d['owners']))
 for o in d['owners']:u(o['id'],o['role']);s(o['name'])
 for values in (d['slots'],d['stats']):u(len(values));[s(v)for v in values]
 u(len(d['levels']))
 for row in d['levels']:i(*row)
 u(d['refresh_hp']);out.extend(struct.pack('<d',d['message_delay']));u(d['initial_level']);q(d['initial_hp']);q(d['initial_pp']);q(d['initial_cash'])
 u(d['description_rows']);i(*d['sort_policy']['weights'],d['sort_policy']['slot_step'],*d['sort_policy']['scores'])
 asset=read(ASSET);require(asset['commit']==PIN and asset['ir_sha256']==sha(IR)and asset['source_sha256']==d['sources'][d['equipped']['source']],'Stale inventory equipped glyph receipt');s(d['equipped']['output']);u(asset['bytes'],*d['equipped']['region'][2:]);f(*d['equipped']['position']);out.extend(bytes.fromhex(asset['sha256']))
 u(len(d['initial']))
 for row in d['initial']:u(row['owner'],row['definition'],row['doses'],int(row['equipped']))
 u(len(d['policies']))
 funcs={'consume':1,'use':2,'equip':3}
 for p in d['policies']:
  u(p['definition']);s(p['source']);out.extend(bytes.fromhex(p['source_sha']));u(int(p['consume_allowed']),int(p['use_allowed']),*p['boost_order'],len(p['actions']))
  for a in p['actions']:u(funcs[a['function']]);s(a['name']);s(a['fail'])
 u(len(d['statuses']))
 selectors={'food':1,'status_heals':2,'HPrecover':3,'PPrecover':4}
 for p in d['statuses']:
  s(p['id']);s(p['source']);i(p['priority']);u(int(p['persistent'])|int(p['passive'])<<1|int(p['exclusive'])<<2|int(p['unconscious'])<<3);s(p['heal_message']);s(p['fail_message']);u(len(p['receive_blocks']))
  for b in p['receive_blocks']:u(selectors[b['type']]);s(b['message'])
 for sound in ('eat','equip','clear'):s(d['sounds'][sound])
 for key in d['feedback']+d['action_labels']:s(key)
 u(len(d['texts']))
 for t in d['texts']:
  for k in ('key','en','zh'):s(t[k])
 u(len(d['layouts']))
 for row in d['layouts']:s(row['role']);f(*row['rect'],*row['color'])
 u(len(d['parameters']))
 for name,row in d['parameters'].items():s(name);f(*row)
 u(len(d['sources']))
 for p,h in d['sources'].items():s(p);out.extend(bytes.fromhex(h))
 struct.pack_into('<8s6I20sIII',out,0,b'ENCFINV1',1,len(out),0,1,1,0x454e0043,bytes.fromhex(PIN),len(d['policies']),len(d['sources']),0);out[64:96]=bytes.fromhex(sha(IR));struct.pack_into('<I',out,16,zlib.crc32(out)&0xffffffff);return bytes(out)

def compile_pack():raw=encode(load());PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(raw);return raw
def stage_files(source):
 d=load();raw=encode(d);require((Path(source)/'data/podunk.encinventory').read_bytes()==raw,'Stale staged field inventory');asset=read(ASSET);p=Path(d['equipped']['output']);b=(Path(source)/p).read_bytes();require(len(b)==asset['bytes']and sha(Path(source)/p)==asset['sha256'],'Stale inventory glyph texture');return{Path('data/podunk.encinventory'):raw,p:b}
def glyph_requests():
 d=load();return{'Fonts/EBMain_la.tres':[''.join(t[k]for t in d['texts']for k in ('en','zh'))]}
def compile_assets(tex3ds):
 from PIL import Image
 d=load();v=d['equipped'];image=Image.open(ROOT/'upstream/MOTHER-Encore'/v['source']).convert('RGBA');x,y,w,h=v['region'];require(x>=0 and y>=0 and x+w<=image.width and y+h<=image.height,'Source equipped region extent')
 work=ROOT/'build/field-inventory';work.mkdir(parents=True,exist_ok=True);png=work/'equipped.png';image.crop((x,y,x+w,y+h)).save(png);output=ROOT/'romfs'/v['output'];output.parent.mkdir(parents=True,exist_ok=True)
 subprocess.run([str(tex3ds),'-f','rgba8888','-z','auto','-o',str(output),str(png)],check=True)
 asset=dict(schema=1,commit=PIN,ir_sha256=sha(IR),source=v['source'],source_sha256=d['sources'][v['source']],output=v['output'],sha256=sha(output),bytes=output.stat().st_size,region=v['region'],position=v['position'],nearest=True)
 write(ASSET,asset);write(RECEIPT,asset);return asset
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','assets']);p.add_argument('--tex3ds',type=Path);a=p.parse_args()
 try:
  if a.action=='extract':print('Field inventory source:',len(extract()['policies']),'definitions')
  elif a.action=='assets':print('Field inventory glyph:',compile_assets(a.tex3ds)['bytes'],'bytes')
  else:print('Field inventory:',len(compile_pack()),'bytes')
 except (ValueError,KeyError,TypeError,OSError,struct.error)as e:sys.exit('FIELD INVENTORY ERROR: '+str(e))
