#!/usr/bin/env python3
"""Test the new region kernel with a generated one-friend baseline overlay only."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess
ROOT=Path(__file__).resolve().parents[1]
# Semantic review and normalization provenance: reports/github-publication/region-baseline-review.json.
REVIEWED_BASELINE_SHA256 = {
 'e722c99c7a475444faf71a1d5165bb1c0f9443995dd9f6426a2a399d4b6054c5',
 '2c33c6934889a8a48951571da0391e2de041474b39b1fd141faa151f81319dcb',
}
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--sanitize',action='store_true');p.add_argument('--arm',action='store_true');a=p.parse_args()
 build=ROOT/'build/region-background-integration';overlay=build/'overlay/encore';overlay.mkdir(parents=True,exist_ok=True);report=ROOT/'reports/perf-boundary-prototype';baseline=ROOT/'include/encore/background_kernel.hpp';header=ROOT/'include/encore/region_background_kernel.hpp'
 source=baseline.read_text();friend='    friend class RegionBackgroundKernel;\n';normalized=source.replace(friend,'')
 if hashlib.sha256(normalized.encode()).hexdigest() not in REVIEWED_BASELINE_SHA256:
  raise RuntimeError('Baseline changed: review before generating overlay')
 report.mkdir(parents=True,exist_ok=True)
 if friend not in source:source=source.replace('class BackgroundKernel {\n','class BackgroundKernel {\n'+friend,1)
 (overlay/'background_kernel.hpp').write_text(source)
 flags=['-std=c++17','-O2','-Wall','-Wextra','-Wpedantic','-Werror','-ffp-contract=off','-I'+str(overlay.parent),'-I'+str(ROOT),'-I'+str(ROOT/'include')]
 tag='integration-sanitize' if a.sanitize else 'integration-host';binary=build/tag
 if a.sanitize:flags+=['-g','-fsanitize=address,undefined,float-cast-overflow','-fno-omit-frame-pointer','-fno-pie','-no-pie']
 compiler=os.environ.get('CXX','c++');commands=[[compiler,*flags,str(ROOT/'tests/region_background_kernel_tests.cpp'),str(ROOT/'runtime/battle_data.cpp'),str(ROOT/'runtime/file_io.cpp'),'-o',str(binary)],[str(binary)]];env=dict(os.environ);env['ASAN_OPTIONS']='detect_leaks=0'
 if a.arm:
  tag='integration-arm-compile';sdk=Path(os.environ['DEVKITPRO']);arm=Path(os.environ['DEVKITARM']);probe=build/'arm_probe.cpp';probe.write_text('#include "encore/region_background_kernel.hpp"\nextern "C" bool region_probe(encore::RegionBackgroundKernel* k,const std::vector<encore::RegionBackgroundKernel::Layer>* l,unsigned w,unsigned h,float t,uint32_t* out,size_t count,std::string* error){return k->prepare(*l,w,h,*error)&&k->compose(t,0,out)&&k->compose_mapped(t,0,out,count);}\n')
  binary=build/'region-probe.o';commands=[[str(arm/'bin/arm-none-eabi-g++'),*flags,'-march=armv6k','-mtune=mpcore','-mfloat-abi=hard','-mtp=soft','-D__3DS__','-fno-exceptions','-fno-rtti','-isystem',str(sdk/'libctru/include'),'-c',str(probe),'-o',str(binary)]]
 with (report/(tag+'.log')).open('w') as log:
  for command in commands:log.write(' '.join(command)+'\n');log.flush();subprocess.run(command,cwd=ROOT,check=True,env=env,stdout=log,stderr=subprocess.STDOUT)
 result={'status':'passed','scope':'ARM object compile only' if a.arm else 'Host focused tests with generated one-friend baseline overlay','commands':commands,'inputs':{str(q.relative_to(ROOT)):sha(q) for q in [baseline,header,overlay/'background_kernel.hpp',ROOT/'tests/region_background_kernel_tests.cpp',Path(__file__).resolve()]},'binary':str(binary),'binary_sha256':sha(binary),'baseline_changed':False,'sanitize':a.sanitize,'leak_check':False,'log':str(report/(tag+'.log'))};(report/(tag+'.json')).write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
