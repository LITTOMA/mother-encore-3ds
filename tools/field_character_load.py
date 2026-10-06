#!/usr/bin/env python3
"""The actual cold save_default eight-character LOAD, not new-game content."""
import argparse, copy, json, re, struct, sys, zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.extract_battle_entry import Extractor
from tools.podunk_scene import PIN,read,write,sha,require,stable
IR=ROOT/'content/native-field-character-load.json'
REVIEW=ROOT/'reports/field-character-load/source-review.json'
PACK=ROOT/'romfs/data/global.enccharacterload'
FAMILY=0x454e0050
SOURCES=['Scripts/global/global.gd','Scripts/global/globalData.gd','Scripts/global/Character.gd','Scripts/global/PartyMember.gd','Scripts/global/PartyNPC.gd','Scripts/global/Inventory.gd','Scripts/global/Item.gd','Scripts/Main/Status.gd','Scripts/UI/Battle/EnemySkill.gd','Data/save_default.yaml','Data/save_overrides.yaml']

def merge(a,b):
 for k,v in b.items():
  if k in a and isinstance(a[k],dict) and isinstance(v,dict):merge(a[k],v)
  elif k in a and isinstance(a[k],list) and isinstance(v,list):
   if not v or (a[k] and isinstance(a[k][0],(dict,list))):a[k]=copy.deepcopy(v)
   else:
    for x in v:
     if x not in a[k]:a[k].append(copy.deepcopy(x))
  else:a[k]=copy.deepcopy(v)
 return a

def declaration_defaults(source):
 rows=[]
 for line in source.split('func ',1)[0].splitlines():
  if not line.startswith('var '):continue
  line=re.sub(r'\s+#.*$','',line)
  m=re.fullmatch(r'var (\w+)(?::\s*(String|int|bool|Inventory))?(?:\s*(?::=|=)\s*(.*?))?(?:\s+setget\s+(.*))?',line.strip())
  require(m is not None,'Unknown source constructor declaration '+line)
  name,typed,expr,getter=m.groups();expr=(expr or '').strip()
  if not expr:
   require(typed in ('String','int','bool','Inventory'),'Unknown default type '+line)
   kind,value={'String':(1,''),'int':(2,0),'Inventory':(5,''),'bool':(6,0)}[typed]
  elif expr in ('[]','{}','{ }','false'):
   kind,value={'[]':(3,''),'{}':(4,''),'{ }':(4,''),'false':(6,0)}[expr]
  elif typed=='int' and expr=='0':kind,value=2,0
  else:raise ValueError('Unknown literal constructor '+line)
  rows.append(dict(name=name,kind=kind,value=value))
 return rows

