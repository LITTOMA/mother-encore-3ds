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
 for fragment in ['return _set_exp(_exp + quantity, true, out_stats, out_learned_skills)',
                  '_exp = min(new_value, _level_to_exp(LEVEL_CAP)) as int',
                  'var new_level := _exp_to_level(_exp)', 'if _level != new_level:',
                  'var difference := new_value - get_base_stat(stat)',
                  'set_stat(stat, new_value)', 'if difference > 0:',
                  '_learn_new_skills(level_diff, out_learned_skills)']:
  require(fragment in pm,'Unreviewed conditional progression source '+fragment)
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

def build():
 ex=Extractor(ROOT);base=read(ROOT/'content/native-round.json');native.verify_sources(base)
 ir=copy.deepcopy(base);entry=read(ROOT/'content/doll-entry.json');lamp=read(ROOT/'content/native-battle.json')
 for path,sha in base['sources'].items():require(digest(ex.upstream/path)==sha,'Changed inherited source '+path);ex.data(path)
 enemy=ex.yaml('Data/Battlers/doll.yaml');scene=ex.yaml('Data/Dialogue/Podunk/cutscenes/doll_attack.yaml')
 require(scene['6']['startbattle']=={'battlers':[{'doll':'doll'}],'actorskeep':{'doll':True},'wincutscene':'Podunk/cutscenes/doll_defeated'},'Doll encounter source changed')
 require(enemy['boss'] and [x['skill']for x in enemy['skills']]==['tackle','float']and not enemy.get('items'),'Doll AI/reward path changed')
 require(entry['binding']['stable_id']==2 and entry['enemy']['data']==enemy,'Doll entry mismatch')
 trans={}
 for name in['battletext','battlers','battleskills','menus']:
  for row in csv.DictReader(io.StringIO(ex.text('Translations/TranslatedText/'+name+' - sheet.csv'))):trans[row['key']]=row['en']
 def text(key,role,values=None):
  raw=trans[key];value=raw
  for k,v in(values or {}).items():value=value.replace('{'+k+'}',str(v))
  require(not re.search(r'\{[^}]+\}',value),'Unhandled Doll text '+value)
  ir['texts'].append(dict(id=len(ir['texts'])+1,role=role,key=key,source_text=raw,text=value));return len(ir['texts'])-1
 name=lamp['party']['initial_save_data']['name'];ename=trans[enemy['name']];articles=trans[enemy['article']].split(',');party_articles=trans['ARTICLES_NINTEN'].split(',')
 contexts=[dict(name=name,n0=party_articles[0],n4=party_articles[4],target=ename,t0=articles[0],t1=articles[1]),dict(name=ename,n0=articles[0],n4=articles[4],target=name,t0=party_articles[0],t1=party_articles[1])]
 for i,s in enumerate(ir['skills']):
  source=ex.yaml(s['source'])
  if source['dialog']:s['dialog']=text(source['dialog'],3,contexts[0 if i in[0,3]else 1])
 ir['binding'].update(battle_id=entry['binding']['stable_id'],enemy_name=text(enemy['name'],4),enemy_article=text(enemy['article'],5),win_flag='',show_intro_outro=int(entry['entry']['show_intro_outro']))
 ir['enemy_choices']=[dict(skill=next(i for i,s in enumerate(ir['skills'])if Path(s['source']).stem==c['skill']),weight=c['weight'])for c in enemy['skills']]
 v=ir['victory'];v.update(initial_exp=base['victory']['reward_exp'],initial_bank=base['victory']['reward_cash'],initial_earned_cash=base['victory']['reward_cash'],reward_exp=enemy['exp'],reward_cash=enemy['cash'],exp_text=text('BATTLE_MSG_EXP_ONE_ALLY',9,dict(name=name,value=enemy['exp'])))
 from native_content import parse_pack
 room=parse_pack((ROOT/'romfs/data/opening.encroom').read_bytes());body=[b for b in room['sections']['BodyRule']if room['strings'][b['source_path_string']]=='Objects/npcdoll'];require(len(body)==1,'Doll body binding');v['enemy_body_id']=body[0]['body_id']
 p=ir['presentation'];receipt=read(ROOT/'romfs/doll-preview/source.json');r=next(r for r in receipt['resources']if r['name']=='enemy');idx=next(m['resource']for m in p['media']if m['role']==2)
 resource=p['resources'][idx];resource.update(path=r['output'],width=r['width'],height=r['height'],columns=1,rows=1,sha256=digest(ROOT/'romfs'/r['output']));ex.data('Graphics/Battle Sprites/doll.png')
 for m in p['media']:
  if m['role']==2:m['rect'][2:]=[resource['width'],resource['height']]
 pres=Presentation(ex,p['resources'])
 for k in['media','tracks','keys','events','bindings','parameters']:setattr(pres,k,p[k])
 def rawanim(path,name,role,res,rect,value_props,method_kind):
  source=ex.text(path);rid=node(source,'AnimationPlayer')['anims/'+name]['SubResource'];body=one(r'^\[sub_resource type="Animation" id='+str(rid)+r'\]\n(.*?)(?=^\[|\Z)',source,name,re.M|re.S)[1];props=properties(body)
  m=pres.add(path+':'+name,role,res,props['length'],rect,flags=4 if role==2 else 0,anchor=(.5,.5)if role==2 else(0,0))
  tracks={}
  for key,value in props.items():
   if key.startswith('tracks/'):
    _,index,field=key.split('/');tracks.setdefault(int(index),{})[field]=value
  for tr in tracks.values():
   require(tr['enabled']and not tr['imported'],'Disabled/imported boss track');keys=tr['keys']
   if tr['type']=='value':
    require(tr['path']in value_props and tr['interp']==1,'Unreviewed boss value track');pres.track(m,value_props[tr['path']],keys['times'],keys['values'],keys.get('update',0),eases=keys['transitions'])
   elif tr['type']=='method':
    for time,value in zip(keys['times'],keys['values']):
     if value['method']=='shake':
      require(role==2 and len(value['args'])==3,'Unknown shake');mag,length,interval=value['args'];ir['boss_shakes'].append(dict(time=time,magnitude=mag,length=length,interval=interval,weight=.5))
     else:require(value=={'args':[],'method':method_kind[0]},'Unknown boss callback');pres.event(m,time,method_kind[1])
   elif tr['type']=='audio':
    times,ref=([1.55,3.05],3)if role==2 else([1],1)
    require(tr['path']=='AudioStreamPlayer'and keys['times']==times and all(c=={'end_offset':0.0,'start_offset':0.0,'stream':{'ExtResource':ref}}for c in keys['clips']),'Unreviewed boss audio track')
   else:raise ValueError('Unknown boss track')
  return m
 ir['boss_shakes']=[]
 boss=rawanim('Nodes/Ui/Battle/EnemySprite.tscn','bossDefeat',2,idx,[0,0,resource['width'],resource['height']],{'.:material:shader_param/flash_color':9,'.:material:shader_param/flash_modifier':10,'.:material:shader_param/glow_modifier':14,'.:modulate':15},('start_boss_defeat_flash',10));pres.bind('EnemyDefeat',boss)
 flash=rawanim('Nodes/Ui/Battle/BossDefeatFlash.tscn','DefeatFlash',12,0xffffffff,[0,0,entry['viewport']['width'],entry['viewport']['height']],{'ColorRect:material:shader_param/radius':18,'ColorRect:color':8},('defeat_enemies',11))
 shaker=ex.text('Scripts/misc/Shaker.gd');require('interval := 0.2, weight := 0.5, diminish := true'in shaker and 'round(rand_range(-1, 1)), round(rand_range(-1, 1))'in shaker and 'while (_dir == old__dir)'in shaker,'Unreviewed boss shake mechanism')
 system=ex.text('Scripts/UI/Battle/BattleSystem.gd');participant=ex.text('Scripts/UI/Battle/BattleParticipant.gd');sprite=ex.text('Scripts/UI/Battle/EnemySprite.gd');ex.data('Scripts/UI/Battle/BossDefeatFlash.gd')
 for f in['$BossDefeatFlash.connect("animation_finished", self, "_win")','$BossDefeatFlash.connect("defeat_enemies", self, "_kill_all_enemies")','bp.get_plate().stop_scrolling()','pause_battle()']:require(f in system,'Unreviewed boss lifecycle '+f)
 require('if is_boss() and !silent:'in participant and 'yield(_battle_sprite, "start_boss_defeat_flash")'in participant and 'if boss: $AnimationPlayer.play("bossDefeat")'in sprite,'Unreviewed boss handoff')
 pm=ex.text('Scripts/global/PartyMember.gd');character=ex.text('Scripts/global/Character.gd');skill=ex.yaml('Data/BattleSkills/telepathy.yaml')
 require('2: ["telepathy"]'in pm and skill['use_cases']==-1 and 'required_weapon'not in skill,'Unreviewed level2 learning')
 require('return int(lerp(lower_stat, upper_stat, level_units / 10.0))'in pm and 'set_hp(_hp + diff)'in character and 'set_pp(_pp + diff)'in character,'Unreviewed deterministic growth')
 stats=lamp['party']['stat_targets'];effective=lamp['party']['effective_stats'];bases=lamp['party']['base_stats'];ir['growth']=[]
 for i,(stat,targets)in enumerate(stats.items()):
  after=int(targets[0]+(targets[1]-targets[0])*.2);before=bases[stat];boost=effective[stat]-before;gain=after-before
  ir['growth'].append(dict(stat=i+1,before=effective[stat],after=after+boost,text=text('BATTLE_MSG_LEVEL_UP_STAT',11,dict(stat=trans['STAT_'+stat.upper()],value=gain))if gain else 0))
 ir['encounter']=dict(boss=int(enemy['boss']),keep_actor=int(scene['6']['startbattle']['actorskeep']['doll']),post_win_script=scene['6']['startbattle']['wincutscene'],boss_flash_media=flash['id']-1,promoted_level=2,following_level_exp=room['sections']['Experience'][2]['required_total_exp'],level_text=text('BATTLE_MSG_LEVEL_UP',10,dict(name=name,value=2)),learned_skill='telepathy',learned_text=text('BATTLE_MSG_LEARNING',12,dict(n0=party_articles[0],name=name,skill=trans[skill['name']],skillLevel='')),stop_area_music_if_overworld=return_music_policy(enemy,system))
 conditional_growth_policy(pm)
 ir['schema']=5;ir['scope']='Doll actions, retained actor, source boss callbacks, live carried session with conditional level2 growth and field Telepathy, typed post-win boundary and pre-return owned area music cleanup'
 ir['sources']=ex.sources;ir['dependencies']={path:digest(ROOT/path)for path in['content/native-battle.json','content/native-opening.json','content/doll-entry.json','content/native-round.json']}
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
