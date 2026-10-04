#!/usr/bin/env python3
"""Run discovery cases once, preserving separately registered CTest suites."""
import argparse
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import subprocess
import sys
import time
import unittest


DEDICATED = {
    'encounter_dependency_manifest': 'test_encounter_dependencies',
    'battle_round_source_bindings': 'test_battle_round_bindings',
    'round_recipe_source': 'test_round_presentation_recipe',
    'boss_presentation_bindings': 'test_boss_presentation_bindings',
    'phone_presentation_bindings': 'test_phone_presentation_bindings',
    'items_presentation_bindings': 'test_items_presentation_bindings',
    'world_program_bindings': 'test_world_program_bindings',
    'programme_lowering_recipe': 'test_programme_lowering_recipe',
    'battle_entry_bindings': 'test_battle_entry_bindings',
    'ui_presentation_bindings': 'test_ui_presentation_bindings',
    'house_source_bindings': 'test_house_source_bindings',
    'naming_presentation_bindings': 'test_naming_presentation_bindings',
    'phone_linker_bindings': 'test_phone_linker_bindings',
    'pillow_source_bindings': 'test_pillow_source_bindings',
}

# Audited modules read shipping inputs and mutate only copies/unique temporary
# directories. Module processes isolate their unittest mocks and module globals.
# New modules remain serial; changes to these modules require an IO re-review.
READ_ONLY = frozenset(('test_native_content', 'test_native_house',
    'test_native_round', 'test_native_restore', 'test_phone_dialogue',
    'test_dad_record_dialogue'))


def cases(suite):
    for item in suite:
        if isinstance(item, unittest.TestSuite):
            yield from cases(item)
        else:
            if isinstance(item, unittest.loader._FailedTest):
                raise ValueError('Test collection import failed: ' + item.id()) from item._exception
            yield item


def identity(case):
    value = case.id()
    return value[6:] if value.startswith('tests.') else value


def registered_module(command, project):
    if len(command) == 4 and command[1:3] == ['-m', 'unittest']:
        name = command[3]
        if name.startswith('tests.test_') and '.' not in name[6:]:
            return name[6:]
    if len(command) == 2:
        path = Path(command[1]).resolve()
        if path.parent == project / 'tests' and path.name.startswith('test_') and path.suffix == '.py':
            return path.stem
    return None


def parallel_partition(suite):
    original = list(cases(suite))
    groups = {}
    serial = []
    for case in original:
        module = identity(case).split('.')[0]
        if module in READ_ONLY:
            groups.setdefault(module, []).append(identity(case))
        else:
            serial.append(case)
    combined = Counter(identity(c) for c in serial)
    for module, ids in groups.items():
        observed = Counter(identity(c) for c in cases(
            unittest.defaultTestLoader.loadTestsFromName('tests.' + module)))
        if not ids or len(ids) != len(set(ids)) or observed != Counter(ids):
            raise ValueError('Parallel module differs from discovery: ' + module)
        combined.update(ids)
    if combined != Counter(identity(c) for c in original):
        raise ValueError('Parallel partition loses or duplicates methods')
    return unittest.TestSuite(serial), groups


def worker(project, expected, result_path):
    sys.path.insert(0, str(Path(project).resolve()))
    value = json.loads(Path(expected).read_text(encoding='utf-8'))
    if type(value) is not dict or set(value) != {'module', 'ids'}:
        raise ValueError('Unknown worker plan fields')
    module, ids = value['module'], value['ids']
    if (type(module) is not str or module not in READ_ONLY or type(ids) is not list
        or not ids or any(type(i) is not str for i in ids) or len(ids) != len(set(ids))):
        raise ValueError('Invalid worker module or method IDs')
    suite = unittest.defaultTestLoader.loadTestsFromName('tests.' + module)
    if Counter(identity(c) for c in cases(suite)) != Counter(ids):
        raise ValueError('Worker collection differs from admitted method IDs')
    executed = []
    class TrackedResult(unittest.TextTestResult):
        def startTest(self, test):
            executed.append(identity(test))
            super().startTest(test)
    start = time.monotonic()
    result = unittest.TextTestRunner(verbosity=2, resultclass=TrackedResult).run(suite)
    success = result.wasSuccessful() and Counter(executed) == Counter(ids)
    Path(result_path).write_text(json.dumps(dict(module=module, expected_ids=ids,
        executed_ids=executed, success=success, seconds=time.monotonic()-start), indent=2)+'\n', encoding='utf-8')
    return 0 if success else 1


