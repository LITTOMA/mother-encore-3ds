"""Manual basement source/format negative checks. Never auto-run in development."""
import copy,struct,unittest,zlib,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import basement_progression as pack
class BasementFormat(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.ir=pack.load(ROOT);cls.tables=pack.lower(cls.ir);cls.blob=pack.encode(cls.tables)
    def mutate(self,offset,value):
        b=bytearray(self.blob);struct.pack_into('<I',b,offset,value);b[16:20]=b'\0'*4;struct.pack_into('<I',b,16,zlib.crc32(b)&0xffffffff);return b
    def reject(self,section,row,column,value):
        t=copy.deepcopy(self.tables);t[section][row][column]=value
        with self.assertRaises((ValueError,TypeError,struct.error)):pack.encode(t)
    def test_source_review(self):self.assertEqual(self.ir,pack.build(ROOT))
    def test_roundtrip(self):self.assertEqual(pack.parse_pack(self.blob),self.tables)
    def test_versions(self):
        for offset in (8,20,24):
            with self.subTest(offset=offset),self.assertRaises(ValueError):pack.parse_pack(self.mutate(offset,2))
    def test_directory(self):
        for offset,value in ((28,9),(52,1),(64,99),(68,0),(76,9),(64+16+12,16)):
            with self.subTest(offset=offset),self.assertRaises(ValueError):pack.parse_pack(self.mutate(offset,value))
    def test_crc(self):
        b=bytearray(self.blob);b[-1]^=1
        with self.assertRaises(ValueError):pack.parse_pack(b)
    def test_trailing(self):
        with self.assertRaises(ValueError):pack.parse_pack(self.blob+b'\0')
    def test_string_reference(self):self.reject('KeyItems',0,1,1)
    def test_key_doses(self):self.reject('KeyItems',0,2,0)
    def test_key_grant(self):self.reject('KeyItems',0,3,2)
    def test_duplicate_key_identity(self):self.reject('KeyItems',1,0,self.tables['KeyItems'][0][0])
    def test_skill_rank(self):self.reject('Skills',0,3,len(self.tables['SkillOrder']))
    def test_duplicate_skill_order(self):self.reject('SkillOrder',1,0,self.tables['SkillOrder'][0][0])
    def test_npc_action(self):self.reject('NpcOverrides',0,0,2)
    def test_npc_supported(self):self.reject('NpcOverrides',0,3,2)
    def test_door_key(self):self.reject('Door',0,1,999)
    def test_door_removal(self):self.reject('Door',0,3,2)
    def test_present_item(self):self.reject('Present',0,1,999)
    def test_present_scale(self):self.reject('Present',0,8,0)
    def test_present_interaction_extent(self):self.reject('Present',0,12,0)
    def test_present_pickup(self):self.reject('Present',0,14,2)
    def test_present_turn_mask(self):self.reject('Present',0,15,4)
    def test_present_object_flag(self):self.reject('Present',0,17,1)
    def test_present_reset_area(self):self.reject('Present',0,18,1)
    def test_present_reset_region(self):self.reject('Present',0,19,1)
    def test_present_stable_identity(self):self.reject('Present',0,21,0)
    def test_music_track(self):self.reject('Music',0,3,0)
    def test_music_region(self):self.reject('Music',0,4,0)
    def test_music_default_stop(self):self.reject('Music',0,5,float('nan'))
    def test_music_gain(self):self.reject('Music',0,6,25)
    def test_music_geometry(self):self.reject('Music',0,11,0)
    def test_music_parent_reference(self):self.reject('MusicLifecycle',0,1,1)
    def test_music_order(self):self.reject('MusicLifecycle',0,2,0)
    def test_music_order_duplicate(self):self.reject('MusicLifecycle',1,2,self.tables['MusicLifecycle'][0][2])
    def test_music_identity_link(self):self.reject('MusicLifecycle',0,0,999)
    def test_fade_geometry_nonfinite(self):self.reject('FadeRect',0,0,float('nan'))
    def test_fade_geometry_extent(self):self.reject('FadeRect',0,2,0)
    def test_fade_nonfinite(self):self.reject('FadeKeys',1,0,float('nan'))
    def test_fade_order(self):self.reject('FadeKeys',1,0,0)
    def test_fade_color(self):self.reject('FadeKeys',1,4,2)
    def test_fade_duration(self):self.reject('Parameters',0,1,0)
    def test_mick_prerequisite(self):self.reject('Mick',0,2,self.tables['Strings'].index(b'KeyBasement'))
    def test_source_controls(self):
        with self.assertRaises(ValueError):pack.text_segments('[@]Unknown [any_new_opcode]')
    def test_source_graph_unknown_field(self):
        ir=copy.deepcopy(self.ir);ir['unreviewed_handler']=True
        with self.assertRaises(ValueError):pack.lower(ir)
    def test_key_grant_source_phase(self):
        d=pack.expected_documents()['Podunk/woof_key'];self.assertEqual(list(d),['0','4','5','6']);self.assertEqual(d['4']['item'],'KeyBasement');self.assertEqual(d['6']['setflags'],'mick_telepathy')
if __name__=='__main__':unittest.main()
