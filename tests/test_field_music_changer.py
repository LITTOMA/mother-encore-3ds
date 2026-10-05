"""Manual producer/source negatives, never auto registered or executed here."""
import copy,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools import field_music_changer as music
class FieldMusicSource(unittest.TestCase):
 def test_source(self):
  d=music.load();self.assertEqual(len(d['bindings']),13);self.assertEqual(sum(len(b['shapes'])for b in d['bindings']),16);self.assertEqual(sum(len(o['parts'])for b in d['bindings']for o in b['shapes']),38);self.assertEqual(len(music.audio_bindings()),5);self.assertEqual(music.encode(d),music.PACK.read_bytes())
 def test_unknown_version_identity_shape_and_service(self):
  d=music.load()
  for mutate in [lambda x:x.update(schema=2),lambda x:x['bindings'][0].update(id=0),lambda x:x['bindings'][0].update(region_id=0),lambda x:x['bindings'][0]['shapes'][0].update(order=9),lambda x:x['bindings'][0]['shapes'][0]['parts'][0][0].__setitem__(0,float('nan')),lambda x:x['connections'][0].update(method='unknown')]:
   bad=copy.deepcopy(d);mutate(bad)
   with self.assertRaises(ValueError):music.validate(bad)
if __name__=='__main__':unittest.main()
