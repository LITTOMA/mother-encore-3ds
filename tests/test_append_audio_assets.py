import copy,json,tempfile,unittest
from unittest import mock
from pathlib import Path
from tools import append_audio_assets as m,audio_asset as a
ROOT=Path(__file__).resolve().parents[1]
class AppendAudio(unittest.TestCase):
 @classmethod
 def setUpClass(c):
  conversion=json.loads((ROOT/'reports/mayor-office-import/steps-audio-receipt.json').read_bytes())['conversion'];c.conversion=conversion;c.bank=a.parse_bank((ROOT/'tests/fixtures/mayor-steps.encaudio').read_bytes());c.recipe=json.loads((ROOT/'content/native-audio.json').read_bytes());c.recipe['assets']=[{k:v for k,v in record.items()if k in ('stable_id','source_path','source_sha256','import_sha256','pcm_path','gain_db')}for record in conversion['assets']]
 def piece(self,indices):
  recipe=copy.deepcopy(self.recipe);recipe['assets']=[recipe['assets'][i]for i in indices];manifest=copy.deepcopy(self.conversion);manifest['recipe_sha256']=a.sha(m.canonical(recipe));manifest['assets']=[manifest['assets'][i]for i in indices];records=[self.bank['assets'][i]for i in indices];raw=a.build_bank(records,self.bank['master_db'],self.bank['silence_db']);files={r['path']:r for r in manifest['files']};manifest['files']=sorted([files[r['pcm_path']]for r in records]+[m.file_record('sound/banks/opening.encaudio',raw)],key=lambda r:r['path']);return recipe,manifest,raw
 def inputs(self):return [*self.piece([0]),self.recipe,*self.piece([1])[1:]]
 def test_exact_append_and_existing_identity(self):
  values=self.inputs();raw,manifest=m.merge(*values);self.assertEqual(raw,(ROOT/'tests/fixtures/mayor-steps.encaudio').read_bytes());self.assertEqual(a.parse_bank(raw)['assets'][0],a.parse_bank(values[2])['assets'][0]);self.assertEqual(manifest['assets'],self.conversion['assets']);m.checked_previous(self.recipe,manifest,raw,'sound/banks/opening.encaudio')
 def test_declared_output_rate_uses_actual_decoded_rate(self):
  recipe,manifest,raw=self.piece([0]);recipe['assets'][0]['output_sample_rate']=self.bank['assets'][0]['sample_rate'];manifest['recipe_sha256']=a.sha(m.canonical(recipe));m.checked_previous(recipe,manifest,raw,'sound/banks/opening.encaudio');recipe['assets'][0]['output_sample_rate']+=1;manifest['recipe_sha256']=a.sha(m.canonical(recipe))
  with self.assertRaises(ValueError):m.checked_previous(recipe,manifest,raw,'sound/banks/opening.encaudio')
 def test_prior_recipe_receipt_and_bank_negative(self):
  for change in [lambda v:v[0].update(extra=1),lambda v:v[1].update(extra=1),lambda v:v[0].update(schema=True),lambda v:v[1].update(schema=True),lambda v:v[1].update(recipe_sha256='0'*64),lambda v:v[1]['assets'][0].update(loop=False),lambda v:v[1]['assets'][0].update(loop=1),lambda v:v[1]['assets'][0].update(pcm_crc32=0),lambda v:v[1]['assets'][0].update(extra=1),lambda v:v[1]['files'][0].update(sha256='0'*64),lambda v:v[1]['assets'][0].update(loop_offset_seconds=1),lambda v:v[1]['assets'][0].update(command=[]),lambda v:v[1].update(ffmpeg_sha256='0'*64)]:
   values=copy.deepcopy(self.inputs());change(values)
   with self.assertRaises(ValueError):m.merge(*values)
 def test_append_boundary_and_decoder_negative(self):
  for change in [lambda v:v[3]['assets'].pop(0),lambda v:v[3]['assets'][0].update(gain_db=-1),lambda v:v[3].update(manager_sha256='0'*64),lambda v:v[4].update(ffmpeg_sha256='1'*64),lambda v:v[4].update(ffmpeg_version='different'),lambda v:v[4].update(recipe_sha256='0'*64),lambda v:v[4]['assets'][0].update(stable_id=1)]:
   values=copy.deepcopy(self.inputs());change(values)
   with self.assertRaises(ValueError):m.merge(*values)
 def test_publication_verification_failure_restores_metadata(self):
  with tempfile.TemporaryDirectory()as tmp:
   root=Path(tmp);meta=root/'bank';payload=root/'new.pcm';meta.write_bytes(b'old')
   def reject():raise ValueError('rejected publication')
   with self.assertRaisesRegex(ValueError,'rejected publication'):m.publish({meta:b'new'},{payload:b'pcm'},reject)
   self.assertEqual(meta.read_bytes(),b'old');self.assertFalse(payload.exists())
 def test_existing_payload_is_never_overwritten_or_removed(self):
  with tempfile.TemporaryDirectory()as tmp:
   root=Path(tmp);meta=root/'bank';payload=root/'existing.pcm';meta.write_bytes(b'old');payload.write_bytes(b'owned elsewhere')
   with self.assertRaises(FileExistsError):m.publish({meta:b'new'},{payload:b'pcm'},lambda:None)
   self.assertEqual(meta.read_bytes(),b'old');self.assertEqual(payload.read_bytes(),b'owned elsewhere')
 def test_partial_payload_write_is_cleaned(self):
  with tempfile.TemporaryDirectory()as tmp:
   root=Path(tmp);meta=root/'bank';payload=root/'new.pcm';meta.write_bytes(b'old');original=Path.open
   class Interrupted:
    def __enter__(self):self.stream=original(payload,'xb');return self
    def write(self,raw):self.stream.write(raw[:1]);raise OSError('interrupted output')
    def __exit__(self,*args):self.stream.close()
   def open_path(path,*args,**kwargs):return Interrupted()if path==payload else original(path,*args,**kwargs)
   with mock.patch.object(Path,'open',open_path):
    with self.assertRaisesRegex(OSError,'interrupted output'):m.publish({meta:b'new'},{payload:b'pcm'},lambda:None)
   self.assertFalse(payload.exists());self.assertEqual(meta.read_bytes(),b'old')
 def test_prior_snapshot_paths_cannot_publish(self):
  with tempfile.TemporaryDirectory(prefix='append-guard-',dir=ROOT/'build')as tmp:
   for base,stage in [(ROOT/'content',False),(Path(tmp)/'prior',True)]:
    args=['append','--out',str(Path(tmp)/'out'),'--base',str(base)]+(['--stage']if stage else [])
    with mock.patch('sys.argv',args):
     with self.assertRaisesRegex(ValueError,'prior snapshots'):m.main()
if __name__=='__main__':unittest.main()
