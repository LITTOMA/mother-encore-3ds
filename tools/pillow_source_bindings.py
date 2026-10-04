"""Checked original Pillow/Minnie emissions and append namespace bindings."""
import copy,hashlib,json,math,re
from pathlib import Path
from tools.extract_battle_entry import ROOT,PIN,Extractor,node,require,one
from tools.programme_lowering_recipe import resolve,equal,document,at,KINDS,COMMAND_FIELDS
IR=ROOT/'content/pillow-source-bindings.json'

def read(path):
 def unique(pairs):
  out={}
  for key,value in pairs:require(key not in out,'Duplicate Pillow binding field');out[key]=value
  return out
 require(Path(path).stat().st_size<=512*1024,'Pillow binding authoring size')
 return json.loads(Path(path).read_text(encoding='utf-8'),object_pairs_hook=unique)
def fields(value,names):require(type(value)is dict and set(value)==set(names),'Unknown/missing Pillow binding fields')
def uint(value):return type(value)is int and 0<=value<=65535
def load(root=ROOT,recipe=None):
 root=Path(root);value=copy.deepcopy(read(IR)if recipe is None else recipe)
 fields(value,['schema','kind','commit','sources','paths','documents','facts','identities','linears','tutorial','append'])
 require(type(value['schema'])is int and value['schema']==1 and value['kind']=='encore.pillow-source-bindings'and value['commit']==PIN,'Pillow binding version/pin')
 ex=Extractor(root);require(type(value['sources'])is dict and value['sources'],'Pillow source coverage')
 for path,digest in value['sources'].items():require(type(digest)is str and hashlib.sha256(ex.data(path)).hexdigest()==digest,'Changed Pillow source fingerprint')
 fields(value['paths'],['attack','leave','door','tutorial','open']);require(len(set(value['paths'].values()))==5 and set(value['paths'].values())==set(value['documents']),'Pillow document coverage')
 for path in value['documents']:
  require(path in value['sources'],'Missing Pillow document provenance');document(value,path,ex.yaml(path))
  for phrase in value['documents'][path].values():require(type(phrase)is dict and set(phrase)<=set('actors actorsdir actorsmove teleportactors wait autoadvance caninput goto actorsanim actorsshake actorsemote actorsturn changecam returncam stopactorsloop talker name sound text actorsjump showbox setflags startbattle movecam options'.split()),'Unknown Pillow source phrase field')
 for fact in value['facts'].values():
  fields(fact,['source','pattern','value']);require(fact['source']in value['sources']and type(fact['pattern'])is str and len(fact['pattern'])<=4096 and type(fact['value'])in(int,float)and math.isfinite(fact['value']),'Pillow source fact schema')
  raw=one(fact['pattern'],ex.text(fact['source']),'Pillow expression',re.M)['value'];require(float(raw)==fact['value'],'Pillow source expression mismatch')
 a=value['append'];fields(a,'prefix_actors prefix_battles actors template_profile_index profile_id profile_index actor_id actor_index binding_kind actor_flags direction scene node sprite name world_manifest attack_receipt template_receipt door_receipt yaml_slot animation_slot clip_alias template_clip emote emote_actor battle_path battle_id battle_index battle_template flag_remove battle_enemy choices_index choices_id motion motion_id dialogue_first_id text_tables voice_pattern dialogue_script actor_script catalog_id idle_animation source_emotes native_receipts'.split())
 from tools.world_program_bindings import safe
 for key in 'scene node sprite world_manifest attack_receipt template_receipt door_receipt battle_path dialogue_script actor_script'.split():require(safe(a[key]),'Unsafe Pillow source/artifact selector')
 require(type(a['voice_pattern'])is str and a['voice_pattern'].count('{sound}')==1 and safe(a['voice_pattern'].replace('{sound}','checked')),'Unsafe Pillow voice binding')
 for key in 'prefix_actors prefix_battles template_profile_index profile_id profile_index actor_id actor_index binding_kind actor_flags yaml_slot animation_slot battle_id battle_index battle_template flag_remove choices_index motion_id dialogue_first_id catalog_id'.split():require(uint(a[key]),'Invalid Pillow append integer')
 require(a['profile_id']==a['actor_id']==a['actor_index']+1==a['profile_index']+1 and a['battle_id']==a['battle_index']+1,'Pillow append stable namespace')
 require(type(a['actors'])is dict and all(type(k)is str and uint(v)for k,v in a['actors'].items())and len(set(a['actors'].values()))==len(a['actors']),'Duplicate Pillow actor namespace')
 world=read(root/'content/world-program-bindings.json');house=read(root/'content/house-source-bindings.json')
 require(a['prefix_actors']==a['actor_index']and a['prefix_battles']==a['battle_index']and a['profile_index']==a['actor_index'],'Pillow append prefix identity')
 require(a['direction']==house['npc']['default_direction'],'Pillow original NPC direction relation')
 doc=value['documents'][value['paths']['attack']];require(set(a['actors'])=={'None'}|{alias.title()for alias in doc['0']['actors']}and a['actors']['None']==65535,'Pillow actor scope')
 battle=doc[list(doc)[-1]]['startbattle'];require(battle['battlers']==[{a['name']:a['battle_enemy']}]and battle['wincutscene']==value['paths']['leave'].removeprefix('Data/Dialogue/').removesuffix('.yaml'),'Pillow battle source target')
 emotes={emote for doc in value['documents'].values()for phrase in doc.values()for emote in phrase.get('actorsemote',{}).values()};require(type(a['source_emotes'])is list and len(set(a['source_emotes']))==len(a['source_emotes'])and set(a['source_emotes'])==emotes,'Pillow source emote coverage')
 require(type(a['native_receipts'])is list and len(a['native_receipts'])==3 and {r['role']for r in a['native_receipts']}=={'attack','leave','door'},'Pillow native receipt coverage')
 for receipt in a['native_receipts']:fields(receipt,['role','emote','path']);require(receipt['emote']in emotes and safe(receipt['path']),'Pillow native emote role/path')
 require(world['commit']==house['commit']==PIN and a['scene']==world['scene']and house['pillow']['tutorial']==value['paths']['tutorial'].removeprefix('Data/Dialogue/').removesuffix('.yaml'),'Pillow parent recipe relation')
 for actor in world['actors']:
  if actor['alias']in a['actors']:require(a['actors'][actor['alias']]==actor['id']-1,'Pillow shared world actor identity')
 scene=ex.text(a['scene']);source=node(scene,a['node']);require(source['sprite']==a['sprite']and source['no_shadow']and a['name']==a['node'].split('/')[-1],'Pillow source actor node/sprite')
 require(value['documents'][value['paths']['attack']]['0']['actors'][a['name']]==a['node']and a['actors'][a['name'].title()]==a['actor_index'],'Pillow programme append actor relation')
 require(a['clip_alias']==a['name'].title()+' '+a['idle_animation'],'Pillow clip alias source identity')
 template_actor=next(row for row in world['actors']if row['id']==a['template_profile_index']+1)
 require(a['template_clip']==template_actor['alias']+' '+a['idle_animation'],'Pillow inherited clip source identity')
 catalog=read(root/'content/native-resource-catalog.json');require(catalog['commit']==PIN and any(row['id']==a['catalog_id']and row['role']=='EncounterBattle'and row['path']==a['battle_path']for row in catalog['bindings']),'Pillow source encounter catalog relation')
 require(a['motion_id']==next(r['state']for r in world['animation']['motions']if r['animation']==a['motion']),'Pillow shared animation motion')
 choices=read(root/'content/native-dialogue-choices.json');require(a['choices_index']<len(choices['groups'])and choices['groups'][a['choices_index']]['id']==a['choices_id'],'Pillow shared choice identity')
 for key,identity in value['identities'].items():
  fields(identity,['source','path','value','category']);require(identity['source']in value['documents'],'Pillow identity source');raw=at(value['documents'][identity['source']],identity['path'])
  if identity['category']=='actor':require(type(raw)is str and identity['value']==identity['path'][-1].title()and identity['value']in a['actors'],'Pillow source actor alias')
  elif identity['category']=='text':require(type(raw)is str and type(identity['value'])is int and house['pillow']['text_ids'][identity['source']+'::'+identity['path'][0]]==identity['value'],'Pillow House stable text identity')
  elif identity['category']=='clip':require(type(raw)is str and identity['value']==(a['clip_alias']if raw==a['idle_animation']else raw)and(raw in a['source_emotes']or identity['value']==a['clip_alias']),'Pillow source clip alias')
  else:raise ValueError('Unknown Pillow identity category')
 require(set(value['linears'])=={value['paths'][k]for k in ['attack','leave','door']},'Pillow linear programme coverage')
 for path,programme in value['linears'].items():
  fields(programme,['commands']);require(type(programme['commands'])is list and 1<=len(programme['commands'])<=4096,'Pillow emission count')
  for template in programme['commands']:
   require(type(template)is dict and {'kind','phrase','actor'}<=set(template)<=COMMAND_FIELDS|{'target_index'} and template['kind']in KINDS|{'StopActorLoop'} and type(template['phrase'])is int and 0<=template['phrase']<len(value['documents'][path]),'Pillow emission schema/opcode/phase')
   command=resolve(template,value['documents'][path],value,{'end_duration':1});require(command['actor']in a['actors'],'Pillow emission actor identity')
   if 'flags'in command:require(type(command['flags'])is int and 0<=command['flags']<=65535,'Pillow emission flags type')
   if template['actor']!='None':require(type(template['actor'])is dict and '$identity'in template['actor'],'Unreviewed Pillow literal actor')
   label=list(value['documents'][path])[command['phrase']];phrase=value['documents'][path][label];alias=command['actor'].lower();kind=command['kind']
   if kind=='ShowDialogue':require('text'in phrase and command['dialogue_id']==house['pillow']['text_ids'][path+'::'+label],'Pillow emission text phase mismatch')
   if kind=='MoveActorPath':require(alias in phrase.get('actorsmove',{})and equal(command['path'],phrase['actorsmove'][alias]),'Pillow movement source mismatch')
   if kind=='SetFlag':require(command['flag']==phrase.get('setflags')and command['value']==1,'Pillow flag source phase mismatch')
   if kind=='JumpActor':require(alias in phrase.get('actorsjump',{})and command['value']==phrase['actorsjump'][alias]['height']and type(command['flags'])is int and command['flags']==phrase['actorsjump'][alias].get('times',1)-1,'Pillow jump source repetition mismatch')
   if kind=='AnimateActor':require(alias in phrase.get('actorsanim',{})and phrase['actorsanim'][alias]['anim']==a['idle_animation']and command['clip']==a['clip_alias'],'Pillow animated source binding mismatch')
   if kind=='EmoteActor':require(command['clip']==phrase.get('actorsemote',{}).get(alias),'Pillow emote source actor/clip mismatch')
   if kind in ['QueueBattle','RequestBattle']:require(command['actor']==a['name'].title()and 'startbattle'in phrase,'Pillow battle emission actor/phase mismatch')
 t=value['tutorial'];fields(t,['identity','commands','source_labels','choice_groups']);doc=value['documents'][value['paths']['tutorial']]
 require(t['identity']==value['paths']['tutorial'].removeprefix('Data/Dialogue/').removesuffix('.yaml')and t['source_labels']==list(doc)and len(t['choice_groups'])==1,'Pillow tutorial source identity/order')
 group=t['choice_groups'][0];require(group['id']==a['choices_id']and group['program_identity']==t['identity']and group['program_command_count']==len(t['commands']),'Pillow tutorial choice programme')
 options=doc[group['source_label']]['options'];require([(o['translation_key'],o['target_label'])for o in group['options']]==[(k,v)for k,v in options.items()if k!='cancel']and group['cancel_target_label']==options['cancel'],'Pillow source choices mismatch')
 for option in group['options']+[{'target_label':group['cancel_target_label'],'target_pc':group['cancel_target_pc']}]:
  require(type(option['target_pc'])is int and 0<=option['target_pc']<len(t['commands'])and t['commands'][option['target_pc']]['phrase']==t['source_labels'].index(option['target_label']),'Pillow choice target boundary')
 for command in t['commands']:require(type(command)is dict and {'kind','phrase','actor'}<=set(command)<=COMMAND_FIELDS|{'choice_group','target_label','target_pc'}and command['kind']in KINDS|{'AwaitChoices','Jump'}and command['actor']=='None'and type(command['phrase'])is int and 0<=command['phrase']<len(doc),'Pillow tutorial opcode/actor/phase');resolve(command,doc,value,{'end_duration':1},translations={o['translation_key']:''for o in group['options']})
 translations={};import csv,io
 for path in a['text_tables']:
  require(path in value['sources'],'Pillow translation provenance');translations.update({r['key']:r['en']for r in csv.DictReader(io.StringIO(ex.text(path)))})
 require(resolve(group,doc,value,{'end_duration':1},translations)==choices['groups'][a['choices_index']],'Pillow reviewed shared choice graph mismatch')
 return value

