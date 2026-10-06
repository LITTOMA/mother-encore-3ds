"""Manual-only negative producer/stage policy cases; not run automatically."""
import copy,unittest
from tools.field_goods import load,validate
class FieldGoodsPolicyTests(unittest.TestCase):
 def test_unknown_schema(self):
  d=copy.deepcopy(load());d['schema']=99
  with self.assertRaises(ValueError):validate(d)
 def test_unknown_family(self):
  d=copy.deepcopy(load());d['family']=0
  with self.assertRaises(ValueError):validate(d)
 def test_nonfinite_source(self):
  d=copy.deepcopy(load());d['parameters']['Input'][0]=float('nan')
  with self.assertRaises(ValueError):validate(d)
 def test_reordered_stat_identity(self):
  d=copy.deepcopy(load());d['stat_order'][0]=d['stat_order'][1]
  with self.assertRaises(ValueError):validate(d)
if __name__=='__main__':unittest.main()
