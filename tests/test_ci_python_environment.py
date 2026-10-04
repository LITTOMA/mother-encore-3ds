"""The faster CI parser remains safe and requires explicit activation."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
FAST = ROOT / 'tools/ci_python'


def probe(code, paths=()):
    env = dict(os.environ)
    env.pop('PYTHONPATH', None)
    if paths:
        env['PYTHONPATH'] = os.pathsep.join(str(p) for p in paths)
    return subprocess.run([sys.executable, '-c', code], cwd=ROOT, env=env,
                          capture_output=True, text=True)


class CiPythonEnvironmentTests(unittest.TestCase):
    def test_activation_is_explicit(self):
        code = 'import yaml; print(yaml.SafeLoader.__name__)'
        normal = probe(code)
        fast = probe(code, (FAST,))
        self.assertEqual(normal.returncode, 0, normal.stderr)
        self.assertEqual(fast.returncode, 0, fast.stderr)
        self.assertEqual(normal.stdout.strip(), 'SafeLoader')
        self.assertEqual(fast.stdout.strip(), 'CSafeLoader')

    def test_order_values_and_types_match(self):
        code = ('import yaml,json; v=yaml.safe_load(' +
                repr('alpha: [1, 2.5, true, null, "text"]\nbeta: {first: 0, last: false}\n') +
                '); print(json.dumps([list(v),v,[type(x).__name__ for x in v["alpha"]]]))')
        normal = probe(code)
        fast = probe(code, (FAST,))
        self.assertEqual(normal.returncode, 0, normal.stderr)
        self.assertEqual(fast.returncode, 0, fast.stderr)
        self.assertEqual(json.loads(normal.stdout), json.loads(fast.stdout))

    def test_unknown_unsafe_and_malformed_inputs_rejected(self):
        for source in ('!Unknown {}', '!!python/object:builtins.object {}',
                       '!!python/object/apply:os.system [echo forbidden]',
                       'value: [1,', '*undefined', '---\na: 1\n---\nb: 2'):
            with self.subTest(source=source):
                code = 'import yaml; yaml.safe_load(' + repr(source) + ')'
                self.assertNotEqual(probe(code).returncode, 0)
                self.assertNotEqual(probe(code, (FAST,)).returncode, 0)

    def test_missing_or_wrong_parser_fails_before_script_execution(self):
        with tempfile.TemporaryDirectory() as directory:
            fake = Path(directory) / 'yaml.py'
            for version, compiled in (('6.0.2', True), ('6.0.3', False)):
                with self.subTest(version=version, compiled=compiled):
                    fake.write_text('__version__=' + repr(version) +
                                    '\n__with_libyaml__=' + repr(compiled) + '\n', encoding='utf-8')
                    result = probe('print("SCRIPT_EXECUTED")', (FAST, Path(directory)))
                    self.assertNotEqual(result.returncode, 0)
                    self.assertNotIn('SCRIPT_EXECUTED', result.stdout)
                    self.assertIn('CI safe parser requires pinned PyYAML', result.stderr)


if __name__ == '__main__':
    unittest.main()
