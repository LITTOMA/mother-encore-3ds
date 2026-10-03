import copy
import importlib.util
import json
from pathlib import Path
import struct
import unittest
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('content_compiler', ROOT/'tools/content_compiler.py')
compiler=importlib.util.module_from_spec(spec);spec.loader.exec_module(compiler)

class ContentTests(unittest.TestCase):
    def setUp(self):self.data=json.loads((ROOT/'content/sandbox.json').read_text())
    def test_deterministic(self):
        a,ma=compiler.compile_content(self.data);b,mb=compiler.compile_content(copy.deepcopy(self.data))
        self.assertEqual(a,b);self.assertEqual(ma,mb);self.assertEqual(len(a)-64,struct.unpack_from('<I',a,16)[0])
    def test_unknown_opcode_rejected(self):
        self.data['programs'][0]['code'][1]['op']='EXEC_GDSCRIPT'
        with self.assertRaises(compiler.ContentError):compiler.compile_content(self.data)
    def test_unknown_fields_rejected(self):
        self.data['maps'][0]['physics_mode']='guess'
        with self.assertRaises(compiler.ContentError):compiler.compile_content(self.data)
    def test_missing_binding_rejected(self):
        self.data['maps'][0]['objects'][0]['program']='unimplemented'
        with self.assertRaises(compiler.ContentError):compiler.compile_content(self.data)
    def test_duplicate_stable_id_rejected(self):
        self.data['maps'][1]['id']=self.data['maps'][0]['id']
        with self.assertRaises(compiler.ContentError):compiler.compile_content(self.data)
    def test_bad_jump_rejected(self):
        self.data['programs'][0]['code'][0]['target']='missing'
        with self.assertRaises(compiler.ContentError):compiler.compile_content(self.data)
    def test_blocked_spawn_rejected(self):
        self.data['start']['x']=0
        with self.assertRaises(compiler.ContentError):compiler.compile_content(self.data)
    def test_partial_capability_manifest_rejected(self):
        self.data['required_capabilities']=1
        with self.assertRaises(compiler.ContentError):compiler.compile_content(self.data)
    def test_ragged_map_rejected(self):
        self.data['maps'][0]['tiles'][1]='...'
        with self.assertRaises(compiler.ContentError):compiler.compile_content(self.data)
    def test_text_change_only_changes_pack(self):
        before,_=compiler.compile_content(self.data)
        self.data['programs'][0]['code'][1]['text']='New dialogue, same implementation.'
        after,_=compiler.compile_content(self.data)
        self.assertNotEqual(before,after)
    def test_source_map_retained(self):
        _,m=compiler.compile_content(self.data)
        self.assertEqual(m['source_map']['3001:1']['json_pointer'],'/programs/0/code/1')
    def test_bad_flag_type(self):
        self.data['programs'][0]['code'][0]['value']='true'
        with self.assertRaises(compiler.ContentError):compiler.compile_content(self.data)
if __name__=='__main__':unittest.main()
