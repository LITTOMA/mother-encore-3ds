from pathlib import Path
import hashlib,json,subprocess,time
root=Path(__file__).resolve().parents[2];out=root/'build/full-port/item-admission-host-1';out.mkdir(exist_ok=False)
base=root/'build/full-port/shadow-core-host-2';archive=base/'libencore-shadow.a'
assert hashlib.sha256(archive.read_bytes()).hexdigest()=='372b2782ef10b8a1dc922e53941e88224e4c3cf808272ff6b86a98c8e2157426'
modules=['native_session','field_menus','field_menus_data','field_story','item_details','storage_data']
tests=['item_consumption','field_menus','catacombs_item_session','native_session','item_details','catacombs_item_details','catacombs_storage','storage_runtime']
paths=set(json.loads((base/'inputs.json').read_bytes()))|{'tests/'+name+'_tests.cpp'for name in tests}
hashes={path:hashlib.sha256((root/path).read_bytes()).hexdigest()for path in sorted(paths)}
for path in hashes:
    target=out/'source-snapshot'/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes((root/path).read_bytes())
(out/'inputs.json').write_text(json.dumps(hashes,indent=2)+'\n')
(out/'reuse.json').write_text(json.dumps(dict(archive_sha256=hashlib.sha256(archive.read_bytes()).hexdigest(),fresh_modules=modules,scope='Targeted GCC9 ASAN/UBSAN rebuild, not a complete host rebuild. Every archive member actually linked must pass current source and project-header dependency hashes.'),indent=2)+'\n')
script='''#!/bin/bash
set -eu
cd /mnt/d/GameDev/3DS/mother-encore-3ds
export ASAN_OPTIONS=detect_leaks=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
flags=(-std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Wno-misleading-indentation -fno-rtti -fno-exceptions -fno-pie -no-pie -fno-omit-frame-pointer -fsanitize=address,undefined -Iinclude)
out=build/full-port/item-admission-host-1
archive=build/full-port/shadow-core-host-2/libencore-shadow.a
'''
objects=' '.join('"$out/'+name+'.o"'for name in modules)
groups=[('core',[(name,'g++ "${flags[@]}" -c runtime/'+name+'.cpp -o "$out/'+name+'.o"')for name in modules]),('test',[(name,'g++ "${flags[@]}" tests/'+name+'_tests.cpp '+objects+' "$archive" -Wl,-Map="$out/'+name+'.map" -o "$out/'+name+'"')for name in tests])]
for group,commands in groups:
    for start in range(0,len(commands),2):
        script+='compiler_pids=()\n'
        for name,command in commands[start:start+2]:script+=command+' >"$out/compile-'+group+'-'+name+'.stdout.log" 2>"$out/compile-'+group+'-'+name+'.stderr.log" &\ncompiler_pids+=($!)\n'
        script+='compile_status=0\nfor compiler_pid in "${compiler_pids[@]}"; do wait "$compiler_pid" || compile_status=1; done\ntest "$compile_status" -eq 0\n'
script+='python3 build/full-port/check-storage-linked-inputs.py "$out"\n'
args=[('item_consumption','romfs build/full-port/item-consume-fixture-5'),('field_menus','romfs "$out/preferences.encprefs"'),('catacombs_item_session','romfs build/full-port/catacombs-catalog-fixture-3'),('native_session','romfs'),('item_details','romfs'),('catacombs_item_details','build/full-port/catacombs-details-fixture-2'),('catacombs_storage','build/full-port/catacombs-storage-fixture-2'),('storage_runtime','romfs')]
for name,arguments in args:script+='"$out/'+name+'" '+arguments+' >"$out/'+name+'.stdout.log" 2>"$out/'+name+'.stderr.log"\n'
script+='python3 tests/check_storage_sort_failures.py "$out/catacombs_storage" build/full-port/catacombs-storage-fixture-2 "$out/sort-failures"\n'
script+='''for bad in build/full-port/item-consume-fixture-5/bad-reference-*.bin; do
 name=$(basename "$bad" .bin)
 set +e
 "$out/item_consumption" romfs build/full-port/item-consume-fixture-5 "$bad" >"$out/$name.stdout.log" 2>"$out/$name.stderr.log"
 status=$?
 set -e
 test "$status" -eq 1
 if grep -Eq '(runtime error:|ERROR: AddressSanitizer|ERROR: LeakSanitizer)' "$out/$name.stderr.log"; then exit 1; fi
done
'''
(out/'run.sh').write_text(script,newline='\n')
command=['wsl','-d','Ubuntu-20.04','--','bash','/mnt/d/GameDev/3DS/mother-encore-3ds/build/full-port/item-admission-host-1/run.sh']
(out/'command.json').write_text(json.dumps(command)+'\n');start=time.monotonic()
with(out/'stdout.log').open('wb')as stdout,(out/'stderr.log').open('wb')as stderr:result=subprocess.run(command,cwd=root,stdout=stdout,stderr=stderr)
same=all(hashlib.sha256((root/path).read_bytes()).hexdigest()==digest for path,digest in hashes.items())
record=dict(returncode=result.returncode,seconds=time.monotonic()-start,inputs_unchanged=same)
(out/'completion.json').write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(record),flush=True)
for path in sorted(out.glob('*.log')):
    if path.stat().st_size and ('compile'not in path.name or path.name.endswith('stderr.log')):print(path.name+':\n'+path.read_text(errors='replace')[-2000:],flush=True)
assert result.returncode==0 and same
