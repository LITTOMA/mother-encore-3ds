import copy, hashlib, json, struct, unittest
from pathlib import Path
from PIL import Image
from tools import doll_entry_asset as doll
from tools import native_battle as native
from tools.battle_assets import decode_indexed
ROOT=Path(__file__).resolve().parents[1]

class DollEntryAssetTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.ir=json.loads(doll.IR_PATH.read_text());cls.assets=json.loads(doll.RECEIPT.read_text());cls.blob,cls.tables=doll.compile_pack(cls.ir)
 def test_deterministic_bundle_and_safe_staging(self):
  self.assertEqual(self.blob,doll.PACK.read_bytes());self.assertEqual(struct.unpack_from('<I',self.blob,8)[0],2)
  self.assertEqual(self.blob,native.encode(native.lower(self.ir,self.assets),self.ir['commit']))
  files=native.stage_files(ROOT/'romfs',Path('data/doll-entry.encbattle'))
  self.assertEqual(len(files),len(self.tables['resources'])+1);self.assertIn(Path('house-preview/doll.t3x'),files)
  with self.assertRaises(native.ContentError):native.stage_files(ROOT/'romfs',Path('../else'))
 def test_lossless_background_and_palette(self):
  for name in ['background','background-palette']:
   spec=self.ir['presentation']['assets'][name];data=decode_indexed((ROOT/'romfs'/spec['output']).read_bytes())
   image=Image.open(ROOT/'upstream/MOTHER-Encore'/spec['source']).convert('RGBA')
   self.assertEqual([data['palette'][n] for n in data['pixels']],list(image.getdata()))
 def test_exact_roster_audio_geometry_and_shader(self):
  self.assertEqual(self.ir['enemy']['sprite_size'],[35,51]);self.assertEqual(self.ir['enemy']['sprite_center'],[160,73.5]);self.assertEqual(self.ir['enemy']['data']['hp'],38)
  self.assertEqual(self.tables['metadata'][0][2],1002);self.assertEqual(self.tables['metadata'][0][5],2)
  self.assertEqual(self.ir['background']['texture_size'],[176,172]);self.assertEqual(len(self.tables['backgrounds'][0]),29)
  self.assertEqual([r[-2:] for r in self.tables['backgrounds']],[[0,0],[0,0]])
  self.assertFalse(self.ir['entry']['can_run']);self.assertFalse(self.ir['entry']['source_can_run_argument'])
 def test_portrait_inherits_parent_before_own_show(self):
  target=self.tables['roles']['party'];tracks=[t for t in self.tables['tracks'] if t[0]==target and t[1]==3]
  self.assertEqual([t[2] for t in tracks],[0,6])
  self.assertEqual([self.tables['keys'][t[3]][2][0] for t in tracks],[193,116])
  self.assertEqual([self.tables['keys'][t[3]+t[4]-1][2][0] for t in tracks],[116,96])
 def test_native_palette_review_is_bound(self):
  doll.validate(self.ir)
  for key,value in [('fixed_palette_row',1),('review_sha256','0'*64),('review','../outside')]:
   ir=copy.deepcopy(self.ir);ir['background']['compatibility_policy'][key]=value
   with self.assertRaises(ValueError):doll.validate(ir)
 def test_unknown_shader_branch_cannot_be_silently_dropped(self):
  for key,value in [('interlaced_amplitude',[1,0]),('osc_trans_ping_pong',[0,1]),('comp_amp_ping_pong',[1,1]),('blending',1),('unknown_effect',1)]:
   ir=copy.deepcopy(self.ir);ir['background']['layers'][0]['properties'][key]=value
   with self.assertRaises(ValueError):native.lower(ir,self.assets)
  ir=copy.deepcopy(self.ir);ir['binary_version']=1
  with self.assertRaises(ValueError):native.lower(ir,self.assets)
 def test_unknown_pin_or_combat_approval_rejected(self):
  for field,value in [('commit','0'*40),('binary_version',3)]:
   ir=copy.deepcopy(self.ir);ir[field]=value
   with self.assertRaises(ValueError):doll.validate(ir)
  ir=copy.deepcopy(self.ir);ir['scope']['combat_implemented']=True
  with self.assertRaises(ValueError):doll.validate(ir)
 def test_no_fake_reward_flag_or_combat(self):
  self.assertEqual(self.ir['entry']['win_flag'],'');self.assertEqual(self.ir['entry']['post_battle_cutscenes'],{'win':'Podunk/cutscenes/doll_defeated'})
  self.assertFalse(self.ir['scope']['combat_implemented']);self.assertFalse(self.ir['scope']['whole_battle_approved'])
 def test_every_truncated_binary_rejected(self):
  for n in range(len(self.blob)):
   with self.assertRaises((ValueError,UnicodeError,struct.error)):native.parse_sections(self.blob[:n])

if __name__=='__main__':unittest.main()
