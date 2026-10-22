from pathlib import Path
import copy,hashlib,json,subprocess,sys
root=Path(__file__).resolve().parents[2];target=root/'tests/emulator/opening-field-exit.json';candidate=root/'build/full-port/item-admission-facing-retry.json'
raw=target.read_bytes();before=json.loads(raw);after=json.loads(candidate.read_bytes());test=json.loads((root/'build/emulator-regression/item-admission-bb-opening-2/receipt.json').read_bytes())
assert test['status']=='passed'and test['repetitions']==2 and test['initial_save_fixture']is None
assert test['scenario_sha256']==hashlib.sha256(candidate.read_bytes()).hexdigest()
proof=json.loads((root/'build/full-port/item-admission-bb-official/opening2-command.json').read_bytes());assert hashlib.sha256(raw).hexdigest()==proof['source_scenario_sha256']
checked=copy.deepcopy(after);step=next(s for s in checked['steps']if s['name']=='doll_face_left');assert step.pop('repeat_until')==step['wait']and step.pop('max_attempts')==8 and checked==before
out=root/'reports/item-admission/bb-emulator';out.mkdir(exist_ok=False)
(out/'input-before.json').write_bytes(raw);(out/'input-tested.json').write_bytes(candidate.read_bytes())
for number,label in [(1,'preliminary'),(2,'accepted')]:
    source=root/('build/emulator-regression/item-admission-bb-opening-'+str(number));dest=out/label;dest.mkdir()
    for path in source.rglob('*'):
        if path.is_file():
            assert 'temporary'not in path.relative_to(source).parts and path.suffix in('.json','.log','.png'),path
            to=dest/path.relative_to(source);to.parent.mkdir(parents=True,exist_ok=True);to.write_bytes(path.read_bytes())
for name in ['run-item-admission-opening.py','run-item-admission-facing-retry.py','read-admission-opening-success.py']:
    (out/name).write_bytes((root/'build/full-port'/name).read_bytes())
for name in ['opening-command.json','opening2-command.json','opening-completion.json','opening2-completion.json','opening.stdout.log','opening.stderr.log','opening2.stdout.log','opening2.stderr.log']:
    (out/name).write_bytes((root/'build/full-port/item-admission-bb-official'/name).read_bytes())
command=proof['command'];reference=Path(command[command.index('--original-reference')+1]);assert hashlib.sha256(reference.read_bytes()).hexdigest()==test['original_reference_sha256']
(out/'original-reference.json').write_bytes(reference.read_bytes());(out/'original-reference-receipt.json').write_bytes((reference.parent/'receipt.json').read_bytes())
evidence=dict(schema=1,source_game=test['source_commit'],game_sha256=test['game_sha256'],
 actual_preliminary='Run1 passed90,run2 failed61 at one-frame doll_face_left. Failure preserved; sampler cause is an inference, not proven.',
 input_only_change='Bound at most8 existing1-frame ordinary LEFT pulses with4-frame releases until the unchanged direction condition is observed.',
 observation_expectations_unchanged=True,reference_unchanged=True,projection_unchanged=True,position_tolerance_unchanged=True,
 actual_accepted_repetitions=2,checkpoints_each=90,position_tolerance=test['position_tolerance'],position_differences=test['repetition_position_differences'],original_cases=test['compared_original_cases'],
 profiles_removed_after_owned_process_stopped=True,game_program_or_data_changed=False,
 scope='Actual ordinary inputs with no injected save/flags/RNG/inventory. Existing opening and specified DSP checks only; not new catacombs gameplay, perceptual audio or hardware.')
(out/'source.json').write_text(json.dumps(evidence,indent=2)+'\n',encoding='utf-8')
target.write_bytes(candidate.read_bytes())
print('Promoted exact scenario bytes actually tested twice;90 observations and original reference unchanged',flush=True)
