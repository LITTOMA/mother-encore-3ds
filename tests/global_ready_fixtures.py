#!/usr/bin/env python3
"""Explicit manual parser cases. Never run by normal production conversion."""
from pathlib import Path
import argparse,struct,zlib,subprocess,tempfile
def main():
    p=argparse.ArgumentParser();p.add_argument('driver',type=Path);p.add_argument('root',type=Path);p.add_argument('pack',type=Path);p.add_argument('identities',nargs=7);a=p.parse_args()
    raw=a.pack.read_bytes()
    def invoke(path,reject):
        subprocess.run([str(a.driver),'--expect-reject'if reject else'--reader',str(a.root),str(path)]+a.identities,check=True)
    invoke(a.pack,False)
    variants={}
    for name,offset,value in [('version',8,999),('capability',28,999),('rules',32,999),('metadata',124,1)]:
        b=bytearray(raw);struct.pack_into('<I',b,offset,value);variants[name]=b
    b=bytearray(raw);b[-1]^=1;variants['crc']=b
    # The last source step is a length-prefixed UTF-8 string. Its fixed
    # spelling is not an execution expectation: find it from the actual pack.
    b=bytearray(raw);tail=b.rfind(b'_load_default_save()');assert tail>=128
    struct.pack_into('<I',b,tail-8,999);struct.pack_into('<I',b,20,zlib.crc32(b[128:]));variants['unknown-opcode']=b
    with tempfile.TemporaryDirectory(prefix='global-ready-manual-')as tmp:
        for name,b in variants.items():
            path=Path(tmp)/(name+'.encready');path.write_bytes(b);invoke(path,True)
if __name__=='__main__':main()
