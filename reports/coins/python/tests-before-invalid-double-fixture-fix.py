import copy,hashlib,json,struct,tempfile,unittest,zlib
from pathlib import Path
from tools import coins as b,run_coin_reference as native
from tools.coin_reference_protocol import validate

class CoinTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.recipe,cls.model,cls.proof=b.source();cls.blob=b.encode(cls.model);cls.files,cls.copied,cls.native_proof=native.source();cls.config=json.loads(cls.files['input.json']);cls.doc=json.loads((b.ROOT/cls.recipe['direction_reference']['path']).read_bytes());cls.root=b.ROOT/'build/tests';cls.root.mkdir(parents=True,exist_ok=True)
 def temporary(self):
  t=tempfile.TemporaryDirectory(prefix='coin-',dir=self.root);self.addCleanup(t.cleanup);return Path(t.name)
 def changed(self,at,value):
  blob=bytearray(self.blob);struct.pack_into('<I',blob,at,value);struct.pack_into('<I',blob,16,zlib.crc32(blob[32:]));return blob
 def test_source_policy_bindings_and_binary_roundtrip(self):
  data=b.decode(self.blob);self.assertEqual(len(self.proof['sources']),19);self.assertEqual(len(data['directions']),360*8);self.assertEqual(len(data['atlas']),40*24);self.assertEqual(len(data['assets']),6);self.assertEqual(self.model['factory_parent'],'Objects');self.assertEqual(data['meta'][0],1);self.assertEqual(len(self.model['clips']),4);self.assertEqual(b.encode(self.model),self.blob)
 def test_all_truncations_crc_and_trailing_bytes(self):
  for n in range(len(self.blob)):
   with self.assertRaises(ValueError):b.decode(self.blob[:n])
  changed=bytearray(self.blob);changed[-1]^=1
  for value in [None,changed,self.blob+b'\0']:
   with self.assertRaises(ValueError):b.decode(value)
 def test_header_version_capability_directory_and_pin_reject(self):
  changes=[(at,0xffffffff)for at in [0,8,12,20,24,28,32,52,56,60]]+[(64+16*n+4*field,0xffffffff)for n in range(9)for field in range(4)]
  for at,value in changes:
   with self.subTest(at=at),self.assertRaises(ValueError):b.decode(self.changed(at,value))
 def test_record_domains_and_owned_references(self):
  policy,meta,clips,keys,directions,atlas,colors,assets,pool=[struct.unpack_from('<I',self.blob,68+16*n)[0]for n in range(9)]
  changes=[policy+4*n for n in range(28)]+[meta+4*n for n in range(24)]+[clips+4*n for n in range(6)]+[keys+4*n for n in range(6)]+[directions,atlas,atlas+4,atlas+8,atlas+16,colors,colors+32,assets,assets+20,pool]
  for at in changes:
   with self.subTest(at=at),self.assertRaises(ValueError):b.decode(self.changed(at,0xffffffff))
  # Repaired CRC still cannot admit duplicated clip identities or hidden strings.
  for at,value in [(clips+24,struct.unpack_from('<I',self.blob,clips)[0]),(meta+28,0),(atlas+24,0)]:
   with self.subTest(at=at),self.assertRaises(ValueError):b.decode(self.changed(at,value))
 def test_model_unknown_fields_wrong_types_and_spans(self):
  mutations=[lambda m:m.update(extra=1),lambda m:m['policy'].update(extra=1),lambda m:m['meta'].update(cash_amount=True),lambda m:m['meta'].update(sparkle_playing=1),lambda m:m['policy'].update(deceleration=float('nan')),lambda m:m.update(factory_parent='../escape'),lambda m:m.update(radii=[0,7]),lambda m:m.update(colors=[[0,0,0,2],[0,0,0,1]]),lambda m:m['directions'].__setitem__(0,[0,0]),lambda m:m['atlas'].__setitem__(0,[0,0,0,1]),lambda m:m['clips'][0].update(loop=1),lambda m:m['clips'][0].update(extra=1),lambda m:m['clips'][0].update(role='unknown'),lambda m:m['clips'][1].update(id=m['clips'][0]['id']),lambda m:m['clips'][0]['keys'][0].update(target=True),lambda m:m['clips'][0]['keys'][0].update(update=99),lambda m:m['clips'][0]['keys'][0].update(extra=1),lambda m:m['clips'][0]['keys'][0].update(time=-1),lambda m:m['paths'].update(extra='unknown')]
  for mutate in mutations:
   m=copy.deepcopy(self.model);mutate(m)
   with self.subTest(mutate=mutate),self.assertRaises(ValueError):b.encode(m)
 def test_recipe_source_symbols_and_direction_proof_reject(self):
  path=self.temporary()/'recipe.json';mutations=[lambda p:p.update(schema=True),lambda p:p.update(commit='0'*40),lambda p:p.update(extra=1),lambda p:p['sources'].update({p['paths']['script']:'0'*64}),lambda p:p['symbols']['script'].update(_collect='0'*64),lambda p:p['paths'].update(extra='unknown'),lambda p:p['clips'][0].update(name='unknown'),lambda p:p['clips'][0].update(id=True),lambda p:p['direction_reference'].update(sha256='0'*64),lambda p:p['direction_reference'].update(path='../escape.json')]
  for mutate in mutations:
   p=copy.deepcopy(self.recipe);mutate(p);path.write_text(json.dumps(p))
   with self.subTest(mutate=mutate),self.assertRaises((ValueError,KeyError)):b.source(path)
 def test_observer_inputs_and_typed_original_protocol(self):
  self.assertEqual((len(self.config['cases']),len(self.config['spawner_cases'])),(13,3));self.assertEqual(len(self.copied),13);self.assertEqual(self.native_proof['protocol_sha256'],hashlib.sha256((b.ROOT/'tools/coin_reference_protocol.py').read_bytes()).hexdigest());self.assertEqual(validate(self.doc,self.config,{c['name']for c in self.model['clips']}),self.doc);self.assertEqual((len(self.doc['rows']),len(self.doc['contacts']),len(self.doc['spawners'])),(1573,9,33));self.assertIn('not production media/wholeRoom',self.native_proof['scope'])
 def test_unknown_protocol_version_fields_and_nonfinite_values(self):
  mutations=[lambda d:d.update(schema=True),lambda d:d.update(schema=3),lambda d:d.update(extra=1),lambda d:d['godot'].update(major=True),lambda d:d['directions'].__setitem__(0,[0,0]),lambda d:d['rows'].pop(),lambda d:d['rows'][0].update(extra=1),lambda d:d['rows'][0].update(frame=True),lambda d:d['rows'][0].update(alive=1),lambda d:d['rows'][0].update(physics_frame=99),lambda d:d['rows'][0]['state'].update(speed='000000000000f87f'),lambda d:d['rows'][0]['state'].update(time=0x7fc00000),lambda d:d['rows'][0]['state'].update(grounded=1),lambda d:d['rows'][0]['state'].update(animation='unknown'),lambda d:d['rows'][0]['state'].update(position=[0x7fc00000,0]),lambda d:d['contacts'][0].update(kind='unknown'),lambda d:d['contacts'][0].update(identity=99),lambda d:d['contacts'][0].update(case=True),lambda d:d['contacts'].reverse(),lambda d:d['spawners'][0].update(timer='000000000000f87f'),lambda d:d['spawners'][0].update(remaining=True),lambda d:d['spawners'][-1].pop('rng_tail'),lambda d:d['spawners'][2]['coins'][0].update(parent='Unknown'),lambda d:d['spawners'][2]['live'][0]['state'].update(extra=1),lambda d:d['spawners'][3]['coins'].clear(),lambda d:d['spawners'][2]['live'][0].update(ordinal=True)]
  for mutate in mutations:
   d=copy.deepcopy(self.doc);mutate(d)
   with self.subTest(mutate=mutate),self.assertRaises(ValueError):validate(d,self.config,{c['name']for c in self.model['clips']})

if __name__=='__main__':unittest.main()
