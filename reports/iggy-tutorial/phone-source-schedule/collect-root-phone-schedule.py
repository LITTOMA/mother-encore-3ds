from pathlib import Path
import json,sys
from unittest.mock import patch
r=Path(__file__).resolve().parents[2];sys.path.insert(0,str(r))
from tests import python_suite
directory=r/'build/full-port/root-cmake-config-1';raw=(r/'build/full-port/root-phone-schedule-ctest-1.json').read_text(encoding='utf8');meta=json.loads(raw);original=python_suite.subprocess.check_output
def query(command,*args,**kwargs):
 if command==['ctest','--test-dir',str(directory),'--show-only=json-v1']:return raw
 return original(command,*args,**kwargs)
with patch.object(python_suite.subprocess,'check_output',side_effect=query):suite,receipt=python_suite.plan(r,directory)
old=json.loads((r/'build/full-port/root-fixture-compat-plan.json').read_bytes());new_id='test_python_suite.Partition.test_phone_source_registration_owns_every_method_once'
assert set(receipt['discovery_ids'])==set(old['discovery_ids'])|{new_id}
phone=receipt['dedicated_ids']['phone_linker_source'];assert len(phone)==17 and all(n.startswith('test_phone_linker.PhoneLinkerTests.')for n in phone)
assert set(receipt['aggregate_ids'])==set(old['aggregate_ids'])-set(phone)|{new_id}
assert (len(meta['tests']),receipt['discovered'],receipt['aggregate'],receipt['dedicated'])==(273,1256,873,383)
tests={v['name']:v for v in meta['tests']};props={v['name']:v['value']for v in tests['phone_linker_source']['properties']}
assert props['TIMEOUT']==1800 and props['RUN_SERIAL']is True and 'python_unit'in props['LABELS']
aggregate={v['name']:v['value']for v in tests['python_tools']['properties']};assert aggregate['TIMEOUT']==1800
(r/'build/full-port/root-phone-schedule-plan-1.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf8');print('Actual273CTest;1256Python=873aggregate+383dedicated. Exactly17 unchanged phone methods moved,1new partition regression. Phone exclusive1800s;aggregate1800s unchanged. Windows executables not built.')
