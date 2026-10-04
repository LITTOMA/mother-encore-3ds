import copy
import unittest
from tools.ci_quick import REQUIRED, expression, select


def metadata(*extra):
    return {'kind': 'ctestInfo', 'version': {'major': 1, 'minor': 0},
            'tests': [{'name': name} for name in [*REQUIRED, *extra]]}


class QuickSelectionTests(unittest.TestCase):
    def test_real_production_selection_excludes_fixture_and_source_suites(self):
        self.assertEqual(select(metadata('core', 'host_smoke', 'python_tools', 'world_program_bindings')), REQUIRED)

    def test_introduction_is_included_when_implemented(self):
        self.assertEqual(select(metadata('original_introduction'))['original_introduction'], 'encore_introduction_tests')

    def test_every_missing_production_registration_fails(self):
        for name in REQUIRED:
            with self.subTest(name=name):
                value = metadata()
                value['tests'] = [test for test in value['tests'] if test['name'] != name]
                with self.assertRaises(ValueError):
                    select(value)

    def test_invalid_metadata_fails(self):
        for change in ({'kind': 'other'}, {'version': {'major': 2, 'minor': 0}}, {'tests': None}, {'tests': [None]}):
            value = copy.deepcopy(metadata())
            value.update(change)
            with self.subTest(change=change), self.assertRaises(ValueError):
                select(value)

    def test_duplicate_registration_fails(self):
        with self.assertRaises(ValueError):
            select(metadata('playable_opening'))

    def test_regex_is_exact_and_empty_selection_fails(self):
        import re
        pattern = re.compile(expression(REQUIRED))
        self.assertTrue(pattern.fullmatch('playable_opening'))
        self.assertFalse(pattern.fullmatch('playable_opening_fixture'))
        self.assertFalse(pattern.fullmatch('core'))
        with self.assertRaises(ValueError):
            expression([])


if __name__ == '__main__':
    unittest.main()
