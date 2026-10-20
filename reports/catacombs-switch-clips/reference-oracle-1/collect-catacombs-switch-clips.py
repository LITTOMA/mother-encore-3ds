from pathlib import Path
import json,sys
from unittest.mock import patch
r=Path(__file__).resolve().parents[2];sys.path.insert(0,str(r))
from tests import python_suite
directory=r/'build/full-port/root-cmake-config-1';raw=(r/'build/full-port/catacombs-switch-clips-ctest-1.json').read_text(encoding='utf8');meta=json.loads(raw);original=python_suite.subprocess.check_output
def query(command,*args,**kwargs):
 if command==['ctest','--test-dir',str(directory),'--show-only=json-v1']:return raw
 return original(command,*args,**kwargs)
with patch.object(python_suite.subprocess,'check_output',side_effect=query):suite,receipt=python_suite.plan(r,directory)
old=json.loads((r/'build/full-port/catacombs-switch-state-plan-1.json').read_bytes());added=set(receipt['discovery_ids'])-set(old['discovery_ids'])
assert not set(old['discovery_ids'])-set(receipt['discovery_ids'])and len(added)==11 and all(v.startswith('test_dungeon_switch_clips.DungeonSwitchClipsTests.')for v in added)
assert (len(meta['tests']),receipt['discovered'],receipt['aggregate'],receipt['dedicated'])==(284,1294,911,383)
tests={v['name']:v for v in meta['tests']};props={v['name']:v['value']for v in tests['dungeon_switch_clips']['properties']};assert set(props['FIXTURES_REQUIRED'])=={'dungeon_switch_property_data','dungeon_switch_property_reference'}
(r/'build/full-port/catacombs-switch-clips-plan-1.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf8');print('Actual284CTest;1294Python=911aggregate383dedicated;exact previous IDs plus11clip-source methods. Windows metadata only, native executables not built.')
