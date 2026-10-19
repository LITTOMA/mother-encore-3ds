import copy,json,struct,unittest
from tools import run_music_seek_reference as r,audio_asset
class MusicSeekReferenceTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.proof=r.source()[1]
  cls.result=json.loads((r.ROOT/'reports/mayor-office-import/music-seek-native.json').read_bytes())
 def test_reviewed_manager_and_actual_media_dimensions(self):
  self.assertEqual(self.proof,json.loads((r.ROOT/'compatibility/reviews/music-seek-v0410.json').read_bytes()))
  self.assertFalse(self.proof['whole_gameplay_approved'])
  bank=audio_asset.parse_bank((r.ROOT/'romfs/sound/banks/opening.encaudio').read_bytes())
  for imported in self.result['imports']:
   asset=next(v for v in bank['assets']if v['source_path']==imported['source'])
   self.assertAlmostEqual(imported['length'],asset['frames']/asset['sample_rate'],delta=.00002)
 def test_exact_native_fixture_and_float_forwarding(self):
  self.assertEqual(r.fixture(self.result,self.proof),(r.ROOT/'tests/fixtures/music-seek.tsv').read_text(encoding='utf-8'))
  rows=self.result['rows'];self.assertEqual(float(rows[0]['views'][-1][0]['position']),1.25)
  self.assertAlmostEqual(float(rows[2]['views'][-1][0]['position']),struct.unpack('<f',struct.pack('<f',.123456789))[0],delta=1e-15)
  self.assertEqual(float(rows[3]['views'][-1][0]['position']),0)
  self.assertEqual(float(rows[6]['views'][-1][0]['position']),0)
  self.assertEqual(float(rows[7]['views'][-1][0]['position']),2000)
 def test_corrupt_reference_rejected(self):
  changes=[lambda d:d.update(schema=True),lambda d:d['rows'].pop(),lambda d:d['rows'][0]['views'].pop(),lambda d:d['rows'][0]['case']['actions'][0].__setitem__(4,0),lambda d:d['imports'][0].update(loop=1),lambda d:d['imports'][0].update(length=True),lambda d:d['imports'][0].update(length=float('nan')),lambda d:d['rows'][0]['views'][0][0].update(position=0),lambda d:d['rows'][0]['views'][0][0].update(position='nan'),lambda d:d['rows'][0]['views'][0][0].update(position='-1.00000000000000000'),lambda d:d['rows'][0]['views'][0][0].update(position='0.0'),lambda d:d['rows'][0]['views'][0][0].update(id=True),lambda d:d['rows'][0]['views'][0][0].update(index=True),lambda d:d['rows'][0]['views'][0][0].update(stream='res://unreviewed'),lambda d:d['rows'][0]['views'][0].append(copy.deepcopy(d['rows'][0]['views'][0][0]))]
  for change in changes:
   doc=copy.deepcopy(self.result);change(doc)
   with self.assertRaises(ValueError):r.validate_result(doc,self.proof)
if __name__=='__main__':unittest.main()
