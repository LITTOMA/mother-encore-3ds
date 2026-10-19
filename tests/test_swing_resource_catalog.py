"""Schema fixtures: no shipping assets or playable integration are fabricated."""
import copy,struct,unittest,zlib
from pathlib import Path
from unittest.mock import patch
from tools import resource_catalog as c

class SwingCatalog(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.base=c.load_catalog()
 def candidate(self):
  p=copy.deepcopy(self.base)
  for role in ('FieldSwing','FieldSwingGraphics','FieldSwingAction','FieldSwingEntry'):
   p['bindings'].append(dict(id=c.ROLES[role],role=role,path='world/fixture/'+role+c.SUFFIXES[role]))
  return p
 def encode(self,p):
  with patch.object(c,'verify_sources'),patch.object(c,'checked_file',return_value=b'checked-schema-fixture'):return c.encode(p)
 def test_global_action_group_has_independent_capability_and_stable_ids(self):
  self.assertEqual([c.ROLES[r]for r in ('FieldMenus','FieldNpc','FieldStory','FieldSteps','FieldProps')],[40,41,42,43,44])
  p=self.candidate();blob=self.encode(p);self.assertEqual(struct.unpack_from('<I',blob,20)[0],12)
  self.assertEqual(blob,(Path(__file__).parent/'fixtures/catalog-cap12-swing.encresources').read_bytes())
  decoded=c.decode(blob);self.assertEqual({r['id']for r in decoded['bindings']if r['role']in c.SWING_ROLES},{45,46,47,48})
  self.assertEqual(c.encode(self.base),c.OUT.read_bytes())
 def test_missing_duplicate_wrong_identity_or_type_rejects_before_publication(self):
  for role in c.SWING_ROLES:
   p=self.candidate();p['bindings']=[r for r in p['bindings']if r['role']!=role]
   with self.subTest(missing=role),self.assertRaises(ValueError):self.encode(p)
  edits=[lambda p:p['bindings'][-1].update(id=1025),lambda p:p['bindings'][-1].update(id=True),lambda p:p['bindings'][-1].update(role='Unknown'),lambda p:p['bindings'][-1].update(path='world/fixture/wrong.encgraphics'),lambda p:p['bindings'].append(dict(p['bindings'][-1])),lambda p:p['bindings'].pop(next(i for i,r in enumerate(p['bindings'])if r['role']=='FieldProps'))]
  for edit in edits:
   p=self.candidate();edit(p)
   with self.subTest(edit=edit),self.assertRaises(ValueError):self.encode(p)
 def test_version_mismatch_unknown_capability_truncation_and_legacy_are_strict(self):
  blob=self.encode(self.candidate())
  for cap in (0,1,10,11,13):
   bad=bytearray(blob);struct.pack_into('<I',bad,20,cap)
   with self.subTest(cap=cap),self.assertRaises(ValueError):c.decode(bad)
  old=c.OUT.read_bytes();self.assertEqual(c.capability_for(c.decode(old)['bindings']),11)
  bad=bytearray(old);struct.pack_into('<I',bad,20,12)
  with self.assertRaises(ValueError):c.decode(bad)
  for n in range(len(blob)):
   with self.assertRaises(ValueError):c.decode(blob[:n])

if __name__=='__main__':unittest.main()
