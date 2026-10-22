"""Require the real successful sort baseline before checking malformed inputs."""
import hashlib,json,subprocess,sys,tempfile
from pathlib import Path

executable,fixture,output=map(Path,sys.argv[1:])
root=Path(__file__).resolve().parents[1];output=output.resolve()
if root/'build'not in output.parents:raise ValueError('Owned sort failure output required')
marker=output/'.owned-storage-sort-failures'
if output.exists():
    if not marker.is_file()or marker.read_bytes()!=b'1\n':raise ValueError('Unowned sort failure output')
else:output.mkdir(parents=True);marker.write_bytes(b'1\n')
output=Path(tempfile.mkdtemp(prefix='run-',dir=output))
count=int((fixture/'negative-observation-count.txt').read_text())
if count!=5:raise ValueError('Unknown storage sort corruption coverage')
manifest=json.loads((fixture/'manifest.json').read_bytes())
records=[]
for name,args,expected in [('positive',[],0)]+[('negative-%d'%i,[str(fixture/('bad-observation-%d.txt'%i))],1)for i in range(count)]:
    if args:
        path=Path(args[0]);raw=path.read_bytes()
        if hashlib.sha256(raw).hexdigest()!=manifest['files'][path.name]:raise ValueError('Corrupt observation fixture changed')
    command=[str(executable),str(fixture)]+args
    with(output/(name+'.stdout.log')).open('wb')as stdout,(output/(name+'.stderr.log')).open('wb')as stderr:
        result=subprocess.run(command,stdout=stdout,stderr=stderr,timeout=120)
    text=(output/(name+'.stderr.log')).read_text(errors='replace')
    if result.returncode!=expected or any(tag in text for tag in ('runtime error:','ERROR: AddressSanitizer','ERROR: LeakSanitizer')):
        raise ValueError('Unexpected actual storage fixture check: '+name)
    records.append(dict(command=command,returncode=result.returncode))
(output/'receipt.json').write_text(json.dumps(records,indent=2)+'\n')
print('Checked positive original-sort baseline and',count,'rejected malformed observation fixtures')
