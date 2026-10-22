"""Check automatic console artifacts and explicit manual-only tests."""
from pathlib import Path
import unittest
from types import SimpleNamespace
import yaml


def matches(condition, event, action='', mode='', label='', matrix=None):
    github = SimpleNamespace(event_name=event, event=SimpleNamespace(action=action, label=SimpleNamespace(name=label)))
    inputs = SimpleNamespace(mode=mode)
    # The reviewed conditions use this deliberately small expression subset.
    code = ' '.join(condition.replace('&&', ' and ').replace('||', ' or ').split())
    return bool(eval(code, {'__builtins__': {}}, {'github': github, 'inputs': inputs, 'matrix': SimpleNamespace(**(matrix or {}))}))


class WorkflowPolicyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.workflow = yaml.safe_load((Path(__file__).resolve().parents[1] / '.github/workflows/build.yml').read_text(encoding='utf-8'))

    def test_main_build_and_manual_modes_are_registered(self):
        # YAML 1.1 parsers may treat the unquoted GitHub key `on` as True.
        triggers = self.workflow.get('on', self.workflow.get(True))
        self.assertEqual(set(triggers), {'push', 'workflow_dispatch'})
        self.assertIsNone(triggers['push'])  # Every branch push publishes a genuine build.
        mode = triggers['workflow_dispatch']['inputs']['mode']
        self.assertEqual(mode['type'], 'choice')
        self.assertEqual(mode['default'], 'build')
        self.assertEqual(mode['options'], ['build', 'full', 'basement-assets'])
        self.assertEqual(set(self.workflow['jobs']), {'host', 'console', 'basement-assets'})

    def test_only_explicit_full_mode_admits_tests(self):
        for event in ('push', 'pull_request', 'schedule', 'repository_dispatch', 'workflow_dispatch'):
            for mode in ('', 'build', 'full', 'basement-assets'):
                with self.subTest(event=event, mode=mode):
                    full = event == 'workflow_dispatch' and mode == 'full'
                    self.assertEqual(matches(self.workflow['jobs']['host']['if'], event, mode=mode), full)
                    self.assertEqual(matches(self.workflow['jobs']['console']['if'], event, mode=mode),
                                     event == 'push' or (event == 'workflow_dispatch' and mode != 'basement-assets'))
                    self.assertEqual(matches(self.workflow['jobs']['basement-assets']['if'], event, mode=mode),
                                     event == 'workflow_dispatch' and mode == 'basement-assets')
                    step = next(s for s in self.workflow['jobs']['console']['steps']
                                if s.get('run') == 'python tests/test_ci_console_check.py')
                    self.assertEqual(matches(step['if'], event, mode=mode), full)

    def test_successful_console_outputs_are_uploaded_without_canceling_other_commits(self):
        steps = self.workflow['jobs']['console']['steps']
        upload = next(s for s in steps if s.get('uses') == 'actions/upload-artifact@v4')
        self.assertNotIn('if', upload)  # Default success() blocks incomplete outputs.
        self.assertEqual(upload['with']['path'], 'dist/sd/')
        self.assertEqual(upload['with']['if-no-files-found'], 'error')
        self.assertIn('${{ github.sha }}', upload['with']['name'])
        check_index = next(i for i, s in enumerate(steps) if s.get('run') == 'python tools/ci_console_check.py')
        self.assertLess(check_index, steps.index(upload))
        self.assertIn('${{ github.sha }}', self.workflow['concurrency']['group'])
        self.assertFalse(self.workflow['concurrency']['cancel-in-progress'])

    def test_downloadable_builds_enable_existing_pillow_texture_backend(self):
        console = self.workflow['jobs']['console']
        self.assertIn('GPU 1:1:1', console['name'])
        runs = [step.get('run', '') for step in console['steps']]
        builds = [run for run in runs if 'make 3dsx ' in run or 'make cia ' in run]
        self.assertEqual(len(builds), 2)
        for run in builds:
            self.assertIn('EXPERIMENTAL_GPU_BACKGROUND=1', run)
            self.assertIn('EXPERIMENTAL_GPU_CERTIFICATES=1', run)
            self.assertIn('EXPERIMENTAL_GPU_TEXTURE_STRIPS=1', run)
        self.assertIn('texture_strips=1', '\n'.join(runs))

    def test_full_checks_remain_complete(self):
        self.assertEqual({row['compiler'] for row in self.workflow['jobs']['host']['strategy']['matrix']['include']}, {'gcc', 'clang'})
        host_runs = '\n'.join(step.get('run', '') for step in self.workflow['jobs']['host']['steps'])
        console_runs = '\n'.join(step.get('run', '') for step in self.workflow['jobs']['console']['steps'])
        self.assertIn('make test', host_runs)
        self.assertIn('make sanitize', host_runs)
        self.assertIn('make 3dsx', console_runs)
        self.assertIn('make cia', console_runs)
        self.assertIn('ci_console_check.py', console_runs)


    def test_each_compiler_runs_each_complete_phase_once(self):
        host = self.workflow['jobs']['host']
        rows = host['strategy']['matrix']['include']
        self.assertEqual(len(rows), 4)
        self.assertEqual({(r['compiler'], r['phase']) for r in rows},
                         {(c, p) for c in ('gcc', 'clang') for p in ('ordinary', 'sanitizer')})
        self.assertFalse(host['strategy']['fail-fast'])
        self.assertEqual(host['timeout-minutes'], 180)
        for row in rows:
            with self.subTest(row=row):
                active = [s for s in host['steps'] if 'run' in s and
                          ('if' not in s or matches(s['if'], 'workflow_dispatch', mode='full', matrix=row))]
                runs = [s['run'] for s in active]
                self.assertEqual(runs.count('make content'), 1)
                self.assertEqual(runs.count('python tools/ci_bootstrap.py'), 1)
                ordinary = [v for v in runs if v.startswith('make test ')]
                sanitizer = [v for v in runs if v.startswith('make sanitize ')]
                expected = row['phase'] == 'ordinary'
                self.assertEqual(len(ordinary), int(expected))
                self.assertEqual(len(sanitizer), int(not expected))
                selected = (ordinary or sanitizer)[0]
                self.assertIn('--parallel 4 --no-tests=error', selected)
                self.assertIn('-DENCORE_TEST_PARALLEL=ON', selected)
                self.assertIn('-DENCORE_REGENERATE_NATIVE_CONTENT=OFF', selected)
                self.assertEqual(row['ctest_args'], '' if row['compiler'] == 'gcc' else '--label-exclude python_unit')
                if expected:
                    self.assertIn('${{ matrix.ctest_args }}', selected)
                    self.assertIn('build/host', '\n'.join(runs))
                    self.assertNotIn('build/sanitize', '\n'.join(runs))
                else:
                    self.assertIn('--label-exclude python_unit', selected)
                    self.assertIn('-DENCORE_SANITIZERS=ON', '\n'.join(runs))
                    self.assertNotIn('build/host', '\n'.join(runs))
                extra = 'python -m unittest -v tests.test_loading_texture tests.test_loading_indicator'
                self.assertEqual(runs.count(extra), int(expected and row['compiler'] == 'clang'))
                self.assertIn('git diff --exit-code', runs)
        self.assertNotIn('ASAN_OPTIONS', host['env'])
        self.assertNotIn('UBSAN_OPTIONS', host['env'])

    def test_independent_phases_keep_unique_failure_diagnostics(self):
        host = self.workflow['jobs']['host']
        self.assertNotIn('needs', host)
        upload = next(s for s in host['steps'] if s.get('uses') == 'actions/upload-artifact@v4')
        self.assertEqual(upload['if'], 'always()')
        template = upload['with']['name']
        names = [template.replace('${{ matrix.compiler }}', r['compiler']).replace('${{ matrix.phase }}', r['phase'])
                 for r in host['strategy']['matrix']['include']]
        self.assertEqual(len(set(names)), 4)
        for path in ('build/collision-ci/', 'build/host/Testing/Temporary/LastTest.log',
                     'build/sanitize/Testing/Temporary/LastTest.log', 'build/host/python-workers/'):
            self.assertIn(path, upload['with']['path'])


if __name__ == '__main__':
    unittest.main()
