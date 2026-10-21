import json,sys
from pathlib import Path
from unittest.mock import patch
r=Path(__file__).resolve().parents[2];sys.path.insert(0,str(r));from tests import python_suite
directory=r/'build/full-port/key-contact-cmake-config-1';original=python_suite.subprocess.check_output
def query(command,*args,**kwargs):
 if command==['ctest','--test-dir',str(directory),'--show-only=json-v1']:return original(command+['-C','Debug'],*args,**kwargs)
 return original(command,*args,**kwargs)
with patch.object(python_suite.subprocess,'check_output',side_effect=query):suite,receipt=python_suite.plan(r,directory)
metadata=json.loads((r/'build/full-port/key-contact-ctest-debug-1.json').read_text('utf8'));assert len(metadata['tests'])==310;assert receipt['discovered']==1397 and receipt['aggregate']==1014 and receipt['dedicated']==383;assert len([i for i in receipt['discovery_ids']if i.startswith('test_dungeon_key_contacts.')])==6
(r/'build/full-port/key-contact-method-plan-1.json').write_text(json.dumps(receipt,indent=2)+'\n');print('Windows configure/discovery only:310 CTest,1397 Python=1014 aggregate+383 dedicated;6 new method IDs,no Windows runtime pass')
