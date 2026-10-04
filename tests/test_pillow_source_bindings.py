"""Pillow/Minnie content bindings, source references and append identity gates."""
import copy,json,os,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(os.environ.get('ENCORE_SOURCE_ROOT',Path(__file__).resolve().parents[1])).resolve()
sys.path[:0]=[str(ROOT),str(ROOT/'tools')]
from tools import pillow_source_bindings as binding,pillow_dialogue,link_pillow_content
from tools.extract_battle_entry import Extractor
class PillowSourceBindings(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.recipe=binding.read(binding.IR)
 def reject(self,path,value):
  recipe=copy.deepcopy(self.recipe);target=recipe
  for key in path[:-1]:target=target[key]
  target[path[-1]]=value
  with self.assertRaises((ValueError,KeyError,TypeError,IndexError,StopIteration)):binding.load(ROOT,recipe)
 def test_default_source_emissions_and_tutorial_choice_recipe(self):
  b=binding.load(ROOT);ex=Extractor(ROOT);docs=pillow_dialogue.load_documents(ex);rows,translations=link_pillow_content.texts(ex,docs);ids={r['identity']:r['id']for r in rows}
  end=pillow_dialogue.return_duration(ex.text(b['append']['dialogue_script']))
  for path in b['linears']:
   commands=pillow_dialogue.compile_linear(path,docs[path],end,ids,*[b['facts'][k]['value']for k in ['turn','move','jump']],root=ROOT)
   self.assertEqual(len(commands),len(b['linears'][path]['commands']))
   self.assertEqual(commands[0]['kind'],'BeginCutscene')
  group=pillow_dialogue.tutorial_graph(docs[b['paths']['tutorial']],translations,end,root=ROOT)['choice_groups'][0]
  actual=binding.read(ROOT/'content/native-dialogue-choices.json')['groups'][b['append']['choices_index']];self.assertEqual(group,actual)
 def test_version_pin_unknown_fields_and_duplicate_keys(self):
  for path,value in [(['schema'],True),(['schema'],2),(['kind'],'unknown'),(['commit'],'0'*40)]:self.reject(path,value)
  recipe=copy.deepcopy(self.recipe);recipe['unknown']=0
  with self.assertRaises(ValueError):binding.load(ROOT,recipe)
  with tempfile.TemporaryDirectory(dir=ROOT/'build',prefix='pillow-json-')as name:
   p=Path(name)/'duplicate.json';p.write_text('{"schema":1,"schema":1}',encoding='utf-8')
   with self.assertRaises(ValueError):binding.read(p)
 def test_source_fingerprint_documents_and_defaults(self):
  path=self.recipe['paths']['attack'];self.reject(['sources',path],'0'*64);self.reject(['documents',path,'0','actors','pillow'],'Objects/Absent');self.reject(['facts','jump','value'],99)
 def test_actor_profile_node_and_parent_namespace(self):
  for path,value in [(['append','actors','Minnie'],3),(['append','actors','Pillow'],True),(['append','profile_index'],7),(['append','node'],'Objects/Absent'),(['append','sprite'],'Other'),(['append','direction'],[1,0]),(['append','motion_id'],5),(['append','clip_alias'],'Fake Idle'),(['append','template_clip'],'Fake Idle')]:self.reject(path,value)
 def test_unsafe_artifact_selectors_are_rejected(self):
  for path in [['append','world_manifest'],['append','attack_receipt'],['append','scene'],['append','native_receipts',0,'path']]:self.reject(path,'../outside.json')
 def test_battle_catalog_and_choice_identity(self):
  for path,value in [(['append','battle_enemy'],'doll'),(['append','battle_path'],'data/doll-entry.encbattle'),(['append','battle_id'],99),(['append','choices_index'],0),(['append','choices_id'],'unknown'),(['tutorial','choice_groups',0,'cancel_target_pc'],0)]:self.reject(path,value)
 def test_unknown_emission_reference_opcode_phase_and_source_payload(self):
  path=self.recipe['paths']['attack'];self.reject(['linears',path,'commands',0,'kind'],'IgnoreUnknown');self.reject(['linears',path,'commands',0,'phrase'],99)
  i=next(i for i,c in enumerate(self.recipe['linears'][path]['commands'])if c['kind']=='MoveActorPath');self.reject(['linears',path,'commands',i,'path'],{'$unknown':['0']})
  i=next(i for i,c in enumerate(self.recipe['linears'][path]['commands'])if c['kind']=='JumpActor');self.reject(['linears',path,'commands',i,'flags'],True);self.reject(['linears',path,'commands',i,'value'],99)
 def test_changed_callers_cannot_bypass_source_checked_defaults(self):
  b=binding.load(ROOT);path=b['paths']['attack'];doc=b['documents'][path];ids={k:v for k,v in binding.read(ROOT/'content/house-source-bindings.json')['pillow']['text_ids'].items()};end=pillow_dialogue.return_duration((ROOT/'upstream/MOTHER-Encore'/b['append']['dialogue_script']).read_text(encoding='utf-8'))
  with self.assertRaises(ValueError):binding.linear(path,doc,end,ids,99,b['facts']['move']['value'],b['facts']['jump']['value'],ROOT)
  ids[next(iter(ids))]=999
  with self.assertRaises(ValueError):binding.linear(path,doc,end,ids,*[b['facts'][k]['value']for k in ['turn','move','jump']],ROOT)
if __name__=='__main__':unittest.main()
