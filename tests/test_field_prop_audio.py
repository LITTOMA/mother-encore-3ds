import copy,json,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
from tools import field_prop_audio as m,field_props as props
from tools.extract_battle_entry import Extractor

class PropAudioSource(unittest.TestCase):
 @classmethod
 def setUpClass(c):
  c.root=props.ROOT;c.config=json.loads((c.root/'content/field-prop-audio.json').read_bytes());c.recipe=props.read();c.data,c.clips,c.proof=props.source();c.actual=m.bindings(c.root);c.extractor=Extractor(c.root)
 def detached(self,value,data=None,recipe=None):
  with tempfile.TemporaryDirectory(dir=self.root/'build')as tmp:
   root=Path(tmp);(root/'content').mkdir();(root/'content/field-prop-audio.json').write_bytes((json.dumps(value)+'\n').encode())
   with patch.object(m.props,'read',return_value=self.recipe if recipe is None else recipe),patch.object(m.props,'source',return_value=(self.data if data is None else data,self.clips,self.proof)),patch.object(m,'Extractor',return_value=self.extractor):return m.bindings(root)
 def test_actual_source_singleton_identity_and_exact_companion(self):
  self.assertEqual(len(self.actual),1);r=self.actual[0]
  self.assertEqual(r['source'],self.data['policy']['sound']);self.assertEqual(r['identity'],self.config['identity']);self.assertEqual(r['pcm'],self.config['pcm']);self.assertEqual((r['gain_db'],r['conversion']),(0,None));self.assertEqual(self.detached(self.config),self.actual)
 def test_unknown_schema_fields_paths_identity_gain_and_permission_reject(self):
  changes=[lambda v:v.update(schema=True),lambda v:v.update(schema=2),lambda v:v.update(kind='unknown'),lambda v:v.update(commit='0'*40),lambda v:v.update(extra=1),lambda v:v.update(props_recipe='../bad.json'),lambda v:v.update(props_recipe='content/../bad.json'),lambda v:v['identity'].update(value=True),lambda v:v['identity'].update(value=0),lambda v:v['identity'].update(kind='resource'),lambda v:v['identity'].update(extra=1),lambda v:v.update(pcm='sound/sfx/bad.pcm'),lambda v:v.update(pcm='sound/effects/../bad.pcm'),lambda v:v.update(gain_db=-1),lambda v:v.update(gain_db=True),lambda v:v.update(conversion={}),lambda v:v.update(license_review='')]
  for change in changes:
   v=copy.deepcopy(self.config);change(v)
   with self.subTest(value=v),self.assertRaises(ValueError):self.detached(v)
 def test_old_unbound_metadata_and_source_digest_mismatch_reject(self):
  with self.assertRaises(ValueError):self.detached(self.config,recipe=dict(self.recipe,schema=2))
  for key,value in [('sound_sha256','0'*64),('bus',0),('pause_mode',1)]:
   data=copy.deepcopy(self.data);data['policy']['contact'][key]=value
   with self.subTest(key=key),self.assertRaises(ValueError):self.detached(self.config,data=data)
 def test_unknown_append_scope_rejected_before_any_conversion(self):
  from tools import append_audio_assets as a
  with tempfile.TemporaryDirectory(dir=self.root/'build')as tmp,patch('sys.argv',['append','--scope','unknown','--out',tmp]),self.assertRaises(SystemExit):a.main()

if __name__=='__main__':unittest.main()
