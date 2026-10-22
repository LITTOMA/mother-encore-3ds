from pathlib import Path
import hashlib,json,re,subprocess,sys,time
root=Path(__file__).resolve().parents[2];name=sys.argv[1];expected=int(sys.argv[2]);assert re.fullmatch('[a-z0-9-]+',name)and expected in(0,1)
out=root/'build/full-port'/name;out.mkdir(exist_ok=False)
source=root/'tests/items_data_tests.cpp';raw=source.read_bytes();(out/'items_data_tests.cpp').write_bytes(raw)
script='''#!/bin/bash
set -eu
cd /mnt/d/GameDev/3DS/mother-encore-3ds
export ASAN_OPTIONS=detect_leaks=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
out=build/full-port/'''+name+'''
g++ -std=c++17 -O1 -g -fno-rtti -fno-exceptions -fno-pie -no-pie -fno-omit-frame-pointer -fsanitize=address,undefined -Iinclude "$out/items_data_tests.cpp" build/full-port/shadow-core-host-2/libencore-shadow.a -Wl,-Map="$out/items.map" -o "$out/items" >"$out/compile.stdout.log" 2>"$out/compile.stderr.log"
python3 build/full-port/check-storage-linked-inputs.py "$out"
"$out/items" romfs/ui/opening.encitems
'''
(out/'run.sh').write_text(script,newline='\n');command=['wsl','-d','Ubuntu-20.04','--','bash','/mnt/d/GameDev/3DS/mother-encore-3ds/build/full-port/'+name+'/run.sh'];(out/'command.json').write_text(json.dumps(command)+'\n');start=time.monotonic()
with(out/'stdout.log').open('wb')as stdout,(out/'stderr.log').open('wb')as stderr:result=subprocess.run(command,cwd=root,stdout=stdout,stderr=stderr)
same=source.read_bytes()==raw;record=dict(returncode=result.returncode,expected_returncode=expected,seconds=time.monotonic()-start,inputs_unchanged=same,source_sha256=hashlib.sha256(raw).hexdigest())
(out/'completion.json').write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(record));print((out/'stdout.log').read_text(errors='replace'));print((out/'stderr.log').read_text(errors='replace')[-1400:]if result.returncode else'')
assert result.returncode==expected and same
if expected:assert b'view.count(ItemSection::Definitions)==4'in(out/'stderr.log').read_bytes()
