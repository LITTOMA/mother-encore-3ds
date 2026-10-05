"""Manual pinned factory/source lowering and malformed IR cases. Never automatic CI."""
import copy,json,unittest
from pathlib import Path
from tools import field_enemy as pack
class FieldEnemySource(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.ir=pack.read(pack.IR)
 def test_actual_generated_resource(self):self.assertEqual(pack.pack(self.ir),pack.OUTPUT.read_bytes())
 def test_dynamic_factories_and_source_order(self):
  self.assertEqual(len(self.ir['spawners']),68);self.assertEqual(len(self.ir['profiles']),14)
  self.assertEqual([s['ready_ordinal'] for s in self.ir['spawners']],sorted(s['ready_ordinal'] for s in self.ir['spawners']))
  self.assertTrue(all(s['id']==pack.stable(s['node']) for s in self.ir['spawners']))
 def test_concurrent_signal_resource(self):
  self.assertEqual(self.ir['capabilities'],2)
  self.assertEqual([a['name'] for a in self.ir['animations']],['Flash','Stun','RESET'])
  self.assertTrue(all(a['signal']=='animation_finished' for a in self.ir['animations']))
  self.assertEqual(self.ir['unsupported'],[])
 def test_reviewed_case_sensitive_override(self):
  profiles={p['enemy']:p for p in self.ir['profiles']}
  self.assertEqual(profiles['fly']['walk_frequency'],1);self.assertEqual(profiles['straydog']['walk_frequency'],1)
  self.assertEqual(profiles['fly']['ignored_source_spelling'],{'walkFrequency':.5})
 def test_malformed_versions_ids_and_geometry(self):
  for field,value in [('capabilities',1),('schema',2),('commit','0'*40),('scene_id',0)]:
   ir=copy.deepcopy(self.ir);ir[field]=value
   with self.assertRaises(ValueError):pack.pack(ir)
  for section,key,value in [('spawners','id',0),('spawners','flags',4),('spawners','appearance_rate',101),('spawners','profile',99),('profiles','stats',[-1]*8),('geometry','role',99),('geometry','flags',2),('geometry','value',[float('nan'),0]),('animations','role',99),('animations','name','Unknown'),('animations','duration',0),('animations','signal','other'),('animations','handler','other')]:
   ir=copy.deepcopy(self.ir);ir[section][0][key]=value
   with self.assertRaises(ValueError):pack.pack(ir)
if __name__=='__main__':unittest.main()
