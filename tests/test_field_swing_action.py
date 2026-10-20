import copy,hashlib,json,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
from tools import field_swing_action as a,run_swing_action_reference as ref,field_swing
from tools.godot_text import Document,NodePathValue

class SwingAction(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.recipe=json.loads(a.RECIPE.read_bytes());cls.model,cls.proof=a.source();cls.cases=ref.read_cases(a.ROOT/'tests/fixtures/swing-action-cases.json');cls.base,cls.ex=field_swing.read();cls.doc=Document(cls.base['scene'],cls.ex.text(cls.base['scene']))
 def test_checked_binary_is_the_frozen_source_model(self):
  self.assertEqual(a.encode(self.model),(a.ROOT/'tests/fixtures/field-swing-action-v0410.encaction').read_bytes());self.assertEqual(self.model['policy'],[1,63,31,4,0,2,1,200]);self.assertEqual(len(self.model['directions']),8);self.assertEqual({r['frame']for r in self.model['directions']},{1,11,21,31,41,51,61,71})
  again,proof=a.source();self.assertEqual((again,proof),(self.model,self.proof));self.assertIn('not a general tree VM',proof['scope'])
  for p,sha in proof['sources'].items():self.assertEqual(hashlib.sha256((a.ROOT/'upstream/MOTHER-Encore'/p).read_bytes()).hexdigest(),sha)
  self.assertTrue(all('\\'not in p for p in proof['dependencies']))
 def test_native_source_methods_and_two_stable_projections(self):
  first=a.ROOT/'reports/iggy-tutorial/swing-action-reference-5';second=a.ROOT/'reports/iggy-tutorial/swing-action-reference-6';proof=json.loads((first/'source.json').read_bytes());doc=json.loads((first/'reference.json').read_bytes());sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
  self.assertEqual(proof['observations'],683);self.assertEqual(len(doc['rows']),683);self.assertEqual(doc['original_process_modes'],[1]*25);self.assertEqual(proof['case_sha256'],sha(a.ROOT/'tests/fixtures/swing-action-cases.json'));self.assertEqual(proof['sources'],self.recipe['sources']);self.assertEqual(proof['symbols'],self.recipe['symbols']);self.assertEqual((first/'reference.tsv').read_bytes(),(second/'reference.tsv').read_bytes())
  for folder in(first,second):
   p=json.loads((folder/'source.json').read_bytes());self.assertEqual(p['case_sha256'],proof['case_sha256']);self.assertEqual(p['observations'],683)
   for name,digest in p['outputs'].items():self.assertEqual(digest,sha(folder/name))
   for name,digest in p['tools'].items():self.assertNotIn('\\',name);self.assertEqual(digest,sha(a.ROOT/name))
  self.assertTrue(any(row['node']=='Idle'and row['attack']for row in doc['rows']));self.assertTrue(any(row['op']['op']=='flush'and not row['attack']for row in doc['rows']));self.assertIn('asynchronous',proof['scope'])
 def test_recipe_versions_unknown_fields_and_source_bindings_reject(self):
  edits=[lambda p:p.update(schema=True),lambda p:p.update(schema=2),lambda p:p.update(extra=0),lambda p:p.update(source_recipe='unknown'),lambda p:p.update(reference='../escape'),lambda p:p.update(idle_state='Bat'),lambda p:p.update(attack_state='../Bat'),lambda p:p.update(input_action='unknown'),lambda p:p.update(skill_id='unknown'),lambda p:p.update(process_mode=True),lambda p:p.update(process_mode=0),lambda p:p.update(output='../bad.encaction'),lambda p:p['sources'].update({'project.godot':'0'*64}),lambda p:p['symbols'].update({'_input':'0'*64})]
  with tempfile.TemporaryDirectory()as folder:
   path=Path(folder)/'bad.json'
   for edit in edits:
    p=copy.deepcopy(self.recipe);edit(p);path.write_text(json.dumps(p),encoding='utf8')
    with self.subTest(edit=edit),self.assertRaises((ValueError,KeyError)):a.source(path)
   path.write_text('{"schema":1,"schema":1}',encoding='utf8')
   with self.assertRaises(ValueError):a.read(path)
 def test_unknown_idle_graph_tracks_and_semantics_reject(self):
  edits=[lambda d:d.sub[76][1].update(start_node='Bat'),lambda d:d.sub[466][1].update(auto_advance=False),lambda d:d.sub[51][1].update(blend_mode=0),lambda d:d.sub[51][1].update(unknown=0),lambda d:d.sub[10][1].update(loop=False),lambda d:d.sub[10][1].update(length=float('nan')),lambda d:d.sub[10][1].update(unknown=0),lambda d:d.sub[10][1].update({'tracks/3/type':'method'}),lambda d:d.sub[10][1].update({'tracks/0/interp':True}),lambda d:d.sub[10][1].update({'tracks/0/enabled':False}),lambda d:d.sub[10][1].update({'tracks/0/path':NodePathValue('Unknown:property')}),lambda d:d.sub[10][1]['tracks/0/keys'].update(update=0),lambda d:d.sub[10][1]['tracks/0/keys']['times'].__setitem__(0,.01),lambda d:d.sub[10][1]['tracks/0/keys']['transitions'].__setitem__(0,.5),lambda d:d.sub[10][1]['tracks/0/keys']['values'].__setitem__(0,200)]
  for edit in edits:
   doc=copy.deepcopy(self.doc);edit(doc)
   with self.subTest(edit=edit),patch.object(a,'Document',return_value=doc),self.assertRaises(ValueError):a.encode(a.source()[0])
 def test_model_masks_dimensions_roles_and_direction_bounds_reject(self):
  edits=[lambda m:m.update(extra=0),lambda m:m['policy'].pop(),lambda m:m['policy'].__setitem__(0,True),lambda m:m['policy'].__setitem__(0,0),lambda m:m['policy'].__setitem__(1,64),lambda m:m['policy'].__setitem__(2,32),lambda m:m['policy'].__setitem__(3,-1),lambda m:m['policy'].__setitem__(4,1),lambda m:m['policy'].__setitem__(5,0),lambda m:m['policy'].__setitem__(6,0),lambda m:m['policy'].__setitem__(7,0),lambda m:m['strings'].__setitem__(0,m['strings'][1]),lambda m:m['strings'].__setitem__(2,'../bad'),lambda m:m['directions'].pop(),lambda m:m['directions'][0].update(extra=0),lambda m:m['directions'][0].update(clip=True),lambda m:m['directions'][1].update(clip=m['directions'][0]['clip']),lambda m:m['directions'][0].update(direction=[0,0]),lambda m:m['directions'][0].update(direction=[float('inf'),1]),lambda m:m['directions'][0].update(frame=200),lambda m:m['directions'][0].update(flags=4)]
  for edit in edits:
   model=copy.deepcopy(self.model);edit(model)
   with self.subTest(edit=edit),self.assertRaises(ValueError):a.encode(model)
 def test_reference_fixture_schema_and_unknown_operations_reject(self):
  edits=[lambda d:d.update(schema=True),lambda d:d.update(extra=0),lambda d:d.update(scope=''),lambda d:d.update(cases=[]),lambda d:d['cases'][0].update(extra=0),lambda d:d['cases'][0].update(state=True),lambda d:d['cases'][0].update(state=99),lambda d:d['cases'][0].update(paused=1),lambda d:d['cases'][0].update(direction=[0,0]),lambda d:d['cases'][0].update(direction=[True,0]),lambda d:d['cases'][0].update(direction=[float('nan'),0]),lambda d:d['cases'][0].update(actions=[]),lambda d:d['cases'][0]['actions'][0].update(op='unknown'),lambda d:d['cases'][0]['actions'][0].update(button=True),lambda d:d['cases'][0]['actions'][0].update(button=32),lambda d:d['cases'][0]['actions'][0].update(pressed=1),lambda d:d['cases'][0]['actions'][1].update(delta=-1),lambda d:d['cases'][0]['actions'][1].update(delta=float('inf')),lambda d:d['cases'][0]['actions'][-1].update(unknown=1)]
  with tempfile.TemporaryDirectory()as folder:
   path=Path(folder)/'bad.json'
   for edit in edits:
    cases=copy.deepcopy(self.cases);edit(cases);path.write_text(json.dumps(cases),encoding='utf8')
    with self.subTest(edit=edit),self.assertRaises(ValueError):ref.read_cases(path)
   path.write_text('{"schema":1,"schema":1}',encoding='utf8')
   with self.assertRaises(ValueError):ref.read_cases(path)

if __name__=='__main__':unittest.main()
