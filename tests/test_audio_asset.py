import copy
import json
from pathlib import Path
import struct
import tempfile
import unittest
import zlib
from tools import audio_asset as audio

ROOT=Path(__file__).resolve().parents[1]

class AudioAssetTests(unittest.TestCase):
    def setUp(self):
        self.record=dict(stable_id=101,source_sha256='01'*32,sample_rate=44100,channels=2,loop=True,frames=4,loop_start=1,pcm_bytes=16,pcm_crc32=42,pcm_path='sound/effects/test.pcm',source_path='res://Audio/test.ogg',gain_db=0)
        self.bank=audio.build_bank([self.record],-5.93075,-80)
    def test_bank_roundtrip(self):
        bank=audio.parse_bank(self.bank);self.assertEqual(bank['assets'],[self.record]);self.assertAlmostEqual(bank['master_db'],-5.93075,5)
    def test_all_truncations(self):
        for n in range(len(self.bank)):
            with self.assertRaises(audio.AudioError):audio.parse_bank(self.bank[:n])
    def test_all_bit_changes(self):
        for n in range(len(self.bank)):
            data=bytearray(self.bank);data[n]^=128
            with self.assertRaises(audio.AudioError):audio.parse_bank(data)
    def test_semantic_corruption(self):
        for offset,value in [(8,2),(20,65),(24,95),(28,0),(44,1),(64,0),(100,0),(104,3),(108,0),(112,4),(116,1),(124,0),(128,2**32-1),(144,1),(36,0x7fc00000),(40,0),(140,0x7f800000)]:
            data=bytearray(self.bank);struct.pack_into('<I',data,offset,value);struct.pack_into('<I',data,16,0);struct.pack_into('<I',data,16,zlib.crc32(data)&0xffffffff)
            with self.assertRaises(audio.AudioError,msg=f'offset {offset}'):audio.parse_bank(data)
    def test_path_rejection(self):
        for path in ('/tmp/evil','../evil','a/../b','a//b','a/./b','a\\b','a:b','a\0b','a/',''):
            with self.assertRaises(audio.AudioError):audio.safe_path(path)
    def test_duplicate_identity_or_paths(self):
        for field,value in [('stable_id',102),('pcm_path','sound/effects/other.pcm'),('source_path','res://Audio/other.ogg')]:
            second=copy.deepcopy(self.record);second[field]=value
            with self.assertRaises(audio.AudioError):audio.build_bank([self.record,second],0,-80)
    def test_importer_known(self):
        path=ROOT/'upstream/MOTHER-Encore/Audio/Music/Poltergeist.ogg.import'
        self.assertEqual(audio.import_settings(path.read_bytes(),'res://Audio/Music/Poltergeist.ogg'),(True,6.382))
    def test_importer_unknown_fails(self):
        original=(ROOT/'upstream/MOTHER-Encore/Audio/Music/Poltergeist.ogg.import').read_bytes()
        for changed in (original.replace(b'loop=true',b'loop=maybe'),original+b'\nextra=true\n',original.replace(b'6.382',b'nan'),original.replace(b'ogg_vorbis',b'wav'),original.replace(b'source_file="res://',b'source_file="else://')):
            with self.assertRaises(audio.AudioError):audio.import_settings(changed,'res://Audio/Music/Poltergeist.ogg')
    def test_reviewed_menu_wav_import_and_resample(self):
        source='Audio/Sound effects/M3/bump.wav'
        original=(ROOT/'upstream/MOTHER-Encore'/ (source+'.import')).read_bytes()
        self.assertEqual(audio.import_settings(original,'res://'+source),(False,0.0))
        for field in (b'edit/trim=false',b'edit/normalize=false',b'force/mono=false'):
            with self.assertRaises(audio.AudioError):audio.import_settings(original.replace(field,field.replace(b'false',b'true')),'res://'+source)
        manifest=json.loads((ROOT/'content/asset-receipts/audio/opening.json').read_text())
        bump=next(a for a in manifest['assets'] if a['stable_id']==1104)
        self.assertEqual((bump['source_sample_rate'],bump['sample_rate']),(96000,48000))
        self.assertIn('-ar',bump['command'])

    def test_source_digest_rejected(self):
        with self.assertRaises(audio.AudioError):audio.verified(ROOT/'upstream/MOTHER-Encore','Audio/Music/Poltergeist.ogg','01'*32)
    def test_actual_manifest_and_source_loop(self):
        files=audio.stage_files(ROOT/'romfs');bank=audio.parse_bank(files[Path('sound/banks/opening.encaudio')]);self.assertEqual([a['stable_id'] for a in bank['assets']],[21,22,1001,1002,1101,1102,1103,1104,30,32,33,34,36,37,1201,1202])
        self.assertEqual(bank['assets'][0]['loop_start'],int(6.382*44100));self.assertTrue(bank['assets'][0]['loop']);self.assertFalse(bank['assets'][1]['loop']);self.assertFalse(bank['assets'][2]['loop'])
        self.assertEqual(bank['assets'][3]['stable_id'],1002);self.assertEqual(bank['assets'][3]['source_path'],'res://Audio/Music/Battle Encounter/Encounter Boss.mp3');self.assertFalse(bank['assets'][3]['loop'])
        for a in bank['assets']:
            pcm=files[Path(a['pcm_path'])];self.assertEqual(len(pcm),a['pcm_bytes']);self.assertEqual(zlib.crc32(pcm)&0xffffffff,a['pcm_crc32'])
    def test_stage_tamper(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);(root/'sound/banks').mkdir(parents=True);(root/'sound/effects').mkdir();(root/'data').mkdir()
            payloads={'sound/banks/opening.encaudio':self.bank,'sound/effects/test.pcm':bytes(16)}
            manifest=dict(schema=1,files=[dict(path=p,size=len(d),sha256=audio.sha(d)) for p,d in payloads.items()])
            for p,d in payloads.items():(root/p).write_bytes(d)
            (root/'data/opening-audio-manifest.json').write_text(json.dumps(manifest))
            with self.assertRaises(audio.AudioError):audio.stage_files(root) # PCM CRC doesn't match metadata
            (root/'sound/effects/test.pcm').write_bytes(b'altered')
            with self.assertRaises(audio.AudioError):audio.stage_files(root)
    def test_recipe_unknown_rejected_before_processes(self):
        with self.assertRaises(audio.AudioError):audio.compile_assets({'schema':99},Path('.'),Path('.'))

if __name__=='__main__':unittest.main()
