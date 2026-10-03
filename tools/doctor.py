#!/usr/bin/env python3
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
ROOT=Path(__file__).resolve().parents[1]
def main():
    dkp=Path(os.environ.get('DEVKITPRO','/opt/devkitpro'));arm=Path(os.environ.get('DEVKITARM',str(dkp/'devkitARM')))
    suffix='.exe' if os.name=='nt' else ''
    checks={
      'host_cmake':bool(shutil.which('cmake')),'host_cxx':bool(shutil.which('g++') or shutil.which('clang++') or shutil.which('cl')),
      'devkitARM':(arm/'bin'/('arm-none-eabi-g++'+suffix)).is_file(),
      '3ds_rules':(arm/'3ds_rules').is_file(),'libctru':(dkp/'libctru/lib/libctru.a').is_file(),
      'citro2d':(dkp/'libctru/lib/libcitro2d.a').is_file(),'citro3d':(dkp/'libctru/lib/libcitro3d.a').is_file(),
      '3dsxtool':(dkp/'tools/bin'/('3dsxtool'+suffix)).is_file(),
      'smdhtool':(dkp/'tools/bin'/('smdhtool'+suffix)).is_file(),
      'makerom':bool(shutil.which('makerom') or (ROOT/'tools/bin'/('makerom'+suffix)).is_file()),
      'bannertool':bool(shutil.which('bannertool') or (ROOT/'tools/bin'/('bannertool'+suffix)).is_file())}
    result={'system':platform.platform(),'python':platform.python_version(),'DEVKITPRO':str(dkp),'DEVKITARM':str(arm),'checks':checks}
    compiler=arm/'bin'/('arm-none-eabi-g++'+suffix)
    if checks['devkitARM']:result['compiler_version']=subprocess.check_output([str(compiler),'--version'],text=True).splitlines()[0]
    print(json.dumps(result,indent=2));return 0 if all(checks.values()) else 1
if __name__=='__main__':raise SystemExit(main())
