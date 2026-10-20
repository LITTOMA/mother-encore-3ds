import hashlib,json,shutil,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
from tests import prepare_dungeon_mechanism_bindings as f

class MechanismFixture(unittest.TestCase):
 def copy_reference(self,base):
  report=base/'reference';report.mkdir();shutil.copyfile(f.REPORT/'verification.json',report/'verification.json')
  for n in (5,6):
   target=report/f'original-{n}';target.mkdir()
   for name in ('observations.json','input.json','receipt.json'):shutil.copyfile(f.REPORT/f'original-{n}'/name,target/name)
  return report
 def resign_negative_copy(self,report,path):
  audit=report/'verification.json';data=json.loads(audit.read_bytes());data['evidence_sha256'][path.relative_to(report).as_posix()]=hashlib.sha256(path.read_bytes()).hexdigest();audit.write_text(json.dumps(data),encoding='utf8')
 def test_exact_frozen_projection_and_binary(self):
  with tempfile.TemporaryDirectory(dir=f.ROOT/'build',prefix='mechanism-fixture-positive-')as directory:
   out=Path(directory)/'candidate';f.build(out);lines=(out/'reference.tsv').read_text().splitlines();self.assertEqual(sum(n.startswith('step ')for n in lines),232);self.assertEqual(sum(n.startswith('node ')for n in lines),232*14)
   self.assertEqual((out/'source.encdma').stat().st_size,4060);self.assertIn('step wall_timer_complete_before_camera_return idle_wall ',(out/'reference.tsv').read_text())
 def test_changed_frozen_bytes_reject_before_candidate_write(self):
  with tempfile.TemporaryDirectory(dir=f.ROOT/'build',prefix='mechanism-fixture-digest-')as directory:
   base=Path(directory);report=self.copy_reference(base);path=report/'original-6/observations.json';path.write_bytes(path.read_bytes()+b' ')
   with patch.object(f,'REPORT',report),self.assertRaises(ValueError):f.build(base/'candidate')
   self.assertFalse((base/'candidate').exists())
 def test_wrong_producer_with_valid_test_digest_rejects(self):
  with tempfile.TemporaryDirectory(dir=f.ROOT/'build',prefix='mechanism-fixture-producer-')as directory:
   base=Path(directory);report=self.copy_reference(base);path=report/'original-6/receipt.json';p=json.loads(path.read_bytes());p['producer_sha256']='0'*64;path.write_text(json.dumps(p),encoding='utf8');self.resign_negative_copy(report,path)
   with patch.object(f,'REPORT',report),self.assertRaises(ValueError):f.build(base/'candidate')
   self.assertFalse((base/'candidate').exists())
 def test_unknown_frame_action_with_valid_test_digests_rejects(self):
  with tempfile.TemporaryDirectory(dir=f.ROOT/'build',prefix='mechanism-fixture-action-')as directory:
   base=Path(directory);report=self.copy_reference(base)
   for n in (5,6):
    path=report/f'original-{n}/observations.json';p=json.loads(path.read_bytes());p['rows'][0]['action']='unknown/0';path.write_text(json.dumps(p),encoding='utf8');self.resign_negative_copy(report,path)
   with patch.object(f,'REPORT',report),self.assertRaises(ValueError):f.build(base/'candidate')
   self.assertFalse((base/'candidate/reference.tsv').exists())
 def test_wrong_original_engine_with_valid_test_digests_rejects(self):
  with tempfile.TemporaryDirectory(dir=f.ROOT/'build',prefix='mechanism-fixture-engine-')as directory:
   base=Path(directory);report=self.copy_reference(base)
   for n in (5,6):
    path=report/f'original-{n}/observations.json';p=json.loads(path.read_bytes());p['engine']['major']=4;path.write_text(json.dumps(p),encoding='utf8');self.resign_negative_copy(report,path)
   with patch.object(f,'REPORT',report),self.assertRaises(ValueError):f.build(base/'candidate')
   self.assertFalse((base/'candidate').exists())
 def test_unsafe_fixture_root_rejected(self):
  for out in (f.ROOT,f.ROOT/'build'):
   with self.subTest(out=out),self.assertRaises(ValueError):f.build(out)

if __name__=='__main__':unittest.main()
