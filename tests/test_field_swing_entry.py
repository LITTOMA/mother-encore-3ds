import copy,hashlib,json,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
from tools import field_swing_entry as e,field_swing
from tools.godot_text import Document,NodePathValue

class SwingEntry(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.recipe=e.read();cls.model,cls.proof=e.source();base,ex=field_swing.read();cls.doc=Document(base['scene'],ex.text(base['scene']))
 def test_compiled_original_clips_paths_and_frozen_binary(self):
  blob=e.encode(self.model);self.assertEqual(blob,(e.ROOT/'tests/fixtures/field-swing-entry-v0410.encentry').read_bytes());self.assertEqual(len(blob),4087);self.assertEqual(len(self.model['states']),8);self.assertEqual(len(self.model['clips']),56);self.assertEqual(sum(len(c['keys'])for c in self.model['clips']),169);self.assertTrue(any(c['length']==20 and c['loop']for c in self.model['clips']));self.assertEqual(len(self.proof['excluded_unreachable_keys']),6)
  self.assertEqual(self.proof['paths'],self.recipe['paths']);self.assertEqual(e.source(),(self.model,self.proof));self.assertTrue(all('\\'not in k for k in self.proof['dependencies']))
  for p,sha in self.proof['sources'].items():self.assertEqual(hashlib.sha256((e.ROOT/'upstream/MOTHER-Encore'/p).read_bytes()).hexdigest(),sha)
 def test_complete_native_topology_and_repeated_motion_result(self):
  a=e.ROOT/'reports/iggy-tutorial/swing-entry-reference-4';b=e.ROOT/'reports/iggy-tutorial/swing-entry-reference-5';self.assertEqual((a/'reference.tsv').read_bytes(),(b/'reference.tsv').read_bytes());sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
  for folder in(a,b):
   proof=json.loads((folder/'source.json').read_bytes());self.assertEqual(proof['cases'],80);self.assertEqual(proof['observations'],6560);self.assertEqual(proof['paths'],self.recipe['paths']);self.assertGreater(proof['full_topology_states'],len(self.recipe['states']));self.assertEqual(proof['recipe_sha256'],sha(e.RECIPE))
   for p,digest in proof['outputs'].items():self.assertEqual(sha(folder/p),digest)
   for p,digest in proof['tools'].items():self.assertNotIn('\\',p);self.assertEqual(sha(e.ROOT/p),digest)
  rows=json.loads((a/'reference.json').read_bytes())['rows'];self.assertEqual(len(rows),6560);self.assertTrue(any(r['node']=='Idle'and r['path']==['Bat']for r in rows));self.assertTrue(any(r['node']=='Idle'and r['attack']for r in rows));self.assertEqual({r['node']for r in rows},set(s['name']for s in self.recipe['states']))
 def test_recipe_schema_unknown_states_paths_and_cycles_reject(self):
  edits=[lambda p:p.update(schema=True),lambda p:p.update(schema=2),lambda p:p.update(extra=0),lambda p:p.update(source_recipe='unknown'),lambda p:p.update(action_recipe='unknown'),lambda p:p.update(states=[]),lambda p:p['states'][0].update(id=True),lambda p:p['states'][1].update(id=1),lambda p:p['states'][0].update(name='../escape'),lambda p:p.update(idle=99),lambda p:p.update(attack=1),lambda p:p['paths'].update(Unknown=['Bat']),lambda p:p['paths'].update(Walk=['Unknown','Bat']),lambda p:p['paths'].update(Walk=['Walk','Bat']),lambda p:p['paths'].update(Run=['Walk','Bat']),lambda p:p.update(motion_states=[True,2,3,4]),lambda p:p.update(motion_states=[{},2,3,4]),lambda p:p.update(motion_states=[1,2,3,8]),lambda p:p.update(motion_states=[1,1,3,4]),lambda p:p.update(output='../escape.encentry')]
  with tempfile.TemporaryDirectory()as folder:
   path=Path(folder)/'bad.json'
   for edit in edits:
    p=copy.deepcopy(self.recipe);edit(p);path.write_text(json.dumps(p),encoding='utf8')
    with self.subTest(edit=edit),self.assertRaises(ValueError):e.read(path)
   path.write_text('{"schema":1,"schema":1}',encoding='utf8')
   with self.assertRaises(ValueError):e.read(path)
 def test_valid_graph_route_still_requires_the_actual_native_path(self):
  recipe=copy.deepcopy(self.recipe);recipe['paths']['Crouch']=['Run','Walk','Idle','Bat']
  with tempfile.TemporaryDirectory()as folder:
   path=Path(folder)/'different-valid-route.json';path.write_text(json.dumps(recipe),encoding='utf8');self.assertEqual(e.read(path)['paths'],recipe['paths'])
   with self.assertRaisesRegex(ValueError,'original topology reference differs'):e.source(path)
 def test_model_corruption_graph_spans_clips_and_frames_reject(self):
  edits=[lambda m:m.update(extra=0),lambda m:m['policy'].__setitem__(0,True),lambda m:m['policy'].__setitem__(0,0),lambda m:m['states'][0].update(extra=0),lambda m:m['states'][0].update(next=99),lambda m:m['states'][1].update(next=2),lambda m:m['states'][0].update(first=1),lambda m:m['states'][0].update(count=9),lambda m:m['states'][0].update(name=''),lambda m:m['states'][1].update(name=m['states'][0]['name']),lambda m:m['clips'][0].update(extra=0),lambda m:m['clips'][0].update(state=99),lambda m:m['clips'][0].update(ordinal=1),lambda m:m['clips'][0].update(direction=[0,0]),lambda m:m['clips'][0].update(direction=[True,0]),lambda m:m['clips'][0].update(direction=[float('nan'),0]),lambda m:m['clips'][0].update(length=61),lambda m:m['clips'][0].update(loop=1),lambda m:m['clips'][0].update(main=2),lambda m:m['clips'][0].update(keys=[]),lambda m:m['clips'][0]['keys'][0].update(time=.01),lambda m:m['clips'][0]['keys'][0].update(time=float('inf')),lambda m:m['clips'][0]['keys'][0].update(frame=200),lambda m:m['clips'][0]['keys'][0].update(frame=True)]
  for edit in edits:
   model=copy.deepcopy(self.model);edit(model)
   with self.subTest(edit=edit),self.assertRaises(ValueError):e.encode(model)
 def test_unmapped_original_track_property_or_blend_node_rejects(self):
  edits=[lambda d:d.sub[10][1].update(unknown=0),lambda d:d.sub[10][1].update({'tracks/0/type':'method'}),lambda d:d.sub[10][1].update({'tracks/0/path':NodePathValue('Unknown:value')}),lambda d:d.sub[10][1]['tracks/0/keys'].update(update=0),lambda d:d.sub[10][1]['tracks/0/keys']['values'].__setitem__(0,True),lambda d:d.sub[525][1].update(unknown=0),lambda d:d.sub[525][1]['triangles'].__setitem__(0,99),lambda d:d.sub[524][1].update(unknown=0),lambda d:d.sub[408][1].update(switch_mode=1)]
  for edit in edits:
   doc=copy.deepcopy(self.doc);edit(doc)
   with self.subTest(edit=edit),patch.object(e,'Document',return_value=doc),self.assertRaises(ValueError):e.source()

if __name__=='__main__':unittest.main()
