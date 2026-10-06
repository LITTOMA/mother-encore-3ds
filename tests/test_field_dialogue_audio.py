"""Manual-only native source checks; no default test registration."""
import struct,unittest,zlib
from tools import field_dialogue_audio as source
class SourceCases(unittest.TestCase):
 def test_real_recipe_and_pcm_bindings(self):
  d=source.load();self.assertEqual(3,len(d['nodes']));self.assertFalse(d['scene_admitted']);self.assertEqual(3,len(d['engine']['proofs']));self.assertEqual(['AudioStreamPlayer','SoundEffect','InputSound'],[n['node']for n in d['nodes']]);self.assertTrue(all(n['bus']=='SFX' and n['native_unconfigured_bus']=='Master' for n in d['nodes']));self.assertTrue(all(any(a['id']==n['stream']for a in d['assets'])for n in d['nodes']))
 def test_final_pack_and_source_double_range(self):
  d=source.load();b=source.encode(d);self.assertEqual(b[:8],b'ENCFDAU1');self.assertEqual(struct.unpack_from('<I',b,24)[0],0x454e0049);self.assertEqual(struct.unpack_from('<I',b,32)[0],3);self.assertEqual(struct.unpack_from('<I',b,20)[0],zlib.crc32(b[128:]));self.assertEqual(struct.unpack_from('<2d',b,160),tuple(d['pitch_range']));self.assertEqual(source.OUT.read_bytes(),b)
if __name__=='__main__':unittest.main()
