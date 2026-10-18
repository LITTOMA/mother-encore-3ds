"""Schema fixtures only: no upstream scene or console build claims."""
import copy,struct,unittest,zlib
from unittest.mock import patch
from tools import resource_catalog as c
class SceneCatalogCodec(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.base=c.load_catalog()
 def extra(self):
  p=copy.deepcopy(self.base)
  for id,role,ext in [(1024,'FieldRoom','encroom'),(1025,'FieldMap','encmap'),(1026,'FieldStory','encstory')]:p['bindings'].append(dict(id=id,role=role,path='world/fixture/'+role+'.'+ext))
  return p
 def encode(self,p):
  with patch.object(c,'verify_sources'),patch.object(c,'checked_file',return_value=b'checked-schema-fixture'):return c.encode(p)
 def test_old_and_multiple_scene_binding_capabilities(self):
  self.assertEqual(c.capability_for(self.base['bindings']),8);self.assertEqual(c.capability_for(self.extra()['bindings']),9)
  decoded=c.decode(self.encode(self.extra()));self.assertEqual(len(decoded['bindings']),len(self.base['bindings'])+3);self.assertEqual({r['role']for r in decoded['bindings']if r['id']>=1024},{'FieldRoom','FieldMap','FieldStory'})
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
