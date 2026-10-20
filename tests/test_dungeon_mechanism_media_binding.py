import copy,hashlib,json,struct,tempfile,unittest,zlib
from pathlib import Path
from tools import dungeon_mechanism_media as m,dungeon_mechanism_presentation as d,audio_asset
from tools.podunk_field import pack_container,unpack_container

class MechanismMediaBinding(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.temp=tempfile.TemporaryDirectory(dir=m.ROOT/'build',prefix='mechanism-media-binding-');cls.out=Path(cls.temp.name)/'candidate';cls.proof=m.build(cls.out);cls.blob=(cls.out/'source.encdmp').read_bytes();cls.model=d.parse(cls.blob);cls.review=m.read()
 @classmethod
 def tearDownClass(cls):cls.temp.cleanup()
 def reject_review(self,edit,build=False):
  p=copy.deepcopy(self.review);edit(p)
  with tempfile.TemporaryDirectory(dir=m.ROOT/'build',prefix='mechanism-media-review-')as directory:
   file=Path(directory)/'review.json';file.write_text(json.dumps(p),encoding='utf8');out=Path(directory)/'out'
   with self.assertRaises(ValueError):(m.build(out,file)if build else m.read(file))
   self.assertFalse(out.exists())
 def changed(self,at,value):
  b=bytearray(self.blob);struct.pack_into('<I',b,at,value);struct.pack_into('<I',b,16,zlib.crc32(b[32:]));return b
 def test_sound_owners_bank_and_old_capability_geometry(self):
  self.assertEqual(len(self.blob),9192);self.assertEqual(len(self.model['sounds']),7);self.assertEqual(sum(s['kind']==1 for s in self.model['sounds']),6);self.assertEqual(sum(s['kind']==2 for s in self.model['sounds']),1)
  bank=audio_asset.parse_bank(self.model['audio_bank']);self.assertEqual(len(bank['assets']),4);self.assertFalse(any(a['loop']for a in bank['assets']));self.assertTrue(all('/Sound effects/'in a['source_path']for a in bank['assets']))
  old=d.extract()[1];self.assertEqual(len(old),7356);legacy=d.parse(old);self.assertNotIn('sounds',legacy)
  for k,v in legacy.items():self.assertEqual(self.model[k],v)
  self.assertFalse(self.proof['live_output_admission'])
 def test_copied_original_pcm_and_temporary_decoder_cleanup(self):
  proof=self.proof['audio'];self.assertEqual(proof['pcm_bytes'],409260);self.assertEqual(len(proof['files']),5)
  for file in proof['files']:
   data=(self.out/file['path']).read_bytes();self.assertEqual(len(data),file['bytes']);self.assertEqual(hashlib.sha256(data).hexdigest(),file['sha256'])
  self.assertFalse(any(self.out.glob('decoded-*')))
 def test_schema_review_dependency_engine_and_json_reject(self):
  for edit in [lambda p:p.update(schema=True),lambda p:p.update(schema=2),lambda p:p.update(kind='unknown'),lambda p:p.update(commit='0'*40),lambda p:p.update(scope=''),lambda p:p.update(extra=1),lambda p:p['dependencies']['audio'].update(sha256='0'*64),lambda p:p['engine'].update(commit='0'*40),lambda p:p['engine'].update(source='https://example.invalid'),lambda p:p['engine'].update(header_source='https://example.invalid'),lambda p:p['engine'].update(header_sha256='0'*64),lambda p:p['engine'].update(functions={}),lambda p:p.update(denied_owner_path='../escape')]:
   with self.subTest(edit=edit):self.reject_review(edit)
  with tempfile.TemporaryDirectory(dir=m.ROOT/'build',prefix='mechanism-media-json-')as directory:
   file=Path(directory)/'review.json';file.write_text('{"schema":1,"schema":1}',encoding='utf8')
   with self.assertRaises(ValueError):m.read(file)
 def test_actual_shared_autoload_owner_mismatch_and_output_escape_reject(self):
  self.reject_review(lambda p:p.update(denied_owner_path='audioManager'),True)
  for out in (m.ROOT,m.ROOT/'build',m.ROOT/'romfs'):
   with self.subTest(out=out),self.assertRaises(ValueError):m.build(out)
 def test_all_truncations_and_capability_section_paths_reject(self):
  for size in range(len(self.blob)):
   with self.assertRaises(ValueError):d.parse(self.blob[:size])
  for at,value in [(8,2),(20,1),(20,3),(52,5),(144,7),(148,0),(152,0),(152,258),(156,84),(160,6),(164,0),(168,0)]:
   with self.subTest(at=at),self.assertRaises(ValueError):d.parse(self.changed(at,value))
 def test_sound_owner_source_identity_flags_and_wall_binding_reject(self):
  sound=struct.unpack_from('<I',self.blob,148)[0];bank=struct.unpack_from('<I',self.blob,164)[0]
  for at,value in [(sound,0),(sound+4,0),(sound+4,3),(sound+24,0),(sound+56,0),(sound+60,0x7fc00000),(sound+64,0),(sound+64,0x40800001),(sound+68,2),(sound+72,1),(sound+76,1),(sound+84,1),(bank+8,2),(bank+20,5),(sound+88,self.model['sounds'][0]['owner'])]:
   with self.subTest(at=at),self.assertRaises(ValueError):d.parse(self.changed(at,value))
  wall=next(p['wall']['owner']for p in d.bindings.extract()[0]['profiles']if p['family']==4)
  index=next(i for i,s in enumerate(self.model['sounds'])if s['owner']==wall)
  with self.assertRaises(ValueError):d.parse(self.changed(sound+index*88+60,0x3f800000))
 def test_unowned_bank_asset_missing_owner_and_channel_reject(self):
  scene,parts=unpack_container(self.blob,d.MAGIC,d.MEDIA_STRIDES,d.LIMIT,capability=2);bank=audio_asset.parse_bank(parts[6]);extra=copy.deepcopy(bank['assets'][-1]);extra.update(stable_id=1,source_path='res://Audio/Sound effects/Synthetic negative.wav',pcm_path='sound/effects/synthetic-negative.pcm')
  newer=list(parts);newer[6]=audio_asset.build_bank(bank['assets']+[extra],bank['master_db'],bank['silence_db']);bad=pack_container(d.MAGIC,scene,newer,d.MEDIA_STRIDES,d.LIMIT,capability=2)
  with self.assertRaises(ValueError):d.parse(bad)
  newer=list(parts);newer[5]=newer[5][88:]
  with self.assertRaises(ValueError):d.parse(pack_container(d.MAGIC,scene,newer,d.MEDIA_STRIDES,d.LIMIT,capability=2))
  at=struct.unpack_from('<I',self.blob,148)[0];index=next(i for i,s in enumerate(self.model['sounds'])if s['kind']==2)
  with self.assertRaises(ValueError):d.parse(self.changed(at+index*88+80,0))

if __name__=='__main__':unittest.main()
