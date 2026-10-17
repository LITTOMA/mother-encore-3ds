"""Manual Pause/Equip source and ENCFIE01 parser cases; not automatic CI."""
import copy,hashlib,json,struct,sys,unittest,zlib
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
    def test_six_known_commands_equipment_and_field_psi(self):
        self.assertEqual([r[3] for r in self.tables['Commands']],[1,1,1,1,1,1])
        self.rejects('Commands',0,3,0)
    def test_source_slots_and_equipment(self):
        self.assertEqual([s['source'] for s in self.ir['slots']],['weapon','body','arms','other'])
        self.assertEqual(self.ir['equipment'][0]['boosts'],[0,0,0,5,0,0,0])
        self.rejects('Equipment',0,2,4)
    def test_unknown_versions_and_capabilities(self):
        for offset,value in ((8,1),(8,4),(20,1),(20,2),(20,6),(24,2)):
            with self.subTest(offset=offset,value=value),self.assertRaises(ValueError):pack.parse_pack(self.changed(offset,struct.pack('<I',value)))
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
    def test_expanded_backgrounds_and_title(self):
        layouts={r['role']:r for r in self.ir['layouts']}
        self.assertEqual(layouts['PausePanel']['rect'],[4,4,159,72])
        self.assertEqual(layouts['PauseInside']['rect'],[4,4,159,63])
        self.assertEqual(layouts['PauseCommand']['rect'],[15,15,61,12])
        self.assertEqual(layouts['StatsPanel']['rect'],[28,23,162,110])
        self.assertEqual(layouts['EquipmentPanel']['resource'],0)
        self.assertEqual(layouts['PauseTitleBackdrop']['rect'],[14,5,13,10])
        self.assertEqual(layouts['PauseTitleBackdrop']['kind'],'Rectangle')
        self.assertLess(layouts['PauseInside']['rect'][0],layouts['PauseCommand']['rect'][0])
    def test_unknown_style_operations_rejected(self):
        scene=(ROOT/'upstream/MOTHER-Encore/Nodes/Ui/Pause menu.tscn').read_text(encoding='utf-8')
        for replacement in ('expand_margin_left = -1.0','expand_margin_left = NaN',
                            'expand_margin_left = 11.0\nexpand_margin_left = 12.0',
                            'expand_margin_left = 11.0\ndraw_center = false'):
            with self.subTest(replacement=replacement),self.assertRaises(ValueError):
                pack.panel_geometry(scene.replace('expand_margin_left = 11.0',replacement),
                                    'menu/Commands/InsideBG',[15,15,140,50],4)
    def test_title_backdrop_required_and_typed(self):
        row=next(i for i,r in enumerate(self.ir['layouts']) if r['role']=='PauseTitleBackdrop')
        self.rejects('Layouts',row,3,pack.KINDS.index('Text')+1)
        self.rejects('Layouts',row,11,0)
        tables=copy.deepcopy(self.tables);tables['Layouts'].pop(row)
        with self.assertRaises(ValueError):pack.encode(tables)
    def test_unchanged_asset_reuse_guards(self):
        previous=(ROOT/pack.IR).read_bytes();producer=(ROOT/'tools/field_equipment.py').read_bytes()
        self.assertEqual(pack.reuse_asset_receipt(previous,producer,self.receipt,self.ir),self.receipt)
        changed=copy.deepcopy(self.ir);changed['resources'][0]['source']='unreviewed.png'
        with self.assertRaises(ValueError):pack.reuse_asset_receipt(previous,producer,self.receipt,changed)
        changed=copy.deepcopy(self.ir);changed['sources'][next(iter(changed['sources']))]='0'*64
        with self.assertRaises(ValueError):pack.reuse_asset_receipt(previous,producer,self.receipt,changed)
        receipt=copy.deepcopy(self.receipt);receipt['producer_sha256']='0'*64
        with self.assertRaises(ValueError):pack.reuse_asset_receipt(previous,producer,receipt,self.ir)
        altered=producer.replace(b"'-f','rgba8'",b"'-f','rgb565'")
        self.assertNotEqual(altered,producer)
        receipt=copy.deepcopy(self.receipt);receipt['producer_sha256']=hashlib.sha256(altered).hexdigest()
        with self.assertRaises(ValueError):pack.reuse_asset_receipt(previous,altered,receipt,self.ir)
    def test_field_psi_source_and_negative_policy(self):
        self.assertEqual(self.ir['bindings']['PsiName']['zh_Hans_CN'],'心灵感应')
        self.assertEqual(self.ir['bindings']['PsiSkillId']['en'],'telepathy')
        self.assertEqual(self.ir['parameters']['PsiCost'],0)
        self.assertEqual(self.ir['parameters']['PsiLearnLevel'],2)
        self.assertEqual(self.ir['parameters']['PsiRayY'],6)
        for name,value in [('PsiCost',-1),('PsiCost',0.5),('PsiLearnLevel',0),('PsiAction',2),('PsiRayLength',0),('PsiRayY',65),('PsiFoodLimitEn',0),('PsiFoodLimitZh',513)]:
            self.rejects('Parameters',pack.PARAMETERS.index(name),1,value)
        row=next(i for i,l in enumerate(self.ir['layouts']) if l['role']=='PsiPanel')
        self.rejects('Layouts',row,4,99);self.rejects('Layouts',row,3,5)
        self.rejects('Bindings',pack.BINDINGS.index('PsiSkillId'),1,self.tables['Bindings'][pack.BINDINGS.index('PsiBaseSkill0')][1])
        t=copy.deepcopy(self.tables);t['Layouts'].pop(row)
        with self.assertRaises(ValueError):pack.encode(t)
    def test_actual_staged_resources(self):
        files=pack.stage_files(ROOT/'romfs')
        self.assertEqual(files[Path('ui/opening.encfield')],self.blob)
        self.assertEqual(len(files),1+len(self.ir['resources']))
if __name__=='__main__':unittest.main()
