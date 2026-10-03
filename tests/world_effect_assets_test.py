#!/usr/bin/env python3
import copy,json,struct,sys,unittest,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.world_effect_assets import encode,validate_ir,verify_sources,IR,PACK
class WorldEffectAssets(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.ir=json.loads(IR.read_text())
 def test_reproducible_checked_pack(self):
  verify_sources(self.ir);b=encode(self.ir);self.assertEqual(b,PACK.read_bytes());self.assertEqual(len(b),532)
  raw=bytearray(b);crc=struct.unpack_from('<I',raw,16)[0];struct.pack_into('<I',raw,16,0);self.assertEqual(crc,zlib.crc32(raw))
 def test_unknown_fields_fail(self):
  for path in [[],['settings'],['texture'],['keys',0]]:
   ir=copy.deepcopy(self.ir);where=ir
   for key in path:where=where[key]
   where['unreviewed']=1
   with self.assertRaises(ValueError):encode(ir)
 def test_bad_texture_fail(self):
  for key,value in [('width',0),('height',257),('indices',[0]*159),('indices',[2]*160),('palette',[[256,0,0,0]])]:
   ir=copy.deepcopy(self.ir);ir['texture'][key]=value
   with self.assertRaises(ValueError):encode(ir)
 def test_bad_settings_fail(self):
  for key,value in [('cycle_duration',0),('appear_duration',float('nan')),('move_divisor',0),('pixel_snap_uv_epsilon',.01),('opacity',2),('source_width',2048),('appear_duration',1e-50),('move_divisor',1e-50)]:
   ir=copy.deepcopy(self.ir);ir['settings'][key]=value
   with self.assertRaises(ValueError):encode(ir)
 def test_bad_key_order_and_modes_fail(self):
  for key,value in [('time',-.1),('time',1.2),('ease',2),('color',[1,1,1,float('nan')])]:
   ir=copy.deepcopy(self.ir);ir['keys'][1][key]=value
   with self.assertRaises(ValueError):encode(ir)
 def test_float32_collapsed_keys_fail(self):
  ir=copy.deepcopy(self.ir);ir['keys'][1]['time']=1e-50
  with self.assertRaises(ValueError):encode(ir)
 def test_source_coverage_fail(self):
  ir=copy.deepcopy(self.ir);ir['sources'].pop(next(iter(ir['sources'])))
  with self.assertRaises(ValueError):verify_sources(ir)
if __name__=='__main__':unittest.main()
