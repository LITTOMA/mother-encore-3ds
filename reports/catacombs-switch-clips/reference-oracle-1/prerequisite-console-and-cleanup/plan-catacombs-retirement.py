from pathlib import Path
import hashlib,json,re
r=Path(__file__).resolve().parents[2];base=(r/'build/full-port').resolve();h=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
key=json.loads((r/'reports/catacombs-keys/metadata-oracle-1/verification.json').read_bytes());rows=[]
for item in key['executables']:
 p=(r/item['path']).resolve();assert p.is_relative_to(base)and p.is_file()and h(p)==item['sha256'];rows.append(dict(path=str(p),kind='file',bytes=p.stat().st_size,sha256=h(p),evidence='Published key parser metadata-oracle-1 executable fingerprint'))
p=base/'catacombs-switch-candidate-2/dungeon_switches_tests';assert p.is_file();rows.append(dict(path=str(p.resolve()),kind='file',bytes=p.stat().st_size,sha256=h(p),evidence='Retired1360-byte development prototype; current graph1412 and newer parser/state executables retained.'))
for directory in base.iterdir():
 if not directory.is_dir():continue
 name=directory.name;marker=None
 if re.fullmatch(r'catacombs-key-method-reference-[1-7]',name):marker=directory/'project/.owned-key-reference';expected='source-key-method-reference-v1';evidence=r/'reports/catacombs-keys/metadata-oracle-1'/('original-method-'+name.rsplit('-',1)[1])
 elif re.fullmatch(r'catacombs-switch-reference-[1-5]',name):marker=directory/'project/.owned-switch-reference';expected='source-switch-method-reference-v1';evidence=r/'reports/catacombs-switches/metadata-oracle-1'/('original-method-'+name.rsplit('-',1)[1])
 elif name=='catacombs-switch-restart-1':marker=directory/'project/.owned-switch-reference';expected='source-switch-method-reference-v1';evidence=r/'reports/catacombs-switches/method-state-1/original-restart'
 if marker is None:continue
 assert marker.is_file()and marker.read_text() == expected and evidence.is_dir()
 project=marker.parent.resolve();assert project.is_relative_to(base)and project.name=='project'
 files=[]
 for p in sorted(project.rglob('*')):
  assert not p.is_symlink()
  if p.is_file():files.append(dict(path=p.relative_to(project).as_posix(),bytes=p.stat().st_size,sha256=h(p)))
 rows.append(dict(path=str(project),kind='directory',bytes=sum(v['bytes']for v in files),marker=marker.name,marker_value=expected,files=files,evidence=evidence.relative_to(r).as_posix()))
plan=dict(scope='Only superseded owned native probes and private original-engine project copies. Published raw logs/observations/adapters/source fingerprints retained. Current native state/graph executables,61ea game, SDK/runtime caches, real Record, upstream and4original untracked dirs retained.',rows=rows,bytes=sum(v['bytes']for v in rows))
(base/'catacombs-retirement-plan-1.json').write_text(json.dumps(plan,indent=2)+'\n',encoding='utf8');print('Checked retirement plan:',len(rows),'targets,',plan['bytes'],'bytes; no deletion yet')
