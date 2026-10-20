from pathlib import Path
import json,sys
from unittest.mock import patch
r=Path(__file__).resolve().parents[2];sys.path.insert(0,str(r))
from tests import python_suite
directory=r/'build/full-port/root-cmake-config-1';raw=(r/'build/full-port/catacombs-keys-ctest-debug-2.json').read_text(encoding='utf8');metadata=json.loads(raw)
original=python_suite.subprocess.check_output
def query(command,*args,**kwargs):
 if command==['ctest','--test-dir',str(directory),'--show-only=json-v1']:return raw
 return original(command,*args,**kwargs)
with patch.object(python_suite.subprocess,'check_output',side_effect=query):suite,receipt=python_suite.plan(r,directory)
previous=json.loads((r/'build/full-port/root-fixture-compat-plan.json').read_bytes());added=set(receipt['discovery_ids'])-set(previous['discovery_ids'])
assert not set(previous['discovery_ids'])-set(receipt['discovery_ids'])and len(added)==13 and all(name.startswith('test_dungeon_keys.DungeonKeysTests.')for name in added)
assert (len(metadata['tests']),receipt['discovered'],receipt['aggregate'],receipt['dedicated'])==(274,1268,902,366)
generated=(directory/'CTestTestfile.cmake').read_text(encoding='utf8');assert 'tests/prepare_dungeon_keys.py'in generated and 'alignment-fixture.encdkeys'in generated
(r/'build/full-port/catacombs-keys-method-plan-1.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf8')
print('Actual274CTest names;1268Python=902aggregate+366dedicated;exact old methods plus13new parser/source methods. Windows native executables not built.')
