"""Check real worker isolation, method accounting, and CTest scheduling gates."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from tests import python_suite as runner


ROOT = Path(__file__).resolve().parents[1]


class ParallelTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / 'tests').mkdir()
        (self.root / 'tests/__init__.py').write_text('', encoding='utf-8')
        self.output = self.root / 'receipts'

    def module(self, name, body):
        (self.root / ('tests/' + name + '.py')).write_text(body, encoding='utf-8')
        return name + '.Case.test_run'

    def test_four_processes_overlap_and_isolate_module_mocks(self):
        (self.root / 'tests/shared.py').write_text('value = 0\n', encoding='utf-8')
        groups = {}
        names = ['test_native_content', 'test_native_house', 'test_native_round', 'test_native_restore']
        for value, name in enumerate(names, 1):
            others = [other for other in names if other != name]
            body = '''import os, time, unittest
from pathlib import Path
from unittest.mock import patch
from tests import shared
class Case(unittest.TestCase):
 def test_run(self):
  with patch.object(shared, 'value', VALUE):
   Path('NAME.started').write_text(str(os.getpid()))
   deadline = time.monotonic() + 10
   while not all(Path(n + '.started').exists() for n in OTHERS) and time.monotonic() < deadline:
    time.sleep(.01)
   self.assertTrue(all(Path(n + '.started').exists() for n in OTHERS), 'workers did not overlap')
   self.assertEqual(shared.value, VALUE)
'''.replace('VALUE', str(value)).replace('NAME', name).replace('OTHERS', repr(others))
            groups[name] = [self.module(name, body)]
        self.assertTrue(runner.run_parallel(self.root, groups, self.output, 4))
        pids = [(self.root / (name + '.started')).read_text() for name in groups]
        self.assertEqual(len(set(pids)), 4)
        for name, ids in groups.items():
            receipt = json.loads((self.output / (name + '.result.json')).read_text())
            self.assertEqual(receipt['executed_ids'], ids)
            self.assertTrue(receipt['success'])

    def test_failure_propagates_and_other_worker_still_runs(self):
        good = self.module('test_native_content', 'import unittest\nclass Case(unittest.TestCase):\n def test_run(self): pass\n')
        bad = self.module('test_native_house', 'import unittest\nclass Case(unittest.TestCase):\n def test_run(self): self.fail("intentional worker failure")\n')
        groups = dict(test_native_content=[good], test_native_house=[bad])
        self.assertFalse(runner.run_parallel(self.root, groups, self.output, 4))
        for name in groups:
            self.assertTrue((self.output / (name + '.log')).is_file())
            receipt = json.loads((self.output / (name + '.result.json')).read_text())
            self.assertEqual(receipt['executed_ids'], groups[name])
            self.assertEqual(receipt['success'], name == 'test_native_content')

    def test_changed_collection_cannot_reuse_stale_success(self):
        actual = self.module('test_native_content', 'import unittest\nclass Case(unittest.TestCase):\n def test_run(self): pass\n')
        self.output.mkdir()
        receipt = self.output / 'test_native_content.result.json'
        receipt.write_text(json.dumps(dict(success=True, module='test_native_content',
            expected_ids=[actual], executed_ids=[actual])))
        self.assertFalse(runner.run_parallel(self.root,
            {'test_native_content': ['test_native_content.Case.test_missing']}, self.output, 4))
        self.assertFalse(receipt.exists())

    def test_worker_rejects_unknown_or_duplicate_plan_before_execution(self):
        actual = self.module('test_native_content', '''import unittest
from pathlib import Path
class Case(unittest.TestCase):
 def test_run(self): Path('executed').touch()
''')
        plans = [dict(module='test_unknown', ids=[actual]),
                 dict(module='test_native_content', ids=[]),
                 dict(module='test_native_content', ids=[actual, actual]),
                 dict(module='test_native_content', ids=[actual], unknown=True),
                 dict(module='test_native_content', ids=['test_native_content.Case.test_missing'])]
        for value in plans:
            with self.subTest(plan=value):
                plan = self.root / 'plan.json'
                plan.write_text(json.dumps(value))
                result = subprocess.run([sys.executable, str(ROOT / 'tests/python_suite.py'),
                    '--project', str(self.root), '--worker-plan', str(plan),
                    '--worker-result', str(self.root / 'result.json')], cwd=self.root,
                    capture_output=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse((self.root / 'executed').exists())
                self.assertFalse((self.root / 'result.json').exists())

    def test_invalid_job_count_and_unreviewed_modules_fail_closed(self):
        for jobs in (0, 2, 3, 65, True):
            with self.subTest(jobs=jobs), self.assertRaises(ValueError):
                runner.run_parallel(self.root, {}, self.output, jobs)
        with self.assertRaises(ValueError):
            runner.run_parallel(self.root, {'../unknown': ['x']}, self.output, 4)

    def test_partition_preserves_unknown_modules_and_detects_missing_methods(self):
        class Case(unittest.TestCase):
            def __init__(self, name):
                super().__init__()
                self.name = name
            def id(self):
                return self.name
        selected = Case('tests.test_native_content.Case.test_a')
        unknown = Case('test_future_writer.Case.test_b')
        with patch.object(unittest.defaultTestLoader, 'loadTestsFromName',
                          return_value=unittest.TestSuite([selected])):
            serial, groups = runner.parallel_partition(unittest.TestSuite([selected, unknown]))
            self.assertEqual([runner.identity(c) for c in runner.cases(serial)],
                             ['test_future_writer.Case.test_b'])
            self.assertEqual(groups, {'test_native_content': ['test_native_content.Case.test_a']})
            with self.assertRaises(ValueError):
                runner.parallel_partition(unittest.TestSuite([selected, selected]))
        with patch.object(unittest.defaultTestLoader, 'loadTestsFromName',
                          return_value=unittest.TestSuite()):
            with self.assertRaises(ValueError):
                runner.parallel_partition(unittest.TestSuite([selected]))

    @unittest.skipUnless(shutil.which('cmake') and shutil.which('ctest'), 'CMake/CTest unavailable')
    def test_ctest_defaults_and_four_slot_policy(self):
        helper = (ROOT / 'cmake/TestParallelism.cmake').as_posix()
        source = self.root / 'CMakeLists.txt'
        source.write_text('cmake_minimum_required(VERSION 3.16)\nproject(Schedule NONE)\n'
            'enable_testing()\nset(BUILD_TESTING ON)\n'
            'foreach(name room_data phone_presentation_bindings python_tools future_writer timing_probe)\n'
            ' add_test(NAME ${name} COMMAND "' + sys.executable.replace('\\', '/') + '" -c "pass")\n'
            'endforeach()\ninclude("' + helper + '")\n', encoding='utf-8')
        for jobs in (0, 2, 3, 65, 'invalid'):
            with self.subTest(invalid_jobs=jobs):
                result = subprocess.run(['cmake', '-S', str(self.root), '-B',
                    str(self.root / ('invalid-' + str(jobs))), '-DENCORE_TEST_PARALLEL=ON',
                    '-DENCORE_TEST_JOBS=' + str(jobs)], capture_output=True)
                self.assertNotEqual(result.returncode, 0)
        for enabled in ('OFF', 'ON'):
            with self.subTest(enabled=enabled):
                build = self.root / enabled
                subprocess.run(['cmake', '-S', str(self.root), '-B', str(build),
                    '-DENCORE_TEST_PARALLEL=' + enabled], check=True, capture_output=True)
                metadata = json.loads(subprocess.check_output(['ctest', '--test-dir',
                    str(build), '-C', 'Release', '--show-only=json-v1'], text=True))
                props = {t['name']: {p['name']: p['value'] for p in t['properties']}
                         for t in metadata['tests']}
                if enabled == 'OFF':
                    self.assertTrue(all('RUN_SERIAL' not in p for p in props.values()))
                else:
                    self.assertTrue(props['future_writer']['RUN_SERIAL'])
                    self.assertTrue(props['timing_probe']['RUN_SERIAL'])
                    self.assertFalse(props['room_data'].get('RUN_SERIAL', False))
                    self.assertFalse(props['phone_presentation_bindings'].get('RUN_SERIAL', False))
                    self.assertEqual(props['python_tools']['PROCESSORS'], 4)
                    self.assertIn('ENCORE_PYTHON_TEST_JOBS=4', props['python_tools']['ENVIRONMENT'])


if __name__ == '__main__':
    unittest.main()
