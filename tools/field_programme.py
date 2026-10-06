#!/usr/bin/env python3
"""Independent original field programmes, executed by the checked Room scheduler."""
from __future__ import annotations
import argparse,csv,hashlib,io,json,struct,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.podunk_scene import PIN,SCENE,stable,read,write,sha,require
from tools.extract_battle_entry import Extractor
from tools.basement_progression import load as basement_load
from tools.native_content import OPCODES,NONE,NO_ACTOR
IR=ROOT/'content/native-field-programme.json';REVIEW=ROOT/'reports/field-programme/source-review.json';PACK=ROOT/'romfs/data/podunk-programmes.encprog'
TOKENS={'Literal':1,'PlayerName':2,'HintStart':3,'HintEnd':4,'FavoriteFood':11,'InputAction':12}
SUPPORTED={'BeginCutscene','ShowDialogue','GrantKeyItem','PlaySound','SetFlag','AwaitChoices','AwaitDialogue','Jump','StopInteraction','SetTalker','CutsceneEnded','DialogueDone'}
def build():
 d=basement_load(ROOT);ex=Extractor(ROOT);npc=read(ROOT/'content/native-field-npc.json');require(npc['commit']==PIN and npc['scene']==SCENE,'NPC source pin')
 m=[n for n in npc['npcs']if n['node']==d['mick']['node']];require(len(m)==1 and m[0]['id']==stable(m[0]['node']),'Source Mick instance identity');m=m[0]
 source_script=ex.text('Scripts/Main/npc.gd')
 flag_source=ex.text('Scripts/global/globalData.gd');flag_emit='emit_signal := true' in flag_source.split('func set_flag(',1)[1].split('):',1)[0];require(flag_emit,'Source flag default changed')
 require('globaldata.seen_dialogue_flags[last_dialog_hash] = true'in source_script and '"%s:%s:%s:%s" % [get_path(), flag, j, cur_dialog]'in source_script and 'uiManager.open_dialogue_box(_get_right_dialog(true, true))\n\tuiManager.set_telepathy_effect(true, self)'in source_script,'Unknown NPC dialogue/Telepathy lifecycle')
 programs=[p for p in d['programs']if any(o['supported']and o['program']==p['identity']for o in d['mick']['overrides'])]
 require(len(programs)==5 and all(c['kind']in SUPPORTED for p in programs for c in p['commands']),'Unsupported Mick execution opcode')
 paths={p['identity']for p in programs};texts=[t for t in d['texts']if t['source'][14:-5]in paths]
 choices=[c for c in d['choice_groups']if c['program_identity']in paths]
 table={r['key']:r for r in csv.DictReader(io.StringIO(ex.text('Translations/TranslatedText/dialogue_Podunk - sheet.csv')))}
 for c in choices:
  for o in c['options']:o['texts']={lang:table[o['translation_key']][col]for lang,col in [('en','en'),('zh_Hans_CN','zh_CN')]};o.pop('text')
 refs=[]
 for r in m['dialogues']:
  name='Data/Dialogue/'+r['program']+'.yaml';ex.text(name);refs.append(dict(r,supported=r['program']in paths,sha256=ex.sources[name]))
 ex.sources.update(d['sources'])
 commandsource=ex.text('Scripts/UI/DialogueBox.gd').split('func _handle_phrase() -> void:',1)[1];require(commandsource.index('if _curr_phrase.has("item")')<commandsource.index('if _curr_phrase.has("text")')<commandsource.index('if _curr_phrase.has("soundeffect")')<commandsource.index('if _curr_phrase.has("setflags")'),'Unreviewed source phrase effect order')
 return dict(schema=1,kind='encore.field-programme.source-ir',commit=PIN,scene=SCENE,flag_emit=flag_emit,source_npc=dict(id=m['id'],node=m['node'],ready_ordinal=m['ready_ordinal'],dialogues=refs),programs=programs,texts=texts,choices=choices,key_items=[k for k in d['key_items']if any(c.get('key_id')==k['id']for p in programs for c in p['commands'])],hint_color=d['hint_color'],sources=dict(sorted(ex.sources.items())),dependency=dict(basement_ir_sha256=sha(ROOT/'content/native-basement-progression.json'),npc_ir_sha256=sha(ROOT/'content/native-field-npc.json')),semantics=['Actual NPC21 source identity and ordered normal/thought rows; last enabled row wins, retain get_path seen-dialogue identity including flag/j/program','Complete five original Mick scratch choice/thought/key programmes, existing DialoguePlayer execution and suspension; unknown future DogTreat branches fail before any admission mutation or PP debit','Source woof_key label4 grants independent keybag item before text then Item Received audio; label6 writes mick_telepathy before final text completes','Bilingual source text tokens and choice translation/targets retain WAIT segments; presenter uses existing checked font/geometry/source input labels','Live SourceActor ObjectID supplied by actual tree, never stable source identity cast as ObjectID; admission validates all typed lifecycle/audio/text/item/flag/choice endpoints before mark_seen/start','NPC marks actual get_path seen identity before UI instantiation; source open_dialogue_box deferred node addition and actual dialogue Ready precede scheduler execution; Telepathy effect follows open call without synchronously forcing Ready'],unsupported=['Two future DogTreats dialogue branches remain explicitly pending until their full mechanisms are reviewed','Host must supply actual NPC lifecycle, save/UID/shared RNG atomic key grant, existing NDSP audio/presenter and SceneTree completion endpoints; missing endpoints fail closed'])