def source_bindings(src,constants,stats):
 ch,pm,npc,inv,enemy=(src[SOURCES[n]]for n in (2,3,4,5,8))
 def assignment(source,key):
  found=re.findall(r'^\t+(_\w+) = dict(?:\["'+re.escape(key)+r'"\]|\.get\("'+re.escape(key)+r'"[,\)])',source,re.M)
  require(len(set(found))==1,'Missing source field mapping '+key);return found[0]
 b={k:assignment(ch,k)for k in ('name','level','exp','hp','pp')}
 b['status']=re.search(r'^\t\t(_\w+) = loaded_sts',ch,re.M)[1]
 b.update({k:assignment(pm,key)for k,key in [('nickname','nickname'),('learned_skills','learnedSkills'),('permanent_boosts','permanent_boosts'),('affinities','affinity_multipliers')]})
 b['inventory']=re.search(r'^\t(_\w+) = Inventory.new',pm,re.M)[1]
 alias=re.search(r'^var (\w+): Inventory setget ,(\w+)$',pm,re.M)
 b['inventory_getter'],b['inventory_getter_method']=alias.groups()
 b['untargetable']=assignment(npc,'untargetable');b['npc_skills']=re.search(r'^\t(_\w+) = \[\]',npc,re.M)[1]
 b['stat_fields']=[assignment(ch,key)for key in stats]
 b.update({k:SOURCES[n]for k,n in [('character_script',2),('member_script',3),('npc_script',4),('inventory_script',5),('enemy_skill_script',8)]})
 defaults={k:declaration_defaults(src[SOURCES[n]])for k,n in [('character_defaults',2),('member_defaults',3),('npc_defaults',4),('inventory_defaults',5),('enemy_skill_defaults',8)]};b.update(defaults)
 b['inventory_type_field']=re.search(r'^\t(_\w+) = type$',inv,re.M)[1]
 b['inventory_items_field']=re.search(r'^\t(_\w+) = inv_content$',inv,re.M)[1]
 enum=re.search(r'enum InvType \{([^}]+)\}',inv)[1].replace(' ','').split(',');b['normal_inventory_type']=enum.index('NORMAL')
 names=re.findall(r'^var (\w+)(?::|\s)',enemy,re.M);require(len(names)==4,'EnemySkill complete declaration changed')
 for k,v in zip(('enemy_id_field','enemy_weight_field','enemy_cooldown_field','enemy_remaining_field'),names):b[k]=v
 ed=[]
 for member,key,fallback in re.findall(r'^\t(\w+) = data.get\("(\w+)", ([^\n]+)\)',enemy,re.M):
  value=json.loads(fallback);ed.append(dict(name=member,kind=1 if type(value)is str else 2,value=value))
 require(len(ed)==3 and [v['name']for v in ed]==names[:3],'Unknown EnemySkill constructor assignments');b['enemy_constructor_defaults']=ed
 template=re.search(r'return "([^"\n]+)" \+ _name.to_upper\(\) \+ "([^"\n]+)"',npc);require(template,'Unknown NPC nickname template');b['npc_nickname_prefix'],b['npc_nickname_suffix']=template.groups()
 # Typed source order, admitted against exact original statement bodies.
 member_lines=['_inventory = Inventory.new(Inventory.InvType.NORMAL, dict.get("inventory", []))','_name = dict["name"]','_set_exp(max(dict["exp"], _level_to_exp(dict["level"])) as int, false)','_status_init_from_dict(dict)','_nickname = dict.get("nickname", _name)','_learned_skills = dict.get("learnedSkills", [])','_permanent_boosts = dict.get("permanent_boosts", {})','_affinity_multipliers = dict.get("affinity_multipliers", {})','set_hp(dict["hp"])','set_pp(dict["pp"])','_learned_skills.sort_custom(self, "_sort_skills")']
 body=pm.split('func init_from_dict(dict: Dictionary):',1)[1].split('\nfunc ',1)[0];actual=[x.strip()for x in body.splitlines()if x.strip()and not x.strip().startswith('#')];require(actual==member_lines,'Unknown PartyMember LOAD cursor')
 b['member_load_order']=list(range(1,12))
 character_lines=['_name = dict["name"]','_level = dict["level"]','_exp = dict["exp"]','_hp = dict.get("hp", 1)','_maxhp = dict["maxhp"]','_pp = dict.get("pp", 0)','_maxpp = dict["maxpp"]','_offense = dict["offense"]','_defense = dict["defense"]','_speed = dict["speed"]','_iq = dict["iq"]','_guts = dict["guts"]','_status_init_from_dict(dict)']
 body=ch.split('func init_from_dict(dict: Dictionary):',1)[1].split('\nfunc ',1)[0];actual=[x.strip()for x in body.splitlines()if x.strip()and not x.strip().startswith('#')];require(actual==character_lines,'Unknown Character LOAD cursor')
 b['character_load_order']=[2,12,13,14,15,16,17,18,19,20,21,22,4]
 require('\tif dict:\n\t\t.init_from_dict(dict)\n\t\t_untargetable = dict.get("untargetable", false)\n\t\t_set_skills(dict.get("skills", []))' in npc and '_skills = []\n\tfor skill in skills:\n\t\tif skill is Dictionary and globaldata.does_battle_skill_exist(skill.get("skill")):\n\t\t\t_skills.append(EnemySkill.new(skill))'in npc,'Unknown NPC skill construction cursor')
 b['npc_load_order']=[23,24,25,26,27]
 require('Item.new(item["item_name"], item.get("equipped", false),\\\n\t\t\t\titem.get("doses", 1), int(item.get("uid", Item.get_uid(Item.used_uids_tab))))'in inv,'Unknown source serialized argument evaluation')
 b['item_argument_order']=list(range(1,15))
 item=src[SOURCES[6]];require(not re.search(r'^extends ',item,re.M)and not re.search(r'^extends ',enemy,re.M),'Changed implicit Reference inheritance')
 b['item_script']=SOURCES[6];b['item_native']=b['enemy_skill_native']='Reference';b['enemy_skill_id']=stable(SOURCES[8]);b['item_constructor_id']=stable(SOURCES[6]);b['item_defaults']=declaration_defaults(item)
 for semantic,member in zip(('item_name_field','item_uid_field','item_equipped_field','item_doses_field'),b['item_defaults']):b[semantic]=member['name']
 return b

