"""Exercise the actual workflow conditions for quick and full run routing."""
from pathlib import Path
import unittest
from types import SimpleNamespace
import yaml


def matches(condition, event, action='', suite='', label=''):
    github = SimpleNamespace(event_name=event, event=SimpleNamespace(action=action, label=SimpleNamespace(name=label)))
    inputs = SimpleNamespace(suite=suite)
    # The reviewed conditions use this deliberately small expression subset.
    code = ' '.join(condition.replace('&&', ' and ').replace('||', ' or ').split())
    return bool(eval(code, {'__builtins__': {}}, {'github': github, 'inputs': inputs}))


class WorkflowPolicyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.workflow = yaml.safe_load((Path(__file__).resolve().parents[1] / '.github/workflows/build.yml').read_text(encoding='utf-8'))

    def test_actual_conditions_separate_quick_and_full_events(self):
        for event, action, suite, label, full, quick in (
            ('pull_request', 'opened', '', '', False, True),
            ('pull_request', 'synchronize', '', '', False, True),
            ('pull_request', 'reopened', '', '', False, True),
            ('pull_request', 'ready_for_review', '', '', True, False),
            ('pull_request', 'labeled', '', 'ci/full', True, False),
            ('pull_request', 'labeled', '', 'bug', False, False),
            ('workflow_dispatch', '', 'full', '', True, False),
            ('workflow_dispatch', '', 'quick', '', False, True),
            ('push', '', '', '', True, False),
        ):
            with self.subTest(event=event, action=action, suite=suite, label=label):
                args = (event, action, suite, label)
                self.assertEqual(matches(self.workflow['jobs']['quick']['if'], *args), quick)
                for name in ('host', 'console'):
                    self.assertEqual(matches(self.workflow['jobs'][name]['if'], *args), full)

    def test_quick_uses_checked_production_selection_not_full_make_test(self):
        runs = '\n'.join(step.get('run', '') for step in self.workflow['jobs']['quick']['steps'])
        for required in ('ci_bootstrap.py', 'make content', 'git diff --exit-code', 'tools/ci_quick.py'):
            self.assertIn(required, runs)
        self.assertNotIn('make test', runs)
        self.assertNotIn('make sanitize', runs)

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
