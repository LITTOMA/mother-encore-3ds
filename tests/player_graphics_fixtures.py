#!/usr/bin/env python3
"""Explicit manual parser cases; every malformed pack reaches real C++ loader."""
import argparse,json,struct,subprocess,tempfile,zlib
from pathlib import Path
def fixtures(raw):
    def changed(at,value):
        b=bytearray(raw);struct.pack_into('<I',b,at,value);struct.pack_into('<I',b,20,zlib.crc32(b[128:]));return b
    proof=bytearray(raw);proof[92:124]=bytes(32)
    return {'version':changed(8,2),'capability':changed(28,2),'rules':changed(32,2),'metadata':changed(124,1),'unknown-family':changed(24,0),'unknown-source-texture':changed(164,0),'unsupported-filter':changed(180,0),'unsupported-repeat':changed(184,1),'missing-proof':proof,'crc':raw[:-1]+bytes([raw[-1]^1]),'truncated':raw[:-1]}
def main():
    p=argparse.ArgumentParser();p.add_argument('--cli',type=Path,required=True);p.add_argument('--root',type=Path,required=True);p.add_argument('--pack',type=Path,required=True);p.add_argument('--global-ir',type=Path,required=True);a=p.parse_args();d=json.loads(a.global_ir.read_text(encoding='utf-8'));identity=[str(d['scene_id']),d['commit'],d['source_sha256']]
    subprocess.run([str(a.cli),'--reader',str(a.root),str(a.pack),*identity],check=True)
    with tempfile.TemporaryDirectory(prefix='player-graphics-manual-')as tmp:
        for name,raw in fixtures(a.pack.read_bytes()).items():
            path=Path(tmp)/(name+'.encgraphics');path.write_bytes(raw);subprocess.run([str(a.cli),'--expect-reject',str(a.root),str(path),*identity],check=True)
if __name__=='__main__':main()
