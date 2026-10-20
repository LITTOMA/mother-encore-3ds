from pathlib import Path
import hashlib,json,shutil,sys
r=Path(__file__).resolve().parents[2];sys.path.insert(0,str(r))
from tools import dungeon_switches as d
h=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
data,source=d.extract();blob=d.encode(data);assert blob==(r/'build/full-port/catacombs-switch-candidate-3/source.encdsw').read_bytes()
a=r/'build/full-port/catacombs-switch-reference-4/observations.json';b=r/'build/full-port/catacombs-switch-reference-5/observations.json';assert json.loads(a.read_bytes())==json.loads(b.read_bytes())
dest=r/'reports/catacombs-switches/metadata-oracle-1';dest.mkdir(parents=True,exist_ok=True)
names=['author-catacombs-switch-review.py','catacombs-switch-effective-1.txt','catacombs-switch-review-1.log','catacombs-switch-candidate-1.log','catacombs-switch-candidate-2.log','catacombs-switch-candidate-3.log','catacombs-switch-python-1.log','catacombs-switch-python-2.log','catacombs-switch-native-1-compile.log','catacombs-switch-native-1-result.log','catacombs-switch-native-2-compile.log','catacombs-switch-native-2-result.log','catacombs-switch-cmake-1.log','catacombs-switch-ctest-1.json','catacombs-switch-collection-1.log','catacombs-switch-method-plan-1.json','collect-catacombs-switch-metadata.py','retain-catacombs-switch-metadata.py']
for name in names:shutil.copyfile(r/'build/full-port'/name,dest/name)
shutil.copyfile(r/'build/full-port/catacombs-switch-candidate-3/source.source.json',dest/'candidate-source.json')
for i in range(1,6):
 folder=r/f'build/full-port/catacombs-switch-reference-{i}';out=dest/f'original-method-{i}';out.mkdir(exist_ok=True)
 shutil.copyfile(r/f'build/full-port/catacombs-switch-reference-{i}.log',out/'driver.log')
 for name in('import.log','engine.log','observations.json','receipt.json'):
  if (folder/name).is_file():shutil.copyfile(folder/name,out/name)
 for file in(folder/'project').glob('*.gd'):shutil.copyfile(file,out/file.name)
 if(folder/'project/input.json').is_file():shutil.copyfile(folder/'project/input.json',out/'input.json')
files=['CMakeLists.txt','content/catacombs-switches.json','tools/dungeon_switches.py','tools/run_dungeon_switches_reference.py','include/encore/dungeon_switches.hpp','runtime/dungeon_switches.cpp','runtime/file_io.cpp','tests/test_dungeon_switches.py','tests/dungeon_switches_tests.cpp','tests/prepare_dungeon_switches.py']
proof=dict(parent='372c17614dc73608111983f736f0474ed221ebf5',scope='Source graph metadata/parser and original-method reference, not native execution, source timeline/physics/audio/camera/map/save admission.',whole_scripts_approved=False,resource_bytes=len(blob),resource_sha256=hashlib.sha256(blob).hexdigest(),python={'passed':14,'seconds':6.806},native={'checks':209,'compiler':'GCC9.4','sanitizers':'ASan+UBSan detect_leaks=1 halt_on_error=1 non-PIE','inputs':'All direct source compile: dungeon_switches/file_io/test; not whole core.'},original={'rows_per_run':28,'runs':[4,5],'equal':True,'engine':'Official Godot3.6.2 3cd3caab6779a7f3ec3bbeb9f200db50c735cfc8','scope':'Exact source scripts and extracted methods; explicit service adapters, no timeline/physics/audible/map pass.'},failures={'candidate1':'Offline NodePath was incorrectly based at parent, fixed to source self-relative node path. No source or expected observations changed.','original1':'Unused signal warnings in service stubs rejected. Added real service completion/emission methods; original scripts unchanged.','prototype':'1360-byte prototype policy omitted denial sound/channel, now explicit source-derived strings and policy48. Prototype was never published/admitted.'},collection={'ctest':278,'python':1282,'aggregate':916,'dedicated':366,'scope':'Actual Windows CMake metadata only; native executables not built.'},sources_sha256={p:h(r/p)for p in files},evidence_sha256={p.relative_to(dest).as_posix():h(p)for p in dest.rglob('*')if p.is_file()and p.name!='verification.json'})
exe=r/'build/full-port/catacombs-switch-candidate-3/dungeon_switches_tests';proof['executable']={'path':exe.relative_to(r).as_posix(),'bytes':exe.stat().st_size,'sha256':h(exe)}
(dest/'verification.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Retained',len(list(dest.rglob('*'))),'entries',sum(p.stat().st_size for p in dest.rglob('*')if p.is_file()),'bytes;28 original states repeat identically')
