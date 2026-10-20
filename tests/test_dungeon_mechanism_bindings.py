import copy,json,struct,tempfile,unittest,zlib
from pathlib import Path
from tools import dungeon_mechanism_bindings as d

class MechanismBindings(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.review=d.read();cls.model,cls.blob,cls.proof=d.extract()
 def reject(self,edit,extract=False):
  p=copy.deepcopy(self.review);edit(p)
  with tempfile.TemporaryDirectory(dir=d.ROOT/'build',prefix='mechanism-binding-negative-')as directory:
   file=Path(directory)/'review.json';file.write_text(json.dumps(p),encoding='utf8')
   with self.assertRaises(ValueError):(d.extract if extract else d.read)(file)
 def changed(self,offset,fmt,value,inner=None):
  b=bytearray(self.blob);struct.pack_into(fmt,b,offset,value)
  if inner is not None:
   n=struct.unpack_from('<I',b,inner+12)[0];struct.pack_into('<I',b,inner+16,zlib.crc32(b[inner+32:inner+n]))
  struct.pack_into('<I',b,16,zlib.crc32(b[32:]));return bytes(b)
 def test_exact_source_join_and_independent_resource(self):
  m=self.model;self.assertEqual(d.parse(self.blob),m);self.assertEqual(len(self.blob),4060)
  self.assertEqual(m['scene'],4);self.assertEqual(len(m['bindings']),14);self.assertEqual([p['family']for p in m['profiles']],[1,2,3,4])
  self.assertFalse(self.proof['whole_scripts_approved']or self.proof['execution_admission'])
  self.assertEqual(self.proof['source_node_order'],[r['path']for r in self.review['bindings']])
  for row in m['bindings']:self.assertEqual(row['roles'][0],0)
 def test_review_schema_pin_and_fields_reject(self):
  for edit in [lambda p:p.update(schema=True),lambda p:p.update(schema=2),lambda p:p.update(kind='unknown'),lambda p:p.update(commit='0'*40),lambda p:p.update(scope=''),lambda p:p.update(extra=1),lambda p:p['dependencies'].update(extra={})]:
   with self.subTest(edit=edit):self.reject(edit)
 def test_review_dependency_and_structural_types_reject(self):
  for edit in [lambda p:p['dependencies']['graph'].update(path='../escape'),lambda p:p['dependencies']['graph'].update(sha256='0'*64),lambda p:p.update(profiles=[]),lambda p:p.update(bindings=[]),lambda p:p['profiles'][0].update(id=True),lambda p:p['profiles'][0].update(name=[]),lambda p:p['profiles'][0].update(kind='unknown'),lambda p:p['profiles'][0].update(roles={'Unknown':1}),lambda p:p['profiles'][0].update(roles={'None':0}),lambda p:p['bindings'][0].update(owner=0),lambda p:p['bindings'][0].update(path='../escape'),lambda p:p['bindings'][0].update(kind='unknown')]:
   with self.subTest(edit=edit):self.reject(edit)
 def test_review_source_roles_profile_and_owner_mismatches_reject(self):
  edits=[lambda p:p['profiles'][0].update(source_profile=65535),lambda p:p['profiles'][0]['roles'].update(TurnOn=65535),lambda p:p['bindings'][0].update(owner=1),lambda p:p['bindings'][0].update(path='Unknown/path'),lambda p:p['bindings'][0].update(profile=65535)]
  for edit in edits:
   with self.subTest(edit=edit):self.reject(edit,True)
 def test_original_effective_child_order_and_duplicate_owner_reject(self):
  self.reject(lambda p:p['bindings'].reverse(),True)
  self.reject(lambda p:p['bindings'].__setitem__(1,p['bindings'][0]),True)
 def test_all_truncations_version_capability_pin_and_spans_reject(self):
  for size in range(len(self.blob)):
   with self.assertRaises(ValueError):d.parse(self.blob[:size])
  for at,value in [(8,0),(8,2),(12,len(self.blob)+1),(20,0),(20,2),(24,0),(24,65536),(28,1),(32,0),(52,5),(56,1),(64,0),(68,0),(72,0),(76,1),(80,1),(84,0),(88,0),(92,40),(96,1),(100,0),(104,0),(112,1),(116,0),(120,0)]:
   with self.subTest(at=at,value=value),self.assertRaises(ValueError):d.parse(self.changed(at,'<I',value))
  for bad in [self.blob+b'x',bytes([self.blob[0]^1])+self.blob[1:],self.blob[:180]+bytes([self.blob[180]^1])+self.blob[181:]]:
   with self.assertRaises(ValueError):d.parse(bad)
 def test_binary_binding_identity_role_coverage_and_alias_reject(self):
  base=struct.unpack_from('<I',self.blob,68)[0];kind=struct.unpack_from('<I',self.blob,base+4)[0]
  for at,value in [(base,0),(base,1),(base+4,3-kind),(base+8,0),(base+8,65535),(base+12,7),(base+12,2**32-1),(base+12+4*4,65535)]:
   with self.subTest(at=at,value=value),self.assertRaises(ValueError):d.parse(self.changed(at,'<I',value))
  b=bytearray(self.blob);b[base+40:base+80]=b[base:base+40];struct.pack_into('<I',b,16,zlib.crc32(b[32:]))
  with self.assertRaises(ValueError):d.parse(bytes(b))
 def test_binary_profile_kind_span_and_nested_versions_reject(self):
  table=struct.unpack_from('<I',self.blob,84)[0];payload=struct.unpack_from('<I',self.blob,100)[0];graph=struct.unpack_from('<I',self.blob,116)[0]
  for at,value in [(table,0),(table+4,0),(table+4,5),(table+8,1),(table+12,0),(table+12,2**32-1),(payload+8,2),(payload+20,2),(graph+8,2),(graph+20,2),(graph+24,5)]:
   with self.subTest(at=at,value=value),self.assertRaises(ValueError):d.parse(self.changed(at,'<I',value))
  policy=payload+struct.unpack_from('<I',self.blob,payload+68)[0]
  with self.assertRaises(ValueError):d.parse(self.changed(policy+8,'<I',4,payload))
  keys=payload+struct.unpack_from('<I',self.blob,payload+100)[0]
  with self.assertRaises(ValueError):d.parse(self.changed(keys+4,'<I',5,payload))
  wall=payload+struct.unpack_from('<I',self.blob,table+3*16+8)[0];wall_strings=wall+struct.unpack_from('<I',self.blob,wall+68)[0]
  b=bytearray(self.blob);b[wall_strings]=ord('X');n=struct.unpack_from('<I',b,wall+12)[0];struct.pack_into('<I',b,wall+16,zlib.crc32(b[wall+32:wall+n]));struct.pack_into('<I',b,16,zlib.crc32(b[32:]))
  with self.assertRaises(ValueError):d.parse(bytes(b))
  bindings=struct.unpack_from('<I',self.blob,68)[0]
  index=next(i for i,r in enumerate(self.model['bindings'])if r['owner']==3761819854);at=bindings+index*40;b=bytearray(self.blob);a,c=struct.unpack_from('<2I',b,at+32);struct.pack_into('<2I',b,at+32,c,a);struct.pack_into('<I',b,16,zlib.crc32(b[32:]))
  with self.assertRaises(ValueError):d.parse(bytes(b))
 def test_duplicate_json_and_unsafe_candidate_output_reject(self):
  with tempfile.TemporaryDirectory(dir=d.ROOT/'build',prefix='mechanism-json-negative-')as directory:
   p=Path(directory)/'review.json';p.write_text('{"schema":1,"schema":1}',encoding='utf8')
   with self.assertRaises(ValueError):d.read(p)
  for path in [d.ROOT,d.ROOT/'build']:
   with self.subTest(path=path),self.assertRaises(ValueError):d.build(path)

if __name__=='__main__':unittest.main()
