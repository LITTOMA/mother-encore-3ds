import hashlib,importlib.util,json,struct,tempfile,unittest,zlib,sys
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
spec=importlib.util.spec_from_file_location('native_battle',ROOT/'tools/native_battle.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class BattleBinaryTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.ir=json.loads((ROOT/'content/native-battle.json').read_text());cls.assets=json.loads((ROOT/'romfs/battle-preview/source.json').read_text());cls.room=(ROOT/'romfs/data/opening.encroom').read_bytes();cls.tables=m.lower(cls.ir,cls.assets,cls.room);cls.blob=m.encode(cls.tables,cls.ir['commit'])
 def test_deterministic_external_binary(self):
  self.assertEqual(self.blob,m.encode(m.lower(self.ir,self.assets,self.room),self.ir['commit']));self.assertEqual(self.blob,(ROOT/'romfs/data/opening.encbattle').read_bytes());self.assertEqual(len(m.parse_sections(self.blob)),12)
 def test_all_truncations(self):
  for n in range(len(self.blob)):
   with self.assertRaises((ValueError,UnicodeError,struct.error)):m.parse_sections(self.blob[:n])
 def test_crc_correct_header_directory_rejections(self):
  for offset,value in[(8,2),(20,11),(24,2),(28,2),(52,1),(68,1),(64+16+8,1000000),(64+16+12,1)]:
   b=bytearray(self.blob);struct.pack_into('<I',b,offset,value);struct.pack_into('<I',b,16,0);struct.pack_into('<I',b,16,zlib.crc32(b))
   with self.assertRaises((ValueError,UnicodeError,struct.error)):m.parse_sections(b)
 def test_staging_exact_resources(self):
  files=m.stage_files(ROOT/'romfs');self.assertIn(Path('data/opening.encbattle'),files);self.assertEqual(len(files),len(self.tables['resources'])+1)
 def test_changed_source_fails(self):
  ir=json.loads(json.dumps(self.ir));key=next(iter(ir['sources']));ir['sources'][key]='0'*64
  with self.assertRaises(m.ContentError):m.verify_sources(ir)
 def test_data_changes_do_not_require_cpp(self):
  tables=m.lower(self.ir,self.assets,self.room);tables['participants'][0][5]-=1
  self.assertNotEqual(self.blob,m.encode(tables,self.ir['commit']));self.assertEqual(m.parse_sections(m.encode(tables,self.ir['commit']))[6][20:24],struct.pack('<i',61))
 def test_real_source_stats_and_menu(self):
  self.assertEqual(self.tables['participants'][0][5:9],[62,62,26,26]);self.assertEqual(len(self.tables['menus']),3);self.assertFalse(self.tables['metadata'][0][6]&1)
 def test_invalid_vector(self):
  with self.assertRaises(m.ContentError):m.vec(float('nan'))
  with self.assertRaises(m.ContentError):m.vec([1]*5)
if __name__=='__main__':unittest.main()
