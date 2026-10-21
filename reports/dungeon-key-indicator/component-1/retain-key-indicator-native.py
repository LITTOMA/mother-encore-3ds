import hashlib,json,shutil
from pathlib import Path
r=Path(__file__).resolve().parents[2];p=r/'build/full-port';out=r/'reports/dungeon-key-indicator';assert not out.exists()
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
a=json.loads((p/'key-indicator-native-12/indicator.json').read_text());b=json.loads((p/'key-indicator-native-13/indicator.json').read_text());assert a==b and len(a['rows'])==81 and len(a['append_attempts'])==3
assert (p/'key-indicator-native-12/input.json').read_bytes()==(p/'key-indicator-native-13/input.json').read_bytes()
for n in[12,13]:
 folder=p/f'key-indicator-native-{n}';receipt=json.loads((folder/'receipt.json').read_text());assert receipt['rows']==81 and receipt['cases']==7 and receipt['expected_negative_exit']==17
 for name in['indicator','input']:assert receipt[name+'_sha256']==sha(folder/(name+'.json'))
out.mkdir(parents=True)
for n in range(1,14):
 dest=out/f'native-{n}';dest.mkdir();log=p/f'key-indicator-native-{n}.log';assert log.is_file();shutil.copyfile(log,dest/'producer.log');folder=p/f'key-indicator-native-{n}'
 if folder.is_dir():
  for file in folder.iterdir():
   if file.is_file():shutil.copyfile(file,dest/file.name)
  project=folder/'project'
  if project.is_dir():
   for name in['probe.gd','global.gd','ui.gd','data.gd','project.godot','probe.tscn','input.json']:
    file=project/name
    if file.is_file():(dest/'adapters').mkdir(exist_ok=True);shutil.copyfile(file,dest/'adapters'/name)
   originals={file.relative_to(project).as_posix():sha(file)for file in project.rglob('*')if file.is_file()};(dest/'owned-project-files.json').write_text(json.dumps(originals,indent=2)+'\n')
summary=dict(scope='Unchanged source key HUD/native SceneTreeTween state only;no font/icon/minimum-size layout,scene/save/hardware',native_repetitions=[12,13],cases=7,rows=81,identical=True,diagnostic_triggers=a['append_attempts'],expected_negative_exit=17,known_source_errors=json.loads((p/'key-indicator-native-12/receipt.json').read_text())['expected_original_diagnostics'],failed_adapters={'1':'Godot text Section has attrs,not path;preflight only','2,3':'--script SceneTree mode did not register source autoload names;fixed normal main-scene startup','4':'adapter inline semicolon return parsed outside if;fixed indented blocks,not warnings','5':'actual source append failure and null.from error discovered;no positive claim','6':'postprocessing diagnostic wording differed from actual null-instance wording','9':'two added zero-step source paths also fail append;observer positive-step-only tracking rejected;fixed zero-step lifecycle and admitted three exact mapped pairs'},historical_repetitions={'7,8':'five cases/67 rows,one known diagnostic pair','10,11':'seven cases/81 rows,three known pairs;unconsumed config durations still f32'},final_numeric_parameters='Source GDScript tween durations preserved as f64;Vec2 samples remain source f32;no earlier binary version published',files={file.relative_to(out).as_posix():sha(file)for file in sorted(out.rglob('*'))if file.is_file()})
(out/'native-verification.json').write_text(json.dumps(summary,indent=2)+'\n');print('Final native12/13 identical:7 cases/81 observations,3 individually mapped paired diagnostics;all failed adapters retained')
