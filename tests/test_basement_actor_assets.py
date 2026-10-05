"""Manual suite only: source special animation and malformed binary checks."""
import copy,struct,unittest,zlib
from tools import basement_actor_assets as pack
class BasementActorFormat(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.ir=pack.load();cls.tables=pack.lower(cls.ir,pack.verify_receipt());cls.raw=pack.encode(cls.tables)
 def reject(self,offset,value):
  b=bytearray(self.raw);struct.pack_into('<I',b,offset,value);b[16:20]=bytes(4);struct.pack_into('<I',b,16,zlib.crc32(b)&0xffffffff)
  with self.assertRaises(ValueError):pack.parse_pack(bytes(b))
 def test_source_cells(self):
  r=self.ir['resources'][0];self.assertEqual(r['source_size'],[1280,640]);self.assertEqual(r['size'],[512,448]);self.assertEqual(len(r['frame_map']),53);self.assertEqual(r['frame_map'][0],15);self.assertEqual(self.ir['animations'][0]['keys'][0]['frame'],0)
 def test_binary(self):self.assertEqual(len(pack.parse_pack(self.raw)),6)
 def test_versions(self):
  for o,v in [(8,2),(20,2),(24,2),(28,5),(52,1),(64,7),(68,0),(76,99)]:self.reject(o,v)
 def test_spans(self):
  o=struct.unpack_from('<I',self.raw,64+2*16+4)[0]
  for p,v in [(0,0),(8,999),(12,1),(16,0),(20,0)]:self.reject(o+p,v)
 def test_key_frames(self):
  o=struct.unpack_from('<I',self.raw,64+3*16+4)[0];self.reject(o+4,9999)
 def test_audio(self):
  o=struct.unpack_from('<I',self.raw,64+5*16+4)[0];self.reject(o,0);self.reject(o+4,999);self.reject(o+8,999);self.reject(o+12,0x7fc00000);self.reject(o+16,0)
 def test_truncation_crc(self):
  for n in (0,63,pack.HEADER-1,len(self.raw)-1):
   with self.assertRaises(ValueError):pack.parse_pack(self.raw[:n])
  b=bytearray(self.raw);b[-1]^=1
  with self.assertRaises(ValueError):pack.parse_pack(bytes(b))
  with self.assertRaises(ValueError):pack.parse_pack(self.raw+b'\0')
if __name__=='__main__':unittest.main()
