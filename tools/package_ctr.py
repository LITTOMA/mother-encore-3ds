#!/usr/bin/env python3
"""Package a REAL ARM ELF. Refuses to manufacture placeholder console binaries."""
import argparse
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]
def tool(name):
    suffix='.exe' if os.name=='nt' else ''
    local=ROOT/'tools/bin'/(name+suffix)
    found=str(local) if local.is_file() else shutil.which(name)
    if not found:raise RuntimeError(f'Missing {name}; see docs/BUILD.md')
    return found

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--format',choices=['cia','cxi','cci'],default='cia')
    ap.add_argument('--unique-id',default=os.environ.get('ENCORE_UNIQUE_ID','0xF3E21'))
    a=ap.parse_args()
    try:
        uid=int(a.unique_id,0)
        if not 0x10000<=uid<=0xfffff:raise RuntimeError('Use a five-hex-digit non-system homebrew test ID')
        elf=ROOT/'dist/encore-native.elf';smdh=ROOT/'dist/encore-native.smdh';romfs=ROOT/'build/ctr/native-romfs'
        if not elf.is_file():raise RuntimeError('Missing ELF; first run make 3dsx with devkitARM')
        head=elf.read_bytes()[:52]
        if len(head)<52 or head[:6]!=b'\x7fELF\x01\x01' or struct.unpack_from('<H',head,18)[0]!=40:
            raise RuntimeError('Input is not a 32-bit little-endian ARM ELF')
        if not smdh.is_file() or smdh.read_bytes()[:4]!=b'SMDH':raise RuntimeError('Missing/invalid SMDH')
        if not (romfs/'data/opening.encroom').is_file():raise RuntimeError('Missing staged native content; first run make 3dsx')
        banner=ROOT/'build/ctr/banner.bin';banner.parent.mkdir(parents=True,exist_ok=True)
        subprocess.run([tool('bannertool'),'makebanner','-i',str(ROOT/'assets/banner.png'),'-a',str(ROOT/'assets/silence.wav'),'-o',str(banner)],check=True)
        extension={'cci':'3ds','cia':'cia','cxi':'cxi'}[a.format]
        output=ROOT/f'dist/encore-native.{extension}'
        args=[tool('makerom'),'-f',a.format,'-o',str(output),'-rsf',str(ROOT/'assets/cia.rsf'),
              '-target','t','-elf',str(elf),'-icon',str(smdh),'-banner',str(banner),
              f'-DAPP_UNIQUE_ID=0x{uid:05X}',f'-DAPP_ROMFS={romfs}']
        # Keep the CIA title version at RSF RemasterVersion 0. In makerom 0.19.0,
        # version.h's VER_MINOR=19 collides with the titleVersion[] enum index in
        # user_settings.c. Passing -ver/-minor writes outside its 3-element array.
        # App semver 0.1.0 and CIA install version are independent identities.
        subprocess.run(args,cwd=ROOT,check=True)
        if not output.is_file() or output.stat().st_size<512:raise RuntimeError('Packaging produced no valid-sized output')
        print(f'Packaged {output.name}; test title ID 00040000{uid:06X}00. This ID is NOT registered.')
        return 0
    except (ValueError,OSError,RuntimeError,subprocess.CalledProcessError) as e:
        print(f'PACKAGING ERROR: {e}',file=sys.stderr);return 1
if __name__=='__main__':raise SystemExit(main())
