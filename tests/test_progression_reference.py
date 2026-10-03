import copy
import hashlib
from pathlib import Path
import unittest
from tools.reference_progression import extract, fixture

class ProgressionReferenceTests(unittest.TestCase):
    def setUp(self):
        self.raw=(b'extends Character\nconst LEVEL_CAP := 30\n'
            b'static func _level_to_exp(level: int) -> int:\n\treturn level\n\n'
            b'static func _exp_to_level(xp: int) -> int:\n\treturn xp\n')
        self.review={'schema':1,'game_version':'0.4.1.0','commit':'a'*40,'level_cap':30,
            'source_sha256':hashlib.sha256(self.raw).hexdigest(),'symbols':['_level_to_exp','_exp_to_level']}
    def test_extract_preserves_audited_functions(self):
        code,hashes=extract(self.raw,self.review)
        self.assertTrue(code.startswith('extends Reference\n'))
        self.assertIn('static func _level_to_exp(level: int) -> int:\n\treturn level\n',code)
        self.assertEqual(set(hashes),set(self.review['symbols']))
    def test_changed_source_requires_review(self):
        with self.assertRaisesRegex(ValueError,'source changed'):extract(self.raw+b'\n',self.review)
    def test_unknown_schema_version_cap_symbol_rejected(self):
        for key,value in [('schema',2),('game_version','0.4.2.0'),('level_cap',99),('symbols',['other'])]:
            review=copy.deepcopy(self.review);review[key]=value
            with self.assertRaises(ValueError):extract(self.raw,review)
    def test_missing_or_duplicate_symbol_rejected(self):
        for raw in [self.raw.replace(b'_exp_to_level',b'_unknown'),self.raw+self.raw]:
            review=dict(self.review,source_sha256=hashlib.sha256(raw).hexdigest())
            with self.assertRaises(ValueError):extract(raw,review)
    def test_missing_constant_rejected(self):
        raw=self.raw.replace(b'LEVEL_CAP := 30',b'LEVEL_CAP := 31')
        review=dict(self.review,source_sha256=hashlib.sha256(raw).hexdigest())
        with self.assertRaises(ValueError):extract(raw,review)
    def test_wrong_reference_engine_or_provenance_rejected(self):
        document={k:self.review[k] for k in ('schema','commit','game_version','source_sha256')}
        document['godot']={'major':4,'minor':6,'patch':0,'status':'stable'}
        with self.assertRaisesRegex(ValueError,'Godot'):fixture(document,self.review)
        document['commit']='b'*40
        with self.assertRaisesRegex(ValueError,'provenance'):fixture(document,self.review)
    def test_incomplete_reference_domain_rejected(self):
        document={k:self.review[k] for k in ('schema','commit','game_version','source_sha256')}
        document['godot']={'major':3,'minor':6,'patch':2,'status':'stable'}
        document['level_cases']=[]
        with self.assertRaisesRegex(ValueError,'level domain'):fixture(document,self.review)
        document['level_cases']=[[i,0] for i in range(1,61)]+[[2147483647,0]]
        document['xp_levels']=[1]*20926
        with self.assertRaisesRegex(ValueError,'XP domain'):fixture(document,self.review)
        document['xp_levels']=[31]*20927
        with self.assertRaisesRegex(ValueError,'XP domain'):fixture(document,self.review)

if __name__=='__main__':unittest.main()
