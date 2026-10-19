import copy,json,unittest
from tools import music_programs as r
class MusicProgramsTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.recipe,cls.proof=r.original()
 def test_source_mapping_and_exact_binary(self):
  self.assertEqual(self.recipe,json.loads(r.RECIPE.read_bytes()))
  self.assertEqual(self.proof,json.loads((r.ROOT/'compatibility/reviews/music-programs-v0410.json').read_bytes()))
  self.assertFalse(self.proof['release_staged'])
  self.assertEqual(r.encode(self.recipe),(r.ROOT/'tests/fixtures/music-programs.encmop').read_bytes())
  self.assertEqual([v['music']for v in self.recipe['encounters'][:3]],['','',''])
  self.assertTrue(self.recipe['encounters'][3]['music'].endswith('/Battle Themes/Flippant Foe.mp3'))
  self.assertEqual([v['delay_idle_frames']for v in self.recipe['programs']],[0]*7+[1]+[0]*6)
 def test_unknown_recipe_id_source_and_operand_types_reject(self):
  changes=[lambda p:p.update(schema=True),lambda p:p.update(kind='other'),lambda p:p.update(source_commit='0'*40),lambda p:p.update(extra=1),lambda p:p['programs'].clear(),lambda p:p['programs'].append(copy.deepcopy(p['programs'][0])),lambda p:p['programs'][0].update(id=True),lambda p:p['programs'][0].update(delay_idle_frames=2),lambda p:p['programs'][0]['instructions'][0].update(op='other'),lambda p:p['programs'][0]['instructions'][0].update(skip=0),lambda p:p['programs'][0]['instructions'][0].update(skip=999),lambda p:p['programs'][0]['instructions'][0]['target'].update(kind='other'),lambda p:p['programs'][0]['instructions'][0]['args'].__setitem__(0,{'number':0}),lambda p:p['programs'][0]['instructions'][1]['args'].__setitem__(0,{'number':-1}),lambda p:p['programs'][2]['instructions'][-1]['args'].__setitem__(0,{'number':25}),lambda p:p['programs'][2]['instructions'][-1]['args'].__setitem__(0,{'number':float('inf')}),lambda p:p['programs'][2]['instructions'][-1]['args'].__setitem__(0,{'gain':False}),lambda p:p['programs'][3]['instructions'][0]['args'].__setitem__(0,{'argument':7}),lambda p:p['encounters'].append(copy.deepcopy(p['encounters'][0]))]
  for change in changes:
   p=copy.deepcopy(self.recipe);change(p)
   with self.assertRaises((ValueError,KeyError)):r.encode(p)
 def test_opcode_signatures_fail_before_producing_pack(self):
  for index in range(len(self.recipe['programs'])):
   p=copy.deepcopy(self.recipe);p['programs'][index]['instructions'][-1]['args'].append({'number':0})
   with self.assertRaises(ValueError):r.encode(p)
if __name__=='__main__':unittest.main()
