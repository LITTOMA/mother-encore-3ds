"""Source/format/clock-oracle negatives;no GPU or complete player assertion."""
import copy,hashlib,json,struct,tempfile,unittest,zlib
from pathlib import Path
from tools import catacombs_shadow_graphics as g,run_shadow_clock_reference as native
ROOT=Path(__file__).resolve().parents[1];REPORT=ROOT/'reports/catacombs-shadow-graphics'
class ShadowGraphicsTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.recipe,cls.model,cls.atlas,cls.proof=g.build();cls.model['texture_bytes']=35;cls.model['texture_crc']=123;cls.blob=g.encode(cls.model)
 def test_original_child_and_untrimmed_frame_order(self):
  self.assertEqual(self.atlas.size,(68,14));self.assertEqual(self.model['position'],[0,7]);self.assertEqual(self.model['flags'],25);self.assertEqual(len(self.model['frames']),7);self.assertEqual([r['source_region'][0]for r in self.proof['source_frames'][1:4]],[0,30,15]);self.assertEqual([m['fps']for m in self.model['modes']],[5,5,8])
 def test_roundtrip_and_exact_resource_size(self):self.assertEqual(g.parse(self.blob),self.model);self.assertEqual(len(self.blob),564)
 def test_metadata_owner_and_texture_rejections(self):
  for k,v in [('owner',0),('owner',True),('texture','../bad.t3x'),('texture','/bad.t3x'),('texture','C:/bad.t3x'),('width',0),('height',1025),('texture_bytes',0),('texture_crc',-1),('position',[float('nan'),0]),('offset',[0,float('inf')]),('flags',32),('default',3),('playing',1),('modes',[]),('frames',[])]:
   bad=copy.deepcopy(self.model);bad[k]=v
   with self.subTest(key=k),self.assertRaises(ValueError):g.encode(bad)
 def test_mode_and_frame_rejections(self):
  for k,v in [('name','Invalid'),('fps',0),('fps',float('nan')),('fps',1.401298464324817e-45),('first',1),('count',0),('front',1),('loop',0)]:
   bad=copy.deepcopy(self.model);bad['modes'][0][k]=v
   with self.subTest(key=k),self.assertRaises(ValueError):g.encode(bad)
  for k,v in [('mode',1),('index',1),('x',68),('y',14),('width',0),('height',0),('width',True)]:
   bad=copy.deepcopy(self.model);bad['frames'][0][k]=v
   with self.subTest(key=k),self.assertRaises(ValueError):g.encode(bad)
 def changed(self,at,value):
  bad=bytearray(self.blob);struct.pack_into('<I',bad,at,value);struct.pack_into('<I',bad,16,zlib.crc32(bad[32:]));return bad
 def test_all_binary_truncations_and_header_versions(self):
  for n in range(len(self.blob)):
   with self.subTest(size=n),self.assertRaises(ValueError):g.parse(self.blob[:n])
  for at,value in[(8,2),(12,0),(20,2),(24,0),(24,2**32-1),(28,1),(32,0),(52,3),(56,1),(60,1),(64,2),(68,0),(72,0),(76,2),(92,32),(108,64),(124,16)]:
   with self.subTest(at=at,value=value),self.assertRaises(ValueError):g.parse(self.changed(at,value))
 def test_binary_owned_modes_and_flags(self):
  c,m,f=[struct.unpack_from('<I',self.blob,at)[0]for at in[84,100,116]]
  for at,value in[(c,2**32-1),(c+8,0),(c+12,1025),(c+16,0),(c+24,0x7fc00000),(c+40,32),(c+44,3),(c+48,2),(c+52,1),(m+8,1),(m+12,1),(m+16,0),(m+20,4),(m+24,1),(m+28,1),(f,1),(f+4,1),(f+8,68),(f+12,14),(f+16,0),(f+20,0),(f+24,1),(f+28,1)]:
   with self.subTest(at=at),self.assertRaises(ValueError):g.parse(self.changed(at,value))
 def test_duplicate_modes_and_orphan_frames(self):
  bad=copy.deepcopy(self.model);bad['modes'][1]['name']=bad['modes'][0]['name']
  with self.assertRaises(ValueError):g.encode(bad)
  bad=copy.deepcopy(self.model);bad['frames'].append(copy.deepcopy(bad['frames'][-1]))
  with self.assertRaises(ValueError):g.encode(bad)
 def test_native_three_profiles_repeat_semantics(self):
  for label in['60','10','no-draw']:
   docs=[]
   for n in[1,2]:
    p=REPORT/('native-'+label+'-'+str(n));proof=json.loads((p/'receipt.json').read_bytes());doc=native.validate_result(json.loads((p/'reference.json').read_bytes()),proof);self.assertEqual(len(doc['rows']),128)
    for row in doc['rows']:row.pop('drawn_frames')
    docs.append(doc)
   self.assertEqual(*docs)
 def test_hidden_and_parent_pause_continue_tree_pause_stops(self):
  d=json.loads((REPORT/'native-60-1/reference.json').read_bytes());self.assertFalse(d['rows'][36]['visible']);self.assertNotEqual(d['rows'][36]['frame'],d['rows'][47]['frame']);self.assertTrue(d['rows'][50]['player_pause']);self.assertTrue(d['rows'][50]['process_enabled']);self.assertTrue(all(not r['process_enabled']for r in d['rows'][70:80]));self.assertEqual(len({r['frame']for r in d['rows'][69:80]}),1)
 def test_disabled_render_loop_does_not_pause_game_clock(self):
  normal=json.loads((REPORT/'native-60-1/reference.json').read_bytes());nodraw=json.loads((REPORT/'native-no-draw-1/reference.json').read_bytes());self.assertTrue(nodraw['no_draw']);self.assertEqual([r['frame']for r in normal['rows']],[r['frame']for r in nodraw['rows']]);self.assertEqual({r['drawn_frames']for r in nodraw['rows']},{0})
 def test_native_result_version_and_domain_rejections(self):
  p=REPORT/'native-60-1';proof=json.loads((p/'receipt.json').read_bytes());doc=json.loads((p/'reference.json').read_bytes())
  for change in['version','count','delta','mode','frame','gate','defaults']:
   bad=copy.deepcopy(doc)
   if change=='version':bad['schema']=True
   elif change=='count':bad['rows'].pop()
   elif change=='delta':bad['rows'][0]['delta_bits']+=1
   elif change=='mode':bad['rows'][0]['mode']='missing'
   elif change=='frame':bad['rows'][0]['frame']=99
   elif change=='gate':bad['rows'][70]['process_enabled']=True
   else:bad['initial']['offset']=[1,0]
   with self.subTest(change=change),self.assertRaises(ValueError):native.validate_result(bad,proof)
 def test_producer_review_and_official_engine_pins(self):
  review=json.loads((ROOT/'compatibility/reviews/shadow-graphics-v0410.json').read_bytes());self.assertEqual(review['source'],self.proof);self.assertEqual(review['native_producer_sha256'],hashlib.sha256(Path(native.__file__).read_bytes()).hexdigest());self.assertEqual(self.recipe['engine_commit'],'3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8')
 def test_source_recipe_negative_paths_and_duplicate_fields(self):
  with tempfile.TemporaryDirectory(prefix='shadow-source-review-',dir=ROOT/'build')as raw:
   directory=Path(raw).resolve();self.assertTrue(directory.is_relative_to(ROOT/'build'));path=directory/'review.json'
   for key,value in [('schema',2),('schema',True),('kind','unknown'),('commit','0'*40),('owner_id',0),('owner_id',self.recipe['owner_id']+1),('atlas_columns',0),('output','../out.encshadow'),('texture','../bad.t3x'),('source_recipe_sha256','0'*64),('engine_commit','0'*40),('engine_sources',{}),('engine_license_sha256','0'*64),('scope','')]:
    data=copy.deepcopy(self.recipe);data[key]=value;path.write_text(json.dumps(data),encoding='utf8')
    with self.subTest(key=key),self.assertRaises(ValueError):g.read(path)
   path.write_text('{"schema":1,"schema":1}',encoding='utf8')
   with self.assertRaises(ValueError):g.read(path)
if __name__=='__main__':unittest.main()
