#!/usr/bin/env python3
"""Compile pinned Doll actions, boss callbacks and bounded level2 rewards."""
import argparse,copy,csv,io,json,re,sys,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'));sys.path.insert(0,str(ROOT))
import native_round as native
from extract_battle_entry import Extractor,one,properties,require,node
from round_assets import Presentation
IR=ROOT/'content/doll-round.json';PACK=ROOT/'romfs/data/doll-entry.encround';REPORT=ROOT/'reports/doll-round'
def digest(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def read(p):return json.loads(Path(p).read_text())
def write(p,v):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(v,indent=2,ensure_ascii=False)+'\n')
def conditional_growth_policy(pm):
 """Audit the source boundary that makes growth conditional on live EXP."""
 from boss_presentation_bindings import load
 recipe=load(ROOT);facts=recipe['source_facts'][recipe['progression']['source']]
 for fragment in facts:require(fragment in pm,'Unreviewed conditional progression source '+fragment)

def return_music_policy(enemy,system):
 """Lower the reviewed single-enemy source return branch into encounter data."""
 def function(name):return one(r'^func '+name+r'\([^\n]*\):[^\n]*\n(.*?)(?=^func |\Z)',system,name,re.M|re.S)[1]
 require('_music = _enemy_BPs[0].character.get_data().get("music", "")'in system and 'if enemy.is_boss(): _is_boss = true'in system,'Unreviewed battle music/boss source binding')
 cleanup=function('_remove_battle_music');ending=function('_end_battle_to_overworld')
 require('\tif _music:\n'in cleanup and '\telif _is_boss and audioManager.overworldBattleMusic:\n\t\tfor musicChanger in audioManager.musicChangers:\n\t\t\tmusicChanger.stop_music_immediately()'in cleanup,'Unreviewed area music cleanup condition/owner')
 require('\tif !audioManager.overworldBattleMusic or _is_boss:\n\t\t_remove_battle_music()'in ending,'Unreviewed battle return cleanup condition')
 markers=['_remove_battle_music()', 'audioManager.resume_all_music()', '$AnimScene.play("transitionOut")', 'emit_signal("battle_to_ov")']
 require(all(marker in ending for marker in markers),'Missing battle return music boundary')
 require([ending.index(marker)for marker in markers]==sorted(ending.index(marker)for marker in markers),'Unreviewed battle return music ordering')
 require(type(enemy.get('boss',False))is bool and isinstance(enemy.get('music',''),str),'Unreviewed enemy music/boss value')
 return int(enemy.get('boss',False)and not enemy.get('music',''))

