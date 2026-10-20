import struct,tempfile,unittest,zlib
from pathlib import Path
from tests import prepare_bound_field_objects as fixture
from tools import podunk_field as f
ROOT=Path(__file__).resolve().parents[1]
class BoundObjectTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.tmp=tempfile.TemporaryDirectory(prefix='bound-objects-',dir=ROOT/'build');cls.blob,cls.objects,cls.proof=fixture.build(Path(cls.tmp.name))
 @classmethod
 def tearDownClass(cls):cls.tmp.cleanup()
 def test_actual_source_joins_and_collision_filter(self):
  p=self.proof;self.assertEqual(p['body_extents'],[9,8]);self.assertEqual(p['body_center'],[1432,4664]);self.assertEqual((p['body_layer'],p['body_mask'],p['player_layer'],p['player_mask']),(4,0,1,4353));self.assertFalse(p['player_collides']);self.assertEqual(p['body'],f.NONE);self.assertEqual(p['resolved'],6)
  _,sections=f.unpack_container(self.blob,f.MAP_MAGIC,f.MAP_STRIDES,f.MAP_LIMIT);self.assertEqual(struct.unpack_from('<HII',sections[13],p['draw']*32+2),(0,1,1));self.assertEqual(struct.unpack_from('<H',sections[13],p['draw']*32)[0],7)
 def test_old_and_unknown_map_capability_rejected(self):
  for cap in(0,1,2,3,4,5,7):
   b=bytearray(self.blob);struct.pack_into('<I',b,20,cap)
   with self.subTest(cap=cap),self.assertRaises(ValueError):f.unpack_container(b,f.MAP_MAGIC,f.MAP_STRIDES,f.MAP_LIMIT)
  b=bytearray(self.blob);struct.pack_into('<I',b,20,7)
  with self.assertRaises(ValueError):f.unpack_container(b,f.MAP_MAGIC,f.MAP_STRIDES,f.MAP_LIMIT,capability=7)
 def test_bad_object_identity_or_unknown_item_rejected(self):
  at=struct.unpack_from('<I',self.blob,64+13*16+4)[0]+self.proof['draw']*32
  for offset,value in((0,8),(4,0),(4,f.NONE),(8,2)):
   b=bytearray(self.blob);struct.pack_into('<I',b,at+offset,value);struct.pack_into('<I',b,16,zlib.crc32(b[32:]))
   with self.subTest(offset=offset,value=value),self.assertRaises(ValueError):f.unpack_container(b,f.MAP_MAGIC,f.MAP_STRIDES,f.MAP_LIMIT)
 def test_unreviewed_object_owner_or_id_rejected(self):
  for bindings in([],{'missing':1},{'Objects/Items/Item':True},{'Objects/Items/Item':0},{'Objects/Items/Item':f.NONE},{'Objects':1},{'Objects/Items/Item':1,'Objects/Items/PresentCem5':1}):
   with self.subTest(bindings=bindings),self.assertRaises(ValueError):f.Model(f.load_recipe(),object_bindings=bindings)
if __name__=='__main__':unittest.main()
