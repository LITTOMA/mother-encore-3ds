#!/usr/bin/env python3
"""Explicit manual parser negatives; never called during development."""
import argparse,struct,subprocess,tempfile,zlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--loader',required=True);p.add_argument('--root',required=True);p.add_argument('--pack',required=True);p.add_argument('identities',nargs=5);a=p.parse_args()
raw=Path(a.pack).read_bytes();cases={}
for name,offset in [('version',8),('capability',28),('rules',32),('reserved',124),('crc',20)]:
 b=bytearray(raw);b[offset]^=0x80;cases[name]=b
b=bytearray(raw);b[128]^=1;struct.pack_into('<I',b,20,zlib.crc32(b[128:]));cases['wrong_bridge_dependency']=b
o=192
for _ in range(4):n=struct.unpack_from('<I',raw,o)[0];o+=4+n
o+=4
for _ in range(2):n=struct.unpack_from('<I',raw,o)[0];o+=4+n
o+=32
b=bytearray(raw);struct.pack_into('<I',b,o,99);struct.pack_into('<I',b,20,zlib.crc32(b[128:]));cases['unknown_valuekind']=b
cases['truncated']=raw[:-1];cases['trailing']=raw+b'\0'
with tempfile.TemporaryDirectory(prefix='house-status-manual-')as temp:
 for name,b in cases.items():
  path=Path(temp)/(name+'.pack');path.write_bytes(b)
  subprocess.run([a.loader,'--expect-reject',a.root,str(path)]+a.identities,check=True)
print('Explicit manual parser negatives complete; no source lifecycle fabricated')
