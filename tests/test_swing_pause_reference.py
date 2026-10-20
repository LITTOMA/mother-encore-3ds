"""Frozen original-method observations, not a passing native pause executor."""
import hashlib,json,re,tempfile,unittest
from pathlib import Path
from tools import run_swing_pause_reference as p

class SwingPauseReference(unittest.TestCase):
 def test_original_methods_topology_and_repeated_stable_results(self):
  a=p.ROOT/'reports/iggy-tutorial/swing-pause-reference-4';b=p.ROOT/'reports/iggy-tutorial/swing-pause-reference-5';self.assertEqual((a/'stable.json').read_bytes(),(b/'stable.json').read_bytes())
  sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
  for folder in(a,b):
   proof=json.loads((folder/'source.json').read_bytes());self.assertEqual((proof['cases'],proof['observations']),(28,3556));self.assertEqual(proof['looped_animations'],['Walk','Run','FaintedWalk']);self.assertEqual(proof['engine']['string'],'3.6.2-stable (official)')
   for path,digest in proof['sources'].items():self.assertEqual(digest,sha(p.ROOT/'upstream/MOTHER-Encore'/path))
   for path,digest in proof['tools'].items():self.assertNotIn('\\',path);self.assertEqual(digest,sha(p.ROOT/path))
   for path,digest in proof['outputs'].items():self.assertEqual(digest,sha(folder/path))
   for symbol,digest in proof['pause_methods'].items():
    path,name=symbol.split('::');source=(p.ROOT/'upstream/MOTHER-Encore'/path).read_text(encoding='utf8');method=re.search(r'^func '+re.escape(name)+r'\(.*?(?=^func |^static func |\Z)',source,re.M|re.S);self.assertIsNotNone(method);self.assertEqual(digest,hashlib.sha256(method[0].encode()).hexdigest())
   doc=json.loads((folder/'reference.json').read_bytes());self.assertEqual(doc['paths'][0],doc['paths'][1]);self.assertEqual(len(doc['paths'][0]),16)
   stable=json.loads((folder/'stable.json').read_bytes());self.assertEqual(stable,[{k:v for k,v in row.items()if k!='stream_playing'}for row in doc['rows']])
   self.assertEqual(len(stable),3556)
   self.assertTrue(any(row['node']=='Bat'and row['state']==0 and row['paused']and row['hit']for row in stable))
   for case in range(28):
    rows=[row for row in stable if row['case']==case];pause=next(row for row in rows if row['op']==4);blocked=next(row for row in rows if row['op']==5);resume=next(row for row in rows if row['op']==6);again=next(row for row in rows if row['op']==7)
    self.assertEqual((pause['state'],pause['paused'],pause['stream_paused'],pause['timer_paused'],pause['collision_calls']),(0,True,True,True,[False]))
    self.assertEqual(blocked['starts'],1);self.assertEqual((resume['state'],resume['paused'],resume['stream_paused'],resume['timer_paused'],resume['collision_calls'],resume['event_queries']),(0,False,False,False,[False,True],1))
    self.assertEqual((again['state'],again['starts'],again['time']),(2,2,resume['time']))
   early=[row for row in stable if row['case']==0 and row['op']in(2,3)and row['paused']];self.assertTrue(early);self.assertTrue(all(row['node']=='Idle'and not row['hit']for row in early))
   current=[row for row in stable if row['case']==2];at=next(i for i,row in enumerate(current)if row['op']==7);self.assertEqual(current[at]['node'],'Bat');self.assertEqual(current[at+1]['node'],'Bat');self.assertGreater(float(current[at+1]['time']),float(current[at]['time']))
 def test_fixture_does_not_create_outputs_outside_owned_roots(self):
  with tempfile.TemporaryDirectory()as name:
   root=Path(name)
   with self.assertRaisesRegex(ValueError,'Fresh owned'):p.run(root/'work',root/'reports',root/'no-engine')
   self.assertEqual(list(root.iterdir()),[])

if __name__=='__main__':unittest.main()
