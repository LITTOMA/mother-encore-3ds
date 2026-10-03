#!/usr/bin/env python3
"""Explicit, bounded extractor for the reviewed first Lamp battle round.

This is not a GDScript interpreter. Selectors below describe reviewed source
mechanisms and reject changes; the ordinary compiler never runs this extractor.
"""
import argparse,csv,hashlib,io,json,re,sys
from pathlib import Path
from extract_battle_entry import Extractor, PIN, ROOT, one, require, node

SKILLS=['attack','tackle','float','guard']
SYSTEM='Scripts/UI/Battle/BattleSystem.gd'
PLATE='Scripts/UI/Battle/PartyInfoPlate.gd'
SOURCE_LIST=[SYSTEM,PLATE,'Scripts/UI/Battle/BattleParticipant.gd','Scripts/global/Character.gd','Scripts/global/Enemy.gd','Scripts/global/PartyMember.gd','Scripts/global/globalData.gd','Scripts/UI/Battle/EnemySkill.gd','Scripts/UI/DialogueBox.gd','Scripts/global/text_tools.gd','Data/Battlers/lamp.yaml','Data/save_new_game.yaml','Data/save_overrides.yaml','Data/Items/BaseballCap.yaml','Scripts/UI/Battle/BattleDialogueBox.gd','Scripts/UI/Battle/BattleItemPool.gd','Scripts/UI/AbstractDialogueBox.gd','Scripts/global/uiManager.gd']
RULE_NAMES=['DefenseDivisor','GuardDivisor','MinimumDamage','GutsDivisor','MinimumCritPercent','PercentScale','AdrenalineMultiplier','SmashMultiplier','EnemyChoiceDelay','ActionStartDelay','TargetEndDelay','ActionEndDelay','RoundEndDelay','DefeatDialogDelay','HpFrameSeconds','HpTransitionFrames','HpBaseSpeed','HpDefendingMultiplier','HpFastMultiplier','TextSecondsPerChar','TextAcceptMultiplier','TextCancelMultiplier','TextAutoAdvanceSeconds','TextNormalMultiplier','TextFasterMultiplier','TextSlowerMultiplier','SmashTimeScale','SmashRealSeconds','VictoryBannerSeconds']

def return_camera_parameter(ex,room):
 camera=ex.text('Scripts/Main/Camera2D.gd')
 area=ex.text('Scripts/Main/camarea.gd')
 area_scene=ex.text('Nodes/Overworld/camarea.tscn')
 player=ex.text('Nodes/Reusables/Player.tscn')
 camera_scene=ex.text('Nodes/Ui/Camera.tscn')
 scene=room['strings'][room['sections']['Scene'][0]['source_scene_string']]
 require(scene.startswith('res://'),'Unsupported camera room source')
 house=ex.text(scene[6:])
 require('uiManager.connect("battle_to_ov", self, "_scoping_stop")'in camera,'Unreviewed camera return signal')
 stop=one(r'func _scoping_stop\(\):([\s\S]*?)(?=\nfunc )',camera,'camera scoping stop')[1]
 duration=float(one(r'return_offset\(([0-9.]+)\)',stop,'battle camera return duration')[1])
 returning=one(r'func return_offset\(time := 1.0\):([\s\S]*?)(?=\nfunc )',camera,'camera return offset')[1]
 for fragment in ['if tween: tween.kill()','create_tween().set_trans(Tween.TRANS_SINE).set_ease(Tween.EASE_OUT)','tween.tween_property(self, "_base_offset", Vector2.ZERO, time)','tween.parallel().tween_property(self, "position", _camarea_offset, time)','position = _camarea_offset','_base_offset = Vector2.ZERO']:
  require(fragment in returning,'Unreviewed return camera mechanism: '+fragment)
 require('var _camarea_offset := Vector2.ZERO'in camera and 'var _base_offset := Vector2.ZERO'in camera,'Unreviewed camera default offset')
 require(re.search(r'^export var camera_offset: Vector2\s*$',area,re.M)and 'global.currentCamera.set_camarea_offset(camera_offset)'in area,'Unreviewed camarea offset binding')
 require(not re.search(r'^camera_offset\s*=',house+'\n'+area_scene,re.M),'Opening camarea offset outside bounded zero path')
 for defaults in [node(player,'Camera2D'),node(camera_scene,'.')]:
  require(defaults.get('position',[0,0])==[0,0]and defaults.get('offset',[0,0])==[0,0],'Opening camera default position/offset changed')
 return [duration,0,0,0]

