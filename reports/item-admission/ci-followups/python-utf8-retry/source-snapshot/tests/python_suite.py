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
    'field_area_source': ('test_field_area',),
    'field_steps_source': ('test_field_footsteps', 'test_run_footstep_reference'),
    'field_story_source': ('test_field_story',),
    'bridge_round_source': ('test_bridge_round',),
    'field_npc_source': ('test_field_npcs',),
    'encounter_dependency_manifest': ('test_encounter_dependencies',),
    'battle_round_source_bindings': ('test_battle_round_bindings',),
    'round_recipe_source': ('test_round_presentation_recipe',),
    'boss_presentation_bindings': ('test_boss_presentation_bindings',),
    'phone_presentation_bindings': ('test_phone_presentation_bindings',),
    'items_presentation_bindings': ('test_items_presentation_bindings',),
    'world_program_bindings': ('test_world_program_bindings',),
    'programme_lowering_recipe': ('test_programme_lowering_recipe',),
    'battle_entry_bindings': ('test_battle_entry_bindings',),
    'ui_presentation_bindings': ('test_ui_presentation_bindings',),
    'house_source_bindings': ('test_house_source_bindings',),
    'naming_presentation_bindings': ('test_naming_presentation_bindings',),
    'phone_linker_bindings': ('test_phone_linker_bindings',),
    'phone_linker_source': ('test_phone_linker',),
    'pillow_source_bindings': ('test_pillow_source_bindings',),
    'house_inspection_source': ('test_house_inspections',),
    'family_followup_source': ('test_family_followup',),
    'drawer_program_source': ('test_drawer_program',),
    'storage_source': ('test_native_storage', 'test_storage_dialogue'),
    'item_details_source': ('test_item_details', 'test_catacombs_item_details'),
    'field_equipment_source': ('test_field_equipment',),
    'field_audio_source': ('test_field_audio',),
    'field_menus_source': ('test_field_menus',),
    'present_data_source': ('test_house_presents',),
    'basement_presents_source': ('test_basement_presents',),
    'mick_treats_source': ('test_mick_treats',),
    'runtime_layout': ('test_runtime_layout',),
}

# Audited modules read shipping inputs and mutate only copies/unique temporary
# directories. Module processes isolate their unittest mocks and module globals.
# New modules remain serial; changes to these modules require an IO re-review.
READ_ONLY = frozenset(('test_native_content', 'test_native_house',
    'test_native_round', 'test_native_restore', 'test_phone_dialogue',
    'test_dad_record_dialogue',
    # Reviewed after the aggregate exceeded its unchanged 1800-second bound.
    # These suites write only copies/unique temporary directories; see TESTING.
    'test_phone_linker', 'test_native_session', 'test_doll_postwin',
    'test_melody_content', 'test_mayor_scene', 'test_mayor_field',
    'test_house_assets', 'test_new_game_assets'))


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


def registered_modules(command, project):
    if command[1:3] == ['-m', 'unittest']:
        names = command[3:]
        if not names or any(not name.startswith('tests.test_') or
                            not name[6:].isidentifier() for name in names):
            raise ValueError('Unsupported dedicated unittest command')
        modules = tuple(name[6:] for name in names)
        if len(set(modules)) != len(modules):
            raise ValueError('Duplicate module in dedicated unittest command')
        return modules
    if len(command) >= 2:
        path = Path(command[1]).resolve()
        if path.parent == project / 'tests' and path.name.startswith('test_') and path.suffix == '.py':
            if len(command) != 2:
                raise ValueError('Unsupported dedicated unittest script arguments')
            return (path.stem,)
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


def shard_partition(suite, count):
    """Keep whole modules together; preserve every method exactly once."""
    if type(count) is not int or count not in (1, 2, 4):
        raise ValueError('Unsupported aggregate shard count')
    original = list(cases(suite))
    full_ids = Counter(identity(c) for c in original)
    if any(n != 1 for n in full_ids.values()):
        raise ValueError('Duplicate aggregate shard method IDs')
    modules = {}
    for case in original:
        modules.setdefault(identity(case).split('.')[0], []).append(case)
    assigned, loads = {}, [0] * count
    for module, methods in sorted(modules.items(), key=lambda item: (-len(item[1]), item[0])):
        selected = min(range(count), key=lambda i: (loads[i], i))
        assigned[module] = selected
        loads[selected] += len(methods)
    shards = [unittest.TestSuite(c for c in original
        if assigned[identity(c).split('.')[0]] == i) for i in range(count)]
    ids = [sorted(identity(c) for c in cases(part)) for part in shards]
    if sum((Counter(part) for part in ids), Counter()) != full_ids:
        raise ValueError('Aggregate sharding loses or duplicates methods')
    return shards, ids


