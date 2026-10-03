import copy
import json
from pathlib import Path
import unittest
from tools.reference_actor_actions import fixture,REVIEW,ROOT

class ActorActionReferenceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.review=json.loads(REVIEW.read_text())
        cls.reference=json.loads((ROOT/'reports/actor-actions-reference-9/reference.json').read_text())
    def test_regeneration(self):
        self.assertEqual(fixture(self.reference,self.review),(ROOT/'tests/fixtures/actor_actions_v0410.hpp').read_text())
    def reject(self,edit):
        reference=copy.deepcopy(self.reference)
        edit(reference)
        with self.assertRaises(ValueError): fixture(reference,self.review)
    def test_wrong_schema(self):self.reject(lambda d:d.update(schema=2))
    def test_changed_source(self):self.reject(lambda d:d['sources'].update({'Scripts/Main/actor.gd':'0'*64}))
    def test_changed_function(self):self.reject(lambda d:d['symbols'].update(jump='0'*64))
    def test_wrong_engine(self):self.reject(lambda d:d['godot'].update(string='4.0'))
    def test_missing_case(self):self.reject(lambda d:d['cases'].pop())
    def test_missing_frame(self):self.reject(lambda d:d['cases'][0]['frames'].pop())
    def test_unknown_command(self):self.reject(lambda d:d['cases'][0]['definition']['actions'][0].update(op='teleport'))
    def test_nonfinite(self):self.reject(lambda d:d['cases'][0]['frames'][0].update(position=[float('nan'),0]))
    def test_unknown_property(self):self.reject(lambda d:d['cases'][0]['frames'][0].update(unsupported=True))
    def test_bad_frame(self):self.reject(lambda d:d['cases'][0]['frames'][0].update(frame=4))
    def test_wrong_boolean_type(self):self.reject(lambda d:d['cases'][0]['frames'][0].update(moving=1))

if __name__=='__main__':unittest.main()
