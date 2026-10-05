#!/usr/bin/env python3
"""Manual-only parser and unsupported-capability checks for house inspections."""
import copy, struct, sys, unittest, zlib
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.house_inspection import encode, parse_pack, lower, PIN, NONE
from tools.link_house_inspections import text_segments

class InspectionParserTests(unittest.TestCase):
    def fixture(self):
        pool=b'\0Objects/interact_fixture\0Data/Dialogue/fixture.yaml\0flag\0'
        path=pool.index(b'Data/'); flag=pool.index(b'flag')
        return dict(Strings=pool,Objects=[[1,1,path,0,0,3,0,1,1,0,0,10.,20.,10.,20.,4.,4.,0.,-5.]],Overrides=[[0,flag,path,0]])

    def changed(self,blob,offset,fmt,value):
        blob=bytearray(blob);struct.pack_into(fmt,blob,offset,value)
        struct.pack_into('<I',blob,16,0);struct.pack_into('<I',blob,16,zlib.crc32(blob));return bytes(blob)

    def test_checked_binary_roundtrip(self):
        tables=self.fixture();self.assertEqual(parse_pack(encode(tables))['Objects'][0],tuple(tables['Objects'][0]))

    def test_unknown_header_and_truncation(self):
        blob=encode(self.fixture())
        for damaged in (blob[:20],blob[:-1],self.changed(blob,8,'I',2),self.changed(blob,24,'I',2),self.changed(blob,52,'I',1)):
            with self.assertRaises(ValueError):parse_pack(damaged)

    def test_crc_and_span_rejection(self):
        blob=encode(self.fixture())
        for damaged in (blob[:-1]+bytes([blob[-1]^1]),self.changed(blob,84,'I',112),self.changed(blob,82,'H',72),self.changed(blob,92,'I',77)):
            with self.assertRaises(ValueError):parse_pack(damaged)

    def test_unknown_policy_geometry_and_owned_override(self):
        for category,value in ((0,0),(1,2),(5,4),(8,2),(9,1),(13,float('nan')),(15,0.)):
            tables=self.fixture();tables['Objects'][0][category]=value
            with self.assertRaises((ValueError,UnicodeDecodeError)):parse_pack(encode(tables))
        tables=self.fixture();tables['Overrides'][0][0]=1
        with self.assertRaises(ValueError):parse_pack(encode(tables))
        tables=self.fixture();tables['Objects'][0][7]=0
        with self.assertRaises(ValueError):parse_pack(encode(tables))

    def test_unknown_text_controls_fail_closed(self):
        for raw in ('missing bullet','[@][Unknown]','[@][color]unfinished','[@][/color]','[@]one[WAIT@]','[@][ItemReceiver]'):
            with self.assertRaises(ValueError):text_segments(raw)
        self.assertEqual(text_segments('[@]A [Ninten] system.[WAIT@]Next.'),[[{'kind':1,'text':'A '},{'kind':2,'text':''},{'kind':1,'text':' system.'}],[{'kind':1,'text':'Next.'}]])

    def test_explicit_unsupported_binding_and_no_flattening(self):
        obj=dict(id=1,source_path='Objects/interact_fixture',position=[0,0],interact_center=[0,0],interact_extents=[4,4],prompt_offset=[0,-5],player_turn=0,collision_mask=1,appear_flag='',disappear_flag='',seen_key='',default_dialogue='Data/Dialogue/item.yaml',default_supported=False,overrides=[])
        ir=dict(schema=1,kind='encore.house-inspections.source-ir',commit=PIN,scope='fixture',sources={},objects=[obj],texts=[],unsupported=[])
        house=dict(commit=PIN,dialogues=[])
        self.assertEqual(lower(ir,house)['Objects'][0][10],NONE)
        house['dialogues']=[dict(source_path='Data/Dialogue/item.yaml')]
        with self.assertRaises(ValueError):lower(ir,house)
        obj['default_supported']=True;house['dialogues']=[]
        with self.assertRaises(ValueError):lower(ir,house)
        house['dialogues']=[dict(source_path='Data/Dialogue/item.yaml')]*2
        with self.assertRaises(ValueError):lower(ir,house)
        house['dialogues']=[dict(source_path='Data/Dialogue/item.yaml'),
                           dict(source_path='Data/Dialogue/other.yaml'),
                           dict(source_path='Data/Dialogue/other.yaml')]
        self.assertEqual(lower(ir,house)['Objects'][0][10],0)
        obj['default_supported']=False;obj['unknown']=1
        with self.assertRaises(ValueError):lower(ir,house)

if __name__=='__main__':unittest.main()
