from pathlib import Path
import json,subprocess,sys,time
root=Path(__file__).resolve().parents[2];out=root/'build/full-port/item-admission-fixtures-2';out.mkdir(exist_ok=False)
commands=[('catalog',['tests/prepare_catacombs_item_catalog.py','build/full-port/catacombs-catalog-fixture-3']),
 ('consumption',['tests/prepare_item_consumption.py','build/full-port/item-consume-fixture-5','reports/item-consumption/original-1']),
 ('details',['tests/prepare_catacombs_item_details.py','build/full-port/catacombs-details-fixture-2']),
 ('storage',['tests/prepare_catacombs_storage.py','build/full-port/catacombs-storage-fixture-2']),
 ('source-stage',['tools/stage_native_romfs.py','--verify-only'])]
records=[]
for name,args in commands:
    command=[sys.executable,'-X','utf8']+args;start=time.monotonic()
    with(out/(name+'.stdout.log')).open('wb')as stdout,(out/(name+'.stderr.log')).open('wb')as stderr:result=subprocess.run(command,cwd=root,stdout=stdout,stderr=stderr)
    record=dict(step=name,command=command,returncode=result.returncode,seconds=time.monotonic()-start);records.append(record)
    (out/'steps.json').write_text(json.dumps(records,indent=2)+'\n');print(json.dumps(record),flush=True)
    if result.returncode:
        print((out/(name+'.stderr.log')).read_text(errors='replace')[-4500:],flush=True);sys.exit(result.returncode)
