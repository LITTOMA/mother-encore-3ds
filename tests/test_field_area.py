import copy,json,struct,tempfile,unittest,zlib
from pathlib import Path
from tools import field_area as a,podunk_field as f
class AreaTests(unittest.TestCase):
 @classmethod
 def setUpClass(c):c.metadata,c.sources=a.source();c.blob=(a.ROOT/'tests/fixtures/mayor-area.encmap').read_bytes()
 def test_source_defaults_and_instance(self):
  m=self.metadata;self.assertEqual(m['map_offset'],[110,240]);self.assertEqual(m['background'],0x1c1c1cff);self.assertTrue(m['sub_area']);self.assertEqual(m['visit_flag'],'visited_podunk');self.assertEqual(m['party_guard_flag'],'flying_man_in_party');self.assertEqual(m['map_item'],'MapPodunk');self.assertEqual(m['map_override_item'],'MapMerrysville')
  self.assertIn('Scripts/global/Inventory.gd',self.sources);self.assertIn('Data/Items/MapMerrysville.yaml',self.sources)
 def test_checked_codec(self):
  ident,sections=f.unpack_container(self.blob,f.MAP_MAGIC,f.MAP_STRIDES,f.MAP_LIMIT);self.assertEqual(ident,3);self.assertEqual(a.decode(sections[-1],sections[0]),self.metadata)
  for pos,value in [(8,2),(20,4),(20,6),(52,21),(64+21*16+8,0),(64+21*16+12,84)]:
   bad=bytearray(self.blob);struct.pack_into('<I',bad,pos,value);struct.pack_into('<I',bad,16,zlib.crc32(bad[32:]))
   with self.assertRaises(ValueError):f.unpack_container(bad,f.MAP_MAGIC,f.MAP_STRIDES,f.MAP_LIMIT)
  for pos in [0,7,64,415,len(self.blob)-1]:
   with self.assertRaises(ValueError):f.unpack_container(self.blob[:pos],f.MAP_MAGIC,f.MAP_STRIDES,f.MAP_LIMIT)
 def test_metadata_negative(self):
  for k,v in [('source','res://../escape'),('region',''),('map_item','../a'),('map_override_item',''),('visit_flag','bad:flag'),('map_offset',[float('nan'),1]),('map_offset',[True,1]),('map_divisor',0),('map_divisor',True),('background',0x1c1c1c00),('background',True),('sub_area',1)]:
   bad=copy.deepcopy(self.metadata);bad[k]=v
   with self.subTest(key=k,value=v):
    with self.assertRaises(ValueError):a.validate(bad)
  bad=dict(self.metadata,unknown=1)
  with self.assertRaises(ValueError):a.validate(bad)
 def test_recipe_negative(self):
  for k,v in [('schema',True),('schema',2),('commit','0'*40),('field_recipe','../escape'),('maps',['MapPodunk','MapPodunk']),('maps',['MapPodunk']),('script','Scripts/Main/Door.gd')]:
   p=a.policy();p[k]=v
   with tempfile.TemporaryDirectory()as tmp:
    file=Path(tmp)/'area.json';file.write_text(json.dumps(p),encoding='utf8')
    with self.subTest(key=k,value=v):
     with self.assertRaises(ValueError):a.source(file)
 def test_native_result_and_review_negative(self):
  from tools.run_area_reference import source,validate_result
  self.assertEqual(source()[3],json.loads((a.ROOT/'compatibility/reviews/field-area-v0410.json').read_bytes()))
  r=json.loads((a.ROOT/'reports/mayor-office-import/area-native.json').read_bytes());validate_result(r)
  for change in [lambda x:x.update(schema=True),lambda x:x['results'].pop(),lambda x:x['results'][0].update(sub=0),lambda x:x['results'][0].update(position=[True,-63]),lambda x:x['results'][0].update(map=[float('nan'),0]),lambda x:x['results'][0].update(signals=True)]:
   bad=copy.deepcopy(r);change(bad)
   with self.assertRaises(ValueError):validate_result(bad)
if __name__=='__main__':unittest.main()
