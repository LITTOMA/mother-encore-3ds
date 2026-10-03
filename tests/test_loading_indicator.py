"""Source/crop parity and actual C++ checked reader/renderer with public API doubles."""
import copy
import os
import subprocess
import tempfile
import unittest
from pathlib import Path
from PIL import Image
from tools import loading_indicator_assets as assets

ROOT = Path(__file__).resolve().parents[1]


class LoadingIndicatorTests(unittest.TestCase):
    def test_source_and_staged_pack(self):
        recipe = assets.read_json(assets.RECIPE)
        assets.verify_recipe(recipe)
        files = assets.stage_files(ROOT / 'romfs')
        self.assertEqual(files[Path('loading-preview/indicator.encload')], assets.encode(recipe))
        self.assertEqual(recipe['texture']['source_frames'], [20, 21, 22])
        self.assertLess(len(files[Path(assets.TEXTURE_PATH)]), 20000)
        self.assertEqual([k['source_frame'] for k in recipe['animation']['keys']], [20, 21, 22, 21])

    def test_invalid_external_content(self):
        recipe = assets.read_json(assets.RECIPE)
        for modify in [lambda r:r['animation'].update(loop=False),
                       lambda r:r['animation']['keys'][1].update(time=0),
                       lambda r:r['animation']['keys'][0].update(frame=99),
                       lambda r:r['texture'].update(width=310),
                       lambda r:r['layout'].update(scale=2),
                       lambda r:r['layout'].update(viewports=[[30, 20]]),
                       lambda r:r.update(background_color=0)]:
            bad=copy.deepcopy(recipe);modify(bad)
            with self.assertRaises(ValueError):assets.encode(bad)
        bad=copy.deepcopy(recipe);bad['layout']['right_margin']+=1
        with self.assertRaises(ValueError):assets.verify_recipe(bad)
        bad=copy.deepcopy(recipe);bad['outputs'][assets.TEXTURE_PATH]['bytes']+=1
        with self.assertRaises(ValueError):assets.verify_recipe(bad)

    def test_reader_and_renderer(self):
        with tempfile.TemporaryDirectory() as directory:
            output=Path(directory)/'loading-indicator-tests'
            subprocess.run([os.environ.get('CXX','g++'),'-std=c++17','-Wall','-Wextra','-Werror','-fno-exceptions','-fno-rtti',
                            '-I',str(ROOT/'tests/loading_indicator_stubs'),'-I',str(ROOT),'-I',str(ROOT/'include'),
                            str(ROOT/'tests/loading_indicator_tests.cpp'),str(ROOT/'runtime/loading_indicator_data.cpp'),
                            str(ROOT/'runtime/animation.cpp'),'-o',str(output)],check=True)
            subprocess.run([str(output),str(ROOT/'romfs')+'/'],check=True)


if __name__=='__main__':unittest.main()
