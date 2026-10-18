import copy,json,unittest
from tools import run_npc_sprite_reference as n,field_scene_npcs as npc
class NpcSpriteReference(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.result=json.loads((n.ROOT/'reports/mayor-office-import/npc-sprite-native.json').read_bytes())
  cls.review=json.loads((n.ROOT/'compatibility/reviews/npc-sprite-v0410.json').read_bytes())
 def test_native_source_and_frozen_projection(self):
  n.validate_review(self.review,self.review['commit']);n.validate_result(self.result)
  rows=''.join('\t'.join(str(v).lower()if type(v)is bool else str(v)for v in row)+'\n'for row in self.result['rows'])
  self.assertEqual(rows,(n.ROOT/'tests/fixtures/npc-sprite-states.tsv').read_text())
  r=[v for v in self.result['rows']if v[1]==5]
  self.assertEqual(r[3][2],1);self.assertEqual(r[4][2],51)
  # Source delayed first Talk key retains Idle, then finishes Talk before Idle.
  r=[v for v in self.result['rows']if v[1]==0]
  self.assertEqual(r[10][2:4],[1,'Talk']);self.assertEqual(r[17][2:4],[4,'Talk']);self.assertEqual(r[34][3],'Idle')
 def test_review_negative(self):
  for change in [lambda r:r.update(schema=True),lambda r:r.update(schema=2),lambda r:r.update(extra=1),lambda r:r.update(whole_file_approved=True),lambda r:r['symbols']['Scripts/Main/npc.gd'].append('interact'),lambda r:r.update(commit='0'*40),lambda r:r['sources'].pop('Scripts/Main/npc.gd')]:
   r=copy.deepcopy(self.review);change(r)
   with self.assertRaises(ValueError):n.validate_review(r,self.review['commit'])
 def test_result_negative(self):
  for change in [lambda r:r.update(schema=True),lambda r:r.update(extra=1),lambda r:r['rows'].pop(),lambda r:r['rows'][0].__setitem__(0,True),lambda r:r['rows'][0].__setitem__(2,-1),lambda r:r['rows'][0].__setitem__(3,'TalkIdle'),lambda r:r['rows'][0].__setitem__(4,float('nan')),lambda r:r['rows'][0].__setitem__(8,1),lambda r:r['godot'].update(string='other'),lambda r:r['cases'][0].update(idle='Walk')]:
   r=copy.deepcopy(self.result);change(r)
   with self.assertRaises(ValueError):n.validate_result(r)
 def test_source_animation_policy_negative(self):
  npc.validate_sprite_policy('Idle','Idle',[['Talk','Idle',2]],True)
  npc.validate_sprite_policy('Walk','Walk',[['Talk','Idle',2]],False)
  for args in [('Talk','Talk',[['Talk','Idle',2]],True),('Idle','TalkIdle',[['Talk','Idle',2]],True),('Walk','Walk',[['Talk','Idle',2]],True),('Idle','Idle',[['Talk','Idle',True]],True),('Idle','Idle',[['Talk','Idle',1]],True),('Idle','Idle',[],True),('Idle','Idle',[['Talk','Idle',2]],1)]:
   with self.assertRaises(ValueError):npc.validate_sprite_policy(*args)
if __name__=='__main__':unittest.main()
