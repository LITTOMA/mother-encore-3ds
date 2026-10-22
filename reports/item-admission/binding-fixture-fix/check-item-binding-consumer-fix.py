from pathlib import Path
import hashlib,json,subprocess,time
root=Path(__file__).resolve().parents[2];out=root/'build/full-port/item-admission-binding-consumer-1';out.mkdir(exist_ok=False)
inputs={p.relative_to(root).as_posix():hashlib.sha256(p.read_bytes()).hexdigest()for directory in ['include','runtime']for p in(root/directory).rglob('*')if p.is_file()and p.suffix in('.hpp','.cpp')}
inputs['tests/items_binding_consumer_tests.cpp']=hashlib.sha256((root/'tests/items_binding_consumer_tests.cpp').read_bytes()).hexdigest()
(out/'inputs.json').write_text(json.dumps(inputs,indent=2)+'\n')
script='''#!/bin/bash
set -eu
cd /mnt/d/GameDev/3DS/mother-encore-3ds
export ASAN_OPTIONS=detect_leaks=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
out=build/full-port/item-admission-binding-consumer-1
g++ -std=c++17 -O1 -g -fno-rtti -fno-exceptions -fno-pie -no-pie -fno-omit-frame-pointer -fsanitize=address,undefined -Iinclude tests/items_binding_consumer_tests.cpp build/full-port/shadow-core-host-2/libencore-shadow.a -Wl,-Map="$out/consumer.map" -o "$out/consumer" >"$out/compile.stdout.log" 2>"$out/compile.stderr.log"
python3 build/full-port/check-storage-linked-inputs.py "$out"
"$out/consumer" build/full-port/item-admission-binding-fixtures-1 romfs/battle/lamp/opening.encbattle romfs/battle/lamp/opening.encround
'''
(out/'run.sh').write_text(script,newline='\n');command=['wsl','-d','Ubuntu-20.04','--','bash','/mnt/d/GameDev/3DS/mother-encore-3ds/build/full-port/item-admission-binding-consumer-1/run.sh'];(out/'command.json').write_text(json.dumps(command)+'\n');start=time.monotonic()
with(out/'stdout.log').open('wb')as stdout,(out/'stderr.log').open('wb')as stderr:result=subprocess.run(command,cwd=root,stdout=stdout,stderr=stderr)
same=all(hashlib.sha256((root/p).read_bytes()).hexdigest()==h for p,h in inputs.items());record=dict(returncode=result.returncode,seconds=time.monotonic()-start,inputs_unchanged=same)
(out/'completion.json').write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(record));print((out/'stdout.log').read_text(errors='replace'))
if result.returncode:print((out/'stderr.log').read_text(errors='replace')[-2000:])
assert result.returncode==0 and same
