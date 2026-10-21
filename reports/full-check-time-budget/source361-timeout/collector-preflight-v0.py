import hashlib,json,re,shutil,subprocess
from pathlib import Path
r=Path(__file__).resolve().parents[2];p=r/'build/full-port';out=r/'reports/full-check-time-budget/source361-timeout';assert not out.exists()
source='361c04c2054658e3c0c78b18175fb9b4638ffd9f';status=json.loads((p/'key-ndsp-full361-cancelled-status.json').read_text());assert status['headSha']==source and status['conclusion']=='cancelled';job=next(j for j in status['jobs']if j['databaseId']==118847566763);assert job['conclusion']=='cancelled'and next(s for s in job['steps']if s['number']==11)['conclusion']=='success'and next(s for s in job['steps']if s['number']==13)['conclusion']=='cancelled'
annotations=json.loads((p/'key-ndsp-full361-cancelled-annotations.json').read_text());assert any(a['message']=='The job has exceeded the maximum execution time of 2h0m0s'for a in annotations)
log=(p/'key-ndsp-full361-cancelled-gcc.log').read_text();counts=re.findall(r'100% tests passed, 0 tests failed out of (\d+)',log);assert counts==['1','1','310']and '[ 99%]'in log and 'The operation was canceled.'in log
base='0761232c4819b893f281fcccd2ed2a0f3f35c8cf';changed=subprocess.check_output(['git','diff','--name-only',base],cwd=r,text=True).splitlines();assert changed==['.github/workflows/build.yml']
out.mkdir(parents=True)
for suffix in['gcc.log','status.json','check.json','annotations.json']:shutil.copyfile(p/('key-ndsp-full361-cancelled-'+suffix),out/suffix)
shutil.copyfile(p/'full-time-budget-policy-1.log',out/'policy.log')
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
proof=dict(source=source,run=39486326366,job=118847566763,conclusion='cancelled_by_120_minute_job_limit',ordinary_gcc=310,full_sanitizer='build99%;no completed full test result',focused_sanitizer_collision=1,annotation='The job has exceeded the maximum execution time of 2h0m0s',fix='Full host job180minutes;console120 and basement30 unchanged;no test selection/per-test deadlines/compiler/leak changes',local='Five existing workflow policy tests only;not full host/console/emulator/hardware',same_game_test_and_resource_files_as=base,changed_tracked_paths_before_docs=changed,files={file.name:sha(file)for file in out.iterdir()if file.is_file()})
(out/'verification.json').write_text(json.dumps(proof,indent=2)+'\n');print('Actual source361 GCC ordinary310 passed;official job120min annotation and sanitizer99% cancellation retained;five policy tests only')
