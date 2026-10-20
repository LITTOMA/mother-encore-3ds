import hashlib,json,struct,unittest
from pathlib import Path
from tools import field_swing
ROOT=Path(__file__).resolve().parents[1]
class SwingContact(unittest.TestCase):
 def test_native_provenance_and_all_source_queries(self):
  folder=ROOT/'reports/iggy-tutorial/swing-contact-reference-2';proof=json.loads((folder/'source.json').read_bytes());sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
  self.assertEqual(proof['commit'],field_swing.PIN);self.assertEqual(proof['engine']['string'],'3.6.2-stable (official)');self.assertEqual(proof['engine_sha256'],'e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd')
  self.assertEqual(proof['producer_sha256'],sha(ROOT/'tools/run_swing_contact_reference.py'))
  for path,digest in proof['sources'].items():self.assertEqual(digest,sha(ROOT/'upstream/MOTHER-Encore'/path))
  for path,digest in proof['outputs'].items():self.assertEqual(digest,sha(folder/path))
  rows=[r.split('\t')for r in(folder/'reference.tsv').read_text().splitlines()];self.assertEqual(len(rows),proof['queries']);self.assertEqual(len(rows),8766);self.assertTrue(all(len(r)==8 and r[7]in('0','1')for r in rows));self.assertEqual(sum(int(r[7])for r in rows),1775)
 def test_source_geometry_is_the_same_checked_action_resource(self):
  from tools import podunk_field as f
  proof=json.loads((ROOT/'reports/iggy-tutorial/swing-contact-reference-2/source.json').read_bytes());_,sections=f.unpack_container((ROOT/'tests/fixtures/field-swing-v0410.encswing').read_bytes(),field_swing.MAGIC,field_swing.STRIDES,field_swing.LIMIT)
  self.assertEqual(struct.pack('<19f',*proof['policy']),sections[0][16:92])
  self.assertEqual(proof['policy'][13:15],[10.4999,17.0]);self.assertIn('geometry',proof['scope']);self.assertIn('not Area2D',proof['scope'])
if __name__=='__main__':unittest.main()
