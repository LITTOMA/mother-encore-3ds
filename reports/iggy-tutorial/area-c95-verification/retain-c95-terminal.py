from pathlib import Path
import subprocess,hashlib
r=Path(__file__).resolve().parents[2]
for label,job in [('gcc',118241290410),('console',118241290351)]:
 p=r/('build/full-port/c95-'+label+'-full.log')
 with p.open('wb')as f:subprocess.run(['gh','api','repos/LITTOMA/mother-encore-3ds/actions/jobs/'+str(job)+'/logs'],cwd=r,stdout=f,check=True)
 lines=p.read_text(encoding='utf8',errors='replace').splitlines();summary=[line for line in lines if 'tests passed,'in line];print(label,hashlib.sha256(p.read_bytes()).hexdigest());print('\n'.join(summary[-4:]))
