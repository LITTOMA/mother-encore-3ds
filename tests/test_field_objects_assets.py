import copy,json,struct,tempfile,unittest,zlib
from pathlib import Path
from unittest.mock import patch
from PIL import Image
from tools import field_objects_assets as a
ROOT=Path(__file__).resolve().parents[1]
class ObjectAssetTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.policy,cls.model,cls.canvas,cls.meta,cls.proof=a.source()
 def candidate_policy(self,value,operation=a.read):
  with tempfile.TemporaryDirectory(prefix='object-source-',dir=ROOT/'build')as folder:
   p=Path(folder)/'policy.json';p.write_text(json.dumps(value),encoding='utf8');return operation(p)
 def test_actual_source_visual_ready_and_filtered_body(self):
  m=self.meta;r=m['objects'][0];self.assertEqual((m['width'],m['height'],m['ready_state']),(32,32,621950984495336987));self.assertEqual((r['initial'],r['resolved'],r['fps'],r['range']),(22,6,12,[0,47]));self.assertEqual(r['position'],[1432,4664]);self.assertEqual((r['body'],r['body_layer'],r['body_mask'],r['body_extents']),(0xffffffff,4,0,[9,8]));self.assertEqual(len(r['sequence']),40);self.assertEqual(len(m['frames']),6);self.assertEqual(r['flags'],511)
 def test_each_packed_crop_preserves_source_rgba(self):
  for key,rect in zip(self.proof['images'],self.meta['frames']):
   name,x,y,w,h=key
   with Image.open(ROOT/'upstream/MOTHER-Encore'/name)as im:expected=im.convert('RGBA').crop((x,y,x+w,y+h)).tobytes()
   self.assertEqual(self.canvas.crop((rect['x'],rect['y'],rect['x']+rect['width'],rect['y']+rect['height'])).tobytes(),expected)
 def test_encoder_checks_texture_receipt_and_new_capability(self):
  raw=b'Explicit synthetic CRC fixture';blob=a.encode(self.meta,raw);self.assertEqual(struct.unpack_from('<I',blob,20)[0],3);self.assertEqual(struct.unpack_from('<2I',blob,56),(len(raw),zlib.crc32(raw)));self.assertEqual(struct.unpack_from('<I',blob,16)[0],zlib.crc32(blob[32:]));self.assertEqual(struct.unpack_from('<I',blob,108)[0],136)
  for value in(b'',None,bytearray(b'bad'),b'x'*(1024*1024+1)):
   with self.subTest(value=type(value)),self.assertRaises(ValueError):a.encode(self.meta,value)
 def test_unknown_missing_or_bad_policy_fields_rejected(self):
  edits=[('schema',2),('schema',True),('commit','0'*40),('scope',''),('texture','../a.t3x'),('output','graphics/a.t3x'),('atlas_size',[31,32]),('atlas_size',[True,32]),('objects',[]),('sources',{}),('unknown',0)]
  for k,v in edits:
   p=copy.deepcopy(self.policy);p[k]=v
   with self.subTest(k=k,v=v),self.assertRaises(ValueError):self.candidate_policy(p)
  for k in self.policy:
   p=copy.deepcopy(self.policy);del p[k]
   with self.subTest(missing=k),self.assertRaises(ValueError):self.candidate_policy(p)
 def test_duplicate_bad_ids_paths_and_source_hash_rejected(self):
  for key,value in [('id',True),('id',0),('id',0xffffffff),('path','/absolute'),('effect','../escape')]:
   p=copy.deepcopy(self.policy);p['objects'][0][key]=value
   with self.subTest(key=key,value=value),self.assertRaises(ValueError):self.candidate_policy(p)
  p=copy.deepcopy(self.policy);p['objects']*=2
  with self.assertRaises(ValueError):self.candidate_policy(p)
  p=copy.deepcopy(self.policy);p['sources'][p['scripts']['object']]='0'*64
  with self.assertRaisesRegex(ValueError,'reviewed source changed'):self.candidate_policy(p,a.source)
 def test_duplicate_and_nonfinite_json_rejected(self):
  with tempfile.TemporaryDirectory(prefix='object-json-',dir=ROOT/'build')as folder:
   file=Path(folder)/'bad.json'
   for text in('{"schema":1,"schema":1}','{"schema":NaN}'):
    file.write_text(text,encoding='utf8')
    with self.assertRaises(ValueError):a.read(file)
 def test_unknown_render_and_flag_overrides_fail_closed(self):
  path=self.policy['objects'][0]['path']
  for target,key,value in[(path,'is_object_flag',True),(path,'reset_when_consumed',True),(path,'modulate','unmapped'),(path+'/Sprite','flip_h',True),(path+'/Sparkles','visible',False)]:
   model=copy.copy(self.model);model.scene=copy.copy(self.model.scene);model.scene.by_path=dict(self.model.scene.by_path);node=copy.copy(model.scene.by_path[target]);node.props=dict(node.props);node.props[key]=value;model.scene.by_path[target]=node
   with self.subTest(target=target,key=key),patch.object(a.f,'Model',return_value=model),self.assertRaises(ValueError):a.source()
 def test_missing_visual_child_and_atlas_capacity_rejected(self):
  for key,value in [('sprite','absent'),('effect','absent'),('body_shape','absent')]:
   p=copy.deepcopy(self.policy);p['objects'][0][key]=value
   with self.subTest(key=key),self.assertRaises(ValueError):self.candidate_policy(p,a.source)
  p=copy.deepcopy(self.policy);p['atlas_size']=[8,8]
  with self.assertRaisesRegex(ValueError,'atlas capacity'):self.candidate_policy(p,a.source)
 def test_receipt_roundtrip_and_stale_source_or_output_rejected(self):
  raw=b'Explicit in-memory receipt fixture';blob=a.encode(self.meta,raw);receipt=dict(self.proof,schema=1,kind='encore.field-objects.assets',meta=self.meta,tex3ds_sha256='a'*64,outputs={self.policy['texture']:a.output_record(raw),self.policy['output']:a.output_record(blob)})
  def check(value):
   with patch.object(a.Path,'read_text',return_value=json.dumps(value)),patch.object(a.Path,'read_bytes',return_value=raw):return a.checked_receipt(self.policy,self.meta,self.proof,ROOT/'romfs')
  self.assertEqual(check(receipt),(raw,blob))
  for key,value in [('schema',2),('schema',True),('kind','unknown'),('tex3ds_sha256','bad'),('outputs',{}),('producers',{}),('meta',{}),('unknown',1)]:
   other=copy.deepcopy(receipt);other[key]=value
   with self.subTest(key=key),self.assertRaises(ValueError):check(other)
  other=copy.deepcopy(receipt);other['outputs'][self.policy['texture']]['bytes']+=1
  with self.assertRaises(ValueError):check(other)
if __name__=='__main__':unittest.main()
