import copy,hashlib,json,struct,unittest
from pathlib import Path
from types import SimpleNamespace as Node
from tools import field_ready_rng as f,mick_treats as m
from tools.extract_battle_entry import Extractor
from tools.godot_text import Document,instantiate
ROOT=Path(__file__).resolve().parents[1]
class ReadyRngTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.folder=ROOT/'reports/iggy-tutorial/ready-rng-reference-1';cls.input=json.loads((cls.folder/'input.json').read_bytes());cls.ops=cls.input['ops'];cls.result=f.evaluate(cls.ops)
 def test_every_original_engine_value_and_state(self):
  proof=json.loads((self.folder/'proof.json').read_bytes());self.assertEqual(proof['engine']['hash'],'3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8');self.assertEqual(proof['operations'],3506);self.assertEqual(proof['sparkle_samples'],22)
  for name,digest in proof['files_sha256'].items():self.assertEqual(hashlib.sha256((self.folder/name).read_bytes()).hexdigest(),digest,name)
  for name,digest in proof['sources'].items():self.assertEqual(hashlib.sha256((ROOT/'upstream/MOTHER-Encore'/name).read_bytes()).hexdigest(),digest,name)
  raw=(self.folder/'reference.bin').read_bytes();self.assertEqual(struct.unpack_from('<8sI',raw),(b'ERDY362\n',len(self.ops)));self.assertEqual(len(raw),12+24*len(self.ops)+16)
  for i,row in enumerate(self.result['rows']):
   index,kind,value,state=struct.unpack_from('<IIdQ',raw,12+24*i)
   with self.subTest(i=i):self.assertEqual((index,kind,state),(i,['Seed','Randi','Randf','RandRange'].index(row['kind']),row['state']));self.assertEqual(struct.pack('<d',value),struct.pack('<d',row['value']))
  state,actual,mirror=struct.unpack_from('<QII',raw,12+24*len(self.ops));self.assertEqual(state,self.result['state']);self.assertEqual(actual,mirror)
 def test_actual_source_tree_operations_are_identical(self):
  ex=Extractor(ROOT);docs={}
  def load(path):
   if path not in docs:docs[path]=Document(path,ex.text(path))
   return docs[path]
  order=instantiate(load,m.SCENE);ops,result=f.replay(ex,order,m.READY_SCRIPTS);self.assertEqual(ops,self.ops);self.assertEqual(result,self.result)
 def test_existing_mick_pack_and_viewport_event_policy_unchanged(self):
  ir=m.build(ROOT);self.assertEqual((ir['actor']['rng_hi']<<32)|ir['actor']['rng_lo'],self.result['state']);self.assertEqual(len(ir['rng']),294)
  self.assertEqual(hashlib.sha256(m.encode(ir)).hexdigest(),'a7c50629baf53e2fb303ea5bc9f5cff4dd4eaa2a01f8583227640e6b36301e2c')
 def test_owned_sample_is_after_seed_and_no_extra_draw_required(self):
  row=self.result['samples']['Objects/Items/Item/Sparkles'];self.assertTrue(row['seeded']);self.assertEqual(row['operation'],3186);self.assertEqual(row['value'],6.629633243201263);self.assertEqual(int(row['value']),6);self.assertEqual(self.result['state'],621950984495336987)
 def test_unknown_or_bad_operation_rejected(self):
  for op in [{'kind':'Bad','path':'A'},{'kind':'Randi','path':'A','unused':0},{'kind':'Seed','path':'A','seed':True},{'kind':'Seed','path':'A','seed':-1},{'kind':'Seed','path':'A','seed':1<<32},{'kind':'Randi','path':''},{'kind':'Randi','path':None},{'kind':'RandRange','path':'A','low':0,'high':0},{'kind':'RandRange','path':'A','low':-1,'high':47},{'kind':'RandRange','path':'A','low':False,'high':47},{'kind':'RandRange','path':'A','low':0,'high':float('inf')},{'kind':'RandRange','path':'A','low':0,'high':float('nan')}]:
   with self.subTest(op=op),self.assertRaises(ValueError):f.evaluate([op])
  with self.assertRaises(ValueError):f.evaluate([{'kind':'RandRange','path':'A','low':0,'high':47}]*2)
 def test_source_tree_shape_and_unmapped_script_rejected(self):
  class Ex:
   def text(self,path):return 'func _ready():\n\trandi()\n'
  root=Node(path='.',script=None,name='root');a=Node(path='A',script='unknown.gd',name='A')
  for nodes,expected in [([a],set()),([root,root],set()),([root,Node(path='X/A',script=None,name='A')],set()),([root,a],set()),([root,a],{'unknown.gd'})]:
   with self.subTest(nodes=nodes),self.assertRaises(ValueError):f.operations(Ex(),nodes,expected)
 def test_changed_sparkle_ready_body_rejected(self):
  class Ex:
   def text(self,path):return 'func _ready():\n\tframe=int(rand_range(0, 48))\n'
  nodes=[Node(path='.',script=None,name='root'),Node(path='S',script='Scripts/misc/sparkles.gd',name='S')]
  with self.assertRaises(ValueError):f.operations(Ex(),nodes,{'Scripts/misc/sparkles.gd'})
if __name__=='__main__':unittest.main()
