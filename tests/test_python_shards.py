"""Exact coverage and actual execution gates for bounded aggregate shards."""
from collections import Counter
import copy,json,sys,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
from tests import python_suite as runner

ROOT=Path(__file__).resolve().parents[1]

class Named(unittest.TestCase):
    def __init__(self,name):super().__init__();self.name=name
    def id(self):return self.name

class PythonShards(unittest.TestCase):
    def make_suite(self):
        return unittest.TestSuite(Named('tests.'+name+'.Cases.test_'+str(i))
            for name,size in [('test_native_content',7),('test_a',6),('test_future_writer',3),('test_z',2)] for i in range(size))
    def commands(self,project=ROOT,directory=ROOT/'build'):
        return [dict(name='python_tools'if i==0 else'python_tools_part2',
            command=[sys.executable,str(project/'tests/python_suite.py'),'--ctest-dir',str(directory),'--shard-index',str(i),'--shard-count','2'])for i in range(2)]
    def test_exact_multiset_whole_modules_order_and_determinism(self):
        suite=self.make_suite();original=[runner.identity(c)for c in runner.cases(suite)];parts,ids=runner.shard_partition(suite,2)
        self.assertEqual(sum(map(Counter,ids),Counter()),Counter(original));self.assertEqual(sum(map(len,ids)),18)
        owners={}
        for index,part in enumerate(parts):
            actual=[runner.identity(c)for c in runner.cases(part)]
            self.assertEqual(actual,[s for s in original if s in ids[index]])
            for ident in actual:owners.setdefault(ident.split('.')[0],set()).add(index)
        self.assertTrue(all(len(v)==1 for v in owners.values()));self.assertEqual(runner.shard_partition(self.make_suite(),2)[1],ids)
        self.assertIn('test_future_writer',owners)
    def test_legacy_one_empty_and_invalid_counts_or_duplicate_ids(self):
        parts,ids=runner.shard_partition(self.make_suite(),1);self.assertEqual(len(parts),1);self.assertEqual(len(ids[0]),18)
        self.assertEqual(runner.shard_partition(unittest.TestSuite(),2)[1],[[],[]])
        for count in (0,3,True,2.0,'2'):
            with self.subTest(count=count),self.assertRaises(ValueError):runner.shard_partition(self.make_suite(),count)
        with self.assertRaises(ValueError):runner.shard_partition(unittest.TestSuite([Named('test_a.C.test'),Named('tests.test_a.C.test')]),2)
    def test_exact_actual_ctest_registration_and_legacy_command(self):
        runner.validate_shard_registrations(self.commands(),ROOT,ROOT/'build',2)
        legacy=[dict(name='python_tools',command=[sys.executable,str(ROOT/'tests/python_suite.py'),'--ctest-dir',str(ROOT/'build')])]
        runner.validate_shard_registrations(legacy,ROOT,ROOT/'build',1)
    def test_missing_duplicate_unknown_changed_shards_and_arguments_reject(self):
        edits=[lambda c:c.pop(),lambda c:c.append(c[0]),lambda c:c[1].update(name='python_tools'),lambda c:c[1].update(name='unknown'),lambda c:c[1]['command'].__setitem__(-3,'0'),lambda c:c[0]['command'].__setitem__(-1,'1'),lambda c:c[0]['command'].append('--unknown'),lambda c:c[0]['command'].__setitem__(1,str(ROOT/'tools/python_suite.py')),lambda c:c[0]['command'].__setitem__(3,str(ROOT/'other')),lambda c:c[0].update(extra=1),lambda c:c[0].update(command=None)]
        for edit in edits:
            commands=self.commands();edit(commands)
            with self.subTest(edit=edit),self.assertRaises(ValueError):runner.validate_shard_registrations(commands,ROOT,ROOT/'build',2)
    def test_actual_two_shard_execution_includes_unreviewed_module_once(self):
        with tempfile.TemporaryDirectory()as directory:
            project=Path(directory);tests=project/'tests';tests.mkdir();(tests/'__init__.py').write_text('',encoding='utf8');build=project/'build';build.mkdir()
            names=['test_shard_demo_alpha','test_shard_demo_future_writer']
            for name in names:
                (tests/(name+'.py')).write_text('from pathlib import Path\nimport unittest\nclass Case(unittest.TestCase):\n def test_run(self):\n  p=Path(__file__).resolve().parents[1]/"'+name+'.executed"\n  with p.open("a")as f:f.write("executed\\n")\n',encoding='utf8')
            metadata=json.dumps(dict(tests=self.commands(project,build)))
            with patch.object(runner,'DEDICATED',{}),patch.object(runner.unittest,'defaultTestLoader',unittest.TestLoader()),patch.object(runner.subprocess,'check_output',return_value=metadata):
                for index in range(2):
                    with patch.object(sys,'argv',['python_suite.py','--project',str(project),'--ctest-dir',str(build),'--shard-index',str(index),'--shard-count','2']):self.assertEqual(runner.main(),0)
            for name in names:self.assertEqual((project/(name+'.executed')).read_text().splitlines(),['executed'])

if __name__=='__main__':unittest.main()
