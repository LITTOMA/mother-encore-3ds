"""Manual regression cases for dependency scope and failed publication.

These tests are registered for explicit full runs; normal generation does not
execute them. Their temporary files are infrastructure fixtures, not gameplay.
"""
import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from tools.catalog_projection import binding_projection, verify_projection
from tools import content_pipeline as pipeline
from tools.resource_catalog import load_ir

ROOT = Path(__file__).resolve().parents[1]


class CatalogProjectionTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.path = 'content/native-resource-catalog.json'
        self.catalog = load_ir(ROOT / self.path)
        self.ids = sorted(r['id'] for r in self.catalog['bindings'] if r['role'] == 'EncounterBattle')
        (self.root / 'content').mkdir()
        self.write(self.catalog)

    def write(self, value):
        (self.root / self.path).write_text(json.dumps(value), encoding='utf-8')

    def test_unrelated_binding_and_catalog_prose_do_not_invalidate_consumer(self):
        _, record = binding_projection(self.root, self.path, self.ids)
        other = copy.deepcopy(self.catalog)
        other['scope'] = 'Another reviewed independent resource was added'
        row = next(r for r in other['bindings'] if r['role'] == 'FieldInventory')
        row['path'] = 'data/another.encinventory'
        other['bindings'].reverse()
        self.write(other)
        verify_projection(self.root, record)

    def test_used_binding_path_change_is_rejected(self):
        _, record = binding_projection(self.root, self.path, self.ids)
        other = copy.deepcopy(self.catalog)
        next(r for r in other['bindings'] if r['id'] == self.ids[0])['path'] = 'data/changed.encbattle'
        self.write(other)
        with self.assertRaisesRegex(ValueError, 'Changed reviewed catalog bindings'):
            verify_projection(self.root, record)

    def test_missing_duplicate_wrong_type_unknown_pin_are_rejected(self):
        for change in (lambda r: r['bindings'].pop(0),
                       lambda r: r['bindings'].append(copy.deepcopy(r['bindings'][0])),
                       lambda r: r['bindings'][0].update(role='Unknown'),
                       lambda r: r.update(commit='0' * 40)):
            other = copy.deepcopy(self.catalog)
            change(other)
            self.write(other)
            with self.assertRaises(ValueError):
                binding_projection(self.root, self.path, self.ids)

    def test_unknown_projection_and_unsafe_path_are_rejected(self):
        _, record = binding_projection(self.root, self.path, self.ids)
        for key, value in (('kind', 'unknown'), ('path', '../escape'),
                           ('identities', [True]), ('sha256', '0' * 64)):
            with self.assertRaises(ValueError):
                verify_projection(self.root, dict(record, **{key: value}))

    def test_room_cannot_omit_or_reduce_its_actual_catalog_dependency(self):
        from tools.native_content import verify_provenance, ContentError
        original=json.loads((ROOT/'content/native-opening.json').read_bytes())
        for reduced in ([], [3, 256]):
            room=copy.deepcopy(original)
            if not reduced:
                room['provenance']['projections']=[]
            else:
                room['provenance']['projections'][0]['identities']=reduced
            with self.assertRaises(ContentError):
                verify_provenance(room, ROOT)


class PublicationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.base = Path(self.temporary.name)
        self.root, self.work = self.base / 'root', self.base / 'work'
        self.names = ['content/input.json', 'romfs/data/resource.bin']
        for parent in (self.root, self.work):
            for name in self.names:
                path = parent / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b'old')
        self.originals = {n: pipeline.digest(self.root / n) for n in self.names}
        for name in self.names:
            (self.work / name).write_bytes(b'new')
        self.outputs = pipeline.candidate_outputs(self.work)
        self.journal = self.base / 'publication/publication.json'

    def publish(self, mode='refresh'):
        return pipeline.publish(self.root, self.work, self.originals,
                                self.outputs, self.journal, mode)

    def assert_original(self):
        for name in self.names:
            self.assertEqual((self.root / name).read_bytes(), b'old')

    def test_complete_closure_publishes_together(self):
        self.assertEqual(len(self.publish()), 2)
        self.assertEqual(json.loads(self.journal.read_text())['state'], 'complete')
        for name in self.names:
            self.assertEqual((self.root / name).read_bytes(), b'new')

    def test_normal_check_rejects_stale_closure_without_writes(self):
        with self.assertRaisesRegex(ValueError, 'Incomplete generated closure'):
            self.publish('check')
        self.assert_original()
        self.assertFalse(self.journal.exists())

    def test_second_write_failure_rolls_back_first(self):
        original = pipeline.atomic_copy
        calls = []
        def injected_failure(source, target):
            calls.append(target)
            if len(calls) == 2:
                raise OSError('injected publication failure')
            original(source, target)
        with patch.object(pipeline, 'atomic_copy', side_effect=injected_failure):
            with self.assertRaisesRegex(OSError, 'injected publication failure'):
                self.publish()
        self.assert_original()
        self.assertEqual(json.loads(self.journal.read_text())['state'], 'rolled-back')

    def test_concurrent_input_change_is_preserved(self):
        (self.root / self.names[0]).write_bytes(b'contributor edit')
        with self.assertRaisesRegex(ValueError, 'Input changed during generation'):
            self.publish()
        self.assertEqual((self.root / self.names[0]).read_bytes(), b'contributor edit')
        self.assertEqual((self.root / self.names[1]).read_bytes(), b'old')

    def test_shader_edit_during_publication_rolls_back_resources(self):
        name = 'platform/ctr/shaders/field_canvas_art.v.pica'
        shader = self.root / name
        shader.parent.mkdir(parents=True)
        shader.write_bytes(b'original shader')
        self.originals[name] = pipeline.digest(shader)
        copy_output = pipeline.atomic_copy
        edited = []
        def edit_during_copy(source, target):
            copy_output(source, target)
            if not edited:
                shader.write_bytes(b'contributor shader edit')
                edited.append(True)
        with patch.object(pipeline, 'atomic_copy', side_effect=edit_during_copy):
            with self.assertRaisesRegex(ValueError, 'Input changed during generation'):
                self.publish()
        self.assert_original()
        self.assertEqual(shader.read_bytes(), b'contributor shader edit')
        self.assertEqual(json.loads(self.journal.read_text())['state'], 'rolled-back')

    def test_new_output_collision_and_path_escape_are_rejected(self):
        self.outputs['content/new.json'] = self.outputs[self.names[0]]
        (self.root / 'content/new.json').write_bytes(b'other work')
        with self.assertRaisesRegex(ValueError, 'collision'):
            self.publish()
        self.assert_original()
        for value in ('../escape', 'romfs/../escape', 'C:/escape', 'romfs\\escape'):
            with self.assertRaises(ValueError):
                pipeline.output_target(self.root, value)


if __name__ == '__main__':
    unittest.main()
