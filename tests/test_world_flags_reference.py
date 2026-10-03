import copy
import json
from pathlib import Path
import tempfile
import unittest
from tools import reference_world_flags as reference

ROOT=Path(__file__).resolve().parents[1]
class WorldFlagReferenceTests(unittest.TestCase):
    def setUp(self):
        self.data=json.loads((ROOT/'reports/m4-world-flags-reference-final/world-flags.json').read_text())
    def test_native_fixture_is_unchanged(self):
        self.assertEqual(reference.fixture(self.data),(ROOT/'tests/fixtures/world_flags_v0410.hpp').read_text())
    def test_reject_incomplete_and_unknown_reference_fields(self):
        for operation in ['missing','extra','short']:
            data=copy.deepcopy(self.data)
            if operation=='missing': del data['appearance']
            elif operation=='extra': data['unsupported']=[]
            else: data['appearance'].pop()
            with self.assertRaises(ValueError): reference.fixture(data)
    def test_reject_wrong_variant_types_and_ranges(self):
        for section,index,value in [('appearance',0,-1),('doors',0,16),('npc',0,0),('writes',6,2),('objects',5,['a','b'])]:
            data=copy.deepcopy(self.data);data[section][0][index]=value
            with self.assertRaises(ValueError): reference.fixture(data)
        data=copy.deepcopy(self.data);data['flag_names'][1]=data['flag_names'][0]
        with self.assertRaises(ValueError): reference.fixture(data)
    def test_reject_changed_source_before_writing_project(self):
        with tempfile.TemporaryDirectory() as directory:
            source=Path(directory)/'source';path=source/'Scripts/global/globalData.gd'
            path.parent.mkdir(parents=True);path.write_text('extends Node\n')
            work=Path(directory)/'work'
            with self.assertRaisesRegex(ValueError,'Changed unreviewed source'):
                reference.prepare(source,work)
            self.assertFalse(work.exists())
    def test_reject_missing_reviewed_function(self):
        with self.assertRaisesRegex(ValueError,'Missing reviewed function'):
            reference.functions('extends Node\n',['check_appear_disappear_flags'])
if __name__=='__main__': unittest.main()