def derive():
 ex=Extractor(ROOT);s={p:ex.text(p)for p in SOURCES};pm=s[SOURCES[3]];ch=s[SOURCES[2]];gd=s[SOURCES[1]]
 require('_load_dict_to_game(save_data, false)' in s[SOURCES[0]] and 'globaldata.get_json_data(SAVE_DEFAULT_PATH)'in s[SOURCES[0]],'Changed default LOAD')
 require('_set_exp(max(dict["exp"], _level_to_exp(dict["level"])) as int, false)'in pm and 'emit_signal("stat_changed", stat, new_value, max_value)'in ch,'Changed stat execution')
 require('var loaded_sts := []'in ch and '_status = loaded_sts'in ch,'Changed Status replacement')
 raw=ex.yaml(SOURCES[-2]);merged=merge(copy.deepcopy(raw),ex.yaml(SOURCES[-1]));block=gd.split('var characters := {',1)[1].split('\n}',1)[0]
 names=re.findall(r'(PartyMember|PartyNPC)\.(\w+): \w+\.new\(\)',block);require(len(names)==8,'Incomplete eight owners')
 constants={k:v for k,v in re.findall(r'^const (\w+) := "([^"]+)"',ch,re.M)}
 statorder=[constants[x]for x in re.search(r'const BOOSTABLE_STATS := \[([^]]+)\]',ch)[1].replace(' ','').split(',')]
 tables=pm.split('const PLAYER_STAT_TARGET_TABLE := {',1)[1].split('\n}\n',1)[0]
 skills=re.findall(r'"(\w+)"', '\n'.join(x.split('#')[0]for x in pm.split('const SKILLS_ORDER := [',1)[1].split('\n]',1)[0].splitlines()))
 rows=[]
 for cls,key in names:
  name=re.search(r'^const '+key+r' := "([^"]+)"',s['Scripts/global/'+cls+'.gd'],re.M)[1];d=merged[name]
  require('status'in d and d['status']==[],'Cold source status no longer empty; new Status capability required')
  member=cls=='PartyMember';row=dict(id=stable(SOURCES[1]+'#characters.'+name),name=name,role=0 if member else 1,display_name=d['name'],nickname=d.get('nickname',d['name']),level=d['level'],exp=d['exp'],hp=d.get('hp',1),pp=d.get('pp',0),status=[],inventory=[],skills=d.get('learnedSkills',[])if member else[],npc_skills=[],untargetable=d.get('untargetable',False),affinities=[[k,v]for k,v in d.get('affinity_multipliers',{}).items()]if member else[],permanent=[d.get('permanent_boosts',{}).get(k,0)for k in statorder]if member else[0]*len(statorder),permanent_fields=list(d.get('permanent_boosts',{}).items())if member else[],targets=[],npc_stats=[])
  for item in d.get('inventory',[]):
   require(set(item)<=set('item_name equipped doses uid'.split())and type(item['item_name'])is str,'Unknown serialized Item')
   row['inventory'].append(dict(name=item['item_name'],equipped=item.get('equipped',False),doses=item.get('doses',1),has_uid='uid'in item,uid=item.get('uid',0)))
  if member:
   text=re.search(r'\b'+key+r': \{(.*?)\n\t\}',tables,re.S)[1]
   t=[(constants[k],json.loads(a))for k,a in re.findall(r'(\w+):\s*(\[[^]]+\])',text)]
   require([k for k,_ in t]==statorder,'Unknown stat target insertion order');row['targets']=[v for _,v in t]
  else:
   row['npc_stats']=[d[k]for k in statorder]
   for skill in d.get('skills',[]):
    require(set(skill)<=set('skill weight cooldown'.split()),'Unknown EnemySkill fields');row['npc_skills'].append(dict(skill=skill.get('skill','bash'),weight=skill.get('weight',0),cooldown=skill.get('cooldown',0)))
  rows.append(row)
 cap=int(re.search(r'const LEVEL_CAP := (\d+)',pm)[1]);slots=json.loads(re.search(r'const SLOTS := (\[[^\n]+)',s[SOURCES[5]])[1])
 return dict(schema=2,format=2,capability=2,rules=1,bindings=source_bindings(s,constants,statorder),kind='encore.field-character-load.source-ir',commit=PIN,family=FAMILY,rows=rows,stat_order=statorder,hp=constants['HP'],pp=constants['PP'],source_slots=slots,npc_skill_getter=re.search(r'globaldata\.(does_battle_skill_exist)\(',s[SOURCES[4]])[1],skill_order=skills,exp_floors=[0 if n==1 else int(n*n*(n+1)*.75)for n in range(1,cap+1)],sources=dict(sorted(ex.sources.items())),cold_status_only=True,whole_global_ready=False,whole_load=False,source_save=SOURCES[-2])

