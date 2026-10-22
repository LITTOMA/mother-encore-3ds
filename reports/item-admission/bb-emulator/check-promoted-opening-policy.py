from pathlib import Path
import hashlib,json,subprocess,sys,time
root=Path(__file__).resolve().parents[2];label=sys.argv[1];expected=int(sys.argv[2]);assert label in('before','after')and expected in(0,1)
out=root/('build/full-port/item-admission-facing-policy-'+label);out.mkdir(exist_ok=False)
paths=['tests/test_emulator_opening_controls.py','tests/test_emulator_semantics.py','tests/emulator/opening-field-exit.json','tools/run_emulator_semantics.py'];hashes={p:hashlib.sha256((root/p).read_bytes()).hexdigest()for p in paths}
for path in paths:
    dest=out/'source-snapshot'/path;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes((root/path).read_bytes())
(out/'inputs.json').write_text(json.dumps(hashes,indent=2)+'\n')
command=[sys.executable,'-X','utf8','-m','unittest','tests.test_emulator_semantics','tests.test_emulator_opening_controls','-v'];start=time.monotonic()
with(out/'stdout.log').open('wb')as stdout,(out/'stderr.log').open('wb')as stderr:result=subprocess.run(command,cwd=root,stdout=stdout,stderr=stderr)
same=all(hashlib.sha256((root/p).read_bytes()).hexdigest()==h for p,h in hashes.items());record=dict(command=command,returncode=result.returncode,expected=expected,seconds=time.monotonic()-start,inputs_unchanged=same)
(out/'completion.json').write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(record));print((out/'stderr.log').read_text(errors='replace')[-1300:]if result.returncode else'')
assert result.returncode==expected and same
