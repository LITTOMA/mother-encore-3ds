#!/usr/bin/env python3
"""Fail-closed resource wiring and compiler/prod-consumer integration checks."""
import copy
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools import resource_catalog as catalog


class ResourceCatalogTests(unittest.TestCase):
    def setUp(self):
        self.base = catalog.load_ir()
        self.fields = catalog.load_ir(catalog.EXTENSION_IR)
        self.ir = catalog.load_catalog()
        self.fixture_parent = ROOT / 'build/resource-catalog-python'
        self.fixture_parent.mkdir(parents=True, exist_ok=True)

    def reject(self, edit):
        candidate = copy.deepcopy(self.ir)
        edit(candidate)
        with self.assertRaises(ValueError):
            catalog.validate(candidate)

    def test_reproducible_checked_pack_and_compiler_cli(self):
        expected = catalog.OUT.read_bytes()
        self.assertEqual(catalog.encode(self.ir), expected)
        reversed_bindings = copy.deepcopy(self.ir)
        reversed_bindings['bindings'].reverse()
        self.assertEqual(catalog.encode(reversed_bindings), expected)
        decoded = catalog.decode(expected)
        self.assertEqual(sorted(decoded['bindings'], key=lambda x: x['id']),
                         sorted(self.ir['bindings'], key=lambda x: x['id']))
        self.assertEqual(decoded['encounters'], self.ir['encounters'])
        with tempfile.TemporaryDirectory(dir=self.fixture_parent) as temporary:
            output = Path(temporary) / 'compiled.encresources'
            for operation in ('compile', 'verify'):
                result = subprocess.run([sys.executable, str(ROOT / 'tools/resource_catalog.py'),
                                         operation, '--out', str(output)], cwd=ROOT,
                                        text=True, capture_output=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(output.read_bytes(), expected)

    def test_field_extension_is_separate_and_fail_closed(self):
        self.assertFalse(any(row['role'] in catalog.EXTENSION_ROLES for row in self.base['bindings']))
        self.assertEqual({row['role'] for row in self.fields['bindings']}, catalog.EXTENSION_ROLES)
        self.assertEqual(catalog.capability_for(self.base['bindings']), 1)
        self.assertEqual(catalog.capability_for(self.ir['bindings']), 4)
        without_mick = [row for row in self.ir['bindings'] if row['role'] != 'MickTreats']
        self.assertEqual(catalog.capability_for(without_mick), 3)
        without_presents = [row for row in without_mick if row['role'] != 'HousePresents']
        self.assertEqual(catalog.capability_for(without_presents), 2)

        def reject(edit_base=None, edit_fields=None):
            base, fields = copy.deepcopy(self.base), copy.deepcopy(self.fields)
            if edit_base:
                edit_base(base)
            if edit_fields:
                edit_fields(fields)
            with self.assertRaises(ValueError):
                catalog.merge_catalog(base, fields)

        for key, value in [('schema', 2), ('schema', True), ('kind', 'encore.native-resource-catalog.source-ir'),
                           ('kind', 'encore.field-resource-catalog.source-ir'),
                           ('commit', '0' * 40), ('scope', ''), ('sources', {}), ('bindings', []),
                           ('bindings', {}), ('encounters', []), ('unknown', 1)]:
            with self.subTest(key=key, value=value):
                reject(edit_fields=lambda ir: ir.update({key: value}))
        reject(edit_fields=lambda ir: ir.pop('scope'))
        reject(edit_fields=lambda ir: ir['bindings'][0].update(unknown=1))
        reject(edit_fields=lambda ir: ir['bindings'].pop(0))
        reject(edit_fields=lambda ir: ir.update(bindings=[row for row in ir['bindings'] if row['role'] == 'HousePresents']))
        reject(edit_fields=lambda ir: ir['bindings'][3].update(id=35))
        reject(edit_fields=lambda ir: ir['bindings'][3].update(path='data/opening.encroom'))
        reject(edit_fields=lambda ir: ir['bindings'].append(copy.deepcopy(ir['bindings'][0])))
        reject(edit_fields=lambda ir: ir['bindings'].append(dict(id=1, role='Room', path='data/other.encroom')))
        reject(edit_fields=lambda ir: ir['bindings'][0].update(id=30))
        reject(edit_fields=lambda ir: ir['bindings'][0].update(path=self.base['bindings'][0]['path']))
        reject(edit_fields=lambda ir: ir['bindings'][1].update(path='data/podunk.encroom'))
        reject(edit_fields=lambda ir: ir['sources'].update({'project.godot': '0' * 64}))
        reject(edit_base=lambda ir: ir['bindings'].append(copy.deepcopy(self.fields['bindings'][0])))
        candidate = copy.deepcopy(self.fields)
        candidate['sources'][next(iter(candidate['sources']))] = '0' * 64
        with self.assertRaises(ValueError):
            catalog.encode(catalog.merge_catalog(copy.deepcopy(self.base), candidate))

    def test_unknown_schema_fields_types_pin_and_empty_scope(self):
        for key, value in [('schema', 2), ('schema', True), ('kind', 'unknown'),
                           ('commit', '0' * 40), ('scope', ''), ('sources', {}),
                           ('unknown', 1)]:
            with self.subTest(key=key, value=value):
                self.reject(lambda ir: ir.update({key: value}))
        for collection in ('bindings', 'encounters'):
            self.reject(lambda ir: ir[collection][0].update(unknown=1))
            self.reject(lambda ir: ir.update({collection: {}}))

    def test_unknown_duplicate_or_reserved_ids_and_roles(self):
        root = next(i for i, row in enumerate(self.ir['bindings']) if row['role'] == 'Round')
        extra = next(i for i, row in enumerate(self.ir['bindings']) if row['role'] == 'EncounterBattle')
        for index, key, value in [(root, 'id', 0), (root, 'id', True),
                                  (root, 'id', 256), (root, 'role', 'Unknown'),
                                  (extra, 'id', 255), (extra, 'role', 'Battle'),
                                  (extra, 'id', self.ir['bindings'][root]['id'])]:
            with self.subTest(index=index, key=key, value=value):
                self.reject(lambda ir: ir['bindings'][index].update({key: value}))
        self.reject(lambda ir: ir['bindings'].append(copy.deepcopy(ir['bindings'][0])))
        self.reject(lambda ir: ir['bindings'].pop(root))
        self.reject(lambda ir: ir.update(bindings=[]))
        self.reject(lambda ir: ir['bindings'][extra].update(path=next(x['path'] for x in ir['bindings'] if x['role'] == 'Battle')))

    def test_introduction_optional_singleton_and_typed_path(self):
        index = next(i for i, row in enumerate(self.ir['bindings']) if row['role'] == 'Introduction')
        self.assertEqual(self.ir['bindings'][index]['id'], 25)
        for key, value in [('id', 256), ('id', 24), ('path', 'data/opening.encroom'), ('role', 'Unknown')]:
            self.reject(lambda ir: ir['bindings'][index].update({key: value}))
        older = copy.deepcopy(self.ir)
        older['bindings'].pop(index)
        catalog.validate(older)  # Existing schema1 catalogs remain readable.

    def test_bad_paths_suffixes_and_source_declarations(self):
        index = next(i for i, row in enumerate(self.ir['bindings']) if row['role'] == 'Round')
        for path in ('../round.encround', '/round.encround', 'C:/round.encround',
                     'data/../round.encround', 'data/./round.encround',
                     'data//round.encround', 'data\\round.encround',
                     'data/round.encbattle', 'data/round.encround/',
                     'data/round.encround?x', 'data/round.encround#x',
                     'data/round\x00.encround', 'data/é.encround',
                     'data/' + 'x' * 257 + '.encround', ''):
            with self.subTest(path=path):
                self.reject(lambda ir: ir['bindings'][index].update(path=path))
        for path, digest in [('../project.godot', '0' * 64), ('/project.godot', '0' * 64),
                             ('project.godot', 'invalid'), ('project.godot', 'A' * 64)]:
            self.reject(lambda ir: ir.update(sources={path: digest}))

    def test_encounter_type_reference_uniqueness_and_complete_coverage(self):
        for key, value in [('battle_id', 0), ('battle_id', 4),
                           ('battle_id', 255), ('round_id', 3),
                           ('round_id', True), ('round_id', 255)]:
            with self.subTest(key=key, value=value):
                self.reject(lambda ir: ir['encounters'][0].update({key: value}))
        self.reject(lambda ir: ir['encounters'].append(copy.deepcopy(ir['encounters'][0])))
        self.reject(lambda ir: ir['encounters'].pop())
        self.reject(lambda ir: ir['encounters'][1].update(battle_id=ir['encounters'][0]['battle_id']))
        self.reject(lambda ir: ir['encounters'][1].update(round_id=ir['encounters'][0]['round_id']))

    def test_extra_encounters_use_external_stable_ids(self):
        # Reassign every supplemental identity through IR. The compiler/schema
        # must accept data identities without named encounters in the program.
        candidate = copy.deepcopy(self.ir)
        remap = {row['id']: row['id'] + 1000 for row in candidate['bindings']
                 if row['role'] in ('EncounterBattle', 'EncounterRound')}
        for row in candidate['bindings']:
            row['id'] = remap.get(row['id'], row['id'])
        for row in candidate['encounters']:
            for key in ('battle_id', 'round_id'):
                row[key] = remap.get(row[key], row[key])
        decoded = catalog.decode(catalog.encode(candidate))
        self.assertEqual(decoded['encounters'], candidate['encounters'])

    def test_source_fingerprint_mismatch_never_compiles(self):
        candidate = copy.deepcopy(self.ir)
        source = next(iter(candidate['sources']))
        candidate['sources'][source] = '0' * 64
        with self.assertRaises(ValueError):
            catalog.encode(candidate)
        candidate = copy.deepcopy(self.ir)
        candidate['sources']['unreviewed/absent-source.gd'] = '0' * 64
        with self.assertRaises(ValueError):
            catalog.encode(candidate)

    def test_duplicate_json_fields_rejected(self):
        with tempfile.TemporaryDirectory(dir=self.fixture_parent) as temporary:
            file = Path(temporary) / 'duplicate.json'
            file.write_text('{"schema": 1, "schema": 1}', encoding='utf-8')
            with self.assertRaisesRegex(ValueError, 'Duplicate'):
                catalog.load_ir(file)

    def test_missing_empty_oversized_resources_and_symlink_escape(self):
        with tempfile.TemporaryDirectory(dir=self.fixture_parent) as temporary:
            root = Path(temporary) / 'romfs'
            root.mkdir()
            path = self.ir['bindings'][0]['path']
            with self.assertRaises(ValueError):
                catalog.checked_file(root, path)
            target = root / path
            target.parent.mkdir(parents=True)
            target.write_bytes(b'')
            with self.assertRaises(ValueError):
                catalog.checked_file(root, path)
            with target.open('wb') as stream:
                stream.truncate(catalog.RESOURCE_LIMIT + 1)
            with self.assertRaises(ValueError):
                catalog.checked_file(root, path)
            # Traversal is rejected even before a pointed-to file is read.
            with self.assertRaises(ValueError):
                catalog.checked_file(root, '../outside')

    def test_binary_unknown_versions_capabilities_crc_and_truncation(self):
        original = catalog.OUT.read_bytes()
        for size in range(len(original)):
            with self.subTest(size=size), self.assertRaises(ValueError):
                catalog.decode(original[:size])
        for offset, value in [(8, 0), (8, 2), (20, 0), (20, 1), (20, 2), (20, 4), (24, 1), (28, 1)]:
            bad = bytearray(original)
            struct.pack_into('<I', bad, offset, value)
            with self.subTest(offset=offset, value=value), self.assertRaises(ValueError):
                catalog.decode(bad)
        bad = bytearray(original)
        bad[-1] ^= 1
        with self.assertRaises(ValueError):
            catalog.decode(bad)
        bad = bytearray(original)
        bad[32] ^= 1
        struct.pack_into('<I', bad, 16, zlib.crc32(bad[32:]))
        with self.assertRaises(ValueError):
            catalog.decode(bad)

    def test_release_stage_requires_every_catalog_bound_file_and_fingerprint(self):
        files = {Path(row['path']): (ROOT / 'romfs' / row['path']).read_bytes()
                 for row in self.ir['bindings']}
        expected = {Path('data/native.encresources'): catalog.OUT.read_bytes()}
        self.assertEqual(catalog.stage_files(ROOT / 'romfs', files), expected)
        victim = next(iter(files))
        missing = dict(files)
        del missing[victim]
        with self.assertRaises(ValueError):
            catalog.stage_files(ROOT / 'romfs', missing)
        corrupt = dict(files)
        value = bytearray(corrupt[victim])
        value[-1] ^= 1
        corrupt[victim] = bytes(value)
        with self.assertRaises(ValueError):
            catalog.stage_files(ROOT / 'romfs', corrupt)

    def test_3ds_entry_uses_catalog_without_embedded_root_game_paths(self):
        source = (ROOT / 'platform/ctr/main.cpp').read_text(encoding='utf-8')
        self.assertIn('ResourceCatalog', source)
        self.assertIn('companion_path', source)
        for row in self.ir['bindings']:
            with self.subTest(role=row['role'], path=row['path']):
                self.assertNotIn('"' + row['path'] + '"', source)
                self.assertNotIn('"romfs:/' + row['path'] + '"', source)
                self.assertNotIn('"' + Path(row['path']).name + '"', source)


if __name__ == '__main__':
    unittest.main()
