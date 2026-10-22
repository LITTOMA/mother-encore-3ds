import copy
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from tools import catacombs_item_catalog as catalog
from tests import prepare_catacombs_item_catalog as fixture


class CatacombsItemCatalogTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.actual = catalog.derive()
        cls.root = catalog.ROOT / 'build/tests'
        cls.root.mkdir(parents=True, exist_ok=True)

    def temporary(self):
        folder = tempfile.TemporaryDirectory(prefix='catacombs-catalog-', dir=self.root)
        self.addCleanup(folder.cleanup)
        return Path(folder.name)

    def test_actual_original_definitions_flags_and_genuine_art_receipt(self):
        self.assertEqual(catalog.load(), self.actual)
        rows = self.actual['entries']
        self.assertEqual([(v['id'], v['source'], len(v['grants'])) for v in rows],
                         [(5, 'Hamburger', 4), (6, 'Fries', 1), (7, 'Antidote', 2)])
        self.assertEqual([v['document']['HPrecover'] for v in rows], [50, 20, 0])
        self.assertEqual(rows[2]['document']['status_heals'], ['poisoned'])
        self.assertEqual(len({g['owner'] for v in rows for g in v['grants']}), 7)
        proof = catalog.receipt()
        self.assertEqual(set(proof['outputs']), {v['output'] for v in rows})

    def test_unknown_malformed_and_type_coerced_recipe_reject(self):
        path = self.temporary() / 'candidate.json'
        changes = [lambda v: v.update(schema=True), lambda v: v.update(schema=1.0),
                   lambda v: v.update(extra=1), lambda v: v.update(commit='0' * 40),
                   lambda v: v['entries'].pop(), lambda v: v['entries'].reverse(),
                   lambda v: v['entries'][0].update(id=6),
                   lambda v: v['entries'][0].update(output='../outside.t3x'),
                   lambda v: v['entries'][0]['document'].update(HPrecover=50.0),
                   lambda v: v['entries'][0]['grants'].pop(),
                   lambda v: v['entries'][0]['grants'][0].update(flag='Catacombs/Other'),
                   lambda v: v['entries'][0]['labels']['en'].update(name='Changed'),
                   lambda v: v.update(dropped_recipe_sha256='0' * 64)]
        with patch.object(catalog, 'derive', return_value=self.actual):
            for change in changes:
                data = copy.deepcopy(self.actual)
                change(data)
                path.write_text(json.dumps(data), encoding='utf-8')
                with self.subTest(change=change), self.assertRaises(ValueError):
                    catalog.load(path)

    def test_review_hash_and_duplicate_json_keys_reject(self):
        path = self.temporary() / 'review.json'
        path.write_text('{"schema":1,"schema":1}')
        with self.assertRaises(ValueError):
            catalog.read(path)
        review = catalog.read(catalog.ROOT / catalog.REVIEW)
        changes = [lambda v: v.update(schema=True), lambda v: v.update(extra=1),
                   lambda v: v.update(ir_sha256='0' * 64),
                   lambda v: v.update(producer_sha256='0' * 64),
                   lambda v: v.update(permission='changed')]
        original_read = catalog.read
        for change in changes:
            data = copy.deepcopy(review)
            change(data)
            with patch.object(catalog, 'derive', return_value=self.actual), \
                 patch.object(catalog, 'read', side_effect=lambda p: data if Path(p) == catalog.ROOT / catalog.REVIEW else original_read(p)), \
                 self.assertRaises(ValueError):
                catalog.load()

    def test_texture_coverage_and_provenance_reject(self):
        proof = catalog.read(catalog.ROOT / catalog.RECEIPT)
        changes = [lambda v: v.update(schema=True), lambda v: v.update(extra=1),
                   lambda v: v.update(tex3ds_sha256='not-a-sha'),
                   lambda v: v.update(ir_sha256='0' * 64),
                   lambda v: v.update(producer_sha256='0' * 64),
                   lambda v: v['outputs'].pop(next(iter(v['outputs']))),
                   lambda v: v['outputs'].update({'../outside.t3x': {}}),
                   lambda v: next(iter(v['outputs'].values())).update(sha256='0' * 64),
                   lambda v: next(iter(v['outputs'].values())).update(bytes=1.0)]
        for change in changes:
            data = copy.deepcopy(proof)
            change(data)
            with patch.object(catalog, 'load', return_value=self.actual), \
                 patch.object(catalog, 'read', return_value=data), \
                 self.assertRaises(ValueError):
                catalog.receipt()

    def test_missing_corrupt_and_repeated_stage(self):
        original = catalog.stage_files(catalog.ROOT / 'romfs')
        self.assertEqual(original, catalog.stage_files(catalog.ROOT / 'romfs'))
        folder = self.temporary()
        with patch.object(catalog, 'load', return_value=self.actual):
            with self.assertRaises(ValueError):
                catalog.receipt(folder)

    def test_append_preserves_stable_catalog_and_keeps_actions_disabled(self):
        base = catalog.read(catalog.ROOT / 'content/native-items.json')
        original = copy.deepcopy(base)
        candidate = catalog.append_item(None, base)
        self.assertEqual(base, original)
        self.assertEqual(candidate['definitions'][:4], base['definitions'])
        self.assertEqual(candidate['resources'][:len(base['resources'])], base['resources'])
        self.assertEqual([(v['id'], v['source'], v['can_use'], v['heal_hp'])
                          for v in candidate['definitions'][4:]],
                         [(5, 'Hamburger', 0, 0), (6, 'Fries', 0, 0), (7, 'Antidote', 0, 0)])
        base['definitions'][0]['id'] = 4
        with self.assertRaises(ValueError):
            catalog.append_item(None, base)

    def test_owned_fixture_repeatability_and_unowned_corruption_reject(self):
        root = self.temporary()
        path = root / 'fixture'
        first = fixture.build(path)
        before = {p.name: (p.read_bytes(), p.stat().st_mtime_ns) for p in path.iterdir()}
        self.assertEqual(fixture.build(path), first)
        self.assertEqual(before, {p.name: (p.read_bytes(), p.stat().st_mtime_ns) for p in path.iterdir()})
        (path / 'candidate.encsession').write_bytes(b'changed')
        with self.assertRaises(ValueError):
            fixture.build(path)
        unowned = root / 'unowned'
        unowned.mkdir()
        with self.assertRaises(ValueError):
            fixture.build(unowned)
        with self.assertRaises(ValueError):
            fixture.build(catalog.ROOT / 'build')
            for relative, blob in original.items():
                path = folder / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(blob)
            self.assertEqual(original, catalog.stage_files(folder))
            path.write_bytes(blob + b'bad')
            with self.assertRaises(ValueError):
                catalog.receipt(folder)


if __name__ == '__main__':
    unittest.main()
