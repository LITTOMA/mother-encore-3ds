#!/usr/bin/env python3
import copy,hashlib,json,struct,subprocess,sys,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from tools.house_button_prompt_assets import encode,IR,REPORT
WORK=ROOT/'build/house-button-prompts';WORK.mkdir(parents=True,exist_ok=True);REPORT.mkdir(parents=True,exist_ok=True)
log=[(REPORT/"focused-host.log").read_text()] if (REPORT/"focused-host.log").exists() else []
def run(args):
 r=subprocess.run(args,cwd=ROOT,text=True,capture_output=True);log.append('$ '+' '.join(map(str,args))+'\n'+r.stdout+r.stderr);(REPORT/'focused-host.log').write_text('\n'.join(log));assert r.returncode==0,log[-1];return r.stdout
source=json.loads(IR.read_text());encoded=encode(source)
magic,schema,total,checksum,capability=struct.unpack_from('<8s4I',encoded)
assert magic==b'ENCPRMPT' and schema==source['schema']==2 and capability==source['capability']==2
assert total==len(encoded) and checksum==zlib.crc32(encoded[24:])&0xffffffff
assert any(target['kind']==4 for target in source['targets']), 'Current source must cover real inspection prompts'
for key,value in [('schema',1),('capability',1),('schema',3),('capability',3),('schema',True),('capability',True)]:
 bad=copy.deepcopy(source);bad[key]=value
 try:encode(bad)
 except ValueError:pass
 else:raise AssertionError('Unadmitted producer version/capability accepted')
run(['python3','tools/house_button_prompt_assets.py','verify'])
run(['g++','-std=c++17','-O2','-Wall','-Wextra','-Wpedantic','-fno-exceptions','-fno-rtti','-Iinclude','tests/settings-house-prompts.cpp','runtime/house_inspection_data.cpp','runtime/house_button_prompts.cpp','runtime/house_data.cpp','runtime/phone_data.cpp','runtime/battle_data.cpp','runtime/file_io.cpp','-o',str(WORK/'settings-house-prompts')])
phone=next((ROOT/'romfs/data').glob('*.encphone'))
result=run([str(WORK/'settings-house-prompts'),str(ROOT/'romfs/data/opening.encprompts'),str(ROOT/'romfs/data/opening.enchouse'),str(phone),str(ROOT/'romfs/data/opening.encinspect')])
(REPORT/'result.json').write_text(json.dumps(dict(status='passed',result=result.strip(),scope='Focused host parser/geometry/visibility validation + official Godot 3.6.2 headless font/layout; no new GUI screenshot, ARM or hardware run',pack_sha256=hashlib.sha256((ROOT/'romfs/data/opening.encprompts').read_bytes()).hexdigest(),limitations=json.loads(IR.read_text())['scope']),indent=2)+'\n');print(result,end='')
