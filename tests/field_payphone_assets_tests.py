"""Manual source policy/parser negative cases. Not run by this task."""
import copy,sys,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import field_payphone as phone
class Sources(unittest.TestCase):
 def setUp(self):self.ir=phone.load()
 def reject(self,change):
  d=copy.deepcopy(self.ir);change(d)
  with self.assertRaises(ValueError):phone.validate(d)
 def test_exact_source_roster(self):
  self.assertEqual(len(self.ir['records']),4)
  self.assertEqual(len({r['sprite_id']for r in self.ir['records']}),4)
 def test_unknown_schema_fields_and_scope(self):
  self.reject(lambda d:d.update(schema=2));self.reject(lambda d:d.update(ignored_unknown=True))
  self.reject(lambda d:d['records'].pop());self.reject(lambda d:d['records'][0].update(unknown_ready=True))
  self.reject(lambda d:d['records'][0].update(item='UnreviewedCard'))
  self.reject(lambda d:d['records'][0].update(sprite_flags=8))
 def test_timer_doses_and_duplicate_identity(self):
  self.reject(lambda d:d.update(update_seconds=float('nan')))
  self.reject(lambda d:d.update(close_seconds=0));self.reject(lambda d:d.update(card_step=d['card_max_doses']+1))
  self.reject(lambda d:d['records'][1].update(id=d['records'][0]['id']))
 def test_real_pack_and_atlas(self):
  a=phone.receipt(self.ir);self.assertEqual(phone.encode(self.ir,a),phone.PACK.read_bytes())
  self.assertEqual(phone.stage_files(ROOT/'romfs')[Path('data/podunk.encpayphone')],phone.PACK.read_bytes())
if __name__=='__main__':unittest.main()