def build(bindings=None):
 ex=Extractor(ROOT);base=read(ROOT/'content/native-round.json');native.verify_sources(base)
 from battle_round_bindings import load
 recipe=load();skill_bindings=recipe['skills'];enemy_skill_names=[s['name']for s in skill_bindings if s['actor']=='enemy']
 from boss_presentation_bindings import load as boss_bindings, MEDIA, EVENT, STAT, animation
 boss_recipe=boss_bindings(ROOT,bindings);enc=boss_recipe['encounter'];progression=boss_recipe['progression'];text_bindings=boss_recipe['texts']
 ir=copy.deepcopy(base);entry=read(ROOT/'content/doll-entry.json');lamp=read(ROOT/'content/native-battle.json')
 for path,sha in base['sources'].items():require(digest(ex.upstream/path)==sha,'Changed inherited source '+path);ex.data(path)
 enemy=ex.yaml(enc['enemy_source']);scene=ex.yaml(enc['cutscene_source']);request=scene[enc['cutscene_step']]['startbattle']
 require(request['battlers']==[{enc['actor']:enc['actor']}]and request['actorskeep']=={enc['actor']:True},'Doll encounter source changed')
 require(enemy['boss'] and [x['skill']for x in enemy['skills']]==enemy_skill_names and not enemy.get('items'),'Doll AI/reward path changed')
 require(entry['binding']['stable_id']==enc['battle_id'] and entry['enemy']['data']==enemy,'Doll entry mismatch')
 trans={}
 for source in boss_recipe['translation_sources']:
  for row in csv.DictReader(io.StringIO(ex.text(source))):trans[row['key']]=row['en']
 def text(key,role,values=None):
  raw=trans[key];value=raw
  for k,v in(values or {}).items():value=value.replace('{'+k+'}',str(v))
  require(not re.search(r'\{[^}]+\}',value),'Unhandled Doll text '+value)
  ir['texts'].append(dict(id=len(ir['texts'])+1,role=role,key=key,source_text=raw,text=value));return len(ir['texts'])-1
 name=lamp['party']['initial_save_data']['name'];ename=trans[enemy['name']];articles=trans[enemy['article']].split(',');party_articles=trans[enc['party_articles_key']].split(',')
 contexts=[dict(name=name,n0=party_articles[0],n4=party_articles[4],target=ename,t0=articles[0],t1=articles[1]),dict(name=ename,n0=articles[0],n4=articles[4],target=name,t0=party_articles[0],t1=party_articles[1])]
 for i,s in enumerate(ir['skills']):
  source=ex.yaml(s['source'])
  if source['dialog']:s['dialog']=text(source['dialog'],3,contexts[0 if skill_bindings[i]['actor']=='party'else 1])
 ir['binding'].update(battle_id=entry['binding']['stable_id'],enemy_name=text(enemy['name'],4),enemy_article=text(enemy['article'],5),win_flag='',show_intro_outro=int(entry['entry']['show_intro_outro']))
 ir['enemy_choices']=[dict(skill=next(i for i,s in enumerate(ir['skills'])if Path(s['source']).stem==c['skill']),weight=c['weight'])for c in enemy['skills']]
 v=ir['victory'];v.update(initial_exp=base['victory']['reward_exp'],initial_bank=base['victory']['reward_cash'],initial_earned_cash=base['victory']['reward_cash'],reward_exp=enemy['exp'],reward_cash=enemy['cash'],exp_text=text(text_bindings['experience']['key'],text_bindings['experience']['role'],dict(name=name,value=enemy['exp'])))
 from native_content import parse_pack
 room=parse_pack((ROOT/'romfs/data/opening.encroom').read_bytes());body=[b for b in room['sections']['BodyRule']if room['strings'][b['source_path_string']]==enc['body_source']];require(len(body)==1,'Doll body binding');v['enemy_body_id']=body[0]['body_id']
 p=ir['presentation'];receipt=read(ROOT/'content/asset-receipts/graphics/battle/doll/source.json');r=next(r for r in receipt['resources']if r['name']==enc['receipt_resource']);idx=next(m['resource']for m in p['media']if m['role']==2)
 resource=p['resources'][idx];resource.update(path=r['output'],width=r['width'],height=r['height'],columns=1,rows=1,sha256=digest(ROOT/'romfs'/r['output']));ex.data(enc['sprite_source'])
 for m in p['media']:
  if m['role']==2:m['rect'][2:]=[resource['width'],resource['height']]
 pres=Presentation(ex,p['resources'])
 for k in['media','tracks','keys','events','bindings','parameters']:setattr(pres,k,p[k])
 def rawanim(binding):
  source=ex.text(binding['source']);props,tracks=animation(source,binding);role=MEDIA[binding['role']]
  rect=[0,0,resource['width'],resource['height']]if binding['geometry']=='enemy'else[0,0,entry['viewport']['width'],entry['viewport']['height']]
  res=idx if binding['resource']=='enemy'else 0xffffffff
  m=pres.add(binding['source']+':'+binding['clip'],role,res,props['length'],rect,flags=binding['flags'],anchor=binding['anchor'])
  for tr in tracks.values():
   keys=tr['keys']
   if tr['type']=='value':pres.track(m,binding['value_properties'][tr['path']],keys['times'],keys['values'],keys.get('update',0),eases=keys['transitions'])
   elif tr['type']=='method':
    for time,value in zip(keys['times'],keys['values']):
     operation=binding['methods'][value['method']]['operation']
     if operation=='Shake':
      mag,length,interval=value['args'];ir['boss_shakes'].append(dict(time=time,magnitude=mag,length=length,interval=interval,weight=recipe['boss_shake']['value']))
     else:pres.event(m,time,EVENT[operation])
   elif tr['type']!='audio':raise ValueError('Unknown boss track')
  if binding['binding_slot']:pres.bind(binding['binding_slot'],m)
  return m
 ir['boss_shakes']=[]
 boss=rawanim(boss_recipe['animations']['enemy_defeat']);flash=rawanim(boss_recipe['animations']['defeat_flash'])
 ex.data(recipe['boss_shake']['source']) # load() checks the default and reviewed mechanism facts.
 for source,facts in boss_recipe['source_facts'].items():
  actual=ex.text(source)
  for fact in facts:require(fact in actual,'Unreviewed boss lifecycle/growth fact')
 system=ex.text(enc['system_source'])
 pm=ex.text(progression['source']);skill=ex.yaml(progression['skill_source'])
 stats=lamp['party']['stat_targets'];effective=lamp['party']['effective_stats'];bases=lamp['party']['base_stats'];ir['growth']=[]
 for stat in progression['stat_order']:
  targets=stats[stat];after=int(targets[0]+(targets[1]-targets[0])*(progression['promoted_level']/10.0));before=bases[stat];boost=effective[stat]-before;gain=after-before
  ir['growth'].append(dict(stat=STAT[stat],before=effective[stat],after=after+boost,text=text(text_bindings['growth']['key'],text_bindings['growth']['role'],dict(stat=trans[progression['stat_labels'][stat]],value=gain))if gain else 0))
 ir['encounter']=dict(boss=int(enemy['boss']),keep_actor=int(request['actorskeep'][enc['actor']]),post_win_script=request['wincutscene'],boss_flash_media=flash['id']-1,promoted_level=progression['promoted_level'],following_level_exp=room['sections']['Experience'][progression['following_level']-1]['required_total_exp'],level_text=text(text_bindings['level']['key'],text_bindings['level']['role'],dict(name=name,value=progression['promoted_level'])),learned_skill=progression['learned_skill'],learned_text=text(text_bindings['learning']['key'],text_bindings['learning']['role'],dict(n0=party_articles[0],name=name,skill=trans[skill['name']],skillLevel='')),stop_area_music_if_overworld=return_music_policy(enemy,system))
 conditional_growth_policy(pm)
 ir['schema']=5;ir['scope']='Doll actions, retained actor, source boss callbacks, live carried session with conditional level2 growth and field Telepathy, typed post-win boundary and pre-return owned area music cleanup'
 ir['sources']=ex.sources;ir['dependencies']={path:digest(ROOT/path)for path in['content/native-battle.json','content/native-opening.json','content/doll-entry.json','content/native-round.json']}
 from boss_presentation_bindings import check_round
 check_round(ir,boss_recipe,ROOT)
 return ir

