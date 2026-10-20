import copy,struct,unittest
from unittest.mock import patch
from tools import world_scene_routes as w,resource_catalog as c,podunk_field as f
class ObjectLinks(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.base=copy.deepcopy(w.load());cls.base['schema']=3
  for s in cls.base['scenes']:
   if s['map']:s['assets'].pop('objects',None)
  cls.recipe=copy.deepcopy(cls.base);cls.recipe['schema']=4
  for s in cls.recipe['scenes']:
   if s['map']:s['assets']['objects']=1032 if s['id']==2 else 0
  cls.routes=w.rows(cls.recipe,f.Sources());cls.blob=w.encode(cls.recipe,cls.routes);cls.catalog=c.load_catalog();cls.catalog['bindings']=[r for r in cls.catalog['bindings']if r['role']!='FieldObjects'];cls.catalog['bindings'].append(dict(id=1032,role='FieldObjects',path='world/podunk/podunk.encobjects'))
 def test_new_section_preserves_old_route_and_prop_sections(self):
  old=w.encode(self.base,self.routes);_,before=f.unpack_container(old,f.LINK_MAGIC,w.PROP_STRIDES,f.LINK_LIMIT,capability=4);_,after=f.unpack_container(self.blob,f.LINK_MAGIC,w.OBJECT_STRIDES,f.LINK_LIMIT,capability=5);self.assertEqual(before,after[:6]);self.assertEqual(struct.unpack('<2I',after[6]),(2,1032))
 def test_missing_unknown_bad_or_unused_object_reference_rejected(self):
  for value in(True,49,1023,-1,0,2**32,1031):
   p=copy.deepcopy(self.recipe);p['scenes'][1]['assets']['objects']=value
   with self.subTest(value=value),self.assertRaises(ValueError):w.validate(p)
  p=copy.deepcopy(self.recipe);del p['scenes'][1]['assets']['objects']
  with self.assertRaises(ValueError):w.validate(p)
 def test_duplicate_owner_and_unknown_schema_rejected(self):
  p=copy.deepcopy(self.recipe);p['scenes'][2]['assets']['objects']=1032
  with self.assertRaises(ValueError):w.validate(p)
  for version in(0,3,5,True):
   p=copy.deepcopy(self.recipe);p['schema']=version
   with self.subTest(version=version),self.assertRaises(ValueError):w.validate(p)
 def test_catalog_has_stable_role_and_complete_parent_group(self):
  self.assertEqual(c.ROLES['FieldObjects'],49);self.assertEqual(c.capability_for(self.catalog['bindings']),13);c.validate(self.catalog)
  for role in('FieldProps','FieldSteps','FieldSwing','FieldSwingGraphics','FieldSwingAction','FieldSwingEntry'):
   p=copy.deepcopy(self.catalog);p['bindings']=[r for r in p['bindings']if r['role']!=role]
   with self.subTest(role=role),self.assertRaises(ValueError):c.validate(p)
 def test_catalog_bad_independent_id_or_suffix_rejected(self):
  for key,value in [('id',True),('id',49),('id',1023),('id',1031),('path','world/podunk/wrong.encprops'),('role','unknown')]:
   p=copy.deepcopy(self.catalog);p['bindings'][-1][key]=value
   with self.subTest(key=key,value=value),self.assertRaises(ValueError):c.validate(p)
 def test_catalog_roundtrip_and_unknown_or_downgraded_version_rejected(self):
  original=c.checked_file
  def read(root,path):return b'Explicit catalog codec fixture'if path=='world/podunk/podunk.encobjects'else original(root,path)
  with patch.object(c,'checked_file',side_effect=read):blob=c.encode(self.catalog)
  c.decode(blob);self.assertEqual(struct.unpack_from('<I',blob,20)[0],13)
  for cap in(0,12,14):
   bad=bytearray(blob);struct.pack_into('<I',bad,20,cap)
   with self.subTest(cap=cap),self.assertRaises(ValueError):c.decode(bad)
if __name__=='__main__':unittest.main()
