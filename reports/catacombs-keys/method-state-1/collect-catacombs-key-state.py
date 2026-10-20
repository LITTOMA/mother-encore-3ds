from pathlib import Path
import json,sys
from unittest.mock import patch
r=Path(__file__).resolve().parents[2];sys.path.insert(0,str(r))
from tests import python_suite
directory=r/'build/full-port/root-cmake-config-1'
raw=(r/'build/full-port/catacombs-key-state-ctest-1.json').read_text(encoding='utf8')
metadata=json.loads(raw);original=python_suite.subprocess.check_output
def query(command,*args,**kwargs):
 if command==['ctest','--test-dir',str(directory),'--show-only=json-v1']:return raw
 return original(command,*args,**kwargs)
with patch.object(python_suite.subprocess,'check_output',side_effect=query):suite,receipt=python_suite.plan(r,directory)
previous=json.loads((r/'build/full-port/catacombs-keys-method-plan-1.json').read_bytes())
assert receipt['discovery_ids']==previous['discovery_ids']
assert (len(metadata['tests']),receipt['discovered'],receipt['aggregate'],receipt['dedicated'])==(276,1268,902,366)
tests={t['name']:t for t in metadata['tests']}
assert {'dungeon_keys_fixture','dungeon_keys_data','dungeon_key_reference_fixture','dungeon_key_state'}<=set(tests)
properties={p['name']:p['value']for p in tests['dungeon_key_state']['properties']}
assert set(properties['FIXTURES_REQUIRED'])=={'dungeon_keys_metadata','dungeon_key_original_reference'}
generated=(directory/'CTestTestfile.cmake').read_text(encoding='utf8')
assert 'tests/prepare_dungeon_key_reference.py'in generated and 'dungeon-key-reference/reference.tsv'in generated
(r/'build/full-port/catacombs-key-state-method-plan-1.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf8')
print('Actual276CTest names;1268Python=902aggregate+366dedicated unchanged; state depends on metadata and frozen reference fixtures. Windows executables not built.')
