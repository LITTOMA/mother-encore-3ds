import copy,json,unittest
from tools import music_library as r,audio_asset
class MusicLibraryTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.recipe,cls.metadata,cls.records,cls.bank,cls.proof=r.source()
 def test_exact_sources_and_generator_fixtures(self):
  self.assertEqual(self.proof,json.loads((r.ROOT/'compatibility/reviews/music-library-v0410.json').read_bytes()))
  self.assertFalse(self.proof['release_staged'])
  self.assertEqual(r.encode(self.metadata),(r.ROOT/'tests/fixtures/global-music.encmusic').read_bytes())
  for ident,label in [(1001,'intro'),(34,'house'),(4107189403,'victory')]:
   index=next(i for i,t in enumerate(self.metadata['tracks'])if t['id']==ident)
   self.assertEqual(r.encode({**self.metadata,'tracks':[self.metadata['tracks'][index]]}),(r.ROOT/('tests/fixtures/global-'+label+'.encmusic')).read_bytes())
   self.assertEqual(audio_asset.build_bank([self.records[index]],self.bank['master_db'],self.bank['silence_db']),(r.ROOT/('tests/fixtures/global-'+label+'.encaudio')).read_bytes())
 def test_reject_changed_or_partial_scope_recipes(self):
  changes=[lambda p:p.update(schema=True),lambda p:p.update(kind='unknown'),lambda p:p.update(source_commit='0'*40),lambda p:p.update(audio_recipe='../escape'),lambda p:p.update(audio_bank='../escape'),lambda p:p.update(default_gain_db=True),lambda p:p.update(default_gain_db=float('nan')),lambda p:p.update(default_gain_db=1),lambda p:p['tracks'].pop(),lambda p:p['tracks'].append(p['tracks'][0]),lambda p:p['tracks'].__setitem__(0,True),lambda p:p['tracks'].__setitem__(0,22),lambda p:p.update(extra=1)]
  for change in changes:
   value=copy.deepcopy(self.recipe);change(value)
   with self.assertRaises(ValueError):r.source(value)
 def test_bad_metadata_encoder_paths(self):
  changes=[lambda p:p.update(extra=1),lambda p:p.update(silence_db=True),lambda p:p.update(fade_to_seconds=float('inf')),lambda p:p.update(empty_player_gain_db=25),lambda p:p['tracks'].clear(),lambda p:p['tracks'].append(copy.deepcopy(p['tracks'][0])),lambda p:p['tracks'][0].update(id=True),lambda p:p['tracks'][0].update(builtin_loop=1),lambda p:p['tracks'][0].update(source_sha256='0'*64),lambda p:p['tracks'][0].update(import_sha256='g'*64),lambda p:p['tracks'][0].update(source_path='res://../escape'),lambda p:p['tracks'][0].update(source_path='res://bad//path'),lambda p:p['tracks'][0].update(source_path='res://bad\\path')]
  for change in changes:
   value=copy.deepcopy(self.metadata);change(value)
   with self.assertRaises(ValueError):r.encode(value)
if __name__=='__main__':unittest.main()
