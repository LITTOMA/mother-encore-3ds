"""Original Catacombs source review and independently frozen native results."""
import copy,json,tempfile,unittest
from pathlib import Path
from tools import catacombs_music as m,run_catacombs_music_reference as native
from tests import prepare_catacombs_music as prepare
class CatacombsMusicTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.policy,_=m.read();cls.source=m.source();cls.reference=native.source()[1]
 def reject(self,change,full=False):
  with tempfile.TemporaryDirectory(dir=m.ROOT/'build')as folder:
   policy=copy.deepcopy(self.policy);change(policy);p=Path(folder)/'review.json';p.write_bytes(json.dumps(policy).encode('utf-8'))
   with self.assertRaises(ValueError):(m.source if full else m.read)(p)
 def test_original_source_metadata_and_codecs(self):
  _,metadata,geometry,proof=self.source;self.assertEqual(len(metadata['regions']),5);self.assertEqual(len(metadata['tracks']),2);self.assertEqual(len(m.codec.encode(metadata,geometry)[0]),850);self.assertEqual([r['volume_db']for r in metadata['regions']],[0,-10,-5,-10,-15]);self.assertEqual([v['offset']for v in proof['imports']],[3.77,16.694])
 def test_unknown_schema_kind_pin_and_field_rejected(self):
  for f in [lambda v:v.update(schema=2),lambda v:v.update(schema=True),lambda v:v.update(kind='unknown'),lambda v:v.update(commit='0'*40),lambda v:v.update(extra=1)]:self.reject(f)
 def test_source_changes_and_missing_import_rejected(self):
  self.reject(lambda v:v['sources'].update({v['scene']:'0'*64}));self.reject(lambda v:v['sources'].pop('Audio/Music/Underground.ogg.import'),True)
 def test_area_identity_and_exact_source_coverage(self):
  self.reject(lambda v:v['areas'][0].update(id=1));self.reject(lambda v:v['areas'].append(copy.deepcopy(v['areas'][0])));self.reject(lambda v:v['areas'].pop(),True);self.reject(lambda v:v['areas'].reverse(),True)
 def test_unsafe_output_and_source_paths_rejected(self):
  self.reject(lambda v:v['output'].update(binding='sound/banks/../bad.encmusic'));self.reject(lambda v:v.update(scene='../map.tscn'));self.reject(lambda v:v.update(pcm_prefix='../music'))
 def test_duplicate_and_nonfinite_json_rejected(self):
  with tempfile.TemporaryDirectory(dir=m.ROOT/'build')as folder:
   p=Path(folder)/'review.json'
   for text in ['{"schema":1,"schema":1}','{"schema":NaN}']:
    p.write_bytes(text.encode())
    with self.assertRaises(ValueError):m.read(p)
 def test_actual_native_repetitions_and_frozen_review(self):
  proof,tsv,geometry=prepare.fixture();self.assertEqual(proof,self.reference);self.assertEqual(len(proof['cases']),10);self.assertEqual(len(geometry['rows']),5);self.assertIn(b'# original scene callbacks',tsv)
 def test_native_geometry_and_protocol_corruption_rejected(self):
  root=m.ROOT/'reports/catacombs-music/native-2';geometry=json.loads((root/'geometry.json').read_bytes());geometry['rows'][0]['gain']=100
  with self.assertRaises(ValueError):native.validate_geometry(geometry,self.reference['source'])
  result=json.loads((root/'reference.json').read_bytes());result['rows'][0]['case']['name']='wrong'
  with self.assertRaises(ValueError):native.validate_result(result,self.reference)
 def test_converted_audio_metadata_rebuilds_exactly(self):
  bank=prepare.audio_bank(self.source[1]);self.assertEqual(len(bank),401);self.assertEqual(bank[:8],b'ENCAUD01')
 def test_metadata_unknown_tracks_and_geometry_rejected(self):
  for change in [lambda a,b:a['regions'][0].update(track_id=0),lambda a,b:a['regions'][0].update(appear_flag='../flag'),lambda a,b:b[0]['polygons'][0].__setitem__(0,[float('inf'),0])]:
   metadata=copy.deepcopy(self.source[1]);geometry=copy.deepcopy(self.source[2]);change(metadata,geometry)
   with self.assertRaises(ValueError):m.codec.encode(metadata,geometry)
 def test_cli_output_guard_runs_before_creating_files(self):
  for out in [m.ROOT/'docs/music-guard-must-not-create',m.ROOT/'build']:
   with self.assertRaises(ValueError):m.compile(out)
   if out!=m.ROOT/'build':self.assertFalse(out.exists())
if __name__=='__main__':unittest.main()