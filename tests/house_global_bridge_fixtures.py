#!/usr/bin/env python3
"""Explicit manual malformed-resource CLI; never invoked by normal builds."""
import argparse,struct,subprocess,tempfile,zlib
from pathlib import Path

def main():
 p=argparse.ArgumentParser();p.add_argument('loader');p.add_argument('root');p.add_argument('pack');p.add_argument('source',nargs=5,help='GLOBALDATA_ID PIN GLOBALDATA_SHA CHARACTER_ID CHARACTER_SHA');a=p.parse_args()
 original=Path(a.pack).read_bytes()
 cases={}
 for name,offset,value in [('version',8,2),('capability',28,2),('rules',32,2),('metadata_reserved',124,1)]:
  raw=bytearray(original);struct.pack_into('<I',raw,offset,value);cases[name]=raw
 raw=bytearray(original);raw[20]^=1;cases['crc']=raw
 raw=bytearray(original);raw[128]^=1;cases['character_dependency']=raw
 # Four IR hashes, leader text, declaration and mapping count precede roles.
 role=256+4+struct.unpack_from('<I',original,256)[0]+8
 raw=bytearray(original);struct.pack_into('<I',raw,role,99);cases['unknown_opcode']=raw
 raw=bytearray(original);marker=struct.pack('<I',4)+b'Node';at=raw.find(marker,role);assert at>=0;raw[at+7]=ord('X');cases['wrong_native_status']=raw
 with tempfile.TemporaryDirectory(prefix='house-global-bridge-manual-')as temp:
  for name,raw in cases.items():
   if name!='crc':struct.pack_into('<I',raw,20,zlib.crc32(raw[128:]))
   target=Path(temp)/(name+'.bin');target.write_bytes(raw)
   subprocess.run([a.loader,'--expect-reject',a.root,str(target),*a.source],check=True)
 print('Manual malformed House continuation resources rejected by actual typed loader')
if __name__=='__main__':main()
