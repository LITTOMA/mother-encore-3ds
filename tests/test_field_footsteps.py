import copy,json,struct,tempfile,unittest,zlib
from pathlib import Path
from tools import field_footsteps as s
class FootstepTests(unittest.TestCase):
 @classmethod
 def setUpClass(c):c.p,c.metadata,c.proof=s.source();c.blob=s.encode(c.metadata)
 def test_source_rectangles_and_real_loop_imports(self):
  m=self.metadata;self.assertEqual((m['default'],m['slot'],m['shadow']),('wood','run','shadow'));self.assertEqual([a['enter']for a in m['areas']],['wood','stone']);self.assertEqual([a['exit']for a in m['areas']],['','wood']);self.assertEqual([a['center']for a in m['areas']],[[8,12],[8,-581]]);self.assertEqual([a['extents']for a in m['areas']],[[32,28],[41,25]])
  self.assertTrue(all(v['loop']for v in m['sounds']));self.assertFalse(self.proof['whole_player_approved']);self.assertEqual(set(self.proof['player_methods']),{'_set_running','_move_state','_movement'})
 def test_codec_and_frozen_candidate(self):
  expected=copy.deepcopy(self.metadata)
  for sound in expected['sounds']:sound.pop('pcm');sound.pop('import_sha256')
  self.assertEqual(s.parse(self.blob),expected);self.assertEqual(self.blob,(s.ROOT/'tests/fixtures/mayor.encsteps').read_bytes())
  for at,value in [(8,2),(20,2),(24,0),(28,1),(32,0),(52,3),(64,2),(76,4),(64+2*16+12,60)]:
   bad=bytearray(self.blob);struct.pack_into('<I',bad,at,value);struct.pack_into('<I',bad,16,zlib.crc32(bad[32:]))
   with self.assertRaises(ValueError):s.parse(bad)
  for n in range(0,len(self.blob),7):
   with self.assertRaises(ValueError):s.parse(self.blob[:n])
 def test_metadata_negative(self):
  for change in [lambda m:m.update(scene=True),lambda m:m.update(slot=''),lambda m:m.update(default='missing'),lambda m:m['sounds'][0].update(id=True),lambda m:m['sounds'][0].update(loop=False),lambda m:m['sounds'][1].update(id=m['sounds'][0]['id']),lambda m:m['sounds'][0].update(sha256='0'*64),lambda m:m['sounds'][0].update(pcm='../escape.pcm'),lambda m:m['sounds'][0].update(source='res://../escape'),lambda m:m['areas'][0].update(center=[float('nan'),0]),lambda m:m['areas'][0].update(extents=[0,1]),lambda m:m['areas'][0].update(enabled=1),lambda m:m['areas'][0].update(enter='unmapped'),lambda m:m['areas'][0].update(exit_shadow='magic'),lambda m:m['areas'][0].update(mask=True),lambda m:m['areas'].append(copy.deepcopy(m['areas'][0])),lambda m:m.update(extra=1)]:
   bad=copy.deepcopy(self.metadata);change(bad)
   with self.assertRaises(ValueError):s.encode(bad)
 def test_recipe_paths_and_source_coverage(self):
  for k,v in [('schema',True),('schema',2),('commit','0'*40),('field_recipe','../escape'),('areas',['Stepping Sounds/Carpet','Stepping Sounds/Wood']),('areas',['Stepping Sounds/Wood']),('area_scene','Nodes/Reusables/npc.tscn'),('output','../../escape.encsteps')]:
   p=copy.deepcopy(self.p);p[k]=v
   with tempfile.TemporaryDirectory()as tmp:
    path=Path(tmp)/'recipe.json';path.write_text(json.dumps(p),encoding='utf8')
    with self.assertRaises(ValueError):s.source(path)
 def test_output_location_guard(self):
  import subprocess,sys
  out=s.ROOT/'romfs/should-not-be-created-steps'
  r=subprocess.run([sys.executable,str(s.ROOT/'tools/field_footsteps.py'),'--out',str(out)],capture_output=True)
  self.assertNotEqual(r.returncode,0);self.assertFalse(out.exists())
if __name__=='__main__':unittest.main()
