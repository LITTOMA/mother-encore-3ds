from pathlib import Path
from concurrent.futures import ThreadPoolExecutor,as_completed
import hashlib,json,subprocess,sys,time
root=Path(__file__).resolve().parents[2];out=root/'build/full-port/item-admission-python-2';out.mkdir(exist_ok=False)
names=['catacombs_item_catalog','item_consumption','item_consume_audio','catacombs_item_details','item_details','catacombs_storage','native_storage','storage_dialogue','field_menus','field_equipment','field_audio','field_prop_audio','append_audio_assets','new_game_assets','breakables','dropped_breakables','native_items','native_session']
names=[name for name in names if (root/('tests/test_'+name+'.py')).is_file()]
files=[p for directory in ['tools','tests','runtime','include','content']for p in (root/directory).rglob('*')if p.is_file()and p.suffix in ('.py','.cpp','.hpp','.json')and '__pycache__'not in p.parts]
inputs={p.relative_to(root).as_posix():hashlib.sha256(p.read_bytes()).hexdigest()for p in files}
(out/'inputs.json').write_text(json.dumps(inputs,indent=2)+'\n')
def run(name):
    command=[sys.executable,'-X','utf8','-m','unittest','tests.test_'+name,'-v'];start=time.monotonic()
    with(out/(name+'.stdout.log')).open('wb')as stdout,(out/(name+'.stderr.log')).open('wb')as stderr:
        try:code=subprocess.run(command,cwd=root,stdout=stdout,stderr=stderr,timeout=900).returncode
        except subprocess.TimeoutExpired:code='timeout900'
    record=dict(module=name,command=command,returncode=code,seconds=time.monotonic()-start)
    print(json.dumps(record),flush=True);return record
records=[]
with ThreadPoolExecutor(max_workers=3)as executor:
    futures=[executor.submit(run,name)for name in names]
    for future in as_completed(futures):
        records.append(future.result());(out/'steps.json').write_text(json.dumps(records,indent=2)+'\n')
same=all(hashlib.sha256((root/p).read_bytes()).hexdigest()==h for p,h in inputs.items())
record=dict(modules=len(records),failed=[r['module']for r in records if r['returncode']!=0],inputs_unchanged=same)
(out/'completion.json').write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(record),flush=True)
sys.exit(0 if same and not record['failed']else 1)
