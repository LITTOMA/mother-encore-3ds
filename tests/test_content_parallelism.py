"""Exercise GNU make scheduling and failure gates with independent processes."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
MAKE = shutil.which('gmake') or shutil.which('make')


class ContentParallelTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    @unittest.skipUnless(MAKE, 'GNU make unavailable')
    def test_four_process_overlap_dependencies_and_final_barrier(self):
        self.run_graph(False)

    @unittest.skipUnless(MAKE, 'GNU make unavailable')
    def test_failure_blocks_room_and_final_fingerprints(self):
        self.run_graph(True)

    def run_graph(self, fail):
        fake = self.root / 'task.py'
        fake.write_text('''import json, os, sys, time
from pathlib import Path
task = sys.argv[1]
deps = {'room': ['audio', 'effects', 'doll-entry', 'pillow-entry'],
        'battle': ['room'], 'restore': ['room', 'house'],
        'items': ['items-check'], 'locale': ['migration'], 'settings-check': ['settings']}
producers = 'audio bars input phone effects doll-entry pillow-entry room battle round doll-round pillow-round house items-check items session migration restore continue loading naming settings settings-check prompts locale'.split()
if task in ('catalog', 'encounters'): deps[task] = producers
for dependency in deps.get(task, []):
 assert Path(dependency + '.done').exists(), (task, dependency)
Path(task + '.started').write_text(str(os.getpid()))
if task in ('audio', 'bars', 'input', 'phone'):
 deadline = time.monotonic() + 10
 while not all(Path(n + '.started').exists() for n in ('audio', 'bars', 'input', 'phone')):
  assert time.monotonic() < deadline, 'four workers failed to overlap'
  time.sleep(.01)
if task == 'effects' and os.environ['FAIL_EFFECTS'] == '1': sys.exit(7)
time.sleep(.02)
assert not Path(task + '.done').exists(), 'duplicate execution'
Path(task + '.done').write_text(json.dumps(sys.argv[3:]))
''', encoding='utf-8')
        command = '"{}" "{}"'.format(sys.executable, fake)
        result = subprocess.run([MAKE, '--no-print-directory', '-f',
            str(ROOT / 'make/native-content.mk'), '-j4', 'CONTENT_RUNNER=' + command,
            'native-content'], cwd=self.root, env=dict(os.environ, FAIL_EFFECTS=str(int(fail))),
            capture_output=True, text=True, timeout=30)
        if fail:
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
            for task in ('room', 'battle', 'restore', 'catalog', 'encounters'):
                self.assertFalse((self.root / (task + '.started')).exists(), task)
        else:
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(len(list(self.root.glob('*.done'))), 27)
            for task in ('catalog', 'encounters'):
                self.assertTrue((self.root / (task + '.done')).exists())
        pids = [(self.root / (n + '.started')).read_text()
                for n in ('audio', 'bars', 'input', 'phone')]
        self.assertEqual(len(set(pids)), 4)

    def test_task_failure_replaces_stale_success_and_retains_log(self):
        directory = self.root / 'logs'
        directory.mkdir()
        receipt = directory / 'probe.json'
        receipt.write_text('{"returncode":0}')
        result = subprocess.run([sys.executable, str(ROOT / 'tools/run_content_task.py'),
            'probe', '--', sys.executable, '-c', 'print("actual failure"); raise SystemExit(7)'],
            env=dict(os.environ, ENCORE_CONTENT_LOG_DIR=str(directory)), capture_output=True)
        self.assertEqual(result.returncode, 7)
        record = json.loads(receipt.read_text())
        self.assertEqual(record['returncode'], 7)
        self.assertGreater(record['pid'], 0)
        self.assertGreaterEqual(record['finished'], record['started'])
        self.assertIn('actual failure', (directory / 'probe.log').read_text())

    def test_invalid_task_does_not_launch_command(self):
        marker = self.root / 'executed'
        for task in ('../escape', 'probe/escape', ''):
            with self.subTest(task=task):
                result = subprocess.run([sys.executable, str(ROOT / 'tools/run_content_task.py'),
                    task, '--', sys.executable, '-c',
                    'from pathlib import Path; Path({!r}).touch()'.format(str(marker))], capture_output=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse(marker.exists())


if __name__ == '__main__':
    unittest.main()