BINDING_TEXT_FIELDS='name level exp status hp pp nickname inventory inventory_getter learned_skills permanent_boosts affinities untargetable npc_skills character_script member_script npc_script inventory_script inventory_type_field inventory_items_field inventory_getter_method enemy_skill_script enemy_id_field enemy_weight_field enemy_cooldown_field enemy_remaining_field item_script item_native enemy_skill_native item_name_field item_uid_field item_equipped_field item_doses_field npc_nickname_prefix npc_nickname_suffix'.split()
BINDING_DEFAULTS='character_defaults member_defaults npc_defaults inventory_defaults enemy_skill_defaults item_defaults enemy_constructor_defaults'.split()
BINDING_STEPS='member_load_order character_load_order npc_load_order item_argument_order'.split()

def validate_bindings(b):
 expected=set(BINDING_TEXT_FIELDS+BINDING_DEFAULTS+BINDING_STEPS+['stat_fields','normal_inventory_type','enemy_skill_id','item_constructor_id'])
 require(set(b)==expected and all(type(b[k])is str and b[k]for k in BINDING_TEXT_FIELDS),'Unknown source field mapping')
 require(b['normal_inventory_type']==0 and b['enemy_skill_id']==stable(b['enemy_skill_script'])and b['item_constructor_id']==stable(b['item_script']),'Unknown source constructor identity')
 require(b['item_native']==b['enemy_skill_native']=='Reference','Unknown native source kind')
 require(len(b['stat_fields'])==7 and len(set(b['stat_fields']))==7,'Unknown source stat mapping')
 for group in BINDING_DEFAULTS:
  rows=b[group];require(type(rows)is list and len({r['name']for r in rows})==len(rows),'Duplicate source defaults')
  for r in rows:
   require(set(r)=={'name','kind','value'}and type(r['name'])is str and r['name']and r['kind']in range(1,7),'Unknown declaration metadata')
   require((type(r['value'])is str)if r['kind']in(1,3,4,5)else(type(r['value'])is int and 0<=r['value']<=2147483647),'Unknown default literal')
  if group!='enemy_constructor_defaults':require(all(r['value']in('',0)for r in rows),'Unknown cold constructor literal')
 expected_steps=dict(member_load_order=list(range(1,12)),character_load_order=[2,12,13,14,15,16,17,18,19,20,21,22,4],npc_load_order=[23,24,25,26,27],item_argument_order=list(range(1,15)))
 require(all(b[k]==v for k,v in expected_steps.items()),'Unknown source evaluation opcode/order')
 return b

def validate(d):
 require(d['schema']==2 and d['format']==2 and d['capability']==2 and d['rules']==1 and d['family']==FAMILY and d['commit']==PIN and d['cold_status_only']is True and d['whole_global_ready']is False and d['whole_load']is False,'Unknown character LOAD domain')
 validate_bindings(d['bindings'])
 require(len(d['rows'])==8 and len(d['stat_order'])==7 and len(set(d['stat_order']))==7 and len(set(d['skill_order']))==len(d['skill_order']),'Invalid source collections')
 for r in d['rows']:
  require(r['status']==[] and r['role']in(0,1)and all(type(r[k])is int and 0<=r[k]<=2147483647 for k in('level','exp','hp','pp')),'Invalid source scalar/status')
  require(len({k for k,v in r['permanent_fields']})==len(r['permanent_fields']) and all(k in d['stat_order']and type(v)is int and 0<=v<=2147483647 for k,v in r['permanent_fields']),'Unknown sparse permanent dictionary')
  require([dict(r['permanent_fields']).get(k,0)for k in d['stat_order']]==r['permanent'],'Permanent dict/projection mismatch')
  require(len(r['permanent'])==7 and all(type(x)is int and 0<=x<=2147483647 for x in r['permanent']),'Invalid permanent boost')
  require((len(r['targets'])==7 and not r['npc_stats'])if r['role']==0 else(len(r['npc_stats'])==7 and not r['targets']),'Invalid stat table type')
  require(all(len(t)>=2 and all(type(n)is int and 0<=n<=2147483647 for n in t)for t in r['targets']),'Invalid source stat target')
  for x in r['inventory']:require(type(x['equipped'])is bool and type(x['has_uid'])is bool and type(x['doses'])is int and 0<x['doses']<=65535 and type(x['uid'])is int and 0<=x['uid']<=0xffffffff,'Invalid Item saved payload')
 return d

