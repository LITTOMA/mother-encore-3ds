#!/usr/bin/env python3
import copy,json,sys,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import session_migration as migration
class MigrationAssets(unittest.TestCase):
 def test_reproducible_checked_pack(self):
  self.assertEqual(migration.encode(),migration.PACK.read_bytes())
  self.assertEqual(migration.stage_files(ROOT/'romfs'),{Path('data/rules6-to7.encmigration'):migration.PACK.read_bytes()})
 def reject_changed_recipe(self,filename,edit):
  original=migration.read
  def changed(path):
   value=original(path)
   if str(path).endswith(filename):edit(value)
   return value
  with patch.object(migration,'read',changed):
   with self.assertRaises(ValueError):migration.encode()
 def test_unknown_endpoints(self):
  self.reject_changed_recipe('manifest.json',lambda x:x['to'].update(rules_revision=8))
  self.reject_changed_recipe('manifest.json',lambda x:x['from'].update(content_revision=2))
 def test_unknown_or_tampered_frozen_resource(self):
  self.reject_changed_recipe('manifest.json',lambda x:x['files'].update({'other.encsave':'0'*64}))
  self.reject_changed_recipe('manifest.json',lambda x:x['files'].update({'opening.encsession':'0'*64}))
 def test_lossy_defaults_or_scopes(self):
  # Only edit today's recipe; the frozen source is left byte-identical.
  original=migration.read
  for edit in [lambda x:x['defaults'].update(bank=999),lambda x:x['defaults']['flags'].pop(),lambda x:x.update(mutable_flags=[]),lambda x:x.update(camera_area_ids=[])]:
   def changed(path):
    value=original(path)
    if Path(path)==ROOT/'content/native-session.json':edit(value)
    return value
   with patch.object(migration,'read',changed):
    with self.assertRaises(ValueError):migration.encode()
if __name__=='__main__':unittest.main()