def validate(d):
 require(d['schema']==1 and d['kind']=='encore.field-programme.source-ir'and d['commit']==PIN and d['scene']==SCENE,'Field programme version/source')
 n=d['source_npc'];require(n['id']==stable(n['node'])and n['ready_ordinal']>0 and len(n['dialogues'])==8,'Field NPC source rows')
 ps={p['identity']:p for p in d['programs']};require(len(ps)==len(d['programs'])==5,'Field programme table scope')
 tids={t['id']for t in d['texts']};require(len(tids)==len(d['texts'])and tids,'Field text identities')
 for p in d['programs']:
  require(p['commands'][0]['kind']=='BeginCutscene'and p['commands'][-1]['kind']=='DialogueDone'and 0<p['commands'][-1]['duration']<=60,'Field complete programme terminator')
  for c in p['commands']:
   require(c['kind']in SUPPORTED and c['label']in p['document'],'Unknown field command/label')
   if c['kind']=='Jump':require(0<=c['target_pc']<len(p['commands'])and c['target_pc']==p['labels'][c['target_label']],'Field branch target')
   if c['kind']=='ShowDialogue':require(c['text_id']in tids,'Field text command binding')
 for t in d['texts']:
  require(set(t['segments'])==set(t['speakers'])=={'en','zh_Hans_CN'},'Field text locales')
  for segments in t['segments'].values():
   require(0<len(segments)<=64,'Field text segment bound')
   for s in segments:
    for k in s['tokens']:require(k['kind']in TOKENS,'Unknown field text token')
 for r in n['dialogues']:require(r['supported']==(r['program']in ps)and r['ordinal']==1 and r['last']and r['sha256']==d['sources'][r['source']],'Field source programme admission')
 return d

def load():
 d=validate(read(IR));r=read(REVIEW);require(r['commit']==PIN and r['ir_sha256']==sha(IR)and r['sources']==d['sources'],'Field programme semantic review stale')
 require(d['dependency']==dict(basement_ir_sha256=sha(ROOT/'content/native-basement-progression.json'),npc_ir_sha256=sha(ROOT/'content/native-field-npc.json')),'Field programme dependency changed')
 require(d==build(),'Field programme source lowering mismatch');return d

