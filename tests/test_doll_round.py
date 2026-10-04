import copy,json,struct,sys,unittest,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
import native_round as n
import doll_round as doll
class DollRoundTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.ir=json.loads((ROOT/'content/doll-round.json').read_text());cls.blob=(ROOT/'romfs/data/doll-entry.encround').read_bytes()
 def test_checked_sources_and_deterministic_binary(self):
  n.verify_sources(self.ir);self.assertEqual(n.encode(n.lower(self.ir)),self.blob);t=n.parse_pack(self.blob);self.assertEqual(struct.unpack_from('<I',self.blob,8)[0],5);self.assertEqual(len(t['Growth']),7);self.assertEqual(len(t['Encounter'][0]),10)
 def test_source_encounter_and_progression(self):
  self.assertEqual(self.ir['binding']['battle_id'],2);self.assertEqual(self.ir['binding']['win_flag'],'');self.assertEqual([x['weight']for x in self.ir['enemy_choices']],[5,1]);e=self.ir['encounter'];self.assertEqual(e['post_win_script'],'Podunk/cutscenes/doll_defeated');self.assertEqual((e['boss'],e['keep_actor'],e['learned_skill']),(1,1,'telepathy'))
  self.assertEqual([g['after']for g in self.ir['growth']],[65,27,12,12,6,6,8]);self.assertEqual([g['after']-g['before']for g in self.ir['growth']],[3,1,1,0,0,1,0]);v=self.ir['victory'];self.assertEqual((v['initial_exp'],v['reward_exp'],v['initial_bank'],v['reward_cash']),(3,8,5,10))
 def test_native_source_growth_oracle(self):
  r=json.loads((ROOT/'reports/doll-round/growth-reference/reference.json').read_text());self.assertEqual(r['engine']['string'],'3.6.2-stable (official)');self.assertEqual(r['actual_next_randi'],r['expected_next_randi']);self.assertEqual(r['learned'],['telepathy']);self.assertEqual((r['hp'],r['pp']),(54,27))
 def test_native_source_actions_oracle(self):
  cases=json.loads((ROOT/'reports/doll-round/action-reference/comparison.json').read_text());self.assertEqual(len(cases),8);self.assertTrue(all(c['match']for c in cases))
 def test_unknown_or_missing_sections_rejected(self):
  for name in ['encounter','growth','boss_shakes']:
   ir=copy.deepcopy(self.ir);del ir[name]
   with self.subTest(name=name),self.assertRaises((n.ContentError,KeyError)):n.lower(ir,verify_assets=False)
  for count in [14,16,18]:
   blob=bytearray(self.blob);struct.pack_into('<I',blob,20,count);struct.pack_into('<I',blob,16,0);struct.pack_into('<I',blob,16,zlib.crc32(blob))
   with self.subTest(count=count),self.assertRaises(n.ContentError):n.parse_pack(blob)
 def test_bad_encounter_policy_rejected(self):
  for field,value in [('boss',0),('keep_actor',2),('post_win_script','../escape'),('boss_flash_media',99999),('promoted_level',1),('following_level_exp',10),('level_text',0),('learned_skill',''),('learned_text',0),('stop_area_music_if_overworld',2),('stop_area_music_if_overworld',0xffffffff)]:
   ir=copy.deepcopy(self.ir);ir['encounter'][field]=value
   with self.subTest(field=field),self.assertRaises(n.ContentError):n.parse_pack(n.encode(n.lower(ir,verify_assets=False)))
 def test_source_return_music_policy_and_order(self):
  system=(ROOT/'upstream/MOTHER-Encore/Scripts/UI/Battle/BattleSystem.gd').read_text()
  self.assertEqual(self.ir['encounter']['stop_area_music_if_overworld'],1)
  for boss,music,expected in [(True,'',1),(True,'boss.ogg',0),(False,'',0),(False,'regular.ogg',0)]:
   self.assertEqual(doll.return_music_policy({'boss':boss,'music':music},system),expected)
  for before,after in [('elif _is_boss and audioManager.overworldBattleMusic:','elif audioManager.overworldBattleMusic:'),('musicChanger.stop_music_immediately()','musicChanger.stop_music()'),('if !audioManager.overworldBattleMusic or _is_boss:','if !audioManager.overworldBattleMusic:'),('_music = _enemy_BPs[0].character.get_data().get("music", "")','_music = "fixed.ogg"')]:
   with self.subTest(before=before),self.assertRaises(ValueError):doll.return_music_policy({'boss':True,'music':''},system.replace(before,after))
  changed=system.replace('\t\t_remove_battle_music() # Remove victory and level up tracks','\t\tpass # cleanup moved').replace('\t$AnimScene.play("transitionOut")','\t$AnimScene.play("transitionOut")\n\t_remove_battle_music()')
  with self.assertRaises(ValueError):doll.return_music_policy({'boss':True,'music':''},changed)
 def test_explicit_legacy_schema_and_unknown_rejection(self):
  legacy=copy.deepcopy(self.ir);legacy['schema']=3;del legacy['encounter']['stop_area_music_if_overworld']
  blob=n.encode(n.lower(legacy,verify_assets=False));parsed=n.parse_pack(blob)
  self.assertEqual(parsed.version,3);self.assertEqual(len(parsed['Encounter'][0]),9);self.assertEqual(n.encode(parsed),blob)
  legacy4=copy.deepcopy(self.ir);legacy4['schema']=4
  self.assertEqual(n.parse_pack(n.encode(n.lower(legacy4,verify_assets=False))).version,4)
  for schema in [1,6,99]:
   ir=copy.deepcopy(self.ir);ir['schema']=schema
   with self.subTest(schema=schema),self.assertRaises(n.ContentError):n.lower(ir,verify_assets=False)
  for schema in [2,3,6,99]:
   blob=bytearray(self.blob)
   for offset in [8,24,28]:struct.pack_into('<I',blob,offset,schema)
   struct.pack_into('<I',blob,16,0);struct.pack_into('<I',blob,16,zlib.crc32(blob))
   with self.subTest(binary_schema=schema),self.assertRaises(n.ContentError):n.parse_pack(blob)
  missing=copy.deepcopy(self.ir);del missing['encounter']['stop_area_music_if_overworld']
  with self.assertRaises(n.ContentError):n.lower(missing,verify_assets=False)
  with self.assertRaises(n.ContentError):n.lower(dict(legacy,encounter=self.ir['encounter']),verify_assets=False)
 def test_data_only_return_music_policy(self):
  changed=copy.deepcopy(self.ir);changed['encounter']['stop_area_music_if_overworld']=0
  parsed=n.parse_pack(n.encode(n.lower(changed,verify_assets=False)))
  self.assertEqual(parsed.version,5);self.assertEqual(parsed['Encounter'][0][-1],0)
 def test_bad_growth_and_shake_rejected(self):
  for field,value in [('stat',8),('before',99),('after',0),('text',0)]:
   ir=copy.deepcopy(self.ir);ir['growth'][0][field]=value
   with self.subTest(field=field),self.assertRaises(n.ContentError):n.parse_pack(n.encode(n.lower(ir,verify_assets=False)))
  for field,value in [('time',-1),('magnitude',0),('length',0),('interval',0),('weight',2)]:
   ir=copy.deepcopy(self.ir);ir['boss_shakes'][0][field]=value
   with self.subTest(field=field),self.assertRaises(n.ContentError):n.parse_pack(n.encode(n.lower(ir,verify_assets=False)))
 def test_boss_callbacks_and_data_only_media(self):
  p=self.ir['presentation'];boss=p['media'][p['bindings']['EnemyDefeat']];flash=p['media'][self.ir['encounter']['boss_flash_media']];self.assertEqual((boss['duration'],flash['duration']),(6,4.5));self.assertEqual(p['events'][boss['first_event']],{'media':boss['id']-1,'time':4.5,'kind':10});self.assertEqual(p['events'][flash['first_event']],{'media':flash['id']-1,'time':2,'kind':11})
  for m in[boss,flash]:
   ir=copy.deepcopy(self.ir);ir['presentation']['events'][m['first_event']]['kind']=1
   with self.assertRaises(n.ContentError):n.parse_pack(n.encode(n.lower(ir,verify_assets=False)))
 def test_staging_uses_companion_pack(self):
  files=n.stage_files(ROOT/'romfs',Path('data/doll-entry.encround'));self.assertEqual(files[Path('data/doll-entry.encround')],self.blob);self.assertIn(Path('graphics/battle/doll/doll-enemy.t3x'),files)
if __name__=='__main__':unittest.main()
