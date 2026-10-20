from pathlib import Path
import hashlib,json,shutil,subprocess
r=Path(__file__).resolve().parents[2];dest=r/'reports/catacombs-switches/phone-schedule-integration';dest.mkdir(parents=True,exist_ok=True)
assert not subprocess.check_output(['git','diff','HEAD','--name-only','--','runtime','include','tools','content','romfs','platform/ctr'],cwd=r).strip()
for name in('catacombs-phone-merge-cmake-1.log','catacombs-phone-merge-ctest-1.json','catacombs-phone-merge-collection-1.log','catacombs-phone-merge-plan-1.json','collect-catacombs-phone-merge.py','retain-catacombs-phone-merge.py'):shutil.copyfile(r/'build/full-port'/name,dest/name)
h=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
proof=dict(parents=['fbbc25ed98e86d85a9f8d687127d053e46a40957','6e245cdd5b7c89cd7db047bdfb035abdded4666b'],scope='Feature-to-feature integration of PR69 test schedule repair into dependent PR71; main/origin-main untouched. No game source/resource change versus fbbc25e.',conflicts='Only docs/STATUS.md overlapping progress entries; preserved both.',verification='Actual Windows CMake configuration and279CTest metadata.1283Python=900aggregate383dedicated; exact17phone methods moved,1partition regression added, all dungeon methods retained. No Windows native execution or complete merged CI pass inferred.',evidence_sha256={p.name:h(p)for p in dest.iterdir()if p.name!='verification.json'})
(dest/'verification.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf8');print('Retained integration metadata and unchanged game-source proof')
