import hashlib,json,subprocess,sys,time
from pathlib import Path
r=Path(__file__).resolve().parents[2];n=sys.argv[1];script=r/('build/full-port/key-indicator-host-'+n+'.sh');prefix=r/('build/full-port/key-indicator-host-'+n)
paths=[*sorted((r/'include').rglob('*.hpp')),*[r/p for p in['runtime/dungeon_key_indicator.cpp','runtime/dungeon_keys.cpp','runtime/content.cpp','runtime/file_io.cpp','tests/dungeon_key_indicator_tests.cpp']],script,r/'build/full-port/key-indicator-fixture-1/source.enckui',r/'build/full-port/key-indicator-fixture-1/reference.tsv']
def hashes():return{p.relative_to(r).as_posix():hashlib.sha256(p.read_bytes()).hexdigest()for p in paths}
before=hashes();start=time.time()
with prefix.with_suffix('.log').open('wb')as out,prefix.with_suffix('.stderr.log').open('wb')as err:result=subprocess.run(['wsl','-d','Ubuntu-20.04','--','bash','/mnt/d/GameDev/3DS/mother-encore-3ds/'+script.relative_to(r).as_posix()],cwd=r,stdout=out,stderr=err,timeout=240)
after=hashes();proof=dict(command=result.args,exit=result.returncode,seconds=time.time()-start,inputs=before,unchanged=before==after,stdout_sha256=hashlib.sha256(prefix.with_suffix('.log').read_bytes()).hexdigest(),stderr_sha256=hashlib.sha256(prefix.with_suffix('.stderr.log').read_bytes()).hexdigest());prefix.with_suffix('.proof.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps({k:v for k,v in proof.items()if k!='inputs'}));sys.exit(result.returncode if before==after else 23)
