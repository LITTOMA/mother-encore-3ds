import hashlib,json,tempfile,unittest
from pathlib import Path
from tools import run_swing_rebase_reference as r

class SwingRebase(unittest.TestCase):
 def test_original_persistent_sequences_are_repeatable_and_source_bound(self):
  folders=[r.ROOT/'reports/iggy-tutorial'/('swing-rebase-reference-'+str(i))for i in(3,4)]
  self.assertEqual((folders[0]/'reference.tsv').read_bytes(),(folders[1]/'reference.tsv').read_bytes())
  sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
  for folder in folders:
   proof=json.loads((folder/'source.json').read_bytes());self.assertEqual((proof['cases'],proof['sequences'],proof['observations']),(80,8,6560))
   for name,digest in proof['sources'].items():self.assertEqual(digest,sha(r.ROOT/'upstream/MOTHER-Encore'/name))
   for name,digest in proof['tools'].items():self.assertNotIn('\\',name);self.assertEqual(digest,sha(r.ROOT/name))
   for name,digest in proof['outputs'].items():self.assertEqual(digest,sha(folder/name))
   rows=json.loads((folder/'reference.json').read_bytes())['rows'];self.assertEqual(len(rows),6560)
   self.assertTrue(any(row['phase']==0 and row['frame']!=0 and row['selected']=='swing'for row in rows))
   self.assertEqual({row['starts']for row in rows},set(range(11)))
   self.assertEqual({row['sequence']for row in rows},set(range(8)))
 def test_reference_requires_fresh_owned_outputs_before_any_engine_call(self):
  with tempfile.TemporaryDirectory()as name:
   root=Path(name)
   with self.assertRaisesRegex(ValueError,'Fresh owned'):r.run(root/'work',root/'reports',root/'missing-engine')
   self.assertEqual(list(root.iterdir()),[])
  with self.assertRaisesRegex(ValueError,'Fresh owned'):r.run(r.ROOT/'build',r.ROOT/'reports',r.ROOT/'missing-engine')

if __name__=='__main__':unittest.main()
