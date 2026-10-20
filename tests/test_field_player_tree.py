import copy,hashlib,json,struct,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
from tools import field_player_tree as t,podunk_field as f,field_swing
from tools.godot_text import Document

class PlayerTree(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.model,cls.proof=t.source();p,ex=field_swing.read();cls.doc=Document(p['scene'],ex.text(p['scene']))
 def test_original_graph_pause_policy_and_both_repeated_path_queries(self):
  model=self.model;self.assertEqual(len(model['routes']),64);self.assertEqual(self.proof['automatic_edges'],{'Bat':'Idle'});self.assertEqual([r['state']for r in model['flags']if r['pausable']],[2,3,7]);self.assertEqual(model['incapacitated_idle'],6)
  a=t.ROOT/'reports/iggy-tutorial/player-state-paths-1';b=t.ROOT/'reports/iggy-tutorial/player-state-paths-2';self.assertEqual((a/'reference.json').read_bytes(),(b/'reference.json').read_bytes());sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
  for folder in(a,b):
   proof=json.loads((folder/'source.json').read_bytes());self.assertTrue(proof['closed']);self.assertTrue(proof['imported_methods_not_executed']);self.assertEqual(proof['routes_each'],64)
   for name,digest in proof['sources'].items():self.assertEqual(sha(t.ROOT/'upstream/MOTHER-Encore'/name),digest)
   for name,digest in proof['outputs'].items():self.assertEqual(sha(folder/name),digest)
   for name,digest in proof['tools'].items():self.assertEqual(sha(t.ROOT/name),digest)
  for name,digest in self.proof['dependencies'].items():self.assertNotIn('\\',name);self.assertEqual(sha(t.ROOT/name),digest)
 def test_capability_two_keeps_original_frame_payloads(self):
  blob=t.encode(self.model);self.assertEqual(len(blob),5128);self.assertEqual(struct.unpack_from('<I',blob,20)[0],2)
  _,sections=f.unpack_container(blob,t.entry.MAGIC,t.STRIDES,t.entry.LIMIT,capability=2);_,old=f.unpack_container(t.entry.encode(self.model['entry']),t.entry.MAGIC,t.entry.STRIDES,t.entry.LIMIT)
  self.assertEqual(sections[2:5],old[2:5]);self.assertEqual(sections[0][:28],old[0][:28]);self.assertEqual(sections[0][32:],old[0][32:]);self.assertEqual(len(sections[5]),64*16)
  with self.assertRaises(ValueError):f.unpack_container(blob,t.entry.MAGIC,t.STRIDES,t.entry.LIMIT)
  with self.assertRaises(ValueError):f.unpack_container(blob,t.entry.MAGIC,t.STRIDES,t.entry.LIMIT,capability=3)
 def test_bad_bindings_versions_roles_and_reference_paths(self):
  edits=[lambda p:p.update(extra=0),lambda p:p.update(schema=True),lambda p:p.update(schema=2),lambda p:p.update(entry_recipe='missing'),lambda p:p.update(incapacitated_idle=True),lambda p:p.update(incapacitated_idle=0),lambda p:p.update(path_reference='../escape'),lambda p:p.update(pause_reference=[])]
  with tempfile.TemporaryDirectory()as folder:
   path=Path(folder)/'bad.json'
   for edit in edits:
    model=t.read();edit(model);path.write_text(json.dumps(model),encoding='utf8')
    with self.subTest(edit=edit),self.assertRaises(ValueError):t.read(path)
   path.write_text('{"schema":1,"schema":1}',encoding='utf8')
   with self.assertRaises(ValueError):t.read(path)
 def test_bad_route_coverage_cycles_switches_roles_and_state_flags(self):
  edits=[lambda m:m.update(extra=0),lambda m:m.update(incapacitated_idle=True),lambda m:m.update(incapacitated_idle=1),lambda m:m.update(incapacitated_idle=8),lambda m:m.update(incapacitated_idle=99),lambda m:m.update(flags=[]),lambda m:m.update(routes=[]),lambda m:m['flags'][0].update(extra=0),lambda m:m['flags'][0].update(state=True),lambda m:m['flags'][0].update(automatic=True),lambda m:m['flags'][0].update(automatic=1),lambda m:m['flags'][0].update(pausable=1),lambda m:m['flags'][7].update(pausable=True),lambda m:m['routes'][0].update(extra=0),lambda m:m['routes'][0].update(from_state=True),lambda m:m['routes'][0].update(next=1),lambda m:m['routes'][1].update(next=0),lambda m:m['routes'][1].update(next=1),lambda m:m['routes'][1].update(switch=1),lambda m:m['routes'].__setitem__(1,m['routes'][0]),lambda m:m['routes'][1].update(next=3)]
  for edit in edits:
   model=copy.deepcopy(self.model);edit(model)
   with self.subTest(edit=edit),self.assertRaises(ValueError):t.encode(model)
 def test_original_edge_policy_unknown_property_rejected(self):
  for edit in(lambda d:d.sub[408][1].update(unknown=0),lambda d:d.sub[408][1].update(switch_mode=1),lambda d:d.sub[408][1].update(auto_advance=1)):
   doc=copy.deepcopy(self.doc);edit(doc)
   with self.subTest(edit=edit),patch.object(t,'Document',return_value=doc),self.assertRaises(ValueError):t.source()

if __name__=='__main__':unittest.main()
