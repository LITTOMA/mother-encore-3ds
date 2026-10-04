import copy,hashlib,json,os,shutil,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
from tools import items_assets as assets,items_presentation_bindings as bindings,native_items as native
ROOT=Path(os.environ.get('ENCORE_SOURCE_ROOT',Path(__file__).resolve().parents[1]))

class ItemsPresentationBindingsTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.recipe=bindings.read(bindings.IR);cls.ir=bindings.read(ROOT/'content/native-items.json');cls.reference=bindings.read(ROOT/'reports/items-menu-source/native-layout.json')
  cls.fixture=ROOT/'build/items-binding-tests';cls.fixture.mkdir(parents=True,exist_ok=True)
 def reject(self,edit):
  recipe=copy.deepcopy(self.recipe);edit(recipe)
  with self.assertRaises(ValueError):bindings.load(ROOT,recipe)
 def test_default_art_ir_and_checked_pack_are_identical(self):
  ex,definitions,raw=assets.reviewed_extract()
  self.assertEqual(assets.make_recipe(ex),bindings.read(ROOT/'content/items-assets.json'))
  ir=assets.export_ir(ex,definitions,raw,self.reference,self.ir['resources'],self.ir['dependencies'],write=False)
  self.assertEqual(ir,self.ir);native.verify_sources(ir,ROOT)
  self.assertEqual(native.encode(native.lower(ir,root=ROOT)),(ROOT/'romfs/data/opening.encitems').read_bytes())
  receipt=bindings.read(ROOT/'content/asset-receipts/graphics/ui/items/source.json')
  for row in receipt['resources']:self.assertEqual(bindings.sha(ROOT/'romfs'/row['path']),row['sha256'])
 def test_versions_pin_unknown_missing_and_duplicate_fields(self):
  for version in [0,2,True,'1',None]:self.reject(lambda r:r.update(schema=version))
  self.reject(lambda r:r.update(commit='0'*40))
  for route in [(),('cursor',),('info',),('platform',),('item',),('clips',),('probe',),('layouts','panel'),('assets',0),('sounds',0),('texture_links',0)]:
   for missing in [False,True]:
    def edit(r):
     for key in route:r=r[key]
     if missing:r.pop(next(iter(r)))
     else:r['unknown']=1
    with self.subTest(route=route,missing=missing):self.reject(edit)
  with self.assertRaises(ValueError):json.loads('{"schema":1,"schema":1}',object_pairs_hook=bindings.unique)
 def test_asset_source_crop_reuse_and_identity_bindings(self):
  for field,value in [('source','../escape.png'),('output','../escape.t3x'),('output','graphics/ui/items/x.bin'),('size',[24,23]),('grid',[0,1]),('id',True)]:
   with self.subTest(field=field):self.reject(lambda r:r['assets'][0].update({field:value}))
  self.reject(lambda r:r['assets'][1].update(name=r['assets'][0]['name']))
  self.reject(lambda r:r['assets'][1].update(output=r['assets'][0]['output']))
  self.reject(lambda r:r['assets'][3].update(crop=[1,19,8,7]))
  self.reject(lambda r:r['assets'][8].update(glyph='X'))
  self.reject(lambda r:r['assets'][10].update(quarter_turns=3))
  self.reject(lambda r:r['reuse']['cursor'].update(name='box'))
  self.reject(lambda r:r['asset_roles'].update(cursor='cap'))
  for field,value in [('party','ana'),('source_name','unknown'),('owner_id',0),('instance_id',True),('flags',2),('can_use',2),('slot','head')]:
   self.reject(lambda r:r['item'].update({field:value}))
 def test_nodes_source_expressions_tweens_input_fonts_and_sound_id(self):
  self.reject(lambda r:r['nodes'].update(panel='Missing'))
  self.reject(lambda r:r['nodes'].update(info=r['nodes']['panel']))
  self.reject(lambda r:r['cursor'].update(x_expression='-size.x/5.0'))
  self.reject(lambda r:r['cursor'].update(x_expression='execute()'))
  self.reject(lambda r:r['cursor'].update(y_expression='size.y/0.0'))
  self.reject(lambda r:r['info'].update(show_expression='_init_pos + _hide_offset'))
  self.reject(lambda r:r['info'].update(trans='CUBIC'))
  self.reject(lambda r:r['platform'].update(scope_action='ui_accept'))
  self.reject(lambda r:r['platform'].update(scope_button=True))
  self.reject(lambda r:r['probe'].update(hint_outline=2))
  self.reject(lambda r:r['grid'].update(columns=3))
  self.reject(lambda r:r['layouts']['panel'].update(anchor=[1.1,0]))
  self.reject(lambda r:r['sounds'][0].update(audio_id=1101))
  self.reject(lambda r:r['source_facts'].pop(next(iter(r['source_facts']))))
 def test_ordinary_compiler_checks_content_not_only_source_fingerprints(self):
  for edit in [lambda ir:ir['parameters']['CursorOffset'].__setitem__(0,-8),lambda ir:ir['parameters']['InfoMotion'].__setitem__(0,.2),lambda ir:ir['parameters']['InputBinding'].__setitem__(2,256),lambda ir:ir['definitions'][0].update(source='invented'),lambda ir:ir['resources'][0].update(width=25)]:
   ir=copy.deepcopy(self.ir);edit(ir)
   with self.assertRaises(ValueError):native.verify_sources(ir,ROOT)
 def test_rehashed_source_semantics_still_reject(self):
  changes=[('cursor_script','size.x = -size.x/6.0','size.x = -size.x/5.0'),('info_script','_init_pos - _hide_offset','_init_pos + _hide_offset'),('battle','menu_parent_path = NodePath("../GridContainer")','menu_parent_path = NodePath("../InfoBox")')]
  for key,before,after in changes:
   with self.subTest(key=key),tempfile.TemporaryDirectory(dir=self.fixture)as directory:
    root=Path(directory);recipe=copy.deepcopy(self.recipe);inventory=bindings.read(ROOT/'compatibility/upstream-inventory.json');changed=recipe['source_refs'][key]
    for path in recipe['sources']:
     data=(ROOT/'upstream/MOTHER-Encore'/path).read_bytes()
     if path==changed:
      self.assertIn(before.encode(),data);data=data.replace(before.encode(),after.encode());sha=hashlib.sha256(data).hexdigest();recipe['sources'][path]=sha;inventory['files'][path]['sha256']=sha
     target=root/'upstream/MOTHER-Encore'/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
    files=['upstream.lock','content/native-audio.json']
    for reuse in recipe['reuse'].values():
     files.append(reuse['receipt']);receipt=bindings.read(ROOT/reuse['receipt']);resource=next(r for r in receipt['resources']if r['name']==reuse['name'])
     asset=next(r for r in recipe['assets']if r['name']in recipe['reuse']and recipe['reuse'][r['name']]==reuse);files.append('romfs/'+asset['output'])
    for path in set(files):
     target=root/path;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(ROOT/path,target)
    path=root/'compatibility/upstream-inventory.json';path.parent.mkdir(parents=True);path.write_text(json.dumps(inventory),encoding='utf-8')
    with self.assertRaises(ValueError):bindings.load(root,recipe)
 def test_checked_adapter_data_changes_without_new_code(self):
  recipe=copy.deepcopy(self.recipe);recipe['layouts']['info']['anchor']=[.75,1];recipe['platform']['hint_offset']=[-2,-1]
  with tempfile.TemporaryDirectory(dir=self.fixture)as directory:
   path=Path(directory)/'bindings.json';path.write_text(json.dumps(recipe),encoding='utf-8')
   with patch.object(bindings,'IR',path):
    ex,definitions,raw=assets.reviewed_extract();changed=assets.export_ir(ex,definitions,raw,self.reference,self.ir['resources'],self.ir['dependencies'],write=False)
    native.verify_sources(changed,ROOT);blob=native.encode(native.lower(changed,root=ROOT));native.parse_pack(blob)
   self.assertNotEqual(blob,(ROOT/'romfs/data/opening.encitems').read_bytes())
   self.assertEqual(changed['definitions'],self.ir['definitions']);self.assertEqual(changed['initial_inventory'],self.ir['initial_inventory']);self.assertEqual(changed['sounds'],self.ir['sounds']);self.assertEqual(changed['clips'],self.ir['clips'])

if __name__=='__main__':unittest.main()
