"""Manual-only source/IR negatives for complete Podunk NPCs. Never automatic CI."""
import copy,unittest
from tools import field_npc as npc
class FieldNpcSource(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.ir=npc.read(npc.IR)
 def test_actual_generated_binary(self):self.assertEqual(npc.pack(self.ir),npc.OUT.read_bytes())
 def test_mick_source_priority_and_sprite(self):
  m=next(n for n in self.ir['npcs'] if n['node']=='Objects/NPCS/npc21')
  self.assertEqual(len(self.ir['npcs']),67);self.assertEqual(m['position'],[-88,8]);self.assertTrue(m['flags']&128)
  self.assertEqual([d['program'] for d in m['dialogues'] if d['thoughts']],['Podunk/woof_food','Podunk/woof_key','Podunk/woof_food'])
  self.assertEqual([g['role'] for g in m['geometry']],list(range(1,10)))
 def test_ir_header_identity_negatives(self):
  for key,value in [('schema',99),('commit','0'*40),('scene_id',0)]:
   d=copy.deepcopy(self.ir);d[key]=value
   with self.assertRaises(ValueError):npc.pack(d)
 def test_ir_references_and_shape_negatives(self):
  for key,value in [('id',0),('flags',4096),('extended_interact',4),('columns',0),('texture','missing'),('speed',0)]:
   d=copy.deepcopy(self.ir);d['npcs'][0][key]=value
   with self.assertRaises(ValueError):npc.pack(d)
  for key,value in [('role',99),('kind',99),('mask',65536),('value',[float('nan'),1]),('scale',[0,1])]:
   d=copy.deepcopy(self.ir);d['npcs'][0]['geometry'][0][key]=value
   with self.assertRaises(ValueError):npc.pack(d)
  d=copy.deepcopy(self.ir);d['npcs'][0]['motions'][0]['directions'][0]['keys'][0][1]=999999
  with self.assertRaises(ValueError):npc.pack(d)
if __name__=='__main__':unittest.main()
