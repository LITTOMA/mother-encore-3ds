"""Independent source,version,polygon and actual-engine oracle rejection paths."""
import copy,hashlib,json,struct,subprocess,sys,tempfile,unittest,zlib
from pathlib import Path
from tools import catacombs_steps as s,field_footsteps as old,run_catacombs_steps_reference as calls,run_catacombs_step_contacts_reference as contacts
ROOT=Path(__file__).resolve().parents[1];REPORT=ROOT/'reports/catacombs-stepping'

class CatacombsStepsTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.recipe,cls.source,cls.proof=s.source();cls.model=s.binary_model(cls.source);cls.blob=s.encode(cls.model)
 def test_original_geometry_modes_and_inherited_run_sound(self):
  self.assertEqual((len(self.source['sounds']),len(self.source['areas']),sum(len(p['parts'])for a in self.source['areas']for p in a['shapes'])),(8,3,23));self.assertEqual(self.source['default'],'wood');self.assertEqual([len(a['shapes'])for a in self.source['areas']],[11,1,1]);self.assertEqual(self.source['shadows']['animations'][1]['frames'][1]['region'],[30,0,15,5]);self.assertEqual(self.source['shadows']['front'],['ripple']);self.assertEqual(self.recipe['sources'],self.proof['sources']);self.assertEqual(self.proof['source_rules']['shadow'],'ANY PartyObject and nonempty effect,independent of enabled')
 def test_extended_roundtrip_and_both_legacy_rejections(self):
  self.assertEqual(s.parse(self.blob),self.model);self.assertEqual(len(self.blob),2748);self.assertEqual(struct.unpack_from('<I',self.blob,20)[0],2)
  with self.assertRaises(ValueError):old.parse(self.blob)
  _,metadata,_=old.source();legacy=old.encode(metadata)
  self.assertEqual(old.parse(legacy)['default'],metadata['default'])
  with self.assertRaises(ValueError):s.parse(legacy)
 def test_metadata_identity_and_unknown_fields(self):
  for key,value in [('scene',0),('scene',True),('playing',1),('sounds',[]),('areas',[]),('modes',[]),('default','missing'),('shadow','missing'),('slot','unknown path')]:
   bad=copy.deepcopy(self.model);bad[key]=value
   with self.subTest(key=key,value=value),self.assertRaises(ValueError):s.encode(bad)
  bad=copy.deepcopy(self.model);bad['unsupported']=True
  with self.assertRaises(ValueError):s.encode(bad)
 def test_sound_and_shadow_metadata_rejections(self):
  for key,value in [('name','Bad'),('id',0),('id',True),('source','res://../unsafe'),('sha256','0'*64),('loop',False)]:
   bad=copy.deepcopy(self.model);bad['sounds'][0][key]=value
   with self.subTest(key=key),self.assertRaises(ValueError):s.encode(bad)
  for key,value in [('name','Bad'),('fps',0),('fps',float('nan')),('frames',0),('frames',True),('front',1),('loop',0)]:
   bad=copy.deepcopy(self.model);bad['modes'][0][key]=value
   with self.subTest(key=key),self.assertRaises(ValueError):s.encode(bad)
 def test_polygon_and_area_source_domain(self):
  for key,value in [('path','../unsafe'),('id',0),('id',True),('enter','missing'),('exit','missing'),('enter_shadow','missing'),('exit_shadow','missing'),('layer',0),('mask',True),('enabled',1),('parts',[]),('center',[0,0]),('extents',[0,0])]:
   bad=copy.deepcopy(self.model);bad['areas'][0][key]=value
   with self.subTest(key=key),self.assertRaises(ValueError):s.encode(bad)
  for polygon in [[[0,0],[0,0],[1,1]],[[0,0],[1,0],[2,0]],[[0,0],[2,0],[1,1],[2,2],[0,2]],[[0,0],[1,0],[0,float('nan')]],[[0,0],[1,0],[True,1]]]:
   with self.subTest(polygon=polygon),self.assertRaises(ValueError):s.convex(polygon)
 def test_binary_header_and_all_truncations(self):
  for n in range(len(self.blob)):
   with self.subTest(length=n),self.assertRaises(ValueError):s.parse(self.blob[:n])
  for at,value in [(8,2),(12,0),(20,1),(20,3),(24,0),(28,1),(32,0),(52,4),(52,8),(56,1),(60,1),(64,2),(68,0),(72,0),(76,2),(124,64),(140,8),(156,16),(172,16)]:
   with self.subTest(at=at,value=value),self.assertRaises(ValueError):s.parse(self.changed(at,value))
  bad=bytearray(self.blob);bad[-1]^=1
  with self.assertRaises(ValueError):s.parse(bad)
  with self.assertRaises(ValueError):s.parse(self.blob+b'\0')
 def changed(self,at,value):
  bad=bytearray(self.blob);struct.pack_into('<I',bad,at,value);struct.pack_into('<I',bad,16,zlib.crc32(bad[32:]));return bad
 def test_binary_owned_spans_shadow_flags_and_coordinates(self):
  policy,sounds,areas,parts,points,modes=[struct.unpack_from('<I',self.blob,at)[0]for at in[84,100,116,132,148,164]]
  mutations=[(policy,8),(policy+20,2),(sounds,2**32-1),(sounds+48,0),(sounds+52,0),(areas+8,8),(areas+12,8),(areas+16,0x7fc00000),(areas+24,0),(areas+32,2**32-1),(areas+40,1),(areas+48,0),(areas+52,0),(areas+56,2),(areas+60,0),(areas+64,1),(areas+68,0),(areas+68,65),(parts,1),(parts+4,1),(parts+8,2),(parts+12,1),(points,0x7fc00000),(modes+8,0),(modes+12,0),(modes+16,2),(modes+20,2),(modes+24,1),(modes+28,1)]
  for at,value in mutations:
   with self.subTest(at=at,value=value),self.assertRaises(ValueError):s.parse(self.changed(at,value))
 def test_reviewed_source_identity_and_json_duplicates(self):
  with tempfile.TemporaryDirectory(prefix='catsteps-review-',dir=ROOT/'build')as raw:
   directory=Path(raw).resolve();self.assertTrue(directory.is_relative_to(ROOT/'build'));path=directory/'review.json'
   for key,value in [('schema',2),('schema',True),('commit','0'*40),('kind','unknown'),('scene_id',0),('scope','')]:
    bad=copy.deepcopy(self.recipe);bad[key]=value;path.write_text(json.dumps(bad),encoding='utf8')
    with self.subTest(key=key),self.assertRaises(ValueError):s.read(path)
   bad=copy.deepcopy(self.recipe);bad['sources'][bad['scene']]='0'*64;path.write_text(json.dumps(bad),encoding='utf8')
   with self.assertRaises(ValueError):s.read(path)
   path.write_text('{"schema":1,"schema":1}',encoding='utf8')
   with self.assertRaises(ValueError):s.read(path)
 def test_frozen_calls_and_repeated_frame_preservation(self):
  a=(REPORT/'native-calls-4/reference.json').read_bytes();b=(REPORT/'native-calls-5/reference.json').read_bytes();self.assertEqual(a,b);doc=calls.validate_result(json.loads(a));self.assertEqual(len(doc['rows']),60);self.assertEqual(doc['rows'][33]['player']['frame'],1);self.assertEqual(doc['rows'][34]['player']['frame'],1);self.assertEqual(doc['rows'][36]['player']['mode'],'shadow');self.assertEqual(doc['rows'][36]['wanted'],doc['rows'][35]['wanted']);self.assertEqual(doc['rows'][38]['party']['mode'],'ripple')
  for change in ['version','count','action','shadow','frameorder','soundtype']:
   bad=copy.deepcopy(doc)
   if change=='version':bad['schema']=True
   elif change=='count':bad['rows'].pop()
   elif change=='action':bad['rows'][0]['action']=[99]
   elif change=='shadow':bad['rows'][0]['player']['mode']='missing'
   elif change=='frameorder':bad['animations'][1]['frames'].reverse()
   else:bad['rows'][0]['wanted']=True
   with self.subTest(change=change),self.assertRaises(ValueError):calls.validate_result(bad)
 def test_frozen_contacts_and_aabb_false_positives(self):
  a=(REPORT/'native-contacts-4/contacts.json').read_bytes();self.assertEqual(a,(REPORT/'native-contacts-5/contacts.json').read_bytes());receipt=json.loads((REPORT/'native-contacts-4/receipt.json').read_bytes());doc=contacts.validate_result(json.loads(a),receipt);self.assertEqual(len(doc['rows']),164)
  for i,case in enumerate(receipt['cases']):
   row=doc['rows'][i*4+1]
   if case['name'].startswith('water_broad_bounds'):self.assertFalse(any(row['overlaps'][0]))
   if case['name']in['filter_1_0','filter_2_2048','filter_0_2048']:self.assertTrue(row['overlaps'][0][1])
  for change in ['version','count','body','shape','part','event','shadow']:
   bad=copy.deepcopy(doc)
   if change=='version':bad['schema']=2
   elif change=='count':bad['rows'].pop()
   elif change=='body':bad['rows'][0]['bodies'][0]['position'][0]=float('nan')
   elif change=='shape':bad['geometry'][0]['shapes'].pop()
   elif change=='part':bad['geometry'][0]['shapes'][0]['parts'][0][0]=[0,0]
   elif change=='event':bad['rows'][1]['events'][0]['body']=3
   else:bad['rows'][0]['party']['mode']='missing'
   with self.subTest(change=change),self.assertRaises(ValueError):contacts.validate_result(bad,receipt)
 def test_semantic_review_and_native_producer_binding(self):
  review=json.loads((ROOT/'compatibility/reviews/catacombs-stepping-v0410.json').read_bytes());self.assertEqual(review['source'],self.proof)
  for kind,tool,file in [('calls',calls,'reference.json'),('contacts',contacts,'contacts.json')]:
   path=REPORT/('native-'+kind+'-4');receipt=json.loads((path/'receipt.json').read_bytes());self.assertEqual(receipt['producer_sha256'],hashlib.sha256(Path(tool.__file__).read_bytes()).hexdigest());self.assertEqual(review[kind+'_sha256'],hashlib.sha256((path/file).read_bytes()).hexdigest())
 def test_cli_rejects_unscoped_output_before_writing(self):
  target=ROOT/'unscoped-catacombs-steps-candidate';self.assertFalse(target.exists());run=subprocess.run([sys.executable,str(ROOT/'tools/catacombs_steps.py'),'--out',str(target)],stdout=subprocess.PIPE,stderr=subprocess.PIPE);self.assertNotEqual(run.returncode,0);self.assertFalse(target.exists());self.assertIn(b'must stay under build',run.stderr)
if __name__=='__main__':unittest.main()
