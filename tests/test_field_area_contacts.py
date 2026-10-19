import hashlib,json,unittest
from pathlib import Path
from tools import field_swing
ROOT=Path(__file__).resolve().parents[1]
class FieldAreaReference(unittest.TestCase):
 def test_actual_engine_reference_sources_and_translator_are_unchanged(self):
  folder=ROOT/'reports/iggy-tutorial/area-timing-reference-3';proof=json.loads((folder/'proof.json').read_bytes())
  self.assertEqual(proof['source_commit'],'7d9246600fffe518408f5830d4848635019005a3')
  self.assertEqual(proof['engine']['hash'],'3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8')
  self.assertEqual((proof['observations'],proof['physics_samples']),(99,41))
  for name,digest in proof['reports_sha256'].items():self.assertEqual(hashlib.sha256((folder/name).read_bytes()).hexdigest(),digest,name)
  for name,digest in proof['sources'].items():self.assertEqual(hashlib.sha256((ROOT/'upstream/MOTHER-Encore'/name).read_bytes()).hexdigest(),digest,name)
  self.assertEqual(json.loads((folder/'input.json').read_bytes())['policy'],field_swing.source()['real'])
  rows=[line.split('\t')for line in(folder/'reference.tsv').read_text().splitlines()]
  self.assertEqual([int(row[0])for row in rows],list(range(41)))
  self.assertEqual({int(row[0]):int(row[-1])for row in rows if int(row[-1])},{9:1,13:2,17:1,24:2,29:1,32:2,37:1})
if __name__=='__main__':unittest.main()
