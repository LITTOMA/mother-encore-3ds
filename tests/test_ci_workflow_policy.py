"""Check manual-only admission and preservation of comprehensive verification."""
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

    def test_only_manual_trigger_is_registered(self):
        # YAML 1.1 parsers may treat the unquoted GitHub key `on` as True.
        triggers = self.workflow.get('on', self.workflow.get(True))
        self.assertEqual(triggers, {'workflow_dispatch': None})
        self.assertEqual(set(self.workflow['jobs']), {'host', 'console'})

    def test_automatic_events_cannot_admit_jobs(self):
        for event, action, label in (
            ('push', '', ''), ('pull_request', 'opened', ''),
            ('pull_request', 'synchronize', ''), ('pull_request', 'reopened', ''),
            ('pull_request', 'ready_for_review', ''),
            ('pull_request', 'labeled', 'ci/full'), ('schedule', '', ''),
            ('repository_dispatch', '', ''), ('workflow_dispatch', '', ''),
        ):
            with self.subTest(event=event, action=action, label=label):
                for name in ('host', 'console'):
                    self.assertEqual(matches(self.workflow['jobs'][name]['if'],
                        event, action, label=label), event == 'workflow_dispatch')

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
