"""Manual Pause/Equip source and ENCFIE01 parser cases; not automatic CI."""
import copy,struct,sys,unittest,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools import field_equipment as pack
class FieldEquipmentFormat(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ir=pack.load();cls.receipt=pack.verify_receipt(cls.ir)
        cls.tables=pack.lower(cls.ir,cls.receipt);cls.blob=pack.encode(cls.tables)
    def changed(self,offset,raw):
        b=bytearray(self.blob);b[offset:offset+len(raw)]=raw;b[16:20]=b'\0'*4;struct.pack_into('<I',b,16,zlib.crc32(b));return b
    def rejects(self,section,row,column,value):
        t=copy.deepcopy(self.tables);t[section][row][column]=value
        with self.assertRaises((ValueError,TypeError,struct.error,OverflowError)):pack.encode(t)
    def test_owner_id_contract(self):
        row=pack.PARAMETERS.index('OwnerId')
        self.assertEqual(self.tables['Parameters'][row][1],self.ir['parameters']['OwnerId'])
        for value in (0,1.5,8193):self.rejects('Parameters',row,1,value)
    def test_source_and_actual_pack(self):
        self.assertEqual(pack.build(),self.ir)
        decoded=pack.parse_pack(self.blob)
        self.assertEqual(decoded['Strings'],self.tables['Strings'])
        self.assertEqual(decoded['Commands'],self.tables['Commands'])
        self.assertEqual(decoded['Slots'],self.tables['Slots'])
        for actual,source in zip(decoded['Keys'],self.tables['Keys']):
            for value,expected in zip(actual,source):self.assertAlmostEqual(value,expected,places=5)
        self.assertEqual((ROOT/pack.PACK).read_bytes(),self.blob)
    def test_six_known_commands_goods_and_equipment(self):
        self.assertEqual([r[3] for r in self.tables['Commands']],[2,0,1,0,0,0])
        self.rejects('Commands',0,3,1);self.rejects('Commands',0,3,3)
    def test_source_slots_and_equipment(self):
        self.assertEqual([s['source'] for s in self.ir['slots']],['weapon','body','arms','other'])
        self.assertEqual(self.ir['equipment'][0]['boosts'],[0,0,0,5,0,0,0])
        self.rejects('Equipment',0,2,4)
    def test_unknown_versions_and_capabilities(self):
        for offset,value in ((8,1),(8,3),(20,3),(24,2)):
            with self.subTest(offset=offset,value=value),self.assertRaises(ValueError):pack.parse_pack(self.changed(offset,struct.pack('<I',value)))
    def test_goods_rejects_old_capability_without_widening(self):
        with self.assertRaises(ValueError):pack.parse_pack(self.changed(20,struct.pack('<I',1)))
    def test_corrupt_pin_reserved_crc_and_truncation(self):
        for blob in (self.changed(32,b'\x01'),self.changed(52,b'\x01'),self.blob[:-1],self.blob+b'\0'):
            with self.assertRaises(ValueError):pack.parse_pack(blob)
        b=bytearray(self.blob);b[-1]^=1
        with self.assertRaises(ValueError):pack.parse_pack(b)
    def test_overlap_bad_stride_and_unknown_directory(self):
        for offset,value in ((64,99),(68,0),(76,2)):
            with self.assertRaises(ValueError):pack.parse_pack(self.changed(offset,struct.pack('<I',value)))
    def test_duplicate_identity_and_middle_string(self):
        self.rejects('Slots',1,0,1);self.rejects('Bindings',0,1,2)
        self.rejects('Layouts',1,1,self.tables['Layouts'][0][1])
    def test_bad_texture_and_layout_bounds(self):
        self.rejects('Resources',0,2,2);self.rejects('Resources',0,8,0)
        self.rejects('Resources',0,7,b'\0'*32)
        self.rejects('Layouts',0,4,99);self.rejects('Layouts',0,9,float('nan'))
    def test_unknown_input_and_animation(self):
        self.rejects('Parameters',pack.PARAMETERS.index('ConfirmMask'),1,3)
        self.rejects('Clips',0,0,99);self.rejects('Clips',0,1,1)
        self.rejects('Keys',0,2,0)
    def test_actual_staged_resources(self):
        files=pack.stage_files(ROOT/'romfs')
        self.assertEqual(files[Path('data/opening.encfield')],self.blob)
        self.assertEqual(len(files),1+len(self.ir['resources']))
if __name__=='__main__':unittest.main()
