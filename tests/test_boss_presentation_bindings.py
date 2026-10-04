import copy,hashlib,json,os,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SOURCE_ROOT=Path(os.environ.get('ENCORE_SOURCE_ROOT',str(ROOT)))
sys.path.insert(0,str(SOURCE_ROOT/'tools'));sys.path.insert(0,str(ROOT/'tools'))
import boss_presentation_bindings as bindings
import doll_round,native_round
# These are identical paths after integration; the override also permits the
# ignored, isolated patch checkout to test its real adapters against the source.
doll_round.ROOT=SOURCE_ROOT
native_round.verify_sources.__defaults__=(SOURCE_ROOT,)
native_round.lower.__defaults__=(SOURCE_ROOT,True)

class BossPresentationBindings(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.recipe=bindings.load(SOURCE_ROOT)
  cls.ir=json.loads((SOURCE_ROOT/'content/doll-round.json').read_text(encoding='utf-8'))
  cls.fixture=ROOT/'build/boss-binding-tests';cls.fixture.mkdir(parents=True,exist_ok=True)
 def reject(self,edit):
  recipe=copy.deepcopy(self.recipe);edit(recipe)
  with self.assertRaises(ValueError):bindings.load(SOURCE_ROOT,recipe)
 def test_actual_extraction_and_binary_remain_identical(self):
  extracted=doll_round.build();self.assertEqual(extracted,self.ir)
  native_round.verify_sources(extracted,SOURCE_ROOT)
  self.assertEqual(native_round.encode(native_round.lower(extracted,root=SOURCE_ROOT)),(SOURCE_ROOT/'romfs/data/doll-entry.encround').read_bytes())
 def test_unknown_versions_pins_fields_and_duplicate_json(self):
  for value in [0,2,99,True,'1',None]:self.reject(lambda r:r.update(schema=value))
  self.reject(lambda r:r.update(commit='0'*40))
  for route in [(),('animations','enemy_defeat'),('animations','enemy_defeat','audio'),('animations','enemy_defeat','methods','shake'),('scene_links','battle'),('progression',),('encounter',),('texts','growth')]:
   for missing in [False,True]:
    def edit(r):
     target=r
     for key in route:target=target[key]
     if missing:target.pop(next(iter(target)))
     else:target['unknown']=1
    with self.subTest(route=route,missing=missing):self.reject(edit)
  with self.assertRaises(ValueError):json.loads('{"schema":1,"schema":1}',object_pairs_hook=bindings.unique)
 def test_unreviewed_animation_nodes_roles_properties_and_callbacks(self):
  for key,value in [('source','../unknown.tscn'),('player_node','Missing'),('clip','Missing'),('role','Unknown'),('role','BossFlash'),('resource','unknown'),('geometry','unknown'),('flags',8),('anchor',[1.1,0]),('method_node','Sprite')]:
   with self.subTest(key=key,value=value):self.reject(lambda r:r['animations']['enemy_defeat'].update({key:value}))
  self.reject(lambda r:r['animations']['enemy_defeat']['value_properties'].update({'.:material:shader_param/flash_color':8}))
  self.reject(lambda r:r['animations']['enemy_defeat']['value_properties'].pop('.:modulate'))
  self.reject(lambda r:r['animations']['enemy_defeat']['methods']['shake'].update(operation='Unknown'))
  self.reject(lambda r:r['animations']['enemy_defeat']['methods']['start_boss_defeat_flash'].update(signal='animation_finished'))
  self.reject(lambda r:r['animations']['enemy_defeat']['methods'].pop('shake'))
 def test_audio_identity_timing_streams_and_unknown_clips(self):
  for key,value in [('node','Missing'),('resource_id',4),('source','Audio/Sound effects/Boss Defeat.mp3')]:
   with self.subTest(key=key):self.reject(lambda r:r['animations']['enemy_defeat']['audio'].update({key:value}))
  self.reject(lambda r:r['animations']['enemy_defeat']['audio']['keys'].update(times=[1.56,3.05]))
  self.reject(lambda r:r['animations']['enemy_defeat']['audio']['keys']['clips'][0].update(stream={'ExtResource':4}))
  self.reject(lambda r:r['animations']['enemy_defeat']['audio']['keys']['clips'][0].update(start_offset=1))
  self.reject(lambda r:r['animations']['enemy_defeat']['audio']['keys']['clips'][0].update(end_offset=False))
  self.reject(lambda r:r['animations']['defeat_flash']['audio']['keys']['clips'][0].update(stream={'ExtResource':True}))
  self.reject(lambda r:r['animations']['enemy_defeat']['audio']['keys']['clips'].pop())
 def test_scene_instance_and_progression_stable_id_bindings(self):
  self.reject(lambda r:r['scene_links']['battle'].update(flash_node='Missing'))
  self.reject(lambda r:r['scene_links']['battle'].update(animation='enemy_defeat'))
  self.reject(lambda r:r['scene_links']['enemy'].update(animation='defeat_flash'))
  for key,value in [('battle_id',99),('battle_id',True),('actor','lamp'),('body_source','Objects/missing'),('cutscene_step','0'),('party_articles_key','UNKNOWN_ARTICLES')]:
   with self.subTest(key=key):self.reject(lambda r:r['encounter'].update({key:value}))
  for key,value in [('party_id','ana'),('learn_table','Missing'),('promoted_level',3),('following_level',4),('learned_skill','speedUpA'),('skill_source','Data/BattleSkills/attack.yaml')]:
   with self.subTest(key=key):self.reject(lambda r:r['progression'].update({key:value}))
  self.reject(lambda r:r['progression']['stat_order'].reverse())
  self.reject(lambda r:r['progression']['stat_order'].append('maxhp'))
  self.reject(lambda r:r['texts']['learning'].update(key='UNKNOWN_TEXT'))
 def test_round_resource_semantics_are_verified_before_compilation(self):
  edits=[lambda ir:ir['encounter'].update(learned_skill='unknown'),lambda ir:ir['encounter'].update(promoted_level=3),lambda ir:ir['encounter'].update(post_win_script='Podunk/unknown'),lambda ir:ir['growth'][0].update(after=66),lambda ir:ir['victory'].update(enemy_body_id=1)]
  enemy=self.ir['presentation']['bindings']['EnemyDefeat'];media=self.ir['presentation']['media'][enemy]
  edits.extend([lambda ir:ir['presentation']['media'][enemy].update(duration=6.1),lambda ir:ir['presentation']['events'][media['first_event']].update(time=4.6),lambda ir:ir['presentation']['tracks'][media['first_track']].update(property=8),lambda ir:ir['presentation']['keys'][ir['presentation']['tracks'][media['first_track']]['first']].update(ease=2),lambda ir:ir['boss_shakes'][0].update(magnitude=6)])
  for edit in edits:
   ir=copy.deepcopy(self.ir);edit(ir)
   with self.assertRaises(ValueError):bindings.check_round(ir,self.recipe,SOURCE_ROOT)
 def test_source_fingerprint_missing_facts_and_rehashed_semantics_reject(self):
  self.reject(lambda r:r['sources'].update({next(iter(r['sources'])):'0'*64}))
  self.reject(lambda r:r['sources'].update({'../escaped.gd':'0'*64}))
  self.reject(lambda r:r['source_facts'].update({r['progression']['source']:['changed source mechanism']}))
  scene=self.recipe['animations']['enemy_defeat']['source']
  changes=[(scene,'PoolRealArray( 1.55, 3.05 )','PoolRealArray( 1.56, 3.05 )'),(scene,'tracks/5/path = NodePath(".")','tracks/5/path = NodePath("Sprite")'),(self.recipe['progression']['source'],'2: ["telepathy"]','2: ["speedUpA"]')]
  for source,before,after in changes:
   with self.subTest(source=source,before=before),tempfile.TemporaryDirectory(dir=self.fixture)as directory:
    root=Path(directory);recipe=copy.deepcopy(self.recipe);inventory=json.loads((SOURCE_ROOT/'compatibility/upstream-inventory.json').read_text(encoding='utf-8'))
    for path in recipe['sources']:
     data=(SOURCE_ROOT/'upstream/MOTHER-Encore'/path).read_bytes()
     if path==source:
      self.assertIn(before.encode(),data);data=data.replace(before.encode(),after.encode());sha=hashlib.sha256(data).hexdigest();recipe['sources'][path]=sha;inventory['files'][path]['sha256']=sha
     target=root/'upstream/MOTHER-Encore'/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
    for path in ['upstream.lock','content/doll-entry.json','content/native-battle.json','romfs/data/opening.encroom']:
     target=root/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes((SOURCE_ROOT/path).read_bytes())
    target=root/'compatibility/upstream-inventory.json';target.parent.mkdir(parents=True);target.write_text(json.dumps(inventory),encoding='utf-8')
    with self.assertRaises(ValueError):bindings.load(root,recipe)
 def test_data_only_presentation_binding_changes_lower_into_real_round(self):
  recipe=copy.deepcopy(self.recipe);recipe['animations']['enemy_defeat']['anchor']=[.25,.5]
  ir=doll_round.build(recipe);media=ir['presentation']['media'][ir['presentation']['bindings']['EnemyDefeat']]
  self.assertEqual(media['anchor'],[.25,.5]);self.assertEqual(ir['encounter'],self.ir['encounter']);self.assertEqual(ir['growth'],self.ir['growth'])
  blob=native_round.encode(native_round.lower(ir,root=SOURCE_ROOT));self.assertNotEqual(blob,(SOURCE_ROOT/'romfs/data/doll-entry.encround').read_bytes());native_round.parse_pack(blob)

if __name__=='__main__':unittest.main()
