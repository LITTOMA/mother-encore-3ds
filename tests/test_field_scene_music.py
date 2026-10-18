import copy,json,struct,tempfile,unittest,zlib
from pathlib import Path
from tools import field_scene_music as m
class FieldSceneMusic(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.policy,cls.metadata,cls.polygons,cls.sources=m.source()
 def test_actual_source_values_and_geometry(self):
  r=self.metadata['regions'][0];self.assertEqual((r['source_path'],r['volume_db'],r['fadein_seconds'],r['fadeout_seconds']),('MusicArea',6,1,1.5));self.assertEqual(r['shape_paths'],['MusicArea/CollisionShape2D']);self.assertFalse(r['disabled'])
  self.assertEqual(self.metadata['tracks'][0]['source_path'],'res://Audio/Music/Town Hall.mp3');self.assertEqual(self.metadata['tracks'][0]['gain_db'],0)
  self.assertEqual(self.polygons,[{'source':'MusicArea','polygons':[[[-240,-736],[240,-736],[240,40],[-240,40]]]}])
 def test_frozen_metadata_and_independent_versions(self):
  bank,zones=m.encode(self.metadata,self.polygons)
  self.assertEqual(bank,(m.ROOT/'tests/fixtures/mayor.encmusic').read_bytes());self.assertEqual(zones,(m.ROOT/'tests/fixtures/mayor.enczones').read_bytes())
  self.assertEqual(struct.unpack_from('<I',bank,8)[0],1);self.assertEqual(struct.unpack_from('<I',zones,8)[0],2);self.assertEqual(zlib.crc32(zones[24:]),struct.unpack_from('<I',zones,16)[0])
 def test_metadata_unknown_and_numeric_paths(self):
  for change in [lambda x:x.update(extra=1),lambda x:x.update(silence_db=True),lambda x:x.update(fade_to_seconds=float('nan')),lambda x:x['tracks'][0].update(stable_id=True),lambda x:x['tracks'][0].update(pcm_path='../escape.pcm'),lambda x:x['tracks'][0].update(source_sha256='0'*64),lambda x:x['tracks'][0].update(gain_db=1),lambda x:x['regions'][0].update(track_id=1),lambda x:x['regions'][0].update(volume_db=25),lambda x:x['regions'][0].update(disabled=1),lambda x:x['regions'][0].update(shape_paths=['Other/Shape']),lambda x:x['regions'][0].update(source_path='../escape'),lambda x:x['regions'][0].update(appear_flag='invalid:flag')]:
   changed=copy.deepcopy(self.metadata);change(changed)
   with self.assertRaises(ValueError):m.encode(changed,self.polygons)
 def test_geometry_unknown_or_degenerate(self):
  for change in [lambda x:x.pop(),lambda x:x[0].update(source='Elsewhere'),lambda x:x[0]['polygons'][0].pop(),lambda x:x[0]['polygons'][0][0].__setitem__(0,float('inf')),lambda x:x[0]['polygons'][0].__setitem__(2,[-240,-736])]:
   changed=copy.deepcopy(self.polygons);change(changed)
   with self.assertRaises(ValueError):m.encode(self.metadata,changed)
 def test_recipe_missing_source_version_and_owner(self):
  for change in [lambda x:x.update(schema=True),lambda x:x.update(schema=2),lambda x:x.update(extra=1),lambda x:x.update(areas=['missing']),lambda x:x.update(areas=['Objects/npc']),lambda x:x.update(areas=['MusicArea','MusicArea']),lambda x:x.update(audio='romfs/bank.encaudio'),lambda x:x.update(field_recipe='../escape.json'),lambda x:x.update(pcm_prefix='../../escape'),lambda x:x.update(scene='Maps/podunk/podunk.tscn')]:
   changed=copy.deepcopy(self.policy);change(changed)
   with tempfile.TemporaryDirectory()as tmp:
    p=Path(tmp)/'music.json';p.write_text(json.dumps(changed),encoding='utf8')
    with self.assertRaises(ValueError):m.source(p)
  for path in ['../escape',str(m.ROOT/'romfs/system/native.encresources')]:
   with self.assertRaises(ValueError):m.npc_target((m.ROOT/'build/music-test').resolve(),path)
 def test_existing_podunk_binding_remains_exact(self):
  from tools.podunk_music import checked
  recipe=json.loads((m.ROOT/'content/podunk-music.json').read_bytes())
  self.assertEqual(checked(recipe,m.ROOT/'upstream/MOTHER-Encore'),(m.ROOT/'romfs/sound/banks/podunk.encmusic').read_bytes())
if __name__=='__main__':unittest.main()
