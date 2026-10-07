#!/usr/bin/env python3
"""Explicit manual negative CLI; each generated candidate hits the real reader."""
import argparse,struct,subprocess,tempfile,zlib
from pathlib import Path
def main():
 p=argparse.ArgumentParser();p.add_argument('reader',type=Path);p.add_argument('resources',type=Path,nargs=6);a=p.parse_args()
 raw=a.resources[0].read_bytes()
 # Actual normal input must be accepted by the same typed loader executable.
 subprocess.run([str(a.reader)]+[str(v)for v in a.resources],check=True)
 cases={}
 for name,offset,value in [('version',8,99),('capability',12,99),('rules',16,99)]:
  v=bytearray(raw);struct.pack_into('<I',v,offset,value);cases[name]=v
 v=bytearray(raw);v[-1]^=1;cases['crc']=v
 # Source native metadata is no longer trusted just because its CRC matches.
 v=bytearray(raw);at=88
 for _ in range(6):n=struct.unpack_from('<I',v,at)[0];at+=4+n
 at+=16 # process/method uints and speed/blend floats
 n=struct.unpack_from('<I',v,at)[0];at+=4
 for _ in range(n):size=struct.unpack_from('<I',v,at)[0];at+=4+size+32
 count=struct.unpack_from('<I',v,at)[0];at+=4
 assert count>0
 struct.pack_into('<I',v,at+12,99)
 struct.pack_into('<I',v,32,zlib.crc32(v[88:]));cases['owner-opcode']=v
 v=bytearray(raw)+b'unknown';struct.pack_into('<I',v,28,len(v)-88);struct.pack_into('<I',v,32,zlib.crc32(v[88:]));cases['trailing-metadata']=v
 with tempfile.TemporaryDirectory()as d:
  for name,v in cases.items():
   out=Path(d)/(name+'.encclipnative');out.write_bytes(v)
   subprocess.run([str(a.reader),str(out)]+[str(v)for v in a.resources[1:]]+['--expect-reject'],check=True)
if __name__=='__main__':main()
