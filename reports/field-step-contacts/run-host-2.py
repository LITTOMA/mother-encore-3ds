import hashlib,json,subprocess
from pathlib import Path
r=Path.cwd();core=r/'build/full-port/shadow-core-host-2';inputs=json.loads((core/'inputs.json').read_bytes());assert all(hashlib.sha256((r/p).read_bytes()).hexdigest()==h for p,h in inputs.items());assert hashlib.sha256((core/'libencore-shadow.a').read_bytes()).hexdigest()=='372b2782ef10b8a1dc922e53941e88224e4c3cf808272ff6b86a98c8e2157426'
out=r/'build/full-port/step-contacts-host-2';assert not out.exists();out.mkdir();(out/'.owned-step-contacts-host').write_bytes(b'step-contacts-host-v1');files=['include/encore/field_step_contacts.hpp','runtime/field_step_contacts.cpp','tests/field_step_contacts_tests.cpp'];(out/'new-inputs.json').write_text(json.dumps({p:hashlib.sha256((r/p).read_bytes()).hexdigest()for p in files},indent=2)+'\n')
script='''#!/bin/bash
set -eu
cd /mnt/d/GameDev/3DS/mother-encore-3ds
export ASAN_OPTIONS=detect_leaks=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
g++ -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Wno-misleading-indentation -fno-rtti -fno-exceptions -fno-pie -no-pie -fno-omit-frame-pointer -fsanitize=address,undefined -Iinclude tests/field_step_contacts_tests.cpp runtime/field_step_contacts.cpp build/full-port/shadow-core-host-2/libencore-shadow.a -o build/full-port/step-contacts-host-2/contacts
build/full-port/step-contacts-host-2/contacts build/full-port/catacombs-steps-fixture-1
''';(out/'run.sh').write_bytes(script.encode());cmd=['wsl.exe','-d','Ubuntu-20.04','--','bash','/mnt/d/GameDev/3DS/mother-encore-3ds/build/full-port/step-contacts-host-2/run.sh'];(out/'command.json').write_text(json.dumps(cmd)+'\n')
with(out/'stdout.log').open('wb')as stdout,(out/'stderr.log').open('wb')as stderr:p=subprocess.run(cmd,stdout=stdout,stderr=stderr)
(out/'completion.json').write_text(json.dumps(dict(exit_code=p.returncode,normal_leak_checks=True,unchanged_117_core_inputs_match=True,compiled_new_module=True,scope='Current new contact module + exact unchanged117 core archive;not a fresh118-source rebuild or full test suite'))+'\n');print((out/'stdout.log').read_text('utf8',errors='replace'));print('Raw stderr retained;bytes',len((out/'stderr.log').read_bytes()));raise SystemExit(p.returncode)