def encode(d):
 validate(d);b=bytearray(64)
 def u(*v):b.extend(struct.pack('<'+'I'*len(v),*v))
 def f(v):b.extend(struct.pack('<d',v))
 def text(v):r=v.encode();u(len(r));b.extend(r)
 flags=[];sounds=[]
 for p in d['programs']:
  for c in p['commands']:
   for key,array in [('flag_id',flags),('sound_source',sounds)]:
    if key in c and c[key]not in array:array.append(c[key])
 choices={c['id']:i for i,c in enumerate(d['choices'])};n=d['source_npc']
 text(d['scene']);text(d['hint_color']);u(int(d['flag_emit']));u(n['id'],n['ready_ordinal']);text(n['node']);u(len(n['dialogues']))
 for r in n['dialogues']:u(int(r['thoughts']),r['group'],r['ordinal'],int(r['last']),int(r['supported']));text(r['flag']);text(r['program']);text(r['source']);b.extend(bytes.fromhex(r['sha256']))
 u(len(flags))
 for v in flags:text(v)
 u(len(sounds))
 for v in sounds:text(v)
 u(len(d['key_items']))
 for k in d['key_items']:u(k['id'],k['doses'],int(k['grant']));text(k['source']);text(k['name_key'])
 u(len(d['texts']))
 for t in d['texts']:
  u(t['id']);text(t['source']);text(t['label']);text(t['key']);text(t['speaker_key']);text(t['voice']);u(len(t['segments']))
  for lang,segs in t['segments'].items():
   text(lang);text(t['speakers'][lang]);u(len(segs))
   for s in segs:
    u(int(s['bullet']),len(s['tokens']))
    for k in s['tokens']:u(TOKENS[k['kind']]);text(k.get('text',k.get('action',d['hint_color']if k['kind']=='HintStart'else'')))
 u(len(d['choices']))
 for c in d['choices']:
  text(c['id']);text(c['program_identity']);text(c['source_label']);u(c['initial_selection'],c['cancel_target_pc'],len(c['options']))
  for o in c['options']:
   text(o['translation_key']);u(o['target_pc'],len(o['texts']))
   for lang,v in o['texts'].items():text(lang);text(v)
 u(len(d['programs']));first=0
 for p in d['programs']:
  text(p['identity']);text('Data/Dialogue/'+p['identity']+'.yaml');u(zlib.crc32(p['identity'].encode())&NONE,first,len(p['commands']),len(p['document']));first+=len(p['commands'])
  for c in p['commands']:
   kind=c['kind'];target=NONE
   if kind=='ShowDialogue':target=c['text_id']
   elif kind=='GrantKeyItem':target=c['key_id']
   elif kind=='SetFlag':target=flags.index(c['flag_id'])
   elif kind=='PlaySound':target=sounds.index(c['sound_source'])
   elif kind=='AwaitChoices':target=choices[c['group']]
   elif kind=='Jump':target=c['target_pc']
   u(OPCODES.index(kind),NO_ACTOR,c['phrase'],target,c.get('flags',0),NONE);f(c.get('value',0));f(c.get('duration',0));text(c['label'])
 u(len(d['sources']))
 for p,h in d['sources'].items():text(p);b.extend(bytes.fromhex(h))
 struct.pack_into('<8s6I20s12x',b,0,b'ENCFPG01',1,len(b),0,1,1,len(d['programs']),bytes.fromhex(PIN));struct.pack_into('<I',b,16,zlib.crc32(b)&NONE);return bytes(b)
def stage_files(source):
 b=encode(load());p=Path('data/podunk-programmes.encprog');require((Path(source)/p).read_bytes()==b,'Stale staged field programme pack');return{p:b}
def main():
 a=argparse.ArgumentParser();a.add_argument('action',choices=['extract','compile']);args=a.parse_args()
 if args.action=='extract':
  d=validate(build());write(IR,d);write(REVIEW,dict(schema=1,commit=PIN,ir_sha256=sha(IR),sources=d['sources'],semantics=d['semantics'],unsupported=d['unsupported']));print('Field programme source: actual NPC21 / five complete original Mick programmes');return
 b=encode(load());PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(b);print('Field programme binary:',len(b),'bytes')
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,OSError,TypeError,struct.error)as e:sys.exit('FIELD PROGRAMME ERROR: '+str(e))
