#!/usr/bin/env python3
"""Default naming source/data equivalence and fail-closed recipe compilation."""
from pathlib import Path
import copy,hashlib,json,struct,sys,unittest,zlib
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools import new_game_assets as assets
from tools import naming_presentation_bindings as recipe

def candidate():
 ex,r,_=assets.base();old=json.loads((assets.ROOT/'content/native-new-game.json').read_text(encoding='utf-8'))
 for k in ('rects','font_height','native_layout_sha256','outputs','resources'):r[k]=copy.deepcopy(old[k])
 for panel in r['panels']:
  for key,neighbors,rect in zip(panel,assets.graph(panel,r['groups'],r['columns'],r['rects']),r['rects']):key.update(neighbors=neighbors,rect=rect)
 assets.resolve_bindings(ex,r);return r,old

def prepare_fixtures():
 r,_=candidate();assets.verify(r);path=assets.ROOT/'build/naming-bindings-fixtures';path.mkdir(parents=True,exist_ok=True);(path/'default.encnewgame').write_bytes(assets.encode(r));return path

REFERENCE=json.loads((ROOT/'tests/fixtures/naming-romfs-paths.json').read_text(),object_pairs_hook=recipe.pairs)
ORIGINAL_PAYLOAD_SHA='bc79853503a565c772a4316e8fa7d6ce9e320b66a11c47052397141f555f0395'

def original_payload(raw):
 # Compare the historical bytes without excluding path bytes or replacing the
 # reviewed digest. Only the eleven explicitly reviewed length-prefixed paths
 # may be restored, exactly once each; every other gameplay byte is preserved.
 if len(raw)<140 or struct.unpack_from('<8s4I',raw)!= (b'ENCNAMES',3,len(raw),zlib.crc32(raw[24:]),2):raise ValueError('Invalid naming reference header/CRC')
 payload=raw[24:]
 for current,previous in REFERENCE['paths'].items():
  a=current.encode();a=struct.pack('<I',len(a))+a
  b=previous.encode();b=struct.pack('<I',len(b))+b
  if payload.count(a)!=1:raise ValueError('Missing/duplicate naming resource path: '+current)
  payload=payload.replace(a,b,1)
 if (hashlib.sha256(payload[:-116]).hexdigest()!=ORIGINAL_PAYLOAD_SHA or
     hashlib.sha256(payload).hexdigest()!=REFERENCE['historical_full_payload_sha256']):raise ValueError('Original naming gameplay payload changed')
 return payload[:-116]

def reseal(raw):
 raw=bytearray(raw);struct.pack_into('<II',raw,12,len(raw),zlib.crc32(raw[24:]));return bytes(raw)

class NamingBindingsTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.config,cls.facts,cls.binding=recipe.load(assets.ROOT);cls.r,cls.old=candidate()
 def reject(self,change):
  c=copy.deepcopy(self.config);change(c)
  with self.assertRaises(ValueError):recipe.load(assets.ROOT,document=c)
 def test_original_ir_and_payload(self):
  for k in self.old:
   if k not in ('sources','dependencies','schema','presentation','presentation_sha256'):self.assertEqual(self.old[k],self.r[k],k)
  raw=assets.encode(self.r);self.assertEqual((3,2),(struct.unpack_from('<I',raw,8)[0],struct.unpack_from('<I',raw,20)[0]))
  self.assertEqual(REFERENCE['schema'],1);self.assertEqual(REFERENCE['historical_payload_sha256'],ORIGINAL_PAYLOAD_SHA)
  self.assertEqual([a['path']for a in self.r['resources']],list(REFERENCE['paths']))
  self.assertEqual(len(set(REFERENCE['paths'].values())),len(REFERENCE['paths']))
  self.assertEqual(hashlib.sha256(original_payload(raw)).hexdigest(),ORIGINAL_PAYLOAD_SHA)
  self.assertEqual(raw,assets.PACK.read_bytes())
  assets.verify(self.r)
 def test_path_reference_rejects_missing_duplicate_and_unknown_paths(self):
  raw=assets.encode(self.r);path=next(iter(REFERENCE['paths'])).encode();token=struct.pack('<I',len(path))+path
  for replacement in (b'',token+token,struct.pack('<I',len(path))+b'x'*len(path)):
   with self.subTest(replacement=replacement),self.assertRaises(ValueError):original_payload(reseal(raw.replace(token,replacement,1)))
 def test_path_reference_keeps_gameplay_bytes_checked(self):
  raw=bytearray(assets.encode(self.r));struct.pack_into('<I',raw,24,self.r['maximum']+1)
  with self.assertRaisesRegex(ValueError,'gameplay payload changed'):original_payload(reseal(raw))
  raw=bytearray(assets.encode(self.r));raw[-1]^=1
  with self.assertRaisesRegex(ValueError,'gameplay payload changed'):original_payload(reseal(raw))
 def test_path_reference_checks_header_version_crc_and_truncation(self):
  raw=assets.encode(self.r)
  for candidate_raw in (raw[:100],raw[:-1],raw[:8]+struct.pack('<I',99)+raw[12:],raw[:16]+b'\0'*4+raw[20:]):
   with self.subTest(size=len(candidate_raw)),self.assertRaises(ValueError):original_payload(candidate_raw)
 def test_source_tuning(self):
  self.assertEqual(self.facts['error_duration'],2.0);self.assertEqual(self.facts['navigation_point'],[8-8/6,1+4]);self.assertEqual(self.facts['dim_color'],0xff866c7a);self.assertEqual(self.binding['source_width'],320)
 def test_unknown_missing_version(self):
  self.reject(lambda c:c.update(unknown=0));self.reject(lambda c:c.pop('routing'));self.reject(lambda c:c.update(schema=2));self.reject(lambda c:c.update(schema=True))
 def test_source_review_and_fingerprint(self):
  self.reject(lambda c:c['sources'].update({c['paths']['script']:'0'*64}))
  self.reject(lambda c:c['review'][c['paths']['script']].update(_highlight_color='func _highlight_color(): pass\n'))
 def test_unknown_action_condition_and_ref(self):
  self.reject(lambda c:c['routing'][0][0].update(op='execute'))
  self.reject(lambda c:c['routing'][1][2].update(condition='eval'))
  self.reject(lambda c:c['routing'][0][0].update(group=99))
 def test_duplicate_paths_roles_groups(self):
  self.reject(lambda c:c['groups'].__setitem__(1,c['groups'][0]));self.reject(lambda c:c['presentation']['sounds'].__setitem__(1,0));self.reject(lambda c:c['panel_paths'].__setitem__(1,c['panel_paths'][0]))
 def test_unsafe_paths_and_source_commands(self):
  self.reject(lambda c:c['paths'].update(scene='../bad'));self.reject(lambda c:c['commands'][0].update(text='MENU_OK'))
 def test_stale_ir_compile_gate(self):
  for change in (lambda r:r['presentation'].update(cursor=99),lambda r:r.update(schema=2),lambda r:r.update(presentation_sha256='0'*64),lambda r:r.update(error_duration=.8)):
   r=copy.deepcopy(self.r);change(r)
   with self.assertRaises(ValueError):assets.encode(r)
  r=copy.deepcopy(self.r);r['unknown']=0
  with self.assertRaises(ValueError):assets.verify(r)
 def test_duplicate_json(self):
  with self.assertRaises(ValueError):json.loads('{"schema":1,"schema":1}',object_pairs_hook=recipe.pairs)
if __name__=='__main__':
 if '--prepare-only'in sys.argv:print(prepare_fixtures())
 else:unittest.main()
