#!/usr/bin/env python3
"""Manual source emotes coverage; not automatically registered or executed."""
import copy,unittest
from tools.field_emotes import load,validate,encode,audio_bindings
class FieldEmotesTests(unittest.TestCase):
 def setUp(self):self.data=load()
 def reject(self,change):
  d=copy.deepcopy(self.data);change(d)
  with self.assertRaises((ValueError,KeyError,TypeError)):validate(d)
 def test_actual_source_children_clips_and_tracks(self):
  d=self.data;self.assertEqual(len(d['records']),71);self.assertEqual(sum(not r['object_id']for r in d['records']),4);self.assertEqual(len(d['clips']),14);self.assertEqual(len(audio_bindings()),2);self.assertEqual(encode(d)[:8],b'ENCEMO01');self.assertEqual({t['role']for a in d['clips']for t in a['tracks']},{1,2,3,4})
  self.assertEqual({a['name']for a in d['clips']if a['direction_sensitive']},{'sweat','angry'})
 def test_negative_source_schema_topology_and_ready(self):
  self.reject(lambda d:d.update(schema=9));self.reject(lambda d:d.update(commit='0'*40));self.reject(lambda d:d['records'].pop());self.reject(lambda d:d['records'][1].update(ready_ordinal=d['records'][0]['ready_ordinal']));self.reject(lambda d:d['records'][0].update(object_id=0));self.reject(lambda d:d.update(bubble_gap=float('nan')))
 def test_negative_clip_property_easing_reference(self):
  self.reject(lambda d:d['clips'][0]['tracks'][0].update(role=9));self.reject(lambda d:d['clips'][0]['tracks'][0].update(update=9));self.reject(lambda d:d['clips'][0].update(length=0));self.reject(lambda d:d['clips'][0]['tracks'][0]['keys'][0].update(time=-1));self.reject(lambda d:d.update(default_sound=0))
if __name__=='__main__':unittest.main()
