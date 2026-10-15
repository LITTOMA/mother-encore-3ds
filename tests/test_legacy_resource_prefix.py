import copy,json,unittest
from tests.legacy_resource_prefix import ROOT,historical_room

class LegacyResourcePrefixTests(unittest.TestCase):
 def setUp(self):self.ir=json.loads((ROOT/'content/native-opening.json').read_text(encoding='utf8'))
 def test_projection_keeps_source_unchanged(self):
  before=copy.deepcopy(self.ir);old=historical_room(self.ir)
  self.assertEqual(self.ir,before);self.assertEqual(old['strings'][22],'actor-preview/ninten-main.t3x')
  self.assertEqual(old['sections']['Resource'][23]['sha256'],'19549db8dd6838bc0b3a853e232d959d5e1b76265447943ed8840369b8fe4f52')
 def test_unknown_relocated_hash_rejected(self):
  self.ir['sections']['Resource'][23]['sha256']='0'*64
  with self.assertRaisesRegex(ValueError,'Unreviewed relocated'):historical_room(self.ir)
 def test_changed_string_or_resource_identity_rejected(self):
  for change in ('string','identity'):
   ir=copy.deepcopy(self.ir)
   if change=='string':ir['strings'][22]='graphics/actors/unreviewed.t3x'
   else:ir['sections']['Resource'][26]['stable_id']=99
   with self.subTest(change=change),self.assertRaises(ValueError):historical_room(ir)