def validate_shard_registrations(commands, project, directory, count):
    """An omitted, duplicate or changed CTest shard cannot silently pass."""
    if type(count) is not int or count not in (1, 2, 4):
        raise ValueError('Unsupported aggregate shard registration count')
    if type(commands) is not list or any(type(row) is not dict
        or set(row) != {'name', 'command'} or type(row['name']) is not str
        or type(row['command']) is not list
        or any(type(arg) is not str or not arg for arg in row['command']) for row in commands):
        raise ValueError('Malformed aggregate CTest shard registration')
    expected = ['python_tools'] + ['python_tools_part' + str(i + 1) for i in range(1, count)]
    if len(commands) != count or sorted(c['name'] for c in commands) != sorted(expected):
        raise ValueError('Missing/duplicate aggregate CTest shard registration')
    for row in commands:
        index = expected.index(row['name'])
        command = row['command']
        suffix = [] if count == 1 else ['--shard-index', str(index), '--shard-count', str(count)]
        if (len(command) != 4 + len(suffix)
            or Path(command[0]).resolve() != Path(sys.executable).resolve()
            or Path(command[1]).resolve() != Path(project).resolve() / 'tests/python_suite.py'
            or command[2] != '--ctest-dir' or Path(command[3]).resolve() != Path(directory).resolve()
            or command[4:] != suffix):
            raise ValueError('Changed aggregate CTest shard command: ' + row['name'])


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
        executed_ids=executed, success=success, pid=os.getpid(), started=start, seconds=time.monotonic()-start), indent=2)+'\n', encoding='utf-8')
    return 0 if success else 1


def run_parallel(project, groups, directory, jobs):
    if type(jobs) is not int or not (jobs == 1 or 4 <= jobs <= 64):
        raise ValueError('Python test jobs must be 1 (serial) or between 4 and 64')
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
            process = subprocess.Popen([sys.executable, str(Path(__file__).resolve()),
                '--project', str(project), '--worker-plan', str(expected),
                '--worker-result', str(result_path)], cwd=project, stdout=output,
                stderr=subprocess.STDOUT, env=dict(os.environ, PYTHONIOENCODING='utf-8'))
            print('PYTHON START {} pid={}'.format(module, process.pid), flush=True)
            process.wait()
            print('PYTHON END {} pid={} status={}'.format(module, process.pid, process.returncode), flush=True)
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
        results = list(pool.map(run, sorted(groups.items(),
            key=lambda item: (-len(item[1]), item[0]))))
    return all(results)


