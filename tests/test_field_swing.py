import copy,json,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
from tools import field_swing as c
from tools.godot_text import Document,NodePathValue
ROOT=Path(__file__).resolve().parents[1]

class FieldSwingTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.recipe,cls.ex=c.read();cls.source=c.source();cls.doc=Document(cls.recipe['scene'],cls.ex.text(cls.recipe['scene']))
 def test_actual_eight_bindings_and_frozen_candidate(self):
  r=self.source;self.assertEqual(len(r['clips']),8)
  self.assertEqual([x['name']for x in r['clips']],['Attack Down','Attack Up','Attack Right','Attack Left','Attack DownRight','Attack DownLeft','Attack UpRight','Attack UpLeft'])
  ap=next(n for n in self.doc.nodes if n.attrs['name']=='AnimationPlayer');self.assertEqual(ap.props['anims/Attack UpRight'].id,550)
  right=next(c for c in r['clips']if c['name']=='Attack Right');self.assertEqual(next(k['time']for k in right['keys']if k['kind']==1 and k['value']==21),.067)
  self.assertEqual(c.encode(r),(ROOT/'tests/fixtures/field-swing-v0410.encswing').read_bytes())
  self.assertEqual(r['strings'][-1],'bat');self.assertEqual(r['real'][:2],[1,.7])
  self.assertEqual(r['real'][13:15],[10.4999,17]);self.assertEqual(r['real'][15:],[0,.35,0,.2])
 def test_recipe_schema_sources_and_identities_reject(self):
  changes=[lambda p:p.update(schema=True),lambda p:p.update(schema=2),lambda p:p.update(kind='unknown'),lambda p:p.update(commit='0'*40),lambda p:p.update(unknown=0),lambda p:p.update(scope=''),lambda p:p['clips'][1].update(id=1),lambda p:p['clips'][0].update(id=True),lambda p:p['clips'][1].update(name=p['clips'][0]['name']),lambda p:p['tracks'][0].update(kind=True),lambda p:p['tracks'][0].update(kind=9),lambda p:p['tracks'][1].update(path=p['tracks'][0]['path']),lambda p:p['sources'].update({p['script']:'0'*64})]
  with tempfile.TemporaryDirectory()as folder:
   path=Path(folder)/'recipe.json'
   for change in changes:
    p=copy.deepcopy(self.recipe);change(p);path.write_text(json.dumps(p),encoding='utf8')
    with self.subTest(change=change),self.assertRaises(ValueError):c.read(path)
   path.write_text('{"schema":1,"schema":1}',encoding='utf8')
   with self.assertRaises(ValueError):c.read(path)
 def test_source_tracks_modes_callbacks_and_geometry_reject(self):
  def source(doc):
   with patch.object(c,'Document',side_effect=lambda name,raw:doc if name==self.recipe['scene']else Document(name,raw)):return c.source()
  self.assertEqual(c.encode(source(copy.deepcopy(self.doc))),c.encode(self.source))
  def node(d,name,parent):return next(n for n in d.nodes if n.attrs['name']==name and n.attrs.get('parent')==parent)
  changes=[lambda d:d.sub[550][1].update(loop=True),lambda d:d.sub[550][1].update(length=0),lambda d:d.sub[550][1].update(unknown=1),lambda d:d.sub[550][1].update({'tracks/0/interp':True}),lambda d:d.sub[550][1].update({'tracks/00/type':'value'}),lambda d:d.sub[550][1].update({'tracks/0/path':NodePathValue('Unknown:value')}),lambda d:d.sub[550][1]['tracks/0/keys'].update(update=0),lambda d:d.sub[550][1]['tracks/0/keys']['times'].__setitem__(1,0),lambda d:d.sub[550][1]['tracks/0/keys']['values'].__setitem__(0,100),lambda d:d.sub[550][1]['tracks/1/keys']['values'].__setitem__(0,{'args':[],'method':'unknown'}),lambda d:d.sub[550][1]['tracks/3/keys']['values'].__setitem__(0,1),lambda d:node(d,'SpecialAnimations','.').props.update(frame=100),lambda d:node(d,'SpecialAnimations','.').props.update(visible=1),lambda d:node(d,'AnimationPlayer','.').props.update(method_call_mode=1),lambda d:d.sub[80][1].update(radius=-1)]
  for change in changes:
   doc=copy.deepcopy(self.doc);change(doc)
   with self.subTest(change=change),self.assertRaises(ValueError):source(doc)
 def test_native_fixture_transport_and_scope(self):
  base=ROOT/'reports/graveyard-import/field-swing';receipt=json.loads((base/'receipt.json').read_bytes());reference=json.loads((base/'reference.json').read_bytes());lines=(ROOT/'tests/fixtures/field-swing-v0410.tsv').read_text().splitlines()
  self.assertEqual(receipt['rows'],314);self.assertEqual(len(reference['rows']),314);self.assertEqual(len(lines),315)
  self.assertIn('isolated',receipt['scope']);self.assertIn('not input/overlap/audio/hardware',(ROOT/'tests/field_swing_tests.cpp').read_text())
  for i,row in enumerate(reference['rows']):self.assertEqual(row['index'],i)
  self.assertTrue(any(row['attack']and not row['playing']for row in reference['rows']))
  self.assertTrue(any(row['action']['op']=='flush'and not row['attack']for row in reference['rows']))
  self.assertEqual({r['action']['name']for r in reference['rows']if r['action']['op']=='play'},{c['name']for c in self.source['clips']})
if __name__=='__main__':unittest.main()
