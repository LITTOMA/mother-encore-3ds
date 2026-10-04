import copy
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import audio_asset
import restore_audio

ROOT = Path(__file__).resolve().parents[1]


class RestoreAudioTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name) / 'project'
        self.output = self.root / 'romfs'
        self.upstream = self.root / 'upstream/MOTHER-Encore'
        self.banks = (('content/audio-test.json', 'content/asset-receipts/audio/test.json', 'data/test.encaudio'),)
        recipe = json.loads((ROOT / 'content/native-audio.json').read_bytes())
        original = json.loads((ROOT / 'content/asset-receipts/audio/opening.json').read_bytes())
        self.path = 'sound/effects/cursor-back.pcm'
        entry = next(x for x in recipe['assets'] if x['pcm_path'] == self.path)
        self.receipt = next(x for x in original['assets'] if x['pcm_path'] == self.path)
        self.pcm = (ROOT / 'romfs' / self.path).read_bytes()
        recipe['assets'] = [entry]
        self.recipe = recipe
        self.manifest = copy.deepcopy(original)
        self.manifest['assets'] = [self.receipt]
        self.manifest['recipe_sha256'] = audio_asset.sha(json.dumps(recipe, sort_keys=True, separators=(',', ':')).encode())
        old_bank = audio_asset.parse_bank((ROOT / 'romfs/sound/banks/opening.encaudio').read_bytes())
        bank = audio_asset.build_bank([self.receipt], old_bank['master_db'], old_bank['silence_db'])
        self.manifest['files'] = [
            dict(path=self.path, size=len(self.pcm), sha256=audio_asset.sha(self.pcm)),
            dict(path='data/test.encaudio', size=len(bank), sha256=audio_asset.sha(bank))]
        self.put('romfs/data/test.encaudio', bank)
        self.save_metadata()
        for relative in ('upstream.lock', 'compatibility/upstream-inventory.json'):
            self.put(relative, (ROOT / relative).read_bytes())
        source = audio_asset.source_path(entry['source_path'])
        self.source = source
        for relative in (recipe['bus_source'], recipe['manager_source'], source, source + '.import'):
            self.put('upstream/MOTHER-Encore/' + relative, (ROOT / 'upstream/MOTHER-Encore' / relative).read_bytes())

    def put(self, relative, data):
        target = self.root / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        return target

    def save_metadata(self):
        self.put('content/audio-test.json', json.dumps(self.recipe).encode())
        self.put('content/asset-receipts/audio/test.json', json.dumps(self.manifest).encode())

    def restore(self, **kwargs):
        return restore_audio.restore(self.root, self.output, banks=self.banks, **kwargs)

    def test_existing_correct_pcm_never_invokes_decoder_or_git(self):
        target = self.put('romfs/' + self.path, self.pcm)
        before = target.stat().st_mtime_ns
        with patch.object(subprocess, 'run', side_effect=AssertionError('No subprocess needed')), patch.object(restore_audio.ci_bootstrap, 'verify', side_effect=AssertionError('No Git needed')):
            result = self.restore(ffmpeg='deliberately-unavailable', ffprobe='deliberately-unavailable')
        self.assertEqual(result['restored'], 0)
        self.assertEqual(target.stat().st_mtime_ns, before)

    @unittest.skipUnless(shutil.which('ffmpeg') and shutil.which('ffprobe'), 'FFmpeg conversion test requires FFmpeg')
    def test_missing_pcm_reproduces_reviewed_bytes(self):
        with patch.object(restore_audio.ci_bootstrap, 'verify', return_value={}):
            result = self.restore()
        self.assertEqual(result['restored'], 1)
        self.assertEqual((self.output / self.path).read_bytes(), self.pcm)

    def test_changed_existing_pcm_is_not_overwritten(self):
        target = self.put('romfs/' + self.path, b'user-data')
        with self.assertRaisesRegex(audio_asset.AudioError, 'refusing overwrite'):
            self.restore()
        self.assertEqual(target.read_bytes(), b'user-data')

    def test_changed_source_is_rejected(self):
        self.put('upstream/MOTHER-Encore/' + self.source, b'changed source')
        with self.assertRaisesRegex(audio_asset.AudioError, 'fingerprint mismatch'):
            self.restore()

    def test_changed_recipe_is_rejected(self):
        self.recipe['assets'][0]['gain_db'] = -1
        self.save_metadata()
        with self.assertRaisesRegex(audio_asset.AudioError, 'recipe changed'):
            self.restore()

    def test_wrong_pin_is_rejected(self):
        self.recipe['upstream_commit'] = '0' * 40
        self.save_metadata()
        with self.assertRaisesRegex(audio_asset.AudioError, 'pin mismatch'):
            self.restore()

    def test_fingerprint_disagreement_is_rejected(self):
        self.manifest['files'][0]['sha256'] = '0' * 64
        self.save_metadata()
        with self.assertRaisesRegex(audio_asset.AudioError, 'fingerprint disagreement'):
            self.restore()

    def test_changed_bank_is_rejected(self):
        self.put('romfs/data/test.encaudio', b'bad bank')
        with self.assertRaisesRegex(audio_asset.AudioError, 'bank changed'):
            self.restore()

    def test_output_symlink_escape_is_rejected(self):
        outside = Path(self.temporary.name) / 'outside'
        outside.mkdir()
        (self.output / 'audio').symlink_to(outside, target_is_directory=True)
        with self.assertRaisesRegex(audio_asset.AudioError, 'escaped root'):
            self.restore()
        self.assertEqual(list(outside.iterdir()), [])

    def test_missing_tools_fail_without_creating_pcm(self):
        with patch.object(restore_audio.ci_bootstrap, 'verify', return_value={}):
            with self.assertRaisesRegex(audio_asset.AudioError, 'Missing FFmpeg'):
                self.restore(ffmpeg='deliberately-unavailable', ffprobe='deliberately-unavailable')
        self.assertFalse((self.output / self.path).exists())

    def test_decoder_mismatch_is_never_published(self):
        def run(command, **kwargs):
            if '-show_entries' in command:
                stream = dict(codec_type='audio', channels=self.receipt['channels'], sample_rate=str(self.receipt['source_sample_rate']))
                return subprocess.CompletedProcess(command, 0, json.dumps(dict(streams=[stream])))
            Path(command[-1]).write_bytes(b'incorrect decoder output')
            return subprocess.CompletedProcess(command, 0)
        with patch.object(restore_audio.ci_bootstrap, 'verify', return_value={}), patch.object(shutil, 'which', side_effect=lambda name: name), patch.object(subprocess, 'run', side_effect=run):
            with self.assertRaisesRegex(audio_asset.AudioError, 'no file published'):
                self.restore()
        self.assertFalse((self.output / self.path).exists())


if __name__ == '__main__':
    unittest.main()