def run_parallel(project, groups, directory, jobs):
    if type(jobs) is not int or jobs not in (1, 2):
        raise ValueError('Python test jobs must be 1 or 2')
    for module, ids in groups.items():
        if (module not in READ_ONLY or type(ids) is not list or not ids
            or any(type(i) is not str or not i.startswith(module + '.') for i in ids)
            or len(ids) != len(set(ids))):
            raise ValueError('Invalid parallel module or method IDs')
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    def run(item):
        module, ids = item
        expected = directory / (module + '.plan.json')
        result_path = directory / (module + '.result.json')
        log = directory / (module + '.log')
        expected.write_text(json.dumps(dict(module=module, ids=ids)), encoding='utf-8')
        # A failed launch must never reuse a preceding invocation's receipt.
        result_path.unlink(missing_ok=True)
        with log.open('wb') as output:
            process = subprocess.run([sys.executable, str(Path(__file__).resolve()),
                '--project', str(project), '--worker-plan', str(expected),
                '--worker-result', str(result_path)], cwd=project, stdout=output,
                stderr=subprocess.STDOUT, env=dict(os.environ, PYTHONIOENCODING='utf-8'))
        print(log.read_text(encoding='utf-8', errors='replace'), flush=True)
        if process.returncode != 0 or not result_path.is_file():
            return False
        result = json.loads(result_path.read_text(encoding='utf-8'))
        return (result.get('module') == module and result.get('success') is True
            and Counter(result.get('expected_ids', [])) == Counter(ids)
            and Counter(result.get('executed_ids', [])) == Counter(ids))
    with ThreadPoolExecutor(max_workers=jobs) as pool:
        # Threads only supervise independent interpreter processes; tests do
        # not share Python globals. Consume every result, including failures.
        results = list(pool.map(run, sorted(groups.items())))
    return all(results)


def plan(project, ctest_dir):
    project = Path(project).resolve()
    if str(project) not in sys.path:
        sys.path.insert(0, str(project))
    metadata = json.loads(subprocess.check_output(
        ['ctest', '--test-dir', str(ctest_dir), '--show-only=json-v1'], text=True))
    observed = {}
    for test in metadata['tests']:
        name = test['name']
        command = test.get('command', [])
        module = registered_module(command, project)
        if name in DEDICATED:
            if name in observed or module != DEDICATED[name]:
                raise ValueError('Missing/changed dedicated unittest command: ' + name)
            observed[name] = module
        elif module is not None:
            raise ValueError('Unmapped dedicated unittest registration: ' + name)
    if observed != DEDICATED:
        raise ValueError('Missing dedicated CTest registration: ' + ', '.join(sorted(set(DEDICATED) - set(observed))))
    full = list(cases(unittest.defaultTestLoader.discover(str(project / 'tests'), pattern='test_*.py')))
    full_ids = Counter(identity(c) for c in full)
    if any(count != 1 for count in full_ids.values()):
        raise ValueError('Duplicate IDs in unittest discovery')
    modules = set(DEDICATED.values())
    remaining = [c for c in full if identity(c).split('.')[0] not in modules]
    remaining_ids = Counter(identity(c) for c in remaining)
    groups = {}
    combined = remaining_ids.copy()
    for name, module in DEDICATED.items():
        dedicated = list(cases(unittest.defaultTestLoader.loadTestsFromName('tests.' + module)))
        ids = Counter(identity(c) for c in dedicated)
        expected = Counter({key: count for key, count in full_ids.items() if key.split('.')[0] == module})
        if not expected or ids != expected:
            raise ValueError('Dedicated suite differs from discovery: ' + name)
        combined.update(ids)
        groups[name] = sorted(ids.elements())
    if combined != full_ids:
        raise ValueError('Python test partition loses or duplicates methods')
    receipt = dict(discovered=len(full), aggregate=len(remaining), dedicated=len(full) - len(remaining),
                   discovery_ids=sorted(full_ids.elements()), aggregate_ids=sorted(remaining_ids.elements()),
                   dedicated_ids=groups, multiset_equal=True,
                   policy='New unregistered modules stay in discovery; dedicated modules run through their existing CTest commands. No test methods or subTest assertions changed.')
    return unittest.TestSuite(remaining), receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--ctest-dir', type=Path)
    parser.add_argument('--jobs', type=int, choices=(1, 2), default=int(os.environ.get('ENCORE_PYTHON_TEST_JOBS', '1')))
    parser.add_argument('--worker-plan', type=Path)
    parser.add_argument('--worker-result', type=Path)
    parser.add_argument('--collect-only', action='store_true')
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    if args.worker_plan or args.worker_result:
        if not args.worker_plan or not args.worker_result:
            parser.error('worker plan and result must be supplied together')
        return worker(args.project, args.worker_plan, args.worker_result)
    if args.ctest_dir is None:
        parser.error('--ctest-dir is required')
    if args.jobs not in (1, 2):
        parser.error('Python test jobs must be 1 or 2')
    suite, receipt = plan(args.project, args.ctest_dir)
    serial, groups = parallel_partition(suite) if args.jobs == 2 else (suite, {})
    receipt['parallel_modules'] = groups
    receipt['serial_ids'] = sorted(identity(c) for c in cases(serial))
    print('Python method partition: {discovered} discovered = {aggregate} aggregate + {dedicated} dedicated; exact ID multiset preserved'.format(**receipt), flush=True)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf8')
    if args.collect_only:
        return 0
    # Writers/unreviewed modules run only after every read-only worker exits.
    parallel_ok = run_parallel(args.project.resolve(), groups, args.ctest_dir.resolve() / 'python-workers', args.jobs) if groups else True
    serial_ok = unittest.TextTestRunner(verbosity=2).run(serial).wasSuccessful()
    return 0 if parallel_ok and serial_ok else 1


if __name__ == '__main__':
    raise SystemExit(main())
