from pathlib import Path
import hashlib,json,subprocess,os
r=Path('/mnt/d/GameDev/3DS/mother-encore-3ds');cache=r/'build/podunk-routes/host';state=json.loads((cache/'source.json').read_text())['source_sha256'];sources=sorted((r/'runtime').glob('*.cpp'));headers=sorted((r/'include/encore').glob('*.hpp'))
for p in sources+headers:
 if state.get(str(p.relative_to(r)))!=hashlib.sha256(p.read_bytes()).hexdigest():raise RuntimeError('Stale actual core: '+str(p))
objects=[cache/(p.stem+'.o')for p in sources];out=r/'build/full-port/player-tree-regression-tests';flags=['g++','-std=c++17','-Wall','-Wextra','-Werror','-O0','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-fno-exceptions','-fno-rtti','-Iinclude','-DENCORE_TEST_ROOM_PATH="romfs/world/house/opening.encroom"']
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
with(r/'build/full-port/player-tree-regressions.log').open('wb')as log:
 for name,args in [('field_swing_entry',['tests/fixtures/field-swing-entry-v0410.encentry','tests/fixtures/field-swing-v0410.encswing','tests/fixtures/field-swing-action-v0410.encaction','reports/iggy-tutorial/swing-entry-reference-4/reference.tsv']),('field_swing_resources',['build/full-port/swing-resource-fixture','romfs/system/native.encresources'])]:
  subprocess.run(flags+['tests/'+name+'_tests.cpp',*[str(p)for p in objects],'-o',str(out)],cwd=r,stdout=log,stderr=subprocess.STDOUT,check=True)
  subprocess.run([str(out),*args],cwd=r,stdout=log,stderr=subprocess.STDOUT,env=env,check=True)
 # New and changed execution units also pass the strict compiler flags.
 for name in ['field_player_tree','field_swing_entry','field_swing']:
  subprocess.run(flags+['-c','runtime/'+name+'.cpp','-o',str(out)+'.o'],cwd=r,stdout=log,stderr=subprocess.STDOUT,check=True)
out.resolve().relative_to((r/'build/full-port').resolve())
out.unlink();Path(str(out)+'.o').unlink();print('Actual GCC9 ASan/UBSan regressions and strict core compile passed; temporary executable/object retired')
