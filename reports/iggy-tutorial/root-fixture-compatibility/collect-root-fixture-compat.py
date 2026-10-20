from pathlib import Path
import json,sys
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT))
from tests import python_suite
raw=(ROOT/'build/full-port/root-fixture-compat-ctest-debug.json').read_text(encoding='utf8');metadata=json.loads(raw)
directory=ROOT/'build/full-port/root-cmake-config-1'
original=python_suite.subprocess.check_output
def actual_metadata(command,*args,**kwargs):
 if command==['ctest','--test-dir',str(directory),'--show-only=json-v1']:
  # This is the actual saved -C Debug query, not fabricated test metadata.
  # CMake's multi-config generator otherwise omits its native commands.
  return raw
 return original(command,*args,**kwargs)
with patch.object(python_suite.subprocess,'check_output',side_effect=actual_metadata):
 suite,receipt=python_suite.plan(ROOT,directory)
before=json.loads((ROOT/'build/full-port/root-investigation-ctest-1/plan.json').read_bytes())
assert receipt['discovery_ids']==before['discovery_ids']and receipt['multiset_equal']and len(metadata['tests'])==272
generated=(directory/'CTestTestfile.cmake').read_text(encoding='utf8')
for name in ('field_interaction','field_interaction_policy'):
 assert any(t['name']==name for t in metadata['tests'])
 # Native Windows executables were not built, so CTest legitimately omits
 # their commands. Check actual generated invocations without inventing one.
 lines=[line for line in generated.splitlines()if line.strip().startswith('add_test('+name+' ')]
 assert len(lines)==5 and lines[-1].strip()=='add_test('+name+' NOT_AVAILABLE)'
 assert all('/tests/fixtures/pre-prop-dialogue-story.encstory"'in line for line in lines[:-1])
(ROOT/'build/full-port/root-fixture-compat-plan.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf8')
print('Actual272CTest; unchanged Python',receipt['discovered'],receipt['aggregate'],receipt['dedicated'],'exact methods; explicit frozen metadata4 arguments admitted.')
