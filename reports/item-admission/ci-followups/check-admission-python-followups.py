from pathlib import Path
import hashlib,json,os,subprocess,sys,time
root=Path(__file__).resolve().parents[2];out=root/'build/full-port/item-admission-python-followups-2';out.mkdir(exist_ok=False)
paths=['tests/test_phone_linker.py','tests/test_phone_linker_bindings.py','tests/python_suite.py','tests/test_python_suite.py','reports/phone-integration/link-review.json','tools/link_phone_content.py']
inputs={p:hashlib.sha256((root/p).read_bytes()).hexdigest()for p in paths}
for p in paths:
 dest=out/'source-snapshot'/p;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes((root/p).read_bytes())
previous=json.loads((root/'build/full-port/item-admission-python-followups-1/completion.json').read_bytes())
assert previous['returncode']==1 and previous['inputs_unchanged']and previous['inputs']==inputs
assert b"'gbk' codec can't decode"in(root/'build/full-port/item-admission-python-followups-1/stderr.log').read_bytes()
command=[sys.executable,'-X','utf8','-m','unittest','tests.test_phone_linker.PhoneLinkerTests.test_independent_hash_seeds_reproduce_all_linked_files','-v'];start=time.monotonic()
with(out/'stdout.log').open('wb')as stdout,(out/'stderr.log').open('wb')as stderr:result=subprocess.run(command,cwd=root,env=dict(os.environ,PYTHONUTF8='1'),stdout=stdout,stderr=stderr)
same=all(hashlib.sha256((root/p).read_bytes()).hexdigest()==h for p,h in inputs.items())
record=dict(command=command,environment_override=dict(PYTHONUTF8='1'),returncode=result.returncode,seconds=time.monotonic()-start,inputs_unchanged=same,inputs=inputs,scope='Only the failed inherited Windows GBK child-process case rerun; previous44 passed cases retain exact same inputs. Both original hash-seed --check commands execute.')
(out/'completion.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8');print(json.dumps(record));print((out/'stderr.log').read_text(errors='replace')[-1900:]);assert result.returncode==0 and same
