"""Manual actual source Tint resource/schema cases; never automatic."""
import copy,unittest
from tools import field_tint as p
class FieldTintTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.ir=p.load()
 def test_actual_resource(self):
  self.assertEqual(p.PACK.read_bytes(),p.encode(self.ir));self.assertEqual(sum(r['kind']==1 for r in self.ir['records']),87);self.assertEqual({r['kind']for r in self.ir['records']},{1,2,3});self.assertTrue(all(r['ready_ordinal']==0xffffffff for r in self.ir['records']if r['kind']!=1))
 def test_reject_schema_and_targets(self):
  for edit in [lambda x:x.update(commit='0'*40),lambda x:x['records'][0].update(kind=9),lambda x:x['records'][0]['targets'][0].update(initial_self_modulate=[float('nan')]*4),lambda x:x['records'][0]['targets'][0].update(source_id=0)]:
   x=copy.deepcopy(self.ir);edit(x)
   with self.assertRaises(ValueError):p.validate(x)
 def test_reject_duplicate_source_identity(self):
  x=copy.deepcopy(self.ir);x['records'][1]['id']=x['records'][0]['id']
  with self.assertRaises(ValueError):p.validate(x)
if __name__=='__main__':unittest.main()
