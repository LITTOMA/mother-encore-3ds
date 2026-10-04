#!/usr/bin/env python3
"""Build an isolated ARM exact mask compositor witness; no production edits."""
from pathlib import Path
import hashlib,json,os,shlex,shutil,subprocess
ROOT=Path(__file__).resolve().parents[1]
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    build=ROOT/'build/transition-mask';report=ROOT/'reports/transition-mask';build.mkdir(parents=True,exist_ok=True);report.mkdir(parents=True,exist_ok=True)
    sdk=Path(os.environ['DEVKITPRO']);arm=Path(os.environ['DEVKITARM'])
    assets=['graphics/battle/lamp/transition.bpx','data/pillow-entry.encbattle']
    # Resource entries use validated project relative paths; take Pillow's
    # already-built resource family, not the entire game or upstream tree.
    assets+=sorted(str(p.relative_to(ROOT/'romfs')) for p in (ROOT/'romfs/graphics/battle/pillow').glob('*.bpx'))
    for item in assets:
        destination=build/'romfs'/item;destination.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(ROOT/'romfs'/item,destination)
    elf=build/'transition-mask.elf';smdh=build/'transition-mask.smdh';output=build/'transition-mask.3dsx'
    commands=[[str(arm/'bin/arm-none-eabi-g++'),'-std=gnu++17','-O2','-g','-Wall','-Wextra','-Wpedantic','-Werror','-Wno-misleading-indentation','-ffp-contract=off','-march=armv6k','-mtune=mpcore','-mfloat-abi=hard','-mtp=soft','-D__3DS__','-fno-exceptions','-fno-rtti','-ffunction-sections','-fdata-sections','-I'+str(ROOT),'-I'+str(ROOT/'include'),'-I'+str(ROOT/'build/ctr'),'-isystem',str(sdk/'libctru/include'),'-specs=3dsx.specs','-Wl,--gc-sections',str(ROOT/'tests/transition_mask_readback.cpp'),str(ROOT/'runtime/battle_data.cpp'),str(ROOT/'runtime/file_io.cpp'),'-L'+str(sdk/'libctru/lib'),'-lcitro2d','-lcitro3d','-lctru','-lm','-o',str(elf)],
      [str(sdk/'tools/bin/smdhtool'),'--create','Transition mask readback','Exact source-alpha mask GPU witness','Encore Native tests',str(ROOT/'assets/icon.png'),str(smdh)],
      [str(sdk/'tools/bin/3dsxtool'),str(elf),str(output),'--smdh='+str(smdh),'--romfs='+str(build/'romfs')]]
    with (report/'arm-build.log').open('w')as log:
        for command in commands:log.write(shlex.join(command)+'\n');log.flush();subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
    paths=['include/encore/transition_mask_plan.hpp','platform/ctr/gpu_transition_mask_batch.hpp','platform/ctr/gpu_region_batch.hpp','build/ctr/gpu_region_shader.hpp','tests/transition_mask_readback.cpp','tests/transition_mask_test_common.hpp','tools/build_transition_mask_readback.py']
    result={'scope':'Real ARM cross-build only; readback execution pending','binary':str(output),'binary_sha256':digest(output),'sources':{x:digest(ROOT/x)for x in paths},'resources':{x:digest(build/'romfs'/x)for x in assets},'commands':commands,'sd_log':'sdmc:/encore-transition-mask-readback.log','expected':{'frames':51,'all_channel_pixels':4896000,'canvases':[[400,240],[320,180]],'actual_mask_frames':25,'stress_spans':96000,'physical_hardware':False}}
    (report/'arm-build.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'binary':str(output),'sha256':digest(output)}))
if __name__=='__main__':main()
