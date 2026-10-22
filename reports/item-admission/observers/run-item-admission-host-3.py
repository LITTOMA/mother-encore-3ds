from pathlib import Path
import hashlib,json,shutil,subprocess,time
root=Path(__file__).resolve().parents[2];first=root/'build/full-port/item-admission-host-1';prior=root/'build/full-port/item-admission-host-2';out=root/'build/full-port/item-admission-host-3';out.mkdir(exist_ok=False)
old=json.loads((prior/'inputs.json').read_bytes());inputs={p:hashlib.sha256((root/p).read_bytes()).hexdigest()for p in old}
assert {p for p in old if old[p]!=inputs[p]}=={'tests/field_menus_tests.cpp','tests/native_session_tests.cpp'}
assert json.loads((prior/'completion.json').read_bytes())['inputs_unchanged']is True
assert (prior/'linked-baseline-inputs.json').is_file()
modules=['native_session','field_menus','field_menus_data','field_story','item_details','storage_data']
objects=[first/(name+'.o')for name in modules]+[prior/'session_restore.o']
reused=['item_consumption','catacombs_item_session','item_details','catacombs_item_details','catacombs_storage','storage_runtime']
hashes={p.relative_to(root).as_posix():hashlib.sha256(p.read_bytes()).hexdigest()for p in objects+[prior/name for name in reused]}
for name in reused:shutil.copyfile(prior/(name+'.map'),out/(name+'.map'))
(out/'inputs.json').write_text(json.dumps(inputs,indent=2)+'\n')
(out/'reuse.json').write_text(json.dumps(dict(reused_exact_files=hashes,changed_inputs=sorted(p for p in old if old[p]!=inputs[p]),fresh_test_programs=['field_menus','native_session'],scope='Seven current core modules were freshly compiled in attempts1/2; only the two changed test programs need rebuilding. The other six exact executable and every source/header hash are checked. Not a complete host build.'),indent=2)+'\n')
script='''#!/bin/bash
set -eu
cd /mnt/d/GameDev/3DS/mother-encore-3ds
export ASAN_OPTIONS=detect_leaks=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
flags=(-std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Wno-misleading-indentation -fno-rtti -fno-exceptions -fno-pie -no-pie -fno-omit-frame-pointer -fsanitize=address,undefined -Iinclude)
out=build/full-port/item-admission-host-3
prior=build/full-port/item-admission-host-2
archive=build/full-port/shadow-core-host-2/libencore-shadow.a
compiler_pids=()
'''
obj=' '.join(p.relative_to(root).as_posix()for p in objects)
for name in ['field_menus','native_session']:script+='g++ "${flags[@]}" tests/'+name+'_tests.cpp '+obj+' "$archive" -Wl,-Map="$out/'+name+'.map" -o "$out/'+name+'" >"$out/compile-test-'+name+'.stdout.log" 2>"$out/compile-test-'+name+'.stderr.log" &\ncompiler_pids+=($!)\n'
script+='compile_status=0\nfor compiler_pid in "${compiler_pids[@]}"; do wait "$compiler_pid" || compile_status=1; done\ntest "$compile_status" -eq 0\npython3 build/full-port/check-storage-linked-inputs.py "$out"\ntest_status=0\n'
args=[('item_consumption','romfs build/full-port/item-consume-fixture-5'),('field_menus','romfs "$out/preferences.encprefs"'),('catacombs_item_session','romfs build/full-port/catacombs-catalog-fixture-3'),('native_session','romfs build/full-port/catacombs-catalog-fixture-3/legacy11.encsession'),('item_details','romfs'),('catacombs_item_details','build/full-port/catacombs-details-fixture-2'),('catacombs_storage','build/full-port/catacombs-storage-fixture-2'),('storage_runtime','romfs')]
for name,arguments in args:
    source='$prior'if name in reused else'$out'
    script+='if ! "'+source+'/'+name+'" '+arguments+' >"$out/'+name+'.stdout.log" 2>"$out/'+name+'.stderr.log"; then test_status=1; fi\n'
script+='if ! python3 tests/check_storage_sort_failures.py "$prior/catacombs_storage" build/full-port/catacombs-storage-fixture-2 "$out/sort-failures"; then test_status=1; fi\n'
script+='''for bad in build/full-port/item-consume-fixture-5/bad-reference-*.bin; do
 name=$(basename "$bad" .bin)
 set +e
 "$prior/item_consumption" romfs build/full-port/item-consume-fixture-5 "$bad" >"$out/$name.stdout.log" 2>"$out/$name.stderr.log"
 status=$?
 set -e
 test "$status" -eq 1 || test_status=1
 if grep -Eq '(runtime error:|ERROR: AddressSanitizer|ERROR: LeakSanitizer)' "$out/$name.stderr.log"; then test_status=1; fi
done
exit "$test_status"
'''
(out/'run.sh').write_text(script,newline='\n')
command=['wsl','-d','Ubuntu-20.04','--','bash','/mnt/d/GameDev/3DS/mother-encore-3ds/build/full-port/item-admission-host-3/run.sh'];(out/'command.json').write_text(json.dumps(command)+'\n')
start=time.monotonic()
with(out/'stdout.log').open('wb')as stdout,(out/'stderr.log').open('wb')as stderr:result=subprocess.run(command,cwd=root,stdout=stdout,stderr=stderr)
same=all(hashlib.sha256((root/p).read_bytes()).hexdigest()==h for p,h in {**inputs,**hashes}.items())
record=dict(returncode=result.returncode,seconds=time.monotonic()-start,inputs_unchanged=same)
(out/'completion.json').write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(record),flush=True)
for path in sorted(out.glob('*.log')):
    if path.stat().st_size and ('compile'not in path.name or path.name.endswith('stderr.log')):print(path.name+':\n'+path.read_text(errors='replace')[-1600:],flush=True)
assert result.returncode==0 and same
