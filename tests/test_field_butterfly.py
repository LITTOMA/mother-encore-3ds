#!/usr/bin/env python3
"""Manual-only source and unknown binding checks, never run on commit."""
import copy,sys,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import field_butterfly as tool
class SourceTests(unittest.TestCase):
 def test_actual_source_bundle(self):
  d=tool.load();self.assertEqual(len(d['bindings']),94);self.assertEqual(d['modulus'],5);self.assertEqual(d['frames'],2);self.assertEqual(d['fly_seek'],0.3);self.assertEqual(d['orbit_seek'],20);self.assertTrue(d['pixel_snap']);self.assertEqual(tool.encode(d),(ROOT/'romfs/data/podunk-butterflies.encfly').read_bytes());self.assertNotIn(b'private-podunk',tool.REVIEW.read_bytes());self.assertTrue(all(b['timer_wait']==1 for b in d['bindings']))
 def test_source_binding_fail_closed(self):
  for change in [lambda d:d.update(schema=2),lambda d:d.update(commit='0'*40),lambda d:d['bindings'][0].update(id=d['bindings'][1]['id']),lambda d:d['bindings'][0].update(timer_ordinal=d['bindings'][0]['ready_ordinal']),lambda d:d['bindings'][0].update(position=[float('nan'),0])]:
   d=copy.deepcopy(tool.load());change(d)
   with self.assertRaises((ValueError,KeyError)):tool.validate(d)
if __name__=='__main__':unittest.main()
