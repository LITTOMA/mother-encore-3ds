import copy,json,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
from tools import field_player_assets as p,field_swing_presentation as presentation,romfs_layout,resource_catalog as catalog
from tools.godot_text import Document

class PlayerAssets(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.recipe=p.read();cls.actual=json.loads((p.ROOT/cls.recipe['atlas_receipt']).read_bytes());cls.directory=presentation.read()['texture_directory'];cls.load=staticmethod(lambda name:p.checked(p.ROOT/'romfs',cls.directory+'/'+name))
 def test_real_group_and_receipts_are_reproducible(self):
  files,receipt=p.generate(self.actual,self.load);self.assertEqual(files,p.stage_files());self.assertEqual(receipt,json.loads((p.ROOT/self.recipe['receipt']).read_bytes()));self.assertEqual(len(files),6);self.assertEqual(sum(len(v)for v in files.values()),2106624);romfs_layout.check_layout(files)
  self.assertEqual({r['role']for r in catalog.decode(catalog.OUT.read_bytes())['bindings']if r['role']in catalog.SWING_ROLES},catalog.SWING_ROLES)
 def test_unknown_receipt_source_tool_frame_or_texture_rejects(self):
  edits=[lambda a:a.update(unknown=0),lambda a:a.update(commit='0'*40),lambda a:a.update(tex3ds_sha256='unknown'),lambda a:a['dependencies'].update({'tools/field_swing_graphics.py':'0'*64}),lambda a:a['layers'][0].update(role=2),lambda a:a['layers'][0].update(t3x_bytes=0),lambda a:a['layers'][0].update(t3x_sha256='0'*64),lambda a:a['layers'][0]['frames'][0].update(x=1)]
  for edit in edits:
   a=copy.deepcopy(self.actual);edit(a)
   with self.subTest(edit=edit),self.assertRaises(ValueError):p.graphics_packet(a,self.load)
  with self.assertRaises(ValueError):p.graphics_packet(self.actual,lambda name:self.load(name)[:-1])
 def test_recipe_schema_and_unsafe_outputs_reject(self):
  edits=[lambda a:a.update(schema=True),lambda a:a.update(schema=2),lambda a:a.update(extra=0),lambda a:a.update(swing_output='../x.encswing'),lambda a:a.update(swing_output='world/x.encentry'),lambda a:a.update(receipt='romfs/source.json'),lambda a:a.update(receipt=a['atlas_receipt'])]
  with tempfile.TemporaryDirectory()as folder:
   path=Path(folder)/'bad.json'
   for edit in edits:
    a=copy.deepcopy(self.recipe);edit(a);path.write_text(json.dumps(a),encoding='utf8')
    with self.subTest(edit=edit),self.assertRaises(ValueError):p.read(path)
   path.write_text('{"schema":1,"schema":1}',encoding='utf8')
   with self.assertRaises(ValueError):p.read(path)
 def test_missing_or_escaped_resource_rejects(self):
  with tempfile.TemporaryDirectory()as folder:
   for path in ('missing.t3x','../escape.t3x'):
    with self.subTest(path=path),self.assertRaises(ValueError):p.checked(folder,path)
 def test_nonidentity_source_material_is_not_silently_ignored(self):
  base,ex=p.swing.read();original=Document(base['scene'],ex.text(base['scene']))
  for key,value in [('shader_param/width',1),('shader_param/inside',True),('shader_param/pattern',2)]:
   doc=copy.deepcopy(original);node=next(n for n in doc.nodes if n.attrs['name']=='SpecialAnimations'and n.attrs.get('parent')=='.');doc.sub_resource(node.props['material'])[key]=value
   with self.subTest(key=key),patch.object(p,'Document',return_value=doc),self.assertRaises(ValueError):p.generate(self.actual,self.load)

if __name__=='__main__':unittest.main()
