from pathlib import Path
import copy,hashlib,json,subprocess,sys,time
root=Path(__file__).resolve().parents[2];sys.path.insert(0,str(root));from tools.run_emulator_semantics import validate_scenario
source=root/'tests/emulator/opening-field-exit.json';before=json.loads(source.read_bytes());candidate=copy.deepcopy(before)
steps=[step for step in candidate['steps']if step['name']=='doll_face_left'];assert len(steps)==1
step=steps[0];assert step==dict(name='doll_face_left',buttons=['LEFT'],frames=1,wait=dict(direction_x=-1,direction_y=0))
step['repeat_until']=copy.deepcopy(step['wait']);step['max_attempts']=8
check=copy.deepcopy(candidate);changed=next(s for s in check['steps']if s['name']=='doll_face_left');del changed['repeat_until'];del changed['max_attempts'];assert check==before
validate_scenario(candidate)
scenario=root/'build/full-port/item-admission-facing-retry.json';assert not scenario.exists();scenario.write_bytes((json.dumps(candidate,indent=2)+'\n').encode())
references=[p for p in(root/'build/emulator-regression').glob('*/reference.json')if hashlib.sha256(p.read_bytes()).hexdigest()=='8f25cba4b518c68ffee1253d4832360c7916cf8f514b98618c8e995b5366dfec'];assert references
base=root/'build/full-port/item-admission-bb-official';game=base/'verified-artifact/3ds/encore-native/encore-native.3dsx';receipt=base/'verified-artifact/build-receipt.json';assert hashlib.sha256(game.read_bytes()).hexdigest()=='a51308775df4f125a0d09dfa434079a619733bf98298ce19f944cb89383a24cf'
out=root/'build/emulator-regression/item-admission-bb-opening-2';assert not out.exists()
command=[sys.executable,'-X','utf8',str(root/'build/full-port/run_current_opening.py'),'--emulator',str(root/'build/azahar-cdb/bin/Release/azahar.exe'),'--game',str(game),'--receipt',str(receipt),'--scenario',str(scenario),'--original-reference',str(references[0]),'--out',str(out),'--repetitions','2','--dsp-component','C:/Users/litto/AppData/Roaming/Azahar/sdmc/3ds/dspfirm.cdc','--require-audio']
proof=dict(command=command,source_scenario_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),candidate_scenario_sha256=hashlib.sha256(scenario.read_bytes()).hexdigest(),changed_input_only='doll_face_left adds at most8 existing1-frame normal LEFT pulses until the SAME original direction condition is observed; release4 frames between pulses.',
 observation_expectations_equal=True,reference_equal=True,projection_equal=True,position_tolerance_equal=True,checkpoint_count_equal=len(before['steps']),scope='Timing diagnosis, not an upstream/game behavior change. Existing frame.step completion does not check that a one-video-frame pulse contains a fresh guest input sample. Do not infer the cause is proven; retain first actual failed pair. No state injection or weakened trace expectations.')
(base/'opening2-command.json').write_text(json.dumps(proof,indent=2)+'\n');start=time.monotonic()
with(base/'opening2.stdout.log').open('wb')as stdout,(base/'opening2.stderr.log').open('wb')as stderr:result=subprocess.run(command,cwd=root,stdout=stdout,stderr=stderr)
record=dict(returncode=result.returncode,seconds=time.monotonic()-start,receipt=str(out/'receipt.json'));(base/'opening2-completion.json').write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(record),flush=True)
if result.returncode:print((base/'opening2.stderr.log').read_text(errors='replace')[-1800:])
assert result.returncode==0