def main():
 ap=argparse.ArgumentParser();ap.add_argument('action',choices=['extract','compile','verify'],nargs='?',default='compile');a=ap.parse_args()
 if a.action=='extract':
  ir=build();write(IR,ir);write(REPORT/'source-review.json',dict(schema=ir['schema'],commit=native.PIN,sources=ir['sources'],dependencies=ir['dependencies'],scope=ir['scope'],limits='Host semantic verification only; audited boss audio tracks not yet mapped to audio backend; field Telepathy learned state retained, field menu unavailable'))
 ir=read(IR);native.verify_sources(ir);blob=native.encode(native.lower(ir));native.parse_pack(blob)
 if a.action=='verify':require(PACK.read_bytes()==blob,'Stale Doll round binary')
 else:PACK.parent.mkdir(parents=True,exist_ok=True);PACK.write_bytes(blob);write(REPORT/'compile.json',dict(schema=ir['schema'],bytes=len(blob),sha256=digest(PACK),ir_sha256=digest(IR)))
 print('Doll round:',len(blob),'bytes; checked source ENCRND01 v'+str(ir['schema']))
if __name__=='__main__':
 try:main()
 except(ValueError,KeyError,OSError,TypeError)as error:print('DOLL ROUND ERROR:',error,file=sys.stderr);raise SystemExit(1)
