import copy,json,math,struct,unittest,zlib
from pathlib import Path
from unittest import mock
from tools import native_introduction as intro
class NativeIntroductionTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.data=json.loads(intro.IR.read_text(encoding='utf8'))
  intro.verify(cls.data)
 def test_source_binary_and_original_chain(self):
  blob=intro.encode(self.data)
  self.assertEqual(intro.PACK.read_bytes(),blob)
  self.assertEqual(struct.unpack_from('<8s4I',blob),(b'ENCINTRO',1,len(blob),zlib.crc32(blob[24:]),1))
  self.assertEqual(self.data['doors'][-1]['destination']['scene'],'res://Maps/podunk/Nintens House.tscn')
  self.assertEqual(self.data['doors'][-1]['destination']['y'],397)
  self.assertEqual(self.data['scenes'][0]['length'],116)
  self.assertEqual(self.data['scenes'][0]['events'][-1]['time'],116.5)
  self.assertEqual(self.data['scenes'][1]['length'],22)
  self.assertEqual([r['frame_count'] for r in self.data['resources']][-1],31)
 def test_schema_and_reference_negatives(self):
  mutations=[lambda d:d.update(schema=2),lambda d:d.update(commit='0'*40),lambda d:d.update(unknown_rule=True),lambda d:d.update(finish_stop_slot=2),lambda d:d['hint_curve'].__setitem__(0,math.nan),lambda d:d['scenes'][0].update(round_images=99),lambda d:d['resources'][0].update(path='../escape.t3x'),lambda d:d['resources'][0].update(columns=0),lambda d:d['resources'][0].update(trim_x=9999),lambda d:d['resources'][0].update(sha256='no'),lambda d:d['scenes'][0].update(pitch_min=math.nan),lambda d:d['scenes'][0]['resources'].__setitem__(0,999),lambda d:d['scenes'][0]['tracks'][0].update(target=99),lambda d:d['scenes'][0]['tracks'][0]['keys'].__setitem__(0,dict(time=-1,value=[0]*4,transition=1)),lambda d:d['locales'][0]['texts'].pop(),lambda d:d['scenes'][0]['events'].pop(next(i for i,e in enumerate(d['scenes'][0]['events']) if e['kind']==0))]
  for mutation in mutations:
   d=copy.deepcopy(self.data);mutation(d)
   with self.subTest(mutation=mutation),self.assertRaises(ValueError):intro.encode(d)
 def test_unknown_source_tracks_and_methods_fail_closed(self):
  original=intro.animation
  for method in (False,True):
   def changed(text,name=None,ident=None):
    length,tracks=original(text,name,ident)
    if name==self.data['source_map']['scenes'][0]['animation'] and len(tracks)==self.data['source_map']['scenes'][0]['track_count']:
     tracks=copy.deepcopy(tracks)
     if method:next(t for t in tracks if t['type']=='method')['keys']['values'][0]['method']='unsupported_source_method'
     else:next(t for t in tracks if t['type']=='value')['path']='Unknown/Actor:rect_position'
    return length,tracks
   with self.subTest(method=method),mock.patch.object(intro,'animation',side_effect=changed),self.assertRaisesRegex(ValueError,'Unknown|Unsupported'):intro.extract()
 def test_checked_asset_sources_and_pack_staging(self):
  files=intro.stage_files(intro.ROOT/'romfs')
  self.assertEqual(len(files),12)
  self.assertTrue(all(p.suffix!='.json' for p in files))
 def test_no_unchecked_semantic_refresh(self):
  d=copy.deepcopy(self.data);d['scenes'][1]['speed']=.12
  with self.assertRaisesRegex(ValueError,'semantics'):intro.verify(d)
  d=copy.deepcopy(self.data);path=next(iter(d['sources']));d['sources'][path]='0'*64
  with self.assertRaisesRegex(ValueError,'Changed Intro source'):intro.verify(d)
if __name__=='__main__':unittest.main()
