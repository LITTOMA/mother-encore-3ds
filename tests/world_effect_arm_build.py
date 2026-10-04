#!/usr/bin/env python3
"""Build isolated ARM+PICA numeric validation; never launches an emulator."""
import argparse,hashlib,json,os,shlex,shutil,struct,subprocess,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def replace(s,a,b):
 if s.count(a)!=1:raise ValueError('Instrumentation source changed: '+a)
 return s.replace(a,b)
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--variant',default='');args=parser.parse_args();assert not args.variant or args.variant.isalnum()
 build=ROOT/'build/world-effect-arm'/args.variant;report=ROOT/'reports/world-effect-arm'/args.variant;build.mkdir(exist_ok=True,parents=True);report.mkdir(exist_ok=True,parents=True);romfs=build/'romfs';romfs.mkdir(exist_ok=True)
 sdk=Path(os.environ['DEVKITPRO']);arm=Path(os.environ['DEVKITARM']);commands=[]
 def run(args):
  commands.append([str(x)for x in args]);log.write(shlex.join(commands[-1])+'\n');log.flush();subprocess.run(commands[-1],check=True,stdout=log,stderr=subprocess.STDOUT)
 with (build/'build.log').open('w')as log:
  sources=['tests/world_effect_tests.cpp','runtime/world_effect.cpp','runtime/world_effect_data.cpp','runtime/content.cpp','runtime/file_io.cpp']
  host=build/'host-probe';run(['g++','-std=c++17','-O2','-I'+str(ROOT/'include'),*[ROOT/p for p in sources],'-o',host])
  ref=ROOT/'reports/world-effect-reference';manifest=json.loads((ref/'reference.json').read_text());native=zlib.decompress((ref/'native-rgba.zlib').read_bytes());assert hashlib.sha256(native).hexdigest()==manifest['numeric_rgba_sha256']
  samples=manifest['samples'];assert len(samples)==20
  fixture=bytearray(32+48*len(samples));cursor=0
  for i,s in enumerate(samples):
   out=build/'host-sample.rgba';run([host,'--compose',ROOT/'romfs/data/melody.encfx',s['width'],s['height'],s['time'],*s['color'],s['alpha'],out]);cpu=out.read_bytes();amount=s['width']*s['height']*4;assert len(cpu)==amount
   cpu_offset=len(fixture);fixture.extend(cpu);gpu_offset=len(fixture);fixture.extend(native[cursor:cursor+amount]);cursor+=amount
   struct.pack_into('<2I6f4I',fixture,32+i*48,s['width'],s['height'],s['time'],*s['color'],s['alpha'],cpu_offset,amount,gpu_offset,amount)
  assert cursor==len(native);struct.pack_into('<8s6I',fixture,0,b'ENCWFQA1',1,20,48,len(fixture),0,0);(romfs/'cases.bin').write_bytes(fixture);out.unlink()
  shutil.copyfile(ROOT/'romfs/data/melody.encfx',romfs/'melody.encfx')
  production=(ROOT/'platform/ctr/world_effect_renderer.hpp').read_text();instrumented=replace(production,' WorldEffectRenderer()=default;',' double arm_kernel_ms=0,arm_upload_ms=0;\n WorldEffectRenderer()=default;')
  instrumented=replace(instrumented,'  if(!kernel_.compose(global_shader_time,sample,surface_.data(),surface_.size()))','  arm_kernel_ms=arm_upload_ms=0;const u64 arm_start=svcGetSystemTick();\n  if(!kernel_.compose(global_shader_time,sample,surface_.data(),surface_.size()))')
  instrumented=replace(instrumented,'  visible_=sample.active&&sample.alpha>0;','  arm_kernel_ms=double(svcGetSystemTick()-arm_start)/CPU_TICKS_PER_MSEC;const u64 arm_upload_start=svcGetSystemTick();\n  visible_=sample.active&&sample.alpha>0;')
  instrumented=replace(instrumented,'  C3D_TexFlush(&texture_);return true;','  C3D_TexFlush(&texture_);arm_upload_ms=double(svcGetSystemTick()-arm_upload_start)/CPU_TICKS_PER_MSEC;return true;')
  (build/'world_effect_renderer_instrumented.hpp').write_text(instrumented)
  elf=build/'world-effect-arm.elf';smdh=build/'world-effect-arm.smdh';binary=build/'world-effect-arm.3dsx'
  run([arm/'bin/arm-none-eabi-g++','-std=gnu++17','-O2','-g','-Wall','-Wextra','-Wpedantic','-Werror','-march=armv6k','-mtune=mpcore','-mfloat-abi=hard','-mtp=soft','-D__3DS__','-fno-exceptions','-fno-rtti','-ffunction-sections','-fdata-sections','-I'+str(build),'-I'+str(ROOT/'include'),'-isystem',sdk/'libctru/include','-specs=3dsx.specs','-Wl,--gc-sections',ROOT/'tests/world_effect_arm.cpp',ROOT/'runtime/world_effect.cpp',ROOT/'runtime/world_effect_data.cpp',ROOT/'runtime/file_io.cpp','-L'+str(sdk/'libctru/lib'),'-lcitro2d','-lcitro3d','-lctru','-lm','-o',elf])
  run([sdk/'tools/bin/smdhtool','--create','World Effect ARM QA','Native numeric parity probe','Encore Native tests',ROOT/'assets/icon.png',smdh])
  run([sdk/'tools/bin/3dsxtool',elf,binary,'--smdh='+str(smdh),'--romfs='+str(romfs)])
 receipt={'schema':1,'status':'built_not_executed','scope':'20 native source numeric cases; CPU straight RGBA and actual renderer normal alpha blend readback. Timing-only renderer copy; production unchanged.','commands':commands,'sources':{p:digest(ROOT/p)for p in ['tests/world_effect_arm.cpp','tests/world_effect_arm_build.py','platform/ctr/world_effect_renderer.hpp','runtime/world_effect.cpp','runtime/world_effect_data.cpp','include/encore/world_effect.hpp','include/encore/world_effect_data.hpp']},'fixtures':{'cases.bin':digest(romfs/'cases.bin'),'melody.encfx':digest(romfs/'melody.encfx')},'instrumented_renderer_sha256':digest(build/'world_effect_renderer_instrumented.hpp'),'elf_sha256':digest(elf),'binary_sha256':digest(binary),'binary':str(binary),'fixture_bytes':len(fixture),'expected_cpu_pixels':1459200,'expected_gpu_pixels':4377600,'source_samples':20,'timed_draws':60,'sd_log':'sdmc:/encore-world-effect-arm.log'}
 (build/'build.json').write_text(json.dumps(receipt,indent=2)+'\n');shutil.copyfile(build/'build.json',report/'build.json');shutil.copyfile(build/'build.log',report/'build.log');print(json.dumps({k:receipt[k]for k in ['binary','binary_sha256','fixture_bytes','sd_log']},indent=2))
if __name__=='__main__':main()
