#!/usr/bin/env python3
"""One focused build/run; fixtures and evidence stay in the private feature lane."""
import copy, hashlib, json, re, subprocess, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.native_house import lower, encode
WORK=ROOT/'build/settings-text-speed'; REPORT=ROOT/'reports/settings-text-speed'
WORK.mkdir(parents=True,exist_ok=True);REPORT.mkdir(parents=True,exist_ok=True)
UP=ROOT/'upstream/MOTHER-Encore'
paths=['Scripts/UI/AbstractDialogueBox.gd','Scripts/global/text_tools.gd','Scripts/global/globalData.gd','Scripts/UI/Battle/BattleDialogueBox.gd']
sources={name:(UP/name).read_text() for name in paths}
speeds=json.loads(re.search(r'const TEXT_SPEEDS := (\[[^\n]+\])',sources[paths[2]])[1])
assert 'return globaldata.text_speed / _speed_multiplier_from_input / _speed_multiplier_from_tags' in sources[paths[0]]
assert 'return CHAR_DELAY.repeat(int(amount / (globaldata.text_speed * 16)))' in sources[paths[1]]
assert '_t = 0\n\t_finished = true' in sources[paths[3]]
base=json.loads((ROOT/'content/native-house.json').read_text());presentation=json.loads((ROOT/'content/native-house-presentation.json').read_text())
first=next(d['first_segment'] for d in base['dialogues'] if d['segment_count']==2)
for i in range(2):
 base['segments'][first+i]['speaker']='Carol';base['segments'][first+i]['voice']=base['segments'][0]['voice'];base['segments'][first+i]['flags']=3 if i==0 else 5
base['segments'][first]['tokens']=[dict(kind=1,text='A'),dict(kind=8,text='4'),dict(kind=1,text='B')]
base['segments'][first+1]['tokens']=[dict(kind=1,text='C')]
def pack(name,data):(WORK/name).write_bytes(encode(lower(data,presentation,verify_assets=False),version=6))
pack('default.enchouse',base)
for i,speed in enumerate(speeds):
 data=copy.deepcopy(base);data['interaction']['text_seconds']=speed;pack(f'speed-{i}.enchouse',data)
(WORK/'cases.tsv').write_text(''.join(f'{first} {s:.17g} {int(4/(s*16))}\n' for s in speeds))
texts={name:json.loads((ROOT/f'content/{name}-round.json').read_text())['texts'] for name in ['native','doll','pillow']}
for name,rows in texts.items():
 for row in rows:
  assert not re.search(r'\[|delay|wait|[^\x20-\x7e]',row['source_text']), (name,row)
  assert not re.search(r'[^\x20-\x7e]',row['text']), (name,row)
# The read-only upstream symlink identifies the matching canonical report owner.
oracle=ROOT/'reports/battle-round-presentation/native-hp-text.tsv'
if '--prepare-only' in sys.argv:
 print('Prepared checked source speed fixtures');raise SystemExit(0)
units=['house_presentation','battle_action_presentation','house_data','battle_data','battle_round_data','source_random','battle_entry','battle_round','file_io']
command=['g++','-std=c++17','-O2','-Wall','-Wextra','-Wpedantic','-fno-exceptions','-fno-rtti','-ffunction-sections','-fdata-sections','-Iinclude','tests/settings-text-speed.cpp',*[f'runtime/{u}.cpp' for u in units],'-Wl,--gc-sections','-o',str(WORK/'settings-text-speed')]
log=[(REPORT/"focused-host.log").read_text()] if (REPORT/"focused-host.log").exists() else []
def run(args):
 result=subprocess.run(args,cwd=ROOT,capture_output=True,text=True);log.append('$ '+' '.join(map(str,args))+'\n'+result.stdout+result.stderr);(REPORT/'focused-host.log').write_text('\n'.join(log));assert result.returncode==0,log[-1];return result.stdout
run(command)
result=run([str(WORK/'settings-text-speed'),str(WORK),str(ROOT/'romfs/data/opening.encround'),str(ROOT/'romfs/data/opening.encbattle'),str(oracle)])
(REPORT/'result.json').write_text(json.dumps(dict(status='passed',scope='One focused host check; no new Godot run, ARM, GUI, hardware or save operations',result=result.strip(),source_choices=speeds,sources={p:hashlib.sha256((UP/p).read_bytes()).hexdigest() for p in paths},recorded_default_oracle=dict(path=str(oracle),sha256=hashlib.sha256(oracle.read_bytes()).hexdigest()),battle_text_rows={k:len(v) for k,v in texts.items()},battle_token_scope='Current checked battle rows contain no delay, speed or wait tags. Future tagged content needs a source-preserving token schema; no guessed reconstruction from precompiled text.'),indent=2)+'\n')
print(result,end='')
