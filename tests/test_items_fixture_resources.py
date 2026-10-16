import copy,json,unittest
from pathlib import Path
from tests.items_binding_fixtures import base_resources
ROOT=Path(__file__).resolve().parents[1]
class ItemFixtureResources(unittest.TestCase):
 def setUp(self):self.ir=json.loads((ROOT/'content/native-items.json').read_text())
 def test_exact_checked_prefix_and_independent_copy(self):
  result=base_resources(self.ir);self.assertEqual(len(result),12);self.assertEqual(len(self.ir['resources']),14)
  result[0]['path']='changed';self.assertNotEqual(result[0],self.ir['resources'][0])
 def test_no_unknown_or_duplicate_extension_discarded(self):
  for change in (lambda r:r.pop(),lambda r:r.append(copy.deepcopy(r[-1])),lambda r:r[-1].update(sha256='0'*64),lambda r:r[-1].update(path='unknown.t3x'),lambda r:r[0].update(path='unknown-prefix.t3x')):
   candidate=copy.deepcopy(self.ir);change(candidate['resources'])
   with self.assertRaises(ValueError):base_resources(candidate)
if __name__=='__main__':unittest.main()
