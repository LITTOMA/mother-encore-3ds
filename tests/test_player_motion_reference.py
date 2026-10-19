import hashlib,json,re,tempfile,unittest
from pathlib import Path
from tools import run_player_motion_reference as p
from tests import prepare_player_action_world as world

class PlayerMotionReference(unittest.TestCase):
 def test_original_direction_methods_and_repeated_results(self):
  a=p.ROOT/'reports/iggy-tutorial/player-motion-reference-2';b=p.ROOT/'reports/iggy-tutorial/player-motion-reference-3';sha=lambda q:hashlib.sha256(q.read_bytes()).hexdigest();self.assertEqual((a/'stable.json').read_bytes(),(b/'stable.json').read_bytes());self.assertEqual((a/'reference.tsv').read_bytes(),(b/'reference.tsv').read_bytes())
  for folder in(a,b):
   proof=json.loads((folder/'source.json').read_bytes());self.assertEqual((proof['cases'],proof['observations']),(14,2058));self.assertEqual(proof['engine']['string'],'3.6.2-stable (official)')
   for path,digest in proof['sources'].items():self.assertEqual(sha(p.ROOT/'upstream/MOTHER-Encore'/path),digest)
   for path,digest in proof['tools'].items():self.assertNotIn('\\',path);self.assertEqual(sha(p.ROOT/path),digest)
   for path,digest in proof['outputs'].items():self.assertEqual(sha(folder/path),digest)
   for symbol,digest in proof['direction_methods'].items():
    path,name=symbol.split('::');source=(p.ROOT/'upstream/MOTHER-Encore'/path).read_text(encoding='utf8');method=re.search(r'^func '+re.escape(name)+r'\(.*?(?=^func |^static func |\Z)',source,re.M|re.S);self.assertIsNotNone(method);self.assertEqual(hashlib.sha256(method[0].encode()).hexdigest(),digest)
   raw=json.loads((folder/'reference.json').read_bytes())['rows'];stable=json.loads((folder/'stable.json').read_bytes());self.assertEqual(stable,[{k:v for k,v in row.items()if k!='stream_playing'}for row in raw]);self.assertEqual(len(stable),2058)
   rows=[r for r in raw if r['case']==0];at=next(i for i,r in enumerate(rows)if r['op']==8);self.assertEqual(rows[at]['node'],'Bat');self.assertEqual(rows[at+1]['node'],'Idle');self.assertTrue(rows[at+1]['special']);self.assertFalse(rows[at+1]['main']);self.assertTrue(rows[at+3]['main']);self.assertFalse(rows[at+3]['special'])
 def test_fresh_motion_fixture_roots_are_checked_before_writes(self):
  with tempfile.TemporaryDirectory()as folder:
   root=Path(folder)
   with self.assertRaisesRegex(ValueError,'Fresh owned'):p.run(root/'work',root/'reports',root/'no-engine')
   self.assertEqual(list(root.iterdir()),[])
 def test_world_metadata_fixture_rejects_an_unowned_root(self):
  with tempfile.TemporaryDirectory()as folder:
   root=Path(folder)
   with self.assertRaisesRegex(ValueError,'World action fixture'):world.prepare(root/'output')
   self.assertEqual(list(root.iterdir()),[])

if __name__=='__main__':unittest.main()
