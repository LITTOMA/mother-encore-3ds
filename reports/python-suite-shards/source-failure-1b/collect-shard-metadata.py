import importlib.util,json,subprocess,sys
from pathlib import Path
from unittest.mock import patch
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root));directory=root/'build/config'
spec=importlib.util.spec_from_file_location('shard_metadata_runner',root/'tests/python_suite.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);real=subprocess.check_output;commands=[]
def configured(command,*args,**kwargs):
 if command[0]=='ctest'and command[-1]=='--show-only=json-v1':command=command+['-C','Debug'];commands.append(command)
 return real(command,*args,**kwargs)
with patch.object(module.subprocess,'check_output',side_effect=configured):suite,receipt=module.plan(root,directory)
module.validate_shard_registrations(receipt['aggregate_commands'],root,directory,2);parts,ids=module.shard_partition(suite,2)
meta=json.loads(real(['ctest','--test-dir',str(directory),'-C','Debug','--show-only=json-v1'],text=True));selected={t['name']:{p['name']:p['value']for p in t['properties']}for t in meta['tests']if t['name']in('python_tools','python_tools_part2')}
for name,props in selected.items():assert props['RUN_SERIAL']and props['TIMEOUT']==1800 and props['PROCESSORS']==4 and'python_unit'in props['LABELS']
assert len(selected)==2
receipt.update(shard_ids=ids,shard_counts=[len(p)for p in ids],ctest_count=len(meta['tests']),ctest_properties=selected,actual_collection_commands=commands,scope='Actual Windows Debug CTest metadata and unittest discovery only. No Windows native build/full-suite execution; targeted scheduler tests logged separately. Source/game data unchanged.')
(root/'build/shard-plan.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf8');print('Actual metadata:',len(meta['tests']),'CTest;',receipt['discovered'],'Python=',receipt['aggregate'],'aggregate+',receipt['dedicated'],'dedicated; whole-module shards',receipt['shard_counts'],'exclusive,4 workers,1800 seconds each')
