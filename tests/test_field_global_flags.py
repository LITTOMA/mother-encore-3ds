"""Manual generator negative cases. Not registered with automatic CI."""
import copy, unittest
from tools import field_global_flags as flags

class SourceFlags(unittest.TestCase):
    def test_reviewed(self):
        d=flags.load()
        self.assertEqual(len(d['registered']),178)
        self.assertEqual(flags.PACK.read_bytes(),flags.encode(d))

    def test_unknown_source_policy(self):
        for field, value in [('schema',2),('family',0),('commit','0'*40),('owner','unknown')]:
            d=copy.deepcopy(flags.load());d[field]=value
            with self.assertRaises(ValueError):flags.validate(d)
        d=copy.deepcopy(flags.load());d['policy']['normal_set']='insert'
        with self.assertRaises(ValueError):flags.validate(d)

    def test_duplicate_and_nonbool(self):
        d=copy.deepcopy(flags.load());d['registered'].append(d['registered'][0])
        with self.assertRaises(ValueError):flags.validate(d)
        d=copy.deepcopy(flags.load());d['profiles'][0]['normal'][0]=1
        with self.assertRaises(ValueError):flags.validate(d)
        d=copy.deepcopy(flags.load());d['profiles'][0]['objects']=[['a',True],['a',False]]
        with self.assertRaises(ValueError):flags.validate(d)

if __name__=='__main__':unittest.main()
