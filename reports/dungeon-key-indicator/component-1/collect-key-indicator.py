import json,sys
from pathlib import Path
from unittest.mock import patch
r=Path(__file__).resolve().parents[2];sys.path.insert(0,str(r));from tests import python_suite
directory=r/'build/full-port/key-indicator-cmake-config-1';original=python_suite.subprocess.check_output
def query(command,*args,**kwargs):
 if command==['ctest','--test-dir',str(directory),'--show-only=json-v1']:return original(command+['-C','Debug'],*args,**kwargs)
 return original(command,*args,**kwargs)
with patch.object(python_suite.subprocess,'check_output',side_effect=query):suite,receipt=python_suite.plan(r,directory)
metadata=json.loads((r/'build/full-port/key-indicator-ctest-debug-1.json').read_text('utf8'));assert len(metadata['tests'])==314;assert receipt['discovered']==1405 and receipt['aggregate']==1022 and receipt['dedicated']==383;assert len([name for name in receipt['discovery_ids']if name.startswith('test_dungeon_key_indicator.')])==8
(r/'build/full-port/key-indicator-method-plan-1.json').write_text(json.dumps(receipt,indent=2)+'\n');print('Windows configuration/discovery ONLY:314 CTest;1405 Python=1022 aggregate+383 dedicated;8 new methods,no runtime claim')
