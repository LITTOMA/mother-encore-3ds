"""Manual checks for the source-bound Pause audio admission.

This suite is retained for explicitly requested comprehensive testing only.
The negative cases isolate binding validation with detached dictionaries; they
never convert sound, overwrite resource IR, or substitute fixture game audio.
"""
import copy
import json
from pathlib import Path
import sys
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools import field_audio, present_audio


class FieldAudioBinding(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ir = json.loads((ROOT / 'content/native-field-equipment.json').read_text(encoding='utf-8'))
        cls.config = json.loads((ROOT / 'content/field-audio-binding.json').read_text(encoding='utf-8'))
        cls.present = present_audio.bindings(ROOT)

    def validate_detached(self, config=None, ir=None):
        config = copy.deepcopy(self.config if config is None else config)
        ir = copy.deepcopy(self.ir if ir is None else ir)
        with mock.patch.object(field_audio, 'load', return_value=ir) as load, \
                mock.patch.object(field_audio, 'read_json', return_value=config) as read:
            result = field_audio.bindings(ROOT)
            load.assert_called_once_with(ROOT)
            read.assert_called_once_with(ROOT / 'content/field-audio-binding.json')
            return result

    def combined(self, pause=None):
        return (self.config['assets'] if pause is None else pause) + self.present

    def reject(self, change, ir_change=None):
        config, ir = copy.deepcopy(self.config), copy.deepcopy(self.ir)
        change(config)
        if ir_change:
            ir_change(ir)
        with self.assertRaises((ValueError, TypeError, KeyError)):
            self.validate_detached(config, ir)

    def test_actual_reviewed_source_binding(self):
        # This is the actual source admission, not the detached parser double.
        self.assertEqual(field_audio.bindings(ROOT), self.combined())

    def test_detached_valid_binding(self):
        self.assertEqual(self.validate_detached(), self.combined())

    def test_source_admission_failure_propagates(self):
        with mock.patch.object(field_audio, 'load', side_effect=ValueError('unreviewed field source')), \
                mock.patch.object(field_audio, 'read_json') as read:
            with self.assertRaisesRegex(ValueError, 'unreviewed field source'):
                field_audio.bindings(ROOT)
            read.assert_not_called()

    def test_unknown_and_missing_top_level_fields(self):
        self.reject(lambda c: c.update(unknown=True))
        for name in self.config:
            with self.subTest(field=name):
                self.reject(lambda c, name=name: c.pop(name))

    def test_unknown_schema_kind_pin(self):
        for value in (0, 2, True, 1.0, '1', None):
            with self.subTest(schema=value):
                self.reject(lambda c, value=value: c.update(schema=value))
        for name, value in (('kind', 'encore.field-audio.unknown'),
                            ('commit', '0' * 40), ('commit', None)):
            with self.subTest(field=name, value=value):
                self.reject(lambda c, name=name, value=value: c.update({name: value}))

    def test_license_review_required(self):
        for value in ('', ' \t\r\n', None, False, []):
            with self.subTest(value=value):
                self.reject(lambda c, value=value: c.update(license_review=value))

    def test_coverage_and_asset_container(self):
        for value in (None, {}, (), 'sounds', []):
            with self.subTest(value=value):
                self.reject(lambda c, value=value: c.update(assets=value))
        self.reject(lambda c: c['assets'].pop())
        self.reject(lambda c: c['assets'].append(copy.deepcopy(c['assets'][0])))

    def test_unknown_and_missing_row_fields(self):
        self.reject(lambda c: c['assets'][0].update(unknown=True))
        for name in self.config['assets'][0]:
            with self.subTest(field=name):
                self.reject(lambda c, name=name: c['assets'][0].pop(name))

    def test_stable_identity_shape(self):
        self.reject(lambda c: c['assets'][0]['identity'].update(source='menu_open'))
        self.reject(lambda c: c['assets'][0]['identity'].pop('kind'))
        self.reject(lambda c: c['assets'][0]['identity'].pop('value'))
        for value in ('source', 'unknown', None):
            with self.subTest(kind=value):
                self.reject(lambda c, value=value: c['assets'][0]['identity'].update(kind=value))

    def test_invalid_and_duplicate_stable_ids(self):
        for value in (0, -1, 2 ** 32, True, 1601.0, '1601', None):
            with self.subTest(identity=value):
                self.reject(lambda c, value=value: c['assets'][0]['identity'].update(value=value))
        self.reject(lambda c: c['assets'][1]['identity'].update(value=c['assets'][0]['identity']['value']))

    def test_unsigned_stable_identity_boundary(self):
        c = copy.deepcopy(self.config)
        c['assets'][0]['identity']['value'] = 2 ** 32 - 1
        self.assertEqual(self.validate_detached(c), self.combined(c['assets']))

    def test_unknown_duplicate_and_unsafe_source(self):
        for value in ('Audio/Sound effects/M3/menu_open.wav',
                      '../Audio/Sound effects/M3/menu_open2.wav',
                      '/Audio/Sound effects/M3/menu_open2.wav',
                      'Audio\\Sound effects\\M3\\menu_open2.wav',
                      'res://Audio/Sound effects/M3/menu_open2.wav',
                      'Audio//Sound effects/M3/menu_open2.wav',
                      'Audio/Sound effects/M3/menu_open2.wav\n', '', None):
            with self.subTest(source=value):
                self.reject(lambda c, value=value: c['assets'][0].update(source=value))
        self.reject(lambda c: c['assets'][1].update(source=c['assets'][0]['source']))

    def test_source_and_import_provenance_required(self):
        source = self.config['assets'][0]['source']
        self.reject(lambda c: None, lambda ir: ir['sources'].pop(source))
        self.reject(lambda c: None, lambda ir: ir['sources'].pop(source + '.import'))

    def test_source_coverage_matches_actual_pause_bindings(self):
        self.reject(lambda c: None,
                    lambda ir: ir['bindings']['PauseOpenSound'].update(en='Audio/Sound effects/M3/menu_open.wav'))

    def test_unknown_conversions(self):
        for value in ({}, {'output_sample_rate': 22050}, 1, False, 'wav-to-pcm', []):
            with self.subTest(conversion=value):
                self.reject(lambda c, value=value: c['assets'][0].update(conversion=value))

    def test_nonzero_nonfinite_and_unknown_gain(self):
        for value in (1, -1, .1, float('nan'), float('inf'), -float('inf'), True, False, '0', None):
            with self.subTest(gain=value):
                self.reject(lambda c, value=value: c['assets'][0].update(gain_db=value))

    def test_invalid_and_duplicate_pcm_paths(self):
        for value in ('data/menu.pcm', 'sound/music/menu.pcm',
                      'sound/effects/menu.wav', 'sound/effects/menu.PCM',
                      '../sound/effects/menu.pcm', '/sound/effects/menu.pcm',
                      'sound/effects/../menu.pcm', 'sound//effects/menu.pcm',
                      'sound\\effects\\menu.pcm', 'romfs:/sound/effects/menu.pcm',
                      'sound/effects/menu.pcm\x7f', '', None):
            with self.subTest(pcm=value):
                self.reject(lambda c, value=value: c['assets'][0].update(pcm=value))
        self.reject(lambda c: c['assets'][1].update(pcm=c['assets'][0]['pcm']))


if __name__ == '__main__':
    unittest.main()
