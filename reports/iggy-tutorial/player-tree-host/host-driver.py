from pathlib import Path
import hashlib,json,subprocess,os
r=Path('/mnt/d/GameDev/3DS/mother-encore-3ds');cache=r/'build/podunk-routes/host';state=json.loads((cache/'source.json').read_text())['source_sha256'];sources=sorted((r/'runtime').glob('*.cpp'));headers=sorted((r/'include/encore').glob('*.hpp'))
for p in sources+headers:
 if state.get(str(p.relative_to(r)))!=hashlib.sha256(p.read_bytes()).hexdigest():raise RuntimeError('Stale actual core: '+str(p))
objects=[cache/(p.stem+'.o')for p in sources];out=r/'build/full-port/player-tree-tests';flags=['g++','-std=c++17','-Wall','-Wextra','-Werror','-O0','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-fno-exceptions','-fno-rtti','-Iinclude']
with(r/'build/full-port/player-tree-compile.log').open('wb')as log:subprocess.run(flags+['tests/field_player_tree_tests.cpp',*[str(p)for p in objects],'-o',str(out)],cwd=r,stdout=log,stderr=subprocess.STDOUT,check=True)
with(r/'build/full-port/player-tree-result.log').open('wb')as log:code=subprocess.run([str(out),'build/full-port/player-tree-fixture/swing.encentry','tests/fixtures/field-swing-v0410.encswing','tests/fixtures/field-swing-action-v0410.encaction','build/full-port/player-tree-fixture/pause.tsv'],cwd=r,stdout=log,stderr=subprocess.STDOUT,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')).returncode
print('Player tree test exit',code,flush=True)
raise SystemExit(code)
