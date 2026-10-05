#!/usr/bin/env python3
"""Manual negative cases for the source DeadBush schema; not run by this task."""
import copy,sys,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import field_dead_bush as bush
class BushSources(unittest.TestCase):
 def setUp(self):self.ir=bush.load()
 def reject(self,edit):
  ir=copy.deepcopy(self.ir);edit(ir)
  with self.assertRaises(ValueError):bush.validate(ir)
 def test_source_coverage(self):
  self.assertEqual(len(self.ir['records']),13);self.assertEqual(len(self.ir['clips']),5)
  self.assertEqual(sum(r['call_kind']==1 for r in self.ir['records']),1)
  self.assertEqual(len(self.ir['dialogues']),2)
 def test_unknown_versions_tracks_and_targets(self):
  self.reject(lambda d:d.update(schema=2))
  self.reject(lambda d:d['records'].pop())
  self.reject(lambda d:d['clips'][0]['tracks'][0].update(role=6))
  self.reject(lambda d:d['clips'][0]['tracks'][0].update(update=2))
  self.reject(lambda d:d['records'][0].update(call_kind=2))
  self.reject(lambda d:d['records'][0].update(flags=512))
  self.reject(lambda d:d['records'][0].update(notifier_id=0))
  self.reject(lambda d:d['records'][0].update(notifier_scale=[0,1]))
  self.reject(lambda d:d['records'][0].update(notifier_rect=[[0,0],[0,20]]))
 def test_source_key_bounds(self):
  self.reject(lambda d:d['clips'][0]['tracks'][0]['keys'][0].update(time=-1))
  self.reject(lambda d:d['clips'][0]['tracks'][0]['keys'][0].update(value=99999))
  self.reject(lambda d:d['records'][1].update(ready_ordinal=d['records'][0]['ready_ordinal']))
 def test_real_assets_and_pack(self):
  # Validates real tex3ds receipt; never substitutes a fabricated texture file.
  ir=self.ir;receipt=bush.texture_receipt(ir);self.assertEqual(bush.encode(ir,receipt),bush.PACK.read_bytes())
  self.assertEqual(bush.stage_files(ROOT/'romfs')[Path('data/podunk-dead-bush.encbush')],bush.PACK.read_bytes())
if __name__=='__main__':unittest.main()
