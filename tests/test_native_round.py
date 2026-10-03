import copy,hashlib,json,struct,sys,tempfile,unittest,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import native_round as n
class RoundCompilerTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.ir=json.loads((ROOT/'content/native-round.json').read_text());cls.blob=(ROOT/'romfs/data/opening.encround').read_bytes()
 def test_sources_and_deterministic_compile(self):
  n.verify_sources(self.ir);self.assertEqual(n.encode(n.lower(self.ir)),self.blob);n.parse_pack(self.blob)
 def test_actual_attack_not_similarly_named_bash(self):
  s=self.ir['skills'][self.ir['binding']['basic_skill']]
  self.assertTrue(s['source'].endswith('/attack.yaml'));self.assertEqual((s['power'],s['variance'],s['crit_chance']),(10,5,5));self.assertEqual([c['weight']for c in self.ir['enemy_choices']],[2,1])
 def test_literal_precision(self):
  t=n.parse_pack(self.blob);self.assertEqual(dict(t['Rules'])[n.RULE_NAMES.index('EnemyChoiceDelay')+1],.3);self.assertEqual(dict(t['Rules'])[n.RULE_NAMES.index('HpFrameSeconds')+1],1/30);self.assertEqual(dict(t['Rules'])[n.RULE_NAMES.index('TextSecondsPerChar')+1],.02);self.assertEqual(dict(t['Rules'])[n.RULE_NAMES.index('TextSlowerMultiplier')+1],.7)
 def test_external_text_templates(self):
  text=self.ir['texts'][self.ir['skills'][1]['dialog']];self.assertEqual(text['key'],'TACKLE_DIALOG');self.assertEqual(text['source_text'],'{n0}{name} threw {n4} body at {t1}{target}!');self.assertEqual(text['text'],'The Lamp threw its body at Ninten!')
 def test_unknown_ir_fields_fail(self):
  for route in [(),('skills',0),('binding',),('texts',0),('presentation',),('victory',)]:
   ir=copy.deepcopy(self.ir);target=ir
   for key in route:target=target[key]
   target['unreviewed']=1
   with self.subTest(route=route),self.assertRaises(n.ContentError):n.lower(ir,verify_assets=False)
 def test_changed_source_and_missing_review_fail(self):
  ir=copy.deepcopy(self.ir);path=next(iter(ir['sources']));ir['sources'][path]='0'*64
  with self.assertRaises(n.ContentError):n.verify_sources(ir)
 def test_unsupported_skill_paths_fail(self):
  for field,value in [('action_type',99),('target_type',99),('skill_type',99),('traits',2),('pp_cost',1),('hp_cost',1),('fail_chance',1),('dialog',999),('user_media',99999)]:
   ir=copy.deepcopy(self.ir);ir['skills'][0][field]=value
   with self.subTest(field=field),self.assertRaises(n.ContentError):n.parse_pack(n.encode(n.lower(ir,verify_assets=False)))
 def test_invalid_rule_fail(self):
  for field,value in [('PercentScale',.5),('HpTransitionFrames',.5),('GuardDivisor',0),('EnemyChoiceDelay',float('nan'))]:
   ir=copy.deepcopy(self.ir);ir['rules'][field]=value
   with self.subTest(field=field),self.assertRaises(n.ContentError):n.parse_pack(n.encode(n.lower(ir,verify_assets=False)))
 def test_negative_binary_headers_and_truncations(self):
  for size in [0,1,64,n.HEADER-1,len(self.blob)-1]:
   with self.subTest(size=size),self.assertRaises(n.ContentError):n.parse_pack(self.blob[:size])
  for off,value in [(8,1),(8,3),(20,13),(20,99),(24,1),(24,99),(28,1),(28,99),(52,1),(68,0),(76,99999)]:
   b=bytearray(self.blob);struct.pack_into('<I',b,off,value);struct.pack_into('<I',b,16,0);struct.pack_into('<I',b,16,zlib.crc32(b))
   with self.subTest(offset=off),self.assertRaises(n.ContentError):n.parse_pack(b)
 def test_staging_hashes_and_escape(self):
  files=n.stage_files(ROOT/'romfs');self.assertEqual(files[Path('data/opening.encround')],self.blob)
  if self.ir['presentation']['resources']:
   ir=copy.deepcopy(self.ir);ir['presentation']['resources'][0]['path']='../secret'
   with self.assertRaises(n.ContentError):n.lower(ir,verify_assets=False)
 def test_media_ownership_and_parameter_divisors(self):
  if not self.ir['presentation']['media']:return
  ir=copy.deepcopy(self.ir);ir['presentation']['media'][0]['track_count']=0
  with self.assertRaises(n.ContentError):n.parse_pack(n.encode(n.lower(ir,verify_assets=False)))
  for name,index in [('TextTiming',0),('HpDigits',0),('PartyHit',0),('PartyShown',3),('DamageGlyphGrid',3),('TargetPointerOffset',0),('SmashTiming',0),('PartyBounceMotion',2)]:
   ir=copy.deepcopy(self.ir);ir['presentation']['parameters'][name][index]=0
   with self.subTest(name=name),self.assertRaises(n.ContentError):n.parse_pack(n.encode(n.lower(ir,verify_assets=False)))
 def test_unknown_media_opcodes_and_nan(self):
  if not self.ir['presentation']['media']:return
  for table,field,value in [('media','role',99),('media','flags',8),('tracks','property',99),('tracks','interpolation',99),('tracks','mode',99),('events','kind',99),('keys','time',float('nan'))]:
   ir=copy.deepcopy(self.ir);ir['presentation'][table][0][field]=value
   with self.subTest(table=table,field=field),self.assertRaises(n.ContentError):n.parse_pack(n.encode(n.lower(ir,verify_assets=False)))
 def test_source_victory_contract(self):
  v=self.ir['victory'];self.assertEqual([v[x]for x in ['initial_exp','initial_level','initial_bank','initial_cash','initial_earned_cash']],[0,1,0,0,0]);self.assertEqual([v[x]for x in ['reward_exp','reward_cash','reward_item_count','level_cap','next_level_exp','max_exp']],[3,5,0,30,9,20925])
  self.assertEqual(v['earned_cash_flag'],'earned_cash');self.assertEqual((v['acknowledgment'],v['currency_policy']),(1,1));self.assertEqual(self.ir['rules']['VictoryBannerSeconds'],3)
  text=self.ir['texts'][v['exp_text']];self.assertEqual(text['role'],9);self.assertEqual(text['key'],'BATTLE_MSG_EXP_ONE_ALLY');self.assertEqual(text['source_text'],'{name} gained {value} exp.');self.assertEqual(text['text'],'Ninten gained 3 exp.')
  self.assertEqual(struct.unpack_from('<I',self.blob,8)[0],2);self.assertEqual(struct.unpack_from('<III',self.blob,20),(14,2,2));self.assertEqual(n.STRIDES[-1],64);self.assertEqual(v['enemy_body_id'],14)
 def test_unsupported_victory_state_and_policy(self):
  for field,value in [('enemy_body_id',0),('initial_level',0),('initial_level',30),('initial_exp',6),('reward_exp',0),('reward_exp',9),('reward_item_count',1),('level_cap',0),('level_cap',1001),('next_level_exp',0),('max_exp',8),('max_exp',0xffffffff),('initial_cash',0xffffffff),('initial_bank',0x7fffffff),('initial_earned_cash',0x7fffffff),('reward_cash',0xffffffff),('exp_text',0),('exp_text',99999),('acknowledgment',0),('acknowledgment',2),('currency_policy',0),('currency_policy',2),('earned_cash_flag',''),('earned_cash_flag',self.ir['binding']['win_flag'])]:
   ir=copy.deepcopy(self.ir);ir['victory'][field]=value
   with self.subTest(field=field,value=value),self.assertRaises(n.ContentError):n.parse_pack(n.encode(n.lower(ir,verify_assets=False)))
 def test_victory_missing_section_and_wrong_stride(self):
  for relative,value in [(2,56),(8,0)]:
   blob=bytearray(self.blob);offset=64+(len(n.STRIDES)-1)*16+relative
   struct.pack_into('<H'if relative==2 else '<I',blob,offset,value);struct.pack_into('<I',blob,16,0);struct.pack_into('<I',blob,16,zlib.crc32(blob))
   with self.subTest(relative=relative),self.assertRaises(n.ContentError):n.parse_pack(blob)
 def test_data_only_victory_text_and_duration(self):
  ir=copy.deepcopy(self.ir);ir['rules']['VictoryBannerSeconds']=2.75;ir['texts'][ir['victory']['exp_text']]['text']='Ninten gained 3 exp. [edited external data]'
  parsed=n.parse_pack(n.encode(n.lower(ir,verify_assets=False)));self.assertEqual(dict(parsed['Rules'])[n.RULE_NAMES.index('VictoryBannerSeconds')+1],2.75)
  textrow=parsed['Texts'][ir['victory']['exp_text']];self.assertTrue(parsed['Strings'][textrow[4]:].startswith(b'Ninten gained 3 exp. [edited external data]\0'))
 def test_victory_callback_contract_and_parameters(self):
  p=self.ir['presentation'];timeline=p['bindings']['ReturnTimeline'];jump=p['bindings']['PartyJumpToWorld']
  self.assertEqual([e['kind']for e in p['events']if e['media']==timeline],[4,5,6,7,8]);self.assertEqual([e['kind']for e in p['events']if e['media']==jump],[9])
  for name,index,value in [('ReturnPartyGeometry',2,0),('ReturnPartyGeometry',3,0),('ReturnPartyFrames',0,.5),('ReturnPartyFrames',1,99999),('ReturnPartyFrames',2,1),('ReturnPartyTurn',0,1),('ReturnPartyTurn',1,0),('ReturnPartyTurn',2,0),('ReturnPartyTurn',3,1),('ReturnCamera',0,0),('ReturnCamera',0,121),('ReturnCamera',1,1),('ReturnCamera',2,1),('ReturnCamera',3,1)]:
   ir=copy.deepcopy(self.ir);ir['presentation']['parameters'][name][index]=value
   with self.subTest(name=name,index=index),self.assertRaises(n.ContentError):n.parse_pack(n.encode(n.lower(ir,verify_assets=False)))
  for event in [next(i for i,e in enumerate(p['events'])if e['media']==timeline),next(i for i,e in enumerate(p['events'])if e['media']==jump)]:
   ir=copy.deepcopy(self.ir);ir['presentation']['events'][event]['kind']=3
   with self.subTest(event=event),self.assertRaises(n.ContentError):n.parse_pack(n.encode(n.lower(ir,verify_assets=False)))
  for slot in ['ReturnTimeline','ReturnTop','ReturnBottom','ReturnPlate','PartyJumpToWorld','PartyVictory','VictoryBanner']:
   ir=copy.deepcopy(self.ir);ir['presentation']['media'][p['bindings'][slot]]['role']=2
   with self.subTest(slot=slot),self.assertRaises(n.ContentError):n.parse_pack(n.encode(n.lower(ir,verify_assets=False)))
 def test_missing_victory_media_has_no_fallback(self):
  ir=copy.deepcopy(self.ir);ir['presentation']={k:({}if k in ['bindings','parameters']else[])for k in ir['presentation']}
  for skill in ir['skills']:skill['user_media']=skill['hit_media']=0xffffffff
  with self.assertRaises(n.ContentError):n.parse_pack(n.encode(n.lower(ir,verify_assets=False)))
 def test_return_camera_source_and_rejection(self):
  import extract_battle_round as extract
  from native_content import parse_pack
  room=parse_pack((ROOT/'romfs/data/opening.encroom').read_bytes())
  class Source:
   def __init__(self,changed=None):self.changed=changed or {}
   def text(self,path):return self.changed.get(path,(ROOT/'upstream/MOTHER-Encore'/path).read_text())
  self.assertEqual(extract.return_camera_parameter(Source(),room),[.3,0,0,0]);self.assertEqual(self.ir['presentation']['parameters']['ReturnCamera'],[.3,0,0,0])
  camera='Scripts/Main/Camera2D.gd';area='Nodes/Overworld/camarea.tscn';player='Nodes/Reusables/Player.tscn'
  changes=[(camera,Source().text(camera).replace('uiManager.connect("battle_to_ov", self, "_scoping_stop")','uiManager.connect("battle_ended", self, "_scoping_stop")')),(camera,Source().text(camera).replace('Tween.TRANS_SINE','Tween.TRANS_LINEAR')),(camera,Source().text(camera).replace('var _camarea_offset := Vector2.ZERO','var _camarea_offset := Vector2.ONE')),(area,Source().text(area)+'\ncamera_offset = Vector2( 1, 0 )\n'),(player,Source().text(player).replace('[node name="Camera2D" parent="." instance=ExtResource( 3 )]','[node name="Camera2D" parent="." instance=ExtResource( 3 )]\nposition = Vector2( 1, 0 )'))]
  for path,changed in changes:
   with self.subTest(path=path),self.assertRaises(ValueError):extract.return_camera_parameter(Source({path:changed}),room)
if __name__=='__main__':unittest.main()
