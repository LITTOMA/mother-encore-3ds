#!/usr/bin/env python3
"""Download version-selected CIA packaging tools into tools/bin (Linux x86_64).
Makerom is hash-pinned. The old bannertool release has no published digest here;
its observed digest is recorded, NOT presented as a cryptographic trust pin.
Pass --bannertool-sha256 to enforce your independently checked digest.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path
import platform
import stat
import subprocess
import sys
import urllib.request
import zipfile
ROOT=Path(__file__).resolve().parents[1]
TOOLS=[('makerom','https://github.com/3DSGuy/Project_CTR/releases/download/makerom-v0.19.0/makerom-v0.19.0-ubuntu_x86_64.zip',
        '287b809dec064e0ad597e3d272c49ecb7eed41693d5ee6fef9d8a8aa24c2497e'),
       ('bannertool','https://github.com/Epicpkmn11/bannertool/releases/download/v1.2.2/bannertool.zip',None)]
def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--bannertool-sha256')
    ap.add_argument('--from-source',action='store_true',help='Build makerom and CTRTool from fixed official sources (compatible with the verified Debian 12 image)')
    a=ap.parse_args()
    if platform.system()!='Linux' or platform.machine() not in ('x86_64','AMD64'):
        raise SystemExit('Auto-bootstrap supports Linux x86_64; on other systems install matching official tool releases in PATH.')
    dest=ROOT/'tools/bin';dest.mkdir(parents=True,exist_ok=True);records=[]
    for name,url,pinned in TOOLS:
        if name=='makerom' and a.from_source:continue
        expected=pinned or a.bannertool_sha256
        req=urllib.request.Request(url,headers={'User-Agent':'encore-native-build/0.1'})
        with urllib.request.urlopen(req,timeout=60) as r:data=r.read(32*1024*1024)
        actual=hashlib.sha256(data).hexdigest()
        if expected and actual!=expected:raise SystemExit(f'{name}: SHA-256 mismatch; refusing to install')
        with zipfile.ZipFile(io.BytesIO(data)) as z:
            candidates=[i for i in z.infolist() if Path(i.filename).name==name and ('linux-x86_64' in i.filename.lower() or name=='makerom')]
            if len(candidates)!=1:raise SystemExit(f'{name}: ambiguous archive layout: {[i.filename for i in candidates]}')
            # Never extract untrusted archive paths; only write the selected executable.
            binary=z.read(candidates[0]);path=dest/name;path.write_bytes(binary);path.chmod(path.stat().st_mode|stat.S_IXUSR|stat.S_IXGRP|stat.S_IXOTH)
        records.append({'tool':name,'url':url,'archive_sha256':actual,'digest_pinned':bool(expected),'binary_sha256':hashlib.sha256(binary).hexdigest()})
        print(f'{name}: installed; archive sha256={actual}; pinned={bool(expected)}')
    (dest/'download-manifest.json').write_text(json.dumps(records,indent=2)+'\n')
    if a.from_source:
        subprocess.run([sys.executable,str(ROOT/'tools/build_ctr_tools.py')],check=True)
if __name__=='__main__':main()
