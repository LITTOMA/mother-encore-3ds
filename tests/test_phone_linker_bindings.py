"""Real linked Room/AudioIR equivalence and strict Phone linker recipe checks."""
import copy,hashlib,json
from pathlib import Path
import unittest
from unittest.mock import patch
from tools import phone_linker_bindings as recipe
from tools import link_phone_content as linker,audio_asset
from tools.extract_native_content import Extractor
ROOT=Path(__file__).resolve().parents[1]
class PhoneLinkerBindingTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.root=linker.ROOT;cls.c,cls.actors=recipe.load(cls.root);cls.room=recipe.read(cls.root/'content/native-opening.json');cls.audio=recipe.read(cls.root/'content/native-audio.json')
 def reject(self,change):
  c=copy.deepcopy(self.c);change(c)
  with self.assertRaises(ValueError):recipe.load(self.root,c)
 def test_audio_ir_and_existing_binary_exact(self):
  self.assertEqual(self.audio,linker.build_audio(self.root,self.room));raw=(self.root/'romfs/sound/banks/opening.encaudio').read_bytes();parsed=audio_asset.parse_bank(raw)
  self.assertEqual(raw,audio_asset.build_bank(parsed['assets'],parsed['master_db'],parsed['silence_db']))
 def test_actual_room_append_and_commands_exact(self):
  fresh,_=Extractor(self.root).run();self.assertEqual(self.room['strings'],fresh['strings']);self.assertEqual(self.room['sections'],fresh['sections']);self.assertEqual({'None':65535,'Ninten':0,'Carol':5},self.actors)
 def test_unknown_schema_fields_and_paths(self):
  self.reject(lambda c:c.update(schema=True));self.reject(lambda c:c.update(schema=2));self.reject(lambda c:c.update(unknown=0));self.reject(lambda c:c['paths'].update(scene='../bad'))
 def test_stale_source(self):self.reject(lambda c:c['sources'].update({c['paths']['npc_script']:'0'*64}))
 def test_bad_actor_and_stable_identity(self):
  self.reject(lambda c:c['actors'][1].update(reference='Absent'));self.reject(lambda c:c['carol'].update(actor_stable_id=99))
  self.reject(lambda c:c['carol'].update(direction=[1,0]))
 def test_audio_ids_must_match_actual_room_sources(self):
  c=copy.deepcopy(self.c);indices=[i for i,r in enumerate(c['audio'])if r['identity']['kind']=='stable'];a,b=indices[:2]
  c['audio'][a]['identity'],c['audio'][b]['identity']=c['audio'][b]['identity'],c['audio'][a]['identity']
  with self.assertRaises(ValueError):recipe.audio(self.root,self.room,c)
 def test_duplicate_and_unsafe_audio(self):
  self.reject(lambda c:c['audio'][1].update(pcm=c['audio'][0]['pcm']));self.reject(lambda c:c['audio'][0].update(pcm='../bad.pcm'));self.reject(lambda c:c['audio'][0]['identity'].update(kind='execute'))
 def test_source_rate_and_unknown_conversion(self):
  index=next(i for i,r in enumerate(self.c['audio'])if r['conversion']is not None)
  self.reject(lambda c:c['audio'][index]['conversion'].update(source_sample_rate=48000));self.reject(lambda c:c['audio'][index]['conversion'].update(output_sample_rate=192000));self.reject(lambda c:c['audio'][index]['conversion'].update(unknown=0))
 def test_ordinary_audio_compile_gate(self):
  audio=copy.deepcopy(self.audio);audio['assets'][0]['pcm_path']='audio/stale.pcm'
  with self.assertRaises(ValueError):recipe.verify_audio(audio,self.root)
 def intro_document(self):return recipe.read(self.root/self.c['introduction_audio'])
 def reject_intro(self,change,linked=False):
  document=copy.deepcopy(self.intro_document());change(document);original=recipe.read;reference=(self.root/self.c['introduction_audio']).resolve()
  def read(path):return document if Path(path).resolve()==reference else original(path)
  with patch.object(recipe,'read',side_effect=read),self.assertRaises(ValueError):
   if linked:recipe.audio(self.root,self.room,self.c)
   else:recipe.introduction_audio(self.root,self.c)
 def test_intro_author_rows_and_actual_linked_prefix(self):
  rows=recipe.introduction_audio(self.root,self.c)
  self.assertEqual([r['identity']['value']for r in rows],[1301,1302,1303,1304,1305])
  self.assertTrue(all(r['gain_db']==0 and r['conversion']is None for r in rows))
  linked=recipe.audio(self.root,self.room,self.c)['assets']
  self.assertEqual(len(linked),21)
  self.assertEqual([r['stable_id']for r in linked[-5:]],[1301,1302,1303,1304,1305])
 def test_intro_schema_pin_unknown_hash_paths_and_gain(self):
  def row(c):return next(iter(c['audio'].values()))
  changes=[lambda c:c.update(schema=True),lambda c:c.update(schema=2),lambda c:c.update(commit='0'*40),lambda c:row(c).update(extra=1),lambda c:row(c).update(stable_id=True),lambda c:row(c).update(source_sha256='0'*64),lambda c:row(c).update(import_sha256='0'*64),lambda c:row(c).update(pcm_path='../escaped.pcm'),lambda c:row(c).update(gain_db=True),lambda c:row(c).update(gain_db=1),lambda c:row(c).update(gain_db=float('nan'))]
  for change in changes:
   with self.subTest(change=change):self.reject_intro(change)
  config=copy.deepcopy(self.c);config['introduction_audio']='../outside.json'
  with self.assertRaises(ValueError):recipe.introduction_audio(self.root,config)
 def test_intro_duplicate_id_pcm_and_source(self):
  for field in ('stable_id','pcm_path','source_path'):
   def change(c,field=field):
    rows=list(c['audio'].values());rows[1][field]=rows[0][field]
   with self.subTest(field=field):self.reject_intro(change)
 def test_intro_cross_phone_id_pcm_and_source_conflict(self):
  existing=self.c['audio'][0]
  identity=self.audio['assets'][0]['stable_id']
  self.reject_intro(lambda c:next(iter(c['audio'].values())).update(stable_id=identity),linked=True)
  self.reject_intro(lambda c:next(iter(c['audio'].values())).update(pcm_path=existing['pcm']),linked=True)
  def source_conflict(c):
   source=existing['source'];row=next(iter(c['audio'].values()));upstream=self.root/'upstream/MOTHER-Encore'
   row.update(source_path='res://'+source,source_sha256=hashlib.sha256((upstream/source).read_bytes()).hexdigest(),import_sha256=hashlib.sha256((upstream/(source+'.import')).read_bytes()).hexdigest())
  self.reject_intro(source_conflict,linked=True)
if __name__=='__main__':unittest.main()
