import copy,json,struct,tempfile,unittest
from pathlib import Path
from tools import catacombs_terrain as t
class TerrainTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.policy,_=t.read();cls.model,cls.atlas,cls.graphics,cls.native,cls.proof=t.source_model();cls.blob=t.encode_source(cls.model,cls.graphics,cls.native,cls.policy)
 def review(self,mutate,model=False):
  with tempfile.TemporaryDirectory(dir=t.ROOT/'build')as tmp:
   p=Path(tmp)/'review.json';v=copy.deepcopy(self.policy);mutate(v);p.write_text(json.dumps(v),encoding='utf-8')
   with self.assertRaises((ValueError,TypeError)):(t.source_model if model else t.read)(p)
 def test_original_native_repeat_and_cells(self):
  a,b=[json.loads((t.ROOT/k).read_text('utf-8'))for k in self.policy['native_oracles']];self.assertEqual(a,b);self.assertEqual(sum(len(v['cells'])for v in a['layers']),12248);self.assertEqual(len(self.model.cells),12247)
 def test_no_playable_actor_program(self):
  scene,sections=t.field.unpack_container(self.graphics,t.MAP_MAGIC,t.field.MAP_STRIDES,t.field.MAP_LIMIT,capability=1);self.assertEqual(scene,self.policy['scene_id']);kinds=[row[0]for row in struct.iter_unpack('<2H2IfI3I',sections[13])];self.assertEqual(set(kinds),{1,2})
  for index in [8,12,15,16,17,18,19,20]:self.assertFalse(sections[index])
 def test_source_metadata_kept(self):
  scene,sections=t.field.unpack_container(self.blob,t.MAGIC,t.STRIDES,t.LIMIT,capability=1);self.assertEqual(len(sections[2]),32);self.assertEqual(len(sections[4])//32,12248);self.assertEqual(len(sections[5])//72,30);self.assertEqual(len(sections[6])//32,1);self.assertEqual(len(sections[7])//8,4);self.assertEqual(len(sections[8])//4,4);self.assertEqual(sections[1],self.graphics)
 def test_review_identity_and_unknown_fields(self):
  for f in [lambda v:v.update(schema=2),lambda v:v.update(extra=1),lambda v:v.update(commit='0'*40),lambda v:v.update(schema=True)]:self.review(f)
 def test_source_sha_and_native_sha_rejected(self):
  self.review(lambda v:v['sources'].update({next(iter(v['sources'])):'0'*64}));self.review(lambda v:v['native_oracles'].update({next(iter(v['native_oracles'])):'0'*64}),model=True)
 def test_active_or_unreviewed_script_rejected(self):
  for key,value in [('can_instance',True),('source_code','extends TileSet'),('methods',['run']),('subresource',153)]:self.review(lambda v,k=key,x=value:v['inactive_script'].update({k:x}),model=key=='subresource')
 def test_no_blanket_missing_cell_ignore(self):
  self.review(lambda v:v.update(missing_cells=[]),model=True);self.review(lambda v:v['missing_cells'][0].update(x=-43),model=True)
 def test_navigation_binding_and_layer_coverage_rejected(self):
  self.review(lambda v:v['navigation'][0].update(tile=25),model=True);self.review(lambda v:v['layers'].pop(),model=True);self.review(lambda v:v['layers'][1].update(id=v['layers'][0]['id']))
 def test_duplicate_and_nonfinite_json_rejected(self):
  with tempfile.TemporaryDirectory(dir=t.ROOT/'build')as tmp:
   p=Path(tmp)/'review.json'
   for text in ['{"schema":1,"schema":1}','{"schema":NaN}']:
    p.write_text(text,encoding='utf-8')
    with self.assertRaises(ValueError):t.read(p)
if __name__=='__main__':unittest.main()