def linear(path,doc,end,text_ids,turn,move,jump,root=ROOT):
 value=load(root);require(path in value['linears'],'Unknown Pillow programme');document(value,path,doc)
 require([turn,move,jump]==[value['facts'][k]['value']for k in ['turn','move','jump']],'Pillow caller defaults differ from source')
 from tools.doll_postwin import return_duration
 require(end==return_duration((Path(root)/'upstream/MOTHER-Encore'/value['append']['dialogue_script']).read_text(encoding='utf-8')),'Pillow caller camera duration mismatch')
 out=[resolve(c,doc,value,{'end_duration':end})for c in value['linears'][path]['commands']]
 for command in out:
  if'dialogue_id'in command:require(command['dialogue_id']==text_ids[path+'::'+list(doc)[command['phrase']]],'Pillow caller stable text identity mismatch')
 return out
def tutorial(doc,translations,end,root=ROOT):
 value=load(root);document(value,value['paths']['tutorial'],doc)
 from tools.doll_postwin import return_duration
 require(end==return_duration((Path(root)/'upstream/MOTHER-Encore'/value['append']['dialogue_script']).read_text(encoding='utf-8')),'Pillow tutorial camera duration mismatch')
 actual={};import csv,io
 for path in value['append']['text_tables']:
  actual.update({row['key']:row['en']for row in csv.DictReader(io.StringIO((Path(root)/'upstream/MOTHER-Encore'/path).read_text(encoding='utf-8')))})
 for option in value['tutorial']['choice_groups'][0]['options']:require(translations[option['translation_key']]==actual[option['translation_key']],'Pillow choice translation mismatch')
 return resolve(value['tutorial'],doc,value,{'end_duration':end},translations)
