import copy,json,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
from tools import field_props as p
from tools.run_prop_lifecycle_reference import cases,write_tsv
ROOT=Path(__file__).resolve().parents[1]

class FieldProps(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.policy=p.read();cls.data,cls.clips,cls.proof=p.source()
 def test_source_instances_and_bytes_match_fixture(self):
  self.assertEqual(p.encode(self.data,self.clips),(ROOT/'tests/fixtures/source-field-props.encprops').read_bytes())
  r=self.data;self.assertEqual([v['id']for v in r['props']],list(range(1,14)))
  self.assertEqual(r['flags'],['bat','cem_roots_grown','got_shovel']);self.assertEqual(sum(v['appear']!=p.f.NONE for v in r['props']),5)
  self.assertEqual([v['id']for v in r['props']if v['callback']!=p.f.NONE],[12]);self.assertEqual(r['props'][11]['finish'],2)
  self.assertEqual((r['policy']['width'],r['policy']['height']),(48,48));self.assertEqual(r['policy']['rumble'],[0,.5,.7,.8])
  self.assertEqual({r['clone_item']for r in r['props']},{8});self.assertEqual(self.proof['commit'],p.f.PIN)
 def test_unknown_recipe_fields_versions_paths_and_identities_reject(self):
  mutations=[lambda v:v.update(schema=True),lambda v:v.update(schema=2),lambda v:v.update(unknown=True),lambda v:v.update(commit='0'*40),lambda v:v.update(scope=False),lambda v:v.update(texture='../bad.t3x'),lambda v:v.update(texture='C:/bad.t3x'),lambda v:v.update(texture='sound/bad.t3x'),lambda v:v.update(map_sha256='x'*64),lambda v:v['instances'][0].update(id=0),lambda v:v['instances'][0].update(id=True),lambda v:v['instances'][1].update(id=1),lambda v:v['instances'][1].update(path=v['instances'][0]['path']),lambda v:v['callbacks'][0].update(method='unknown')]
  with tempfile.TemporaryDirectory()as folder:
   path=Path(folder)/'recipe.json'
   for mutate in mutations:
    v=copy.deepcopy(self.policy);mutate(v);path.write_bytes(json.dumps(v).encode())
    with self.subTest(recipe=v),self.assertRaises(ValueError):p.read(path)
   path.write_bytes(b'{"schema":1,"schema":1}')
   with self.assertRaises(ValueError):p.read(path)
 def test_incomplete_coverage_or_unreviewed_map_and_callback_reject(self):
  for mutate in (lambda v:v['instances'].pop(),lambda v:v['instances'].reverse(),lambda v:v.update(map_sha256='0'*64),lambda v:v['callbacks'][0].update(path='Unknown/Target')):
   v=copy.deepcopy(self.policy);mutate(v)
   with self.subTest(recipe=v),patch.object(p,'read',return_value=v),self.assertRaises(ValueError):p.source()
 def test_controlled_native_fixture_has_all_cases_and_preserves_input(self):
  lines=(ROOT/'tests/fixtures/prop-lifecycle-native.tsv').read_text().splitlines();self.assertEqual(sum(v.startswith('start ')for v in lines),14);self.assertEqual(sum(v.startswith('step ')for v in lines),86)
  self.assertEqual(len(cases()),14);self.assertIn('step advance 0.5 0 1 7 ', '\n'.join(lines))
 def test_native_transport_rejects_unknown_callback_and_geometry(self):
  config={'cases':[dict(name='one',actions=[],flags={'bat':False,'cem_roots_grown':False,'got_shovel':False},flag='',callback_flag='',callback=False,function=False)]}
  base=dict(case='one',action=dict(op='ready'),alive=True,clones=[],calls=[],rumble=[],dialogue=[],props={})
  with tempfile.TemporaryDirectory()as folder:
   path=Path(folder)/'trace.tsv'
   for mutate in (lambda r:r.update(case='wrong'),lambda r:r.update(clones=[dict(frame=1,visible=True,x=0,y=0)]),lambda r:r.update(calls=[dict(alive=False,queued=True)]),lambda r:r.update(dialogue=['Unknown'])):
    r=copy.deepcopy(base);mutate(r)
    with self.assertRaises(ValueError):write_tsv(config,dict(rows=[r]),path)

if __name__=='__main__':unittest.main()
