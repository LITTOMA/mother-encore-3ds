#!/usr/bin/env python3
"""Manual source and malformed capability checks. Not automatically registered."""
import copy,sys,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import field_dropped as tool
class SourceTests(unittest.TestCase):
 def test_source_bundle(self):
  d=tool.load();self.assertEqual(len(d['bindings']),3);self.assertEqual(d['initial_speed'],0);self.assertEqual([i['keyitem']for i in d['items']],[True,True,False]);self.assertEqual(d['timers'],[5.,2.,2.,3.]);self.assertEqual(d['speeds'],[2.,3.]);self.assertEqual(tool.encode(d),(ROOT/'romfs/data/podunk-dropped.encdrop').read_bytes());self.assertNotIn(b'\\GameDev\\',tool.IR.read_bytes());self.assertNotIn(b'private-podunk',tool.REVIEW.read_bytes())
 def test_owner_and_ready_fail_closed(self):
  for change in [lambda d:d.update(schema=2),lambda d:d.update(commit='0'*40),lambda d:d['bindings'][0].update(id=d['bindings'][1]['id']),lambda d:d['bindings'][0].update(sparkles_ready=d['bindings'][0]['ready_ordinal']),lambda d:d['bindings'][0].update(item='unreviewed'),lambda d:d['bindings'][0].update(position=[float('nan'),0])]:
   d=copy.deepcopy(tool.load());change(d)
   with self.assertRaises((ValueError,KeyError)):tool.validate(d)
if __name__=='__main__':unittest.main()
