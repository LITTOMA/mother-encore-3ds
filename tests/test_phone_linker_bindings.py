"""Real linked Room/AudioIR equivalence and strict Phone linker recipe checks."""
import copy,json
from pathlib import Path
import unittest
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
  self.assertEqual(self.audio,linker.build_audio(self.root,self.room));raw=(self.root/'romfs/data/opening.encaudio').read_bytes();parsed=audio_asset.parse_bank(raw)
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
if __name__=='__main__':unittest.main()
