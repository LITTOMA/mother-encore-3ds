from pathlib import Path
import json,sys
from unittest.mock import patch
r=Path(__file__).resolve().parents[2];sys.path.insert(0,str(r))
from tests import python_suite
directory=r/'build/full-port/root-cmake-config-1';raw=(r/'build/full-port/catacombs-switch-state-ctest-1.json').read_text(encoding='utf8');meta=json.loads(raw);original=python_suite.subprocess.check_output
def query(command,*args,**kwargs):
 if command==['ctest','--test-dir',str(directory),'--show-only=json-v1']:return raw
 return original(command,*args,**kwargs)
with patch.object(python_suite.subprocess,'check_output',side_effect=query):suite,receipt=python_suite.plan(r,directory)
old=json.loads((r/'build/full-port/catacombs-phone-merge-plan-1.json').read_bytes());assert receipt['discovery_ids']==old['discovery_ids']
assert (len(meta['tests']),receipt['discovered'],receipt['aggregate'],receipt['dedicated'])==(281,1283,900,383)
tests={v['name']:v for v in meta['tests']};props={v['name']:v['value']for v in tests['dungeon_switch_state']['properties']};assert set(props['FIXTURES_REQUIRED'])=={'dungeon_switch_metadata','dungeon_switch_original_reference'}
(r/'build/full-port/catacombs-switch-state-plan-1.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf8');print('Actual281CTest;1283Python=900aggregate383dedicated unchanged; state has source graph and frozen original-reference fixtures. Windows native executables not built.')
