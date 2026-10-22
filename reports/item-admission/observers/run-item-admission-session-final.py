from pathlib import Path
import hashlib,json,subprocess,time
root=Path(__file__).resolve().parents[2];prior=root/'build/full-port/item-admission-host-3';out=root/'build/full-port/item-admission-host-4';out.mkdir(exist_ok=False)
previous=json.loads((prior/'inputs.json').read_bytes());inputs={p:hashlib.sha256((root/p).read_bytes()).hexdigest()for p in previous}
assert [p for p in inputs if inputs[p]!=previous[p]]==['tests/native_session_tests.cpp']
assert json.loads((prior/'completion.json').read_bytes())['inputs_unchanged']is True
objects=[root/('build/full-port/item-admission-host-1/'+name+'.o')for name in ['native_session','field_menus','field_menus_data','field_story','item_details','storage_data']]+[root/'build/full-port/item-admission-host-2/session_restore.o']
obj_hash={p.relative_to(root).as_posix():hashlib.sha256(p.read_bytes()).hexdigest()for p in objects}
(out/'inputs.json').write_text(json.dumps(inputs,indent=2)+'\n');(out/'reused-objects.json').write_text(json.dumps(obj_hash,indent=2)+'\n')
script='''#!/bin/bash
set -eu
cd /mnt/d/GameDev/3DS/mother-encore-3ds
export ASAN_OPTIONS=detect_leaks=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
flags=(-std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Wno-misleading-indentation -fno-rtti -fno-exceptions -fno-pie -no-pie -fno-omit-frame-pointer -fsanitize=address,undefined -Iinclude)
out=build/full-port/item-admission-host-4
'''
script+='g++ "${flags[@]}" tests/native_session_tests.cpp '+' '.join(p.relative_to(root).as_posix()for p in objects)+' build/full-port/shadow-core-host-2/libencore-shadow.a -Wl,-Map="$out/native_session.map" -o "$out/native_session" >"$out/compile.stdout.log" 2>"$out/compile.stderr.log"\n'
script+='python3 build/full-port/check-storage-linked-inputs.py "$out"\n"$out/native_session" romfs build/full-port/catacombs-catalog-fixture-3/legacy11.encsession\n'
(out/'run.sh').write_text(script,newline='\n');command=['wsl','-d','Ubuntu-20.04','--','bash','/mnt/d/GameDev/3DS/mother-encore-3ds/build/full-port/item-admission-host-4/run.sh']
(out/'command.json').write_text(json.dumps(command)+'\n');start=time.monotonic()
with(out/'stdout.log').open('wb')as stdout,(out/'stderr.log').open('wb')as stderr:result=subprocess.run(command,cwd=root,stdout=stdout,stderr=stderr)
same=all(hashlib.sha256((root/p).read_bytes()).hexdigest()==h for p,h in {**inputs,**obj_hash}.items());record=dict(returncode=result.returncode,seconds=time.monotonic()-start,inputs_unchanged=same)
(out/'completion.json').write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(record));print((out/'stdout.log').read_text(errors='replace'))
if result.returncode:print((out/'stderr.log').read_text(errors='replace')[-2200:])
assert result.returncode==0 and same
