#!/usr/bin/env python3
"""Manual source/producer positive and negative coverage; never auto-run."""
import copy,unittest
from tools.field_sprite_bridge import load,validate,encode,identity
class FieldSpriteProducerTests(unittest.TestCase):
 def setUp(self):self.data=load()
 def test_complete_actual_children_and_early_yaml_ready(self):
  d=self.data;self.assertEqual(len(d['records']),150);self.assertEqual(sum(x['kind']==1 for x in d['records']),67);self.assertEqual(sum(x['kind']==2 for x in d['records']),83);self.assertFalse(d['reflector']['exists']);self.assertEqual(sum(bool(x['initial_animation'])for x in d['records']),1);self.assertEqual(encode(d)[:8],b'ENCSPR01')
 def reject(self,mutation):
  d=copy.deepcopy(self.data);mutation(d)
  with self.assertRaises((ValueError,KeyError,TypeError)):validate(d)
 def test_negative_schema_pin_reflector_topology(self):
  for k,v in [('schema',9),('commit','0'*40),('kind','fixture')]:self.reject(lambda d,k=k,v=v:d.update({k:v}))
  self.reject(lambda d:d['reflector'].update(exists=True));self.reject(lambda d:d['records'].pop())
 def test_negative_duplicate_ready_resource_and_nonfinite(self):
  self.reject(lambda d:d['records'][1].update(id=d['records'][0]['id']))
  self.reject(lambda d:d['records'][1].update(ready_ordinal=d['records'][0]['ready_ordinal']))
  self.reject(lambda d:next(r for r in d['records']if r['kind']==1).update(setup_texture=0))
  self.reject(lambda d:d['records'][0].update(reflect_offset=float('nan')))
  self.reject(lambda d:next(r for r in d['records']if r['kind']==2).update(target_id=0))
 def test_negative_motion_and_transition(self):
  self.reject(lambda d:d['animations'][0]['motions'][0]['directions'][0]['keys'][0].__setitem__(1,100000))
  self.reject(lambda d:d['animations'][0]['motions'][0]['directions'][0].update(duration=0))
  self.reject(lambda d:next(r for r in d['records']if r['connections'])['connections'][0].__setitem__(2,9))
if __name__=='__main__':unittest.main()
