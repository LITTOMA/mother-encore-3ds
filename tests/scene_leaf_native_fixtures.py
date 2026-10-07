#!/usr/bin/env python3
"""Explicit manual malformed-resource driver; never invoked by normal builds."""
import argparse, struct, subprocess, tempfile, zlib
from pathlib import Path

def fixtures(raw):
    for name, offset, value in [("version",8,9),("capability",12,9),("rules",16,9),("family",20,0),("crc",32,0)]:
        b=bytearray(raw);struct.pack_into("<I",b,offset,value);yield name,bytes(b)
    n=88
    def u():
        nonlocal n
        v=struct.unpack_from("<I",raw,n)[0];n+=4;return v
    def skip_string():
        nonlocal n
        size=u();n+=size
    for _ in range(5):skip_string()
    for _ in range(u()):skip_string();n+=32
    count=u()
    if not count:raise ValueError("Actual record roster missing")
    b=bytearray(raw);struct.pack_into("<I",b,n+12,999)
    struct.pack_into("<I",b,32,zlib.crc32(b[88:]));yield "unknown-kind",bytes(b)
    b=bytearray(raw);struct.pack_into("<I",b,n+20,999)
    struct.pack_into("<I",b,32,zlib.crc32(b[88:]));yield "unknown-method-mode",bytes(b)
    b=bytearray(raw);struct.pack_into("<I",b,n+24,0x7fc00000)
    struct.pack_into("<I",b,32,zlib.crc32(b[88:]));yield "nonfinite-speed",bytes(b)
    yield "truncated",raw[:-1]

def main():
    p=argparse.ArgumentParser();p.add_argument("--loader",type=Path,required=True)
    p.add_argument("resources",type=Path,nargs=8,help="leaf, tree, camera, transitions, birds, dropped, phone, melody")
    a=p.parse_args()
    subprocess.run([str(a.loader),*[str(v)for v in a.resources]],check=True)
    with tempfile.TemporaryDirectory(prefix="manual-scene-leaf-")as d:
        for name,raw in fixtures(a.resources[0].read_bytes()):
            f=Path(d)/(name+".pack");f.write_bytes(raw)
            subprocess.run([str(a.loader),str(f),*[str(v)for v in a.resources[1:]],"--expect-reject"],check=True)
if __name__=="__main__":main()
