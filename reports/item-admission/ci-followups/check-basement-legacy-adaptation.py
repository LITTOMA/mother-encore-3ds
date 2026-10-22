from pathlib import Path
import hashlib,json,subprocess,sys,time
root=Path(__file__).resolve().parents[2]
out=root/'build/full-port/basement-legacy-adaptation-1';out.mkdir(exist_ok=False)
paths=['tests/test_basement_presents.py','tools/new_game_assets.py','tests/fixtures/item-admission-legacy/native-session.json']
hashes={p:hashlib.sha256((root/p).read_bytes()).hexdigest()for p in paths}
for p in paths:
 dest=out/'source-snapshot'/p;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes((root/p).read_bytes())
command=[sys.executable,'-X','utf8','-m','unittest','tests.test_basement_presents','-v'];start=time.monotonic()
with(out/'stdout.log').open('wb')as stdout,(out/'stderr.log').open('wb')as stderr:result=subprocess.run(command,cwd=root,stdout=stdout,stderr=stderr)
same=all(hashlib.sha256((root/p).read_bytes()).hexdigest()==h for p,h in hashes.items())
record=dict(command=command,returncode=result.returncode,seconds=time.monotonic()-start,inputs_unchanged=same,sha256=hashes)
(out/'completion.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8');print(json.dumps(record));print((out/'stderr.log').read_text(errors='replace')[-1300:]);assert result.returncode==0 and same
