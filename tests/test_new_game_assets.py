"""Manual naming dependency admission; no Godot/texture generation in these cases."""
import copy,json,unittest
from pathlib import Path
from tools import new_game_assets as naming
ROOT=Path(__file__).resolve().parents[1]
class NamingSessionDependency(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.recipe=naming.read_json(naming.IR)
        cls.current=naming.read_json(ROOT/'content/native-session.json')
        cls.previous=copy.deepcopy(cls.current)
        cls.previous['mutable_flags'].remove('mick_telepathy')
        cls.previous['key_acquisitions']=[k for k in cls.previous['key_acquisitions'] if k['item_id']!='KeyBasement']
    def test_only_session_fingerprint_changes_and_binary_stays_identical(self):
        before=copy.deepcopy(self.recipe);out=naming.rebound_session_dependency(before,self.previous,self.current,'a'*64)
        self.assertEqual(before,self.recipe)
        out['dependencies']['content/native-session.json']=before['dependencies']['content/native-session.json']
        self.assertEqual(out,before)
        self.assertEqual(naming.encode(out),naming.PACK.read_bytes())
    def test_defaults_startup_and_levels_cannot_be_rebound(self):
        for key in ('defaults','startup','levels','sources'):
            value=copy.deepcopy(self.current);value[key]={}
            with self.subTest(key=key),self.assertRaises(ValueError):naming.rebound_session_dependency(self.recipe,self.previous,value,'a'*64)
    def test_removed_flags_acquisitions_unknown_fields_and_bad_hash_rejected(self):
        for kind in ('flags','keys','unknown'):
            value=copy.deepcopy(self.current)
            if kind=='flags':value['mutable_flags']=[]
            elif kind=='keys':value['key_acquisitions']=[]
            else:value['unknown']=True
            with self.subTest(kind=kind),self.assertRaises(ValueError):naming.rebound_session_dependency(self.recipe,self.previous,value,'a'*64)
        for sha in ('','unknown','g'*64):
            with self.assertRaises(ValueError):naming.rebound_session_dependency(self.recipe,self.previous,self.current,sha)
    def test_field_session_extension_preserves_startup_and_rejects_unreviewed_policy(self):
        current=copy.deepcopy(self.current);current['schema']=10;current.pop('run_sounds')
        previous=copy.deepcopy(current);previous['schema']=9;previous.pop('field_encounters')
        previous['acquisitions']=previous['acquisitions'][:-1];previous['storage_policies']=previous['storage_policies'][:-1];previous['field_items']=previous['field_items'][:-1]
        previous['mutable_flags']=[x for x in previous['mutable_flags']if x not in ('beat_dog_bridge','pippis_mom_call','pippis_mom_help')]
        candidate=naming.rebound_session_dependency(self.recipe,previous,current,'b'*64)
        self.assertEqual(naming.encode(candidate),naming.PACK.read_bytes())
        for key,value in [('field_encounters',['unknown']),('acquisitions',[]),('storage_policies',[]),('field_items',[]),('schema',11)]:
            bad=copy.deepcopy(current);bad[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):naming.rebound_session_dependency(self.recipe,previous,bad,'b'*64)
        # Merged mainline menus occupy v9. v8 -> v10 is an unreviewed skip,
        # and must never be admitted merely to make the historical fixture pass.
        skipped=copy.deepcopy(previous);skipped['schema']=8
        with self.assertRaises(ValueError):naming.rebound_session_dependency(self.recipe,skipped,current,'b'*64)
    def test_source_run_extension_keeps_naming_content(self):
        previous=copy.deepcopy(self.current);previous['schema']=10;previous.pop('run_sounds')
        candidate=naming.rebound_session_dependency(self.recipe,previous,self.current,'b'*64);self.assertEqual(naming.encode(candidate),naming.PACK.read_bytes())
        for key,value in [('run_sounds',['wood','unreviewed']),('run_sounds',['wood']),('run_sounds',['wood','wood']),('field_items',[]),('schema',12),('defaults',{})]:
            bad=copy.deepcopy(self.current);bad[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):naming.rebound_session_dependency(self.recipe,previous,bad,'b'*64)
        skipped=copy.deepcopy(previous);skipped['schema']=9
        with self.assertRaises(ValueError):naming.rebound_session_dependency(self.recipe,skipped,self.current,'b'*64)
    def test_exact_baseline_required(self):
        for sha in (None,'main','1234','g'*40):
            with self.assertRaises(ValueError):naming.reuse_session(sha)
if __name__=='__main__':unittest.main()
