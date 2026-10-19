"""Schema fixtures only: no upstream scene or console build claims."""
import copy,struct,unittest,zlib
from pathlib import Path
from unittest.mock import patch
from tools import resource_catalog as c
class SceneCatalogCodec(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.base=copy.deepcopy(c.load_catalog())
  # Freeze the pre-Mayor schema baseline: shipping now uses capability10 and
  # owns the synthetic IDs that these independent capability tests append.
  cls.base['bindings']=c.decode((Path(__file__).parent/'fixtures/catalog-cap8.encresources').read_bytes())['bindings']
 def extra(self):
  p=copy.deepcopy(self.base)
  for id,role,ext in [(1024,'FieldRoom','encroom'),(1025,'FieldMap','encmap'),(1026,'FieldStory','encstory')]:p['bindings'].append(dict(id=id,role=role,path='world/fixture/'+role+'.'+ext))
  return p
 def encode(self,p):
  with patch.object(c,'verify_sources'),patch.object(c,'checked_file',return_value=b'checked-schema-fixture'):return c.encode(p)
 def test_old_and_multiple_scene_binding_capabilities(self):
  self.assertEqual(c.capability_for(self.base['bindings']),8);self.assertEqual(c.capability_for(self.extra()['bindings']),9)
  decoded=c.decode(self.encode(self.extra()));self.assertEqual(len(decoded['bindings']),len(self.base['bindings'])+3);self.assertEqual({r['role']for r in decoded['bindings']if r['id']>=1024},{'FieldRoom','FieldMap','FieldStory'})
 def steps(self):
  p=copy.deepcopy(self.base);p['bindings'].append(dict(id=1030,role='FieldSteps',path='world/fixture/steps.encsteps'));return p
 def test_stepping_uses_independent_capability10_identity(self):
  p=self.steps();blob=self.encode(p);self.assertEqual(c.capability_for(p['bindings']),10);self.assertEqual(c.decode(blob)['bindings'][-1]['role'],'FieldSteps')
  for cap in (0,1,8,9,11):
   bad=bytearray(blob);struct.pack_into('<I',bad,20,cap)
   with self.subTest(cap=cap),self.assertRaises(ValueError):c.decode(bad)
 def test_stepping_roles_and_paths_are_strict(self):
  for mutate in (lambda p:p['bindings'][-1].update(id=43),lambda p:p['bindings'][-1].update(id=True),lambda p:p['bindings'][-1].update(id=1023),lambda p:p['bindings'][-1].update(path='world/fixture/steps.encstory'),lambda p:p['bindings'].pop(next(i for i,r in enumerate(p['bindings'])if r['role']=='FieldStory')),lambda p:p['bindings'].append(dict(p['bindings'][-1]))):
   p=self.steps();mutate(p)
   with self.assertRaises(ValueError):self.encode(p)
 def test_wrong_ids_types_and_missing_default_are_rejected(self):
  for mutate in (lambda p:p['bindings'][-1].update(id=100),lambda p:p['bindings'][-1].update(role='FieldMenus'),lambda p:p['bindings'][-1].update(id=True),lambda p:p['bindings'][-1].update(path='world/fixture/wrong.encroom'),lambda p:p['bindings'].pop(next(i for i,r in enumerate(p['bindings'])if r['role']=='FieldRoom'and r['id']==31))):
   p=self.extra();mutate(p)
   with self.assertRaises(ValueError):self.encode(p)
 def test_new_and_old_capability_mismatch_and_truncated_payload(self):
  blob=self.encode(self.extra())
  for cap in (0,1,8,10):
   bad=bytearray(blob);struct.pack_into('<I',bad,20,cap)
   with self.assertRaises(ValueError):c.decode(bad)
  bad=bytearray(self.encode(self.base));struct.pack_into('<I',bad,20,9)
  with self.assertRaises(ValueError):c.decode(bad)
  for n in (0,59,len(blob)-1):
   with self.assertRaises(ValueError):c.decode(blob[:n])
if __name__=='__main__':unittest.main()
