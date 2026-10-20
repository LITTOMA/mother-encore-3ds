from pathlib import Path
import hashlib,json,shutil,subprocess,sys
r=Path(__file__).resolve().parents[2];sys.path.insert(0,str(r))
from tests.prepare_dungeon_keys import build
out=r/'build/full-port/catacombs-key-state-fixture-1';build(out)
hashof=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert (out/'catacombs.encdkeys').read_bytes()==(r/'build/full-port/catacombs-keys-candidate-7/catacombs.encdkeys').read_bytes()
assert (r/'build/full-port/catacombs-key-state-1/reference.tsv').read_bytes()==(r/'build/full-port/catacombs-key-state-1/reference-ctest.tsv').read_bytes()
dest=r/'reports/catacombs-keys/method-state-1';dest.mkdir(parents=True,exist_ok=True)
files=['catacombs-key-state-1-compile.log','catacombs-key-state-2-compile.log','catacombs-key-state-2-result.log','catacombs-key-state-3-compile.log','catacombs-key-state-4-compile.log','catacombs-key-state-4-result.log','catacombs-key-state-clang-1-compile.log','catacombs-key-state-python-1.log','catacombs-key-state-cmake-1.log','catacombs-key-state-ctest-1.json','catacombs-key-state-method-plan-1.json','catacombs-key-state-collection-1.log','catacombs-key-state-reference-1.log','catacombs-key-state-reference-2.log','collect-catacombs-key-state.py','retain-catacombs-key-state.py']
for name in files:shutil.copyfile(r/'build/full-port'/name,dest/name)
shutil.copyfile(r/'build/full-port/catacombs-key-state-1/reference.tsv',dest/'original-method-projection.tsv')
shutil.copyfile(out/'fixture-source.json',dest/'fixture-source.json')
source=['include/encore/dungeon_key_state.hpp','runtime/dungeon_key_state.cpp','runtime/file_io.cpp','runtime/dungeon_keys.cpp','tests/dungeon_key_state_tests.cpp','tests/prepare_dungeon_key_reference.py','tools/dungeon_keys.py','tests/test_dungeon_keys.py','content/catacombs-keys.json','CMakeLists.txt']
proof={'parent':'b9befdca75bf84f3d2a2f6b6f034d1408fe8f5b1','scope':'Detached source-method state component; not playable Catacombs, production scene/audio/UI/timeline/save admission or complete game.',
 'native_sanitizer_checks':1713,'native':'GCC9.4 actual direct compile of dungeon_keys/dungeon_key_state/file_io and test; ASan+UBSan detect_leaks=1, halt_on_error=1, non-PIE. Not a whole-core build.',
 'failures_retained':{'native1':'C++ heterogeneous auto declaration compile failure; split declarations fixed.','native3':'Driver omitted non-PIE flags for prior non-PIE cached file_io.o; linker rejected relocation. Native4 compiled all inputs directly from source with non-PIE flags.','clang1':'Local WSL clang++ unavailable (127); no local Clang pass claimed. Full CI provides independent Clang.'},
 'original_reference':'Committed metadata-oracle-1/original-method-7/observations.json frozen at original verification SHA.16 valid method projections twice;2 source missing-method signal dispatch negatives preserved separately, not implemented by state kernel.',
 'python':{'passed':13,'seconds':5.497},'collection':{'ctest':276,'python':1268,'aggregate':902,'dedicated':366,'scope':'Actual Windows MSVC CMake metadata and method collection only; executables not built.'},
 'game_data_sha256':hashof(out/'catacombs.encdkeys'),'game_data_unchanged':True,'source_sha256':{name:hashof(r/name)for name in source},
 'executable':{'path':'build/full-port/catacombs-key-state-1/dungeon_key_state_tests','bytes':(r/'build/full-port/catacombs-key-state-1/dungeon_key_state_tests').stat().st_size,'sha256':hashof(r/'build/full-port/catacombs-key-state-1/dungeon_key_state_tests')},
 'evidence_sha256':{p.name:hashof(p)for p in dest.iterdir()if p.name!='verification.json'}}
(dest/'verification.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8')
print('Retained',len(list(dest.iterdir())),'files',sum(p.stat().st_size for p in dest.iterdir()),'bytes. No executable or private runtime copied.')
