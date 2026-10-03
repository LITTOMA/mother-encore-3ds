from pathlib import Path
import os,subprocess,json,hashlib,difflib
import build_continue_text_qa as qa
r=Path(__file__).resolve().parents[1];b=r/'build/normal-entry-qa';b.mkdir(parents=True,exist_ok=True)
(r/'reports').mkdir(exist_ok=True)
asset_root=r if (r/'build/ctr/native-romfs').is_dir() else r.parent.parent/'encore-native'
original=(r/'platform/ctr/main.cpp').read_text();source=qa.instrument_source(original,True).replace('sdmc:/encore-frame-profile-qa.log','sdmc:/encore-normal-entry-qa.log')
(b/'main.cpp').write_text(source)
(r/'reports/normal-entry-qa.diff').write_text(''.join(difflib.unified_diff(original.splitlines(True),source.splitlines(True),fromfile='production/main.cpp',tofile='diagnostic/main.cpp')))
sdk=Path(os.environ['DEVKITPRO']);arm=Path(os.environ['DEVKITARM']);compile=qa.compile_command(r,b,sdk,arm,True);compile[1:1]=['-DENCORE_EXPERIMENTAL_GPU_BACKGROUND','-DENCORE_GPU_CERTIFICATE_TABLES','-I'+str(r/'build/ctr')]
objects=sorted((r/'build/ctr/runtime').glob('*.o'))+[r/'build/ctr/platform/ctr/audio_player.o',r/'build/ctr/platform/ctr/music_region_player.o',r/'build/ctr/platform/ctr/music_region_service.o',b/'main.o']
commands=[compile,[str(arm/'bin/arm-none-eabi-g++'),'-march=armv6k','-mtune=mpcore','-mfloat-abi=hard','-mtp=soft','-specs=3dsx.specs','-Wl,--gc-sections',*[str(x)for x in objects],'-L'+str(sdk/'libctru/lib'),'-lcitro2d','-lcitro3d','-lctru','-lm','-o',str(b/'qa.elf')],[str(sdk/'tools/bin/3dsxtool'),str(b/'qa.elf'),str(b/'qa.3dsx'),'--smdh='+str(asset_root/'dist/encore-native.smdh'),'--romfs='+str(asset_root/'build/ctr/native-romfs')]]
with (r/'reports/normal-entry-qa-build.log').open('w')as log:
 for command in commands:log.write(' '.join(command)+'\n');log.flush();subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
(r/'reports/normal-entry-qa-build.json').write_text(json.dumps({'scope':'Normal title, no scripted inputs, no injected state. Same production source and core with text/frame diagnostics only. Isolated generated save fixture is external to binary.','binary':str(b/'qa.3dsx'),'sha256':sha(b/'qa.3dsx'),'production_main_sha256':sha(r/'platform/ctr/main.cpp')},indent=2)+'\n');print(b/'qa.3dsx')
