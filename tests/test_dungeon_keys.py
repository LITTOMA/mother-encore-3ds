"""Candidate only: actual key/lock source admission and negative codec paths."""
import copy,json,struct,tempfile,unittest,zlib
from pathlib import Path
from unittest.mock import patch
from tools import dungeon_keys as d
ROOT=Path(__file__).resolve().parents[1]

class DungeonKeysTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.review,_=d.read();cls.data,cls.proof=d.extract();cls.blob=d.encode(cls.data)
 def candidate(self,change,lower=False):
  value=copy.deepcopy(self.review);change(value)
  with tempfile.TemporaryDirectory(dir=ROOT/'build')as folder:
   file=Path(folder)/'review.json';file.write_text(json.dumps(value),encoding='utf8')
   with self.assertRaises(ValueError):(d.extract if lower else d.read)(file)
 def bad_blob(self,changes):
  blob=bytearray(self.blob)
  for offset,value in changes:struct.pack_into('<I',blob,offset,value)
  struct.pack_into('<I',blob,16,zlib.crc32(blob[32:]))
  with self.assertRaises(ValueError):d.parse(bytes(blob))
 def test_actual_source_counter_flags_and_stable_nodes(self):
  self.assertEqual((self.data['region'],self.data['initial'],self.data['minimum'],self.data['maximum']),('Catacombs',0,0,3))
  self.assertEqual([r['flag']for r in self.data['nodes']],['Catacombs/Locked Door3','Catacombs/Locked Door1','Catacombs/Locked Door2','Catacombs/Key1','Catacombs/Key2','Catacombs/Key3'])
  self.assertEqual([r['delta']for r in self.data['nodes']],[-1,-1,-1,1,1,1]);self.assertTrue(all(r['flags']==3 for r in self.data['nodes']))
  self.assertEqual(self.data['locked'],'Reusable/locklocked');self.assertEqual(self.proof['commit'],d.PIN)
 def test_actual_source_reproduction_and_canonical_roundtrip(self):
  self.assertEqual(d.extract()[0],self.data);self.assertEqual(d.parse(self.blob),self.data);self.assertEqual(d.encode(d.parse(self.blob)),self.blob)
 def test_unknown_review_fields_versions_and_pin(self):
  for edit in (lambda p:p.update(schema=True),lambda p:p.update(schema=2),lambda p:p.update(commit='0'*40),lambda p:p.update(kind='other'),lambda p:p.update(ignore=True),lambda p:p.pop('sources'),lambda p:p.update(scene_id=0),lambda p:p.update(scene_id=True)):
   with self.subTest(edit=edit):self.candidate(edit)
 def test_unsafe_selectors_and_source_coverage(self):
  for value in ('../escape','/absolute','bad\\path','C:/absolute','bad\npath','a//b'):
   with self.subTest(value=value):self.candidate(lambda p:p['paths'].update(scene=value))
  self.candidate(lambda p:p['sources'].pop(p['paths']['scene']));self.candidate(lambda p:p['sources'].update(extra='0'*64))
 def test_stale_source_unknown_node_and_duplicate_identity(self):
  self.candidate(lambda p:p['sources'].update({p['paths']['key_script']:'0'*64}))
  self.candidate(lambda p:p['nodes'][0].update(id=True));self.candidate(lambda p:p['nodes'][1].update(id=p['nodes'][0]['id']))
  self.candidate(lambda p:p['nodes'][0].update(kind='ignore'));self.candidate(lambda p:p['nodes'][0].update(extra=True))
 def test_source_node_order_coverage_and_script_kind(self):
  for edit in (lambda p:p['nodes'].reverse(),lambda p:p['nodes'].pop(),lambda p:p['nodes'][0].update(kind='key')):
   with self.subTest(edit=edit):self.candidate(edit,True)
 def test_effective_node_unknown_override_and_flag_notification(self):
  original=d.instantiate
  for edit in (lambda n:n.props.update(ignore=True),lambda n:n.props.update(emit_flag_updated_signal=False),lambda n:n.props.update(reset_when_leaving_area=True),lambda n:n.props.update(reset_when_leaving_area=0),lambda n:n.props.update(reset_when_leaving_region=None),lambda n:n.props.update(flag=False),lambda n:n.props.update(flag='Catacombs/Key1')):
   def changed(load,source,edit=edit):
    nodes=copy.deepcopy(original(load,source));edit(next(n for n in nodes if n.path==self.review['nodes'][0]['path']));return nodes
   with self.subTest(edit=edit),patch.object(d,'instantiate',side_effect=changed),self.assertRaises(ValueError):d.extract()
 def test_semantic_source_order_rejected_even_after_detached_admission(self):
  original=d.Extractor.text
  def changed(ex,path):
   value=original(ex,path)
   if path==self.review['paths']['key_script']:value=value.replace('_set_flag_status()','self.visible = false')
   return value
  with patch.object(d.Extractor,'text',changed),self.assertRaisesRegex(ValueError,'source order'):d.extract()
 def test_malformed_json_duplicates_nonfinite_and_boolean_integer(self):
  with tempfile.TemporaryDirectory(dir=ROOT/'build')as folder:
   file=Path(folder)/'bad.json'
   for text in ('{','{"schema":1,"schema":1}','{"schema":NaN}','{"schema":Infinity}'):
    file.write_text(text,encoding='utf8')
    with self.subTest(text=text),self.assertRaises((ValueError,json.JSONDecodeError)):d.read(file)
  for edit in (lambda p:p.update(initial=True),lambda p:p['nodes'][0].update(id=True),lambda p:p['nodes'][0].update(delta=True),lambda p:p['nodes'][0].update(extra=True)):
   value=copy.deepcopy(self.data);edit(value)
   with self.subTest(edit=edit),self.assertRaises(ValueError):d.encode(value)
 def test_unknown_header_capability_pin_and_truncation(self):
  for offset,value in ((8,2),(12,len(self.blob)-1),(20,0),(20,2),(24,0),(24,65536),(28,1),(52,2),(56,1),(60,1),(32,0)):
   with self.subTest(offset=offset,value=value):self.bad_blob([(offset,value)])
  for size in (0,63,111,len(self.blob)-1):
   with self.subTest(size=size),self.assertRaises(ValueError):d.parse(self.blob[:size])
 def test_unknown_node_kind_flags_duplicate_ids_and_counter_delta(self):
  at=struct.unpack_from('<I',self.blob,64+2*16+4)[0];identity=struct.unpack_from('<I',self.blob,at)[0]
  for changes in ([(at+4,0)],[(at+4,3)],[(at+28,2)],[(at+28,4)],[(at+32,identity)],[(at+24,0)],[(at+24,1)],[(at+3*32+24,0xffffffff)]):
   with self.subTest(changes=changes):self.bad_blob(changes)
 def test_counter_bounds_text_spans_and_unsafe_text(self):
  policy=struct.unpack_from('<I',self.blob,64+16+4)[0];at=struct.unpack_from('<I',self.blob,64+2*16+4)[0]
  for changes in ([(policy+20,1)],[(policy+16,4)],[(policy+12,1)],[(at+8,0xffffffff)],[(at+12,0)],[(at+16,0xffffffff)]):
   with self.subTest(changes=changes):self.bad_blob(changes)
  strings=struct.unpack_from('<I',self.blob,64+4)[0]
  for value in (ord(':'),0,255):
   blob=bytearray(self.blob);blob[strings]=value;struct.pack_into('<I',blob,16,zlib.crc32(blob[32:]))
   with self.subTest(value=value),self.assertRaises(ValueError):d.parse(bytes(blob))
 def test_candidate_cli_cannot_write_romfs(self):
  target=ROOT/'romfs/world/catacombs/candidate.encdkeys';self.assertFalse(target.exists())
  with patch.object(d.sys,'argv',['dungeon_keys.py','--out',str(target)]),self.assertRaisesRegex(ValueError,'under build'):d.main()
  self.assertFalse(target.exists())

if __name__=='__main__':unittest.main()
