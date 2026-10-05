"""Manual source-coverage negatives. Not run automatically during development."""
import copy,sys,unittest
from pathlib import Path
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import basement_audio as audio
from tools.drawer_program import read_json
class BasementAudio(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.config=read_json(ROOT/'content/basement-audio-binding.json')
    def reject(self,change):
        value=copy.deepcopy(self.config);change(value)
        with patch.object(audio,'read_json',return_value=value),self.assertRaises((ValueError,KeyError,TypeError)):audio.bindings(ROOT)
    def test_source_complete(self):self.assertEqual(audio.bindings(ROOT),self.config['assets']+self.config['music_assets'])
    def test_unknown_field(self):self.reject(lambda c:c.update(arbitrary_sound=True))
    def test_unknown_pin(self):self.reject(lambda c:c.update(commit='0'*40))
    def test_unknown_version(self):self.reject(lambda c:c.update(schema=2))
    def test_missing_sound(self):self.reject(lambda c:c['assets'].pop())
    def test_duplicate_identity(self):self.reject(lambda c:c['assets'][1]['identity'].update(value=c['assets'][0]['identity']['value']))
    def test_duplicate_source(self):self.reject(lambda c:c['assets'][1].update(source=c['assets'][0]['source']))
    def test_unknown_source(self):self.reject(lambda c:c['assets'][0].update(source='Audio/Unreviewed.wav'))
    def test_pcm_escape(self):self.reject(lambda c:c['assets'][0].update(pcm='../outside.pcm'))
    def test_unreviewed_gain(self):self.reject(lambda c:c['assets'][0].update(gain_db=-3))
    def test_nonfinite_gain(self):self.reject(lambda c:c['assets'][0].update(gain_db=float('nan')))
    def test_unreviewed_conversion(self):self.reject(lambda c:c['assets'][0].update(conversion={'output_sample_rate':8000}))
    def test_shared_id_changed(self):self.reject(lambda c:c['shared_assets'][0]['identity'].update(value=9001))
    def test_shared_sound_removed(self):self.reject(lambda c:c['shared_assets'].clear())
if __name__=='__main__':unittest.main()
