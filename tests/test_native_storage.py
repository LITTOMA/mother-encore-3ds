"""Manual source/format negatives for ordinary Minnie Storage.

Not automatically executed during development or routine CI.
"""
import copy,struct,unittest,zlib
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import native_storage as pack
from tools.storage_assets import build,load

class StorageFormat(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ir=load(ROOT)
        # Synthetic hashes exercise only pack layout; these are never written
        # as actual console textures or substituted for source admission.
        receipt={'outputs':{r['output']:{'sha256':'ab'*32,'size':r['crop'][2:] if r['crop'] else r['size']} for r in cls.ir['resources']}}
        cls.tables=pack.lower(cls.ir,receipt);cls.blob=pack.encode(cls.tables)
    def reject_table(self,section,row,column,value):
        t=copy.deepcopy(self.tables);t[section][row][column]=value
        with self.assertRaises((ValueError,TypeError,struct.error,OverflowError)):pack.encode(t)
    def changed_blob(self,offset,data):
        b=bytearray(self.blob);b[offset:offset+len(data)]=data;b[16:20]=b'\0'*4;struct.pack_into('<I',b,16,zlib.crc32(b)&0xffffffff);return b
    def test_positive_round_trip(self):self.assertEqual(pack.parse_pack(self.blob),self.tables)
    def test_pinned_source_ir(self):self.assertEqual(load(ROOT),build(ROOT))
    def test_source_unknown_field(self):
        ir=copy.deepcopy(self.ir);ir['unknown']=True
        with self.assertRaises(ValueError):pack.lower(ir,{'outputs':{}})
    def test_unknown_schema(self):
        with self.assertRaises(ValueError):pack.parse_pack(self.changed_blob(8,struct.pack('<I',2)))
    def test_unknown_capability(self):
        with self.assertRaises(ValueError):pack.parse_pack(self.changed_blob(20,struct.pack('<I',3)))
    def test_legacy_capability_keeps_exact_doses(self):
        t=copy.deepcopy(self.tables)
        for p in t['Policies']:p[6]=p[2]
        self.assertEqual(pack.parse_pack(pack.encode(t,caps=1)),t)
    def test_legacy_cannot_encode_partial_policy(self):
        with self.assertRaises(ValueError):pack.encode(self.tables,caps=1)
    def test_capability_stride_mismatch(self):
        with self.assertRaises(ValueError):pack.parse_pack(self.changed_blob(20,struct.pack('<I',1)))
    def test_unknown_rules(self):
        with self.assertRaises(ValueError):pack.parse_pack(self.changed_blob(24,struct.pack('<I',2)))
    def test_wrong_source_pin(self):
        with self.assertRaises(ValueError):pack.parse_pack(self.changed_blob(32,b'x'*20))
    def test_reserved_header(self):
        with self.assertRaises(ValueError):pack.parse_pack(self.changed_blob(52,b'\1'))
    def test_corrupt_crc(self):
        b=bytearray(self.blob);b[-1]^=1
        with self.assertRaises(ValueError):pack.parse_pack(b)
    def test_truncated(self):
        with self.assertRaises(ValueError):pack.parse_pack(self.blob[:-1])
    def test_unknown_section(self):
        with self.assertRaises(ValueError):pack.parse_pack(self.changed_blob(64,struct.pack('<I',99)))
    def test_wrong_stride(self):
        with self.assertRaises(ValueError):pack.parse_pack(self.changed_blob(64+16+12,struct.pack('<I',20)))
    def test_overlapping_directory(self):
        with self.assertRaises(ValueError):pack.parse_pack(self.changed_blob(64+16+4,struct.pack('<I',pack.HEADER)))
    def test_trailing_bytes(self):
        with self.assertRaises(ValueError):pack.parse_pack(self.blob+b'\0')
    def test_bad_string_reference(self):self.reject_table('Policies',0,1,1)
    def test_invalid_utf8(self):
        t=copy.deepcopy(self.tables);t['Strings']=b'\0\xff\0'
        with self.assertRaises(ValueError):pack.encode(t)
    def test_duplicate_policy(self):
        t=copy.deepcopy(self.tables);t['Policies'].append(t['Policies'][0]);t['Equipment'].append(t['Equipment'][0])
        with self.assertRaises(ValueError):pack.encode(t)
    def test_unknown_sort_tie(self):self.reject_table('Policies',1,4,self.tables['Policies'][0][4])
    def test_zero_doses(self):self.reject_table('Policies',0,2,0)
    def test_zero_minimum_doses(self):self.reject_table('Policies',0,6,0)
    def test_minimum_exceeds_maximum(self):self.reject_table('Policies',0,6,self.tables['Policies'][0][2]+1)
    def test_unknown_conservation_count(self):self.reject_table('Policies',0,3,2)
    def test_zero_storage_capacity(self):self.reject_table('Parameters',0,1,0)
    def test_infinite_parameter(self):self.reject_table('Parameters',0,1,float('inf'))
    def test_nan_parameter(self):self.reject_table('Parameters',0,1,float('nan'))
    def test_unknown_parameter(self):self.reject_table('Parameters',0,0,999)
    def test_duplicate_binding(self):self.reject_table('Bindings',1,0,1)
    def test_unknown_binding(self):self.reject_table('Bindings',0,0,999)
    def test_bad_resource_kind(self):self.reject_table('Resources',0,2,2)
    def test_zero_resource_size(self):self.reject_table('Resources',0,3,0)
    def test_bad_resource_grid(self):self.reject_table('Resources',0,5,999)
    def test_empty_texture_hash(self):self.reject_table('Resources',0,7,b'\0'*32)
    def test_unknown_layout_role(self):self.reject_table('Layouts',0,1,999)
    def test_cycle_parent(self):self.reject_table('Layouts',0,2,0)
    def test_unknown_draw_kind(self):self.reject_table('Layouts',0,3,999)
    def test_invalid_texture_index(self):self.reject_table('Layouts',0,4,999)
    def test_unknown_layout_flags(self):self.reject_table('Layouts',0,6,1)
    def test_invalid_color(self):self.reject_table('Layouts',0,13,2)
    def test_negative_rectangle(self):self.reject_table('Layouts',0,11,-1)
    def test_ninepatch_overflow(self):self.reject_table('Layouts',0,17,999)
    def test_missing_pane_owner(self):self.reject_table('Layouts',next(i for i,l in enumerate(self.tables['Layouts']) if l[1]==8),2,pack.NIL)
    def test_unknown_equipment(self):self.reject_table('Equipment',0,0,999)
    def test_duplicate_equipment(self):self.reject_table('Equipment',1,0,self.tables['Equipment'][0][0])
    def test_equipment_range(self):self.reject_table('Equipment',0,1,65536)

if __name__=='__main__':unittest.main()
