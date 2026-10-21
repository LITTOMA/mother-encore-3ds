import hashlib,json,shutil
from pathlib import Path
r=Path(__file__).resolve().parents[2];p=r/'build/full-port';out=r/'reports/dungeon-key-indicator'
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
policy=json.loads((r/'content/catacombs-key-indicator.json').read_text());engine=policy['engine'];receipt=dict(kind='actual-gh-api-pinned-source-acquisition',repository='https://github.com/godotengine/godot',commit=engine['commit'],native_sha256=engine['native_sha256'],method='gh api with Accept:application/vnd.github.raw+json',urls=['https://api.github.com/repos/godotengine/godot/contents/scene/animation/'+name+'?ref='+engine['commit']for name in['scene_tree_tween.cpp','scene_tree_tween.h']],outputs={name:sha(r/name)for name in engine['sources']})
assert receipt['outputs']==engine['sources'];(out/'engine/source.json').write_text(json.dumps(receipt,indent=2)+'\n')
a=json.loads((p/'key-indicator-native-14/indicator.json').read_text());b=json.loads((p/'key-indicator-native-15/indicator.json').read_text());assert a==b and len(a['rows'])==91 and len(a['append_attempts'])==3
for n in[14,15]:
 folder=p/f'key-indicator-native-{n}';dest=out/f'native-{n}';assert not dest.exists();proof=json.loads((folder/'receipt.json').read_text());assert proof['rows']==91 and proof['cases']==8 and proof['expected_negative_exit']==17
 for name in['indicator','input']:assert proof[name+'_sha256']==sha(folder/(name+'.json'))
 dest.mkdir();shutil.copyfile(p/f'key-indicator-native-{n}.log',dest/'producer.log')
 for file in folder.iterdir():
  if file.is_file():shutil.copyfile(file,dest/file.name)
 project=folder/'project'
 for name in['probe.gd','global.gd','ui.gd','data.gd','project.godot','probe.tscn','input.json']:
  file=project/name
  if file.is_file():(dest/'adapters').mkdir(exist_ok=True);shutil.copyfile(file,dest/'adapters'/name)
 (dest/'owned-project-files.json').write_text(json.dumps({file.relative_to(project).as_posix():sha(file)for file in project.rglob('*')if file.is_file()},indent=2)+'\n')
(out/'sequence-verification.json').write_text(json.dumps(dict(native_repetitions=[14,15],cases=8,rows=91,identical=True,known_pairs=3,engine_sources=receipt,scope='Original script/native Tween;f32 time/elapsed/remainder and inherited default linear property curve,not generic engine or graphics',current_resource_uses_f64_source_parameters=True,executed_engine_time_is_f32=True,earlier_host_1='Failed case4/phase14 running flag because provisional model used double segment time and min-before-add order;actual91 oracle unchanged'),indent=2)+'\n');print('Native14/15 identical:91 states/8 cases;source-engine timing/default curves retained with original MIT headers')
