import copy,json,math,struct,tempfile,unittest,zlib
from pathlib import Path
from tools import dungeon_key_hud_graphics as g
class KeyHudGraphicsTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.model,_=g.source_model();cls.model.update(texture_bytes=57,texture_crc=zlib.crc32(b'fixture'));cls.blob=g.encode(cls.model)
 def invalid(self,raw):
  with self.assertRaises((ValueError,UnicodeError,struct.error,OverflowError)):g.parse(bytes(raw))
 def changed(self,at,value,fmt='<I'):
  b=bytearray(self.blob);struct.pack_into(fmt,b,at,value);struct.pack_into('<I',b,16,zlib.crc32(b[32:]));return b
 def test_source_font_and_layout(self):
  p,_=g.read();a,b=[json.loads((g.ROOT/k).read_text('utf-8'))for k in p['native_layouts']];self.assertEqual(a,b);self.assertEqual(len(a['rows']),5);self.assertEqual(g.parse(self.blob),self.model)
 def test_truncation_and_crc(self):
  for n in range(len(self.blob)):self.invalid(self.blob[:n])
  self.invalid(self.blob+b'x');b=bytearray(self.blob);b[-1]^=1;self.invalid(b)
 def test_unknown_header_paths(self):
  for at in [8,20,24,28]:self.invalid(self.changed(at,99))
  for at in [32,36,40,44,108,112,116,120,124]:self.invalid(self.changed(at,1))
 def test_bounds_and_nonfinite(self):
  for at,v in [(48,0),(48,1025),(52,0),(52,1025),(56,0),(56,8*1024*1024+1),(64,23),(64,1024*1024+1)]:self.invalid(self.changed(at,v))
  for at in range(72,104,4):
   for v in [math.nan,math.inf,-math.inf,65536.]:self.invalid(self.changed(at,v,'<f'))
  for at,v in [(72,0),(76,0),(80,1),(84,1),(80,1025),(84,1025)]:self.invalid(self.changed(at,v,'<f'))
 def test_unsafe_and_unowned_path(self):
  at=struct.unpack_from('<I',self.blob,40)[0]
  for c in [0,31,127,255,47,58,92]:
   b=bytearray(self.blob);b[at]=c;struct.pack_into('<I',b,16,zlib.crc32(b[32:]));self.invalid(b)
 def test_nested_unknown_version(self):self.invalid(self.changed(128+8,99))
 def test_review_fail_closed(self):
  p,_=g.read()
  with tempfile.TemporaryDirectory(dir=g.ROOT/'build')as tmp:
   path=Path(tmp)/'review.json'
   for mutation in [lambda v:v.update(schema=2),lambda v:v.update(extra=True),lambda v:v.update(indicator_sha256='0'*64),lambda v:v.update(font_pack_sha256='0'*64),lambda v:v['sources'].update({next(iter(v['sources'])):'0'*64}),lambda v:v.update(viewports=[[1,1],[2,2]])]:
    v=copy.deepcopy(p);mutation(v);path.write_text(json.dumps(v),encoding='utf-8')
    with self.assertRaises(ValueError):g.read(path)
   path.write_text('{"schema":1,"schema":1}',encoding='utf-8')
   with self.assertRaises(ValueError):g.read(path)
 def test_model_ownership_and_types(self):
  for k,v in [('extra',1),('width',True),('color',-1),('reference',[math.nan,1]),('texture','../bad.t3x'),('indicator',b'')]:
   m=copy.deepcopy(self.model);m[k]=v
   with self.assertRaises((ValueError,OverflowError,struct.error)):g.encode(m)
if __name__=='__main__':unittest.main()
