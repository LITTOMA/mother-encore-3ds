import hashlib,json,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class FieldObjectReference(unittest.TestCase):
 def test_actual_engine_and_unchanged_source_ready_subset(self):
  folder=ROOT/'reports/iggy-tutorial/field-objects-reference-1';proof=json.loads((folder/'proof.json').read_bytes())
  self.assertEqual(proof['engine']['hash'],'3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8')
  self.assertEqual(proof['engine_sha256'],'e748acc92ee5205dd3cc1754d4422870f6a68ae6e00a16ff88bd3438c957adfd')
  for name,digest in proof['sources'].items():self.assertEqual(hashlib.sha256((ROOT/'upstream/MOTHER-Encore'/name).read_bytes()).hexdigest(),digest,name)
  for name,digest in proof['files_sha256'].items():self.assertEqual(hashlib.sha256((folder/name).read_bytes()).hexdigest(),digest,name)
  source=(ROOT/'upstream/MOTHER-Encore/Scripts/Main/DroppedItem.gd').read_text();ready=source[source.index('func _ready():'):source.index('func _set_item(')]
  self.assertEqual(hashlib.sha256(ready.encode()).hexdigest(),proof['ready_function_sha256'])
  self.assertTrue((folder/'life.gd').read_text().endswith(ready))
  self.assertEqual((folder/'sparkles.gd').read_bytes(),(ROOT/'upstream/MOTHER-Encore/Scripts/misc/sparkles.gd').read_bytes())
  rows=(folder/'reference.tsv').read_text().splitlines();self.assertEqual(len(rows),500)
  self.assertEqual(len([r for r in rows if r.startswith('ready\t')]),64)
  self.assertEqual(len([r for r in rows if r.startswith('clock\t')]),430)
if __name__=='__main__':unittest.main()