def build(root=ROOT,presentation=None):
 ex=Extractor(root);root=Path(root)
 source={p:ex.text(p) for p in SOURCE_LIST}
 sysrc=source[SYSTEM];plate=source[PLATE]
 entry=json.loads((root/'content/native-battle.json').read_text())
 require(entry['commit']==PIN and entry['party']['basic_skill_id']=='attack','Unreviewed initial basic skill')
 require(not entry['party']['statuses'] and not entry['party']['initial_save_data']['status'] and not entry['party']['usable_skills'] and not entry['party']['encore_enabled'],'Unsupported initial statuses, actions or Encore')
 require(all(value==1 for value in entry['party']['initial_save_data']['affinity_multipliers'].values()),'Unsupported initial affinity')
 require(not entry['enemy']['defaults']['passive_skills'] and not entry['enemy']['defaults']['affinity_multipliers'],'Unsupported enemy passive/affinity')
 for p,sha in entry['sources'].items():
  if p in ['Data/Dialogue/Podunk/cutscenes/lamp_attack.yaml','Data/save_new_game.yaml','Data/save_overrides.yaml','Data/Items/BaseballCap.yaml']:
   require(hashlib.sha256(ex.data(p)).hexdigest()==sha,'Entry source mismatch: '+p)
 require('return globaldata.SKILL_ATTACK' in source['Scripts/global/PartyMember.gd'],'Basic skill mechanism changed')
 require('const SKILL_ATTACK := "attack"' in source['Scripts/global/globalData.gd'],'Basic skill binding changed')
 translations={}
 for table in ['battleskills','battletext','battlers']:
  for row in csv.DictReader(io.StringIO(ex.text('Translations/TranslatedText/'+table+' - sheet.csv'))):
   require(row['key'] not in translations,'Duplicate translation key '+row['key']);translations[row['key']]=row['en']
 texts=[dict(id=1,role=0,key='',source_text='',text='')]
 def text(key,role,values=None,literal=None):
  raw=translations.get(key,key) if literal is None else literal
  result=raw
  for token,value in (values or {}).items():result=result.replace('{'+token+'}',value)
  require(not re.search(r'\{[^}]+\}',result),'Unhandled localization token: '+result)
  texts.append(dict(id=len(texts)+1,role=role,key=key,source_text=raw,text=result));return len(texts)-1
 enemy=ex.yaml('Data/Battlers/lamp.yaml');player=entry['party']['initial_save_data']['name']
 require(set(enemy)=={'name','description','article','maxhp','hp','maxpp','pp','offense','defense','speed','iq','guts','level','exp','cash','skills','boss','sprite','music','bg'},'Unreviewed battler fields')
 ename=translations[enemy['name']];articles=translations[enemy['article']].split(',')
 party_articles=translations['ARTICLES_'+entry['party']['id'].upper()].split(',')
 fail_default=int(one(r'_chance_roll\(action.skill.get\("fail_chance", (\d+)\)\)',sysrc,'failure default')[1])
 crit_default=int(one(r'var crit_chance = action.skill.get\("crit_chance", (\d+)\)',sysrc,'critical default')[1])
 player_context={'name':player,'n0':party_articles[0],'n4':party_articles[4],'target':ename,'t0':articles[0],'t1':articles[1]}
 enemy_context={'name':ename,'n0':articles[0],'n4':articles[4],'target':player,'t0':party_articles[0],'t1':party_articles[1]}
 allowed={'name','description','dialog','skill_type','action_type','use_cases','damage_type','target_type','damage_or_heal','variance','priority','miss_chance','pp_cost','hp_cost','crit_chance','fail_chance','value_type','hit_effect','pre_hit_effect','user_anim','use_sound','hit_sound','traits'}
 skills=[]
 for index,name in enumerate(SKILLS):
  path='Data/BattleSkills/'+name+'.yaml';s=ex.yaml(path)
  require(not(set(s)-allowed),'Unreviewed skill fields: '+name)
  require(s.get('value_type','normal')=='normal' and s.get('fail_chance',fail_default)==0,'Unreviewed value type/failure path')
  require(s['pp_cost']==0 and s['hp_cost']==0,'Unreviewed skill costs')
  require(s['use_cases']==1 and s.get('traits',[]) in ([],['guard']),'Unreviewed skill case/traits')
  require(s['action_type'] in [0,4] and s['target_type'] in [0,5] and s['skill_type'] in ['','basic','skill'],'Unreviewed action classification')
  require(s.get('damage_type','') in ['','normal'] and not s['pre_hit_effect'] and not s['use_sound'],'Unreviewed skill effect')
  ctx=player_context if index in [0,3] else enemy_context
  skills.append(dict(id=index+1,source=path,name=text(s['name'],1),description=text(s['description'],2),dialog=text(s['dialog'],3,ctx) if s['dialog'] else 0,action_type=s['action_type'],target_type=s['target_type'],skill_type={'':0,'basic':1,'skill':2}[s['skill_type']],damage_type={'':0,'normal':1}[s.get('damage_type','')],traits=int('guard'in s.get('traits',[])),power=s['damage_or_heal'],variance=s['variance'],priority=s['priority'],miss_chance=s['miss_chance'],pp_cost=s['pp_cost'],hp_cost=s['hp_cost'],crit_chance=s.get('crit_chance',crit_default),user_media=4294967295,hit_media=4294967295,fail_chance=s.get('fail_chance',fail_default)))
 def num(pattern,label,source=sysrc):return float(one(pattern,source,label)[1])
 rules={
 'DefenseDivisor':num(r'var def = defense / ([0-9.]+)','defense divisor'),
 'GuardDivisor':num(r'if target.defending: val /= ([0-9.]+)','guard divisor'),
 'MinimumDamage':num(r'val = max\(val, ([0-9.]+)\) # At this point','minimum damage'),
 'GutsDivisor':num(r'const GUTS_MULT := ([0-9.]+)','guts divisor'),
 'MinimumCritPercent':num(r'var min_chance = ([0-9.]+) if guts','minimum crit'),
 'PercentScale':num(r'var max_chance = guts/GUTS_MULT \* ([0-9.]+)','percentage scale'),
 'AdrenalineMultiplier':num(r'const ADRENALINE_MULT := ([0-9.]+)','adrenaline multiplier'),
 'SmashMultiplier':num(r'const SMASH_MULT := ([0-9.]+)','smash multiplier'),
 'EnemyChoiceDelay':num(r'if with_enemy_delay:\s+yield\(get_tree\(\).create_timer\(([0-9.]+)\)','enemy choice delay'),
 'ActionStartDelay':num(r'yield\(get_tree\(\).create_timer\(([0-9.]+)\), "timeout"\)\s+if !_active: return\s+_apply_confusion','action start delay'),
 'TargetEndDelay':num(r'yield\(get_tree\(\).create_timer\(([0-9.]+)\), "timeout"\)\s+if miss and action.target_type','target end delay'),
 'ActionEndDelay':num(r'if action.get_dialog\(\) != "":\s+yield\(get_tree\(\).create_timer\(([0-9.]+)\)','action end delay'),
 'RoundEndDelay':num(r'emit_signal\("round_done", _turns_count\)\s+yield\(get_tree\(\).create_timer\(([0-9.]+)\)','round end delay'),
 'DefeatDialogDelay':num(r'if _show_intro_outro:\s+yield\(get_tree\(\).create_timer\(([0-9.]+)\)','defeat dialog delay'),
 'HpFrameSeconds':1/num(r'var _frame_time = 1.0/([0-9.]+)','HP frame time',plate),
 'HpTransitionFrames':num(r'const TRANSITION_FRAMES := ([0-9.]+)','HP transition frames',plate),
 'HpBaseSpeed':num(r'var base_scroll_speed := ([0-9.]+)','HP base speed',plate),
 'HpDefendingMultiplier':num(r'const SCROLL_SPEED_DEFENDING_MULT := ([0-9.]+)','HP guard multiplier',plate),
 'HpFastMultiplier':num(r'const SCROLL_SPEED_FAST_MODE_MULT := ([0-9.]+)','HP fast multiplier',plate)}
 require(list(rules)==RULE_NAMES[:19],'Rule schema drift')
 from native_content import parse_pack
 room=parse_pack((root/'romfs/data/opening.encroom').read_bytes());battle=room['sections']['Battle'][0]
 binding=dict(battle_id=battle['stable_id'],player_participant=0,enemy_participant=1,basic_skill=0,guard_skill=3,basic_menu=1,items_menu=2,guard_menu=3,locale='en',enemy_name=text(enemy['name'],4),enemy_article=text(enemy['article'],5),enemy_outro=text('',6,enemy_context,literal=one(r'else "(\{n0\}\{name\} became tame!)"',sysrc,'default enemy outro')[1]),mortal_damage=text('BATTLE_MSG_MORTAL_DAMAGE',7,enemy_context),no_effect=text('BATTLE_MSG_TARGET_NO_EFFECT',8,player_context),show_intro_outro=int(entry['entry']['show_intro_outro']),win_flag=entry['entry']['win_flag'])
 # Menu IDs are compiler bindings to the existing independent battle-entry resource.
 from native_battle import parse_sections
 import struct
 sections=parse_sections((root/'romfs/data/opening.encbattle').read_bytes())
 mids=[struct.unpack_from('<4I',sections[7],i)[0] for i in range(0,len(sections[7]),16)]
 require(len(mids)==len(entry['menu']['actions'])==3,'Unreviewed first menu')
 menus={v['id']:mids[i]for i,v in enumerate(entry['menu']['actions'])}
 binding.update(basic_menu=menus['Basic'],items_menu=menus['Items'],guard_menu=menus['Defend'])
 choices=[dict(skill=SKILLS.index(s['skill']),weight=s['weight'])for s in enemy['skills']]
 media=json.loads(Path(presentation or root/'reports/battle-victory-presentation/presentation.json').read_text())
 require(media['schema']==1,'Presentation schema')
 require('ReturnCamera'not in media['parameters'],'ReturnCamera must be derived by the victory extractor')
 media['parameters']['ReturnCamera']=return_camera_parameter(ex,room)
 params=media['parameters'];text_timing=params['TextTiming'];text_speeds=params['TextTagSpeeds'];smash=params['SmashTiming']
 rules.update(zip(RULE_NAMES[19:28],[*text_timing,*text_speeds[:3],*smash[:2]]))
 rules['VictoryBannerSeconds']=num(r'func play_win\(\):[\s\S]*?create_timer\(([0-9.]+)\)', 'victory banner timer',source['Scripts/UI/Battle/BattleDialogueBox.gd'])
 require(list(rules)==RULE_NAMES,'Complete rule schema drift')
 for p,sha in media['sources'].items():require(hashlib.sha256(ex.data(p)).hexdigest()==sha,'Presentation source mismatch '+p)
 for name,refs in media['skill_media'].items():
  require(name in SKILLS and set(refs)=={'user_media','hit_media'},'Presentation skill media binding')
  skills[SKILLS.index(name)].update(refs)
 # The one conscious party member starts below the next level threshold. This
 # adapter must refuse items, level-up/skill learning and altered entry state.
 fresh=ex.yaml('Data/save_new_game.yaml');initial=entry['party']['initial_save_data']
 require(fresh['party']==[entry['party']['id']] and initial['level']==fresh[entry['party']['id']]['level'] and initial['exp']==fresh[entry['party']['id']]['exp'],'Unreviewed victory initial party/progression')
 require(not enemy.get('items',[]) and not enemy['boss'],'Unsupported victory reward pool or boss')
 pm=source['Scripts/global/PartyMember.gd'];cap=int(one(r'^const LEVEL_CAP := (\d+)',pm,'level cap')[1])
 require('int(level * level * (level + 1) * .75)'in pm and '_set_exp(max(dict["exp"], _level_to_exp(dict["level"])) as int, false)'in pm,'Unreviewed progression initialization')
 levels=room['sections']['Experience'];require(len(levels)==cap,'Room progression cap changed')
 # This table is already separately exported in opening.encroom; reference
 # totals here allow the world consumer to cross-check the dependent packs.
 xp=[r['required_total_exp'] for r in levels]
 require(xp==[0 if level==1 else int(level*level*(level+1)*.75)for level in range(1,cap+1)],'Room progression source mismatch')
 level=initial['level'];initial_exp=max(initial['exp'],xp[level-1])
 require(0<level<cap and initial_exp+enemy['exp']<xp[level],'Unsupported victory level-up')
 abstract=source['Scripts/UI/AbstractDialogueBox.gd']
 require('elif btn_next:\n\t\tif _finished:\n\t\t\t_next_phrase()'in abstract and 'var btn_next = event.is_action_pressed("ui_accept") or event.is_action_pressed("ui_cancel")'in abstract,'Unreviewed player acknowledgment')
 require('$Dialoguebox.set_auto_advance(false)'in sysrc,'Unreviewed victory auto-advance')
 reward_block=one(r'func _do_rewards\(items_to_overworld: Array\):([\s\S]*?)(?=\nfunc )',sysrc,'reward function')[1]
 require('globaldata.set_flag(_win_flag, true)'in reward_block and sysrc.index('var in_progress = _do_rewards(items_to_overworld)')<sysrc.index('\n\t_give_cash()'),'Unreviewed reward flag/cash ordering')
 cash=one(r'func give_cash_to_bank\(amount: int\):\s+globaldata.earned_cash \+= amount\s+globaldata.bank \+= amount\s+globaldata.set_flag\("([^"\n]+)", true, false\)',source['Scripts/global/uiManager.gd'],'cash bank/counter/flag')[1]
 require(cash in [room['strings'][r['name_string']]for r in room['sections']['Flag']],'Cash flag missing from room pack')
 itempool=source['Scripts/UI/Battle/BattleItemPool.gd'];require('if candidates.empty(): return' in itempool and 'return enemy.character.get_data().get("items", [])'in itempool,'Unreviewed empty reward pool')
 scene_source=room['strings'][room['sections']['Scene'][0]['source_scene_string']]
 body_path='Objects/'+room['strings'][battle['enemy_string']]
 require(scene_source.startswith('res://'),'Unsupported room source scheme')
 source_node=node(ex.text(scene_source[6:]),body_path)
 actor=room['sections']['ActorInstance'][battle['actor_instance_index']]
 require(source_node['position']==actor['position'],'Battle world actor source binding changed')
 bodies=[r for r in room['sections']['BodyRule']if room['strings'][r['source_path_string']]==body_path]
 require(len(bodies)==1 and bodies[0]['initially_enabled'],'Missing or unsupported enemy collision body')
 victory=dict(initial_exp=initial_exp,initial_level=level,initial_bank=fresh['bank'],initial_cash=fresh['cash'],initial_earned_cash=fresh['earned_cash'],reward_exp=enemy['exp'],reward_cash=enemy['cash'],reward_item_count=len(enemy.get('items',[])),level_cap=cap,next_level_exp=xp[level],max_exp=xp[-1],exp_text=text('BATTLE_MSG_EXP_ONE_ALLY',9,{'name':player,'value':str(enemy['exp'])}),acknowledgment=1,currency_policy=1,earned_cash_flag=cash,enemy_body_id=bodies[0]['body_id'])
 return dict(schema=2,kind='encore.native-battle-round.source-ir',commit=PIN,scope='original single-party Lamp battle, no-level-up victory, empty item pool, source rewards and acknowledged world return',sources=ex.sources,dependencies={path:hashlib.sha256((root/path).read_bytes()).hexdigest()for path in ['content/native-battle.json','content/native-opening.json']},skills=skills,enemy_choices=choices,rules=rules,binding=binding,texts=texts,victory=victory,presentation={k:v for k,v in media.items() if k not in ['schema','sources','skill_media']})

