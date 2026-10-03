from pathlib import Path
import json
import subprocess
import tempfile
import unittest
from tools.upstream import snapshot, pin_existing, write_json
from tools.upstream_audit import scan_text, audit

class InventoryTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name)
        self.command('init','-q')
        self.command('config','user.email','fixture@example.invalid')
        self.command('config','user.name','Inventory fixture')
        self.command('config','core.autocrlf','false')
        self.command('remote','add','origin','https://example.invalid/source.git')
        (self.root/'.godot').mkdir();(self.root/'.godot/tracked.cfg').write_text('tracked=true\n')
        (self.root/'Scripts/global').mkdir(parents=True)
        (self.root/'Scripts/global/global.gd').write_text('const GAME_VERSION := "0.4.1.0"\n')
        (self.root/'a.tscn').write_text('[node name="A" type="Node2D"]\ntexture = "res://with spaces.png"\n')
        (self.root/'binary.res').write_bytes(b'\xff\x00\x80')
        self.command('add','.');self.command('commit','-qm','test fixture')
        self.lock=self.root.parent/(self.root.name+'-lock.json')
        write_json(self.lock,{'schema':1,'repository':'https://example.invalid/source.git'})
    def command(self,*args):
        return subprocess.check_output(['git','-C',str(self.root),*args],stderr=subprocess.PIPE,text=True).strip()
    def tearDown(self):
        self.lock.unlink(missing_ok=True);self.temp.cleanup()
    def test_all_tracked_blobs_and_spaces_and_binary(self):
        inv=snapshot(self.root)
        self.assertEqual(len(inv['files']),4)
        self.assertIn('.godot/tracked.cfg',inv['files'])
        self.assertEqual(inv['files']['a.tscn']['res_references'],['res://with spaces.png'])
        self.assertNotIn('res_references',inv['files']['binary.res'])
    def test_dirty_source_cannot_claim_commit(self):
        (self.root/'a.tscn').write_text('changed')
        with self.assertRaisesRegex(ValueError,'Dirty'):snapshot(self.root)
    def test_ignored_untracked_file_is_not_silently_dropped(self):
        (self.root/'.git/info/exclude').write_text('cache.tmp\n')
        (self.root/'cache.tmp').write_text('unreviewed')
        with self.assertRaisesRegex(ValueError,'Untracked'):snapshot(self.root)
    def test_git_symlink_mode_rejected(self):
        blob=self.command('hash-object','a.tscn')
        self.command('update-index','--add','--cacheinfo',f'120000,{blob},link')
        self.command('commit','-qm','symlink fixture')
        # Materializing a fake regular file does not make Git's symlink safe.
        (self.root/'link').write_bytes((self.root/'a.tscn').read_bytes())
        with self.assertRaises(ValueError):snapshot(self.root)
    def test_invalid_utf8_source_rejected(self):
        (self.root/'bad.gd').write_bytes(b'\xff')
        self.command('add','bad.gd');self.command('commit','-qm','invalid text fixture')
        with self.assertRaises(UnicodeError):snapshot(self.root)
    def test_existing_pin_matches_actual_version_and_commit(self):
        sha=self.command('rev-parse','HEAD');pin_existing(sha,self.root,self.lock,'0.4.1.0')
        lock=json.loads(self.lock.read_text());self.assertEqual(lock['commit'],sha)
        result=audit(self.root,lock);self.assertFalse(result['native_compatible'])
        self.assertEqual(result['unresolved_references'][0]['reference'],'res://with spaces.png')
    def test_pin_rejects_unknown_version_and_ref(self):
        sha=self.command('rev-parse','HEAD')
        for ref,version in [(sha,'9.9.9'),('0'*40,'0.4.1.0'),('main','0.4.1.0')]:
            with self.assertRaises(ValueError):pin_existing(ref,self.root,self.lock,version)
    def test_audit_rejects_unpinned_version_path(self):
        with self.assertRaises(ValueError):audit(self.root,{'schema':1,'commit':None})
        with self.assertRaises(ValueError):audit(self.root,{'schema':2})
    def test_dynamic_diagnostics_keep_location_and_no_approval(self):
        d=scan_text('a.gd','class_name A\nextends Node\nvar v = load(prefix + name)\nv.call(method)\n')
        self.assertEqual(d['capabilities']['dynamic_resource_loading'][0]['line'],3)
        self.assertEqual(d['capabilities']['reflection_or_dynamic_call'][0]['line'],4)
        self.assertEqual(d['classes'][0]['name'],'A')

if __name__=='__main__':unittest.main()
