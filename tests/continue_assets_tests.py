#!/usr/bin/env python3
import copy,sys,tempfile,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools import continue_assets as c
class ContinueAssetsTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.recipe=c.read_json(c.RECIPE)
 def test_source_pack_and_dependencies(self):
  c.verify_recipe(self.recipe);self.assertEqual(c.PACK.read_bytes(),c.encode(self.recipe));self.assertEqual(len(c.stage_files(c.ROOT/'romfs')),len(self.recipe['resources'])+1)
 def test_presentation_and_mechanism_drift_rejected(self):
  mutations=[lambda r:r.update(unknown='autoload_latest'),lambda r:r['sounds'].__setitem__(3,r['sounds'][0]),lambda r:r['title_options'][1]['rect'].__setitem__(0,0),lambda r:r['title_layers'][0]['viewport_offsets'][1].__setitem__(1,30),lambda r:r['title_options'][1]['viewport_offsets'][0].__setitem__(0,40),lambda r:r.update(title_background_color=0xff000000),lambda r:r['viewports'][1]['actions'][3].__setitem__(0,0),lambda r:r['animations'][2].update(length=.4),lambda r:r['layouts'][1].__setitem__(0,32),lambda r:r['outputs'].pop(next(iter(r['outputs'])))]
  for edit in mutations:
   r=copy.deepcopy(self.recipe);edit(r)
   with self.assertRaises(ValueError):c.verify_recipe(r)
 def test_title_reference_and_expanded_geometry(self):
  r=self.recipe;self.assertEqual(r['schema'],2);self.assertEqual(r['layouts'][0],[320,180,400,240])
  self.assertEqual([a['rect']for a in r['title_layers']],[[0,0,320,180],[178.5,73,89,40],[70,45,180,36],[105.5,39.5,47,47]])
  self.assertEqual([a['rect']for a in r['title_options']],[[133,121,54,12],[145.5,133,29,12],[133.5,145,53,12],[147.5,157,25,12]])
  self.assertEqual(r['title_background_color'],0xff0b0007)
  self.assertEqual(r['layouts'][7],[0,0,40,30]);self.assertEqual(r['layouts'][7],sum(r['title_options'][0]['viewport_offsets'],[]))
  for i,a in enumerate(r['title_layers']+r['title_options']):
   self.assertEqual(a['viewport_offsets'],[[0,0],[40,60]if i==0 else[40,30]])
   self.assertEqual(a['rect'][2:],[r['resources'][a['resource']][key]for key in ['width','height']])
   for (width,height),(dx,dy)in zip([(320,180),(400,240)],a['viewport_offsets']):
    x,y,w,h=a['rect'];self.assertGreaterEqual(x+dx,0);self.assertGreaterEqual(y+dy,0);self.assertLessEqual(x+dx+w,width);self.assertLessEqual(y+dy+h,height)
  x,y,w,h=r['title_layers'][0]['rect'];dx,dy=r['title_layers'][0]['viewport_offsets'][1];self.assertEqual(y+dy+h,240)
 def test_title_exterior_requires_uniform_opaque_source_edges(self):
  ex,r,_=c.extract();title=ex.text(c.SOURCES[0]);a=r['resources'][0];image=c.Image.open(ex.upstream/a['source']).convert('RGBA')
  reference,foreground,background,color=c.title_viewport_layout(title,image,a['tint']);self.assertEqual(color,r['title_background_color'])
  # The lower source edge contains the globe; preserving it at the viewport
  # bottom is required. It must never be used as an invented repeating border.
  self.assertGreater(len({image.getpixel((x,image.height-1))for x in range(image.width)}),1)
  for point in [(image.width//2,0),(0,image.height//2),(image.width-1,image.height//2)]:
   changed=image.copy();changed.putpixel(point,(0,0,0,255))
   with self.assertRaises(ValueError):c.title_viewport_layout(title,changed,a['tint'])
  changed=image.copy();changed.putpixel((0,0),(11,0,11,0))
  with self.assertRaises(ValueError):c.title_viewport_layout(title,changed,a['tint'])
 def test_stage_corruption_and_escape(self):
  files=c.stage_files(c.ROOT/'romfs')
  with tempfile.TemporaryDirectory()as temp:
   root=Path(temp)/'romfs';root.mkdir()
   for p,b in files.items():target=root/p;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(b)
   pack=root/'data/opening.enccontinue';pack.write_bytes(pack.read_bytes()+b'x')
   with self.assertRaises(ValueError):c.stage_files(root)
   pack.write_bytes(files[Path('data/opening.enccontinue')]);asset=next(p for p in files if p.suffix=='.t3x');outside=Path(temp)/'outside';outside.write_bytes(files[asset]);(root/asset).unlink();(root/asset).symlink_to(outside)
   with self.assertRaises(ValueError):c.stage_files(root)
if __name__=='__main__':unittest.main()
