import copy,json,struct,tempfile,unittest,zlib
from pathlib import Path
from tools import field_scene_npcs as n,field_npc_motion as codec,mick_treats as m

class SceneNpcs(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.policy,cls.records,cls.sources=n.build()
 def test_actual_source_owners(self):
  self.assertEqual([r['binding']['id']for r in self.records],[2,3,4,5,6])
  self.assertEqual([r['directions']for r in self.records],[4,4,4,4,8])
  self.assertEqual([r['declared_no_collision']for r in self.records],[True,True,True,False,False])
  self.assertEqual([r['shadow_visible']for r in self.records],[False,False,False,True,True])
  self.assertTrue(all(r['effective_collision']=='visible_in_tree'for r in self.records))
  self.assertFalse(self.records[3]['has_initial_dialogue'])
  self.assertEqual(self.records[0]['return_after_interaction_seconds'],1)
  self.assertEqual(self.records[-1]['return_after_interaction_seconds'],0)
 def test_runtime_offsets_override_authored_children(self):
  front,side=self.records[1:3]
  self.assertEqual(front['ir']['actor']['sprite_position'],[8,-620])
  self.assertEqual(side['ir']['actor']['sprite_position'],[8,-622])
  self.assertAlmostEqual(front['ir']['actor']['interact_center'][1],-584)
  self.assertAlmostEqual(side['ir']['actor']['interact_extents'][0],31.4985,places=4)
  self.assertEqual(self.records[-1]['ir']['actor']['sprite_position'],[-216,-604])
  self.assertEqual(self.records[-1]['animation'],'Data/Animations/PartyMember.yaml')
 def test_generated_motion_versions_and_frozen_source(self):
  for r in self.records:
   blob=r['blob'];self.assertEqual(struct.unpack_from('<I',blob,20)[0],6 if r['directions']==8 else 4)
   (codec.parse(blob)if r['directions']==8 else m.parse(blob,allow_motion=True))
   self.assertFalse(r['ir']['rng']);self.assertFalse(r['ir']['commands'])
  self.assertEqual(self.records[-1]['blob'],(n.ROOT/'tests/fixtures/mayor-pippi-motion.encmick').read_bytes())
 def test_eight_direction_negative_paths(self):
  original=self.records[-1]['blob']
  for offset,value in [(20,4),(20,5),(20,7),(56,0),(56,4),(60,1),(28,8),(28,1)]:
   bad=bytearray(original);struct.pack_into('<I',bad,offset,value);struct.pack_into('<I',bad,16,zlib.crc32(bad[32:]))
   with self.assertRaises(ValueError):codec.parse(bad)
  for size in (0,63,len(original)-1):
   with self.assertRaises(ValueError):codec.parse(original[:size])
 def test_strict_recipe(self):
  for mutate in [lambda p:p.update(schema=True),lambda p:p.update(extra=1),lambda p:p['actors'][0].update(id=True),lambda p:p['actors'][0].update(motion='../escape.encmick'),lambda p:p['actors'][0].update(path='missing'),lambda p:p['actors'][1].update(id=2),lambda p:p.update(npc_scene='Nodes/Overworld/Door.tscn')]:
   p=copy.deepcopy(self.policy);mutate(p)
   with tempfile.TemporaryDirectory()as d:
    path=Path(d)/'recipe.json';path.write_text(json.dumps(p),encoding='utf8')
    with self.assertRaises(ValueError):n.build(path)

 def test_candidate_output_owner(self):
  root=n.ROOT/'build'/'npc-output-test'
  for relative in ['../escape','/absolute']:
   with self.assertRaises(ValueError):n.target(root,relative)

if __name__=='__main__':unittest.main()
