"""Manual source-binding cases. Not registered in ordinary builds."""
import copy
import unittest
from tools.relink_dialogue_audio import verify_bank_projection
from tools import field_dialogue_audio as source


class DialogueAudioRelink(unittest.TestCase):
    def setUp(self):
        self.reviewed = source.read(source.IR)
        self.bank = source.read(source.ROOT / 'content/native-audio.json')

    def test_unchanged_mp3_projection(self):
        verify_bank_projection(self.reviewed, self.bank)

    def test_unrelated_wav_binding_preserves_reviewed_streams(self):
        bank = copy.deepcopy(self.bank)
        bank['assets'].append(dict(stable_id=0xFFFFFFFE,
                                   source_path='res://manual-extra.wav',
                                   source_sha256='0' * 64))
        verify_bank_projection(self.reviewed, bank)

    def test_changed_bus_pin_or_stream_rejected(self):
        for field in ('bus_sha256', 'upstream_commit'):
            bank = copy.deepcopy(self.bank)
            bank[field] = '0' * len(bank[field])
            with self.assertRaises(ValueError):
                verify_bank_projection(self.reviewed, bank)
        first = next(i for i, a in enumerate(self.bank['assets'])
                     if a['source_path'].endswith('.mp3'))
        for field, value in (('stable_id', 0), ('source_sha256', '0' * 64),
                             ('source_path', 'res://unknown.mp3')):
            bank = copy.deepcopy(self.bank)
            bank['assets'][first][field] = value
            with self.assertRaises(ValueError):
                verify_bank_projection(self.reviewed, bank)
        bank = copy.deepcopy(self.bank)
        bank['assets'].pop(first)
        with self.assertRaises(ValueError):
            verify_bank_projection(self.reviewed, bank)
        bank = copy.deepcopy(self.bank)
        bank['assets'].append(copy.deepcopy(bank['assets'][first]))
        with self.assertRaises(ValueError):
            verify_bank_projection(self.reviewed, bank)


if __name__ == '__main__':
    unittest.main()
