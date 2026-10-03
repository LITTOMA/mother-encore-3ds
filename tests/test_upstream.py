import copy
import json
from pathlib import Path
import tempfile
import unittest
from tools.upstream import snapshot,gate,diff_snapshots,digest
ROOT=Path(__file__).resolve().parents[1]
class UpstreamTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name)
        (self.root/'content.json').write_bytes((ROOT/'content/sandbox.json').read_bytes())
        (self.root/'logic.gd').write_text('func damage():\n    return 1\n')
        self.base=snapshot(self.root)
        self.registry={'schema':1,'bindings':{
            'content.json':{'kind':'exported','exporter':'sandbox-json-v1'},
            'logic.gd':{'kind':'native','reviewed_sha256':self.base['files']['logic.gd']['sha256'],
                        'native_files':['runtime/game.cpp'],'tests':['fixture_battle_only']}}}
    def tearDown(self):self.temp.cleanup()
    def test_unchanged_passes(self):self.assertTrue(gate(self.base,snapshot(self.root),self.registry,self.root)['passed'])
    def test_content_update_is_automatic(self):
        d=json.loads((self.root/'content.json').read_text());d['enemies'][0]['hp']=28
        (self.root/'content.json').write_text(json.dumps(d))
        r=gate(self.base,snapshot(self.root),self.registry,self.root)
        self.assertTrue(r['passed']);self.assertEqual(r['automatic_content_updates'],['content.json'])
    def test_native_change_blocks(self):
        (self.root/'logic.gd').write_text('func damage():\n    return 2\n')
        r=gate(self.base,snapshot(self.root),self.registry,self.root)
        self.assertFalse(r['passed']);self.assertEqual(r['problems'][0]['reason'],'native_source_changed_or_unreviewed')
    def test_new_script_blocks(self):
        (self.root/'new.gd').write_text('extends Node\n')
        r=gate(self.base,snapshot(self.root),self.registry,self.root)
        self.assertFalse(r['passed']);self.assertTrue(any(x['reason']=='unmapped_source' for x in r['problems']))
    def test_unknown_opcode_blocks(self):
        d=json.loads((self.root/'content.json').read_text());d['programs'][0]['code'][1]['op']='NEW_MECHANIC'
        (self.root/'content.json').write_text(json.dumps(d))
        r=gate(self.base,snapshot(self.root),self.registry,self.root)
        self.assertFalse(r['passed']);self.assertTrue(any(x['reason']=='import_validation_failed' for x in r['problems']))
    def test_removed_file_blocks(self):
        (self.root/'logic.gd').unlink()
        self.assertFalse(gate(self.base,snapshot(self.root),self.registry,self.root)['passed'])
    def test_dependency_change_blocks(self):
        self.registry['bindings']['logic.gd']['dependencies']={'content.json':self.base['files']['content.json']['sha256']}
        (self.root/'content.json').write_text((self.root/'content.json').read_text()+'\n')
        r=gate(self.base,snapshot(self.root),self.registry,self.root)
        self.assertFalse(r['passed']);self.assertTrue(any(x['reason']=='native_dependency_changed' for x in r['problems']))
    def test_snapshot_must_match_checkout(self):
        (self.root/'logic.gd').write_text('# changed after snapshot\n')
        self.assertFalse(gate(self.base,self.base,self.registry,self.root)['passed'])
    def test_real_encore_adapter_not_faked(self):
        self.registry['bindings']['content.json']['exporter']='encorescript-v1'
        self.assertFalse(gate(self.base,snapshot(self.root),self.registry,self.root)['passed'])
    def test_missing_cpp_implementation_blocks(self):
        self.registry['bindings']['logic.gd']['native_files']=['runtime/not_implemented.cpp']
        self.assertFalse(gate(self.base,self.base,self.registry,self.root)['passed'])
    def test_path_traversal_rejected(self):
        self.registry['bindings']['logic.gd']['native_files']=['../outside.cpp']
        with self.assertRaises(ValueError):gate(self.base,self.base,self.registry,self.root)
    def test_function_discovery_is_diagnostic(self):
        self.assertEqual(self.base['files']['logic.gd']['functions'],['damage'])
if __name__=='__main__':unittest.main()
