"""Coverage registration accepts explicit modules without losing or duplicating cases."""
import json
from pathlib import Path
import unittest
from unittest import mock
from tests import python_suite as subject

ROOT = Path(__file__).resolve().parents[1]

class Case:
    def __init__(self, identifier):
        self.identifier = identifier
    def __call__(self, *args, **kwargs): pass
    def id(self): return self.identifier

class Loader:
    full = ('test_one.Check.case', 'test_two.Check.case', 'test_new.Check.case')
    dedicated = None
    def discover(self, *args, **kwargs):
        return unittest.TestSuite(Case(i) for i in self.full)
    def loadTestsFromNames(self, names):
        modules = {n.removeprefix('tests.') for n in names}
        ids = self.full if self.dedicated is None else self.dedicated
        return unittest.TestSuite(Case(i) for i in ids if i.split('.')[0] in modules)

class Registration(unittest.TestCase):
    def test_explicit_single_and_multiple_modules(self):
        for names in [('test_one',), ('test_one', 'test_two')]:
            command = ['python', '-m', 'unittest'] + ['tests.' + n for n in names]
            self.assertEqual(subject.registered_modules(command, ROOT), names)
    def test_direct_script(self):
        self.assertEqual(subject.registered_modules(['python', str(ROOT/'tests/test_one.py')], ROOT), ('test_one',))
    def test_unrelated_program_is_not_a_registration(self):
        for command in ([], ['program'], ['program', '--version'], ['python', str(ROOT/'tools/test_one.py')]):
            self.assertIsNone(subject.registered_modules(command, ROOT))
    def test_unknown_or_ambiguous_unittest_selectors(self):
        for names in ([], ['tests.test_one.Check'], ['test_one'], ['tests.test_one', 'tests.test_one'], ['tests.test_one', '-q']):
            with self.subTest(names=names), self.assertRaises(ValueError):
                subject.registered_modules(['python', '-m', 'unittest'] + names, ROOT)
    def test_direct_script_arguments_are_not_silently_ignored(self):
        with self.assertRaises(ValueError):
            subject.registered_modules(['python', str(ROOT/'tests/test_one.py'), '--unknown'], ROOT)

class Partition(unittest.TestCase):
    registry = {'dedicated': ('test_one', 'test_two')}
    def metadata(self):
        return [{'name':'dedicated', 'command':['python','-m','unittest','tests.test_one','tests.test_two']}]
    def plan(self, metadata=None, loader=None, registry=None):
        with mock.patch.object(subject, 'DEDICATED', self.registry if registry is None else registry), \
             mock.patch.object(subject.unittest, 'defaultTestLoader', loader or Loader()), \
             mock.patch.object(subject.subprocess, 'check_output', return_value=json.dumps({'tests':self.metadata() if metadata is None else metadata})):
            return subject.plan(ROOT, ROOT/'build/full-content-regressions/cmake')
    def test_exact_partition_keeps_new_modules_in_aggregate(self):
        remaining, receipt = self.plan()
        self.assertEqual([subject.identity(c) for c in subject.cases(remaining)], ['test_new.Check.case'])
        self.assertEqual(receipt['discovered'], 3)
        self.assertEqual(receipt['aggregate'], 1)
        self.assertEqual(receipt['dedicated'], 2)
        self.assertTrue(receipt['multiset_equal'])
    def test_actual_area_and_run_sources_are_partitioned_once(self):
        registry = {name: subject.DEDICATED[name] for name in ('field_area_source', 'field_steps_source')}
        loader = Loader()
        loader.full = ('test_field_area.Check.case', 'test_field_footsteps.Check.case',
                       'test_run_footstep_reference.Check.case', 'test_new.Check.case')
        metadata = [dict(name=name, command=['python', '-m', 'unittest'] +
                        ['tests.' + module for module in modules]) for name, modules in registry.items()]
        remaining, receipt = self.plan(metadata, loader, registry)
        self.assertEqual([subject.identity(c) for c in subject.cases(remaining)], ['test_new.Check.case'])
        self.assertEqual((receipt['discovered'], receipt['aggregate'], receipt['dedicated']), (4, 1, 3))
        self.assertTrue(receipt['multiset_equal'])
    def test_missing_registration_rejected(self):
        with self.assertRaisesRegex(ValueError, 'Missing dedicated'): self.plan([])
    def test_phone_source_registration_owns_every_method_once(self):
        registry = {'phone_linker_source': subject.DEDICATED['phone_linker_source']}
        self.assertEqual(registry['phone_linker_source'], ('test_phone_linker',))
        loader = Loader()
        loader.full = ('test_phone_linker.Check.first', 'test_phone_linker.Check.last', 'test_new.Check.case')
        metadata = [dict(name='phone_linker_source', command=['python', '-m', 'unittest', 'tests.test_phone_linker'])]
        remaining, receipt = self.plan(metadata, loader, registry)
        self.assertEqual([subject.identity(c) for c in subject.cases(remaining)], ['test_new.Check.case'])
        self.assertEqual((receipt['discovered'], receipt['aggregate'], receipt['dedicated']), (3, 1, 2))
        self.assertEqual(receipt['dedicated_ids']['phone_linker_source'], sorted(loader.full[:2]))
        self.assertTrue(receipt['multiset_equal'])
    def test_changed_module_list_rejected(self):
        data=self.metadata(); data[0]['command'].pop()
        with self.assertRaisesRegex(ValueError, 'changed dedicated'): self.plan(data)
    def test_unknown_registration_rejected(self):
        data=self.metadata()+[{'name':'unreviewed','command':['python','-m','unittest','tests.test_new']}]
        with self.assertRaisesRegex(ValueError, 'Unmapped dedicated'): self.plan(data)
    def test_duplicate_registration_rejected(self):
        with self.assertRaisesRegex(ValueError, 'changed dedicated'): self.plan(self.metadata()*2)
    def test_duplicate_discovery_case_rejected(self):
        loader=Loader();loader.full=loader.full+(loader.full[0],)
        with self.assertRaisesRegex(ValueError, 'Duplicate IDs'): self.plan(loader=loader)
    def test_dedicated_case_loss_rejected(self):
        loader=Loader();loader.dedicated=('test_one.Check.case',)
        with self.assertRaisesRegex(ValueError, 'differs from discovery'): self.plan(loader=loader)
    def test_case_cannot_be_owned_by_two_registrations(self):
        data=[{'name':name,'command':['python','-m','unittest','tests.test_one']} for name in ('first','second')]
        with self.assertRaisesRegex(ValueError, 'loses or duplicates'):
            self.plan(data, registry={'first':('test_one',),'second':('test_one',)})

if __name__ == '__main__': unittest.main()
