import copy,json,sys,tempfile,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import podunk_music as m
class TestPodunkMusic(unittest.TestCase):
 def setUp(self):self.recipe=json.loads((m.ROOT/'content/podunk-music.json').read_text())
 def test_pinned_source(self):
  data=m.checked(self.recipe,m.DEFAULT_PROJECT/'upstream/MOTHER-Encore');self.assertEqual(data,(m.ROOT/'romfs/sound/banks/podunk.encmusic').read_bytes());self.assertEqual(len(self.recipe['regions']),13);self.assertEqual(len(self.recipe['tracks']),5)
 def test_negative_recipe(self):
  cases=[]
  def add(fn):v=copy.deepcopy(self.recipe);fn(v);cases.append(v)
  add(lambda v:v.update(extra=1));add(lambda v:v.update(schema=2));add(lambda v:v['regions'][0].update(priority=10));add(lambda v:v['regions'][0].update(track_id=0));add(lambda v:v['regions'][0].update(fadein_seconds=float('nan')));add(lambda v:v['regions'][0].update(disabled=1));add(lambda v:v['tracks'][0].update(source_sha256='0'*64));add(lambda v:v['regions'][1].update(id=v['regions'][0]['id']));add(lambda v:v['regions'][1].update(shape_paths=v['regions'][0]['shape_paths']));add(lambda v:v['tracks'][0].update(pcm_path='../unsafe.pcm'))
  for case in cases:
   with self.assertRaises(m.MusicError):m.build(case)
 def test_source_changes_rejected(self):
  self.recipe['regions'][0]['volume_db']=1
  with self.assertRaises(m.MusicError):m.checked(self.recipe,m.DEFAULT_PROJECT/'upstream/MOTHER-Encore')
 def test_audio_output_keeps_house_bank(self):
  with tempfile.TemporaryDirectory() as t:
   root=Path(t);(root/'data').mkdir();(root/'sound/banks').mkdir(parents=True);(root/'sound/banks/opening.encaudio').write_bytes(b'house');(root/'data/opening-audio-manifest.json').write_bytes(b'house-manifest')
   def compiler(recipe,upstream,out):
    files=[]
    for path,data in [('sound/banks/opening.encaudio',b'podunk-bank'),('sound/music/podunk-1.pcm',b'pcm')]:
     f=out/path;f.parent.mkdir(parents=True,exist_ok=True);f.write_bytes(data);files.append(dict(path=path,size=len(data),sha256=m.sha(data)))
    return dict(files=files)
   manifest=m.compile_audio_isolated({'assets':[{'pcm_path':'sound/music/podunk-1.pcm'}]},root,root,compiler)
   self.assertEqual((root/'sound/banks/opening.encaudio').read_bytes(),b'house');self.assertEqual((root/'data/opening-audio-manifest.json').read_bytes(),b'house-manifest')
   self.assertEqual((root/'sound/banks/podunk.encaudio').read_bytes(),b'podunk-bank');self.assertIn('sound/banks/podunk.encaudio',[x['path'] for x in manifest['files']])
 def test_audio_output_rejects_non_owned_file(self):
  with tempfile.TemporaryDirectory() as t:
   root=Path(t);(root/'sentinel').write_bytes(b'kept')
   def compiler(recipe,upstream,out):return {'files':[{'path':'sentinel','size':0,'sha256':m.sha(b'')}]}
   with self.assertRaises(m.MusicError):m.compile_audio_isolated({'assets':[]},root,root,compiler)
   self.assertEqual((root/'sentinel').read_bytes(),b'kept')
if __name__=='__main__':unittest.main()
