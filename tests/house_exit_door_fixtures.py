"""Explicit manual malformed fixtures submitted to the actual C++ Door reader."""
import argparse,json,struct,subprocess,tempfile,zlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--driver',required=True);p.add_argument('--root',type=Path,required=True);a=p.parse_args()
ir=json.loads((a.root/'content/native-house-exit-door.json').read_text());raw=(a.root/'romfs/data/house.encdoor').read_bytes();args=[str(ir['scene_id']),ir['commit'],ir['source_sha256']]
subprocess.run([a.driver,str(a.root/'romfs/data/house.encdoor')]+args,check=True)
cases={}
for name,offset,value in [('version',8,99),('family',28,0),('capability',32,99),('scene',36,0),('unknown-directory',128,99)]:
 b=bytearray(raw);struct.pack_into('<I',b,offset,value);cases[name]=b
b=bytearray(raw);b[-1]^=1;cases['crc']=b
row=struct.unpack_from('<I',raw,128+24*2+12)[0]
for name,offset,value in [('unknown-flags',row+32,512),('unknown-pause',row+36,99),('unknown-string',row+40,0xffffffff),('missing-child',row+12,0)]:
 b=bytearray(raw);struct.pack_into('<I',b,offset,value);struct.pack_into('<I',b,24,zlib.crc32(b[296:]));cases[name]=b
with tempfile.TemporaryDirectory()as d:
 for name,b in cases.items():
  f=Path(d)/(name+'.encdoor');f.write_bytes(b);subprocess.run([a.driver,str(f)]+args+['--expect-reject'],check=True)
print('Explicit manual source Door fixtures completed')
