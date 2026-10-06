"""Manual-only checked Dialogue visual source cases; never auto-registered."""
import copy,unittest
from tools import field_dialogue_visual as source
class SourceCases(unittest.TestCase):
 def test_source_scope(self):
  d=source.load();self.assertEqual(20,len(d['records']));self.assertEqual(2,len(d['cursors']));self.assertFalse(d['scene_admitted']);self.assertTrue(d['cursors'][0]['flags']&128)
 def test_source_crc_and_schema(self):
  d=source.load();b=source.encode(d);self.assertEqual(b[:8],b'ENCFDVS1');self.assertEqual(int.from_bytes(b[24:28],'little'),0x454e0046);self.assertEqual(int.from_bytes(b[32:36],'little'),20)
if __name__=='__main__':unittest.main()
