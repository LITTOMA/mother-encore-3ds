#!/usr/bin/env python3
"""Build isolated GPU span test. No CMake, production, or emulator mutation."""
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
ROOT=Path(__file__).resolve().parents[1]
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    sdk=Path(os.environ['DEVKITPRO']);arm=Path(os.environ['DEVKITARM'])
    frozen={'tests/gpu_span_cpu_reference.hpp':'e722c99c7a475444faf71a1d5165bb1c0f9443995dd9f6426a2a399d4b6054c5','tests/baby_background_scalar_reference.hpp':'883d150df13a8bfefda5009a078fdc07051e2ec195ab8b86a01424e03470bbef'}
    for p,digest in frozen.items():
        if sha(ROOT/p)!=digest: raise SystemExit('Frozen input changed: '+p)
    build=ROOT/'build/gpu-span-experiment';report=ROOT/'reports/gpu-span-experiment';build.mkdir(parents=True,exist_ok=True);report.mkdir(parents=True,exist_ok=True)
    assets=['data/doll-entry.encbattle','doll-preview/doll-background.bpx','doll-preview/doll-palette.bpx']
    for p in assets:
        dest=build/'romfs'/p;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(ROOT/'romfs'/p,dest)
    elf=build/'gpu-span-benchmark.elf';smdh=build/'gpu-span-benchmark.smdh';pending=build/'gpu-span-benchmark.pending.3dsx';output=build/'gpu-span-benchmark.3dsx'
    commands=[[str(arm/'bin/arm-none-eabi-g++'),'-std=gnu++17','-O2','-g','-Wall','-Wextra','-Wpedantic','-Werror','-ffp-contract=off','-march=armv6k','-mtune=mpcore','-mfloat-abi=hard','-mtp=soft','-D__3DS__','-fno-exceptions','-fno-rtti','-ffunction-sections','-fdata-sections','-I'+str(ROOT),'-I'+str(ROOT/'include'),'-isystem',str(sdk/'libctru/include'),'-specs=3dsx.specs','-Wl,--gc-sections','-Wl,--wrap=C3D_DrawElements',str(ROOT/'tests/gpu_span_arm_benchmark.cpp'),str(ROOT/'runtime/battle_data.cpp'),str(ROOT/'runtime/file_io.cpp'),'-L'+str(sdk/'libctru/lib'),'-lcitro2d','-lcitro3d','-lctru','-lm','-o',str(elf)],
      [str(sdk/'tools/bin/smdhtool'),'--create','GPU Span Benchmark','Exact RGBA readback test','Encore Native tests',str(ROOT/'assets/icon.png'),str(smdh)],
      [str(sdk/'tools/bin/3dsxtool'),str(elf),str(pending),'--smdh='+str(smdh),'--romfs='+str(build/'romfs')]]
    with (report/'build.log').open('w') as log:
        for command in commands:
            log.write(shlex.join(command)+'\n');log.flush();subprocess.run(command,check=True,stdout=log,stderr=subprocess.STDOUT)
    pending.replace(output)
    result={'scope':'Isolated real ARM compile/package; GPU execution and performance pending','binary':str(output),'binary_sha256':sha(output),'elf_sha256':sha(elf),'commands':commands,'source_sha256':{str(p.relative_to(ROOT)):sha(p) for p in sorted((ROOT/'tests').glob('gpu_span_*'))},'oracle_sha256':frozen,'romfs_sha256':{p:sha(build/'romfs'/p) for p in assets},'expected':{'game_frames':24,'game_pixels':1843200,'calibration_frames':2,'times':[0,1.25,60,1000],'sizes':[[400,240],[320,180]],'gpu_draws_per_frame':1},'sd_log':'sdmc:/encore-gpu-span-benchmark.log'}
    (report/'build.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'binary':str(output),'sha256':sha(output),'manifest':str(report/'build.json')}))
if __name__=='__main__': main()
