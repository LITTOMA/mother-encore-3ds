from pathlib import Path
import hashlib,json,shutil,subprocess
r=Path(__file__).resolve().parents[2];out=r/'reports/iggy-tutorial/object-consumer-fix';out.mkdir(parents=True,exist_ok=True);sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
results=[]
for folder in ('object-consumers-host-1','object-consumers-host-2'):
 source=r/'build/full-port'/folder;target=out/folder;target.mkdir(exist_ok=True)
 result=json.loads((source/'results.json').read_bytes());results.extend(result)
 for p in source.iterdir():
  if p.is_file()and p.suffix in('.log','.json','.hpp'):shutil.copyfile(p,target/p.name)
assert len(results)==14 and all(v['returncode']==0 for v in results)
for n in ('object-consumers-native.py','object-consumers-native-rest.py','object-consumers-native-driver-1.log','object-consumers-native-driver-2.log','object-consumer-swing-prepare.log','object-consumer-step-prepare.log','object-consumer-cmake-1.log','object-consumer-cmake-proof.json','object-consumer-resource-unchanged.json','fix-object-test-consumers.py'):
 shutil.copyfile(r/'build/full-port'/n,out/n)
ci=out/'259-ci';ci.mkdir(exist_ok=True)
for p in(r/'build/full-port/objects259-ci').iterdir():
 if p.is_file():shutil.copyfile(p,ci/p.name)
old=out/'3b-terminal';old.mkdir(exist_ok=True)
for n in ('3b-gcc.api.log','3b-terminal-status.json'):shutil.copyfile(r/'build/full-port/older-ci-terminal'/n,old/n)
retire=[]
for folder in ('object-consumers-host-1','object-consumers-host-2'):
 for p in sorted((r/'build/full-port'/folder).glob('*-regression')):
  assert p.is_file()and p.resolve().parent==(r/'build/full-port'/folder).resolve()and p.read_bytes()[:4]==b'\x7fELF'
  retire.append(dict(path=p.relative_to(r).as_posix(),bytes=p.stat().st_size,sha256=sha(p)))
source_files=[line[3:]for line in subprocess.check_output(['git','status','--porcelain','-uall'],cwd=r).decode('utf8').splitlines()if line[:2]!='??']
proof=dict(baseline='259b782747932ddf92e60a52d5adf6166487aa29',source_sha256={n:sha(r/n)for n in source_files},native_cases=results,retired=retire,retired_bytes=sum(v['bytes']for v in retire),scope='Test/fixture/CMake compatibility only; production content and runtime remain exact259; Windows configure/collection, actual GCC9.4 ASan/UBSan14 affected native cases; no full-CI/emulator/hardware completion claim')
(out/'proof.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8')
for row in retire:(r/row['path']).unlink()
print('Retained actual14 native cases,259CI failure+console and3b terminalGCC; retired',proof['retired_bytes'],'owned native ELF bytes; production resources unchanged.',flush=True)
