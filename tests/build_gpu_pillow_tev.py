from pathlib import Path
import json,hashlib,os,subprocess,shlex,shutil
r=Path(__file__).resolve().parents[1];b=r/'build/gpu-pillow-texture';p=r/'reports/gpu-pillow-texture';sdk=Path(os.environ['DEVKITPRO']);arm=Path(os.environ['DEVKITARM']);b.mkdir(parents=True,exist_ok=True)
commands=[[str(sdk/'tools/bin/picasso'),'-o',str(b/'shader.shbin'),str(r/'tests/gpu_pillow_tev.v.pica')]]
subprocess.run(commands[0],check=True);blob=(b/'shader.shbin').read_bytes();(b/'pillow_texture_shader.hpp').write_text('#pragma once\n#include <cstdint>\nalignas(4) static const uint8_t pillow_texture_shader[]={'+','.join(str(x)for x in blob)+'};\n')
commands += [[str(arm/'bin/arm-none-eabi-g++'),'-std=gnu++17','-O2','-g','-Wall','-Wextra','-Wpedantic','-ffp-contract=off','-march=armv6k','-mtune=mpcore','-mfloat-abi=hard','-mtp=soft','-D__3DS__','-fno-exceptions','-fno-rtti','-ffunction-sections','-fdata-sections','-I'+str(r),'-I'+str(r/'include'),'-I'+str(b),'-isystem',str(sdk/'libctru/include'),'-specs=3dsx.specs','-Wl,--gc-sections',str(r/'tests/gpu_pillow_tev_arm.cpp'),str(r/'runtime/battle_data.cpp'),str(r/'runtime/file_io.cpp'),'-L'+str(sdk/'libctru/lib'),'-lcitro3d','-lctru','-lm','-o',str(b/'tev.elf')]]
rom=b/'romfs';(rom/'data').mkdir(parents=True,exist_ok=True);(rom/'pillow-preview').mkdir(exist_ok=True)
for f in ['data/pillow-entry.encbattle','pillow-preview/pillow-background.bpx']:shutil.copy2(r/'romfs'/f,rom/f)
commands.append([str(sdk/'tools/bin/smdhtool'),'--create','Pillow TEV','36 exact color pairs','Encore tests',str(r/'assets/icon.png'),str(b/'tev.smdh')])
commands.append([str(sdk/'tools/bin/3dsxtool'),str(b/'tev.elf'),str(b/'tev.3dsx'),'--smdh='+str(b/'tev.smdh'),'--romfs='+str(rom)])
with(p/'tev-build.log').open('w')as log:
 for command in commands[1:]:log.write(shlex.join(command)+'\n');log.flush();subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
sha=lambda f:hashlib.sha256(f.read_bytes()).hexdigest();result={'binary_sha256':sha(b/'tev.3dsx'),'sources':{str(f.relative_to(r)):sha(f)for f in [r/'tests/gpu_pillow_tev_arm.cpp',r/'tests/gpu_pillow_tev.v.pica',Path(__file__)]},'commands':commands};(p/'tev-build.json').write_text(json.dumps(result,indent=2)+'\n');print(result['binary_sha256'])
