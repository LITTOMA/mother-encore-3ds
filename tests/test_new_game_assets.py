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
    def test_exact_baseline_required(self):
        for sha in (None,'main','1234','g'*40):
            with self.assertRaises(ValueError):naming.reuse_session(sha)
if __name__=='__main__':unittest.main()