def encode(d):
 validate(d);b=bytearray(128)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def q(*v):b.extend(struct.pack('<'+'q'*len(v),*v))
 def text(v):z=v.encode();u(len(z));b.extend(z)
 def strings(v):u(len(v));[text(x)for x in v]
 def ints(v):u(len(v));q(*v)
 bindings=d['bindings']
 for k in BINDING_TEXT_FIELDS:text(bindings[k])
 u(bindings['normal_inventory_type'],bindings['enemy_skill_id'],bindings['item_constructor_id']);strings(bindings['stat_fields'])
 for key in BINDING_DEFAULTS:
  u(len(bindings[key]))
  for f in bindings[key]:text(f['name']);u(f['kind']);text(f['value']if type(f['value'])is str else '');q(f['value']if type(f['value'])is int else 0)
 for key in BINDING_STEPS:u(len(bindings[key]),*bindings[key])
 strings(d['stat_order']);text(d['hp']);text(d['pp']);text(d['npc_skill_getter']);strings(d['source_slots']);strings(d['skill_order']);ints(d['exp_floors']);u(len(d['sources']))
 for p,h in d['sources'].items():text(p);b.extend(bytes.fromhex(h))
 u(len(d['rows']))
 for r in d['rows']:
  u(r['id'],r['role']);text(r['name']);text(r['display_name']);text(r['nickname']);q(r['level'],r['exp'],r['hp'],r['pp']);u(r['untargetable']);ints(r['permanent']);u(len(r['permanent_fields']))
  for key,value in r['permanent_fields']:text(key);q(value)
  strings(r['skills']);u(len(r['affinities']))
  for k,v in r['affinities']:text(k);b.extend(struct.pack('<d',v))
  u(len(r['targets']));[ints(t)for t in r['targets']];ints(r['npc_stats']);u(len(r['inventory']))
  for x in r['inventory']:text(x['name']);u(x['equipped'],x['doses'],x['has_uid'],x['uid'])
  u(len(r['npc_skills']))
  for x in r['npc_skills']:text(x['skill']);q(x['weight'],x['cooldown'])
 struct.pack_into('<8s8I',b,0,b'ENCCHAR1',2,128,len(b),zlib.crc32(b[128:]),FAMILY,2,1,stable(d['source_save']));b[40:60]=bytes.fromhex(PIN);b[60:92]=bytes.fromhex(d['sources'][d['source_save']]);b[92:124]=bytes.fromhex(sha(IR));return bytes(b)

def main():
 p=argparse.ArgumentParser();p.add_argument('action',choices=['extract','compile','verify']);a=p.parse_args();d=validate(derive())
 if a.action=='extract':write(IR,d);write(REVIEW,dict(schema=2,commit=PIN,ir_sha256=sha(IR),sources=d['sources'],engine=dict(tag='3.6.2-stable',gdscript_functions_sha256='ded4a283e0d876f43e5315bea2f5c543c3893b027eea56c9e344b33095fca2d2',scalar_lerp='double',sort_helper='include/encore/field_node_sort.hpp'),scope='Actual cold save_default eight-character LOAD only; empty source Status arrays; no whole global Ready/save/load admission',mapped_execution=dict(declarations='Character14, PartyMember5 including inv alias, PartyNPC2, Inventory2, Item4, EnemySkill4; all actual declared defaults',collections='Ordered sparse permanent dictionary preserved separately from seven-stat projection; learned skills, affinities, inventories and NPC skills preserve actual source order',cursor='PartyMember Inventory-before-name and setter/signal order; inherited Character then NPC skills filter/new/append; serialized Item left-to-right eager UID fallback before allocation',native_reference='Item and EnemySkill actual script adapters are separate stable IDs; implicit native Reference',source_boundary='Selected actual default save has empty Status arrays and positive serialized doses; no dynamic Status constructors, whole LOAD, normal new-game or whole Ready admission')));return
 old=read(IR);require(old==d and read(REVIEW)['ir_sha256']==sha(IR),'Changed source or unaudited IR');b=encode(old)
 if a.action=='compile':PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(b)
 else:require(PACK.read_bytes()==b,'Stale character LOAD pack')
 print('Cold save_default:',len(d['rows']),'actual owners;',sum(len(x['inventory'])for x in d['rows']),'serialized Items;',len(b),'bytes')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,TypeError,OSError,struct.error)as e:sys.exit('CHARACTER LOAD ERROR: '+str(e))
