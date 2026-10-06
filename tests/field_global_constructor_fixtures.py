#!/usr/bin/env python3
"""Manual parser cases; invokes the actual loader CLI, never native Ready."""
import argparse, json, struct, subprocess, tempfile, zlib
from pathlib import Path

def cases(raw):
    def mutate(offset, value, checksum=True):
        data=bytearray(raw);struct.pack_into('<I',data,offset,value)
        if checksum:struct.pack_into('<I',data,20,zlib.crc32(data[128:]))
        return data
    pos=128
    for _ in range(2):pos+=4+struct.unpack_from('<I',raw,pos)[0]
    first_role=pos+4
    kind=first_role+4
    for _ in range(2):kind+=4+struct.unpack_from('<I',raw,kind)[0]
    return {'version':mutate(8,2),'capability':mutate(28,2),
            'rules':mutate(32,2),'metadata':mutate(124,1),
            'role':mutate(first_role,0),'unknown-opcode':mutate(kind,99),
            'crc':mutate(kind,99,False),'truncated':raw[:-1]}

def main():
    p=argparse.ArgumentParser();p.add_argument('--cli',type=Path,required=True)
    p.add_argument('--pack',type=Path,required=True);p.add_argument('--ir',type=Path,required=True)
    a=p.parse_args();d=json.loads(a.ir.read_text(encoding='utf-8'))
    identity=[str(d['scene_id']),d['commit'],d['source_sha256']]
    subprocess.run([str(a.cli),'--reader',str(a.pack),*identity],check=True)
    with tempfile.TemporaryDirectory(prefix='global-constructor-manual-')as tmp:
        for name,raw in cases(a.pack.read_bytes()).items():
            out=Path(tmp)/(name+'.encnodeconstructor');out.write_bytes(raw)
            subprocess.run([str(a.cli),'--expect-reject',str(out),*identity],check=True)

if __name__=='__main__':main()
