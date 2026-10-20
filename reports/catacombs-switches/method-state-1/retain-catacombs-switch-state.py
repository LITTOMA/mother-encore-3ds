from pathlib import Path
import hashlib,json,shutil,sys
r=Path(__file__).resolve().parents[2];sys.path.insert(0,str(r))
from tests.prepare_dungeon_switches import build
fixture=r/'build/full-port/catacombs-switch-state-fixture-1';build(fixture)
assert(fixture/'source.encdsw').read_bytes()==(r/'build/full-port/catacombs-switch-candidate-3/source.encdsw').read_bytes()
dest=r/'reports/catacombs-switches/method-state-1';dest.mkdir(parents=True,exist_ok=True)
names=['catacombs-switch-state-reference-1.log','catacombs-switch-state-1-compile.log','catacombs-switch-state-1-result.log','catacombs-switch-state-2-compile.log','catacombs-switch-state-2-result.log','catacombs-switch-state-python-1.log','catacombs-switch-state-cmake-1.log','catacombs-switch-state-ctest-1.json','catacombs-switch-state-collection-1.log','catacombs-switch-state-plan-1.json','collect-catacombs-switch-state.py','retain-catacombs-switch-state.py']
for name in names:shutil.copyfile(r/'build/full-port'/name,dest/name)
shutil.copyfile(r/'build/full-port/catacombs-switch-state-1/reference.tsv',dest/'original-method-projection.tsv');shutil.copyfile(fixture/'source.json',dest/'candidate-source.json')
source=r/'build/full-port/catacombs-switch-restart-1';out=dest/'original-restart';out.mkdir(exist_ok=True)
for name in('import.log','engine.log','receipt.json','observations.json','restart-engine.log','restart.json','restart-receipt.json'):shutil.copyfile(source/name,out/name)
shutil.copyfile(r/'build/full-port/catacombs-switch-restart-1.log',out/'driver.log')
for p in(source/'project').glob('*.gd'):shutil.copyfile(p,out/p.name)
shutil.copyfile(source/'project/input.json',out/'input.json')
h=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
files=['CMakeLists.txt','platform/ctr/Makefile','include/encore/dungeon_switch_state.hpp','runtime/dungeon_switch_state.cpp','runtime/dungeon_switches.cpp','runtime/file_io.cpp','tests/dungeon_switch_state_tests.cpp','tests/prepare_dungeon_switch_reference.py','tools/run_dungeon_switch_restart_reference.py','content/catacombs-switches.json']
exe=r/'build/full-port/catacombs-switch-state-1/dungeon_switch_state_tests'
proof=dict(parent='fdb02c9052de9589a0b10150cf50eb5fcfa5b67e',scope='Detached source-method state component; no production scene, timer/animation clocks, camera/audio/UI consumers, save admission or full-map gameplay.',native={'checks':6123,'compiler':'GCC9.4','sanitizers':'ASan+UBSan detect_leaks=1 halt_on_error=1 non-PIE','inputs':'All direct source: dungeon_switches,dungeon_switch_state,file_io,test; not whole-core build.'},original={'frozen_rows':28,'native_replays':2,'projection_fields':71,'source':'Committed metadata-oracle-1/original-method-5/observations.json SHA checked; expectations unchanged.','additional_restart':'Official3.6.2 confirms8 callbacks from4objects after2requests restart before1completion; captured first/second states survive and remain ordered. Source scripts byte-exact, explicit services only.'},admission='Only source zero update-order choreography; format-valid nonzero tiers explicitly reject atomically. No unsupported Off wall animation is silently discarded.',targeted_python={'passed':17,'graph':14,'console_sources':3,'seconds':7.020},collection={'ctest':281,'python':1283,'aggregate':900,'dedicated':383,'scope':'Actual Windows CMake metadata only; native executables not built.'},source_sha256={p:h(r/p)for p in files},executable={'path':exe.relative_to(r).as_posix(),'bytes':exe.stat().st_size,'sha256':h(exe)},resource_sha256=h(fixture/'source.encdsw'),resource_unchanged=True,evidence_sha256={p.relative_to(dest).as_posix():h(p)for p in dest.rglob('*')if p.is_file()and p.name!='verification.json'})
(dest/'verification.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Retained',sum(p.stat().st_size for p in dest.rglob('*')if p.is_file()),'bytes, no executable/private runtime copied')