def plan(project, ctest_dir):
    project = Path(project).resolve()
    if str(project) not in sys.path:
        sys.path.insert(0, str(project))
    metadata = json.loads(subprocess.check_output(
        ['ctest', '--test-dir', str(ctest_dir), '--show-only=json-v1'], text=True))
    observed = {}
    aggregate_commands = []
    for test in metadata['tests']:
        name = test['name']
        command = test.get('command', [])
        if len(command) >= 2 and Path(command[1]).resolve() == project / 'tests/python_suite.py':
            aggregate_commands.append(dict(name=name, command=command))
        registered = registered_modules(command, project)
        if name in DEDICATED:
            if name in observed or registered != DEDICATED[name]:
                raise ValueError('Missing/changed dedicated unittest command: ' + name)
            observed[name] = registered
        elif registered is not None:
            raise ValueError('Unmapped dedicated unittest registration: ' + name)
    if observed != DEDICATED:
        raise ValueError('Missing dedicated CTest registration: ' + ', '.join(sorted(set(DEDICATED) - set(observed))))
    full = list(cases(unittest.defaultTestLoader.discover(str(project / 'tests'), pattern='test_*.py')))
    full_ids = Counter(identity(c) for c in full)
    if any(count != 1 for count in full_ids.values()):
        raise ValueError('Duplicate IDs in unittest discovery')
    modules = {module for group in DEDICATED.values() for module in group}
    remaining = [c for c in full if identity(c).split('.')[0] not in modules]
    remaining_ids = Counter(identity(c) for c in remaining)
    groups = {}
    combined = remaining_ids.copy()
    for name, modules in DEDICATED.items():
        dedicated = list(cases(unittest.defaultTestLoader.loadTestsFromNames(['tests.' + module for module in modules])))
        ids = Counter(identity(c) for c in dedicated)
        expected = Counter({key: count for key, count in full_ids.items() if key.split('.')[0] in modules})
        if not expected or ids != expected:
            raise ValueError('Dedicated suite differs from discovery: ' + name)
        combined.update(ids)
        groups[name] = sorted(ids.elements())
    if combined != full_ids:
        raise ValueError('Python test partition loses or duplicates methods')
    receipt = dict(discovered=len(full), aggregate=len(remaining), dedicated=len(full) - len(remaining),
                   discovery_ids=sorted(full_ids.elements()), aggregate_ids=sorted(remaining_ids.elements()),
                   dedicated_ids=groups, multiset_equal=True, aggregate_commands=aggregate_commands,
                   policy='New unregistered modules stay in discovery; dedicated commands and the complete discovered method multiset are preserved. Assertions remain owned by their suites.')
    return unittest.TestSuite(remaining), receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--ctest-dir', type=Path)
    parser.add_argument('--jobs', type=int, choices=(1, *range(4, 65)), default=int(os.environ.get('ENCORE_PYTHON_TEST_JOBS', '1')))
    parser.add_argument('--worker-plan', type=Path)
    parser.add_argument('--worker-result', type=Path)
    parser.add_argument('--collect-only', action='store_true')
    parser.add_argument('--report', type=Path)
    parser.add_argument('--shard-index', type=int, default=0)
    parser.add_argument('--shard-count', type=int, choices=(1, 2, 4), default=1)
    args = parser.parse_args()
    if args.worker_plan or args.worker_result:
        if not args.worker_plan or not args.worker_result:
            parser.error('worker plan and result must be supplied together')
        return worker(args.project, args.worker_plan, args.worker_result)
    if args.ctest_dir is None:
        parser.error('--ctest-dir is required')
    if not (args.jobs == 1 or 4 <= args.jobs <= 64):
        parser.error('Python test jobs must be 1 (serial) or between 4 and 64')
    suite, receipt = plan(args.project, args.ctest_dir)
    if not 0 <= args.shard_index < args.shard_count:
        parser.error('Aggregate shard index is outside its count')
    # Collection-only remains a global accounting operation. Actual execution
    # also checks that CTest registered every shard with its exact command.
    if not args.collect_only:
        validate_shard_registrations(receipt['aggregate_commands'], args.project,
                                     args.ctest_dir, args.shard_count)
    shards, shard_ids = shard_partition(suite, args.shard_count)
    suite = shards[args.shard_index]
    receipt.update(shard_count=args.shard_count, shard_index=args.shard_index,
                   shard_ids=shard_ids, selected_ids=shard_ids[args.shard_index])
    serial, groups = parallel_partition(suite) if args.jobs >= 4 else (suite, {})
    receipt['parallel_modules'] = groups
    receipt['serial_ids'] = sorted(identity(c) for c in cases(serial))
    print('Python method partition: {discovered} discovered = {aggregate} aggregate + {dedicated} dedicated; exact ID multiset preserved'.format(**receipt), flush=True)
    print('Aggregate shard {}/{}: {} methods; whole modules and every original assertion retained'.format(
        args.shard_index + 1, args.shard_count, len(receipt['selected_ids'])), flush=True)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf8')
    if args.collect_only:
        return 0
    # Writers/unreviewed modules run only after every read-only worker exits.
    parallel_ok = run_parallel(args.project.resolve(), groups,
        args.ctest_dir.resolve() / 'python-workers' / ('shard-' + str(args.shard_index)), args.jobs) if groups else True
    serial_ok = unittest.TextTestRunner(verbosity=2).run(serial).wasSuccessful()
    return 0 if parallel_ok and serial_ok else 1


if __name__ == '__main__':
    raise SystemExit(main())
