from pathlib import Path
import json,subprocess,time
root=Path(__file__).resolve().parents[2];out=root/'build/full-port/item-admission-configure-3';out.mkdir(exist_ok=False)
records=[]
for name,command in [('configure',['cmake','-S','.', '-B',str(out),'-DBUILD_TESTING=ON','-DENCORE_REGENERATE_NATIVE_CONTENT=OFF']),('discovery',['ctest','--test-dir',str(out),'-C','Release','--show-only=json-v1'])]:
    start=time.monotonic();result=subprocess.run(command,cwd=root,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    (out/(name+'.stdout.log')).write_bytes(result.stdout);(out/(name+'.stderr.log')).write_bytes(result.stderr)
    records.append(dict(command=command,returncode=result.returncode,seconds=time.monotonic()-start))
    (out/'commands.json').write_text(json.dumps(records,indent=2)+'\n');assert result.returncode==0
doc=json.loads(result.stdout);names={test['name']for test in doc['tests']}
expected={'prepare_catacombs_item_catalog','catacombs_item_session','prepare_item_consumption','item_consumption','item_consumption_bad_reference','prepare_catacombs_storage','catacombs_storage','catacombs_storage_bad_reference','prepare_catacombs_item_details','catacombs_item_details'}
assert expected<=names,sorted(expected-names)
record=dict(registered_tests=len(names),new_required_tests=sorted(expected),scope='Actual MSVC configuration and discovery only; no MSVC compilation or CTest execution.')
(out/'discovery-receipt.json').write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(record),flush=True)
