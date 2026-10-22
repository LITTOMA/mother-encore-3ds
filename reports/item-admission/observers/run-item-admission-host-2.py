from pathlib import Path
import hashlib,json,subprocess,time
root=Path(__file__).resolve().parents[2];prior=root/'build/full-port/item-admission-host-1';out=root/'build/full-port/item-admission-host-2';out.mkdir(exist_ok=False)
completion=json.loads((prior/'completion.json').read_bytes());assert completion==dict(returncode=1,seconds=completion['seconds'],inputs_unchanged=True)
inputs=json.loads((prior/'inputs.json').read_bytes());assert all(hashlib.sha256((root/p).read_bytes()).hexdigest()==h for p,h in inputs.items())
stale=json.loads((prior/'stale-archive-dependencies.json').read_bytes());assert stale['stale']=={'runtime/session_restore.cpp':['include/encore/native_session.hpp']}
modules=['native_session','field_menus','field_menus_data','field_story','item_details','storage_data']
tests=['item_consumption','field_menus','catacombs_item_session','native_session','item_details','catacombs_item_details','catacombs_storage','storage_runtime']
objects={name:hashlib.sha256((prior/(name+'.o')).read_bytes()).hexdigest()for name in modules}
assert all((prior/(name+'.o')).read_bytes()[:4]==b'\x7fELF'for name in modules)
(out/'inputs.json').write_text(json.dumps(inputs,indent=2)+'\n')
(out/'reuse.json').write_text(json.dumps(dict(checked_source_inputs_unchanged=True,reused_fresh_objects=objects,fresh_module='runtime/session_restore.cpp',reason='Actual linker-map dependency check rejected archived session_restore after NativeSession header change. No tests ran before that rejection. Rebuild the seventh affected module; all remaining linked archived module dependencies must still match.'),indent=2)+'\n')
script='''#!/bin/bash
set -eu
cd /mnt/d/GameDev/3DS/mother-encore-3ds
export ASAN_OPTIONS=detect_leaks=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
flags=(-std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Wno-misleading-indentation -fno-rtti -fno-exceptions -fno-pie -no-pie -fno-omit-frame-pointer -fsanitize=address,undefined -Iinclude)
out=build/full-port/item-admission-host-2
archive=build/full-port/shadow-core-host-2/libencore-shadow.a
g++ "${flags[@]}" -c runtime/session_restore.cpp -o "$out/session_restore.o" >"$out/compile-core-session_restore.stdout.log" 2>"$out/compile-core-session_restore.stderr.log"
'''
obj=' '.join('build/full-port/item-admission-host-1/'+name+'.o'for name in modules)+' "$out/session_restore.o"'
for start in range(0,len(tests),2):
    script+='compiler_pids=()\n'
    for name in tests[start:start+2]:script+='g++ "${flags[@]}" tests/'+name+'_tests.cpp '+obj+' "$archive" -Wl,-Map="$out/'+name+'.map" -o "$out/'+name+'" >"$out/compile-test-'+name+'.stdout.log" 2>"$out/compile-test-'+name+'.stderr.log" &\ncompiler_pids+=($!)\n'
    script+='compile_status=0\nfor compiler_pid in "${compiler_pids[@]}"; do wait "$compiler_pid" || compile_status=1; done\ntest "$compile_status" -eq 0\n'
tail=(prior/'run.sh').read_text().split('python3 build/full-port/check-storage-linked-inputs.py',1)[1]
script+='python3 build/full-port/check-storage-linked-inputs.py'+tail
(out/'run.sh').write_text(script,newline='\n')
command=['wsl','-d','Ubuntu-20.04','--','bash','/mnt/d/GameDev/3DS/mother-encore-3ds/build/full-port/item-admission-host-2/run.sh'];(out/'command.json').write_text(json.dumps(command)+'\n')
start=time.monotonic()
with(out/'stdout.log').open('wb')as stdout,(out/'stderr.log').open('wb')as stderr:result=subprocess.run(command,cwd=root,stdout=stdout,stderr=stderr)
same=all(hashlib.sha256((root/p).read_bytes()).hexdigest()==h for p,h in inputs.items())and all(hashlib.sha256((prior/(name+'.o')).read_bytes()).hexdigest()==h for name,h in objects.items())
record=dict(returncode=result.returncode,seconds=time.monotonic()-start,inputs_unchanged=same)
(out/'completion.json').write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(record),flush=True)
for path in sorted(out.glob('*.log')):
    if path.stat().st_size and ('compile'not in path.name or path.name.endswith('stderr.log')):print(path.name+':\n'+path.read_text(errors='replace')[-2000:],flush=True)
assert result.returncode==0 and same
