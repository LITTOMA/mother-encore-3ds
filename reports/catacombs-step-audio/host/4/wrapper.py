import hashlib,json,subprocess
from pathlib import Path
r=Path.cwd();core=r/'build/full-port/shadow-core-host-2';assert all(hashlib.sha256((r/p).read_bytes()).hexdigest()==h for p,h in json.loads((core/'inputs.json').read_bytes()).items());assert hashlib.sha256((core/'libencore-shadow.a').read_bytes()).hexdigest()=='372b2782ef10b8a1dc922e53941e88224e4c3cf808272ff6b86a98c8e2157426'
out=r/'build/full-port/step-bank-host-4';assert not out.exists();out.mkdir();(out/'.owned-step-bank-host').write_bytes(b'step-bank-host-v1');rows=[];doc=json.loads((r/'reports/catacombs-step-audio/native-1/reference.json').read_bytes())
for row in doc['rows']:rows.append(' '.join(str(v)for v in[row['index'],['initial','play','repeat','stop'].index(row['phase']),int(row['running']),int(row['playing']),row['gain_db'],row['pitch'],row['length']]))
(out/'native.tsv').write_bytes(('\n'.join(rows)+'\n').encode());files=['tests/field_run_bank_audio_tests.cpp','platform/ctr/field_run_bank_audio.hpp','platform/ctr/field_run_bank_audio.cpp','platform/ctr/audio_player.cpp','runtime/field_step_contacts.cpp'];(out/'inputs.json').write_text(json.dumps({p:hashlib.sha256((r/p).read_bytes()).hexdigest()for p in files},indent=2)+'\n')
script='''#!/bin/bash
set -eu
cd /mnt/d/GameDev/3DS/mother-encore-3ds
export ASAN_OPTIONS=detect_leaks=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
g++ -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Wno-misleading-indentation -fno-rtti -fno-exceptions -fno-pie -no-pie -fno-omit-frame-pointer -fsanitize=address,undefined -Iinclude -Iplatform/ctr -Itests/fixtures/audio_ndsp tests/field_run_bank_audio_tests.cpp platform/ctr/audio_player.cpp runtime/field_step_contacts.cpp build/full-port/shadow-core-host-2/libencore-shadow.a -o build/full-port/step-bank-host-4/run-bank
build/full-port/step-bank-host-4/run-bank romfs/sound/banks/opening.encaudio romfs build/full-port/catacombs-steps-fixture-1/catacombs.encsteps build/full-port/catacombs-step-audio-candidate-1/sound/banks/catacombs-steps.encaudio build/full-port/catacombs-step-audio-candidate-1 build/full-port/step-bank-host-4/scratch build/full-port/step-bank-host-4/native.tsv build/full-port/catacombs-steps-fixture-1/body.tsv
''';(out/'run.sh').write_bytes(script.encode());cmd=['wsl.exe','-d','Ubuntu-20.04','--','bash','/mnt/d/GameDev/3DS/mother-encore-3ds/build/full-port/step-bank-host-4/run.sh'];(out/'command.json').write_text(json.dumps(cmd)+'\n')
with(out/'stdout.log').open('wb')as stdout,(out/'stderr.log').open('wb')as stderr:p=subprocess.run(cmd,stdout=stdout,stderr=stderr)
(out/'completion.json').write_text(json.dumps(dict(exit_code=p.returncode,normal_leak_checks=True,scope='Actual new production independent run service+AudioPlayer+87contacts,unchanged117-core archive,real8convertedPCM;explicit NDSP API double,not a fresh118 rebuild or SDK/GPU/audible/hardware pass'))+'\n');print((out/'stdout.log').read_text('utf8',errors='backslashreplace'));print('Raw compiler/WSL stderr retained;bytes',len((out/'stderr.log').read_bytes()));raise SystemExit(p.returncode)
