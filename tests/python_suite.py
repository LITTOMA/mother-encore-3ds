#!/usr/bin/env python3
"""Run discovery cases once, preserving separately registered CTest suites."""
import argparse
from collections import Counter
import json
from pathlib import Path
import subprocess
import sys
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
    parser.add_argument('--ctest-dir', type=Path, required=True)
    parser.add_argument('--collect-only', action='store_true')
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    suite, receipt = plan(args.project, args.ctest_dir)
    print('Python method partition: {discovered} discovered = {aggregate} aggregate + {dedicated} dedicated; exact ID multiset preserved'.format(**receipt), flush=True)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf8')
    if args.collect_only:
        return 0
    return 0 if unittest.TextTestRunner(verbosity=2).run(suite).wasSuccessful() else 1


if __name__ == '__main__':
    raise SystemExit(main())
