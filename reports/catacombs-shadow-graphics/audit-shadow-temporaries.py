import hashlib,json,shutil,subprocess,sys
from pathlib import Path
r=Path.cwd();root=r/'reports/catacombs-shadow-graphics';private=r/'build/full-port';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();sys.path.insert(0,str(r));from tools import catacombs_shadow_graphics as graphics
def copy(src,dest):
 if dest.exists():assert sha(src)==sha(dest),(src,dest)
 else:shutil.copy2(src,dest)
assert json.loads((private/'shadow-host-1/completion.json').read_bytes())['exit_code']==0
assert json.loads((private/'shadow-uv-host-1/completion.json').read_bytes())['exit_code']==0
current=private/'shadow-core-host-2';assert all(sha(r/p)==h for p,h in json.loads((current/'inputs.json').read_bytes()).items());assert sha(current/'libencore-shadow.a')=='372b2782ef10b8a1dc922e53941e88224e4c3cf808272ff6b86a98c8e2157426'
for name in ['shadow-host-1','shadow-uv-host-1','shadow-discovery-1']:
 dest=root/name;dest.mkdir(exist_ok=True)
 for p in(private/name).iterdir():
  if p.is_file()and p.suffix in['.log','.json','.sh']:copy(p,dest/p.name)
for name in ['shadow-core-host-1','shadow-core-host-2']:
 dest=root/name;dest.mkdir(exist_ok=True)
 for p in(private/name).iterdir():
  if p.is_file()and p.suffix in['.log','.json','.py']:copy(p,dest/p.name)
(root/'host-python.log').write_bytes((private/'shadow-python-2.log').read_bytes())
(root/'core-count-annotation.json').write_text(json.dumps(dict(actual_source_count=117,core1_actual=json.loads((private/'shadow-core-host-1/completion.json').read_bytes())['compiled_sources'],core2_actual=json.loads((current/'completion.json').read_bytes())['compiled_sources'],correction='Original private template scope says116;actual117 source list,logs and completion counts retained verbatim;not116.'),indent=2)+'\n')
source=json.loads((private/'shadow-graphics-candidate-2/source.json').read_bytes());assert source['producer_sha256']==sha(r/'tools/catacombs_shadow_graphics.py');dest=root/'actual-conversion';dest.mkdir(exist_ok=True);copy(private/'shadow-graphics-candidate-2/source.json',dest/'source.json');copy(private/'shadow-graphics-candidate-2/source-atlas.png',dest/'source-atlas.png');copy(private/'shadow-graphics-candidate-2.log',dest/'producer.log')
_,_,_,proof=graphics.build();sources=proof['source']['sources'];targets=[]
for n in[1,2,3,4,8,9,10,11,12,13,14]:
 src=private/f'shadow-clock-native-{n}';project=src/'temporary';assert project.is_dir()and not project.is_symlink();assert(project/'.owned-shadow-clock-reference').read_bytes()==b'shadow-clock-reference-v1';dest=root/'preliminary-native'/str(n) if n in[1,2,3,4,8]else None
 if dest:
  dest.mkdir(parents=True,exist_ok=True);(dest/'scope.txt').write_text('Preliminary/failed observations retained as produced;not final accepted oracle. See final native-* paired observations and failure analysis.\n')
  for p in src.iterdir():
   if p.is_file()and p.suffix in['.log','.json']:copy(p,dest/p.name)
  copy(private/f'shadow-clock-native-{n}.log',dest/'wrapper.log')
  for p in project.iterdir():
   if p.is_file()and p.suffix in['.gd','.tscn','.godot','.json']:copy(p,dest/p.name)
  # The original engine writes the game's result one directory above this project.
 records=[]
 for p in project.rglob('*'):
  assert not p.is_symlink(),p
  if p.is_file():
   rel=p.relative_to(project).as_posix();digest=sha(p)
   if rel in sources and rel!='project.godot':assert digest==sources[rel],p
   records.append(dict(relative=rel,bytes=p.stat().st_size,sha256=digest))
 targets.append(dict(kind='native-project',path=str(project),marker='.owned-shadow-clock-reference',marker_value='shadow-clock-reference-v1',files=records,bytes=sum(p['bytes']for p in records)))
# Compare every removable object to the actual tested archive member inside WSL.
checker=private/'shadow-object-audit.py';checker.write_text('''import hashlib,json,subprocess
from pathlib import Path
root=Path('/mnt/d/GameDev/3DS/mother-encore-3ds/build/full-port');rows=[]
for name in ['shadow-core-host-1','shadow-core-host-2']:
 base=root/name;archive=base/'libencore-shadow.a';sources=json.loads((base/'sources.json').read_text());expected=[p.replace('/','_')+'.o' for p in sources];members=subprocess.check_output(['ar','t',str(archive)],text=True).splitlines();assert members==expected and len(expected)==117
 for member in expected:
  path=base/member;raw=path.read_bytes();assert subprocess.check_output(['ar','p',str(archive),member])==raw
  rows.append(dict(base=name,name=member,bytes=len(raw),sha256=hashlib.sha256(raw).hexdigest()))
 if name=='shadow-core-host-1':
  raw=archive.read_bytes();rows.append(dict(base=name,name=archive.name,bytes=len(raw),sha256=hashlib.sha256(raw).hexdigest()))
print(json.dumps(rows))
''',encoding='utf8');raw=subprocess.check_output(['wsl.exe','-d','Ubuntu-20.04','--','python3','/mnt/d/GameDev/3DS/mother-encore-3ds/build/full-port/shadow-object-audit.py']);rows=json.loads(raw)
for row in rows:
 base=private/row.pop('base');assert(base/'.owned-shadow-core-host').read_bytes()==b'shadow-core-host-v1';path=base/row.pop('name');assert path.is_file()and sha(path)==row['sha256'];targets.append(dict(kind='compiler-file',path=str(path),marker='.owned-shadow-core-host',marker_value='shadow-core-host-v1',**row))
audit=dict(owned_build=str(private),targets=targets,total_bytes=sum(t['bytes']for t in targets),kept_archive=str(current/'libencore-shadow.a'),kept_archive_sha256=sha(current/'libencore-shadow.a'),actual_current_host_exit=0,object_member_equality=True,scope='Retained actual raw observations/adapters/logs/input hashes;remove only owned temporary native projects and archive-proven duplicate or obsolete host objects/archive. Current tested archive and real game candidates remain.')
(private/'shadow-cleanup-audit.json').write_text(json.dumps(audit,indent=2)+'\n');shutil.copy2(private/'shadow-cleanup-audit.json',root/'cleanup-audit.json');print('Audited',len(targets),'owned targets;',audit['total_bytes'],'bytes;all234 objects match actual archive members')
