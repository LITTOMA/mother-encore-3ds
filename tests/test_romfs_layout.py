import unittest
from tools.romfs_layout import check_layout, checked_inventory
from tools.asset_receipts import receipt_path, receipt_entries
from pathlib import Path
import tempfile


class RomFSLayoutTests(unittest.TestCase):
    def test_runtime_categories(self):
        check_layout(['graphics/actors/ninten.t3x', 'graphics/battle/lamp/background.bpx',
                      'sound/music/house.pcm', 'sound/effects/bash.pcm',
                      'sound/banks/opening.encaudio', 'sound/banks/podunk.encmusic',
                      'fonts/source-fonts.encfont', 'fonts/page.t3x',
                      Path('data/opening.encroom'), 'data/native.encinput', 'data/melody.encfx',
                      'licenses/license-sources.json'])

    def test_build_metadata_and_legacy_paths_rejected(self):
        for name in ('actor-preview/ninten.t3x', 'graphics/actor-preview/ninten.t3x',
                     'graphics/actors/source.json', 'data/source.json', 'fonts/source.json',
                     'scripts/main.gd', 'data/sandbox.encpak', 'unknown/item.t3x',
                     'data/opening.encaudio', 'sound/house.pcm', 'sound/banks/house.pcm',
                     'sound/music/opening.encaudio'):
            with self.subTest(path=name), self.assertRaises(ValueError):
                check_layout([name])

    def test_unsafe_paths_rejected(self):
        for name in ('../graphics/ninten.t3x', '/graphics/ninten.t3x',
                     'graphics//ninten.t3x', 'graphics/./ninten.t3x',
                     'graphics/../ninten.t3x', 'graphics\\ninten.t3x',
                     'C:/graphics/ninten.t3x', 'ninten.t3x'):
            with self.subTest(path=name), self.assertRaises(ValueError):
                check_layout([name])

    def test_receipt_separation_and_isolated_outputs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            output = root / 'romfs/graphics/actors'
            self.assertEqual(receipt_path(output, root),
                             root / 'content/asset-receipts/graphics/actors/source.json')
            self.assertEqual(receipt_entries(output, root), set())
            isolated = root / 'build/corruption-assets'
            self.assertEqual(receipt_path(isolated, root), isolated / 'source.json')
            self.assertEqual(receipt_entries(isolated, root), {'source.json'})

    def test_actual_inventory_rejects_added_metadata(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            asset = root / 'sound/music/house.pcm'
            asset.parent.mkdir(parents=True)
            asset.write_bytes(b'\0\0')
            self.assertEqual(checked_inventory(root), {Path('sound/music/house.pcm')})
            (asset.parent / 'source.json').write_text('{}')
            with self.assertRaises(ValueError):
                checked_inventory(root)

    def test_missing_inventory_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(ValueError):
                checked_inventory(Path(directory) / 'missing')


if __name__ == '__main__':
    unittest.main()
