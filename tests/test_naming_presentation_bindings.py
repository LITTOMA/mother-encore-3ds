#!/usr/bin/env python3
"""Default naming source/data equivalence and fail-closed recipe compilation."""
from pathlib import Path
import copy,hashlib,json,struct,sys,unittest
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import new_game_assets as assets
from tools import naming_presentation_bindings as recipe

def candidate():
 ex,r,_=assets.base();old=json.loads((assets.ROOT/'content/native-new-game.json').read_text(encoding='utf-8'))
 for k in ('rects','font_height','native_layout_sha256','outputs','resources'):r[k]=copy.deepcopy(old[k])
 for panel in r['panels']:
  for key,neighbors,rect in zip(panel,assets.graph(panel,r['groups'],r['columns'],r['rects']),r['rects']):key.update(neighbors=neighbors,rect=rect)
 assets.resolve_bindings(ex,r);return r,old

def prepare_fixtures():
 r,_=candidate();assets.verify(r);path=assets.ROOT/'build/naming-bindings-fixtures';path.mkdir(parents=True,exist_ok=True);(path/'default.encnewgame').write_bytes(assets.encode(r));return path

class NamingBindingsTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.config,cls.facts,cls.binding=recipe.load(assets.ROOT);cls.r,cls.old=candidate()
 def reject(self,change):
  c=copy.deepcopy(self.config);change(c)
  with self.assertRaises(ValueError):recipe.load(assets.ROOT,document=c)
 def test_original_ir_and_payload(self):
  for k in self.old:
   if k not in ('sources','dependencies','schema','presentation','presentation_sha256'):self.assertEqual(self.old[k],self.r[k],k)
  raw=assets.encode(self.r);self.assertEqual((3,2),(struct.unpack_from('<I',raw,8)[0],struct.unpack_from('<I',raw,20)[0]))
  # Reviewed original gameplay payload; source/data metadata changes header/tail only.
  self.assertEqual(hashlib.sha256(raw[24:-116]).hexdigest(),'bc79853503a565c772a4316e8fa7d6ce9e320b66a11c47052397141f555f0395')
  assets.verify(self.r)
 def test_source_tuning(self):
  self.assertEqual(self.facts['error_duration'],2.0);self.assertEqual(self.facts['navigation_point'],[8-8/6,1+4]);self.assertEqual(self.facts['dim_color'],0xff866c7a);self.assertEqual(self.binding['source_width'],320)
 def test_unknown_missing_version(self):
  self.reject(lambda c:c.update(unknown=0));self.reject(lambda c:c.pop('routing'));self.reject(lambda c:c.update(schema=2));self.reject(lambda c:c.update(schema=True))
 def test_source_review_and_fingerprint(self):
  self.reject(lambda c:c['sources'].update({c['paths']['script']:'0'*64}))
  self.reject(lambda c:c['review'][c['paths']['script']].update(_highlight_color='func _highlight_color(): pass\n'))
 def test_unknown_action_condition_and_ref(self):
  self.reject(lambda c:c['routing'][0][0].update(op='execute'))
  self.reject(lambda c:c['routing'][1][2].update(condition='eval'))
  self.reject(lambda c:c['routing'][0][0].update(group=99))
 def test_duplicate_paths_roles_groups(self):
  self.reject(lambda c:c['groups'].__setitem__(1,c['groups'][0]));self.reject(lambda c:c['presentation']['sounds'].__setitem__(1,0));self.reject(lambda c:c['panel_paths'].__setitem__(1,c['panel_paths'][0]))
 def test_unsafe_paths_and_source_commands(self):
  self.reject(lambda c:c['paths'].update(scene='../bad'));self.reject(lambda c:c['commands'][0].update(text='MENU_OK'))
 def test_stale_ir_compile_gate(self):
  for change in (lambda r:r['presentation'].update(cursor=99),lambda r:r.update(schema=2),lambda r:r.update(presentation_sha256='0'*64),lambda r:r.update(error_duration=.8)):
   r=copy.deepcopy(self.r);change(r)
   with self.assertRaises(ValueError):assets.encode(r)
  r=copy.deepcopy(self.r);r['unknown']=0
  with self.assertRaises(ValueError):assets.verify(r)
 def test_duplicate_json(self):
  with self.assertRaises(ValueError):json.loads('{"schema":1,"schema":1}',object_pairs_hook=recipe.pairs)
if __name__=='__main__':
 if '--prepare-only'in sys.argv:print(prepare_fixtures())
 else:unittest.main()
