import copy
from collections import Counter
import json
from pathlib import Path
import re
import shutil
import struct
import tempfile
import unittest
from unittest import mock

from PIL import Image

from tools import house_layers as h
from tools.native_content import ContentError, compile_ir, file_crc, parse_pack


class HouseLayersTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.upstream = h.ROOT / 'upstream/MOTHER-Encore'
        cls.data = h.read_json(h.ROOT / h.SOURCE)
        cls.rows = h.records(cls.data)
        cls.ir = h.read_json(h.ROOT / 'content/native-opening.json')

    @staticmethod
    def node(data, path='Above'):
        return next(node for node in data['nodes'] if node['path'] == path)

    @classmethod
    def tileset(cls, data):
        index = cls.node(data)['properties']['tile_set']['id']
        return next(resource['properties'] for resource in data['resources'] if resource['id'] == index)

    def rejects(self, mutation):
        data = copy.deepcopy(self.data)
        mutation(data)
        with self.assertRaises(ValueError):
            h.records(data)

    def test_original_layer_counts_order_and_resource_identity(self):
        h.verify(self.upstream)
        self.assertEqual(Counter(row['layer'] for row in self.rows), {'Objects': 14, 'Above': 276})
        self.assertTrue(all(row['flags'] == 0 and row['resource'] == 'objects' for row in self.rows[:14]))
        self.assertTrue(all(row['flags'] == 1 and row['resource'] == 'above' for row in self.rows[14:]))
        self.assertEqual({(row['width'], row['height']) for row in self.rows[14:]}, {(16, 16)})
        self.assertEqual([(row['name'], row['x'], row['y'], row['sort_y']) for row in self.rows if row['name'] == 'NintenBed'],
                         [('NintenBed', 400, 400, 432)])

    def test_every_foreground_cell_matches_original_serialized_atlas(self):
        # Decode the original serialized triples independently of the native
        # export's resolved cells and the production compiler's derivation.
        source = (self.upstream / h.SCENE).read_text()
        block = re.search(r'^\[node name="Above" type="TileMap" parent="\."\]\n(.*?)(?=^\[|\Z)', source, re.M | re.S)[1]
        raw = [int(value) for value in re.search(r'^tile_data = PoolIntArray\( (.*?) \)$', block, re.M)[1].split(', ')]
        expected = []
        def signed16(value):
            value &= 0xffff
            return value - 65536 if value & 0x8000 else value
        for index in range(0, len(raw), 3):
            location, tile, atlas = raw[index:index + 3]
            self.assertEqual(tile, 14)
            x, y = signed16(location) * 16, signed16(location >> 16) * 16
            u, v = 192 + (atlas & 0xffff) * 16, (atlas >> 16) * 16
            expected.append((x, y, u, v, 16, 16))
        actual = [(row['x'], row['y'], row['u'], row['v'], row['width'], row['height']) for row in self.rows[14:]]
        self.assertEqual(sorted(actual), sorted(expected))
        self.assertEqual(len(set(actual)), 276)

    def test_original_corner_alpha_and_bedroom_floor_mask(self):
        with Image.open(self.upstream / h.ABOVE_TEXTURE) as image:
            pixels = image.convert('RGBA')
        distribution = Counter((row['u'], row['v']) for row in self.rows[14:])
        self.assertEqual(distribution, {(224, 0): 264, (192, 96): 3, (208, 96): 3, (192, 112): 3, (208, 112): 3})
        opaque_counts = {(224, 0): 256, (192, 96): 255, (208, 96): 3, (192, 112): 3, (208, 112): 255}
        for (u, v), expected in opaque_counts.items():
            colors = Counter(pixels.getpixel((u + x, v + y)) for y in range(16) for x in range(16))
            self.assertEqual(colors[(28, 28, 28, 255)], expected)
            self.assertEqual(colors[(0, 0, 0, 0)], 256 - expected)
        covers = [row for row in self.rows[14:] if row['x'] <= 485 < row['x'] + 16 and row['y'] <= 471 < row['y'] + 16]
        self.assertEqual(len(covers), 1)
        row = covers[0]
        self.assertEqual(pixels.getpixel((row['u'] + 485 - row['x'], row['v'] + 471 - row['y'])), (28, 28, 28, 255))

    def test_unreviewed_layer_order_and_transforms_rejected(self):
        def swap(data):
            a, b = data['nodes'].index(self.node(data, 'Objects')), data['nodes'].index(self.node(data))
            data['nodes'][a], data['nodes'][b] = data['nodes'][b], data['nodes'][a]
        for mutation in [swap,
                         lambda d: self.node(d)['properties'].update(rotation=1),
                         lambda d: self.node(d)['properties']['scale'].update(x=2),
                         lambda d: self.node(d)['world_transform']['origin'].update(y=1),
                         lambda d: self.node(d, '.')['properties']['position'].update(x=1)]:
            self.rejects(mutation)

    def test_unreviewed_layer_modes_depth_and_appearance_rejected(self):
        for key, value in [('mode', {'type': 'int64', 'value': '1'}), ('cell_y_sort', True),
                           ('z_index', {'type': 'int64', 'value': '1'}), ('z_as_relative', False),
                           ('cell_tile_origin', {'type': 'int64', 'value': '1'}),
                           ('cell_half_offset', {'type': 'int64', 'value': '0'}),
                           ('centered_textures', True), ('compatibility_mode', True),
                           ('cell_clip_uv', True), ('visible', False), ('show_behind_parent', True)]:
            with self.subTest(key=key):
                self.rejects(lambda d: self.node(d)['properties'].update({key: value}))
        self.rejects(lambda d: self.node(d)['properties']['modulate'].update(a=.5))

    def test_unknown_cell_flips_counts_positions_and_atlas_bounds_rejected(self):
        for key in ('flip_x', 'flip_y', 'transpose'):
            self.rejects(lambda d: self.node(d)['cells'][0].update({key: True}))
        for mutation in [lambda d: self.node(d)['cells'].pop(),
                         lambda d: self.node(d)['cells'].__setitem__(1, copy.deepcopy(self.node(d)['cells'][0])),
                         lambda d: self.node(d)['cells'][0]['local_origin'].update(x=1),
                         lambda d: self.node(d)['cells'][0]['autotile'].update(x=-1),
                         lambda d: self.node(d)['cells'][0]['autotile'].update(x=3),
                         lambda d: self.node(d)['cells'][0]['autotile'].update(y=8),
                         lambda d: self.node(d)['cells'][0]['autotile'].update(x=.5),
                         lambda d: self.node(d, 'Objects')['cells'][0]['autotile'].update(x=1)]:
            self.rejects(mutation)

    def test_unknown_tile_modes_offsets_spacing_and_z_rejected(self):
        for key, value in [('14/tile_mode', {'type': 'int64', 'value': '1'}),
                           ('14/z_index', {'type': 'int64', 'value': '1'}),
                           ('14/autotile/spacing', {'type': 'int64', 'value': '1'}),
                           ('14/tex_offset', {'type': 'Vector2', 'x': 1, 'y': 0}),
                           ('14/autotile/tile_size', {'type': 'Vector2', 'x': 32, 'y': 16}),
                           ('14/autotile/z_index_map', {'type': 'Array', 'value': [1]})]:
            with self.subTest(key=key):
                self.rejects(lambda d: self.tileset(d).update({key: value}))
        self.rejects(lambda d: self.tileset(d)['14/region']['position'].update(x=900))
        self.rejects(lambda d: self.tileset(d)['14/modulate'].update(r=.5))

    def test_new_and_old_output_tampering_rejected(self):
        for name in ('objects.t3x', 'above.t3x'):
            with tempfile.TemporaryDirectory() as td:
                out = Path(td)
                for file in h.OUT.iterdir():
                    shutil.copyfile(file, out / file.name)
                (out / name).write_bytes(b'changed texture')
                with self.assertRaisesRegex(ValueError, 'Stale or tampered'):
                    h.verify(self.upstream, out)

    def test_receipt_schema_source_resource_and_layout_tampering_rejected(self):
        for mutation in [lambda r: r.update(schema=2), lambda r: r.update(commit='0' * 40),
                         lambda r: r['resources']['above'].update(source=h.TEXTURE),
                         lambda r: r['sources'].update({h.TILESET: '0' * 64}),
                         lambda r: r['tiles'][-1].update(flags=0)]:
            with tempfile.TemporaryDirectory() as td:
                out = Path(td)
                for file in h.OUT.iterdir():
                    shutil.copyfile(file, out / file.name)
                receipt = h.read_json(out / 'source.json')
                mutation(receipt)
                h.write_json(out / 'source.json', receipt)
                with self.assertRaisesRegex(ValueError, 'Stale or tampered'):
                    h.verify(self.upstream, out)

    def test_unreviewed_source_and_native_export_rejected(self):
        original = h.sha
        for path in (self.upstream / h.TILESET, self.upstream / h.ABOVE_TEXTURE, h.ROOT / h.SOURCE, h.ROOT / h.SOURCE_RECEIPT):
            with mock.patch.object(h, 'sha', side_effect=lambda p: '0' * 64 if p == path else original(p)):
                with self.assertRaisesRegex(ValueError, 'requires review'):
                    h.validate(self.upstream)

    def test_foreground_ir_roundtrip_and_unknown_flag_rejected(self):
        overlays = self.ir['sections']['Overlay']
        self.assertEqual(len(overlays), 290)
        self.assertEqual([row['stable_id'] for row in overlays], list(range(1, 291)))
        self.assertEqual(Counter(row['flags'] for row in overlays), {0: 14, 1: 276})
        for source, actual in zip(self.rows, overlays):
            self.assertEqual([actual[k] for k in ('x', 'y', 'sort_y', 'u', 'v', 'flags')],
                             [source[k] for k in ('x', 'y', 'sort_y', 'u', 'v', 'flags')])
            resource = self.ir['sections']['Resource'][actual['resource_index']]
            self.assertEqual(self.ir['strings'][resource['path_string']], 'house-layers/' + source['resource'] + '.t3x')
        blob, manifest = compile_ir(self.ir)
        self.assertEqual(parse_pack(blob)['sections']['Overlay'], overlays)
        changed = copy.deepcopy(self.ir)
        changed['sections']['Overlay'][-1]['flags'] = 2
        with self.assertRaises(ContentError):
            compile_ir(changed)
        damaged = bytearray(blob)
        section = manifest['sections']['Overlay']
        struct.pack_into('<H', damaged, section['offset'] + 14 * section['stride'] + 28, 2)
        struct.pack_into('<I', damaged, 52, file_crc(damaged))
        with self.assertRaises(ContentError):
            parse_pack(damaged)

    def test_foreground_resource_appends_after_existing_stable_identities(self):
        # These published identities are shared with independently compiled
        # audio/battle resources; inserting a texture must never renumber them.
        original_paths = ['actor-preview/ninten-main.t3x', 'actor-preview/lamp.t3x',
                          'actor-preview/emotes.t3x', 'actor-preview/shadow.t3x', 'house-layers/objects.t3x']
        original_paths += ['map-preview/house-%02d.t3x' % i for i in range(15)]
        original_paths += ['res://Audio/Music/Poltergeist.ogg', 'res://Audio/Sound effects/bash.mp3',
                           'res://Audio/Sound effects/M3/PK_Thunder_a_b_y_O_hit.wav', 'data/opening.encbattle',
                           'house-preview/doll.t3x', 'house-preview/mimmie.t3x', 'data/doll-entry.encbattle']
        resources = self.ir['sections']['Resource']
        self.assertEqual([(r['stable_id'], self.ir['strings'][r['path_string']]) for r in resources[:27]],
                         list(enumerate(original_paths, 1)))
        self.assertEqual(resources[27]['stable_id'], 28)
        self.assertEqual(self.ir['strings'][resources[27]['path_string']], 'house-layers/above.t3x')
        self.assertTrue(all(row['resource_index'] == 27 for row in self.ir['sections']['Overlay'][14:]))
        audio = h.read_json(h.ROOT / 'content/native-audio.json')
        paths = {self.ir['strings'][r['path_string']]: r for r in resources}
        shared = [asset for asset in audio['assets'] if asset['source_path'] in paths]
        self.assertEqual([asset['stable_id'] for asset in shared], [21, 22, 30, 32, 33, 34, 36, 37])  # Preserved source identities plus phone ring/hangup.
        for asset in shared:
            self.assertEqual(paths[asset['source_path']]['stable_id'], asset['stable_id'])
            self.assertEqual(paths[asset['source_path']]['sha256'], asset['source_sha256'])


if __name__ == '__main__':
    unittest.main()
