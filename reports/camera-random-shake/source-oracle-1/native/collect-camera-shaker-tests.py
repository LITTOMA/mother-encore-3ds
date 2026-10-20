from pathlib import Path
import importlib.util,json,subprocess
from unittest.mock import patch
root=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('camera_shaker_collection',root/'tests/python_suite.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
real=subprocess.check_output
commands=[]
def configured(command,*args,**kwargs):
    if command[0]=='ctest' and command[-1]=='--show-only=json-v1':
        actual=command+['-C','Debug'];commands.append(actual)
        return real(actual,*args,**kwargs)
    return real(command,*args,**kwargs)
directory=root/'build/full-port/root-cmake-config-1'
with patch.object(module.subprocess,'check_output',side_effect=configured):
    _,receipt=module.plan(root,directory)
metadata=json.loads(real(['ctest','--test-dir',str(directory),'--show-only=json-v1','-C','Debug'],text=True))
assert len(metadata['tests'])==285
assert sum(t['name']=='camera_random_shake'for t in metadata['tests'])==1
assert receipt['discovered']==1299 and receipt['aggregate']==916 and receipt['dedicated']==383
receipt.update(actual_ctest_commands=commands,ctest_count=285,scope='Actual Windows Debug CTest metadata and unittest discovery only; no Windows executables built or tests executed. Explicit -C Debug is required by the existing Visual Studio multi-configuration generator.')
(root/'build/full-port/camera-random-shake-native/test-plan.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf8')
print('Actual Windows Debug metadata:285 CTest;1299 Python =916 aggregate+383 dedicated; identities exact; no Windows native execution.')
