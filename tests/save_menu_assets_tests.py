#!/usr/bin/env python3
import copy,sys,tempfile,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from PIL import Image
from tools import save_menu_assets as s
class SaveMenuAssetsTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.recipe=s.read_json(s.RECIPE)
 def test_source_and_pack(self):
  s.verify_recipe(self.recipe);self.assertEqual(s.encode(self.recipe),s.PACK.read_bytes());self.assertEqual(len(s.stage_files(s.ROOT/'romfs')),len(self.recipe['resources'])+2)
 def test_source_rule_and_glyph_drift(self):
  for kind in ('slots','unknown','glyph','coverage'):
   r=copy.deepcopy(self.recipe)
   if kind=='slots':r['slot_count']+=1
   if kind=='unknown':r['unreviewed_feature']='settings'
   if kind=='glyph':r['glyphs'][10]['offset_x']+=1
   if kind=='coverage':r['outputs'].pop(next(iter(r['outputs'])))
   with self.assertRaises(ValueError,msg=kind):s.verify_recipe(r)
 def test_negative_ninepatch_seams(self):
  # Sample the original reversed center UV and normalized increasing UV at the
  # centers of many stretched destination pixels. This is a CPU equivalence
  # check, not a GPU pixel comparison.
  for file,margin in [('Graphics/UI/Misc/file_select_cursor.png',9),('Graphics/UI/Overworld/flavours/defaultbox.png',13)]:
   original=Image.open(s.ROOT/'upstream/MOTHER-Encore'/file).convert('RGBA');size=original.width
   normalized=s.normalize(original,[margin]*4);span=2*margin-size
   for dest in (31,69,192,256,336):
    for k in range(dest):
     t=(k+.5)/dest;old=int(margin-t*span);new=int(margin+t*span)
     self.assertEqual(original.getpixel((old,old)),normalized.getpixel((new,new)))
  patterned=Image.new('RGBA',(24,24));patterned.putdata([(x,y,0,255)for y in range(24)for x in range(24)])
  with self.assertRaises(ValueError):s.normalize(patterned,[13]*4)
 def test_stage_rejects_stale_pack_and_symlink_escape(self):
  files=s.stage_files(s.ROOT/'romfs')
  with tempfile.TemporaryDirectory()as temp:
   root=Path(temp)/'romfs';root.mkdir()
   for p,b in files.items():target=root/p;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(b)
   pack=root/'data/opening.encsavemenu';pack.write_bytes(pack.read_bytes()+b'x')
   with self.assertRaises(ValueError):s.stage_files(root)
   pack.write_bytes(files[Path('data/opening.encsavemenu')]);a=next(x for x in files if x.suffix=='.t3x');outside=Path(temp)/'outside';outside.write_bytes(files[a]);(root/a).unlink();(root/a).symlink_to(outside)
   with self.assertRaises(ValueError):s.stage_files(root)
if __name__=='__main__':unittest.main()
