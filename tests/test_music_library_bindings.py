import copy,json,struct,unittest,zlib
from tools import music_library_bindings as r
class MusicLibraryBindingsTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.recipe,cls.doc,cls.files,cls.proof=r.source()
 def test_source_proof_and_exact_generated_bundle(self):
  self.assertEqual(self.proof,json.loads((r.ROOT/'compatibility/reviews/music-library-bindings-v0410.json').read_bytes()))
  self.assertFalse(self.proof['release_staged']);self.assertEqual(len(self.files),28)
  for name,data in self.files.items():self.assertEqual(data,(r.ROOT/'tests/fixtures/global-library'/name).read_bytes())
  self.assertEqual(r.decode(r.encode(self.doc)),self.doc)
 def test_partial_changed_unknown_recipe_rejected(self):
  changes=[lambda p:p.update(schema=True),lambda p:p.update(kind='unknown'),lambda p:p.update(source_commit='0'*40),lambda p:p.update(extra=1),lambda p:p.update(output='../escape.enclib'),lambda p:p.update(index='other.encmusic'),lambda p:p['tracks'].pop(),lambda p:p['tracks'][0].update(track=True),lambda p:p['tracks'][0].update(track=0),lambda p:p['tracks'][0].update(metadata='other.encmusic'),lambda p:p['tracks'][0].update(audio='other.encaudio'),lambda p:p['tracks'][0].update(extra=1)]
  for change in changes:
   p=copy.deepcopy(self.recipe);change(p)
   with self.assertRaises(ValueError):r.source(p)
 def test_encoder_schema_path_fingerprint_rejected(self):
  changes=[lambda d:d.update(extra=1),lambda d:d['bindings'].clear(),lambda d:d['bindings'].append(copy.deepcopy(d['bindings'][0])),lambda d:d['index'].update(bytes=True),lambda d:d['index'].update(bytes=65537),lambda d:d['index'].update(crc=-1),lambda d:d['index'].update(crc=True),lambda d:d['bindings'][0].update(track=0),lambda d:d['bindings'][0].update(track=True),lambda d:d['bindings'][0]['metadata'].update(path=d['index']['path']),lambda d:d['bindings'][0]['audio'].update(path='bad.encmusic')]
  for name in ['../x.encmusic','/x.encmusic','bad//x.encmusic','bad\\x.encmusic','x:y.encmusic','bad/./x.encmusic','bad/#x.encmusic','bad/ x.encmusic','é.encmusic']:
   changes.append(lambda d,name=name:d['index'].update(path=name))
  for change in changes:
   d=copy.deepcopy(self.doc);change(d)
   with self.assertRaises(ValueError):r.encode(d)
 def test_decoder_negative_versions_and_records(self):
  raw=r.encode(self.doc)
  for size in range(len(raw)):
   with self.assertRaises(ValueError):r.decode(raw[:size])
  def change(offset,value):
   data=bytearray(raw);struct.pack_into('<I',data,offset,value);struct.pack_into('<I',data,16,0);struct.pack_into('<I',data,16,zlib.crc32(data));return bytes(data)
  for offset,value in [(8,0),(8,2),(12,len(raw)+1),(20,0),(20,2),(24,0),(24,65),(28,1),(32,0),(32,65537),(40,0),(40,257)]:
   with self.assertRaises(ValueError):r.decode(change(offset,value))
  for data in [bytearray(raw),raw+b'x',bytes([raw[0]^1])+raw[1:],raw[:60]+bytes([raw[60]^1])+raw[61:]]:
   with self.assertRaises(ValueError):r.decode(data)
if __name__=='__main__':unittest.main()
