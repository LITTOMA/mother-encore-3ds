import hashlib,json,shutil,tempfile,unittest
from pathlib import Path
from tools import run_dungeon_contact_reference as producer
from tests import prepare_dungeon_mechanism_contacts as fixture
ROOT=Path(__file__).resolve().parents[1]
class DungeonMechanismContacts(unittest.TestCase):
 def test_native_replays_are_identical_and_sources_remain_fixed(self):
  left=fixture.REFERENCE;right=left.parent/'native-3'
  self.assertEqual((left/'contacts.json').read_bytes(),(right/'contacts.json').read_bytes())
  self.assertEqual((left/'contact-input.json').read_bytes(),(right/'contact-input.json').read_bytes())
  for folder in (left,right):
   rows=fixture.reference(folder);proof=json.loads((folder/'contact-receipt.json').read_bytes())
   self.assertEqual((len(rows),proof['cases']),(49,22))
   self.assertEqual(proof['producer_sha256'],hashlib.sha256(Path(producer.__file__).read_bytes()).hexdigest())
   self.assertEqual(proof['bootstrap_sha256'],hashlib.sha256((folder/'receipt.json').read_bytes()).hexdigest())
 def test_shape_filter_and_contact_outcomes_are_retained(self):
  doc=json.loads((fixture.REFERENCE/'contacts.json').read_bytes());rows={r['case']+'/'+r['phase']:r for r in doc['rows']}
  hits=lambda key:sum(e['kind']=='switch_hit'for e in rows[key]['events'])
  self.assertEqual([hits('bat_layer_'+str(n)+'/enter')for n in(0,1,2,64,128)],[0,0,1,1,1])
  self.assertEqual(hits('bat_reverse_mask/enter'),1)
  self.assertEqual([hits('body_filter_'+key+'/enter')for key in('0_4353','1_0','2_4353','0_0','2_0')],[1,1,1,0,0])
  for kind in ('bat','body'):
   self.assertEqual([hits(kind+'_paused/'+p)for p in('enter_paused','unpause_stay','leave','reenter')],[0,0,0,1])
   self.assertEqual([hits(kind+'_not_ready/'+p)for p in('early_enter','ready_stay','leave','reenter')],[0,0,0,1])
  self.assertEqual(hits('body_enter_stay_reenter/reenter'),0)
  self.assertEqual(hits('other_body/other_enters'),0)
 def test_source_body_matches_existing_shared_core_hull(self):
  player,swing,ex=producer.source_player()
  ir=json.loads((ROOT/'content/native-opening.json').read_bytes());scene=ir['sections']['Scene'][0]
  hull=ir['sections']['Vec2'][scene['actor_hull_first']:scene['actor_hull_first']+scene['actor_hull_count']]
  self.assertEqual([[p[0]+player['position'][0],p[1]+player['position'][1]]for p in player['points']],[[p['x'],p['y']]for p in hull])
 def test_changed_frozen_outputs_and_source_commit_are_rejected(self):
  with tempfile.TemporaryDirectory()as tmp:
   folder=Path(tmp)
   for p in fixture.REFERENCE.iterdir():
    if p.is_file():shutil.copyfile(p,folder/p.name)
   proof=json.loads((folder/'contact-receipt.json').read_bytes());proof['commit']='0'*40;(folder/'contact-receipt.json').write_text(json.dumps(proof))
   with self.assertRaisesRegex(ValueError,'source commit'):fixture.reference(folder)
   shutil.copyfile(fixture.REFERENCE/'contact-receipt.json',folder/'contact-receipt.json');(folder/'contacts.json').write_bytes(b'{}')
   with self.assertRaisesRegex(ValueError,'output changed'):fixture.reference(folder)
if __name__=='__main__':unittest.main()
