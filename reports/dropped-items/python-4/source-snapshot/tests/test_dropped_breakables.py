import copy
import hashlib
import json
import struct
import tempfile
import unittest
from pathlib import Path
from tools import dropped_items as d
from tools.dropped_breakable_protocol import validate, semantic_digest
from tests.dropped_reference_fixture import Writer, write_outputs
from tests.prepare_dropped_breakables import prepare


class DroppedBreakableTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.base = d.ROOT / 'reports/dropped-items/composed-original-1'
        cls.native = json.loads((cls.base / 'reference.json').read_bytes())
        cls.config = json.loads((cls.base / 'config.json').read_bytes())

    def test_actual_original_outcomes_and_idempotence(self):
        other = d.ROOT / 'reports/dropped-items/composed-original-2'
        second = json.loads((other / 'reference.json').read_bytes())
        validate(self.native, self.config)
        validate(second, json.loads((other / 'config.json').read_bytes()))
        self.assertEqual(semantic_digest(self.native), semantic_digest(second))
        self.assertNotEqual(self.native['entropy_allocations'], second['entropy_allocations'])
        self.assertEqual(len(self.native['cases']), 11)
        self.assertEqual(sum(len(v['rows']) for v in self.native['cases']), 1067)
        self.assertEqual({v['content'] for v in self.config['covers']},
                         {v['path'] for v in self.config['bindings']})
        for owner in self.config['covers']:
            self.assertEqual(owner['drop_parent'], 'Objects')
            self.assertEqual(owner['new_parent'], 'Bottom2')
            self.assertFalse(owner['interact_content'])
        cases = {v['case']: v for v in self.native['cases']}
        self.assertEqual(cases['full']['rows'][-1]['dialogues'], ['ItemDialogue/itemfull'] * 2)
        self.assertEqual(cases['full']['rows'][-1]['ledger_size'], 2)
        self.assertTrue(cases['full']['rows'][-1]['alive'])
        self.assertEqual([len(v) for v in cases['second-holder']['rows'][-1]['party']], [16, 1])
        self.assertEqual(cases['already-collected']['rows'][-1]['ledger_size'], 0)
        for case in self.native['cases'][:7]:
            before, after = case['rows'][1], case['rows'][-1]
            self.assertEqual(before['dialogues'], [])
            self.assertEqual(after['dialogues'], ['ItemDialogue/itemcheck'])
            self.assertFalse(after['alive'])
            self.assertTrue(any(v.get('state', {}).get('area_disabled') for v in case['rows']))
            self.assertTrue(all(not v['state']['body_disabled'] for v in case['rows'] if v['alive']))

    def test_unknown_versions_fields_and_cover_states_rejected(self):
        changes = [lambda x: x.update(schema=2), lambda x: x.update(schema=True),
                   lambda x: x.update(unknown=0), lambda x: x['cases'].pop(),
                   lambda x: x['cases'][0]['rows'].pop(),
                   lambda x: x['cases'][0]['rows'][0].pop('cover'),
                   lambda x: x['cases'][0]['rows'][0]['cover'].update(extra=0),
                   lambda x: x['cases'][0]['rows'][0]['cover'].update(parent='foreign'),
                   lambda x: x['cases'][0]['rows'][0]['cover'].update(animation='foreign'),
                   lambda x: x['cases'][0]['rows'][0]['cover'].update(timer_left=0x7fc00000),
                   lambda x: x['cases'][0]['rows'][0]['cover'].update(body_layer=1 << 20),
                   lambda x: x['cases'][0]['rows'][0]['cover'].update(audio=[[0, 'yes']]),
                   lambda x: x['cases'][0]['rows'][0]['cover'].update(rumble=[[0, 0, 0.5, 0.8, 0.3]]),
                   lambda x: x['cases'][0]['rows'][0]['state'].update(parent='Bottom2'),
                   lambda x: x['cases'][0]['rows'][0]['state'].update(body_disabled=0),
                   lambda x: x['cases'][0]['rows'][0]['state'].pop('area_disabled')]
        for index, change in enumerate(changes):
            value = copy.deepcopy(self.native)
            change(value)
            with self.subTest(index=index), self.assertRaises(ValueError):
                validate(value, self.config)

    def test_frozen_producers_and_source_fixtures_are_checked(self):
        with tempfile.TemporaryDirectory(prefix='dropped-composed-', dir=d.ROOT / 'build') as name:
            target = prepare(Path(name) / 'fixture')
            source = json.loads((target / 'source.json').read_bytes())
            self.assertEqual(source['sha256'], hashlib.sha256((target / 'native.bin').read_bytes()).hexdigest())
            clock = json.loads((d.ROOT / 'reports/dropped-items/composed-clock-original-1/reference.json').read_bytes())
            self.assertEqual(source['semantic_sha256'], semantic_digest(clock))
            self.assertEqual((target / 'native.bin').read_bytes()[:8], b'ENCDRPB2')
            model, _ = d.build_model()
            self.assertEqual((target / 'source.encdrp').read_bytes(), d.encode(model))
            self.assertEqual(struct.unpack_from('<I', (target / 'native.bin').read_bytes(), 8)[0], 11)
            saved = {p.name: (p.read_bytes(), p.stat().st_mtime_ns) for p in target.iterdir()}
            prepare(target)
            self.assertEqual(saved, {p.name: (p.read_bytes(), p.stat().st_mtime_ns) for p in target.iterdir()})
            damaged = (target / 'native.bin').read_bytes() + b'\0'
            (target / 'native.bin').write_bytes(damaged)
            with self.assertRaises(ValueError):
                prepare(target)
            self.assertEqual((target / 'native.bin').read_bytes(), damaged)
        with self.assertRaises(ValueError):
            prepare(d.ROOT / 'content/forbidden-fixture')

    def test_fixture_writer_rejects_mistyped_fields_without_partial_output(self):
        for magic in [b'', b'short', 'ENCDRPB1', False]:
            with self.assertRaises(ValueError):
                Writer(magic)
        w = Writer(b'ENCDRPB1')
        for method, values in [(w.word, [-1, 1 << 32, True, 1.0]),
                               (w.signed, [-(1 << 31) - 1, 1 << 31, False, 0.0]),
                               (w.text, ['nul\0text', 'x' * 4097, 1, None])]:
            for value in values:
                with self.subTest(value=value), self.assertRaises(ValueError):
                    method(value)
                self.assertEqual(w.data, bytearray(b'ENCDRPB1'))

    def test_fixture_cache_rejects_unowned_paths_and_partial_changes(self):
        with tempfile.TemporaryDirectory(prefix='dropped-cache-', dir=d.ROOT / 'build') as name:
            parent = Path(name)
            self.assertTrue(parent.resolve().is_relative_to(d.ROOT / 'build'))
            target = parent / 'cache'
            marker, value = '.owned-test', b'dropped-test-v1'
            for outputs in [{}, {'../escape': b'a'}, {'..\\escape': b'a'}, {'a:b': b'a'}, {'a\0b': b'a'}, {'x': 'text'}]:
                with self.assertRaises(ValueError):
                    write_outputs(d.ROOT, target, outputs, marker, value)
                self.assertFalse(target.exists())
            for key, data in [('../escape', value), (marker, b''), (marker, 'text')]:
                with self.assertRaises(ValueError):
                    write_outputs(d.ROOT, target, {'x': b'a'}, key, data)
            write_outputs(d.ROOT, target, {'x': b'a', 'y': b'b'}, marker, value)
            with self.assertRaises(ValueError):
                write_outputs(d.ROOT, target, {'x': b'changed', 'y': b'b', 'z': b'new'}, marker, value)
            self.assertEqual((target / 'x').read_bytes(), b'a')
            self.assertFalse((target / 'z').exists())
            (target / 'unowned').write_bytes(b'preserve')
            with self.assertRaises(ValueError):
                write_outputs(d.ROOT, target, {'x': b'a', 'y': b'b'}, marker, value)
            self.assertEqual((target / 'unowned').read_bytes(), b'preserve')

    def test_measured_initial_clocks_preserve_every_existing_state(self):
        from tools.dropped_breakable_protocol import legacy_projection
        docs = []
        for index in [1, 2]:
            base = d.ROOT / ('reports/dropped-items/composed-clock-original-' + str(index))
            doc = json.loads((base / 'reference.json').read_bytes())
            validate(doc, self.config)
            self.assertEqual([len(c['initial_idle_deltas']) for c in doc['cases']], [0] + [1] * 10)
            self.assertEqual(semantic_digest(legacy_projection(doc)), semantic_digest(self.native))
            docs.append(doc)
        self.assertEqual(semantic_digest(docs[0]), semantic_digest(docs[1]))
        for change in [lambda c: c.pop('initial_idle_deltas'),
                       lambda c: c.update(initial_idle_deltas=[True]),
                       lambda c: c.update(initial_idle_deltas=[0]),
                       lambda c: c.update(initial_idle_deltas=[0x7fc00000]),
                       lambda c: c.update(initial_idle_deltas=[0x3f800000] * 33)]:
            value = copy.deepcopy(docs[0])
            change(value['cases'][0])
            with self.assertRaises(ValueError):
                validate(value, self.config)


if __name__ == '__main__':
    unittest.main()
