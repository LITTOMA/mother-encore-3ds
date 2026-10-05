"""Manual source/IR negatives; not registered for automatic CI."""
import copy,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.field_cash_box import load,validate,stage_files,ROOT
class CashBoxTests(unittest.TestCase):
 def setUp(self):self.source=load()
 def rejected(self,change):
  d=copy.deepcopy(self.source);change(d)
  with self.assertRaises((ValueError,KeyError,TypeError)):validate(d)
 def test_actual_staging(self):self.assertGreater(len(stage_files(ROOT/'romfs')),1)
 def test_unknown_top(self):self.rejected(lambda d:d.update(unknown=True))
 def test_unknown_node(self):self.rejected(lambda d:d['boxes'][0]['nodes'][0].update(ignore_script=True))
 def test_unknown_style(self):self.rejected(lambda d:d['boxes'][0]['styles'][0].update(unchecked_region=True))
 def test_unknown_animation(self):self.rejected(lambda d:d['boxes'][0]['clips'][0].update(events=[]))
 def test_missing_animation_zero(self):self.rejected(lambda d:d['boxes'][0]['clips'][0]['keys'][0].update(time=.01))
 def test_nonfinite_key(self):self.rejected(lambda d:d['boxes'][0]['clips'][0]['keys'][0].update(ease=float('nan')))
 def test_cycle(self):self.rejected(lambda d:d['boxes'][0]['nodes'][1].update(parent=1))
 def test_missing_box(self):self.rejected(lambda d:d['boxes'].pop())
if __name__=='__main__':unittest.main()
