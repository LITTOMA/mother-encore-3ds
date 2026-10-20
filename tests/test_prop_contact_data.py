import copy,json,struct,tempfile,unittest,zlib
from pathlib import Path
from tools import field_props as p,field_swing as swing,podunk_field as field

class PropContactData(unittest.TestCase):
 @classmethod
 def setUpClass(c):
  c.policy=dict(p.read(),schema=3,contact_recipe='content/field-swing.json',scope='Private source contact metadata test')
  with tempfile.TemporaryDirectory(dir=p.ROOT/'build')as tmp:
   path=Path(tmp)/'policy.json';path.write_text(json.dumps(c.policy),encoding='utf8');c.data,c.clips,c.proof=p.source(path)
  c.action=swing.source();c.model=field.Model(field.load_recipe())
 def test_source_filters_sound_and_owner_identities(self):
  c=self.data['policy']['contact'];action=swing.encode(self.action)
  self.assertEqual((c['target'],c['emitter']),([0,2,1],[2,2,3]))
  self.assertEqual((c['gain_db'],c['pitch'],c['bus'],c['pause_mode']),(0,1,1,0))
  self.assertEqual((c['swing_bytes'],c['swing_crc']),(len(action),zlib.crc32(action)))
  self.assertEqual(c['sound_sha256'],self.proof['sources'][self.data['policy']['sound']])
  self.assertEqual(len({r['notice']for r in self.data['props']}),13)
  self.assertEqual([r['path']for r in self.data['props']],[r['path']for r in self.policy['instances']])
  blob=p.encode(self.data,self.clips,capability=3)
  _,parts=field.unpack_container(blob,p.MAGIC,p.strides(3),p.LIMIT,capability=3)
  self.assertEqual(len(parts[1]),176);self.assertEqual(len(parts[3]),13*116)
  legacy=dict(self.policy,schema=2);legacy.pop('contact_recipe')
  self.assertTrue(set(p.producers(self.policy))>set(p.producers(legacy)))
 def test_legacy_packets_remain_byte_exact(self):
  self.assertEqual(p.encode(self.data,self.clips,capability=1),(p.ROOT/'tests/fixtures/source-field-props.encprops').read_bytes())
  self.assertEqual(p.encode(self.data,self.clips,capability=2),(p.ROOT/'tests/fixtures/source-field-props-cap2.encprops').read_bytes())
 def test_versions_missing_fields_and_unsafe_companions_reject(self):
  mutations=[lambda v:v.update(schema=4),lambda v:v.update(schema=True),lambda v:v.update(schema=2),lambda v:v.pop('contact_recipe'),lambda v:v.update(contact_recipe='../escape.json'),lambda v:v.update(contact_recipe='world/action.json'),lambda v:v.update(contact_recipe='content/action.encswing'),lambda v:v.update(unknown=True)]
  with tempfile.TemporaryDirectory(dir=p.ROOT/'build')as tmp:
   path=Path(tmp)/'policy.json'
   for change in mutations:
    value=copy.deepcopy(self.policy);change(value);path.write_text(json.dumps(value),encoding='utf8')
    with self.subTest(value=value),self.assertRaises(ValueError):p.read(path)
  for cap in (0,4,True):
   with self.subTest(cap=cap),self.assertRaises(ValueError):p.encode(self.data,self.clips,capability=cap)
  value=copy.deepcopy(self.data);del value['policy']['contact']
  with self.assertRaises(ValueError):p.encode(value,self.clips,capability=3)
 def test_unreviewed_collision_and_audio_overrides_reject(self):
  from unittest.mock import patch
  first=self.policy['instances'][0]['path'];hit=self.model.scene.by_path[first+'/Hitbox'];audio=self.model.scene.by_path[first+'/AudioStreamPlayer']
  cases=[(hit,'collision_mask',0),(hit,'collision_layer',1<<20),(hit,'monitoring',False),(hit,'monitorable',1),(hit,'unknown',True),(audio,'bus','Master'),(audio,'pause_mode',1),(audio,'pause_mode',True),(audio,'autoplay',True),(audio,'mix_target',True),(audio,'pitch_scale',float('nan')),(audio,'pitch_scale',0),(audio,'volume_db',129),(audio,'unknown',0)]
  with patch.object(swing,'source',return_value=self.action):
   for node,key,value in cases:
    before=copy.deepcopy(node.props)
    try:
     node.props[key]=value
     with self.subTest(key=key,value=value),self.assertRaises(ValueError):p.contact_source(self.policy,self.model,self.data['policy']['sound'])
    finally:node.props.clear();node.props.update(before)

if __name__=='__main__':unittest.main()
