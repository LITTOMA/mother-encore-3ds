"""Check automatic console artifacts and explicit manual-only tests."""
from pathlib import Path
import unittest
from types import SimpleNamespace
import yaml


def matches(condition, event, action='', mode='', label=''):
    github = SimpleNamespace(event_name=event, event=SimpleNamespace(action=action, label=SimpleNamespace(name=label)))
    inputs = SimpleNamespace(mode=mode)
    # The reviewed conditions use this deliberately small expression subset.
    code = ' '.join(condition.replace('&&', ' and ').replace('||', ' or ').split())
    return bool(eval(code, {'__builtins__': {}}, {'github': github, 'inputs': inputs}))


class WorkflowPolicyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.workflow = yaml.safe_load((Path(__file__).resolve().parents[1] / '.github/workflows/build.yml').read_text(encoding='utf-8'))

    def test_main_build_and_manual_modes_are_registered(self):
        # YAML 1.1 parsers may treat the unquoted GitHub key `on` as True.
        triggers = self.workflow.get('on', self.workflow.get(True))
        self.assertEqual(set(triggers), {'push', 'workflow_dispatch'})
        self.assertEqual(triggers['push'], {'branches': ['main']})
        mode = triggers['workflow_dispatch']['inputs']['mode']
        self.assertEqual(mode['type'], 'choice')
        self.assertEqual(mode['default'], 'build')
        self.assertEqual(mode['options'], ['build', 'full'])
        self.assertEqual(set(self.workflow['jobs']), {'host', 'console'})

    def test_only_explicit_full_mode_admits_tests(self):
        for event in ('push', 'pull_request', 'schedule', 'repository_dispatch', 'workflow_dispatch'):
            for mode in ('', 'build', 'full'):
                with self.subTest(event=event, mode=mode):
                    full = event == 'workflow_dispatch' and mode == 'full'
                    self.assertEqual(matches(self.workflow['jobs']['host']['if'], event, mode=mode), full)
                    self.assertEqual(matches(self.workflow['jobs']['console']['if'], event, mode=mode),
                                     event in ('push', 'workflow_dispatch'))
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

    def test_full_checks_remain_complete(self):
        self.assertEqual({row['compiler'] for row in self.workflow['jobs']['host']['strategy']['matrix']['include']}, {'gcc', 'clang'})
        host_runs = '\n'.join(step.get('run', '') for step in self.workflow['jobs']['host']['steps'])
        console_runs = '\n'.join(step.get('run', '') for step in self.workflow['jobs']['console']['steps'])
        self.assertIn('make test', host_runs)
        self.assertIn('make sanitize', host_runs)
        self.assertIn('make 3dsx', console_runs)
        self.assertIn('make cia', console_runs)
        self.assertIn('ci_console_check.py', console_runs)


if __name__ == '__main__':
    unittest.main()