def main():
 p=argparse.ArgumentParser();p.add_argument('--out',type=Path,default=ROOT/'content/native-round.json');p.add_argument('--presentation',type=Path);a=p.parse_args()
 try:
  ir=build(ROOT,a.presentation);a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_text(json.dumps(ir,indent=2,ensure_ascii=False)+'\n')
  report=ROOT/'reports/battle-victory-data';report.mkdir(parents=True,exist_ok=True)
  (report/'source-review.json').write_text(json.dumps({'schema':2,'format':2,'capabilities':2,'rules':2,'commit':PIN,'scope':ir['scope'],'sources':ir['sources'],'dependencies':ir['dependencies'],'review':'Retains reviewed action semantics; extends the existing external pack with a bounded victory singleton, source EXP label, empty reward pool, cash/bank accounting policy, source acknowledgment rule, progression references, victory animation and transition callbacks. World must cross-check progression and flag bindings before writes. No level-up, items, new encounter, general script compatibility or audio playback is implied.'},indent=2)+'\n')
  print('Extracted round IR:',len(ir['sources']),'reviewed sources')
 except (OSError,ValueError,KeyError,TypeError)as e:print('ROUND EXTRACTION ERROR:',e,file=sys.stderr);return 1
 return 0
if __name__=='__main__':raise SystemExit(main())
