from pathlib import Path
import subprocess,json,hashlib
r=Path(__file__).resolve().parents[2]
for label,job in [('clang',118222788686),('console',118222788022)]:
 p=r/('build/full-port/f029-'+label+'-full.log')
 with p.open('wb')as f:subprocess.run(['gh','api','repos/LITTOMA/mother-encore-3ds/actions/jobs/'+str(job)+'/logs'],cwd=r,stdout=f,check=True)
 lines=p.read_text(encoding='utf8',errors='replace').splitlines();summary=[line for line in lines if('tests passed,'in line or 'actual packaged resources'in line or 'Verified'in line or '3DSX and CIA'in line or 'Upload'in line)]
 print(label,'sha256',hashlib.sha256(p.read_bytes()).hexdigest());print('\n'.join(summary[-8:]))
