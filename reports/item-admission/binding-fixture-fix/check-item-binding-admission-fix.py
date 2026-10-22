from pathlib import Path
import hashlib,json,subprocess,sys,time
root=Path(__file__).resolve().parents[2];out=root/'build/full-port/item-admission-binding-fix-1';out.mkdir(exist_ok=False)
paths=['tests/items_binding_fixtures.py','tests/test_items_fixture_resources.py','tests/test_items_presentation_bindings.py'];inputs={p:hashlib.sha256((root/p).read_bytes()).hexdigest()for p in paths}
(out/'inputs.json').write_text(json.dumps(inputs,indent=2)+'\n')
commands=[('unit',['-m','unittest','tests.test_items_fixture_resources','tests.test_items_presentation_bindings','-v']),('fixture-readonly',['tests/items_binding_fixtures.py','--output','build/full-port/item-admission-binding-fixtures-1'])]
records=[]
for name,args in commands:
    command=[sys.executable,'-X','utf8']+args;start=time.monotonic()
    with(out/(name+'.stdout.log')).open('wb')as stdout,(out/(name+'.stderr.log')).open('wb')as stderr:result=subprocess.run(command,cwd=root,stdout=stdout,stderr=stderr,timeout=600)
    row=dict(command=command,returncode=result.returncode,seconds=time.monotonic()-start);records.append(row);(out/'steps.json').write_text(json.dumps(records,indent=2)+'\n');print(json.dumps(row),flush=True)
    if result.returncode:
        print((out/(name+'.stderr.log')).read_text(errors='replace')[-2600:]);sys.exit(result.returncode)
same=all(hashlib.sha256((root/p).read_bytes()).hexdigest()==h for p,h in inputs.items());assert same
(out/'completion.json').write_text(json.dumps(dict(inputs_unchanged=True,steps=records),indent=2)+'\n')
