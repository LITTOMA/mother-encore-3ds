"""Manual parser negatives: no automatic invocation."""
import unittest,zlib,struct
from tools.field_item_details import load,lower,verify_receipt,encode,parse_pack,tokenize
class FieldItemDetailsCases(unittest.TestCase):
 def test_actual_source(self):
  d=load();self.assertEqual(len(d['definitions']),19);raw=encode(lower(d,verify_receipt(d)));parse_pack(raw)
  for change in ('magic','cap','tail','token'):
   b=bytearray(raw)
   if change=='magic':b[0]^=1
   elif change=='cap':struct.pack_into('<I',b,24,2)
   elif change=='tail':b.append(0)
   else:
    offset=struct.unpack_from('<I',b,64+4*16+4)[0];struct.pack_into('<I',b,offset,999)
   struct.pack_into('<I',b,16,0);struct.pack_into('<I',b,16,zlib.crc32(b))
   with self.assertRaises(ValueError):parse_pack(bytes(b))
 def test_text_controls(self):
  self.assertEqual(tokenize('99% Beef.')[0]['value'],'99% Beef.')
  self.assertEqual(tokenize('[Blinded]')[0]['value'],1)
  with self.assertRaises(ValueError):tokenize('[UnreviewedStatus]')
if __name__=='__main__':unittest.main()
