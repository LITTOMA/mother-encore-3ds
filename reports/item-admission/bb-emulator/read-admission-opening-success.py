from pathlib import Path
import json,sys
root=Path(__file__).resolve().parents[2];sys.path.insert(0,str(root));from tools.run_emulator_semantics import compare_repetitions
folder=root/'build/emulator-regression/item-admission-bb-opening-2';receipt=json.loads((folder/'receipt.json').read_bytes());assert receipt['status']=='passed'and receipt['repetitions']==2 and receipt['initial_save_fixture']is None
result=[]
for index in(1,2):
    row=json.loads((folder/('run-'+str(index))/'result.json').read_bytes());assert row['status']=='passed'and len(row['checkpoints'])==90
    assert not(folder/('run-'+str(index))/'temporary').exists()
    result.append(dict(run=index,checkpoints=len(row['checkpoints']),logical_temporary_bytes_removed=row['temporary_bytes']))
baseline=json.loads((root/'build/emulator-regression/item-admission-bb-opening-1/run-1/result.json').read_bytes())
current=json.loads((folder/'run-1/result.json').read_bytes());scenario=json.loads((root/'tests/emulator/opening-field-exit.json').read_bytes())
comparison=compare_repetitions({k:v['semantic']for k,v in baseline['checkpoints'].items()},{k:v['semantic']for k,v in current['checkpoints'].items()},scenario.get('position_tolerance',0))
print(json.dumps(dict(status=receipt['status'],runs=result,source=receipt['source_commit'],game=receipt['game_sha256'],original_comparison=receipt['compared_original_cases'],position_tolerance=receipt['position_tolerance'],repetition_position_differences=receipt['repetition_position_differences'],original_single_success_comparison_positions=comparison),ensure_ascii=False),flush=True)
