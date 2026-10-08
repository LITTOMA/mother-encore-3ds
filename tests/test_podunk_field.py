import copy
import json
import struct
import tempfile
import unittest
import zlib
from pathlib import Path

from tools import podunk_field as pf


class DecompositionTests(unittest.TestCase):
    def test_godot_hertel_mehlhorn_parts(self):
        # Pinned DoorRoundWhite shape: concave points declared as ConvexPolygonShape2D.
        door = [(4, 28), (48, 28), (48, 48), (40, 48), (32, 52), (36, 40), (12, 40), (4, 52)]
        self.assertEqual(pf.decompose_polygon_in_convex(door), [
            [(12.0, 40.0), (4.0, 52.0), (4.0, 28.0)], [(40.0, 48.0), (32.0, 52.0), (36.0, 40.0)],
            [(12.0, 40.0), (4.0, 28.0), (48.0, 28.0), (36.0, 40.0)], [(36.0, 40.0), (48.0, 28.0), (48.0, 48.0), (40.0, 48.0)]])

    def test_convex_degenerate_and_orientation(self):
        square = [(0, 0), (16, 0), (16, 16), (0, 16)]
        self.assertEqual(pf.decompose_polygon_in_convex(square), [[(0.0, 0.0), (16.0, 0.0), (16.0, 16.0), (0.0, 16.0)]])
        # Clockwise input is inverted to Godot's TRIANGULATOR_CCW (positive area) order.
        self.assertEqual(pf.decompose_polygon_in_convex(list(reversed(square))), [[(0.0, 0.0), (16.0, 0.0), (16.0, 16.0), (0.0, 16.0)]])
        line = [(16, 16), (0, 16), (0, 16), (16, 16)]
        self.assertEqual(len(pf.decompose_polygon_in_convex(line)), 1)

    def test_source_hash_and_rng(self):
        self.assertEqual(pf.godot_string_hash(''), 5381)
        self.assertEqual(pf.godot_string_hash('a'), 5381 * 33 + 97)
        a, b = pf.SourceRandom(), pf.SourceRandom()
        a.seed(123)
        b.seed(123)
        self.assertEqual([a.randi() for _ in range(4)], [b.randi() for _ in range(4)])


class RecipeTests(unittest.TestCase):
    def write(self, data):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        path = Path(directory.name) / 'recipe.json'
        path.write_text(json.dumps(data), encoding='utf-8')
        return path

    def test_recipe_rejections(self):
        base = pf.load_recipe()
        mutations = [
            lambda r: r.update(extra=1),
            lambda r: r.update(commit='0' * 40),
            lambda r: r['categories'].update({'Nodes/Overworld/Door.tscn': 'teleport'}),
            lambda r: r['prop_sprites'].pop('Nodes/Overworld/Objects/Present.tscn'),
            lambda r: r.pop('routes'),
        ]
        for mutate in mutations:
            data = copy.deepcopy(base)
            mutate(data)
            with self.subTest(mutate=mutate), self.assertRaises(pf.FieldError):
                pf.load_recipe(self.write(data))

    def test_unclassified_scene_class_fails_closed(self):
        recipe = copy.deepcopy(pf.load_recipe())
        del recipe['categories']['Nodes/Overworld/butterfly.tscn']
        with self.assertRaises(pf.FieldError):
            pf.Scene(pf.Sources(), recipe)


class ContainerTests(unittest.TestCase):
    def test_round_trip_and_rejections(self):
        blob = pf.pack_container(pf.LINK_MAGIC, 0, [b'abc', b'', bytes(84)], pf.LINK_STRIDES, pf.LINK_LIMIT)
        scene, sections = pf.unpack_container(blob, pf.LINK_MAGIC, pf.LINK_STRIDES, pf.LINK_LIMIT)
        self.assertEqual((scene, sections), (0, [b'abc', b'', bytes(84)]))

        def resign(data):
            data = bytearray(data)
            struct.pack_into('<I', data, 16, zlib.crc32(bytes(data[32:])))
            return bytes(data)
        corrupt = [
            blob[:-1],
            blob + b'\0',
            bytes(blob[:16]) + b'\0\0\0\0' + bytes(blob[20:]),
            resign(blob[:40] + bytes([blob[40] ^ 1]) + blob[41:]),
            resign(blob[:76] + struct.pack('<I', 2) + blob[80:]),
            resign(blob[:20] + struct.pack('<I', 2) + blob[24:]),
        ]
        for data in corrupt:
            with self.subTest(data=data[:24]), self.assertRaises(pf.FieldError):
                pf.unpack_container(data, pf.LINK_MAGIC, pf.LINK_STRIDES, pf.LINK_LIMIT)
        with self.assertRaises(pf.FieldError):
            pf.unpack_container(blob, pf.MAP_MAGIC, pf.LINK_STRIDES, pf.LINK_LIMIT)


class CommittedOutputTests(unittest.TestCase):
    def test_committed_outputs_match_pinned_sources(self):
        model, outputs = pf.verify_all(pf.load_recipe())
        self.assertEqual(model.undefined_cells, json.loads(pf.RECEIPT.read_text())['undefined_tile_cells'])
        rows = {r['door']: r for r in model.route_rows}
        self.assertEqual((rows['Doors/Podunk']['x'], rows['Doors/Podunk']['y']), (8.0, -39.0))
        self.assertEqual((rows['Objects/Doors/NintensHouse']['x'], rows['Objects/Doors/NintensHouse']['y']), (136.0, 809.0))
        self.assertEqual(rows['Doors/Podunk']['flag'], 'good_morning')
        self.assertFalse(rows['Doors/Podunk']['flag_value'])

    def test_stage_files_are_receipt_checked(self):
        files = pf.stage_files()
        names = {p.as_posix() for p in files}
        self.assertTrue({'data/podunk.encmap', 'data/podunk.encroom', 'data/world.enclinks'} <= names)
        self.assertTrue(any(n.startswith('graphics/world/podunk/') for n in names))


if __name__ == '__main__':
    unittest.main()
