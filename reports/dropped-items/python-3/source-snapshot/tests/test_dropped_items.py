import copy
import hashlib
import json
import math
import struct
import unittest
import zlib
from pathlib import Path
from types import SimpleNamespace

from tools import dropped_items as d
from tools.dropped_item_protocol import validate, semantic_digest


class DroppedItemsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.model, cls.proof = d.build_model()
        cls.blob = d.encode(cls.model)
        cls.report = d.ROOT / 'reports/dropped-items'
        cls.native = json.loads((cls.report / 'original-1/reference.json').read_bytes())
        cls.config = json.loads((cls.report / 'original-1/config.json').read_bytes())

    def test_original_bindings_and_roundtrip(self):
        result = d.decode(self.blob)
        self.assertEqual(result['scene'], self.model['scene'])
        self.assertEqual(len(result['owners']), 7)
        self.assertEqual(len(result['templates']), 3)
        self.assertEqual({v['source'] for v in self.model['templates']}, {'Hamburger', 'Fries', 'Antidote'})
        self.assertEqual(self.model['policy']['SparkleMaximum'], 47)
        self.assertEqual(self.model['policy']['SparkleFrames'], 40)
        self.assertEqual(self.model['policy']['PromptOffsetY'], -8)
        self.assertEqual(self.model['policy']['CollectTransition'], 3)
        self.assertEqual(self.model['geometry']['interact/CollisionShape2D']['position'],
                         [-4.768369876728684e-7, 4.768369876728684e-7])
        self.assertEqual(len(self.proof['methods']['script']), 11)

    def test_rejects_every_truncation_and_unknown_header(self):
        for end in range(len(self.blob)):
            with self.subTest(end=end), self.assertRaises(ValueError):
                d.decode(self.blob[:end])
        for offset, value in [(8, 0), (8, 2), (20, 2), (28, 1), (52, 7), (56, 1), (60, 1)]:
            blob = bytearray(self.blob)
            struct.pack_into('<I', blob, offset, value)
            struct.pack_into('<I', blob, 16, zlib.crc32(blob[32:]))
            with self.subTest(offset=offset, value=value), self.assertRaises(ValueError):
                d.decode(blob)
        for blob in [b'', b'unknown', self.blob + b'\0', b'\0' * (d.LIMIT + 1)]:
            with self.assertRaises(ValueError):
                d.decode(blob)

    def test_rejects_directory_aliases_and_typed_records(self):
        offsets = [struct.unpack_from('<I', self.blob, 68 + 16 * i)[0] for i in range(8)]
        meta, policy, template, owner, shape, clip, key, pool = offsets
        changes = [(32, 0), (meta + 4, 2), (meta + 36, 0), (meta + 56, 2),
                   (meta + 72, 4), (policy, 22), (policy + 8, 0x7ff80000),
                   (template + 12, 2), (template + 28, 1), (owner + 20, 0),
                   (owner + 28, 1), (owner + 32, 0x7fc00000), (shape + 12, 0),
                   (clip + 4, 3), (clip + 16, 0), (clip + 20, 2),
                   (key, 0x7fc00000), (key + 4, 2), (key + 8, 2)]
        changes += [(64 + 16 * i + j, value) for i in range(8)
                    for j, value in [(0, 0), (4, 0), (8, 0), (12, 0xffffffff)]]
        for offset, value in changes:
            blob = bytearray(self.blob)
            struct.pack_into('<I', blob, offset, value)
            struct.pack_into('<I', blob, 16, zlib.crc32(blob[32:]))
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                d.decode(blob)
        for offset, value in [(pool, 1), (pool + 1, ord(':')), (len(self.blob) - 1, 1)]:
            blob = bytearray(self.blob)
            blob[offset] = value
            struct.pack_into('<I', blob, 16, zlib.crc32(blob[32:]))
            with self.assertRaises(ValueError):
                d.decode(blob)
        duplicate = bytearray(self.blob)
        struct.pack_into('<I', duplicate, shape + 24 + 20, struct.unpack_from('<I', duplicate, shape + 20)[0])
        struct.pack_into('<I', duplicate, 16, zlib.crc32(duplicate[32:]))
        with self.assertRaisesRegex(ValueError, 'duplicate/invalid child shape'):
            d.decode(duplicate)

    def test_model_encoder_rejects_unknown_and_mistyped_content(self):
        operations = [lambda m: m.update(unknown=1),
                      lambda m: m.update(scene=True),
                      lambda m: m['policy'].update(CollectTravelSeconds=True),
                      lambda m: m['policy'].update(CollectTravelSeconds=math.nan),
                      lambda m: m['templates'][0].update(key_item=1),
                      lambda m: m['templates'][0].update(doses=True),
                      lambda m: m['templates'][0].update(size=[20]),
                      lambda m: m['bindings'][0].update(flag_kind='unsupported'),
                      lambda m: m['bindings'][0].update(position=[0, math.inf]),
                      lambda m: m['geometry']['interact/CollisionShape2D'].update(extents=[0, 1]),
                      lambda m: m['clips'][0].update(role=3),
                      lambda m: m['clips'][0]['keys'][0].update(value=2)]
        for operation in operations:
            model = copy.deepcopy(self.model)
            operation(model)
            with self.assertRaises(ValueError):
                d.encode(model)

    def test_source_parent_coordinates_fail_closed(self):
        nodes = [SimpleNamespace(path='.', props={}), SimpleNamespace(path='Objects', props={})]
        d.identity_parent(nodes, 'Objects')
        for key, value in [('position', d.Vec2(1, 0)), ('scale', d.Vec2(1, 1)),
                           ('rotation', 0), ('rotation_degrees', 0), ('transform', 0), ('skew', 0)]:
            with self.subTest(key=key), self.assertRaises(ValueError):
                d.identity_parent([nodes[0], SimpleNamespace(path='Objects', props={key: value})], 'Objects')
        for invalid in [nodes[1:], nodes + [nodes[1]]]:
            with self.assertRaises(ValueError):
                d.identity_parent(invalid, 'Objects')

    def test_two_actual_original_runs_match_opaque_identity_semantics(self):
        digests = []
        raw = []
        for index in [1, 2]:
            base = self.report / ('original-' + str(index))
            reference = json.loads((base / 'reference.json').read_bytes())
            receipt = json.loads((base / 'receipt.json').read_bytes())
            config = json.loads((base / 'config.json').read_bytes())
            validate(reference, config)
            for name, field in [('producer.py', 'producer_sha256'), ('protocol.py', 'protocol_sha256')]:
                self.assertEqual(hashlib.sha256((base / name).read_bytes()).hexdigest(), receipt[field])
            inputs = json.loads((base / 'inputs.json').read_bytes())
            self.assertEqual(hashlib.sha256((base / 'source-adapter.py').read_bytes()).hexdigest(), inputs['producer_sha256'])
            self.assertEqual(hashlib.sha256((base / 'reference.json').read_bytes()).hexdigest(), receipt['result_sha256'])
            self.assertEqual(semantic_digest(reference), receipt['semantic_sha256'])
            digests.append(semantic_digest(reference))
            raw.append(receipt['result_sha256'])
        self.assertEqual(digests[0], digests[1])
        self.assertNotEqual(raw[0], raw[1])

    def test_reference_rejects_versions_fields_entropy_and_lifecycle(self):
        operations = [lambda v: v.update(schema=1), lambda v: v.update(schema=True),
                      lambda v: v.update(unknown=0), lambda v: v['godot'].update(unknown=0),
                      lambda v: v['godot'].update(hash='0' * 40),
                      lambda v: v['cases'][0].update(case='foreign'),
                      lambda v: v['cases'][0]['rows'][0].update(frame=0),
                      lambda v: v['cases'][0]['rows'][0].update(delta=0x7fc00000),
                      lambda v: v['cases'][0]['rows'][0].update(alive=1),
                      lambda v: v['cases'][0]['rows'][0]['flags'].update(foreign=True),
                      lambda v: v['cases'][0]['rows'][0]['state'].update(queued=0),
                      lambda v: v['cases'][0]['rows'][0]['state'].update(sparkle=40),
                      lambda v: v['cases'][0]['rows'][0]['state'].update(animation='unknown'),
                      lambda v: v['cases'][0]['rows'][0]['prompt'].update(parent='../foreign'),
                      lambda v: v['cases'][0]['rows'][1]['party'][0][0].update(uid='allocated:99'),
                      lambda v: v['entropy_allocations'][0].update(uids=[1, 1]),
                      lambda v: v['entropy_allocations'][0].update(uids=[-1]),
                      lambda v: v['cases'].pop()]
        for index, operation in enumerate(operations):
            doc = copy.deepcopy(self.native)
            operation(doc)
            with self.subTest(index=index), self.assertRaises(ValueError):
                validate(doc, self.config)


if __name__ == '__main__':
    unittest.main()
