from pathlib import Path
import hashlib,json,subprocess,sys,time
from unittest.mock import patch
root=Path(__file__).resolve().parents[2];sys.path.insert(0,str(root))
from tests import python_suite as s
out=root/'build/full-port/item-admission-partition-after-3';out.mkdir(exist_ok=False)
config=root/'build/full-port/item-admission-configure-3';original=subprocess.check_output;queries=[]
def query(command,**kwargs):
 if command[0]!='ctest':return original(command,**kwargs)
 assert command==['ctest','--test-dir',str(config),'--show-only=json-v1']
 actual=[*command,'-C','Release'];queries.append(actual)
 raw=original(actual,**kwargs);(out/'actual-ctest-metadata.json').write_text(raw,encoding='utf-8',newline='\n');return raw
start=time.monotonic()
with patch.object(s.subprocess,'check_output',side_effect=query):suite,receipt=s.plan(root,config)
s.validate_shard_registrations(receipt['aggregate_commands'],root,config,4)
shards,ids=s.shard_partition(suite,4)
receipt.update(shard_count=4,shard_ids=ids,actual_ctest_queries=queries,
 scope='Actual Windows Release CTest metadata supplied to unchanged strict planner. First no-configuration query failure retained. Collection and coverage accounting only, not full test execution.',
 seconds=time.monotonic()-start,inputs={p:hashlib.sha256((root/p).read_bytes()).hexdigest()for p in ['tests/python_suite.py','CMakeLists.txt']})
(out/'partition.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
print(json.dumps({k:receipt[k]for k in ['discovered','aggregate','dedicated','multiset_equal','seconds']}));print([len(v)for v in ids])
